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
#include "menu/tag_controllers.h"
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
    return tagPage>=4 ? 9 : tagPage==3 ? 3 : tagPage ? icons[item%6] : item==MAINMENU_TBO?4:item;
}
#define MM_ICON(item) tag_icon(item)

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
    if(page==3) TagSetup_Reset();
    for(lit=0;lit<2;lit++) for(row=0;row<(page?count:12);row++) {
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
}
static void TagMenu_Prepare(void) {
    extern s32 Port_MainTagLabel(s32,s32,s32,s32);
    s32 lit,row,x,y,ok=1;
    if(!Port_TagAvailable()) { tagPage=0; return; }
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
static void TagMenu_Guide(void) {
    static char descriptions[][96]={"Fight with your own teams and player assignments.",
        "Every fighter for themselves. Last fighter standing wins.",
        "Practice with teams and adjustable training rules.",
        "Survive successive waves of enemies with your team.",
        "Enter your teams in a tournament bracket.",
        "Practice together while the host chooses your next mode.",
        "Change the mod's gameplay, cameras and HUD settings.","Return to the previous menu."};
    s32 row=MAINMENU_CUR(gMainMenu);
    char value[96],heading[64];
    if(tagPage==TAG_MENU_PRESET) Ps_Copy(heading,tagMode==TAG_MODE_GAUNTLET?"CHOOSE GAUNTLET":"CHOOSE TOURNAMENT",sizeof(heading));
    else if(tagPage>=5) Ps_Copy(heading,tagOptionGroups[tagPage-5],sizeof(heading));
    else Ps_Copy(heading,tagPage==4?"MOD SETTINGS":tagPage==3?"BATTLE SETUP":tagPage==2?"PLAYERS":"MODDED MODES",sizeof(heading));
    Ps_Heading(390,14,heading,0.75f,225,128);
    Ps_Rect(10,342,502,414,Ps_Rgba(12,49,65,115),Ps_Rgba(12,49,65,115));
    if(tagPage!=TAG_MENU_PRESET && tagPage>=5 && row<TagMenu_Count(tagPage)-1) {
        s32 id=TagMenu_Option(tagPage,row),v=Port_TagOption(id);
        { extern void Port_TagOptionValue(s32,u32,s32),Port_TagPreview(s32),Hud_LoadTagPreview(void);
          Hud_LoadTagPreview();
          Port_TagOptionValue(id,(u32)value,sizeof(value));Port_TagPreview(id); }
        Ps_Text(256,350,value,1.0f,472,3);
        Ps_Text(256,376,tagOptionInfo[id].help,0.7f,472,3);
        Ps_Text(256,398,"LEFT / RIGHT - CHANGE",0.6f,472,3);
    } else {
        char message[128];
        Ps_Copy(message,descriptions[7],sizeof(message));
        if(tagPage==TAG_MENU_PRESET) {sprintf(message,Port_TagText("Choose a challenge. Page %d of %d."),presetPage+1,(preset_count()+7)/8);}
        else if(tagPage==1 && (u32)row<7) Ps_Copy(message,TagNet_HostMenu() && row==6?"Close the online lobby and disconnect its guests.":descriptions[TagNet_HostMenu() && row==5?6:row],sizeof(message));
        else if(tagPage==2 && row!=5) Ps_Copy(message,row==4?
            "Choose both rosters and watch the battle. L3 changes the fighter you follow.":
            "Choose how many people are playing on this PC.",sizeof(message));
        else if(tagPage==4 && row!=TAG_OPTION_GROUP_COUNT) Ps_Copy(message,"Choose a group of mod settings.",sizeof(message));
        Ps_Text(256,359,message,0.8f,472,3);
    }
}
#define MAINMENU_VOICED(item) (!tagPage && (item) < MAINMENU_TBO)
#else
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
        if (i == MAINMENU_TBO) { /* (listed after Dragon Net Battle, below: not again at the end) */
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
#endif
    }
#ifdef PORT
    /* the second sheet of each state (slot 8 not chosen, 12 chosen) is the entry behind the first */
    if (!PortMainMenu_WriteDisc(gMainMenu->tex[8] + 0x40, 3, 0) || !PortMainMenu_WriteDisc(gMainMenu->tex[12] + 0x40, 3, 1)) {
        PortMainMenu_WriteRuns(gMainMenu->tex[8] + 0x40, 3, sPortTboOff, sizeof(sPortTboOff) / sizeof(sPortTboOff[0]));
        PortMainMenu_WriteRuns(gMainMenu->tex[12] + 0x40, 3, sPortTboOn, sizeof(sPortTboOn) / sizeof(sPortTboOn[0]));
    }
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

    MsgWin_Term();
    IconWin_Term();
    for (i = 0; i < MAINMENU_FLASH_NUM; i++) {
        Flash_Destroy(&gMainMenu->flash[i]);
    }
    if (gMainMenu->res != NULL) {
        Heap_Free(gMainMenu->res);
        gMainMenu->res = NULL;
    }
    if (gMainMenu != NULL) {
        Heap_Free(gMainMenu);
        gMainMenu = NULL;
    }
}

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
            uv.y0 = (item % 8) * 0x20;
            uv.y1 = uv.y0 + 0x20;
            sprintf(name, "mc_menu_plate_%d", i + 1);
            Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, item / 8);
            Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, item / 8);
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
        uv.y0 = (item % 8) * 0x20;
        uv.y1 = uv.y0 + 0x20;
        Flash_FindLabel(flash, "mc_menu_plate_7", "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetTex(flash, &ref, item / 8);
        Flash_FindLabel(flash, "mc_menu_plate_7", "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetTex(flash, &ref, item / 8);
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
    for (i = 0; i < MAINMENU_FLASH_NUM; i++) {
        Flash_Draw(&gMainMenu->flash[i]);
    }
#ifdef PORT
    if(tagPage==3) for(i=0;i<9;i++) Flash_ClipSetFlags(flash,&hidden[i],2,visible[i]);
    if(tagPage!=3)
#endif
    IconWin_Draw();
#ifdef PORT
    if(!tagPage)
#endif
    MsgWin_Draw(0, 0, gMainMenu->voiceLine);
#ifdef PORT
    if(Port_TagAvailable()) {
        Ps_PageSet(); Ps_SetShift(0); Ps_Align(1);
        if(tagPage==3) TagSetup_Draw();
        else {
            if(tagPage) TagMenu_Guide();
            Ps_Text(256,427,TagNet_HostMenu()?(TagNet_HostChoosing()?"BRINGING GUESTS TO BATTLE SETUP":"CHOOSE A MODE FOR THE LOBBY"):tagPage?"SELECT - NORMAL MODES":"PRESS SELECT FOR MODDED MODES",0.65f,470,3);
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

/* Pad 0: up / down move the cursor (the ring scrolls at the ends), confirm enters a mode, cancel leaves. */
void MainMenu_Input(s32 *result) {
#ifdef PORT
    { extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);
      if(tagPage!=3) Port_TagMenuTrace(4,tagPage,MAINMENU_CUR(gMainMenu),tagLabelsReady,gMainMenu->flash[0].flags,gPad[0].gamePressed); }
    if(TagNet_HostMenu()) {
        extern s32 TagLobbyRoster_Input(void);
        if(TagNet_HostPoll()) {gPortScreen=PORT_SCREEN_TAG_LOBBY;MAINMENU_CHOOSE(MPROG_MODE_PORT_SCREEN);return;}
        if(TagNet_HostChoosing())return;
        if(TagLobbyRoster_Input())return;
    }
    /* Select is available immediately, including while the native menu enters. */
    if(!TagNet_HostMenu() && tagLabelsReady && !Port_TagControllersActive() && (gPad[0].gamePressed&PADG_SELECT)) {
        TagSession_Leave(); TagMenu_SetPage(tagPage?0:1); Snd_PlaySe(1,0); return;
    }
    if(tagPage==3) {
        s32 action=TagSetup_Input();
        if(action<0) TagMenu_SetPage(tagMode==TAG_MODE_GAUNTLET || tagMode==TAG_MODE_TOURNAMENT?TAG_MENU_PRESET:2);
        if(action>0) { Snd_PlaySe(1,1); MAINMENU_CHOOSE(40); }
        return;
    }
    if(tagPage!=TAG_MENU_PRESET && tagPage>=5 && (gPad[0].gameRepeat&(PADG_LEFT|PADG_RIGHT))) {
        TagMenu_SettingChange(MAINMENU_CUR(gMainMenu),(gPad[0].gameRepeat&PADG_LEFT)?-1:1); return;
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
            s32 item=MAINMENU_CUR(gMainMenu);
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
            else if(tagPage==1 && TagNet_HostMenu()) {
                if(item<3)TagNet_ChoosePreset(item,0);
                else if(item<5){tagMode=item;presetPage=0;TagMenu_SetPage(TAG_MENU_PRESET);}
                else if(item==5)TagMenu_SetPage(4);
                else TagNet_Leave();
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
