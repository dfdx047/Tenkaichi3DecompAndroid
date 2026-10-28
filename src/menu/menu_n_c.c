#include "common.h"
#include "menu/menu_n.h"
#include "sys/pad.h"

/*
 * UbRank, 0x373A68..0x3760C8: the ranking ladder of mode 26. A list of 100 places (five plates visible); the
 * player holds place gSaveData->rank + 1 and may challenge a place; challenging the place above can bring in an
 * intruder instead. Back from the battle the screen shows the outcome and moves the player up.
 * One object: its .data is the work pointer 0x3B7354, its .rodata runs from 0x3B7C70 (sUbRankChipTex) to
 * 0x3B7E50.
 */

UbRank *gUbRank = NULL; /* 0x3B7354 */

/*
 * As in menu_n_b.c, this file saw Snd_PlaySe as a function returning a value (UbRank_Input's cancel branch keeps
 * $v0 reserved behind the call).
 */
#define Snd_PlaySe ((s32 (*)(u32, s32))Snd_PlaySe)

/* Movie image records that show the small pictures of the six plates' holders. */
static const s32 sUbRankChipTex[6] = { 13, 15, 16, 17, 18, 19 };

#define UR_RES(n) \
    res = (MTexRes *)MPACK_AT(gUbRank->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* The texture list of a character's chip in the chip pack. */
#define UR_CHIP_RES(chara) ((MTexRes *)MPACK_AT(gUbRank->chips, (chara) + 1))

/* The battle hand-off of the ladder: the player's fighter against the opponent chosen by UbRank_PickFoe. */
void UbRank_SetupBattle(void) {
    u16 items[8];
    NRankRule *rule = gUbRank->curRule;
    s32 unk14 = rule->unk14 != 0;
    s32 unk4 = rule->unk4 != 0;
    s32 announcer = rule->announcer;
    s32 stage = rule->stage;
    s32 bgm = rule->bgm;
    s32 timeLimit = rule->timeLimit;
    f32 health;
    s32 n;
    s32 j;

    if (announcer == N_RANDOM) {
        announcer = Rand_Range(8);
    }
    if (stage == N_RANDOM) {
        stage = gUbRank->stageList[Rand_Range(0x1C)];
    }
    if (bgm == N_RANDOM) {
        bgm = gUbRank->bgmList[Rand_Range(0xB)];
    }
    memset(items, 0, sizeof(items));
    n = 0;
    for (j = 0; j < 7; j++) {
        s32 item = gUbRank->curFoe->item[j];

        if (item != N_NONE) {
            items[n++] = item + 1;
        }
    }
    if (gUbRank->curFoe->lastItem == N_NONE) {
        items[7] = 0;
    } else {
        items[7] = gUbRank->curFoe->lastItem + 1;
    }
    Battle_ClearWork();
    health = 100.0f;
    BattleSetup_SetRule(0, 2, bgm, timeLimit, announcer, stage, unk4);
    BattleSetup_SetSide(0, 0, 0, 1, 1, 0, 0, NULL);
    BattleSetup_SetSide(1, 2, 1, 1, unk14, 0, 0, NULL);
    BattleSetup_SetMember(0, 0, NPROG->chara, NPROG->color, 0, 0, health, NPROG->items);
    BattleSetup_SetMember(1, 0, gUbRank->curFoe->chara, gUbRank->curFoe->color, 0, gUbRank->curFoe->cpuLevel, health,
                          items);
    BattleSetup_Finish();
}

/* The rule entry of a place: the places below the player's own are shifted by one (the player's is left out). */
s32 UbRank_PlaceToRule(s32 place) {
    if (gSaveData->rank < place) {
        return place - 1;
    }
    return place;
}

/* Makes entry idx of the place table (or of the intruder table) the opponent. */
void UbRank_SetFoe(s32 idx, s32 intruder) {
    if (!intruder) {
        gUbRank->curRule = &gUbRank->rule[idx];
        gUbRank->curFoe = &gUbRank->foe[gUbRank->curRule->foe];
        gUbRank->isIntruder = 0;
    } else {
        gUbRank->curRule = &gUbRank->intruderRule[idx];
        gUbRank->curFoe = &gUbRank->intruderFoe[gUbRank->curRule->foe];
        gUbRank->isIntruder = 1;
    }
}

/*
 * Chooses the opponent for a challenge of `place`: its holder, or, one time in ten when the place is the tenth
 * or lower and above the player's own, one of the 37 intruders. Returns 1 for an intruder.
 */
s32 UbRank_PickFoe(s32 place) {
    s32 intrude = (s32)Rand_Range(0xFFFF) < 0x199A;

    /* the one-pass loop keeps the compiler from moving the comparison down to its use, as the original has it */
    do {
        if (place < 10 || !intrude || gSaveData->rank < place) {
            UbRank_SetFoe(UbRank_PlaceToRule(place), 0);
            NPROG->ubFlags &= ~NPROG_UB_INTRUDER;
            return 0;
        }
    } while (0);
    UbRank_SetFoe(Rand_Range(0x25), 1);
    NPROG->ubFlags |= NPROG_UB_INTRUDER;
    return 1;
}

/* Scrolls the list one place towards first place; returns 0 at the top. */
s32 UbRank_ScrollUp(void) {
    if (gUbRank->top != 0) {
        gUbRank->top--;
        Flash_GotoLabel(&gUbRank->flash[0], "fl_ranking_down", 1);
        Snd_PlaySe(1, 0);
        gUbRank->extra = gUbRank->top + 5;
        return 1;
    }
    return 0;
}

/* Loads the screen's file, creates the three movies, and takes over the outcome of a ladder battle. */
void UbRank_Init(void) {
    MTexRes *res = NULL;
    s32 i;

    gUbRank = Heap_Alloc(sizeof(UbRank), 0x20, 0, 2);
    memset(gUbRank, 0, sizeof(UbRank));
    gUbRank->file = File_LoadSync(gProgress->baseFile + 0x1B, NULL, 0);
    gUbRank->res = Sprite_Unpack(gUbRank->file, NULL, NULL);
    gUbRank->faceFile[0] = Heap_Alloc(0x16800, 0x40, 0, 2);
    gUbRank->faceFile[1] = Heap_Alloc(0x16800, 0x40, 0, 2);
    gUbRank->faceRes[0] = Heap_Alloc(0x20800, 0x20, 0, 2);
    gUbRank->faceRes[1] = Heap_Alloc(0x20800, 0x20, 0, 2);
    gUbRank->rule = (NRankRule *)MPACK_AT(gUbRank->res, 8);
    gUbRank->intruderRule = (NRankRule *)MPACK_AT(gUbRank->res, 9);
    gUbRank->foe = (NFoe *)MPACK_AT(gUbRank->res, 10);
    gUbRank->intruderFoe = (NFoe *)MPACK_AT(gUbRank->res, 11);
    gUbRank->stageList = (s32 *)MPACK_AT(gUbRank->res, 12);
    gUbRank->bgmList = (s32 *)MPACK_AT(gUbRank->res, 13);
    UR_RES(4);
    gUbRank->bg = res;
    UR_RES(5);
    gUbRank->tex[31] = MTEX(res, 0);
    gUbRank->tex[32] = MTEX(res, 1);
    gUbRank->tex[33] = MTEX(res, 3);
    UR_RES(3);
    gUbRank->tex[0] = MTEX(res, 0);
    gUbRank->tex[1] = MTEX(res, 1);
    gUbRank->tex[25] = MTEX(res, 2);
    gUbRank->tex[26] = MTEX(res, 3);
    gUbRank->tex[27] = MTEX(res, 4);
    gUbRank->tex[28] = MTEX(res, 5);
    gUbRank->tex[29] = MTEX(res, 6);
    gUbRank->tex[8] = MTEX(res, 7);
    gUbRank->tex[22] = MTEX(res, 8);
    gUbRank->tex[21] = MTEX(res, 9);
    gUbRank->tex[24] = MTEX(res, 10);
    gUbRank->tex[12] = MTEX(res, 11);
    gUbRank->tex[14] = MTEX(res, 12);
    gUbRank->tex[11] = MTEX(res, 13);
    gUbRank->tex[10] = MTEX(res, 14);
    gUbRank->tex[20] = MTEX(res, 15);
    gUbRank->tex[7] = MTEX(res, 16);
    gUbRank->tex[3] = MTEX(res, 17);
    gUbRank->tex[2] = MTEX(res, 18);
    gUbRank->tex[30] = MTEX(res, 19);
    gUbRank->tex[4] = MTEX(res, 22);
    gUbRank->tex[5] = MTEX(res, 23);
    gUbRank->tex[6] = MTEX(res, 24);
    gUbRank->tex[23] = MTEX(res, 25);
    gUbRank->tex[34] = MTEX(res, 27);
    gUbRank->tex[35] = MTEX(res, 28);
    gUbRank->tex[36] = MTEX(res, 29);
    gUbRank->tex[13] = NULL;
    gUbRank->tex[15] = NULL;
    gUbRank->tex[16] = NULL;
    gUbRank->tex[17] = NULL;
    gUbRank->tex[18] = NULL;
    gUbRank->tex[19] = NULL;
    gUbRank->tex[9] = NULL;
    gUbRank->texB[1] = MTEX(res, 11);
    gUbRank->texB[2] = MTEX(res, 12);
    gUbRank->texB[5] = MTEX(res, 20);
    gUbRank->texB[4] = MTEX(res, 21);
    gUbRank->texB[0] = NULL;
    gUbRank->texB[3] = NULL;
    gUbRank->playerChip = MTEX(res, 26);
    UR_RES(17);
    gUbRank->texC[0] = MTEX(res, 0);
    gUbRank->texC[1] = MTEX(res, 1);
    gUbRank->texC[2] = MTEX(res, 2);
    gUbRank->texC[3] = MTEX(res, 3);
    gUbRank->texC[4] = MTEX(res, 4);
    gUbRank->texC[5] = MTEX(res, 5);
    gUbRank->texC[6] = MTEX(res, 6);
    gUbRank->texC[7] = MTEX(res, 7);
    gUbRank->texC[8] = MTEX(res, 8);
    gUbRank->texC[9] = MTEX(res, 9);
    gUbRank->texC[12] = MTEX(res, 10);
    gUbRank->chips = (u32 *)MPACK_AT(gUbRank->res, 6);
    #ifdef PORT
    for (i = 0; i < gUbRank->chips[0] && i < 250; i++) {
#else
    for (i = 0; i < N_CHIP_NUM; i++) {
#endif
        res = (MTexRes *)MPACK_AT(gUbRank->chips, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    Flash_Create(&gUbRank->flash[0], MPACK_AT(gUbRank->res, 1), gUbRank->tex);
    Flash_Play(&gUbRank->flash[0], 1);
    Flash_Create(&gUbRank->flash[1], MPACK_AT(gUbRank->res, 15), gUbRank->texB);
    Flash_Play(&gUbRank->flash[1], 1);
    Flash_Create(&gUbRank->flash[2], MPACK_AT(gUbRank->res, 16), gUbRank->texC);
    gUbRank->blink = Rand_Libc() % 32;
    UR_RES(2);
    IconWin_Init(MPACK_AT(gUbRank->res, 7), res);
    IconWin_Open();
    gUbRank->text = MPACK_AT(gUbRank->res, 14);
    for (i = 0; i < UBRANK_BOX_NUM; i++) {
        TextBox_Init(&gUbRank->box[i], gUbRank->text, 0);
        TextBox_SetUnk50(&gUbRank->box[i], 0);
    }
    gUbRank->subtitles = MPACK_AT(gUbRank->res, 18);
    gUbRank->voiceLine = -1;
    gUbRank->voiceSkip = 0;
    /* from here on `i` is the player's place, on which the list is centred */
    if (NPROG->ubFlags & NPROG_UB_FOUGHT) {
        NPROG->ubFlags &= ~NPROG_UB_FOUGHT;
        i = gSaveData->rank;
        if (BattleResult_GetFlags() == 1) {
            gUbRank->won = 1;
        } else {
            gUbRank->won = 0;
        }
        if (NPROG->ubFlags & NPROG_UB_UPWARD) {
            gUbRank->flags |= UBRANK_RESULT;
            UbRank_SetFoe(NPROG->ubChoice, 0);
            if (gUbRank->won) {
                if (i > 0) {
                    i--;
                }
                if (NPROG->ubFlags & NPROG_UB_INTRUDER) {
                    gUbRank->climb = 5;
                }
            }
        }
    } else {
        gUbRank->state = UBRANK_ST_LIST;
        i = gSaveData->rank;
    }
    if (i >= 98) {
        gUbRank->top = UBRANK_PLACES - UBRANK_ROWS;
    } else if (i < 2) {
        gUbRank->top = 0;
    } else {
        gUbRank->top = i - 2;
    }
}

/* Frees everything Init made. */
void UbRank_Term(void) {
    s32 i;

    IconWin_Term();
    for (i = 0; i < UBRANK_FLASH_NUM; i++) {
        Flash_Destroy(&gUbRank->flash[i]);
    }
    if (gUbRank->faceFile[0] != NULL) {
        Heap_Free(gUbRank->faceFile[0]);
        gUbRank->faceFile[0] = NULL;
    }
    if (gUbRank->faceFile[1] != NULL) {
        Heap_Free(gUbRank->faceFile[1]);
        gUbRank->faceFile[1] = NULL;
    }
    if (gUbRank->faceRes[0] != NULL) {
        Heap_Free(gUbRank->faceRes[0]);
        gUbRank->faceRes[0] = NULL;
    }
    if (gUbRank->faceRes[1] != NULL) {
        Heap_Free(gUbRank->faceRes[1]);
        gUbRank->faceRes[1] = NULL;
    }
    if (gUbRank->res != NULL) {
        Heap_Free(gUbRank->res);
        gUbRank->res = NULL;
    }
    if (gUbRank->file != NULL) {
        Heap_Free(gUbRank->file);
        gUbRank->file = NULL;
    }
    if (gUbRank != NULL) {
        Heap_Free(gUbRank);
        gUbRank = NULL;
    }
}

/* Draws the screen: the guide, the six plates of the list, the scroll arrows, the versus panel. */
void UbRank_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlash *flash;
    MTexRes *res;
    s32 i;

    Sprite_DrawPicture(gUbRank->bg, 0, 0, 0x80);
    flash = &gUbRank->flash[0];
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gUbRank->blink, 0);
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gUbRank->talk, 0);
    for (i = 0; i < UBRANK_ROWS; i++) {
        s32 place = gUbRank->top + i;

        sprintf(name, "mc_ranking_plate%02d", i);
        Num_DrawChild(flash, name, "mc_jyuni_suji%d", 0, 3, place + 1, 0x40, 0x40, 0, 0);
        if (place == gSaveData->rank) {
            gUbRank->tex[sUbRankChipTex[i]] = gUbRank->playerChip;
            Flash_FindLabel(flash, name, "mc_charaname_kari", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, NPROG->chara, &gUbRank->box[i]);
        } else {
            s32 chara = gUbRank->foe[gUbRank->rule[UbRank_PlaceToRule(place)].foe].chara;

            res = UR_CHIP_RES(chara);
            gUbRank->tex[sUbRankChipTex[i]] = MTEX(res, 0);
            Flash_FindLabel(flash, name, "mc_charaname_kari", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, chara, &gUbRank->box[i]);
        }
        sprintf(name, "mc_senreki_chara%d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        switch (gUbRank->state) {
        case UBRANK_ST_VS:
        case UBRANK_ST_CLOSING:
        case UBRANK_ST_WAIT:
        case UBRANK_ST_RESULT:
            if ((NPROG->ubFlags & NPROG_UB_UPWARD) && (gSaveData->rank == place || NPROG->ubChoice == place)) {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            }
            break;
        default:
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            break;
        }
    }
    sprintf(name, "mc_ranking_plate%02d", 5);
    Num_DrawChild(flash, name, "mc_jyuni_suji%d", 0, 3, gUbRank->extra + 1, 0x40, 0x40, 0, 0);
    if (gUbRank->extra == gSaveData->rank) {
        gUbRank->tex[sUbRankChipTex[5]] = gUbRank->playerChip;
        Flash_FindLabel(flash, name, "mc_charaname_kari", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, NPROG->chara, &gUbRank->box[5]);
    } else {
        s32 chara = gUbRank->foe[gUbRank->rule[UbRank_PlaceToRule(gUbRank->extra)].foe].chara;

        res = UR_CHIP_RES(chara);
        gUbRank->tex[sUbRankChipTex[5]] = MTEX(res, 0);
        Flash_FindLabel(flash, name, "mc_charaname_kari", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, chara, &gUbRank->box[5]);
    }
    Flash_FindLabel(flash, "mc_yajirusi_up", "mc_yajirusi_icon_up", &ref);
    if (gUbRank->top > 0) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x20;
    uv.y1 = 0x40;
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_FindLabel(flash, "mc_yajirusi_down", "mc_yajirusi_icon_down", &ref);
    if (gUbRank->top < UBRANK_PLACES - UBRANK_ROWS) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    uv.x0 = 0x20;
    uv.y0 = 0x20;
    uv.x1 = 0x40;
    uv.y1 = 0x40;
    Flash_ClipSetUv(flash, &ref, &uv);
    flash = &gUbRank->flash[1];
    switch (gUbRank->state) {
    case UBRANK_ST_INTRUDE:
        if (gUbRank->flags & UBRANK_INTRUDE) {
            s32 chara = gUbRank->foe[gUbRank->rule[UbRank_PlaceToRule(NPROG->ubChoice)].foe].chara;

            Flash_SetOffset(flash, 0xC5, (NPROG->ubChoice - gUbRank->top) * 0x34 + 0x84);
            res = UR_CHIP_RES(chara);
            gUbRank->texB[3] = MTEX(res, 0);
            res = UR_CHIP_RES(gUbRank->curFoe->chara);
            gUbRank->texB[0] = MTEX(res, 0);
            Flash_SetFlag(flash, 4, 0);
        }
        break;
    case UBRANK_ST_VS:
    case UBRANK_ST_CLOSING:
    case UBRANK_ST_WAIT:
    case UBRANK_ST_RESULT:
        if (NPROG->ubFlags & NPROG_UB_UPWARD) {
            Flash_SetOffset(flash, 0xC5, (NPROG->ubChoice - gUbRank->top) * 0x34 + 0xB8);
            res = UR_CHIP_RES(gUbRank->curFoe->chara);
            if (gSaveData->rank > NPROG->ubChoice) {
                gUbRank->texB[3] = MTEX(res, 0);
                gUbRank->texB[0] = gUbRank->playerChip;
            } else {
                gUbRank->texB[0] = MTEX(res, 0);
                gUbRank->texB[3] = gUbRank->playerChip;
            }
            Flash_SetFlag(flash, 4, 0);
        } else {
            Flash_SetFlag(flash, 4, 1);
        }
        break;
    default:
        Flash_SetFlag(flash, 4, 1);
        break;
    }
    for (i = 0; i < UBRANK_FLASH_NUM; i++) {
        if (gUbRank->state != UBRANK_ST_WAIT || i == 2) {
            Flash_Draw(&gUbRank->flash[i]);
        }
    }
    if (gUbRank->state != UBRANK_ST_WAIT) {
        IconWin_Draw();
    }
}

/* Advances the leave timer and the movies; follows the marks of the list and panel movies. */
void UbRank_Update(void) {
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gUbRank->timer > 0) {
        gUbRank->timer--;
    }
    if ((gUbRank->flash[0].trig & 1) && gUbRank->state == UBRANK_ST_INTRUDE) {
        Flash_GotoLabel(&gUbRank->flash[1], "fl_intrude_in", 1);
        gUbRank->flags |= UBRANK_INTRUDE;
    }
    if ((gUbRank->flash[0].trig & 2) && gUbRank->state == UBRANK_ST_CLIMB) {
        Flash_GotoLabel(&gUbRank->flash[1], "fl_win_player", 1);
        gUbRank->state = UBRANK_ST_RESULT;
    }
    if (gUbRank->flash[1].trig & 1) {
        switch (gUbRank->state) {
        case UBRANK_ST_INTRUDE:
            gUbRank->flags |= UBRANK_INTRUDE_DONE;
            gUbRank->flags &= ~UBRANK_INTRUDE;
            break;
        case UBRANK_ST_VS:
            gUbRank->flags |= UBRANK_PANEL_DONE;
            break;
        case UBRANK_ST_RESULT:
            gUbRank->flags |= UBRANK_PANEL_DONE;
            break;
        }
    }
    if (gUbRank->flash[1].se & 1) {
        Snd_PlaySe(2, 0x3A);
    }
    for (i = 0; i < UBRANK_FLASH_NUM; i++) {
        Flash_Advance(&gUbRank->flash[i]);
    }
}

/* The guide: starts the requested line; while one plays, confirm cuts it short. */
void UbRank_UpdateVoice(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (!gUbRank->voiceSkip) {
        if (gUbRank->voiceReq == 0) {
            return;
        }
        if (gUbRank->voiceLine != -1 && Voice_GetStat(0) != N_VOICE_IDLE && gUbRank->voiceReq == gUbRank->voicePrev) {
            if (gPad[0].gamePressed & 0x200) {
                Snd_PlaySe(1, 1);
                gUbRank->voiceSkip = 1;
            }
            return;
        }
    } else {
        gUbRank->voiceSkip = 0;
    }
    switch (gUbRank->voiceReq) {
    case 1:
        gUbRank->voiceLine = 0x4A;
        break;
    case 2:
        gUbRank->voiceLine = 0x4B;
        break;
    case 3:
        gUbRank->voiceLine = 0x4C;
        break;
    case 4:
        gUbRank->voiceLine = 0x50;
        break;
    case 5:
        gUbRank->voiceLine = 0x51;
        break;
    case 6:
        gUbRank->voiceLine = 0x35;
        break;
    }
    gUbRank->voiceReq = 0;
    Voice_PlayWithSubtitle(gUbRank->subtitles, N_VOICE_BASE, gUbRank->voiceLine);
    gUbRank->voicePrev = gUbRank->voiceReq;
}

/* Pad 0 and the screen's state machine: the list, the intruder, the versus panel, the outcome of a battle. */
void UbRank_Input(s32 *result) {
    s32 state = gUbRank->state;
    s32 top = gUbRank->top;
    s32 pos = top + gUbRank->cursor[state];
    s32 up = gPad[0].gameRepeat & 8;
    s32 down = gPad[0].gameRepeat & 4;
    s32 ok = gPad[0].gamePressed & 0x200;
    s32 cancel = gPad[0].gamePressed & 0x400;

    if (!(gUbRank->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gUbRank->flags & UBRANK_STARTED)) {
        gUbRank->cursor[0] = gSaveData->rank - top;
        UbRank_ClipGoto(0, 0, "fl_on_start");
        gUbRank->flags |= UBRANK_STARTED;
        if (gUbRank->flags & UBRANK_RESULT) {
            gUbRank->state = UBRANK_ST_RESULT;
            if (gUbRank->won) {
                if (gSaveData->rank == 1) {
                    gUbRank->voiceReq = 5;
                } else if (gSaveData->rank == 2) {
                    gUbRank->voiceReq = 3;
                } else if (gSaveData->rank != 0) {
                    gUbRank->voiceReq = Rand_Range(2) + 1;
                }
                Flash_GotoLabel(&gUbRank->flash[1], "fl_win_player", 1);
                return;
            }
            Flash_GotoLabel(&gUbRank->flash[1], "fl_win_cpu", 1);
            return;
        }
        return;
    }
    switch (state) {
    case UBRANK_ST_LIST:
        if (up && gSaveData->rank - 1 < pos) {
            if (gUbRank->cursor[gUbRank->state] == 1 && gUbRank->top != 0) {
                gUbRank->top--;
                Flash_GotoLabel(&gUbRank->flash[0], "fl_ranking_down", 1);
                Snd_PlaySe(1, 0);
                gUbRank->extra = gUbRank->top + 5;
            } else if (gUbRank->cursor[gUbRank->state] > 0) {
                UbRank_ClipGoto(0, 0, "fl_off_start");
                gUbRank->cursor[gUbRank->state]--;
                UbRank_ClipGoto(0, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
            gUbRank->idle = 0;
        } else if (down) {
            if (gUbRank->cursor[gUbRank->state] < UBRANK_ROWS - 1) {
                UbRank_ClipGoto(0, 0, "fl_off_start");
                gUbRank->cursor[gUbRank->state]++;
                UbRank_ClipGoto(0, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            } else if (gUbRank->top < UBRANK_PLACES - UBRANK_ROWS) {
                gUbRank->top++;
                Flash_GotoLabel(&gUbRank->flash[0], "fl_ranking_up", 1);
                Snd_PlaySe(1, 0);
                gUbRank->extra = gUbRank->top - 1;
            }
            gUbRank->idle = 0;
        } else if (ok) {
            NPROG->ubChoice = gUbRank->top + gUbRank->cursor[gUbRank->state];
            if (NPROG->ubChoice == gSaveData->rank) {
                Snd_PlaySe(1, 7);
            } else {
                if (NPROG->ubChoice < gSaveData->rank) {
                    NPROG->ubFlags |= NPROG_UB_UPWARD;
                } else {
                    NPROG->ubFlags &= ~NPROG_UB_UPWARD;
                }
                if (UbRank_PickFoe(NPROG->ubChoice)) {
                    Flash_GotoLabel(&gUbRank->flash[0], "fl_intrude_in", 1);
                    gUbRank->state = UBRANK_ST_INTRUDE;
                    gUbRank->voiceReq = 4;
                } else {
                    gUbRank->state = UBRANK_ST_VS;
                    if (NPROG->ubFlags & NPROG_UB_UPWARD) {
                        Flash_GotoLabel(&gUbRank->flash[1], "fl_vs_action", 1);
                    } else {
                        gUbRank->flags |= UBRANK_PANEL_DONE;
                    }
                }
                gUbRank->loadState = UBRANK_LOAD_REQUEST;
                Snd_PlaySe(1, 1);
            }
            gUbRank->idle = 0;
        } else if (cancel) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Flash_GotoLabel(&gUbRank->flash[0], "fl_ranking_cancel", 1);
            Snd_PlaySe(1, 2);
            gUbRank->idle = 0;
        } else {
            gUbRank->idle++;
            if (gUbRank->idle == N_IDLE_FRAMES) {
                gUbRank->voiceReq = 6;
                gUbRank->idle = 0;
            }
        }
        break;
    case UBRANK_ST_INTRUDE:
        if (gUbRank->flags & UBRANK_INTRUDE_DONE) {
            gUbRank->state = UBRANK_ST_VS;
            Flash_GotoLabel(&gUbRank->flash[1], "fl_vs_action", 1);
            gUbRank->flags &= ~UBRANK_INTRUDE_DONE;
        }
        break;
    case UBRANK_ST_VS:
        if ((gUbRank->flags & (UBRANK_FACES_READY | UBRANK_PANEL_DONE)) == (UBRANK_FACES_READY | UBRANK_PANEL_DONE)) {
            gUbRank->state = UBRANK_ST_CLOSING;
            Flash_Play(&gUbRank->flash[2], 1);
            gUbRank->flags &= ~UBRANK_PANEL_DONE;
        }
        break;
    case UBRANK_ST_CLOSING:
        gUbRank->state = UBRANK_ST_WAIT;
        gUbRank->wait = 300;
        break;
    case UBRANK_ST_WAIT:
        gUbRank->wait--;
        if (ok || gUbRank->wait == 0) {
            Bgm_FadeOutStep();
            gUbRank->flags |= UBRANK_CHOSEN;
            gUbRank->flags |= UBRANK_LEAVING;
            gUbRank->timer = 15;
            if (ok) {
                Snd_PlaySe(1, 1);
            }
            NPROG->ubFlags |= NPROG_UB_FOUGHT;
        }
        break;
    case UBRANK_ST_RESULT:
        if (gUbRank->flags & UBRANK_PANEL_DONE) {
            gUbRank->flags &= ~UBRANK_PANEL_DONE;
            if ((NPROG->ubFlags & NPROG_UB_UPWARD) && gUbRank->won) {
                gSaveData->rank--;
            }
            if (gUbRank->climb != 0 && gSaveData->rank != 0) {
                UbRank_SetFoe(UbRank_PlaceToRule(gSaveData->rank - 1), 0);
                gUbRank->climb--;
                if (!UbRank_ScrollUp()) {
                    Flash_GotoLabel(&gUbRank->flash[1], "fl_win_player", 1);
                } else {
                    NPROG->ubChoice--;
                    gUbRank->state = UBRANK_ST_CLIMB;
                }
            } else {
                gUbRank->state = UBRANK_ST_LIST;
            }
        }
        break;
    }
}

/* Sends the plate of cursor[state] in one of the movies to a label. */
void UbRank_ClipGoto(s32 movie, s32 state, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gUbRank->flash[movie];

    sprintf(name, "mc_ranking_plate%02d", gUbRank->cursor[state]);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Background loader of the two fighters' large pictures (files 0x2F9 + character), one step per frame. */
void UbRank_UpdateFaceLoad(void) {
    s32 chara[2];
    MTexRes *res;
    s32 *p = chara; /* through a pointer: `memset(chara, 0, 8)` is expanded in line, the original calls memset */

    memset(p, 0, 8);
    switch (gUbRank->loadState) {
    case UBRANK_LOAD_REQUEST:
        gUbRank->texC[11] = NULL;
        gUbRank->texC[10] = NULL;
        chara[0] = NPROG->chara;
        chara[1] = gUbRank->curFoe->chara;
        File_CancelRequests();
        File_Request(chara[0] + N_FACE_FILE, gUbRank->faceFile[0], 0x16800);
        File_Request(chara[1] + N_FACE_FILE, gUbRank->faceFile[1], 0x16800);
        gUbRank->loadState = UBRANK_LOAD_READ;
        break;
    case UBRANK_LOAD_READ:
        if (File_UpdateRequests()) {
            gUbRank->loadState = UBRANK_LOAD_UNPACK;
        }
        break;
    case UBRANK_LOAD_UNPACK:
        Sprite_Unpack(gUbRank->faceFile[0], gUbRank->faceRes[0], NULL);
        Sprite_Unpack(gUbRank->faceFile[1], gUbRank->faceRes[1], NULL);
        res = gUbRank->faceRes[0];
        Res_RelocateOffsets(&res, res, res);
        gUbRank->texC[11] = MTEX(res, 0);
        res = gUbRank->faceRes[1];
        Res_RelocateOffsets(&res, res, res);
        gUbRank->texC[10] = MTEX(res, 0);
        gUbRank->flags |= UBRANK_FACES_READY;
        gUbRank->loadState = UBRANK_LOAD_NONE;
        break;
    }
}

/*
 * Mode 26: the ranking ladder. Returns 1 when a battle was set up (BattleSetup_Finish was called), 0 when the
 * player backed out.
 */
s32 UbRank_Run(void) {
    s32 result = 1;

    UbRank_Init();
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        UbRank_UpdateFaceLoad();
        UbRank_Update();
        UbRank_UpdateVoice();
        UbRank_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gUbRank->flags & UBRANK_FADED_IN) && (gUbRank->flash[0].flags & MFLASH_PAD)) {
                gUbRank->flags |= UBRANK_FADED_IN;
            }
        }
        if (ColorFade_IsFadingOut()) {
            Bgm_FadeOutStep();
            Voice_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gUbRank->flags & UBRANK_LEAVING) {
            if (gUbRank->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gUbRank->voiceReq == 0) {
            UbRank_Input(&result);
        }
    }
    if (result) {
        UbRank_SetupBattle();
    }
    UbRank_Term();
    Dma_ResetBuffers();
    return result;
}
