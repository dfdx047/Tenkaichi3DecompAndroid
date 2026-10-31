// EE DMA source-chain walking (synthetic tags; no game data).
#include "ee/dma_chain.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"DMA chain check failed: "<<what<<'\n';std::exit(1);}}
void tag(std::vector<std::uint8_t>& ram,std::uint32_t at,std::uint32_t id,std::uint32_t qwc,std::uint32_t addr,bool irq=false) {
    const std::uint64_t lo=qwc|(std::uint64_t(id)<<28)|(irq?0x80000000ull:0)|(std::uint64_t(addr)<<32);
    std::memcpy(ram.data()+at,&lo,8);
}
template<class F> bool throws(F f) {try{f();}catch(const std::runtime_error&){return true;}return false;}
}

int main() {
    std::vector<std::uint8_t> ram(0x1000);
    // CNT (2 qwords follow) -> REF (1 qword at 0x800) -> NEXT to 0x400 -> END (1 qword follows).
    tag(ram,0x100,1,2,0);tag(ram,0x130,3,1,0x800);tag(ram,0x140,2,0,0x400);tag(ram,0x400,7,1,0);
    const auto chain=walkDmaSourceChain(ram,0x100,"TEST");
    check(chain.segments.size()==4,"four tags");
    check(chain.segments[0].data.data()==ram.data()+0x110 && chain.segments[0].data.size()==32,"CNT data follows the tag");
    check(chain.segments[1].data.data()==ram.data()+0x800 && chain.segments[1].data.size()==16,"REF data at ADDR");
    check(chain.segments[2].data.empty() && chain.segments[3].tag==0x400,"NEXT jumps to ADDR");
    check(chain.next_tadr==0x420,"END leaves TADR after its data");
    check(chain.segments[0].tag_words==ram.data()+0x108,"tag upper words");
    // REFE ends after the tag; unmodelled forms throw before any effect.
    tag(ram,0x200,0,1,0x800);check(walkDmaSourceChain(ram,0x200,"TEST").next_tadr==0x210,"REFE ends the chain");
    tag(ram,0x300,7,0,0,true);check(throws([&]{walkDmaSourceChain(ram,0x300,"TEST");}),"tag IRQ rejected");
    // CALL runs a sub-list and RET comes back after the caller's data; a third nested CALL is refused
    // and a RET without a caller ends the transfer.
    tag(ram,0x300,5,1,0x500);tag(ram,0x320,7,0,0);tag(ram,0x500,1,1,0);tag(ram,0x520,6,0,0);
    const auto called=walkDmaSourceChain(ram,0x300,"TEST");
    check(called.segments.size()==4 && called.segments[0].data.data()==ram.data()+0x310 && called.segments[1].tag==0x500
        && called.segments[2].tag==0x520 && called.segments[3].tag==0x320 && called.next_tadr==0x330,"CALL and RET");
    tag(ram,0x500,5,0,0x540);tag(ram,0x540,5,0,0x560);tag(ram,0x560,7,0,0);
    check(throws([&]{walkDmaSourceChain(ram,0x300,"TEST");}),"third call level rejected");
    tag(ram,0x600,6,1,0);check(walkDmaSourceChain(ram,0x600,"TEST").next_tadr==0x620,"RET without a caller ends");
    check(throws([&]{walkDmaSourceChain(ram,0x108,"TEST");}),"unaligned TADR rejected");
    tag(ram,0x300,3,4,0xFF0);check(throws([&]{walkDmaSourceChain(ram,0x300,"TEST");}),"data outside RAM rejected");
    std::cout<<"DMA chain tests passed\n";
    return 0;
}
