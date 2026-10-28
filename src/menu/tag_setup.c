#ifdef PORT
#include "common.h"
#include "battle/tag_session.h"
#include "menu/port_screen.h"
#include "menu/tag_setup.h"
#include "menu/tag_controllers.h"
#include "battle/tag_options.h"
#include "sys/pad.h"
#include "menu/tbo_art.h"

/* This is a page of the original animated main menu. The roster is spatial:
   left and right are teams, each card is one fighter. No scrolling slot IDs. */
static s32 focus,checkedThisSetup,editing,editOriginal,editDelta;
static s32 tournament(void) {return TagSession_DraftMode()==TAG_MODE_TOURNAMENT;}
static s32 ffa(void) { return TagSession_DraftMode()==TAG_MODE_FREE_FOR_ALL; }
static s32 gauntlet(void) {return TagSession_DraftMode()==TAG_MODE_GAUNTLET;}
static s32 controls_card(void) {return TagSession_Players()+(gauntlet()?3:2);}
static f32 rules_y(void) {return gauntlet()?270:275;}
static f32 controls_y(void) {return gauntlet()?330:310;}
static f32 buttons_y(void) {return gauntlet()?360:342;}
void TagSetup_Reset(void) { Port_TagControllersCancel(); focus=!TagSession_Players()&&tournament()?1:0; checkedThisSetup=0; editing=0; }
static s32 visible(s32 id) {
    s32 players=TagSession_Players();
    if(id==players)return !tournament();
    if(id==players+1)return tournament();
    return 1;
}
static void card_center(s32 id,f32 *x,f32 *y) {
    s32 p,n=0,players=TagSession_Players(),team;
    if(id<players) {
        if(tournament()) {*x=256;*y=157+31*id;return;}
        team=TagSession_PlayerTeam(id);
        for(p=0;p<id;p++) if(TagSession_PlayerTeam(p)==team) n++;
        *x=139+234*team; *y=157+31*n;
    } else if(id<=players+1) {*x=256;*y=rules_y()+14;}
    else if(gauntlet() && id==players+2) {*x=256;*y=313;}
    else if(id==controls_card()) {*x=256;*y=controls_y()+13;}
    else {*x=id==controls_card()+1?318:86;*y=buttons_y()+17;}
}
static void navigate(u32 nav) {
    s32 i,best=focus,players=TagSession_Players();
    f32 x,y,bestScore=100000;
    card_center(focus,&x,&y);
    for(i=0;i<controls_card()+3;i++) {
        f32 nx,ny,along,across,score;
        if(i==focus || !visible(i) || (!players&&i==controls_card())) continue;
        card_center(i,&nx,&ny);
        if(nav&(PADG_LEFT|PADG_RIGHT)) {
            along=(nx-x)*((nav&PADG_RIGHT)?1:-1); across=__builtin_fabsf(ny-y);
            if(across>20) continue;
        } else {along=(ny-y)*((nav&PADG_DOWN)?1:-1);across=__builtin_fabsf(nx-x);}
        if(along<1) continue;
        /* A full-width rule sits below both columns. Prefer the next row
           over jumping past it to a lower button whose centre is closer. */
        score=(nav&(PADG_LEFT|PADG_RIGHT))?along+across*2:along*4+across;
        if(score<bestScore) {best=i;bestScore=score;}
    }
    focus=best;
}
static s32 controllers_ready(void) {
    return Port_TagControllersReady(TagSession_Players()) &&
        (checkedThisSetup || Port_TagOption(TAG_OPT_KEEP_CHECKINS));
}
static void plate(f32 x,f32 y,f32 w,f32 h,s32 selected,s32 red) {
    u32 edge=Ps_Rgba(69,30,92,128);
    u32 top=Ps_Rgba(red?225:165,red?177:223,red?220:246,118);
    u32 bottom=Ps_Rgba(red?166:83,red?101:166,red?186:213,118);
    if(selected) {
        edge=Ps_Rgba(255,214,57,128);
        top=Ps_Rgba(255,246,176,128); bottom=Ps_Rgba(247,183,70,128);
    }
    { u32 shade=Ps_Rgba(0,0,12,52); /* a soft shadow under the plate: it stands off the window */
      Ps_Quad(x+10,y+3,x+w+3,y+3,x+3,y+h+3,x+w-4,y+h+3,shade,shade,shade,shade); }
    Ps_Quad(x+7,y,x+w,y,x,y+h,x+w-7,y+h,edge,edge,edge,edge);
    Ps_Quad(x+8,y+2,x+w-3,y+2,x+3,y+h-2,x+w-8,y+h-2,top,top,bottom,bottom);
}
void TagSetup_Draw(void) {
    s32 p,t,n[4]={0,0,0,0},players=TagSession_Players();
    f32 x,y;
    char text[96],heading[64];
    Ps_PageSet(); Ps_SetShift(0); Ps_Align(1);
    if(Port_TagControllersActive()) {
        Ps_Heading(256,25,"CONTROLLER SETUP",1.55f,460,128);
        Ps_Panel(TBO_KIT_WINDOW,18,126,494,141+TagSession_Players()*42,0x80);
        Ps_Text(256,80,"Press START in player order, using your own controller.",0.8f,470,2);
        Ps_Text(256,102,"SELECT leaves a seat. A controller can join only once.",0.7f,470,2);
        for(p=0;p<TagSession_Players();p++) {
            char device[72];
            Port_TagControllersName(p,(u32)device,sizeof(device));
            plate(30,139+p*42,452,37,0,p&1);
            sprintf(text,"P%d",p+1);
            Ps_Align(0); Ps_Text(44,143+p*42,text,1.0f,45,2);
            Ps_Text(101,148+p*42,device,0.75f,365,2);
        }
        Ps_Align(1);
        Ps_Text(256,333,Port_TagControllersReady(TagSession_Players())?
            "Everyone is ready. Player 1: press Cross to confirm.":
            "Waiting for each player to join...",0.8f,470,2);
#ifdef __ANDROID__
        Ps_Text(256,362,"Touch screen: START joins, SELECT leaves.",0.65f,475,0);
#else
        Ps_Text(256,362,"Keyboard players use their START binding. Edit bindings with F1.",0.65f,475,0);
#endif
        Ps_Align(0); Ps_PromptsClear();
        if(Port_TagControllersReady(TagSession_Players())) Ps_Prompt(0,"Confirm");
        Ps_Prompt(2,"Cancel"); Ps_PromptsDraw(); Ps_Align(1);
        return;
    }
    Ps_Heading(174,8,ffa()?"BATTLE LINEUP":"TEAM SETUP",2.1f,300,128);
    Ps_Copy(heading,"TEAM BATTLE",sizeof(heading));
    if(ffa()) Ps_Copy(heading,"FREE FOR ALL",sizeof(heading));
    else if(TagSession_DraftMode()==TAG_MODE_TRAINING) Ps_Copy(heading,"MODDED TRAINING",sizeof(heading));
    else if(TagSession_DraftMode()==TAG_MODE_GAUNTLET) Ps_Copy(heading,"FIGHTER GAUNTLET",sizeof(heading));
    else if(TagSession_DraftMode()==TAG_MODE_TOURNAMENT) Ps_Copy(heading,"TAG TEAM TOURNAMENT",sizeof(heading));
    Ps_Heading(391,14,heading,0.8f,224,128);
    Ps_Panel(TBO_KIT_WINDOW,12,68,500,gauntlet()?416:404,0x80);
    /* the three steps, this one on gold */
    Ps_Pill(TBO_KIT_STEEL,60,75,164,95,0x70);
    Ps_Pill(TBO_KIT_GOLD,204,74,308,96,0x80);
    Ps_Pill(TBO_KIT_STEEL,348,75,452,95,0x70);
    Ps_Text(112,Ps_TextTop(85,0.62f),"PLAYERS",0.62f,92,0);
    Ps_Text(184,Ps_TextTop(85,0.62f),">",0.62f,18,2);
    Ps_Text(256,Ps_TextTop(85,0.66f),ffa()?"LINEUP":"TEAMS",0.66f,92,1);
    Ps_Text(328,Ps_TextTop(85,0.62f),">",0.62f,18,2);
    Ps_Text(400,Ps_TextTop(85,0.62f),"FIGHTERS",0.62f,92,0);
    Ps_Text(256,101,"Choose your fighters on the next screen. End each team when ready.",0.65f,460,2);
    if(tournament()) Ps_Text(256,119,"ENTER UP TO FOUR TEAMS - other entrants are CPU teams",0.75f,452,2);
    else if(TagSession_DraftMode()==TAG_MODE_GAUNTLET) Ps_Heading(139,119,"YOUR TEAM",1.05f,195,128);
    else if(!ffa()) {
        Ps_Heading(139,119,"TEAM 1",1.05f,195,128);
        Ps_Heading(373,119,"TEAM 2",1.05f,195,128);
    } else Ps_Text(256,119,"EVERY FIGHTER FOR THEMSELVES",0.85f,440,2);
    for(p=0;p<TagSession_Players();p++) {
        t=TagSession_PlayerTeam(p); x=tournament()?96:30+t*234; y=143+(tournament()?p:n[t])*31;n[t]++;
        plate(x,y,tournament()?320:218,28,focus==p,t);
        if(tournament())sprintf(text,Port_TagText("Player %d  -  Team %d"),p+1,t+1);else sprintf(text,Port_TagText("Player %d"),p+1);
        Ps_Align(0); Ps_Text(x+14,y+2,text,0.9f,172,focus==p?1:2);
        if(focus==p && editing && !ffa()) Ps_Text(x+196,y+2,t?"<":">",0.9f,16,1);
    }
    Ps_Align(1);
    for(t=0;t<2;t++) if(n[t]==0 && !ffa() && !tournament() && TagSession_DraftMode()!=TAG_MODE_GAUNTLET) Ps_Text(139+t*234,160,"CPU team",0.85f,205,2);
    if(!players) Ps_Text(256,211,"CPU ONLY - choose every fighter, then watch the battle.",0.8f,440,2);
    for(t=0;t<2;t++) {
        if(!visible(players+t))continue;
        x=30;
        plate(x,rules_y(),452,29,focus==players+t,0);
        sprintf(text,"%s%s: %s%s",editing&&focus==players+t?"< ":"",
            Port_TagText(t?"Opponent CPU difficulty":gauntlet()?"Heal per Wave":"Duel Time"),Port_TagText(TagSession_RuleLabel(t)),editing&&focus==players+t?" >":"");
        Ps_Text(256,rules_y()+3,text,0.8f,430,focus==players+t?1:2);
    }
    if(gauntlet()) {
        plate(30,300,452,27,focus==players+2,0);
        sprintf(text,Port_TagText("%sMusic: %s%s"),editing&&focus==players+2?"< ":"",
            TagSession_RuleLabel(2),editing&&focus==players+2?" >":"");
        Ps_Text(256,301,text,0.75f,430,focus==players+2?1:2);
    }
    if(players) {
        plate(30,controls_y(),452,27,focus==controls_card(),0);
        Ps_Text(256,controls_y()+1,controllers_ready()?"Controller Setup - assignments ready":"Controller Setup - assign players",0.8f,430,2);
    } else Ps_Text(256,controls_y()+2,"L3 changes who you follow. Start opens the pause menu.",0.7f,440,2);
    plate(30,buttons_y(),114,gauntlet()?31:35,focus==controls_card()+2,0);
    plate(154,buttons_y(),328,gauntlet()?31:35,focus==controls_card()+1,0);
    Ps_Text(86,buttons_y()+4,"Back",0.95f,100,2);
    Ps_Text(318,buttons_y()+4,"Choose Fighters",1.05f,290,focus==controls_card()+1?1:2);
    Ps_Text(256,gauntlet()?396:383,editing?"Left / Right changes the value. Cross saves. Triangle cancels.":
        "Directions move between cards. Cross selects. Start continues.",0.6f,455,4);
    Ps_Align(0); Ps_PromptsClear();
    Ps_Prompt(0,editing?"Confirm":"Select"); Ps_Prompt(7,"Continue"); Ps_Prompt(2,editing?"Cancel":"Back"); Ps_PromptsDraw(); Ps_Align(1);
}
s32 TagSetup_Input(void) {
    extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);
    s32 old=focus,players=TagSession_Players();
    u32 pressed=gPad[0].gamePressed,nav=gPad[0].gameRepeat,raw=gPad[0].repeat;
    if(Port_TagControllersActive()) {
        Port_TagControllersPoll();
        if(pressed&PADG_TRIANGLE) {Port_TagControllersCancel(); Snd_PlaySe(1,2);}
        else if((pressed&PADG_CROSS) && Port_TagControllersCommit()) {
            checkedThisSetup=1; Snd_PlaySe(1,1); focus=controls_card()+1;
        }
        return 0;
    }
    if(raw&PAD_LSTICK_LEFT) nav|=PADG_LEFT;
    if(raw&PAD_LSTICK_RIGHT) nav|=PADG_RIGHT;
    if(raw&PAD_LSTICK_UP) nav|=PADG_UP;
    if(raw&PAD_LSTICK_DOWN) nav|=PADG_DOWN;
    Port_TagMenuTrace(4,3,focus,editing,0,0);
    if(editing) {
        if(pressed&PADG_TRIANGLE) {
            if(focus<players) TagSession_SetPlayerTeam(focus,editOriginal);
            else while(editDelta) {
                s32 undo=editDelta>0?-1:1;
                TagSession_ChangeRule(focus-players,undo);editDelta+=undo;
            }
            editing=0;Snd_PlaySe(1,2);return 0;
        }
        if(nav&(PADG_LEFT|PADG_RIGHT)) {
            s32 direction=(nav&PADG_RIGHT)?1:-1;
            if(focus<players) TagSession_SetPlayerTeam(focus,tournament()?(TagSession_PlayerTeam(focus)+direction+4)%4:direction>0?1:0);
            else {TagSession_ChangeRule(focus-players,direction);editDelta+=direction;}
            Snd_PlaySe(1,0);
        }
        if(pressed&(PADG_CROSS|PADG_START)) {editing=0;Snd_PlaySe(1,1);}
        if(!(pressed&PADG_START)) return 0;
    }
    if(pressed&PADG_TRIANGLE) { Snd_PlaySe(1,2); return -1; }
    if(nav&(PADG_UP|PADG_DOWN|PADG_LEFT|PADG_RIGHT)) navigate(nav);
    if(old!=focus) Snd_PlaySe(1,0);
    if(pressed&(PADG_CROSS|PADG_START)) {
        if(focus==controls_card()+2 && !(pressed&PADG_START)) { Snd_PlaySe(1,2); return -1; }
        if(focus<controls_card() && !(pressed&PADG_START)) {
            if(focus<players && ffa()) return 0;
            editing=1;editDelta=0;
            editOriginal=focus<players?TagSession_PlayerTeam(focus):0;
            Snd_PlaySe(1,1); return 0;
        }
        if((focus==controls_card() && players) || (players>1 && Port_TagOption(TAG_OPT_CONTROLLER_CHECK) && !controllers_ready())) {
            Port_TagControllersBegin(players); Snd_PlaySe(1,1); return 0;
        }
        Snd_PlaySe(1,1); return TagSession_Commit()?1:0;
    }
    return 0;
}
#endif
