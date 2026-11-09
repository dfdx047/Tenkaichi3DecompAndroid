// A player's run through SDL3 (Android, Linux), with the Win32 session's interface (host/player_session.h):
// the settings file decides the volumes, the controls and fast forward; the Android app's on-screen
// controller (host/sdl/touch_pad.h) is added to the gamepad and the keyboard. The in-game menu
// (host/overlay.h) draws with GDI and is not here yet: the back key pressed twice within two seconds (or
// the menu combination of host/menu_input.h twice) leaves the game for the launcher (`quit_to_launcher`;
// the Android app's own screens stand in for the settings meanwhile).
#pragma once
#include "host/frame_pacer.h"
#include "host/input_mapping.h"
#include "host/menu_input.h"
#include "host/settings.h"
#include "host/sdl/sdl_host_input.h"
#include "host/sdl/sdl_viewer_window.h"
#include "host/sdl/touch_pad.h"
#include "services/audio_mixer.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <functional>

class PlayerSession {
public:
    static constexpr int launcher_exit_code=2; // The player chose "Quit to the launcher".
    // What leaving for the launcher does: by default the run ends with launcher_exit_code; the Android app
    // brings its launcher forward instead (runtime/android/android_main.cpp).
    inline static std::function<void()> quit_to_launcher=[]{std::exit(launcher_exit_code);};
    Settings settings;
    // The Android app's in-game menu (Bt2GameActivity): while `paused` the game stands still at its next field;
    // a new `reload` reads the settings file again (volumes, scaling) from that field on. Any thread.
    inline static std::atomic<bool> paused{false};
    inline static std::atomic<unsigned> reload{0};
    // Running counts for the Android app's performance panel (Bt2Bridge.nativeStats): pictures shown and fields the
    // game ran; the app takes their rates.
    inline static std::atomic<std::uint64_t> pictures_total{0},fields_total{0};

    PlayerSession(std::string path,ViewerWindow& window,HostInput& input,AudioMixer& mixer,FramePacer* pacer)
        :path_(std::move(path)),window_(window),input_(input),mixer_(mixer),pacer_(pacer) {
        if(auto loaded=Settings::load(path_))settings=*loaded;else throw std::runtime_error("No settings file at "+path_);
        apply();
    }
    bool fastForward() const {return fast_;}
    std::function<DisplayFrame()> picture_source; // (For the in-game menu's background, when there is one.)

    // Once per field: the controller state for the game.
    PadInput padInput() {
        if(paused) {
            std::fprintf(stderr,"Paused by the menu.\n");
            while(paused) {SDL_PumpEvents();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            std::fprintf(stderr,"Resumed.\n");
        }
        ++fields_total;
        if(const unsigned wanted=reload;wanted!=reloaded_) {
            reloaded_=wanted;
            if(auto loaded=Settings::load(path_)) {settings=*loaded;apply();}
        }
        const auto pad=input_.gamepad();
#ifdef __ANDROID__
        // On Android a controller is only the PS2's: Guide and L3 + R3 are the game's (L3 and R3 its buttons), not a way
        // out; the app's own menu (the back key, the touch controls' menu button) leaves the game.
        if(window_.backPressed())leaveRequested();
#else
        if(window_.backPressed() | trigger_.update(pad,false,settings.overlay_start_select))leaveRequested();
#endif
        if(!window_.focused()) {fast_switch_.stop();setFast(false);return {};}
        const bool fast_button=settings.fast_forward_button<GamepadState::Count && pad.connected && pad.held(GamepadState::Button(settings.fast_forward_button));
        setFast(fast_switch_.update((settings.fast_forward_key && HostInput::keyDown(std::uint8_t(settings.fast_forward_key))) || fast_button,settings.fast_forward_toggle));
        return TouchPad::merge(mapInput(settings.mapping,masked(pad),&HostInput::keyDown),TouchPad::get());
    }
    // Each picture shown: counted for the frame rate (written to the log while fast forward or the
    // frame rate display is on, as there is no text on the picture yet).
    void shown(const DisplayFrame* =nullptr) {
        ++pictures_;++pictures_total;
        const auto now=std::chrono::steady_clock::now();
        const double seconds=std::chrono::duration<double>(now-window_start_).count();
        if(seconds>=5) {
            if(settings.show_frame_rate || fast_)std::fprintf(stderr,"%.1f fps%s\n",pictures_/seconds,fast_?"  >>":"");
            pictures_=0;window_start_=now;
        }
    }

private:
    using Clock=std::chrono::steady_clock;
    std::string path_;ViewerWindow& window_;HostInput& input_;AudioMixer& mixer_;FramePacer* pacer_;
    OverlayTrigger trigger_;FastForward fast_switch_;bool fast_=false;
    Clock::time_point last_leave_{};bool leave_armed_=false;
    unsigned pictures_=0;Clock::time_point window_start_=Clock::now();
    unsigned reloaded_=0;

    // A first press arms leaving; a second one within two seconds leaves.
    void leaveRequested() {
        const auto now=Clock::now();
        if(leave_armed_ && now-last_leave_<std::chrono::seconds(2)) {
            std::fprintf(stderr,"Back pressed twice: leaving for the launcher.\n");
            quit_to_launcher();
            return;
        }
        std::fprintf(stderr,"Back pressed: press it again within two seconds to leave the game.\n");
        leave_armed_=true;last_leave_=now;
    }
    void apply() {
        window_.setScaling(settings.integer_scaling?ViewerWindow::Scaling::Integer:ViewerWindow::Scaling::Stretch);
        window_.setLayout(settings.window_scale,settings.fullscreen);
        mixer_.master_percent=settings.master_volume;mixer_.stream_percent=settings.music_volume;mixer_.extra_percent=settings.effects_volume;
    }
    void setFast(bool on) {
        if(on==fast_)return;
        fast_=on;if(pacer_)pacer_->free_run=on;
    }
    // The fast forward button does not reach the game.
    GamepadState masked(GamepadState pad) const {
        if(settings.fast_forward_button<GamepadState::Count)pad.buttons&=~(1u<<settings.fast_forward_button);
        return pad;
    }
};
