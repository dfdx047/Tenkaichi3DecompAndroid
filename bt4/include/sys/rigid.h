#ifndef SYS_RIGID_H
#define SYS_RIGID_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Rigid-body dynamics of one free sphere. Source range 0x266728-0x267AB8.
 *
 * A RigidBody is a sphere of uniform density with a position, an orientation (3x3 rotation kept in
 * the first three rows of a Mtx44), a linear velocity and an angular velocity. Forces and torques
 * are accumulated during a frame and consumed by Rigid_Step.
 *
 * Conventions (all verified by the matching code):
 *   - Row-vector matrices, as everywhere else in the game: row i of `rot` is body axis i written in
 *     world coordinates. Mat3_MulVecRows(v, rot) = v * rot takes body -> world;
 *     Mat3_MulVecCols(v, rot) = rot * v takes world -> body (the inverse, the matrix being
 *     orthonormal).
 *   - +Y is down: gravity is the force (0, +9.8 * mass, 0).
 *   - `inertia` is the diagonal of the body-space inertia tensor; a sphere has three equal entries.
 *
 * One step (Rigid_Step(body, dt)):
 *   Rigid_Integrate:
 *     vel       += force * (dt / mass)
 *     tb         = Mat3_MulVecCols(torque, rot)                    torque in body space
 *     a.x        = (tb.x + (I.y - I.z) * wb.y * wb.z) / I.x  ...   Euler's equations (Rigid_EulerAccel)
 *     wb        += dt * a * 0.999                                  wb = angVelBody
 *     angVel     = Mat3_MulVecRows(wb, rot)                        back to world space
 *     forceSum  += |force|;  force = torque = (0, 0, 0, 1)
 *   then
 *     pos'       = pos + dt * vel
 *     rot'       = orthonormalise(rot + rot * skew(dt * angVel))   first-order rotation update
 *   Explicit (forward) Euler, one sub-step, no collision between bodies.
 *
 * Users (not decompiled, 0x22FDA0-0x230AA0): the battle stage update (BtlStage_Update) keeps a linked
 * list of bodies at *(gStgRigid + 0x12004), each node being 0x10 bytes of links followed by a
 * RigidBody. StgRigid_Update steps the list unless BATTLE_FLAG_PAUSE is set, in sub-steps of
 * dt = 0.016677 until the accumulated time exceeds 1/6 (10 sub-steps per frame); per sub-step:
 * update centre, contact, gravity + damping, Rigid_Step; then once per frame Rigid_CheckRest.
 *
 * Forces: Rigid_AddGravity, Rigid_AddContact (penalty spring against a plane, with damping and an
 * optional friction force), Rigid_Damp (velocity drag). Rigid_CheckRest damps slow rotation and
 * reports when the body has come to rest.
 */

/* A 3x3 matrix is the upper-left part of a Mtx44: three rows of four floats, w unused. */
typedef Mtx44 Mat3;

/* Bounding sphere written by ColSphere_Set (centre, radius): 0x18 bytes used, 0x20 with padding.
   The struct is copied with 64-bit moves, so it is 8-byte aligned in the original. */
typedef struct RigidSphere {
    /* 0x00 */ Vec4 center;   /* w = 1 */
    /* 0x10 */ f32 radius;
    /* 0x14 */ f32 radiusSq;
    /* 0x18 */ s64 pad18;
} RigidSphere; /* size 0x20 */

typedef struct RigidBody {
    /* 0x000 */ s32 flags;        /* 1 after Rigid_Init; Rigid_BeginFrame keeps the low byte only */
    /* 0x004 */ s32 prevFlags;    /* flags of the previous frame */
    /* 0x008 */ f32 mass;         /* radius^3 * 2.356 * density */
    /* 0x00C */ s32 unkC;
    /* 0x010 */ Vec4 inertia;     /* diagonal inertia tensor (2/5 m r^2, times 20), w = 1 */
    /* 0x020 */ Vec4 localCenter; /* centre of mass in body space; (0, 0, 0, 1) */
    /* 0x030 */ Vec4 center;      /* centre of mass in world space: localCenter * rot + pos */
    /* 0x040 */ Vec4 pos;         /* world position of the body origin */
    /* 0x050 */ Mat3 rot;         /* orientation, rows = body axes in world space */
    /* 0x090 */ Vec4 vel;         /* linear velocity, world */
    /* 0x0A0 */ Vec4 angVel;      /* angular velocity, world */
    /* 0x0B0 */ Vec4 force;       /* accumulated this frame, world */
    /* 0x0C0 */ Vec4 torque;      /* accumulated this frame, world, about `center` */
    /* 0x0D0 */ Vec4 angVelBody;  /* angular velocity, body space: the integrated quantity */
    /* 0x0E0 */ s32 unkE0;
    /* 0x0E4 */ s32 user;         /* second argument of Rigid_Init */
    /* 0x0E8 */ s32 unkE8[2];
    /* 0x0F0 */ Vec4 prevPos;     /* pos at Rigid_BeginFrame / Rigid_SetTransform */
    /* 0x100 */ Mat3 prevRot;     /* rot at Rigid_BeginFrame / Rigid_SetTransform */
    /* 0x140 */ f32 moved;        /* |pos - prevPos|, written by Rigid_CheckRest */
    /* 0x144 */ u8 unk144[0x8C];  /* not touched in this file */
    /* 0x1D0 */ RigidSphere prevSphere; /* copy of `sphere` made by Rigid_Init; centre set by Rigid_SyncSpheres */
    /* 0x1F0 */ RigidSphere sphere;     /* collision sphere: centre = pos, radius */
    /* 0x210 */ s64 unk210;       /* cleared by Rigid_BeginFrame */
    /* 0x218 */ f32 forceSum;     /* sum of |force| over the steps */
    /* 0x21C */ s32 unk21C;
} RigidBody; /* size 0x220 */

void Vec3_ScaleNeg(Vec4 *out, Vec4 *v, f32 scale);
void Rigid_EulerAccel(Vec4 *out, Vec4 *angVel, Vec4 *torque, Vec4 *inertia);
void Mat3_SetSkew(Mat3 *out, Vec4 *v);
void Mat3_Mul(Mat3 *out, Mat3 *a, Mat3 *b);
void Mat3_Add(Mat3 *out, Mat3 *a, Mat3 *b);
void Mat3_MulVecCols(Vec4 *out, Vec4 *v, Mat3 *m);
void Mat3_MulVecRows(Vec4 *out, Vec4 *v, Mat3 *m);
void Mat3_Orthonormalize(Mat3 *m);
void Rigid_Stub266CC0(void);
void Rigid_AddForceAt(RigidBody *body, Vec4 *point, Vec4 *force);
void Rigid_Integrate(RigidBody *body, f32 dt);
void Rigid_GetMatrix(Mtx44 *out, RigidBody *body);
void Rigid_AddGravity(RigidBody *body);
void Rigid_Damp(RigidBody *body, f32 k);
void Rigid_UpdateCenter(RigidBody *body);
void Rigid_AddContact(RigidBody *body, Vec4 *point, Vec4 *normal, f32 restitution, f32 friction, f32 depth);
void Rigid_Stub267390(void);
void Rigid_Step(RigidBody *body, f32 dt);
void Rigid_SetTransform(RigidBody *body, Vec4 *pos, Mtx44 *rot);
void Rigid_SetTransform2(RigidBody *body, Vec4 *pos, Mtx44 *rot);
void Rigid_BeginFrame(RigidBody *body);
void Rigid_SetVelocity(RigidBody *body, Vec4 *vel, Vec4 *angVel);
s32 Rigid_CheckRest(RigidBody *body);
void Rigid_SyncSpheres(RigidBody *body);
void Rigid_Init(RigidBody *body, s32 user, f32 radius, f32 density);

#endif
