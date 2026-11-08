#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include "services/devices/disc_metadata.h"
#include "services/devices/disc_sectors.h"

// The code of a runtime-loaded overlay (DBZP.BIN), read from the player's own disc image when the engine starts.
// Each generated overlay function is dispatchable only while main RAM holds the bytes it was generated from
// (EeRuntime::registerOverlayFunction); those bytes used to be copied into the library, which made the library
// carry a copy of the disc's file. Now the library holds only the image's size and hash, and the bytes come from
// the disc: a disc whose file differs (another release, a modified image) is refused here, with a message, instead
// of running code made for other bytes. The per-call check in dispatch is the same as before.

// 64-bit FNV-1a: identifies the image (tools/ps2recomp/expand_startup.py computes the same value).
inline std::uint64_t overlayImageHash(std::span<const std::uint8_t> bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const std::uint8_t byte : bytes) {hash ^= byte; hash *= 0x100000001b3ull;}
    return hash;
}

struct OverlayImage {
    std::uint32_t base = 0;
    std::vector<std::uint8_t> bytes;

    // The image's bytes for the guest range [start, end).
    std::span<const std::uint8_t> code(std::uint32_t start, std::uint32_t end) const {
        if (start < base || end <= start || end - base > bytes.size())
            throw std::runtime_error("Overlay function outside its image");
        return std::span<const std::uint8_t>(bytes).subspan(start - base, end - start);
    }
};

// `path` is the file's place on the disc ("\\BIN\\DBZP.BIN;1"); `size` and `hash` are the generated image's.
inline OverlayImage loadOverlayImage(const std::string& disc, const std::string& path, std::uint32_t base,
                                     std::uint32_t size, std::uint64_t hash) {
    if (disc.empty())
        throw std::runtime_error("The overlay's code is read from the disc image, and none is set");
    const auto record = lookup_disc_file(disc, path);
    if (!record) throw std::runtime_error("This disc image has no " + path + ": it is not Budokai Tenkaichi 2 (USA)");
    std::uint32_t lba, length;
    std::memcpy(&lba, record->data(), 4);
    std::memcpy(&length, record->data() + 4, 4);
    const auto refused = [&] {
        return std::runtime_error(path + " on this disc is not the one the engine was generated from "
                                  "(Budokai Tenkaichi 2, USA, SLUS-21441; other releases and modified images do not run)");
    };
    if (length != size) throw refused(); // (before reading: the disc's length is not trusted)
    OverlayImage image{base, std::vector<std::uint8_t>(length)};
    if (!read_iso_bytes(disc, std::uint64_t(lba) * 2048, reinterpret_cast<char*>(image.bytes.data()), length)
            || overlayImageHash(image.bytes) != hash)
        throw refused();
    return image;
}
