#include "common.h"
#include "battle/view_b.h"
#include "sys/common.h"
#include "sys/fade.h"
#include "sys/file.h"
#include "sys/heap.h"
#include "sys/job.h"
#include "sys/math3d.h"
#include "sys/ramp.h"

/*
 * ShenScene, 0x261ED8..0x262FF0: the 3D backdrop of the dragon (wish) screen, sys/late_a.c. Like the character
 * viewer (view_a_d.c) it runs the battle's object, stage, scene and ordering-table modules without a battle:
 * a backdrop (file 0x194 + dragon), the seven balls and the dragon, with scripted cameras that come from the
 * dragon's model file. The summoning is a fixed sequence (ShenScene_StepSeq): balls glowing, a flash, the
 * dragon in front of the fixed camera until the screen asks to leave, a farewell camera, fade out.
 * See battle/view_b.h.
 */

extern void *memset(void *dst, s32 c, u32 n);

/* local views of the battle modules used here */
typedef struct ShenBattleWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags; /* 0x100 = paused */
} ShenBattleWork;

/* gCommonRes->unk20: the scene keeps its backdrop file there (as the character viewer does) */
typedef struct ShenCommon {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ void *stage;
} ShenCommon;

extern ShenBattleWork *Battle_GetWork(void);
extern void Battle_ClearWork(void);
extern s32 BtlRes_Request(s32 fixed, s32 model, s32 anim0, s32 anim1);
extern void *BtlRes_GetSlot(s32 handle);
extern s32 BtlObj_Create(s32 type, void *res, s32 active);
extern u8 *BtlObj_Get(s32 id);
extern void BtlObjAnim_PlayModel(u8 *obj, s32 anim, s32 mode);
extern void BtlObjXf_SetMtx(u8 *obj, Mtx44 *m);
extern void BtlObj_SetUnkB30(u8 *obj, s32 value);
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
extern void StgModel_InitStage(void);
extern void StgModel_Cull(void *view);
extern void StgModel_Draw(void *view);
extern void StgFx_Init(void);
extern void StgFx_Term(void);
extern void StgFx_DrawPreNoCheck(void);
extern void StgFx_DrawNop(void);
extern void StgFx_DrawPost(void);
extern void StgFx_DrawOverlay(void);
extern void StgBlur_SetCenter(s32 view, Vec4 *center, s32 screenSpace);
extern void StgBlur_SetColor0Rgba(s32 view, u8 r, u8 g, u8 b, u8 a);
extern void StgBlur_SetColor1Rgba(s32 view, u8 r, u8 g, u8 b, u8 a);
extern void StgBlur_SetColor2Rgba(s32 view, u8 r, u8 g, u8 b, u8 a);
extern void StgBlur_SetColor3Rgba(s32 view, u8 r, u8 g, u8 b, u8 a);
extern s32 ScrWarp_Spawn(s32 view, Vec4 *pos, f32 seconds, f32 radius, f32 width, f32 speed, f32 jitter);
extern void ScrXfade_RequestCapture(void);
extern void ScrXfade_Start(s32 request, f32 seconds); /* stg_b.h lists (request, seconds); this order matches the callers here */
extern void BtlObjDraw_Draw(void);
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
extern void Mtx_StoreIdentity(Mtx44 *m);
extern s32 Snd_PlaySeEx(u32 mask, s32 id, s32 volume, s32 pan, s32 pitch);
extern void DemoCam_Init(void);
extern void DemoCam_Term(void);
extern void DemoCam_SetAnim(void *anim);
extern s32 DemoCam_Update(void);
extern f32 DemoCam_GetLength(void);
extern f32 DemoCam_GetTime(void);
extern void DemoCam_SetTime(f32 time);
extern s32 DemoCam_IsActive(void);
extern void DemoCam_StartAuto(void);
extern void DemoCam_AddShake(f32 time);
extern f32 DemoCam_GetShakeTime(void);
extern void DemoCam_SetFixedPose(Vec4 *pos, Vec4 *rot);
extern void DemoCam_ClearFixed(void);

extern u8 gBattleProf[];
extern void *gBtlCamView;

/* sys/file.h declares two parameters; the call passes a third (0), which File_Request ignores */
extern void *File_Request3(s32 id, void *buf, s32 unused) __asm__("File_Request");

/* Defined here: this object's .sdata (0x2FF118). */
ShenScene *gShenScene = NULL;

#define SHEN_OBJ_FLAGS(obj) (*(u32 *)((obj) + 0xA40))
#define SHEN_OBJ_CAMS(obj) ((void **)((obj) + 0x908))
#define SHEN_RAMP(actor) ((Ramp *)(actor)->ramp)

/* The sequence, one call per frame; returns 1 when it is over.
     0  first camera; balls and dragon placed at the origin, the balls start to pulse (glow 0..127, 0.5 s)
     1  pulse; from camera time 354 the blur is set up; when the camera animation ends ->
     2  the dragon's animation 1 starts, fixed camera pose of this dragon, shake, a warp ring, a cross-fade,
        state READY
     3  the dragon pulses (0..64, 2 s) and the blur follows the camera shake, until the screen asks to leave
     4  second camera, dragon hidden, balls shown with animation 1 ->
     5  when that camera ends: fade out (1 s)
     6  until the fade is done
   Sounds 0x3D..0x40 of bank mask 2 follow the steps.

   NOT MATCHING: kept as INCLUDE_ASM. The attempt below is behaviourally exact and differs from the original in
   one place only: in step 1 the original converts the constant 0.0f of the third blur layer with the full
   float-to-unsigned sequence on a scratch register ($f0, loaded right there), while this C keeps the constant
   in $f20 (set before the glow level is passed on) and the compiler then folds the >= 2^31 arm; about 30
   instructions around 0x2620D8 are ordered differently because of it, everything else is identical. The
   read-only data the attempt emits (the two blur centres, the camera poses, the jump table) and its two
   .lit4 constants (pi 0x2FE7AC, 3.8f 0x2FE7B0) have the original values and order. */
/* Cleanup 4 (dumps). What the original shows is `lo` NOT living in a saved register: its 0.0 is put in $f0 at
   the conversion and used three times there (compare, trunc, sub), just as `hi`'s 1.0 is loaded next to its
   one use (`hi * 128.0f`; a pseudo set once to a constant and used once has its load moved to the use by
   local-alloc, which is why `hi` already matches). Facts measured:
   - `lo = 0.0f; hi = 1.0f;` must stand in front of the BtlObj_SetUnkB30 call: that call's own float-to-
     unsigned conversion has a branch and a join, and behind the join cse no longer knows the constants. Moved
     behind the call (or into the `if`), cse folds both conversions away (592 instructions).
   - gcse cannot put a constant into the conversion's instructions (compare / fix), so `lo` keeps its three
     uses and gets $f20. Any plain copy of it (`f32 a = lo;`) is found by gcse's constant propagation and
     the conversion is then folded by cse2 (610 instructions); an inline helper with float parameters gives
     the same code as passing `(u32)lo` (cse forwards the parameter copy).
   So the source must make ONE use of the 0.0 variable that is neither a foldable copy nor the conversion
   itself, or leave the pseudo without a hard register (reload then loads the constant once and inherits it).
   Not found. */
#if 0
/* A sound of bank mask 2, queued twice. */
static inline void ShenScene_PlaySe(s32 id, s32 volume) {
    Snd_PlaySeEx(2, id, volume, 0, 0);
    Snd_PlaySeEx(2, id, volume, 0, 0);
}

/* A vector as this file's locals and tables hold it: 16-byte aligned. */
typedef Vec4 ShenVec __attribute__((aligned(16)));

/* The radial blur of the flash: the alpha of its four grey layers. */
static inline void ShenScene_SetBlur(u8 a3, u8 a0, u8 a1, u8 a2) {
    StgBlur_SetColor3Rgba(0, 0x80, 0x80, 0x80, a3);
    StgBlur_SetColor0Rgba(0, 0x80, 0x80, 0x80, a0);
    StgBlur_SetColor1Rgba(0, 0x80, 0x80, 0x80, a1);
    StgBlur_SetColor2Rgba(0, 0x80, 0x80, 0x80, a2);
}

s32 ShenScene_StepSeq(ShenSeq *seq) {
    Mtx44 m;
    ShenVec center;
    f32 lo;
    f32 hi;
    ShenActor *actor;
    Ramp *ramp;
    ShenScene *work;
    f32 shake;
    f32 zero;
    f32 half;
    s32 dragon;
    s32 frame;
    s32 volume;

    switch (seq->step) {
    case 0: {
        ShenActor *dragon;
        ShenActor *balls;
        ShenScene *w;

        Mtx_StoreIdentity(&m);
        ShenScene_NextCam(seq);
        dragon = &gShenScene->actor[SHENSCENE_ACTOR_DRAGON];
        SHEN_OBJ_FLAGS(dragon->obj) &= ~0x02000000;
        BtlObjXf_SetMtx(dragon->obj, &m);
        BtlObjAnim_PlayModel(dragon->obj, 0, 2);
        BtlObj_SetUnkB30(dragon->obj, 0x40);
        w = gShenScene;
        balls = &w->actor[SHENSCENE_ACTOR_BALLS];
        SHEN_OBJ_FLAGS(balls->obj) &= ~0x02000000;
        BtlObjXf_SetMtx(balls->obj, &m);
        BtlObjAnim_PlayModel(balls->obj, 0, 2);
        Ramp_Start(SHEN_RAMP(&w->actor[SHENSCENE_ACTOR_BALLS]), 0.5f, 0.0f, 127.0f);
        BtlObj_SetUnkB30(balls->obj, (u8)(u32)SHEN_RAMP(balls)->value);
        seq->step++;
        break;
    }
    case 1:
        ramp = SHEN_RAMP(&gShenScene->actor[SHENSCENE_ACTOR_BALLS]);
        actor = &gShenScene->actor[SHENSCENE_ACTOR_BALLS];
        if (Ramp_Step(ramp)) {
            if (SHEN_RAMP(actor)->value == 0.0f) {
                Ramp_Start(ramp, 0.5f, 0.0f, 127.0f);
            } else {
                Ramp_Start(ramp, 0.5f, 127.0f, 0.0f);
            }
        }
        lo = 0.0f;
        hi = 1.0f;
        BtlObj_SetUnkB30(actor->obj, (u8)(u32)SHEN_RAMP(actor)->value);
        if (DemoCam_GetTime() >= 354.0f) {
            static const ShenVec c = {0.0f, 0.0f, 1.0f, 1.0f};

            center = c;
            ShenScene_SetBlur(0x40, 0, (u32)lo, (u32)(hi * 128.0f));
            StgBlur_SetCenter(0, &center, 0);
        }
        if (!ShenScene_IsCamEnd()) {
            break;
        }
        seq->step++;
        /* fall through */
    case 2: {
        work = gShenScene;
        zero = 0.0f;
        half = 0.5f;
        BtlObjAnim_PlayModel(work->actor[SHENSCENE_ACTOR_DRAGON].obj, 1, 2);
        Ramp_Start(SHEN_RAMP(&work->actor[SHENSCENE_ACTOR_DRAGON]), 2.0f, 64.0f, zero);
        SHEN_OBJ_FLAGS(gShenScene->actor[SHENSCENE_ACTOR_BALLS].obj) &= ~2;
        {
            ShenVec p[3] = {
                {0.86f, -90.9870f, -19.1110f, 1.0f},
                {-0.891f, -169.7360f, -33.2710f, 1.0f},
                {-0.399f, -2.547f, -10.358f, 1.0f},
            };
            ShenVec r[3] = {
                {10.72f, 0.0f, 0.0f, 1.0f},
                {16.778f, -0.823f, 0.0f, 1.0f},
                {5.142f, 3.569f, 0.0f, 1.0f},
            };

            dragon = gShenScene->dragon;
            r[dragon].x = r[dragon].x * 3.14159265f / 180.0f;
            r[dragon].y = r[dragon].y * 3.14159265f / 180.0f;
            r[dragon].z = r[dragon].z * 3.14159265f / 180.0f;
            DemoCam_SetFixedPose(&p[dragon], &r[dragon]);
            DemoCam_AddShake(1.0f);
            p[dragon].z = zero;
            p[dragon].x = zero;
            ScrWarp_Spawn(0, &p[dragon], 3.8f, 10.0f, 50.0f, 10.0f, half);
        }
        ScrXfade_RequestCapture();
        ScrXfade_Start(1, half);
        ShenScene_SetState(SHENSCENE_STATE_READY);
        seq->step++;
    }
        /* fall through */
    case 3:
        ramp = SHEN_RAMP(&gShenScene->actor[SHENSCENE_ACTOR_DRAGON]);
        actor = &gShenScene->actor[SHENSCENE_ACTOR_DRAGON];
        if (Ramp_Step(ramp)) {
            if (SHEN_RAMP(actor)->value == 0.0f) {
                Ramp_Start(ramp, 2.0f, 0.0f, 64.0f);
            } else {
                Ramp_Start(ramp, 2.0f, 64.0f, 0.0f);
            }
        }
        BtlObj_SetUnkB30(actor->obj, (u8)(u32)SHEN_RAMP(actor)->value);
        {
            static const ShenVec c = {0.0f, -0.3f, 0.55f, 1.0f};

            center = c;
            shake = DemoCam_GetShakeTime();
            ShenScene_SetBlur((u32)(shake * 96.0f), (u32)(shake * 0.0f), (u32)(shake * 52.0f), (u32)(shake * 128.0f));
            StgBlur_SetCenter(0, &center, 0);
        }
        if (ShenScene_GetState() != SHENSCENE_STATE_LEAVE) {
            break;
        }
        ShenScene_SetState(SHENSCENE_STATE_INTRO);
        ScrXfade_RequestCapture();
        ScrXfade_Start(1, 1.0f);
        seq->step++;
        break;
    case 4:
        DemoCam_ClearFixed();
        ShenScene_NextCam(seq);
        SHEN_OBJ_FLAGS(gShenScene->actor[SHENSCENE_ACTOR_DRAGON].obj) &= ~2;
        {
            ShenActor *balls = &gShenScene->actor[SHENSCENE_ACTOR_BALLS];

            SHEN_OBJ_FLAGS(balls->obj) |= 2;
            BtlObjAnim_PlayModel(balls->obj, 1, 1);
            BtlObj_SetUnkB30(balls->obj, 0x7F);
        }
        seq->step++;
        /* fall through */
    case 5:
        if (ShenScene_IsCamEnd()) {
            Fade_Start(0, 0, 1.0f);
            seq->step++;
        }
        break;
    case 6:
        if (Fade_IsDone(0)) {
            return 1;
        }
        break;
    default:
        return 1;
    }
    frame = gShenScene->frame;
    switch (seq->seStep) {
    case 0:
        if (!((u32)seq->step < 2)) {
            break;
        }
        if (DemoCam_GetTime() < 354.0f) {
            f32 level = 1.0f - DemoCam_GetTime() / 420.0f;

            if (frame % 27 == 0) {
                volume = level * 127.0f;
                ShenScene_PlaySe(0x3D, volume);
            }
        } else {
            ShenScene_PlaySe(0x3E, 0x7F);
            seq->seStep++;
        }
        break;
    case 1:
        if (seq->step == 4) {
            ShenScene_PlaySe(0x3F, 0x7F);
            seq->seStep++;
        }
        break;
    case 2:
        if (DemoCam_GetTime() >= 322.0f) {
            ShenScene_PlaySe(0x40, 0x7F);
            seq->seStep++;
        }
        break;
    }
    return 0;
}
#else
RODATA_ALIGN16(); /* the object has a jump table: its .rodata is 16-byte aligned (0x2F33D0) */
INCLUDE_RODATA("asm/nonmatchings/battle/view_b_d", D_002F33D0);
INCLUDE_RODATA("asm/nonmatchings/battle/view_b_d", D_002F33E0);
INCLUDE_RODATA("asm/nonmatchings/battle/view_b_d", D_002F3410);
INCLUDE_RODATA("asm/nonmatchings/battle/view_b_d", D_002F3440);
INCLUDE_ASM("asm/nonmatchings/battle/view_b_d", ShenScene_StepSeq);
#endif

/* Load job: requests the backdrop and the two models of this dragon, then creates their objects. */
s32 ShenScene_StepLoad(ShenJob *job) {
    ShenCommon *res = (ShenCommon *)gCommonRes->unk20;
    ShenActor *actor;
    s32 i;
    void *slot;
    ShenScene *work;

    switch (job->state) {
    case 0: {
        s32 files[3][SHENSCENE_ACTOR_COUNT] = {{0xD3C, 0xD46}, {0xD3D, 0xD46}, {0xD45, 0xD47}};

        res->stage = File_Request3(gShenScene->dragon + SHENSCENE_FILE_STAGE, NULL, 0);
        for (i = 0; i < SHENSCENE_ACTOR_COUNT; i++) {
            ShenActor *a = &gShenScene->actor[i];

            a->id = BtlRes_Request(0, files[gShenScene->dragon][i], -1, -1);
        }
        job->state++;
        break;
    }
    case 1:
        if (File_UpdateRequests() == 0) {
            return 0;
        }
        for (i = 0; i < SHENSCENE_ACTOR_COUNT; i++) {
            actor = &gShenScene->actor[i];
            slot = BtlRes_GetSlot(actor->id);
            if (i == 0) {
                actor->id = BtlObj_Create(1, slot, 1);
            } else {
                actor->id = BtlObj_Create(3, slot, 1);
            }
            actor->obj = BtlObj_Get(actor->id);
        }
        return 1;
    default:
        return 1;
    }
    return 0;
}

/* Takes the two camera animations from the dragon's object and makes the first one the next to play. */
void ShenScene_InitCams(ShenSeq *seq) {
    ShenActor *actor = &gShenScene->actor[SHENSCENE_ACTOR_DRAGON];
    void **cams = seq->cams;
    s32 i;

    for (i = 0; i < 2; i++) {
        seq->cams[i] = SHEN_OBJ_CAMS(actor->obj)[i];
    }
    seq->camIdx = 0;
    seq->cam = *cams;
}

/* Starts the next camera animation from its beginning. Returns 0 when there is none left. */
s32 ShenScene_NextCam(ShenSeq *seq) {
    if (seq->cam != NULL) {
        DemoCam_SetAnim(seq->cam);
        DemoCam_SetTime(0.0f);
        DemoCam_StartAuto();
        seq->camIdx++;
        if (seq->camIdx >= 2) {
            seq->cam = NULL;
        } else {
            seq->cam = seq->cams[seq->camIdx];
        }
        return 1;
    }
    return 0;
}

/* Whether the camera animation has reached its end. */
s32 ShenScene_IsCamEnd(void) {
    f32 time = DemoCam_GetTime();

    if (time >= DemoCam_GetLength()) {
        return 1;
    }
    return 0;
}

/* Runs the sequence for a frame. Returns 1 when it has ended (and on every call after that). */
s32 ShenScene_RunSeq(void) {
    ShenSeq *seq = &gShenScene->seq;

    if (seq->fn != NULL) {
        if (seq->fn(seq) == 0) {
            return 0;
        }
        seq->fn = NULL;
    }
    return 1;
}

/* Starts the sequence from its first step. */
void ShenScene_StartSeq(void) {
    ShenSeq *seq = &gShenScene->seq;

    memset(seq, 0, sizeof(ShenSeq));
    seq->fn = ShenScene_StepSeq;
    ShenScene_InitCams(seq);
}

/* Empty; called first by ShenScene_Term. */
void ShenScene_Nop(void) {
}

/* SHENSCENE_STATE_* */
s32 ShenScene_GetState(void) {
    return gShenScene->seq.state;
}

/* Sets the state; LEAVE is only accepted while the scene is READY. */
void ShenScene_SetState(s32 state) {
    ShenSeq *seq = &gShenScene->seq;

    if (state == SHENSCENE_STATE_LEAVE && ShenScene_GetState() != SHENSCENE_STATE_READY) {
        return;
    }
    seq->state = state;
}

/* Empty, no caller. */
void ShenScene_Nop2(void) {
}

/* One frame of the scene: the sequence and the camera, the objects, then everything is drawn. */
s32 ShenScene_Update(void) {
    void *view;

    if (!(Battle_GetWork()->flags & 0x100)) {
        if (ShenScene_RunSeq()) {
            ShenScene_SetState(SHENSCENE_STATE_ENDED);
        }
    }
    if (DemoCam_IsActive()) {
        DemoCam_Update();
    }
    if (!(Battle_GetWork()->flags & 0x100)) {
        Dbg_ProfMark(gBattleProf);
        BtlObj_UpdateAll();
        BtlObj_UpdateVisibility(0);
        BtlObj_FinishVisibility(0);
        BtlStage_Update();
        BtlScene_Update();
        gShenScene->time += 2.0f;
        gShenScene->frame++;
        Dbg_ProfColor(gBattleProf, 0x80FFFFFF);
    }
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
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(3);
    StgFx_DrawNop();
    Dbg_ProfColor(gBattleProf, 0x80FFFFFF);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(4);
    Gfx_AddDefaultEnv();
    BtlScene_Draw(1);
    Ot_Draw();
    Dbg_ProfColor(gBattleProf, 0x804040FF);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(3);
    StgFx_DrawPost();
    Dbg_ProfColor(gBattleProf, 0x80FFFFFF);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(3);
    StgFx_DrawOverlay();
    Dbg_ProfColor(gBattleProf, 0x80FFFFFF);
    Gfx_MarkPass(0);
    return 0;
}

/* Starts the scene for a dragon (0..2): loads the backdrop and the models (blocking), sets up the battle
   modules it uses, starts the sequence and a fade in. */
void ShenScene_Init(s32 dragon) {
    ShenJob *job;

    Battle_GetWork()->flags &= ~0x100;
    gShenScene = Heap_Alloc(sizeof(ShenScene), 0x20, 0, HEAP_ANY);
    memset(gShenScene, 0, sizeof(ShenScene));
    gShenScene->dragon = dragon;
    BtlObj_Init(0);
    BtlObj_SetUnk44FC4(2.0f);
    Job_Clear();
    job = &gShenScene->job;
    memset(job, 0, sizeof(ShenJob));
    job->step = ShenScene_StepLoad;
    Job_Push((Job *)job);
    Load_RunBlocking();
    Battle_ClearWork();
    DemoCam_Init();
    StgModel_InitStage();
    StgFx_Init();
    Ot_Init();
    BtlScene_Init(8);
    ShenScene_StartSeq();
    Fade_Start(0, 1, 1.0f);
}

/* Ends the scene and frees everything ShenScene_Init set up, the backdrop file included. */
void ShenScene_Term(void) {
    ShenScene_Nop();
    Heap_Free(gShenScene);
    File_CancelRequests();
    Job_Clear();
    BtlObj_Term();
    DemoCam_Term();
    StgFx_Term();
    BtlStage_Term();
    BtlScene_Term();
    Ot_Term();
    Dma_ResetBuffers();
    Adx_StopAll();
    Heap_Free(((ShenCommon *)gCommonRes->unk20)->stage);
    Fade_ResetAll();
}
