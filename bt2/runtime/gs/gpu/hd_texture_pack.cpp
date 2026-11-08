#include "gs/gpu/hd_texture_pack.h"
#include "gs/gpu/hd_texture_image.h"
#include "device.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

struct HdTexturePack::Entry {Image image;std::filesystem::path path;std::uint64_t bytes=0,used=0;bool none=false;};

namespace {
std::uint32_t u32(const std::vector<std::uint8_t>& d,std::size_t at) {std::uint32_t v;std::memcpy(&v,d.data()+at,4);return v;}
bool isDds(const std::filesystem::path& p) {
    auto e=p.extension().string();std::transform(e.begin(),e.end(),e.begin(),[](unsigned char c){return char(std::tolower(c));});
    return e==".dds";
}
// The pack's names below `root`, in path order, so the first of a duplicated name is always the same one.
std::vector<std::filesystem::path> ddsFiles(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> files;
    if(!std::filesystem::is_directory(root))return files;
    for(const auto& e:std::filesystem::recursive_directory_iterator(root))if(e.is_regular_file() && isDds(e.path()))files.push_back(e.path());
    std::sort(files.begin(),files.end());
    return files;
}
}

std::string HdTexturePack::normalized(const std::string& stem) {
    const auto dash=stem.rfind('-');
    if(dash==std::string::npos)return stem;
    char* end=nullptr;const unsigned long bits=std::strtoul(stem.c_str()+dash+1,&end,16);
    if(end==stem.c_str()+dash+1 || *end)return stem;
    char word[16];std::snprintf(word,sizeof word,"%08lx",bits&~0x4000ul);
    return stem.substr(0,dash+1)+word;
}

std::optional<HdTexturePack::Dds> HdTexturePack::readDds(const std::vector<std::uint8_t>& file) {
    if(file.size()<128 || std::memcmp(file.data(),"DDS ",4))return std::nullopt;
    Dds dds;dds.height=u32(file,12);dds.width=u32(file,16);
    const std::uint32_t mips=std::max<std::uint32_t>(1,u32(file,28));
    const std::uint32_t fourcc=u32(file,84);
    std::size_t at=128;
    auto code=[](const char* c){std::uint32_t v;std::memcpy(&v,c,4);return v;};
    if(fourcc==code("DX10")) {
        if(file.size()<148)return std::nullopt;
        switch(u32(file,128)) { // DXGI format; sRGB variants are read as UNORM.
        case 71:case 72:dds.format=VK_FORMAT_BC1_RGBA_UNORM_BLOCK;dds.block_bytes=8;break;
        case 74:case 75:dds.format=VK_FORMAT_BC2_UNORM_BLOCK;dds.block_bytes=16;break;
        case 77:case 78:dds.format=VK_FORMAT_BC3_UNORM_BLOCK;dds.block_bytes=16;break;
        case 98:case 99:dds.format=VK_FORMAT_BC7_UNORM_BLOCK;dds.block_bytes=16;break;
        default:return std::nullopt;
        }
        at=148;
    } else if(fourcc==code("DXT1")) {dds.format=VK_FORMAT_BC1_RGBA_UNORM_BLOCK;dds.block_bytes=8;}
    else if(fourcc==code("DXT3")) {dds.format=VK_FORMAT_BC2_UNORM_BLOCK;dds.block_bytes=16;}
    else if(fourcc==code("DXT5")) {dds.format=VK_FORMAT_BC3_UNORM_BLOCK;dds.block_bytes=16;}
    else return std::nullopt;
    if(!dds.width || !dds.height || dds.width>16384 || dds.height>16384)return std::nullopt;
    for(std::uint32_t level=0,w=dds.width,h=dds.height;level<mips;++level,w=std::max(1u,w/2),h=std::max(1u,h/2)) {
        const std::size_t size=std::size_t((w+3)/4)*((h+3)/4)*dds.block_bytes;
        if(at+size>file.size())break; // A shorter mip chain than the header says: keep the levels present.
        dds.levels.emplace_back(file.begin()+std::ptrdiff_t(at),file.begin()+std::ptrdiff_t(at+size));at+=size;
    }
    if(dds.levels.empty())return std::nullopt;
    return dds;
}

HdTexturePack::HdTexturePack(Vulkan::Device& device,const std::filesystem::path& folder,std::uint64_t budget_bytes,bool playstation_buttons)
    :device_(device),budget_(budget_bytes) {
    const auto replacements=std::filesystem::is_directory(folder/"replacements")?folder/"replacements":folder;
    if(!std::filesystem::is_directory(replacements))throw std::runtime_error("HD textures: no pack folder at "+folder.string());
    for(const auto& file:ddsFiles(replacements))
        if(!index_.emplace(normalized(file.stem().string()),file).second)++duplicates;
    if(playstation_buttons)for(const auto& file:ddsFiles(folder/"Original Buttons"))index_[normalized(file.stem().string())]=file;
}

HdTexturePack::~HdTexturePack() {for(auto& [name,entry]:cache_)delete entry;}

const HdTexturePack::Image* HdTexturePack::find(const std::string& name) {
    std::lock_guard<std::mutex> hold(lock_);
    const std::filesystem::path* file=nullptr;
    if(const auto same=index_.find(name);same!=index_.end())file=&same->second;
    else if(const auto first=name.find('-'),last=name.rfind('-');first!=last)
        if(const auto every=index_.find(name.substr(0,first)+name.substr(last));every!=index_.end())file=&every->second;
    if(!file) {++missing;return nullptr;}
    const auto key=file->generic_string();
    auto cached=cache_.find(key);
    if(cached==cache_.end()) {
        auto* entry=new Entry;entry->path=*file;
        cached=cache_.emplace(key,entry).first;
        pending_.push_back(key); // Loaded at the next frame boundary.
    }
    auto* entry=cached->second;
    if(entry->none) {++missing;return nullptr;}
    if(!entry->image.handle) {++waiting;return nullptr;} // Drawn as the original until loaded.
    entry->used=++tick_;++found;
    return &entry->image;
}

void HdTexturePack::loadPending() {
    std::lock_guard<std::mutex> hold(lock_);
    for(const auto& name:pending_) {
        const auto it=cache_.find(name);
        if(it==cache_.end())continue;
        auto* entry=it->second;
        std::ifstream in(entry->path,std::ios::binary);
        const std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
        const auto dds=readDds(file);
        if(!dds) {entry->none=true;++refused;continue;}
        auto info=Vulkan::ImageCreateInfo::immutable_2d_image(dds->width,dds->height,VkFormat(dds->format));
        info.levels=unsigned(dds->levels.size());
        std::vector<Vulkan::ImageInitialData> initial;
        std::uint64_t bytes=0;
        for(const auto& level:dds->levels) {initial.push_back({level.data(),0,0});bytes+=level.size();}
        entry->image.handle=device_.create_image(info,initial.data());
        if(!entry->image.handle) {entry->none=true;++refused;continue;}
        entry->bytes=bytes;entry->used=++tick_;++loaded;resident_bytes+=bytes;peak_bytes=std::max(peak_bytes,resident_bytes);
    }
    pending_.clear();
    if(resident_bytes>budget_)evict();
}

void HdTexturePack::evict() {
    // Least recently used first; one dropped is loaded again when it is drawn again.
    std::vector<std::pair<std::uint64_t,std::string>> order;
    for(const auto& [name,entry]:cache_)if(entry->image.handle)order.push_back({entry->used,name});
    std::sort(order.begin(),order.end());
    for(const auto& [used,name]:order) {
        if(resident_bytes<=budget_)break;
        auto it=cache_.find(name);
        resident_bytes-=it->second->bytes;++evicted;
        delete it->second;cache_.erase(it);
    }
}
