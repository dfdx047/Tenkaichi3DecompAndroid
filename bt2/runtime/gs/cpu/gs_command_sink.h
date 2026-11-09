#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// The GS command sink: checks the GIF data the game sends (startup packets, PACKED, REGLIST and IMAGE
// transfers) and follows the GS registers, the local->host transfers and FINISH, and hands the accepted
// data on to the GPU renderer (paraLLEl-GS, gs/gpu/), which draws every picture; this draws nothing and
// keeps no copy of GS memory (the CPU renderer was removed 2026-10-21, the user's decision). It also feeds
// the texture names (gs/textures/gs_texture_names.h) in step with the renderer. The GS memory layout
// functions below are the reference the texture names' tables are checked against.
class GsTextureNames;

class GsCommandSink {
public:
    void resetDrawingState() {
        // CSR reset establishes SCANMSK/FBA/PABE defaults.
        registers={};tex0={};pabe=0;drawing_reset_=true;
    }
    static std::size_t address(unsigned page,unsigned width,unsigned x,unsigned y,bool depth) {
        // PSMCT32/PSMZ24: 64x32 pages, 8x8 blocks. Public GS bit layout,
        // independently expressed as bit placement rather than reference tables.
        unsigned bx=(x%64)/8, by=(y%32)/8;
        unsigned block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2);
        unsigned px=x%8, py=y%8;
        unsigned pixel=(px&1)|((py&1)<<1)|((px&6)<<1)|((py&6)<<3);
        std::size_t word=((std::size_t(page)+(y/32)*width+x/64)*2048+block*64+pixel);
        if(depth)word^=1536;
        return word%(1024*1024);
    }
    void consume(std::span<const std::uint8_t> packet);
    void consumeConfiguration(std::span<const std::uint8_t> packet);
    // General GIF transfers (frame path): GIFtags in PACKED, REGLIST or IMAGE form update the GS
    // register file. A group of transfers is one transaction: register state and counters are restored
    // if any part is refused, so a rejected group changes nothing.
    void transfer(std::span<const std::uint8_t> data) {transferAll(std::span<const std::span<const std::uint8_t>>(&data,1));}
    void transferAll(std::span<const std::span<const std::uint8_t>> group);
    struct State {
        std::array<std::uint64_t,0x64> regs{};
        std::uint32_t q=0x3F800000; // ST packing sets Q; RGBAQ packing takes it.
        unsigned image_qwords_left=0; // IMAGE data of the last tag still to come in later transfers.
        // Local->host transfer (TRXDIR 1): source rectangle and the bytes already read from the FIFO.
        bool download=false;unsigned dl_bp=0,dl_bw=0,dl_psm=0,dl_x=0,dl_y=0,dl_w=0,dl_h=0;std::size_t dl_at=0;
        unsigned finishes=0; // FINISH register writes: each raises the FINISH event (CSR bit 1).
    };
    // Local->host data still to be read, in quadwords (the transfer's size rounded up).
    std::size_t downloadQwords() const;
    // Advances the local->host transfer by out's size and zeroes out: the pixels are the GPU renderer's
    // (the runtime's gs_download_source fills them in).
    void readDownload(std::span<std::uint8_t> out);
    State state;
    unsigned transfers=0,register_writes=0;
    // Accepted GIF data in order, after validation (GPU mirror, docs/plans/graphics/GPU_RENDERING_PLAN.md).
    std::function<void(std::span<const std::uint8_t>)> on_accepted;
    // Every GIF transfer and startup packet as it arrives, before it is checked (the 60 fps in-between
    // pictures record a frame's GS input; never changes what the sink does).
    std::function<void(std::span<const std::uint8_t>)> on_input;
    // Optional (BT2_TEXTURE_NAMES): sees every register write and the image data, to name the textures
    // drawn as PCSX2 does (gs/textures/gs_texture_names.h); PACKED data is then decoded.
    GsTextureNames* texture_names=nullptr;
    // PSMCT32 word for block base pointer bp (64-word blocks) and buffer width bw (64-pixel units).
    static std::size_t ct32Word(unsigned bp,unsigned bw,unsigned x,unsigned y) {
        unsigned bx=(x%64)/8,by=(y%32)/8;
        unsigned block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2);
        unsigned px=x%8,py=y%8;
        unsigned pixel=(px&1)|((py&1)<<1)|((px&6)<<1)|((py&6)<<3);
        return (std::size_t(bp)*64+(std::size_t(y/32)*bw+x/64)*2048+block*64+pixel)%(1024*1024);
    }
    // PSMT4 nibble: 128x128 pages, 32x16 blocks, four 32x4 columns per block (128 nibbles each).
    // Within a column, pixel x = 8g+k places k's bits at nibble strides 8, 32 and 64 and g at stride 2;
    // odd rows add 16, rows 2-3 add 1 and flip the 64 stride, as do odd columns (public GS layout,
    // expressed as bit placement and checked against the reference layout). Returns word*8+nibble.
    static std::size_t t4Nibble(unsigned bp,unsigned bw,unsigned x,unsigned y) {
        const unsigned pages_per_row=bw/2;
        const unsigned page=(y/128)*pages_per_row+x/128;
        const unsigned bx=(x%128)/32,by=(y%128)/16;
        const unsigned block=(by&1)|((bx&1)<<1)|((by&2)<<1)|((bx&2)<<2)|((by&4)<<2);
        const unsigned cx=x%32,cy=y%16,column=cy/4,row=cy%4,k=cx%8,g=cx/8;
        const unsigned n=2*g+8*(k&1)+32*((k>>1)&1)+64*(((k>>2)&1)^(row>>1)^(column&1))+16*(row&1)+(row>>1);
        const std::size_t nibble=std::size_t(column)*128+n;
        return ((std::size_t(bp)+std::size_t(page)*32+block)*64*8+nibble)%(std::size_t(1024)*1024*8);
    }
    // PSMT8 byte: 128x64 pages, 16x16 blocks (PSMCT32 block order), four 16x4 columns per block
    // (64 bytes each). Within a column, x's bits 0-3 sit at byte strides 4, 16, 32 and 2; odd rows add
    // 8, rows 2-3 add 1 and flip the 32 stride, as do odd columns (public GS layout, expressed as bit
    // placement and checked against the reference layout). Returns word*4+byte.
    static std::size_t t8Byte(unsigned bp,unsigned bw,unsigned x,unsigned y) {
        const unsigned page=(y/64)*(bw/2)+x/128;
        const unsigned bx=(x%128)/16,by=(y%64)/16;
        const unsigned block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2);
        const unsigned cx=x%16,cy=y%16,column=cy/4,row=cy%4;
        const unsigned b=column*64+4*(cx&1)+16*((cx>>1)&1)+32*(((cx>>2)&1)^(row>>1)^(column&1))+2*((cx>>3)&1)+8*(row&1)+(row>>1);
        return ((std::size_t(bp)+std::size_t(page)*32+block)*64*4+b)%(std::size_t(1024)*1024*4);
    }
private:
    void run(std::span<const std::uint8_t> data,bool apply);
    // Texture names' lockstep (transferAll): the transfer being run, how much of it went on to the
    // renderer, and where the qword being run starts.
    std::span<const std::uint8_t> streaming_{};std::size_t forwarded_=0,cursor_=0;
    void forwardBefore(std::size_t at);
    void image(std::span<const std::uint8_t> data);
    void startTransfer(std::uint64_t dir);
    void packed(unsigned d,std::uint64_t lo,std::uint64_t hi,bool apply);
    void write(unsigned reg,std::uint64_t value,bool apply);
public:
    std::array<std::uint64_t,2> tex0{};
    std::uint64_t pabe=0;
    unsigned configuration_packets=0;
    unsigned packets=0,sprites=0;
    std::uint64_t pixels=0;
    std::array<std::uint64_t,14> registers{};
    // The startup clears accepted (consume): frame buffer, rectangle [x0,x1) x [y0,y1), colour and the
    // FRAME mask of the bits kept.
    struct Clear {unsigned fbp=0,width=0;int x0=0,y0=0,x1=0,y1=0;std::uint32_t color=0,mask=0;};
    std::vector<Clear> clears;
private:
    bool drawing_reset_=false;
};
