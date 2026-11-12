#include "common.h"
#include "battle/orbit_cam.h"
#include "sys/heap.h"
#include "sys/pad.h"

/* Orbit camera, 0x23F620-0x23FB20: a camera on a sphere around a target point, turned and zoomed with pad 0's
 * sticks. See battle/orbit_cam.h.
 *
 * Its .lit4 pool (0x2FE550..0x2FE588) directly follows btl_cam.c's (0x2FE4CC..0x2FE550); nothing in the data says
 * whether the two were one source file. */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);

extern OrbitCam *gOrbitCam;

/* Puts the orbit camera back to its defaults. */
void OrbitCam_Reset(void) {
    memset(gOrbitCam, 0, sizeof(OrbitCam));
    View_InitLayout(&gOrbitCam->view, VIEW_LAYOUT_FULL);
    OrbitCam_SetDist(21.0f);
    OrbitCam_SetDistRange(5.0f, 500.0f);
    OrbitCam_SetPitchLimit(1.5707963f);
    OrbitCam_SetTarget(0.0f, -8.29f, 0.0f);
    OrbitCam_SetAngles(0.0f, 3.14159265f, 0.0f);
}

/* Allocates the orbit camera. */
void OrbitCam_Init(void) {
    gOrbitCam = Heap_Alloc(sizeof(OrbitCam), 0x20, 0, HEAP_ANY);
    OrbitCam_Reset();
}

/* Frees the orbit camera. */
void OrbitCam_Term(void) {
    Heap_Free(gOrbitCam);
    gOrbitCam = NULL;
}

/* Turns and zooms the orbit camera from pad 0's sticks (if allowed), then builds and applies its view. */
void OrbitCam_Update(s32 usePad) {
    Vec4 eye;
    Mtx44 unused;   /* 0x50 bytes of stack the original reserves and never touches */
    Vec4 unused2;
    Pad *p = &gPad[0];
    View *view = &gOrbitCam->view;
    Mtx44 *m;
    f32 max;
    f32 cur;
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;

    if (gOrbitCam->padLocked != 0) {
        usePad = 0;
    }
    if (usePad != 0) {
        if (p->gameLeft[0] > 0.1f || p->gameLeft[0] < -0.1f) {
            x = p->gameLeft[0];
        }
        if (p->gameLeft[1] > 0.1f || p->gameLeft[1] < -0.1f) {
            y = p->gameLeft[1];
        }
        if (p->gameRight[1] > 0.1f || p->gameRight[1] < -0.1f) {
            z = p->gameRight[1];
        }
    }
    gOrbitCam->angles.y += x * 0.1f;
    if (gOrbitCam->angles.y < -3.14159265f) {
        gOrbitCam->angles.y += 6.2831853f;
    }
    if (gOrbitCam->angles.y > 3.14159265f) {
        gOrbitCam->angles.y -= 6.2831853f;
    }
    gOrbitCam->angles.x -= y * 0.1f;
    if (gOrbitCam->floorClamp != 0) {
        if (gOrbitCam->angles.x < -gOrbitCam->pitchLimit) {
            gOrbitCam->angles.x = -gOrbitCam->pitchLimit;
        }
        max = 0.0f;
        cur = gOrbitCam->angles.x;
    } else {
        if (gOrbitCam->angles.x < -gOrbitCam->pitchLimit) {
            gOrbitCam->angles.x = -gOrbitCam->pitchLimit;
        }
        cur = gOrbitCam->angles.x;
        max = gOrbitCam->pitchLimit;
    }
    m = &view->world2view2;
    if (cur > max) {
        gOrbitCam->angles.x = max;
    }
    gOrbitCam->dist += z * 0.5f;
    if (gOrbitCam->dist < gOrbitCam->distMin) {
        gOrbitCam->dist = gOrbitCam->distMin;
    }
    if (gOrbitCam->dist > gOrbitCam->distMax) {
        gOrbitCam->dist = gOrbitCam->distMax;
    }
    eye.x = cosf(gOrbitCam->angles.x) * sinf(gOrbitCam->angles.y);
    eye.y = -sinf(gOrbitCam->angles.x);
    eye.z = cosf(gOrbitCam->angles.x) * cosf(gOrbitCam->angles.y);
    eye.w = 1.0f;
    Vec4_Scale(&eye, &eye, gOrbitCam->dist);
    Vec4_Sub(&eye, &gOrbitCam->target, &eye);
    View_SetTransform(&view->world2view, m, &eye, &gOrbitCam->angles);
    (void)&unused;
    (void)&unused2;
    View_UpdateMatrices(view, &eye);
    View_Apply(view, 1);
    Vec4_Copy(&gOrbitCam->eye, &eye);
}

/* Sets the point the orbit camera looks at. */
void OrbitCam_SetTarget(f32 x, f32 y, f32 z) {
    gOrbitCam->target.x = x;
    gOrbitCam->target.y = y;
    gOrbitCam->target.z = z;
    gOrbitCam->target.w = 1.0f;
}

/* Sets the orbit camera's angles. */
void OrbitCam_SetAngles(f32 pitch, f32 yaw, f32 roll) {
    gOrbitCam->angles.x = pitch;
    gOrbitCam->angles.y = yaw;
    gOrbitCam->angles.z = roll;
    gOrbitCam->angles.w = 1.0f;
}

/* Sets the orbit distance. */
void OrbitCam_SetDist(f32 dist) {
    gOrbitCam->dist = dist;
}

/* Sets the allowed orbit distance range. */
void OrbitCam_SetDistRange(f32 min, f32 max) {
    gOrbitCam->distMin = min;
    gOrbitCam->distMax = max;
}

/* Sets the pitch limit. */
void OrbitCam_SetPitchLimit(f32 limit) {
    gOrbitCam->pitchLimit = limit;
}

/* Copies the target point out. */
void OrbitCam_GetTarget(Vec4 *out) {
    Vec4_Copy(out, &gOrbitCam->target);
}

/* Copies the angles out. */
void OrbitCam_GetAngles(Vec4 *out) {
    Vec4_Copy(out, &gOrbitCam->angles);
}

/* Returns the orbit distance. */
f32 OrbitCam_GetDist(void) {
    return gOrbitCam->dist;
}

/* Returns the orbit distance range. */
void OrbitCam_GetDistRange(f32 *min, f32 *max) {
    *min = gOrbitCam->distMin;
    *max = gOrbitCam->distMax;
}

/* Returns the pitch limit. */
f32 OrbitCam_GetPitchLimit(void) {
    return gOrbitCam->pitchLimit;
}

/* Zeroes pitch and yaw. */
void OrbitCam_ClearAngles(void) {
    gOrbitCam->angles.x = 0.0f;
    gOrbitCam->angles.y = 0.0f;
}

/* Non-zero makes OrbitCam_Update ignore the pad. */
void OrbitCam_SetPadLocked(s32 locked) {
    gOrbitCam->padLocked = locked;
}

/* Non-zero keeps the pitch at or below zero. */
void OrbitCam_SetFloorClamp(s32 clamp) {
    gOrbitCam->floorClamp = clamp;
}
