/*
 * Battle object, fourth part (0x250B28..0x2527B0): secondary motion. The chains of extra bones a model carries
 * (hair, tails, cloth) are bent every frame by the object's movement, by pushes the fighter code adds, by the
 * stage's wind and by a noise source, and the result is written into the nodes' local rotations.
 * Layouts are in include/battle/bobj_b.h.
 *
 * This is a separate file because the original was: BObjChainA_StepAll and BObjChainB_StepAll only match when
 * the compiler has not seen the body of BtlObj_GetNode (0x2505A8, in the second part of bobj_a.c). The cut is put at the first
 * function of the group; nothing fixes it more exactly than "after 0x2505A8 and before 0x251940".
 *
 * Per frame, from BtlObj_UpdateAll: BtlObj_UpdateChains(obj) = every type A link, every type B link, then the
 * decay of the push. Inputs: last frame's node positions (BtlObj_SaveNodePositions), the owner's movement
 * (BtlObj_SetMoveVec), the push and sway the fighter code added, the stage wind, the object's own noise value.
 * No camera, view or draw list is read.
 */
#include "common.h"
#include "battle/bobj_b.h"

extern void *memset(void *dst, s32 c, u32 n);
extern f32 atan2f(f32 y, f32 x);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern f32 Vec3_Length(Vec4 *v);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern f32 Mathf_Sin(f32 a);
extern f32 Mathf_Cos(f32 a);

/* dst = 0. */
extern void Vec4_SetZero(Vec4 *dst);
/* Inverse of a rotation + translation matrix. */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
/* Matrix product into the first argument. */
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);
/* dst = lerp(a, b, t). */
extern void Vec4_Lerp(Vec4 *dst, Vec4 *a, Vec4 *b, f32 t);
/* Rotations of a vector: about an axis vector, about x, about y. */
extern void Vec3_RotateAxis(Vec4 *dst, Vec4 *src, Vec4 *axis, f32 angle);
extern void Vec3_RotateX(Vec4 *dst, Vec4 *src, f32 angle);
extern void Vec3_RotateY(Vec4 *dst, Vec4 *src, f32 angle);
/* Named "light direction" in stg_b.c; the chains use it as a wind vector whose w is the strength. */
extern s32 BtlStage_GetLightDir(Vec4 *out);

/* Written by BtlObj_InitChains (0.1, 0, 0, 0); no reader found. */
extern Vec4 D_003337D0;

#define Q_IDENT(q) ((q)->x = 0.0f, (q)->y = 0.0f, (q)->z = 0.0f, (q)->w = 1.0f)

/* Lets the push and the sway die away (x 0.85), except on a frame that added to them. */
void BtlObj_DecayPush(BObj *obj) {
    f32 rate;

    if (obj->pushed != 0) {
        obj->pushed = 0;
    } else {
        rate = 0.85f;
        Vec4_Scale(&obj->push, &obj->push, rate);
        obj->sway *= rate;
    }
}

/* Adds v to the push on the chains, keeping its length at most `max`. */
void BtlObj_AddPush(BObj *obj, Vec4 *v, f32 max) {
    Vec4 *push;
    f32 len;

    push = &obj->push;
    if (max <= 0.0f) {
        return;
    }
    if (Vec3_Length(push) < max) {
        Vec4_Add(push, push, v);
        len = Vec3_Length(push);
        if (max < len) {
            Vec4_Scale(push, push, max / len);
        }
    }
    obj->pushed = 1;
}

/* Adds to the sway (the push along each chain's own axis), keeping it at most `max`. */
void BtlObj_AddSway(BObj *obj, f32 add, f32 max) {
    if (max <= 0.0f) {
        return;
    }
    Vec3_Length(&obj->push);
    if (obj->sway < max) {
        obj->sway += add;
        if (max < obj->sway) {
            obj->sway = max;
        }
    }
    obj->pushed = 1;
}

/* Stores the owner's movement of this frame; the chains lag behind it. */
void BtlObj_SetMoveVec(BObj *obj, Vec4 *v) {
    Vec4_Copy(&obj->move, v);
}

/* Clears the secondary motion state and builds both chain tables from the model. */
void BtlObj_InitChains(BObj *obj) {
    memset(&obj->chainA, 0, 0x650);
    obj->chaos = 0.5f;
    Vec4_Set(&D_003337D0, 0.1f, 0.0f, 0.0f, 0.0f);
    BObjChainA_Build(obj);
    BObjChainB_Build(obj);
}

/* Per-frame secondary motion: both kinds of chains, then the decay of the push. */
void BtlObj_UpdateChains(BObj *obj) {
    BObjChainA_StepAll(obj);
    BObjChainB_StepAll(obj);
    BtlObj_DecayPush(obj);
}

/* The object's own noise source: the logistic map x = 3.9999 x (1 - x), restarted from 0.5 if it reaches 0. */
f32 BtlObj_ChaosRand(BObj *obj) {
    if (obj->chaos == 0.0f) {
        obj->chaos = 0.5f;
    }
    obj->chaos = obj->chaos * 3.9999f * (1.0f - obj->chaos);
    return obj->chaos;
}

/* Wraps an angle into -pi..pi (one turn at most). */
f32 BObjChainB_WrapAngle(f32 a) {
    if (a < -3.14159265f) {
        a += 6.2831853f;
    }
    if (a > 3.14159265f) {
        a -= 6.2831853f;
    }
    return a;
}

/* Steps one link of a type B chain and writes the node's rotation. `parent` is the frame the link hangs in, `out`
   receives the link's own frame for its child. */
void BObjChainB_Step(BObj *obj, BObjLink *link, Mtx44 *parent, Mtx44 *out) {
    Mtx44 inv;
    Vec4 pull;
    Vec4 dir;
    Vec4 vel;
    Vec4 force;
    Vec4 tmp;
    Quat prev;
    Quat rot;
    Vec4 axis;
    Vec4 wind;
    Vec4 side;
    Quat q0;
    Quat q1;
    Quat q2;
    Quat q3;
    f32 angle[2];
    Mtx44 mtx;
    BObjChainParam *param;
    BObjNode *node;
    BObjNode *chest;
    BObjLink *up;
    f32 scale;
    f32 mult;
    f32 fps;
    f32 yawMax;
    f32 yawMin;
    f32 gravity;
    f32 swing;
    f32 amount;
    f32 len;
    f32 sum;
    f32 k;
    f32 t;
    f32 ang;
    f32 lo;
    f32 hi;
    s32 slot;
    s32 id;
    f32 limA;
    f32 limB;
    f32 rate;
    f32 half;

    scale = 1.0f;
    mult = 1.0f;
    fps = 30.0f;
    id = link->node;
    if (obj->state.flags & BOBJ_FLAG_SLOW_CHAINS) {
        fps = 60.0f;
        mult = 2.0f;
        scale = 0.5f;
    }
    param = obj->mdl.chain;
    /* The original keeps these four in variables set here (30 and 5 degrees, the lag factor, the damping). */
    limA = 0.52359875f; /* 0x3F060A91: one bit below the nearest float to pi / 6 */
    limB = 0.08726646f;
    rate = 0.3f;
    half = 0.5f;
    slot = link->slot;
    axis.x = 0.0f;
    axis.y = Mathf_Cos(param->yaw[slot] * 3.14159265f / 180.0f);
    axis.z = Mathf_Sin(param->yaw[slot] * 3.14159265f / 180.0f);
    axis.w = 0.0f;
    gravity = param->gravityB[slot];
    swing = param->swingB[slot];
    yawMax = param->yawMax[slot] * 3.14159265f / 180.0f;
    yawMin = param->yawMin[slot] * 3.14159265f / 180.0f;
    Vec4_Copy((Vec4 *)&prev, (Vec4 *)&link->rot);
    node = BtlObj_GetNode(obj, id);
    if (node == NULL) {
        return;
    }
    Mtx_InverseRT(&inv, parent);
    Vec4_SetZero(&pull);
    Vec4_SetZero(&vel);
    Vec4_SetZero(&force);
    if (link->parent == NULL) {
        BtlObj_GetNodeVelocity(obj, id, &tmp);
        tmp.w = 0.0f;
        Mtx_MulVec4(&tmp, &obj->pose.world, &tmp);
        Vec4_Scale(&tmp, &tmp, mult * rate);
        Vec4_Add(&vel, &vel, &tmp);
    }
    Vec4_Scale(&tmp, &obj->move, mult * rate);
    Vec4_Add(&vel, &vel, &tmp);
    len = Vec3_Length(&vel);
    if (1388.8888f / fps < len) {
        Vec4_SetZero(&vel);
    }
    Vec4_Sub(&pull, &pull, &vel);
    BtlStage_GetLightDir(&wind);
    len = Vec3_Length(&wind);
    if (0.001f < len) {
        force.x = wind.x / len * wind.w * 0.2f;
        force.y = wind.y / len * wind.w * 0.2f;
        force.z = wind.z / len * wind.w * 0.2f;
        force.w = 0.0f;
    }
    up = link->parent;
    if (up == NULL) {
        link->idle[0] = BObjChainB_WrapAngle(link->idle[0] + scale * 0.1f);
        link->idle[1] = BObjChainB_WrapAngle(link->idle[1] + scale * 0.23f);
        link->idle[2] = BObjChainB_WrapAngle(link->idle[2] + scale * 0.27f);
    } else {
        link->idle[0] = BObjChainB_WrapAngle(up->idle[0] - 0.9f);
        link->idle[1] = BObjChainB_WrapAngle(up->idle[1] - 0.9f);
        link->idle[2] = BObjChainB_WrapAngle(up->idle[2] - 0.9f);
    }
    sum = 0.0f;
    sum += Mathf_Sin(link->idle[0]) * 0.25f + 0.25f;
    sum += Mathf_Sin(link->idle[1]) * 0.125f + 0.125f;
    sum += Mathf_Sin(link->idle[2]) * 0.125f + 0.125f;
    Vec4_Scale(&force, &force, sum);
    Vec4_Scale(&tmp, &vel, half * mult);
    Vec4_Sub(&force, &force, &tmp);
    Vec4_Add(&force, &force, &obj->push);
    Vec4_Scale(&tmp, &axis, obj->sway);
    tmp.w = 0.0f;
    Mtx_MulVec4(&tmp, &inv, &tmp);
    Vec4_Add(&force, &force, &tmp);
    Vec4_Set(&side, 1.0f, 0.0f, 0.0f, 0.0f);
    chest = BtlObj_GetNode(obj, 3);
    if (chest != NULL) {
        Vec4_Copy(&side, (Vec4 *)chest->world.m[0]);
    }
    amount = Vec3_Length(&obj->move) * 2.0f * mult;
    amount += Vec3_Length(&obj->push) * 4.0f;
    amount += obj->sway * 4.0f;
    if (link->depthB & 1) {
        link->swing[0] = BObjChainB_WrapAngle(link->swing[0] + amount * 0.1f * (BtlObj_ChaosRand(obj) * 0.3f + 0.9f) * scale);
        link->swing[1] = BObjChainB_WrapAngle(link->swing[1] + amount * 0.1f * (BtlObj_ChaosRand(obj) * 0.3f + 0.7f) * scale);
        link->swing[2] = BObjChainB_WrapAngle(link->swing[2] + amount * 0.1f * (BtlObj_ChaosRand(obj) * 0.3f + 1.3f) * scale);
    } else {
        link->swing[0] = BObjChainB_WrapAngle(link->swing[0] - amount * 0.1f * (BtlObj_ChaosRand(obj) * 0.3f + 0.9f));
        link->swing[1] = BObjChainB_WrapAngle(link->swing[1] - amount * 0.1f * (BtlObj_ChaosRand(obj) * 0.3f + 0.7f));
        link->swing[2] = BObjChainB_WrapAngle(link->swing[2] - amount * 0.1f * (BtlObj_ChaosRand(obj) * 0.3f + 1.3f));
    }
    scale = 0.5f; /* the variable is reused: weight of the two swing phases and upper limit of the blend below */
    sum = 0.0f;
    sum += Mathf_Sin(link->swing[0]) * scale;
    sum += Mathf_Sin(link->swing[1]) * scale;
    k = amount * swing * scale;
    ang = sum * k;
    if (ang < -limA) {
        ang = -limA;
    }
    if (limA < ang) {
        ang = limA;
    }
    Vec3_RotateAxis(&force, &force, &side, ang);
    sum = 0.0f;
    sum += Mathf_Sin(link->swing[2]);
    ang = sum * k;
    if (ang < -limB) {
        ang = -limB;
    }
    if (limB < ang) {
        ang = limB;
    }
    Vec3_RotateY(&force, &force, ang);
    Vec4_Add(&pull, &pull, &force);
    pull.w = 0.0f;
    pull.y += gravity;
    Mtx_MulVec4(&pull, &inv, &pull);
    t = Vec3_Length(&pull) * 0.2f;
    if (t < 0.1f) {
        t = 0.1f;
    }
    if (scale < t) {
        t = scale;
    }
    Quat_ConjugateBy((Quat *)&side, (Quat *)&axis, &prev);
    len = Vec3_Length(&pull);
    if (0.0001f < len) {
        Vec4_Scale(&dir, &pull, 1.0f / len);
    } else {
        Vec4_Copy(&dir, &axis);
    }
    Quat_FromVectors(&q0, &side, &dir, t);
    Quat_ConjugateBy((Quat *)&dir, (Quat *)&side, &q0);
    Quat_FromMtx(&q1, parent);
    Quat_Inverse(&q2, &q1);
    Quat_Mul(&q3, &q2, (Quat *)&link->offset);
    Quat_ConjugateBy((Quat *)&dir, (Quat *)&dir, &q3);
    Vec4_Copy(&link->offset, (Vec4 *)&q1);
    angle[0] = atan2f(axis.z, axis.y);
    angle[1] = atan2f(dir.z, dir.y);
    t = BObjChainB_WrapAngle(angle[1] - angle[0]);
    if (t < yawMax) {
        Vec3_RotateX(&dir, &dir, yawMax - t);
    }
    if (yawMin < t) {
        Vec3_RotateX(&dir, &dir, yawMin - t);
    }
    lo = Mathf_Sin(-0.1f);
    hi = Mathf_Sin(0.1f);
    if (dir.x < lo) {
        dir.x = lo;
    }
    if (hi < dir.x) {
        dir.x = hi;
    }
    Vec3_Normalize(&dir, &dir);
    Vec3_Normalize(&tmp, &dir);
    Quat_FromVectors(&rot, &axis, &tmp, 1.0f);
    Vec4_Copy((Vec4 *)&node->rot, (Vec4 *)&rot);
    Vec4_Copy((Vec4 *)&link->rot, (Vec4 *)&rot);
    node->flags &= ~1;
    Quat_ToMtx(&mtx, &rot);
    Mtx_Mul(out, parent, &mtx);
}
/* The function's 28 float constants, 0x2FE698..0x2FE708, in the middle of this file's pool. */

/* Builds the type B link table: one link per bounds record whose node (id >= 0x47) is listed in the model's
   chain parameters, with its depth in the chain and its parent link. */
void BObjChainB_Build(BObj *obj) {
    BObjLink *stack[8];
    s32 ids[32];
    s32 *p;
    BObjChainB *work;
    BObjLink *link;
    BObjBound *bound;
    BObjChainParam *param;
    s32 depth;
    s32 level;
    s32 slot;
    s32 i;

    depth = 0;
    level = 0;
    work = &obj->chainB;
    bound = obj->mdl.bounds;
    work->count = 0;
    {
        /* The original walks a pointer to the second quaternion of each link. */
        Vec4 *q = &obj->chainB.links[0].offset;

        for (i = 0; i < BOBJ_CHAIN_B_MAX; i++, q += 4) {
            *((s8 *)q + 0x10) = -1;
            Q_IDENT(&q[-1]);
            Q_IDENT(&q[0]);
        }
    }
    if (obj->mdl.chain != NULL) {
        p = ids; /* only this store goes through the pointer; every other access indexes the array */
        p[0] = -1;
        while (1) {
            BtlObj_GetNode(obj, bound->node);
            level++;
            ids[level] = bound->node;
            slot = -1;
            if (ids[level] >= 0x47) {
                param = obj->mdl.chain;
                for (i = 0; i < 8; i++) {
                    if (param->nodeB[i] == ids[level]) {
                        slot = i;
                        break;
                    }
                }
            }
            if (slot >= 0) {
                link = &work->links[work->count];
                link->slot = slot;
                link->node = ids[level];
                link->depthA = ids[level - 1];
                stack[depth] = link;
                link->depthB = depth;
                if (depth > 0) {
                    link->parent = stack[depth - 1];
                } else {
                    link->parent = NULL;
                }
                work->count++;
                depth++;
                if (bound->pop != 0) {
                    depth -= bound->pop;
                    if (depth < 0) {
                        depth = 0;
                    }
                }
            }
            if (bound->pop != 0) {
                level -= bound->pop;
            }
            if (bound->last != 0) {
                break;
            }
            bound = (BObjBound *)((u8 *)bound + bound->next);
        }
    }
}

/* Steps every type B link in table order (a parent always comes before its children). */
void BObjChainB_StepAll(BObj *obj) {
    Mtx44 frames[BOBJ_CHAIN_B_MAX];
    BObjNode *node;
    s32 i;
    s8 slot;
    s8 depth;
    s8 id;

    for (i = 0; i < obj->chainB.count; i++) {
        slot = obj->chainB.links[i].slot;
        depth = obj->chainB.links[i].depthB;
        id = obj->chainB.links[i].node;
        if (slot >= 0) {
            if (depth <= 0) {
                node = BtlObj_GetNode(obj, id);
                if (node != NULL) {
                    BObjChainB_Step(obj, &obj->chainB.links[i], &node->parent, &frames[0]);
                }
            } else {
                BObjChainB_Step(obj, &obj->chainB.links[i], &frames[depth - 1], &frames[depth]);
            }
        }
    }
}

/* Wraps an angle into -pi..pi (one turn at most); the type A copy. */
/* The original compiler knew this helper to be free of side effects (it was most likely `static`, which this
   compiler marks const by itself; nothing outside this file calls it). BObjChainA_Step matches only with that:
   a call to a const function does not flush the scheduler's list of pending memory reads, which decides the
   order of two loads in front of the first call of each `parent != NULL` arm. */
f32 BObjChainA_WrapAngle(f32 a) __attribute__((const));
f32 BObjChainA_WrapAngle(f32 a) {
    if (a < -3.14159265f) {
        a += 6.2831853f;
    }
    if (a > 3.14159265f) {
        a -= 6.2831853f;
    }
    return a;
}

/* Matching notes: what made this match is the same as for BObjChainB_Step (see there), plus: `stiff` declared
   before `scale` / `mult` (it ties with `mult` in the allocator's priority and the lower pseudo wins), the order
   of the spilled variables' declarations (it is the order of their stack slots), `limit` read before `hingeOfs`,
   a variable of its own for the first sum, the variables `sum` / `t` / `k` reused as written below, and
   BObjChainA_WrapAngle known as const (see above). Emits the 19 constants at 0x2FE718..0x2FE764. */
/* Steps one link of a type A chain and writes the node's rotation; arguments as BObjChainB_Step. */
void BObjChainA_Step(BObj *obj, BObjLink *link, Mtx44 *parent, Mtx44 *out) {
    Mtx44 inv;
    Vec4 pull;
    Vec4 vel;
    Vec4 force;
    Vec4 tmp;
    Quat prev;
    Quat target;
    Quat rot;
    Vec4 axis;
    Vec4 wind;
    Vec4 side;
    Quat twist;
    Quat fix;
    Mtx44 mtx;
    BObjChainParam *param;
    BObjNode *node;
    BObjNode *chest;
    BObjLink *up;
    s32 hinge;
    f32 follow;
    f32 rest;
    f32 gravity;
    f32 limit;
    f32 hingeOfs;
    f32 inertia;
    f32 damping;
    f32 stiff;
    f32 scale;
    f32 mult;
    f32 fps;
    f32 hingeLimit;
    f32 speed;
    f32 phase;
    f32 swingMax;
    f32 swing;
    f32 amount;
    f32 angle;
    f32 len;
    f32 sum;
    f32 idle;
    f32 k;
    f32 t;
    f32 ang;
    f32 eps;
    f32 rate;
    f32 half;
    s32 slot;
    s32 id;

    scale = 1.0f;
    mult = 1.0f;
    fps = 30.0f;
    slot = link->slot;
    id = link->node;
    if (obj->state.flags & BOBJ_FLAG_SLOW_CHAINS) {
        fps = 60.0f;
        mult = 2.0f;
        scale = 0.5f;
    }
    param = obj->mdl.chain;
    /* As in BObjChainB_Step, the original keeps these in variables set here. */
    eps = 0.001f;
    rate = 0.2f;
    half = 0.5f;
    axis.x = param->axisX[slot];
    axis.y = -param->axisY[slot];
    axis.z = -param->axisZ[slot];
    axis.w = 0.0f;
    limit = param->limit[slot] * 3.14159265f / 180.0f;
    hingeOfs = param->hingeOfs[slot] * 3.14159265f / 180.0f;
    hingeLimit = param->hingeLimit[slot] * 3.14159265f / 180.0f;
    hinge = param->hinge[slot];
    speed = param->speed[slot] * 3.14159265f / 180.0f;
    phase = param->phase[slot] * 3.14159265f / 180.0f;
    swingMax = param->swingMax[slot] * 3.14159265f / 180.0f;
    stiff = param->stiff[slot] * 0.01f;
    follow = param->follow[slot];
    rest = param->rest[slot];
    gravity = param->gravity[slot];
    inertia = param->inertia[slot];
    damping = param->damping[slot];
    swing = param->swing[slot];
    len = Vec3_Length(&axis);
    if (eps < len) {
        Vec4_Scale(&axis, &axis, 1.0f / len);
    }
    Vec4_Copy((Vec4 *)&prev, (Vec4 *)&link->rot);
    node = BtlObj_GetNode(obj, id);
    if (node == NULL) {
        return;
    }
    Mtx_InverseRT(&inv, parent);
    Vec4_SetZero(&pull);
    Vec4_SetZero(&vel);
    Vec4_SetZero(&force);
    if (link->parent == NULL) {
        BtlObj_GetNodeVelocity(obj, id, &tmp);
        tmp.w = 0.0f;
        Mtx_MulVec4(&tmp, &obj->pose.world, &tmp);
        Vec4_Scale(&tmp, &tmp, mult * rate);
        Vec4_Add(&vel, &vel, &tmp);
    }
    Vec4_Scale(&tmp, &obj->move, mult * rate);
    Vec4_Add(&vel, &vel, &tmp);
    len = Vec3_Length(&vel);
    if (1388.8888f / fps < len) {
        Vec4_SetZero(&vel);
    }
    Vec4_Sub(&pull, &pull, &vel);
    BtlStage_GetLightDir(&wind);
    len = Vec3_Length(&wind);
    if (eps < len) {
        force.x = wind.x / len * wind.w * rate;
        force.y = wind.y / len * wind.w * rate;
        force.z = wind.z / len * wind.w * rate;
        force.w = 0.0f;
    }
    up = link->parent;
    if (up == NULL) {
        link->idle[0] = BObjChainA_WrapAngle(link->idle[0] + (speed + 0.1f) * scale);
        link->idle[1] = BObjChainA_WrapAngle(link->idle[1] + (speed + 0.23f) * scale);
        link->idle[2] = BObjChainA_WrapAngle(link->idle[2] + (speed + 0.27f) * scale);
    } else {
        link->idle[0] = BObjChainA_WrapAngle(up->idle[0] - 0.9f);
        link->idle[1] = BObjChainA_WrapAngle(up->idle[1] - 0.9f);
        link->idle[2] = BObjChainA_WrapAngle(up->idle[2] - 0.9f);
    }
    idle = 0.0f;
    idle += Mathf_Sin(link->idle[0] + phase) * 0.25f + 0.25f;
    idle += Mathf_Sin(link->idle[1] + phase) * 0.125f + 0.125f;
    idle += Mathf_Sin(link->idle[2] + phase) * 0.125f + 0.125f;
    Vec4_Scale(&force, &force, idle);
    Vec4_Scale(&tmp, &vel, half * mult);
    Vec4_Sub(&force, &force, &tmp);
    Vec4_Add(&force, &force, &obj->push);
    Vec4_Scale(&tmp, &axis, obj->sway);
    tmp.w = 0.0f;
    Mtx_MulVec4(&tmp, &inv, &tmp);
    Vec4_Add(&force, &force, &tmp);
    Vec4_Set(&side, 1.0f, 0.0f, 0.0f, 0.0f);
    chest = BtlObj_GetNode(obj, 3);
    if (chest != NULL) {
        Vec4_Copy(&side, (Vec4 *)chest->world.m[0]);
    }
    amount = Vec3_Length(&obj->move) * mult;
    amount += Vec3_Length(&obj->push) * 3.0f;
    amount += obj->sway * 3.0f;
    up = link->parent;
    if (up == NULL) {
        link->swing[0] = BObjChainA_WrapAngle(link->swing[0] + amount * 0.1f * (BtlObj_ChaosRand(obj) * 0.3f + 0.9f) * scale);
        link->swing[1] = BObjChainA_WrapAngle(link->swing[1] + amount * 0.1f * (BtlObj_ChaosRand(obj) * 0.3f + 0.7f) * scale);
        link->swing[2] = BObjChainA_WrapAngle(link->swing[2] + amount * 0.1f * (BtlObj_ChaosRand(obj) * 0.3f + 1.3f) * scale);
    } else {
        link->swing[0] = BObjChainA_WrapAngle(up->swing[0] - 1.5707963f);
        link->swing[1] = BObjChainA_WrapAngle(up->swing[1] - 1.5707963f);
        link->swing[2] = BObjChainA_WrapAngle(up->swing[2] - 1.5707963f);
    }
    sum = 0.0f;
    sum += Mathf_Sin(link->swing[0]) * 0.5f;
    sum += Mathf_Sin(link->swing[1]) * 0.5f;
    k = amount * swing * 0.1f;
    ang = sum * k;
    if (ang < -swingMax) {
        ang = -swingMax;
    }
    if (swingMax < ang) {
        ang = swingMax;
    }
    Vec3_RotateAxis(&force, &force, &side, ang);
    sum = 0.0f;
    sum += Mathf_Sin(link->swing[2]);
    ang = sum * k;
    if (ang < -swingMax) {
        ang = -swingMax;
    }
    if (swingMax < ang) {
        ang = swingMax;
    }
    Vec3_RotateY(&force, &force, ang);
    Vec4_Add(&pull, &pull, &force);
    Vec4_Lerp(&link->offset, &vel, &link->offset, damping);
    Vec4_Scale(&tmp, &link->offset, inertia);
    Vec4_Add(&pull, &pull, &tmp);
    pull.w = 0.0f;
    pull.y += gravity;
    Mtx_MulVec4(&pull, &inv, &pull);
    Vec4_Scale(&tmp, &axis, rest);
    Vec4_Add(&pull, &pull, &tmp);
    len = Vec3_Length(&pull);
    if (0.0001f < len) {
        Vec4_Scale(&tmp, &pull, 1.0f / len);
        Quat_FromVectors(&target, &axis, &tmp, follow);
    } else {
        Quat_SetIdentity(&target);
    }
    if (0.0f < stiff) {
        k = 0.5f;
        t = Vec3_Length(&pull) * 0.05f * stiff;
        ang = (1.0f - stiff) * k + t;
        if (ang < 0.05f) {
            ang = 0.05f;
        }
        if (k < ang) {
            ang = k;
        }
        Quat_Slerp(&rot, &prev, &target, ang);
    } else {
        Quat_Slerp(&rot, &prev, &target, 0.5f);
    }
    Quat_LimitAngle(&rot, &rot, limit);
    if (0.0f < hingeLimit) {
        switch (hinge) {
        case 0:
            side.x = 0.0f;
            side.y = axis.z;
            side.z = -axis.y;
            side.w = 0.0f;
            break;
        case 1:
            side.x = -axis.z;
            side.y = 0.0f;
            side.z = axis.x;
            side.w = 0.0f;
            break;
        case 2:
            side.x = axis.y;
            side.y = -axis.x;
            side.z = 0.0f;
            side.w = 0.0f;
            break;
        }
        sum = 0.0f;
        Quat_FromVectors(&twist, &side, (Vec4 *)&rot, 1.0f);
        angle = Quat_GetAngle(&twist);
        if (Vec3_Dot(&axis, (Vec4 *)&twist) < sum) {
            angle = -angle;
        }
        angle = BObjChainA_WrapAngle(angle + hingeOfs);
        t = 3.14159265f - hingeLimit;
        if (t < __builtin_fabsf(angle)) {
            if (!(sum < angle)) {
                t = -t;
            }
            angle = t - angle;
            sum = Mathf_Cos(angle);
            if (0.0f < sum) {
                Quat_FromAxisAngle(&fix, axis.x, axis.y, axis.z, angle);
                Quat_ConjugateBy(&rot, &rot, &fix);
                Quat_SlerpIdentity(&rot, &rot, sum);
            } else {
                Quat_SetIdentity(&rot);
            }
        }
    }
    Vec4_Copy((Vec4 *)&node->rot, (Vec4 *)&rot);
    Vec4_Copy((Vec4 *)&link->rot, (Vec4 *)&rot);
    node->flags &= ~1;
    Quat_ToMtx(&mtx, &rot);
    Mtx_Mul(out, parent, &mtx);
}


/* Builds the type A link table, like BObjChainB_Build. */
void BObjChainA_Build(BObj *obj) {
    BObjLink *stack[BOBJ_CHAIN_A_MAX];
    BObjChainA *work;
    BObjLink *link;
    BObjBound *bound;
    BObjChainParam *param;
    s32 depth;
    s32 slot;
    u8 id;
    s32 i;

    depth = 0;
    work = &obj->chainA;
    bound = obj->mdl.bounds;
    work->count = 0;
    {
        Vec4 *q = (Vec4 *)&work->links[0].rot;

        for (i = 0; i < BOBJ_CHAIN_A_MAX; i++, q += 4) {
            *((s8 *)q + 0x20) = -1;
            Q_IDENT(q);
        }
    }
    if (obj->mdl.chain != NULL) {
        while (1) {
            BtlObj_GetNode(obj, bound->node);
            slot = -1;
            if (bound->node >= 0x47) {
                param = obj->mdl.chain;
                for (i = 0; i < 0x10; i++) {
                    if (param->nodeA[i] == bound->node) {
                        slot = i;
                        break;
                    }
                }
            }
            if (slot >= 0) {
                link = &work->links[work->count];
                link->slot = slot;
                id = bound->node;
                stack[depth] = link;
                link->node = id;
                link->depthA = depth;
                if (depth > 0) {
                    link->parent = stack[depth - 1];
                } else {
                    link->parent = NULL;
                }
                work->count++;
                depth++;
                if (bound->pop != 0) {
                    depth -= bound->pop;
                    if (depth < 0) {
                        depth = 0;
                    }
                }
            }
            if (bound->last != 0) {
                break;
            }
            bound = (BObjBound *)((u8 *)bound + bound->next);
        }
    }
}

/* Steps every type A link in table order. */
void BObjChainA_StepAll(BObj *obj) {
    Mtx44 frames[BOBJ_CHAIN_A_MAX];
    BObjNode *node;
    s32 i;
    s8 slot;
    s8 depth;
    s8 id;

    for (i = 0; i < obj->chainA.count; i++) {
        slot = obj->chainA.links[i].slot;
        depth = obj->chainA.links[i].depthA;
        id = obj->chainA.links[i].node;
        if (slot >= 0) {
            if (depth <= 0) {
                node = BtlObj_GetNode(obj, id);
                if (node != NULL) {
                    BObjChainA_Step(obj, &obj->chainA.links[i], &node->parent, &frames[0]);
                }
            } else {
                BObjChainA_Step(obj, &obj->chainA.links[i], &frames[depth - 1], &frames[depth]);
            }
        }
    }
}

