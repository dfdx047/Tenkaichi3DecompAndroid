#ifndef BATTLE_BOBJ_B_H
#define BATTLE_BOBJ_B_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Battle object, third part: second part of src/battle/bobj_a.c (0x24F1F0..0x250B28) and src/battle/bobj_b_b.c (..0x2527B0).
 * The face (eyes and mouth), the tables an object reads out of its files, the colour mode and part
 * visibility bits, the model node accessors, and the secondary motion chains (hair, tails, capes).
 *
 * The types here are this module's own partial views (prefix BObj): include/battle/btl_obj.h describes the
 * same object for src/battle/btl_obj.c and is not included. Offsets marked (v) are fixed by matching code of
 * this file; the rest is taken from btl_obj.h or from disassembly.
 */

/* ---- files ---------------------------------------------------------------------------------- */

/* One loaded file of a resource slot (BtlResFile of btl_obj.h). The file starts with a table of
   byte offsets; entry n is absent when offset[n] == offset[n + 1]. */
typedef struct BObjFile {
    /* 0x00 */ u32 *buf;
    /* 0x04 */ s32 size;
    /* 0x08 */ s32 id;
    /* 0x0C */ void *orig;
} BObjFile; /* size 0x10 */

/* The resource slot an object is built from (BtlResSlot): model file, two animation files. */
typedef struct BObjSlot {
    /* 0x00 */ BObjFile file[3];
} BObjSlot;

/* Model header, as far as this file reads it. */
typedef struct BObjModel {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ u32 flags;      /* BOBJ_MODEL_* */
    /* 0x0F: the top byte of flags is also read as a signed byte (bit 0 of it = flags bit 24) */
} BObjModel;

/* The same header for the one byte the original reads as a plain char (bit 0 of it is flags bit 24). */
typedef struct BObjModelBytes {
    /* 0x00 */ u8 unk00[0xF];
    /* 0x0F */ char flagsTop;
} BObjModelBytes;

#define BOBJ_MODEL_FADE 0x1            /* distance fade: object flag 8 at init */
#define BOBJ_MODEL_PARTS_A 0x10000     /* the three "part layout" classes BtlObj_IsNodeShown tells apart */
#define BOBJ_MODEL_PARTS_B 0x20000
#define BOBJ_MODEL_PARTS_C 0x40000
#define BOBJ_MODEL_JAW 0xC00000        /* the mouth is a jaw bone (either bit); 0x400000 picks the second table set */
#define BOBJ_MODEL_COLOR_A 0x1000000   /* default colour mode is 0x20000 instead of 0x10000 */

/* One record of the model's bounds list (BtlObjBound of btl_obj.h): one per model node, in skeleton order. */
typedef struct BObjBound {
    /* 0x00 */ s32 next;     /* byte offset to the next record */
    /* 0x04 */ u16 pop;      /* (v) chain builders: how many levels the hierarchy goes back up after this node */
    /* 0x06 */ u16 last;     /* (v) non-zero on the last record */
    /* 0x08 */ u16 enabled;
    /* 0x0A */ u16 node;     /* (v) node id */
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ Vec4 unk10;
    /* 0x20 */ Vec4 unk20;
    /* 0x30 */ Vec4 unk30;
    /* 0x40 */ Vec4 center;
    /* 0x50 */ Vec4 extent;  /* w (+0x5C) is the "size factor" BtlCapi reads */
} BObjBound;

/* Face texture table of a model (object + 0x48 / + 0x4C / mouth sets). */
typedef struct BObjTexEntry {
    /* 0x00 */ u8 unk00[0x40];
} BObjTexEntry;

typedef struct BObjTexTable {
    /* 0x00 */ u32 count;
    /* 0x04 */ u8 unk04[0xC];
    /* 0x10 */ BObjTexEntry *entries;
} BObjTexTable;

/* Lip-sync track: `count` records of {frame, code}; code 0 = closed, anything else = talking. */
typedef struct BObjLipKey {
    /* 0x00 */ u16 frame;
    /* 0x02 */ u16 code;
} BObjLipKey;

typedef struct BObjLipData {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u16 count;
    /* 0x0A */ u8 unk0A[6];
    /* 0x10 */ BObjLipKey keys[1];
} BObjLipData;

/* ---- face ----------------------------------------------------------------------------------- */

/* BObjFace.flags */
#define BOBJ_FACE_EYES 1   /* the model has an eye texture table (object + 0x4C) */
#define BOBJ_FACE_MOUTH 2  /* the model has mouth texture sets or a jaw bone */

/* Mouth modes: second argument of BtlObj_SetSubState, kept in BObjFace.mode. */
enum {
    BOBJ_MOUTH_REST = 0,       /* reset everything */
    BOBJ_MOUTH_CLOSED = 1,     /* stop the current talk loop, keep the mode data */
    BOBJ_MOUTH_LIP = 2,        /* play lip track number `arg` of the object's table (100 lines) */
    BOBJ_MOUTH_TALK = 3,       /* random talk loop */
    BOBJ_MOUTH_TALK_ONCE = 4,  /* one talk pattern, then rest */
    BOBJ_MOUTH_SHAPE_1 = 5,    /* 5..8: fixed mouth shape 1..4 */
    BOBJ_MOUTH_SHAPE_2 = 6,
    BOBJ_MOUTH_SHAPE_3 = 7,
    BOBJ_MOUTH_SHAPE_4 = 8,
    BOBJ_MOUTH_HOLD_5 = 9,     /* 9..12: fixed shape 5..8; these four can only be left through mode 0 */
    BOBJ_MOUTH_HOLD_OPEN = 10, /* shape 6 and the jaw opened by 0.3665 rad (21 degrees) */
    BOBJ_MOUTH_HOLD_7 = 11,
    BOBJ_MOUTH_HOLD_8 = 12,
    BOBJ_MOUTH_TALK_3 = 13,    /* three talk patterns, then rest */
    BOBJ_MOUTH_LIP_PTR = 14    /* play the lip track `arg` points to */
};

/* Face state: an element of the 4-entry pool of btl_obj.c (BtlObjPool70), object + 0x1664. */
typedef struct BObjFace {
    /* 0x00 */ void *link;
    /* 0x04 */ s32 flags;        /* (v) BOBJ_FACE_* */
    /* 0x08 */ s32 blinkWait;    /* frames to the next blink (neighbour file: 90 + rand() % 90) */
    /* 0x0C */ s32 blinkFrames;  /* frames the eyes stay shut (3 + rand() % 3) */
    /* 0x10 */ s32 eye;          /* (v) eye texture number, 1-based; 0 = the model's own */
    /* 0x14 */ u32 eyeForced;    /* (v) value BtlObj_SetEyeFrame asked for */
    /* 0x18 */ s32 mode;         /* (v) BOBJ_MOUTH_* */
    /* 0x1C */ s32 shape;        /* (v) mouth texture set in use, 1-based; 0 = closed */
    /* 0x20 */ void *jawTable;   /* jaw key table picked by BtlObjFace_PickJawKeys (rand() % 4) */
    /* 0x24 */ s16 *talkTable;   /* (v) talk pattern: pairs {frame, shape} */
    /* 0x28 */ BObjLipKey *lip;  /* (v) lip track being played */
    /* 0x2C */ f32 step;         /* (v) frames per update: 2.0 */
    /* 0x30 */ f32 talkEnd;      /* (v) last frame of the talk pattern */
    /* 0x34 */ f32 talkTime;     /* (v) */
    /* 0x38 */ s32 talkCount;    /* (v) records in talkTable */
    /* 0x3C */ s32 lipArg;       /* (v) the `arg` the track was started with */
    /* 0x40 */ u16 lipIndex;     /* (v) current record */
    /* 0x42 */ u16 lipCount;     /* (v) */
    /* 0x44 */ f32 lipTime;      /* (v) */
    /* 0x48 */ f32 lipEnd;       /* (v) frame of the last record */
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ Vec4 jaw;         /* (v) jaw rotation as Euler angles, w = 1 */
    /* 0x60 */ s32 talkLoops;    /* (v) patterns played in mode 13 */
    /* 0x64 */ u8 unk64[0xC];
} BObjFace; /* size 0x70 */

/* ---- model nodes ---------------------------------------------------------------------------- */

/*
 * A model node (bone). The nodes are elements of the 512-entry pool of btl_obj.c (BtlObjPoolE0, 0xE0
 * bytes), linked into the object by the model bind code at 0x113xxx; object + 0xD6C is the table
 * id -> node (NULL for ids the model does not have).
 */
typedef struct BObjNode {
    /* 0x00 */ void *link;
    /* 0x04 */ u32 flags;      /* (v) bit 0 is cleared when a chain overwrites the rotation; 0 at creation */
    /* 0x08 */ s32 id;         /* node id, set when the node is created (0x113478) */
    /* 0x0C */ u8 active;
    /* 0x0D */ u8 unk0D[3];
    /* 0x10 */ Mtx44 world;    /* (v) node to world; row 3 (+0x40) is the world position */
    /* 0x50 */ Mtx44 parent;   /* (v) world matrix of the node's parent as the pose pass left it (the object's own
                                  matrix for the first node); a chain's first link hangs in this frame */
    /* 0x90 */ Vec4 unk90;     /* (v) read from node 0 by BtlObj_SaveNodePositions(obj, 1) */
    /* 0xA0 */ Quat rot;       /* (v) local rotation; the chains write it */
    /* 0xB0 */ u8 unkB0[0x20];
    /* 0xD0 */ Vec4 prevPos;   /* (v) position in the object's frame at the last BtlObj_SaveNodePositions */
} BObjNode; /* size 0xE0 */

#define BOBJ_NODE_MAX 0x80   /* table size is not known from this file; ids up to 0x76 are tested */

/* ---- secondary motion ----------------------------------------------------------------------- */

/*
 * Parameter block of the secondary motion (model file entry 0x17, object + 0x940).
 * Type A (16 slots) and type B (8 slots) chains; a node belongs to slot i when nodeA[i] / nodeB[i] holds its
 * id. Only nodes with id >= 0x47 are looked up.
 */
typedef struct BObjChainParam {
    /* 0x000 */ u8 nodeA[0x10];     /* (v) node id per type A slot */
    /* 0x010 */ f32 axisX[0x10];    /* rest direction of the chain in the node's frame: (x, -y, -z) */
    /* 0x050 */ f32 axisY[0x10];
    /* 0x090 */ f32 axisZ[0x10];
    /* 0x0D0 */ u8 hinge[0x10];     /* 0, 1, 2: hinge axis; other = none */
    /* 0x0E0 */ s16 hingeOfs[0x10]; /* degrees */
    /* 0x100 */ u8 hingeLimit[0x10];/* degrees; 0 = no hinge limit */
    /* 0x110 */ f32 inertia[0x10];
    /* 0x150 */ f32 damping[0x10];  /* interpolation toward the previous offset */
    /* 0x190 */ s8 speed[0x10];     /* degrees per frame added to the idle phase */
    /* 0x1A0 */ s16 phase[0x10];    /* degrees */
    /* 0x1C0 */ u8 swingMax[0x10];  /* degrees */
    /* 0x1D0 */ f32 swing[0x10];
    /* 0x210 */ f32 follow[0x10];   /* quaternion blend factor toward the target */
    /* 0x250 */ u8 limit[0x10];     /* degrees: largest angle from the rest direction */
    /* 0x260 */ f32 rest[0x10];     /* pull toward the rest direction */
    /* 0x2A0 */ f32 gravity[0x10];  /* added to y */
    /* 0x2E0 */ u8 stiff[0x10];     /* percent */
    /* 0x2F0 */ u8 nodeB[8];        /* (v) node id per type B slot */
    /* 0x2F8 */ s8 yaw[8];          /* degrees: direction of the type B chain in the y/z plane */
    /* 0x300 */ f32 gravityB[8];
    /* 0x320 */ f32 swingB[8];
    /* 0x340 */ s8 yawMax[8];       /* degrees */
    /* 0x348 */ s8 yawMin[8];       /* degrees */
} BObjChainParam;

#define BOBJ_CHAIN_A_MAX 16
#define BOBJ_CHAIN_B_MAX 8

/* One link of a chain. Both types use 0x40 bytes; the bytes at +0x20 differ. */
typedef struct BObjLink {
    /* 0x00 */ Quat rot;       /* (v) rotation written to the node last frame */
    /* 0x10 */ Vec4 offset;    /* A: smoothed offset; B: last frame's parent rotation */
    /* 0x20 */ s8 slot;        /* (v) parameter slot, -1 = unused */
    /* 0x21 */ s8 node;        /* (v) node id */
    /* 0x22 */ s8 depthA;      /* (v) A: depth in the hierarchy.  B: id of the previous bounds record's node */
    /* 0x23 */ s8 depthB;      /* (v) B: depth in the hierarchy */
    /* 0x24 */ struct BObjLink *parent; /* (v) link one level up, NULL for the chain's root */
    /* 0x28 */ f32 idle[3];    /* (v) idle sway phases */
    /* 0x34 */ f32 swing[3];   /* (v) swing phases, advanced by BtlObj_ChaosRand */
} BObjLink; /* size 0x40 */


/* The two link tables (object + 0x1010 and + 0x1420). */
typedef struct BObjChainA {
    /* 0x000 */ BObjLink links[BOBJ_CHAIN_A_MAX];
    /* 0x400 */ s32 count;
    /* 0x404 */ u8 pad[0xC];
} BObjChainA; /* size 0x410 */

typedef struct BObjChainB {
    /* 0x000 */ BObjLink links[BOBJ_CHAIN_B_MAX];
    /* 0x200 */ s32 count;
    /* 0x204 */ u8 pad[0xC];
} BObjChainB; /* size 0x210 */

/* ---- the object ----------------------------------------------------------------------------- */

/* The tables an object reads out of its files (object + 0x18). */
typedef struct BObjMdl {
    /* 0x000 */ void *unk00;            /* (v) model file entry 3 */
    /* 0x004 */ void *unk04;            /* (v) entry 0xC */
    /* 0x008 */ u8 unk08[0x20];
    /* 0x028 */ BObjModel *model;       /* object + 0x40 */
    /* 0x02C */ BObjBound *bounds;      /* object + 0x44 */
    /* 0x030 */ BObjTexTable *tex;      /* object + 0x48: the model's texture table */
    /* 0x034 */ BObjTexTable *eyes;     /* (v) entry 0xD: eye textures; NULL = no eyes */
    /* 0x038 */ void *unk38;            /* (v) entry 1, relocated in place */
    /* 0x03C */ void *unk3C;            /* (v) entry 2 */
    /* 0x040 */ void *unk40;            /* (v) animation file 2, entry 6 */
    /* 0x044 */ void *mouthA[8];        /* (v) entries 4..11 */
    /* 0x064 */ void *mouthB[8];        /* (v) [4..7] = entries 0xE..0x11; [0..3] = NULL */
    /* 0x084 */ void *unk84[5];         /* (v) animation file 2, entries 1..5 */
    /* 0x098 */ void *unk98[1];         /* (v) entries 0x29..0x2B run from here ... */
    /* 0x09C */ void *common9C;         /* (v) ... but BtlObj_BindCommonTables then stores common entry 12 here */
    /* 0x0A0 */ void *unkA0;
    /* 0x0A4 */ void *unkA4;            /* (v) entry 0x2D + gProgress->unk00 */
    /* 0x0A8 */ void *anims[0x19E];     /* (v) animation file 1, entries 1..0x19E (object + 0xC0) */
    /* 0x720 */ void *unk720[8];        /* (v) entries 0x1D..0x24 */
    /* 0x740 */ BObjLipData *lip[100];  /* (v) entries 0x35.. (0x99.. for the second voice language) */
    /* 0x8D0 */ void *unk8D0[8];        /* (v) entries 0x1D..0x24 again */
    /* 0x8F0 */ void *unk8F0[4];        /* (v) entries 0x25..0x28 */
    /* 0x900 */ void *common900;        /* (v) common entry 13 */
    /* 0x904 */ s8 *unk904;             /* (v) entry 0x12; byte 3 = colour preset row */
    /* 0x908 */ void *unk908;           /* (v) entry 0x13 */
    /* 0x90C */ void *unk90C;           /* (v) entry 0x14 */
    /* 0x910 */ void *unk910;           /* (v) entry 0x15 */
    /* 0x914 */ void *unk914;           /* (v) entry 0x18 */
    /* 0x918 */ void *unk918;           /* (v) entry 0x19 */
    /* 0x91C */ void *unk91C;           /* (v) entry 0x1B */
    /* 0x920 */ void *unk920;           /* (v) entry 0x1C */
    /* 0x924 */ void *unk924;           /* (v) entry 0x16 */
    /* 0x928 */ BObjChainParam *chain;  /* (v) entry 0x17 (object + 0x940) */
} BObjMdl; /* size 0x92C */

/* BObjState.flags (object + 0xA40): the low byte is part visibility, bits 16..23 the colour mode. */
#define BOBJ_FLAG_VISIBLE 0x2        /* part 0: the whole model */
#define BOBJ_FLAG_PART1 0x10         /* parts 1..4, each only meaningful together with VISIBLE */
#define BOBJ_FLAG_PART2 0x20
#define BOBJ_FLAG_PART3 0x40
#define BOBJ_FLAG_PART4 0x80
#define BOBJ_FLAG_HIDE_A 0x100       /* with model class A: hides nodes 0x23..0x2C and 0x40..0x43 */
#define BOBJ_FLAG_COLOR_MASK 0xFF0000
#define BOBJ_FLAG_SLOW_CHAINS 0x1000000 /* chains use the half-speed constants */
#define BOBJ_FLAG_EYE_FORCED 0x4000000  /* the eye frame is BObjFace.eyeForced, not the blink */
#define BOBJ_FLAG_NODE76 0x40000000  /* with VISIBLE: hides node 0x76 */

/* The same word as bitfields, for the tests the original compiled to one 64-bit load. */
typedef struct BObjFlagBits {
    u32 bit0 : 1;
    u32 visible : 1;
    u32 bit2 : 1;
    u32 bit3 : 1;
    u32 part1 : 1;
    u32 part2 : 1;
    u32 part3 : 1;
    u32 part4 : 1;
    u32 hideA : 1;
    u32 bits9 : 21;
    u32 node76 : 1;
    u32 bit31 : 1;
} __attribute__((aligned(8))) BObjFlagBits;

/* Draw state (object + 0xA40). */
typedef struct BObjState {
    /* 0x00 */ u32 flags;      /* (v) BOBJ_FLAG_* */
    /* 0x04 */ u8 unk04[0xC8];
    /* 0xCC */ f32 preset[5];  /* (v) row of the colour preset table; [4] is BtlObjState.unkDC of btl_obj.h */
    /* 0xE0 */ u8 unkE0[0x14];
    /* 0xF4 */ u32 modeBits;   /* (v) colour mode requests, one bit per requester (BtlObj_SetColorMode) */
} BObjState; /* size 0xF8 */

/* Placement block of the object (object + 0x950, 0xF0 bytes: the size of the fighter's pose block). */
typedef struct BObjPose {
    /* 0x00 */ u8 unk00[0x50];
    /* 0x50 */ Mtx44 world;    /* (v) object + 0x9A0: object to world; the root of the node matrix stack (the pose
                                  pass in the previous file loads it), and what the chains use to turn a node
                                  velocity back into a world direction */
    /* 0x90 */ Mtx44 worldInv; /* (v) object + 0x9E0: world position -> object frame (node position snapshots) */
    /* 0xD0 */ u8 unkD0[0x20];
} BObjPose; /* size 0xF0 */

/* A battle object, as this file reads it (0x1670 bytes). */
typedef struct BObj {
    /* 0x0000 */ s32 type;            /* (v) 0 / 2, 1, other: three sets of initial flags */
    /* 0x0004 */ u8 unk04[0x10];
    /* 0x0014 */ BObjSlot *slot;      /* (v) the resource slot the object was built from */
    /* 0x0018 */ BObjMdl mdl;
    /* 0x0944 */ u8 unk944[0xC];
    /* 0x0950 */ BObjPose pose;       /* (v) */
    /* 0x0A40 */ BObjState state;
    /* 0x0B38 */ u8 unkB38[0x228];
    /* 0x0D60 */ u8 nodeList[0xC];    /* SList of the nodes in bounds-list order (filled at 0x113478) */
    /* 0x0D6C */ BObjNode *nodes[0xA9]; /* (v) id -> node; the size is only bounded by the next field */
    /* 0x1010 */ BObjChainA chainA;   /* (v) */
    /* 0x1420 */ BObjChainB chainB;   /* (v) */
    /* 0x1630 */ Vec4 move;           /* (v) the owner's movement this frame (BtlObj_SetMoveVec) */
    /* 0x1640 */ Vec4 push;           /* (v) push applied to the chains, decays by 0.85 per frame */
    /* 0x1650 */ f32 sway;            /* (v) push along each chain's own axis, decays by 0.85 per frame */
    /* 0x1654 */ f32 chaos;           /* (v) state of the logistic-map generator */
    /* 0x1658 */ s32 pushed;          /* (v) a push was added this frame: skip one decay */
    /* 0x165C */ s32 unk165C;
    /* 0x1660 */ u8 *charaWork;
    /* 0x1664 */ BObjFace *face;      /* (v) NULL when the model has neither eyes nor a mouth */
    /* 0x1668 */ u8 unk1668[8];
} BObj; /* size 0x1670 */

/* face */
void BObjFace_InitMouth(BObj *obj, BObjFace *face);
void BObjFace_StepLipData(BObj *obj, BObjFace *face);
void BObjFace_UpdateMouth(BObj *obj, BObjFace *face);
s32 BtlObj_UpdateFace(BObj *obj); /* declared non-void in the original (no value is returned) */
void BtlObj_CreateFace(BObj *obj);
void BtlObj_FreeFace(BObj *obj);
void BtlObj_SetEyeFrame(BObj *obj, u32 frame);
void BtlObj_SetSubState(BObj *obj, s32 mode, s32 arg);
s32 BtlObj_GetSubState(BObj *obj);
BObjTexEntry *BtlObj_GetFaceTexture(BObj *obj);
s32 BtlObj_HasEyes(BObj *obj);
s32 BtlObj_HasMouth(BObj *obj);
s32 BtlObj_IsJawActive(BObj *obj);
s32 BtlObj_GetJawRot(BObj *obj, Quat *out);
s32 BtlObj_GetMouthMode(BObj *obj);
s32 BtlObj_GetEyeFrame(BObj *obj);

/* file tables, flags */
void *BObjFile_GetEntry(BObjFile *file, s32 n);
void BtlObj_BindTables(BObj *obj);
void BtlObj_InitFlags(BObj *obj);
void BtlObj_SetColorPreset(BObj *obj, s32 row, s32 set);
void BtlObj_BindCommonTables(BObj *obj);
void BtlObj_CopyLipTables(BObj *obj, BObj *src);
void BtlObj_SetColorMode(BObj *obj, s32 bit, s32 on);
s32 BtlObj_GetColorMode(BObj *obj);
void BtlObj_SetPartVisible(BObj *obj, u32 part, s32 on);
s32 BtlObj_IsPartVisible(BObj *obj, u32 part);

/* nodes */
BObjBound *BtlObj_FindBound(BObj *obj, s32 node);
BObjNode *BtlObj_GetNode(BObj *obj, s32 node);
s32 BtlObj_HasNode(BObj *obj, s32 node);
s32 BtlObj_IsNodeShown(BObj *obj, s32 node);
s32 BtlObj_GetNodeSide(s32 node);
void BtlObj_SaveNodePositions(BObj *obj, s32 relative);
void BtlObj_GetNodeVelocity(BObj *obj, s32 node, Vec4 *out);
BObjNode *BtlObj_GetNodeByCode(BObj *obj, u32 code);

/* secondary motion */
void BtlObj_DecayPush(BObj *obj);
void BtlObj_AddPush(BObj *obj, Vec4 *v, f32 max);
void BtlObj_AddSway(BObj *obj, f32 add, f32 max);
void BtlObj_SetMoveVec(BObj *obj, Vec4 *v);
void BtlObj_InitChains(BObj *obj);
void BtlObj_UpdateChains(BObj *obj);
f32 BtlObj_ChaosRand(BObj *obj);
f32 BObjChainB_WrapAngle(f32 a);
void BObjChainB_Step(BObj *obj, BObjLink *link, Mtx44 *parent, Mtx44 *out);
void BObjChainB_Build(BObj *obj);
void BObjChainB_StepAll(BObj *obj);
f32 BObjChainA_WrapAngle(f32 a);
void BObjChainA_Step(BObj *obj, BObjLink *link, Mtx44 *parent, Mtx44 *out);
void BObjChainA_Build(BObj *obj);
void BObjChainA_StepAll(BObj *obj);

#endif
