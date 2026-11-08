#include "gs/gpu/gpu_gs_backend.h"
#include "gs/gpu/hd_texture_image.h"
#include "gs/gpu/gpu_window_presenter.h"
#include "gs/textures/gs_texture_names.h"
#include "host/display_frame.h"
#include "context.hpp"
#include "device.hpp"
#include "gs_interface.hpp"
#include "thread_id.hpp"
#include <array>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <span>
#include <cstdio>
#include <cstring>
#include <stdexcept>

struct GpuGsBackend::State {
    Vulkan::ContextHandle context=Util::make_handle<Vulkan::Context>();
    Vulkan::DeviceHandle device=Util::make_handle<Vulkan::Device>();
    ParallelGS::GSInterface gs;
    std::shared_ptr<HdTexturePack> pack; // After the GS: destroyed first. Shared with a second renderer on the device.
    bool high_resolution=false,timestamps=false;unsigned scanouts=0;
    Vulkan::ImageHandle picture; // The last scanout.
    const GpuGsBackend* shown_from=nullptr; // The renderer whose picture the window shows (null: this one).
    std::unique_ptr<GpuWindowPresenter> presenter; // Destroyed first: it waits for the device.
    struct Kept {Vulkan::BufferHandle vram,clut;};
    std::array<Kept,2> kept; // keepState's slots.
    // Drawn palettes (gs/textures/gs_texture_names.h): the pages the names watch, copied back after each
    // frame into host memory in turn; a copy the GPU finished goes to every names object of the pack.
    struct PaletteCopy {Vulkan::BufferHandle buffer;Vulkan::Fence fence;std::vector<unsigned> pages;bool pending=false;};
    std::array<PaletteCopy,3> palette_copies;unsigned next_palette_copy=0;
    GsTextureNames* names=nullptr;std::vector<GsTextureNames*> palette_targets;
    unsigned hacks_seen=0; // The requestHacks generation this renderer has.
    // Frame skipping ("frame_skip"): a frame is not drawn when the GPU has not finished the one before it.
    bool frame_skip=false,skipped_last=false,frame_not_drawn=false;std::uint64_t previous_frame_timeline=0;unsigned frames_skipped=0;
    bool saves_shader_cache=false; // The primary renderer keeps the device's compiled pipelines (BT2_SHADER_CACHE).
    ~State();
};

namespace {
PFN_vkGetInstanceProcAddr vulkan_loader=nullptr;

// BT2_SHADER_CACHE (Android: <root>/bt2/user/shader-cache.bin): the driver's compiled pipelines, kept between
// runs so the renderer's shaders are compiled once per driver rather than on every start, which takes seconds of
// all the CPU's cores on a phone. One file per GPU and driver version (shader-cache-<device>-<driver>.bin), so
// switching between the system's driver and a custom one keeps both.
std::string shaderCachePath(Vulkan::Device& device) {
    const char* base=std::getenv("BT2_SHADER_CACHE");
    if(!base)return {};
    std::string path=base;
    const auto& gpu=device.get_gpu_properties();
    char tag[48];std::snprintf(tag,sizeof tag,"-%08x-%08x",gpu.deviceID,gpu.driverVersion);
    const auto dot=path.rfind('.');
    return dot==std::string::npos?path+tag:path.substr(0,dot)+tag+path.substr(dot);
}

void loadShaderCache(Vulkan::Device& device) {
    const auto file_path=shaderCachePath(device);
    if(file_path.empty())return;
    // (the one file of earlier builds, for the driver it was made with: Granite refuses another's)
    const char* path=file_path.c_str();
    std::FILE* file=std::fopen(path,"rb");
    if(!file)file=std::fopen(std::getenv("BT2_SHADER_CACHE"),"rb");
    std::vector<std::uint8_t> data;
    if(file) {
        std::fseek(file,0,SEEK_END);const long size=std::ftell(file);std::fseek(file,0,SEEK_SET);
        if(size>0) {data.resize(std::size_t(size));if(std::fread(data.data(),1,data.size(),file)!=data.size())data.clear();}
        std::fclose(file);
    }
    if(!device.init_pipeline_cache(data.empty()?nullptr:data.data(),data.size()))std::fprintf(stderr,"GPU GS: no pipeline cache\n");
    else std::fprintf(stderr,"GPU GS: pipeline cache %s (%zu bytes): %s\n",data.empty()?"started":"loaded",data.size(),path);
}

void saveShaderCache(Vulkan::Device& device) {
    const auto file_path=shaderCachePath(device);
    if(file_path.empty())return;
    const char* path=file_path.c_str();
    // The cache can grow between its size and its copy (pipelines compiled on other threads): asked again then.
    std::vector<std::uint8_t> data;
    for(unsigned attempt=0;attempt<3;++attempt) {
        data.resize(device.get_pipeline_cache_size()); // Exactly: the file's hash covers all of it.
        if(!data.empty() && device.get_pipeline_cache_data(data.data(),data.size()))break;
        data.clear();
    }
    if(data.empty())return;
    const std::string temporary=std::string(path)+".new"; // Written whole, then put in place: never half a file.
    std::FILE* file=std::fopen(temporary.c_str(),"wb");
    if(!file)return;
    const bool written=std::fwrite(data.data(),1,data.size(),file)==data.size();
    if(std::fclose(file)==0 && written)std::rename(temporary.c_str(),path);
}

// paraLLEl-GS's minimum (gs_renderer.cpp, GSRenderer::init), checked first so the player learns what the
// GPU's driver lacks instead of a bare failure; empty when everything is there.
std::string missingRequirements(Vulkan::Device& device) {
    const auto& ext=device.get_device_features();
    std::string missing;
    const auto need=[&](bool present,const char* what){if(!present)missing+=std::string(missing.empty()?"":", ")+what;};
    constexpr VkSubgroupFeatureFlags subgroup_ops=VK_SUBGROUP_FEATURE_ARITHMETIC_BIT|VK_SUBGROUP_FEATURE_SHUFFLE_BIT|
        VK_SUBGROUP_FEATURE_VOTE_BIT|VK_SUBGROUP_FEATURE_BALLOT_BIT|VK_SUBGROUP_FEATURE_BASIC_BIT;
    need(ext.vk12_features.descriptorIndexing,"descriptor indexing");
    need(ext.vk12_features.timelineSemaphore,"timeline semaphores");
    need(ext.vk12_features.bufferDeviceAddress,"buffer device address");
    need(ext.vk11_features.storageBuffer16BitAccess,"16-bit storage");
    need(ext.enabled_features.shaderInt16,"16-bit integers in shaders");
    need(ext.vk12_features.scalarBlockLayout,"scalar block layout");
    need((ext.vk11_props.subgroupSupportedOperations&subgroup_ops)==subgroup_ops,"subgroup operations");
    need(device.supports_subgroup_size_log2(true,2,6),"subgroup size control (Vulkan 1.3)");
    need(device.get_gpu_properties().limits.maxComputeSharedMemorySize>=32*1024,"32 KiB of compute shared memory");
    return missing;
}

ParallelGS::Hacks parseHacks(const std::string& text) {
    ParallelGS::Hacks hacks;
    for(std::size_t at=0;at<=text.size();) {
        const auto end=std::min(text.find(',',at),text.size());
        const auto name=text.substr(at,end-at);
        if(name=="blend_demote")hacks.allow_blend_demote=true;
        else if(name=="unsynced_readbacks")hacks.unsynced_readbacks=true;
        else if(name=="backbuffer_promotion")hacks.backbuffer_promotion=true;
        else if(name=="no_mipmaps")hacks.disable_mipmaps=true;
        else if(name=="late_readbacks")hacks.late_readbacks=true;
        else if(name=="skip_feedback")hacks.skip_feedback_effects=true;
        else if(name=="no_fog")hacks.disable_fog=true;
        else if(name=="frame_skip");      // Not paraLLEl-GS's: GpuGsBackend::flush (frameSkip).
        else if(!name.empty())throw std::runtime_error("BT2_GS_HACKS: unknown option "+name);
        at=end+1;
    }
    return hacks;
}

// Whether `text` (BT2_GS_HACKS's list) asks for frame skipping.
bool frameSkip(const std::string& text) {return (","+text+",").find(",frame_skip,")!=std::string::npos;}

ParallelGS::Hacks hacksFromEnvironment() {
    const char* list=std::getenv("BT2_GS_HACKS");
    if(!list)return {};
    std::fprintf(stderr,"GPU GS: speed-for-accuracy options %s\n",list);
    return parseHacks(list);
}

// requestHacks: the list and its generation; each renderer takes a newer one at its next frame.
std::mutex requested_lock;std::string requested_hacks;std::atomic<unsigned> hacks_generation{0};
}

void GpuGsBackend::setVulkanLoader(void* get_instance_proc_addr) {vulkan_loader=reinterpret_cast<PFN_vkGetInstanceProcAddr>(get_instance_proc_addr);}

void GpuGsBackend::requestHacks(const std::string& list) {
    parseHacks(list); // An unknown name throws here, on the caller's thread, rather than in the game's.
    {std::lock_guard hold(requested_lock);requested_hacks=list;}
    ++hacks_generation;
}

GpuGsBackend::GpuGsBackend()=default;
GpuGsBackend::~GpuGsBackend()=default;

GpuGsBackend::State::~State() {
    if(saves_shader_cache && device)saveShaderCache(*device);
    presenter.reset();picture.reset();
}

void GpuGsBackend::init(unsigned super_sampling) {
    if(super_sampling!=1 && super_sampling!=2 && super_sampling!=4 && super_sampling!=8 && super_sampling!=16)
        throw std::runtime_error("GPU GS: super-sampling must be 1, 2, 4, 8 or 16");
    if(!Vulkan::Context::init_loader(vulkan_loader))throw std::runtime_error("GPU GS: Vulkan loader unavailable");
    auto state=std::make_unique<State>();
    state->context->set_num_thread_indices(2); // The game's thread and the in-between pictures' worker.
    // The surface extensions let the window presenter make a swapchain on this device.
    const auto instance_extensions=GpuWindowPresenter::instanceExtensions();
    const char* device_extensions[]{"VK_KHR_swapchain"};
    if(!state->context->init_instance_and_device(instance_extensions.data(),unsigned(instance_extensions.size()),device_extensions,1,Vulkan::CONTEXT_CREATION_ENABLE_PUSH_DESCRIPTOR_BIT))
        throw std::runtime_error("GPU GS: no usable Vulkan device");
    state->device->set_context(*state->context);
    {   // Which GPU and driver: in the log a player sends.
        const auto& gpu=state->device->get_gpu_properties();
        std::fprintf(stderr,"GPU GS: %s, Vulkan %u.%u.%u, driver id %u version 0x%08X%s\n",gpu.deviceName,VK_API_VERSION_MAJOR(gpu.apiVersion),
            VK_API_VERSION_MINOR(gpu.apiVersion),VK_API_VERSION_PATCH(gpu.apiVersion),unsigned(state->device->get_device_features().driver_id),
            gpu.driverVersion,vulkan_loader?" (custom driver)":"");
        const auto missing=missingRequirements(*state->device);
        if(!missing.empty())throw std::runtime_error(std::string("This GPU's Vulkan driver (")+gpu.deviceName+") lacks what the renderer needs: "+missing+
            ". Mali GPUs before Valhall (G52, G72, G76) and Adreno before the 600 series do not have it; on Adreno a newer driver (such as Turnip) may.");
    }
    loadShaderCache(*state->device);state->saves_shader_cache=true; // Before paraLLEl-GS starts compiling.
    state->device->init_frame_contexts(8);
    ParallelGS::GSOptions options;options.super_sampling=ParallelGS::SuperSampling(super_sampling);
    state->high_resolution=super_sampling>=4;
    if(!state->gs.init(state->device.get(),options))throw std::runtime_error("GPU GS: paraLLEl-GS initialization failed");
    state->gs.set_hacks(hacksFromEnvironment());
    if(const char* list=std::getenv("BT2_GS_HACKS"))state->frame_skip=frameSkip(list);
    state_=std::move(state);
}

void GpuGsBackend::initShared(GpuGsBackend& primary,unsigned super_sampling) {
    auto state=std::make_unique<State>();
    state->context=primary.state_->context;state->device=primary.state_->device;
    ParallelGS::GSOptions options;options.super_sampling=ParallelGS::SuperSampling(super_sampling);
    state->high_resolution=super_sampling>=4;
    if(!state->gs.init(state->device.get(),options))throw std::runtime_error("GPU GS: paraLLEl-GS initialization failed (second renderer)");
    state->gs.set_hacks(hacksFromEnvironment());
    // Its flushes only submit: the primary moves the shared device to its next frame context, so this
    // renderer can flush from another thread while the primary's frame is open.
    state->gs.set_frame_boundaries(false);
    state_=std::move(state);
}

void GpuGsBackend::showPictureOf(const GpuGsBackend* source) {state_->shown_from=source==this?nullptr:source;}

void GpuGsBackend::transfer(std::span<const std::uint8_t> data) {
    if(data.size()%16)throw std::runtime_error("GPU GS: GIF transfer size is not a multiple of 16");
    state_->gs.gif_transfer(3,data.data(),data.size());
}

std::span<const std::uint32_t> GpuGsBackend::readVram() {
    constexpr std::size_t size=4*1024*1024;
    state_->gs.flush();
    const auto* words=static_cast<const std::uint32_t*>(state_->gs.map_vram_read(0,size));
    return {words,size/4};
}

namespace {
// A copy between buffers of equal size, ordered after all earlier work on the queue and before all later.
void copyBuffers(Vulkan::Device& device,std::initializer_list<std::pair<const Vulkan::Buffer*,const Vulkan::Buffer*>> copies) {
    auto cmd=device.request_command_buffer();
    cmd->barrier(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    for(const auto& [to,from]:copies) {
        if(to->get_create_info().size!=from->get_create_info().size)throw std::runtime_error("GPU GS: state copy between buffers of different sizes");
        cmd->copy_buffer(*to,*from);
    }
    cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT);
    device.submit(cmd);
}
}

void GpuGsBackend::keepState(unsigned slot) {
    auto& s=*state_;
    const auto& vram=s.gs.get_vram_buffer();const auto& clut=s.gs.get_clut_buffer();
    auto& k=s.kept.at(slot);
    if(!k.vram) {
        Vulkan::BufferCreateInfo info{};info.domain=Vulkan::BufferDomain::Device;
        info.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.size=vram.get_create_info().size;k.vram=s.device->create_buffer(info);
        info.size=clut.get_create_info().size;k.clut=s.device->create_buffer(info);
    }
    copyBuffers(*s.device,{{k.vram.get(),&vram},{k.clut.get(),&clut}});
}

void GpuGsBackend::takeState(const GpuGsBackend& primary,unsigned slot) {
    auto& s=*state_;
    const auto& k=primary.state_->kept.at(slot);
    if(!k.vram)throw std::runtime_error("GPU GS: no kept state in that slot");
    s.gs.flush(); // This renderer's work so far goes first.
    copyBuffers(*s.device,{{&s.gs.get_vram_buffer(),k.vram.get()},{&s.gs.get_clut_buffer(),k.clut.get()}});
    s.gs.invalidate_vram();
}

void GpuGsBackend::readFifo(std::span<std::uint8_t> out) {
    if(out.size()%16)throw std::runtime_error("GPU GS: FIFO read size is not a multiple of 16");
    state_->gs.read_transfer_fifo(out.data(),std::uint32_t(out.size()/16));
}

void GpuGsBackend::flush() {
    if(const unsigned generation=hacks_generation;generation!=state_->hacks_seen) {
        std::string list;{std::lock_guard hold(requested_lock);list=requested_hacks;}
        state_->gs.set_hacks(parseHacks(list));state_->hacks_seen=generation;
        state_->frame_skip=frameSkip(list);
        if(!state_->frame_skip) {state_->gs.set_skip_draws(false);state_->skipped_last=false;}
        std::fprintf(stderr,"GPU GS: speed-for-accuracy options now %s\n",list.empty()?"none":list.c_str());
    }
    state_->gs.flush();++state_->scanouts;
    state_->frame_not_drawn=state_->frame_skip && state_->skipped_last; // This frame's draws were dropped.
    if(state_->frame_skip) {
        // Behind: this frame took longer than its time on the display (frame_late), or the one before it is not
        // done on the GPU yet. Then the next one is not drawn (its transfers still are), never two in a row, so
        // the game keeps its speed and shows every other frame at worst.
        const bool behind=frame_late || (state_->previous_frame_timeline && state_->gs.completed_timeline_value()<state_->previous_frame_timeline);
        const bool skip=behind && !state_->skipped_last;
        state_->gs.set_skip_draws(skip);state_->skipped_last=skip;
        if(skip && ++state_->frames_skipped%600==0)std::fprintf(stderr,"GPU GS: %u frames skipped so far\n",state_->frames_skipped);
        state_->previous_frame_timeline=state_->gs.submitted_timeline_value();
    }
    // Kept once the start's shaders are in (about ten seconds), then every minute or so: Android may end the
    // process without a shutdown.
    if(state_->saves_shader_cache && (state_->scanouts==600 || state_->scanouts%3600==0))saveShaderCache(*state_->device);
    if(state_->pack) {
        state_->pack->loadPending(); // Between frames, after the renderer's work is submitted.
        copyDrawnPalettes();
    }
    state_->device->next_frame_context();
}

void GpuGsBackend::copyDrawnPalettes() {
    auto& s=*state_;
    if(!s.names)return;
    auto& device=*s.device;
    // Finished copies, oldest first, so the newest is what the names keep.
    for(unsigned k=1;k<=s.palette_copies.size();++k) {
        auto& copy=s.palette_copies[(s.next_palette_copy+k)%s.palette_copies.size()];
        if(!copy.pending || !copy.fence->wait_timeout(0))continue;
        const auto* words=static_cast<const std::uint32_t*>(device.map_host_buffer(*copy.buffer,Vulkan::MEMORY_ACCESS_READ_BIT));
        for(const auto page:copy.pages)for(auto* names:s.palette_targets)names->takeDrawnPage(page,words+std::size_t(page)*2048);
        device.unmap_host_buffer(*copy.buffer,Vulkan::MEMORY_ACCESS_READ_BIT);
        copy.pending=false;copy.fence.reset();
    }
    auto pages=s.names->drawnPalettePages();
    if(pages.empty())return;
    auto& copy=s.palette_copies[s.next_palette_copy];
    if(copy.pending)return; // The GPU is three frames behind: this frame's palettes are skipped.
    s.next_palette_copy=(s.next_palette_copy+1)%unsigned(s.palette_copies.size());
    const auto& vram=s.gs.get_vram_buffer(); // Its first 4 MB is GS memory as the host lays it out.
    if(!copy.buffer) {
        Vulkan::BufferCreateInfo info{};info.domain=Vulkan::BufferDomain::CachedHost;
        info.size=4*1024*1024;info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        copy.buffer=device.create_buffer(info);
    }
    std::vector<VkBufferCopy> regions;regions.reserve(pages.size());
    for(const auto page:pages)regions.push_back({VkDeviceSize(page)*8192,VkDeviceSize(page)*8192,8192});
    auto cmd=device.request_command_buffer();
    cmd->barrier(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_WRITE_BIT,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
    cmd->copy_buffer(*copy.buffer,vram,regions.data(),regions.size());
    cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    device.submit(cmd,&copy.fence);
    copy.pages=std::move(pages);copy.pending=true;
}

void GpuGsBackend::privileged(unsigned index,std::uint64_t value) {
    if(index>14)throw std::runtime_error("GPU GS: privileged register index out of range");
    state_->gs.get_priv_register_state().qwords_lo[index*2]=value; // Same order as the PS2 register block.
}

void GpuGsBackend::submit() {state_->gs.flush();}

void GpuGsBackend::registerWorkerThread() {Util::register_thread_index(1);}

DisplayFrame GpuGsBackend::scanout(unsigned field) {
    vsync(field);
    return readback();
}

std::pair<unsigned,unsigned> GpuGsBackend::vsync(unsigned field) {
    ParallelGS::VSyncInfo info{};
    info.phase=field&1;
    // Read by a copy (readback) or a blit (the window presenter).
    info.dst_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    info.dst_stage=VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    info.dst_access=VK_ACCESS_2_TRANSFER_READ_BIT;
    info.force_progressive=true;info.anti_blur=true;info.high_resolution_scanout=state_->high_resolution;
    // A frame that frame skipping did not draw: its buffer holds the picture from two frames back (the game
    // alternates two), so the last picture is shown again instead.
    if(state_->frame_not_drawn && state_->picture)return {state_->picture->get_width(),state_->picture->get_height()};
    const auto result=state_->gs.vsync(info);
    state_->picture=result.image;
    if(!result.image)return {0,0};
    return {result.image->get_width(),result.image->get_height()};
}

DisplayFrame GpuGsBackend::readback() {
    DisplayFrame frame;
    if(!state_->picture)return frame;
    submit(); // Between flips (the in-game menu opens there); the next frame context below waits for it.
    auto& device=*state_->device;
    const auto& image=*state_->picture;
    frame.width=image.get_width();frame.height=image.get_height();
    Vulkan::BufferCreateInfo buffer_info{};
    buffer_info.domain=Vulkan::BufferDomain::CachedHost;
    buffer_info.size=VkDeviceSize(frame.width)*frame.height*4;
    buffer_info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto buffer=device.create_buffer(buffer_info);
    auto cmd=device.request_command_buffer();
    cmd->copy_image_to_buffer(*buffer,image,0,{0,0,0},{frame.width,frame.height,1},0,0,{VK_IMAGE_ASPECT_COLOR_BIT,0,0,1});
    cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    Vulkan::Fence fence;
    device.submit(cmd,&fence);
    fence->wait();
    const auto* rgba=static_cast<const std::uint32_t*>(device.map_host_buffer(*buffer,Vulkan::MEMORY_ACCESS_READ_BIT));
    frame.bgra.resize(std::size_t(frame.width)*frame.height);
    for(std::size_t i=0;i<frame.bgra.size();++i) { // RGBA8 to the window's BGRA, opaque.
        const auto p=rgba[i];
        frame.bgra[i]=0xFF000000u|((p&0xFF)<<16)|(p&0xFF00)|((p>>16)&0xFF);
    }
    device.unmap_host_buffer(*buffer,Vulkan::MEMORY_ACCESS_READ_BIT);
    device.next_frame_context();
    return frame;
}

WindowPresenter& GpuGsBackend::windowPresenter() {
    if(!state_->presenter)
        state_->presenter=std::make_unique<GpuWindowPresenter>(state_->context,state_->device,[this]{
            return (state_->shown_from?state_->shown_from->state_->picture:state_->picture).get();
        });
    return *state_->presenter;
}

void GpuGsBackend::enableHdTextures(const std::filesystem::path& folder,std::uint64_t budget_bytes,GsTextureNames& names) {
    state_->pack=std::make_shared<HdTexturePack>(*state_->device,folder,budget_bytes);
    replaceTextures(names);
}

void GpuGsBackend::shareHdTextures(const GpuGsBackend& primary,GsTextureNames& names) {
    if(!primary.state_->pack)return;
    state_->pack=primary.state_->pack;
    primary.state_->palette_targets.push_back(&names); // Drawn palettes from the primary's copies.
    replaceTextures(names);
}

void GpuGsBackend::replaceTextures(GsTextureNames& names) {
    if(!state_->names) {state_->names=&names;state_->palette_targets.push_back(&names);}
    state_->gs.texture_replacement=[this,&names](const ParallelGS::TextureDescriptor& desc)->Vulkan::ImageHandle {
        const auto name=names.name(desc.tex0.bits,desc.texa.bits);
        if(!name)return {};
        // As older PCSX2 versions named it (the used palette entries), then as newer ones do (the whole palette).
        const auto* image=name->has_palette?state_->pack->find(name->textUsed()):nullptr;
        if(!image)image=state_->pack->find(name->text());
        return image?image->handle:Vulkan::ImageHandle{};
    };
}

void GpuGsBackend::enableTimestamps() {
    ParallelGS::DebugMode mode;mode.timestamps=true;
    state_->gs.set_debug_mode(mode);state_->timestamps=true;
}

std::string GpuGsBackend::timestampSummary() const {
    if(!state_ || !state_->timestamps || !state_->scanouts)return {};
    static constexpr const char* names[]{"host to VRAM","VRAM copies","palettes","texture uploads","triangle setup","binning","shading","readback","scanout"};
    double total=0;std::string parts;
    for(int n=0;n<int(ParallelGS::TimestampType::Count);++n) {
        const double ms=state_->gs.get_accumulated_timestamps(ParallelGS::TimestampType(n))*1000/state_->scanouts;total+=ms;
        char text[64];std::snprintf(text,sizeof text,"%s%s %.3f",parts.empty()?"":", ",names[n],ms);parts+=text;
    }
    char head[96];std::snprintf(head,sizeof head,"GPU time per frame (%u frames): %.3f ms: ",state_->scanouts,total);
    return head+parts;
}

std::string GpuGsBackend::hdTextureSummary() const {
    if(!state_ || !state_->pack)return {};
    const auto& p=*state_->pack;
    return "HD textures: "+std::to_string(p.names())+" in the pack ("+std::to_string(p.duplicates)+" duplicate names), "
        +std::to_string(p.found)+" draws replaced, "+std::to_string(p.missing)+" without a replacement; "+std::to_string(p.loaded)+" loaded, "
        +std::to_string(p.evicted)+" dropped for the budget, "+std::to_string(p.refused)+" files refused; peak "+std::to_string(p.peak_bytes>>20)+" MB";
}
