#pragma once
#include "ee/ee_runtime.h"
#include <cstdint>
#include <string>
#include <string_view>

// Tool runs only (WHOLE_GAME_COVERAGE_PLAN.md, step 6): game states started without input. The
// overlay's entry, which runs the title and the menus, is replaced on its first call. The replacement
// does the part of the entry's first-run work later screens and battles need (mounts the second archive
// and loads the common files; no card check, logos or films), then:
// - BT2_START_BATTLE: sets a CPU-against-CPU battle up with the setter calls the title's demo fight
//   makes (characters, variants, stage, music; CPU on both sides) and the versus settings, and returns,
//   so main runs its battle routine;
// - BT2_START_SCREEN: writes the mode the main menu would and runs the original entry, which shows
//   that screen; a battle the screen starts has the CPU on both sides, except in Ultimate Training,
//   where side 0 stays with the pad and the starter takes the 21 lessons in turn and clears each
//   lesson's practice through the game's task report;
// - BT2_START_EPISODE: writes the story episode and the flag that makes main run the story routine,
//   and returns.
// Later calls run the original entry.
struct BattleStart {
    std::uint32_t character[2]{};
    std::uint32_t variant[2]{};
    std::uint32_t stage = 0;
    std::uint32_t music = 9; // The music the versus menus chose with Wasteland.
};

// "<character>[:<variant>],<character>[:<variant>],<stage>[,<music>]", decimal; throws on other text.
BattleStart parseBattleStart(std::string_view text);
void installBattleStarter(EeRuntime& runtime, const BattleStart& start);

// A mode of the overlay's mode table, 1 to 64, decimal; throws on other text.
std::uint32_t parseScreenStart(std::string_view text);
void installScreenStarter(EeRuntime& runtime, std::uint32_t mode);

// BT2_START_EPISODE=<episode>[,<part>[,marker:<m>]]: the story routine with that episode, as after its opening
// (STORY_MODE_PLAN.md). Decimal; throws on other text.
struct EpisodeStart {
    std::uint32_t episode = 0;
    std::uint32_t part = 0;
    std::uint32_t marker = 0; // ",marker:<m>" (1 to 25): the player is reported at that map marker once.
};
EpisodeStart parseEpisodeStart(std::string_view text);
// Gives both sides of the story's battles to the CPU. Also answers story mode's waits for a button (message boxes, the pre-battle screen, the results, the
// save questions: no save is written) and ends the run 300 flips after the part's after-battle step (results, and the save flow after a win).
void installEpisodeStarter(EeRuntime& runtime, const EpisodeStart& start);

// The report's fields for the episode starter: parts completed and presses per waiting function.
std::string gameStarterReport();
