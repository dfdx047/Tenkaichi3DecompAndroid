#include "common.h"
#include "battle/eft_e.h"
#include "sys/gfx_ot.h"

/*
 * Effect code 0x13F430..0x147050. See include/battle/eft_e.h and eft_f.h for the modules and their layouts.
 * As linked: the former head of this file (0x13EA00..0x13F430, the transition task) moved to the end of
 * eft_d_b.c, and the former eft_f.c (0x142CA0..0x147050) is appended below as the second part:
 * EftWaterRing_Update only matches with EftWater_GetSurfaceY defined earlier in its file.
 *
 *   0x13F430..0x140338  EftSteam_*: particle emitters of the stage (layer 0, sub-task 5).
 *   0x140338..0x142CA0  EftWater_*: water splashes, technique spray trails and fighter wakes (layer 0, sub-task
 *                       7).
 *   0x142CA0..0x147050  the particle pools they fill (second part).
 *
 * Other modules are declared locally with this file's own view types (the integrator was linking while this was
 * written). Names of the task system (BtlTask*), EftCam_*, EftStage_*, EftHit_*, EftUtil_*, EftRec_*, BtlStage_*
 * and the EftWater* pool functions are the ones recorded in config/symbols by their owners.
 *
 * Random numbers: everything here draws from the C library rand() (EFT_RANDF) except EftSteam_Emit, which uses
 * the VU0 generator through Rand_FloatRange. No draw reaches a fighter, a hit record or a battle object.
 */

extern void *Heap_Alloc(s32 size, u32 align, s32 fromTail, s32 heap);
extern void Heap_Free(void *ptr);
extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 sinf(f32 x);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern void *BtlTask_CreateChildList(void *task, s32 capacity, s32 workSize);
extern void *BtlTaskList_AddTail(void *list, BtlTaskClass *cls, void *arg);
extern void BtlTask_SetDead(void *task);
extern void EftCam_Start(s32 *arg);
extern void EftCam_SetHold(s32 arg);
extern void EftCam_Stop(void);
extern void Gfx_AddDefaultEnv(void);
extern void EftBurst_DrawModel(void *model, s32 arg);
extern void EftBurst_SetTexture(void *tex, s32 x, s32 y);
extern void EftBurst_RelocateModel(void *model);
extern void Res_RelocateOffsets(void *dst, void *base, void *table);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);

extern s32 BtlStage_GetFxResC(void);
extern s32 BtlStage_GetFxResA(void);
extern EftStageMarker *BtlStage_GetFxResC2(void);
extern void EftTexSet_Load8(EftSteamTex *tex, s32 *data);
extern u64 EftVram_AddTex(u64 *tex0, s32 a1, s32 a2);
extern s32 EftStage_IsDrawOn(void);
extern void Vu0Cur_Push(void);
extern void Vu0Cur_LoadMtx(Mtx44 *mtx);
extern void Vu0Cur_Pop(void);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern f32 Rand_FloatRange(f32 a, f32 b);

/* The stage state (gp 0x2FEBE0); only the flag word. */
typedef struct EftStageState {
    s32 unk0[2];
    s32 flags; /* bit 0: not drawable (BtlStage_IsReady tests it too) */
} EftStageState;
extern EftStageState *gBtlStage;

/* The view being drawn (btl_cam.h); only the matrices and position used here. */
typedef struct EftView {
    /* 0x000 */ Mtx44 world2view;
    /* 0x040 */ Mtx44 world2view2;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 world2screen;
    /* 0x180 */ u8 unk180[0xA0];
    /* 0x220 */ Vec4 pos;
} EftView;
extern EftView *gBtlCamView;

extern f32 sqrtf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern s32 BtlStage_GetWaterLevel(f32 *y);
extern s32 BtlStage_GetId(void);
extern s32 Battle_IsSplitScreen(void);
extern s32 BtlStage_IsReady(void);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern f32 BtlCharApi_GetGroundY(s32 objId);
extern void BtlCharApi_GetPos(s32 objId, Vec4 *out);
extern void BtlCharApi_GetDir(s32 objId, Vec4 *out);
extern f32 BtlCharApi_GetSpeed(s32 objId);
extern s32 BtlCharApi_IsInTechnique(s32 objId);
extern void BtlCharApi_PlaySoundAt(Vec4 *pos, s32 kind, s32 id, f32 near, f32 far);
extern f32 EftHit_GetRadiusA(EftWaterBlast *rec);
extern void EftUtil_ClipSegToWater(EftEVec *out, Vec4 *a, Vec4 *b);
extern s32 EftRec_GetDefClass(EftWaterBlast *rec);
extern void EftTexSet_Load32(u8 *tex, s32 *data);
extern void EftWater_UpdateTextures(s32 a0, s32 a1);
/* The pool functions below are defined in the second part of this file (formerly eft_f.c) with that part's own
 * types; this part calls them through aliased declarations with its view types (same symbol, same code). */
extern void EftWaterSplash_UpdateList_e(EftWaterSplash **head, EftWaterSplash **tail) __asm__("EftWaterSplash_UpdateList");
#define EftWaterSplash_UpdateList EftWaterSplash_UpdateList_e
extern void EftWaterSplash_DrawList_e(void *head) __asm__("EftWaterSplash_DrawList");
#define EftWaterSplash_DrawList EftWaterSplash_DrawList_e
extern void EftWaterTrail_Spawn_e(void **head, void **tail, s32 objId, EftWaterBlast *rec) __asm__("EftWaterTrail_Spawn");
#define EftWaterTrail_Spawn EftWaterTrail_Spawn_e
extern void EftWaterTrail_UpdateList_e(void **head, void **tail) __asm__("EftWaterTrail_UpdateList");
#define EftWaterTrail_UpdateList EftWaterTrail_UpdateList_e
extern void EftWaterTrail_DrawList_e(void *head) __asm__("EftWaterTrail_DrawList");
#define EftWaterTrail_DrawList EftWaterTrail_DrawList_e
extern void EftWaterDrop_UpdateList_e(void **head, void **tail) __asm__("EftWaterDrop_UpdateList");
#define EftWaterDrop_UpdateList EftWaterDrop_UpdateList_e
extern void EftWaterDrop_DrawList_e(void *head, EftEVec origin) __asm__("EftWaterDrop_DrawList");
#define EftWaterDrop_DrawList EftWaterDrop_DrawList_e
extern void EftWaterRing_UpdateList_e(void **head, void **tail) __asm__("EftWaterRing_UpdateList");
#define EftWaterRing_UpdateList EftWaterRing_UpdateList_e
extern void EftWaterRing_DrawList_e(void *head) __asm__("EftWaterRing_DrawList");
#define EftWaterRing_DrawList EftWaterRing_DrawList_e
extern void EftWaterSpray_UpdateList_e(void **head, void **tail) __asm__("EftWaterSpray_UpdateList");
#define EftWaterSpray_UpdateList EftWaterSpray_UpdateList_e
extern void EftWaterSpray_DrawList_e(void *head, EftEVec origin) __asm__("EftWaterSpray_DrawList");
#define EftWaterSpray_DrawList EftWaterSpray_DrawList_e
extern void EftWaterMist_UpdateList_e(void **head, void **tail) __asm__("EftWaterMist_UpdateList");
#define EftWaterMist_UpdateList EftWaterMist_UpdateList_e
extern void EftWaterMist_DrawList_e(void *head) __asm__("EftWaterMist_DrawList");
#define EftWaterMist_DrawList EftWaterMist_DrawList_e

/* Billboard: yaw / pitch / speed of its flight, rot, size and its growth, gravity, life, start delay. */
extern void EftWaterDrop_Spawn_e(void **head, void **tail, EftEVec pos, f32 yaw, f32 pitch, f32 speed, f32 rot, f32 rotVel,
                          f32 size, f32 sizeVel, f32 sizeDamp, f32 gravity, f32 life, f32 delay, u8 r, u8 g, u8 b,
                          u8 tex, u8 layer, s32 flags) __asm__("EftWaterDrop_Spawn");
#define EftWaterDrop_Spawn EftWaterDrop_Spawn_e
/* Ring lying on the surface. */
extern void EftWaterRing_Spawn_e(void **head, void **tail, EftEVec pos, f32 rot, f32 size, f32 sizeVel, f32 sizeDamp,
                          f32 alphaMax, f32 time, f32 delay, u8 r, u8 g, u8 b, u8 tex, u8 layer, s32 flags) __asm__("EftWaterRing_Spawn");
#define EftWaterRing_Spawn EftWaterRing_Spawn_e
/* Streak thrown out along a direction. */
extern void EftWaterSpray_Spawn_e(void **head, void **tail, EftEVec pos, f32 dist, f32 height, f32 yaw, f32 pitch, f32 roll,
                          f32 tilt, f32 tiltEnd, f32 speed, f32 size, f32 sizeVel, f32 sizeDamp, f32 gravity,
                          f32 nearScale, f32 widthScale, f32 life, f32 delay, u8 r, u8 g, u8 b, u8 tex, u8 layer,
                          s32 flags) __asm__("EftWaterSpray_Spawn");
#define EftWaterSpray_Spawn EftWaterSpray_Spawn_e
/* Streak that orbits. */
extern void EftWaterMist_Spawn_e(void **head, void **tail, EftEVec pos, f32 radius, f32 height, f32 yaw, f32 yawVel,
                          f32 yawDamp, f32 pitch, f32 roll, f32 tilt, f32 tiltEnd, f32 speed, f32 size, f32 sizeVel,
                          f32 sizeDamp, f32 gravity, f32 nearScale, f32 widthScale, f32 life, f32 delay, u8 r, u8 g,
                          u8 b, u8 tex, u8 layer) __asm__("EftWaterMist_Spawn");
#define EftWaterMist_Spawn EftWaterMist_Spawn_e

#define EFT_RANDF() ((f32)rand() / 2147483647.0f)
#define EFT_UNIT(objId) (BtlCharApi_GetHeight(objId) * 0.05f)
#define EFT_SCALE(objId, scale) (BtlCharApi_GetHeight(objId) * 0.05f * (scale))
#define EFT_PAUSED() (Battle_GetWork()->flags & 0x100)

/* The battle work (battle.h); only the flag word. */
typedef struct EftBattleWork {
    u8 unk0[0x19F0];
    u64 flags; /* 0x100: pause */
} EftBattleWork;
extern EftBattleWork *Battle_GetWork(void);

extern EftEVec D_002C3630; /* (0, 0, 0, 1) */

extern EftTransPart *EftBurst_AllocPtcl(void);
extern void EftBurst_InitSpark(EftTransPart *part, s32 idx);
extern void EftBurst_InitStreak(EftTransPart *part);
extern void EftBurst_InitFlash(EftTransPart *part);
extern void EftBurst_InitRing(EftTransPart *part);
extern void EftBurst_InitGlow(EftTransPart *part);
extern void EftBurst_InitDebris(EftTransPart *part);
extern s32 Snd_PlaySeEx(u32 mask, s32 id, s32 volume, s32 pan, s32 pitch);

extern f32 EftStage_GetTintScale(void);
extern void EftMath_MtxFromDir(Mtx44 *out, Vec4 *dir, f32 angle);
extern void Mtx_MulVec4(Vec4 *out, Mtx44 *mtx, Vec4 *v);

/* A projected point as Vu0Cur_ProjectPoints writes it: GS x, y in 12.4 fixed point, z, and a fourth word. */
typedef struct EftSteamScr {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;
    /* 0x0C */ s32 unkC;
} EftSteamScr; /* size 0x10 */
extern void Vu0Cur_ProjectPoints(EftSteamScr *out, Vec4 *in, s32 count);

/* GS XYZF2 register value. */
typedef struct EftSteamXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftSteamXyzf;

/* The packet of one particle quad: REGLIST of PRIM, TEX0_1, RGBAQ and four (ST, XYZF2). */
typedef struct EftSteamPkt {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ u8 rgba[4];
    /* 0x34 */ f32 q;
    /* 0x38 */ struct {
        /* 0x00 */ f32 s;
        /* 0x04 */ f32 t;
        /* 0x08 */ EftSteamXyzf xyz;
    } v[4];
    /* 0x78 */ u64 pad;
} EftSteamPkt; /* size 0x80 */


typedef struct EftCommonRes {
    u8 unk0[0x34];
    s32 *transition;
} EftCommonRes;
extern EftCommonRes *gCommonRes;

/* Creates an emitter from a 0x60-byte description; NULL when the stage has no emitter manager. */
void *EftSteam_Create(EftSteamArg *arg) {
    if (gEftSteam == NULL) {
        return NULL;
    }
    return BtlTaskList_AddTail(gEftSteam->list, &gEftSteamClass, arg);
}

/* Lets an emitter die: no new particles, and the task ends when the last one is gone. */
void EftSteam_Stop(EftSteamTask *task) {
    if (task != NULL && task->cls->update == (void (*)(void *))EftSteam_Update && !EFT_TASK_IS_DEAD(task)) {
        ((EftSteamWork *)task->work)->flags |= EFT_STEAM_STOP;
    }
}

/* Pauses or resumes the creation of particles. */
void EftSteam_SetPaused(EftSteamTask *task, s32 paused) {
    if (task != NULL && task->cls->update == (void (*)(void *))EftSteam_Update && !EFT_TASK_IS_DEAD(task)) {
        EftSteamWork *work = task->work;

        if (paused) {
            work->flags |= EFT_STEAM_PAUSED;
        } else {
            work->flags &= ~EFT_STEAM_PAUSED;
        }
    }
}

/* The work of an emitter task, NULL when the task is not a live emitter. */
EftSteamWork *EftSteam_GetWork(EftSteamTask *task) {
    if (task == NULL) {
        return NULL;
    }
    if (!(task->cls->update == (void (*)(void *))EftSteam_Update)) {
        return NULL;
    }
    if (EFT_TASK_IS_DEAD(task)) {
        return NULL;
    }
    return task->work;
}

/* Moves an emitter. */
void EftSteam_SetPos(EftSteamTask *task, Vec4 *pos) {
    if (task != NULL && task->cls->update == (void (*)(void *))EftSteam_Update && !EFT_TASK_IS_DEAD(task)) {
        Vec4_Copy(task->work, pos);
    }
}

/* Emitter manager init (layer 0, sub-task 5): the list, the texture, and one emitter per stage marker. */
void EftSteamMgr_Init(void *task) {
    EftSteamArg arg;
    s32 *stage;
    s32 count;
    EftStageMarker *base;
    EftStageMarker *marker;
    s32 i;

    stage = (s32 *)BtlScene_GetStageData();
    count = BtlStage_GetFxResC();
    gEftSteam = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftSteamShared));
    memset(gEftSteam, 0, sizeof(EftSteamShared));
    gEftSteam->list = BtlTask_CreateChildList(task, count + BtlStage_GetFxResA(), sizeof(EftSteamWork));
    EftTexSet_Load8(&gEftSteam->tex, BtlScene_GetPackEntry(stage, 0x12));
    base = BtlStage_GetFxResC2();
    if (base != NULL) {
        for (i = 0; i < count; i++) {
            marker = &base[i];
            memset(&arg, 0, sizeof(EftSteamArg));
            arg.pos.x = marker->pos[0];
            arg.pos.y = marker->pos[1];
            arg.pos.z = marker->pos[2];
            arg.pos.w = 1.0f;
            arg.dir.x = 0.0f;
            arg.dir.y = -1.0f;
            arg.dir.z = 0.0f;
            arg.dir.w = 1.0f;
            arg.color.x = marker->color[0];
            arg.color.y = marker->color[1];
            arg.color.z = marker->color[2];
            arg.color.w = marker->color[3];
            arg.speed = marker->speed;
            arg.accel = marker->accel;
            arg.size = marker->size;
            arg.life = marker->life;
            arg.posRange = 1.0f;
            arg.dirRange = 0.3f;
            arg.speedRange = 1.0f;
            arg.sizeRange = 0.5f;
            arg.lifeRange = 5;
            EftSteam_Create(&arg);
        }
    }
}

/* Emitter manager term. */
void EftSteamMgr_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftSteam);
    gEftSteam = NULL;
}

/* Emitter manager update: renews the TEX0 of the texture while any emitter exists. */
void EftSteamMgr_Update(void) {
    if (((void **)gEftSteam->list)[1] != NULL) {
        gEftSteam->tex.tex0 = EftVram_AddTex(&gEftSteam->tex.tex0, 1, 0);
    }
}

/* Emitter init: copies the description into the work. */
void EftSteam_Init(EftSteamTask *task, EftSteamArg *arg) {
    EftSteamWork *work = task->work;

    memset(work, 0, sizeof(EftSteamWork));
    work->arg = *arg;
    work->tex = &gEftSteam->tex;
}

/* Emitter term: nothing. */
void EftSteam_Term(void) {
}

/* Emitter reset: nothing. */
void EftSteam_Reset(void) {
}

/* Emitter update: one new particle per frame after the first, then moves them; ends when stopped and empty. */
void EftSteam_Update(EftSteamTask *task) {
    EftSteamWork *work = task->work;

    if (!(work->flags & (EFT_STEAM_PAUSED | EFT_STEAM_STOP))) {
        if (work->flags & EFT_STEAM_STARTED) {
            EftSteam_Emit(work);
        }
    }
    if ((work->flags & EFT_STEAM_STOP) && work->count <= 0) {
        BtlTask_SetDead(task);
        return;
    }
    EftSteam_MoveParts(work);
    work->flags |= EFT_STEAM_STARTED;
}

/* Emitter draw. */
void EftSteam_Draw(EftSteamTask *task) {
    EftSteamWork *work = task->work;

    if (!(gBtlStage->flags & 1)) {
        if (EftStage_IsDrawOn()) {
            Vu0Cur_Push();
            Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
            EftSteam_DrawParts(work, work->tex->tex0);
            Vu0Cur_Pop();
        }
    }
}

/* First free particle of an emitter. */
EftSteamPart *EftSteam_FindFreePart(EftSteamPart *part) {
    s32 i;

    for (i = 0; i < EFT_STEAM_PART_COUNT; i++, part++) {
        if (part->life == 0) {
            return part;
        }
    }
    return NULL;
}

/* Creates one particle: start, direction, speed, size and life are the emitter values plus a random spread. */
void EftSteam_Emit(EftSteamWork *work) {
    Mtx44 cam;
    Vec4 toCam;
    Vec4 side;
    EftSteamPart *part = EftSteam_FindFreePart(work->part);
    EftEVec up = {0.0f, -1.0f, 0.0f, 1.0f};

    if (part != NULL) {
        Mtx_InverseRT(&cam, &gBtlCamView->world2view2);
        Vec4_Sub(&toCam, (Vec4 *)cam.m[3], (Vec4 *)&work->arg.pos);
        Vec3_Normalize(&toCam, &toCam);
        Vec3_Cross(&side, (Vec4 *)&up, &toCam);
        Vec4_Copy((Vec4 *)&part->pos, (Vec4 *)&work->arg.pos);
        part->pos.x += side.x * Rand_FloatRange(-work->arg.posRange, work->arg.posRange);
        part->pos.z += side.z * Rand_FloatRange(-work->arg.posRange, work->arg.posRange);
        part->pos.w = 1.0f;
        toCam.x *= Rand_FloatRange(-work->arg.dirRange, work->arg.dirRange);
        toCam.z *= Rand_FloatRange(-work->arg.dirRange, work->arg.dirRange);
        Vec4_Set((Vec4 *)&part->dir, toCam.x, -1.0f, toCam.z, 1.0f);
        Vec3_Normalize((Vec4 *)&part->dir, (Vec4 *)&part->dir);
        Vec4_Copy((Vec4 *)&part->color, (Vec4 *)&work->arg.color);
        part->accel = work->arg.accel;
        part->speed = work->arg.speed + Rand_FloatRange(-work->arg.speedRange, work->arg.speedRange);
        part->size = work->arg.size + Rand_FloatRange(-work->arg.sizeRange, work->arg.sizeRange);
        part->life = work->arg.life + (s32)Rand_FloatRange(-work->arg.lifeRange, work->arg.lifeRange);
        part->angle = Rand_FloatRange(0.0f, 6.2831853f);
        Vec3_Scale((Vec4 *)&part->vel, (Vec4 *)&part->dir, part->speed);
        work->count++;
    }
}

/* Moves the particles of an emitter (not while time is stopped) and frees the ones whose life ran out. */
void EftSteam_MoveParts(EftSteamWork *work) {
    s32 i;
    EftSteamPart *part = work->part;

    if (!BtlScene_IsTimeStopped()) {
        for (i = 0; i < EFT_STEAM_PART_COUNT; i++, part++) {
            if (part->life != 0) {
                Vec3_Add((Vec4 *)&part->pos, (Vec4 *)&part->pos, (Vec4 *)&part->vel);
                part->vel.y += part->accel;
                part->life--;
                if (part->life < 0) {
                    part->life = 0;
                    work->count--;
                }
            }
        }
    }
}

/* Draws the particles of an emitter. */
void EftSteam_DrawParts(EftSteamWork *work, u64 tex0) {
    s32 i;
    EftSteamPart *part = work->part;

    for (i = 0; i < EFT_STEAM_PART_COUNT; i++, part++) {
        if (part->life != 0) {
            EftSteam_DrawQuad(&part->color, &part->pos, 32.0f, 32.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1, tex0, part->size,
                             part->angle);
        }
    }
}

/* Links a packet into the chain of a depth slot (clamped to 0..0xFFF); layers 2 and 3 fold onto 0 and 1. */
static inline void EftSteam_OtAdd(OtPrim *p, s32 z, s32 layer) {
    OtEntry *e;

    if (layer >= 2) {
        layer -= 2;
    }
    if (z < 0) {
        e = &gOtZ[0].layer[layer];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[layer];
    } else {
        e = &gOtZ[z].layer[layer];
    }
    e->tail->next = p;
    e->tail = p;
}

/* 1 when a projected point is behind the camera or outside the GS coordinate range. */
static inline s32 EftSteam_IsClipped(s32 x, s32 y, s32 z) {
    s32 r = 1;

    if (z > 0 && x <= 0xFFEF && x > 0) {
        if (y <= 0xFFEF) {
            r = y < 1;
        }
    }
    return r;
}

/* Queues one camera-facing textured quad (a 4-vertex strip) in the ordering table: w x h scaled by size / 16
   around pos, rolled by angle, colour scaled by the stage tint. Dropped when any corner is off the GS range. */
void EftSteam_DrawQuad(EftEVec *color, EftEVec *pos, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, s32 prio, u64 tex0,
                      f32 size, f32 angle) {
    EftSteamScr scr[4];
    Vec4 corner[4];
    Vec4 toCam;
    Mtx44 mtx;
    f32 tint;
    f32 hh;
    f32 hw;
    s32 i;
    EftSteamPkt *p;
    u64 prim;

    tint = EftStage_GetTintScale();
    if (angle > 3.14159265f) {
        angle -= 6.2831853f;
    } else if (angle < -3.14159265f) {
        angle += 6.2831853f;
    }
    hw = w * size;
    hh = h * size;
    hh *= 0.0625f;
    hw *= 0.0625f;
    Vec4_Sub(&toCam, &gBtlCamView->pos, (Vec4 *)pos);
    Vec3_Normalize(&toCam, &toCam);
    EftMath_MtxFromDir(&mtx, &toCam, angle);
    Vec4_Set(&corner[0], -hw, -hh, 0.0f, 1.0f);
    Vec4_Set(&corner[1], hw, -hh, 0.0f, 1.0f);
    Vec4_Set(&corner[2], -hw, hh, 0.0f, 1.0f);
    Vec4_Set(&corner[3], hw, hh, 0.0f, 1.0f);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&corner[i], &mtx, &corner[i]);
        Vec3_Add(&corner[i], &corner[i], (Vec4 *)pos);
        corner[i].w = 1.0f;
    }
    Vu0Cur_ProjectPoints(scr, corner, 4);
    if (EftSteam_IsClipped(scr[0].x, scr[0].y, scr[0].z)) {
        return;
    }
    if (EftSteam_IsClipped(scr[1].x, scr[1].y, scr[1].z)) {
        return;
    }
    if (EftSteam_IsClipped(scr[2].x, scr[2].y, scr[2].z)) {
        return;
    }
    if (EftSteam_IsClipped(scr[3].x, scr[3].y, scr[3].z)) {
        return;
    }
    prim = 0x54;
    p = (EftSteamPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    p->tag = 0x20000007;
    p->vif1 = 0x50000007;
    p->tex0 = tex0;
    p->prim = prim;
    p->vif0 = 0x10000000;
    p->gif0 = 0xC400000000008001;
    p->gif1 = 0xF42424242160;
    p->next = 0;
    p->rgba[0] = (u32)(color->x * tint);
    p->rgba[1] = (u32)(color->y * tint);
    p->rgba[2] = (u32)(color->z * tint);
    p->rgba[3] = (u32)color->w;
    p->q = 1.0f;
    p->v[0].s = u0;
    p->v[0].t = v0;
    p->v[1].s = u1;
    p->v[1].t = v0;
    p->v[2].s = u0;
    p->v[2].t = v1;
    p->v[3].s = u1;
    p->v[3].t = v1;
    p->v[0].xyz.x = scr[0].x;
    p->v[0].xyz.y = scr[0].y;
    p->v[0].xyz.z = scr[0].z;
    p->v[0].xyz.f = 0xFF;
    p->v[1].xyz.x = scr[1].x;
    p->v[1].xyz.y = scr[1].y;
    p->v[1].xyz.z = scr[1].z;
    p->v[1].xyz.f = 0xFF;
    p->v[2].xyz.x = scr[2].x;
    p->v[2].xyz.y = scr[2].y;
    p->v[2].xyz.z = scr[2].z;
    p->v[2].xyz.f = 0xFF;
    p->v[3].xyz.x = scr[3].x;
    p->v[3].xyz.y = scr[3].y;
    p->v[3].xyz.z = scr[3].z;
    p->v[3].xyz.f = 0xFF;
    EftSteam_OtAdd((OtPrim *)p, scr[0].z >> 8, prio);
}

/* The stage's water level; always reports success. */
s32 EftWater_GetSurfaceY(f32 *y) {
    BtlStage_GetWaterLevel(y);
    return 1;
}

/* Starts (off == 0) or stops (off == 1) the wake behind a fighter. Does nothing in split-screen. */
s32 EftWater_SetWake(s32 objId, u8 off) {
    EftWaterWake *wake;

    if (gEftDust == NULL) {
        return 0;
    }
    if (Battle_IsSplitScreen()) {
        return 0;
    }
    wake = &gEftDust->wake[objId];
    switch (off) {
    case 0:
        if (wake->on != 0) {
            break;
        }
        wake->objId = objId;
        wake->on = 1;
        goto reset;
    case 1:
        if (wake->on == 0) {
            break;
        }
        wake->on = 0;
    reset:
        wake->frame = 0;
        break;
    }
    return 1;
}

/* Spray along a technique's hit record that travels just above the water. Does nothing in split-screen. */
s32 EftWater_AddBlastTrail(s32 objId, EftWaterBlast *rec) {
    if (gEftDust == NULL) {
        return 0;
    }
    if (Battle_IsSplitScreen()) {
        return 0;
    }
    EftWaterTrail_Spawn(&gEftDust->trailList[0], &gEftDust->trailList[1], objId, rec);
    return 1;
}

/* A splash at a point of the surface, sized by sqrt(speed) * 0.33. kind EFT_WATER_SPLASH_CHAR: a fighter entering
   or leaving the water (also scaled by height * 0.05), _BLAST: a ki blast (* 0.8, fewer particles), _TECH: a
   technique (height * 0.05 * 1.2). Returns 0 and does nothing in split-screen or without the water task.
   The square root is assigned to the parameter itself (`speed`, which is why it lives in $f12, the register the
   parameter arrived in) and the scaled value to a second variable. */
s32 EftWater_AddSplashFor(s32 objId, EftEVec pos, u8 kind, f32 speed) {
    Vec4 at;
    f32 level;
    f32 raw = speed;
    f32 scale;

    if (gEftDust == NULL) {
        return 0;
    }
    if (Battle_IsSplitScreen()) {
        return 0;
    }
    Vec4_Copy(&at, (Vec4 *)&pos);
    if (EftWater_GetSurfaceY(&level)) {
        at.y = level + -1.0f;
    }
    speed = sqrtf(raw);
    scale = speed * 0.33f;
    switch (kind) {
    case EFT_WATER_SPLASH_CHAR:
        EftWater_AddSplash(&gEftDust->splashList[0], &gEftDust->splashList[1], *(EftEVec *)&at, 7, 4, 3, 2,
                           BtlCharApi_GetHeight(objId) * 0.05f * scale);
        break;
    case EFT_WATER_SPLASH_BLAST:
        EftWater_AddSplash(&gEftDust->splashList[0], &gEftDust->splashList[1], *(EftEVec *)&at, 4, 3, 2, 2, scale * 0.8f);
        break;
    case EFT_WATER_SPLASH_TECH:
        EftWater_AddSplash(&gEftDust->splashList[0], &gEftDust->splashList[1], *(EftEVec *)&at, 7, 4, 3, 2,
                           BtlCharApi_GetHeight(objId) * 0.05f * scale * 1.2f);
        break;
    }
    return 1;
}

/* A full-size splash at a point (called by the stage code, 0x241DB4). Does nothing in split-screen. */
s32 EftWater_AddSplashAt(EftEVec pos, f32 scale) {
    Vec4 at;
    f32 level;

    if (gEftDust == NULL) {
        return 0;
    }
    if (Battle_IsSplitScreen()) {
        return 0;
    }
    Vec4_Copy(&at, (Vec4 *)&pos);
    if (EftWater_GetSurfaceY(&level)) {
        at.y = level + -1.0f;
    }
    EftWater_AddSplash(&gEftDust->splashList[0], &gEftDust->splashList[1], *(EftEVec *)&at, 7, 4, 3, 2, scale);
    return 1;
}

/* A burst around a fighter standing in the water: count swirls spread evenly around a random angle, a ring and
   two pairs of billboards, all sized by the fighter's height and delayed by delay frames. Does nothing in
   split-screen or when the fighter's ground point is above the water. No caller. */
s32 EftWater_AddBurst(s32 objId, s32 count, f32 delay) {
    EftEVec pos;
    f32 level = 0.0f;
    f32 y;
    f32 height;
    f32 yaw;
    f32 step;
    f32 radius;
    f32 speed;
    f32 gravity;
    f32 size;
    f32 sizeVel;
    f32 life;
    f32 pitch;
    f32 yawVel;
    f32 dist;
    s32 i;

    if (gEftDust == NULL) {
        return 0;
    }
    if (Battle_IsSplitScreen()) {
        return 0;
    }
    EftWater_GetSurfaceY(&level);
    if (BtlCharApi_GetGroundY(objId) < level) {
        return 0;
    }
    BtlCharApi_GetPos(objId, (Vec4 *)&pos);
    if (EftWater_GetSurfaceY(&y)) {
        pos.y = y + -1.0f;
    }
    height = 7.0f * EFT_UNIT(objId);
    step = 360.0f / count;
    yaw = EFT_RANDF() * 360.0f;
    for (i = 0; i < count; i++) {
        radius = 10.0f;
        speed = 25.0f;
        gravity = 2.4f;
        size = EFT_RANDF() * 1.5f + 1.0f;
        sizeVel = EFT_RANDF() * 1.2f + 2.8f;
        life = EFT_RANDF() * 0.25f + 0.4f;
        pitch = EFT_RANDF() * 10.0f + 50.0f;
        yawVel = EFT_RANDF() * 5.0f + 14.0f;
        radius *= EFT_UNIT(objId);
        speed *= EFT_UNIT(objId);
        sizeVel *= EFT_UNIT(objId);
        gravity *= EFT_UNIT(objId);
        EftWaterMist_Spawn(&gEftDust->mistList[0], &gEftDust->mistList[1], pos, radius, height, yaw + step * i, yawVel, -7.0f,
                      pitch, EFT_RANDF() * 360.0f, 0.9f, 0.75f, speed, size, sizeVel, -5.0f, gravity, 1.0f, 1.5f, life,
                      delay, 0x8C, 0x8C, 0x8C, 3, 0);
    }
    step = 180.0f;
    dist = 0.0f;
    size = EFT_UNIT(objId) * dist;
    EftWaterRing_Spawn(&gEftDust->ringList[0], &gEftDust->ringList[1], pos, dist, size,
                  EFT_UNIT(objId) * 8.0f, -24.0f, 112.0f, 1.2f, delay, 0xFF, 0xFF, 0xFF, 2, 0, 0xD);
    yaw = EFT_RANDF() * 360.0f;
    for (i = 0; i < 2; i++) {
        dist = 0.0f;
        gravity = 4.8f;
        speed = EFT_RANDF() * 10.0f + 35.0f;
        size = EFT_RANDF() * 2.0f + 1.0f;
        sizeVel = EFT_RANDF() * 2.5f + 4.5f;
        life = EFT_RANDF() * 0.3f + 0.6f;
        pitch = EFT_RANDF() * 10.0f + 30.0f;
        dist *= EFT_UNIT(objId);
        speed *= EFT_UNIT(objId);
        sizeVel *= EFT_UNIT(objId);
        gravity *= EFT_UNIT(objId);
        EftWaterDrop_Spawn(&gEftDust->dropList[0], &gEftDust->dropList[1], pos, yaw + step * i, pitch, speed,
                      EFT_RANDF() * 360.0f, 0.0f, size, sizeVel, -5.0f, gravity, life, delay, 0x80, 0x80, 0x80, 1, 0, 9);
    }
    step = 180.0f;
    yaw = EFT_RANDF() * 360.0f;
    for (i = 0; i < 2; i++) {
        dist = 0.0f;
        gravity = 4.8f;
        speed = EFT_RANDF() * 10.0f + 30.0f;
        size = EFT_RANDF() * 2.0f + 2.0f;
        sizeVel = EFT_RANDF() * 3.0f + 5.0f;
        life = EFT_RANDF() * 0.3f + 0.7f;
        pitch = EFT_RANDF() * 10.0f + 40.0f;
        dist *= EFT_UNIT(objId);
        speed *= EFT_UNIT(objId);
        sizeVel *= EFT_UNIT(objId);
        gravity *= EFT_UNIT(objId);
        EftWaterDrop_Spawn(&gEftDust->dropList[0], &gEftDust->dropList[1], pos, yaw + step * i, pitch, speed,
                      EFT_RANDF() * 360.0f, 0.0f, size, sizeVel, -5.0f, gravity, life, delay, 0x8C, 0x8C, 0x8C, 4, 0,
                      0x105);
    }
    return 1;
}

/* Called by BtlScene_UpdateRecords for every hit record of the frame: the splash where a record crosses the
   water surface, and the spray trail of a technique that skims it. Writes EFT task flag 0x400 only. */
void EftWater_UpdateBlast(EftWaterBlast *rec) {
    Vec4 pos;
    Vec4 prev;
    Vec4 dir;
    EftEVec hit;
    f32 level = 0.0f;
    f32 dy = 1.0f;
    f32 scale;
    f32 size;
    u8 *wait;

    if (Battle_GetWork()->flags & 0x100) {
        return;
    }
    if (gEftDust == NULL) {
        return;
    }
    if (rec->flags & 8) {
        return;
    }
    if (rec->task->flags & EFT_WATER_TASK_HIT_4) {
        return;
    }
    if (rec->task->flags & EFT_WATER_TASK_DONE) {
        return;
    }
    EftWater_GetSurfaceY(&level);
    if (BtlStage_GetId() == 12) {
        level += 5.0f;
    }
    Vec4_Copy(&pos, (Vec4 *)&rec->pos);
    Vec4_Copy(&prev, (Vec4 *)&rec->prevPos);
    if (rec->task->flags & EFT_WATER_TASK_HIT_CHAR) {
        Vec4_Sub(&dir, &prev, &pos);
        Vec3_Normalize(&dir, &dir);
        dir.x *= rec->radius;
        dir.y *= rec->radius;
        dir.z *= rec->radius;
        dir.w = dy;
        Vec4_Add(&prev, (Vec4 *)&rec->task->pos, &dir);
        Vec4_Copy(&pos, (Vec4 *)&rec->task->pos);
        rec->task->flags |= EFT_WATER_TASK_DONE;
    }
    dy = __builtin_fabsf(pos.y - prev.y);
    if (dy > 20.0f) {
        dy = 20.0f;
    }
    scale = EftHit_GetRadiusA(rec) / 5.0f;
    size = (dy * 0.8f + (rand() / 2147483647.0f * 3.0f - 1.5f)) * scale;
    if (pos.y >= level && prev.y <= level) {
        EftUtil_ClipSegToWater(&hit, &pos, &prev);
        if (rec->type == 0) {
            EftWater_AddSplashFor(rec->objId, hit, EFT_WATER_SPLASH_BLAST, size);
            BtlCharApi_PlaySoundAt((Vec4 *)&hit, 0, 0x3C, 200.0f, 1500.0f);
        } else if (rec->src != NULL) {
            if ((u32)(rec->src->def[5] - 3) < 2) {
                if (rec->objId == 0) {
                    wait = &gEftDust->blastWait[0];
                } else {
                    wait = &gEftDust->blastWait[1];
                }
                if (*wait != 0) {
                    (*wait)--;
                } else {
                    EftWater_AddSplashFor(rec->objId, hit, EFT_WATER_SPLASH_TECH, dy);
                    *wait = EftRec_GetDefClass(rec);
                }
            } else {
                EftWater_AddSplashFor(rec->objId, hit, EFT_WATER_SPLASH_TECH, dy);
            }
        }
    } else if (pos.y <= level && level <= prev.y) {
        EftUtil_ClipSegToWater(&hit, &pos, &prev);
        if (rec->type == 0) {
            EftWater_AddSplashFor(rec->objId, hit, EFT_WATER_SPLASH_BLAST, size);
            BtlCharApi_PlaySoundAt((Vec4 *)&hit, 0, 0x3D, 200.0f, 1500.0f);
        } else if (rec->src != NULL) {
            if ((u32)(rec->src->def[5] - 3) < 2) {
                if (rec->objId == 0) {
                    wait = &gEftDust->blastWait[0];
                } else {
                    wait = &gEftDust->blastWait[1];
                }
                if (*wait != 0) {
                    (*wait)--;
                } else {
                    EftWater_AddSplashFor(rec->objId, hit, EFT_WATER_SPLASH_TECH, dy);
                    *wait = EftRec_GetDefClass(rec);
                }
            } else {
                EftWater_AddSplashFor(rec->objId, hit, EFT_WATER_SPLASH_TECH, dy);
            }
        }
    } else if (rec->type != 0) {
        if (BtlStage_GetId() == 10) {
            if (pos.y > level && pos.y < level + 12.0f) {
                EftWater_AddBlastTrail(rec->objId, rec);
            }
        } else {
            if (level - 6.0f < pos.y && pos.y < level + 12.0f) {
                EftWater_AddBlastTrail(rec->objId, rec);
            }
        }
    }
}

/* Water task init (layer 0, sub-task 7): allocates the state and the particle pools, loads the textures. The task
   ends at once on a stage whose pack has no entry 0x12. */
void EftWater_Init(void *task) {
    s32 *stage;

    if (gEftDust == NULL) {
        gEftDust = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftWater));
        memset(gEftDust, 0, sizeof(EftWater));
    }
    if (gEftDust->wake == NULL) {
        gEftDust->wake = BtlPool_Alloc(BtlPool_GetCurrent(), 0x18);
        memset(gEftDust->wake, 0, 0x18);
    }
    if (gEftDust->splash == NULL) {
        gEftDust->splash = BtlPool_Alloc(BtlPool_GetCurrent(), 0x280);
        memset(gEftDust->splash, 0, 0x280);
    }
    if (gEftDust->trailPool == NULL) {
        gEftDust->trailPool = BtlPool_Alloc(BtlPool_GetCurrent(), 0x21C);
        memset(gEftDust->trailPool, 0, 0x21C);
    }
    if (gEftDust->dropPool == NULL) {
        gEftDust->dropPool = BtlPool_Alloc(BtlPool_GetCurrent(), 0x1A40);
        memset(gEftDust->dropPool, 0, 0x1A40);
    }
    if (gEftDust->ringPool == NULL) {
        gEftDust->ringPool = BtlPool_Alloc(BtlPool_GetCurrent(), 0x960);
        memset(gEftDust->ringPool, 0, 0x960);
    }
    if (gEftDust->sprayPool == NULL) {
        gEftDust->sprayPool = BtlPool_Alloc(BtlPool_GetCurrent(), 0x10E0);
        memset(gEftDust->sprayPool, 0, 0x10E0);
    }
    if (gEftDust->mistPool == NULL) {
        gEftDust->mistPool = BtlPool_Alloc(BtlPool_GetCurrent(), 0x14A0);
        memset(gEftDust->mistPool, 0, 0x14A0);
    }
    stage = (s32 *)BtlScene_GetStageData();
    if (BtlScene_GetPackEntrySize(stage, 0x12) > 0) {
        EftTexSet_Load32(gEftDust->tex, BtlScene_GetPackEntry(stage, 0x12));
    } else {
        BtlTask_SetDead(task);
    }
}

/* Water task term: frees the pools and the state. */
void EftWater_Term(void) {
    if (gEftDust->mistPool != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftDust->mistPool);
        gEftDust->mistPool = NULL;
    }
    if (gEftDust->sprayPool != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftDust->sprayPool);
        gEftDust->sprayPool = NULL;
    }
    if (gEftDust->ringPool != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftDust->ringPool);
        gEftDust->ringPool = NULL;
    }
    if (gEftDust->dropPool != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftDust->dropPool);
        gEftDust->dropPool = NULL;
    }
    if (gEftDust->splash != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftDust->splash);
        gEftDust->splash = NULL;
    }
    if (gEftDust->trailPool != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftDust->trailPool);
        gEftDust->trailPool = NULL;
    }
    if (gEftDust->wake != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftDust->wake);
        gEftDust->wake = NULL;
    }
    if (gEftDust != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftDust);
        gEftDust = NULL;
    }
}

/* Water task update: the wakes, then every particle list. */
void EftWater_Update(void) {
    if (!BtlCharApi_IsInTechnique(0)) {
        gEftDust->blastWait[0] = 0;
    }
    if (!BtlCharApi_IsInTechnique(1)) {
        gEftDust->blastWait[1] = 0;
    }
    EftWater_UpdateWakes();
    EftWaterSplash_UpdateList(&gEftDust->splashList[0], &gEftDust->splashList[1]);
    EftWaterTrail_UpdateList(&gEftDust->trailList[0], &gEftDust->trailList[1]);
    EftWaterDrop_UpdateList(&gEftDust->dropList[0], &gEftDust->dropList[1]);
    EftWaterRing_UpdateList(&gEftDust->ringList[0], &gEftDust->ringList[1]);
    EftWaterSpray_UpdateList(&gEftDust->sprayList[0], &gEftDust->sprayList[1]);
    EftWaterMist_UpdateList(&gEftDust->mistList[0], &gEftDust->mistList[1]);
}

/* Water task post-update: EftWater_UpdateTextures(1, 0). */
void EftWater_PostUpdate(void) {
    EftWater_UpdateTextures(1, 0);
}

/* Water task reset: stops both wakes. */
void EftWater_Reset(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        EftWater_SetWake(i, 1);
    }
}

/* Water task draw. */
void EftWater_Draw(void) {
    if (!(gBtlStage->flags & 1)) {
        if (EftStage_IsDrawOn()) {
            if (BtlStage_IsReady()) {
                Vu0Cur_Push();
                Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
                Mtx_InverseRT(&gEftDust->camMtx, &gBtlCamView->world2view2);
                gEftDust->camMtx.m[3][0] = gEftDust->camMtx.m[3][1] = gEftDust->camMtx.m[3][2] = 0.0f;
                EftWaterSplash_DrawList(gEftDust->splashList[0]);
                EftWaterTrail_DrawList(gEftDust->trailList[0]);
                EftWaterRing_DrawList(gEftDust->ringList[0]);
                EftWaterSpray_DrawList(gEftDust->sprayList[0], D_002C3630);
                EftWaterDrop_DrawList(gEftDust->dropList[0], D_002C3630);
                EftWaterMist_DrawList(gEftDust->mistList[0]);
                Vu0Cur_Pop();
            }
        }
    }
}

/* The wake behind each fighter whose wake is on: every frame a streak (two on even frames), on even frames two
   billboards, every third frame a ring. Sizes follow the fighter's speed and height. Skipped while paused. */
void EftWater_UpdateWakes(void) {
    EftEVec pos;
    Vec4 dir;
    Vec4 delta;
    f32 y;
    f32 yaw;
    f32 height;
    f32 scale;
    f32 size;
    f32 sizeVel;
    f32 life;
    f32 pitch;
    f32 speed;
    f32 gravity;
    f32 puffYaw;
    f32 dist;
    EftWaterWake *wake;
    s32 n;
    s32 i;

    if (!EFT_PAUSED()) {
        wake = gEftDust->wake;
        for (n = 0; n < 2; n++, wake++) {
            if (wake->on != 0) {
                BtlCharApi_GetPos(wake->objId, (Vec4 *)&pos);
                if (EftWater_GetSurfaceY(&y)) {
                    pos.y = y + -1.0f;
                }
                BtlCharApi_GetDir(wake->objId, &dir);
                Vec3_Scale(&delta, &dir, BtlCharApi_GetSpeed(wake->objId));
                delta.y = 0.0f;
                scale = sqrtf(Vec3_Length(&delta)) * 0.26f;
                if (scale < 0.9f) {
                    scale *= 0.65f;
                }
                yaw = atan2f(dir.x, dir.z) * 180.0f / 3.14159265f;
                height = 7.0f * EFT_SCALE(wake->objId, scale);
                if (!(wake->frame & 1)) {
                    for (i = 0; i < 2; i++) {
                        dist = 0.0f;
                        speed = 40.0f;
                        gravity = 6.4f;
                        size = EFT_RANDF() * 3.0f + 1.0f;
                        sizeVel = EFT_RANDF() * 3.0f + 5.0f;
                        life = EFT_RANDF() * 0.2f + 0.3f;
                        pitch = EFT_RANDF() * 10.0f + 50.0f;
                        speed *= EFT_SCALE(wake->objId, scale);
                        sizeVel *= EFT_SCALE(wake->objId, scale);
                        gravity *= EFT_SCALE(wake->objId, scale);
                        EftWaterSpray_Spawn(&gEftDust->sprayList[0], &gEftDust->sprayList[1], pos, dist, height,
                                      yaw - 60.0f + i * 120.0f, pitch, EFT_RANDF() * 360.0f, 0.8f, 0.6f, speed, size,
                                      sizeVel, -5.0f, gravity, 1.0f, 1.5f, life, 0.0f, 0x8C, 0x8C, 0x8C, 3, 0, 0x102);
                    }
                } else {
                    dist = 0.0f;
                    speed = 40.0f;
                    gravity = 6.4f;
                    size = EFT_RANDF() * 3.0f + 1.0f;
                    sizeVel = EFT_RANDF() * 3.0f + 5.0f;
                    life = EFT_RANDF() * 0.2f + 0.3f;
                    pitch = EFT_RANDF() * 10.0f + 50.0f;
                    speed *= EFT_SCALE(wake->objId, scale);
                    sizeVel *= EFT_SCALE(wake->objId, scale);
                    gravity *= EFT_SCALE(wake->objId, scale);
                    EftWaterSpray_Spawn(&gEftDust->sprayList[0], &gEftDust->sprayList[1], pos, dist, height,
                                  yaw - 60.0f + (rand() % 2) * 120.0f, pitch, EFT_RANDF() * 360.0f, 0.8f, 0.5f, speed,
                                  size, sizeVel, -5.0f, gravity, 1.0f, 1.5f, life, 0.0f, 0x8C, 0x8C, 0x8C, 3, 0, 0x102);
                }
                if (!(wake->frame & 1)) {
                    speed = 90.0f;
                    size = 2.0f;
                    gravity = 9.0f;
                    sizeVel = EFT_RANDF() * 3.5f + 6.5f;
                    life = EFT_RANDF() * 0.2f + 0.4f;
                    pitch = EFT_RANDF() * 10.0f + 60.0f;
                    speed *= EFT_SCALE(wake->objId, scale);
                    size *= EFT_SCALE(wake->objId, scale);
                    sizeVel *= EFT_SCALE(wake->objId, scale);
                    gravity *= EFT_SCALE(wake->objId, scale);
                    puffYaw = yaw - 30.0f + (rand() % 2) * 60.0f;
                    EftWaterDrop_Spawn(&gEftDust->dropList[0], &gEftDust->dropList[1], pos, puffYaw, pitch, speed,
                                  EFT_RANDF() * 360.0f, 0.0f, size, sizeVel, -5.0f, gravity, life, 0.0f, 0x80, 0x80,
                                  0x80, 4, 0, 0x106);
                    speed = 55.0f;
                    size = 2.0f;
                    gravity = 3.5f;
                    sizeVel = EFT_RANDF() * 3.5f + 6.5f;
                    life = EFT_RANDF() * 0.3f + 0.6f;
                    pitch = EFT_RANDF() * 10.0f + 70.0f;
                    speed *= EFT_SCALE(wake->objId, scale);
                    size *= EFT_SCALE(wake->objId, scale);
                    sizeVel *= EFT_SCALE(wake->objId, scale);
                    gravity *= EFT_SCALE(wake->objId, scale);
                    EftWaterDrop_Spawn(&gEftDust->dropList[0], &gEftDust->dropList[1], pos, yaw, pitch, speed,
                                  EFT_RANDF() * 360.0f, 0.0f, size, sizeVel, -5.0f, gravity, life, 0.0f, 0x80, 0x80,
                                  0x80, 1, 0, 0xA);
                }
                if (wake->frame % 3 == 0) {
                    size = EFT_SCALE(wake->objId, scale) * 0.0f;
                    EftWaterRing_Spawn(&gEftDust->ringList[0], &gEftDust->ringList[1], pos, 0.0f, size,
                                  EFT_SCALE(wake->objId, scale) * 10.0f, -24.0f,
                                  112.0f, 0.6f, 0.1f, 0xFF, 0xFF, 0xFF, 2, 0, 0xD);
                }
                wake->frame++;
            }
        }
    }
}

/* A free splash record, zeroed; NULL when all ten are in use. */
EftWaterSplash *EftWater_AllocSplash(void) {
    s32 i;
    EftWaterSplash *splash = gEftDust->splash;

    for (i = 0; i < EFT_WATER_SPLASH_COUNT; i++, splash++) {
        if (splash->state == 0) {
            memset(splash, 0, sizeof(EftWaterSplash));
            return splash;
        }
    }
    return NULL;
}

/* 1 when every particle of a splash has died. */
s32 EftWaterSplash_IsEmpty(EftWaterSplash *splash) {
    if (splash->dropList[0] != NULL) {
        return 0;
    }
    if (splash->sprayList[0] != NULL) {
        return 0;
    }
    return splash->ringList[0] == NULL;
}

/* Creates a splash at pos: sprayA + sprayB streaks, dropA + dropB billboards and four surface rings, all scaled by
   scale. The streak and billboard directions are spread evenly around a random start angle. */
void EftWater_AddSplash(EftWaterSplash **head, EftWaterSplash **tail, EftEVec pos, s32 sprayA, s32 sprayB, s32 dropA,
                        s32 dropB, f32 scale) {
    EftWaterSplash *splash;
    f32 height;
    f32 yaw;
    f32 step;
    f32 spread;
    f32 dist;
    f32 speed;
    f32 gravity;
    f32 size;
    f32 sizeVel;
    f32 life;
    f32 pitch;
    s32 i;

    splash = EftWater_AllocSplash();
    if (splash != NULL) {
        if (*head == NULL) {
            *head = splash;
            splash->prev = NULL;
            splash->next = NULL;
        } else {
            splash->prev = *tail;
            splash->next = NULL;
        }
        if (*tail != NULL) {
            (*tail)->next = splash;
        }
        *tail = splash;
        Vec4_Copy((Vec4 *)&splash->pos, (Vec4 *)&pos);
        splash->state = 2;
        gEftDust->splashCount++;
        height = 7.0f * scale;
        yaw = EFT_RANDF() * 360.0f;
        step = 360.0f / sprayA;
        spread = EFT_RANDF() * 0.2f;
        for (i = 0; i < sprayA; i++) {
            dist = 0.0f;
            speed = 40.0f;
            gravity = 6.4f;
            sizeVel = EFT_RANDF() * 2.4f + 5.6f;
            life = EFT_RANDF() * 0.4f + 0.5f;
            pitch = EFT_RANDF() * 12.5f + 62.5f;
            speed *= scale;
            sizeVel *= scale;
            gravity *= scale;
            EftWaterSpray_Spawn(&splash->sprayList[0], &splash->sprayList[1], D_002C3630, dist, height, yaw + step * i, pitch,
                          0.0f, 0.8f, 0.65f, speed, 0.0f, sizeVel, -5.0f, gravity, spread + 1.0f, 1.2f, life, 0.0f, 0x8C,
                          0x8C, 0x8C, 3, 0, 0x102);
        }
        yaw = EFT_RANDF() * 360.0f;
        step = 360.0f / sprayB;
        for (i = 0; i < sprayB; i++) {
            dist = 2.0f;
            speed = 40.0f;
            gravity = 6.4f;
            sizeVel = EFT_RANDF() * 4.0f + 6.0f;
            life = EFT_RANDF() * 0.4f + 0.8f;
            pitch = EFT_RANDF() * 12.5f + 72.5f;
            speed *= scale;
            sizeVel *= scale;
            gravity *= scale;
            EftWaterSpray_Spawn(&splash->sprayList[0], &splash->sprayList[1], D_002C3630, dist, height, yaw + step * i, pitch,
                          0.0f, 0.9f, 0.75f, speed, 0.0f, sizeVel, -5.0f, gravity, spread + 1.2f, 0.7f, life, 0.1f, 0x8C,
                          0x8C, 0x8C, 3, 0, 0x101);
        }
        yaw = EFT_RANDF() * 360.0f;
        step = 360.0f / dropA;
        for (i = 0; i < dropA; i++) {
            size = 2.0f;
            gravity = 11.0f;
            speed = EFT_RANDF() * 40.0f + 110.0f;
            sizeVel = EFT_RANDF() * 4.0f + 8.0f;
            life = EFT_RANDF() * 0.5f + 0.9f;
            pitch = EFT_RANDF() * 10.0f + 72.0f;
            speed *= scale;
            size *= scale;
            sizeVel *= scale;
            gravity *= scale;
            EftWaterDrop_Spawn(&splash->dropList[0], &splash->dropList[1], D_002C3630, yaw + step * i, pitch, speed,
                          EFT_RANDF() * 360.0f, 0.0f, size, sizeVel, -5.0f, gravity, life, 0.0f, 0x80, 0x80, 0x80, 4, 0,
                          0x106);
        }
        yaw = EFT_RANDF() * 360.0f;
        step = 360.0f / dropB;
        for (i = 0; i < dropB; i++) {
            speed = 55.0f;
            size = 2.0f;
            gravity = 3.5f;
            sizeVel = EFT_RANDF() * 4.0f + 8.0f;
            life = EFT_RANDF() * 0.6f + 1.4f;
            pitch = EFT_RANDF() * 10.0f + 75.0f;
            speed *= scale;
            size *= scale;
            sizeVel *= scale;
            gravity *= scale;
            EftWaterDrop_Spawn(&splash->dropList[0], &splash->dropList[1], D_002C3630, yaw + step * i, pitch, speed,
                          EFT_RANDF() * 360.0f, 0.0f, size, sizeVel, -5.0f, gravity, life, 0.0f, 0x80, 0x80, 0x80, 1, 0,
                          0xA);
        }
        EftWaterRing_Spawn(&splash->ringList[0], &splash->ringList[1], pos, 0.0f, scale * 5.0f, scale * 12.0f, -4.0f, 96.0f, 1.4f, 0.0f,
                      0x80, 0x80, 0x80, 0, 0, 0xE);
        EftWaterRing_Spawn(&splash->ringList[0], &splash->ringList[1], pos, 0.0f, scale * 8.0f, scale * 6.0f, -24.0f, 112.0f, 1.5f, 0.0f,
                      0xFF, 0xFF, 0xFF, 2, 0, 0xE);
        size = scale * 10.0f;
        sizeVel = scale * 4.0f;
        EftWaterRing_Spawn(&splash->ringList[0], &splash->ringList[1], pos, 0.0f, size, sizeVel, -24.0f, 96.0f, 1.2f, 0.9f,
                      0xFF, 0xFF, 0xFF, 2, 0, 0xD);
        EftWaterRing_Spawn(&splash->ringList[0], &splash->ringList[1], pos, 0.0f, size, sizeVel, -24.0f, 88.0f, 1.2f, 0.5f,
                      0xFF, 0xFF, 0xFF, 2, 0, 0xD);
    }
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly eft_f.c), with its own header and view types. Names the first part already declared with
 * other types are reached through cast macros (the generated code is the same).
 * ------------------------------------------------------------------------------------------------------------ */
/* The first part reaches these through aliases with its own types; from here on they are the real functions. */
#undef EftWaterSplash_UpdateList
#undef EftWaterSplash_DrawList
#undef EftWaterTrail_Spawn
#undef EftWaterTrail_UpdateList
#undef EftWaterTrail_DrawList
#undef EftWaterDrop_Spawn
#undef EftWaterDrop_UpdateList
#undef EftWaterDrop_DrawList
#undef EftWaterRing_Spawn
#undef EftWaterRing_UpdateList
#undef EftWaterRing_DrawList
#undef EftWaterSpray_Spawn
#undef EftWaterSpray_UpdateList
#undef EftWaterSpray_DrawList
#undef EftWaterMist_Spawn
#undef EftWaterMist_UpdateList
#undef EftWaterMist_DrawList
/* eft_f.h declares the module state with its own view type: hide that declaration and cast. */
#define gEftDust gEftDust_fDecl
#include "battle/eft_f.h"
#undef gEftDust
#define gEftDust ((EftWaterView *)gEftDust)
#include "sys/gfx_ot.h"

/* Water-surface effects, 0x142CA0..0x147050: the particle half of the water module that starts in eft_e.c.
   Purely visual: it reads a blast hit record's position, the fighter height and the water surface height, and
   writes only its own pools and draw packets. See include/battle/eft_f.h. */

/* Local views of what the module uses from elsewhere. */
typedef struct EftWaterBattleWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags; /* 0x100 = paused */
} EftWaterBattleWork;

typedef struct EftWaterCamView {
    /* 0x000 */ u8 unk0[0x140];
    /* 0x140 */ EftWaterMtx world2screen;
} EftWaterCamView;

#define Battle_GetWork ((EftWaterBattleWork *(*)(void))Battle_GetWork)
#define gBtlCamView ((EftWaterCamView *)gBtlCamView)
#define D_002C3630 (*(EftWaterVec *)&D_002C3630) /* (0, 0, 0, 1) */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 atan2f(f32 y, f32 x);
extern f32 sqrtf(f32 x);

#define Vec4_Set ((void (*)(EftWaterVec *dst, f32 x, f32 y, f32 z, f32 w))Vec4_Set)
#define Vec4_Copy ((void (*)(EftWaterVec *dst, EftWaterVec *src))Vec4_Copy)
#define Vec4_Add ((void (*)(EftWaterVec *dst, EftWaterVec *a, EftWaterVec *b))Vec4_Add)
#define Vec4_Sub ((void (*)(EftWaterVec *dst, EftWaterVec *a, EftWaterVec *b))Vec4_Sub)
extern void Vec4_Scale(EftWaterVec *dst, EftWaterVec *src, f32 scale);
#define Vec3_Length ((f32 (*)(EftWaterVec *v))Vec3_Length)
#define Vec3_Normalize ((void (*)(EftWaterVec *dst, EftWaterVec *src))Vec3_Normalize)
extern f32 Mathf_SinFast(f32 angle);
extern f32 Mathf_CosFast(f32 angle);
extern f32 BtlCharApi_GetHeight(s32 objId);

extern s32 EftWater_GetSurfaceY(f32 *outY); /* eft_e.c: height of the water surface; 0 when there is none */
#define EftWaterSplash_IsEmpty ((s32 (*)(EftWaterSplashView *src))EftWaterSplash_IsEmpty) /* eft_e.c: 1 when the splash has nothing left */
extern f32 EftMath_WrapAngle(f32 angle);        /* wraps an angle into -pi..pi */
extern void Mtx_Copy(EftWaterMtx *dst, EftWaterMtx *src); /* matrix copy */

extern void Vec3_Lerp(EftWaterVec *out, EftWaterVec *up, EftWaterVec *dir, f32 angle); /* orientation from an up vector, a direction and a tilt */
extern void EftUtil_MakeFacingMtx(EftWaterMtx *out, EftWaterVec *rot, EftWaterVec *pos);            /* matrix from that orientation and a position */

/* libc rand() scaled to 0..1 (appearance only) */
#define EFT_WATER_RANDF() ((f32)rand() / 2147483647.0f)
/* every size and speed of a trail is scaled by the fighter's height and the hit record's speed */
#define EFT_WATER_SCALE(objId, scale) (BtlCharApi_GetHeight(objId) * 0.05f * (scale))
extern void Mtx_StoreIdentity(EftWaterMtx *m);
#define Mtx_MulVec4 ((void (*)(EftWaterVec *dst, EftWaterMtx *m, EftWaterVec *src))Mtx_MulVec4)
extern void Mtx_Mul(EftWaterMtx *dst, EftWaterMtx *a, EftWaterMtx *b);  /* matrix product */
extern void Mtx_RotateZ(EftWaterMtx *dst, EftWaterMtx *src, f32 angle);    /* rotate about Z */
extern void Mtx_RotateY(EftWaterMtx *dst, EftWaterMtx *src, f32 angle);    /* rotate about Y */
extern s32 Mtx_ProjectPoint(EftWaterIVec *out, EftWaterMtx *m, EftWaterVec *v);  /* projects a point to GS coordinates */
extern void Mtx_ProjectPointsStq(EftWaterIVec *xyz, EftWaterVec *stq, EftWaterMtx *m, EftWaterVec *pos, EftWaterVec *uv,
                          s32 n);                                           /* projects n points, perspective STQ */
extern void ClipVtx_SetArray(EftWaterClipVtx *out, EftWaterVec *pos, EftWaterVec *uv, EftWaterVec *color, s32 n);
extern s32 ClipPoly_ClipPlane(EftWaterClipVtx *poly, EftWaterVec *plane, s32 n);  /* clips a polygon, returns its size */
extern void ClipPoly_ProjectCur(EftWaterIVec *xyz, EftWaterVec *stq, EftWaterClipVtx *poly, s32 n); /* projects it */
extern EftWaterVec *EftGfx_GetClipPlanes(void);                                    /* the 5 clip planes of the view */
extern s32 EftUtil_IsCamUnderWater(void); /* eft_g.c: camera height against the water level; picks the depth bias */
#define EftVram_AddTex ((u64 (*)(void *entry, s32 a1, s32 a2))EftVram_AddTex)                     /* advances a texture, returns TEX0 */

/* GS XYZF2 register value. */
typedef struct EftWaterXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftWaterXyzf;

typedef struct EftWaterGsVtx {
    /* 0x00 */ u8 rgba[4];
    /* 0x04 */ f32 q;
    /* 0x08 */ f32 s;
    /* 0x0C */ f32 t;
    /* 0x10 */ EftWaterXyzf xyz;
} EftWaterGsVtx; /* 0x18: RGBAQ, ST, XYZF2 */

typedef struct EftWaterPktHead {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
} EftWaterPktHead;

/* A+D packet that sets one register (TEST_1 here). 0x30 bytes. */
typedef struct EftWaterRegPkt {
    /* 0x00 */ EftWaterPktHead h;
    /* 0x20 */ u64 data;
    /* 0x28 */ u64 reg;
} EftWaterRegPkt;

/* Triangle strip of one RGBAQ and four ST + XYZF2 (EftWater_DrawBillboard). 0x80 bytes. */
typedef struct EftWaterSpritePkt {
    /* 0x00 */ EftWaterPktHead h;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ u8 rgba[4];
    /* 0x34 */ f32 q;
    /* 0x38 */ struct {
        /* 0x00 */ f32 s;
        /* 0x04 */ f32 t;
        /* 0x08 */ EftWaterXyzf xyz;
    } v[4];
    /* 0x78 */ u64 pad;
} EftWaterSpritePkt;

/* Triangle strip of four full vertices (EftWater_DrawSprayQuad). 0x90 bytes. */
typedef struct EftWaterQuadPkt {
    /* 0x00 */ EftWaterPktHead h;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftWaterGsVtx v[4];
} EftWaterQuadPkt;

/* One triangle (EftWater_DrawClippedFan). 0x80 bytes. */
typedef struct EftWaterTriPkt {
    /* 0x00 */ EftWaterPktHead h;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftWaterGsVtx v[3];
    /* 0x78 */ u64 pad;
} EftWaterTriPkt;

/* Links a packet into the chain of a depth slot (clamped to 0..0xFFF). */
static inline void EftWaterOt_Add(OtPrim *p, s32 z, s32 layer) {
    OtEntry *e;

    if (layer >= 2) {
        layer -= 2;
    }
    if (z < 0) {
        e = &gOtZ[0].layer[layer];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[layer];
    } else {
        e = &gOtZ[z].layer[layer];
    }
    e->tail->next = p;
    e->tail = p;
}

/* Queues a packet that sets GS TEST_1 at depth z; leaves the calling function when the packet memory is used up. */
#define EFT_WATER_QUEUE_TEST(test, z, layer)                                                                         \
    {                                                                                                               \
        EftWaterRegPkt *q = (EftWaterRegPkt *)gOtCur;                                                                 \
        u64 *d;                                                                                                     \
                                                                                                                    \
        gOtCur = (u32 *)(q + 1);                                                                                    \
        d = &q->h.gif0;                                                                                             \
        if (q == NULL) {                                                                                            \
            return;                                                                                                 \
        }                                                                                                           \
        q->h.tag = 0x20000002;                                                                                      \
        q->h.vif0 = 0x10000000;                                                                                     \
        q->h.vif1 = 0x50000002;                                                                                     \
        q->h.next = 0;                                                                                              \
        *d = 0x1000000000008001;                                                                                    \
        q->h.gif1 = 0xE;                                                                                            \
        q->data = (test);                                                                                           \
        q->reg = 0x47;                                                                                              \
        EftWaterOt_Add((OtPrim *)q, z, layer);                                                                       \
    }

#define EFT_WATER_PAUSED() (Battle_GetWork()->flags & 0x100)

/* Updates every source of a list and unlinks the ones that are finished. */
void EftWaterSplash_UpdateList(EftWaterSplashView **head, EftWaterSplashView **tail) {
    EftWaterSplashView *next;
    EftWaterSplashView *src;

    if (gEftDust->srcCount != 0) {
        if (!EFT_WATER_PAUSED()) {
            next = *head;
            gEftDust->unk24 = 0;
            while (next != NULL) {
                s32 done;

                src = next;
                done = EftWaterSplash_Update(next);
                next = next->next;
                if (done) {
                    src->flags = 0;
                    if (src->prev == NULL) {
                        *head = src->next;
                    } else {
                        src->prev->next = src->next;
                    }
                    if (src->next == NULL) {
                        *tail = src->prev;
                    } else {
                        src->next->prev = src->prev;
                    }
                    src->prev = NULL;
                    src->next = NULL;
                    gEftDust->srcCount--;
                }
            }
        }
    }
}

/* Updates a source's three particle lists; returns 1 when it has nothing left. */
s32 EftWaterSplash_Update(EftWaterSplashView *src) {
    EftWaterDrop_UpdateList(&src->dropHead, &src->dropTail);
    EftWaterSpray_UpdateList(&src->sprayHead, &src->sprayTail);
    EftWaterRing_UpdateList(&src->ringHead, &src->ringTail);
    return EftWaterSplash_IsEmpty(src) != 0;
}

/* Draws every source of a list. */
void EftWaterSplash_DrawList(EftWaterSplashView *src) {
    EftWaterVec pos;
    f32 y;

    for (; src != NULL; src = src->next) {
        Vec4_Copy(&pos, &src->pos);
        if (EftWater_GetSurfaceY(&y)) {
            pos.y = y + -1.0f;
        }
        EftWaterDrop_DrawList(src->dropHead, src->pos);
        EftWaterSpray_DrawList(src->sprayHead, src->pos);
        EftWaterRing_DrawList(src->ringHead);
    }
}

/* Starts the final fade of everything a source owns (once). */
void EftWaterSplash_FadeOut(EftWaterSplashView *src) {
    if (!(src->flags & EFT_WATER_ENDING)) {
        EftWaterDrop_FadeOutList(src->dropHead);
        EftWaterRing_FadeOutList(src->ringHead);
        EftWaterSpray_FadeOutList(src->sprayHead);
        src->flags |= EFT_WATER_ENDING;
    }
}

/* Takes a free trail from the pool of 15, zeroed; NULL when all are in use. */
EftWaterTrail *EftWaterTrail_Alloc(void) {
    EftWaterTrail *trail;
    s32 i;

    trail = gEftDust->trailPool;
    for (i = 0; i < 15; i++, trail++) {
        if (trail->flags == 0) {
            memset(trail, 0, sizeof(EftWaterTrail));
            return trail;
        }
    }
    return NULL;
}

/* 1 when all three particle lists of a trail are empty. */
s32 EftWaterTrail_IsEmpty(EftWaterTrail *trail) {
    if (trail->dropHead != NULL) {
        return 0;
    }
    if (trail->sprayHead != NULL) {
        return 0;
    }
    return trail->ringHead == NULL;
}

/* Starts a trail where a blast hit record is: two sprays to the sides of its direction of travel, two drops along
   it and a ground ring, all scaled by the fighter's height and by the record's horizontal speed. */
void EftWaterTrail_Spawn(EftWaterTrail **head, EftWaterTrail **tail, s32 objId, EftWaterHitRec *hit) {
    EftWaterTrail *trail;
    EftWaterVec pos;
    EftWaterVec dir;
    EftWaterVec delta;
    f32 y;
    f32 yaw;
    f32 height;
    f32 scale;
    f32 size;
    f32 sizeVel;
    f32 life;
    f32 pitch;
    f32 speed;
    f32 gravity;
    f32 dropYaw;
    f32 dist;
    s32 i;

    trail = EftWaterTrail_Alloc();
    if (trail != NULL) {
        if (*head == NULL) {
            *head = trail;
            trail->prev = NULL;
            trail->next = NULL;
        } else {
            trail->prev = *tail;
            trail->next = NULL;
        }
        if (*tail != NULL) {
            (*tail)->next = trail;
        }
        *tail = trail;
        trail->flags = EFT_WATER_LIVE;
        gEftDust->trailCount++;
        Vec4_Copy(&pos, &hit->pos);
        if (EftWater_GetSurfaceY(&y)) {
            pos.y = y + -1.0f;
        }
        Vec4_Sub(&delta, &hit->pos, &hit->prevPos);
        Vec3_Normalize(&dir, &delta);
        delta.y = 0.0f;
        scale = sqrtf(Vec3_Length(&delta)) * 0.26f;
        if (scale < 0.9f) {
            scale *= 0.65f;
        }
        yaw = atan2f(dir.x, dir.z) * 180.0f / 3.14159265f;
        height = 7.0f * EFT_WATER_SCALE(objId, scale);
        for (i = 0; i < 2; i++) {
            dist = 0.0f;
            speed = 40.0f;
            gravity = 6.4f;
            size = EFT_WATER_RANDF() * 3.0f + 1.0f;
            sizeVel = EFT_WATER_RANDF() * 3.0f + 5.0f;
            life = EFT_WATER_RANDF() * 0.2f + 0.3f;
            pitch = EFT_WATER_RANDF() * 10.0f + 50.0f;
            speed *= EFT_WATER_SCALE(objId, scale);
            sizeVel *= EFT_WATER_SCALE(objId, scale);
            gravity *= EFT_WATER_SCALE(objId, scale);
            EftWaterSpray_Spawn(&trail->sprayHead, &trail->sprayTail, pos, dist, height,
                                yaw - 60.0f + i * 120.0f, pitch, EFT_WATER_RANDF() * 360.0f, 0.8f, 0.6f, speed, size,
                                sizeVel, -5.0f, gravity, 1.0f, 1.5f, life, 0.0f, 0x8C, 0x8C, 0x8C, 3, 0,
                                EFT_WATER_RISE | EFT_WATER_LIVE);
        }
        speed = 90.0f;
        size = 2.0f;
        gravity = 9.0f;
        sizeVel = EFT_WATER_RANDF() * 3.5f + 6.5f;
        life = EFT_WATER_RANDF() * 0.2f + 0.4f;
        pitch = EFT_WATER_RANDF() * 10.0f + 60.0f;
        speed *= EFT_WATER_SCALE(objId, scale);
        size *= EFT_WATER_SCALE(objId, scale);
        sizeVel *= EFT_WATER_SCALE(objId, scale);
        gravity *= EFT_WATER_SCALE(objId, scale);
        dropYaw = yaw - 30.0f + (rand() % 2) * 60.0f;
        EftWaterDrop_Spawn(&trail->dropHead, &trail->dropTail, pos, dropYaw, pitch, speed,
                          EFT_WATER_RANDF() * 360.0f, 0.0f, size, sizeVel, -5.0f, gravity, life, 0.0f, 0x80, 0x80,
                          0x80, 4, 0, EFT_WATER_RISE | EFT_WATER_FADE_IN | EFT_WATER_LIVE);
        speed = 55.0f;
        size = 2.0f;
        gravity = 3.5f;
        sizeVel = EFT_WATER_RANDF() * 3.5f + 6.5f;
        life = EFT_WATER_RANDF() * 0.3f + 0.6f;
        pitch = EFT_WATER_RANDF() * 10.0f + 70.0f;
        speed *= EFT_WATER_SCALE(objId, scale);
        size *= EFT_WATER_SCALE(objId, scale);
        sizeVel *= EFT_WATER_SCALE(objId, scale);
        gravity *= EFT_WATER_SCALE(objId, scale);
        EftWaterDrop_Spawn(&trail->dropHead, &trail->dropTail, pos, yaw, pitch, speed,
                          EFT_WATER_RANDF() * 360.0f, 0.0f, size, sizeVel, -5.0f, gravity, life, 0.0f, 0x80, 0x80,
                          0x80, 1, 0, EFT_WATER_FADE_OUT | EFT_WATER_LIVE);
        size = 0.0f;
        sizeVel = 10.0f;
        size *= EFT_WATER_SCALE(objId, scale);
        sizeVel *= EFT_WATER_SCALE(objId, scale);
        EftWaterRing_Spawn(&trail->ringHead, &trail->ringTail, pos, 0.0f, size, sizeVel, -24.0f, 112.0f, 0.6f, 0.1f,
                          0xFF, 0xFF, 0xFF, 2, 0, EFT_WATER_FADE_OUT | EFT_WATER_FADE_IN | EFT_WATER_WAIT);
    }
}

/* Updates every trail of a list, unlinks the finished ones and fades the oldest so at most 5 stay at full strength. */
void EftWaterTrail_UpdateList(EftWaterTrail **head, EftWaterTrail **tail) {
    EftWaterTrail *next;
    EftWaterTrail *trail;

    if (gEftDust->trailCount != 0) {
        if (!EFT_WATER_PAUSED()) {
            next = *head;
            gEftDust->trailFading = 0;
            while (next != NULL) {
                s32 done;

                trail = next;
                done = EftWaterTrail_Update(next);
                next = next->next;
                if (done) {
                    trail->flags = 0;
                    if (trail->prev == NULL) {
                        *head = trail->next;
                    } else {
                        trail->prev->next = trail->next;
                    }
                    if (trail->next == NULL) {
                        *tail = trail->prev;
                    } else {
                        trail->next->prev = trail->prev;
                    }
                    trail->prev = NULL;
                    trail->next = NULL;
                    gEftDust->trailCount--;
                }
                if ((u32)(gEftDust->trailCount - gEftDust->trailFading) >= 6) {
                    EftWaterTrail_FadeOut(trail);
                    gEftDust->trailFading++;
                }
            }
        }
    }
}

/* Updates a trail's three particle lists; returns 1 when it has nothing left. */
s32 EftWaterTrail_Update(EftWaterTrail *trail) {
    EftWaterDrop_UpdateList(&trail->dropHead, &trail->dropTail);
    EftWaterSpray_UpdateList(&trail->sprayHead, &trail->sprayTail);
    EftWaterRing_UpdateList(&trail->ringHead, &trail->ringTail);
    return EftWaterTrail_IsEmpty(trail) != 0;
}

/* Draws every trail of a list (its particles hold world positions, so the origin is zero). */
void EftWaterTrail_DrawList(EftWaterTrail *trail) {
    for (; trail != NULL; trail = trail->next) {
        EftWaterDrop_DrawList(trail->dropHead, D_002C3630);
        EftWaterSpray_DrawList(trail->sprayHead, D_002C3630);
        EftWaterRing_DrawList(trail->ringHead);
    }
}

/* Starts the final fade of everything a trail owns (once). */
void EftWaterTrail_FadeOut(EftWaterTrail *trail) {
    if (!(trail->flags & EFT_WATER_ENDING)) {
        EftWaterDrop_FadeOutList(trail->dropHead);
        EftWaterRing_FadeOutList(trail->ringHead);
        EftWaterSpray_FadeOutList(trail->sprayHead);
        trail->flags |= EFT_WATER_ENDING;
    }
}

/* Takes a free drop from the pool of 60, zeroed; NULL when all are in use. */
EftWaterDrop *EftWaterDrop_Alloc(void) {
    EftWaterDrop *drop;
    s32 i;

    drop = gEftDust->dropPool;
    for (i = 0; i < 60; i++, drop++) {
        if (drop->flags == 0) {
            memset(drop, 0, sizeof(EftWaterDrop));
            return drop;
        }
    }
    return NULL;
}

/* Adds a drop to a list: launched from pos along yaw / pitch (degrees) at `speed`, fading over `life` seconds. */
void EftWaterDrop_Spawn(EftWaterDrop **head, EftWaterDrop **tail, EftWaterVec pos, f32 yaw, f32 pitch, f32 speed, f32 rot, f32 rotVel, f32 size, f32 sizeVel, f32 sizeDamp,
                       f32 gravity, f32 life, f32 delay, u8 r, u8 g, u8 b, u8 tex,
                       u8 layer, s32 flags) {
    EftWaterDrop *drop;
    f32 angle;

    drop = EftWaterDrop_Alloc();
    if (drop != NULL) {
        if (*head == NULL) {
            *head = drop;
            drop->prev = NULL;
            drop->next = NULL;
        } else {
            drop->prev = *tail;
            drop->next = NULL;
        }
        if (*tail != NULL) {
            (*tail)->next = drop;
        }
        *tail = drop;
        Vec4_Copy(&drop->pos, &D_002C3630);
        Vec4_Copy(&drop->base, &pos);
        drop->rot = rot;
        drop->rotVel = rotVel;
        drop->size = size;
        drop->sizeVel = sizeVel;
        drop->sizeDamp = (sizeDamp == 0.0f) ? 0.0f : 1.0f / sizeDamp;
        drop->gravity = gravity * 9.80665f;
        drop->dt = 1.0f / 30.0f;
        drop->delay = delay * 30.0f;
        drop->r = r;
        drop->g = g;
        drop->b = b;
        drop->tex = tex;
        drop->layer = layer;
        drop->flags = flags;
        drop->t = 0.0f;
        angle = EftMath_WrapAngle(yaw * 3.14159265f / 180.0f);
        drop->vel.x = Mathf_SinFast(angle);
        drop->vel.y = 0.0f;
        drop->vel.z = Mathf_CosFast(angle);
        angle = pitch * 3.14159265f / 180.0f;
        drop->vel.w = 1.0f;
        Vec3_Normalize(&drop->vel, &drop->vel);
        drop->vel.x = drop->vel.x * speed * Mathf_CosFast(angle);
        drop->vel.y = -speed * Mathf_SinFast(angle);
        drop->vel.z = drop->vel.z * speed * Mathf_CosFast(angle);
        if (flags & EFT_WATER_RISE) {
            drop->alpha = 0.0f;
            drop->alphaStep = 128.0f / (life * 30.0f);
        } else {
            drop->alpha = 128.0f;
            drop->alphaStep = 128.0f / (life * 30.0f);
        }
        gEftDust->dropCount++;
    }
}

/* Updates every drop of a list and unlinks the dead ones. */
void EftWaterDrop_UpdateList(EftWaterDrop **head, EftWaterDrop **tail) {
    EftWaterDrop *next;
    EftWaterDrop *drop;

    if (!EFT_WATER_PAUSED()) {
        next = *head;
        while (next != NULL) {
            s32 done;

            drop = next;
            done = EftWaterDrop_Update(next);
            next = next->next;
            if (done) {
                drop->flags = 0;
                if (drop->prev == NULL) {
                    *head = drop->next;
                } else {
                    drop->prev->next = drop->next;
                }
                if (drop->next == NULL) {
                    *tail = drop->prev;
                } else {
                    drop->next->prev = drop->prev;
                }
                drop->prev = NULL;
                drop->next = NULL;
                gEftDust->dropCount--;
            }
        }
    }
}

/* One frame of a drop: delay, ballistic motion, growth and fades. Returns 1 when it is dead. */
s32 EftWaterDrop_Update(EftWaterDrop *drop) {
    if (drop->flags & EFT_WATER_WAIT) {
        drop->delay -= 1.0f;
        if (drop->delay <= 0.0f) {
            drop->delay = 0.0f;
            drop->flags = (drop->flags & ~EFT_WATER_WAIT) | EFT_WATER_LIVE;
        }
    }
    if (drop->flags & EFT_WATER_LIVE) {
        drop->pos.x = drop->base.x + drop->vel.x * drop->t;
        drop->pos.y = drop->base.y + drop->vel.y * drop->t + drop->gravity * drop->t * drop->t;
        drop->pos.z = drop->base.z + drop->vel.z * drop->t;
        drop->size += drop->sizeVel;
        drop->sizeVel += drop->sizeVel * drop->sizeDamp;
        drop->t += drop->dt;
        if (!(drop->flags & EFT_WATER_ENDING)) {
            if (drop->flags & EFT_WATER_FADE_IN) {
                drop->alpha += drop->alphaStep;
                if (drop->alpha >= 128.0f) {
                    drop->alpha = 128.0f;
                    if (drop->flags & EFT_WATER_RISE) {
                        drop->flags |= EFT_WATER_ENDING;
                    }
                }
            } else if (drop->flags & EFT_WATER_FADE_OUT) {
                drop->alpha -= drop->alphaStep;
                if (drop->alpha <= 0.0f) {
                    drop->alpha = 0.0f;
                    drop->flags |= EFT_WATER_ENDING;
                }
            }
        }
    }
    if (drop->flags & EFT_WATER_ENDING) {
        if (drop->flags & EFT_WATER_RISE) {
            drop->alpha += drop->alphaStep;
            if (drop->alpha >= 128.0f) {
                drop->alpha = 128.0f;
                drop->flags |= EFT_WATER_DEAD;
            }
        } else if (drop->flags & EFT_WATER_FADE_OUT) {
            drop->alpha -= drop->alphaStep;
            if (drop->alpha <= 0.0f) {
                drop->alpha = 0.0f;
                drop->flags |= EFT_WATER_DEAD;
            }
        }
    }
    if (drop->flags & EFT_WATER_DEAD) {
        return 1;
    }
    return 0;
}

/* Draws the live drops of a list as camera-facing quads at origin + pos. */
void EftWaterDrop_DrawList(EftWaterDrop *drop, EftWaterVec origin) {
    EftWaterMtx world2screen;
    EftWaterVec pos;

    Mtx_Copy(&world2screen, &gBtlCamView->world2screen);
    for (; drop != NULL; drop = drop->next) {
        if (drop->flags & EFT_WATER_LIVE) {
            Vec4_Add(&pos, &drop->pos, &origin);
            pos.w = 1.0f;
            if (drop->flags & EFT_WATER_RISE) {
                EftWater_DrawBillboard(&pos, &world2screen, drop->size, drop->rot, drop->r, drop->g, drop->b, 0x60,
                                      (u32)drop->alpha, &gEftDust->tex[drop->tex].tex0, drop->layer);
            } else {
                EftWater_DrawBillboard(&pos, &world2screen, drop->size, drop->rot, drop->r, drop->g, drop->b,
                                      (u32)drop->alpha, 0, &gEftDust->tex[drop->tex].tex0, drop->layer);
            }
        }
    }
}

/* Makes every drop of a list finish within 0.1 s. */
void EftWaterDrop_FadeOutList(EftWaterDrop *drop) {
    for (; drop != NULL; drop = drop->next) {
        if (!(drop->flags & EFT_WATER_ENDING)) {
            if (drop->flags & EFT_WATER_RISE) {
                drop->alphaStep = (128.0f - drop->alpha) / (0.1f * 30.0f);
            } else {
                drop->alphaStep = drop->alpha / (0.1f * 30.0f);
            }
            drop->flags |= EFT_WATER_ENDING;
        }
    }
}

/* Takes a free ring from the pool of 30, zeroed; NULL when all are in use. */
EftWaterRing *EftWaterRing_Alloc(void) {
    EftWaterRing *ring;
    s32 i;

    ring = gEftDust->ringPool;
    for (i = 0; i < 30; i++, ring++) {
        if (ring->flags == 0) {
            memset(ring, 0, sizeof(EftWaterRing));
            return ring;
        }
    }
    return NULL;
}

/* Adds a ground ring to a list. With FADE_IN it rises to alphaMax over 30% of `time` and falls over the rest. */
void EftWaterRing_Spawn(EftWaterRing **head, EftWaterRing **tail, EftWaterVec pos, f32 rot, f32 size, f32 sizeVel, f32 sizeDamp, f32 alphaMax, f32 time, f32 delay, u8 r, u8 g, u8 b, u8 tex,
                       u8 layer, s32 flags) {
    EftWaterRing *ring;

    ring = EftWaterRing_Alloc();
    if (ring != NULL) {
        if (*head == NULL) {
            *head = ring;
            ring->prev = NULL;
            ring->next = NULL;
        } else {
            ring->prev = *tail;
            ring->next = NULL;
        }
        if (*tail != NULL) {
            (*tail)->next = ring;
        }
        *tail = ring;
        Vec4_Copy(&ring->pos, &pos);
        ring->size = size;
        ring->sizeVel = sizeVel;
        ring->sizeDamp = (sizeDamp == 0.0f) ? 0.0f : 1.0f / sizeDamp;
        ring->rot = rot;
        ring->delay = delay * 30.0f;
        ring->r = r;
        ring->g = g;
        ring->b = b;
        ring->tex = tex;
        ring->layer = layer;
        ring->flags = flags;
        if (flags & EFT_WATER_FADE_IN) {
            ring->alphaMax = alphaMax;
            ring->alpha = 0.0f;
            ring->fadeTime = time * 0.7f;
            ring->alphaStep = alphaMax / (time * 0.3f * 30.0f);
        } else if (flags & EFT_WATER_FADE_OUT) {
            ring->fadeTime = time;
            ring->alpha = alphaMax;
            ring->alphaMax = 0.0f;
            ring->alphaStep = alphaMax / (time * 30.0f);
        }
        gEftDust->ringCount++;
    }
}

/* Updates every ring of a list and unlinks the dead ones. */
void EftWaterRing_UpdateList(EftWaterRing **head, EftWaterRing **tail) {
    EftWaterRing *next;
    EftWaterRing *ring;

    if (gEftDust->ringCount != 0) {
        if (!EFT_WATER_PAUSED()) {
            next = *head;
            while (next != NULL) {
                s32 done;

                ring = next;
                done = EftWaterRing_Update(next);
                next = next->next;
                if (done) {
                    ring->flags = 0;
                    if (ring->prev == NULL) {
                        *head = ring->next;
                    } else {
                        ring->prev->next = ring->next;
                    }
                    if (ring->next == NULL) {
                        *tail = ring->prev;
                    } else {
                        ring->next->prev = ring->prev;
                    }
                    ring->prev = NULL;
                    ring->next = NULL;
                    gEftDust->ringCount--;
                }
            }
        }
    }
}

/* One frame of a ring: delay, follow the surface, grow, fade in then out. Returns 1 when it is dead. */
s32 EftWaterRing_Update(EftWaterRing *ring) {
    f32 y;

    if (ring->flags & EFT_WATER_WAIT) {
        ring->delay -= 1.0f;
        if (ring->delay <= 0.0f) {
            ring->delay = 0.0f;
            ring->flags = (ring->flags | EFT_WATER_LIVE) & ~EFT_WATER_WAIT;
        }
    }
    if (ring->flags & EFT_WATER_LIVE) {
        if (EftWater_GetSurfaceY(&y)) {
            ring->pos.y = y - 4.0f;
        }
        ring->size += ring->sizeVel;
        ring->sizeVel += ring->sizeVel * ring->sizeDamp;
        if (!(ring->flags & EFT_WATER_ENDING)) {
            if (ring->flags & EFT_WATER_FADE_IN) {
                ring->alpha += ring->alphaStep;
                if (ring->alpha >= ring->alphaMax) {
                    ring->alpha = ring->alphaMax;
                    ring->flags &= ~EFT_WATER_FADE_IN;
                    if (ring->flags & EFT_WATER_FADE_OUT) {
                        ring->alphaMax = 0.0f;
                        ring->alphaStep = ring->alpha / (ring->fadeTime * 30.0f);
                    }
                }
            } else if (ring->flags & EFT_WATER_FADE_OUT) {
                ring->alpha -= ring->alphaStep;
                if (ring->alpha <= ring->alphaMax) {
                    ring->alpha = ring->alphaMax;
                    ring->flags = (ring->flags & ~EFT_WATER_FADE_OUT) | EFT_WATER_ENDING;
                }
            }
        }
    }
    if (ring->flags & EFT_WATER_ENDING) {
        ring->alpha -= ring->alphaStep;
        if (ring->alpha <= 0.0f) {
            ring->alpha = 0.0f;
            ring->flags |= EFT_WATER_DEAD;
        }
    }
    if (ring->flags & EFT_WATER_DEAD) {
        return 1;
    }
    return 0;
}

/* Draws the live rings of a list as quads lying on the ground. */
void EftWaterRing_DrawList(EftWaterRing *ring) {
    EftWaterMtx world2screen;

    Mtx_Copy(&world2screen, &gBtlCamView->world2screen);
    for (; ring != NULL; ring = ring->next) {
        if (ring->flags & EFT_WATER_LIVE) {
            EftWater_DrawGroundQuad(&ring->pos, &world2screen, ring->size, ring->rot, ring->r, ring->g, ring->b,
                                   (u32)ring->alpha, &gEftDust->tex[ring->tex].tex0, ring->layer);
        }
    }
}

/* Makes every ring of a list fade out within 0.1 s. */
void EftWaterRing_FadeOutList(EftWaterRing *ring) {
    for (; ring != NULL; ring = ring->next) {
        if (!(ring->flags & EFT_WATER_ENDING)) {
            ring->flags |= EFT_WATER_ENDING;
            ring->alphaStep = ring->alpha / (0.1f * 30.0f);
        }
    }
}

/* Takes a free spray from the pool of 30, zeroed; NULL when all are in use. */
EftWaterSpray *EftWaterSpray_Alloc(void) {
    EftWaterSpray *spray;
    s32 i;

    spray = gEftDust->sprayPool;
    for (i = 0; i < 30; i++, spray++) {
        if (spray->flags == 0) {
            memset(spray, 0, sizeof(EftWaterSpray));
            return spray;
        }
    }
    return NULL;
}

/* Adds a spray to a list: starts `dist` out from pos along yaw, `height` up, and flies along yaw / pitch. */
void EftWaterSpray_Spawn(EftWaterSpray **head, EftWaterSpray **tail, EftWaterVec pos, f32 dist, f32 height, f32 yaw, f32 pitch, f32 roll, f32 tilt, f32 tiltEnd,
                         f32 speed, f32 size, f32 sizeVel, f32 sizeDamp, f32 gravity, f32 nearScale,
                         f32 widthScale, f32 life, f32 delay, u8 r, u8 g, u8 b, u8 tex,
                         u8 layer, s32 flags) {
    EftWaterSpray *spray;
    EftWaterVec offset;
    f32 angle;

    spray = EftWaterSpray_Alloc();
    if (spray != NULL) {
        if (*head == NULL) {
            *head = spray;
            spray->prev = NULL;
            spray->next = NULL;
        } else {
            spray->prev = *tail;
            spray->next = NULL;
        }
        if (*tail != NULL) {
            (*tail)->next = spray;
        }
        *tail = spray;
        Vec4_Set(&spray->pos, 0.0f, 0.0f, 0.0f, 1.0f);
        Vec4_Copy(&spray->base, &pos);
        spray->base.y += height;
        spray->size = size;
        spray->sizeVel = sizeVel;
        spray->tilt = tilt;
        spray->tiltVel = (tiltEnd - tilt) / (life * 30.0f);
        if (sizeDamp != 0.0f) {
            spray->sizeDamp = 1.0f / sizeDamp;
        } else {
            spray->sizeDamp = 0.0f;
        }
        spray->gravity = gravity * 9.80665f;
        spray->nearScale = nearScale;
        spray->widthScale = widthScale;
        spray->dt = 1.0f / 30.0f;
        spray->delay = delay * 30.0f;
        spray->roll = roll;
        spray->r = r;
        spray->g = g;
        spray->b = b;
        spray->tex = tex;
        spray->layer = layer;
        spray->flags = flags;
        spray->t = 0.0f;
        angle = EftMath_WrapAngle(yaw * 3.14159265f / 180.0f);
        spray->dir.x = Mathf_SinFast(angle);
        spray->dir.y = 0.0f;
        spray->dir.z = Mathf_CosFast(angle);
        spray->dir.w = 1.0f;
        angle = pitch * 3.14159265f / 180.0f;
        Vec3_Normalize(&spray->dir, &spray->dir);
        spray->vel.x = spray->dir.x * speed * Mathf_CosFast(angle);
        spray->vel.y = -speed * Mathf_SinFast(angle);
        spray->vel.z = spray->dir.z * speed * Mathf_CosFast(angle);
        Vec4_Scale(&offset, &spray->dir, dist);
        Vec4_Add(&spray->base, &spray->base, &offset);
        spray->base.w = 1.0f;
        if (flags & EFT_WATER_RISE) {
            spray->progress = 0.0f;
            spray->progressMax = 96.0f;
            spray->progressStep = 96.0f / (life * 1.6f * 30.0f);
        }
        gEftDust->sprayCount++;
    }
}

/* Updates every spray of a list and unlinks the dead ones. */
void EftWaterSpray_UpdateList(EftWaterSpray **head, EftWaterSpray **tail) {
    EftWaterSpray *next;
    EftWaterSpray *spray;

    if (gEftDust->sprayCount != 0) {
        if (!EFT_WATER_PAUSED()) {
            next = *head;
            while (next != NULL) {
                s32 done;

                spray = next;
                done = EftWaterSpray_Update(next);
                next = next->next;
                if (done) {
                    spray->flags = 0;
                    if (spray->prev == NULL) {
                        *head = spray->next;
                    } else {
                        spray->prev->next = spray->next;
                    }
                    if (spray->next == NULL) {
                        *tail = spray->prev;
                    } else {
                        spray->next->prev = spray->prev;
                    }
                    spray->prev = NULL;
                    spray->next = NULL;
                    gEftDust->sprayCount--;
                }
            }
        }
    }
}

/* One frame of a spray: delay, ballistic motion, growth, tilt and dissolve. Returns 1 when it is dead. */
s32 EftWaterSpray_Update(EftWaterSpray *spray) {
    if (spray->flags & EFT_WATER_WAIT) {
        spray->delay -= 1.0f;
        if (spray->delay <= 0.0f) {
            spray->delay = 0.0f;
            spray->flags = (spray->flags | EFT_WATER_LIVE) & ~EFT_WATER_WAIT;
        }
    }
    if (spray->flags & EFT_WATER_LIVE) {
        spray->pos.x = spray->base.x + spray->vel.x * spray->t;
        spray->pos.y = spray->base.y + spray->vel.y * spray->t + spray->gravity * spray->t * spray->t;
        spray->pos.z = spray->base.z + spray->vel.z * spray->t;
        spray->size += spray->sizeVel;
        spray->sizeVel += spray->sizeVel * spray->sizeDamp;
        spray->tilt += spray->tiltVel;
        spray->t += spray->dt;
        if (!(spray->flags & EFT_WATER_ENDING)) {
            if (spray->flags & EFT_WATER_RISE) {
                spray->progress += spray->progressStep;
                if (spray->progress >= spray->progressMax) {
                    spray->progress = spray->progressMax;
                    spray->flags |= EFT_WATER_ENDING;
                }
            }
        }
    }
    if (spray->flags & EFT_WATER_ENDING) {
        if (spray->flags & EFT_WATER_RISE) {
            spray->progress += spray->progressStep;
            if (spray->progress >= spray->progressMax) {
                spray->progress = spray->progressMax;
                spray->flags |= EFT_WATER_DEAD;
            }
        }
    }
    if (spray->flags & EFT_WATER_DEAD) {
        return 1;
    }
    return 0;
}

/* Draws the live sprays of a list: a quad turned to its flight direction and tilt, at origin + pos. */
void EftWaterSpray_DrawList(EftWaterSpray *spray, EftWaterVec origin) {
    EftWaterMtx orient;
    EftWaterMtx world2screen;
    EftWaterVec up = { 0.0f, -1.0f, 0.0f, 1.0f };
    EftWaterVec pos;
    EftWaterVec rot;

    Mtx_Copy(&world2screen, &gBtlCamView->world2screen);
    for (; spray != NULL; spray = spray->next) {
        if (spray->flags & EFT_WATER_LIVE) {
            Vec3_Lerp(&rot, &up, &spray->dir, spray->tilt);
            EftUtil_MakeFacingMtx(&orient, &rot, &D_002C3630);
            Vec4_Add(&pos, &spray->pos, &origin);
            pos.w = 1.0f;
            EftWater_DrawSprayQuad(&pos, &orient, &world2screen, spray->roll, spray->size, spray->nearScale, 0.0f,
                                   spray->widthScale, spray->r, spray->g, spray->b, 0x40,
                                   (u32)spray->progress, &gEftDust->tex[spray->tex].tex0, spray->layer);
        }
    }
}

/* Makes every spray of a list finish dissolving within 0.1 s. */
void EftWaterSpray_FadeOutList(EftWaterSpray *spray) {
    for (; spray != NULL; spray = spray->next) {
        if (!(spray->flags & EFT_WATER_ENDING)) {
            spray->flags |= EFT_WATER_ENDING;
            spray->progressStep = (spray->progressMax - spray->progress) / (0.1f * 30.0f);
        }
    }
}

/* Takes a free mist from the pool of 30, zeroed; NULL when all are in use. */
EftWaterMist *EftWaterMist_Alloc(void) {
    EftWaterMist *mist;
    s32 i;

    mist = gEftDust->mistPool;
    for (i = 0; i < 30; i++, mist++) {
        if (mist->used == 0) {
            memset(mist, 0, sizeof(EftWaterMist));
            return mist;
        }
    }
    return NULL;
}

/* Adds a mist to a list: it circles pos at `radius`, its yaw turning by yawVel each frame. */
void EftWaterMist_Spawn(EftWaterMist **head, EftWaterMist **tail, EftWaterVec pos, f32 radius, f32 height, f32 yaw, f32 yawVel, f32 yawDamp, f32 pitch, f32 roll,
                        f32 tilt, f32 tiltEnd, f32 speed, f32 size, f32 sizeVel, f32 sizeDamp, f32 gravity,
                        f32 nearScale, f32 widthScale, f32 life, f32 delay, u8 r, u8 g, u8 b,
                        u8 tex, u8 layer) {
    EftWaterMist *mist;
    f32 angle;
    f32 time;

    mist = EftWaterMist_Alloc();
    if (mist != NULL) {
        if (*head == NULL) {
            *head = mist;
            mist->prev = NULL;
            mist->next = NULL;
        } else {
            mist->prev = *tail;
            mist->next = NULL;
        }
        if (*tail != NULL) {
            (*tail)->next = mist;
        }
        *tail = mist;
        Vec4_Set(&mist->pos, 0.0f, 0.0f, 0.0f, 1.0f);
        Vec4_Copy(&mist->base, &pos);
        mist->base.y += height;
        mist->radius = radius;
        mist->yawVel = yawVel;
        mist->yaw = yaw;
        if (yawDamp != 0.0f) {
            mist->yawDamp = 1.0f / yawDamp;
        } else {
            mist->yawDamp = 0.0f;
        }
        mist->size = size;
        mist->tilt = tilt;
        mist->sizeVel = sizeVel;
        mist->tiltVel = (tiltEnd - tilt) / (life * 30.0f);
        if (sizeDamp != 0.0f) {
            mist->sizeDamp = 1.0f / sizeDamp;
        } else {
            mist->sizeDamp = 0.0f;
        }
        mist->gravity = gravity * 9.80665f;
        mist->nearScale = nearScale;
        mist->widthScale = widthScale;
        mist->dt = 1.0f / 30.0f;
        mist->used = 1;
        mist->delay = delay * 30.0f;
        mist->roll = roll;
        mist->r = r;
        mist->g = g;
        mist->b = b;
        mist->tex = tex;
        mist->layer = layer;
        mist->t = 0.0f;
        mist->state = 0;
        mist->fading = 0;
        mist->live = 0;
        angle = EftMath_WrapAngle(yaw * 3.14159265f / 180.0f);
        mist->dir.x = Mathf_SinFast(angle);
        mist->dir.y = 0.0f;
        mist->dir.z = Mathf_CosFast(angle);
        angle = pitch * 3.14159265f / 180.0f;
        mist->dir.w = 1.0f;
        Vec3_Normalize(&mist->dir, &mist->dir);
        mist->vel.x = speed * Mathf_CosFast(angle);
        mist->vel.y = -speed * Mathf_SinFast(angle);
        mist->vel.z = speed * Mathf_CosFast(angle);
        mist->progress = 0.0f;
        time = life * 1.6f;
        mist->progressStep = 96.0f;
        if (time != mist->progress) {
            mist->progressStep = 96.0f / (time * 30.0f);
        }
        mist->progressMax = 96.0f;
        mist->fading |= 1;
        gEftDust->mistCount++;
    }
}

/* Updates every mist of a list and unlinks the finished ones. */
void EftWaterMist_UpdateList(EftWaterMist **head, EftWaterMist **tail) {
    EftWaterMist *next;
    EftWaterMist *mist;

    if (gEftDust->mistCount != 0) {
        if (!EFT_WATER_PAUSED()) {
            next = *head;
            while (next != NULL) {
                s32 done;

                mist = next;
                done = EftWaterMist_Update(next);
                next = next->next;
                if (done) {
                    mist->used = 0;
                    mist->live = 0;
                    if (mist->prev == NULL) {
                        *head = mist->next;
                    } else {
                        mist->prev->next = mist->next;
                    }
                    if (mist->next == NULL) {
                        *tail = mist->prev;
                    } else {
                        mist->next->prev = mist->prev;
                    }
                    mist->prev = NULL;
                    mist->next = NULL;
                    gEftDust->mistCount--;
                }
            }
        }
    }
}

/* One frame of a mist: delay, turn the direction, move out along it, grow, dissolve. Returns 1 when finished. */
s32 EftWaterMist_Update(EftWaterMist *mist) {
    EftWaterVec offset;
    EftWaterVec centre;
    f32 angle;

    switch (mist->state) {
    case 0:
        mist->delay -= 1.0f;
        if (mist->delay <= 0.0f) {
            mist->delay = 0.0f;
            mist->live = 1;
            mist->state = 1;
        } else {
            break;
        }
    case 1:
        mist->state++;
        break;
    }
    if (mist->live) {
        angle = EftMath_WrapAngle(mist->yaw * 3.14159265f / 180.0f);
        mist->dir.x = Mathf_SinFast(angle);
        mist->dir.y = 0.0f;
        mist->dir.z = Mathf_CosFast(angle);
        mist->dir.w = 1.0f;
        mist->yaw += mist->yawVel;
        mist->yawVel += mist->yawVel * mist->yawDamp;
        Vec4_Scale(&offset, &mist->dir, mist->radius);
        Vec4_Add(&centre, &mist->base, &offset);
        mist->pos.x = centre.x + mist->dir.x * mist->vel.x * mist->t;
        mist->pos.y = centre.y + mist->vel.y * mist->t + mist->gravity * mist->t * mist->t;
        mist->pos.z = centre.z + mist->dir.z * mist->vel.z * mist->t;
        centre.w = 1.0f;
        mist->size += mist->sizeVel;
        mist->sizeVel += mist->sizeVel * mist->sizeDamp;
        mist->tilt += mist->tiltVel;
        mist->t += mist->dt;
        if ((u8)(mist->fading & 1)) {
            mist->progress += mist->progressStep;
            if (mist->progressStep > 0.0f) {
                if (mist->progress > mist->progressMax) {
                    mist->progress = mist->progressMax;
                    mist->fading &= ~1;
                }
            } else {
                if (mist->progress < mist->progressMax) {
                    mist->progress = mist->progressMax;
                    mist->fading &= ~1;
                }
            }
        }
        if (!(u8)(mist->fading & 1)) {
            return 1;
        }
    }
    return 0;
}

/* Draws the live mists of a list. */
void EftWaterMist_DrawList(EftWaterMist *mist) {
    EftWaterMtx orient;
    EftWaterMtx world2screen;
    EftWaterVec2 up = { 0.0f, -1.0f, 0.0f, 1.0f };
    EftWaterVec rot;

    Mtx_Copy(&world2screen, &gBtlCamView->world2screen);
    for (; mist != NULL; mist = mist->next) {
        if (mist->live) {
            Vec3_Lerp(&rot, (EftWaterVec *)&up, &mist->dir, mist->tilt);
            EftUtil_MakeFacingMtx(&orient, &rot, &D_002C3630);
            EftWater_DrawSprayQuad(&mist->pos, &orient, &world2screen, mist->roll, mist->size, mist->nearScale,
                                   0.0f, mist->widthScale, mist->r, mist->g, mist->b, 0x40,
                                   (u32)mist->progress, &gEftDust->tex[mist->tex].tex0, mist->layer);
        }
    }
}

/* 1 when a projected vertex (GS coordinates, 12.4 fixed, centre 2048) is in front of the camera and within a
   box around the screen centre: margin 0 = the 512 x 448 screen, 1..3 = wider boxes, else the whole GS range. */
s32 EftWater_IsOnScreen(EftWaterIVec v, s32 margin) {
    switch (margin) {
    case 0:
        if (v.z <= 0) {
            return 0;
        }
        if (v.x > 0x8FFF) {
            return 0;
        }
        if (v.x <= 0x7000) {
            return 0;
        }
        if (v.y > 0x8DFF) {
            return 0;
        }
        if (v.y <= 0x7200) {
            return 0;
        }
        break;
    case 1:
        if (v.z <= 0) {
            return 0;
        }
        if (v.x > 0x93FF) {
            return 0;
        }
        if (v.x <= 0x6C00) {
            return 0;
        }
        if (v.y > 0x917F) {
            return 0;
        }
        if (v.y <= 0x6E80) {
            return 0;
        }
        break;
    case 2:
        if (v.z <= 0) {
            return 0;
        }
        if (v.x > 0x97FF) {
            return 0;
        }
        if (v.x <= 0x6800) {
            return 0;
        }
        if (v.y > 0x94FF) {
            return 0;
        }
        if (v.y <= 0x6B00) {
            return 0;
        }
        break;
    case 3:
        if (v.z <= 0) {
            return 0;
        }
        if (v.x > 0x9FFF) {
            return 0;
        }
        if (v.x <= 0x6000) {
            return 0;
        }
        if (v.y > 0x9BFF) {
            return 0;
        }
        if (v.y <= 0x6400) {
            return 0;
        }
        break;
    default:
        if (v.z <= 0) {
            return 0;
        }
        if (v.x > 0xFFEF) {
            return 0;
        }
        if (v.x <= 0x0) {
            return 0;
        }
        if (v.y > 0xFFEF) {
            return 0;
        }
        if (v.y <= 0x0) {
            return 0;
        }
        break;
    }
    return 1;
}

/* Queues a camera-facing textured quad of side `size` at pos, turned by `rot` degrees. Skipped when a corner is
   outside the wide screen box. A non-zero alphaRef wraps it in alpha-test register packets. */
void EftWater_DrawBillboard(EftWaterVec *pos, EftWaterMtx *world2screen, f32 size, f32 rot, u8 r, u8 g, u8 b, u8 a,
                           u8 alphaRef, u64 *tex, u8 layer) {
    EftWaterMtx m;
    EftWaterVec corner[4];
    EftWaterIVec scr[4];
    EftWaterMtx rotM;
    EftWaterSpritePkt *p;
    f32 half;
    s32 z;
    s32 i;

    Mtx_Copy(&m, &gEftDust->billboard);
    if (rot != 0.0f) {
        Mtx_StoreIdentity(&rotM);
        Mtx_RotateZ(&rotM, &rotM, EftMath_WrapAngle(rot * 3.14159265f / 180.0f));
        Mtx_Mul(&m, &m, &rotM);
    }
    half = size * 0.5f;
    Vec4_Copy((EftWaterVec *)m.m[3], pos);
    Vec4_Set(&corner[0], -half, -half, 0.0f, 1.0f);
    Vec4_Set(&corner[1], half, -half, 0.0f, 1.0f);
    Vec4_Set(&corner[2], -half, half, 0.0f, 1.0f);
    Vec4_Set(&corner[3], half, half, 0.0f, 1.0f);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&corner[i], &m, &corner[i]);
        Mtx_ProjectPoint(&scr[i], world2screen, &corner[i]);
        if (!EftWater_IsOnScreen(scr[i], 2)) {
            return;
        }
    }
    p = (EftWaterSpritePkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    if (p == NULL) {
        return;
    }
    p->h.tag = 0x20000007;
    p->prim = 0x54;
    p->h.vif1 = 0x50000007;
    p->h.gif0 = 0xC400000000008001;
    p->h.gif1 = 0xF42424242160;
    p->h.next = 0;
    p->h.vif0 = 0x10000000;
    p->rgba[0] = r;
    p->rgba[1] = g;
    p->rgba[2] = b;
    p->rgba[3] = a;
    p->q = 1.0f;
    p->v[0].s = 0.0f;
    p->v[0].t = 0.0f;
    p->v[1].s = 1.0f;
    p->v[1].t = 0.0f;
    p->v[2].s = 0.0f;
    p->v[2].t = 1.0f;
    p->v[3].s = 1.0f;
    p->v[3].t = 1.0f;
    p->v[0].xyz.x = scr[0].x;
    p->v[0].xyz.y = scr[0].y;
    p->v[0].xyz.z = scr[0].z;
    p->v[0].xyz.f = 0xFF;
    p->v[1].xyz.x = scr[1].x;
    p->v[1].xyz.y = scr[1].y;
    p->v[1].xyz.z = scr[1].z;
    p->v[1].xyz.f = 0xFF;
    p->v[2].xyz.x = scr[2].x;
    p->v[2].xyz.y = scr[2].y;
    p->v[2].xyz.z = scr[2].z;
    p->v[2].xyz.f = 0xFF;
    p->v[3].xyz.x = scr[3].x;
    p->v[3].xyz.y = scr[3].y;
    p->v[3].xyz.z = scr[3].z;
    p->v[3].xyz.f = 0xFF;
    z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
    if (EftUtil_IsCamUnderWater()) {
        z -= 500;
    } else {
        z += 500;
    }
    p->tex0 = *tex;
    if (alphaRef != 0) {
        EFT_WATER_QUEUE_TEST(((u64)alphaRef << 4) | 0x5000D, z, layer);
    }
    EftWaterOt_Add((OtPrim *)p, z, layer);
    if (alphaRef != 0) {
        EFT_WATER_QUEUE_TEST(0x50000, z, layer);
    }
}

/* Queues a textured quad of side `size` lying in the ground plane at pos, turned by `rot` degrees about the
   vertical, as two triangles through the clipper. */
void EftWater_DrawGroundQuad(EftWaterVec *pos, EftWaterMtx *world2screen, f32 size, f32 rot, u8 r, u8 g, u8 b, u8 a,
                            u64 *tex, u8 layer) {
    EftWaterClipVtx poly[9];
    EftWaterVec corner[4];
    EftWaterVec uv[4];
    EftWaterVec color[4];
    EftWaterMtx m;
    f32 half;
    s32 i;

#ifdef PORT
    /* A ring's size grows by a share of itself every tick (EftWaterRing_Update: sizeVel += sizeVel * sizeDamp) and
       reaches the largest number a float holds within two seconds; its quad then has corners at 1.7e38. Cut off at
       the view's edges with numbers like that, what comes out is wherever the rounding puts it: on the console
       and at 4:3 here, off the screen; with the side planes of a 16:9 view, one corner on the screen and two
       behind the eye, a flat light-blue triangle across the picture for as long as the ring lives (found by the
       user going in and out of the water at Kame House). A ring ten times the size of a map has nothing left on
       the screen that is the ring: it is not drawn. */
    if (!(size < 100000.0f)) {
        return;
    }
#endif
    Mtx_StoreIdentity(&m);
    if (rot != 0.0f) {
        Mtx_RotateY(&m, &m, EftMath_WrapAngle(rot * 3.14159265f / 180.0f));
    }
    half = size * 0.5f;
    Vec4_Set(&corner[0], -half, 0.0f, -half, 1.0f);
    Vec4_Set(&corner[1], half, 0.0f, -half, 1.0f);
    Vec4_Set(&corner[2], -half, 0.0f, half, 1.0f);
    Vec4_Set(&corner[3], half, 0.0f, half, 1.0f);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&corner[i], &m, &corner[i]);
        Vec4_Add(&corner[i], &corner[i], pos);
        corner[i].w = 1.0f;
    }
    Vec4_Set(&uv[0], 0.0f, 0.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[1], 1.0f, 0.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[2], 0.0f, 1.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[3], 1.0f, 1.0f, 1.0f, 0.0f);
    Vec4_Set(&color[0], r, g, b, a);
    Vec4_Set(&color[1], r, g, b, a);
    Vec4_Set(&color[2], r, g, b, a);
    Vec4_Set(&color[3], r, g, b, a);
    ClipVtx_SetArray(poly, &corner[0], &uv[0], &color[0], 3);
    EftWater_DrawClippedFan(poly, layer, *tex);
    ClipVtx_SetArray(poly, &corner[1], &uv[1], &color[1], 3);
    EftWater_DrawClippedFan(poly, layer, *tex);
}

/* Queues a textured quad that lies along `orient`'s z axis from size * near to size * far, size * width wide,
   rolled about its middle by `roll` degrees. The alpha test reference makes it dissolve. */
void EftWater_DrawSprayQuad(EftWaterVec *pos, EftWaterMtx *orient, EftWaterMtx *world2screen, f32 roll, f32 size,
                            f32 near, f32 far, f32 width, u8 r, u8 g, u8 b, u8 a, u8 alphaRef, u64 *tex,
                            u8 layer) {
    EftWaterVec corner[4];
    EftWaterVec stq[4];
    EftWaterVec uv[4];
    EftWaterIVec scr[4];
    EftWaterMtx rollM;
    EftWaterQuadPkt *p;
    EftWaterVec *c;
    f32 *zp;
    f32 zNear;
    f32 half;
    f32 mid;
    s32 z;
    s32 i;

    half = size * 0.5f;
    zNear = size * near;
    size = size * far; /* the far edge from here on */
    half *= width;
    Vec4_Set(&corner[0], -half, 0.0f, zNear, 1.0f);
    Vec4_Set(&corner[1], half, 0.0f, zNear, 1.0f);
    Vec4_Set(&corner[2], -half, 0.0f, size, 1.0f);
    Vec4_Set(&corner[3], half, 0.0f, size, 1.0f);
    if (roll != 0.0f) {
        mid = (corner[0].z - corner[2].z) * 0.5f;
        Mtx_StoreIdentity(&rollM);
        Mtx_RotateY(&rollM, &rollM, EftMath_WrapAngle(roll * 3.14159265f / 180.0f));
        /* A backward goto, not a `for`: every counted loop form of this body is reversed by the compiler (li 3 /
           addiu -1 / bgez), while the original counts up with slti and keeps the two pointers apart. */
        zp = &corner[0].z;
        c = corner;
        i = 0;
    roll_next:
        *zp -= mid;
        Mtx_MulVec4(c, &rollM, c);
        *zp += mid;
        c++;
        zp += 4;
        i++;
        if (i < 4) {
            goto roll_next;
        }
    }
    Vec4_Set(&uv[0], 0.0f, 0.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[1], 1.0f, 0.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[2], 0.0f, 1.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[3], 1.0f, 1.0f, 1.0f, 0.0f);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&corner[i], orient, &corner[i]);
        Vec4_Add(&corner[i], &corner[i], pos);
        corner[i].w = 1.0f;
    }
    Mtx_ProjectPointsStq(scr, stq, world2screen, corner, uv, 4);
    for (i = 0; i < 4; i++) {
        if (!EftWater_IsOnScreen(scr[i], 4)) {
            return;
        }
    }
    z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
    if (EftUtil_IsCamUnderWater()) {
        z -= 500;
    } else {
        z += 500;
    }
    EFT_WATER_QUEUE_TEST(((u64)alphaRef << 4) | 0x5000D, z, layer);
    p = (EftWaterQuadPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    if (p == NULL) {
        return;
    }
    p->prim = 0x5C;
    p->h.tag = 0x20000008;
    p->h.vif0 = 0x10000000;
    p->h.gif0 = 0xE400000000008001;
    p->h.gif1 = 0x42142142142160;
    p->h.next = 0;
    p->h.vif1 = 0x50000008;
    p->v[0].rgba[0] = r;
    p->v[0].rgba[1] = g;
    p->v[0].rgba[2] = b;
    p->v[0].rgba[3] = a;
    p->v[0].q = stq[0].z;
    p->v[1].rgba[0] = r;
    p->v[1].rgba[1] = g;
    p->v[1].rgba[2] = b;
    p->v[1].rgba[3] = a;
    p->v[1].q = stq[1].z;
    p->v[2].rgba[0] = r;
    p->v[2].rgba[1] = g;
    p->v[2].rgba[2] = b;
    p->v[2].rgba[3] = a;
    p->v[2].q = stq[2].z;
    p->v[3].rgba[0] = r;
    p->v[3].rgba[1] = g;
    p->v[3].rgba[2] = b;
    p->v[3].rgba[3] = a;
    p->v[3].q = stq[3].z;
    p->v[0].s = stq[0].x;
    p->v[0].t = stq[0].y;
    p->v[1].s = stq[1].x;
    p->v[1].t = stq[1].y;
    p->v[2].s = stq[2].x;
    p->v[2].t = stq[2].y;
    p->v[3].s = stq[3].x;
    p->v[3].t = stq[3].y;
    p->v[0].xyz.x = scr[0].x;
    p->v[0].xyz.y = scr[0].y;
    p->v[0].xyz.z = scr[0].z;
    p->v[0].xyz.f = 0xFF;
    p->v[1].xyz.x = scr[1].x;
    p->v[1].xyz.y = scr[1].y;
    p->v[1].xyz.z = scr[1].z;
    p->v[1].xyz.f = 0xFF;
    p->v[2].xyz.x = scr[2].x;
    p->v[2].xyz.y = scr[2].y;
    p->v[2].xyz.z = scr[2].z;
    p->v[2].xyz.f = 0xFF;
    p->v[3].xyz.x = scr[3].x;
    p->v[3].xyz.y = scr[3].y;
    p->v[3].xyz.z = scr[3].z;
    p->v[3].xyz.f = 0xFF;
    p->tex0 = *tex;
    EftWaterOt_Add((OtPrim *)p, z, layer);
    EFT_WATER_QUEUE_TEST(0x50000, z, layer);
}

/* Refreshes the TEX0 of the module's five textures (only while particles exist). */
void EftWater_UpdateTextures(s32 a0, s32 a1) {
    s32 i;

    if (gEftDust != NULL && gEftDust->dropCount + gEftDust->ringCount + gEftDust->sprayCount != 0) {
        for (i = 0; i < 5; i++) {
            gEftDust->tex[i].tex0 = EftVram_AddTex(&gEftDust->tex[i], a0, a1);
        }
    }
}

/* Clips a triangle (three EftWaterClipVtx) against the five planes of the view, projects what is left and queues
   it as a fan of textured triangles. A triangle whose three corners are all unusable is dropped. */
#if 0 /* NON-MATCHING: 429 instructions against 426 */
/* A register-masked structural diff (build/scratch_cleanup3_V/sdiff.py) leaves 23 lines (142 before round 4). */
/* Round 4 (scratch build/scratch_cleanup4_W2/cf*.c). The function is the water twin of EftMesh_DrawTriClip +
   EftMesh_QueueTri (eft_ad_b.c, matched): rewritten after them it has the original's blocks, frame (0x1B0), stack
   slots (poly / layer / tex0 at sp+288 / 292 / 296, the three sq / lq saves around EftUtil_IsCamUnderWater) and
   every instruction of the depth, cap, clip-test and packet code. What made the difference:
   - the triangle writer is a `static inline` taking the nine corner pointers (EftWater_QueueTri), header stores
     in the order prim, tag, vif0, vif1, gif0, gif1, next, pad;
   - scr is an array of 4-ALIGNED structs indexed at every use (`scr[i - 1].z`): that gives the original's
     `sll s2,s5,4 / addiu a0,sp,8 / addiu v1,s2,-16 / addu s0,a0,v1 / addu s1,a0,s2` exactly (the 8-aligned
     EftWaterIVec and the `zp` pointer form of the mesh twin both give walking pointers here);
   - the screen test is `clipped = 1; if (z > 0 && x <= 0xFFEF && x > 0) { if (y <= 0xFFEF) clipped = y <= 0; }`
     through a POINTER parameter (movz on the two y tests; the by-value form of EftMesh_IsOffScreen copies the
     16 bytes here and costs 26 instructions).
   What still differs (all one cause, the loop pass's choice of induction pointers):
   - the original does not strength-reduce &scr[i - 1]: it recomputes it where it is used
     (`addu v1,sp,s2 / addiu v1,v1,-16` for the clip test, `addiu t4,t2,-16` for the writer) and keeps only
     &stq[i - 1] (t6, +16) and &poly[i] (s6, +48) as walking pointers; here &scr[i - 1] becomes a third walking
     pointer (s8). With one saved register gone, n lands in t6 (saved / restored with sq / lq around
     ClipPoly_ProjectCur) instead of s8, and the XYZF2 mask 0xFF000000FFFFFFFF is rebuilt in the loop instead of
     being hoisted into t7. `&scr[i] - 1` and `scr + i - 1` compile to the same thing.
   - to try next: the giv is "not worth while" for the loop pass when lifetime * threshold * benefit < the loop's
     insn count (-dL, `giv at N combined with ...`): look for a form in which the clip test's and the writer's
     uses of &scr[i - 1] do not combine into one giv (in the original the two addresses are computed in
     different blocks from sp + (i << 4)). */
typedef struct EftWaterScr4 {
    s32 x, y, z, w;
} EftWaterScr4;

/* 1 when a projected vertex cannot be drawn (behind the camera or outside the GS coordinate range). */
static inline s32 EftWater_IsClipped(EftWaterScr4 *v) {
    s32 clipped = 1;

    if (v->z > 0 && v->x <= 0xFFEF && v->x > 0) {
        if (v->y <= 0xFFEF) {
            clipped = v->y <= 0;
        }
    }
    return clipped;
}

static inline void EftWater_QueueTri(EftWaterScr4 *p0, EftWaterScr4 *p1, EftWaterScr4 *p2, EftWaterVec *c0,
                                     EftWaterVec *c1, EftWaterVec *c2, EftWaterVec *t0, EftWaterVec *t1,
                                     EftWaterVec *t2, s32 layer, s32 z, u64 tex0) {
    EftWaterTriPkt *p = (EftWaterTriPkt *)gOtCur;

    gOtCur = (u32 *)(p + 1);
    p->prim = 0x5B;
    p->h.tag = 0x20000007;
    p->h.vif0 = 0x10000000;
    p->h.vif1 = 0x50000007;
    p->h.gif0 = 0xC400000000008001;
    p->h.gif1 = 0xF42142142160;
    p->h.next = 0;
    p->pad = 0;
    p->v[0].rgba[0] = (u32)c0->x;
    p->v[0].rgba[1] = (u32)c0->y;
    p->v[0].rgba[2] = (u32)c0->z;
    p->v[0].rgba[3] = (u32)c0->w;
    p->v[0].q = t0->z;
    p->v[1].rgba[0] = (u32)c1->x;
    p->v[1].rgba[1] = (u32)c1->y;
    p->v[1].rgba[2] = (u32)c1->z;
    p->v[1].rgba[3] = (u32)c1->w;
    p->v[1].q = t1->z;
    p->v[2].rgba[0] = (u32)c2->x;
    p->v[2].rgba[1] = (u32)c2->y;
    p->v[2].rgba[2] = (u32)c2->z;
    p->v[2].rgba[3] = (u32)c2->w;
    p->v[2].q = t2->z;
    p->tex0 = tex0;
    p->v[0].s = t0->x;
    p->v[0].t = t0->y;
    p->v[1].s = t1->x;
    p->v[1].t = t1->y;
    p->v[2].s = t2->x;
    p->v[2].t = t2->y;
    p->v[0].xyz.x = p0->x;
    p->v[0].xyz.y = p0->y;
    p->v[0].xyz.z = p0->z;
    p->v[0].xyz.f = 0xFF;
    p->v[1].xyz.x = p1->x;
    p->v[1].xyz.y = p1->y;
    p->v[1].xyz.z = p1->z;
    p->v[1].xyz.f = 0xFF;
    p->v[2].xyz.x = p2->x;
    p->v[2].xyz.y = p2->y;
    p->v[2].xyz.z = p2->z;
    p->v[2].xyz.f = 0xFF;
    EftWaterOt_Add((OtPrim *)p, z, layer);
}

void EftWater_DrawClippedFan(EftWaterClipVtx *poly, s32 layer, u64 tex0) {
#ifdef PORT
    /* The triangle is clipped in place and can grow to eight corners, but callers hand over room for three. On
       the PS2 the extra corners land in the caller's other locals; on PC they reach the return address (the
       crash in EftRibbon_DrawStrip). The work is done on a copy with room. */
    EftWaterClipVtx portPoly[16];
    __builtin_memcpy(portPoly, poly, 3 * sizeof(EftWaterClipVtx));
    poly = portPoly;
#endif
    EftWaterScr4 scr[9];
    EftWaterVec stq[9];
    EftWaterVec *plane;
    s32 n;
    s32 i;
    s32 z;

    plane = EftGfx_GetClipPlanes();
    n = 3;
    for (i = 0; i < 5; i++) {
        n = ClipPoly_ClipPlane(poly, plane, n);
        plane++;
    }
    if (n == 0) {
        return;
    }
    ClipPoly_ProjectCur((EftWaterIVec *)scr, stq, poly, n);
    for (i = 2; i < n; i++) {
        z = ((scr[0].z + scr[i - 1].z + scr[i].z) / 3) >> 8;
        if (EftUtil_IsCamUnderWater()) {
            z -= 500;
        } else {
            z += 500;
        }
        if (scr[0].z > 0xFFFFFF) {
            scr[0].z = 0xFFFFFF;
        }
        if (scr[i - 1].z > 0xFFFFFF) {
            scr[i - 1].z = 0xFFFFFF;
        }
        if (scr[i].z > 0xFFFFFF) {
            scr[i].z = 0xFFFFFF;
        }
        if (EftWater_IsClipped(&scr[0]) && EftWater_IsClipped(&scr[i - 1]) && EftWater_IsClipped(&scr[i])) {
            continue;
        }
        EftWater_QueueTri(&scr[0], &scr[i - 1], &scr[i], &poly[0].color, &poly[i - 1].color, &poly[i].color,
                          &stq[0], &stq[i - 1], &stq[i], layer, z, tex0);
    }
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/eft_e", EftWater_DrawClippedFan);
