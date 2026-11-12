#ifndef BATTLE_BATTLE_WORK_H
#define BATTLE_BATTLE_WORK_H

#include "types.h"
#include "sys/list.h"
#include "battle/battle.h"

/*
 * Battle work and battle loader. Two translation units (see the note in src/battle/battle_work.c):
 *   src/battle/battle_work.c  0x126EC8..0x127120  the static BattleWork block: accessors, reset, per-frame
 *                                                 update, Battle_Load / Battle_Unload
 *   src/battle/battle_load.c  0x127120..0x12B570  first half (to 0x129170): loader job pool, the five job step
 *                                                 functions, load requests, result block accessors; second
 *                                                 half: events, setup, replay (battle/battle_setup.h)
 * The battle block (BattleWork, BattleSide, BattleMember, BattleResult, BattleEvents) is in battle/battle.h.
 *
 * Everything below marked "verified" is what the matching C reads or writes; the rest is inference.
 *
 * --- Loader jobs ---------------------------------------------------------------------------------
 * All loading is done by jobs (sys/job.h): 0x40-byte BtlJob records taken from a private pool of 8
 * (gBtlJobPool, a static block; BtlJob_Alloc / BtlJob_Free), pushed with Job_Push() and stepped once per
 * frame by Job_Run() in Battle_Loop (or in a blocking loop by Load_RunBlocking()). A job frees itself
 * (BtlJob_Free) in the step that returns 1.
 *
 * File reads are asynchronous: File_Request(id, buf, size) queues a read (buf == NULL: the file layer
 * allocates from the heap; the third argument is passed but ignored by File_Request), and
 * File_UpdateRequests() returns non-zero once every queued read is finished.
 *
 * Buffers: everything comes from the main heap (Heap_Alloc(size, 0x40, 0, HEAP_ANY)); no BtlPool slot is
 * used in this range. The pointers live in BattleRes, the 0x50 bytes at gCommonRes + 0x20.
 *
 * --- Battle_Load(): the first load (job BtlLoad_StepInitial, run to completion by Load_RunBlocking) ---
 *   Before the job: BtlObj_Init(1), Gsc_InitDefault(gBtlScriptCommands), BtlScript_Init(), BtlJob_InitPool().
 *   The job starts at step 0, or at step 0x5A when setup.script != 0 (Battle_GetScript() >= 0).
 *   0x5A  request file 0x1FF + (setup.script - 1) (heap)                       -> res->script
 *   0x5B  wait; events->script = Gsc_LoadFile(res->script); Gsc_RunAction(script, 1000);
 *         BtlScript_ScanEvents(script); BtlScript_Nop(); BattleSetup_Finish(); then go to step 0.
 *   0     request, all heap-allocated by the file layer:
 *           0x14A                                    -> res->sndCommon   (common battle sound bank)
 *           0x14E + rule.stage                       -> res->sndStage
 *           0xBDA + side 0 character (0xC7B + .. when gSaveData->flags bit 0: alternate voices) -> res->sndChara[0]
 *           the same for side 1                      -> res->sndChara[1]
 *   1     wait for the reads
 *   2     Snd_LoadBank(4, sndCommon), (8, sndStage), (0x10, sndChara[0]), (0x20, sndChara[1])
 *   3     wait until Snd_IsUploadDone() (no sound bank transfer pending), then Heap_Free the four files
 *   4     for each side: objA = BtlObj_RequestCharaModel(side, chara, costume, variant) -> side->modelSlot (BattleSide_SetModelSlot)
 *         res->stage = Heap_Alloc(0x6CB800) (zeroed), res->stageSize = 0x6CB800   (stage model buffer)
 *         res->bank  = Heap_Alloc(0x90800)  (zeroed), res->bankSize  = 0x90800    (sound bank reload buffer)
 *         for each side, for each of the 5 members: member->buf[0], buf[1] = Heap_Alloc(0x1000) (zeroed),
 *           member->data = buf[0]
 *         request 0x171 + stage (0x198 + stage in split-screen) into res->stage
 *         for each side, for each member in use (side->memberCount of them): request file
 *           8 + member->chara * 2 + side into member->buf[0] (0x1000 bytes)
 *         request file 6 + gProgress->unk0[0] (heap) -> res->unk8
 *   5     wait; Res_RelocateOffsets(&member->buf[0], buf[0], buf[0]) for every member in use; done.
 *   After the job: side->objId = BtlObj_CreateChara(side->modelSlot) for both sides (BattleSide_SetObjId).
 *   The character models themselves are not loaded here: Battle_Restart -> Battle_ResetWork ->
 *   BtlLoad_Reload pushes one BtlLoad_StepChara job per side whose current character differs from
 *   the initial one (always the case after the memset of the first load).
 *
 * --- BtlLoad_StepChara (flag BATTLE_FLAG_LOAD_CHARA while it runs) -----------------------------------
 *   0  model file = 0x590 + chara * 10 + costume (+4 when `variant` is non-zero).
 *      Full load (modelOnly == 0): BtlRes_Reload(side->modelSlot, model, 0x598 + animChara * 10, 0x599 + unk1C * 10);
 *        voice bank 0xBDA / 0xC7B + voiceChara into res->bank; unless BtlCtrl_IsSwitching(side), file
 *        8 + chara * 2 + side into the member's second buffer (member = 0 for the initial load, else
 *        BtlSide_GetActiveMember(side)).
 *      Model only: BtlRes_Reload(side->modelSlot, model, -1, -1).
 *   1  wait; for kind 1 (in-battle change) BtlChange_NotifyLoaded().
 *   2  kind 1 waits for BtlChange_IsReady() (fighter manager request state 4, not paused); then
 *      BtlRes_CommitReload2(); Snd bank 0x10 (side 0) / 0x20 (side 1) reloaded from res->bank through Snd_ReloadBank;
 *      BtlObj_Rebind(side->objId, side->modelSlot); kind 0: BtlObjAnim_PlayAuto(BtlObj_Get(objId), 0, 2), kind 1:
 *      BtlChars_OnModelLoaded(side); full load: BtlAiMgr_ResetSide(side), BtlScene_CreateChar(side), relocate the member's
 *      second buffer and make it member->data. Clears the flag.
 *
 * --- BtlLoad_StepObject (flag BATTLE_FLAG_LOAD_OBJECT) ------------------------------------------------
 *   0  file = id + 0xC1C when id >= 0x100, else the character model file as above;
 *      gBtlLoadHandle = BtlRes_Request(0, file, -1, -1)
 *   1  wait; kind 2: BtlChange_NotifyLoaded()
 *   2  not while paused; kind 2 waits for BtlChange_IsReady() (fighter manager request state 4);
 *      kind 0: gBtlLoadObj = BtlObj_Create(2, BtlRes_GetSlot(handle), 1), BtlObjAnim_PlayModel(BtlObj_Get(obj), 0, 2)
 *      kind 2: gBtlLoadObj = BtlObj_Create(job->costume [request word +0x20], BtlRes_GetSlot(handle), 1), BtlCtrl_AttachPartner(side, handle, obj)
 *
 * --- Stage jobs (flag BATTLE_FLAG_LOADING) ---------------------------------------------------------
 *   BtlLoad_StepStageReload: 0 set flag; 1 BtlLoad_BeginStageSwap(), request 0x171/0x198 + rule.curStage into
 *     res->stage and 0x14E + curStage into res->bank; 2 wait, Snd_ReloadBank(8, res->bank, 0); 3 when not paused:
 *     clear flag, BtlLoad_EndStageSwap().
 *   BtlLoad_StepStageChange (in-battle stage change, with fade 2): 0 set flag; 1 request file 0x1BF (when the
 *     new stage is 3) or 0x1C0 (heap) -> res->transition, BtlLoad_BeginStageSwap(); 2 wait; 3 when not
 *     paused EftBurst_Start(); 4 request stage 0x171 + curStage and bank 0x14E + curStage (never the split-screen
 *     file); 5 wait, Snd_ReloadBank(8, bank, 0); 6 when not paused and EftBurst_IsBusy() == 0: Fade_Start(2, 0, 1.0);
 *     7 Fade_IsDone(2): EftBurst_End(); 8 when not paused: clear flag, BtlLoad_EndStageSwap(), free
 *     res->transition; 9 Fade_Start(2, 1, 1.0), done.
 */

/* Battle resources: the 0x50 bytes at gCommonRes + 0x20 (CommonRes.unk20). All fields verified. */
typedef struct BattleRes {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ void *stage;        /* stage model file 0x171 + stage (0x198 + stage in split-screen) */
    /* 0x08 */ void *unk8;         /* file 6 + gProgress->unk0[0] */
    /* 0x0C */ void *bank;         /* sound bank file being (re)loaded: 0x14E + stage, 0xBDA / 0xC7B + character */
    /* 0x10 */ void *script;       /* file 0x1FF + n, only when setup.script != 0 */
    /* 0x14 */ void *transition;   /* file 0x1BF / 0x1C0 during a stage change */
    /* 0x18 */ void *sndCommon;    /* file 0x14A, freed once sent to the sound driver */
    /* 0x1C */ void *sndStage;     /* file 0x14E + stage, same */
    /* 0x20 */ void *sndChara[2];  /* file 0xBDA / 0xC7B + character per side, same */
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 stageSize;      /* 0x6CB800 */
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 bankSize;       /* 0x90800 */
    /* 0x38 */ u8 unk38[0x18];
} BattleRes; /* size 0x50 */

/* File ids used by the loader. */
#define BTL_FILE_CHARA_DATA 0xD48        /* + character * 2 + side: 0x1000-byte per-member block */
#define BTL_FILE_SND_COMMON 0x14A
#define BTL_FILE_SND_STAGE 0x590     /* + stage */
#define BTL_FILE_STAGE 0x5F3         /* + stage */
#define BTL_FILE_STAGE_SPLIT 0x656   /* + stage, split-screen version */
#define BTL_FILE_TRANSITION_3 0x1BF  /* stage change to stage 3 */
#define BTL_FILE_TRANSITION 0x1C0
#define BTL_FILE_SCRIPT 0x1FF        /* + setup.script - 1 */
#define BTL_VOICE_FILE(chara, alt) ((chara) + ((alt) ? BTL_FILE_VOICE_ALT : BTL_FILE_VOICE))
#define BTL_FILE_CHARA 0x590         /* + character * 10 + costume; +4: variant; +8, +9: two more files */
#define BTL_FILE_VOICE 0x1D012         /* + character */
#define BTL_FILE_VOICE_ALT 0x1D10C     /* + character, when gSaveData->flags bit 0 */
#define BTL_FILE_OBJECT 0xC1C         /* SLUS 0x1276F4: extra cinematic model + id >= 0x100 */

#define BTL_STAGE_BUF_SIZE 0x6CB800
#define BTL_BANK_BUF_SIZE 0x90800
#define BTL_MEMBER_BUF_SIZE 0x1000

#define BTL_JOB_COUNT 8

/* BtlJob.kind */
#define BTL_JOB_KIND_RESTART 0 /* pushed by BtlLoad_Reload */
#define BTL_JOB_KIND_CHANGE 1  /* fighter manager request of type 0 (character change) */
#define BTL_JOB_KIND_OBJECT 2  /* fighter manager request of type 1 */

/* One loader job (0x40 bytes). `next`/`step` line up with sys/job.h's Job. */
typedef struct BtlJob {
    /* 0x00 */ SListNode node;                  /* free-list link while in the pool */
    /* 0x04 */ s32 (*step)(struct BtlJob *job); /* returns 1 when finished */
    /* 0x08 */ s32 state;
    /* 0x0C */ s32 kind;       /* BTL_JOB_KIND_*; the stage jobs keep the stage id here, the initial job setup.script - 1 */
    /* 0x10 */ s32 side;
    /* 0x14 */ s32 chara;      /* character (or object id) whose model is loaded */
    /* 0x18 */ s32 animChara;  /* file 0x598 + n * 10; BtlLoad_StepObject: costume */
    /* 0x1C */ s32 unk1C;      /* file 0x599 + n * 10; BtlLoad_StepObject: variant */
    /* 0x20 */ s32 voiceChara; /* voice bank 0xBDA + n */
    /* 0x24 */ s32 costume;    /* BtlLoad_StepObject: first argument of BtlObj_Create */
    /* 0x28 */ s32 variant;    /* non-zero: model file + 4 */
    /* 0x2C */ s32 modelOnly;  /* 1: only the model is replaced (transformation without new voice/data) */
    /* 0x30 */ s32 initial;    /* 1: pushed by BtlLoad_Reload, uses member 0 */
    /* 0x34 */ u8 unk34[0xC];
} BtlJob; /* size 0x40 */

/* gBtlJobPool */
typedef struct BtlJobPool {
    /* 0x000 */ BtlJob jobs[BTL_JOB_COUNT];
    /* 0x200 */ SList free;
} BtlJobPool; /* size 0x20C */

/*
 * What this code verifies of the battle block (battle/battle.h): the whole 0x1A00 bytes are zeroed by
 * Battle_ClearWork; the pool is at +0x5A8, the result block (0x48 bytes, zeroed by BattleResult_Clear) at
 * +0x1938, the event work at +0x1980 and the flags at +0x19F0 (= 0 in Battle_ResetWork; PAUSE tested;
 * LOAD_CHARA / LOAD_OBJECT / LOADING set and cleared by the job step functions and tested by the two stage
 * requests). Result bits read here: winner 1 (side 0 won), 2 (side 1 won), 8 (aborted); reason bits 0 K.O.,
 * 1 time up, 2, 5|6, 15 / 16 (restart; BattleResult_Set forwards them to BattleReplay_SetActive(0 / 1)), 18, 20.
 */

void Battle_ClearWork(void);
void Battle_ResetWork(void);
void Battle_UpdateWork(void);
s32 Battle_Load(void);
s32 Battle_Unload(void);

BtlJobPool *BtlJob_GetPool(void);
void BtlJob_InitPool(void);
BtlJob *BtlJob_Alloc(void);
s32 BtlJob_Free(BtlJob *job);
s32 BtlJob_GetFreeCount(void);

void BtlLoad_BeginStageSwap(void);
void BtlLoad_EndStageSwap(void);
s32 BtlLoad_StepStageReload(BtlJob *job);
s32 BtlLoad_StepStageChange(BtlJob *job);
s32 BtlLoad_StepObject(BtlJob *job);
s32 BtlLoad_StepChara(BtlJob *job);
s32 BtlLoad_StepInitial(BtlJob *job);
s32 BtlLoad_RequestStageReload(s32 stage);
s32 BtlLoad_RequestStageChange(s32 stage);
void BtlLoad_PollObjectRequest(void);
void BtlLoad_PollCharaRequest(void);
void BtlLoad_Reload(void);
void BtlLoad_PushInitialJob(void);
void BtlLoad_FreeAll(void);

void BattleResult_Clear(void);
void BattleResult_Set(s32 flags, s32 reason);
s32 BattleResult_IsAborted(void);
s32 Battle_IsRematchRequested(void);
s32 BattleResult_HasWinner(void);
s32 BattleResult_IsPlayerWin(void);
s32 BattleResult_IsReasonBit20(void);
s32 BattleResult_GetWinnerSide(void);
s32 BattleResult_IsKo(void);
s32 BattleResult_IsTimeUp(void);
s32 BattleResult_IsReasonBit2(void);
s32 BattleResult_IsReasonBit18(void);
s32 BattleResult_IsReasonBit5or6(void);
s32 BattleResult_IsWinnerEvent59Clear(void);
s32 BattleResult_IsEvent3CSet(s32 side);
void BattleResult_CollectEvents(BattleResult *result);

#endif
