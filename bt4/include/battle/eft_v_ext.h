#ifndef BATTLE_EFT_V_EXT_H
#define BATTLE_EFT_V_EXT_H

/*
 * What the second part of src/battle/eft_u_b.c (formerly eft_v.c), eft_v_b.c and eft_v_c.c use from other modules: local declarations only (every
 * prototype here is this module's view; the callee's own file is the reference), plus the macros the three
 * files share. Not meant to be included by anything else.
 */

#include "battle/eft_v.h"
#include "sys/math3d.h"
#include "sys/gfx_ot.h"

extern EftVView *gBtlCamView;
extern EftPtclMgr *gEftPtcl;
extern void *gEftPtclList;
extern void *gEftPtclClass[6];
extern EftImpactMgr *gEftImpact;
extern void *gEftImpactList;
extern void *gEftImpactClass[6];
extern EftLinkMgr *gEftLink;

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 atan2f(f32 y, f32 x);
extern f32 asinf(f32 x);
extern f32 Mathf_Asin(f32 x);
extern f32 Rand_FloatRange(f32 a, f32 b);
extern void Vec4_Set(EftVVec *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(EftVVec *dst, EftVVec *src);
extern void Vec4_Add(EftVVec *dst, EftVVec *a, EftVVec *b);
extern void Vec4_Sub(EftVVec *dst, EftVVec *a, EftVVec *b);
extern void Vec4_Scale(EftVVec *dst, EftVVec *src, f32 s);
extern void Vec3_Add(EftVVec *dst, EftVVec *a, EftVVec *b);
extern void Vec3_Sub(EftVVec *dst, EftVVec *a, EftVVec *b);
extern void Vec3_Scale(EftVVec *dst, EftVVec *src, f32 s);
extern void Vec3_Cross(EftVVec *dst, EftVVec *a, EftVVec *b);
extern void Vec3_Normalize(EftVVec *dst, EftVVec *src);
extern f32 Vec3_Length(EftVVec *v);
extern void Mtx_StoreIdentity(EftVMtx *m);
extern void Mtx_MulVec4(EftVVec *dst, EftVMtx *m, EftVVec *src);
extern void Mtx_Copy(EftVMtx *dst, EftVMtx *src);              /* matrix copy */
extern void Mtx_RotateX(EftVMtx *dst, EftVMtx *src, f32 angle);   /* rotate about X */
extern void Mtx_RotateY(EftVMtx *dst, EftVMtx *src, f32 angle);   /* rotate about Y */
extern void Vu0Cur_Push(void);                                    /* VU0 matrix stack: push */
extern void Vu0Cur_Pop(void);                                    /* VU0 matrix stack: pop */
extern void Vu0Cur_LoadMtx(EftVMtx *m);                              /* VU0 current matrix = m */
extern s32 Vu0Cur_ProjectPoints(EftVIVec *out, EftVVec *pos, s32 count);   /* project count points; 0 when clipped */
extern void ClipVtx_Set(EftVVert *out, EftVVec *pos, EftVVec *uv, EftVVec *color);
extern void Vec3_Copy(EftVVec *dst, EftVVec *src);              /* copies x, y, z */
extern void Vec4_ToInt(EftVIVec *dst, EftVVec *src);             /* float vector to integer vector */
extern void Vec3_Clamp(EftVVec *dst, EftVVec *src, f32 lo, f32 hi); /* clamp x, y, z */
extern void Vec3_ScaleAdd(EftVVec *dst, EftVVec *dir, EftVVec *base, f32 s); /* dst = base + dir * s */
extern f32 EftMath_WrapAngle(f32 angle);
extern void EftPrim_DrawQuadDepth(EftVVec *pos, EftVVec *color, s32 layer, s32 front, u64 tex0, f32 w, f32 h, f32 u0,
                                  f32 v0, f32 u1, f32 v1, f32 rot);
extern void EftGfx_DrawSprite(EftVVec *pos, EftVVec *color, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1,
                              f32 rot, s32 layer, s32 front, u64 tex0);
extern void EftGfx_DrawPolyScaledZ(EftVVert *verts, s32 layer, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex0,
                                   f32 zScale);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *p);
extern void *BtlTask_CreateChildList(EftVTask *task, s32 count, s32 workSize);
extern EftVTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_SetDead(EftVTask *task);                          /* marks a task as dying */
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);
extern s32 BtlScene_IsCharInView(s32 objId);
extern s32 BtlScene_IsTimeStopped(void);
extern f32 BtlScene_GetCharScale(s32 objId);

/* The head of the particle emitter module (0x182CE8..0x1853C8, another file). */
extern void EftPtcl_PickTexture(EftPtclWork *w, void *res, s32 a, s32 b);    /* picks the emitter's texture (tex0) */
extern void EftPtcl_UploadTexture(EftPtclWork *w, EftPtclWork *w2);            /* per-frame step of the emitter's own values */
extern void EftPtcl_SetKey(EftPtclCur *cur, EftPtclWork *w, s32 key);   /* current values = key `key` of the definition */
extern void EftPtcl_BlendKeys(EftPtclWork *w);                             /* steps the current values between keys */
extern void EftPtcl_FreePtcls(EftPtclWork *w);                             /* frees the emitter's particles */
extern void EftPtcl_Spawn(EftPtclWork *w, EftPtclWork *w2, s32 count); /* creates `count` particles */
extern void EftPtcl_StepPtcls(EftPtclWork *w, EftPtclWork *w2);            /* steps the particles */

/* The effect pack library (eft_h.c / eft_i.c). */
extern void EftEmit_LoadSet(void *owner, EftVSet *set, void *head, s32 *base, s32 common, s32 idx);
extern void EftEmit_FreeSet(EftVSet *set);
extern void EftEmit_BeginFrame(EftVSet *set);
extern s32 EftEmit_GetEndFrames(EftVSet *set);
extern void EftEmit_InitState(EftVSet *set, EftVState *st);
extern void EftEmit_TermState(EftVSet *set, EftVState *st);
extern s32 EftEmit_GetFlagsFromMask(EftVSet *set, EftVState *st, s32 objId, s32 type, s32 idx, s32 ending, s32 kill,
                                    s32 mask);
extern void EftEmit_Spawn(EftVSet *set, EftVState *state, EftVNodes *nodes, EftVVec *pos, EftVVec *dir, s32 objId,
                          s32 node, s32 arg7, s32 type, s32 idx, s32 flags, f32 scale);
extern void EftEmit_KillAll(EftVSet *set, EftVState *state);
extern s32 EftEmit_UpdateAlive(EftVSet *set, EftVState *state);
extern void EftEmit_SetNode(EftVNodes *nodes, s32 slot, s32 node, EftVVec *pos);
extern void EftEmit_SetNodePos(EftVNodes *nodes, s32 slot, EftVVec *pos);

/* The rest of the sprite chain module (0x1895E8.., another file). */
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftLink_DrawQuad(EftLinkNode *n, EftVVec uv0, EftVVec uv1, EftVVec color, s32 layer, s32 frame, s32 front, EftVTex *tex);
#define EftLink_DrawQuad(n, uv0, uv1, color, layer, frame, front, tex) EftLink_DrawQuad(n, *(EftVVec *)(uv0), *(EftVVec *)(uv1), *(EftVVec *)(color), layer, frame, front, tex)
#else
extern void EftLink_DrawQuad(EftLinkNode *n, EftVVec *uv0, EftVVec *uv1, EftVVec *color, s32 layer, s32 frame, s32 front,
                          EftVTex *tex);
#endif
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftLink_DrawQuadClipped(EftLinkNode *n, EftVVec uv0, EftVVec uv1, EftVVec color, s32 layer, s32 frame, s32 front, EftVTex *tex);
#define EftLink_DrawQuadClipped(n, uv0, uv1, color, layer, frame, front, tex) EftLink_DrawQuadClipped(n, *(EftVVec *)(uv0), *(EftVVec *)(uv1), *(EftVVec *)(color), layer, frame, front, tex)
#else
extern void EftLink_DrawQuadClipped(EftLinkNode *n, EftVVec *uv0, EftVVec *uv1, EftVVec *color, s32 layer, s32 frame, s32 front,
                          EftVTex *tex);
#endif
extern void EftLink_SetKey(EftLinkWork *w, s32 key);                    /* current values = key `key` */
extern void EftLink_SelectTex(EftLinkWork *w, EftVTex *tex, s32 frame, s32 arg3);
extern void EftLink_BuildTex(EftLinkWork *w);
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern EftLinkNode *EftLink_NewNode(EftLinkWork *w, EftVVec pos, f32 index);
#define EftLink_NewNode(w, pos, index) EftLink_NewNode(w, *(EftVVec *)(pos), index)
#else
extern EftLinkNode *EftLink_NewNode(EftLinkWork *w, EftVVec *pos, f32 index); /* takes a node from the pool */
#endif
extern void EftLink_InitNode(EftLinkNode *n, EftLinkWork *w);             /* initialises a node from the current values */
extern void EftLink_StepNodes(EftLinkWork *w, f32 fade);                   /* steps the nodes */
extern void EftLink_UnlinkNode(EftLinkNode **head, EftLinkNode **tail, EftLinkNode *n); /* unlinks a node */
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void EftLink_DrawBillboardClipped(EftVVec *pos, f32 w, f32 h, EftVVec *color, EftVVec *unk, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot, s32 layer, s32 front, u64 tex0, f32 zScale);
#define EftLink_DrawBillboardClipped(pos, color, unk, w, h, u0, v0, u1, v1, rot, layer, front, tex0, zScale) EftLink_DrawBillboardClipped(pos, w, h, color, unk, u0, v0, u1, v1, rot, layer, front, tex0, zScale)
#else
extern void EftLink_DrawBillboardClipped(EftVVec *pos, EftVVec *color, EftVVec *unk, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1,
                          f32 rot, s32 layer, s32 front, u64 tex0, f32 zScale);
#endif
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void EftLink_DrawBillboard(EftVVec *pos, f32 sx, f32 sy, EftVVec *color, s32 w, s32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot, s32 layer, s32 front, u64 tex0);
#define EftLink_DrawBillboard(pos, color, w, h, sx, sy, u0, v0, u1, v1, rot, layer, front, tex0) EftLink_DrawBillboard(pos, sx, sy, color, w, h, u0, v0, u1, v1, rot, layer, front, tex0)
#else
extern void EftLink_DrawBillboard(EftVVec *pos, EftVVec *color, s32 w, s32 h, f32 sx, f32 sy, f32 u0, f32 v0, f32 u1, f32 v1,
                          f32 rot, s32 layer, s32 front, u64 tex0);
#endif

/* Locals of the sprite chain module are 4-byte aligned vectors (with the aligned type the compiler clears them
   inline instead of calling memset): the plain Vec4 / Mtx44 in EftLink_Draw, the union EftVVecU in EftLink_Place.
   Each of the two functions matches only with that choice. */
#define EFTV_VEC(p) ((EftVVec *)(p))
#define EFTV_MTX(p) ((EftVMtx *)(p))

/* The float `o` bytes after *p. The quad drawing addresses its vertex arrays as "field of element 0, plus a byte
   offset": with plain uv[i].z the compiler strength-reduces the loops, which the original did not. */
#define EFTV_AT(p, o) (*(f32 *)((u8 *)(p) + (o)))

#define EFTV_RAND01() ((f32)rand() / 2147483647.0f)

#endif
