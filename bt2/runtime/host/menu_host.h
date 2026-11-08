// Runs a menu in the window: reads the keyboard and the gamepad, feeds the menu model, draws it and
// shows it, until an entry ends it or the window is closed. Also waits for "the next key or button"
// when an entry assigns one. Shared by the launcher and the in-game menu, with the settings screens
// both of them show (docs/plans/launcher/LAUNCHER_PLAN.md).
#pragma once
#ifdef _WIN32
#include "host/host_input.h"
#include "host/menu_input.h"
#include "host/menu_model.h"
#include "host/menu_renderer.h"
#include "host/settings.h"
#include "host/viewer_window.h"
#include <array>
#include <chrono>
#include <functional>
#include <string>

class MenuHost {
public:
    struct Captured {unsigned key=0;unsigned button=GamepadState::Count;}; // One of the two is set.
    Menu menu;
    bool finished=false; // Set by an entry to end `run`.
    // For the pause test (BT2_PAUSE_TEST): `run` ends by itself after this many milliseconds, and the
    // last picture it drew is kept.
    unsigned close_after_ms=0;DisplayFrame last_drawn;

    MenuHost(ViewerWindow& window,HostInput& input):window_(window),input_(input) {}
    ViewerWindow& window() {return window_;}
    // Runs `action` once the press being handled is done: for an entry that replaces its own screen.
    void later(std::function<void()> action) {later_=std::move(action);}
    // Runs what `later` holds now, as `run` does after a press (for tests that press without `run`).
    void settle() {if(later_) {const auto action=std::move(later_);later_=nullptr;action();}}
    // The logo on the first screen (host/logo_image.h).
    void setLogo(const std::filesystem::path& path) {renderer_.setLogo(path);}
    // Whether a disc is in: the first screen shows it spinning (host/menu_renderer.h).
    void setDiscPresent(std::function<bool()> present) {renderer_.setDiscPresent(std::move(present));}
    // The current screen drawn at a size, as `run` would show it (for tests and pictures).
    DisplayFrame picture(unsigned width,unsigned height,double seconds) {return renderer_.draw(menu.screen(),width,height,seconds,menu.depth()==1,prompt_);}
    // Runs `action` every `ms` milliseconds while the menu runs (the launcher reads its folders again).
    void every(unsigned ms,std::function<void()> action) {tick_ms_=ms;tick_=std::move(action);}

    // The next key or gamepad button pressed goes to `done`; Escape cancels.
    void capture(std::string prompt,std::function<void(Captured)> done) {prompt_=std::move(prompt);capture_=std::move(done);snapshot();}

    // Returns false when the window was closed.
    bool run(const std::string& title,const DisplayFrame* backdrop=nullptr) {
        using Clock=std::chrono::steady_clock;
        const auto start=Clock::now();
        const auto ms=[&]{return std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start).count());};
        now_=ms();steps_.swallow(held(),now_);finished=false;
        while(!finished && !menu.empty()) {
            now_=ms();
            if(close_after_ms && now_>=close_after_ms)break;
            window_.pump();
            if(window_.closed())return false;
            if(window_.focused() || !window_.opened()) {
                if(capture_)captureStep();
                else for(const MenuKey key:steps_.step(held(),now_))menu.press(key);
                if(later_) {const auto action=std::move(later_);later_=nullptr;action();}
            }
            if(tick_ && now_>=next_tick_) {
                next_tick_=now_+tick_ms_;tick_();
                if(later_) {const auto action=std::move(later_);later_=nullptr;action();}
            }
            if(finished || menu.empty())break;
            // Drawn at the window's size, halved while that is larger than a screen needs.
            auto [width,height]=window_.clientSize();
            while(height>1200) {width/=2;height/=2;}
            auto drawn=renderer_.draw(menu.screen(),width,height,double(ms())/1000.0,menu.depth()==1,prompt_,backdrop);
            if(close_after_ms)last_drawn=drawn;
            window_.present(drawn,title);
            Sleep(15);
        }
        return !window_.closed();
    }

    static std::string keyName(unsigned key) {
        if(!key)return "none";
        char name[64]{};
        UINT scan=MapVirtualKeyA(key,MAPVK_VK_TO_VSC);
        // The arrow and editing keys share scan codes with the number pad: mark them as the extended ones.
        if(key>=0x21 && key<=0x2E)scan|=0x100;
        if(scan && GetKeyNameTextA(LONG(scan<<16),name,sizeof name)>0)return name;
        return "key "+std::to_string(key);
    }

private:
    ViewerWindow& window_;HostInput& input_;MenuRenderer renderer_;MenuInput steps_;std::function<void()> later_;
    std::function<void(Captured)> capture_;std::string prompt_;
    std::function<void()> tick_;unsigned tick_ms_=0;std::uint64_t next_tick_=0;
    std::array<bool,256> keys_{};std::uint32_t buttons_=0;std::uint64_t now_=0;

    unsigned held() {
        unsigned mask=menuButtons(input_.gamepad());
        const auto set=[&](MenuKey key,bool on){if(on)mask|=1u<<unsigned(key);};
        set(MenuKey::Up,HostInput::keyDown(VK_UP));set(MenuKey::Down,HostInput::keyDown(VK_DOWN));
        set(MenuKey::Left,HostInput::keyDown(VK_LEFT));set(MenuKey::Right,HostInput::keyDown(VK_RIGHT));
        set(MenuKey::Confirm,HostInput::keyDown(VK_RETURN) || HostInput::keyDown(VK_SPACE));
        set(MenuKey::Back,HostInput::keyDown(VK_BACK) || HostInput::keyDown(VK_ESCAPE));
        return mask;
    }
    void snapshot() {
        for(unsigned key=8;key<255;++key)keys_[key]=HostInput::keyDown(std::uint8_t(key));
        buttons_=input_.gamepad().buttons;
    }
    void captureStep() {
        Captured got;bool cancelled=false;
        for(unsigned key=8;key<255 && !got.key;++key) {
            // The generic Shift, Control and Alt codes repeat their left and right forms.
            if(key==VK_SHIFT || key==VK_CONTROL || key==VK_MENU)continue;
            const bool down=HostInput::keyDown(std::uint8_t(key));
            if(down && !keys_[key]) {if(key==VK_ESCAPE)cancelled=true;else got.key=key;}
            keys_[key]=down;
        }
        const auto pad=input_.gamepad();
        for(unsigned b=0;b<GamepadState::Count && !got.key && got.button==GamepadState::Count;++b)
            if((pad.buttons>>b&1) && !(buttons_>>b&1))got.button=b;
        buttons_=pad.buttons;
        if(!cancelled && !got.key && got.button==GamepadState::Count)return;
        const auto done=std::move(capture_);capture_=nullptr;prompt_.clear();
        if(!cancelled)done(got);
        steps_.swallow(~0u,now_); // Whatever is held now is not a menu press.
    }
};

// The settings screens. `changed` runs after every change (apply it, save it).
namespace menu_screens {
inline MenuEntry toggle(std::string label,bool& value,std::function<void()> changed,std::string note={}) {
    MenuEntry e=menuToggle(std::move(label),value,std::move(note));
    e.adjust=[&value,changed](int){value=!value;changed();};return e;
}
inline MenuEntry number(std::string label,unsigned& value,unsigned low,unsigned high,unsigned step,std::string unit,std::function<void()> changed,std::string note={}) {
    MenuEntry e=menuNumber(std::move(label),value,low,high,step,std::move(unit),std::move(note));
    const auto adjust=e.adjust;e.adjust=[adjust,changed](int direction){adjust(direction);changed();};return e;
}
inline MenuScreen display(MenuHost& host,Settings& s,std::function<void()> changed) {
    MenuScreen screen;screen.title="Display";
    const unsigned largest=std::min(host.window().largestScale(),Settings::max_window_scale);
    if(s.window_scale>largest)s.window_scale=largest;
    screen.entries.push_back(toggle("Integer scaling",s.integer_scaling,changed,"On: whole multiples of the picture, sharp. Off: stretched to the window."));
    screen.entries.push_back(toggle("Fullscreen",s.fullscreen,changed));
    screen.entries.push_back(number("Window size",s.window_scale,1,largest,1,"x",changed,"A multiple of 640 x 448. "+std::to_string(largest)+"x is the nearest to this screen."));
    screen.entries.push_back(toggle("Frame rate display",s.show_frame_rate,changed,"Pictures per second: 60 in menus, 30 in fights."));
    screen.entries.push_back(toggle("HD textures",s.hd_textures,changed,"A PCSX2 texture pack in data/textures (its folder holds \"replacements\"). From the next start."));
    MenuEntry samples;samples.label="Super-sampling";
    samples.value=[&s]{return s.super_sampling==1?std::string("Off"):std::to_string(s.super_sampling)+"x";};
    samples.adjust=[&s,changed](int direction) { // 1, 2, 4, 8, 16, round.
        s.super_sampling=direction<0?(s.super_sampling==1?16:s.super_sampling/2):(s.super_sampling==16?1:s.super_sampling*2);changed();
    };
    samples.note="Smoother edges and finer textures; 4x and more draw at twice the resolution (1280 x 896). From the next start.";
    screen.entries.push_back(samples);
    screen.entries.push_back(toggle("Widescreen",s.widescreen,changed,"16:9: the game draws a wider view (PCSX2's widescreen patch). Menus and the HUD are stretched. From the next start."));
    screen.entries.push_back(toggle("60 fps battles",s.sixty_fps,changed,"A picture between each two of the fight's 30 frames per second; the fight itself is unchanged. From the next start."));
    return screen;
}
inline MenuScreen sound(Settings& s,std::function<void()> changed) {
    MenuScreen screen;screen.title="Sound";
    screen.entries.push_back(number("Master volume",s.master_volume,0,100,5,"%",changed));
    screen.entries.push_back(number("Music",s.music_volume,0,100,5,"%",changed,"The game's streamed sound: music and films."));
    screen.entries.push_back(number("Effects and voices",s.effects_volume,0,100,5,"%",changed,"Sound effects and the characters' voices."));
    return screen;
}
inline MenuScreen fastForward(MenuHost& host,Settings& s,std::function<void()> changed) {
    MenuScreen screen;screen.title="Fast forward";
    MenuEntry key;key.label="Key";key.value=[&s]{return MenuHost::keyName(s.fast_forward_key);};
    key.confirm=[&host,&s,changed]{host.capture("Press the key for fast forward (Escape cancels)",[&s,changed](MenuHost::Captured c){if(c.key) {s.fast_forward_key=c.key;changed();}});};
    key.note="Confirm, then press the key.";
    MenuEntry button;button.label="Gamepad button";button.value=[&s]{return std::string(GamepadState::labels[s.fast_forward_button]);};
    button.confirm=[&host,&s,changed]{host.capture("Press the gamepad button for fast forward (Escape cancels)",[&s,changed](MenuHost::Captured c){if(c.button!=GamepadState::Count) {s.fast_forward_button=c.button;changed();}});};
    button.note="Confirm, then press the button. The game no longer sees that button.";
    screen.entries.push_back(key);screen.entries.push_back(button);
    screen.entries.push_back(menuAction("Clear the gamepad button",[&s,changed]{s.fast_forward_button=GamepadState::Count;changed();}));
    screen.entries.push_back(toggle("Switch instead of hold",s.fast_forward_toggle,changed,"Off: fast while held. On: each press switches it."));
    return screen;
}
// Which key and which gamepad button each PS2 button is.
inline MenuScreen buttonMapping(MenuHost& host,Settings& s,std::function<void()> changed) {
    static constexpr const char* labels[16]={"Select","L3","R3","Start","Up","Right","Down","Left","L2","R2","L1","R1","Triangle","Circle","Cross","Square"};
    static constexpr unsigned order[16]={14,13,15,12,4,6,7,5,10,11,8,9,3,0,1,2}; // Face buttons first.
    MenuScreen screen;screen.title="Button mapping";
    for(const unsigned n:order) {
        MenuEntry e;e.label=labels[n];
        e.value=[&s,n]{return MenuHost::keyName(s.mapping.keys[n])+"  -  "+std::string(GamepadState::labels[s.mapping.buttons[n]]);};
        e.confirm=[&host,&s,n,changed]{
            host.capture(std::string("Press a key or a gamepad button for ")+labels[n]+" (Escape cancels)",[&s,n,changed](MenuHost::Captured c) {
                if(c.key)s.mapping.keys[n]=std::uint8_t(c.key);else s.mapping.buttons[n]=GamepadState::Button(c.button);
                changed();
            });
        };
        e.note="Keyboard key  -  gamepad button. Confirm, then press the new one.";
        screen.entries.push_back(e);
    }
    screen.entries.push_back(menuAction("Reset to defaults",[&s,changed] {
        const unsigned sensitivity=s.mapping.stick_sensitivity;s.mapping=InputMapping{};s.mapping.stick_sensitivity=sensitivity;changed();
    },"The keys and gamepad buttons as the program came."));
    return screen;
}
inline MenuScreen controls(MenuHost& host,Settings& s,std::function<void()> changed) {
    MenuScreen screen;screen.title="Controls";
    screen.entries.push_back(menuAction("Button mapping",[&host,&s,changed]{host.menu.open(buttonMapping(host,s,changed));},"The key and the gamepad button of each PS2 button."));
    screen.entries.push_back(number("Stick sensitivity",s.mapping.stick_sensitivity,50,200,10,"%",changed,"How far the game sees the sticks pushed. 100%: as they are."));
    screen.entries.push_back(menuAction("Fast forward",[&host,&s,changed]{host.menu.open(fastForward(host,s,changed));}));
    screen.entries.push_back(toggle("Menu on Start + Select",s.overlay_start_select,changed,"Opens the in-game menu. Off: both stick buttons. The PS button and Escape always do."));
    return screen;
}
}
#endif
