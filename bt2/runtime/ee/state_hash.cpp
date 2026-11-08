#include "state_hash.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
constexpr std::uint64_t mix_prime = 0x9E3779B97F4A7C15ull;

std::uint64_t mix(std::uint64_t value) {
    value ^= value >> 33; value *= 0xFF51AFD7ED558CCDull;
    value ^= value >> 33; value *= 0xC4CEB9FE1A85EC53ull;
    return value ^ (value >> 33);
}
}

std::uint64_t StateHashLog::hashBytes(std::span<const std::uint8_t> bytes, std::uint64_t seed) {
    // Four independent lanes over 8-byte words, then the tail byte by byte: fast enough to hash
    // 32 MB per field, and every byte and its position change the result.
    std::uint64_t lane[4] = {seed ^ mix_prime, seed + mix_prime, ~seed, seed * mix_prime + 1};
    std::size_t at = 0;
    for (; at + 32 <= bytes.size(); at += 32) {
        for (unsigned n = 0; n < 4; ++n) {
            std::uint64_t word;
            std::memcpy(&word, bytes.data() + at + n * 8, 8);
            lane[n] = (lane[n] ^ word) * mix_prime;
            lane[n] ^= lane[n] >> 29;
        }
    }
    std::uint64_t result = mix(lane[0]) ^ mix(lane[1] + 1) ^ mix(lane[2] + 2) ^ mix(lane[3] + 3);
    for (; at < bytes.size(); ++at) result = (result ^ bytes[at]) * mix_prime;
    return mix(result ^ bytes.size());
}

StateHashLog::StateHashLog(const std::string& path, unsigned pages_at)
    : log_(path), pages_path_(path + ".pages"), pages_at_(pages_at) {
    if (!log_) throw std::runtime_error("Cannot write the state hash log " + path);
}

std::uint64_t StateHashLog::field(unsigned field, unsigned flips, std::span<const Region> regions) {
    page_hashes_.clear();
    for (std::size_t r = 0; r < regions.size(); ++r) {
        const auto bytes = regions[r].bytes;
        for (std::size_t at = 0; at < bytes.size(); at += page_size)
            page_hashes_.push_back(hashBytes(bytes.subspan(at, std::min(page_size, bytes.size() - at)), r));
    }
    const auto hash = hashBytes({reinterpret_cast<const std::uint8_t*>(page_hashes_.data()), page_hashes_.size() * 8}, field);
    char line[96];
    std::snprintf(line, sizeof line, "%u %u %016llx\n", field, flips, static_cast<unsigned long long>(hash));
    log_ << line;
    if (field % 60 == 0) log_.flush(); // (a phone's run ends when its app is closed, without the destructor)
    if (pages_at_ && field == pages_at_) {
        std::ofstream pages(pages_path_);
        if (!pages) throw std::runtime_error("Cannot write the state hash pages " + pages_path_);
        std::size_t index = 0;
        for (const auto& region : regions)
            for (std::size_t at = 0; at < region.bytes.size(); at += page_size) {
                std::snprintf(line, sizeof line, "%s %zx %016llx\n", region.name, at / page_size, static_cast<unsigned long long>(page_hashes_[index++]));
                pages << line;
            }
    }
    return hash;
}
