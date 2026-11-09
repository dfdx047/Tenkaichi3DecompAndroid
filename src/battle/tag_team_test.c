/* Opt-in integration fixture: drives real combat, KO and recovery code. It is
 * dormant unless BT3_TAG_TEST_REVIVE is explicitly set by the test runner. */
#ifdef PORT
#include "common.h"
#include "battle/tag_team.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_seq.h"
#include "sys/adx.h"
extern s32 Port_TagReviveTest(void);
extern s32 Port_TagAudioTest(void);
extern s32 Port_TagClashTest(void);
extern s32 Port_TagControlsTest(void);
extern s32 Port_TagBeamTest(void);
extern s32 Port_TagCameraTest(void);
extern s32 Port_TagFusionTest(void);
extern s32 Port_TagGiantTest(void);
extern s32 Port_TagCpuTest(void);
extern s32 Port_TagStageTest(void);
extern s32 Port_TagCinemaTest(void),Port_TagResultTest(void);
extern void TagCinema_MatrixTick(s32);
extern void TagStage_TestTick(s32);
extern void TagCpu_TestTick(s32);
extern void TagGiant_TestTick(s32);
extern void TagFusion_TestTick(s32);
extern void TagCamera_TestTick(s32);
extern void TagBeam_TestTick(s32);
extern void TagControls_TestTick(s32);
extern void TagClash_TestTick(s32);
extern void Port_TagTestCheck(s32, s32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern s32 BtlMember_Damage(BtlMgrChr *,s32,s32);
extern void BtlAct_Request(BtlMgrChr *,s32), BtlAct_ClearQueue(BtlMgrChr *);
extern void BtlChar_SetHeldFlag(BtlMgrChr *,s32);
extern s32 TagRevive_Progress(s32);
extern s32 TagRevive_Nearby(s32);
extern s32 BtlColl_IsGuarding(BtlMgrChr *);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);
extern s32 BtlStage_FindZoneNear(s32,Vec4 *);

static void place(BtlMgrChr *chr,Vec4 pos) {
    Vec4 rot=((BtlMovePose *)&chr->pose)->rot;
    BtlChar_Place(chr,&pos,&rot,BtlStage_FindZoneNear(-1,&pos));
}

static void audio_check(void) {
    extern void BtlCharSnd_ClearRequests(void), BtlCharSnd_PlayRequests(void);
    extern void BtlCharSnd_PlayVoice(BtlMgrChr *,s32), BtlCharSnd_PlayOwn(BtlMgrChr *,s32);
    extern void BtlCharSnd_PlayCommon(BtlMgrChr *,s32), BtlCharSnd_PlayStream(BtlMgrChr *,s32);
    extern void Snd_Update(void), Snd_StopBank(s32), Snd_SetPause(s32,s32);
    extern void BtlScript_StopVoices(void);
    extern s32 Port_TagAudioProbe(s32,s32), Port_TagSampleProbe(s32);
    extern void Port_TagSampleProbeReset(void);
    s32 i,all=0, banks=4,count=TagTeam_Count(),last=count-1;
    for(i=0;i<count;i++) {
        all|=(1<<(4+i)) | (1<<(i<2?2+i:4+PORT_FIGHTER_CAPACITY+i-2));
        banks|=1<<(4+i);
    }
    BtlCharSnd_ClearRequests();
    Port_TagSampleProbeReset();
    for(i=0;i<count;i++) {
        BtlMgrChr *chr=BtlChar_Get(i);
        BtlCharSnd_PlayCommon(chr,0);
        /* More than the console's 32 commands in one ten-fighter frame. */
        BtlCharSnd_PlayCommon(chr,0);
        BtlCharSnd_PlayCommon(chr,0);
        BtlCharSnd_PlayOwn(chr,0);
        BtlCharSnd_PlayVoice(chr,0);
        /* A real, long ADX file also exercises each streamed-effect channel. */
        BtlCharSnd_PlayStream(chr,PortGame_VoiceLine(TagTeam_Side(i)->form.chara,0,0));
    }
    BtlCharSnd_PlayRequests();
    Snd_Update();
    Port_TagTestCheck(Port_TagAudioProbe(all,0),20);
    Port_TagTestCheck(Port_TagSampleProbe(banks),21);
    Adx_PauseSeVoice(); Snd_SetPause(banks,1);
    Port_TagTestCheck(Port_TagAudioProbe(all,1),22);
    Port_TagTestCheck(Port_TagAudioProbe(1,0),23); /* music keeps playing */
    Adx_ResumeSeVoice(); Snd_SetPause(banks,0);
    Port_TagTestCheck(Port_TagAudioProbe(all,0),24);
    Voice_Stop(last); StreamSe_Stop(last); Snd_StopBank(1<<(4+last));
    Port_TagTestCheck(Voice_IsStopped(last)&&StreamSe_IsStopped(last),25);
    Port_TagTestCheck(Port_TagAudioProbe(all&~((1<<(4+last))|(1<<(4+PORT_FIGHTER_CAPACITY+last-2))),0),26);
    BtlScript_StopVoices();
    for(i=0;i<count;i++) Port_TagTestCheck(Voice_IsStopped(i)&&StreamSe_IsStopped(i),30+i);
    Port_TagTestCheck(!Bgm_IsStopped(),34);
    Port_TagTestCheck(1,99);
}

static void check_start_spacing(void) {
        /* Check spacing before the fixture changes positions. */
        s32 a,b;
        extern f32 BtlCharApi_GetRadius(s32),BtlCharApi_GetHeight(s32);
        for(a=0;a<TagTeam_Count();a++) for(b=0;b<a;b++) {
            Vec4 *p=&((BtlMovePose *)&BtlChar_Get(a)->pose)->pos;
            Vec4 *q=&((BtlMovePose *)&BtlChar_Get(b)->pose)->pos;
            s32 ao=BattleSide_GetObjId(a),bo=BattleSide_GetObjId(b);
            f32 x=p->x-q->x,z=p->z-q->z,y=p->y-q->y;
            f32 r=BtlCharApi_GetRadius(ao)+BtlCharApi_GetRadius(bo);
            s32 apart=x*x+z*z>=r*r || y>=BtlCharApi_GetHeight(ao) || -y>=BtlCharApi_GetHeight(bo);
            if(!apart) {
                extern void Port_TagTestPositions(u32,u32);
                extern s32 BtlStage_GetStartPlace(s32,Vec4 *,Vec4 *,s32);
                Vec4 expectedA,expectedB,rotation;
                Port_TagTestPositions((u32)p,(u32)q);
                BtlStage_GetStartPlace(a,&expectedA,&rotation,0);
                BtlStage_GetStartPlace(b,&expectedB,&rotation,0);
                Port_TagTestPositions((u32)&expectedA,(u32)&expectedB);
            }
            Port_TagTestCheck(apart,200+a*10+b);
        }
}

/* Ready releases the CPUs before the Fight sequence begins. On short stages
   they can already reach an opponent before the audio fixture gets its first
   fighting tick, so check the ten initial positions before that release. */
void TagTeam_TestStartSpacing(void) {
    if(Port_TagAudioTest() && TagTeam_Active())check_start_spacing();
}

void TagTeam_TestTick(void) {
    static s32 tick, beforeMove, beforeBlock;
    BtlMgrChr *rescuer,*body;
    BtlMgrGauge *r,*g;
    BtlMovePose *rp,*bp;
    /* Online fixtures own their clock and completion state across rematches. */
    {extern s32 TagNet_Enabled(void);if(TagNet_Enabled())return;}
    {extern s32 Port_TagModeRushTest(void);extern void TagModeRush_TestTick(void);
     if(Port_TagModeRushTest()) {TagModeRush_TestTick();return;}}
    /* Ring-out coverage follows the actual finish and victory states too. */
    if(Port_TagControlsTest()==4) {
        static s32 ringTick;
        if(TagTeam_Active() && (BtlSeq_IsFighting() || ringTick)) TagControls_TestTick(++ringTick);
        return;
    }
    if((!Port_TagReviveTest()&&!Port_TagAudioTest()&&!Port_TagClashTest()&&!Port_TagControlsTest()&&!Port_TagBeamTest()&&!Port_TagCameraTest()&&!Port_TagFusionTest()&&!Port_TagGiantTest()&&!Port_TagCpuTest()&&!Port_TagStageTest()&&!Port_TagCinemaTest()&&!Port_TagResultTest()) || !TagTeam_Active() || !BtlSeq_IsFighting()) return;
    if(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) return;
    tick++;
    { extern s32 Port_TagShowcase(void);
      if(Port_TagShowcase()) {TagBeam_TestTick(tick);return;} }
    if(Port_TagCinemaTest()) {TagCinema_MatrixTick(tick);return;}
    if(Port_TagResultTest()) {
        if(tick==90) {
            s32 i,lose=Port_TagResultTest()==2?0:1;
            for(i=0;i<TagTeam_Count();i++) if(TagTeam_Team(i)==lose) {
                BtlMgrChr *c=BtlChar_Get(i);
                BtlMember_Damage(c,BtlMember_GetActiveGauge(c)->health+1000,0x1C3);
                BtlAct_ClearQueue(c);BtlAct_Request(c,0xD8);
            }
        }
        if(tick>180 && TagTeam_IsAlive(0)&&TagTeam_IsAlive(1)&&TagTeam_IsAlive(2)&&TagTeam_IsAlive(3)) {
            extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);
            Port_TagMenuTrace(92,TagTeam_Count(),TagTeam_ViewCount(),TagTeam_Team(0),TagTeam_Team(1),0);
        }
        return;
    }
    if(Port_TagStageTest()) {TagStage_TestTick(tick);return;}
    if(Port_TagCpuTest()) {TagCpu_TestTick(tick);return;}
    /* The catcher fixture places its actors below the arena itself; ordinary
       spawn spacing has separate coverage and is not its starting condition. */
    if(tick==1 && !Port_TagAudioTest() && Port_TagControlsTest()!=5)check_start_spacing();
    if(Port_TagGiantTest()) { TagGiant_TestTick(tick); return; }
    if(Port_TagCameraTest()) { TagCamera_TestTick(tick); return; }
    if(Port_TagFusionTest()) { TagFusion_TestTick(tick); return; }
    if(Port_TagBeamTest()) { TagBeam_TestTick(tick); return; }
    if(Port_TagControlsTest()) { TagControls_TestTick(tick); return; }
    if(Port_TagClashTest()) { TagClash_TestTick(tick); return; }
    if(Port_TagAudioTest()) { if(tick==30) audio_check(); return; }
    rescuer=BtlChar_Get(0); body=BtlChar_Get(2);
    r=BtlMember_GetActiveGauge(rescuer); g=BtlMember_GetActiveGauge(body);
    rp=(BtlMovePose *)&rescuer->pose; bp=(BtlMovePose *)&body->pose;
    if(tick==60) {
        Vec4 p=rp->pos; p.y=0; place(rescuer,p);
        p.x+=30; place(body,p);
        r->blast=400000;
        BtlMember_Damage(body,g->health+1000,0x1C3);
        Port_TagTestCheck(g->health==0,10);
        BtlAct_ClearQueue(body); BtlAct_Request(body,0xD8);
    }
    if(tick==65) {
        /* Exercise the real target/camera path while the revival meter fills. */
        s32 tries;
        for(tries=0;tries<TagTeam_Count() && TagTeam_Target(0)!=2;tries++)
            TagTeam_CycleTarget(0);
        Port_TagTestCheck(TagTeam_Target(0)==2,16);
    }
    if(tick==80) {
        extern void Port_TagTestState(s32,s32,s32,s32);
        extern void Port_TagTestPositions(u32,u32);
        Port_TagTestState(body->action,g->health,r->blast,TagRevive_Progress(2));
        Port_TagTestPositions((u32)rp,(u32)bp);
        Vec4 p=bp->pos;
        Port_TagTestCheck(TagRevive_Progress(2)>0,1);
        Port_TagTestCheck(TagRevive_Nearby(0)==2,17);
        p.x-=100; place(rescuer,p);
    }
    if(tick==82) {
        extern void Port_TagTestPositions(u32,u32);
        extern void Port_TagTestState(s32,s32,s32,s32);
        Port_TagTestPositions((u32)rp,(u32)bp);
        Port_TagTestState(body->action,g->health,r->blast,TagRevive_Progress(2));
        Port_TagTestCheck(TagRevive_Progress(2)==0,2);
        Port_TagTestCheck(TagRevive_Nearby(0)==-1,18);
    }
    if(tick==100) { Vec4 p=bp->pos; p.x-=30; place(rescuer,p); }
    if(tick==110) {
        Vec4 p=rp->pos; p.z+=5; beforeMove=TagRevive_Progress(2); place(rescuer,p);
    }
    if(tick==111) Port_TagTestCheck(TagRevive_Progress(2)>beforeMove,11);
    if(tick==120) BtlAct_Request(rescuer,0x38);
    if(tick==121) {
        Port_TagTestCheck(BtlColl_IsGuarding(rescuer),12);
        beforeBlock=TagRevive_Progress(2);
        BtlMember_Damage(rescuer,100,0x1C3);
    }
    if(tick==122) Port_TagTestCheck(TagRevive_Progress(2)>beforeBlock,13);
    if(tick==135) {
        Port_TagTestCheck(TagRevive_Progress(2)>0,3);
        Port_TagTestCheck(!BtlColl_IsGuarding(rescuer),14);
        BtlMember_Damage(rescuer,1000,0x1C3);
    }
    if(tick==136) Port_TagTestCheck(TagRevive_Progress(2)==0,4);
    if(tick==230) {
        Port_TagTestCheck(g->health==g->healthMax/4,5);
        Port_TagTestCheck(r->blast>=200000 && r->blast<300000,6);
        Port_TagTestCheck(body->action!=0xD8 && body->action!=0xD9 && body->freeze==0,7);
        Port_TagTestCheck(TagTeam_IsEnemy(0,TagTeam_Target(0)),17);
    }
    if(tick==320) Port_TagTestCheck(body->action==0xB,15);
    if(tick==350) { g->health=0; BtlChar_SetHeldFlag(body,7); }
    if(tick==460) { Port_TagTestCheck(g->health==0 && TagRevive_Progress(2)==0,8); Port_TagTestCheck(1,99); }
}
#endif
