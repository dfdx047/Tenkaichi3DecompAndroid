#include "common.h"
#include "battle/tag_lobby.h"
#include "battle/battle_setup.h"
#include "battle/tag_stage.h"
#include "battle/tag_team.h"
#include "battle/tag_position.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_char_member.h"
#include "battle/btl_obj.h"
#include "battle/btl_seq.h"
#include "battle/btl_cam.h"
#include "battle/stg_a.h"
#include "menu/port_screen.h"
#include "sys/pad.h"
#include "sys/gfxm_e.h"
#include "sys/job.h"
#include "battle/battle_work.h"
#include "battle/tag_fusion.h"
#include "../../port/src/gs/tag_net.h"

extern BtlMgrChr *BtlChar_Get(s32);
extern s32 Port_TagLobbyTest(void),CpuLevel_FromSetting(u32);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32),BtlChar_PoseToObj(BtlMgrChr *,s32);
extern void BtlChar_ClearFlag(BtlMgrChr *,s32),BtlAct_ClearQueue(BtlMgrChr *),BtlAct_Request(BtlMgrChr *,s32);
extern void BtlPartner_Release(BtlMgrChr *),BtlAiMgr_ResetSide(s32);
extern s32 BtlChar_TestFlag(BtlMgrChr *,s32);
extern void Port_TagTestCheck(s32,s32);

static int practice,positioned,testTicks;
static TlRoom room;
static TlPractice world;
static Vec4 homes[TL_ACTORS],rotations[TL_ACTORS];
static BtlMgrGauge fresh[TL_ACTORS];
static unsigned appliedEvent;
static int editor=-1,reloading=-1,pickDone;
static unsigned editRequests,rejoinReloads;
static int silentReload,editDenied[TL_MEMBERS];
static TlLoadout replacement;
static Vec4 editPosition,editRotation;
extern void TeamSel_PracticeStart(TlLoadout *),TeamSel_PracticeDraw(s32),TeamSel_PracticeEnd(void);
extern s32 TeamSel_PracticeUpdate(s32,TlLoadout *);
int TagLobby_Practice(void) {return practice;}
int TagLobby_IsSelecting(int member) {return practice && editor==member && !silentReload;}
void TagLobby_EnterPractice(const TlRoom *r) {room=*r;practice=1;positioned=testTicks=0;appliedEvent=0;editor=reloading=-1;pickDone=0;editRequests=rejoinReloads=0;silentReload=0;memset(editDenied,0,sizeof(editDenied));}
void TagLobby_LeavePractice(void) {if(editor>=0)TeamSel_PracticeEnd();editor=reloading=-1;practice=0;positioned=0;}
int TagLobby_Editing(int i) {return practice && (unsigned)i<TL_ACTORS && world.actors[i].editing;}
int TagLobby_LocalMenu(void) {
    TagPracticeUi ui;if(!TagNet_IsPractice())return 0;
    TagNet_PracticeUi((u32)&ui);return ui.visible || editor==TagNet_Role();
}
const char *TagLobby_Prompt(void) {
    static char text[112];int p=TagNet_Role(),a=p-1,i;
    if(!practice || p<1 || p>=TL_MEMBERS || editor==p || !positioned)return "";
    if(world.actors[a].recovering && !Port_TagOption(TAG_OPT_PRACTICE_RECOVERY_TEXT))return "";
    if(!world.actors[a].recovering && !Port_TagOption(TAG_OPT_PRACTICE_PROMPTS))return "";
    if(editDenied[p])return "Finish your challenge before changing fighters.";
    if(editRequests&(1u<<p))return "Waiting for the character selector...";
    if(world.actors[a].recovering){sprintf(text,Port_TagText("Back on your feet in %d"),(world.actors[a].recovering+29)/30);return text;}
    if(world.actors[a].opponent>=0)return "Start - Practice menu";
    for(i=0;i<9;i++)if(i!=a && world.actors[i].enabled) {
        Vec4 x=((BtlMovePose *)&BtlChar_Get(a)->pose)->pos,y=((BtlMovePose *)&BtlChar_Get(i)->pose)->pos;
        float dx=x.x-y.x,dy=x.y-y.y,dz=x.z-y.z;
        if(dx*dx+dy*dy+dz*dz<3600) {
            sprintf(text,world.actors[i].request==a?Port_TagText("P%d challenges you - Square to accept"):world.actors[a].request==i?Port_TagText("Waiting for P%d to accept"):Port_TagText("Square - Challenge P%d"),i+2);return text;
        }
    }
    return "Start - Practice menu";
}
static BtlMgrGauge *gauge(int i) {return (BtlMgrGauge *)BtlMember_GetActiveGauge((BtlMemberChr *)BtlChar_Get(i));}
int TagLobby_Hidden(int i) {return practice && ((unsigned)i>=TL_ACTORS || !world.actors[i].enabled || reloading==i || (rejoinReloads&(1u<<i)));}
int TagLobby_CanContact(int a,int b) {return !practice || TlPractice_CanHit(&world,a,b);}
void TagLobby_Contact(int a,int b) {if(practice)TlPractice_Contact(&world,a,b);}
int TagLobby_CpuActive(int a) {return !practice || TlPractice_CpuActive(&world,a);}
const char *TagLobby_Label(int a) {
    extern const char *CpuLevel_SettingName(s32);
    if(!practice || a<TL_MEMBERS-1 || a>=TL_ACTORS)return "";
    return a==TL_MEMBERS-1?"Training Dummy":CpuLevel_SettingName(a-TL_MEMBERS);
}
int TagLobby_Enemy(int a,int b) {
    if(!practice || a==b || (unsigned)a>=TL_ACTORS || (unsigned)b>=TL_ACTORS)return 0;
    if(!world.actors[a].enabled || !world.actors[b].enabled)return 0;
    return world.actors[a].opponent==b || (world.actors[a].member>0 && world.actors[b].member<0 && world.actors[b].opponent<0);
}
void TagLobby_Reset(void) {
    if(!practice)return;
    TlPractice_Init(&world,&room,40000);positioned=testTicks=0;
}
int TagLobby_BuildSides(BattleSide *sides,int *teams,int *nativeSides,int *assignment) {
    int i,dummy=PortGame_IsBT4()?-1:79;
    if(!practice)return 0;
    Battle_GetSetup()->rule.stage=PortGame_IsBT4()?24:5;
    Battle_GetSetup()->rule.timeLimit=0;
    for(i=0;dummy<0 && i<PortGame_CharacterCount();i++) {
        const char *name=PortGame_CharacterName(i);
        if(name && (!strcmp(name,"Saibamen") || !strcmp(name,"Saibaman"))){dummy=i;break;}
    }
    if(dummy<0){extern void Port_TagTeamFatal(const char *);Port_TagTeamFatal("The practice dummy was not found in this disc profile.");}
    for(i=0;i<TL_ACTORS;i++) {
        BattleSide *dst=&sides[i];TlLoadout *f=i<9?&room.members[i+1].loadout:NULL;
        BattleItemSet items;int k;
        *dst=Battle_GetSetup()->sides[i&1];memset(dst->members,0,sizeof(dst->members));
        for(k=0;k<8;k++)items.id[k]=f?f->items[k]:0;
        BtlMember_Init(&dst->members[0],f?f->chara:dummy,f?f->costume:0,0,CpuLevel_FromSetting(i<10?0:i-10),100.0f,&items);
        dst->memberCount=1;dst->lead=0;
        dst->startForm.chara=dst->members[0].chara;dst->startForm.costume=dst->members[0].costume;dst->startForm.variant=0;
        dst->form=dst->startForm;dst->objId=dst->modelSlot=-1;
        dst->control=i<9?BTL_CONTROL_PAD:BTL_CONTROL_CPU;dst->pad=i<9?i+1:0;
        teams[i]=i;nativeSides[i]=i&1;assignment[i]=i<9?i+1:-1;
    }
    return TL_ACTORS;
}
static void reset_actor(int i) {
    BtlMgrChr *c=BtlChar_Get(i);
    BtlPartner_Release(c);
    /* Clear queued damage as well as HP: a delayed projectile from the last
       challenge must not immediately knock the newly recovered actor down. */
    memset(&((BtlMemberChr *)c)->queue,0,sizeof(((BtlMemberChr *)c)->queue));
    *gauge(i)=fresh[i];gauge(i)->lowHealth=gauge(i)->lowHealthIdle=0;
    BtlChar_ClearFlag(c,7);BtlChar_ClearFlag(c,0x137);BtlChar_ClearFlag(c,0x96);BtlChar_ClearFlag(c,0xE7);
    c->freeze=c->freezeNext=c->freezeDelay=0;
    BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
    /* Dummies always return to their labelled station. Players recover where
       they fell unless the terrain changed or their position became unsafe. */
    if(world.actors[i].member<0)BtlChar_Place(c,&homes[i],&rotations[i],BtlStage_FindZoneNear(-1,&homes[i]));
    else {
        Vec4 p=((BtlMovePose *)&c->pose)->pos,r=((BtlMovePose *)&c->pose)->rot;
        if(!TagPosition_SafeSpot(i,&p))p=homes[i];
        BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));
    }
    BtlChar_PoseToObj(c,1);BtlAiMgr_ResetSide(i);
}
static void finish_editor(void) {
    if(editor<0)return;
    if(!silentReload)TeamSel_PracticeEnd();TlPractice_Edit(&world,editor-1,0);editor=-1;pickDone=0;silentReload=0;
}
static void load_fighter(void) {
    int actor=editor-1,k;BattleSide *s=TagTeam_Side(actor);BattleMember *m=&s->members[0];
    void *a=m->buf[0],*b=m->buf[1];BattleItemSet items;BtlJob *job;
    extern void BtlScene_FreeChar(s32);
    editPosition=((BtlMovePose *)&BtlChar_Get(actor)->pose)->pos;editRotation=((BtlMovePose *)&BtlChar_Get(actor)->pose)->rot;
    for(k=0;k<8;k++)items.id[k]=replacement.items[k];
    BtlPartner_Release(BtlChar_Get(actor));TagFusion_Remove(actor);BtlScene_FreeChar(actor);
    BtlMember_Init(m,replacement.chara,replacement.costume,0,CpuLevel_FromSetting(0),100,&items);
    m->buf[0]=a;m->buf[1]=b;m->data=b;
    s->form.chara=replacement.chara;s->form.costume=replacement.costume;s->form.variant=0;s->startForm=s->form;
    job=BtlJob_Alloc();memset(job,0,sizeof(*job));job->step=BtlLoad_StepChara;job->kind=BTL_JOB_KIND_RESTART;
    job->side=actor;job->chara=job->animChara=job->unk1C=job->voiceChara=replacement.chara;
    job->costume=replacement.costume;job->initial=1;Job_Push((Job *)job);reloading=actor;
}
static void update_editor(void) {
    TagPracticeEvent e;TlLoadout f;int result,i;
    while(TagNet_PracticeEvent(appliedEvent,(u32)&e)) {
        appliedEvent++;
        if(e.kind==1 && editor!=e.member)editRequests|=1u<<e.member;
        else if(e.kind==2 && editor==e.member && pickDone==1) {replacement=e.fighter;pickDone=2;}
    }
    for(i=1;i<TL_MEMBERS;i++)if(editDenied[i]>0)editDenied[i]--;
    if(editor<0 && BtlJob_GetFreeCount()==BTL_JOB_COUNT) {
        for(i=0;i<9;i++)if(rejoinReloads&(1u<<i)) {
            rejoinReloads&=~(1u<<i);if(!room.members[i+1].present)continue;
            editor=i+1;silentReload=1;pickDone=2;world.actors[i].editing=1;replacement=room.members[editor].loadout;break;
        }
        if(editor<0)for(i=1;i<TL_MEMBERS;i++)if(editRequests&(1u<<i)) {
            int k;editRequests&=~(1u<<i);if(!room.members[i].present)continue;
            if(TlPractice_Edit(&world,i-1,1)!=TL_OK){editDenied[i]=150;continue;}
            memset(&f,0,sizeof(f));f.chara=TagTeam_Side(i-1)->form.chara;f.costume=TagTeam_Side(i-1)->form.costume;
            for(k=0;k<8;k++)f.items[k]=TagTeam_Side(i-1)->members[0].items.id[k];
            editor=i;silentReload=0;pickDone=0;TeamSel_PracticeStart(&f);break;
        }
    }
    if(editor<0)return;
    if(reloading>=0) {
        if(BtlJob_GetFreeCount()!=BTL_JOB_COUNT)return;
        {
            extern void BtlChar_BindObject(BtlMgrChr *),BtlChar_Reset(BtlMgrChr *);
            BtlMgrChr *c=BtlChar_Get(reloading);int actor=reloading;
            memset(c,0,sizeof(*c));c->side=c->index=actor;c->pad=actor+1;BtlChar_BindObject(c);BtlChar_Reset(c);
            fresh[actor]=*gauge(actor);world.actors[actor].hp=gauge(actor)->health;world.actors[actor].maxHp=gauge(actor)->healthMax;
            reloading=-1;reset_actor(actor);
            if(!TagPosition_SafeSpot(actor,&editPosition))editPosition=homes[actor];
            BtlChar_Place(c,&editPosition,&editRotation,BtlStage_FindZoneNear(-1,&editPosition));BtlChar_PoseToObj(c,1);
            BtlObj_Get(BattleSide_GetObjId(actor))->active=1;
            {extern void TagNet_TestCheck(s32,s32),Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);Port_TagMenuTrace(96,2,editor,replacement.chara,replacement.costume,0);if(TagNet_TestCase()==10)TagNet_TestCheck(gauge(actor)->health==gauge(actor)->healthMax,2510);}
            finish_editor();
        }
        return;
    }
    if(!room.members[editor].present){finish_editor();return;}
    if(pickDone==2 && BtlJob_GetFreeCount()==BTL_JOB_COUNT){load_fighter();return;}
    if(pickDone)return;
    result=TeamSel_PracticeUpdate(editor,&f);
    if(result<0){if(TagNet_TestCase()==12)TagNet_TestCheck(1,2512);finish_editor();return;}
    if(result>0) {
        replacement=f;
        pickDone=1;TagNet_PracticeLoadout(editor,(u32)&replacement);
    }
}
void TagLobby_Update(void) {
    int i;
    if(!practice || !BtlSeq_IsFighting())return;
    if(TagNet_PracticeEnding() && BtlJob_GetFreeCount()==BTL_JOB_COUNT){finish_editor();BattleResult_Set(BTL_RESULT_ABORT,0);return;}
    if(TagNet_IsPractice()) {
        TlRoom updated;unsigned newcomers=0;TagNet_PracticeState((u32)&updated);
        for(i=1;i<TL_MEMBERS;i++)if(updated.members[i].present && (!room.members[i].present || updated.members[i].generation!=room.members[i].generation))newcomers|=1u<<(i-1);
        room=updated;TlPractice_Refresh(&world,&room,40000);
        for(i=0;i<TL_MEMBERS-1;i++)if((newcomers&(1u<<i)) && positioned) {
            BattleSide *side=TagTeam_Side(i);TlLoadout *f=&room.members[i+1].loadout;int k,changed=side->form.chara!=f->chara || side->form.costume!=f->costume;
            for(k=0;k<8;k++)if(side->members[0].items.id[k]!=f->items[k])changed=1;
            world.wins[i+1]=room.members[i+1].wins;world.losses[i+1]=room.members[i+1].losses;world.draws[i+1]=room.members[i+1].draws;
            if(changed)rejoinReloads|=1u<<i;
            BtlObj_Get(BattleSide_GetObjId(i))->active=1;reset_actor(i);
        }
        update_editor();
    }
    for(i=0;i<TL_ACTORS;i++)if(TagLobby_Hidden(i))BtlObj_Get(BattleSide_GetObjId(i))->active=0;
    if(!positioned) {
        for(i=0;i<TL_ACTORS;i++) {
            BtlMovePose *p=(BtlMovePose *)&BtlChar_Get(i)->pose;
            homes[i]=p->pos;rotations[i]=p->rot;fresh[i]=*gauge(i);
            world.actors[i].hp=gauge(i)->health;world.actors[i].maxHp=gauge(i)->healthMax;
        }
        positioned=1;
    }
    for(i=0;i<TL_ACTORS;i++) {
        int p=TagTeam_InputPlayer(i),j,best=-1;float nearest=60*60;
        if(p<0 || !(gPad[p].pressed&PAD_SQUARE) || world.actors[i].member<1)continue;
        for(j=0;j<9;j++)if(i!=j && world.actors[j].enabled) {
            Vec4 a=((BtlMovePose *)&BtlChar_Get(i)->pose)->pos,b=((BtlMovePose *)&BtlChar_Get(j)->pose)->pos;
            float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z,d=x*x+y*y+z*z;
            if(d<nearest){nearest=d;best=j;}
        }
        if(best>=0)TlPractice_Challenge(&world,i,best,1);
    }
}
void TagLobby_AfterUpdate(void) {
    int i;unsigned resets;
    if(!practice || !positioned || !BtlSeq_IsFighting())return;
    for(i=0;i<TL_ACTORS;i++)world.actors[i].hp=gauge(i)->health;
    resets=TlPractice_Tick(&world);
    for(i=0;i<TL_ACTORS;i++)if(resets&(1u<<i))reset_actor(i);
    if(TagNet_IsPractice())TagNet_PracticeScores((u32)&world);
}
void TagLobby_Draw(void) {
    if(practice && editor>=0 && !silentReload){Ps_PageSet();Ps_SetShift(0);TeamSel_PracticeDraw(TagNet_Role()==editor);}
}
unsigned long long TagLobby_Digest(void) {
    const unsigned char *p=(const unsigned char *)&world;unsigned i;
    unsigned long long h=14695981039346656037ull;
    if(!practice)return 0;
    for(i=0;i<sizeof(world);i++)h=(h^p[i])*1099511628211ull;
    return h;
}
void TagLobby_TestSetup(void) {
    TlRoom r;TlLoadout f={0,0,{0}};int i;
    if(!Port_TagLobbyTest())return;
    TlRoom_Init(&r,&f);for(i=1;i<10;i++)TlRoom_Join(&r,&f);
    TagLobby_EnterPractice(&r);
    Battle_GetSetup()->rule.timeLimit=0;
}
void TagLobby_TestTick(void) {
    int i;
    if(!Port_TagLobbyTest() || !positioned || !BtlSeq_IsFighting())return;
    ++testTicks;
    if(testTicks==5) {
        extern s32 Battle_GetStage(void);
        Port_TagTestCheck(TagTeam_Count()==15,2400);
        Port_TagTestCheck(TagTeam_ControlActor(0)==-1,2401);
        Port_TagTestCheck(Battle_GetStage()==(PortGame_IsBT4()?24:5) && TagStage_Scale()==2.0f,2414);
        for(i=0;i<TL_ACTORS;i++) {
            Vec4 p=((BtlMovePose *)&BtlChar_Get(i)->pose)->pos;
            int j;
            Port_TagTestCheck(gauge(i)->health==gauge(i)->healthMax && gauge(i)->health>=10000,2415);
            Port_TagTestCheck(((ObjMdlObj *)BtlObj_Get(BattleSide_GetObjId(i)))->shadow!=NULL,2416);
            Port_TagTestCheck(TagPosition_SafeSpot(i,&p),2417);
            for(j=0;j<i;j++) {
                Vec4 q=((BtlMovePose *)&BtlChar_Get(j)->pose)->pos;
                float x=p.x-q.x,z=p.z-q.z;
                Port_TagTestCheck(x*x+z*z>45*45,2418);
            }
        }
        for(i=9;i<15;i++) {
            Vec4 p={0},r={0};
            const char *labels3[]={"Training Dummy","Very Weak","Weak","Normal","Strong","Very Strong"};
            const char *labels4[]={"Training Dummy","Easy","Normal","Hard","Very Hard","Z Hard"};
            const char *name=PortGame_CharacterName(TagTeam_Side(i)->form.chara);
            Port_TagTestCheck(PortGame_IsBT4()?(name && (!strcmp(name,"Saibamen")||!strcmp(name,"Saibaman"))):TagTeam_Side(i)->form.chara==79,2402);
            Port_TagTestCheck(!TagLobby_CpuActive(i),2403);
            Port_TagTestCheck(!strcmp(TagLobby_Label(i),(PortGame_IsBT4()?labels4:labels3)[i-9]),2419);
            Port_TagTestCheck(TagPosition_FreeForAllStart(i,&p,&r),2420);
            if(i>9) {
                Vec4 first=homes[9],step=homes[10];
                float dx=step.x-first.x,dz=step.z-first.z;
                float x=homes[i].x-first.x-dx*(i-9),z=homes[i].z-first.z-dz*(i-9);
                Port_TagTestCheck(x*x+z*z<1 && __builtin_fabsf(dx*dx+dz*dz-10000)<2 &&
                    __builtin_fabsf(homes[i].y-first.y)<=8 && __builtin_fabsf(rotations[i].y-rotations[9].y)<.01f,2421);
            }
        }
        Port_TagTestCheck(!TagTeam_CanContact(0,1),2404);
        Port_TagTestCheck(TlPractice_Challenge(&world,0,1,1)==TL_OK,2405);
        Port_TagTestCheck(TlPractice_Challenge(&world,1,0,1)==TL_OK && TagTeam_CanContact(0,1),2406);
        gauge(1)->health=0;
    }
    if(testTicks==10) {
        Port_TagTestCheck(world.wins[1]==1 && world.losses[2]==1,2407);
        Port_TagTestCheck(!TagTeam_CanContact(0,1) && gauge(1)->health==0,2408);
    }
    if(testTicks==100) {
        Port_TagTestCheck(gauge(1)->health==gauge(1)->healthMax && BtlChar_Get(1)->action!=0x36,2409);
        Port_TagTestCheck(!TagTeam_CheckEnd(0),2410);
        Port_TagTestCheck(TlPractice_Contact(&world,0,14) && TagLobby_CpuActive(14),2411);
    }
    if(testTicks==110)gauge(14)->health=0;
    if(testTicks==210) {
        Vec4 p=((BtlMovePose *)&BtlChar_Get(14)->pose)->pos;
        float x=p.x-homes[14].x,z=p.z-homes[14].z;
        Port_TagTestCheck(gauge(14)->health==gauge(14)->healthMax && !TagLobby_CpuActive(14),2412);
        Port_TagTestCheck(world.wins[1]==1,2413);
        Port_TagTestCheck(x*x+z*z<1,2422);
        Port_TagTestCheck(1,99);
    }
}

/* Frame the entire row for the opt-in runtime capture. Never changes a real
   guest's follow camera or any normal match camera. */
s32 TagLobby_TestCamera(void) {
    extern f32 sqrtf(f32),atan2f(f32,f32);
    BtlCamView *v;Vec4 look,eye;float dx,dz,len,x,y,z;
    if(!Port_TagLobbyTest() || !practice || !positioned)return 0;
    look=homes[9];look.x=(look.x+homes[14].x)*.5f;look.z=(look.z+homes[14].z)*.5f;look.y-=12;
    dx=homes[14].x-homes[9].x;dz=homes[14].z-homes[9].z;len=sqrtf(dx*dx+dz*dz);
    if(len<1)return 0;
    if(look.x*dz-look.z*dx<0){dx=-dx;dz=-dz;}
    eye=look;eye.x+=dz/len*460;eye.z-=dx/len*460;eye.y-=75;
    if(Port_TagLobbyTest()==2) {
        look=homes[11];look.y-=6;
        eye=look;eye.x+=dz/len*25;eye.z-=dx/len*25;eye.y-=3;
    }
    BtlCam_SelectView(0);BtlCam_SetLayout(0,0);v=BtlCam_GetNativeView(0);
    x=look.x-eye.x;y=look.y-eye.y;z=look.z-eye.z;
    v->pos=eye;v->rot=(Vec4){-atan2f(y,sqrtf(x*x+z*z)),atan2f(x,z),0,0};
    BtlCam_BuildView(v,1);return 1;
}
