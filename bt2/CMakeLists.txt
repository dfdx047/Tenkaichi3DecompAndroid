cmake_minimum_required(VERSION 3.20)
project(BT2Recomp VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "BT2 Recomp requires Windows and MSVC (Visual Studio 2022).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "BT2 Recomp requires x64. Configure with -A x64 in a fresh build directory.")
endif()

# Shared settings for every handwritten target. Handwritten code includes its headers by path from
# runtime/ (for example "gs/cpu/gs_command_sink.h"); generated code includes ps2_runtime.h or
# native_runtime.h by name through the per-target include directories below.
function(bt2_target_defaults target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
endfunction()

add_executable(bt2-port runtime/main.cpp)
set_target_properties(bt2-port PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
target_compile_options(bt2-port PRIVATE /W4 /permissive- /utf-8)

# EE runtime: the EeRuntime and GS implementation, built once as the bt2-ee-runtime library and linked by
# every probe and test target that includes ee_runtime.h.
set(BT2_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(BT2_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2" CACHE PATH "Ignored generated BT2 sources")
include(cmake/bt2_sources.cmake) # BT2_EE_RUNTIME_SOURCES, BT2_EE_HOT_SOURCES, BT2_ENTRY_PROBE_SOURCES
# BT2_EE_HOT_SOURCES: Debug compiles them optimized so the gate's Debug probe runs take a quarter of the
# time; they keep the debug standard library checks but not /RTC1.

# XXH3 for PCSX2-compatible texture names (docs/plans/graphics/HD_TEXTURES_PLAN.md); fetch it with
# tools/graphics/textures/fetch_xxhash.py.
set(BT2_XXHASH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/work/dependencies/xxhash-e626a72bc232" CACHE PATH "Pinned xxHash checkout")
if(NOT EXISTS "${BT2_XXHASH_DIR}/xxhash.h")
    message(FATAL_ERROR "Run tools/graphics/textures/fetch_xxhash.py first.")
endif()
set_source_files_properties(runtime/gs/textures/gs_texture_names.cpp PROPERTIES INCLUDE_DIRECTORIES "${BT2_XXHASH_DIR}")

# Native translations of VU1 microprograms, when tools/vu1/translate_programs.py has generated them
# (local, ignored); otherwise every program is interpreted. Tests always interpret.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/vu1/vu1_native.cpp")
    list(APPEND BT2_EE_HOT_SOURCES game/generated/vu1/vu1_native.cpp)
    message(STATUS "VU1: native microprogram translations from game/generated/vu1")
else()
    list(APPEND BT2_EE_HOT_SOURCES runtime/vu/vu1_native_none.cpp)
endif()

option(BT2_BUILD_GENERATED_PROBE "Build the isolated local generated-code probe" OFF)
if(BT2_BUILD_GENERATED_PROBE)
    set(BT2_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2" CACHE PATH "Ignored generated BT2 sources")
    set(BT2_PROBE_SOURCE "${BT2_GENERATED_DIR}/FUN_0032EEE0_0x32eee0.cpp")
    if(NOT EXISTS "${BT2_PROBE_SOURCE}")
        message(FATAL_ERROR "Generate local BT2 code with tools/ps2recomp/run_pipeline.py first.")
    endif()
    enable_testing()
    # EE probes and tests: generated units include ps2_runtime.h (the interface they see) through runtime/ee.
    function(bt2_ee_target target)
        bt2_target_defaults(${target})
        target_include_directories(${target} PRIVATE runtime/ee "${BT2_GENERATED_DIR}")
    endfunction()
    add_library(bt2-ee-runtime-hot OBJECT ${BT2_EE_HOT_SOURCES})
    bt2_ee_target(bt2-ee-runtime-hot)
    # Release keeps line information for the hot part, so a sample profile can be read by source line.
    target_compile_options(bt2-ee-runtime-hot PRIVATE $<$<CONFIG:Debug>:/O2 /Ob2> $<$<CONFIG:Release>:/Z7>)
    add_library(bt2-ee-runtime STATIC ${BT2_EE_RUNTIME_SOURCES} $<TARGET_OBJECTS:bt2-ee-runtime-hot>)
    bt2_ee_target(bt2-ee-runtime)
    add_executable(bt2-generated-probe runtime/probe/generated_probe.cpp "${BT2_PROBE_SOURCE}")
    bt2_ee_target(bt2-generated-probe)
    target_compile_definitions(bt2-generated-probe PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    add_test(NAME generated-bt2-probe COMMAND bt2-generated-probe)
    add_executable(bt2-disc-metadata-tests runtime/tests/disc_metadata_tests.cpp)
    bt2_target_defaults(bt2-disc-metadata-tests)
    add_test(NAME disc-metadata COMMAND bt2-disc-metadata-tests)
    add_executable(bt2-bootstrap-tests runtime/tests/bootstrap_tests.cpp runtime/probe/game_starter.cpp)
    bt2_ee_target(bt2-bootstrap-tests)
    # Its main keeps many EeRuntime objects (each several megabytes) as locals.
    target_link_options(bt2-bootstrap-tests PRIVATE /STACK:268435456)
    add_test(NAME bootstrap-runtime COMMAND bt2-bootstrap-tests)
    add_executable(bt2-ee-stack-tests runtime/tests/retained_ee_stack_tests.cpp runtime/ee/retained_ee_stack.cpp)
    bt2_ee_target(bt2-ee-stack-tests)
    add_test(NAME retained-ee-stack COMMAND bt2-ee-stack-tests)
    add_executable(bt2-gs-command-tests runtime/tests/gs_command_tests.cpp)
    bt2_ee_target(bt2-gs-command-tests)
    add_test(NAME gs-command-consumption COMMAND bt2-gs-command-tests)
    add_executable(bt2-texture-names-tests runtime/tests/texture_names_tests.cpp runtime/gs/textures/gs_texture_names.cpp)
    bt2_ee_target(bt2-texture-names-tests)
    target_include_directories(bt2-texture-names-tests PRIVATE "${BT2_XXHASH_DIR}")
    add_test(NAME texture-names COMMAND bt2-texture-names-tests)
    add_executable(bt2-ipu-tests runtime/tests/ipu_tests.cpp runtime/ee/ipu_device.cpp runtime/ee/ipu_input.cpp runtime/ee/ipu_vlc.cpp runtime/ee/ipu_block.cpp runtime/ee/ipu_csc.cpp)
    bt2_target_defaults(bt2-ipu-tests)
    add_test(NAME ipu-setup COMMAND bt2-ipu-tests)
    add_executable(bt2-native-services-tests runtime/tests/native_services_tests.cpp)
    bt2_target_defaults(bt2-native-services-tests)
    add_test(NAME native-services COMMAND bt2-native-services-tests)
    add_executable(bt2-host-input-tests runtime/tests/host_input_tests.cpp)
    bt2_target_defaults(bt2-host-input-tests)
    add_test(NAME host-input COMMAND bt2-host-input-tests)
    add_executable(bt2-menu-tests runtime/tests/menu_tests.cpp)
    bt2_target_defaults(bt2-menu-tests)
    # The launcher's tests build its screens (host input, the logo) and read the logo from the sources.
    target_link_libraries(bt2-menu-tests PRIVATE winmm xinput hid setupapi windowscodecs)
    target_compile_definitions(bt2-menu-tests PRIVATE "BT2_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")
    add_test(NAME menu COMMAND bt2-menu-tests)
    add_executable(bt2-card-folder-tests runtime/tests/card_folder_tests.cpp)
    bt2_target_defaults(bt2-card-folder-tests)
    add_test(NAME card-folder COMMAND bt2-card-folder-tests)
    # The player's programs (docs/plans/launcher/LAUNCHER_PLAN.md): settings and launcher, then the
    # runtime program as a child process. Small and quick to build; they hold no game code.
    foreach(bt2_player_program bt2-game bt2-launcher)
        add_executable(${bt2_player_program} WIN32 runtime/game/game_main.cpp)
        bt2_target_defaults(${bt2_player_program})
        target_link_libraries(${bt2_player_program} PRIVATE winmm xinput hid setupapi windowscodecs)
        # The launcher's logo goes into the assets folder next to the program, so it is always there.
        add_custom_command(TARGET ${bt2_player_program} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${bt2_player_program}>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/launcher/logo.png" "$<TARGET_FILE_DIR:${bt2_player_program}>/assets/logo.png")
    endforeach()
    target_compile_definitions(bt2-launcher PRIVATE BT2_LAUNCHER_ALWAYS=1)
    add_executable(bt2-vif1-blend-tests runtime/tests/vif1_blend_tests.cpp runtime/vu/vif1_blend.cpp)
    bt2_target_defaults(bt2-vif1-blend-tests)
    add_test(NAME vif1-blend COMMAND bt2-vif1-blend-tests)
    add_executable(bt2-game-patches-tests runtime/tests/game_patches_tests.cpp runtime/ee/game_patches.cpp)
    bt2_target_defaults(bt2-game-patches-tests)
    add_test(NAME game-patches COMMAND bt2-game-patches-tests)
    add_executable(bt2-dma-chain-tests runtime/tests/dma_chain_tests.cpp runtime/ee/dma_chain.cpp)
    bt2_target_defaults(bt2-dma-chain-tests)
    add_test(NAME dma-chain COMMAND bt2-dma-chain-tests)
    add_executable(bt2-vu1-tests runtime/tests/vu1_tests.cpp runtime/vu/vu1.cpp runtime/vu/vu1_handwritten_registry.cpp runtime/vu/vu1_skinned_vertex_programs.cpp runtime/vu/vu1_triangle_strip_program.cpp runtime/vu/vu1_triangle_clipper.cpp runtime/vu/vif1.cpp runtime/vu/vu1_native_none.cpp)
    bt2_target_defaults(bt2-vu1-tests)
    # Its main keeps many Vu1 objects (each over 100 KB with the decoded-pair table) as locals.
    target_link_options(bt2-vu1-tests PRIVATE /STACK:67108864)
    add_test(NAME vu1 COMMAND bt2-vu1-tests)
    # GPU GS through paraLLEl-GS (docs/plans/graphics/GPU_RENDERING_PLAN.md); fetch it with
    # tools/graphics/gpu/fetch_parallel_gs.py. Off until it matches the CPU model.
    option(BT2_GPU_GS "Build the paraLLEl-GS backend and its tests" OFF)
    if(BT2_GPU_GS)
        set(BT2_PARALLEL_GS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/work/dependencies/parallel-gs-cc6184af7e0c" CACHE PATH "Pinned paraLLEl-GS checkout")
        if(NOT EXISTS "${BT2_PARALLEL_GS_DIR}/gs/gs_interface.hpp")
            message(FATAL_ERROR "Run tools/graphics/gpu/fetch_parallel_gs.py first.")
        endif()
        set(PARALLEL_GS_STANDALONE ON CACHE BOOL "" FORCE)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL") # Match our targets.
        add_subdirectory("${BT2_PARALLEL_GS_DIR}" parallel-gs EXCLUDE_FROM_ALL)
        add_library(bt2-gpu-gs STATIC runtime/gs/gpu/gpu_gs_backend.cpp runtime/gs/gpu/gpu_gs_mirror.cpp runtime/gs/gpu/hd_texture_pack.cpp runtime/gs/gpu/gpu_window_presenter.cpp)
        bt2_target_defaults(bt2-gpu-gs)
        target_link_libraries(bt2-gpu-gs PRIVATE parallel-gs)
        add_executable(bt2-gpu-gs-tests runtime/tests/gpu_gs_tests.cpp runtime/gs/cpu/gs_command_sink.cpp runtime/gs/textures/gs_texture_names.cpp)
        target_include_directories(bt2-gpu-gs-tests PRIVATE "${BT2_XXHASH_DIR}")
        bt2_ee_target(bt2-gpu-gs-tests)
        target_link_libraries(bt2-gpu-gs-tests PRIVATE bt2-gpu-gs)
        add_test(NAME gpu-gs COMMAND bt2-gpu-gs-tests)
        set_tests_properties(gpu-gs PROPERTIES SKIP_RETURN_CODE 77)
    endif()
    add_executable(bt2-post-sif-tests runtime/tests/post_reset_sif_tests.cpp)
    bt2_ee_target(bt2-post-sif-tests)
    add_test(NAME post-reset-sif COMMAND bt2-post-sif-tests)
    # Single generated functions checked against handwritten expectations.
    function(bt2_generated_test target test_name source strict)
        add_executable(${target} runtime/tests/${source} ${ARGN})
        bt2_ee_target(${target})
        if(strict)
            target_compile_definitions(${target} PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
        endif()
        add_test(NAME ${test_name} COMMAND ${target})
    endfunction()
    bt2_generated_test(bt2-kernel-handler-tests generated-kernel-handler kernel_handler_tests.cpp ON
        "${BT2_GENERATED_DIR}/FUN_00338388_0x338388.cpp")
    bt2_generated_test(bt2-buffer-fill-tests generated-buffer-fill buffer_fill_tests.cpp ON
        "${BT2_GENERATED_DIR}/FUN_003251DC_0x3251dc.cpp")
    bt2_generated_test(bt2-timer-write-tests generated-timer-write timer_write_tests.cpp OFF
        "${BT2_GENERATED_DIR}/FUN_00338AD8_0x338ad8.cpp")
    bt2_generated_test(bt2-copy-handler-tests generated-copy-handler copy_handler_tests.cpp ON
        "${BT2_GENERATED_DIR}/FUN_0033A7C8_0x33a7c8.cpp")
    bt2_generated_test(bt2-compare-bytes-tests generated-byte-comparison compare_bytes_tests.cpp ON
        "${BT2_GENERATED_DIR}/FUN_00325098_0x325098.cpp" "${BT2_GENERATED_DIR}/FUN_00326270_0x326270.cpp")
    # Independent response validation; no entry-path interrupt delivery.
    add_executable(bt2-rpc-bind-probe runtime/probe/rpc_bind_probe.cpp
        "${BT2_GENERATED_DIR}/FUN_003322E8_0x3322e8.cpp"
        "${BT2_GENERATED_DIR}/FUN_00332258_0x332258.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032EC90_0x32ec90.cpp")
    bt2_ee_target(bt2-rpc-bind-probe)
    target_compile_definitions(bt2-rpc-bind-probe PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    # Compile/link entry separately so future blockers do not break the independent host/probe.
    add_executable(bt2-entry-probe EXCLUDE_FROM_ALL ${BT2_ENTRY_PROBE_SOURCES})
    # The player's programs start this runtime and share its settings file: building them builds it,
    # so the two are never from different sources.
    foreach(bt2_player_program bt2-game bt2-launcher)
        if(TARGET ${bt2_player_program})
            add_dependencies(${bt2_player_program} bt2-entry-probe)
        endif()
    endforeach()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/extra-startup.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/extra-startup.cmake")
        target_include_directories(bt2-entry-probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation")
        target_compile_definitions(bt2-entry-probe PRIVATE BT2_EXTRA_STARTUP_BINDINGS=1)
    endif()
    # Reached entries of runtime-loaded overlays, each bound to the bytes it was generated from.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/overlays.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/overlays.cmake")
        target_include_directories(bt2-entry-probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation")
        target_compile_definitions(bt2-entry-probe PRIVATE BT2_OVERLAY_BINDINGS=1)
    endif()
    # The EE interpreter's handlers (tools/ps2recomp/generate_interpreter.py, local and ignored); without
    # them dispatch to code no generated function holds stops as before.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/ee-interpreter/ee_interpreter.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/game/generated/ee-interpreter/ee_interpreter.cmake")
        message(STATUS "EE interpreter: handlers from game/generated/ee-interpreter")
    else()
        target_sources(bt2-entry-probe PRIVATE runtime/probe/interpreter_handlers_none.cpp)
    endif()
    option(BT2_BUILD_KERNEL_PAYLOAD "Link the scoped ignored copied-kernel entry" OFF)
    if(BT2_BUILD_KERNEL_PAYLOAD)
        set(BT2_KERNEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-kernel-runtime")
        target_sources(bt2-entry-probe PRIVATE "${BT2_KERNEL_DIR}/FUN_80076000_0x80076000.cpp")
        target_include_directories(bt2-entry-probe PRIVATE "${BT2_KERNEL_DIR}")
        target_compile_definitions(bt2-entry-probe PRIVATE BT2_KERNEL_PAYLOAD=1)
    endif()
    option(BT2_BUILD_STARTUP_KERNEL_PAYLOAD "Link the scoped ignored startup kernel lookup" OFF)
    if(BT2_BUILD_STARTUP_KERNEL_PAYLOAD)
        if(NOT BT2_BUILD_KERNEL_PAYLOAD)
            message(FATAL_ERROR "Startup kernel payload requires primary kernel payload")
        endif()
        set(BT2_STARTUP_KERNEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-kernel-runtime-startup")
        target_sources(bt2-entry-probe PRIVATE "${BT2_STARTUP_KERNEL_DIR}/FUN_80075000_0x80075000.cpp")
        target_include_directories(bt2-entry-probe PRIVATE "${BT2_STARTUP_KERNEL_DIR}")
        target_compile_definitions(bt2-entry-probe PRIVATE BT2_STARTUP_KERNEL_PAYLOAD=1)
    endif()
    target_link_libraries(bt2-entry-probe PRIVATE bt2-ee-runtime winmm xinput hid setupapi)
    # Function addresses and line information for tools/perf/profile_report.py; the optimizations /DEBUG would turn off stay on.
    target_link_options(bt2-entry-probe PRIVATE /MAP $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
    bt2_ee_target(bt2-entry-probe)
    if(BT2_GPU_GS)
        target_link_libraries(bt2-entry-probe PRIVATE bt2-gpu-gs)
        target_sources(bt2-entry-probe PRIVATE runtime/gs/gpu/in_between_frames.cpp) # Needs VIF1/VU1 and the GS sink as well.
        target_compile_definitions(bt2-entry-probe PRIVATE BT2_GPU_GS=1)
    endif()
    target_compile_definitions(bt2-entry-probe PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    # Build speed only: a full link of the 10,000 generated objects takes about a minute, so Release links
    # incrementally as Debug does (which rules out the linker's unreferenced-code and folding passes).
    target_link_options(bt2-entry-probe PRIVATE $<$<CONFIG:Release>:/INCREMENTAL /OPT:NOREF /OPT:NOICF>)
    # Build speed only: precompile the stable standard headers that every generated unit reaches through
    # ps2_runtime.h, and that the runtime itself uses. Project and generated headers (ps2_runtime.h, generated
    # declarations) change often and stay out of the PCH; unity builds are not used.
    target_precompile_headers(bt2-entry-probe PRIVATE
        <algorithm> <array> <bit> <cstdint> <cstdlib> <cstring> <functional> <iostream> <map> <memory>
        <optional> <set> <span> <stdexcept> <string> <unordered_map> <vector> <immintrin.h>)
    add_test(NAME generated-entry-probe COMMAND bt2-entry-probe
        "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/guest-memory.bin"
        "${CMAKE_CURRENT_BINARY_DIR}/ctest-entry-stop.json") # Its own report: the gate's runs own work/bt2-generation.
endif()

if(BT2_BUILD_GENERATED_PROBE)
    add_executable(bt2-disc-sector-tests runtime/tests/disc_sector_tests.cpp)
    bt2_target_defaults(bt2-disc-sector-tests)
    add_test(NAME disc-sector-bounds COMMAND bt2-disc-sector-tests)
endif()

foreach(bt2_ee_runtime_target bt2-bootstrap-tests bt2-generated-probe bt2-gs-command-tests bt2-post-sif-tests bt2-kernel-handler-tests bt2-buffer-fill-tests bt2-timer-write-tests bt2-copy-handler-tests bt2-compare-bytes-tests bt2-rpc-bind-probe)
    if(TARGET ${bt2_ee_runtime_target})
        target_link_libraries(${bt2_ee_runtime_target} PRIVATE bt2-ee-runtime)
    endif()
endforeach()

# Build speed only: the probe build uses Ninja (docs/guides/BUILDING.md), which compiles in parallel by
# itself. A Visual Studio generator compiles a project's sources one at a time unless the compiler is
# asked to parallelize, so every MSVC target here also carries /MP.
get_property(bt2_all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(bt2_target IN LISTS bt2_all_targets)
    get_target_property(bt2_target_type ${bt2_target} TYPE)
    if(bt2_target_type STREQUAL "EXECUTABLE")
        target_compile_options(${bt2_target} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
    endif()
endforeach()

# The entry probe's generated game code is its own object library, apart from the handwritten runtime:
# - it has its own small precompiled header (the standard headers ps2_runtime.h needs);
# - Debug compiles it optimized, as Release does, so the gate's Debug probe runs take a fraction of the
#   time (together with the hot runtime sources above). Other handwritten code keeps the full Debug
#   settings. Its runtime checks (/RTC1) cannot be combined with optimization, so they move from the
#   configuration's flags to each handwritten target.
string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
foreach(bt2_target IN LISTS bt2_all_targets)
    get_target_property(bt2_target_type ${bt2_target} TYPE)
    if(bt2_target_type STREQUAL "EXECUTABLE" OR bt2_target_type STREQUAL "STATIC_LIBRARY")
        target_compile_options(${bt2_target} PRIVATE $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/RTC1>)
    endif()
endforeach()
if(TARGET bt2-entry-probe)
    get_target_property(bt2_handwritten bt2-entry-probe SOURCES)
    set(bt2_generated ${bt2_handwritten})
    list(FILTER bt2_generated INCLUDE REGEX "/game/generated/")
    list(FILTER bt2_handwritten EXCLUDE REGEX "/game/generated/")
    set_property(TARGET bt2-entry-probe PROPERTY SOURCES ${bt2_handwritten})
    add_library(bt2-entry-generated OBJECT ${bt2_generated})
    target_compile_features(bt2-entry-generated PRIVATE cxx_std_20)
    target_compile_options(bt2-entry-generated PRIVATE /W4 /permissive- /utf-8 /MP $<$<CONFIG:Debug>:/O2 /Ob2>)
    target_include_directories(bt2-entry-generated PRIVATE $<TARGET_PROPERTY:bt2-entry-probe,INCLUDE_DIRECTORIES>)
    target_compile_definitions(bt2-entry-generated PRIVATE $<TARGET_PROPERTY:bt2-entry-probe,COMPILE_DEFINITIONS>)
    target_precompile_headers(bt2-entry-generated PRIVATE <array> <bit> <cstdint> <cstring> <stdexcept> <immintrin.h>)
    target_link_libraries(bt2-entry-probe PRIVATE bt2-entry-generated)
endif()
