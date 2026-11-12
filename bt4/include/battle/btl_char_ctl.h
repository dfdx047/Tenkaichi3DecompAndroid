#ifndef BATTLE_BTL_CHAR_CTL_H
#define BATTLE_BTL_CHAR_CTL_H

#include "types.h"
#include "sys/math3d.h"
#include "battle/btl_input.h"

/*
 * Fighter control helpers, 0x1D4F30..0x1D8330, in the tail of btl_input.c and three C files:
 *   src/battle/btl_input.c       0x1D4F30..0x1D60A0  action input queries, stick readers (the tail of that file:
 *                                                    BtlInput_TestAction needs its readers defined above it)
 *   src/battle/btl_char_ctl_b.c  0x1D60A0..0x1D6438  character change request queue, the "time stopped" word
 *   src/battle/btl_char_ctl_c.c  0x1D6438..0x1D70E8  head tracking
 *   src/battle/btl_char_ctl_d.c  0x1D70E8..0x1D8330  pose <-> object, placement, hold attachment, snapshots
 * The structures are partial views local to this module (BtlCtl*); the fighter is 0x1600 bytes.
 */

#define BTLC_DIR_ANY (BTLC_DIR_LEFT | BTLC_DIR_RIGHT | BTLC_DIR_UP | BTLC_DIR_DOWN) /* 0x3C000000 */

/* ---- Character change requests (roster + 0x138, 0x140 bytes) ---- */

#define BTL_CHANGE_RING 8 /* holds 7 requests */

enum {
    BTL_CHANGE_IDLE = 0,
    BTL_CHANGE_STARTED = 1, /* BtlChange_Update made it the active request */
    BTL_CHANGE_TAKEN = 2,   /* BtlChange_SetTaken: the loader started on it */
    BTL_CHANGE_LOADED = 3,  /* BtlChange_SetLoaded: its files are in memory */
    BTL_CHANGE_READY = 4,   /* BtlChange_SetReady: the fighter allows the swap */
    BTL_CHANGE_DONE = 5     /* BtlChange_SetDone: retired by the next BtlChange_Update */
};

enum {
    BTL_CHANGE_KIND_CHARA = 0, /* replace the player's character model */
    BTL_CHANGE_KIND_OBJECT = 1 /* load an extra object */
};

typedef struct BtlChangeReq {
    /* 0x00 */ s32 player;
    /* 0x04 */ s32 kind;       /* BTL_CHANGE_KIND_* */
    /* 0x08 */ union {
        s32 chara;             /* kind 0 */
        s32 id;                /* kind 1 */
    };
    /* 0x0C */ s32 costume;
    /* 0x10 */ s32 variant;
    /* 0x14 */ s32 animChara;  /* -1 for kind 1 */
    /* 0x18 */ s32 unk18;      /* -1 for kind 1 */
    /* 0x1C */ s32 voiceChara; /* -1 for kind 1 */
    /* 0x20 */ s32 slot;       /* kind 1 only; 0 for kind 0 */
} BtlChangeReq; /* 0x24 */

typedef struct BtlChangeQueue {
    /* 0x000 [0x138] */ BtlChangeReq req[BTL_CHANGE_RING];
    /* 0x120 [0x258] */ BtlChangeReq *cur; /* the active request */
    /* 0x124 [0x25C] */ s32 write;
    /* 0x128 [0x260] */ s32 read;
    /* 0x12C [0x264] */ s32 state;         /* BTL_CHANGE_* */
    /* 0x130 [0x268] */ s32 loaded;
    /* 0x134 [0x26C] */ s32 unk134;
    /* 0x138 [0x270] */ s32 unk138;
    /* 0x13C [0x274] */ s32 timeStop;      /* 1 while a request is active (BtlChars_IsTimeStopped) */
} BtlChangeQueue; /* 0x140 */

/* Partial view of the roster (0x280 bytes). */
typedef struct BtlCtlRoster {
    /* 0x000 */ u8 unk0[0x7C];
    /* 0x07C */ s32 unk7C[4];   /* [2] (+0x84): path id for BtlStage_GetPath; [3] (+0x88): point counter (BtlChar_PlaceOnPath) */
    /* 0x08C */ u8 unk8C[0x138 - 0x8C];
    /* 0x138 */ BtlChangeQueue change;
    /* 0x278 */ u8 unk278[8];
} BtlCtlRoster; /* 0x280 */

BtlChangeReq *BtlChange_Alloc(void);
BtlChangeReq *BtlChange_Pop(void);
void BtlChange_Reset(void);
void BtlChange_Update(void);
void BtlChange_RequestChara(s32 player, s32 chara, s32 costume, s32 variant, s32 animChara, s32 unk18, s32 voiceChara);
void BtlChange_RequestObject(s32 player, s32 id, s32 costume, s32 variant, s32 slot);
void BtlChange_SetTaken(void);
void BtlChange_SetLoaded(void);
s32 BtlChange_IsLoadedFor(s32 player);
s32 BtlChange_IsLoaded(void);
s32 BtlChars_IsTimeStopped(void);
s32 BtlChange_GetPlayer(void);
void BtlChange_SetReady(s32 player);
void BtlChange_SetDone(s32 player);

/* ---- Pose, placement, snapshots ---- */

/* The pose block at fighter + 0x10 (BtlChar_GetPos), 0xF0 bytes. Offsets in brackets are fighter offsets. */
typedef struct BtlCtlPose {
    /* 0x00 [0x10] */ Vec4 pos;     /* world position */
    /* 0x10 [0x20] */ Vec4 rot;     /* euler angles; y is the yaw */
    /* 0x20 [0x30] */ Vec4 move;    /* added to pos when the pose goes to the object, subtracted on the way back */
    /* 0x30 [0x40] */ Vec4 unk30;   /* zeroed by a placement */
    /* 0x40 [0x50] */ Vec4 unk40;   /* zeroed by a placement */
    /* 0x50 [0x60] */ Vec4 rootPos; /* translation of model node 0 (animation root motion) */
    /* 0x60 [0x70] */ Vec4 rootRot; /* rotation of model node 0 as euler angles */
    /* 0x70 [0x80] */ Vec4 unk70;
    /* 0x80 [0x90] */ Vec4 dir;     /* (sin yaw, 0, cos yaw, 0) at placement */
    /* 0x90 [0xA0] */ f32 speed;    /* 0 at placement; scaled by cos(rootRot.y) in BtlChar_ApplyRootYaw */
    /* 0x94 [0xA4] */ f32 heading;  /* yaw; rot.y at placement */
    /* 0x98 [0xA8] */ f32 unk98;    /* 0 at placement */
    /* 0x9C [0xAC] */ f32 unk9C;    /* 0 at placement */
    /* 0xA0 [0xB0] */ f32 leanX;    /* negated into the x angle of the object's quaternion at +0xD20 */
    /* 0xA4 [0xB4] */ f32 leanZ;    /* z angle of the same */
    /* 0xA8 [0xB8] */ u8 unkA8[0xF0 - 0xA8];
} BtlCtlPose; /* 0xF0 */

/* A model node as returned by BtlObj_GetNode(obj, index). */
typedef struct BtlCtlNode {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ u32 flags;   /* bit 0 cleared by the head tracking */
    /* 0x08 */ u8 unk8[0x40 - 0x8];
    /* 0x40 */ Vec4 unk40;
    /* 0x50 */ Mtx44 mtx;
    /* 0x90 */ Vec4 pos;
    /* 0xA0 */ Quat rot;
} BtlCtlNode;

/* Character parameter block (obj + 0x940): only the head limits. */
typedef struct BtlCtlParam {
    /* 0x000 */ u8 unk0[0x350];
    /* 0x350 */ u8 headYawMax;   /* degrees */
    /* 0x351 */ u8 headUpMax;    /* degrees */
    /* 0x352 */ u8 headDownMax;  /* degrees */
    /* 0x353 */ s8 neckPitch;    /* degrees */
    /* 0x354 */ s8 headPitch;    /* degrees */
} BtlCtlParam;

/* Partial view of the battle object a fighter drives (BtlChar_GetObj). */
typedef struct BtlCtlObj {
    /* 0x000 */ u8 unk0[0x940];
    /* 0x940 */ BtlCtlParam *param;
    /* 0x944 */ u8 unk944[0x950 - 0x944];
    /* 0x950 */ Vec4 pos;      /* written from the pose by BtlChar_PoseToObj */
    /* 0x960 */ Vec4 rot;
    /* 0x970 */ Vec4 outPos;   /* read back into the pose by BtlChar_ObjToPose */
    /* 0x980 */ u8 unk980[0xA20 - 0x980];
    /* 0xA20 */ f32 scale;     /* copy of fighter + 0x988 */
    /* 0xA24 */ s32 area;      /* what BtlStage_FindZoneNear(-1, pos) returns for the position */
    /* 0xA28 */ u8 unkA28[0xC84 - 0xA28];
    /* 0xC84 */ f32 unkC84;
    /* 0xC88 */ u8 unkC88[0xCB4 - 0xC88];
    /* 0xCB4 */ s32 holdNode;  /* node of this object that is attached to the holder */
    /* 0xCB8 */ u8 unkCB8[0xD20 - 0xCB8];
    /* 0xD20 */ Quat lean;
    /* 0xD30 */ Quat headRot;  /* extra rotation of node 0x2E */
    /* 0xD40 */ Quat neckRot;  /* extra rotation of node 0x2F */
    /* 0xD50 */ f32 headBlend; /* 0..1 weight of headRot */
    /* 0xD54 */ f32 neckBlend;
    /* 0xD58 */ u8 unkD58[0xFA0 - 0xD58];
    /* 0xFA0 */ Vec4 *unkFA0;
} BtlCtlObj;

#define BTL_SNAP_COUNT 6
#define BTL_SNAP_LAST 6 /* slot written by every snapshot */

/* Head tracking state (fighter + 0x15A0). */
typedef struct BtlCtlLook {
    /* 0x00 [0x15A0] */ Vec4 offset;   /* smoothed offset added to the direction of the target */
    /* 0x10 [0x15B0] */ Vec4 dir;      /* smoothed unit direction to look along; w doubles as the first argument of a Vec4_Set */
    /* 0x20 [0x15C0] */ f32 height;
    /* 0x24 [0x15C4] */ s32 enabled;   /* BtlChar_SetLookEnabled */
    /* 0x28 [0x15C8] */ s32 tracking;  /* the target is inside the limits */
    /* 0x2C [0x15CC] */ s32 snap;      /* non-zero: no smoothing */
} BtlCtlLook; /* 0x30 */

/* Partial view of a fighter (0x1600 bytes): what the pose / placement / head tracking code touches. */
typedef struct BtlCtlChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ BtlCtlPose pose;
    /* 0x0100 */ u8 unk100[0x2E0 - 0x100];
    /* 0x02E0 */ Vec4 snapPos[BTL_SNAP_COUNT + 1]; /* position at each BtlChars_Snapshot(n) of this frame; [6] = the latest */
    /* 0x0350 */ Vec4 snapRot[BTL_SNAP_COUNT + 1]; /* the same for the rotation */
    /* 0x03C0 */ u32 snapMask;  /* bit n: snapshot n was taken this frame; bit 6: any */
    /* 0x03C4 */ u8 unk3C4[0x420 - 0x3C4];
    /* 0x0420 */ f32 camEye[3]; /* fighter camera eye (ChrCam) */
    /* 0x042C */ u8 unk42C[0x4A0 - 0x42C];
    /* 0x04A0 */ f32 camYaw;    /* fighter camera yaw */
    /* 0x04A4 */ u8 unk4A4[0x988 - 0x4A4];
    /* 0x0988 */ f32 scale;
    /* 0x098C */ u8 unk98C[0xE70 - 0x98C];
    /* 0x0E70 */ Vec4 relPos;   /* offsets for the flag 0xF6 placement (BtlChar_RequestPlaceRelative) */
    /* 0x0E80 */ Vec4 relRot;
    /* 0x0E90 */ u8 unkE90[0x12A0 - 0xE90];
    /* 0x12A0 */ Vec4 savedPos; /* placement restored by flag 0xF5 */
    /* 0x12B0 */ Vec4 savedRot;
    /* 0x12C0 */ s32 savedArea;
    /* 0x12C4 */ s32 savedFlagF; /* fighter flag 0xF at the time of the save */
    /* 0x12C8 */ s32 savedFlagE; /* fighter flag 0xE */
    /* 0x12CC */ u8 unk12CC[0x1310 - 0x12CC];
    /* 0x1310 */ Vec4 unk1310;  /* copy of *obj->unkFA0 */
    /* 0x1320 */ u8 unk1320[0x1560 - 0x1320];
    /* 0x1560 */ Vec4 warpPos;  /* taken by flag 0xFC */
    /* 0x1570 */ Vec4 warpRot;  /* taken by flag 0xFD */
    /* 0x1580 */ u8 unk1580[0x15A0 - 0x1580];
    /* 0x15A0 */ BtlCtlLook look;
    /* 0x15D0 */ u8 unk15D0[0x1600 - 0x15D0];
} BtlCtlChr; /* 0x1600 */

/* A stage path as returned by BtlStage_GetPath. */
typedef struct BtlCtlPathPoint {
    /* 0x00 */ f32 x, y, z;
    /* 0x0C */ u8 unkC[0x30 - 0xC];
} BtlCtlPathPoint; /* 0x30 */

typedef struct BtlCtlPath {
    /* 0x0 */ s32 count;
    /* 0x4 */ BtlCtlPathPoint *points;
    /* 0x8 */ s32 first;
} BtlCtlPath;

void BtlChar_ResetLook(BtlCtlChr *chr);
void BtlChar_UpdateLookOffset(BtlCtlChr *chr);
void BtlChar_SetLookEnabled(BtlCtlChr *chr, s32 enabled);
void BtlChar_UpdateLook(BtlCtlChr *chr);
void BtlChar_UpdateLookAlt(BtlCtlChr *chr);
s32 BtlChar_UpdateHead(BtlCtlChr *chr);

void BtlChar_ObjToPose(BtlCtlChr *chr);
void BtlChar_PoseToObj(BtlCtlChr *chr, s32 keepRoot);
void BtlChar_ApplyRootYaw(BtlCtlChr *chr);
void BtlChar_UpdateLean(BtlCtlChr *chr);
void BtlChar_Place(BtlCtlChr *chr, Vec4 *pos, Vec4 *rot, s32 area);
void BtlChar_PlaceAtStart(BtlCtlChr *chr);
void BtlChar_PlaceRestart(BtlCtlChr *chr);
void BtlChar_PlaceCenter(BtlCtlChr *chr);
void BtlChar_PlaceRelative(BtlCtlChr *chr);
void BtlChar_PlaceOnPath(BtlCtlChr *chr);
void BtlChar_PlaceCenterHigh(BtlCtlChr *chr);
s32 BtlChar_PlaceSaved(BtlCtlChr *chr);
void BtlChar_PlaceWarp(BtlCtlChr *chr);
f32 BtlChar_GetSpacing(BtlCtlChr *chr, s32 mode);
void BtlChars_UpdateHold(void);
void BtlChar_SavePlacement(BtlCtlChr *chr);
void BtlChar_SetSavedPlacement(BtlCtlChr *chr, Vec4 *pos, Vec4 *rot, Vec4 *unk1310, s32 flagF, s32 flagE);
void BtlChar_SetUnk1310(BtlCtlChr *chr, Vec4 *v);
void BtlChar_ResetUnk1310(BtlCtlChr *chr);
void BtlChar_RequestPlaceRelative(BtlCtlChr *chr, s32 arg1, s32 arg2, s32 self);
void BtlChar_RequestPlaceOnPath(BtlCtlChr *chr);
void BtlChar_ClearSnapshots(BtlCtlChr *chr);
void BtlChar_Snapshot(BtlCtlChr *chr, Vec4 *pos, Vec4 *rot, s32 n);
void BtlChars_Snapshot(s32 n);
void BtlChar_GetSnapPos(BtlCtlChr *chr, Vec4 *out, s32 n);
void BtlChar_GetSnapRot(BtlCtlChr *chr, Vec4 *out, s32 n);
void BtlChar_GetSnapDelta(BtlCtlChr *chr, Vec4 *out, s32 from, s32 to);
void BtlChar_GetMoveSince(BtlCtlChr *chr, Vec4 *out, s32 n);

/* ---- Action input queries ---- */

/*
 * Action input ids of BtlInput_TestAction(chr, id, want). B = button word (BTLB_*), C = command word (BTLC_*);
 * H = held, P = pressed this frame, R = released this frame, !H = not held. "dir" = BTLB_DIR_MASK (any of
 * up / down / left / right). held(n) / notHeld(n) / sincePress(n) are the frame counters of button bit n.
 * All conditions are read from the matching C; the gameplay meaning of each id is not known from this file.
 *
 *   1  !H R3 and H dir                         2  UP, DOWN, LEFT, RIGHT each not held for >= 3 frames
 *   3  !H R3 and C-H DASH_HELD                 4  H dir and (stick more than 45 deg off the facing and dir newly
 *                                                 held, or stick turned more than pi/2 - 0.1 since last frame)
 *   5  H dir, stick > 45 deg off the facing, C-H DASH_HELD
 *   6  H R3, or (C DASH_HELD not held for >= 6 frames and !H dir)
 *   7  !H CHARGE and P DASH                    8, 9  P ASCEND
 *  10  !H DESCEND and H ASCEND                11  !H ASCEND or H DESCEND
 *  12  !H ASCEND and H DESCEND                13  !H DESCEND or H ASCEND
 *  14  !H ASCEND and P DESCEND                15  P ASCEND_TAP2 (double tap)
 *  16  !H ASCEND                              17  P DESCEND_TAP2
 *  18  !H DESCEND                             19  H CHARGE
 *  20  !H dir and C-P DASH_CHARGE_P           21  H dir and C-P DASH_CHARGE_P
 *  22  C-P DASH_P                             23  !H dir and C-H DASH_CHARGE_H
 *  24  H dir and C-H DASH_CHARGE_H
 *  25..28  C-P DASH_P2 and C-P DIR_DOWN / DIR_LEFT / DIR_RIGHT / DIR_UP
 *  29..32  C-P GUARD_H and H DOWN / LEFT / RIGHT / UP
 *  33, 39, 81  C-P GUARD_H                    34  C-H GUARD_H
 *  35  C-H GUARD_H and !H UP and !H DOWN      36, 37  C-H GUARD_H and H UP / H DOWN
 *  38  P UP and P RUSH                        40  H DOWN            41  H UP
 *  42  C-P DASH_P3      43  C-P GUARD_SIDE    44  C-P GUARD_H2
 *  45  H DOWN and held(11 ASCEND) >= 6 and held(12 DESCEND) >= 6
 *  46  C-P LOCKON_P and the previous LOCKON_P press was under 7 frames ago (double tap of command bit 22)
 *  47  H DOWN and P ANY_FACE                  48  H UP and P ANY_FACE
 *  49  H dir and !H CHARGE and C-P GUARD_H    50  C GUARD_H not held
 *  51  C-H DIR_P                              52  C-H RUSH_TAP
 *  53..56  C-P RUSH_HOLD6 and C-H DIR_UP / DIR_DOWN / DIR_LEFT / DIR_RIGHT
 *  57  C-P RUSH_HOLD6 and no C DIR bit held
 *  58..61  C-H RUSH_H and C-H DIR_UP / DIR_DOWN / DIR_LEFT / DIR_RIGHT
 *  62  C-H RUSH_H and no C DIR bit held       63  C-H RUSH_NOT_H
 *  64  !H BLAST and P RUSH                    65  !H RUSH
 *  66  !H (dir or RUSH) and P BLAST           67, 91  !H BLAST
 *  68..71  C-H BLAST_P and C-H DIR_UP / DIR_DOWN / DIR_LEFT / DIR_RIGHT
 *  72, 73  C-H RUSH_P and C-H DIR_LEFT / DIR_RIGHT      74, 75  C-H RUSH_P and C-H DIR_UP / DIR_DOWN
 *  76  C-H RUSH_P and no C DIR bit held
 *  77, 78  !H RUSH and H UP / H DOWN and P BLAST
 *  79  P RUSH           80  P BLAST
 *  82, 83, 84  H UP and P RUSH / P BLAST / C-P GUARD_H
 *  85, 86, 87  H DOWN and P RUSH / P BLAST / C-P GUARD_H
 *  88  !H (CHARGE or RUSH) and BLAST "tap or hold": (H BLAST and sincePress(2) == 6), else when BLAST is not
 *      held: R BLAST and sincePress(2) < 7
 *  89  !H (CHARGE or RUSH) and R BLAST and sincePress(2) < 7     (tap)
 *  90  !H (CHARGE or RUSH) and H BLAST and sincePress(2) == 6    (held for 6 frames)
 *  92  !H CHARGE and C-P DASH_TAP2            93  C-P DASH_P4
 *  94  !H CHARGE and H UP and P BLAST         95  C-H GUARD_H and P BLAST2
 *  96  !H dir and C-P GUARD_H                 97  H RS_LEFT         98  H RS_RIGHT
 *  99..103  notHeld(15 L3R3) >= 30 and R R3 and sincePress(13 R3) < 13, with: 99 !H dir, 100 H LEFT, 101 H UP,
 *      102 H RIGHT, 103 H DOWN                (R3 tapped)
 * 104..106  notHeld(15) >= 30 and H R3 and sincePress(13) == 12, with H LEFT / H UP / H RIGHT   (R3 held 12 frames)
 * 107  P L3R3
 * 108..110  C-H CHARGE_BLAST / CHARGE_BLAST_U / CHARGE_BLAST_D, or fighter flag 0x122 / 0x123 / 0x124 (consumed)
 * 111, 112  C-H SWITCH / C-H UNUSED2000 (never set), or fighter flag 0x11F
 * 113, 114  C-H CHARGE_GUARD / CHARGE_GUARD_U, or fighter flag 0x120 / 0x121 (consumed)
 */
#define BTL_ACTION_INPUT_MAX 114


s32 BtlInput_TestAction(BtlInputChr *chr, s32 id, s32 want);
f32 BtlInput_GetStickX(BtlInputChr *chr);
f32 BtlInput_GetStickY(BtlInputChr *chr);
f32 BtlInput_GetStickLength(BtlInputChr *chr);
f32 BtlInput_GetStickAngle(BtlInputChr *chr);
f32 BtlInput_GetStickTurnFromFacing(BtlInputChr *chr);

#endif
