#include "common.h"
#include "menu/menu_u.h"

/* The customising screen's work pointer (0x3BB140): this object owns it. */
UEvoZ *gEvoZ = NULL;

/*
 * EvoZ, 0x392F10..0x393C58: the character customising screen (gProgress->mode 49; the callers' debug strings
 * call the mode's files "ez_"): work area, frame loop and the conversion of Z points into experience. Read-only
 * data 0x3BB150..0x3BB180 (two strings). The work pointer gEvoZ is the first word of the group's `.data`
 * (0x3BB140). Names are guesses from what the code does.
 */

/* Allocates the work area, loads the pack, builds the character grid and the item lists. */
void EvoZ_Init(s32 section) {
    MTexRes *res = 0;
    s32 i;

    gEvoZ = Heap_Alloc(sizeof(UEvoZ), 0x20, 0, 2);
    memset(gEvoZ, 0, sizeof(UEvoZ));
    EvoZ_Load(gEvoZ, section);
    ChrGrid_Build(&gEvoZ->cellCount, gEvoZ->cells, &gEvoZ->gridCount, gEvoZ->grid, 0, 0);
    gEvoZ->gridCount = gEvoZ->cellCount;
    gEvoZ->grid = gEvoZ->cells;
    gEvoZ->rows = gEvoZ->gridCount / 7;
    if (gEvoZ->gridCount % 7 != 0) {
        gEvoZ->rows++;
    }
    gEvoZ->faceFile = Heap_Alloc(0x16800, 0x40, 0, 2);
    gEvoZ->faceRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    gEvoZ->side = &gEvoZ->sideRec;
    gEvoZ->side->pick = UPROG->pick;
    if (!ChrGrid_IsSelectable(gEvoZ->grid, gEvoZ->side->pick.col + gEvoZ->side->pick.row * 7)) {
        memset(gEvoZ->side, 0, sizeof(UPick));
        gEvoZ->side->pick.col = 0;
        UPROG->pick = gEvoZ->side->pick;
    }
    gEvoZ->faceState = 4;
    EvoZ_SetRowChips(gEvoZ);
    gEvoZ->side->chara = gEvoZ->side->chip[gEvoZ->side->pick.col];
    EvoZ_RefreshStatus(gEvoZ);
    for (i = 0; i < UEVOZ_ITEM_MAX; i++) {
        if (!(gEvoZ->items[i].flags & 5) && gEvoZ->items[i].type < 3 && (gEvoZ->items[i].flags & 0x40)) {
            if (gEvoZ->items[i].type != 2) {
                gEvoZ->list.ids[0][gEvoZ->list.count[0]] = i;
                gEvoZ->list.count[0]++;
            }
            gEvoZ->list.ids[gEvoZ->items[i].type + 1][gEvoZ->list.count[gEvoZ->items[i].type + 1]] = i;
            gEvoZ->list.count[gEvoZ->items[i].type + 1]++;
        }
    }
    gEvoZ->list.rows = 6;
    gEvoZ->list.rowsF = 6.16f;
    for (i = 0; i < UEVOZ_TABS; i++) {
        gEvoZ->list.cur[i] = 0;
        gEvoZ->list.top[i] = gEvoZ->list.cur[i];
        gEvoZ->list.bottom[i] = gEvoZ->list.cur[i] + gEvoZ->list.rows - 1;
    }
    File_LoadSync(gEvoZ->side->chara + 0x2F9, gEvoZ->faceFile, 0x16800);
    Sprite_Unpack(gEvoZ->faceFile, gEvoZ->faceRes, 0);
    res = gEvoZ->faceRes;
    Res_RelocateOffsets(&res, res, res);
    gEvoZ->tex0[42] = res->tex;
    gEvoZ->side->flags |= 2;
    for (i = 0; i < UEVOZ_BOX_NUM; i++) {
        MTextBox *box = &gEvoZ->box[i];

        if (i <= 0) {
            TextBox_Init(box, gEvoZ->text[0], 1);
        } else if (i < 2) {
            TextBox_Init(box, gEvoZ->text[1], 3);
        } else if (i < 3) {
            TextBox_Init(box, gEvoZ->text[2], 6);
            TextBox_SetUnk80(box, 1);
        } else if (i < 10) {
            TextBox_Init(box, gEvoZ->text[2], 6);
            TextBox_SetUnk80(box, 1);
            TextBox_SetRect(box, 0, 0x200, 0xC6, 0x198);
        } else {
            TextBox_Init(box, gEvoZ->text[2], 6);
            TextBox_SetUnk80(box, 1);
        }
    }
}

/* Frees everything Init and Load made. */
void EvoZ_Term(void) {
    s32 i;

    PassWin_Term();
    Dialog_Term();
    ItemHelp_Term();
    for (i = 0; i < UEVOZ_FLASH_NUM; i++) {
        Flash_Destroy(&gEvoZ->flash[i]);
    }
    if (gEvoZ->chipRes != 0) {
        Heap_Free(gEvoZ->chipRes);
        gEvoZ->chipRes = 0;
    }
    if (gEvoZ->chipFile != 0) {
        Heap_Free(gEvoZ->chipFile);
        gEvoZ->chipFile = 0;
    }
    if (gEvoZ->faceRes != 0) {
        Heap_Free(gEvoZ->faceRes);
        gEvoZ->faceRes = 0;
    }
    if (gEvoZ->faceFile != 0) {
        Heap_Free(gEvoZ->faceFile);
        gEvoZ->faceFile = 0;
    }
    if (gEvoZ->res != 0) {
        Heap_Free(gEvoZ->res);
        gEvoZ->res = 0;
    }
    if (gEvoZ != 0) {
        Heap_Free(gEvoZ);
        gEvoZ = 0;
    }
}

/* Draws the background, the three movies, the text and the item details page. */
void EvoZ_Draw(void) {
    s32 i;

    Sprite_DrawPicture(gEvoZ->bg, 0, 0, 0x80);
    EvoZ_Refresh(gEvoZ);
    for (i = 0; i < UEVOZ_FLASH_NUM; i++) {
        if (i != 1 || !(gEvoZ->flags & UEVOZ_REEL_READY)) {
            Flash_Draw(&gEvoZ->flash[i]);
        }
    }
    Font_FlushAll();
    PassWin_Draw();
    ItemHelp_Draw(gEvoZ->helpItem);
}

/* Advances the movies and turns Z points into experience, 33 a frame. */
void EvoZ_Update(void) {
    MFlashRef ref;
    s32 i;

    for (i = 0; i < UEVOZ_FLASH_NUM; i++) {
        if (i != 1 || !(gEvoZ->flags & UEVOZ_REEL_READY)) {
            Flash_Advance(&gEvoZ->flash[i]);
        }
    }
    if (gEvoZ->side->mask != 0 && (gEvoZ->flash[1].trig & 1)) {
        gEvoZ->side->mask = 0;
    }
    if ((gEvoZ->flags & UEVOZ_LIST_BUSY) && (gEvoZ->flash[2].trig & 1)) {
        EvoZ_ToggleList(gEvoZ);
    }
    if (!(gEvoZ->flags & UEVOZ_REEL_READY) && (gEvoZ->flash[1].trig & 2)) {
        gEvoZ->flags |= UEVOZ_REEL_READY;
    }
    if (gEvoZ->flags & UEVOZ_PAYING) {
        if (gEvoZ->pay > 33) {
            gEvoZ->pay -= 33;
            gSaveData->money -= 33;
            SAVE_CUSTOM_AT(gSaveData, gEvoZ->side->pick.col + gEvoZ->side->pick.row * 7).exp += 33;
            gEvoZ->exp = SAVE_CUSTOM_AT(gSaveData, gEvoZ->side->pick.col + gEvoZ->side->pick.row * 7).exp;
        } else {
            gSaveData->money -= gEvoZ->pay;
            SAVE_CUSTOM_AT(gSaveData, gEvoZ->side->pick.col + gEvoZ->side->pick.row * 7).exp += gEvoZ->pay;
            gEvoZ->exp = SAVE_CUSTOM_AT(gSaveData, gEvoZ->side->pick.col + gEvoZ->side->pick.row * 7).exp;
            gEvoZ->pay = 0;
            gEvoZ->expLeft = ChrTbl_GetMaxExp(gEvoZ->side->chara) - gEvoZ->exp;
            gEvoZ->flags ^= UEVOZ_PAYING;
            gEvoZ->state = 5;
        }
        if (gEvoZ->exp >= gEvoZ->nextExp) {
            Flash_FindLabel(&gEvoZ->flash[0], 0, "mc_ability_limit_up", &ref);
            Flash_ClipGotoLabel(&gEvoZ->flash[0], &ref, "fl_ability_limit_up_in");
            gEvoZ->flags |= UEVOZ_LIMIT_UP;
            gEvoZ->state = 7;
            SAVE_CUSTOM_AT(gSaveData, gEvoZ->side->pick.col + gEvoZ->side->pick.row * 7).level++;
            EvoZ_RefreshStatus(gEvoZ);
            Snd_PlaySe(2, 0x2F);
        } else {
            s32 base = EvoZ_GetLevelExp(gEvoZ, gEvoZ->side->chara, gEvoZ->side->pick.col + gEvoZ->side->pick.row * 7);

            gEvoZ->expBar = (f32)(gEvoZ->exp - base) / (f32)(gEvoZ->nextExp - base) * 6.66f;
        }
        if (--gEvoZ->seTimer == -1) {
            Snd_PlaySe(2, 5);
            gEvoZ->seTimer = 10;
        }
    }
    if ((gEvoZ->flags & UEVOZ_LIMIT_UP) && (gEvoZ->flash[0].flags & 8)) {
        gEvoZ->flags ^= UEVOZ_LIMIT_UP;
        if (gEvoZ->flags & UEVOZ_PAYING) {
            gEvoZ->state = 6;
        } else {
            gEvoZ->state = 5;
        }
    }
}

/* Mode 49: runs the screen until it is left. */
s32 EvoZ_Run(s32 section) {
    s32 result = 1;

    EvoZ_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        EvoZ_UpdateFace(gEvoZ);
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(UPROG->flags & 0x100)) {
            EvoZ_Update();
        }
        EvoZ_Draw();
        EvoZ_UpdateDialog(gEvoZ);
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (UPROG->flags & 0x100) {
            continue;
        }
        ColorFade_IsInDone();
        if (ColorFade_IsFadingOut()) {
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gEvoZ->faceState != 4) {
                continue;
            }
            break;
        }
        if (gEvoZ->flags & UEVOZ_LEAVING) {
            if (--gEvoZ->leaveTimer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gEvoZ->dialogState == 0 && !(gEvoZ->flags & UEVOZ_LIST_BUSY)) {
            EvoZ_Input(gEvoZ, &result);
        }
    }
    EvoZ_Term();
    Dma_ResetBuffers();
    return result;
}
