#ifndef BATTLE_BATTLE_H
#define BATTLE_BATTLE_H

#include "types.h"

/*
 * Battle scene top level: src/battle/battle.c = 0x12B570..0x12BD58 (eight functions).
 *
 *   Battle_Main      work->running = 1; Battle_Init(); Battle_Loop(); Battle_Term(); work->running = 0.
 *   Battle_Init      allocates every battle subsystem once, then Battle_Restart().
 *   Battle_Restart   puts every subsystem back to "start of match" without reloading; also run by
 *                    Battle_Loop when BATTLE_FLAG_RESTART is raised (rematch).
 *   Battle_Loop      one iteration per displayed frame, see below.
 *   Battle_Term      frees everything in roughly the reverse order.
 *
 * One frame (Battle_Loop), in this order:
 *    1. restart      if (flags & BATTLE_FLAG_RESTART) { Battle_Restart(); clear the flag; }
 *    2. Job_Run()    background loader jobs (character/stage streaming pushed by step 9)
 *    3. Gfx_BeginFrame()
 *    4. unless BATTLE_FLAG_PAUSE: Gsc_Update (walks the list at 0x333B80) and BtlScript_Update
 *       (only acts in sequence states 3 and 5)
 *    5. BtlChars_CheckStart()   fighter manager: first-frame hook when the sequence reaches state 2 or 3
 *    6. Pad_Update()      <- controller input is sampled here, once per frame
 *    7. Snd_Update(), Snd_SendFighters() (per-fighter sound update)
 *    8. BtlGame_PreUpdate()   HUD pre-update + BtlSeq_PreUpdate (sequence state's preUpdate)
 *    9. Battle_UpdateWork()   play-time counter and event bit sets (skipped while paused)
 *   10. BtlAiMgr_Update(), BtlChars_SampleInput()   two more fighter-side passes (see battle.c)
 *   11. Battle_Update()       the simulation: fighters, objects/effects, cameras, visibility lists
 *   12. BtlGame_Update()      BtlSeq_Update: runs the state's update, switches state; returns 1 = leave
 *   13. draw                  Battle_DrawSplit() when split-screen and Battle_Update() returned 1,
 *                             else Battle_Draw()
 *   14. Gfx_EndFrame(2)       waits for 2 vsync ticks
 *   15. Dma_Flush()           sends the display list built by step 13
 *   The loop ends after the frame in which step 12 returned non-zero.
 *
 * Pause / restart / exit:
 *   - Pause is BATTLE_FLAG_PAUSE in work->flags. Nothing in this file tests it except step 4; every
 *     subsystem update tests it itself and returns early (126 readers), so a paused frame still runs
 *     the whole loop, reads the pad, runs the sequence and draws. The pause menu sets
 *     PAUSE | PAUSE_MENU (0x4100) from the sequence/HUD code (0x218230, 0x22FA80) and clears it at 0x218298.
 *   - Restart: BtlSeq_Update, on reaching state 99 with a rematch asked for, sets BATTLE_FLAG_RESTART
 *     and returns 0; the next iteration calls Battle_Restart() before anything else.
 *   - Exit: BtlSeq_Update returns 1 on state 99 otherwise; Battle_Loop returns, Battle_Main runs
 *     Battle_Term and returns 1 to Game_Main, which reloads the menu overlay.
 *
 * Split-screen (setup.rule.screenMode == 1): Battle_Update updates both player cameras in either case; it then
 * builds the object visibility lists once per view (2 views) instead of once. Battle_DrawSplit is
 * Battle_Draw with the two scene passes (stage, then fighters/objects) run once per view after
 * BtlCam_SelectView(i) + BtlCam_ApplyView(1) (which loads the view's matrices and scissor); the
 * effect, HUD and overlay passes are drawn once for the whole screen. When the camera module forces
 * a single full-screen view (BtlCam_UpdateOverride() != 0: cut-scene style cameras), Battle_Update
 * returns 0 and the frame is drawn by Battle_Draw even in split-screen.
 */

/*
 * The battle block. This header is the single description of gBattleWork (0x1A00 bytes at 0x331DC8) for
 * every battle module; "verified" in a comment means matching C reads or writes the field at that offset
 * (src/battle/battle_work.c, battle_load.c). How a battle is set up, what the modes are and how the events
 * and the replay block work is described in battle/battle_setup.h; the loader in battle/battle_work.h.
 */

/* BattleWork.flags (64-bit). Bits not listed were not seen. */
#define BATTLE_FLAG_PAUSE       0x100  /* simulation frozen; tested by nearly every update function */
#define BATTLE_FLAG_DEMO        0x200  /* set by every non-fight sequence state (and BtlFacade_SetFlag200), cleared
                                          when the fighters are released: fetched input is neutral */
#define BATTLE_FLAG_READY       0x400  /* set only during the 2 s of the Ready state: fetched input is masked */
#define BATTLE_FLAG_LOAD_CHARA  0x800  /* a BtlLoad_StepChara job is running */
#define BATTLE_FLAG_LOAD_OBJECT 0x1000 /* a BtlLoad_StepObject job is running */
#define BATTLE_FLAG_LOADING     0x2000 /* a stage job (BtlLoad_StepStageReload / StepStageChange) is running: fighters,
                                          stage and cameras are neither updated nor drawn */
#define BATTLE_FLAG_PAUSE_MENU  0x4000 /* pause menu open (always set together with PAUSE) */
#define BATTLE_FLAG_RESTART     0x8000 /* Battle_Loop must call Battle_Restart() */
/* Bit 58 (0x8000 << 43) is tested by the effect draw pass 0x247578. */

/* BattleRule.mode; what each is comes from the callers (battle/battle_setup.h). */
enum {
    BTL_MODE_VERSUS = 0,
    BTL_MODE_SCRIPT = 1,
    BTL_MODE_2 = 2,
    BTL_MODE_POOL = 3,
    BTL_MODE_4 = 4,
    BTL_MODE_5 = 5,
    BTL_MODE_TRAINING = 6, /* guess */
    BTL_MODE_DEMO = 7,
    BTL_MODE_8 = 8,
    BTL_MODE_9 = 9
};

/* BattleSide.control */
#define BTL_CONTROL_PAD 0
#define BTL_CONTROL_CPU 2 /* 1 was not seen */

#define BTL_SETUP_MAGIC 0x736C7462 /* "btls" */
#define BTL_SETUP_VERSION 7
#define BTL_MEMBER_MAX 5
#define BTL_POOL_MAX 50
#define BTL_BGM_RANDOM 24      /* BattleSetup_SetRule: pick rand() % 24 */
#define BTL_CPU_LEVEL_MAX 29   /* CpuLevel_FromSetting maps the 5 difficulty settings to 0, 6, 13, 21, 29 */
#define BTL_TIME_EVENT_COUNT 19

/* BattleResult.winner */
#define BTL_RESULT_WIN_P1 0x01
#define BTL_RESULT_WIN_P2 0x02
#define BTL_RESULT_DRAW   0x04
#define BTL_RESULT_ABORT  0x08 /* battle left without a finish scene (set from the pause menu in mode 7) */
#define BTL_RESULT_OTHER  0x10 /* set together with reason 0x40000 */
/* BattleResult.reason. Also seen: bits 5 / 6 (force "side 1 won" / "side 0 won" at the end), bit 20. */
#define BTL_REASON_KO      0x00001 /* every character of a side has no health left */
#define BTL_REASON_TIME_UP 0x00002
#define BTL_REASON_FLAG7   0x00004 /* character flag 7 set on the active fighter (ring out / forced defeat) */
#define BTL_REASON_RESTART 0x18000 /* either bit: restart the battle instead of leaving; BattleResult_Set forwards
                                      bit 15 / 16 to BattleReplay_SetActive(0 / 1) */
#define BTL_REASON_BIT18   0x40000

/* A running clock. One tick is 34, 32, 34 ms in turn: 100 ms per 3 frames, 30 ticks per second. */
typedef struct BtlClock {
    /* 0x00 */ u32 ticks;
    /* 0x04 */ s16 hours;    /* stops at 9:59:59.999 */
    /* 0x06 */ s16 minutes;
    /* 0x08 */ s16 seconds;
    /* 0x0A */ s16 ms;
    /* 0x0C */ s16 timeLeft; /* battle clock only: seconds left of the time limit */
    /* 0x0E */ s16 unkE;
} BtlClock; /* size 0x10 */

/* Equipped items of a member: 1-based item ids, 0 = empty (same layout as SaveCustom.item[set]). */
typedef struct BattleItemSet {
    /* 0x00 */ u16 id[8];
} BattleItemSet; /* size 0x10 */

/* One team member: Battle_GetSetup()->sides[s].members[m]. All offsets verified. */
typedef struct BattleMember {
    /* 0x00 */ s32 chara;       /* character id (file 8 + chara * 2 + side, model 0x590 + chara * 10 + costume) */
    /* 0x04 */ s32 costume;
    /* 0x08 */ s32 variant;     /* non-zero: model file + 4; passed to BtlMember_LoadParams at fighter init */
    /* 0x0C */ s32 cpuLevel;    /* 0..29, -1 for the training opponent; copied to the fighter's member entry + 0x38 */
    /* 0x10 */ f32 health;      /* 100.0f from every menu caller, an integer script argument: percent (inferred) */
    /* 0x14 */ BattleItemSet items; /* all zero on every member of side 0 -> result summary bit 45 */
    /* 0x24 */ s32 bonus[8];    /* levels clamped to -20..20 (or 60): [0] = 0, [1] = [4] = item sum 2, [2] = sum 0,
                                   [3] = sum 1, [5] = [6] = [7] = sum 3 (the four s16 at ItemInfo + 0xC) */
    /* 0x44 */ s32 aiType;      /* item id - 0x87 of an equipped item whose ItemInfo byte 0 is 2, else the character
                                   table's first word; copied to the fighter's member entry + 0x3C */
    /* 0x48 */ s32 ability[4];  /* OR of the four words at ItemInfo + 0x18 of every equipped item */
    /* 0x58 */ void *data;      /* loader: the 0x1000-byte block in use: buf[0] after a (re)start, buf[1] after an
                                   in-battle change */
    /* 0x5C */ void *buf[2];    /* loader: two 0x1000-byte heap blocks */
} BattleMember; /* size 0x64 */

/* Character, costume and model variant of a side's fighter. */
typedef struct BattleForm {
    /* 0x00 */ s32 chara;
    /* 0x04 */ s32 costume;
    /* 0x08 */ s32 variant;
} BattleForm; /* size 0xC */

/* Characters a side may turn into (bit = character id). Only words 0..2 are ever written. */
typedef struct BattleCharaBits {
    /* 0x00 */ u64 bits[8];
} BattleCharaBits; /* size 0x40 */

/* One side (player) of the battle. All offsets verified. */
typedef struct BattleSide {
    /* 0x000 */ s32 memberCount; /* team size, 1..5 (fighter + 0x998) */
    /* 0x004 */ BattleMember members[BTL_MEMBER_MAX];
    /* 0x1F8 */ s32 lead;        /* index of the member that starts the battle */
    /* 0x1FC */ s32 unk1FC;      /* fighter + 0x1300. 1, or the inverted save rule[3] / rule[4] for a CPU side */
    /* 0x200 */ s32 unk200;      /* fighter + 0xCF4. 1 from most callers, 0 from menu 0x373A68 */
    /* 0x204 */ s32 pad;         /* controller number: fighter + 4, used as SAVE_FLAG_PAD_A(pad) */
    /* 0x208 */ s32 control;     /* BTL_CONTROL_* */
    /* 0x20C */ s32 unk20C;
    /* 0x210 */ BattleCharaBits charaBits; /* tested before a transformation / fusion (0x2033C8, 0x203788) */
    /* 0x250 */ BattleForm startForm; /* the lead member's, set by BattleSetup_FinishEx */
    /* 0x25C */ BattleForm form;      /* what is loaded now; the loader compares it with startForm at a restart */
    /* 0x268 */ s32 objId;         /* BtlObj_Get() argument of the side's fighter object */
    /* 0x26C */ s32 modelSlot;     /* result of BtlObj_RequestCharaModel(side, chara, costume, variant) */
} BattleSide; /* size 0x270 */

/* Battle rules: setup + 8. All offsets verified. */
typedef struct BattleRule {
    /* 0x00 */ s32 mode;       /* BTL_MODE_*; picks the sequence table */
    /* 0x04 */ s32 bgm;        /* BGM file 0x10B16 + bgm (Battle_ResetWork) */
    /* 0x08 */ s32 timeLimit;  /* index into gBattleTimeLimitTbl: 0 none, 1..5 = 60, 90, 180, 240, 45 s */
    /* 0x0C */ s32 announcer;  /* 0..7: announcement stream = base + announcer * 7 + n (0x22AB50) */
    /* 0x10 */ s32 unk10;      /* inverted save rule[5] in versus, 1 in mode 4; tested by 0x12D450 */
    /* 0x14 */ s32 stage;      /* stage the battle starts on, 0..34 */
    /* 0x18 */ s32 unk18;      /* mode 1: 1 + index of the first clear bit 0..6 of SaveData.unlockFlags, else 0 */
    /* 0x1C */ s32 screenMode; /* 1 = split-screen */
    /* 0x20 */ s32 curStage;   /* current stage (in-battle stage changes) */
} BattleRule; /* size 0x24 */

/* Options: setup + 0x2C. */
typedef struct BattleOption {
    /* 0x00 */ s32 isSet;      /* 0: BattleSetup_FinishEx fills optA / optB from the save data */
    /* 0x04 */ s32 optA[2];    /* per side, SaveData.unk1694 by default -> fighter + 0x49C */
    /* 0x0C */ s32 optB[2];    /* per side, SaveData.unk1698 by default -> fighter + 0x4B8 = (optB == 0) */
    /* 0x14 */ s32 unk14;      /* BattleSetup_SetOption14 / Battle_GetOption14, neither has a caller */
    /* 0x18 */ u8 unk18[0x78];
} BattleOption; /* size 0x90 */

/* The battle setup: the first 0x5A8 bytes of the battle work (Battle_GetSetup()), cleared, filled and copied whole. */
typedef struct BattleSetup {
    /* 0x000 */ u8 magic[4];   /* "btls" */
    /* 0x004 */ s32 version;   /* 7 */
    /* 0x008 */ BattleRule rule;
    /* 0x02C */ BattleOption option;
    /* 0x0BC */ s32 script;    /* battle script number + 1 (file 0x1FF + n), 0 = none */
    /* 0x0C0 */ BattleSide sides[2];
    /* 0x5A0 */ s32 unk5A0;    /* 1 after BattleSetup_FinishEx */
    /* 0x5A4 */ s32 unk5A4;
} BattleSetup; /* size 0x5A8 */

/* Opponent queue of mode 3: battle work + 0x5A8 (Battle_GetWork5A8()). */
typedef struct BattleMemberPool {
    /* 0x00 */ s32 cur;        /* reset to 0 */
    /* 0x04 */ s32 count;
    /* 0x08 */ BattleMember members[BTL_POOL_MAX];
} BattleMemberPool; /* size 0x1390 */

/* Result block: battle work + 0x1938 (Battle_GetResult()). Cleared by BattleResult_Clear (memset 0x48), filled
 * during the match by the sequence and by BattleResult_Finish at Term. */
typedef struct BattleResult {
    /* 0x00 */ s32 winner;     /* BTL_RESULT_* */
    /* 0x04 */ s32 reason;     /* BTL_REASON_* */
    /* 0x08 */ s32 unk8;       /* 1 when the battle was aborted */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ u64 eventSummary; /* built by BattleResult_CollectEvents from side 0's held events */
    /* 0x18 */ s32 frames;     /* BattleResult_CountFrame */
    /* 0x1C */ s32 unk1C[2];   /* per side, zeroed at fighter init (0x1C02C8) */
    /* 0x24 */ s32 unk24[2];   /* same */
    /* 0x2C */ f32 health[2];  /* compared to pick the winner of a time up or of a double finish */
    /* 0x34 */ BtlClock clock; /* copy of the battle clock at the end */
    /* 0x44 */ s32 unk44;
} BattleResult; /* size 0x48 */

/* One side's event bits (up to 128 events). */
typedef struct BattleEventSet {
    /* 0x00 */ u64 now[2];     /* raised since the last BtlEvent_Update */
    /* 0x10 */ u64 prev[2];    /* raised in the frame before */
    /* 0x20 */ u64 held[2];    /* raised at any time */
} BattleEventSet; /* size 0x30 */

/* Event work: battle work + 0x1980 (Battle_GetEventWork()). */
typedef struct BattleEvents {
    /* 0x00 */ s32 script;     /* handle made by Gsc_LoadFile from BattleRes.script; kept by BtlEvent_Reset */
    /* 0x04 */ s32 unk4;       /* Gsc_StartMain(script), mode 1 only */
    /* 0x08 */ BattleEventSet set[2];
    /* 0x68 */ s32 interrupt;  /* 1: waiting for BtlFacade_AreBothInterruptible() to raise event 0x4C */
    /* 0x6C */ s32 waitFlag;   /* 1 until event 0x4D or 0x4E is new on a side */
} BattleEvents; /* size 0x70 */

/* The battle's shared state: the static block gBattleWork, returned by Battle_GetWork(). */
typedef struct BattleWork {
    /* 0x0000 */ BattleSetup setup;     /* Battle_GetSetup() */
    /* 0x05A8 */ BattleMemberPool pool; /* Battle_GetWork5A8() */
    /* 0x1938 */ BattleResult result;   /* Battle_GetResult() */
    /* 0x1980 */ BattleEvents events;   /* Battle_GetEventWork() */
    /* 0x19F0 */ u64 flags;             /* BATTLE_FLAG_*; = 0 in Battle_ResetWork */
    /* 0x19F8 */ s32 running;           /* 1 between Battle_Main's entry and exit */
    /* 0x19FC */ s32 unk19FC;
} BattleWork; /* size 0x1A00 */

/* src/battle/battle_work.c */
BattleWork *Battle_GetWork(void);
BattleSetup *Battle_GetSetup(void);
BattleResult *Battle_GetResult(void);
BattleEvents *Battle_GetEventWork(void);
BattleMemberPool *Battle_GetWork5A8(void);
/* src/battle/battle_load.c (setup getters, see battle/battle_setup.h for the rest) */
s32 Battle_IsSplitScreen(void);
s32 Battle_GetMode(void);

s32 Battle_Restart(void);
s32 Battle_Init(void);
s32 Battle_Term(void);
s32 Battle_Update(void);
s32 Battle_Draw(void);
s32 Battle_DrawSplit(void);
void Battle_Loop(void);
s32 Battle_Main(void);

#endif
