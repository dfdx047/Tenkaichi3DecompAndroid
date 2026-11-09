#include "services/devices/disc_sectors.h"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <limits>
int main(int argc,char** argv){
    if(argc==3){try{const auto bytes=read_iso_sectors(argv[1],0xE2A,2);std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!out)throw std::runtime_error("Local validation output failed");return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
    if(argc!=1)return 1;
    const auto path=std::filesystem::temp_directory_path()/"bt2-authored-sector-fixture.bin";
    try {
        if(std::filesystem::exists(path))throw std::runtime_error("Fixture already exists");
        std::vector<std::uint8_t> expected(6144);for(unsigned n=0;n<expected.size();++n)expected[n]=static_cast<std::uint8_t>((n*37+n/2048)&255);
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(expected.data()),expected.size());if(!file)throw std::runtime_error("Fixture write failed");}
        const auto bytes=read_iso_sectors(path.string(),1,2);
        if(!std::equal(bytes.begin(),bytes.end(),expected.begin()+2048) || bytes.size()!=4096)throw std::runtime_error("Sector bytes differ");
        auto reject=[&](unsigned lba,unsigned count){bool failed=false;try{read_iso_sectors(path.string(),lba,count);}catch(const std::runtime_error&){failed=true;}if(!failed)throw std::runtime_error("Invalid range accepted");};
        reject(0,0);reject(0,513);reject(3,1);reject(2,2);reject(UINT32_MAX,1);reject(UINT32_MAX,16);
        {std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write("short",5);}reject(0,1);
        std::filesystem::remove(path);bool missing=false;try{read_iso_sectors(path.string(),0,1);}catch(const std::runtime_error&){missing=true;}if(!missing)throw std::runtime_error("Missing file accepted");
        std::cout<<"Read-only sector bytes and physical range guards verified; no guest completion generated.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
