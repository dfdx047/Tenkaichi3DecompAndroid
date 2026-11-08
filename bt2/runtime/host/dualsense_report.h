// A DualSense's HID input report as a GamepadState. The layout follows the controller's publicly
// documented reports:
// - USB, report 0x01 (64 bytes): sticks at 1..4, triggers at 5 and 6, buttons at 8..10.
// - Bluetooth, report 0x31 (78 bytes): the same fields one byte later.
// - Bluetooth before the full report is enabled, report 0x01: sticks at 1..4, buttons at 5..7,
//   triggers at 8 and 9.
// Button bytes: the first holds the d-pad as a direction (0 up, clockwise, 8 none) in its low nibble
// and square, cross, circle, triangle above; the second L1, R1, L2, R2, create, options, L3, R3; the
// third the PS button in bit 0.
#pragma once
#include "host/gamepad.h"
#include <cstdint>
#include <optional>
#include <span>

// `bluetooth` tells which transport the report came over (the device's report length says so).
// Returns nothing for a report that is not an input report of the layouts above.
inline std::optional<GamepadState> parseDualSenseReport(std::span<const std::uint8_t> report,bool bluetooth) {
    std::size_t sticks=0,buttons=0;
    if(report.empty())return std::nullopt;
    if(!bluetooth && report[0]==0x01) {sticks=1;buttons=8;}
    else if(bluetooth && report[0]==0x31) {sticks=2;buttons=9;}
    else if(bluetooth && report[0]==0x01) {sticks=1;buttons=5;}
    else return std::nullopt;
    if(report.size()<buttons+3)return std::nullopt;
    GamepadState s;s.connected=true;
    s.lx=report[sticks];s.ly=report[sticks+1];s.rx=report[sticks+2];s.ry=report[sticks+3];
    const std::uint8_t b0=report[buttons],b1=report[buttons+1],b2=report[buttons+2];
    const auto set=[&](GamepadState::Button b,bool on){if(on)s.buttons|=1u<<b;};
    const unsigned hat=b0&15;
    set(GamepadState::Up,hat==7 || hat==0 || hat==1);set(GamepadState::Right,hat>=1 && hat<=3);
    set(GamepadState::Down,hat>=3 && hat<=5);set(GamepadState::Left,hat>=5 && hat<=7);
    set(GamepadState::West,b0&0x10);set(GamepadState::South,b0&0x20);set(GamepadState::East,b0&0x40);set(GamepadState::North,b0&0x80);
    set(GamepadState::LeftShoulder,b1&0x01);set(GamepadState::RightShoulder,b1&0x02);set(GamepadState::LeftTrigger,b1&0x04);set(GamepadState::RightTrigger,b1&0x08);
    set(GamepadState::Back,b1&0x10);set(GamepadState::Start,b1&0x20);set(GamepadState::LeftStick,b1&0x40);set(GamepadState::RightStick,b1&0x80);
    set(GamepadState::Guide,b2&0x01);
    return s;
}
