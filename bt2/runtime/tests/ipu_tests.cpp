#include <vector>
#include <cstring>
#include "ee/ipu_device.h"
#include "ee/ipu_vlc.h"
#include "ee/stopped_dma_channels.h"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {
void check(bool value,std::source_location where=std::source_location::current()) {
    if(!value) {std::printf("IPU test failed at line %u\n",unsigned(where.line()));std::exit(1);}
}
template<class F> bool refused(F f) {try {f();} catch(const std::runtime_error&) {return true;}return false;}
}

int main() {
    // sceIpuInit: reset, BCLR, two SETIQ tables from eight FIFO quadwords, SETVQ, SETTH, reset, BCLR.
    IpuDevice ipu;
    check(ipu.write32(0x10002010,0x40000000) && ipu.read32(0x10002010)==0u);
    check(ipu.write32(0x10002000,0));
    for(unsigned q=0;q<8;++q)check(ipu.writeFifo(0x10007010,{q*4,q*4+1,q*4+2,q*4+3}));
    check(ipu.read32(0x10002010)==8u); // IFC counts the queued quadwords; BUSY stays clear.
    check(refused([&]{ipu.writeFifo(0x10007010,{});}));  // The FIFO holds eight quadwords.
    check(ipu.write32(0x10002000,0x50000000) && ipu.iq_loaded[0] && ipu.intra_iq[4]==1);
    check(ipu.write32(0x10002000,0x58000000) && ipu.iq_loaded[1] && ipu.nonintra_iq[0]==16 && ipu.read32(0x10002010)==0u);
    // SETVQ without its two quadwords waits with BUSY set and completes when they arrive.
    check(ipu.write32(0x10002000,0x60000000) && ipu.busy() && (*ipu.read32(0x10002010)&0x80000000u));
    ipu.writeFifo(0x10007010,{0x00020001,0,0,0});ipu.writeFifo(0x10007010,{0,0,0,0x80000000});
    check(!ipu.busy() && ipu.vq_loaded && ipu.vq_clut[0]==1 && ipu.vq_clut[1]==2 && ipu.vq_clut[15]==0x8000);
    check(ipu.write32(0x10002000,0x90000000) && ipu.thresholds==0);
    check(ipu.write32(0x10002010,0x40000000) && ipu.resets==2 && ipu.commands==5);
    // Decode parameters (MP1 here) are retained and read back; read-only fields ignore writes.
    check(ipu.write32(0x10002010,0x00800000) && ipu.read32(0x10002010)==0x00800000u);
    check(ipu.write32(0x10002010,0x0080C108) && ipu.read32(0x10002010)==0x00800000u);
    // Decode commands and IPU DMA starts are refused; stopped channel registers are retained.
    check(refused([&]{ipu.write32(0x1000B400,0x100);})); // toIPU must read from memory (DIR 1).
    {
        // toIPU source chain feeding the input on demand: REF to 0x200 (1 qword), then END with one
        // qword after the tag. FDEC skips FB bits and returns the next 32 (MSB first, memory byte order).
        IpuDevice dev;std::vector<std::uint8_t> ram(0x400);
        auto tag=[&](std::uint32_t at,std::uint64_t lo){std::memcpy(ram.data()+at,&lo,8);};
        tag(0x100,1|(3ull<<28)|(0x200ull<<32));tag(0x110,1|(7ull<<28));
        const std::uint8_t first[16]={0x00,0x00,0x01,0xB3,0x20,0x01,0xC0,0x24};std::memcpy(ram.data()+0x200,first,16);
        ram[0x120]=0xAB;dev.ram=ram;
        check(dev.write32(0x1000B430,0x100) && dev.write32(0x1000B420,0) && dev.write32(0x1000B400,0x105));
        check(dev.write32(0x10002000,0x40000000) && *dev.read32(0x10002000)==0x000001B3u); // FDEC 0.
        check(!(*dev.read32(0x1000B400)&0x100) && *dev.read32(0x1000B430)==0x130);         // The start filled the FIFO: both tags done.
        check(dev.write32(0x10002000,0x40000020) && *dev.read32(0x10002000)==0x2001C024u); // FDEC 32.
        check(*dev.read32(0x10002030)==0x2001C024u && (*dev.read32(0x10002020)&0x7F)==32);
        // FDEC 96 + 32 bits reach into the END quadword.
        check(dev.write32(0x10002000,0x40000020) && dev.write32(0x10002000,0x40000020) && dev.write32(0x10002000,0x40000020));
        check(*dev.read32(0x10002000)==0xAB000000u);
        {
            // Clearing STR stops a running chain; a restart continues the interrupted tag's data.
            IpuDevice stop;std::vector<std::uint8_t> mem(0x400);
            auto put=[&](std::uint32_t at,std::uint64_t lo){std::memcpy(mem.data()+at,&lo,8);};
            // REF of 12 quadwords: the start fills the FIFO (8) and stops there; 4 remain in the tag.
            put(0x100,12|(3ull<<28)|(0x200ull<<32));put(0x110,1|(7ull<<28));mem[0x280]=0x77;stop.ram=mem;
            check(stop.write32(0x1000B430,0x100) && stop.write32(0x1000B400,0x105) && *stop.read32(0x1000B420)==4);
            check(((*stop.read32(0x10002020)>>8)&15)==8);
            check(stop.write32(0x1000B400,0x5) && !(*stop.read32(0x1000B400)&0x100) && *stop.read32(0x1000B420)==4);
            // BCLR empties the FIFO; the restart continues the interrupted tag at quadword 8 (0x280).
            check(stop.write32(0x10002000,0x00000000) && stop.write32(0x1000B400,0x105) && stop.write32(0x10002000,0x40000000));
            check(*stop.read32(0x10002000)==0x77000000u);
        }
        // Past the end of the input the command waits; a new transfer completes it.
        // (FB is 6 bits: two FDEC 63 need 63+63+32 bits, 30 more than the 128 left.)
        check(dev.write32(0x10002000,0x4000003F) && !dev.busy() && dev.write32(0x10002000,0x4000003F) && dev.busy());
        tag(0x130,1|(7ull<<28));ram[0x140]=0xCD;dev.ram=ram;
        check(dev.write32(0x1000B430,0x130) && dev.write32(0x1000B400,0x105) && !dev.busy() && *dev.read32(0x10002000)==(0xCDu<<22));
    }
    check(ipu.write32(0x1000B020,0x12345) && ipu.read32(0x1000B020)==0x2345u && !ipu.read32(0x10003000));
    // Stopped scratchpad channels: registers retained (QWC 16 bits, SADR within 16 KB), STR refused.
    StoppedDmaChannels spr{0x1000D000,0x1000D400};
    check(spr.write32(0x1000D400,0) && spr.write32(0x1000D420,0x12345) && spr.read32(0x1000D420)==0x2345u);
    check(spr.write32(0x1000D480,0x7FFF) && spr.read32(0x1000D480)==0x3FF0u && !spr.read32(0x1000D440) && !spr.write32(0x1000E000,0));
    check(refused([&]{spr.write32(0x1000D000,0x100);}));
    {
        // SETIQ at an unaligned bit pointer (a matrix inside an MPEG sequence header): 64 bytes from bit 4.
        IpuDevice bits;
        bits.write32(0x10002000,4); // BCLR with BP 4.
        for(unsigned q=0;q<5;++q) {
            std::array<std::uint32_t,4> words;std::uint8_t bytes[16];
            for(unsigned n=0;n<16;++n)bytes[n]=std::uint8_t((n<<4)|((n+1)&0xF)); // Nibbles n, n+1.
            std::memcpy(words.data(),bytes,16);bits.writeFifo(0x10007010,words);
        }
        check(bits.write32(0x10002000,0x50000000) && !bits.busy() && bits.iq_loaded[0]);
        check(bits.intra_iq[0]==0x11 && bits.intra_iq[1]==0x22 && bits.intra_iq[62]==0xFF && bits.intra_iq[63]==0x00 && (*bits.read32(0x10002020)&0x7F)==4);
    }
    {
        // VDEC: one header code per command; the result's low half is the value, ECD reports no code.
        IpuDevice vdec;
        // Bits: 011 (increment 2) | 0000 0001 000 (escape) | 1 (I type: intra) | 01 (I: intra+quant)
        //       | 0011 (motion code -2) | 1 (motion 0) | 11 (dmv -1) | 000... (invalid increment).
        vdec.writeFifo(0x10007010,{0x009F2260u,0,0,0}); // Bytes 60 22 9F, then zeros.
        vdec.write32(0x10002010,0x01000000);             // PCT 1 (I picture).
        check(vdec.write32(0x10002000,0x30000000) && *vdec.read32(0x10002000)==0x00030002u);
        check(vdec.write32(0x10002000,0x30000000) && *vdec.read32(0x10002000)==0x000B0023u);
        check(vdec.write32(0x10002000,0x34000000) && *vdec.read32(0x10002000)==1u);
        check(vdec.write32(0x10002000,0x34000000) && *vdec.read32(0x10002000)==0x11u);
        check(vdec.write32(0x10002000,0x38000000) && *vdec.read32(0x10002000)==0xFFFFFFFEu);
        check(vdec.write32(0x10002000,0x38000000) && *vdec.read32(0x10002000)==0x00010000u);
        check(vdec.write32(0x10002000,0x3C000000) && *vdec.read32(0x10002000)==0xFFFFFFFFu);
        check((*vdec.read32(0x10002020)&0x7F)==24 && !(*vdec.read32(0x10002010)&0x4000));
        check(vdec.write32(0x10002000,0x30000000) && *vdec.read32(0x10002000)==0u && (*vdec.read32(0x10002010)&0x4000));
        check(vdec.write32(0x10002000,0x40000000) && !(*vdec.read32(0x10002010)&0x4000)); // The next command clears ECD.
        // P and B macroblock types: frame-motion flag; only B reports the length.
        check(ipuMacroblockType(0x8000,2).result==0x8Au && ipuMacroblockType(0x4000,2).result==0x02u);
        check(ipuMacroblockType(0x1000,3).result==0x0005009Eu && ipuMacroblockType(0x1000,3).consumed==5);
        check(ipuMotionCode(0x0300).result==(0x000A0000u|16u) && ipuMotionCode(0x0300).consumed==11);
        check(ipuAddressIncrement(0x0300,false).result==0x000B0021u && ipuAddressIncrement(0x01E0,true).result==0x000B0022u && !ipuAddressIncrement(0x01E0,false).result);
        check(refused([]{ipuMacroblockType(0x8000,4);}));
    }
    {
        // BDEC: MPEG-1 intra macroblocks to RAM through fromIPU. Flat matrices (16), QSC 8 (scale 16).
        auto prepared=[](std::vector<std::uint8_t>& ram) {
            IpuDevice dev;dev.output_ram=ram;
            dev.write32(0x10002010,0x00800000); // MP1.
            for(unsigned table=0;table<2;++table) {
                for(unsigned q=0;q<4;++q)dev.writeFifo(0x10007010,{0x10101010u,0x10101010u,0x10101010u,0x10101010u});
                dev.write32(0x10002000,table?0x58000000u:0x50000000u);
            }
            return dev;
        };
        auto sample=[](const std::vector<std::uint8_t>& ram,unsigned index) {std::int16_t value;std::memcpy(&value,ram.data()+0x100+index*2,2);return int(value);};
        {
            // Every block: DC difference 0, end of block. All samples are the reset predictor, 128.
            std::vector<std::uint8_t> ram(0x800);auto dev=prepared(ram);
            dev.writeFifo(0x10007010,{0x2F22A594u,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu});
            check(dev.write32(0x10002000,0x2C080000) && dev.busy());       // The macroblock waits for fromIPU.
            check((*dev.read32(0x10002010)&0xF0)==0x80);                    // OFC: a full output FIFO.
            check(dev.write32(0x1000B010,0x100) && dev.write32(0x1000B020,48) && dev.write32(0x1000B000,0x100));
            check(!dev.busy() && dev.macroblocks==1 && !(*dev.read32(0x1000B000)&0x100) && *dev.read32(0x1000B020)==0u);
            check(sample(ram,0)==128 && sample(ram,255)==128 && sample(ram,256)==128 && sample(ram,383)==128);
            check((*dev.read32(0x10002020)&0x7F)==28 && !(*dev.read32(0x10002010)&0xC0F0));
        }
        {
            // First block: DC +1 and one AC coefficient (run 0, level 1 -> 15 after MPEG-1 oddification) at
            // horizontal frequency 1; zero bytes and a start code follow (SCD, pointer at the prefix).
            std::vector<std::uint8_t> ram(0x800);auto dev=prepared(ram);
            check(dev.write32(0x1000B010,0x100) && dev.write32(0x1000B020,48) && dev.write32(0x1000B000,0x100));
            dev.writeFifo(0x10007010,{0x44A4943Au,0x01000000u,0xFFFFFFFFu,0xFFFFFFFFu});
            check(dev.write32(0x10002000,0x2C080000) && !dev.busy());
            check(sample(ram,0)==132 && sample(ram,7)==126 && sample(ram,16)==132 && sample(ram,8)==129 && sample(ram,128)==129 && sample(ram,300)==128);
            check((*dev.read32(0x10002010)&0xC000)==0x8000u && (*dev.read32(0x10002020)&0x7F)==40);
            check(*dev.read32(0x10002030)==0x000001FFu);
        }
        {
            // Non-intra: pattern 32 ('1010': first luma block only), first coefficient '1' + sign 1:
            // level -1 at DC -> ((2+1)*256)>>5 = 24 -> 23 (odd) -> residual -23/8 rounds to -3.
            std::vector<std::uint8_t> ram(0x800);auto dev=prepared(ram);
            check(dev.write32(0x1000B010,0x100) && dev.write32(0x1000B020,48) && dev.write32(0x1000B000,0x100));
            dev.writeFifo(0x10007010,{0xFFFFFFAEu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu}); // 1010 11 10 | 1...
            check(dev.write32(0x10002000,0x20080000) && !dev.busy());
            check(sample(ram,0)==-3 && sample(ram,119)==-3 && sample(ram,8)==0 && sample(ram,256)==0);
        }
        {
            // MPEG-2 with intra table one (IVF): DC +1, then '110' + sign 0 (run 0, level 2 -> 32, kept
            // even), end of block '0110'; the other blocks are DC 0 and '0110'.
            std::vector<std::uint8_t> ram(0x800);auto dev=prepared(ram);
            dev.write32(0x10002010,0x00200000); // IVF, MPEG-2.
            check(dev.write32(0x1000B010,0x100) && dev.write32(0x1000B020,48) && dev.write32(0x1000B000,0x100));
            // 00 1 110 0 0110 | 100 0110 x3 | 00 0110 x2 | 1...
            dev.writeFifo(0x10007010,{0x46A3D138u,0xFFFF6F18u,0xFFFFFFFFu,0xFFFFFFFFu});
            check(dev.write32(0x10002000,0x2C080000) && !dev.busy() && (*dev.read32(0x10002020)&0x7F)==44);
            check(sample(ram,0)==135 && sample(ram,7)==123 && sample(ram,8)==129 && sample(ram,300)==128);
        }
        // The inverse DCT of a lone DC coefficient is flat; quantiser scales follow both mappings.
        std::array<int,64> flat{};flat[0]=80;ipuInverseDct(flat);
        check(flat[0]==10 && flat[63]==10);
        check(ipuQuantiserScale(8,false)==16 && ipuQuantiserScale(8,true)==8 && ipuQuantiserScale(31,true)==112);
        // IDEC, CSC and PACK stay refused.
        IpuDevice other;
        check(refused([&]{other.write32(0x10002000,0x10000000);}) && refused([&]{other.write32(0x10002000,0x78000001);}));
        {
            // CSC: two RAW8 macroblocks through the input FIFO to RGB32 in RAM; the command waits for
            // input and output and ends with the last macroblock.
            std::vector<std::uint8_t> ram(0x1000);IpuDevice csc;csc.output_ram=ram;
            csc.write32(0x10002000,0x90000000|0x10);                 // SETTH: TH0 16.
            check(csc.write32(0x10002000,0x70000002) && csc.busy());
            check(csc.write32(0x1000B010,0x100) && csc.write32(0x1000B020,128) && csc.write32(0x1000B000,0x100));
            auto feed=[&](std::uint8_t y,std::uint8_t cb,std::uint8_t cr) {
                for(unsigned q=0;q<24;++q) {
                    const std::uint32_t byte=q<16?y:q<20?cb:cr,word=byte*0x01010101u;
                    csc.writeFifo(0x10007010,{word,word,word,word});
                }
            };
            feed(128,128,200);check(csc.busy() && csc.converted_macroblocks==1);
            feed(16,128,128);check(!csc.busy() && csc.converted_macroblocks==2 && !(*csc.read32(0x1000B000)&0x100));
            std::uint32_t first,last;std::memcpy(&first,ram.data()+0x100,4);std::memcpy(&last,ram.data()+0x100+511*4,4);
            check(first==0x808248F5u && last==0u); // R 245, G 72, B 130, A 0x80; black below TH0 is transparent.
        }
    }
    std::printf("IPU setup register and command tests passed\n");
}
