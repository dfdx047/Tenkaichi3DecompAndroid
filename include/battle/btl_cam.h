#ifndef BATTLE_BTL_CAM_H
#define BATTLE_BTL_CAM_H

#include "types.h"
#include "sys/math3d.h"

/* Battle camera, src/battle/btl_cam.c = 0x23E040..0x23F620.
 *
 * Neighbours, each its own C file: battle/btl_demo_cam.h (0x23D1E8..0x23E040, the scripted camera, before)
 * and battle/orbit_cam.h (0x23F620..0x23FB20, a viewer's orbit camera, after).
 * Four pieces sit back to back in this range (possibly separate source files originally; nothing but the
 * function boundaries separates them):
 *
 *   0x23E040..0x23E950  View_*     the generic view object: view matrix, projection, scissor, screen layout
 *   0x23E950..0x23F0B0  BtlCam_*   the two per-side battle views and the choice between them
 *   0x23F0B0..0x23F3C8  DbgCam_*   a free-fly camera driven straight from a pad; nothing calls it
 *   0x23F3C8..0x23F620  CamShake_* four {time, strength} slots turned into a random jitter
 *
 * == How a battle frame gets its cameras ==
 * The camera POSE (position + Euler rotation) is not computed here. Each fighter carries its own camera
 * (fighter + 0x430 position, + 0x440 rotation, maintained by the fighter code at 0x1C69C8); this file only
 * copies it. Battle_Update does, every frame, for i = 0, 1:
 *     BtlCam_SelectView(i);            gBtlCam->cur = &views[i]
 *     BtlCam_UpdateView(i);
 *         id = BattleSide_GetObjId(i)                     the side's active fighter
 *         split-screen: BtlCam_SetLayout(1, 0)            back to its half if it was full screen
 *         BtlCharApi_GetCamPose(id, &view->pos, &view->rot)       copy the fighter camera's pose
 *         view->priority = BtlCharApi_HasCamPriority(id)              fighter flag 0xD3: 0 or 1
 *         BtlCam_BuildView(view, 1)                       pose -> matrices, loaded into VU0, scissor set
 * then BtlCam_UpdateOverride(). So both views always hold the pose of their own side's fighter camera,
 * in split-screen or not.
 *
 * == Override (one full-screen view) ==
 * BtlCam_UpdateOverride() decides whether this frame is drawn once, full screen, and from which view. It
 * returns 1 if so; Battle_Update passes that to BtlScene_SetSingleView and returns the inverse.
 *   - Not split-screen, or BattleReplay_IsActive(): always 1. The view is BtlCam_GetPriorityView(), or
 *     BtlCam_GetDefaultView() when that gives -1.
 *   - Split-screen: 1 only when BtlCam_GetPriorityView() >= 0; the chosen view is switched to the
 *     full-screen layout (BtlCam_SetLayout(0, 1)) and BtlCam_UpdateView puts it back the next frame.
 *   - BtlCam_GetPriorityView(): the view whose fighter has flag 0xD3 when only one has it; when both
 *     have it, BtlCam_GetDefaultView(); when neither has it, -1.
 *   - BtlCam_GetDefaultView(): mode 8: 0 if BattleSide_GetControl(0) == 0, else 1 if
 *     BattleSide_GetControl(1) == 0, else the word at 0x2FF280 (no writer found: 0); other modes:
 *     BtlCharApi_GetMgrUnk134() during a replay, else 0.
 *   - Last, if DemoCam_IsActive() (scripted camera: intro cuts, fixed pose) DemoCam_Update() builds and
 *     loads its view instead and the result is 1.
 *
 * == Pad input ==
 * Nothing in View_*, BtlCam_* or CamShake_* reads a pad. DbgCam_Update(view, pos, rot, pad, speed) reads
 * gPad[pad].gameHeld & PADG_L1, gameLeft[0], gameLeft[1] and gameRight[1], and changes only the pos / rot
 * it is given and the view it builds; it has no caller and no table entry in the executable or the menu
 * overlay (and DbgCam_Init / DbgCam_SetLocked have none either), so it is dead debug code.
 *
 * == What leaves this file ==
 * gBtlCamView (the view being built / drawn) and the matrices loaded into VU0. gBtlCamView is read by about
 * 120 functions, all drawing, effects and sound code as far as checked (View_GetDistXZ here; 0x1D9DB0 turns
 * the view matrix into 3D sound volume / pan). CamShake_Calc calls the C library rand() five times per call
 * while a shake is active: that advances the same generator the rest of the game uses.
 */

/* Vec4 and Mtx44 come from sys/math3d.h. Matrices here are m[row][col] with the translation in row 3, as there. */

/* A view: matrices, scissor and projection parameters. 0x260 bytes.
   (main.h's ViewScissor is the part of this that View_ApplyScissor reads.) */
typedef struct View {
    /* 0x000 */ Mtx44 world2view;   /* View_SetTransform writes the same matrix to both of these; */
    /* 0x040 */ Mtx44 world2view2;  /*   only the second is read again */
    /* 0x080 */ Mtx44 view2screen;  /* projection to GS screen coordinates (View_BuildProjection) */
    /* 0x0C0 */ Mtx44 view2clip;    /* projection to clip space */
    /* 0x100 */ Mtx44 unk100;       /* diagonal + translation built by View_BuildProjection, not read here */
    /* 0x140 */ Mtx44 world2screen; /* view2screen * world2view2 (View_UpdateMatrices) */
    /* 0x180 */ Mtx44 world2clip;   /* view2clip * world2view2 */
    /* 0x1C0 */ Mtx44 unk1C0;       /* built by View_BuildProjection, not read here */
    /* 0x200 */ s32 scissorX0;
    /* 0x204 */ s32 scissorX1;
    /* 0x208 */ s32 scissorY0;
    /* 0x20C */ s32 scissorY1;
    /* 0x210 */ Vec4 screenSize;  /* (2047 or 1919, 2047, 0.1, 16777215): x, y, ?, z range */
    /* 0x220 */ Vec4 pos;         /* camera position given to View_UpdateMatrices */
    /* 0x230 */ f32 aspect;       /* 1.1666666 (= 448 * 4 / 512 / 3), passed as aspectY */
    /* 0x234 */ f32 screenDist;   /* 433 */
    /* 0x238 */ f32 aspectX;      /* the larger of the two is normalised to 1 */
    /* 0x23C */ f32 aspectY;
    /* 0x240 */ f32 centerX;      /* GS screen centre: 2048 full screen, 1920 / 2176 for the halves */
    /* 0x244 */ f32 centerY;      /* 2048 */
    /* 0x248 */ f32 zMin;         /* 1 */
    /* 0x24C */ f32 zMax;         /* 10877000 */
    /* 0x250 */ f32 nearZ;        /* 0.3 */
    /* 0x254 */ f32 farZ;         /* 65536 */
    /* 0x258 */ f32 unk258;       /* 1, scales [0][0] and [1][1] of the projections */
    /* 0x25C */ f32 unk25C;
} __attribute__((aligned(16))) View; /* size 0x260; the alignment is needed for the struct copy in BtlCam_SetViewLayout to match */

/* View layouts (View_InitLayout / BtlCam_SetViewLayout / BtlCam_SetLayout). */
#define VIEW_LAYOUT_FULL  0 /* scissor x 0..511, centre 2048 */
#define VIEW_LAYOUT_LEFT  1 /* scissor x 0..254, centre 1920 */
#define VIEW_LAYOUT_RIGHT 2 /* scissor x 257..511, centre 2176 */

/* One player's battle view. */
typedef struct BtlCamView {
    /* 0x000 */ View view;
    /* 0x260 */ Vec4 pos;      /* camera position, copied from fighter + 0x430 (BtlCharApi_GetCamPose) */
    /* 0x270 */ Vec4 rot;      /* camera rotation, copied from fighter + 0x440; w is forced to 1 by View_SetTransform */
    /* 0x280 */ s32 split;     /* 0 = full-screen layout, 1 = half-screen layout */
    /* 0x284 */ s32 index;     /* 0 / 1 */
    /* 0x288 */ s32 priority;  /* 1 when the side's fighter has flag 0xD3 (BtlCharApi_HasCamPriority), else 0 */
    /* 0x28C */ s32 unk28C;
} BtlCamView; /* size 0x290 */

/* gBtlCam: the block allocated by BtlCam_Init. */
typedef struct BtlCam {
    /* 0x000 */ View layouts[3];     /* VIEW_LAYOUT_* templates, copied over a view to change its layout */
    /* 0x720 */ BtlCamView views[2]; /* one per side */
    /* 0xC40 */ BtlCamView *cur;     /* the view selected by BtlCam_SelectView */
    /* 0xC44 */ u8 unkC44[0xC];
} BtlCam; /* size 0xC50 */

/* Camera shake: up to four requests, the strongest one drives the jitter. */
typedef struct CamShake {
    /* 0x00 */ f32 time[4];     /* seconds left; CamShake_Tick takes 1/30 off per call */
    /* 0x10 */ f32 strength[4];
} CamShake; /* size 0x20 */

extern BtlCam *gBtlCam;
extern View *gBtlCamView;

void View_BuildProjection(View *view);
void View_SetProjection(View *view, Vec4 *screenSize, f32 screenDist, f32 aspectX, f32 aspectY, f32 centerX,
                        f32 centerY, f32 zMin, f32 zMax, f32 nearZ, f32 farZ, f32 unk258);
void View_UpdateMatrices(View *view, Vec4 *pos);
void View_SetTransform(Mtx44 *dst0, Mtx44 *dst1, Vec4 *pos, Vec4 *rot);
void View_Apply(View *view, s32 scissor);
f32 View_GetDistXZ(Vec4 *pos);
void View_InitLayout(View *view, s32 layout);

void BtlCam_SetViewLayout(BtlCamView *view, s32 layout);
void BtlCam_InitViews(void);
BtlCamView *BtlCam_GetNativeView(s32 idx);
void BtlCam_BuildView(BtlCamView *view, s32 apply);
void BtlCam_Reset(void);
void BtlCam_Init(void);
void BtlCam_Term(void);
void BtlCam_UpdateView(s32 side);
s32 BtlCam_TraceStage(Vec4 *out, Vec4 *from, Vec4 *to, f32 *frac, s32 *hitObj);
s32 BtlCam_GetDefaultView(void);
void BtlCam_SetLayout(s32 split, s32 apply);
s32 BtlCam_GetPriorityView(void);
void BtlCam_SelectView(s32 idx);
void BtlCam_ApplyView(s32 scissor);
s32 BtlCam_UpdateOverride(void);

void DbgCam_Init(void);
void DbgCam_Update(View *view, Vec4 *pos, Vec4 *rot, s32 pad, f32 speed);
void DbgCam_SetLocked(s32 locked);

void CamShake_Add(CamShake *shake, f32 strength, f32 time);
void CamShake_Tick(CamShake *shake);
void CamShake_Calc(CamShake *shake, Vec4 *posOfs, Vec4 *rotOfs);
f32 CamShake_GetTime(CamShake *shake);

#endif
