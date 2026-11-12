#include "common.h"
extern s32 TagBattle_IsActorLoading(s32 actor);
#include "battle/btl_char_cam_int.h"

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

    scale = BtlCharApi_GetHeight(chr->objId);
    pitch = 0.0f;
    dist = scale * 0.5f + 1.0f;
    if (dist < 10.0f) {
        dist = 10.0f;
    }
    switch (mode) {
        case 1:
            out->x = 0.0f;
            out->y = -dist * 1.5f;
            out->z = -dist * 2.8f;
            pitch = scale * -0.001f + -0.06f;
            break;
        case 2:
            out->x = 0.0f;
            out->y = -dist * 2.0f;
            out->z = -dist * 3.3f;
            pitch = scale * -0.001f + -0.1f;
            break;
        case 0:
            out->x = 0.0f;
            out->y = -dist * 1.2f;
            out->z = -dist * 2.4f;
            pitch = scale * -0.001f + -0.05f;
            break;
    }
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
    } else if (TagBattle_IsActorLoading(chr->side)) {
        cam->rate = 1.0f;
    }
    return cam->rate;
}

/* Vertical swing of the camera: advances the phase while flag 0xE is up, else lets it die out.
   The C is right but the build needs a prelude fix: gas has to move the `li.s $f14` in front of the unfilled
   `jal BtlUtil_ApproachF` into its delay slot, as the original assembler did (see the report). */
f32 ChrCam_GetBob(ChrCamChr *chr) {
    ChrCam *cam = &chr->cam;

    if (TagBattle_IsActorLoading(chr->side) || cam->unk98 == 0) {
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

