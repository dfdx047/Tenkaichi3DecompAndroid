#ifndef MENU_MENU_C_H
#define MENU_MENU_C_H

#include "types.h"
#include "menu/menu_a.h"
#include "sys/save.h"

/*
 * Menu overlay DBZP.BIN, 0x33E108..0x342588 (placeholder stem "menu_c"): the history (story) mode screens that
 * Hist_Main (0x33CBF8, previous chunk) runs for progress modes 6, 8 and 10. Five files, cut at object
 * boundaries (see each file's head comment for the evidence):
 *
 *   (menu_c.c   0x33E108..0x33F700  HistSel     tail of the saga select (mode 6); now merged into
 *                                               src/menu/menu_b_d.c, 0x33CFC8..0x33F700, which includes this header)
 *   menu_c_b.c  0x33F700..0x3405A0  HistGuide   the scripted dialogues of the saga select
 *   menu_c_c.c  0x3405A0..0x341E08  HistResult  the result screen after a story battle (mode 8)
 *   menu_c_d.c  0x341E08..0x342190  HistSave    the save screen (mode 10)
 *   menu_c_e.c  0x342190..0x342588  CharSel     head of the character select object (body: src/menu/menu_d.c)
 *
 * HistSel below is the layout of the whole structure (include/menu/menu_b.h had a view from the object's head
 * only; it was removed when the two halves were merged).
 */

/* gProgress as the mode 6..10 screens use it (menu_a.h's MenuProgress has no names past 0x38). */
typedef struct HistReward {
    /* 0x00 */ s32 points[3];   /* by level */
    /* 0x0C */ s32 item[3];     /* item ids, -1 = none */
    /* 0x18 */ s32 chara[3];    /* character ids, -1 = none */
    /* 0x24 */ s32 stage[3];    /* stage ids, -1 = none */
    /* 0x30 */ s32 episode[3];  /* episode numbers counted through all sub menus, -1 = none */
} HistReward; /* 0x3C */

typedef struct HistProgress {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 baseFile;
    /* 0x08 */ void *unk8[3];
    /* 0x14 */ s32 flags;
    /* 0x18 */ s32 mode;
    /* 0x1C */ s32 prevMode;
    /* 0x20 */ s32 unk20[5];
    /* 0x34 */ s32 subMenu;
    /* 0x38 */ s32 subMenuItem;
    /* 0x3C */ s32 level;
    /* 0x40 */ HistReward reward;
} HistProgress;

#define HPROG ((HistProgress *)gProgress)

/*
 * gSaveData as the overlay's code sees it: an 8-byte checksum and a body (the part the checksum covers).
 * The nesting is what makes indexed accesses compile to the original address arithmetic (this compiler keeps
 * a member's offset modulo 16 apart from the index, and the two remainders of 8 add up to a constant of 0x10
 * that is added to the base before the index); include/sys/save.h has the same offsets as one flat structure.
 */
typedef struct MSaveBody {
    /* 0x0008 */ s32 unlockFlags;  /* bits 0-6 dragon balls, 0x80 / 0x100 / 0x200 guide events (see menu_c_b.c) */
    /* 0x000C */ s32 level;        /* difficulty chosen in the history menu, 0..2 */
    /* 0x0010 */ SaveSlot slot[SAVE_SLOT_COUNT]; /* per sub menu: flags (MSLOT_), val[0] listed, val[1] cleared, val[2] new */
    /* 0x00A0 */ u8 unkA0[0xC10 - 0xA0];
    /* 0x0C10 */ u64 charaBits[SAVE_CHARA_WORDS];
    /* 0x0C28 */ u64 stageBits;
    /* 0x0C30 */ u32 bgmBits;
    /* 0x0C34 */ u8 unkC34[0x2EC8 - 0xC34];
    /* 0x2EC8 */ u8 item[SAVE_ITEM_COUNT];
    /* 0x3026 */ u8 unk3026[2];
    /* 0x3028 */ s32 money;
} MSaveBody;

typedef struct MSave {
    /* 0x00 */ s32 sum[2];
    /* 0x08 */ MSaveBody body;
} MSave;

#define MSAVE (&((MSave *)gSaveData)->body)

/* SaveSlot.flags of a history sub menu */
#define MSLOT_LISTED 1        /* the sub menu is in the saga select's list */
#define MSLOT_NEW 2           /* it shows the "new" icon there */
#define MSLOT_OUTRO_SEEN 4    /* every episode was won and the outro (mode 9) was shown */
#define MSLOT_INTRODUCED 8    /* its guide has introduced it (script 151) */
#define MSLOT_EVENT_DONE 0x20 /* its guide's event dialogue was shown (script 651) */
#define MSLOT_NEW_EPISODE 0x40 /* a reward added an episode; cleared with MSLOT_NEW when the sub menu is entered */

/* MSaveBody.unlockFlags beyond the seven dragon balls */
#define MUNLOCK_HIST_INTRO 0x80     /* Goku's explanation of the mode was heard (script 1) */
#define MUNLOCK_HIST_COMPLETE 0x100 /* his speech at 100 % was heard (script 1151) */
#define MUNLOCK_HIST_NEW_SAGA 0x200 /* a reward listed a new sub menu: he announces it once (voice line 42) */

typedef struct MBattleResult {
    /* 0x00 */ u8 unk0[0x44];
    /* 0x44 */ s32 unk44;
} MBattleResult;

typedef struct HistResultReward {
    /* 0x00 */ s32 kind;
    /* 0x04 */ s32 id;
} HistResultReward;

#define HISTRESULT_FLASH_NUM 1
#define HISTRESULT_REWARD_MAX 17

typedef struct HistResult {
    /* 0x000 */ u32 *pack;
    /* 0x004 */ u32 *res;
    /* 0x008 */ void *file;
    /* 0x00C */ u32 *fileRes;
    /* 0x010 */ void *bgFile;
    /* 0x014 */ void *bgRes;
    /* 0x018 */ MFlash flash[HISTRESULT_FLASH_NUM];
    /* 0x044 */ void *bg;
    /* 0x048 */ u8 *tex[14];
    /* 0x080 */ s32 flags;
    /* 0x084 */ s32 unk84;          /* never used */
    /* 0x088 */ s32 timer;
    /* 0x08C */ s32 unk8C;          /* tested by Input, never set */
    /* 0x090 */ s32 state;
    /* 0x094 */ s32 points;
    /* 0x098 */ s32 pointsTotal;
    /* 0x09C */ s32 seState;
    /* 0x0A0 */ f32 scroll[4];
    /* 0x0B0 */ HistResultReward reward[HISTRESULT_REWARD_MAX];
    /* 0x138 */ s32 rewardCount;
    /* 0x13C */ s32 rewardIdx;
} HistResult; /* 0x140 */

#define HISTRESULT_CHOSEN 1
#define HISTRESULT_LEAVING 2
#define HISTRESULT_STARTED 4
#define HISTRESULT_FADED_IN 8
#define HISTRESULT_LOST 0x20
#define HISTRESULT_OUT 0x40

/* ---- HistSel: the saga select of the story mode (progress mode 6). Head of the object: 0x33CFC8. ---- */

#define HISTSEL_FLASH_NUM 1
#define HISTSEL_GUIDES 9      /* guides 0..7 belong to the sub menus, 8 is the menu's own (Goku) */
#define HISTSEL_ITEM_NONE 9   /* list filler */
#define HISTSEL_ITEM_ENDING 8 /* the item that replays the ending movie */

typedef struct HistSel {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 2 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked */
    /* 0x008 */ void *msgText[HISTSEL_GUIDES];   /* text of the message window per guide */
    /* 0x02C */ void *subtitles[HISTSEL_GUIDES]; /* subtitle table per guide */
    /* 0x050 */ MFlash flash[HISTSEL_FLASH_NUM];
    /* 0x07C */ void *bg;           /* section 1: background picture */
    /* 0x080 */ u8 *tex[83];
    /* 0x1CC */ s32 flags;          /* HISTSEL_ */
    /* 0x1D0 */ s32 cursor[3];      /* by focus: [0] menu item (0 episodes, 1 level), [1] list index of the sub menu, [2] level */
    /* 0x1DC */ s32 timer;          /* frames until the fade out starts */
    /* 0x1E0 */ s32 voiceLine;      /* line of the current guide's voice set, -1 = none */
    /* 0x1E4 */ s32 guide;          /* whose voice set and text: 0..7 a sub menu's guide, 8 Goku */
    /* 0x1E8 */ s32 idle;           /* frames without input; at 3600 Goku says an idle line */
    /* 0x1EC */ s32 focus;          /* 0 menu, 1 sub menu list, 2 level */
    /* 0x1F0 */ s32 script;         /* step of the guide script (HistGuide_Update), 0 = none: the pad is read */
    /* 0x1F4 */ s32 items[9];       /* listed sub menus (ids 0..8), padded with 9 up to three */
    /* 0x218 */ s32 itemCount;
    /* 0x21C */ s32 top;            /* list index on the first of the three plates (the list is a ring) */
    /* 0x220 */ s32 extra;          /* list index shown on the fourth plate while the list scrolls */
    /* 0x224 */ s32 prev;           /* list index the cursor was on before the last move */
    /* 0x228 */ s32 percent;        /* cleared episodes of 48, in percent */
    /* 0x22C */ s32 guest;          /* sub menu whose guide stands next to Goku, -1 = none */
    /* 0x230 */ s32 talker;         /* whose mouth moves: 0 Goku, 1 the guest */
    /* 0x234 */ s32 blink[2];       /* [0] Goku, [1] the guest (Init seeds both in a loop) */
    /* 0x23C */ s32 talk[2];        /* [0] Goku, [1] the guest */
    /* 0x244 */ f32 scroll[3];      /* vertical offset of the three haze layers ("mc_yuragi") */
    /* 0x250 */ s32 seTimer;        /* frames until the next ambient sound (300..840) */
    /* 0x254 */ f32 foregroundAlpha; /* BT4's guest foreground fade */
} HistSel; /* 0x258 */

#define HISTSEL_CHOSEN 1
#define HISTSEL_LEAVING 2
#define HISTSEL_STARTED 4     /* the cursor plate was lit once */
#define HISTSEL_GREETED 8     /* the greeting (or a guide script) was started */
#define HISTSEL_FIRST_VISIT 0x10  /* set by Init (unlockFlags 0x80 clear): the script starts at step 1 */
#define HISTSEL_EVENT 0x20        /* set by Init (HistSel_RollEvent picked a guest): step 651 */
#define HISTSEL_COMPLETE 0x40     /* set by Init (100 % and unlockFlags 0x100 clear): step 1151 */

extern HistSel *gHistSel; /* 0x3B12FC */

typedef struct HistSave {
    /* 0x00 */ u32 *pack;
    /* 0x04 */ u32 *res;
    /* 0x08 */ s32 flags;
    /* 0x0C */ s32 timer;
    /* 0x10 */ s32 mcState;
} HistSave; /* 0x14 */

extern HistResult *gHistResult; /* 0x3B1300 */
extern HistSave *gHistSave; /* 0x3B1304 */

extern MBattleResult *BattleResult_GetPtr(void);
extern void Save_AddItem(s32 idx);
extern void Save_AddMoney(s32 amount);
extern void GetWin_Init(void *pack, s32 lang);
extern void GetWin_Term(void);
extern void GetWin_Draw(void);
extern void GetWin_Open(void);
extern void GetWin_Close(void);
extern void GetWin_Next(void);
extern s32 GetWin_IsAnimating(void);
extern void GetWin_Setup(s32 kind, s32 value);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern void StreamSe_PlayPausedDefault(s32 se, s32 id);
extern void StreamSe_Resume(s32 se);
extern s32 StreamSe_GetStat(s32 se);
extern void StreamSe_FadeOutStep(s32 se);

extern void Flash_StepFrames(MFlash *flash, s32 step);
extern void MsgWin_SetText(void *text);

/* HistSel (src/menu/menu_b_d.c). */
extern void HistSel_PlayVoice(void);    /* plays voiceLine of the current guide's set, with its subtitle */
extern void HistSel_Idle(void);         /* idle frame: after 3600 of them Goku says one of four lines */
extern void HistSel_SayItem(void);      /* Goku comments the sub menu under the cursor */
extern void HistSel_RollEvent(void);      /* picks this visit's event saga (4 % each) */
extern void HistSel_Init(s32 section);

void HistGuide_Update(HistSel *menu, s32 *result);
s32 HistSel_Run(s32 section);
s32 HistResult_Run(s32 section);
s32 HistSave_Run(s32 section);

#endif
