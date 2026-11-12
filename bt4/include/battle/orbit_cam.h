#ifndef BATTLE_ORBIT_CAM_H
#define BATTLE_ORBIT_CAM_H

#include "types.h"
#include "battle/btl_cam.h"

/* Orbit camera, src/battle/orbit_cam.c = 0x23F620..0x23FB20, the file right after btl_cam.c (what follows it at
 * 0x23FB20 is pointer-fixup code of another module).
 *
 * A camera at `dist` from `target`, looking at it, with yaw/pitch `angles`. OrbitCam_Update(usePad) is the only
 * per-frame function: with usePad != 0 and padLocked == 0 it reads controller port 0 only:
 *     gPad[0].gameLeft[0]   (left stick X)  yaw   += x * 0.1, wrapped to -pi..pi
 *     gPad[0].gameLeft[1]   (left stick Y)  pitch -= y * 0.1, clamped to +-pitchLimit (or -pitchLimit..0)
 *     gPad[0].gameRight[1]  (right stick Y) dist  += z * 0.5, clamped to distMin..distMax
 * each with a dead zone of +-0.1. No button is read. It then builds its own view (View_SetTransform,
 * View_UpdateMatrices, View_Apply) and stores the eye position. Nothing else is written.
 *
 * Callers: 0x25D578 (reset + parameters), 0x25DA58 (update), 0x25DC20 / 0x25DDB0 (init / term): a viewer screen
 * outside the battle code. The battle never calls this file.
 */

/* gOrbitCam: a camera on a sphere around `target`. */
typedef struct OrbitCam {
    /* 0x00 */ Vec4 eye;       /* resulting camera position (output of OrbitCam_Update) */
    /* 0x10 */ Vec4 angles;    /* x = pitch, y = yaw (kept in -pi..pi) */
    /* 0x20 */ Vec4 target;
    /* 0x30 */ f32 dist;
    /* 0x34 */ f32 distMin;
    /* 0x38 */ f32 distMax;
    /* 0x3C */ f32 pitchLimit; /* pitch is kept in -pitchLimit..pitchLimit */
    /* 0x40 */ s32 padLocked;  /* non-zero: OrbitCam_Update ignores the pad */
    /* 0x44 */ s32 floorClamp; /* non-zero: pitch is kept in -pitchLimit..0 */
    /* 0x48 */ u8 unk48[8];
    /* 0x50 */ View view;
} OrbitCam; /* size 0x2B0 */

extern OrbitCam *gOrbitCam;

void OrbitCam_Reset(void);
void OrbitCam_Init(void);
void OrbitCam_Term(void);
void OrbitCam_Update(s32 usePad);
void OrbitCam_SetTarget(f32 x, f32 y, f32 z);
void OrbitCam_SetAngles(f32 pitch, f32 yaw, f32 roll);
void OrbitCam_SetDist(f32 dist);
void OrbitCam_SetDistRange(f32 min, f32 max);
void OrbitCam_SetPitchLimit(f32 limit);
void OrbitCam_GetTarget(Vec4 *out);
void OrbitCam_GetAngles(Vec4 *out);
f32 OrbitCam_GetDist(void);
void OrbitCam_GetDistRange(f32 *min, f32 *max);
f32 OrbitCam_GetPitchLimit(void);
void OrbitCam_ClearAngles(void);
void OrbitCam_SetPadLocked(s32 locked);
void OrbitCam_SetFloorClamp(s32 clamp);

#endif
