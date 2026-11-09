#include "common.h"
#include "menu/menu_l.h"
#include "sys/pad.h"

/*
 * SoloSel, 0x36B3E0..0x36E028: the one-character select of the modes 13..30 group (archive gMenuArc3; run for
 * progress modes 15, 18, 21, 25 and 29 by the handler at 0x379A58). A variant of EntrySel (menu_i_d.c, menu_j.c)
 * with a single entrant, three movies and the two guides "17go" / "18go". One object: its work pointer gSoloSel
 * (0x3B7348) is the first of this group of modules, its .rodata runs 0x3B7450..0x3B7738 and ends with the jump
 * table of its pad handler. Was written as two chunks: menu_l_b.c (up to 0x36DBE8) and menu_m.c (the closing step
 * 0x36DBE8 and SoloSel_Run 0x36DD58, which emit no read-only data).
 */

SoloSel *gSoloSel = NULL; /* 0x3B7348 */

#define SS_CUR (gSoloSel->sel->entry)
#define SS_CELL (gSoloSel->grid[SS_CUR.row * SOLOSEL_COLS + SS_CUR.col])

/* The cursor moved to another grid row: the seven chips take the characters of the new row. */
void SoloSel_SwapRowTex(void) {
    s32 tbl[SOLOSEL_COLS] = {8, 11, 12, 13, 14, 15, 16};
    MTexRes *res;
    s32 i;

    for (i = 0; i < SOLOSEL_COLS; i++) {
        s32 chara = gSoloSel->grid[SS_CUR.row * SOLOSEL_COLS + i].id;

        gSoloSel->sel->prevRowChara[i] = gSoloSel->sel->rowChara[i];
        gSoloSel->sel->rowChara[i] = chara;
        res = (MTexRes *)MPACK_AT(gSoloSel->chips, chara + 1);
        gSoloSel->texB[18 + i] = gSoloSel->texB[tbl[i]];
        gSoloSel->texB[tbl[i]] = MTEX(res, 0);
    }
}

/* The seven chips take the forms of the cell under the cursor (empty chips past the last form). */
void SoloSel_SetRowTex(void) {
    s32 tbl[SOLOSEL_COLS] = {8, 11, 12, 13, 14, 15, 16};
    MTexRes *res;
    s32 i;

    for (i = 0; i < SOLOSEL_COLS; i++) {
        s32 chara;

        if (i < SS_CELL.formCount) {
            chara = SS_CELL.form[i];
        } else {
            chara = LCHR_ID_NONE;
        }
        gSoloSel->sel->prevRowChara[i] = gSoloSel->sel->rowChara[i];
        gSoloSel->sel->rowChara[i] = chara;
        res = (MTexRes *)MPACK_AT(gSoloSel->chips, chara + 1);
        gSoloSel->texB[18 + i] = gSoloSel->texB[tbl[i]];
        gSoloSel->texB[tbl[i]] = MTEX(res, 0);
    }
}

/* One step of the background loader of the large picture (file 0x2F9 + character). */
void SoloSel_UpdateImage(void) {
    MTexRes *res;

    switch (gSoloSel->loadState) {
    case SOLOSEL_LOAD_ABORT:
        gSoloSel->loadState = SOLOSEL_LOAD_RESTART;
        break;
    case SOLOSEL_LOAD_RESTART:
        if (gSoloSel->sel->flags & SOLOSEL_SEL_IMAGE_CHANGE) {
            gSoloSel->sel->flags ^= SOLOSEL_SEL_IMAGE_CHANGE;
        }
        gSoloSel->loadState = SOLOSEL_LOAD_REQUEST;
        break;
    case SOLOSEL_LOAD_REQUEST:
        File_CancelRequests();
        File_Request(gSoloSel->sel->image + DISC_RESOURCE(0x2F9, 0x71C), gSoloSel->imageFile, 0x16800);
        gSoloSel->loadState = SOLOSEL_LOAD_READ;
        break;
    case SOLOSEL_LOAD_READ:
        if (File_UpdateRequests()) {
            gSoloSel->loadState = SOLOSEL_LOAD_UNPACK;
        }
        break;
    case SOLOSEL_LOAD_UNPACK:
        Sprite_Unpack(gSoloSel->imageFile, gSoloSel->imageRes, NULL);
        res = gSoloSel->imageRes;
        Res_RelocateOffsets(&res, res, res);
        gSoloSel->texA[17] = MTEX(res, 0);
        gSoloSel->sel->flags |= SOLOSEL_SEL_IMAGE_READY;
        gSoloSel->loadState = SOLOSEL_LOAD_SHOWN;
        break;
    case SOLOSEL_LOAD_SHOWN:
        if (gSoloSel->sel->flags & SOLOSEL_SEL_IMAGE_CHANGE) {
            gSoloSel->texA[17] = NULL;
            gSoloSel->loadState = SOLOSEL_LOAD_RESTART;
        }
        break;
    }
}

/* The shown character changed: hide the picture, or abort a load in progress, and ask for the new one. */
void SoloSel_ChangeImage(void) {
    if (gSoloSel->sel->flags & SOLOSEL_SEL_IMAGE_READY) {
        gSoloSel->sel->flags ^= SOLOSEL_SEL_IMAGE_READY;
    } else {
        gSoloSel->loadState = SOLOSEL_LOAD_ABORT;
    }
    gSoloSel->sel->flags |= SOLOSEL_SEL_IMAGE_CHANGE;
}

/* Sends a clip of movie `movie` to a label: the chip or plate the cursor is on. */
void SoloSel_ClipGoto(s32 movie, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gSoloSel->flash[movie];

    switch (kind) {
    case 0:
        sprintf(name, "mc_chara_chip_%03d", SS_CUR.col);
        break;
    case 1:
        sprintf(name, "mc_chara_chip_%03d", SS_CUR.form);
        break;
    case 2:
        sprintf(name, "mc_custom_plate_%d", SS_CUR.custom + 1);
        break;
    case 5:
        sprintf(name, "mc_color_plate_%d", SS_CUR.costume + 1);
        break;
    case 3:
    case 6:
        return;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

#define SS_RES(n) \
    res = (MTexRes *)MPACK_AT(gSoloSel->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 3), builds its three movies and the grid. */
void SoloSel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gSoloSel = Heap_Alloc(0x1AA4, 0x20, 0, 2);
    memset(gSoloSel, 0, 0x1AA4);
    gSoloSel->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gSoloSel->res = Sprite_Unpack(gSoloSel->pack, NULL, NULL);
    if (gProgress->mode == 21) {
        gSoloSel->flags |= SOLOSEL_NO_CUSTOM;
    }
    SS_RES(32);
    gSoloSel->bg = res;
    SS_RES(33);
    gSoloSel->texA[0] = MTEX(res, 0);
    gSoloSel->texA[1] = MTEX(res, 1);
    gSoloSel->texA[11] = MTEX(res, 2);
    gSoloSel->texA[12] = MTEX(res, 3);
    gSoloSel->texA[13] = MTEX(res, 4);
    SS_RES(1);
    gSoloSel->texA[8] = MTEX(res, 0);
    SS_RES(2);
    gSoloSel->texA[9] = MTEX(res, 0);
    gSoloSel->texA[18] = MTEX(res, 2);
    SS_RES(34);
    gSoloSel->texA[2] = MTEX(res, 0);
    gSoloSel->texA[3] = MTEX(res, 1);
    gSoloSel->texA[4] = MTEX(res, 3);
    gSoloSel->texA[14] = MTEX(res, 5);
    gSoloSel->texA[15] = MTEX(res, 6);
    gSoloSel->texA[16] = MTEX(res, 8);
    Flash_Create(&gSoloSel->flash[0], MPACK_AT(gSoloSel->res, 31), gSoloSel->texA);
    Flash_Play(&gSoloSel->flash[0], 1);
    SS_RES(6);
    gSoloSel->texB[0] = MTEX(res, 0);
    gSoloSel->texB[2] = MTEX(res, 1);
    SS_RES(7);
    gSoloSel->texB[1] = MTEX(res, 1);
    gSoloSel->texB[4] = MTEX(res, 2);
    gSoloSel->texB[5] = MTEX(res, 4);
    gSoloSel->texB[6] = MTEX(res, 3);
    SS_RES(8);
    gSoloSel->texB[7] = MTEX(res, 0);
    SS_RES(9);
    gSoloSel->texB[3] = MTEX(res, 0);
    SS_RES(10);
    gSoloSel->texB[10] = MTEX(res, 0);
    gSoloSel->texB[17] = MTEX(res, 1);
    gSoloSel->texC[4] = MTEX(res, 1);
    Flash_Create(&gSoloSel->flash[1], MPACK_AT(gSoloSel->res, 11), gSoloSel->texB);
    Flash_Play(&gSoloSel->flash[1], 1);
    SS_RES(12);
    gSoloSel->texC[9] = MTEX(res, 0);
    gSoloSel->texC[0] = MTEX(res, 1);
    SS_RES(13);
    gSoloSel->texC[7] = MTEX(res, 0);
    gSoloSel->texC[3] = MTEX(res, 1);
    SS_RES(14);
    gSoloSel->texC[2] = MTEX(res, 0);
    gSoloSel->texC[5] = MTEX(res, 1);
    SS_RES(15);
    gSoloSel->texC[6] = MTEX(res, 0);
    gSoloSel->texC[8] = MTEX(res, 1);
    SS_RES(16);
    gSoloSel->texC[10] = MTEX(res, 0);
    gSoloSel->texC[1] = MTEX(res, 1);
    Flash_Create(&gSoloSel->flash[2], MPACK_AT(gSoloSel->res, 17), gSoloSel->texC);
    Flash_Play(&gSoloSel->flash[2], 1);
    ItemPanel_Init((u32 *)MPACK_AT(gSoloSel->res, 22), 0);
    gSoloSel->msgText = MPACK_AT(gSoloSel->res, 25);
    gSoloSel->subtitles = MPACK_AT(gSoloSel->res, 26);
    MsgWin_Init(MPACK_AT(gSoloSel->res, 23), gSoloSel->msgText, 1, 0);
    MsgWin_Open();
    ItemHelp_Init(MPACK_AT(gSoloSel->res, 24));
    gSoloSel->items = MPACK_AT(gCommonRes->data[2], 2);
    gSoloSel->nameText = MPACK_AT(gSoloSel->res, 28);
    gSoloSel->formText = MPACK_AT(gSoloSel->res, 29);
    gSoloSel->chips = (u32 *)MPACK_AT(gSoloSel->res, 27);
    #ifdef PORT
    for (i = 0; i < gSoloSel->chips[0] && i < 250; i++) {
#else
    for (i = 0; i < LCHR_CELL_MAX; i++) {
#endif
        res = (MTexRes *)MPACK_AT(gSoloSel->chips, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    gSoloSel->grid = ((LChrGridList *)MPACK_AT(gSoloSel->res, 30))->cell;
    gSoloSel->gridCount = gSoloSel->res[gSoloSel->res[30] >> 2];
    ChrGrid_Build(&gSoloSel->gridOutCount, gSoloSel->gridBuf, &gSoloSel->gridCount, gSoloSel->grid, NULL, NULL);
    gSoloSel->gridCount = gSoloSel->gridOutCount;
    gSoloSel->grid = gSoloSel->gridBuf;
    {
        s32 cols = SOLOSEL_COLS;
        s32 cols2 = SOLOSEL_COLS;

        gSoloSel->rows = gSoloSel->gridCount / cols;
        if (gSoloSel->gridCount % cols2) {
            gSoloSel->rows++;
        }
    }
    gSoloSel->imageFile = Heap_Alloc(0x16800, 0x40, 0, 2);
    gSoloSel->imageRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    gSoloSel->sel = &gSoloSel->state;
    gSoloSel->sel->entry = SOLO_PROG->lastEntry;
    gSoloSel->loadState = SOLOSEL_LOAD_SHOWN;
    if (!ChrGrid_IsSelectable(gSoloSel->grid, SS_CUR.col + SS_CUR.row * SOLOSEL_COLS)) {
        memset(gSoloSel->sel, 0, sizeof(LSelEntry));
        gSoloSel->sel->entry.col = 0;
        SOLO_PROG->lastEntry = gSoloSel->sel->entry;
    }
    SoloSel_SwapRowTex();
    gSoloSel->sel->image = gSoloSel->sel->rowChara[SS_CUR.col];
    for (i = 0; i < 2; i++) {
        gSoloSel->blink[i] = Rand_Range(0x20);
    }
    gSoloSel->voiceLine = -1;
    File_LoadSync(gSoloSel->sel->image + DISC_RESOURCE(0x2F9, 0x71C), gSoloSel->imageFile, 0x16800);
    Sprite_Unpack(gSoloSel->imageFile, gSoloSel->imageRes, NULL);
    res = gSoloSel->imageRes;
    Res_RelocateOffsets(&res, res, res);
    gSoloSel->texA[17] = MTEX(res, 0);
    gSoloSel->sel->flags |= SOLOSEL_SEL_IMAGE_READY;
    TextBox_Init(&gSoloSel->box[0], gSoloSel->nameText, 1);
    TextBox_SetUnk80(&gSoloSel->box[0], 1);
    TextBox_Init(&gSoloSel->box[1], gSoloSel->formText, 3);
    TextBox_SetUnk80(&gSoloSel->box[1], 1);
}

/* Frees everything SoloSel_Init made. */
void SoloSel_Term(void) {
    s32 i;

    ItemHelp_Term();
    MsgWin_Term();
    ItemPanel_Term(0);
    for (i = 0; i < SOLOSEL_FLASH_NUM; i++) {
        Flash_Destroy(&gSoloSel->flash[i]);
    }
    if (gSoloSel->imageRes != NULL) {
        Heap_Free(gSoloSel->imageRes);
        gSoloSel->imageRes = NULL;
    }
    if (gSoloSel->imageFile != NULL) {
        Heap_Free(gSoloSel->imageFile);
        gSoloSel->imageFile = NULL;
    }
    if (gSoloSel->res != NULL) {
        Heap_Free(gSoloSel->res);
        gSoloSel->res = NULL;
    }
    if (gSoloSel != NULL) {
        Heap_Free(gSoloSel);
        gSoloSel = NULL;
    }
}

/* Sets up every clip of the three movies from the current state and draws the screen. */
void SoloSel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    char part[64];
    s32 i;
    s32 k;
    MFlash *flash;

    Sprite_DrawPicture(gSoloSel->bg, 0, 0, 0x80);
    flash = &gSoloSel->flash[0];
    for (k = 0; k < 2; k++) {
        /* the two guides: both blink, the one talking moves its mouth */
        sprintf(name, "mc_guide_%02dgo", k + 17);
        sprintf(part, "mc_guide_%02dgo_eye", k + 17);
        Flash_FindLabel(flash, name, part, &ref);
        FlashAnim_Blink(flash, &ref, &gSoloSel->blink[k], 0);
        sprintf(part, "mc_guide_%02dgo_mouth", k + 17);
        Flash_FindLabel(flash, name, part, &ref);
        if (gSoloSel->talker == k) {
            FlashAnim_Talk(flash, &ref, &gSoloSel->talk[k], 0);
        } else {
            FlashAnim_ShowNext2(flash, &ref, 0);
        }
    }
    if (gSoloSel->sel->flags & SOLOSEL_SEL_IMAGE_READY) {
        gSoloSel->imageAlpha += 0.075f;
        if (gSoloSel->imageAlpha >= 1.0f) {
            gSoloSel->imageAlpha = 1.0f;
        }
    } else {
        gSoloSel->imageAlpha = 0.0f;
    }
    Flash_FindLabel(flash, NULL, "mc_single_chara_l", &ref);
    Flash_ClipSetAlpha(flash, &ref, gSoloSel->imageAlpha);
    Flash_FindLabel(flash, NULL, "mc_name_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gSoloSel->sel->image, &gSoloSel->box[0]);
    Flash_FindLabel(flash, NULL, "mc_form_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gSoloSel->sel->image, &gSoloSel->box[1]);
    flash = &gSoloSel->flash[2];
    uv.x0 = 0x20;
    uv.y0 = 0;
    uv.x1 = 0x40;
    uv.y1 = 0x20;
    Flash_FindLabel(flash, NULL, "mc_yajirusi", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    for (i = 0; i < 4; i++) {
        /* item set plates: dimmed when they cannot be chosen */
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x100;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_custom_plate_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetColor(flash, &ref, i < gSoloSel->customCount ? 1.0f : 0.3f);
        Flash_FindLabel(flash, name, "mc_custom_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_custom_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (i == 0) {
            Flash_FindLabel(flash, name, "mc_yajirusi", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    for (i = 0; i < 4; i++) {
        /* costume plates */
        uv.x0 = (i % 2) * 0x40;
        uv.y0 = (i / 2) * 0x20;
        uv.x1 = uv.x0 + 0x40;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_color_plate_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (i < gSoloSel->colorCount) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            switch (gSoloSel->colorCount) {
            case 2:
                Flash_ClipSetOffset(flash, &ref, 0x2D, 0);
                break;
            case 3:
                Flash_ClipSetOffset(flash, &ref, 0x16, 0);
                break;
            }
            Flash_FindLabel(flash, name, "mc_color_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_color_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    flash = &gSoloSel->flash[1];
    uv.x0 = 0;
    uv.y0 = 0;
    uv.x1 = 0x40;
    uv.y1 = 0x20;
    Flash_FindLabel(flash, NULL, "mc_plate_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gSoloSel->rows >= 2) {
        for (i = 0; i < 2; i++) {
            uv.x0 = i * 0x20;
            uv.y0 = 0x20;
            uv.x1 = uv.x0 + 0x20;
            uv.y1 = 0x40;
            Flash_FindLabel(flash, i != 0 ? "mc_yajirusi_down" : "mc_yajirusi_up",
                            i != 0 ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, NULL, i != 0 ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
            if (gSoloSel->sel->flags & SOLOSEL_SEL_FORM) {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            }
        }
    } else {
        for (i = 0; i < 2; i++) {
            Flash_FindLabel(flash, NULL, i != 0 ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    Flash_FindLabel(flash, NULL, "mc_chara_mask", &ref);
    if (gSoloSel->sel->rowAnim != 0) {
        Flash_ClipSetFlags(flash, &ref, 0x102, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 0x102, 0);
    }
    for (i = 0; i < 14; i++) {
        sprintf(name, "mc_chara_chip_%03d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetFlags(flash, &ref, 0x80, (u8)gSoloSel->sel->rowAnim);
    }
    Flash_Draw(&gSoloSel->flash[0]);
    Flash_Draw(&gSoloSel->flash[1]);
    Font_FlushAll();
    Flash_Draw(&gSoloSel->flash[2]);
    MsgWin_Draw(0, 0, gSoloSel->voiceLine);
    ItemPanel_Draw(0);
    ItemHelp_Draw(gSoloSel->helpItem);
}

/* Advances the movies and the item panel; ends the chip-row scroll when its clip says so. */
void SoloSel_Update(void) {
    s32 i;

    for (i = 0; i < SOLOSEL_FLASH_NUM; i++) {
        Flash_Advance(&gSoloSel->flash[i]);
    }
    ItemPanel_Update(0);
    if (gSoloSel->sel->rowAnim != 0 && (gSoloSel->flash[1].trig & 1)) {
        gSoloSel->sel->rowAnim = 0;
    }
}

/* Pad 0: grid cursor, form, item set, item panel and costume of the character being chosen. */
void SoloSel_Input(s32 *result) {
    if (!(gSoloSel->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gSoloSel->flags & SOLOSEL_STARTED)) {
        SoloSel_ClipGoto(1, 0, "fl_on_start");
        gSoloSel->flags |= SOLOSEL_STARTED;
    }
    switch (gSoloSel->sel->step) {
    case SOLOSEL_STEP_CHARA:
        if (gPad[0].gameRepeat & 1) {
            SoloSel_ClipGoto(1, 0, "fl_off_start");
            ChrGrid_MoveLeft(gSoloSel->grid, &SS_CUR.col, SS_CUR.row);
            SoloSel_ClipGoto(1, 0, "fl_on_start");
            gSoloSel->sel->image = gSoloSel->sel->rowChara[SS_CUR.col];
            SoloSel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gameRepeat & 2) {
            SoloSel_ClipGoto(1, 0, "fl_off_start");
            ChrGrid_MoveRight(gSoloSel->grid, &SS_CUR.col, SS_CUR.row);
            SoloSel_ClipGoto(1, 0, "fl_on_start");
            gSoloSel->sel->image = gSoloSel->sel->rowChara[SS_CUR.col];
            SoloSel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gameRepeat & 8) {
            if (gSoloSel->rows >= 2) {
                Flash_GotoLabel(&gSoloSel->flash[1], "fl_reel_down", 1);
                gSoloSel->sel->rowAnim = 1;
                SoloSel_ClipGoto(1, 0, "fl_off_start");
                ChrGrid_MoveUp(gSoloSel->grid, &SS_CUR.col, &SS_CUR.row, gSoloSel->rows);
                SoloSel_ClipGoto(1, 0, "fl_on_start");
                SoloSel_SwapRowTex();
                gSoloSel->sel->image = gSoloSel->sel->rowChara[SS_CUR.col];
                SoloSel_ChangeImage();
                Snd_PlaySe(2, 2);
            }
        } else if (gPad[0].gameRepeat & 4) {
            if (gSoloSel->rows >= 2) {
                Flash_GotoLabel(&gSoloSel->flash[1], "fl_reel_up", 1);
                gSoloSel->sel->rowAnim = 1;
                SoloSel_ClipGoto(1, 0, "fl_off_start");
                ChrGrid_MoveDown(gSoloSel->grid, &SS_CUR.col, &SS_CUR.row, gSoloSel->rows);
                SoloSel_ClipGoto(1, 0, "fl_on_start");
                SoloSel_SwapRowTex();
                gSoloSel->sel->image = gSoloSel->sel->rowChara[SS_CUR.col];
                SoloSel_ChangeImage();
                Snd_PlaySe(2, 1);
            }
        } else if (gPad[0].gamePressed & 0x200) {
            if (SS_CELL.formCount != 0) {
                gSoloSel->sel->flags |= SOLOSEL_SEL_FORM;
                if (SS_CUR.form >= SS_CELL.formCount) {
                    SS_CUR.form = 0;
                }
                Flash_GotoLabel(&gSoloSel->flash[1], "fl_form", 1);
                gSoloSel->sel->rowAnim = 1;
                SoloSel_ClipGoto(1, 0, "fl_off_start");
                SoloSel_ClipGoto(1, 1, "fl_on_start");
                SoloSel_SetRowTex();
                gSoloSel->sel->image = gSoloSel->sel->rowChara[SS_CUR.form];
                SoloSel_ChangeImage();
                gSoloSel->sel->step = SOLOSEL_STEP_FORM;
                Snd_PlaySe(2, 0x29);
            } else {
                Flash_GotoLabel(&gSoloSel->flash[2], "fl_custom_in", 1);
                SoloSel_ClipGoto(1, 0, "fl_ok");
                gSoloSel->customCount = (gSoloSel->flags & SOLOSEL_NO_CUSTOM) ? 1 : 4;
                if (SS_CUR.custom >= gSoloSel->customCount) {
                    SS_CUR.custom = 0;
                }
                gSoloSel->colorCount = ChrTbl_WrapCostume(gSoloSel->sel->image, &SS_CUR.costume);
                SoloSel_ClipGoto(2, 2, "fl_on_start");
                gSoloSel->sel->step = SOLOSEL_STEP_CUSTOM;
                Snd_PlaySe(1, 1);
            }
        } else if (gPad[0].gamePressed & 0x400) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Snd_PlaySe(1, 2);
        }
        break;
    case SOLOSEL_STEP_FORM:
        if (gPad[0].gameRepeat & 1) {
            SoloSel_ClipGoto(1, 1, "fl_off_start");
            ChrGrid_PrevForm(gSoloSel->sel->rowChara, &SS_CUR.form);
            SoloSel_ClipGoto(1, 1, "fl_on_start");
            gSoloSel->sel->image = gSoloSel->sel->rowChara[SS_CUR.form];
            SoloSel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gameRepeat & 2) {
            SoloSel_ClipGoto(1, 1, "fl_off_start");
            ChrGrid_NextForm(gSoloSel->sel->rowChara, &SS_CUR.form);
            SoloSel_ClipGoto(1, 1, "fl_on_start");
            gSoloSel->sel->image = gSoloSel->sel->rowChara[SS_CUR.form];
            SoloSel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&gSoloSel->flash[2], "fl_custom_in", 1);
            SoloSel_ClipGoto(1, 1, "fl_ok");
            gSoloSel->customCount = (gSoloSel->flags & SOLOSEL_NO_CUSTOM) ? 1 : 4;
            if (SS_CUR.custom >= gSoloSel->customCount) {
                SS_CUR.custom = 0;
            }
            SoloSel_ClipGoto(2, 2, "fl_on_start");
            gSoloSel->colorCount = ChrTbl_WrapCostume(gSoloSel->sel->image, &SS_CUR.costume);
            gSoloSel->sel->step = SOLOSEL_STEP_CUSTOM;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gSoloSel->sel->flags ^= SOLOSEL_SEL_FORM;
            Flash_GotoLabel(&gSoloSel->flash[1], "fl_form", 1);
            gSoloSel->sel->rowAnim = 1;
            SoloSel_ClipGoto(1, 1, "fl_off_start");
            SoloSel_ClipGoto(1, 0, "fl_on_start");
            SoloSel_SwapRowTex();
            gSoloSel->sel->image = gSoloSel->sel->rowChara[SS_CUR.col];
            SoloSel_ChangeImage();
            gSoloSel->sel->step = SOLOSEL_STEP_CHARA;
            Snd_PlaySe(2, 0x29);
        }
        break;
    case SOLOSEL_STEP_CUSTOM:
        if (gPad[0].gameRepeat & 8) {
            if (gSoloSel->customCount >= 2) {
                SoloSel_ClipGoto(2, 2, "fl_off_start");
                SS_CUR.custom--;
                if (SS_CUR.custom < 0) {
                    SS_CUR.custom = gSoloSel->customCount - 1;
                }
                SoloSel_ClipGoto(2, 2, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & 4) {
            if (gSoloSel->customCount >= 2) {
                SoloSel_ClipGoto(2, 2, "fl_off_start");
                SS_CUR.custom++;
                if (SS_CUR.custom >= gSoloSel->customCount) {
                    SS_CUR.custom = 0;
                }
                SoloSel_ClipGoto(2, 2, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gamePressed & 2) {
            /* look at the items of the set */
            if (SS_CUR.custom > 0) {
                SoloSel_ClipGoto(2, 2, "fl_ok");
                ItemPanel_SetChara(0, gSoloSel->sel->image, SS_CUR.col + SS_CUR.row * SOLOSEL_COLS, SS_CUR.custom - 1,
                                   0);
                ItemPanel_Show(0);
                gSoloSel->sel->flags |= SOLOSEL_SEL_PANEL;
                gSoloSel->sel->step = SOLOSEL_STEP_PANEL;
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&gSoloSel->flash[2], "fl_color_in", 1);
            SoloSel_ClipGoto(2, 2, "fl_ok");
            SoloSel_ClipGoto(2, 5, "fl_on_start");
            gSoloSel->sel->step = SOLOSEL_STEP_COLOR;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gSoloSel->flash[2], "fl_custom_cansel", 1);
            SoloSel_ClipGoto(2, 2, "fl_off_start");
            if (gSoloSel->sel->flags & SOLOSEL_SEL_FORM) {
                SoloSel_ClipGoto(1, 1, "fl_on_start");
                gSoloSel->sel->step = SOLOSEL_STEP_FORM;
            } else {
                SoloSel_ClipGoto(1, 0, "fl_on_start");
                gSoloSel->sel->step = SOLOSEL_STEP_CHARA;
            }
            Snd_PlaySe(1, 2);
        }
        break;
    case SOLOSEL_STEP_PANEL: {
        s32 item = ItemPanel_Input(0, 0);

        if (item > 0) {
            ItemHelp_Open();
            gSoloSel->helpItem = item - 1;
            gSoloSel->sel->step = SOLOSEL_STEP_HELP;
        } else if (item < 0) {
            ItemPanel_Hide(0);
            SoloSel_ClipGoto(2, 2, "fl_on_start");
            gSoloSel->sel->step = SOLOSEL_STEP_CUSTOM;
        }
        break;
    }
    case SOLOSEL_STEP_HELP:
        if (gPad[0].gamePressed & 0x600) {
            ItemHelp_Close();
            gSoloSel->sel->step = SOLOSEL_STEP_PANEL;
            Snd_PlaySe(1, 2);
        }
        break;
    case SOLOSEL_STEP_COLOR:
        if (gPad[0].gameRepeat & 1) {
            SoloSel_ClipGoto(2, 5, "fl_off_start");
            SS_CUR.costume--;
            if (SS_CUR.costume < 0) {
                SS_CUR.costume = gSoloSel->colorCount - 1;
            }
            SoloSel_ClipGoto(2, 5, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 2) {
            SoloSel_ClipGoto(2, 5, "fl_off_start");
            SS_CUR.costume++;
            if (SS_CUR.costume >= gSoloSel->colorCount) {
                SS_CUR.costume = 0;
            }
            SoloSel_ClipGoto(2, 5, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            /* the character is decided: fix it and take the item set along */
            gSoloSel->sel->rowAnim = 0;
            Flash_GotoLabel(&gSoloSel->flash[2], "fl_color_ok", 1);
            Flash_GotoLabel(&gSoloSel->flash[1], "fl_out", 1);
            SoloSel_ClipGoto(2, 5, "fl_ok");
            SoloSel_ClipGoto(2, 2, "fl_off_start");
            if (gSoloSel->sel->flags & SOLOSEL_SEL_FORM) {
                SS_CUR.chara = gSoloSel->sel->rowChara[SS_CUR.form];
                if (SS_CUR.custom != 0) {
                    SS_CUR.items = SAVE_CUSTOM_AT(LSAVE, SS_CUR.col + SS_CUR.row * SOLOSEL_COLS).set[SS_CUR.custom - 1];
                } else {
                    memset(&SS_CUR.items, 0, sizeof(LItemSet));
                }
                SoloSel_ClipGoto(1, 1, "fl_off_start");
            } else {
                SS_CUR.chara = gSoloSel->sel->rowChara[SS_CUR.col];
                if (SS_CUR.custom != 0) {
                    SS_CUR.items = SAVE_CUSTOM_AT(LSAVE, SS_CUR.col + SS_CUR.row * SOLOSEL_COLS).set[SS_CUR.custom - 1];
                } else {
                    memset(&SS_CUR.items, 0, sizeof(LItemSet));
                }
                SoloSel_ClipGoto(1, 0, "fl_off_start");
            }
            gSoloSel->endStep = SOLOSEL_END_SPEAK;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gSoloSel->flash[2], "fl_color_cansel", 1);
            SoloSel_ClipGoto(2, 5, "fl_off_start");
            SoloSel_ClipGoto(2, 2, "fl_on_start");
            gSoloSel->sel->step = SOLOSEL_STEP_CUSTOM;
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

/* Once the fighter is chosen: the guide's closing line, then the leave timer. */
void SoloSel_UpdateEnd(void) {
    s32 step = gSoloSel->endStep;

    if (step == SOLOSEL_END_NONE) {
        return;
    }
    switch (step) {
    case SOLOSEL_END_SPEAK:
        if (gProgress->mode == 21) {
            gSoloSel->talker = 1;
            gSoloSel->voiceLine = 0x17;
            if (gSoloSel->sel->image == 0x66) {
                gSoloSel->voiceLine++;
            }
        } else {
            gSoloSel->talker = 0;
            gSoloSel->voiceLine = 0x37;
        }
        Voice_PlayWithSubtitle(gSoloSel->subtitles, SOLO_VOICE_BASE, gSoloSel->voiceLine);
        gSoloSel->endStep++;
        break;
    case SOLOSEL_END_WAIT:
        if (Voice_GetStat(0) == SOLO_VOICE_IDLE) {
            gSoloSel->endStep++;
        } else if (gPad[0].gamePressed & 0x200) {
            gSoloSel->endStep++;
            Snd_PlaySe(1, 1);
        }
        break;
    case SOLOSEL_END_LEAVE:
        gSoloSel->flags |= SOLOSEL_DONE;
        gSoloSel->flags |= SOLOSEL_LEAVING;
        gSoloSel->timer = 15;
        gSoloSel->endStep = SOLOSEL_END_NONE;
        break;
    }
}

/*
 * The one-character select (modes 15 with a team size below 2, 18, 21, 25 and 29). Returns 1 when a fighter was
 * chosen (the choice is then in gProgress->team[0]), 0 when the player backed out.
 */
s32 SoloSel_Run(s32 section) {
    s32 result = 1;

    SoloSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        SoloSel_UpdateImage();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            SoloSel_Update();
            SoloSel_UpdateEnd();
        }
        SoloSel_Draw();
        Font_FlushAll();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gSoloSel->flags & SOLOSEL_GREETED) && (gSoloSel->flash[0].flags & MFLASH_PAD)) {
                gSoloSel->flags |= SOLOSEL_GREETED;
                if (gProgress->mode == 21) {
                    gSoloSel->talker = 1;
                    gSoloSel->voiceLine = 0x16;
                    Voice_PlayWithSubtitle(gSoloSel->subtitles, SOLO_VOICE_BASE, gSoloSel->voiceLine);
                } else {
                    gSoloSel->talker = 0;
                    gSoloSel->voiceLine = 0x36;
                    Voice_PlayWithSubtitle(gSoloSel->subtitles, SOLO_VOICE_BASE, gSoloSel->voiceLine);
                }
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gSoloSel->loadState != SOLOSEL_LOAD_SHOWN) {
                continue;
            }
            break;
        }
        if (gSoloSel->flags & SOLOSEL_LEAVING) {
            if (--gSoloSel->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                /* hand the choice to the mode */
                SOLO_PROG->lastEntry = gSoloSel->sel->entry;
            }
        } else if (gSoloSel->endStep == SOLOSEL_END_NONE) {
            SoloSel_Input(&result);
        }
    }
    SoloSel_Term();
    Dma_ResetBuffers();
    return result;
}
