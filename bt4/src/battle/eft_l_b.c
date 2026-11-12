#include "common.h"
#include "battle/eft_l.h"

/*
 * Technique effect type 4, the "ring shot", 0x15C728..0x15E5D0: gEftShotClass row 5 (manager 0x2C39B8, task
 * 0x2C39D0). One task creates up to 20 blast objects (EftBlastObj_Create, the tasks of class 0x2C3AD8 that carry the
 * hits) and places them until they are launched. Variant by effect id (EftRingShot.type):
 *   0  id 0x26D: on the owner's event 4 the shots are created at the set's start node and slide to places on two
 *      rings around the OPPONENT (radius 70 and 35 times the body scale, 35 * 2 and 35 * 0.7 above and below),
 *      bob there, and once all have arrived the fighter gets held flag 0xA8; the owner's event 0x40 launches them
 *      at the opponent's node 0x11, shot n after (20 - n) * 0.75;
 *   1  id 0x18D: the same with rings around the OWNER (radius 30 and 15, heights 15 * 2 and 15);
 *   2  ids 0x156, 0x1DC, 0x2EC: a carrier point flies a three-point spline towards a point above the opponent in
 *      20 frames, carrying a hit volume; then (held flag 0xA8) the owner's event 0x10 fires a volley of
 *      def->count shots from it at the opponent, two of three with a random cone spread, 3 apart in delay;
 *   3  ids 0x197, 0x198, 0x200, 0x2A4: no shots, only a hit volume at the opponent's node 3.
 * Simulation: yes. It creates, places and aims the blast objects, adds hit records (variants 2 and 3), sets the
 * fighter's held flags 0xA8 / 0xA9 and restarts the technique event timer. BtlScene_RandF decides each shot's bob
 * phase (variants 0, 1: it moves where the shot is when launched) and the volley's spread (variant 2).
 */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 sinf(f32 x);
extern f32 sqrtf(f32 x);
extern f32 asinf(f32 x);
extern f32 atan2f(f32 y, f32 x);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Vec3_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi);
extern void Vec3_ScaleAdd(Vec4 *dst, Vec4 *a, Vec4 *b, f32 t);

extern s32 BtlScene_IsCharStopped(s32 objId);
extern f32 BtlScene_GetCharScale(s32 objId);
extern f32 BtlScene_RandF(void);
extern void BtlTask_CreateChildList(EftTask *task, s32 count, s32 workSize);

extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);

extern void *EftHit_GetNew(void);
extern void EftHit_Add(void *rec);
extern void *EftHitArena_AllocSphere(void);
extern void *EftHitArena_AllocBox(void);
extern void EftHit_SetShapeSpheres(void *rec, void *a, void *b);
extern void EftHit_SetShapeBoxes(void *rec, void *a, void *b);
extern f32 EftMath_WrapAngle(f32 angle);
extern void EftMath_Spline3(Vec4 *dst, Vec4 *path, f32 t);
extern void EftAim_GetDir(void *dst, void *src, s32 objId);
extern void EftAim_GetDirKeep(EftTechArg *arg, Vec4 *dir, Vec4 *pos, s32 objId);
extern s32 EftShot_TestBits(s32 objId, s32 mask);
extern void EftShot_SetHeldFlagA8(s32 objId);
extern void EftShot_SetHeldFlagA9(s32 objId);
extern void EftShot_Nop(s32 size);
extern void EftEmit_LoadSet(EftTechArg *arg, void *model, s32 a2, s32 *pack, s32 a4, s32 a5);
extern void EftEmit_FreeSet(void *model);
extern void EftEmit_BeginFrame(void *model);
extern s32 EftEmit_GetEndFrames(EftModel *model);
extern s32 EftEmit_GetPhaseMask(EftModel *model);
extern s32 EftEmit_IsPartDeferred(EftModel *model, s32 kind, s32 sub);
extern void EftEmit_InitState(void *model, EftModelInst *inst);
extern void EftEmit_TermState(EftModel *model, EftModelInst *inst);
extern s32 EftEmit_GetFlagsFromReq(EftModel *model, EftModelInst *inst, s32 objId, s32 part, s32 sub, s32 end, s32 fast);
extern s32 EftEmit_GetResetFlags(EftModel *model, EftModelInst *inst, s32 part, s32 sub);
extern void EftEmit_SpawnOwn(EftModel *model, EftModelInst *inst, void *nodes, Vec4 *pos, Vec4 *dir, s32 part, s32 sub,
                             s32 flags, f32 size);
extern void EftEmit_KillAll(EftModel *model, EftModelInst *inst);
extern s32 EftEmit_UpdateAlive(EftModel *model, EftModelInst *inst);
extern void EftEmit_UpdateNodesReq(EftTechArg *arg, void *nodes);
extern void EftEmit_UpdateTrailWidth(EftModel *model, EftModelInst *inst);
extern f32 EftEmit_GetTrailWidth(EftModelInst *inst);
extern void EftTechEvt_RequestRestart(s32 objId);
extern void *EftBlastObj_Create(EftShotArg *arg);
extern s32 EftBlastObj_IsAlive(void *shot);
extern void EftBlastObj_SetTarget(void *shot, s32 mode, Vec4 *pos);
extern void EftBlastObj_SetPrevPos(void *shot, Vec4 *pos);
extern void EftBlastObj_SetDir(void *shot, Vec4 *dir);
extern void EftBlastObj_SetHeld(void *shot, s32 hold);
extern void EftBlastObj_MarkLast(void *shot);
extern void EftBlastObj_SetDelay(void *shot, f32 delay);
extern void BtlTask_SetDead(EftTask *task);
extern void BtlTask_SetOwnerTag(EftTask *task, s32 flag);
extern EftTask *BtlTask_GetParent(EftTask *task);
extern void ColCapsule_Set(void *shape, Vec4 *from, Vec4 *to, f32 radius);
extern void ColSphere_Set(void *shape, Vec4 *pos, f32 radius);

/* The part of a hit record (0x190 bytes, EftHit_GetNew) this module fills. */
typedef struct EftRingShotPath {
    EftVec start;
    EftVec pos;
    EftVec target;
    EftVec vel;
} EftRingShotPath;

typedef struct EftHitRec {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ u64 path[8];    /* start, pos, target, vel */
    /* 0x50 */ u32 flags;      /* 0x80: last hit of the technique */
    /* 0x54 */ u8 unk54[0xC];
    /* 0x60 */ EftTask *task;  /* source task: gets the results in task->flags / hit / hitPos */
    /* 0x64 */ EftTechArg *arg;
} EftHitRec;

/* Returns a cleared free entry of the shot table, NULL when all 20 are in use. */
EftRingShotOne *EftRingShot_AllocShot(EftRingShot *w) {
    EftRingShotOne *shot = w->shot;
    EftRingShotOne *found = NULL;
    s32 i = 0;

    while (i < 20) {
        i++;
        if (shot->shot == NULL) {
            found = shot;
            memset(found, 0, sizeof(EftRingShotOne));
            break;
        }
        shot++;
    }
    return found;
}

/* Creates the shots of one volley (one shot, or the definition's count when `volley` is set): takes a free
 * entry, works out where the shot waits and which way it will fly, and creates its blast object. */
void EftRingShot_Fire(s32 objId, EftTask *task, s32 node, s32 arg3, s32 volley) {
    EftShotArg sarg;
    EftRingShot *w = task->work;
    EftTechArg *arg = w->arg;
    EftTechDef *def = arg->def;
    s32 count = 1;
    s32 i;
    f32 t;      /* delay, then a limit, then an angle */
    f32 h;      /* ring height factor; spread and yaw for the volley */
    f32 scale;
    f32 r;      /* ring radius factor (variant 1); 10.0 in variant 0 */
    f32 r0;     /* ring radius factor (variant 0) */

    if (volley) {
        count = def->count;
        if (count <= 0) {
            count = 1;
        }
    }
    for (i = 0; i < count; i++) {
        EftRingShotOne *shot = EftRingShot_AllocShot(w);

        if (shot != NULL) {
            t = (f32)arg->def->unk28 / 30.0f;
            sarg = (EftShotArg){
                arg, w->model, w->unk2D0, &shot->pos, &shot->dir, w->fired, w->total, node, arg3, 0, w->drawSize, w->speed
            };
            shot->index = w->fired;
            Vec4_Copy(&shot->dir, &w->dir);
            switch (w->type) {
                case 0:
                case 1:
                    BtlCharApi_GetNodePos(objId, w->node, &shot->pos);
                    if (w->fired == 0) {
                        shot->flags |= 4;
                    }
                    break;
                case 2:
                    sarg.delay = t;
                    Vec4_Copy(&shot->pos, &w->pos);
                    if (!(i + 1 < count)) {
                        shot->flags |= 4;
                    }
                    break;
            }
            switch (w->type) {
                case 0: {
                    Vec4 center;
                    Vec4 v;
                    Vec4 d;
                    Mtx44 m;
                    s32 opp;
                    s32 half;
                    s32 inner;
                    s32 outer;
                    Vec4 *home;
                    Vec4 *from;

                    t = 0.7f;
                    h = 1.0f;
                    opp = BtlCharApi_GetOpponentObjId(arg->objId);
                    half = w->total / 2;
                    r0 = h;
                    inner = (f32)half * 0.4f;

                    if (inner <= 0) {
                        inner = 1;
                    }
                    BtlCharApi_GetNodePos(opp, 0x11, &center);
                    scale = BtlScene_GetCharScale(opp);
                    outer = half - inner;
                    if (scale < t) {
                        scale = t;
                    }
                    Vec4_Set(&v, 0.0f, 0.0f, -1.0f, h);
                    Mtx_StoreIdentity(&m);
                    if (w->fired < half) {
                        if (w->fired < inner) {
                            h = 2.0f;
                            r0 = 0.5f;
                            t = (f32)w->fired / (f32)inner * 6.2831853f;
                        } else {
                            h = t;
                            t = (f32)(w->fired - inner) / (f32)outer * 6.2831853f;
                        }
                    } else {
                        if (w->fired - half < inner) {
                            h = -2.0f;
                            r0 = 0.5f;
                            t = (f32)(w->fired - half + 1) / (f32)inner * 6.2831853f;
                        } else {
                            h = -0.7f;
                            t = (f32)(w->fired - (half + inner) + 1) / (f32)outer * 6.2831853f;
                        }
                    }
                    home = &shot->home;
                    Mtx_RotateY(&m, &m, EftMath_WrapAngle(t));
                    Mtx_MulVec4(&v, &m, &v);
                    r = 10.0f;
                    from = &shot->from;
                    Vec3_Sub(&d, &center, &w->start);
                    t = atan2f(d.x, d.z);
                    Mtx_StoreIdentity(&m);
                    Mtx_RotateY(&m, &m, t);
                    Mtx_MulVec4(&v, &m, &v);
                    v.x *= scale * 70.0f * r0;
                    v.y -= scale * 35.0f * h;
                    v.z *= scale * 70.0f * r0;
                    Vec3_Add(home, &center, &v);
                    Vec4_Copy(&v, home);
                    shot->phase = BtlScene_RandF() * 3.14159265f;
                    v.y += sinf(shot->phase) * r;
                    Vec3_Sub(from, &v, &w->start);
                    shot->slideTime = sqrtf(Vec3_Dot(from, from)) / def->speed;
                    if (shot->slideTime < r) {
                        shot->slideTime = r;
                    }
                    shot->time = 0.0f;
                    break;
                }
                case 1: {
                    Mtx44 m;
                    Vec4 center;
                    Vec4 v;
                    Vec4 d;
                    s32 half = w->total / 2;
                    s32 inner;
                    s32 outer;

                    h = 1.0f;
                    r = h;
                    inner = (f32)half * 0.4f;

                    if (inner <= 0) {
                        inner = 1;
                    }
                    BtlCharApi_GetNodePos(arg->objId, 0x11, &center);
                    outer = half - inner;
                    scale = BtlScene_GetCharScale(arg->objId);
                    if (scale < 0.7f) {
                        scale = 0.7f;
                    }
                    Vec4_Set(&v, 0.0f, 0.0f, -1.0f, h);
                    Mtx_StoreIdentity(&m);
                    if (w->fired < half) {
                        if (w->fired < inner) {
                            h = 2.0f;
                            r = 0.0f;
                            t = (f32)w->fired / (f32)inner * 6.2831853f;
                            if (inner >= 2) {
                                r = 0.5f;
                            }
                        } else {
                            t = (f32)(w->fired - inner) / (f32)outer * 6.2831853f;
                        }
                    } else {
                        if (w->fired - half < inner) {
                            h = -2.0f;
                            r = 0.0f;
                            t = (f32)(w->fired - half + 1) / (f32)inner * 6.2831853f;
                            if (inner >= 2) {
                                r = 0.5f;
                            }
                        } else {
                            h = -1.0f;
                            t = (f32)(w->fired - (half + inner) + 1) / (f32)outer * 6.2831853f;
                        }
                    }
                    Mtx_RotateY(&m, &m, EftMath_WrapAngle(t));
                    Mtx_MulVec4(&v, &m, &v);
                    Vec3_Sub(&d, &center, &w->start);
                    t = atan2f(d.x, d.z);
                    Mtx_StoreIdentity(&m);
                    Mtx_RotateY(&m, &m, t);
                    Mtx_MulVec4(&v, &m, &v);
                    v.x *= scale * 30.0f * r;
                    v.y -= scale * 15.0f * h;
                    v.z *= scale * 30.0f * r;
                    Vec3_Add(&shot->home, &center, &v);
                    Vec4_Copy(&v, &shot->home);
                    shot->phase = BtlScene_RandF() * 3.14159265f;
                    v.y += sinf(shot->phase) * 10.0f;
                    Vec3_Sub(&shot->from, &v, &w->start);
                    shot->slideTime = sqrtf(Vec3_Dot(&shot->from, &shot->from)) / def->speed;
                    if (shot->slideTime < 1.0f) {
                        shot->slideTime = 1.0f;
                    }
                    shot->time = 0.0f;
                    break;
                }
                case 2: {
                    Mtx44 m;
                    Vec4 v;
                    s32 three;

                    h = BtlScene_RandF() * 0.35f + 0.15f;
                    BtlCharApi_GetNodePos(BtlCharApi_GetOpponentObjId(arg->objId), 0x11, &shot->dir);
                    Vec3_Sub(&shot->dir, &shot->dir, &shot->pos);
                    Vec3_Normalize(&shot->dir, &shot->dir);
                    if (i + 1 < count) {
                        three = 3;
                        if (i % three != 0) {
                            t = EftMath_WrapAngle((f32)i / (f32)count * 6.2831853f);
                            Vec4_Set(&v, 0.0f, h, 1.0f - h, 1.0f);
                            Mtx_StoreIdentity(&m);
                            Mtx_RotateZ(&m, &m, t);
                            Mtx_MulVec4(&v, &m, &v);
                            Vec3_Clamp(&shot->dir, &shot->dir, -1.0f, 1.0f);
                            t = asinf(-shot->dir.y);
                            h = atan2f(shot->dir.x, shot->dir.z);
                            Mtx_StoreIdentity(&m);
                            Mtx_RotateX(&m, &m, t);
                            Mtx_RotateY(&m, &m, h);
                            Mtx_MulVec4(&shot->dir, &m, &v);
                            Vec3_Normalize(&shot->dir, &shot->dir);
                        }
                    }
                    break;
                }
            }
            shot->shot = EftBlastObj_Create(&sarg);
            shot->flags |= 1;
            switch (w->type) {
                case 0:
                case 1:
                    EftBlastObj_SetHeld(shot->shot, 1);
                    break;
                case 2:
                    EftBlastObj_SetDelay(shot->shot, (f32)i * 3.0f);
                    EftBlastObj_SetPrevPos(shot->shot, &shot->pos);
                    break;
            }
            if (shot->flags & 4) {
                EftBlastObj_MarkLast(shot->shot);
            }
            w->flags |= EFT_RINGSHOT_SHOT;
            w->fired++;
        }
    }
}

/* Holds a waiting shot at its place, bobbing 10 units up and down (10.8 degrees per frame). */
void EftRingShot_BobShot(EftRingShotOne *shot) {
    Vec4 pos;

    Vec4_Copy(&pos, &shot->home);
    pos.y += sinf(shot->phase) * 10.0f;
    shot->phase += 10.8f / 180.0f * 3.14159265f;
    if (shot->phase >= 3.14159265f) {
        shot->phase -= 6.2831853f;
    }
    EftBlastObj_SetTarget(shot->shot, 1, &pos);
}

/* Variants 0 and 1: creates the shots on the owner's bit 4, slides each to its place, and launches them all at
 * the opponent's node 0x11 on bit 0x40. */
void EftRingShot_UpdateRing(s32 objId, EftTask *task) {
    s32 node = -1;
    s32 arg3 = 0;
    EftRingShot *w = task->work;
    EftTechArg *arg;
    s32 allArrived;
    s32 volley;
    EftRingShotOne *shot;
    s32 i;
    s32 tmp; /* one temporary for the flags word and the launch order, as in the original */
    Vec4 pos;
    Vec4 target;
    Vec4 dir;

    arg = w->arg;
    tmp = w->flags;
    if (!(tmp & EFT_RINGSHOT_DONE)) {
        allArrived = 1;
        volley = 0;
        if (EftShot_TestBits(objId, 4)) {
            node = 1;
            arg3 = 1;
            volley = 1;
        }
        if (node >= 0) {
            if ((EftEmit_GetPhaseMask(w->model) >> node) & 1) {
                EftRingShot_Fire(objId, task, node, arg3, volley);
                EftTechEvt_RequestRestart(arg->objId);
            }
        }
        for (i = 0; i < 20; i++) {
            shot = &w->shot[i];
            if (EftBlastObj_IsAlive(shot->shot)) {
                if (shot->time > shot->slideTime) {
                    shot->flags |= 2;
                }
                if (!(shot->flags & 2)) {
                    f32 t = shot->time / shot->slideTime;

                    if (t > 1.0f) {
                        t = 1.0f;
                    }
                    Vec3_ScaleAdd(&pos, &shot->from, &w->start, t);
                    EftBlastObj_SetTarget(shot->shot, 1, &pos);
                } else {
                    EftRingShot_BobShot(shot);
                }
                shot->time += 1.0f;
            } else if (shot->flags & 1) {
                shot->flags |= 2;
            }
        }
        if (w->fired >= w->total) {
            i = 0;
            shot = &w->shot[0];
            while (shot->shot == NULL || (shot->flags & 2)) {
                i++;
                if (i >= 20) {
                    goto checked;
                }
                shot = &w->shot[i];
            }
            allArrived = 0;
checked:
            if (allArrived) {
                for (i = 0; i < 20; i++) {
                    shot = &w->shot[i];
                    if (shot->shot != NULL) {
                        EftBlastObj_SetHeld(shot->shot, 0);
                    }
                }
                EftShot_SetHeldFlagA8(arg->objId);
                w->flags |= EFT_RINGSHOT_DONE;
            }
        }
    } else {
        if (!(tmp & EFT_RINGSHOT_LAUNCHED)) {
            for (i = 0; i < 20; i++) {
                shot = &w->shot[i];
                if (shot->shot != NULL) {
                    EftRingShot_BobShot(shot);
                }
            }
        }
        if (EftShot_TestBits(arg->objId, 0x40)) {
            BtlCharApi_GetNodePos(BtlCharApi_GetOpponentObjId(arg->objId), 0x11, &target);
            for (i = 0; i < 20; i++) {
                shot = &w->shot[i];
                if (shot->shot != NULL) {
                    EftBlastObj_SetTarget(shot->shot, 0, NULL);
                    Vec3_Sub(&dir, &target, &shot->home);
                    Vec3_Normalize(&dir, &dir);
                    EftBlastObj_SetDir(shot->shot, &dir);
                    tmp = shot->index;
                    tmp = 20 - tmp;
                    EftBlastObj_SetDelay(shot->shot, tmp * 0.75f);
                }
            }
            w->flags |= EFT_RINGSHOT_LAUNCHED;
        }
    }
}

/* Variant 2: on the owner's bit 4 lays an arc from the start to above the opponent and moves the carrier point
 * along it for 20 frames; afterwards the owner's bit 0x10 fires the volley. */
void EftRingShot_UpdatePath(s32 objId, EftTask *task) {
    s32 node = -1;
    s32 arg3 = 0;
    EftRingShot *w = task->work;
    EftTechArg *arg = w->arg;

    if (!(w->flags & EFT_RINGSHOT_DONE)) {
        if (EftShot_TestBits(objId, 4)) {
            s32 opp = BtlCharApi_GetOpponentObjId(arg->objId);
            f32 scale = BtlScene_GetCharScale(opp);

            if (scale < 0.6f) {
                scale = 0.6f;
            }
            Vec4_Copy(&w->path0, &w->start);
            BtlCharApi_GetNodePos(opp, 0x11, &w->path2);
            Vec3_Sub(&w->path1, &w->path2, &w->path0);
            Vec3_Scale(&w->path1, &w->path1, 0.5f);
            Vec3_Add(&w->path1, &w->path1, &w->path0);
            w->path1.y -= scale * 10.0f + 40.0f;
            w->path2.y -= scale * 25.0f + 145.0f;
            w->path1.w = 1.0f;
            Vec4_Copy(&w->path3, &w->path2);
            w->flags |= EFT_RINGSHOT_PATH;
        }
        if (w->flags & EFT_RINGSHOT_PATH) {
            f32 t = w->pathTime;
            f32 u;

            if (t > 18.0f) {
                t = 18.0f;
            }
            if (t < 9.0f) {
                EftMath_Spline3(&w->pos, &w->path0, t / 9.0f);
            } else {
                u = (t - 9.0f) / 9.0f;
                if (u > 1.0f) {
                    u = 1.0f;
                }
                EftMath_Spline3(&w->pos, &w->path1, u);
            }
            w->pathTime += 1.0f;
            if (w->pathTime >= 20.0f) {
                EftShot_SetHeldFlagA8(arg->objId);
                w->flags |= EFT_RINGSHOT_DONE;
            }
        }
    } else if (!(w->flags & EFT_RINGSHOT_SHOT)) {
        s32 volley = 0;

        if (EftShot_TestBits(objId, 0x10)) {
            node = 3;
            arg3 = 3;
            volley = 1;
        }
        if (node >= 0) {
            EftRingShot_Fire(objId, task, node, arg3, volley);
            EftTechEvt_RequestRestart(arg->objId);
        }
    }
}

/* Variant 3: on the owner's bit 0x10 puts both ends of the hit volume at the opponent's node 3. */
void EftRingShot_UpdateFixed(s32 objId, EftTask *task) {
    EftRingShot *w = task->work;
    EftTechArg *arg = w->arg;

    if (!(w->flags & EFT_RINGSHOT_DONE)) {
        if (EftShot_TestBits(objId, 0x10)) {
            s32 opp = BtlCharApi_GetOpponentObjId(arg->objId);

            BtlCharApi_GetNodePos(opp, 3, &w->pos);
            BtlCharApi_GetNodePos(opp, 3, &w->prev);
            EftShot_SetHeldFlagA8(arg->objId);
            w->flags |= EFT_RINGSHOT_DONE;
        }
    } else {
        EftShot_TestBits(arg->objId, 4);
    }
}

/* Forgets the shots whose blast object is gone; returns 1 while any is left. */
s32 EftRingShot_ReapShots(EftTask *task) {
    s32 alive = 0;
    EftRingShot *w = task->work;
    EftRingShotOne *shot;
    s32 i;

    for (i = 19, shot = w->shot; i >= 0; i--, shot++) {
        if (EftBlastObj_IsAlive(shot->shot)) {
            alive = 1;
        } else {
            shot->shot = NULL;
        }
    }
    return alive;
}

/* Runs the variant's update, then EftRingShot_ReapShots. */
s32 EftRingShot_UpdateShots(s32 objId, EftTask *task) {
    EftRingShot *w = task->work;

    switch (w->type) {
        case 0:
            EftRingShot_UpdateRing(objId, task);
            break;
        case 1:
            EftRingShot_UpdateRing(objId, task);
            break;
        case 2:
            EftRingShot_UpdatePath(objId, task);
            break;
        case 3:
            EftRingShot_UpdateFixed(objId, task);
            break;
    }
    return EftRingShot_ReapShots(task);
}

/* Adds this frame's hit record: the volume between pos and target (two spheres or two capsules by the
 * definition's shape), radius = size times the model's trail width. */
void EftRingShot_AddHit(EftTask *task, s32 last) {
    EftHitRec *rec;
    EftRingShot *w = task->work;
    f32 radius;

    rec = EftHit_GetNew();
    radius = w->hitSize * EftEmit_GetTrailWidth(&w->inst);
    *(EftRingShotPath *)rec->path = *(EftRingShotPath *)&w->start;
    rec->task = task;
    rec->arg = w->arg;
    if (last) {
        rec->flags |= 0x80;
    }
    switch (w->arg->def->hitShape) {
        case 1: {
            void *a = EftHitArena_AllocBox();
            void *b = EftHitArena_AllocBox();

            ColCapsule_Set(a, &w->unk330, &w->pos, radius);
            ColCapsule_Set(b, &w->unk330, &w->prev, radius);
            EftHit_SetShapeBoxes(rec, a, b);
            break;
        }
        case 0: {
            void *a = EftHitArena_AllocSphere();
            void *b = EftHitArena_AllocSphere();

            ColSphere_Set(a, &w->pos, radius);
            ColSphere_Set(b, &w->prev, radius);
            EftHit_SetShapeSpheres(rec, a, b);
            break;
        }
        default:
            return;
    }
    EftHit_Add(rec);
}

/* Steps every emitter of the model and spawns its particles at the carrier point. */
void EftRingShot_Draw(s32 objId, EftTask *task, EftModel *model, s32 mode) {
    EftRingShot *w = task->work;
    s32 part;
    s32 sub;

    for (part = 0; part < 0x13; part++) {
        if (*model->mask & (1 << part)) {
            EftModelPartDef *def = model->part[part].def;

            for (sub = 0; sub < def->count; sub++) {
                s32 spawn;

                if (EftEmit_IsPartDeferred(model, def->unk0, sub)) {
                    continue;
                }
                if (mode == 0) {
                    spawn = EftEmit_GetFlagsFromReq(model, &w->inst, objId, part, sub, w->flags & EFT_RINGSHOT_COUNT,
                                          w->flags & EFT_RINGSHOT_FAST_END);
                } else {
                    spawn = EftEmit_GetResetFlags(model, &w->inst, part, sub);
                }
                if (w->flags & EFT_RINGSHOT_RESET) {
                    spawn = 2;
                }
                if (spawn != 0) {
                    EftEmit_SpawnOwn(model, &w->inst, w->unk2D0, &w->pos, &w->dir, part, sub, spawn, w->drawSize);
                }
            }
        }
    }
}

/* Init callback: clears the work, reads the definition, picks the variant from the effect id. */
void EftRingShot_Init(EftTask *task, EftTechArg *arg) {
    EftRingShotMgr *mgr = BtlTask_GetParent(task)->work;
    EftRingShot *w = task->work;
    EftModel *model;
    EftTechDef *def;
    s32 id;

    memset(w, 0, sizeof(EftRingShot));
    w->arg = arg;
    model = &mgr->model;
    def = arg->def;
    w->speed = def->speed;
    w->unk5CC = def->unk38;
    w->size = def->size;
    w->drawSize = def->size;
    w->total = def->unk9 * def->count;
    w->hitSize = w->size;
    w->model = model;
    EftEmit_InitState(model, &w->inst);
    w->life = EftEmit_GetEndFrames(w->model);
    id = def->id;
    if (id == 0x26D) {
        w->type = 0;
    } else if (id == 0x18D) {
        w->type = 1;
    } else if (id == 0x156) {
        w->type = 2;
    } else if (id == 0x1DC) {
        w->type = 2;
    } else if (id == 0x2EC) {
        w->type = 2;
    } else if (id == 0x197) {
        w->type = 3;
    } else if (id == 0x198) {
        w->type = 3;
    } else if (id == 0x200) {
        w->type = 3;
    } else if (id == 0x2A4) {
        w->type = 3;
    }
    BtlTask_SetOwnerTag(task, arg->objId == 0 ? 0x800 : 0x1000);
}

/* Term callback: releases the model instance and the fighter's held flags. */
void EftRingShot_Term(EftTask *task) {
    EftRingShot *w = task->work;
    EftTechArg *arg = w->arg;

    EftEmit_TermState(w->model, &w->inst);
    if (!(w->flags & EFT_RINGSHOT_DONE)) {
        EftShot_SetHeldFlagA8(arg->objId);
    }
    if (arg->def->unk4 != 0) {
        EftShot_SetHeldFlagA9(arg->objId);
    }
}

/* Update callback. */
void EftRingShot_Update(EftTask *task) {
    s32 advance = 0;
    EftRingShot *w = task->work;
    EftTechArg *arg = w->arg;
    s32 alive;

    if (BtlScene_IsCharStopped(arg->objId)) {
        return;
    }
    EftEmit_UpdateNodesReq(arg, w->unk2D0);
    if (!(w->flags & EFT_RINGSHOT_STARTED)) {
        if (EftShot_TestBits(arg->objId, 2)) {
            EftAim_GetDir(&w->dir, w->unk2F0, arg->objId);
            task->step = 0;
            w->flags |= EFT_RINGSHOT_STARTED;
        } else if (EftShot_TestBits(arg->objId, 4)) {
            task->step = 0;
            w->flags |= EFT_RINGSHOT_STARTED;
        }
    }
    if (w->flags & EFT_RINGSHOT_STARTED) {
        switch (task->step) {
            case 0:
                if (EftShot_TestBits(arg->objId, 4)) {
                    w->flags |= EFT_RINGSHOT_FIRED;
                    switch (w->type) {
                        case 0:
                        case 1:
                            BtlCharApi_GetNodePos(arg->objId, 0x11, &w->start);
                            Vec4_Copy(&w->pos, &w->start);
                            BtlCharApi_GetNodePos(arg->objId, 0x11, &w->prev);
                            EftAim_GetDirKeep(arg, &w->dir, &w->pos, arg->objId);
                            Vec3_Scale(&w->vel, &w->dir, w->speed);
                            break;
                        case 2:
                            Vec4_Copy(&w->start, &w->unk330);
                            Vec4_Copy(&w->pos, &w->start);
                            BtlCharApi_GetNodePos(arg->objId, 0x11, &w->prev);
                            EftAim_GetDirKeep(arg, &w->dir, &w->pos, arg->objId);
                            Vec3_Scale(&w->vel, &w->dir, w->speed);
                            break;
                        case 3:
                            break;
                    }
                    task->step = 1;
                }
                break;
            case 2:
                if (!(w->flags & EFT_RINGSHOT_HIT_MOVED)) {
                    Vec4_Copy(&w->prev, &w->pos);
                }
                if (EftShot_TestBits(arg->objId, 8)) {
                    w->flags |= EFT_RINGSHOT_ENDED | EFT_RINGSHOT_COUNT;
                    task->step = 3;
                }
                break;
            case 1:
                advance = 1;
                break;
            case 3:
                advance = 1;
                break;
        }
    }
    alive = EftRingShot_UpdateShots(arg->objId, task);
    if (!alive) {
        if (w->flags & EFT_RINGSHOT_SHOT) {
            w->flags |= EFT_RINGSHOT_COUNT;
        }
    }
    if (EftShot_TestBits(arg->objId, 0x400)) {
        if (!(w->flags & EFT_RINGSHOT_ENDED)) {
            w->flags |= EFT_RINGSHOT_FAST_END;
        }
        w->flags |= EFT_RINGSHOT_COUNT;
    }
    EftRingShot_Draw(arg->objId, task, w->model, 0);
    if (advance) {
        task->step++;
    }
    if (w->flags & EFT_RINGSHOT_COUNT) {
        w->timer += 1.0f;
    }
    if (!alive && (w->flags & EFT_RINGSHOT_KILL)) {
        BtlTask_SetDead(task);
    } else if (w->flags & EFT_RINGSHOT_COUNT) {
        if ((w->flags & EFT_RINGSHOT_FAST_END) || w->timer >= w->life) {
            w->flags |= EFT_RINGSHOT_KILL;
        }
    } else if (arg->def->flags & 1) {
        if (w->flags & EFT_RINGSHOT_FIRED) {
            EftEmit_UpdateTrailWidth(w->model, &w->inst);
            switch (w->type) {
                case 0:
                case 1:
                    break;
                case 2:
                    if (!(w->flags & EFT_RINGSHOT_DONE)) {
                        EftRingShot_AddHit(task, 1);
                    }
                    break;
                case 3:
                    EftRingShot_AddHit(task, 0);
                    break;
            }
        }
    }
}

/* Post-update callback: follows the hit record when it was moved, and starts the end when it ended. */
void EftRingShot_PostUpdate(EftTask *task) {
    EftRingShot *w = task->work;
    Vec4 move;

    if (BtlScene_IsCharStopped(w->arg->objId)) {
        return;
    }
    EftEmit_UpdateAlive(w->model, &w->inst);
    EftRingShot_ReapShots(task);
    if (task->hit & 1) {
        Vec3_Sub(&move, &task->hitPos, &w->pos);
        Vec3_Copy(&w->pos, &task->hitPos);
        Vec3_Add(&w->prev, &w->prev, &move);
        if (!(w->flags & EFT_RINGSHOT_HIT_MOVED)) {
            w->flags |= EFT_RINGSHOT_HIT_MOVED;
        }
    } else {
        w->flags &= ~EFT_RINGSHOT_HIT_MOVED;
    }
    if (task->hit & 4) {
        w->flags |= EFT_RINGSHOT_COUNT;
    }
}

/* Reset callback: destroys the particles once and kills the task. */
void EftRingShot_Reset(EftTask *task) {
    EftRingShot *w = task->work;

    if (!(w->flags & EFT_RINGSHOT_RESET)) {
        w->flags |= EFT_RINGSHOT_RESET;
        EftEmit_KillAll(w->model, &w->inst);
    }
    BtlTask_SetDead(task);
}

/* Draw callback: nothing. */
void EftRingShot_Nop(void) {
}

/* Manager init: loads the effect model from the technique's pack and makes room for two volleys. */
void EftRingShotMgr_Init(EftTask *task, EftTechArg *arg) {
    EftRingShotMgr *mgr = task->work;

    EftShot_Nop(sizeof(EftRingShotMgr));
    memset(mgr, 0, sizeof(EftRingShotMgr));
    EftEmit_LoadSet(arg, &mgr->model, 0, arg->pack, 0, 4);
    BtlTask_CreateChildList(task, 2, sizeof(EftRingShot));
}

/* Manager term: releases the model. */
void EftRingShotMgr_Term(EftTask *task) {
    EftRingShotMgr *mgr = task->work;

    EftEmit_FreeSet(&mgr->model);
}

/* Manager update: steps the model. */
void EftRingShotMgr_Update(EftTask *task) {
    EftRingShotMgr *mgr = task->work;

    EftEmit_BeginFrame(&mgr->model);
}

/* Manager reset: nothing. */
void EftRingShotMgr_Reset(void) {
}
