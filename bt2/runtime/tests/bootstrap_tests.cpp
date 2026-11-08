#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include "ee/state_hash.h"
#include "probe/game_starter.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <algorithm>
#include <source_location>

namespace {
void require(bool value,const std::source_location where=std::source_location::current()) { if (!value) {std::cerr<<"Failed assertion line "<<where.line()<<"\n";throw std::runtime_error("Bootstrap helper test failed");} }
template<class Action> void reject(Action action,const std::source_location where=std::source_location::current()) {
    bool caught = false;
    try { action(); } catch (const std::runtime_error&) { caught = true; }
    require(caught,where);
}
void return_leaf(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* guest) {
    auto* runtime=static_cast<EeRuntime*>(guest);
    runtime->dispatchGuestBranch(ram, ctx, GPR_U32(ctx, 31), ctx->pc, 0,
                                EeRuntime::GuestBranchKind::Return, "synthetic return");
}
// Worker entries that end with a thread call: ExitDeleteThread, or SleepThread.
void exiting_worker(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* guest) {
    ctx->r[3].low=0x24;static_cast<EeRuntime*>(guest)->handleSyscall(ram,ctx,0);
}
void sleeping_worker(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* guest) {
    ctx->r[3].low=0x32;static_cast<EeRuntime*>(guest)->handleSyscall(ram,ctx,0);
    throw std::runtime_error("A sleeping test worker resumed");
}
void nested_call(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* guest) {
    auto* runtime=static_cast<EeRuntime*>(guest);
    const auto saved_ra = GPR_U32(ctx, 31);
    SET_GPR_U32(ctx, 31, 0x204);
    runtime->dispatchGuestBranch(ram, ctx, 0x300, 0x200, 0x204,
                                EeRuntime::GuestBranchKind::IndirectCall, "synthetic indirect call");
    SET_GPR_U32(ctx, 31, saved_ra);
    return_leaf(ram, ctx, runtime);
}
void bad_return(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* guest) {
    auto* runtime=static_cast<EeRuntime*>(guest);
    runtime->dispatchGuestBranch(ram, ctx, 0xBAD, ctx->pc, 0,
                                EeRuntime::GuestBranchKind::Return, "synthetic invalid return");
}
void tail_call(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* guest) {
    auto* runtime=static_cast<EeRuntime*>(guest);
    runtime->dispatchGuestBranch(ram, ctx, 0x300, ctx->pc, 0,
                                EeRuntime::GuestBranchKind::TailJump, "synthetic tail");
}

void clobber_rpc_callback(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* guest) {
    auto* runtime=static_cast<EeRuntime*>(guest);
    const auto return_address = ctx->r[31].low;
    const auto sp = ctx->r[29].low;
    ctx->r[3].low = 0xFFFFFFBD;
    ctx->r[4].low = 3;
    runtime->handleSyscall(ram, ctx, 0);
    runtime->write32(ram, 0x3A87A8, 0);
    runtime->write32(ram, 0x3A87CC, 0x3F570);
    runtime->write32(ram, 0x3A87BC, 0x3FB20);
    runtime->write32(ram, 0x3A97D0, 4);
    runtime->write32(ram, 0x3A97D8, 0);
    for (auto& reg : ctx->r) reg = {UINT64_MAX, UINT64_MAX};
    for (auto& value : ctx->f) value = 123.0f;
    ctx->hi = ctx->lo = ctx->hi1 = ctx->lo1 = UINT64_MAX;
    ctx->cop0_status = ctx->fcr31 = UINT32_MAX;
    std::fill_n(ram + sp - 64, 64, 0xCC);
    ctx->r[29].low = sp;
    ctx->r[31].low = return_address;
    return_leaf(ram, ctx, runtime);
}

unsigned vblank_fixture_calls=0;
void vblank_fixture(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* guest) {
    auto* runtime=static_cast<EeRuntime*>(guest);
    // Synthetic handler: checks the interrupt ABI, then wakes ID 5 through interrupt syscalls.
    require(GPR_U32(ctx,4)==2 && GPR_U32(ctx,5)==0x1234 && GPR_U32(ctx,28)==0x38BDF0 && GPR_U32(ctx,29)==0x81FC0
        && !(ctx->cop0_status&0x10001) && runtime->in_interrupt_);
    auto call=[&](unsigned selector,unsigned a0,unsigned a1){
        auto c=*ctx;c.r[3].low=static_cast<std::uint32_t>(selector);c.r[4].low=a0;c.r[5].low=a1;
        runtime->handleSyscall(ram,&c,0);return static_cast<unsigned>(c.r[2].low);};
    require(call(0xFFFFFFD1u,0,0)==1 && call(0xFFFFFFCFu,5,0x81F00)==4 && runtime->read32(ram,0x81F00)==4);
    require(call(0xFFFFFFCCu,5,0)==5 && call(0xFFFFFFCFu,5,0x81F00)==2 && call(0xFFFFFFCCu,5,0)==5);
    ++vblank_fixture_calls;ctx->r[2].low=0;
    runtime->dispatchGuestBranch(ram, ctx, GPR_U32(ctx, 31), ctx->pc, 0, EeRuntime::GuestBranchKind::Return, "synthetic handler return");
}
void vblank_bad_fixture(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* guest) {
    auto* runtime=static_cast<EeRuntime*>(guest);
    auto c=*ctx;c.r[3].low=0x29;c.r[4].low=0;c.r[5].low=1;runtime->handleSyscall(ram,&c,0); // Thread-context call.
}
}
int main() {
    {
        // Trapping 32-bit add: overflow only when equal-signed operands give the other sign.
        std::uint32_t sum=0;bool overflow=true;
        ADD32_OV(5,0xFFFFFFFCu,sum,overflow);require(sum==1 && !overflow);
        ADD32_OV(0x7FFFFFFFu,1,sum,overflow);require(sum==0x80000000u && overflow);
        ADD32_OV(0x80000000u,0xFFFFFFFFu,sum,overflow);require(sum==0x7FFFFFFFu && overflow);
        EeRuntime traps;R5900Context trap_ctx{};
        reject([&]{traps.SignalException(&trap_ctx,EXCEPTION_INTEGER_OVERFLOW);});
        // Multiply-accumulate helpers: HI:LO low words as one value, 32-bit sign extension.
        require(Ps2HiLoToU64(0xFFFFFFFF00000001ull,0xAAAAAAAA80000002ull)==0x0000000180000002ull);
        require(Ps2SignExt32ToU64(0x80000000u)==0xFFFFFFFF80000000ull && Ps2SignExt32ToU64(7)==7);
        // Halfword MMI: wrapping add, signed min/max, and byte packing (rt low, rs high).
        const ProbeGpr x{0x7FFF000100FF8000ull,0x0001000200030004ull},y{0x0001FFFF00010001ull,0x00FF01FE02FD03FCull};
        require(PS2_PADDH(x,y).low==0x8000000001008001ull && PS2_PADDH(x,y).high==0x0100020003000400ull);
        require(PS2_PMAXH(x,y).low==0x7FFF000100FF0001ull && PS2_PMINH(x,y).low==0x0001FFFF00018000ull);
        // VU0 macro mode: VF0 is (0, 0, 0, 1); a masked write changes only the selected lanes.
        R5900Context vu{};float lanes[4];
        _mm_storeu_ps(lanes,vu.vu0_vf[0]);require(lanes[0]==0 && lanes[1]==0 && lanes[2]==0 && lanes[3]==1);
        _mm_storeu_ps(lanes,vu.vu0_vf[5]);require(lanes[0]==0 && lanes[3]==0);
        const __m128 a=_mm_set_ps(4,3,2,1),b=_mm_set_ps(40,30,20,10);
        _mm_storeu_ps(lanes,PS2_VBLEND(a,PS2_VADD(a,b),_mm_castsi128_ps(_mm_set_epi32(-1,0,0,0))));
        require(lanes[0]==1 && lanes[1]==2 && lanes[2]==3 && lanes[3]==44);
        _mm_storeu_ps(lanes,PS2_VSUB(b,a));require(lanes[0]==9 && lanes[3]==36);
        _mm_storeu_ps(lanes,PS2_VMUL(a,b));require(lanes[1]==40 && lanes[2]==90);
        {
            // VU0 memory instructions from their words: VSQI.xyzw vf16,(vi15++) then VLQD.xyzw vf19,(--vi15)
            // push and pop a register; a partial dest mask moves only its lanes; other words are refused.
            EeRuntime vum;R5900Context c{};float out[4];
            c.vu0_vf[16]=_mm_set_ps(4,3,2,1);c.vi[15]=5;
            vum.vu0MemoryInstruction(&c,0x4BEF837D);require(c.vi[15]==6);
            vum.vu0MemoryInstruction(&c,0x4BF37B7E);_mm_storeu_ps(out,c.vu0_vf[19]);
            require(c.vi[15]==5 && out[0]==1 && out[1]==2 && out[2]==3 && out[3]==4 && vum.vu0_memory_instructions==2);
            c.vu0_vf[19]=_mm_setzero_ps();c.vi[15]=6;
            vum.vu0MemoryInstruction(&c,0x4B137B7E); // VLQD.x: only x is loaded.
            _mm_storeu_ps(out,c.vu0_vf[19]);require(out[0]==1 && out[1]==0 && out[3]==0);
            reject([&]{vum.vu0MemoryInstruction(&c,0x4BEF8368);});
            {
                // The random number register: VRINIT R,vf5x takes the field's low 23 bits; VRNEXT.x vf5,R
                // shifts in bit 4 xor bit 22 and writes the value, a float in [1, 2), to x only; VRGET
                // reads without a step; VRXOR mixes a field in; other words are refused.
                EeRuntime random;R5900Context r{};std::uint32_t bits[4];
                r.vu0_vf[5]=_mm_castsi128_ps(_mm_set_epi32(7,7,7,int(0xC0400010u)));
                random.vu0RandomInstruction(&r,0x4A002C3E);require(random.vu0_random_==0x3FC00010u);
                random.vu0RandomInstruction(&r,0x4B05043C);std::memcpy(bits,&r.vu0_vf[5],16);
                require(random.vu0_random_==0x3F800020u && bits[0]==0x3F800020u && bits[1]==7 && bits[3]==7); // Bits 4 and 22 both set: 0 shifted in.
                random.vu0RandomInstruction(&r,0x4B05043C);require(random.vu0_random_==0x3F800040u);
                random.vu0RandomInstruction(&r,0x4B05043C);require(random.vu0_random_==0x3F800080u);
                random.vu0RandomInstruction(&r,0x4B05043C);std::memcpy(bits,&r.vu0_vf[5],16);
                require(bits[0]==0x3F800100u);
                r.vu0_vf[5]=_mm_castsi128_ps(_mm_set_epi32(0,0,0,0x10));
                random.vu0RandomInstruction(&r,0x4A002C3E);random.vu0RandomInstruction(&r,0x4B05043C);
                require(random.vu0_random_==0x3F800021u);                                              // Bit 4 alone: 1 shifted in.
                random.vu0RandomInstruction(&r,0x4BE6043D);std::memcpy(bits,&r.vu0_vf[6],16);          // VRGET.xyzw vf6,R.
                require(bits[0]==0x3F800021u && bits[3]==0x3F800021u && random.vu0_random_==0x3F800021u);
                r.vu0_vf[5]=_mm_castsi128_ps(_mm_set_epi32(0,0,0,int(0xFF800010u)));                  // VRNEXT had written vf5x.
                random.vu0RandomInstruction(&r,0x4A002C3F);require(random.vu0_random_==0x3F800031u);   // VRXOR R,vf5x: the low 23 bits only.
                require(random.vu0_random_instructions==9);
                reject([&]{random.vu0RandomInstruction(&r,0x4B05043B);});
            }
            // Status flags as the game's clip test uses them: clear, VSUB.xyw vf0,vf5,vf11 and
            // VSUB.xy vf0,vf12,vf5, then read the sticky zero/sign bits (0xC0).
            c.vu0_vf[5]=_mm_set_ps(1,5,200,100);c.vu0_vf[11]=_mm_setzero_ps();c.vu0_vf[12]=_mm_set_ps(0,0,4096,4096);
            vum.writeVuStatusFlag(&c,0);vum.vu0UpdateFlags(&c,0x4BAB282C);vum.vu0UpdateFlags(&c,0x4B85602C);
            require((vum.readVuStatusFlag(&c)&0xC0)==0);                     // Inside: nothing zero or negative.
            c.vu0_vf[5]=_mm_set_ps(1,5,200,5000);                             // x beyond 4096: 4096 - x is negative.
            vum.writeVuStatusFlag(&c,0);vum.vu0UpdateFlags(&c,0x4BAB282C);vum.vu0UpdateFlags(&c,0x4B85602C);
            require((vum.readVuStatusFlag(&c)&0xC0)==0x80);
            c.vu0_vf[5]=_mm_set_ps(0,5,200,100);                              // w zero: sticky zero, kept by a later operation.
            vum.writeVuStatusFlag(&c,0);vum.vu0UpdateFlags(&c,0x4BAB282C);vum.vu0UpdateFlags(&c,0x4B85602C);
            require((vum.readVuStatusFlag(&c)&0xC0)==0x40);
            reject([&]{vum.vu0UpdateFlags(&c,0x4BE4294A);});                  // VMUL: not modelled.
        }
        require(PS2_PCGTH(x,y).low==0xFFFFFFFFFFFF0000ull && PS2_PCGTH(x,y).high==0);
        require(PS2_PPACB(x,y).low==0xFFFEFDFC01FF0101ull && PS2_PPACB(x,y).high==0x01020304FF01FF00ull);
        // Byte interleave (rt's byte first) and the 256-bit funnel shift rs:rt >> SA.
        const ProbeGpr lo{0x0706050403020100ull,0x0F0E0D0C0B0A0908ull},hi{0x1716151413121110ull,0x1F1E1D1C1B1A1918ull};
        require(PS2_PEXTLB(hi,lo).low==0x1303120211011000ull && PS2_PEXTLB(hi,lo).high==0x1707160615051404ull);
        require(PS2_PEXTUB(hi,lo).low==0x1B0B1A0A19091808ull && PS2_PEXTUB(hi,lo).high==0x1F0F1E0E1D0D1C0Cull);
        require(PS2_PEXTLW(hi,lo).low==0x1312111003020100ull && PS2_PEXTLW(hi,lo).high==0x1716151407060504ull);
        require(PS2_PEXTUW(hi,lo).low==0x1B1A19180B0A0908ull && PS2_PEXTUW(hi,lo).high==0x1F1E1D1C0F0E0D0Cull);
        require(PS2_QFSRV(hi,lo,0).low==lo.low && PS2_QFSRV(hi,lo,0).high==lo.high);
        require(PS2_QFSRV(hi,lo,24).low==0x0A09080706050403ull && PS2_QFSRV(hi,lo,24).high==0x1211100F0E0D0C0Bull);
        require(PS2_QFSRV(hi,lo,120).low==0x161514131211100Full && PS2_QFSRV(hi,lo,120).high==0x1E1D1C1B1A191817ull);
    }
    {
        // Overlay functions dispatch only while RAM holds the exact bytes they were generated from.
        EeRuntime overlay;overlay.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        auto* ram=overlay.memory().data();R5900Context ctx{};
        const std::uint8_t code[8]={0x08,0x00,0xE0,0x03,0x00,0x00,0x00,0x00}; // Synthetic JR RA / NOP.
        reject([&]{overlay.registerOverlayFunction(0x400000,return_leaf,std::span<const std::uint8_t>(code,6));});
        reject([&]{overlay.registerOverlayFunction(0x400000,return_leaf,std::span<const std::uint8_t>());});
        overlay.registerOverlayFunction(0x400000,return_leaf,code);
        reject([&]{overlay.registerOverlayFunction(0x400000,return_leaf,code);});
        ctx.r[31].low=0x204;
        reject([&]{overlay.dispatchGuestBranch(ram,&ctx,0x400000,0x200,0x204,EeRuntime::GuestBranchKind::DirectCall,"not loaded");});
        require(!overlay.overlay_dispatch_validations);
        std::copy(std::begin(code),std::end(code),ram+0x400000);
        overlay.dispatchGuestBranch(ram,&ctx,0x400000,0x200,0x204,EeRuntime::GuestBranchKind::DirectCall,"loaded");
        require(overlay.overlay_dispatch_validations==1 && ctx.pc==0x204);
        ram[0x400004]=1; // Any change to the bound body is refused.
        reject([&]{overlay.dispatchGuestBranch(ram,&ctx,0x400000,0x200,0x204,EeRuntime::GuestBranchKind::DirectCall,"changed");});
        require(overlay.overlay_dispatch_validations==1);
    }
    {
        // Logical Timer 1, H-BLANK clock: 525 half-lines per field, one count per HBLANK while CUE.
        LogicalTimer1 t;reject([&]{t.writeMode(0x83);});reject([&]{t.configure(1);});
        t.configure(3);t.field();require(t.halflines()==525 && t.count()==0); // Stopped: no counting.
        t.writeMode(0x83);require(t.counting());
        for(unsigned n=0;n<10;++n){const auto target=t.nextLine();require(!t.crossesField(target));t.lineTo(target);}
        require(t.count()==10 && t.halflines()==545);
        t.field();require(t.halflines()==1050 && t.count()==10+(1050/2-545/2)); // 253 more HBLANKs.
        const auto before=t.count();
        t.field();require(t.count()==before+(1575/2-1050/2));
        reject([&]{t.writeCount(0);});reject([&]{t.writeMode(0x82);});
        t.writeMode(3);require(!t.counting());t.writeCount(0);require(t.count()==0);
        // A line that reaches the boundary: the field is delivered first, then the line completes.
        LogicalTimer1 u;u.configure(3);u.writeMode(0x83);
        while(!u.crossesField(u.nextLine()))u.lineTo(u.nextLine());
        const auto target=u.nextLine();u.field();u.lineTo(target);require(u.halflines()==target && u.count()==target/2);
        // COUNT wraps at 16 bits without a flag (MODE 3 enables no overflow interrupt).
        LogicalTimer1 w;w.configure(3);w.writeMode(0x83);
        for(unsigned f=0;f<250;++f)w.field();
        require(w.count()==(250*525/2)%0x10000 && w.mode()==0x83);
        // Frame-phase BUSCLK/256: 9,609.6 ticks per field, exact over five fields; H-BLANK modes unchanged.
        LogicalTimer1 b;b.configure(3);b.reconfigure(0x82);b.setCount(0);
        b.field();require(b.count()==9609);for(unsigned f=0;f<4;++f)b.field();require(b.count()==48048);
        reject([&]{b.reconfigure(0x182);});reject([&]{b.reconfigure(0x81);});
        b.reconfigure(0xC83);require(b.mode()==0x83);b.reconfigure(0x82); // Flag acknowledgement bits are not mode bits.
    }
    {
        // Logical Timer 2: one NTSC field = 9,609.6 BUSCLK/256 ticks; W1C flags; compare and wrap latch.
        LogicalTimer2 t;reject([&]{t.advanceField();});reject([&]{t.start(0x80,0,0);});
        t.start(0x382,0,0xFFFF);
        for(unsigned n=0;n<5;++n)t.advanceField();
        require(t.count()==48048 && t.mode()==0x382 && !t.interruptPending() && !t.wraps);
        for(unsigned n=0;n<2;++n)t.advanceField();
        require(t.wraps==1 && t.count()==(67267u&0xFFFF) && (t.mode()&0xC00)==0xC00 && t.interruptPending());
        reject([&]{t.writeMode(0x383);});
        t.writeMode(0x382|0x800);require((t.mode()&0xC00)==0x400 && t.interruptPending());
        t.writeComp(0);t.writeMode(0x382|0x400);require(!t.interruptPending() && t.comp()==0);
        for(unsigned n=0;n<6;++n)t.advanceField();
        require(t.wraps==1 && !t.interruptPending());t.advanceField();
        require(t.wraps==2 && (t.mode()&0xC00)==0xC00); // COMP 0 matches at the wrap itself.
        reject([&]{t.writeComp(0x10000);});
    }
    {
        EeRuntime cache; cache.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size)); R5900Context ctx;
        ctx.r[4].low=0x1000;ctx.r[5].low=0x1040;
        ctx.pc=0x32F4CC;
        reject([&]{cache.readCacheTagLo(&ctx);});
        for (unsigned slot=0;slot<128;++slot) {
            const unsigned way=slot&1u,address=(slot/2)*64+way;
            ctx.pc=way?0x32F500:0x32F4C4;
            if (!slot) {
                reject([&]{cache.indexedDataCache(cache.memory().data(),&ctx,0x10,address+64);});
                ctx.cop0_status=0x10000;
                reject([&]{cache.indexedDataCache(cache.memory().data(),&ctx,0x10,address);});
                ctx.cop0_status=0;
                require(cache.indexed_cache_operations==0);
            }
            cache.indexedDataCache(cache.memory().data(),&ctx,0x10,address);
            reject([&]{cache.indexedDataCache(cache.memory().data(),&ctx,0x10,address);});
            ctx.pc=way?0x32F508:0x32F4CC;
            require(cache.readCacheTagLo(&ctx)==0);
            reject([&]{cache.readCacheTagLo(&ctx);});
        }
        require(cache.indexed_cache_tag_reads==128 && cache.indexed_cache_operations==128);
        reject([&]{cache.indexedDataCache(cache.memory().data(),&ctx,0x14,0);});
    }

    try {
        {
            // Interpreter bridge: unmapped code runs through handlers, branches and jumps are decoded, calls
            // and code reached by running on go to compiled code, and what ran is counted.
            static const EeInterpreterHandler handlers[]={
                {0x00000000u,+[](std::uint8_t*,R5900Context*,PS2Runtime*,std::uint32_t){},0},                       // NOP
                {0x24420001u,+[](std::uint8_t*,R5900Context* c,PS2Runtime*,std::uint32_t){c->r[2].low+=1;},0},      // ADDIU v0,v0,1
                {0x42000018u,+[](std::uint8_t*,R5900Context* c,PS2Runtime*,std::uint32_t){c->pc=c->r[31].low;},0},  // ERET (to ra here)
            };
            static const EeInterpreterHandlerPart parts[]={{handlers,3}};
            EeRuntime rt;rt.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
            auto* ram=rt.memory().data();R5900Context c{};
            const auto code=[&](std::uint32_t at,std::initializer_list<std::uint32_t> words){for(auto w:words){std::memcpy(ram+at,&w,4);at+=4;}};
            const auto call=[&](std::uint32_t target){c.r[31].low=0x204;rt.dispatchGuestBranch(ram,&c,target,0x200,0x204,EeRuntime::GuestBranchKind::DirectCall,"test");};
            code(0x1000,{0x24420001,0x24420001,0x03E00008,0});
            reject([&]{call(0x1000);}); // Without handlers such a dispatch stops as before.
            rt.setInterpreterHandlers(parts);
            c.r[2].low=0;call(0x1000);
            require(c.r[2].low==2 && c.pc==0x204 && rt.interpreted_entries.at(0x1000).dispatches==1 && rt.interpreted_entries.at(0x1000).instructions==4 && rt.interpreted_instructions==4);
            // A loop: BNE v0,a0 back to 0x2000; each taken backward branch is a loop iteration.
            code(0x2000,{0x24420001,0x1444FFFE,0,0x03E00008,0});
            rt.current_thread_id=1;rt.main_checkpoints_since_field_=0;c.r[2].low=0;c.r[4].low=3;call(0x2000);
            require(c.r[2].low==3 && c.pc==0x204 && rt.main_checkpoints_since_field_==2);
            // A call into compiled code returns into the interpreted code.
            static unsigned compiled_calls;compiled_calls=0;
            // (The callee puts back the caller's return address, which real code keeps on its stack.)
            rt.registerFunction(0x300,+[](std::uint8_t* r,R5900Context* x,PS2Runtime* g){++compiled_calls;return_leaf(r,x,g);x->r[31].low=0x204;});
            code(0x3000,{0x0C0000C0,0,0x24420001,0x03E00008,0}); // JAL 0x300; NOP; ADDIU; JR RA; NOP
            c.r[2].low=0;call(0x3000);
            require(compiled_calls==1 && c.r[2].low==1 && c.pc==0x204);
            // Unmapped code that runs into a compiled function hands over to it.
            code(0x3FFC,{0x24420001});rt.registerFunction(0x4000,+[](std::uint8_t* r,R5900Context* x,PS2Runtime* g){++compiled_calls;return_leaf(r,x,g);});
            c.r[2].low=0;call(0x3FFC);
            require(compiled_calls==2 && c.r[2].low==1 && c.pc==0x204);
            // ERET ends the frame where its handler put the pc.
            code(0x5000,{0x42000018});call(0x5000);require(c.pc==0x204);
            // Explicit stops: a word without a handler, a branch in a delay slot.
            code(0x5100,{0x24420005});reject([&]{call(0x5100);});
            code(0x5200,{0x10000001,0x03E00008});reject([&]{call(0x5200);});
            // BT2_EE_INTERPRET=all: a generated function runs interpreted within its extent.
            static bool compiled_ran;compiled_ran=false;
            rt.registerFunction(0x6000,+[](std::uint8_t* r,R5900Context* x,PS2Runtime* g){compiled_ran=true;return_leaf(r,x,g);});
            rt.registerFunctionExtent(0x6000,0x600C);code(0x6000,{0x24420001,0x03E00008,0});
            rt.interpret_all=true;c.r[2].low=0;call(0x6000);
            require(!compiled_ran && c.r[2].low==1 && c.pc==0x204 && rt.interpret_all_instructions==3 && !rt.interpreted_entries.contains(0x6000));
        }
        {
            // State hash: equal state gives equal field hashes, one changed byte changes the field's hash;
            // the page hashes are written at the requested field.
            std::vector<std::uint8_t> ram(3*StateHashLog::page_size+100,7);
            const auto path=(std::filesystem::temp_directory_path()/"bt2-state-hash-test.txt").string();
            std::uint64_t first,second,changed;
            {
                StateHashLog log(path,3);
                const StateHashLog::Region regions[]={{"ee",ram}};
                first=log.field(2,0,regions);second=log.field(2,0,regions);
                ram[StateHashLog::page_size+5]^=1;changed=log.field(3,0,regions);
            }
            require(first==second && first!=changed);
            std::string region,page,digest;unsigned lines=0;
            {std::ifstream pages(path+".pages");while(pages>>region>>page>>digest)++lines;}
            require(lines==4 && region=="ee" && page=="3");
            require(StateHashLog::hashBytes(ram,0)!=StateHashLog::hashBytes(ram,1));
            std::filesystem::remove(path);std::filesystem::remove(path+".pages");
        }
        {
            // The game starter's switches and the replacement of a registered function (game_starter.cpp).
            const auto start=parseBattleStart("4:1,70,22");
            require(start.character[0]==4 && start.variant[0]==1 && start.character[1]==70 && start.variant[1]==0
                && start.stage==22 && start.music==9);
            require(parseBattleStart("0,0,9,3").music==3);
            reject([]{parseBattleStart("4,70");});
            reject([]{parseBattleStart("4:9,70,22");});
            reject([]{parseBattleStart("4,x,22");});
            reject([]{parseBattleStart("4,70,22,1,5");});
            reject([]{parseBattleStart("4,130,22");});
            require(parseBattleStart("129,0,22").character[0]==129);
            require(parseScreenStart("6")==6 && parseScreenStart("64")==64);
            {const auto e=parseEpisodeStart("3,1");require(e.episode==3 && e.part==1 && parseEpisodeStart("7").part==0);}
            reject([]{parseEpisodeStart("");});reject([]{parseEpisodeStart("2,x");});
            {const auto e=parseEpisodeStart("0,0,marker:14");require(e.marker==14 && !parseEpisodeStart("0,0").marker);}
            reject([]{parseEpisodeStart("0,0,battle");});reject([]{parseEpisodeStart("0,0,marker:26");});
            reject([]{parseEpisodeStart("0,0,marker:0");});reject([]{parseEpisodeStart("0,0,marker:14,1");});
            reject([]{parseScreenStart("0");});reject([]{parseScreenStart("65");});reject([]{parseScreenStart("x");});
            EeRuntime runtime;
            reject([&]{runtime.replaceFunction(0x300,return_leaf);});
            runtime.registerFunction(0x300,nested_call);
            require(runtime.replaceFunction(0x300,return_leaf)==nested_call);
        }
        {
            // Ultimate Battle Z's screen starter: Course Select gets the first course not cleared, and side 1's
            // fighters share one health bar while side 0's get the highest of side 1's strength.
            EeRuntime runtime;
            for(const std::uint32_t address:{0x3B7FA8u,0x101AB0u,0x101BF0u,0x101D78u,0x3FE450u,0x3CEF10u,0x3F5F00u})runtime.registerFunction(address,return_leaf);
            installScreenStarter(runtime,10);
            std::vector<std::uint8_t> ram(0x800000);R5900Context context{};
            const auto put=[&](std::uint32_t at,std::uint32_t value){std::memcpy(ram.data()+at,&value,4);};
            const auto get=[&](std::uint32_t at){std::uint32_t value;std::memcpy(&value,ram.data()+at,4);return value;};
            put(0x3876A4,0x500000);put(0x387F2C,0x600000);put(0x3876A0,0x700000);
            ram[0x6002C0]=2;ram[0x6002C1]=1;ram[0x6002C3]=1; // Courses 0, 1 and 3 cleared.
            runtime.replaceFunction(0x3F5F00,return_leaf)(ram.data(),&context,&runtime);
            require(get(0x500064)==2);
            const std::uint32_t side0=0x7000E0,side1=side0+0x1D0;
            put(side0,1);put(side1,2);
            put(side1+8+2*4,3);put(side1+8+0x58+2*4,5);put(side1+8+17*4,55000);put(side1+8+18*4,55000);put(side1+8+0x58+17*4,4000);
            runtime.replaceFunction(0x101BF0,return_leaf)(ram.data(),&context,&runtime);
            require(get(side1+8+17*4)==5000 && get(side1+8+18*4)==5000 && get(side1+8+0x58+17*4)==4000);
            require(get(side0+8+2*4)==5 && get(side0+8+17*4)==0);
        }
        {
            // The episode starter's confirmations: every 30th call of a waiting function sees cross
            // pressed in port 0's pressed buttons; the word is restored after the call.
            static unsigned calls=0,pressed_on=0;
            const auto waiting=+[](std::uint8_t* r,R5900Context*,PS2Runtime*){
                ++calls;std::uint32_t buttons;std::memcpy(&buttons,r+0x3B0C34,4);if(buttons&0x4000)pressed_on=calls;};
            EeRuntime runtime;
            for(const std::uint32_t address:{0x3B7FA8u,0x2A7400u,0x255580u,0x2581F0u,0x29C640u,0x2AC4A0u,0x2582E8u,0x2ACE58u,0x247638u,0x246860u,0x2A6F98u})runtime.registerFunction(address,waiting);
            static std::uint64_t control=0;
            runtime.registerFunction(0x101AB0,+[](std::uint8_t*,R5900Context* c,PS2Runtime*){control=c->r[6].low;});
            runtime.registerFunction(0x20C8A0,+[](std::uint8_t*,R5900Context* c,PS2Runtime*){c->r[2].low=0;});
            static unsigned town_calls=0,town_pressed_on=0;
            runtime.registerFunction(0x2A99B8,+[](std::uint8_t* r,R5900Context*,PS2Runtime*){
                ++town_calls;std::uint32_t buttons;std::memcpy(&buttons,r+0x3B0C34,4);if(buttons&0x4000 && !town_pressed_on)town_pressed_on=town_calls;});
            for(const std::uint32_t address:{0x20C950u,0x20C980u})
                runtime.registerFunction(address,+[](std::uint8_t*,R5900Context* c,PS2Runtime*){c->r[2].low=0;});
            installEpisodeStarter(runtime,parseEpisodeStart("0,0"));
            const auto prebattle=runtime.replaceFunction(0x255580,waiting);
            std::vector<std::uint8_t> ram(0x400000);R5900Context context{};
            for(int n=0;n<30;++n)prebattle(ram.data(),&context,&runtime);
            std::uint32_t after;std::memcpy(&after,ram.data()+0x3B0C34,4);
            require(calls==30 && pressed_on==30 && after==0);
            require(gameStarterReport().find("[2446720,1]")!=std::string::npos);
            // The player's side of a story battle goes to the CPU (the side setter's third argument).
            const auto side=runtime.replaceFunction(0x101AB0,return_leaf);
            context.r[6].low=0;side(ram.data(),&context,&runtime);
            require(control==1);
            // In a chase the enemy is reported caught once, on the map's 120th query.
            const auto caught=runtime.replaceFunction(0x20C8A0,return_leaf);
            context.r[31].low=runtime.expected_return;
            unsigned answered_on=0;
            for(unsigned n=1;n<=150;++n) {caught(ram.data(),&context,&runtime);if(context.r[2].low)answered_on=answered_on?~0u:n;}
            require(answered_on==120 && gameStarterReport().find("\"starter_chase_caught\":true")!=std::string::npos);
            // Towns: after 120 town frames the first character is reported in reach and cross is pressed
            // every 30th call; once its block has run (0x3B2664 nonzero, then zero) the next one is touched.
            const auto town=runtime.replaceFunction(0x2A99B8,return_leaf);
            const auto reach=runtime.replaceFunction(0x20C950,return_leaf);
            const auto touch=runtime.replaceFunction(0x20C980,return_leaf);
            for(int n=0;n<150;++n)town(ram.data(),&context,&runtime);
            reach(ram.data(),&context,&runtime);
            require(town_pressed_on==150 && context.r[2].low==1);
            const std::uint32_t running=1;std::memcpy(ram.data()+0x3B2664,&running,4);
            town(ram.data(),&context,&runtime);
            std::memset(ram.data()+0x3B2664,0,4);
            for(int n=0;n<121;++n)town(ram.data(),&context,&runtime);
            touch(ram.data(),&context,&runtime);
            require(context.r[2].low==2 && gameStarterReport().find("\"starter_town_blocks\":1")!=std::string::npos);
        }
        {
            // Interior entries: the function holding the label runs with the pc at the label, and the
            // function counts as entered. Overlay entries are checked against their owner's bytes.
            EeRuntime runtime;runtime.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
            auto* ram=runtime.memory().data();R5900Context ctx{};
            static std::uint32_t entered_at;entered_at=0;
            const auto record=[](std::uint8_t* r,R5900Context* c,PS2Runtime* g){entered_at=c->pc;return_leaf(r,c,g);};
            reject([&]{runtime.registerInteriorEntry(0x308,record,0x300);}); // No owner yet.
            runtime.registerFunction(0x300,record);
            reject([&]{runtime.registerInteriorEntry(0x300,record,0x300);});
            reject([&]{runtime.registerInteriorEntry(0x2FC,record,0x300);});
            reject([&]{runtime.registerInteriorEntry(0x30A,record,0x300);});
            reject([&]{runtime.registerInteriorEntry(0x308,return_leaf,0x300);}); // Another function than the owner's.
            runtime.registerInteriorEntry(0x308,record,0x300);
            reject([&]{runtime.registerInteriorEntry(0x308,record,0x300);});
            ctx.r[31].low=0x204;
            runtime.dispatchGuestBranch(ram,&ctx,0x308,0x200,0x204,EeRuntime::GuestBranchKind::DirectCall,"interior");
            require(entered_at==0x308 && ctx.pc==0x204 && runtime.interior_entry_dispatches==1);
            require(runtime.entered_functions.contains(0x300) && !runtime.entered_functions.contains(0x308));
            reject([&]{runtime.dispatchGuestBranch(ram,&ctx,0x30C,0x200,0x204,EeRuntime::GuestBranchKind::DirectCall,"no label");});

            const std::uint8_t code[8]={0x08,0x00,0xE0,0x03,0x00,0x00,0x00,0x00}; // Synthetic JR RA / NOP.
            runtime.registerOverlayFunction(0x400000,record,code);
            runtime.registerInteriorEntry(0x400004,record,0x400000);
            reject([&]{runtime.dispatchGuestBranch(ram,&ctx,0x400004,0x200,0x204,EeRuntime::GuestBranchKind::DirectCall,"not loaded");});
            std::copy(std::begin(code),std::end(code),ram+0x400000);
            runtime.dispatchGuestBranch(ram,&ctx,0x400004,0x200,0x204,EeRuntime::GuestBranchKind::DirectCall,"loaded");
            require(entered_at==0x400004 && runtime.overlay_dispatch_validations==1 && runtime.entered_functions.contains(0x400000));
        }
        {
            EeRuntime console;console.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
            console.sound_rpc_calls=1;console.native_module_loads=14;
            R5900Context cpu;cpu.pc=0x32F098;cpu.r[3].low=0x7C;cpu.r[4].low=16;cpu.r[5].low=cpu.r[29].low=0x1000;
            console.write32(console.memory().data(),0x1000,0x1100);std::memcpy(console.memory().data()+0x1100,"synthetic log",14);
            console.handleSyscall(console.memory().data(),&cpu,0);require(console.ee_console_messages.size()==1 && cpu.r[2].low==UINT64_MAX);
            cpu.r[4].low=17;reject([&]{console.handleSyscall(console.memory().data(),&cpu,0);});require(console.ee_console_messages.size()==1);
            cpu.r[4].low=16;std::fill_n(console.memory().begin()+0x1100,4096,static_cast<std::uint8_t>('x'));
            reject([&]{console.handleSyscall(console.memory().data(),&cpu,0);});require(console.ee_console_messages.size()==1);
            require(std::bit_cast<std::uint32_t>(FPU_MOV_S(std::bit_cast<float>(0x80000000u)))==0x80000000u);
            require(FPU_C_EQ_S(0.0f,-0.0f) && !FPU_C_EQ_S(1.0f,0.0f) && FPU_C_EQ_S(std::bit_cast<float>(1u),0.0f)); // Denormal equals zero.
        }

        {
            std::vector<std::uint8_t> packet(240,0x5A),ram(512,0);
            std::copy(packet.begin(),packet.end(),ram.begin()+16);
            GifDmaStaging stage;reject([&]{stage.readIdle();});reject([&]{stage.setCount(15);});
            reject([&]{stage.supply(std::span(packet).first(16));});stage.supply(packet);
            reject([&]{stage.supply(packet);});require(stage.readIdle()==0);
            reject([&]{stage.readIdle();});reject([&]{stage.setCount(0);});stage.setCount(15);
            reject([&]{stage.setCount(15);});
            reject([&]{stage.setAddress(17,17,ram);});reject([&]{stage.setAddress(16,32,ram);});
            reject([&]{stage.setAddress(496,496,ram);});reject([&]{stage.setAddress(528,528,ram);});
            ram[16]^=1;reject([&]{stage.setAddress(16,16,ram);});ram[16]^=1;
            stage.setAddress(16,16,ram);require(stage.ready() && stage.packet_verified && stage.qwc==15
                && stage.madr==16 && stage.idle_reads==1 && stage.staging_writes==2);
            reject([&]{stage.setAddress(16,16,ram);});
        }
        {
            // Authored A+D words: semantic bit mutations must still fail.
            std::vector<std::uint8_t> packet(240,0),ram(256,0);
            const std::array<std::uint64_t,3> registers{0x1A,0x46,0x45};
            for(unsigned i=0;i<3;++i)std::memcpy(packet.data()+24+i*16,&registers[i],8);
            std::copy(packet.begin(),packet.end(),ram.begin()+16);
            for(unsigned i=0;i<3;++i)ram[32+i*16]=0x80;
            GifDmaStaging stage;stage.supply(packet);stage.readIdle();stage.setCount(15);
            ram[32]|=1;reject([&]{stage.setAddress(16,16,ram);});ram[32]&=0xFE;
            ram[40]^=1;reject([&]{stage.setAddress(16,16,ram);});ram[40]^=1;
            ram[16]^=1;reject([&]{stage.setAddress(16,16,ram);});ram[16]^=1;
            ram[80]^=1;reject([&]{stage.setAddress(16,16,ram);});ram[80]^=1;
            stage.setAddress(16,16,ram);require(stage.ready() && stage.reserved_difference_words==3);
            require(ram[32]==0x80 && ram[48]==0x80 && ram[64]==0x80);
        }
        {
            IntcCheckpoint wait;reject([&]{wait.acknowledge(4);});
            wait.arm(12,0x1802);reject([&]{wait.arm(12,0x1802);});
            reject([&]{wait.acknowledge(8);});wait.acknowledge(4);
            require(wait.status()==8 && wait.events==0 && !wait.callbackPending());
            reject([&]{wait.acknowledge(4);});
            require(wait.poll()==8 && wait.events==0);
            require(wait.poll()==12 && wait.events==1 && !wait.callbackPending());
            require(wait.poll()==12 && wait.events==1);wait.acknowledge(4);
            require(wait.status()==8);reject([&]{wait.acknowledge(4);});
            IntcCheckpoint absent;absent.arm(12,0x1802);absent.acknowledge(4);
            for(unsigned i=0;i<64;++i)require(absent.poll(false)==8);
            reject([&]{absent.poll(false);});require(absent.events==0);
        }
        {
            EeRuntime widths;std::vector<std::uint8_t> image(EeRuntime::ram_size,0xA5);
            widths.loadMemory(image);auto* ram=widths.memory().data();R5900Context ctx{};
            widths.write16(ram,2,0x1234);
            require(widths.read16(ram,2)==0x1234);
            reject([&]{(void)widths.read16(ram,3);});
            reject([&]{(void)widths.read16(ram,EeRuntime::ram_size-1);});
            const auto bits=PS2_POR({0x0123456789ABCDEF,0xF00000000000000F},{0xFEDCBA9876543210,0x000000000000FFF0});
            require(bits.low==UINT64_MAX && bits.high==0xF00000000000FFFF);
            require(ram[1]==0xA5 && ram[2]==0x34 && ram[3]==0x12 && ram[4]==0xA5 && widths.halfword_writes==1);
            reject([&]{widths.write16(ram,3,0);});
            reject([&]{widths.write16(ram,EeRuntime::ram_size-1,0);});
            reject([&]{widths.write16(ram,0x12001000,0);});
            reject([&]{widths.handleBreak(ram,&ctx);});
            require(widths.halfword_writes==1 && widths.gs_csr_reads==0 && widths.gs_csr_reset_writes==0);
        }

        {
            // The GS library's reset path is one behavior wherever it is called from (group 2 of
            // RUNTIME_GENERALIZATION_PLAN.md): a CSR reset from any pc, the read after it with the
            // status bits and FIELD clear until the next VBlank, PutIMR returning the previous mask,
            // SetGsCrt recording the mode; the first reset's counters advance once.
            EeRuntime gs;std::vector<std::uint8_t> image(EeRuntime::ram_size);gs.loadMemory(image);
            auto* ram=gs.memory().data();R5900Context ctx{};ctx.pc=0x200000;
            gs.Store64(ram,&ctx,0x12001000,0x200);
            require(gs.gs_resets==1 && gs.gs_csr_reset_writes==1 && gs.gs_csr_cleared); // A CSR reset from any pc resets the GS.
            require(gs.Load64(ram,&ctx,0x12001000)==0x551B4000 && gs.gs_csr_reads==1); // The read after a reset sees the revision and clear status.
            gs.display_field=true;gs.display_intc_stat=0xC;gs.gs_csr_cleared=false;
            require(gs.Load64(ram,&ctx,0x12001000)==0x551B600C && gs.gs_csr_reads==1); // Later reads carry the status bits and FIELD.
            auto syscall=[&](std::uint32_t number,std::uint64_t a0,std::uint64_t a1=0,std::uint64_t a2=0) {
                ctx.r[3].low=number;ctx.r[4].low=a0;ctx.r[5].low=a1;ctx.r[6].low=a2;ctx.r[2].low=0x1234;ctx.r[2].high=7;
                gs.handleSyscall(ram,&ctx,0);return ctx.r[2].low;
            };
            require(syscall(0x71,0x7F00)==0xFF00 && ctx.r[2].high==0 && gs.gs_imr==0x7F00 && gs.gs_imr_writes==1); // PutIMR returns the previous mask.
            require(syscall(0x71,0xFF00)==0x7F00 && syscall(0x70,0)==0xFF00 && gs.gs_imr_writes==1); // GetIMR reads the mask.
            reject([&]{syscall(0x71,0x10000);});
            require(syscall(2,1,2,0)==0 && gs.gs_crt_request==std::array<std::uint32_t,3>{1,2,0} && gs.gs_crt_calls==1); // SetGsCrt records the mode.
            require(syscall(2,0,2,1)==0 && gs.gs_crt_request==std::array<std::uint32_t,3>{0,2,1} && gs.gs_crt_calls==1); // A later SetGsCrt replaces it.
            reject([&]{syscall(2,1,3,0);}); // PAL is not presented.
            gs.Store64(ram,&ctx,0x12001000,0x200);
            require(gs.gs_resets==2 && gs.gs_csr_reset_writes==1 && gs.graph_startup_phase==1); // The next reset is the graph startup's.
        }

        {
            EeRuntime sif;
            std::vector<std::uint8_t> sif_image(EeRuntime::ram_size);
            sif.loadMemory(sif_image);
            auto* sif_ram = sif.memory().data();
            R5900Context sif_context{};
            sif_context.r[3].low = 0x7A;
            sif_context.r[4].low = 0xFFFFFFFF80000000ull;
            sif_context.r[2] = {0xA5, 0xB6};
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            reject([&] { sif.supplySifCommand(0x19600); });
            sif.supplyDmaStatus(0x800040);
            sif.supplyDma5Control(0x184);
            reject([&] { sif.supplySifCommand(0x19601); });
            sif.supplySifCommand(0x19600);
            reject([&] { sif.supplySifCommand(0x19600); });
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            sif.dmac_enable_calls = 1; // Synthetic prerequisite fixture.
            sif_context.r[4].low = 0x80000001;
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            require(sif.sif_command_reads == 0 && sif_context.r[2].low == 0xA5);
            sif_context.r[4].low = 0xFFFFFFFF80000000ull;
            sif.handleSyscall(sif_ram, &sif_context, 0);
            require(sif_context.r[2].low == 0x19600 && sif_context.r[2].high == 0xB6 && sif.sif_command_reads == 1);
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            sif.memory().back() = 0xF1;
            require(sif.read8(sif_ram, EeRuntime::ram_size - 1) == 0xF1);
            reject([&] { sif.read8(sif_ram, EeRuntime::ram_size); });
            reject([&] { sif.read8(nullptr, 0); });
            sif.dataCacheLine(sif_ram, EeRuntime::ram_size - 64);
            reject([&] { sif.dataCacheLine(sif_ram, 1); });
            reject([&] { sif.dataCacheLine(sif_ram, EeRuntime::ram_size); });
            require(sif.data_cache_lines == 1);
            SET_GPR_ZE32(&sif_context, 2, 0x80000000);
            require(sif_context.r[2].low == 0x80000000 && sif_context.r[2].high == 0xB6);
            SET_GPR_ZE32(&sif_context, 0, 1);
            require(GPR_U32(&sif_context, 0) == 0 && SUB32(0, 1) == UINT32_MAX);
            std::vector<std::uint8_t> iop_fixture(EeRuntime::sif_iop_size, 0xA5);
            const std::array<std::uint32_t, 3> iop_table{0x19600, 0x19680, 0x93640};
            std::memcpy(iop_fixture.data() + 0x196C0, iop_table.data(), 12);
            reject([&] { sif.supplySifDma(iop_fixture, 0x702); });
            sif.supplySifDma(iop_fixture, 0x701);
            reject([&] { sif.supplySifRpcState(0); });
            sif.supplySifRpcState(1);
            reject([&] { sif.supplySifRpcState(1); });
            sif_context.r[4].low = 0x80000002;
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            reject([&] { sif.read32(sif_ram, 0x20000200); });
            require(sif.sif_rpc_reads == 0);
            reject([&] { sif.supplySifDma(iop_fixture, 0x701); });
            const std::array<std::uint32_t, 4> descriptor{0x3A9580, 0x19600, 20, 0x44};
            const std::array<std::uint32_t, 5> packet{20, 0, 0x80000000, 0, 0x3A94C0};
            for (unsigned index = 0; index < descriptor.size(); ++index) sif.write32(sif_ram, 0x200 + index * 4, descriptor[index]);
            for (unsigned index = 0; index < packet.size(); ++index) sif.write32(sif_ram, 0x3A9580 + index * 4, packet[index]);
            sif_context.r[3].low = 0x77;
            sif_context.r[4].low = 0x200;
            sif_context.r[5].low = 2;
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            sif_context.r[5].low = 1;
            sif.write32(sif_ram, 0x3A9588, 0x80000009);
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            require(std::equal(iop_fixture.begin(), iop_fixture.end(), sif.sifIopMemory().begin()) && sif.sif_dma_submissions == 0);
            sif.write32(sif_ram, 0x3A9588, 0x80000000);
            const std::vector<std::uint8_t> ee_before_dma(sif.memory().begin(), sif.memory().end());
            sif.handleSyscall(sif_ram, &sif_context, 0);
            auto expected_iop = iop_fixture;
            std::memcpy(expected_iop.data() + 0x19600, packet.data(), 20);
            expected_iop[0x19600] = 0;
            const std::uint32_t new_receive = 0x3A94C0;
            std::memcpy(expected_iop.data() + 0x196C8, &new_receive, 4);
            require(std::equal(expected_iop.begin(), expected_iop.end(), sif.sifIopMemory().begin())
                    && sif.sif_dma_submissions == 1 && sif.sif_ee_receive_address == new_receive
                    && sif_context.r[2].low == 0x701 && sif_context.r[2].high == 0xB6
                    && std::equal(ee_before_dma.begin(), ee_before_dma.end(), sif.memory().begin()));
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            sif_context.r[3].low = 0x7A;
            sif_context.r[4].low = 0xFFFFFFFF80000002ull;
            sif.handleSyscall(sif_ram, &sif_context, 0);
            require(sif.sif_rpc_reads == 1 && sif_context.r[2].low == 1 && sif_context.r[2].high == 0xB6);
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            sif.write32(sif_ram, 0x20000100, 0x89ABCDEF);
            require(sif.read32(sif_ram, 0x100) == 0x89ABCDEF && sif.read32(sif_ram, 0x20000100) == 0x89ABCDEF);
            sif.write32(sif_ram, 0x104, 0x76543210);
            require(sif.read32(sif_ram, 0x20000104) == 0x76543210);
            reject([&] { sif.read32(sif_ram, 0x20000001); });
            reject([&] { sif.write32(sif_ram, 0x22000000, 0); });
            reject([&] { sif.read32(nullptr, 0x20000000); });
            require(sif.uncached_word_accesses == 3);
            sif.write8(sif_ram, 0x20000101, 0x55);
            require(sif.read8(sif_ram, 0x101) == 0x55 && sif.read8(sif_ram, 0x20000101) == 0x55);
            reject([&] { sif.read8(sif_ram, 0x22000000); });
            sif.dataCacheLine(sif_ram, 0x20000100);
            require(sif.uncached_byte_accesses == 2 && sif.data_cache_lines == 2);
            // Authored hostile callback fixture checks interruption isolation,
            // not the generated callback's implementation.
            std::vector<std::uint8_t> bind_bundle(EeRuntime::sif_iop_size + 96);
            auto put = [&](std::size_t offset, std::uint32_t value) {
                std::memcpy(bind_bundle.data() + offset, &value, 4);
            };
            const std::array<std::uint32_t, 16> reply{0, 0, 0x80000008, 0, 0, 0x203A97C0, 0,
                0x3A87A8, 0x80000009, 0x3F570, 0x3FB20, 0, 0, 0, 0, 0};
            std::memcpy(bind_bundle.data() + EeRuntime::sif_iop_size, reply.data(), 64);
            put(EeRuntime::sif_iop_size + 64, 0x801);
            put(0x3F570, 0x80000592); put(0x3F578, 0x3FB20);
            put(0x3F5B0, 0x3F558); put(0x3F560, 0x3F570);
            const std::array<std::uint32_t, 7> call_sema{0, 1, 0, 0, 0, 0x3838E8, 3};
            std::memcpy(bind_bundle.data() + EeRuntime::sif_iop_size + 68, call_sema.data(), 28);
            put(0x3F560, 0);
            reject([&] { sif.supplySifBind(bind_bundle); });
            put(0x3F560, 0x3F570);
            sif.supplySifBind(bind_bundle);
            reject([&] { sif.supplySifBind(bind_bundle); });
            sif.initial_sp = 0x1FFED60;
            const auto frame = sif.initial_sp - 464;
            const std::array<std::uint32_t, 4> bind_descriptor{0x203A97C0, 0x19600, 64, 0x44};
            const std::array<std::uint32_t, 16> bind_packet{64, 0, 0x80000009, 0, 5, 0x203A97C0,
                2, 0x3A87A8, 0x80000592, 0, 0, 0, 0, 0, 0, 0};
            std::memcpy(sif_ram + frame, bind_descriptor.data(), 16);
            std::memcpy(sif_ram + 0x3A97C0, bind_packet.data(), 64);
            sif.write32(sif_ram, 0x3A87A8, 0x203A97C0);
            sif.write32(sif_ram, 0x3A87AC, 2); sif.write32(sif_ram, 0x3A87B0, 3);
            sif.write32(sif_ram, 0x3A9620, 0x3322E8);
            sif.write32(sif_ram, 0x3A9624, 0x3AAFC0); sif.write32(sif_ram, 0x3A9628, 0x38BDF0);
            sif.semaphores.resize(4); sif.semaphores[3] = {0, 1, 0x3838D8};
            for (unsigned index = 0; index < 32; ++index) {
                sif_context.r[index] = {0x1234567800000000ull + index, 0xABCDEF0010000000ull + index};
                sif_context.f[index] = static_cast<float>(index);
            }
            sif_context.r[3].low = 0x77; sif_context.r[4].low = frame;
            sif_context.r[5].low = 1; sif_context.r[29].low = frame;
            sif_context.hi = 19; sif_context.lo = 23; sif_context.cop0_status = 0x1234;
            sif_context.pc = 0x32F028;
            const auto cpu_before = sif_context;
            std::fill_n(sif_ram + frame - 64, 64, 0xA7);
            const auto iop_before = std::vector<std::uint8_t>(sif.sifIopMemory().begin(), sif.sifIopMemory().end());
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); }); // Missing compiled handler.
            require(sif.sif_bind_callbacks == 0 && sif.semaphores[3].count == 0
                    && std::equal(iop_before.begin(), iop_before.end(), sif.sifIopMemory().begin()));
            sif.registerFunction(0x3322E8, clobber_rpc_callback);
            sif.registerFunction(0x332258, return_leaf); sif.registerFunction(0x32EC90, return_leaf);
            sif.handleSyscall(sif_ram, &sif_context, 0);
            auto expected_cpu = cpu_before; expected_cpu.r[2].low = 0x801;
            require(std::memcmp(&expected_cpu, &sif_context, sizeof(R5900Context)) == 0
                    && sif.sif_bind_callbacks == 1 && sif.sif_bind_context_preservations == 1
                    && sif.sif_dma_submissions == 2 && sif.semaphores[3].count == 1
                    && std::all_of(sif_ram + frame - 64, sif_ram + frame, [](auto value) { return value == 0xA7; }));
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); }); // Repeated/unknown transfer.
            sif_context.r[3].low = 0xFFFFFFBD;
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); }); // Scope closed.
        }
        const auto left = std::bit_cast<ProbeGpr>(std::array<std::uint32_t, 4>{UINT32_MAX, 0, 0x80000000, 1});
        const auto right = std::bit_cast<ProbeGpr>(std::array<std::uint32_t, 4>{1, 2, 0x80000000, 2});
        require(std::bit_cast<std::array<std::uint32_t, 4>>(ps2_paddu32(left, right))
                == std::array<std::uint32_t, 4>{UINT32_MAX, 2, UINT32_MAX, 3});
        R5900Context context{};
        require(std::bit_cast<std::array<std::uint32_t,4>>(PS2_PSUBW(left,right))
            ==std::array<std::uint32_t,4>{0xFFFFFFFE,0xFFFFFFFE,0,UINT32_MAX});
        context.r[0] = left;
        SET_GPR_VEC(&context, 0, right);
        require(GPR_VEC(&context, 0).low == 0 && context.r[0].low == left.low);
        context.r[4] = {0x8000000000000000ull, 0x12345678};
        require(GPR_S64(&context, 4) == INT64_MIN);
        SET_GPR_S64(&context, 4, GPR_S64(&context, 4) >> 32);
        require(context.r[4].low == 0xFFFFFFFF80000000ull && context.r[4].high == 0x12345678);
        SET_GPR_S64(&context, 4, INT64_MAX);
        require(GPR_S64(&context, 4) >> 32 == 0x7FFFFFFF);
        SET_GPR_S64(&context, 0, -1);
        require(GPR_S64(&context, 0) == 0 && context.r[0].low == left.low);
        EeRuntime runtime;
        std::vector<std::uint8_t> image(EeRuntime::ram_size, 0xA5);
        runtime.loadMemory(image);
        auto* memory = runtime.memory().data();
        runtime.write128(memory, 16, right);
        require(runtime.read32(memory, 16) == 1 && memory[15] == 0xA5 && memory[32] == 0xA5);
        runtime.write8(memory, static_cast<std::uint32_t>(EeRuntime::ram_size - 1), 7);
        require(runtime.memory().back() == 7);
        reject([&] { runtime.write8(memory, static_cast<std::uint32_t>(EeRuntime::ram_size), 0); });
        reject([&] { runtime.write128(memory, static_cast<std::uint32_t>(EeRuntime::ram_size - 8), right); });
        reject([&] { runtime.write128(memory, 1, right); });
        reject([&] { (void)runtime.read32(memory, 1); });
        reject([&] { (void)runtime.read32(memory, 0x10000000); });
        reject([&] { runtime.write8(nullptr, 0, 0); });
        context.r[3].low = 0x3C;
        context.r[5].low = UINT32_MAX;
        context.r[6].low = 0x100000;
        context.r[7].low = 0x100;
        context.r[2].high = 0x1234;
        context.r[6].low = UINT32_MAX;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        require(!runtime.thread_initialized && memory[0x100] == 0xA5);
        context.r[6].low = 0x100000;
        context.r[7].low = EeRuntime::ram_size - 4;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        require(!runtime.thread_initialized);
        context.r[7].low = 0x101;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[7].low = 0x100;
        context.r[5].low = 0x100000;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[5].low = UINT32_MAX;
        runtime.handleSyscall(memory, &context, 0);
        require(context.r[2].low == 0x01FFED60 && context.r[2].high == 0x1234
                && runtime.stack_base == 0x01EFF000 && runtime.read32(memory, 0x100) == 0
                && runtime.read32(memory, 0x104) == 0 && memory[0x108] == 0xA5);
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[3].low = 0x3D;
        context.r[4].low = runtime.stack_base;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        require(!runtime.heap_initialized);
        context.r[4].low = 0x43058F;
        context.r[5].low = 0x1000;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[5].low = UINT32_MAX;
        runtime.handleSyscall(memory, &context, 0);
        require(runtime.heap_start == 0x43058F && runtime.heap_end == 0x1EFF000
                && context.r[2].low == runtime.heap_end && context.r[2].high == 0x1234);
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[3].low=0x3E;runtime.handleSyscall(memory,&context,0);
        require(context.r[2].low==runtime.heap_end && context.r[2].high==0x1234 && runtime.heap_end_queries==1);
        EeRuntime uninitialized;
        reject([&] { uninitialized.handleSyscall(nullptr, &context, 0); });
        runtime.write32(memory, 0x170, 0x89ABCDEF);
        require(runtime.read32(memory, 0x170) == 0x89ABCDEF && memory[0x174] == 0xA5);
        reject([&] { runtime.write32(memory, 1, 0); });
        reject([&] { runtime.write32(memory, EeRuntime::ram_size - 2, 0); });
        runtime.write64(memory, 0x180, UINT64_C(0xFEDCBA9876543210));
        require(runtime.read64(memory, 0x180) == UINT64_C(0xFEDCBA9876543210) && memory[0x188] == 0xA5);
        reject([&] { runtime.write64(memory, 3, 0); });
        reject([&] { (void)runtime.read64(memory, EeRuntime::ram_size - 4); });
        runtime.registerFunction(0x200, nested_call);
        runtime.registerFunction(0x300, return_leaf);
        runtime.registerFunction(0x400, bad_return);
        runtime.registerFunction(0x500, tail_call);
        runtime.registerFunction(0x600, return_leaf);
        SET_GPR_U32(&context, 31, 0x104);
        require(runtime.dispatchGuestBranch(memory, &context, 0x200, 0x100, 0x104,
                EeRuntime::GuestBranchKind::DirectCall, "synthetic call") && context.pc == 0x104);
        require(runtime.returns == 2 && runtime.guest_calls == 2);
        reject([&] { runtime.dispatchGuestBranch(memory, &context, 0x400, 0x100, 0x104,
                     EeRuntime::GuestBranchKind::DirectCall, "bad callee"); });
        SET_GPR_U32(&context, 31, 0x104);
        require(runtime.dispatchGuestBranch(memory, &context, 0x500, 0x100, 0x104,
                EeRuntime::GuestBranchKind::DirectCall, "tail callee") && context.pc == 0x104);
        require(runtime.entered_functions==std::set<std::uint32_t>{0x200,0x300,0x400,0x500});
        require(runtime.completed_functions==std::set<std::uint32_t>{0x200,0x300,0x500});
        SET_GPR_U32(&context,31,0x104);
        require(runtime.dispatchGuestBranch(memory,&context,0x300,0x100,0x104,EeRuntime::GuestBranchKind::DirectCall,"repeated entry"));
        require(runtime.entered_functions.size()==4 && runtime.completed_functions.size()==3);
        reject([&]{runtime.dispatchGuestBranch(memory,&context,0x600,0x100,0,EeRuntime::GuestBranchKind::DirectCall,"invalid resume");});
        require(runtime.entered_functions.size()==4 && runtime.completed_functions.size()==3);
        runtime.Store32(memory, &context, 0x500, 0x12345678);
        require(runtime.read32(memory, 0x500) == 0x12345678);
        reject([&] { runtime.Store32(memory, &context, 0x1000E010, 1); });
        require(runtime.read32(memory, 0x500) == 0x12345678);
        EeRuntime dma_checkpoint;
        dma_checkpoint.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size, 0xA5));
        auto* dma_ram = dma_checkpoint.memory().data();
        reject([&] { dma_checkpoint.read32(dma_ram, 0x1000E010); });
        reject([&] { dma_checkpoint.supplyDma5Control(0x184); });
        reject([&] { dma_checkpoint.supplyDmaStatus(0); });
        dma_checkpoint.supplyDmaStatus(0x00800040);
        reject([&] { dma_checkpoint.supplyDmaStatus(0x00800040); });
        reject([&] { dma_checkpoint.supplyDma5Control(0); });
        dma_checkpoint.supplyDma5Control(0x184);
        reject([&] { dma_checkpoint.supplyDma5Control(0x184); });
        reject([&] { dma_checkpoint.read32(dma_ram, 0x1000C000); });
        reject([&] { dma_checkpoint.Load32(nullptr, &context, 0x1000E010); });
        require(dma_checkpoint.dma_status_reads == 0 && dma_checkpoint.dma5_control_reads == 0);
        require(dma_checkpoint.Load32(dma_ram, &context, 0x1000E010) == 0x00800040);
        require(dma_checkpoint.read32(dma_ram, 0x1000C000) == 0x184);
        reject([&] { dma_checkpoint.read32(dma_ram, 0x1000E010); });
        reject([&] { dma_checkpoint.read32(dma_ram, 0x1000C000); });
        reject([&] { dma_checkpoint.Store32(dma_ram, &context, 0x1000E010, 0x20); });
        require(dma_checkpoint.dma_status_reads == 1 && dma_checkpoint.dma5_control_reads == 1
                && std::all_of(dma_checkpoint.memory().begin(), dma_checkpoint.memory().end(),
                               [](auto byte) { return byte == 0xA5; }));
        R5900Context dma_context{};
        dma_context.r[3].low = 0x16;
        dma_context.r[4].low = 5;
        reject([&] { dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0); });
        dma_context.r[3].low = 0x12;
        dma_context.r[5].low = 0x1000;
        dma_context.r[6].low = 0;
        dma_context.r[7].low = 0x20;
        dma_context.r[28].low = 0x2000;
        dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0);
        require(dma_context.r[2].low == 1 && dma_checkpoint.dmac_handlers.size() == 1
                && dma_checkpoint.dmac_handlers[0].channel == 5
                && dma_checkpoint.dmac_handlers[0].address == 0x1000
                && dma_checkpoint.dmac_handlers[0].arg == 0x20
                && dma_checkpoint.dmac_handlers[0].gp == 0x2000
                && dma_checkpoint.dmac_handler_order == std::vector<std::uint32_t>{1});
        for (const auto index : {4u, 5u, 6u}) {
            const auto saved = dma_context.r[index];
            dma_context.r[index].low = index == 4 ? 6 : index == 5 ? 0x1001 : 1;
            reject([&] { dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0); });
            dma_context.r[index] = saved;
        }
        require(dma_checkpoint.dmac_handlers.size() == 1);
        dma_context.r[3].low = 0x16;
        dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0);
        require(dma_context.r[2].low == 1 && dma_checkpoint.dmac_mask == 0x00A00000
                && dma_checkpoint.dmac_enable_calls == 1);
        dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0);
        require(dma_context.r[2].low == 0 && dma_checkpoint.dmac_enable_calls == 2);
        dma_context.r[4].low = 6;
        reject([&] { dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0); });
        require(dma_checkpoint.dmac_mask == 0x00A00000 && dma_checkpoint.dmac_enable_calls == 2);
        dma_context.r[4].low = 5;
        dma_context.r[3].low = 0x12;
        dma_checkpoint.dmac_handlers.resize(128);
        dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0);
        require(dma_context.r[2].low == UINT64_MAX && dma_checkpoint.dmac_handlers.size() == 128);
        EeRuntime semaphore_runtime;
        semaphore_runtime.semaphores.push_back({1, 1, 0});
        R5900Context semaphore_context{};
        semaphore_context.r[3].low = 0x44;
        semaphore_context.r[4].low = 0;
        semaphore_context.r[2] = {0x1234, 0x5678};
        semaphore_runtime.handleSyscall(nullptr, &semaphore_context, 0);
        require(semaphore_context.r[2].low == 0 && semaphore_context.r[2].high == 0x5678
                && semaphore_runtime.semaphores[0].count == 0 && semaphore_runtime.semaphore_waits == 1);
        semaphore_context.r[2].low = 0x1234;
        reject([&] { semaphore_runtime.handleSyscall(nullptr, &semaphore_context, 0); });
        require(semaphore_context.r[2].low == 0x1234 && semaphore_runtime.semaphore_waits == 1);
        semaphore_context.r[3].low = 0x42;
        semaphore_runtime.handleSyscall(nullptr, &semaphore_context, 0);
        require(semaphore_context.r[2].low == 0 && semaphore_runtime.semaphores[0].count == 1
                && semaphore_runtime.semaphore_signals == 1);
        reject([&] { semaphore_runtime.handleSyscall(nullptr, &semaphore_context, 0); });
        require(semaphore_runtime.semaphores[0].count == 1 && semaphore_runtime.semaphore_signals == 1);
        for (const auto selector : {0x42u, 0x44u}) {
            semaphore_context.r[3].low = selector;
            semaphore_context.r[4].low = UINT32_MAX;
            semaphore_runtime.handleSyscall(nullptr, &semaphore_context, 0);
            require(semaphore_context.r[2].low == UINT64_MAX && semaphore_context.r[2].high == 0x5678);
        }
        {
            EeRuntime disc;disc.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
            R5900Context dc{}; auto* dm=disc.memory().data();
            disc.initial_sp=0x1FFED60;disc.mc_rpc_calls=2;disc.mc_rpc_binds=1;
            disc.semaphores.resize(10);dc.pc=0x32EC68;dc.r[3].low=0x40;
            dc.r[4].low=disc.initial_sp-592;require(dc.r[4].low==0x1FFEB10);dc.r[31].low=0x332814;
            dc.r[17].low=0x3A8780;dc.r[19].low=0xFFFFFFFF80000597;
            disc.write32(dm,0x3A8780,0x203A97C0);disc.write32(dm,0x3A8784,42);
            for(unsigned i=0;i<6;++i)disc.write32(dm,static_cast<std::uint32_t>(dc.r[4].low)+4*i,
                std::array<std::uint32_t,6>{144,1,0,0x80808080,1,0x3838D8}[i]);
            dc.r[19].low=0x80000596;disc.handleSyscall(dm,&dc,0); // Any attr is accepted; not the disc's bind semaphore.
            require(dc.r[2].low==10 && disc.disc_bind_semaphores==0 && disc.semaphores.size()==11);
            disc.semaphores.resize(10);
            dc.r[19].low=0xFFFFFFFF80000597;disc.handleSyscall(dm,&dc,0);
            require(dc.r[2].low==10 && disc.disc_bind_semaphores==1 && disc.semaphores[10].count==0
                && disc.semaphores[10].maximum==1 && disc.semaphores[10].option==0x3838D8);
            disc.handleSyscall(dm,&dc,0);require(dc.r[2].low==11 && disc.disc_bind_semaphores==1); // Counted once.
        }
        {
            EeRuntime disc;disc.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
            auto* dm=disc.memory().data();R5900Context dc{};disc.initial_sp=0x1FFED60;
            disc.disc_rpc_calls=1;disc.semaphores.resize(10);dc.pc=0x32EC68;dc.r[3].low=0x40;
            dc.r[4].low=disc.initial_sp-608;dc.r[31].low=0x332814;dc.r[17].low=0x369590;dc.r[19].low=0xFFFFFFFF80000595;
            disc.write32(dm,0x369594,44);
            for(unsigned i=0;i<6;++i)disc.write32(dm,static_cast<unsigned>(dc.r[4].low)+i*4,std::array<unsigned,6>{144,1,0,0,1,0x3838D8}[i]);
            dc.r[17].low=0x369A90;disc.handleSyscall(dm,&dc,0);require(disc.ncmd_bind_semaphores==0 && dc.r[2].low==10);
            disc.semaphores.resize(10);
            dc.r[17].low=0x369590;disc.handleSyscall(dm,&dc,0);require(disc.ncmd_bind_semaphores==1 && dc.r[2].low==10);
            disc.handleSyscall(dm,&dc,0);require(disc.ncmd_bind_semaphores==1 && dc.r[2].low==11);
            disc.semaphores.resize(10);disc.ncmd_rpc_binds=1;dc.r[4].low=disc.initial_sp-624;dc.r[31].low=0x332AD0;dc.r[19].low=1;
            disc.write32(dm,0x369594,45);
            for(unsigned i=0;i<6;++i)disc.write32(dm,static_cast<unsigned>(dc.r[4].low)+i*4,std::array<unsigned,6>{144,1,0,0,1,0x3838E8}[i]);
            dc.r[31].low=0x332814;disc.handleSyscall(dm,&dc,0);require(disc.semaphores.size()==11);
            disc.semaphores.resize(10);dc.r[31].low=0x332AD0;disc.handleSyscall(dm,&dc,0);require(dc.r[2].low==10 && disc.semaphores[10].option==0x3838E8);
        }
        context.r[3].low = 0x40;
        context.r[4].low = 0x600;
        context.r[2].high = 0x1234;
        runtime.write32(memory, 0x600, UINT32_MAX); // Output-only count ignored.
        runtime.write32(memory, 0x604, 2);
        runtime.write32(memory, 0x608, 1);
        runtime.write32(memory, 0x60C, UINT32_MAX); // Output-only wait count ignored.
        runtime.write32(memory, 0x610, 0);
        runtime.write32(memory, 0x614, 0xDEADBEEF);
        runtime.handleSyscall(memory, &context, 0);
        require(context.r[2].low == 0 && context.r[2].high == 0x1234
                && runtime.semaphores[0].count == 1 && runtime.semaphores[0].maximum == 2
                && runtime.semaphores[0].option == 0xDEADBEEF && runtime.read32(memory, 0x600) == UINT32_MAX);
        runtime.write32(memory, 0x608, 3);
        runtime.handleSyscall(memory, &context, 0);
        require(context.r[2].low == UINT64_MAX && runtime.semaphores.size() == 1);
        runtime.write32(memory, 0x608, 1);
        runtime.write32(memory, 0x610, 1); // attr is ignored, as by the kernel.
        runtime.handleSyscall(memory, &context, 0);
        require(context.r[2].low == 1 && runtime.semaphores.size() == 2);
        {   // DeleteSema: any semaphore; an older one leaves its id free for the next CreateSema.
            R5900Context d{};d.r[3].low=0x41;d.r[4].low=0;
            runtime.handleSyscall(memory, &d, 0);
            require(d.r[2].low == 0 && runtime.semaphores.size() == 2 && runtime.semaphores[0].deleted);
            runtime.handleSyscall(memory, &d, 0);require(d.r[2].low == UINT64_MAX); // Already deleted.
            d.r[3].low=0x45;runtime.handleSyscall(memory, &d, 0);require(d.r[2].low == UINT64_MAX); // Poll of a deleted id.
            runtime.handleSyscall(memory, &context, 0);
            require(context.r[2].low == 0 && !runtime.semaphores[0].deleted && runtime.semaphores.size() == 2);
            d.r[3].low=0x41;d.r[4].low=1;runtime.handleSyscall(memory, &d, 0); // The newest is removed.
            require(d.r[2].low == 1 && runtime.semaphores.size() == 1);
        }
        runtime.write32(memory, 0x610, 0);
        context.r[4].low = EeRuntime::ram_size - 20;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[4].low = 0x601;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[4].low = 0x600;
        for (unsigned id = 1; id < 256; ++id) {
            runtime.handleSyscall(memory, &context, 0);
            require(context.r[2].low == id);
        }
        runtime.handleSyscall(memory, &context, 0);
        require(context.r[2].low == UINT64_MAX && runtime.semaphores.size() == 256);
        context.r[3].low = 0x74;
        context.r[4].low = 0x83;
        context.r[5].low = 0x700;
        context.r[2].low = 0x987654;
        runtime.handleSyscall(memory, &context, 0);
        require(runtime.syscall_handlers[0x83] == 0x700 && context.r[2].low == 0x987654);
        context.r[5].low = 0x701;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        require(runtime.syscall_handlers[0x83] == 0x700);
        context.r[5].low = EeRuntime::ram_size;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[4].low = 256;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[3].low = 0x83;
        bool handler_stop = false;
        try { runtime.handleSyscall(memory, &context, 0); }
        catch (const UnsupportedSyscall& stop) {
            handler_stop = stop.selector == 0x83 && stop.handler == 0x700 && stop.resume_pc == context.pc;
        }
        require(handler_stop);
        runtime.registerFunction(0x700, return_leaf);
        context.pc = 0x104;
        context.r[31] = {0xABCDEF, 0x123456};
        runtime.handleSyscall(memory, &context, 0);
        require(context.pc == 0x104 && context.r[31].low == 0xABCDEF
                && context.r[31].high == 0x123456 && runtime.custom_syscall_calls == 1);
        bool mmio_stop = false;
        try { (void)runtime.Load32(memory, &context, 0x10001810); }
        catch (const UnsupportedGuestMemory& stop) { mmio_stop = stop.address == 0x10001810 && stop.size == 4; }
        require(mmio_stop);
        runtime.supplyTimer3Mode(0x83);
        require(runtime.Load32(memory, &context, 0x10001810) == 0x83 && runtime.timer3_mode_reads == 1);
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001810); });
        reject([&] { runtime.supplyTimer3Mode(0x83); });
        reject([&] { runtime.write32(memory, 0x10001810, 0); });
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001800); });
        EeRuntime invalid_timer;
        reject([&] { invalid_timer.supplyTimer3Mode(0x10000); });
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001010); });
        reject([&] { invalid_timer.supplyTimer2Mode(0x10000); });
        reject([&] { runtime.write32(memory, 0xB0001010, 0x382); });
        runtime.supplyTimer2Mode(0x80);
        reject([&] { runtime.write32(memory, 0xB0001010, 0x382); });
        reject([&] { runtime.supplyTimer2Mode(0x80); });
        reject([&] { (void)runtime.Load32(nullptr, &context, 0x10001010); });
        require(runtime.timer2_mode_reads == 0);
        require(runtime.Load32(memory, &context, 0x10001010) == 0x80 && runtime.timer2_mode_reads == 1);
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001010); });
        reject([&] { runtime.write32(memory, 0x10001010, 0x382); });
        reject([&] { runtime.write32(memory, 0xB0001020, 0x382); });
        reject([&] { runtime.write32(memory, 0xB0001010, 0x381); });
        reject([&] { runtime.write32(nullptr, 0xB0001010, 0x382); });
        runtime.write32(memory, 0xB0001010, 0x382);
        require(runtime.timer2_mode_writes == 1 && runtime.timer2_mode_after_write == 0x382);
        reject([&] { runtime.write32(memory, 0xB0001010, 0x382); });
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001010); });
        reject([&] { runtime.supplyTimer2Readback(0x10000); });
        runtime.supplyTimer2Readback(0x382);
        reject([&] { runtime.supplyTimer2Readback(0x382); });
        reject([&] { (void)runtime.Load32(nullptr, &context, 0x10001010); });
        require(runtime.timer2_mode_reads == 1);
        require(runtime.Load32(memory, &context, 0x10001010) == 0x382 && runtime.timer2_mode_reads == 2);
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001010); });
        EeRuntime mismatch;
        mismatch.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        mismatch.supplyTimer2Mode(0x80);
        mismatch.supplyTimer2Readback(0x782);
        auto* mismatch_ram = mismatch.memory().data();
        require(mismatch.Load32(mismatch_ram, &context, 0x10001010) == 0x80);
        reject([&] { (void)mismatch.Load32(mismatch_ram, &context, 0x10001010); });
        mismatch.write32(mismatch_ram, 0xB0001010, 0x382);
        reject([&] { (void)mismatch.Load32(mismatch_ram, &context, 0x10001010); });
        require(mismatch.timer2_mode_reads == 1 && mismatch.timer2_mode_after_write == 0x382);
        context.ll_bit = true;
        runtime.clearLLBit(&context);
        require(!context.ll_bit);
        EeRuntime patched;
        patched.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        auto* patch_ram = patched.memory().data();
        reject([&] { patched.enableKernelPatch(0x80076000, 8); });
        const std::vector<std::uint8_t> baseline(EeRuntime::kernel_snapshot_size, 0x5A);
        patched.loadKernelSnapshot(baseline);
        reject([&] { patched.enableKernelPatch(0x80076001, 8); });
        reject([&] { patched.enableKernelPatch(0x80076000, 0); });
        reject([&] { patched.enableKernelPatch(0x8007FFFC, 8); });
        reject([&] { patched.enableKernelPatch(0x80076000, UINT32_MAX); });
        patched.enableKernelPatch(0x80076000, 8);
        reject([&] { patched.enableKernelPatch(0x80076000, 8); });
        reject([&] { patched.loadKernelSnapshot(baseline); });
        patched.write32(patch_ram, 0x80076000, 0x12345678);
        patched.write32(patch_ram, 0x80076004, 0xABCDEF01);
        require(patched.read32(patch_ram, 0x80076000) == 0x12345678
                && patched.read32(patch_ram, 0x80076004) == 0xABCDEF01);
        reject([&] { patched.write32(patch_ram, 0x80075FFC, 0); });
        reject([&] { patched.write32(patch_ram, 0x80076008, 0); });
        reject([&] { patched.write32(patch_ram, 0x80076002, 0); });
        reject([&] { patched.write32(nullptr, 0x80076000, 0); });
        reject([&] { patched.write32(patch_ram, 0xA0076000, 0); });
        require(std::equal(baseline.begin(), baseline.end(), patched.kernelBaseline().begin()));
        require(patched.kernel_word_writes == 2 && patched.word_writes == 2
                && patched.read32(patch_ram, 0x80076008) == 0x5A5A5A5A);
        R5900Context registration{};
        registration.r[3].low = 0x74;
        registration.r[4].low = 0x5B;
        registration.r[2] = {0x1234, 0x5678};
        registration.pc = 0x204;
        for (const auto address : {0x80075FFCu, 0x80076008u, 0xA0076000u}) {
            registration.r[5].low = address;
            reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        }
        EeRuntime unwritten;
        unwritten.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        unwritten.loadKernelSnapshot(baseline);
        unwritten.enableKernelPatch(0x80076000, 8);
        registration.r[5].low = 0x80076000;
        reject([&] { unwritten.handleSyscall(unwritten.memory().data(), &registration, 0); });
        reject([&] { patched.handleSyscall(nullptr, &registration, 0); });
        require(patched.syscall_registrations == 0 && patched.syscall_handlers[0x5B] == 0);
        for (const auto address : {0x80076000u, 0x80076004u}) {
            registration.r[5].low = address;
            patched.handleSyscall(patch_ram, &registration, 0);
            require(patched.syscall_handlers[0x5B] == address && registration.r[2].low == 0x1234
                    && registration.r[2].high == 0x5678 && registration.pc == 0x204);
        }
        registration.r[3].low = 0x5B;
        bool kernel_callback_stop = false;
        try { patched.handleSyscall(patch_ram, &registration, 0); }
        catch (const UnsupportedSyscall& stop) {
            kernel_callback_stop = stop.selector == 0x5B && stop.handler == 0x80076004 && stop.resume_pc == 0x204;
        }
        require(kernel_callback_stop && patched.custom_syscall_calls == 0);
        registration.r[3].low = 0x74;
        registration.r[5].low = 0;
        patched.handleSyscall(patch_ram, &registration, 0);
        require(patched.syscall_handlers[0x5B] == 0 && patched.syscall_registrations == 3);
        registration.r[5].low = 0x80076000;
        patched.handleSyscall(patch_ram, &registration, 0);
        const std::array<std::uint32_t, 2> compiled_words{0x12345678, 0xABCDEF01};
        const auto compiled_bytes = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(compiled_words.data()), 8);
        reject([&] { patched.registerFunction(0x80076000, return_leaf); });
        reject([&] { patched.registerKernelFunction(0x80076000, nullptr, compiled_bytes); });
        reject([&] { patched.registerKernelFunction(0x80076004, return_leaf, compiled_bytes); });
        reject([&] { patched.registerKernelFunction(0x80076000, return_leaf, compiled_bytes.first(4)); });
        patched.registerKernelFunction(0x80076000, return_leaf, compiled_bytes);
        reject([&] { patched.registerKernelFunction(0x80076000, return_leaf, compiled_bytes); });
        registration.r[3].low = 0x5B;
        patched.handleSyscall(patch_ram, &registration, 0);
        require(patched.kernel_dispatch_validations == 1 && patched.returns == 1 && registration.pc == 0x204);
        patched.write32(patch_ram, 0x80076004, 0);
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        require(patched.kernel_dispatch_validations == 1 && patched.returns == 1);
        unwritten.registerKernelFunction(0x80076000, return_leaf, compiled_bytes);
        reject([&] { unwritten.callFunction(0x80076000, registration); });
        require(unwritten.kernel_dispatch_validations == 0);
        registration.r[3].low = 0x74;
        registration.r[4].low = 0x12C;
        registration.r[5].low = 0x80076000;
        reject([&] { unwritten.handleSyscall(unwritten.memory().data(), &registration, 0); });
        require(unwritten.timer3_interrupt_handler == 0 && !unwritten.timer3_interrupt_enable_requested);
        for (const auto address : {0u, 0x100u, 0x80076008u, 0x80076001u, 0xA0076000u}) {
            registration.r[5].low = address;
            reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        }
        require(!patched.timer3_interrupt_handler && !patched.extended_registrations);
        registration.r[5].low = 0x80076004;
        const auto ordinary_handlers = patched.syscall_handlers;
        const auto registrations_before = patched.syscall_registrations;
        registration.r[2] = {0x1234, 0x5678};
        patched.handleSyscall(patch_ram, &registration, 0);
        require(patched.timer3_interrupt_handler == 0x80076004 && patched.timer3_interrupt_enable_requested
                && patched.extended_registrations == 1 && patched.syscall_registrations == registrations_before + 1
                && patched.syscall_handlers == ordinary_handlers && registration.r[2].low == 0x1234
                && registration.r[2].high == 0x5678);
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        registration.r[4].low = 0x12D;
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        registration.r[3].low = 0x12C;
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        require(patched.timer3_interrupt_handler == 0x80076004 && patched.extended_registrations == 1
                && patched.syscall_registrations == registrations_before + 1 && patched.syscall_handlers == ordinary_handlers);
        require(patched.intc_handlers.size() == 1 && patched.intc_handler_order == std::vector<std::uint32_t>{1});
        registration.r[3].low = 0x10;
        registration.r[4].low = 11;
        registration.r[5].low = 0x100;
        registration.r[6].low = registration.r[7].low = 0;
        registration.r[28].low = 0x12345678;
        patched.handleSyscall(patch_ram, &registration, 0);
        require(registration.r[2].low == 2 && registration.r[2].high == 0x5678
                && patched.intc_handlers[1].cause == 11 && patched.intc_handlers[1].address == 0x100
                && patched.intc_handlers[1].arg == 0 && patched.intc_handlers[1].gp == 0x12345678
                && patched.intc_handler_order == std::vector<std::uint32_t>{2, 1});
        for (const auto address : {0u, 0x101u, static_cast<std::uint32_t>(EeRuntime::ram_size), 0x80076000u}) {
            registration.r[5].low = address;
            reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        }
        registration.r[5].low = 0x100;
        registration.r[6].low = UINT32_MAX;
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        registration.r[6].low = 0;
        registration.r[4].low = 12;
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        require(patched.intc_handlers.size() == 2 && registration.r[2].low == 2);
        registration.r[4].low = 11;
        for (unsigned id = 3; id <= 128; ++id) {
            patched.handleSyscall(patch_ram, &registration, 0);
            require(registration.r[2].low == id);
        }
        patched.handleSyscall(patch_ram, &registration, 0);
        require(registration.r[2].low == UINT64_MAX && registration.r[2].high == 0x5678
                && patched.intc_handlers.size() == 128 && patched.intc_handler_order.front() == 128);
        EeRuntime intc;
        R5900Context intc_context{};
        intc_context.r[3].low = 0x14;
        intc_context.r[4].low = 11;
        intc_context.r[2] = {0xABCD, UINT64_MAX};
        reject([&] { intc.handleSyscall(nullptr, &intc_context, 0); });
        reject([&] { intc.supplyIntcMask(0x10000); });
        intc.supplyIntcMask(0x1002);
        reject([&] { intc.supplyIntcMask(0x1002); });
        for (const auto cause : {0u, 12u, 32u, UINT32_MAX}) {
            intc_context.r[4].low = cause;
            reject([&] { intc.handleSyscall(nullptr, &intc_context, 0); });
        }
        require(intc.intc_mask == 0x1002 && intc.intc_enable_calls == 0 && intc_context.r[2].low == 0xABCD);
        intc_context.r[4].low = 11;
        reject([&] { intc.handleSyscall(nullptr, &intc_context, 1); });
        intc.handleSyscall(nullptr, &intc_context, 0);
        require(intc.intc_mask == 0x1802 && intc_context.r[2].low == 1 && intc_context.r[2].high == UINT64_MAX);
        intc.handleSyscall(nullptr, &intc_context, 0);
        require(intc.intc_mask == 0x1802 && intc_context.r[2].low == 0 && intc.intc_enable_calls == 2);
        EeRuntime already_enabled;
        already_enabled.supplyIntcMask(0x1802);
        already_enabled.handleSyscall(nullptr, &intc_context, 0);
        require(intc_context.r[2].low == 0 && already_enabled.intc_mask == 0x1802);
        EeRuntime creation;
        creation.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size, 0xA5));
        auto* creation_ram = creation.memory().data();
        const auto descriptor = [&](unsigned offset, std::uint32_t value) {
            creation.write32(creation_ram, 0x300 + offset, value);
        };
        descriptor(4, 0x1000); descriptor(8, 0x10000); descriptor(12, 0x400);
        descriptor(16, 0x2000); descriptor(20, 0);
        R5900Context create_context{};
        create_context.r[3].low = 0x20;
        create_context.r[4].low = 0x300;
        create_context.r[2] = {0xCAFE, UINT64_MAX};
        reject([&] { creation.handleSyscall(creation_ram, &create_context, 0); });
        creation.thread_initialized = true;creation.current_thread_id = 1;creation.thread_arguments[4] = 0x100250;
        reject([&] { creation.handleSyscall(creation_ram, &create_context, 0); });
        creation.heap_initialized = true;
        creation.heap_start = 0x4000;
        for (const auto address : {0x301u, static_cast<std::uint32_t>(EeRuntime::ram_size - 32)}) {
            create_context.r[4].low = address;
            reject([&] { creation.handleSyscall(creation_ram, &create_context, 0); });
        }
        create_context.r[4].low = 0x300;
        for (const auto [offset, invalid, original] : std::array<std::array<std::uint32_t, 3>, 8>{
                {{4, 0, 0x1000}, {4, 0x1001, 0x1000}, {8, 0x10001, 0x10000},
                 {8, 0x1FFFF00, 0x10000}, {8, 0, 0x10000}, {12, 0x2A0, 0x400}, {20, 128, 0}, {16, 0x80002000, 0x2000}}}) {
            descriptor(offset, invalid);
            reject([&] { creation.handleSyscall(creation_ram, &create_context, 0); });
            descriptor(offset, original);
        }
        require(creation.threads.empty() && create_context.r[2].low == 0xCAFE);
        const std::vector<std::uint8_t> before_creation(creation.memory().begin(), creation.memory().end());
        const auto before_words = creation.word_writes;
        creation.handleSyscall(creation_ram, &create_context, 0);
        require(creation.threads.size() == 1 && create_context.r[2].low == 2
                && create_context.r[2].high == UINT64_MAX && creation.word_writes == before_words + 4);
        const auto& created = creation.threads[0];
        require(created.id == 2 && created.entry == 0x1000 && created.stack_base == 0x10000
                && created.stack_size == 0x400 && created.gp == 0x2000 && created.initial_priority == 0
                && created.heap_base == 0x4000 && created.context_address == 0x10160 && created.initial_sp == 0x103E0);
        auto expected_creation = before_creation;
        for (const auto [index, value] : std::array<std::array<std::uint32_t, 2>, 4>{
                {{28, 0x2000}, {29, 0x103E0}, {30, 0x103E0}, {31, 0x100250}}}) { // ra: SetupThread's root.
            std::memcpy(expected_creation.data() + 0x10160 + index * 16, &value, 4);
        }
        require(std::equal(creation.memory().begin(), creation.memory().end(), expected_creation.begin()));
        for (unsigned id = 3; id <= 255; ++id) {
            creation.handleSyscall(creation_ram, &create_context, 0);
            require(create_context.r[2].low == id);
        }
        const auto full_words = creation.word_writes;
        creation.handleSyscall(creation_ram, &create_context, 0);
        require(create_context.r[2].low == UINT64_MAX && create_context.r[2].high == UINT64_MAX
                && creation.threads.size() == 254 && creation.word_writes == full_words);
        EeRuntime identity;
        R5900Context identity_context{};
        identity_context.r[3].low = 0x2F;
        identity_context.r[2] = {0xABCD, UINT64_MAX};
        reject([&] { identity.handleSyscall(nullptr, &identity_context, 0); });
        identity.thread_initialized = true;
        reject([&] { identity.handleSyscall(nullptr, &identity_context, 0); });
        identity.current_thread_id = 1;
        identity.handleSyscall(nullptr, &identity_context, 0);
        require(identity_context.r[2].low == 1 && identity_context.r[2].high == UINT64_MAX);
        reject([&] { identity.handleSyscall(nullptr, &identity_context, 1); });
        R5900Context priority_context{};
        priority_context.r[3].low = 0x29;
        priority_context.r[4].low = priority_context.r[5].low = 1;
        priority_context.r[2] = {0xABCD, UINT64_MAX};
        EeRuntime priority;
        reject([&] { priority.handleSyscall(nullptr, &priority_context, 0); });
        priority.thread_initialized = true;
        priority.current_thread_id = 1;
        priority_context.r[4].low = 2;
        reject([&] { priority.handleSyscall(nullptr, &priority_context, 0); });
        priority_context.r[4].low = 1;
        for (const auto value : {0u, 128u, UINT32_MAX}) {
            priority_context.r[5].low = value;
            reject([&] { priority.handleSyscall(nullptr, &priority_context, 0); });
        }
        require(priority.main_thread_priority == 0 && priority.priority_changes == 0 && priority_context.r[2].low == 0xABCD);
        priority_context.r[5].low = 1;
        reject([&] { priority.handleSyscall(nullptr, &priority_context, 1); });
        priority.handleSyscall(nullptr, &priority_context, 0);
        require(priority_context.r[2].low == 0 && priority_context.r[2].high == UINT64_MAX && priority.main_thread_priority == 1);
        priority.handleSyscall(nullptr, &priority_context, 0);
        require(priority_context.r[2].low == 1 && priority.priority_changes == 2);
        EeRuntime osd;
        osd.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        R5900Context osd_context{};
        osd_context.r[3].low = 0x4B;
        osd_context.r[4].low = 0x100;
        osd_context.r[2] = {0xABCD, UINT64_MAX};
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 0); });
        osd.supplyOsdConfig(0x21C12010);
        reject([&] { osd.supplyOsdConfig(0); });
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 1); });
        reject([&] { osd.handleSyscall(nullptr, &osd_context, 0); });
        osd_context.r[4].low = 0x101;
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 0); });
        osd_context.r[4].low = EeRuntime::ram_size;
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 0); });
        osd_context.r[4].low = UINT64_MAX;
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 0); });
        require(osd.osd_config_reads == 0 && osd.word_writes == 0);
        osd_context.r[4].low = 0x100;
        osd.memory()[0xFF] = osd.memory()[0x104] = 0xA5;
        osd.handleSyscall(osd.memory().data(), &osd_context, 0);
        require(osd.read32(osd.memory().data(), 0x100) == 0x21C12010 && osd.osd_config_reads == 1
                && osd.word_writes == 1 && osd_context.r[2].low == 0xABCD && osd_context.r[2].high == UINT64_MAX
                && osd.memory()[0xFF] == 0xA5 && osd.memory()[0x104] == 0xA5);
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 0); });
        EeRuntime osd_sequence;
        osd_sequence.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        R5900Context sequence_context{};
        sequence_context.r[3].low = 0x4B;
        sequence_context.r[4].low = 0x100;
        sequence_context.r[2] = {0xABCD, UINT64_MAX};
        reject([&] { osd_sequence.supplyOsdReadback(0x21C12010); });
        osd_sequence.supplyOsdConfig(0x21C12010);
        reject([&] { osd_sequence.supplyOsdReadback(0); });
        osd_sequence.supplyOsdReadback(0x21C12010);
        reject([&] { osd_sequence.supplyOsdReadback(0x21C12010); });
        sequence_context.r[3].low = 0x4A;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[3].low = 0x4B;
        osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0);
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[3].low = 0x4A;
        sequence_context.r[4].low = 0x104;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[4].low = EeRuntime::ram_size;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[4].low = UINT64_MAX;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[4].low = 0x101;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[4].low = 0x100;
        reject([&] { osd_sequence.handleSyscall(nullptr, &sequence_context, 0); });
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 1); });
        require(osd_sequence.osd_config_sets == 0 && osd_sequence.osd_config_reads == 1);
        osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0);
        require(osd_sequence.osd_config_sets == 1 && osd_sequence.word_writes == 1);
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[3].low = 0x4B;
        sequence_context.r[4].low = 0x104;
        osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0);
        require(osd_sequence.read32(osd_sequence.memory().data(), 0x104) == 0x21C12010
                && osd_sequence.osd_config_reads == 2 && osd_sequence.word_writes == 2
                && sequence_context.r[2].low == 0xABCD && sequence_context.r[2].high == UINT64_MAX);
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[3].low = 0x4A;
        osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0);
        require(osd_sequence.osd_config_sets == 2 && osd_sequence.word_writes == 2);
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[3].low = 0x4B;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        EeRuntime second_overlay;
        second_overlay.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        const std::vector<std::uint8_t> payload(0x330, 0xC3);
        reject([&] { second_overlay.enableStartupKernelPayload(payload); });
        second_overlay.loadKernelSnapshot(baseline);
        second_overlay.enableKernelPatch(0x80076000, 8);
        reject([&] { second_overlay.enableStartupKernelPayload(std::span<const std::uint8_t>(payload).first(4)); });
        second_overlay.enableStartupKernelPayload(payload);
        reject([&] { second_overlay.enableStartupKernelPayload(payload); });
        auto* overlay_ram = second_overlay.memory().data();
        reject([&] { second_overlay.write32(nullptr, 0x80075000, 0xC3C3C3C3); });
        reject([&] { second_overlay.write32(overlay_ram, 0x80075002, 0xC3C3C3C3); });
        reject([&] { second_overlay.write32(overlay_ram, 0x80074FFC, 0xC3C3C3C3); });
        reject([&] { second_overlay.write32(overlay_ram, 0x80075330, 0xC3C3C3C3); });
        reject([&] { second_overlay.write32(overlay_ram, 0xA0075000, 0xC3C3C3C3); });
        reject([&] { second_overlay.write32(overlay_ram, 0x80075000, 0); });
        R5900Context overlay_registration{};
        overlay_registration.r[3].low = 0x74;
        overlay_registration.r[4].low = 0x5B;
        overlay_registration.r[5].low = 0x80075000;
        reject([&] { second_overlay.handleSyscall(overlay_ram, &overlay_registration, 0); });
        second_overlay.write32(overlay_ram, 0x80075000, 0xC3C3C3C3);
        reject([&] { second_overlay.write32(overlay_ram, 0x80075000, 0xC3C3C3C3); });
        reject([&] { second_overlay.handleSyscall(overlay_ram, &overlay_registration, 0); });
        for (std::uint32_t offset = 4; offset < 0x330; offset += 4)
            second_overlay.write32(overlay_ram, 0x80075000 + offset, 0xC3C3C3C3);
        second_overlay.handleSyscall(overlay_ram, &overlay_registration, 0);
        require(second_overlay.syscall_handlers[0x5B] == 0x80075000
                && second_overlay.startup_kernel_word_writes == 204 && second_overlay.kernel_word_writes == 204
                && second_overlay.word_writes == 204 && second_overlay.read32(overlay_ram, 0x8007532C) == 0xC3C3C3C3
                && second_overlay.read32(overlay_ram, 0x80075330) == 0x5A5A5A5A
                && std::equal(baseline.begin(), baseline.end(), second_overlay.kernelBaseline().begin()));
        overlay_registration.r[5].low = 0x80075004;
        reject([&] { second_overlay.handleSyscall(overlay_ram, &overlay_registration, 0); });
        reject([&] { second_overlay.registerKernelFunction(0x80075000, return_leaf, std::span<const std::uint8_t>(payload).first(4)); });
        auto changed_payload = payload;
        changed_payload.back() ^= 1;
        reject([&] { second_overlay.registerKernelFunction(0x80075000, return_leaf, changed_payload); });
        reject([&] { second_overlay.registerKernelFunction(0x80075004, return_leaf, payload); });
        second_overlay.registerKernelFunction(0x80075000, return_leaf, payload);
        reject([&] { second_overlay.registerKernelFunction(0x80075000, return_leaf, payload); });
        second_overlay.expected_return = 0xFFFFFFFC;
        R5900Context bound_context{};
        bound_context.r[31].low = second_overlay.expected_return;
        second_overlay.callFunction(0x80075000, bound_context);
        require(second_overlay.kernel_dispatch_validations == 1 && second_overlay.returns == 1);
        EeRuntime incomplete_binding;
        incomplete_binding.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        incomplete_binding.loadKernelSnapshot(baseline);
        incomplete_binding.enableKernelPatch(0x80076000, 8);
        incomplete_binding.enableStartupKernelPayload(payload);
        incomplete_binding.registerKernelFunction(0x80075000, return_leaf, payload);
        incomplete_binding.write32(incomplete_binding.memory().data(), 0x80075000, 0xC3C3C3C3);
        reject([&] { incomplete_binding.callFunction(0x80075000, bound_context); });
        require(incomplete_binding.kernel_dispatch_validations == 0 && incomplete_binding.guest_calls == 0);
        EeRuntime table_registration;
        table_registration.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        table_registration.loadKernelSnapshot(baseline);
        table_registration.enableKernelPatch(0x80076000, 8);
        auto table_payload = payload;
        const std::array<std::uint32_t, 4> test_pairs{0x55, 0x80075038, 0x56, 0x800750C8};
        std::memcpy(table_payload.data() + 0x300, test_pairs.data(), sizeof(test_pairs));
        table_registration.enableStartupKernelPayload(table_payload);
        R5900Context table_context{};
        table_context.r[3].low = 0x74;
        table_context.r[4].low = 0x55;
        table_context.r[5].low = 0x80075038;
        table_context.r[2] = {0x1234, UINT64_MAX};
        auto* table_ram = table_registration.memory().data();
        reject([&] { table_registration.handleSyscall(table_ram, &table_context, 0); });
        for (std::uint32_t offset = 0; offset < 0x330; offset += 4) {
            std::uint32_t value;
            std::memcpy(&value, table_payload.data() + offset, 4);
            table_registration.write32(table_ram, 0x80075000 + offset, value);
        }
        table_registration.handleSyscall(table_ram, &table_context, 0);
        require(table_registration.syscall_handlers[0x55] == 0x80075038
                && table_context.r[2].low == 0x1234 && table_context.r[2].high == UINT64_MAX);
        table_context.r[4].low = 0x57;
        reject([&] { table_registration.handleSyscall(table_ram, &table_context, 0); });
        table_context.r[4].low = 0x55;
        table_context.r[5].low = 0x8007503C;
        reject([&] { table_registration.handleSyscall(table_ram, &table_context, 0); });
        table_context.r[5].low = 0x80075330;
        reject([&] { table_registration.handleSyscall(table_ram, &table_context, 0); });
        table_context.r[3].low = 0x55;
        reject([&] { table_registration.handleSyscall(table_ram, &table_context, 0); });
        require(table_registration.custom_syscall_calls == 0 && table_registration.syscall_registrations == 1);
        EeRuntime poll;
        poll.semaphores.push_back({1, 1, 0});
        R5900Context poll_context{};
        poll_context.r[3].low = 0x45; poll_context.r[4].low = 0;
        poll_context.r[2].high = 0x12345678;
        poll.handleSyscall(nullptr, &poll_context, 0);
        require(poll_context.r[2].low == 0 && poll.semaphores[0].count == 0);
        poll.handleSyscall(nullptr, &poll_context, 0);
        require(poll_context.r[2].low == UINT64_MAX && poll.semaphores[0].count == 0);
        poll_context.r[4].low = 99;
        poll.handleSyscall(nullptr, &poll_context, 0);
        require(poll_context.r[2].low == UINT64_MAX && poll_context.r[2].high == 0x12345678
                && poll.semaphore_polls == 3 && poll.semaphore_waits == 0 && poll.semaphore_signals == 0);
        EeRuntime cache;
        R5900Context cache_context{};
        cache_context.r[3].low = 0x64;
        cache_context.r[2] = {0x12345678, 0xABCDEF};
        cache_context.pc = 0x204;
        for (const auto operation : {0u, 2u}) {
            cache_context.r[4].low = operation;
            cache.handleSyscall(nullptr, &cache_context, 0);
            require(cache_context.r[2].low == 0x12345678 && cache_context.r[2].high == 0xABCDEF
                    && cache_context.pc == 0x204);
        }
        require(cache.cache_requests == 2 && cache.instruction_cache_invalidations == 1);
        for (const auto operation : {1u, 3u, UINT32_MAX}) {
            cache_context.r[4].low = operation;
            reject([&] { cache.handleSyscall(nullptr, &cache_context, 0); });
        }
        cache_context.r[4].low = 2;
        reject([&] { cache.handleSyscall(nullptr, &cache_context, 1); });
        require(cache.cache_requests == 2 && cache.instruction_cache_invalidations == 1);
        EeRuntime shutdown;
        R5900Context shutdown_context{};
        shutdown_context.r[3].low = 0x17; shutdown_context.r[4].low = 5;
        shutdown_context.r[2].high = 0x1122334455667788ULL;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        shutdown.scmd_mmode_calls = 1;
        shutdown.dmac_handlers.push_back({5, 0x331DF0, 0, 0x38BDF0});
        shutdown.dmac_handler_order.push_back(1); shutdown.dmac_mask = 0xA00000;
        shutdown_context.r[4].low = 4;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.dmac_mask == 0xA00000 && shutdown.dmac_disable_calls == 0);
        shutdown_context.r[4].low = 5;
        shutdown.handleSyscall(nullptr, &shutdown_context, 0);
        require(shutdown.dmac_mask == 0x800000 && shutdown_context.r[2].low == 1
                && shutdown_context.r[2].high == 0x1122334455667788ULL);
        shutdown_context.r[3].low = 0x13; shutdown_context.r[5].low = 2;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.dmac_handlers.size() == 1 && shutdown.dmac_removals == 0);
        shutdown_context.r[5].low = 1;
        shutdown.handleSyscall(nullptr, &shutdown_context, 0);
        require(shutdown.dmac_handlers.empty() && shutdown.dmac_handler_order.empty()
                && shutdown.dmac_removals == 1 && shutdown_context.r[2].low == 0
                && shutdown_context.r[2].high == 0x1122334455667788ULL);
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        shutdown_context.r[3].low = 0x6B; shutdown_context.r[4].low = 1;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.sif_stop_calls == 0);
        shutdown_context.r[3].low = 0x79; shutdown_context.r[4].low = 4;
        shutdown_context.r[5].low = 0x40000; shutdown.sif_stop_calls = 1;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.reset_smflag_writes == 0);
        shutdown_context.r[3].low = 0x7A; shutdown_context.r[4].low = 0x80000000;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.sif_reset_subaddr_reads == 0);
        shutdown_context.r[3].low = 0x77;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.iop_epoch == 0 && !shutdown.iop_reset_pending && shutdown.prior_iop_services_valid);
        shutdown.iop_reset_pending = true;
        shutdown_context.r[3].low = 0x79; shutdown_context.r[4].low = 4;
        shutdown_context.r[5].low = 0x10000;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.reset_clear_writes == 0);
        EeRuntime zero_checkpoint;
        reject([&] { zero_checkpoint.expectStartupZeroRegion(16, 32); });
        zero_checkpoint.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size, 0xA5));
        reject([&] { zero_checkpoint.expectStartupZeroRegion(16, 16); });
        zero_checkpoint.expectStartupZeroRegion(16, 32);
        reject([&] { zero_checkpoint.expectStartupZeroRegion(16, 32); });
        R5900Context zero_context{};
        zero_context.r[3].low = 0x3C;
        zero_context.r[5].low = UINT32_MAX;
        zero_context.r[6].low = 0x100000;
        zero_context.r[7].low = 0x100;
        auto* zero_ram = zero_checkpoint.memory().data();
        reject([&] { zero_checkpoint.handleSyscall(zero_ram, &zero_context, 0); });
        require(!zero_checkpoint.startup_zero_verified && !zero_checkpoint.thread_initialized);
        for (std::uint32_t offset = 16; offset < 32; offset += 4) zero_checkpoint.write32(zero_ram, offset, 0);
        zero_checkpoint.handleSyscall(zero_ram, &zero_context, 0);
        require(zero_checkpoint.startup_zero_verified && zero_checkpoint.thread_initialized);
        const auto lanes = std::bit_cast<std::array<std::uint32_t, 4>>(
            PS2_PCPYLD({0x1111111122222222ULL, UINT64_MAX}, {0x3333333344444444ULL, UINT64_MAX}));
        require(lanes == std::array<std::uint32_t, 4>{0x44444444, 0x33333333, 0x22222222, 0x11111111});
        R5900Context signed_context{};
        signed_context.r[0] = {UINT64_MAX, UINT64_MAX};
        signed_context.r[1] = {0xABCDEF0080000000ULL, UINT64_MAX};
        require(GPR_S32(&signed_context, 0) == 0 && GPR_S32(&signed_context, 1) == INT32_MIN);
        signed_context.r[1].low = UINT64_MAX;
        require(GPR_S32(&signed_context, 1) == -1);
        signed_context.r[1].low = 0xABCDEF007FFFFFFFULL;
        require(GPR_S32(&signed_context, 1) == INT32_MAX);
        require(SLL32(1, 31) == 0x80000000 && SLL32(0x80000000, 1) == 0
                && SLL32(1, 32) == 1 && SLL32(0xFFFFFFFF, 4) == 0xFFFFFFF0);
        require(SRL32(0x740, 2) == 0x1D0);
        require(SRL32(0x80000000, 1) == 0x40000000);
        require(SRL32(0xFFFFFFFF, 31) == 1 && SRL32(0xFFFFFFFF, 32) == 0xFFFFFFFF);
        require(SRA32(0x80000000, 31) == UINT32_MAX && SRA32(0x80000000, 0) == 0x80000000
                && SRA32(UINT32_MAX, 32) == UINT32_MAX && SRA32(0x7FFFFFFF, 1) == 0x3FFFFFFF);
        {
            // R5900 COP1: round toward zero, overflow clamps to +-max, denormals flush, CVT.W saturates.
            const auto bits=[](float v){return std::bit_cast<std::uint32_t>(v);};
            const float max=std::bit_cast<float>(0x7F7FFFFFu),third=std::bit_cast<float>(0x3EAAAAABu);
            require(FPU_ADD_S(0, 0) == 0 && FPU_ADD_S(1.5f,2.25f)==3.75f);
            require(bits(FPU_MUL_S(third,3.0f))==0x3F800000u && bits(FPU_ADD_S(1.0f,std::bit_cast<float>(0x33C00000u)))==0x3F800000u); // Nearest would give 0x3F800001.
            require(bits(FPU_ADD_S(1.0f,-std::bit_cast<float>(0x33000000u)))==0x3F7FFFFFu); // 1 - 2^-25 chops.
            require(bits(FPU_MUL_S(max,2.0f))==0x7F7FFFFFu && bits(FPU_MUL_S(-max,2.0f))==0xFF7FFFFFu);
            require(bits(FPU_MUL_S(std::bit_cast<float>(0x00800000u),0.5f))==0u);  // Denormal result flushes.
            require(bits(FPU_ADD_S(std::bit_cast<float>(0x7F800000u),1.0f))==0x7F7FFFFFu); // Inf operand clamps.
            require(bits(FPU_CVT_S_W(16777217))==0x4B800000u);              // 2^24+1 chops to 2^24.
            require(bits(FPU_SUB_S(1.0f,std::bit_cast<float>(0x33C00000u)))==0x3F7FFFFEu && FPU_SQRT_S(-4.0f)==2.0f
                && bits(FPU_SQRT_S(2.0f))==0x3FB504F3u && bits(FPU_NEG_S(0.0f))==0x80000000u && FPU_ABS_S(-3.5f)==3.5f
                && FPU_C_OLT_S(-1.0f,0.0f) && !FPU_C_OLT_S(std::bit_cast<float>(1u),0.0f) && FPU_C_OLE_S(0.0f,-0.0f));
            require(FPU_CVT_W_S(-2.75f)==-2 && FPU_CVT_W_S(2.75f)==2 && FPU_CVT_W_S(3.0e9f)==INT32_MAX
                && FPU_CVT_W_S(-3.0e9f)==INT32_MIN && FPU_CVT_W_S(std::bit_cast<float>(0x80000001u))==0);
        }
        EeRuntime start_boundary;R5900Context start_context{};
        start_boundary.registerFunction(0x32FDA0,return_leaf);start_boundary.sound_rpc_calls=1;
        start_context.r[2].low=0x1234;
        reject([&]{start_boundary.dispatchGuestBranch(nullptr,&start_context,0x32FDA0,0x315210,0x315218,
            EeRuntime::GuestBranchKind::DirectCall,"checked StartThread");});
        require(start_boundary.guest_calls==0 && start_context.r[2].low==0x1234);
        {
            EeRuntime rt;rt.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
            const std::array<std::uint32_t,14> data={0x70030C11,3,0x10,0x315380,0x3A5CC0,0x1000,0x38BDF0,5,5,0,0,0,0,0};
            const auto evidence=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(data.data()),sizeof(data));
            reject([&]{rt.supplySoundThreadStart(evidence.first(52));});require(!rt.sound_thread_start_supplied);
            rt.supplySoundThreadStart(evidence);reject([&]{rt.supplySoundThreadStart(evidence);});
            rt.thread_initialized=rt.heap_initialized=true;rt.initial_sp=0x1FFED60;rt.current_thread_id=1;rt.main_thread_priority=2;rt.thread_arguments[4]=0x100250;
            rt.threads.push_back({2,0x200,0x1000,0x400,0x300,0,0,0x1160,0x13E0});
            auto* ram=rt.memory().data();const std::array<std::uint32_t,9> descriptor={0,0x315380,0x3A5CC0,0x1000,0x38BDF0,5,0,0,0};
            std::memcpy(ram+0x2000,descriptor.data(),sizeof(descriptor));R5900Context cpu{};cpu.r[3].low=0x20;cpu.r[4].low=0x2000;
            rt.handleSyscall(ram,&cpu,0);require(cpu.r[2].low==3 && !rt.threads[1].ready);
            auto& t=rt.threads[1];require(rt.read32(ram,t.context_address+31*16)==0x100250);
            rt.sound_thread_checkpoint=true;cpu.pc=0x32EB68;cpu.cop0_status=1;cpu.r[3].low=0x30;cpu.r[4].low=3;cpu.r[5].low=rt.initial_sp-256;
            const std::vector<std::uint8_t> untouched(rt.memory().begin(),rt.memory().end());cpu.r[4].low=9;reject([&]{rt.handleSyscall(ram,&cpu,0);});require(std::equal(rt.memory().begin(),rt.memory().end(),untouched.begin()) && !rt.sound_thread_queries);
            cpu.r[4].low=3;rt.handleSyscall(ram,&cpu,0);require(cpu.r[2].low==0x10 && rt.sound_thread_queries==1);
            // The full descriptor, as the startup capture of this query shows.
            const std::array<std::uint32_t,12> dormant={0x10,0x315380,0x3A5CC0,0x1000,0x38BDF0,5,5,0,0,0,0,0};
            for(unsigned n=0;n<12;++n)require(rt.read32(ram,rt.initial_sp-256+n*4)==dormant[n]);
            cpu.pc=0x32EA88;cpu.cop0_status=0x10001;cpu.r[3].low=0x22;cpu.r[5].low=0;
            rt.handleSyscall(ram,&cpu,0);require(t.ready && t.context.pc==0x315380 && t.context.r[31].low==0x100250 && cpu.r[2].low==3 && rt.current_thread_id==1);
            reject([&]{rt.handleSyscall(ram,&cpu,0);});require(rt.sound_thread_starts==1);
        }
        {
            EeRuntime rt;rt.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));auto* ram=rt.memory().data();
            rt.cri_rpc_calls=37;rt.heap_calls=6;rt.thread_initialized=true;rt.current_thread_id=1;rt.main_thread_priority=2;
            rt.initial_sp=0x1FFED60;rt.stack_base=0x1EFF000;rt.thread_arguments={0x38BDF0,UINT32_MAX,0x100000,0,0x100250};
            rt.threads.push_back({2,0,0,0,0,0,0,0,0});rt.threads.push_back({3,0,0,0,0,0,0,0,0});rt.threads[1].ready=true;rt.semaphores.resize(10);
            R5900Context cpu{};cpu.pc=0x32EB68;cpu.cop0_status=0x10001;cpu.r[3].low=0x30;cpu.r[4].low=1;cpu.r[5].low=cpu.r[29].low=rt.initial_sp-144;cpu.r[31].low=0x2EE9FC;
            std::fill_n(ram+rt.initial_sp-144,48,0xA5);
            const auto prior=rt.memory()[rt.initial_sp-144];cpu.r[5].low+=2;reject([&]{rt.handleSyscall(ram,&cpu,0);});require(rt.memory()[rt.initial_sp-144]==prior && !rt.main_thread_queries);
            cpu.r[5].low-=2;rt.handleSyscall(ram,&cpu,0);
            require(cpu.r[2].low==1 && rt.main_thread_queries==1 && rt.read32(ram,rt.initial_sp-144+24)==2);
            require(rt.read32(ram,rt.initial_sp-144+4)==0xA5A5A5A5 && rt.read32(ram,rt.initial_sp-144+40)==0xA5A5A5A5);
            cpu.pc=0x200000;cpu.r[5].low=rt.initial_sp-400;rt.handleSyscall(ram,&cpu,0); // From anywhere.
            require(cpu.r[2].low==1 && rt.main_thread_queries==2 && rt.read32(ram,rt.initial_sp-400+24)==2);
        }
        {
            EeRuntime rt;rt.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));auto* ram=rt.memory().data();
            rt.cri_rpc_calls=37;rt.heap_calls=6;rt.thread_initialized=rt.heap_initialized=true;rt.main_thread_queries=1;
            rt.current_thread_id=1;rt.main_thread_priority=2;rt.initial_sp=0x1FFED60;rt.thread_arguments[4]=0x100250;
            rt.threads.resize(2);rt.threads[1].id=3;rt.threads[1].ready=true;rt.threads[1].current_priority=5;
            // Independently captured descriptors and helper returns, including the previously omitted IDs 4/5.
            const std::array<std::array<std::uint32_t,6>,4> captured={{{0x2EDE58,0x34BC90,0x800,0x2EE51C,0x2EE564,8},
                {0x2EDF48,0x34D490,0x1000,0x2EE684,0x2EE6A8,16},{0x2EE050,0x34E490,0x1000,0x2EE70C,0x2EE730,18},
                {0x2EE140,0x34F490,0x2000,0x2EE794,0x2EE7B8,25}}};
            try { for(unsigned n=0;n<4;++n) {
                const auto& c=captured[n];const auto id=n+4;
                const std::array<std::uint32_t,9> descriptor={0,c[0],c[1],c[2],0x38BDF0,2,0,0,0};
                std::memcpy(ram+0x2000,descriptor.data(),sizeof(descriptor));R5900Context cpu{};
                cpu.pc=0x32EA68;cpu.r[31].low=c[3];cpu.r[3].low=0x20;cpu.r[4].low=0x2000;
                rt.handleSyscall(ram,&cpu,0);auto& t=rt.threads.back();require(t.id==id && !t.ready);
                require(rt.read32(ram,t.context_address+31*16)==0x100250);
                cpu.pc=0x200000+n*8;cpu.cop0_status=1;cpu.r[3].low=0x30;cpu.r[4].low=id;cpu.r[5].low=rt.initial_sp-318;
                const auto before=rt.read32(ram,rt.initial_sp-320);reject([&]{rt.handleSyscall(ram,&cpu,0);});
                require(rt.startup_worker_queries==n && rt.read32(ram,rt.initial_sp-320)==before);
                cpu.r[5].low=rt.initial_sp-320;rt.handleSyscall(ram,&cpu,0);require(cpu.r[2].low==0x10 && rt.startup_worker_queries==n+1);
                require(rt.read32(ram,rt.initial_sp-320+4)==c[0] && rt.read32(ram,rt.initial_sp-320+24)==2);
                cpu.pc=0x210000+n*8;cpu.cop0_status=0x10001;cpu.r[3].low=0x22;cpu.r[5].low=0x55;
                rt.handleSyscall(ram,&cpu,0);
                require(t.ready && t.context.pc==c[0] && t.context.r[29].low==c[1]+c[2]-32 && t.context.r[31].low==0x100250
                    && t.context.r[4].low==0x55 && t.context.cop0_status==0x10001 && rt.current_thread_id==1);
                reject([&]{rt.handleSyscall(ram,&cpu,0);});require(rt.startup_worker_starts==n+1);
                if(n==0) {
                    cpu.pc=0x32EB68;cpu.r[31].low=0x2EE494;cpu.r[3].low=0x30;cpu.r[5].low=rt.initial_sp-288;
                    rt.handleSyscall(ram,&cpu,0);require(cpu.r[2].low==2 && rt.startup_ready_queries==1);
                    cpu.pc=0x32EBD8;cpu.r[31].low=0x2EE4B4;cpu.r[3].low=0x37;
                    rt.handleSyscall(ram,&cpu,0);require(cpu.r[2].low==4 && rt.startup_worker_suspends==1);
                    reject([&]{rt.handleSyscall(ram,&cpu,0);});require(rt.startup_worker_suspends==1 && !t.ready && t.suspended);
                }
                cpu.pc=0x220000;cpu.r[31].low=c[4];cpu.r[3].low=0x29;cpu.r[5].low=128;
                reject([&]{rt.handleSyscall(ram,&cpu,0);});require(t.current_priority==2 && rt.startup_worker_priority_changes==n);
                cpu.r[5].low=c[5];rt.handleSyscall(ram,&cpu,0);require(cpu.r[2].low==2 && t.current_priority==c[5] && rt.startup_worker_priority_changes==n+1);
                if(n==3) {
                    cpu.pc=0x32EB68;cpu.r[31].low=0x2EE494;cpu.r[3].low=0x30;cpu.r[5].low=rt.initial_sp-288;
                    rt.handleSyscall(ram,&cpu,0);require(cpu.r[2].low==2 && rt.read32(ram,rt.initial_sp-288+24)==25);
                    cpu.pc=0x32EBD8;cpu.r[31].low=0x2EE4B4;cpu.r[3].low=0x37;
                    rt.handleSyscall(ram,&cpu,0);require(cpu.r[2].low==7 && t.suspended && !t.ready);
                    reject([&]{rt.handleSyscall(ram,&cpu,0);});require(rt.startup_worker_suspends==2);
                }
            }
            } catch(const std::exception&) { std::cerr<<"Worker lifecycle counters: "<<rt.startup_worker_queries<<"/"<<rt.startup_worker_starts<<"/"<<rt.startup_worker_priority_changes<<"\n";throw; }
            require(rt.startup_worker_queries==4 && rt.startup_worker_starts==4 && rt.startup_worker_priority_changes==4);
        }
        EeRuntime heap_evidence_runtime;
        const std::array<std::uint32_t,16> heap_reply={0,0,0x80000008,0,0,0x203A97C0,0,0x3AC840,0x80000009,0x48438,0x48480,0,0,0,0,0};
        auto heap_bytes=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(heap_reply.data()),sizeof(heap_reply));
        reject([&] {heap_evidence_runtime.supplyHeapBind(heap_bytes.first(60));});
        auto wrong_heap=heap_reply;wrong_heap[9]+=4;
        reject([&] {heap_evidence_runtime.supplyHeapBind(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(wrong_heap.data()),sizeof(wrong_heap)));});
        require(!heap_evidence_runtime.heap_bind_supplied && !heap_evidence_runtime.heap_binds);
        heap_evidence_runtime.supplyHeapBind(heap_bytes);
        require(heap_evidence_runtime.heap_bind_supplied && !heap_evidence_runtime.heap_binds);
        reject([&] {heap_evidence_runtime.supplyHeapBind(heap_bytes);});
        EeRuntime heap_allocation_runtime;
        const std::array<std::uint32_t,17> allocation_evidence={0x400,0x3AC880,0x80000008,0,0,0,0,0x3AC840,0x8000000A,0,0,0,0,0,0,0,0xCE500};
        auto allocation_bytes=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(allocation_evidence.data()),sizeof(allocation_evidence));
        reject([&]{heap_allocation_runtime.supplyHeapAllocation(allocation_bytes.first(64));});
        auto wrong_allocation=allocation_evidence;wrong_allocation[16]+=4;
        reject([&]{heap_allocation_runtime.supplyHeapAllocation(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(wrong_allocation.data()),sizeof(wrong_allocation)));});
        require(!heap_allocation_runtime.heap_allocation_supplied && !heap_allocation_runtime.heap_copy_supplied);
        heap_allocation_runtime.supplyHeapAllocation(allocation_bytes);
        require(heap_allocation_runtime.heap_allocation_supplied && !heap_allocation_runtime.heap_copy_supplied);
        reject([&]{heap_allocation_runtime.supplyHeapAllocation(allocation_bytes);});
        EeRuntime dmac;dmac.loadMemory(std::vector<std::uint8_t>(32*1024*1024));
        R5900Context dc;dc.r[29].low=dmac.initial_sp-32;dc.pc=0x28B2B4;
        reject([&]{dmac.Store32(dmac.memory().data(),&dc,0x1000E000,1);});
        dmac.heap_frees=1;dmac.native_module_loads=14;
        dmac.Store32(dmac.memory().data(),&dc,0x1000E000,1);dc.pc=0x28B2B8;
        reject([&]{dmac.Store32(dmac.memory().data(),&dc,0x1000E020,0);});
        dmac.Store32(dmac.memory().data(),&dc,0x1000E020,0x83FF0002);dmac.dmac_mask=0xA00000;
        dmac.write32(dmac.memory().data(),0x1000E010,0);
        require(dmac.dmac_startup_writes==3 && dmac.dmac_control==1 && dmac.dmac_priority==0x83FF0002 && dmac.dmac_mask==0xA00000);
        reject([&]{dmac.write32(dmac.memory().data(),0x1000E010,0);});
        VifStartup stream;stream.consume(std::array<std::uint32_t,2>{0x03000080,0x02000020});
        require(stream.base==128 && stream.offset==32 && stream.tops==128 && !stream.double_buffer);
        stream.consume(std::array<std::uint32_t,1>{0x20000000});require(stream.pending_mask);
        stream.consume(std::array<std::uint32_t,2>{0xFFFFFFFF,0x01000804});
        require(!stream.pending_mask && stream.mask==UINT32_MAX && stream.cycle==0x804 && stream.commands==4 && stream.words==5);
        const auto before=stream;
        reject([&]{stream.consume(std::array<std::uint32_t,2>{0x05000001,0x14000000});});
        require(stream.mode==before.mode && stream.commands==before.commands && stream.words==before.words);
        reject([&]{stream.consume(std::array<std::uint32_t,1>{0x81000404});});
        reject([&]{stream.consume(std::array<std::uint32_t,1>{0x01010404});});
        EeRuntime disc_alias;disc_alias.loadMemory(std::vector<std::uint8_t>(32*1024*1024));
        auto* disc_ram=disc_alias.memory().data();disc_alias.write64(disc_ram,0x3A8600,0x123456789ABCDEF0);
        // The uncached alias at every width follows the word/byte rule: only after RPCINIT; owned RAM is
        // coherent, so the alias reads and writes the same bytes.
        reject([&]{disc_alias.read64(disc_ram,0x203A8600);});disc_alias.sif_rpc_reads=1;
        require(disc_alias.read64(disc_ram,0x203A8600)==0x123456789ABCDEF0);
        disc_alias.write64(disc_ram,0x20BC9880,0x0102030405060708);require(disc_alias.read64(disc_ram,0xBC9880)==0x0102030405060708);
        disc_alias.write16(disc_ram,0x20BC9890,0xBEEF);require(disc_alias.read16(disc_ram,0xBC9890)==0xBEEF && disc_alias.uncached_wide_accesses==3);
        reject([&]{disc_alias.read64(disc_ram,0x203A8601);});
        reject([&]{disc_alias.read64(disc_ram,0x22000000);});
        // Scratchpad (0x70000000, 16 KB) at every width; raw physical addresses past RAM stay refused;
        // dumps still cover main RAM only.
        disc_alias.write32(disc_ram,0x70000010,0xCAFEF00D);require(disc_alias.read32(disc_ram,0x70000010)==0xCAFEF00D);
        disc_alias.write64(disc_ram,0x70003FF8,0x1122334455667788);require(disc_alias.read8(disc_ram,0x70003FFF)==0x11);
        require(disc_alias.read32(disc_ram,0x10)!=0xCAFEF00D && disc_alias.memory().size()==EeRuntime::ram_size);
        reject([&]{disc_alias.read32(disc_ram,0x70004000);});reject([&]{disc_alias.read32(disc_ram,0x70003FFE);});
        reject([&]{disc_alias.read32(disc_ram,0x02000000);});
        // toSPR then fromSPR (normal mode, synchronous): RAM 0x100000 -> SPR 0x100 -> RAM 0x200000.
        disc_alias.gs_display_flips=1;
        disc_alias.write32(disc_ram,0x100000,0x13579BDF);
        disc_alias.ipuWrite(0x1000D410,0x100000);disc_alias.ipuWrite(0x1000D420,2);disc_alias.ipuWrite(0x1000D480,0x100);
        disc_alias.ipuWrite(0x1000D400,0x100);require(disc_alias.read32(disc_ram,0x70000100)==0x13579BDF && disc_alias.scratchpad_transfers==1);
        disc_alias.ipuWrite(0x1000D010,0x200000);disc_alias.ipuWrite(0x1000D020,2);disc_alias.ipuWrite(0x1000D080,0x100);
        disc_alias.ipuWrite(0x1000D000,0x100);require(disc_alias.read32(disc_ram,0x200000)==0x13579BDF);
        std::uint32_t chcr=0;require(disc_alias.ipuRead(0x1000D000,chcr) && !(chcr&0x100));
        disc_alias.ipuWrite(0x1000D020,0x400);disc_alias.ipuWrite(0x1000D080,0x10);
        reject([&]{disc_alias.ipuWrite(0x1000D000,0x100);}); // Would wrap the scratchpad.
        disc_alias.ipuWrite(0x1000D080,0);reject([&]{disc_alias.ipuWrite(0x1000D000,0x104);}); // Chain mode.
        // toSPR source chain: REF to 0x100000 (1 qword), then END with one qword after the tag.
        disc_alias.write64(disc_ram,0x300000,1|(3ull<<28)|(0x100000ull<<32));disc_alias.write64(disc_ram,0x300010,1|(7ull<<28));
        disc_alias.write32(disc_ram,0x300020,0x2468ACE0);
        disc_alias.ipuWrite(0x1000D420,0);disc_alias.ipuWrite(0x1000D430,0x300000);disc_alias.ipuWrite(0x1000D480,0x200);
        disc_alias.ipuWrite(0x1000D400,0x105);
        require(disc_alias.read32(disc_ram,0x70000200)==0x13579BDF && disc_alias.read32(disc_ram,0x70000210)==0x2468ACE0);
        require(disc_alias.ipuRead(0x1000D400,chcr) && !(chcr&0x100) && disc_alias.ipuRead(0x1000D480,chcr) && chcr==0x220);

        EeRuntime sound_dispatch;
        sound_dispatch.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        sound_dispatch.thread_initialized=true;sound_dispatch.current_thread_id=1;sound_dispatch.main_thread_priority=2;
        sound_dispatch.heap_calls=6;sound_dispatch.cri_rpc_calls=37;
        sound_dispatch.startup_worker_starts=4;sound_dispatch.startup_worker_priority_changes=4;sound_dispatch.startup_worker_suspends=2;
        sound_dispatch.threads.resize(6);
        auto& sound_child=sound_dispatch.threads[1];sound_child.id=3;sound_child.ready=true;sound_child.entry=0x315380;sound_child.current_priority=5;
        sound_child.context.cop0_status=0x10001;sound_child.context.r[29].low=0x3A6CA0;sound_child.context.r[28].low=0x38BDF0;sound_child.context.r[31].low=0x100250;
        R5900Context parent;parent.pc=0x32EAF8;parent.r[31].low=0x2EEA34;parent.r[4].low=1;parent.r[5].low=24;
        auto sound_parent_before=parent;
        sound_child.suspended=true;
        reject([&]{sound_dispatch.probeReadySoundThread(sound_dispatch.memory().data(),&parent);});
        require(!sound_dispatch.ee_sound_probe_attempted && sound_dispatch.current_thread_id==1);
        sound_child.suspended=false;
        bool missing_child=false;
        try {sound_dispatch.probeReadySoundThread(sound_dispatch.memory().data(),&parent);}
        catch(const UnsupportedGuestBranch& e){missing_child=e.target==0x315380;}
        require(missing_child && sound_dispatch.ee_sound_probe_attempted && !sound_dispatch.ee_sound_probe_waits
            && sound_dispatch.current_thread_id==1 && sound_dispatch.main_thread_priority==2
            && parent.pc==sound_parent_before.pc && parent.r[31].low==sound_parent_before.r[31].low && parent.r[2].low==sound_parent_before.r[2].low
            && !sound_dispatch.active_ee_stack && !sound_dispatch.completed_functions.contains(0x315380));
        {
            // Thread exit and deletion: a worker ending with ExitDeleteThread is released by the
            // scheduler and its id reused; TerminateThread makes a sleeping worker dormant and unwinds
            // its parked stack; DeleteThread takes only a dormant thread; main cannot exit.
            EeRuntime ex;ex.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));auto* ram=ex.memory().data();
            ex.thread_initialized=ex.heap_initialized=true;ex.current_thread_id=1;ex.main_thread_priority=1;
            ex.main_priority_admissions=1;ex.thread_arguments[4]=0x100250;
            ex.registerFunction(0x100000,exiting_worker);ex.registerFunction(0x100100,sleeping_worker);
            auto call=[&](unsigned selector,std::uint64_t a0,std::uint64_t a1=0) {
                R5900Context c{};c.pc=0x200000;c.r[3].low=selector;c.r[4].low=a0;c.r[5].low=a1;
                ex.handleSyscall(ram,&c,0);return c.r[2].low;
            };
            auto create=[&](std::uint32_t entry,std::uint32_t stack) {
                const std::array<std::uint32_t,9> descriptor={0,entry,stack,0x1000,0x2000,8,0,0,0};
                std::memcpy(ram+0x3000,descriptor.data(),sizeof(descriptor));return call(0x20,0x3000);
            };
            require(create(0x100000,0x10000)==2 && create(0x100100,0x20000)==3);
            require(call(0x22,2)==2 && call(0x22,3)==3 && ex.threads[0].ready && ex.threads[1].ready);
            reject([&]{call(0x21,2);});                                         // Not dormant.
            require(call(0x29,1,24)==1);                                        // Main lowered: both run.
            require(ex.thread_exits==1 && ex.threads[0].deleted && !ex.threads[0].context.pc && ex.threads[1].waiting);
            reject([&]{call(0x30,2,0x4000);});                                  // A deleted id is unknown.
            require(call(0x30,3,0x4000)==4 && ex.read32(ram,0x4000+36)==1);     // Sleeping: wait type 1.
            require(call(0x25,3)==3 && !ex.threads[1].context.pc && !ex.threads[1].waiting && ex.thread_terminations==1);
            require(call(0x30,3,0x4000)==0x10 && call(0x21,3)==3 && ex.threads[1].deleted && ex.thread_deletions==1);
            require(create(0x100100,0x20000)==2 && !ex.threads[0].deleted && ex.threads[0].entry==0x100100); // Lowest free id.
            reject([&]{call(0x23,0);});                                         // Main's exit is not modelled.
            reject([&]{call(0x25,1);});
        }
        {
            // Admitted-main thread semantics: previous-priority returns, status codes, TH_SELF,
            // and fail-closed transitions whenever a strictly higher-priority thread would run.
            EeRuntime sched;sched.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));auto* ram=sched.memory().data();
            sched.thread_initialized=true;sched.current_thread_id=1;sched.main_thread_priority=24;sched.main_priority_admissions=1;
            sched.threads.resize(6);
            for(unsigned n=0;n<6;++n){auto& t=sched.threads[n];t.id=n+2;t.current_priority=n==2?8u:20u;t.context.pc=n?0x100000u:0u;}
            sched.threads[2].suspended=true;                                    // ID 4: started, suspended
            sched.threads[3].waiting=true;sched.threads[3].current_priority=16; // ID 5: sleeping
            auto call=[&](unsigned selector,unsigned a0,unsigned a1) {
                R5900Context c{};c.pc=0x32EAF8;c.r[3].low=selector;c.r[4].low=a0;c.r[5].low=a1;
                sched.handleSyscall(ram,&c,0);return static_cast<unsigned>(c.r[2].low);
            };
            require(call(0x29,0,1)==24 && sched.main_thread_priority==1);
            require(call(0x30,4,0x1000)==8 && sched.read32(ram,0x1000)==8 && call(0x30,5,0x1000)==4 && call(0x30,2,0x1000)==0x10);
            require(call(0x39,4,0)==4 && !sched.threads[2].suspended && sched.threads[2].ready && call(0x30,4,0x1000)==2);
            reject([&]{call(0x39,4,0);});
            {
                // Main lowering itself below ready ID 4 (8) yields, as a worker's change does: ID 4 runs
                // first, and its unregistered entry stops at a checked branch with main restored. A fresh
                // fixture, since the stopped worker keeps its retained stack.
                EeRuntime yield;yield.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
                yield.thread_initialized=true;yield.current_thread_id=1;yield.main_thread_priority=1;yield.main_priority_admissions=1;
                yield.threads.resize(6);
                for(unsigned n=0;n<6;++n){auto& t=yield.threads[n];t.id=n+2;t.current_priority=n==2?8u:20u;t.context.pc=n?0x100000u:0u;}
                yield.threads[2].ready=true;yield.threads[2].entry=0x100000;
                R5900Context c{};c.pc=0x32EAF8;c.r[3].low=0x29;c.r[4].low=1;c.r[5].low=24;
                bool yielded=false;
                try{yield.handleSyscall(yield.memory().data(),&c,0);}catch(const UnsupportedGuestBranch& e){yielded=e.target==0x100000;}
                require(yielded && yield.startup_worker_dispatches==1 && yield.current_thread_id==1 && !yield.active_ee_stack);
            }
            require(call(0x37,4,0)==4 && sched.threads[2].suspended && !sched.threads[2].ready && call(0x29,0,24)==1);
            reject([&]{call(0x30,9,0x1000);});require(call(0x30,1,0x1000)==1 && sched.read32(ram,0x1000+24)==24);reject([&]{call(0x37,1,0);});
            sched.threads[3].waiting=false;sched.threads[3].ready=true;         // Ready ID 5 (16) above main.
            // Resuming ID 4 (8) from main yields: the ready workers run before main continues (ID 4
            // first); its unregistered entry stops at a checked branch with main restored.
            bool yielded=false;
            sched.threads[2].entry=0x100000;
            try{call(0x39,4,0);}catch(const UnsupportedGuestBranch& e){yielded=e.target==0x100000;}
            require(yielded && sched.startup_worker_dispatches==1 && sched.scheduled_resumes==2 && sched.scheduled_suspends==1
                && sched.current_thread_id==1 && !sched.startup_scheduler_active && !sched.active_ee_stack);
            sched.threads[2].suspended=true;sched.threads[2].ready=false;
            sched.threads[3].ready=false;sched.threads[3].waiting=true;     // Re-park ID 5 before VBlank delivery.
            // VBLANK_START registration requires the explicit mask checkpoint; no delivery is modeled.
            reject([&]{call(0x14,2,0);});
            sched.supplyIntcMask(0x1802);sched.registerFunction(0x1006D8,return_leaf); // Enabling delivers the latched VBlank.
            reject([&]{call(0x10,2,0);});                                        // Null handler.
            R5900Context add{};add.pc=0x32E948;add.r[3].low=0x10;add.r[4].low=2;add.r[5].low=0x1006D8;add.r[6].low=UINT64_MAX;add.r[28].low=0x38BDF0;
            sched.handleSyscall(ram,&add,0);
            require(add.r[2].low==1 && sched.intc_handlers.back().address==0x1006D8 && sched.intc_handlers.back().gp==0x38BDF0);
            reject([&]{sched.handleSyscall(ram,&add,0);});                       // Second cause-2 handler is unobserved.
            require(call(0x14,2,0)==1 && sched.vblank_deliveries==1 && call(0x14,2,0)==0 && (sched.intc_mask&4) && call(0x15,2,0)==1 && call(0x15,2,0)==0);
            reject([&]{call(0x14,3,0);});
        }
        {
            // VBLANK_START delivery: chain ABI, interrupt-context wake semantics, banked wakeups
            // and fail-closed thread-context syscalls. Woken ID 5 stays below main's priority here.
            EeRuntime irq;irq.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));auto* ram=irq.memory().data();
            irq.thread_initialized=true;irq.current_thread_id=1;irq.main_thread_priority=24;irq.main_priority_admissions=1;
            irq.threads.resize(6);
            for(unsigned n=0;n<6;++n){auto& t=irq.threads[n];t.id=n+2;t.current_priority=30;t.context.pc=0x100000;}
            irq.threads[3].waiting=true;
            irq.supplyIntcMask(0x1802);irq.registerFunction(0x1006D8,vblank_fixture);
            R5900Context main{};main.pc=0x32E9A8;main.cop0_status=0x10001;main.r[28].low=0x38BDF0;
            reject([&]{irq.deliverVblank(ram,&main);});                    // Cause 2 still masked.
            R5900Context add=main;add.r[3].low=0x10;add.r[4].low=2;add.r[5].low=0x1006D8;add.r[6].low=UINT64_MAX;add.r[7].low=0x1234;
            irq.handleSyscall(ram,&add,0);
            R5900Context enable=main;enable.r[3].low=0x14;enable.r[4].low=2;irq.handleSyscall(ram,&enable,0);
            require(enable.r[2].low==1 && vblank_fixture_calls==1 && irq.vblank_deliveries==1 && !irq.in_interrupt_);
            require(!irq.threads[3].waiting && irq.threads[3].ready && irq.threads[3].wakeups==1 && irq.worker_wakes==2);
            require(main.pc==0x32E9A8 && irq.current_thread_id==1);
            irq.handleSyscall(ram,&enable,0);require(enable.r[2].low==0 && irq.vblank_deliveries==1); // Not newly enabled.
            reject([&]{R5900Context c=main;c.r[3].low=0xFFFFFFCCu;c.r[4].low=5;irq.handleSyscall(ram,&c,0);}); // Thread context.
            EeRuntime bad;bad.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
            bad.thread_initialized=true;bad.current_thread_id=1;bad.main_thread_priority=24;bad.main_priority_admissions=1;
            bad.supplyIntcMask(0x1806);bad.registerFunction(0x1006D8,vblank_bad_fixture);
            R5900Context badadd=add;bad.handleSyscall(bad.memory().data(),&badadd,0);
            reject([&]{bad.deliverVblank(bad.memory().data(),&main);});
            require(!bad.in_interrupt_ && bad.vblank_deliveries==1);
        }
        {
            // Main sleep/wake: an interrupt wakeup before the sleep is banked and returns at once;
            // sleeping with nothing to wake main is refused; a worker sees main WAIT while it
            // sleeps and READY otherwise, and its WakeupThread ends the sleep.
            EeRuntime ms;ms.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));auto* ram=ms.memory().data();
            ms.thread_initialized=true;ms.current_thread_id=1;ms.main_thread_priority=24;ms.main_priority_admissions=1;
            ms.threads.resize(1);ms.threads[0].id=2;ms.threads[0].current_priority=8;ms.threads[0].context.pc=0x100000;ms.threads[0].waiting=true;
            R5900Context wake{};wake.pc=0x1006D8;wake.r[3].low=0xFFFFFFCCu;wake.r[4].low=1;
            ms.in_interrupt_=true;ms.handleSyscall(ram,&wake,0);ms.in_interrupt_=false;
            require(wake.r[2].low==1 && ms.main_wakeups==1 && ms.main_wakes==1);
            R5900Context sleep{};sleep.pc=0x32EB88;sleep.r[3].low=0x32;ms.handleSyscall(ram,&sleep,0);
            require(sleep.r[2].low==1 && !ms.main_wakeups && ms.main_sleeps==1 && !ms.main_sleeping);
            reject([&]{R5900Context again=sleep;ms.handleSyscall(ram,&again,0);}); // VBLANK_START disabled: nothing wakes main.
            require(!ms.main_sleeping && ms.main_sleeps==1 && !ms.main_sleep_fields);
            // Any semaphore is deleted whatever its count; deleting the newest also drops older deleted ids.
            ms.semaphores.push_back({0,1,0x1234});ms.semaphores.push_back({1,1,0});
            R5900Context older{};older.pc=0x32EC78;older.r[3].low=0x41;older.r[4].low=0;
            ms.handleSyscall(ram,&older,0);require(older.r[2].low==0 && ms.semaphores[0].deleted);
            R5900Context newest=older;newest.r[4].low=1;ms.handleSyscall(ram,&newest,0);
            require(newest.r[2].low==1 && ms.semaphores.empty());
            ms.semaphores.clear();
            ms.current_thread_id=2;ms.main_sleeping=true;
            R5900Context query{};query.r[4].low=1;query.r[5].low=0x1000;
            require(ms.handleThreadSyscall(ram,0x30,&query) && query.r[2].low==4 && ms.read32(ram,0x1000)==4);
            R5900Context worker_wake{};worker_wake.r[4].low=1;
            require(ms.handleThreadSyscall(ram,0x33,&worker_wake) && worker_wake.r[2].low==1 && !ms.main_sleeping && ms.main_wakes==2);
            require(ms.handleThreadSyscall(ram,0x30,&query) && query.r[2].low==2);
            ms.main_sleeping=true;ms.threads[0].current_priority=30;          // Waking main above the worker would preempt it.
            reject([&]{R5900Context w=worker_wake;ms.handleThreadSyscall(ram,0x33,&w);});
        }
        {
            // One-shot VSync registration: preserve the void ABI, reject misaligned addresses, accept it
            // from any code before or after the first frame, let a later call replace a pending one,
            // publish checked RAM once, and retain the previous publication on later fields.
            EeRuntime vs;vs.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));auto* ram=vs.memory().data();
            vs.current_thread_id=1;vs.main_thread_priority=24;vs.intc_mask=4;
            R5900Context c{};c.pc=0x123450;c.r[3].low=0x73;c.r[2].low=0x12345678;
            c.r[29].low=0x2000;c.r[4].low=0x1000;c.r[5].low=0x1004;
            reject([&]{vs.handleSyscall(ram,&c,0);});require(!vs.vsync_flag);
            c.r[4].low=0x1010;c.r[5].low=0x1018;vs.handleSyscall(ram,&c,0);
            require(vs.vsync_flag==0x1010 && vs.vsync_csr==0x1018);
            vs.gs_display_flips=1;vs.timer1_stops=1;
            c.r[4].low=0x1000;c.r[5].low=0x1008;vs.handleSyscall(ram,&c,0);
            require(c.r[2].low==0x12345678 && vs.vsync_flag==0x1000 && vs.vsync_csr==0x1008);
            vs.deliverVblank(ram,&c);
            require(vs.read32(ram,0x1000)==1 && vs.read32(ram,0x1008)==0x551B400C && !vs.vsync_flag && vs.frame_vsync_deliveries==1);
            vs.write32(ram,0x1000,7);vs.deliverVblank(ram,&c);
            require(vs.read32(ram,0x1000)==7 && vs.frame_vsync_deliveries==1);
            // In the frame phase no VU microprogram runs: VPU_STAT reads idle from any code or thread, and the COP2
            // condition is false; before the first frame the read stays refused.
            R5900Context stat{};stat.pc=0x123456;
            require(vs.readVuStatus(&stat)==0 && !vs.cop2Condition(&stat));
            vs.current_thread_id=2;require(vs.readVuStatus(&stat)==0 && !vs.cop2Condition(&stat));vs.current_thread_id=1; // Whoever asks.
            {auto early=std::make_unique<EeRuntime>();reject([&]{early->readVuStatus(&stat);});} // Heap: main's frame is already large.
            // Admitted main that keeps calling without an idle point gets a VBLANK after a field's worth
            // of calls (a busy-wait on a flag); each delivery restarts the count.
            vs.main_priority_admissions=1;vs.registerFunction(0x300,return_leaf);
            const auto before=vs.vblank_deliveries;
            R5900Context busy{};busy.pc=0x200;busy.cop0_status=0x10001;
            for(unsigned n=0;n<EeRuntime::main_calls_per_field*2+2;++n) {
                busy.r[31].low=0x204;
                vs.dispatchGuestBranch(ram,&busy,0x300,0x200,0x204,EeRuntime::GuestBranchKind::DirectCall,"busy poll");
            }
            require(vs.vblank_deliveries==before+2 && vs.busy_main_vblanks==2);
            // The same holds for loop iterations without calls (a long computation), but not while
            // interrupts are disabled: the field stays due until they are enabled again.
            for(unsigned n=0;n<=EeRuntime::main_checkpoints_per_field;++n)vs.eeCheckpointDue();
            require(vs.busy_main_vblanks==3);
            busy.cop0_status=0x10000;
            for(unsigned n=0;n<=EeRuntime::main_checkpoints_per_field+5;++n)vs.eeCheckpointDue();
            require(vs.busy_main_vblanks==3);
            busy.cop0_status=0x10001;vs.eeCheckpointDue();
            require(vs.busy_main_vblanks==4);
        }
        {
            // Frame VIF1 chains: NOP-only and DIRECT chains complete (DIRECT data reaches the GS, also
            // when it continues behind the next tag); a code that is not modelled or a DIRECT the GS
            // refuses stops at the channel start with no channel or GS effect and the code/tag recorded.
            // The codes themselves are covered by vu1_tests.cpp.
            EeRuntime vf;vf.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));auto* ram=vf.memory().data();
            auto tag=[&](std::uint32_t at,std::uint64_t lo,std::uint64_t hi){std::memcpy(ram+at,&lo,8);std::memcpy(ram+at+8,&hi,8);};
            tag(0x2000,0x70000000,0);vf.frame_vif1.tadr=0x2000;vf.frameVif1Start(0x145);
            require(vf.frame_vif1.starts==1 && vf.frame_vif1.tags==1 && vf.frame_vif1.tadr==0x2010 && vf.frame_vif1.cis);
            tag(0x2100,0x10000001,0x5000000110000000ull);tag(0x2110,0x8000,0);tag(0x2120,0x70000000,0); // FLUSHE, DIRECT 1: empty GIFtag (EOP).
            vf.frame_vif1.tadr=0x2100;vf.frameVif1Start(0x145);
            require(vf.frame_vif1.starts==2 && vf.frame_vif1.directs==1 && vf.frame_vif1.direct_qwords==1 && vf.gif_dma.gs.transfers==1 && vf.frame_vif1.tadr==0x2130);
            auto refuse=[&](std::uint32_t at,std::uint32_t code) {
                vf.frame_vif1.cis=false;vf.frame_vif1.tadr=at;const auto before=vf.frame_vif1.unsupported;const auto starts=vf.frame_vif1.starts,transfers=vf.gif_dma.gs.transfers;
                bool refused=false;try{vf.frameVif1Start(0x145);}catch(const UnsupportedGuestMemory&){refused=true;}
                require(refused && vf.frame_vif1.starts==starts && vf.frame_vif1.tadr==at && !vf.frame_vif1.cis && vf.gif_dma.gs.transfers==transfers
                    && vf.frame_vif1.blocked_code==code && vf.frame_vif1.unsupported==before+1);
            };
            tag(0x3000,0x10000001,0x0800000000000000ull);tag(0x3010,0,0);tag(0x3020,0x70000000,0);refuse(0x3000,0x08000000); // Undefined VIF command.
            // DIRECT 2 whose second quadword is the next tag's upper half and the first half of its data.
            tag(0x3100,0x10000001,0x5000000200000000ull);tag(0x3110,0x8000,0);tag(0x3120,0x70000001,0);tag(0x3130,0,0);
            vf.frame_vif1.cis=false;vf.frame_vif1.tadr=0x3100;vf.frameVif1Start(0x145);
            require(vf.frame_vif1.starts==3 && vf.frame_vif1.directs==2 && vf.frame_vif1.direct_qwords==3 && vf.gif_dma.gs.transfers==2 && vf.frame_vif1.tadr==0x3140);
            tag(0x3200,0x10000001,0x5000000100000000ull);tag(0x3210,0x1000000000008001ull,0xE);tag(0x3220,0x70000000,0);
            refuse(0x3200,0x50000000); // DIRECT whose PACKED GIF packet claims more data than it holds: refused by the GS.
        }
        {
            // GS readback (the library's store-image sequence): a normal-mode VIF1 packet starts a
            // local->host transfer and raises FINISH; with the FIFO and the bus turned around, a DMA
            // to memory and FIFO reads deliver the rectangle in order.
            EeRuntime rb;rb.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));auto* ram=rb.memory().data();rb.gs_display_flips=1;
            // The GPU renderer supplies the rectangle's pixels (8 x 4 CT32, row by row) through the download hook.
            std::size_t delivered=0;
            rb.gs_download_source=[&delivered](std::span<std::uint8_t> out) {
                for(std::size_t n=0;n<out.size();++n,++delivered) {
                    const std::size_t at=delivered/4;
                    const std::uint32_t word=at<32?0x22000000u|std::uint32_t((at/8)<<8)|std::uint32_t(at%8):0u;
                    out[n]=std::uint8_t(word>>(8*(delivered%4)));
                }
            };
            auto qword=[&](std::uint32_t at,std::uint64_t lo,std::uint64_t hi){std::memcpy(ram+at,&lo,8);std::memcpy(ram+at+8,&hi,8);};
            qword(0x4000,0x1300000006008000ull,0x5000000600000000ull); // MSKPATH3 on, FLUSHA, NOP, DIRECT 6.
            qword(0x4010,0x1000000000008005ull,0xE);qword(0x4020,0x40ull|(1ull<<16),0x50);qword(0x4030,0,0x51);
            qword(0x4040,0x0000000400000008ull,0x52);qword(0x4050,0,0x61);qword(0x4060,1,0x53);
            require(!rb.gsFinishPending() && rb.vif1Status()==0x40);
            rb.frame_vif1.madr=0x4000;rb.frame_vif1.qwc=7;rb.frameVif1Send(0x101);
            require(rb.frame_vif1.sends==1 && !rb.frame_vif1.qwc && rb.frame_vif1.madr==0x4070 && rb.frame_vif1.chcr==1 && rb.vif1.r.mskpath3
                && rb.gsFinishPending() && rb.gif_dma.gs.downloadQwords()==8);
            require(rb.gsReadbackStore64(0x12001000,2) && !rb.gsFinishPending() && !rb.gsReadbackStore64(0x12000000,0));
            // Nothing is delivered before both directions are turned; the GIF channel stays held.
            rb.frame_vif1.madr=0x5000;rb.frame_vif1.qwc=6;reject([&]{rb.frameVif1Receive(0x100);});reject([&]{rb.vif1FifoRead();});
            rb.frame_gif.tadr=0x4010;reject([&]{rb.frameGifChainStart(0x105);});
            reject([&]{rb.gsReadbackStore32(0x10003C00,0x40);});
            require(rb.gsReadbackStore32(0x10003C00,0x800000) && !rb.gsReadbackStore32(0x10003C10,0) && (rb.vif1Status()>>24)==0);
            reject([&]{rb.gsReadbackStore64(0x12001040,2);});
            require(rb.gsReadbackStore64(0x12001040,1) && rb.vif1Status()==(0x40u|0x800000u|(8u<<24)) && rb.gs_readback.downloads==1);
            rb.frameVif1Receive(0x100);
            std::uint32_t first,last;std::memcpy(&first,ram+0x5000,4);std::memcpy(&last,ram+0x505C,4);
            require(first==0x22000000 && last==0x22000207 && rb.frame_vif1.madr==0x5060 && !rb.frame_vif1.qwc && (rb.vif1Status()>>24)==2 && rb.gs_readback.dma_qwords==6);
            const auto a=rb.vif1FifoRead(),b=rb.vif1FifoRead();
            require(std::uint32_t(a.low)==0x22000300 && std::uint32_t(b.high>>32)==0x22000307 && (rb.vif1Status()>>24)==0 && rb.gs_readback.fifo_qwords==2);
            reject([&]{rb.vif1FifoRead();}); // The transfer is exhausted.
            reject([&]{rb.vif1FifoWrite(ProbeGpr{});}); // No VIF codes while the FIFO runs toward the EE.
            require(rb.gsReadbackStore32(0x10003C00,0) && rb.gsReadbackStore64(0x12001040,0) && rb.vif1Status()==0x40);
            ProbeGpr release{};release.low=0x06000000;rb.vif1FifoWrite(release);
            require(!rb.vif1.r.mskpath3);
        }
        std::cout << "Bootstrap register, saturation, memory bounds and unsupported FPU checks passed.\n";
        return 0;
    } catch (const UnsupportedSyscall& s) { std::cerr << "Unsupported syscall 0x" << std::hex << s.selector << " at pc 0x" << s.resume_pc << '\n'; return 1; } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
