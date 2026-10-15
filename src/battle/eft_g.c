#include "common.h"
#include "battle/btl_pool.h"
#include "battle/eft_g.h"
#include "sys/gfx_ot.h"

/*
 * Effect code, 0x147050..0x14B108. See include/battle/eft_g.h for the five pieces and their layouts.
 *
 * One function (EftStorm_DrawBolts) is INCLUDE_ASM with the C attempt in `#if 0` above it and a note on what differs;
 * turning the `#if 0` into `#if 1` and dropping the INCLUDE_ASM line gives a file that fdiff can compile.
 * EftSmoke_Draw and EftBound_BuildWall match since cleanup W1 (EftSmoke_Draw emits a 4-byte .sdata constant, 0x2FE9EC).
 *
 * Callees that have no name yet, from a first read of how they are used here:
 *   Vec4_Lerp(out,a,b,t) linear interpolation of two vectors
 *   Mtx_InverseRT(out, m)    inverse of a view matrix: gives the camera's world matrix (row 3 = position)
 *   EftTexSet_Load32(set, pack) loads a texture set; EftTexSet_Load8(tex, pack) loads a single texture
 *   EftTexSet_Keep32(set, 1, 0) advances every texture of a set; EftVram_AddTex(entry, 1, 0) advances one, returns TEX0
 *   Vu0Cur_Push() / Vu0Cur_Pop()   begin / end of a block of projections; Vu0Cur_LoadMtx(m) sets the matrix
 *   Vu0Cur_ProjectPoint(out, pos)  projects a point to GS screen coordinates; Vu0Cur_ProjectPoints(out, pos, n) projects n
 *                            points and returns 0 when they are rejected
 *   ClipVtx_Set(out, pos, st, col)   builds one clip-space vertex (0x30 bytes) for EftGfx_DrawPolyScaledZ
 *   EftSpr_DrawFlat(...)       queues a camera-facing sprite
 *   BtlTaskList_KillAll(list)      destroys a task list; BtlTask_SetDead(task) kills a task
 * Named by the neighbouring effect and stage files (config/symbols/eft_c.txt, eft_e.txt, eft_h.txt, eft_j.txt,
 * stg_*.txt): EftWater_GetSurfaceY, EftStage_GetTintScale (brightness 0..1 of the stage effects layer),
 * EftStage_IsDrawOn, EftGfx_DrawPolyScaledZ (clips and queues a triangle), EftMath_WrapAngle,
 * EftTechEvt_GetEvents (bits of the shot the fighter is firing), EftShot_GetCharPack / EftShot_BuildParam /
 * EftShot_CreateSlotTask / EftShot_Start (the rest of the shot module, next file), BtlStage_GetWaterLevel,
 * BtlStage_GetLightVecB, BtlStage_GetInnerRadius / GetTop / GetBottom, BtlStage_GetFxResB (number of stage
 * emitters) / GetFxResB2 (their list) / GetFxResA (number of extra emitter tasks to reserve).
 */

typedef struct EftCamView {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ Mtx44 view;      /* world to view */
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 screen;    /* world to screen */
    /* 0x180 */ u8 unk180[0xA0];
    /* 0x220 */ Vec4 pos;
    /* 0x230 */ u8 unk230[0x54];
    /* 0x284 */ s32 index;       /* 0 or 1 */
} EftCamView;

typedef struct EftCam {
    /* 0x000 */ u8 unk0[0xC40];
    /* 0xC40 */ EftCamView *cur;
} EftCam;

extern EftCamView *gBtlCamView;
extern EftCam *gBtlCam;

extern void *memset(void *dst, s32 c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);
extern f32 fabsf(f32 x);
extern s32 rand(void);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 sqrtf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern f32 Rand_FloatRange(f32 a, f32 b);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);

extern s32 BtlScene_GetStageData(void);
extern s32 *BtlScene_GetPackEntry(s32 *base, s32 idx);
extern s32 BtlScene_IsTimeStopped(void);
extern s32 BtlScene_GetCharCount(void);
extern void *BtlTask_CreateChildList(void *task, s32 count, s32 workSize);
extern void *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void *Battle_GetWork(void);
extern s32 Battle_IsSplitScreen(void);

extern void EftWater_GetSurfaceY(f32 *y);
extern void Vec4_Lerp(Vec4 *out, Vec4 *a, Vec4 *b, f32 t);
extern void Mtx_InverseRT(Mtx44 *out, Mtx44 *m);
extern void BtlStage_GetWaterLevel(f32 *y);
extern void EftTexSet_Load32(void *set, void *pack);
extern void EftTexSet_Load8(void *tex, void *pack);
extern void EftTexSet_Keep32(void *set, s32 a, s32 b);
extern u64 EftVram_AddTex(void *entry, s32 a, s32 b);
extern f32 EftStage_GetTintScale(void);
extern s32 EftStage_IsDrawOn(void);
extern void Vu0Cur_Push(void);
extern void Vu0Cur_Pop(void);
extern void Vu0Cur_LoadMtx(Mtx44 *m);
extern s32 Vu0Cur_ProjectPoint(EftScrXyz *out, Vec4 *pos); /* returns a value (see EftAura_DrawFlames, eft_n.c) */
extern s32 Vu0Cur_ProjectPoints(EftScrXyz *out, Vec4 *pos, s32 count);
extern void BtlStage_GetLightVecB(Vec4 *dir);
extern f32 EftMath_WrapAngle(f32 angle);
extern f32 BtlStage_GetInnerRadius(void);
extern f32 BtlStage_GetTop(void);
extern f32 BtlStage_GetBottom(void);
extern s32 BtlStage_GetFxResB(void);
extern s32 BtlStage_GetFxResA(void);
extern EftSmokeSrc *BtlStage_GetFxResB2(void);
extern void BtlTaskList_KillAll(void *list);
extern void BtlTask_SetDead(void *task);
extern s32 EftTechEvt_GetEvents(s32 objId);
extern void *EftShot_GetCharPack(s32 chr, s32 slot);
extern void EftShot_BuildParam(s32 chr, s32 slot, EftShotDef *def, s32 clear);
extern void EftShot_CreateSlotTask(s32 chr, s32 slot);
extern void EftShot_Start(void *req);

extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern f32 BtlCharApi_GetRadius(s32 objId);
extern s32 BtlCharApi_IsHidden(s32 objId);
extern s32 BtlCharApi_IsLockedOn(s32 objId);
extern s32 BtlCharApi_IsInTechnique(s32 objId);
extern s32 BtlCharApi_IsRushConnected(s32 objId);
extern s32 BtlCharApi_IsInSkill(s32 objId);
extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern s32 BtlCharApi_ObjTestAttr(s32 objId, u64 mask);
extern s32 BtlCharApi_ObjGetAttrValue(s32 objId, u64 mask);
extern s32 BtlCharApi_ObjGetAttrKind(s32 objId, u64 mask);
extern void BtlCharApi_SetHeldFlagA8(s32 objId);
extern void BtlCharApi_SetHeldFlagA9(s32 objId);
extern s32 BtlCharApi_PlayTechniqueSound(s32 objId, u32 n);

extern void *gEftSmokeClass[6];
extern void *gEftBoundClass[6];
extern void *gEftShotCharClass[6];
extern void *gEftTechEvtClass[6];   /* class of the first task of the shot layer (next file) */
extern f32 D_002FE9E4[];      /* -0.0f, small data reached without $gp */

typedef struct EftStageView {
    /* 0x00 */ s32 unk0[2];
    /* 0x08 */ s32 flags;      /* bit 0: the stage is not drawn */
} EftStageView;

extern EftStageView *gBtlStage;

/* Vertex handed to the shared triangle clipper. */
typedef struct EftClipVtx {
    /* 0x00 */ f32 pos[4];
    /* 0x10 */ f32 st[4];
    /* 0x20 */ f32 col[4];
} EftClipVtx; /* 0x30 */

extern void ClipVtx_Set(EftClipVtx *out, EftVecArg *pos, EftVecArg *st, EftVecArg *col);
extern void EftGfx_DrawPolyScaledZ(EftClipVtx *v, s32 otZ, s32 a3, s32 a4, s32 a5, s32 a6, u64 tex0, f32 unk);
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void EftSpr_DrawFlat(s32 r, s32 g, s32 b, s32 a, f32 u0, f32 v0, f32 u1, f32 v1, Vec4 *pos, u32 w, u32 h, s32 unk, s32 unkS0, void *tex);
#define EftSpr_DrawFlat(r, g, b, a, pos, u0, v0, u1, v1, w, h, unk, unkS0, tex) EftSpr_DrawFlat(r, g, b, a, u0, v0, u1, v1, pos, w, h, unk, unkS0, tex)
#else
extern void EftSpr_DrawFlat(s32 r, s32 g, s32 b, s32 a, Vec4 *pos, f32 u0, f32 v0, f32 u1, f32 v1, u32 w, u32 h,
                          s32 unk, s32 unkS0, void *tex);
#endif

/* GS XYZF2 register value. */
typedef struct EftGsXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftGsXyzf;

typedef struct EftGsVtx {
    /* 0x00 */ u8 rgba[4];
    /* 0x04 */ f32 q;
    /* 0x08 */ f32 s;
    /* 0x0C */ f32 t;
    /* 0x10 */ EftGsXyzf xyz;
} EftGsVtx; /* 0x18: RGBAQ, ST, XYZF2 */

/* Packet of EftUtil_DrawTri: REGLIST of PRIM, TEX0_1 and three vertices. */
typedef struct EftTriPkt {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftGsVtx v[3];
    /* 0x78 */ u64 pad;
} EftTriPkt; /* 0x80 */

/* Packet of the lightning sprites: REGLIST of PRIM, TEX0_1, RGBAQ and four (ST, XYZF2). */
typedef struct EftStripPkt {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ u8 rgba[4];
    /* 0x34 */ f32 q;
    /* 0x38 */ struct {
        f32 s;
        f32 t;
        EftGsXyzf xyz;
    } v[4];
    /* 0x78 */ u64 pad;
} EftStripPkt; /* 0x80 */

/* Packet of EftStorm_DrawRainLines: PACKED PRIM, RGBAQ and two XYZF2. */
typedef struct EftLinePkt {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u8 rgba[4];
    /* 0x2C */ f32 q;
    /* 0x30 */ EftGsXyzf xyz[2];
} EftLinePkt; /* 0x40 */

/* 1 when a GS screen position can be drawn. */
static inline s32 EftScr_IsValid(EftScrXyz *p) {
    s32 ok = 0;

    if (p->z > 0 && p->x <= 0xFFFF && p->x > 0) {
        if (p->y <= 0xFFFF) {
            if (p->y > 0) ok = 1; else ok = 0;
        }
    }
    return ok;
}

/* Links a packet into the chain of a depth slot (clamped to 0..0xFFF). */
static inline void EftOt_Add(OtPrim *p, s32 z, s32 layer) {
    OtEntry *e;

    if (layer >= 2) {
        layer -= 2;
    }
    if (z < 0) {
        e = &gOtZ[0].layer[layer];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[layer];
    } else {
        e = &gOtZ[z].layer[layer];
    }
    e->tail->next = p;
    e->tail = p;
}

static inline void *EftOt_Alloc(s32 size) {
    u32 *p = gOtCur;

    gOtCur = (u32 *)((u8 *)p + size);
    return p;
}

/* Class 0..4 of a blast record, from the definition of the shot slot it came from. */
s32 EftRec_GetDefClass(EftRecView *rec) {
    s32 cls = 0;
    EftShotDef *def;
    s32 kind;
    s32 id;

    if (rec->src == NULL) {
        return 0;
    }
    def = rec->src->def;
    kind = def->kind;
    id = def->id;
    if (kind != 3) {
        if (kind == 4) {
            cls = 2;
        }
    } else {
        cls = 3;
    }
    if (id == 0x1C8 || id == 0x159 || id == 0x270) {
        cls = 3;
    }
    if (id == 0x16F) {
        cls = 4;
    }
    return cls;
}

/* Builds a matrix at pos whose Z axis is dir pulled half way towards the camera's view axis. */
void EftUtil_MakeFacingMtx(Mtx44 *out, Vec4 *dir, Vec4 *pos) {
    Vec4 a;
    Vec4 x;
    Vec4 y;
    Vec4 z;
    Vec4 cam;
    f32 d;
    f32 s = 0.0f;

    cam.x = gBtlCamView->view.m[0][2];
    cam.y = gBtlCamView->view.m[1][2];
    cam.z = gBtlCamView->view.m[2][2];
    cam.w = 1.0f;
    d = Vec3_Dot(&cam, dir);
    if (d < s) {
        d = -d;
        a.x = -cam.x;
        a.y = -cam.y;
        a.z = -cam.z;
        a.w = 1.0f;
        s = d * 0.5f;
    } else {
        s = d * 0.5f;
        a.x = cam.x;
        a.y = cam.y;
        a.z = cam.z;
        a.w = 1.0f;
    }
    Vec4_Sub(&a, dir, &a);
    Vec3_Normalize(&a, &a);
    Vec4_Scale(&a, &a, s);
    z.x = dir->x + a.x;
    z.y = dir->y + a.y;
    z.z = dir->z + a.z;
    z.w = 1.0f;
    Vec3_Normalize(&z, &z);
    Vec3_Cross(&x, &cam, &z);
    Vec3_Normalize(&x, &x);
    Vec3_Cross(&y, &x, &z);
    Vec3_Normalize(&y, &y);
    Mtx_StoreIdentity(out);
    out->m[0][0] = x.x;
    out->m[1][0] = y.x;
    out->m[2][0] = z.x;
    out->m[0][1] = x.y;
    out->m[1][1] = y.y;
    out->m[2][1] = z.y;
    out->m[0][2] = x.z;
    out->m[1][2] = y.z;
    out->m[2][2] = z.z;
    out->m[3][0] = pos->x;
    out->m[3][1] = pos->y;
    out->m[3][2] = pos->z;
}

/* Point of the segment a-b that lies at the height of the water surface (EftWater_GetSurfaceY). */
void EftUtil_ClipSegToWater(Vec4 *out, Vec4 *a, Vec4 *b) {
    Vec4 plane;
    Vec4 dir;
    Vec4 p;
    f32 da;
    f32 dd;

    memset(&p, 0, sizeof(p));
    p.w = 1.0f;
    EftWater_GetSurfaceY(&p.y);
    plane.x = 0.0f;
    plane.y = -1.0f;
    plane.z = 0.0f;
    plane.w = -Vec3_Dot(&plane, &p);
    Vec4_Sub(&dir, a, b);
    da = Vec3_Dot(&plane, a);
    dd = Vec3_Dot(&plane, &dir);
    Vec4_Lerp(out, a, b, 1.0f - (da + plane.w) / dd);
    out->w = 1.0f;
}

/* 1 when the camera is at or below the stage's water level (+Y is down). */
s32 EftUtil_IsCamUnderWater(void) {
    Mtx44 m;
    f32 y;

    y = 0.0f;
    Mtx_InverseRT(&m, &gBtlCamView->view);
    BtlStage_GetWaterLevel(&y);
    return m.m[3][1] >= y;
}

/* 1 when a GS screen position cannot be drawn. */
s32 EftUtil_IsScreenPosClipped(EftScrXyz *p) {
    s32 r;

    if (p->z <= 0) {
        return 1;
    }
    if (p->x > 0xFFEF) {
        return 1;
    }
    if (p->x <= 0) {
        return 1;
    }
    r = p->y < 1;
    if (p->y > 0xFFEF) {
        r = 1;
    }
    return r;
}

/* Queues one textured Gouraud triangle: screen positions, colours 0..255, texture coordinates (s, t, q). */
void EftUtil_DrawTri(EftScrXyz *xyz0, EftScrXyz *xyz1, EftScrXyz *xyz2, f32 *col0, f32 *col1, f32 *col2,
                     f32 *st0, f32 *st1, f32 *st2, s32 layer, s32 z, u64 tex0) {
    u64 gif1 = 0xF42142142160;
    EftTriPkt *p = (EftTriPkt *)gOtCur;

    gOtCur = (u32 *)(p + 1);
    p->tag = 0x20000007;
    p->vif1 = 0x50000007;
    p->prim = 0x5B;
    p->vif0 = 0x10000000;
    p->gif0 = 0xC400000000008001;
    p->gif1 = gif1;
    p->next = 0;
    p->pad = 0;
    p->v[0].rgba[0] = (u32)col0[0];
    p->v[0].rgba[1] = (u32)col0[1];
    p->v[0].rgba[2] = (u32)col0[2];
    p->v[0].rgba[3] = (u32)col0[3];
    p->v[0].q = st0[2];
    p->v[1].rgba[0] = (u32)col1[0];
    p->v[1].rgba[1] = (u32)col1[1];
    p->v[1].rgba[2] = (u32)col1[2];
    p->v[1].rgba[3] = (u32)col1[3];
    p->v[1].q = st1[2];
    p->v[2].rgba[0] = (u32)col2[0];
    p->v[2].rgba[1] = (u32)col2[1];
    p->v[2].rgba[2] = (u32)col2[2];
    p->v[2].rgba[3] = (u32)col2[3];
    p->v[2].q = st2[2];
    p->tex0 = tex0;
    p->v[0].s = st0[0];
    p->v[0].t = st0[1];
    p->v[1].s = st1[0];
    p->v[1].t = st1[1];
    p->v[2].s = st2[0];
    p->v[2].t = st2[1];
    p->v[0].xyz.x = xyz0->x;
    p->v[0].xyz.y = xyz0->y;
    p->v[0].xyz.z = xyz0->z;
    p->v[0].xyz.f = 0xFF;
    p->v[1].xyz.x = xyz1->x;
    p->v[1].xyz.y = xyz1->y;
    p->v[1].xyz.z = xyz1->z;
    p->v[1].xyz.f = 0xFF;
    p->v[2].xyz.x = xyz2->x;
    p->v[2].xyz.y = xyz2->y;
    p->v[2].xyz.z = xyz2->z;
    p->v[2].xyz.f = 0xFF;
    EftOt_Add((OtPrim *)p, z, layer);
}

/* Storm init callback: allocates the work block, loads the textures (stage pack entry 14), spawns everything. */
void EftStorm_Init(EftTask *task) {
    s32 pack = BtlScene_GetStageData();

    if (gEftStorm != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftStorm);
        gEftStorm = NULL;
    }
    gEftStorm = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftStorm));
    memset(gEftStorm, 0, sizeof(EftStorm));
    EftTexSet_Load32(gEftStorm, BtlScene_GetPackEntry((s32 *)pack, 14));
    EftStorm_Reset(task);
    EftStorm_InitRain();
}

/* Storm term callback. */
void EftStorm_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftStorm);
}

/* Storm update callback: steps the textures, the four lightning bolts and the rain. */
void EftStorm_Update(void) {
    EftStormBolt *bolt = gEftStorm->bolt;
    s32 i;

    EftTexSet_Keep32(gEftStorm, 1, 0);
    if (*(u64 *)((u8 *)Battle_GetWork() + 0x19F0) & 0x100) {
        return;
    }
    for (i = 0; i < EFT_STORM_BOLTS; i++, bolt++) {
        if (bolt->wait != 0) {
            bolt->wait--;
        } else if (bolt->life != 0) {
            bolt->life--;
        } else {
            EftStorm_SpawnBolt(bolt);
        }
    }
    EftStorm_MoveRain();
}

/* Draws the live lightning bolts as camera-facing sprites. Four corner offsets ((-4, -1.5), (4, 8), (-8, -3),
   (8, -0) times 150) are turned by the camera's world matrix and added to the bolt position; a bolt is skipped
   when any of the four projects off screen. Kind 0 draws two sprites (textures texA and texB), kind 1 only the
   second, larger one. */
#if 0 /* not matched: 636 instructions against 636, 116 differ when aligned. Found in the second retry: the body of
the second sprite exists twice (`if (kind != 0) { B } else { A; B }`); the two tag words are scalars that end up in
spill slots together with work and i (sp+256 work, 260 i, 264 / 268 the tag words: declaration order work, i, tag,
vif); the texture word is read where it is stored, after the texture coordinates, so the sprite code is a macro
(or was written out), not a function taking the value; the packet pointer is one function-level variable filled
through an inline allocator (that gives the `move v1,a0` copy of the first two expansions and none in the third).
What still differs is register choice that follows from reload: the original reloads spilled pseudos into a2 where
this C uses a1 (first Vec4_Scale: `addiu a2,sp,64 / move a0,a2 / move a1,a2`; the loop counter `lw a2,260(sp)`; the
header constants alternate a2 / v0), the on-screen test keeps 0xFFFF in a0 and the result in a1 (here a1 / a0), and
the original loads 0x80000000 (t0) before Vu0Cur_LoadMtx and saves it with sq / lq around that call, while
`tag |= 7`, `vif |= 7` and `i = 3` stay in source order (here the scheduler moves them up across the calls).
Putting the four locals in a struct (memory) keeps them in place but then they are no longer reloaded into a2.
Cleanup W1: Vu0Cur_ProjectPoint is now declared with its return value (that alone fixed the same "x / limit /
reload register" permutation in EftAura_DrawFlames, eft_n.c); here it changes nothing (121 instructions out of
place with registers, 87 register-masked). Not tried yet with what EftTrail_Draw (eft_q.c) and EftBound_BuildWall
taught: constants as variables assigned in front of each use (not loop invariants), int constants that are
converted to float through a spilled variable, and pointer variables assigned twice (unknown alias base). */
/* Fills and queues one lightning sprite: a triangle strip between two projected corners, white scaled by the
   layer brightness, alpha 128 * life / lifeMax, in the second chain of the depth slot of its z. */
#define EFT_STORM_SPRITE(bolt, a, b, texv, col, tagv, vifv) \
    do { \
        p = EftOt_Alloc(sizeof(EftStripPkt)); \
        p->gif1 = 0xF42424242160; \
        p->vif0 = 0x10000000; \
        p->tag = (tagv); \
        p->next = 0; \
        p->gif0 = 0xC400000000008001; \
        p->vif1 = (vifv); \
        p->prim = 0x54; \
        p->rgba[0] = (u32)(col); \
        p->rgba[1] = (u32)(col); \
        p->rgba[2] = (u32)(col); \
        p->rgba[3] = (bolt->life << 7) / bolt->lifeMax; \
        p->q = 1.0f; \
        p->v[0].xyz.x =  (a)->x; \
        p->v[0].xyz.y =  (a)->y; \
        p->v[0].xyz.z =  (a)->z; \
        p->v[0].xyz.f = 0xFF; \
        p->v[1].xyz.x = (b)->x; \
        p->v[1].xyz.y =  (a)->y; \
        p->v[1].xyz.z =  (a)->z; \
        p->v[1].xyz.f = 0xFF; \
        p->v[2].xyz.x =  (a)->x; \
        p->v[2].xyz.y = (b)->y; \
        p->v[2].xyz.z =  (a)->z; \
        p->v[2].xyz.f = 0xFF; \
        p->v[3].xyz.x = (b)->x; \
        p->v[3].xyz.y = (b)->y; \
        p->v[3].xyz.z =  (a)->z; \
        p->v[3].xyz.f = 0xFF; \
        p->v[0].s = 0.0f; \
        p->v[0].t = 0.0f; \
        p->v[1].s = 1.0f; \
        p->v[1].t = 0.0f; \
        p->v[2].s = 0.0f; \
        p->v[2].t = 1.0f; \
        p->v[3].s = 1.0f; \
        p->v[3].t = 1.0f; \
        p->tex0 = (texv); \
        EftOt_Add((OtPrim *)p,  (a)->z >> 8, 1); \
    } while (0)

void EftStorm_DrawBolts(EftStormBolt *bolt) {
    Mtx44 cam;
    Vec4 corner[4];
    Vec4 world[4];
    EftScrXyz xyz[4];
    EftStorm *work = gEftStorm;
    s32 i;
    u32 tag;
    u32 vif;
    EftStripPkt *p;
    f32 col;
    s32 j;

    Vec4_Set(&corner[0], -4.0f, -1.5f, 0.0f, 0.0f);
    Vec4_Set(&corner[1], 4.0f, 8.0f, 0.0f, 0.0f);
    Vec4_Set(&corner[2], -8.0f, -3.0f, 0.0f, 0.0f);
    Vec4_Set(&corner[3], 8.0f, D_002FE9E4[0], 0.0f, 0.0f);
    col = EftStage_GetTintScale() * 128.0f;
    Vec4_Scale(&corner[0], &corner[0], 150.0f);
    Vec4_Scale(&corner[1], &corner[1], 150.0f);
    Vec4_Scale(&corner[2], &corner[2], 150.0f);
    Vec4_Scale(&corner[3], &corner[3], 150.0f);
    Mtx_StoreIdentity(&cam);
    if (gBtlCamView != NULL) {
        Mtx_InverseRT(&cam, &gBtlCamView->view);
    }
    tag = 0x20000000;
    Mtx_MulVec4(&corner[0], &cam, &corner[0]);
    vif = 0x50000000;
    Mtx_MulVec4(&corner[1], &cam, &corner[1]);
    Mtx_MulVec4(&corner[2], &cam, &corner[2]);
    Mtx_MulVec4(&corner[3], &cam, &corner[3]);
    Vu0Cur_Push();
    tag |= 7;
    vif |= 7;
    Vu0Cur_LoadMtx(&gBtlCamView->screen);
    for (i = 3; i >= 0; i--, bolt++) {
        if (bolt->life == 0 || bolt->wait != 0) {
            continue;
        }
        for (j = 0; j < 4; j++) {
            bolt->pos.w = 1.0f;
            Vec4_Add(&world[j], &corner[j], &bolt->pos);
            Vu0Cur_ProjectPoint(&xyz[j], &world[j]);
            if (!EftScr_IsValid(&xyz[j])) {
                break;
            }
        }
        if (j != 4) {
            continue;
        }
        if (bolt->kind != 0) {
            EFT_STORM_SPRITE(bolt, &xyz[2], &xyz[3], work->tex.entry[bolt->texB].tex0, col, tag, vif);
        } else {
            EFT_STORM_SPRITE(bolt, &xyz[0], &xyz[1], work->tex.entry[bolt->texA].tex0, col, tag, vif);
            EFT_STORM_SPRITE(bolt, &xyz[2], &xyz[3], work->tex.entry[bolt->texB].tex0, col, tag, vif);
        }
    }
    Vu0Cur_Pop();
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/eft_g", EftStorm_DrawBolts);
#endif

/* Storm draw callback. */
void EftStorm_Draw(void) {
    EftStorm_DrawBolts(gEftStorm->bolt);
    EftStorm_DrawRain();
}

/* Storm post-update callback: nothing. */
void EftStorm_PostUpdate(void) {
}

/* Storm reset callback: respawns the four bolts. */
void EftStorm_Reset(EftTask *task) {
    EftStormBolt *bolt = gEftStorm->bolt;
    s32 i;

    for (i = 0; i < EFT_STORM_BOLTS; i++, bolt++) {
        EftStorm_SpawnBolt(bolt);
    }
}

/* Places a bolt on the horizon (about 5000 away, 700 up) with a life of 16..31 frames; a bolt closer than 700
   to a live one is dropped. Seven or eight rand() calls. */
void EftStorm_SpawnBolt(EftStormBolt *bolt) {
    Vec4 d;
    f32 dist;
    EftStormBolt *other;
    s32 i;
    s32 r;
    s32 r2;

    dist = (f32)((rand() & 0x3FF) - 0x1FF) + 5000.0f;
    bolt->pos.x = (rand() & 0x1FF) - 0xFF;
    bolt->pos.y = 0.0f;
    bolt->pos.z = (rand() & 0xFF) - 0x1F;
    bolt->pos.w = 1.0f;
    if (bolt->pos.x == 0.0f && bolt->pos.z == 0.0f) {
        bolt->pos.x = 1.0f;
    }
    Vec3_Normalize(&bolt->pos, &bolt->pos);
    Vec4_Scale(&bolt->pos, &bolt->pos, dist);
    bolt->lifeMax = bolt->life = (u32)((rand() & 0x1F) + 0x20) >> 1;
    bolt->texA = (u32)rand() % 3;
    r = rand();
    r2 = rand() & 1;
    bolt->pos.y = -700.0f;
    bolt->texB = (r & 1) + r2 + 3;
    rand();
    bolt->wait = 1;
    if (dist < 5000.0f) {
        r2 = 1;
        bolt->kind = r2;
    } else {
        r2 = rand() & 1;
        bolt->kind = r2;
    }
    for (other = gEftStorm->bolt, i = 0; i < EFT_STORM_BOLTS; other++) {
        i++;
        if (other == bolt) {
            continue;
        }
        if (other->life == 0) {
            continue;
        }
        Vec4_Sub(&d, &other->pos, &bolt->pos);
        if (Vec3_Dot(&d, &d) < 490000.0f) {
            bolt->life = 0;
            break;
        }
    }
}

/* Scatters the 64 rain drops in a 1023-wide cube around the origin, all falling 19.6 per frame. */
void EftStorm_InitRain(void) {
    EftStormDrop *drop = gEftStorm->drop;
    s32 i;

    for (i = 0; i < EFT_STORM_DROPS; i++, drop++) {
        drop->pos.x = (f32)((rand() & 0x3FF) - 0x1FF) + 0.0f;
        drop->pos.y = (f32)((rand() & 0x3FF) - 0x1FF) + 0.0f;
        drop->pos.z = (f32)((rand() & 0x3FF) - 0x1FF) + 0.0f;
        drop->pos.w = 1.0f;
        drop->vel.x = 0.0f;
        drop->vel.y = 19.6f;
        drop->vel.z = 0.0f;
        drop->vel.w = 0.0f;
    }
}

/* Moves every rain drop by its velocity. */
void EftStorm_MoveRain(void) {
    EftStormDrop *drop = gEftStorm->drop;
    s32 i;

    for (i = 0; i < EFT_STORM_DROPS; i++, drop++) {
        Vec4_Add(&drop->pos, &drop->pos, &drop->vel);
    }
}

/* Queues one line per drop, from the drop to drop + streak, in the first depth slot. */
void EftStorm_DrawRainLines(EftStormDrop *drop, s32 count, Vec4 *streak) {
    EftScrXyz xyz[2];
    Vec4 pos[2];
    EftLinePkt *p;
    s32 i;

    for (i = 0; i < count; i++, drop++) {
        Vec4_Copy(&pos[0], &drop->pos);
        Vec4_Add(&pos[1], &drop->pos, streak);
        if (Vu0Cur_ProjectPoints(xyz, pos, 2) != 0 && EftScr_IsValid(&xyz[0]) && EftScr_IsValid(&xyz[1])) {
        { u32 *cur = gOtCur; p = (EftLinePkt *)cur; cur += 16; gOtCur = cur; }
        p->prim = 0x41;
        p->tag = 0x20000003;
        p->vif0 = 0x10000000;
        p->vif1 = 0x50000003;
        p->gif0 = 0x4400000000008001;
        p->gif1 = 0x4410;
        p->rgba[3] = 0x20;
        p->q = 1.0f;
        p->next = 0;
        p->rgba[0] = 0x60;
        p->rgba[1] = 0x60;
        p->rgba[2] = 0x60;
        p->xyz[0].x = xyz[0].x;
        p->xyz[0].y = xyz[0].y;
        p->xyz[0].z = xyz[0].z;
        p->xyz[0].f = 0xFF;
        p->xyz[1].x = xyz[1].x;
        p->xyz[1].y = xyz[1].y;
        p->xyz[1].z = xyz[1].z;
        p->xyz[1].f = 0xFF;
        EftOt_Add((OtPrim *)p, 0, 0);
        }
    }
}

/* Keeps the drops inside a 200-wide cube around center: a drop leaving through one face re-enters through the
   opposite one; one that leaves through the top or the bottom also gets a new random x and z (two rand()). */
void EftStorm_WrapRain(Vec4 *center, EftStormDrop *drop, s32 count) {
    s32 i;

    for (i = 0; i < count; i++, drop++) {
        if (center->x + 100.0f < drop->pos.x) {
            drop->pos.x -= 200.0f;
        }
        if (drop->pos.x < center->x - 100.0f) {
            drop->pos.x += 200.0f;
        }
        if (center->y + 100.0f < drop->pos.y) {
            drop->pos.x = center->x + (f32)((rand() & 0x3FF) - 0x1FF) * 0.25f;
            drop->pos.y -= 200.0f;
            drop->pos.z = center->z + (f32)((rand() & 0x3FF) - 0x1FF) * 0.25f;
        }
        if (drop->pos.y < center->y - 100.0f) {
            drop->pos.x = center->x + (f32)((rand() & 0x3FF) - 0x1FF) * 0.25f;
            drop->pos.y += 200.0f;
            drop->pos.z = center->z + (f32)((rand() & 0x3FF) - 0x1FF) * 0.25f;
        }
        if (center->z + 100.0f < drop->pos.z) {
            drop->pos.z -= 200.0f;
        }
        if (drop->pos.z < center->z - 100.0f) {
            drop->pos.z += 200.0f;
        }
    }
}

/* Draws the rain of the current view: the drops are wrapped around a point 150 in front of the camera and drawn
   as streaks that lean against the camera's own movement. In split screen each view gets half of the drops. */
void EftStorm_DrawRain(void) {
    Vec4 center;
    Vec4 streak;
    Vec4 delta;
    Mtx44 cam;
    s32 view = gBtlCam->cur->index;

    Vec4_Set(&center, 0.0f, 0.0f, 150.0f, 1.0f);
    Vec4_Set(&streak, 0.0f, 100.0f, 0.0f, 0.0f);
    Mtx_InverseRT(&cam, &gBtlCamView->view);
    Mtx_MulVec4(&center, &cam, &center);
    Vec4_Sub(&delta, &gEftStorm->prevCam[view], (Vec4 *)cam.m[3]);
    Vec4_Add(&streak, &streak, &delta);
    Vec3_Normalize(&streak, &streak);
    Vec4_Scale(&streak, &streak, 20.0f);
    Vec4_Copy(&gEftStorm->prevCam[view], (Vec4 *)cam.m[3]);
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->screen);
    if (!Battle_IsSplitScreen()) {
        EftStorm_WrapRain(&center, gEftStorm->drop, EFT_STORM_DROPS);
        EftStorm_DrawRainLines(gEftStorm->drop, EFT_STORM_DROPS, &streak);
    } else if (view != 0) {
        EftStorm_WrapRain(&center, gEftStorm->drop, EFT_STORM_DROPS / 2);
        EftStorm_DrawRainLines(gEftStorm->drop, EFT_STORM_DROPS / 2, &streak);
    } else {
        EftStorm_WrapRain(&center, &gEftStorm->drop[EFT_STORM_DROPS / 2], EFT_STORM_DROPS / 2);
        EftStorm_DrawRainLines(&gEftStorm->drop[EFT_STORM_DROPS / 2], EFT_STORM_DROPS / 2, &streak);
    }
    Vu0Cur_Pop();
}

/* Creates a smoke emitter task. Returns the task, NULL when the manager does not exist. */
void *EftSmoke_Create(EftSmokeArg *arg) {
    if (gEftSmokeMgr == NULL) {
        return NULL;
    }
    return BtlTaskList_AddTail(gEftSmokeMgr->list, gEftSmokeClass, arg);
}

/* Tells a live emitter to stop: it kills itself when its last particle has gone. */
void EftSmoke_Stop(EftTask *task) {
    if (task != NULL && task->cls[0] == (void *)EftSmoke_Update && !(task->flags & 1)) {
        ((EftSmoke *)task->work)->flags |= 1;
    }
}

/* Position of a live emitter, NULL when the task is not one. */
Vec4 *EftSmoke_GetPos(EftTask *task) {
    if (task == NULL) {
        return NULL;
    }
    if (task->cls[0] != (void *)EftSmoke_Update) {
        return NULL;
    }
    if (task->flags & 1) {
        return NULL;
    }
    return &((EftSmoke *)task->work)->arg.pos;
}

/* Moves a live emitter. */
void EftSmoke_SetPos(EftTask *task, Vec4 *pos) {
    if (task != NULL && task->cls[0] == (void *)EftSmoke_Update && !(task->flags & 1)) {
        Vec4_Copy(&((EftSmoke *)task->work)->arg.pos, pos);
    }
}

/* Smoke manager init callback: loads the texture (stage pack entry 18) and creates one emitter per record of
   the stage's emitter list. */
void EftSmokeMgr_Init(EftTask *task) {
    EftSmokeArg arg;
    s32 pack = BtlScene_GetStageData();
    s32 count = BtlStage_GetFxResB();
    EftSmokeSrc *src;
    EftSmokeSrc *list;
    s32 i;

    gEftSmokeMgr = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftSmokeMgr));
    memset(gEftSmokeMgr, 0, sizeof(EftSmokeMgr));
    EftTexSet_Load8(gEftSmokeMgr->tex, BtlScene_GetPackEntry((s32 *)pack, 18));
    gEftSmokeMgr->list = BtlTask_CreateChildList(task, count + BtlStage_GetFxResA(), sizeof(EftSmoke));
    list = BtlStage_GetFxResB2();
    if (list == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        src = &list[i];
        memset(&arg, 0, sizeof(arg));
        arg.pos.x = src->pos[0];
        arg.pos.y = src->pos[1];
        arg.pos.z = src->pos[2];
        arg.pos.w = 1.0f;
        arg.ambient.x = src->ambient[0];
        arg.ambient.y = src->ambient[1];
        arg.ambient.z = src->ambient[2];
        arg.ambient.w = src->ambient[3];
        arg.diffuse.x = src->diffuse[0];
        arg.diffuse.y = src->diffuse[1];
        arg.diffuse.z = src->diffuse[2];
        arg.diffuse.w = src->diffuse[3];
        arg.alpha = src->ambient[3];
        arg.size = src->size;
        arg.damp = src->damp;
        arg.lifeBase = src->lifeBase;
        arg.lifeRange = 30;
        arg.rate = src->rate;
        arg.speed = src->speed;
        EftSmoke_Create(&arg);
    }
}

/* Smoke manager term callback. */
void EftSmokeMgr_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftSmokeMgr);
    gEftSmokeMgr = NULL;
}

/* Smoke manager update callback: advances the texture while there is an emitter. */
void EftSmokeMgr_Update(void) {
    if (((void **)gEftSmokeMgr->list)[1] != NULL) {
        gEftSmokeMgr->tex0 = EftVram_AddTex(&gEftSmokeMgr->tex0, 1, 0);
    }
}

/* Emitter init callback: copies the argument and runs 60..89 updates so that the smoke is already there. */
void EftSmoke_Init(EftTask *task, EftSmokeArg *arg) {
    EftSmoke *work = task->work;
    s32 n;
    s32 i;

    memset(work, 0, sizeof(EftSmoke));
    work->arg = *arg;
    work->tex = gEftSmokeMgr->tex;
    n = rand() % 30 + 60;
    for (i = 0; i < n; i++) {
        EftSmoke_Update(task);
    }
}

/* Emitter term callback: nothing. */
void EftSmoke_Term(void) {
}

/* Emitter reset callback: nothing. */
void EftSmoke_Reset(void) {
}

/* Emitter update callback. Not while time is stopped. With probability 1 / rate starts one free particle at
   the emitter, moving up inside a cone; then moves every live particle and damps its horizontal speed. A
   stopped emitter kills its task once no particle is left. Random: rand() once or twice, Rand_FloatRange twice. */
void EftSmoke_Update(EftTask *task) {
    EftSmoke *work = task->work;
    EftSmokePart *part;
    s32 i;
    s32 n;
    f32 x;
    f32 z;
    u32 flags;

    if (BtlScene_IsTimeStopped()) {
        return;
    }
    flags = work->flags;
    if (!(flags & 1)) {
        if (rand() % work->arg.rate == 0) {
            part = work->part;
            for (i = 0; i < EFT_SMOKE_PARTS; i++, part++) {
                if (part->life == 0) {
                    Vec4_Copy(&part->pos, &work->arg.pos);
                    x = Rand_FloatRange(-50.0f, 50.0f);
                    z = Rand_FloatRange(-50.0f, 50.0f);
                    Vec4_Set(&part->vel, x, -100.0f, z, 1.0f);
                    Vec3_Normalize(&part->vel, &part->vel);
                    Vec4_Scale(&part->vel, &part->vel, work->arg.speed);
                    part->life = work->arg.lifeBase + rand() % work->arg.lifeRange;
                    break;
                }
            }
        }
    }
    part = work->part;
    if (work->flags & 1) {
        n = 0;
        for (i = 0; i < EFT_SMOKE_PARTS; i++, part++) {
            if (part->life != 0) {
                n++;
            }
        }
        if (n == 0) {
            BtlTask_SetDead(task);
            return;
        }
    }
    part = work->part;
    for (i = 0; i < EFT_SMOKE_PARTS; i++, part++) {
        if (part->life != 0) {
            Vec3_Add(&part->pos, &part->pos, &part->vel);
            part->vel.x *= work->arg.damp;
            part->vel.z *= work->arg.damp;
            part->life--;
        }
    }
}

/* Emitter draw callback: one camera-facing sprite per live particle, lit by the stage light direction (ambient
   + diffuse * max(0, n.l), n pointing from 50 above the emitter to the particle), fading over its last 50 frames. */
/* Matched (cleanup W1). Three things did it:
   - EftSpr_DrawFlat's parameter order: the position comes BEFORE the four texture coordinates (r, g, b, a, pos, u0,
     v0, u1, v1, w, h, ...); float and integer arguments use separate registers, so only the order in which the
     argument registers are loaded shows it (`move t0,s2` in front of the four `mov.s`).
   - the 50 of the fade is an int variable that gets no register: reload replaces it by its constant, and the
     int-to-float conversion then reads it from a constant-pool word in .sdata (0x2FE9EC, the former
     "gEftSmokeFadeFrames").
   - STAND-IN (possibly a fake match): the copies of 1.0 / 0.0 that the loop uses (f23 / f24, made from f21 / f20 in
     front of the loop) survive only when gcse cannot see `hi = one` / `lo = zero` as available inside the loop, i.e.
     when `one` and `zero` are assigned again in the loop. The two dead assignments below do that (flow deletes
     them); what the original had in their place is unknown. */
void EftSmoke_Draw(EftTask *task) {
    Vec4 base;
    Vec4 light;
    Vec4 col;
    Vec4 diffuse;
    Vec4 ambient;
    Vec4 d;
    void *tex;
    EftSmoke *work = task->work;
    EftSmokePart *part;
    f32 bright;
    f32 alpha;
    f32 one;
    f32 zero;
    f32 hi;
    f32 lo;
    s32 life;
    s32 fade = 50;
    s32 i;

    tex = (u8 *)work->tex + 0x20;
    if (gBtlStage->flags & 1) {
        return;
    }
    if (!EftStage_IsDrawOn()) {
        return;
    }
    part = work->part;
    bright = EftStage_GetTintScale();
    one = 1.0f;
    zero = 0.0f;
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->screen);
    BtlStage_GetLightVecB(&light);
    Vec3_Normalize(&light, &light);
    Vec4_Set(&base, work->arg.pos.x, work->arg.pos.y - 50.0f, work->arg.pos.z, one);
    Vec4_Set(&ambient, work->arg.ambient.x, work->arg.ambient.y, work->arg.ambient.z, zero);
    Vec4_Set(&diffuse, work->arg.diffuse.x, work->arg.diffuse.y, work->arg.diffuse.z, zero);
    hi = one;
    lo = zero;
    for (i = 0; i < EFT_SMOKE_PARTS; i++, part++) {
        if (part->life != 0) {
            life = part->life;
            alpha = work->arg.alpha;
            if (life < fade) {
                alpha = alpha * (f32)life / (f32)fade;
            }
            Vec3_Sub(&d, &part->pos, &base);
            d.y *= 0.8f;
            Vec3_Normalize(&d, &d);
            Vec3_Scale(&col, &diffuse, Vec3_Dot(&d, &light));
            if (col.x < 0.0f) {
                col.x = 0.0f;
            }
            if (col.y < 0.0f) {
                col.y = 0.0f;
            }
            if (col.z < 0.0f) {
                col.z = 0.0f;
            }
            Vec3_Add(&col, &col, &ambient);
            if (col.x > 255.0f) {
                col.x = 255.0f;
            }
            if (col.y > 255.0f) {
                col.y = 255.0f;
            }
            if (col.z > 255.0f) {
                col.z = 255.0f;
            }
            part->pos.w = hi;
            one = alpha; /* stand-in, see above */
            zero = alpha;
            EftSpr_DrawFlat(col.x * bright, col.y * bright, col.z * bright, alpha, &part->pos, lo, lo, hi, hi,
                          work->arg.size * 128.0f, work->arg.size * 128.0f, 0, 0, tex);
        }
    }
    Vu0Cur_Pop();
}

static inline void EftVec3_SubInl(Vec4 *out, Vec4 *a, Vec4 *b) {
    out->x = a->x - b->x;
    out->y = a->y - b->y;
    out->z = a->z - b->z;
}

/* Reads the fighter's position (node 3) and its bearing from the stage axis. */
void EftBound_UpdateAngle(EftBoundChar *work, EftBoundChar *src) {
    Vec4 dir;

    BtlCharApi_GetNodePos(src->objId, 3, &work->pos);
    Vec3_Normalize(&dir, &work->pos);
    work->angle = atan2f(dir.x, dir.z);
}

/* Rebuilds the wall mesh of a fighter: a 6 x 4 grid on the cylinder of the stage radius, centred on the
   fighter's bearing. Its opacity grows as the fighter gets within param->range of the cylinder and falls off
   with the distance of each vertex from the point of the cylinder nearest to the fighter. The mesh is off
   while the fighter is in a technique or has a rush connected. Texture scroll and the pulse phase advance
   only while time is not stopped. */
/* Matched in cleanup W1 (it was "saved registers numbered differently", 53 instructions out of place). Causes:
   - the vertex pointer of the build loop and the `vtx` of the normal loop are ONE variable: two assignments give
     the pseudo an unknown alias base, so the stores to the local `d` force the reloads of vtx->pos;
   - the texture coordinates are computed into two temporaries before the st stores; param is declared before color;
   - the quad loop has a walking variable of its own (`m2`: sharing `m` with the normal loop makes its induction
     variable "not replaceable" and the loop pass will not reduce it);
   - the four vertex numbers are stored through the int pointer that the inner loop also walks (`pi`, assigned
     twice: unknown alias base again). The one store whose address stays a register (`pi[0]`) then invalidates
     everything cse knows about memory, which is what makes the original convert mesh->cols a second time and
     re-read mesh->quadCount (`lh`) for the loop's entry test, while gcse's PRE and reload's cse still pass the
     count to the entry test of the NORMAL loop as `move v1,a0`;
   STAND-INS (the natural source form is not known, marked in the code):
   - `pi` starts at element 1 (`&loc.idx.i[1]`, stores at -1, 0, 1, 2), and `d` / `idx` are one local object, so
     that no register holds the address of element 0 in front of the quad loop (with `pi = idx.i` the first store
     keeps `addiu a2,sp,16` alive and the loop's own `addiu t1,sp,16` disappears);
   - the quad pointer `q` has a dead `= NULL` initialiser: it must not be "replaceable" for the loop pass, which
     keeps `addiu v0,a2,0x640` in the loop instead of a second walking pointer. */
void EftBound_BuildWall(EftBoundChar *work, EftBoundChar *src) {
    struct {
        Vec4 d;
        EftBoundIdx idx;
    } loc; /* one object: see the note above */
    Vec4 e1;
    Vec4 e2;
    EftBoundIdx tri;
    EftBoundParam *param = gEftBound->param;
    f32 *color = gEftBound->color;
    EftBoundMesh *mesh = &work->wall;
    EftBoundVtx *vtx;
    f32 alpha;
    EftBoundMesh *m;
    EftBoundMesh *m2;
    EftBoundQuad *q = NULL;
    f32 one = 1.0f;
    f32 fade;
    f32 y0;
    f32 pulse;
    f32 a;
    f32 t;
    f32 u;
    f32 k;
    f32 len;
    s32 i;
    s32 j;
    s32 n;
    s32 col;
    s32 *pi;

    mesh->center.x = sinf(work->angle) * work->radius;
    mesh->center.y = work->pos.y;
    mesh->center.z = cosf(work->angle) * work->radius;
    mesh->center.w = one;
    Vec3_Sub(&loc.d, &work->wall.center, &work->pos);
    mesh->dist = sqrtf(loc.d.x * loc.d.x + loc.d.z * loc.d.z);
    mesh->dist -= work->charRadius;
    if (mesh->dist < 0.0f) {
        mesh->dist = 0.0f;
    }
    fade = (param->range - mesh->dist) / param->range;
    if (fade < 0.0f) {
        fade = 0.0f;
    }
    if (!BtlScene_IsTimeStopped()) {
        mesh->scrollS += param->scrollS;
        if (mesh->scrollS > one) {
            mesh->scrollS -= one;
        }
        mesh->scrollT += param->scrollT;
        if (mesh->scrollT > one) {
            mesh->scrollT -= one;
        }
        mesh->phase += param->phaseRate * 6.2831853f;
        mesh->phase = EftMath_WrapAngle(mesh->phase);
    }
    if (BtlCharApi_IsInTechnique(src->objId) || BtlCharApi_IsRushConnected(src->objId)) {
        fade = 0.0f;
    }
    if (!(fade > 0.0f)) {
        mesh->active = 0;
        return;
    }
    mesh->active = 1;
    mesh->arc = param->arc;
    mesh->cols = 6.0f;
    mesh->rows = 4.0f;
    y0 = work->pos.y - param->height * 0.5f;
    pulse = fabsf(cosf(mesh->phase));
    for (j = 0; j < (s32)mesh->rows; j++) {
        for (i = 0; i < (s32)mesh->cols; i++) {
            vtx = &mesh->vtx[j * (s32)mesh->cols + i];
            a = work->angle - mesh->arc * 0.5f + mesh->arc * ((f32)i / (mesh->cols - 1.0f));
            vtx->pos.v[0] = sinf(a) * work->radius;
            vtx->pos.v[2] = cosf(a) * work->radius;
            vtx->pos.v[1] = y0 + (f32)j / (mesh->rows - 1.0f) * param->height;
            vtx->pos.v[3] = 1.0f;
            u = mesh->scrollS + a / param->texArc;
            t = mesh->scrollT + vtx->pos.v[1] / param->texHeight;
            vtx->st.v[0] = u;
            vtx->st.v[1] = t;
            vtx->st.v[2] = 1.0f;
            vtx->st.v[3] = 0.0f;
            loc.d.x = vtx->pos.v[0] - mesh->center.x;
            loc.d.y = vtx->pos.v[1] - mesh->center.y;
            loc.d.z = vtx->pos.v[2] - mesh->center.z;
            len = sqrtf(loc.d.x * loc.d.x + loc.d.y * loc.d.y + loc.d.z * loc.d.z);
            k = (param->fadeDist - len) / param->fadeDist;
            if (k < 0.0f) {
                k = 0.0f;
            } else if (k > 1.0f) {
                k = 1.0f;
            }
            vtx->col.v[0] = color[0];
            vtx->col.v[1] = color[1];
            vtx->col.v[2] = color[2];
            alpha = param->alpha * fade * k;
            alpha = alpha + alpha * pulse;
            vtx->col.v[3] = alpha;
            if (alpha > 255.0f) {
                vtx->col.v[3] = 255.0f;
            }
        }
    }
    mesh->quadCount = ((s32)mesh->cols - 1) * ((s32)mesh->rows - 1);
    pi = &loc.idx.i[1];
    pi[-1] = 0;
    pi[0] = 1;
    pi[1] = (s32)mesh->cols;
    pi[2] = (s32)mesh->cols + 1;
    col = 0;
    for (n = 0; n < mesh->quadCount; n++) {
        m2 = (EftBoundMesh *)((u8 *)mesh + n * sizeof(EftBoundQuad));
        q = &m2->quad[0];
        q->idx = loc.idx;
        col = (col + 1) % ((s32)mesh->cols - 1);
        pi = loc.idx.i;
        for (j = 0; j < 4; j++, pi++) {
            if (col == 0) {
                *pi += 2;
            } else {
                *pi += 1;
            }
        }
    }
    vtx = mesh->vtx;
    for (i = 0; i < mesh->quadCount; i++) {
        m = (EftBoundMesh *)((u8 *)mesh + i * sizeof(EftBoundQuad));
        memcpy(&tri, &m->quad[0].idx, 12);
        Vec3_Sub(&e1, (Vec4 *)&vtx[tri.i[1]].pos, (Vec4 *)&vtx[tri.i[0]].pos);
        Vec3_Sub(&e2, (Vec4 *)&vtx[tri.i[2]].pos, (Vec4 *)&vtx[tri.i[1]].pos);
        Vec3_Cross(&m->quad[0].normal, &e2, &e1);
        Vec3_Normalize(&m->quad[0].normal, &m->quad[0].normal);
    }
}

/* Queues one triangle of the wall: builds three clip vertices, takes the whole texture repeats off the
   coordinates and hands them to the shared clipper (EftGfx_DrawPolyScaledZ). The matrix argument is not used. */
void EftBound_DrawTri(EftMtxArg m, EftVecArg p0, EftVecArg p1, EftVecArg p2, EftVecArg st0, EftVecArg st1,
                      EftVecArg st2, EftVecArg c0, EftVecArg c1, EftVecArg c2, s32 otZ, u64 tex0) {
    EftClipVtx v[9];
    f32 fs;
    f32 ft;

    memset(v, 0, sizeof(v));
    ClipVtx_Set(&v[0], &p0, &st0, &c0);
    ClipVtx_Set(&v[1], &p1, &st1, &c1);
    ClipVtx_Set(&v[2], &p2, &st2, &c2);
    fs = (s32)st0.v[0];
    ft = (s32)st0.v[1];
    v[0].st[0] -= fs;
    v[1].st[0] -= fs;
    v[2].st[0] -= fs;
    v[0].st[1] -= ft;
    v[1].st[1] -= ft;
    v[2].st[1] -= ft;
    EftGfx_DrawPolyScaledZ(v, otZ, 1, 0, 0, 0, tex0, 2.0f);
}

#ifdef PORT
extern void Port_WallMesh(const void *key, int second, int culled, const float *screen, const float *cam, const float *vtx,
                          int quadCount, const void *quads, unsigned long long tex0);
extern s32 BtlScene_IsSecondView(void);
#endif

/* Draws the quads of a mesh that face the camera, two triangles each. */
void EftBound_DrawMeshCulled(EftBoundMesh *mesh) {
    Vec4 cam;
    Vec4 d;
    EftBoundIdx idx;
    EftBoundParam *param = gEftBound->param;
    EftTexEntry *tex = gEftBound->tex.entry;
    EftBoundVtx *vtx = mesh->vtx;
    EftBoundMesh *m;
    s32 i;

    Vec4_Copy(&cam, &gBtlCamView->pos);
#ifdef PORT
    /* The PC port's in-between pictures draw the wall from this patch (port/src/gs/gs_draw.c, interp_wall): the
       triangles below reach the GS cut off at the view's edges, differently every tick. */
    Port_WallMesh(mesh, BtlScene_IsSecondView(), 1, (const float *)&gBtlCamView->screen, (const float *)&gBtlCamView->pos,
                  (const float *)mesh->vtx, mesh->quadCount, mesh->quad, tex->tex0);
#endif
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->screen);
    for (i = 0; i < mesh->quadCount; i++) {
        m = (EftBoundMesh *)((u8 *)mesh + i * sizeof(EftBoundQuad));
        idx = m->quad[0].idx;
        Vec3_Sub(&d, (Vec4 *)&vtx[idx.i[1]].pos, &cam);
        Vec3_Normalize(&d, &d);
        if (Vec3_Dot(&m->quad[0].normal, &d) < 0.0f) {
            EftBound_DrawTri(*(EftMtxArg *)&gBtlCamView->screen, vtx[idx.i[0]].pos, vtx[idx.i[1]].pos,
                             vtx[idx.i[2]].pos, vtx[idx.i[0]].st, vtx[idx.i[1]].st, vtx[idx.i[2]].st,
                             vtx[idx.i[0]].col, vtx[idx.i[1]].col, vtx[idx.i[2]].col, param->otZ, tex->tex0);
            EftBound_DrawTri(*(EftMtxArg *)&gBtlCamView->screen, vtx[idx.i[2]].pos, vtx[idx.i[1]].pos,
                             vtx[idx.i[3]].pos, vtx[idx.i[2]].st, vtx[idx.i[1]].st, vtx[idx.i[3]].st,
                             vtx[idx.i[2]].col, vtx[idx.i[1]].col, vtx[idx.i[3]].col, param->otZ, tex->tex0);
        }
    }
    Vu0Cur_Pop();
}

/* Draws every quad of a mesh, two triangles each. */
void EftBound_DrawMesh(EftBoundMesh *mesh) {
    EftBoundIdx idx;
    EftBoundParam *param = gEftBound->param;
    EftTexEntry *tex = gEftBound->tex.entry;
    EftBoundVtx *vtx = mesh->vtx;
    s32 i;

#ifdef PORT
    Port_WallMesh(mesh, BtlScene_IsSecondView(), 0, (const float *)&gBtlCamView->screen, (const float *)&gBtlCamView->pos,
                  (const float *)mesh->vtx, mesh->quadCount, mesh->quad, tex->tex0);
#endif
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->screen);
    for (i = 0; i < mesh->quadCount; i++) {
        idx = mesh->quad[i].idx;
        EftBound_DrawTri(*(EftMtxArg *)&gBtlCamView->screen, vtx[idx.i[0]].pos, vtx[idx.i[1]].pos,
                         vtx[idx.i[2]].pos, vtx[idx.i[0]].st, vtx[idx.i[1]].st, vtx[idx.i[2]].st,
                         vtx[idx.i[0]].col, vtx[idx.i[1]].col, vtx[idx.i[2]].col, param->otZ, tex->tex0);
        EftBound_DrawTri(*(EftMtxArg *)&gBtlCamView->screen, vtx[idx.i[2]].pos, vtx[idx.i[1]].pos,
                         vtx[idx.i[3]].pos, vtx[idx.i[2]].st, vtx[idx.i[1]].st, vtx[idx.i[3]].st,
                         vtx[idx.i[2]].col, vtx[idx.i[1]].col, vtx[idx.i[3]].col, param->otZ, tex->tex0);
    }
    Vu0Cur_Pop();
}

/* Per-fighter init callback: reads the fighter's size and the stage's radius and heights. */
void EftBound_Init(EftTask *task, s32 *arg) {
    EftBoundChar *work = task->work;
    EftBoundParam *param = gEftBound->param;
    f32 h;

    memset(work, 0, sizeof(EftBoundChar));
    work->objId = *arg;
    work->scale = BtlCharApi_GetHeight(*arg) / 19.35f;
    if (work->scale < 0.7f) {
        work->scale = 0.7f;
    }
    work->charRadius = BtlCharApi_GetRadius(*arg);
    work->radius = BtlStage_GetInnerRadius() + param->radiusAdd;
    work->unk8 = param->unk10 - BtlStage_GetTop();
    h = BtlStage_GetBottom();
    work->unkC = h - work->unk8;
    work->wall.scrollS = 0.0f;
    work->wall.scrollT = 0.0f;
    work->unk850.scrollS = 0.0f;
    work->unk850.scrollT = 0.0f;
    work->wall.active = 0;
    work->unk850.active = 0;
}

/* Per-fighter term callback: nothing. */
void EftBound_Term(void) {
}

/* Per-fighter update callback: rebuilds the wall mesh; the first fighter with a visible mesh advances the
   shared textures for this frame. */
void EftBound_Update(EftTask *task) {
    EftBoundChar *work = task->work;
    EftTexSet *tex;
    s32 i;

    EftBound_UpdateAngle(work, work);
    EftBound_BuildWall(work, work);
    if (gEftBound->texStepped == 0) {
        tex = &gEftBound->tex;
        if (work->wall.active != 0 || work->unk850.active != 0) {
            for (i = 0; i < tex->count; i++) {
                tex->entry[i].tex0 = EftVram_AddTex(&tex->entry[i], 1, 0);
            }
            gEftBound->texStepped = 1;
        }
    }
}

/* Per-fighter post-update callback. */
void EftBound_PostUpdate(void) {
    if (BtlScene_IsTimeStopped()) {
        return;
    }
}

/* Per-fighter reset callback: nothing. */
void EftBound_Reset(void) {
}

/* Per-fighter draw callback. Skipped when the fighter is hidden, or locked on while it or its opponent has a
   rush connected, or when the wall is disabled. */
void EftBound_Draw(EftTask *task) {
    EftBoundChar *work = task->work;

    if (!EftStage_IsDrawOn()) {
        return;
    }
    if (BtlCharApi_IsHidden(work->objId)) {
        return;
    }
    if (BtlCharApi_IsLockedOn(work->objId)) {
        if (BtlCharApi_IsRushConnected(work->objId)) {
            return;
        }
        if (BtlCharApi_IsRushConnected(BtlCharApi_GetOpponentObjId(work->objId))) {
            return;
        }
    }
    if (gEftBound->enabled != 1) {
        return;
    }
    if (work->wall.active != 0) {
        EftBound_DrawMeshCulled(&work->wall);
    }
    if (work->unk850.active != 0) {
        EftBound_DrawMesh(&work->unk850);
    }
}

/* Boundary manager init callback: loads the textures (stage pack entry 21), the parameters (23) and the colour
   (22), enables the wall and creates the tasks of fighters 0 and 1. */
void EftBoundMgr_Init(EftTask *task) {
    s32 pack = BtlScene_GetStageData();

    gEftBound = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftBound));
    memset(gEftBound, 0, sizeof(EftBound));
    gEftBound->texPack = BtlScene_GetPackEntry((s32 *)pack, 21);
    gEftBound->param = (struct EftBoundParam *)BtlScene_GetPackEntry((s32 *)pack, 23);
    gEftBound->color = (f32 *)BtlScene_GetPackEntry((s32 *)pack, 22);
    EftTexSet_Load32(&gEftBound->tex, gEftBound->texPack);
    EftBoundMgr_Enable();
    gEftBoundList = BtlTask_CreateChildList(task, 2, sizeof(EftBoundChar));
    EftBoundMgr_AddChar(0);
    EftBoundMgr_AddChar(1);
}

/* Boundary manager term callback. */
void EftBoundMgr_Term(void) {
    BtlTaskList_KillAll(gEftBoundList);
    BtlPool_Free(BtlPool_GetCurrent(), gEftBound);
    gEftBound = NULL;
}

/* Boundary manager update callback: a new frame, the textures have not been advanced yet. */
void EftBoundMgr_Update(void) {
    gEftBound->texStepped = 0;
}

/* Creates the wall task of one fighter (object id). */
s32 EftBoundMgr_AddChar(s32 objId) {
    s32 arg[4];

    arg[0] = objId;
    return BtlTaskList_AddTail(gEftBoundList, gEftBoundClass, arg) != NULL;
}

/* Lets the walls be drawn. */
void EftBoundMgr_Enable(void) {
    if (gEftBound != NULL) {
        gEftBound->enabled = 1;
    }
}

/* Stops the walls from being drawn. No caller. */
void EftBoundMgr_Disable(void) {
    if (gEftBound != NULL) {
        gEftBound->enabled = 0;
    }
}

/* Turns the low bits of a shot's flag word into the attribute mask of the fighter's animation events. */
u64 EftShot_AttrMaskFromBits(u64 bits) {
    u64 mask = (bits << 8) & 0x200;

    if (bits & 4) {
        mask |= 0x400;
    }
    if (bits & 8) {
        mask |= 0x800;
    }
    if (bits & 0x10) {
        mask |= 0x1000;
    }
    if (bits & 0x20) {
        mask |= 0x2000;
    }
    if (bits & 0x40) {
        mask |= 0x4000;
    }
    if (bits & 0x80) {
        mask |= 0x200000;
    }
    if (bits & 0x100) {
        mask |= 0x400000;
    }
    if (bits & 0x200) {
        mask |= 0x800000;
    }
    return mask;
}

/* 1 when the shot the fighter is firing has any of the given bits (EftTechEvt_GetEvents). */
s32 EftShot_TestBits(s32 objId, s32 mask) {
    return (EftTechEvt_GetEvents(objId) & mask) != 0;
}

/* Value of the fighter's animation attribute selected by the shot bits, 0 when it is not set. */
s32 EftShot_GetAttrValue(s32 objId, u64 bits) {
    u64 mask = EftShot_AttrMaskFromBits(bits);
    s32 r = BtlCharApi_ObjTestAttr(objId, mask);

    if (r != 0) {
        return BtlCharApi_ObjGetAttrValue(objId, mask);
    }
    return r;
}

/* Kind of the attribute selected by the shot bits: 0x11 when the fighter has no current slot or the attribute
   value has two of the bits 0x10000 / 0x20000 / 0x40000, 0x36 when the slot's definition has flag 0x2000,
   otherwise the fighter's own answer (0 when the attribute is not set). */
s32 EftShot_GetAttrKind(s32 objId, u64 bits) {
    s32 slot = EftShot_GetCurSlot(objId);
    u64 mask;
    s32 r;

    if (slot < 0) {
        return 0x11;
    }
    if (EftShot_HasTwoAttrs(objId, bits)) {
        return 0x11;
    }
    if (EftShot_IsSlotFlag2000(objId, slot)) {
        return 0x36;
    }
    mask = EftShot_AttrMaskFromBits(bits);
    r = BtlCharApi_ObjTestAttr(objId, mask);
    if (r != 0) {
        return BtlCharApi_ObjGetAttrKind(objId, mask);
    }
    return r;
}

/* 1 when the definition of a character's slot has flag 0x2000. */
s32 EftShot_IsSlotFlag2000(s32 chr, s32 slot) {
    EftShotChar *c = &gEftShot->chr[chr];
    EftShotSlot *s = &c->slot[slot];

    if (s->def->flags & 0x2000) {
        return 1;
    }
    return 0;
}

/* 1 when the attribute value has at least two of the bits 0x10000, 0x20000 and 0x40000. */
s32 EftShot_HasTwoAttrs(s32 objId, u64 bits) {
    s32 v = EftShot_GetAttrValue(objId, bits);
    s32 n = 0;

    if (v & 0x20000) {
        n = 1;
    }
    if (v & 0x10000) {
        n++;
    }
    if (v & 0x40000) {
        n++;
    }
    if (n < 2) {
        return 0;
    }
    return 1;
}

/* Splits the attribute value into two kinds (0x11 = none): 0x20000 gives A = 0x2D, 0x10000 gives B = 0x1F, and
   0x40000 turns the other one into 0x36. Returns 0 when none of the three bits is set. */
s32 EftShot_GetAttrPair(s32 objId, u64 bits, s32 *outA, s32 *outB) {
    s32 v = EftShot_GetAttrValue(objId, bits);
    s32 a;
    s32 b;

    *outA = 0x11;
    *outB = 0x11;
    if (!(v & 0x70000)) {
        return 0;
    }
    a = v & 0x20000;
    if (a) {
        *outA = 0x2D;
    }
    b = v & 0x10000;
    if (b) {
        *outB = 0x1F;
    }
    if (v & 0x40000) {
        if (a) {
            *outB = 0x36;
        }
        if (b) {
            *outA = 0x36;
        }
    }
    return 1;
}

/* Sets held flag 0xA8 on the fighter (BTL_SUPER_FLAG_END: the beam is over, leave the firing loop). */
void EftShot_SetHeldFlagA8(s32 objId) {
    BtlCharApi_SetHeldFlagA8(objId);
}

/* Sets held flag 0xA9 on the fighter (BTL_SUPER_FLAG_END2: the same for the second firing stage). */
void EftShot_SetHeldFlagA9(s32 objId) {
    BtlCharApi_SetHeldFlagA9(objId);
}

/* Entry point of the fighter's effect layer (BtlChar_SpawnFxBits3C): starts the shot of a slot. */
void EftShot_Request(void *req) {
    EftShot_Start(req);
}

/* Nothing. */
void EftShot_Nop(void) {
}

/* Sets the slot a character is using. */
void EftShot_SetCurSlot(s32 chr, s32 slot) {
    EftShotChar *c = &gEftShot->chr[chr];

    c->curSlot = slot;
}

/* Slot a character is using, -1 for none. */
s32 EftShot_GetCurSlot(s32 chr) {
    EftShotChar *c = &gEftShot->chr[chr];

    return c->curSlot;
}

/* Plays one of the fighter's technique sounds for a shot whose definition has unk4 == 0: 0 for bit 2 (except
   shot id 0x27), 1 for bit 4, 2 when the fighter has attribute 0x40000; the last that applies wins. */
void EftShot_PlayFireSound(s32 objId, EftShotSlot *slot) {
    s32 snd = -1;

    if (slot->def->unk4 == 0) {
        if (EftShot_TestBits(objId, 2)) {
            if (slot->def->id != 0x27) {
                snd = 0;
            }
        }
        if (EftShot_TestBits(objId, 4)) {
            snd = 1;
        }
        if (BtlCharApi_ObjTestAttr(objId, 0x40000)) {
            snd = 2;
        }
    }
    if (snd >= 0) {
        BtlCharApi_PlayTechniqueSound(objId, snd);
    }
}

/* Shot layer init callback (scene layer 1): one EftShotChar per character from pool 3, a child list with one
   task of class 0x2C3928 and one task per character (0 and 1). */
void EftShotMgr_Init(EftTask *task) {
    s32 arg[2];
    s32 size;
    EftShotChar *c;

    BtlPool_SetCurrent(3);
    gEftShot = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftShotMgr));
    memset(gEftShot, 0, sizeof(EftShotMgr));
    gEftShot->count = BtlScene_GetCharCount();
    size = gEftShot->count * sizeof(EftShotChar);
    gEftShot->chr = BtlPool_Alloc(BtlPool_GetCurrent(), size);
    memset(gEftShot->chr, 0, size);
    gEftShot->list = BtlTask_CreateChildList(task, gEftShot->count + 1, 0);
    BtlTaskList_AddTail(gEftShot->list, gEftTechEvtClass, NULL);
    arg[0] = 0;
    arg[1] = 1;
    c = gEftShot->chr;
    c->task = BtlTaskList_AddTail(gEftShot->list, gEftShotCharClass, &arg[0]);
    c = gEftShot->chr + 1;
    c->task = BtlTaskList_AddTail(gEftShot->list, gEftShotCharClass, &arg[1]);
}

/* Shot layer term callback: frees the blocks and empties pools 3, 4 and 5. */
void EftShotMgr_Term(void) {
    BtlPool_SetCurrent(3);
    BtlPool_Free(BtlPool_GetCurrent(), gEftShot->chr);
    BtlPool_Free(BtlPool_GetCurrent(), gEftShot);
    gEftShot = NULL;
    BtlPool_Reset(3);
    BtlPool_Reset(4);
    BtlPool_Reset(5);
}

/* Shot layer update callback: nothing. */
void EftShotMgr_Update(void) {
}

/* Character task init callback: selects the character's pool, creates its child list (6 tasks of 0x2000 bytes)
   and sets up its five slots from the character pack. */
void EftShotChar_Init(EftTask *task, s32 *arg) {
    EftShotChar *chr;
    s32 i;

    chr = &gEftShot->chr[*arg];
    task->chr = *arg;
    EftShot_SetCharPool(*arg);
    chr->list = BtlTask_CreateChildList(task, 6, 0x2000);
    for (i = 0; i < EFT_SHOT_SLOTS; i++) {
        if (EftShot_InitSlot(*arg, i, EftShot_GetCharPack(*arg, i))) {
            EftShot_CreateSlotTask(*arg, i);
        }
    }
}

/* Character task term callback: empties the character's pool. */
void EftShotChar_Term(EftTask *task) {
    EftShot_ResetCharPool(task->chr);
}

/* Character task update callback (runs once per character task, each time over every character): a character
   that is in neither a technique nor a skill has no current slot. The character index is used as object id. */
void EftShotChar_Update(void) {
    EftShotChar *chr;
    s32 i;

    for (i = 0; i < gEftShot->count; i++) {
        chr = &gEftShot->chr[i];
        if (!BtlCharApi_IsInTechnique(i) && !BtlCharApi_IsInSkill(i)) {
            chr->curSlot = -1;
        }
    }
}

/* Makes the character's pool current: 4 for character 0, 5 for the others. */
void EftShot_SetCharPool(s32 chr) {
    s32 pool;

    if (chr == 0) {
        pool = 4;
    } else {
        pool = 5;
    }
    BtlPool_SetCurrent(pool);
}

/* Empties the character's pool. */
void EftShot_ResetCharPool(s32 chr) {
    s32 pool;

    if (chr == 0) {
        pool = 4;
    } else {
        pool = 5;
    }
    BtlPool_Reset(pool);
}

/* Clears a slot and binds it to its definition; reads the definition from the pack data when there is one.
   Returns 1 when the slot has pack data. */
s32 EftShot_InitSlot(s32 chr, s32 slot, void *pack) {
    EftShotChar *c = &gEftShot->chr[chr];
    EftShotSlot *s = &c->slot[slot];
    EftShotDef *def;

    memset(s, 0, sizeof(EftShotSlot));
    if (pack == NULL) {
        def = &c->def[slot];
        s->def = def;
        EftShot_BuildParam(chr, slot, def, 1);
        return 0;
    }
    s->pack = pack;
    s->def = &c->def[slot];
    EftShot_BuildParam(chr, slot, &c->def[slot], 0);
    return 1;
}
