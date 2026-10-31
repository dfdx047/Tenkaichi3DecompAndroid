#include "common.h"
#include "battle/view_b.h"
#include "battle/battle_setup.h"
#include "battle/battle_work.h"
#include "sys/adx.h"
#include "sys/common.h"
#include "sys/mem_read.h"
#include "sys/rand.h"
#include "sys/save.h"
#include "sys/snd.h"

/*
 * MenuUtil, 0x2614B0..0x261ED8: small helpers of the menus.
 *
 *   Voice_PlayWithSubtitle / Voice_StopWithLip / LipSync_*   a voice line on ADX player 4 with the mouth movement
 *                          of the portrait that speaks it (FlashAnim_Talk in view_a_e.c asks LipSync_IsOpen)
 *   SndOpt_Apply           the sound options of a save that was just loaded
 *   Progress_ClearTeams    empties the team records of gProgress
 *   ChrTbl_IsRelated       two character ids are forms of the same character
 *   CpuLevel_*             the five difficulty settings and the CPU levels 0, 6, 13, 21, 29
 *   Demo_SetupBattle       picks one of nine fixed pairings for a CPU against CPU battle
 *   MenuUtil_ClassifyId, Text_FindBullet, Text_PutNumber      not called by anything
 */

extern void *memset(void *dst, s32 c, u32 n);

/* This object's small data: 0x2FF110 and 0x2FF114 (.sdata, so they are defined here). */
s32 gLipSyncOpen = 0;    /* the mouth is open on this frame */
s32 gLipSyncWaiting = 0; /* the voice has not started playing yet */

extern LipSync gLipSync; /* 0x31E570, .bss */
extern MenuUtilProgress *gProgress;

/* Plays menu voice line `base + line` (in the language of the save's bit 0) and starts its mouth movement,
   if a lip data pack is given. */
void Voice_PlayWithSubtitle(LipPack *pack, s32 base, s32 line) {
    if (gSaveData->flags & 1) {
        Voice_Play(0, base + line + VOICE_LANG2_OFFSET, 0x80, 0);
        if (pack != NULL) {
            LipSync_Start((u8 *)pack + ((((u32 *)pack)[line * 2 + 2] >> 2) << 2));
        }
    } else {
        Voice_Play(0, base + line, 0x80, 0);
        if (pack != NULL) {
            LipSync_Start((u8 *)pack + ((((u32 *)pack)[line * 2 + 1] >> 2) << 2));
        }
    }
}

/* Stops the menu voice and its mouth movement. */
void Voice_StopWithLip(void) {
    Voice_Stop(0);
    LipSync_Clear();
}

/* Applies the sound options of the save: music volume back to 0x40, stereo or mono. */
void SndOpt_Apply(void) {
    Bgm_SetVolume(0x40);
    Snd_SetStereo(gSaveData->soundMode ^ 1);
}

/* Empties the seven team records of the progress block: every character id becomes "no character". */
void Progress_ClearTeams(void) {
    s32 i;
    s32 j;
    s32 k;

    memset(gProgress->team, 0, sizeof(gProgress->team));
    for (i = 0; i < PROGRESS_TEAM_COUNT; i++) {
        for (j = 0; j < 2; j++) {
            for (k = 0; k < 5; k++) {
                gProgress->team[i].chara[j][k] = PROGRESS_TEAM_EMPTY;
            }
        }
    }
}

/* Whether two character ids are forms of one character: equal, or one names the other in its link list.
   Three ids are never related to anything, themselves included. */
s32 ChrTbl_IsRelated(s32 a, s32 b) {
    ChrTblFile *file;
    ChrTblEntry *table;
    s32 i;

    if (a == CHRTBL_ID_4F || a == CHRTBL_ID_6D || a == CHRTBL_ID_80 || b == CHRTBL_ID_4F || b == CHRTBL_ID_6D ||
        b == CHRTBL_ID_80) {
        return 0;
    }
    file = (ChrTblFile *)gCommonRes->data[2];
    table = (ChrTblEntry *)((u8 *)file + ((file->charaOffset >> 2) << 2));
    if (a == b) {
        return 1;
    }
    for (i = 0; i < CHRTBL_LINK_COUNT; i++) {
        if (table[a].link[i] != CHRTBL_LINK_NONE && table[a].link[i] == b) {
            return 1;
        }
        if (table[b].link[i] != CHRTBL_LINK_NONE && table[b].link[i] == a) {
            return 1;
        }
    }
    return 0;
}

/* BT4's disc maps the same five menu slots to 6,13,21,29,99.
 * Keep these indices identical to the original options and save data. */
#ifdef PORT
const char *CpuLevel_SettingName(s32 setting) {
    static const char *bt3[]={"Very Weak","Weak","Normal","Strong","Very Strong"};
    static const char *bt4[]={"Easy","Normal","Hard","Very Hard","Z Hard"};
    extern s32 PortGame_IsBT4(void);
    if((u32)setting>=5)setting=2;
    if(PortGame_IsBT4())return bt4[setting];
    return bt3[setting];
}
#endif
/* CPU level of a difficulty setting 0..4. */
s32 CpuLevel_FromSetting(u32 setting) {
    s32 level = 0;
#ifdef PORT
    extern s32 PortGame_IsBT4(void);
    static const s32 bt4[]={6,13,21,29,99};
    if(PortGame_IsBT4())return setting<5?bt4[setting]:0;
#endif

    switch (setting) {
    case 0:
        level = 0;
        break;
    case 1:
        level = 6;
        break;
    case 2:
        level = 13;
        break;
    case 3:
        level = 21;
        break;
    case 4:
        level = 29;
        break;
    }
    return level;
}

/* Difficulty setting of a CPU level; -1 (no CPU) stays -1, a level in between gives 0. */
s32 CpuLevel_ToSetting(s32 level) {
    s32 setting = 0;
#ifdef PORT
    extern s32 PortGame_IsBT4(void);
    if(PortGame_IsBT4()) {
        if(level==-1)return -1;
        for(setting=0;setting<5;setting++)if(CpuLevel_FromSetting(setting)==level)return setting;
        return 0;
    }
#endif

    switch (level) {
    case -1:
        setting = -1;
        break;
    case 0:
        setting = 0;
        break;
    case 6:
        setting = 1;
        break;
    case 13:
        setting = 2;
        break;
    case 21:
        setting = 3;
        break;
    case 29:
        setting = 4;
        break;
    }
    return setting;
}

#define DEMO_PAIR(a, b, stg) \
    chara[0] = a; \
    chara[1] = b; \
    stage = stg; \
    costume[0] = 0; \
    costume[1] = 0; \
    bgm = Rand_Range(9) + 8

/* Sets up a one against one CPU battle with one of nine fixed pairings (never the same twice in a row) on the
   pairing's stage, with random music 8..16. Draws Rand_Range(9) until the pairing differs, then once more. */
void Demo_SetupBattle(void) {
    s32 chara[2];
    s32 costume[2];
    s32 bgm = 0;
    s32 stage = 0;
    s32 pick;
    s32 i;
    s32 size = sizeof(chara);

    memset(chara, 0, size);
    memset(costume, 0, size);
    do {
        pick = Rand_Range(9);
    } while (gProgress->demoPick == pick);
    gProgress->demoPick = pick;
    switch (pick) {
    case 0:
        DEMO_PAIR(0xA, 0x93, 0x1D);
        break;
    case 1:
        DEMO_PAIR(0x2A, 0x6C, 6);
        break;
    case 2:
        DEMO_PAIR(0x19, 0x5C, 2);
        break;
    case 3:
        DEMO_PAIR(0x27, 0x60, 1);
        break;
    case 4:
        DEMO_PAIR(0x17, 0x6F, 1);
        break;
    case 5:
        DEMO_PAIR(0x49, 0x5B, 0x10);
        break;
    case 6:
        DEMO_PAIR(0, 0x1E, 0x19);
        break;
    case 7:
        DEMO_PAIR(0x32, 0x72, 5);
        break;
    case 8:
        DEMO_PAIR(5, 0x25, 1);
        break;
    }
    Battle_ClearWork();
    BattleSetup_SetRule(0, 7, bgm, 5, 4, stage, 0);
    BattleSetup_SetSide(0, 2, 0, 1, 1, 1, 0, NULL);
    BattleSetup_SetSide(1, 2, 1, 1, 1, 1, 0, NULL);
    for (i = 0; i < 2; i++) {
        BattleSetup_SetMember(i, 0, chara[i], costume[i], 0, CpuLevel_FromSetting(2), 100.0f, NULL);
    }
    BattleSetup_Finish();
}

/* Sorts an id into one of eight classes by its value. No caller. */
s32 MenuUtil_ClassifyId(s32 id) {
    if ((u32)(id - 20102) < 8 || (u32)(id - 20111) < 889) {
        return 0;
    }
    if (id == 20101 || (u32)(id - 23000) < 1000) {
        return 1;
    }
    if (id == 20110) {
        return 2;
    }
    if (id == 29000) {
        return 3;
    }
    if (id == 29001) {
        return 4;
    }
    if (id == 20100 || (u32)(id - 50000) < 10000) {
        return 5;
    }
    return id == 80430 ? 6 : 7;
}

/* Looks for the first black circle in a line of a text file and returns its address in *out (unchanged when
   the line has none). The line offset is used without masking its low bits. No caller. */
void Text_FindBullet(u8 *text, s32 line, u16 **out) {
    u16 *p;

    if (line < 0 || text == NULL || out == NULL) {
        return;
    }
    p = (u16 *)(text + ((u32 *)text)[line + 1]);
    while (*p != 0) {
        if (*p == TEXT_CHAR_BULLET) {
            break;
        }
        p++;
    }
    if (*p == TEXT_CHAR_BULLET) {
        *out = p;
    }
}

/* Writes a value (at most 99999) as five fullwidth digits, leading zeros included. No caller. */
void Text_PutNumber(u16 *dst, u32 value) {
    u16 digit[10] = {0x10FF, 0x11FF, 0x12FF, 0x13FF, 0x14FF, 0x15FF, 0x16FF, 0x17FF, 0x18FF, 0x19FF};
    s32 div[TEXT_NUMBER_DIGITS] = {10000, 1000, 100, 10, 1};
    s32 i;
    u16 *p;

    if (dst != NULL) {
        p = dst;
        if (value > TEXT_NUMBER_MAX) {
            value = TEXT_NUMBER_MAX;
        }
        for (i = 0; i < TEXT_NUMBER_DIGITS; i++) {
            *p = digit[(s32)(value / div[i]) % 10];
            value %= div[i];
            p++;
        }
    }
}

/* Starts the mouth movement of a voice line from its key data; it runs once the voice is playing. */
void LipSync_Start(u8 *data) {
    LipSync *lip = &gLipSync;
    s32 on = 1;

    memset(lip, 0, sizeof(LipSync));
    lip->index = 0;
    lip->active = on;
    lip->count = Mem_ReadS32(data + 8);
    data += 0x10;
    lip->keys = data;
    lip->time = 0.0f;
    data += lip->count * LIPSYNC_KEY_SIZE;
    lip->length = Mem_ReadU16(data - LIPSYNC_KEY_SIZE);
    gLipSyncOpen = 0;
    gLipSyncWaiting = on;
}

/* One frame of mouth movement: finds the key the time falls in and takes its open / closed state. Ends when
   the time has passed the last key. Does nothing while the menu animations are frozen. */
void LipSync_Update(void) {
    LipSync *lip;
    u16 i;
    s32 found;

    if (gProgress->flags & 0x100) {
        return;
    }
    gLipSyncOpen = 0;
    if (gLipSyncWaiting != 0) {
        if (Voice_GetStat(0) != 3) {
            return;
        }
        gLipSyncWaiting = 0;
    }
    if (gLipSync.active == 0) {
        return;
    }
    found = -1;
    for (i = gLipSync.index; i < gLipSync.count - 1; i++) {
        if ((f32)Mem_ReadU16(gLipSync.keys + i * LIPSYNC_KEY_SIZE) <= gLipSync.time) {
            if (gLipSync.time < (f32)Mem_ReadU16(gLipSync.keys + i * LIPSYNC_KEY_SIZE + 4)) {
                found = i;
                gLipSync.index = found;
                break;
            }
        }
    }
    lip = &gLipSync;
    if (lip->time > lip->length || found == -1) {
        memset(lip, 0, sizeof(LipSync));
        return;
    }
    if (Mem_ReadU16(lip->keys + found * LIPSYNC_KEY_SIZE + 2) != 0) {
        gLipSyncOpen = 1;
    }
    lip->time += 1.0f;
}

/* Whether the mouth is open on this frame. */
s32 LipSync_IsOpen(void) {
    return gLipSyncOpen;
}

/* Stops the mouth movement. */
void LipSync_Clear(void) {
    memset(&gLipSync, 0, sizeof(LipSync));
}
