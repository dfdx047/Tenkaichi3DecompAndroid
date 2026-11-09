#include <map>
#include <string>
#include <cstdio>
#include <algorithm>
#include "gs_command_sink.h"
#include "gs/textures/gs_texture_names.h"

// GsCommandSink implementation: GIF parsing, register writes and transfer tracking. Kept out of the
// header so changes rebuild one translation unit instead of every generated unit that includes
// ps2_runtime.h.

void GsCommandSink::consume(std::span<const std::uint8_t> packet) {
    if(on_input && packet.size()==240)on_input(packet); // Configuration packets are reported by consumeConfiguration.
    if(packet.size()!=240) {consumeConfiguration(packet);return;}
    auto read=[&](std::size_t offset) {std::uint64_t v;std::memcpy(&v,packet.data()+offset,8);return v;};
    auto reject=[](){throw std::runtime_error("Unsupported GS sprite packet/state");};
    if(!drawing_reset_ || packet.size()!=240)reject();
    if(read(0)!=0x100000000000800EULL || read(8)!=0xE)reject();
    constexpr std::array<unsigned,14> regs={0x4C,0x4E,0x18,0x40,0x1A,0x46,0x45,0x47,0x47,0,1,5,5,0x47};
    std::array<std::uint64_t,14> v{};
    for(unsigned i=0;i<14;++i) {if(read(24+i*16)!=regs[i])reject();v[i]=read(16+i*16);}
    unsigned width=unsigned((v[0]>>16)&63),fbp=unsigned(v[0]&511),zbp=unsigned(v[1]&511);
    if(!width || ((v[0]>>24)&63)!=0 || (v[0]&0xC0C0FE00ULL)
        || ((v[1]>>24)&63)!=1 || (v[1]&~0x1000001FFULL)!=0x1000000ULL
        || v[4]%2!=1 || v[5]%2!=1 || v[6]%2!=0 || v[7]!=0x50000
        || v[8]!=0x30000 || v[9]!=6 || v[13]!=0x50000)reject();
    // At the draw: alpha/destination tests disabled, depth ALWAYS, no blend,
    // texture, fog or dither. ZMSK is supported; FRAME mask preserves bits.
    unsigned ox=unsigned(v[2]&65535),oy=unsigned((v[2]>>32)&65535);
    if(v[2]&0xFFFF0000FFFF0000ULL || v[3]&0xF800F800F800F800ULL)reject();
    auto coordinate=[&](std::uint64_t vertex,bool y) {
        int n=int((vertex>>(y?16:0))&65535)-int(y?oy:ox);
        if(n%16)reject();return n/16;
    };
    int x0=coordinate(v[11],false),y0=coordinate(v[11],true);
    int x1=coordinate(v[12],false),y1=coordinate(v[12],true);
    if(x1<x0 || y1<y0)reject();
    x0=std::max(x0,int(v[3]&2047));y0=std::max(y0,int((v[3]>>32)&2047));
    x1=std::min(x1,int((v[3]>>16)&2047)+1);y1=std::min(y1,int((v[3]>>48)&2047)+1);
    if(x0<0 || y0<0 || x1>int(width*64) || y1>2048)reject();
    // All rejection occurs above: no partial effects on malformed requests.
    (void)zbp;
    if(clears.size()==64)clears.erase(clears.begin()); // The startup checks need the last few.
    clears.push_back({fbp,width,x0,y0,x1,y1,std::uint32_t(v[10]),std::uint32_t(v[0]>>32)});
    pixels+=std::uint64_t(x1-x0)*std::uint64_t(y1-y0);
    registers=v;++packets;++sprites;
    if(on_accepted)on_accepted(packet);
}

void GsCommandSink::consumeConfiguration(std::span<const std::uint8_t> packet) {
    if(on_input)on_input(packet);
    auto reject=[](){throw std::runtime_error("Unsupported GS configuration packet");};
    if(!drawing_reset_ || packet.size()<32 || packet.size()%16)reject();
    auto read=[&](std::size_t offset) {std::uint64_t value;std::memcpy(&value,packet.data()+offset,8);return value;};
    auto tag=read(0);const auto loops=tag&32767;
    // Packed, one A+D register, EOP, no PRE/primitive side effect.
    if(tag!=(0x1000000000008000ULL|loops) || read(8)!=14 || loops!=(packet.size()-16)/16)reject();
    auto next_tex0=tex0;auto next_pabe=pabe;
    for(std::size_t i=16;i<packet.size();i+=16) {
        const auto value=read(i),reg=read(i+8);
        if(reg==0x49)next_pabe=value&1;
        else if(reg==8 || reg==9) {
            // CLD requires a CLUT transaction, not a register-only update.
            if(value>>61)reject();
            next_tex0[reg-8]=value;
        } else reject();
    }
    tex0=next_tex0;pabe=next_pabe;++packets;++configuration_packets;
    if(on_accepted)on_accepted(packet);
}

void GsCommandSink::transferAll(std::span<const std::span<const std::uint8_t>> group) {
    if(on_input)for(const auto& data:group)on_input(data);
    for(const auto& data:group)if(data.empty() || data.size()%16)throw std::runtime_error("Unsupported GIF transfer size");
    // Rollback: the register state and the counters are copied.
    const auto saved_state=state;
    const std::array<unsigned,2> saved_counts{transfers,register_writes};
    // With texture names, the data goes on to the renderer in pieces that end before each change of GS
    // memory or of the CLUT the names follow (forwardBefore), so the names' copy is never ahead of the
    // renderer when it takes a texture; a refused group still ends the run, as it would anyway.
    const bool lockstep=texture_names && on_accepted;
    try {
        for(const auto& data:group) {
            if(lockstep) {streaming_=data;forwarded_=0;cursor_=0;}
            run(data,true);
            if(lockstep) {forwardBefore(data.size());streaming_={};}
            ++transfers;
        }
    }
    catch(...) {
        streaming_={};
        state=saved_state;transfers=saved_counts[0];register_writes=saved_counts[1];throw;
    }
    if(on_accepted && !lockstep)for(const auto& data:group)on_accepted(data);
}

void GsCommandSink::forwardBefore(std::size_t at) {
    if(streaming_.empty() || at<=forwarded_)return;
    on_accepted(streaming_.subspan(forwarded_,at-forwarded_));forwarded_=at;
}

void GsCommandSink::run(std::span<const std::uint8_t> data,bool apply) {
    auto read=[&](std::size_t offset) {std::uint64_t v;std::memcpy(&v,data.data()+offset,8);return v;};
    std::size_t at=0;
    // The GIF path keeps its tag across DMA transfers: IMAGE data left over from an earlier transfer comes first.
    if(state.image_qwords_left) {
        const std::size_t take=std::min<std::size_t>(state.image_qwords_left,data.size()/16);
        cursor_=0;image(data.subspan(0,take*16));at=take*16;state.image_qwords_left-=unsigned(take);
    }
    while(at<data.size()) {
        const auto tag=read(at),regs=read(at+8);at+=16;
        const unsigned nloop=unsigned(tag&0x7FFF),flg=unsigned((tag>>58)&3);
        const unsigned nreg=unsigned((tag>>60)&15)?unsigned((tag>>60)&15):16;
        if((tag>>46)&1)write(0x00,(tag>>47)&0x7FF,apply); // PRE: PRIM from the tag.
        if(flg==0) {
            if(at+std::size_t(nloop)*nreg*16>data.size())throw std::runtime_error("GIF PACKED data exceeds transfer");
            if(!texture_names) {
                // The renderer draws: this follows only what is set through A+D (the transfer registers,
                // FINISH); vertex and texture data pass by unread.
                for(unsigned l=0;l<nloop;++l)for(unsigned r=0;r<nreg;++r,at+=16)
                    if(((regs>>(4*r))&15)==0xE)write(unsigned(read(at+8)&0xFF),read(at),apply);
            } else for(unsigned l=0;l<nloop;++l)for(unsigned r=0;r<nreg;++r,at+=16) {cursor_=at;packed(unsigned((regs>>(4*r))&15),read(at),read(at+8),apply);}
        } else if(flg==1) {
            const std::size_t words=std::size_t(nloop)*nreg;
            if(at+((words+1)/2)*16>data.size())throw std::runtime_error("GIF REGLIST data exceeds transfer");
            for(std::size_t w=0;w<words;++w) {
                const unsigned d=unsigned((regs>>(4*(w%nreg)))&15);
                // A+D has no REGLIST meaning; that descriptor's data is not a register write.
                if(d!=0xE && d!=0xF) {cursor_=at+w/2*16;write(d,read(at+w*8),apply);}
            }
            at+=((words+1)/2)*16;
        } else {
            const std::size_t take=std::min<std::size_t>(nloop,(data.size()-at)/16);
            cursor_=at;image(data.subspan(at,take*16));at+=take*16;state.image_qwords_left=nloop-unsigned(take);
        }
        // EOP ends this GIF packet; any further data in the transfer starts the next packet.
    }
}

void GsCommandSink::image(std::span<const std::uint8_t> data) {
    // Host->local pixels: the renderer's GS memory takes them; the texture names follow them.
    if(texture_names) {forwardBefore(cursor_);texture_names->image(data);}
}

void GsCommandSink::startTransfer(std::uint64_t dir) {
    const auto bitblt=state.regs[0x50],pos=state.regs[0x51],reg=state.regs[0x52];
    const unsigned dbp=unsigned((bitblt>>32)&0x3FFF),dbw=unsigned((bitblt>>48)&63),dpsm=unsigned((bitblt>>56)&63);
    const unsigned dx=unsigned((pos>>32)&0x7FF),dy=unsigned((pos>>48)&0x7FF),order=unsigned((pos>>59)&3);
    const unsigned w=unsigned(reg&0xFFF),h=unsigned((reg>>32)&0xFFF);
    // A TRXDIR write starts a new transfer; an incomplete earlier one is abandoned, as on the GS. The
    // renderer performs host->local and local->local transfers; local->host is followed here so its
    // FIFO reads can be counted.
    state.download=false;
    if((dir&3)==1) {
        // Local->host: the source rectangle is read back through the FIFO (the renderer supplies the pixels).
        const unsigned sbp=unsigned(bitblt&0x3FFF),sbw=unsigned((bitblt>>16)&63),spsm=unsigned((bitblt>>24)&63);
        const unsigned sx=unsigned(pos&0x7FF),sy=unsigned((pos>>16)&0x7FF);
        if(!w || !h || !sbw)
            throw std::runtime_error("Unsupported GS local-to-host transfer: spsm "+std::to_string(spsm)+" "+std::to_string(w)+"x"+std::to_string(h)
                +" at "+std::to_string(sx)+","+std::to_string(sy)+" sbw "+std::to_string(sbw));
        state.download=true;state.dl_bp=sbp;state.dl_bw=sbw;state.dl_psm=spsm;state.dl_x=sx;state.dl_y=sy;state.dl_w=w;state.dl_h=h;state.dl_at=0;
        return;
    }
    (void)dbp;(void)dbw;(void)dpsm;(void)dx;(void)dy;(void)order;
}

namespace {
// Bits per pixel of a transfer format (PSMCT32 0, CT24 1, CT16 2, CT16S 0xA, T8 0x13, T4 0x14, the
// high-byte formats 0x1B/0x24/0x2C and the depth formats 0x30-0x3A).
unsigned transferBits(unsigned psm) {
    switch(psm) {
    case 0x00:case 0x30:return 32;
    case 0x01:case 0x31:return 24;
    case 0x02:case 0x0A:case 0x32:case 0x3A:return 16;
    case 0x13:case 0x1B:return 8;
    case 0x14:case 0x24:case 0x2C:return 4;
    default:throw std::runtime_error("Unsupported GS transfer format "+std::to_string(psm));
    }
}
}

std::size_t GsCommandSink::downloadQwords() const {
    if(!state.download)return 0;
    const std::size_t bytes=(std::size_t(state.dl_w)*state.dl_h*transferBits(state.dl_psm)+7)/8;
    return bytes>state.dl_at?(bytes-state.dl_at+15)/16:0;
}

void GsCommandSink::readDownload(std::span<std::uint8_t> out) {
    if(!state.download)throw std::runtime_error("GS FIFO read without a local-to-host transfer");
    std::fill(out.begin(),out.end(),std::uint8_t(0));
    state.dl_at+=out.size();
}

void GsCommandSink::packed(unsigned d,std::uint64_t lo,std::uint64_t hi,bool apply) {
    switch(d) {
    case 0x0: write(0x00,lo&0x7FF,apply);break;
    case 0x1: write(0x01,(lo&0xFF)|((lo>>24)&0xFF00)|((hi&0xFF)<<16)|(((hi>>32)&0xFF)<<24)|(std::uint64_t(state.q)<<32),apply);break;
    case 0x2: state.q=std::uint32_t(hi);write(0x02,lo,apply);break;
    case 0x3: write(0x03,(lo&0x3FFF)|(((lo>>32)&0x3FFF)<<16),apply);break;
    case 0x4: write((hi>>47)&1?0x0C:0x04,(lo&0xFFFF)|(((lo>>32)&0xFFFF)<<16)|(((hi>>4)&0xFFFFFF)<<32)|(((hi>>36)&0xFF)<<56),apply);break;
    case 0x5: write((hi>>47)&1?0x0D:0x05,(lo&0xFFFF)|(((lo>>32)&0xFFFF)<<16)|((hi&0xFFFFFFFF)<<32),apply);break;
    case 0xA: write(0x0A,((hi>>36)&0xFF)<<56,apply);break;
    case 0xE: write(unsigned(hi&0xFF),lo,apply);break;
    case 0xF: break;
    default: write(d,lo,apply);break; // TEX0_1/2, CLAMP_1/2, XYZF3, XYZ3: register-valued.
    }
}

void GsCommandSink::write(unsigned reg,std::uint64_t value,bool apply) {
    if(reg>=state.regs.size() || (reg>0x0F && reg<0x14) || reg==0x54)
        throw std::runtime_error("Unsupported GS register write");
    state.regs[reg]=value;if(apply)++register_writes;
    if(texture_names) {
        if((reg==0x06 || reg==0x07 || reg==0x16 || reg==0x17) && (value>>61))forwardBefore(cursor_); // A CLUT load.
        texture_names->reg(reg,value);
    }
    if(reg==0x53) {startTransfer(value);return;}
    if(reg==0x61) {++state.finishes;return;}
    if(reg==0x06 || reg==0x07) {
        const unsigned cld=unsigned((value>>61)&7);
        if(cld>1)throw std::runtime_error("Unsupported GS CLUT load control");
        return; // The renderer loads the CLUT.
    }
    // Vertices and primitives are the renderer's.
}
