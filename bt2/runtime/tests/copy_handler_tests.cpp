#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#include <algorithm>
#include <iostream>

int main() {
    try {
        for (const auto size : {0u, 12u, 0x330u}) {
            EeRuntime runtime;
            std::vector<std::uint8_t> image(EeRuntime::ram_size, 0xA5);
            constexpr std::uint32_t source = 0x1000, destination = 0x2000;
            for (std::uint32_t i = 0; i < size; ++i) image[source + i] = static_cast<std::uint8_t>(i);
            runtime.loadMemory(image);
            runtime.expected_return = 0xFFFFFFFC;
            runtime.registerFunction(0x0033A7C8, FUN_0033A7C8_0x33a7c8);
            R5900Context context{};
            SET_GPR_U32(&context, 4, destination);
            SET_GPR_U32(&context, 5, source);
            SET_GPR_U32(&context, 6, size);
            SET_GPR_U32(&context, 31, runtime.expected_return);
            runtime.callFunction(0x0033A7C8, context);
            if (context.pc != runtime.expected_return || GPR_U32(&context, 2) != 0
                    || runtime.returns != 1 || runtime.word_writes != size / 4
                    || !std::equal(image.begin() + source, image.begin() + source + size,
                                   runtime.memory().begin() + destination))
                throw std::runtime_error("Generated copy result/return differs");
            std::copy(image.begin() + source, image.begin() + source + size, image.begin() + destination);
            if (!std::equal(image.begin(), image.end(), runtime.memory().begin()))
                throw std::runtime_error("Generated copy changed RAM outside destination");
        }
        std::cout << "Generated FUN_0033A7C8 copy/zero-size/return verified in bounded RAM.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
