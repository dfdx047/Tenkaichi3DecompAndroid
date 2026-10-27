#include "imgui.h"
#include "tag_net.h"
#include "../../include/plat_game.h"
#include "../../include/tag_text.h"
#include <algorithm>
#include <cstdio>
#include "menu_translation.h"
extern const char *TagHud_Name(int);

static const char *name(int c) {
    const char *n=TagHud_Name(c);return n&&*n?n:"Fighter";
}
static const char *stage_name(int i) {
    static const char *bt4[]={
        "Wasteland - Evening", "Rocky Area - Noon", "Planet Namek", "Tournament of Power - Final",
        "World Tournament - Noon", "Kami's Lookout - Noon", "Cell Game Arena - Ruins", "Supreme Kai's World",
        "Hyperbolic Time Chamber", "City Ruins - Noon", "Mountain Road - Noon", "Archipelago - Evening",
        "Kame House - Noon", "Glacier", "Ice Continent", "Dying Planet", "Outer Space - Earth",
        "Tournament of Power - Climax", "Hell", "Desert - Noon", "King's Castle", "Penguin Village - Noon",
        "Mount Paozu", "Plains - Noon", "South City", "Baba's Palace - Night", "Rocky Area - Night",
        "World Tournament - Evening", "Cell Games Arena - Noon", "City Ruins - Evening", "City Ruins - Future",
        "Tournament of Power", "Champa's Ring", "Islands", "Planet - Evening"
    };
    if(PortGame_IsBT4() && i<35)return bt4[i];
    const auto names=MenuTranslation_StageNames();
    if(!PortGame_IsBT4() && names && i<35)return names[i];
    static char fallback[64];std::snprintf(fallback,sizeof(fallback),Port_TagText("Additional stage %d"),i);return fallback;
}
#include <SDL3/SDL.h>
#include "ui.h"
#include <cstring>
#include <cstdlib>
static SDL_SpinLock pointerLock;
static int pointerState[3]={-1,-1,0};
extern "C" void TagNet_MenuPointer(unsigned output) {
    SDL_LockSpinlock(&pointerLock);std::memcpy((void *)(uintptr_t)output,pointerState,sizeof(pointerState));pointerState[2]=0;SDL_UnlockSpinlock(&pointerLock);
}
extern "C" void TagNet_MenuName(int kind,int id,unsigned output,int size) {
    const char *n=kind==0?name(id):kind==1?stage_name(id):nullptr;
    static const char *bt4Songs[]={"Bu Yuu Retsuden","Dragon Crisis","Victory Road","Little Light","Super Sonic","Explosion (RB1)","Final Confrontation","Dark Half","Denouement","Stand After Confusion","The Warrior","Tense Atmosphere","Challenger","Gallant","Sky Battle for Justice","Move Forward Fearlessly","Dynamite Battle","Burnin' Up","Wild Rush","Evolution","Force Of Justice","Challengers","Joker","Evil Desires"};
    if(kind==2){auto songs=PortGame_IsBT4()?bt4Songs:MenuTranslation_SongNames();n=id==24?"Random":songs&&id>=0&&id<24?songs[id]:"Music";}
    std::snprintf((char *)(uintptr_t)output,size,"%s",Port_TagText(n?n:"Unknown"));
}
extern "C" void TagNet_Draw(void) {
    if(!TagNet_Enabled())return;
    if(TagNet_NativeMenu()) {
        auto &io=ImGui::GetIO();
        if(!gPortOverlayOpen && !std::getenv("BT3_PAD") && gUiPresentW>0&&gUiPresentH>0) {
            SDL_LockSpinlock(&pointerLock);
            pointerState[0]=int((io.MousePos.x-gUiPresentX)*512/gUiPresentW);
            pointerState[1]=int((io.MousePos.y-gUiPresentY)*448/gUiPresentH);
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))pointerState[2]=1;
            SDL_UnlockSpinlock(&pointerLock);
        }
        return;
    }
    TagNetStatus s;TagNet_Status(&s);ImGuiIO &io=ImGui::GetIO();
    if(s.phase==4) {
        if(TagNet_Observer()) {
            // Camera bindings remain usable with every spectator label hidden.
            int view=s.view,n=s.config.size[0]+s.config.size[1];
            bool free=s.freeCam!=0;
            if(n<1)return;
            if(!io.WantTextInput && !gPortOverlayOpen) {
                if(ImGui::IsKeyPressed(ImGuiKey_LeftBracket))view=(view+n-1)%n;
                if(ImGui::IsKeyPressed(ImGuiKey_RightBracket))view=(view+1)%n;
                if(ImGui::IsKeyPressed(ImGuiKey_F))free=!free;
            }
            if(view!=s.view || free!=(s.freeCam!=0))TagNet_Watch(view,free);
            bool hints=Port_TagOption(TAG_OPT_SPECTATOR_HINTS)!=0;
            bool names=Port_TagOption(TAG_OPT_SPECTATOR_NAME)!=0;
            if(Port_TagOption(TAG_OPT_HIDE_ALL_HUD) || (!hints && !names))return;
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x-12,io.DisplaySize.y-12),ImGuiCond_Always,ImVec2(1,1));
            ImGui::SetNextWindowSize(ImVec2(290,0),ImGuiCond_FirstUseEver);
            if(ImGui::Begin("##Spectator",nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoTitleBar)) {
                if(names)ImGui::TextWrapped(Port_TagText("Watching %s"),name(s.config.chara[view]));
                if(hints) {
                    ImGui::TextUnformatted(Port_TagText("L2/R2: switch view | Select: free camera"));
                    bool change=false;
                    if(ImGui::Button(Port_TagText("Previous ["))){view=(view+n-1)%n;change=true;}
                    ImGui::SameLine();if(ImGui::Button(Port_TagText("Next ]"))){view=(view+1)%n;change=true;}
                    change=ImGui::Checkbox(Port_TagText("Free camera (F)"),&free)||change;
                    if(change)TagNet_Watch(view,free);
                    if(free)ImGui::TextWrapped("%s",Port_TagText("Left stick: move. Right stick: look. L1/R1: height. Keyboard uses your game bindings (F1 to change)."));
                    if(ImGui::Button(Port_TagText("Leave match")))TagNet_Leave();
                }
            }ImGui::End();
        }
        return;
    }
}
