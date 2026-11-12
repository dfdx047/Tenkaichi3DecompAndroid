#ifndef BATTLE_BTL_DEMO_CAM_H
#define BATTLE_BTL_DEMO_CAM_H

#include "types.h"
#include "battle/btl_cam.h"

/* Scripted ("demo") camera, src/battle/btl_demo_cam.c = 0x23D1E8..0x23E040, the file right before
 * btl_cam.c. One full-screen view driven either by a keyframed camera animation or by a fixed pose.
 * BtlCam_Init / BtlCam_Term / BtlCam_Reset create, free and reset it together with the battle views,
 * and BtlCam_UpdateOverride runs it: while DemoCam_IsActive() it calls DemoCam_Update() last, so the
 * demo camera's matrices are the ones left loaded and the frame is drawn once, full screen.
 *
 * Users: the battle sequence's intro (DemoCam_PlayStageAnim(0..2) for the stage cuts,
 * DemoCam_PlayObjAnim(obj, n) for the fighter entrance cuts), 0x1C6DE0 (fighter: DemoCam_PlayCharAnim0..2),
 * 0x12F5F0 / 0x12F720 (a caller that steps the time itself), 0x12BD58..0x12BDA8 and 0x261ED8 (fixed pose
 * and shake), 0x262A68 / 0x262AF0.
 *
 * Time: animation time is in the animation's own units. With DEMO_CAM_AUTO set, DemoCam_Update adds
 * 2.0 per call unless BATTLE_FLAG_PAUSE is set; at the end (time >= length) it holds the last frame,
 * and with DEMO_CAM_STOP_AT_END also raises DEMO_CAM_STOP, which makes the next DemoCam_IsActive()
 * reset the camera and return 0. No pad is read anywhere in this file.
 */

/* One key of a channel. */
typedef struct DemoCamKey {
    /* 0x00 */ u32 flags;   /* 0x10 = step: hold this key's value until the next key */
    /* 0x04 */ f32 time;
    /* 0x08 */ f32 value;
    /* 0x0C */ u8 unkC[0x14];
} DemoCamKey; /* size 0x20 */

/* One animated value. */
typedef struct DemoCamChannel {
    /* 0x00 */ s32 index;        /* which DemoCamPose word it drives */
    /* 0x04 */ DemoCamKey *keys; /* file offset until DemoCam_FixupAnim */
    /* 0x08 */ s32 keyCount;
    /* 0x0C */ s32 unkC;
} DemoCamChannel; /* size 0x10 */

/* The eight words a camera animation produces; channels address them by index (v). */
typedef union DemoCamPose {
    f32 v[8];
    struct {
        /* 0x00 */ f32 pos[3];
        /* 0x0C */ f32 rot[3]; /* Euler angles, applied X, Y, Z */
        /* 0x18 */ s32 frame;  /* (s32)time of the last key hit exactly */
        /* 0x1C */ f32 unk1C;
    } f;
} DemoCamPose; /* size 0x20 */

/* Camera animation file. */
typedef struct DemoCamAnim {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 needsFixup;        /* non-zero while the pointers below are still file offsets */
    /* 0x08 */ DemoCamChannel *channels;
    /* 0x0C */ s32 channelCount;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ f32 length;            /* DemoCam_GetLength */
    /* 0x18 */ DemoCamPose *defaults; /* the pose before any channel is applied */
    /* 0x1C */ u8 *unk1C;             /* optional block; forced to NULL when version < 2 */
    /* 0x20 */ s32 version;
} DemoCamAnim;

/* DemoCam.flags */
#define DEMO_CAM_PLAYING     1 /* DemoCam_IsActive() returns 1 */
#define DEMO_CAM_STOP        2 /* DemoCam_IsActive() resets the camera and returns 0 */
#define DEMO_CAM_AUTO        4 /* DemoCam_Update advances the time itself */
#define DEMO_CAM_STOP_AT_END 8 /* raise DEMO_CAM_STOP when the time reaches the length */

/* gDemoCam: the block allocated by DemoCam_Init. */
typedef struct DemoCam {
    /* 0x000 */ View view;
    /* 0x260 */ Mtx44 base;         /* the animation is relative to this matrix (identity = world) */
    /* 0x2A0 */ DemoCamPose pose; /* animation output of this frame */
    /* 0x2C0 */ DemoCamAnim *anim;
    /* 0x2C4 */ u8 unk2C4[0xC];
    /* 0x2D0 */ Vec4 fixedPos;    /* pose used while `fixed` is set */
    /* 0x2E0 */ Vec4 fixedRot;
    /* 0x2F0 */ f32 time;
    /* 0x2F4 */ f32 prevTime;     /* min(old time, new time) at the last DemoCam_SetTime */
    /* 0x2F8 */ s32 unk2F8;
    /* 0x2FC */ s32 unk2FC;
    /* 0x300 */ void *obj;        /* object the animation is attached to (base = its matrix), or NULL */
    /* 0x304 */ void *chr;        /* fighter the animation is attached to (base = chr + 0x9A0), or NULL */
    /* 0x308 */ s32 flags;        /* DEMO_CAM_* */
    /* 0x30C */ CamShake shake;
    /* 0x32C */ s32 fixed;        /* 1 = show fixedPos / fixedRot instead of an animation */
    /* 0x330 */ s32 scaleHeight;  /* scale the animated height by the attached fighter's size */
    /* 0x334 */ u8 unk334[0xC];
} DemoCam; /* size 0x340 */

extern DemoCam *gDemoCam;

void DemoCam_FixupAnim(DemoCamAnim *anim);
void DemoCam_EvalAnim(DemoCamPose *out, f32 time);
DemoCam *DemoCam_Get(void);
void DemoCam_Reset(void);
void DemoCam_Init(void);
void DemoCam_Term(void);
void DemoCam_SetAnim(DemoCamAnim *anim);
s32 DemoCam_Update(void);
void DemoCam_SetBase(Mtx44 *base);
f32 DemoCam_GetLength(void);
f32 DemoCam_GetTime(void);
void DemoCam_SetTime(f32 time);
s32 DemoCam_IsActive(void);
s32 DemoCam_IsInUse(void);
void DemoCam_Start(void);
void DemoCam_StartAuto(void);
void DemoCam_StartAutoOnce(void);
void DemoCam_Stop(void);
void DemoCam_SetObj(void *obj);
void DemoCam_SetChr(void *chr);
void DemoCam_AddShake(f32 time);
f32 DemoCam_GetShakeTime(void);
void DemoCam_SetFixedPose(Vec4 *pos, Vec4 *rot);
void DemoCam_GetFixedPose(Vec4 *pos, Vec4 *rot);
void DemoCam_SetScaleHeight(s32 on);
void DemoCam_ClearFixed(void);
s32 DemoCam_IsFixed(void);
void DemoCam_PlayObjAnim(s32 objId, s32 idx);
void DemoCam_PlayCharAnim0(s32 objId);
void DemoCam_PlayCharAnim1(s32 objId);
void DemoCam_PlayCharAnim2(s32 objId);
s32 DemoCam_PlayStageAnim(s32 idx);

#endif
