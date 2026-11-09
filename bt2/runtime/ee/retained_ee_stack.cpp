#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "retained_ee_stack.h"
#include <exception>
#include <stdexcept>
#include <utility>

struct RetainedEeStack::Impl {
    struct Cancel {};
    std::function<void()> entry;
    void* fiber = nullptr;
    void* parent = nullptr;
    DWORD owner = GetCurrentThreadId();
    State state = State::Ready;
    bool cancel = false;
    std::exception_ptr error;
    static void WINAPI run(void* pointer) noexcept {
        auto& self = *static_cast<Impl*>(pointer);
        try {
            self.entry();
            self.state = State::Completed;
        } catch (const Cancel&) {
            self.state = State::Cancelled;
        } catch (...) {
            self.error = std::current_exception();
            self.state = State::Failed;
        }
        SwitchToFiber(self.parent);
        // A terminal stack must never be scheduled again.
        std::terminate();
    }
};
RetainedEeStack::RetainedEeStack(std::function<void()> entry) : impl_(std::make_unique<Impl>()) {
    if (!entry) throw std::invalid_argument("Missing EE stack entry");
    impl_->entry = std::move(entry);
    impl_->fiber = CreateFiberEx(64 * 1024, 4 * 1024 * 1024, FIBER_FLAG_FLOAT_SWITCH, Impl::run, impl_.get());
    if (!impl_->fiber) throw std::runtime_error("Cannot allocate retained EE stack");
}
RetainedEeStack::~RetainedEeStack() {
    if (impl_->owner != GetCurrentThreadId() || impl_->state == State::Running) std::terminate();
    if (impl_->state == State::Waiting) {
        impl_->cancel = true;
        try { resume(); } catch (...) { std::terminate(); }
        if (impl_->state != State::Cancelled) std::terminate();
    }
    DeleteFiber(impl_->fiber);
}
void RetainedEeStack::resume() {
    auto& self = *impl_;
    if (self.owner != GetCurrentThreadId() || (self.state != State::Ready && self.state != State::Waiting))
        throw std::logic_error("Invalid retained EE dispatch");
    const bool converted = !IsThreadAFiber();
    self.parent = converted ? ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH) : GetCurrentFiber();
    if (!self.parent) throw std::runtime_error("Cannot retain EE parent stack");
    self.state = State::Running;
    SwitchToFiber(self.fiber);
    if (converted && !ConvertFiberToThread()) std::terminate();
    if (self.error) std::rethrow_exception(self.error);
}
void RetainedEeStack::yield() {
    auto& self = *impl_;
    if (self.owner != GetCurrentThreadId() || self.state != State::Running || GetCurrentFiber() != self.fiber)
        throw std::logic_error("EE yield outside owned stack");
    self.state = State::Waiting;
    SwitchToFiber(self.parent);
    if (self.cancel) throw Impl::Cancel{};
}
RetainedEeStack::State RetainedEeStack::state() const { return impl_->state; }
#else
#include "retained_ee_stack.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

// The same cooperative stack without Win32 fibers (Android, Linux; bionic has no ucontext): the
// stack is mapped here, with a guard page below it, and a switch is a few instructions
// (bt2_stack_switch) that push the callee-saved registers on the stack being left, keep its stack
// pointer, and pop them from the stack being entered. A new stack starts in bt2_stack_start, which
// calls its entry with its argument (both placed in callee-saved registers of its first frame).
extern "C" void bt2_stack_switch(void** save_sp, void* load_sp);
extern "C" void bt2_stack_start();
#if defined(__aarch64__)
asm(R"(
    .text
    .p2align 2
    .globl bt2_stack_switch
    .hidden bt2_stack_switch
    .type bt2_stack_switch, %function
bt2_stack_switch:
    sub sp, sp, #160
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]
    stp d8, d9, [sp, #96]
    stp d10, d11, [sp, #112]
    stp d12, d13, [sp, #128]
    stp d14, d15, [sp, #144]
    mov x9, sp
    str x9, [x0]
    mov sp, x1
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    ldp d8, d9, [sp, #96]
    ldp d10, d11, [sp, #112]
    ldp d12, d13, [sp, #128]
    ldp d14, d15, [sp, #144]
    add sp, sp, #160
    ret
    .size bt2_stack_switch, .-bt2_stack_switch

    .p2align 2
    .globl bt2_stack_start
    .hidden bt2_stack_start
    .type bt2_stack_start, %function
bt2_stack_start:
    mov x0, x19
    blr x20
    brk #0
    .size bt2_stack_start, .-bt2_stack_start
)");
#elif defined(__x86_64__)
asm(R"(
    .text
    .p2align 4
    .globl bt2_stack_switch
    .hidden bt2_stack_switch
    .type bt2_stack_switch, @function
bt2_stack_switch:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, (%rdi)
    movq %rsi, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret
    .size bt2_stack_switch, .-bt2_stack_switch

    .p2align 4
    .globl bt2_stack_start
    .hidden bt2_stack_start
    .type bt2_stack_start, @function
bt2_stack_start:
    movq %r13, %rdi
    callq *%r12
    ud2
    .size bt2_stack_start, .-bt2_stack_start
)");
#else
#error "RetainedEeStack: no stack switch for this processor"
#endif

namespace {
constexpr std::size_t stack_bytes = 4 * 1024 * 1024; // As the fibers' reserve.
}

struct RetainedEeStack::Impl {
    struct Cancel {};
    std::function<void()> entry;
    void* memory = nullptr;
    std::size_t mapped = 0, guard = 0;
    void* sp = nullptr;        // The stack's own, while it is not running.
    void* parent_sp = nullptr; // The resumer's, while the stack runs.
    std::thread::id owner = std::this_thread::get_id();
    State state = State::Ready;
    bool cancel = false;
    std::exception_ptr error;
    static void run(void* pointer) noexcept {
        auto& self = *static_cast<Impl*>(pointer);
        try {
            self.entry();
            self.state = State::Completed;
        } catch (const Cancel&) {
            self.state = State::Cancelled;
        } catch (...) {
            self.error = std::current_exception();
            self.state = State::Failed;
        }
        bt2_stack_switch(&self.sp, self.parent_sp);
        // A terminal stack must never be scheduled again.
        std::terminate();
    }
    bool onStack() const {
        const char here = 0;
        const auto at = reinterpret_cast<std::uintptr_t>(&here), base = reinterpret_cast<std::uintptr_t>(memory);
        return at >= base + guard && at < base + mapped;
    }
};
RetainedEeStack::RetainedEeStack(std::function<void()> entry) : impl_(std::make_unique<Impl>()) {
    if (!entry) throw std::invalid_argument("Missing EE stack entry");
    auto& self = *impl_;
    self.entry = std::move(entry);
    self.guard = std::size_t(sysconf(_SC_PAGESIZE));
    self.mapped = stack_bytes + self.guard;
    self.memory = mmap(nullptr, self.mapped, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (self.memory == MAP_FAILED) {self.memory = nullptr;throw std::runtime_error("Cannot allocate retained EE stack");}
    mprotect(self.memory, self.guard, PROT_NONE); // An overflow faults instead of writing below the stack.
    // The first frame, as bt2_stack_switch pops it: the entry and its argument in callee-saved
    // registers, and bt2_stack_start as the return address.
    const auto top = (reinterpret_cast<std::uintptr_t>(self.memory) + self.mapped) & ~std::uintptr_t(15);
    const auto function = reinterpret_cast<std::uintptr_t>(&Impl::run), argument = reinterpret_cast<std::uintptr_t>(&self);
    const auto start = reinterpret_cast<std::uintptr_t>(&bt2_stack_start);
#if defined(__aarch64__)
    auto* frame = reinterpret_cast<std::uintptr_t*>(top - 160); // x19..x30, d8..d15
    for (unsigned n = 0; n < 20; ++n) frame[n] = 0;
    frame[0] = argument; frame[1] = function; frame[11] = start; // x19, x20, x30
#else
    auto* frame = reinterpret_cast<std::uintptr_t*>(top - 16 - 56); // r15 r14 r13 r12 rbx rbp, return address
    for (unsigned n = 0; n < 7; ++n) frame[n] = 0;
    frame[2] = argument; frame[3] = function; frame[6] = start; // r13, r12; bt2_stack_start's call is then 16-byte aligned
#endif
    self.sp = frame;
}
RetainedEeStack::~RetainedEeStack() {
    if (impl_->owner != std::this_thread::get_id() || impl_->state == State::Running) std::terminate();
    if (impl_->state == State::Waiting) {
        impl_->cancel = true;
        try { resume(); } catch (...) { std::terminate(); }
        if (impl_->state != State::Cancelled) std::terminate();
    }
    if (impl_->memory) munmap(impl_->memory, impl_->mapped);
}
void RetainedEeStack::resume() {
    auto& self = *impl_;
    if (self.owner != std::this_thread::get_id() || (self.state != State::Ready && self.state != State::Waiting))
        throw std::logic_error("Invalid retained EE dispatch");
    self.state = State::Running;
    bt2_stack_switch(&self.parent_sp, self.sp);
    if (self.error) std::rethrow_exception(self.error);
}
void RetainedEeStack::yield() {
    auto& self = *impl_;
    if (self.owner != std::this_thread::get_id() || self.state != State::Running || !self.onStack())
        throw std::logic_error("EE yield outside owned stack");
    self.state = State::Waiting;
    bt2_stack_switch(&self.sp, self.parent_sp);
    if (self.cancel) throw Impl::Cancel{};
}
RetainedEeStack::State RetainedEeStack::state() const { return impl_->state; }
#endif
