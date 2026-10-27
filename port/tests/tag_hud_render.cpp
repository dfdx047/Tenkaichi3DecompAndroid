/* Draw the production HUD into real ImGui command lists. These fixtures do
 * not drive gameplay: they check that labels are optional without removing
 * the meter, ring or camera controls underneath them. */
#include <cassert>
#include <map>
#include <string>
#include "../src/gs/tag_hud.cpp"

static bool connected;
static std::map<std::string,int> saved;
extern "C" {
volatile int gUiPresentX=0,gUiPresentY=0,gUiPresentW=1280,gUiPresentH=720;
int gPortOverlayOpen=0;
int Port_Setting(const char *key,int initial) {auto i=saved.find(key);return i==saved.end()?initial:i->second;}
void Port_SettingSave(const char *key,int v) {saved[key]=v;}
void Port_SettingsWrite() {}
const char *Port_TagText(const char *s) {return s;}
int TagNet_Active() {return connected;}
int TagNet_Option(int,int v) {return v;}
int TagNet_Observer() {return 0;}
int TagNet_NativeMenu() {return 0;}
}
const char *TagHud_Name(int) {return "Goku";}
SDL_GPUTexture *Ui_Texture(uint8_t *pixels,uint32_t,uint32_t) {free(pixels);return nullptr;}
void Ui_TextureFree(SDL_GPUTexture *) {}

static int draw() {
    ImGui::NewFrame();TagHud_Draw();ImGui::Render();
    return ImGui::GetDrawData()->TotalVtxCount;
}
static void reset() {
    for(int i=0;i<TAG_OPT_COUNT;i++)opts[i]=tagOptionInfo[i].initial;
    latest={};latest.active=latest.shown=latest.views=1;latest.count=2;
    for(auto &a:latest.actors) {
        a.fusedInto=a.fusionBody=a.fusionRequest=a.assistAlly=a.assistReady=-1;
        a.player=0;a.hp=a.hpMax=30000;a.assistPower=100;
    }
    auto &v=latest.view[0];v.visible=1;v.rect[2]=512;v.rect[3]=448;
    v.reviveBody=v.takeover=-1;v.controlActor=0;
    previewOption=-1;
}
int main() {
    ImGui::CreateContext();auto &io=ImGui::GetIO();
    io.IniFilename=nullptr;io.DisplaySize=ImVec2(1280,720);io.DeltaTime=1.f/30;
    unsigned char *pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    saved["tag_fusion_meter"]=saved["tag_spectator_hints"]=0;
    Port_TagOption(0);
    assert(!Port_TagOption(TAG_OPT_FUSION_TIME_TEXT) && !Port_TagOption(TAG_OPT_SPECTATOR_NAME));
    reset();assert(draw()==0);
    // Fusion text and meter work in all four combinations.
    auto &a=latest.actors[0];a.fusionBody=0;a.fusionSeconds=42;a.fusionDuration=80;
    opts[TAG_OPT_FUSION_OWNER]=opts[TAG_OPT_FUSION_COUNTDOWN]=0;
    int both=draw();opts[TAG_OPT_FUSION_TIME_TEXT]=0;int meter=draw();
    assert(meter>0 && both>meter);
    opts[TAG_OPT_FUSION_METER]=0;assert(draw()==0);
    opts[TAG_OPT_FUSION_TIME_TEXT]=1;assert(draw()>0);
    reset();latest.view[0].spectating=1;
    int full=draw();opts[TAG_OPT_SPECTATOR_NAME]=0;int hints=draw();
    assert(hints>0 && full>hints);opts[TAG_OPT_SPECTATOR_HINTS]=0;assert(draw()==0);
    opts[TAG_OPT_SPECTATOR_NAME]=1;assert(draw()>0);
    // Each remaining mod label has an independent draw switch.
    const int options[]={TAG_OPT_PLAYER_LABELS,TAG_OPT_ALLY_NAMES,
        TAG_OPT_REVIVE_PROGRESS_TEXT,TAG_OPT_REVIVE_COST_TEXT,
        TAG_OPT_FUSION_REQUEST_TEXT,TAG_OPT_ASSIST_POWER_TEXT,TAG_OPT_ASSIST_HINT_TEXT,
        TAG_OPT_PRACTICE_LABELS,TAG_OPT_FUSION_OWNER,TAG_OPT_FUSION_COUNTDOWN};
    for(int id:options) {
        reset();opts[id]=1;
        switch(id) {
        case TAG_OPT_ALLY_NAMES:opts[TAG_OPT_ALLY_PANEL]=1;break;
        case TAG_OPT_REVIVE_PROGRESS_TEXT:latest.view[0].reviveBody=1;latest.reviveDuration=90;latest.actors[1].reviveTicks=45;break;
        case TAG_OPT_REVIVE_COST_TEXT:latest.view[0].reviveBody=1;latest.reviveCost=2;break;
        case TAG_OPT_FUSION_REQUEST_TEXT:latest.actors[0].fusionRequest=1;break;
        case TAG_OPT_ASSIST_POWER_TEXT:latest.actors[0].assistAlly=1;latest.actors[0].assistPower=150;break;
        case TAG_OPT_ASSIST_HINT_TEXT:latest.actors[0].assistReady=1;break;
        case TAG_OPT_PRACTICE_LABELS:
            strcpy(latest.actors[1].practiceLabel,"Training Dummy");
            latest.view[0].heads[1]={256,224,1,1};break;
        case TAG_OPT_FUSION_OWNER:latest.actors[0].fusionBody=0;break;
        case TAG_OPT_FUSION_COUNTDOWN:
            latest.actors[0].fusionBody=0;latest.actors[0].fusionNext=1;
            latest.actors[0].fusionSwitchSeconds=5;break;
        }
        int on=draw();opts[id]=0;int off=draw();assert(on>off);
        if(id==TAG_OPT_ALLY_NAMES || id==TAG_OPT_REVIVE_PROGRESS_TEXT)assert(off>0);
        opts[id]=1;opts[TAG_OPT_HIDE_ALL_HUD]=1;assert(draw()==0);
    }
    // Master hiding must not make the settings previews disappear.
    reset();opts[TAG_OPT_HIDE_ALL_HUD]=1;
    Port_TagPreview(TAG_OPT_FUSION_METER);assert(draw()>0);
    // During online matches only presentation settings can change locally.
    connected=true;
    Port_TagOptionSet(TAG_OPT_SPECTATOR_NAME,0);
    assert(Port_TagOption(TAG_OPT_SPECTATOR_NAME)==0 && saved["tag_spectator_name"]==0);
    Port_TagOptionSet(TAG_OPT_FUSIONS,0);assert(Port_TagOption(TAG_OPT_FUSIONS)==1);
    ImGui::DestroyContext();puts("PASS: independent HUD text, meters, master hiding, previews and local online display settings");
}
