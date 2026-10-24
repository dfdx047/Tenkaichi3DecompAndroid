#ifdef PORT
#include "common.h"
#include "battle/tag_challenge.h"
#include "battle/tag_lobby.h"
#include "battle/tag_team.h"
#include "battle/tag_hud_data.h"
#include "battle/tag_fusion.h"
#include "battle/tag_assist.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_cam.h"
#include "battle/btl_seq.h"
#include "battle/hud_a.h"
#include "battle/btl_input.h"

extern void *memset(void *, s32, u32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern s32 BtlChar_TestFlag(BtlMgrChr *, s32);
extern void BtlCharApi_GetPos(s32, Vec4 *);
extern f32 BtlCharApi_GetHeight(s32);
extern s32 BtlCharApi_GetChara(s32);
extern s32 BtlScene_IsSingleView(void);
extern s32 DemoCam_IsActive(void);
extern BtlCamView *BtlCam_GetNativeView(s32);
extern void Port_TagHudSubmit(u32);
extern void TagRevive_Update(void);
extern s32 TagRevive_Progress(s32);
extern s32 TagRevive_Helper(s32);
extern s32 TagRevive_Enabled(void);
extern s32 TagRevive_Duration(void);
extern s32 TagRevive_Cost(void);
extern s32 TagRevive_Nearby(s32);
extern f32 TagRevive_Radius(void);
extern s32 TagOnline_PresentationActor(s32);
extern s32 TagNet_Active(void),TagNet_Role(void);

static void project(TagHudPoint *out, View *view, Vec4 *point) {
    Mtx44 *m = &view->world2screen;
    f32 w = point->x * m->m[0][3] + point->y * m->m[1][3] + point->z * m->m[2][3] + m->m[3][3];
    out->visible = w > 0.3f;
    /* A gentle perspective cue, bounded at both ends to keep distant bars
       legible and close fighters from filling the view with UI. */
    out->scale = 0.8f + 0.65f * 80.0f / (80.0f + (w>0?w:0));
    if (!out->visible) return;
    out->x = (point->x*m->m[0][0]+point->y*m->m[1][0]+point->z*m->m[2][0]+m->m[3][0])/w - 1792.0f;
    out->y = (point->x*m->m[0][1]+point->y*m->m[1][1]+point->z*m->m[2][1]+m->m[3][1])/w - 1824.0f;
}

void TagTeam_UpdateMechanics(void) {
    if(!TagTeam_Active())return;
    { extern void TagTeam_TestTick(void); TagTeam_TestTick(); }
    { extern void TagOnline_TestTick(void); TagOnline_TestTick(); }
    TagRevive_Update();
    { extern void TagTraining_Update(void); TagTraining_Update(); }
    TagLobby_AfterUpdate();TagLobby_TestTick();
}

void TagTeam_AfterUpdate(void) {
    static TagHudState s;
    s32 i, v, k;
    memset(&s, 0, sizeof(s));
    if (!TagTeam_Active() || TagLobby_LocalMenu()) { Port_TagHudSubmit((u32)&s); return; }
    s.active = 1; s.count = TagTeam_Count(); s.views = TagTeam_ViewCount();
    s.shared = BtlScene_IsSingleView();
    s.paused = (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) != 0;
    s.shown = !s.paused && BtlSeq_IsFighting() && (gHud->flags & HUD_SHOW_GAUGES) &&
        !DemoCam_IsActive() && BtlCam_GetPriorityView() < 0;
    { extern s32 Port_TagShowcase(void); if(Port_TagShowcase())s.shown=0; }
    s.timeLeft = BtlSeq_GetTimeLeft(); s.frame = BtlSeq_GetClock()->ticks;
    s.reviveEnabled = TagRevive_Enabled(); s.reviveDuration = TagRevive_Duration(); s.reviveCost = TagRevive_Cost();
    for (i = 0; i < s.count; i++) {
        BtlMgrChr *chr = BtlChar_Get(i);
        BtlMgrGauge *g = BtlMember_GetActiveGauge(chr);
        TagHudActor *a = &s.actors[i];
        a->chara = BtlCharApi_GetChara(chr->objId);
        a->costume = TagTeam_Side(i)->form.costume;
        a->team = TagTeam_Team(i); a->player = TagTeam_Player(i); a->target = TagTeam_LockedOff(i) ? -1 : TagTeam_Target(i);
        a->hp = g->health; a->hpMax = g->healthMax; a->ki = g->ki; a->kiMax = g->kiMax;
        a->stocks = g->blast; a->stockMax = g->blastMax;
        a->attacking=(chr->action>=0x55&&chr->action<=0xBC) ||
            (chr->action>=0xFD&&chr->action<=0x12F);
        a->down = g->health <= 0; a->eliminated = BtlChar_TestFlag(chr, 7);
        a->fusedInto=TagFusion_Hidden(i)?TagFusion_Owner(i):-1;
        a->absent=TagChallenge_Hidden(i) || TagLobby_Hidden(i);
        {
            const char *label=TagLobby_Label(i);s32 j;
            for(j=0;j<31 && label[j];j++)a->practiceLabel[j]=label[j];
        }
        a->fusionSeconds=TagFusion_Seconds(i);a->fusionRequest=TagFusion_Requester(i);
        a->fusionBody=TagFusion_SharedBody(i);
        a->fusionDuration=TagFusion_Duration(i);
        a->fusionController=TagFusion_Controller(i);
        a->fusionNext=TagFusion_NextController(i);
        a->fusionSwitchSeconds=TagFusion_SwitchSeconds(i);
        a->assistAlly=TagAssist_Ally(i);a->assistReady=TagAssist_ReadyAlly(i);
        a->assistPower=TagAssist_Multiplier(a->assistAlly>=0?a->assistAlly:i);
        a->hits = chr->unkD44; a->damage = chr->unkD40;
        {
            extern void Port_TagInputTrace(s32,s32,u32,s32);
            Port_TagInputTrace(i,chr->pad,BtlInput_Get((BtlInputChr *)chr)->pressed,chr->action);
        }
        a->helper = TagRevive_Helper(i); a->reviveTicks = TagRevive_Progress(i);
        a->prompt = (chr->action >= 0x130 && chr->action <= 0x132) ||
                    chr->action == 0xFA || chr->action == 0xFB || chr->action == 0xFC;
    }
    for (v = 0; v < s.views; v++) {
        View *view = &BtlCam_GetNativeView(v)->view;
        TagHudView *h = &s.view[v];
        h->visible=s.shared?v==0:TagTeam_ViewVisible(v);
        if(!h->visible) continue;
        h->actor = TagOnline_PresentationActor(TagTeam_ViewActor(v));
        h->controlActor = TagTeam_ControlActor(v);
        h->takeover = TagTeam_TakeoverCandidate(v);
        h->spectating = TagTeam_Spectating(v);
        if(TagNet_Active()) {
            s32 role=TagNet_Role();
            h->controlActor=role>=0 && role<10?TagTeam_ControlActor(role):-1;
            {extern int TagNet_Observer(void);h->spectating=TagNet_Observer() || TagTeam_Spectating(role);
            h->takeover=TagNet_Observer()?-1:TagTeam_TakeoverCandidate(role);}
        }
        h->reviveBody = h->spectating ? -1 : TagRevive_Nearby(h->actor);
        TagTeam_ViewRect(v, h->rect);
        if(TagNet_Active()){h->rect[0]=h->rect[1]=0;h->rect[2]=512;h->rect[3]=448;}
        for (i = 0; i < s.count; i++) {
            Vec4 p;
            BtlMgrChr *chr = BtlChar_Get(i);
            BtlCharApi_GetPos(chr->objId, &p);
            /* Fallen fighters lie on the floor: their standing skeleton
               height would put the revival gauge far above the body. */
            p.y -= BtlCharApi_GetHeight(chr->objId) * (s.actors[i].down ? 0.2f : 1.0f) + 0.5f;
            if(s.actors[i].practiceLabel[0]) {
                extern void BtlCharApi_GetNodePos(s32,s32,Vec4 *);
                /* Saibamen crouch well below their full model bounds. Keep
                   the station label over the actual head, including recovery. */
                BtlCharApi_GetNodePos(chr->objId,0x11,&p);p.y-=1.5f;
            }
            project(&h->heads[i], view, &p);
            {
                Mtx44 *m=&view->world2screen;
                f32 w=p.x*m->m[0][3]+p.y*m->m[1][3]+p.z*m->m[2][3]+m->m[3][3];
                TagHudPoint *a=&h->warnings[i];
                f32 cx=1792+h->rect[0]+h->rect[2]*0.5f,cy=1824+h->rect[1]+h->rect[3]*0.5f;
                a->visible=w>0.3f;
                if(w>0.3f) *a=h->heads[i];
                else {
                    /* A behind-camera threat still has a direction. Project
                       camera-relative X/Y without flipping on negative W. */
                    f32 dx=p.x*m->m[0][0]+p.y*m->m[1][0]+p.z*m->m[2][0]+m->m[3][0]-cx*w;
                    f32 dy=p.x*m->m[0][1]+p.y*m->m[1][1]+p.z*m->m[2][1]+m->m[3][1]-cy*w;
                    if(__builtin_fabsf(dx)+__builtin_fabsf(dy)<0.001f)dy=1;
                    a->x=h->rect[0]+h->rect[2]*0.5f+dx;
                    a->y=h->rect[1]+h->rect[3]*0.5f+dy;
                }
            }
            if (!s.shown || h->spectating || !s.reviveEnabled || !s.actors[i].down || s.actors[i].eliminated ||
                TagTeam_IsEnemy(h->actor,i)) continue;
            BtlCharApi_GetPos(chr->objId, &p);
            p.y -= 0.1f;
            for (k = 0; k < 4; k++) {
                TagHudRing *r = &h->rings[i];
                Mtx44 *m = &view->world2screen;
                r->centre[k] = p.x*m->m[0][k]+p.y*m->m[1][k]+p.z*m->m[2][k]+m->m[3][k];
                r->x[k] = m->m[0][k]*TagRevive_Radius();
                r->z[k] = m->m[2][k]*TagRevive_Radius();
                r->y[k] = -m->m[1][k]; /* Negative world Y is up. */
                r->visible = 1;
            }
        }
    }
    Port_TagHudSubmit((u32)&s);
}
#endif
