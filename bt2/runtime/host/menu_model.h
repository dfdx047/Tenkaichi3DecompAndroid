// A menu as data and the effect of presses on it; no drawing and no host calls. Used by the launcher
// and the in-game overlay (docs/plans/launcher/LAUNCHER_PLAN.md). A menu is a stack of screens; a
// screen is a title and entries. Entries act through small callbacks, so the menu owns no settings.
#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

enum class MenuKey {Up,Down,Left,Right,Confirm,Back};
// What an entry of a browser screen is drawn as.
enum class MenuIcon {None,Disc,NoDisc,Card,CardImage,NewCard};

struct MenuEntry {
    std::string label;
    std::function<std::string()> value;          // Shown beside the label; empty for none.
    std::function<void()> confirm;               // Cross / Enter.
    std::function<void(int)> adjust;             // Left (-1) and right (+1).
    std::string note;                            // A line of explanation shown while highlighted.
    bool enabled=true;                           // A disabled entry is shown but cannot be highlighted.
    MenuIcon icon=MenuIcon::None;                // Its picture on a browser screen.
};

struct MenuScreen {
    std::string title;
    std::vector<MenuEntry> entries;
    unsigned highlighted=0;
    // Drawn as the console's browser (a row of icons on grey) instead of a list; every arrow moves
    // through the row.
    bool browser=false;
};

class Menu {
public:
    std::function<void()> on_leave_root; // Back on the first screen.

    void open(MenuScreen screen) {
        if(screen.entries.empty())throw std::runtime_error("Menu screen without entries: "+screen.title);
        screen.highlighted=0;
        while(screen.highlighted<screen.entries.size() && !screen.entries[screen.highlighted].enabled)++screen.highlighted;
        if(screen.highlighted==screen.entries.size())throw std::runtime_error("Menu screen without an enabled entry: "+screen.title);
        stack_.push_back(std::move(screen));
    }
    void close() {if(stack_.size()>1)stack_.pop_back();else if(on_leave_root)on_leave_root();}
    bool empty() const {return stack_.empty();}
    std::size_t depth() const {return stack_.size();}
    const MenuScreen& screen() const {return stack_.back();}

    void press(MenuKey key) {
        if(stack_.empty())return;
        auto& s=stack_.back();
        switch(key) {
        case MenuKey::Up: case MenuKey::Down: {
            // To the next enabled entry, around the ends.
            const auto count=unsigned(s.entries.size());
            do s.highlighted=(s.highlighted+(key==MenuKey::Down?1:count-1))%count; while(!s.entries[s.highlighted].enabled);
            break;
        }
        case MenuKey::Left: case MenuKey::Right:
            if(s.browser) {press(key==MenuKey::Right?MenuKey::Down:MenuKey::Up);break;}
            if(const auto adjust=s.entries[s.highlighted].adjust)adjust(key==MenuKey::Right?1:-1); // A copy: the callback may change the stack.
            break;
        case MenuKey::Confirm: {
            const auto entry=s.entries[s.highlighted];
            if(entry.confirm)entry.confirm();else if(entry.adjust)entry.adjust(1);
            break;
        }
        case MenuKey::Back: close();break;
        }
    }

private:
    std::vector<MenuScreen> stack_;
};

// Entry builders for the common kinds.
inline MenuEntry menuAction(std::string label,std::function<void()> action,std::string note={}) {
    MenuEntry e;e.label=std::move(label);e.confirm=std::move(action);e.note=std::move(note);return e;
}
inline MenuEntry menuToggle(std::string label,bool& value,std::string note={}) {
    MenuEntry e;e.label=std::move(label);e.note=std::move(note);
    e.value=[&value]{return std::string(value?"On":"Off");};e.adjust=[&value](int){value=!value;};return e;
}
// A number from `low` to `high` in steps of `step`; it stops at the ends.
inline MenuEntry menuNumber(std::string label,unsigned& value,unsigned low,unsigned high,unsigned step,std::string unit={},std::string note={}) {
    MenuEntry e;e.label=std::move(label);e.note=std::move(note);
    e.value=[&value,unit]{return std::to_string(value)+unit;};
    e.adjust=[&value,low,high,step](int direction) {
        if(direction>0)value=value+step>high?high:value+step;
        else value=value<low+step?low:value-step;
    };
    e.confirm=[]{}; // Confirm does nothing on a number.
    return e;
}
