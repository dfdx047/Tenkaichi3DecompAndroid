extern int TagBattle_Active(void);
#include "common.h"
#include <stdio.h>
#include "main.h"
#include "battle/battle.h"
#include "battle/btl_cam.h"
#ifdef PORT
#include "battle/stg_a.h" /* (script mods' camera: the stage) */
#endif
#include "battle/btl_demo_cam.h"
#include "battle/tag_team_native.h"
#include "sys/heap.h"
#include "sys/pad.h"
#ifdef PORT
#include "../../port/src/port_host.h"
#include "../../port/src/gs/presentation.h"
#endif

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
#ifdef PORT
PORT_HOST static BtlCamView sPortFreeCam;
PORT_HOST static Vec4 sPortFreeCamPos, sPortFreeCamRot;
PORT_HOST static s32 sPortFreeCamLive;
#endif

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
#ifdef PORT
    {
        /* PC build: the picture's shape can be changed while the game runs (the settings window), and a view's
           projection is only worked out when the view is set up, at the start of a fight or of a scene: changed in
           a fight, the picture was shown at the new shape with the old projection, stretched, until the next
           fight. The view is held to the shape of now: its two factors as View_SetProjection makes them of
           `aspect` (the larger becomes 1), and the projection again when they are not those. With the shape
           unchanged the factors are the same numbers and nothing is done. */
        f32 y = view->aspect * Port_WideFactor();
        f32 x = 1.0f;
        f32 dx, dy;
        if (view->aspect > 0.0f) {
            if (y > 1.0f) {
                x = 1.0f / y;
                y = 1.0f;
            }
            dx = view->aspectX - x;
            dy = view->aspectY - y;
            if (dx > 0.0005f || dx < -0.0005f || dy > 0.0005f || dy < -0.0005f) {
                view->aspectX = x;
                view->aspectY = y;
                View_BuildProjection(view);
            }
        }
    }
#endif
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

#ifdef PORT
/* A camera mod's zoom, kept in the view's own projection (view2screen, view2clip): the passes that build their
   transform from the view's parts (the ground shadows, Vu1Pkt_LoadProg6: view2screen * world2view2) then agree with
   the picture; zoomed only for the moment of View_UpdateMatrices, the shadows were drawn with the game's projection
   and a moved camera, in the wrong place. The projection without the zoom is kept here, per view, and the zoom put
   on it again each frame (never on itself). A projection the game made anew (a new picture shape, a split-screen
   layout copied over the view) is recognised by not being the zoomed one, and becomes the base. Returns 1 when
   the view's projection was replaced since the last call (the caller zooms and builds again). */
static Mtx44 sPortBase[2][2], sPortZoomed[2][2];
static s32 sPortZoomOn[2];

static s32 Mtx_PortSame(Mtx44 *a, Mtx44 *b) {
    s32 i;
    for (i = 0; i < 16; i++) {
        if (((f32 *)a)[i] != ((f32 *)b)[i]) {
            return 0;
        }
    }
    return 1;
}

static s32 BtlCam_PortZoom(BtlCamView *view, s32 zoom) {
    s32 k = view->index & 1;
    s32 replaced = 0;
    f32 z;
    if (sPortZoomOn[k]) {
        if (Mtx_PortSame(&view->view.view2screen, &sPortZoomed[k][0]) && Mtx_PortSame(&view->view.view2clip, &sPortZoomed[k][1])) {
            view->view.view2screen = sPortBase[k][0]; /* (back to the game's, to zoom it afresh) */
            view->view.view2clip = sPortBase[k][1];
        } else {
            replaced = 1; /* (made anew by the game: that is the base now) */
        }
    }
    sPortZoomOn[k] = 0;
    if (zoom == 1000 || zoom <= 0) {
        return replaced;
    }
    z = (f32)zoom / 1000.0f;
    sPortBase[k][0] = view->view.view2screen;
    sPortBase[k][1] = view->view.view2clip;
    view->view.view2screen.m[0][0] *= z;
    view->view.view2screen.m[1][1] *= z;
    view->view.view2clip.m[0][0] *= z;
    view->view.view2clip.m[1][1] *= z;
    sPortZoomed[k][0] = view->view.view2screen;
    sPortZoomed[k][1] = view->view.view2clip;
    sPortZoomOn[k] = 1;
    return replaced;
}
#endif

#ifdef PORT
/* The zoom the script mods asked for each view (thousandths, 1000 the game's: port/src/mods_game.c), set by
   BtlCam_UpdateView. How much wider than the game's the selected view is drawn: the culling of what is drawn (the
   stage's frustum, src/battle/stg_a.c; the effects' clip planes, eft_a.c) widens by as much, or what the wider view
   shows at its edges is left out. 1 without (never narrower than the game's). */
static s32 sPortZoomView[2] = {1000, 1000};
/* Host presentation state. A priority/chase override can read the same view
 * twice during one blank, and rollback can rebuild old blanks. Neither should
 * advance Lua inertia or stage-collision hysteresis a second time. */
PORT_HOST static struct {
    s32 valid, side, obj, zoom;
    u32 blank;
    Vec4 pos, rot;
} sPortCameraPose[2];
PORT_HOST static f32 sPortCollisionHeld[2] = {1.0f, 1.0f};
PORT_HOST static s32 sPortCollisionClear[2], sPortCollisionObj[2];
f32 Port_CamCull(void) {
    s32 zoom;
    if (gBtlCam == NULL || gBtlCam->cur == NULL) {
        return 1.0f;
    }
    zoom = sPortZoomView[gBtlCam->cur->index & 1];
    return zoom > 0 && zoom < 1000 ? 1000.0f / (f32)zoom : 1.0f;
}

/* A pose the script mods gave (view->pos) kept out of the stage: traced from what the fighter's camera looks at
   (the fighter) to it, as the game traces its own camera (btl_char_cam_modes.c), and a hit stops it a clearance
   (4: a fifth of a fighter) before the stage: on the surface its face filled the picture. Held (hysteresis): a
   nearer limit is taken at once (eased), a farther one only after the way has been clear for 15 frames, then eased
   out; the game's camera sways, and a limit that came and went with it made the view wobble. The trace only reads
   the stage; the collision's query mode it sets (which the game's effect queries also use) is put back. */
static f32 SpecCamera_StageRay(const f32 *from,const f32 *to);
static void BtlCam_PortCollide(BtlCamView *view, s32 objId) {
    extern void *BtlChar_FindByObjId(s32 objId);
    u8 *chr = BtlChar_FindByObjId(objId);
    s32 v = view->index & 1, i;
    Vec4 target;
    f32 f = 1.0f, len = 0.0f, got = 0.0f;

    if (chr == NULL) {
        return;
    }
    if (sPortCollisionObj[v] != objId) {
        sPortCollisionHeld[v] = 1.0f;
        sPortCollisionClear[v] = 0;
        sPortCollisionObj[v] = objId;
    }
    target = *(Vec4 *)(chr + 0x450); /* (ChrCam.target, include/battle/btl_char_cam.h) */
    /* Presentation collision reads the stage BVH without changing simulation scratch globals. */
    f = SpecCamera_StageRay((const f32 *)&target,(const f32 *)&view->pos);
    if (f < 1.0f) {
        for (i = 0; i < 3; i++) {
            f32 a = ((f32 *)&view->pos)[i] - ((f32 *)&target)[i];
            len += a * a;
        }
        f = len > 0.0001f ? f - 4.0f / __builtin_sqrtf(len) : 0.0f;
        f = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
    }
    if (f < sPortCollisionHeld[v]) {
        sPortCollisionHeld[v] += (f - sPortCollisionHeld[v]) * 0.5f;
        sPortCollisionClear[v] = 0;
    } else if (++sPortCollisionClear[v] >= 15) {
        sPortCollisionHeld[v] += (f - sPortCollisionHeld[v]) * 0.1f;
    }
    f = sPortCollisionHeld[v];
    for (i = 0; i < 3; i++) {
        ((f32 *)&view->pos)[i] = ((f32 *)&target)[i] + f * (((f32 *)&view->pos)[i] - ((f32 *)&target)[i]);
    }
}
#endif

/* Turns a battle view's pose into its matrices and optionally loads them for drawing. */
void BtlCam_BuildView(BtlCamView *view, s32 apply) {
    View_SetTransform(&view->view.world2view, &view->view.world2view2, &view->pos, &view->rot);
#ifdef PORT
    {
        s32 zoom = sPortZoomView[view->index & 1];
        BtlCam_PortZoom(view, zoom); /* (the zoom kept in the view: every pass that builds from its parts agrees) */
        View_UpdateMatrices(&view->view, &view->pos);
        if (BtlCam_PortZoom(view, zoom)) { /* (the projection was built again inside: zoomed again) */
            View_UpdateMatrices(&view->view, &view->pos);
        }
    }
#else
    View_UpdateMatrices(&view->view, &view->pos);
#endif
    if (apply != 0) {
        View_Apply(&view->view, 1);
    }
}

/* Clears the camera block, rebuilds the views, selects view 0 and resets the demo camera. */
void BtlCam_Reset(void) {
#ifdef PORT
    sPortFreeCamLive = 0;
    memset(sPortCameraPose, 0, sizeof(sPortCameraPose));
    sPortCollisionHeld[0] = sPortCollisionHeld[1] = 1.0f;
    memset(sPortCollisionClear, 0, sizeof(sPortCollisionClear));
    sPortCollisionObj[0] = sPortCollisionObj[1] = -1;
    PortSpectator_EndBattle();
#endif
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
#ifdef PORT
    sPortFreeCamLive = 0;
    PortSpectator_EndBattle();
#endif
    Heap_Free(gBtlCam);
    gBtlCam = NULL;
    DemoCam_Term();
}

#ifdef PORT
static void BtlCam_PortMods(BtlCamView *view,s32 side,s32 objId) {
        extern s32 gPortResim;
        extern u32 gPortLiveBlanks;
        extern void Port_NaturalFightCamera(s32,s32,s32);
        extern void Port_ModsCamera(s32 side, s32 objId, s32 pos, s32 rot, s32 paused); /* (addresses in game memory) */
        extern s32 Port_ModsCamZoom(s32 side);
        extern s32 Port_ModsCamCollide(s32 side);
        s32 k = view->index & 1;
        if (gPortResim) return;
        if (sPortCameraPose[k].valid && sPortCameraPose[k].blank == gPortLiveBlanks &&
            sPortCameraPose[k].side == side && sPortCameraPose[k].obj == objId) {
            view->pos = sPortCameraPose[k].pos;
            view->rot = sPortCameraPose[k].rot;
            sPortZoomView[k] = sPortCameraPose[k].zoom;
            return;
        }
        Port_NaturalFightCamera(objId,(s32)(u32)&view->pos,(s32)(u32)&view->rot);
        Port_ModsCamera(side, objId, (s32)(u32)&view->pos, (s32)(u32)&view->rot, (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)!=0);
        sPortZoomView[view->index & 1] = Port_ModsCamZoom(side); /* (the zoom, the stage: what they also asked) */
        if (Port_ModsCamCollide(side)) {
            BtlCam_PortCollide(view, objId);
        }
        sPortCameraPose[k].valid = 1;
        sPortCameraPose[k].blank = gPortLiveBlanks;
        sPortCameraPose[k].side = side;
        sPortCameraPose[k].obj = objId;
        sPortCameraPose[k].pos = view->pos;
        sPortCameraPose[k].rot = view->rot;
        sPortCameraPose[k].zoom = sPortZoomView[k];
}
#endif

/* Per frame: copies one side's fighter camera pose and priority into the selected view and builds it. */
void BtlCam_UpdateView(s32 side) {
    BtlCamView *view = gBtlCam->cur;
    s32 objId = BattleSide_GetObjId(side);

    if (Battle_IsSplitScreen()) {
        BtlCam_SetLayout(1, 0);
    }
    BtlCharApi_GetCamPose(objId, &view->pos, &view->rot);
#ifdef PORT
    /* Tag selects its actual viewer in UpdateOverride. Applying the Lua
       camera here as well advances side 0's inertia for two different actors
       in one tick, before the local actor's final camera is known. */
    if(!TagBattle_Active())BtlCam_PortMods(view,side,objId);
#endif
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

#include "spectator_collision.inc"
/* A separate local view, with camera settings and raw input in host memory. */
static s32 BtlCam_PortFreeCamera(s32 selected) {
    extern int gPortResim,Port_Setting(const char *,int);
    extern void BtlCharApi_GetBasePos(s32,Vec4 *);
    extern f32 BtlCharApi_GetHeight(s32),BtlCharApi_GetRadius(s32);
    BtlCamView *native;
    Vec4 targets[10],eye,rotation,previous,nativeEye,nativeRotation;
    f32 zoom,tanY;
    s32 count,i,idx,mode=PortSpectator_Mode();
    if(gPortResim)return 0;
    PortSpectator_SetInputActive(gBtlCam!=NULL && !(Battle_GetWork()->flags&BATTLE_FLAG_PAUSE_MENU));
    if(!mode){sPortFreeCamLive=0;return 0;}
    if(!gBtlCam || !gBtlCam->cur)return 0;
    native=gBtlCam->cur;
    if(!selected) {
        idx=BtlCam_GetPriorityView();if(idx<0)idx=BtlCam_GetDefaultView();
        if(idx<0 || idx>1)idx=0;native=&gBtlCam->views[idx];
    }
    nativeEye=native->pos;nativeRotation=native->rot;
    count=TagBattle_Active()?TagBattle_Count():2;
    if(count<1 || count>10)return 0;
    for(i=0;i<count;i++) {
        s32 object=BattleSide_GetObjId(i);
        f32 height=BtlCharApi_GetHeight(object),radius=BtlCharApi_GetRadius(object);
        BtlCharApi_GetBasePos(object,&targets[i]);
        targets[i].y-=height*.5f;targets[i].w=fmaxf(fabsf(height)*.65f,fabsf(radius))+2;
    }
    previous=sPortFreeCamLive?sPortFreeCamPos:native->pos;
    tanY=224.f/gBtlCam->layouts[0].screenDist;
    if(!PortSpectator_Camera((f32 *)&native->pos,(f32 *)&native->rot,(f32 *)targets,count,
        tanY,Port_Setting("aspect_milli",1333)/1000.f,(f32 *)&eye,(f32 *)&rotation,&zoom))return 0;
    if(sPortFreeCamLive)SpecCamera_AvoidStage(&eye,&previous);
    PortSpectator_ResolveEye((f32 *)&eye);
    sPortFreeCam.view=gBtlCam->layouts[0];sPortFreeCamPos=eye;sPortFreeCamRot=rotation;
    sPortFreeCam.view.screenDist*=zoom;View_BuildProjection(&sPortFreeCam.view);
    View_SetTransform(&sPortFreeCam.view.world2view,&sPortFreeCam.view.world2view2,&sPortFreeCamPos,&sPortFreeCamRot);
    View_UpdateMatrices(&sPortFreeCam.view,&sPortFreeCamPos);View_Apply(&sPortFreeCam.view,1);
    sPortFreeCamLive=1;
    if(getenv("BT4_RENDER_TEST") && getenv("BT4_FREE_CAMERA_AUDIT")) {
        PORT_HOST static s32 ticks;
        for(i=0;i<4;i++)if(((f32 *)&nativeEye)[i]!=((f32 *)&native->pos)[i] ||
                          ((f32 *)&nativeRotation)[i]!=((f32 *)&native->rot)[i]) {
            fprintf(stderr,"free camera audit: FAIL native-pose changed\\n");exit(6);
        }
        if(!__builtin_isfinite(eye.x) || !__builtin_isfinite(eye.y) || !__builtin_isfinite(eye.z)) {
            fprintf(stderr,"free camera audit: FAIL non-finite eye\\n");exit(6);
        }
        if(++ticks==30)fprintf(stderr,"free camera audit: PASS mode=%d tag=%d moved=1 native-pose-unchanged=1 camera-mode=%d\n",Battle_GetMode(),TagBattle_Active(),mode);
    }
    return 1;
}
#endif
#ifdef PORT
static void BtlCam_PortChaseView(s32 side) {
    BtlCam_SelectView(side);
    BtlCam_UpdateView(side);
    /* UpdateView restores the native split-screen projection. Apply the
     * local full-screen projection only after copying that pose. */
    BtlCam_SetLayout(0,1);
    BtlCam_ApplyView(1);
}
#endif
/* Decides whether one full-screen view replaces the per-player views this frame; returns 1 if so. */
s32 BtlCam_UpdateOverride(void) {
    s32 ret = 0;
    s32 idx;

    if (TagBattle_Active()) {
        /* Each TAG client keeps its own fighter's camera. Another fighter's
         * cut must not replace the local view. Advance shared demo animation
         * time normally, then restore the local view when it has another owner. */
        void *own;
        s32 localActor=TagBattle_ViewActor();
        s32 actor,focus=localActor,priority=-1,demoOwner=-1;
        s32 defaultOwner=TagBattle_DefaultCutOwner();
        extern void *BtlChar_Get(s32 actor);
        extern s32 BtlAct_GetCurrent(void *chr),ChrCam_IsCutActive(void *chr);
        extern void *BtlObj_Get(s32 objId);
        focus=TagBattle_CameraActor(localActor);
        for(actor=0;actor<TagBattle_Count();++actor){
            void *obj=BtlObj_Get(BattleSide_GetObjId(actor));
            if(gDemoCam->obj==obj || gDemoCam->chr==obj)demoOwner=actor;
        }
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_RUSH_SCENE")){
            static s32 last=-1;int action=BtlAct_GetCurrent(BtlChar_Get(0));
            if(action!=last){printf("tag rush camera: viewer=%d action=%x focus=%d pair=%d global=%d demo=%d active=%d visible=%d\n",localActor,action,focus,TagBattle_Pair(0),defaultOwner,demoOwner,DemoCam_IsActive(),demoOwner>=0?TagBattle_CutVisible(demoOwner,localActor):0);last=action;}
        }
        BtlCam_SelectView(0);
        BtlCam_SetLayout(0, 1);
        BtlCharApi_GetCamPose(BattleSide_GetObjId(focus),&gBtlCam->cur->pos,&gBtlCam->cur->rot);
#ifdef PORT
        BtlCam_PortMods(gBtlCam->cur,0,BattleSide_GetObjId(focus));
#endif
        BtlCam_BuildView(gBtlCam->cur,1);
        own=BtlObj_Get(BattleSide_GetObjId(0));
        if(DemoCam_IsActive()) {
            extern s32 Port_SpecialMoveCameras(s32 change);
            s32 local=demoOwner>=0 && demoOwner==focus && Port_SpecialMoveCameras(-1);
            DemoCam_Update();
            if(!local) { BtlCam_SelectView(0); BtlCam_ApplyView(1); }
        }
        if(getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_HUD_VIEW_TEST")){
            extern s32 BtlChar_FrameMod(s32 n);
            s32 phase=BtlChar_FrameMod(240)/60;
            /* A hidden renderer fixture, using the real scene and projection.
             * Holds front/right/rear/left views long enough to inspect edge
             * indicators. It never changes the character's pose or inputs. */
            BtlCam_SelectView(0);
            gBtlCam->cur->rot.y+=phase*1.57079632679f;
            BtlCam_BuildView(gBtlCam->cur,1);
        }
#ifdef PORT
        (void)BtlCam_PortFreeCamera(1);
#endif
        return 1;
    }

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
#ifdef PORT
    {
        extern s32 Port_SpecialMoveCameras(s32 change);
        extern s32 Battle_GetHumanSide(void);
        extern void *BtlChar_Get(s32 actor);
        extern s32 BtlAct_GetCurrent(void *chr),ChrCam_IsCutActive(void *chr);
        s32 side=Port_NetView(),i,special=0;
        if(side<0)side=Battle_GetHumanSide();if(side<0||side>1)side=0;
        for(i=0;i<2;++i){s32 a=BtlAct_GetCurrent(BtlChar_Get(i));if(a>=0xFD&&a<=0x13B)special=1;}
        if(special&&!Port_SpecialMoveCameras(-1)){
            BtlCam_PortChaseView(side);
            ret=1;
        }
    }
    if (BtlCam_PortFreeCamera(ret)) {
        ret = 1;
    }
#endif
    return ret;
}

#include "battle/tag_team_hud_math.h"
static f32 sTagHudView[16],sTagHudProjection[16],sTagHudNear;
static s32 sTagHudViewValid;
/* Snapshot the view used to draw the scene, before any effect/HUD pass can
 * replace the global current view. UI queries do not change render state. */
void TagBattle_HudCaptureView(void *ptr) {
    View *view=ptr;s32 i;
    sTagHudViewValid=TagBattle_Active() && view!=NULL;
    if(!sTagHudViewValid)return;
    for(i=0;i<16;++i){sTagHudView[i]=((f32 *)&view->world2view2)[i];sTagHudProjection[i]=((f32 *)&view->world2screen)[i];}
    sTagHudNear=view->nearZ;
    if(getenv("BT4_TAG_HUD_LOG")){
        extern void Port_TagHudView(f32 *v,f32 *p,f32 *nearZ);
        Port_TagHudView(sTagHudView,sTagHudProjection,&sTagHudNear);
    }
}
s32 TagBattle_HudCamera(f32 *basis) { return sTagHudViewValid && ThBasis(sTagHudView,basis); }
s32 TagBattle_HudHead(s32 actor,f32 *out) {
    Vec4 p,base;
    s32 j;
    extern void BtlCharApi_GetPos(s32 actor,Vec4 *out);
    extern void BtlCharApi_GetNodePos(s32 actor,s32 node,Vec4 *out);
    extern f32 BtlCharApi_GetHeight(s32 actor);
    if(!sTagHudViewValid || actor<0 || actor>=TagBattle_Count())return 0;
    BtlCharApi_GetPos(BattleSide_GetObjId(actor),&base);
    /* Node 0x2F is also used by the engine's head-tracking code. Following
     * its animated pose keeps the marker attached during jumps and attacks. */
    BtlCharApi_GetNodePos(BattleSide_GetObjId(actor),0x2F,&p);
    if(p.x==base.x && p.y==base.y && p.z==base.z)
        p.y-=BtlCharApi_GetHeight(BattleSide_GetObjId(actor))*0.7f;
    p.y-=3.0f;
    for(j=0;j<4;++j)out[j]=((f32 *)&p)[j];
    return 1;
}
s32 TagBattle_HudProject(f32 *head,f32 *out) { return sTagHudViewValid && ThProject(sTagHudProjection,head,sTagHudNear,out); }
void TagBattle_HudRear(f32 *head,f32 *out) { ThRear(sTagHudView,head,out); }

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
