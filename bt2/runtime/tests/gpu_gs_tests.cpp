// paraLLEl-GS backend on synthetic GIF packets (no game data): drawn pixels checked word by word.
#include "gs/gpu/gpu_gs_backend.h"
#include "gs/cpu/gs_command_sink.h"
#include "gs/gpu/hd_texture_pack.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
// A+D-only PACKED packet: one GIF tag, then one register write per qword.
std::vector<std::uint8_t> packet(std::initializer_list<std::pair<std::uint64_t,std::uint64_t>> writes) {
    std::vector<std::uint8_t> bytes;
    auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);bytes.insert(bytes.end(),b,b+16);};
    q(writes.size()|(1ULL<<15)|(1ULL<<60),0xE); // NLOOP, EOP, PACKED, NREG 1: A+D.
    for(const auto& [reg,value]:writes)q(value,reg);
    return bytes;
}

// The PSMCT32 word at (x, y) of a frame buffer at page 0, 64 pixels wide.
std::uint32_t pixel(std::span<const std::uint32_t> vram,unsigned x,unsigned y) {return vram[GsCommandSink::ct32Word(0,1,x,y)];}
}

// HD texture packs (hd_texture_pack.h): names as PCSX2 reads them and the DDS files the pack loads.
void hdTexturePackFiles() {
    auto require=[](bool ok,const char* what){if(!ok)throw std::runtime_error(std::string("HD texture pack: ")+what);};
    require(HdTexturePack::normalized("1a-2b-00005a53")=="1a-2b-00001a53","bit 14 cleared");
    require(HdTexturePack::normalized("3c-00001a54")=="3c-00001a54","a name without a palette part");
    auto dds=[](const char* fourcc,std::uint32_t dxgi,std::uint32_t width,std::uint32_t height,std::uint32_t mips,std::size_t data) {
        std::vector<std::uint8_t> file(std::strcmp(fourcc,"DX10")?128:148);
        std::memcpy(file.data(),"DDS ",4);
        std::memcpy(file.data()+12,&height,4);std::memcpy(file.data()+16,&width,4);std::memcpy(file.data()+28,&mips,4);
        std::memcpy(file.data()+84,fourcc,4);
        if(!std::strcmp(fourcc,"DX10"))std::memcpy(file.data()+128,&dxgi,4);
        for(std::size_t i=0;i<data;++i)file.push_back(std::uint8_t(i));
        return file;
    };
    // BC3 (DXGI 77), 8 x 8 with two levels: 4 blocks, then 1.
    const auto bc3=HdTexturePack::readDds(dds("DX10",77,8,8,2,5*16));
    require(bc3 && bc3->format==137 && bc3->levels.size()==2 && bc3->levels[0].size()==64 && bc3->levels[1].size()==16 && bc3->levels[1][0]==64,"BC3 levels");
    // A mip chain shorter than the header says keeps the levels present.
    require(HdTexturePack::readDds(dds("DX10",77,8,8,4,5*16))->levels.size()==2,"short mip chain");
    // DXT1 (BC1, 8 bytes a block) without the DX10 header.
    const auto bc1=HdTexturePack::readDds(dds("DXT1",0,8,8,1,32));
    require(bc1 && bc1->format==133 && bc1->levels[0].size()==32,"DXT1");
    require(!HdTexturePack::readDds(dds("DX10",28,8,8,1,256)),"an uncompressed format is refused");
    require(!HdTexturePack::readDds(std::vector<std::uint8_t>(16)),"not a DDS file");
}

int main() try {
    hdTexturePackFiles();
    GpuGsBackend gpu;
    try {gpu.init();}
    catch(const std::exception& e) {std::cout<<"skipped: "<<e.what()<<'\n';return 77;}
    // Flat sprite into a 64-pixel-wide PSMCT32 frame at page 0; depth writes masked.
    const auto sprite=packet({{0x4C,0x0000000000010000ULL},{0x4E,0x0000000101000020ULL},{0x18,0},
        {0x40,0x001F0000003F0000ULL},{0x47,0x30000},{0x00,6},{0x01,0x0000000080402010ULL},
        {0x05,0x0000000000400040ULL},{0x05,0x0000000001000100ULL}});
    gpu.transfer(sprite);
    {
        // Pixels (4,4) to (15,15) take the colour; the right and bottom edges and beyond do not.
        const auto vram=gpu.readVram();
        for(unsigned y=4;y<16;++y)for(unsigned x=4;x<16;++x)if(pixel(vram,x,y)!=0x80402010u)throw std::runtime_error("Sprite pixel missing");
        if(pixel(vram,16,8)==0x80402010u || pixel(vram,8,16)==0x80402010u || pixel(vram,3,3)==0x80402010u)throw std::runtime_error("Sprite drew past its edges");
    }
    {
        // A draw packet that reaches the renderer in pieces ending inside the packet (as the texture names'
        // lockstep sends it) draws as the whole packet does: a PACKED UV/RGBAQ/XYZ2 triangle strip (the layout
        // the renderer runs a whole loop at a time; no texture), 4 vertices.
        std::vector<std::uint8_t> strip;
        auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);strip.insert(strip.end(),b,b+16);};
        q(4|(1ULL<<15)|(1ULL<<46)|(4ULL<<47)|(3ULL<<60),0x513); // NLOOP 4, EOP, PRE triangle strip, NREG 3: UV, RGBAQ, XYZ2.
        const std::uint64_t xy[4][2]={{0x200,0x80},{0x300,0x80},{0x200,0x180},{0x300,0x180}}; // 12.4: x 32..48, y 8..24.
        for(const auto& v:xy) {q(0,0);q(0x40ULL|(0x80ULL<<32),0x20ULL|(0x80ULL<<32));q(v[0]|(v[1]<<32),0);}
        // Each piece in its own buffer followed by poison (a vertex over the whole scissor area), so reading
        // past a piece's end draws pixels outside the square.
        for(std::size_t at=0;at<strip.size();at+=48) {
            const std::size_t size=std::min<std::size_t>(48,strip.size()-at);
            std::vector<std::uint8_t> piece(strip.begin()+std::ptrdiff_t(at),strip.begin()+std::ptrdiff_t(at+size));
            for(int k=0;k<8;++k) {const std::uint64_t poison[2]={0x0FF000000FF0ULL,0};piece.insert(piece.end(),reinterpret_cast<const std::uint8_t*>(poison),reinterpret_cast<const std::uint8_t*>(poison)+16);}
            gpu.transfer(std::span<const std::uint8_t>(piece).first(size));
        }
        // The square x 32..47, y 8..23 in R 0x40, G 0x80, B 0x20, A 0x80; nothing outside it.
        const auto vram=gpu.readVram();
        for(unsigned y=8;y<24;++y)for(unsigned x=32;x<48;++x)if(pixel(vram,x,y)!=0x80208040u)throw std::runtime_error("Strip pixel missing");
        if(pixel(vram,48,16) || pixel(vram,40,24) || pixel(vram,60,28))throw std::runtime_error("The strip drew outside its square (read past a piece)");
    }
    // The sink checks the packet's framing and passes it on; it draws nothing.
    GsCommandSink validator;validator.transfer(sprite);
    if(validator.transfers!=1)throw std::runtime_error("The sink lost the transfer");
    std::cout<<"GPU GS draws the sprite and the strip as expected\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
