#include "common.h"
#include "port_variant.h"
#include "battle/view_a.h"
#include "battle/orbit_cam.h"
#include "sys/common.h"
#include "sys/fade.h"
#include "sys/file.h"
#include "sys/heap.h"

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
extern void BtlObjAnim_PlayModel(u8 *obj, s32 anim, s32 mode);
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
#ifdef PORT
/* BT4's encyclopedia poses live outside the model's battle animation bank.
 * Keep one allocation per model slot: a replacement is prepared while the old
 * object can still be drawn, and cancelling a load must release only its slot. */
static void *sBt4ViewerMotion[CHRVIEW_SLOT_COUNT];
#endif

#define BTL_OBJ_FLAGS(obj) (*(u32 *)((obj) + 0xA40))

static s32 Bt4_Costumes(s32 chara) {
    ChrViewInfo *info = ChrView_GetCharaInfo(chara);
    return info != NULL ? *(u16 *)((u8 *)info + 0x28) : 0;
}
s32 Bt4_ModelFile(s32 chara, s32 costume, s32 damaged) {
    s32 i, file = 0x1612;
    for (i = 0; i < chara; i++) file += 2 * Bt4_Costumes(i);
    return file + costume + (damaged ? Bt4_Costumes(chara) : 0);
}

s32 Bt4_CharaFromModelFile(s32 id) {
    s32 c, base = 0x1612;
    for (c = 0; c < 250; c++) {
        s32 end = base + 2 * Bt4_Costumes(c);
        if (id >= base && id < end) return c;
        base = end;
    }
    return -1;
}
/* File id of a character's model: costume n, or its damaged version. */
s32 ChrView_GetModelFile(s32 chara, s32 costume, s32 damaged) {
#ifdef PORT
    if (gPortVariant != 0) {
        return Bt4_ModelFile(chara, costume, damaged != 0);
    }
#endif
    if (damaged != 0) {
        return chara * 10 + costume + (CHRVIEW_FILE_FIRST + 4);
    }
    return chara * 10 + costume + CHRVIEW_FILE_FIRST;
}

/* The character's entry in the table of common file 4, NULL for an id that is not a character. */
ChrViewInfo *ChrView_GetCharaInfo(u32 chara) {
    ChrViewInfo *table = PACK_AT(gCommonRes->data[2], 1);
    if (chara >= CHRVIEW_CHARA_COUNT) {
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
#ifdef PORT
        s32 index = slot - gChrView->slot;
        if (index >= 0 && index < CHRVIEW_SLOT_COUNT && sBt4ViewerMotion[index] != NULL) {
            Heap_Free(sBt4ViewerMotion[index]);
            sBt4ViewerMotion[index] = NULL;
        }
#endif
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
            OrbitCam_SetDistRange(PORT_V(3.0f, *(f32 *)((u8 *)info + 0x38)), 100.0f);
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
        if (gPortVariant != 0) {
            s32 index = job->slot - gChrView->slot;
            /* BT4 ELF 0x26BF0C: pose file 0xF3C + character, installed before
             * playing animation 7. File_LoadSync allocates the complete file;
             * its third parameter is unused, not a destination capacity. */
            sBt4ViewerMotion[index] = File_LoadSync(0xF3C + job->chara, NULL, 0);
            *(void **)(job->obj + 0x754) = sBt4ViewerMotion[index];
        }
#endif
        BtlObjAnim_PlayModel(job->obj, gChrView->anim, 2);
        BTL_OBJ_FLAGS(job->obj) &= ~0x02000000;
        BtlObj_SetPartVisible(job->obj, 0, 0);
        job->count = 0;
        job->state++;
        /* fall through */
    case 3:
        if (job->count++ >= 3) {
            ChrView_SetupCamera(job->chara);
            BtlObj_SetPartVisible(job->obj, 0, 1);
#ifdef PORT
            if (gPortVariant != 0) {
                s32 part;
                /* BT4's third Show argument includes visible costume parts. */
                for (part = 4; part >= 1; --part) {
                    if ((job->damaged >> part) & 1) BtlObj_SetPartVisible(job->obj, part, 1);
                }
            }
#endif
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
        res->stage = File_Request3(CHRVIEW_FILE_STAGE, NULL, 0);
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
#ifdef PORT
    memset(sBt4ViewerMotion, 0, sizeof(sBt4ViewerMotion));
#endif
    gChrView->anim = 7;
    for (i = 0; i < CHRVIEW_SLOT_COUNT; i++) {
        gChrView->slot[i].file[0].buf = Heap_Alloc(CHRVIEW_MODEL_SIZE, 0x40, 0, HEAP_ANY);
        gChrView->slot[i].file[0].size = 0;
        gChrView->slot[i].file[0].id = -1;
        *(s32 *)gChrView->slot[i].file[0].buf = 0;
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
extern s32 File_PortReadRawStart(s32 id, void *buf, s32 cap);
extern s32 File_PortSize(s32 id);
extern s32 File_PortReadRawPoll(s32 job);
extern void File_PortReadRawCancel(void);
extern u32 Port_LoadTimeUs(void);
extern s32 BtlObjAnim_PortPlay(u8 *obj, void *buf, s32 anim, s32 mode);
extern s32 BtlObjAnim_PortStopped(u8 *obj);
extern s32 BtlObjAnim_PortSpan(u8 *obj, f32 *top, f32 *bottom);
extern void BtlObjAnim_SetStep(u8 *obj, f32 step);
extern void BtlObjAnim_PortPlace(u8 *obj, f32 x, f32 yaw, f32 scale, const f32 *feet, f32 lift);
extern s32 BtlObjAnim_PortFeet(u8 *obj, f32 *feet);
extern void BtlObjAnim_PortSeek(u8 *obj, s32 end);
extern void BtlObjAnim_PortIdle(u8 *obj, f32 seconds);
extern f32 BtlObjAnim_PortBlendStep(u8 *obj);
extern void BtlObjAnim_PortRebind(u8 *obj, void *file);
extern s32 BtlObjAnim_PortBlendPlay(u8 *obj, void *buf, s32 anim, f32 seconds);
extern void BtlObjAnim_PortSpreadClear(void);
extern void BtlObjAnim_PortSpread(u8 *obj, f32 angle);
extern void BtlObjAnim_PortDumpNodes(u8 *obj);
extern s32 BtlObjAnim_PortFeetSpan(u8 *obj, f32 *feet, f32 *top);
extern int Port_SelectArms(int which);
extern int Port_SelectDebug(void);
/* The viewers: 0 and 1 the two sides of the select (1 only with two people choosing); from CHRVIEW_PORT_LINE on, BT4's
   new select's team in a row, one each, made the first time a team is shown (one model buffer each). */
#define CHRVIEW_PORT_VIEWS 7
#define CHRVIEW_PORT_LINE 2
#define CHRVIEW_PORT_LINE_MAX 5
extern u32 *gDmaCur;
extern s32 gDmaVif1Started;
extern u32 gDmaBuf[2];
extern s32 gDmaBufSize;

/* what the screen reads with the game's values in place */
static CHRVIEW_HOST s32 sHostVisible[CHRVIEW_PORT_VIEWS], sHostPedestal[CHRVIEW_PORT_VIEWS];
static CHRVIEW_HOST f32 sHostCx[CHRVIEW_PORT_VIEWS], sHostCy[CHRVIEW_PORT_VIEWS], sHostRx[CHRVIEW_PORT_VIEWS], sHostRy[CHRVIEW_PORT_VIEWS];
static CHRVIEW_HOST void *sHostStage;
static CHRVIEW_HOST s32 sHostLive;
/* BT4's new select (one screen, wide): the fighter large and to one side, coming down from the sky, the camera gliding
   to it. Set by the screen (ChrView_PortStyle) before the world is entered; read there after it (ChrView_PortAir). */
static CHRVIEW_HOST s32 sHostWide;
static CHRVIEW_HOST s32 sHostEntrance = -1; /* whose motions every fighter plays (its entrance, then still); -1 its own */
static CHRVIEW_HOST f32 sHostTurn;          /* turned so much more, to face the camera at the end of that entrance */
static CHRVIEW_HOST f32 sHostAir[CHRVIEW_PORT_VIEWS], sHostLand[CHRVIEW_PORT_VIEWS];
/* BT4's new select, a team complete: its fighters standing in a row in the stage, each in its own entrance (no fall),
   instead of the one fighter (ChrView_PortLineup). 0: the one fighter. */
static CHRVIEW_HOST s32 sHostLine;
static CHRVIEW_HOST s32 sHostStageOk; /* a fight's stage was read (not the character reference's room) */
static CHRVIEW_HOST s32 sHostStageDrawn; /* and drawn all round (BT4's one-screen select): its 2D field is not needed */
static CHRVIEW_HOST s32 sHostLineChara[CHRVIEW_PORT_LINE_MAX], sHostLineCostume[CHRVIEW_PORT_LINE_MAX];
static CHRVIEW_HOST f32 sHostLineX[CHRVIEW_PORT_LINE_MAX];

/* in the world only: one viewer a side (two people choosing, the select in two halves) */
static ChrView *sPortView[CHRVIEW_PORT_VIEWS];
static void *sPortMotion[CHRVIEW_PORT_VIEWS][CHRVIEW_SLOT_COUNT];
static s32 sPortStanding[CHRVIEW_PORT_VIEWS];
static s32 sPortChara[CHRVIEW_PORT_VIEWS], sPortCostume[CHRVIEW_PORT_VIEWS], sPortShown[CHRVIEW_PORT_VIEWS];
static f32 sPortX[CHRVIEW_PORT_VIEWS];
static ChrViewSlot *sPortNew[CHRVIEW_PORT_VIEWS];
static s32 sPortCount[CHRVIEW_PORT_VIEWS];
/* how far the feet of the fighter shown, and of the one on its way, are from where they belong */
static f32 sPortFeet[CHRVIEW_PORT_VIEWS][3];
static f32 sPortFeetNew[CHRVIEW_PORT_VIEWS][3];
static s32 sPortPinned[CHRVIEW_PORT_VIEWS], sPortPinnedNew[CHRVIEW_PORT_VIEWS];
/* A fighter whose files are being read (ChrView_PortReadStep): by a thread of the port's, while the frames go on. Read
   in the frame, as before, they made the frame where the cursor came to rest tens of milliseconds longer on this
   machine alone; online the other machine (on the game's own select) then waited for this one or went back over the
   blanks it had guessed, and both dropped frames. */
static ChrViewSlot *sPortReading[CHRVIEW_PORT_VIEWS]; /* the buffer its files go into; NULL: none being read */
static s32 sPortReadJob[CHRVIEW_PORT_VIEWS];          /* the read under way (File_PortReadRawStart); 0: it was read in the frame */
static s32 sPortReadGot[CHRVIEW_PORT_VIEWS];          /* (that one's bytes) */
static s32 sPortReadCap[CHRVIEW_PORT_VIEWS];          /* (the room it was given) */
/* Each slot's model and motion buffers as large as they were made: the game's sizes, or a mod's larger file. */
static s32 sPortCap[CHRVIEW_PORT_VIEWS][CHRVIEW_SLOT_COUNT][2];
static s32 sPortReadStep[CHRVIEW_PORT_VIEWS];         /* 0 its costume's model, 1 the first costume's model instead, 2 its motions */
static s32 sPortReadChara[CHRVIEW_PORT_VIEWS], sPortReadCostume[CHRVIEW_PORT_VIEWS], sPortReadModel[CHRVIEW_PORT_VIEWS];
static u32 sPortReadSince[CHRVIEW_PORT_VIEWS];
/* BT4: each slot's display pose, the character reference's own (file 0xF3C + character, the model's motion 7: what
   ChrView_StepLoad installs at BtlObj + 0x754). An upright, quiet stance: the fight's hold (0x181) has many fighters
   bent over or never still. */
#define CHRVIEW_PORT_POSE_FILE 0xF3C
#define CHRVIEW_PORT_POSE_SIZE 0x40000
static void *sPortPose[CHRVIEW_PORT_VIEWS][CHRVIEW_SLOT_COUNT];
static s32 sPortPoseGot[CHRVIEW_PORT_VIEWS];
/* the fall: frames since the fighter appeared (CHRVIEW_PORT_FALL of them in the air, then the landing) */
#define CHRVIEW_PORT_FALL 16
#define CHRVIEW_PORT_LAND 14
static s32 sPortFall[CHRVIEW_PORT_VIEWS];
/* the camera's glide to a new fighter: frames since it began (a dolly in over 40) */
static s32 sPortCamTurn;
/* the fighter's size: its tallest span of joints yet (the entrance crouches), what it is on screen, where the camera is
   going; and whether it was still in the air (the landing shows) */
static f32 sPortSpanMax, sPortSeen, sPortDist, sPortAimY;
static f32 sPortSeenLow; /* its lowest joint over the floor, last frame */
static s32 sPortAloft, sPortLandAt;
/* Every fighter coming down in one fighter's entrance (sHostEntrance): played on a smaller body, that motion leaves it
   standing in the air (its hips at the other's height). How far over the floor its feet end, measured at the motion's
   last frame before it is shown (sPortGroundNew: 1 being measured), and the part of it still taken off (1 in the
   entrance, going to 0 as it goes over into its own stance, which stands on the floor). */
static f32 sPortGround[CHRVIEW_PORT_VIEWS], sPortGroundNew[CHRVIEW_PORT_VIEWS], sPortGroundW[CHRVIEW_PORT_VIEWS];
static s32 sPortMeasure[CHRVIEW_PORT_VIEWS];
/* its feet's height and its body's at the end of the entrance (measured before it is shown; body 0: not measured) */
static f32 sPortEndFeet[CHRVIEW_PORT_VIEWS], sPortEndBody[CHRVIEW_PORT_VIEWS], sPortEndFeetNew[CHRVIEW_PORT_VIEWS], sPortEndBodyNew[CHRVIEW_PORT_VIEWS];
static s32 sPortIdleOn[CHRVIEW_PORT_VIEWS]; /* gone into its stance: the blend is being stepped */
/* the character reference's stance read for a slot (sPortPose), to go over into after the entrance */
static s32 sPortPoseOk[CHRVIEW_PORT_VIEWS][CHRVIEW_SLOT_COUNT];
/* BT4's new select, one on one: the fighter's own motions besides the shared entrance's, read with it; after it has
   come down in that entrance it plays its own (its presentation, 0x180) and stays as that ends. sPortPhase: 0 coming
   down, 1 its own presentation, 2 done. */
static void *sPortOwn[CHRVIEW_PORT_VIEWS][CHRVIEW_SLOT_COUNT];
static s32 sPortOwnOk[CHRVIEW_PORT_VIEWS][CHRVIEW_SLOT_COUNT];
static s32 sPortPhase[CHRVIEW_PORT_VIEWS];
static f32 sPortLineDist, sPortLineAim; /* the team's camera, eased */
static f32 sPortLineAt[CHRVIEW_PORT_LINE_MAX]; /* where each of the team stands across, in the world */
/* each side's fighter coming down: its tallest span yet, its lowest joint last frame, whether it has come down some
   way, frames its feet have stood still, and sPortGround as taken off now (eased in) */
static f32 sPortSpanV[CHRVIEW_PORT_VIEWS], sPortLowV[CHRVIEW_PORT_VIEWS], sPortGroundShown[CHRVIEW_PORT_VIEWS];
static s32 sPortFell[CHRVIEW_PORT_VIEWS], sPortStill[CHRVIEW_PORT_VIEWS];
static f32 sPortSpreadNow[CHRVIEW_PORT_VIEWS]; /* the arms opened so much now (eased in once it stands) */
static s32 sPortCurChara[CHRVIEW_PORT_VIEWS];  /* who the model on screen is (sPortShown is already the next one while it is read) */
static s32 sPortShownFor[CHRVIEW_PORT_VIEWS];  /* frames since it was shown (testing's log) */

/* The fighters kept on the pedestal by their feet; every other one stands where its motion puts it. */
#define CHRVIEW_CHARA_FAT_BUU 112 /* (Majin Buu, the fat one) */
#define CHRVIEW_CHARA_GARLIC_JR 118
#define CHRVIEW_CHARA_SLUG 123
#define CHRVIEW_CHARA_METAL_COOLER 128
#define CHRVIEW_CHARA_TAOPAIPAI 148

static s32 ChrView_PortPinned(s32 chara) {
    return chara == CHRVIEW_CHARA_GARLIC_JR || chara == CHRVIEW_CHARA_SLUG || chara == CHRVIEW_CHARA_METAL_COOLER ||
           chara == CHRVIEW_CHARA_TAOPAIPAI;
}

/* A giant (a Great Ape, a giant form: the character table frames it from three times as far): one fighter's entrance
   does not fit its body (its feet ended well under the floor), so it plays its own. */
static s32 ChrView_PortGiant(s32 chara) {
    ChrViewInfo *info;

    if (chara < 0 || chara >= CHRVIEW_CHARA_COUNT) {
        return 0;
    }
    info = ChrView_GetCharaInfo(chara);
    return info != NULL && info->dist > 40.0f;
}

s32 ChrView_PortVisible(s32 side) {
    return sHostVisible[side];
}

/* BT4's new select, one screen: the viewer's world is up with a fight's stage, which it draws all round on every
   blank it is asked for (ChrView_PortFrame). The screen's own field (port_charsel.c Cs_Field) is then under it, never
   seen: left out. */
s32 ChrView_PortStageUp(void) {
    return sHostLive && sHostStageDrawn; /* (host variables: this is asked outside the viewer's world) */
}

/* BT4's new select: the screen's look (wide: the fighter framed large, falling in, the stage around it). */
void ChrView_PortStyle(s32 wide, s32 entrance, f32 turn) {
    sHostWide = wide;
    sHostEntrance = entrance;
    sHostTurn = turn;
}

/* How high the fighter still is (1 at the top of its fall, 0 down) and how far its landing has gone (0..1, 0 none). */
void ChrView_PortAir(s32 side, f32 *air, f32 *land) {
    *air = sHostAir[side];
    *land = sHostLand[side];
}

s32 ChrView_PortPedestal(s32 side, f32 *cx, f32 *cy, f32 *rx, f32 *ry) {
    *cx = sHostCx[side];
    *cy = sHostCy[side];
    *rx = sHostRx[side];
    *ry = sHostRy[side];
    return sHostPedestal[side];
}

/* The viewer's own buffers: from the world's arena only. (Heap_Alloc goes on to the game's heap when the arena is full:
   a block of the game's taken here would change its state, and online the two machines would no longer agree.) NULL:
   no room. */
static void *ChrView_PortMem(s32 size) {
    extern u32 Port_WorldAlloc(s32 size, u32 align);
    u32 at = Port_WorldAlloc(size, 0x40);

    if (at == 0) {
        fprintf(stderr, "bt3: select: no room in the viewer's memory for %d bytes\n", (int)size);
        return NULL;
    }
    return (void *)at;
}

/* Room for a team's viewer (its buffers, its own motions besides the shared entrance's) and still a good margin for what the game's code takes as it makes and moves
   the fighters. */
#define CHRVIEW_PORT_LINE_ROOM (CHRVIEW_MODEL_SIZE + CHRVIEW_PORT_ANIM_SIZE * 2 + CHRVIEW_PORT_MOTION_SIZE + 0x400000)

/* BT4's new select, a team complete: n of its fighters (chara, costume) standing in a row, each at screenX on the
   screen's 512 across; n 0: the one fighter again. Set before ChrView_PortFrame. */
void ChrView_PortLineup(s32 n, const s32 *chara, const s32 *costume, const f32 *screenX) {
    s32 i;

    sHostLine = n < 0 ? 0 : n > CHRVIEW_PORT_LINE_MAX ? CHRVIEW_PORT_LINE_MAX : n;
    for (i = 0; i < sHostLine; i++) {
        sHostLineChara[i] = chara[i];
        sHostLineCostume[i] = costume[i];
        sHostLineX[i] = screenX[i];
    }
}

/* A viewer of slots model buffers (the sides two: one shown, the next read beside it; a team's fighters one). */
static void ChrView_PortOneView(s32 side, s32 slots) {
    s32 i;

    sPortView[side] = gChrView = ChrView_PortMem(sizeof(ChrView));
    if (gChrView == NULL) {
        return;
    }
    memset(gChrView, 0, sizeof(ChrView));
    memset(sPortCap[side], 0, sizeof(sPortCap[side])); /* (the buffers below are the game's sizes again) */
    gChrView->anim = 7;
    for (i = 0; i < slots && i < CHRVIEW_SLOT_COUNT; i++) {
        gChrView->slot[i].file[0].buf = ChrView_PortMem(CHRVIEW_MODEL_SIZE);
        gChrView->slot[i].file[0].id = -1;
        gChrView->slot[i].file[1].buf = ChrView_PortMem(CHRVIEW_PORT_ANIM_SIZE);
        gChrView->slot[i].file[1].id = -1;
        sPortMotion[side][i] = ChrView_PortMem(CHRVIEW_PORT_MOTION_SIZE);
        /* (no character reference's stance any more: the user wants the presentation's own last pose kept) */
        sPortPose[side][i] = NULL;
        sPortPoseOk[side][i] = 0;
        /* (the one fighter's own motions, after the shared entrance: the sides only) */
        sPortOwn[side][i] = side >= CHRVIEW_PORT_LINE ? ChrView_PortMem(CHRVIEW_PORT_ANIM_SIZE) : NULL;
        sPortOwnOk[side][i] = 0;
        if (gChrView->slot[i].file[0].buf == NULL || gChrView->slot[i].file[1].buf == NULL || sPortMotion[side][i] == NULL) {
            break; /* (the viewer's memory is full: fewer buffers) */
        }
        SList_PushFront(&gChrView->free, &gChrView->slot[i].node);
    }
    sPortChara[side] = -1;
    sPortCostume[side] = -1;
    sPortShown[side] = -1;
    sPortNew[side] = NULL;
    sPortReading[side] = NULL;
    sPortStanding[side] = 1;
    sPortGround[side] = sPortGroundNew[side] = 0.0f;
    sPortGroundW[side] = 0.0f;
    sPortMeasure[side] = 0;
    sPortIdleOn[side] = 0;
}

static void ChrView_PortInit(s32 split) {
    ChrViewCommon *res = (ChrViewCommon *)gCommonRes->unk20;
    s32 got;

    Battle_GetWork()->flags &= ~0x100;
    for (got = 0; got < CHRVIEW_PORT_VIEWS; got++) {
        sPortView[got] = NULL; /* (a team's: made when one is first shown) */
    }
    ChrView_PortOneView(0, CHRVIEW_SLOT_COUNT);
    if (split) {
        ChrView_PortOneView(1, CHRVIEW_SLOT_COUNT);
    }
    sPortLineDist = sPortLineAim = 0.0f;
    BtlObj_Init(0);
    BtlObj_SetUnk44FC4(1.0f);
    OrbitCam_Init();
    OrbitCam_SetPadLocked(1);
    sPortFall[0] = sPortFall[1] = -1;
    sPortCamTurn = 0;
    sPortSpanMax = sPortSeen = sPortDist = sPortAimY = 0.0f;
    sPortAloft = 0;
    sPortLandAt = -1;
    {   /* BT4's new select: a fight's own grass stage behind the fighters (as the approved design's field), not the
           character reference's room (CHRVIEW_FILE_STAGE, stage 38). A stage's model is one file, BTL_FILE_STAGE + its id,
           as the fight reads it (battle_load.c), in a buffer of the fight's size. BT4_SELECT_STAGE=<id> (testing,
           Port_SelectStage) picks another; 22 Mount Paozu by default. If the stage cannot be read, the room as before. */
        extern int Port_SelectStage(void); /* (port/src/plat_bt3only.c: the environment is the host's, its pointers 8 bytes) */
        s32 stage = Port_SelectStage();
        s32 size = 0x6CB800; /* (the fight's stage buffer, battle_work.h) */

        res->stage = ChrView_PortMem(size);
        got = res->stage != NULL && stage >= 0 && stage < 38 ? File_PortReadRaw(0x5F3 + stage, res->stage, size) : 0;
        if (got < 0x1000 && res->stage != NULL) {
            stage = -1;
            got = File_PortReadRaw(CHRVIEW_FILE_STAGE, res->stage, size);
        }
        sHostStageOk = stage >= 0;
        fprintf(stderr, "bt3: select: the viewer's world is up (stage %d, %d bytes, %d side(s))\n", (int)stage, (int)got,
                (int)(split ? 2 : 1));
    }
    StgModel_InitStage();
    StgFx_Init();
    Ot_Init();
    BtlScene_Init(8);
    Fade_Start(0, 1, 1.0f);
}

/* One of a fighter's files, into buf: begun on the port's thread, or (no thread) read now. */
static void ChrView_PortRead(s32 side, s32 id, void *buf, s32 cap) {
    sPortReadCap[side] = cap;
    sPortReadJob[side] = File_PortReadRawStart(id, buf, cap);
    if (sPortReadJob[side] == 0) {
        sPortReadGot[side] = File_PortReadRaw(id, buf, cap);
    }
}

/* The room of a slot's model (f 0) or motion (f 1) buffer. */
static s32 ChrView_PortCap(s32 side, ChrViewSlot *slot, s32 f) {
    s32 at = slot - sPortView[side]->slot;
    s32 cap = sPortCap[side][at][f];

    return cap > 0 ? cap : f == 0 ? CHRVIEW_MODEL_SIZE : CHRVIEW_PORT_ANIM_SIZE;
}

/* A read that filled its buffer to the end: a mod's model or motions can be larger than the game's buffer. A fight's
   File_Request reads a file whole whatever the buffer, so such a fighter shows there; here it was cut at the buffer's
   size and the select showed no fighter (or a broken one). The slot's buffer is made as large as the file (the
   viewer's own memory, never the game's) and 1 returned: the file is to be read again into it. 0: it was whole. */
static s32 ChrView_PortGrow(s32 side, ChrViewSlot *slot, s32 f, s32 id, s32 got) {
    s32 at = slot - sPortView[side]->slot;
    s32 size, room;
    void *buf;

    if (got < sPortReadCap[side] || (size = File_PortSize(id)) <= got) {
        return 0;
    }
    room = (size + 0xFFFF) & ~0xFFFF;
    buf = ChrView_PortMem(room);
    if (buf == NULL) {
        fprintf(stderr, "bt3: select: file %d is %d bytes, more than its buffer (%d), and no room for it: cut\n", (int)id, (int)size,
                (int)got);
        return 0;
    }
    fprintf(stderr, "bt3: select: file %d is %d bytes, more than its buffer (%d, a mod's): read again whole\n", (int)id, (int)size,
            (int)got);
    Heap_Free(slot->file[f].buf);
    slot->file[f].buf = buf;
    sPortCap[side][at][f] = room;
    ChrView_PortRead(side, id, buf, room);
    return 1;
}

/* Begins reading a fighter into a free buffer of this side's viewer. 0: no buffer free. */
static s32 ChrView_PortLoad(s32 side, s32 chara, s32 costume) {
    ChrViewSlot *slot = ChrView_AllocSlot();

    if (slot == NULL) {
        return 0;
    }
    sPortReading[side] = slot;
    sPortReadChara[side] = chara;
    sPortReadCostume[side] = costume;
    sPortReadModel[side] = ChrView_GetModelFile(chara, costume, 0);
    sPortReadStep[side] = 0;
    sPortReadSince[side] = Port_LoadTimeUs();
    ChrView_PortRead(side, sPortReadModel[side], slot->file[0].buf, ChrView_PortCap(side, slot, 0));
    return 1;
}

static void ChrView_PortDrop(ChrViewSlot *slot);

/* This side shows no fighter. */
static void ChrView_PortNone(void) {
    if (gChrView->cur != NULL) {
        ChrView_PortDrop(gChrView->cur);
        gChrView->cur = NULL;
        gChrView->visible = 0;
    }
}

/* A frame of the reading: when its files are there, the fighter's object is made. chara and costume: the fighter wanted
   now (one read for a fighter no longer wanted is let go: the next frame reads the one that is). */
static void ChrView_PortReadStep(s32 side, s32 chara, s32 costume) {
    ChrViewSlot *slot = sPortReading[side];
    s32 got = sPortReadJob[side] != 0 ? File_PortReadRawPoll(sPortReadJob[side]) : sPortReadGot[side];
    s32 at;
    u32 *head;
    u8 *obj;

    if (got < 0) {
        return; /* (still being read) */
    }
    sPortReadJob[side] = 0;
    head = (u32 *)slot->file[0].buf;
    if (chara != sPortReadChara[side] || costume != sPortReadCostume[side]) {
        ChrView_FreeSlot(slot);
        sPortReading[side] = NULL;
        return;
    }
    if (sPortReadStep[side] == 0 && got < CHRVIEW_PORT_MODEL_MIN) {
        sPortReadModel[side] = ChrView_GetModelFile(chara, 0, 0);
        sPortReadStep[side] = 1;
        ChrView_PortRead(side, sPortReadModel[side], head, ChrView_PortCap(side, slot, 0));
        return;
    }
    if (sPortReadStep[side] < 3 &&
        ChrView_PortGrow(side, slot, sPortReadStep[side] < 2 ? 0 : 1, sPortReadStep[side] < 2 ? sPortReadModel[side] : slot->file[1].id, got)) {
        return; /* (a mod's larger file: read again whole) */
    }
    head = (u32 *)slot->file[0].buf;
    if (sPortReadStep[side] < 2) {
        /* (what was read has the table a model starts with, and the entries BtlObjMdl_BindFiles goes through) */
        if (got < CHRVIEW_PORT_MODEL_MIN || head[3] == head[4] || head[2] == head[3] || head[12] == head[13]) {
            fprintf(stderr, "bt3: select: fighter %d has no model (file %d, %d bytes; entries 2..4 %x %x %x, 12..13 %x %x)\n", (int)chara,
                    (int)sPortReadModel[side], (int)got, got >= 64 ? (unsigned)head[2] : 0u, got >= 64 ? (unsigned)head[3] : 0u,
                    got >= 64 ? (unsigned)head[4] : 0u, got >= 64 ? (unsigned)head[12] : 0u, got >= 64 ? (unsigned)head[13] : 0u);
            ChrView_FreeSlot(slot);
            sPortReading[side] = NULL;
            ChrView_PortNone();
            return;
        }
        slot->file[0].id = sPortReadModel[side];
        slot->file[0].size = got;
        memset(slot->file[1].buf, 0, 0x1000); /* an empty motion file leaves an empty table */
        /* the fight's motions (entrance 0x180, hold 0x181): BT3's file chara * 10 + 0x598; BT4 renumbers them, 0x141E +
           chara, as the fight asks for them (btl_obj.c BtlObj_RequestCharaModel; SLUS_219.78 0x249C60) */
        slot->file[1].id = PORT_V(chara * 10 + CHRVIEW_FILE_FIRST + 8, 0x141E + chara);
        if (gPortVariant != 0 && sHostEntrance >= 0 && sHostEntrance < CHRVIEW_CHARA_COUNT && !ChrView_PortGiant(chara)) {
            /* BT4's new select: one fighter's motions for all (a skeleton is the same on every fighter): its entrance
               ends standing still, facing the camera, where many fighters' own end bent over or keep moving */
            slot->file[1].id = 0x141E + sHostEntrance;
        }
        sPortReadStep[side] = 2;
        ChrView_PortRead(side, slot->file[1].id, slot->file[1].buf, ChrView_PortCap(side, slot, 1));
        return;
    }
    at = slot - gChrView->slot;
    if (sPortReadStep[side] == 2) {
        slot->file[1].size = got;
        sPortOwnOk[side][at] = 0;
        if (sPortOwn[side][at] != NULL && slot->file[1].id != 0x141E + chara && gPortVariant != 0) {
            /* (it comes down in another's entrance: its own motions too, for its presentation after) */
            sPortReadStep[side] = 3;
            ChrView_PortRead(side, 0x141E + chara, sPortOwn[side][at], CHRVIEW_PORT_ANIM_SIZE);
            return;
        }
        got = 0;
    }
    if (sPortReadStep[side] == 3) {
        sPortOwnOk[side][at] = got >= 0x1000 && got < CHRVIEW_PORT_ANIM_SIZE;
        got = 0;
    }
    /* (the pose: whole, and with a motion's header; cut or missing, the fight's hold stands in) */
    sPortPoseGot[side] = got > 0x40 && got < CHRVIEW_PORT_POSE_SIZE ? got : 0;
    sPortReading[side] = NULL;
    fprintf(stderr, "bt3: select: fighter %d read in %u ms, beside the frames (pose %d bytes)\n", (int)chara,
            (unsigned)((Port_LoadTimeUs() - sPortReadSince[side]) / 1000u), (int)sPortPoseGot[side]);
    at = slot - gChrView->slot;
    slot->obj = BtlObj_Create(3, &slot->file, 1);
    if (slot->obj < 0) {
        ChrView_FreeSlot(slot);
        ChrView_PortNone();
        return;
    }
    obj = BtlObj_Get(slot->obj);
    /* BT4's new select: the entrance first (Goku Black's for the one fighter, coming down from the sky; a team's own
       each), then the character reference's quiet stance in a loop, gone into smoothly (ChrView_PortFrame). Without that
       stance the fight's hold, as on the other selects. */
    sPortPoseOk[side][at] = sPortPoseGot[side] > 0;
    sPortMeasure[side] = 0;
    sPortGroundNew[side] = 0.0f;
    sPortEndBodyNew[side] = 0.0f;
    sPortPhase[side] = 0;
    if (chara == CHRVIEW_CHARA_TAOPAIPAI && sPortPoseOk[side][at]) {
        /* (Taopaipai's entrance comes flying from far off, on his pillar: he goes straight to his stance) */
        *(void **)(obj + 0x754) = sPortPose[side][at];
        BtlObjAnim_PlayModel(obj, 7, 2);
        sPortStanding[side] = 1;
    } else if (chara == CHRVIEW_CHARA_TAOPAIPAI && BtlObjAnim_PortPlay(obj, sPortMotion[side][at], 0x181, 2)) {
        sPortStanding[side] = 1;
    } else if (BtlObjAnim_PortPlay(obj, sPortMotion[side][at], 0x180, 1)) {
        sPortStanding[side] = 0;
        if (gPortVariant != 0 && sHostEntrance >= 0 && !ChrView_PortGiant(chara)) {
            BtlObjAnim_SetStep(obj, 1.3f); /* (the shared entrance, a little quicker: it is the fighter's coming down) */
            if (chara != sHostEntrance) {
                /* its last frame first: where its feet end, measured before it is shown (ChrView_PortFrame), so a small
                   body is brought down from the start and nothing moves after its landing */
                BtlObjAnim_PortSeek(obj, 1);
                sPortMeasure[side] = 1;
            }
        }
    } else if (sPortPoseOk[side][at]) {
        *(void **)(obj + 0x754) = sPortPose[side][at];
        BtlObjAnim_PlayModel(obj, 7, 2);
        sPortStanding[side] = 1;
    } else {
        BtlObjAnim_PlayModel(obj, gChrView->anim, 2);
        sPortStanding[side] = 1;
    }
    BTL_OBJ_FLAGS(obj) &= ~0x32000000; /* (no shadow pass for it, already while it is not on screen) */
    BtlObj_SetPartVisible(obj, 0, 0);
    /* (placed once, upright at the origin: its joints are worked out in its own axes from the next update, which the
       measure of where its feet end needs) */
    BtlObjAnim_PortPlace(obj, 0.0f, 0.0f, 1.0f, NULL, 0.0f);
    sPortNew[side] = slot;
    sPortCount[side] = 0;
    memset(sPortFeetNew[side], 0, sizeof(sPortFeetNew[side]));
    sPortPinnedNew[side] = ChrView_PortPinned(chara);
    sPortShown[side] = chara;
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

/* A fighter read for view v, three frames on (its first poses worked out; one coming down in another's entrance also
   measured, ChrView_PortFrame), takes the place of the one shown. */
static void ChrView_PortShowNew(s32 v, s32 split) {
    extern s32 gPortResim;
    ChrViewSlot *fresh;

    /* (a blank run again counts for nothing: the new fighter's poses are worked out only on blanks run for the first
       time, ChrView_PortFrame) */
    if (sPortNew[v] == NULL || gPortResim || sPortCount[v]++ < 3 || sPortMeasure[v] == 1) {
        return;
    }
    fresh = sPortNew[v];
    sPortNew[v] = NULL;
    if (v < CHRVIEW_PORT_LINE && !split) {
        ChrView_SetupCamera(sPortShown[v]);
    }
    if (v < CHRVIEW_PORT_LINE && sHostWide && !split) {
        /* the camera going from the last fighter to this one, nothing turning; the fall is the entrance's own motion */
        sPortCamTurn = 0;
        sPortSpanMax = 0.0f;
        sPortAloft = 0;
        sPortLandAt = -1;
    }
    sPortFall[v] = -1;
    BtlObj_SetPartVisible(BtlObj_Get(fresh->obj), 0, 1);
    if (gChrView->cur != NULL) {
        ChrView_PortDrop(gChrView->cur);
    }
    gChrView->cur = fresh;
    sPortCurChara[v] = sPortShown[v];
    memcpy(sPortFeet[v], sPortFeetNew[v], sizeof(sPortFeet[v]));
    sPortPinned[v] = sPortPinnedNew[v];
    sPortGround[v] = sPortGroundNew[v]; /* (measured before it was shown: taken off from the start) */
    sPortEndFeet[v] = sPortEndFeetNew[v];
    sPortEndBody[v] = sPortEndBodyNew[v];
    sPortGroundW[v] = 1.0f;
    sPortFell[v] = sPortStill[v] = 0;
    sPortSpreadNow[v] = 0.0f;
    if (Port_SelectDebug()) {
        ChrViewInfo *ti = ChrView_GetCharaInfo(sPortShown[v]);

        if (ti != NULL) {
            fprintf(stderr, "bt3: select: shown %d table dist %d aim %d\n", (int)sPortShown[v], (int)(ti->dist * 1000.0f), (int)(ti->targetY * 1000.0f));
        }
    }
    sPortShownFor[v] = 0;
    sPortSpanV[v] = sPortLowV[v] = 0.0f;
    sPortGroundShown[v] = sPortGround[v];
    sPortIdleOn[v] = 0;
    gChrView->stageOn = 1;
    gChrView->visible = 1;
}

/* One frame. For each side the fighter wanted (chara < 0: none), standing at screenX on the screen's 512 across and turned
   by yaw. split: two people choosing, two fighters in one view: one camera, framed for the first character and a little
   back, each fighter scaled to what its own framing would have made of it. BT4's new select with a team complete
   (ChrView_PortLineup): its fighters in a row instead, the one fighter kept, hidden. What the screen drew before this
   stays behind them. */
void ChrView_PortFrame(f32 *screenX, f32 *yaw, s32 *chara, s32 *costume, s32 split) {
    u32 *dma = gDmaCur;
    s32 vif = gDmaVif1Started;
    ChrViewCommon *res;
    void *gameStage;
    s32 side;
    s32 sides = split ? 2 : 1;
    s32 line = sHostWide && !split ? sHostLine : 0;
    s32 act[CHRVIEW_PORT_VIEWS]; /* the views on screen this frame */
    s32 acts = 0;
    s32 any = 0;
    s32 k;
    extern s32 gPortResim;
    s32 again = gPortResim; /* a blank run again (online): drawn, not stepped */
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
        if (split && sPortView[0] != NULL && sPortView[1] != NULL) {
            gChrView = sPortView[0];
            ChrView_SetupCamera(0);
            OrbitCam_SetDistRange(ref->dist / 0.8f, ref->dist / 0.8f);
            OrbitCam_SetDist(ref->dist / 0.8f);
        }
    }
    if (sPortView[0] == NULL || (split && sPortView[1] == NULL)) {
        sides = 0; /* (no memory for the viewer: nothing is shown, the portrait stands in) */
        line = 0;
    }
    for (side = 0; side < sides; side++) {
        gChrView = sPortView[side];
        /* a new fighter: read beside the frames, on screen three frames after it is there (its first poses have to be
           worked out). (With a team in a row the one fighter stays as it was, hidden, for when the team is left.) */
        if (sPortReading[side] != NULL) {
            ChrView_PortReadStep(side, chara[side], costume[side]);
        } else if (!line && sPortNew[side] == NULL && (chara[side] != sPortChara[side] || costume[side] != sPortCostume[side])) {
            s32 ok = chara[side] >= 0 && chara[side] < CHRVIEW_CHARA_COUNT && ChrView_PortLoad(side, chara[side], costume[side]);

            if (!ok) {
                ChrView_PortNone();
            }
            sPortChara[side] = chara[side];
            sPortCostume[side] = costume[side];
        }
        ChrView_PortShowNew(side, split);
        if (!line) {
            act[acts++] = side;
        }
    }
    if (line) {
        /* the team: one fighter read at a time (each a model and its motions), the others coming as it is there */
        s32 busy = 0;

        for (k = 0; k < CHRVIEW_PORT_LINE_MAX; k++) {
            busy |= sPortView[CHRVIEW_PORT_LINE + k] != NULL && sPortReading[CHRVIEW_PORT_LINE + k] != NULL;
        }
        for (k = 0; k < line; k++) {
            s32 v = CHRVIEW_PORT_LINE + k;
            s32 want = sHostLineChara[k];
            s32 wantCostume = sHostLineCostume[k];

            if (sPortView[v] == NULL) {
                extern u32 Port_WorldFreeBytes(void);

                if (busy || Port_WorldFreeBytes() < (u32)CHRVIEW_PORT_LINE_ROOM) {
                    continue; /* (no room: the team shows the ones there are) */
                }
                ChrView_PortOneView(v, 1);
                if (sPortView[v] == NULL) {
                    break; /* (no memory for it: the ones there are) */
                }
            }
            gChrView = sPortView[v];
            if (sPortReading[v] != NULL) {
                ChrView_PortReadStep(v, want, wantCostume);
            } else if (!busy && sPortNew[v] == NULL && (want != sPortChara[v] || wantCostume != sPortCostume[v])) {
                s32 ok;

                ChrView_PortNone(); /* (its one buffer: the fighter there goes first) */
                ok = want >= 0 && want < CHRVIEW_CHARA_COUNT && ChrView_PortLoad(v, want, wantCostume);
                sPortChara[v] = want;
                sPortCostume[v] = wantCostume;
                busy |= ok;
            }
            ChrView_PortShowNew(v, 0);
            act[acts++] = v;
        }
    }
    /* what is not on screen this frame is hidden (and kept: a team shown again, or left, is there at once) */
    for (k = 0; k < CHRVIEW_PORT_VIEWS; k++) {
        s32 on = 0;
        s32 j;

        if (sPortView[k] == NULL) {
            sHostVisible[k] = 0;
            sHostPedestal[k] = 0;
            continue;
        }
        for (j = 0; j < acts; j++) {
            on |= act[j] == k;
        }
        if (sPortView[k]->cur != NULL) {
            extern void BtlObj_PortSetActive(u8 *obj, s32 on); /* (btl_obj.c: out of every object pass while hidden) */

            BtlObj_SetPartVisible(BtlObj_Get(sPortView[k]->cur->obj), 0, on && sPortView[k]->visible);
            BtlObj_PortSetActive(BtlObj_Get(sPortView[k]->cur->obj), on);
        }
        if (!on) {
            sHostVisible[k] = 0;
            sHostPedestal[k] = 0;
        }
    }
    if (line) {
        /* The team in a row, laid out in the world by each one's own size (the character table's distance for it, as
           large as the body: a giant takes three times a man's room), the first in the middle and the others out to
           either side, a gap between each: no one stands inside another, a giant included. The camera then takes in
           the whole row and its tallest from head to feet, a little from above, eased. */
        f32 far = 0.0f;
        f32 left = 0.0f;
        f32 right = 0.0f;
        f32 width = 0.0f;
        f32 aim;

        for (k = 0; k < line; k++) {
            ChrViewInfo *info = NULL;
            f32 size = 25.0f;
            f32 w;

            if (sHostLineChara[k] >= 0 && sHostLineChara[k] < CHRVIEW_CHARA_COUNT) { /* (no pointer chosen by ?: here) */
                info = ChrView_GetCharaInfo(sHostLineChara[k]);
            }
            if (info != NULL && info->dist > 1.0f) {
                size = info->dist;
            }
            far = size > far ? size : far;
            w = size * 0.42f; /* (a body's room across: Goku's table distance 25, some 10 across with his arms) */
            if (k == 0) {
                sPortLineAt[k] = 0.0f;
                left = -w * 0.5f;
                right = w * 0.5f;
            } else if (k & 1) {
                sPortLineAt[k] = left - 2.5f - w * 0.5f;
                left = sPortLineAt[k] - w * 0.5f;
            } else {
                sPortLineAt[k] = right + 2.5f + w * 0.5f;
                right = sPortLineAt[k] + w * 0.5f;
            }
        }
        for (k = 0; k < line; k++) {
            sPortLineAt[k] -= (left + right) * 0.5f; /* (the row in the middle) */
        }
        width = right - left;
        aim = -far * 0.63f * 0.5f; /* (its tallest's middle: a body is some 0.63 of its table distance tall) */
        if (far > 0.0f) {
            /* far enough for the row across the screen's width (0.645 of it, as the one fighter's framing shows) and
               for the tallest from head to feet (0.86 of its table distance) */
            f32 goal = width * 0.66f > far * 0.92f ? width * 0.66f : far * 0.92f;

            if (sPortLineDist <= 0.0f) {
                sPortLineDist = goal;
                sPortLineAim = aim;
            }
            if (!again) { /* (eased per blank shown: not again on a blank run again) */
                sPortLineDist += (goal - sPortLineDist) * 0.1f;
                sPortLineAim += (aim - sPortLineAim) * 0.1f;
            }
            OrbitCam_SetFloorClamp(0);
            OrbitCam_SetPitchLimit(0.78539816f);
            OrbitCam_SetTarget(0.0f, sPortLineAim, 0.0f);
            OrbitCam_SetAngles(-0.04f, 3.14159265f, 0.0f);
            OrbitCam_SetDistRange(sPortLineDist, sPortLineDist);
            OrbitCam_SetDist(sPortLineDist);
        }
    } else if (sHostWide && !split && sides > 0 && sPortView[0]->cur != NULL) {
        ChrViewInfo *info = ChrView_GetCharaInfo(sPortShown[0]);

        sPortLineDist = 0.0f; /* (a team shown again starts from its own framing) */
        if (info != NULL) {
            /* As the user's picture, the same for every fighter by its own measured size: the camera as far as 0.829
               of its body and looking at 0.812 of it over the floor it stands on (the numbers of Goku's framing the user
               approved: the character table's, 0.52 of its distance and 1.68 of its height, over a body of 15.9 whose
               feet end 2.9 up). A giant from further off, by the table. Not measured (the entrance's own fighter): the
               table, as approved. Eased: from one fighter to the next the camera glides, it does not cut. */
            extern int Port_SelectCam(int which); /* (port/src/plat_bt3only.c: thousandths, testing) */
            f32 t = sPortCamTurn < 40 ? (f32)sPortCamTurn / 40.0f : 1.0f;
            f32 e = t * t * (3.0f - 2.0f * t);
            f32 body = sPortEndBody[0];
            f32 goal;
            f32 aim;

            if (ChrView_PortGiant(sPortShown[0])) {
                goal = info->dist * 0.78f;
                aim = info->targetY * 1.35f;
            } else if (body > 1.0f) {
                f32 floor = sPortEndFeet[0] - sPortGround[0] - body * 0.047f;

                goal = body * 0.829f;
                aim = -(floor + body * 0.812f);
            } else {
                goal = info->dist * 0.52f;
                aim = info->targetY * 1.68f;
            }
            goal *= (f32)Port_SelectCam(0) * 0.001f * (1.06f - 0.06f * e);
            aim *= (f32)Port_SelectCam(1) * 0.001f;
            if (sPortDist <= 0.0f) {
                sPortDist = goal;
                sPortAimY = aim;
            }
            if (!again) {
                sPortDist += (goal - sPortDist) * 0.12f;
                sPortAimY += (aim - sPortAimY) * 0.12f;
            }
            OrbitCam_SetFloorClamp(0);
            OrbitCam_SetPitchLimit(0.78539816f);
            OrbitCam_SetTarget(0.0f, sPortAimY, 0.0f);
            OrbitCam_SetAngles(-0.04f, 3.14159265f, 0.0f);
            OrbitCam_SetDistRange(sPortDist, sPortDist);
            OrbitCam_SetDist(sPortDist);
        }
        if (sPortCamTurn < 40 && !again) {
            sPortCamTurn++;
        }
    }
    OrbitCam_Update(0);
    gBtlStage->flags &= ~1;
    for (k = 0; k < acts; k++) {
        s32 v = act[k];
        s32 inLine = v >= CHRVIEW_PORT_LINE;
        ChrViewSlot *cur;

        gChrView = sPortView[v];
        cur = gChrView->cur;
        sHostVisible[v] = gChrView->visible;
        any |= gChrView->visible;
        if (cur != NULL) {
            u8 *obj = BtlObj_Get(cur->obj);
            ChrViewInfo *info = ChrView_GetCharaInfo(sPortShown[v]);
            f32 scale = 1.0f;
            f32 x = inLine ? sHostLineX[v - CHRVIEW_PORT_LINE] : screenX[v];
            s32 at = cur - gChrView->slot;

            if (!sPortStanding[v] && BtlObjAnim_PortStopped(obj)) {
                /* the entrance done: into the character reference's stance, smoothly (BT4's new select); else the
                   fight's hold, or (one fighter's entrance for all, which ends standing) its last frame */
                if (gPortVariant != 0 && sPortPhase[v] == 0 && sPortOwnOk[v][at] && inLine) {
                    /* a team (View Team): come down in the shared entrance, now its own presentation, out of that pose
                       smoothly (one on one stays as it lands, as the user approved); its feet
                       go back to its own motion's ground as the two mix (sPortGroundW) */
                    BtlObjAnim_PortRebind(obj, sPortOwn[v][at]);
                    if (BtlObjAnim_PortBlendPlay(obj, sPortMotion[v][at], 0x180, 0.4f)) {
                        sPortIdleOn[v] = 1;
                        sPortPhase[v] = 1;
                    } else {
                        sPortPhase[v] = 2;
                        sPortStanding[v] = 1;
                    }
                } else if (gPortVariant != 0) {
                    /* BT4's select (the user): it keeps the last pose of its presentation, never a fighting stance */
                    sPortPhase[v] = 2;
                    sPortStanding[v] = 1;
                } else {
                    BtlObjAnim_PortPlay(obj, sPortMotion[v][at], 0x181, 2);
                    sPortStanding[v] = 1;
                }
            }
            if (inLine) {
                sPortX[v] = sPortLineAt[v - CHRVIEW_PORT_LINE];
            } else if (info != NULL) {
                f32 a;
                f32 b;
                f32 unused;
                f32 h = split ? ref->targetY : info->targetY;

                ChrView_PortProject(0.0f, h, 0.0f, &a, &unused);
                ChrView_PortProject(1.0f, h, 0.0f, &b, &unused);
                sPortX[v] = b != a ? (x - a) / (b - a) : 0.0f;
                if (split && info->dist > 0.0f) {
                    scale = ref->dist / info->dist;
                }
            }
            BTL_OBJ_FLAGS(obj) &= ~0x30000000; /* no hard shadow: the screen lays a soft one on the pedestal */
            {
                /* brought down by as much as it would end over the floor (its entrance someone else's), less as it goes
                   over into its own stance */
                f32 lift = -sPortGroundShown[v] * sPortGroundW[v];
                f32 turn = sHostWide && !split && !inLine ? sHostTurn : 0.0f;
                f32 y = inLine ? 0.0f : yaw[v];

                sHostAir[v] = 0.0f;
                sHostLand[v] = 0.0f;
                if (sHostWide && !split && !inLine && sPortSpanMax > 1.0f) {
                    /* in the air while its lowest joint is well off the floor; the dust when it comes down */
                    f32 low = sPortSeenLow - sPortGroundShown[v] * sPortGroundW[v];
                    f32 air = low / (sPortSpanMax * 1.5f);

                    sHostAir[v] = air < 0.0f ? 0.0f : air > 1.0f ? 1.0f : air;
                    if (low > sPortSpanMax * 0.25f) {
                        sPortAloft = 1;
                    } else if (sPortAloft) {
                        sPortAloft = 0;
                        sPortLandAt = 0;
                    }
                    if (sPortLandAt >= 0) {
                        sHostLand[v] = (f32)sPortLandAt / 16.0f;
                        if (!again) {
                            sPortLandAt = sPortLandAt + 1 < 16 ? sPortLandAt + 1 : -1;
                        }
                    }
                }
                /* (Metal Cooler only, the user: on BT4's select he comes down from the sky in the shared entrance as
                   the others, and one on one stays as he lands; kept by his feet on the pedestal, that fall was taken
                   off. His feet are still followed meanwhile: in a team's row his own presentation after the fall is
                   kept on its place by them, as before) */
                s32 freeFall = sPortCurChara[v] == CHRVIEW_CHARA_METAL_COOLER && gPortVariant != 0 && sHostEntrance >= 0 &&
                               (sPortPhase[v] == 0 || !inLine);

                if (sPortPinned[v] && !freeFall) {
                    BtlObjAnim_PortPlace(obj, sPortX[v], y + turn, scale, &sPortFeet[v][0], lift);
                } else {
                    BtlObjAnim_PortPlace(obj, sPortX[v], y + turn, scale, NULL, lift);
                }
            }
        }
    }
    /* Online, blanks run again after a rollback (and the session's start-up, run ahead) show no picture of their own
       (the last one's list stands in: drawn below as ever, the screen's comment in port_charsel.c): the fighters are
       drawn as they stand, nothing of theirs steps on. Before, every such blank animated and posed them again: a
       rollback of eight blanks at a cursor move of the other player cost eight frames of skeletons in one (the hitch
       a player on this select had where one on the game's own had none), and their motions ran that much ahead. */
    BtlObjAnim_PortSpreadClear();
    for (k = 0; k < acts; k++) {
        /* one on one: the arms a little closer to the body all along, from the top of its fall (the user: not a
           change once it is down). BtlObj_UpdateAll turns them. */
        s32 v = act[k];

        /* one on one: the arms closer to the body all along, from the top of its fall to the end (as approved);
           never on a team's (its own presentation) nor a giant */
        if (gPortVariant != 0 && sHostEntrance >= 0 && sPortView[v]->cur != NULL && sPortCurChara[v] == CHRVIEW_CHARA_FAT_BUU) {
            /* Fat Buu only (the user): the shared entrance's arms go into his body; opened a little instead, from the
               top of his fall. On a team's row only while he comes down in it, let go as he goes into his own
               presentation (sPortGroundW: 1 .. 0 over that blend). */
            extern int Port_SelectArmsBuu(void);
            f32 open = (f32)Port_SelectArmsBuu() * 0.001f;

            sPortSpreadNow[v] = v < CHRVIEW_PORT_LINE || sPortPhase[v] == 0 ? open : sPortPhase[v] == 1 ? open * sPortGroundW[v] : 0.0f;
        } else if (v < CHRVIEW_PORT_LINE && gPortVariant != 0 && sHostEntrance >= 0 && !ChrView_PortGiant(sPortShown[v])) {
            sPortSpreadNow[v] = (f32)Port_SelectArms(4) * 0.001f;
        } else {
            sPortSpreadNow[v] = 0.0f;
        }
        if (sPortView[v]->cur != NULL && (sPortSpreadNow[v] > 0.002f || sPortSpreadNow[v] < -0.002f)) {
            BtlObjAnim_PortSpread(BtlObj_Get(sPortView[v]->cur->obj), sPortSpreadNow[v]);
        }
    }
    if (!again) {
        BtlObj_UpdateAll();
    }
    for (k = 0; k < acts && !again; k++) {
        /* Down: a side's fighter (one fighter's entrance for all, BT4) has come down and its lowest joint stopped,
           three frames still. What is left between it and the floor (a body smaller than the entrance's own stands in
           the air) is taken off, quickly but smoothly. */
        s32 v = act[k];
        f32 top;
        f32 low;

        if (v >= CHRVIEW_PORT_LINE || sPortView[v]->cur == NULL || gPortVariant == 0 || sHostEntrance < 0 ||
            !BtlObjAnim_PortFeetSpan(BtlObj_Get(sPortView[v]->cur->obj), &low, &top)) {
            continue;
        }
        if (top - low > sPortSpanV[v]) {
            sPortSpanV[v] = top - low;
        }
        if (Port_SelectDebug() && sPortShownFor[v] == 85) {
            fprintf(stderr, "bt3: select: sum %d feet %d top %d span %d fell %d still %d ground %d standing %d\n", (int)sPortShown[v],
                    (int)(low * 1000.0f), (int)(top * 1000.0f), (int)(sPortSpanV[v] * 1000.0f), (int)sPortFell[v], (int)sPortStill[v],
                    (int)(sPortGround[v] * 1000.0f), (int)sPortStanding[v]);
        }
        if (Port_SelectDebug() > 1 && sPortShownFor[v] < 200) {
            /* (in thousandths, as whole numbers: the game's code passes no floating point to printf) */
            fprintf(stderr, "bt3: select: debug %d f%d feet %d top %d span %d fell %d still %d ground %d standing %d\n", (int)sPortShown[v],
                    (int)sPortShownFor[v], (int)(low * 1000.0f), (int)(top * 1000.0f), (int)(sPortSpanV[v] * 1000.0f), (int)sPortFell[v],
                    (int)sPortStill[v], (int)(sPortGround[v] * 1000.0f), (int)sPortStanding[v]);
        }
        if (Port_SelectDebug() > 1 && sPortShownFor[v] == 199) {
            BtlObjAnim_PortDumpNodes(BtlObj_Get(sPortView[v]->cur->obj));
        }
        sPortShownFor[v]++;
        if (sPortSpanV[v] > 1.0f && sPortStill[v] < 3) {
            f32 fall = sPortLowV[v] - low;

            if (fall > sPortSpanV[v] * 0.01f) {
                sPortFell[v] = 1;
                sPortStill[v] = 0;
            } else if (sPortFell[v] && fall < sPortSpanV[v] * 0.004f && fall > -sPortSpanV[v] * 0.004f) {
                if (++sPortStill[v] == 3) {
                    f32 rest = sPortSpanV[v] * 0.047f; /* (the ankle's joint stands that far over the sole) */

                    /* (only a small fighter, a body shorter than 14: on the others what is left does not show, and
                       taking it off after the landing read as a jolt to the user) */
                    (void)rest; /* (the ground is measured before it is shown now: nothing changes after the landing) */
                    fprintf(stderr, "bt3: select: fighter %d down, feet %d thousandths over the floor (body %d): brought down\n",
                            (int)sPortShown[v], (int)(sPortGround[v] * 1000.0f), (int)(sPortSpanV[v] * 1000.0f));
                }
            } else {
                sPortStill[v] = 0;
            }
        }
        sPortLowV[v] = low;
        sPortGroundShown[v] = sPortGround[v];
    }
    for (k = 0; k < CHRVIEW_PORT_VIEWS; k++) {
        if (sPortView[k] == NULL) {
            continue;
        }
        if (again) {
            break; /* (nothing was worked out this blank: nothing to measure or blend) */
        }
        if (sPortNew[k] != NULL && sPortMeasure[k] == 1 && sPortCount[k] >= 2) {
            /* the entrance's last frame worked out (by an update of its own): its feet's lowest joint that far over the
               floor; a small body (shorter than 14: the entrance's own is some 16) is taken down by what is left above
               an ankle's height. (Measured on the feet: the root's nodes stay at the origin.) */
            u8 *obj = BtlObj_Get(sPortNew[k]->obj);
            f32 feet;
            f32 top;

            sPortGroundNew[k] = 0.0f;
            sPortEndBodyNew[k] = 0.0f;
            if (BtlObjAnim_PortFeetSpan(obj, &feet, &top) && top - feet > 1.0f) {
                f32 span = top - feet;
                f32 rest = span * 0.047f;

                sPortEndFeetNew[k] = feet;
                sPortEndBodyNew[k] = span;
                if (span < 14.0f && feet > rest) {
                    sPortGroundNew[k] = feet - rest;
                }
                if (Port_SelectDebug()) {
                    fprintf(stderr, "bt3: select: fighter %d ends with its feet at %d (body %d): down by %d\n", (int)sPortShown[k], (int)(feet * 1000.0f),
                            (int)(span * 1000.0f), (int)(sPortGroundNew[k] * 1000.0f));
                }
            }
            BtlObjAnim_PortSeek(obj, 0);
            sPortMeasure[k] = 2;
        }
        if (sPortView[k]->cur != NULL && sPortIdleOn[k]) {
            sPortGroundW[k] = BtlObjAnim_PortBlendStep(BtlObj_Get(sPortView[k]->cur->obj));
        }
    }
    if (sHostWide && !split && !line && sides > 0 && sPortView[0]->cur != NULL) {
        u8 *obj = BtlObj_Get(sPortView[0]->cur->obj);
        f32 top;
        f32 low;

        if (BtlObjAnim_PortFeetSpan(obj, &low, &top)) {
            f32 a;
            f32 b;
            f32 sx;

            if (top - low > sPortSpanMax) {
                sPortSpanMax = top - low;
            }
            sPortSeenLow = low;
            /* how tall a body of that span stands on the screen now */
            ChrView_PortProject(sPortX[0], 0.0f, 0.0f, &sx, &a);
            ChrView_PortProject(sPortX[0], -sPortSpanMax * 1.12f, 0.0f, &sx, &b);
            sPortSeen = a > b ? a - b : b - a;
        }
    }
    for (k = 0; k < CHRVIEW_PORT_VIEWS; k++) {
        ChrViewSlot *cur;
        f32 now[3];
        s32 j;

        if (sPortView[k] == NULL) {
            continue;
        }
        cur = sPortView[k]->cur;
        sHostPedestal[k] = 0;
        /* (eased: a pose going on to the next does not jump, and a walk on the spot does not rock the fighter) */
        if (cur != NULL && sPortPinned[k] && BtlObjAnim_PortFeet(BtlObj_Get(cur->obj), now)) {
            for (j = 0; j < 3; j++) {
                sPortFeet[k][j] += (now[j] - sPortFeet[k][j]) * 0.2f;
            }
        }
        if (sPortNew[k] != NULL && sPortPinnedNew[k]) {
            BtlObjAnim_PortFeet(BtlObj_Get(sPortNew[k]->obj), sPortFeetNew[k]);
        }
    }
    if ((any || sHostWide) && sPortView[0] != NULL) {
        gChrView = sPortView[0];
        BtlObj_UpdateVisibility(0);
        BtlObj_FinishVisibility(0);
        BtlStage_Update();
        BtlScene_Update();
        if (sHostWide || (gPortVariant != 0 && sHostStageOk)) {
            /* BT4's new select (and BT4 with two choosing): the fight's stage all around (the BT3 select drew no room: its backdrop is 2D) */
            Gfx_MarkPass(1);
            StgModel_Cull(gBtlCamView);
            StgModel_Draw(gBtlCamView);
            sHostStageDrawn = sHostWide && sHostStageOk;
            /* (not the stage's effects, StgFx_DrawPreNoCheck: on Mount Paozu they doubled the render passes and the
               select fell to some 40 pictures a second on an Adreno 710) */
        }
        Gfx_MarkPass(2);
        BtlObjDraw_Draw();
        Gfx_MarkPass(0);
        Gfx_MarkPass(4);
        Gfx_AddDefaultEnv();
        BtlScene_Draw(1);
        Ot_Draw();
        Gfx_MarkPass(0);
        for (k = 0; k < acts; k++) {
            s32 v = act[k];
            ChrViewInfo *info = ChrView_GetCharaInfo(sPortShown[v]);

            if (info != NULL && sPortView[v]->visible && sPortView[v]->cur != NULL) {
                f32 s = split && info->dist > 0.0f ? ref->dist / info->dist : 1.0f;
                f32 r = info->targetY * s * 0.62f;
                f32 cx;
                f32 cy;
                f32 ex;
                f32 ey;
                f32 fx;
                f32 fy;

                ChrView_PortProject(sPortX[v], 0.0f, 0.0f, &cx, &cy);
                ChrView_PortProject(sPortX[v] + r, 0.0f, 0.0f, &ex, &ey);
                ChrView_PortProject(sPortX[v], 0.0f, r, &fx, &fy);
                sHostCx[v] = cx;
                sHostCy[v] = cy;
                sHostRx[v] = ex > cx ? ex - cx : cx - ex;
                sHostRy[v] = fy > cy ? fy - cy : cy - fy;
                sHostPedestal[v] = 1;
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
    File_PortReadRawCancel(); /* (no fighter's file is written into the viewer's memory any more: it is let go) */
    if (sHostLive) {
        Port_WorldForget();
    }
    sHostLive = 0;
    sHostStageDrawn = 0;
    memset(sHostVisible, 0, sizeof(sHostVisible));
    memset(sHostPedestal, 0, sizeof(sHostPedestal));
    sHostLine = 0;
    sHostStage = NULL;
}
#endif

/* Ends the viewer and frees everything ChrView_Init set up, the backdrop file included. */
void ChrView_Term(void) {
    s32 i;

    for (i = 0; i < CHRVIEW_SLOT_COUNT; i++) {
        Heap_Free(gChrView->slot[i].file[0].buf);
    }
    Heap_Free(gChrView);
    File_CancelRequests();
    Job_Clear();
    BtlObj_Term();
#ifdef PORT
    for (i = 0; i < CHRVIEW_SLOT_COUNT; i++) {
        if (sBt4ViewerMotion[i] != NULL) Heap_Free(sBt4ViewerMotion[i]);
        sBt4ViewerMotion[i] = NULL;
    }
#endif
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
