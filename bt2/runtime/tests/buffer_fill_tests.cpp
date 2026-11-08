#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#include <algorithm>
#include <iostream>
#include <utility>

int main() {
    try {
        for (const auto [address, size] : std::array<std::pair<std::uint32_t, std::uint32_t>, 4>{
                {{0x1000, 8192}, {0x1003, 23}, {0x1000, 11}, {0x1000, 0}}}) {
            EeRuntime runtime;
            runtime.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size, 0xA5));
            runtime.expected_return = 0xFFFFFFFC;
            runtime.registerFunction(0x003251DC, FUN_003251DC_0x3251dc);
            R5900Context context{};
            for (auto& reg : context.r) reg.high = UINT64_MAX;
            SET_GPR_U32(&context, 4, address);
            SET_GPR_U32(&context, 5, 0x15A);
            SET_GPR_U32(&context, 6, size);
            SET_GPR_U32(&context, 31, runtime.expected_return);
            runtime.callFunction(0x003251DC, context);
            if (context.pc != runtime.expected_return || runtime.returns != 1 || GPR_U32(&context, 2) != address
                    || !std::all_of(runtime.memory().begin(), runtime.memory().begin() + address,
                                    [](auto byte) { return byte == 0xA5; })
                    || !std::all_of(runtime.memory().begin() + address, runtime.memory().begin() + address + size,
                                    [](auto byte) { return byte == 0x5A; })
                    || !std::all_of(runtime.memory().begin() + address + size, runtime.memory().end(),
                                    [](auto byte) { return byte == 0xA5; }))
                throw std::runtime_error("Generated buffer fill bounds/values/return failed");
        }
        std::cout << "Generated buffer fill passed: vector, unaligned, short and zero-size cases.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
