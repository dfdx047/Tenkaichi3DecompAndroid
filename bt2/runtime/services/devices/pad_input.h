// What a controller holds at one field: the pressed buttons as an active-high mask in the order of
// `PadScript::names` (the wire order of a poll reply) and the stick positions, 0x80 at rest.
#pragma once
#include <array>
#include <cstdint>

struct PadInput {
    std::uint16_t pressed=0;
    std::array<std::uint8_t,4> sticks{0x80,0x80,0x80,0x80}; // RX, RY, LX, LY.
    bool operator==(const PadInput&) const=default;
};
