// The launcher's and the in-game menu's logic without a window: the settings file, the menu model
// driven by presses, presses from held buttons, the combination that opens the in-game menu and
// fast forward.
#include "host/launcher.h"
#include "host/menu_input.h"
#include "host/menu_model.h"
#include "host/settings.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"Menu check failed: "<<what<<'\n';std::exit(1);}}
template<class F> bool refused(F&& f) {try{f();}catch(const std::runtime_error&){return true;}return false;}
unsigned bit(MenuKey key) {return 1u<<unsigned(key);}
// The brightest channel of the first screen's pixel where the disc mark's data side is (top right).
unsigned discArea(MenuHost& host,double seconds=2.0) {
    const auto frame=host.picture(1280,896,seconds);
    const auto p=frame.bgra[std::size_t(896*8/100)*1280+1280*92/100+20];
    return std::max({(p>>16)&255u,(p>>8)&255u,p&255u});
}
namespace fs=std::filesystem;
void touch(const fs::path& path,const std::string& text="x") {fs::create_directories(path.parent_path());std::ofstream(path,std::ios::binary)<<text;}
std::vector<std::string> labels(const MenuScreen& screen) {std::vector<std::string> out;for(const auto& e:screen.entries)out.push_back(e.label);return out;}
// BT2_MENU_PICTURES=<folder>: each launcher screen of the test is saved there as <name>.ppm for a look.
void picture(MenuHost& host,const char* name) {
    const char* folder=std::getenv("BT2_MENU_PICTURES");
    if(!folder)return;
    const auto frame=host.picture(1280,896,2.0);
    std::ofstream out(fs::path(folder)/(std::string(name)+".ppm"),std::ios::binary);
    out<<"P6\n"<<frame.width<<" "<<frame.height<<"\n255\n";
    for(const auto p:frame.bgra) {const char rgb[3]={char((p>>16)&255),char((p>>8)&255),char(p&255)};out.write(rgb,3);}
}
}

int main() {
    {
        // Settings: the text form reads back equal, also with every field changed; older files with
        // fewer keys keep the defaults; what is not a setting is refused by name.
        Settings s;check(Settings::parse(s.text())==s,"defaults read back");
        s.disc="a disc = image.iso";s.integer_scaling=false;s.fullscreen=true;s.window_scale=3;s.show_frame_rate=true;
        s.master_volume=80;s.music_volume=0;s.effects_volume=55;s.fast_forward_key='F';s.fast_forward_button=GamepadState::RightTrigger;
        s.fast_forward_toggle=true;s.overlay_start_select=true;s.memory_card="Card 1";s.mapping.keys[14]='K';s.mapping.buttons[14]=GamepadState::East;
        s.mapping.stick_sensitivity=150;s.window_scale=9;s.hd_textures=true;s.super_sampling=16;s.widescreen=true;s.sixty_fps=true;
        check(Settings::parse(s.text())==s,"changed settings read back");
        check(refused([]{Settings::parse("super_sampling = 3\n");}) && refused([]{Settings::parse("super_sampling = 32\n");}),"super-sampling is 1, 2, 4, 8 or 16");
        // Older files: the dialog folders and the hidden cards are ignored, a card elsewhere is dropped.
        const auto old=Settings::parse("disc = E:\\x.iso\ndisc_folder = E:\\discs\ncard_folder = E:\\saves\\One\nhidden_card = Card 2\nmemory_card = E:\\saves\\My card\n");
        check(old.disc=="E:\\x.iso" && old.memory_card.empty(),"older keys are read and dropped");
        check(refused([]{Settings::parse("stick_sensitivity = 10\n");}),"stick sensitivity has its range");
        check(Settings::parse("window_scale = 4\r\ndisc =\r\n").window_scale==4,"a short file with CRLF lines and an empty value");
        check(Settings::parse("fast_forward_button = none\n").fast_forward_button==GamepadState::Count,"no fast forward button");
        check(refused([]{Settings::parse("speed = 2\n");}),"an unknown key is refused");
        check(refused([]{Settings::parse("window_scale = 17\n");}),"a value out of range is refused");
        check(refused([]{Settings::parse("master_volume = loud\n");}),"a value that is not a number is refused");
        check(refused([]{Settings::parse("button_cross = elbow\n");}),"an unknown gamepad button is refused");
        check(refused([]{Settings::parse("button_cross = none\n");}),"a PS2 button needs a gamepad button");
        check(refused([]{Settings::parse("key_elbow = 65\n");}),"an unknown PS2 button is refused");
        check(refused([]{Settings::parse("window_scale 2\n");}),"a line without = is refused");
        check(refused([]{Settings::parse("memory_card = ..\\elsewhere\n");}) && refused([]{Settings::parse("memory_card = ..\n");}),"a card is a folder name, not a relative path");
        const auto path=(std::filesystem::temp_directory_path()/"bt2-menu-tests-settings").string();
        std::filesystem::remove(path);
        check(!Settings::load(path),"no file, no settings");
        s.save(path);check(Settings::load(path)==s,"saved and loaded");
        std::filesystem::remove(path);
    }
    {
        // The menu model: moving around the ends and over disabled entries, entering and leaving
        // screens, toggles, numbers that stop at their ends.
        bool flag=false;unsigned volume=95;unsigned started=0,left_root=0;
        Menu menu;menu.on_leave_root=[&]{++left_root;};
        MenuScreen main;main.title="Main";
        main.entries.push_back(menuAction("Start",[&]{++started;}));
        MenuEntry off;off.label="Unavailable";off.enabled=false;main.entries.push_back(off);
        main.entries.push_back(menuAction("Sound",[&] {
            MenuScreen sound;sound.title="Sound";
            sound.entries.push_back(menuNumber("Volume",volume,0,100,10,"%"));
            sound.entries.push_back(menuToggle("Mute",flag));
            menu.open(std::move(sound));
        }));
        menu.open(std::move(main));
        check(menu.screen().highlighted==0,"the first entry is highlighted");
        menu.press(MenuKey::Confirm);check(started==1,"confirm runs the entry");
        menu.press(MenuKey::Down);check(menu.screen().highlighted==2,"a disabled entry is skipped");
        menu.press(MenuKey::Down);check(menu.screen().highlighted==0,"down wraps around");
        menu.press(MenuKey::Up);check(menu.screen().highlighted==2,"up wraps around");
        menu.press(MenuKey::Confirm);check(menu.depth()==2 && menu.screen().title=="Sound","an entry opens a screen");
        check(menu.screen().entries[0].value()=="95%","a number shows its unit");
        menu.press(MenuKey::Right);check(volume==100,"a number stops at its upper end");
        menu.press(MenuKey::Left);menu.press(MenuKey::Left);check(volume==80,"left lowers it by its step");
        volume=5;menu.press(MenuKey::Left);check(volume==0,"and stops at its lower end");
        menu.press(MenuKey::Confirm);check(volume==0,"confirm does nothing on a number");
        menu.press(MenuKey::Down);menu.press(MenuKey::Confirm);check(flag,"confirm switches a toggle");
        menu.press(MenuKey::Left);check(!flag && menu.screen().entries[1].value()=="Off","so do left and right");
        menu.press(MenuKey::Back);check(menu.depth()==1 && menu.screen().highlighted==2,"back returns to the entry it came from");
        menu.press(MenuKey::Back);check(left_root==1 && menu.depth()==1,"back on the first screen is reported, the screen stays");
        check(refused([&]{menu.open(MenuScreen{"Empty",{},0});}),"a screen without entries is refused");
        MenuScreen dead;dead.title="Dead";dead.entries.push_back(off);
        check(refused([&]{menu.open(dead);}),"a screen without an enabled entry is refused");
    }
    {
        // Presses from held buttons: one when a button goes down, repeats for directions only.
        MenuInput input;
        check(input.step(bit(MenuKey::Down),0)==std::vector<MenuKey>{MenuKey::Down},"a press when the button goes down");
        check(input.step(bit(MenuKey::Down),399).empty(),"nothing while it is held at first");
        check(input.step(bit(MenuKey::Down),400).size()==1 && input.step(bit(MenuKey::Down),489).empty() && input.step(bit(MenuKey::Down),490).size()==1,"then it repeats");
        check(input.step(0,500).empty() && input.step(bit(MenuKey::Down),501).size()==1,"released and pressed again");
        check(input.step(bit(MenuKey::Confirm),600).size()==1 && input.step(bit(MenuKey::Confirm),5000).empty(),"confirm does not repeat");
        MenuInput opened;opened.swallow(bit(MenuKey::Confirm),0);
        check(opened.step(bit(MenuKey::Confirm),10).empty() && opened.step(0,20).empty() && opened.step(bit(MenuKey::Confirm),30).size()==1,"a button held when the menu opens counts after its release");
        GamepadState pad;check(menuButtons(pad)==0,"no gamepad, no buttons");
        pad.connected=true;pad.buttons=1u<<GamepadState::South|1u<<GamepadState::Up;pad.lx=0xFF;
        check(menuButtons(pad)==(bit(MenuKey::Confirm)|bit(MenuKey::Up)|bit(MenuKey::Right)),"direction buttons, the stick and south");
    }
    {
        // The in-game menu's combination: once per press, by the chosen pair of buttons.
        OverlayTrigger trigger;GamepadState pad;pad.connected=true;
        pad.buttons=1u<<GamepadState::LeftStick;check(!trigger.update(pad,false,false),"one stick button alone does nothing");
        pad.buttons|=1u<<GamepadState::RightStick;check(trigger.update(pad,false,false),"both open the menu");
        check(!trigger.update(pad,false,false),"once");
        pad.buttons=0;check(!trigger.update(pad,false,false),"released");
        pad.buttons=1u<<GamepadState::Start|1u<<GamepadState::Back;check(!trigger.update(pad,false,false) && trigger.update(pad,false,true),"Start with Select only when chosen");
        pad.buttons=0;trigger.update(pad,false,true);
        check(trigger.update(pad,true,false),"Escape opens it");
        trigger.update(pad,false,false);pad.buttons=1u<<GamepadState::Guide;check(trigger.update(pad,false,false),"so does the guide button");
        OverlayTrigger closed;closed.swallow();check(!closed.update(pad,false,false),"what closed the menu does not open it again");
        // Fast forward: held, or switched by each press.
        FastForward held;check(held.update(true,false) && !held.update(false,false),"fast while held");
        FastForward toggled;
        check(toggled.update(true,true) && toggled.update(true,true) && toggled.update(false,true),"a press switches it on and it stays");
        check(!toggled.update(true,true) && !toggled.update(false,true),"the next press switches it off");
        FastForward unfocused;unfocused.update(true,true);unfocused.update(false,true);unfocused.stop();
        check(!unfocused.update(false,true) && unfocused.update(true,true),"losing the focus stops it; the next press starts it again");
        FastForward held_away;held_away.update(true,false);held_away.stop();
        check(!held_away.update(false,false),"a key released while the window had no focus does not keep it on");
    }
    {
        // The game's folders next to the programs: made when missing, never replaced or emptied; their
        // contents listed; the settings checked against them.
        const fs::path program=fs::temp_directory_path()/"bt2-menu-tests-game";
        fs::remove_all(program);fs::create_directories(program);
        const GameFolders folders(program);
        check(folders.create() && fs::is_directory(program/"data") && fs::is_directory(program/"saves"),"first start makes data and saves");
        touch(program/"data"/"kept.txt");touch(program/"saves"/"Card A"/"BASLUS-21441DBZT2"/"BASLUS-21441DBZT2","save");
        check(!folders.create() && fs::exists(program/"data"/"kept.txt") && fs::exists(program/"saves"/"Card A"/"BASLUS-21441DBZT2"/"BASLUS-21441DBZT2"),
              "later starts make nothing and leave the files");
        {
            const fs::path odd=fs::temp_directory_path()/"bt2-menu-tests-odd";fs::remove_all(odd);touch(odd/"data","not a folder");
            check(refused([&]{GameFolders(odd).create();}) && fs::is_regular_file(odd/"data"),"a file named data is refused, not replaced");
            fs::remove_all(odd);
        }
        touch(program/"data"/"Game.ISO");touch(program/"data"/"notes.txt");touch(program/"saves"/"other.ps2");
        check(folders.discImages()==std::vector<std::string>{"Game.ISO"},"disc images: the .iso files, any case");
        // HD texture packs: the first folder in data/textures that holds "replacements".
        check(fs::is_directory(program/"data"/"textures") && folders.texturePack().empty(),"the textures folder is made, empty");
        touch(program/"data"/"textures"/"A notes"/"readme.txt");touch(program/"data"/"textures"/"B pack"/"replacements"/"x.dds");
        check(folders.texturePack()==program/"data"/"textures"/"B pack","a pack is the folder with replacements");
        check(folders.cards()==std::vector<std::string>{"Card A"} && folders.cardImages()==std::vector<std::string>{"other.ps2"},"cards are folders; card image files listed apart");
        Settings s;
        check(!folders.configured(s),"no disc chosen: not configured");
        s.disc="Game.ISO";check(folders.configured(s) && folders.disc(s)==program/"data"/"Game.ISO","a disc of the data folder");
        s.disc=(program/"data"/"Game.ISO").string();check(!folders.configured(s),"a full path is not a disc of the data folder");
        s.disc="..\\data\\Game.ISO";check(!folders.configured(s),"nor is a relative path");
        s.disc="missing.iso";check(!folders.configured(s),"nor a file that is not there");
        s.memory_card="Card A";check(folders.card(s)==program/"saves"/"Card A","a card of the saves folder");
        s.memory_card="Nope";check(folders.card(s).empty(),"a card that is not there");
        check(folders.newCard()=="Card 1" && folders.newCard()=="Card 2" && fs::is_directory(program/"saves"/"Card 2"),"new cards take free names");
        // The older card folder is copied in once: nothing replaced, nothing moved.
        const fs::path older=fs::temp_directory_path()/"bt2-menu-tests-old-cards";fs::remove_all(older);
        touch(older/"Card A"/"other","old");touch(older/"Old card"/"BASLUS-21441DBZT2"/"BASLUS-21441DBZT2","old save");
        check(folders.copyCardsFrom(older)==1 && fs::exists(program/"saves"/"Old card"/"BASLUS-21441DBZT2"/"BASLUS-21441DBZT2")
              && !fs::exists(program/"saves"/"Card A"/"other") && fs::exists(older/"Old card"),"older cards copied, existing ones kept");
        fs::remove_all(older);

        // The launcher, driven by presses without a window.
        fs::remove_all(program);fs::create_directories(program);folders.create();
        const auto settings_path=(program/"settings").string();
        Settings settings;ViewerWindow window;HostInput input;MenuHost host(window,input);
        host.setLogo(fs::path(BT2_SOURCE_DIR)/"assets"/"launcher"/"logo.png");
        Launcher launcher(settings,settings_path,folders,host);
        launcher.open();
        check(labels(host.menu.screen())==std::vector<std::string>{"Start game","Game disc","Memory card","Controls","Display","Sound","Quit"},"the first screen");
        check(host.menu.screen().entries[0].value()=="No game disc in the data folder","Start says what is missing");
        picture(host,"1-first-start");
        check(discArea(host)<60,"no disc: no disc mark");
        host.menu.press(MenuKey::Confirm);host.settle();
        check(!host.finished && launcher.result==LauncherResult::Quit,"no game without a disc");
        host.menu.press(MenuKey::Down);host.menu.press(MenuKey::Confirm);host.settle();
        check(host.menu.screen().title=="Game disc" && labels(host.menu.screen())==std::vector<std::string>{"No disc image yet"},"an empty data folder");
        picture(host,"2-no-disc");
        // A disc image moved in while the screen is open appears by itself and, being the only one, is chosen.
        touch(program/"data"/"Budokai Tenkaichi 2.iso");launcher.refresh();host.settle();
        check(labels(host.menu.screen())==std::vector<std::string>{"Budokai Tenkaichi 2.iso"} && settings.disc=="Budokai Tenkaichi 2.iso","a new disc image is found and chosen");
        check(Settings::load(settings_path)->disc=="Budokai Tenkaichi 2.iso","and saved");
        touch(program/"data"/"Another.iso");launcher.refresh();host.settle();
        check(labels(host.menu.screen())==std::vector<std::string>{"Another.iso","Budokai Tenkaichi 2.iso"} && settings.disc=="Budokai Tenkaichi 2.iso","a second image does not change the choice");
        host.menu.press(MenuKey::Down);picture(host,"3-discs");
        host.menu.press(MenuKey::Up);host.menu.press(MenuKey::Confirm);host.settle();check(settings.disc=="Another.iso","a disc is chosen by a press");
        host.menu.press(MenuKey::Back);
        // The memory cards: none yet, then a card folder and a card image moved in.
        host.menu.press(MenuKey::Down);host.menu.press(MenuKey::Confirm);host.settle();
        check(host.menu.screen().title=="Memory card (slot 1)" && labels(host.menu.screen())==std::vector<std::string>{"New empty card"},"no cards yet");
        touch(program/"saves"/"My card"/"BASLUS-21441DBZT2"/"BASLUS-21441DBZT2","save");touch(program/"saves"/"pcsx2 card.ps2","image");
        launcher.refresh();host.settle();
        check(labels(host.menu.screen())==std::vector<std::string>{"My card","pcsx2 card.ps2","New empty card"} && settings.memory_card=="My card","a card moved in is found and put in the slot");
        check(!host.menu.screen().entries[1].enabled && host.menu.screen().entries[1].value()=="unsupported","a card image is listed as not usable");
        picture(host,"4-cards");
        host.menu.press(MenuKey::Down);check(host.menu.screen().highlighted==2,"the card image cannot be chosen");
        host.menu.press(MenuKey::Confirm);host.settle();
        check(settings.memory_card=="Card 1" && fs::is_directory(program/"saves"/"Card 1") && labels(host.menu.screen()).size()==4,"a new empty card");
        check(fs::exists(program/"saves"/"My card"/"BASLUS-21441DBZT2"/"BASLUS-21441DBZT2"),"the other card is untouched");
        host.menu.press(MenuKey::Back);
        // A list screen that is not open is not rebuilt over the first screen.
        touch(program/"saves"/"Late card"/"x");launcher.refresh();host.settle();
        check(host.menu.depth()==1 && host.menu.screen().title=="BT2 Recomp","refresh leaves the first screen alone");
        host.menu.press(MenuKey::Up);host.menu.press(MenuKey::Up);picture(host,"5-ready");
        check(discArea(host)>110,"a disc in: the disc mark blinks in the corner");
        check(discArea(host,4.0)>110 && discArea(host,7.0)<60,"it blinks for a few seconds, then goes away");
        host.menu.press(MenuKey::Confirm);host.settle();
        check(host.finished && launcher.result==LauncherResult::Start,"Start game with a disc");
        for(int n=0;n<4;++n)host.menu.press(MenuKey::Down);
        host.menu.press(MenuKey::Confirm);host.settle();picture(host,"6-display");
        fs::remove_all(program);
    }
    std::cout<<"Menu checks passed.\n";
}
