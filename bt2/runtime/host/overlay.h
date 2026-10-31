// The in-game menu (docs/plans/launcher/LAUNCHER_PLAN.md, Overlay): shown over the dimmed game
// picture while the game stands still. Changes take effect and are saved at once.
#pragma once
#ifdef _WIN32
#include "host/menu_host.h"

enum class OverlayResult {Resume,Launcher,Quit};

// `applied` runs after every change, after the settings were saved. `close_after_ms` and `last_drawn`
// serve the pause test: the menu closes by itself and hands back the last picture it drew.
inline OverlayResult runOverlay(Settings& settings,const std::string& settings_path,ViewerWindow& window,HostInput& input,
                                const DisplayFrame& picture,const std::function<void()>& applied,unsigned close_after_ms=0,DisplayFrame* last_drawn=nullptr) {
    MenuHost host(window,input);host.close_after_ms=close_after_ms;
    OverlayResult result=OverlayResult::Resume;
    const std::function<void()> changed=[&] {settings.save(settings_path);applied();};
    const auto confirmed=[&](const char* title,OverlayResult chosen) {
        MenuScreen screen;screen.title=title;
        screen.entries.push_back(menuAction("No, go back",[&host]{host.menu.close();}));
        screen.entries.push_back(menuAction("Yes",[&host,&result,chosen]{result=chosen;host.finished=true;},"Progress that the game has not saved is lost."));
        host.menu.open(std::move(screen));
    };
    MenuScreen main;main.title="Paused";
    main.entries.push_back(menuAction("Resume",[&]{host.finished=true;}));
    main.entries.push_back(menuAction("Sound",[&]{host.menu.open(menu_screens::sound(settings,changed));}));
    main.entries.push_back(menuAction("Display",[&]{host.menu.open(menu_screens::display(host,settings,changed));}));
    main.entries.push_back(menuAction("Fast forward",[&]{host.menu.open(menu_screens::fastForward(host,settings,changed));},"Which key and button run the game as fast as the machine allows."));
    main.entries.push_back(menuAction("Controls",[&]{host.menu.open(menu_screens::controls(host,settings,changed));}));
    main.entries.push_back(menuAction("Quit to the launcher",[&]{confirmed("Quit to the launcher?",OverlayResult::Launcher);}));
    main.entries.push_back(menuAction("Quit game",[&]{confirmed("Quit the game?",OverlayResult::Quit);}));
    host.menu.open(std::move(main));
    host.menu.on_leave_root=[&]{host.finished=true;}; // Back resumes.
    const bool open=host.run("BT2 Recomp - paused",&picture);
    if(last_drawn)*last_drawn=host.last_drawn;
    if(!open)return OverlayResult::Quit;
    return result;
}
#endif
