#include "common.h"
#include "menu/menu_e.h"
#include "menu/menu_f.h"
#include "sys/pad.h"
#include "sys/save.h"
#ifdef PORT
/* include/battle/view_a.h holds these; repeated here (pulling it in clashes with the menu headers). The PC build
   moves the locked / empty markers up so added stages keep their own ids. */
#define STGGRID_ID_LOCKED 0x3E
#define STGGRID_ID_EMPTY 0x3F
#endif

/*
 * TeamSel: the team character / stage / music select of the versus modes, 0x348D78..0x351C38. One object,
 * written as two chunks (menu_e_b.c 0x348D78..0x34D368: helpers, loaders, Init, Term, Draw; menu_f.c
 * 0x34D368..0x351C38: Update, Input, Run) and merged here. Two sides choose up to five fighters each from the
 * character grid (form reel, custom-character list, item-set plates, costume plates), optionally under a DP
 * limit (battleType 2: each character has a cost and the team total may not exceed 10 / 15 / 20), then the
 * stage and the music; TeamSel_Run, the frame loop, ends by writing the battle setup. It is the team version
 * of CharSel (0x342190..0x348D78) and shares its movies, grids and loaders. Called by Duel_Main (0x352CB8,
 * progress modes 38..41).
 */

TeamSel *gTeamSel = NULL;

extern s32 ChrTbl_GetCost(s32 chara);
extern s32 ChrTbl_IsRelated(s32 a, s32 b);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern s32 ChrGrid_IsSelectable(TsCell *cells, s32 index);
extern s32 StgGrid_IsSelectable(s32 *ids, s32 index);
extern void ChrGrid_Build(s32 *outCount, TsCell *out, s32 *inCount, TsCell *in, s32 *customCount, TsCell *custom);
extern void StgGrid_ApplyUnlocks(s32 *count, s32 *ids);
extern void BgmList_ApplyUnlocks(s32 *count, s32 *ids);

/* First word of a section of a pack (the count of a list section, whose entries start at +0x10). */
#define TS_PACK_WORD(pack, n) (*(s32 *)(((((u32 *)(pack))[n] >> 2) << 2) + (u32)(pack)))

/* The common resources of the main executable (include/sys/common.h). */
typedef struct TsCommonRes {
    /* 0x00 */ void *boot;
    /* 0x04 */ u32 *data[3]; /* files 2, 3, 4 */
} TsCommonRes;
extern TsCommonRes *gCommonRes;

/* Neighbours in the overlay: the item panel of a side (0x351C38.., names from config/symbols/menu_g.txt) and the
   item help window (0x399240.., not named yet). */
extern void ItemPanel_Init(void *data, s32 side);
extern void ItemPanel_Term(s32 side);
extern void ItemPanel_Draw(s32 side);
extern void ItemHelp_Init(void *data);
extern void ItemHelp_Term(void);
extern void ItemHelp_Draw(s32 arg);

/* DP battle: adds up the cost of a side's members; with skipCur the member being chosen is left out. */
void TeamSel_SumCost(s32 side, s32 skipCur) {
    s32 i;

    if (gTeamSel->battleType == 2) {
        gTeamSel->side[side]->cost = 0;
        for (i = 0; i < gTeamSel->side[side]->memberCount; i++) {
            if (i != gTeamSel->side[side]->cur || !skipCur) {
                gTeamSel->side[side]->cost += ChrTbl_GetCost(gTeamSel->side[side]->member[i].chara);
            }
        }
    }
}

/* Returns 0 if another member of the side's team is the same person as `chara` (a form of the same character). */
s32 TeamSel_IsCharaFree(s32 side, s32 chara) {
    s32 ok = 1;
    s32 i;

    for (i = 0; i < gTeamSel->side[side]->memberCount; i++) {
        if (i != gTeamSel->side[side]->cur) {
            if (ChrTbl_IsRelated(chara, gTeamSel->side[side]->member[i].chara)) {
                ok = 0;
            }
        }
    }
    return ok;
}

/* DP battle: whether the side's team can still afford `chara`. Always 1 otherwise and for the special cells. */
s32 TeamSel_FitsDp(s32 side, s32 chara) {
    TeamSel *w = gTeamSel;

    if (w->battleType != 2) {
        return 1;
    }
    if (chara == TS_RANDOM || chara == TS_CUSTOM) {
        return 1;
    }
    return gTeamSel->side[side]->cost + ChrTbl_GetCost(chara) <= w->dpMax;
}

/* Takes member `idx` out of a side's team: the members behind it move up and the last slot is emptied. */
void TeamSel_RemoveMember(s32 side, s32 idx) {
    s32 i;

    memset(&gTeamSel->side[side]->member[idx], 0, sizeof(TsMember));
    gTeamSel->side[side]->member[idx].chara = -1;
    if (idx < gTeamSel->side[side]->memberCount) {
        for (i = idx; i < gTeamSel->side[side]->memberCount - 1; i++) {
            gTeamSel->side[side]->member[i] = gTeamSel->side[side]->member[i + 1];
        }
        memset(&gTeamSel->side[side]->member[gTeamSel->side[side]->memberCount - 1], 0, sizeof(TsMember));
        gTeamSel->side[side]->member[gTeamSel->side[side]->memberCount - 1].chara = -1;
    }
}

/* The six stage chips show the cursor's row of the stage grid. */
void TeamSel_SetStageChips(void) {
    s32 slot[6] = { 8, 11, 12, 13, 14, 15 };
    s32 i;

    for (i = 0; i < 6; i++) {
        s32 id = gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + i];
        MTexRes *res;

        gTeamSel->stage->chip[1][i] = gTeamSel->stage->chip[0][i];
        gTeamSel->stage->chip[0][i] = id;
#ifdef PORT
        /* PC build: the markers moved up, so they keep the locked / empty icons (37, 38). */
        {
            s32 icon = id == STGGRID_ID_LOCKED ? 37 : (id == STGGRID_ID_EMPTY ? 38 : id + 1);
            res = (MTexRes *)MPACK_AT(gTeamSel->stagePack, icon);
        }
#else
        res = (MTexRes *)MPACK_AT(gTeamSel->stagePack, id + 1);
#endif
        gTeamSel->tex[41 + i] = gTeamSel->tex[slot[i]];
        gTeamSel->tex[slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the cursor's row of the character grid. */
void TeamSel_SetChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id = gTeamSel->cells[side][gTeamSel->side[side]->member[gTeamSel->side[side]->cur].row * 7 + i].id;
        MTexRes *res;

        gTeamSel->side[side]->chip[1][i] = gTeamSel->side[side]->chip[0][i];
        gTeamSel->side[side]->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, id + 1);
        gTeamSel->sideTex[side][18 + i] = gTeamSel->sideTex[side][slot[i]];
        gTeamSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the forms of the grid cell under the cursor. */
void TeamSel_SetGridFormChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id;
        MTexRes *res;

        if (i < gTeamSel->cells[side][gTeamSel->side[side]->member[gTeamSel->side[side]->cur].col +
                                      gTeamSel->side[side]->member[gTeamSel->side[side]->cur].row * 7].formCount) {
            id = gTeamSel->cells[side][gTeamSel->side[side]->member[gTeamSel->side[side]->cur].col +
                                       gTeamSel->side[side]->member[gTeamSel->side[side]->cur].row * 7].form[i];
        } else {
            id = TS_EMPTY;
        }
        gTeamSel->side[side]->chip[1][i] = gTeamSel->side[side]->chip[0][i];
        gTeamSel->side[side]->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, id + 1);
        gTeamSel->sideTex[side][18 + i] = gTeamSel->sideTex[side][slot[i]];
        gTeamSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show a row of the custom-character list. */
void TeamSel_SetCustomChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id = gTeamSel->custom[side][i + gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customRow * 7].id;
        MTexRes *res;

        gTeamSel->side[side]->chip[1][i] = gTeamSel->side[side]->chip[0][i];
        gTeamSel->side[side]->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, id + 1);
        gTeamSel->sideTex[side][18 + i] = gTeamSel->sideTex[side][slot[i]];
        gTeamSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the forms of the custom cell under the cursor. */
void TeamSel_SetFormChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id;
        MTexRes *res;

        if (i < gTeamSel->custom[side][gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customCol +
                                       gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customRow * 7].formCount) {
            id = gTeamSel->custom[side][gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customCol +
                                        gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customRow * 7].form[i];
        } else {
            id = TS_EMPTY;
        }
        gTeamSel->side[side]->chip[1][i] = gTeamSel->side[side]->chip[0][i];
        gTeamSel->side[side]->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, id + 1);
        gTeamSel->sideTex[side][18 + i] = gTeamSel->sideTex[side][slot[i]];
        gTeamSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* Puts the chips of a side's members on its team movie and, in a DP battle, adds up their cost. */
void TeamSel_SetTeamTex(s32 side) {
    s32 slot[5] = { 1, 4, 5, 6, 7 };
    s32 i;

    if (gTeamSel->battleType == 2) {
        gTeamSel->side[side]->cost = 0;
    }
    for (i = 0; i < 5; i++) {
        gTeamSel->teamTex[side][slot[i]] = NULL;
        if (i < gTeamSel->side[side]->memberCount) {
            s32 id = gTeamSel->side[side]->member[i].chara;
            MTexRes *res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, id + 1);

            gTeamSel->teamTex[side][slot[i]] = res->tex;
            if (gTeamSel->battleType == 2) {
                gTeamSel->side[side]->cost += ChrTbl_GetCost(id);
            }
        }
    }
}

/* One step of the background loader of the two portraits. */
void TeamSel_UpdateFaceLoad(void) {
    MTexRes *res;

    switch (gTeamSel->faceState) {
    case TEAMSEL_LOAD_ABORT:
        if (gTeamSel->side[gTeamSel->faceSide ^ 1]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
            gTeamSel->faceSide ^= 1;
        }
        gTeamSel->faceState = TEAMSEL_LOAD_RESTART;
        break;
    case TEAMSEL_LOAD_RESTART:
        if (gTeamSel->side[gTeamSel->faceSide]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
            gTeamSel->side[gTeamSel->faceSide]->flags ^= TEAMSEL_SIDE_FACE_CHANGE;
        }
        gTeamSel->faceState = TEAMSEL_LOAD_REQUEST;
        break;
    case TEAMSEL_LOAD_REQUEST:
        File_CancelRequests();
        if (gTeamSel->side[gTeamSel->faceSide]->chara < 0) {
            gTeamSel->side[gTeamSel->faceSide]->flags |= TEAMSEL_SIDE_NO_FACE;
            gTeamSel->side[gTeamSel->faceSide]->flags |= TEAMSEL_SIDE_FACE_READY;
            if (gTeamSel->side[gTeamSel->faceSide ^ 1]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
                gTeamSel->faceSide ^= 1;
                gTeamSel->faceState = TEAMSEL_LOAD_RESTART;
            } else {
                gTeamSel->faceState = TEAMSEL_LOAD_IDLE;
            }
        } else {
            File_Request(gTeamSel->side[gTeamSel->faceSide]->chara + TS_FACE_FILE,
                         gTeamSel->faceFile[gTeamSel->faceSide], 0x16800);
            gTeamSel->faceState = TEAMSEL_LOAD_READ;
        }
        break;
    case TEAMSEL_LOAD_READ:
        if (File_UpdateRequests()) {
            gTeamSel->faceState = TEAMSEL_LOAD_UNPACK;
        }
        break;
    case TEAMSEL_LOAD_UNPACK:
        Sprite_Unpack(gTeamSel->faceFile[gTeamSel->faceSide], gTeamSel->faceRes[gTeamSel->faceSide], NULL);
        res = gTeamSel->faceRes[gTeamSel->faceSide];
        Res_RelocateOffsets(&res, res, res);
        if (gTeamSel->faceSide == 0) {
            gTeamSel->tex[23] = MTEX(res, 0);
        } else {
            gTeamSel->tex[22] = MTEX(res, 0);
        }
        gTeamSel->side[gTeamSel->faceSide]->flags |= TEAMSEL_SIDE_FACE_READY;
        if (gTeamSel->side[gTeamSel->faceSide ^ 1]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
            gTeamSel->faceSide ^= 1;
            gTeamSel->faceState = TEAMSEL_LOAD_RESTART;
        } else {
            gTeamSel->faceState = TEAMSEL_LOAD_IDLE;
        }
        break;
    case TEAMSEL_LOAD_IDLE:
        if (gTeamSel->stageState != TEAMSEL_LOAD_IDLE) {
            break;
        }
        if (gTeamSel->side[0]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
            gTeamSel->tex[23] = NULL;
            gTeamSel->faceSide = 0;
            gTeamSel->faceState = TEAMSEL_LOAD_RESTART;
        } else if (gTeamSel->side[1]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
            gTeamSel->tex[22] = NULL;
            gTeamSel->faceSide = 1;
            gTeamSel->faceState = TEAMSEL_LOAD_RESTART;
        }
        break;
    }
}

/* A side's character changed: hide its portrait, or abort its load in progress, and ask for the new one. */
void TeamSel_RequestFace(s32 side) {
    if (gTeamSel->side[side]->flags & TEAMSEL_SIDE_FACE_READY) {
        gTeamSel->side[side]->flags ^= TEAMSEL_SIDE_FACE_READY;
    } else if (gTeamSel->faceSide == side) {
        gTeamSel->faceState = TEAMSEL_LOAD_ABORT;
    }
    if (gTeamSel->side[side]->flags & TEAMSEL_SIDE_NO_FACE) {
        gTeamSel->side[side]->flags ^= TEAMSEL_SIDE_NO_FACE;
    }
    gTeamSel->side[side]->flags |= TEAMSEL_SIDE_FACE_CHANGE;
}

/* One step of the background loader of the stage picture. */
void TeamSel_UpdateStageLoad(void) {
    MTexRes *res;

    switch (gTeamSel->stageState) {
    case TEAMSEL_LOAD_ABORT:
        gTeamSel->stageState = TEAMSEL_LOAD_RESTART;
        break;
    case TEAMSEL_LOAD_RESTART:
        if (gTeamSel->flags & TEAMSEL_STAGE_CHANGE) {
            gTeamSel->flags ^= TEAMSEL_STAGE_CHANGE;
        }
        gTeamSel->stageState = TEAMSEL_LOAD_REQUEST;
        break;
    case TEAMSEL_LOAD_REQUEST:
        File_CancelRequests();
        File_Request(gTeamSel->stage->stage + TS_STAGE_FILE, gTeamSel->stageFile, 0x3B800);
        gTeamSel->stageState = TEAMSEL_LOAD_READ;
        break;
    case TEAMSEL_LOAD_READ:
        if (File_UpdateRequests()) {
            gTeamSel->stageState = TEAMSEL_LOAD_UNPACK;
        }
        break;
    case TEAMSEL_LOAD_UNPACK:
        Sprite_Unpack(gTeamSel->stageFile, gTeamSel->stageRes[gTeamSel->stageBuf], NULL);
        res = gTeamSel->stageRes[gTeamSel->stageBuf];
        Res_RelocateOffsets(&res, res, res);
        gTeamSel->bg[0] = res;
        gTeamSel->stageBuf ^= 1;
        gTeamSel->flags |= TEAMSEL_STAGE_READY;
        gTeamSel->stageState = TEAMSEL_LOAD_IDLE;
        break;
    case TEAMSEL_LOAD_IDLE:
        if (gTeamSel->faceState != TEAMSEL_LOAD_IDLE) {
            break;
        }
        if (gTeamSel->flags & TEAMSEL_STAGE_CHANGE) {
            gTeamSel->stageState = TEAMSEL_LOAD_RESTART;
        }
        break;
    }
}

/* The stage under the cursor changed: start the cross fade, or abort a load in progress, and ask for the new picture. */
void TeamSel_RequestStage(void) {
    if (gTeamSel->flags & TEAMSEL_STAGE_READY) {
        gTeamSel->flags ^= TEAMSEL_STAGE_READY;
    } else {
        gTeamSel->stageState = TEAMSEL_LOAD_ABORT;
    }
    gTeamSel->flags |= TEAMSEL_STAGE_CHANGE;
    gTeamSel->bg[0] = gTeamSel->stageRes[gTeamSel->stageBuf];
    gTeamSel->bg[1] = gTeamSel->stageRes[gTeamSel->stageBuf ^ 1];
}

/* Sends the clip of one chip or plate of a movie to a label; `kind` says which clip and from which cursor. */
void TeamSel_ClipGoto(s32 flash, s32 side, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *f = &gTeamSel->flash[flash];

    switch (kind) {
    case TEAMSEL_CLIP_CHIP:
        sprintf(name, "mc_chara_chip_%03d", gTeamSel->side[side]->member[gTeamSel->side[side]->cur].col);
        break;
    case TEAMSEL_CLIP_FORM_CHIP:
        sprintf(name, "mc_chara_chip_%03d", gTeamSel->side[side]->member[gTeamSel->side[side]->cur].form);
        break;
    case TEAMSEL_CLIP_CUSTOM_PLATE:
        sprintf(name, "mc_custom_plate_%d", gTeamSel->side[side]->member[gTeamSel->side[side]->cur].plate + 1);
        break;
    case TEAMSEL_CLIP_COLOR_PLATE:
        sprintf(name, "mc_color_plate_%d", gTeamSel->side[side]->member[gTeamSel->side[side]->cur].color + 1);
        break;
    case TEAMSEL_CLIP_TEAM:
        switch (gTeamSel->side[side]->cur) {
        case TS_MEMBER_MAX:
            sprintf(name, "mc_menu_plate");
            break;
        default:
            sprintf(name, "mc_team_%d", gTeamSel->side[side]->cur);
            break;
        }
        break;
    case TEAMSEL_CLIP_CUSTOM_CHIP:
        sprintf(name, "mc_chara_chip_%03d", gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customCol);
        break;
    case TEAMSEL_CLIP_STAGE_CHIP:
        if (gTeamSel->stage->col == 6) {
            sprintf(name, "mc_bgm_now");
        } else {
            sprintf(name, "mc_map_chip_%02d", gTeamSel->stage->col);
        }
        break;
    case 3:
    case 10:
        return;
    case 4:
    case 8:
    default:
        /* original oddity: these kinds go on with `name` never written */
        break;
    }
    Flash_FindLabel(f, NULL, name, &ref);
    Flash_ClipGotoLabel(f, &ref, label);
}

#define TS_RES(n) \
    res = (MTexRes *)MPACK_AT(gTeamSel->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 5), builds the grids and restores the last choices. */
void TeamSel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    s32 j;

    gTeamSel = Heap_Alloc(sizeof(TeamSel), 0x20, 0, 2);
    memset(gTeamSel, 0, sizeof(TeamSel));
    gTeamSel->pack = (u32 *)MPACK_AT(gMenuArc5, section);
    gTeamSel->res = Sprite_Unpack(gTeamSel->pack, NULL, NULL);
    TS_RES(47);
    gTeamSel->tex[0] = MTEX(res, 0);
    TS_RES(1);
    gTeamSel->tex[17] = MTEX(res, 0);
    gTeamSel->tex[16] = MTEX(res, 1);
    TS_RES(2);
    gTeamSel->tex[18] = MTEX(res, 1);
    TS_RES(3);
    gTeamSel->tex[20] = MTEX(res, 0);
    TS_RES(4);
    gTeamSel->tex[19] = MTEX(res, 0);
    TS_RES(5);
    gTeamSel->tex[9] = MTEX(res, 0);
    gTeamSel->tex[40] = MTEX(res, 1);
    if (gTsProgress->players == 1) {
        TS_RES(50);
        gTeamSel->tex[10] = MTEX(res, 0);
    }
    TS_RES(6);
    gTeamSel->tex[39] = MTEX(res, 0);
    TS_RES(7);
    gTeamSel->tex[38] = MTEX(res, 0);
    TS_RES(8);
    gTeamSel->tex[2] = MTEX(res, 1);
    gTeamSel->tex[4] = MTEX(res, 2);
    gTeamSel->tex[5] = MTEX(res, 4);
    gTeamSel->tex[6] = MTEX(res, 3);
    TS_RES(9);
    gTeamSel->tex[3] = MTEX(res, 1);
    gTeamSel->tex[1] = MTEX(res, 0);
    TS_RES(10);
    gTeamSel->tex[21] = MTEX(res, 0);
    TS_RES(11);
    gTeamSel->tex[24] = MTEX(res, 0);
    gTeamSel->tex[26] = MTEX(res, 1);
    gTeamSel->tex[25] = MTEX(res, 2);
    gTeamSel->tex[27] = MTEX(res, 3);
    TS_RES(12);
    gTeamSel->tex[47] = MTEX(res, 0);
    gTeamSel->tex[48] = MTEX(res, 1);
    if (gTsProgress->battleType == 2) {
        TS_RES(13);
        gTeamSel->tex[28] = MTEX(res, 0);
        gTeamSel->tex[31] = MTEX(res, 1);
        TS_RES(14);
        gTeamSel->tex[29] = MTEX(res, 0);
        gTeamSel->tex[30] = MTEX(res, 1);
    }
    Flash_Create(&gTeamSel->flash[0], MPACK_AT(gTeamSel->res, 15), gTeamSel->tex);
    Flash_Play(&gTeamSel->flash[0], 1);
    if (gTsProgress->battleType == 2) {
        TS_RES(16);
        gTeamSel->sideTex[0][9] = MTEX(res, 2);
        gTeamSel->sideTex[1][9] = MTEX(res, 2);
        gTeamSel->teamTex[0][2] = MTEX(res, 2);
        gTeamSel->teamTex[1][2] = MTEX(res, 2);
    }
    TS_RES(17);
    gTeamSel->sideTex[0][0] = MTEX(res, 0);
    gTeamSel->sideTex[0][2] = MTEX(res, 1);
    TS_RES(18);
    gTeamSel->sideTex[1][0] = MTEX(res, 0);
    gTeamSel->sideTex[1][2] = MTEX(res, 1);
    TS_RES(19);
    gTeamSel->sideTex[0][1] = MTEX(res, 1);
    gTeamSel->sideTex[0][4] = MTEX(res, 2);
    gTeamSel->sideTex[0][5] = MTEX(res, 4);
    gTeamSel->sideTex[0][6] = MTEX(res, 3);
    TS_RES(20);
    gTeamSel->sideTex[1][1] = MTEX(res, 1);
    gTeamSel->sideTex[1][4] = MTEX(res, 2);
    gTeamSel->sideTex[1][5] = MTEX(res, 4);
    gTeamSel->sideTex[1][6] = MTEX(res, 3);
    TS_RES(21);
    gTeamSel->sideTex[0][7] = MTEX(res, 0);
    gTeamSel->sideTex[1][7] = MTEX(res, 0);
    gTeamSel->tex[7] = MTEX(res, 0);
    TS_RES(22);
    gTeamSel->sideTex[0][3] = MTEX(res, 0);
    gTeamSel->sideTex[1][3] = MTEX(res, 0);
    TS_RES(23);
    gTeamSel->sideTex[0][10] = MTEX(res, 0);
    gTeamSel->sideTex[0][17] = MTEX(res, 1);
    gTeamSel->sideTex[1][10] = MTEX(res, 0);
    gTeamSel->sideTex[1][17] = MTEX(res, 1);
    gTeamSel->plateTex[0][4] = MTEX(res, 1);
    gTeamSel->plateTex[1][4] = MTEX(res, 1);
    gTeamSel->teamTex[0][3] = MTEX(res, 0);
    gTeamSel->teamTex[1][3] = MTEX(res, 0);
    Flash_Create(&gTeamSel->flash[3], MPACK_AT(gTeamSel->res, 24), gTeamSel->sideTex[0]);
    Flash_Play(&gTeamSel->flash[3], 1);
    Flash_Create(&gTeamSel->flash[4], MPACK_AT(gTeamSel->res, 25), gTeamSel->sideTex[1]);
    Flash_Play(&gTeamSel->flash[4], 1);
    TS_RES(48);
    gTeamSel->plateTex[0][9] = MTEX(res, 0);
    gTeamSel->plateTex[0][0] = MTEX(res, 1);
    TS_RES(49);
    gTeamSel->plateTex[1][9] = MTEX(res, 0);
    gTeamSel->plateTex[1][0] = MTEX(res, 1);
    TS_RES(26);
    gTeamSel->plateTex[0][7] = MTEX(res, 0);
    gTeamSel->plateTex[0][3] = MTEX(res, 1);
    TS_RES(27);
    gTeamSel->plateTex[1][7] = MTEX(res, 0);
    gTeamSel->plateTex[1][3] = MTEX(res, 1);
    TS_RES(28);
    gTeamSel->plateTex[0][2] = MTEX(res, 0);
    gTeamSel->plateTex[0][5] = MTEX(res, 1);
    gTeamSel->plateTex[1][2] = MTEX(res, 0);
    gTeamSel->plateTex[1][5] = MTEX(res, 1);
    TS_RES(29);
    gTeamSel->plateTex[0][6] = MTEX(res, 0);
    gTeamSel->plateTex[0][8] = MTEX(res, 1);
    gTeamSel->plateTex[1][6] = MTEX(res, 0);
    gTeamSel->plateTex[1][8] = MTEX(res, 1);
    TS_RES(30);
    gTeamSel->plateTex[0][10] = MTEX(res, 0);
    gTeamSel->plateTex[0][1] = MTEX(res, 1);
    TS_RES(31);
    gTeamSel->plateTex[1][10] = MTEX(res, 0);
    gTeamSel->plateTex[1][1] = MTEX(res, 1);
    Flash_Create(&gTeamSel->flash[5], MPACK_AT(gTeamSel->res, 32), gTeamSel->plateTex[0]);
    Flash_Play(&gTeamSel->flash[5], 1);
    Flash_Create(&gTeamSel->flash[6], MPACK_AT(gTeamSel->res, 33), gTeamSel->plateTex[1]);
    Flash_Play(&gTeamSel->flash[6], 1);
    TS_RES(34);
    gTeamSel->teamTex[0][0] = MTEX(res, 0);
    gTeamSel->teamTex[1][0] = MTEX(res, 0);
    TS_RES(35);
    gTeamSel->teamTex[0][8] = MTEX(res, 0);
    gTeamSel->teamTex[0][11] = MTEX(res, 1);
    gTeamSel->teamTex[0][10] = MTEX(res, 2);
    gTeamSel->teamTex[1][8] = MTEX(res, 0);
    gTeamSel->teamTex[1][11] = MTEX(res, 1);
    gTeamSel->teamTex[1][10] = MTEX(res, 2);
    TS_RES(36);
    gTeamSel->teamTex[0][9] = MTEX(res, 0);
    gTeamSel->teamTex[0][12] = MTEX(res, 1);
    gTeamSel->teamTex[1][9] = MTEX(res, 0);
    gTeamSel->teamTex[1][12] = MTEX(res, 1);
    Flash_Create(&gTeamSel->flash[1], MPACK_AT(gTeamSel->res, 37), gTeamSel->teamTex[0]);
    Flash_Play(&gTeamSel->flash[1], 1);
    Flash_Create(&gTeamSel->flash[2], MPACK_AT(gTeamSel->res, 38), gTeamSel->teamTex[1]);
    Flash_Play(&gTeamSel->flash[2], 1);
    {
        void *panel = MPACK_AT(gTeamSel->res, 53);

        ItemPanel_Init(panel, 0);
        ItemPanel_Init(panel, 1);
    }
    ItemHelp_Init(MPACK_AT(gTeamSel->res, 51));
    TS_RES(55);
    IconWin_Init(MPACK_AT(gTeamSel->res, 54), res);
    gTeamSel->unk3EA0 = MPACK_AT(gCommonRes->data[2], 2);
    gTeamSel->nameText = MPACK_AT(gTeamSel->res, 43);
    gTeamSel->formText = MPACK_AT(gTeamSel->res, 44);
    gTeamSel->chipPack = (u32 *)MPACK_AT(gTeamSel->res, 45);
    for (i = 0; i < TS_CELL_MAX; i++) {
        res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    gTeamSel->stagePack = (u32 *)MPACK_AT(gTeamSel->res, 46);
    for (i = 0; i < 38; i++) {
        res = (MTexRes *)MPACK_AT(gTeamSel->stagePack, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    gTeamSel->stageIds = (s32 *)(MPACK_AT(gTeamSel->res, 39) + 0x10);
    gTeamSel->stageCount = TS_PACK_WORD(gTeamSel->res, 39);
    StgGrid_ApplyUnlocks(&gTeamSel->stageCount, gTeamSel->stageIds);
    gTeamSel->bgmIds = (s32 *)(MPACK_AT(gTeamSel->res, 56) + 0x10);
    gTeamSel->bgmCount = TS_PACK_WORD(gTeamSel->res, 56);
    BgmList_ApplyUnlocks(&gTeamSel->bgmCount, gTeamSel->bgmIds);
    /* the last four entries of the list are not offered; the last one kept becomes "random" */
    gTeamSel->bgmCount -= 4;
    gTeamSel->bgmIds[gTeamSel->bgmCount - 1] = TS_BGM_RANDOM;
    gTeamSel->cells[0] = (TsCell *)(MPACK_AT(gTeamSel->res, 42) + 0x10);
    gTeamSel->masterCount[0] = TS_PACK_WORD(gTeamSel->res, 42);
    gTeamSel->cells[1] = gTeamSel->cells[0];
    gTeamSel->masterCount[1] = gTeamSel->masterCount[0];
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        s32 cols = TS_COLS;

        ChrGrid_Build(&gTeamSel->cellCount[i], gTeamSel->grid[i], &gTeamSel->masterCount[i], gTeamSel->cells[i],
                      &gTeamSel->customCount[i], gTeamSel->custom[i]);
        gTeamSel->masterCount[i] = gTeamSel->cellCount[i];
        gTeamSel->cells[i] = gTeamSel->grid[i];
        gTeamSel->rows[i] = gTeamSel->masterCount[i] / cols;
        if (gTeamSel->masterCount[i] % cols) {
            gTeamSel->rows[i]++;
        }
    }
    gTeamSel->faceFile[0] = Heap_Alloc(0x16800, 0x40, 0, 2);
    gTeamSel->faceFile[1] = Heap_Alloc(0x16800, 0x40, 0, 2);
    gTeamSel->faceRes[0] = Heap_Alloc(0x20800, 0x20, 0, 2);
    gTeamSel->faceRes[1] = Heap_Alloc(0x20800, 0x20, 0, 2);
    gTeamSel->stageFile = Heap_Alloc(0x3B800, 0x40, 0, 2);
    gTeamSel->stageRes[0] = Heap_Alloc(0x43000, 0x20, 0, 2);
    gTeamSel->stageRes[1] = Heap_Alloc(0x43000, 0x20, 0, 2);
    gTeamSel->side[0] = &gTeamSel->sideData[0];
    gTeamSel->side[1] = &gTeamSel->sideData[1];
    gTeamSel->stage = &gTeamSel->stageData;
    *(TsTeam *)gTeamSel->side[0] = gTsProgress->team[0];
    *(TsTeam *)gTeamSel->side[1] = gTsProgress->team[1];
    gTeamSel->players = gTsProgress->players;
    gTeamSel->battleType = gTsProgress->battleType;
    gTeamSel->dpLevel = gTsProgress->dpLevel;
    {
        /* two variables are needed to match: the original leaves a dead `li 6` beside the divisor */
        s32 cols = TS_STAGE_COLS;
        s32 rows = TS_STAGE_COLS;

        gTeamSel->stage->col = gTsProgress->stageCell % cols;
        gTeamSel->stage->row = gTsProgress->stageCell / rows;
    }
    for (i = 0; i < gTeamSel->bgmCount; i++) {
        if (gTeamSel->bgmIds[i] == gTsProgress->bgm) {
            gTeamSel->stage->bgmCursor = i;
        }
    }
    gTeamSel->faceState = TEAMSEL_LOAD_IDLE;
    if (gTeamSel->battleType == 2) {
        switch (gTeamSel->dpLevel) {
        case 0:
            gTeamSel->dpMax = 10;
            break;
        case 1:
            gTeamSel->dpMax = 15;
            break;
        case 2:
            gTeamSel->dpMax = 20;
            break;
        }
    }
    /* a remembered cursor on a cell that cannot be chosen any more goes back to column `side`, row 0 */
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        for (j = 0; j < TS_MEMBER_MAX; j++) {
            if (!ChrGrid_IsSelectable(gTeamSel->cells[i], gTeamSel->side[i]->member[j].col +
                                                           gTeamSel->side[i]->member[j].row * TS_COLS)) {
                memset(&gTeamSel->side[i]->member[j], 0, sizeof(TsMember));
                gTeamSel->side[i]->member[j].col = i;
                gTsProgress->team[i].member[j] = gTeamSel->side[i]->member[j];
            }
        }
    }
    if (!StgGrid_IsSelectable(gTeamSel->stageIds, gTsProgress->stageCell)) {
        gTsProgress->stageCell = 0;
        gTeamSel->stage->col = 0;
        gTeamSel->stage->row = 0;
    }
    TeamSel_SetChips(0);
    TeamSel_SetChips(1);
    TeamSel_SetStageChips();
    gTeamSel->side[0]->chara = gTeamSel->side[0]->chip[0][gTeamSel->side[0]->member[gTeamSel->side[0]->cur].col];
    gTeamSel->side[1]->chara = gTeamSel->side[1]->chip[0][gTeamSel->side[1]->member[gTeamSel->side[1]->cur].col];
    gTeamSel->stage->stage = gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + gTeamSel->stage->col];
    gTeamSel->stage->bgm = gTeamSel->stage->bgmCursor;
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        for (j = 0; j < TS_MEMBER_MAX; j++) {
            gTeamSel->side[i]->member[j].chara = -1;
        }
    }
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        File_LoadSync(gTeamSel->side[i]->chara + TS_FACE_FILE, gTeamSel->faceFile[i], 0x16800);
        Sprite_Unpack(gTeamSel->faceFile[i], gTeamSel->faceRes[i], NULL);
        res = gTeamSel->faceRes[i];
        Res_RelocateOffsets(&res, res, res);
        gTeamSel->tex[i ? 22 : 23] = MTEX(res, 0);
        gTeamSel->side[i]->flags |= TEAMSEL_SIDE_FACE_READY;
    }
    File_LoadSync(gTeamSel->stage->stage + TS_STAGE_FILE, gTeamSel->stageFile, 0x3B800);
    Sprite_Unpack(gTeamSel->stageFile, gTeamSel->stageRes[gTeamSel->stageBuf], NULL);
    res = gTeamSel->stageRes[gTeamSel->stageBuf];
    Res_RelocateOffsets(&res, res, res);
    gTeamSel->bg[0] = res;
    gTeamSel->bg[1] = res;
    gTeamSel->stageState = TEAMSEL_LOAD_IDLE;
    gTeamSel->flags |= TEAMSEL_STAGE_READY;
    gTeamSel->bgAlpha[0] = 0x80;
    gTeamSel->bgAlpha[1] = 0;
    gTeamSel->stageBuf = 1;
    if (gTeamSel->bgmIds[gTeamSel->stage->bgm] == TS_BGM_RANDOM) {
        Bgm_Play(Rand_Range(9) + 0x10B1E);
    } else {
        Bgm_Play(gTeamSel->bgmIds[gTeamSel->stage->bgm] + 0x10B16);
    }
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        TextBox_Init(&gTeamSel->nameBox[i], gTeamSel->nameText, i + 1);
        TextBox_SetUnk80(&gTeamSel->nameBox[i], 1);
        TextBox_Init(&gTeamSel->formBox[i], gTeamSel->formText, i + 3);
        TextBox_SetUnk80(&gTeamSel->formBox[i], 1);
    }
}

/* Frees the screen: the windows, the movies, the picture buffers and the work area. */
void TeamSel_Term(void) {
    s32 i;

    IconWin_Term();
    ItemHelp_Term();
    ItemPanel_Term(1);
    ItemPanel_Term(0);
    for (i = 0; i < TEAMSEL_FLASH_NUM; i++) {
        Flash_Destroy(&gTeamSel->flash[i]);
    }
    if (gTeamSel->stageRes[1] != NULL) {
        Heap_Free(gTeamSel->stageRes[1]);
        gTeamSel->stageRes[1] = NULL;
    }
    if (gTeamSel->stageRes[0] != NULL) {
        Heap_Free(gTeamSel->stageRes[0]);
        gTeamSel->stageRes[0] = NULL;
    }
    if (gTeamSel->stageFile != NULL) {
        Heap_Free(gTeamSel->stageFile);
        gTeamSel->stageFile = NULL;
    }
    if (gTeamSel->faceRes[1] != NULL) {
        Heap_Free(gTeamSel->faceRes[1]);
        gTeamSel->faceRes[1] = NULL;
    }
    if (gTeamSel->faceRes[0] != NULL) {
        Heap_Free(gTeamSel->faceRes[0]);
        gTeamSel->faceRes[0] = NULL;
    }
    if (gTeamSel->faceFile[1] != NULL) {
        Heap_Free(gTeamSel->faceFile[1]);
        gTeamSel->faceFile[1] = NULL;
    }
    if (gTeamSel->faceFile[0] != NULL) {
        Heap_Free(gTeamSel->faceFile[0]);
        gTeamSel->faceFile[0] = NULL;
    }
    if (gTeamSel->res != NULL) {
        Heap_Free(gTeamSel->res);
        gTeamSel->res = NULL;
    }
    if (gTeamSel != NULL) {
        Heap_Free(gTeamSel);
        gTeamSel = NULL;
    }
}

/* Draws the two stage pictures (cross fade), sets up every clip of the seven movies and draws them and the windows. */
void TeamSel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    char name2[64];
    s32 i;
    s32 j;
    MFlash *flash;
    s32 chara;
    s32 cost;

    for (i = 0; i < 2; i++) {
        if (gTeamSel->flags & TEAMSEL_STAGE_READY) {
            switch (i) {
            case 0:
                gTeamSel->bgAlpha[i] += 5;
                if (gTeamSel->bgAlpha[i] >= 0x80) {
                    gTeamSel->bgAlpha[i] = 0x80;
                }
                break;
            case 1:
                gTeamSel->bgAlpha[i] -= 5;
                if (gTeamSel->bgAlpha[i] < 0) {
                    gTeamSel->bgAlpha[i] = 0;
                }
                break;
            }
        } else {
            /* while the new picture loads only the old one is drawn, opaque */
            if (i == 0) {
                gTeamSel->bgAlpha[i] = 0;
                continue;
            } else if (i == 1) {
                gTeamSel->bgAlpha[i] = 0x80;
            }
        }
        Sprite_DrawPicture(gTeamSel->bg[i], 0, 0, gTeamSel->bgAlpha[i]);
    }
    flash = &gTeamSel->flash[0];
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        Flash_FindLabel(flash, NULL, i ? "mc_face_mask_r" : "mc_face_mask_l", &ref);
        if (gTeamSel->faceMask != 0) {
            Flash_ClipSetFlags(flash, &ref, 0x102, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 0x102, 0);
        }
        if ((gTeamSel->side[i]->flags & (TEAMSEL_SIDE_NO_FACE | TEAMSEL_SIDE_FACE_READY)) == TEAMSEL_SIDE_FACE_READY) {
            gTeamSel->faceAlpha[i] += 0.075f;
            if (gTeamSel->faceAlpha[i] >= 1.0f) {
                gTeamSel->faceAlpha[i] = 1.0f;
            }
        } else {
            gTeamSel->faceAlpha[i] = 0.0f;
        }
        Flash_FindLabel(flash, NULL, i ? "mc_single_chara_r" : "mc_single_chara_l", &ref);
        Flash_ClipSetAlpha(flash, &ref, gTeamSel->faceAlpha[i]);
        Flash_ClipSetFlags(flash, &ref, 0x80, (u8)gTeamSel->faceMask);
        Flash_FindLabel(flash, NULL, i ? "mc_name_text_r" : "mc_name_text_l", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, gTeamSel->side[i]->chara, &gTeamSel->nameBox[i]);
        Flash_FindLabel(flash, NULL, i ? "mc_form_text_r" : "mc_form_text_l", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, gTeamSel->side[i]->chara, &gTeamSel->formBox[i]);
    }
    uv.x0 = 0;
    uv.x1 = 0x200;
    uv.y0 = (gTeamSel->stage->stage % 4) * 0x40;
    uv.y1 = uv.y0 + 0x40;
    uv.unk10 = gTeamSel->stage->stage / 4;
    Flash_FindLabel(flash, NULL, "mc_map_name", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetTex(flash, &ref, uv.unk10);
    if (gTeamSel->battleType == 2) {
        for (i = 0; i < TEAMSEL_SIDES; i++) {
            uv.x0 = (gTeamSel->dpLevel % 2) * 0x40;
            uv.y0 = (gTeamSel->dpLevel / 2) * 0x20;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = uv.y0 + 0x20;
            Flash_FindLabel(flash, NULL, i ? "mc_dp_max_r" : "mc_dp_max_l", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Num_Draw(flash, i ? "mc_dp_total_r_%d" : "mc_dp_total_l_%d", 0, 2, gTeamSel->side[i]->cost, 0x20, 0x40, 1);
        }
    }
    Flash_FindLabel(flash, NULL, "mc_map_mask", &ref);
    if (gTeamSel->stage->mask != 0) {
        Flash_ClipSetFlags(flash, &ref, 0x102, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 0x102, 0);
    }
    for (i = 0; i < 12; i++) {
        sprintf(name, "mc_map_chip_%02d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetFlags(flash, &ref, 0x80, (u8)gTeamSel->stage->mask);
    }
    for (i = 0; i < 2; i++) {
        uv.x0 = i * 0x20;
        uv.x1 = uv.x0 + 0x20;
        uv.y0 = 0x20;
        uv.y1 = 0x40;
        Flash_FindLabel(flash, i ? "mc_yajirusi_down" : "mc_yajirusi_up",
                        i ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    uv.x0 = 0;
    uv.x1 = 0x200;
    uv.y0 = (gTeamSel->bgmIds[gTeamSel->stage->bgmCursor] % 8) * 0x20;
    uv.y1 = uv.y0 + 0x20;
    uv.unk10 = gTeamSel->bgmIds[gTeamSel->stage->bgmCursor] / 8;
    Flash_FindLabel(flash, "mc_bgm_now", "mc_bgm_now_text_off", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetTex(flash, &ref, uv.unk10);
    Flash_FindLabel(flash, "mc_bgm_now", "mc_bgm_now_text_on", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetTex(flash, &ref, uv.unk10);
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        flash = &gTeamSel->flash[5 + i];
        uv.x0 = (i ^ 1) * 0x20;
        uv.x1 = uv.x0 + 0x20;
        uv.y0 = 0;
        uv.y1 = 0x20;
        Flash_FindLabel(flash, NULL, "mc_yajirusi", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        for (j = 0; j < 4; j++) {
            uv.x0 = 0;
            uv.x1 = 0x100;
            uv.y0 = j * 0x20;
            uv.y1 = uv.y0 + 0x20;
            sprintf(name, "mc_custom_plate_%d", j + 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetColor(flash, &ref, j < gTeamSel->plateCount[i] ? 1.0f : 0.3f);
            Flash_FindLabel(flash, name, "mc_custom_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_custom_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if (j == 0) {
                Flash_FindLabel(flash, name, "mc_yajirusi", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_FindLabel(flash, name, "mc_yajirusi", &ref);
                Flash_ClipSetFlags(flash, &ref, 2,
                                   gTeamSel->cells[i][gTeamSel->side[i]->member[gTeamSel->side[i]->cur].col +
                                                      gTeamSel->side[i]->member[gTeamSel->side[i]->cur].row * 7].id !=
                                       TS_RANDOM);
            }
        }
        for (j = 0; j < 4; j++) {
            uv.x0 = (j % 2) * 0x40;
            uv.x1 = uv.x0 + 0x40;
            uv.y0 = (j / 2) * 0x20;
            uv.y1 = uv.y0 + 0x20;
            sprintf(name, "mc_color_plate_%d", j + 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            if (j < gTeamSel->colorCount[i]) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
                switch (gTeamSel->colorCount[i]) {
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
    }
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        u32 n;

        flash = &gTeamSel->flash[3 + i];
        n = 0;
        switch (gTeamSel->players) {
        case 0:
            n = i ? 2 : 0;
            break;
        case 1:
            n = i ? 1 : 0;
            break;
        case 2:
            n = 2;
            break;
        }
        /* the cell of the "1P / 2P / COM" text; the original takes the remainder by subtraction, not `& 1` */
        uv.y0 = (n / 2) * 0x20;
        uv.x0 = (n - (n / 2) * 2) * 0x40;
        uv.y1 = uv.y0 + 0x20;
        uv.x1 = uv.x0 + 0x40;
        Flash_FindLabel(flash, NULL, "mc_plate_text", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (gTeamSel->rows[i] >= 2) {
            for (j = 0; j < 2; j++) {
                uv.x0 = j * 0x20;
                uv.y0 = 0x20;
                uv.x1 = uv.x0 + 0x20;
                uv.y1 = 0x40;
                Flash_FindLabel(flash, j ? "mc_yajirusi_down" : "mc_yajirusi_up",
                                j ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_FindLabel(flash, NULL, j ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
                if (gTeamSel->side[i]->flags & TEAMSEL_SIDE_FORM) {
                    Flash_ClipSetFlags(flash, &ref, 2, 0);
                } else if (i == 1 && gTeamSel->players != 1) {
                    if (gTeamSel->side[0]->state != 8) {
                        Flash_ClipSetFlags(flash, &ref, 2, 0);
                    } else {
                        Flash_ClipSetFlags(flash, &ref, 2, 1);
                    }
                } else {
                    Flash_ClipSetFlags(flash, &ref, 2, 1);
                }
            }
        } else {
            for (j = 0; j < 2; j++) {
                Flash_FindLabel(flash, NULL, j ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
        Flash_FindLabel(flash, NULL, "mc_chara_mask", &ref);
        if (gTeamSel->side[i]->mask != 0) {
            Flash_ClipSetFlags(flash, &ref, 0x102, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 0x102, 0);
        }
        for (j = 0; j < 14; j++) {
            s32 cols = TS_COLS;

            sprintf(name, "mc_chara_chip_%03d", j);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 0x80, (u8)gTeamSel->side[i]->mask);
            chara = gTeamSel->side[i]->chip[j / cols][j % cols];
            if (gTeamSel->battleType == 2) {
                if (chara < TS_RANDOM) {
                    cost = ChrTbl_GetCost(chara);
                } else {
                    cost = 0;
                }
                sprintf(name2, "mc_chara_%03d", j);
                Flash_FindLabel(flash, name, name2, &ref);
                if (gTeamSel->side[i]->cost + cost > gTeamSel->dpMax) {
                    Flash_ClipSetColor(flash, &ref, 0.4f);
                } else if (chara < TS_RANDOM) {
                    Flash_ClipSetColor(flash, &ref, TeamSel_IsCharaFree(i, chara) ? 1.0f : 0.4f);
                } else {
                    Flash_ClipSetColor(flash, &ref, 1.0f);
                }
                sprintf(name2, "mc_dp_num_%03d", j);
                Flash_FindLabel(flash, name, name2, &ref);
                if (chara < TS_RANDOM) {
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
                sprintf(name2, "mc_chara_%03d", j);
                Flash_FindLabel(flash, name, name2, &ref);
                if (chara < TS_RANDOM) {
                    Flash_ClipSetColor(flash, &ref, TeamSel_IsCharaFree(i, chara) ? 1.0f : 0.4f);
                } else {
                    Flash_ClipSetColor(flash, &ref, 1.0f);
                }
            }
        }
    }
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        flash = &gTeamSel->flash[1 + i];
        for (j = 0; j < TS_MEMBER_MAX; j++) {
            sprintf(name, "mc_team_%d", j);
            if (i == 0) {
                uv.x0 = (j % 4) * 0x40;
                uv.x1 = uv.x0 + 0x40;
                uv.y0 = (j / 4) * 0x48;
                uv.y1 = uv.y0 + 0x48;
            } else {
                uv.x0 = ((j + 5) % 4) * 0x40;
                uv.x1 = uv.x0 + 0x40;
                uv.y0 = ((j + 5) / 4) * 0x48;
                uv.y1 = uv.y0 + 0x48;
            }
            sprintf(name2, "mc_team_plate_%d", j);
            Flash_FindLabel(flash, name, name2, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            sprintf(name2, "mc_dp_num_%03d", j);
            Flash_FindLabel(flash, name, name2, &ref);
            if (j < gTeamSel->side[i]->memberCount) {
                cost = ChrTbl_GetCost(gTeamSel->side[i]->member[j].chara);
                uv.x0 = (cost % 4) * 0x20;
                uv.x1 = uv.x0 + 0x20;
                uv.y0 = (cost / 4) * 0x20;
                uv.y1 = uv.y0 + 0x20;
                Flash_ClipSetFlags(flash, &ref, 2, 1);
                Flash_ClipSetUv(flash, &ref, &uv);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            sprintf(name2, "mc_dp_plate_%03d", j);
            Flash_FindLabel(flash, name, name2, &ref);
            if (j < gTeamSel->side[i]->memberCount) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
    }
    for (i = 0; i < TEAMSEL_FLASH_NUM; i++) {
        Flash_Draw(&gTeamSel->flash[i]);
    }
    Font_FlushAll();
    IconWin_Draw();
    ItemPanel_Draw(0);
    ItemPanel_Draw(1);
    ItemHelp_Draw(gTeamSel->help);
}

/* ======== 0x34D368: the per-frame update, the pad handler and the main loop ======== */

/* ---- ItemPanel (the next object, src/menu/menu_g.c) ---- */
extern void ItemPanel_Update(s32 side);
extern s32 ItemPanel_Input(s32 side, s32 pad);   /* > 0: item row + 1 whose help was asked for; < 0: closed */
extern void ItemPanel_SetChara(s32 side, s32 chara, s32 slot, s32 set, s32 fromRec);
extern void ItemPanel_Show(s32 side);
extern void ItemPanel_Hide(s32 side);
extern void ItemHelp_Open(void);                 /* opens the help window over the item panel */
extern void ItemHelp_Close(void);                 /* closes it */

/* ---- main executable ---- */
extern void IconWin_SetIcon(s32 icon);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);
extern s32 CpuLevel_FromSetting(u32 setting);
extern s32 ChrGrid_MoveRight(TsCell *cells, s32 *col, s32 row);
extern s32 ChrGrid_MoveLeft(TsCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveDown(TsCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_MoveUp(TsCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_NextForm(s32 *forms, s32 *index);
extern void ChrGrid_PrevForm(s32 *forms, s32 *index);
extern s32 ChrGrid_FixCursor(TsCell *cells, s32 *col, s32 *row, s32 rows);
extern void StgGrid_MoveRight(s32 *ids, s32 *col, s32 row);
extern void StgGrid_MoveLeft(s32 *ids, s32 *col, s32 row);
extern void StgGrid_MoveDown(s32 *ids, s32 *col, s32 *row, s32 rows);
extern void StgGrid_MoveUp(s32 *ids, s32 *col, s32 *row, s32 rows);
extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                                void *bits);
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                                  void *items);
extern void BattleSetup_Finish(void);

/* Advances the movies and the two item panels, and drops the "hidden" marks once the movies say so. */
void TeamSel_Update(void) {
    s32 i;

    for (i = 0; i < TEAMSEL_FLASH_NUM; i++) {
        Flash_Advance(&gTeamSel->flash[i]);
    }
    ItemPanel_Update(0);
    ItemPanel_Update(1);
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        if (gTeamSel->side[i]->mask != 0) {
            if ((&gTeamSel->flash[3])[i].trig & 1) {
                gTeamSel->side[i]->mask = 0;
            }
        }
    }
    if (gTeamSel->stage->mask != 0) {
        if (gTeamSel->flash[0].trig & 1) {
            gTeamSel->stage->mask = 0;
        }
    }
    if (gTeamSel->faceMask != 0) {
        if ((s32)gTeamSel->flash[0].trig < 0) {
            gTeamSel->faceMask = 0;
        }
    }
}

/* The side pad i is choosing for, that side's work, and the member under its cursor. */
#define CUR_N (i + gTeamSel->base)
#define CUR_SIDE (gTeamSel->side[i + gTeamSel->base])
#define CUR_MEMBER (gTeamSel->side[i + gTeamSel->base]->member[gTeamSel->side[i + gTeamSel->base]->cur])
/* The same member's items (one object, copied whole). */
#define CUR_ITEMS (CUR_MEMBER.items)
#define CUR_CELL                                                                                                        \
    (gTeamSel->cells[i + gTeamSel->base][CUR_MEMBER.row * TS_COLS + CUR_MEMBER.col])
#define CUR_CUSTOM_CELL                                                                                                 \
    (gTeamSel->custom[i + gTeamSel->base][CUR_MEMBER.customCol + CUR_MEMBER.customRow * TS_COLS])

/*
 * The pad handler, once per frame while no fade runs and the screen is not frozen. Does nothing until the
 * background movie accepts input. While TEAMSEL_STAGE is set pad 0 chooses the stage and the music; before
 * that each pad drives its side's state machine (TeamSelSide.state). `*result` is cleared when the screen is
 * left with cancel (TeamSel_Run then returns 0 and writes no battle setup).
 *
 * The return type is a matching device: the original makes no tail calls (so it is not `void`) and treats
 * $v0 as dead at the exit (so it does not return an integer); a function declared `f32` that returns nothing
 * reproduces both. The caller ignores the value.
 */
f32 TeamSel_Input(s32 *result) {
    s32 i;
    s32 pads = 0;
    s32 r;
    s32 chara;
    s32 idx;

    if (!(gTeamSel->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    switch (gTeamSel->players) {
    case TEAMSEL_PLAYERS_VS_CPU:
    case TEAMSEL_PLAYERS_CPU_CPU:
        pads = 1;
        break;
    case TEAMSEL_PLAYERS_TWO:
        pads = 2;
        break;
    }
    if (!(gTeamSel->flags & TEAMSEL_STARTED)) {
        for (i = 0; i < pads; i++) {
            TeamSel_ClipGoto(i + 3, i, TEAMSEL_CLIP_CHIP, "fl_on_start");
        }
        gTeamSel->flags |= TEAMSEL_STARTED;
    }

    if (gTeamSel->flags & TEAMSEL_STAGE) {
        switch (gTeamSel->stage->state) {
        case TEAMSEL_STAGE_GRID:
            if (gPad[0].gameRepeat & 1) {
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveLeft(gTeamSel->stageIds, &gTeamSel->stage->col, gTeamSel->stage->row);
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                if (gTeamSel->stage->col != TS_STAGE_COLS) {
                    gTeamSel->stage->stage =
                        gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + gTeamSel->stage->col];
                    TeamSel_RequestStage();
                }
                Snd_PlaySe(2, 0);
            } else if (gPad[0].gameRepeat & 2) {
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveRight(gTeamSel->stageIds, &gTeamSel->stage->col, gTeamSel->stage->row);
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                if (gTeamSel->stage->col != TS_STAGE_COLS) {
                    gTeamSel->stage->stage =
                        gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + gTeamSel->stage->col];
                    TeamSel_RequestStage();
                }
                Snd_PlaySe(2, 0);
            } else if ((gPad[0].gameRepeat & 8) && gTeamSel->stage->col != TS_STAGE_COLS) {
                Flash_GotoLabel(&gTeamSel->flash[0], "fl_reel_down", 1);
                gTeamSel->stage->mask = 1;
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveUp(gTeamSel->stageIds, &gTeamSel->stage->col, &gTeamSel->stage->row, TS_STAGE_COLS);
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                TeamSel_SetStageChips();
                gTeamSel->stage->stage = gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + gTeamSel->stage->col];
                TeamSel_RequestStage();
                Snd_PlaySe(2, 2);
            } else if ((gPad[0].gameRepeat & 4) && gTeamSel->stage->col != TS_STAGE_COLS) {
                Flash_GotoLabel(&gTeamSel->flash[0], "fl_reel_up", 1);
                gTeamSel->stage->mask = 1;
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveDown(gTeamSel->stageIds, &gTeamSel->stage->col, &gTeamSel->stage->row, TS_STAGE_COLS);
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                TeamSel_SetStageChips();
                gTeamSel->stage->stage = gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + gTeamSel->stage->col];
                TeamSel_RequestStage();
                Snd_PlaySe(2, 1);
            } else if (gPad[0].gamePressed & 0x200) {
                gTeamSel->stage->mask = 0;
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_ok");
                if (gTeamSel->stage->col == TS_STAGE_COLS) {
                    /* the music chip: open the music list */
                    Flash_GotoLabel(&gTeamSel->flash[0], "fl_bgm", 1);
                    IconWin_SetIcon(1);
                    TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_BGM, "fl_on_start");
                    gTeamSel->stage->bgmCursor = gTeamSel->stage->bgm;
                    gTeamSel->stage->state = TEAMSEL_STAGE_BGM;
                } else {
                    /* the stage is chosen: the battle starts after 60 frames */
                    Flash_GotoLabel(&gTeamSel->flash[0], "fl_vs", 1);
                    IconWin_Close();
                    gTeamSel->faceMask = 0;
                    gTeamSel->flags |= TEAMSEL_DECIDED;
                    gTeamSel->flags |= TEAMSEL_LEAVING;
                    gTeamSel->timer = 0x3C;
                }
                Snd_PlaySe(1, 1);
            } else if (gPad[0].gamePressed & 0x400) {
                /* back to the teams */
                gTeamSel->stage->mask = 0;
                gTeamSel->flags ^= TEAMSEL_STAGE;
                Flash_GotoLabel(&gTeamSel->flash[0], "fl_map_cansel", 1);
                IconWin_Close();
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_off_start");
                if (gTeamSel->players != TEAMSEL_PLAYERS_TWO) {
                    Flash_GotoLabel(&gTeamSel->flash[2], "fl_fast_in", 1);
                    TeamSel_ClipGoto(2, 1, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    gTeamSel->side[1]->state = TEAMSEL_ST_TEAM;
                } else {
                    Flash_GotoLabel(&gTeamSel->flash[1], "fl_fast_in", 1);
                    Flash_GotoLabel(&gTeamSel->flash[2], "fl_fast_in", 1);
                    TeamSel_ClipGoto(1, 0, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    TeamSel_ClipGoto(2, 1, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    gTeamSel->side[0]->state = TEAMSEL_ST_TEAM;
                    gTeamSel->side[1]->state = TEAMSEL_ST_TEAM;
                }
                Snd_PlaySe(1, 2);
            }
            break;
        case TEAMSEL_STAGE_BGM:
            if (gPad[0].gameRepeat & 8) {
                gTeamSel->stage->bgmCursor--;
                if (gTeamSel->stage->bgmCursor < 0) {
                    gTeamSel->stage->bgmCursor = gTeamSel->bgmCount - 1;
                }
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gameRepeat & 4) {
                gTeamSel->stage->bgmCursor++;
                if (gTeamSel->stage->bgmCursor > gTeamSel->bgmCount - 1) {
                    gTeamSel->stage->bgmCursor = 0;
                }
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gamePressed & 0x200) {
                if (gTeamSel->bgmIds[gTeamSel->stage->bgmCursor] != TS_BGM_LOCKED) {
                    Flash_GotoLabel(&gTeamSel->flash[0], "fl_bgm_cansel", 1);
                    IconWin_SetIcon(0);
                    TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_BGM, "fl_off_start");
                    TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                    gTeamSel->stage->bgm = gTeamSel->stage->bgmCursor;
                    if (gTeamSel->bgmIds[gTeamSel->stage->bgm] != TS_BGM_RANDOM) {
                        Bgm_Play(gTeamSel->bgmIds[gTeamSel->stage->bgm] + TS_BGM_FILE);
                    }
                    gTeamSel->stage->state = TEAMSEL_STAGE_GRID;
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
            } else if (gPad[0].gamePressed & 0x400) {
                Flash_GotoLabel(&gTeamSel->flash[0], "fl_bgm_cansel", 1);
                IconWin_SetIcon(0);
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_BGM, "fl_off_start");
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                gTeamSel->stage->bgmCursor = gTeamSel->stage->bgm;
                gTeamSel->stage->state = TEAMSEL_STAGE_GRID;
                Snd_PlaySe(1, 2);
            }
            break;
        }
    } else {
        for (i = 0; i < pads; i++) {
            /* while the other side's item panel is open this pad is ignored */
            if (gTeamSel->side[(i + gTeamSel->base) ^ 1]->flags & TEAMSEL_SIDE_PANEL) {
                continue;
            }
            switch (CUR_SIDE->state) {
            case TEAMSEL_ST_GRID:
                if (gPad[i].gameRepeat & 1) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                    ChrGrid_MoveLeft(gTeamSel->cells[CUR_N], &CUR_MEMBER.col, CUR_MEMBER.row);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gameRepeat & 2) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                    ChrGrid_MoveRight(gTeamSel->cells[CUR_N], &CUR_MEMBER.col, CUR_MEMBER.row);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gameRepeat & 8) {
                    if (gTeamSel->rows[CUR_N] < 2) {
                        continue;
                    }
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_reel_down", 1);
                    CUR_SIDE->mask = 1;
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                    ChrGrid_MoveUp(gTeamSel->cells[CUR_N], &CUR_MEMBER.col, &CUR_MEMBER.row, gTeamSel->rows[CUR_N]);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                    TeamSel_SetChips(CUR_N);
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 2);
                } else if (gPad[i].gameRepeat & 4) {
                    if (gTeamSel->rows[CUR_N] < 2) {
                        continue;
                    }
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_reel_up", 1);
                    CUR_SIDE->mask = 1;
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                    ChrGrid_MoveDown(gTeamSel->cells[CUR_N], &CUR_MEMBER.col, &CUR_MEMBER.row, gTeamSel->rows[CUR_N]);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                    TeamSel_SetChips(CUR_N);
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 1);
                } else if (gPad[i].gamePressed & 0x200) {
                    if (CUR_CELL.id == TS_RANDOM) {
                        /* random: straight to the plates; the character is drawn when the costume is confirmed */
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_custom_in", 1);
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_ok");
                        gTeamSel->plateCount[CUR_N] = (gTeamSel->battleType == 2) ? 1 : 4;
                        if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                            CUR_MEMBER.plate = 0;
                        }
                        TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                        gTeamSel->colorCount[CUR_N] = 2;
                        if (!(CUR_MEMBER.color < gTeamSel->colorCount[CUR_N])) {
                            CUR_MEMBER.color = 0;
                        }
                        CUR_SIDE->state = TEAMSEL_ST_PLATE;
                        Snd_PlaySe(1, 1);
                    } else if (CUR_CELL.id == TS_CUSTOM) {
                        /* the list of saved custom characters, if there is one */
                        if (gTeamSel->customCount[CUR_N] != 0) {
                            ChrGrid_FixCursor(gTeamSel->custom[CUR_N], &CUR_MEMBER.customCol, &CUR_MEMBER.customRow,
                                              TS_CUSTOM_ROWS);
                            CUR_SIDE->flags |= TEAMSEL_SIDE_CUSTOM;
                            Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_form", 1);
                            CUR_SIDE->mask = 1;
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                            TeamSel_SetCustomChips(CUR_N);
                            CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                            TeamSel_RequestFace(CUR_N);
                            CUR_SIDE->state = TEAMSEL_ST_CUSTOM;
                            Snd_PlaySe(2, 0x29);
                        } else {
                            Snd_PlaySe(1, 7);
                        }
                    } else if (TeamSel_FitsDp(CUR_N, CUR_SIDE->chara)) {
                        if (CUR_CELL.formCount != 0) {
                            CUR_SIDE->flags |= TEAMSEL_SIDE_FORM;
                            if (!(CUR_MEMBER.form < CUR_CELL.formCount)) {
                                CUR_MEMBER.form = 0;
                            }
                            Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_form", 1);
                            CUR_SIDE->mask = 1;
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_on_start");
                            TeamSel_SetGridFormChips(CUR_N);
                            CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.form];
                            TeamSel_RequestFace(CUR_N);
                            CUR_SIDE->state = TEAMSEL_ST_FORM;
                            Snd_PlaySe(2, 0x29);
                        } else if (TeamSel_IsCharaFree(CUR_N, CUR_SIDE->chara)) {
                            Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_custom_in", 1);
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_ok");
                            gTeamSel->plateCount[CUR_N] = (gTeamSel->battleType == 2) ? 1 : 4;
                            if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                                CUR_MEMBER.plate = 0;
                            }
                            TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                            gTeamSel->colorCount[CUR_N] = ChrTbl_WrapCostume(CUR_SIDE->chara, &CUR_MEMBER.color);
                            CUR_SIDE->state = TEAMSEL_ST_PLATE;
                            Snd_PlaySe(1, 1);
                        } else {
                            Snd_PlaySe(1, 7);
                        }
                    } else {
                        Snd_PlaySe(1, 7);
                    }
                } else if (gPad[i].gamePressed & 0x400) {
                    CUR_SIDE->mask = 0;
                    if (CUR_SIDE->memberCount != 0) {
                        /* back to the member list; the member is as it was */
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_out", 1);
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_in", 1);
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                        CUR_MEMBER = gTeamSel->backup[CUR_N];
                        CUR_SIDE->chara = CUR_MEMBER.chara;
                        TeamSel_RequestFace(CUR_N);
                        TeamSel_SumCost(CUR_N, 0);
                        CUR_SIDE->state = TEAMSEL_ST_TEAM;
                    } else if (gTeamSel->players != TEAMSEL_PLAYERS_TWO) {
                        switch (gTeamSel->base) {
                        case 0:
                            /* leave the screen */
                            ColorFade_StartOut(0, 0, 0, 0x14);
                            *result = 0;
                            break;
                        case 1:
                            /* back to the first team */
                            TeamSel_ClipGoto(i + 4, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                            gTeamSel->base = 0;
                            Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_fast_in", 1);
                            TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                            CUR_SIDE->state = TEAMSEL_ST_TEAM;
                            break;
                        }
                    } else {
                        ColorFade_StartOut(0, 0, 0, 0x14);
                        *result = 0;
                    }
                    Snd_PlaySe(1, 2);
                }
                break;

            case TEAMSEL_ST_FORM:
                if (gPad[i].gameRepeat & 1) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_off_start");
                    ChrGrid_PrevForm(CUR_SIDE->chip[0], &CUR_MEMBER.form);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.form];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gameRepeat & 2) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_off_start");
                    ChrGrid_NextForm(CUR_SIDE->chip[0], &CUR_MEMBER.form);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.form];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gamePressed & 0x200) {
                    if (!TeamSel_FitsDp(CUR_N, CUR_SIDE->chara)) {
                        Snd_PlaySe(1, 7);
                    } else if (!TeamSel_IsCharaFree(CUR_N, CUR_SIDE->chara)) {
                        Snd_PlaySe(1, 7);
                    } else {
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_custom_in", 1);
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_ok");
                        if (CUR_SIDE->flags & TEAMSEL_SIDE_CUSTOM) {
                            gTeamSel->plateCount[CUR_N] = (gTeamSel->battleType == 2) ? 1 : 2;
                            if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                                CUR_MEMBER.plate = 0;
                            }
                        } else {
                            gTeamSel->plateCount[CUR_N] = (gTeamSel->battleType == 2) ? 1 : 4;
                            if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                                CUR_MEMBER.plate = 0;
                            }
                        }
                        TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                        gTeamSel->colorCount[CUR_N] = ChrTbl_WrapCostume(CUR_SIDE->chara, &CUR_MEMBER.color);
                        CUR_SIDE->state = TEAMSEL_ST_PLATE;
                        Snd_PlaySe(1, 1);
                    }
                } else if (gPad[i].gamePressed & 0x400) {
                    CUR_SIDE->flags ^= TEAMSEL_SIDE_FORM;
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_form", 1);
                    CUR_SIDE->mask = 1;
                    if (CUR_SIDE->flags & TEAMSEL_SIDE_CUSTOM) {
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_off_start");
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                        TeamSel_SetCustomChips(CUR_N);
                        CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                        TeamSel_RequestFace(CUR_N);
                        CUR_SIDE->state = TEAMSEL_ST_CUSTOM;
                    } else {
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_off_start");
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                        TeamSel_SetChips(CUR_N);
                        CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                        TeamSel_RequestFace(CUR_N);
                        CUR_SIDE->state = TEAMSEL_ST_GRID;
                    }
                    Snd_PlaySe(2, 0x29);
                }
                break;

            case TEAMSEL_ST_PLATE:
                if (gPad[i].gameRepeat & 8) {
                    if (gTeamSel->plateCount[CUR_N] < 2) {
                        continue;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                    CUR_MEMBER.plate--;
                    if (CUR_MEMBER.plate < 0) {
                        CUR_MEMBER.plate = gTeamSel->plateCount[CUR_N] - 1;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gameRepeat & 4) {
                    if (gTeamSel->plateCount[CUR_N] < 2) {
                        continue;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                    CUR_MEMBER.plate++;
                    if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                        CUR_MEMBER.plate = 0;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gamePressed & ((CUR_N == 0) ? 2 : 1)) {
                    /* towards the middle of the screen: look at the item set of the plate */
                    if (CUR_MEMBER.plate <= 0) {
                        continue;
                    }
                    if (CUR_CELL.id != TS_RANDOM) {
                        TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_ok");
                        if (CUR_CELL.id == TS_CUSTOM) {
                            ItemPanel_SetChara(CUR_N, CUR_SIDE->chara, CUR_MEMBER.customCol + CUR_MEMBER.customRow * TS_COLS, 0, 1);
                        } else {
                            ItemPanel_SetChara(CUR_N, CUR_SIDE->chara, CUR_MEMBER.row * TS_COLS + CUR_MEMBER.col,
                                          CUR_MEMBER.plate - 1, 0);
                        }
                        ItemPanel_Show(CUR_N);
                        CUR_SIDE->flags |= TEAMSEL_SIDE_PANEL;
                        CUR_SIDE->state = TEAMSEL_ST_PANEL;
                    }
                } else if (gPad[i].gamePressed & 0x200) {
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_color_in", 1);
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_ok");
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_on_start");
                    CUR_SIDE->state = TEAMSEL_ST_COLOR;
                    Snd_PlaySe(1, 1);
                } else if (gPad[i].gamePressed & 0x400) {
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_custom_cansel", 1);
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                    if (CUR_SIDE->flags & TEAMSEL_SIDE_FORM) {
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_on_start");
                        CUR_SIDE->state = TEAMSEL_ST_FORM;
                    } else if (CUR_SIDE->flags & TEAMSEL_SIDE_CUSTOM) {
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                        CUR_SIDE->state = TEAMSEL_ST_CUSTOM;
                    } else {
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                        CUR_SIDE->state = TEAMSEL_ST_GRID;
                    }
                    Snd_PlaySe(1, 2);
                }
                break;

            case TEAMSEL_ST_PANEL:
                r = ItemPanel_Input(CUR_N, i);
                if (r > 0) {
                    ItemHelp_Open();
                    gTeamSel->help = r - 1;
                    CUR_SIDE->state = TEAMSEL_ST_PANEL_HELP;
                } else if (r < 0) {
                    ItemPanel_Hide(CUR_N);
                    CUR_SIDE->flags &= ~TEAMSEL_SIDE_PANEL;
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    CUR_SIDE->state = TEAMSEL_ST_PLATE;
                }
                break;

            case TEAMSEL_ST_PANEL_HELP:
                if (gPad[i].gamePressed & 0x600) {
                    ItemHelp_Close();
                    CUR_SIDE->state = TEAMSEL_ST_PANEL;
                    Snd_PlaySe(1, 2);
                }
                break;

            case TEAMSEL_ST_COLOR:
                if (gPad[i].gameRepeat & 1) {
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_off_start");
                    CUR_MEMBER.color--;
                    if (CUR_MEMBER.color < 0) {
                        CUR_MEMBER.color = gTeamSel->colorCount[CUR_N] - 1;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_on_start");
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gameRepeat & 2) {
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_off_start");
                    CUR_MEMBER.color++;
                    if (!(CUR_MEMBER.color < gTeamSel->colorCount[CUR_N])) {
                        CUR_MEMBER.color = 0;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_on_start");
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gamePressed & 0x200) {
                    /* the member is complete: fix the character and its item set */
                    CUR_SIDE->mask = 0;
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_color_ok", 1);
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_out", 1);
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_in", 1);
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_ok");
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                    if (CUR_SIDE->flags & TEAMSEL_SIDE_FORM) {
                        CUR_MEMBER.chara = CUR_SIDE->chip[0][CUR_MEMBER.form];
                        if (CUR_MEMBER.plate != 0) {
                            if (CUR_CELL.id == TS_CUSTOM) {
                                CUR_ITEMS = TS_SAVE->rec[CUR_MEMBER.customCol + CUR_MEMBER.customRow * TS_COLS].items;
                            } else {
                                CUR_ITEMS = TS_SAVE->custom[CUR_MEMBER.row * TS_COLS + CUR_MEMBER.col].set[CUR_MEMBER.plate - 1];
                            }
                        } else {
                            memset(&CUR_ITEMS, 0, sizeof(TsItemSet));
                        }
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_off_start");
                    } else if (CUR_SIDE->flags & TEAMSEL_SIDE_CUSTOM) {
                        CUR_MEMBER.chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                        if (CUR_MEMBER.plate != 0) {
                            CUR_ITEMS = TS_SAVE->rec[CUR_MEMBER.customCol + CUR_MEMBER.customRow * TS_COLS].items;
                        } else {
                            memset(&CUR_ITEMS, 0, sizeof(TsItemSet));
                        }
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    } else {
                        if (CUR_CELL.id == TS_RANDOM) {
                            /* draw a character (and one of its forms) until one is not related to another member */
                            do {
                                for (;;) {
                                    idx = Rand_Range(gTeamSel->masterCount[CUR_N]);
                                    if (gTeamSel->cells[CUR_N][idx].id < TS_RANDOM) {
                                        chara = gTeamSel->cells[CUR_N][idx].id;
                                        break;
                                    }
                                }
                                if (gTeamSel->cells[CUR_N][idx].formCount != 0) {
                                    chara = gTeamSel->cells[CUR_N][idx]
                                                .form[Rand_Range(gTeamSel->cells[CUR_N][idx].formCount)];
                                }
                            } while (!TeamSel_IsCharaFree(CUR_N, chara));
                            CUR_MEMBER.chara = chara;
                        } else {
                            CUR_MEMBER.chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                            idx = CUR_MEMBER.col + CUR_MEMBER.row * TS_COLS;
                        }
                        if (CUR_MEMBER.plate != 0) {
                            CUR_ITEMS = TS_SAVE->custom[idx].set[CUR_MEMBER.plate - 1];
                        } else {
                            memset(&CUR_ITEMS, 0, sizeof(TsItemSet));
                        }
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                    }
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_off_start");
                    if (CUR_SIDE->cur == CUR_SIDE->memberCount) {
                        /* it was a new member: the cursor goes to the next free slot */
                        CUR_SIDE->memberCount = CUR_SIDE->cur + 1;
                        CUR_SIDE->cur = CUR_SIDE->memberCount;
                    }
                    TeamSel_SetTeamTex(CUR_N);
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    if (CUR_SIDE->cur == TS_CUR_MENU) {
                        CUR_SIDE->chara = CUR_SIDE->member[0].chara;
                        TeamSel_RequestFace(CUR_N);
                    }
                    if (CUR_SIDE->flags & TEAMSEL_SIDE_FORM) {
                        CUR_SIDE->flags ^= TEAMSEL_SIDE_FORM;
                    }
                    if (CUR_SIDE->flags & TEAMSEL_SIDE_CUSTOM) {
                        CUR_SIDE->flags ^= TEAMSEL_SIDE_CUSTOM;
                    }
                    CUR_SIDE->state = TEAMSEL_ST_TEAM;
                    Snd_PlaySe(1, 1);
                } else if (gPad[i].gamePressed & 0x400) {
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_color_cansel", 1);
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_off_start");
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    CUR_SIDE->state = TEAMSEL_ST_PLATE;
                    Snd_PlaySe(1, 2);
                }
                break;

            case TEAMSEL_ST_TEAM:
                if (gPad[i].gameRepeat & 1) {
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_off_start");
                    CUR_SIDE->cur--;
                    if (CUR_SIDE->cur < 0) {
                        CUR_SIDE->cur = TS_CUR_MENU;
                    } else if (CUR_SIDE->cur > CUR_SIDE->memberCount) {
                        CUR_SIDE->cur = CUR_SIDE->memberCount;
                    }
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    if (CUR_SIDE->cur == TS_CUR_MENU) {
                        CUR_SIDE->chara = CUR_SIDE->member[0].chara;
                    } else {
                        CUR_SIDE->chara = CUR_MEMBER.chara;
                    }
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gameRepeat & 2) {
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_off_start");
                    CUR_SIDE->cur++;
                    if (CUR_SIDE->cur > TS_CUR_MENU) {
                        CUR_SIDE->cur = 0;
                    } else if (CUR_SIDE->cur > CUR_SIDE->memberCount) {
                        CUR_SIDE->cur = TS_CUR_MENU;
                    }
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    if (CUR_SIDE->cur == TS_CUR_MENU) {
                        CUR_SIDE->chara = CUR_SIDE->member[0].chara;
                    } else {
                        CUR_SIDE->chara = CUR_MEMBER.chara;
                    }
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gamePressed & 0x200) {
                    if (CUR_SIDE->cur != TS_CUR_MENU) {
                        /* choose (or change) the member under the cursor */
                        TeamSel_SumCost(CUR_N, 1);
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_out", 1);
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_in", 1);
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_ok");
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                        TeamSel_SetChips(CUR_N);
                        CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                        TeamSel_RequestFace(CUR_N);
                        gTeamSel->backup[CUR_N] = CUR_MEMBER;
                        CUR_SIDE->state = TEAMSEL_ST_GRID;
                        Snd_PlaySe(1, 1);
                    } else if (CUR_SIDE->memberCount != 0) {
                        /* the team is complete */
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_out", 1);
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_ok");
                        CUR_SIDE->state = TEAMSEL_ST_DONE;
                        if (gTeamSel->players != TEAMSEL_PLAYERS_TWO && gTeamSel->base == 0) {
                            /* one pad: it goes on to the second team */
                            gTeamSel->base = 1;
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                            CUR_SIDE->state = TEAMSEL_ST_GRID;
                        }
                        Snd_PlaySe(1, 1);
                    } else {
                        Snd_PlaySe(1, 7);
                    }
                } else if (gPad[i].gamePressed & 0x400) {
                    /* remove a member */
                    if (CUR_SIDE->memberCount == 1) {
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_out", 1);
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_in", 1);
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_ok");
                        CUR_SIDE->cur = 0;
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                        TeamSel_SetChips(CUR_N);
                        CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                        TeamSel_RequestFace(CUR_N);
                        CUR_SIDE->state = TEAMSEL_ST_GRID;
                    } else if (CUR_SIDE->cur == TS_CUR_MENU) {
                        TeamSel_RemoveMember(CUR_N, CUR_SIDE->memberCount - 1);
                    } else if (CUR_SIDE->cur == CUR_SIDE->memberCount) {
                        TeamSel_RemoveMember(CUR_N, CUR_SIDE->cur - 1);
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_off_start");
                        CUR_SIDE->cur--;
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                        CUR_SIDE->chara = CUR_MEMBER.chara;
                        TeamSel_RequestFace(CUR_N);
                    } else {
                        TeamSel_RemoveMember(CUR_N, CUR_SIDE->cur);
                        CUR_SIDE->chara = CUR_MEMBER.chara;
                        TeamSel_RequestFace(CUR_N);
                    }
                    CUR_SIDE->memberCount--;
                    TeamSel_SetTeamTex(CUR_N);
                    Snd_PlaySe(1, 2);
                }
                break;

            case TEAMSEL_ST_CUSTOM:
                if (gPad[i].gameRepeat & 1) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    ChrGrid_MoveLeft(gTeamSel->custom[CUR_N], &CUR_MEMBER.customCol, CUR_MEMBER.customRow);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gameRepeat & 2) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    ChrGrid_MoveRight(gTeamSel->custom[CUR_N], &CUR_MEMBER.customCol, CUR_MEMBER.customRow);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gameRepeat & 8) {
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_reel_down", 1);
                    CUR_SIDE->mask = 1;
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    ChrGrid_MoveUp(gTeamSel->custom[CUR_N], &CUR_MEMBER.customCol, &CUR_MEMBER.customRow, TS_CUSTOM_ROWS);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                    TeamSel_SetCustomChips(CUR_N);
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 2);
                } else if (gPad[i].gameRepeat & 4) {
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_reel_up", 1);
                    CUR_SIDE->mask = 1;
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    ChrGrid_MoveDown(gTeamSel->custom[CUR_N], &CUR_MEMBER.customCol, &CUR_MEMBER.customRow, TS_CUSTOM_ROWS);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                    TeamSel_SetCustomChips(CUR_N);
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 1);
                } else if (gPad[i].gamePressed & 0x200) {
                    if (!TeamSel_FitsDp(CUR_N, CUR_SIDE->chara)) {
                        Snd_PlaySe(1, 7);
                    } else if (CUR_CUSTOM_CELL.formCount != 0) {
                        CUR_SIDE->flags |= TEAMSEL_SIDE_FORM;
                        if (!(CUR_MEMBER.form < CUR_CUSTOM_CELL.formCount)) {
                            CUR_MEMBER.form = 0;
                        }
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_form", 1);
                        CUR_SIDE->mask = 1;
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_on_start");
                        TeamSel_SetFormChips(CUR_N);
                        CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.form];
                        TeamSel_RequestFace(CUR_N);
                        CUR_SIDE->state = TEAMSEL_ST_FORM;
                        Snd_PlaySe(2, 0x29);
                    } else if (TeamSel_IsCharaFree(CUR_N, CUR_SIDE->chara)) {
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_custom_in", 1);
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_ok");
                        gTeamSel->plateCount[CUR_N] = (gTeamSel->battleType == 2) ? 1 : 2;
                        if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                            CUR_MEMBER.plate = 0;
                        }
                        TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                        gTeamSel->colorCount[CUR_N] = ChrTbl_WrapCostume(CUR_SIDE->chara, &CUR_MEMBER.color);
                        CUR_SIDE->state = TEAMSEL_ST_PLATE;
                        Snd_PlaySe(1, 1);
                    } else {
                        Snd_PlaySe(1, 7);
                    }
                } else if (gPad[i].gamePressed & 0x400) {
                    CUR_SIDE->flags ^= TEAMSEL_SIDE_CUSTOM;
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_form", 1);
                    CUR_SIDE->mask = 1;
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                    TeamSel_SetChips(CUR_N);
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                    TeamSel_RequestFace(CUR_N);
                    CUR_SIDE->state = TEAMSEL_ST_GRID;
                    Snd_PlaySe(2, 0x29);
                }
                break;

            case TEAMSEL_ST_DONE:
                if (gTeamSel->flags & TEAMSEL_STAGE) {
                    continue;
                }
                if (gTeamSel->side[(i + gTeamSel->base) ^ 1]->state == TEAMSEL_ST_DONE) {
                    /* both teams are complete: on to the stage */
                    Flash_GotoLabel(&gTeamSel->flash[0], "fl_map", 1);
                    IconWin_Open();
                    TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                    gTeamSel->flags |= TEAMSEL_STAGE;
                    gTeamSel->faceMask = 1;
                    gTeamSel->stage->state = TEAMSEL_STAGE_GRID;
                } else if (gPad[i].gamePressed & 0x400) {
                    /* two pads: reopen this side's team while the other is still choosing */
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_fast_in", 1);
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    CUR_SIDE->state = TEAMSEL_ST_TEAM;
                    Snd_PlaySe(1, 2);
                }
                break;
            }
        }
    }
}

/*
 * The team select screen: its own frame loop until the fade out is done and both picture loaders are idle.
 * Returns 0 when the screen was left with cancel; otherwise the battle setup has been written (mode 0,
 * the versus battle) and 1 is returned.
 */
s32 TeamSel_Run(s32 section) {
    s32 result = 1;
    s32 i;
    s32 j;
    s32 bgm;
    s32 timeLimit;
    s32 cpuLevel;

    TeamSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        TeamSel_UpdateFaceLoad();
        TeamSel_UpdateStageLoad();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            TeamSel_Update();
        }
        TeamSel_Draw();
        Font_FlushAll();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsFadingOut()) {
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gTeamSel->faceState == TEAMSEL_LOAD_IDLE || gTeamSel->stageState == TEAMSEL_LOAD_IDLE) {
                break;
            }
            continue;
        }
        if (gTeamSel->flags & TEAMSEL_LEAVING) {
            if (--gTeamSel->timer == -1) {
                /* the stage was chosen 60 frames ago: fade out and remember the choices for the next visit */
                ColorFade_StartOut(0, 0, 0, 0x14);
                gTsProgress->team[0] = *(TsTeam *)gTeamSel->side[0];
                gTsProgress->team[1] = *(TsTeam *)gTeamSel->side[1];
                gTsProgress->stageCell = gTeamSel->stage->col + gTeamSel->stage->row * TS_STAGE_COLS;
                gTsProgress->bgm = gTeamSel->bgmIds[gTeamSel->stage->bgm];
            }
            continue;
        }
        TeamSel_Input(&result);
    }

    if (result != 0) {
        s32 handicap[2] = { 1, 1 };

        if (gTeamSel->stage->stage == TS_STAGE_RANDOM) {
            do {
                gTeamSel->stage->stage = gTeamSel->stageIds[Rand_Range(gTeamSel->stageCount - 1)];
            } while (gTeamSel->stage->stage >= TS_STAGE_RANDOM);
        }
        bgm = gTeamSel->bgmIds[gTeamSel->stage->bgm];
        if (bgm == TS_BGM_RANDOM) {
            bgm = Rand_Range(9) + 8;
        }
        switch (gSaveData->rule[0]) {
        case 0:
            timeLimit = 1;
            break;
        case 1:
            timeLimit = 2;
            break;
        case 2:
            timeLimit = 3;
            break;
        case 3:
            timeLimit = 4;
            break;
        case 4:
            timeLimit = 0;
            break;
        default:
            timeLimit = 5;
            break;
        }
        handicap[0] = gSaveData->rule[3] ^ 1;
        handicap[1] = gSaveData->rule[4] ^ 1;
        cpuLevel = CpuLevel_FromSetting(gSaveData->rule[1]);
        Battle_ClearWork();
        BattleSetup_SetRule(gTeamSel->players == TEAMSEL_PLAYERS_TWO, 0, bgm, timeLimit, gSaveData->rule[2],
                            gTeamSel->stage->stage, gSaveData->rule[5] ^ 1);
        switch (gTeamSel->players) {
        case TEAMSEL_PLAYERS_VS_CPU:
            BattleSetup_SetSide(0, 0, 0, gTeamSel->side[0]->memberCount, 1, 1, 0, NULL);
            BattleSetup_SetSide(1, 2, 1, gTeamSel->side[1]->memberCount, handicap[1], 1, 0, NULL);
            break;
        case TEAMSEL_PLAYERS_TWO:
            BattleSetup_SetSide(0, 0, 0, gTeamSel->side[0]->memberCount, 1, 1, 0, NULL);
            BattleSetup_SetSide(1, 0, 1, gTeamSel->side[1]->memberCount, 1, 1, 0, NULL);
            break;
        case TEAMSEL_PLAYERS_CPU_CPU:
            BattleSetup_SetSide(0, 2, 0, gTeamSel->side[0]->memberCount, handicap[0], 1, 0, NULL);
            BattleSetup_SetSide(1, 2, 1, gTeamSel->side[1]->memberCount, handicap[1], 1, 0, NULL);
            break;
        }
        for (i = 0; i < TEAMSEL_SIDES; i++) {
            for (j = 0; j < gTeamSel->side[i]->memberCount; j++) {
                BattleSetup_SetMember(i, j, gTeamSel->side[i]->member[j].chara, gTeamSel->side[i]->member[j].color,
                                      0, cpuLevel, 100.0f, gTeamSel->side[i]->member[j].items.id);
            }
        }
        BattleSetup_Finish();
    }
    TeamSel_Term();
    Dma_ResetBuffers();
    return result;
}
