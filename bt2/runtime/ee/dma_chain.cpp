#include "dma_chain.h"
#include <cstring>
#include <stdexcept>
#include <string>

DmaChain walkDmaSourceChain(std::span<const std::uint8_t> ram,std::uint32_t tadr,const char* channel) {
    auto fail=[&](const char* what){throw std::runtime_error(std::string(channel)+what);};
    DmaChain chain;std::uint32_t tag=tadr;
    std::uint32_t returns[2];unsigned depth=0; // The channel's two address stack registers (ASR0, ASR1).
    for(unsigned n=0;;++n) {
        if(n>=65536 || tag%16 || tag>=ram.size()-16)fail(" DMA chain outside RAM");
        std::uint64_t lo;std::memcpy(&lo,ram.data()+tag,8);
        const std::uint32_t qwc=std::uint32_t(lo&0xFFFF),id=std::uint32_t((lo>>28)&7),addr=std::uint32_t((lo>>32)&0x7FFFFFF0);
        if(lo&0x80000000)fail(" DMA tag IRQ is not modelled");
        DmaChainSegment segment{tag,ram.data()+tag+8,{}};
        if(qwc) {
            const auto data=(id==3 || id==4 || id==0)?addr:tag+16; // REF/REFS/REFE: data at ADDR; otherwise it follows the tag.
            if(data>=ram.size() || std::size_t(qwc)*16>ram.size()-data)fail(" DMA data outside RAM");
            segment.data=ram.subspan(data,std::size_t(qwc)*16);
        }
        chain.segments.push_back(segment);
        if(id==0 || id==7) {chain.next_tadr=id==7?tag+16+qwc*16:tag+16;return chain;}
        if(id==5) { // CALL: continue at ADDR and come back after this tag's data.
            if(depth==2)fail(" DMA chain calls nested deeper than two levels");
            returns[depth++]=tag+16+qwc*16;tag=addr;continue;
        }
        if(id==6) { // RET: back to the caller; without one the transfer ends here.
            if(!depth) {chain.next_tadr=tag+16+qwc*16;return chain;}
            tag=returns[--depth];continue;
        }
        tag=id==1?tag+16+qwc*16:id==2?addr:tag+16; // CNT: after the data; NEXT: ADDR; REF/REFS: next qword.
    }
}
