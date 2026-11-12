#ifndef BATTLE_BTL_SCENE_H
#define BATTLE_BTL_SCENE_H

#include "types.h"

/*
 * Battle effect scene: src/battle/btl_scene.c = 0x12C9F0..0x12DD80 (53 functions).
 * State: gBtlScene (gp 0x2FE9A0, 0x34 bytes from the heap) and gBtlSceneCharCount (gp 0x2FE9A4, initialised 2).
 *
 * What it manages. Not the fighters and not the stage: it owns the task tree that everything attached to the
 * fighters runs in (the modules 0x12DD80..0x1AE200: auras, beams, impacts and the like), plus the services
 * those tasks share. In detail:
 *
 *   - the BtlPool arenas (BtlPool_Init / BtlPool_Term are called from here and nowhere else);
 *   - a task tree (0x1AD150..0x1ADC00, called BtlTask here). A task list is created by BtlTaskList_Create
 *     and holds 0x40-byte tasks; a task's class is a table of six callbacks (BtlTaskClass). The scene owns
 *       root   one list holding one task of class gBtlSceneRootClass (0x2C3490). Its init callback,
 *              BtlSceneRoot_Init, creates
 *       group  a child list of 5 tasks, the "layers", one per subsystem. Which layers exist is decided by
 *              BtlScene.layerMask (BTL_SCENE_LAYER_*), 0x1F = all in the battle;
 *   - the blast record list of the next module (gp 0x2FE9A8: 64 records of 0x190 bytes and a count at
 *     +0x6400), emptied at the start of every unpaused frame and refilled by the tasks;
 *   - a table of 7 floats per character (BtlSceneRate): six rates set to 1.0 and the body scale, which is
 *     the fighter object's height (+0xFF4) / 19.35;
 *   - a random number generator private to the effects (BtlScene_Rand, state in BtlScene.randState);
 *   - the answers to "is time stopped for this object" that the tasks ask before animating
 *     (BtlScene_IsEffectStopped / BtlScene_IsEffectHidden, 28 and 17 callers);
 *   - the stage change ("stage destruction") trigger, BtlScene_CheckStageChange.
 *
 * It has no state machine of its own. The only "states" are
 *   - BtlScene_Reset(mode): BTL_SCENE_RESET_ALL (0) calls the reset callback of every task and empties the
 *     record list (Battle_Restart, script reset); BTL_SCENE_RESET_REBUILD (1) also destroys and recreates
 *     the five layers; BTL_SCENE_RESET_CHAR0 / CHAR1 (2, 3) reset only the tasks flagged 0x800 / 0x1000,
 *     which are the tasks of character 0 / 1 (used when a character is streamed out: 0x1721D0, 0x14BB20);
 *     BTL_SCENE_RESET_2000 (4) resets the tasks flagged 0x2000;
 *   - BtlScene.flags bit 0: a stage change was asked for by BtlScene_RequestStageChange;
 *   - BtlScene.singleView: Battle_Update stores here whether the camera forced one full-screen view.
 *
 * Entry points, in the order Battle_Loop reaches them:
 *   BtlScene_Init(0)          from Battle_Init: allocates the state, layerMask = 0x1F, character count 2,
 *                             BtlPool_Init, then the inits of 0x1ADBA8, the record list (0x12DD80), 0x12F550,
 *                             the rate table and 0x1AA818, and creates the root task (which creates the layers).
 *   BtlScene_Reset(0)         from Battle_Restart.
 *   BtlScene_Update()         step 11, after the two first fighter phases: 0x12E040 (empties the record list
 *                             unless time is stopped), 0x12F720, BtlScene_UpdateCharScales, BtlTaskList_Update
 *                             (update callback of every task, depth first; a task whose flag bit 0 is set is
 *                             killed), 0x12EB10.
 *   BtlScene_PostUpdate()     after EftDet_Update: 0x12ECC8, BtlTaskList_PostUpdate (second callback),
 *                             BtlScene_UpdateRecords (0x140ED0 and 0x133478 on every record), 0x12E160.
 *   BtlScene_CheckStageChange()  after the third fighter phase, see the function.
 *   BtlScene_SetSingleView(v) end of Battle_Update.
 *   BtlScene_Draw(first)      step 13, inside the fighter pass, once per view. Stores first != 1 in
 *                             notFirstView; for the first view only runs 0x13A388, 0x1AE0A0 (when the stage
 *                             is ready) and 0x1AE118; then 0x130BA8 and BtlTaskList_Draw (draw callback of
 *                             every task that has been updated at least once).
 *   BtlScene_Term()           from Battle_Term: Reset(0), destroys the tree and everything Init created.
 * The menu overlay side also runs a scene of its own with the same calls (0x25DC20 / 0x25DA58 / 0x25DDB0 and
 * 0x262E70 / 0x262C30 / 0x262F70: Init, Update + Draw, Term).
 */

/* BtlScene.layerMask: one bit per layer task. Listed in the order BtlScene_CreateLayers creates them. */
#define BTL_SCENE_LAYER_0 0x08 /* class 0x2C3550 (init 0x137C70, pool slot 2, state gp 0x2FE9C0) */
#define BTL_SCENE_LAYER_1 0x04 /* class 0x2C36D0 (init 0x14ACB0, pool slot 3, state gp 0x2FE9F8), per character */
#define BTL_SCENE_LAYER_2 0x02 /* class 0x2C3BB0 (init 0x171D80, pool slot 6, state gp 0x2FEA48), per character */
#define BTL_SCENE_LAYER_3 0x01 /* class 0x2C3F98 (init 0x190CC8, pool slot 1, state gp 0x2FEA8C), 31 sub-tasks */
#define BTL_SCENE_LAYER_4 0x10 /* class 0x2C35D0 (init 0x13EA00, state 0x31BE60) */
#define BTL_SCENE_LAYER_ALL 0x1F

/* Argument of BtlScene_Reset. */
#define BTL_SCENE_RESET_ALL 0
#define BTL_SCENE_RESET_REBUILD 1
#define BTL_SCENE_RESET_CHAR0 2 /* BtlTaskList_Reset(root, 0x800) */
#define BTL_SCENE_RESET_CHAR1 3 /* BtlTaskList_Reset(root, 0x1000) */
#define BTL_SCENE_RESET_2000 4  /* BtlTaskList_Reset(root, 0x2000) */

/* BtlScene.flags */
#define BTL_SCENE_FLAG_STAGE_CHANGE 1

/* Number of rates in a BtlSceneRate. */
#define BTL_SCENE_RATE_COUNT 6

/* Callbacks of a task class (the tables at 0x2C3490, 0x2C3550...). Names from how 0x1AD150..0x1ADA80 call them. */
typedef struct BtlTaskClass {
    /* 0x00 */ void (*update)(void *task);      /* BtlTaskList_Update */
    /* 0x04 */ void (*init)(void *task, void *arg);
    /* 0x08 */ void (*term)(void *task);        /* BtlTask_Kill */
    /* 0x0C */ void (*postUpdate)(void *task);  /* BtlTaskList_PostUpdate */
    /* 0x10 */ void (*reset)(void *task);       /* BtlTaskList_Reset */
    /* 0x14 */ void (*draw)(void *task);        /* BtlTaskList_Draw */
} BtlTaskClass; /* size 0x18 */

/* Per-character effect rates, gBtlScene->rates[character]. */
typedef struct BtlSceneRate {
    /* 0x00 */ f32 rate[BTL_SCENE_RATE_COUNT]; /* 1.0 after init; BtlScene_SetCharRate / GetCharRate */
    /* 0x18 */ f32 scale;                      /* fighter object +0xFF4 divided by 19.35 */
} BtlSceneRate; /* size 0x1C */

/* The scene state, Heap_Alloc'ed by BtlScene_Init. */
typedef struct BtlScene {
    /* 0x00 */ void *root;       /* task list holding the root task */
    /* 0x04 */ void *group;      /* child list of the root task: the five layers */
    /* 0x08 */ void *layer[5];   /* layer tasks; [0] BTL_SCENE_LAYER_0 ... [4] BTL_SCENE_LAYER_4 */
    /* 0x1C */ s32 layerMask;    /* BTL_SCENE_LAYER_* */
    /* 0x20 */ s32 notFirstView; /* 0 while the first view is drawn, 1 for the second (split-screen) */
    /* 0x24 */ s32 singleView;   /* argument of BtlScene_SetSingleView */
    /* 0x28 */ s32 flags;        /* BTL_SCENE_FLAG_* */
    /* 0x2C */ BtlSceneRate *rates; /* gBtlSceneCharCount entries, from the pool */
    /* 0x30 */ u32 randState;    /* BtlScene_Rand */
} BtlScene; /* size 0x34 */

/* One record of the blast list of the next module (0x12DD80..); only what this file reads. */
typedef struct BtlBlastRec {
    /* 0x00 */ s32 objId;   /* object id of the fighter it belongs to */
    /* 0x04 */ s32 unk4[2];
    /* 0x0C */ s32 active;
    /* 0x10 */ u8 unk10[0x40];
    /* 0x50 */ s32 flags;   /* bit 0x10 tested, 0xA set by BtlScene_CheckStageChange */
    /* 0x54 */ u8 unk54[0x13C];
} BtlBlastRec; /* size 0x190 */

typedef struct BtlBlastList {
    /* 0x0000 */ BtlBlastRec rec[64];
    /* 0x6400 */ s32 count;
} BtlBlastList;

extern BtlScene *gBtlScene;
extern s32 gBtlSceneCharCount;

void BtlScene_Init(s32 layerMask);
void BtlScene_Term(void);
void BtlScene_Update(void);
void BtlScene_PostUpdate(void);
void BtlScene_Reset(s32 mode);
void BtlScene_Draw(s32 first);

s32 *BtlScene_GetCommonEntry(s32 idx);
s32 *BtlScene_GetPackEntry(s32 *base, s32 idx);
s32 BtlScene_GetPackEntrySize(s32 *base, s32 idx);
s32 BtlScene_TestCharPackBit(s32 side, s32 bit);
s32 *BtlScene_GetCharPackEntry(s32 side, s32 idx);
s32 BtlScene_GetStageData(void);

s32 BtlScene_IsTimeStopped(void);
s32 BtlScene_IsCharStopped(s32 objId);
s32 BtlScene_IsAnyCharStopped(void);
void BtlScene_SetCharCount(s32 count);
s32 BtlScene_GetCharCount(void);
s32 BtlScene_IsSecondView(void);
s32 BtlScene_IsCharInView(s32 objId);
s32 BtlScene_IsStageFlagOn(void);
s32 BtlScene_GetStageFlag(void);
void BtlScene_SetSingleView(s32 singleView);
s32 BtlScene_IsSingleView(void);

void BtlScene_FreeLayer0(void);
void BtlScene_CreateLayer0(void);
void *BtlScene_GetGroup(void);
void BtlScene_FreeCharLayer2(s32 chr);
void BtlScene_CreateCharLayer2(s32 chr);
void BtlScene_FreeChar(s32 chr);
void BtlScene_CreateChar(s32 chr);

s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);

void BtlScene_RequestStageChange(void);
s32 BtlScene_IsStageChangeRequested(void);
void BtlScene_ClearStageChangeRequest(void);
void BtlScene_CheckStageChange(void);

void BtlScene_InitRates(void);
void BtlScene_TermRates(void);
void BtlScene_SetCharRate(s32 chr, s32 slot, f32 value);
f32 BtlScene_GetCharRate(s32 chr, s32 slot);
void BtlScene_UpdateAllCharScales(void);
void BtlScene_UpdateCharScales(void);
f32 BtlScene_GetCharScale(s32 chr);

void BtlSceneRoot_Init(void *task);
void BtlSceneRoot_Term(void);
void BtlSceneRoot_Update(void);
void BtlScene_CreateLayers(void *group);
void BtlScene_FreeLayers(void);

s32 BtlScene_Rand(void);
f32 BtlScene_RandF(void);
f32 BtlScene_RandRangeF(f32 a, f32 b);
s32 BtlScene_RandRange(s32 a, s32 b);
void BtlScene_UpdateRecords(void);

#endif
