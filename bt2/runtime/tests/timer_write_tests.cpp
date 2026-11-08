#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#include <algorithm>
#include <iostream>

int main() {
    try {
        for (const auto status : {0u, 0x10000u}) {
            EeRuntime runtime;
            runtime.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size, 0xA5));
            runtime.supplyTimer2Mode(0xC80);
            R5900Context context{};
            context.cop0_status = status;
            context.cop0_epc = 0x1234;
            context.ll_bit = true;
            context.r[4] = {0xB0001010, UINT64_MAX};
            context.r[5] = {0x382, UINT64_MAX};
            context.r[31].low = 0xFFFFFFFC;
            runtime.Load32(runtime.memory().data(), &context, 0x10001010);
            runtime.registerFunction(0x00338AD8, FUN_00338AD8_0x338ad8);
            runtime.callFunction(0x00338AD8, context);
            if (context.pc != 0xFFFFFFFC || context.cop0_errorepc != 0xFFFFFFFC
                    || context.cop0_status != status || context.cop0_epc != 0x1234 || context.ll_bit
                    || runtime.timer2_mode_writes != 1 || runtime.timer2_mode_after_write != 0xF82
                    || runtime.word_writes != 1 || context.r[4].high != UINT64_MAX
                    || !std::all_of(runtime.memory().begin(), runtime.memory().end(), [](auto b) { return b == 0xA5; }))
                throw std::runtime_error("Generated Timer 2 store/Status/ErrorEPC/ERET state failed");
        }
        std::cout << "Generated timer helper returned through ErrorEPC with EIE clear/set; LL bit cleared, RAM unchanged.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
