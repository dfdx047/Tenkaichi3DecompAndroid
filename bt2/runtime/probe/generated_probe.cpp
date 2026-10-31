#include "ee/ee_runtime.h"
#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#include <iostream>

namespace {
constexpr std::uint32_t probe_address = 0x0032EEE0;
constexpr std::uint32_t return_sentinel = 0xFFFFFFFC;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Action> void must_reject(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "Expected unsupported operation to be rejected");
}

void run_probe() {
    R5900Context context{};
    for (unsigned index = 0; index < context.r.size(); ++index)
        context.r[index] = {0x12340000ull + index, 0xABCD0000ull + index};
    context.r[4].low = 0;
    context.r[31].low = return_sentinel;
    const auto before = context.r;
    EeRuntime runtime;
    runtime.expected_return = return_sentinel;
    runtime.registerFunction(probe_address, FUN_0032EEE0_0x32eee0);
    runtime.callFunction(probe_address, context);
    require(runtime.cache_requests == 1, "Generated code did not reach the syscall boundary");
    require(runtime.syscall_resume_pc == probe_address + 8, "Incorrect syscall resume PC");
    require(runtime.returns == 1 && context.pc == return_sentinel, "Generated return did not reach sentinel");
    for (unsigned index = 0; index < context.r.size(); ++index) {
        require(context.r[index].high == before[index].high, "Upper GPR bits changed");
        require(context.r[index].low == (index == 3 ? 0x64 : before[index].low), "Unexpected GPR write");
    }
    SET_GPR_S32(&context, 0, -1);
    require(context.r[0].low == before[0].low && GPR_U32(&context, 0) == 0, "Zero register semantics failed");
    SET_GPR_S32(&context, 5, -1);
    require(context.r[5].low == UINT64_MAX && context.r[5].high == before[5].high, "GPR sign extension failed");
    must_reject([&] { runtime.callFunction(0x003B7FA8, context); });
    context.r[3].low = 0x3C;
    must_reject([&] { runtime.handleSyscall(nullptr, &context, 0); });
    context.r[4].low = 1;
    must_reject([&] { runtime.callFunction(probe_address, context); });
    context.r[4].low = 0;
    context.r[31].low = return_sentinel - 4;
    must_reject([&] { runtime.callFunction(probe_address, context); });
    std::cout << "BT2 generated-code probe passed.\n"
              << "Executed FUN_0032EEE0: syscall selector 0x64, operation 0; returned to host sentinel.\n"
              << "Unsupported syscall, cache operation, guest target, and return target rejected.\n"
              << "This isolated probe does not boot the game or model PS2 cache behavior.\n";
}
}

int main() {
    try { run_probe(); return 0; }
    catch (const std::exception& error) { std::cerr << "Probe failed: " << error.what() << '\n'; return 1; }
}
