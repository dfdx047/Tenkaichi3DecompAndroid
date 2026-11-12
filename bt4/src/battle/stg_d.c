/*
 * Stage rigid bodies (0x22FD10..0x230B38): the list of falling debris pieces. See battle/stg_d.h.
 *
 * Per frame (StgRigid_Update, skipped while the battle is paused):
 *   StgRigidList_BeginFrame   remember the transform, collect nearby triangles (only for bodies with a hit
 *                             buffer), choose the drag
 *   10 sub-steps of 1/60 + 0.00001: StgRigidList_UpdateCenters, StgRigidList_Collide, StgRigidList_AddForces
 *                             (gravity + drag), StgRigidList_Step
 *   StgRigidList_EndFrame     clamp to the stage radius, rest test
 * Every pass walks the active list in creation order; bodies do not interact with each other.
 */
#include "common.h"
#include "sys/math3d.h"
#include "sys/heap.h"
#include "sys/randf.h"
#include "sys/rigid.h"
#include "battle/stg_d.h"

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 sqrtf(f32 x);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void ColSphere_Set(RigidSphere *out, Vec4 *center, f32 radius);        /* fills a sphere */
extern s32 ColSphere_TestTri(Vec4 *out, RigidSphere *sphere, StgRigidTri *tri);    /* sphere against triangle: closest point */
extern void StgCol_CollectSphere(RigidSphere *sphere, s32 hits);                     /* collects the stage triangles near a sphere */

extern s32 BtlStage_IsReady(void);
extern f32 BtlStage_GetInnerRadius(void);
extern s32 BtlStage_GetWaterLevel(f32 *level);

/* Length of one sub-step. The constant is 0x3C889D80 (0.016676664): 1 / 60 plus a little, so that ten steps pass
   the 1 / 6 limit of StgRigid_Update. (1.0f / 60.0f * 1.0006f gives the same bits.) */
#define STG_RIGID_DT (1.0f / 60.0f + 0.00001f)

/* The two list helpers in front of this file. */
extern s32 StgRigidList_Count(StgRigidNode *head);       /* number of nodes */
extern StgRigidNode *StgRigid_AllocNode(void);          /* a free node, NULL when the pool is used up */

/* Local view of the battle work: only the flag word. */
typedef struct StgRigidWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags;       /* 0x100: paused */
} StgRigidWork;
extern StgRigidWork *Battle_GetWork(void);

/* Last node of a list, NULL when it is empty. */
StgRigidNode *StgRigidList_Last(StgRigidNode *head) {
    StgRigidNode **link = &head;
    StgRigidNode *last = NULL;

    while (*link != NULL) {
        last = *link;
        link = &last->next;
    }
    return last;
}

/* The active body with this id, NULL when there is none. */
StgRigidNode *StgRigid_Find(s32 id) {
    StgRigidNode *head = gStgRigid->active;
    StgRigidNode **link = &head;
    StgRigidNode *n;

    while (*link != NULL) {
        n = *link;
        if (n->id == id) {
            return n;
        }
        link = &n->next;
    }
    return NULL;
}

/* Keeps the body inside the stage cylinder: when its horizontal distance from the origin plus its radius exceeds
   the inner radius, x and z are scaled back and both angular velocities are halved. */
void StgRigid_ClampToStage(RigidBody *body) {
    Vec4 v;
    f32 dist;
    f32 limit;

    v.y = 0.0f;
    v.x = body->pos.x;
    v.z = body->pos.z;
    dist = sqrtf(v.x * v.x + v.z * v.z) + body->prevSphere.radius;
    limit = BtlStage_GetInnerRadius();
    if (limit < dist) {
        f32 k = limit / dist;

        body->pos.x *= k;
        body->pos.z *= k;
        body->angVel.x *= 0.5f;
        body->angVel.y *= 0.5f;
        body->angVel.z *= 0.5f;
        body->angVelBody.x *= 0.5f;
        body->angVelBody.y *= 0.5f;
        body->angVelBody.z *= 0.5f;
        Rigid_SyncSpheres(body);
    }
}

/* Start of a frame for every body: remembers the transform; a moving body with a hit buffer gets the stage
   triangles near it, and its drag becomes the under-water value when its centre is below the water level. */
void StgRigidList_BeginFrame(StgRigidNode *head) {
    RigidSphere sphere;
    f32 water;
    StgRigidNode *n;
    StgRigidNode **link;
    s32 hasWater;

    hasWater = BtlStage_GetWaterLevel(&water);
    for (link = &head; *link != NULL; link = &n->next) {
        n = *link;
        Rigid_BeginFrame(&n->body);
        Rigid_SyncSpheres(&n->body);
        if (n->resting) {
            continue;
        }
        if (n->body.user != 0 && BtlStage_IsReady()) {
            ColSphere_Set(&sphere, &n->body.prevSphere.center, n->body.prevSphere.radius + n->body.moved + 0.1f);
            StgCol_CollectSphere(&sphere, n->body.user);
        }
        n->damp = 0.0015f;
        if (BtlStage_IsReady() && hasWater) {
            if (n->body.user != 0 && water < n->body.prevSphere.center.y) {
                n->damp = 0.1115f;
            }
        }
    }
}

/* Sub-step, first pass: world-space centre of mass and collision spheres of every moving body. */
void StgRigidList_UpdateCenters(StgRigidNode *head) {
    StgRigidNode *n;
    StgRigidNode **link;

    for (link = &head; *link != NULL; link = &n->next) {
        n = *link;
        if (n->resting) {
            continue;
        }
        Rigid_UpdateCenter(&n->body);
        Rigid_SyncSpheres(&n->body);
    }
}

/* Sub-step, second pass: a contact force for every collected triangle the body's sphere touches. */
void StgRigidList_Collide(StgRigidNode *head) {
    Vec4 hit;
    Vec4 d;
    Vec4 point;
    StgRigidNode *n;
    StgRigidNode **link;
    u32 i;

    for (link = &head; *link != NULL; link = &n->next) {
        n = *link;
        if (!BtlStage_IsReady()) {
            continue;
        }
        if (n->resting) {
            continue;
        }
        if (n->body.user == 0) {
            continue;
        }
        for (i = 0; i < ((StgRigidHits *)n->body.user)->count; i++) {
            if (ColSphere_TestTri(&hit, &n->body.prevSphere, &((StgRigidHits *)n->body.user)->tri[i])) {
                f32 depth;

                Vec4_Sub(&d, &n->body.prevSphere.center, &hit);
                depth = n->body.prevSphere.radius - sqrtf(Vec3_Dot(&d, &d));
                point.x = n->body.prevSphere.center.x + -((StgRigidHits *)n->body.user)->tri[i].normal.x * n->body.prevSphere.radius;
                point.y = n->body.prevSphere.center.y + -((StgRigidHits *)n->body.user)->tri[i].normal.y * n->body.prevSphere.radius;
                point.z = n->body.prevSphere.center.z + -((StgRigidHits *)n->body.user)->tri[i].normal.z * n->body.prevSphere.radius;
                point.w = 1.0f;
                Rigid_AddContact(&n->body, &point, &((StgRigidHits *)n->body.user)->tri[i].normal, gStgRigidRestitution,
                                 gStgRigidFriction, depth);
            }
        }
    }
}

/* Sub-step, third pass: gravity and drag on every moving body. */
void StgRigidList_AddForces(StgRigidNode *head) {
    StgRigidNode *n;
    StgRigidNode **link;

    for (link = &head; *link != NULL; link = &n->next) {
        n = *link;
        if (n->resting) {
            continue;
        }
        Rigid_AddGravity(&n->body);
        Rigid_Damp(&n->body, n->damp);
    }
}

/* Sub-step, last pass: integrates every moving body over STG_RIGID_DT. */
void StgRigidList_Step(StgRigidNode *head) {
    StgRigidNode *n;
    StgRigidNode **link;

    for (link = &head; *link != NULL; link = &n->next) {
        n = *link;
        if (n->resting) {
            continue;
        }
        Rigid_Step(&n->body, STG_RIGID_DT);
    }
}

/* End of a frame: stage clamp, then the rest test (only bodies with a hit buffer can come to rest). */
void StgRigidList_EndFrame(StgRigidNode *head) {
    StgRigidNode *n;
    StgRigidNode **link;

    for (link = &head; *link != NULL; link = &n->next) {
        n = *link;
        if (n->resting) {
            continue;
        }
        StgRigid_ClampToStage(&n->body);
        if (n->body.user != 0) {
            n->resting = Rigid_CheckRest(&n->body);
        } else {
            n->resting = 0;
        }
    }
}

/* Forgets every body: both lists empty, the pool unused. */
void StgRigid_Reset(void) {
    gStgRigid->used = 0;
    gStgRigid->free = NULL;
    gStgRigid->active = NULL;
}

/* Allocates the pool and numbers its nodes 1..128. */
void StgRigid_Init(void) {
    s32 i;

    gStgRigid = Heap_Alloc(sizeof(StgRigidPool), 0x20, 0, 2);
    memset(gStgRigid, 0, sizeof(StgRigidPool));
    for (i = 0; i < STG_RIGID_MAX; i++) {
        gStgRigid->nodes[i].id = i + 1;
    }
    StgRigid_Reset();
}

/* Frees the pool. */
void StgRigid_Term(void) {
    if (gStgRigid != NULL) {
        Heap_Free(gStgRigid);
        gStgRigid = NULL;
    }
}

/* New body at `pos`, unrotated and at rest, appended to the active list. Returns its id, -1 when the pool is full. */
s32 StgRigid_Create(Vec4 *pos, f32 radius, s32 user) {
    Mtx44 rot;
    StgRigidNode *n;
    StgRigidNode **link;
    StgRigidNode *last;

    memset(&rot, 0, sizeof(Mtx44));
    rot.m[0][0] = 1.0f;
    rot.m[1][1] = 1.0f;
    rot.m[2][2] = 1.0f;
    rot.m[3][3] = 1.0f;
    n = StgRigid_AllocNode();
    if (n == NULL) {
        return -1;
    }
    last = StgRigidList_Last(gStgRigid->active);
    if (last == NULL) {
        gStgRigid->active = n;
        n->prev = NULL;
    } else {
        last->next = n;
        n->prev = last;
    }
    n->next = NULL;
    Rigid_Init(&n->body, user, radius, gStgRigidDensity);
    Rigid_SetTransform(&n->body, pos, &rot);
    Rigid_SyncSpheres(&n->body);
    n->damp = 0.0015f;
    return n->id;
}

/* Throws a body: horizontal velocity = `push` (shortened when its squared length exceeds 1200), a random upward
   speed that depends on the material, each component randomly boosted, and a random spin. */
void StgRigid_Launch(s32 id, Vec4 *push, s32 material) {
    Vec4 spin;
    Vec4 vel;
    StgRigidNode *n;
    StgRigidNode **link;
    f32 up;
    f32 d;

    memset(&vel, 0, sizeof(Vec4));
    vel.w = 1.0f;
    spin.x = Rand_FloatRange(0.0f, 1.0f) * -0.3f;
    spin.y = Rand_FloatRange(0.0f, 1.0f) * -0.35f;
    spin.z = Rand_FloatRange(0.0f, 1.0f) * -0.25f;
    spin.w = 1.0f;
    n = StgRigid_Find(id);
    if (n == NULL) {
        BtlStage_IsReady();
        return;
    }
    Vec4_Copy(&vel, push);
    vel.y = 0.0f;
    d = Vec3_Dot(&vel, &vel);
    if (1200.0f < d) {
        d = 1200.0f / d;
        vel.x *= d;
        vel.z *= d;
    }
    switch (material) {
    case 0:
        up = -10.0f;
        break;
    case 1:
        up = -17.0f;
        break;
    case 2:
        up = -21.0f;
        break;
    case 3:
        spin.z = 0.0f;
        spin.y = 0.0f;
        spin.x = 0.0f;
    default:
        up = -10.0f;
        break;
    }
    vel.y = up * 0.5f * Rand_FloatRange(0.0f, 1.0f) + up * 0.5f;
    if (rand() & 1) {
        spin.x = -spin.x;
    }
    if (rand() & 1) {
        spin.y = -spin.y;
    }
    if (rand() & 1) {
        spin.z = -spin.z;
    }
    if (rand() & 1) {
        vel.x += vel.x * Rand_FloatRange(0.0f, 1.0f);
    }
    if (rand() & 1) {
        vel.y += vel.y * Rand_FloatRange(0.0f, 1.0f);
    }
    if (rand() & 1) {
        vel.z += vel.z * Rand_FloatRange(0.0f, 1.0f);
    }
    Rigid_SetVelocity(&n->body, &vel, &spin);
}

/* Like StgRigid_Launch with a given spin and an upward speed of 5..10. No caller. */
void StgRigid_LaunchSpin(s32 id, Vec4 *push, Vec4 *angVel) {
    Vec4 vel;
    StgRigidNode *n;
    StgRigidNode **link;
    f32 d;

    memset(&vel, 0, sizeof(Vec4));
    vel.w = 1.0f;
    n = StgRigid_Find(id);
    if (n == NULL) {
        BtlStage_IsReady();
        return;
    }
    Vec4_Copy(&vel, push);
    vel.y = 0.0f;
    d = Vec3_Dot(&vel, &vel);
    if (1200.0f < d) {
        d = 1200.0f / d;
        vel.x *= d;
        vel.z *= d;
    }
    vel.y = Rand_FloatRange(0.0f, 1.0f) * -5.0f + -5.0f;
    if (rand() & 1) {
        vel.x += vel.x * Rand_FloatRange(0.0f, 1.0f);
    }
    if (rand() & 1) {
        vel.y += vel.y * Rand_FloatRange(0.0f, 1.0f);
    }
    if (rand() & 1) {
        vel.z += vel.z * Rand_FloatRange(0.0f, 1.0f);
    }
    Rigid_SetVelocity(&n->body, &vel, angVel);
}

/* Takes a body off the active list and puts its node on the free list. */
void StgRigid_Release(s32 id) {
    StgRigidNode *n = StgRigid_Find(id);

    if (n == NULL) {
        return;
    }
    if (gStgRigid->active == n) {
        gStgRigid->active = n->next;
    } else {
        if (n->prev != NULL) {
            n->prev->next = n->next;
        }
        if (n->next != NULL) {
            n->next->prev = n->prev;
        }
    }
    n->next = gStgRigid->free;
    gStgRigid->free = n;
}

/* One frame of every body: 10 sub-steps of STG_RIGID_DT (until the summed time passes 1 / 6). Nothing while paused. */
void StgRigid_Update(void) {
    f32 t = 0.0f;

    if (Battle_GetWork()->flags & 0x100) {
        return;
    }
    StgRigidList_BeginFrame(gStgRigid->active);
    do {
        t += STG_RIGID_DT;
        StgRigidList_UpdateCenters(gStgRigid->active);
        StgRigidList_Collide(gStgRigid->active);
        StgRigidList_AddForces(gStgRigid->active);
        StgRigidList_Step(gStgRigid->active);
    } while (t <= 1.0f / 6.0f);
    StgRigidList_EndFrame(gStgRigid->active);
    gStgRigid->count = StgRigidList_Count(gStgRigid->active);
}

/* Writes a body's world matrix; returns 0 when the id names no body. */
s32 StgRigid_GetMatrix(Mtx44 *out, s32 id) {
    StgRigidNode *n = StgRigid_Find(id);

    if (n != NULL) {
        Rigid_GetMatrix(out, &n->body);
        return 1;
    }
    return 0;
}

/* A body's collision sphere, NULL when the id names no body. No caller. */
RigidSphere *StgRigid_GetSphere(s32 id) {
    StgRigidNode *n = StgRigid_Find(id);

    if (n != NULL) {
        return &n->body.prevSphere;
    }
    return NULL;
}

/* Makes a box empty (min = +1e10, max = -1e10) so that points can be added to it. First of the box helpers. */
void StgAabb_SetEmpty(StgAabb *box) {
    f32 big = 1e10f;

    box->min[0] = big;
    box->min[1] = big;
    box->min[2] = big;
    box->max[0] = -big;
    box->max[1] = -big;
    box->max[2] = -big;
}
