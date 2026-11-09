#include "ee_runtime.h"
#include "ee_execution_owner.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

// EE thread scheduling: thread syscalls (status, priority, suspend/resume, sleep/wakeup), interrupt-
// context thread calls, and running ready workers on their retained stacks while main is parked.
// Main runs on the host stack; workers run as EeExecutionOwner stacks and park on SleepThread.

void EeRuntime::requireNoPreemption(std::uint32_t selector,const R5900Context* context) const {
    // Equal priorities keep FIFO order; only a strictly higher-priority ready thread preempts.
    // Main is runnable whenever it is not the caller; workers are parked or ready below it.
    const bool main=current_thread_id==1;
    const auto priority=main?main_thread_priority:threads.at(current_thread_id-2).current_priority;
    if(!main && !main_sleeping && main_thread_priority<priority)throw UnsupportedSyscall(selector,context->pc);
    for(const auto& t:threads)
        if(t.id!=current_thread_id && t.ready && !t.suspended && !t.waiting && t.current_priority<priority)
            throw UnsupportedSyscall(selector,context->pc);
}

bool EeRuntime::mainYieldsToReadyWorkers(std::uint8_t* rdram,R5900Context* context) {
    // A syscall from main that readies a worker above main's priority returns only after that worker
    // (and any other ready worker above main) has run until it parks again, as the kernel reschedules
    // on syscall return. Nested scheduling and interrupt context stay refused by the caller's check.
    if(current_thread_id!=1 || in_interrupt_ || startup_scheduler_active)return false;
    if(std::none_of(threads.begin(),threads.end(),[&](const auto& t){return t.ready && !t.suspended && !t.waiting && t.current_priority<main_thread_priority;}))
        return false;
    runReadyThreads(rdram,context);++main_yields;return true;
}

bool EeRuntime::sleepMainThread(std::uint8_t* rdram,R5900Context* context) {
    // Main sleeps until a WakeupThread/iWakeupThread names it (a banked wakeup returns at once).
    // While it sleeps, ready workers run at any priority and the idle CPU receives one logical
    // VBLANK_START per field; handlers and workers run in that order, as in deliverVblank.
    if(in_interrupt_ || startup_scheduler_active)throw UnsupportedSyscall(0x32,context->pc);
    if(main_wakeups){--main_wakeups;++main_sleeps;context->r[2].low=1;return true;}
    main_sleeping=true;
    try {
        runReadyThreads(rdram,context);
        for(unsigned fields=0;main_sleeping;++fields) {
            if(!(intc_mask&(1u<<2)) || fields>=600)throw UnsupportedSyscall(0x32,context->pc); // Nothing would wake main.
            deliverVblank(rdram,context);++main_sleep_fields;
        }
    } catch(...) {main_sleeping=false;throw;}
    ++main_sleeps;context->r[2].low=1;return true; // SleepThread returns the caller ID.
}

void EeRuntime::wakeMainThread() {
    if(main_sleeping)main_sleeping=false;else ++main_wakeups;
    ++main_wakes;
}

void EeRuntime::writeThreadStatus(std::uint8_t* rdram,std::uint32_t id,std::uint32_t out,std::uint32_t pc) {
    // ReferThreadStatus's descriptor: status, entry, stack, stack size, gp, initial and current priority,
    // attr, option, wait type (1: SleepThread), wait id and banked wakeups, as the kernel fills it and the
    // startup captures of the created threads show. Main's record holds only its status and current
    // priority (the other fields of the startup's main thread are not known), so only those are written.
    if(out%4)throw UnsupportedSyscall(0x30,pc);
    checkMemory(rdram,out,48);
    const bool self=id==current_thread_id && !in_interrupt_;
    if(id==1) {
        const std::uint32_t status=self?1u:main_sleeping?4u:2u;
        write32(rdram,out,status);write32(rdram,out+24,main_thread_priority);return;
    }
    const auto& t=threads.at(id-2);
    const std::uint32_t status=self?1u:t.suspended?(t.waiting?0xCu:8u):t.waiting?4u:t.ready?2u:0x10u;
    const std::array<std::uint32_t,12> info={status,t.entry,t.stack_base,t.stack_size,t.gp,t.initial_priority,t.current_priority,
        t.attr,t.option,t.waiting?1u:0u,0,t.wakeups};
    for(unsigned n=0;n<info.size();++n)write32(rdram,out+n*4,info[n]);
}

void EeRuntime::createThread(std::uint8_t* rdram,R5900Context* context) {
    const auto parameter=static_cast<std::uint32_t>(context->r[4].low);
    if(!heap_initialized || parameter%4)throw UnsupportedSyscall(0x20,context->pc);
    checkMemory(rdram,parameter,36);
    const auto entry=read32(rdram,parameter+4),stack=read32(rdram,parameter+8),size=read32(rdram,parameter+12);
    const auto gp=read32(rdram,parameter+16),priority=read32(rdram,parameter+20);
    const auto attr=read32(rdram,parameter+28),option=read32(rdram,parameter+32);
    if(!entry || entry%4 || stack%16 || size%16 || size<0x2C0 || priority>=128)throw UnsupportedSyscall(0x20,context->pc);
    checkMemory(rdram,entry,4);checkMemory(rdram,stack,size);checkMemory(rdram,gp,4);
    const auto top=stack+size,saved_context=top-0x2A0;
    if(parameter<top && saved_context<parameter+36)throw UnsupportedSyscall(0x20,context->pc);
    // The kernel reserves id 1 for main and 256 for idle; a deleted thread's id is free again, the
    // lowest first.
    auto slot=std::find_if(threads.begin(),threads.end(),[](const auto& t){return t.deleted;});
    if(slot==threads.end() && threads.size()==254) {context->r[2].low=UINT64_MAX;return;}
    const auto id=slot==threads.end()?static_cast<std::uint32_t>(threads.size())+2:slot->id;
    const DormantThread created{id,entry,stack,size,gp,priority,heap_start,saved_context,top-0x20};
    if(slot==threads.end())threads.push_back(created);else *slot=created;
    auto& t=threads.at(id-2);t.current_priority=priority;t.attr=attr;t.option=option;
    // The thread's saved context: gp, sp, fp and the return address, which is the root function given to
    // SetupThread (the startup's library passes its thread exit); the other bytes keep what was there.
    write32(rdram,saved_context+28*16,gp);
    write32(rdram,saved_context+29*16,top-0x20);
    write32(rdram,saved_context+30*16,top-0x20);
    write32(rdram,saved_context+31*16,thread_arguments[4]);
    context->r[2].low=id;
}

bool EeRuntime::handleThreadSyscall(std::uint8_t* rdram,std::uint32_t selector,R5900Context* context) {
    // One implementation of the thread calls for main and the workers, from the startup on. Main's
    // admission (its first priority change that lets workers above it run) opens the scheduled part of
    // the run; the startup's counters (before it) and the scheduled ones (after) count the same calls.
    const bool main=current_thread_id==1,startup=!main_priority_admissions;
    if(selector==0x20) {createThread(rdram,context);return true;}
    if(selector==0x2F) {context->r[2].low=current_thread_id;return true;}
    if(main && selector==0x32)return sleepMainThread(rdram,context);
    if(selector==0x23 || selector==0x24)exitThread(selector,context);
    if(selector!=0x21 && selector!=0x22 && selector!=0x25 && selector!=0x29 && selector!=0x30 && selector!=0x33 && selector!=0x37 && selector!=0x39 && (main || selector!=0x32))return false;
    const auto argument=static_cast<std::uint32_t>(context->r[4].low);
    const auto id=selector==0x32||!argument?current_thread_id:argument; // TH_SELF (0) names the caller.
    if(selector==0x30 && (id==1 || (id>=2 && id-2<threads.size() && !threads[id-2].deleted))) {
        const auto out=static_cast<std::uint32_t>(context->r[5].low);
        writeThreadStatus(rdram,id,out,context->pc);
        const auto status=read32(rdram,out);
        if(!startup)++startup_worker_status_queries;
        else if(id==1)++main_thread_queries;
        else if(status==0x10)++(id==3?sound_thread_queries:startup_worker_queries);
        else ++startup_ready_queries;
        context->r[2].low=status;return true;
    }
    if(!main && selector==0x33 && id==1) {
        // A worker wakes main. Main above the worker preempts it: not yet reached.
        if(main_sleeping && main_thread_priority<threads.at(current_thread_id-2).current_priority)throw UnsupportedSyscall(selector,context->pc);
        wakeMainThread();context->r[2].low=1;return true;
    }
    if(selector==0x29 && id==1) {
        // Main may change only its own priority here; its record is held outside threads.
        const auto priority=static_cast<std::uint32_t>(context->r[5].low);
        if(!main || !priority || priority>=128)throw UnsupportedSyscall(selector,context->pc);
        const auto previous=main_thread_priority;main_thread_priority=priority;
        const bool admission=startup && std::any_of(threads.begin(),threads.end(),
            [&](const auto& t){return t.ready && !t.suspended && !t.waiting && t.current_priority<priority;});
        if(admission) {
            // Lowered below ready workers for the first time: they run before main resumes, and the
            // scheduled part of the run begins (workers return to this call's pc).
            startup_dispatch_parent=*context;main_priority_previous=previous;++main_priority_admissions;
            runReadyThreads(rdram,context);
        } else if(!mainYieldsToReadyWorkers(rdram,context))
            try{requireNoPreemption(selector,context);}catch(...){main_thread_priority=previous;throw;}
        context->r[2].low=previous;++(startup?priority_changes:scheduled_priority_changes);return true;
    }
    // Main and unknown IDs are otherwise outside this bounded scope.
    if(id<2 || id-2>=threads.size() || threads[id-2].id!=id || threads[id-2].deleted)throw UnsupportedSyscall(selector,context->pc);
    auto& t=threads[id-2];const bool self=id==current_thread_id;
    if(selector==0x21) {
        // DeleteThread: a dormant thread's record goes and its id is free; another state is refused.
        if(self || t.context.pc)throw UnsupportedSyscall(selector,context->pc);
        t.deleted=true;++thread_deletions;context->r[2].low=id;return true;
    }
    if(selector==0x25) {
        // TerminateThread: another thread becomes dormant wherever it is; its parked stack is released.
        if(self || !t.context.pc)throw UnsupportedSyscall(selector,context->pc);
        releaseThread(t);++thread_terminations;context->r[2].low=id;return true;
    }
    if(selector==0x22) {
        // StartThread: a dormant thread begins at its entry with the saved context CreateThread prepared,
        // the argument in a0 and the caller's interrupt state.
        if(self || t.context.pc || t.ready)throw UnsupportedSyscall(selector,context->pc);
        checkMemory(rdram,t.context_address,512);R5900Context child{};
        for(unsigned n=0;n<32;++n)std::memcpy(&child.r[n],memory_.data()+t.context_address+n*16,16);
        child.r[0]={0,0};child.r[4]=context->r[5];child.pc=t.entry;child.cop0_status=context->cop0_status;
        t.context=child;t.ready=true;
        if(id==3)++sound_thread_starts;else if(startup)++startup_worker_starts;else ++scheduled_starts;
        if(!mainYieldsToReadyWorkers(rdram,context))
            try{requireNoPreemption(selector,context);}catch(...){t.ready=false;t.context=R5900Context{};throw;}
        context->r[2].low=id;return true;
    }
    if(selector==0x29) {
        const auto priority=static_cast<std::uint32_t>(context->r[5].low);
        if(!priority || priority>=128)throw UnsupportedSyscall(selector,context->pc);
        const auto previous=t.current_priority;t.current_priority=priority;
        if(!mainYieldsToReadyWorkers(rdram,context))
            try{requireNoPreemption(selector,context);}catch(...){t.current_priority=previous;throw;}
        context->r[2].low=previous;++(startup?startup_worker_priority_changes:scheduled_priority_changes);return true;
    }
    if(selector==0x37 || selector==0x39) {
        if(self || !t.context.pc || t.suspended!=(selector==0x39))throw UnsupportedSyscall(selector,context->pc);
        if(selector==0x37) {t.suspended=true;t.ready=false;++(startup?startup_worker_suspends:scheduled_suspends);}
        else {
            t.suspended=false;t.ready=!t.waiting;++scheduled_resumes;
            if(!mainYieldsToReadyWorkers(rdram,context))
                try{requireNoPreemption(selector,context);}catch(...){t.suspended=true;t.ready=false;--scheduled_resumes;throw;}
        }
        context->r[2].low=t.id;return true;
    }
    if(selector==0x33) { // WakeupThread: a sleeping thread becomes ready (unless suspended); otherwise the wakeup is banked.
        if(self || !t.context.pc)throw UnsupportedSyscall(selector,context->pc);
        const bool was_waiting=t.waiting;
        if(was_waiting){t.waiting=false;t.ready=!t.suspended;}else ++t.wakeups;
        ++scheduled_wakes;
        if(!mainYieldsToReadyWorkers(rdram,context))
            try{requireNoPreemption(selector,context);}
            catch(...){if(was_waiting){t.waiting=true;t.ready=false;}else --t.wakeups;--scheduled_wakes;throw;}
        context->r[2].low=t.id;return true;
    }
    if(selector==0x32) {
        if(t.wakeups)throw UnsupportedSyscall(selector,context->pc); // Banked wakeups are not yet reached.
        t.ready=false;t.waiting=true;t.context=*context;++startup_worker_waits;
        active_ee_stack->yield();
        // Only an owned wake readies a parked worker; the scheduler then resumes this stack.
        if(t.waiting || !t.ready || t.suspended || current_thread_id!=t.id)
            throw std::runtime_error("Unvalidated startup worker wake");
        context->r[2].low=t.id;++worker_sleep_returns;return true; // SleepThread returns the caller ID.
    }
    throw UnsupportedSyscall(selector,context->pc);
}

void EeRuntime::releaseThread(DormantThread& t) {
    // Dormant again: no context, no wait, no banked wakeups. A stack parked in a wait is unwound.
    t.ready=t.waiting=t.suspended=false;t.wakeups=0;t.context=R5900Context{};
    const auto found=std::find_if(worker_executions_.begin(),worker_executions_.end(),[&](const auto& o){return o->id==t.id;});
    if(found!=worker_executions_.end()) {
        if((*found)->stack->state()!=RetainedEeStack::State::Waiting)throw std::runtime_error("Releasing a running thread's stack");
        worker_executions_.erase(found);
    }
}

void EeRuntime::exitThread(std::uint32_t selector,R5900Context* context) {
    // ExitThread / ExitDeleteThread: the calling worker ends (and with 0x24 its record goes). Its stack
    // parks once more; the scheduler sees the thread exited and releases it. Main ending the program is
    // not modelled.
    if(current_thread_id==1 || in_interrupt_ || !active_ee_stack || !startup_scheduler_active)throw UnsupportedSyscall(selector,context->pc);
    auto& t=threads.at(current_thread_id-2);
    t.ready=t.waiting=t.suspended=false;t.wakeups=0;t.exited=true;t.deleted=selector==0x24;
    ++thread_exits;
    active_ee_stack->yield();
    throw std::runtime_error("An exited thread resumed");
}

bool EeRuntime::handleInterruptSyscall(std::uint8_t* rdram,std::uint32_t selector,R5900Context* context) {
    // Interrupt context: current_thread_id names the interrupted thread; no reschedule happens here.
    const auto id=static_cast<std::uint32_t>(context->r[4].low);
    if(selector==0xFFFFFFD1u) {context->r[2].low=current_thread_id;return true;} // iGetThreadId
    if(selector==0xFFFFFFCCu && id==1) {wakeMainThread();context->r[2].low=1;return true;} // iWakeupThread(main)
    if(id<2 || id-2>=threads.size() || threads[id-2].id!=id || threads[id-2].deleted)return false;
    auto& t=threads[id-2];
    if(selector==0xFFFFFFCFu) { // iReferThreadStatus
        const auto out=static_cast<std::uint32_t>(context->r[5].low);
        if(out%4)return false;
        writeThreadStatus(rdram,id,out,context->pc);context->r[2].low=read32(rdram,out);return true;
    }
    if(selector==0xFFFFFFCCu) { // iWakeupThread
        if(id==current_thread_id || !t.context.pc)return false;
        if(t.waiting) {t.waiting=false;t.ready=!t.suspended;}
        else ++t.wakeups;
        ++worker_wakes;context->r[2].low=id;return true;
    }
    return false;
}

void EeRuntime::runReadyThreads(std::uint8_t*,R5900Context* parent) {
    // Main is the parent host stack. Ready workers above main run in priority order: a new
    // owner starts a started worker; a parked owner resumes only after an owned wake.
    const auto saved=*parent;const bool nested=startup_scheduler_active;
    if(current_thread_id!=1 || in_interrupt_)throw UnsupportedSyscall(0,parent->pc);
    startup_scheduler_active=true;
    try {
        for(;;) {
            auto next=threads.end();
            for(auto t=threads.begin();t!=threads.end();++t)
                if(t->ready && !t->suspended && !t->waiting && (main_sleeping || t->current_priority<main_thread_priority)
                    && (next==threads.end() || t->current_priority<next->current_priority))next=t;
            if(next==threads.end())break;
            const auto id=next->id;
            auto found=std::find_if(worker_executions_.begin(),worker_executions_.end(),[&](const auto& o){return o->id==id;});
            std::shared_ptr<EeExecutionOwner> owner;
            if(found!=worker_executions_.end()) {
                owner=*found;
                if(owner->stack->state()!=RetainedEeStack::State::Waiting)throw UnsupportedSyscall(0,parent->pc);
                ++worker_resumes;
            } else {
                // The sound thread's parked stack belongs to the startup's sound dispatch.
                if(id==3 && sound_execution_)throw UnsupportedSyscall(0,parent->pc);
                owner=std::make_shared<EeExecutionOwner>(*this,id);worker_executions_.push_back(owner);
                ++startup_worker_dispatches;
            }
            try {owner->dispatch();}
            catch(...) {threads.at(id-2).context=owner->context;*parent=owner->context;throw;}
            auto& ran=threads.at(id-2);
            if(owner->stack->state()!=RetainedEeStack::State::Waiting || (!ran.waiting && !ran.exited)
                || std::memcmp(parent,&saved,sizeof(*parent)))
                throw std::runtime_error("Scheduler lost retained parent/worker ownership");
            if(ran.exited) {ran.exited=false;owner.reset();releaseThread(ran);} // Unwinds the exited stack.
        }
    } catch(...) {startup_scheduler_active=nested;throw;}
    startup_scheduler_active=nested;
}
