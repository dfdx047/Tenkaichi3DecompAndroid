#pragma once
#include "ee_runtime.h"
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

// A persistent guest invocation owns its CPU, host frames and guest return stack.
// Main remains live on the parent host stack; each selection swaps only ownership.
struct EeExecutionOwner {
    EeRuntime& rt;
    R5900Context context;
    std::vector<unsigned> returns;
    unsigned depth=0,expected=0x100250,saved_id=0,id=3,entry=0x315380;
    RetainedEeStack* saved_active=nullptr;
    std::unique_ptr<RetainedEeStack> stack;
    explicit EeExecutionOwner(EeRuntime& runtime,unsigned thread_id=3):rt(runtime),context(runtime.threads.at(thread_id-2).context),id(thread_id),entry(runtime.threads.at(thread_id-2).entry) {
        stack=std::make_unique<RetainedEeStack>([this] {
            rt.dispatchGuestBranch(rt.memory().data(),&context,entry,id==3?rt.sound_dispatch_wait_ra:rt.startup_dispatch_parent.pc,0,EeRuntime::GuestBranchKind::TailJump,"owned EE sound worker");
            throw std::runtime_error("Unexpected persistent sound worker exit");
        });
    }
    void select() {
        saved_id=rt.current_thread_id;saved_active=rt.active_ee_stack;
        std::swap(returns,rt.call_returns_);std::swap(depth,rt.dispatch_depth_);std::swap(expected,rt.expected_return);
        rt.current_thread_id=id;rt.active_ee_stack=stack.get();
    }
    void restore() {
        rt.current_thread_id=saved_id;rt.active_ee_stack=saved_active;
        std::swap(returns,rt.call_returns_);std::swap(depth,rt.dispatch_depth_);std::swap(expected,rt.expected_return);
    }
    void dispatch() {select();try{stack->resume();}catch(...){restore();throw;}restore();}
    ~EeExecutionOwner() {
        if(stack && stack->state()==RetainedEeStack::State::Waiting) {
            select();stack.reset();restore();
        }
    }
};
