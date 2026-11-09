#include "ee_runtime.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

// VU0 macro-mode instructions that address VU0 data memory (4 KB, 256 quadwords), executed natively
// from the instruction word (the generator's own output for them is replaced in prepare_runtime.py):
//   VLQI.dest ft,(is++)   VSQI.dest fs,(it++)   VLQD.dest ft,(--is)   VSQD.dest fs,(--it)
// COP2 fields: dest bits 24-21 (x, y, z, w), bits 20-16 and 15-11 the two registers, low bits the form.

void EeRuntime::vu0MemoryInstruction(R5900Context* context,std::uint32_t instruction) {
    const unsigned form=instruction&0x7FF,upper=(instruction>>16)&31,lower=(instruction>>11)&31;
    if(instruction>>25!=0x25 || form<0x37C || form>0x37F) {
        char text[96];std::snprintf(text,sizeof text,"VU0 memory instruction 0x%08X is not modelled (pc 0x%X)",instruction,context->pc);
        throw std::runtime_error(text);
    }
    const bool store=form&1,decrement=form&2;
    // Loads name the float register in the upper field and the integer register in the lower; stores the reverse.
    const unsigned vf=store?lower:upper,vi=store?upper:lower;
    if(vi>15)throw std::runtime_error("VU0 memory instruction with a control register as address");
    auto& address=context->vi[vi];
    if(decrement)--address;
    std::uint8_t* cell=vu0_memory_.data()+(address&0xFF)*16;
    float memory[4],value[4];std::memcpy(memory,cell,16);_mm_storeu_ps(value,context->vu0_vf[vf]);
    for(unsigned lane=0;lane<4;++lane) {
        if(!(instruction&(1u<<(24-lane))))continue; // dest: x is bit 24.
        if(store)memory[lane]=value[lane]; else value[lane]=memory[lane];
    }
    if(store)std::memcpy(cell,memory,16); else if(vf)context->vu0_vf[vf]=_mm_loadu_ps(value); // VF0 is constant.
    if(!decrement)++address;
    ++vu0_memory_instructions;
}

// The random number instructions, executed natively from the instruction word (the generator's output
// for them neither follows the register's sequence nor writes VRNEXT's destination):
//   VRINIT R,fs[fsf]   R = the low 23 bits of the field
//   VRXOR R,fs[fsf]    R ^= the low 23 bits of the field
//   VRNEXT.dest ft,R   R advances one step, then ft's dest fields = R
//   VRGET.dest ft,R    ft's dest fields = R
// R is a 23-bit shift register (new bit 0 = bit 4 xor bit 22) read with the exponent of 1.0, so a
// value is a float in [1, 2). An all-zero register stays zero: the game seeds it before use.
void EeRuntime::vu0RandomInstruction(R5900Context* context,std::uint32_t instruction) {
    const unsigned form=instruction&0x7FF,ft=(instruction>>16)&31,fs=(instruction>>11)&31,field=(instruction>>21)&3;
    if(instruction>>25!=0x25 || form<0x43C || form>0x43F) {
        char text[96];std::snprintf(text,sizeof text,"VU0 random instruction 0x%08X is not modelled (pc 0x%X)",instruction,context->pc);
        throw std::runtime_error(text);
    }
    ++vu0_random_instructions;
    if(form&2) { // VRINIT (0x43E), VRXOR (0x43F).
        std::uint32_t lanes[4];std::memcpy(lanes,&context->vu0_vf[fs],16);
        const std::uint32_t bits=lanes[field]&0x7FFFFFu;
        vu0_random_=0x3F800000u|(form&1?(vu0_random_^bits)&0x7FFFFFu:bits);
        return;
    }
    if(form==0x43C) { // VRNEXT.
        const std::uint32_t bit=((vu0_random_>>4)^(vu0_random_>>22))&1u;
        vu0_random_=0x3F800000u|(((vu0_random_<<1)|bit)&0x7FFFFFu);
    }
    if(!ft)return; // VF0 is constant.
    std::uint32_t lanes[4];std::memcpy(lanes,&context->vu0_vf[ft],16);
    for(unsigned lane=0;lane<4;++lane)if(instruction&(1u<<(24-lane)))lanes[lane]=vu0_random_;
    std::memcpy(&context->vu0_vf[ft],lanes,16);
}

// Once frames are presented no VU microprogram has been started by anything the runtime models (VIF
// microprogram calls are refused), so both units are idle whoever asks: main, a worker thread or an
// interrupt handler. Before the first frame only the captured startup sequences are answered.
bool EeRuntime::cop2Condition(R5900Context*) {
    if(gs_display_flips>=1)return false;
    throw UnsupportedVuStatus();
}

std::uint32_t EeRuntime::readVuStatus(R5900Context* context) {
    if(clear_repeat_gif && displayResetStep(context,6,0x31062C,624))return clear_repeat_gif->readVuIdle();
    if(gs_display_flips>=1)return 0;
    if(gif_dma.syncSupplied() && context->pc==0x31062C && context->r[29].low==initial_sp-624)
        return gif_dma.readVuIdle();
    throw UnsupportedVuStatus();
}

// CFC2/CTC2 of the VU0 status flag register. The flags are maintained only inside functions that clear
// and then read them (the game's clip tests): there every flag-setting instruction between the two is
// routed through vu0UpdateFlags. CTC2 writes the sticky bits (6-11); the others follow the operations.
std::uint32_t EeRuntime::readVuStatusFlag(R5900Context*) {return vu0_status_flags_;}
void EeRuntime::writeVuStatusFlag(R5900Context*,std::uint32_t value) {vu0_status_flags_=std::uint16_t((vu0_status_flags_&0x3Fu)|(value&0xFC0u));}

// VADD/VSUB with a full or broadcast operand (COP2 special1 functions 0x28, 0x2C, 0x00-0x07): the zero
// and sign flags over the destination fields, and their sticky copies. Overflow and underflow are not
// modelled (host IEEE arithmetic); other instructions are refused by name.
void EeRuntime::vu0UpdateFlags(R5900Context* context,std::uint32_t instruction) {
    const unsigned function=instruction&0x3F,ft=(instruction>>16)&31,fs=(instruction>>11)&31;
    if(instruction>>25!=0x25 || (function!=0x28 && function!=0x2C && function>7)) {
        char text[96];std::snprintf(text,sizeof text,"VU0 flag effects of instruction 0x%08X are not modelled (pc 0x%X)",instruction,context->pc);
        throw std::runtime_error(text);
    }
    const bool subtract=function==0x2C || (function>=4 && function<=7);
    float left[4],right[4];_mm_storeu_ps(left,context->vu0_vf[fs]);_mm_storeu_ps(right,context->vu0_vf[ft]);
    bool zero=false,sign=false;
    for(unsigned lane=0;lane<4;++lane) {
        if(!(instruction&(1u<<(24-lane))))continue;
        const float operand=function<8?right[function&3]:right[lane]; // Broadcast forms use one field of ft.
        const float result=subtract?left[lane]-operand:left[lane]+operand;
        zero|=result==0.0f;sign|=result<0.0f;
    }
    vu0_status_flags_=std::uint16_t((vu0_status_flags_&0xFC0u)|(zero?0x41u:0u)|(sign?0x82u:0u));
}
