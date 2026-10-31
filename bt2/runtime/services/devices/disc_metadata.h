#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Read-only ISO9660 metadata. No file payload or sector completion API.
class DiscMetadata {
    std::ifstream file_;std::uint64_t length_=0;
    std::vector<std::uint8_t> read(std::uint64_t offset,std::uint32_t size) {
        if(offset>length_ || size>length_-offset)throw std::runtime_error("Disc extent outside file");
        std::vector<std::uint8_t> b(size);file_.clear();file_.seekg(static_cast<std::streamoff>(offset));
        if(!file_.read(reinterpret_cast<char*>(b.data()),size))throw std::runtime_error("Short disc read");return b;
    }
    static std::uint32_t pair(const std::vector<std::uint8_t>& b,unsigned p) {
        if(p+8>b.size())throw std::runtime_error("Short ISO paired field");
        std::uint32_t lo=0,hi=0;for(unsigned n=0;n<4;++n){lo|=std::uint32_t(b[p+n])<<(8*n);hi=(hi<<8)|b[p+4+n];}
        if(lo!=hi)throw std::runtime_error("ISO paired field differs");return lo;
    }
    std::pair<std::uint32_t,std::uint32_t> extent(const std::vector<std::uint8_t>& r) {
        if(r.size()<34 || r[1] || r[26] || r[27] || (r[25]&128) || 33u+r[32]>r.size())throw std::runtime_error("Unsupported ISO record");
        const auto lba=pair(r,2),size=pair(r,10);
        if(std::uint64_t(lba)*2048>length_ || size>length_-std::uint64_t(lba)*2048)throw std::runtime_error("ISO extent outside disc");return {lba,size};
    }
public:
    explicit DiscMetadata(const std::string& path):file_(path,std::ios::binary|std::ios::ate) {
        if(!file_ || file_.tellg()<0)throw std::runtime_error("Local disc unavailable");length_=static_cast<std::uint64_t>(file_.tellg());
    }
    std::optional<std::array<std::uint8_t,32>> lookup(const std::string& path) {
        if(path.empty() || path.size()>255 || path[0]!='\\')throw std::runtime_error("Invalid disc path");
        std::vector<std::string> parts;std::size_t at=1;
        while(at<path.size()){const auto end=path.find('\\',at);const auto part=path.substr(at,end==std::string::npos?end:end-at);
            if(part.empty() || part=="." || part=="..")throw std::runtime_error("Invalid disc component");
            for(unsigned char c:part)if(c<32 || c>126 || c=='/' || c==':')throw std::runtime_error("Invalid disc filename");
            parts.push_back(part);if(end==std::string::npos)break;at=end+1;if(at==path.size())throw std::runtime_error("Empty disc component");}
        if(parts.empty())throw std::runtime_error("No disc filename");
        auto pvd=read(16*2048,2048);
        if(std::memcmp(pvd.data(),"\1CD001\1",7) || pvd[128]!=0 || pvd[129]!=8 || pvd[130]!=8 || pvd[131]!=0 || std::uint64_t(pair(pvd,80))*2048>length_)throw std::runtime_error("Unsupported ISO volume");
        const auto n=pvd[156];if(n<34 || 156u+n>pvd.size())throw std::runtime_error("Invalid root record");
        std::vector<std::uint8_t> record(pvd.begin()+156,pvd.begin()+156+n);
        for(unsigned index=0;index<parts.size();++index) {
            if(!(record[25]&2))throw std::runtime_error("ISO path parent is not directory");auto [lba,size]=extent(record);
            if(size>16*1024*1024)throw std::runtime_error("ISO directory too large");auto directory=read(std::uint64_t(lba)*2048,size);
            std::optional<std::vector<std::uint8_t>> found;
            for(std::size_t pos=0;pos<directory.size();) {
                const auto count=directory[pos];if(!count){pos=(pos/2048+1)*2048;continue;}
                if(count<34 || count>directory.size()-pos || pos%2048+count>2048)throw std::runtime_error("Invalid ISO directory boundary");
                std::vector<std::uint8_t> r(directory.begin()+pos,directory.begin()+pos+count);extent(r);
                const std::string name(reinterpret_cast<char*>(r.data()+33),r[32]);
                if(name==parts[index]){if(found)throw std::runtime_error("Ambiguous ISO filename");found=std::move(r);}pos+=count;
            }
            if(!found)return std::nullopt;record=std::move(*found);
        }
        if(record[25]&2)throw std::runtime_error("Disc lookup expects file");auto [lba,size]=extent(record);
        std::array<std::uint8_t,32> out{};std::memcpy(out.data(),&lba,4);std::memcpy(out.data()+4,&size,4);
        const auto name=parts.back();if(name.size()>15)throw std::runtime_error("Unsupported long disc result name");std::memcpy(out.data()+8,name.data(),name.size());
        // sceCdlFILE uses binary second/minute/hour/day/month and little-endian year.
        if(!record[19] || record[19]>12 || !record[20] || record[20]>31 || record[21]>23 || record[22]>59 || record[23]>59)
            throw std::runtime_error("Unsupported ISO record date");
        const unsigned year=1900+record[18];out[24]=0;out[25]=record[23];out[26]=record[22];out[27]=record[21];out[28]=record[20];out[29]=record[19];out[30]=year&255;out[31]=static_cast<std::uint8_t>(year>>8);
        return out;
    }
};

// A file's directory record looked up in the disc image kept open (opened again for every lookup before: on Android's
// shared storage, behind FUSE, an open costs milliseconds; see disc_sectors.h). Same results and failures as a new
// DiscMetadata(path).lookup(name).
#include <memory>
#include <mutex>
inline std::optional<std::array<std::uint8_t,32>> lookup_disc_file(const std::string& image,const std::string& name) {
    static std::mutex lock;static std::unique_ptr<DiscMetadata> disc;static std::string disc_path;
    std::lock_guard<std::mutex> hold(lock);
    if(!disc || disc_path!=image) {disc.reset();disc=std::make_unique<DiscMetadata>(image);disc_path=image;}
    return disc->lookup(name);
}
