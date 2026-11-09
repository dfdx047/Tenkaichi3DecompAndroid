#include "ee/frame_profile.h"
#include "vif1.h"
#include <chrono>
#include <cstdio>
#include <cstring>

namespace {
[[noreturn]] void refuse(const char* what,std::uint32_t code,std::size_t index) {
    char text[128];std::snprintf(text,sizeof text,"VIF1 %s (code 0x%08X) is not modelled",what,code);
    throw Vif1Unsupported(code,index,text);
}
}

void Vif1::process(std::span<const std::uint32_t> words,Registers& r,Vif1* self,const Vu1::Kick* direct,const Vu1::Kick* kick) {
    for(std::size_t at=0;at<words.size();) {
        const std::uint32_t code=words[at];
        const unsigned command=(code>>24)&0x7F,num=(code>>16)&0xFF,imm=code&0xFFFF;
        if(code&0x80000000u)refuse("interrupt bit",code,at);
        // The code's data words follow it in the same transfer.
        auto operands=[&](std::size_t count) {
            if(count>words.size()-at-1)refuse("data continuing in the next transfer",code,at);
            const auto data=words.subspan(at+1,count);at+=1+count;return data;
        };
        if(self)++self->codes;
        if(command>=0x60) {
            // UNPACK vn/vl: `num` vectors to VU1 data memory from address imm (plus TOPS with FLG).
            // Modelled: the forms that define all four fields (S and V4), written to consecutive
            // quadwords (CL = WL), without masking (m) or an addition mode (STMOD).
            const unsigned vn=(command>>2)&3,vl=command&3,cl=r.cycle&0xFF,wl=(r.cycle>>8)&0xFF;
            const bool usn=imm&0x4000,v45=vn==3 && vl==3;
            if((vn!=0 && vn!=3) || (vl==3 && !v45))refuse("UNPACK format",code,at);
            if(command&0x10)refuse("UNPACK with masking",code,at);
            if(r.mode)refuse("UNPACK with an addition mode",code,at);
            if(cl!=wl || !wl)refuse("UNPACK with a skipping or filling write cycle",code,at);
            const unsigned count=num?num:256,fields=vn+1,bytes=v45?2:(4u>>vl)*fields;
            const std::uint32_t address=(imm&0x3FF)+((imm&0x8000)?r.tops:0);
            const auto data=operands((std::size_t(count)*bytes+3)/4);
            if(!self)continue;
            const auto* from=reinterpret_cast<const std::uint8_t*>(data.data());
            auto element=[&](std::size_t n) -> std::uint32_t {
                if(vl==0) {std::uint32_t v;std::memcpy(&v,from+n*4,4);return v;}
                if(vl==1) {std::uint16_t v;std::memcpy(&v,from+n*2,2);return usn?v:std::uint32_t(std::int32_t(std::int16_t(v)));}
                return usn?from[n]:std::uint32_t(std::int32_t(std::int8_t(from[n])));
            };
            for(unsigned n=0;n<count;++n) {
                std::array<std::uint32_t,4> vector;
                if(v45) { // RGBA 5:5:5:1 to four 8-bit values.
                    std::uint16_t v;std::memcpy(&v,from+n*2,2);
                    vector={(v&31u)<<3,((v>>5)&31u)<<3,((v>>10)&31u)<<3,(v>>15)*0x80u};
                } else for(unsigned k=0;k<4;++k)vector[k]=element(std::size_t(n)*fields+(vn?k:0));
                std::memcpy(self->vu.data.data()+((address+n)&Vu1::data_mask)*16,vector.data(),16);
            }
            ++self->unpacks;self->unpacked_qwords+=count;
            continue;
        }
        switch(command) {
            case 0x00:++at;break;                                   // NOP
            case 0x01:r.cycle=imm;++at;break;                       // STCYCL
            case 0x02:r.ofst=imm&0x3FF;r.dbf=false;r.tops=r.base;++at;break; // OFFSET
            case 0x03:r.base=imm&0x3FF;++at;break;                  // BASE
            case 0x04:r.itops=imm&0x3FF;++at;break;                 // ITOP
            case 0x05:r.mode=imm&3;++at;break;                      // STMOD
            case 0x06:r.mskpath3=(imm&0x8000)!=0;++at;break;        // MSKPATH3
            case 0x07:r.mark=imm;++at;break;                        // MARK
            case 0x10:case 0x11:case 0x13:++at;break;               // FLUSHE, FLUSH, FLUSHA: nothing runs behind a code.
            case 0x14:case 0x15:case 0x17:                          // MSCAL, MSCALF, MSCNT
                ++at;
                r.top=r.tops;r.itop=r.itops;
                r.dbf=!r.dbf;r.tops=(r.base+(r.dbf?r.ofst:0))&0x3FF; // The double buffers swap at each start.
                if(!self)break;
                ++self->calls;self->vu.top=r.top;self->vu.itop=r.itop;
                {
                    FrameProfile::Scope timed(frame_profile.vu1);
                    const bool measured=bool(self->vu.on_ran);
                    const auto began=measured?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
                    const std::uint32_t from=(command==0x17?self->vu.pc:imm)&Vu1::program_mask;const auto before=self->vu.instructions;
                    if(command==0x17)self->vu.resume(*kick); else self->vu.run(imm,*kick);
                    if(measured)self->vu.on_ran(from,self->vu.instructions-before,std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count()));
                }
                break;
            case 0x20:r.mask=operands(1)[0];break;                  // STMASK
            case 0x30:case 0x31: {                                  // STROW, STCOL
                const auto data=operands(4);
                std::memcpy((command==0x30?r.row:r.col).data(),data.data(),16);
                break;
            }
            case 0x4A: {                                            // MPG: num instructions at address imm
                const unsigned count=num?num:256;
                const auto data=operands(std::size_t(count)*2);
                if(!self)break;
                for(unsigned n=0;n<count;++n)self->vu.writeProgram(imm+n,data.data()+n*2);
                ++self->programs;
                break;
            }
            case 0x50:case 0x51: {                                  // DIRECT, DIRECTHL: imm quadwords of GIF data
                const std::size_t qwords=imm?imm:65536;
                const auto data=operands(qwords*4);
                if(!self)break;
                ++self->directs;self->direct_qwords+=qwords;
                if(*direct)(*direct)({reinterpret_cast<const std::uint8_t*>(data.data()),qwords*16});
                break;
            }
            default:refuse("command",code,at);
        }
    }
}
