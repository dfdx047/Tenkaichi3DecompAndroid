#include "common.h"
#include "menu/menu_i.h"
#include "menu/menu_j.h"
#include "sys/pad.h"
#include "sys/save.h"

ESel *gEntrySel = NULL; /* 0x3B5910 */

/*
 * EntrySel, 0x35E0F8..0x3623A8: the entrant select of the tournament mode ("Dragon World Tour", mode 34) and,
 * last, Tour_Main (0x362160), the handler of modes 33..35 that runs it. One object: the chunk cut at 0x35F650
 * had put everything after Init in menu_j.c, merged in below. Its .data is the pointer gEntrySel (0x3B5910), its
 * .rodata runs from 0x3B5920 to 0x3B5DB0. The work area is `ESel` (menu/menu_j.h).
 */

#define ES_CUR (gEntrySel->sel->entry[gEntrySel->sel->cur])
#define ES_CELL (gEntrySel->grid[ES_CUR.row * ESEL_COLS + ES_CUR.col])

/* The cursor moved to another grid row: the seven chips take the characters of the new row. */
void EntrySel_SwapRowTex(void) {
    s32 tbl[ESEL_COLS] = {8, 11, 12, 13, 14, 15, 16};
    MTexRes *res;
    s32 i;

    for (i = 0; i < ESEL_COLS; i++) {
        s32 chara =
            gEntrySel->grid[ES_CUR.row * ESEL_COLS + i].id;

        gEntrySel->sel->prevRowChara[i] = gEntrySel->sel->rowChara[i];
        gEntrySel->sel->rowChara[i] = chara;
        res = (MTexRes *)MPACK_AT(gEntrySel->chips, chara + 1);
        gEntrySel->texB[18 + i] = gEntrySel->texB[tbl[i]];
        gEntrySel->texB[tbl[i]] = MTEX(res, 0);
    }
}

/* The seven chips take the forms of the cell under the cursor (empty chips past the last form). */
void EntrySel_SetRowTex(void) {
    s32 tbl[ESEL_COLS] = {8, 11, 12, 13, 14, 15, 16};
    MTexRes *res;
    s32 i;

    for (i = 0; i < ESEL_COLS; i++) {
        s32 chara;

        if (i < ES_CELL.formCount) {
            chara = ES_CELL.form[i];
        } else {
            chara = ESEL_ID_NONE;
        }
        gEntrySel->sel->prevRowChara[i] = gEntrySel->sel->rowChara[i];
        gEntrySel->sel->rowChara[i] = chara;
        res = (MTexRes *)MPACK_AT(gEntrySel->chips, chara + 1);
        gEntrySel->texB[18 + i] = gEntrySel->texB[tbl[i]];
        gEntrySel->texB[tbl[i]] = MTEX(res, 0);
    }
}

/* Sets the chips of the entrants chosen so far and clears the rest. */
void EntrySel_SetMemberTex(void) {
    MTexRes *res;
    s32 i;

    for (i = 0; i < ESEL_ENTRY_MAX; i++) {
        gEntrySel->texA[10 + i] = NULL;
    }
    for (i = 0; i < gEntrySel->sel->cur; i++) {
        res = (MTexRes *)MPACK_AT(gEntrySel->chips, gEntrySel->sel->entry[i].chara + 1);
        gEntrySel->texA[10 + i] = MTEX(res, 0);
    }
}

/* One step of the background loader of the large picture (file 0x2F9 + character). */
void EntrySel_UpdateImage(void) {
    MTexRes *res;

    switch (gEntrySel->loadState) {
    case ESEL_LOAD_ABORT:
        gEntrySel->loadState = ESEL_LOAD_RESTART;
        break;
    case ESEL_LOAD_RESTART:
        if (gEntrySel->sel->flags & ESEL_SEL_IMAGE_CHANGE) {
            gEntrySel->sel->flags ^= ESEL_SEL_IMAGE_CHANGE;
        }
        gEntrySel->loadState = ESEL_LOAD_REQUEST;
        break;
    case ESEL_LOAD_REQUEST:
        File_CancelRequests();
        File_Request(gEntrySel->sel->image + 0x2F9, gEntrySel->imageFile, 0x16800);
        gEntrySel->loadState = ESEL_LOAD_READ;
        break;
    case ESEL_LOAD_READ:
        if (File_UpdateRequests()) {
            gEntrySel->loadState = ESEL_LOAD_UNPACK;
        }
        break;
    case ESEL_LOAD_UNPACK:
        Sprite_Unpack(gEntrySel->imageFile, gEntrySel->imageRes, NULL);
        res = gEntrySel->imageRes;
        Res_RelocateOffsets(&res, res, res);
        gEntrySel->texA[5] = MTEX(res, 0);
        gEntrySel->sel->flags |= ESEL_SEL_IMAGE_READY;
        gEntrySel->loadState = ESEL_LOAD_SHOWN;
        break;
    case ESEL_LOAD_SHOWN:
        if (gEntrySel->sel->flags & ESEL_SEL_IMAGE_CHANGE) {
            gEntrySel->texA[5] = NULL;
            gEntrySel->loadState = ESEL_LOAD_RESTART;
        }
        break;
    }
}

/* The shown character changed: hide the picture, or abort a load in progress, and ask for the new one. */
void EntrySel_ChangeImage(void) {
    if (gEntrySel->sel->flags & ESEL_SEL_IMAGE_READY) {
        gEntrySel->sel->flags ^= ESEL_SEL_IMAGE_READY;
    } else {
        gEntrySel->loadState = ESEL_LOAD_ABORT;
    }
    gEntrySel->sel->flags |= ESEL_SEL_IMAGE_CHANGE;
}

/* Sends a clip of movie `movie` to a label: the chip, plate or text of the entrant being chosen. */
void EntrySel_ClipGoto(s32 movie, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gEntrySel->flash[movie];

    switch (kind) {
    case 0:
        sprintf(name, "mc_chara_chip_%03d", ES_CUR.col);
        break;
    case 1:
        sprintf(name, "mc_chara_chip_%03d", ES_CUR.form);
        break;
    case 2:
        sprintf(name, "mc_custom_plate_%d", ES_CUR.custom + 1);
        break;
    case 5:
        sprintf(name, "mc_color_plate_%d", ES_CUR.costume + 1);
        break;
    case 7:
        sprintf(name, "mc_entry_text_%d", gEntrySel->sel->cur);
        break;
    case 3:
        return;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

#define ES_RES(n) \
    res = (MTexRes *)MPACK_AT(gEntrySel->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 4), builds its four movies and the grid. */
void EntrySel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 movie;
    s32 i;

    gEntrySel = Heap_Alloc(0x1C48, 0x20, 0, 2);
    memset(gEntrySel, 0, 0x1C48);
    gEntrySel->pack = (u32 *)MPACK_AT(gMenuArc4, section);
    gEntrySel->res = Sprite_Unpack(gEntrySel->pack, NULL, NULL);
    gEntrySel->file = File_LoadSync(TOUR_PROG->t.tour + 0x3C9, NULL, 0);
    TourBg_Init(gEntrySel->file, TOUR_PROG->t.tour, NULL);
    ES_RES(1);
    gEntrySel->texA[0] = MTEX(res, 0);
    ES_RES(2);
    gEntrySel->texA[1] = MTEX(res, 0);
    gEntrySel->texA[3] = MTEX(res, 1);
    gEntrySel->texA[18] = MTEX(res, 2);
    gEntrySel->texB[3] = MTEX(res, 1);
    ES_RES(4);
    gEntrySel->texA[4] = MTEX(res, 0);
    ES_RES(5);
    gEntrySel->texA[6] = MTEX(res, 0);
    gEntrySel->texA[7] = MTEX(res, 2);
    Flash_Create(&gEntrySel->flash[0], MPACK_AT(gEntrySel->res, 3), gEntrySel->texA);
    Flash_Play(&gEntrySel->flash[0], 1);
    ES_RES(6);
    gEntrySel->texB[0] = MTEX(res, 0);
    gEntrySel->texB[2] = MTEX(res, 1);
    ES_RES(7);
    gEntrySel->texB[1] = MTEX(res, 1);
    gEntrySel->texB[4] = MTEX(res, 2);
    gEntrySel->texB[5] = MTEX(res, 4);
    gEntrySel->texB[6] = MTEX(res, 3);
    ES_RES(8);
    gEntrySel->texB[7] = MTEX(res, 0);
    ES_RES(9);
    gEntrySel->texB[10] = MTEX(res, 0);
    gEntrySel->texB[17] = MTEX(res, 1);
    gEntrySel->texA[2] = MTEX(res, 0);
    gEntrySel->texC[4] = MTEX(res, 1);
    Flash_Create(&gEntrySel->flash[1], MPACK_AT(gEntrySel->res, 10), gEntrySel->texB);
    Flash_Play(&gEntrySel->flash[1], 1);
    ES_RES(11);
    gEntrySel->texC[9] = MTEX(res, 0);
    gEntrySel->texC[0] = MTEX(res, 1);
    ES_RES(12);
    gEntrySel->texC[7] = MTEX(res, 0);
    gEntrySel->texC[3] = MTEX(res, 1);
    ES_RES(13);
    gEntrySel->texC[2] = MTEX(res, 0);
    gEntrySel->texC[5] = MTEX(res, 1);
    ES_RES(14);
    gEntrySel->texC[6] = MTEX(res, 0);
    gEntrySel->texC[8] = MTEX(res, 1);
    ES_RES(15);
    gEntrySel->texC[10] = MTEX(res, 0);
    gEntrySel->texC[1] = MTEX(res, 1);
    Flash_Create(&gEntrySel->flash[2], MPACK_AT(gEntrySel->res, 16), gEntrySel->texC);
    Flash_Play(&gEntrySel->flash[2], 1);

    movie = 0;
    switch (TOUR_PROG->t.tour) {
    case 0:
        ES_RES(17);
        gEntrySel->texD[0] = MTEX(res, 0);
        gEntrySel->texD[1] = MTEX(res, 1);
        movie = 18;
        break;
    case 1:
        ES_RES(19);
        gEntrySel->texD[0] = MTEX(res, 0);
        gEntrySel->texD[2] = MTEX(res, 1);
        gEntrySel->texD[1] = MTEX(res, 3);
        movie = 20;
        break;
    case 2:
        ES_RES(21);
        gEntrySel->texD[0] = MTEX(res, 0);
        gEntrySel->texD[2] = MTEX(res, 1);
        gEntrySel->texD[1] = MTEX(res, 3);
        movie = 22;
        break;
    case 3:
        ES_RES(23);
        gEntrySel->texD[0] = MTEX(res, 0);
        gEntrySel->texD[1] = MTEX(res, 1);
        movie = 24;
        break;
    case 4:
        ES_RES(26);
        gEntrySel->texD[0] = MTEX(res, 0);
        gEntrySel->texD[2] = MTEX(res, 1);
        gEntrySel->texD[1] = MTEX(res, 3);
        ES_RES(28);
        gEntrySel->texD[6] = MTEX(res, 0);
        gEntrySel->texD[8] = MTEX(res, 1);
        gEntrySel->texD[7] = MTEX(res, 3);
        movie = 25;
        break;
    }
    Flash_Create(&gEntrySel->flash[3], MPACK_AT(gEntrySel->res, movie), gEntrySel->texD);
    Flash_Play(&gEntrySel->flash[3], 1);
    Flash_GotoLabel(&gEntrySel->flash[3], "fl_guide_in", 1);
    ItemPanel_Init((u32 *)MPACK_AT(gEntrySel->res, 39), 0);
    ItemHelp_Init(MPACK_AT(gEntrySel->res, 35));
    gEntrySel->msgText = MPACK_AT(gEntrySel->res, 33);
    gEntrySel->subtitles = MPACK_AT(gEntrySel->res, 36);
    MsgWin_Init(MPACK_AT(gEntrySel->res, 34), gEntrySel->msgText, 0, 0);
    MsgWin_Open();
    gEntrySel->items = MPACK_AT(gCommonRes->data[2], 2);
    gEntrySel->nameText = MPACK_AT(gEntrySel->res, 30);
    gEntrySel->formText = MPACK_AT(gEntrySel->res, 31);
    gEntrySel->chips = (u32 *)MPACK_AT(gEntrySel->res, 32);
    for (i = 0; i < ESEL_CELL_MAX; i++) {
        res = (MTexRes *)MPACK_AT(gEntrySel->chips, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    gEntrySel->grid = ((ESelGridList *)MPACK_AT(gEntrySel->res, 29))->cell;
    gEntrySel->gridCount = gEntrySel->res[gEntrySel->res[29] >> 2];
    ChrGrid_Build(&gEntrySel->gridOutCount, gEntrySel->gridBuf, &gEntrySel->gridCount, gEntrySel->grid, NULL, NULL);
    gEntrySel->gridCount = gEntrySel->gridOutCount;
    gEntrySel->grid = gEntrySel->gridBuf;
    {
        s32 cols = ESEL_COLS;
        s32 cols2 = ESEL_COLS;

        gEntrySel->rows = gEntrySel->gridCount / cols;
        if (gEntrySel->gridCount % cols2) {
            gEntrySel->rows++;
        }
    }
    gEntrySel->imageFile = Heap_Alloc(0x16800, 0x40, 0, 2);
    gEntrySel->imageRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    gEntrySel->sel = &gEntrySel->state;
    gEntrySel->sel->entry[0] = TOUR_PROG->lastEntry;
    gEntrySel->loadState = ESEL_LOAD_SHOWN;
    if (!ChrGrid_IsSelectable(gEntrySel->grid, ES_CUR.col + ES_CUR.row * ESEL_COLS)) {
        memset(gEntrySel->sel, 0, sizeof(ESelEntry));
        gEntrySel->sel->entry[0].col = 0;
        TOUR_PROG->lastEntry = gEntrySel->sel->entry[0];
    }
    switch (TOUR_PROG->t.tour) {
    case 0:
    case 1:
    case 2:
    case 3:
        break;
    case 4:
        memset(gEntrySel->sel, 0, sizeof(ESelEntry));
        for (i = 0; i < TOUR_PROG->t.entryNum; i++) {
            s32 cols = ESEL_COLS;
            s32 n;

            while (1) {
                n = Rand_Range(gEntrySel->gridCount);
                if (gEntrySel->grid[n].id <= 0xA0) {
                    gEntrySel->sel->entry[i].col = n % cols;
                    gEntrySel->sel->entry[i].row = n / cols;
                    gEntrySel->sel->entry[i].custom = Rand_Range(4);
                    if (gEntrySel->grid[n].formCount != 0) {
                        gEntrySel->sel->entry[i].form = Rand_Range(gEntrySel->grid[n].formCount);
                    }
                    gEntrySel->sel->entry[i].costume = Rand_Range(ChrTbl_WrapCostume(gEntrySel->grid[n].id, NULL));
                    break;
                }
            }
        }
        break;
    }
    EntrySel_SwapRowTex();
    gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
    gEntrySel->voiceLine = -1;
    for (i = 0; i < 2; i++) {
        gEntrySel->blink[i] = Rand_Range(0x20);
    }
    File_LoadSync(gEntrySel->sel->image + 0x2F9, gEntrySel->imageFile, 0x16800);
    Sprite_Unpack(gEntrySel->imageFile, gEntrySel->imageRes, NULL);
    res = gEntrySel->imageRes;
    Res_RelocateOffsets(&res, res, res);
    gEntrySel->texA[5] = MTEX(res, 0);
    gEntrySel->sel->flags |= ESEL_SEL_IMAGE_READY;
    memset(TOUR_PROG->t.entrant, 0, 0x2A8);
    TextBox_Init(&gEntrySel->box[0], gEntrySel->nameText, 1);
    TextBox_SetUnk80(&gEntrySel->box[0], 1);
    TextBox_Init(&gEntrySel->box[1], gEntrySel->formText, 3);
    TextBox_SetUnk80(&gEntrySel->box[1], 1);
}

/*
 * 0x35F650..0x3623A8: the rest of the entrant select of Dragon World Tour (progress mode 34) and the handler of
 * modes 33..35; this part was menu_j.c until the merge.
 */

/* Frees everything EntrySel_Init made. */
void EntrySel_Term(void) {
    s32 i;

    ItemHelp_Term();
    MsgWin_Term();
    ItemPanel_Term(0);
    TourBg_Term();
    for (i = 0; i < ESEL_FLASH_NUM; i++) {
        Flash_Destroy(&gEntrySel->flash[i]);
    }
    if (gEntrySel->imageFile != NULL) {
        Heap_Free(gEntrySel->imageFile);
        gEntrySel->imageFile = NULL;
    }
    if (gEntrySel->imageRes != NULL) {
        Heap_Free(gEntrySel->imageRes);
        gEntrySel->imageRes = NULL;
    }
    if (gEntrySel->file != NULL) {
        Heap_Free(gEntrySel->file);
        gEntrySel->file = NULL;
    }
    if (gEntrySel->res != NULL) {
        Heap_Free(gEntrySel->res);
        gEntrySel->res = NULL;
    }
    if (gEntrySel != NULL) {
        Heap_Free(gEntrySel);
        gEntrySel = NULL;
    }
}

/* Sets up every clip of the four movies from the current state and draws the screen. */
void EntrySel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    s32 i;
    MFlash *flash;

    TourBg_Draw();
    flash = &gEntrySel->flash[0];
    if (gEntrySel->sel->flags & ESEL_SEL_IMAGE_READY) {
        gEntrySel->imageAlpha += 0.075f;
        if (gEntrySel->imageAlpha >= 1.0f) {
            gEntrySel->imageAlpha = 1.0f;
        }
    } else {
        gEntrySel->imageAlpha = 0.0f;
    }
    Flash_FindLabel(flash, NULL, "mc_single_chara_l", &ref);
    Flash_ClipSetAlpha(flash, &ref, gEntrySel->imageAlpha);
    Flash_FindLabel(flash, NULL, "mc_name_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gEntrySel->sel->image, &gEntrySel->box[0]);
    Flash_FindLabel(flash, NULL, "mc_form_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gEntrySel->sel->image, &gEntrySel->box[1]);
    for (i = 0; i < ESEL_ENTRY_MAX; i++) {
        /* the entry list: "entry n" for those still to choose, the "decided" text for the chosen ones */
        sprintf(name, "mc_entry_text_%d", i);
        if (i >= TOUR_PROG->t.entryNum || i < gEntrySel->sel->cur) {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        } else {
            uv.x0 = (i % 2) * 0x40;
            uv.y0 = (i / 2) * 0x20;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = uv.y0 + 0x20;
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_FindLabel(flash, name, "mc_entry_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_entry_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
        }
        sprintf(name, "mc_entry_decision_text_%d", i);
        if (i < gEntrySel->sel->cur) {
            uv.x0 = (i % 2) * 0x40;
            uv.y0 = (i / 2) * 0x20;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = uv.y0 + 0x20;
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_ClipSetUv(flash, &ref, &uv);
        } else {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    flash = &gEntrySel->flash[2];
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
        Flash_ClipSetColor(flash, &ref, i < gEntrySel->customCount ? 1.0f : 0.3f);
        Flash_FindLabel(flash, name, "mc_custom_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_custom_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (i == 0) {
            Flash_FindLabel(flash, name, "mc_yajirusi", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        } else {
            Flash_FindLabel(flash, name, "mc_yajirusi", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, ES_CELL.id != ESEL_ID_RANDOM);
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
        if (i < gEntrySel->colorCount) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            switch (gEntrySel->colorCount) {
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
    flash = &gEntrySel->flash[1];
    if (TOUR_PROG->t.entryNum - 1 < gEntrySel->sel->cur) {
        uv.x0 = ((gEntrySel->sel->cur - 1) % 2) * 0x40;
        uv.y0 = ((gEntrySel->sel->cur - 1) / 2) * 0x20;
        uv.x1 = uv.x0 + 0x40;
        uv.y1 = uv.y0 + 0x20;
    } else {
        uv.x0 = (gEntrySel->sel->cur % 2) * 0x40;
        uv.y0 = (gEntrySel->sel->cur / 2) * 0x20;
        uv.x1 = uv.x0 + 0x40;
        uv.y1 = uv.y0 + 0x20;
    }
    Flash_FindLabel(flash, NULL, "mc_plate_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gEntrySel->rows >= 2) {
        for (i = 0; i < 2; i++) {
            uv.x0 = i * 0x20;
            uv.y0 = 0x20;
            uv.x1 = uv.x0 + 0x20;
            uv.y1 = 0x40;
            Flash_FindLabel(flash, i != 0 ? "mc_yajirusi_down" : "mc_yajirusi_up",
                            i != 0 ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if ((gEntrySel->sel->flags & ESEL_SEL_FORM) || TOUR_PROG->t.tour == TOUR_YAMCHA) {
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
    if (gEntrySel->sel->rowAnim != 0) {
        Flash_ClipSetFlags(flash, &ref, 0x102, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 0x102, 0);
    }
    for (i = 0; i < 14; i++) {
        sprintf(name, "mc_chara_chip_%03d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetFlags(flash, &ref, 0x80, (u8)gEntrySel->sel->rowAnim);
    }
    flash = &gEntrySel->flash[3];
    switch (TOUR_PROG->t.tour) {
    case TOUR_WORLD:
        Flash_FindLabel(flash, NULL, "mc_guide_tenkaichi_mouth", &ref);
        FlashAnim_Talk(flash, &ref, &gEntrySel->talk[0], 0);
        break;
    case TOUR_BIG:
        Flash_FindLabel(flash, NULL, "mc_guide_ceru_01_eye", &ref);
        FlashAnim_Blink(flash, &ref, &gEntrySel->blink[0], 0);
        Flash_FindLabel(flash, NULL, "mc_guide_ceru_01_mouth", &ref);
        FlashAnim_Talk(flash, &ref, &gEntrySel->talk[0], 0);
        break;
    case TOUR_CELL:
        Flash_FindLabel(flash, NULL, "mc_guide_ceru_01_eye", &ref);
        FlashAnim_Blink(flash, &ref, &gEntrySel->blink[0], 0);
        Flash_FindLabel(flash, NULL, "mc_guide_ceru_01_mouth", &ref);
        FlashAnim_Talk(flash, &ref, &gEntrySel->talk[0], 0);
        break;
    case TOUR_OTHERWORLD:
        Flash_FindLabel(flash, NULL, "mc_guide_anoyo_mouth", &ref);
        FlashAnim_Talk(flash, &ref, &gEntrySel->talk[0], 0);
        break;
    case TOUR_YAMCHA:
        for (i = 0; i < 2; i++) {
            Flash_FindLabel(flash, NULL, i != 0 ? "mc_guide_puaru_eye01" : "mc_guide_yamucha_eye", &ref);
            FlashAnim_Blink(flash, &ref, &gEntrySel->blink[i], 0);
            Flash_FindLabel(flash, NULL, i != 0 ? "mc_guide_puaru_mouth01" : "mc_guide_yamucha_mouth", &ref);
            if (i == gEntrySel->talker) {
                FlashAnim_Talk(flash, &ref, &gEntrySel->talk[i], 0);
            } else {
                FlashAnim_ShowNext2(flash, &ref, 0);
            }
        }
        break;
    }
    Flash_Draw(&gEntrySel->flash[0]);
    Font_FlushAll();
    Flash_Draw(&gEntrySel->flash[1]);
    Flash_Draw(&gEntrySel->flash[2]);
    MsgWin_Draw(0, 0, gEntrySel->voiceLine);
    Flash_Draw(&gEntrySel->flash[3]);
    ItemPanel_Draw(0);
    ItemHelp_Draw(gEntrySel->helpItem);
}

/* Advances the movies and the item panel; ends the chip-row scroll when its clip says so. */
void EntrySel_Update(void) {
    s32 i;

    for (i = 0; i < ESEL_FLASH_NUM; i++) {
        Flash_Advance(&gEntrySel->flash[i]);
    }
    ItemPanel_Update(0);
    if (gEntrySel->sel->rowAnim != 0 && (gEntrySel->flash[1].trig & 1)) {
        gEntrySel->sel->rowAnim = 0;
    }
}

/* Pad 0: grid cursor, form, item set, item panel and costume of the entrant being chosen. */
void EntrySel_Input(s32 *result) {
    if (!(gEntrySel->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gEntrySel->flags & ESEL_STARTED)) {
        EntrySel_ClipGoto(1, 0, "fl_on_start");
        EntrySel_ClipGoto(0, 7, "fl_loop");
        gEntrySel->flags |= ESEL_STARTED;
    }
    switch (gEntrySel->sel->step) {
    case ESEL_STEP_CHARA:
        if ((gPad[0].gameRepeat & 1) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(1, 0, "fl_off_start");
            ChrGrid_MoveLeft(gEntrySel->grid, &ES_CUR.col, ES_CUR.row);
            EntrySel_ClipGoto(1, 0, "fl_on_start");
            gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
            EntrySel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if ((gPad[0].gameRepeat & 2) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(1, 0, "fl_off_start");
            ChrGrid_MoveRight(gEntrySel->grid, &ES_CUR.col, ES_CUR.row);
            EntrySel_ClipGoto(1, 0, "fl_on_start");
            gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
            EntrySel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if ((gPad[0].gameRepeat & 8) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            if (gEntrySel->rows >= 2) {
                Flash_GotoLabel(&gEntrySel->flash[1], "fl_reel_down", 1);
                gEntrySel->sel->rowAnim = 1;
                EntrySel_ClipGoto(1, 0, "fl_off_start");
                ChrGrid_MoveUp(gEntrySel->grid, &ES_CUR.col, &ES_CUR.row, gEntrySel->rows);
                EntrySel_ClipGoto(1, 0, "fl_on_start");
                EntrySel_SwapRowTex();
                gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
                EntrySel_ChangeImage();
                Snd_PlaySe(2, 2);
            }
        } else if ((gPad[0].gameRepeat & 4) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            if (gEntrySel->rows >= 2) {
                Flash_GotoLabel(&gEntrySel->flash[1], "fl_reel_up", 1);
                gEntrySel->sel->rowAnim = 1;
                EntrySel_ClipGoto(1, 0, "fl_off_start");
                ChrGrid_MoveDown(gEntrySel->grid, &ES_CUR.col, &ES_CUR.row, gEntrySel->rows);
                EntrySel_ClipGoto(1, 0, "fl_on_start");
                EntrySel_SwapRowTex();
                gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
                EntrySel_ChangeImage();
                Snd_PlaySe(2, 1);
            }
        } else if (gPad[0].gamePressed & 0x200) {
            if (ES_CELL.id == ESEL_ID_RANDOM) {
                /* the random cell: no form, two costumes */
                Flash_GotoLabel(&gEntrySel->flash[2], "fl_custom_in", 1);
                EntrySel_ClipGoto(1, 0, "fl_ok");
                EntrySel_ClipGoto(2, 2, "fl_on_start");
                gEntrySel->customCount = (gEntrySel->flags & ESEL_NO_CUSTOM) ? 1 : 4;
                if (ES_CUR.custom >= gEntrySel->customCount) {
                    ES_CUR.custom = 0;
                }
                gEntrySel->colorCount = 2;
                if (ES_CUR.costume >= gEntrySel->colorCount) {
                    ES_CUR.costume = 0;
                }
                gEntrySel->sel->step = ESEL_STEP_CUSTOM;
                Snd_PlaySe(1, 1);
            } else if (ES_CELL.formCount != 0) {
                gEntrySel->sel->flags |= ESEL_SEL_FORM;
                if (ES_CUR.form >= ES_CELL.formCount) {
                    ES_CUR.form = 0;
                }
                Flash_GotoLabel(&gEntrySel->flash[1], "fl_form", 1);
                gEntrySel->sel->rowAnim = 1;
                EntrySel_ClipGoto(1, 0, "fl_off_start");
                EntrySel_ClipGoto(1, 1, "fl_on_start");
                EntrySel_SetRowTex();
                gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.form];
                EntrySel_ChangeImage();
                gEntrySel->sel->step = ESEL_STEP_FORM;
                Snd_PlaySe(2, 0x29);
            } else {
                Flash_GotoLabel(&gEntrySel->flash[2], "fl_custom_in", 1);
                EntrySel_ClipGoto(1, 0, "fl_ok");
                EntrySel_ClipGoto(2, 2, "fl_on_start");
                gEntrySel->customCount = (gEntrySel->flags & ESEL_NO_CUSTOM) ? 1 : 4;
                if (ES_CUR.custom >= gEntrySel->customCount) {
                    ES_CUR.custom = 0;
                }
                gEntrySel->colorCount = ChrTbl_WrapCostume(gEntrySel->sel->image, &ES_CUR.costume);
                gEntrySel->sel->step = ESEL_STEP_CUSTOM;
                Snd_PlaySe(1, 1);
            }
        } else if (gPad[0].gamePressed & 0x400) {
            if (gEntrySel->sel->cur != 0) {
                /* back to the previous entrant */
                EntrySel_ClipGoto(1, 0, "fl_off_start");
                EntrySel_ClipGoto(0, 7, "fl_stop");
                gEntrySel->sel->cur--;
                EntrySel_SetMemberTex();
                EntrySel_ClipGoto(1, 0, "fl_on_start");
                EntrySel_ClipGoto(0, 7, "fl_loop");
                EntrySel_SwapRowTex();
                gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
                EntrySel_ChangeImage();
            } else {
                ColorFade_StartOut(0, 0, 0, 0x14);
                *result = 0;
            }
            Snd_PlaySe(1, 2);
        }
        break;
    case ESEL_STEP_FORM:
        if ((gPad[0].gameRepeat & 1) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(1, 1, "fl_off_start");
            ChrGrid_PrevForm(gEntrySel->sel->rowChara, &ES_CUR.form);
            EntrySel_ClipGoto(1, 1, "fl_on_start");
            gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.form];
            EntrySel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if ((gPad[0].gameRepeat & 2) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(1, 1, "fl_off_start");
            ChrGrid_NextForm(gEntrySel->sel->rowChara, &ES_CUR.form);
            EntrySel_ClipGoto(1, 1, "fl_on_start");
            gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.form];
            EntrySel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&gEntrySel->flash[2], "fl_custom_in", 1);
            EntrySel_ClipGoto(1, 1, "fl_ok");
            EntrySel_ClipGoto(2, 2, "fl_on_start");
            gEntrySel->customCount = (gEntrySel->flags & ESEL_NO_CUSTOM) ? 1 : 4;
            if (ES_CUR.custom >= gEntrySel->customCount) {
                ES_CUR.custom = 0;
            }
            gEntrySel->colorCount = ChrTbl_WrapCostume(gEntrySel->sel->image, &ES_CUR.costume);
            gEntrySel->sel->step = ESEL_STEP_CUSTOM;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gEntrySel->sel->flags ^= ESEL_SEL_FORM;
            Flash_GotoLabel(&gEntrySel->flash[1], "fl_form", 1);
            gEntrySel->sel->rowAnim = 1;
            EntrySel_ClipGoto(1, 1, "fl_off_start");
            EntrySel_ClipGoto(1, 0, "fl_on_start");
            EntrySel_SwapRowTex();
            gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
            EntrySel_ChangeImage();
            gEntrySel->sel->step = ESEL_STEP_CHARA;
            Snd_PlaySe(2, 0x29);
        }
        break;
    case ESEL_STEP_CUSTOM:
        if ((gPad[0].gameRepeat & 8) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            if (gEntrySel->customCount >= 2) {
                EntrySel_ClipGoto(2, 2, "fl_off_start");
                ES_CUR.custom--;
                if (ES_CUR.custom < 0) {
                    ES_CUR.custom = gEntrySel->customCount - 1;
                }
                EntrySel_ClipGoto(2, 2, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if ((gPad[0].gameRepeat & 4) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            if (gEntrySel->customCount >= 2) {
                EntrySel_ClipGoto(2, 2, "fl_off_start");
                ES_CUR.custom++;
                if (ES_CUR.custom >= gEntrySel->customCount) {
                    ES_CUR.custom = 0;
                }
                EntrySel_ClipGoto(2, 2, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gamePressed & 2) {
            /* look at the items of the set */
            if (ES_CUR.custom > 0 && ES_CELL.id != ESEL_ID_RANDOM) {
                EntrySel_ClipGoto(2, 2, "fl_ok");
                ItemPanel_SetChara(0, gEntrySel->sel->image, ES_CUR.col + ES_CUR.row * ESEL_COLS, ES_CUR.custom - 1,
                                   0);
                ItemPanel_Show(0);
                Flash_GotoLabel(&gEntrySel->flash[3], "fl_guide_out", 1);
                MsgWin_Close();
                gEntrySel->sel->step = ESEL_STEP_PANEL;
            }
        } else if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&gEntrySel->flash[2], "fl_color_in", 1);
            EntrySel_ClipGoto(2, 2, "fl_ok");
            EntrySel_ClipGoto(2, 5, "fl_on_start");
            gEntrySel->sel->step = ESEL_STEP_COLOR;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gEntrySel->flash[2], "fl_custom_cansel", 1);
            EntrySel_ClipGoto(2, 2, "fl_off_start");
            if (gEntrySel->sel->flags & ESEL_SEL_FORM) {
                EntrySel_ClipGoto(1, 1, "fl_on_start");
                gEntrySel->sel->step = ESEL_STEP_FORM;
            } else {
                EntrySel_ClipGoto(1, 0, "fl_on_start");
                gEntrySel->sel->step = ESEL_STEP_CHARA;
            }
            Snd_PlaySe(1, 2);
        }
        break;
    case ESEL_STEP_PANEL: {
        s32 item = ItemPanel_Input(0, 0);

        if (item > 0) {
            ItemHelp_Open();
            gEntrySel->helpItem = item - 1;
            gEntrySel->sel->step = ESEL_STEP_HELP;
        } else if (item < 0) {
            ItemPanel_Hide(0);
            EntrySel_ClipGoto(2, 2, "fl_on_start");
            Flash_GotoLabel(&gEntrySel->flash[3], "fl_guide_in", 1);
            MsgWin_Open();
            gEntrySel->sel->step = ESEL_STEP_CUSTOM;
        }
        break;
    }
    case ESEL_STEP_HELP:
        if (gPad[0].gamePressed & 0x600) {
            ItemHelp_Close();
            gEntrySel->sel->step = ESEL_STEP_PANEL;
            Snd_PlaySe(1, 2);
        }
        break;
    case ESEL_STEP_COLOR:
        if ((gPad[0].gameRepeat & 1) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(2, 5, "fl_off_start");
            ES_CUR.costume--;
            if (ES_CUR.costume < 0) {
                ES_CUR.costume = gEntrySel->colorCount - 1;
            }
            EntrySel_ClipGoto(2, 5, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gameRepeat & 2) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(2, 5, "fl_off_start");
            ES_CUR.costume++;
            if (ES_CUR.costume >= gEntrySel->colorCount) {
                ES_CUR.costume = 0;
            }
            EntrySel_ClipGoto(2, 5, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            /* the entrant is decided: fix the character and take the item set along */
            s32 slot;

            gEntrySel->sel->rowAnim = 0;
            Snd_PlaySe(1, 1);
            Flash_GotoLabel(&gEntrySel->flash[2], "fl_color_ok", 1);
            EntrySel_ClipGoto(2, 5, "fl_ok");
            EntrySel_ClipGoto(0, 7, "fl_stop");
            EntrySel_ClipGoto(2, 2, "fl_off_start");
            if (gEntrySel->sel->flags & ESEL_SEL_FORM) {
                ES_CUR.chara = gEntrySel->sel->rowChara[ES_CUR.form];
                if (ES_CELL.id == ESEL_ID_REC) {
                    ES_CUR.items = TOUR_SAVE->rec[ES_CUR.recCol + ES_CUR.recRow * 7].items;
                } else if (ES_CUR.custom != 0) {
                    ES_CUR.items =
                        *(TourItemSet *)SAVE_CUSTOM_AT(gSaveData, ES_CUR.row * ESEL_COLS + ES_CUR.col).item[ES_CUR.custom - 1];
                } else {
                    memset(&ES_CUR.items, 0, 16);
                }
                EntrySel_ClipGoto(1, 1, "fl_off_start");
            } else if (gEntrySel->sel->flags & ESEL_SEL_REC) {
                /* never reached from this range: nothing sets the flag, and clip kind 6 does not exist */
                ES_CUR.chara = gEntrySel->sel->rowChara[ES_CUR.recCol];
                ES_CUR.items = TOUR_SAVE->rec[ES_CUR.recCol + ES_CUR.recRow * 7].items;
                EntrySel_ClipGoto(1, 6, "fl_off_start");
            } else {
                if (ES_CELL.id == ESEL_ID_RANDOM) {
                    ESelCell *cell;
                    s32 chara;
                    s32 id;

                    do {
                        slot = Rand_Range(gEntrySel->gridCount);
                        id = gEntrySel->grid[slot].id;
                    } while (id >= ESEL_ID_RANDOM);
                    chara = id;
                    if (gEntrySel->grid[slot].formCount != 0) {
                        cell = &gEntrySel->grid[slot];
                        chara = cell->form[Rand_Range(cell->formCount)];
                    }
                    ES_CUR.chara = chara;
                } else {
                    ES_CUR.chara = gEntrySel->sel->rowChara[ES_CUR.col];
                    slot = ES_CUR.col + ES_CUR.row * ESEL_COLS;
                }
                if (ES_CUR.custom != 0) {
                    ES_CUR.items = *(TourItemSet *)SAVE_CUSTOM_AT(gSaveData, slot).item[ES_CUR.custom - 1];
                } else {
                    memset(&ES_CUR.items, 0, 16);
                }
                EntrySel_ClipGoto(1, 0, "fl_off_start");
            }
            if (gEntrySel->sel->flags & ESEL_SEL_FORM) {
                gEntrySel->sel->flags ^= ESEL_SEL_FORM;
            }
            if (gEntrySel->sel->flags & ESEL_SEL_REC) {
                gEntrySel->sel->flags ^= ESEL_SEL_REC;
            }
            gEntrySel->sel->cur++;
            EntrySel_ClipGoto(0, 7, "fl_loop");
            EntrySel_SetMemberTex();
            if (gEntrySel->sel->cur == TOUR_PROG->t.entryNum) {
                Flash_GotoLabel(&gEntrySel->flash[1], "fl_out", 1);
                gEntrySel->endStep = ESEL_END_SPEAK;
            } else {
                EntrySel_ClipGoto(1, 0, "fl_on_start");
                EntrySel_SwapRowTex();
                gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
                EntrySel_ChangeImage();
                gEntrySel->sel->step = ESEL_STEP_CHARA;
            }
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gEntrySel->flash[2], "fl_color_cansel", 1);
            EntrySel_ClipGoto(2, 5, "fl_off_start");
            EntrySel_ClipGoto(2, 2, "fl_on_start");
            gEntrySel->sel->step = ESEL_STEP_CUSTOM;
            Snd_PlaySe(1, 2);
        }
        break;
    case 6:
    case 7:
        break;
    }
}

/* Once every entrant is chosen: the guide's closing line, then the leave timer. */
void EntrySel_UpdateEnd(void) {
    if (gEntrySel->endStep == ESEL_END_NONE) {
        return;
    }
    switch (gEntrySel->endStep) {
    case ESEL_END_SPEAK:
        switch (TOUR_PROG->t.tour) {
        case TOUR_WORLD:
            gEntrySel->talker = 0;
            gEntrySel->voiceLine = 0x23;
            break;
        case TOUR_BIG:
            gEntrySel->talker = 0;
            gEntrySel->voiceLine = Rand_Range(2) + 0x7B;
            break;
        case TOUR_CELL:
            gEntrySel->talker = 0;
            gEntrySel->voiceLine = Rand_Range(2) + 0x4E;
            break;
        case TOUR_OTHERWORLD:
            gEntrySel->talker = 0;
            gEntrySel->voiceLine = 0xA0;
            break;
        case TOUR_YAMCHA:
            gEntrySel->talker = Rand_Range(2);
            gEntrySel->voiceLine = (gEntrySel->talker ^ 1) + 0xCB;
            break;
        }
        gEntrySel->endStep++;
        Voice_PlayWithSubtitle(gEntrySel->subtitles, TOUR_VOICE_BASE, gEntrySel->voiceLine);
        break;
    case ESEL_END_WAIT:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gEntrySel->endStep++;
        } else if (gPad[0].gamePressed & 0x200) {
            gEntrySel->endStep++;
            Snd_PlaySe(1, 1);
        }
        break;
    case ESEL_END_LEAVE:
        gEntrySel->flags |= ESEL_DONE;
        gEntrySel->flags |= ESEL_LEAVING;
        gEntrySel->timer = 15;
        gEntrySel->endStep = ESEL_END_NONE;
        break;
    }
}

/*
 * The entrant select (mode 34). Returns 1 when the entrants were chosen (they are then in gProgress->entrant[]),
 * 0 when the player backed out.
 */
s32 EntrySel_Run(s32 section) {
    s32 result = 1;

    EntrySel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        EntrySel_UpdateImage();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            EntrySel_Update();
            EntrySel_UpdateEnd();
        }
        EntrySel_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gEntrySel->flags & ESEL_GREETED) && (gEntrySel->flash[0].flags & MFLASH_PAD)) {
                gEntrySel->flags |= ESEL_GREETED;
                switch (TOUR_PROG->t.tour) {
                case TOUR_WORLD:
                    gEntrySel->voiceLine = 0x22;
                    break;
                case TOUR_BIG:
                    gEntrySel->voiceLine = 0x7A;
                    break;
                case TOUR_CELL:
                    gEntrySel->voiceLine = 0x4D;
                    break;
                case TOUR_OTHERWORLD:
                    gEntrySel->voiceLine = 0x9F;
                    break;
                case TOUR_YAMCHA:
                    gEntrySel->voiceLine = 0xCA;
                    break;
                }
                gEntrySel->talker = 0;
                Voice_PlayWithSubtitle(gEntrySel->subtitles, TOUR_VOICE_BASE, gEntrySel->voiceLine);
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            /* the picture loader must be idle before the buffers are freed */
            if (gEntrySel->loadState != 4) {
                continue;
            }
            break;
        }
        if (gEntrySel->flags & ESEL_LEAVING) {
            if (--gEntrySel->timer == -1) {
                s32 i;

                ColorFade_StartOut(0, 0, 0, 0x14);
                /* hand the entrants to the tournament */
                for (i = 0; i < TOUR_PROG->t.entryNum; i++) {
                    TOUR_PROG->t.entrant[i].chara = gEntrySel->sel->entry[i].chara;
                    TOUR_PROG->t.entrant[i].costume = gEntrySel->sel->entry[i].costume;
                    TOUR_PROG->t.entrant[i].player = i;
                    TOUR_PROG->t.entrant[i].flags |= TOUR_ENT_PLAYER;
                    if (gEntrySel->sel->entry[i].custom != 0) {
                        TOUR_PROG->t.entrant[i].flags |= TOUR_ENT_ITEMS;
                        *(TourItemSet *)TOUR_PROG->t.entrant[i].item = gEntrySel->sel->entry[i].items;
                        if (gEntrySel->grid[gEntrySel->sel->entry[i].row * ESEL_COLS + gEntrySel->sel->entry[i].col]
                                .id == ESEL_ID_REC) {
                            TOUR_PROG->t.entrant[i].flags |= TOUR_ENT_REC;
                        }
                    }
                }
            }
        } else if (gEntrySel->endStep == ESEL_END_NONE) {
            EntrySel_Input(&result);
        }
    }
    EntrySel_Term();
    Dma_ResetBuffers();
    return result;
}

/*
 * Handler of progress modes 33..35, Dragon World Tour: 33 the tournament menu, 34 the entrant select, 35 the
 * tournament itself. Returns 1 to leave the overlay (a battle was set up), 0 to go on dispatching (mode 4, the
 * main menu).
 */
s32 Tour_Main(void) {
    s32 result = 1;
    s32 done = 0;
    /*
     * Always set, and the compiler folds the tests away; they are needed to match. The original had some test
     * inside the two "load the archive" blocks: a label inside the block ends the path of the first CSE pass, so
     * the gProgress address of the stores after it is not shared with the one of the switch (four `lw
     * %lo(gProgress)` against the hoisted `addiu`), which also leaves `result` without a register.
     */
    s32 load = 1;

    do {
        switch (gProgress->mode) {
        case 33:
            if (gMenuArc4 == NULL) {
                if (load) {
                    gMenuArc4 = File_LoadSync(gProgress->baseFile + 2, NULL, 0);
                }
            }
            Bgm_Play(0x10B19);
            if (TourMenu_Run(1)) {
                Adx_StopAll();
                gProgress->mode = 34;
            } else {
                Adx_StopAll();
                gProgress->mode = 4;
                result = 0;
                done = 1;
            }
            break;
        case 34:
            if (gMenuArc4 == NULL) {
                if (load) {
                    gMenuArc4 = File_LoadSync(gProgress->baseFile + 2, NULL, 0);
                }
            }
            Bgm_Play(0x10B1B);
            if (EntrySel_Run(2)) {
                Adx_StopAll();
                gProgress->mode = 35;
                if (gMenuArc4 != NULL) {
                    Heap_Free(gMenuArc4);
                    gMenuArc4 = NULL;
                }
            } else {
                Adx_StopAll();
                gProgress->mode = 33;
            }
            break;
        case 35:
            if (Bracket_Run(0)) {
                done = 1;
                Adx_StopAll();
            } else {
                /* back from the bracket without a battle to play: the mode's clock advances one hour */
                Adx_StopAll();
                gProgress->mode = 33;
                gSaveData->unkA0C++;
#ifdef PORT
                { extern s32 TagSession_Selected(void);extern void TagSession_Open(void);
                  if(TagSession_Selected()) {TagSession_Open();result=0;done=1;} }
#endif
                if (gSaveData->unkA0C >= TOUR_HOURS) {
                    gSaveData->unkA0C = 0;
                }
            }
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    if (gMenuArc4 != NULL) {
        Heap_Free(gMenuArc4);
        gMenuArc4 = NULL;
    }
    return result;
}
