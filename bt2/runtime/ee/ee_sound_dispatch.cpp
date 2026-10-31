#include <cstdio>
#include "ee_runtime.h"
#include "ee_execution_owner.h"
#include <utility>
#include <cstdlib>
#include <algorithm>

void EeRuntime::probeReadySoundThread(std::uint8_t* rdram, R5900Context* parent) {
    if(ee_sound_probe_attempted || !thread_initialized || current_thread_id!=1 || main_thread_priority!=2
        || heap_calls!=6 || cri_rpc_calls!=37 || startup_worker_starts!=4 || startup_worker_priority_changes!=4
        || startup_worker_suspends!=2 || parent->pc!=0x32EAF8 || parent->r[31].low!=0x2EEA34
        || parent->r[4].low!=1 || parent->r[5].low!=24 || threads.size()!=6
        || threads[1].id!=3 || !threads[1].ready || threads[1].suspended || threads[1].entry!=0x315380
        || threads[1].current_priority!=5)
        throw UnsupportedSyscall(0x29,parent->pc);
    auto child=threads[1].context;
    if(child.cop0_status!=0x10001) throw UnsupportedSyscall(0x29,parent->pc);
    ee_sound_probe_parent=*parent;
    ee_sound_probe_attempted=true;
    auto parent_returns=std::move(call_returns_);
    const auto parent_depth=dispatch_depth_, parent_expected=expected_return;
    call_returns_.clear();dispatch_depth_=0;expected_return=0x100250;current_thread_id=3;
    auto restore=[&] {
        active_ee_stack=nullptr;current_thread_id=1;expected_return=parent_expected;
        dispatch_depth_=parent_depth;call_returns_=std::move(parent_returns);
    };
    try {
        // The retained stack is cancelled at this diagnostic boundary. No successful
        // SleepThread return, ready-state change or priority acknowledgement is supplied.
        RetainedEeStack stack([&] {
            try { dispatchGuestBranch(rdram,&child,0x315380,parent->pc,0,GuestBranchKind::TailJump,"sound child"); }
            catch (...) {ee_sound_probe_frontier=child;throw;}
            throw std::runtime_error("Unexpected sound child exit");
        });
        active_ee_stack=&stack;
        stack.resume();
        if(stack.state()!=RetainedEeStack::State::Waiting || ee_sound_probe_waits!=1)
            throw std::runtime_error("Sound child did not retain first wait");
        // Destructor resumes only to unwind cancellation, while child return ownership
        // is still selected. Parent invocation guards remain live and unchanged.
    } catch (...) {restore();throw;}
    restore();
    throw UnsupportedSyscall(0x32,ee_sound_probe_frontier.pc);
}

void EeRuntime::queueSoundsBind(R5900Context* context,const std::array<unsigned,16>& reply) {
    if(!ee_queued_sound_rpc || queued_sounds_bind.pending || queued_sound_submissions || sounds_rpc_binds
        || current_thread_id!=1 || main_thread_priority!=2 || iop_epoch!=1 || reply[7]!=0x6351C0 || reply[8]!=0x80000009)
        throw UnsupportedSyscall(0x77,context->pc);
    queued_sounds_bind={true,iop_epoch,reply};++queued_sound_submissions;
    ++sif_dma_submissions;context->r[2].low=0x60003; // Accepted transport submission only.
}
void EeRuntime::finishQueuedSoundsBind(std::uint8_t* rdram,R5900Context* context) {
    if(!ee_queued_sound_rpc || !queued_sounds_bind.pending || queued_sound_completions || queued_sounds_bind.epoch!=iop_epoch
        || current_thread_id!=1 || main_thread_priority!=2 || context->pc!=0x32ECA8 || context->r[4].low!=7
        || context->r[31].low!=0x332874 || threads.size()!=2 || !threads[1].ready || threads[1].entry!=0x315380
        || threads[1].context.cop0_status!=0x10001 || semaphores.size()!=8 || semaphores[7].count
        || semaphores[7].option!=0x3838D8 || read32(rdram,0x3A97D8)!=29 || read32(rdram,0x3A97DC)!=0x6351C0
        || read32(rdram,0x3A97E0)!=0x02000004 || sound_execution_)
        throw UnsupportedSyscall(0x44,context->pc);
    const auto saved=*context;
    sound_dispatch_bind_id=29;sound_dispatch_wait_ra=static_cast<unsigned>(context->r[31].low);
    sound_dispatch_wait_sp=static_cast<unsigned>(context->r[29].low);
    sound_execution_=std::make_shared<EeExecutionOwner>(*this);
    sound_execution_->dispatch();
    if(!sound_thread_waiting || sound_execution_->stack->state()!=RetainedEeStack::State::Waiting
        || std::memcmp(&saved,context,sizeof(saved)) || current_thread_id!=1)
        throw std::runtime_error("Queued bind lost retained main/child ownership");
    const auto reply=queued_sounds_bind.reply;
    for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x6351C0,7,reply[9],reply[10]);
    queued_sounds_bind.pending=false;++queued_sound_completions;++sounds_rpc_binds;
    if(std::memcmp(&saved,context,sizeof(saved))) throw std::runtime_error("Queued completion changed waiting main CPU");
}

bool EeRuntime::handleAdmittedIntcSyscall(std::uint8_t* rdram,std::uint32_t selector,R5900Context* context) {
    // VBLANK_START (cause 2) registration after main's admitted priority, and its replacement
    // during main's display re-initialization (GS reset removes it; the VSync setter re-adds it).
    if(selector!=0x10 && selector!=0x11 && selector!=0x14 && selector!=0x15)return false;
    constexpr std::uint32_t cause=2,bit=1u<<cause;
    if(static_cast<std::uint32_t>(context->r[4].low)!=cause || !intc_mask_supplied_)throw UnsupportedSyscall(selector,context->pc);
    if(selector==0x10) {
        const auto address=static_cast<std::uint32_t>(context->r[5].low);
        const auto next=static_cast<std::int32_t>(context->r[6].low);
        const bool replacement=display_reset_phase==12 && vblank_handler_registrations==1 && address==0x1006D8
            && context->pc==0x32E948 && context->r[31].low==0x311410 && context->r[29].low==initial_sp-96;
        // After a frame-phase GS reset removed the VSync handler, the library re-adds one (once).
        const bool frame_replacement=frame_vsync_handler_removed && display_reset_phase==13 && !frame_gs_reset_step
            && current_thread_id==1 && !in_interrupt_ && context->pc==0x32E948;
        if(!address || address%4 || next!=-1 || (vblank_handler_registrations && !replacement && !frame_replacement)
            || std::any_of(intc_handlers.begin(),intc_handlers.end(),[](const IntcHandler& h){return h.cause==cause;}))
            throw UnsupportedSyscall(selector,context->pc);
        checkMemory(rdram,address,4);
        if(intc_handlers.size()==128){context->r[2].low=UINT64_MAX;return true;}
        // The kernel records the caller GP and argument register; next=-1 appends to the chain tail.
        intc_handlers.push_back({cause,address,static_cast<std::uint32_t>(context->r[7].low),static_cast<std::uint32_t>(context->r[28].low)});
        const auto id=static_cast<std::uint32_t>(intc_handlers.size());intc_handler_order.push_back(id);
        // A removed slot is not reused: the original kernel also returned a fresh ID (4 -> 5).
        if(frame_replacement){frame_vsync_handler_removed=false;frame_vsync_handler=address;}
        ++vblank_handler_registrations;if(replacement)display_reset_phase=13;context->r[2].low=id;return true;
    }
    if(selector==0x11) {
        const auto id=static_cast<std::uint32_t>(context->r[5].low);
        const auto order=std::find(intc_handler_order.begin(),intc_handler_order.end(),id);
        if(frameGsResetStep(context,3,0x32E968)) {
            // Frame-phase reset: the GS library unlinks its VSync handler while cause 2 is disabled.
            if((intc_mask&bit) || order==intc_handler_order.end() || intc_handlers.at(id-1).cause!=cause || context->r[31].low!=0x30FE90)
                throw UnsupportedSyscall(selector,context->pc);
            intc_handler_order.erase(order);intc_handlers.at(id-1).cause=~0u;frame_gs_reset_step=4;frame_vsync_handler_removed=true;
            context->r[2].low=0;return true;
        }
        if(!displayResetStep(context,3,0x32E968,688) || (intc_mask&bit) || order==intc_handler_order.end() || intc_handlers.at(id-1).cause!=cause
            || intc_handlers.at(id-1).address!=0x1006D8 || context->r[31].low!=0x30FE90)throw UnsupportedSyscall(selector,context->pc);
        // Unlinked while cause 2 is disabled; both reached callers ignore the result.
        intc_handler_order.erase(order);intc_handlers.at(id-1).cause=~0u;display_reset_phase=4;context->r[2].low=0;return true;
    }
    if(selector==0x14) {
        const bool newly=!(intc_mask&bit);
        context->r[2].low=newly?1:0;intc_mask|=bit;++vblank_enable_calls;
        // VBLANK_START latches in INTC_STAT while masked. Original evidence places the first handler
        // entry after this enable and before main's next call, i.e. at the wrapper's EI.
        if(newly && !vblank_deliveries)deliverVblank(rdram,context);
        return true;
    }
    context->r[2].low=(intc_mask&bit)?1:0;intc_mask&=~bit;++vblank_disable_calls;return true;
}

void EeRuntime::advanceLogicalField() {
    timer1.field(); // Display time reaches the field boundary (credits Timer 1 HBLANKs while counting).
    // Timer 2 runs from the startup mode write; its BIOS-era COUNT/COMP residue is not observable
    // (initial MODE already had CUE set), so the logical counter starts at COUNT 0, COMP 0.
    if(!timer2.started()) {
        if(!timer2_mode_writes)return; // Timer 2 never programmed: nothing counts.
        if(timer2_mode_writes!=1 || timer2_mode_after_write!=0x382 || !(intc_mask&(1u<<11)))throw std::runtime_error("Timer 2 startup state differs");
        timer2.start(timer2_mode_after_write,0,0);
    }
    timer2.advanceField();
}

void EeRuntime::runIntcHandlers(std::uint8_t* rdram,R5900Context* interrupted,std::uint32_t cause,const char* name) {
    // Registered handlers in chain order with the kernel handler ABI (a0 cause, a1 argument,
    // registered GP) on the captured kernel interrupt stack.
    constexpr std::uint32_t stack=0x81FC0,callback_return=0xFFFFFFF8;
    checkMemory(rdram,stack-0x800,0x800);
    in_interrupt_=true;delivering_cause_=cause;
    try {
        for(const auto id:intc_handler_order) {
            const auto& h=intc_handlers.at(id-1);
            if(h.cause!=cause)continue;
            R5900Context ic{};ic.pc=h.address;ic.r[4].low=cause;ic.r[5].low=h.arg;ic.r[28].low=h.gp;
            ic.r[29].low=stack;ic.r[31].low=callback_return;ic.cop0_status=interrupted->cop0_status&~0x10001u;
            dispatchGuestBranch(rdram,&ic,h.address,interrupted->pc,callback_return,GuestBranchKind::DirectCall,name);
            if(static_cast<std::int32_t>(ic.r[2].low)<0)break; // A negative result ends the chain.
        }
    } catch(...) {in_interrupt_=false;delivering_cause_=0;throw;}
    in_interrupt_=false;delivering_cause_=0;
}

void EeRuntime::deliverVblank(std::uint8_t* rdram,R5900Context* interrupted) {
    constexpr std::uint32_t cause=2;
    if(in_interrupt_ || current_thread_id!=1 || startup_scheduler_active || !(intc_mask&(1u<<cause)))
        throw UnsupportedSyscall(0x14,interrupted->pc);
    if(on_field_end)on_field_end(); // The state at the end of the field that ends here.
    // The field that ends here shows what the game set up: a field with display register writes but
    // no library flip (the per-field update used while a movie plays) presents now.
    const bool unflipped_update=gs_display_flips && gs_display_flips==field_flips_ && gs_display_writes!=field_writes_;
    field_writes_=gs_display_writes;
    if(unflipped_update)countDisplayFlip();
    field_flips_=gs_display_flips;
    advanceLogicalField();
    // A Timer 2 event inside the elapsed field precedes this VBLANK_START. The library handler
    // only services its alarm list (empty here; callbacks are not modelled) and overflow count.
    if(timer2.interruptPending() && (intc_mask&(1u<<11))) {
        if(read32(rdram,0x36BAE8))throw UnsupportedSyscall(0x14,interrupted->pc);
        runIntcHandlers(rdram,interrupted,11,"Timer 2 handler");++timer2_deliveries;
        if(timer2.interruptPending())throw UnsupportedSyscall(0x14,interrupted->pc); // Handler must acknowledge.
    }
    // The field alternates at every VBlank, waited for or not: code that samples it once per field
    // without a pending wait (a film's frame timing) sees it change as on the console.
    display_field=!display_field;gs_csr_cleared=false;
    if(vsync_flag) {
        display_intc_stat|=0xC;
        write32(rdram,vsync_flag,1);write32(rdram,vsync_csr,0x551B400Cu|(display_field?0x2000u:0u));write32(rdram,vsync_csr+4,0);
        vsync_flag=vsync_csr=0;++frame_vsync_deliveries;
    }
    ++vblank_deliveries;main_calls_since_field_=main_checkpoints_since_field_=0;
    // Time advancing without any presented frame is bounded: 3,600 fields (a minute) stop the run.
    if(gs_display_flips!=flipless_flips_) {flipless_flips_=gs_display_flips;flipless_fields_=0;}
    // Counted once frames are presented or main is busy past a field (a run started into a battle loads before its first frame).
    // The stop names where the field interrupted the game and its last call, for finding the wait.
    if((gs_display_flips || busy_main_vblanks) && ++flipless_fields_>flipless_field_limit) {
        char where[160];std::snprintf(where,sizeof where,"; interrupted at pc 0x%X ra 0x%X (thread %u), last call 0x%X -> 0x%X",
            interrupted->pc,static_cast<std::uint32_t>(interrupted->r[31].low),current_thread_id,last_branch_source_,last_branch_target_);
        throw std::runtime_error("Bootstrap execution budget exceeded; no display flip for "+std::to_string(flipless_field_limit)+" fields"+where);
    }
    nativeCriVblank(interrupted); // CRI stream buffers returned for this field; a paced run waits for the field's deadline here.
    nativePadVblank(rdram);       // Pad state for this field, before EE handlers run; live input is read after that wait.
    runIntcHandlers(rdram,interrupted,cause,"VBLANK_START handler");
    runReadyThreads(rdram,interrupted); // Woken threads above the interrupted one run on return.
}

