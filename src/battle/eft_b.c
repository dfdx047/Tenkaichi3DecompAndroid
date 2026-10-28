#include "common.h"
#include "battle/eft_fighter_capacity.h"
#include "battle/eft_b.h"
#include "sys/gfx_ot.h"

#ifdef PORT
extern f32 Port_WideFactor(void); /* widescreen on PC: 1, or 4/3 for a 16:9 picture (port/src/plat_stub.c) */
#endif

/*
 * Effect code 0x132290..0x136760: see include/battle/eft_b.h for the four pieces and their layouts.
 *
 * Nothing here touches a fighter, a hit record or a battle flag: it is all drawing. What it reads from the
 * simulation: fighter speed, height, position, direction, node positions and "in water" through BtlCharApi_*,
 * the battle pause flag, BtlScene_IsTimeStopped and the stage's water height (0x140338).
 * The random numbers are all the C library rand(). Their NUMBER per frame depends on the camera: a view whose
 * camera is under water draws one rand() per frame plus five per bubble it spawns, and nothing in split-screen
 * (EftBubble_UpdateAmbient). rand() is shared with the rest of the game.
 *
 * Three functions are INCLUDE_ASM with the attempt in `#if 0` above them: EftPrim_DrawBillboard,
 * EftPrim_DrawTriangle, EftGeyser_DrawColumn.
 * With every attempt enabled the file's .lit4 (0x2FC368..0x2FC400) and .rodata (0x2EC620..0x2EC720) come out
 * identical to the original; as shipped, the C emits .lit4 0x2FC370..0x2FC3FC (EftBubble_EmitBody's ten constants
 * come first), the bone table at 0x2EC620 and the jump table at 0x2EC6C0.
 */

/* ---- local views of other modules ---- */

/* Battle block: only the flags (battle/battle.h). */
typedef struct EftBBattle {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags;
} EftBBattle;
#define EFTB_BATTLE_PAUSE 0x100

/* A camera view (battle/btl_cam.h: View / BtlCamView / BtlCam). */
typedef struct EftBView {
    /* 0x000 */ Mtx44 world2view;
    /* 0x040 */ Mtx44 world2view2;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 world2screen;
    /* 0x180 */ u8 unk180[0xA0];
    /* 0x220 */ Vec4 pos;
    /* 0x230 */ f32 aspect;
    /* 0x234 */ f32 screenDist;
    /* 0x238 */ u8 unk238[0x28];
    /* 0x260 */ Vec4 camPos;
    /* 0x270 */ u8 unk270[0x20];
} EftBView; /* size 0x290 */

typedef struct EftBCam {
    /* 0x000 */ u8 layouts[0x720];
    /* 0x720 */ EftBView views[2];
} EftBCam;

/* gCommonRes: only the stage model file (battle/battle_work.h BattleRes.stage). */
typedef struct EftBCommonRes {
    /* 0x00 */ u8 unk0[0x24];
    /* 0x24 */ u8 *stage;
} EftBCommonRes;

/* The stage manager: only its flag word. */
typedef struct EftBStage {
    /* 0x00 */ u8 unk0[8];
    /* 0x08 */ s32 flags;
} EftBStage;

/* gBtlCam seen from view i, for the one loop whose induction variable is i * 0x290 with the field offset kept
   in the load (EftBubble_UpdateAmbient). */
typedef struct EftBCamAt {
    /* 0x000 */ u8 layouts[0x720];
    /* 0x720 */ EftBView view;
} EftBCamAt;
#define EFTB_CAM_AT(i) ((EftBCamAt *)((u8 *)gBtlCam + (i) * sizeof(EftBView)))

extern EftBView *gBtlCamView;
extern EftBCam *gBtlCam;
extern EftBCommonRes *gCommonRes;
extern EftBStage *gBtlStage;

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 Mathf_SinFast(f32 angle);
extern f32 Mathf_CosFast(f32 angle);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern f32 Vec3_Length(Vec4 *v);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b); /* matrix product */
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);          /* matrix copy */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);          /* inverse of a rotation + translation matrix */
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Z */
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Y */
extern void Vu0Cur_Push(void);                            /* VU0 matrix stack: push */
extern void Vu0Cur_Pop(void);                            /* VU0 matrix stack: pop */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                        /* VU0 current matrix = m */
extern s32 Vu0Cur_ProjectPoint(EftBIVec *out, Vec4 *pos);         /* project one point with the current view; returns a value */
extern s32 Vu0Cur_ProjectPoints(EftBIVec *out, Vec4 *pos, s32 count); /* project count points; 0 when clipped */
extern void ClipVtx_Set(void *out, Vec4 *pos, Vec4 *uv, Vec4 *color); /* fills one EftBVert */
extern void Vec4_Mul(Vec4 *dst, Vec4 *a, Vec4 *b);     /* per-component product */
extern void Vec4_ToInt(EftBIVec *dst, Vec4 *src);        /* float to fixed vector */
extern void Mtx_ProjectPoint(EftBIVec *out, Mtx44 *m, Vec4 *pos); /* project with a matrix */
extern void Vec3_ScaleAdd(Vec4 *dst, Vec4 *dir, Vec4 *base, f32 s); /* dst = base + dir * s */
extern f32 EftMath_WrapAngle(f32 angle);                        /* wraps an angle into -pi..pi */
extern void EftGfx_UpdateClipPlanes(void);
extern void EftGfx_DrawPolyFixedZ(void *verts, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, u64 tex0, s32 z);
extern s32 EftStage_IsDrawOn(void);
extern f32 EftStage_GetTintScale(void);
extern void EftSteam_Stop(void *emitter);
extern void EftSteam_SetPaused(void *emitter, s32 on);
extern void EftSteam_SetPos(void *emitter, EftGeyser *work);
extern s32 EftWater_GetSurfaceY(f32 *outY);                        /* water surface height; 0 when the stage has none */
extern void EftSmoke_Stop(void *emitter);
extern void EftSmoke_SetPos(void *emitter, EftGeyser *work);
extern void EftGeyser_StartSmoke(EftBTask *task);
extern void EftGeyser_StartSteam(EftBTask *task);
extern void Tex_Upload(EftBTexImg *img, s32 tbp, s32 cbp);
extern void EftMesh_Init(void *model, void *data);
extern void EftMesh_SetTex(void *model, EftBTexSet *tex);
extern void EftMesh_SetMtx(void *model, Mtx44 *m);
extern void EftMesh_SetClip(void *model, s32 on);
extern void EftMesh_SetRepeat(void *model, s32 on);
extern void EftMesh_SetUvOfs(void *model, f32 x, f32 y);
extern void EftMesh_SetUvScale(void *model, f32 size);
extern void EftMesh_DrawNow(void *model, s32 on);
extern void BtlTask_SetDead(void *task);                      /* marks a task as dying */
extern u64 EftVram_AddTex(EftBTex *tex, s32 a, s32 b);       /* TEX0 of a texture with two mode bits */
extern void EftTexSet_Load32(EftBTexSet *set, void *pack);     /* builds a texture set from a pack */

extern EftBBattle *Battle_GetWork(void);
extern s32 Battle_IsSplitScreen(void);
extern s32 BtlStage_IsReady(void);
extern s32 BtlStage_GetFxResA(void);
extern EftGeyserRec *BtlStage_GetFxResA2(void);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 *BtlScene_GetStageData(void);
extern s32 *BtlScene_GetPackEntry(s32 *base, s32 idx);
extern s32 BtlScene_GetPackEntrySize(s32 *base, s32 idx);
extern s32 BtlScene_IsSecondView(void);
extern s32 BtlScene_IsTimeStopped(void);
extern void *BtlTask_CreateChildList(void *task, s32 count, s32 workSize);
extern void *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern u64 *Dma_BeginDirect(void);
extern void Dma_EndDirect(u64 *end);

extern f32 BtlCharApi_GetHeight(s32 objId);
extern void BtlCharApi_GetPos(s32 objId, Vec4 *out);
extern void BtlCharApi_GetDir(s32 objId, Vec4 *out);
extern f32 BtlCharApi_GetSpeed(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern s32 BtlCharApi_IsInWater(s32 objId);

extern Vec4 gEftBubbleZeroVel;
extern void *gEftGeyserClass[6];

#define EFTB_RAND01() ((f32)rand() / 2147483647.0f)

/* Reads an int from a byte pointer that need not be aligned, and steps over it. */
#define EFTB_READ_INT(v, p) (__builtin_memcpy((u8 *)&(v), (p), 4), (p) += 4)

void EftBubble_EmitBody(s32 objId, s32 chance, s32 view);
void EftBubble_EmitBurst(s32 objId, s32 count);
void EftBubble_EmitCamera(s32 view);
void EftBubble_UpdateChrs(void);
void EftBubble_UpdateTrail(EftBubbleChr *chr);
void EftBubble_UpdateBurst(EftBubbleChr *chr);
void EftBubble_UpdateAmbient(void);
EftBubble *EftBubble_AllocSlot(void);
void EftBubble_UpdateList(EftBubble **head, EftBubble **tail);
s32 EftBubble_Step(EftBubble *b);
void EftBubble_DrawList(EftBubble *b);
s32 EftBubble_IsOnScreen(EftBIVec v);
void EftBubble_RandDir(EftBVec v);
void EftBubble_DrawOne(EftBVec pos, EftBMtx m, f32 size, f32 rot, u8 r, u8 g, u8 b, u8 a, EftBTex *tex, u8 layer);
void EftBubble_SetTexMode(s32 a, s32 b);
void EftStageScroll_PlaceTextures(s32 tcc, s32 tfx);
void EftStageScroll_Load(void);
void EftGeyser_Update(EftBTask *task);
void EftGeyser_Restart(EftBTask *task, EftGeyser *w);
void EftGeyser_DrawColumn(EftBTask *task);

/* GS vertex register with fog (XYZF2), and the packets this file queues in the ordering table. */
typedef struct EftBXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftBXyzf;

typedef struct EftBRgbaq {
    u8 r, g, b, a;
    f32 q;
} EftBRgbaq;

typedef struct EftBSt {
    f32 s, t;
} EftBSt;

/* DMA tag + REGLIST GIF tag (12 registers): PRIM, TEX0, RGBAQ, four (ST, XYZF2), NOP: one flat textured strip. */
typedef struct EftBQuadPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000007: NEXT, 7 quadwords */
    /* 0x04 */ struct EftBQuadPkt *next;
    /* 0x08 */ u32 vif0;         /* 0x10000000 */
    /* 0x0C */ u32 vif1;         /* 0x50000007: DIRECT, 7 quadwords */
    /* 0x10 */ u64 gifTag;       /* 0xC400000000008001: NLOOP 1, EOP, REGLIST, 12 registers */
    /* 0x18 */ u64 regs;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftBRgbaq rgbaq;
    /* 0x38 */ EftBSt st0;
    /* 0x40 */ EftBXyzf xyz0;
    /* 0x48 */ EftBSt st1;
    /* 0x50 */ EftBXyzf xyz1;
    /* 0x58 */ EftBSt st2;
    /* 0x60 */ EftBXyzf xyz2;
    /* 0x68 */ EftBSt st3;
    /* 0x70 */ EftBXyzf xyz3;
    /* 0x78 */ u64 nop;
} EftBQuadPkt; /* size 0x80 */

/* DMA tag + REGLIST GIF tag (12 registers): PRIM, TEX0, three (RGBAQ, ST, XYZF2), NOP: one gouraud triangle. */
typedef struct EftBTriPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000007 */
    /* 0x04 */ struct EftBTriPkt *next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;         /* 0x50000007 */
    /* 0x10 */ u64 gifTag;       /* 0xC400000000008001 */
    /* 0x18 */ u64 regs;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftBRgbaq rgbaq0;
    /* 0x38 */ EftBSt st0;
    /* 0x40 */ EftBXyzf xyz0;
    /* 0x48 */ EftBRgbaq rgbaq1;
    /* 0x50 */ EftBSt st1;
    /* 0x58 */ EftBXyzf xyz1;
    /* 0x60 */ EftBRgbaq rgbaq2;
    /* 0x68 */ EftBSt st2;
    /* 0x70 */ EftBXyzf xyz2;
    /* 0x78 */ u64 nop;
} EftBTriPkt; /* size 0x80 */

/* DMA tag + A+D GIF tag: CLAMP_1 = 0 and CLAMP_2 = 0 (repeat), queued in front of a strip that scrolls its texture. */
typedef struct EftBClampPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000003 */
    /* 0x04 */ struct EftBClampPkt *next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;         /* 0x50000003 */
    /* 0x10 */ u64 gifTag;       /* 0x1000000000008002: NLOOP 2, EOP, PACKED, 1 register */
    /* 0x18 */ u64 regs;         /* 0xE: A+D */
    /* 0x20 */ u64 clamp1;
    /* 0x28 */ u64 clamp1Reg;    /* 8 */
    /* 0x30 */ u64 clamp2;
    /* 0x38 */ u64 clamp2Reg;    /* 9 */
} EftBClampPkt; /* size 0x40 */

/* DMA tag + REGLIST GIF tag (14 registers): PRIM, TEX0, four (RGBAQ, ST, XYZF2): one gouraud textured strip. */
typedef struct EftBStripPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000008 */
    /* 0x04 */ struct EftBStripPkt *next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;         /* 0x50000008 */
    /* 0x10 */ u64 gifTag;       /* 0xE400000000008001 */
    /* 0x18 */ u64 regs;         /* 0x42142142142160 */
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftBRgbaq rgbaq0;
    /* 0x38 */ EftBSt st0;
    /* 0x40 */ EftBXyzf xyz0;
    /* 0x48 */ EftBRgbaq rgbaq1;
    /* 0x50 */ EftBSt st1;
    /* 0x58 */ EftBXyzf xyz1;
    /* 0x60 */ EftBRgbaq rgbaq2;
    /* 0x68 */ EftBSt st2;
    /* 0x70 */ EftBXyzf xyz2;
    /* 0x78 */ EftBRgbaq rgbaq3;
    /* 0x80 */ EftBSt st3;
    /* 0x88 */ EftBXyzf xyz3;
} EftBStripPkt; /* size 0x90 */

/* Queues a screen-aligned textured quad centred on the projection of `pos`: w x h are half sizes in world units
   (scaled by the perspective of pos), rot turns it about the view axis, zScale scales its depth slot. Dropped when
   smaller than 2 pixels or when any corner leaves the GS drawing area. Sets pos->w to 1. */
void EftPrim_DrawBillboard(Vec4 *pos, Vec4 *color, s32 layer, s32 noDepth, u64 tex0, f32 w, f32 h, f32 u0, f32 v0,
                           f32 u1, f32 v1, f32 rot, f32 zScale) {
    Mtx44 m;
    Vec4 aspect;
    Vec4 a;
    Vec4 b;
    EftBIVec scr;
    EftBIVec ia;
    EftBIVec ib;
    f32 scale = gBtlCamView->screenDist;
    s32 sw;
    s32 sh;
    s32 z;
    s32 oz;
    s32 x0, y0, zz, x1, y1, x2, y2, x3, y3;
    s32 abe = 1;
    s32 ctx;
    s32 l;
    u32 alpha;
    EftBQuadPkt *p;
    OtEntry *e;

#ifdef PORT
    Vec4_Set(&aspect, 1.0f, 1.1666667f * Port_WideFactor(), 1.0f, 1.0f); /* pixels are wider still at 16:9 */
#else
    Vec4_Set(&aspect, 1.0f, 1.1666667f, 1.0f, 1.0f);
#endif
    pos->w = 1.0f;
    Vu0Cur_ProjectPoint(&scr, pos);
    sw = w;
    sh = h;
    scale *= 4096.0f;
    z = scr.z >> 8;
    scale /= scr.w;
    sw = (sw * (s32)scale) >> 12;
    sh = (sh * (s32)scale) >> 12;
    if (sw < 2 || sh < 2) {
        return;
    }
    w = sw;
    h = sh;
    Vec4_Set(&a, w, h, 0.0f, 1.0f);
    Vec4_Set(&b, -w, h, 0.0f, 1.0f);
    Mtx_StoreIdentity(&m);
    Mtx_RotateZ(&m, &m, rot);
    Mtx_MulVec4(&a, &m, &a);
    Mtx_MulVec4(&b, &m, &b);
    Vec4_Mul(&a, &a, &aspect);
    Vec4_Mul(&b, &b, &aspect);
    Vec4_ToInt(&ia, &a);
    Vec4_ToInt(&ib, &b);
    if (EftPrim_IsOffScreen(scr.x - ia.x, scr.y - ia.y, scr.z)) {
        return;
    }
    if (EftPrim_IsOffScreen(scr.x + ib.x, scr.y + ib.y, scr.z)) {
        return;
    }
    if (EftPrim_IsOffScreen(scr.x - ib.x, scr.y - ib.y, scr.z)) {
        return;
    }
    if (EftPrim_IsOffScreen(scr.x + ia.x, scr.y + ia.y, scr.z)) {
        return;
    }
    if (noDepth) {
        scr.z = 0xFFFFFF;
    }
    p = (EftBQuadPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    x0 = scr.x - ia.x;
    y0 = scr.y - ia.y;
    x1 = scr.x + ib.x;
    y1 = scr.y + ib.y;
    x2 = scr.x - ib.x;
    y2 = scr.y - ib.y;
    x3 = scr.x + ia.x;
    y3 = scr.y + ia.y;
    zz = scr.z;
    if (p == NULL) {
        return;
    }
    ctx = layer >= 2;
    p->prim = ((u64)abe << 6) | ((u64)ctx << 9) | 0x14;
    p->dmaTag = 0x20000007;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000007;
    p->gifTag = 0xC400000000008001;
    p->regs = 0xF42424242160 + (ctx << 4);
    p->next = NULL;
    p->rgbaq.r = color->x;
    p->rgbaq.g = color->y;
    p->rgbaq.b = color->z;
    alpha = color->w;
    p->xyz0.x = x0;
    p->xyz0.y = y0;
    l = layer;
    p->xyz1.x = x1;
    p->xyz1.y = y1;
    p->xyz2.x = x2;
    p->xyz2.y = y2;
    p->xyz3.x = x3;
    p->xyz3.y = y3;
    oz = z * zScale;
    p->xyz0.z = zz;
    p->xyz1.z = zz;
    p->xyz2.z = zz;
    p->xyz3.z = zz;
    if (l >= 2) {
        l -= 2;
    }
    p->rgbaq.a = alpha;
    p->rgbaq.q = 1.0f;
    p->st0.s = u0;
    p->st0.t = v0;
    p->st1.s = u0;
    p->st1.t = v1;
    p->st2.s = u1;
    p->st2.t = v0;
    p->st3.s = u1;
    p->st3.t = v1;
    p->xyz0.f = 0xFF;
    p->xyz1.f = 0xFF;
    p->xyz2.f = 0xFF;
    p->xyz3.f = 0xFF;
    p->tex0 = tex0;
    if (oz < 0) {
        e = &gOtZ[0].layer[l];
    } else if (oz >= 0x1000) {
        e = &gOtZ[0xFFF].layer[l];
    } else {
        e = &gOtZ[oz].layer[l];
    }
    e->tail->next = (OtPrim *)p;
    e->tail = (OtPrim *)p;
}

/* One vertex as ClipVtx_Set builds it for EftGfx_DrawPolyFixedZ: position, texture coordinates and colour. */
typedef struct EftBVert {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 uv;
    /* 0x20 */ Vec4 color;
} EftBVert; /* size 0x30 */

/* Draws a w x h quad (half sizes) at `pos`, facing the camera and rotated by `rot` about the view axis, as two
   triangles through the clipping triangle routine 0x131478. Its depth slot is that of `pos`. */
void EftPrim_DrawQuadDepth(Vec4 *pos, Vec4 *color, s32 arg2, s32 arg3, u64 tex0, f32 w, f32 h, f32 u0, f32 v0, f32 u1,
                           f32 v1, f32 rot) {
    EftBVert vert[9];
    Mtx44 m;
    Mtx44 inv;
    Vec4 corner[4];
    Vec4 uv[4];
    EftBIVec scr;
    s32 i;
    s32 z;

    Vec4_Set(&corner[0], -w, -h, 0.0f, 1.0f);
    Vec4_Set(&corner[1], w, -h, 0.0f, 1.0f);
    Vec4_Set(&corner[2], -w, h, 0.0f, 1.0f);
    Vec4_Set(&corner[3], w, h, 0.0f, 1.0f);
    Mtx_StoreIdentity(&m);
    Mtx_RotateZ(&m, &m, rot);
    Mtx_InverseRT(&inv, &gBtlCamView->world2view2);
    inv.m[3][2] = inv.m[3][1] = inv.m[3][0] = 0.0f;
    Mtx_Mul(&m, &inv, &m);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&corner[i], &m, &corner[i]);
        Vec3_Add(&corner[i], &corner[i], pos);
        corner[i].w = 1.0f;
    }
    Vu0Cur_ProjectPoint(&scr, pos);
    z = scr.z >> 8;
    if (scr.z >= 0) {
        Vec4_Set(&uv[0], u0, v0, 1.0f, 0.0f);
        Vec4_Set(&uv[1], u1, v0, 1.0f, 0.0f);
        Vec4_Set(&uv[2], u0, v1, 1.0f, 0.0f);
        Vec4_Set(&uv[3], u1, v1, 1.0f, 0.0f);
        for (i = 0; i < 2; i++) {
            ClipVtx_Set(&vert[0], &corner[i], &uv[i], color);
            ClipVtx_Set(&vert[1], &corner[i + 1], &uv[i + 1], color);
            ClipVtx_Set(&vert[2], &corner[i + 2], &uv[i + 2], color);
            EftGfx_DrawPolyFixedZ(vert, arg2, 0, 0, arg3, 0, tex0, z);
        }
    }
}

/* Same as EftPrim_DrawQuadDepth with the depth slot multiplied by zScale.
   The float parameters are declared BEFORE arg2 / arg3 / tex0 (integer and float arguments use separate
   registers, so callers are not affected): only with that order are the incoming float registers copied before
   the integer ones, which decides whether u0 or v1 gets $f29. The callers' local prototypes list the integers
   first; both forms pass every argument in the same register. */
void EftPrim_DrawQuadDepthScaled(Vec4 *pos, Vec4 *color, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot,
                                 s32 arg2, s32 arg3, u64 tex0, f32 zScale) {
    EftBVert vert[9];
    Mtx44 m;
    Mtx44 inv;
    Vec4 corner[4];
    Vec4 uv[4];
    EftBIVec scr;
    s32 i;
    s32 z;

    Vec4_Set(&corner[0], -w, -h, 0.0f, 1.0f);
    Vec4_Set(&corner[1], w, -h, 0.0f, 1.0f);
    Vec4_Set(&corner[2], -w, h, 0.0f, 1.0f);
    Vec4_Set(&corner[3], w, h, 0.0f, 1.0f);
    Mtx_StoreIdentity(&m);
    Mtx_RotateZ(&m, &m, rot);
    Mtx_InverseRT(&inv, &gBtlCamView->world2view2);
    inv.m[3][2] = inv.m[3][1] = inv.m[3][0] = 0.0f;
    Mtx_Mul(&m, &inv, &m);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&corner[i], &m, &corner[i]);
        Vec3_Add(&corner[i], &corner[i], pos);
        corner[i].w = 1.0f;
    }
    Vu0Cur_ProjectPoint(&scr, pos);
    z = scr.z >> 8;
    if (scr.z >= 0) {
        Vec4_Set(&uv[0], u0, v0, 1.0f, 0.0f);
        Vec4_Set(&uv[1], u1, v0, 1.0f, 0.0f);
        Vec4_Set(&uv[2], u0, v1, 1.0f, 0.0f);
        Vec4_Set(&uv[3], u1, v1, 1.0f, 0.0f);
        for (i = 0; i < 2; i++) {
            ClipVtx_Set(&vert[0], &corner[i], &uv[i], color);
            ClipVtx_Set(&vert[1], &corner[i + 1], &uv[i + 1], color);
            ClipVtx_Set(&vert[2], &corner[i + 2], &uv[i + 2], color);
            EftGfx_DrawPolyFixedZ(vert, arg2, 0, 0, arg3, 0, tex0, z * zScale);
        }
    }
}

/* Queues one gouraud textured triangle from projected vertices, float colours (0..255) and (s, t, q) texture
   coordinates. Skipped when all three alphas are 0.01 or less. A negative layer means layer 0 without blending;
   layers 2 and 3 are layers 0 and 1 drawn with GS context 2. */
void EftPrim_DrawTriangle(EftBIVec *v0, EftBIVec *v1, EftBIVec *v2, Vec4 *c0, Vec4 *c1, Vec4 *c2, Vec4 *uv0, Vec4 *uv1,
                          Vec4 *uv2, s32 unk1, s32 unk2, s32 layer, s32 z, u64 tex0) {
    s32 abe = 1;
    s32 ctx;
    s32 l;
    EftBTriPkt *p;
    OtEntry *e;

    if (c0->w <= 0.01f && c1->w <= 0.01f && c2->w <= 0.01f) {
        return;
    }
    if (layer < 0) {
        layer = 0;
        abe = 0;
    }
    ctx = layer >= 2;
    p = (EftBTriPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    p->prim = ((u64)abe << 6) | ((u64)ctx << 9) | 0x1B;
    p->dmaTag = 0x20000007;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000007;
    p->gifTag = 0xC400000000008001;
    p->regs = 0xF42142142160 + (ctx << 4);
    p->next = NULL;
    p->nop = 0;
    p->rgbaq0.r = c0->x;
    p->rgbaq0.g = c0->y;
    p->rgbaq0.b = c0->z;
    p->rgbaq0.a = c0->w;
    p->rgbaq0.q = uv0->z;
    p->rgbaq1.r = c1->x;
    p->rgbaq1.g = c1->y;
    p->rgbaq1.b = c1->z;
    p->rgbaq1.a = c1->w;
    p->rgbaq1.q = uv1->z;
    p->rgbaq2.r = c2->x;
    p->rgbaq2.g = c2->y;
    p->rgbaq2.b = c2->z;
    p->rgbaq2.a = c2->w;
    l = layer;
    if (l >= 2) {
        l -= 2;
    }
    p->rgbaq2.q = uv2->z;
    p->tex0 = tex0;
    p->st0.s = uv0->x;
    p->st0.t = uv0->y;
    p->st1.s = uv1->x;
    p->st1.t = uv1->y;
    p->st2.s = uv2->x;
    p->st2.t = uv2->y;
    p->xyz0.x = v0->x;
    p->xyz0.y = v0->y;
    p->xyz0.z = v0->z;
    p->xyz0.f = 0xFF;
    p->xyz1.x = v1->x;
    p->xyz1.y = v1->y;
    p->xyz1.z = v1->z;
    p->xyz1.f = 0xFF;
    p->xyz2.x = v2->x;
    p->xyz2.y = v2->y;
    p->xyz2.z = v2->z;
    p->xyz2.f = 0xFF;
    if (z < 0) {
        e = &gOtZ[0].layer[l];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[l];
    } else {
        e = &gOtZ[z].layer[l];
    }
    e->tail->next = (OtPrim *)p;
    e->tail = (OtPrim *)p;
}

/* 1 when a projected point cannot be drawn: behind the camera or outside the GS drawing area. */
s32 EftPrim_IsOffScreen(s32 x, s32 y, s32 z) {
    if (z <= 0) {
        return 1;
    } else if (x > 0xFFEF) {
        return 1;
    } else if (x <= 0) {
        return 1;
    } else if (y > 0xFFEF) {
        return 1;
    } else if (y <= 0) {
        return 1;
    }
    return 0;
}

/* Starts the burst of bubbles of a fighter that just went under water: 5 per unit of speed, 40 to 60. */
s32 EftBubble_StartBurst(s32 objId) {
    EftBubbleChr *chr;
    f32 n;

    if (gEftBubble == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) {
        return 0;
    }
    chr = &gEftBubble->chr[objId];
    if (EFT_BUBBLE_CHR_FLAGS(chr)->bursting) {
        return 0;
    }
    n = BtlCharApi_GetSpeed(objId) * 5.0f;
    chr->objId = objId;
    if (n < 40.0f) {
        chr->burst = 40.0f;
    } else if (n > 60.0f) {
        chr->burst = 60.0f;
    } else {
        chr->burst = n;
    }
    chr->unk4 = 0;
    EFT_BUBBLE_CHR_FLAGS(chr)->bursting = 1;
    return 1;
}

/* Per blast record hook of BtlScene_UpdateRecords: does nothing. */
void EftBubble_OnBlastRecord(void *rec) {
}

/* Task init: allocates the state and the pool, loads the textures; kills the task when the stage has fewer than 6. */
void EftBubble_Init(EftBTask *task) {
    s32 *stage;

    if (gEftBubble == NULL) {
        gEftBubble = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftBubbleWork));
        memset(gEftBubble, 0, sizeof(EftBubbleWork));
    }
    if (gEftBubble->chr == NULL) {
        gEftBubble->chr = BtlPool_Alloc(BtlPool_GetCurrent(), EFT_FIGHTER_CAPACITY * sizeof(EftBubbleChr));
        memset(gEftBubble->chr, 0, EFT_FIGHTER_CAPACITY * sizeof(EftBubbleChr));
    }
    if (gEftBubble->pool == NULL) {
        gEftBubble->pool = BtlPool_Alloc(BtlPool_GetCurrent(), EFT_BUBBLE_MAX * sizeof(EftBubble));
        memset(gEftBubble->pool, 0, EFT_BUBBLE_MAX * sizeof(EftBubble));
    }
    stage = BtlScene_GetStageData();
    if (BtlScene_GetPackEntrySize(stage, 0x12) > 0) {
        EftTexSet_Load32(&gEftBubble->tex, BtlScene_GetPackEntry(stage, 0x12));
        if (gEftBubble->tex.count < 6) {
            BtlTask_SetDead(task);
        }
    } else {
        BtlTask_SetDead(task);
    }
}

/* Task term: frees the pool, the per-fighter state and the state. */
void EftBubble_Term(void) {
    if (gEftBubble->pool != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftBubble->pool);
        gEftBubble->pool = NULL;
    }
    if (gEftBubble->chr != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftBubble->chr);
        gEftBubble->chr = NULL;
    }
    if (gEftBubble != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftBubble);
        gEftBubble = NULL;
    }
}

/* Task update: emits from the fighters and the cameras, then moves every bubble. */
void EftBubble_Update(void) {
    EftBubble_UpdateChrs();
    EftBubble_UpdateAmbient();
    EftBubble_UpdateList(&gEftBubble->head, &gEftBubble->tail);
}

/* Task post-update: refreshes the TEX0 of textures 5..8. */
void EftBubble_PostUpdate(void) {
    EftBubble_SetTexMode(1, 0);
}

/* Task draw: takes the rotation of the current view and draws the list. */
void EftBubble_Draw(void) {
    if (EftStage_IsDrawOn()) {
        if (BtlStage_IsReady()) {
            Mtx_InverseRT(&gEftBubble->camMtx, &gBtlCamView->world2view2);
            gEftBubble->camMtx.m[3][0] = gEftBubble->camMtx.m[3][1] = gEftBubble->camMtx.m[3][2] = 0.0f;
            EftBubble_DrawList(gEftBubble->head);
        }
    }
}

/* A pair of model nodes: a segment from the first to the second, or one point when the second is -1. */
typedef struct EftBubbleBone {
    /* 0x0 */ s32 from;
    /* 0x4 */ s32 to;
} EftBubbleBone;

/* Emits small bubbles along the body of a fighter: each point of 20 bone segments has a 1 in `chance` chance.
   The table is an array of pairs (structs): as a flat `s32 bones[40]` indexed with i * 2 the compiler walks a
   pointer through it, which the original does not (it recomputes i * 8 for each access). */
void EftBubble_EmitBody(s32 objId, s32 chance, s32 view) {
    /* Pairs of model nodes: a segment from the first to the second, or one point when the second is -1. */
    EftBubbleBone bones[20] = {
        { 0x0B, 0x0A },
        { 0x0A, 0x09 },
        { 0x09, 0x08 },
        { 0x08, -1 },
        { 0x0F, 0x0E },
        { 0x0E, 0x0D },
        { 0x0D, 0x0C },
        { 0x0C, -1 },
        { 0x12, 0x13 },
        { 0x13, 0x14 },
        { 0x14, 0x15 },
        { 0x15, -1 },
        { 0x20, 0x21 },
        { 0x21, 0x22 },
        { 0x22, 0x23 },
        { 0x23, -1 },
        { 0x03, 0x10 },
        { 0x10, 0x11 },
        { 0x11, 0x30 },
        { 0x30, -1 },
    };
    Vec4 a;
    Vec4 dir;
    Vec4 base;
    Vec4 step;
    Vec4 b;
    Vec4 vel;
    f32 spacing;
    f32 len;
    f32 size;
    f32 amp;
    f32 rise;
    f32 life;
    s32 i;
    s32 j;
    s32 n;

    BtlCharApi_GetDir(objId, &dir);
    spacing = BtlCharApi_GetHeight(objId) * 0.25f;
    if (spacing < 6.0f) {
        spacing = 6.0f;
    }
    for (i = 0; i < 20; i++) {
        BtlCharApi_GetNodePos(objId, bones[i].from, &a);
        Vec4_Copy(&base, &a);
        if (bones[i].to >= 0) {
            BtlCharApi_GetNodePos(objId, bones[i].to, &b);
            Vec4_Sub(&b, &b, &a);
            len = Vec3_Length(&b);
            if (spacing < len) {
                n = len / spacing + 1.0f;
                Vec3_Scale(&step, &b, 1.0f / n);
            } else {
                Vec4_Set(&step, 0.0f, 0.0f, 0.0f, 1.0f);
                n = 1;
            }
        } else {
            Vec4_Set(&step, 0.0f, 0.0f, 0.0f, 1.0f);
            n = 1;
        }
        for (j = 0; j < n; j++) {
            if (rand() % chance == 0) {
                size = EFTB_RAND01() * 0.4f + 0.4f;
                amp = EFTB_RAND01() * 0.2f + 0.1f;
                rise = EFTB_RAND01() * -0.05f - 0.05f;
                life = EFTB_RAND01() * 0.4f + 0.6f;
                size *= BtlCharApi_GetHeight(objId) * 0.05f;
                amp *= BtlCharApi_GetHeight(objId) * 0.05f;
                rise *= BtlCharApi_GetHeight(objId) * 0.05f;
                Vec4_Scale(&vel, &dir, EFTB_RAND01() * 0.3f + 0.7f);
                a.x = base.x + j * step.x;
                a.y = base.y + j * step.y;
                a.z = base.z + j * step.z;
                EftBubble_Add(a, vel, -8.0f, size, 128.0f, EFTB_RAND01() * 360.0f, rise, 6.0f,
                              EFTB_RAND01() * 3.14159265f, amp, 0x80, 0x80, 0x80, objId, view, 5, 0.25f, life, 0);
            }
        }
    }
}

/* Emits `count` big bubbles around a fighter (the burst after it dives in). */
void EftBubble_EmitBurst(s32 objId, s32 count) {
    Vec4 pos;
    Vec4 dir;
    Vec4 p;
    f32 size;
    f32 rise;
    f32 amp;
    f32 dist;
    f32 rot;
    f32 phase;
    s32 i;

    BtlCharApi_GetPos(objId, &pos);
    BtlCharApi_GetDir(objId, &dir);
    for (i = 0; i < count; i++) {
        size = EFTB_RAND01() * 4.5f + 3.0f;
        rise = EFTB_RAND01() * -0.2f - 0.2f;
        amp = EFTB_RAND01() * 0.5f + 0.3f;
        dist = EFTB_RAND01() * 10.0f;
        size *= BtlCharApi_GetHeight(objId) * 0.05f;
        rise *= BtlCharApi_GetHeight(objId) * 0.05f;
        amp *= BtlCharApi_GetHeight(objId) * 0.05f;
        dist *= BtlCharApi_GetHeight(objId) * 0.05f;
        /* The direction is passed by value, so `p` is never written: it is used uninitialised. */
        EftBubble_RandDir(p);
        Vec4_Scale(&p, &p, dist);
        Vec4_Add(&p, &p, &pos);
        Vec4_Scale(&dir, &dir, EFTB_RAND01() * 0.3f + 0.7f);
        rot = EFTB_RAND01() * 360.0f;
        phase = EFTB_RAND01() * 3.14159265f;
        EftBubble_Add(p, dir, -8.0f, size, 112.0f, rot, rise, 6.0f, phase, amp, 0x80, 0x80, 0x80, objId, -1,
                      (rand() & 3) ? 7 : 8, 0.5f, -1.0f, 0);
    }
}

/* Emits one bubble in front of a view's camera, at a random place of the screen. */
void EftBubble_EmitCamera(s32 view) {
    Vec4 pos;
    Vec4 tmp;
    Mtx44 m;
    extern EftBView *BtlCam_GetNativeView(s32);
    EftBView *v = BtlCam_GetNativeView(view);
    f32 x;
    f32 y;
    f32 size;
    s32 tex;

    Mtx_InverseRT(&m, &v->world2view2);
    Vec4_Scale(&pos, (Vec4 *)m.m[2], 10.0f);
    Vec4_Add(&pos, &pos, &v->camPos);
    pos.y += 20.0f;
    x = (rand() % 2001 - 1000) * 0.015f;
    y = (rand() % 2001 - 1000) * 0.015f;
    Vec4_Scale(&tmp, (Vec4 *)m.m[0], x);
    Vec4_Add(&pos, &pos, &tmp);
    Vec4_Scale(&tmp, (Vec4 *)m.m[1], y);
    Vec4_Add(&pos, &pos, &tmp);
    if (rand() % 6 != 0) {
        tex = 5;
        size = EFTB_RAND01() * 0.15f + 0.15f;
    } else {
        tex = 6;
        size = EFTB_RAND01() * 1.5f + 1.5f;
    }
    EftBubble_Add(pos, gEftBubbleZeroVel, 0.0f, size, 128.0f, 0.0f, -0.3f, 0.0f, EFTB_RAND01() * 3.14159265f, 0.1f,
                  0x80, 0x80, 0x80, -1, view, tex, 0.25f, -1.0f, 0);
}

/* Runs the two per-fighter emitters, unless the battle is paused. */
void EftBubble_UpdateChrs(void) {
    EftBubbleChr *chr;
    s32 i;

    if (Battle_GetWork()->flags & EFTB_BATTLE_PAUSE) {
        return;
    }
    chr = gEftBubble->chr;
    for (i = 0; i < EFT_FIGHTER_CAPACITY; i++, chr++) {
        if (EFT_BUBBLE_CHR_FLAGS(chr)->trail) {
            EftBubble_UpdateTrail(chr);
        }
        if (EFT_BUBBLE_CHR_FLAGS(chr)->bursting) {
            EftBubble_UpdateBurst(chr);
        }
    }
}

/* Continuous emission from the body (its flag is never set). */
void EftBubble_UpdateTrail(EftBubbleChr *chr) {
    EftBubble_EmitBody(chr->objId, 20, chr->objId);
}

/* One frame of a burst: emits `burst` bubbles and takes 8 off, until none are left. */
void EftBubble_UpdateBurst(EftBubbleChr *chr) {
    if (chr->burst > 0.0f) {
        EftBubble_EmitBurst(chr->objId, chr->burst);
        chr->burst -= 8.0f;
    } else {
        chr->burst = 0.0f;
        EFT_BUBBLE_CHR_FLAGS(chr)->bursting = 0;
    }
}

/* Bubbles in front of each camera that is under water, and from the body of a fighter in water. */
void EftBubble_UpdateAmbient(void) {
    f32 water = 0.0f;
    EftBubbleChr *chr;
    s32 views;
    s32 i;

    if (Battle_GetWork()->flags & EFTB_BATTLE_PAUSE) {
        return;
    }
    if (Battle_IsSplitScreen()) {
        return;
    }
    EftWater_GetSurfaceY(&water);
    views = Battle_IsSplitScreen() ? 2 : 1;
    chr = gEftBubble->chr;
    for (i = 0; i < views; i++) {
        if (!(EFTB_CAM_AT(i)->view.camPos.y < water)) {
            gEftBubble->camTimer[i] += (rand() & 3) ? 0.05f : 0.1f;
            if (gEftBubble->camTimer[i] > 1.0f) {
                gEftBubble->camTimer[i] -= 1.0f;
                EftBubble_EmitCamera(i);
            }
            if (!EFT_BUBBLE_CHR_FLAGS(&chr[i])->trail) {
                if (BtlCharApi_IsInWater(chr[i].objId)) {
                    EftBubble_EmitBody(chr[i].objId, 120, i);
                }
            }
        }
    }
}

/* Returns a cleared free bubble, or NULL when all 100 are in use. */
EftBubble *EftBubble_AllocSlot(void) {
    EftBubble *b = gEftBubble->pool;
    s32 i;

    for (i = 0; i < EFT_BUBBLE_MAX; i++, b++) {
        if (!b->active) {
            memset(b, 0, sizeof(EftBubble));
            return b;
        }
    }
    return NULL;
}

/* Adds a bubble at the tail of the list. life > 0 fades it out over that many seconds. */
void EftBubble_Add(EftBVec pos, EftBVec vel, f32 drag, f32 size, f32 alpha, f32 rot, f32 rise, f32 riseGrow, f32 phase,
                   f32 amp, u8 r, u8 g, u8 b, s32 objId, s32 view, u8 tex, f32 phaseStep, f32 life, u8 layer) {
    EftBubble *bub = EftBubble_AllocSlot();

    if (bub != NULL) {
        if (gEftBubble->head == NULL) {
            gEftBubble->head = bub;
            bub->prev = NULL;
            bub->next = NULL;
        } else {
            bub->prev = gEftBubble->tail;
            bub->next = NULL;
        }
        if (gEftBubble->tail != NULL) {
            gEftBubble->tail->next = bub;
        }
        gEftBubble->tail = bub;
        Vec4_Copy(&bub->pos, &pos);
        Vec4_Copy(&bub->vel, &vel);
        if (drag == 0.0f) {
            bub->drag = 0.0f;
        } else {
            bub->drag = 1.0f / drag;
        }
        bub->rot = rot;
        bub->size = size;
        bub->alpha = alpha;
        if (life <= 0.0f) {
            bub->alphaStep = 0.0f;
        } else {
            bub->alphaStep = -alpha / (life * 30.0f);
        }
        bub->rise = rise;
        if (riseGrow == 0.0f) {
            bub->riseGrow = 0.0f;
        } else {
            bub->riseGrow = 1.0f / riseGrow;
        }
        bub->phase = phase;
        bub->amp = amp;
        bub->phaseStep = phaseStep;
        bub->r = r;
        bub->g = g;
        bub->b = b;
        bub->objId = objId;
        bub->view = view;
        bub->tex = tex;
        bub->layer = layer;
        bub->active = 1;
        gEftBubble->count++;
    }
}

/* Moves every bubble and unlinks the ones that ended. Does nothing while the battle is paused. */
void EftBubble_UpdateList(EftBubble **head, EftBubble **tail) {
    EftBubble *b;
    EftBubble *cur;
    s32 dead;

    if (gEftBubble->count == 0) {
        return;
    }
    if (Battle_GetWork()->flags & EFTB_BATTLE_PAUSE) {
        return;
    }
    b = *head;
    while (b != NULL) {
        cur = b;
        dead = EftBubble_Step(b);
        b = b->next;
        if (dead) {
            cur->active = 0;
            if (cur->prev == NULL) {
                *head = cur->next;
            } else {
                cur->prev->next = cur->next;
            }
            if (cur->next == NULL) {
                *tail = cur->prev;
            } else {
                cur->next->prev = cur->prev;
            }
            cur->prev = NULL;
            cur->next = NULL;
            gEftBubble->count--;
        }
    }
}

/* One frame of a bubble. Returns 1 when it faded out, reached the surface, or its camera left the water. */
s32 EftBubble_Step(EftBubble *b) {
    f32 water;

    Vec4_Add(&b->pos, &b->pos, &b->vel);
    b->vel.x += b->vel.x * b->drag;
    b->pos.y += b->rise;
    b->vel.y += b->vel.y * b->drag;
    b->rise += b->rise * b->riseGrow;
    b->vel.z += b->vel.z * b->drag;
    b->pos.x += Mathf_CosFast(b->phase) * b->amp;
    b->pos.z += Mathf_SinFast(b->phase) * b->amp;
    b->phase += b->phaseStep;
    if (b->phase > 3.14159265f) {
        b->phase -= 6.2831853f;
    }
    b->alpha += b->alphaStep;
    if (b->alpha <= 0.0f) {
        b->alpha = 0.0f;
        return 1;
    }
    if (!EftWater_GetSurfaceY(&water)) {
        water = 5.0f;
    }
    if (b->pos.y - b->size * 0.5f < water) {
        return 1;
    }
    if (b->view != -1) {
        extern EftBView *BtlCam_GetNativeView(s32);
        if (BtlCam_GetNativeView(b->view)->camPos.y < water) {
            return 1;
        }
    }
    return 0;
}

/* Draws the bubbles that belong to the view being drawn. */
void EftBubble_DrawList(EftBubble *b) {
    Mtx44 m;
    Vec4 pos;

    Mtx_Copy(&m, &gBtlCamView->world2screen);
    for (; b != NULL; b = b->next) {
        if (b->active) {
            Vec4_Copy(&pos, &b->pos);
            if (b->view == BtlScene_IsSecondView() || b->view == -1) {
                EftBubble_DrawOne(pos, m, b->size, b->rot, b->r, b->g, b->b, (u32)b->alpha,
                                  &gEftBubble->tex.tex[b->tex], b->layer);
            }
        }
    }
}

/* 1 when a projected corner is in front of the camera and inside the visible part of the GS area. */
s32 EftBubble_IsOnScreen(EftBIVec v) {
    if (v.z <= 0) {
        return 0;
    }
    if (v.x > 0x97FF) {
        return 0;
    }
    if (v.x <= 0x6800) {
        return 0;
    }
    if (v.y > 0x94FF) {
        return 0;
    }
    if (v.y <= 0x6B00) {
        return 0;
    }
    return 1;
}

/* Builds a random direction. The vector is taken by value, so the caller never sees the result. */
void EftBubble_RandDir(EftBVec v) {
    Mtx44 m;
    f32 a;

    a = EFTB_RAND01() * 6.2831853f;
    v.x = Mathf_SinFast(a);
    v.y = Mathf_CosFast(a);
    v.z = v.w = 0.0f;
    a = EFTB_RAND01() * 6.2831853f;
    Mtx_StoreIdentity(&m);
    Mtx_RotateY(&m, &m, EftMath_WrapAngle(a));
    Mtx_MulVec4(&v, &m, &v);
}

/* Draws one bubble: a camera-facing quad of `size`, rotated by `rot` degrees, as one textured strip. Nothing is
   drawn unless all four corners are on screen. */
void EftBubble_DrawOne(EftBVec pos, EftBMtx m, f32 size, f32 rot, u8 r, u8 g, u8 b, u8 a, EftBTex *tex, u8 layer) {
    Mtx44 view;
    Vec4 corner[4];
    EftBIVec scr[4];
    Mtx44 rotM;
    f32 half;
    s32 i;
    s32 z;
    s32 l;
    EftBQuadPkt *p;
    OtEntry *e;

    Mtx_Copy(&view, &gEftBubble->camMtx);
    if (rot != 0.0f) {
        Mtx_StoreIdentity(&rotM);
        Mtx_RotateZ(&rotM, &rotM, EftMath_WrapAngle(rot * 3.14159265f / 180.0f));
        Mtx_Mul(&view, &view, &rotM);
    }
    half = size * 0.5f;
    Vec4_Copy((Vec4 *)view.m[3], &pos);
    Vec4_Set(&corner[0], -half, -half, 0.0f, 1.0f);
    Vec4_Set(&corner[1], half, -half, 0.0f, 1.0f);
    Vec4_Set(&corner[2], -half, half, 0.0f, 1.0f);
    Vec4_Set(&corner[3], half, half, 0.0f, 1.0f);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&corner[i], &view, &corner[i]);
        Mtx_ProjectPoint(&scr[i], &m, &corner[i]);
        if (!EftBubble_IsOnScreen(scr[i])) {
            return;
        }
    }
    p = (EftBQuadPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    if (p == NULL) {
        return;
    }
    l = layer;
    if (l >= 2) {
        l -= 2;
    }
    p->prim = 0x54;
    p->dmaTag = 0x20000007;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000007;
    p->gifTag = 0xC400000000008001;
    p->regs = 0xF42424242160;
    p->rgbaq.r = r;
    p->rgbaq.g = g;
    p->rgbaq.b = b;
    p->rgbaq.a = a;
    p->next = NULL;
    p->rgbaq.q = 1.0f;
    p->st0.s = 0.0f;
    p->st0.t = 0.0f;
    p->st1.s = 1.0f;
    p->st1.t = 0.0f;
    p->st2.s = 0.0f;
    p->st2.t = 1.0f;
    p->st3.s = 1.0f;
    p->st3.t = 1.0f;
    p->xyz0.x = scr[0].x;
    p->xyz0.y = scr[0].y;
    p->xyz0.z = scr[0].z;
    p->xyz0.f = 0xFF;
    p->xyz1.x = scr[1].x;
    p->xyz1.y = scr[1].y;
    p->xyz1.z = scr[1].z;
    p->xyz1.f = 0xFF;
    p->xyz2.x = scr[2].x;
    p->xyz2.y = scr[2].y;
    p->xyz2.z = scr[2].z;
    p->xyz2.f = 0xFF;
    p->xyz3.x = scr[3].x;
    p->xyz3.y = scr[3].y;
    p->xyz3.z = scr[3].z;
    p->xyz3.f = 0xFF;
    z = (scr[0].z + scr[2].z + (scr[3].z + scr[1].z)) >> 10;
    p->tex0 = tex->tex0;
    if (z < 0) {
        e = &gOtZ[0].layer[l];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[l];
    } else {
        e = &gOtZ[z].layer[l];
    }
    e->tail->next = (OtPrim *)p;
    e->tail = (OtPrim *)p;
}

/* Refreshes the TEX0 of textures 5..8 with the given mode bits, when any bubble exists. */
void EftBubble_SetTexMode(s32 a, s32 b) {
    s32 i;

    if (gEftBubble->count != 0) {
        for (i = 5; i < 9; i++) {
            gEftBubble->tex.tex[i].tex0 = EftVram_AddTex(&gEftBubble->tex.tex[i], a, b);
        }
    }
}

/* Task init: allocates the state and loads the sheet. */
void EftStageScroll_Init(void) {
    gEftStageScroll = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftStageScroll));
    memset(gEftStageScroll, 0, sizeof(EftStageScroll));
    EftStageScroll_Load();
}

/* Task term. */
void EftStageScroll_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftStageScroll);
    gEftStageScroll = NULL;
}

/* Task update: advances the scroll offset and wraps it into -size..size. Stops while paused or loading. */
void EftStageScroll_Update(void) {
    if (Battle_GetWork()->flags & EFTB_BATTLE_PAUSE) {
        return;
    }
    if (!BtlStage_IsReady()) {
        return;
    }
    if (!gEftStageScroll->enabled) {
        return;
    }
    gEftStageScroll->offX += gEftStageScroll->dirX * gEftStageScroll->speed;
    gEftStageScroll->offY += gEftStageScroll->dirY * gEftStageScroll->speed;
    if (gEftStageScroll->offX > gEftStageScroll->size) {
        gEftStageScroll->offX = gEftStageScroll->offX - gEftStageScroll->size;
    } else if (gEftStageScroll->offX < -gEftStageScroll->size) {
        gEftStageScroll->offX = gEftStageScroll->offX + gEftStageScroll->size;
    }
    if (gEftStageScroll->offY > gEftStageScroll->size) {
        gEftStageScroll->offY = gEftStageScroll->offY - gEftStageScroll->size;
    } else if (gEftStageScroll->offY < -gEftStageScroll->size) {
        gEftStageScroll->offY = gEftStageScroll->offY + gEftStageScroll->size;
    }
}

/* Task reset: reloads the sheet (a new stage file after a stage change). */
void EftStageScroll_Reset(void) {
    EftStageScroll_Load();
}

/* Task draw: nothing (the sheet is drawn by the stage). */
void EftStageScroll_DrawStub(void) {
    if (!BtlStage_IsReady()) {
        return;
    }
}

/* Draws the sheet: called by the stage draw, once per view. */
void EftStageScroll_Draw(void) {
    Mtx44 m;
    u64 *p;

    if (gEftStageScroll == NULL) {
        return;
    }
    if (!BtlStage_IsReady()) {
        return;
    }
    EftGfx_UpdateClipPlanes();
    if (!gEftStageScroll->enabled) {
        return;
    }
    if (!EftStage_IsDrawOn()) {
        return;
    }
    EftStageScroll_PlaceTextures(1, 0);
    p = Dma_BeginDirect();
    p[0] = 0x1000000000008003;
    p[1] = 0xE;
    p += 2;
    p[0] = 0x44;
    p[1] = 0x42;
    p += 2;
    p[0] = 0x50000;
    p[1] = 0x47;
    p += 2;
    p[0] = 0;
    p[1] = 8;
    p += 2;
    Dma_EndDirect(p);
    Mtx_StoreIdentity(&m);
    m.m[3][0] = 0.0f;
    m.m[3][1] = 0.0f;
    m.m[3][2] = 0.0f;
    EftMesh_SetRepeat(gEftStageScroll->model, 1);
    EftMesh_SetUvScale(gEftStageScroll->model, gEftStageScroll->size);
    EftMesh_SetUvOfs(gEftStageScroll->model, gEftStageScroll->offX, gEftStageScroll->offY);
    EftMesh_SetMtx(gEftStageScroll->model, &m);
    EftMesh_DrawNow(gEftStageScroll->model, 1);
}

/* Uploads the sheet's textures one after the other from VRAM block 0x2A20 (palettes from 0x2A00) and rewrites
   the TBP0, TCC, TFX and CBP fields of each TEX0 to match. */
void EftStageScroll_PlaceTextures(s32 tcc, s32 tfx) {
    s32 tbp = 0x2A20;
    s32 cbp = 0x2A00;
    EftBTexSet *set = &gEftStageScroll->tex;
    s32 i;

    for (i = 0; i < set->count; i++) {
        set->tex[i].tex0 = (set->tex[i].tex0 & 0xFFF80003FFFFC000) | ((u64)tcc << 34) | (((u64)tfx << 35) | (u32)tbp)
                           | ((u64)cbp << 37);
        Tex_Upload(set->tex[i].img, tbp, cbp);
        tbp += set->tex[i].img->texBlocks;
        cbp += set->tex[i].img->clutBlocks;
    }
}

/* Reads the sheet from the stage file: size, speed, direction (random when negative), textures and model. */
void EftStageScroll_Load(void) {
    Vec4 dir;
    s32 v;
    u8 *file = gCommonRes->stage;
    u8 *p;
    u8 *texPack;
    f32 deg;
    f32 angle;

    p = file + (*(u32 *)(file + 0x1C) / 4) * 4;
    texPack = file + (*(u32 *)(file + 0x18) / 4) * 4;
    gEftStageScroll->enabled = 1;
    EFTB_READ_INT(v, p);
    gEftStageScroll->size = v;
    EFTB_READ_INT(v, p);
    gEftStageScroll->unk2B8 = v;
    EFTB_READ_INT(v, p);
    gEftStageScroll->speed = v * 0.00001f * gEftStageScroll->size;
    EFTB_READ_INT(v, p);
    gEftStageScroll->angle = v;
    if (gEftStageScroll->angle < 0.0f) {
        deg = EFTB_RAND01() * 360.0f;
    } else {
        deg = gEftStageScroll->angle;
    }
    angle = EftMath_WrapAngle(deg * 3.14159265f / 180.0f);
    dir.x = cosf(angle);
    dir.y = 0.0f;
    dir.z = sinf(angle);
    dir.w = 1.0f;
    gEftStageScroll->dirX = dir.x;
    gEftStageScroll->dirY = dir.z;
    EftTexSet_Load32(&gEftStageScroll->tex, texPack);
    EftMesh_Init(gEftStageScroll->model, p);
    EftMesh_SetTex(gEftStageScroll->model, &gEftStageScroll->tex);
    EftMesh_SetClip(gEftStageScroll->model, 1);
}

/* Creates one column task from its parameters. */
void *EftGeyser_Create(EftGeyserParam *param) {
    if (gEftGeyserMgr == NULL) {
        return NULL;
    }
    return BtlTaskList_AddTail(gEftGeyserMgr->list, gEftGeyserClass, param);
}

/* The work of a live column task, NULL for anything else. */
EftGeyser *EftGeyser_GetWork(EftBTask *task) {
    if (task == NULL) {
        return NULL;
    }
    if (task->cls[0] != EftGeyser_Update) {
        return NULL;
    }
    if (EFTB_TASK_FLAGS(task)->dying) {
        return NULL;
    }
    return task->work;
}

/* Kills a live column task. */
void EftGeyser_Kill(EftBTask *task) {
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftGeyser_Update) {
        return;
    }
    if (EFTB_TASK_FLAGS(task)->dying) {
        return;
    }
    BtlTask_SetDead(task);
}

/* Manager init: loads the textures and creates a column for every record of the stage. */
void EftGeyser_MgrInit(EftBTask *task) {
    EftGeyserParam param;
    s32 *stage = BtlScene_GetStageData();
    s32 count = BtlStage_GetFxResA();
    EftGeyserRec *res;
    EftGeyserRec *rec;
    s32 i;

    gEftGeyserMgr = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftGeyserMgr));
    memset(gEftGeyserMgr, 0, sizeof(EftGeyserMgr));
    gEftGeyserMgr->list = BtlTask_CreateChildList(task, count + 1, sizeof(EftGeyser));
    EftTexSet_Load32(&gEftGeyserMgr->tex, BtlScene_GetPackEntry(stage, 0x12));
    res = BtlStage_GetFxResA2();
    if (res != NULL) {
        for (i = 0; i < count; i++) {
            rec = &res[i];
            memset(&param, 0, sizeof(EftGeyserParam));
            Vec4_Set(&param.color, rec->r, rec->g, rec->b, rec->a);
            Vec4_Copy(&param.curColor, &param.color);
            param.width = rec->width;
            param.height = rec->height;
            param.unk48 = 1.0f;
            param.unk4C = 1.0f;
            param.sink = 2.0f;
            param.scroll = rec->scroll;
            param.hold = rec->hold;
            param.fade = rec->fade;
            param.wait = rec->wait;
            param.pos.x = rec->x;
            param.pos.y = rec->y;
            param.pos.z = rec->z;
            param.pos.w = 1.0f;
            Vec4_Copy(&param.base, &param.pos);
            param.base.y = param.pos.y + param.height;
            EftGeyser_Create(&param);
        }
    }
}

/* Manager term. */
void EftGeyser_MgrTerm(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftGeyserMgr);
    gEftGeyserMgr = NULL;
}

/* Manager update: refreshes the TEX0 of texture 1 while any column exists. */
void EftGeyser_MgrUpdate(void) {
    if (((s32 *)gEftGeyserMgr->list)[1] != 0) {
        gEftGeyserMgr->tex.tex[1].tex0 = EftVram_AddTex(&gEftGeyserMgr->tex.tex[1], 1, 0);
    }
}

/* Column init: keeps the parameters twice, picks the first delay and creates the two emitters. */
void EftGeyser_Init(EftBTask *task, EftGeyserParam *param) {
    EftGeyser *w = task->work;

    memset(w, 0, sizeof(EftGeyser));
    w->init = *param;
    w->cur = *param;
    w->tex = &gEftGeyserMgr->tex;
    w->delay = rand() % 180;
    EftGeyser_Restart(task, w);
    EftGeyser_StartSmoke(task);
    EftGeyser_StartSteam(task);
}

/* Column term: frees the two emitters. */
void EftGeyser_Term(EftBTask *task) {
    EftGeyser *w = task->work;

    if (w->emitterB != NULL) {
        EftSteam_Stop(w->emitterB);
        w->emitterB = NULL;
    }
    if (w->emitterA != NULL) {
        EftSmoke_Stop(w->emitterA);
        w->emitterA = NULL;
    }
}

/* Column reset: nothing. */
void EftGeyser_ResetStub(void) {
}

/* Column update: wait, rise and fade in, hold, sink and fade out, wait again; scrolls the texture throughout. */
void EftGeyser_Update(EftBTask *task) {
    EftGeyser *w = task->work;

    if (BtlScene_IsTimeStopped()) {
        return;
    }
    switch (task->state) {
    case 0:
        if (--w->delay <= 0) {
            task->state = 1;
        }
        break;
    case 1:
        EftGeyser_Restart(task, w);
        task->state++;
        /* fallthrough */
    case 2:
        w->cur.base.y = w->cur.pos.y;
        task->state++;
        break;
    case 3:
        w->cur.curColor.w += w->alphaStep;
        if (w->cur.curColor.w > w->init.color.w) {
            w->cur.curColor.w = w->init.color.w;
        }
        if (--w->cur.hold <= 0) {
            EftSteam_SetPaused(w->emitterB, 1);
            w->cur.hold = 0;
            task->state++;
        }
        break;
    case 4:
        w->cur.curColor.w -= w->alphaStep;
        w->cur.base.y += w->cur.sink;
        if (w->cur.curColor.w <= 0.0f) {
            w->cur.curColor.w = 0.0f;
            task->state++;
        }
        break;
    case 5:
        if (--w->cur.wait <= 0) {
            w->cur.wait = 0;
            task->state = 1;
        }
        break;
    }
    w->texV -= w->cur.scroll;
    if (w->texV > 1.0f) {
        w->texV -= 1.0f;
    }
    if (w->texV < 0.0f) {
        w->texV += 1.0f;
    }
}

/* Column draw: with the stage visible, draws the column under the world-to-screen matrix of the view. */
void EftGeyser_Draw(EftBTask *task) {
    if (gBtlStage->flags & 1) {
        return;
    }
    if (!EftStage_IsDrawOn()) {
        return;
    }
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    EftGeyser_DrawColumn(task);
    Vu0Cur_Pop();
}

/* Starts a new eruption: current parameters = initial ones, alpha 0, emitters repositioned. */
void EftGeyser_Restart(EftBTask *task, EftGeyser *w) {
    w->cur = w->init;
    Vec4_Copy(&w->cur.curColor, &w->cur.color);
    w->cur.curColor.w = 0.0f;
    w->alphaStep = w->init.color.w / w->cur.fade;
    EftSteam_SetPaused(w->emitterB, 0);
    EftSteam_SetPos(w->emitterB, w);
    EftSmoke_SetPos(w->emitterA, w);
}

/* Draws the column as a ribbon of three camera-facing segments between four points spaced a quarter of the
   height apart, upwards from the base. The texture runs once along the ribbon and scrolls by texV; the colour is
   the current colour scaled by 0x137F60(), and the top edge has alpha 0. */
#if 0
/* NON-MATCHING: not matched (register allocation throughout; 631 instructions). Written from the
   disassembly: same calls in the same order, same packets. */
void EftGeyser_DrawColumn(EftBTask *task) {
    Vec4 pt[4];
    Vec4 cam;
    Vec4 toCam;
    Vec4 col[4];
    Vec4 side;
    Vec4 prev[2];
    EftBIVec scr[4];
    Vec4 quad[4];
    Vec4 uv[4];
    EftBVec uvInit[4] = {
        { 0.0f, 0.0f, 1.0f, 1.0f },
        { 1.0f, 0.0f, 1.0f, 1.0f },
        { 0.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
    };
    s32 first = 1;
    EftGeyser *w = task->work;
    f32 v = 0.33333334f;
    f32 dv = v;
    f32 vPrev = 0.0f;
    f32 width = w->cur.width;
    f32 texV = w->texV;
    f32 fade;
    s32 i;
    s32 z;
    s32 abe;
    u64 tex0;
    EftBClampPkt *q;
    EftBStripPkt *p;
    OtEntry *e;

    fade = EftStage_GetTintScale();
    Vec4_Copy(&cam, &gBtlCamView->pos);
    for (i = 0; i < 4; i++) {
        Vec4_Copy(&uv[i], &uvInit[i]);
    }
    for (i = 0; i < 4; i++) {
        pt[i].x = w->cur.base.x;
        pt[i].y = w->cur.base.y - w->cur.height * 0.25f * i;
        pt[i].z = w->cur.base.z;
        pt[i].w = 1.0f;
    }
    Vec3_Scale(&col[0], &w->cur.curColor, fade);
    Vec3_Scale(&col[1], &w->cur.curColor, fade);
    Vec3_Scale(&col[2], &w->cur.curColor, fade);
    Vec3_Scale(&col[3], &w->cur.curColor, fade);
    for (i = 0; i < 3; i++) {
        Vec4_Sub(&side, &pt[i + 1], &pt[i]);
        Vec4_Sub(&toCam, &pt[i], &cam);
        Vec3_Cross(&side, &side, &toCam);
        Vec3_Normalize(&side, &side);
        if (first) {
            Vec3_ScaleAdd(&quad[0], &side, &pt[i], width);
            Vec3_ScaleAdd(&quad[1], &side, &pt[i], -width);
            first = 0;
        } else {
            Vec4_Copy(&quad[0], &prev[0]);
            Vec4_Copy(&quad[1], &prev[1]);
        }
        Vec3_ScaleAdd(&quad[2], &side, &pt[i + 1], width);
        Vec3_ScaleAdd(&quad[3], &side, &pt[i + 1], -width);
        Vec4_Copy(&prev[0], &quad[2]);
        Vec4_Copy(&prev[1], &quad[3]);
        quad[0].w = quad[3].w = quad[2].w = quad[1].w = 1.0f;
        if (Vu0Cur_ProjectPoints(scr, quad, 4)) {
            uv[1].y = vPrev + texV;
            uv[3].y = v + texV;
            uv[0].y = vPrev + texV;
            z = (scr[0].z + scr[2].z + (scr[3].z + scr[1].z)) >> 10;
            uv[2].y = v + texV;
            if (i == 2) {
                col[2].w = 0.0f;
                col[3].w = 0.0f;
            }
            abe = 1;
            tex0 = w->tex->tex[1].tex0;
            if (!(col[0].w <= 0.0f && col[1].w <= 0.0f && col[2].w <= 0.0f)) {
                if (z < 0) {
                    e = &gOtZ[0].layer[0];
                } else if (z >= 0x1000) {
                    e = &gOtZ[0xFFF].layer[0];
                } else {
                    e = &gOtZ[z].layer[0];
                }
                q = (EftBClampPkt *)gOtCur;
                gOtCur = (u32 *)(q + 1);
                q->dmaTag = 0x20000003;
                q->vif1 = 0x50000003;
                q->gifTag = 0x1000000000008002;
                q->regs = 0xE;
                q->clamp1Reg = 8;
                q->clamp2Reg = 9;
                q->next = NULL;
                q->vif0 = 0x10000000;
                q->clamp1 = 0;
                q->clamp2 = 0;
                e->tail->next = (OtPrim *)q;
                e->tail = (OtPrim *)q;
                p = (EftBStripPkt *)gOtCur;
                gOtCur = (u32 *)(p + 1);
                p->vif0 = 0x10000000;
                p->prim = ((u64)abe << 6) | 0x1C;
                p->dmaTag = 0x20000008;
                p->vif1 = 0x50000008;
                p->gifTag = 0xE400000000008001;
                p->regs = 0x42142142142160;
                p->next = NULL;
                p->rgbaq0.r = col[0].x;
                p->rgbaq0.g = col[0].y;
                p->rgbaq0.b = col[0].z;
                p->rgbaq0.a = col[0].w;
                p->rgbaq0.q = uv[0].z;
                p->rgbaq1.r = col[1].x;
                p->rgbaq1.g = col[1].y;
                p->rgbaq1.b = col[1].z;
                p->rgbaq1.a = col[1].w;
                p->rgbaq1.q = uv[1].z;
                p->rgbaq2.r = col[2].x;
                p->rgbaq2.g = col[2].y;
                p->rgbaq2.b = col[2].z;
                p->rgbaq2.a = col[2].w;
                p->rgbaq2.q = uv[2].z;
                p->rgbaq3.r = col[3].x;
                p->rgbaq3.g = col[3].y;
                p->rgbaq3.b = col[3].z;
                p->rgbaq3.a = col[3].w;
                p->tex0 = tex0;
                p->rgbaq3.q = uv[3].z;
                p->st0.s = uv[0].x;
                p->st0.t = uv[0].y;
                p->st1.s = uv[1].x;
                p->st1.t = uv[1].y;
                p->st2.s = uv[2].x;
                p->st2.t = uv[2].y;
                p->st3.s = uv[3].x;
                p->st3.t = uv[3].y;
                p->xyz0.x = scr[0].x;
                p->xyz0.y = scr[0].y;
                p->xyz0.z = scr[0].z;
                p->xyz0.f = 0xFF;
                p->xyz1.x = scr[1].x;
                p->xyz1.y = scr[1].y;
                p->xyz1.z = scr[1].z;
                p->xyz1.f = 0xFF;
                p->xyz2.x = scr[2].x;
                p->xyz2.y = scr[2].y;
                p->xyz2.z = scr[2].z;
                p->xyz2.f = 0xFF;
                p->xyz3.x = scr[3].x;
                p->xyz3.y = scr[3].y;
                p->xyz3.z = scr[3].z;
                p->xyz3.f = 0xFF;
                if (z < 0) {
                    e = &gOtZ[0].layer[0];
                } else if (z >= 0x1000) {
                    e = &gOtZ[0xFFF].layer[0];
                } else {
                    e = &gOtZ[z].layer[0];
                }
                e->tail->next = (OtPrim *)p;
                e->tail = (OtPrim *)p;
            }
            vPrev += dv;
            v += dv;
            if (i >= 2) {
                v = 1.0f;
            }
        }
    }
}
#else
RODATA_ALIGN16();
INCLUDE_RODATA("asm/nonmatchings/battle/eft_b", D_002EC6E0);
INCLUDE_ASM("asm/nonmatchings/battle/eft_b", EftGeyser_DrawColumn);
#endif
