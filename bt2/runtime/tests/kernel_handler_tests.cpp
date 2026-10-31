#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#include <iostream>
#include <fstream>

void require(bool ok) { if (!ok) throw std::runtime_error("Kernel handler check failed"); }
int main(int argc, char** argv) {
    try {
        EeRuntime runtime;
        runtime.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        std::vector<std::uint8_t> snapshot(EeRuntime::kernel_snapshot_size);
        if (argc == 2) {
            std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
            if (!input || input.tellg() != static_cast<std::streamoff>(snapshot.size()))
                throw std::runtime_error("Invalid local kernel snapshot size");
            input.seekg(0);
            if (!input.read(reinterpret_cast<char*>(snapshot.data()), static_cast<std::streamsize>(snapshot.size())))
                throw std::runtime_error("Snapshot read failed");
            runtime.loadKernelSnapshot(snapshot);
            runtime.expected_return = 0xFFFFFFFC;
            runtime.registerFunction(0x338388, FUN_00338388_0x338388);
            R5900Context ctx{};
            SET_GPR_S32(&ctx, 4, static_cast<std::int32_t>(0x80000000u));
            SET_GPR_S32(&ctx, 5, static_cast<std::int32_t>(0x80080000u));
            SET_GPR_U32(&ctx, 6, 0x338388);
            SET_GPR_U32(&ctx, 31, runtime.expected_return);
            runtime.callFunction(0x338388, ctx);
            std::cout << "Local snapshot handler result: 0x" << std::hex << GPR_U32(&ctx, 2)
                      << "; diagnostic only, game boot remains incomplete.\n";
            return 0;
        }
        if (argc != 1) throw std::runtime_error("Supply at most one local snapshot path");
        // Authored test words only: these are not retail kernel contents.
        const std::uint32_t target = 0x123456;
        std::memcpy(snapshot.data() + 8, &target, 4);
        runtime.loadKernelSnapshot(snapshot);
        runtime.expected_return = 0xFFFFFFFC;
        runtime.registerFunction(0x338388, FUN_00338388_0x338388);
        R5900Context ctx{};
        SET_GPR_S32(&ctx, 4, static_cast<std::int32_t>(0x80000000u));
        SET_GPR_S32(&ctx, 5, static_cast<std::int32_t>(0x80000010u));
        SET_GPR_U32(&ctx, 6, target);
        SET_GPR_U32(&ctx, 31, runtime.expected_return);
        runtime.callFunction(0x338388, ctx);
        require(GPR_U32(&ctx, 2) == 0x80000008 && ctx.pc == runtime.expected_return);
        SET_GPR_S32(&ctx, 4, static_cast<std::int32_t>(0x80000000u));
        SET_GPR_U32(&ctx, 6, 0x654321);
        runtime.callFunction(0x338388, ctx);
        require(GPR_U32(&ctx, 2) == 0);
        auto* ram = runtime.memory().data();
        require(runtime.read32(ram, 0x80080000) == 0);
        bool rejected = false;
        try { (void)runtime.read32(ram, 0x80080004); } catch (const UnsupportedGuestMemory&) { rejected = true; }
        require(rejected);
        rejected = false;
        try { runtime.write32(ram, 0x80000000, 1); } catch (const UnsupportedGuestMemory&) { rejected = true; }
        require(rejected);
        rejected = false;
        try { runtime.loadKernelSnapshot(std::span(snapshot).first(8)); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected);
        std::cout << "Generated handler match/no-match and read-only kernel snapshot bounds passed (synthetic bytes only).\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
