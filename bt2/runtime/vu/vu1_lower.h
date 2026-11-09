// VU1 lower instructions, inline (included at the end of vu1.h). The native translations of microprograms
// (game/generated/vu1/vu1_native.cpp) call `lower` with the instruction word as a constant at hundreds of places,
// hundreds of thousands of times a frame in a fight: as an ordinary function each call was a call plus the
// decoding of a word known at compile time. Inline, the compiler keeps only the instruction's own work at each
// place. DIV and XGKICK stay out of line (vu1.cpp): they depend on the float mode, which the compiler does not
// see change (vu1.cpp sets it under `fenv_access`), and they are rare.
#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define VU1_INLINE [[gnu::always_inline]] inline
#else
#define VU1_INLINE __forceinline
#endif

namespace vu1_decode {
[[noreturn]] void refuse(const char* what,std::uint32_t word,std::uint32_t at); // (vu1.cpp: stops the run naming it)
inline std::uint32_t word(const std::uint8_t* at) {std::uint32_t w;std::memcpy(&w,at,4);return w;}
inline int imm11(std::uint32_t w) {return int(w&0x7FF)-int((w&0x400)<<1);}
inline unsigned viField(unsigned reg,std::uint32_t w,std::uint32_t at) {if(reg>15)refuse("integer register in",w,at);return reg;}
}

inline std::uint32_t Vu1::readClip() {if(clip_count_)++early_clip_reads;return clip;}

inline void Vu1::setVi(unsigned reg,std::uint16_t v,bool flag_instruction) {
    if(!reg)return;
    vi[reg]=v;
    if(!flag_instruction) {vi_written_=reg;vi_written_cycle_=cycle_;}
}

// A branch reads its integer registers earlier in the pipeline than the previous instruction writes
// them. The flag instructions (FCAND, FCOR ...) are followed directly by their branch throughout the
// game's programs and are read as written; the delayed value after any other writer is not modelled.
inline std::uint16_t Vu1::branchVi(unsigned reg,std::uint32_t at) const {
    if(reg && vi_written_==reg && vi_written_cycle_+1==cycle_)refuseBranch(reg,at);
    return vi[reg];
}

VU1_INLINE void Vu1::lower(std::uint32_t w,std::uint32_t at,const Kick& kick) {
    using namespace vu1_decode;
    const unsigned op=w>>25,it=(w>>16)&31,is=(w>>11)&31,dest=(w>>21)&15;
    auto load=[&](unsigned reg,std::uint32_t address) {
        if(!reg)return;
        const auto* from=cell(address);
        for(unsigned lane=0;lane<4;++lane)if(dest&(8u>>lane))vf[reg][lane]=word(from+lane*4);
    };
    auto store=[&](std::uint32_t address,const std::array<std::uint32_t,4>& v) {
        auto* to=cell(address);
        for(unsigned lane=0;lane<4;++lane)if(dest&(8u>>lane))std::memcpy(to+lane*4,&v[lane],4);
    };
    auto branchTo=[&](std::uint32_t target) {branch_=true;branch_target_=target&program_mask;};
    auto integer=[&](unsigned reg) {return viField(reg,w,at);};
    switch(op) {
        case 0x00:load(it,vi[integer(is)]+imm11(w));return;                 // LQ ft, imm(is)
        case 0x01:store(vi[integer(it)]+imm11(w),vf[is]);return;            // SQ fs, imm(it)
        case 0x04: {                                                        // ILW it, imm(is): the dest field
            if(dest!=8 && dest!=4 && dest!=2 && dest!=1)refuse("ILW field mask in",w,at);
            const unsigned lane=dest==8?0:dest==4?1:dest==2?2:3;
            setVi(integer(it),std::uint16_t(word(cell(vi[integer(is)]+imm11(w))+lane*4)));return;
        }
        case 0x05: {const std::uint32_t v=vi[integer(it)];store(vi[integer(is)]+imm11(w),{v,v,v,v});return;} // ISW
        case 0x08:case 0x09: {                                              // IADDIU, ISUBIU it, is, imm15
            const std::uint32_t imm=((w>>10)&0x7800)|(w&0x7FF),base=vi[integer(is)];
            setVi(integer(it),std::uint16_t(op==8?base+imm:base-imm));return;
        }
        case 0x12:setVi(1,(readClip()&w&0xFFFFFFu)?1:0,true);return;        // FCAND
        case 0x13:setVi(1,((readClip()|w)&0xFFFFFFu)==0xFFFFFFu?1:0,true);return; // FCOR
        case 0x1C:setVi(integer(it),std::uint16_t(readClip()&0xFFF),true);return; // FCGET
        case 0x20:branchTo(at+1+imm11(w));return;                           // B
        case 0x21:setVi(integer(it),std::uint16_t(at+2));branchTo(at+1+imm11(w));return; // BAL
        case 0x24:branchTo(branchVi(integer(is),at));return;                // JR
        case 0x28:case 0x29: {                                              // IBEQ, IBNE
            const bool equal=branchVi(integer(it),at)==branchVi(integer(is),at);
            if(equal==(op==0x28))branchTo(at+1+imm11(w));
            return;
        }
        case 0x2C:case 0x2D:case 0x2E:case 0x2F: {                          // IBLTZ, IBGTZ, IBLEZ, IBGEZ
            const auto v=std::int16_t(branchVi(integer(is),at));
            if(op==0x2C?v<0:op==0x2D?v>0:op==0x2E?v<=0:v>=0)branchTo(at+1+imm11(w));
            return;
        }
        case 0x40:break;
        default:refuse("lower instruction",w,at);
    }
    const unsigned function=w&0x3F,id=(w>>6)&31;
    if(function<0x3C) {
        const std::uint32_t a=vi[integer(is)],b=vi[integer(it)];
        switch(function) {
            case 0x30:setVi(integer(id),std::uint16_t(a+b));return;         // IADD
            case 0x31:setVi(integer(id),std::uint16_t(a-b));return;         // ISUB
            case 0x32:setVi(integer(it),std::uint16_t(a+id-((id&16)<<1)));return; // IADDI it, is, imm5
            case 0x34:setVi(integer(id),std::uint16_t(a&b));return;         // IAND
            case 0x35:setVi(integer(id),std::uint16_t(a|b));return;         // IOR
            default:refuse("lower instruction",w,at);
        }
    }
    switch((id<<2)|(w&3)) {
        case 0x30:if(it)for(unsigned lane=0;lane<4;++lane)if(dest&(8u>>lane))vf[it][lane]=vf[is][lane];return; // MOVE
        case 0x31: {                                                        // MR32: x, y, z, w from y, z, w, x
            const auto from=vf[is];
            if(it)for(unsigned lane=0;lane<4;++lane)if(dest&(8u>>lane))vf[it][lane]=from[(lane+1)&3];
            return;
        }
        case 0x34:load(it,vi[integer(is)]);setVi(is,std::uint16_t(vi[is]+1));return;               // LQI ft, (is++)
        case 0x35:store(vi[integer(it)],vf[is]);setVi(it,std::uint16_t(vi[it]+1));return;          // SQI fs, (it++)
        case 0x36:setVi(integer(is),std::uint16_t(vi[is]-1));load(it,vi[is]);return;               // LQD ft, (--is)
        case 0x37:setVi(integer(it),std::uint16_t(vi[it]-1));store(vi[it],vf[is]);return;          // SQD fs, (--it)
        case 0x38:lowerDiv(w);return;                                       // DIV (vu1.cpp)
        case 0x3B:if(q_pending_) {q=q_next_;q_pending_=false;cycle_=std::max(cycle_,q_ready_);}return; // WAITQ
        case 0x3E: {                                                        // ILWR it, (is): the dest field
            if(dest!=8 && dest!=4 && dest!=2 && dest!=1)refuse("ILWR field mask in",w,at);
            const unsigned lane=dest==8?0:dest==4?1:dest==2?2:3;
            setVi(integer(it),std::uint16_t(word(cell(vi[integer(is)])+lane*4)));return;
        }
        case 0x3F: {const std::uint32_t v=vi[integer(it)];store(vi[integer(is)],{v,v,v,v});return;} // ISWR
        case 0x68:setVi(integer(it),std::uint16_t(top));return;             // XTOP
        case 0x69:setVi(integer(it),std::uint16_t(itop));return;            // XITOP
        case 0x6C:lowerKick(w,at,kick);return;                              // XGKICK is (vu1.cpp)
        default:refuse("lower instruction",w,at);
    }
}
