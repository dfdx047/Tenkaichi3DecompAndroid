// The startup inputs made without captures (game/startup_inputs.h) pass the runtime's own reading and
// checks (probe/probe_inputs.cpp, the supply* functions) in the order of a player's run, and the GS packets
// left as zeros are taken from the game only with take_game_packets.
#include "game/startup_inputs.h"
#include "gs/gif_dma_staging.h"
#include "gs/texture_gif_startup.h"
#include "probe/probe_state.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

std::uint32_t loadStartupEvidence(EeRuntime&,const ProbeModes&,char**);
void loadContinuationEvidence(EeRuntime&,const ProbeModes&,char**);
void loadScmdEvidence(EeRuntime&,char**);

namespace {
namespace fs=std::filesystem;
template<class F> void reject(F f,const char* what) {bool thrown=false;try{f();}catch(const std::exception&){thrown=true;}require(thrown,what);}

// A small program: an ELF header and two PT_LOAD segments (one with a zero-filled tail).
std::vector<std::uint8_t> program() {
    std::vector<std::uint8_t> elf(0x200);
    const auto put=[&](std::size_t at,std::uint32_t v,unsigned bytes=4){std::memcpy(elf.data()+at,&v,bytes);};
    std::memcpy(elf.data(),"\x7F" "ELF\x01\x01\x01",7);
    put(16,2,2);put(18,8,2);put(24,0x100008);put(28,52);put(42,32,2);put(44,3,2);
    put(52,1);put(56,0x100);put(60,0x100000);put(68,0x40);put(72,0x40);   // code
    put(84,4);                                                             // a note: not loaded
    put(116,1);put(120,0x140);put(124,0x200000);put(132,0x10);put(136,0x100); // data, 0xF0 of bss
    for(unsigned n=0;n<0x40;++n)elf[0x100+n]=std::uint8_t(n+1);
    for(unsigned n=0;n<0x10;++n)elf[0x140+n]=0xEE;
    return elf;
}
}

int main() {
    try {
        const fs::path folder=fs::temp_directory_path()/"bt2-startup-inputs-test";
        fs::remove_all(folder);
        const auto elf=program();
        writeStartupInputs(folder,elf);
        for(const char* name:startup_input_names)require(fs::is_regular_file(folder/name),"a startup input is missing");

        // The guest image: the two loads where the program puts them, the rest zero.
        std::ifstream in(folder/"guest-memory.bin",std::ios::binary);
        std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),{});
        require(image.size()==EeRuntime::ram_size,"guest image size");
        require(image[0x100000]==1 && image[0x10003F]==0x40 && image[0x100040]==0,"code load");
        require(image[0x200000]==0xEE && image[0x20000F]==0xEE && image[0x200010]==0,"data load and its zero tail");
        reject([&]{buildGuestMemory(std::vector<std::uint8_t>(64));},"a file that is not an ELF");

        // A player's run: the arguments in the runtime's order (game_main.cpp), checked by its own code.
        std::vector<std::string> args{"bt2",(folder/startup_input_names[0]).string(),(folder/"report.json").string()};
        for(std::size_t n=1;n<startup_input_names.size();++n) {
            if(n==startup_input_names.size()-1)args.push_back((folder/"disc.iso").string());
            args.push_back((folder/startup_input_names[n]).string());
        }
        std::vector<char*> argv;for(auto& a:args)argv.push_back(a.data());argv.push_back(nullptr);
        const ProbeModes modes=ProbeModes::fromArgc(int(args.size()));
        require(args.size()==25 && modes.cd,"a player's run has every checkpoint");
        EeRuntime runtime;
        runtime.loadMemory(image);
        require(loadStartupEvidence(runtime,modes,argv.data())==0x21C12010,"OSD word");
        loadContinuationEvidence(runtime,modes,argv.data());
        loadScmdEvidence(runtime,argv.data());
        require(runtime.texture_gif.supplied && runtime.gif_dma.syncSupplied(),"GS evidence supplied");

        // The GS packets left as zeros: refused unless taken from the game.
        std::vector<std::uint8_t> ram(0x1000);
        for(std::size_t n=0;n<ram.size();++n)ram[n]=std::uint8_t(n*7+3);
        std::vector<std::uint8_t> zeros(240);
        {GifDmaStaging strict;strict.supply(zeros);strict.readIdle();strict.setCount(15);
         reject([&]{strict.setAddress(0x100,0x100,ram);},"a zero packet without take_game_packets");}
        {GifDmaStaging learning;learning.take_game_packets=true;learning.supply(zeros);learning.readIdle();learning.setCount(15);
         learning.setAddress(0x100,0x100,ram);require(learning.packet_verified,"the game's packet taken");}
        {std::vector<std::uint8_t> captured(240,1);GifDmaStaging real;real.take_game_packets=true;real.supply(captured);real.readIdle();real.setCount(15);
         reject([&]{real.setAddress(0x100,0x100,ram);},"a captured packet still checked");}
        {std::ifstream t(folder/"texture-startup.bin",std::ios::binary);std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(t)),{});
         TextureGifStartup strict;strict.supply(bytes);strict.idle();strict.pathIdle();strict.count(4);
         reject([&]{strict.address(0x200,0x200,ram);},"a zero texture packet without take_game_packets");
         TextureGifStartup learning;learning.take_game_packets=true;learning.supply(bytes);learning.idle();learning.pathIdle();learning.count(4);
         learning.address(0x200,0x200,ram);require(learning.phase==4,"the game's texture packet taken");}
        fs::remove_all(folder);
        std::cout<<"Startup inputs without captures: written, read and accepted by the runtime's checks; GS packets taken from the game only when asked."<<std::endl;
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAILED: "<<e.what()<<std::endl;
        return 1;
    }
}
