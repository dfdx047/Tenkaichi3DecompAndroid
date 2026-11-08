// A host gamepad's state in one neutral form, whatever reads it (XInput, a DualSense over HID).
// Buttons are named by position, the way an Xbox pad labels them; `names` are what a settings file
// and the controls screen use.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>

struct GamepadState {
    enum Button : unsigned {South,East,West,North,LeftShoulder,RightShoulder,LeftTrigger,RightTrigger,Back,Start,
        LeftStick,RightStick,Up,Down,Left,Right,Guide,Count};
    static constexpr std::array<std::string_view,Count> names{"south","east","west","north","left_shoulder","right_shoulder",
        "left_trigger","right_trigger","back","start","left_stick","right_stick","up","down","left","right","guide"};
    // What the screens call them: the Xbox name and the PlayStation name.
    static constexpr std::array<std::string_view,Count+1> labels{"Cross (A)","Circle (B)","Square (X)","Triangle (Y)","L1 (LB)","R1 (RB)","L2 (LT)","R2 (RT)",
        "Select (Back)","Start","L3","R3","D-pad up","D-pad down","D-pad left","D-pad right","PS (Guide)","none"};
    bool connected=false;
    std::uint32_t buttons=0;                 // Bit per Button.
    std::uint8_t lx=0x80,ly=0x80,rx=0x80,ry=0x80; // 0 is left or up, 0xFF right or down.
    bool held(Button b) const {return buttons>>b&1;}
    bool operator==(const GamepadState&) const=default;
};
