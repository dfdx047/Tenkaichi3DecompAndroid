#include "common.h"
#include "battle/btl_char_cam_int.h"
#include "port/camera.h"
#include "battle/btl_demo_cam.h"

extern int Port_CamSparking(void);
extern f32 BtlUtil_ApproachF(f32 cur, f32 target, f32 step);
extern s32 BtlAnim_GetId(ChrCamChr *chr);
extern s32 BtlMember_IsKiFull(ChrCamChr *chr);
extern s32 BtlMember_HasBlast(ChrCamChr *chr, s32 amount);

#define CAM_KI_ZOOM 1.2f
#define CAM_MAXPOWER_ZOOM 1.4f
#define CAM_KI_PITCH 0.0f
#define CAM_KI_STEP (5.0f / 30.0f)
#define CAM_KI_ANIM_START 0x34
#define CAM_KI_ANIM_LOOP 0x35
#define CAM_KI_ANIM_ID 0x36
#define CAM_KI_FLAG 0xC
#define CAM_BLAST_COST 100000
#define CAM_KI_FACTOR 1.0f
#define CAM_MAXPOWER_FACTOR 2.0f
#define CAM_KI_MIN_FACTOR 1.0f
#define CAM_KI_ZERO_FACTOR 0.0f

static f32 sChrCamKiFactor[6];

/* Clears Sparking camera zoom smoothing at the start of each fight. */
void ChrCam_ResetPortState(void) {
    sChrCamKiFactor[0] = 0.0f;
    sChrCamKiFactor[1] = 0.0f;
    sChrCamKiFactor[2] = 0.0f;
    sChrCamKiFactor[3] = 0.0f;
    sChrCamKiFactor[4] = 0.0f;
    sChrCamKiFactor[5] = 0.0f;
}

/* The fighter's own camera, 0x1C4BF8-0x1C7B30. See battle/btl_char_cam.h for the structures and the frame flow.
 *
 * Callers: BtlChar_UpdateCamera (0x1C2218) runs ChrCam_StartCut, ChrCam_UpdateDemo, ChrCam_UpdateInput;
 * BtlChar_PostScene (0x1C2318) runs ChrCam_Update. Everything else in the fighter code reaches the camera
 * through ChrCam_AddShake, ChrCam_RequestCut, ChrCam_SetCut and ChrCam_EndCut.
 *
 * The code is in three files only for a link reason: ChrCam_CalcCut does not match yet and owns two .lit4
 * constants (0x2FD08C, 0x2FD090) in the middle of the pool (0x2FD03C..0x2FD120), and an object's .lit4 is
 * contiguous. This file is 0x1C4BF8-0x1C4F68 (.lit4 0x2FD03C..0x2FD08C), btl_char_cam_cut.c is ChrCam_CalcCut
 * (0x1C4F68-0x1C5840), btl_char_cam_modes.c the rest (0x1C5840-0x1C7B30, .lit4 0x2FD094..0x2FD120). Once
 * ChrCam_CalcCut matches they can be one file again.
 */

/* Offset of the follow camera from the fighter for a distance preset; returns the pitch. */
f32 ChrCam_GetOffset(ChrCamChr *chr, Vec4 *out, s32 mode) {
    f32 scale;
    f32 dist;
    f32 pitch;
    int sparking = Port_CamSparking();
    f32 kiFactor;

    scale = BtlCharApi_GetHeight(chr->objId);
    pitch = 0.0f;
    dist = scale * 0.5f + 1.0f;
    if (dist < 10.0f) {
        dist = 10.0f;
    }
    switch (mode) {
        case 1:
            out->x = 0.0f;
            if (sparking) {
                out->y = -dist * 1.65f;
                out->z = -dist * 3.08f;
                pitch = (scale * -0.001f + -0.06f) * 0.9f;
            } else {
                out->y = -dist * 1.5f;
                out->z = -dist * 2.8f;
                pitch = scale * -0.001f + -0.06f;
            }
            break;
        case 2:
            out->x = 0.0f;
            if (sparking) {
                out->y = -dist * 2.2f;
                out->z = -dist * 3.63f;
                pitch = (scale * -0.001f + -0.1f) * 0.9f;
            } else {
                out->y = -dist * 2.0f;
                out->z = -dist * 3.3f;
                pitch = scale * -0.001f + -0.1f;
            }
            break;
        case 0:
            out->x = 0.0f;
            if (sparking) {
                out->y = -dist * 1.8f;
                out->z = -dist * 3.6f;
                pitch = (scale * -0.001f + -0.05f) * 0.9f;
            } else {
                out->y = -dist * 1.2f;
                out->z = -dist * 2.4f;
                pitch = scale * -0.001f + -0.05f;
            }
            break;
    }
    if (sparking) {
        f32 targetFactor;
        f32 kiZoomFactor;
        f32 maxPowerFactor;
        f32 zoom;
        s32 anim;

        anim = BtlAnim_GetId(chr);
        if (anim == CAM_KI_ANIM_ID) {
            targetFactor = CAM_MAXPOWER_FACTOR;
        } else if (anim == CAM_KI_ANIM_START || anim == CAM_KI_ANIM_LOOP ||
                   BtlChar_TestFlag(chr, CAM_KI_FLAG) != 0) {
            targetFactor = BtlMember_IsKiFull(chr) != 0 && BtlMember_HasBlast(chr, CAM_BLAST_COST) != 0
                               ? CAM_MAXPOWER_FACTOR
                               : CAM_KI_FACTOR;
        } else {
            targetFactor = CAM_KI_ZERO_FACTOR;
        }
        sChrCamKiFactor[chr->side] = BtlUtil_ApproachF(sChrCamKiFactor[chr->side], targetFactor, CAM_KI_STEP);
        kiFactor = sChrCamKiFactor[chr->side];
        kiZoomFactor = kiFactor < CAM_KI_MIN_FACTOR ? kiFactor : CAM_KI_MIN_FACTOR;
        maxPowerFactor = kiFactor > CAM_KI_MIN_FACTOR ? kiFactor - CAM_KI_MIN_FACTOR : CAM_KI_ZERO_FACTOR;
        zoom = CAM_KI_FACTOR + (CAM_KI_ZOOM - CAM_KI_FACTOR) * kiZoomFactor +
               (CAM_MAXPOWER_ZOOM - CAM_KI_ZOOM) * maxPowerFactor;
        out->y *= zoom;
        out->z *= zoom;
        pitch += CAM_KI_PITCH * kiZoomFactor;
    }
    if (!DemoCam_IsActive() && Port_CameraValue("cam_dist_adapt", 1.0f) != 0.0f) {
        out->z -= Port_CameraValue("base_distance", 0.0f);
    }
    out->y += Port_CameraValue("base_height", 0.0f);
    pitch -= Port_CameraValue("inclination", 0.0f) * 0.0174532925f;
    out->w = 1.0f;
    return pitch;
}

/* Smoothing factor for a camera at this distance from its target: 0.2 + 0.2 * distance / body scale, at most 1. */
f32 ChrCam_CalcRate(ChrCamChr *chr, Vec4 *eye, Vec4 *target) {
    Vec4 d;
    f32 len;
    f32 rate;

    Vec4_Sub(&d, target, eye);
    len = Vec3_Length(&d);
    rate = len / BtlCharApi_GetHeight(chr->objId) * 0.2f + 0.2f;
    if (1.0f < rate) {
        rate = 1.0f;
    }
    return rate;
}

/* Moves cam->rate towards ChrCam_CalcRate (1/75 per frame), then lets the fighter flags override it. */
f32 ChrCam_GetRate(ChrCamChr *chr, Vec4 *eye, Vec4 *target) {
    ChrCam *cam = &chr->cam;

    cam->rate = BtlUtil_ApproachF(cam->rate, ChrCam_CalcRate(chr, eye, target), 0.4f / 30.0f);
    if (BtlChar_TestFlag(chr, 0xCF)) {
        cam->rate = 0.3f;
    } else if (BtlChar_TestFlag(chr, 0xD0)) {
        cam->rate = 0.25f;
    } else if (BtlChar_TestFlag(chr, 0xD1)) {
        cam->rate = 0.2f;
    } else if (BtlChars_IsTimeStopped()) {
        cam->rate = 1.0f;
    }
    return cam->rate;
}

/* Vertical swing of the camera: advances the phase while flag 0xE is up, else lets it die out.
   The C is right but the build needs a prelude fix: gas has to move the `li.s $f14` in front of the unfilled
   `jal BtlUtil_ApproachF` into its delay slot, as the original assembler did (see the report). */
f32 ChrCam_GetBob(ChrCamChr *chr) {
    ChrCam *cam = &chr->cam;

    if (BtlChars_IsTimeStopped() || cam->unk98 == 0) {
        cam->bob = 0.0f;
        return cam->bob;
    }
    if (BtlChar_TestFlag(chr, 0xE)) {
        cam->bob = BtlUtil_WrapAngle(cam->bob + PI / 75.0f);
    } else {
        if (HALF_PI < cam->bob) {
            cam->bob = PI - cam->bob;
        }
        if (cam->bob < -HALF_PI) {
            cam->bob = -PI - cam->bob;
        }
        cam->bob = BtlUtil_ApproachF(cam->bob, 0.0f, PI / 30.0f);
    }
    return Mathf_Sin(cam->bob) * 1.5f;
}

