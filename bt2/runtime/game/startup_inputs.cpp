#include "game/startup_inputs.h"
#include "services/devices/disc_metadata.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>

// Every value below is the one the named check in the runtime accepts; tests/startup_inputs_tests.cpp
// passes the files through those checks.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
constexpr std::size_t iop_memory_size=2*1024*1024; // EeRuntime::sif_iop_size
constexpr std::size_t kernel_snapshot_size=0x80004; // EeRuntime::kernel_snapshot_size
constexpr std::size_t guest_memory_size=32*1024*1024;

void put(Bytes& bytes,std::size_t offset,std::uint32_t word) {
    if(bytes.size()<offset+4)bytes.resize(offset+4);
    std::memcpy(bytes.data()+offset,&word,4);
}
void put64(Bytes& bytes,std::size_t offset,std::uint64_t value) {put(bytes,offset,std::uint32_t(value));put(bytes,offset+4,std::uint32_t(value>>32));}
void words(Bytes& bytes,std::size_t offset,std::initializer_list<std::uint32_t> list) {for(const auto w:list) {put(bytes,offset,w);offset+=4;}}
Bytes words(std::initializer_list<std::uint32_t> list) {Bytes bytes;words(bytes,0,list);return bytes;}
Bytes word(std::uint32_t w) {return words({w});}

void write(const fs::path& path,const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    if(!out)throw std::runtime_error("Cannot write the startup input "+path.string());
}

// sif-dma: the IOP's receive table (supplySifDma), then the SIF DMA call's result.
Bytes sifDma() {
    Bytes b(iop_memory_size);
    words(b,0x196C0,{0x19600,0x19680,0x93640});
    put(b,iop_memory_size,0x701);
    return b;
}
// sif-bind: the bound service's queue words in IOP memory, the bind reply, its RPC status and the
// call semaphore (supplySifBind).
Bytes sifBind() {
    Bytes b(iop_memory_size);
    put(b,0x3F560,0x3F570);put(b,0x3F570,0x80000592);put(b,0x3F578,0x3FB20);put(b,0x3F584,0);put(b,0x3F5B0,0x3F558);
    words(b,iop_memory_size,{0,0,0x80000008,0,0,0x203A97C0,0,0x3A87A8,0x80000009,0x3F570,0x3FB20,0,0,0,0,0});
    put(b,iop_memory_size+64,0x801);
    words(b,iop_memory_size+68,{0,1,0,0,0,0x3838E8,3});
    return b;
}
// cd-init: the CD initialization's mode, result and reply, then its four semaphores (supplyCdInit).
Bytes cdInit() {
    return words({0,0x902,0x1000,0x3695C0,0x80000008,0,0,0,0,0x3A87A8,0x8000000A,0,0,0,0,0,0,0,1,0,0,0,
                  0x370000,1,0,0,0x370000,0x3825E8,3,4,5,6});
}
// scmd-bind at its full length (1596 bytes): every step supplyScmdBind checks, the two GIF packets of
// the first GS setup left zero (taken from the game with BT2_SELF_STARTUP), then the fresh SIF, CD and
// LOADFILE evidence after the IOP reset (ee/post_reset_sif.h).
Bytes scmdBind() {
    Bytes b;
    words(b,0,{0x370000,1,0,0,2,0x3838D8,0,0,0x80000008,0,0,0x203A97C0,0,0x369F40,0x80000009,0x3F690,0x3F6D8,0,0,0,0,0});
    words(b,88,{0,1,0,0,0x370000,0x3838E8,0x400,0x3695C0,0x80000008,0,0,0,0,0x369F40,0x8000000A,0,0,0,0,0,0,0,2,0}); // media mode
    put(b,184,0);                                  // SIF stop
    put(b,188,0x19600);                            // reset SUBADDR
    words(b,192,{0x30000,0x30000,0x30000});        // reset SMFLAG
    words(b,204,{0xE01,0x20000,0,0,0});            // reboot submission and clears
    put(b,224,0x40000);                            // ready bit
    put64(b,228,0x551B4000);                       // GS CSR
    put64(b,236,0xFF00);                           // GS IMR
    put64(b,244,0);                                // GS CRT
    words(b,252,{12,0x1802,0x1FFEAD0,1,8,0x1802,0x1FFEAD0,1,12,0x1802,0x1FFEAD0,4,8,0x1802,0x1FFEAD0,1}); // INTC
    put64(b,316,0x551B6004);                       // GS field
    words(b,324,{0,0,0x1FFEAF0,0,0,0,1,0x800040,0x1201}); // GIF idle and staging
    put64(b,360,15);put64(b,368,0);                // reference returns
    b.resize(616);                                 // [376, 616): the first GIF packet, from the game
    words(b,616,{0,0,0x1FFEAF0,0,0x101,0,0x1FFEC40,0x40,0xE00,1,0,0x1FFEAF0,0,1,0,0x1FFEC40,0x40,0,
                  0x1000A000,0,0x1FFEAF0,0,1,0,0x1FFEC40,0x40,0}); // GIF synchronization (gif_dma_staging.h)
    words(b,616+108,{0,0,0x1FFEAF0,0,0,0x1FFEB00}); // VU idle and GS synchronization return
    b.resize(988);                                 // [748, 988): the second GIF packet, from the game
    words(b,988,{0,0,0x1FFED40,0x800044,0x84,0x3A9500,0x70000,0,0x1E640,
                  1,0,0x1FFED40,0xA00044,0x184,0x3A94E0,0x70000,0,0x1E640,
                  0x1E640,0x1E6C0,0,0x1E730,0x1E640,0x1E6C0,0x3A94C0,0x1E730,
                  0,0,0x80000001,0,0,1,0,1}); // fresh SIF transport (PostResetSif)
    {   // fresh CD replies (PostResetCd), 85 words
        std::array<std::uint32_t,85> cd{};
        for(unsigned n:{0u,16u}) {cd[n+2]=0x80000008;cd[n+5]=0x203A97C0;cd[n+7]=0x3A87A8;cd[n+8]=0x80000009;}
        cd[25]=0x43388;cd[26]=0x441B8;
        cd[32]=0x1000;cd[33]=0x3695C0;cd[34]=0x80000008;cd[39]=0x3A87A8;cd[40]=0x8000000A;
        cd[48]=1;cd[49]=cd[50]=0x225;
        cd[54]=0x80000008;cd[57]=0x203A97C0;cd[59]=0x369F40;cd[60]=0x80000009;cd[61]=0x434F0;cd[62]=0x439A8;
        cd[68]=0x400;cd[69]=0x3695C0;cd[70]=0x80000008;cd[75]=0x369F40;cd[76]=0x8000000A;cd[84]=1;
        for(unsigned n=0;n<cd.size();++n)put(b,1124+n*4,cd[n]);
    }
    {   // LOADFILE replies (PostResetLoadfile), 33 words
        std::array<std::uint32_t,33> loadfile{};
        loadfile[2]=0x80000008;loadfile[5]=0x203A97C0;loadfile[7]=0x3ACC00;loadfile[8]=0x80000009;loadfile[9]=0x22568;loadfile[10]=0x225B0;
        loadfile[16]=0x400;loadfile[17]=0x3ACA00;loadfile[18]=0x80000008;loadfile[23]=0x3ACC00;loadfile[24]=0x8000000A;loadfile[32]=0x30303033;
        for(unsigned n=0;n<loadfile.size();++n)put(b,1464+n*4,loadfile[n]);
    }
    return b;
}
// texture-startup: the texture GIF request's five status publications, then its packet left zero
// (gs/texture_gif_startup.h).
Bytes textureStartup() {
    Bytes b(224);
    const std::uint32_t first[4]={1,0,0x101,1};
    for(unsigned i=0;i<4;++i)words(b,i*32,{first[i],0,0x1FFECE0,0,1,0x1FFED30,0,0});
    put(b,4*32+8,0x1FFED40);
    return b;
}
}

std::vector<std::uint8_t> readGameProgram(const fs::path& disc) {
    DiscMetadata metadata(disc.string());
    const auto record=metadata.lookup("\\SLUS_214.41;1");
    if(!record)throw std::runtime_error("This disc image has no SLUS_214.41: it is not Budokai Tenkaichi 2 (USA)");
    std::uint32_t lba,size;std::memcpy(&lba,record->data(),4);std::memcpy(&size,record->data()+4,4);
    std::ifstream in(disc,std::ios::binary);
    Bytes program(size);
    in.seekg(std::streamoff(std::uint64_t(lba)*2048));
    if(!in.read(reinterpret_cast<char*>(program.data()),std::streamsize(size)))throw std::runtime_error("Cannot read SLUS_214.41 from the disc image");
    return program;
}

std::vector<std::uint8_t> buildGuestMemory(std::span<const std::uint8_t> program) {
    const auto u16=[&](std::size_t at){if(at+2>program.size())throw std::runtime_error("Short ELF");return std::uint32_t(program[at]|program[at+1]<<8);};
    const auto u32=[&](std::size_t at){if(at+4>program.size())throw std::runtime_error("Short ELF");std::uint32_t v;std::memcpy(&v,program.data()+at,4);return v;};
    if(program.size()<52 || std::memcmp(program.data(),"\x7F" "ELF\x01\x01",6))throw std::runtime_error("SLUS_214.41 is not a 32-bit little-endian ELF");
    const std::uint32_t phoff=u32(28),phentsize=u16(42),phnum=u16(44);
    Bytes memory(guest_memory_size);
    std::vector<std::pair<std::uint32_t,std::uint32_t>> occupied;
    for(std::uint32_t index=0;index<phnum;++index) { // PT_LOAD segments, as prepare_runtime.py's build_memory
        const std::size_t at=std::size_t(phoff)+std::size_t(index)*phentsize;
        if(u32(at)!=1)continue;
        const std::uint32_t offset=u32(at+4),address=u32(at+8),file_size=u32(at+16),length=u32(at+20);
        if(!length)continue;
        if(std::uint64_t(address)+length>memory.size() || file_size>length)throw std::runtime_error("ELF load exceeds bounded guest RAM");
        for(const auto& [start,end]:occupied)if(address<end && start<address+length)throw std::runtime_error("Overlapping ELF loads");
        occupied.emplace_back(address,address+length);
        if(std::uint64_t(offset)+file_size>program.size())throw std::runtime_error("ELF load payload is out of file bounds");
        std::memcpy(memory.data()+address,program.data()+offset,file_size);
    }
    return memory;
}

void writeStartupInputs(const fs::path& folder,std::span<const std::uint8_t> program) {
    fs::create_directories(folder);
    const auto at=[&](const char* name){return folder/name;};
    write(at("guest-memory.bin"),buildGuestMemory(program));
    write(at("kernel-snapshot.bin"),Bytes(kernel_snapshot_size));
    write(at("timer3-mode.bin"),word(0x83));        // probe_inputs.cpp: the validated Timer 3 mode
    write(at("timer2-mode.bin"),word(0x80));
    write(at("intc-mask.bin"),word(0x1002));
    write(at("timer2-readback.bin"),word(0x382));
    write(at("osd-config.bin"),word(0x21C12010));   // the configuration word the tests replay (English, 4:3)
    write(at("osd-readback.bin"),word(0x21C12010)); // supplyOsdReadback: the same word
    write(at("dma-status.bin"),word(0x00800040));
    write(at("dma5-control.bin"),word(0x184));
    write(at("sif-command.bin"),word(0x19600));
    write(at("sif-dma.bin"),sifDma());
    write(at("sif-rpc-state.bin"),word(1));
    write(at("sif-bind.bin"),sifBind());
    write(at("cd-init.bin"),cdInit());
    write(at("scmd-bind.bin"),scmdBind());
    write(at("heap-bind.bin"),words({0,0,0x80000008,0,0,0x203A97C0,0,0x3AC840,0x80000009,0x48438,0x48480,0,0,0,0,0}));
    write(at("heap-allocation.bin"),words({0x400,0x3AC880,0x80000008,0,0,0,0,0x3AC840,0x8000000A,0,0,0,0,0,0,0,0xCE500,
                                            0x369F90,0xCE500,0x360,0,0xFFFFFFFF}));
    write(at("texture-startup.bin"),textureStartup());
    write(at("ee-thread-start.bin"),words({0x70030C11,3,0x10,0x315380,0x3A5CC0,0x1000,0x38BDF0,5,5,0,0,0,0,0}));
    write(at("disc-bind.bin"),words({0,0,0x80000008,0,0,0x203A97C0,0,0x3A8780,0x80000009,0x433D0,0x441C8,0,0,0,0,0}));
    write(at("ncmd-bind.bin"),words({0,0,0x80000008,0,0,0x203A97C0,0,0x369590,0x80000009,0x43418,0x43DB8,0,0,0,0,0}));
}
