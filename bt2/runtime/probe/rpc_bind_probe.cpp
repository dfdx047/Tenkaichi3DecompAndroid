#include "ee/ee_runtime.h"
#include <ps2_recompiled_functions.h>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Usage: bt2-rpc-bind-probe CHECKED_CAPTURE_IMAGE");
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        if (!input || input.tellg() != EeRuntime::ram_size)
            throw std::runtime_error("Invalid checked RPC capture image");
        std::vector<std::uint8_t> image(EeRuntime::ram_size);
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(image.data()), image.size()))
            throw std::runtime_error("Could not read checked capture image");
        EeRuntime runtime;
        runtime.loadMemory(image);
        // Explicit isolated fixture: reconstruct pending packet from the checked
        // IOP request, before the captured callback's completed EE state.
        runtime.sif_rpc_reads = 1;
        runtime.semaphores.resize(4);
        runtime.semaphores[3] = {0, 1, 0x3838D8};
        auto* memory = runtime.memory().data();
        runtime.write32(memory, 0x3A87A8, 0x203A97C0);
        runtime.write32(memory, 0x3A97D0, 5);
        runtime.write32(memory, 0x3A97D8, 2);
        for (const auto offset : {20u, 24u, 36u}) runtime.write32(memory, 0x3A87A8 + offset, 0);
        runtime.registerFunction(0x3322E8, FUN_003322E8_0x3322e8);
        runtime.registerFunction(0x332258, FUN_00332258_0x332258);
        runtime.registerFunction(0x32EC90, FUN_0032EC90_0x32ec90);
        R5900Context context;
        context.r[4].low = 0x3A94C0;
        context.r[5].low = 0x3AAFC0;
        context.r[28].low = 0x38BDF0;
        context.r[29].low = 0x1FFEC20;
        context.r[31].low = runtime.expected_return = 0xFFFFFFF8;
        runtime.validateRpcEndCallback(context);
        if (context.pc != runtime.expected_return || context.r[29].low != 0x1FFEC20
                || runtime.read32(memory, 0x3A87A8) != 0
                || runtime.read32(memory, 0x3A87BC) != 0x3FB20
                || runtime.read32(memory, 0x3A87C0) != 0
                || runtime.read32(memory, 0x3A87CC) != 0x3F570
                || runtime.read32(memory, 0x3A97D0) != 4
                || runtime.read32(memory, 0x3A97D8) != 0
                || runtime.semaphores[3].count != 1 || runtime.semaphore_signals != 1
                || runtime.guest_calls != 2 || runtime.returns != 3)
            throw std::runtime_error("Generated RPC_END effects differ from checked capture");
        // Interrupt-only syscall must remain unavailable outside callback scope.
        context.r[3].low = 0xFFFFFFBD;
        bool rejected = false;
        try { runtime.handleSyscall(memory, &context, 0); }
        catch (const UnsupportedSyscall&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Interrupt syscall escaped callback scope");
        std::cout << "Three generated RPC_END functions returned; client, packet release and semaphore match capture.\n";
        std::cout << "Isolated callback only; automatic DMA/interrupt delivery remains unsupported.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
