#include "common.h"
#include "menu/menu_a.h"
#ifdef PORT
#include "battle/tag_session.h"
#include "battle/tag_challenge.h"
#include "menu/tag_tournament.h"
#include "battle/tag_options.h"
#include "menu/tag_labels.h"
#include "menu/tag_setup.h"
#include "menu/port_screen.h"
#include <stdio.h>
extern s32 Port_TagAvailable(void);
extern void Port_NetMenuOpen(void);
extern int Port_NetMenuListed(void);
extern int Port_NetSession(void);
extern void Port_NetLeave(void);
#endif
#include "sys/pad.h"
#include "sys/save.h"

/* The screen's work (overlay .data, 0x3B0E80). */
MainMenu *gMainMenu = NULL;
void MainMenu_PlateGoto(s32 idx,char *label);

/*
 * MainMenu: the main menu of the game (progress mode 4), 0x334C00..0x336A90. First object of the menu
 * overlay DBZP.BIN. A ring of up to 11 items, six plates visible, the cursor on rows 0..3; four guide
 * characters (Gohan, Videl, Trunks, Goten) of which one talks; the seven dragon balls of the save.
 */

#define MAINMENU_VOICE_BASE 0x86F9

/* The item under the cursor. */
#define MAINMENU_CUR(m) ((m)->items[((m)->top + (m)->cursor + 1) % (m)->itemCount])

#ifdef PORT
/*
 * PC only: "Tenkaichi Budokai Online" (item 11), listed after Dragon Net Battle (after Duel when that one is
 * hidden). Its label is written into the free fourth row of the second label sheet of each plate state (label 11,
 * the item's own number), put together from the letters of the other labels of the player's disc
 * (port/src/gs/menu_text.c, Port_MainItemLabel); without them (another edition of the game) from port_tbo_label.h. Its
 * icon is Dragon Net Battle's. Choosing it opens the port's screen mode (menu/port_screen.c) with the screens of
 * menu/port_tbo.c. It has no voiced line of the guides.
 */
#include "port_tbo_label.h"
#include "menu/tbo_art.h"
#include "menu/tag_controllers.h"
#include "menu/tag_kit.h"
#include "../../port/src/gs/tag_net.h"

#define MAINMENU_TBO 11
static s32 tagPage, tagMode, presetPage;
#define TAG_MENU_PRESET 100
static s32 preset_count(void) {return tagMode==TAG_MODE_GAUNTLET?TagChallenge_Count():5;}
static s32 preset_rows(void) {s32 n=preset_count()-presetPage*8;return n>8?8:n;}
static s32 stockItems[MAINMENU_ITEM_MAX], stockCount, stockTop, stockCursor;
/* The native plate movie uses a second texture sheet after row seven. Keep
   both sheets intact so long settings pages and returning to stock work. */
static u8 stockLabels[2][12][512*32], tagLabels[2][12][512*32];
static s32 tagLabelsReady;
static s32 tmPageAt; /* (Tk frame the page came in: TagMenu_DrawPage) */
static s32 tmRow;    /* the row under the cursor on the mod's pages */
/* The modes' page is a dragon radar (TagMenu_DrawRadar): its seven balls, its lock */
#define TR_BALLS 7
#define TR_LOCK_FRAMES 22 /* the lock's length, frames */
static s32 tmRadarRow;  /* the ball last chosen on the radar (the page comes back to it) */
static s32 tmLockAt;    /* the frame (sTkFrames) the lock began; 0: none */
static s32 tmLockRow;   /* the ball being locked */
static s32 tmLockHost;  /* whether it was an online room's host menu when the lock began */
static void TagMenu_Goto(s32 row);
/* The mod's pages have a song of their own (2026-11-21, the user's choice): CAUTION!, entry 0x0B of the music list; the game's
   own menu music comes back with the game's menu. (BT3's list: BT4's is another, it keeps the menu's.) */
static s32 tmBgm; /* 1: CAUTION! is what plays now */
static void TagMenu_Music(s32 on) {
    if(PortGame_IsBT4()) return;
    if(on && !tmBgm) { Bgm_Play(0x10B16+0x0B); tmBgm=1; }
    else if(!on && tmBgm) { Bgm_Play(0x10B16); tmBgm=0; }
}
static s32 TagMenu_Option(s32 page,s32 row) {
    s32 i;
    for(i=0;i<TAG_OPT_COUNT;i++) if(tagOptionInfo[i].group==page-5 && row--==0) return i;
    return -1;
}
static s32 TagMenu_Count(s32 page) {
    s32 n=0;
    if(page==TAG_MENU_PRESET) return preset_rows()+3;
    if(page==3) return 1; /* spatial setup page draws its own roster */
    if(page==2) return 6;
    if(page==1) return TagNet_HostMenu()?7:8;
    if(page==4) return TAG_OPTION_GROUP_COUNT+1;
    if(page<5) return 6;
    while(TagMenu_Option(page,n)>=0) n++;
    return n+1;
}
static s32 TagMenu_Label(s32 page,s32 row) {
    if(page==TAG_MENU_PRESET) {
        s32 n=preset_rows();
        return row<n?(tagMode==TAG_MODE_GAUNTLET?TAG_LABEL_PRESETS:TAG_LABEL_TOURS)+presetPage*8+row:
            row==n?TAG_LABEL_PREV:row==n+1?TAG_LABEL_NEXT:4;
    }
    if(page==1) {static const s32 labels[]={0,1,2,TAG_LABEL_GAUNTLET,TAG_LABEL_TOURNAMENT,TAG_LABEL_LOBBY,3,4};return TagNet_HostMenu() && row==6?TAG_LABEL_CLOSE_LOBBY:labels[TagNet_HostMenu() && row>=5?row+1:row];}
    if(page==3) return TAG_LABEL_CHOOSE;
    if(page==2 && row>=4) return row==4?TAG_LABEL_CPU:4;
    if(page==4) return row==TAG_OPTION_GROUP_COUNT?4:TAG_LABEL_SETTINGS+row;
    if(page>=5) return row==TagMenu_Count(page)-1?4:TAG_LABEL_OPTIONS+TagMenu_Option(page,row);
    return (page==2?5:0)+row;
}
static void TagMenu_SettingChange(s32 row,s32 direction) {
    s32 id=TagMenu_Option(tagPage,row),lo,hi,step;
    if(id<0) return;
    lo=tagOptionInfo[id].min; hi=tagOptionInfo[id].max; step=tagOptionInfo[id].step;
    Port_TagOptionSet(id,Ps_Wrap(Port_TagOption(id)+direction*step,lo,hi));
    Snd_PlaySe(1,0);
}
void TagMenu_Request(s32 page) { tagPage=page; }
static s32 tag_icon(s32 item) {
    static const s32 icons[6]={3,3,6,9,4,4};
    return tagPage>=4 ? 9 : tagPage==3 ? 3 : tagPage ? icons[item%6] : item==MAINMENU_TBO?4:item==13/*ZW: Dragon History's*/?0:item==14/*Tag Team*/?2:item==12/*Mod Manager*/?7:item;
}
#define MM_ICON(item) tag_icon(item)
#define PORT_LABEL_RUN /* (PortLabelRun: port_tbo_label.h's, the same) */
/* "Mod Manager" (item 12), listed after Options. The label sheets have no free row left, so each plate state gets a
   third sheet (PortMainMenu_AddSheet: a copy of the second with pixels of its own) whose first row is its label
   (label 16, port_modmgr_label.h); its icon is the Data Center's. Choosing it opens the Duel mode with gPortDuelRole
   2, where Duel_Main runs the manager's screen (menu/port_modmgr.c) instead of its own. */
#include "port_modmgr_label.h"

#define MAINMENU_MOD_MANAGER 12
#define MAINMENU_TAG 14 /* "Tag Team": the Tag Team mod's own item (label 18, the third row of the third sheets) */
#define MAINMENU_ZW 13 /* "Z Warriors" (menu/port_zw.c): listed after Evolution Z, label 17 (the second row of the third sheets) */
s32 gPortDuelRole; /* the Duel screens (progress mode 38) are open for a screen of the PC's: 2 the Mod Manager, 3 Z Warriors */
#define MM_LABEL(item) ((item) == MAINMENU_MOD_MANAGER ? 16 : (item) == MAINMENU_ZW ? 17 : (item) == MAINMENU_TAG ? 18 : (item)) /* whose label row an item shows */

/* A third label sheet for a plate state (slot 8: not chosen, 12: chosen): the movie takes a label's sheet as the
   n-th entry behind the one in gMainMenu->tex[slot], so the two entries of the game are copied into a block with
   room for a third, which is the second again with pixels of its own (the same palette: the PC's labels are made
   for it). Returns the new sheet's entry. */
static u8 sPortSheetEnt[2][3 * 0x40];
static u8 *sPortSheetPix[2];

static u8 *PortMainMenu_AddSheet(s32 slot) {
    s32 n = slot == 8 ? 0 : 1;
    u8 *ent = sPortSheetEnt[n];
    u8 *third = ent + 0x80;
    u32 size;

    memcpy(ent, gMainMenu->tex[slot], 0x80);
    memcpy(third, gMainMenu->tex[slot] + 0x40, 0x40);
    size = *(u32 *)(third + 8);
    sPortSheetPix[n] = Heap_Alloc(size, 0x40, 0, 2);
    memcpy(sPortSheetPix[n], *(u8 **)(third + 0x38), size);
    *(u8 **)(third + 0x38) = sPortSheetPix[n];
    gMainMenu->tex[slot] = ent;
    return third;
}

/* A label sheet's pixels are stored the way the GS keeps 8-bit textures (in blocks): where pixel (x, y) is. */
static s32 PortMainMenu_PixelAt(s32 x, s32 y) {
    s32 block = (y & ~0xF) * 512 + (x & ~0xF) * 2;
    s32 swap = (((y + 2) >> 2) & 1) * 4;
    s32 line = (((y & ~3) >> 1) + (y & 1)) & 7;

    return block + line * 512 * 2 + ((x + swap) & 7) * 4 + ((y >> 1) & 1) + ((x >> 2) & 2);
}

/* Writes runs of palette indices into label row `row` of a sheet. */
static void PortMainMenu_WriteRuns(u8 *tex, s32 row, const PortLabelRun *run, s32 count) {
    u8 *px = *(u8 **)(tex + 0x38) + 0x60;
    s32 i;
    s32 k;

    for (i = 0; i < count; i++) {
        for (k = 0; k < run[i].len; k++) {
            px[PortMainMenu_PixelAt(run[i].x + k, row * 0x20 + run[i].y)] = run[i].v;
        }
    }
}

/* Z Warriors' label in the disc's lettering into row `row` of a sheet; 0 when it could not be made. */
static s32 PortMainMenu_WriteZw(u8 *tex, s32 row, s32 lit) {
    extern s32 Port_MainItemLabelZw(s32 entry, s32 lit, s32 out);
    static u8 index[512 * 0x20];
    u8 *px = *(u8 **)(tex + 0x38) + 0x60;
    s32 x;
    s32 y;

    if (!Port_MainItemLabelZw((s32)(u32)tex, lit, (s32)(u32)index)) {
        return 0;
    }
    for (y = 0; y < 0x20; y++) {
        for (x = 0; x < 512; x++) {
            px[PortMainMenu_PixelAt(x, row * 0x20 + y)] = index[y * 512 + x];
        }
    }
    return 1;
}

/* Tag Team's label, the same way. */
static s32 PortMainMenu_WriteTag(u8 *tex, s32 row, s32 lit) {
    extern s32 Port_MainItemLabelTag(s32 entry, s32 lit, s32 out);
    static u8 index[512 * 0x20];
    u8 *px = *(u8 **)(tex + 0x38) + 0x60;
    s32 x;
    s32 y;

    if (!Port_MainItemLabelTag((s32)(u32)tex, lit, (s32)(u32)index)) {
        return 0;
    }
    for (y = 0; y < 0x20; y++) {
        for (x = 0; x < 512; x++) {
            px[PortMainMenu_PixelAt(x, row * 0x20 + y)] = index[y * 512 + x];
        }
    }
    return 1;
}

/* The label in the disc's lettering into row `row` of a sheet; 0 when it could not be made. */
static s32 PortMainMenu_WriteDisc(u8 *tex, s32 row, s32 lit) {
    extern s32 Port_MainItemLabel(s32 entry, s32 lit, s32 out);
    static u8 index[512 * 0x20];
    u8 *px = *(u8 **)(tex + 0x38) + 0x60;
    s32 x;
    s32 y;

    if (!Port_MainItemLabel((s32)(u32)tex, lit, (s32)(u32)index)) {
        return 0;
    }
    for (y = 0; y < 0x20; y++) {
        for (x = 0; x < 512; x++) {
            px[PortMainMenu_PixelAt(x, row * 0x20 + y)] = index[y * 512 + x];
        }
    }
    return 1;
}
static void TagMenu_SetPage(s32 page) {
    extern s32 Port_MainTagLabel(s32,s32,s32,s32);
    s32 lit,row,x,y,count=TagMenu_Count(page);
    MainMenu_PlateGoto(0,"fl_off_start");
    if(!tagPage && page) { stockTop=gMainMenu->top; stockCursor=gMainMenu->cursor; }
    tagPage=page;
    TagMenu_Music(page!=0);
    tmPageAt=sTkFrames;
    if(page==3) TagSetup_Reset();
    if(page) { extern s32 Port_DataFingerprintNeed(void); Port_DataFingerprintNeed(); } /* (the mod's online rooms wait for it: begun now, on its own thread) */
    tmRow=0;
    /* The mod's pages draw their own lists (TagMenu_DrawPage): the game's plates, under the sky, keep the game's
       labels, put back here when the game's menu returns. (A page can have more rows than the game's ring holds,
       12: Mod Settings has 13. Its row is tmRow, not the ring's.) */
    if(!page) for(lit=0;lit<2;lit++) for(row=0;row<12;row++) {
        u8 *tex=gMainMenu->tex[lit?12:8]+(row/8)*0x40;
        u8 *px=*(u8 **)(tex+0x38)+0x60;
        u8 *labelPixels=page?tagLabels[lit][row]:stockLabels[lit][row];
        if(page) {
            s32 label=TagMenu_Label(page,row);
            if(!Port_MainTagLabel((s32)tex,lit,label,(s32)labelPixels))
                memset(labelPixels,0,512*32);
        }
        for(y=0;y<32;y++) for(x=0;x<512;x++)
            px[PortMainMenu_PixelAt(x,(row%8)*32+y)]=labelPixels[y*512+x];
    }
    if(page) {
        if(count>MAINMENU_ITEM_MAX) count=MAINMENU_ITEM_MAX;
        gMainMenu->itemCount=count;
        for(row=0;row<count;row++) gMainMenu->items[row]=row;
        gMainMenu->top=count-1; gMainMenu->cursor=0; gMainMenu->extra=5%count;
    } else {
        memcpy(gMainMenu->items,stockItems,sizeof(stockItems));
        gMainMenu->itemCount=stockCount; gMainMenu->top=stockTop; gMainMenu->cursor=stockCursor;
        gMainMenu->extra=(stockTop+6)%stockCount;
    }
    Voice_Stop(0); gMainMenu->voiceLine=-1;
    gMainMenu->flags=(gMainMenu->flags&~MAINMENU_STARTED)|MAINMENU_GREETED;
    tmLockAt=0; /* (a lock belongs to the radar's page as it was) */
    if(page==1) { if(tmRadarRow<0 || tmRadarRow>=TR_BALLS || tmRadarRow>=count) tmRadarRow=0; TagMenu_Goto(tmRadarRow); }
}
static void TagMenu_Prepare(void) {
    extern s32 Port_MainTagLabel(s32,s32,s32,s32);
    s32 lit,row,x,y,ok=1;
    if(!Port_TagAvailable()) { tagPage=0; return; }
    tmBgm=0; /* (the game started its own music for this menu) */
    memcpy(stockItems,gMainMenu->items,sizeof(stockItems));
    stockCount=gMainMenu->itemCount; stockTop=gMainMenu->top; stockCursor=gMainMenu->cursor;
    for(lit=0;lit<2;lit++) for(row=0;row<12;row++) {
        u8 *tex=gMainMenu->tex[lit?12:8]+(row/8)*0x40;
        u8 *px=*(u8 **)(tex+0x38)+0x60;
        for(y=0;y<32;y++) for(x=0;x<512;x++)
            stockLabels[lit][row][y*512+x]=px[PortMainMenu_PixelAt(x,(row%8)*32+y)];
        if(!tagLabelsReady && row<10) ok&=Port_MainTagLabel((s32)tex,lit,row,(s32)tagLabels[lit][row]);
    }
    tagLabelsReady=ok;
    if(tagPage && ok) TagMenu_SetPage(tagPage);
}
/* ---- The mod's pages (SELECT) in the look of Dragon Ball (menu/tag_kit.h; the design: port/tools/db_assets.py): the
   title and its red capsule at the top left, the plates of the list down the left (each with a dragon ball, the one
   under the cursor long and orange), a gold-framed picture window on the right, the white talk box at the foot, the
   buttons at the top right. Points are the design's (a 2712 x 1220 phone: Tk_PX, Tk_PY). The game's plates and icon
   window are under the sky, still there for the cursor's ring and the sounds. ---- */
#define TM_SHOWN 7     /* plates shown (the rest: the list scrolls) */
#define TM_Y0 290.0f   /* the first plate's top, */
#define TM_PITCH 98.0f /* from plate to plate, */
#define TM_PH 80.0f    /* a plate's height (design pixels) */
static f32 tmGlide;    /* where the orange plate is, gliding to the cursor's row (design pixels) */
/* The cursor onto row `row` of the page (items[row] is row on the mod's pages). */
static void TagMenu_Goto(s32 row) {
    s32 n=gMainMenu->itemCount;
    tmRow=row;
    if(n<=0 || row>=n) return;
    gMainMenu->cursor=0;
    gMainMenu->top=(row-1+n)%n;
    gMainMenu->extra=(gMainMenu->top+6)%n;
}
static s32 TagMenu_Cur(void) { return tagPage?tmRow:MAINMENU_CUR(gMainMenu); }
static s32 TagMenu_IsOption(s32 row) {
    return tagPage!=TAG_MENU_PRESET && tagPage>=5 && row<TagMenu_Count(tagPage)-1;
}
/* What the row under the cursor does (the talk box). */
static void TagMenu_Help(s32 row,char *message,s32 size) {
    static char descriptions[][96]={"Fight with your own teams and player assignments.",
        "Every fighter for themselves. Last fighter standing wins.",
        "Practice with teams and adjustable training rules.",
        "Survive successive waves of enemies with your team.",
        "Enter your teams in a tournament bracket.",
        "Practice together while the host chooses your next mode.",
        "Change the mod's gameplay, cameras and HUD settings.","Return to the previous menu."};
    Ps_Copy(message,descriptions[7],size);
    if(TagMenu_IsOption(row)) Ps_Copy(message,(char *)tagOptionInfo[TagMenu_Option(tagPage,row)].help,size);
    else if(tagPage==TAG_MENU_PRESET) {
        if(row<preset_rows()) sprintf(message,Port_TagText("Choose a challenge. Page %d of %d."),presetPage+1,(preset_count()+7)/8);
        else if(row<preset_rows()+2) Ps_Copy(message,row==preset_rows()?"Previous page":"Next page",size);
    }
    else if(tagPage==1 && (u32)row<7) Ps_Copy(message,TagNet_HostMenu() && row==6?"Close the online lobby and disconnect its guests.":descriptions[TagNet_HostMenu() && row==5?6:row],size);
    else if(tagPage==2 && row!=5) Ps_Copy(message,row==4?
        "Choose both rosters and watch the battle. L3 changes the fighter you follow.":
        "Choose how many people are playing on this PC.",size);
    else if(tagPage==4 && row!=TAG_OPTION_GROUP_COUNT) Ps_Copy(message,"Choose a group of mod settings.",size);
}
/* ---- The modes' page as a dragon radar (2026-11-08, the user's choice of three designs): the seven modes are the seven
   dragon balls on a ring of the radar's green screen, each with a short name; the chosen one big, its search rings
   pulsing, its name in gold. Left / right (and up / down) turn to the next ball, a tap on a ball enters it. Entering:
   the ball runs to the radar's middle and is LOCKED (TagMenu_RadarLock, about a third of a second), then the mode
   opens as before (TagMenu_RadarChoose). The panel at the right says what the mode is. The language switch at the
   top right (PT EN ES, Square too). Everything is drawn from a few pictures (db_night, db_radar, db_balls, db_ring,
   db_glass: port/tools/db_assets.py) and the cached lines of letters: a few dozen quads a frame. The ball's places on
   the ring are numbers worked out once (no sines for them every frame). ---- */
static const f32 kTrCos[TR_BALLS]={0.0f,0.78183f,0.97493f,0.43388f,-0.43388f,-0.97493f,-0.78183f};
static const f32 kTrSin[TR_BALLS]={-1.0f,-0.62349f,0.22252f,0.90097f,0.90097f,0.22252f,-0.62349f};
/* The balls' short names on the radar (the whole name is in the panel). */
static char *TagMenu_RadarShort(s32 i) {
    static char names[TR_BALLS+1][24]={"TEAMS","FREE FOR ALL","TRAINING","GAUNTLET","TOURNAMENT","ONLINE","SETTINGS","LEAVE"};
    if(TagNet_HostMenu() && i==5) return names[6];
    if(TagNet_HostMenu() && i==6) return names[7];
    return names[i];
}
/* What the panel shows of a mode: up to three short rules. */
static void TagMenu_RadarRules(s32 i,char **out,s32 *count) {
    static char rules[9][3][32]={
        {"2 TEAMS","UP TO 10 FIGHTERS","REVIVE ALLIES"},
        {"EVERY FIGHTER","LAST ONE STANDING",""},
        {"YOUR TEAMS","TRAINING RULES",""},
        {"WAVES OF ENEMIES","YOUR TEAM",""},
        {"BRACKET","UP TO 4 TEAMS",""},
        {"ROOM CODE","PLAY TOGETHER",""},
        {"GAMEPLAY","CAMERAS","HUD"},
        {"ENDS THE ROOM","",""},
        {"","",""}};
    s32 k,row=i;
    if(TagNet_HostMenu() && i==5) row=6;
    else if(TagNet_HostMenu() && i==6) row=7;
    if(row<0 || row>7) row=8;
    *count=0;
    for(k=0;k<3;k++) if(rules[row][k][0]) out[(*count)++]=rules[row][k];
}
static f32 TagMenu_Ease(f32 t) {
    if(t<0.0f) t=0.0f;
    if(t>1.0f) t=1.0f;
    t=1.0f-t;
    return 1.0f-t*t*t;
}
/* ---- The mod's pages, the new design (2026-11-21, approved on previews/redesign-v3): the arena behind with Goku (Tk_Backdrop),
   the gold TAG TEAM lettering with a dark ribbon (the page) and the seven dragon balls (the lit one is the row's) at the top
   left, the slanted plates of the list under it (a dragon ball on each, orange under the cursor), a bar at the foot as wide
   as the list (what the row does, in up to two lines, and its rules as chips under them), the buttons beside it. A setting's
   page has a slanted window at the right with the setting's preview. Each thing in its own band: nothing overlaps. ---- */
static void TagMenu_Header(char *sub,s32 lit) {
    f32 lx0=Tk_PX(60),lx1=Tk_PX(760),lw=lx1-lx0,lh=lw/(PS_UNIT_X*Ps_PicW(TBO_DB_LOGO)/Ps_PicH(TBO_DB_LOGO));
    f32 ly=Tk_PY(40)+1.0f*sinf((f32)sTkFrames*0.045f),ry0=ly+lh+Tk_PY(6),ry1=ry0+Tk_PY(62),rw,rx1;
    s32 k;
    Ps_SetShift(0);
    Ps_Sprite(TBO_DB_LOGO,lx0,ly,lx1,ly+lh,0.0f,0.0f,1.0f,1.0f,PS_PLAIN,0x80);
    rw=Ps_TextWidth(sub,0.74f)+Tk_PX(150)-Tk_PX(0);
    if(rw<Tk_PX(540)-Tk_PX(0)) rw=Tk_PX(540)-Tk_PX(0);
    rx1=Tk_PX(70)+rw;
    Tk_Para(Tk_PX(70)-1.5f,ry0-1.5f,rx1+1.5f,ry1+1.5f,(ry1-ry0)*0.4f,Ps_Rgba(120,160,235,110),Ps_Rgba(90,130,220,100));
    Tk_Para(Tk_PX(70),ry0,rx1,ry1,(ry1-ry0)*0.4f,Ps_Rgba(30,58,138,124),Ps_Rgba(6,14,58,126));
    Ps_Align(1);
    Ps_TextEx((Tk_PX(70)+rx1)*0.5f+4.0f,Ps_TextTop((ry0+ry1)*0.5f,0.74f),sub,0.74f,rw-Tk_PX(60)+Tk_PX(0),Ps_Rgba(245,250,255,0x80),Ps_Rgba(200,225,255,0x80),Ps_Rgba(6,16,60,0x80));
    for(k=0;k<7;k++) { /* the seven balls: the row's lit */
        f32 bx=rx1+Tk_PX(60+k*62)-Tk_PX(0),by=(ry0+ry1)*0.5f;
        if(k+1==lit) { Tk_Glow(bx-Tk_PY(40),by-Tk_PY(40),bx+Tk_PY(40),by+Tk_PY(40),255,190,60,0x60); Tk_Ball(bx,by,Tk_PY(24),k+1); }
        else Tk_BallA(bx,by,Tk_PY(17),k+1,0x40);
    }
}
/* A chip under the bar's text: a small dark slanted plate; returns its right edge. */
static f32 TagMenu_Chip(f32 x0,f32 y0,char *text) {
    f32 w=Ps_TextWidth(text,0.62f)+Tk_PX(70)-Tk_PX(0),y1=y0+Tk_PY(50),sk=(y1-y0)*0.4f;
    Tk_Para(x0-1.5f,y0-1.5f,x0+w+1.5f,y1+1.5f,sk,Ps_Rgba(120,160,235,110),Ps_Rgba(90,130,220,100));
    Tk_Para(x0,y0,x0+w,y1,sk,Ps_Rgba(30,58,138,124),Ps_Rgba(6,14,58,126));
    Ps_Align(1);
    Ps_Text(x0+w*0.5f+2.0f,Ps_TextTop((y0+y1)*0.5f,0.62f),text,0.62f,w-6.0f,0);
    return x0+w+Tk_PX(12)-Tk_PX(0);
}
/* A button at the foot (its picture and its word), a place to tap; returns where the next one starts. */
static f32 TagMenu_FootButton(f32 x,s32 kind,char *word,u32 bits) {
    char *label=(char *)Port_TagText(word);
    f32 cy=Tk_PY(1148),bw=Ps_ButtonW(kind,19.0f),w=Ps_MsgWidth(label,1.0f);
    Ps_Align(0);
    Ps_Button(kind,x+bw*0.5f,cy,19.0f);
    Ps_Msg(x+bw+5.0f,cy-(PS_MSG_BASE-PS_MSG_CAP*0.5f),label,1.0f,0x80);
    Ps_Align(1);
    if(bits) Tk_Region(x-4.0f,cy-22.0f,x+bw+w+9.0f,cy+22.0f,TK_PAD,(s32)bits);
    return x+bw+w+Tk_PX(40)-Tk_PX(0);
}
static void TagMenu_DrawPage(void) {
    extern s32 Port_TagLabelText(s32,u32,s32);
    extern void Port_TagOptionValue(s32,u32,s32),Hud_LoadTagPreview(void);
    extern s32 Port_TagPreviewAt(s32,s32,s32,s32,s32);
    static const s32 modeLabels[5]={0,1,2,TAG_LABEL_GAUNTLET,TAG_LABEL_TOURNAMENT};
    s32 count=TagMenu_Count(tagPage),cur=TagMenu_Cur(),first,i,option=TagMenu_IsOption(cur),pageOpts=tagPage!=TAG_MENU_PRESET && tagPage>=5,preview=0;
    f32 in=Tk_In(tmPageAt),slide=(1.0f-in)*-160.0f,target,x;
    f32 px0=Tk_PX(70),pw=Tk_PX(pageOpts?1260:960)-Tk_PX(0),ph=Tk_PY(TM_PH);
    char sub[160],card[96],label[96],value[96],help[192],part[96];
    /* the ribbon's words: the page, and the mode or group it is of */
    Ps_Copy(sub,(char *)Port_TagText(tagPage==TAG_MENU_PRESET?(tagMode==TAG_MODE_GAUNTLET?"CHOOSE GAUNTLET":"CHOOSE TOURNAMENT"):tagPage>=5?"MOD SETTINGS":tagPage==4?"MOD SETTINGS":tagPage==2?"PLAYERS":"MODDED MODES"),sizeof(sub));
    if((tagPage==2 || tagPage==TAG_MENU_PRESET) && (u32)tagMode<5 && Port_TagLabelText(modeLabels[tagMode],(u32)part,sizeof(part))) {
        Ps_Copy(card,part,sizeof(card)); Ps_Cat(card,"  -  ",sizeof(card)); Ps_Cat(card,sub,sizeof(card)); Ps_Copy(sub,card,sizeof(sub));
    } else if(tagPage>=5 && tagPage!=TAG_MENU_PRESET) {
        Ps_Cat(sub,"  -  ",sizeof(sub)); Ps_Cat(sub,(char *)Port_TagText(tagOptionGroups[tagPage-5]),sizeof(sub));
    }
    TagMenu_Header(sub,cur%7+1);
    if(TagNet_HostMenu()) { /* the lobby's owner: its online code, to pass on (port/src/gs/tag_link.c) */
        extern void TagNet_OnlineInfo(u32,u32,s32);
        char code[64],status[64];
        TagNet_OnlineInfo((u32)code,(u32)status,sizeof(code));
        if(code[0]) { Ps_Copy(card,(char *)Port_TagText("ONLINE CODE"),sizeof(card)); Ps_Cat(card,"  ",sizeof(card)); Ps_Cat(card,code,sizeof(card)); Tk_Capsule(Tk_PX(1160),Tk_PY(50),Tk_PX(1160)+Ps_TextWidth(card,0.9f)+Tk_PX(170)-Tk_PX(0),Tk_PY(124),card,0.9f); }
    }
    /* the language, top right (page 1: PT, EN, ES, Square too) */
    if(tagPage==1 && !TagNet_HostMenu()) {
        extern int Port_TagLang(void);
        f32 left=Tk_Languages(Tk_PX(2060),Tk_PY(66),Port_TagLang());
        Ps_Align(2);
        Ps_TextEx(left-6.0f,Ps_TextTop(Tk_PY(66),0.62f),"LANGUAGE",0.62f,80.0f,Ps_Rgba(220,230,255,0x80),Ps_Rgba(170,190,230,0x80),Ps_Rgba(6,16,60,0x80));
        Ps_Align(1);
    }
    /* the setting's window, at the right: only a setting's page has one (Goku is there on the others) */
    if(option) {
        f32 fx0=Tk_PX(1440),fy0=Tk_PY(260),fx1=Tk_PX(2640),fy1=Tk_PY(960),hh=Tk_PY(68),sk=Tk_PY(40)*0.4f;
        s32 id=TagMenu_Option(tagPage,cur);
        Ps_SetShift(-slide*0.5f);
        Tk_Glow(fx0-20,fy0-20,fx1+20,fy1+20,0,0,0,0x58);
        Tk_Para(fx0-2.0f,fy0-2.0f,fx1+2.0f,fy1+2.0f,sk,Ps_Rgba(120,160,235,120),Ps_Rgba(90,130,220,110));
        Tk_Para(fx0,fy0,fx1,fy1,sk,Ps_Rgba(14,30,92,126),Ps_Rgba(4,8,36,127));
        Tk_Para(fx0+2.0f,fy0+2.0f,fx1-2.0f,fy0+hh,sk,Ps_Rgba(255,204,70,128),Ps_Rgba(236,116,8,128));
        Ps_Align(0);
        Ps_Text(fx0+Tk_PX(120)-Tk_PX(0),Ps_TextTop(fy0+hh*0.5f,0.8f),(char *)Port_TagText("PREVIEW"),0.8f,Tk_PX(600),1);
        Ps_Align(1);
        Hud_LoadTagPreview();
        preview=Port_TagPreviewAt(id,(s32)Ps_X(fx0+Tk_PX(90)),(s32)Ps_Y(fy0+hh+Tk_PY(30)),(s32)Ps_X(fx1-Tk_PX(90)),(s32)Ps_Y(fy1-Tk_PY(170)));
        if(!preview) { /* no picture of it: its ball and its search rings */
            f32 br=Tk_PY(120),pulse=0.5f+0.5f*sinf((f32)sTkFrames*0.12f),cx=(fx0+fx1)*0.5f,cy=(fy0+hh+fy1-Tk_PY(170))*0.5f;
            Tk_Glow(cx-br*2.4f,cy-br*2.4f,cx+br*2.4f,cy+br*2.4f,255,180,50,0x60);
            Tk_Ring(cx,cy,br*(1.5f+0.1f*pulse),255,235,140,(s32)(0x30+0x30*pulse));
            Tk_Ball(cx,cy,br,cur%7+1);
        }
        if(!Port_TagLabelText(TagMenu_Label(tagPage,cur),(u32)label,sizeof(label))) label[0]=0;
        Ps_Align(1);
        Ps_Heading((fx0+fx1)*0.5f,Ps_TextTop(fy1-Tk_PY(90),1.1f),label,1.1f,fx1-fx0-Tk_PX(150)+Tk_PX(0),0x80);
    }
    /* the plates */
    first=cur-2;
    if(first>count-TM_SHOWN) first=count-TM_SHOWN;
    if(first<0) first=0;
    target=TM_Y0+TM_PITCH*(f32)(cur-first);
    if(sTkFrames-tmPageAt<=1) tmGlide=target;
    tmGlide+=(target-tmGlide)*0.55f;
    Ps_SetShift(slide);
    for(i=first;i<count && i<first+TM_SHOWN;i++) {
        f32 yt=i==cur?Tk_PY(tmGlide):Tk_PY(TM_Y0+TM_PITCH*(f32)(i-first)),yb=yt+ph,mid=(yt+yb)*0.5f;
        f32 x1=px0+pw+(i==cur && !pageOpts?Tk_PX(130)-Tk_PX(0):0.0f),lit=i==cur,labelX=px0+Tk_PX(150)-Tk_PX(0),room=x1-labelX-Tk_PX(60)+Tk_PX(0);
        if(i==cur) continue; /* (the lit one is drawn last, over the others) */
        Tk_Region(px0,yt,x1,yb,TK_ROW,i);
        Tk_Plate(px0,yt,x1,yb,0,i%7+1);
        if(!Port_TagLabelText(TagMenu_Label(tagPage,i),(u32)label,sizeof(label))) label[0]=0;
        Ps_Align(0);
        if(TagMenu_IsOption(i)) {
            f32 vw,cx1=x1-Tk_PX(70)+Tk_PX(0);
            Port_TagOptionValue(TagMenu_Option(tagPage,i),(u32)value,sizeof(value));
            vw=Ps_TextWidth(value,0.74f)+Tk_PX(150)-Tk_PX(0);
            Tk_Para(cx1-vw,yt+Tk_PY(14),cx1,yb-Tk_PY(14),Tk_PY(20)*0.4f,Ps_Rgba(24,44,112,126),Ps_Rgba(8,18,70,127));
            Ps_Align(1); Ps_Text(cx1-vw*0.5f,Ps_TextTop(mid,0.74f),value,0.74f,vw-Tk_PX(40)+Tk_PX(0),2); Ps_Align(0);
            room=cx1-vw-labelX-Tk_PX(20)+Tk_PX(0);
        }
        Tk_NavyText(labelX,Ps_TextTop(mid,0.9f),label,0.9f,room);
        Ps_Align(1);
        (void)lit;
    }
    { /* the lit plate, gliding to its row */
        f32 yt=Tk_PY(tmGlide),yb=yt+ph,mid=(yt+yb)*0.5f,x1=px0+pw+(!pageOpts?Tk_PX(130)-Tk_PX(0):0.0f),labelX=px0+Tk_PX(150)-Tk_PX(0),room=x1-labelX-Tk_PX(60)+Tk_PX(0);
        Tk_Region(px0,yt,x1,yb,TK_ROW,cur);
        Tk_Plate(px0,yt,x1,yb,1,cur%7+1);
        if(!Port_TagLabelText(TagMenu_Label(tagPage,cur),(u32)label,sizeof(label))) label[0]=0;
        Ps_Align(0);
        if(option) {
            f32 vw,cx1=x1-Tk_PX(70)+Tk_PX(0);
            Port_TagOptionValue(TagMenu_Option(tagPage,cur),(u32)value,sizeof(value));
            vw=Ps_TextWidth(value,0.78f)+Tk_PX(190)-Tk_PX(0);
            Tk_Para(cx1-vw,yt+Tk_PY(12),cx1,yb-Tk_PY(12),Tk_PY(24)*0.4f,Ps_Rgba(8,14,44,126),Ps_Rgba(4,8,30,127));
            Ps_Align(1); Ps_Text(cx1-vw*0.5f,Ps_TextTop(mid,0.78f),value,0.78f,vw-Tk_PX(100)+Tk_PX(0),1); Ps_Align(0);
            Tk_Region(cx1-vw,yt,cx1-vw*0.5f,yb,TK_PAD,PADG_LEFT);
            Tk_Region(cx1-vw*0.5f,yt,cx1,yb,TK_PAD,PADG_RIGHT);
            room=cx1-vw-labelX-Tk_PX(20)+Tk_PX(0);
        }
        Ps_Text(labelX,Ps_TextTop(mid,1.0f),label,1.0f,room,1);
        Ps_Align(1);
    }
    Ps_SetShift(0);
    if(first>0) { Tk_Arrow(Tk_PX(560),Tk_PY(TM_Y0-26),-1); Tk_Region(Tk_PX(470),Tk_PY(TM_Y0-48),Tk_PX(650),Tk_PY(TM_Y0),TK_PAD,PADG_UP); }
    if(first+TM_SHOWN<count) {
        f32 y=Tk_PY(TM_Y0+TM_PITCH*TM_SHOWN-6);
        Tk_Arrow(Tk_PX(560),y,1); Tk_Region(Tk_PX(470),y-12,Tk_PX(650),y+12,TK_PAD,PADG_DOWN);
    }
    { /* the bar at the foot: as wide as the list, a ball, what the row does (up to two lines), its rules as chips under them */
        f32 bx0=Tk_PX(70),by0=Tk_PY(986),bx1=Tk_PX(1160),by1=Tk_PY(1172),sk=(by1-by0)*0.18f,bp=bx0+Tk_PX(90)-Tk_PX(0),bm=(by0+by1)*0.5f;
        f32 room=bx1-bx0-Tk_PX(260)+Tk_PX(0),cx;
        Tk_Para(bx0-1.5f,by0-1.5f,bx1+1.5f,by1+1.5f,sk,Ps_Rgba(120,160,235,120),Ps_Rgba(90,130,220,110));
        Tk_Para(bx0,by0,bx1,by1,sk,Ps_Rgba(16,32,92,126),Ps_Rgba(4,8,36,127));
        Tk_Glow(bp-Tk_PY(66),bm-Tk_PY(66),bp+Tk_PY(66),bm+Tk_PY(66),255,190,60,0x50);
        Tk_Ball(bp,bm,Tk_PY(40),cur%7+1);
        TagMenu_Help(cur,help,sizeof(help));
        Tk_Wrap(help,0.74f,room);
        if(sTkLines>2) sTkLines=2;
        Tk_WrapDraw(bx0+Tk_PX(170)-Tk_PX(0),by0+Tk_PY(46),0.74f,room);
        cx=bx0+Tk_PX(160)-Tk_PX(0);
        if(tagPage==1 && !TagNet_HostMenu()) { /* its rules */
            char *rules[3];
            s32 n,k;
            TagMenu_RadarRules(cur,rules,&n);
            for(k=0;k<n;k++) cx=TagMenu_Chip(cx,by1-Tk_PY(72),(char *)Port_TagText(rules[k]));
        } else if(tagPage==2 && cur<5) { /* who plays: the players, the CPU for the rest */
            s32 humans=cur<4?cur+1:0,k;
            char sides[2][64],one[8];
            Ps_Copy(sides[0],(char *)Port_TagText("BLUE TEAM"),64); Ps_Cat(sides[0],":",64);
            Ps_Copy(sides[1],(char *)Port_TagText("RED TEAM"),64); Ps_Cat(sides[1],":",64);
            for(k=0;k<3;k++) { if(k<humans) sprintf(one," P%d",k+1); else Ps_Copy(one," CPU",sizeof(one)); Ps_Cat(sides[0],one,64); }
            for(k=0;k<3;k++) { if(k+3<humans) sprintf(one," P%d",k+4); else Ps_Copy(one," CPU",sizeof(one)); Ps_Cat(sides[1],one,64); }
            cx=TagMenu_Chip(cx,by1-Tk_PY(72),sides[0]);
            TagMenu_Chip(cx,by1-Tk_PY(72),sides[1]);
        }
    }
    if(!TagNet_HostMenu()) { /* the buttons, at the foot between the bar and the touch pad's buttons */
        f32 x=Tk_PX(1210);
        x=TagMenu_FootButton(x,0,option?"Change":"Enter",PADG_CROSS);
        x=TagMenu_FootButton(x,2,"Back",PADG_TRIANGLE);
        (void)in;
    }
}
#define MAINMENU_VOICED(item) (!tagPage && (item) < MAINMENU_TBO)
#else
#define MM_LABEL(item) (item)
#define MM_ICON(item) (item)
#define MAINMENU_VOICED(item) 1
#endif

/* Starts the idle loop of the seven dragon balls (all seven collected). */
void MainMenu_StartBallLoops(void) {
    MFlashRef ref;
    char name[64];
    s32 i;
    MFlash *flash = &gMainMenu->flash[0];

    for (i = 0; i < 7; i++) {
        sprintf(name, "mc_ch_a_db_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_loop");
    }
}

#ifdef PORT
/* An icon sheet of the menu marked to be drawn from its remastered picture (tbo/ui/menu_icons_<which>.png, four times
   the size; port/src/gs/gs_texpack.c's tagged textures, the same as Z Warriors' photos): the game animates, places and
   fades it as before. The three palette entries that carry the mark are taken from the picture first (its pixels of
   those colours get the nearest other one), so the sheet looks the same until the picture is in. Without the file,
   the sheet is left as it is. */
static void PortMainMenu_HdSheet(u8 *entry, s32 which) {
    extern int Port_ZwHdPath(u32 key, char *out, int max);
    static char path[800];
    u8 *pix, *clut, map[3];
    u32 *pal, mark[3];
    s32 size, i, k, v;

    if (!Port_ZwHdPath(0xFFFFF8u + (u32)which, path, sizeof(path))) return;
    pix = *(u8 **)(entry + 0x38);
    clut = *(u8 **)(entry + 0x3C);
    size = *(s32 *)(entry + 8) - 0x80; /* (the packet: 0x60 bytes before the data, 0x20 after) */
    if (pix == NULL || clut == NULL || size <= 0) return;
    pix += 0x60;
    pal = (u32 *)(clut + 0x60);
#define HD_AT(n) (((n) & 0xE7) | (((n) & 8) << 1) | (((n) & 0x10) >> 1))
    for (k = 0; k < 3; k++) {
        const u8 *want = (const u8 *)&pal[HD_AT(252 + k)];
        s32 best = 0x7FFFFFFF;
        for (v = 0; v < 252; v++) {
            const u8 *c = (const u8 *)&pal[HD_AT(v)];
            s32 d = 0;
            for (i = 0; i < 4; i++) d += (c[i] - want[i]) * (c[i] - want[i]);
            if (d < best) { best = d; map[k] = (u8)v; }
        }
    }
    for (i = 0; i < size; i++) {
        if (pix[i] >= 252 && pix[i] <= 254) pix[i] = map[pix[i] - 252];
    }
    mark[0] = 1u | 3u << 24;
    mark[1] = 0x015A5748u;
    mark[2] = (0xFFFFF8u + (u32)which) | 2u << 24;
    for (k = 0; k < 3; k++) pal[HD_AT(252 + k)] = mark[k];
#undef HD_AT
}
#endif

#define MM_RES(n) \
    res = (MTexRes *)MPACK_AT(gMainMenu->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 1), builds the item list and the windows. */
void MainMenu_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    if (gMenuArc1 == NULL) {
        gMenuArc1 = File_LoadSync(gProgress->baseFile + 1, NULL, 0);
    }
#ifdef PORT
    gMainMenu = Heap_Alloc(sizeof(MainMenu), 0x20, 0, 2); /* (its item list is longer on the PC: MAINMENU_ITEM_MAX) */
    memset(gMainMenu, 0, sizeof(MainMenu));
#else
    gMainMenu = Heap_Alloc(0x188, 0x20, 0, 2);
    memset(gMainMenu, 0, 0x188);
#endif
    gMainMenu->pack = (u32 *)MPACK_AT(gMenuArc1, section);
    gMainMenu->res = Sprite_Unpack(gMainMenu->pack, NULL, NULL);
    MM_RES(1);
    gMainMenu->bg = res;
    MM_RES(2);
    gMainMenu->tex[0] = MTEX(res, 0);
    gMainMenu->tex[1] = MTEX(res, 1);
    MM_RES(3);
    gMainMenu->tex[5] = MTEX(res, 0);
    gMainMenu->tex[4] = MTEX(res, 1);
    MM_RES(4);
    gMainMenu->tex[2] = MTEX(res, 0);
    gMainMenu->tex[3] = MTEX(res, 1);
    MM_RES(5);
    gMainMenu->tex[8] = MTEX(res, 0);
    MM_RES(6);
    gMainMenu->tex[12] = MTEX(res, 0);
#ifdef PORT
    if(PortGame_IsBT4()) {
        extern u32 Port_MainMenuSheets(u32,u32,s32);
        u32 off=(u32)gMainMenu->tex[8],on=(u32)gMainMenu->tex[12];
        gMainMenu->tex[8]=(u8 *)Port_MainMenuSheets(off,on,0);
        gMainMenu->tex[12]=(u8 *)Port_MainMenuSheets(on,on,1);
    }
#endif
    MM_RES(7);
    gMainMenu->tex[7] = MTEX(res, 0);
    gMainMenu->tex[11] = MTEX(res, 1);
#ifdef PORT
    PortMainMenu_HdSheet(MTEX(res, 0), 0); /* the icons at rest */
    PortMainMenu_HdSheet(MTEX(res, 1), 1); /* their animations */
    PortMainMenu_HdSheet(MTEX(res, 2), 2); /* Shenron's */
#endif
    MM_RES(8);
    gMainMenu->tex[6] = MTEX(res, 0);
    gMainMenu->tex[10] = MTEX(res, 1);
    gMainMenu->tex[9] = MTEX(res, 2);
    MM_RES(9);
    gMainMenu->tex[41] = MTEX(res, 0);
    MM_RES(10);
    gMainMenu->tex[43] = MTEX(res, 0);
    MM_RES(11);
    gMainMenu->tex[42] = MTEX(res, 0);
    MM_RES(12);
    gMainMenu->tex[44] = MTEX(res, 0);
    gMainMenu->tex[45] = MTEX(res, 1);
    gMainMenu->tex[46] = MTEX(res, 2);
    gMainMenu->tex[47] = MTEX(res, 3);
    gMainMenu->tex[48] = MTEX(res, 4);
    gMainMenu->tex[49] = MTEX(res, 5);
    MM_RES(13);
    gMainMenu->tex[22] = MTEX(res, 0);
#ifdef PORT
    /* B14 replaces the old guide artwork with three disc-authored banks.
       Its section 26 is empty; interpreting it as textures corrupts the
       following subtitle block and its display-list uploads. */
    MM_RES(PortGame_IsBT4()?30+2*Rand_Range(3):26);
#else
    MM_RES(26);
#endif
    gMainMenu->tex[27] = MTEX(res, 0);
    gMainMenu->tex[28] = MTEX(res, 1);
    gMainMenu->tex[29] = MTEX(res, 2);
    MM_RES(15);
    gMainMenu->tex[30] = MTEX(res, 0);
    gMainMenu->tex[13] = MTEX(res, 1);
    gMainMenu->tex[14] = MTEX(res, 2);
    gMainMenu->tex[15] = MTEX(res, 3);
#ifdef PORT
    MM_RES(PortGame_IsBT4()?29+2*Rand_Range(3):14);
#else
    MM_RES(14);
#endif
    gMainMenu->tex[31] = MTEX(res, 0);
    gMainMenu->tex[32] = MTEX(res, 1);
    gMainMenu->tex[33] = MTEX(res, 2);
    MM_RES(24);
    gMainMenu->tex[38] = MTEX(res, 0);
    gMainMenu->tex[39] = MTEX(res, 1);
    gMainMenu->tex[40] = MTEX(res, 2);
    MM_RES(25);
    gMainMenu->tex[19] = MTEX(res, 0);
    gMainMenu->tex[20] = MTEX(res, 1);
    gMainMenu->tex[21] = MTEX(res, 2);
    gMainMenu->tex[16] = MTEX(res, 3);
    gMainMenu->tex[17] = MTEX(res, 4);
    gMainMenu->tex[18] = MTEX(res, 5);
    MM_RES(16);
    gMainMenu->tex[23] = MTEX(res, 0);
    gMainMenu->tex[24] = MTEX(res, 2);
    MM_RES(17);
    gMainMenu->tex[25] = MTEX(res, 0);
    gMainMenu->tex[26] = MTEX(res, 2);
    MM_RES(18);
    gMainMenu->tex[35] = MTEX(res, 0);
    gMainMenu->tex[34] = MTEX(res, 2);
    MM_RES(19);
    gMainMenu->tex[37] = MTEX(res, 0);
    gMainMenu->tex[36] = MTEX(res, 2);
    Flash_Create(&gMainMenu->flash[0], MPACK_AT(gMainMenu->res, 20), gMainMenu->tex);
    Flash_Play(&gMainMenu->flash[0], 1);
    MM_RES(23);
    IconWin_Init(MPACK_AT(gMainMenu->res, 22), res);
    IconWin_Open();
    gMainMenu->msgText = MPACK_AT(gMainMenu->res, 28);
    gMainMenu->subtitles = MPACK_AT(gMainMenu->res, 27);
    MsgWin_Init(MPACK_AT(gMainMenu->res, 21), gMainMenu->msgText, 1, 0);
    MsgWin_Open();

    for (i = 0; i < MAINMENU_ITEM_MAX; i++) {
#ifdef PORT
        /* Item 4, "Dragon Net Battle", is the one entry the PS2 game never lists: it has its plate, label, icon and
           the guides' voiced lines on the disc, and no mode behind it (the Wii version's online mode). The port
           lists it only when the player asks for it (F1 > Online > Advanced: Port_NetMenuListed, port/src/headless.c):
           Tenkaichi Budokai Online, listed after Duel then, is the way into online play. BT3_MENU_ITEM4=1 lists it
           whatever the setting (tests: the pad recordings made while it was always listed count on that menu). */
        if (i == 4 && !Port_NetMenuListed()) {
            continue;
        }
        if (i == MAINMENU_TBO || i == MAINMENU_MOD_MANAGER || i == MAINMENU_ZW || i == MAINMENU_TAG) { /* (listed after the game's they follow, below) */
            continue;
        }
#else
        if (i == 4) {
            continue;
        }
#endif
        if (i == 10 && (gSaveData->unlockFlags & 0x7F) != 0x7F) {
            continue;
        }
        gMainMenu->items[gMainMenu->itemCount++] = i;
#ifdef PORT
        if (i == 3 && !Port_NetMenuListed() && !Port_TagAvailable()) { /* (Dragon Net Battle opens these screens now: listed alone only without it) */
            gMainMenu->items[gMainMenu->itemCount++] = MAINMENU_TBO;
        }
        /* (the Mod Manager item is not listed in this build: Z Warriors is the only addition to the main menu) */
        if (i == 5 && !PortGame_IsBT4()) { /* (Z Warriors is made of BT3's fighters) */
            gMainMenu->items[gMainMenu->itemCount++] = MAINMENU_ZW;
        }
        if (i == 5) {
            extern s32 Port_TagAvailable(void);

            if (Port_TagAvailable()) {
                gMainMenu->items[gMainMenu->itemCount++] = MAINMENU_TAG;
            }
        }
#endif
    }
#ifdef PORT
    /* the second sheet of each state (slot 8 not chosen, 12 chosen) is the entry behind the first */
    if (!PortMainMenu_WriteDisc(gMainMenu->tex[8] + 0x40, 3, 0) || !PortMainMenu_WriteDisc(gMainMenu->tex[12] + 0x40, 3, 1)) {
        PortMainMenu_WriteRuns(gMainMenu->tex[8] + 0x40, 3, sPortTboOff, sizeof(sPortTboOff) / sizeof(sPortTboOff[0]));
        PortMainMenu_WriteRuns(gMainMenu->tex[12] + 0x40, 3, sPortTboOn, sizeof(sPortTboOn) / sizeof(sPortTboOn[0]));
    }
    /* the Mod Manager: the first row of the third sheets */
    PortMainMenu_WriteRuns(PortMainMenu_AddSheet(8), 0, sPortModMgrOff, sizeof(sPortModMgrOff) / sizeof(sPortModMgrOff[0]));
    PortMainMenu_WriteRuns(PortMainMenu_AddSheet(12), 0, sPortModMgrOn, sizeof(sPortModMgrOn) / sizeof(sPortModMgrOn[0]));
    /* Z Warriors: the second row of the third sheets (in the disc's lettering only) */
    PortMainMenu_WriteZw(sPortSheetEnt[0] + 0x80, 1, 0);
    PortMainMenu_WriteZw(sPortSheetEnt[1] + 0x80, 1, 1);
    /* Tag Team: the third row */
    PortMainMenu_WriteTag(sPortSheetEnt[0] + 0x80, 2, 0);
    PortMainMenu_WriteTag(sPortSheetEnt[1] + 0x80, 2, 1);
#endif
    gMainMenu->top = gMainMenu->itemCount - 1;
    for (i = 0; i < gMainMenu->itemCount; i++) {
        if (gMainMenu->items[i] == gProgress->mainMenuItem) {
            gMainMenu->top = i - 1;
            if (gMainMenu->top < 0) {
                gMainMenu->top += gMainMenu->itemCount;
            }
            break;
        }
    }
    gMainMenu->voiceLine = -1;
    for (i = 0; i < 4; i++) {
        gMainMenu->blink[i] = Rand_Range(0x20);
    }
#ifdef PORT
    TagMenu_Prepare();
#endif
}

/* Frees the screen.
   The movies of a screen are an array (MainMenu_PlateGoto takes the index) of ONE, and Term / Update / Draw
   walk it with a loop. The loop compiles away, but it is what makes the original load the work pointer for the
   Flash call separately (`lw a0,%lo(gMainMenu)(s0)`) instead of sharing the address register with the code
   after it: without the loop the function does not match. */
void MainMenu_Term(void) {
    s32 i;
#ifdef PORT
    extern void GsDraw_ScreenScaleFloor(int floor4); /* port/src/gs/gs_draw.c */

    GsDraw_ScreenScaleFloor(0);
#endif

    MsgWin_Term();
    IconWin_Term();
    for (i = 0; i < MAINMENU_FLASH_NUM; i++) {
        Flash_Destroy(&gMainMenu->flash[i]);
    }
    if (gMainMenu->res != NULL) {
        Heap_Free(gMainMenu->res);
        gMainMenu->res = NULL;
    }
#ifdef PORT
    for (i = 0; i < 2; i++) {
        if (sPortSheetPix[i] != NULL) {
            Heap_Free(sPortSheetPix[i]);
            sPortSheetPix[i] = NULL;
        }
    }
#endif
    if (gMainMenu != NULL) {
        Heap_Free(gMainMenu);
        gMainMenu = NULL;
    }
}

#ifdef PORT
/* Z Warriors' and Tag Team's icons are films of their own. The game's icon clip is given an empty corner of its sheet,
   and the box of the screen it is drawn on is measured while it is (its "before" and "draw over" callbacks around
   gPortFlashProbe, sys/gfxm_c.c); the film is drawn over that box once the movie is done, so that nothing of ours
   changes the renderer's state in the middle of it. The clips are reused by every row: each is set every frame. */
extern f32 *gPortFlashProbe;
static f32 sIconAt[16][7]; /* the box x0 y0 x1 y1, its alpha, which (1 the ball, 2 Tag Team's), 1 the chosen row's */
static s32 sIconCount;

static void PortMainMenu_IconBegin(void *arg) {
    if (sIconCount < 16) {
        f32 *at = sIconAt[sIconCount];

        at[0] = at[1] = 1.0e6f;
        at[2] = at[3] = -1.0e6f;
        at[4] = 0.0f;
        gPortFlashProbe = at;
    }
}

static void PortMainMenu_IconNote(void *arg, f32 *prop) {
    if (gPortFlashProbe != NULL && sIconCount < 16) {
        f32 *at = sIconAt[sIconCount];

        gPortFlashProbe = NULL;
        if (at[2] > at[0] && at[3] > at[1] && at[4] > 0.0f) {
            at[5] = (f32)((s32)arg & 0xFF);
            at[6] = ((s32)arg & 0x100) ? 1.0f : 0.0f;
            sIconCount++;
        }
    }
}

/* 1 if the row's icon is one of the films (its clips' pictures are then emptied). */
static s32 PortMainMenu_FilmIcon(MFlash *flash, char *plate, s32 item) {
    extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
    extern void Flash_ClipSetCallbackC(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
    s32 which = tagPage ? 0 : item == 13 /* Z Warriors */ ? 1 : item == 14 /* Tag Team */ ? 2 : 0;
    MFlashUv empty = {0};
    MFlashRef ref;
    s32 k;

    empty.x1 = 1;
    empty.y1 = 1;
    for (k = 0; k < 2; k++) {
        Flash_FindLabel(flash, plate, k ? "mc_icon_play" : "mc_icon_off", &ref);
        if (which != 0) {
            Flash_ClipSetUv(flash, &ref, &empty);
            Flash_ClipSetCallbackA(flash, &ref, (void *)PortMainMenu_IconBegin, NULL);
            Flash_ClipSetCallbackC(flash, &ref, (void *)PortMainMenu_IconNote, (void *)(which | (k ? 0x100 : 0)));
        } else {
            Flash_ClipSetCallbackA(flash, &ref, NULL, NULL);
            Flash_ClipSetCallbackC(flash, &ref, NULL, NULL);
        }
    }
    return which != 0;
}

static void PortMainMenu_DrawIcons(void) {
    extern s32 Port_PageAspectMilli(void);
    f32 aspect = (f32)Port_PageAspectMilli() / 1000.0f;
    s32 i;

    for (i = 0; i < sIconCount; i++) {
        f32 *at = sIconAt[i];
        s32 chosen = at[6] != 0.0f;
        s32 ball = at[5] == 1.0f;
        f32 x = (at[0] + at[2]) * 0.5f, y = (at[1] + at[3]) * 0.5f;
        /* heights in the movie's lines, as large as the game's own icons (not from the clip's box, whose size differs
           between the two states). The Scouter's picture is wider than tall: a larger square for the same weight. */
        f32 h = ball ? (chosen ? 35.0f : 31.0f) : (chosen ? 45.0f : 40.0f);
        f32 w = h * (512.0f / 448.0f) / aspect;          /* (round on the window, whatever its shape) */
        s32 alpha = at[4] > 128.0f ? 128 : (s32)at[4]; /* (the GS's opaque is 0x80) */

        {
            static s32 told[4];
            s32 n = (ball ? 0 : 1) + (chosen ? 2 : 0);

            if (!told[n]) {
                told[n] = 1;
                fprintf(stderr, "bt3: menu icon %d %s: box %d,%d %d,%d\n", (int)at[5], chosen ? "chosen" : "other", (int)at[0], (int)at[1],
                        (int)at[2], (int)at[3]);
            }
        }
        /* the film plays only under the cursor; elsewhere the picture stands still and a little faded */
        Ps_SpriteScreen(chosen ? (ball ? MENU_BALL : MENU_RADAR) : (ball ? MENU_BALL_STILL : MENU_RADAR_STILL), x - w * 0.5f,
                        y - h * 0.5f, x + w * 0.5f, y + h * 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, chosen ? PS_PLAIN : 0x666666,
                        chosen ? alpha : alpha * 7 / 8);
    }
    sIconCount = 0;
}

#endif

/* Draws the background, sets up every clip of the movie for this frame and draws it and the two windows. */
void MainMenu_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlashRef eye;
    MFlashRef mouth;
    s32 i;
    s32 item;
    MFlash *flash;
#ifdef PORT
    MFlashRef hidden[9];
    s32 visible[9];
#endif

    Sprite_DrawPicture(gMainMenu->bg, 0, 0, 0x80);
    flash = &gMainMenu->flash[0];
    for (i = 0; i < 4; i++) {
        switch (i) {
        case 0:
            Flash_FindLabel(flash, NULL, "mc_guide_gohan_eye", &eye);
            Flash_FindLabel(flash, NULL, "mc_guide_gohan_mouth", &mouth);
            break;
        case 1:
            Flash_FindLabel(flash, NULL, "mc_guide_bidel_eye", &eye);
            Flash_FindLabel(flash, NULL, "mc_guide_bidel_mouth", &mouth);
            break;
        case 3:
            Flash_FindLabel(flash, NULL, "mc_guide_torankusu_eye", &eye);
            Flash_FindLabel(flash, NULL, "mc_guide_torankusu_mouth", &mouth);
            break;
        case 2:
            Flash_FindLabel(flash, NULL, "mc_guide_goten_eye", &eye);
            Flash_FindLabel(flash, NULL, "mc_guide_goten_mouth", &mouth);
            break;
        }
        FlashAnim_Blink(flash, &eye, &gMainMenu->blink[i], 0);
        if (gMainMenu->guide == i) {
            FlashAnim_Talk(flash, &mouth, &gMainMenu->talk[i], 0);
        } else {
            FlashAnim_ShowNext2(flash, &mouth, 0);
        }
    }

    {
        /* columns of each item's animated icon sheet */
        s32 cols[MAINMENU_ITEM_MAX] = { 4, 4, 4, 5, 5, 5, 5, 4, 5, 5, 4 };

        for (i = 0; i < 6; i++) {
            item = gMainMenu->items[(gMainMenu->top + i) % gMainMenu->itemCount];
            uv.x0 = 0;
            uv.x1 = 0x200;
            uv.y0 = (MM_LABEL(item) % 8) * 0x20;
            uv.y1 = uv.y0 + 0x20;
            sprintf(name, "mc_menu_plate_%d", i + 1);
            Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, MM_LABEL(item) / 8);
            Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, MM_LABEL(item) / 8);
            uv.x0 = (MM_ICON(item) % 4) * 0x40;
            uv.y0 = (MM_ICON(item) / 4) * 0x40;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = uv.y0 + 0x40;
            Flash_FindLabel(flash, name, "mc_icon_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            uv.x0 = 0;
            uv.y0 = (MM_ICON(item) % 10) * 0x30;
            uv.x1 = 0x30;
            uv.y1 = uv.y0 + 0x30;
            Flash_FindLabel(flash, name, "mc_icon_play", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, MM_ICON(item) / 10);
#ifdef PORT
            if (PortMainMenu_FilmIcon(flash, name, item)) {
                /* (a film: no sheet animation of the game's) */
            } else
#endif
            if (item == MAINMENU_CUR(gMainMenu)) {
                FlashAnim_Sheet(flash, &ref, &gMainMenu->iconTimer, &gMainMenu->iconFrame, &uv,
                                cols[MM_ICON(MAINMENU_CUR(gMainMenu))], 1, MM_ICON(MAINMENU_CUR(gMainMenu)) == 3 ? 3 : 6);
            }
        }
    }

    {
        item = gMainMenu->items[gMainMenu->extra];
        uv.x1 = 0x200;
        uv.x0 = 0;
        uv.y0 = (MM_LABEL(item) % 8) * 0x20;
        uv.y1 = uv.y0 + 0x20;
        Flash_FindLabel(flash, "mc_menu_plate_7", "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetTex(flash, &ref, MM_LABEL(item) / 8);
        Flash_FindLabel(flash, "mc_menu_plate_7", "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetTex(flash, &ref, MM_LABEL(item) / 8);
        uv.x0 = (MM_ICON(item) % 4) * 0x40;
        uv.y0 = (MM_ICON(item) / 4) * 0x40;
        uv.x1 = uv.x0 + 0x40;
        uv.y1 = uv.y0 + 0x40;
        Flash_FindLabel(flash, "mc_menu_plate_7", "mc_icon_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        uv.x0 = 0;
        uv.y0 = (MM_ICON(item) % 10) * 0x30;
        uv.x1 = 0x30;
        uv.y1 = uv.y0 + 0x30;
        Flash_FindLabel(flash, "mc_menu_plate_7", "mc_icon_play", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetTex(flash, &ref, MM_ICON(item) / 10);
#ifdef PORT
        PortMainMenu_FilmIcon(flash, "mc_menu_plate_7", item);
#endif
    }

    for (i = 0; i < 2; i++) {
        uv.x0 = i * 0x20;
        uv.y0 = 0x20;
        uv.x1 = uv.x0 + 0x20;
        uv.y1 = 0x40;
        Flash_FindLabel(flash, NULL, i ? "mc_menu_yajirusi_down" : "mc_menu_yajirusi_up", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    for (i = 0; i < 7; i++) {
        sprintf(name, "mc_ch_a_db_%d", i + 1);
        if (gSaveData->unlockFlags & (s32)(1U << i)) {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            uv.x0 = (i % 4) * 0x40;
            uv.y0 = (i / 4) * 0x40;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = uv.y0 + 0x40;
            Flash_FindLabel(flash, name, "mc_dragonball", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
        } else {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }

    for (i = 0; i < 2; i++) {
        uv.y0 = 0;
        uv.y1 = 0x100;
        uv.x0 = 0;
        uv.x1 = 0x200;
        Flash_FindLabel(flash, NULL, i ? "mc_bg_cloud" : "mc_bg_cloud_a", &ref);
        FlashAnim_Scroll(flash, &ref, &uv, &gMainMenu->cloud[i], NULL, i ? -0.18962963f : -0.35555556f, 0.0f);
    }

#ifdef PORT
    if(tagPage==3) {
        extern s32 Flash_ClipIsVisible(MFlash *,MFlashRef *);
        for(i=0;i<9;i++) {
            if(i<7) sprintf(name,"mc_menu_plate_%d",i+1);
            else Ps_Copy(name,i==7?"mc_menu_yajirusi_up":"mc_menu_yajirusi_down",sizeof(name));
            Flash_FindLabel(flash,NULL,name,&hidden[i]);
            visible[i]=Flash_ClipIsVisible(flash,&hidden[i]);
            Flash_ClipSetFlags(flash,&hidden[i],2,0);
        }
    }
#endif
#ifdef PORT
    {   /* Tag Team's own pages: at least 4x whatever the player's resolution, as Dragon Net's screens */
        extern void GsDraw_ScreenScaleFloor(int floor4); /* port/src/gs/gs_draw.c */

        GsDraw_ScreenScaleFloor(tagPage ? 16 : 0);
    }
    sIconCount = 0;
#endif
    for (i = 0; i < MAINMENU_FLASH_NUM; i++) {
        Flash_Draw(&gMainMenu->flash[i]);
    }
#ifdef PORT
    PortMainMenu_DrawIcons();
#endif
#ifdef PORT
    if(tagPage==3) for(i=0;i<9;i++) Flash_ClipSetFlags(flash,&hidden[i],2,visible[i]);
    if(!tagPage)
#endif
    IconWin_Draw();
#ifdef PORT
    if(!tagPage)
#endif
    MsgWin_Draw(0, 0, gMainMenu->voiceLine);
#ifdef PORT
    if(Port_TagAvailable()) {
        Tk_Begin(); Ps_Align(1);
        if(tagPage==3) TagSetup_Draw();
        else {
            if(tagPage) { Tk_Backdrop(); TagMenu_DrawPage(); }
            if(TagNet_HostMenu()) Ps_Text(256,Ps_TextTop(Tk_PY(1180),0.65f),TagNet_HostChoosing()?"BRINGING GUESTS TO BATTLE SETUP":"CHOOSE A MODE FOR THE LOBBY",0.65f,470,3);
            ;
            if(TagNet_HostMenu())TagLobbyRoster_Draw(1);
        }
    }
#endif
}

/* Sends the plate under the cursor of movie `idx` to a label. */
void MainMenu_PlateGoto(s32 idx, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gMainMenu->flash[idx];

    sprintf(name, "mc_menu_plate_%d", gMainMenu->cursor + 2);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Per frame: when the guide's greeting has ended, lets a random guide read out the current item; steps the movie. */
void MainMenu_Update(void) {
    s32 i;

    if (gMainMenu->flags & MAINMENU_GREETED) {
        if (gMainMenu->voiceLine == gMainMenu->guide + 0xC || gMainMenu->voiceLine == gMainMenu->guide + 0x3C) {
            if (Voice_GetStat(0) == 5) {
                gMainMenu->guide = Rand_Range(4);
                if (MAINMENU_VOICED(MAINMENU_CUR(gMainMenu))) {
                    gMainMenu->voiceLine = MAINMENU_CUR(gMainMenu) * 4 + gMainMenu->guide + 0x10;
                    Voice_PlayWithSubtitle(gMainMenu->subtitles, MAINMENU_VOICE_BASE, gMainMenu->voiceLine);
                } else {
                    gMainMenu->voiceLine = -1;
                }
            }
        }
    }
    for (i = 0; i < MAINMENU_FLASH_NUM; i++) {
        Flash_Advance(&gMainMenu->flash[i]);
    }
}

/* What a choice does: marks the screen as leaving, sets the next mode and plays the movie out. */
#define MAINMENU_CHOOSE(next) \
    gMainMenu->flags |= MAINMENU_CHOSEN; \
    gMainMenu->flags |= MAINMENU_LEAVING; \
    gMainMenu->timer = 15; \
    gProgress->mode = (next); \
    Flash_GotoLabel(&gMainMenu->flash[0], "fl_out", 1)

/* Counts the choices made on the main menu in the save, 0..23. */
#define MAINMENU_COUNT() \
    gSaveData->unkA0C++; \
    if (gSaveData->unkA0C >= 24) { \
        gSaveData->unkA0C = 0; \
    }

#ifdef PORT
/* What the radar's ball (the modes' page's row) `item` does: as the list's Cross did before the radar. */
static void TagMenu_RadarChoose(s32 item) {
    if(TagNet_HostMenu()) {
        if(item<3)TagNet_ChoosePreset(item,0);
        else if(item<5){tagMode=item;presetPage=0;TagMenu_SetPage(TAG_MENU_PRESET);}
        else if(item==5)TagMenu_SetPage(4);
        else TagNet_Leave();
    }
    else if(item==TagMenu_Count(1)-1) TagMenu_SetPage(0);
    else if(item==3 || item==4) { tagMode=item==3?TAG_MODE_GAUNTLET:TAG_MODE_TOURNAMENT; TagMenu_SetPage(2); }
    else if(item==5) {
        TagSession_Leave();gPortScreen=PORT_SCREEN_TAG_LOBBY;
        MAINMENU_CHOOSE(MPROG_MODE_PORT_SCREEN);
        MainMenu_PlateGoto(0,"fl_ok");
    }
    else if(item==6) TagMenu_SetPage(4);
    else { tagMode=item; TagMenu_SetPage(2); }
}
#endif

/* Pad 0: up / down move the cursor (the ring scrolls at the ends), confirm enters a mode, cancel leaves. */
void MainMenu_Input(s32 *result) {
#ifdef PORT
    { extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);
      if(tagPage!=3) Port_TagMenuTrace(4,tagPage,TagMenu_Cur(),tagLabelsReady,gMainMenu->flash[0].flags,gPad[0].gamePressed); }
    if(TagNet_HostMenu()) {
        extern s32 TagLobbyRoster_Input(void);
        if(TagNet_HostPoll()) {gPortScreen=PORT_SCREEN_TAG_LOBBY;MAINMENU_CHOOSE(MPROG_MODE_PORT_SCREEN);return;}
        if(TagNet_HostChoosing())return;
        if(TagLobbyRoster_Input())return;
    }
    /* The radar locking on a ball: nothing else is taken until it ends; then the mode opens. (Dropped if the page is
       no longer the radar's, or an online room's host menu came or went under it: its choices are another list.) */
    if(tmLockAt) {
        if(tagPage!=1 || (gMainMenu->flags & MAINMENU_LEAVING) || tmLockHost!=(TagNet_HostMenu()!=0)) { tmLockAt=0; }
        else if(sTkFrames-tmLockAt>=TR_LOCK_FRAMES) {
            s32 item=tmLockRow;
            tmLockAt=0;
            if(item>=0 && item<TagMenu_Count(1)) { TagMenu_RadarChoose(item); Snd_PlaySe(1,4); }
            return;
        }
        else return;
    }
    /* A tap (menu/tag_kit.h): a row is chosen and entered, a button at the foot pressed (the setup page takes its own) */
    if(tagPage!=3) {
        s32 kind,value;
        if(Tk_Tap(&kind,&value)) {
            if(kind==TK_LANG) { extern void Port_TagLangSet(int); Port_TagLangSet(value); Snd_PlaySe(1,1); return; }
            if(kind==TK_PAD) { gPad[0].gamePressed|=value; gPad[0].gameRepeat|=value; }
            else if(tagPage && value>=0 && value<TagMenu_Count(tagPage)) {
                if(!TagMenu_IsOption(value)) { TagMenu_Goto(value); gPad[0].gamePressed|=PADG_CROSS; }
                else if(value==TagMenu_Cur()) gPad[0].gameRepeat|=PADG_RIGHT; /* (a setting: the next value) */
                else { TagMenu_Goto(value); Snd_PlaySe(1,0); return; }
            }
        }
    }
    if(tagPage==3) {
        s32 action=TagSetup_Input();
        if(action<0) TagMenu_SetPage(tagMode==TAG_MODE_GAUNTLET || tagMode==TAG_MODE_TOURNAMENT?TAG_MENU_PRESET:2);
        if(action>0) { Snd_PlaySe(1,1); MAINMENU_CHOOSE(40); }
        return;
    }
    if(tagPage!=TAG_MENU_PRESET && tagPage>=5 && (gPad[0].gameRepeat&(PADG_LEFT|PADG_RIGHT))) {
        TagMenu_SettingChange(TagMenu_Cur(),(gPad[0].gameRepeat&PADG_LEFT)?-1:1); return;
    }
    if(tagPage==1 && (gPad[0].gamePressed&PADG_SQUARE)) { /* the language: PT, EN, ES in turn */
        extern int Port_TagLang(void);
        extern void Port_TagLangSet(int);
        s32 lang=Port_TagLang(); /* (0 English, 1 Portuguese, 2 Spanish; in the switch's order PT, EN, ES) */
        Port_TagLangSet(lang==1?0:lang==0?2:1);
        Snd_PlaySe(1,1); return;
    }
    /* up / down on the mod's pages at once (the game's plates are not shown there: no waiting for their movie) */
    if(tagPage && !(gPad[0].gamePressed&(PADG_CROSS|PADG_TRIANGLE)) && (gPad[0].gameRepeat&(PADG_UP|PADG_DOWN)) && TagMenu_Count(tagPage)>0) {
        s32 n=TagMenu_Count(tagPage);
        TagMenu_Goto((TagMenu_Cur()+((gPad[0].gameRepeat&PADG_UP)?n-1:1))%n);
        Snd_PlaySe(1,0); return;
    }
#endif
    if (!(gMainMenu->flash[0].flags & MFLASH_PAD)) {
        return;
    }
#ifdef PORT
    {   /* In a lobby (joined from the Dragon Net Battle window, or back here after its match): its screen, in
           Tenkaichi Budokai Online (menu/port_tbo.c; port/src/gs/net_lobby.c) */
        extern int Port_LobbyWantsScreen(void);

        if (!(gMainMenu->flags & MAINMENU_LEAVING) && Port_LobbyWantsScreen()) {
            gPortScreen = PORT_SCREEN_TBO;
            MAINMENU_CHOOSE(MPROG_MODE_PORT_SCREEN);
            return;
        }
    }
#endif
    if (!(gMainMenu->flags & MAINMENU_STARTED)) {
        MainMenu_PlateGoto(0, "fl_on_start");
        if ((gSaveData->unlockFlags & 0x7F) == 0x7F) {
            MainMenu_StartBallLoops();
        }
        gMainMenu->flags |= MAINMENU_STARTED;
    }
    if (gPad[0].gameRepeat & 8) {
        MainMenu_PlateGoto(0, "fl_off_start");
        gMainMenu->iconTimer = 0;
        gMainMenu->iconFrame = 0;
        gMainMenu->cursor--;
        if (gMainMenu->cursor < 0) {
            gMainMenu->cursor = 0;
            Flash_GotoLabel(&gMainMenu->flash[0], "fl_menu_down", 1);
            gMainMenu->top--;
            if (gMainMenu->top < 0) {
                gMainMenu->top += gMainMenu->itemCount;
            }
            gMainMenu->extra = (gMainMenu->top + 6) % gMainMenu->itemCount;
        }
        goto moved;
    } else if (gPad[0].gameRepeat & 4) {
        MainMenu_PlateGoto(0, "fl_off_start");
        gMainMenu->iconTimer = 0;
        gMainMenu->iconFrame = 0;
        gMainMenu->cursor++;
        if (gMainMenu->cursor >= MAINMENU_ROWS) {
            gMainMenu->cursor = MAINMENU_ROWS - 1;
            Flash_GotoLabel(&gMainMenu->flash[0], "fl_menu_up", 1);
            gMainMenu->extra = gMainMenu->top;
            gMainMenu->top++;
            if (gMainMenu->top >= gMainMenu->itemCount) {
                gMainMenu->top -= gMainMenu->itemCount;
            }
        }
    moved:
        MainMenu_PlateGoto(0, "fl_on_start");
        gMainMenu->guide = Rand_Range(4);
        if (MAINMENU_VOICED(MAINMENU_CUR(gMainMenu))) {
            gMainMenu->voiceLine = MAINMENU_CUR(gMainMenu) * 4 + gMainMenu->guide + 0x10;
            Voice_PlayWithSubtitle(gMainMenu->subtitles, MAINMENU_VOICE_BASE, gMainMenu->voiceLine);
        } else {
            gMainMenu->voiceLine = -1;
        }
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gamePressed & 0x200) {
#ifdef PORT
        if(tagPage) {
            s32 item=TagMenu_Cur();
            if(tagPage==TAG_MENU_PRESET) {
                s32 n=preset_rows(),pages=(preset_count()+7)/8;
                if(item<n) {
                    if(tagMode==TAG_MODE_GAUNTLET)TagChallenge_Choose(presetPage*8+item);
                    else TagTournament_Choose(item);
                    if(TagNet_HostMenu())TagNet_ChoosePreset(tagMode,presetPage*8+item);
                    else TagMenu_SetPage(3);
                } else if(item==n+2) TagMenu_SetPage(TagNet_HostMenu()?1:2);
                else {presetPage=(presetPage+(item==n?-1:1)+pages)%pages;TagMenu_SetPage(TAG_MENU_PRESET);}
            } else if(tagPage>=5) {
                if(item==TagMenu_Count(tagPage)-1) TagMenu_SetPage(4);
                else TagMenu_SettingChange(item,1);
            } else if(tagPage==4) TagMenu_SetPage(item==TAG_OPTION_GROUP_COUNT?1:5+item);
            else if(tagPage==1) {
                tmRadarRow=item; TagMenu_RadarChoose(item);
            }
            else if(item==TagMenu_Count(tagPage)-1) TagMenu_SetPage(tagPage==2?1:0);
            else if(tagPage==2) {
                TagSession_Setup(tagMode,item==4?0:item+1);
                presetPage=0;TagMenu_SetPage(tagMode==TAG_MODE_GAUNTLET || tagMode==TAG_MODE_TOURNAMENT?TAG_MENU_PRESET:3);
            } else if(item==3 || item==4) {
                tagMode=item==3?TAG_MODE_GAUNTLET:TAG_MODE_TOURNAMENT;TagMenu_SetPage(2);
            } else if(item==5) {
                TagSession_Leave();gPortScreen=PORT_SCREEN_TAG_LOBBY;
                MAINMENU_CHOOSE(MPROG_MODE_PORT_SCREEN);
                MainMenu_PlateGoto(0,"fl_ok");
            } else if(item==6) {
                TagMenu_SetPage(4);
            } else { tagMode=item; TagMenu_SetPage(2); }
            Snd_PlaySe(1,1); return;
        }
#endif
        switch (MAINMENU_CUR(gMainMenu)) {
        case 0:
            MAINMENU_CHOOSE(6);
            MAINMENU_COUNT();
            break;
        case 1:
            MAINMENU_CHOOSE(13);
            MAINMENU_COUNT();
            break;
        case 2:
            MAINMENU_CHOOSE(33);
            MAINMENU_COUNT();
            break;
        case 3:
            MAINMENU_CHOOSE(38);
#ifdef PORT
            { extern void TagSession_Leave(void); TagSession_Leave(); }
#endif
            MAINMENU_COUNT();
            break;
        case 4:
#ifdef PORT
            /* Dragon Net Online: the screens of menu/port_tbo.c (Quick Match, Ranked, rooms, lobbies, against
               Android + Switch or the PC), in the port's screen mode */
            gPortScreen = PORT_SCREEN_TBO;
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(MPROG_MODE_PORT_SCREEN);
#endif
            break;
        case 5:
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(48);
            break;
        case 6:
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(44);
            break;
        case 7:
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(53);
            break;
        case 8:
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(60);
            break;
        case 9:
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(62);
            break;
        case 10:
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(70);
            break;
#ifdef PORT
        case MAINMENU_TBO: /* "Tenkaichi Budokai Online": the port's screen mode runs its screens (menu/port_tbo.c) */
            { extern s32 Port_TagAvailable(void);
              gPortScreen = Port_TagAvailable()?PORT_SCREEN_TAG_TEAM:PORT_SCREEN_TBO; }
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(MPROG_MODE_PORT_SCREEN);
            break;
        case MAINMENU_MOD_MANAGER: /* the Duel mode runs the manager's screen (menu_g_b.c) */
            gPortDuelRole = 2;
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(38);
            break;
        case MAINMENU_TAG: /* the Tag Team mod: its own pages, over this menu */
            { extern void TagSession_Leave(void); TagSession_Leave(); }
            if (tagLabelsReady) {
                TagMenu_SetPage(1);
                Snd_PlaySe(1, 0);
            } else {
                Snd_PlaySe(1, 2);
            }
            return;
        case MAINMENU_ZW: /* the Duel mode runs the Z Warriors screen (menu_g_b.c) */
            gPortDuelRole = 3;
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(38);
            break;
#endif
        }
        MainMenu_PlateGoto(0, "fl_ok");
        Snd_PlaySe(1, 1);
    } else if (gPad[0].gamePressed & 0x400) {
#ifdef PORT
        if(tagPage==1 && TagNet_HostMenu()){Snd_PlaySe(1,2);return;}
        if(tagPage) { TagMenu_SetPage(tagPage==TAG_MENU_PRESET?(TagNet_HostMenu()?1:2):tagPage>=5?4:tagPage==4?1:tagPage==3?2:tagPage==2?1:0); Snd_PlaySe(1,2); return; }
#endif
        Flash_GotoLabel(&gMainMenu->flash[0], "fl_cancel", 1);
        IconWin_Close();
        MsgWin_Close();
        ColorFade_StartOut(0, 0, 0, 0x14);
        *result = 0;
        Snd_PlaySe(1, 2);
    }
}

/* The main menu's own frame loop. Returns 1 when a mode was chosen (gProgress->mode is set), 0 on cancel. */
s32 MainMenu_Run(s32 section) {
    s32 result = 1;

#ifdef PORT
    if (Port_NetSession()) {
        { /* Dragon Net (flow:2): the Revanche screen (port/src/gs/ui.cpp) comes before the session ends */
            extern volatile int gPortRematchRequest, gPortRematchDone;
            extern int DrRanked_RematchOffer(void);
            if (DrRanked_RematchOffer()) {
                int frames;
                gPortRematchDone = 0;
                gPortRematchRequest = 1;
                for (frames = 0; frames < 3000 && !gPortRematchDone; frames++) {
                    Gfx_BeginFrame();
                    Pad_Update();
                    Snd_Update();
                    Gfx_EndFrame(1);
                    Dma_Flush();
                }
            }
        }
        Port_NetLeave(); /* an online session lives in the versus mode: back at the main menu it is over (no return) */
    }
#endif
    MainMenu_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            MainMenu_Update();
        }
        MainMenu_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gMainMenu->flags & MAINMENU_GREETED) && (gMainMenu->flash[0].flags & MFLASH_PAD)) {
                gMainMenu->flags |= MAINMENU_GREETED;
                gMainMenu->guide = Rand_Range(4);
                if ((gSaveData->unlockFlags & 0x7F) == 0x7F) {
                    gMainMenu->voiceLine = gMainMenu->guide + 0x3C;
                } else {
                    gMainMenu->voiceLine = gMainMenu->guide + 0xC;
                }
                Voice_PlayWithSubtitle(gMainMenu->subtitles, MAINMENU_VOICE_BASE, gMainMenu->voiceLine);
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gMainMenu->flags & MAINMENU_LEAVING) {
            if (--gMainMenu->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
#ifdef PORT
                gProgress->mainMenuItem = tagPage ? stockItems[(stockTop+stockCursor+1)%stockCount] : MAINMENU_CUR(gMainMenu);
#else
                gProgress->mainMenuItem = MAINMENU_CUR(gMainMenu);
#endif
            }
        } else {
            MainMenu_Input(&result);
        }
    }
    sceGsSyncPath(0, 0);
    MainMenu_Term();
    Dma_ResetBuffers();
    return result;
}
