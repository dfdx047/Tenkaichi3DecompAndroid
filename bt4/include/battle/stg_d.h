#ifndef BATTLE_STG_D_H
#define BATTLE_STG_D_H

#include "types.h"
#include "sys/math3d.h"
#include "sys/rigid.h"

/*
 * Stage rigid bodies: the debris pieces of broken stage objects (src/battle/stg_d.c, 0x22FD10..0x230B38).
 *
 * A pool of 128 nodes of 0x240 bytes (two list links, a drag factor, an id, one RigidBody of sys/rigid.h and a
 * "resting" flag) is allocated once per battle. Bodies in use are on a doubly linked list in creation order;
 * released ones go on a singly linked free list (linked through `next`). A body is named by its id (1..128), not
 * by a pointer; an id is looked up by walking the active list.
 *
 * The only user is the stage core (battle/stg_a.c): BtlStage_UpdateObjs creates one body per single-key debris
 * piece on the first frame an object falls (StgRigid_Create + StgRigid_Launch), copies the body's transform into
 * the piece's model node every frame (StgRigid_GetMatrix) and releases the bodies when the object's break
 * animation ends. StgRigid_Update is called from BtlStage_Update after BtlStage_UpdateObjs and
 * StgModel_UpdateAnims.
 *
 * The two functions the list functions need that lie just before this file (the same object, not decompiled
 * here): 0x22FC40 counts a list, 0x22FC80 takes a node from the free list or from the unused tail of the pool.
 */

#define STG_RIGID_MAX 128

/* The triangles a body may touch this frame, filled by StgCol_CollectSphere (collision range, not decompiled). The stage
   passes no such buffer (user = 0), so its debris never touches the stage geometry. */
typedef struct StgRigidTri {
    /* 0x00 */ Vec4 v[3];
    /* 0x30 */ Vec4 normal;
} StgRigidTri; /* size 0x40 */

typedef struct StgRigidHits {
    /* 0x00 */ u32 count;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ StgRigidTri tri[1];
} StgRigidHits;

typedef struct StgRigidNode {
    /* 0x000 */ struct StgRigidNode *prev;   /* active list only */
    /* 0x004 */ struct StgRigidNode *next;   /* active list, or the free list */
    /* 0x008 */ f32 damp;                    /* drag per sub-step: 0.0015, or 0.1115 under water */
    /* 0x00C */ s32 id;                      /* pool index + 1, written once by StgRigid_Init */
    /* 0x010 */ RigidBody body;              /* body.user: StgRigidHits * or 0 */
    /* 0x230 */ s32 resting;                 /* Rigid_CheckRest said the body stopped: skipped from then on */
    /* 0x234 */ s32 unk234[3];
} StgRigidNode; /* size 0x240 */

typedef struct StgRigidPool {
    /* 0x00000 */ StgRigidNode nodes[STG_RIGID_MAX];
    /* 0x12000 */ StgRigidNode *free;        /* released nodes */
    /* 0x12004 */ StgRigidNode *active;      /* bodies in use, oldest first */
    /* 0x12008 */ s32 used;                  /* nodes ever taken from `nodes` since the last reset (max 127) */
    /* 0x1200C */ s32 count;                 /* length of the active list after the last update */
} StgRigidPool; /* size 0x12010 */

/* The list walkers take the head node and walk `for (link = &head; *link != NULL; link = &n->next)`: the head
   parameter has its address taken, which is why each of them stores it on its stack. */

/* Axis-aligned box of the box helpers that follow this file (0x230B10..). */
typedef struct StgAabb {
    /* 0x00 */ f32 min[3];
    /* 0x0C */ f32 max[3];
} StgAabb; /* size 0x18 */

extern StgRigidPool *gStgRigid;
extern f32 gStgRigidRestitution;  /* 0.25 */
extern f32 gStgRigidFriction;     /* 0.7 */
extern f32 gStgRigidDensity;      /* 200 */

StgRigidNode *StgRigidList_Last(StgRigidNode *head);
StgRigidNode *StgRigid_Find(s32 id);
void StgRigid_ClampToStage(RigidBody *body);
void StgRigidList_BeginFrame(StgRigidNode *head);
void StgRigidList_UpdateCenters(StgRigidNode *head);
void StgRigidList_Collide(StgRigidNode *head);
void StgRigidList_AddForces(StgRigidNode *head);
void StgRigidList_Step(StgRigidNode *head);
void StgRigidList_EndFrame(StgRigidNode *head);
void StgRigid_Reset(void);
void StgRigid_Init(void);
void StgRigid_Term(void);
s32 StgRigid_Create(Vec4 *pos, f32 radius, s32 user);
void StgRigid_Launch(s32 id, Vec4 *push, s32 material);
void StgRigid_LaunchSpin(s32 id, Vec4 *push, Vec4 *angVel);
void StgRigid_Release(s32 id);
void StgRigid_Update(void);
s32 StgRigid_GetMatrix(Mtx44 *out, s32 id);
RigidSphere *StgRigid_GetSphere(s32 id);
void StgAabb_SetEmpty(StgAabb *box);

/* Stage object animations (src/battle/stg_d_b.c, 0x115170..0x115478). */
void StgModel_InitStage(void);
void StgModel_ResetAnims(void);
void StgModel_BindAnims(void);
void StgModel_ClearAnims(void);
void StgModel_UpdateAnims(void);

#endif
