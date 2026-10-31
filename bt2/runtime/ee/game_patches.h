// Optional patches of the game's executable, chosen by the player (docs/plans/graphics/IMAGE_QUALITY_PLAN.md):
// words written into guest memory after the executable is loaded. An instruction a patch changes is
// read from guest memory by the generated code (tools/ps2recomp/prepare_runtime.py, the words listed in
// config/game_patches.json), so a patch is a memory write for compiled and interpreted code alike.
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct GamePatchWord {std::uint32_t address,original,patched;};
struct GamePatch {std::string_view name,description;std::span<const GamePatchWord> words;};

std::span<const GamePatch> gamePatches();
// The patch of that name; an unknown name stops with the names there are.
const GamePatch& gamePatch(std::string_view name);
// Writes the patch's words into main RAM after checking each one still holds its original word,
// so a patch never lands on another build of the game. Words already patched are left as they are.
void applyGamePatch(std::span<std::uint8_t> memory,const GamePatch& patch);
// A comma-separated list of names (BT2_GAME_PATCHES), each applied once; an empty list applies none.
std::vector<const GamePatch*> parseGamePatches(std::string_view list);
