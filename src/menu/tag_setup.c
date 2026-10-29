#ifdef PORT
#include "common.h"
#include "battle/tag_session.h"
#include "menu/port_screen.h"
#include "menu/tag_setup.h"
#include "menu/tag_controllers.h"
#include "battle/tag_options.h"
#include "sys/pad.h"
#include "menu/tbo_art.h"
#include "menu/tag_kit.h"

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
/* A card: a bar of the online screens' kit (gold under the cursor), its team's colour in a dot at its left end;
   a place to tap (its id). */
static void plate(f32 x,f32 y,f32 w,f32 h,s32 selected,s32 red,s32 id) {
    Tk_Bar(x,y,x+w,y+h,selected);
    if(red>=0) Ps_Disc(x+h*0.62f,y+h*0.5f,h*0.2f,red?Ps_Rgba(255,150,150,128):Ps_Rgba(150,210,255,128),
        red?Ps_Rgba(200,40,50,128):Ps_Rgba(40,110,220,128));
    if(id>=0) Tk_Region(x,y,x+w,y+h,TK_ROW,id);
}
/* While a card's value changes: the arrows at its right end, each half a place to tap. */
static void arrows(f32 x,f32 y,f32 w,f32 h) {
    Ps_Button(3,x+w-26,y+h*0.5f,h*0.72f);
    Tk_Region(x+w-48,y-4,x+w-26,y+h+4,TK_PAD,PADG_LEFT);
    Tk_Region(x+w-26,y-4,x+w,y+h+4,TK_PAD,PADG_RIGHT);
}
static void setup_title(char *title) {
    char card[64];
    Ps_Copy(card,"TEAM BATTLE",sizeof(card));
    if(ffa()) Ps_Copy(card,"FREE FOR ALL",sizeof(card));
    else if(TagSession_DraftMode()==TAG_MODE_TRAINING) Ps_Copy(card,"MODDED TRAINING",sizeof(card));
    else if(TagSession_DraftMode()==TAG_MODE_GAUNTLET) Ps_Copy(card,"FIGHTER GAUNTLET",sizeof(card));
    else if(TagSession_DraftMode()==TAG_MODE_TOURNAMENT) Ps_Copy(card,"TAG TEAM TOURNAMENT",sizeof(card));
    Tk_Title(title,card);
}
void TagSetup_Draw(void) {
    s32 p,t,n[4]={0,0,0,0},players=TagSession_Players();
    f32 x,y;
    char text[96];
    Tk_Begin(); Tk_Backdrop(); Ps_Align(1);
    if(Port_TagControllersActive()) {
        setup_title("CONTROLLER SETUP");
        Tk_Box(18,64,494,141+TagSession_Players()*42,0);
        Ps_Text(256,80,"Press START in player order, using your own controller.",0.8f,450,2);
        Ps_Text(256,102,"SELECT leaves a seat. A controller can join only once.",0.7f,450,0);
        for(p=0;p<TagSession_Players();p++) {
            char device[72];
            Port_TagControllersName(p,(u32)device,sizeof(device));
            plate(30,135+p*42,452,36,0,p&1,-1);
            sprintf(text,"P%d",p+1);
            Ps_Align(0); Ps_Text(64,Ps_TextTop(153+p*42,0.95f),text,0.95f,45,2);
            Ps_Text(110,Ps_TextTop(153+p*42,0.75f),device,0.75f,340,0);
        }
        Ps_Align(1);
        Tk_Talk(Tk_Left(),320,Tk_Left()+400,408,Port_TagControllersReady(TagSession_Players())?
#ifdef __ANDROID__
            "Everyone is ready. Player 1: press Cross to confirm.":"Waiting for each player to join... Touch screen: START joins, SELECT leaves.",0.74f);
#else
            "Everyone is ready. Player 1: press Cross to confirm.":"Waiting for each player to join... Keyboard players use their START binding.",0.74f);
#endif
        Tk_PromptsClear();
        if(Port_TagControllersReady(TagSession_Players())) Tk_Prompt(0,"Confirm");
        Tk_Prompt(2,"Cancel"); Tk_PromptsDraw();
        return;
    }
    setup_title(ffa()?"BATTLE LINEUP":"TEAM SETUP");
    Tk_Box(12,62,500,gauntlet()?416:404,0);
    /* the three steps, this one on gold */
    Tk_Para(60,73,164,91,7,Ps_Rgba(60,80,150,110),Ps_Rgba(30,45,110,110));
    Tk_Capsule(204,71,308,93,ffa()?"LINEUP":"TEAMS",0.66f);
    Tk_Para(348,73,452,91,7,Ps_Rgba(60,80,150,110),Ps_Rgba(30,45,110,110));
    Tk_Ball(72,82,8,1); Tk_Ball(222,82,9,2); Tk_Ball(360,82,8,3);
    Ps_Text(120,Ps_TextTop(82,0.62f),"PLAYERS",0.62f,64,0);
    Ps_Text(184,Ps_TextTop(82,0.62f),">",0.62f,18,2);
    Ps_Text(328,Ps_TextTop(82,0.62f),">",0.62f,18,2);
    Ps_Text(408,Ps_TextTop(82,0.62f),"FIGHTERS",0.62f,64,0);
    Ps_Text(256,99,"Choose your fighters on the next screen. End each team when ready.",0.62f,460,0);
    if(tournament()) Ps_Text(256,119,"ENTER UP TO FOUR TEAMS - other entrants are CPU teams",0.75f,452,2);
    else if(TagSession_DraftMode()==TAG_MODE_GAUNTLET) Ps_Heading(139,117,"YOUR TEAM",1.0f,195,128);
    else if(!ffa()) {
        Ps_Heading(139,117,"TEAM 1",1.0f,195,128);
        Ps_Heading(373,117,"TEAM 2",1.0f,195,128);
    } else Ps_Text(256,119,"EVERY FIGHTER FOR THEMSELVES",0.85f,440,2);
    for(p=0;p<TagSession_Players();p++) {
        t=TagSession_PlayerTeam(p); x=tournament()?96:30+t*234; y=143+(tournament()?p:n[t])*31;n[t]++;
        plate(x,y,tournament()?320:218,28,focus==p,ffa()?-1:t&1,p);
        if(tournament())sprintf(text,Port_TagText("Player %d  -  Team %d"),p+1,t+1);else sprintf(text,Port_TagText("Player %d"),p+1);
        Ps_Align(0); Ps_Text(x+(ffa()?20:32),Ps_TextTop(y+14,0.85f),text,0.85f,150,focus==p?1:0);
        Ps_Align(1);
        if(focus==p && editing && !ffa()) arrows(x,y,tournament()?320:218,28);
    }
    Ps_Align(1);
    for(t=0;t<2;t++) if(n[t]==0 && !ffa() && !tournament() && TagSession_DraftMode()!=TAG_MODE_GAUNTLET) Ps_Text(139+t*234,Ps_TextTop(157,0.85f),"CPU team",0.85f,205,3);
    if(!players) Ps_Text(256,211,"CPU ONLY - choose every fighter, then watch the battle.",0.8f,440,2);
    for(t=0;t<2;t++) {
        if(!visible(players+t))continue;
        x=30;
        plate(x,rules_y(),452,29,focus==players+t,-1,players+t);
        Ps_Copy(text,(char *)Port_TagText(t?"Opponent CPU difficulty":gauntlet()?"Heal per Wave":"Duel Time"),sizeof(text));
        Ps_Cat(text,":  ",sizeof(text)); Ps_Cat(text,(char *)Port_TagText(TagSession_RuleLabel(t)),sizeof(text));
        Ps_Text(250,Ps_TextTop(rules_y()+14.5f,0.8f),text,0.8f,380,focus==players+t?1:0);
        if(editing&&focus==players+t) arrows(x,rules_y(),452,29);
    }
    if(gauntlet()) {
        plate(30,300,452,27,focus==players+2,-1,players+2);
        Ps_Copy(text,(char *)Port_TagText("Music"),sizeof(text));Ps_Cat(text,":  ",sizeof(text));Ps_Cat(text,(char *)TagSession_RuleLabel(2),sizeof(text));
        Ps_Text(250,Ps_TextTop(313.5f,0.75f),text,0.75f,380,focus==players+2?1:0);
        if(editing&&focus==players+2) arrows(30,300,452,27);
    }
    if(players) {
        plate(30,controls_y(),452,27,focus==controls_card(),-1,controls_card());
        Ps_Text(256,Ps_TextTop(controls_y()+13.5f,0.78f),controllers_ready()?"Controller Setup - assignments ready":"Controller Setup - assign players",0.78f,420,focus==controls_card()?1:0);
    } else Ps_Text(256,Ps_TextTop(controls_y()+13.5f,0.7f),"L3 changes who you follow. Start opens the pause menu.",0.7f,440,3);
    plate(30,buttons_y(),114,gauntlet()?31:35,focus==controls_card()+2,-1,controls_card()+2);
    plate(154,buttons_y(),328,gauntlet()?31:35,focus==controls_card()+1,-1,controls_card()+1);
    y=buttons_y()+(gauntlet()?15.5f:17.5f);
    Ps_Text(87,Ps_TextTop(y,0.92f),"Back",0.92f,90,focus==controls_card()+2?1:0);
    Ps_Text(318,Ps_TextTop(y,1.02f),"Choose Fighters",1.02f,290,focus==controls_card()+1?1:2);
    Ps_Text(256,gauntlet()?396:383,editing?"Left / Right changes the value. Cross saves. Triangle cancels.":
        "Tap a card or use the directions. Cross selects. Start continues.",0.6f,455,3);
    Tk_PromptsClear();
    Tk_Prompt(0,editing?"Confirm":"Select"); Tk_Prompt(7,"Continue"); Tk_Prompt(2,editing?"Cancel":"Back"); Tk_PromptsDraw();
}
s32 TagSetup_Input(void) {
    extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);
    s32 old=focus,players=TagSession_Players();
    u32 pressed=gPad[0].gamePressed,nav=gPad[0].gameRepeat,raw=gPad[0].repeat;
    { /* a tap (menu/tag_kit.h): a card is chosen and pressed; while a value changes, anywhere else saves it */
        s32 kind,value;
        if(Tk_Tap(&kind,&value)) {
            if(kind==TK_PAD) {pressed|=value;nav|=value;}
            else if(editing) pressed|=PADG_CROSS;
            else if(!Port_TagControllersActive()) {if(value!=focus)Snd_PlaySe(1,0);focus=value;old=focus;pressed|=PADG_CROSS;}
        }
    }
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
