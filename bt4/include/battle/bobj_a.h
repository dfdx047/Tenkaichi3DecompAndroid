#ifndef BATTLE_BOBJ_A_H
#define BATTLE_BOBJ_A_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Battle object, second part (first part of src/battle/bobj_a.c, 0x24BBE8..0x24F1F0): the animation player, the animation
 * events and attack volumes, the body volumes, the object transform and node matrices, the hit flash / fade
 * timers and the face animation. Continues src/battle/btl_obj.c; the types here are this module's own view of
 * the battle object (Bobj*), with the offsets it reads.
 *
 * ---- Animation file (one per motion; BPE-compressed in the character's animation table) ----
 *
 *   0x00  u8  unk00
 *   0x01  u8  eventCount
 *   0x02  u16 length                  last frame; the player's frame runs 0..length
 *   0x04  u16 unk04
 *   0x06  u16 track[71]               per model node id: offset of its track in 4-byte units from the start of
 *                                     the file, 0 = the node is not animated (rest pose)
 *   0x94  BobjAnimEvent event[eventCount]
 *
 * Track (BobjTrack): u16 flags (bit 0 = rotation only), u16 keyCount, then
 *   rotation only: u64 quat[keyCount] (Quat_Pack format), u16 frame[keyCount]
 *   full:          keyCount x { f32 x, y, z; u32 frame; u64 quat }   (0x18 bytes)
 * Keys are in increasing frame order; a frame between two keys is interpolated (translation linear, rotation
 * Quat_Slerp), a frame outside every key gives translation 0,0,0,1 and the identity rotation.
 *
 * Event (0x10 bytes): u64 attr, u16 frame, u16 shape, u32 arg.
 *   attr bit 0 (BOBJ_EV_HIT_BEGIN)   a hit window opens at `frame`; arg = mask of attack nodes (bit n = attack
 *                                    node n, 0..18), shape = offset (4-byte units) of the volume record,
 *                                    attr bit 32 = 1: the volume is a sphere, 0: a box
 *   attr bit 1 (BOBJ_EV_HIT_END)     the window closes at `frame`
 *   any other bit                    a plain event at `frame`; arg is OR-ed per identical attr
 *   attr bit 34 (BOBJ_EV_NODE)       arg selects a model node (BtlObjAnim_MaskToNode) kept in BobjHit.node
 * Volume record (floats): x, y, z (offset in the node's space), unused, r0, r1, r2 (sphere radius = r0; box
 * half sizes r0, r1, r2), all three multiplied by BobjHit.scale.
 */

#define BOBJ_NODE_MAX 71      /* model node ids 0..70 can be animated */
#define BOBJ_ATK_NODE_MAX 19  /* attack nodes 0..18 (BtlObj_GetNodeByCode maps them to model nodes) */
#define BOBJ_EVENT_MAX 8      /* distinct events kept per frame */
#define BOBJ_ANIM_MAX 0x19E   /* motions in a character's table */

/* Whether a track has rotation keys only. */
#define BOBJ_TRACK_ROT_ONLY(trk) ((u16)((trk)->flags & 1))

#define BOBJ_EV_HIT_BEGIN 1
#define BOBJ_EV_HIT_END 2
#define BOBJ_EV_NODE 0x400000000 /* attr bit 34 */

/* BobjAnimPlayer.mode */
#define BOBJ_PLAY_NONE 0
#define BOBJ_PLAY_ONCE 1 /* stops at the last frame and becomes NONE */
#define BOBJ_PLAY_LOOP 2 /* wraps to frame 0 */

/* A packed quaternion as the files store it: two words (the code always adds lo + (hi << 32)). */
typedef struct BobjPackedQuat {
    /* 0x00 */ u32 lo;
    /* 0x04 */ u32 hi;
} BobjPackedQuat;

/* Key of a full track. */
typedef struct BobjKey {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ u32 frame;
    /* 0x10 */ BobjPackedQuat rot;
} BobjKey; /* size 0x18 */

typedef struct BobjTrack {
    /* 0x00 */ u16 flags;      /* bit 0: rotation only */
    /* 0x02 */ u16 count;
    /* 0x04 */ union {
        BobjPackedQuat rot[1]; /* rotation only; u16 frame[count] follows */
        BobjKey key[1];
    } u;
} BobjTrack;

typedef struct BobjAnimEvent {
    /* 0x00 */ u32 attrLo;
    /* 0x04 */ u32 attrHi;
    /* 0x08 */ u16 frame;
    /* 0x0A */ u16 shape;
    /* 0x0C */ u32 arg;
} BobjAnimEvent; /* size 0x10 */

typedef struct BobjAnim {
    /* 0x00 */ u8 unk00;
    /* 0x01 */ u8 eventCount;
    /* 0x02 */ u16 length;
    /* 0x04 */ u16 unk04;
    /* 0x06 */ u16 track[BOBJ_NODE_MAX];
    /* 0x94 */ BobjAnimEvent event[1];
} BobjAnim;

/* Volume record of a hit event. */
typedef struct BobjAnimShape {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 unk0C;
    /* 0x10 */ f32 r0;
    /* 0x14 */ f32 r1;
    /* 0x18 */ f32 r2;
} BobjAnimShape;

/* Skeleton record of the model (BobjMdl.nodes): variable length, linked by byte offset, in parent-first order.
   The same records btl_obj.h calls BtlObjBound. */
typedef struct BobjBone {
    /* 0x00 */ s32 next;   /* byte offset to the next record */
    /* 0x04 */ u16 pop;    /* matrices to pop from the stack after this node (it closes that many branches) */
    /* 0x06 */ u16 last;   /* non-zero on the last record */
    /* 0x08 */ u16 unk08;
    /* 0x0A */ u16 id;     /* model node id */
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ Vec4 origin; /* the node's position in model space (body part offsets are relative to it) */
    /* 0x20 */ Vec4 unk20;
    /* 0x30 */ Vec4 rest;  /* translation from the parent in the rest pose */
} BobjBone;

/* Pose of one node (what BtlObj_GetNode returns). */
typedef struct BobjPart {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ u32 flags;     /* bit 0 cleared by BtlObjAnim_SamplePose */
    /* 0x08 */ u32 id;        /* model node id */
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ Mtx44 mtx;     /* world matrix; row 3 (+0x40) is the node's world position */
    /* 0x50 */ Mtx44 parent;  /* the parent's world matrix */
    /* 0x90 */ Vec4 pos;      /* local translation of this frame */
    /* 0xA0 */ Quat rot;      /* local rotation of this frame */
    /* 0xB0 */ Vec4 fromPos;  /* pose the blend started from (BtlObjAnim_StartBlend) */
    /* 0xC0 */ Quat fromRot;
} BobjPart;

/* One animation layer. */
typedef struct BobjAnimLayer {
    /* 0x00 */ BobjAnim *data;  /* NULL = nothing playing */
    /* 0x04 */ f32 length;      /* data->length */
    /* 0x08 */ u16 id;          /* motion id */
    /* 0x0A */ u16 cursor[BOBJ_NODE_MAX]; /* per node: key index found last frame (search starts there) */
} BobjAnimLayer; /* size 0x98 */

/* The animation player (BtlObj + 0xB40). */
typedef struct BobjAnimPlayer {
    /* 0x000 */ BobjAnimLayer layer[2]; /* 0 = the motion, 1 = a second motion mixed in by `mix` */
    /* 0x130 */ s32 manual;     /* BtlObj + 0xC70: 1 = the owner steps the object (BtlObj_UpdateAll skips it) */
    /* 0x134 */ u32 mode;       /* BOBJ_PLAY_*: only BtlObjAnim_Step uses it */
    /* 0x138 */ f32 frame;      /* BtlObj + 0xC78 */
    /* 0x13C */ f32 prevFrame;  /* BtlObj + 0xC7C */
    /* 0x140 */ f32 step;       /* BtlObj + 0xC80: frames per update */
    /* 0x144 */ f32 blend;      /* BtlObj + 0xC84: 1 -> 0 while blending from the previous pose */
    /* 0x148 */ f32 blendStep;  /* BtlObj + 0xC88: 1 / (seconds * 30) */
    /* 0x14C */ f32 mix;        /* BtlObj + 0xC8C: weight of layer 1 */
} BobjAnimPlayer; /* size 0x150 */

/* Events and attack volume of the current frame (BtlObj + 0xC90). */
typedef struct BobjHit {
    /* 0x00 */ f32 x;           /* volume offset in the attack node's space */
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ u32 nodeMask;    /* attack nodes that carry the volume this frame; 0 = no hit window */
    /* 0x10 */ f32 r0;          /* sphere radius / box half sizes, already scaled */
    /* 0x14 */ f32 r1;
    /* 0x18 */ f32 r2;
    /* 0x1C */ s8 hitCount;     /* BtlObj + 0xCAC: hit windows in the motion */
    /* 0x1D */ s8 hitIndex;     /* BtlObj + 0xCAD: window open this frame (0..), else -1 - windows already begun */
    /* 0x1E */ s8 eventCount;   /* BtlObj + 0xCAE */
    /* 0x1F */ s8 unk1F;
    /* 0x20 */ s32 sphere;      /* 1 = sphere, 0 = box */
    /* 0x24 */ s32 node;        /* BtlObj + 0xCB4: model node of the last BOBJ_EV_NODE event; 3 after a play */
    /* 0x28 */ f32 scale;       /* 1.0 at init */
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ u64 event[BOBJ_EVENT_MAX];    /* BtlObj + 0xCC0: attr words in (previous frame, frame] */
    /* 0x70 */ u32 eventArg[BOBJ_EVENT_MAX]; /* BtlObj + 0xD00 */
} BobjHit; /* size 0x90 */

/* Extra rotations applied on top of the motion (BtlObj + 0xD20); written by other modules. */
typedef struct BobjLook {
    /* 0x00 */ Quat node2;      /* multiplied onto node 2 */
    /* 0x10 */ Quat node2E;     /* node 0x2E is slerped towards this by weight2E */
    /* 0x20 */ Quat node2F;     /* node 0x2F is slerped towards this by weight2F */
    /* 0x30 */ f32 weight2E;
    /* 0x34 */ f32 weight2F;
} BobjLook;

/* Sphere as ColSphere_Set fills it. */
typedef struct BobjSphere {
    /* 0x00 */ Vec4 center;
    /* 0x10 */ f32 radius;
    /* 0x14 */ f32 radiusSq;
    /* 0x18 */ f32 unk18[2];
} BobjSphere; /* size 0x20 */

/* Oriented box (ColObb_Init / ColObb_Update / BtlObjObb_SetMtx). */
typedef struct BobjObb {
    /* 0x000 */ Vec4 center;     /* row 3 of mtx */
    /* 0x010 */ Vec4 unk10;
    /* 0x020 */ Vec4 axis[3];    /* rows 0..2 of mtx */
    /* 0x050 */ Mtx44 mtx;
    /* 0x090 */ Vec4 local[8];   /* corners in the box's space */
    /* 0x110 */ Vec4 world[8];   /* corners in world space */
} BobjObb; /* size 0x190 */

/* Axis-aligned box of 12-byte corners (StgAabb_SetEmpty and friends). */
typedef struct BobjAabb {
    /* 0x00 */ f32 min[3];
    /* 0x0C */ f32 max[3];
} BobjAabb; /* size 0x18 */

/* One body part of a fighter model (list in the model file, BobjMdl.body + 0x10). */
typedef struct BobjBodyPart {
    /* 0x000 */ u32 flags;       /* bit 0: last */
    /* 0x004 */ s32 node;        /* model node id */
    /* 0x008 */ BobjBone *bone;  /* filled by BtlObjBody_Init */
    /* 0x00C */ BobjPart *part;  /* filled by BtlObjBody_Init */
    /* 0x010 */ Vec4 offset;     /* centre in model space; minus the bone's rest translation = node space */
    /* 0x020 */ Vec4 center;     /* world centre of this frame */
    /* 0x030 */ u8 unk30[0x10];
    /* 0x040 */ BobjObb box;
} BobjBodyPart; /* size 0x1D0 */

/* Body collision data of a fighter model (BobjMdl.body). */
typedef struct BobjBodyData {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ f32 unk04;
    /* 0x08 */ u8 unk08[8];
    /* 0x10 */ BobjBodyPart part[1];
} BobjBodyData;

/* Body block of a fighter object (BtlObj + 0xF50, 0xC0 bytes). */
typedef struct BobjBody {
    /* 0x00 */ BobjBodyPart *parts;
    /* 0x04 */ u8 unk04[0xC];
    /* 0x10 */ BobjSphere sphere[2]; /* this frame's and the previous frame's body sphere */
    /* 0x50 */ BobjSphere *cur;      /* BtlObj + 0xFA0 */
    /* 0x54 */ BobjSphere *prev;
    /* 0x58 */ s32 index;
    /* 0x5C */ u8 unk5C[0x14];
    /* 0x70 */ BobjAabb box;         /* BtlObj + 0xFC0: bounds of every part box */
    /* 0x88 */ BobjAabb centerBox;   /* bounds of the part centres */
    /* 0xA0 */ f32 unkA0;            /* body data + 4 */
    /* 0xA4 */ f32 size[4];          /* BtlObj + 0xFF4: model header + 0x18..0x24 */
    /* 0xB4 */ f32 radius;           /* BtlObj + 0x1004: parameter block + 8, or 2 * size[3] */
    /* 0xB8 */ f32 unkB8;            /* 4.1 */
    /* 0xBC */ s32 reset;            /* set by BtlObjBody_Reset */
} BobjBody; /* size 0xC0 */

/* Flash / fade block inside the fighter buffer (work + 0x18000). */
typedef struct BobjFlash {
    /* 0x00 */ Vec4 color;     /* r, g, b 0..255, w = strength (32 or 96) */
    /* 0x10 */ f32 time;       /* 0.2 -> 0 seconds */
    /* 0x14 */ f32 fade;       /* 0..1, +-0.1 per frame after object flag 0x40000 */
    /* 0x18 */ u8 unk18[8];
} BobjFlash; /* size 0x20 */

#define BOBJ_WORK_LAYER_SIZE 0xC000

/* The fighter buffer (BtlObj + 0x1660 -> one of the two 0x1A0C0-byte blocks of the object table). */
typedef struct BobjWork {
    /* 0x00000 */ u8 layer[2][BOBJ_WORK_LAYER_SIZE]; /* the decoded motions of layers 0 and 1 */
    /* 0x18000 */ BobjFlash flash;
    /* 0x18020 */ u32 contact;       /* stage contact word; the top byte is cleared by BtlObjHit_BuildVolumes */
    /* 0x18024 */ u32 prevContact;
    /* 0x18028 */ s32 retarget;      /* non-zero: motions are retargeted to this character when played */
    /* 0x1802C */ u8 unk1802C[0x54];
    /* 0x18080 */ BobjAabb atkBox;   /* bounds of the attack volumes */
    /* 0x18098 */ u8 unk18098[8];
    /* 0x180A0 */ BobjSphere atkSphere[BOBJ_ATK_NODE_MAX];
    /* 0x18300 */ BobjObb atkObb[BOBJ_ATK_NODE_MAX];
    /* 0x1A0B0 */ s32 atkSphereCount;
    /* 0x1A0B4 */ s32 atkObbCount;
    /* 0x1A0B8 */ u8 unk1A0B8[8];
} BobjWork; /* size 0x1A0C0 */

/* Model file header as far as this file reads it. */
typedef struct BobjModelHdr {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ u32 flags;       /* 0xC00000: has a jaw bone; 0x400000 picks the jaw key set */
    /* 0x10 */ u8 unk10[8];
    /* 0x18 */ f32 size[4];
    /* 0x28 */ u8 unk28[0x24];
    /* 0x4C */ u8 faceTex;      /* record of the texture table whose alpha bytes the eye textures copy */
} BobjModelHdr;

/* Object parameter block (BobjMdl.param). */
typedef struct BobjParam {
    /* 0x00 */ f32 unk00;
    /* 0x04 */ f32 height;      /* scale of the vertical root motion */
    /* 0x08 */ f32 radius;      /* body sphere radius; <= 0: use 2 * model size[3] */
} BobjParam;

/* 12-byte vector of the per-node reference tables. */
typedef struct BobjVec3 {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
} BobjVec3;

/* Texture table header (BObjTexTable of bobj_b.h): `count` records of 0x40 bytes at `rec`. */
typedef struct BobjTexTable {
    /* 0x00 */ u32 count;
    /* 0x04 */ u8 unk04[0xC];
    /* 0x10 */ u8 *rec;         /* count x 0x40 bytes; +0x3C = pointer to the colour block */
} BobjTexTable;

/* Model data block of an object (BtlObj + 0x18). */
typedef struct BobjMdl {
    /* 0x000 */ u8 unk00[0x28];
    /* 0x028 */ BobjModelHdr *model;   /* BtlObj + 0x40 */
    /* 0x02C */ BobjBone *nodes;       /* BtlObj + 0x44 */
    /* 0x030 */ BobjTexTable *faceTex; /* BtlObj + 0x48: the model's texture table */
    /* 0x034 */ BobjTexTable *eyeTex;  /* BtlObj + 0x4C: eye texture table (its header is relocated in place) */
    /* 0x038 */ s32 unk38;
    /* 0x03C */ BobjBodyData *body;    /* BtlObj + 0x54: NULL = no body volumes */
    /* 0x040 */ s32 unk40;
    /* 0x044 */ void *mouthTexA[8];    /* BtlObj + 0x5C: mouth texture sets */
    /* 0x064 */ void *mouthTexB[8];    /* BtlObj + 0x7C */
    /* 0x084 */ u8 unk84[0x24];
    /* 0x0A8 */ void *anims[BOBJ_ANIM_MAX]; /* BtlObj + 0xC0: compressed motions */
    /* 0x720 */ BobjAnim *modelAnims[8];    /* BtlObj + 0x738: uncompressed motions of the model file */
    /* 0x740 */ void *lip[100];        /* BtlObj + 0x758: lip tracks (bound by the second part of bobj_a.c) */
    /* 0x8D0 */ u8 unk8D0[0x30];
    /* 0x900 */ BobjVec3 *refPose;     /* BtlObj + 0x918: per node reference translation of this character */
    /* 0x904 */ BobjParam *param;      /* BtlObj + 0x91C */
    /* 0x908 */ u8 unk908[0x30];
} BobjMdl; /* size 0x938 */

/* Object transform (BtlObj + 0x950). */
typedef struct BobjXf {
    /* 0x00 */ Vec4 pos;       /* BtlObj + 0x950 */
    /* 0x10 */ Vec4 rot;       /* BtlObj + 0x960: Euler angles, radians */
    /* 0x20 */ Vec4 rootPos;   /* BtlObj + 0x970: world position of node 0 after BtlObjPose_CalcMatrices */
    /* 0x30 */ Vec4 mtxPos;    /* pos the matrix was built from */
    /* 0x40 */ Vec4 mtxRot;    /* rot the matrix was built from, y turned by pi */
    /* 0x50 */ Mtx44 mtx;      /* BtlObj + 0x9A0 */
    /* 0x90 */ Mtx44 inv;      /* BtlObj + 0x9E0 */
    /* 0xD0 */ f32 scale;      /* BtlObj + 0xA20 */
    /* 0xD4 */ u8 unkD4[0xC];
    /* 0xE0 */ s32 same;       /* BtlObj + 0xA30: ~(rot != mtxRot), i.e. -1 or -2 (sic) */
    /* 0xE4 */ u8 unkE4[0xC];
} BobjXf; /* size 0xF0 */

/* The object as this file sees it. */
typedef struct BobjObj {
    /* 0x0000 */ s32 type;       /* 0 = fighter */
    /* 0x0004 */ u8 unk04[0x14];
    /* 0x0018 */ BobjMdl mdl;
    /* 0x0950 */ BobjXf xf;
    /* 0x0A40 */ u32 flags;      /* BTL_OBJ_FLAG_* (0x40000: the fade value rises) */
    /* 0x0A44 */ u8 unkA44[0xEC];
    /* 0x0B30 */ u8 unkB30;
    /* 0x0B31 */ u8 unkB31[0xF];
    /* 0x0B40 */ BobjAnimPlayer anim;
    /* 0x0C90 */ BobjHit hit;
    /* 0x0D20 */ BobjLook look;
    /* 0x0D58 */ u8 unkD58[0x1F8];
    /* 0x0F50 */ BobjBody body;
    /* 0x1010 */ u8 unk1010[0x650];
    /* 0x1660 */ BobjWork *work;      /* the fighter buffer (BtlObj_GetCharaWork), NULL for other objects */
    /* 0x1664 */ u8 unk1664[0xC];
} BobjObj; /* size 0x1670 */

/* Jaw rotation key and talk pattern step (tables at 0x2C6C20..0x2C6E58). */
typedef struct BobjJawKey {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ u32 frame;
} BobjJawKey; /* size 0x10 */

typedef struct BobjTalkStep {
    /* 0x00 */ s16 frame;
    /* 0x02 */ s16 shape;
} BobjTalkStep;

/* Face state (object + 0x1664; the pool element and its owner are in the second part of bobj_a.c, whose BObjFace this mirrors). */
typedef struct BobjFace {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ s32 blinkWait;   /* updates until the next blink */
    /* 0x0C */ s32 blinkTime;   /* updates the eyes stay shut */
    /* 0x10 */ s32 eye;         /* eye texture: 1 = shut, 0 = the model's own, or the forced number */
    /* 0x14 */ s32 eyeForced;
    /* 0x18 */ s32 mode;        /* mouth mode 0..14 (BOBJ_MOUTH_* of bobj_b.h) */
    /* 0x1C */ s32 shape;       /* mouth texture set, 0 = closed */
    /* 0x20 */ BobjJawKey *jawKeys;
    /* 0x24 */ BobjTalkStep *talkKeys;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ f32 step;        /* 2.0 */
    /* 0x30 */ f32 talkEnd;
    /* 0x34 */ f32 talkTime;
    /* 0x38 */ s32 talkCount;
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ s16 unk40;
    /* 0x42 */ s16 unk42;
    /* 0x44 */ f32 unk44;
    /* 0x48 */ f32 unk48;
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ Vec4 jaw;        /* jaw rotation, Euler angles */
    /* 0x60 */ s32 talkLoops;
} BobjFace;

void BtlObjAnim_Retarget(BobjAnim *anim, BobjObj *obj, BobjObj *other, s32 useTable, s32 useOwn);
void BtlObjAnim_SampleRot(BobjTrack *trk, Quat *out, u16 *cursor, f32 frame);
void BtlObjAnim_SamplePosRot(BobjTrack *trk, Vec4 *pos, Quat *rot, u16 *cursor, f32 frame);
BobjAnim *BtlObjAnim_Load(BobjObj *obj, void *buf, s32 anim, s32 *size);
void BtlObjAnim_Init(BobjObj *obj);
void BtlObjAnim_StartBlend(BobjObj *obj, f32 seconds);
void BtlObjAnim_ZeroRootAxes(BobjAnim *anim, s32 x, s32 y, s32 z);
void BtlObjAnim_RebaseRoot(BobjAnim *anim);
void BtlObjAnim_ClearNodeRot(BobjAnim *anim, s32 node);
void BtlObjAnim_SamplePose(BobjObj *obj);
void BtlObjAnim_UpdateEvents(BobjObj *obj);
void BtlObjAnim_SetStep(BobjObj *obj, f32 step);
void BtlObjAnim_Play(BobjObj *obj, s32 layer, s32 anim, s32 reset);
void BtlObjAnim_PlayFrom(BobjObj *obj, BobjObj *other, s32 anim, s32 useTable);
void BtlObjAnim_PromoteLayer(BobjObj *obj);
void BtlObjAnim_PlayAuto(BobjObj *obj, s32 anim, s32 mode);
void BtlObjAnim_PlayModel(BobjObj *obj, s32 anim, s32 mode);
void BtlObjAnim_Step(BobjObj *obj);
s32 BtlObjAnim_TestEvent(BobjObj *obj, u64 mask);
u32 BtlObjAnim_GetEventArg(BobjObj *obj, u64 mask);
s32 BtlObjAnim_MaskToNode(u32 mask);
s32 BtlObjAnim_QueryEvent(BobjObj *obj, u64 mask, s32 layer, u32 what);
s32 BtlObjAnim_GetMode(BobjObj *obj);
s32 BtlObjBody_Exists(BobjObj *obj);
void BtlObjBody_Swap(BobjBody *body);
void BtlObjObb_SetMtx(BobjObb *box, Mtx44 *m);
void BtlObjBody_SetCenter(Vec4 *dst, Vec4 *src);
void BtlObjBody_UpdateSphere(BobjObj *obj, BobjBody *body);
void BtlObjBody_UpdateParts(BobjBody *body);
void BtlObjBody_Init(BobjObj *obj);
s32 BtlObjBody_Update(BobjObj *obj, s32 parts);
void BtlObjBody_BeginFrame(BobjObj *obj);
void BtlObjBody_Reset(BobjObj *obj);
void BtlObjBody_Warp(BobjObj *obj, Vec4 *pos);
void BtlObjHit_BuildVolumes(BobjObj *obj);
void BtlObjPose_GetNodeMtx(BobjObj *obj, Mtx44 *out, BobjPart *part);
void BtlObjXf_Reset(BobjObj *obj);
void BtlObjXf_Update(BobjObj *obj);
void BtlObjXf_SetMtx(BobjObj *obj, Mtx44 *m);
void BtlObjPose_CalcMatrices(BobjObj *obj);
s32 BtlObj_HasWork(BobjObj *obj);
BobjFlash *BtlObjFlash_Get(BobjObj *obj);
void BtlObjFlash_Clear(BobjObj *obj);
void BtlObjFlash_Reset(BobjObj *obj);
s32 BtlObjFlash_Start(BobjObj *obj, s32 kind);
s32 BtlObjFlash_Step(BobjObj *obj);
s32 BtlObjFlash_GetColor(BobjObj *obj, Vec4 *color, f32 *alpha);
s32 BtlObjFade_Step(BobjObj *obj);
s32 BtlObjFade_Get(BobjObj *obj, f32 *out);
void BtlObj_SetUnkB30(BobjObj *obj, s32 value);
void BtlObjFace_Reset(BobjFace *face, s32 keep);
s32 BtlObjMdl_HasLipTrack(BobjMdl *mdl, u32 index);
void BtlObjMdl_CopyTexAlpha(BobjMdl *mdl, BobjTexTable *src, BobjTexTable *dst);
s32 BtlObjMdl_HasJaw(BobjObj *obj);
void BtlObjFace_PickJawKeys(BobjObj *obj, BobjFace *face);
void BtlObjFace_Nop(BobjFace *face);
void BtlObjFace_Init(BobjObj *obj, BobjFace *face);
void BtlObjFace_StepBlink(BobjObj *obj, BobjFace *face);
s32 BtlObjFace_CanStart(BobjFace *face, s32 mode);
void BtlObjFace_Step(BobjObj *obj, BobjFace *face);
s32 BtlObjMdl_HasMouthSet(BobjMdl *mdl, s32 n);
s32 BtlObjMdl_HasMouth(BobjMdl *mdl);

#endif
