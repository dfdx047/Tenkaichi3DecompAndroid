#include "ee/ee_runtime.h"
#include "ee/native_functions.h"
#include "ee/ps2_runtime_macros.h"
#include <cstring>
#include <iostream>
#include <vector>

// The native memcpy (ee/native_functions.cpp) against a model of the guest function, instruction by
// instruction, standing in for the generated code: the same memory, registers (all 128 bits), pc and
// checkpoints, and the model runs whenever the native fast path does not apply.
namespace {
void check(bool value,const char* what) {if(!value)throw std::runtime_error(what);}
unsigned model_runs=0;

std::uint64_t s32(std::uint64_t value) {return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(value))));}

// The C library's memcpy as the guest runs it (delay slots in place); a taken back-edge is a checkpoint.
void modelMemcpy(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    auto& rt=*static_cast<EeRuntime*>(base);++model_runs;
    auto& r=ctx->r;
    const auto u32=[&](unsigned n){return static_cast<std::uint32_t>(r[n].low);};
    const auto quad=[&](std::uint32_t address){return rt.read128(rdram,address);};
    r[8].low=r[4].low;                                    // daddu t0,a0
    r[2].low=r[6].low<32;r[3].low=r[8].low;               // sltiu v0,a2,32; delay: daddu v1,t0
    if(r[2].low)goto bytes;                               // bnez v0
    r[2].low=(r[5].low|r[8].low)&0xF;                     // or; andi
    if(r[2].low) {r[6].low=s32(r[6].low-1);goto count_down;} // bnel (delay only when taken)
    r[7].low=r[8].low;                                    // daddu a3,t0
    for(;;) {
        r[3]=quad(u32(5));r[6].low=s32(r[6].low-32);r[5].low=s32(r[5].low+16);r[4].low=r[6].low<32;
        rt.write128(rdram,u32(7),r[3]);r[7].low=s32(r[7].low+16);
        r[2]=quad(u32(5));r[5].low=s32(r[5].low+16);rt.write128(rdram,u32(7),r[2]);
        const bool again=r[4].low==0;r[7].low=s32(r[7].low+16); // beqz a0; delay: addiu a3
        if(!again)break;
        rt.eeCheckpointDue();
    }
    r[2].low=r[6].low<8;r[3].low=r[7].low;                // sltiu v0,a2,8; delay: daddu v1,a3
    if(!r[2].low) {
        for(;;) {
            r[3].low=rt.read64(rdram,u32(5));r[6].low=s32(r[6].low-8);r[5].low=s32(r[5].low+8);r[2].low=r[6].low<8;
            rt.write64(rdram,u32(7),r[3].low);
            const bool again=r[2].low==0;r[7].low=s32(r[7].low+8); // beqz v0; delay: addiu a3
            if(!again)break;
            rt.eeCheckpointDue();
        }
        r[3].low=r[7].low;                                // daddu v1,a3
    }
bytes:
    r[6].low=s32(r[6].low-1);
count_down:
    r[2].low=s32(0xFFFFFFFF);r[4].low=r[2].low;           // addiu v0,-1; beq a2,v0 with delay daddu a0,v0
    if(r[6].low!=r[2].low)
        for(;;) {
            r[2].low=rt.read8(rdram,u32(5));r[6].low=s32(r[6].low-1);r[5].low=s32(r[5].low+1);
            rt.write8(rdram,u32(3),static_cast<std::uint8_t>(r[2].low));r[3].low=s32(r[3].low+1);
            if(r[6].low==r[4].low)break;                  // bne a2,a0
            rt.eeCheckpointDue();
        }
    ctx->pc=u32(31);r[2].low=r[8].low;                    // jr ra; delay: daddu v0,t0
}

struct Run {EeRuntime rt;R5900Context ctx{};bool threw=false;};

void start(Run& run,bool native,std::uint32_t destination,std::uint32_t source,std::uint32_t count,unsigned limit) {
    std::vector<std::uint8_t> image(EeRuntime::ram_size);
    for(std::uint32_t n=0;n<0x10000;++n)image[n]=static_cast<std::uint8_t>(n*7+3);
    run.rt.loadMemory(image);run.rt.expected_return=0xFFFFFFFC;run.rt.checkpoint_limit=limit;
    run.rt.registerFunction(0x32512C,modelMemcpy);
    if(native)check(installNativeMemcpyForTest(run.rt)==0x32512C,"install");
    for(unsigned n=0;n<32;++n)run.ctx.r[n]={0x1111111100000000ull+n,0x2222222200000000ull+n};
    run.ctx.r[4].low=destination;run.ctx.r[5].low=source;run.ctx.r[6].low=count;run.ctx.r[31].low=run.rt.expected_return;
    try {run.rt.callFunction(0x32512C,run.ctx);} catch(const std::runtime_error&) {run.threw=true;}
}

void same(std::uint32_t destination,std::uint32_t source,std::uint32_t count,unsigned limit=0,bool expect_native=true) {
    Run native,model;
    const auto runs=model_runs;
    start(native,true,destination,source,count,limit);
    const bool native_ran=model_runs==runs;
    start(model,false,destination,source,count,limit);
    check(native_ran==expect_native,"native fast path taken or refused as expected");
    check(native.threw==model.threw,"same stop");
    check(native.threw || native.ctx.pc==model.ctx.pc,"same pc");
    for(unsigned n=0;n<32;++n)check(native.threw || (native.ctx.r[n].low==model.ctx.r[n].low && native.ctx.r[n].high==model.ctx.r[n].high),"same registers");
    check(std::equal(native.rt.memory().begin(),native.rt.memory().begin()+0x10000,model.rt.memory().begin()),"same memory");
}
}

int main() {
    try {
        // Bytes only (short, unaligned), the aligned rounds with and without doublewords and bytes, overlaps
        // both ways, aligned and not.
        same(0x4000,0x2000,0);same(0x4000,0x2000,5);same(0x4000,0x2000,31);same(0x4000,0x2000,32);
        same(0x4000,0x2000,77);same(0x4000,0x2000,4096+13);same(0x4003,0x2001,100);same(0x4000,0x2000,64);same(0x4000,0x2000,40);
        same(0x2010,0x2000,100);same(0x2000,0x2010,100);same(0x2001,0x2000,50);same(0x2000,0x2003,50);
        // Its checkpoints: 77 bytes aligned are 2 rounds, 1 doubleword and 5 bytes, 1 + 0 + 4 = 5 back-edges.
        same(0x4000,0x2000,77,5);           // Exactly the budget: native.
        same(0x4000,0x2000,77,4,false);     // One short: the model runs and stops at the same checkpoint.
        // Outside main RAM (the scratchpad): the model.
        same(0x70000000,0x2000,64,0,false);
        std::cout<<"Native function tests passed.\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Native function test failed: "<<error.what()<<'\n';
        return 1;
    }
}
