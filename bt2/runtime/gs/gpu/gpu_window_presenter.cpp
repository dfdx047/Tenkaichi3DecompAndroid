#ifdef __ANDROID__
#define VK_USE_PLATFORM_ANDROID_KHR 1 // vkCreateAndroidSurfaceKHR's types, before Granite's Vulkan headers
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include "gs/gpu/gpu_window_presenter.h"
#include "wsi.hpp"
#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#else
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#endif
#include <algorithm>
#include <stdexcept>

namespace {
#ifdef _WIN32
using NativeWindow=HWND;
// Granite's WSI asks its platform for the surface and its size; here that is the viewer's window.
class WindowPlatform final : public Vulkan::WSIPlatform {
public:
    explicit WindowPlatform(HWND window):window_(window) {}
    VkSurfaceKHR create_surface(VkInstance instance,VkPhysicalDevice) override {
        const auto create=reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(vkGetInstanceProcAddr(instance,"vkCreateWin32SurfaceKHR"));
        if(!create)return VK_NULL_HANDLE;
        VkWin32SurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        info.hinstance=GetModuleHandleA(nullptr);info.hwnd=window_;
        VkSurfaceKHR surface=VK_NULL_HANDLE;
        return create(instance,&info,nullptr,&surface)==VK_SUCCESS?surface:VK_NULL_HANDLE;
    }
    std::vector<const char*> get_instance_extensions() override {return {"VK_KHR_surface","VK_KHR_win32_surface"};}
    uint32_t get_surface_width() override {return width_;}
    uint32_t get_surface_height() override {return height_;}
    bool alive(Vulkan::WSI&) override {return true;}
    void poll_input() override {}
    void poll_input_async(Granite::InputTrackerHandler*) override {}
    uintptr_t get_native_window() override {return reinterpret_cast<uintptr_t>(window_);}
    // The client area's size; a change makes the WSI build the swapchain again at the next frame.
    void size(unsigned width,unsigned height) {
        if(width==width_ && height==height_)return;
        width_=width;height_=height;resize=true;
    }
private:
    HWND window_;unsigned width_=1,height_=1;
};
#else
using NativeWindow=SDL_Window*;
// The same on SDL's window (host/sdl/sdl_viewer_window.h). On Android the surface is made here from the
// window's ANativeWindow through the renderer's own Vulkan loader, so a custom driver (Turnip, loaded by
// libadrenotools) draws and presents alike; elsewhere SDL makes it for its platform (X11, Wayland).
class WindowPlatform final : public Vulkan::WSIPlatform {
public:
    explicit WindowPlatform(SDL_Window* window):window_(window) {}
    VkSurfaceKHR create_surface(VkInstance instance,VkPhysicalDevice) override {
        VkSurfaceKHR surface=VK_NULL_HANDLE;
#ifdef __ANDROID__
        auto* native=static_cast<ANativeWindow*>(SDL_GetPointerProperty(SDL_GetWindowProperties(window_),SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER,nullptr));
        const auto create=reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(vkGetInstanceProcAddr(instance,"vkCreateAndroidSurfaceKHR"));
        if(!native || !create)return VK_NULL_HANDLE;
        VkAndroidSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
        info.window=native;
        return create(instance,&info,nullptr,&surface)==VK_SUCCESS?surface:VK_NULL_HANDLE;
#else
        return SDL_Vulkan_CreateSurface(window_,instance,nullptr,&surface)?surface:VK_NULL_HANDLE;
#endif
    }
#ifndef __ANDROID__
    void destroy_surface(VkInstance instance,VkSurfaceKHR surface) override {SDL_Vulkan_DestroySurface(instance,surface,nullptr);}
#endif
    std::vector<const char*> get_instance_extensions() override {return GpuWindowPresenter::instanceExtensions();}
    uint32_t get_surface_width() override {return width_;}
    uint32_t get_surface_height() override {return height_;}
    bool alive(Vulkan::WSI&) override {return true;}
    void poll_input() override {}
    void poll_input_async(Granite::InputTrackerHandler*) override {}
    uintptr_t get_native_window() override {return reinterpret_cast<uintptr_t>(window_);}
    void size(unsigned width,unsigned height) {
        if(width==width_ && height==height_)return;
        width_=width;height_=height;resize=true;
    }
private:
    SDL_Window* window_;unsigned width_=1,height_=1;
};
#endif

VkOffset3D corner(int x,int y) {return {x,y,0};}
VkOffset3D extent(int width,int height) {return {width,height,1};}
}

std::vector<const char*> GpuWindowPresenter::instanceExtensions() {
#ifdef _WIN32
    return {"VK_KHR_surface","VK_KHR_win32_surface"};
#elif defined(__ANDROID__)
    return {"VK_KHR_surface","VK_KHR_android_surface"};
#else
    // What SDL's video driver needs for its surfaces (on Android: VK_KHR_surface, VK_KHR_android_surface).
    if(!SDL_InitSubSystem(SDL_INIT_VIDEO))throw std::runtime_error(std::string("GPU presenter: SDL video: ")+SDL_GetError());
    Uint32 count=0;
    const char* const* names=SDL_Vulkan_GetInstanceExtensions(&count);
    if(!names)throw std::runtime_error(std::string("GPU presenter: no Vulkan surface extensions: ")+SDL_GetError());
    return std::vector<const char*>(names,names+count);
#endif
}

struct GpuWindowPresenter::State {
    NativeWindow window=nullptr;
    std::unique_ptr<WindowPlatform> platform;
    std::unique_ptr<Vulkan::WSI> wsi;
    Vulkan::ImageHandle enlarged; // The picture at whole multiples of its size, kept while the size stays.
    int mode=0; // The present_mode the swapchain has.
    ~State() {enlarged.reset();wsi.reset();} // The swapchain goes before its platform.
};

GpuWindowPresenter::GpuWindowPresenter(Vulkan::ContextHandle context,Vulkan::DeviceHandle device,std::function<const Vulkan::Image*()> renderer_picture)
    :state_(std::make_unique<State>()),context_(std::move(context)),device_(std::move(device)),renderer_picture_(std::move(renderer_picture)) {}

GpuWindowPresenter::~GpuWindowPresenter() {
    if(device_)device_->wait_idle();
    state_.reset();
}

namespace {
Vulkan::PresentMode presentMode(int mode) {
    switch(mode) {
    case 1:return Vulkan::PresentMode::SyncToVBlank;
    case 2:return Vulkan::PresentMode::UnlockedNoTearing;
    case 3:return Vulkan::PresentMode::UnlockedForceTearing;
    default:return Vulkan::PresentMode::UnlockedMaybeTear; // Our pacer decides when a picture appears.
    }
}
}

void GpuWindowPresenter::show(void* window,const DisplayFrame* frame,Rect at,unsigned client_width,unsigned client_height,bool smooth,
                              const DisplayFrame& status,Rect status_at) {
    auto& s=*state_;
    if(!s.wsi || s.window!=static_cast<NativeWindow>(window)) { // The first picture (or a new window or surface): its swapchain.
        s.wsi.reset();
        s.window=static_cast<NativeWindow>(window);
        s.platform=std::make_unique<WindowPlatform>(s.window);
        s.platform->size(client_width,client_height);
        s.wsi=std::make_unique<Vulkan::WSI>();
        s.wsi->set_platform(s.platform.get());
        // The chosen presentation (present_mode); automatic leaves when a picture appears to our pacer.
        s.wsi->set_present_mode(presentMode(present_mode));s.mode=present_mode;
        s.wsi->set_backbuffer_format(Vulkan::BackbufferFormat::UNORM); // The picture's values as they are.
        s.wsi->set_extra_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);   // Pictures are copied in.
        if(!s.wsi->init_from_existing_context(context_) || !s.wsi->init_device(device_) || !s.wsi->init_surface_swapchain())
            throw std::runtime_error("GPU presenter: no swapchain for the window");
    }
    s.platform->size(client_width,client_height);
    if(const int mode=present_mode;mode!=s.mode) {s.wsi->set_present_mode(presentMode(mode));s.mode=mode;}
    auto& device=*device_;
    if(!s.wsi->begin_frame()) {
#ifndef _WIN32
        lost();return; // A surface that went with the app's window (Android): a new swapchain at the next picture.
#endif
        throw std::runtime_error("GPU presenter: the swapchain was lost");
    }
    // The source: a picture of the processor's uploaded for this frame, or the renderer's scanout.
    Vulkan::ImageHandle uploaded,status_image;
    const auto upload=[&](const DisplayFrame& picture) {
        auto info=Vulkan::ImageCreateInfo::immutable_2d_image(picture.width,picture.height,VK_FORMAT_B8G8R8A8_UNORM);
        info.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;info.initial_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        const Vulkan::ImageInitialData data{picture.bgra.data(),0,0};
        return device.create_image(info,&data);
    };
    const Vulkan::Image* source=nullptr;
    if(frame) {if(frame->width)uploaded=upload(*frame);source=uploaded.get();}
    else source=renderer_picture_();
    if(status.width)status_image=upload(status);
    auto& target=device.get_swapchain_view().get_image();
    const int width=int(target.get_width()),height=int(target.get_height());
    auto cmd=device.request_command_buffer();
    cmd->image_barrier(target,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE,0,VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkClearValue black{};
    cmd->clear_image(target,black);
    cmd->barrier(VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
    // A rectangle of the source copied into one of the target, both cut to the target's area.
    const auto blit=[&](const Vulkan::Image& from,Rect to,VkFilter filter) {
        const int x0=std::max(to.x,0),y0=std::max(to.y,0),x1=std::min(to.x+to.width,width),y1=std::min(to.y+to.height,height);
        if(x0>=x1 || y0>=y1 || to.width<=0 || to.height<=0)return;
        const double sx=double(from.get_width())/to.width,sy=double(from.get_height())/to.height;
        const int u0=int((x0-to.x)*sx+0.5),v0=int((y0-to.y)*sy+0.5),u1=int((x1-to.x)*sx+0.5),v1=int((y1-to.y)*sy+0.5);
        if(u0>=u1 || v0>=v1)return;
        cmd->blit_image(target,from,corner(x0,y0),extent(x1-x0,y1-y0),corner(u0,v0),extent(u1-u0,v1-v0),0,0,0,0,1,filter);
    };
    // A stretch that is not a whole multiple (widescreen, scaling off) is done in two steps: first the
    // largest whole multiple that fits, pixels copied, then the rest filtered. A plain filtered stretch
    // makes one-pixel lines thinner or thicker with where they fall, so thin edges pulse as the
    // picture moves; this way only the lines' edges are smoothed.
    if(source && smooth && at.width>0 && at.height>0) {
        const unsigned kx=std::max(1u,unsigned(at.width)/source->get_width()),ky=std::max(1u,unsigned(at.height)/source->get_height());
        if(kx>1 || ky>1) {
            const unsigned w=source->get_width()*kx,h=source->get_height()*ky;
            if(!s.enlarged || s.enlarged->get_width()!=w || s.enlarged->get_height()!=h) {
                auto info=Vulkan::ImageCreateInfo::render_target(w,h,VK_FORMAT_R8G8B8A8_UNORM);
                info.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;info.initial_layout=VK_IMAGE_LAYOUT_UNDEFINED;
                s.enlarged=device.create_image(info);
            }
            auto& enlarged=*s.enlarged;
            cmd->image_barrier(enlarged,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,0,VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
            cmd->blit_image(enlarged,*source,corner(0,0),extent(int(w),int(h)),corner(0,0),
                extent(int(source->get_width()),int(source->get_height())),0,0,0,0,1,VK_FILTER_NEAREST);
            cmd->image_barrier(enlarged,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
            source=&enlarged;
        }
    }
    if(source)blit(*source,at,smooth?VK_FILTER_LINEAR:VK_FILTER_NEAREST);
    if(status_image)blit(*status_image,status_at,VK_FILTER_NEAREST);
    cmd->image_barrier(target,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_NONE,0);
    cmd->swapchain_touch_in_stages(VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
    device.submit(cmd);
    if(!s.wsi->end_frame()) {
#ifndef _WIN32
        lost();return;
#endif
        throw std::runtime_error("GPU presenter: presenting failed");
    }
}

void GpuWindowPresenter::lost() {
    auto& s=*state_;
    if(!s.wsi)return;
    if(device_)device_->wait_idle();
    s.enlarged.reset();s.wsi.reset();s.platform.reset();
}
