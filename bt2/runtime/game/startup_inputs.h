// The runtime's startup inputs made from the player's own disc image, without PCSX2, a BIOS or a capture
// (docs/guides/ANDROID.md, "Startup inputs without captures"). The entry probe takes them in this order
// (runtime/game/game_main.cpp, tools/ps2recomp/run_startup_probe.py).
//
// What each holds:
//   guest-memory.bin       the game's program (SLUS_214.41 on the disc) loaded into 32 MB of main RAM,
//                          as tools/ps2recomp/prepare_runtime.py builds it
//   most others            the one value the runtime's checks accept (ee/ee_runtime.h, the supply*
//                          functions): a captured word or bundle is replayed only when it is exactly the
//                          validated observation, so the observation is written here
//   sif-dma, sif-bind      2 MB of IOP memory each, of which the runtime reads only the checked words;
//                          the rest is zero
//   the three GS packets   in scmd-bind (two GIF packets) and texture-startup (one): bytes the game itself
//                          builds in RAM. They are zero here, which the runtime takes as "not captured":
//                          with BT2_SELF_STARTUP it accepts the packet the game built (gs/gif_dma_staging.h,
//                          gs/texture_gif_startup.h)
//   kernel-snapshot.bin    the BIOS kernel's memory: zero (no BIOS here); with BT2_SELF_STARTUP the runtime
//                          logs each kernel word the game reads from it
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

constexpr std::array<const char*,22> startup_input_names{"guest-memory.bin","kernel-snapshot.bin","timer3-mode.bin","timer2-mode.bin",
    "intc-mask.bin","timer2-readback.bin","osd-config.bin","osd-readback.bin","dma-status.bin","dma5-control.bin","sif-command.bin","sif-dma.bin",
    "sif-rpc-state.bin","sif-bind.bin","cd-init.bin","scmd-bind.bin","heap-bind.bin","heap-allocation.bin","texture-startup.bin",
    "ee-thread-start.bin","disc-bind.bin","ncmd-bind.bin"};

// The game's program from a disc image (\SLUS_214.41;1).
std::vector<std::uint8_t> readGameProgram(const std::filesystem::path& disc);
// The program's PT_LOAD segments in 32 MB of zeroed main RAM.
std::vector<std::uint8_t> buildGuestMemory(std::span<const std::uint8_t> program);
// Writes every startup input into `folder` (made when missing) for `program`.
void writeStartupInputs(const std::filesystem::path& folder,std::span<const std::uint8_t> program);
