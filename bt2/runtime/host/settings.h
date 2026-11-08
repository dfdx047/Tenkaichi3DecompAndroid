// The player's settings (docs/plans/launcher/LAUNCHER_PLAN.md): a typed record and its text form,
// one "key = value" per line. A file with an unknown key or a value out of range is refused by name;
// a missing key keeps its default, so older files stay readable.
#pragma once
#include "host/input_mapping.h"
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Settings {
    std::string disc;               // The game's disc image: a file name in the data folder (host/game_folders.h).
    bool integer_scaling=true;      // Off: the picture is stretched to the window.
    bool fullscreen=false;
    unsigned window_scale=2;        // Window size as a multiple of the game's picture; the launcher offers what fits the screen.
    bool show_frame_rate=false;
    unsigned master_volume=100,music_volume=100,effects_volume=100; // Percent.
    bool hd_textures=false;         // The texture pack in data/textures (host/game_folders.h), from the next start.
    unsigned super_sampling=1;      // Samples per pixel (1, 2, 4, 8, 16); 4 and more draw at twice the resolution. From the next start.
    bool widescreen=false;          // The game's 16:9 patch (ee/game_patches.h), from the next start.
    bool sixty_fps=false;           // 60 fps battles: pictures between the fight's frames (gs/gpu/in_between_frames.h), from the next start.
    InputMapping mapping;           // Keys and gamepad buttons of the PS2 buttons.
    // Fast forward: a key (virtual-key code, 0 for none) and a gamepad button (Count for none), held
    // or pressed once to switch.
    unsigned fast_forward_key=0x09; // Tab.
    unsigned fast_forward_button=GamepadState::Count;
    bool fast_forward_toggle=false;
    // What opens the in-game menu on a gamepad besides the DualSense's PS button and Escape:
    // the two stick buttons together, or Start with Select.
    bool overlay_start_select=false;
    // The card in slot 1: a folder name in the saves folder; empty: none (the game program then makes one).
    std::string memory_card;
    static constexpr unsigned max_window_scale=16;
    bool operator==(const Settings&) const=default;

    std::string text() const {
        std::ostringstream out;
        out<<"disc = "<<disc<<"\nstick_sensitivity = "<<mapping.stick_sensitivity<<"\ninteger_scaling = "<<integer_scaling<<"\nfullscreen = "<<fullscreen<<"\nwindow_scale = "<<window_scale
           <<"\nshow_frame_rate = "<<show_frame_rate<<"\nmaster_volume = "<<master_volume<<"\nmusic_volume = "<<music_volume
           <<"\neffects_volume = "<<effects_volume<<"\nhd_textures = "<<hd_textures<<"\nsuper_sampling = "<<super_sampling<<"\nwidescreen = "<<widescreen<<"\nsixty_fps = "<<sixty_fps<<"\nfast_forward_key = "<<fast_forward_key
           <<"\nfast_forward_button = "<<buttonName(fast_forward_button)<<"\nfast_forward_toggle = "<<fast_forward_toggle
           <<"\noverlay_start_select = "<<overlay_start_select<<"\nmemory_card = "<<memory_card<<"\n";
        for(unsigned n=0;n<16;++n)out<<"key_"<<PadScript::names[n]<<" = "<<unsigned(mapping.keys[n])<<"\n";
        for(unsigned n=0;n<16;++n)out<<"button_"<<PadScript::names[n]<<" = "<<buttonName(mapping.buttons[n])<<"\n";
        return out.str();
    }
    static Settings parse(const std::string& text) {
        Settings s;
        std::istringstream in(text);std::string line;
        while(std::getline(in,line)) {
            if(!line.empty() && line.back()=='\r')line.pop_back();
            if(line.empty())continue;
            if(line.size()>2 && line.ends_with(" ="))line+=' '; // "key =" with nothing after it is an empty value.
            const auto at=line.find(" = ");
            if(at==std::string::npos)throw std::runtime_error("Settings line is not \"key = value\": "+line);
            const std::string key=line.substr(0,at),value=line.substr(at+3);
            const auto number=[&](unsigned low,unsigned high) {
                if(value.empty() || value.size()>4 || value.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Setting "+key+" is not a number: "+value);
                const unsigned v=unsigned(std::stoul(value));
                if(v<low || v>high)throw std::runtime_error("Setting "+key+" is out of range: "+value);
                return v;
            };
            const auto button=[&](bool none_allowed) {
                for(unsigned b=0;b<GamepadState::Count;++b)if(value==GamepadState::names[b])return b;
                if(none_allowed && value=="none")return unsigned(GamepadState::Count);
                throw std::runtime_error("Setting "+key+" names no gamepad button: "+value);
            };
            if(key=="disc")s.disc=value;
            // Keys of older files (folders chosen in a dialog, cards taken off the list): no longer used.
            else if(key=="disc_folder" || key=="card_folder" || key=="hidden_card") {}
            else if(key=="stick_sensitivity")s.mapping.stick_sensitivity=number(50,200);
            else if(key=="integer_scaling")s.integer_scaling=number(0,1);
            else if(key=="fullscreen")s.fullscreen=number(0,1);
            else if(key=="window_scale")s.window_scale=number(1,max_window_scale);
            else if(key=="show_frame_rate")s.show_frame_rate=number(0,1);
            else if(key=="master_volume")s.master_volume=number(0,100);
            else if(key=="music_volume")s.music_volume=number(0,100);
            else if(key=="effects_volume")s.effects_volume=number(0,100);
            else if(key=="hd_textures")s.hd_textures=number(0,1);
            else if(key=="widescreen")s.widescreen=number(0,1);
            else if(key=="sixty_fps")s.sixty_fps=number(0,1);
            else if(key=="super_sampling") {
                s.super_sampling=number(1,16);
                if(s.super_sampling&(s.super_sampling-1))throw std::runtime_error("Setting super_sampling is not 1, 2, 4, 8 or 16: "+value);
            }
            else if(key=="fast_forward_key")s.fast_forward_key=number(0,255);
            else if(key=="fast_forward_button")s.fast_forward_button=button(true);
            else if(key=="fast_forward_toggle")s.fast_forward_toggle=number(0,1);
            else if(key=="overlay_start_select")s.overlay_start_select=number(0,1);
            else if(key=="memory_card") {
                // One folder name. A full path (a card elsewhere, from an older file) is dropped; the
                // launcher or the game program then uses a card of the saves folder.
                const bool full=(value.size()>2 && value[1]==':' && (value[2]=='\\' || value[2]=='/')) || value.starts_with("\\\\");
                if(full)continue;
                if(value.find_first_of("/\\:*?\"<>|")!=std::string::npos || value==".." || value==".")throw std::runtime_error("Setting memory_card is not a folder name: "+value);
                s.memory_card=value;
            }
            else {
                const bool is_key=key.starts_with("key_"),is_button=key.starts_with("button_");
                unsigned index=16;
                if(is_key || is_button)for(unsigned n=0;n<16;++n)if(key.substr(is_key?4:7)==PadScript::names[n])index=n;
                if(index==16)throw std::runtime_error("Unknown setting: "+key);
                if(is_key)s.mapping.keys[index]=std::uint8_t(number(0,255));
                else s.mapping.buttons[index]=GamepadState::Button(button(false));
            }
        }
        return s;
    }
    // The settings in `path`, or nothing when the file does not exist.
    static std::optional<Settings> load(const std::string& path) {
        std::ifstream in(path,std::ios::binary);
        if(!in)return std::nullopt;
        std::ostringstream all;all<<in.rdbuf();
        return parse(all.str());
    }
    void save(const std::string& path) const {
        std::ofstream out(path,std::ios::binary);
        out<<text();
        if(!out)throw std::runtime_error("Cannot write the settings to "+path);
    }
    static std::string buttonName(unsigned button) {return button<GamepadState::Count?std::string(GamepadState::names[button]):"none";}
};
