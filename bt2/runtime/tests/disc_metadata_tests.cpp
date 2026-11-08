#include "services/devices/disc_metadata.h"
#include <filesystem>
#include <iostream>
int main() {
    const auto path=std::filesystem::temp_directory_path()/"bt2-disc-metadata-test.iso";
    try {
        std::vector<unsigned char> image(24*2048);
        auto paired=[&](unsigned at,unsigned value){for(unsigned n=0;n<4;++n){image[at+n]=static_cast<unsigned char>(value>>(8*n));image[at+4+n]=static_cast<unsigned char>(value>>(8*(3-n)));}};
        auto record=[&](unsigned at,const std::string& name,unsigned lba,unsigned size,bool directory){
            const unsigned length=33+static_cast<unsigned>(name.size())+(name.size()%2==0);
            image[at]=static_cast<unsigned char>(length);paired(at+2,lba);paired(at+10,size);image[at+18]=106;image[at+19]=7;image[at+20]=13;
            image[at+21]=9;image[at+22]=49;image[at+23]=51;image[at+25]=directory?2:0;image[at+32]=static_cast<unsigned char>(name.size());
            std::memcpy(image.data()+at+33,name.data(),name.size());return length;
        };
        const unsigned pvd=16*2048;std::memcpy(image.data()+pvd,"\1CD001\1",7);paired(pvd+80,24);
        image[pvd+129]=8;image[pvd+130]=8;record(pvd+156,std::string(1,'\0'),20,2048,true);
        record(20*2048,"DATA",21,2048,true);record(21*2048,"TEST.DIR;1",22,64,false);
        auto save=[&]{std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(image.data()),image.size());};save();
        auto check=[](bool ok){if(!ok)throw std::runtime_error("Metadata test assertion");};
        { DiscMetadata disc(path.string());auto result=disc.lookup("\\DATA\\TEST.DIR;1");check(result && (*result)[0]==22 && (*result)[4]==64 && (*result)[25]==51 && (*result)[30]==0xD6 && (*result)[31]==7);
          check(!disc.lookup("\\DATA\\MISSING;1"));
          for(const auto& invalid:{"DATA\\TEST.DIR;1","\\..\\TEST.DIR;1","\\DATA\\","\\DATA/TEST.DIR;1"}){
            bool rejected=false;try{disc.lookup(invalid);}catch(const std::runtime_error&){rejected=true;}check(rejected);
          }
        }
        image[21*2048+6]^=1;save();{DiscMetadata disc(path.string());bool rejected=false;try{disc.lookup("\\DATA\\TEST.DIR;1");}catch(const std::runtime_error&){rejected=true;}check(rejected);}
        image[21*2048+6]^=1;image[21*2048+19]=13;save();{DiscMetadata disc(path.string());bool rejected=false;try{disc.lookup("\\DATA\\TEST.DIR;1");}catch(const std::runtime_error&){rejected=true;}check(rejected);}
        image[21*2048+19]=7;image[pvd+84]^=1;save();{DiscMetadata disc(path.string());bool rejected=false;try{disc.lookup("\\DATA\\TEST.DIR;1");}catch(const std::runtime_error&){rejected=true;}check(rejected);}
        std::filesystem::remove(path);std::cout<<"Synthetic ISO metadata, missing path, invalid names and corrupt extent verified.\n";return 0;
    }catch(const std::exception& e){std::filesystem::remove(path);std::cerr<<e.what()<<'\n';return 1;}
}
