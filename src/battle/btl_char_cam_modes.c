#include "common.h"
#include "battle/btl_char_cam_int.h"
#include "port/camera.h"

/* The fighter camera's modes, per-frame update and cut control, 0x1C5840-0x1C7B30: the last part of
 * btl_char_cam.c (see the note at the top of that file). */

/* Fixed-distance camera (fighter flags 0xDC / 0xB8): beside the fighter, angled by the height of the opponent. */
void ChrCam_CalcFixed(ChrCamChr *chr, Vec4 *eye, Vec4 *rot, Vec4 *target) {
    ChrCam *cam = &chr->cam;
    Vec4 pos;
    Vec4 opp;
    Vec4 toOpp;
    Vec4 dir;
    Vec4 ofs;
    Vec4 out;
    Vec4 ang;
    Vec4 ang2;
    f32 scale;
    f32 len;
    f32 a;
    f32 b;
    f32 horiz;
    f32 yaw;
    f32 camYaw;
    f32 sign;
    f32 s;
    f32 c;
    f32 t;
    f32 y;
    f32 maxPitch;
    f32 pitch;
    f32 k;
    f32 base;

    scale = BtlCharApi_GetHeight(chr->objId);
    Vec4_Copy(&pos, &BtlChar_GetPos(chr)->pos);
    BtlOpp_GetTargetPos(chr, &opp);
    Vec4_Sub(&toOpp, &opp, &pos);
    toOpp.y += BtlOpp_GetHalfHeightDiff(chr);
    len = Vec3_Length(&toOpp);
    if (len < 0.01f) {
        Vec4_Set(&dir, 0.0f, 0.0f, 1.0f, 0.0f);
    } else {
        Vec4_Scale(&dir, &toOpp, 1.0f / len);
    }
    y = -(scale * 0.7f);
    maxPitch = PI * 0.35f;
    k = 0.4f;
    base = -0.1f;
    Vec4_Set(target, 0.0f, y, 0.0f, 1.0f);
    ofs.x = 0.0f;
    ofs.y = -BtlCharApi_GetHeight(chr->objId) * k - 10.0f;
    ofs.z = -35.0f;
    Vec4_Sub(&ofs, &ofs, target);
    yaw = cam->yaw;
    camYaw = yaw;
    if (1.0f < dir.y) {
        dir.y = 1.0f;
    }
    if (dir.y < -1.0f) {
        dir.y = -1.0f;
    }
    a = -Mathf_Asin(dir.y) / maxPitch;
    if (1.0f < a) {
        a = 1.0f;
    }
    if (a < -1.0f) {
        a = -1.0f;
    }
    b = a;
    if (b < 0.0f) {
        b = 0.0f;
    }
    b = b * 2.0f - 1.0f;
    a = -a * __builtin_fabsf(a) + a * 2.0f;
    b = -b * __builtin_fabsf(b) + b * 2.0f;
    pitch = a * maxPitch * k;
    b = b * 0.5f + 0.5f;
    Vec4_Set(&ang, pitch, 0.0f, 0.0f, 0.0f);
    ofs.w = 0.0f;
    Vec4_RotateEuler(&ofs, &ang, &ofs);
    Vec4_Scale(&ofs, &ofs, 1.0f - b * 0.3f);
    base = pitch - b * base + base;
    horiz = BtlUtil_LengthXZ(&toOpp);
    sign = 1.0f;
    if (cam->side < 0.0f) {
        sign = -1.0f;
    }
    Vec4_Set(&ang2, 0.0f, sign * (PI * 0.4f), 0.0f, 0.0f);
    ofs.w = 0.0f;
    Vec4_RotateEuler(&ofs, &ang2, &ofs);
    t = atan2f(-ofs.x, horiz * 0.5f - ofs.z);
    cam->yawOfs = t;
    yaw = BtlUtil_WrapAngle(yaw - t);
    target->x += pos.x;
    target->y += pos.y;
    target->z += pos.z;
    target->w = 1.0f;
    s = Mathf_Sin(camYaw);
    c = Mathf_Cos(camYaw);
    out.x = -c * ofs.x + s * ofs.z;
    out.y = ofs.y;
    out.z = s * ofs.x + c * ofs.z;
    out.w = 1.0f;
    eye->x = target->x + out.x;
    eye->y = target->y + out.y;
    eye->z = target->z + out.z;
    eye->w = 1.0f;
    rot->x = base;
    rot->y = yaw;
    rot->z = 0.0f;
    rot->w = 0.0f;
    eye->y += ChrCam_GetBob(chr);
}

/* Lock-on camera: behind and to one side of the fighter so that both fighters stay in view. */
void ChrCam_CalcLockOn(ChrCamChr *chr, Vec4 *eye, Vec4 *rot, Vec4 *target) {
    ChrCam *cam = &chr->cam;
    Vec4 pos;
    Vec4 opp;
    Vec4 toOpp;
    Vec4 dir;
    Vec4 ofs;
    Vec4 out;
    Vec4 ang;
    Vec4 ang2;
    f32 scale;
    f32 len;
    f32 a;
    f32 b;
    f32 u;
    f32 horiz;
    f32 ofsLen;
    f32 yaw;
    f32 camYaw;
    f32 s;
    f32 c;
    f32 maxPitch;
    f32 pitch;
    f32 base;
    f32 floorY;
    f32 limit;
    f32 width;
    f32 fov;
    f32 half;
    f32 tn;
    f32 spread;
    f32 side;
    f32 turn;
    f32 d;
    f32 m;

    scale = BtlCharApi_GetHeight(chr->objId);
    Vec4_Copy(&pos, &BtlChar_GetPos(chr)->pos);
    floorY = BtlStage_GetBottom() + scale * 0.5f;
    if (floorY < pos.y) {
        pos.y = floorY;
    }
    BtlOpp_GetTargetPos(chr, &opp);
    Vec4_Sub(&toOpp, &opp, &pos);
    toOpp.y += BtlOpp_GetHalfHeightDiff(chr);
    len = Vec3_Length(&toOpp);
    if (len < 0.01f) {
        Vec4_Set(&dir, 0.0f, 0.0f, 1.0f, 0.0f);
    } else {
        Vec4_Scale(&dir, &toOpp, 1.0f / len);
    }
    Vec4_Set(target, 0.0f, -(scale * 0.7f), 0.0f, 1.0f);
    base = ChrCam_GetOffset(chr, &ofs, cam->distMode);
    Vec4_Sub(&ofs, &ofs, target);
    yaw = cam->yaw;
    camYaw = yaw;
    if (len < 0.0f) {
        u = 0.0f;
    } else if (len < 100.0f) {
        u = (len - 0.0f) / 100.0f;
    } else {
        u = 1.0f;
    }
    u = u * 2.0f - 1.0f;
    u = -u * __builtin_fabsf(u) + u * 2.0f;
    u = u * 0.5f + 0.5f;
    if (Port_CameraEnabled()) {
        u = u * 0.3f + 0.85f;
    } else {
        u = u * 0.2f + 0.8f;
    }
    Vec4_Scale(&ofs, &ofs, u);
    Vec4_Scale(target, target, 1.0f - (1.0f - u) * 0.5f);
    maxPitch = PI * 0.35f;
    if (BtlChar_TestFlag(chr, 0xD6)) {
        maxPitch = PI * 0.99f;
    }
    if (1.0f < dir.y) {
        dir.y = 1.0f;
    }
    if (dir.y < -1.0f) {
        dir.y = -1.0f;
    }
    a = -Mathf_Asin(dir.y) / maxPitch;
    if (1.0f < a) {
        a = 1.0f;
    }
    if (a < -1.0f) {
        a = -1.0f;
    }
    b = a;
    if (b < 0.0f) {
        b = 0.0f;
    }
    b = b * 2.0f - 1.0f;
    a = -a * __builtin_fabsf(a) + a * 2.0f;
    b = -b * __builtin_fabsf(b) + b * 2.0f;
    pitch = maxPitch * a * 0.4f;
    b = b * 0.5f + 0.5f;
    Vec4_Set(&ang, pitch, 0.0f, 0.0f, 0.0f);
    ofs.w = 0.0f;
    Vec4_RotateEuler(&ofs, &ang, &ofs);
    Vec4_Scale(&ofs, &ofs, 1.0f - b * 0.3f);
    base = base + (pitch - base * b);
    limit = ChrCam_GetSideLimit(chr);
    horiz = BtlUtil_LengthXZ(&toOpp);
    ofsLen = BtlUtil_LengthXZ(&ofs);
    width = Battle_IsSplitScreen() ? 256.0f : 512.0f;
    fov = Mathf_Atan(width * 0.5f / 433.0f) * 2.0f;
    half = fov * 0.7f;
    tn = Mathf_Tan(half);
    if (0.01f < horiz) {
        spread = Mathf_Atan(tn * (ofsLen / horiz));
    } else {
        spread = HALF_PI;
        if (!(0.0f < tn * ofsLen * horiz)) {
            spread = -HALF_PI;
        }
    }
    d = limit - fov;
    side = cam->side / limit;
    m = spread * d;
    m = m / limit;
    turn = half + m;
    Vec4_Set(&ang2, 0.0f, turn * side, 0.0f, 0.0f);
    ofs.w = 0.0f;
    Vec4_RotateEuler(&ofs, &ang2, &ofs);
    turn = turn + spread * (limit / HALF_PI);
    turn = turn * 0.5f;
    turn = turn * side;
    cam->yawOfs = turn;
    yaw = BtlUtil_WrapAngle(yaw - turn);
    target->x += pos.x;
    target->y += pos.y;
    target->z += pos.z;
    target->w = 1.0f;
    s = Mathf_Sin(camYaw);
    c = Mathf_Cos(camYaw);
    out.x = -c * ofs.x + s * ofs.z;
    out.y = ofs.y;
    out.z = s * ofs.x + c * ofs.z;
    out.w = 1.0f;
    eye->x = target->x + out.x;
    eye->y = target->y + out.y;
    eye->z = target->z + out.z;
    eye->w = 1.0f;
    rot->x = base;
    rot->y = yaw;
    rot->z = 0.0f;
    rot->w = 0.0f;
    eye->y += ChrCam_GetBob(chr);
}

/* Free (no lock-on) camera: behind the fighter at cam->yaw, at the preset distance. */
void ChrCam_CalcFree(ChrCamChr *chr, Vec4 *eye, Vec4 *rot, Vec4 *target) {
    ChrCam *cam = &chr->cam;
    Vec4 pos;
    Vec4 ofs;
    Vec4 look;
    f32 one;
    f32 scale;
    f32 pitch;
    f32 yaw;
    f32 s;
    f32 c;
    ChrCamBody *body;

    body = BtlChar_GetPos(chr);
    one = 1.0f;
    scale = BtlCharApi_GetHeight(chr->objId);
    Vec4_Copy(&pos, &body->pos);
    scale *= 0.7f;
    look.w = one;
    look.x = 0.0f;
    look.z = 0.0f;
    look.y = -scale;
    pitch = ChrCam_GetOffset(chr, &ofs, cam->distMode);
    yaw = cam->yaw;
    s = Mathf_Sin(yaw);
    eye->x = pos.x + s * (ofs.z - look.z) + look.z;
    eye->y = pos.y + (ofs.y - look.y) + look.y;
    c = Mathf_Cos(yaw);
    eye->w = one;
    eye->z = pos.z + c * (ofs.z - look.z) + look.z;
    target->x = look.x + pos.x;
    target->y = look.y + pos.y;
    target->z = look.z + pos.z;
    target->w = one;
    rot->x = pitch;
    rot->y = yaw;
    rot->z = 0.0f;
    rot->w = 0.0f;
    eye->y += ChrCam_GetBob(chr);
}

/* Pushes the eye back onto a vertical plane behind model node 0x30 so the camera never passes it. */
void ChrCam_KeepBehindHead(ChrCamChr *chr, Vec4 *eye, Vec4 *rot) {
    Vec4 plane;
    Vec4 node;
    Vec4 dir;
    Vec4 push;
    f32 one;
    f32 margin;
    f32 d;

    one = 1.0f;
    BtlCharApi_GetNodePos(chr->objId, 0x30, &node);
    margin = BtlCharApi_GetHeight(chr->objId) * 0.15f;
    dir.x = Mathf_Sin(rot->y);
    dir.y = 0.0f;
    dir.z = Mathf_Cos(rot->y);
    plane.x = dir.x;
    plane.y = dir.y;
    plane.z = dir.z;
    dir.w = 0.0f;
    plane.w = -Vec3_Dot(&dir, &node);
    plane.w = plane.w + margin * (Mathf_Cos(rot->x) + one) + 10.0f;
    d = Vec3_Dot(&plane, eye) + plane.w;
    if (0.0f < d) {
        Vec4_Scale(&push, &plane, d);
        Vec4_Sub(eye, eye, &push);
        eye->w = one;
    }
}

/* Lock-on: turns cam->yaw towards the opponent and updates which side of the fighter the camera sits on. */
void ChrCam_UpdateLockOnYaw(ChrCamChr *chr) {
    ChrCam *cam;
    f32 step;
    f32 limit;
    f32 d;

    step = BtlOpp_GetYaw(chr);
    cam = &chr->cam;
    step = BtlUtil_WrapAngle(step - cam->yaw);
    if (!BtlChar_TestFlag(chr, 0xCD)) {
        step *= 0.25f;
    }
    cam->yaw = BtlUtil_WrapAngle(cam->yaw + step);
    cam->yawStep = step;
    if (BtlChar_TestFlag(chr, 0xD2)) {
        return;
    }
    limit = ChrCam_GetSideLimit(chr);
    if (BtlChar_IsFlagRaised(chr, 5)) {
        goto keep;
    }
    if (BtlInput_IsHeld(chr, 0x100)) {
        cam->side = -limit;
    } else if (BtlInput_IsHeld(chr, 0x400)) {
        cam->side = limit;
    } else if (BtlChar_TestFlag(chr, 0xD5)) {
    keep:
        if (0.0f < cam->side) {
            cam->side = limit;
        } else {
            cam->side = -limit;
        }
    } else {
        d = cam->yawStep;
        if (HALF_PI < d) {
            d -= PI;
        }
        if (d < -HALF_PI) {
            d += PI;
        }
        cam->side += d;
        if (cam->side < -limit) {
            cam->side = -limit;
        }
        if (limit < cam->side) {
            cam->side = limit;
        }
    }
}

/* No lock-on: the stick (flag 0xC9) or the fighter's facing (flag 0xCA) drives cam->yaw. */
void ChrCam_UpdateFreeYaw(ChrCamChr *chr) {
    ChrCam *cam = &chr->cam;
    f32 d;

    if (BtlChar_TestFlag(chr, 0xC9)) {
        cam->yaw = BtlUtil_WrapAngle(cam->yaw + BtlInput_GetStickX(chr) * 0.0333333333f);
        if (BtlInput_TestAction(chr, 0x61, 1)) {
            cam->yaw -= PI / 60.0f;
        }
        if (BtlInput_TestAction(chr, 0x62, 1)) {
            cam->yaw += PI / 60.0f;
        }
    } else if (BtlChar_TestFlag(chr, 0xCA)) {
        d = BtlUtil_WrapAngle(BtlChar_GetPos(chr)->yaw - cam->yaw);
        cam->yaw = BtlUtil_WrapAngle(BtlUtil_ApproachF(cam->yaw, cam->yaw + d,
                                               BtlUtil_MinF(Mathf_Sin(__builtin_fabsf(d)) * 0.05f, 0.0333333333f)));
    }
}

/* Camera blocked by the stage: turns the yaw towards the opponent, more the closer it is and the nearer the hit. */
void ChrCam_TurnToOpponent(ChrCamChr *chr, Vec4 *rot) {
    Vec4 camPos;
    Vec4 pos;
    Vec4 opp;
    Vec4 d;
    ChrCam *cam;
    f32 t;
    f32 yaw;
    f32 dist;
    

    Vec4_Copy(&camPos, &chr->cam.pos);
    cam = &chr->cam;
    
    Vec4_Copy(&pos, &BtlChar_GetPos(chr)->pos);
    BtlOpp_GetTargetPos(chr, &opp);
    Vec4_Sub(&d, &opp, &camPos);
    yaw = atan2f(d.x, d.z);
    dist = BtlOpp_GetDistance(chr);
    if (dist < 50.0f) {
        t = 1.0f;
    } else if (dist < 1000.0f) {
        t = 1.0f - (dist - 50.0f) / 950.0f;
    } else {
        t = 0.0f;
    }
    t = t * 2.0f - 1.0f;
    t = -t * __builtin_fabsf(t) + t * 2.0f;
    t = t * 0.5f + 0.5f;
    t *= 1.0f - cam->hitFrac;
    if (0.0f < t) {
        rot->y = BtlUtil_WrapAngle(rot->y + BtlUtil_WrapAngle(yaw - rot->y) * t);
    }
}

/* Camera controls, run in the fighter's control phase: SELECT steps the distance preset, then the yaw update. */
void ChrCam_UpdateInput(ChrCamChr *chr) {
    Vec4 *dst = &chr->cam.unk40;

    if (BtlInput_IsPressed(chr, 0x800000)) {
        chr->cam.distMode++;
        chr->cam.distMode %= CHRCAM_DIST_COUNT;
    }
    if (BtlChar_TestFlag(chr, 5)) {
        ChrCam_UpdateLockOnYaw(chr);
    } else {
        ChrCam_UpdateFreeYaw(chr);
    }
    BtlAnim_GetId(chr);
    Vec4_Copy(dst, *(Vec4 **)((u8 *)BtlChar_GetObj(chr) + 0xFA0));
}

/* Per-frame camera update: picks the mode, adds shake, smooths, and keeps the camera out of the stage. */
void ChrCam_Update(ChrCamChr *chr) {
    ChrCam *cam = &chr->cam;
    Vec4 eye;
    Vec4 rot;
    Vec4 target;
    Vec4 shakePos;
    Vec4 shakeRot;
    Vec4 d;
    s32 keepBehind = 0;
    s32 turn = 0;
    s32 smooth;
    f32 limit;
    f32 len;
    f32 rate;

    if (BtlChar_TestFlag(chr, 0xCC)) {
        Vec4_Copy(&eye, &chr->cam.pos);
        Vec4_Copy(&rot, &chr->cam.rot);
        Vec4_Copy(&target, &chr->cam.target);
        if (BtlChar_TestPrevFlag(chr, 0xD3)) {
            BtlChar_SetFlag(chr, 0xD3);
            goto shake;
        }
    } else if (ChrCam_IsCutActive(chr)) {
        ChrCam_CalcCut(chr, &eye, &rot, &target);
        goto shake;
    } else if (BtlChar_TestFlag(chr, 0xDC) || BtlChar_TestFlag(chr, 0xB8)) {
        ChrCam_CalcFixed(chr, &eye, &rot, &target);
        turn = 1;
    } else if (BtlChar_TestFlag(chr, 5)) {
        ChrCam_CalcLockOn(chr, &eye, &rot, &target);
        keepBehind = 1;
        turn = 1;
    } else {
        ChrCam_CalcFree(chr, &eye, &rot, &target);
        keepBehind = 1;
    }
shake:
    CamShake_Calc(&cam->shake, &shakePos, &shakeRot);
    Vec4_Add(&eye, &eye, &shakePos);
    CamShake_Tick(&cam->shake);
    limit = BtlStage_GetRadius();
    len = BtlUtil_LengthXZ(&eye);
    if (limit < len) {
        f32 k = limit / len;
        eye.x *= k;
        eye.z *= k;
    }
    len = BtlUtil_LengthXZ(&target);
    if (limit < len) {
        f32 k = limit / len;
        target.x *= k;
        target.z *= k;
    }
    smooth = BtlChar_TestFlag(chr, 0xCD) == 0;
    if (BtlChar_TestPrevFlag(chr, 0xCE)) {
        smooth = 0;
    }
    rate = ChrCam_GetRate(chr, &eye, &target);
    rate *= Port_CameraValue("follow_smoothing", 1.0f);
    if (rate > 1.0f) rate = 1.0f;
    if (smooth) {
        Vec4_Lerp(&cam->eye, &eye, &cam->eye, rate);
    } else {
        Vec4_Copy(&cam->eye, &eye);
        cam->rate = ChrCam_CalcRate(chr, &eye, &target);
    }
    Vec4_Copy(&cam->target, &target);
    if (keepBehind) {
        ChrCam_KeepBehindHead(chr, &cam->eye, &rot);
    }
    cam->hit = BtlCam_TraceStage(&cam->pos, &cam->eye, &cam->target, &cam->hitFrac, &cam->hitObj);
    if (cam->hit && turn) {
        ChrCam_TurnToOpponent(chr, &rot);
    }
    if (smooth) {
        Vec4_Sub(&d, &rot, &cam->rot);
        BtlUtil_WrapAngles(&d, &d);
        Vec4_Scale(&d, &d, rate);
        Vec4_Add(&cam->rot, &cam->rot, &d);
        BtlUtil_WrapAngles(&cam->rot, &cam->rot);
    } else {
        Vec4_Copy(&cam->rot, &rot);
    }
    Vec4_Add(&cam->rot, &cam->rot, &shakeRot);
    BtlUtil_WrapAngles(&cam->rot, &cam->rot);
    cam->eye.w = 1.0f;
    cam->rot.w = 0.0f;
    cam->pos.w = 1.0f;
    cam->target.w = 1.0f;
}

/* Largest side angle of the lock-on camera: atan(obj + 0x1000), at least pi/4. */
f32 ChrCam_GetSideLimit(ChrCamChr *chr) {
    f32 a;

    a = Mathf_Atan(BtlCharApi_GetBodyUnk1000(chr->objId));
    if (a < 0.785398163f) {
        a = 0.785398163f;
    }
    return a;
}

/* Requests a camera shake on this fighter's camera (ignored while cam->unk98 is 0). */
void ChrCam_AddShake(ChrCamChr *chr, f32 strength, f32 time) {
    if (chr->cam.unk98 != 0) {
        CamShake_Add(&chr->cam.shake, strength, time);
    }
}

/* Fighter flags 0xDA / 0xD7 / 0xD8 / 0xD9: stop the scripted camera / play one of the character's three camera animations. */
void ChrCam_UpdateDemo(ChrCamChr *chr) {
    if (BtlChar_TestFlag(chr, 0xDA)) {
        DemoCam_Stop();
    }
    if (BtlChar_TestFlag(chr, 0xD7)) {
        DemoCam_PlayCharAnim0(chr->side);
    }
    if (BtlChar_TestFlag(chr, 0xD8)) {
        DemoCam_PlayCharAnim1(chr->side);
    }
    if (BtlChar_TestFlag(chr, 0xD9)) {
        DemoCam_PlayCharAnim2(chr->side);
    }
}

/* Starts a camera cut from explicit (start, delta) pairs. */
void ChrCam_SetCut(ChrCamChr *chr, Vec4 *vecA, Vec4 *vecADelta, Vec4 *vecB, Vec4 *vecBDelta, Vec4 *vecC,
                   Vec4 *vecCDelta, s32 unk88, f32 valA, f32 valADelta, f32 valB, f32 valBDelta, f32 valC,
                   f32 valCDelta, s32 unk8C, s32 unk90, s32 unk94, s32 time, s32 flags) {
    ChrCamCut *cut = &chr->cut;

    Vec4_Copy(&chr->cut.vecA, vecA);
    Vec4_Copy(&chr->cut.vecADelta, vecADelta);
    Vec4_Copy(&chr->cut.vecB, vecB);
    Vec4_Copy(&chr->cut.vecBDelta, vecBDelta);
    Vec4_Copy(&chr->cut.vecC, vecC);
    Vec4_Copy(&chr->cut.vecCDelta, vecCDelta);
    cut->unk88 = unk88;
    cut->unk8C = unk8C;
    cut->unk90 = unk90;
    cut->unk94 = unk94;
    cut->valA = valA;
    cut->valADelta = valADelta;
    cut->valB = valB;
    cut->valBDelta = valBDelta;
    cut->valC = valC;
    cut->valCDelta = valCDelta;
    cut->timer = time;
    cut->total = time;
    cut->flags = flags;
    if (flags & CHRCUT_F_SNAP) {
        BtlChar_SetFlag(chr, 0xCD);
    }
    if (flags & CHRCUT_F_RATE) {
        BtlChar_SetFlag(chr, 0xD1);
    }
}

/* Starts a cut that begins where the camera is now: the running cut's current values, or the live camera. */
void ChrCam_BlendToCut(ChrCamChr *chr, Vec4 *vecADelta, Vec4 *vecBDelta, Vec4 *vecCDelta, s32 unk8C, s32 unk94,
                       f32 valADelta, f32 valBDelta, f32 valCDelta, s32 time, s32 flags) {
    Vec4 vecA;
    Vec4 vecB;
    Vec4 vecC;
    Vec4 dir;
    f32 valA;
    f32 valB;
    f32 valC;
    f32 pitch;
    ChrCamCut *cut = &chr->cut;
    ChrCam *cam;
    Vec4 *pb;
    Vec4 *pc;
    s32 unk88;
    s32 unk90;
    f32 t = 0.0f;

    if (cut->timer > 0 || (cut->flags & CHRCUT_F_HOLD)) {
        if (cut->total > 0) {
            t = 1.0f - (f32)cut->timer / (f32)cut->total;
        }
        Vec4_Scale(&vecA, &chr->cut.vecADelta, t);
        Vec4_Add(&vecA, &chr->cut.vecA, &vecA);
        pb = &vecB;
        Vec4_Scale(pb, &chr->cut.vecBDelta, t);
        Vec4_Add(pb, &chr->cut.vecB, pb);
        pc = &vecC;
        Vec4_Scale(pc, &chr->cut.vecCDelta, t);
        Vec4_Add(pc, &chr->cut.vecC, pc);
        unk88 = cut->unk88;
        valA = cut->valA + cut->valADelta * t;
        valB = cut->valB + cut->valBDelta * t;
        unk90 = cut->unk90;
        valC = cut->valC + cut->valCDelta * t;
    } else {
        cam = &chr->cam;
        unk90 = -1;
        unk88 = -1;
        pb = &vecB;
        valB = cam->rot.x;
        valA = cam->rot.y;
        Vec4_Copy(pb, &chr->cam.target);
        pitch = -valB;
        Vec4_Sub(&dir, &chr->cam.target, &cam->eye);
        valC = Vec3_Length(&dir);
        dir.x = Mathf_Cos(pitch) * Mathf_Sin(valA);
        dir.y = Mathf_Sin(pitch);
        dir.z = Mathf_Cos(pitch) * Mathf_Cos(valA);
        Vec4_Scale(&dir, &dir, valC);
        Vec4_Add(&vecA, &cam->eye, &dir);
        pc = &vecC;
        Vec4_Copy(pc, &vecA);
    }
    flags &= ~9;
    ChrCam_SetCut(chr, &vecA, vecADelta, pb, vecBDelta, pc, vecCDelta, unk88, valA, valADelta, valB, valBDelta,
                  valC, valCDelta, unk8C, unk90, unk94, time, flags);
}

/* Asks for cut `index` of table `table` (0: the character's own, 1: the common one); ChrCam_StartCut acts on it. */
void ChrCam_RequestCut(ChrCamChr *chr, s32 table, s32 index) {
    chr->cut.unk0 = table;
    chr->cut.unk4 = index;
    BtlChar_SetFlag(chr, 0xDB);
}

/* Starts the cut asked for with ChrCam_RequestCut from its table entry. */
void ChrCam_StartCut(ChrCamChr *chr) {
    Vec4 vecA;
    Vec4 vecADelta;
    Vec4 vecB;
    Vec4 vecC;
    Vec4 vecCDelta;
    Vec4 oppRot;
    Vec4 tmpA;
    Vec4 own;
    Vec4 opp;
    Vec4 mid;
    f32 scales[2];
    Vec4 tmpB;
    Vec4 own2;
    Vec4 opp2;
    Vec4 mid2;
    ChrCamCutDef *def = NULL;
    s32 unk88;
    u8 *obj;
    u16 flags;
    s32 unk8C;
    s32 unk90;
    s32 unk94;
    f32 yaw;
    f32 scale;
    f32 pi;
    f32 half;
    f32 valA;
    f32 valADelta;
    f32 valB;
    f32 valBDelta;
    f32 valC;
    f32 valCDelta;
    s32 time;
    s32 cutFlags;

    obj = BtlChar_GetObj(chr);
    if (!BtlChar_TestFlag(chr, 0xDB)) {
        return;
    }
    switch (chr->cut.unk0) {
        case 0:
            def = &(*(ChrCamCutDef **)(obj + 0x938))[chr->cut.unk4];
            break;
        case 1:
            def = &gBtlChars->cutDefs[chr->cut.unk4];
            break;
    }
    if (def == NULL) {
        return;
    }
    flags = def->flags;
    if (flags & CHRCUTDEF_OPP) {
        BtlOpp_GetTargetRot(chr, &oppRot);
        yaw = oppRot.y;
        scale = BtlOpp_GetHeight(chr);
        if (flags & CHRCUTDEF_A_TRACK) {
            Vec4_SetZeroW1(&vecA);
            Vec4_SetZeroW1(&vecADelta);
            unk88 = def->nodeA | CHRCUT_NODE_OPP;
            unk8C = def->nodeA2 | CHRCUT_NODE_OPP;
        } else {
            unk88 = -1;
            BtlOpp_GetNodePos(chr, def->nodeA, &vecA);
            BtlOpp_GetNodePos(chr, def->nodeA2, &tmpA);
            Vec4_Sub(&vecADelta, &tmpA, &vecA);
            unk8C = -1;
        }
        Vec4_Copy(&vecB, *(Vec4 **)((u8 *)BtlOpp_GetObj(chr) + 0xFA0));
    } else if (flags & CHRCUTDEF_MID) {
        yaw = BtlChar_GetPos(chr)->yaw;
        scales[0] = BtlCharApi_GetHeight(chr->objId);
        scales[1] = BtlOpp_GetHeight(chr);
        scale = (scales[1] < scales[0]) ? scales[0] : scales[1];
        unk88 = -1;
        BtlCharApi_GetNodePos(chr->objId, def->nodeA, &own);
        half = 0.5f;
        BtlOpp_GetNodePos(chr, def->nodeA, &opp);
        Vec4_Lerp(&vecA, &own, &opp, half);
        unk8C = -1;
        BtlCharApi_GetNodePos(chr->objId, def->nodeA2, &own);
        BtlOpp_GetNodePos(chr, def->nodeA2, &opp);
        Vec4_Lerp(&mid, &own, &opp, half);
        Vec4_Sub(&vecADelta, &mid, &vecA);
        Vec4_Copy(&vecB, &vecA);
        if (flags & CHRCUTDEF_A_TRACK) {
            Vec4_SetZeroW1(&vecA);
            Vec4_SetZeroW1(&vecADelta);
            unk88 = def->nodeA | CHRCUT_NODE_MID;
            unk8C = def->nodeA2 | CHRCUT_NODE_MID;
        }
    } else {
        yaw = BtlChar_GetPos(chr)->yaw;
        scale = BtlCharApi_GetHeight(chr->objId);
        if (flags & CHRCUTDEF_A_TRACK) {
            Vec4_SetZeroW1(&vecA);
            Vec4_SetZeroW1(&vecADelta);
            unk88 = def->nodeA;
            unk8C = def->nodeA2;
        } else {
            unk88 = -1;
            BtlCharApi_GetNodePos(chr->objId, def->nodeA, &vecA);
            unk8C = -1;
            BtlCharApi_GetNodePos(chr->objId, def->nodeA2, &tmpB);
            Vec4_Sub(&vecADelta, &tmpB, &vecA);
        }
        Vec4_Copy(&vecB, *(Vec4 **)((u8 *)BtlChar_GetObj(chr) + 0xFA0));
    }
    if (flags & CHRCUTDEF_C_OPP) {
        if (flags & CHRCUTDEF_C_TRACK) {
            Vec4_SetZeroW1(&vecC);
            unk90 = def->nodeC | CHRCUT_NODE_OPP;
        } else {
            BtlOpp_GetNodePos(chr, def->nodeC, &vecC);
            unk90 = -1;
        }
    } else if (flags & CHRCUTDEF_C_MID) {
        if (flags & CHRCUTDEF_C_TRACK) {
            Vec4_SetZeroW1(&vecC);
            unk90 = def->nodeC | CHRCUT_NODE_MID;
        } else {
            BtlCharApi_GetNodePos(chr->objId, 3, &own2);
            BtlOpp_GetNodePos(chr, 3, &opp2);
            unk90 = -1;
            Vec4_Lerp(&vecC, &own2, &opp2, 0.5f);
        }
    } else {
        if (flags & CHRCUTDEF_C_TRACK) {
            Vec4_SetZeroW1(&vecC);
            unk90 = def->nodeC;
        } else {
            BtlCharApi_GetNodePos(chr->objId, def->nodeC, &vecC);
            unk90 = -1;
        }
    }
    if (flags & CHRCUTDEF_C2_OPP) {
        if (flags & CHRCUTDEF_C2_TRACK) {
            Vec4_SetZeroW1(&vecCDelta);
            unk94 = def->nodeC2 | CHRCUT_NODE_OPP;
        } else {
            BtlOpp_GetNodePos(chr, def->nodeC2, &own2);
            Vec4_Sub(&vecCDelta, &own2, &vecC);
            unk94 = -1;
        }
    } else if (flags & CHRCUTDEF_C2_MID) {
        if (flags & CHRCUTDEF_C2_TRACK) {
            Vec4_SetZeroW1(&vecCDelta);
            unk94 = def->nodeC2 | CHRCUT_NODE_MID;
        } else {
            BtlCharApi_GetNodePos(chr->objId, 3, &own2);
            BtlOpp_GetNodePos(chr, 3, &opp2);
            unk94 = -1;
            Vec4_Lerp(&mid2, &own2, &opp2, 0.5f);
            Vec4_Sub(&vecCDelta, &mid2, &vecC);
        }
    } else {
        if (flags & CHRCUTDEF_C2_TRACK) {
            Vec4_SetZeroW1(&vecCDelta);
            unk94 = def->nodeC2;
        } else {
            BtlCharApi_GetNodePos(chr->objId, def->nodeC2, &own2);
            unk94 = -1;
            Vec4_Sub(&vecCDelta, &own2, &vecC);
        }
    }
    if (flags & CHRCUTDEF_MIN_SCALE) {
        scale = BtlUtil_MaxF(scale, 15.0f);
    }
    pi = PI;
    valA = BtlUtil_WrapAngle(yaw + def->yaw * pi / 180.0f);
    valADelta = def->yawDelta * pi / 180.0f;
    valB = def->pitch * pi / 180.0f;
    valBDelta = def->pitchDelta * pi / 180.0f;
    valC = def->dist * scale;
    valCDelta = def->distDelta * scale;
    time = def->seconds * 30.0f;
    if (time <= 0) {
        time = 1;
    }
    cutFlags = def->cutFlags;
    if (flags & CHRCUTDEF_BLEND) {
        ChrCam_BlendToCut(chr, &vecADelta, (Vec4 *)&D_002EC2A0, &vecCDelta, unk8C, unk94, valADelta, valBDelta,
                          valCDelta, time, cutFlags);
    } else {
        ChrCam_SetCut(chr, &vecA, &vecADelta, &vecB, (Vec4 *)&D_002EC2A0, &vecC, &vecCDelta, unk88, valA,
                      valADelta, valB, valBDelta, valC, valCDelta, unk8C, unk90, unk94, time, cutFlags);
    }
}

/* Ends the running cut, raising the flags its end bits ask for. */
void ChrCam_EndCut(ChrCamChr *chr) {
    ChrCamCut *cut = &chr->cut;

    if (cut->flags & CHRCUT_F_SNAP_END) {
        BtlChar_SetFlag(chr, 0xCD);
    }
    if (cut->flags & CHRCUT_F_RATE_END) {
        BtlChar_SetFlag(chr, 0xD1);
    }
    cut->timer = 0;
    cut->flags = 0;
}

/* Whether a cut owns the camera this frame: one is running, held, or was just requested. */
s32 ChrCam_IsCutActive(ChrCamChr *chr) {
    ChrCamCut *cut = &chr->cut;

    if (cut->timer > 0 || (cut->flags & CHRCUT_F_HOLD)) {
        return 1;
    }
    return BtlChar_TestFlag(chr, 0xDB) != 0;
}
