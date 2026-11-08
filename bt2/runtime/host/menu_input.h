// From held buttons to menu presses, and the combination that opens the in-game menu. No Win32 and
// no clock of its own: the caller passes what is held and the time, so both are unit-tested.
#pragma once
#include "host/gamepad.h"
#include "host/menu_model.h"
#include <cstdint>
#include <vector>

// A press when a menu button goes down; a held direction repeats after a pause.
class MenuInput {
public:
    static constexpr unsigned first_repeat_ms=400,repeat_ms=90;
    // `held`: bit per MenuKey. Returns the presses of this moment, in MenuKey order.
    std::vector<MenuKey> step(unsigned held,std::uint64_t now_ms) {
        std::vector<MenuKey> presses;
        for(unsigned key=0;key<6;++key) {
            const unsigned bit=1u<<key;
            if(!(held&bit)) {held_&=~bit;continue;}
            if(!(held_&bit)) {held_|=bit;next_[key]=now_ms+first_repeat_ms;presses.push_back(MenuKey(key));}
            else if(key<4 && now_ms>=next_[key]) {next_[key]=now_ms+repeat_ms;presses.push_back(MenuKey(key));}
        }
        return presses;
    }
    // Everything held now counts as already pressed: a button still down when a menu opens does nothing.
    void swallow(unsigned held,std::uint64_t now_ms) {held_=held;for(auto& next:next_)next=now_ms+first_repeat_ms;}

private:
    unsigned held_=0;std::uint64_t next_[6]{};
};

// The menu buttons a gamepad holds: the direction buttons or the left stick, south to confirm, east to go back.
inline unsigned menuButtons(const GamepadState& pad) {
    if(!pad.connected)return 0;
    unsigned held=0;
    const auto set=[&](MenuKey key,bool on){if(on)held|=1u<<unsigned(key);};
    set(MenuKey::Up,pad.held(GamepadState::Up) || pad.ly<0x40);set(MenuKey::Down,pad.held(GamepadState::Down) || pad.ly>0xC0);
    set(MenuKey::Left,pad.held(GamepadState::Left) || pad.lx<0x40);set(MenuKey::Right,pad.held(GamepadState::Right) || pad.lx>0xC0);
    set(MenuKey::Confirm,pad.held(GamepadState::South));set(MenuKey::Back,pad.held(GamepadState::East));
    return held;
}

// Opens the in-game menu: the guide button (the DualSense's PS button), the keyboard's Escape, and on
// any gamepad either both stick buttons together or, when chosen, Start with Select. It fires once
// when the combination becomes complete and again only after it was released.
class OverlayTrigger {
public:
    bool update(const GamepadState& pad,bool escape,bool start_select) {
        const bool combination=pad.connected && (pad.held(GamepadState::Guide)
            || (start_select?pad.held(GamepadState::Start) && pad.held(GamepadState::Back):pad.held(GamepadState::LeftStick) && pad.held(GamepadState::RightStick)));
        const bool now=combination || escape,fired=now && !was_;
        was_=now;
        return fired;
    }
    void swallow() {was_=true;} // The buttons that closed the menu do not open it again.

private:
    bool was_=false;
};

// Fast forward from its key and button: held, or switched by each new press.
class FastForward {
public:
    bool update(bool pressed,bool toggle) {
        if(toggle) {if(pressed && !was_)on_=!on_;} else on_=pressed;
        was_=pressed;
        return on_;
    }
    void stop() {on_=false;} // The window lost the focus: its key and button are no longer seen.

private:
    bool was_=false,on_=false;
};
