#include "common.h"
#include "menu/menu_b.h"
#include "sys/pad.h"
#include "sys/save.h"

ModeMenu *gModeMenu = NULL; /* 0x3B12F0 */

/*
 * ModeMenu, 0x338020..0x33A360 (one object; it was written as two halves cut at 0x339610, the former menu_b.c,
 * and the next object has ModeBg_Init, ModeBg_Term and ModeBg_Draw). The sub menu of one game mode (gProgress->subMenu, 0..7):
 * a horizontal list of up to 16 items of which three are visible, a guide character with voice, a large picture
 * of the current item that is loaded from disc in the background, and three lines of description.
 */

extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_SetUnk50(MTextBox *box, s32 value);
extern void TextBox_SetRect(MTextBox *box, s32 a, s32 b, s32 c, s32 d);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);

#define MODEMENU_BGM 0x10B1C

/* Index of an item's first line in a text shared by the sub menus: items are numbered through all of them, and
   lines 20..23 are rotated by one. */
s32 ModeMenu_GetLine(s32 menu, s32 item) {
    s32 count[8] = { 3, 4, 7, 5, 16, 5, 4, 4 };
    s32 line = 0;
    s32 i;

    for (i = 0; i < menu; i++) {
        line += count[i];
    }
    line += item;
    switch (line) {
    case 20:
        line = 21;
        break;
    case 21:
        line = 22;
        break;
    case 22:
        line = 23;
        break;
    case 23:
        line = 20;
        break;
    }
    return line;
}

/* One step of the background loader of the current item's large picture. */
void ModeMenu_UpdateImage(void) {
    MTexRes *res;

    switch (gModeMenu->loadState) {
    case MODEMENU_LOAD_ABORT:
        gModeMenu->loadState = MODEMENU_LOAD_RESTART;
        break;
    case MODEMENU_LOAD_RESTART:
        if (gModeMenu->flags & MODEMENU_IMAGE_CHANGE) {
            gModeMenu->flags ^= MODEMENU_IMAGE_CHANGE;
        }
        gModeMenu->loadState = MODEMENU_LOAD_REQUEST;
        break;
    case MODEMENU_LOAD_REQUEST:
        File_CancelRequests();
        File_Request(gModeMenu->imageBase + gModeMenu->items[gModeMenu->cursor[0]], gModeMenu->imageFile, 0x1C000);
        gModeMenu->loadState = MODEMENU_LOAD_READ;
        break;
    case MODEMENU_LOAD_READ:
        if (File_UpdateRequests()) {
            gModeMenu->loadState = MODEMENU_LOAD_UNPACK;
        }
        break;
    case MODEMENU_LOAD_UNPACK:
        Sprite_Unpack(gModeMenu->imageFile, gModeMenu->imageRes, NULL);
        res = gModeMenu->imageRes;
        Res_RelocateOffsets(&res, res, res);
        gModeMenu->tex[9] = MTEX(res, 0);
        gModeMenu->flags |= MODEMENU_IMAGE_READY;
        gModeMenu->loadState = MODEMENU_LOAD_SHOWN;
        break;
    case MODEMENU_LOAD_SHOWN:
        if (gModeMenu->flags & MODEMENU_IMAGE_CHANGE) {
            gModeMenu->tex[9] = NULL;
            gModeMenu->loadState = MODEMENU_LOAD_RESTART;
        }
        break;
    }
}

/* The current item changed: hide the picture, or abort a load in progress, and ask for the new one. */
void ModeMenu_ChangeImage(void) {
    if (gModeMenu->flags & MODEMENU_IMAGE_READY) {
        gModeMenu->flags ^= MODEMENU_IMAGE_READY;
    } else {
        gModeMenu->loadState = MODEMENU_LOAD_ABORT;
    }
    gModeMenu->flags |= MODEMENU_IMAGE_CHANGE;
}

#define MD_RES(pack, n) \
    res = (MTexRes *)MPACK_AT(pack, n); \
    Res_RelocateOffsets(&res, res, res)

#define MD_ICONS(a, b, c) \
    gModeMenu->tex[a] = MTEX(res, 0); \
    gModeMenu->tex[b] = MTEX(res, 1); \
    gModeMenu->tex[c] = MTEX(res, 3)

#define MD_SETUP(max, voice, v10C, image) \
    gModeMenu->itemMax = (max); \
    gModeMenu->voiceBase = (voice); \
    gModeMenu->lineBase = (v10C); \
    gModeMenu->imageBase = (image)

/* Loads the screen (section `section` of archive 2 and the sub menu's own file), builds the list of unlocked
   items from the save and starts the music. */
void ModeMenu_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gModeMenu = Heap_Alloc(0x33C, 0x20, 0, 2);
    memset(gModeMenu, 0, 0x33C);
    gModeMenu->pack = (u32 *)MPACK_AT(gMenuArc2, section);
    gModeMenu->res = Sprite_Unpack(gModeMenu->pack, NULL, NULL);
    gModeMenu->file = File_LoadSync(gProgress->baseFile + gProgress->subMenu + 0x10, NULL, 0);
    gModeMenu->fileRes = Sprite_Unpack(gModeMenu->file, NULL, NULL);
    ModeBg_Init(gModeMenu->fileRes);
    MD_RES(gModeMenu->fileRes, 7);
    switch (gProgress->subMenu) {
    case 0:
        MD_ICONS(10, 11, 12);
        break;
    case 1:
        MD_ICONS(13, 14, 15);
        break;
    case 2:
        MD_ICONS(16, 17, 18);
        break;
    case 3:
        MD_ICONS(19, 21, 20);
        break;
    case 4:
        MD_ICONS(22, 23, 24);
        break;
    case 5:
        MD_ICONS(25, 26, 27);
        break;
    case 6:
        MD_ICONS(28, 29, 30);
        break;
    case 7:
        MD_ICONS(31, 32, 33);
        break;
    }
    MD_RES(gModeMenu->res, 1);
    gModeMenu->tex[8] = MTEX(res, 0);
    gModeMenu->tex[7] = MTEX(res, 1);
    gModeMenu->tex[34] = MTEX(res, 2);
    gModeMenu->tex[38] = MTEX(res, 4);
    gModeMenu->tex[0] = MTEX(res, 5);
    gModeMenu->tex[6] = MTEX(res, 6);
    gModeMenu->tex[5] = MTEX(res, 7);
    gModeMenu->tex[3] = MTEX(res, 8);
    MD_RES(gModeMenu->res, 2);
    gModeMenu->tex[2] = MTEX(res, 0);
    MD_RES(gModeMenu->fileRes, 2);
    gModeMenu->tex[4] = MTEX(res, 0);
    gModeMenu->tex[36] = MTEX(res, 1);
    MD_RES(gModeMenu->fileRes, 3);
    gModeMenu->tex[37] = MTEX(res, 0);
    gModeMenu->tex[1] = MTEX(res, 1);
    MD_RES(gModeMenu->res, 7);
    gModeMenu->tex[35] = MTEX(res, 1);
    Flash_Create(&gModeMenu->flash[0], MPACK_AT(gModeMenu->res, 3), gModeMenu->tex);
    Flash_Play(&gModeMenu->flash[0], 1);
    gModeMenu->msgText = MPACK_AT(gModeMenu->fileRes, 4);
    gModeMenu->subtitles = MPACK_AT(gModeMenu->fileRes, 5);
    MsgWin_Init(MPACK_AT(gModeMenu->res, 8), gModeMenu->msgText, 1, 0);
    gModeMenu->text = MPACK_AT(gModeMenu->res, 6);

    switch (gProgress->subMenu) {
    case 0:
        MD_SETUP(3, 0x8537, 0x13, 0x3CE);
        break;
    case 1:
        MD_SETUP(4, 0x8423, 0xC, 0x3D1);
        break;
    case 2:
        MD_SETUP(7, 0x83B4, 0x12, 0x3D5);
        break;
    case 3:
        MD_SETUP(5, 0x83DB, 0x15, 0x3DC);
        break;
    case 4:
        MD_SETUP(16, 0x8484, 0x13, 0x3E1);
        break;
    case 5:
        MD_SETUP(5, 0x8440, 0x14, 0x3F1);
        break;
    case 6:
        MD_SETUP(4, 0x8402, 0x10, 0x3F6);
        break;
    case 7:
        MD_SETUP(4, 0x8466, 0xE, 0x3FA);
        break;
    }

    for (i = 0; i < gModeMenu->itemMax; i++) {
        SaveSlot *slot = &gSaveData->slot[gProgress->subMenu];

        if (slot->val[0] & (s32)(1U << i)) {
            gModeMenu->items[gModeMenu->itemCount++] = i;
        }
    }
    for (i = gModeMenu->itemCount; i < MODEMENU_ROWS; i++) {
        gModeMenu->items[gModeMenu->itemCount++] = gModeMenu->itemMax;
    }
    gModeMenu->descr = (ModeMenuDescr *)MPACK_AT(gModeMenu->fileRes, 6);
    if (gProgress->subMenu == 7) {
        gModeMenu->descr->count = 2;
    }
    if (gProgress->prevMode == 10 || gProgress->prevMode == 8) {
        for (i = 0; i < gModeMenu->itemCount; i++) {
            if (gModeMenu->items[i] == gProgress->subMenuItem) {
                gModeMenu->cursor[0] = i;
                break;
            }
        }
    }
    gModeMenu->top = gModeMenu->cursor[0];
    gModeMenu->bottom = gModeMenu->top + 2;
    while (gModeMenu->bottom >= gModeMenu->itemCount) {
        gModeMenu->top--;
        gModeMenu->bottom--;
    }
    gModeMenu->imageFile = Heap_Alloc(0x1C000, 0x40, 0, 2);
    gModeMenu->imageRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    gModeMenu->loadState = MODEMENU_LOAD_REQUEST;
    gModeMenu->voiceLine = -1;
    gModeMenu->blink = Rand_Range(0x20);
    for (i = 0; i < 3; i++) {
        TextBox_Init(&gModeMenu->box[i], gModeMenu->text, 0);
        TextBox_SetUnk80(&gModeMenu->box[i], 1);
        TextBox_SetUnk50(&gModeMenu->box[i], 1);
        TextBox_SetRect(&gModeMenu->box[i], 0, 0x200, 0, 0x1A1);
    }

    switch (gProgress->subMenu) {
    case 0:
        Bgm_Play(DISC_RESOURCE(MODEMENU_BGM, 0x899));
        StreamSe_PlayDefault(0, 0x10BDC);
        break;
    case 1:
        Bgm_Play(DISC_RESOURCE(MODEMENU_BGM, 0x834));
        StreamSe_PlayDefault(0, 0x10BDD);
        break;
    case 2:
        Bgm_Play(DISC_RESOURCE(MODEMENU_BGM, 0x835));
        StreamSe_PlayDefault(0, 0x10BDE);
        break;
    case 3:
    case 4:
        Bgm_Play(DISC_RESOURCE(MODEMENU_BGM, 0x86C));
        break;
    case 5:
        Bgm_Play(DISC_RESOURCE(0x10B1E, 0x837));
        break;
    case 6:
        Bgm_Play(DISC_RESOURCE(0x10B28, 0x828));
        break;
    case 7:
        Bgm_Play(DISC_RESOURCE(MODEMENU_BGM, 0x867));
        break;
    }
}

#define MD_FREE(p) \
    if ((p) != NULL) { \
        Heap_Free(p); \
        (p) = NULL; \
    }

/* Frees the screen. */
void ModeMenu_Term(void) {
    s32 i;

    ModeBg_Term();
    MsgWin_Term();
    for (i = 0; i < MODEMENU_FLASH_NUM; i++) {
        Flash_Destroy(&gModeMenu->flash[i]);
    }
    MD_FREE(gModeMenu->imageRes);
    MD_FREE(gModeMenu->imageFile);
    MD_FREE(gModeMenu->fileRes);
    MD_FREE(gModeMenu->file);
    MD_FREE(gModeMenu->res);
    MD_FREE(gModeMenu);
}

/* An item's bit in one of the sub menu's three save words. */
#define MD_SAVE_BIT(n, item) (gSaveData->slot[gProgress->subMenu].val[n] & (s32)(1U << (item)))

/* Sets up every clip of the movie for this frame and draws it and the message window. */
void ModeMenu_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    char sub[64];
    s32 i;
    MFlash *flash;

    ModeBg_Draw();
    flash = &gModeMenu->flash[0];
    sprintf(name, "mc_guide_%02d", gProgress->subMenu);
    sprintf(sub, "mc_guide_%02d_eye", gProgress->subMenu);
    Flash_FindLabel(flash, name, sub, &ref);
    FlashAnim_Blink(flash, &ref, &gModeMenu->blink, 0);
    sprintf(sub, "mc_guide_%02d_mouth", gProgress->subMenu);
    Flash_FindLabel(flash, name, sub, &ref);
    FlashAnim_Talk(flash, &ref, &gModeMenu->talk, 0);

    if (gModeMenu->flags & MODEMENU_IMAGE_READY) {
        gModeMenu->imageAlpha += 0.075f;
        if (gModeMenu->imageAlpha >= 1.0f) {
            gModeMenu->imageAlpha = 1.0f;
        }
    } else {
        gModeMenu->imageAlpha = 0.0f;
    }
    Flash_FindLabel(flash, NULL, "mc_image_l_1", &ref);
    Flash_ClipSetFlags(flash, &ref, 0x80, 1);
    Flash_FindLabel(flash, NULL, "mc_battle_mask", &ref);
    Flash_ClipSetAlpha(flash, &ref, gModeMenu->imageAlpha);
    Flash_ClipSetFlags(flash, &ref, 0x100, 1);

    uv.x0 = 0;
    uv.y0 = (gModeMenu->items[gModeMenu->cursor[0]] % 4) * 0x40;
    uv.x1 = 0x200;
    uv.y1 = uv.y0 + 0x40;
    Flash_FindLabel(flash, NULL, "mc_battle_text_1", &ref);
    Flash_ClipSetAlpha(flash, &ref, gModeMenu->imageAlpha);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetTex(flash, &ref, gModeMenu->items[gModeMenu->cursor[0]] / 4);

    Flash_FindLabel(flash, NULL, "mc_clear", &ref);
    if (MD_SAVE_BIT(1, gModeMenu->items[gModeMenu->cursor[0]])) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
        Flash_ClipSetAlpha(flash, &ref, gModeMenu->imageAlpha);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }

    for (i = 0; i < 2; i++) {
        uv.y0 = 0;
        uv.x0 = i * 0x20;
        uv.x1 = uv.x0 + 0x20;
        uv.y1 = 0x20;
        switch (i) {
        case 0:
            Flash_FindLabel(flash, "mc_yajirusi_left", "mc_yajirusi_icon_left", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, gModeMenu->top != 0);
            break;
        case 1:
            Flash_FindLabel(flash, "mc_yajirusi_right", "mc_yajirusi_icon_right", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, gModeMenu->bottom != gModeMenu->itemCount - 1);
            break;
        }
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    for (i = 0; i < MODEMENU_ROWS; i++) {
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_icon_new", &ref);
        if (gModeMenu->items[gModeMenu->top + i] != gModeMenu->itemMax &&
            MD_SAVE_BIT(2, gModeMenu->items[gModeMenu->top + i])) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        Flash_FindLabel(flash, name, "mc_image_s", &ref);
        Flash_ClipSetTex(flash, &ref, gModeMenu->items[gModeMenu->top + i]);
    }
    for (i = 0; i < 2; i++) {
        sprintf(name, "mc_menu_plate_%d", i + 4);
        Flash_FindLabel(flash, name, "mc_icon_new", &ref);
        if (gModeMenu->items[gModeMenu->extra] != gModeMenu->itemMax &&
            MD_SAVE_BIT(2, gModeMenu->items[gModeMenu->extra])) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        Flash_FindLabel(flash, name, "mc_image_s", &ref);
        Flash_ClipSetTex(flash, &ref, gModeMenu->items[gModeMenu->extra]);
    }

    for (i = 0; i < 3; i++) {
        sprintf(name, "mc_episode_text_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (i >= gModeMenu->descr[gModeMenu->items[gModeMenu->cursor[0]]].count) {
            TextBox_AttachLine(flash, &ref, 0, -(s32)gModeMenu->scroll, -1, &gModeMenu->box[i]);
        } else {
            TextBox_AttachLine(flash, &ref, 0, -(s32)gModeMenu->scroll,
                               gModeMenu->descr[gModeMenu->items[gModeMenu->cursor[0]]].line + i, &gModeMenu->box[i]);
        }
    }
    Flash_FindLabel(flash, NULL, "mc_episode_next", &ref);
    Flash_ClipSetFlags(flash, &ref, 2, (gModeMenu->flags >> 5) & 1);

    for (i = 0; i < MODEMENU_FLASH_NUM; i++) {
        Flash_Draw(&gModeMenu->flash[i]);
    }
    Font_FlushAll();
    MsgWin_Draw(0, 0, gModeMenu->voiceLine);
}

/*
 * 0x339610..0x33A360: what was the tail half (menu_b.c). Its strings are shared with the head
 * ("mc_menu_plate_%d" is ModeMenu_Draw's), which is why the two halves are one source.
 */

extern void Voice_StopWithLip(void);
extern void StreamSe_FadeOutStep(s32 se);
extern void BattleSetup_Clear(void);
extern void BattleSetup_SetScript(s32 script);

#define MODEMENU_ITEM(m) ((m)->items[(m)->cursor[0]])

/* Advances the movie, the greeting voice and the scroll of the description. */
void ModeMenu_Update(void) {
    s32 i;

    if ((gModeMenu->flags & MODEMENU_GREETED) &&
        (gModeMenu->voiceLine == gModeMenu->lineBase || gModeMenu->voiceLine == gModeMenu->lineBase + 3)) {
        if (Voice_GetStat(0) == 5) {
            gModeMenu->voiceLine = MODEMENU_ITEM(gModeMenu) + gModeMenu->lineBase + 4;
            Voice_PlayWithSubtitle(gModeMenu->subtitles, gModeMenu->voiceBase, gModeMenu->voiceLine);
        }
    }
    for (i = 0; i < MODEMENU_FLASH_NUM; i++) {
        Flash_Advance(&gModeMenu->flash[i]);
    }
    if (!(gModeMenu->flags & MODEMENU_STARTED) && (gModeMenu->flash[0].trig & 2)) {
        MsgWin_Open();
    }
    if (gModeMenu->flags & MODEMENU_SCROLLING) {
        gModeMenu->scroll += 0.23333333f;
        if (gModeMenu->scroll >= gModeMenu->scrollMax) {
            gModeMenu->scroll = gModeMenu->scrollMax;
            gModeMenu->flags &= ~MODEMENU_SCROLLING;
            gModeMenu->flags |= MODEMENU_NEXT;
        }
    }
}

/* Sends the plate under the cursor to a label of its clip (kind 0; other kinds do nothing). */
void ModeMenu_PlateGoto(s32 movie, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gModeMenu->flash[movie];

    if (kind == 0) {
        sprintf(name, "mc_menu_plate_%d", gModeMenu->cursor[0] - gModeMenu->top + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipGotoLabel(flash, &ref, label);
    }
}

#define MODEMENU_VOICE(line) \
    gModeMenu->voiceLine = (line); \
    Voice_PlayWithSubtitle(gModeMenu->subtitles, gModeMenu->voiceBase, gModeMenu->voiceLine)

/* The cursor moved: light its plate, let the guide present the item and load its picture. */
#define MODEMENU_MOVED() \
    ModeMenu_PlateGoto(0, 0, "fl_on_start"); \
    MODEMENU_VOICE(MODEMENU_ITEM(gModeMenu) + gModeMenu->lineBase + 4); \
    ModeMenu_ChangeImage(); \
    Snd_PlaySe(1, 0)

/* Pad handling: the list (focus 0) or the description of the chosen item (focus 1). */
void ModeMenu_Input(s32 *result) {
    if (!(gModeMenu->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gModeMenu->flags & MODEMENU_STARTED)) {
        ModeMenu_PlateGoto(0, 0, "fl_on_start");
        gModeMenu->flags |= MODEMENU_STARTED;
    }
    switch (gModeMenu->focus) {
    case 0:
        if (gPad[0].gameRepeat & 1) {
            gModeMenu->unk124 = 0;
            if (gModeMenu->cursor[gModeMenu->focus] > 0 &&
                gModeMenu->items[gModeMenu->cursor[gModeMenu->focus] - 1] != gModeMenu->itemMax) {
                ModeMenu_PlateGoto(0, 0, "fl_off_start");
                gModeMenu->cursor[gModeMenu->focus]--;
                if (gModeMenu->cursor[gModeMenu->focus] < gModeMenu->top) {
                    Flash_GotoLabel(&gModeMenu->flash[0], "fl_battle_right", 1);
                    gModeMenu->extra = gModeMenu->bottom;
                    gModeMenu->top--;
                    gModeMenu->bottom--;
                }
                MODEMENU_MOVED();
            }
        } else if (gPad[0].gameRepeat & 2) {
            gModeMenu->unk124 = 0;
            if (gModeMenu->cursor[gModeMenu->focus] < gModeMenu->itemCount - 1 &&
                gModeMenu->items[gModeMenu->cursor[gModeMenu->focus] + 1] != gModeMenu->itemMax) {
                ModeMenu_PlateGoto(0, 0, "fl_off_start");
                gModeMenu->cursor[gModeMenu->focus]++;
                if (gModeMenu->cursor[gModeMenu->focus] > gModeMenu->bottom) {
                    Flash_GotoLabel(&gModeMenu->flash[0], "fl_battle_left", 1);
                    gModeMenu->extra = gModeMenu->top;
                    gModeMenu->top++;
                    gModeMenu->bottom++;
                }
                MODEMENU_MOVED();
            }
        } else if (gPad[0].gamePressed & 0x200) {
            gModeMenu->unk124 = 0;
            ModeMenu_PlateGoto(0, 0, "fl_ok");
            gModeMenu->step = MODEMENU_STEP_GUIDE;
            gModeMenu->scroll = 0.0f;
            gModeMenu->scrollMax = gModeMenu->descr[MODEMENU_ITEM(gModeMenu)].count * 40.0f + 160.0f;
            gModeMenu->flags &= ~MODEMENU_NEXT;
            gModeMenu->focus = 1;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gModeMenu->unk124 = 0;
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Snd_PlaySe(1, 2);
        }
        break;
    case 1:
        if (gPad[0].gamePressed & 0x1200) {
            gModeMenu->flags |= MODEMENU_CHOSEN;
            gModeMenu->flags |= MODEMENU_LEAVING;
            gModeMenu->timer = 15;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            ModeMenu_PlateGoto(0, 0, "fl_on_start");
            Flash_GotoLabel(&gModeMenu->flash[0], "fl_battle_cansel", 1);
            MsgWin_Open();
            MODEMENU_VOICE(gModeMenu->cursor[0] + gModeMenu->lineBase + 4);
            gModeMenu->focus = 0;
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

/* The sequence that follows the choice of an item: guide comment, description scrolled in and read out. */
void ModeMenu_UpdateStep(void) {
    if (gModeMenu->step == 0) {
        return;
    }
    switch (gModeMenu->step) {
    case MODEMENU_STEP_GUIDE:
        MODEMENU_VOICE(gModeMenu->lineBase + 1);
        gModeMenu->step++;
        break;
    case MODEMENU_STEP_GUIDE_WAIT:
        if (Voice_GetStat(0) == 5) {
            gModeMenu->step++;
        } else if (gPad[0].gamePressed & 0x200) {
            gModeMenu->step++;
            Voice_StopWithLip();
            Snd_PlaySe(1, 1);
        }
        break;
    case MODEMENU_STEP_OPEN:
        Flash_GotoLabel(&gModeMenu->flash[0], "fl_battle_ok", 1);
        MsgWin_Close();
        gModeMenu->step++;
        break;
    case MODEMENU_STEP_OPENED:
        gModeMenu->step = MODEMENU_STEP_NARR_INIT;
        break;
    case MODEMENU_STEP_NARR_INIT:
        gModeMenu->narrWait = 0;
        gModeMenu->narrLine = 0;
        gModeMenu->step++;
        break;
    case MODEMENU_STEP_NARR_START:
        if (gModeMenu->flash[0].trig & 1) {
            gModeMenu->flags |= MODEMENU_SCROLLING;
            gModeMenu->step++;
        }
        break;
    case MODEMENU_STEP_NARR_LINE:
        if (gModeMenu->descr[MODEMENU_ITEM(gModeMenu)].delay[gModeMenu->narrLine] <= gModeMenu->narrWait++) {
            Voice_PlayWithSubtitle(NULL, MODEMENU_NARR_VOICE,
                                   gModeMenu->descr[MODEMENU_ITEM(gModeMenu)].line + gModeMenu->narrLine);
            gModeMenu->narrWait = 0;
            gModeMenu->step++;
        }
        break;
    case MODEMENU_STEP_NARR_WAIT:
        if (Voice_GetStat(0) == 5) {
            if (gModeMenu->descr[MODEMENU_ITEM(gModeMenu)].count <= ++gModeMenu->narrLine) {
                gModeMenu->step++;
            } else {
                gModeMenu->step--;
            }
        } else if (gPad[0].gamePressed & 0x1000) {
            gModeMenu->step = MODEMENU_STEP_GO;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            ModeMenu_PlateGoto(0, 0, "fl_on_start");
            Flash_GotoLabel(&gModeMenu->flash[0], "fl_battle_cansel", 1);
            MsgWin_Open();
            MODEMENU_VOICE(gModeMenu->cursor[0] + gModeMenu->lineBase + 4);
            gModeMenu->narrWait = 0;
            gModeMenu->narrLine = 0;
            gModeMenu->step++;
            gModeMenu->focus = 0;
            Snd_PlaySe(1, 2);
        }
        break;
    case MODEMENU_STEP_NARR_END:
        gModeMenu->step = 0;
        break;
    case MODEMENU_STEP_GO:
        gModeMenu->step = 0;
        gModeMenu->flags |= MODEMENU_CHOSEN;
        gModeMenu->flags |= MODEMENU_LEAVING;
        gModeMenu->timer = 15;
        break;
    }
}

/* The sub menu's own frame loop. Returns 1 when a battle was chosen (the battle setup is prepared), 0 on cancel. */
s32 ModeMenu_Run(s32 section) {
    s32 result = 1;

    ModeMenu_Init(section);
    if (gProgress->prevMode == 6) {
        ColorFade_StartIn(0xFF, 0xFF, 0xFF, 0x14);
    } else {
        ColorFade_StartIn(0, 0, 0, 0x14);
    }
    while (1) {
        Gfx_BeginFrame();
        ModeMenu_UpdateImage();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            ModeMenu_Update();
            ModeMenu_UpdateStep();
        }
        ModeMenu_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gModeMenu->flags & MODEMENU_GREETED) && (gModeMenu->flash[0].flags & MFLASH_PAD)) {
                gModeMenu->flags |= MODEMENU_GREETED;
                {
                    SaveSlot *slot = &gSaveData->slot[gProgress->subMenu];

                    if (slot->flags & SAVESLOT_GREET_FIRST) {
                        gModeMenu->voiceLine = gModeMenu->lineBase + 3;
                        slot = gSaveData->slot;
                        slot += gProgress->subMenu;
                        slot->flags &= ~SAVESLOT_GREET_FIRST;
                    } else {
                        gModeMenu->voiceLine = gModeMenu->lineBase;
                    }
                }
                Voice_PlayWithSubtitle(gModeMenu->subtitles, gModeMenu->voiceBase, gModeMenu->voiceLine);
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            StreamSe_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gModeMenu->loadState != MODEMENU_LOAD_SHOWN) {
                continue;
            }
            break;
        } else if (gModeMenu->flags & MODEMENU_LEAVING) {
            if (--gModeMenu->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                gProgress->subMenuItem = MODEMENU_ITEM(gModeMenu);
            }
        } else if (gModeMenu->step == 0) {
            ModeMenu_Input(&result);
        }
    }
    if (result) {
        gSaveData->slot[gProgress->subMenu].val[2] &= ~(1 << MODEMENU_ITEM(gModeMenu));
        BattleSetup_Clear();
        BattleSetup_SetScript(ModeMenu_GetLine(gProgress->subMenu, MODEMENU_ITEM(gModeMenu)));
    }
    ModeMenu_Term();
    Dma_ResetBuffers();
    return result;
}
