#ifndef SYS_SAVE_H
#define SYS_SAVE_H

#include "types.h"

/*
 * gSaveData: the 0x4000-byte block allocated by Save_Init. It is the whole persistent save: the memory
 * card code copies 0x4000 bytes straight into it after a load (McFlow_UpdateLoad / McFlow_UpdateBootLoad, from the
 * card buffer + 0x38) and hands it to the writer with size 0x4000 (McFlow_UpdateSave / McFlow_UpdateNewSave).
 * "How we know" is given per field; "menu" means the DBZP.BIN overlay (asm/dbzp/000000.s).
 */

#define SAVE_SIZE 0x4000

#define SAVE_CHARA_COUNT 161  /* BT4's additional fighters are unlocked by the grid's extended-ID rule. */
#define SAVE_CHARA_WORDS 3
#define SAVE_STAGE_COUNT 64   /* bits in stageBits */
#define SAVE_BGM_LIST_COUNT 32 /* entries of the list bgmBits belongs to (loop bound) */
#define SAVE_BGM_COUNT 32     /* entries of that list that have a bit */
#define SAVE_FLAG8_COUNT 7    /* bits of unlockFlags set by Save_UnlockAll */
#define SAVE_ITEM_COUNT 350   /* entries of item[] and of the item table in common file 4 */
#define SAVE_SLOT_COUNT 9
#define SAVE_CUSTOM_COUNT 97
#define SAVE_CUSTOM_SETS 3
#define SAVE_CUSTOM_ITEMS 8   /* 7 usable item slots + a zero terminator */
#define SAVE_REC_COUNT 14
#define SAVE_PAD_COUNT 2
#define SAVE_KEY_COUNT 8
#define SAVE_RULE_COUNT 6

#define SAVE_MONEY_MAX 9999999
#define SAVE_MONEY_UNLOCK_ALL 4850000
#define SAVE_VOLUME_DEFAULT 9 /* volumes are 0..9 */

/* SaveData.flags (0x1608). Defaults: bits 0-2 set. */
#define SAVE_FLAG_VOICE 1        /* voice set: set = file base 0xCC32, clear = 0x8D4E (sys/adx.c). First choice of the menu's voice item sets it. */
#define SAVE_FLAG_PAD_A(pad) (2 << (pad)) /* bits 1-2, one per controller; battle reads `flags & (2 << player)` (BtlChar_Reset). Vibration: guess. */
#define SAVE_FLAG_PAD_B(pad) (8 << (pad)) /* bits 3-4, one per controller; toggled and reset (cleared) only by the menu's controller page */
#define SAVE_FLAG_DEFAULT 7

/* SaveData.item[] bits. */
#define SAVE_ITEM_OWNED 1
#define SAVE_ITEM_NEW 2 /* set together with OWNED when the item is first obtained: guess for "not looked at yet" */

/* Flag of an ItemInfo entry: the item is owned in a new save. */
#define ITEM_INFO_INITIAL 2

/* 0x10 bytes; nine of them at 0x10. Indexed with `gSaveData + 0x10 + n * 0x10` all over the menu overlay. */
typedef struct SaveSlot {
    /* 0x00 */ s32 flags;  /* bits 0-1 set by Save_UnlockAll and, for slot 0, by the defaults; bit 2 tested/set by menu HistOutro_Init */
    /* 0x04 */ s32 val[3]; /* 0xFFFF each after Save_UnlockAll; slot 0 gets val[0] |= 1 and val[2] |= 1 by default */
} SaveSlot;

/* 0x38 bytes; per-character customisation (menu functions 0x398A60..0x3991D8, main ChrTbl_GetLevel / ChrTbl_GetExp). */
typedef struct SaveCustom {
    /* 0x00 */ u16 item[SAVE_CUSTOM_SETS][SAVE_CUSTOM_ITEMS]; /* equipped item ids (1-based, 0 = empty) of each of the 3 sets */
    /* 0x30 */ s32 unk30;  /* running total compared with the table value picked by `level` (EvoZ_RefreshStatus): experience, guess */
    /* 0x34 */ u16 level;  /* added to the character table's u16 at +0xE by ChrTbl_GetLevel; indexes its s32 table at +0x10 in ChrTbl_GetExp */
    /* 0x36 */ u16 unk36;
} SaveCustom;

/* 0x1C bytes; fourteen of them at 0x2D40. */
typedef struct SaveRec {
    /* 0x00 */ u8 unk0[0x14];
    /* 0x14 */ u16 level;  /* used instead of SaveCustom.level when ChrTbl_GetLevel is asked for one of these */
    /* 0x16 */ u16 unk16;
    /* 0x18 */ s32 chara;  /* character id, -1 = empty (defaults; ChrGrid_Build appends the non-empty ones to the character list) */
} SaveRec;

typedef struct SaveData {
    /* 0x0000 */ s32 unk0;
    /* 0x0004 */ s32 unk4;
    /* 0x0008 */ s32 unlockFlags;   /* bits 0-6 set by Save_UnlockAll; BattleSetup_FinishEx tests `1 << n` for n < 7; the menu also uses bit 9 */
    /* 0x000C */ s32 unkC;          /* 1 by default; picked from a list by menu HistSel_Input */
    /* 0x0010 */ SaveSlot slot[SAVE_SLOT_COUNT];
    /* 0x00A0 */ u8 unkA0[0x208 - 0xA0];
    /* 0x0208 */ s32 unk208;        /* bit 0 set by Save_UnlockAll */
    /* 0x020C */ s32 unk20C;        /* 5 by default, 20 after Save_UnlockAll: a capacity, guess */
    /* 0x0210 */ u8 unk210[0x77C - 0x210]; /* menu 0x37F5F8 / 0x385268 / 0x387F90 use 0x210-0x28D */
    /* 0x077C */ s32 unk77C;        /* 99 by default */
    /* 0x0780 */ u8 unk780[0xA08 - 0x780]; /* byte records at 0x780.. (menu 0x372E98 / 0x3760C8) */
    /* 0x0A08 */ s32 unkA08;        /* flag bits (0x20, 0x40, 1 << n), menu 0x362160..0x364B18 */
    /* 0x0A0C */ s32 unkA0C;        /* counter kept in 0..23 by menu Tour_Main */
    /* 0x0A10 */ s32 unkA10;
    /* 0x0A14 */ u8 unkA14[0xC10 - 0xA14];
    /* 0x0C10 */ u64 charaBits[SAVE_CHARA_WORDS]; /* bit id: character unlocked (ChrGrid_Build, BattleSetup_InitCharaBits) */
    /* 0x0C28 */ u64 stageBits;     /* bit id: stage unlocked (StgGrid_ApplyUnlocks turns locked ids into 0x24) */
    /* 0x0C30 */ u32 bgmBits;       /* bit id: entry of a 25-entry list unlocked (BgmList_ApplyUnlocks turns locked ids into 0x19); BGM is a guess */
    /* 0x0C34 */ s32 rule[SAVE_RULE_COUNT]; /* 3, 2, 2, 0, 0, 0 by default (Save_ResetRules); read by menu 0x348710 / 0x351508 / 0x353518 */
    /* 0x0C4C */ u8 unkC4C[0x1008 - 0xC4C]; /* menu 0x359358 / 0x35BB88 index an s32 array at 0xE0C */
    /* 0x1008 */ s32 unk1008;       /* flag bits, menu HistOutro_Init / Shop_CheckStockLevel */
    /* 0x100C */ s32 unk100C;       /* counter, same functions */
    /* 0x1010 */ u8 unk1010[0x1208 - 0x1010];
    /* 0x1208 */ s32 unk1208;       /* menu DcMenu_Start */
    /* 0x120C */ u8 unk120C[0x1608 - 0x120C];
    /* 0x1608 */ s32 flags;         /* SAVE_FLAG_* */
    /* 0x160C */ s32 key[SAVE_PAD_COUNT][SAVE_KEY_COUNT];     /* button assignment used in battle: BtlInput_GetKeyMask looks an action up in key[player] */
    /* 0x164C */ s32 keyEdit[SAVE_PAD_COUNT][SAVE_KEY_COUNT]; /* the copy the controller page edits and resets (menu Option_Input / Option_UpdateReset) */
    /* 0x168C */ s32 unk168C[2];
    /* 0x1694 */ s32 unk1694;       /* option copied to both players' battle setup (+0x30, +0x34) by BattleSetup_DefaultOptions / BattleSetup_SetOption14; reset with the screen page */
    /* 0x1698 */ s32 unk1698;       /* same, to +0x38 and +0x3C */
    /* 0x169C */ s32 screenX;       /* display offset, Gfx_SetDisplayRegs */
    /* 0x16A0 */ s32 screenY;
    /* 0x16A4 */ s32 soundMode;     /* 0 stereo, 1 mono: SndOpt_Apply calls Snd_SetStereo(soundMode ^ 1), which ends in Adx_SetMono(soundMode) */
    /* 0x16A8 */ s32 bgmVolume;     /* 0..9; sys/adx.c channels 0-1 */
    /* 0x16AC */ s32 seVolume;      /* 0..9; sys/adx.c channels 2-5 (stream SE, voice) and Snd_ScaleVolume (sound effects) */
    /* 0x16B0 */ u8 unk16B0[0x1808 - 0x16B0];
    /* 0x1808 */ SaveCustom custom[SAVE_CUSTOM_COUNT];
    /* 0x2D40 */ SaveRec rec[SAVE_REC_COUNT];
    /* 0x2EC8 */ u8 item[SAVE_ITEM_COUNT]; /* SAVE_ITEM_* per item; ids are 1-based elsewhere (0x2EC7 + id) */
    /* 0x3026 */ u8 unk3026[2];
    /* 0x3028 */ s32 money;         /* 0..SAVE_MONEY_MAX */
    /* 0x302C */ u8 unk302C[SAVE_SIZE - 0x302C];
} SaveData;

/* Header of common file 4 (gCommonRes->data[2]): byte offsets of its tables, rounded down to 4. */
typedef struct ItemFile {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ u32 charaOffset; /* table of 0x3C-byte character entries (ChrTbl_GetLevel) */
    /* 0x08 */ u32 itemOffset;  /* table of SAVE_ITEM_COUNT ItemInfo */
} ItemFile;

typedef struct ItemInfo {
    /* 0x00 */ u8 unk0[0x14];
    /* 0x14 */ s32 flags; /* ITEM_INFO_INITIAL */
    /* 0x18 */ u8 unk18[0x10];
} ItemInfo;

extern SaveData *gSaveData;

/* Word and mask of a character's unlock bit (the pointer form is what matches; `charaBits[id / 64]` does not). */
#define SAVE_CHARA_WORD(opt, id) (*((opt)->charaBits + (id) / 64))
#define SAVE_CHARA_MASK(id) (1LL << ((id) % 64))

void Save_UnlockAll(SaveData *opt);
void Save_SetDefaults(SaveData *opt);
void Save_Stub266600(SaveData *opt);
void Save_Init(void);
void Save_AddItem(s32 idx);
void Save_AddMoney(s32 amount);
void Save_ResetRules(void);

#endif
