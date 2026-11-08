// Host input through SDL3 (Android, Linux): the first connected gamepad and the keyboard, in the same
// neutral form as the Win32 reader (host/host_input.h). Keys are asked by Win32 virtual-key code, as the
// settings file stores them (host/input_mapping.h), and looked up as SDL scancodes. SDL's events are
// pumped by the window (host/sdl/sdl_viewer_window.h), which keeps the states read here current.
#pragma once
#include "host/gamepad.h"
#include <SDL3/SDL.h>
#include <cstdint>

class HostInput {
public:
    HostInput() {SDL_InitSubSystem(SDL_INIT_GAMEPAD);}
    HostInput(const HostInput&)=delete;
    HostInput& operator=(const HostInput&)=delete;
    ~HostInput() {if(pad_)SDL_CloseGamepad(pad_);SDL_QuitSubSystem(SDL_INIT_GAMEPAD);}

    // The first connected gamepad, otherwise a disconnected state.
    GamepadState gamepad() {
        if(pad_ && !SDL_GamepadConnected(pad_)) {SDL_CloseGamepad(pad_);pad_=nullptr;}
        if(!pad_) {
            int count=0;
            if(SDL_JoystickID* ids=SDL_GetGamepads(&count)) {
                if(count>0)pad_=SDL_OpenGamepad(ids[0]);
                SDL_free(ids);
            }
            if(!pad_)return {};
        }
        GamepadState s;s.connected=true;
        const auto set=[&](GamepadState::Button b,bool on){if(on)s.buttons|=1u<<b;};
        const auto button=[&](SDL_GamepadButton b){return SDL_GetGamepadButton(pad_,b);};
        set(GamepadState::South,button(SDL_GAMEPAD_BUTTON_SOUTH));set(GamepadState::East,button(SDL_GAMEPAD_BUTTON_EAST));
        set(GamepadState::West,button(SDL_GAMEPAD_BUTTON_WEST));set(GamepadState::North,button(SDL_GAMEPAD_BUTTON_NORTH));
        set(GamepadState::LeftShoulder,button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));set(GamepadState::RightShoulder,button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));
        // Triggers: past about a quarter of their travel, as XInput's threshold (30 of 255).
        set(GamepadState::LeftTrigger,SDL_GetGamepadAxis(pad_,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)>trigger_threshold);
        set(GamepadState::RightTrigger,SDL_GetGamepadAxis(pad_,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)>trigger_threshold);
        set(GamepadState::Back,button(SDL_GAMEPAD_BUTTON_BACK));set(GamepadState::Start,button(SDL_GAMEPAD_BUTTON_START));
        set(GamepadState::LeftStick,button(SDL_GAMEPAD_BUTTON_LEFT_STICK));set(GamepadState::RightStick,button(SDL_GAMEPAD_BUTTON_RIGHT_STICK));
        set(GamepadState::Up,button(SDL_GAMEPAD_BUTTON_DPAD_UP));set(GamepadState::Down,button(SDL_GAMEPAD_BUTTON_DPAD_DOWN));
        set(GamepadState::Left,button(SDL_GAMEPAD_BUTTON_DPAD_LEFT));set(GamepadState::Right,button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT));
        set(GamepadState::Guide,button(SDL_GAMEPAD_BUTTON_GUIDE));
        // SDL's axes are signed with down positive; the neutral form is 0..255 with up at 0.
        const auto axis=[&](SDL_GamepadAxis a){return std::uint8_t((int(SDL_GetGamepadAxis(pad_,a))+32768)>>8);};
        s.lx=axis(SDL_GAMEPAD_AXIS_LEFTX);s.ly=axis(SDL_GAMEPAD_AXIS_LEFTY);
        s.rx=axis(SDL_GAMEPAD_AXIS_RIGHTX);s.ry=axis(SDL_GAMEPAD_AXIS_RIGHTY);
        return s;
    }
    // Whether a key (Win32 virtual-key code) is down right now.
    static bool keyDown(std::uint8_t key) {
        const SDL_Scancode code=scancode(key);
        if(code==SDL_SCANCODE_UNKNOWN)return false;
        int count=0;const bool* keys=SDL_GetKeyboardState(&count);
        return keys && int(code)<count && keys[code];
    }

private:
    static constexpr Sint16 trigger_threshold=30*32767/255;
    SDL_Gamepad* pad_=nullptr;

    // The virtual-key codes a settings file can hold that a phone or PC keyboard has.
    static SDL_Scancode scancode(std::uint8_t key) {
        if(key>='A' && key<='Z')return SDL_Scancode(SDL_SCANCODE_A+(key-'A'));
        if(key>='1' && key<='9')return SDL_Scancode(SDL_SCANCODE_1+(key-'1'));
        if(key>=0x70 && key<=0x7B)return SDL_Scancode(SDL_SCANCODE_F1+(key-0x70)); // F1..F12
        switch(key) {
        case '0':return SDL_SCANCODE_0;
        case 0x08:return SDL_SCANCODE_BACKSPACE;
        case 0x09:return SDL_SCANCODE_TAB;
        case 0x0D:return SDL_SCANCODE_RETURN;
        case 0x10:case 0xA0:return SDL_SCANCODE_LSHIFT;
        case 0xA1:return SDL_SCANCODE_RSHIFT;
        case 0x11:case 0xA2:return SDL_SCANCODE_LCTRL;
        case 0xA3:return SDL_SCANCODE_RCTRL;
        case 0x12:case 0xA4:return SDL_SCANCODE_LALT;
        case 0x1B:return SDL_SCANCODE_ESCAPE;
        case 0x20:return SDL_SCANCODE_SPACE;
        case 0x25:return SDL_SCANCODE_LEFT;
        case 0x26:return SDL_SCANCODE_UP;
        case 0x27:return SDL_SCANCODE_RIGHT;
        case 0x28:return SDL_SCANCODE_DOWN;
        default:return SDL_SCANCODE_UNKNOWN;
        }
    }
};
