#include "common.h"
#include "menu/menu_a.h"
#ifdef PORT
extern void Port_NetMenuOpen(void);
extern int Port_NetMenuListed(void);
extern int Port_NetSession(void);
extern void Port_NetLeave(void);
#endif
#include "sys/pad.h"
#include "sys/save.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/* The screen's work (overlay .data, 0x3B0E80). */
MainMenu *gMainMenu = NULL;

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
#include "port_bt4_label.h"
#include "port_bt4_label_eng.h"
#include "../../port/src/gs/game_profile.h"

#include "menu/port_screen.h"
#include "menu/tbo_art.h"
extern void Flash_ClipSetCustomDraw(MFlash *,MFlashRef *,void *,void *);
typedef struct OnlineLabelProp {
    u32 flags;f32 x,y,alpha,scaleX,scaleY,color;MFlashUv uv;
    f32 m[9],mul[4];s16 add[4];
} OnlineLabelProp;
static u32 PortMainMenu_LabelTint(const OnlineLabelProp *p){
    u32 tint=0;
    for(s32 channel=0;channel<3;channel++){
        s32 value=(s32)(128*p->mul[channel])+p->add[channel];
        if(p->flags&32)value=(s32)(value*p->color);
        if(value<0)value=0;
        if(value>255)value=255;
        tint|=(u32)value<<(channel*8);
    }
    return tint;
}
static void PortMainMenu_OnlineLabel(void *arg,OnlineLabelProp *p){
    s32 alpha=(s32)(p->mul[3]*128)+p->add[3];
    f32 sx=p->m[0],sy=p->m[4],x=p->m[2],y=p->m[5];
    if(p->flags&16){sx*=p->scaleX;sy*=p->scaleY;}
    if(p->flags&2){x+=p->x;y+=p->y;}
    if(p->flags&8)alpha=(s32)(alpha*p->alpha);
    Ps_PageSet();Ps_SetShift(0);
    /* The long name stays inside the plate's cursor end cap, while retaining
       the native row's height, baseline and normal/highlight animation. */
    Ps_Sprite(arg?TBO_MAIN_ONLINE_ON:TBO_MAIN_ONLINE_OFF,Ps_Left()+x,y,Ps_Left()+x+512*sx*.9f,y+32*sy,0,0,1,1,PortMainMenu_LabelTint(p),alpha);
}

#define MAINMENU_TBO 11
#define MM_ICON(item) ((item) == MAINMENU_TBO ? 4 : (item)) /* whose icon an item shows */

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
    u64 layout = *(u64 *)(tex + 0x30);
    s32 i;
    s32 k;

    if (row == 3 && ((layout >> 26) & 15) == 9 && ((layout >> 30) & 15) == 7) {
        const Bt4LabelPatch *patch;
        s32 patches;
        if (run == sPortTboOff && ((layout >> 20) & 63) == 20) {
            patch = Port_GameProfileEnglish() ? sBt4TboOffEng : sBt4TboOff;
            patches = Port_GameProfileEnglish() ? sizeof(sBt4TboOffEng)/sizeof(sBt4TboOffEng[0]) : sizeof(sBt4TboOff)/sizeof(sBt4TboOff[0]);
        } else if (run == sPortTboOn && ((layout >> 20) & 63) == 19) {
            patch = Port_GameProfileEnglish() ? sBt4TboOnEng : sBt4TboOn;
            patches = Port_GameProfileEnglish() ? sizeof(sBt4TboOnEng)/sizeof(sBt4TboOnEng[0]) : sizeof(sBt4TboOn)/sizeof(sBt4TboOn[0]);
        } else return;
        for (i = 0; i < patches; i++) {
            px[patch[i].off] = (px[patch[i].off] & ~patch[i].mask) | patch[i].value;
        }
        return;
    }
    if (((layout >> 20) & 0x3F) != 0x13 || ((layout >> 26) & 15) != 9 ||
        (1 << ((layout >> 30) & 15)) < (row + 1) * 32) return;

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
#define MAINMENU_VOICED(item) ((item) < MAINMENU_TBO)
#else
#define MM_ICON(item) (item)
#define MAINMENU_VOICED(item) 1
#endif


#ifdef PORT
static u32 *sOnlineDescriptions;
static const u16 sOnlineDescription0[]={65279,70,105,103,104,116,32,119,105,116,104,32,102,97,114,97,119,97,121,32,102,114,105,101,110,100,115,10,97,110,100,32,111,116,104,101,114,32,112,108,97,121,101,114,115,46,0};
static const u16 sOnlineDescription1[]={65279,67,111,109,112,101,116,101,32,119,105,116,104,32,112,108,97,121,101,114,115,10,102,114,111,109,32,102,97,114,97,119,97,121,32,112,108,97,99,101,115,33,0};
static const u16 sOnlineDescription2[]={65279,77,121,32,102,97,109,105,108,121,32,97,110,100,32,84,114,117,110,107,115,39,115,10,102,97,109,105,108,121,32,99,97,110,32,99,111,109,112,101,116,101,46,0};
static const u16 sOnlineDescription3[]={65279,67,111,109,112,101,116,101,32,119,105,116,104,32,112,108,97,121,101,114,115,10,102,114,111,109,32,97,114,111,117,110,100,32,116,104,101,32,119,111,114,108,100,46,10,65,109,97,122,105,110,103,33,0};
static u32 *PortMainMenu_Descriptions(u32 *original){
    const u16 *replacement[4]={sOnlineDescription0,sOnlineDescription1,sOnlineDescription2,sOnlineDescription3};
    s32 count=(s32)original[0],i,chars;u32 bytes,at;const u16 *text;u16 *to;
    if(!Port_GameProfileEnglish()||count<36||count>256)return original;
    bytes=(count+1)*4;
    for(i=0;i<count;i++){
        text=i>=32&&i<36?replacement[i-32]:(u16 *)((u8 *)original+(original[i+1]&~3));
        for(chars=0;text[chars];chars++);bytes+=((chars+1)*2+3)&~3;
    }
    sOnlineDescriptions=Heap_Alloc(bytes,0x20,0,2);
    if(!sOnlineDescriptions)return original;
    memset(sOnlineDescriptions,0,bytes);sOnlineDescriptions[0]=count;at=(count+1)*4;
    for(i=0;i<count;i++){
        text=i>=32&&i<36?replacement[i-32]:(u16 *)((u8 *)original+(original[i+1]&~3));
        sOnlineDescriptions[i+1]=at;to=(u16 *)((u8 *)sOnlineDescriptions+at);
        for(chars=0;text[chars];chars++)to[chars]=text[chars];to[chars]=0;
        at+=((chars+1)*2+3)&~3;
    }
    return sOnlineDescriptions;
}
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
    MM_RES(29 + Rand_Range(3) * 2 + 1);
    gMainMenu->tex[27] = MTEX(res, 0);
    gMainMenu->tex[28] = MTEX(res, 1);
    gMainMenu->tex[29] = MTEX(res, 2);
    MM_RES(15);
    gMainMenu->tex[30] = MTEX(res, 0);
    gMainMenu->tex[13] = MTEX(res, 1);
    gMainMenu->tex[14] = MTEX(res, 2);
    gMainMenu->tex[15] = MTEX(res, 3);
    MM_RES(29 + Rand_Range(3) * 2);
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
#ifdef PORT
    gMainMenu->msgText = PortMainMenu_Descriptions(gMainMenu->msgText);
#endif
    gMainMenu->subtitles = MPACK_AT(gMainMenu->res, 27);
    MsgWin_Init(MPACK_AT(gMainMenu->res, 21), gMainMenu->msgText, 1, 0);
    MsgWin_Open();

    for (i = 0; i < MAINMENU_ITEM_MAX; i++) {
#ifdef PORT
        /* Item 4, "Dragon Net Battle", is the one entry the PS2 game never lists: it has its plate, label, icon and
           the guides' voiced lines on the disc, and no mode behind it (the Wii version's online mode). The port
           lists it: it is the way into online play. BT3_MENU_ITEM4=0 hides it again (the pad recordings made
           before it was listed count on the old menu). */
        if (i == 4) {
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
        if (i == 3) {
            gMainMenu->items[gMainMenu->itemCount++] = MAINMENU_TBO;
        }
#endif
    }
#ifdef PORT
    /* the second sheet of each state (slot 8 not chosen, 12 chosen) is the entry behind the first */
    if (1) { /* The BT4 sheets use their own lettering; the port's bundled online label occupies the spare row. */
        PortMainMenu_WriteRuns(gMainMenu->tex[8] + 0x40, 3, sPortTboOff, sizeof(sPortTboOff) / sizeof(sPortTboOff[0]));
        PortMainMenu_WriteRuns(gMainMenu->tex[12] + 0x40, 3, sPortTboOn, sizeof(sPortTboOn) / sizeof(sPortTboOn[0]));
    }
#endif
    if(gProgress->mainMenuItem==4)gProgress->mainMenuItem=MAINMENU_TBO;
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
#ifdef PORT
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_MAINMENU_REVIEW")){
        fprintf(stderr,"main menu entries:");for(i=0;i<gMainMenu->itemCount;i++)fprintf(stderr," %d",gMainMenu->items[i]);fprintf(stderr,"\n");
    }
#endif
    for (i = 0; i < 4; i++) {
        gMainMenu->blink[i] = Rand_Range(0x20);
    }
}

/* Frees the screen.
   The movies of a screen are an array (MainMenu_PlateGoto takes the index) of ONE, and Term / Update / Draw
   walk it with a loop. The loop compiles away, but it is what makes the original load the work pointer for the
   Flash call separately (`lw a0,%lo(gMainMenu)(s0)`) instead of sharing the address register with the code
   after it: without the loop the function does not match. */
void MainMenu_Term(void) {
    s32 i;

    MsgWin_Term();
#ifdef PORT
    if(sOnlineDescriptions){Heap_Free(sOnlineDescriptions);sOnlineDescriptions=NULL;}
#endif
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
    Port_MenuPage(2); /* widescreen menus: this screen's page (PORT_MENU_SCREEN in port/src/gs_marker.c), its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gMainMenu->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gMainMenu->bg, 0, 0, 0x80);
#endif
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
            Flash_ClipSetCustomDraw(flash,&ref,item==MAINMENU_TBO?(void*)PortMainMenu_OnlineLabel:NULL,NULL);
            Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, item / 8);
            Flash_ClipSetCustomDraw(flash,&ref,item==MAINMENU_TBO?(void*)PortMainMenu_OnlineLabel:NULL,(void*)1);
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
            Flash_ClipSetCustomDraw(flash,&ref,item==MAINMENU_TBO?(void*)PortMainMenu_OnlineLabel:NULL,NULL);
        Flash_FindLabel(flash, "mc_menu_plate_7", "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetTex(flash, &ref, item / 8);
            Flash_ClipSetCustomDraw(flash,&ref,item==MAINMENU_TBO?(void*)PortMainMenu_OnlineLabel:NULL,(void*)1);
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

    for (i = 0; i < MAINMENU_FLASH_NUM; i++) {
        Flash_Draw(&gMainMenu->flash[i]);
    }
    IconWin_Draw();
#ifdef PORT
    MsgWin_Draw(0, 0, MAINMENU_CUR(gMainMenu)==MAINMENU_TBO ? 0x20+gMainMenu->guide : gMainMenu->voiceLine);
#else
    MsgWin_Draw(0, 0, gMainMenu->voiceLine);
#endif
#ifdef PORT
    Port_MenuPage(0);
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
            MAINMENU_COUNT();
            break;
        case 4:
#ifdef PORT
            Port_NetMenuOpen(); /* the port's own online lobby */
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
            gPortScreen = PORT_SCREEN_TBO;
            MAINMENU_COUNT();
            MAINMENU_CHOOSE(MPROG_MODE_PORT_SCREEN);
            break;
#endif
        }
        MainMenu_PlateGoto(0, "fl_ok");
        Snd_PlaySe(1, 1);
    } else if (gPad[0].gamePressed & 0x400) {
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
#ifdef PORT
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_MAINMENU_REVIEW")){
            static s32 reviewTick,reviewHeld,reviewCaptured;
            if(ColorFade_IsInDone()&&(gMainMenu->flash[0].flags&MFLASH_PAD)&&!(gMainMenu->flags&MAINMENU_LEAVING)){
                reviewTick++;
                if(reviewTick>90&&reviewTick%18==0&&MAINMENU_CUR(gMainMenu)!=MAINMENU_TBO)gPad[0].gameRepeat=PADG_DOWN;
                if(MAINMENU_CUR(gMainMenu)==MAINMENU_TBO){
                    reviewHeld++;
                    if(reviewHeld==60&&!reviewCaptured){
                        extern int gPortTagUiCaptureRequest;gPortTagUiCaptureRequest=1;reviewCaptured=1;
                        fprintf(stderr,"main menu review: online selected, description=%d, English=%d\n",0x20+gMainMenu->guide,Port_GameProfileEnglish());
                    }
                }
            }
        }
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_VIDEO_TEST")){
            static s32 videoTick,videoReady;
            if(ColorFade_IsInDone()&&(gMainMenu->flash[0].flags&MFLASH_PAD)&&!(gMainMenu->flags&MAINMENU_LEAVING)){
                ++videoTick;
                if(videoTick>90&&videoTick%18==0){
                    extern s32 TagHudEditor_RecordDone(void);
                    s32 wanted=getenv("BT4_TAG_RECORD_HUD")&&!TagHudEditor_RecordDone()?9:MAINMENU_TBO;
                    if(MAINMENU_CUR(gMainMenu)!=wanted)gPad[0].gameRepeat=PADG_DOWN;
                    else if(++videoReady>=4){gPad[0].gamePressed=PADG_CROSS;fprintf(stderr,"tag video: main menu chose %s\n",wanted==9?"Options":"Tenkaichi Budokai Online");videoReady=0;}
                }
            }
        }
#endif
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
                gProgress->mainMenuItem = MAINMENU_CUR(gMainMenu);
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
