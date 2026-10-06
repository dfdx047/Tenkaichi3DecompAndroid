#include "common.h"
#include "battle/battle_work.h"
#include "battle/battle_setup.h"
#include "battle/btl_seq.h"
#include "sys/common.h"
#include "sys/file.h"
#include "sys/heap.h"
#include "sys/job.h"
#include "sys/loading.h"
#include "sys/save.h"

/*
 * Battle loader, result block, events, battle setup and replay block: 0x127120..0x12B570, one object file in
 * the original (its .rodata only lines up as one object: the jump table of BtlLoad_StepStageChange at +0, the
 * time-event table of BtlEvent_RaiseTimeEvents at +0x28, the jump tables of BattleSetup_FixForMode and
 * BattleSetup_FinishEx at +0x80 and +0xA0; original addresses 0x2EC350, 0x2EC378, 0x2EC3D0, 0x2EC3F0).
 *
 *   0x127120..0x129170  loader job pool, the five job step functions, load requests, result accessors
 *                       (what a load does step by step: include/battle/battle_work.h)
 *   0x129170..0x12B570  result finish, per-side event bits, setup builders and getters, replay block
 *                       (include/battle/battle_setup.h)
 * The battle block itself is described in include/battle/battle.h.
 *
 * The battle work accessors at 0x126EC8..0x127120 are a separate translation unit (src/battle/battle_work.c):
 * with Battle_GetWork or Battle_GetEventWork defined earlier in this file, ee-gcc 2.96 fills the branch delay
 * slots of BtlLoad_StepStageReload / StepObject / StepChara / StepInitial differently and they do not match.
 * For the same reason the order of the functions here must stay the original one: the loader functions call
 * the setup getters that are defined after them.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);
extern s32 rand(void);

extern BtlJobPool gBtlJobPool;
extern s32 gBtlLoadHandle; /* handle of the model BtlLoad_StepObject is loading */
extern s32 gBtlLoadObj;    /* object made from it */

/* sys/file.h declares File_Request with two parameters (it ignores a third); every caller here passes the
 * buffer size as a third argument, so the calls go through a cast. */
#define File_Request3(id, buf, size) ((void *(*)(s32, void *, s32))File_Request)(id, buf, size)

extern void Fade_Start(s32 idx, s32 dir, f32 seconds);
extern s32 Fade_IsDone(s32 idx);
extern void Snd_LoadBank(s32 mask, void *data, s32 arg);
extern void Snd_UnloadBank(s32 mask);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern s32 BtlObj_Get(s32 id);

/* Sound driver. */
extern void Snd_Reset(void);                         /* clears the 8 bank slots' transfer state */
extern void Snd_ReloadBank(s32 mask, void *data, s32 arg); /* reloads a bank from a buffer that stays allocated */
extern s32 Snd_IsUploadDone(void);                          /* 1 when no bank slot has a transfer pending */

extern void BtlScene_FreeChar(s32 side);
extern void BtlScene_CreateChar(s32 side);

/* Stage / scene. */
extern void StgModel_InitStage(void);
extern void EftStage_Recreate(void);
extern void EftStage_ResetAll(void);
extern void EftBurst_Start(void);
extern s32 EftBurst_IsBusy(void);
extern void EftBurst_End(void);
extern void StgNav_Rebind(void);
extern void BtlStage_Term(void);
extern void StgFx_SetDisabled(s32 arg);
extern void StgFx_Reset(void);

/* Fighters. */
extern void BtlAiMgr_ResetSide(s32 side);
extern void BtlChars_OnModelLoaded(s32 side);
extern void BtlChars_OnStageLoaded(void);
extern s32 BtlChange_IsPendingType0(s32 side); /* pending request of type 0 for this side */
extern s32 BtlChange_IsPendingType1(s32 side); /* pending request of type 1 for this side */
extern void BtlChange_GetArgs(s32 *a, s32 *b, s32 *c, s32 *d, s32 *e, s32 *f, s32 *g); /* request words +8..+0x20 */
extern void BtlChange_NotifyTaken(void);    /* request taken */
extern void BtlChange_NotifyLoaded(void);    /* request's files are in */
extern s32 BtlChange_IsReady(void);     /* request state == 4 and not paused */
extern void BtlCtrl_AttachPartner(s32 side, s32 handle, s32 obj);
extern s32 BtlSide_GetActiveMember(s32 side); /* member index */
extern s32 BtlCtrl_IsSwitching(s32 side);

/* Battle objects / models. */
extern s32 BtlObj_Create(s32 slot, s32 model, s32 arg);
extern s32 BtlObj_CreateChara(s32 id);
extern void BtlObj_Rebind(s32 objId, s32 id);
extern s32 BtlObj_RequestCharaModel(s32 side, s32 chara, s32 costume, s32 variant);
extern void BtlRes_CommitReloadEx(void);
extern void BtlObj_Init(s32 arg);
extern void BtlObj_Term(void);
extern s32 BtlRes_Request(s32 arg, s32 file, s32 file8, s32 file9);
extern s32 BtlRes_GetSlot(s32 handle);
extern void BtlRes_Reload(s32 id, s32 file, s32 file8, s32 file9);
extern void BtlRes_CommitReload2(void);
extern void BtlObjAnim_PlayAuto(s32 obj, s32 arg, s32 arg2);
extern void BtlObjAnim_PlayModel(s32 obj, s32 arg, s32 arg2);

/* Script / message objects. */
extern void Gsc_InitDefault(s32 *tbl);
extern void Gsc_Exit(void);
extern s32 Gsc_LoadFile(void *data);
extern void Gsc_UnloadFile(s32 script);
extern s32 Gsc_StartMain(s32 script);
extern void Gsc_RunAction(s32 script, s32 arg);
extern void BtlScript_Init(void);
extern void BtlScript_Nop(void);
extern void BtlScript_ScanEvents(s32 script);
extern void BtlScript_Restart(void);
extern void BtlScript_Free(void);

#define BATTLE_RES ((BattleRes *)((u8 *)gCommonRes + 0x20))

/* Returns the loader job pool. */
BtlJobPool *BtlJob_GetPool(void) {
    return &gBtlJobPool;
}

/* Clears the job pool and puts all 8 jobs on its free list. */
void BtlJob_InitPool(void) {
    BtlJobPool *pool = BtlJob_GetPool();
    s32 i;

    memset(pool, 0, sizeof(BtlJobPool));
    for (i = 0; i < BTL_JOB_COUNT; i++) {
        SList_PushFront(&pool->free, &pool->jobs[i].node);
    }
}

/* Takes a zeroed job from the pool; NULL when all 8 are in use. */
BtlJob *BtlJob_Alloc(void) {
    BtlJob *job = (BtlJob *)SList_PopFront(&BtlJob_GetPool()->free);

    if (job == NULL) {
        return NULL;
    }
    memset(job, 0, sizeof(BtlJob));
    return job;
}

/* Gives a job back to the pool. */
s32 BtlJob_Free(BtlJob *job) {
    BtlJobPool *pool = BtlJob_GetPool();

    if (job != NULL) {
        SList_PushFront(&pool->free, &job->node);
        return 1;
    }
    return 0;
}

/* Number of jobs not in use. */
s32 BtlJob_GetFreeCount(void) {
    return SList_GetCount(&BtlJob_GetPool()->free);
}

/* Shuts the stage-dependent systems down before the stage file is replaced. */
void BtlLoad_BeginStageSwap(void) {
    EftStage_ResetAll();
    BtlStage_Term();
    StgFx_SetDisabled(1);
}

/* Brings them back up on the new stage. */
void BtlLoad_EndStageSwap(void) {
    StgModel_InitStage();
    BtlChars_OnStageLoaded();
    StgNav_Rebind();
    EftStage_Recreate();
    StgFx_Reset();
}

/* Job: reloads the stage model and stage sound bank (no transition). */
s32 BtlLoad_StepStageReload(BtlJob *job) {
    BattleRes *res = BATTLE_RES;
    s32 id;

    switch (job->state) {
    case 0:
        Battle_GetWork()->flags |= BATTLE_FLAG_LOADING;
        job->state++;
        break;
    case 1:
        BtlLoad_BeginStageSwap();
#ifdef PORT
        /* PC build: a stage added from outside the disc (id 0x24 on) has one model only, and the file id its
           split-screen model would have belongs to other files (the menu archives, the stage transitions): it
           is loaded for split screen too. The disc's own stages keep their lighter split-screen model. */
        if (Battle_GetStage() >= 0x24) {
            id = Battle_GetStage() + BTL_FILE_STAGE;
        } else
#endif
        if (Battle_IsSplitScreen()) {
            id = Battle_GetStage() + BTL_FILE_STAGE_SPLIT;
        } else {
            id = Battle_GetStage() + BTL_FILE_STAGE;
        }
        res->stage = File_Request3(id, res->stage, res->stageSize);
        res->bank = File_Request3(Battle_GetStage() + BTL_FILE_SND_STAGE, res->bank, res->bankSize);
        job->state++;
        return 0;
    case 2:
        if (!File_UpdateRequests()) {
            return 0;
        }
        Snd_ReloadBank(8, res->bank, 0);
        job->state++;
        break;
    case 3:
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
            return 0;
        }
        Battle_GetWork()->flags &= ~BATTLE_FLAG_LOADING;
        BtlLoad_EndStageSwap();
        BtlJob_Free(job);
        return 1;
    default:
        return 1;
    }
    return 0;
}

/* Job: in-battle stage change, with the transition scene and a fade out / in. */
s32 BtlLoad_StepStageChange(BtlJob *job) {
    BattleRes *res = BATTLE_RES;

    switch (job->state) {
    case 0:
        Battle_GetWork()->flags |= BATTLE_FLAG_LOADING;
        job->state++;
        break;
    case 1:
        if (job->kind == 3) {
            res->transition = File_Request3(BTL_FILE_TRANSITION_3, NULL, 0);
        } else {
            res->transition = File_Request3(BTL_FILE_TRANSITION, NULL, 0);
        }
        BtlLoad_BeginStageSwap();
        job->state++;
        break;
    case 2:
        if (!File_UpdateRequests()) {
            return 0;
        }
        job->state++;
        break;
    case 3:
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
            return 0;
        }
        EftBurst_Start();
        job->state++;
        return 0;
    case 4:
        res->stage = File_Request3(Battle_GetStage() + BTL_FILE_STAGE, res->stage, res->stageSize);
        res->bank = File_Request3(Battle_GetStage() + BTL_FILE_SND_STAGE, res->bank, res->bankSize);
        job->state++;
        return 0;
    case 5:
        if (!File_UpdateRequests()) {
            return 0;
        }
        Snd_ReloadBank(8, res->bank, 0);
        job->state++;
        /* fall through */
    case 6:
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
            return 0;
        }
        if (EftBurst_IsBusy()) {
            return 0;
        }
        Fade_Start(2, 0, 1.0f);
        job->state++;
        break;
    case 7:
        if (Fade_IsDone(2)) {
            EftBurst_End();
            job->state++;
        }
        break;
    case 8:
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
            return 0;
        }
        Battle_GetWork()->flags &= ~BATTLE_FLAG_LOADING;
        BtlLoad_EndStageSwap();
        if (res->transition != NULL) {
            Heap_Free(res->transition);
            res->transition = NULL;
        }
        job->state++;
        break;
    case 9:
        Fade_Start(2, 1, 1.0f);
        BtlJob_Free(job);
        return 1;
    default:
        return 1;
    }
    return 0;
}

/* Job: loads one extra model and makes a battle object from it. */
s32 BtlLoad_StepObject(BtlJob *job) {
    s32 id;
    s32 ready;

    switch (job->state) {
    case 0:
        Battle_GetWork()->flags |= BATTLE_FLAG_LOAD_OBJECT;
        id = job->chara + BTL_FILE_OBJECT;
        if (job->chara < 0x100) {
            if (job->unk1C != 0) {
                id = job->chara * 10 + job->animChara + (BTL_FILE_CHARA + 4);
            } else {
                id = job->chara * 10 + job->animChara + BTL_FILE_CHARA;
            }
        }
        gBtlLoadHandle = BtlRes_Request(0, id, -1, -1);
        job->state++;
        return 0;
    case 1:
        if (!File_UpdateRequests()) {
            return 0;
        }
        switch (job->kind) {
        case 0:
            break;
        case BTL_JOB_KIND_OBJECT:
            BtlChange_NotifyLoaded();
            break;
        }
        job->state++;
        break;
    case 2:
        ready = 1;
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
            return 0;
        }
        switch (job->kind) {
        case 0:
            break;
        case BTL_JOB_KIND_OBJECT:
            ready = BtlChange_IsReady();
            break;
        }
        if (!ready) {
            return 0;
        }
        switch (job->kind) {
        case 0:
            gBtlLoadObj = BtlObj_Create(2, BtlRes_GetSlot(gBtlLoadHandle), 1);
            BtlObjAnim_PlayModel(BtlObj_Get(gBtlLoadObj), 0, 2);
            break;
        case BTL_JOB_KIND_OBJECT:
            gBtlLoadObj = BtlObj_Create(job->costume, BtlRes_GetSlot(gBtlLoadHandle), 1);
            BtlCtrl_AttachPartner(job->side, gBtlLoadHandle, gBtlLoadObj);
            break;
        }
        Battle_GetWork()->flags &= ~BATTLE_FLAG_LOAD_OBJECT;
        BtlJob_Free(job);
        return 1;
    default:
        return 1;
    }
    return 0;
}

/* Job: loads a side's character (model, voice bank, data block) and swaps it in. */
s32 BtlLoad_StepChara(BtlJob *job) {
    BattleRes *res = BATTLE_RES;
    s32 model;
    s32 voice;
    s32 file8;
    s32 file9;
    s32 member;
    s32 ready;
    BattleMember *m;

    switch (job->state) {
    case 0:
        if (job->variant != 0) {
            model = job->chara * 10 + job->costume + (BTL_FILE_CHARA + 4);
        } else {
            model = job->chara * 10 + job->costume + BTL_FILE_CHARA;
        }
        if (job->modelOnly == 0) {
            file8 = job->animChara * 10 + (BTL_FILE_CHARA + 8);
            file9 = job->unk1C * 10 + (BTL_FILE_CHARA + 9);
            voice = job->voiceChara + ((gSaveData->flags & SAVE_FLAG_VOICE) ? BTL_FILE_VOICE_ALT : BTL_FILE_VOICE);
            BtlRes_Reload(BattleSide_GetModelSlot(job->side), model, file8, file9);
            res->bank = File_Request3(voice, res->bank, res->bankSize);
            if (!BtlCtrl_IsSwitching(job->side)) {
                BattleMember *next;

                member = 0;
                if (job->initial == 0) {
                    member = BtlSide_GetActiveMember(job->side);
                }
                next = BattleSide_GetMember(job->side, member);
                next->buf[1] = File_Request3(job->chara * 2 + job->side + BTL_FILE_CHARA_DATA, next->buf[1], BTL_MEMBER_BUF_SIZE);
            }
        } else {
            BtlRes_Reload(BattleSide_GetModelSlot(job->side), model, -1, -1);
        }
        Battle_GetWork()->flags |= BATTLE_FLAG_LOAD_CHARA;
        job->state++;
        break;
    case 1:
        if (!File_UpdateRequests()) {
            return 0;
        }
        switch (job->kind) {
        case 0:
            break;
        case BTL_JOB_KIND_CHANGE:
            BtlChange_NotifyLoaded();
            break;
        }
        job->state++;
        break;
    case 2:
        ready = 1;
        switch (job->kind) {
        case 0:
            break;
        case BTL_JOB_KIND_CHANGE:
            ready = BtlChange_IsReady();
            break;
        }
        if (!ready) {
            break;
        }
        job->state++;
        BtlRes_CommitReload2();
        if (job->modelOnly == 0) {
            if (job->side == 0) {
                Snd_ReloadBank(0x10, res->bank, 0);
            } else {
                Snd_ReloadBank(0x20, res->bank, 0);
            }
        }
        BtlObj_Rebind(BattleSide_GetObjId(job->side), BattleSide_GetModelSlot(job->side));
        switch (job->kind) {
        case 0:
            BtlObjAnim_PlayAuto(BtlObj_Get(BattleSide_GetObjId(job->side)), 0, 2);
            break;
        case BTL_JOB_KIND_CHANGE:
            BtlChars_OnModelLoaded(job->side);
            break;
        }
        if (job->modelOnly == 0) {
            BtlAiMgr_ResetSide(job->side);
            BtlScene_CreateChar(job->side);
            if (!BtlCtrl_IsSwitching(job->side)) {
                m = BattleSide_GetMember(job->side, BtlSide_GetActiveMember(job->side));
                if (m != NULL) {
                    Res_RelocateOffsets(&m->buf[1], m->buf[1], m->buf[1]);
                    m->data = m->buf[1];
                }
            }
        }
        BtlJob_Free(job);
        Battle_GetWork()->flags &= ~BATTLE_FLAG_LOAD_CHARA;
        return 1;
    default:
        return 1;
    }
    return 0;
}

/* Job: the first load of a battle. */
s32 BtlLoad_StepInitial(BtlJob *job) {
    BattleRes *res = BATTLE_RES;
    BattleMember *m;
    BattleEvents *ev;
    s32 side;
    s32 i;
    s32 id;

    switch (job->state) {
    case 0:
        res->sndCommon = File_Request3(BTL_FILE_SND_COMMON, NULL, 0);
        res->sndStage = File_Request3(Battle_GetStartStage() + BTL_FILE_SND_STAGE, NULL, 0);
        res->sndChara[0] = File_Request3(BattleSide_GetStartChara(0) + ((gSaveData->flags & SAVE_FLAG_VOICE) ? BTL_FILE_VOICE_ALT : BTL_FILE_VOICE), NULL, 0);
        res->sndChara[1] = File_Request3(BattleSide_GetStartChara(1) + ((gSaveData->flags & SAVE_FLAG_VOICE) ? BTL_FILE_VOICE_ALT : BTL_FILE_VOICE), NULL, 0);
        job->state++;
        return 0;
    case 1:
        if (!File_UpdateRequests()) {
            return 0;
        }
        job->state++;
        /* fall through */
    case 2:
        Snd_LoadBank(4, res->sndCommon, 0);
        Snd_LoadBank(8, res->sndStage, 0);
        Snd_LoadBank(0x10, res->sndChara[0], 0);
        Snd_LoadBank(0x20, res->sndChara[1], 0);
        job->state++;
        break;
    case 3:
        if (!Snd_IsUploadDone()) {
            break;
        }
        Heap_Free(res->sndCommon);
        res->sndCommon = NULL;
        Heap_Free(res->sndStage);
        res->sndStage = NULL;
        Heap_Free(res->sndChara[0]);
        res->sndChara[0] = NULL;
        Heap_Free(res->sndChara[1]);
        res->sndChara[1] = NULL;
        job->state++;
        break;
    case 4:
        BattleSide_SetModelSlot(0, BtlObj_RequestCharaModel(0, BattleSide_GetStartChara(0), BattleSide_GetStartCostume(0), BattleSide_GetStartVariant(0)));
        BattleSide_SetModelSlot(1, BtlObj_RequestCharaModel(1, BattleSide_GetStartChara(1), BattleSide_GetStartCostume(1), BattleSide_GetStartVariant(1)));
        res->stageSize = BTL_STAGE_BUF_SIZE;
        res->stage = Heap_Alloc(res->stageSize, 0x40, 0, HEAP_ANY);
        memset(res->stage, 0, res->stageSize);
        res->bankSize = BTL_BANK_BUF_SIZE;
        res->bank = Heap_Alloc(res->bankSize, 0x40, 0, HEAP_ANY);
        memset(res->bank, 0, res->bankSize);
        for (side = 0; side < 2; side++) {
            for (i = 0; i < 5; i++) {
                m = BattleSide_GetMember(side, i);
                m->buf[0] = Heap_Alloc(BTL_MEMBER_BUF_SIZE, 0x40, 0, HEAP_ANY);
                m->buf[1] = Heap_Alloc(BTL_MEMBER_BUF_SIZE, 0x40, 0, HEAP_ANY);
                m->data = m->buf[0];
                memset(m->buf[0], 0, BTL_MEMBER_BUF_SIZE);
                memset(m->buf[1], 0, BTL_MEMBER_BUF_SIZE);
            }
        }
#ifdef PORT
        /* PC build: see the note in the stage-swap job: an added stage has one model, for split screen too. */
        if (Battle_GetStartStage() >= 0x24) {
            id = Battle_GetStartStage() + BTL_FILE_STAGE;
        } else
#endif
        if (Battle_IsSplitScreen()) {
            id = Battle_GetStartStage() + BTL_FILE_STAGE_SPLIT;
        } else {
            id = Battle_GetStartStage() + BTL_FILE_STAGE;
        }
        res->stage = File_Request3(id, res->stage, res->stageSize);
        for (side = 0; side < 2; side++) {
            for (i = 0; (u32)i < (u32)BattleSide_GetMemberCount(side); i++) {
                m = BattleSide_GetMember(side, i);
                m->buf[0] = File_Request3(m->chara * 2 + side + BTL_FILE_CHARA_DATA, m->buf[0], BTL_MEMBER_BUF_SIZE);
            }
        }
        res->unk8 = File_Request3(gProgress->unk0[0] + 6, NULL, 0);
        job->state++;
        break;
    case 5:
        if (!File_UpdateRequests()) {
            return 0;
        }
        for (side = 0; side < 2; side++) {
            for (i = 0; (u32)i < (u32)BattleSide_GetMemberCount(side); i++) {
                m = BattleSide_GetMember(side, i);
                Res_RelocateOffsets(&m->buf[0], m->buf[0], m->buf[0]);
            }
        }
        BtlJob_Free(job);
        return 1;
    case 0x5A:
        res->script = File_Request3(job->kind + BTL_FILE_SCRIPT, NULL, 0);
        job->state++;
        return 0;
    case 0x5B:
        if (!File_UpdateRequests()) {
            return 0;
        }
        ev = Battle_GetEventWork();
        ev->script = Gsc_LoadFile(res->script);
        Gsc_RunAction(ev->script, 1000);
        BtlScript_ScanEvents(ev->script);
        BtlScript_Nop();
        BattleSetup_Finish();
        job->state = 0;
        break;
    default:
        return 1;
    }
    return 0;
}

/* Starts a plain stage reload unless a load is running or the stage is already loaded. */
s32 BtlLoad_RequestStageReload(s32 stage) {
    BtlJob *job;

    if ((Battle_GetWork()->flags & BATTLE_FLAG_LOAD_CHARA) || (Battle_GetWork()->flags & BATTLE_FLAG_LOAD_OBJECT) ||
        (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) || Battle_GetStage() == stage) {
        return 0;
    }
    job = BtlJob_Alloc();
    memset(job, 0, sizeof(BtlJob));
    job->step = BtlLoad_StepStageReload;
    job->state = 0;
    job->kind = stage;
    Battle_SetStage(stage);
    Job_Push((Job *)job);
    return 1;
}

/* Starts an in-battle stage change (with fade) under the same conditions. */
s32 BtlLoad_RequestStageChange(s32 stage) {
    BtlJob *job;

    if ((Battle_GetWork()->flags & BATTLE_FLAG_LOAD_CHARA) || (Battle_GetWork()->flags & BATTLE_FLAG_LOAD_OBJECT)) {
        return 0;
    }
    if ((Battle_GetWork()->flags & BATTLE_FLAG_LOADING) || (stage >= 0 && Battle_GetStage() == stage)) {
        return 0;
    }
    if (stage < 0) {
        return 0;
    }
    job = BtlJob_Alloc();
    memset(job, 0, sizeof(BtlJob));
    job->step = BtlLoad_StepStageChange;
    job->state = 0;
    job->kind = stage;
    Battle_SetStage(stage);
    Job_Push((Job *)job);
    Fade_Start(2, 1, 1.0f);
    return 1;
}

/* Per frame: turns a pending type-1 request of the fighter manager into a BtlLoad_StepObject job. */
void BtlLoad_PollObjectRequest(void) {
    s32 id;
    s32 costume;
    s32 variant;
    s32 slot;
    s32 side;
    BtlJob *job;

    for (side = 0; side < 2; side++) {
        if (BtlChange_IsPendingType1(side)) {
            BtlChange_GetArgs(&id, &costume, &variant, NULL, NULL, NULL, &slot);
            job = BtlJob_Alloc();
            memset(job, 0, sizeof(BtlJob));
            job->step = BtlLoad_StepObject;
            job->side = side;
            job->kind = BTL_JOB_KIND_OBJECT;
            job->costume = slot;
            job->chara = id;
            job->animChara = costume;
            job->unk1C = variant;
            Job_Push((Job *)job);
            BtlChange_NotifyTaken();
        }
    }
}

/* Per frame: turns a pending type-0 request (character change) into a BtlLoad_StepChara job. */
void BtlLoad_PollCharaRequest(void) {
    s32 chara;
    s32 costume;
    s32 variant;
    s32 animChara;
    s32 unk1C;
    s32 voiceChara;
    s32 side;
    s32 modelOnly;
    BtlJob *job;

    for (side = 0; side < 2; side++) {
        if (BtlChange_IsPendingType0(side)) {
            BtlChange_GetArgs(&chara, &costume, &variant, &animChara, &unk1C, &voiceChara, NULL);
            if (animChara < 0 && unk1C < 0 && voiceChara < 0) {
                modelOnly = 1;
            } else {
                modelOnly = 0;
            }
            job = BtlJob_Alloc();
            memset(job, 0, sizeof(BtlJob));
            job->side = side;
            job->kind = BTL_JOB_KIND_CHANGE;
            job->step = BtlLoad_StepChara;
            job->chara = chara;
            job->animChara = animChara;
            job->unk1C = unk1C;
            job->voiceChara = voiceChara;
            job->costume = costume;
            job->variant = variant;
            job->modelOnly = modelOnly;
            job->initial = 0;
            if (modelOnly == 0) {
                BtlScene_FreeChar(side);
            }
            BattleSide_SetForm(job->side, job->chara, job->costume, job->variant);
            Job_Push((Job *)job);
            BtlChange_NotifyTaken();
        }
    }
}

/* Restart: drops running jobs and reloads the initial stage / characters where they changed (blocking). */
void BtlLoad_Reload(void) {
    BattleRes *res = BATTLE_RES;
    s32 side;
    s32 i;
    s32 chara;
    s32 costume;
    s32 variant;
    BtlJob *job;
    BattleMember *m;

    if (res != NULL && res->transition != NULL) {
        Battle_GetWork()->flags &= ~BATTLE_FLAG_LOADING;
        Heap_Free(res->transition);
        res->transition = NULL;
    }
    BtlRes_CommitReloadEx();
    Job_Clear();
    BtlJob_InitPool();
    /* The original passes a second argument (0) that the function does not have. */
    ((s32 (*)(s32, s32))BtlLoad_RequestStageReload)(Battle_GetStartStage(), 0);
    for (side = 0; side < 2; side++) {
        if (BattleSide_IsFormChanged(side)) {
            chara = BattleSide_GetStartChara(side);
            costume = BattleSide_GetStartCostume(side);
            variant = BattleSide_GetStartVariant(side);
            BattleSide_SetForm(side, chara, costume, variant);
            job = BtlJob_Alloc();
            memset(job, 0, sizeof(BtlJob));
            job->step = BtlLoad_StepChara;
            job->side = side;
            job->kind = BTL_JOB_KIND_RESTART;
            job->chara = chara;
            job->animChara = chara;
            job->unk1C = chara;
            job->voiceChara = chara;
            job->costume = costume;
            job->variant = variant;
            job->modelOnly = 0;
            job->initial = 1;
            BtlScene_FreeChar(side);
            Job_Push((Job *)job);
        } else {
            BtlObj_Rebind(BattleSide_GetObjId(side), BattleSide_GetModelSlot(side));
        }
    }
    if (BtlJob_GetFreeCount() != BTL_JOB_COUNT) {
        Load_RunBlocking();
    }
    for (side = 0; side < 2; side++) {
        for (i = 0; (u32)i < (u32)BattleSide_GetMemberCount(side); i++) {
            m = BattleSide_GetMember(side, i);
            m->data = m->buf[0];
        }
    }
}

/* Resets the job pool and queues the first load. */
void BtlLoad_PushInitialJob(void) {
    BtlJob *job;

    BtlJob_InitPool();
    job = BtlJob_Alloc();
    memset(job, 0, sizeof(BtlJob));
    job->step = BtlLoad_StepInitial;
    job->kind = Battle_GetScript();
    if (job->kind >= 0) {
        job->state = 0x5A;
    }
    Job_Push((Job *)job);
}

/* Frees every buffer of the first load and unloads the four battle sound banks. */
void BtlLoad_FreeAll(void) {
    BattleRes *res = BATTLE_RES;
    BattleEvents *ev = Battle_GetEventWork();
    BattleMember *m;
    s32 side;
    s32 i;

    if (Battle_GetMode() == 1) {
        Gsc_UnloadFile(ev->script);
        Heap_Free(res->script);
        res->script = NULL;
    }
    for (side = 0; side < 2; side++) {
        for (i = 0; i < 5; i++) {
            m = BattleSide_GetMember(side, i);
            Heap_Free(m->buf[0]);
            m->buf[0] = NULL;
            Heap_Free(m->buf[1]);
            m->buf[1] = NULL;
        }
    }
    Heap_Free(res->bank);
    res->bank = NULL;
    Heap_Free(res->unk8);
    res->unk8 = NULL;
    Heap_Free(res->stage);
    res->stage = NULL;
    if (res != NULL && res->transition != NULL) {
        Battle_GetWork()->flags &= ~BATTLE_FLAG_LOADING;
        Heap_Free(res->transition);
        res->transition = NULL;
    }
    Snd_Reset();
    Snd_UnloadBank(4);
    Snd_UnloadBank(8);
    Snd_UnloadBank(0x10);
    Snd_UnloadBank(0x20);
}

/* Zeroes the result block. */
void BattleResult_Clear(void) {
    memset(Battle_GetResult(), 0, sizeof(BattleResult));
}

/* Stores the winner bits and the reason bits; a restart reason is forwarded to BattleReplay_SetActive. */
void BattleResult_Set(s32 flags, s32 reason) {
    BattleResult *result = Battle_GetResult();

    result->winner = flags;
    result->reason = reason;
    if (reason & 0x8000) {
        BattleReplay_SetActive(0);
    }
    if (reason & 0x10000) {
        BattleReplay_SetActive(1);
    }
}

/* The battle was left without a finish (winner bit 3). */
s32 BattleResult_IsAborted(void) {
    if (Battle_GetResult()->winner & 8) {
        return 1;
    }
    return 0;
}

/* A restart was asked for (reason bits 15 / 16). */
s32 Battle_IsRematchRequested(void) {
    return (Battle_GetResult()->reason & 0x18000) != 0;
}

/* One of the sides won. */
s32 BattleResult_HasWinner(void) {
    return (Battle_GetResult()->winner & 3) != 0;
}

/* Side 0 won; always 1 in split-screen and in modes 8 and 1. */
s32 BattleResult_IsPlayerWin(void) {
    BattleResult *result = Battle_GetResult();

    if (Battle_IsSplitScreen() || Battle_GetMode() == 8 || Battle_GetMode() == 1) {
        return 1;
    }
    if (result->winner & 1) {
        return 1;
    }
    return 0;
}

/* Reason bit 20. */
s32 BattleResult_IsReasonBit20(void) {
    return (Battle_GetResult()->reason >> 20) & 1;
}

/* Index of the winning side: 1 only when side 1 won and side 0 did not. */
s32 BattleResult_GetWinnerSide(void) {
    s32 winner = Battle_GetResult()->winner;

    if (winner & 1) {
        return 0;
    }
    if (winner & 2) {
        return 1;
    }
    return 0;
}

/* Reason bit 0: K.O. */
s32 BattleResult_IsKo(void) {
    return Battle_GetResult()->reason & 1;
}

/* Reason bit 1: time up. */
s32 BattleResult_IsTimeUp(void) {
    return (Battle_GetResult()->reason >> 1) & 1;
}

/* Reason bit 2. */
s32 BattleResult_IsReasonBit2(void) {
    return (Battle_GetResult()->reason >> 2) & 1;
}

/* Reason bit 18. */
s32 BattleResult_IsReasonBit18(void) {
    return (Battle_GetResult()->reason >> 18) & 1;
}

/* Reason bits 5 or 6. */
s32 BattleResult_IsReasonBit5or6(void) {
    return (Battle_GetResult()->reason & 0x60) != 0;
}

/* Event 0x59 is clear in the winning side's event set. */
s32 BattleResult_IsWinnerEvent59Clear(void) {
    return BtlEvent_WasRaised(BattleResult_GetWinnerSide(), 0x59) == 0;
}

/* Event 0x3C is set in the given event set. */
s32 BattleResult_IsEvent3CSet(s32 side) {
    return BtlEvent_WasRaised(side, 0x3C) != 0;
}

#define RESULT_EVENT(ev, bit) \
    if (BtlEvent_WasRaised(0, ev)) { \
        result->eventSummary |= (bit); \
    }

/* Rebuilds the result's 64-bit event summary from event set 0. */
void BattleResult_CollectEvents(BattleResult *result) {
    u32 i;
    u32 count;
    u16 sum;
    s32 j;
    BattleMember *m;

    result->eventSummary = 0;
    RESULT_EVENT(0x51, 1);
    RESULT_EVENT(0x52, 2);
    RESULT_EVENT(0x53, 4);
    if (!BtlEvent_WasRaised(0, 0x59)) {
        result->eventSummary |= 8;
    }
    RESULT_EVENT(0x54, 0x10);
    if (BtlEvent_WasRaised(0, 0x37) && BtlEvent_WasRaised(0, 0x38) && BtlEvent_WasRaised(0, 0x39) && BtlEvent_WasRaised(0, 0x3A) &&
        BtlEvent_WasRaised(0, 0x3B)) {
        result->eventSummary |= 0x100;
    }
    RESULT_EVENT(0x55, 0x200);
    RESULT_EVENT(0x57, 0x400);
    RESULT_EVENT(0x34, 0x2000);
    RESULT_EVENT(0x3E, 0x8000);
    RESULT_EVENT(0x32, 0x10000);
    RESULT_EVENT(0x3F, 0x20000);
    RESULT_EVENT(0x40, 0x40000);
    RESULT_EVENT(0x56, 0x80000);
    RESULT_EVENT(0x36, 0x1000000);
    RESULT_EVENT(0x24, 0x2000000);
    RESULT_EVENT(0x25, 0x2000000);
    RESULT_EVENT(0x26, 0x4000000);
    RESULT_EVENT(0x27, 0x4000000);
    RESULT_EVENT(0x28, 0x8000000);
    RESULT_EVENT(0x1F, 0x10000000);
    RESULT_EVENT(0x33, 1ULL << 31);
    RESULT_EVENT(0x41, 1ULL << 32);
    RESULT_EVENT(0x45, 1ULL << 33);
    RESULT_EVENT(0x42, 1ULL << 34);
    RESULT_EVENT(0x43, 1ULL << 35);
    RESULT_EVENT(0x44, 1ULL << 36);
    if (!BtlEvent_WasRaised(0, 0x22)) {
        result->eventSummary |= 1ULL << 37;
    }
    RESULT_EVENT(0x58, 1ULL << 39);
    if (!BtlEvent_WasRaised(0, 0x4E) && (u32)BattleSide_GetMemberCount(0) >= 2) {
        result->eventSummary |= 1ULL << 40;
    }
    RESULT_EVENT(0x21, 1ULL << 41);
    RESULT_EVENT(0x3C, 1ULL << 42);
    RESULT_EVENT(0x1E, 1ULL << 44);
    count = BattleSide_GetMemberCount(0);
    sum = 0;
    for (i = 0; i < count; i++) {
        m = BattleSide_GetMember(0, i);
        if (m != NULL) {
            for (j = 0; j < 8; j++) {
                sum += m->items.id[j];
            }
        }
    }
    if (sum == 0) {
        result->eventSummary |= 1ULL << 45;
    }
    RESULT_EVENT(0x46, 1ULL << 46);
    RESULT_EVENT(0x47, 1ULL << 47);
}

/*
 * ---------------------------------------------------------------------------------------------------------
 * Second half, 0x129170..0x12B570: result finish, the per-side event bits, the battle setup builders and
 * getters, and the replay block. include/battle/battle_setup.h describes how they are used.
 * ---------------------------------------------------------------------------------------------------------
 */

/* Other modules called by the setup half. */
s32 BtlFacade_AreBothInterruptible(void);  /* BtlCtrl_IsInterruptible(0) && BtlCtrl_IsInterruptible(1): both fighters ready */
void BtlFacade_SetFlag200(void); /* battle flags |= 0x200 */
void BtlFacade_ClearFlag200(void); /* battle flags &= ~0x200 */
void BtlFacade_ClearFixedCamera(void); /* tail call of DemoCam_ClearFixed (camera module) */
void BtlScript_StartWaitEvents(void); /* script module: reacts to events 0x5A / 0x5B */
void Hud_SlideOut(f32 t); /* five HUD parts (0x21FAF0, 0x21AD40, 0x22F2F0, 0x22E0B0, 0x2240A0), duration t */
void Hud_SlideIn(f32 t); /* their counterparts (0x21FB18, ...), duration t */
/* Sums the four stat bonuses and ORs the four ability words of the items; stats[4] = AI type. */
void ItemSet_GetStats(BattleItemSet *items, s32 *stats, s32 *ability, s32 chara);

#define SETUP() Battle_GetSetup()

/*
 * Side and member lookups. The original computes `setup + side * 0x270` first and adds the field offset
 * last, which only comes out when the setup pointer and the element pointer are locals (as here);
 * `SETUP()->sides[n].field` written in one expression adds the operands the other way round.
 */
static inline BattleSide *Side(s32 n) {
    BattleSetup *setup = SETUP();

    return &setup->sides[n];
}

static inline BattleMember *Member(s32 n, s32 i) {
    BattleSide *side = Side(n);

    return &side->members[i];
}

/* Adds one to result.frames on the frame after event 0x48 of side 0 is newly raised. */
void BattleResult_CountFrame(void) {
    BattleResult *res = Battle_GetResult();

    if (BtlEvent_IsNew(0, 0x48)) {
        res->frames++;
    }
}

/* Empty. */
void BattleResult_Stub1291B8(void) {
}

/* Fills the result block at the end of the battle: event summary, clock copy, abort flag, forced winner. */
void BattleResult_Finish(void) {
    BattleResult *res = Battle_GetResult();

    BattleResult_CollectEvents(res);
    res->clock = *BtlSeq_GetClock();
    if (res->winner & 8) {
        res->unk8 = 1;
    }
    if (res->reason & 0x20) {
        res->winner |= 2;
    }
    if (res->reason & 0x40) {
        res->winner |= 1;
    }
}

/* Returns result.flags (winner / draw / abort bits). */
s32 BattleResult_GetFlags(void) {
    return Battle_GetResult()->winner;
}

/* Returns result.reason. */
s32 BattleResult_GetReason(void) {
    return Battle_GetResult()->reason;
}

/* Returns result.eventSummary (64-bit). */
u64 BattleResult_GetEventSummary(void) {
    return Battle_GetResult()->eventSummary;
}

/* Returns the result block (wrapper of Battle_GetResult). */
BattleResult *BattleResult_GetPtr(void) {
    return Battle_GetResult();
}

/* Converts a clock to whole seconds. */
s32 BtlClock_ToSeconds(BtlClock *clock) {
    return clock->hours * 3600 + clock->minutes * 60 + clock->seconds;
}

/* Raises time event i (0..18) on both sides every frame once the sub clock has reached tbl[i] seconds. */
void BtlEvent_RaiseTimeEvents(void) {
    s32 tbl[19] = { 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, 90, 100, 120, 140, 160, 180 };
    s32 sec;
    u32 i;

    sec = BtlClock_ToSeconds(BtlSeq_GetSubClock());
    for (i = 0; i < 19; i++) {
        if (sec >= tbl[i]) {
            BtlEvent_Raise(0, i);
            BtlEvent_Raise(1, i);
        }
    }
}

/* Per-frame handling of the pending interrupt (event 0x4C), the wait flag (events 0x4D / 0x4E) and event 0x4F. */
void BtlEvent_UpdateRequests(void) {
    BattleEvents *ev = Battle_GetEventWork();
    s32 i;

    if (ev->interrupt != 0) {
        if (BtlFacade_AreBothInterruptible() != 0) {
            BtlEvent_Raise(0, 0x4C);
            BtlEvent_Raise(1, 0x4C);
            BtlFacade_ClearFlag200();
            ev->interrupt = 0;
        }
    }
    if (ev->waitFlag == 1) {
        if (BtlEvent_IsNew(0, 0x4D) || BtlEvent_IsNew(1, 0x4D) || BtlEvent_IsNew(0, 0x4E) || BtlEvent_IsNew(1, 0x4E)) {
            ev->waitFlag = 0;
        }
    }
    for (i = 0; i < 2; i++) {
        if (BtlEvent_IsNew(i, 0x4F)) {
            BtlFacade_ClearFixedCamera();
        }
    }
}

/* End of frame for one side: prev = now, held |= now, now = 0. */
void BtlEventSet_Rotate(BattleEventSet *set) {
    s32 i;

    for (i = 0; i < 2; i++) {
        set->prev[i] = set->now[i];
        set->held[i] |= set->now[i];
        set->now[i] = 0;
    }
}

/* Sets bit n of a 64-bit word array. */
void BtlEvent_SetBit(u64 *bits, s32 n) {
    u64 mask = 1;

    bits += n / 64;
    *bits |= mask << (n % 64);
}

/* 1 when event n is raised in `now` and was not in `prev`. */
s32 BtlEventSet_IsNew(BattleEventSet *set, s32 n) {
    return ((set->now[n / 64] & ~set->prev[n / 64]) >> (n % 64)) & 1;
}

/* 1 when event n was raised at any time since the last reset. */
s32 BtlEventSet_WasRaised(BattleEventSet *set, s32 n) {
    return (set->held[n / 64] >> (n % 64)) & 1;
}

/* Zeroes the whole event work. */
void BtlEvent_ClearAll(void) {
    memset(Battle_GetEventWork(), 0, sizeof(BattleEvents));
}

/* Zeroes the event work but keeps its first word (the script handle). */
void BtlEvent_Reset(void) {
    s32 keep = Battle_GetEventWork()->script;

    BtlEvent_ClearAll();
    Battle_GetEventWork()->script = keep;
}

/* Raises event ev for a side; events 0x5A and 0x5B also notify the script module. */
void BtlEvent_Raise(s32 side, s32 ev) {
    BtlEvent_SetBit(Battle_GetEventWork()->set[side].now, ev);
    switch (ev) {
    case 0x5A:
    case 0x5B:
        BtlScript_StartWaitEvents();
        break;
    }
}

/* BtlEventSet_IsNew on a side. */
s32 BtlEvent_IsNew(s32 side, s32 ev) {
    return BtlEventSet_IsNew(&Battle_GetEventWork()->set[side], ev);
}

/* BtlEventSet_WasRaised on a side. */
s32 BtlEvent_WasRaised(s32 side, s32 ev) {
    return BtlEventSet_WasRaised(&Battle_GetEventWork()->set[side], ev);
}

/* Once per unpaused frame: requests, rotate both sides, then raise the time events for the new frame. */
void BtlEvent_Update(void) {
    BattleEvents *ev = Battle_GetEventWork();
    s32 i;

    BtlEvent_UpdateRequests();
    for (i = 0; i < 2; i++) {
        BtlEventSet_Rotate(&ev->set[i]);
    }
    BtlEvent_RaiseTimeEvents();
}

/* Asks for event 0x4C: sets the pending word and battle flag 0x200, and fades the HUD out over 1.0. */
void BtlEvent_BeginInterrupt(void) {
    Battle_GetEventWork()->interrupt = 1;
    BtlFacade_SetFlag200();
    Hud_SlideOut(1.0f);
}

/* Fades the HUD back in over 1.0. */
void BtlEvent_EndInterrupt(void) {
    Hud_SlideIn(1.0f);
}

/* Stores the wait flag: waitFlag = (off == 0). */
void BtlEvent_SetWaitOff(s32 off) {
    Battle_GetEventWork()->waitFlag = off == 0;
}

/* 1 when the wait flag is clear. */
s32 BtlEvent_IsWaitOff(void) {
    return Battle_GetEventWork()->waitFlag == 0;
}

/* Clamps the eight bonus levels of a member to -20..20, or -20..60 when wide. */
void BtlMember_ClampBonus(BattleMember *m, s32 wide) {
    s32 min;
    s32 max;

    if (wide != 0) {
        min = -20;
        max = 60;
    } else {
        min = -20;
        max = 20;
    }
    if (m->bonus[0] > max) {
        m->bonus[0] = max;
    } else if (m->bonus[0] < -20) {
        m->bonus[0] = min;
    }
    if (m->bonus[1] > max) {
        m->bonus[1] = max;
    } else if (m->bonus[1] < -20) {
        m->bonus[1] = min;
    }
    if (m->bonus[2] > max) {
        m->bonus[2] = max;
    } else if (m->bonus[2] < -20) {
        m->bonus[2] = min;
    }
    if (m->bonus[3] > max) {
        m->bonus[3] = max;
    } else if (m->bonus[3] < -20) {
        m->bonus[3] = min;
    }
    if (m->bonus[4] > max) {
        m->bonus[4] = max;
    } else if (m->bonus[4] < -20) {
        m->bonus[4] = min;
    }
    if (m->bonus[5] > max) {
        m->bonus[5] = max;
    } else if (m->bonus[5] < -20) {
        m->bonus[5] = min;
    }
    if (m->bonus[6] > max) {
        m->bonus[6] = max;
    } else if (m->bonus[6] < -20) {
        m->bonus[6] = min;
    }
    if (m->bonus[7] > max) {
        m->bonus[7] = max;
    } else if (m->bonus[7] < -20) {
        m->bonus[7] = min;
    }
}

/* Recomputes bonus levels, AI type and ability bits of a member from its equipped items. */
void BtlMember_ApplyItems(BattleMember *m) {
    s32 ability[4];
    s32 stats[5];
    s32 i;

    ItemSet_GetStats(&m->items, stats, ability, m->chara);
    m->bonus[0] = 0;
    m->bonus[1] = stats[2];
    m->bonus[2] = stats[0];
    m->bonus[3] = stats[1];
    m->bonus[4] = stats[2];
    m->bonus[5] = stats[3];
    m->bonus[6] = stats[3];
    m->bonus[7] = stats[3];
    m->aiType = stats[4];
    for (i = 0; i < 4; i++) {
        m->ability[i] = ability[i];
    }
}

/*
 * Clears a member and fills it: items (optional), character, costume, variant, CPU level, health.
 * BtlMember_ApplyItems runs while m->chara is still 0, so without an AI item the AI type is the one of
 * character 0, whatever `chara` is.
 */
void BtlMember_Init(BattleMember *m, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                    BattleItemSet *items) {
    memset(m, 0, sizeof(BattleMember));
    if (items != NULL) {
        m->items = *items;
    }
    BtlMember_ApplyItems(m);
    m->chara = chara;
    m->costume = costume;
    m->variant = variant;
    m->cpuLevel = cpuLevel;
    m->health = health;
}

/* Forces the settings a mode does not allow (split-screen, time limit, team size, who is CPU). */
void BattleSetup_FixForMode(void) {
    BattleRule *rule = &SETUP()->rule;
    BattleSide *side;
    s32 i;
    s32 j;

    if (Battle_GetMode() != 0 && Battle_GetMode() != 4) {
        if (rule->screenMode == 1) {
            rule->screenMode = 0;
        }
    }
    if (Battle_GetMode() == 9) {
        if (rule->timeLimit != 0) {
            rule->timeLimit = 0;
        }
    }
    if (Battle_GetMode() == 7) {
        if (rule->timeLimit != 5) {
            rule->timeLimit = 5;
        }
    }
    for (i = 0; i < 2; i++) {
        side = &SETUP()->sides[i];
        switch (Battle_GetMode()) {
        case 2:
            break;
        case 9:
            if (side->control != 2) {
                side->control = 2;
                for (j = 0; j < 5; j++) {
                    BattleMember *m = &SETUP()->sides[i].members[j];

                    m->cpuLevel = 29;
                    m->aiType = 0;
                }
            }
            break;
        case 6:
            if (side->memberCount != 1) {
                side->memberCount = 1;
            }
            if (i == 0) {
                if (side->control != 0) {
                    side->control = 0;
                }
            }
            if (i == 1) {
                if (side->control != 2) {
                    side->control = 2;
                }
                if (side->members[0].cpuLevel != -1) {
                    side->members[0].cpuLevel = -1;
                }
            }
            break;
        case 4:
            if (side->memberCount != 1) {
                side->memberCount = 1;
            }
            break;
        case 3:
            if (i == 0) {
                if (side->memberCount != 1) {
                    side->memberCount = 1;
                }
                if (side->control != 0) {
                    side->control = 0;
                }
            }
            if (i == 1) {
                if (side->control != 2) {
                    side->control = 2;
                }
            }
            break;
        case 7:
            if (side->control != 2) {
                side->control = 2;
            }
            break;
        }
    }
    if (Battle_GetMode() == 9) {
        SETUP()->rule.mode = 6;
    }
}

/* Sets a side's usable-character bits: a copy of src, or the save's unlocked characters. */
void BattleSetup_InitCharaBits(BattleCharaBits *dst, BattleCharaBits *src) {
    if (src != NULL) {
        *dst = *src;
    } else {
        memset(dst, 0, sizeof(BattleCharaBits));
        dst->bits[0] = gSaveData->charaBits[0];
        dst->bits[1] = gSaveData->charaBits[1];
        dst->bits[2] = gSaveData->charaBits[2];
    }
}

/* Unless options were given, takes the two per-side options from the save data. */
void BattleSetup_DefaultOptions(void) {
    BattleOption *opt = &SETUP()->option;

    if (opt->isSet == 0) {
        memset(opt, 0, sizeof(BattleOption));
        opt->optA[0] = gSaveData->unk1694;
        opt->optA[1] = gSaveData->unk1694;
        opt->optB[0] = gSaveData->unk1698;
        opt->optB[1] = gSaveData->unk1698;
    }
}

/* Zeroes the setup and writes its magic "btls" and version 7. */
void BattleSetup_Clear(void) {
    memset(SETUP(), 0, sizeof(BattleSetup));
    SETUP()->magic[0] = 'b';
    SETUP()->magic[1] = 't';
    SETUP()->magic[2] = 'l';
    SETUP()->magic[3] = 's';
    SETUP()->version = 7;
}

/* Clears bit 0 of the replay data flags. */
void BattleReplay_ClearDataFlag(void) {
    BattleReplay_GetData()->flags &= ~1;
}

/* Selects battle script n (stored + 1; 0 = no script). */
void BattleSetup_SetScript(s32 script) {
    SETUP()->script = script + 1;
}

/* Sets the two per-side options explicitly. */
void BattleSetup_SetOptions(s32 optA0, s32 optA1, s32 optB0, s32 optB1) {
    BattleOption *opt = &SETUP()->option;

    memset(opt, 0, sizeof(BattleOption));
    opt->isSet = 1;
    opt->optA[0] = optA0;
    opt->optA[1] = optA1;
    opt->optB[0] = optB0;
    opt->optB[1] = optB1;
}

/* Sets option word 0x14 and takes the per-side options from the save data. */
void BattleSetup_SetOption14(s32 val) {
    BattleOption *opt = &SETUP()->option;

    memset(opt, 0, sizeof(BattleOption));
    opt->isSet = 1;
    opt->unk14 = val;
    opt->optA[0] = gSaveData->unk1694;
    opt->optA[1] = gSaveData->unk1694;
    opt->optB[0] = gSaveData->unk1698;
    opt->optB[1] = gSaveData->unk1698;
}

/* Sets the battle rules; bgm 24 means a random one of 0..23. */
void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10) {
    BattleRule *rule = &SETUP()->rule;

    rule->screenMode = screenMode;
    rule->mode = mode;
    if (bgm == 24) {
        rule->bgm = rand() % 24;
    } else {
        rule->bgm = bgm;
    }
    rule->timeLimit = timeLimit;
    rule->announcer = announcer;
    rule->unk10 = unk10;
    rule->stage = stage;
    rule->curStage = stage;
}

/* Sets one side: who controls it, pad, team size, options, lead member and usable characters. */
void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                   BattleCharaBits *bits) {
    BattleSide *side = Side(sideNo);

    side->control = control;
    side->pad = pad;
    side->memberCount = memberCount;
    side->unk200 = unk200;
    side->unk1FC = unk1FC;
    side->lead = lead;
    BattleSetup_InitCharaBits(&side->charaBits, bits);
}

/* Sets one team member from 0-based item ids stored in 32-bit words. */
void BattleSetup_SetMemberByItemIds(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel,
                                    f32 health, u32 *itemIds) {
    BattleMember *m = Member(sideNo, idx);
    BattleItemSet items;
    s32 i;

    memset(&items, 0, sizeof(items));
    for (i = 0; i < 8; i++) {
        items.id[i] = *(u16 *)&itemIds[i] + 1;
    }
    BtlMember_Init(m, chara, costume, variant, cpuLevel, health, &items);
}

/* Sets one team member (items: 1-based u16 ids or NULL). */
void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                           BattleItemSet *items) {
    BtlMember_Init(Member(sideNo, idx), chara, costume, variant, cpuLevel, health, items);
}

/* Sets entry idx of the opponent pool and the pool size. */
void BattleSetup_SetPoolMember(s32 count, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                               BattleItemSet *items) {
    BattleMemberPool *pool = Battle_GetWork5A8();

    pool->count = count;
    pool->cur = 0;
    BtlMember_Init(&pool->members[idx], chara, costume, variant, cpuLevel, health, items);
}

/* BattleSetup_FinishEx(0). */
void BattleSetup_Finish(void) {
    BattleSetup_FinishEx(0);
}

/* Last step of a setup: mode fix-ups, lead character, bonus clamps, rule.unk18, options, replay copy. */
void BattleSetup_FinishEx(s32 wide) {
    s32 cpuWide;
    s32 i;
    s32 j;
    BattleSide *side;
    BattleMember *m;
    BattleRule *rule;
    BattleSetup *setup;
    BattleMemberPool *pool;

    BattleSetup_FixForMode();
    switch (Battle_GetMode()) {
    case 1:
    case 2:
    case 3:
    case 4:
        cpuWide = 1;
        break;
    default:
        cpuWide = 0;
        break;
    }
    if (Battle_GetMode() == 3) {
        pool = Battle_GetWork5A8();
        pool->cur = 0;
        for (j = 0; j < 50; j++) {
            BtlMember_ClampBonus(&pool->members[j], 1);
        }
        SETUP()->sides[1].members[0] = pool->members[0];
    }
    for (i = 0; i < 2; i++) {
        setup = SETUP();
        side = &setup->sides[i];
        side->startForm.chara = side->members[side->lead].chara;
        side->startForm.costume = side->members[side->lead].costume;
        side->startForm.variant = side->members[side->lead].variant;
        side->form.chara = side->startForm.chara;
        side->form.costume = side->startForm.costume;
        side->form.variant = side->startForm.variant;
        switch (Battle_GetMode()) {
        case 2:
        case 3:
        case 4:
        case 7:
        case 9:
            if (side->control != 2) {
                break;
            }
        case 1:
            side->charaBits.bits[0] = -1;
            side->charaBits.bits[1] = -1;
            side->charaBits.bits[2] = -1;
            break;
        case 5:
        case 6:
        case 8:
            break;
        }
        for (j = 0; j < 5; j++) {
            m = &SETUP()->sides[i].members[j];
            if (side->control == 2) {
                if (cpuWide) {
                    BtlMember_ClampBonus(m, 1);
                } else {
                    BtlMember_ClampBonus(m, 0);
                }
            } else if (wide) {
                BtlMember_ClampBonus(m, 1);
            } else {
                BtlMember_ClampBonus(m, 0);
            }
        }
    }
    rule = &SETUP()->rule;
    rule->unk18 = 0;
    if (Battle_GetMode() == 1) {
        for (i = 0; i < 7; i++) {
            s32 mask = 1 << i;

            if (!(gSaveData->unlockFlags & mask)) {
                rule->unk18 = i + 1;
                break;
            }
        }
    }
    BattleSetup_DefaultOptions();
    SETUP()->unk5A0 = 1;
    memset(&gBattleReplay, 0, sizeof(BtlReplay));
    gBattleReplay.setup = *SETUP();
}

/* Returns the replay block (and its size) for saving; clears its two trailing words. */
BtlReplay *BattleReplay_GetBuffer(s32 *size) {
    BtlReplay *rep;

    if (size != NULL) {
        *size = sizeof(BtlReplay);
    }
    rep = &gBattleReplay;
    rep->loaded = 0;
    rep->unk1ABA4 = 0;
    return rep;
}

/* Installs a loaded replay block: its setup becomes the battle setup. Returns 1 when the block is valid. */
s32 BattleReplay_Load(BtlReplay *buf, s32 size) {
    if (buf != NULL) {
        if (size == sizeof(BtlReplay)) {
            if (*(u64 *)buf == (((u64)BTL_SETUP_VERSION << 32) | BTL_SETUP_MAGIC)) {
                *SETUP() = buf->setup;
                memset(&gBattleReplay, 0, sizeof(BtlReplay));
                gBattleReplay.setup = buf->setup;
                memcpy(&gBattleReplay.data, &buf->data, sizeof(BtlReplayData) + sizeof(s32));
                gBattleReplay.active = 1;
                gBattleReplay.loaded = 1;
                return 1;
            }
        }
    }
    return 0;
}

/* Non-zero while the battle plays a replay. */
s32 BattleReplay_IsActive(void) {
    return gBattleReplay.active;
}

/* Non-zero when the replay block came from BattleReplay_Load. */
s32 BattleReplay_IsLoaded(void) {
    return gBattleReplay.loaded;
}

/* Returns the recorded data part of the replay block. */
BtlReplayRec *BattleReplay_GetData(void) {
    return (BtlReplayRec *)&gBattleReplay.data;
}

/* 1 when no replay is active, else bit 0 of the replay data flags. */
s32 BattleReplay_TestDataFlag(void) {
    BtlReplayRec *data = BattleReplay_GetData();

    if (BattleReplay_IsActive() == 0) {
        return 1;
    }
    if ((data->flags & 1) != 0) {
        return 1;
    }
    return 0;
}

/* Sets the replay-active word. */
void BattleReplay_SetActive(s32 active) {
    gBattleReplay.active = active;
}

/* First side whose control is 0 (a pad), or -1. */
s32 Battle_GetHumanSide(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (Side(i)->control == 0) {
            return i;
        }
    }
    return -1;
}

/* Returns option word 0x14. */
s32 Battle_GetOption14(void) {
    return SETUP()->option.unk14;
}

/* Returns the battle script number, -1 when there is none. */
s32 Battle_GetScript(void) {
    return SETUP()->script - 1;
}

/* 1 when the battle is drawn in split-screen. */
s32 Battle_IsSplitScreen(void) {
    return SETUP()->rule.screenMode == 1;
}

/* Returns the battle mode (BATTLE_MODE_*). */
s32 Battle_GetMode(void) {
    return SETUP()->rule.mode;
}

/* Time limit in seconds (-1 = none). */
s32 Battle_GetTimeLimit(void) {
    return gBattleTimeLimitTbl[SETUP()->rule.timeLimit];
}

/* 1 when the time limit setting is 0 (no limit). */
s32 Battle_IsTimeLimitOff(void) {
    return SETUP()->rule.timeLimit == 0;
}

/* Returns the announcer voice (0..7). */
s32 Battle_GetAnnouncer(void) {
    return SETUP()->rule.announcer;
}

/* Returns the BGM number. */
s32 Battle_GetBgm(void) {
    return SETUP()->rule.bgm;
}

/* Returns the stage the battle starts on. */
s32 Battle_GetStartStage(void) {
    return SETUP()->rule.stage;
}

/* Returns the current stage. */
s32 Battle_GetStage(void) {
    return SETUP()->rule.curStage;
}

/* Sets the current stage. */
void Battle_SetStage(s32 stage) {
    SETUP()->rule.curStage = stage;
}

/* 1 when the current stage is not the starting one. */
s32 Battle_IsStageChanged(void) {
    return Battle_GetStartStage() != Battle_GetStage();
}

/* Current stage = starting stage. */
void Battle_ResetStage(void) {
    BattleRule *rule = &SETUP()->rule;

    rule->curStage = SETUP()->rule.stage;
}

/* Returns rule word 0x10 (setup + 0x18). */
s32 Battle_GetRuleUnk10(void) {
    return SETUP()->rule.unk10;
}

/* Per-side option A (save 0x1694 by default). */
s32 BattleSide_GetOptionA(s32 side) {
    s32 *p = SETUP()->option.optA;

    return p[side];
}

/* Per-side option B (save 0x1698 by default). */
s32 BattleSide_GetOptionB(s32 side) {
    s32 *p = SETUP()->option.optB;

    return p[side];
}

/* Returns the side's controller number. */
s32 BattleSide_GetPad(s32 side) {
    return Side(side)->pad;
}

/* Returns who controls the side (0 pad, 2 CPU). */
s32 BattleSide_GetControl(s32 side) {
    return Side(side)->control;
}

/* 1 when the side is CPU controlled. */
s32 BattleSide_IsCpu(s32 side) {
    return Side(side)->control == 2;
}

/* Returns side word 0x200. */
s32 BattleSide_GetUnk200(s32 side) {
    return Side(side)->unk200;
}

/* Returns side word 0x1FC. */
s32 BattleSide_GetUnk1FC(s32 side) {
    return Side(side)->unk1FC;
}

/* Character the side starts the battle with. */
s32 BattleSide_GetStartChara(s32 side) {
    return Side(side)->startForm.chara;
}

/* Costume the side starts the battle with. */
s32 BattleSide_GetStartCostume(s32 side) {
    return Side(side)->startForm.costume;
}

/* Model variant the side starts the battle with. */
s32 BattleSide_GetStartVariant(s32 side) {
    return Side(side)->startForm.variant;
}

/* Character currently loaded for the side. */
s32 BattleSide_GetChara(s32 side) {
    return Side(side)->form.chara;
}

/* Sets the character currently loaded for the side. */
void BattleSide_SetChara(s32 side, s32 chara) {
    Side(side)->form.chara = chara;
}

/* 1 when the current character is not the starting one. */
s32 BattleSide_IsCharaChanged(s32 side) {
    return BattleSide_GetStartChara(side) != BattleSide_GetChara(side);
}

/* Current character = starting character. */
void BattleSide_ResetChara(s32 side) {
    BattleSide_SetChara(side, BattleSide_GetStartChara(side));
}

/* Character of team member idx. */
s32 BattleSide_GetMemberChara(s32 side, s32 idx) {
    return Member(side, idx)->chara;
}

/* Costume of team member idx. */
s32 BattleSide_GetMemberCostume(s32 side, s32 idx) {
    return Member(side, idx)->costume;
}

/* Model variant of team member idx. */
s32 BattleSide_GetMemberVariant(s32 side, s32 idx) {
    return Member(side, idx)->variant;
}

/* Team size of the side. */
s32 BattleSide_GetMemberCount(s32 side) {
    return Side(side)->memberCount;
}

/* Object id of the side's fighter (argument of BtlObj_Get). */
s32 BattleSide_GetObjId(s32 side) {
    return Side(side)->objId;
}

/* Returns side word 0x26C (model slot from BtlObj_RequestCharaModel). */
s32 BattleSide_GetModelSlot(s32 side) {
    return Side(side)->modelSlot;
}

/* Sets the object id of the side's fighter. */
void BattleSide_SetObjId(s32 side, s32 objId) {
    Side(side)->objId = objId;
}

/* Sets side word 0x26C. */
void BattleSide_SetModelSlot(s32 side, s32 slot) {
    Side(side)->modelSlot = slot;
}

/* Sets the currently loaded character, costume and variant. */
void BattleSide_SetForm(s32 side, s32 chara, s32 costume, s32 variant) {
    Side(side)->form.chara = chara;
    Side(side)->form.costume = costume;
    Side(side)->form.variant = variant;
}

/* 1 when the current character, costume or variant differs from the starting one. */
s32 BattleSide_IsFormChanged(s32 side) {
    if (Side(side)->form.chara != Side(side)->startForm.chara) {
        return 1;
    }
    if (Side(side)->form.costume != Side(side)->startForm.costume) {
        return 1;
    }
    return Side(side)->form.variant != Side(side)->startForm.variant;
}

/* Returns team member idx of a side. */
BattleMember *BattleSide_GetMember(s32 side, s32 idx) {
    return Member(side, idx);
}

/* Replaces a member's items and recomputes what derives from them; nothing when items is NULL. */
void BattleSide_SetMemberItems(s32 side, s32 idx, BattleItemSet *items) {
    BattleMember *m = BattleSide_GetMember(side, idx);

    if (items != NULL) {
        m->items = *items;
        BtlMember_ApplyItems(m);
    }
}

/* 1 when character id is in the side's usable-character bits. */
s32 BattleSide_IsCharaUsable(s32 side, s32 chara) {
    u64 *bits = Side(side)->charaBits.bits;

    return (bits[chara / 64] >> (chara % 64)) & 1;
}
