/* Bridge the immutable online room into the game's native battle setup. */
#ifdef PORT
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_session.h"
#include "battle/tag_fusion.h"
#include "battle/tag_assist.h"
#include "battle/tag_clash.h"
#include "battle/tag_lobby.h"
#include "battle/battle_setup.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_seq.h"
#include "sys/save.h"
#include "../../port/src/gs/tag_net.h"
extern void *memset(void *,s32,u32);
extern void Port_Srand(unsigned),Rand_SeedByArray(unsigned *,int);
extern int Port_Rand(void),CpuLevel_FromSetting(u32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern s32 TagRevive_Progress(s32),TagRevive_Helper(s32);
extern unsigned long long gPortRandNext;
static SaveData offlineSave;
static s32 saveScoped;
unsigned long long TagOnline_SaveFingerprint(void) {
    unsigned long long h=14695981039346656037ull;u8 *p=(u8 *)gSaveData;s32 i;
    for(i=0;p && i<SAVE_SIZE;i++)h=(h^p[i])*1099511628211ull;
    return h;
}
/* Online uses an identical unlocked save while selecting and loading. Keep
   that temporary state out of the offline game when entered via its menu. */
void TagOnline_SaveScope(s32 enter) {
    extern void *memcpy(void *,const void *,u32);
    extern void PortGame_OnlineCustom(s32);
    if(enter && !saveScoped && gSaveData) {
        memcpy(&offlineSave,gSaveData,SAVE_SIZE);saveScoped=1;
        PortGame_OnlineCustom(1);Save_UnlockAll(gSaveData);
    } else if(!enter && saveScoped) {
        memcpy(gSaveData,&offlineSave,SAVE_SIZE);saveScoped=0;
        PortGame_OnlineCustom(0);
    }
}
void TagOnline_Seed(void) {
    TagNetConfig c;unsigned seed[4];s32 i;TagNet_CopyConfig((u32)&c);
    Port_Srand((unsigned)c.seed);for(i=0;i<4;i++)seed[i]=Port_Rand();Rand_SeedByArray(seed,4);
}
void Port_TagOnlineSetup(void) {
    TagNetConfig c;s32 i,t,m,actor=0;BattleItemSet items;
    TagNet_CopyConfig((u32)&c);TagSession_Leave();TagLobby_LeavePractice();
    if(c.party && !c.practice){TagSession_Setup(c.mode,0);TagSession_Commit();}
    if(c.party && c.practice)TagLobby_EnterPractice(&c.lobby);
    {extern void TagOnline_TestReset(void);TagOnline_TestReset();}
    Save_SetDefaults(gSaveData);Save_UnlockAll(gSaveData);
    if(saveScoped) {
        gSaveData->bgmVolume=offlineSave.bgmVolume;gSaveData->seVolume=offlineSave.seVolume;
        gSaveData->soundMode=offlineSave.soundMode;
        gSaveData->screenX=offlineSave.screenX;gSaveData->screenY=offlineSave.screenY;
    }
    TagOnline_Seed();
    Battle_ClearWork();TagTeam_Enable(1);TagTeam_Configure(TN_PLAYERS,TN_PLAYERS,0);
    {static s32 announcer[]={4,5,6,2,3};s32 tour=c.party&&!c.practice&&c.mode==TL_TOURNAMENT;
     BattleSetup_SetRule(1,tour?4:0,c.music,c.time,tour?announcer[c.preset%5]:0,c.stage,1);}
    memset(&items,0,sizeof(items));
    for(t=0;t<2;t++) {
        BattleSetup_SetSide(t,BTL_CONTROL_PAD,t,c.size[t],1,1,0,NULL);
        for(m=0;m<c.size[t];m++) {
            s32 k;
            i=(t?c.size[0]:0)+m;
            for(k=0;k<8;k++)((u16 *)&items)[k]=c.items[i][k];
            BattleSetup_SetMember(t,m,c.chara[i],c.costume[i],0,CpuLevel_FromSetting(c.cpuLevel[i]<0?c.cpu:c.cpuLevel[i]),100.0f,&items);
        }
    }
    if(c.party && !c.practice && c.mode==TL_GAUNTLET) {
        extern void TagChallenge_Choose(s32),TagChallenge_SetRefill(s32),TagChallenge_SetupBattle(void);
        TagChallenge_Choose(c.preset);TagChallenge_SetRefill(c.heal);TagChallenge_SetupBattle();
    }
    /* Runtime actor order is interleaved by team, room order is contiguous. */
    for(m=0;m<5;m++)for(t=0;t<2;t++)if(m<Battle_GetSetup()->sides[t].memberCount) {
        i=(t?c.size[0]:0)+m;TagTeam_SetAssignment(actor++,c.party && c.mode==TL_GAUNTLET && t==1?-1:c.owner[i]);
    }
    BattleSetup_Finish();
}
void TagOnline_ReportResult(void) {
    s32 flags=BattleResult_GetFlags();TagNet_BattleResult((flags&3)==1?0:(flags&3)==2?1:-1);
}
/* Pointer-free, fixed-width semantic checks for every fighter, not only the
   two original slots. Local camera/HUD/audio preferences do not participate. */
unsigned long long TagOnline_Digest(void) {
    struct Actor {
        s32 chara,costume,hp,maxHp,ki,blast,action,target,player,off,clash;
        s32 fused,controller,next,seconds,swap,revive,helper,assist,power;
        Vec4 pos,rot;
    } a;
    unsigned long long h=14695981039346656037ull;unsigned char *p;s32 i,j;
    s32 header[4]={TagTeam_Count(),gBtlSeq?gBtlSeq->state:0,gBtlSeq?BtlSeq_GetClock()->ticks:0,0};
    extern void TagNet_Audit(s32,u32,u32);
    TagNet_Audit(-1,(u32)header,sizeof(header));
    p=(unsigned char *)header;for(j=0;j<sizeof(header);j++)h=(h^p[j])*1099511628211ull;
    if(!TagTeam_Active() || !gBtlSeq)return h;
    for(i=0;i<TagTeam_Count();i++) {
        BtlMgrChr *chr=BtlChar_Get(i);BtlMgrGauge *g=BtlMember_GetActiveGauge(chr);
        memset(&a,0,sizeof(a));a.chara=TagTeam_Side(i)->form.chara;a.costume=TagTeam_Side(i)->form.costume;
        a.hp=g->health;a.maxHp=g->healthMax;a.ki=g->ki;a.blast=g->blast;a.action=chr->action;
        a.target=TagTeam_Target(i);a.player=TagTeam_InputPlayer(i);a.off=TagTeam_LockedOff(i);
        a.clash=BtlClash_Opponent(i);a.fused=TagFusion_SharedBody(i);a.controller=TagFusion_Controller(i);
        a.next=TagFusion_NextController(i);a.seconds=TagFusion_Seconds(i);a.swap=TagFusion_SwitchSeconds(i);
        a.revive=TagRevive_Progress(i);a.helper=TagRevive_Helper(i);a.assist=TagAssist_Ally(i);a.power=TagAssist_Multiplier(i);
        a.pos=((BtlMovePose *)&chr->pose)->pos;a.rot=((BtlMovePose *)&chr->pose)->rot;
        TagNet_Audit(i,(u32)&a,sizeof(a));
        p=(unsigned char *)&a;for(j=0;j<sizeof(a);j++)h=(h^p[j])*1099511628211ull;
    }
    p=(unsigned char *)&gPortRandNext;for(j=0;j<8;j++)h=(h^p[j])*1099511628211ull;
    TagNet_Audit(-2,(u32)&gPortRandNext,8);
    {extern unsigned long long TagLobby_Digest(void);h^=TagLobby_Digest();}
    return h;
}
#endif
