#include "probe_state.h"
#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#ifdef BT2_KERNEL_PAYLOAD
#include <bt2_kernel_functions.h>
#endif
#ifdef BT2_STARTUP_KERNEL_PAYLOAD
#include <bt2_startup_kernel_functions.h>
#endif

// Binds the fixed generated (ignored) startup functions and the copied kernel payloads to their guest
// addresses; the selection loop's entries are bound in their own files (selected_bindings.cpp,
// overlay_bindings.cpp) so a new selection does not recompile this one.

void registerStartupFunctions(EeRuntime& runtime) {
    runtime.registerFunction(0x31EDB0, FUN_0031EDB0_0x31edb0);
    runtime.registerFunction(0x31F010, FUN_0031F010_0x31f010);
    runtime.registerFunction(0x332970, FUN_00332970_0x332970);
    runtime.registerFunction(0x32EC70, FUN_0032EC70_0x32ec70);
    runtime.registerFunction(0x3322E8, FUN_003322E8_0x3322e8);
    runtime.registerFunction(0x332258, FUN_00332258_0x332258);
    runtime.registerFunction(0x32EC90, FUN_0032EC90_0x32ec90);
    runtime.registerFunction(0x00100008, FUN_00100008_0x100008);
    runtime.registerFunction(0x00338520, FUN_00338520_0x338520);
    runtime.registerFunction(0x003382D0, FUN_003382D0_0x3382d0);
    runtime.registerFunction(0x0032EC60, FUN_0032EC60_0x32ec60);
    runtime.registerFunction(0x00338410, FUN_00338410_0x338410);
    runtime.registerFunction(0x00338510, FUN_00338510_0x338510);
    runtime.registerFunction(0x003383C8, FUN_003383C8_0x3383c8);
    runtime.registerFunction(0x00338388, FUN_00338388_0x338388);
    runtime.registerFunction(0x00338A00, FUN_00338A00_0x338a00);
    runtime.registerFunction(0x00338998, FUN_00338998_0x338998);
    runtime.registerFunction(0x003389A8, FUN_003389A8_0x3389a8);
    runtime.registerFunction(0x003389B8, FUN_003389B8_0x3389b8);
    runtime.registerFunction(0x0032EEE0, FUN_0032EEE0_0x32eee0);
    runtime.registerFunction(0x003389F0, FUN_003389F0_0x3389f0);
    runtime.registerFunction(0x00338B78, FUN_00338B78_0x338b78);
    runtime.registerFunction(0x003251DC, FUN_003251DC_0x3251dc);
    runtime.registerFunction(0x00339E48, FUN_00339E48_0x339e48);
    runtime.registerFunction(0x00338648, FUN_00338648_0x338648);
    runtime.registerFunction(0x003385E0, FUN_003385E0_0x3385e0);
    runtime.registerFunction(0x00321E50, FUN_00321E50_0x321e50);
    runtime.registerFunction(0x00321D90, FUN_00321D90_0x321d90);
    runtime.registerFunction(0x00322F90, FUN_00322F90_0x322f90);
    runtime.registerFunction(0x0032EC80, FUN_0032EC80_0x32ec80);
    runtime.registerFunction(0x0032ECA0, FUN_0032ECA0_0x32eca0);
    runtime.registerFunction(0x003242D0, FUN_003242D0_0x3242d0);
    runtime.registerFunction(0x00331FE8, FUN_00331FE8_0x331fe8);
    runtime.registerFunction(0x00331888, FUN_00331888_0x331888);
    runtime.registerFunction(0x003382B8, FUN_003382B8_0x3382b8);
    runtime.registerFunction(0x0032E970, FUN_0032E970_0x32e970);
    runtime.registerFunction(0x0032F878, FUN_0032F878_0x32f878);
    runtime.registerFunction(0x0032E9C0, FUN_0032E9C0_0x32e9c0);
    runtime.registerFunction(0x0032F070, FUN_0032F070_0x32f070);
    runtime.registerFunction(0x00331D70, FUN_00331D70_0x331d70);
    runtime.registerFunction(0x00331C38, FUN_00331C38_0x331c38);
    runtime.registerFunction(0x00331B70, FUN_00331B70_0x331b70);
    runtime.registerFunction(0x0031F8B0, FUN_0031F8B0_0x31f8b0);
    runtime.registerFunction(0x0031F638, FUN_0031F638_0x31f638);
    runtime.registerFunction(0x00332B70, FUN_00332B70_0x332b70);
    runtime.registerFunction(0x00332790, FUN_00332790_0x332790);
    runtime.registerFunction(0x003321B0, FUN_003321B0_0x3321b0);
    runtime.registerFunction(0x0032F020, FUN_0032F020_0x32f020);
    runtime.registerFunction(0x00331F38, FUN_00331F38_0x331f38);
    runtime.registerFunction(0x00100388, FUN_00100388_0x100388);
    runtime.registerFunction(0x0033A238, FUN_0033A238_0x33a238);
    runtime.registerFunction(0x0033A820, FUN_0033A820_0x33a820);
    runtime.registerFunction(0x0033A810, FUN_0033A810_0x33a810);
    runtime.registerFunction(0x0033A800, FUN_0033A800_0x33a800);
    runtime.registerFunction(0x0033A7C8, FUN_0033A7C8_0x33a7c8);
    runtime.registerFunction(0x0033A7B8, FUN_0033A7B8_0x33a7b8);
    runtime.registerFunction(0x0032ED00, FUN_0032ED00_0x32ed00);
    runtime.registerFunction(0x0032ED10, FUN_0032ED10_0x32ed10);
    runtime.registerFunction(0x0032EAF0, FUN_0032EAF0_0x32eaf0);
    runtime.registerFunction(0x0032FDA0, FUN_0032FDA0_0x32fda0);
    runtime.registerFunction(0x0032F730, FUN_0032F730_0x32f730);
    runtime.registerFunction(0x0032EB50, FUN_0032EB50_0x32eb50);
    runtime.registerFunction(0x0032FA38, FUN_0032FA38_0x32fa38);
    runtime.registerFunction(0x0032EA60, FUN_0032EA60_0x32ea60);
    runtime.registerFunction(0x00338DB8, FUN_00338DB8_0x338db8);
    runtime.registerFunction(0x0032F7A8, FUN_0032F7A8_0x32f7a8);
    runtime.registerFunction(0x0032E9A0, FUN_0032E9A0_0x32e9a0);
    runtime.registerFunction(0x00338B58, FUN_00338B58_0x338b58);
    runtime.registerFunction(0x00338AD8, FUN_00338AD8_0x338ad8);
    runtime.registerFunction(0x00338268, FUN_00338268_0x338268);
    runtime.registerFunction(0x0032E950, FUN_0032E950_0x32e950);
    registerSelectedStartup(runtime); // selected_bindings.cpp
    registerSelectedOverlays(runtime); // overlay_bindings.cpp
}

void registerKernelPayloads(EeRuntime& runtime,const ProbeModes& modes) {
    const bool timer_mode=modes.timer,osd_readback_mode=modes.osd_readback;
    if (timer_mode) {
    #ifdef BT2_KERNEL_PAYLOAD
        runtime.registerKernelFunction(0x80076000, FUN_80076000_0x80076000,
            std::span<const std::uint8_t>(runtime.memory()).subspan(0x0036B328, 0x740));
    #else
        throw std::runtime_error("Timer replay requires generated kernel payload integration");
    #endif
    }
    if (osd_readback_mode) {
    #ifdef BT2_STARTUP_KERNEL_PAYLOAD
        runtime.registerKernelFunction(0x80075000, FUN_80075000_0x80075000,
            std::span<const std::uint8_t>(runtime.memory()).subspan(0x0036BAF8, 0x330));
    #else
        throw std::runtime_error("OSD readback requires generated startup kernel payload");
    #endif
    }
}
