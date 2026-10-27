#include "common.h"
#include "battle/view_a.h"
#include "battle/orbit_cam.h"
#include "sys/common.h"
#include "sys/fade.h"
#include "sys/file.h"
#include "sys/heap.h"
#ifdef PORT
#include "battle/bobj_a.h"
extern void Bpe_Decode(void *src, void *dst, s32 *size);
extern void Port_TagTeamFatal(const char *message);
#endif

/*
 * ChrView, 0x25D468..0x25DE68: the character model viewer of the menus. It runs the battle's object, stage and
 * scene modules without a battle: one character model at a time on a backdrop (file 0x197), turned with the
 * orbit camera. All callers of the public functions are in the menu overlay. See battle/view_a.h.
 *
 * A new model is loaded by a job (ChrView_StepLoad) into the buffer that is not on screen; the old model is
 * destroyed when the new one has been set up, three frames after its object was created.
 */

extern void *memset(void *dst, s32 c, u32 n);

/* local views of the battle modules used here */
typedef struct ChrViewBattleWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags; /* 0x100 = paused */
} ChrViewBattleWork;

typedef struct ChrViewStage {
    /* 0x00 */ s32 unk0[2];
    /* 0x08 */ s32 flags;   /* bit 0: not ready */
} ChrViewStage;

/* gCommonRes->unk20: the viewer keeps its backdrop file there */
typedef struct ChrViewCommon {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ void *stage;
} ChrViewCommon;

extern ChrViewBattleWork *Battle_GetWork(void);
extern void BtlRes_LoadSingle(s32 fromHeap, void *slot, s32 model);
extern s32 BtlObj_Create(s32 type, void *res, s32 active);
extern u8 *BtlObj_Get(s32 id);
extern s32 BtlObj_Destroy(s32 id);
#ifndef PORT
extern void BtlObjAnim_PlayModel(u8 *obj, s32 anim, s32 mode);
#endif
extern void BtlObj_SetPartVisible(u8 *obj, u32 part, s32 on);
extern void BtlObj_Init(s32 prealloc);
extern void BtlObj_Term(void);
extern void BtlObj_SetUnk44FC4(f32 value);
extern void BtlObj_UpdateAll(void);
extern void BtlObj_UpdateVisibility(s32 view);
extern void BtlObj_FinishVisibility(s32 split);
extern void BtlStage_Update(void);
extern void BtlStage_Term(void);
extern void BtlScene_Init(s32 layerMask);
extern void BtlScene_Term(void);
extern void BtlScene_Update(void);
extern void BtlScene_Draw(s32 first);
extern void StgAmb_Init(void);
extern void StgAmb_Term(void);
extern void StgModel_InitStage(void);
extern void StgFx_Init(void);
extern void StgFx_Term(void);
extern void StgFx_DrawPreNoCheck(void);
extern void Gfx_MarkPass(s32 pass);
extern void Gfx_AddDefaultEnv(void);
extern void Ot_Init(void);
extern void Ot_Term(void);
extern s32 Ot_Draw(void);
extern void Dma_ResetBuffers(void);
extern void Adx_StopAll(void);
extern void Load_RunBlocking(void);
extern void Dbg_ProfMark(void *prof);
extern void Dbg_ProfColor(void *prof, u32 color);
extern void StgModel_Cull(void *view);
extern void StgModel_Draw(void *view);
extern void BtlObjDraw_Draw(void);

extern u8 gBattleProf[];
extern ChrViewStage *gBtlStage;

/* sys/file.h declares two parameters; the call passes a third (0), which File_Request ignores */
extern void *File_Request3(s32 id, void *buf, s32 unused) __asm__("File_Request");

/* Defined here: this object's .sdata (0x2FF108). */
ChrView *gChrView = NULL;

#define BTL_OBJ_FLAGS(obj) (*(u32 *)((obj) + 0xA40))

/* File id of a character's model: costume n, or its damaged version. */
s32 ChrView_GetModelFile(s32 chara, s32 costume, s32 damaged) {
#ifdef PORT
    return PortGame_Model(chara,costume,damaged);
#endif
    if (damaged != 0) {
        return chara * 10 + costume + (CHRVIEW_FILE_FIRST + 4);
    }
    return chara * 10 + costume + CHRVIEW_FILE_FIRST;
}

/* The character's entry in the table of common file 4, NULL for an id that is not a character. */
ChrViewInfo *ChrView_GetCharaInfo(u32 chara) {
    ChrViewInfo *table = PACK_AT(gCommonRes->data[2], 1);
    if (chara >= PortGame_CharacterCount()) {
        return NULL;
    }
    return &table[chara];
}

/* Takes a model buffer off the free list. */
ChrViewSlot *ChrView_AllocSlot(void) {
    ChrViewSlot *slot = (ChrViewSlot *)SList_PopFront(&gChrView->free);

    if (slot == NULL) {
        return NULL;
    }
    return slot;
}

/* Puts a model buffer back on the free list. */
s32 ChrView_FreeSlot(ChrViewSlot *slot) {
    if (slot != NULL) {
        SList_PushFront(&gChrView->free, &slot->node);
        return 1;
    }
    return 0;
}

/* The viewer, which starts with its load job. */
ChrView *ChrView_Get(void) {
    return gChrView;
}

/* The model on screen. */
ChrViewSlot *ChrView_GetCur(void) {
    return gChrView->cur;
}

void ChrView_SetCur(ChrViewSlot *slot) {
    gChrView->cur = slot;
}

/* 1 while a model is being loaded. */
s32 ChrView_IsLoading(void) {
    return gChrView->loading;
}

void ChrView_SetLoading(s32 loading) {
    gChrView->loading = loading;
}

/* Points the orbit camera at a character: target height and distance come from the character table. */
void ChrView_SetupCamera(s32 chara) {
    ChrViewInfo *info = ChrView_GetCharaInfo(chara);

    if (info != NULL) {
        OrbitCam_Reset();
        OrbitCam_SetFloorClamp(1);
        OrbitCam_SetTarget(0.0f, info->targetY, 0.0f);
        OrbitCam_SetAngles(0.0f, 3.14159265f, 0.0f);
        OrbitCam_SetDist(info->dist);
        if (gChrView->zoom != 0) {
            OrbitCam_SetDistRange(3.0f, 100.0f);
        } else {
            OrbitCam_SetDistRange(info->dist, info->dist);
        }
        OrbitCam_SetPitchLimit(0.78539816f);
    }
}

/* Load job: waits, reads the model into a free buffer, creates its object, and after three frames swaps it in. */
s32 ChrView_StepLoad(ChrViewJob *job) {
    s32 file;
    ChrViewSlot *old;

    switch (job->state) {
    case 0:
        ChrView_SetLoading(1);
        if (job->count++ >= gChrView->delay) {
            job->state++;
        }
        if (gChrView->delay != 0) {
            break;
        }
        /* fall through */
    case 1:
        file = ChrView_GetModelFile(job->chara, job->costume, job->damaged);
        job->slot = ChrView_AllocSlot();
        if (job->slot == NULL) {
            return 1;
        }
        BtlRes_LoadSingle(0, &job->slot->file, file);
        job->state++;
        break;
    case 2:
        if (File_UpdateRequests() == 0) {
            return 0;
        }
        job->slot->obj = BtlObj_Create(3, &job->slot->file, 1);
        job->obj = BtlObj_Get(job->slot->obj);
#ifdef PORT
        {
            BobjObj *obj = (BobjObj *)job->obj;
            /* B14 moved the viewer stance out of several costume files.
               Motion zero in the fighter's animation bank is that same idle. */
            if (PortGame_IsBT4() && !obj->mdl.modelAnims[gChrView->anim]) {
                s32 id = PortGame_Animation(job->chara), size = File_GetSize(id), decoded = 0;
                u32 *bank = File_LoadSync(id, NULL, 0);
                u32 start = bank[1] & ~3u, end = bank[2] & ~3u;
                if (bank[0] < 1 || bank[0] > 512 || start < (bank[0]+2)*4 ||
                    end <= start+4 || end > (u32)size ||
                    *(u32 *)((u8 *)bank+start) < 148 ||
                    *(u32 *)((u8 *)bank+start) > (u32)PortGame_AnimationCapacity())
                    Port_TagTeamFatal("Invalid character viewer idle animation.");
                Bpe_Decode((u8 *)bank+start, job->slot->idleMotion, &decoded);
                Heap_Free(bank);
                if (PortGame_CheckAnimation(job->slot->idleMotion, decoded) < 0)
                    Port_TagTeamFatal("Invalid character viewer idle animation.");
                obj->mdl.modelAnims[gChrView->anim] = job->slot->idleMotion;
            }
            BtlObjAnim_PlayModel(obj, gChrView->anim, 2);
        }
#else
        BtlObjAnim_PlayModel(job->obj, gChrView->anim, 2);
#endif
        BTL_OBJ_FLAGS(job->obj) &= ~0x02000000;
        BtlObj_SetPartVisible(job->obj, 0, 0);
        job->count = 0;
        job->state++;
        /* fall through */
    case 3:
        if (job->count++ >= 3) {
            ChrView_SetupCamera(job->chara);
            BtlObj_SetPartVisible(job->obj, 0, 1);
            old = ChrView_GetCur();
            if (old != NULL) {
                BtlObj_Destroy(old->obj);
                ChrView_FreeSlot(old);
            }
            ChrView_SetCur(job->slot);
            gChrView->stageOn = 1;
            gChrView->visible = 1;
            StgAmb_Init();
            ChrView_SetLoading(0);
            return 1;
        }
        break;
    default:
        return 1;
    }
    return 0;
}

/* Job run once by ChrView_Init: reads the backdrop file. */
s32 ChrView_StepLoadStage(ChrViewJob *job) {
    ChrViewCommon *res = (ChrViewCommon *)gCommonRes->unk20;

    switch (job->state) {
    case 0:
        res->stage = File_Request3(DISC_RESOURCE(CHRVIEW_FILE_STAGE, 0x619), NULL, 0);
        job->state++;
        break;
    case 1:
        return File_UpdateRequests() != 0;
    default:
        return 1;
    }
    return 0;
}

/* Stops a load in progress: frees its buffer (and its object, if it was created) and empties the job queue. */
s32 ChrView_CancelLoad(void) {
    if (ChrView_IsLoading() == 1) {
        if (gChrView->job.slot != NULL) {
            if (gChrView->job.state >= 3) {
                BtlObj_Destroy(gChrView->job.slot->obj);
            }
            ChrView_FreeSlot(gChrView->job.slot);
        }
        File_CancelRequests();
        Job_Clear();
        ChrView_SetLoading(0);
        return 1;
    }
    return 0;
}

/* Takes the model off the screen. Returns 1 if there was one. */
s32 ChrView_Hide(void) {
    ChrViewSlot *slot = ChrView_GetCur();

    gChrView->stageOn = 0;
    StgAmb_Term();
    gChrView->visible = 0;
    if (ChrView_IsLoading() == 1) {
        ChrView_CancelLoad();
    }
    if (slot != NULL) {
        BtlObj_Destroy(slot->obj);
        ChrView_FreeSlot(slot);
        ChrView_SetCur(NULL);
        return 1;
    }
    return 0;
}

/* Starts loading a character model; it replaces the one on screen when it is ready. */
s32 ChrView_Show(s32 chara, s32 costume, s32 damaged) {
    ChrViewJob *job;

    if (ChrView_IsLoading() == 1) {
        ChrView_CancelLoad();
    }
    job = &ChrView_Get()->job;
    memset(job, 0, sizeof(ChrViewJob));
    job->step = ChrView_StepLoad;
    job->chara = chara;
    job->costume = costume;
    job->damaged = damaged;
    Job_Push((Job *)job);
    return 1;
}

/* One frame of the viewer: camera from pad 0, objects, then the stage, the model and the scene are drawn. */
void ChrView_Update(void) {
    View *view;

    OrbitCam_Update(1);
    Dbg_ProfMark(gBattleProf);
    if (gChrView->stageOn != 0) {
        gBtlStage->flags &= ~1;
    } else {
        gBtlStage->flags |= 1;
    }
    BtlObj_UpdateAll();
    Dbg_ProfColor(gBattleProf, 0x80FFFFFF);
    if (ChrView_IsVisible()) {
        BtlObj_UpdateVisibility(0);
        BtlObj_FinishVisibility(0);
        BtlStage_Update();
        BtlScene_Update();
        view = gBtlCamView;
        Dbg_ProfMark(gBattleProf);
        Gfx_MarkPass(1);
        StgModel_Cull(view);
        StgModel_Draw(view);
        Dbg_ProfColor(gBattleProf, 0x80FF4040);
        Dbg_ProfMark(gBattleProf);
        Gfx_MarkPass(3);
        StgFx_DrawPreNoCheck();
        Dbg_ProfColor(gBattleProf, 0x80FFFFFF);
        Dbg_ProfMark(gBattleProf);
        Gfx_MarkPass(2);
        BtlObjDraw_Draw();
        Dbg_ProfColor(gBattleProf, 0x8040FF40);
        Gfx_MarkPass(0);
        Dbg_ProfMark(gBattleProf);
        Gfx_MarkPass(4);
        Gfx_AddDefaultEnv();
        BtlScene_Draw(1);
        Ot_Draw();
        Dbg_ProfColor(gBattleProf, 0x804040FF);
        Dbg_ProfMark(gBattleProf);
        Gfx_MarkPass(0);
        Dbg_ProfColor(gBattleProf, 0x804040FF);
    }
}

/* Non-zero once a model is on screen. */
s32 ChrView_IsVisible(void) {
    return gChrView->visible;
}

/* Starts the viewer: two model buffers, the battle object, stage and scene modules, the backdrop, a fade in. */
void ChrView_Init(void) {
    s32 i;
    ChrViewJob *job;

    Battle_GetWork()->flags &= ~0x100;
    gChrView = Heap_Alloc(sizeof(ChrView), 0x20, 0, HEAP_ANY);
    memset(gChrView, 0, sizeof(ChrView));
    gChrView->anim = 7;
    for (i = 0; i < CHRVIEW_SLOT_COUNT; i++) {
        gChrView->slot[i].file[0].buf = Heap_Alloc(
#ifdef PORT
            PortGame_IsBT4() ? 0xE6800 :
#endif
            CHRVIEW_MODEL_SIZE, 0x40, 0, HEAP_ANY);
        gChrView->slot[i].file[0].size = 0;
        gChrView->slot[i].file[0].id = -1;
        *(s32 *)gChrView->slot[i].file[0].buf = 0;
#ifdef PORT
        if (PortGame_IsBT4())
            gChrView->slot[i].idleMotion = Heap_Alloc(PortGame_AnimationCapacity(), 0x20, 0, HEAP_ANY);
#endif
        SList_PushFront(&gChrView->free, &gChrView->slot[i].node);
    }
    BtlObj_Init(0);
    BtlObj_SetUnk44FC4(1.0f);
    OrbitCam_Init();
    Job_Clear();
    job = &ChrView_Get()->job;
    memset(job, 0, sizeof(ChrViewJob));
    job->step = ChrView_StepLoadStage;
    Job_Push((Job *)job);
    Load_RunBlocking();
    StgModel_InitStage();
    StgAmb_Term();
    StgFx_Init();
    Ot_Init();
    BtlScene_Init(8);
    Fade_Start(0, 1, 1.0f);
}

/* Ends the viewer and frees everything ChrView_Init set up, the backdrop file included. */
void ChrView_Term(void) {
    s32 i;

    BtlObj_Term();
    for (i = 0; i < CHRVIEW_SLOT_COUNT; i++) {
        Heap_Free(gChrView->slot[i].file[0].buf);
#ifdef PORT
        if (gChrView->slot[i].idleMotion) Heap_Free(gChrView->slot[i].idleMotion);
#endif
    }
    Heap_Free(gChrView);
    File_CancelRequests();
    Job_Clear();
    OrbitCam_Term();
    StgFx_Term();
    BtlStage_Term();
    BtlScene_Term();
    Ot_Term();
    Dma_ResetBuffers();
    Adx_StopAll();
    Heap_Free(((ChrViewCommon *)gCommonRes->unk20)->stage);
    Fade_ResetAll();
}
