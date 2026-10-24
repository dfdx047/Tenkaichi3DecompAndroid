/* Native gauntlets keep the arena and surviving team resident. Only the
   defeated team's models/voices are replaced through the normal loader. */
#include "common.h"
#include "battle/tag_challenge.h"
#include "battle/tag_session.h"
#include "battle/tag_team.h"
#include "battle/tag_fusion.h"
#include "battle/battle_work.h"
#include "battle/battle_setup.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_seq.h"
#include "battle/btl_obj.h"
#include "battle/btl_char_move.h"
#include "battle/tag_position.h"
#include "battle/stg_a.h"
#include "battle/btl_demo_cam.h"
#include "sys/job.h"
#include "sys/pad.h"
#include "menu/port_screen.h"

extern s32 PortGame_IsBT4(void),CpuLevel_FromSetting(u32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern void BtlChar_BindObject(BtlMgrChr *),BtlChar_Reset(BtlMgrChr *);
extern void BtlChar_PoseToObj(BtlMgrChr *,s32),BtlPartner_Release(BtlMgrChr *);
extern void BtlScene_FreeChar(s32),BtlAiMgr_ResetSide(s32),TagRevive_Reset(void);
extern void BtlAct_ClearQueue(BtlMgrChr *),BtlAct_Request(BtlMgrChr *,s32);
extern void BtlChar_ClearFlag(BtlMgrChr *,s32);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);
extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);

typedef struct TagWaveFighter {s16 chara,costume,items[8];} TagWaveFighter;
typedef struct TagWave {const char *name;s32 level,count;TagWaveFighter fighters[5];} TagWave;
typedef struct TagGauntlet {const char *name;s32 bt4,count,capacity;const TagWave *waves;} TagGauntlet;
#include "tag_gauntlets.inc"

static s32 selection,refill=2,live,wave,rest,loading=-1,nextEnemy,enemyCount;
#define WAVE_STATS_MAX 128
static s32 waveTicks[WAVE_STATS_MAX],waveDamage[WAVE_STATS_MAX],completed;
static s32 bannerTicks,bannerKind,announcedWave=-1,resultPending,resultVisible,resultPage;
static s32 enemies[5];
static u32 hidden;
static u32 departing;
static s32 departureTicks;
static BattleMember originals[5];
static s32 testAge,testLastWave=-1,testIntroAge;
static s32 testCompleted;
static Vec4 testRestPosition,testIntroPosition;
static const TagGauntlet *preset(void) {
    s32 i,count=0,chosen=selection;
    for(i=0;i<sizeof(gauntlets)/sizeof(*gauntlets);i++)if(gauntlets[i].bt4==PortGame_IsBT4())count++;
    if(!count)return NULL;
    chosen%=count;if(chosen<0)chosen+=count;selection=chosen;
    for(i=0;i<sizeof(gauntlets)/sizeof(*gauntlets);i++)if(gauntlets[i].bt4==PortGame_IsBT4() && chosen--==0)return &gauntlets[i];
    return NULL;
}
s32 TagChallenge_Count(void) {s32 i,n=0;for(i=0;i<sizeof(gauntlets)/sizeof(*gauntlets);i++)if(gauntlets[i].bt4==PortGame_IsBT4())n++;return n;}
void TagChallenge_Choose(s32 index) {selection=index;preset();}
void TagChallenge_Select(s32 direction) {selection+=direction;preset();}
s32 TagChallenge_FirstCount(void) {return preset()->waves[0].count;}
s32 TagChallenge_Capacity(void) {return preset()->capacity;}
void TagChallenge_SetRefill(s32 value) {refill=value>=0&&value<=4?value:2;}
s32 TagChallenge_FirstFighter(s32 member,s32 costume) {
    const TagWaveFighter *f=&preset()->waves[0].fighters[member];return costume?f->costume:f->chara;
}
const char *TagChallenge_Title(void) {const TagGauntlet *g=preset();if(!g)return "No gauntlets";return g->name;}
void TagChallenge_Refill(s32 direction) {refill=(refill+direction+5)%5;}
const char *TagChallenge_RefillLabel(void) {
    static const char *names[]={"None","1/4 bar","1/2 bar","1 bar","Full"};return names[refill];
}
s32 TagChallenge_Active(void) {return live;}
s32 TagChallenge_Hidden(s32 actor) {return live && (u32)actor<10 && (hidden&(1u<<actor));}
/* Retired opponents immediately leave targeting/revival, but their defeated
   models finish falling and remain in the arena for three seconds. */
s32 TagChallenge_ModelHidden(s32 actor) {
    return TagChallenge_Hidden(actor) && !(departureTicks>0 && (departing&(1u<<actor)));
}
s32 TagChallenge_BetweenWaves(void) {return live && rest>0;}
s32 TagChallenge_Timer(void) {return TagChallenge_BetweenWaves()?(rest+29)/30:-1;}
s32 TagChallenge_Wave(void) {return live?wave+1:0;}
static BattleItemSet equipment(const TagWaveFighter *f) {
    BattleItemSet items;s32 i;for(i=0;i<8;i++)items.id[i]=f->items[i]+1;return items;
}
void TagChallenge_SetupBattle(void) {
    const TagGauntlet *g=preset();s32 i;
    if(TagSession_Mode()!=TAG_MODE_GAUNTLET || !g)return;
    Battle_GetSetup()->rule.timeLimit=0;
    BattleSetup_SetSide(1,BTL_CONTROL_CPU,0,g->capacity,1,1,0,NULL);
    for(i=0;i<g->capacity;i++) {
        const TagWaveFighter *f=&g->waves[0].fighters[i%g->waves[0].count];
        BattleItemSet items=equipment(f);
        BattleSetup_SetMember(1,i,f->chara,f->costume,0,CpuLevel_FromSetting(g->waves[0].level),100,&items);
    }
}
void TagChallenge_Begin(void) {
    s32 i;live=TagSession_Mode()==TAG_MODE_GAUNTLET;enemyCount=0;hidden=0;loading=-1;rest=0;wave=0;
    departing=0;departureTicks=0;
    if(!live)return;
    for(i=0;i<TagTeam_Count();i++)if(TagTeam_Team(i)==1) {
        enemies[enemyCount]=i;originals[enemyCount++]=TagTeam_Side(i)->members[0];
    }
}
void TagChallenge_Reset(void) {
    s32 i;const TagGauntlet *g=preset();
    if(!live)return;
    wave=0;rest=0;loading=-1;nextEnemy=0;hidden=0;
    departing=0;departureTicks=0;
    memset(waveTicks,0,sizeof(waveTicks));memset(waveDamage,0,sizeof(waveDamage));completed=0;
    bannerTicks=bannerKind=resultPending=resultVisible=resultPage=0;announcedWave=-1;
    testLastWave=-1;testAge=testIntroAge=0;
    for(i=0;i<enemyCount;i++) {
        BattleMember *m=&TagTeam_Side(enemies[i])->members[0];
        void *a=m->buf[0],*b=m->buf[1];
        *m=originals[i];m->buf[0]=a;m->buf[1]=b;m->data=a;
        if(i>=g->waves[0].count)hidden|=1u<<enemies[i];
    }
    Port_TagMenuTrace(85,0,g->count,g->waves[0].count,enemyCount,0);
}
void TagChallenge_End(void) {live=0;hidden=0;departing=0;departureTicks=0;rest=0;loading=-1;resultPending=resultVisible=0;}
s32 TagChallenge_Advance(void) {
    s32 i;static const s32 gains[]={0,2500,5000,10000,2000000000};
    if(!live)return 0;
    if(rest)return 1;
    if(wave+1>=preset()->count) {completed=wave+1;resultPending=1;return 0;}
    /* A finishing move may still be streaming an effect/model. Let its
       native job finish before replacing opponents; that job may need the move's
       remaining animation updates in order to release its slot. */
    if(BtlJob_GetFreeCount()!=BTL_JOB_COUNT)return 1;
    completed=wave+1;bannerKind=2;bannerTicks=150;
    rest=450;nextEnemy=0;loading=-1;wave++;
    /* The last opponent can die inside a cinematic. Release its camera file
       references before the loader replaces that fighter's animation bank. */
    {extern void DemoCam_ResetFighters(void);DemoCam_ResetFighters();DemoCam_Reset();}
    for(i=0;i<TagTeam_Count();i++) {
        BtlMgrChr *c=BtlChar_Get(i);BtlPartner_Release(c);
        if(TagTeam_IsAlive(i)) {BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);}
    }
    for(i=0;i<TagTeam_Count();i++)if(TagTeam_Team(i)==0 && TagTeam_IsAlive(i)) {
        BtlMgrGauge *hp=BtlMember_GetActiveGauge(BtlChar_Get(i));s32 add=gains[refill];
        if(add>hp->healthMax-hp->health)add=hp->healthMax-hp->health;
        hp->health+=add;hp->lowHealth=hp->lowHealthIdle=0;
    }
    /* The dead opponents cannot be revived or selected during the interval.
       All their resources are still allocated until the normal loader replaces them. */
    departing=0;departureTicks=90;
    for(i=0;i<enemyCount;i++) {
        u32 bit=1u<<enemies[i];
        if(!(hidden&bit))departing|=bit;
        hidden|=bit;
    }
    Port_TagMenuTrace(85,wave,preset()->count,preset()->waves[wave].count,enemyCount,rest);
    return 1;
}
static void begin_enemy(s32 index) {
    s32 actor=enemies[index];BattleSide *side=TagTeam_Side(actor);
    const TagWave *w=&preset()->waves[wave];const TagWaveFighter *f=&w->fighters[index];
    BattleItemSet items=equipment(f);BattleMember *m=&side->members[0];
    void *a=m->buf[0],*b=m->buf[1];BtlJob *job;
    BtlPartner_Release(BtlChar_Get(actor));TagFusion_Remove(actor);BtlScene_FreeChar(actor);
    BtlMember_Init(m,f->chara,f->costume,0,CpuLevel_FromSetting(w->level),100,&items);
    m->buf[0]=a;m->buf[1]=b;m->data=b;
    side->form.chara=f->chara;side->form.costume=f->costume;side->form.variant=0;
    /* Keep startForm and buf[0] untouched: Fight Again restores wave one. */
    job=BtlJob_Alloc();memset(job,0,sizeof(*job));job->step=BtlLoad_StepChara;job->kind=BTL_JOB_KIND_RESTART;
    job->side=actor;job->chara=job->animChara=job->unk1C=job->voiceChara=f->chara;
    job->costume=f->costume;job->initial=1;job->modelOnly=job->variant=0;
    Job_Push((Job *)job);loading=actor;
}
static void place_enemy(s32 actor,s32 placed) {
    BtlMgrChr *c=BtlChar_Get(actor);
    s32 i;u32 occupied=0;Vec4 pos,rot;
    for(i=0;i<TagTeam_Count();i++)if(TagTeam_IsAlive(i))occupied|=1u<<i;
    for(i=0;i<placed;i++)occupied|=1u<<enemies[i];
    occupied&=~(1u<<actor);
    pos=((BtlMovePose *)&c->pose)->pos;rot=((BtlMovePose *)&c->pose)->rot;
    if(TagPosition_ReinforcementStart(actor,occupied,&pos,&rot)) {
        BtlChar_Place(c,&pos,&rot,BtlStage_FindZoneNear(-1,&pos));BtlChar_PoseToObj(c,1);
    }
}
static void finish_enemy(s32 actor) {
    BtlMgrChr *c=BtlChar_Get(actor);
    memset(c,0,sizeof(*c));c->side=c->index=actor;c->pad=BattleSide_GetPad(actor);
    BtlChar_BindObject(c);BtlChar_Reset(c);BtlAiMgr_ResetSide(actor);
    BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);BtlChar_PoseToObj(c,1);
    BtlChar_ClearFlag(c,7);
    place_enemy(actor,nextEnemy);
}
void TagChallenge_Update(void) {
    s32 i;const TagWave *w;
    if(!live)return;
    if(!(Battle_GetWork()->flags&BATTLE_FLAG_PAUSE)) {
        if(bannerTicks>0)bannerTicks--;
        if(!rest && BtlSeq_IsFighting() && !resultPending) {
            if(wave<WAVE_STATS_MAX)waveTicks[wave]++;
            if(announcedWave!=wave) {announcedWave=wave;bannerKind=1;bannerTicks=120;}
        }
    }
    {
        extern s32 Port_TagChallengeTest(void);extern void Port_TagTestCheck(s32,s32);
        extern s32 Port_TagModeRushTest(void);
        if(Port_TagChallengeTest() && !Port_TagModeRushTest()) {
            extern s32 Port_TagChallengeSkipTest(void),Port_TagChallengeFinishTest(void),BtlChar_IsFrozen(BtlMgrChr *);
            /* Resource-lifecycle fixture: keep the surviving team alive while
               every authored wave loads and runs ordinary combat. */
            for(i=0;i<TagTeam_Count();i++)if(TagTeam_Team(i)==0) {
                BtlMgrGauge *hp=BtlMember_GetActiveGauge(BtlChar_Get(i));hp->health=hp->healthMax;
            }
            if(testLastWave!=wave) {
                testLastWave=wave;testAge=testIntroAge=0;
                Port_TagMenuTrace(92,wave,Battle_GetBgm(),0,0,0);
            }
            if(rest>2 && rest<440)for(i=0;i<TagTeam_Count();i++)if(TagTeam_Team(i)==0 && TagTeam_IsAlive(i)) {
                extern s32 BtlChar_TestFlag(BtlMgrChr *,s32);
                /* Check the actual action and native flags after the last
                   simulation step, not just the mod's HUD/target settings. */
                Port_TagTestCheck(BtlChar_Get(i)->action!=0x36,2156);
                Port_TagTestCheck(!BtlChar_TestFlag(BtlChar_Get(i),5),2157);
            }
            if(rest==200) {
                Port_TagTestCheck(BtlSeq_IsFighting() && !(Battle_GetWork()->flags&BATTLE_FLAG_DEMO),2150);
                Port_TagTestCheck(!BtlChar_IsFrozen(BtlChar_Get(0)),2151);
                Port_TagTestCheck(TagTeam_LockedOff(0) && TagTeam_HudTarget(0)<0,2155);
            }
            if(Port_TagChallengeSkipTest()) {
                BtlMovePose *p=(BtlMovePose *)&BtlChar_Get(0)->pose;
                if(rest==300)testRestPosition=p->pos;
                if(rest<=300 && rest>200)gPad[0].left[0]=0.9f;
                if(rest==200) {
                    extern s32 BtlChar_TestFlag(BtlMgrChr *,s32),BtlChars_IsTimeStopped(void);
                    Port_TagMenuTrace(83,BtlChar_Get(0)->pad,BtlChar_Get(0)->injectOn,TagTeam_Player(0),Battle_GetWork()->flags,BtlChars_IsTimeStopped());
                    Port_TagMenuTrace(82,BtlChar_TestFlag(BtlChar_Get(0),3),BtlChar_TestFlag(BtlChar_Get(0),0x136),BtlChar_TestFlag(BtlChar_Get(0),0x11E),BtlChar_TestFlag(BtlChar_Get(0),0x11F),BtlChar_TestFlag(BtlChar_Get(0),0xB1));
                    f32 x=p->pos.x-testRestPosition.x,z=p->pos.z-testRestPosition.z;
                    Port_TagTestCheck(x*x+z*z>1.0f,2152);
                }
                if(wave>0 && BtlSeq_GetState()==BTL_SEQ_INTRO_TALK) {
                    if(!testIntroAge)testIntroPosition=p->pos;
                    else Port_TagTestCheck(p->pos.x==testIntroPosition.x && p->pos.y==testIntroPosition.y &&
                        p->pos.z==testIntroPosition.z && BtlChar_IsFrozen(BtlChar_Get(0)),2153);
                    if(++testIntroAge==30)gPad[0].gamePressed|=0x1000;
                }
                if(wave>0 && BtlSeq_IsFighting() && !rest && testAge==0)
                    Port_TagTestCheck(testIntroAge==30,2154);
            }
            if(!rest && BtlSeq_IsFighting() && ++testAge==240) {
                const TagWave *current=&preset()->waves[wave];
                for(i=0;i<enemyCount;i++) {
                    s32 actor=enemies[i];BattleSide *side=TagTeam_Side(actor);
                    Port_TagTestCheck(TagChallenge_Hidden(actor)==(i>=current->count),2100+i);
                    if(i<current->count) {
                        Port_TagTestCheck(side->form.chara==current->fighters[i].chara,2110+i);
                        Port_TagTestCheck(BtlObj_Get(side->objId)->active!=0,2120+i);
                        Port_TagTestCheck(side->members[0].cpuLevel==CpuLevel_FromSetting(current->level),2130+i);
                    }
                }
                if(testCompleted && wave==0)Port_TagTestCheck(1,99);
                if(wave+1==preset()->count) {
                    if(!Port_TagChallengeFinishTest())Port_TagTestCheck(1,99);
                    testCompleted=1;
                }
                for(i=0;i<enemyCount;i++)BtlMember_GetActiveGauge(BtlChar_Get(enemies[i]))->health=0;
                if(testCompleted) {
                    Port_TagTestCheck(TagTeam_CheckEnd(0),2160);
                    return; /* Let the original result menu offer Fight Again. */
                }
                Port_TagTestCheck(!TagTeam_CheckEnd(0),2140+wave);
            }
        }
    }
    /* ResetAll is also called at Ready/Fight, so enforce presence each frame. */
    for(i=0;i<enemyCount;i++)if(TagChallenge_Hidden(enemies[i])) {
        BtlMember_GetActiveGauge(BtlChar_Get(enemies[i]))->health=0;
        if(TagChallenge_ModelHidden(enemies[i]))BtlObj_Get(BattleSide_GetObjId(enemies[i]))->active=0;
    }
    if(!rest || (Battle_GetWork()->flags&BATTLE_FLAG_PAUSE))return;
    if(departureTicks>0) {
        extern s32 Port_TagChallengeTest(void);extern void Port_TagTestCheck(s32,s32);
        if(Port_TagChallengeTest() && (departureTicks==90 || departureTicks==1)) {
            const TagWave *previous=&preset()->waves[wave-1];
            Port_TagTestCheck(loading<0 && nextEnemy==0 && rest==360+departureTicks,2170);
            for(i=0;i<enemyCount;i++) {
                s32 actor=enemies[i],visible=(departing&(1u<<actor))!=0;
                Port_TagTestCheck(TagChallenge_Hidden(actor) && !TagTeam_IsAlive(actor),2171);
                Port_TagTestCheck((BtlObj_Get(BattleSide_GetObjId(actor))->active!=0)==visible,2172);
                if(visible)Port_TagTestCheck(TagTeam_Side(actor)->form.chara==previous->fighters[i].chara,2173);
            }
        }
        departureTicks--;rest--;
        if(departureTicks)return;
        /* Hide before freeing/rebinding any model or voice resource. */
        for(i=0;i<enemyCount;i++)BtlObj_Get(BattleSide_GetObjId(enemies[i]))->active=0;
        departing=0;
        return;
    }
    w=&preset()->waves[wave];
    if(loading>=0) {
        if(BtlJob_GetFreeCount()!=BTL_JOB_COUNT)return;
        finish_enemy(loading);loading=-1;nextEnemy++;
    }
    if(nextEnemy<w->count) {
        if(BtlJob_GetFreeCount()==BTL_JOB_COUNT)begin_enemy(nextEnemy);
    }
    if(rest>1)rest--;
    if(rest==1 && nextEnemy>=w->count && loading<0) {
        /* Players can move during the rest, so choose the final safe positions
           now rather than using wherever they stood while each model loaded. */
        for(i=0;i<w->count;i++)place_enemy(enemies[i],i);
        hidden=0;
        for(i=0;i<enemyCount;i++) {
            if(i>=w->count)hidden|=1u<<enemies[i];
            else {
                BtlMgrGauge *g=BtlMember_GetActiveGauge(BtlChar_Get(enemies[i]));g->health=g->healthMax;
                BtlObj_Get(BattleSide_GetObjId(enemies[i]))->active=1;
            }
        }
        TagRevive_Reset();rest=0;
        BtlSeq_StartWaveIntro(enemies,w->count);
        Port_TagMenuTrace(86,wave,w->count,0,0,0);
    }
}

/* Count damage actually removed by the combat engine, not heals, model swaps
   or fusion health changes. The summary belongs to this run, including retries. */
void TagChallenge_Damage(s32 actor,s32 amount) {
    if(live && !rest && !resultPending && wave<WAVE_STATS_MAX && TagTeam_Team(actor)==0 && amount>0)
        waveDamage[wave]+=amount;
}
s32 TagChallenge_ResultVisible(void) {return live && resultVisible;}
s32 TagChallenge_ResultInput(s32 pad) {
    s32 pages=(completed+7)/8;
    if(!live || !resultPending || !(BattleResult_GetFlags()&1))return 0;
    if(!resultVisible) {resultVisible=1;return 1;}
    if(gPad[pad].gamePressed&PADG_CROSS) {resultPending=resultVisible=0;Snd_PlaySe(1,1);return 1;}
    if(gPad[pad].gameRepeat&(PADG_LEFT|PADG_UP))resultPage=(resultPage+pages-1)%pages;
    if(gPad[pad].gameRepeat&(PADG_RIGHT|PADG_DOWN))resultPage=(resultPage+1)%pages;
    return 1;
}
void TagChallenge_Draw(void) {
    const TagGauntlet *g;char text[160];s32 i,total=0,damage=0;
    if(!live)return;
    g=preset();
    if(resultVisible) {
        Ps_PageSet();Ps_SetShift(0);Ps_Align(1);
        Ps_Rect(14,24,498,424,Ps_Rgba(20,16,41,126),Ps_Rgba(28,28,60,126));
        Ps_Heading(256,34,"GAUNTLET CLEARED",1.6f,460,128);
        Ps_Text(256,76,g->name,0.95f,460,3);
        for(i=0;i<completed && i<WAVE_STATS_MAX;i++){total+=waveTicks[i];damage+=waveDamage[i];}
        sprintf(text,Port_TagText("Total time %d:%02d    Damage taken %d"),total/1800,total/30%60,damage);
        Ps_Text(256,108,text,0.8f,455,2);
        Ps_Align(0);Ps_Text(32,145,"WAVE",0.65f,230,3);
        Ps_Text(306,145,"TIME",0.65f,70,3);Ps_Text(389,145,"DAMAGE",0.65f,100,3);
        for(i=resultPage*8;i<completed && i<resultPage*8+8 && i<WAVE_STATS_MAX;i++) {
            f32 y=173+25*(i-resultPage*8);
            sprintf(text,"%d. %s",i+1,g->waves[i].name);Ps_Text(32,y,text,0.7f,266,2);
            sprintf(text,"%d:%02d",waveTicks[i]/1800,waveTicks[i]/30%60);Ps_Text(306,y,text,0.75f,74,2);
            sprintf(text,"%d",waveDamage[i]);Ps_Text(389,y,text,0.75f,97,2);
        }
        Ps_Align(1);sprintf(text,Port_TagText("Page %d / %d    Directions - More    Cross - Continue"),resultPage+1,(completed+7)/8);
        Ps_Text(256,393,text,0.68f,460,3);Font_FlushAll();return;
    }
    if(!bannerTicks || (Battle_GetWork()->flags&BATTLE_FLAG_PAUSE))return;
    Ps_PageSet();Ps_SetShift(0);Ps_Align(1);
    if(bannerKind==2)sprintf(text,Port_TagText("ROUND %d OVER"),completed);
    else sprintf(text,Port_TagText("WAVE %d - %s"),wave+1,g->waves[wave].name);
    Ps_TextExOuter(256,55,text,0.65f,340,Ps_Rgba(255,244,194,128),
        Ps_Rgba(255,206,82,128),Ps_Rgba(0,0,0,128),Ps_Rgba(0,0,0,128),0.8f);
    Font_FlushAll();
}
