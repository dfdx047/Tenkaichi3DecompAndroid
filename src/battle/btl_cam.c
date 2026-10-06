#include "common.h"
#include "main.h"
#include "battle/battle.h"
#include "battle/btl_cam.h"
#include "battle/btl_demo_cam.h"
#include "sys/heap.h"
#include "sys/pad.h"

/* Battle camera, 0x23E040-0x23F620. See battle/btl_cam.h for the structures and the per-frame flow. */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);

/* Vector / matrix maths (VU0 macro-mode routines at 0x120098..0x1222xx). */
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec4_SetZeroW1(Vec4 *dst);               /* dst = 0 */
extern void Vec4_SetZero(Vec4 *dst);               /* dst = 0 */
extern f32 Vec3_DistSq(Vec4 *a, Vec4 *b);         /* distance */
extern void Mtx_StoreIdentity(Mtx44 *dst);
extern void Mtx_Mul(Mtx44 *a, Mtx44 *b, Mtx44 *dst); /* matrix product */
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);      /* copy */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);      /* gives the camera axes as rows 0..2 */
extern void Vu0Screen_SetMulMtx(Mtx44 *a, Mtx44 *b);          /* product into VU0 registers ... */
extern void Vu0Screen_StoreMtx(Mtx44 *dst);                /* ... stored here */
extern void Vu0Clip_SetMulMtx(Mtx44 *a, Mtx44 *b);
extern void Vu0Clip_StoreMtx(Mtx44 *dst);
extern void Vu0Screen_LoadMtx(Mtx44 *src);                /* loads the world-to-screen matrix into VU0 registers */
extern void Vu0Clip_LoadMtx(Mtx44 *src);                /* loads the world-to-clip matrix into VU0 registers */
extern void Vu0View_SetRotTrans(Vec4 *rot, Vec4 *trans);  /* rotation + translation matrix into VU0 registers ... */
extern void Vu0View_StoreMtx(Mtx44 *dst);                /* ... stored here */

/* Battle side. */
extern s32 BattleSide_GetObjId(s32 side);                 /* id of the side's fighter object */
extern s32 BattleReplay_IsActive(void);
extern s32 BattleSide_GetControl(s32 side);
extern void BtlCharApi_GetCamPose(s32 objId, Vec4 *pos, Vec4 *rot); /* the fighter camera's pose */
extern s32 BtlCharApi_HasCamPriority(s32 objId);                /* the fighter camera's priority */
extern s32 BtlCharApi_GetMgrUnk134(void);
extern s32 StgCol_TraceSphere(s32 obj, void *seg, Vec4 *hitPos, f32 *frac, void *unk);
extern s32 BtlStage_FindZoneNear(s32 arg0, Vec4 *pos);
extern s32 D_002FF280[2];

extern View gDbgCamView;
extern s32 gDbgCamLocked;

/* Rebuilds the projection matrices of a view from its parameters. */
void View_BuildProjection(View *view) {
    Mtx44 tmp;
    f32 zScale;
    f32 zOfs;
    f32 hw;
    f32 hh;
    f32 ox;
    f32 oy;
    f32 l;
    f32 r;
    f32 t;
    f32 b;
    f32 s;

    zOfs = (-view->zMax * view->nearZ + view->zMin * view->farZ) / (view->farZ - view->nearZ);
    hw = view->nearZ * view->screenSize.x / view->screenDist;
    hh = view->nearZ * view->screenSize.y / view->screenDist;
    zScale = view->farZ * view->nearZ * (view->zMax - view->zMin) / (view->farZ - view->nearZ);
    ox = (view->centerX - 2048.0f) / (view->screenSize.x * view->aspectX);
    oy = (view->centerY - 2048.0f) / (view->screenSize.y * view->aspectY);

    Mtx_StoreIdentity(&view->view2screen);
    view->view2screen.m[0][0] = view->screenDist * view->unk258;
    view->view2screen.m[1][1] = view->screenDist * view->unk258;
    view->view2screen.m[2][2] = 0.0f;
    view->view2screen.m[3][2] = 1.0f;
    view->view2screen.m[2][3] = 1.0f;
    view->view2screen.m[3][3] = 0.0f;
    Mtx_StoreIdentity(&tmp);
    tmp.m[0][0] = view->aspectX;
    tmp.m[1][1] = view->aspectY;
    tmp.m[2][2] = zScale;
    tmp.m[3][0] = view->centerX;
    tmp.m[3][1] = view->centerY;
    tmp.m[3][2] = zOfs;
    Mtx_Mul(&view->view2screen, &tmp, &view->view2screen);

    s = view->nearZ / view->screenDist;
    l = (-256.0f - (view->centerX - 2048.0f)) / view->aspectX * s;
    r = (256.0f - (view->centerX - 2048.0f)) / view->aspectX * s;
    b = (-224.0f - (view->centerY - 2048.0f)) / view->aspectY * -s;
    t = (224.0f - (view->centerY - 2048.0f)) / view->aspectY * -s;
    s = 1.0f / (r - l);
    view->unk1C0.m[0][0] = 2.0f * view->nearZ * s;
    view->unk1C0.m[0][1] = 0.0f;
    view->unk1C0.m[0][2] = (r + l) * s;
    view->unk1C0.m[0][3] = 0.0f;
    s = 1.0f / (b - t);
    view->unk1C0.m[1][0] = 0.0f;
    view->unk1C0.m[1][1] = 2.0f * view->nearZ * s;
    view->unk1C0.m[1][2] = (b + t) * s;
    view->unk1C0.m[1][3] = 0.0f;
    s = 1.0f / (view->farZ - view->nearZ);
    view->unk1C0.m[2][0] = 0.0f;
    view->unk1C0.m[2][1] = 0.0f;
    view->unk1C0.m[2][2] = -view->nearZ * s;
    view->unk1C0.m[2][3] = -(view->farZ * view->nearZ) * s;
    view->unk1C0.m[3][0] = 0.0f;
    view->unk1C0.m[3][1] = 0.0f;
    view->unk1C0.m[3][2] = -1.0f;
    view->unk1C0.m[3][3] = 0.0f;

    Mtx_StoreIdentity(&view->view2clip);
    view->view2clip.m[0][0] = 2.0f * view->nearZ / (2.0f * hw) * view->unk258;
    view->view2clip.m[1][1] = 2.0f * view->nearZ / (2.0f * hh) * view->unk258;
    view->view2clip.m[2][2] = (view->farZ + view->nearZ) / (view->farZ - view->nearZ);
    view->view2clip.m[2][3] = 1.0f;
    view->view2clip.m[3][2] = view->farZ * view->nearZ * -2.0f / (view->farZ - view->nearZ);
    view->view2clip.m[3][3] = 0.0f;
    Mtx_StoreIdentity(&tmp);
    tmp.m[3][0] = ox;
    tmp.m[3][1] = oy;
    Mtx_Mul(&view->view2clip, &tmp, &view->view2clip);

    Mtx_StoreIdentity(&view->unk100);
    view->unk100.m[0][0] = view->screenDist * view->aspectX * hw / view->nearZ;
    view->unk100.m[1][1] = view->screenDist * view->aspectY * hh / view->nearZ;
    view->unk100.m[2][2] = (view->zMin - view->zMax) * 0.5f;
    view->unk100.m[3][0] = 2048.0f;
    view->unk100.m[3][1] = 2048.0f;
    view->unk100.m[3][2] = (view->zMax + view->zMin) * 0.5f;
    view->unk100.m[3][3] = 1.0f;
}

#ifdef PORT
extern f32 Port_WideFactor(void); /* widescreen on PC (port/src/plat_stub.c) */
#endif

/* Stores the projection parameters (the larger aspect factor becomes 1) and rebuilds the projection. */
void View_SetProjection(View *view, Vec4 *screenSize, f32 screenDist, f32 aspectX, f32 aspectY, f32 centerX,
                        f32 centerY, f32 zMin, f32 zMax, f32 nearZ, f32 farZ, f32 unk258) {
#ifdef PORT
    aspectY *= Port_WideFactor(); /* 1, or 4/3 for a 16:9 picture: the same height of view, more to the sides */
#endif
    Vec4_Copy(&view->screenSize, screenSize);
    view->screenDist = screenDist;
    view->centerX = centerX;
    view->centerY = centerY;
    view->zMin = zMin;
    view->zMax = zMax;
    view->nearZ = nearZ;
    view->farZ = farZ;
    view->unk258 = unk258;
    view->aspectX = aspectX;
    view->aspectY = aspectY;
    if (aspectX > 1.0f) {
        view->aspectX = 1.0f;
        view->aspectY = aspectY / aspectX;
    }
    if (view->aspectY > 1.0f) {
        f32 y = view->aspectY;
        f32 x = view->aspectX;
        view->aspectY = 1.0f;
        view->aspectX = x / y;
    }
    View_BuildProjection(view);
}

/* Combines the projections with the view matrix, records the camera position and makes the view current. */
void View_UpdateMatrices(View *view, Vec4 *pos) {
    Vu0Screen_SetMulMtx(&view->view2screen, &view->world2view2);
    Vu0Screen_StoreMtx(&view->world2screen);
    Vu0Clip_SetMulMtx(&view->view2clip, &view->world2view2);
    Vu0Clip_StoreMtx(&view->world2clip);
    Vec4_Copy(&view->pos, pos);
    gBtlCamView = view;
}

/* Builds the world-to-view matrix of a camera at pos with rotation rot into both destinations. */
void View_SetTransform(Mtx44 *dst0, Mtx44 *dst1, Vec4 *pos, Vec4 *rot) {
    Vec4 trans;
    Mtx44 m;

    trans.x = -pos->x;
    trans.y = -pos->y;
    trans.z = -pos->z;
    trans.w = 1.0f;
    rot->w = 1.0f;
    Vu0View_SetRotTrans(rot, &trans);
    Vu0View_StoreMtx(&m);
    Mtx_Copy(dst0, &m);
    Mtx_Copy(dst1, &m);
}

/* Loads the view's combined matrices for drawing, optionally sets its scissor, and makes it current. */
void View_Apply(View *view, s32 scissor) {
    Vu0Screen_LoadMtx(&view->world2screen);
    Vu0Clip_LoadMtx(&view->world2clip);
    if (scissor != 0) {
        View_ApplyScissor((ViewScissor *)view);
    }
    gBtlCamView = view;
}

/* Horizontal (XZ) distance from pos to the current view's camera. */
f32 View_GetDistXZ(Vec4 *pos) {
    Vec4 a;
    Vec4 b;

    Vec4_Set(&a, pos->x, 0.0f, pos->z, 1.0f);
    Vec4_Set(&b, gBtlCamView->pos.x, 0.0f, gBtlCamView->pos.z, 1.0f);
    return Vec3_DistSq(&a, &b);
}

/* Gives a view the projection and scissor of a screen layout (full, left half, right half). */
void View_InitLayout(View *view, s32 layout) {
    Vec4 size;
    s32 split;
    s32 left;

    view->aspect = 1.1666667f;
    switch (layout) {
        case VIEW_LAYOUT_FULL:
            split = 0;
            left = 0;
            break;
        case VIEW_LAYOUT_LEFT:
            split = 1;
            left = 1;
            break;
        case VIEW_LAYOUT_RIGHT:
            split = 1;
            left = 0;
            break;
        default:
            return;
    }
    if (split == 0) {
        Vec4_Set(&size, 2047.0f, 2047.0f, 0.1f, 16777215.0f);
        View_SetProjection(view, &size, 433.0f, 1.0f, view->aspect, 2048.0f, 2048.0f, 1.0f, 10877000.0f, 0.3f,
                           65536.0f, 1.0f);
        view->scissorX0 = 0;
        view->scissorX1 = 511;
        view->scissorY0 = 0;
        view->scissorY1 = 447;
    } else {
        size.x = 1919.0f;
        size.y = 2047.0f;
        size.z = 0.1f;
        size.w = 16777215.0f;
        if (left == 1) {
            View_SetProjection(view, &size, 433.0f, 1.0f, view->aspect, 1920.0f, 2048.0f, 1.0f, 10877000.0f, 0.3f,
                               65536.0f, 1.0f);
            view->scissorX0 = 0;
            view->scissorX1 = 254;
            view->scissorY0 = 0;
            view->scissorY1 = 447;
        } else {
            View_SetProjection(view, &size, 433.0f, 1.0f, view->aspect, 2176.0f, 2048.0f, 1.0f, 10877000.0f, 0.3f,
                               65536.0f, 1.0f);
            view->scissorX0 = 257;
            view->scissorX1 = 511;
            view->scissorY0 = 0;
            view->scissorY1 = 447;
        }
    }
}

/* Copies a layout template over a battle view and records whether it is a half-screen one. */
void BtlCam_SetViewLayout(BtlCamView *view, s32 layout) {
    if (layout != 0) {
        if (layout < 0) {
            return;
        }
        if (layout >= 3) {
            return;
        }
        view->split = 1;
    } else {
        view->split = 0;
    }
    view->view = *(layout + gBtlCam->layouts);
}

/* Builds the three layout templates and gives the two battle views their layout for the screen mode. */
void BtlCam_InitViews(void) {
    s32 i;

    View_InitLayout(&gBtlCam->layouts[0], VIEW_LAYOUT_FULL);
    View_InitLayout(&gBtlCam->layouts[1], VIEW_LAYOUT_LEFT);
    View_InitLayout(&gBtlCam->layouts[2], VIEW_LAYOUT_RIGHT);
    if (Battle_IsSplitScreen()) {
        for (i = 0; i < 2; i++) {
            gBtlCam->views[i].index = i;
            BtlCam_SetViewLayout(&gBtlCam->views[i], i == 0 ? VIEW_LAYOUT_LEFT : VIEW_LAYOUT_RIGHT);
        }
    } else {
        for (i = 0; i < 2; i++) {
            gBtlCam->views[i].index = i;
            BtlCam_SetViewLayout(&gBtlCam->views[i], VIEW_LAYOUT_FULL);
        }
    }
}

/* Turns a battle view's pose into its matrices and optionally loads them for drawing. */
void BtlCam_BuildView(BtlCamView *view, s32 apply) {
    View_SetTransform(&view->view.world2view, &view->view.world2view2, &view->pos, &view->rot);
    View_UpdateMatrices(&view->view, &view->pos);
    if (apply != 0) {
        View_Apply(&view->view, 1);
    }
}

/* Clears the camera block, rebuilds the views, selects view 0 and resets the demo camera. */
void BtlCam_Reset(void) {
    memset(gBtlCam, 0, sizeof(BtlCam));
    BtlCam_InitViews();
    gBtlCam->cur = &gBtlCam->views[0];
    DemoCam_Reset();
}

/* Allocates the camera block and the demo camera, and initialises the battle views. */
void BtlCam_Init(void) {
    gBtlCam = Heap_Alloc(sizeof(BtlCam), 0x20, 0, HEAP_ANY);
    DemoCam_Init();
    BtlCam_Reset();
}

/* Frees the camera block and the demo camera. */
void BtlCam_Term(void) {
    Heap_Free(gBtlCam);
    gBtlCam = NULL;
    DemoCam_Term();
}

/* Per frame: copies one side's fighter camera pose and priority into the selected view and builds it. */
void BtlCam_UpdateView(s32 side) {
    BtlCamView *view = gBtlCam->cur;
    s32 objId = BattleSide_GetObjId(side);

    if (Battle_IsSplitScreen()) {
        BtlCam_SetLayout(1, 0);
    }
    BtlCharApi_GetCamPose(objId, &view->pos, &view->rot);
    view->priority = BtlCharApi_HasCamPriority(objId);
    BtlCam_BuildView(view, 1);
}

/* Segment test from `to` towards `from` against the stage collision: where a camera may stand. */
s32 BtlCam_TraceStage(Vec4 *out, Vec4 *from, Vec4 *to, f32 *frac, s32 *hitObj) {
    Vec4 a;
    Vec4 b;
    struct {
        Vec4 a;
        Vec4 b;
        f32 radius;
    } seg;
    Vec4 hitPos;
    u8 unk[0x40];
    f32 t;
    s32 objA;
    s32 objB;
    s32 obj;
    s32 ret;
    f32 f;

    Vec4_Copy(out, from);
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        if (frac != NULL) {
            *frac = 1.0f;
        }
        return 0;
    }
    Vec4_Copy(&a, to);
    Vec4_Copy(&b, from);
    objA = BtlStage_FindZoneNear(-1, &a);
    objB = BtlStage_FindZoneNear(-1, &b);
    Vec4_Copy(&seg.a, &a);
    Vec4_Copy(&seg.b, &b);
    seg.radius = 2.0f;
    /* The three outcomes are written out separately; a shared `hit:` label allocates registers differently. */
    if (objA != objB) {
        obj = objA;
        if (StgCol_TraceSphere(obj, &seg, &hitPos, &t, unk) != 0) {
            Vec4_Copy(out, &hitPos);
            ret = 1;
            f = t;
        } else {
            obj = objB;
            if (StgCol_TraceSphere(obj, &seg, &hitPos, &t, unk) != 0) {
                Vec4_Copy(out, &hitPos);
                ret = 1;
                f = t;
            } else {
                ret = 0;
                f = 1.0f;
            }
        }
    } else {
        obj = objA;
        if (StgCol_TraceSphere(obj, &seg, &hitPos, &t, unk) != 0) {
            Vec4_Copy(out, &hitPos);
            ret = 1;
            f = t;
        } else {
            f = 1.0f;
            ret = 0;
        }
    }
    if (frac != NULL) {
        *frac = f;
    }
    if (hitObj != NULL) {
        *hitObj = obj;
    }
    return ret;
}

/* Which view to show full screen when neither camera asks for it. */
s32 BtlCam_GetDefaultView(void) {
    if (Battle_GetMode() == 8) {
        if (BattleSide_GetControl(0) != 0) {
            return BattleSide_GetControl(1) == 0 ? 1 : D_002FF280[0];
        }
        return 0;
    }
    if (BattleReplay_IsActive() != 0) {
        return BtlCharApi_GetMgrUnk134();
    }
    return 0;
}

/* Split-screen only: switches the selected view between its half (1) and the full screen (0), rebuilding it. */
void BtlCam_SetLayout(s32 split, s32 apply) {
    if (Battle_IsSplitScreen()) {
        BtlCamView *view = gBtlCam->cur;

        if (split != view->split) {
            if (split == 1) {
                BtlCam_SetViewLayout(view, view->index == 0 ? VIEW_LAYOUT_LEFT : VIEW_LAYOUT_RIGHT);
            } else {
                BtlCam_SetViewLayout(view, VIEW_LAYOUT_FULL);
            }
            BtlCam_BuildView(gBtlCam->cur, apply);
        }
    }
}

/* The view whose camera has the higher priority; on a tie the default view if the priority is > 0, else -1. */
s32 BtlCam_GetPriorityView(void) {
    s32 best = -1;
    s32 bestPri = -9999;
    s32 count = 0;
    s32 i;

    for (i = 0; i < 2; i++) {
        s32 pri = gBtlCam->views[i].priority;

        if (bestPri < pri) {
            bestPri = pri;
            best = i;
            count = 1;
        } else if (pri == bestPri) {
            count++;
        }
    }
    if (count >= 2) {
        if (bestPri > 0) {
            return BtlCam_GetDefaultView();
        }
        return -1;
    }
    return best;
}

/* Makes one of the two battle views the selected and current one. */
void BtlCam_SelectView(s32 idx) {
    gBtlCam->cur = &gBtlCam->views[idx];
    gBtlCamView = &gBtlCam->cur->view;
}

/* Loads the selected view's matrices (and scissor) for drawing. */
void BtlCam_ApplyView(s32 scissor) {
    View_Apply(&gBtlCam->cur->view, scissor);
}

#ifdef PORT
extern int Port_NetView(void); /* port/src/gs/net.c: the local player's view (0 or 1), or -1 */
#endif
/* Decides whether one full-screen view replaces the per-player views this frame; returns 1 if so. */
s32 BtlCam_UpdateOverride(void) {
    s32 ret = 0;
    s32 idx;

    if (!Battle_IsSplitScreen() || BattleReplay_IsActive() != 0) {
        idx = BtlCam_GetPriorityView();
        if (idx < 0) {
            idx = BtlCam_GetDefaultView();
        }
        BtlCam_SelectView(idx);
        ret = 1;
        BtlCam_ApplyView(1);
        BtlCam_SetLayout(0, 1);
    } else {
        idx = BtlCam_GetPriorityView();
#ifdef PORT
        /* Online play: both machines run the two-player game with its two cameras, and each shows only its own
           player's view, full screen. That is this function's own case of "one view has priority", with the
           local player's view taking the place when the game itself gives none priority. */
        if (idx < 0) {
            idx = Port_NetView();
        }
#endif
        if (idx >= 0) {
            BtlCam_SelectView(idx);
            ret = 1;
            BtlCam_SetLayout(0, 1);
        }
    }
    if (DemoCam_IsActive() != 0) {
        DemoCam_Update();
        ret = 1;
    }
    return ret;
}

/* Clears the debug camera's own view and gives it the full-screen layout. */
void DbgCam_Init(void) {
    memset(&gDbgCamView, 0, sizeof(View));
    View_InitLayout(&gDbgCamView, VIEW_LAYOUT_FULL);
}

/* Free-fly camera: the pad's sticks pan (L1 held) or turn and dolly the pose, then the view is built and applied. */
void DbgCam_Update(View *view, Vec4 *pos, Vec4 *rot, s32 pad, f32 speed) {
    Mtx44 axes;
    Vec4 move;
    Vec4 tmp;
    Pad *p = &gPad[pad];
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    s32 pan;

    if (view == NULL) {
        view = &gDbgCamView;
    }
    pan = 0;
    if (p->gameHeld & PADG_L1) {
        pan = 1;
    }
    if (p->gameLeft[0] > 0.1f || p->gameLeft[0] < -0.1f) {
        x = p->gameLeft[0] * 0.5f + x;
    }
    if (p->gameLeft[1] > 0.1f || p->gameLeft[1] < -0.1f) {
        y += p->gameLeft[1] * 0.5f;
    }
    if (p->gameRight[1] > 0.1f || p->gameRight[1] < -0.1f) {
        z += p->gameRight[1] * 0.5f;
    }
    if (gDbgCamLocked != 0) {
        x = 0.0f;
        pan = 0;
        y = 0.0f;
        z = 0.0f;
    }
    memset(&move, 0, sizeof(Vec4));
    memset(&tmp, 0, sizeof(Vec4));
    Mtx_InverseRT(&axes, &view->world2view2);
    if (pan) {
        Vec4_Scale(&tmp, (Vec4 *)axes.m[0], x * speed);
        Vec4_Add(&move, &move, &tmp);
        Vec4_Scale(&tmp, (Vec4 *)axes.m[1], y * speed);
        Vec4_Add(&move, &move, &tmp);
    }
    Vec4_Scale(&tmp, (Vec4 *)axes.m[2], -z * speed);
    Vec4_Add(&move, &move, &tmp);
    Vec4_Add(pos, pos, &move);
    if (!pan) {
        rot->x -= y * 0.04f;
        rot->y += x * 0.04f;
        if (rot->x < -1.57f) {
            rot->x = -1.57f;
        }
        if (rot->x > 1.57f) {
            rot->x = 1.57f;
        }
        if (rot->y < -3.14159265f) {
            rot->y += 6.2831853f;
        }
        if (rot->y > 3.14159265f) {
            rot->y -= 6.2831853f;
        }
    }
    View_SetTransform(&view->world2view, &view->world2view2, pos, rot);
    View_UpdateMatrices(view, pos);
    View_Apply(view, 1);
}

/* Non-zero makes DbgCam_Update ignore the pad. */
void DbgCam_SetLocked(s32 locked) {
    gDbgCamLocked = locked;
}

/* Registers a shake request in the first slot it dominates, unless a slot already dominates it. */
void CamShake_Add(CamShake *shake, f32 strength, f32 time) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (shake->time[i] <= time && shake->strength[i] <= strength) {
            shake->time[i] = time;
            shake->strength[i] = strength;
            return;
        }
        if (time <= shake->time[i] && strength <= shake->strength[i]) {
            break;
        }
    }
}

/* Counts every shake slot down by one frame (1/30 s) and clears the expired ones. */
void CamShake_Tick(CamShake *shake) {
    s32 i;

    for (i = 0; i < 4; i++) {
        shake->time[i] -= 0.033333333f;
        if (shake->time[i] < 0.0f) {
            shake->time[i] = 0.0f;
            shake->strength[i] = 0.0f;
        }
    }
}

/* Produces this frame's random position and rotation offsets from the strongest shake slot. */
void CamShake_Calc(CamShake *shake, Vec4 *posOfs, Vec4 *rotOfs) {
    f32 strength = 0.0f;
    f32 time = 0.0f;
    f32 amp;
    s32 i;

    for (i = 0; i < 4; i++) {
        if (strength < shake->strength[i]) {
            time = shake->time[i];
            strength = shake->strength[i];
        }
    }
    Vec4_SetZeroW1(posOfs);
    Vec4_SetZero(rotOfs);
    if (strength > 0.0f) {
        amp = time * 0.05f * strength;
        if (amp > 0.1f) {
            amp = 0.1f;
        }
        rotOfs->x = ((rand() & 0xFF) - 0x7F) * 0.0078125f * amp;
        rotOfs->y = ((rand() & 0xFF) - 0x7F) * 0.0078125f * amp;
        posOfs->x = ((rand() & 0xFF) - 0x7F) * 0.5f * amp;
        posOfs->y = ((rand() & 0xFF) - 0x7F) * 0.5f * amp;
        posOfs->z = ((rand() & 0xFF) - 0x7F) * 0.5f * amp;
    }
}

/* Time left of the strongest shake slot (0 when none is active). */
f32 CamShake_GetTime(CamShake *shake) {
    f32 time = 0.0f;
    f32 strength = 0.0f;
    s32 i;

    for (i = 0; i < 4; i++) {
        if (strength < shake->strength[i]) {
            time = shake->time[i];
            strength = shake->strength[i];
        }
    }
    return time;
}
