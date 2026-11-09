#pragma once
#include <cstdint>
#include <optional>

// The memory card service's GetInfo answer (MCSERV function 1), by port. Its request uses the public
// mcDescParam layout: the descriptor word, then port and slot. The probe and the player programs put
// one card folder in port 0; any other port is empty. The original answered an empty port with -11
// (no card), type 0 and free space 0; a card folder answers a formatted PS2 card (type 2, its free
// clusters, the format flag), -1 on the first query after it appeared and 0 later.
struct CardInfo {
    std::uint32_t reply = 0, type = 0, free_clusters = 0, formatted = 0;
};

// Port and slot as the request names them; nullopt for a request this service does not answer (a slot
// other than 0, which needs a multitap, or a port above 1).
inline std::optional<CardInfo> cardInfo(std::uint32_t port, std::uint32_t slot, bool folder_in_port0, bool reported_before,
                                        std::uint32_t free_clusters) {
    if (slot || port > 1) return std::nullopt;
    if (port != 0 || !folder_in_port0) return CardInfo{static_cast<std::uint32_t>(-11), 0, 0, 0};
    return CardInfo{reported_before ? 0u : static_cast<std::uint32_t>(-1), 2, free_clusters, 1};
}
