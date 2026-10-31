#include "bt2_plugin.h"
#include "link.h"
#include "fight_session.h"
#include <ctime>
#include "opening_music.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <functional>
#include <initializer_list>
#include <vector>
#include <SDL3/SDL.h>

// This is Dragon Rage's own part of BT2, in a library of its own (bt2_plugin.h): the Online Mode item of the main menu and
// its screens. It was written against the engine's own types and keeps their names: the registers (Bt2Regs) and the engine's
// record of the call (Bt2Call) are what the engine gives it, and every call into the game goes through the table H.
using R5900Context=Bt2Regs;
using PS2Runtime=Bt2Call;
static const Bt2HostApi* H=nullptr;

// The main menu (overlay DBZP.BIN) as the game has it, read from its code (addresses BT2's, USA disc):
// - its state, at *0x421380: +0x0C the guide's speech, +0x10/+0x38/+0x60 three layers (0x28 apart; the wheel and
//   its icons in the second, the guides and the message window in the third), +0xE4 flags (1|2: leaving), +0xE8
//   the cursor, +0xEC the message shown (8: the greeting; item n's description is n+9), +0xF0 the wheel's top item
//   (the cursor minus two, as it was before the last move), +0xF4 which guide speaks, +0xF8 the leaving count,
//   +0xFC the greeting spoken, +0x100/+0x104 the chosen item's icon animation;
// - each frame (0x3B4250): the guides (0x3B3E50), the drawing (0x3B37A8), then the pad (0x3B3F38) when the wheel
//   stands still; a move (0x3B3DE8) restarts the icon and shows and speaks the new item's description;
// - the wheel shows five items, (top + i) of ten, each a plate of the sheet of plates (frame item / 4, row item % 4,
//   64 lines); the chosen one's name is a row of the sheet of names and its icon an animation (frames per item at
//   0x4213D8; Dueling's, item 3, at its own speed).
// Here the same with eleven: item 10 is Online Mode. Its icon is Dueling's; its description the greeting, unspoken.
namespace {
constexpr std::uint32_t state_pointer=0x421380,progress_pointer=0x3876A4,pad=0x3B0B00;
constexpr std::uint32_t init_menu=0x3B3100,guides=0x3B3E50,draw_menu=0x3B37A8,moved_menu=0x3B3DE8,pad_menu=0x3B3F38;
constexpr std::uint32_t items=11,online_item=10,dueling_item=3,greeting=8;
// the game's helpers: layers and their parts (by name), the guides, the message window, sound effects
constexpr std::uint32_t part_find=0x26EBC0,part_frame=0x26EC18,part_rect=0x26EC48,part_animate=0x26EC78,part_scroll=0x26ED60;
constexpr std::uint32_t layer_still=0x26EB60,layer_play=0x26EB78,layer_step=0x26E9E8,layer_draw=0x26EA58,guides_draw=0x24C060;
constexpr std::uint32_t guide_eyes=0x2495A8,guide_talks=0x249778,guide_quiet=0x2498D8,message_window=0x249FB8;
constexpr std::uint32_t speak=0x2493C0,speech_state=0x1018B8,leave_fade=0x243A80,sound=0x297FE0;
// names and tables in the overlay's data
constexpr std::uint32_t icon_rect=0x4213C0,icon_frames=0x4213D8,icon_part=0x421400,row_rect=0x421418,name_part=0x421430;
constexpr std::uint32_t arrow_rect=0x421448,arrow_part=0x421460,wheel_parts=0x421470,guide_parts=0x4216F0,window_part=0x421740;
constexpr std::uint32_t bg_rect=0x421750,bg_speeds=0x421768,bg_parts=0x421770,play_up=0x421790,play_down=0x421780,play_next=0x4217A0;
// the modes the ten items start (the first: 7 once the story has begun, 6 before)
constexpr std::array<std::uint32_t,10> item_modes{6,0xA,0x18,0x1E,0x3D,0x35,0x5,0x26,0x30,0x2B};

std::uint32_t read32(const std::uint8_t* rdram,std::uint32_t at) {std::uint32_t v;std::memcpy(&v,rdram+at,4);return v;}
std::uint64_t read64(const std::uint8_t* rdram,std::uint32_t at) {std::uint64_t v;std::memcpy(&v,rdram+at,8);return v;}
void write32(std::uint8_t* rdram,std::uint32_t at,std::uint32_t v) {std::memcpy(rdram+at,&v,4);}
void write64(std::uint8_t* rdram,std::uint32_t at,std::uint64_t v) {std::memcpy(rdram+at,&v,8);}
void setRegister(R5900Context* c,int index,std::uint32_t value) {c->r[index].low=static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(value)));}
std::uint32_t wrap(std::int32_t item) {return static_cast<std::uint32_t>((item%static_cast<std::int32_t>(items)+items)%items);}

struct Guest {
    std::uint8_t* rdram;R5900Context* c;PS2Runtime* runtime;std::uint32_t self;
    // A call as a JAL makes it: a0-a3 and t0-t3, f12 and f13; returns v0. The callee returns to `self`.
    std::uint32_t call(std::uint32_t target,std::initializer_list<std::uint32_t> arguments={},float f12=0,float f13=0) {
        int index=4;
        for(const auto argument:arguments)setRegister(c,index++,argument);
        c->f[12]=f12;c->f[13]=f13;
        setRegister(c,31,self);
        H->call(runtime,c,target,self);
        return static_cast<std::uint32_t>(c->r[2].low);
    }
    std::uint32_t state() const {return read32(rdram,state_pointer);}
    std::uint32_t get(std::uint32_t offset) const {return read32(rdram,state()+offset);}
    void set(std::uint32_t offset,std::uint32_t value) {write32(rdram,state()+offset,value);}
    void copy(std::uint32_t to,std::uint32_t from,std::uint32_t bytes) {std::memmove(rdram+to,rdram+from,bytes);}
};
// The replaced function returns where its caller's JAL left $ra.
void returnTo(Guest& g,std::uint32_t ra) {
    g.c->pc=ra;
    H->ret(g.runtime,g.c,ra,g.self);
}

constexpr int slot_init=0,slot_menu_mode=1,slot_guides=2,slot_moved=3,slot_pad=4,slot_draw=5,slot_frame_pad=6,slot_pad_connected=7,slot_brightness=8,slot_duel_pad=9; // (the engine's names for what it replaced)
std::uint32_t plate_entry=0; // the texture entry of the third sheet of plates, given 256 lines

// The third sheet of plates (512 x 128: items 8 and 9) is drawn as 512 x 256, so a third row (lines 128 to 191)
// can hold item 10's plate: its texture entry (0x40 bytes: +0x08 the pixels' packet size, +0x30 TEX0), the last of
// a file of three sheets of 512 wide (two of 256 lines, then this one), gets TH 8 instead of 7. Only what is drawn
// changes; the upload is the game's own. The renderer fills the new lines (artSwap below).
void buildPlate(std::uint8_t* rdram,std::uint32_t sheet_entry); // (below)
void growPlateSheet(std::uint8_t* rdram,bool now) {
    static unsigned waited=0;
    if(plate_entry && (read64(rdram,plate_entry+0x30)>>30&15)==8)return;
    plate_entry=0;
    if(!now && ++waited%120)return; // (a look through main RAM: at the menu's set-up, then every two seconds until found)
    const auto sheet=[&](std::uint32_t e,std::uint32_t th,std::uint32_t size) {
        const std::uint64_t t0=read64(rdram,e+0x30);
        return read32(rdram,e+8)==size && (t0>>20&63)==0x13 && (t0>>14&63)==8 && (t0>>26&15)==9 && (t0>>30&15)==th;
    };
    for(std::uint32_t e=0x100000;e+0x40<0x2000000;e+=0x10) {
        if(!sheet(e,7,0x10080) || !sheet(e-0x40,8,0x20080) || !sheet(e-0x80,8,0x20080))continue;
        if(read32(rdram,e-0xA0)!=3 || read32(rdram,e-0x9C)!=8)continue; // the file's header: three entries, from word 8
        write64(rdram,e+0x30,read64(rdram,e+0x30)&~(std::uint64_t{15}<<30)|std::uint64_t{8}<<30);
        plate_entry=e;
        std::fprintf(stderr,"Main menu item: the third sheet of plates (entry 0x%X) drawn with 256 lines\n",e);
        buildPlate(rdram,e-0x80); // (the first sheet of the file: Dueling's plate is a row of it)
        return;
    }
}

void initMenu(std::uint8_t* rdram,R5900Context* c,PS2Runtime* runtime) {
    H->call_original(runtime,c,slot_init);
    // The wheel's top from the cursor kept from the last visit, of eleven (the game counts ten).
    const std::uint32_t state=read32(rdram,state_pointer);
    write32(rdram,state+0xF0,wrap(static_cast<std::int32_t>(read32(rdram,state+0xE8))-2));
    growPlateSheet(rdram,true);
}

void speakItem(Guest& g) {
    const std::uint32_t item=g.get(0xE8);
    if(item==online_item) {g.set(0xEC,greeting);return;} // (no line of the guides is about it)
    g.set(0xEC,item+9);
    g.set(0xF4,(item^1)&1);
    g.call(speak,{g.get(0x0C),0x36DC,0x3AF5,0,g.get(0xEC)});
}

void movedMenu(std::uint8_t* rdram,R5900Context* c,PS2Runtime* runtime) {
    Guest g{rdram,c,runtime,moved_menu};const std::uint32_t ra=static_cast<std::uint32_t>(c->r[31].low);
    g.set(0x100,0);g.set(0x104,0);
    speakItem(g);
    returnTo(g,ra);
}

void guidesMenu(std::uint8_t* rdram,R5900Context* c,PS2Runtime* runtime) {
    Guest g{rdram,c,runtime,guides};const std::uint32_t ra=static_cast<std::uint32_t>(c->r[31].low);
    for(std::uint32_t i=0;i<3;++i)g.call(layer_step,{g.state()+0x10+i*0x28});
    // After the greeting: the chosen item's description (on Online Mode the greeting stays).
    if(g.get(0xEC)==greeting && g.get(0xFC) && g.get(0xE8)!=online_item && g.call(speech_state)==5)speakItem(g);
    returnTo(g,ra);
}

// The Online Mode screen. Choosing the item leaves the menu as any item does (its "next" animation, the fade), to
// the menu's own mode (4) again, marked: the menu's mode function (0x3B4250, below) then runs the screen instead of
// the menu, with the frame's functions the menu calls every frame and none of its own, and runs the menu after it
// (set up anew, the cursor on Online Mode). On the screen: Online or Ranked with up and down, cross shows that it is
// coming soon, triangle or circle goes back. Its pictures are drawn at the flip (overlay below), the opening's
// music under it (opening_music.h).
// page 0: Online or Ranked; page 1 (Online chosen): Create room, Join room, Quick match. The app reads which page is open
// (Bt2MenuItem_OnlineState, in the engine's running counts) to show its own pieces over the game: the player's name,
// the room code, the search. Everything that talks to the network is the app's.
// wait: 0, or 1 + the way in chosen (create a room, join one, quick match) while the app works on it: the engine only draws
// its background and watches for the app's word that someone is there (the file <online dir>/online_peer, the opponent's name).
// linking: the app has the other player's addresses and the two games are saying hello (link.h); found: the line is up and the
// opponent is shown; failed: no answer in time. The app is told by the state below (15 linking, 14 up, 16 failed) and by files.
struct Screen {bool open=false,soon=false,leaving=false;std::uint32_t choice=0;int fade=0;int page=0;int wait=0;int found=0;int linking=0;int failed=0;};
Link link;
std::uint32_t loop_frame=0;
Screen screen;
std::atomic<int> online_state{0}; // 0 closed, 1 + page while a page is open
bool screen_next=false; // the menu was left for the screen
std::uint32_t flips=0,menu_drawn=~0u,menu_item=0; // the menu drawn in this frame (its flip number), its item
std::atomic<bool> menu_active{false}; // the main menu drawn this frame or the last (the renderer's thread reads it)
std::string disc_image;
OpeningMusic music;
unsigned music_volume=100; // the player's, kept while the game's music is silenced

constexpr std::uint32_t menu_mode=0x3B4250,main_menu_mode=4;
constexpr std::uint32_t frame_begin=0x2711A0,frame_pad=0x28B090,frame_pad2=0x28BDA0,frame_sound=0x297978,frame_fade=0x2438E8;
constexpr std::uint32_t frame_draw=0x2A0800,frame_fade_draw=0x2437E8,frame_end=0x271240,fade_in=0x243968,faded_out=0x243BA8;
constexpr std::uint32_t paused=0x243BC0,pause_a=0x101890,pause_b=0x101498;

// The app's words, files in <BT2_ONLINE_DIR>: online_peer (an opponent is there: his name), online_back (go back to the three
// ways in). `clear`: removes the file; else tells whether it is there.
std::string onlinePath(const char* name) {return std::string(H->online_dir?H->online_dir:"")+"/"+name;}
void writeOnlineFile(const char* name,const std::string& text) {
    if(!H->online_dir)return;
    // (written aside and renamed: the app reads these files at any moment and must never see one half written)
    const std::string path=onlinePath(name),aside=path+".tmp";
    {std::ofstream out(aside,std::ios::binary|std::ios::trunc);out<<text;}
    std::error_code error;
    std::filesystem::rename(aside,path,error);
}
bool onlineFile(const char* name,bool clear) {
    const char* dir=H->online_dir;
    if(!dir)return false;
    const std::string path=onlinePath(name);
    std::error_code error;
    if(clear)return std::filesystem::remove(path,error);
    return std::filesystem::exists(path,error);
}

// The other player, as the app leaves him in online_peer: his name, the room's code and his addresses, a line each.
bool readPeer(std::string& name,std::string& code,std::vector<std::string>& addresses) {
    std::ifstream in(onlinePath("online_peer"),std::ios::binary);
    if(!in || !std::getline(in,name))return false;
    std::getline(in,code);
    addresses.clear();
    for(std::string line;std::getline(in,line);) {
        while(!line.empty() && (line.back()=='\r' || line.back()==' '))line.pop_back();
        if(!line.empty())addresses.push_back(line);
    }
    return true;
}
// What the app shows of the line: online_link, "rtt=<ms>".
void writeLinkFile() {writeOnlineFile("online_link","rtt="+std::to_string(link.roundTripMs())+"\n");}
// The host's relay (relay.h): the app leaves the service's TURN login in online_turn (server, user, credential, a line each);
// once the relayed address is there it is left in online_relay for the app to give the service.
bool relayPublished=false,localWritten=true;
// A cover over the frame from the moment the screen is left until the next mode shows (so the main menu, which the game passes through, is not seen).
std::atomic<int> cover{0};
std::atomic<bool> cover_menu_seen{false};
// The mode the game goes to when the line is up (0: back to the three ways in). For finding the way to the two-player versus: the file
// online_mode_test of the user folder holds a mode number (29 to 34 are Dueling's), read at that moment so it can be changed between tries.
std::uint32_t go_to_mode=0,auto_pick=0;
// A match: the line stays open from the moment the opponent is found. In the Dueling menu (duel_lock) the player can only take "1P VS 2P"
// (down once, then cross), or leave with triangle; when the opponent leaves or is lost the screen goes black with a message and the game
// goes back, by triangles, to the main menu mode, where the Online screen opens again (screen_next).
bool session=false,duel_lock=false,returning=false,down_sent=false,transition_mute=false;
std::uint32_t mute_frames=0;
std::uint32_t mode_frames=0,return_frames=0;
// The automatic walk: from "Connected" the screen is black and the sound off while the game takes itself, with the presses of a script (the Dueling menu's
// "1P VS 2P", then what comes before the character selection), to the character selection; nobody presses meanwhile. Then the screen and the sound come back.
bool auto_phase=false;
std::uint32_t auto_frames=0,ready_frames=0,walk_end_wait=0;
std::atomic<bool> nocover{false}; // (development: the file online_nocover shows what happens under the black)
std::atomic<int> left_cover{0};
constexpr std::uint32_t duel_mode=30;
// Dueling's menu (found by comparing the memory with the cursor in other places): its state is the structure the word at 0x42BB84 points to, the cursor
// at +0xF4 (0 1P VS COM, 1 1P VS 2P, 2 COM VS COM, 3 Battle Settings); 0x41B648 tells the menu the cursor moved. "1P VS 2P" is dimmed by the game
// (init 0x41A988, draw 0x41AE58) and refused (pad 0x41B7D8) while func 0x28C4B8(1) says the second controller is not there: with an opponent connected
// the answer is "there" (padConnected), and the three other entries are dimmed over the frame (pluginOverlay).
constexpr std::uint32_t duel_state_pointer=0x42BB84,duel_moved=0x41B648,pad_connected=0x28C4B8;
std::atomic<bool> dim_others{false};
constexpr std::uint32_t brightness=0x26EF88; // (layer, parameters, f12 = the entry's brightness: the draw of Dueling's menu gives 1.0, or 0.4 to a dimmed "1P VS 2P")
std::uint32_t modeToTry() {
    std::ifstream in(onlinePath("online_mode_test"),std::ios::binary);
    int n=0;
    return (in && (in>>n) && n>=1 && n<=63)?static_cast<std::uint32_t>(n):0;
}
// The line's notes (link.h) go to the game's log: the run log of the player's phone then says what the network did.
void flushNotes() {
    if(!H->log)return;
    for(const auto& n:link.takeNotes())H->log(n.c_str());
}
void relayStep() {
    if(!link.relayStarted()) {
        std::ifstream in(onlinePath("online_turn"),std::ios::binary);
        std::string server,user,credential;
        if(in && std::getline(in,server) && std::getline(in,user) && std::getline(in,credential)) {
            for(auto* t:{&server,&user,&credential})while(!t->empty() && (t->back()=='\r' || t->back()==' '))t->pop_back();
            if(!server.empty() && !user.empty())link.beginRelay(server,user,credential);
        }
    } else if(link.relayActive() && !relayPublished) {
        writeOnlineFile("online_relay",link.relayAddress()+"\n");
        relayPublished=true;
    }
}
// A way in is over (given up, or the opponent has been shown): the line closes and the app's files go.
void endWait(bool keep_link=false) {
    if(!keep_link)link.close();
    screen.wait=0;screen.found=0;screen.linking=0;screen.failed=0;
    for(const char* name:{"online_peer","online_back","online_local","online_link","online_turn","online_relay","online_nat"})onlineFile(name,true);
    relayPublished=false;localWritten=true;
}

// The lobby: the other player as the app gave him, kept until the line is up and the match is asked for.
std::string pending_name,pending_code;
std::vector<std::string> pending_addresses;
bool restarting=false;

bool hostHas(std::size_t upTo) {return H && H->size>=upTo;}
bool fightAvailable() { // an engine that has the second pad, fast forward and volume, and no file that turns the match off
    return hostHas(offsetof(Bt2HostApi,set_master_percent)+sizeof(H->set_master_percent)) && H->set_second_pad && H->set_fast_forward && H->set_master_percent &&
        !onlineFile("online_nofight",false) && !pending_addresses.empty();
}
// The line is up and both sides are shown: the match's settings go to a file and the app starts the engine again (the lobby's line stays up meanwhile).
void restartForMatch() {
    SessionFile file;
    std::string role;
    {std::ifstream in(onlinePath("online_role")); in>>role;}
    {std::ifstream in(onlinePath("online_name")); std::getline(in,file.myName);}
    file.host=role=="host";
    file.port=link.port();
    file.code=pending_code;
    file.peerName=pending_name;
    file.addresses=pending_addresses;
    file.at=std::time(nullptr);
    writeOnlineFile("online_session",formatSessionFile(file));
    writeOnlineFile("online_restart","session\n");
    cover.store(1<<30);
    std::fprintf(stderr,"online: the match starts in a fresh engine (%s, port %u, %zu address(es))\n",file.host?"host":"guest",unsigned(file.port),file.addresses.size());
}
void screenLoop(Guest& g) {
    const std::function<void(std::uint32_t)> effect=[&](std::uint32_t which) {g.call(sound,{0,which,0x40,0x40,1});};
    screen={true,false,false,0,0,0,0,0};
    music_volume=H->stream_percent();H->set_stream_percent(0); // (the menu's music stays silent)
    music.start(disc_image);
    g.call(fade_in,{0,0,0});
    for(;;) {
        for(const auto f:{frame_begin,frame_pad,frame_pad2,frame_sound,frame_fade,frame_draw,frame_fade_draw,frame_end})g.call(f);
        if(g.call(paused)) {g.call(pause_a);g.call(pause_b);continue;}
        if(g.call(faded_out))break;
        screen.fade=std::clamp(screen.fade+(screen.leaving?-9:9),0,128);
        online_state.store(screen.leaving?0:screen.failed?16:screen.found?14:screen.linking?15:screen.wait?10+screen.wait:1+screen.page,std::memory_order_relaxed);
        ++loop_frame;
        if(screen.leaving)continue;
        const std::uint32_t held=read32(g.rdram,pad+0x144),pressed=read32(g.rdram,pad+0x134);
        if(screen.wait) { // the app is making the connection
            link.poll();
            if(!localWritten && link.addressesReady()) { // this game's addresses for the app: the router's first (stun.h), then the home's
                std::string lines;for(const auto& a:link.localAddresses())lines+=a+"\n";
                writeOnlineFile("online_local",lines);
                writeOnlineFile("online_nat",link.symmetricRouter()?"symmetric\n":"cone-or-unknown\n");
                localWritten=true;
                std::string flat=lines;for(auto& ch:flat)if(ch=='\n')ch=' ';
                if(H->log){const std::string note="online: addresses for the app: "+flat+(link.symmetricRouter()?"(the router gives a port per destination)":"");H->log(note.c_str());}
            }
            flushNotes();
            if(loop_frame%15==0)relayStep();
            if(screen.found) {
                if(loop_frame%30==0)writeLinkFile();
                if(restarting) {} // (the app is starting the engine again for the match; the line stays up so the other side does not think this one left)
                else if(--screen.found==0 && fightAvailable()) {restartForMatch();restarting=true;screen.found=1;}
                else if(screen.found==0) {
                    const std::uint32_t mode=modeToTry()?modeToTry():duel_mode; // (the file online_mode_test tries another mode)
                    endWait(true); // the line stays: the match is on
                    go_to_mode=mode;screen.leaving=true;effect(1);g.call(leave_fade,{0,0,0});
                    session=true;duel_lock=mode==duel_mode;down_sent=false;mode_frames=0;auto_phase=duel_lock;auto_frames=0;ready_frames=0;walk_end_wait=0;
                }
            } else if(pressed&0x3000 || onlineFile("online_back",true)) {endWait();effect(2);} // triangle, circle, or the app: given up
            else if(screen.failed) {
                // (waits for the player's triangle)
            } else if(!screen.linking) {
                std::string name,code;std::vector<std::string> addresses;
                if(loop_frame%8==0 && readPeer(name,code,addresses)) {
                    // (an opponent with no address to reach him by, because his own game found none: there is no line to make, and "Connected" would be a lie)
                    if(addresses.empty()) {screen.failed=1;writeOnlineFile("online_nat","no-address\n");}
                    else {pending_name=name;pending_code=code;pending_addresses=addresses;link.start(code,addresses);screen.linking=1;}
                }
            } else if(link.state()==Link::Up) {
                screen.linking=0;screen.found=300;writeLinkFile();effect(1);
            } else if(link.state()==Link::Failed) {
                screen.failed=1;
            }
        } else if(screen.soon) {
            if(pressed&0x7000){screen.soon=false;effect(2);} // cross, circle or triangle: the message closes
        } else if(held&0x50) { // up, down: the list turns
            const std::uint32_t n=screen.page?3u:2u;
            screen.choice=(screen.choice+(held&0x10?n-1:1))%n;effect(0);
        } else if(pressed&0x4000) { // cross
            if(screen.page==0 && screen.choice==0) {screen.page=1;screen.choice=0;} // Online: the three ways in
            else if(screen.page==1) { // create, join, quick: the app takes it; the socket first, for the addresses it will give the service
                screen.wait=1+int(screen.choice);
                // (online_local too: one left by a run that ended badly would give the app a port nobody listens on)
                for(const char* name:{"online_peer","online_back","online_link","online_local","online_nat"})onlineFile(name,true);
                localWritten=!link.open(); // (the addresses are written once the STUN answers are in: the loop below)
                if(localWritten)writeOnlineFile("online_local","error\n"); // (no socket: the app says so instead of giving a port nobody listens on)
            }
            else screen.soon=true; // (Ranked: coming soon)
            effect(1);
        } else if(pressed&0x3000) { // triangle, circle
            if(screen.page==1) {screen.page=0;screen.choice=0;effect(2);} // back to Online / Ranked
            else {
                screen.leaving=true;effect(2); // back to the menu
                g.call(leave_fade,{0,0,0});
            }
        }
    }
    music.stop();
    if(go_to_mode)transition_mute=true; // the sound stays off while the game goes on to the next mode; framePad gives it back when that mode shows
    else H->set_stream_percent(music_volume);
    screen={};
    online_state.store(0,std::memory_order_relaxed);
}

// The walk: buttons pressed for the game, by a script, through the menus to the character selection of a two-player versus (what BT3 does with
// a fresh start-up: the same on both sides, so both arrive at the same state). online_walk in the user folder, a line each, "<frames to wait> <button>"
// (up down left right cross circle triangle square start select); the default is Dueling's menu: down to "1P VS 2P", cross.
struct WalkStep {std::uint32_t wait,bits;};
std::vector<WalkStep> walk_steps;
std::size_t walk_at=0;
std::uint32_t walk_wait=0;
bool walking=false;
std::uint32_t buttonBits(const std::string& name) {
    static const std::pair<const char*,std::uint32_t> table[]={{"select",0x1},{"start",0x8},{"up",0x10},{"right",0x20},{"down",0x40},{"left",0x80},
        {"triangle",0x1000},{"circle",0x2000},{"cross",0x4000},{"square",0x8000}};
    for(const auto& t:table)if(name==t.first)return t.second;
    return 0;
}
void startWalk() {
    walk_steps.clear();
    std::ifstream in(onlinePath("online_walk"));
    std::string line;
    bool any=false;
    while(std::getline(in,line)) {
        std::istringstream words(line);
        std::uint32_t wait=0;std::string button;
        if((words>>wait>>button) && buttonBits(button)) {walk_steps.push_back({wait,buttonBits(button)});any=true;}
    }
    if(!any)walk_steps={{150,0x4000}}; // (no script file: after the Dueling menu, "Single Battle")
    walk_at=0;walk_wait=walk_steps[0].wait;walking=true;
    std::fprintf(stderr,"Online Mode: the walk through the menus starts (%zu presses)\n",walk_steps.size());
}
// The match is over from this side (the player left, or the opponent was lost): the line closes and the game goes back to the Online screen.
void leaveSession(bool opponent_gone) {
    if(!session)return;
    if(!opponent_gone)link.sayBye();
    link.close();
    session=false;duel_lock=false;dim_others.store(false);auto_phase=false;walking=false;cover.store(0);
    if(transition_mute){transition_mute=false;H->set_stream_percent(music_volume);}
    if(opponent_gone)left_cover.store(900); // (kept until the Online screen is open again, 15 seconds at most)
    returning=true;return_frames=0;screen_next=true;
    std::fprintf(stderr,"Online Mode: the match ends (%s)\n",opponent_gone?"the opponent left or was lost":"the player left");
}

// ---- A match's own process (fight_session.h). The lobby (the Online screens) writes `online_session` and asks the app to start the engine again
// (`online_restart`); that fresh engine finds the file here, makes the line again on the lobby's port, lets the game walk its menus by itself (silent, not
// held to the clock, the picture covered) to the character select of the two-player versus, waits there for the other side and runs the lockstep.
// When the match is over the app is asked to start the engine once more as it normally is.
FightSession fight;
std::atomic<bool> fight_on{false};
bool fight_booted=false;                    // the game reached the main menu (the walk takes over from there)
std::uint32_t fight_frames=0,fight_menu_frames=0,fight_over_frames=0;
std::atomic<bool> fight_over{false};
void startFightProcess(const SessionFile& file) {
    FightSession::Env env;
    env.log=[](const std::string& text) {std::fprintf(stderr,"%s\n",text.c_str());};
    env.ram=H->ram;
    env.ramSize=0x2000000;
    env.strictDesync=onlineFile("online_strict",false);
    env.onMet=[]() { // both are at the character select: the picture and the sound come back, and the app is told who plays
        H->set_fast_forward(0);
        H->set_master_percent(100);
        H->set_stream_percent(100);
        cover.store(0);
        const bool first=fight.iAmPlayer1();
        writeOnlineFile("online_match",std::string("p1=")+(first?fight.myName():fight.peerName())+"\np2="+(first?fight.peerName():fight.myName())+"\nme="+(first?"1":"2")+"\n");
    };
    env.onEnded=[](const std::string& why) {
        H->set_fast_forward(0);
        H->set_master_percent(100);
        H->set_stream_percent(100);
        onlineFile("online_match",true);
        if(why!="the player left")left_cover.store(900); // (black, with the message that the other player left)
        fight_over.store(true,std::memory_order_release);
    };
    if(!fight.begin(file,env)) {
        std::fprintf(stderr,"online: the match could not start; the engine goes on as it is\n");
        return;
    }
    H->set_second_pad(1);
    H->set_fast_forward(1);
    H->set_master_percent(0);
    H->set_stream_percent(0);
    fight_on.store(true,std::memory_order_release);
    // the walk of the lobby's own session (duel_lock, auto_pick): from the main menu to the character select of "1P VS 2P"
    session=true;duel_lock=true;auto_phase=true;auto_pick=duel_mode;mode_frames=0;ready_frames=0;walk_end_wait=0;down_sent=false;auto_frames=0;
    cover.store(1<<30);
}
// Each frame of a match's process, from the game's own frame hook.
void fightFrame(std::uint8_t* rdram,std::uint32_t mode) {
    ++fight_frames;
    if(fight_over.load(std::memory_order_acquire)) { // over: the message for a few seconds, then the app starts the engine as it normally is
        if(++fight_over_frames==240) {
            onlineFile("online_session",true);
            writeOnlineFile("online_restart","normal\n");
        }
        return;
    }
    if(!fight_booted) { // the game's start-up: start and cross until the main menu is there
        if(mode==main_menu_mode) {fight_booted=true;auto_frames=0;std::fprintf(stderr,"online: the main menu is up after %u frames\n",fight_frames);}
        else if(fight_frames%24==0) {
            const std::uint32_t bits=(fight_frames/24)%2?0x8u:0x4000u;
            write32(rdram,pad+0x144,read32(rdram,pad+0x144)|bits);
            write32(rdram,pad+0x134,read32(rdram,pad+0x134)|bits);
        }
        return;
    }
    if(fight.running()) { // back at the main menu: the player left the versus, which ends the match for both
        if(mode==main_menu_mode) {if(++fight_menu_frames>45)fight.requestLeave();}
        else fight_menu_frames=0;
    }
}
int pluginPadField(const std::uint8_t local[6],std::uint8_t p1[6],std::uint8_t p2[6]) {
    return fight_on.load(std::memory_order_relaxed)?fight.padField(local,p1,p2):0;
}

// For finding where a menu keeps its state (development only): while the file online_dump is in the user folder, the game's whole memory goes to
// dump_<n>.bin there, one per touch of the file (compare the dumps with the cursor in other places).
void dumpMemory(std::uint8_t* rdram) {
    static unsigned count=0,frame=0;
    if(++frame%15!=0 || !onlineFile("online_dump",true))return;
    const std::string path=onlinePath(("dump_"+std::to_string(count++)+".bin").c_str());
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(rdram),0x2000000);
    std::fprintf(stderr,"Online Mode: memory dump to %s\n",path.c_str());
}

// Development only: online_press holds a button name (up, down, cross...); the game gets it held for one field, as if the player pressed it.
std::uint32_t press_bits=0;
void pressFromFile(std::uint8_t* rdram) {
    static unsigned frame=0;
    if(++frame%15==7) {
        std::ifstream in(onlinePath("online_press"));
        std::string name;
        if(in && (in>>name)) {press_bits=buttonBits(name);onlineFile("online_press",true);}
    }
    if(press_bits) {write32(rdram,pad+0x144,read32(rdram,pad+0x144)|press_bits);press_bits=0;}
}

void framePad(std::uint8_t* rdram,R5900Context* c,PS2Runtime* runtime) {
    H->call_original(runtime,c,slot_frame_pad);
    dumpMemory(rdram);
    pressFromFile(rdram);
    const std::uint32_t progress=read32(rdram,progress_pointer);
    const std::uint32_t mode=progress?read32(rdram,progress+0x1C):0;
    if(fight_on.load(std::memory_order_relaxed))fightFrame(rdram,mode);
    if(session && !fight_on.load(std::memory_order_relaxed)) {
        link.poll();
        flushNotes();
        if(link.state()!=Link::Up || link.peerLeft() || link.silentMs()>6000) {
            if(H->log){const std::string note=std::string("online: the match ends: ")+(link.peerLeft()?"the other player said goodbye":link.state()!=Link::Up?"the line is not up":"no packet for "+std::to_string(link.silentMs())+" ms");H->log(note.c_str());}
            leaveSession(true);
        }
    }
    if(transition_mute && ++mute_frames>1800) {transition_mute=false;mute_frames=0;H->set_stream_percent(music_volume);}
    if(session && auto_phase) {
        const std::uint32_t pressed=read32(rdram,pad+0x134);
        if(duel_lock && mode_frames>20 && (pressed&0x1000))leaveSession(false); // triangle: the game's own way back, and the line closes
        else {write32(rdram,pad+0x144,0);write32(rdram,pad+0x134,0);} // (nobody presses while the game takes itself to the selection)
    }
    if(session && duel_lock) {
        if(mode==duel_mode)++mode_frames;
        const std::uint32_t state=read32(rdram,duel_state_pointer);
        const bool menu_ready=mode==duel_mode && mode_frames>=60 && state>0x100000 && state<0x1FFFF00;
        dim_others.store(menu_ready,std::memory_order_relaxed);
        if(menu_ready && read32(rdram,state+0xF4)!=1) { // the cursor goes to "1P VS 2P" and the menu is told
            write32(rdram,state+0xF4,1);
            const R5900Context saved=*c;
            Guest g{rdram,c,runtime,frame_pad};
            g.call(duel_moved);
            *c=saved;
        }
        if(menu_ready && read32(rdram,state+0xF4)==1 && ++ready_frames==12) { // "1P VS 2P" is taken, and the walk goes on from there
            write32(rdram,pad+0x144,read32(rdram,pad+0x144)|0x4000);write32(rdram,pad+0x134,read32(rdram,pad+0x134)|0x4000);
            duel_lock=false;dim_others.store(false);
            startWalk();
        }
    }
    if(session && auto_phase) { // the end of the walk: a moment after its last press the screen and the sound come back
        const bool done=!duel_lock && !walking && walk_steps.size() && walk_at>=walk_steps.size();
        if(done && walk_end_wait==0)walk_end_wait=90;
        if(fight_on.load(std::memory_order_relaxed) && !fight_booted) {} // (the start-up is still on its way to the main menu)
        else if((walk_end_wait && --walk_end_wait==0) || ++auto_frames>2400) {
            auto_phase=false;
            if(fight_on.load(std::memory_order_relaxed)) { // the picture stays covered until the other side is here too (FightSession::onMet)
                fight.arrived();
                std::fprintf(stderr,"online: the walk reached the character select (%u frames after the main menu)\n",auto_frames);
            } else {
                cover.store(0);
                if(transition_mute){transition_mute=false;mute_frames=0;H->set_stream_percent(music_volume);}
                std::fprintf(stderr,"Online Mode: at the end of the walk (%u fields), screen and sound back\n",auto_frames);
            }
        }
    }
    static unsigned cover_check=0;
    if(++cover_check%60==0)nocover.store(onlineFile("online_nocover",false));
    if(returning) { // triangle every twenty fields until the game is at the main menu's mode (where the Online screen opens: screen_next)
        if(mode==main_menu_mode || return_frames>900)returning=false;
        else {
            ++return_frames;
        }
    }
    if(!walking)return;
    if(walk_wait>0){--walk_wait;return;}
    const std::uint32_t bits=walk_steps[walk_at].bits;
    write32(rdram,pad+0x144,read32(rdram,pad+0x144)|bits);
    write32(rdram,pad+0x134,read32(rdram,pad+0x134)|bits);
    if(++walk_at>=walk_steps.size()){walking=false;std::fprintf(stderr,"Online Mode: the walk is over\n");}
    else walk_wait=walk_steps[walk_at].wait;
}

// The game asks whether a controller port is unplugged (1 = yes). With an opponent connected, port 1 (the second player) is plugged.
void padConnected(std::uint8_t* rdram,R5900Context* c,PS2Runtime* runtime) {
    const std::uint32_t ra=static_cast<std::uint32_t>(c->r[31].low);
    if(session && static_cast<std::uint32_t>(c->r[4].low)==1) {
        setRegister(c,2,0);
        Guest g{rdram,c,runtime,pad_connected};
        returnTo(g,ra);
        return;
    }
    H->call_original(runtime,c,slot_pad_connected);
}

// Dueling's menu draws its four entries one after the other, each with a call giving its brightness: while an opponent is connected the game itself is
// told to dim the three that are not open, as it dims "1P VS 2P" when there is no second controller.
void brightnessHook(std::uint8_t* rdram,R5900Context* c,PS2Runtime* runtime) {
    static unsigned entry=0;
    const std::uint32_t ra=static_cast<std::uint32_t>(c->r[31].low);
    static unsigned logged=0;
    if(logged<16 && dim_others.load() && ra>=0x41A988 && ra<0x41BF08) {++logged;std::fprintf(stderr,"Online Mode: brightness from 0x%X, %.2f (entry counter %u)\n",ra,double(c->f[12]),0u);}
    if(ra>=0x41B300 && ra<0x41B340) { // (from the entries' loop of the menu's draw)
        const unsigned which=entry++&3;
        if(dim_others.load(std::memory_order_relaxed) && which!=1)c->f[12]=0.02f; // (the other three are not shown: a brightness of nothing)
    }
    H->call_original(runtime,c,slot_brightness);
}

// Dueling's menu, while the game is being taken back to the Online screen: what its triangle does inside (the fade out, and the "going on" word
// its caller passes set to zero), done once the menu's layers are still, instead of pressing the button.
constexpr std::uint32_t duel_pad=0x41B7D8,layer_still_check=0x26EB60;
void duelPad(std::uint8_t* rdram,R5900Context* c,PS2Runtime* runtime) {
    const std::uint32_t ra=static_cast<std::uint32_t>(c->r[31].low),going_on=static_cast<std::uint32_t>(c->r[4].low);
    const std::uint32_t state=read32(rdram,duel_state_pointer);
    if(returning && !session && going_on && state>0x100000 && state<0x1FFFF00) {
        Guest g{rdram,c,runtime,duel_pad};
        if(g.call(layer_still_check,{state+0x38})) {
            g.call(leave_fade,{0,0,0});
            write32(rdram,going_on,0);
            g.call(sound,{0,2,0x40,0x40,1});
            returning=false;
            std::fprintf(stderr,"Online Mode: out of Dueling's menu, back to the Online screen\n");
            returnTo(g,ra);
            return;
        }
        setRegister(c,31,ra);
    }
    H->call_original(runtime,c,slot_duel_pad);
}

void menuMode(std::uint8_t* rdram,R5900Context* c,PS2Runtime* runtime) {
    if(screen_next) {
        screen_next=false;
        Guest g{rdram,c,runtime,menu_mode};
        const auto ra=c->r[31],a0=c->r[4],sp=c->r[29];
        screenLoop(g);
        c->r[31]=ra;c->r[4]=a0;c->r[29]=sp;c->pc=menu_mode;
        if(go_to_mode)cover.store(2600);
        auto_pick=go_to_mode; // the main menu comes back as a player leaves the screen; its pad step then "presses" the item (padMenu)
        go_to_mode=0;
    }
    H->call_original(runtime,c,slot_menu_mode);
}

void padMenu(std::uint8_t* rdram,R5900Context* c,PS2Runtime* runtime) {
    Guest g{rdram,c,runtime,pad_menu};const std::uint32_t ra=static_cast<std::uint32_t>(c->r[31].low),going_on=static_cast<std::uint32_t>(c->r[4].low);
    const std::function<void(std::uint32_t)> effect=[&](std::uint32_t which) {g.call(sound,{0,which,0x40,0x40,1});};
    if(g.call(layer_still,{g.state()+0x38})) {
        std::uint32_t held=read32(rdram,pad+0x144),pressed=read32(rdram,pad+0x134),item=g.get(0xE8);
        if(auto_pick) { // the cursor goes to the item and cross is pressed: the way a player picks it
            for(std::uint32_t i=0;i<item_modes.size();++i)if(item_modes[i]==auto_pick)item=i;
            g.set(0xF0,wrap(static_cast<std::int32_t>(item)-2));
            g.set(0xE8,item);
            held=0;pressed=0x4000;auto_pick=0;
        }
        if(held&0x50) { // up, down: the wheel turns from where it was
            g.set(0xF0,wrap(static_cast<std::int32_t>(item)-2));
            g.set(0xE8,wrap(static_cast<std::int32_t>(item)+(held&0x10?-1:1)));
            g.call(moved_menu);
            g.call(layer_play,{g.state()+0x38,held&0x10?play_down:play_up});
            effect(0);
        } else if(pressed&0x4000) { // cross
            {
                const std::uint32_t progress=read32(rdram,progress_pointer);
                write32(rdram,progress+0x1C,item==online_item?main_menu_mode:item==0 && (read32(rdram,progress+0x2C)&0x2000000)?7:item_modes[item]);
                screen_next=item==online_item;
                g.set(0xF0,wrap(static_cast<std::int32_t>(item)-2));
                g.set(0xE4,g.get(0xE4)|3);
                g.set(0xF8,15);
                g.call(layer_play,{g.state()+0x38,play_next});
                effect(1);
            }
        } else if(pressed&0x1000) { // triangle: back to the title
            g.call(leave_fade,{0,0,0});
            write32(rdram,going_on,0);
            effect(2);
        }
    }
    returnTo(g,ra);
}

void drawMenu(std::uint8_t* rdram,R5900Context* c,PS2Runtime* runtime) {
    Guest g{rdram,c,runtime,draw_menu};
    const std::uint32_t ra=static_cast<std::uint32_t>(c->r[31].low);
    const auto sp=c->r[29];
    const std::uint32_t frame=static_cast<std::uint32_t>(sp.low)-0x340;
    setRegister(c,29,frame);
    // The frame as the game's: +0 a part's handle, +0x10 a rectangle, +0x30 the icons' frame counts, +0x60 the
    // wheel's five part names, +0x2E0 the backgrounds' speeds.
    const std::uint32_t handle=frame,rect=frame+0x10,counts=frame+0x30,names=frame+0x60,speeds=frame+0x2E0;
    growPlateSheet(rdram,false);
    const std::uint32_t item=g.get(0xE8),wheel=g.state()+0x38;
    menu_drawn=flips;menu_item=item;menu_active.store(true,std::memory_order_relaxed);
    // the chosen item's icon (Online Mode: Dueling's, at its speed)
    g.copy(rect,icon_rect,0x14);g.copy(counts,icon_frames,0x28);
    const std::uint32_t icon=item==online_item?dueling_item:item;
    g.call(part_find,{wheel,icon_part,handle});
    g.call(part_frame,{wheel,handle,icon});
    g.call(part_animate,{wheel,handle,g.state()+0x100,g.state()+0x104,rect,read32(rdram,counts+icon*4),1,icon==dueling_item?7u:6u});
    // its name: row item % 4 of sheet item / 4 (Online Mode: the third sheet's free third row)
    g.copy(rect,row_rect,0x14);
    write32(rdram,rect+4,item%4*64);write32(rdram,rect+12,item%4*64+64);
    g.call(part_find,{wheel,name_part,handle});
    g.call(part_frame,{wheel,handle,item/4});
    g.call(part_rect,{wheel,handle,rect});
    g.copy(counts,arrow_rect,0x14);
    g.call(part_find,{wheel,arrow_part,handle});
    g.call(part_rect,{wheel,handle,counts});
    // the wheel: five plates from its top
    g.copy(names,wheel_parts,0x280);g.copy(rect,row_rect,0x14);
    for(std::uint32_t i=0;i<5;++i) {
        const std::uint32_t shown=(g.get(0xF0)+i)%items;
        write32(rdram,rect+4,shown%4*64);write32(rdram,rect+12,shown%4*64+64);
        g.call(part_find,{wheel,names+i*0x80,handle});
        g.call(part_frame,{wheel,handle,shown/4});
        g.call(part_rect,{wheel,handle,rect});
    }
    // the guides (Videl and Gohan: eyes, and the mouth of the one speaking) and the message window
    const std::uint32_t people=g.state()+0x60;
    for(std::uint32_t k=0;k<2;++k) {
        g.call(part_find,{people,guide_parts+k*0x20,handle});
        g.call(guide_eyes,{people,handle,g.state()+0x110+k*4,k?5u:1u});
        g.call(part_find,{people,guide_parts+k*0x20+(k?0x18u:0x10u),handle});
        if(g.get(0xF4)==k)g.call(guide_talks,{people,handle,g.state()+0x118+k*4,k?7u:3u});
        else g.call(guide_quiet,{people,handle,k?7u:3u});
    }
    g.call(part_find,{people,window_part,handle});
    // (Online Mode: no message of the game's, -1 draws none; its own line is drawn over the window at the flip)
    g.call(message_window,{people,handle,0,0,g.get(0x08),item==online_item?0xFFFFFFFFu:g.get(0xEC),0});
    // the two backgrounds, each scrolling at its speed
    const std::uint32_t back=g.state()+0x10;
    g.copy(counts,bg_rect,0x14);g.copy(speeds,bg_speeds,8);
    for(std::uint32_t k=0;k<2;++k) {
        float speed;std::memcpy(&speed,rdram+speeds+k*4,4);
        g.call(part_find,{back,bg_parts+k*8,handle});
        g.call(part_scroll,{back,handle,counts,g.state()+0x108+k*4,0},speed,0.0f);
    }
    // the three layers, the guides' own drawing before the last
    for(std::uint32_t k=0;k<3;++k) {
        if(k==2)g.call(guides_draw,{g.state()});
        g.call(layer_draw,{g.state()+0x10+k*0x28});
    }
    c->r[29]=sp;
    returnTo(g,ra);
}

// The item's pictures: PNGs of the app's assets in `folder` (bt2/tools/menu_item/make_menu_item.py: all drawn by the
// project, none from the disc), read as RGBA with the GS's alpha (0x80 opaque): the item's name (a row of the sheet of
// names), the word on its plate, the screen with Online chosen and with Ranked chosen, the "coming soon" message and
// the menu's description of the item. The plate itself is the game's own Dueling plate, made into this one while the menu
// is set up (buildPlate), so no picture of the game's is kept anywhere. The two sheets they go into are known by the
// FNV-1a hash of their first 128 lines as the renderer decodes them (the USA disc's).
enum Slot {online_picture,ranked_picture,soon_picture,description_picture,create_picture,join_picture,quick_picture,wait_picture,left_picture,shade_picture,title_picture,gohan_picture,pictures};
struct Picture {std::uint32_t w=0,h=0;std::vector<std::uint8_t> rgba;};
std::array<Picture,pictures> picture;
constexpr std::array<const char*,pictures> picture_names{"online","ranked","soon","description","rooms_create","rooms_join","rooms_quick","wait","left","shade","title","gohan_00"};
Picture name_picture,plate_word;
// The fighter of the Online screens is a looping clip: gohan_00.png, gohan_01.png ... in the pictures' folder and gohan.txt ("frames fps width height",
// make_fighter_video.py). Its slot is drawn like the others, but its pixels are the frame of the moment, read from its file when asked for.
struct Clip {std::uint32_t frames=0,fps=12,w=0,h=0;std::string folder;} clip;
std::atomic<std::uint32_t> clip_frame{0};
std::uint32_t clipNow() {
    if(!clip.frames)return 0;
    const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return static_cast<std::uint32_t>((ms*clip.fps/1000)%clip.frames);
}
std::vector<std::uint8_t> plate_row; // the plate: 512 x 64 RGBA
constexpr std::uint64_t names_sheet_hash=0x838260d6bb66161aull,plates_sheet_hash=0x9cb5f2297eb6c82eull;
std::atomic<bool> art_ready{false},plate_ready{false};

Picture loadPicture(const std::string& path) {
    SDL_Surface* png=SDL_LoadPNG(path.c_str());
    SDL_Surface* rgba=png?SDL_ConvertSurface(png,SDL_PIXELFORMAT_RGBA32):nullptr;
    if(png)SDL_DestroySurface(png);
    if(!rgba)return {};
    Picture p;p.w=std::uint32_t(rgba->w);p.h=std::uint32_t(rgba->h);p.rgba.resize(std::size_t{p.w}*p.h*4);
    for(std::uint32_t y=0;y<p.h;++y) {
        const auto* row=static_cast<const std::uint8_t*>(rgba->pixels)+std::size_t{y}*std::size_t(rgba->pitch);
        for(std::uint32_t x=0;x<p.w;++x) {
            std::uint8_t* out=p.rgba.data()+(std::size_t{y}*p.w+x)*4;
            out[0]=row[x*4];out[1]=row[x*4+1];out[2]=row[x*4+2];out[3]=std::uint8_t((row[x*4+3]*128+127)/255);
        }
    }
    SDL_DestroySurface(rgba);
    return p;
}

// GS memory orders of the 32-bit upload and the 8-bit read (the hardware's tables).
constexpr std::uint8_t kBlock32[4][8]={{0,1,4,5,16,17,20,21},{2,3,6,7,18,19,22,23},{8,9,12,13,24,25,28,29},{10,11,14,15,26,27,30,31}};
constexpr std::uint8_t kCol32[8][8]={{0,1,4,5,8,9,12,13},{2,3,6,7,10,11,14,15},{16,17,20,21,24,25,28,29},{18,19,22,23,26,27,30,31},
                                     {32,33,36,37,40,41,44,45},{34,35,38,39,42,43,46,47},{48,49,52,53,56,57,60,61},{50,51,54,55,58,59,62,63}};
constexpr std::uint8_t kCol8First[4][16]={{0,4,16,20,32,36,48,52,2,6,18,22,34,38,50,54},{8,12,24,28,40,44,56,60,10,14,26,30,42,46,58,62},
                                          {33,37,49,53,1,5,17,21,35,39,51,55,3,7,19,23},{41,45,57,61,9,13,25,29,43,47,59,63,11,15,27,31}};

// A texture entry of the plates' file (0x40 bytes: +0x38 the pixels' packet, +0x3C the colour table's) to 512 x 256
// RGBA with the GS's alpha: 8 bits through a 256-colour table, the pixels sent as a 256 x 128 image of 32 bits.
bool decodeSheet(const std::uint8_t* rdram,std::uint32_t entry,std::vector<std::uint8_t>& out) {
    const std::uint32_t pix_at=read32(rdram,entry+0x38),clut_at=read32(rdram,entry+0x3C);
    const char* why=nullptr;
    if(pix_at<0x100000 || clut_at<0x100000 || pix_at+0x60+512*256>=0x2000000 || clut_at+0x60+0x400>=0x2000000)why="pointers";
    else {
        const std::uint8_t* pix=rdram+pix_at,*clut=rdram+clut_at;
        if(read32(pix,0x30)!=256 || read32(pix,0x34)!=128 || pix[0x38]!=0x52 || (read32(pix,0x50)&0x7FFF)!=512*256/16)why="pixel packet";
        else if(read32(clut,0x30)!=16 || read32(clut,0x34)!=16 || clut[0x38]!=0x52 || (read32(clut,0x50)&0x7FFF)!=0x40)why="colour packet";
    }
    if(why){std::fprintf(stderr,"Main menu item: Dueling's plate is not as expected (%s: pixels 0x%X, colours 0x%X)\n",why,pix_at,clut_at);return false;}
    const std::uint8_t* pix=rdram+pix_at+0x60,*clut=rdram+clut_at+0x60;
    std::vector<std::uint8_t> mem(512*256);
    for(std::uint32_t y=0;y<128;++y)
        for(std::uint32_t x=0;x<256;++x) {
            const std::uint32_t a=(((y&~0x1Fu)*4+((x>>1)&~0x1Fu)+kBlock32[(y>>3)&3][(x>>3)&7])<<6)+kCol32[y&7][x&7];
            std::memcpy(mem.data()+a*4,pix+(y*256+x)*4,4);
        }
    out.resize(512*256*4);
    for(std::uint32_t y=0;y<256;++y)
        for(std::uint32_t x=0;x<512;++x) {
            const std::uint32_t col=(y&15)>>2,sx=(col&1)?((x&15)^4):(x&15);
            const std::uint32_t a=((((y>>1)&~0x1Fu)*4+((x>>2)&~0x1Fu)+kBlock32[(y>>4)&3][(x>>4)&7])<<8)+kCol8First[y&3][sx]+col*64;
            std::uint32_t i=mem[a];
            i=(i&0xE7)|((i&8)<<1)|((i&0x10)>>1); // (the table's own order: entries 8..15 and 16..23 of every 32 change places)
            std::memcpy(out.data()+(std::size_t{y}*512+x)*4,clut+i*4,4);
        }
    return true;
}

// The plate of Online Mode: Dueling's (row 3 of the first sheet: its icon, the bar and its word), the word's place filled
// from the bar to its right, our word over it.
void buildPlate(std::uint8_t* rdram,std::uint32_t entry) {
    if(plate_ready.load() || plate_word.rgba.empty())return;
    std::vector<std::uint8_t> sheet;
    if(!decodeSheet(rdram,entry,sheet))return;
    std::vector<std::uint8_t> row(sheet.begin()+192*512*4,sheet.begin()+256*512*4);
    for(std::uint32_t y=0;y<64;++y)
        for(std::uint32_t x=44;x<146;++x)
            std::memcpy(row.data()+(std::size_t{y}*512+x)*4,row.data()+(std::size_t{y}*512+x+106)*4,4);
    for(std::size_t i=0;i<std::size_t{512}*64;++i) {
        const std::uint8_t* w=plate_word.rgba.data()+i*4;std::uint8_t* d=row.data()+i*4;
        const int sa=w[3],da=d[3],out_a=sa+da*(128-sa)/128;
        if(!sa || !out_a)continue;
        for(int c=0;c<3;++c)d[c]=std::uint8_t((w[c]*sa*128+d[c]*da*(128-sa))/(out_a*128));
        d[3]=std::uint8_t(out_a);
    }
    plate_row=std::move(row);
    plate_ready.store(true,std::memory_order_release);
    std::fprintf(stderr,"Main menu item: the plate of Online Mode made from Dueling's\n");
}
}

static std::uint32_t* pluginPixels(std::uint32_t slot,std::uint32_t* version,std::uint32_t* w,std::uint32_t* h) {
    if(slot==gohan_picture && clip.frames) { // the clip's frame of this moment
        const std::uint32_t frame=clip_frame.load(std::memory_order_relaxed);
        char name[32];std::snprintf(name,sizeof name,"/gohan_%02u.png",frame);
        const Picture p=loadPicture(clip.folder+name);
        if(p.rgba.empty())return nullptr;
        auto* px=static_cast<std::uint32_t*>(std::malloc(p.rgba.size()));
        if(!px)return nullptr;
        std::memcpy(px,p.rgba.data(),p.rgba.size());
        *version=1+frame;*w=p.w;*h=p.h;
        return px;
    }
    if(slot>=pictures || picture[slot].rgba.empty())return nullptr;
    auto* px=static_cast<std::uint32_t*>(std::malloc(picture[slot].rgba.size()));
    if(!px)return nullptr;
    std::memcpy(px,picture[slot].rgba.data(),picture[slot].rgba.size());
    *version=1;*w=picture[slot].w;*h=picture[slot].h;
    return px;
}
// Which page of the Online Mode screen is open (0 none), for the app's pieces over the game (android_main.cpp: the engine's counts).
static int pluginOnlineState() {return online_state.load(std::memory_order_relaxed);}
static std::uint32_t pluginVersion(std::uint32_t slot) {
    if(slot==gohan_picture && clip.frames)return 1+clip_frame.load(std::memory_order_relaxed);
    return slot<pictures && !picture[slot].rgba.empty()?1:0;
}

// At each flip, over the frame the game drew: the screen when it is open, else the item's description while the menu
// shows it. Positions in the frame's own pixels (the game's 640 x 448).
static void pluginOverlay(std::uint64_t dispfb,std::uint64_t display) {
    const std::uint32_t flip=flips++;
    const bool menu=menu_drawn==flip;
    // The pictures go into both buffers of the game's pair: the one it has just drawn (FRAME as it left it) and the
    // one DISPFB names (on the screen now): whichever of the two the renderer shows has them.
    const std::uint64_t frame=H->last_frame();
    const int fbp=int(frame&0x1FF),fbw=int(frame>>16&63),psm=int(frame>>24&63),shown=int(dispfb&0x1FF);
    const int magh=int(display>>23&15)+1,width=int((display>>32&0xFFF)+1)/magh,height=int((display>>44&0x7FF)+1);
    static bool logged=false;
    if(!logged && menu){logged=true;std::fprintf(stderr,"Main menu item: the frame shown is %d x %d (buffer %d, %d wide, format %d)\n",width,height,fbp,fbw*64,psm);}
    if(!menu)menu_active.store(false,std::memory_order_relaxed);
    const auto draw=[&](Slot slot,float x0,float y0,float x1,float y1,int alpha=128) {
        if(picture[slot].rgba.empty())return;
        H->overlay_sprite(fbp,fbw,psm,width,height,slot,x0,y0,x1,y1,alpha);
        if(shown!=fbp)H->overlay_sprite(shown,fbw,psm,width,height,slot,x0,y0,x1,y1,alpha);
    };
    if(screen.open) {
        draw(screen.wait?wait_picture:screen.page?Slot(create_picture+screen.choice):screen.choice?ranked_picture:online_picture,0,0,float(width),float(height),screen.fade);
        if(clip.frames) { // the fighter, where the still one stood (1280 x 720 screen: 30 from the right, 50 above the bottom)
            clip_frame.store(clipNow(),std::memory_order_relaxed);
            const float x0=(1280.0f-float(clip.w)-30.0f)/1280.0f*float(width),y0=(720.0f-float(clip.h)-50.0f)/720.0f*float(height);
            H->overlay_sprite(fbp,fbw,psm,width,height,gohan_picture,x0,y0,x0+float(clip.w)/1280.0f*float(width),y0+float(clip.h)/720.0f*float(height),screen.fade);
            if(shown!=fbp)H->overlay_sprite(shown,fbw,psm,width,height,gohan_picture,x0,y0,x0+float(clip.w)/1280.0f*float(width),y0+float(clip.h)/720.0f*float(height),screen.fade);
        }
        {   // the title floats: up and down a few pixels, slowly (it is drawn apart from the background, at (0, 20) of the 1280 x 720 screen)
            const float bob=std::sin(float(flip)*0.05f)*7.0f/720.0f*float(height);
            draw(title_picture,0,20.0f/720.0f*float(height)+bob,700.0f/1280.0f*float(width),140.0f/720.0f*float(height)+bob,screen.fade);
        }
        if(screen.soon) {
            // (a pixel of the frame is shown wider than tall with the widescreen patch: the picture keeps its shape)
            static const bool wide=[]{const char* p=std::getenv("BT2_GAME_PATCHES");return p && std::strstr(p,"widescreen");}();
            const float shape=(wide?16.0f/9.0f:4.0f/3.0f)*float(height)/float(width);
            const float w=float(width)*0.5f,h=w*shape*float(picture[soon_picture].h)/float(std::max(1u,picture[soon_picture].w));
            draw(soon_picture,(float(width)-w)/2,(float(height)-h)/2,(float(width)+w)/2,(float(height)+h)/2);
        }
    } else if(menu && menu_item==online_item) {
        draw(description_picture,37.4f,347,348.9f,391); // (where the window's two lines are: measured on the frame)
    }
    if(cover.load()>0) { // black over the frame from "Connected" until the walk reaches the character selection
        cover.fetch_sub(1);
        if(!nocover.load(std::memory_order_relaxed))draw(shade_picture,0,0,float(width),float(height),128);
    }
    if(screen.open && left_cover.load()>0 && left_cover.load()<=720)left_cover.store(0); // (the Online screen is back, and the message has been up three seconds: it goes)
    if(left_cover.load()>0) {left_cover.fetch_sub(1);draw(left_picture,0,0,float(width),float(height),128);} // (the opponent left: black, with the message)
}

static int pluginArtSwap(unsigned char* rgba,int w,int h) {
    // (only while the main menu is shown: the two sheets are its own, and the look at every other 512 x 256 picture
    // the renderer decodes, in a fight too, would cost for nothing)
    if(!art_ready.load(std::memory_order_acquire) || w!=512 || h!=256 || !menu_active.load(std::memory_order_relaxed))return 0;
    std::uint64_t hash=14695981039346656037ull;
    for(int i=0;i<512*128*4;++i)hash=(hash^rgba[i])*1099511628211ull;
    static int logged=0;
    const bool text=hash==names_sheet_hash,plate=hash==plates_sheet_hash;
    if(!text && !plate) {
        if(logged<24){++logged;std::fprintf(stderr,"Main menu item: a 512 x 256 sheet %016llx\n",static_cast<unsigned long long>(hash));}
        return 0;
    }
    if(plate && !plate_ready.load(std::memory_order_acquire))return 0; // (no plate of Dueling's to make it from yet)
    std::memcpy(rgba+512*128*4,(text?name_picture.rgba:plate_row).data(),512*64*4);
    if(plate)std::memset(rgba+512*192*4,0,512*64*4); // (the lines past the upload: whatever GS memory holds there)
    return 1;
}

// The sound the library adds to each field's: the opening's music (opening_music.cpp), at the master volume and the player's
// music volume (the engine's own streams were silenced while the screen is open).
static void pluginMix(std::int16_t* stereo,std::size_t samples,unsigned master_percent) {music.mix(stereo,samples,master_percent*music_volume/100);}

extern "C" __attribute__((visibility("default"))) const Bt2Plugin* bt2_plugin_init(const Bt2HostApi* host) {
    if(!host || host->api!=BT2_PLUGIN_API || host->size<sizeof(Bt2HostApi))return nullptr;
    H=host;
    const std::string folder=host->pictures_dir;
    name_picture=loadPicture(folder+"/item_text.png");
    plate_word=loadPicture(folder+"/plate_text.png");
    for(const std::string& dir:{std::string(H->online_dir?H->online_dir:"")+"/extra",folder}) { // the clip: its folder is where gohan.txt is
        std::ifstream info(dir+"/gohan.txt");
        std::uint32_t frames=0,fps=0,w=0,h=0;
        if(info && (info>>frames>>fps>>w>>h) && frames && fps && w && h) {clip={frames,fps,w,h,dir};break;}
    }
    for(std::size_t k=0;k<pictures;++k) {
        if(k==gohan_picture)continue;
        // (for tries, a picture in the folder "extra" of the user folder, which the app leaves alone, goes before the APK's)
        picture[k]=loadPicture(std::string(H->online_dir?H->online_dir:"")+"/extra/"+picture_names[k]+".png");
        if(picture[k].rgba.empty())picture[k]=loadPicture(folder+"/"+picture_names[k]+".png");
    }
    if(name_picture.w!=512 || name_picture.h!=64 || plate_word.w!=512 || plate_word.h!=64) {
        std::fprintf(stderr,"Main menu item: no pictures in %s, the menu stays the game's\n",folder.c_str());
        return nullptr;
    }
    art_ready.store(true,std::memory_order_release);
    H->hook(init_menu,initMenu,slot_init);
    H->hook(guides,guidesMenu,slot_guides);
    H->hook(moved_menu,movedMenu,slot_moved);
    H->hook(pad_menu,padMenu,slot_pad);
    H->hook(draw_menu,drawMenu,slot_draw);
    H->hook(menu_mode,menuMode,slot_menu_mode);
    H->hook(frame_pad,framePad,slot_frame_pad);
    H->hook(pad_connected,padConnected,slot_pad_connected);
    H->hook(brightness,brightnessHook,slot_brightness);
    H->hook(duel_pad,duelPad,slot_duel_pad);
    disc_image=host->disc_image;
    H->set_music_mix(pluginMix);
    std::fprintf(stderr,"Main menu item: Online Mode, the main menu's eleventh item\n");
    {   // the app started this engine for a match (the lobby's file): taken once, a file that is old is not a match
        const std::string path=onlinePath("online_session");
        std::ifstream in(path);
        std::stringstream text;
        text<<in.rdbuf();
        SessionFile file;
        if(in && parseSessionFile(text.str(),file)) {
            std::error_code error;
            std::filesystem::rename(path,path+".last",error);
            const bool fresh=std::time(nullptr)-file.at<300;
            pending_addresses=file.addresses; // (fightAvailable asks)
            if(fresh && fightAvailable())startFightProcess(file);
            else std::fprintf(stderr,"online: a match was asked for but %s\n",fresh?"this engine cannot run one":"the file is old");
        }
    }
    static const Bt2Plugin plugin{BT2_PLUGIN_API,sizeof(Bt2Plugin),pluginOverlay,pluginPixels,pluginVersion,pluginArtSwap,pluginOnlineState,pluginPadField};
    return &plugin;
}
