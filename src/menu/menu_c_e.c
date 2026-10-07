#include "common.h"
#include "menu/menu_d.h"
#include "sys/common.h"
#include "sys/pad.h"
#include "sys/save.h"
#ifdef PORT
#include "battle/col_c.h" /* FontStyle / FONT_ALIGN_* / FONT_SHADOW_*, for the names of added stages */
#include "plat_stages.h"  /* the manifest of added stages: their ids, names and file aliases */
#include "plat_songs.h"   /* the manifest of added songs (the music select) */
#include <stdio.h>
#include <stdlib.h>
/* include/battle/view_a.h holds these, but pulling it in clashes with menu_d.h (MsgWin_Init); the values are
   fixed, so they are repeated here for the stage list of the PC build. */
#define STGGRID_COLS 6
#define STGGRID_ID_LOCKED 0x3E
#define STGGRID_ID_EMPTY 0x3F
static s32 sPortStagesAdded; /* this stage select shows stages from outside the disc */
static s32 sPortSongNameHidden, sPortStageNameHidden;
/* The port's stage-name overlay (port/src/gs/ui.cpp / gs_draw.c): the names of added stages are drawn by the
   port, over the picture, from its own stylized art. The menu only fills the name's rectangle and which image. */
extern volatile int gUiNameX, gUiNameY, gUiNameW, gUiNameH;
extern volatile int gUiNameIdx, gUiNameReady;
extern volatile int gUiSongX, gUiSongY, gUiSongW, gUiSongH;
extern volatile int gUiSongIdx, gUiSongReady, gUiSongLit;
#endif

/*
 * CharSel, 0x342190..0x348D78: the character / stage / music select of the versus modes (progress modes 38..41
 * with archive 5, 44..45 with archive 6). One object, written in three chunks (menu_c_e.c 0x342190..0x342588,
 * menu_d.c 0x342588..0x348710, menu_e.c 0x348710..0x348D78) and merged here; layouts in include/menu/menu_d.h.
 * Its read-only data starts with the chip tables of the first functions at 0x3B38F0; its work pointer gCharSel
 * is the word at 0x3B38D4, right behind the previous object's jump table.
 *
 * Two sides (0 = left) each pick a character on a 7-column reel (optionally a form, or one of the saved custom
 * characters), an item set and a costume; then pad 0 picks the stage on a 6-column reel whose seventh column is
 * the music entry. CharSel_Run is the frame loop: it calls Init / UpdateFaceLoad / UpdateStageLoad / Update /
 * Input / Draw / Term and, when the screen ends with a choice, writes the battle setup: it is the hand-off from
 * the versus menu to the battle.
 */

CharSel *gCharSel = NULL;

extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void IconWin_SetIcon(s32 icon);
extern s32 ChrGrid_IsSelectable(MChrCell *cells, s32 index);
extern s32 ChrGrid_MoveRight(MChrCell *cells, s32 *col, s32 row);
extern s32 ChrGrid_MoveLeft(MChrCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveDown(MChrCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_MoveUp(MChrCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_NextForm(s32 *forms, s32 *index);
extern void ChrGrid_PrevForm(s32 *forms, s32 *index);
extern s32 ChrGrid_FixCursor(MChrCell *cells, s32 *col, s32 *row, s32 rows);
extern s32 StgGrid_IsSelectable(s32 *ids, s32 index);
extern void StgGrid_MoveRight(s32 *ids, s32 *col, s32 row);
extern void StgGrid_MoveLeft(s32 *ids, s32 *col, s32 row);
extern void StgGrid_MoveDown(s32 *ids, s32 *col, s32 *row, s32 rows);
extern void StgGrid_MoveUp(s32 *ids, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_Build(s32 *outCount, MChrCell *out, s32 *inCount, MChrCell *in, s32 *customCount,
                          MChrCell *custom);
extern void StgGrid_ApplyUnlocks(s32 *count, s32 *ids);
extern void BgmList_ApplyUnlocks(s32 *count, s32 *ids);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);

extern void *memcpy(void *, const void *, u32);
extern void ItemPanel_SetChara(s32 side, s32 chara, s32 index, s32 set, s32 saved);
extern void ItemPanel_Show(s32 side);
extern s32 ItemPanel_Input(s32 side, s32 pad);
extern void ItemPanel_Hide(s32 side);
extern void ItemHelp_Open(void);
extern void ItemHelp_Close(void);

/* Neighbours in the overlay: the item panel of a side (menu_g) and the item description window at 0x399240... */
extern void ItemPanel_Init(void *data, s32 side);
extern void ItemPanel_Term(s32 side);
extern void ItemPanel_Draw(s32 side);
extern void ItemPanel_Update(s32 side);
extern void ItemHelp_Init(void *data);
extern void ItemHelp_Term(void);
extern void ItemHelp_Draw(s32 arg);

/* The six chips of the stage reel show the cursor's row of the stage grid. */
void CharSel_SetStageChips(void) {
    s32 slot[6] = { 8, 11, 12, 13, 14, 15 };
    s32 i;

    for (i = 0; i < 6; i++) {
        s32 id = gCharSel->stageIds[i + gCharSel->stage->row * 6];
        MTexRes *res;

        gCharSel->stage->chip[1][i] = gCharSel->stage->chip[0][i];
        gCharSel->stage->chip[0][i] = id;
#ifdef PORT
        /* PC build: the icon pack holds one icon per stage plus the locked and empty ones (37, 38). A map added
           from outside the disc (id >= 0x24) borrows an icon; the markers keep theirs even though they moved up. */
        {
            s32 icon;
            if (id == STGGRID_ID_LOCKED) {
                icon = 37;
            } else if (id == STGGRID_ID_EMPTY) {
                icon = 38;
            } else if (id >= 0x24) {
                icon = 1 + (id - 0x24) % 36;
            } else {
                icon = id + 1;
            }
            res = (MTexRes *)MPACK_AT(gCharSel->stagePack, icon);
        }
#else
        res = (MTexRes *)MPACK_AT(gCharSel->stagePack, id + 1);
#endif
        gCharSel->tex[41 + i] = gCharSel->tex[slot[i]];
        gCharSel->tex[slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the cursor's row of the character grid. */
void CharSel_SetRowChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id = gCharSel->cells[side][i + gCharSel->side[side]->row * 7].id;
        MTexRes *res;

        gCharSel->side[side]->prevChip[i] = gCharSel->side[side]->chip[i];
        gCharSel->side[side]->chip[i] = id;
        res = (MTexRes *)MPACK_AT(gCharSel->chipPack, id + 1);
        gCharSel->sideTex[side][18 + i] = gCharSel->sideTex[side][slot[i]];
        gCharSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the forms of the grid cell under the cursor. */
void CharSel_SetCellFormChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id;
        MTexRes *res;

        if (i < gCharSel->cells[side][gCharSel->side[side]->col + gCharSel->side[side]->row * 7].formCount) {
            id = gCharSel->cells[side][gCharSel->side[side]->col + gCharSel->side[side]->row * 7].form[i];
        } else {
            id = MCHR_EMPTY;
        }
        gCharSel->side[side]->prevChip[i] = gCharSel->side[side]->chip[i];
        gCharSel->side[side]->chip[i] = id;
        res = (MTexRes *)MPACK_AT(gCharSel->chipPack, id + 1);
        gCharSel->sideTex[side][18 + i] = gCharSel->sideTex[side][slot[i]];
        gCharSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show a row of the custom-character list. */
void CharSel_SetCustomChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id = gCharSel->custom[side][i + gCharSel->side[side]->customRow * 7].id;
        MTexRes *res;

        gCharSel->side[side]->prevChip[i] = gCharSel->side[side]->chip[i];
        gCharSel->side[side]->chip[i] = id;
        res = (MTexRes *)MPACK_AT(gCharSel->chipPack, id + 1);
        gCharSel->sideTex[side][18 + i] = gCharSel->sideTex[side][slot[i]];
        gCharSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the forms of the custom cell under the cursor. */
void CharSel_SetFormChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id;
        MTexRes *res;

        if (i < gCharSel->custom[side][gCharSel->side[side]->customCol + gCharSel->side[side]->customRow * 7].formCount) {
            id = gCharSel->custom[side][gCharSel->side[side]->customCol + gCharSel->side[side]->customRow * 7].form[i];
        } else {
            id = MCHR_EMPTY;
        }
        gCharSel->side[side]->prevChip[i] = gCharSel->side[side]->chip[i];
        gCharSel->side[side]->chip[i] = id;
        res = (MTexRes *)MPACK_AT(gCharSel->chipPack, id + 1);
        gCharSel->sideTex[side][18 + i] = gCharSel->sideTex[side][slot[i]];
        gCharSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* One step of the background loader of the two portraits. */
void CharSel_UpdateFaceLoad(void) {
    MTexRes *res;

    switch (gCharSel->faceState) {
    case CHARSEL_LOAD_ABORT:
        if (gCharSel->side[gCharSel->faceSide ^ 1]->flags & CHARSEL_SIDE_FACE_CHANGE) {
            gCharSel->faceSide ^= 1;
        }
        gCharSel->faceState = CHARSEL_LOAD_RESTART;
        break;
    case CHARSEL_LOAD_RESTART:
        if (gCharSel->side[gCharSel->faceSide]->flags & CHARSEL_SIDE_FACE_CHANGE) {
            gCharSel->side[gCharSel->faceSide]->flags ^= CHARSEL_SIDE_FACE_CHANGE;
        }
        gCharSel->faceState = CHARSEL_LOAD_REQUEST;
        break;
    case CHARSEL_LOAD_REQUEST:
        File_CancelRequests();
        File_Request(gCharSel->side[gCharSel->faceSide]->chara + CHARSEL_FACE_FILE,
                     gCharSel->faceFile[gCharSel->faceSide], 0x16800);
        gCharSel->faceState = CHARSEL_LOAD_READ;
        break;
    case CHARSEL_LOAD_READ:
        if (File_UpdateRequests()) {
            gCharSel->faceState = CHARSEL_LOAD_UNPACK;
        }
        break;
    case CHARSEL_LOAD_UNPACK:
        Sprite_Unpack(gCharSel->faceFile[gCharSel->faceSide], gCharSel->faceRes[gCharSel->faceSide], NULL);
        res = gCharSel->faceRes[gCharSel->faceSide];
        Res_RelocateOffsets(&res, res, res);
        if (gCharSel->faceSide == 0) {
            gCharSel->tex[23] = MTEX(res, 0);
        } else {
            gCharSel->tex[22] = MTEX(res, 0);
        }
        gCharSel->side[gCharSel->faceSide]->flags |= CHARSEL_SIDE_FACE_READY;
        if (gCharSel->side[gCharSel->faceSide ^ 1]->flags & CHARSEL_SIDE_FACE_CHANGE) {
            gCharSel->faceSide ^= 1;
            gCharSel->faceState = CHARSEL_LOAD_RESTART;
        } else {
            gCharSel->faceState = CHARSEL_LOAD_IDLE;
        }
        break;
    case CHARSEL_LOAD_IDLE:
        if (gCharSel->stageState != CHARSEL_LOAD_IDLE) {
            break;
        }
        if (gCharSel->side[0]->flags & CHARSEL_SIDE_FACE_CHANGE) {
            gCharSel->tex[23] = NULL;
            gCharSel->faceSide = 0;
            gCharSel->faceState = CHARSEL_LOAD_RESTART;
        } else if (gCharSel->side[1]->flags & CHARSEL_SIDE_FACE_CHANGE) {
            gCharSel->tex[22] = NULL;
            gCharSel->faceSide = 1;
            gCharSel->faceState = CHARSEL_LOAD_RESTART;
        }
        break;
    }
}

/* A side's character changed: hide its portrait, or abort its load in progress, and ask for the new one. */
void CharSel_RequestFace(s32 side) {
    if (gCharSel->side[side]->flags & CHARSEL_SIDE_FACE_READY) {
        gCharSel->side[side]->flags ^= CHARSEL_SIDE_FACE_READY;
    } else if (gCharSel->faceSide == side) {
        gCharSel->faceState = CHARSEL_LOAD_ABORT;
    }
    gCharSel->side[side]->flags |= CHARSEL_SIDE_FACE_CHANGE;
}

/* One step of the background loader of the stage picture. */
void CharSel_UpdateStageLoad(void) {
    MTexRes *res;

    switch (gCharSel->stageState) {
    case CHARSEL_LOAD_ABORT:
        gCharSel->stageState = CHARSEL_LOAD_RESTART;
        break;
    case CHARSEL_LOAD_RESTART:
        if (gCharSel->flags & CHARSEL_STAGE_CHANGE) {
            gCharSel->flags ^= CHARSEL_STAGE_CHANGE;
        }
        gCharSel->stageState = CHARSEL_LOAD_REQUEST;
        break;
    case CHARSEL_LOAD_REQUEST:
        File_CancelRequests();
        File_Request(CHARSEL_STAGE_FILE_ID(gCharSel->stage->stage), gCharSel->stageFile, 0x3B800);
        gCharSel->stageState = CHARSEL_LOAD_READ;
        break;
    case CHARSEL_LOAD_READ:
        if (File_UpdateRequests()) {
            gCharSel->stageState = CHARSEL_LOAD_UNPACK;
        }
        break;
    case CHARSEL_LOAD_UNPACK:
        Sprite_Unpack(gCharSel->stageFile, gCharSel->stageRes[gCharSel->stageBuf], NULL);
        res = gCharSel->stageRes[gCharSel->stageBuf];
        Res_RelocateOffsets(&res, res, res);
        gCharSel->bg[0] = res;
        gCharSel->stageBuf ^= 1;
        gCharSel->flags |= CHARSEL_STAGE_READY;
        gCharSel->stageState = CHARSEL_LOAD_IDLE;
        break;
    case CHARSEL_LOAD_IDLE:
        if (gCharSel->faceState != CHARSEL_LOAD_IDLE) {
            break;
        }
        if (gCharSel->flags & CHARSEL_STAGE_CHANGE) {
            gCharSel->stageState = CHARSEL_LOAD_RESTART;
        }
        break;
    }
}

/* The stage under the cursor changed: start the cross fade, or abort a load in progress, and ask for the new picture. */
void CharSel_RequestStage(void) {
    if (gCharSel->flags & CHARSEL_STAGE_READY) {
        gCharSel->flags ^= CHARSEL_STAGE_READY;
    } else {
        gCharSel->stageState = CHARSEL_LOAD_ABORT;
    }
    gCharSel->flags |= CHARSEL_STAGE_CHANGE;
    gCharSel->bg[0] = gCharSel->stageRes[gCharSel->stageBuf];
    gCharSel->bg[1] = gCharSel->stageRes[gCharSel->stageBuf ^ 1];
}

/* Sends the clip of one chip or plate of a movie to a label; `kind` says which clip and from which cursor. */
void CharSel_ClipGoto(s32 flash, s32 side, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *f = &gCharSel->flash[flash];

    switch (kind) {
    case CHARSEL_CLIP_CHIP:
        sprintf(name, "mc_chara_chip_%03d", gCharSel->side[side]->col);
        break;
    case CHARSEL_CLIP_FORM_CHIP:
        sprintf(name, "mc_chara_chip_%03d", gCharSel->side[side]->form);
        break;
    case CHARSEL_CLIP_CUSTOM_PLATE:
        sprintf(name, "mc_custom_plate_%d", gCharSel->side[side]->customPlate + 1);
        break;
    case CHARSEL_CLIP_COLOR_PLATE:
        sprintf(name, "mc_color_plate_%d", gCharSel->side[side]->colorPlate + 1);
        break;
    case CHARSEL_CLIP_CUSTOM_CHIP:
        sprintf(name, "mc_chara_chip_%03d", gCharSel->side[side]->customCol);
        break;
    case CHARSEL_CLIP_STAGE_CHIP:
        if (gCharSel->stage->col == 6) {
            sprintf(name, "mc_bgm_now");
        } else {
            sprintf(name, "mc_map_chip_%02d", gCharSel->stage->col);
        }
        break;
    case 3:
    case 7:
    case 9:
        return;
    case 4:
    default:
        break;
    }
    Flash_FindLabel(f, NULL, name, &ref);
    Flash_ClipGotoLabel(f, &ref, label);
}

/* First word of section n of a pack. */
#define MPACK_WORD(pack, n) (((s32 *)(pack))[((u32 *)(pack))[n] >> 2])

#define CS_RES(pack, n) \
    res = (MTexRes *)MPACK_AT(pack, n); \
    Res_RelocateOffsets(&res, res, res)

/* Builds the screen: unpacks its section, binds the textures of the five movies, builds each side's character
   grid, the stage grid and the music list from the save's unlock bits, restores the last choice from gProgress,
   loads both portraits and the stage picture, starts the music. */
void CharSel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    s32 cols;
    s32 stageCols;
    s32 stageRows;
    void *data;

    gCharSel = Heap_Alloc(0x3A4C, 0x20, 0, 2);
    memset(gCharSel, 0, 0x3A4C);
    if (gProgress->mode == 0x2D) {
        gCharSel->pack = (u32 *)MPACK_AT(gMenuArc6, section);
        gCharSel->res = Sprite_Unpack(gCharSel->pack, NULL, NULL);
    } else {
        gCharSel->pack = (u32 *)MPACK_AT(gMenuArc5, section);
        gCharSel->res = Sprite_Unpack(gCharSel->pack, NULL, NULL);
    }
    if (gProgress->mode == 0x2D) {
        CSPROG->players = 0;
    }

    CS_RES(gCharSel->res, 47);
    gCharSel->tex[0] = MTEX(res, 0);
    CS_RES(gCharSel->res, 1);
    gCharSel->tex[17] = MTEX(res, 0);
    gCharSel->tex[16] = MTEX(res, 1);
    CS_RES(gCharSel->res, 2);
    gCharSel->tex[18] = MTEX(res, 1);
    CS_RES(gCharSel->res, 3);
    gCharSel->tex[20] = MTEX(res, 0);
    CS_RES(gCharSel->res, 4);
    gCharSel->tex[19] = MTEX(res, 0);
    CS_RES(gCharSel->res, 5);
    gCharSel->tex[9] = MTEX(res, 0);
    gCharSel->tex[40] = MTEX(res, 1);
    if (CSPROG->players == 1) {
        CS_RES(gCharSel->res, 50);
        gCharSel->tex[10] = MTEX(res, 0);
    }
    CS_RES(gCharSel->res, 6);
    gCharSel->tex[39] = MTEX(res, 0);
    CS_RES(gCharSel->res, 7);
    gCharSel->tex[38] = MTEX(res, 0);
    CS_RES(gCharSel->res, 8);
    gCharSel->tex[2] = MTEX(res, 1);
    gCharSel->tex[4] = MTEX(res, 2);
    gCharSel->tex[5] = MTEX(res, 4);
    gCharSel->tex[6] = MTEX(res, 3);
    CS_RES(gCharSel->res, 9);
    gCharSel->tex[3] = MTEX(res, 1);
    gCharSel->tex[1] = MTEX(res, 0);
    CS_RES(gCharSel->res, 10);
    gCharSel->tex[21] = MTEX(res, 0);
    CS_RES(gCharSel->res, 11);
    gCharSel->tex[24] = MTEX(res, 0);
    gCharSel->tex[26] = MTEX(res, 1);
    gCharSel->tex[25] = MTEX(res, 2);
    gCharSel->tex[27] = MTEX(res, 3);
    CS_RES(gCharSel->res, 12);
    gCharSel->tex[47] = MTEX(res, 0);
    gCharSel->tex[48] = MTEX(res, 1);
#ifdef PORT
    {
        /* PC build: the name sheets of this screen (stage names: image 39; song names: image 19, and image 20 for
           the open music list), for the letters the port draws the names of added stages and songs with
           (port/src/gs/namefont.c). Addresses as numbers; the port only reads them. */
        extern void Port_NameFontSheets(int stage, int song, int songLit);
        Port_NameFontSheets((int)(u32)gCharSel->tex[39], (int)(u32)gCharSel->tex[19], (int)(u32)gCharSel->tex[20]);
    }
#endif
    Flash_Create(&gCharSel->flash[0], MPACK_AT(gCharSel->res, 15), gCharSel->tex);
    Flash_Play(&gCharSel->flash[0], 1);

    CS_RES(gCharSel->res, 17);
    gCharSel->sideTex[0][0] = MTEX(res, 0);
    gCharSel->sideTex[0][2] = MTEX(res, 1);
    CS_RES(gCharSel->res, 18);
    gCharSel->sideTex[1][0] = MTEX(res, 0);
    gCharSel->sideTex[1][2] = MTEX(res, 1);
    CS_RES(gCharSel->res, 19);
    gCharSel->sideTex[0][1] = MTEX(res, 1);
    gCharSel->sideTex[0][4] = MTEX(res, 2);
    gCharSel->sideTex[0][5] = MTEX(res, 4);
    gCharSel->sideTex[0][6] = MTEX(res, 3);
    CS_RES(gCharSel->res, 20);
    gCharSel->sideTex[1][1] = MTEX(res, 1);
    gCharSel->sideTex[1][4] = MTEX(res, 2);
    gCharSel->sideTex[1][5] = MTEX(res, 4);
    gCharSel->sideTex[1][6] = MTEX(res, 3);
    CS_RES(gCharSel->res, 21);
    gCharSel->sideTex[0][7] = MTEX(res, 0);
    gCharSel->sideTex[1][7] = MTEX(res, 0);
    gCharSel->tex[7] = MTEX(res, 0);
    CS_RES(gCharSel->res, 22);
    gCharSel->sideTex[0][3] = MTEX(res, 0);
    gCharSel->sideTex[1][3] = MTEX(res, 0);
    CS_RES(gCharSel->res, 23);
    gCharSel->sideTex[0][10] = MTEX(res, 0);
    gCharSel->sideTex[0][17] = MTEX(res, 1);
    gCharSel->sideTex[1][10] = MTEX(res, 0);
    gCharSel->sideTex[1][17] = MTEX(res, 1);
    gCharSel->plateTex[0][4] = MTEX(res, 1);
    gCharSel->plateTex[1][4] = MTEX(res, 1);
    Flash_Create(&gCharSel->flash[1], MPACK_AT(gCharSel->res, 24), gCharSel->sideTex[0]);
    Flash_Play(&gCharSel->flash[1], 1);
    Flash_Create(&gCharSel->flash[2], MPACK_AT(gCharSel->res, 25), gCharSel->sideTex[1]);
    Flash_Play(&gCharSel->flash[2], 1);

    CS_RES(gCharSel->res, 48);
    gCharSel->plateTex[0][9] = MTEX(res, 0);
    gCharSel->plateTex[0][0] = MTEX(res, 1);
    CS_RES(gCharSel->res, 49);
    gCharSel->plateTex[1][9] = MTEX(res, 0);
    gCharSel->plateTex[1][0] = MTEX(res, 1);
    CS_RES(gCharSel->res, 26);
    gCharSel->plateTex[0][7] = MTEX(res, 0);
    gCharSel->plateTex[0][3] = MTEX(res, 1);
    CS_RES(gCharSel->res, 27);
    gCharSel->plateTex[1][7] = MTEX(res, 0);
    gCharSel->plateTex[1][3] = MTEX(res, 1);
    CS_RES(gCharSel->res, 28);
    gCharSel->plateTex[0][2] = MTEX(res, 0);
    gCharSel->plateTex[0][5] = MTEX(res, 1);
    gCharSel->plateTex[1][2] = MTEX(res, 0);
    gCharSel->plateTex[1][5] = MTEX(res, 1);
    CS_RES(gCharSel->res, 29);
    gCharSel->plateTex[0][6] = MTEX(res, 0);
    gCharSel->plateTex[0][8] = MTEX(res, 1);
    gCharSel->plateTex[1][6] = MTEX(res, 0);
    gCharSel->plateTex[1][8] = MTEX(res, 1);
    CS_RES(gCharSel->res, 30);
    gCharSel->plateTex[0][10] = MTEX(res, 0);
    gCharSel->plateTex[0][1] = MTEX(res, 1);
    CS_RES(gCharSel->res, 31);
    gCharSel->plateTex[1][10] = MTEX(res, 0);
    gCharSel->plateTex[1][1] = MTEX(res, 1);
    Flash_Create(&gCharSel->flash[3], MPACK_AT(gCharSel->res, 32), gCharSel->plateTex[0]);
    Flash_Play(&gCharSel->flash[3], 1);
    Flash_Create(&gCharSel->flash[4], MPACK_AT(gCharSel->res, 33), gCharSel->plateTex[1]);
    Flash_Play(&gCharSel->flash[4], 1);

    data = MPACK_AT(gCharSel->res, 53);
    ItemPanel_Init(data, 0);
    ItemPanel_Init(data, 1);
    ItemHelp_Init(MPACK_AT(gCharSel->res, 51));
    CS_RES(gCharSel->res, 55);
    IconWin_Init(MPACK_AT(gCharSel->res, 54), res);
    gCharSel->unk3A48 = MPACK_AT(gCommonRes->data[2], 2);

    gCharSel->nameText = MPACK_AT(gCharSel->res, 43);
    gCharSel->formText = MPACK_AT(gCharSel->res, 44);
    gCharSel->chipPack = (u32 *)MPACK_AT(gCharSel->res, 45);
    for (i = 0; i < MCHR_CELL_MAX; i++) {
        CS_RES(gCharSel->chipPack, i + 1);
    }
    gCharSel->stagePack = (u32 *)MPACK_AT(gCharSel->res, 46);
    for (i = 0; i < 38; i++) {
        CS_RES(gCharSel->stagePack, i + 1);
    }

    gCharSel->stageIds = (s32 *)(MPACK_AT(gCharSel->res, 39) + 0x10);
    gCharSel->stageCount = MPACK_WORD(gCharSel->res, 39);
    StgGrid_ApplyUnlocks(&gCharSel->stageCount, gCharSel->stageIds);
#ifdef PORT
    {
        /* PC build: append stage ids for maps added from outside the disc (BT3_EXTRA_STAGES) and, optionally, swap
           the id in a slot (BT3_STAGE_REPLACE). Their model/sound/picture come from BT3_FILE_ALIAS. The ids are
           parsed in the port layer (headless.c) and read here as globals: a getenv result must not be dereferenced
           in game code (its host pointer would be truncated to the game's 32-bit ones). The list is copied to a
           buffer of ours and padded with "empty" cells to fill the last row, because the grid is a reel of rows and
           it always reads whole rows. This runs AFTER the game has applied the save's unlocks to its own list
           (just above): the added ids are appended to the result and are never looked up in the save, so the
           save file is not touched and stays valid for a copy of the game without these stages. In an online
           session nothing is added: both players must have the same list. */
        extern int gPortStageReplaceCount, gPortReplaceOld[16], gPortReplaceNew[16];
        static s32 sAllIds[64];
        extern int Port_NetSession(void); /* port/src/gs/net.c */
        s32 extra = Port_NetSession() ? 0 : gPortExtraStageCount;
        s32 swaps = Port_NetSession() ? 0 : gPortStageReplaceCount;
        s32 n = gCharSel->stageCount, i, rows, r;
        if (n > 64) {
            n = 64;
        }
        for (i = 0; i < n; i++) {
            sAllIds[i] = gCharSel->stageIds[i];
        }
        for (i = 0; i < extra && n < 60; i++) {
            sAllIds[n++] = (s32)gPortExtraStages[i];
        }
        for (r = 0; r < swaps; r++) {
            int k;
            for (k = 0; k < n; k++) {
                if (sAllIds[k] == gPortReplaceOld[r]) {
                    sAllIds[k] = (s32)gPortReplaceNew[r];
                    break;
                }
            }
        }
        sPortStagesAdded = extra > 0 || swaps > 0;
        if (sPortStagesAdded) {
            rows = (n + STGGRID_COLS - 1) / STGGRID_COLS;
            for (i = n; i < rows * STGGRID_COLS && i < 64; i++) {
                sAllIds[i] = STGGRID_ID_EMPTY;
            }
            gCharSel->stageIds = sAllIds;
            gCharSel->stageCount = n;
        }
    }
#endif
#ifdef PORT /* PC build: list the stage ids the game loaded, to see what a data pack carries (stages report) */
    if (getenv("BT3_STAGES_DEBUG") != NULL) {
        s32 dbg;
        fprintf(stderr, "stages: count %d, ids:", (int)gCharSel->stageCount);
        for (dbg = 0; dbg < gCharSel->stageCount; dbg++) {
            fprintf(stderr, " %02x", (int)(u32)gCharSel->stageIds[dbg]);
        }
        fprintf(stderr, "\n");
    }
#endif
    gCharSel->stageRows = 6;
#ifdef PORT
    if (sPortStagesAdded) { /* (with nothing added the grid is the game's own, 6 rows) */
        gCharSel->stageRows = (gCharSel->stageCount + STGGRID_COLS - 1) / STGGRID_COLS;
    }
#endif
    gCharSel->bgmIds = (s32 *)(MPACK_AT(gCharSel->res, 56) + 0x10);
    gCharSel->bgmCount = MPACK_WORD(gCharSel->res, 56);
    BgmList_ApplyUnlocks(&gCharSel->bgmCount, gCharSel->bgmIds);
#ifdef PORT /* PC build: list the music ids the game loaded, to see the list a data pack carries (music report) */
    if (getenv("BT3_BGM_DEBUG") != NULL) {
        s32 dbg;
        fprintf(stderr, "bgm raw: count %d, ids:", (int)gCharSel->bgmCount);
        for (dbg = 0; dbg < gCharSel->bgmCount; dbg++) {
            fprintf(stderr, " %02x", (int)(u32)gCharSel->bgmIds[dbg]);
        }
        fprintf(stderr, "\n");
    }
#endif
    gCharSel->bgmCount -= 4;
    gCharSel->bgmIds[gCharSel->bgmCount - 1] = CHARSEL_BGM_RANDOM;
#ifdef PORT
    {
        /* PC build: append the tracks added from outside the disc (BT3_SONG manifest), keeping the game's
           "random" entry last. Their offsets point past the disc's own entries; their files come from the
           file layer. This runs after the game has applied the save's unlocks to its own list, and the added
           entries are not looked up in the save: nothing is written to it. In an online session nothing is
           added: both players must have the same list. */
        extern int Port_NetSession(void); /* port/src/gs/net.c */
        s32 added = Port_NetSession() ? 0 : gPortSongCount;
        if (added > 0) {
            static s32 sBgmIds[64];
            s32 n = gCharSel->bgmCount, k, last = gCharSel->bgmIds[n - 1];
            if (n > 62) {
                n = 62;
            }
            for (k = 0; k < n - 1; k++) {
                sBgmIds[k] = gCharSel->bgmIds[k];
            }
            for (k = 0; k < added && (n - 1 + k) < 63; k++) {
                sBgmIds[n - 1 + k] = gPortSongOffsets[k];
            }
            n = n - 1 + added + 1; /* the disc's songs + the added ones + the "random" entry */
            sBgmIds[n - 1] = last;          /* the "random" entry stays last */
            gCharSel->bgmIds = sBgmIds;
            gCharSel->bgmCount = n;
        }
    }
#endif
#ifdef PORT
    if (getenv("BT3_BGM_DEBUG") != NULL) {
        s32 dbg;
        fprintf(stderr, "bgm final(+added): count %d, ids:", (int)gCharSel->bgmCount);
        for (dbg = 0; dbg < gCharSel->bgmCount; dbg++) {
            fprintf(stderr, " %02x", (int)(u32)gCharSel->bgmIds[dbg]);
        }
        fprintf(stderr, "\n");
    }
#endif

    gCharSel->cells[0] = (MChrCell *)(MPACK_AT(gCharSel->res, 42) + 0x10);
    gCharSel->masterCount[0] = MPACK_WORD(gCharSel->res, 42);
    gCharSel->cells[1] = gCharSel->cells[0];
    gCharSel->masterCount[1] = gCharSel->masterCount[0];
    for (i = 0, cols = MCHR_COLS; i < CHARSEL_SIDES; i++) {
        ChrGrid_Build(&gCharSel->cellCount[i], gCharSel->grid[i], &gCharSel->masterCount[i], gCharSel->cells[i],
                      &gCharSel->customCount[i], gCharSel->custom[i]);
        gCharSel->masterCount[i] = gCharSel->cellCount[i];
        gCharSel->cells[i] = gCharSel->grid[i];
        gCharSel->rows[i] = gCharSel->masterCount[i] / cols;
        if (gCharSel->masterCount[i] % cols != 0) {
            gCharSel->rows[i]++;
        }
    }

    gCharSel->faceFile[0] = Heap_Alloc(0x16800, 0x40, 0, 2);
    gCharSel->faceFile[1] = Heap_Alloc(0x16800, 0x40, 0, 2);
    gCharSel->faceRes[0] = Heap_Alloc(0x20800, 0x20, 0, 2);
    gCharSel->faceRes[1] = Heap_Alloc(0x20800, 0x20, 0, 2);
    gCharSel->stageFile = Heap_Alloc(0x3B800, 0x40, 0, 2);
    gCharSel->stageRes[0] = Heap_Alloc(0x43000, 0x20, 0, 2);
    gCharSel->stageRes[1] = Heap_Alloc(0x43000, 0x20, 0, 2);

    gCharSel->side[0] = &gCharSel->sideData[0];
    gCharSel->side[1] = &gCharSel->sideData[1];
    gCharSel->stage = &gCharSel->stageData;
    *(CharSelPick *)gCharSel->side[0] = CSPROG->side[0].pick;
    *(CharSelPick *)gCharSel->side[1] = CSPROG->side[1].pick;
    gCharSel->players = CSPROG->players;
    stageCols = 6;
    stageRows = 6;
    gCharSel->stage->col = CSPROG->stage % stageCols;
    gCharSel->stage->row = CSPROG->stage / stageRows;
    for (i = 0; i < gCharSel->bgmCount; i++) {
        if (gCharSel->bgmIds[i] == CSPROG->bgm) {
            gCharSel->stage->bgm = i;
        }
    }
    gCharSel->faceState = CHARSEL_LOAD_IDLE;
    for (i = 0; i < CHARSEL_SIDES; i++) {
        if (!ChrGrid_IsSelectable(gCharSel->cells[i], gCharSel->side[i]->col + gCharSel->side[i]->row * 7)) {
            memset(gCharSel->side[i], 0, 0x30);
            gCharSel->side[i]->col = i;
            CSPROG->side[i].pick = *(CharSelPick *)gCharSel->side[i];
        }
    }
    if (!StgGrid_IsSelectable(gCharSel->stageIds, CSPROG->stage)) {
        CSPROG->stage = 0;
        gCharSel->stage->col = 0;
        gCharSel->stage->row = 0;
    }
    CharSel_SetRowChips(0);
    CharSel_SetRowChips(1);
    CharSel_SetStageChips();
    gCharSel->side[0]->chara = gCharSel->side[0]->chip[gCharSel->side[0]->col];
    gCharSel->side[1]->chara = gCharSel->side[1]->chip[gCharSel->side[1]->col];
    gCharSel->stage->stage = gCharSel->stageIds[gCharSel->stage->col + gCharSel->stage->row * 6];
    gCharSel->stage->bgmPlaying = gCharSel->stage->bgm;

    for (i = 0; i < CHARSEL_SIDES; i++) {
        File_LoadSync(gCharSel->side[i]->chara + CHARSEL_FACE_FILE, gCharSel->faceFile[i], 0x16800);
        Sprite_Unpack(gCharSel->faceFile[i], gCharSel->faceRes[i], NULL);
        res = gCharSel->faceRes[i];
        Res_RelocateOffsets(&res, res, res);
        gCharSel->tex[i != 0 ? 22 : 23] = MTEX(res, 0);
        gCharSel->side[i]->flags |= CHARSEL_SIDE_FACE_READY;
    }
    File_LoadSync(CHARSEL_STAGE_FILE_ID(gCharSel->stage->stage), gCharSel->stageFile, 0x3B800);
    Sprite_Unpack(gCharSel->stageFile, gCharSel->stageRes[gCharSel->stageBuf], NULL);
    res = gCharSel->stageRes[gCharSel->stageBuf];
    Res_RelocateOffsets(&res, res, res);
    gCharSel->bg[0] = res;
    gCharSel->bg[1] = res;
    gCharSel->stageState = CHARSEL_LOAD_IDLE;
    gCharSel->flags |= CHARSEL_STAGE_READY;
    gCharSel->bgAlpha[0] = 0x80;
    gCharSel->bgAlpha[1] = 0;
    gCharSel->stageBuf = 1;

    if (gCharSel->bgmIds[gCharSel->stage->bgmPlaying] == CHARSEL_BGM_RANDOM) {
        Bgm_Play(CHARSEL_BGM_RANDOM_FIRST + Rand_Range(9));
    } else {
        Bgm_Play(CHARSEL_BGM_FIRST + gCharSel->bgmIds[gCharSel->stage->bgmPlaying]);
    }
    for (i = 0; i < CHARSEL_SIDES; i++) {
        TextBox_Init(&gCharSel->nameBox[i], gCharSel->nameText, i + 1);
        TextBox_SetUnk80(&gCharSel->nameBox[i], 1);
        TextBox_Init(&gCharSel->formBox[i], gCharSel->formText, i + 3);
        TextBox_SetUnk80(&gCharSel->formBox[i], 1);
    }
}

/* Frees the screen: the windows, the movies, the picture buffers and the work area. */
void CharSel_Term(void) {
    s32 i;

#ifdef PORT
    gUiNameIdx = -1; /* drop the port's name overlays */
    gUiSongIdx = -1;
#endif
    IconWin_Term();
    ItemHelp_Term();
    ItemPanel_Term(1);
    ItemPanel_Term(0);
    for (i = 0; i < CHARSEL_FLASH_NUM; i++) {
        Flash_Destroy(&gCharSel->flash[i]);
    }
    if (gCharSel->stageRes[1] != NULL) {
        Heap_Free(gCharSel->stageRes[1]);
        gCharSel->stageRes[1] = NULL;
    }
    if (gCharSel->stageRes[0] != NULL) {
        Heap_Free(gCharSel->stageRes[0]);
        gCharSel->stageRes[0] = NULL;
    }
    if (gCharSel->stageFile != NULL) {
        Heap_Free(gCharSel->stageFile);
        gCharSel->stageFile = NULL;
    }
    if (gCharSel->faceRes[1] != NULL) {
        Heap_Free(gCharSel->faceRes[1]);
        gCharSel->faceRes[1] = NULL;
    }
    if (gCharSel->faceRes[0] != NULL) {
        Heap_Free(gCharSel->faceRes[0]);
        gCharSel->faceRes[0] = NULL;
    }
    if (gCharSel->faceFile[1] != NULL) {
        Heap_Free(gCharSel->faceFile[1]);
        gCharSel->faceFile[1] = NULL;
    }
    if (gCharSel->faceFile[0] != NULL) {
        Heap_Free(gCharSel->faceFile[0]);
        gCharSel->faceFile[0] = NULL;
    }
    if (gCharSel->res != NULL) {
        Heap_Free(gCharSel->res);
        gCharSel->res = NULL;
    }
    if (gCharSel != NULL) {
        Heap_Free(gCharSel);
        gCharSel = NULL;
    }
}

/* Draws the screen: the stage picture cross fade, then the labels, texture rectangles and visibility of every
   clip of the five movies are set from the state, the movies are drawn, then the windows and the side panels. */
void CharSel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    s32 i;
    s32 j;
    MFlash *f;

#ifdef PORT
    gUiNameIdx = -1; /* set again below only for a stage added from outside the disc */
    gUiSongIdx = -1; /* and only for a song added from outside the disc (set in the music-name block) */
#endif
    for (i = 0; i < 2; i++) {
        if (gCharSel->flags & CHARSEL_STAGE_READY) {
            switch (i) {
            case 0:
                gCharSel->bgAlpha[i] += 5;
                if (gCharSel->bgAlpha[i] >= 0x80) {
                    gCharSel->bgAlpha[i] = 0x80;
                }
                break;
            case 1:
                gCharSel->bgAlpha[i] -= 5;
                if (gCharSel->bgAlpha[i] < 0) {
                    gCharSel->bgAlpha[i] = 0;
                }
                break;
            }
        } else {
            if (i == 0) {
                gCharSel->bgAlpha[0] = 0;
                continue;
            } else if (i == 1) {
                gCharSel->bgAlpha[1] = 0x80;
            }
        }
        Sprite_DrawPicture(gCharSel->bg[i], 0, 0, gCharSel->bgAlpha[i]);
    }

    f = &gCharSel->flash[0];
    for (i = 0; i < CHARSEL_SIDES; i++) {
        Flash_FindLabel(f, NULL, i ? "mc_face_mask_r" : "mc_face_mask_l", &ref);
        if (gCharSel->faceMask != 0) {
            Flash_ClipSetFlags(f, &ref, 0x102, 1);
        } else {
            Flash_ClipSetFlags(f, &ref, 0x102, 0);
        }
        if (gCharSel->side[i]->flags & CHARSEL_SIDE_FACE_READY) {
            gCharSel->faceAlpha[i] += 0.075f;
            if (gCharSel->faceAlpha[i] >= 1.0f) {
                gCharSel->faceAlpha[i] = 1.0f;
            }
        } else {
            gCharSel->faceAlpha[i] = 0.0f;
        }
        Flash_FindLabel(f, NULL, i ? "mc_single_chara_r" : "mc_single_chara_l", &ref);
        Flash_ClipSetAlpha(f, &ref, gCharSel->faceAlpha[i]);
        Flash_ClipSetFlags(f, &ref, 0x80, (u8)gCharSel->faceMask);
        Flash_FindLabel(f, NULL, i ? "mc_name_text_r" : "mc_name_text_l", &ref);
        TextBox_AttachLine(f, &ref, 0, 0, gCharSel->side[i]->chara, &gCharSel->nameBox[i]);
        Flash_FindLabel(f, NULL, i ? "mc_form_text_r" : "mc_form_text_l", &ref);
        TextBox_AttachLine(f, &ref, 0, 0, gCharSel->side[i]->chara, &gCharSel->formBox[i]);
    }

    uv.x0 = 0;
    uv.x1 = 0x200;
    Flash_FindLabel(f, NULL, "mc_map_name", &ref);
#ifdef PORT
    {
        /* PC build: the movie's name atlas only holds the stages of the disc (0..0x23). A stage added from
           outside the disc has no name there, so the atlas clip is hidden and the name is printed with the
           game's own font at the clip's place (the font queue is flushed at the end of this draw). */
        extern void Flash_ClipGetPos(MFlash *flash, MFlashRef *ref, s32 *x, s32 *y);
        extern void Font_PrintAsciiAt(s32 x, s32 y, char *str);
        extern s32 Font_GetGlyphHeight(void);
        extern FontStyle gFontStyle;
        s32 nameId = gCharSel->stage->stage;

        if (nameId >= 0x24) {
            s32 nx = 0, ny = 0; /* (an empty clip reference leaves them untouched) */
            s32 idx = nameId - 0x24;
            Flash_ClipSetFlags(f, &ref, 2, 0); /* FLASH_PROP_VISIBLE: hide the atlas name */
            sPortStageNameHidden = 1;
            Flash_ClipGetPos(f, &ref, &nx, &ny);
            if (idx >= 0 && idx < gPortExtraStageCount) {
                if (gUiNameReady) {
                    /* The port draws the name (its own art: the menu's stylized font) over the picture. The
                       rectangle is the clip's, in the game's 512x448 pixels; the strip image is idx. */
                    gUiNameX = nx + 64;   /* centre the 384-wide strip in the 512-wide clip window */
                    gUiNameY = ny + 14;
                    gUiNameW = 0x180;
                    gUiNameH = 0x30;
                    gUiNameIdx = idx;
                } else {
                    /* No overlay: print the name with the game's own font (its plain UI face), centred on the
                       strip, in an opaque white with a drop shadow (the style is copied into the command). */
                    FontStyle saved = gFontStyle;
                    gFontStyle.align = FONT_ALIGN_CENTER;
                    gFontStyle.color = 0xFFFFFFFF;
                    gFontStyle.shadowMode = FONT_SHADOW_DROP;
                    gFontStyle.shadowColor = 0xC0000000;
                    gFontStyle.shadowDx = 1;
                    gFontStyle.shadowDy = 1;
                    Font_PrintAsciiAt(nx + 0x100, ny + (0x40 - Font_GetGlyphHeight()) / 2, gPortStageNames[idx]);
                    gFontStyle = saved;
                }
            }
        } else {
            uv.y0 = (nameId % 4) * 0x40;
            uv.y1 = uv.y0 + 0x40;
            uv.unk10 = nameId / 4;
            Flash_ClipSetUv(f, &ref, &uv);
            Flash_ClipSetTex(f, &ref, uv.unk10);
            if (sPortStageNameHidden) { /* back on a stage of the disc: its name clip was off for the added one */
                Flash_ClipSetFlags(f, &ref, 2, 1);
                sPortStageNameHidden = 0;
            }
        }
    }
#else
    uv.y0 = (gCharSel->stage->stage % 4) * 0x40;
    uv.y1 = uv.y0 + 0x40;
    uv.unk10 = gCharSel->stage->stage / 4;
    Flash_ClipSetUv(f, &ref, &uv);
    Flash_ClipSetTex(f, &ref, uv.unk10);
#endif
    Flash_FindLabel(f, NULL, "mc_map_mask", &ref);
    if (gCharSel->stage->mask != 0) {
        Flash_ClipSetFlags(f, &ref, 0x102, 1);
    } else {
        Flash_ClipSetFlags(f, &ref, 0x102, 0);
    }
    for (i = 0; i < 12; i++) {
        sprintf(name, "mc_map_chip_%02d", i);
        Flash_FindLabel(f, NULL, name, &ref);
        Flash_ClipSetFlags(f, &ref, 0x80, (u8)gCharSel->stage->mask);
    }
    for (i = 0; i < 2; i++) {
        Flash_FindLabel(f, i ? "mc_yajirusi_down" : "mc_yajirusi_up",
                        i ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
        if (gCharSel->stageRows >= 2 || gCharSel->stage->state == 9) {
            uv.x0 = i * 0x20;
            uv.x1 = i * 0x20 + 0x20;
            uv.y0 = 0x20;
            uv.y1 = 0x40;
            Flash_ClipSetUv(f, &ref, &uv);
            Flash_ClipSetFlags(f, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(f, &ref, 2, 0);
        }
    }

    uv.x0 = 0;
    uv.x1 = 0x200;
#ifdef PORT
    {
        extern void Flash_ClipGetPos(MFlash *flash, MFlashRef *ref, s32 *x, s32 *y);
        s32 bgm = gCharSel->bgmIds[gCharSel->stage->bgm];

        if (bgm >= PORT_SONG_FIRST_OFFSET) {
            s32 idx = bgm - PORT_SONG_FIRST_OFFSET;
            s32 cx, cy, px, py;
            /* Use the clip's real position: the game moves "mc_bgm_now" per screen (on the BGM select it is the
               bottom bar; on the map select it sits over the reel) and the child carries its own offset. */
            /* Which of the two name clips exists depends on the menu's state: "off" while the music line is not
               being chosen, "on" while its list is open. The position is taken from the one that is there (an
               empty reference leaves the coordinates as they were: taking "off" alone put the name at whatever
               the stack held while the list was open, and it was not seen). */
            s32 have = 0;
            cx = 0;
            cy = 0;
            Flash_FindLabel(f, "mc_bgm_now", "mc_bgm_now_text_off", &ref);
            if (ref.id >= 0) {
                Flash_ClipGetPos(f, &ref, &cx, &cy);
                have = 1;
            }
            Flash_ClipSetFlags(f, &ref, 2, 0);
            Flash_FindLabel(f, "mc_bgm_now", "mc_bgm_now_text_on", &ref);
            if (!have && ref.id >= 0) {
                Flash_ClipGetPos(f, &ref, &cx, &cy);
            }
            Flash_ClipSetFlags(f, &ref, 2, 0);
            sPortSongNameHidden = 1; /* the game's own name clips are off while an added song's name is shown */
            Flash_FindLabel(f, NULL, "mc_bgm_now", &ref);
            Flash_ClipGetPos(f, &ref, &px, &py);
            if (gUiSongReady) {
                /* The port draws the track's name (its own art) over the menu, at the clip's own place. */
                gUiSongX = px + cx;
                gUiSongY = py + cy;
                gUiSongW = 0x200;
                gUiSongH = 0x20;
                gUiSongLit = !have; /* the "on" clip: the music list is open */
                gUiSongIdx = idx;
                {
                    extern void Port_SongDebug(int state, int idx, int px, int py, int cx, int cy); /* headless.c */
                    Port_SongDebug((int)gCharSel->stage->state, (int)idx, (int)px, (int)py, (int)cx, (int)cy);
                }
            } else if (idx >= 0 && idx < gPortSongCount) {
                /* No overlay (the OpenGL back end has none yet): print the name with the game's own font. */
                extern void Font_PrintAsciiAt(s32 x, s32 y, char *str);
                extern s32 Font_GetGlyphHeight(void);
                extern FontStyle gFontStyle;
                FontStyle saved = gFontStyle;
                gFontStyle.align = FONT_ALIGN_LEFT;
                gFontStyle.color = 0xFFFFFFFF;
                gFontStyle.shadowMode = FONT_SHADOW_DROP;
                gFontStyle.shadowColor = 0xC0000000;
                Font_PrintAsciiAt(px + cx, py + cy + (0x20 - Font_GetGlyphHeight()) / 2, gPortSongNames[idx]);
                gFontStyle = saved;
            }
        } else {
            uv.y0 = (bgm % 8) * 0x20;
            uv.y1 = uv.y0 + 0x20;
            uv.unk10 = bgm / 8;
            Flash_FindLabel(f, "mc_bgm_now", "mc_bgm_now_text_off", &ref);
            Flash_ClipSetUv(f, &ref, &uv);
            Flash_ClipSetTex(f, &ref, uv.unk10);
            if (sPortSongNameHidden) {
                /* Back on a song of the disc after an added one: the clips that were switched off for the added
                   song's name come back. (The game itself never switches them; without this the names stayed
                   invisible while scrolling the list, until the menu's animation reset the clips.) */
                Flash_ClipSetFlags(f, &ref, 2, 1);
            }
            Flash_FindLabel(f, "mc_bgm_now", "mc_bgm_now_text_on", &ref);
            Flash_ClipSetUv(f, &ref, &uv);
            Flash_ClipSetTex(f, &ref, uv.unk10);
            if (sPortSongNameHidden) {
                Flash_ClipSetFlags(f, &ref, 2, 1);
                sPortSongNameHidden = 0;
            }
        }
    }
#else
    uv.y0 = (gCharSel->bgmIds[gCharSel->stage->bgm] % 8) * 0x20;
    uv.y1 = uv.y0 + 0x20;
    uv.unk10 = gCharSel->bgmIds[gCharSel->stage->bgm] / 8;
    Flash_FindLabel(f, "mc_bgm_now", "mc_bgm_now_text_off", &ref);
    Flash_ClipSetUv(f, &ref, &uv);
    Flash_ClipSetTex(f, &ref, uv.unk10);
    Flash_FindLabel(f, "mc_bgm_now", "mc_bgm_now_text_on", &ref);
    Flash_ClipSetUv(f, &ref, &uv);
    Flash_ClipSetTex(f, &ref, uv.unk10);
#endif

    for (i = 0; i < CHARSEL_SIDES; i++) {
        f = &gCharSel->flash[3 + i];
        uv.x0 = (i ^ 1) * 0x20;
        uv.x1 = uv.x0 + 0x20;
        uv.y0 = 0;
        uv.y1 = 0x20;
        Flash_FindLabel(f, NULL, "mc_yajirusi", &ref);
        Flash_ClipSetUv(f, &ref, &uv);
        for (j = 0; j < 4; j++) {
            uv.x0 = 0;
            uv.y0 = j * 0x20;
            uv.x1 = 0x100;
            uv.y1 = j * 0x20 + 0x20;
            sprintf(name, "mc_custom_plate_%d", j + 1);
            Flash_FindLabel(f, NULL, name, &ref);
            Flash_ClipSetColor(f, &ref, j < gCharSel->customRows[i] ? 1.0f : 0.3f);
            Flash_FindLabel(f, name, "mc_custom_text_off", &ref);
            Flash_ClipSetUv(f, &ref, &uv);
            Flash_FindLabel(f, name, "mc_custom_text_on", &ref);
            Flash_ClipSetUv(f, &ref, &uv);
            if (j == 0) {
                Flash_FindLabel(f, name, "mc_yajirusi", &ref);
                Flash_ClipSetFlags(f, &ref, 2, 0);
            } else {
                Flash_FindLabel(f, name, "mc_yajirusi", &ref);
                Flash_ClipSetFlags(f, &ref, 2,
                                   gCharSel->cells[i][gCharSel->side[i]->col + gCharSel->side[i]->row * 7].id != MCHR_RANDOM);
            }
        }
        for (j = 0; j < 4; j++) {
            uv.x0 = (j % 2) * 0x40;
            uv.y0 = (j / 2) * 0x20;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = uv.y0 + 0x20;
            sprintf(name, "mc_color_plate_%d", j + 1);
            Flash_FindLabel(f, NULL, name, &ref);
            if (j < gCharSel->colorCount[i]) {
                Flash_ClipSetFlags(f, &ref, 2, 1);
                switch (gCharSel->colorCount[i]) {
                case 2:
                    Flash_ClipSetOffset(f, &ref, 0x2D, 0);
                    break;
                case 3:
                    Flash_ClipSetOffset(f, &ref, 0x16, 0);
                    break;
                }
                Flash_FindLabel(f, name, "mc_color_text_off", &ref);
                Flash_ClipSetUv(f, &ref, &uv);
                Flash_FindLabel(f, name, "mc_color_text_on", &ref);
                Flash_ClipSetUv(f, &ref, &uv);
            } else {
                Flash_ClipSetFlags(f, &ref, 2, 0);
            }
        }
    }

    for (i = 0; i < CHARSEL_SIDES; i++) {
        u32 plate = 0;

        f = &gCharSel->flash[1 + i];
        switch (gCharSel->players) {
        case 0:
            plate = i == 0 ? 0 : 2;
            break;
        case 1:
            plate = i != 0;
            break;
        case 2:
            plate = 2;
            break;
        }
        uv.x0 = (plate - (plate / 2) * 2) * 0x40;
        uv.y1 = (plate / 2) * 0x20 + 0x20;
        uv.x1 = (plate - (plate / 2) * 2) * 0x40 + 0x40;
        uv.y0 = (plate / 2) * 0x20;
        Flash_FindLabel(f, NULL, "mc_plate_text", &ref);
        Flash_ClipSetUv(f, &ref, &uv);
        if (gCharSel->rows[i] >= 2) {
            for (j = 0; j < 2; j++) {
                uv.y0 = 0x20;
                uv.x1 = j * 0x20 + 0x20;
                uv.y1 = 0x40;
                uv.x0 = j * 0x20;
                Flash_FindLabel(f, j ? "mc_yajirusi_down" : "mc_yajirusi_up",
                                j ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
                Flash_ClipSetUv(f, &ref, &uv);
                Flash_FindLabel(f, NULL, j ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
                if (gCharSel->side[i]->flags & CHARSEL_SIDE_FORMS) {
                    Flash_ClipSetFlags(f, &ref, 2, 0);
                } else if (i == 1 && gCharSel->players != i) {
                    if (gCharSel->side[0]->state != 7) {
                        Flash_ClipSetFlags(f, &ref, 2, 0);
                    } else {
                        Flash_ClipSetFlags(f, &ref, 2, 1);
                    }
                } else {
                    Flash_ClipSetFlags(f, &ref, 2, 1);
                }
            }
        } else {
            for (j = 0; j < 2; j++) {
                Flash_FindLabel(f, NULL, j ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
                Flash_ClipSetFlags(f, &ref, 2, 0);
            }
        }
        Flash_FindLabel(f, NULL, "mc_chara_mask", &ref);
        if (gCharSel->side[i]->mask != 0) {
            Flash_ClipSetFlags(f, &ref, 0x102, 1);
        } else {
            Flash_ClipSetFlags(f, &ref, 0x102, 0);
        }
        for (j = 0; j < 14; j++) {
            sprintf(name, "mc_chara_chip_%03d", j);
            Flash_FindLabel(f, NULL, name, &ref);
            Flash_ClipSetFlags(f, &ref, 0x80, (u8)gCharSel->side[i]->mask);
        }
    }

    for (i = 0; i < CHARSEL_FLASH_NUM; i++) {
        Flash_Draw(&gCharSel->flash[i]);
    }
    Font_FlushAll();
    IconWin_Draw();
    ItemPanel_Draw(0);
    ItemPanel_Draw(1);
    ItemHelp_Draw(gCharSel->itemInfo);
}

/* Advances the movies and the two side panels, and drops the "hidden" marks once the movies say so. */
void CharSel_Update(void) {
    s32 i;

    for (i = 0; i < CHARSEL_FLASH_NUM; i++) {
        Flash_Advance(&gCharSel->flash[i]);
    }
    ItemPanel_Update(0);
    ItemPanel_Update(1);
    for (i = 0; i < CHARSEL_SIDES; i++) {
        if (gCharSel->side[i]->mask != 0) {
            if ((&gCharSel->flash[1])[i].trig & 1) {
                gCharSel->side[i]->mask = 0;
            }
        }
    }
    if (gCharSel->stage->mask != 0) {
        if (gCharSel->flash[0].trig & 1) {
            gCharSel->stage->mask = 0;
        }
    }
    if (gCharSel->faceMask != 0) {
        if ((s32)gCharSel->flash[0].trig < 0) {
            gCharSel->faceMask = 0;
        }
    }
}

/* The side a pad is choosing for, its number and its movies. */
#define CS_N (i + gCharSel->turn)
#define CS_SIDE (gCharSel->side[CS_N])
#define CS_REEL (&gCharSel->flash[1 + CS_N])
#define CS_PLATE (&gCharSel->flash[3 + CS_N])
#define CS_CELL (gCharSel->cells[CS_N][CS_SIDE->col + CS_SIDE->row * 7])
#define CS_CUSTOM_CELL (gCharSel->custom[CS_N][CS_SIDE->customCol + CS_SIDE->customRow * 7])

/* Handles the pads: the stage / music choice (pad 0) once both sides are done, otherwise each side's own state
   machine (grid, form reel, saved custom characters, item set, item panel, costume, done). `*running` is
   cleared when the screen is left with cancel. */
/* Integration step 11: this function only matches compiled inside the merged object (this file); in the old
   menu_d.c half it differed in 3 instructions (a `lui` in a delay slot). */
/* Not matching (as a separate menu_d.c): 3 of 3097 instructions differ, all in the cancel arm of CHARSEL_ST_FORM. Both arms of its
   `if (flags & CHARSEL_SIDE_CUSTOM)` begin with the same CharSel_ClipGoto(n + 1, n, 1, "fl_off_start"); the
   original loads the upper half of that string's address once, in front of the branch (its delay slot), and
   starts both arms with `move a1,a2`, while this compiles with `move a1,a2` in the delay slot and the `lui` at
   the start of each arm. Everything else, including every branch, is the original's code. */
void CharSel_Input(s32 *running) {
    s32 count = 0;
    s32 i;
    s32 j;
    s32 n;
    s32 id;
    s32 chara;
    s32 r;

    if (!(gCharSel->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    switch (gCharSel->players) {
    case 0:
    case 2:
        count = 1;
        break;
    case 1:
        count = 2;
        break;
    }
    if (!(gCharSel->flags & CHARSEL_STARTED)) {
        for (i = 0; i < count; i++) {
            CharSel_ClipGoto(i + 1, i, CHARSEL_CLIP_CHIP, "fl_on_start");
        }
        gCharSel->flags |= CHARSEL_STARTED;
    }

    if (gCharSel->flags & CHARSEL_STAGE_PHASE) {
        switch (gCharSel->stage->state) {
        case CHARSEL_ST_STAGE:
            if (gPad[0].gameRepeat & 1) {
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveLeft(gCharSel->stageIds, &gCharSel->stage->col, gCharSel->stage->row);
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_on_start");
                if (gCharSel->stage->col != CHARSEL_STAGE_BGM_COL) {
                    gCharSel->stage->stage = gCharSel->stageIds[gCharSel->stage->col + gCharSel->stage->row * 6];
                    CharSel_RequestStage();
                }
                Snd_PlaySe(2, 0);
            } else if (gPad[0].gameRepeat & 2) {
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveRight(gCharSel->stageIds, &gCharSel->stage->col, gCharSel->stage->row);
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_on_start");
                if (gCharSel->stage->col != CHARSEL_STAGE_BGM_COL) {
                    gCharSel->stage->stage = gCharSel->stageIds[gCharSel->stage->col + gCharSel->stage->row * 6];
                    CharSel_RequestStage();
                }
                Snd_PlaySe(2, 0);
            } else if ((gPad[0].gameRepeat & 8) && gCharSel->stage->col != CHARSEL_STAGE_BGM_COL) {
                if (gCharSel->stageRows < 2) {
                    return;
                }
                Flash_GotoLabel(&gCharSel->flash[0], "fl_reel_down", 1);
                gCharSel->stage->mask = 1;
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveUp(gCharSel->stageIds, &gCharSel->stage->col, &gCharSel->stage->row, gCharSel->stageRows);
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_on_start");
                CharSel_SetStageChips();
                gCharSel->stage->stage = gCharSel->stageIds[gCharSel->stage->col + gCharSel->stage->row * 6];
                CharSel_RequestStage();
                Snd_PlaySe(2, 2);
            } else if ((gPad[0].gameRepeat & 4) && gCharSel->stage->col != CHARSEL_STAGE_BGM_COL) {
                if (gCharSel->stageRows < 2) {
                    return;
                }
                Flash_GotoLabel(&gCharSel->flash[0], "fl_reel_up", 1);
                gCharSel->stage->mask = 1;
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveDown(gCharSel->stageIds, &gCharSel->stage->col, &gCharSel->stage->row, gCharSel->stageRows);
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_on_start");
                CharSel_SetStageChips();
                gCharSel->stage->stage = gCharSel->stageIds[gCharSel->stage->col + gCharSel->stage->row * 6];
                CharSel_RequestStage();
                Snd_PlaySe(2, 1);
            } else if (gPad[0].gamePressed & 0x200) {
                gCharSel->stage->mask = 0;
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_ok");
                if (gCharSel->stage->col == CHARSEL_STAGE_BGM_COL) {
                    Flash_GotoLabel(&gCharSel->flash[0], "fl_bgm", 1);
                    IconWin_SetIcon(1);
                    CharSel_ClipGoto(0, 0, CHARSEL_CLIP_NONE, "fl_on_start");
                    gCharSel->stage->bgm = gCharSel->stage->bgmPlaying;
                    gCharSel->stage->state = CHARSEL_ST_BGM;
                } else {
                    Flash_GotoLabel(&gCharSel->flash[0], "fl_vs", 1);
                    IconWin_Close();
                    gCharSel->faceMask = 0;
                    gCharSel->flags |= CHARSEL_LEAVING;
                    gCharSel->flags |= CHARSEL_FLAG10;
                    gCharSel->timer = 60;
                }
                Snd_PlaySe(1, 1);
            } else if (gPad[0].gamePressed & 0x400) {
                gCharSel->stage->mask = 0;
                gCharSel->flags ^= CHARSEL_STAGE_PHASE;
                Flash_GotoLabel(&gCharSel->flash[0], "fl_map_cansel", 1);
                IconWin_Close();
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_off_start");
                if (gCharSel->players != 1) {
                    Flash_GotoLabel(&gCharSel->flash[2], "fl_in", 1);
                    if (gCharSel->side[1]->flags & CHARSEL_SIDE_FORMS) {
                        CharSel_ClipGoto(2, 1, CHARSEL_CLIP_FORM_CHIP, "fl_on_start");
                        gCharSel->side[1]->state = CHARSEL_ST_FORM;
                    } else if (gCharSel->side[1]->flags & CHARSEL_SIDE_CUSTOM) {
                        CharSel_ClipGoto(2, 1, CHARSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                        gCharSel->side[1]->state = CHARSEL_ST_FORM;
                    } else {
                        CharSel_ClipGoto(2, 1, CHARSEL_CLIP_CHIP, "fl_on_start");
                        gCharSel->side[1]->state = CHARSEL_ST_GRID;
                    }
                } else {
                    for (j = 0; j < CHARSEL_SIDES; j++) {
                        Flash_GotoLabel(&gCharSel->flash[1 + j], "fl_in", 1);
                        if (gCharSel->side[j]->flags & CHARSEL_SIDE_FORMS) {
                            CharSel_ClipGoto(j + 1, j, CHARSEL_CLIP_FORM_CHIP, "fl_on_start");
                            gCharSel->side[j]->state = CHARSEL_ST_FORM;
                        } else if (gCharSel->side[j]->flags & CHARSEL_SIDE_CUSTOM) {
                            /* original: goes back to the form state, not to CHARSEL_ST_CUSTOM */
                            CharSel_ClipGoto(j + 1, j, CHARSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                            gCharSel->side[j]->state = CHARSEL_ST_FORM;
                        } else {
                            CharSel_ClipGoto(j + 1, j, CHARSEL_CLIP_CHIP, "fl_on_start");
                            gCharSel->side[j]->state = CHARSEL_ST_GRID;
                        }
                    }
                }
                Snd_PlaySe(1, 2);
            }
            break;
        case CHARSEL_ST_BGM:
            if (gPad[0].gameRepeat & 8) {
                gCharSel->stage->bgm--;
                if (gCharSel->stage->bgm < 0) {
                    gCharSel->stage->bgm = gCharSel->bgmCount - 1;
                }
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gameRepeat & 4) {
                gCharSel->stage->bgm++;
                if (gCharSel->stage->bgm > gCharSel->bgmCount - 1) {
                    gCharSel->stage->bgm = 0;
                }
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gamePressed & 0x200) {
                if (gCharSel->bgmIds[gCharSel->stage->bgm] != CHARSEL_BGM_LOCKED) {
                    Flash_GotoLabel(&gCharSel->flash[0], "fl_bgm_cansel", 1);
                    IconWin_SetIcon(0);
                    CharSel_ClipGoto(0, 0, CHARSEL_CLIP_NONE, "fl_off_start");
                    CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_on_start");
                    gCharSel->stage->bgmPlaying = gCharSel->stage->bgm;
                    if (gCharSel->bgmIds[gCharSel->stage->bgmPlaying] != CHARSEL_BGM_RANDOM) {
                        Bgm_Play(CHARSEL_BGM_FIRST + gCharSel->bgmIds[gCharSel->stage->bgmPlaying]);
                    }
                    gCharSel->stage->state = CHARSEL_ST_STAGE;
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
            } else if (gPad[0].gamePressed & 0x400) {
                Flash_GotoLabel(&gCharSel->flash[0], "fl_bgm_cansel", 1);
                IconWin_SetIcon(0);
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_NONE, "fl_off_start");
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_on_start");
                gCharSel->stage->bgm = gCharSel->stage->bgmPlaying;
                gCharSel->stage->state = CHARSEL_ST_STAGE;
                Snd_PlaySe(1, 2);
            }
            break;
        }
    } else {
    for (i = 0; i < count; i++) {
        if (gCharSel->side[CS_N ^ 1]->flags & CHARSEL_SIDE_ITEMS) {
            continue;
        }
        switch (CS_SIDE->state) {
        case CHARSEL_ST_GRID:
            if (gPad[i].gameRepeat & 1) {
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_off_start");
                ChrGrid_MoveLeft(gCharSel->cells[CS_N], &CS_SIDE->col, CS_SIDE->row);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_on_start");
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->col];
                CharSel_RequestFace(CS_N);
                Snd_PlaySe(2, 0);
            } else if (gPad[i].gameRepeat & 2) {
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_off_start");
                ChrGrid_MoveRight(gCharSel->cells[CS_N], &CS_SIDE->col, CS_SIDE->row);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_on_start");
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->col];
                CharSel_RequestFace(CS_N);
                Snd_PlaySe(2, 0);
            } else if (gPad[i].gameRepeat & 8) {
                if (gCharSel->rows[CS_N] < 2) {
                    continue;
                }
                Flash_GotoLabel(CS_REEL, "fl_reel_down", 1);
                CS_SIDE->mask = 1;
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_off_start");
                ChrGrid_MoveUp(gCharSel->cells[CS_N], &CS_SIDE->col, &CS_SIDE->row, gCharSel->rows[CS_N]);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_on_start");
                CharSel_SetRowChips(CS_N);
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->col];
                CharSel_RequestFace(CS_N);
                Snd_PlaySe(2, 2);
            } else if (gPad[i].gameRepeat & 4) {
                if (gCharSel->rows[CS_N] < 2) {
                    continue;
                }
                Flash_GotoLabel(CS_REEL, "fl_reel_up", 1);
                CS_SIDE->mask = 1;
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_off_start");
                ChrGrid_MoveDown(gCharSel->cells[CS_N], &CS_SIDE->col, &CS_SIDE->row, gCharSel->rows[CS_N]);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_on_start");
                CharSel_SetRowChips(CS_N);
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->col];
                CharSel_RequestFace(CS_N);
                Snd_PlaySe(2, 1);
            } else if (gPad[i].gamePressed & 0x200) {
                if (CS_CELL.id == MCHR_RANDOM) {
                    Flash_GotoLabel(CS_PLATE, "fl_custom_in", 1);
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_ok");
                    gCharSel->customRows[CS_N] = 4;
                    gCharSel->colorCount[CS_N] = 2;
                    if (!(CS_SIDE->colorPlate < gCharSel->colorCount[CS_N])) {
                        CS_SIDE->colorPlate = 0;
                    }
                    CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    CS_SIDE->state = CHARSEL_ST_ITEMSET;
                    Snd_PlaySe(1, 1);
                } else if (CS_CELL.id == MCHR_CUSTOM) {
                    if (gCharSel->customCount[CS_N] != 0) {
                        ChrGrid_FixCursor(gCharSel->custom[CS_N], &CS_SIDE->customCol, &CS_SIDE->customRow, 2);
                        CS_SIDE->flags |= CHARSEL_SIDE_CUSTOM;
                        Flash_GotoLabel(CS_REEL, "fl_form", 1);
                        CS_SIDE->mask = 1;
                        CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_off_start");
                        CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                        CharSel_SetCustomChips(CS_N);
                        CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->customCol];
                        CharSel_RequestFace(CS_N);
                        CS_SIDE->state = CHARSEL_ST_CUSTOM;
                        Snd_PlaySe(2, 0x29);
                    } else {
                        Snd_PlaySe(1, 7);
                    }
                } else if (CS_CELL.formCount != 0) {
                    CS_SIDE->flags |= CHARSEL_SIDE_FORMS;
                    if (!(CS_SIDE->form < CS_CELL.formCount)) {
                        CS_SIDE->form = 0;
                    }
                    Flash_GotoLabel(CS_REEL, "fl_form", 1);
                    CS_SIDE->mask = 1;
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_off_start");
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_on_start");
                    CharSel_SetCellFormChips(CS_N);
                    CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->form];
                    CharSel_RequestFace(CS_N);
                    CS_SIDE->state = CHARSEL_ST_FORM;
                    Snd_PlaySe(2, 0x29);
                } else {
                    Flash_GotoLabel(CS_PLATE, "fl_custom_in", 1);
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_ok");
                    gCharSel->customRows[CS_N] = 4;
                    gCharSel->colorCount[CS_N] = ChrTbl_WrapCostume(CS_SIDE->chara, &CS_SIDE->colorPlate);
                    CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    CS_SIDE->state = CHARSEL_ST_ITEMSET;
                    Snd_PlaySe(1, 1);
                }
            } else if (gPad[i].gamePressed & 0x400) {
                s32 turn;

                if (gCharSel->players != 1 && (turn = gCharSel->turn) != 0) {
                    if (turn == 1) {
                        CharSel_ClipGoto(i + 2, i + turn, CHARSEL_CLIP_CHIP, "fl_off_start");
                        gCharSel->turn = 0;
                        Flash_GotoLabel(CS_REEL, "fl_in", 1);
                        if (CS_SIDE->flags & CHARSEL_SIDE_FORMS) {
                            CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_on_start");
                            CS_SIDE->state = turn; /* CHARSEL_ST_FORM */
                        } else if (CS_SIDE->flags & CHARSEL_SIDE_CUSTOM) {
                            CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                            CS_SIDE->state = CHARSEL_ST_CUSTOM;
                        } else {
                            CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_on_start");
                            CS_SIDE->state = CHARSEL_ST_GRID;
                        }
                    }
                } else {
                    ColorFade_StartOut(0, 0, 0, 20);
                    *running = 0;
                }
                Snd_PlaySe(1, 2);
            }
            break;

        case CHARSEL_ST_FORM:
            if (gPad[i].gameRepeat & 1) {
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_off_start");
                ChrGrid_PrevForm(CS_SIDE->chip, &CS_SIDE->form);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_on_start");
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->form];
                CharSel_RequestFace(CS_N);
                Snd_PlaySe(2, 0);
            } else if (gPad[i].gameRepeat & 2) {
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_off_start");
                ChrGrid_NextForm(CS_SIDE->chip, &CS_SIDE->form);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_on_start");
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->form];
                CharSel_RequestFace(CS_N);
                Snd_PlaySe(2, 0);
            } else if (gPad[i].gamePressed & 0x200) {
                Flash_GotoLabel(CS_PLATE, "fl_custom_in", 1);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_ok");
                if (CS_SIDE->flags & CHARSEL_SIDE_CUSTOM) {
                    gCharSel->customRows[CS_N] = 2;
                    if (!(CS_SIDE->customPlate < gCharSel->customRows[CS_N])) {
                        CS_SIDE->customPlate = 0;
                    }
                } else {
                    gCharSel->customRows[CS_N] = 4;
                }
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                gCharSel->colorCount[CS_N] = ChrTbl_WrapCostume(CS_SIDE->chara, &CS_SIDE->colorPlate);
                CS_SIDE->state = CHARSEL_ST_ITEMSET;
                Snd_PlaySe(1, 1);
            } else if (gPad[i].gamePressed & 0x400) {
                CS_SIDE->flags ^= CHARSEL_SIDE_FORMS;
                Flash_GotoLabel(CS_REEL, "fl_form", 1);
                CS_SIDE->mask = 1;
                if (CS_SIDE->flags & CHARSEL_SIDE_CUSTOM) {
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_off_start");
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                    CharSel_SetCustomChips(CS_N);
                    CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->customCol];
                    CharSel_RequestFace(CS_N);
                    CS_SIDE->state = CHARSEL_ST_CUSTOM;
                } else {
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_off_start");
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_on_start");
                    CharSel_SetRowChips(CS_N);
                    CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->col];
                    CharSel_RequestFace(CS_N);
                    CS_SIDE->state = CHARSEL_ST_GRID;
                }
                Snd_PlaySe(2, 0x29);
            }
            break;

        case CHARSEL_ST_ITEMSET:
            if (gPad[i].gameRepeat & 8) {
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                CS_SIDE->customPlate--;
                if (CS_SIDE->customPlate < 0) {
                    CS_SIDE->customPlate = gCharSel->customRows[CS_N] - 1;
                }
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                Snd_PlaySe(1, 0);
            } else if (gPad[i].gameRepeat & 4) {
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                CS_SIDE->customPlate++;
                if (!(CS_SIDE->customPlate < gCharSel->customRows[CS_N])) {
                    CS_SIDE->customPlate = 0;
                }
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                Snd_PlaySe(1, 0);
            } else {
                    u32 pressed = gPad[i].gamePressed;
    
                    if (CS_N == 0 ? (pressed & 2) : (pressed & 1)) {
                    /* towards the middle of the screen: look at the item set */
                    if (CS_SIDE->customPlate <= 0) {
                        continue;
                    }
                    if (CS_CELL.id == MCHR_RANDOM) {
                        continue;
                    }
                    CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_ok");
                    if (CS_CELL.id == MCHR_CUSTOM) {
                        ItemPanel_SetChara(CS_N, CS_SIDE->chara, CS_SIDE->customCol + CS_SIDE->customRow * 7, 0, 1);
                    } else {
                        ItemPanel_SetChara(CS_N, CS_SIDE->chara, CS_SIDE->col + CS_SIDE->row * 7, CS_SIDE->customPlate - 1, 0);
                    }
                    ItemPanel_Show(CS_N);
                    CS_SIDE->flags |= CHARSEL_SIDE_ITEMS;
                    CS_SIDE->state = CHARSEL_ST_ITEMS;
                } else if (gPad[i].gamePressed & 0x200) {
                    Flash_GotoLabel(CS_PLATE, "fl_color_in", 1);
                    CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_ok");
                    CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_COLOR_PLATE, "fl_on_start");
                    CS_SIDE->state = CHARSEL_ST_COLOR;
                    Snd_PlaySe(1, 1);
                } else if (gPad[i].gamePressed & 0x400) {
                    Flash_GotoLabel(CS_PLATE, "fl_custom_cansel", 1);
                    CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                    if (CS_SIDE->flags & CHARSEL_SIDE_FORMS) {
                        CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_on_start");
                        CS_SIDE->state = CHARSEL_ST_FORM;
                    } else if (CS_SIDE->flags & CHARSEL_SIDE_CUSTOM) {
                        CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                        CS_SIDE->state = CHARSEL_ST_CUSTOM;
                    } else {
                        CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_on_start");
                        CS_SIDE->state = CHARSEL_ST_GRID;
                    }
                    Snd_PlaySe(1, 2);
                }
            }
            break;

        case CHARSEL_ST_ITEMS:
            r = ItemPanel_Input(CS_N, i);
            if (r > 0) {
                ItemHelp_Open();
                gCharSel->itemInfo = r - 1;
                CS_SIDE->state = CHARSEL_ST_ITEMINFO;
            } else if (r < 0) {
                ItemPanel_Hide(CS_N);
                CS_SIDE->flags &= ~CHARSEL_SIDE_ITEMS;
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                CS_SIDE->state = CHARSEL_ST_ITEMSET;
            }
            break;

        case CHARSEL_ST_ITEMINFO:
            if (gPad[i].gamePressed & 0x600) {
                ItemHelp_Close();
                CS_SIDE->state = CHARSEL_ST_ITEMS;
                Snd_PlaySe(1, 2);
            }
            break;

        case CHARSEL_ST_COLOR:
            if (gPad[i].gameRepeat & 1) {
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_COLOR_PLATE, "fl_off_start");
                CS_SIDE->colorPlate--;
                if (CS_SIDE->colorPlate < 0) {
                    CS_SIDE->colorPlate = gCharSel->colorCount[CS_N] - 1;
                }
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_COLOR_PLATE, "fl_on_start");
                Snd_PlaySe(1, 0);
            } else if (gPad[i].gameRepeat & 2) {
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_COLOR_PLATE, "fl_off_start");
                CS_SIDE->colorPlate++;
                if (!(CS_SIDE->colorPlate < gCharSel->colorCount[CS_N])) {
                    CS_SIDE->colorPlate = 0;
                }
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_COLOR_PLATE, "fl_on_start");
                Snd_PlaySe(1, 0);
            } else if (gPad[i].gamePressed & 0x200) {
                CS_SIDE->mask = 0;
                Flash_GotoLabel(CS_PLATE, "fl_color_ok", 1);
                Flash_GotoLabel(CS_REEL, "fl_out", 1);
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_COLOR_PLATE, "fl_ok");
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                if (CS_SIDE->flags & CHARSEL_SIDE_FORMS) {
                    CS_SIDE->picked = CS_SIDE->chip[CS_SIDE->form];
                    if (CS_SIDE->customPlate != 0) {
                        if (CS_CELL.id == MCHR_CUSTOM) {
                            CS_SIDE->items = CSSAVE->rec[CS_SIDE->customCol + CS_SIDE->customRow * 7].items;
                        } else {
                            CS_SIDE->items = CSSAVE->custom[CS_SIDE->col + CS_SIDE->row * 7].set[CS_SIDE->customPlate - 1];
                        }
                    } else {
                        memset(&CS_SIDE->items, 0, 0x10);
                    }
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_off_start");
                } else if (CS_SIDE->flags & CHARSEL_SIDE_CUSTOM) {
                    CS_SIDE->picked = CS_SIDE->chip[CS_SIDE->customCol];
                    if (CS_SIDE->customPlate != 0) {
                        CS_SIDE->items = CSSAVE->rec[CS_SIDE->customCol + CS_SIDE->customRow * 7].items;
                    } else {
                        memset(&CS_SIDE->items, 0, 0x10);
                    }
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                } else {
                    if (CS_CELL.id == MCHR_RANDOM) {
                        /* the random cell: draw a cell that is a character, then one of its forms */
                        for (;;) {
                            n = Rand_Range(gCharSel->masterCount[CS_N]);
                            id = gCharSel->cells[CS_N][n].id;
                            if (id < MCHR_RANDOM) {
                                chara = id;
                                break;
                            }
                        }
                        if (gCharSel->cells[CS_N][n].formCount != 0) {
                            chara = gCharSel->cells[CS_N][n].form[Rand_Range(gCharSel->cells[CS_N][n].formCount)];
                        }
                        CS_SIDE->picked = chara;
                    } else {
                        CS_SIDE->picked = CS_SIDE->chip[CS_SIDE->col];
                        n = CS_SIDE->col + CS_SIDE->row * 7;
                    }
                    if (CS_SIDE->customPlate != 0) {
                        CS_SIDE->items = CSSAVE->custom[n].set[CS_SIDE->customPlate - 1];
                    } else {
                        memset(&CS_SIDE->items, 0, 0x10);
                    }
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_off_start");
                }
                CS_SIDE->state = CHARSEL_ST_DONE;
                if (gCharSel->players != 1 && gCharSel->turn == 0) {
                    /* one pad: now choose for the other side */
                    gCharSel->turn = 1;
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_on_start");
                    CS_SIDE->state = CHARSEL_ST_GRID;
                }
                Snd_PlaySe(1, 1);
            } else if (gPad[i].gamePressed & 0x400) {
                Flash_GotoLabel(CS_PLATE, "fl_color_cansel", 1);
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_COLOR_PLATE, "fl_off_start");
                CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                CS_SIDE->state = CHARSEL_ST_ITEMSET;
                Snd_PlaySe(1, 2);
            }
            break;

        case CHARSEL_ST_CUSTOM:
            if (gPad[i].gameRepeat & 1) {
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                ChrGrid_MoveLeft(gCharSel->custom[CS_N], &CS_SIDE->customCol, CS_SIDE->customRow);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->customCol];
                CharSel_RequestFace(CS_N);
                Snd_PlaySe(2, 0);
            } else if (gPad[i].gameRepeat & 2) {
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                ChrGrid_MoveRight(gCharSel->custom[CS_N], &CS_SIDE->customCol, CS_SIDE->customRow);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->customCol];
                CharSel_RequestFace(CS_N);
                Snd_PlaySe(2, 0);
            } else if (gPad[i].gameRepeat & 8) {
                Flash_GotoLabel(CS_REEL, "fl_reel_down", 1);
                CS_SIDE->mask = 1;
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                ChrGrid_MoveUp(gCharSel->custom[CS_N], &CS_SIDE->customCol, &CS_SIDE->customRow, 2);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                CharSel_SetCustomChips(CS_N);
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->customCol];
                CharSel_RequestFace(CS_N);
                Snd_PlaySe(2, 2);
            } else if (gPad[i].gameRepeat & 4) {
                Flash_GotoLabel(CS_REEL, "fl_reel_up", 1);
                CS_SIDE->mask = 1;
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                ChrGrid_MoveDown(gCharSel->custom[CS_N], &CS_SIDE->customCol, &CS_SIDE->customRow, 2);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                CharSel_SetCustomChips(CS_N);
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->customCol];
                CharSel_RequestFace(CS_N);
                Snd_PlaySe(2, 1);
            } else if (gPad[i].gamePressed & 0x200) {
                if (CS_CUSTOM_CELL.formCount != 0) {
                    CS_SIDE->flags |= CHARSEL_SIDE_FORMS;
                    if (!(CS_SIDE->form < CS_CUSTOM_CELL.formCount)) {
                        CS_SIDE->form = 0;
                    }
                    Flash_GotoLabel(CS_REEL, "fl_form", 1);
                    CS_SIDE->mask = 1;
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_FORM_CHIP, "fl_on_start");
                    CharSel_SetFormChips(CS_N);
                    CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->form];
                    CharSel_RequestFace(CS_N);
                    CS_SIDE->state = CHARSEL_ST_FORM;
                    Snd_PlaySe(2, 0x29);
                } else {
                    Flash_GotoLabel(CS_PLATE, "fl_custom_in", 1);
                    CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_ok");
                    gCharSel->customRows[CS_N] = 2;
                    if (!(CS_SIDE->customPlate < gCharSel->customRows[CS_N])) {
                        CS_SIDE->customPlate = 0;
                    }
                    CharSel_ClipGoto(CS_N + 3, CS_N, CHARSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    gCharSel->colorCount[CS_N] = ChrTbl_WrapCostume(CS_SIDE->chara, &CS_SIDE->colorPlate);
                    CS_SIDE->state = CHARSEL_ST_ITEMSET;
                    Snd_PlaySe(1, 1);
                }
            } else if (gPad[i].gamePressed & 0x400) {
                CS_SIDE->flags ^= CHARSEL_SIDE_CUSTOM;
                Flash_GotoLabel(CS_REEL, "fl_form", 1);
                CS_SIDE->mask = 1;
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_on_start");
                CharSel_SetRowChips(CS_N);
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->col];
                CharSel_RequestFace(CS_N);
                CS_SIDE->state = CHARSEL_ST_GRID;
                Snd_PlaySe(2, 0x29);
            }
            break;

        case CHARSEL_ST_DONE:
            if (gCharSel->flags & CHARSEL_STAGE_PHASE) {
                continue;
            }
            if (gCharSel->side[CS_N ^ 1]->state == CHARSEL_ST_DONE) {
                /* both sides are done: on to the stage */
                Flash_GotoLabel(&gCharSel->flash[0], "fl_map", 1);
                IconWin_Open();
                CharSel_ClipGoto(0, 0, CHARSEL_CLIP_STAGE_CHIP, "fl_on_start");
                gCharSel->flags |= CHARSEL_STAGE_PHASE;
                gCharSel->stage->state = CHARSEL_ST_STAGE;
                gCharSel->faceMask = 1;
            } else if (gPad[i].gamePressed & 0x400) {
                Flash_GotoLabel(CS_REEL, "fl_in", 1);
                CharSel_ClipGoto(CS_N + 1, CS_N, CHARSEL_CLIP_CHIP, "fl_on_start");
                CharSel_SetRowChips(CS_N);
                CS_SIDE->chara = CS_SIDE->chip[CS_SIDE->col];
                CharSel_RequestFace(CS_N);
                CS_SIDE->state = CHARSEL_ST_GRID;
                Snd_PlaySe(1, 2);
            }
            break;
        }
    }
    }
}

/* ---- the frame loop (0x348710) ---- */

extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                                void *bits);
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                                  void *items);
extern void BattleSetup_Finish(void);
extern s32 CpuLevel_FromSetting(u32 setting);

/* Runs the screen. Returns 0 when it was cancelled, otherwise non-zero with the battle set up. */
s32 CharSel_Run(s32 section) {
    s32 result = 1;
    s32 screenMode;
    s32 bgm;
    s32 timeLimit;
    s32 announcer;
    s32 stage;
    s32 unk10;
    s32 cpuLevel;
    s32 i;

    CharSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        CharSel_UpdateFaceLoad();
        CharSel_UpdateStageLoad();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            CharSel_Update();
        }
        CharSel_Draw();
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
            if (gCharSel->faceState == CHARSEL_LOAD_IDLE || gCharSel->stageState == CHARSEL_LOAD_IDLE) {
                break;
            }
            continue;
        }
        if (gCharSel->flags & CHARSEL_FLAG10) {
            if (--gCharSel->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                CSPROG->side[0].pick = *(CharSelPick *)gCharSel->side[0];
                CSPROG->side[1].pick = *(CharSelPick *)gCharSel->side[1];
                CSPROG->stage = gCharSel->stage->col + gCharSel->stage->row * CHARSEL_STAGE_COLS;
                CSPROG->bgm = gCharSel->bgmIds[gCharSel->stage->bgmPlaying];
            }
        } else {
            CharSel_Input(&result);
        }
    }
    if (result != 0) {
        s32 flag[2] = { 1, 1 };
        s32 mode;

        if (gCharSel->stage->stage == CHARSEL_STAGE_RANDOM) {
            do {
                gCharSel->stage->stage = gCharSel->stageIds[Rand_Range(gCharSel->stageCount - 1)];
            } while (gCharSel->stage->stage >= CHARSEL_STAGE_RANDOM);
        }
        if (gProgress->mode == 0x2D) {
            mode = 6;
            bgm = gCharSel->bgmIds[gCharSel->stage->bgmPlaying];
            screenMode = 0;
            if (bgm == CHARSEL_BGM_RANDOM) {
                bgm = Rand_Range(9) + 8;
            }
            announcer = 1;
            timeLimit = 0;
            unk10 = 0;
            stage = gCharSel->stage->stage;
            cpuLevel = -1;
            flag[0] = 1;
            flag[1] = 1;
        } else {
            mode = 0;
            bgm = gCharSel->bgmIds[gCharSel->stage->bgmPlaying];
            screenMode = gCharSel->players == 1;
            if (bgm == CHARSEL_BGM_RANDOM) {
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
            stage = gCharSel->stage->stage;
            unk10 = gSaveData->rule[5] ^ 1;
            announcer = gSaveData->rule[2];
            cpuLevel = CpuLevel_FromSetting(gSaveData->rule[1]);
            flag[0] = gSaveData->rule[3] ^ 1;
            flag[1] = gSaveData->rule[4] ^ 1;
        }
        Battle_ClearWork();
        BattleSetup_SetRule(screenMode, mode, bgm, timeLimit, announcer, stage, unk10);
        switch (gCharSel->players) {
        case 0:
            BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
            BattleSetup_SetSide(1, 2, 1, 1, flag[1], 1, 0, NULL);
            break;
        case 1:
            BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
            BattleSetup_SetSide(1, 0, 1, 1, 1, 1, 0, NULL);
            break;
        case 2:
            BattleSetup_SetSide(0, 2, 0, 1, flag[0], 1, 0, NULL);
            BattleSetup_SetSide(1, 2, 1, 1, flag[1], 1, 0, NULL);
            break;
        }
        for (i = 0; i < 2; i++) {
            BattleSetup_SetMember(i, 0, gCharSel->side[i]->picked, gCharSel->side[i]->colorPlate, 0, cpuLevel, 100.0f,
                                  gCharSel->side[i]->items.id);
        }
        BattleSetup_Finish();
    }
    CharSel_Term();
    Dma_ResetBuffers();
    return result;
}
