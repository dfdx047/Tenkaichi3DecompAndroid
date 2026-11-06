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

#ifdef PORT
/* ---- the character selects' fighter (src/menu/port_charsel.c) ----
   Runs in a world of its own, whatever the mode, online above all. Port_WorldEnter (port/src/gs/state.c) puts in the
   game's variables what this code left in them the frame before, and Port_WorldLeave puts the game's values back; what
   it allocates comes from a part of the port's memory no saved state holds (Heap_Alloc while the world is in), and it
   reads its files by itself, away from the game's file queue. So no game variable, heap block, file request or random
   number is different for having shown a fighter: the two sides of an online match stay the same with this on one of
   them only, and the screen below (a Training, a lobby) leaves it no less room. All that stays is what it adds to
   the frame's display list, like any drawing. It is never torn down: the world is just forgotten.

   The fighter plays the entrance pose of a fight's intro (motion 0x180, as BtlCtrl_StartEntrance has it) and goes on
   into its own hold (0x181 in a loop, as BtlAct_Action01); without one it stays on its last frame. */
#include <stdio.h>
#define CHRVIEW_PORT_ANIM_SIZE 0x160800 /* BTL_RES_ANIM0_SIZE */
#define CHRVIEW_PORT_MOTION_SIZE 0xC000 /* BOBJ_WORK_LAYER_SIZE */
#define CHRVIEW_PORT_MODEL_MIN 0x4000   /* a number with no costume behind it is a stub of 2,048 bytes */
#ifdef __APPLE__
#define CHRVIEW_HOST __attribute__((section("__DATA,__porthst")))
#else
#define CHRVIEW_HOST __attribute__((section(".porthst")))
#endif
extern void Port_WorldEnter(void);
extern void Port_WorldLeave(void);
extern void Port_WorldForget(void);
extern void Port_WorldCaptureBegin(void);
extern void Port_WorldCaptureEnd(void);
extern void Port_WorldNoteDma(u32 a, u32 b, s32 size);
extern s32 File_PortReadRaw(s32 id, void *buf, s32 cap);
extern s32 BtlObjAnim_PortPlay(u8 *obj, void *buf, s32 anim, s32 mode);
extern s32 BtlObjAnim_PortStopped(u8 *obj);
extern void BtlObjAnim_PortPlace(u8 *obj, f32 x, f32 yaw, f32 scale);
extern u32 *gDmaCur;
extern s32 gDmaVif1Started;
extern u32 gDmaBuf[2];
extern s32 gDmaBufSize;

/* what the screen reads with the game's values in place */
static CHRVIEW_HOST s32 sHostVisible[2], sHostPedestal[2];
static CHRVIEW_HOST f32 sHostCx[2], sHostCy[2], sHostRx[2], sHostRy[2];
static CHRVIEW_HOST void *sHostStage;
static CHRVIEW_HOST s32 sHostLive;

/* in the world only: one viewer a side (two people choosing, the select in two halves) */
static ChrView *sPortView[2];
static void *sPortMotion[2][CHRVIEW_SLOT_COUNT];
static s32 sPortStanding[2];
static s32 sPortChara[2], sPortCostume[2], sPortShown[2];
static f32 sPortX[2];
static ChrViewSlot *sPortNew[2];
static s32 sPortCount[2];

s32 ChrView_PortVisible(s32 side) {
    return sHostVisible[side];
}

s32 ChrView_PortPedestal(s32 side, f32 *cx, f32 *cy, f32 *rx, f32 *ry) {
    *cx = sHostCx[side];
    *cy = sHostCy[side];
    *rx = sHostRx[side];
    *ry = sHostRy[side];
    return sHostPedestal[side];
}

static void *ChrView_PortMem(s32 size) {
    return Heap_Alloc(size, 0x40, 0, HEAP_ANY);
}

static void ChrView_PortOneView(s32 side) {
    s32 i;

    sPortView[side] = gChrView = ChrView_PortMem(sizeof(ChrView));
    memset(gChrView, 0, sizeof(ChrView));
    gChrView->anim = 7;
    for (i = 0; i < CHRVIEW_SLOT_COUNT; i++) {
        gChrView->slot[i].file[0].buf = ChrView_PortMem(CHRVIEW_MODEL_SIZE);
        gChrView->slot[i].file[0].id = -1;
        gChrView->slot[i].file[1].buf = ChrView_PortMem(CHRVIEW_PORT_ANIM_SIZE);
        gChrView->slot[i].file[1].id = -1;
        sPortMotion[side][i] = ChrView_PortMem(CHRVIEW_PORT_MOTION_SIZE);
        SList_PushFront(&gChrView->free, &gChrView->slot[i].node);
    }
    sPortChara[side] = -1;
    sPortCostume[side] = -1;
    sPortShown[side] = -1;
    sPortNew[side] = NULL;
}

static void ChrView_PortInit(s32 split) {
    ChrViewCommon *res = (ChrViewCommon *)gCommonRes->unk20;
    s32 got;

    Battle_GetWork()->flags &= ~0x100;
    ChrView_PortOneView(0);
    sPortView[1] = NULL;
    if (split) {
        ChrView_PortOneView(1);
    }
    BtlObj_Init(0);
    BtlObj_SetUnk44FC4(1.0f);
    OrbitCam_Init();
    OrbitCam_SetPadLocked(1);
    res->stage = ChrView_PortMem(0x400000);
    got = File_PortReadRaw(CHRVIEW_FILE_STAGE, res->stage, 0x400000);
    fprintf(stderr, "bt3: select: the viewer's world is up (backdrop %d bytes, %d side(s))\n", (int)got, (int)(split ? 2 : 1));
    StgModel_InitStage();
    StgFx_Init();
    Ot_Init();
    BtlScene_Init(8);
    Fade_Start(0, 1, 1.0f);
}

/* Reads a fighter into a free buffer of this side's viewer and makes its object. 0: it could not be used. */
static s32 ChrView_PortLoad(s32 side, s32 chara, s32 costume) {
    ChrViewSlot *slot = ChrView_AllocSlot();
    s32 model = ChrView_GetModelFile(chara, costume, 0);
    s32 at;
    s32 got;
    u32 *head;
    u8 *obj;

    if (slot == NULL) {
        return 0;
    }
    head = (u32 *)slot->file[0].buf;
    got = File_PortReadRaw(model, head, CHRVIEW_MODEL_SIZE);
    if (got < CHRVIEW_PORT_MODEL_MIN) {
        model = ChrView_GetModelFile(chara, 0, 0);
        got = File_PortReadRaw(model, head, CHRVIEW_MODEL_SIZE);
    }
    /* (what was read has the table a model starts with, and the entries BtlObjMdl_BindFiles goes through) */
    if (got < CHRVIEW_PORT_MODEL_MIN || head[3] == head[4] || head[2] == head[3] || head[12] == head[13]) {
        fprintf(stderr, "bt3: select: fighter %d has no model (file %d, %d bytes)\n", (int)chara, (int)model, (int)got);
        ChrView_FreeSlot(slot);
        return 0;
    }
    slot->file[0].id = model;
    slot->file[0].size = got;
    at = slot - gChrView->slot;
    memset(slot->file[1].buf, 0, 0x1000); /* an empty motion file leaves an empty table */
    slot->file[1].id = chara * 10 + CHRVIEW_FILE_FIRST + 8;
    slot->file[1].size = File_PortReadRaw(slot->file[1].id, slot->file[1].buf, CHRVIEW_PORT_ANIM_SIZE);
    slot->obj = BtlObj_Create(3, &slot->file, 1);
    if (slot->obj < 0) {
        ChrView_FreeSlot(slot);
        return 0;
    }
    obj = BtlObj_Get(slot->obj);
    if (BtlObjAnim_PortPlay(obj, sPortMotion[side][at], 0x180, 1)) {
        sPortStanding[side] = 0;
    } else {
        BtlObjAnim_PlayModel(obj, gChrView->anim, 2);
        sPortStanding[side] = 1;
    }
    BTL_OBJ_FLAGS(obj) &= ~0x32000000; /* (no shadow pass for it, already while it is not on screen) */
    BtlObj_SetPartVisible(obj, 0, 0);
    sPortNew[side] = slot;
    sPortCount[side] = 0;
    sPortShown[side] = chara;
    return 1;
}

/* A point of the world on the screen's 512 x 448, by the frame's camera. */
static void ChrView_PortProject(f32 x, f32 y, f32 z, f32 *sx, f32 *sy) {
    Mtx44 *m = &gBtlCamView->world2screen;
    f32 w = x * m->m[0][3] + y * m->m[1][3] + z * m->m[2][3] + m->m[3][3];

    if (w == 0.0f) {
        w = 1.0f;
    }
    *sx = (x * m->m[0][0] + y * m->m[1][0] + z * m->m[2][0] + m->m[3][0]) / w - 1792.0f;
    *sy = (x * m->m[0][1] + y * m->m[1][1] + z * m->m[2][1] + m->m[3][1]) / w - 1824.0f;
}

static void ChrView_PortDrop(ChrViewSlot *slot) {
    BtlObj_Destroy(slot->obj);
    ChrView_FreeSlot(slot);
}

/* One frame. For each side the fighter wanted (chara < 0: none), standing at screenX on the screen's 512 across and turned
   by yaw. split: two people choosing, two fighters in one view: one camera, framed for the first character and a little
   back, each fighter scaled to what its own framing would have made of it. What the screen drew before this stays
   behind them. */
void ChrView_PortFrame(f32 *screenX, f32 *yaw, s32 *chara, s32 *costume, s32 split) {
    u32 *dma = gDmaCur;
    s32 vif = gDmaVif1Started;
    ChrViewCommon *res;
    void *gameStage;
    s32 side;
    s32 sides = split ? 2 : 1;
    s32 any = 0;
    ChrViewInfo *ref = ChrView_GetCharaInfo(0);

    Port_WorldEnter();
    gDmaCur = dma;
    gDmaVif1Started = vif;
    res = (ChrViewCommon *)gCommonRes->unk20;
    gameStage = res->stage;
    res->stage = sHostStage;

    if (!sHostLive) {
        Port_WorldNoteDma(gDmaBuf[0], gDmaBuf[1], gDmaBufSize);
        Port_WorldCaptureBegin();
        ChrView_PortInit(split);
        Port_WorldCaptureEnd();
        sHostLive = 1;
        if (split) {
            gChrView = sPortView[0];
            ChrView_SetupCamera(0);
            OrbitCam_SetDistRange(ref->dist / 0.8f, ref->dist / 0.8f);
            OrbitCam_SetDist(ref->dist / 0.8f);
        }
    }
    for (side = 0; side < sides; side++) {
        gChrView = sPortView[side];
        /* a new fighter: read now, on screen three frames later (its first poses have to be worked out) */
        if (sPortNew[side] == NULL && (chara[side] != sPortChara[side] || costume[side] != sPortCostume[side])) {
            s32 ok = chara[side] >= 0 && chara[side] < CHRVIEW_CHARA_COUNT && ChrView_PortLoad(side, chara[side], costume[side]);

            if (!ok && gChrView->cur != NULL) {
                ChrView_PortDrop(gChrView->cur);
                gChrView->cur = NULL;
                gChrView->visible = 0;
            }
            sPortChara[side] = chara[side];
            sPortCostume[side] = costume[side];
        }
        if (sPortNew[side] != NULL && sPortCount[side]++ >= 3) {
            ChrViewSlot *fresh = sPortNew[side];

            sPortNew[side] = NULL;
            if (!split) {
                ChrView_SetupCamera(sPortShown[side]);
            }
            BtlObj_SetPartVisible(BtlObj_Get(fresh->obj), 0, 1);
            if (gChrView->cur != NULL) {
                ChrView_PortDrop(gChrView->cur);
            }
            gChrView->cur = fresh;
            gChrView->stageOn = 1;
            gChrView->visible = 1;
        }
    }
    OrbitCam_Update(0);
    gBtlStage->flags &= ~1;
    for (side = 0; side < sides; side++) {
        ChrViewSlot *cur;

        gChrView = sPortView[side];
        cur = gChrView->cur;
        sHostVisible[side] = gChrView->visible;
        any |= gChrView->visible;
        if (cur != NULL) {
            u8 *obj = BtlObj_Get(cur->obj);
            ChrViewInfo *info = ChrView_GetCharaInfo(sPortShown[side]);
            f32 scale = 1.0f;

            if (!sPortStanding[side] && BtlObjAnim_PortStopped(obj)) {
                BtlObjAnim_PortPlay(obj, sPortMotion[side][cur - gChrView->slot], 0x181, 2);
                sPortStanding[side] = 1;
            }
            if (info != NULL) {
                f32 a;
                f32 b;
                f32 unused;
                f32 h = split ? ref->targetY : info->targetY;

                ChrView_PortProject(0.0f, h, 0.0f, &a, &unused);
                ChrView_PortProject(1.0f, h, 0.0f, &b, &unused);
                sPortX[side] = b != a ? (screenX[side] - a) / (b - a) : 0.0f;
                if (split && info->dist > 0.0f) {
                    scale = ref->dist / info->dist;
                }
            }
            BTL_OBJ_FLAGS(obj) &= ~0x30000000; /* no hard shadow: the screen lays a soft one on the pedestal */
            BtlObjAnim_PortPlace(obj, sPortX[side], yaw[side], scale);
        }
    }
    BtlObj_UpdateAll();
    for (side = 0; side < sides; side++) {
        sHostPedestal[side] = 0;
    }
    if (any) {
        gChrView = sPortView[0];
        BtlObj_UpdateVisibility(0);
        BtlObj_FinishVisibility(0);
        BtlStage_Update();
        BtlScene_Update();
        Gfx_MarkPass(2);
        BtlObjDraw_Draw();
        Gfx_MarkPass(0);
        Gfx_MarkPass(4);
        Gfx_AddDefaultEnv();
        BtlScene_Draw(1);
        Ot_Draw();
        Gfx_MarkPass(0);
        for (side = 0; side < sides; side++) {
            ChrViewInfo *info = ChrView_GetCharaInfo(sPortShown[side]);

            if (info != NULL && sPortView[side]->visible) {
                f32 s = split && info->dist > 0.0f ? ref->dist / info->dist : 1.0f;
                f32 r = info->targetY * s * 0.62f;
                f32 cx;
                f32 cy;
                f32 ex;
                f32 ey;
                f32 fx;
                f32 fy;

                ChrView_PortProject(sPortX[side], 0.0f, 0.0f, &cx, &cy);
                ChrView_PortProject(sPortX[side] + r, 0.0f, 0.0f, &ex, &ey);
                ChrView_PortProject(sPortX[side], 0.0f, r, &fx, &fy);
                sHostCx[side] = cx;
                sHostCy[side] = cy;
                sHostRx[side] = ex > cx ? ex - cx : cx - ex;
                sHostRy[side] = fy > cy ? fy - cy : cy - fy;
                sHostPedestal[side] = 1;
            }
        }
    }

    sHostStage = res->stage;
    res->stage = gameStage;
    dma = gDmaCur;
    vif = gDmaVif1Started;
    Port_WorldLeave();
    gDmaCur = dma;
    gDmaVif1Started = vif;
}

/* The screen is left: the world is forgotten, nothing of it was ever in the game. */
void ChrView_PortEnd(void) {
    if (sHostLive) {
        Port_WorldForget();
    }
    sHostLive = 0;
    sHostVisible[0] = sHostVisible[1] = 0;
    sHostPedestal[0] = sHostPedestal[1] = 0;
    sHostStage = NULL;
}
#endif

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
