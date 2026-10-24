#include "common.h"
#include "menu/menu_m.h"
#include "sys/pad.h"
#include "sys/save.h"

/*
 * UbTeamSel, 0x36E028..0x372148: the team select of the mode group 13..30 (run for mode 15 when
 * gProgress->teamSize >= 2). One side chooses up to five fighters from the character grid (form reel, item-set
 * plates, costume plates), optionally under a DP limit (gProgress->dpRule 1..3: the team total may not exceed
 * 10 / 15 / 20). It is the one-sided version of TeamSel (src/menu/menu_e_b.c) with two guides in place of the
 * stage choice. The object's .data is the work pointer 0x3B734C (second word after 0x3B7348 of SoloSel); its
 * .rodata runs from 0x3B7740 to 0x3B7ADC. Its last two functions (0x372148, 0x372260: the guide's closing line
 * and the frame loop) were written in the next chunk (menu_n.c) and are merged in here: 0x36E028..0x372560.
 */

UbTeamSel *gUbTeamSel = NULL; /* 0x3B734C */

/* The common resources of the main executable (include/sys/common.h). */
typedef struct UbCommonRes {
    /* 0x00 */ void *boot;
    /* 0x04 */ u32 *data[3]; /* files 2, 3, 4 */
} UbCommonRes;
extern UbCommonRes *gCommonRes;

#define UT_CUR (gUbTeamSel->sel->team.member[gUbTeamSel->sel->cur])
#define UT_CELL (gUbTeamSel->grid[UT_CUR.row * UB_COLS + UT_CUR.col])

/* DP rule: adds up the cost of the team's members; with skipCur the member being chosen is left out. */
void UbTeamSel_SumCost(s32 skipCur) {
    s32 i;

    if (gUbTeamSel->flags & UBSEL_DP) {
        gUbTeamSel->sel->cost = 0;
        for (i = 0; i < gUbTeamSel->sel->memberCount; i++) {
            if (i != gUbTeamSel->sel->cur || !skipCur) {
                gUbTeamSel->sel->cost += ChrTbl_GetCost(gUbTeamSel->sel->team.member[i].chara);
            }
        }
    }
}

/* Returns 0 if another member of the team is the same person as `chara` (a form of the same character). */
s32 UbTeamSel_IsCharaFree(s32 chara) {
    s32 ok = 1;
    s32 i;

    for (i = 0; i < gUbTeamSel->sel->memberCount; i++) {
        if (i != gUbTeamSel->sel->cur) {
            if (ChrTbl_IsRelated(chara, gUbTeamSel->sel->team.member[i].chara)) {
                ok = 0;
            }
        }
    }
    return ok;
}

/* DP rule: whether the team can still afford `chara`. Always 1 otherwise and for the special cells. */
s32 UbTeamSel_FitsDp(s32 chara) {
    UbTeamSel *w = gUbTeamSel;

    if (!(w->flags & UBSEL_DP)) {
        return 1;
    }
    if (chara == UB_ID_RANDOM || chara == UB_ID_REC) {
        return 1;
    }
    return gUbTeamSel->sel->cost + ChrTbl_GetCost(chara) <= w->dpMax;
}

/* Takes member `idx` out of the team: the members behind it move up and the last slot is emptied. */
void UbTeamSel_RemoveMember(s32 idx) {
    s32 i;

    memset(&gUbTeamSel->sel->team.member[idx], 0, sizeof(UbMember));
    gUbTeamSel->sel->team.member[idx].chara = -1;
    if (idx < gUbTeamSel->sel->memberCount) {
        for (i = idx; i < gUbTeamSel->sel->memberCount - 1; i++) {
            gUbTeamSel->sel->team.member[i] = gUbTeamSel->sel->team.member[i + 1];
        }
        memset(&gUbTeamSel->sel->team.member[gUbTeamSel->sel->memberCount - 1], 0, sizeof(UbMember));
        gUbTeamSel->sel->team.member[gUbTeamSel->sel->memberCount - 1].chara = -1;
    }
}

/* The seven chips of the reel show the cursor's row of the character grid. */
void UbTeamSel_SetChips(void) {
    s32 slot[UB_COLS] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < UB_COLS; i++) {
        s32 id = gUbTeamSel->grid[UT_CUR.row * UB_COLS + i].id;
        MTexRes *res;

        gUbTeamSel->sel->chip[1][i] = gUbTeamSel->sel->chip[0][i];
        gUbTeamSel->sel->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gUbTeamSel->chips, id + 1);
        gUbTeamSel->texC[18 + i] = gUbTeamSel->texC[slot[i]];
        gUbTeamSel->texC[slot[i]] = res->tex;
    }
}

/* The seven chips of the reel show the forms of the grid cell under the cursor (empty chips past the last). */
void UbTeamSel_SetFormChips(void) {
    s32 slot[UB_COLS] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < UB_COLS; i++) {
        s32 id;
        MTexRes *res;

        if (i < UT_CELL.formCount) {
            id = UT_CELL.form[i];
        } else {
            id = UB_ID_EMPTY;
        }
        gUbTeamSel->sel->chip[1][i] = gUbTeamSel->sel->chip[0][i];
        gUbTeamSel->sel->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gUbTeamSel->chips, id + 1);
        gUbTeamSel->texC[18 + i] = gUbTeamSel->texC[slot[i]];
        gUbTeamSel->texC[slot[i]] = res->tex;
    }
}

/*
 * Puts the chips of the members on the team movie and, under the DP rule, adds up their cost. Slots the team
 * may not use (>= teamMax) show the empty chip; free slots show nothing.
 */
void UbTeamSel_SetTeamTex(void) {
    s32 slot[UB_MEMBER_MAX] = { 1, 4, 5, 6, 7 };
    s32 i;

    if (gUbTeamSel->flags & UBSEL_DP) {
        gUbTeamSel->sel->cost = 0;
    }
    for (i = 0; i < UB_MEMBER_MAX; i++) {
        gUbTeamSel->texB[slot[i]] = NULL;
        if (i < gUbTeamSel->sel->memberCount) {
            s32 id = gUbTeamSel->sel->team.member[i].chara;
            MTexRes *res = (MTexRes *)MPACK_AT(gUbTeamSel->chips, id + 1);

            gUbTeamSel->texB[slot[i]] = res->tex;
            if (gUbTeamSel->flags & UBSEL_DP) {
                gUbTeamSel->sel->cost += ChrTbl_GetCost(id);
            }
        } else if (i >= gUbTeamSel->teamMax) {
            MTexRes *res = (MTexRes *)MPACK_AT(gUbTeamSel->chips, UB_ID_EMPTY + 1);

            gUbTeamSel->texB[slot[i]] = res->tex;
        }
    }
}

/* One step of the background loader of the portrait (file 0x2F9 + character). */
void UbTeamSel_UpdateFaceLoad(void) {
    MTexRes *res;

    switch (gUbTeamSel->loadState) {
    case UBSEL_LOAD_ABORT:
        gUbTeamSel->loadState = UBSEL_LOAD_RESTART;
        break;
    case UBSEL_LOAD_RESTART:
        if (gUbTeamSel->sel->flags & UBTEAM_SEL_FACE_CHANGE) {
            gUbTeamSel->sel->flags ^= UBTEAM_SEL_FACE_CHANGE;
        }
        gUbTeamSel->loadState = UBSEL_LOAD_REQUEST;
        break;
    case UBSEL_LOAD_REQUEST:
        File_CancelRequests();
        if (gUbTeamSel->sel->chara < 0) {
            gUbTeamSel->sel->flags |= UBTEAM_SEL_NO_FACE;
            gUbTeamSel->sel->flags |= UBTEAM_SEL_FACE_READY;
            gUbTeamSel->loadState = UBSEL_LOAD_IDLE;
        } else {
            File_Request(gUbTeamSel->sel->chara + UB_FACE_FILE, gUbTeamSel->faceFile, 0x16800);
            gUbTeamSel->loadState = UBSEL_LOAD_READ;
        }
        break;
    case UBSEL_LOAD_READ:
        if (File_UpdateRequests()) {
            gUbTeamSel->loadState = UBSEL_LOAD_UNPACK;
        }
        break;
    case UBSEL_LOAD_UNPACK:
        Sprite_Unpack(gUbTeamSel->faceFile, gUbTeamSel->faceRes, NULL);
        res = gUbTeamSel->faceRes;
        Res_RelocateOffsets(&res, res, res);
        gUbTeamSel->texA[17] = MTEX(res, 0);
        gUbTeamSel->sel->flags |= UBTEAM_SEL_FACE_READY;
        gUbTeamSel->loadState = UBSEL_LOAD_IDLE;
        break;
    case UBSEL_LOAD_IDLE:
        if (gUbTeamSel->sel->flags & UBTEAM_SEL_FACE_CHANGE) {
            gUbTeamSel->texA[17] = NULL;
            gUbTeamSel->loadState = UBSEL_LOAD_RESTART;
        }
        break;
    }
}

/* The shown character changed: hide the portrait, or abort a load in progress, and ask for the new one. */
void UbTeamSel_RequestFace(void) {
    if (gUbTeamSel->sel->flags & UBTEAM_SEL_FACE_READY) {
        gUbTeamSel->sel->flags ^= UBTEAM_SEL_FACE_READY;
    } else {
        gUbTeamSel->loadState = UBSEL_LOAD_ABORT;
    }
    if (gUbTeamSel->sel->flags & UBTEAM_SEL_NO_FACE) {
        gUbTeamSel->sel->flags ^= UBTEAM_SEL_NO_FACE;
    }
    gUbTeamSel->sel->flags |= UBTEAM_SEL_FACE_CHANGE;
}

/* Sends the clip of one chip or plate of movie `movie` to a label; `kind` says which clip and from which cursor. */
void UbTeamSel_ClipGoto(s32 movie, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gUbTeamSel->flash[movie];

    switch (kind) {
    case UBTEAM_CLIP_CHIP:
        sprintf(name, "mc_chara_chip_%03d", UT_CUR.col);
        break;
    case UBTEAM_CLIP_FORM_CHIP:
        sprintf(name, "mc_chara_chip_%03d", UT_CUR.form);
        break;
    case UBTEAM_CLIP_PLATE:
        sprintf(name, "mc_custom_plate_%d", UT_CUR.plate + 1);
        break;
    case UBTEAM_CLIP_COLOR:
        sprintf(name, "mc_color_plate_%d", UT_CUR.color + 1);
        break;
    case UBTEAM_CLIP_TEAM:
        switch (gUbTeamSel->sel->cur) {
        case UB_MEMBER_MAX:
            sprintf(name, "mc_menu_plate");
            break;
        default:
            sprintf(name, "mc_team_%d", gUbTeamSel->sel->cur);
            break;
        }
        break;
    case 3:
        return;
    case 4:
    default:
        /* original oddity: these kinds go on with `name` never written */
        break;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

#define UT_RES(n) \
    res = (MTexRes *)MPACK_AT(gUbTeamSel->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 3), builds its four movies and the grid. */
void UbTeamSel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    s32 j;

    gUbTeamSel = Heap_Alloc(sizeof(UbTeamSel), 0x20, 0, 2);
    memset(gUbTeamSel, 0, sizeof(UbTeamSel));
    gUbTeamSel->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gUbTeamSel->res = Sprite_Unpack(gUbTeamSel->pack, NULL, NULL);
    switch (UB_PROG->dpRule) {
    case 0:
        break;
    case 1:
        gUbTeamSel->flags |= UBSEL_DP;
        gUbTeamSel->dpLevel = UB_PROG->dpRule - 1;
        gUbTeamSel->dpMax = 10;
        break;
    case 2:
        gUbTeamSel->flags |= UBSEL_DP;
        gUbTeamSel->dpLevel = UB_PROG->dpRule - 1;
        gUbTeamSel->dpMax = 15;
        break;
    case 3:
        gUbTeamSel->flags |= UBSEL_DP;
        gUbTeamSel->dpLevel = UB_PROG->dpRule - 1;
        gUbTeamSel->dpMax = 20;
        break;
    }
    gUbTeamSel->teamMax = UB_PROG->teamSize;
    UT_RES(32);
    gUbTeamSel->bg = res;
    UT_RES(33);
    gUbTeamSel->texA[0] = MTEX(res, 0);
    gUbTeamSel->texA[1] = MTEX(res, 1);
    gUbTeamSel->texA[11] = MTEX(res, 2);
    gUbTeamSel->texA[12] = MTEX(res, 3);
    gUbTeamSel->texA[13] = MTEX(res, 4);
    UT_RES(1);
    gUbTeamSel->texA[8] = MTEX(res, 0);
    UT_RES(2);
    gUbTeamSel->texA[9] = MTEX(res, 0);
    gUbTeamSel->texA[18] = MTEX(res, 2);
    UT_RES(34);
    gUbTeamSel->texA[2] = MTEX(res, 0);
    gUbTeamSel->texA[3] = MTEX(res, 1);
    gUbTeamSel->texA[4] = MTEX(res, 3);
    gUbTeamSel->texA[14] = MTEX(res, 5);
    gUbTeamSel->texA[15] = MTEX(res, 6);
    gUbTeamSel->texA[16] = MTEX(res, 8);
    if (gUbTeamSel->flags & UBSEL_DP) {
        UT_RES(3);
        gUbTeamSel->texA[5] = MTEX(res, 0);
        UT_RES(4);
        gUbTeamSel->texA[6] = MTEX(res, 0);
        gUbTeamSel->texA[7] = MTEX(res, 1);
    }
    Flash_Create(&gUbTeamSel->flash[0], MPACK_AT(gUbTeamSel->res, 31), gUbTeamSel->texA);
    Flash_Play(&gUbTeamSel->flash[0], 1);
    if (gUbTeamSel->flags & UBSEL_DP) {
        UT_RES(5);
        gUbTeamSel->texC[9] = MTEX(res, 2);
        gUbTeamSel->texB[2] = MTEX(res, 2);
    }
    UT_RES(6);
    gUbTeamSel->texC[0] = MTEX(res, 0);
    gUbTeamSel->texC[2] = MTEX(res, 1);
    UT_RES(7);
    gUbTeamSel->texC[1] = MTEX(res, 1);
    gUbTeamSel->texC[4] = MTEX(res, 2);
    gUbTeamSel->texC[5] = MTEX(res, 4);
    gUbTeamSel->texC[6] = MTEX(res, 3);
    UT_RES(8);
    gUbTeamSel->texC[7] = MTEX(res, 0);
    UT_RES(9);
    gUbTeamSel->texC[3] = MTEX(res, 0);
    UT_RES(10);
    gUbTeamSel->texC[10] = MTEX(res, 0);
    gUbTeamSel->texC[17] = MTEX(res, 1);
    gUbTeamSel->texD[4] = MTEX(res, 1);
    gUbTeamSel->texB[3] = MTEX(res, 0);
    gUbTeamSel->texB[3] = MTEX(res, 0);
    Flash_Create(&gUbTeamSel->flash[2], MPACK_AT(gUbTeamSel->res, 11), gUbTeamSel->texC);
    Flash_Play(&gUbTeamSel->flash[2], 1);
    UT_RES(12);
    gUbTeamSel->texD[9] = MTEX(res, 0);
    gUbTeamSel->texD[0] = MTEX(res, 1);
    UT_RES(13);
    gUbTeamSel->texD[7] = MTEX(res, 0);
    gUbTeamSel->texD[3] = MTEX(res, 1);
    UT_RES(14);
    gUbTeamSel->texD[2] = MTEX(res, 0);
    gUbTeamSel->texD[5] = MTEX(res, 1);
    UT_RES(15);
    gUbTeamSel->texD[6] = MTEX(res, 0);
    gUbTeamSel->texD[8] = MTEX(res, 1);
    UT_RES(16);
    gUbTeamSel->texD[10] = MTEX(res, 0);
    gUbTeamSel->texD[1] = MTEX(res, 1);
    Flash_Create(&gUbTeamSel->flash[3], MPACK_AT(gUbTeamSel->res, 17), gUbTeamSel->texD);
    Flash_Play(&gUbTeamSel->flash[3], 1);
    UT_RES(18);
    gUbTeamSel->texB[0] = MTEX(res, 0);
    UT_RES(19);
    gUbTeamSel->texB[8] = MTEX(res, 0);
    gUbTeamSel->texB[11] = MTEX(res, 1);
    gUbTeamSel->texB[10] = MTEX(res, 2);
    UT_RES(20);
    gUbTeamSel->texB[9] = MTEX(res, 0);
    gUbTeamSel->texB[12] = MTEX(res, 1);
    Flash_Create(&gUbTeamSel->flash[1], MPACK_AT(gUbTeamSel->res, 21), gUbTeamSel->texB);
    Flash_Play(&gUbTeamSel->flash[1], 1);
    ItemPanel_Init(MPACK_AT(gUbTeamSel->res, 22), 0);
    gUbTeamSel->msgText = MPACK_AT(gUbTeamSel->res, 25);
    gUbTeamSel->subtitles = MPACK_AT(gUbTeamSel->res, 26);
    MsgWin_Init(MPACK_AT(gUbTeamSel->res, 23), gUbTeamSel->msgText, 1, 0);
    MsgWin_Open();
    ItemHelp_Init(MPACK_AT(gUbTeamSel->res, 24));
    gUbTeamSel->items = MPACK_AT(gCommonRes->data[2], 2);
    gUbTeamSel->nameText = MPACK_AT(gUbTeamSel->res, 28);
    gUbTeamSel->formText = MPACK_AT(gUbTeamSel->res, 29);
    gUbTeamSel->chips = (u32 *)MPACK_AT(gUbTeamSel->res, 27);
    for (i = 0; i < UB_CELL_MAX; i++) {
        res = (MTexRes *)MPACK_AT(gUbTeamSel->chips, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    gUbTeamSel->grid = (UbCell *)(MPACK_AT(gUbTeamSel->res, 30) + 0x10);
    gUbTeamSel->gridCount = gUbTeamSel->res[gUbTeamSel->res[30] >> 2];
    ChrGrid_Build(&gUbTeamSel->gridOutCount, gUbTeamSel->gridBuf, &gUbTeamSel->gridCount, gUbTeamSel->grid, NULL, NULL);
    gUbTeamSel->gridCount = gUbTeamSel->gridOutCount;
    gUbTeamSel->grid = gUbTeamSel->gridBuf;
    {
        s32 cols = UB_COLS;
        s32 cols2 = UB_COLS;

        gUbTeamSel->rows = gUbTeamSel->gridCount / cols;
        if (gUbTeamSel->gridCount % cols2) {
            gUbTeamSel->rows++;
        }
    }
    gUbTeamSel->faceFile = Heap_Alloc(0x16800, 0x40, 0, 2);
    gUbTeamSel->faceRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    gUbTeamSel->sel = &gUbTeamSel->state;
    gUbTeamSel->sel->team = UB_PROG->team;
    gUbTeamSel->loadState = UBSEL_LOAD_IDLE;
    /* a remembered cursor on a cell that cannot be chosen any more goes back to column 0, row 0 */
    for (j = 0; j < UB_MEMBER_MAX; j++) {
        if (!ChrGrid_IsSelectable(gUbTeamSel->grid,
                                  gUbTeamSel->sel->team.member[j].col + gUbTeamSel->sel->team.member[j].row * UB_COLS)) {
            memset(&gUbTeamSel->sel->team.member[j], 0, sizeof(UbMember));
            gUbTeamSel->sel->team.member[j].col = 0;
            UB_PROG->team.member[j] = gUbTeamSel->sel->team.member[j];
        }
    }
    UbTeamSel_SetChips();
    gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.col];
    for (j = 0; j < UB_MEMBER_MAX; j++) {
        gUbTeamSel->sel->team.member[j].chara = -1;
    }
    for (i = 0; i < 2; i++) {
        gUbTeamSel->blink[i] = Rand_Range(0x20);
    }
    gUbTeamSel->voiceLine = -1;
    File_LoadSync(gUbTeamSel->sel->chara + UB_FACE_FILE, gUbTeamSel->faceFile, 0x16800);
    Sprite_Unpack(gUbTeamSel->faceFile, gUbTeamSel->faceRes, NULL);
    res = gUbTeamSel->faceRes;
    Res_RelocateOffsets(&res, res, res);
    gUbTeamSel->texA[17] = MTEX(res, 0);
    gUbTeamSel->sel->flags |= UBTEAM_SEL_FACE_READY;
    TextBox_Init(&gUbTeamSel->box[0], gUbTeamSel->nameText, 1);
    TextBox_SetUnk80(&gUbTeamSel->box[0], 1);
    TextBox_Init(&gUbTeamSel->box[1], gUbTeamSel->formText, 3);
    TextBox_SetUnk80(&gUbTeamSel->box[1], 1);
}

/* Frees the screen: the windows, the movies, the portrait buffers and the work area. */
void UbTeamSel_Term(void) {
    s32 i;

    ItemHelp_Term();
    MsgWin_Term();
    ItemPanel_Term(0);
    for (i = 0; i < UBTEAM_FLASH_NUM; i++) {
        Flash_Destroy(&gUbTeamSel->flash[i]);
    }
    if (gUbTeamSel->faceRes != NULL) {
        Heap_Free(gUbTeamSel->faceRes);
        gUbTeamSel->faceRes = NULL;
    }
    if (gUbTeamSel->faceFile != NULL) {
        Heap_Free(gUbTeamSel->faceFile);
        gUbTeamSel->faceFile = NULL;
    }
    if (gUbTeamSel->res != NULL) {
        Heap_Free(gUbTeamSel->res);
        gUbTeamSel->res = NULL;
    }
    if (gUbTeamSel != NULL) {
        Heap_Free(gUbTeamSel);
        gUbTeamSel = NULL;
    }
}

/* Draws the background, sets up every clip of the four movies from the current state and draws them and the windows. */
void UbTeamSel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    char name2[64];
    s32 i;
    MFlash *flash;
    s32 chara;
    s32 cost;

    Sprite_DrawPicture(gUbTeamSel->bg, 0, 0, 0x80);
    flash = &gUbTeamSel->flash[0];
    for (i = 0; i < 2; i++) {
        /* the two guides ("17go", "18go"): only the one talking moves its mouth */
        sprintf(name, "mc_guide_%02dgo", i + 17);
        sprintf(name2, "mc_guide_%02dgo_eye", i + 17);
        Flash_FindLabel(flash, name, name2, &ref);
        FlashAnim_Blink(flash, &ref, &gUbTeamSel->blink[i], 0);
        sprintf(name2, "mc_guide_%02dgo_mouth", i + 17);
        Flash_FindLabel(flash, name, name2, &ref);
        if (gUbTeamSel->talker == i) {
            FlashAnim_Talk(flash, &ref, &gUbTeamSel->talk[i], 0);
        } else {
            FlashAnim_ShowNext2(flash, &ref, 0);
        }
    }
    if ((gUbTeamSel->sel->flags & (UBTEAM_SEL_NO_FACE | UBTEAM_SEL_FACE_READY)) == UBTEAM_SEL_FACE_READY) {
        gUbTeamSel->faceAlpha += 0.075f;
        if (gUbTeamSel->faceAlpha >= 1.0f) {
            gUbTeamSel->faceAlpha = 1.0f;
        }
    } else {
        gUbTeamSel->faceAlpha = 0.0f;
    }
    Flash_FindLabel(flash, NULL, "mc_single_chara_l", &ref);
    Flash_ClipSetAlpha(flash, &ref, gUbTeamSel->faceAlpha);
    Flash_FindLabel(flash, NULL, "mc_name_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gUbTeamSel->sel->chara, &gUbTeamSel->box[0]);
    Flash_FindLabel(flash, NULL, "mc_form_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gUbTeamSel->sel->chara, &gUbTeamSel->box[1]);
    if (gUbTeamSel->flags & UBSEL_DP) {
        uv.x0 = (gUbTeamSel->dpLevel % 2) * 0x40;
        uv.y0 = (gUbTeamSel->dpLevel / 2) * 0x20;
        uv.x1 = uv.x0 + 0x40;
        uv.y1 = uv.y0 + 0x20;
        Flash_FindLabel(flash, NULL, "mc_dp_max_l", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Num_Draw(flash, "mc_dp_total_l_%d", 0, 2, gUbTeamSel->sel->cost, 0x20, 0x40, 1);
    }
    flash = &gUbTeamSel->flash[3];
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    uv.y0 = 0;
    uv.y1 = 0x20;
    Flash_FindLabel(flash, NULL, "mc_yajirusi", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    for (i = 0; i < 4; i++) {
        /* item set plates: dimmed when they cannot be chosen */
        uv.x0 = 0;
        uv.x1 = 0x100;
        uv.y0 = i * 0x20;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_custom_plate_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetColor(flash, &ref, i < gUbTeamSel->plateCount ? 1.0f : 0.3f);
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
        uv.x1 = uv.x0 + 0x40;
        uv.y0 = (i / 2) * 0x20;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_color_plate_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (i < gUbTeamSel->colorCount) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            switch (gUbTeamSel->colorCount) {
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
    flash = &gUbTeamSel->flash[2];
    uv.x0 = 0;
    uv.x1 = 0x40;
    uv.y0 = 0;
    uv.y1 = 0x20;
    Flash_FindLabel(flash, NULL, "mc_plate_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gUbTeamSel->rows >= 2) {
        for (i = 0; i < 2; i++) {
            uv.x0 = i * 0x20;
            uv.y0 = 0x20;
            uv.x1 = uv.x0 + 0x20;
            uv.y1 = 0x40;
            Flash_FindLabel(flash, i ? "mc_yajirusi_down" : "mc_yajirusi_up",
                            i ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, NULL, i ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
            if (gUbTeamSel->sel->flags & UBTEAM_SEL_FORM) {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            }
        }
    } else {
        for (i = 0; i < 2; i++) {
            Flash_FindLabel(flash, NULL, i ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    Flash_FindLabel(flash, NULL, "mc_chara_mask", &ref);
    if (gUbTeamSel->sel->mask != 0) {
        Flash_ClipSetFlags(flash, &ref, 0x102, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 0x102, 0);
    }
    for (i = 0; i < 14; i++) {
        /* the chips of the reel (the new row and the old one): dimmed when the character cannot join */
        sprintf(name, "mc_chara_chip_%03d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetFlags(flash, &ref, 0x80, (u8)gUbTeamSel->sel->mask);
        {
            s32 cols = UB_COLS;

            chara = gUbTeamSel->sel->chip[i / cols][i % cols];
        }
        if (gUbTeamSel->flags & UBSEL_DP) {
            if (chara < UB_ID_RANDOM) {
                cost = ChrTbl_GetCost(chara);
            } else {
                cost = 0;
            }
            sprintf(name2, "mc_chara_%03d", i);
            Flash_FindLabel(flash, name, name2, &ref);
            if (gUbTeamSel->sel->cost + cost > gUbTeamSel->dpMax) {
                Flash_ClipSetColor(flash, &ref, 0.4f);
            } else if (chara < UB_ID_RANDOM) {
                Flash_ClipSetColor(flash, &ref, UbTeamSel_IsCharaFree(chara) ? 1.0f : 0.4f);
            } else {
                Flash_ClipSetColor(flash, &ref, 1.0f);
            }
            sprintf(name2, "mc_dp_num_%03d", i);
            Flash_FindLabel(flash, name, name2, &ref);
            if (chara < UB_ID_RANDOM) {
                uv.x0 = (cost % 4) * 0x20;
                uv.x1 = uv.x0 + 0x20;
                uv.y0 = (cost / 4) * 0x20;
                uv.y1 = uv.y0 + 0x20;
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        } else {
            sprintf(name2, "mc_chara_%03d", i);
            Flash_FindLabel(flash, name, name2, &ref);
            if (chara < UB_ID_RANDOM) {
                Flash_ClipSetColor(flash, &ref, UbTeamSel_IsCharaFree(chara) ? 1.0f : 0.4f);
            } else {
                Flash_ClipSetColor(flash, &ref, 1.0f);
            }
        }
    }
    flash = &gUbTeamSel->flash[1];
    for (i = 0; i < UB_MEMBER_MAX; i++) {
        /* the team plates and the cost of each member */
        sprintf(name, "mc_team_%d", i);
        uv.x0 = (i % 4) * 0x40;
        uv.x1 = uv.x0 + 0x40;
        uv.y0 = (i / 4) * 0x48;
        uv.y1 = uv.y0 + 0x48;
        sprintf(name2, "mc_team_plate_%d", i);
        Flash_FindLabel(flash, name, name2, &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        sprintf(name2, "mc_dp_num_%03d", i);
        Flash_FindLabel(flash, name, name2, &ref);
        if (i < gUbTeamSel->sel->memberCount) {
            cost = ChrTbl_GetCost(gUbTeamSel->sel->team.member[i].chara);
            uv.x0 = (cost % 4) * 0x20;
            uv.x1 = uv.x0 + 0x20;
            uv.y0 = (cost / 4) * 0x20;
            uv.y1 = uv.y0 + 0x20;
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_ClipSetUv(flash, &ref, &uv);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    Flash_Draw(&gUbTeamSel->flash[0]);
    Flash_Draw(&gUbTeamSel->flash[1]);
    Flash_Draw(&gUbTeamSel->flash[2]);
    Font_FlushAll();
    Flash_Draw(&gUbTeamSel->flash[3]);
    MsgWin_Draw(0, 0, gUbTeamSel->voiceLine);
    ItemPanel_Draw(0);
    ItemHelp_Draw(gUbTeamSel->help);
}

/* Advances the movies and the item panel, and shows the chips again once the reel movie says so. */
void UbTeamSel_Update(void) {
    s32 i;

    for (i = 0; i < UBTEAM_FLASH_NUM; i++) {
        Flash_Advance(&gUbTeamSel->flash[i]);
    }
    ItemPanel_Update(0);
    if (gUbTeamSel->sel->mask != 0 && (gUbTeamSel->flash[2].trig & 1)) {
        gUbTeamSel->sel->mask = 0;
    }
}

/*
 * The pad handler (pad 0), once per frame while no fade runs, the screen is not frozen and the guide's closing
 * line has not started. Does nothing until the first movie accepts input. `*result` is cleared when the screen
 * is left with cancel.
 */
void UbTeamSel_Input(s32 *result) {
    if (!(gUbTeamSel->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gUbTeamSel->flags & UBSEL_STARTED)) {
        UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_on_start");
        gUbTeamSel->flags |= UBSEL_STARTED;
    }
    switch (gUbTeamSel->sel->step) {
    case UBTEAM_STEP_CHARA:
        if (gPad[0].gameRepeat & 1) {
            UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_off_start");
            ChrGrid_MoveLeft(gUbTeamSel->grid, &UT_CUR.col, UT_CUR.row);
            UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_on_start");
            gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.col];
            UbTeamSel_RequestFace();
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gameRepeat & 2) {
            UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_off_start");
            ChrGrid_MoveRight(gUbTeamSel->grid, &UT_CUR.col, UT_CUR.row);
            UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_on_start");
            gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.col];
            UbTeamSel_RequestFace();
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gameRepeat & 8) {
            if (gUbTeamSel->rows >= 2) {
                Flash_GotoLabel(&gUbTeamSel->flash[2], "fl_reel_down", 1);
                gUbTeamSel->sel->mask = 1;
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_off_start");
                ChrGrid_MoveUp(gUbTeamSel->grid, &UT_CUR.col, &UT_CUR.row, gUbTeamSel->rows);
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_on_start");
                UbTeamSel_SetChips();
                gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.col];
                UbTeamSel_RequestFace();
                Snd_PlaySe(2, 2);
            }
        } else if (gPad[0].gameRepeat & 4) {
            if (gUbTeamSel->rows >= 2) {
                Flash_GotoLabel(&gUbTeamSel->flash[2], "fl_reel_up", 1);
                gUbTeamSel->sel->mask = 1;
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_off_start");
                ChrGrid_MoveDown(gUbTeamSel->grid, &UT_CUR.col, &UT_CUR.row, gUbTeamSel->rows);
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_on_start");
                UbTeamSel_SetChips();
                gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.col];
                UbTeamSel_RequestFace();
                Snd_PlaySe(2, 1);
            }
        } else if (gPad[0].gamePressed & 0x200) {
            if (UbTeamSel_FitsDp(gUbTeamSel->sel->chara)) {
                if (UT_CELL.formCount != 0) {
                    gUbTeamSel->sel->flags |= UBTEAM_SEL_FORM;
                    if (UT_CUR.form >= UT_CELL.formCount) {
                        UT_CUR.form = 0;
                    }
                    Flash_GotoLabel(&gUbTeamSel->flash[2], "fl_form", 1);
                    gUbTeamSel->sel->mask = 1;
                    UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_off_start");
                    UbTeamSel_ClipGoto(2, UBTEAM_CLIP_FORM_CHIP, "fl_on_start");
                    UbTeamSel_SetFormChips();
                    gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.form];
                    UbTeamSel_RequestFace();
                    gUbTeamSel->sel->step = UBTEAM_STEP_FORM;
                    Snd_PlaySe(2, 0x29);
                } else if (UbTeamSel_IsCharaFree(gUbTeamSel->sel->chara)) {
                    Flash_GotoLabel(&gUbTeamSel->flash[3], "fl_custom_in", 1);
                    UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_ok");
                    gUbTeamSel->plateCount = (gUbTeamSel->flags & UBSEL_NO_CUSTOM) ? 1 : 4;
                    if (UT_CUR.plate >= gUbTeamSel->plateCount) {
                        UT_CUR.plate = 0;
                    }
                    UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_on_start");
                    gUbTeamSel->colorCount = ChrTbl_WrapCostume(gUbTeamSel->sel->chara, &UT_CUR.color);
                    gUbTeamSel->sel->step = UBTEAM_STEP_PLATE;
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
            } else {
                Snd_PlaySe(1, 7);
            }
        } else if (gPad[0].gamePressed & 0x400) {
            gUbTeamSel->sel->mask = 0;
            if (gUbTeamSel->sel->memberCount != 0) {
                /* back to the team plates: the member being changed is restored */
                Flash_GotoLabel(&gUbTeamSel->flash[2], "fl_out", 1);
                Flash_GotoLabel(&gUbTeamSel->flash[1], "fl_in", 1);
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_off_start");
                UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_on_start");
                UT_CUR = gUbTeamSel->backup;
                gUbTeamSel->sel->chara = UT_CUR.chara;
                UbTeamSel_RequestFace();
                UbTeamSel_SumCost(0);
                gUbTeamSel->sel->step = UBTEAM_STEP_TEAM;
            } else {
                ColorFade_StartOut(0, 0, 0, 0x14);
                *result = 0;
            }
            Snd_PlaySe(1, 2);
        }
        break;
    case UBTEAM_STEP_FORM:
        if (gPad[0].gameRepeat & 1) {
            UbTeamSel_ClipGoto(2, UBTEAM_CLIP_FORM_CHIP, "fl_off_start");
            ChrGrid_PrevForm(gUbTeamSel->sel->chip[0], &UT_CUR.form);
            UbTeamSel_ClipGoto(2, UBTEAM_CLIP_FORM_CHIP, "fl_on_start");
            gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.form];
            UbTeamSel_RequestFace();
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gameRepeat & 2) {
            UbTeamSel_ClipGoto(2, UBTEAM_CLIP_FORM_CHIP, "fl_off_start");
            ChrGrid_NextForm(gUbTeamSel->sel->chip[0], &UT_CUR.form);
            UbTeamSel_ClipGoto(2, UBTEAM_CLIP_FORM_CHIP, "fl_on_start");
            gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.form];
            UbTeamSel_RequestFace();
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            if (UbTeamSel_FitsDp(gUbTeamSel->sel->chara)) {
                if (UbTeamSel_IsCharaFree(gUbTeamSel->sel->chara)) {
                    Flash_GotoLabel(&gUbTeamSel->flash[3], "fl_custom_in", 1);
                    UbTeamSel_ClipGoto(2, UBTEAM_CLIP_FORM_CHIP, "fl_ok");
                    gUbTeamSel->plateCount = (gUbTeamSel->flags & UBSEL_NO_CUSTOM) ? 1 : 4;
                    if (UT_CUR.plate >= gUbTeamSel->plateCount) {
                        UT_CUR.plate = 0;
                    }
                    UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_on_start");
                    gUbTeamSel->colorCount = ChrTbl_WrapCostume(gUbTeamSel->sel->chara, &UT_CUR.color);
                    gUbTeamSel->sel->step = UBTEAM_STEP_PLATE;
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
            } else {
                Snd_PlaySe(1, 7);
            }
        } else if (gPad[0].gamePressed & 0x400) {
            gUbTeamSel->sel->flags ^= UBTEAM_SEL_FORM;
            Flash_GotoLabel(&gUbTeamSel->flash[2], "fl_form", 1);
            gUbTeamSel->sel->mask = 1;
            UbTeamSel_ClipGoto(2, UBTEAM_CLIP_FORM_CHIP, "fl_off_start");
            UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_on_start");
            UbTeamSel_SetChips();
            gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.col];
            UbTeamSel_RequestFace();
            gUbTeamSel->sel->step = UBTEAM_STEP_CHARA;
            Snd_PlaySe(2, 0x29);
        }
        break;
    case UBTEAM_STEP_PLATE:
        if (gPad[0].gameRepeat & 8) {
            if (gUbTeamSel->plateCount >= 2) {
                UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_off_start");
                UT_CUR.plate--;
                if (UT_CUR.plate < 0) {
                    UT_CUR.plate = gUbTeamSel->plateCount - 1;
                }
                UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & 4) {
            if (gUbTeamSel->plateCount >= 2) {
                UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_off_start");
                UT_CUR.plate++;
                if (UT_CUR.plate >= gUbTeamSel->plateCount) {
                    UT_CUR.plate = 0;
                }
                UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gamePressed & 2) {
            /* look at the items of the set */
            if (UT_CUR.plate > 0) {
                UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_ok");
                ItemPanel_SetChara(0, gUbTeamSel->sel->chara, UT_CUR.col + UT_CUR.row * UB_COLS, UT_CUR.plate - 1, 0);
                ItemPanel_Show(0);
                gUbTeamSel->sel->flags |= UBTEAM_SEL_PANEL;
                gUbTeamSel->sel->step = UBTEAM_STEP_PANEL;
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&gUbTeamSel->flash[3], "fl_color_in", 1);
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_ok");
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_COLOR, "fl_on_start");
            gUbTeamSel->sel->step = UBTEAM_STEP_COLOR;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gUbTeamSel->flash[3], "fl_custom_cansel", 1);
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_off_start");
            if (gUbTeamSel->sel->flags & UBTEAM_SEL_FORM) {
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_FORM_CHIP, "fl_on_start");
                gUbTeamSel->sel->step = UBTEAM_STEP_FORM;
            } else {
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_on_start");
                gUbTeamSel->sel->step = UBTEAM_STEP_CHARA;
            }
            Snd_PlaySe(1, 2);
        }
        break;
    case UBTEAM_STEP_PANEL: {
        s32 item = ItemPanel_Input(0, 0);

        if (item > 0) {
            ItemHelp_Open();
            gUbTeamSel->help = item - 1;
            gUbTeamSel->sel->step = UBTEAM_STEP_HELP;
        } else if (item < 0) {
            ItemPanel_Hide(0);
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_on_start");
            gUbTeamSel->sel->step = UBTEAM_STEP_PLATE;
        }
        break;
    }
    case UBTEAM_STEP_HELP:
        if (gPad[0].gamePressed & 0x600) {
            ItemHelp_Close();
            gUbTeamSel->sel->step = UBTEAM_STEP_PANEL;
            Snd_PlaySe(1, 2);
        }
        break;
    case UBTEAM_STEP_COLOR:
        if (gPad[0].gameRepeat & 1) {
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_COLOR, "fl_off_start");
            UT_CUR.color--;
            if (UT_CUR.color < 0) {
                UT_CUR.color = gUbTeamSel->colorCount - 1;
            }
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_COLOR, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 2) {
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_COLOR, "fl_off_start");
            UT_CUR.color++;
            if (UT_CUR.color >= gUbTeamSel->colorCount) {
                UT_CUR.color = 0;
            }
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_COLOR, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            /* the member is complete: fix the character and its item set */
            gUbTeamSel->sel->mask = 0;
            Flash_GotoLabel(&gUbTeamSel->flash[3], "fl_color_ok", 1);
            Flash_GotoLabel(&gUbTeamSel->flash[2], "fl_out", 1);
            Flash_GotoLabel(&gUbTeamSel->flash[1], "fl_in", 1);
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_COLOR, "fl_ok");
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_off_start");
            if (gUbTeamSel->sel->flags & UBTEAM_SEL_FORM) {
                UT_CUR.chara = gUbTeamSel->sel->chip[0][UT_CUR.form];
                if (UT_CUR.plate != 0) {
                    UT_CUR.items = SAVE_CUSTOM_AT(UB_SAVE, UT_CUR.col + UT_CUR.row * UB_COLS).set[UT_CUR.plate - 1];
                } else {
                    memset(&UT_CUR.items, 0, sizeof(UbItemSet));
                }
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_FORM_CHIP, "fl_off_start");
            } else {
                UT_CUR.chara = gUbTeamSel->sel->chip[0][UT_CUR.col];
                if (UT_CUR.plate != 0) {
                    UT_CUR.items = SAVE_CUSTOM_AT(UB_SAVE, UT_CUR.col + UT_CUR.row * UB_COLS).set[UT_CUR.plate - 1];
                } else {
                    memset(&UT_CUR.items, 0, sizeof(UbItemSet));
                }
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_off_start");
            }
            UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_off_start");
            if (gUbTeamSel->sel->cur == gUbTeamSel->sel->memberCount) {
                /* it was a new member: the cursor goes to the next free slot, or to the menu plate when full */
                gUbTeamSel->sel->memberCount = gUbTeamSel->sel->cur + 1;
                gUbTeamSel->sel->cur = gUbTeamSel->sel->memberCount;
                if (gUbTeamSel->sel->cur >= gUbTeamSel->teamMax) {
                    gUbTeamSel->sel->cur = UB_CUR_MENU;
                }
            }
            UbTeamSel_SetTeamTex();
            UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_on_start");
            if (gUbTeamSel->sel->cur == UB_CUR_MENU) {
                gUbTeamSel->sel->chara = gUbTeamSel->sel->team.member[0].chara;
                UbTeamSel_RequestFace();
            }
            if (gUbTeamSel->sel->flags & UBTEAM_SEL_FORM) {
                gUbTeamSel->sel->flags ^= UBTEAM_SEL_FORM;
            }
            if (gUbTeamSel->sel->flags & UBTEAM_SEL_REC) {
                gUbTeamSel->sel->flags ^= UBTEAM_SEL_REC;
            }
            gUbTeamSel->sel->step = UBTEAM_STEP_TEAM;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gUbTeamSel->flash[3], "fl_color_cansel", 1);
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_COLOR, "fl_off_start");
            UbTeamSel_ClipGoto(3, UBTEAM_CLIP_PLATE, "fl_on_start");
            gUbTeamSel->sel->step = UBTEAM_STEP_PLATE;
            Snd_PlaySe(1, 2);
        }
        break;
    case UBTEAM_STEP_TEAM:
        if (gPad[0].gameRepeat & 1) {
            UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_off_start");
            gUbTeamSel->sel->cur--;
            if (gUbTeamSel->sel->cur < 0) {
                gUbTeamSel->sel->cur = UB_CUR_MENU;
            } else if (gUbTeamSel->sel->cur > gUbTeamSel->sel->memberCount) {
                gUbTeamSel->sel->cur = gUbTeamSel->sel->memberCount;
                if (gUbTeamSel->sel->cur >= gUbTeamSel->teamMax) {
                    gUbTeamSel->sel->cur = gUbTeamSel->teamMax - 1;
                }
            }
            UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_on_start");
            if (gUbTeamSel->sel->cur == UB_CUR_MENU) {
                gUbTeamSel->sel->chara = gUbTeamSel->sel->team.member[0].chara;
            } else {
                gUbTeamSel->sel->chara = UT_CUR.chara;
            }
            UbTeamSel_RequestFace();
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 2) {
            UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_off_start");
            gUbTeamSel->sel->cur++;
            if (gUbTeamSel->sel->cur > UB_CUR_MENU) {
                gUbTeamSel->sel->cur = 0;
            } else if (gUbTeamSel->sel->cur > gUbTeamSel->sel->memberCount) {
                gUbTeamSel->sel->cur = UB_CUR_MENU;
            } else if (gUbTeamSel->sel->cur >= gUbTeamSel->teamMax) {
                gUbTeamSel->sel->cur = UB_CUR_MENU;
            }
            UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_on_start");
            if (gUbTeamSel->sel->cur == UB_CUR_MENU) {
                gUbTeamSel->sel->chara = gUbTeamSel->sel->team.member[0].chara;
            } else {
                gUbTeamSel->sel->chara = UT_CUR.chara;
            }
            UbTeamSel_RequestFace();
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            if (gUbTeamSel->sel->cur != UB_CUR_MENU) {
                /* choose (or change) the member under the cursor */
                UbTeamSel_SumCost(1);
                Flash_GotoLabel(&gUbTeamSel->flash[1], "fl_out", 1);
                Flash_GotoLabel(&gUbTeamSel->flash[2], "fl_in", 1);
                UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_ok");
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_on_start");
                UbTeamSel_SetChips();
                gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.col];
                UbTeamSel_RequestFace();
                gUbTeamSel->backup = UT_CUR;
                gUbTeamSel->sel->step = UBTEAM_STEP_CHARA;
                Snd_PlaySe(1, 1);
            } else if (gUbTeamSel->sel->memberCount != 0) {
                /* the team is complete: the guide's closing line, then the screen leaves */
                Flash_GotoLabel(&gUbTeamSel->flash[1], "fl_out", 1);
                UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_ok");
                gUbTeamSel->endStep = UBSEL_END_SPEAK;
                Snd_PlaySe(1, 1);
            } else {
                Snd_PlaySe(1, 7);
            }
        } else if (gPad[0].gamePressed & 0x400) {
            /* remove a member */
            if (gUbTeamSel->sel->memberCount == 1) {
                Flash_GotoLabel(&gUbTeamSel->flash[1], "fl_out", 1);
                Flash_GotoLabel(&gUbTeamSel->flash[2], "fl_in", 1);
                UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_ok");
                gUbTeamSel->sel->cur = 0;
                UbTeamSel_ClipGoto(2, UBTEAM_CLIP_CHIP, "fl_on_start");
                UbTeamSel_SetChips();
                gUbTeamSel->sel->chara = gUbTeamSel->sel->chip[0][UT_CUR.col];
                UbTeamSel_RequestFace();
                gUbTeamSel->sel->step = UBTEAM_STEP_CHARA;
            } else if (gUbTeamSel->sel->cur == UB_CUR_MENU) {
                UbTeamSel_RemoveMember(gUbTeamSel->sel->memberCount - 1);
            } else if (gUbTeamSel->sel->cur == gUbTeamSel->sel->memberCount) {
                UbTeamSel_RemoveMember(gUbTeamSel->sel->cur - 1);
                UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_off_start");
                gUbTeamSel->sel->cur--;
                UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_on_start");
                gUbTeamSel->sel->chara = UT_CUR.chara;
                UbTeamSel_RequestFace();
            } else {
                UbTeamSel_RemoveMember(gUbTeamSel->sel->cur);
                gUbTeamSel->sel->chara = UT_CUR.chara;
                UbTeamSel_RequestFace();
            }
            gUbTeamSel->sel->memberCount--;
            UbTeamSel_SetTeamTex();
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

/* Once the team is chosen: the guide's closing line, then the leave timer. */
void UbTeamSel_UpdateEnd(void) {
    s32 step = gUbTeamSel->endStep;

    if (step == UBSEL_END_NONE) {
        return;
    }
    switch (step) {
    case UBSEL_END_SPEAK:
        gUbTeamSel->talker = 0;
        gUbTeamSel->voiceLine = 0x37;
        Voice_PlayWithSubtitle(gUbTeamSel->subtitles, UB_VOICE_BASE, gUbTeamSel->voiceLine);
        gUbTeamSel->endStep++;
        break;
    case UBSEL_END_WAIT:
        if (Voice_GetStat(0) == UB_VOICE_IDLE) {
            gUbTeamSel->endStep++;
        } else if (gPad[0].gamePressed & 0x200) {
            gUbTeamSel->endStep++;
            Snd_PlaySe(1, 1);
        }
        break;
    case UBSEL_END_LEAVE:
        gUbTeamSel->flags |= UBSEL_DONE;
        gUbTeamSel->flags |= UBSEL_LEAVING;
        gUbTeamSel->timer = 15;
        gUbTeamSel->endStep = UBSEL_END_NONE;
        break;
    }
}

/*
 * The team select (mode 15 when gProgress->teamSize >= 2). Returns 1 when a team was chosen (the members are then
 * in gProgress->team and their number in gProgress->teamSize), 0 when the player backed out.
 */
s32 UbTeamSel_Run(s32 section) {
    s32 result = 1;

    UbTeamSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        UbTeamSel_UpdateFaceLoad();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            UbTeamSel_Update();
            UbTeamSel_UpdateEnd();
        }
        UbTeamSel_Draw();
        Font_FlushAll();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gUbTeamSel->flags & UBSEL_GREETED) && (gUbTeamSel->flash[0].flags & MFLASH_PAD)) {
                gUbTeamSel->flags |= UBSEL_GREETED;
                gUbTeamSel->talker = 0;
                gUbTeamSel->voiceLine = 0x36;
                Voice_PlayWithSubtitle(gUbTeamSel->subtitles, UB_VOICE_BASE, gUbTeamSel->voiceLine);
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gUbTeamSel->loadState != UBSEL_LOAD_IDLE) {
                continue;
            }
            break;
        }
        if (gUbTeamSel->flags & UBSEL_LEAVING) {
            if (--gUbTeamSel->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                /* hand the choice to the mode */
                UB_PROG->team = gUbTeamSel->sel->team;
            }
        } else if (gUbTeamSel->endStep == UBSEL_END_NONE) {
            UbTeamSel_Input(&result);
        }
    }
    if (result) {
        UB_PROG->teamSize = gUbTeamSel->sel->memberCount;
    }
    UbTeamSel_Term();
    Dma_ResetBuffers();
    return result;
}
