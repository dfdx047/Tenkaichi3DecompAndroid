#pragma once
#include <cstdint>
#include <fstream>
#include <span>
#include <string>
#include <vector>

// Hash of the guest state at the end of every field (WHOLE_GAME_COVERAGE_PLAN.md, step 2): two runs
// of the same input that differ show the first field at which they do, and the 4 KB pages that
// differ there. Used to check that a change of the runtime alters nothing the game can see.
//
// Each region (EE RAM, scratchpad, VU memories) is hashed per 4 KB page; the field's hash is the hash
// of all page hashes. One line per field: "<field> <flips> <hash>". At the field `pages_at` (when not
// zero) every page hash is written to "<path>.pages": "<region> <page> <hash>".
class StateHashLog {
public:
    struct Region {const char* name; std::span<const std::uint8_t> bytes;};
    static constexpr std::size_t page_size = 4096;

    StateHashLog(const std::string& path, unsigned pages_at);
    StateHashLog(const StateHashLog&) = delete;
    StateHashLog& operator=(const StateHashLog&) = delete;

    // The state at the end of `field`; returns the field's hash.
    std::uint64_t field(unsigned field, unsigned flips, std::span<const Region> regions);

    static std::uint64_t hashBytes(std::span<const std::uint8_t> bytes, std::uint64_t seed);

private:
    std::ofstream log_;
    std::string pages_path_;
    unsigned pages_at_ = 0;
    std::vector<std::uint64_t> page_hashes_;
};
