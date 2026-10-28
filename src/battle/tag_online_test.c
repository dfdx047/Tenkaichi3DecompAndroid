/* Deterministic, opt-in socket regression fixtures. Disabled in normal play. */
#ifdef PORT
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_fusion.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_act_j.h"
#include "battle/btl_input.h"
#include "battle/btl_seq.h"
#include "battle/battle.h"
#include "battle/hud_0.h"
#include "battle/tag_lobby.h"
#include "battle/tag_session.h"
#include "battle/tag_challenge.h"
#include "sys/pad.h"
extern s32 TagNet_TestCase(void);
extern s32 TagNet_TestInputs(void);
extern void TagNet_TestCheck(s32,s32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern s32 BtlParam_GetUnkB4(BtlMgrChr *,s32);
extern s32 BtlMember_Damage(BtlMgrChr *,s32,s32);
extern void BtlAct_ClearQueue(BtlMgrChr *),BtlAct_Request(BtlMgrChr *,s32);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);
extern s32 BtlStage_FindZoneNear(s32,Vec4 *);
extern s32 TagRevive_Progress(s32),TagRevive_Helper(s32);
static s32 tick,fused,switched,revived,complete,inputChecked,inputTicks;
void TagOnline_TestReset(void) {tick=fused=switched=revived=complete=inputChecked=inputTicks=0;}
void TagOnline_TestDone(void) {complete=1;}
void TagOnline_TestPrepare(void) {
    s32 i;
    if(!TagNet_TestCase() || !TagTeam_Active())return;
    /* Park the CPUs only in the directed mechanics cases. Ordinary socket
       stress runs retain the actual AI for every unclaimed fighter. */
    for(i=0;i<TagTeam_Count();i++)if(TagTeam_InputPlayer(i)<0) {
        BtlMgrChr *c=BtlChar_Get(i);BtlInputChr *in=(BtlInputChr *)c;
        c->injectOn=1;in->injectButtons=0;in->injectStickX=in->injectStickY=0;
    }
}
s32 TagOnline_TestPark(void) {return TagNet_TestCase()!=0;}
static void place(s32 actor,f32 x,f32 y,f32 z) {
    BtlMgrChr *c=BtlChar_Get(actor);Vec4 p={x,y,z,1},r=((BtlMovePose *)&c->pose)->rot;
    BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));
}
void TagOnline_TestTick(void) {
    s32 mode=TagNet_TestCase(),i;
    BtlMgrChr *a,*b;
    /* Practice skips Ready/Fight, so its first playable blanks still contain
       the intentional neutral startup padding. Check once real inputs arrive. */
    if(TagNet_TestInputs() && BtlSeq_IsFighting() && !inputChecked && ++inputTicks>=30) {
        inputChecked=1;
        for(i=0;i<TagTeam_Count();i++) {
            s32 player=TagTeam_InputPlayer(i);
            if(player>=0) {
                s32 value=gPad[player].data[6];
                if(TagNet_IsPractice() && !TagNet_MemberPresent(player)) {
                    TagNet_TestCheck(value==0,900+player);continue;
                }
                TagNet_TestCheck(TagTeam_ControlActor(player)==i && BtlChar_Get(i)->pad==player &&
                    value>=(player+1)*10 && value<=(player+1)*10+1,900+player);
            }
        }
    }
    if(!mode || !BtlSeq_IsFighting() || complete)return;
    tick++;a=BtlChar_Get(0);b=BtlChar_Get(2);
    if(mode==3 || mode==4) {extern void TagBeam_TestTick(s32);TagBeam_TestTick(tick);return;}
    if(mode==6 || mode==7) {extern void TagCinema_MatrixTick(s32);TagCinema_MatrixTick(tick);return;}
    if(mode==8) {
        extern BtlMenu *PauseMenu_GetMenu(s32);
        if(tick==1) {
            BtlMenu *menu=PauseMenu_GetMenu(0);
            TagNet_TestCheck(BtlMenu_FindItem(menu,0xD)!=NULL && BtlMenu_FindItem(menu,0xF)==NULL,801);
        }
        if(!fused && (Battle_GetWork()->flags&BATTLE_FLAG_PAUSE_MENU)){fused=1;TagNet_TestCheck(1,802);}
        if(fused && !revived && !(Battle_GetWork()->flags&BATTLE_FLAG_PAUSE_MENU)){revived=1;TagNet_TestCheck(1,803);}
        return;
    }
    if(mode==9) {
        if(TagLobby_Practice()) {
            if(tick==1)TagNet_TestCheck(TagTeam_Count()==15 && TagTeam_ControlActor(0)<0,2500);
        } else if(TagSession_Mode()==TAG_MODE_TRAINING && tick==90) {
            BattleResult_Set(BTL_RESULT_ABORT,0);TagNet_TestCheck(1,2501);complete=1;
        } else if((TagSession_Mode()==TAG_MODE_GAUNTLET && tick%120==90 && !TagChallenge_BetweenWaves()) || tick==90) {
            for(i=1;i<TagTeam_Count();i++) {
                if(TagSession_Mode()==TAG_MODE_GAUNTLET && (TagTeam_Team(i)!=1 || !TagTeam_IsAlive(i)))continue;
                BtlMgrChr *c=BtlChar_Get(i);BtlMember_Damage(c,BtlMember_GetActiveGauge(c)->health+1000,0x1C3);
                BtlAct_ClearQueue(c);BtlAct_Request(c,0xD8);
            }
            TagNet_TestCheck(1,2501);if(TagSession_Mode()!=TAG_MODE_GAUNTLET)complete=1;
        }
        return;
    }
    if(tick==1)for(i=0;i<TagTeam_Count();i++)place(i,100+(i%5)*45,-100,(i&1)?250:-250);
    if(mode==1) {
        if(tick==30) {
            place(0,0,0,0);place(2,30,0,0);BtlMember_GetActiveGauge(a)->blast=400000;
            BtlMember_Damage(b,BtlMember_GetActiveGauge(b)->health+1000,0x1C3);
            BtlAct_ClearQueue(b);BtlAct_Request(b,0xD8);
            TagNet_TestCheck(BtlMember_GetActiveGauge(b)->health==0,101);
        }
        if(tick>30 && !revived){place(0,0,0,0);place(2,30,0,0);}
        if(tick==65)TagNet_TestCheck(TagRevive_Progress(2)>15 && TagRevive_Helper(2)==0,102);
        if(tick>30 && BtlMember_GetActiveGauge(b)->health>0 && !revived) {
            revived=1;TagNet_TestCheck(BtlMember_GetActiveGauge(b)->health==BtlMember_GetActiveGauge(b)->healthMax/4,103);
            TagNet_TestCheck(BtlMember_GetActiveGauge(a)->blast<300000,104);
        }
        if(tick==250){TagNet_TestCheck(revived,105);complete=1;}
    } else if(mode==2) {
        if(tick==30) {
            s32 partner=-1,slot=-1;
            place(0,0,0,0);place(2,35,0,0);
            BtlMember_GetActiveGauge(a)->blast=BtlMember_GetActiveGauge(a)->blastMax;
            BtlMember_GetActiveGauge(b)->blast=BtlMember_GetActiveGauge(b)->blastMax;
            for(i=0;i<3;i++)if(BtlParam_GetUnkB4(a,i)==53)slot=i;
            TagNet_TestCheck(slot>=0,201);
            if(slot<0){complete=1;return;}
            TagNet_TestCheck(BtlAct_CanFuse((BtlActJChr *)a,slot,1,1,&partner)&&partner==2,202);
            BtlAct_QueueFusion((BtlActJChr *)a,slot,partner);
        }
        if(tick==35){s32 p=TagTeam_Player(2);gPad[p].pressed|=PAD_R3;TagFusion_Update();gPad[p].pressed&=~PAD_R3;TagNet_TestCheck(TagFusion_Hidden(2),203);}
        if(TagTeam_Side(0)->form.chara==53) {
            if(!fused){fused=1;TagNet_TestCheck(1,204);}
            if(TagFusion_Controller(0)==TagTeam_Player(2) && !switched){switched=1;TagNet_TestCheck(1,205);}
        }
        if(fused && !TagFusion_Hidden(2) && TagTeam_Side(0)->form.chara==3) {
            TagNet_TestCheck(switched && TagTeam_InputPlayer(0)==TagTeam_Player(0) && TagTeam_InputPlayer(2)==TagTeam_Player(2),206);
            complete=1;
        }
        if(tick==1800){TagNet_TestCheck(0,207);complete=1;}
    } else if(mode==5 && tick==90) {
        for(i=0;i<TagTeam_Count();i++)if(TagTeam_Team(i)==1) {
            BtlMgrChr *c=BtlChar_Get(i);BtlMember_Damage(c,BtlMember_GetActiveGauge(c)->health+1000,0x1C3);
            BtlAct_ClearQueue(c);BtlAct_Request(c,0xD8);
        }
        TagNet_TestCheck(1,501);complete=1;
    }
}
#endif
