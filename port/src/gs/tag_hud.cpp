// Native multi-fighter HUD. Render-thread drawing consumes a game-thread copy.
#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <mutex>
#include "imgui.h"
#include "ui.h"
#include "../../../include/battle/tag_hud_data.h"
#include "../../../include/battle/tag_options.h"
#include "tag_hud_marker.h"
#include "tag_net.h"
#include "../../include/tag_text.h"

extern "C" void Port_SettingSave(const char *, int);
extern "C" void Port_SettingsWrite(void);
const char *TagHud_Name(int);
static SDL_SpinLock lock;
static TagHudState latest;
static std::atomic<int> opts[TAG_OPT_COUNT];
static std::once_flag loaded;
static std::atomic<int> previewOption{-1};
static std::atomic<Uint64> previewTime{0};
extern "C" void Port_TagPreview(int option) {
    previewOption=option;previewTime=SDL_GetTicks();
}
static void preview(ImDrawList *,int,ImVec2,ImVec2);
static bool has_preview(int option);
struct OnlinePreview {float rect[4];int id,values[TAG_OPT_COUNT];};
static OnlinePreview onlinePreview;
static Uint64 onlinePreviewTime;
static thread_local const int *previewValues;
extern "C" int Port_TagOnlinePreview(unsigned address) {
    auto &p=*(const OnlinePreview *)(uintptr_t)address;
    if(!has_preview(p.id))return 0;
    SDL_LockSpinlock(&lock);onlinePreview=p;onlinePreviewTime=SDL_GetTicks();SDL_UnlockSpinlock(&lock);
    return 1;
}
static SDL_SpinLock artLock;
static unsigned char artPixels[6][256*64*4];
static bool artPending;
static SDL_GPUTexture *artTextures[6];
extern SDL_GPUTexture *Ui_Texture(uint8_t *,uint32_t,uint32_t);
extern void Ui_TextureFree(SDL_GPUTexture *);
/* Copy the player's resident HUD atlas once at battle load. No renderer ever
   dereferences a live game resource; texture creation stays on its own thread. */
extern "C" void Port_TagHudArt(unsigned address) {
    const auto *res=(const unsigned char *)(uintptr_t)address;
    auto word=[](const unsigned char *p){uint32_t v;memcpy(&v,p,4);return v;};
    if(word(res)<14)return;
    const auto *table=(const unsigned char *)(uintptr_t)word(res+16);
    const auto *e=table+8*64;
    uint64_t tex;memcpy(&tex,e+48,8);
    if(((tex>>20)&63)!=19 || ((tex>>26)&15)!=8 || ((tex>>30)&15)!=6 ||
        word(e+8)<256*64+96 || word(e+12)<1024+96)return;
    const auto *pixels=(const unsigned char *)(uintptr_t)word(e+56)+96;
    for(int k=0;k<6;k++) if(word(table+(8+k)*64+12)<1120)return;
    SDL_LockSpinlock(&artLock);
    for(int k=0;k<6;k++) {
        const auto *pal=(const unsigned char *)(uintptr_t)word(table+(8+k)*64+60)+96;
        for(int y=0;y<64;y++) for(int x=0;x<256;x++) {
            int block=(y&~15)*256+(x&~15)*2,swap=(((y+2)>>2)&1)*4;
            int line=(((y&~3)>>1)+(y&1))&7;
            int index=pixels[block+line*256*2+((x+swap)&7)*4+((y>>1)&1)+((x>>2)&2)];
            index=(index&~24)|((index&8)<<1)|((index&16)>>1);
            auto *out=artPixels[k]+4*(y*256+x);
            memcpy(out,pal+index*4,4);out[3]=std::min(255,int(out[3])*2);
        }
    }
    artPending=true;SDL_UnlockSpinlock(&artLock);
}
static void load_art() {
    SDL_LockSpinlock(&artLock);
    if(artPending) {
        for(int k=0;k<6;k++) {
            auto *p=(uint8_t *)malloc(sizeof(artPixels[k]));
            if(!p)continue;
            memcpy(p,artPixels[k],sizeof(artPixels[k]));
            Ui_TextureFree(artTextures[k]);artTextures[k]=Ui_Texture(p,256,64);
        }
        artPending=false;
    }
    SDL_UnlockSpinlock(&artLock);
}
extern "C" int Port_TagOption(int i) {
    if(i<0 || i>=TAG_OPT_COUNT) return 0;
    if(previewValues)return previewValues[i];
    std::call_once(loaded,[] {
        for(int k=0;k<TAG_OPT_COUNT;k++) {
            const auto &o=tagOptionInfo[k];
            int initial=o.initial;
            // Preserve an older choice to hide both parts until the new text
            // setting has been saved separately.
            if(k==TAG_OPT_FUSION_TIME_TEXT)initial=Port_Setting("tag_fusion_meter",initial);
            if(k==TAG_OPT_SPECTATOR_NAME)initial=Port_Setting("tag_spectator_hints",initial);
            opts[k]=std::clamp(Port_Setting(o.key,initial),o.min,o.max);
        }
    });
    return TagNet_Option(i,opts[i].load());
}
extern "C" void Port_TagOptionSet(int i,int value) {
    if(i<0 || i>=TAG_OPT_COUNT) return;
    if(TagNet_Active() && !TagOption_IsPresentation(i)) return; // Agreed match rules stay fixed.
    Port_TagOption(i);
    const auto &o=tagOptionInfo[i];
    opts[i]=std::clamp(value,o.min,o.max);
    /* The settings store has its own mutex. Save here because the renderer
       need not draw an ImGui HUD while the native menu is open. */
    Port_SettingSave(o.key,opts[i].load());
    Port_SettingsWrite();
}
extern "C" void Port_TagHudSubmit(unsigned address) {
    SDL_LockSpinlock(&lock);
    latest=*(const TagHudState *)(uintptr_t)address;
    SDL_UnlockSpinlock(&lock);
}
extern "C" void Port_TagRevived(int actor,int rescuer,int hp) {
    fprintf(stderr,"tagteam: revived actor=%d helper=%d hp=%d\n",actor,rescuer,hp);
}
extern "C" void Port_TagHudClear() {
    SDL_LockSpinlock(&lock); latest.active=0; SDL_UnlockSpinlock(&lock);
}
bool TagHud_Visible() {
    SDL_LockSpinlock(&lock); bool active=latest.active!=0; SDL_UnlockSpinlock(&lock);
    int option=previewOption.load();
    return active || (option>=0 && has_preview(option) && SDL_GetTicks()-previewTime.load()<150);
}
static bool tag_combo(const char *label,int *value,const char *items) {
    const char *translated[8];int count=0;
    while(*items && count<8){translated[count++]=Port_TagText(items);items+=strlen(items)+1;}
    return ImGui::Combo(label,value,translated,count);
}
void TagHud_Settings() {
    if (!getenv("BT3_TAG_TEAM")) return;
    ImGui::SeparatorText(Port_TagText("Mod Settings"));
    for(int group=0;group<TAG_OPTION_GROUP_COUNT;group++) {
        if(!ImGui::TreeNode(Port_TagText(tagOptionGroups[group]))) continue;
        for(int i=0;i<TAG_OPT_COUNT;i++) {
            auto o=tagOptionInfo[i]; if(o.group!=group) continue;
            o.label=Port_TagText(o.label);o.help=Port_TagText(o.help);
            ImGui::BeginDisabled(TagNet_Active() && !TagOption_IsPresentation(i));
            int value=Port_TagOption(i); bool changed=false;
            if(i==TAG_OPT_THREE_WIDE) changed=tag_combo(o.label,&value,"Centered bottom view\0Full width bottom view\0");
            else if(i==TAG_OPT_CINEMATIC_PAUSE) changed=tag_combo(o.label,&value,"Keep moving\0Pause target\0Pause other fighters\0");
            else if(i==TAG_OPT_OUT_SCOPE) changed=tag_combo(o.label,&value,"Smaller team\0Smaller team or 2+ attackers\0");
            else if(i==TAG_OPT_OUT_PROTECTION) {
                float seconds=value/10.0f;
                changed=ImGui::SliderFloat(o.label,&seconds,0,3,Port_TagText("%.1f seconds"));
                value=(int)std::round(seconds*10);
            }
            else if(o.max==1) { bool on=value!=0; changed=ImGui::Checkbox(o.label,&on); value=on; }
            else if(i==TAG_OPT_UNLOCKED_HUD) changed=tag_combo(o.label,&value,"Show\0Only with one enemy\0Hide\0");
            else if(i==TAG_OPT_TARGET_STYLE) changed=tag_combo(o.label,&value,"Arrow\0Ring\0Corner reticle\0Arrow and ring\0");
            else if(i==TAG_OPT_TARGET_STICK) changed=tag_combo(o.label,&value,"Off\0Hold L3 and flick\0Flick alone\0");
            else if(i==TAG_OPT_OUTNUMBERED) changed=tag_combo(o.label,&value,"Off\0Balanced\0Strong\0Custom\0");
            else changed=ImGui::SliderInt(o.label,&value,o.min,o.max);
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",o.help);
            if(changed) Port_TagOptionSet(i,value);
            ImGui::EndDisabled();
            if(has_preview(i)) {
                float width=std::min(340.0f,ImGui::GetContentRegionAvail().x);
                ImVec2 p=ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton(("preview##"+std::to_string(i)).c_str(),ImVec2(width,width*0.55f));
                preview(ImGui::GetWindowDrawList(),i,p,ImVec2(p.x+width,p.y+width*0.55f));
            }
        }
        ImGui::TreePop();
    }
    ImGui::TextWrapped("%s",Port_TagText("Tap L3 to target an attacker; otherwise select an enemy near the centre of your view. Hold for half a second to unlock. Right-stick targeting follows the direction on your screen. F1 > Controls edits each player's bindings; Controller Setup assigns devices to players."));
}
extern "C" void Port_TagOptionFormat(int id,int v,unsigned output,int size) {
    if(id<0||id>=TAG_OPT_COUNT||size<1)return;
    char *p=(char *)(uintptr_t)output;
    const char *label=nullptr;
    switch(id) {
    case TAG_OPT_TARGET_STICK:{static const char *s[]={"Off","Hold L3 and flick","Flick the right stick"};label=s[v];break;}
    case TAG_OPT_TARGET_STYLE:{static const char *s[]={"Arrow","Ring","Corner reticle","Arrow and ring"};label=s[v];break;}
    case TAG_OPT_OUTNUMBERED:{static const char *s[]={"Off","Balanced","Strong","Custom"};label=s[v];break;}
    case TAG_OPT_OUT_SCOPE:label=v?"Smaller team or 2+ attackers":"Smaller team";break;
    case TAG_OPT_OUT_PROTECTION:snprintf(p,size,Port_TagText("%d.%d seconds (Custom)"),v/10,v%10);return;
    case TAG_OPT_OUT_BREAKER:snprintf(p,size,Port_TagText(v?"%d hits (Custom)":"Off (Custom)"),v);return;
    case TAG_OPT_CINEMATIC_PAUSE:label=v==2?"Pause other fighters":v==1?"Pause only the target":"Keep other fighters moving";break;
    case TAG_OPT_THREE_WIDE:label=v?"Full width bottom view":"Centered bottom view";break;
    case TAG_OPT_UNLOCKED_HUD:label=v==0?"Always shown":v==1?"Only with one enemy":"Hidden";break;
    case TAG_OPT_REVIVE_SECONDS:case TAG_OPT_FUSION_SECONDS:case TAG_OPT_FUSION_SWAP_SECONDS:
        snprintf(p,size,Port_TagText("%d seconds"),v);return;
    case TAG_OPT_REVIVE_RADIUS:case TAG_OPT_BEAM_ASSIST_RANGE:case TAG_OPT_REVIVE_WAVE_HEIGHT:snprintf(p,size,Port_TagText("%d units"),v);return;
    case TAG_OPT_REVIVE_STOCKS:snprintf(p,size,Port_TagText("%d blast stocks"),v);return;
    case TAG_OPT_GIANT_ARMOR:snprintf(p,size,"%d",v);return;
    default:
        if(tagOptionInfo[id].max>2){snprintf(p,size,Port_TagText("%d percent"),v);return;}
        if(tagOptionInfo[id].group==2 && id!=TAG_OPT_CINEMATIC_PAUSE && id!=TAG_OPT_LOCAL_CINEMATICS)label=v?"Shared camera":"Keep player views";
        else label=v?"On":"Off";
    }
    snprintf(p,size,"%s",Port_TagText(label));
}
extern "C" void Port_TagOptionValue(int id,unsigned output,int size) { Port_TagOptionFormat(id,Port_TagOption(id),output,size); }
static ImU32 team(int t) {return t?IM_COL32(255,115,95,255):IM_COL32(75,195,255,255);}
static float clamp(float v){return std::clamp(v,0.0f,1.0f);}
static void text(ImDrawList *d,float x,float y,float size,ImU32 color,const char *s,float width=10000) {
    s=Port_TagText(s);
    ImFont *font=ImGui::GetFont();
    size*=2.0f;
    float w=font->CalcTextSizeA(size,10000,0,s).x;
    if(w>width) size*=width/w;
    d->AddText(font,size,ImVec2(x+1,y+1),IM_COL32(0,0,0,240),s);
    d->AddText(font,size,ImVec2(x,y),color,s);
}
static void bar(ImDrawList *d,float x,float y,float w,float h,float fill,ImU32 color,int palette=-1) {
    if(Port_TagOption(TAG_OPT_STYLED_HUD)) {
        h=std::max(h,w*0.055f);
        float shear=h*0.5f;
        if(artTextures[0]) {
            auto image=[&](int palette,float left,float top,float width,float height,float u0,float v0,float u1,float v1,ImU32 tint) {
                d->AddImageQuad(ImTextureRef((ImTextureID)(intptr_t)artTextures[palette]),
                    ImVec2(left+shear,top),ImVec2(left+width+shear,top),ImVec2(left+width,top+height),ImVec2(left,top+height),
                    ImVec2(u0/256,v0/64),ImVec2(u1/256,v0/64),ImVec2(u1/256,v1/64),ImVec2(u0/256,v1/64),tint);
            };
            image(0,x-1,y-1,w+2,h+2,64,48,160,60,IM_COL32(255,255,255,220));
            image(0,x,y,w,h,66,0,156,12,IM_COL32_WHITE);
            if(palette<0)palette=((color>>16)&255)>190&&(color&255)<128?5:
                (color&255)>190&&((color>>8)&255)>190?4:3;
            if(fill>0)image(palette,x,y,w*clamp(fill),h,66,0,66+90*clamp(fill),12,IM_COL32_WHITE);
        } else {
            // Matching slanted silhouette before the first battle loads its art.
            d->AddQuadFilled(ImVec2(x+shear-2,y-2),ImVec2(x+w+shear+2,y-2),ImVec2(x+w+2,y+h+2),ImVec2(x-2,y+h+2),IM_COL32(5,8,12,240));
            d->AddQuadFilled(ImVec2(x+shear,y),ImVec2(x+w+shear,y),ImVec2(x+w,y+h),ImVec2(x,y+h),IM_COL32(40,50,60,255));
            if(fill>0)d->AddQuadFilled(ImVec2(x+shear,y),ImVec2(x+w*clamp(fill)+shear,y),ImVec2(x+w*clamp(fill),y+h),ImVec2(x,y+h),color);
            d->AddLine(ImVec2(x+shear,y),ImVec2(x+w*clamp(fill)+shear,y),IM_COL32(235,250,255,220),std::max(1.0f,h*0.18f));
        }
        return;
    }
    d->AddRectFilled(ImVec2(x-1,y-1),ImVec2(x+w+1,y+h+1),IM_COL32(4,10,18,235),2);
    d->AddRectFilled(ImVec2(x,y),ImVec2(x+w,y+h),IM_COL32(35,45,58,240),1);
    if(fill>0) d->AddRectFilled(ImVec2(x,y),ImVec2(x+w*clamp(fill),y+h),color,1);
}
static void marker(ImDrawList *d,ImVec2 p,float sc,ImU32 color) {
    if(!Port_TagOption(TAG_OPT_STYLED_HUD)) {
        d->AddTriangleFilled(ImVec2(p.x-4*sc,p.y-3*sc),ImVec2(p.x+4*sc,p.y-3*sc),ImVec2(p.x,p.y+3*sc),color);return;
    }
    for(unsigned n=0;n<sizeof(markerVertices)/sizeof(*markerVertices);n+=3) {
        d->PrimReserve(3,3);unsigned start=d->_VtxCurrentIdx;
        for(unsigned k=0;k<3;k++) {
            auto &v=markerVertices[n+k];
            d->PrimWriteIdx(start+k);d->PrimWriteVtx(ImVec2(p.x+v.x*sc*0.75f,p.y+v.y*sc*0.75f),ImGui::GetFontTexUvWhitePixel(),v.color);
        }
    }
}
static void corners(ImDrawList *d,ImVec2 p,float sc,ImU32 color) {
    for(int dx:{-1,1}) for(int dy:{-1,1}) {
        ImVec2 c(p.x+dx*7*sc,p.y+dy*7*sc);
        for(float thickness:{4*sc,2*sc}) {
            ImU32 ink=thickness>3*sc?IM_COL32(0,0,0,230):color;
            d->AddLine(c,ImVec2(c.x-dx*4*sc,c.y),ink,thickness);
            d->AddLine(c,ImVec2(c.x,c.y-dy*4*sc),ink,thickness);
        }
    }
}
/* Same radii, colour, layered glow and three traveling crests as the PCSX2
 * revival ring. Tessellate at native output resolution, without a texture. */
template<class Project> static void revival_ring(ImDrawList *d,float phase,Project project) {
    if(!Port_TagOption(TAG_OPT_REVIVE_RING))return;
    const float bands[5][3]={{.93f,1.07f,6},{.965f,1.035f,3},{.988f,1.012f,1},{0,1,2},{.92f,1,1}};
    const float height=Port_TagOption(TAG_OPT_REVIVE_WAVE_HEIGHT);
    const int opacity=255*Port_TagOption(TAG_OPT_REVIVE_OPACITY)/100;
    const auto flags=d->Flags;
    // Shared edges must not produce dark antialiasing seams between quads.
    d->Flags&=~ImDrawListFlags_AntiAliasedFill;
    for(int layer=0;layer<(height>0?5:3);layer++) {
        int alpha=int(opacity/bands[layer][2]);if(!alpha)continue;
        ImVec2 previous[2];bool previousValid=false;
        for(int k=0;k<=64;k++) {
            float angle=(k%64)*(6.283185307f/64);
            float cs=cosf(angle),sn=sinf(angle);
            float wave=height*(.6f+.4f*sinf(angle*3+phase));
            ImVec2 now[2]{};bool valid=true;
            for(int edge=0;edge<2;edge++) {
                float radius=layer<3?bands[layer][edge]:1;
                float rise=layer<3?0:wave*bands[layer][edge];
                valid=project(cs*radius,sn*radius,rise,now[edge])&&valid;
            }
            if(k && valid && previousValid)
                d->AddQuadFilled(previous[0],previous[1],now[1],now[0],IM_COL32(112,255,160,alpha));
            previous[0]=now[0];previous[1]=now[1];previousValid=valid;
        }
    }
    d->Flags=flags;
}
static bool has_preview(int i) {
    return i==TAG_OPT_STYLED_HUD || i==TAG_OPT_TARGET_STYLE || i==TAG_OPT_TARGET_MARKER ||
        i==TAG_OPT_ENEMY_OVERHEAD || i==TAG_OPT_ALLY_OVERHEAD || i==TAG_OPT_ALLY_PANEL ||
        i==TAG_OPT_ATTACK_WARNING || i==TAG_OPT_FUSION_METER || i==TAG_OPT_FUSION_TIME_TEXT || i==TAG_OPT_FUSION_COUNTDOWN ||
        i==TAG_OPT_FUSION_OWNER || i==TAG_OPT_REVIVE_RING || i==TAG_OPT_REVIVE_OPACITY ||
        i==TAG_OPT_REVIVE_WAVE_HEIGHT || i==TAG_OPT_REVIVE_WAVE_SPEED ||
        i==TAG_OPT_REVIVE || i==TAG_OPT_REVIVE_SECONDS || i==TAG_OPT_REVIVE_STOCKS || i==TAG_OPT_THREE_WIDE;
}
static void preview(ImDrawList *d,int id,ImVec2 p,ImVec2 end) {
    float w=end.x-p.x,h=end.y-p.y,sc=w/190;
    d->PushClipRect(p,end,true);
    d->AddRectFilled(p,end,IM_COL32(18,27,36,245),6*sc);
    d->AddRect(p,end,IM_COL32(203,170,91,220),6*sc,0,sc);
    text(d,p.x+8*sc,p.y+6*sc,5*sc,IM_COL32(255,225,160,255),"EXAMPLE",w-16*sc);
    ImVec2 c(p.x+w/2,p.y+h*0.60f);
    if(id==TAG_OPT_THREE_WIDE) {
        float x=p.x+10*sc,y=p.y+25*sc,r=w-20*sc,b=h-35*sc;
        for(int k=0;k<3;k++) {
            float ww=(k==2&&Port_TagOption(id))?r:r/2-2*sc;
            float xx=k<2?x+k*(r/2+sc):x+(r-ww)/2;
            float yy=y+(k==2?b/2:0);
            d->AddRectFilled(ImVec2(xx,yy),ImVec2(xx+ww,yy+b/2-2*sc),team(k==1),3*sc);
        }
    } else if(id==TAG_OPT_REVIVE_RING || id==TAG_OPT_REVIVE_OPACITY ||
              id==TAG_OPT_REVIVE_WAVE_HEIGHT || id==TAG_OPT_REVIVE_WAVE_SPEED) {
        if(Port_TagOption(TAG_OPT_REVIVE_RING)) {
            float phase=float(SDL_GetTicks()%100000)*.000062831853f*Port_TagOption(TAG_OPT_REVIVE_WAVE_SPEED);
            revival_ring(d,phase,[&](float x,float z,float rise,ImVec2 &out) {
                out=ImVec2(c.x+x*58*sc,c.y+z*18*sc-rise*.6f*sc);return true;
            });
        } else text(d,c.x-20*sc,c.y,7*sc,IM_COL32_WHITE,"Hidden");
    } else if(id==TAG_OPT_REVIVE || id==TAG_OPT_REVIVE_SECONDS || id==TAG_OPT_REVIVE_STOCKS) {
        if(Port_TagOption(TAG_OPT_REVIVE)) {
            char label[64];
            snprintf(label,sizeof(label),Port_TagText("Reviving - %ds / %d stocks"),
                     Port_TagOption(TAG_OPT_REVIVE_SECONDS),Port_TagOption(TAG_OPT_REVIVE_STOCKS));
            text(d,p.x+10*sc,p.y+27*sc,6*sc,IM_COL32(110,255,210,255),label,w-20*sc);
            bar(d,p.x+15*sc,c.y,w-36*sc,7*sc,0.65f,IM_COL32(100,255,200,255),1);
        } else text(d,c.x-20*sc,c.y,7*sc,IM_COL32_WHITE,"Disabled");
    } else if(id==TAG_OPT_TARGET_STYLE || id==TAG_OPT_TARGET_MARKER) {
        if(Port_TagOption(TAG_OPT_TARGET_MARKER)) {
            int style=Port_TagOption(TAG_OPT_TARGET_STYLE);
            if(style==0||style==3)marker(d,ImVec2(c.x,c.y-10*sc),sc*1.4f,team(1));
            if(style==1||style==3){d->AddCircle(c,16*sc,IM_COL32(20,15,0,255),32,4*sc);d->AddCircle(c,16*sc,IM_COL32(255,221,45,255),32,2*sc);}
            if(style==2)corners(d,c,sc*2,team(1));
        } else text(d,c.x-20*sc,c.y,7*sc,IM_COL32_WHITE,"Hidden");
    } else if(id==TAG_OPT_ATTACK_WARNING) {
        if(Port_TagOption(id)) {
            ImVec2 a(c.x,c.y-21*sc),b(c.x-6*sc,c.y-9*sc),e(c.x+6*sc,c.y-9*sc);
            d->AddTriangle(a,b,e,IM_COL32(0,0,0,255),3*sc);
            d->AddTriangleFilled(a,b,e,IM_COL32(245,55,40,255));
            text(d,c.x-45*sc,c.y+8*sc,6*sc,IM_COL32(255,85,60,255),"ENEMY ATTACKING");
        }
        else text(d,c.x-20*sc,c.y,7*sc,IM_COL32_WHITE,"Hidden");
    } else {
        bool on=id==TAG_OPT_STYLED_HUD || Port_TagOption(id);
        bool fusion=id==TAG_OPT_FUSION_METER||id==TAG_OPT_FUSION_TIME_TEXT||id==TAG_OPT_FUSION_OWNER||id==TAG_OPT_FUSION_COUNTDOWN;
        if(id==TAG_OPT_FUSION_METER || id==TAG_OPT_FUSION_TIME_TEXT) {
            if(Port_TagOption(TAG_OPT_FUSION_TIME_TEXT))text(d,p.x+10*sc,p.y+27*sc,6*sc,
                IM_COL32(255,225,160,255),"Fusion - 42s remaining",w-20*sc);
            if(Port_TagOption(TAG_OPT_FUSION_METER))bar(d,p.x+15*sc,c.y,w-36*sc,7*sc,0.65f,IM_COL32(255,220,120,255));
            if(!Port_TagOption(TAG_OPT_FUSION_TIME_TEXT) && !Port_TagOption(TAG_OPT_FUSION_METER))
                text(d,c.x-20*sc,c.y,7*sc,IM_COL32_WHITE,"Hidden");
            d->PopClipRect();return;
        }
        if(on) {
            text(d,p.x+10*sc,p.y+27*sc,6*sc,IM_COL32(255,225,160,255),
                fusion?(id==TAG_OPT_FUSION_OWNER?"Fused - P1 has control":id==TAG_OPT_FUSION_COUNTDOWN?"P2 takes control in 5s":"Fusion - 42s remaining"):"Teammate health",w-20*sc);
            if(!fusion || id==TAG_OPT_FUSION_METER)bar(d,p.x+15*sc,c.y,w-36*sc,7*sc,0.65f,fusion?IM_COL32(255,220,120,255):team(0));
        } else text(d,c.x-20*sc,c.y,7*sc,IM_COL32_WHITE,"Hidden");
    }
    d->PopClipRect();
}
void TagHud_Draw() {
    load_art();
    OnlinePreview op;Uint64 at;
    SDL_LockSpinlock(&lock);op=onlinePreview;at=onlinePreviewTime;SDL_UnlockSpinlock(&lock);
    if(TagNet_NativeMenu() && at && SDL_GetTicks()-at<100 && !gPortOverlayOpen && gUiPresentW>0) {
        float sx=gUiPresentW/512.0f,sy=gUiPresentH/448.0f;
        previewValues=op.values;
        preview(ImGui::GetBackgroundDrawList(),op.id,
            ImVec2(gUiPresentX+op.rect[0]*sx,gUiPresentY+op.rect[1]*sy),
            ImVec2(gUiPresentX+op.rect[2]*sx,gUiPresentY+op.rect[3]*sy));
        previewValues=nullptr;
    }
    int opt=previewOption.load();
    if(opt>=0 && SDL_GetTicks()-previewTime.load()<150 && has_preview(opt) && gUiPresentW>0) {
        float sx=gUiPresentW/512.0f,sy=gUiPresentH/448.0f;
        preview(ImGui::GetBackgroundDrawList(),opt,
            ImVec2(gUiPresentX+304*sx,gUiPresentY+191*sy),ImVec2(gUiPresentX+494*sx,gUiPresentY+320*sy));
    }
    TagHudState s;
    SDL_LockSpinlock(&lock); s=latest; SDL_UnlockSpinlock(&lock);
    if(!s.active||gUiPresentW<=0||gUiPresentH<=0) return;
    ImDrawList *d=ImGui::GetBackgroundDrawList();
    float sx=gUiPresentW/512.0f, sy=gUiPresentH/448.0f;
    auto point=[&](float x,float y){return ImVec2(std::round(gUiPresentX+x*sx),std::round(gUiPresentY+y*sy));};
    if(!s.shown || s.paused || Port_TagOption(TAG_OPT_HIDE_ALL_HUD)) return;
    const bool onlineObserver=TagNet_Active() && TagNet_Observer();
    int views=s.shared?1:s.views;
    for(int v=0;v<views;v++) {
        if(!s.view[v].visible) continue;
        TagHudView &view=s.view[v]; int actor=view.actor; auto &a=s.actors[actor];
        int x=s.shared?0:view.rect[0],y=s.shared?0:view.rect[1],w=s.shared?512:view.rect[2],h=s.shared?448:view.rect[3];
        ImVec2 begin=point(x,y),end=point(x+w,y+h);
        d->PushClipRect(begin,end,true);
        float sc=std::min(sx,sy)*(s.views>=3&&!s.shared?0.73f:s.views==2&&!s.shared?0.78f:1.0f);
        float margin=6*sc;
        char label[96]; int controller=a.fusionBody>=0?a.fusionController:a.player;
        if(view.spectating) snprintf(label,sizeof(label),Port_TagText("Watching %s"),TagHud_Name(a.chara));
        else snprintf(label,sizeof(label),controller>=0?"P%d":"CPU %d",controller>=0?controller+1:actor+1);
        // Native panels use half scale in every split rectangle and full
        // scale only in the merged/fullscreen view.
        // Names, event messages and combo counters occupy the area below
        // the native gauges. Keep our status block below their full extent,
        // even while they slide or grow on a hit.
        float row=begin.y+216*sy*(w==512&&h==448?1.0f:0.5f);
        bool allies=Port_TagOption(TAG_OPT_ALLY_PANEL)!=0;
        bool columns=allies&&(a.fusionBody>=0||a.assistAlly>=0||a.assistPower>100);
        float statusX=begin.x+margin;
        float statusWidth=end.x-begin.x-2*margin;
        if(columns) statusWidth=(statusWidth-margin)*0.5f;
        float allyRow=row+20*sc;
        if(!onlineObserver && (view.spectating ? Port_TagOption(TAG_OPT_SPECTATOR_NAME) : Port_TagOption(TAG_OPT_PLAYER_LABELS)))
            text(d,statusX,row,8*sc,team(a.team),label,statusWidth);
        for(int i=0;i<s.count;i++) if(allies&&i!=actor&&s.actors[i].team==a.team) {
            auto &ally=s.actors[i];
            if(ally.fusedInto>=0 || ally.absent) continue;
            snprintf(label,sizeof(label),"%s %s",Port_TagText(ally.eliminated?"OUT":ally.down?"DOWN":"ALLY"),TagHud_Name(ally.chara));
            if(Port_TagOption(TAG_OPT_ALLY_NAMES)) {
                text(d,statusX,allyRow,8*sc,team(a.team),label,statusWidth); allyRow+=18*sc;
            }
            bar(d,statusX,allyRow,std::min(80*sc,statusWidth-3*sc),3*sc,
                ally.hpMax>0?(float)ally.hp/ally.hpMax:0,team(a.team)); allyRow+=8*sc;
        }
        if(columns) statusX+=statusWidth+margin;
        else if(!allies) row+=20*sc;
        for(int i=0;i<s.count;i++) {
            auto &other=s.actors[i]; auto &head=view.heads[i];
            if(other.fusedInto>=0 || other.absent) continue;
            if(head.visible && other.practiceLabel[0] && Port_TagOption(TAG_OPT_PRACTICE_LABELS)) {
                ImVec2 p=point(head.x,head.y);
                float hs=sc*head.scale, fs=14*hs;
                float width=ImGui::GetFont()->CalcTextSizeA(fs,10000,0,other.practiceLabel).x;
                bool clear=true;
                for(int j=0;j<s.count;j++)if(j!=i && !s.actors[j].absent && s.actors[j].practiceLabel[0] && view.heads[j].visible) {
                    auto &q=view.heads[j];
                    if(i==a.target || (j!=a.target && (q.scale<head.scale || (q.scale==head.scale && j>i))))continue;
                    ImVec2 qp=point(q.x,q.y);
                    float qw=ImGui::GetFont()->CalcTextSizeA(14*sc*q.scale,10000,0,s.actors[j].practiceLabel).x;
                    if(fabsf(qp.x-p.x)<(width+qw)*.5f+3*sc && fabsf(qp.y-p.y)<18*sc){clear=false;break;}
                }
                if(clear)text(d,p.x-width*.5f,p.y-29*hs,fs*.5f,IM_COL32(255,240,170,255),other.practiceLabel);
            }
            if(i!=actor && head.visible && i==a.target && Port_TagOption(TAG_OPT_TARGET_MARKER)) {
                ImVec2 p=point(head.x,head.y); p.y-=(other.practiceLabel[0]&&Port_TagOption(TAG_OPT_PRACTICE_LABELS)?46:other.down?45:12)*sc;
                int style=Port_TagOption(TAG_OPT_TARGET_STYLE);
                auto color=team(other.team);
                if(style==0||style==3) marker(d,p,sc,color);
                if(style==1||style==3) {d->AddCircle(p,6*sc,IM_COL32(0,0,0,230),24,4*sc);d->AddCircle(p,6*sc,IM_COL32(255,221,45,255),24,2*sc);}
                if(style==2)corners(d,p,sc,color);
            }
            if(Port_TagOption(TAG_OPT_ATTACK_WARNING) && other.team!=a.team && other.target==actor && !other.down&&!other.eliminated) {
                auto &warning=view.warnings[i];ImVec2 p=point(warning.x,warning.y);
                // Keep the pointer beside the fighter's health and target
                // marker. Offscreen threats still use the viewport edge.
                if(warning.visible)p.x+=34*sc;
                float cx=(begin.x+end.x)*0.5f,cy=(begin.y+end.y)*0.5f;
                float dx=p.x-cx,dy=p.y-cy,rx=(end.x-begin.x)*0.5f-15*sc,ry=(end.y-begin.y)*0.5f-25*sc;
                if(!warning.visible || fabsf(dx)>rx || fabsf(dy)>ry) {
                    float ratio=std::max(fabsf(dx)/rx,fabsf(dy)/ry);
                    if(ratio>0.001f){p.x=cx+dx/ratio;p.y=cy+dy/ratio;}
                }
                float angle=atan2f(p.y-cy,p.x-cx),cs=cosf(angle),sn=sinf(angle),r=6*sc;
                ImU32 col=other.attacking&&s.frame%12<6?IM_COL32(255,220,155,255):IM_COL32(245,55,40,255);
                ImVec2 tip(p.x+cs*r,p.y+sn*r),a1(p.x-cs*r-sn*r*.65f,p.y-sn*r+cs*r*.65f),a2(p.x-cs*r+sn*r*.65f,p.y-sn*r-cs*r*.65f);
                d->AddTriangle(tip,a1,a2,IM_COL32(0,0,0,255),3*sc);d->AddTriangleFilled(tip,a1,a2,col);
            }
            if(s.reviveEnabled&&other.team==a.team&&other.down&&!other.eliminated) {
                const auto &r=view.rings[i];
                float phase=s.frame*(6.283185307f/3000)*Port_TagOption(TAG_OPT_REVIVE_WAVE_SPEED)+i*(17*6.283185307f/256);
                if(r.visible)revival_ring(d,phase,[&](float x,float z,float rise,ImVec2 &out) {
                    float clip[4];
                    for(int k=0;k<4;k++)clip[k]=r.centre[k]+x*r.x[k]+z*r.z[k]+rise*r.y[k];
                    if(!std::isfinite(clip[3])||clip[3]<=.3f)return false;
                    float px=clip[0]/clip[3]-1792,py=clip[1]/clip[3]-1824;
                    if(!std::isfinite(px)||!std::isfinite(py))return false;
                    out=point(px,py);
                    // Reject near-plane/boundary quads rather than allowing
                    // giant ribbons or spilling into another player's view.
                    return out.x>=begin.x&&out.x<=end.x&&out.y>=begin.y&&out.y<=end.y;
                });
            } else if(i!=actor&&head.visible&&!other.down&&!other.eliminated) {
                ImVec2 p=point(head.x,head.y);
                float hs=sc*head.scale;
                if(Port_TagOption(other.team==a.team?TAG_OPT_ALLY_OVERHEAD:TAG_OPT_ENEMY_OVERHEAD))
                    bar(d,p.x-20*hs,p.y,40*hs,3*hs,other.hpMax>0?(float)other.hp/other.hpMax:0,team(other.team));
            }
        }

        if(view.reviveBody>=0) {
            const auto &body=s.actors[view.reviveBody];
            int missing=std::max(0,s.reviveCost-a.stocks/100000);
            if(missing) snprintf(label,sizeof(label),Port_TagText("Need %d blast stock%s to revive"),missing,missing==1?"":"s");
            else snprintf(label,sizeof(label),Port_TagText("Reviving - %d%%"),body.reviveTicks*100/std::max(1,s.reviveDuration));
            float available=end.x-begin.x-2*margin;
            float width=std::min(available,ImGui::GetFont()->CalcTextSizeA(18*sc,10000,0,label).x);
            float x=(begin.x+end.x-width)*.5f,y=end.y-77*sc;
            if(Port_TagOption(missing?TAG_OPT_REVIVE_COST_TEXT:TAG_OPT_REVIVE_PROGRESS_TEXT))
                text(d,x,y,9*sc,IM_COL32(110,255,210,255),label,available);
            if(!missing) bar(d,(begin.x+end.x)*.5f-42*sc,y+25*sc,84*sc,5*sc,
                (float)body.reviveTicks/std::max(1,s.reviveDuration),IM_COL32(100,255,200,255),1);
        }
        // Observers have no control actor. Never index actors[-1] or offer
        // another participant's fusion/assist/takeover controls to a viewer.
        const bool controls=view.controlActor>=0 && view.controlActor<s.count && !onlineObserver;
        const int request=controls?s.actors[view.controlActor].fusionRequest:-1;
        if(request>=0 && request<s.count && Port_TagOption(TAG_OPT_FUSION_REQUEST_TEXT)) {
            snprintf(label,sizeof(label),Port_TagText("P%d requests fusion - press R3"),s.actors[request].player+1);
            text(d,begin.x+margin,end.y-38*sc,9*sc,IM_COL32(255,220,120,255),label,end.x-begin.x-2*margin);
        }
        if(a.fusionBody>=0) {
            // The left edge below the gauge belongs to the native reserve
            // portraits. Start to their right, immediately below HP/ki.
            const float nativeScale=w==512&&h==448?1.0f:0.5f;
            const float fusionX=begin.x+94*sx*nativeScale;
            float fusionY=begin.y+78*sy*nativeScale;
            const float fusionWidth=end.x-fusionX-margin;
            const auto gold=IM_COL32(255,220,120,255);
            const float width=std::min(148*sc,fusionWidth-3*sc);
            const float textWidth=fusionWidth;
            if(Port_TagOption(TAG_OPT_FUSION_OWNER)) {
                if(a.fusionController>=0) snprintf(label,sizeof(label),Port_TagText("Fused - P%d has control"),a.fusionController+1);
                else snprintf(label,sizeof(label),"%s",Port_TagText("Fused - CPU has control"));
                text(d,fusionX,fusionY,8*sc,gold,label,textWidth); fusionY+=21*sc;
            }
            // Match the PCSX2 HUD: warn only as the control handoff approaches.
            if(a.fusionNext>=0 && a.fusionSwitchSeconds>0 && a.fusionSwitchSeconds<=5 &&
               Port_TagOption(TAG_OPT_FUSION_COUNTDOWN)) {
                snprintf(label,sizeof(label),Port_TagText("P%d takes control in %ds"),a.fusionNext+1,a.fusionSwitchSeconds);
                text(d,fusionX,fusionY,8*sc,gold,label,textWidth); fusionY+=21*sc;
            }
            if(a.fusionSeconds>=0) {
                if(Port_TagOption(TAG_OPT_FUSION_TIME_TEXT)) {
                    snprintf(label,sizeof(label),Port_TagText("Fusion - %ds remaining"),a.fusionSeconds);
                    text(d,fusionX,fusionY,8*sc,gold,label,textWidth); fusionY+=21*sc;
                }
                if(Port_TagOption(TAG_OPT_FUSION_METER)) {
                    bar(d,fusionX,fusionY,width,7*sc,a.fusionDuration>0?(float)a.fusionSeconds/a.fusionDuration:0,gold);
                    fusionY+=15*sc;
                }
            }
            row=std::max(row,fusionY+6*sc);
        }
        if(a.assistAlly>=0 || a.assistPower>100) {
            snprintf(label,sizeof(label),Port_TagText("Beam assist - %d%% power"),a.assistPower);
            if(Port_TagOption(TAG_OPT_ASSIST_POWER_TEXT))
                text(d,statusX,row,9*sc,IM_COL32(110,255,210,255),label,statusWidth);
        } else if(controls && !view.spectating && a.assistReady>=0 && request<0 && Port_TagOption(TAG_OPT_ASSIST_HINT_TEXT)) {
            text(d,begin.x+margin,end.y-38*sc,9*sc,IM_COL32(110,255,210,255),
                "R3 - beam assist (1 stock)",end.x-begin.x-2*margin);
        }
        if(view.spectating && !onlineObserver && Port_TagOption(TAG_OPT_SPECTATOR_HINTS)) {
            text(d,begin.x+margin,end.y-24*sc,9*sc,IM_COL32(255,220,120,255),
                view.takeover>=0?"L3 - change view   Square - take over teammate":"L3 - change spectator view",end.x-begin.x-2*margin);
        }
        d->PopClipRect();
    }
}
