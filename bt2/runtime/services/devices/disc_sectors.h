#pragma once
#include <cstdint>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

// The disc image, opened once and kept open. Every read opened, sized and closed the image again; on Android the
// image lies on shared storage behind FUSE, where opening a file costs milliseconds and sometimes far more, and
// the game reads the disc all through the menus and in fights (voices, music): the game thread stopped for each.
// Reads `size` bytes at `offset` into `out` (and gives the image's length); false when the image cannot be opened
// or is too short there.
inline bool read_iso_bytes(const std::string& path,std::uint64_t offset,char* out,std::uint64_t size,std::uint64_t* length_out=nullptr) {
    static std::mutex lock;static std::ifstream file;static std::string open_path;static std::uint64_t length=0;
    std::lock_guard<std::mutex> hold(lock);
    if(open_path!=path || !file.is_open()) {
        file.close();file.clear();open_path.clear();
        file.open(path,std::ios::binary|std::ios::ate);
        if(!file || file.tellg()<0) {file.close();file.clear();return false;}
        length=static_cast<std::uint64_t>(file.tellg());open_path=path;
    }
    if(length_out)*length_out=length;
    if(offset>length || size>length-offset)return false;
    if(!size)return true;
    file.clear();file.seekg(static_cast<std::streamoff>(offset));
    if(!file.read(out,static_cast<std::streamsize>(size))) {file.clear();return false;}
    return true;
}

// Read-only bytes owned by the caller. This performs no guest DMA, IRQ or completion.
inline std::vector<std::uint8_t> read_iso_sectors(const std::string& path,
        std::uint32_t lba,std::uint32_t count) {
    constexpr std::uint32_t sector_size=2048,maximum_sectors=512; // Streamed CDVDFSV requests read whole requests from the ISO.
    if(!count || count>maximum_sectors)throw std::runtime_error("Unsupported bounded sector count");
    std::uint64_t length=0;
    if(!read_iso_bytes(path,0,nullptr,0,&length))throw std::runtime_error("Local disc unavailable");
    const auto offset=static_cast<std::uint64_t>(lba)*sector_size;
    const auto size=static_cast<std::uint64_t>(count)*sector_size;
    if(offset>length || size>length-offset)throw std::runtime_error("Sector range outside local disc");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if(!read_iso_bytes(path,offset,reinterpret_cast<char*>(bytes.data()),size))throw std::runtime_error("Short sector read");
    return bytes;
}
