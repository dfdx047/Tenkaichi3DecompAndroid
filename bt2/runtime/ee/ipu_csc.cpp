#include "ipu_csc.h"

namespace {
std::uint8_t component(int value) {
    value=(value+1)>>1;
    return std::uint8_t(value<0?0:value>255?255:value);
}
}

void ipuConvertMacroblock(const std::uint8_t* raw8,std::uint32_t thresholds,std::uint8_t* rgb32) {
    const unsigned th0=thresholds&0x1FF,th1=(thresholds>>16)&0x1FF;
    const std::uint8_t* cb_plane=raw8+256;
    const std::uint8_t* cr_plane=raw8+320;
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
        const int luma=int(raw8[y*16+x])-16,cb=int(cb_plane[(y/2)*8+x/2])-128,cr=int(cr_plane[(y/2)*8+x/2])-128;
        const int lum=(0x95*(luma<0?0:luma))>>6;
        std::uint8_t* pixel=rgb32+(y*16+x)*4;
        pixel[0]=component(lum+((0xCC*cr)>>6));
        pixel[1]=component(lum+((-0x68*cr)>>6)+((-0x32*cb)>>6));
        pixel[2]=component(lum+((0x102*cb)>>6));
        pixel[3]=0x80;
        const auto below=[&](unsigned limit){return pixel[0]<limit && pixel[1]<limit && pixel[2]<limit;};
        if(th0 && below(th0))pixel[0]=pixel[1]=pixel[2]=pixel[3]=0;
        else if(th1 && below(th1))pixel[3]=0x40;
    }
}
