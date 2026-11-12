#include "common.h"
#include "sys/rigid.h"

#define RIGID_ABS(a) ((a) < 0.0f ? -(a) : (a))

extern void *memset(void *dst, s32 value, u32 size);
extern f32 sqrtf(f32 x);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Mtx_StoreIdentity(Mtx44 *dst);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);              /* 4x4 matrix copy */
extern f32 Vec3_Dist(Vec4 *a, Vec4 *b);                     /* distance between two points */
extern f32 Mathf_Sqrt(f32 x);
extern void ColSphere_Set(RigidSphere *out, Vec4 *center, f32 radius); /* fills a sphere */

/* out = v * -scale (xyz). */
void Vec3_ScaleNeg(Vec4 *out, Vec4 *v, f32 scale) {
    scale = -scale;
    out->x = scale * v->x;
    out->y = scale * v->y;
    out->z = scale * v->z;
}

/* Euler's equations for a diagonal inertia tensor: out = (torque + (I x-permuted) w w) / I, body space. */
void Rigid_EulerAccel(Vec4 *out, Vec4 *angVel, Vec4 *torque, Vec4 *inertia) {
    out->x = (torque->x + (inertia->y - inertia->z) * angVel->y * angVel->z) / inertia->x;
    out->y = (torque->y + (inertia->z - inertia->x) * angVel->z * angVel->x) / inertia->y;
    out->z = (torque->z + (inertia->x - inertia->y) * angVel->x * angVel->y) / inertia->z;
}

/* out = the skew-symmetric (cross-product) matrix of v. */
void Mat3_SetSkew(Mat3 *out, Vec4 *v) {
    out->m[0][0] = 0.0f;
    out->m[0][1] = v->z;
    out->m[0][2] = -v->y;
    out->m[1][0] = -v->z;
    out->m[1][1] = 0.0f;
    out->m[1][2] = v->x;
    out->m[2][0] = v->y;
    out->m[2][1] = -v->x;
    out->m[2][2] = 0.0f;
}

/* out = a * b (3x3). out must not alias a or b. */
void Mat3_Mul(Mat3 *out, Mat3 *a, Mat3 *b) {
    out->m[0][0] = a->m[0][0] * b->m[0][0] + a->m[0][1] * b->m[1][0] + a->m[0][2] * b->m[2][0];
    out->m[0][1] = a->m[0][0] * b->m[0][1] + a->m[0][1] * b->m[1][1] + a->m[0][2] * b->m[2][1];
    out->m[0][2] = a->m[0][0] * b->m[0][2] + a->m[0][1] * b->m[1][2] + a->m[0][2] * b->m[2][2];
    out->m[1][0] = a->m[1][0] * b->m[0][0] + a->m[1][1] * b->m[1][0] + a->m[1][2] * b->m[2][0];
    out->m[1][1] = a->m[1][0] * b->m[0][1] + a->m[1][1] * b->m[1][1] + a->m[1][2] * b->m[2][1];
    out->m[1][2] = a->m[1][0] * b->m[0][2] + a->m[1][1] * b->m[1][2] + a->m[1][2] * b->m[2][2];
    out->m[2][0] = a->m[2][0] * b->m[0][0] + a->m[2][1] * b->m[1][0] + a->m[2][2] * b->m[2][0];
    out->m[2][1] = a->m[2][0] * b->m[0][1] + a->m[2][1] * b->m[1][1] + a->m[2][2] * b->m[2][1];
    out->m[2][2] = a->m[2][0] * b->m[0][2] + a->m[2][1] * b->m[1][2] + a->m[2][2] * b->m[2][2];
}

/* out = a + b (3x3). */
void Mat3_Add(Mat3 *out, Mat3 *a, Mat3 *b) {
    out->m[0][0] = a->m[0][0] + b->m[0][0];
    out->m[0][1] = a->m[0][1] + b->m[0][1];
    out->m[0][2] = a->m[0][2] + b->m[0][2];
    out->m[1][0] = a->m[1][0] + b->m[1][0];
    out->m[1][1] = a->m[1][1] + b->m[1][1];
    out->m[1][2] = a->m[1][2] + b->m[1][2];
    out->m[2][0] = a->m[2][0] + b->m[2][0];
    out->m[2][1] = a->m[2][1] + b->m[2][1];
    out->m[2][2] = a->m[2][2] + b->m[2][2];
}

/* out = m * v with v as a column: out[i] = row i of m . v. With a rotation: world -> body. */
void Mat3_MulVecCols(Vec4 *out, Vec4 *v, Mat3 *m) {
    out->x = v->x * m->m[0][0] + v->y * m->m[0][1] + v->z * m->m[0][2];
    out->y = v->x * m->m[1][0] + v->y * m->m[1][1] + v->z * m->m[1][2];
    out->z = v->x * m->m[2][0] + v->y * m->m[2][1] + v->z * m->m[2][2];
}

/* out = v * m with v as a row: out[j] = v . column j of m. With a rotation: body -> world. */
void Mat3_MulVecRows(Vec4 *out, Vec4 *v, Mat3 *m) {
    out->x = v->x * m->m[0][0] + v->y * m->m[1][0] + v->z * m->m[2][0];
    out->y = v->x * m->m[0][1] + v->y * m->m[1][1] + v->z * m->m[2][1];
    out->z = v->x * m->m[0][2] + v->y * m->m[1][2] + v->z * m->m[2][2];
}

/* Makes the three rows orthonormal again: row 2 is kept (normalised), row 0 = row1 x row2, row 1 = row2 x row0. */
void Mat3_Orthonormalize(Mat3 *m) {
    Vec4 z;
    Vec4 y;
    Vec4 x;

    z.x = m->m[2][0];
    z.y = m->m[2][1];
    z.z = m->m[2][2];
    Vec3_Normalize(&z, &z);
    m->m[2][0] = z.x;
    m->m[2][1] = z.y;
    m->m[2][2] = z.z;
    y.x = m->m[1][0];
    y.y = m->m[1][1];
    y.z = m->m[1][2];
    Vec3_Cross(&x, &y, &z);
    Vec3_Normalize(&x, &x);
    m->m[0][0] = x.x;
    m->m[0][1] = x.y;
    m->m[0][2] = x.z;
    Vec3_Cross(&y, &z, &x);
    m->m[1][0] = y.x;
    m->m[1][1] = y.y;
    m->m[1][2] = y.z;
}

/* Empty; called at the start of Rigid_Integrate. */
void Rigid_Stub266CC0(void) {
}

/* Accumulates a world-space force applied at a world-space point: force += f, torque += (point - center) x f. */
void Rigid_AddForceAt(RigidBody *body, Vec4 *point, Vec4 *force) {
    Vec4 torque;
    Vec4 arm;

    Vec4_Sub(&arm, point, &body->center);
    Vec3_Cross(&torque, &arm, force);
    body->force.x += force->x;
    body->force.y += force->y;
    body->force.z += force->z;
    body->torque.x += torque.x;
    body->torque.y += torque.y;
    body->torque.z += torque.z;
}

/* Turns the accumulated force and torque into velocity changes over dt, then clears them. */
void Rigid_Integrate(RigidBody *body, f32 dt) {
    Vec4 torque;
    Vec4 accel;
    f32 k;

    Rigid_Stub266CC0();
    k = dt / body->mass;
    body->vel.x += body->force.x * k;
    body->vel.y += body->force.y * k;
    body->vel.z += body->force.z * k;
    Mat3_MulVecCols(&torque, &body->torque, &body->rot);
    Rigid_EulerAccel(&accel, &body->angVelBody, &torque, &body->inertia);
    body->angVelBody.x += dt * accel.x * 0.999f;
    body->angVelBody.y += dt * accel.y * 0.999f;
    body->angVelBody.z += dt * accel.z * 0.999f;
    Mat3_MulVecRows(&body->angVel, &body->angVelBody, &body->rot);
    body->forceSum += sqrtf(Vec3_Dot(&body->force, &body->force));
    body->force.x = 0.0f;
    body->force.y = 0.0f;
    body->force.z = 0.0f;
    body->force.w = 1.0f;
    body->torque.x = 0.0f;
    body->torque.y = 0.0f;
    body->torque.z = 0.0f;
    body->torque.w = 1.0f;
}

/* Builds the body's 4x4 world matrix: rotation rows, position in row 3. */
void Rigid_GetMatrix(Mtx44 *out, RigidBody *body) {
    Mtx_StoreIdentity(out);
    out->m[0][0] = body->rot.m[0][0];
    out->m[0][1] = body->rot.m[0][1];
    out->m[0][2] = body->rot.m[0][2];
    out->m[1][0] = body->rot.m[1][0];
    out->m[1][1] = body->rot.m[1][1];
    out->m[1][2] = body->rot.m[1][2];
    out->m[2][0] = body->rot.m[2][0];
    out->m[2][1] = body->rot.m[2][1];
    out->m[2][2] = body->rot.m[2][2];
    out->m[3][0] = body->pos.x;
    out->m[3][1] = body->pos.y;
    out->m[3][2] = body->pos.z;
}

/* Adds the weight (0, 9.8 * mass, 0) at the centre of mass; +Y is down. */
void Rigid_AddGravity(RigidBody *body) {
    Vec4 force;

    force.x = 0.0f;
    force.y = body->mass * 9.8f;
    force.z = 0.0f;
    Rigid_AddForceAt(body, &body->center, &force);
}

/* Drag: removes the fraction k of the linear velocity and of the body-space angular velocity. */
void Rigid_Damp(RigidBody *body, f32 k) {
    body->vel.x -= body->vel.x * k;
    body->vel.y -= body->vel.y * k;
    body->vel.z -= body->vel.z * k;
    body->angVelBody.x -= body->angVelBody.x * k;
    body->angVelBody.y -= body->angVelBody.y * k;
    body->angVelBody.z -= body->angVelBody.z * k;
}

/* Recomputes the world-space centre of mass: localCenter * rot + pos. */
void Rigid_UpdateCenter(RigidBody *body) {
    Mat3_MulVecRows(&body->center, &body->localCenter, &body->rot);
    body->center.x += body->pos.x;
    body->center.y += body->pos.y;
    body->center.z += body->pos.z;
}

/* Contact with a plane at `point` with unit `normal`, penetrating by `depth`: a penalty spring
   (147 * mass * depth) minus a damper on the approach speed, plus sliding friction when friction > 0. */
void Rigid_AddContact(RigidBody *body, Vec4 *point, Vec4 *normal, f32 restitution, f32 friction, f32 depth) {
    Vec4 arm;
    Vec4 vel;
    Vec4 force;
    Vec4 torque;
    Vec4 arm2;
    Vec4 frictionTorque;
    Vec4 tangent;
    Vec4 frictionForce;
    f32 damp;
    f32 spring;
    f32 mag;
    f32 d;

    damp = (1.0f - restitution) * 19.6f * body->mass;
    spring = body->mass * (9.8f * 15.0f);
    Vec4_Sub(&arm, point, &body->center);
    Vec3_Cross(&vel, &body->angVel, &arm);
    vel.x += body->vel.x;
    vel.y += body->vel.y;
    vel.z += body->vel.z;
    mag = spring * depth - damp * Vec3_Dot(&vel, normal);
    if (mag < 0.0f) {
        return;
    }
    force.x = mag * normal->x;
    force.y = mag * normal->y;
    force.z = mag * normal->z;
    Vec4_Sub(&arm2, point, &body->center);
    Vec3_Cross(&torque, &arm2, &force);
    Vec3_Dot(&torque, &torque);
    body->force.x += force.x;
    body->force.y += force.y;
    body->force.z += force.z;
    body->torque.x += torque.x;
    body->torque.y += torque.y;
    body->torque.z += torque.z;
    if (0.0f < friction) {
        d = Vec3_Dot(&vel, normal);
        tangent.x = vel.x - d * normal->x;
        tangent.y = vel.y - d * normal->y;
        tangent.z = vel.z - d * normal->z;
        Vec3_Normalize(&tangent, &tangent);
        Vec3_ScaleNeg(&frictionForce, &tangent, mag * 0.9f);
        Vec3_Cross(&frictionTorque, &arm2, &frictionForce);
        Mathf_Sqrt(Vec3_Dot(&frictionTorque, &frictionTorque));
        body->force.x += frictionForce.x;
        body->force.y += frictionForce.y;
        body->force.z += frictionForce.z;
        body->torque.x += frictionTorque.x;
        body->torque.y += frictionTorque.y;
        body->torque.z += frictionTorque.z;
    }
}

/* Empty; no caller. */
void Rigid_Stub267390(void) {
}

/* One simulation step of dt seconds: integrates the velocities, then moves and rotates the body. */
void Rigid_Step(RigidBody *body, f32 dt) {
    Vec4 pos;
    Vec4 angle;
    Mat3 rot;
    Mat3 skew;
    Mat3 delta;

    Rigid_Integrate(body, dt);
    pos.x = body->pos.x + dt * body->vel.x;
    pos.y = body->pos.y + dt * body->vel.y;
    pos.z = body->pos.z + dt * body->vel.z;
    angle.x = dt * body->angVel.x;
    angle.y = dt * body->angVel.y;
    angle.z = dt * body->angVel.z;
    Mat3_SetSkew(&skew, &angle);
    Mat3_Mul(&delta, &body->rot, &skew);
    Mat3_Add(&rot, &body->rot, &delta);
    Mat3_Orthonormalize(&rot);
    Vec4_Copy(&body->pos, &pos);
    Mtx_Copy(&body->rot, &rot);
}

/* Places the body and stops it: velocities, force and torque are cleared, previous transform = current. */
void Rigid_SetTransform(RigidBody *body, Vec4 *pos, Mtx44 *rot) {
    Vec4_Copy(&body->pos, pos);
    Mtx_Copy(&body->rot, rot);
    Vec4_Copy(&body->prevPos, &body->pos);
    Mtx_Copy(&body->prevRot, &body->rot);
    body->vel.x = 0.0f;
    body->vel.y = 0.0f;
    body->vel.z = 0.0f;
    body->vel.w = 1.0f;
    body->force.x = 0.0f;
    body->force.y = 0.0f;
    body->force.z = 0.0f;
    body->force.w = 1.0f;
    body->torque.x = 0.0f;
    body->torque.y = 0.0f;
    body->torque.z = 0.0f;
    body->torque.w = 1.0f;
    body->angVel.x = 0.0f;
    body->angVel.y = 0.0f;
    body->angVel.z = 0.0f;
    body->angVel.w = 1.0f;
    body->angVelBody.x = 0.0f;
    body->angVelBody.y = 0.0f;
    body->angVelBody.z = 0.0f;
    body->angVelBody.w = 1.0f;
    body->moved = 0.0f;
}

/* Same as Rigid_SetTransform (plain forwarder); no caller. */
void Rigid_SetTransform2(RigidBody *body, Vec4 *pos, Mtx44 *rot) {
    Rigid_SetTransform(body, pos, rot);
}

/* Start of a frame: remembers the flags and the transform, keeps only the low byte of the flags. */
void Rigid_BeginFrame(RigidBody *body) {
    body->prevFlags = body->flags;
    Vec4_Copy(&body->prevPos, &body->pos);
    Mtx_Copy(&body->prevRot, &body->rot);
    body->flags &= 0xFF;
    body->unk210 = 0;
}

/* Sets the linear velocity and replaces the body-space angular velocity with that of angVel's Y part only. */
void Rigid_SetVelocity(RigidBody *body, Vec4 *vel, Vec4 *angVel) {
    Vec4 a;
    Vec4 b;
    Vec4 w;

    Vec4_Copy(&w, angVel);
    Mat3_MulVecRows(&a, &w, &body->rot);
    Mat3_MulVecCols(&b, &a, &body->rot);
    body->angVelBody.x -= b.x;
    body->angVelBody.y -= b.y;
    body->angVelBody.z -= b.z;
    w.x = 0.0f;
    w.z = 0.0f;
    w.y = angVel->y;
    Mat3_MulVecCols(&b, &w, &body->rot);
    body->angVelBody.x += b.x;
    body->angVelBody.y += b.y;
    body->angVelBody.z += b.z;
    body->vel.x = vel->x;
    body->vel.y = vel->y;
    body->vel.z = vel->z;
}

/* Damps slow rotation and returns 1 (restoring the previous transform) when the body is at rest. */
s32 Rigid_CheckRest(RigidBody *body) {
    f32 sum;
    s32 ret;

    body->moved = Vec3_Dist(&body->pos, &body->prevPos);
    sum = RIGID_ABS(body->angVel.x);
    sum += RIGID_ABS(body->angVel.y);
    sum += RIGID_ABS(body->angVel.z);
    if (__builtin_fabsf(body->angVel.x) < 0.1f) {
        body->angVel.x *= 0.2f;
    }
    if (__builtin_fabsf(body->angVel.y) < 0.1f) {
        body->angVel.y *= 0.2f;
    }
    if (__builtin_fabsf(body->angVel.z) < 0.1f) {
        body->angVel.z *= 0.2f;
    }
    if (__builtin_fabsf(body->angVelBody.x) < 0.1f) {
        body->angVelBody.x *= 0.2f;
    }
    if (__builtin_fabsf(body->angVelBody.y) < 0.1f) {
        body->angVelBody.y *= 0.2f;
    }
    if (__builtin_fabsf(body->angVelBody.z) < 0.1f) {
        body->angVelBody.z *= 0.2f;
    }
    if (__builtin_fabsf(body->angVel.x) < 0.15f) {
        body->angVel.x *= 0.7f;
    }
    if (__builtin_fabsf(body->angVel.y) < 0.15f) {
        body->angVel.y *= 0.7f;
    }
    if (__builtin_fabsf(body->angVel.z) < 0.15f) {
        body->angVel.z *= 0.7f;
    }
    if (__builtin_fabsf(body->angVelBody.x) < 0.25f) {
        body->angVelBody.x *= 0.7f;
    }
    if (__builtin_fabsf(body->angVelBody.y) < 0.25f) {
        body->angVelBody.y *= 0.7f;
    }
    if (__builtin_fabsf(body->angVelBody.z) < 0.25f) {
        body->angVelBody.z *= 0.7f;
    }
    ret = 0;
    if (body->moved <= 0.015f && sum < 0.2f) {
        Vec4_Copy(&body->pos, &body->prevPos);
        Mtx_Copy(&body->rot, &body->prevRot);
        ret = 1;
    }
    return ret;
}

/* Moves the centres of both collision spheres to the body position. */
void Rigid_SyncSpheres(RigidBody *body) {
    Vec4_Copy(&body->prevSphere.center, &body->pos);
    Vec4_Copy(&body->sphere.center, &body->pos);
}

/*
 * Clears the body and sets it up as a sphere of the given radius and density:
 *   mass = radius^3 * 2.3561945 (3 pi / 4) * density;  inertia = (2 / 5) mass radius^2 * 20 on all three axes;
 *   sphere = prevSphere = { centre (0, 0, 0), radius };  flags = 1;  user = second argument.
 *
 * The float constant 2.35619449f (0x4016CBE3) of this function is D_002FE818, the last entry of this file's
 * .lit4 pool.
 */
/* FAKE MATCH (permuter): `body++; body--;` in front of the clear. The two statements combine to a self-move
   (`body = body`) that survives until after the first scheduling pass and vanishes later. All it does is add ONE
   instruction to the instructions issued before the first memset call (the clear of `zero`); the scheduler
   issues two per cycle, so the call becomes the second instruction of its cycle instead of the first, the three
   argument loads of the second memset are then sorted together and come out as size, body, zero like the
   original (without it `a0 = body` is issued in the call's own cycle: `move a0,s0` in front of `li a2,0x220`).
   It stands for whatever gave the original an odd number of instructions there; no natural form was found
   (a local copy of `body`, inline identity / clear helpers, a cast through u32, `&body[0]`, an explicit memset
   of `zero`, a compound literal and a local for 1.0f all leave the pair as it was). */
void Rigid_Init(RigidBody *body, s32 user, f32 radius, f32 density) {
    Vec4 zero = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 inertia;

    body++;
    body--;
    memset(body, 0, sizeof(RigidBody));
    body->flags = 1;
    ColSphere_Set(&body->sphere, &zero, radius);
    Vec4_Copy(&body->localCenter, &zero);
    body->mass = radius * 2.35619449f * radius * radius * density;
    inertia = body->mass * radius * radius * 2.0f / 5.0f * 20.0f;
    body->inertia.x = inertia;
    body->inertia.y = inertia;
    body->inertia.z = inertia;
    body->inertia.w = 1.0f;
    body->prevSphere = body->sphere;
    body->user = user;
}
