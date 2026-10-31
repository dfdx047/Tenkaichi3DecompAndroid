# libbt2.so: BT2 Recomp for Android arm64, the engine of Dragon Rage's Budokai Tenkaichi 2
# (android/app/.../engine/Bt2GameActivity.kt in the app). Built by android/build.py with the NDK's
# toolchain file; the same runtime and generated code as the Windows entry probe (cmake/bt2_sources.cmake),
# with the SDL3 host layer (runtime/host/sdl, BT2_SDL), the x86 intrinsics on NEON (runtime/platform/sse)
# and paraLLEl-GS on the phone's Vulkan. The game's code must have been generated first
# (tools/ps2recomp: game/generated, work/bt2-generation); it never goes into the repository.
#
# BT2_RUNTIME_CHECK (android/build.py check, the public CI): without the game's code, compile only the
# handwritten runtime and paraLLEl-GS for Android, as a check that the port builds (bt2-runtime-check).
cmake_minimum_required(VERSION 3.22)
project(bt2-android LANGUAGES C CXX)

if(NOT ANDROID)
    message(FATAL_ERROR "android/CMakeLists.txt builds for Android: configure it with the NDK's toolchain file (android/build.py).")
endif()

get_filename_component(BT2_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(BT2_GENERATED_DIR "${BT2_SOURCE_ROOT}/game/generated/bt2" CACHE PATH "Generated BT2 sources")
set(BT2_GENERATION_DIR "${BT2_SOURCE_ROOT}/work/bt2-generation" CACHE PATH "Generated startup bindings and overlay lists")
set(BT2_SDL3_DIR "" CACHE PATH "SDL3 for Android arm64: include/ and lib/libSDL3.so (Dragon Rage's toolchain: port/build/android-toolchain/sdl3)")
set(BT2_PARALLEL_GS_DIR "${BT2_SOURCE_ROOT}/work/dependencies/parallel-gs-cc6184af7e0c" CACHE PATH "Pinned paraLLEl-GS checkout")
set(BT2_XXHASH_DIR "${BT2_SOURCE_ROOT}/work/dependencies/xxhash-e626a72bc232" CACHE PATH "Pinned xxHash checkout")
set(BT2_ADRENOTOOLS_DIR "${BT2_SOURCE_ROOT}/../port/third_party/libadrenotools" CACHE PATH "libadrenotools (Dragon Rage's copy): custom GPU drivers")
option(BT2_RUNTIME_CHECK "Compile the handwritten runtime only, without the game's code (a build check)" OFF)

if(NOT BT2_RUNTIME_CHECK AND NOT EXISTS "${BT2_GENERATED_DIR}/FUN_0032EEE0_0x32eee0.cpp")
    message(FATAL_ERROR "No generated BT2 code in ${BT2_GENERATED_DIR}: run tools/ps2recomp/run_pipeline.py first.")
endif()
if(NOT EXISTS "${BT2_SDL3_DIR}/include/SDL3/SDL.h" OR NOT EXISTS "${BT2_SDL3_DIR}/lib/libSDL3.so")
    message(FATAL_ERROR "BT2_SDL3_DIR (${BT2_SDL3_DIR}) has no SDL3 for Android.")
endif()
if(NOT EXISTS "${BT2_PARALLEL_GS_DIR}/gs/gs_interface.hpp")
    message(FATAL_ERROR "Run tools/graphics/gpu/fetch_parallel_gs.py first.")
endif()
if(NOT EXISTS "${BT2_XXHASH_DIR}/xxhash.h")
    message(FATAL_ERROR "Run tools/graphics/textures/fetch_xxhash.py first.")
endif()

include("${BT2_SOURCE_ROOT}/cmake/bt2_sources.cmake")

# paraLLEl-GS and Granite as libraries, their shaders built in (the Windows build's settings). On Android
# Granite configures its own application layer, which wants the Game Development Kit's packages; that layer
# is not built (SDL is the platform), so empty stand-ins satisfy it (android/cmake).
foreach(bt2_agdk_package game-activity games-controller games-frame-pacing)
    set(${bt2_agdk_package}_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake" CACHE PATH "" FORCE)
endforeach()
set(PARALLEL_GS_STANDALONE ON CACHE BOOL "" FORCE)
add_subdirectory("${BT2_PARALLEL_GS_DIR}" parallel-gs EXCLUDE_FROM_ALL)

# Custom Vulkan drivers (Turnip on Adreno), as BT3's loader uses them; its hook libraries are in the APK already.
add_subdirectory("${BT2_ADRENOTOOLS_DIR}" adrenotools EXCLUDE_FROM_ALL)

add_library(SDL3 SHARED IMPORTED)
set_target_properties(SDL3 PROPERTIES IMPORTED_LOCATION "${BT2_SDL3_DIR}/lib/libSDL3.so" INTERFACE_INCLUDE_DIRECTORIES "${BT2_SDL3_DIR}/include")

# Dragon Rage's own part, in a library of its own (plugin/bt2_plugin.h): the Online Mode item and its screens, the opening's music,
# later the online play. It needs nothing of the game's code, so the public CI builds it (build.py plugin) and the APK carries it;
# the engine loads it when it starts (runtime/plugin/plugin_host.cpp).
add_library(bt2online SHARED
    "${BT2_SOURCE_ROOT}/plugin/online/online_plugin.cpp"
    "${BT2_SOURCE_ROOT}/plugin/online/opening_music.cpp"
    "${BT2_SOURCE_ROOT}/plugin/online/link.cpp"
    "${BT2_SOURCE_ROOT}/plugin/online/lockstep.cpp"
    "${BT2_SOURCE_ROOT}/plugin/online/session.cpp"
    "${BT2_SOURCE_ROOT}/plugin/online/fight_session.cpp"
    "${BT2_SOURCE_ROOT}/plugin/online/relay.cpp"
    "${BT2_SOURCE_ROOT}/plugin/online/stun.cpp")
set_target_properties(bt2online PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_include_directories(bt2online PRIVATE "${BT2_SOURCE_ROOT}/plugin" "${BT2_SOURCE_ROOT}/plugin/online")
target_compile_options(bt2online PRIVATE -O2 -fvisibility=hidden -w)
target_link_libraries(bt2online PRIVATE SDL3 log)

set(BT2_ANDROID_SOURCES
    "${BT2_SOURCE_ROOT}/runtime/gs/hw/hw_gs_mirror.cpp"
    "${BT2_SOURCE_ROOT}/runtime/gs/gpu/gpu_gs_backend.cpp"
    "${BT2_SOURCE_ROOT}/runtime/gs/gpu/gpu_gs_mirror.cpp"
    "${BT2_SOURCE_ROOT}/runtime/gs/gpu/hd_texture_pack.cpp"
    "${BT2_SOURCE_ROOT}/runtime/gs/gpu/gpu_window_presenter.cpp"
    "${BT2_SOURCE_ROOT}/runtime/gs/gpu/in_between_frames.cpp"
    "${BT2_SOURCE_ROOT}/runtime/game/startup_inputs.cpp"
    "${BT2_SOURCE_ROOT}/runtime/plugin/plugin_host.cpp"
    "${BT2_SOURCE_ROOT}/runtime/android/android_main.cpp")

# What every unit of the engine is built with.
# BT3's renderer (runtime/gs/hw/bt3: a copy of Tenkaichi3DecompAndroid's port/src/gs, BT2_RENDERER=bt3), in C as BT3
# builds it, with its shaders made the same way (tools/graphics/hw/make_bt3_shaders.py) and bt3_host.c for what it
# asks of the BT3 port around it.
set(BT2_HW_GS_DIR "${BT2_SOURCE_ROOT}/runtime/gs/hw")
file(GLOB BT2_HW_GS_SHADERS "${BT2_HW_GS_DIR}/bt3/shaders/*.vert" "${BT2_HW_GS_DIR}/bt3/shaders/*.frag")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hw_gs/shaders.h"
    COMMAND "${Python3_EXECUTABLE}" "${BT2_SOURCE_ROOT}/tools/graphics/hw/make_bt3_shaders.py" "${CMAKE_CURRENT_BINARY_DIR}/hw_gs"
    DEPENDS ${BT2_HW_GS_SHADERS} "${BT2_SOURCE_ROOT}/tools/graphics/hw/make_bt3_shaders.py")
add_library(bt2-hw-gs STATIC
    "${BT2_HW_GS_DIR}/bt3/gs_core.c" "${BT2_HW_GS_DIR}/bt3/gs_draw.c" "${BT2_HW_GS_DIR}/bt3/gs_gpu.c" "${BT2_HW_GS_DIR}/bt3/gs_gl.c"
    "${BT2_HW_GS_DIR}/bt3_host.c" "${CMAKE_CURRENT_BINARY_DIR}/hw_gs/shaders.h")
target_include_directories(bt2-hw-gs PRIVATE "${BT2_HW_GS_DIR}/bt3" "${CMAKE_CURRENT_BINARY_DIR}/hw_gs")
target_compile_definitions(bt2-hw-gs PRIVATE BT3_ANDROID=1)
target_compile_options(bt2-hw-gs PRIVATE -O2 -fPIC -fvisibility=hidden -fno-strict-aliasing -w)
target_link_libraries(bt2-hw-gs PRIVATE SDL3)

function(bt2_android_target target)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    # The x86 intrinsic headers stand in for the compiler's (runtime/platform/sse): first on the path.
    target_include_directories(${target} BEFORE PRIVATE "${BT2_SOURCE_ROOT}/runtime/platform/sse")
    target_include_directories(${target} PRIVATE
        "${BT2_SOURCE_ROOT}/runtime" "${BT2_SOURCE_ROOT}/runtime/ee" "${BT2_GENERATED_DIR}" "${BT2_GENERATION_DIR}" "${BT2_XXHASH_DIR}")
    target_compile_definitions(${target} PRIVATE BT2_SDL=1 BT2_GPU_GS=1 PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    # The PS2's float results are compared bit for bit: no contraction into fused multiply-adds (x86 has
    # none in SSE either), no fast math.
    target_compile_options(${target} PRIVATE -O2 -ffp-contract=off -fno-fast-math -fvisibility=hidden -w)
    target_link_libraries(${target} PRIVATE parallel-gs SDL3 adrenotools bt2-hw-gs)
endfunction()
# The runtime's main is called by android_main.cpp.
# (and BT3's renderer, gs/hw: only this build has it)
set_source_files_properties("${BT2_SOURCE_ROOT}/runtime/probe/entry_probe.cpp" PROPERTIES COMPILE_DEFINITIONS "main=bt2_entry_main;BT2_HW_GS=1")

if(BT2_RUNTIME_CHECK)
    # The handwritten units only: generated functions, and the bindings that include their declarations, left out.
    set(bt2_handwritten ${BT2_EE_RUNTIME_SOURCES} ${BT2_EE_HOT_SOURCES} ${BT2_ENTRY_PROBE_SOURCES} ${BT2_ANDROID_SOURCES}
        "${BT2_SOURCE_ROOT}/runtime/vu/vu1_native_none.cpp" "${BT2_SOURCE_ROOT}/runtime/probe/interpreter_handlers_none.cpp")
    list(FILTER bt2_handwritten EXCLUDE REGEX "/game/generated/|/startup_bindings\\.cpp$")
    add_library(bt2-runtime-check STATIC ${bt2_handwritten})
    bt2_android_target(bt2-runtime-check)
    return()
endif()

# One library; named bt2-entry-probe so the generated cmake fragments (target_sources(bt2-entry-probe ...))
# add to it as they do to the Windows probe.
add_library(bt2-entry-probe SHARED ${BT2_EE_RUNTIME_SOURCES} ${BT2_EE_HOT_SOURCES} ${BT2_ENTRY_PROBE_SOURCES} ${BT2_ANDROID_SOURCES})
set_target_properties(bt2-entry-probe PROPERTIES OUTPUT_NAME bt2)
bt2_android_target(bt2-entry-probe)

# Native VU1 programs and the EE interpreter's handlers, when generated (as in CMakeLists.txt).
if(EXISTS "${BT2_SOURCE_ROOT}/game/generated/vu1/vu1_native.cpp")
    target_sources(bt2-entry-probe PRIVATE "${BT2_SOURCE_ROOT}/game/generated/vu1/vu1_native.cpp")
else()
    target_sources(bt2-entry-probe PRIVATE "${BT2_SOURCE_ROOT}/runtime/vu/vu1_native_none.cpp")
endif()
if(EXISTS "${BT2_SOURCE_ROOT}/game/generated/ee-interpreter/ee_interpreter.cmake")
    # Its paths start at the repository's root, written as CMAKE_CURRENT_SOURCE_DIR of the Windows build.
    file(READ "${BT2_SOURCE_ROOT}/game/generated/ee-interpreter/ee_interpreter.cmake" bt2_interpreter)
    string(REPLACE "\${CMAKE_CURRENT_SOURCE_DIR}" "${BT2_SOURCE_ROOT}" bt2_interpreter "${bt2_interpreter}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ee_interpreter.cmake" "${bt2_interpreter}")
    include("${CMAKE_CURRENT_BINARY_DIR}/ee_interpreter.cmake")
else()
    target_sources(bt2-entry-probe PRIVATE "${BT2_SOURCE_ROOT}/runtime/probe/interpreter_handlers_none.cpp")
endif()
# The kernel code the game copies into kernel memory (tools/ps2recomp/generate_kernel.py), when generated:
# the Windows build's BT2_BUILD_KERNEL_PAYLOAD and BT2_BUILD_STARTUP_KERNEL_PAYLOAD.
if(EXISTS "${BT2_SOURCE_ROOT}/game/generated/bt2-kernel-runtime/FUN_80076000_0x80076000.cpp")
    target_sources(bt2-entry-probe PRIVATE "${BT2_SOURCE_ROOT}/game/generated/bt2-kernel-runtime/FUN_80076000_0x80076000.cpp")
    target_include_directories(bt2-entry-probe PRIVATE "${BT2_SOURCE_ROOT}/game/generated/bt2-kernel-runtime")
    target_compile_definitions(bt2-entry-probe PRIVATE BT2_KERNEL_PAYLOAD=1)
    if(EXISTS "${BT2_SOURCE_ROOT}/game/generated/bt2-kernel-runtime-startup/FUN_80075000_0x80075000.cpp")
        target_sources(bt2-entry-probe PRIVATE "${BT2_SOURCE_ROOT}/game/generated/bt2-kernel-runtime-startup/FUN_80075000_0x80075000.cpp")
        target_include_directories(bt2-entry-probe PRIVATE "${BT2_SOURCE_ROOT}/game/generated/bt2-kernel-runtime-startup")
        target_compile_definitions(bt2-entry-probe PRIVATE BT2_STARTUP_KERNEL_PAYLOAD=1)
    endif()
endif()
if(EXISTS "${BT2_GENERATION_DIR}/extra-startup.cmake")
    include("${BT2_GENERATION_DIR}/extra-startup.cmake")
    target_compile_definitions(bt2-entry-probe PRIVATE BT2_EXTRA_STARTUP_BINDINGS=1)
endif()
if(EXISTS "${BT2_GENERATION_DIR}/overlays.cmake")
    include("${BT2_GENERATION_DIR}/overlays.cmake")
    target_compile_definitions(bt2-entry-probe PRIVATE BT2_OVERLAY_BINDINGS=1)
endif()

target_link_libraries(bt2-entry-probe PRIVATE log android dl m)
target_link_options(bt2-entry-probe PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,--gc-sections" "-Wl,--build-id=sha1" "-Wl,--no-undefined")
