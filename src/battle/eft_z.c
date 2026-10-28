#include "common.h"
#include "battle/eft_y.h"
#include "sys/math3d.h"
#include "sys/gfx_ot.h"

/*
 * Line helpers (effect pack part kind 16), 0x195038..0x195EE8, written as the tail of eft_y.c (the declarations down to
 * the first function are eft_y.c's preamble; see include/battle/eft_y.h). They are the head of the sprite module below
 * (`EftBill_*`, formerly the whole of eft_z.c): the same module under two names, `EftLine_*` here and `EftBill_*`
 * there. `EftBill_SetTexture` only matches with `EftLine_SetTex` defined above it in the same file.
 */

extern EftQuadPool *gEftQuadMgr;  /* the quad pool; NULL outside a battle */
extern void *gEftQuadClass[6];      /* task class of the quad emitter */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);
extern f32 EftMath_WrapAngle(f32 angle);
extern void Vec4_Set(EftYVec *v, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(EftYVec *dst, EftYVec *src);
extern void Vec4_Sub(EftYVec *dst, EftYVec *a, EftYVec *b);
extern void Vec4_Add(EftYVec *dst, EftYVec *a, EftYVec *b);
extern void Vec4_Scale(EftYVec *dst, EftYVec *src, f32 s);
extern void Vec3_Add(EftYVec *dst, EftYVec *a, EftYVec *b);
extern void Vec3_Sub(EftYVec *dst, EftYVec *a, EftYVec *b);
extern void Vec3_Scale(EftYVec *dst, EftYVec *src, f32 s);
extern void Vec3_Cross(EftYVec *dst, EftYVec *a, EftYVec *b);
extern void Vec3_Normalize(EftYVec *dst, EftYVec *src);
extern void Vu0Cur_LoadIdentity(void);                 /* VU0 current matrix = identity (inferred) */
extern void Vu0Cur_Push(void);                 /* VU0 matrix stack: push */
extern void Vu0Cur_Pop(void);                 /* pop */
extern void Vu0Cur_Translate(EftYVec *v);           /* current matrix: translate */
extern void Vu0Cur_RotateZ(f32 angle);            /* current matrix: rotate about Z */
extern void Vu0Cur_RotateX(f32 angle);            /* rotate about X */
extern void Vu0Cur_RotateY(f32 angle);            /* rotate about Y */
extern void Vu0Cur_ScaleDiagUniform(f32 scale);            /* scale */
extern void Vu0Cur_MulVec4(EftYVec *dst, EftYVec *src); /* transform a point by the current matrix */
extern s32 Vu0Cur_ProjectPoints(EftYScr *out, EftYVec *pos, s32 count); /* project; 0 when clipped */
extern s32 Vu0Cur_ProjectPointsStq(EftYScr *xyz, EftYVec *stq, EftYVec *pos, EftYVec *uv, s32 n); /* project with texture */
extern void ClipVtx_Set(EftYClipVtx *out, EftYVec *pos, EftYVec *st, EftYVec *color);
extern void Vec3_Copy(EftYVec *dst, EftYVec *src); /* copies x, y, z */
extern void Vec4_ToInt(EftYCol *dst, EftYVec *src); /* float vector to integer vector */
extern void Vec4_Clamp(EftYVec *dst, EftYVec *src, f32 lo, f32 hi); /* clamps each component */
extern void Vec3_ScaleAdd(EftYVec *dst, EftYVec *dir, EftYVec *base, f32 s); /* dst = base + dir * s */
extern void EftGfx_DrawPolyFixedZ(EftYClipVtx *verts, s32 layer, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex,
                                  s32 z);
extern void EftGfx_DrawPolyScaledZ(EftYClipVtx *verts, s32 layer, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex,
                                   f32 zScale);
extern u64 EftVram_AddImage(EftYTex *tex, s32 a, s32 b);
extern u64 EftVram_AddClut(EftYTex *tex);
extern EftYTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern s32 EftQuad_InitQuad(EftQuad *q, EftQuadWork *w); /* fills a new quad from the emitter's current values */

typedef struct EftYCamView {
    /* 0x000 */ u8 unk0[0x220];
    /* 0x220 */ EftYVec pos;
} EftYCamView;
extern EftYCamView *gBtlCamView;

#define EFTY_CLAMP(x, lo, hi) (((x) < (lo)) ? (lo) : (((hi) < (x)) ? (hi) : (x)))
#define EFTY_CLAMP01(x) EFTY_CLAMP(x, 0.0f, 1.0f)

/* Binds a line to two entries of a texture table. */
void EftLine_SetTex(EftLineWork *w, EftYTex16 *tex, s32 a, s32 b) {
    w->tex0 = *(a + tex->e);
    w->tex1 = *(b + tex->e);
    w->arg.texIdx = b;
}

/* Builds the line's GS TEX0 value, or takes the one already built for its table entry. */
void EftLine_LoadTex(EftLineWork *w, EftLineArg *arg) {
    if (!(arg->tex->loaded & (1 << arg->texIdx))) {
        w->gsTex0 = EftVram_AddImage(&w->tex0, 1, 0);
        w->gsTex0 |= (u64)EftVram_AddClut(&w->tex1) << 37;
        arg->tex->e[arg->texIdx].tex0 = w->gsTex0;
        arg->tex->loaded |= 1 << arg->texIdx;
    } else {
        w->gsTex0 = arg->tex->e[arg->texIdx].tex0;
    }
}

/* The line's animated values at one key. */
void EftLine_SetKey(EftLineCur *out, EftLineArg *arg, s32 key) {
    EftYVec a;
    EftYVec b;
    EftYVec c;
    EftLineDef *def = arg->def;
    EftLineDef2 *def2 = arg->def2;
    s32 i;

    Vec4_Copy(&out->color, &def2->color[key]);
    for (i = 0; i < 2; i++) {
        a.v[i] = def2->u[key][i];
        b.v[i] = def2->v[key][i];
        c.v[i] = def2->q[key][i];
    }
    out->u0 = a.v[0];
    out->du = a.v[1] - a.v[0];
    out->v0 = b.v[0];
    out->q0 = c.v[0];
    out->dv = b.v[1] - b.v[0];
    out->dq = c.v[1] - c.v[0];
    out->scroll = def2->scroll[key] * 30.0f;
    out->spin = def->spin[key] * 3.14159265f;
    out->width = def->width[key];
    out->length = def->length[key];
    a.v[0] = def->b0[key];
    a.v[1] = def->b1[key];
    out->b0 = a.v[0];
    out->bRange = a.v[1] - a.v[0];
    out->bTime = def->bTime[key] * 30.0f;
    a.v[0] = def->c0[key];
    a.v[1] = def->c1[key];
    out->c0 = a.v[0];
    out->cRange = a.v[1] - a.v[0];
    out->cTime = def->cTime[key] * 30.0f;
}

/* The line's animated values between two keys, from the time since the animation started. */
void EftLine_Animate(EftLineWork *w) {
    EftYVec d;
    EftYVec a;
    EftYVec b;
    EftYVec c;
    EftLineCur *out = &w->cur;
    EftLineDef *def = w->arg.def;
    EftLineDef2 *def2 = w->arg.def2;
    f32 t;
    s32 k0;
    s32 k1;
    s32 i;

    if (w->time < w->animSplit) {
        t = w->time / w->animSplit;
        k0 = 0;
        k1 = 1;
    } else {
        t = w->time - w->animSplit;
        k0 = 1;
        k1 = 2;
        t /= w->animEnd - w->animSplit;
    }
    Vec4_Sub(&d, &def2->color[k1], &def2->color[k0]);
    out->color.v[0] = def2->color[k0].v[0] + d.v[0] * t;
    out->color.v[1] = def2->color[k0].v[1] + d.v[1] * t;
    out->color.v[2] = def2->color[k0].v[2] + d.v[2] * t;
    out->color.v[3] = def2->color[k0].v[3] + d.v[3] * t;
    for (i = 0; i < 2; i++) {
        d.x = def2->u[k1][i] - def2->u[k0][i];
        d.y = def2->v[k1][i] - def2->v[k0][i];
        d.z = def2->q[k1][i] - def2->q[k0][i];
        a.v[i] = def2->u[k0][i] + d.x * t;
        b.v[i] = def2->v[k0][i] + d.y * t;
        c.v[i] = def2->q[k0][i] + d.z * t;
    }
    out->u0 = a.v[0];
    out->du = a.v[1] - a.v[0];
    out->v0 = b.v[0];
    out->q0 = c.v[0];
    out->dv = b.v[1] - b.v[0];
    out->dq = c.v[1] - c.v[0];
    d.x = def2->scroll[k1] - def2->scroll[k0];
    out->scroll = (def2->scroll[k0] + d.x * t) * 30.0f;
    d.x = def->spin[k1] - def->spin[k0];
    out->spin = (def->spin[k0] + d.x * t) * 3.14159265f;
    d.x = def->width[k1] - def->width[k0];
    d.y = def->length[k1] - def->length[k0];
    out->width = def->width[k0] + d.x * t;
    out->length = def->length[k0] + d.y * t;
    d.x = def->b0[k1] - def->b0[k0];
    d.y = def->b1[k1] - def->b1[k0];
    d.z = def->bTime[k1] - def->bTime[k0];
    a.v[0] = def->b0[k0] + d.x * t;
    a.v[1] = def->b1[k0] + d.y * t;
    out->b0 = a.v[0];
    out->bRange = a.v[1] - a.v[0];
    out->bTime = (def->bTime[k0] + d.z * t) * 30.0f;
    d.x = def->c0[k1] - def->c0[k0];
    d.y = def->c1[k1] - def->c1[k0];
    d.z = def->cTime[k1] - def->cTime[k0];
    a.v[0] = def->c0[k0] + d.x * t;
    a.v[1] = def->c1[k0] + d.y * t;
    out->c0 = a.v[0];
    out->cRange = a.v[1] - a.v[0];
    out->cTime = (def->cTime[k0] + d.z * t) * 30.0f;
}

/* Draws the line as one order table strip: a quad from pos -/+ side * width to the same two points + dir *
   length, where side is perpendicular to dir and to the direction to the camera. */
/* NON-MATCHING: 26 of 340 instructions differ, one cause: the original fills the delay slot of the branch after
   Vu0Cur_ProjectPoints with the first instruction of the epilogue (ld s0) and this C with the following load of gOtCur, which
   shifts the 25 instructions up to the next alignment nop by one. Same instructions otherwise.
   Cleanup E: the delay-slot pass tries the fall-through first for a forward `beqz` and takes `lw s0,gOtCur`; the
   original took the target's first instruction and retargeted the branch (the second branch still goes to the
   unshifted label), so there the fall-through was not usable: a label directly behind the branch, or s0 live at the
   target. Early return, goto, an inverted test, do-while around the body, the tail stores duplicated in the
   three depth arms and an empty do-while in front of the end label all compile to the same 26. */
#if 0
void EftLine_DrawSprite(EftYTask *task) {
    EftYVec p[4];
    EftYVecU st[4];
    EftYVec stq[4];
    EftYVec cam;
    EftYVec side;
    EftYVec ofs;
    EftYVec len;
    EftYScr scr[4];
    EftYCol col;
    EftLineDef *def;
    EftLineWork *w = task->work;
    EftYStripPkt *pk;
    OtEntry *e;
    s32 i;
    s32 z;
    s32 layer;

    def = w->arg.def;
    Vec4_Copy(&cam, &gBtlCamView->pos);
    Vec3_Sub(&side, &w->arg.pos, &cam);
    Vec3_Cross(&side, &w->arg.dir, &side);
    Vec3_Normalize(&side, &side);
    Vec3_Scale(&ofs, &side, w->width);
    Vec3_Add(&p[0], &w->arg.pos, &ofs);
    Vec3_Sub(&p[1], &w->arg.pos, &ofs);
    Vec3_Scale(&len, &w->arg.dir, w->length);
    Vec3_Add(&p[2], &p[0], &len);
    Vec3_Add(&p[3], &p[1], &len);
    if (Vu0Cur_ProjectPoints(scr, p, 4)) {
        pk = (EftYStripPkt *)gOtCur;
        gOtCur = (u32 *)(pk + 1);
        if (pk != NULL) {
            if (def->layer < 2) {
                pk->prim = 0x5C;
                pk->dmaTag = 0x20000008;
                pk->vif0 = 0x10000000;
                pk->vif1 = 0x50000008;
                pk->gifTag = 0xE400000000008001;
                pk->regs = 0x42142142142160;
                pk->next = NULL;
            } else {
                pk->prim = 0x25C;
                pk->dmaTag = 0x20000008;
                pk->vif0 = 0x10000000;
                pk->vif1 = 0x50000008;
                pk->gifTag = 0xE400000000008001;
                pk->regs = 0x42142142142170;
                pk->next = NULL;
            }
            for (i = 0; i < 4; i++) {
                f32 rhw = 1.0f / scr[i].w;

                st[i].x = (i % 2) * w->du + w->u0;
                st[i].y = (i / 2) * w->dv + w->v0;
                st[i].z = 1.0f;
                st[i].w = 0.0f;
                Vec3_Scale(&stq[i], (EftYVec *)&st[i], rhw);
            }
            if (w->flags & 0x20) {
                for (i = 3; i >= 0; i--) {
                    scr[i].z = 0xFFFFFF;
                }
            }
            z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
            Vec4_ToInt(&col, &w->color);
            pk->v[0].rgbaq.r = col.r;
            pk->v[0].rgbaq.g = col.g;
            pk->v[0].rgbaq.b = col.b;
            pk->v[0].rgbaq.a = col.a;
            pk->v[0].rgbaq.q = stq[0].z;
            pk->v[1].rgbaq.r = col.r;
            pk->v[1].rgbaq.g = col.g;
            pk->v[1].rgbaq.b = col.b;
            pk->v[1].rgbaq.a = col.a;
            pk->v[1].rgbaq.q = stq[1].z;
            pk->v[2].rgbaq.r = col.r;
            pk->v[2].rgbaq.g = col.g;
            pk->v[2].rgbaq.b = col.b;
            pk->v[2].rgbaq.a = col.a;
            pk->v[2].rgbaq.q = stq[2].z;
            pk->v[3].rgbaq.r = col.r;
            pk->v[3].rgbaq.g = col.g;
            pk->v[3].rgbaq.b = col.b;
            pk->v[3].rgbaq.a = col.a;
            pk->v[3].rgbaq.q = stq[3].z;
            pk->v[0].st.s = stq[0].x;
            pk->v[0].st.t = stq[0].y;
            pk->v[1].st.s = stq[1].x;
            pk->v[1].st.t = stq[1].y;
            pk->v[2].st.s = stq[2].x;
            pk->v[2].st.t = stq[2].y;
            pk->v[3].st.s = stq[3].x;
            pk->v[3].st.t = stq[3].y;
            pk->v[0].xyz.x = scr[0].x;
            pk->v[0].xyz.y = scr[0].y;
            pk->v[0].xyz.z = scr[0].z;
            pk->v[0].xyz.f = 0xFF;
            pk->v[1].xyz.x = scr[1].x;
            pk->v[1].xyz.y = scr[1].y;
            pk->v[1].xyz.z = scr[1].z;
            pk->v[1].xyz.f = 0xFF;
            pk->v[2].xyz.x = scr[2].x;
            pk->v[2].xyz.y = scr[2].y;
            pk->v[2].xyz.z = scr[2].z;
            pk->v[2].xyz.f = 0xFF;
            pk->v[3].xyz.x = scr[3].x;
            pk->v[3].xyz.y = scr[3].y;
            pk->v[3].xyz.z = scr[3].z;
            pk->v[3].xyz.f = 0xFF;
            pk->tex0 = w->gsTex0;
            layer = def->layer;
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
            e->tail->next = (OtPrim *)pk;
            e->tail = (OtPrim *)pk;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/eft_z", EftLine_DrawSprite);
#endif

/* Draws the same quad as two triangles through the clipping polygon drawer, at the depth slot of the unclipped
   projection. */
void EftLine_DrawClipped(EftYTask *task) {
    EftYVec p[4];
    EftYVecU st[4];
    EftYVec cam;
    EftYVec side;
    EftYVec ofs;
    EftYVec len;
    EftYScr scr[4];
    EftYClipVtx v[9];
    EftLineDef *def;
    EftLineWork *w;
    s32 i;
    s32 z;

    i = 0;
    w = task->work;
    def = w->arg.def;
    Vec4_Copy(&cam, &gBtlCamView->pos);
    Vec3_Sub(&side, &w->arg.pos, &cam);
    Vec3_Cross(&side, &w->arg.dir, &side);
    Vec3_Normalize(&side, &side);
    Vec3_Scale(&ofs, &side, w->width);
    Vec3_Add(&p[0], &w->arg.pos, &ofs);
    Vec3_Sub(&p[1], &w->arg.pos, &ofs);
    Vec3_Scale(&len, &w->arg.dir, w->length);
    Vec3_Add(&p[2], &p[0], &len);
    Vec3_Add(&p[3], &p[1], &len);
    for (; i < 4; i++) {
        st[i].x = (i % 2) * w->du + w->u0;
        st[i].y = (i / 2) * w->dv + w->v0;
        st[i].z = 1.0f;
        st[i].w = 0.0f;
    }
    Vu0Cur_ProjectPoints(scr, p, 4);
    z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
    for (i = 0; i < 2; i++) {
        ClipVtx_Set(&v[0], &p[i], (EftYVec *)&st[i], &w->color);
        ClipVtx_Set(&v[1], &p[i + 1], (EftYVec *)&st[i + 1], &w->color);
        ClipVtx_Set(&v[2], &p[i + 2], (EftYVec *)&st[i + 2], &w->color);
        EftGfx_DrawPolyFixedZ(v, def->layer, 0, 0, (w->flags >> 5) & 1, 0, w->gsTex0, z);
    }
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly eft_z.c), with its own header and view types. Names the first part already declared with
 * other types are reached through cast macros (the generated code is the same).
 * ------------------------------------------------------------------------------------------------------------ */
#define EFT_Z_IMPL
#define gBtlCamView gBtlCamView__p2
#define Vec4_Set Vec4_Set__p2
#define Vec4_Copy Vec4_Copy__p2
#define Vec3_Copy func_00121FB8__p2
#define Vec4_Add Vec4_Add__p2
#define Vec3_Add Vec3_Add__p2
#define Vec3_Sub Vec3_Sub__p2
#define Vec4_Scale Vec4_Scale__p2
#define Vec3_Scale Vec3_Scale__p2
#define Vec3_Normalize Vec3_Normalize__p2
#define BtlTaskList_AddTail BtlTaskList_AddTail__p2
#define EftLine_SetTex EftLine_SetTex__p2
#define EftLine_LoadTex EftLine_LoadTex__p2
#define EftLine_SetKey EftLine_SetKey__p2
#define EftLine_Animate EftLine_Animate__p2
#define EftLine_DrawSprite EftLine_DrawSprite__p2
#define EftLine_DrawClipped EftLine_DrawClipped__p2
#include "battle/eft_z.h"
#undef gBtlCamView
#undef Vec4_Set
#undef Vec4_Copy
#undef Vec3_Copy
#undef Vec4_Add
#undef Vec3_Add
#undef Vec3_Sub
#undef Vec4_Scale
#undef Vec3_Scale
#undef Vec3_Normalize
#undef BtlTaskList_AddTail
#undef EftLine_SetTex
#undef EftLine_LoadTex
#undef EftLine_SetKey
#undef EftLine_Animate
#undef EftLine_DrawSprite
#undef EftLine_DrawClipped
#define gBtlCamView ((EftZView *)gBtlCamView)
#define Vec4_Set ((void (*)(Vec4 *dst, f32 x, f32 y, f32 z, f32 w))Vec4_Set)
#define Vec4_Copy ((void (*)(Vec4 *dst, Vec4 *src))Vec4_Copy)
#define Vec3_Copy ((void (*)(Vec4 *dst, Vec4 *src))Vec3_Copy)
#define Vec4_Add ((void (*)(Vec4 *dst, Vec4 *a, Vec4 *b))Vec4_Add)
#define Vec3_Add ((void (*)(Vec4 *dst, Vec4 *a, Vec4 *b))Vec3_Add)
#define Vec3_Sub ((void (*)(Vec4 *dst, Vec4 *a, Vec4 *b))Vec3_Sub)
#define Vec4_Scale ((void (*)(Vec4 *dst, Vec4 *src, f32 s))Vec4_Scale)
#define Vec3_Scale ((void (*)(Vec4 *dst, Vec4 *src, f32 s))Vec3_Scale)
#define Vec3_Normalize ((void (*)(Vec4 *dst, Vec4 *src))Vec3_Normalize)
#define BtlTaskList_AddTail ((EftZTask *(*)(void *list, void *cls, void *arg))BtlTaskList_AddTail)
#define EftLine_SetTex ((void (*)(EftBill *w, void *res, s32 a2, s32 a3))EftLine_SetTex)
#define EftLine_LoadTex ((void (*)(EftBill *w, EftBill *w2))EftLine_LoadTex)
#define EftLine_SetKey ((void (*)(EftBillKey *key, EftBill *w, s32 mode))EftLine_SetKey)
#define EftLine_Animate ((void (*)(EftBill *w))EftLine_Animate)
#define EftLine_DrawSprite ((void (*)(EftZTask *task))EftLine_DrawSprite)
#define EftLine_DrawClipped ((void (*)(EftZTask *task))EftLine_DrawClipped)

/*
 * Effect tasks, 0x195EE8..0x198BC0. See include/battle/eft_z.h; continues in eft_z_b.c and eft_z_c.c.
 * Nothing here writes a fighter, a battle object, a hit record or the stage: both modules only build draw data.
 *
 * EftGndDustSlide_Init matches since the EftGndDust_SpawnPiece prototype returns a pointer (see eft_z.h). The C
 * emits .lit4 0x2FCD80..0x2FCDCC and .rodata 0x2ED310..0x2ED360 (the jump table of EftGndDust_Create and three
 * vector constants).
 */

/* Item init: copies the argument, loads the first key and randomises angle and spin. */
void EftBill_Init(EftZTask *task, EftBillArg *arg) {
    EftBill *w = task->work;
    EftBillDef *def = arg->def;
    EftBillKey *key = &w->key;
    f32 a;

    memset(w, 0, sizeof(EftBill));
    w->arg = *arg;
    w->flags |= EFT_BILL_ALIVE;
    if (arg->life <= 0.0f) {
        w->flags |= EFT_BILL_NO_LIFE;
    } else {
        w->life = arg->life * 30.0f;
    }
    if (def->flags & 1) {
        w->keyTime = def->keyTime * 30.0f;
        w->unkE4 = w->keyTime * def->unk18;
        EftLine_SetKey(key, w, 0);
    } else {
        EftLine_SetKey(key, w, 2);
    }
    w->angle = def->angleRange * RANDF() * 3.14159265f;
    w->spin = def->spinRange * RANDF() * 3.14159265f;
    w->w = key->w;
    w->h = key->h;
    Vec3_Copy(&w->color, &key->color);
    w->color.w = 0.0f;
    a = w->color.w;
    w->fadeInLen = def->fadeIn * 30.0f;
    if (w->fadeInLen <= a) {
        w->flags |= EFT_BILL_FADED_IN;
    }
    w->fadeOutLen = def->fadeOut * 30.0f;
    if (w->fadeOutLen <= a) {
        w->flags |= EFT_BILL_FADED_OUT;
    } else {
        w->fadeOut = w->fadeOutLen;
    }
    if (def->flags & 8) {
        w->frameCount = def->cols * def->rows;
        w->frameTime = def->frameTime * 30.0f;
        w->du = 1.0f / def->cols;
        w->dv = 1.0f / def->rows;
    } else {
        w->u0 = 0.0f;
        w->v1 = w->u1 = w->dv = w->du = 1.0f;
        w->v0 = 0.0f;
    }
    EftLine_SetTex(w, arg->res, arg->unk24, arg->unk24);
}

/* Item term: clears the flags. */
void EftBill_Term(EftZTask *task) {
    EftBill *w = task->work;

    w->flags = 0;
}

/* Item update: spin, size and colour pulses, sheet animation, fades, key animation, life; kills the task when
   the fade-out is over. */
void EftBill_Update(EftZTask *task) {
    EftBill *w = task->work;
    EftBillDef *def = w->arg.def;
    EftBillKey *key;
    f32 r;
    f32 k;

    if (!BtlScene_IsEffectStopped(w->arg.chr, w->type)) {
        if (w->delay <= 0.0f) {
            key = &w->key;
            w->angle += key->spin + w->spin;
            w->angle = EftMath_WrapAngle(w->angle);
            w->w = key->w;
            w->h = key->h;
            if (def->flags & 4) {
                if (key->pulseWTime > 0.0f) {
                    r = w->pulseWTimer / key->pulseWTime;
                    k = key->pulseW + key->pulseWAmp * r;
                    w->w *= k;
                    if (!(w->flags & EFT_BILL_W_DOWN)) {
                        w->pulseWTimer += 1.0f;
                        if (key->pulseWTime <= w->pulseWTimer) {
                            w->pulseWTimer = key->pulseWTime;
                            w->flags |= EFT_BILL_W_DOWN;
                        }
                    } else {
                        w->pulseWTimer -= 1.0f;
                        if (w->pulseWTimer <= 0.0f) {
                            w->pulseWTimer = 0.0f;
                            w->flags &= ~EFT_BILL_W_DOWN;
                        }
                    }
                }
                if (key->pulseHTime > 0.0f) {
                    r = w->pulseHTimer / key->pulseHTime;
                    k = key->pulseH + key->pulseHAmp * r;
                    w->h *= k;
                    if (!(w->flags & EFT_BILL_H_DOWN)) {
                        w->pulseHTimer += 1.0f;
                        if (key->pulseHTime <= w->pulseHTimer) {
                            w->pulseHTimer = key->pulseHTime;
                            w->flags |= EFT_BILL_H_DOWN;
                        }
                    } else {
                        w->pulseHTimer -= 1.0f;
                        if (w->pulseHTimer <= 0.0f) {
                            w->pulseHTimer = 0.0f;
                            w->flags &= ~EFT_BILL_H_DOWN;
                        }
                    }
                }
            }
            w->w *= w->arg.size;
            w->h *= w->arg.size;
            if (def->flags & 8) {
                w->u0 = (f32)(w->frame % def->cols) * w->du;
                w->u1 = w->u0 + w->du;
                w->v0 = (f32)(w->frame / def->cols) * w->dv;
                w->v1 = w->v0 + w->dv;
                w->frameTimer += 1.0f;
                if (w->frameTime <= w->frameTimer) {
                    w->frameTimer -= w->frameTime;
                    w->frame++;
                    if (w->frame >= w->frameCount) {
                        w->frame = 0;
                    }
                }
            }
            Vec4_Copy(&w->color, &key->color);
            if (!(w->flags & EFT_BILL_FADED_IN)) {
                r = w->fadeIn / w->fadeInLen;
                w->fadeIn += 1.0f;
                w->color.w *= r;
                if (w->fadeInLen <= w->fadeIn) {
                    w->flags |= EFT_BILL_FADED_IN;
                    w->color.w = key->color.w;
                }
            }
            if ((w->flags & EFT_BILL_ENDING) && w->endDelay <= 0.0f) {
                if (!(w->flags & EFT_BILL_FADED_OUT)) {
                    r = w->fadeOut / w->fadeOutLen;
                    w->fadeOut -= 1.0f;
                    w->color.w *= r;
                    if (w->fadeOut <= 0.0f) {
                        w->color.w = 0.0f;
                        w->flags |= EFT_BILL_FADED_OUT;
                    }
                } else {
                    w->color.w = 0.0f;
                }
            }
            if ((def->flags & 0x10) && key->pulseColTime > 0.0f) {
                r = w->pulseColTimer / key->pulseColTime;
                k = key->pulseCol[0] + key->pulseColAmp[0] * r;
                w->color.x *= k;
                k = key->pulseCol[1] + key->pulseColAmp[1] * r;
                w->color.y *= k;
                k = key->pulseCol[2] + key->pulseColAmp[2] * r;
                w->color.z *= k;
                if (!(w->flags & EFT_BILL_COL_DOWN)) {
                    w->pulseColTimer += 1.0f;
                    if (key->pulseColTime <= w->pulseColTimer) {
                        w->pulseColTimer = key->pulseColTime;
                        w->flags |= EFT_BILL_COL_DOWN;
                    }
                } else {
                    w->pulseColTimer -= 1.0f;
                    if (w->pulseColTimer <= 0.0f) {
                        w->pulseColTimer = 0.0f;
                        w->flags &= ~EFT_BILL_COL_DOWN;
                    }
                }
            }
            if (w->color.w <= 0.0f || w->w <= 0.0f || w->h <= 0.0f) {
                w->flags &= ~EFT_BILL_VISIBLE;
            } else {
                w->flags |= EFT_BILL_VISIBLE;
            }
            if ((def->flags & 1) && w->keyTimer <= w->keyTime) {
                EftLine_Animate(w);
                w->keyTimer += 1.0f;
                if (w->keyTime <= w->keyTimer) {
                    EftLine_SetKey(key, w, 2);
                }
            }
            if (!(w->flags & EFT_BILL_NO_LIFE)) {
                w->life -= 1.0f;
                if (w->life <= 0.0f) {
                    w->flags |= EFT_BILL_ENDING;
                    if (w->unkD8 > 0.0f) {
                        w->flags |= EFT_BILL_UNK800;
                    }
                }
            }
        } else {
            w->delay -= 1.0f;
        }
        if (w->flags & EFT_BILL_ENDING) {
            if (w->endDelay > 0.0f) {
                w->endDelay -= 1.0f;
            }
            if (w->endDelay <= 0.0f && w->color.w <= 0.0f) {
                w->flags |= EFT_BILL_DEAD;
            }
        }
    }
    if (w->flags & EFT_BILL_DEAD) {
        BtlTask_SetDead(task);
    } else {
        EftLine_LoadTex(w, w);
    }
}

/* Item post-update: nothing. */
void EftBill_PostUpdate(EftZTask *task) {
}

/* Item reset: kills the task. */
void EftBill_Reset(EftZTask *task) {
    BtlTask_SetDead(task);
}

/* Item draw: one sprite facing the camera (or the oriented quads of shape 1). */
void EftBill_Draw(EftZTask *task) {
    EftBill *w = task->work;
    EftBillDef *def = w->arg.def;

    if (BtlScene_IsEffectHidden(w->arg.chr, w->type)) {
        return;
    }
    if ((w->flags & EFT_BILL_FRONT) && !BtlScene_IsCharInView(w->arg.chr)) {
        return;
    }
    if (!(w->flags & EFT_BILL_VISIBLE)) {
        return;
    }
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    switch (def->shape) {
    case 0:
        if (def->flags & 2) {
            EftPrim_DrawQuadDepth(V(&w->arg.pos), &w->color, def->blend, w->w * 0.0625f, w->h * 0.0625f, w->u0, w->v0, w->u1, w->v1, w->angle, w->flags & EFT_BILL_FRONT, w->tex);
        } else {
            EftGfx_DrawSprite(V(&w->arg.pos), &w->color, w->w, w->h, w->u0, w->v0, w->u1, w->v1, w->angle, def->blend, w->flags & EFT_BILL_FRONT, w->tex);
        }
        break;
    case 1:
        if (def->flags & 2) {
            EftLine_DrawClipped(task);
        } else {
            EftLine_DrawSprite(task);
        }
        break;
    }
    Vu0Cur_Pop();
}

/* Manager init: a 4-byte block nothing reads, and the child list of 40 sprites. */
void EftBillMgr_Init(EftZTask *task) {
    gEftBillUnused = BtlPool_Alloc(BtlPool_GetCurrent(), 4);
    *gEftBillUnused = 0;
    gEftBillList = BtlTask_CreateChildList(task, 40, sizeof(EftBill));
}

/* Manager term. */
void EftBillMgr_Term(EftZTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftBillUnused);
    gEftBillUnused = NULL;
}

/* Manager update: nothing. */
void EftBillMgr_Update(EftZTask *task) {
}

/* Manager reset: nothing. */
void EftBillMgr_Reset(EftZTask *task) {
}

/* Creates a sprite from the argument block. */
EftZTask *EftBill_Create(EftBillArg *arg) {
    EftBillArg a = *arg;

    return BtlTaskList_AddTail(gEftBillList, D_002C40F0, &a);
}

/* The sprite's work when `task` is a live sprite task, else NULL. */
static inline EftBill *EftBill_GetWork(EftZTask *task) {
    if (task != NULL && *task->cls == (void *)EftBill_Update) {
        return task->work;
    }
    return NULL;
}

/* Starts the fade-out. */
s32 EftBill_Stop(EftZTask *task) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_BILL_ENDING;
    if (w->unkD8 > 0.0f) {
        w->flags |= EFT_BILL_UNK800;
    }
    return 1;
}

/* Makes the task kill itself at its next update. */
s32 EftBill_Kill(EftZTask *task) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_BILL_DEAD;
    return 1;
}

/* Whether the task is a live sprite. */
s32 EftBill_IsAlive(EftZTask *task) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    return (w->flags & EFT_BILL_ALIVE) ? w->flags != 0 : 0;
}

/* Moves the sprite. */
s32 EftBill_SetPos(EftZTask *task, Vec4 *pos) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    Vec3_Copy(V(&w->arg.pos), pos);
    return 1;
}

/* Sets the direction (normalised). */
s32 EftBill_SetDir(EftZTask *task, Vec4 *dir) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    Vec3_Normalize(V(&w->arg.dir), dir);
    w->arg.dir.w = 1.0f;
    return 1;
}

/* Sets the size factor. */
s32 EftBill_SetSize(EftZTask *task, f32 size) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    w->arg.size = size;
    return 1;
}

/* Sets the frames to wait before the sprite starts. */
s32 EftBill_SetDelay(EftZTask *task, f32 frames) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    w->delay = frames;
    return 1;
}

/* Sets the frames between the stop and the fade-out. */
s32 EftBill_SetEndDelay(EftZTask *task, f32 frames) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    w->endDelay = frames;
    return 1;
}

/* Stores the value that decides flag 0x800 at the stop. */
s32 EftBill_SetUnkD8(EftZTask *task, f32 v) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    w->unkD8 = v;
    return 1;
}

/* Changes the texture (arguments as 0x195038 takes them). */
/* NON-MATCHING: 2 instructions. The original fills the class test's delay slot with `ld ra` (plain bne) and sends the alive test
   straight to `jr ra`; standalone this compiles to a bnel and a branch to the `ld ra`. It prints OK when
   EftLine_SetTex is DEFINED earlier in the same file (tested with a dummy body), so this file and the range before
   it (0x195038..) are one source file: merge and re-diff. */
s32 EftBill_SetTexture(EftZTask *task, s32 a1, s32 a2, s32 a3) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    EftLine_SetTex(w, (void *)a1, a2, a3);
    return 1;
}

/* Draws the sprite in front, and only while its owner is in view. */
s32 EftBill_SetFront(EftZTask *task) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_BILL_FRONT;
    return 1;
}

/* Sets the effect type the stop / hide tests use. */
s32 EftBill_SetType(EftZTask *task, s32 type) {
    EftBill *w;

    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftBill_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BILL_ALIVE)) {
        return 0;
    }
    w->type = type;
    return 1;
}

/* ---- ground dust ---------------------------------------------------------------------------------------- */

/* Adds a dust task of the given kind (0..5) to the manager's list. */
EftZTask *EftGndDust_Create(EftGndDustArg *arg, s32 kind) {
    EftZTask *task = NULL;

    if (gEftGndDust == NULL) {
        return NULL;
    }
    switch (kind) {
    case 0:
        task = BtlTaskList_AddTail(gEftGndDustList, D_002C4120, arg);
        break;
    case 1:
        task = BtlTaskList_AddTail(gEftGndDustList, D_002C4138, arg);
        break;
    case 2:
        task = BtlTaskList_AddTail(gEftGndDustList, D_002C4150, arg);
        break;
    case 3:
        task = BtlTaskList_AddTail(gEftGndDustList, D_002C4168, arg);
        break;
    case 4:
        task = BtlTaskList_AddTail(gEftGndDustList, D_002C4180, arg);
        break;
    case 5:
        task = BtlTaskList_AddTail(gEftGndDustList, D_002C4198, arg);
        break;
    }
    return task;
}

/* Stage debris: a puff of the stage's dust at `pos` (called by BtlStage_UpdateObjs). */
void EftGndDust_SpawnDebris(Vec4 *pos, f32 scale, f32 unused, f32 bright) {
    EftGndDustArg *arg;

    if (gEftGndDust != NULL) {
        arg = EftGndDust_GetTemplate();
        *V(&arg->pos) = *pos;
        arg->scale = scale;
        arg->bright = bright;
        EftGndDust_Create(arg, 0);
    }
}

/* Fighter request 0x30: starts (stop = 0) or stops (stop = 1) the fighter's first continuous emitter, creating
   it the first time. */
void EftGndDust_SetSlide(s32 objId, s32 stop, f32 bright) {
    EftGndDustArg arg;
    EftGndDustEmit *w;

    if (gEftGndDust == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) {
        return;
    }
    if (gEftGndDust->slide[objId] == NULL) {
        memset(&arg, 0, sizeof(arg));
        Vec4_Set(V(&arg.pos), 0.0f, 0.0f, 0.0f, 1.0f);
        Vec4_Set(V(&arg.dir), 0.0f, -1.0f, 0.0f, 1.0f);
        Vec4_Set(V(&arg.accel), 0.0f, 0.0f, 0.0f, 1.0f);
        EftGndDust_GetLightColors(arg.colA, arg.colB);
        arg.chr = objId;
        arg.pool = 1;
        arg.blend = 0;
        arg.tex = 0;
        arg.size = 50;
        arg.life = 15;
        arg.fade = 10;
        arg.spin = 0;
        arg.rMin = 0;
        arg.rMax = 0;
        arg.grow = 0.1f;
        arg.bright = bright;
        arg.speed = 1.1f;
        arg.unk54 = 0.0f;
        arg.drag = 1.0f;
        arg.unk5C = 0.0f;
        arg.scale = BtlScene_GetCharScale(objId);
        gEftGndDust->slide[objId] = EftGndDust_Create(&arg, 1);
        if (gEftGndDust->slide[objId] == NULL) {
            return;
        }
    }
    w = gEftGndDust->slide[objId]->work;
    switch (stop) {
    case 0:
        w->flags |= EFT_GDUST_EMIT;
        w->flags &= ~EFT_GDUST_FORCE_DROP;
        break;
    case 1:
        w->flags &= ~EFT_GDUST_EMIT;
        w->flags |= EFT_GDUST_FORCE_DROP;
        break;
    }
}

/* Fighter request 0x33: the same for the second continuous emitter. */
void EftGndDust_SetDash(s32 objId, s32 stop, f32 bright) {
    EftGndDustArg arg;
    EftGndDustEmit *w;

    if (gEftGndDust == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) {
        return;
    }
    if (gEftGndDust->dash[objId] == NULL) {
        memset(&arg, 0, sizeof(arg));
        Vec4_Set(V(&arg.pos), 0.0f, 0.0f, 0.0f, 1.0f);
        Vec4_Set(V(&arg.dir), 0.0f, -1.0f, 0.0f, 1.0f);
        Vec4_Set(V(&arg.accel), 0.0f, -1.0f, 0.0f, 1.0f);
        EftGndDust_GetLightColors(arg.colA, arg.colB);
        arg.chr = objId;
        arg.pool = 1;
        arg.blend = 0;
        arg.tex = 0;
        arg.size = 100;
        arg.life = 25;
        arg.fade = 10;
        arg.spin = 2;
        arg.rMin = 0;
        arg.rMax = 0;
        arg.grow = 0.05f;
        arg.bright = bright;
        arg.speed = 1.1f;
        arg.unk54 = 0.01f;
        arg.drag = 1.0f;
        arg.unk5C = 0.005f;
        arg.scale = BtlScene_GetCharScale(objId);
        gEftGndDust->dash[objId] = EftGndDust_Create(&arg, 2);
        if (gEftGndDust->dash[objId] == NULL) {
            return;
        }
    }
    w = gEftGndDust->dash[objId]->work;
    switch (stop) {
    case 0:
        w->flags |= EFT_GDUST_EMIT;
        break;
    case 1:
        w->flags &= ~EFT_GDUST_EMIT;
        break;
    }
}

/* Fighter request 0x34 (and 0x35 when the ground point is under water): one burst at the fighter. */
void EftGndDust_SpawnBurst(s32 objId, f32 scale, f32 bright) {
    EftGndDustArg arg;

    if (gEftGndDust != NULL) {
        memset(&arg, 0, sizeof(arg));
        Vec4_Set(V(&arg.pos), 0.0f, 0.0f, 0.0f, 1.0f);
        Vec4_Set(V(&arg.dir), 0.0f, -1.0f, 0.0f, 1.0f);
        Vec4_Set(V(&arg.accel), 0.0f, -1.0f, 0.0f, 1.0f);
        EftGndDust_GetLightColors(arg.colA, arg.colB);
        arg.chr = objId;
        arg.pool = 1;
        arg.blend = 0;
        arg.tex = 0;
        arg.size = 20;
        arg.life = 15;
        arg.fade = 10;
        arg.spin = 2;
        arg.rMin = 0;
        arg.rMax = 0;
        arg.bright = bright;
        arg.scale = BtlScene_GetCharScale(objId);
        if (1.0f < arg.scale) {
            arg.scale = 1.0f;
        }
        arg.scale *= scale;
        EftGndDust_Create(&arg, 3);
    }
}

/* Fighter request 0x35: a ring of dust at a ground point. The normal is not used. */
void EftGndDust_SpawnLanding(s32 objId, Vec4 *pos, Vec4 *normal, f32 bright) {
    EftGndDustArg arg;
    EftZVec up = { { 0.0f, -1.0f, 0.0f, 1.0f } };

    if (gEftGndDust != NULL) {
        memset(&arg, 0, sizeof(arg));
        Vec4_Copy(V(&arg.pos), pos);
        Vec4_Copy(V(&arg.dir), V(&up));
        arg.bright = bright;
        arg.chr = objId;
        arg.scale = 1.0f;
        EftGndDust_Create(&arg, 4);
    }
}

/* The same with a scale (EftShotTech_UpdateTargetBurst, eft_k.c). The direction is not used. */
void EftGndDust_SpawnLandingScaled(s32 objId, Vec4 *pos, Vec4 *dir, f32 bright, f32 scale) {
    EftGndDustArg arg;
    EftZVec up = { { 0.0f, -1.0f, 0.0f, 1.0f } };

    if (gEftGndDust != NULL) {
        memset(&arg, 0, sizeof(arg));
        Vec4_Copy(V(&arg.pos), pos);
        Vec4_Copy(V(&arg.dir), V(&up));
        arg.bright = bright;
        arg.chr = objId;
        arg.scale = scale;
        EftGndDust_Create(&arg, 4);
    }
}

/* Fighter request 0x31: nothing. */
void EftGndDust_Stub(s32 objId, f32 scale) {
}

/* Two small dust particles at a ground point: a projectile that hit the ground (EftHit_SpawnBlastImpact), and each
   foot of a fighter with request 0x32 (BtlFx_UpdateGroundFx). */
void EftGndDust_SpawnImpact(s32 objId, Vec4 *pos, f32 bright) {
    EftGndDustArg arg;
    EftZVec up = { { 0.0f, -1.0f, 0.0f, 1.0f } };

    if (gEftGndDust != NULL) {
        memset(&arg, 0, sizeof(arg));
        Vec4_Copy(V(&arg.pos), pos);
        Vec4_Copy(V(&arg.dir), V(&up));
        arg.bright = bright;
        arg.chr = objId;
        EftGndDust_Create(&arg, 5);
    }
}

/* The stage debris template, NULL before the manager exists. */
EftGndDustArg *EftGndDust_GetTemplate(void) {
    if (gEftGndDust != NULL) {
        return &gEftGndDust->tmpl;
    }
    return NULL;
}

/* Fills the debris template with defaults, then with the stage's dust record when it has one. */
void EftGndDust_InitTemplate(void) {
    EftGndDustArg *t = &gEftGndDust->tmpl;
    EftGndDustStage *stg;

    if (gEftGndDust != NULL) {
        Vec4_Set(V(&t->pos), 0.0f, -100.0f, 0.0f, 1.0f);
        Vec4_Set(V(&t->dir), 0.0f, -1.0f, 0.0f, 1.0f);
        Vec4_Set(V(&t->accel), 0.0f, 1.0f, 0.0f, 1.0f);
        /* The order of these stores was found by search: the compiler's scheduler reorders them, and only
           some source orders give the original sequence. */
        t->colA[0] = 80;
        t->colA[3] = 80;
        t->colA[2] = 35;
        t->pool = 0;
        t->unk54 = 0.0f;
        t->colB[0] = 80;
        t->colA[1] = 55;
        t->colB[1] = 65;
        t->colB[2] = 45;
        t->colB[3] = 30;
        t->blend = 0;
        t->tex = 0;
        t->chr = -1;
        t->speed = 0.5f;
        t->drag = 0.9f;
        t->spin = 0;
        t->size = 500;
        t->grow = 0.01f;
        t->life = 30;
        t->bright = 1.0f;
        t->fade = 30;
        t->unk5C = 0.0f;
        t->rMin = 17;
        t->rMax = 40;
        t->scale = 1.0f;
        stg = BtlStage_GetList68();
        if (stg->enabled != 0) {
            t->colA[0] = stg->colA[0];
            t->colA[1] = stg->colA[1];
            t->colA[2] = stg->colA[2];
            t->colA[3] = stg->colA[3];
            t->colB[0] = stg->colB[0];
            t->colB[1] = stg->colB[1];
            t->colB[2] = stg->colB[2];
            t->colB[3] = stg->colB[3];
            t->speed = stg->speed;
            t->unk54 = stg->unk54;
            t->size = stg->size;
            t->rMin = stg->rMin;
            t->rMax = stg->rMax;
        }
    }
}

/* Manager init: the work, both particle pools, the texture (common entry 0xB) and the task list. */
void EftGndDustMgr_Init(EftZTask *task) {
    s32 i;

    gEftGndDust = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftGndDustWork));
    memset(gEftGndDust, 0, sizeof(EftGndDustWork));
    gEftGndDust->pool[0] = gEftGndDust->partsA;
    List_Init(&gEftGndDust->free[0]);
    for (i = 0; i < 64; i++) {
        List_PushBack(&gEftGndDust->free[0], &gEftGndDust->pool[0][i].link);
    }
    gEftGndDust->pool[1] = gEftGndDust->partsB;
    List_Init(&gEftGndDust->free[1]);
    for (i = 0; i < 63; i++) {
        List_PushBack(&gEftGndDust->free[1], &gEftGndDust->pool[1][i].link);
    }
    EftTexSet_Load8(&gEftGndDust->tex, BtlScene_GetCommonEntry(0xB));
    gEftGndDust->texPtr = &gEftGndDust->tex;
    /* Two persistent emitters per actor, plus twelve short-lived bursts. */
    gEftGndDustList = BtlTask_CreateChildList(task, 2 * EFT_FIGHTER_CAPACITY + 12, sizeof(EftGndDustEmit));
    EftGndDust_InitTemplate();
}

/* Manager term: frees the work. */
void EftGndDustMgr_Term(EftZTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftGndDust);
    gEftGndDust = NULL;
}

/* Manager update: clears a field of the texture. */
void EftGndDustMgr_Update(EftZTask *task) {
    if (gEftGndDust != NULL) {
        gEftGndDust->texPtr->unk84 = 0;
    }
}

/* Kind 0 init: four particles at random points around the position, flying outwards. */
void EftGndDustPuff_Init(EftZTask *task, EftGndDustArg *arg) {
    EftGndDustEmit *w = task->work;
    EftGndDustPart *p;
    s32 i;
    f32 rMin;
    f32 rMax;
    f32 spin;

    memset(w, 0, sizeof(EftGndDustEmit));
    w->arg = *arg;
    List_Init(&w->parts);
    w->free = EftGndDust_GetFreeList(arg->pool);
    for (i = 0; i < 4; i++) {
        p = EftGndDust_AllocPart(&w->parts, w->free);
        if (p == NULL) {
            return;
        }
        Vec4_Copy(&p->pos, V(&arg->pos));
        Vec4_Copy(&p->dir, V(&arg->dir));
        Vec4_Set(&p->color, 128.0f, 128.0f, 128.0f, 128.0f);
        Vec4_Set(&p->base, 0.0f, 0.0f, 0.0f, 1.0f);
        Vec4_Set(&p->size, 1.0f, 1.0f, 1.0f, 1.0f);
        Vec4_Set(&p->grow, arg->grow, arg->grow, 1.0f, 1.0f);
        Vec3_Scale(&p->accel, V(&arg->accel), arg->unk5C * arg->scale + arg->unk54);
        *(EftZCol *)p->colA = *(EftZCol *)arg->colA;
        *(EftZCol *)p->colB = *(EftZCol *)arg->colB;
        p->dir.x = Rand_Float01() * 2.0f - 1.0f;
        p->dir.y = Rand_Float01() * 2.0f - 1.0f;
        p->dir.z = Rand_Float01() * 2.0f - 1.0f;
        p->dir.w = 1.0f;
        Vec3_Normalize(&p->dir, &p->dir);
        rMin = arg->rMin * arg->scale;
        rMax = arg->rMax * arg->scale;
        p->pos.x = arg->pos.x + p->dir.x * (rMin + Rand_Float01() * (rMax - rMin));
        p->pos.y = arg->pos.y + p->dir.y * (rMin + Rand_Float01() * (rMax - rMin));
        p->pos.z = arg->pos.z + p->dir.z * (rMin + Rand_Float01() * (rMax - rMin));
        p->pos.w = 1.0f;
        p->speed = arg->speed + arg->unk54 * Rand_Float01();
        p->drag = arg->drag;
        p->lifeMax = p->life = (f32)arg->life;
        p->fadeLen = p->fade = (f32)arg->fade;
        p->alpha = 1.0f;
        p->growDamp = 1.0f;
        p->angle = 0.0f;
        p->scale = arg->size;
        spin = arg->spin * 0.1f;
        p->spin = spin;
        p->spin = Rand_Float01() > 0.5f ? -spin : spin;
        Vec3_Scale(&p->vel, &p->dir, p->speed);
        p->flags = EFT_GDUST_PART_ALIVE;
        gEftGndDust->count++;
    }
}

/* Kind 0 term. */
void EftGndDustPuff_Term(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    EftGndDust_FreeParts(w);
}

/* Kind 0 reset: kills the task. */
void EftGndDustPuff_Reset(EftZTask *task) {
    BtlTask_SetDead(task);
}

/* Kind 0 update: steps the particles; the task ends with its last particle. */
void EftGndDustPuff_Update(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    if (!BtlScene_IsTimeStopped()) {
        EftGndDust_UpdateParts(w);
    }
    if (List_GetHead(&w->parts) != NULL) {
        w->tex = EftGndDust_GetTex(gEftGndDust->texPtr, w->arg.tex);
    } else {
        BtlTask_SetDead(task);
    }
}

/* Kind 0 draw: one sprite per particle. */
void EftGndDustPuff_Draw(EftZTask *task) {
    EftGndDustEmit *w = task->work;
    EftGndDustPart *p = (EftGndDustPart *)List_GetHead(&w->parts);
    Vec4 color;
    Vec4 pos;

    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    while (p != NULL) {
        Vec4_Set(&color, p->color.x * w->arg.bright, p->color.y * w->arg.bright, p->color.z * w->arg.bright,
                 p->color.w * p->alpha);
        Vec3_Add(&pos, &p->pos, &p->base);
        EftGfx_DrawSprite(&pos, &color, (f32)p->scale * w->arg.scale * p->size.x, (f32)p->scale * w->arg.scale * p->size.y, 0.0f, 0.0f, 1.0f, 1.0f, p->angle, w->arg.blend, 0, w->tex);
        p = (EftGndDustPart *)List_GetNext(&p->link);
    }
    Vu0Cur_Pop();
}

/* Kind 1 init: binds to the fighter and emits the first particles. */
void EftGndDustSlide_Init(EftZTask *task, EftGndDustArg *arg) {
    EftGndDustEmit *w = task->work;

    memset(w, 0, sizeof(EftGndDustEmit));
    w->arg = *arg;
    List_Init(&w->parts);
    w->free = EftGndDust_GetFreeList(arg->pool);
    EftGndDust_SpawnPiece(w, arg, 0x30, 1.0f, 0.0f);
    BtlCharApi_GetPos(arg->chr, &w->pos);
    BtlCharApi_GetDir(arg->chr, &w->dir);
    Vec3_Scale(&w->dir, &w->dir, -1.0f);
    Vec3_Normalize(&w->dir, &w->dir);
}

/* Kind 1 term: frees the particles and the fighter's slot. */
void EftGndDustSlide_Term(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    EftGndDust_FreeParts(w);
    gEftGndDust->slide[w->arg.chr] = NULL;
}

/* Kind 1 reset: kills the task. */
void EftGndDustSlide_Reset(EftZTask *task) {
    BtlTask_SetDead(task);
}

/* Kind 1 update: follows the fighter, emits every 5th frame while switched on, and leaves the particles
   behind when the fighter reverses or the emitter was stopped. Never ends by itself. */
void EftGndDustSlide_Update(EftZTask *task) {
    s32 period = 5;
    EftGndDustEmit *w = task->work;
    Vec4 move;

    if (!BtlScene_IsTimeStopped()) {
        BtlCharApi_GetPos(w->arg.chr, &w->pos);
        BtlCharApi_GetDir(w->arg.chr, &w->dir);
        Vec3_Scale(&w->dir, &w->dir, -1.0f);
        Vec3_Normalize(&w->dir, &w->dir);
        w->arg.scale = BtlScene_GetCharScale(w->arg.chr);
        if (2.3f < w->arg.scale) {
            w->arg.scale = 2.3f;
        }
        if (w->arg.scale < 0.8f) {
            w->arg.scale = 0.8f;
        }
        if (w->flags & EFT_GDUST_EMIT) {
            w->tick++;
            if (w->tick % period == 0) {
                EftGndDust_SpawnPiece(w, &w->arg, 0x30, 1.0f, 0.0f);
                EftGndDust_SpawnChip(w, &w->arg, &w->dir, w->arg.colA, w->arg.colB, 5, 10);
            }
        }
        BtlCharApi_GetFrameMove(w->arg.chr, &move);
        Vec3_Normalize(&move, &move);
        if (Vec3_Dot(&w->move, &move) < 0.0f || (w->flags & EFT_GDUST_FORCE_DROP)) {
            w->flags |= EFT_GDUST_DROP;
            w->partFlags = EFT_GDUST_PART_FOLLOW;
        } else {
            w->flags &= ~EFT_GDUST_DROP;
        }
        Vec4_Copy(&w->move, &move);
        EftGndDust_UpdateParts(w);
    }
    if (List_GetHead(&w->parts) != NULL) {
        w->tex = EftGndDust_GetTex(gEftGndDust->texPtr, 0);
        w->texFar = EftGndDust_GetTex(gEftGndDust->texPtr, 2);
    }
}

/* Kind 1 draw: oriented quads. */
void EftGndDustSlide_Draw(EftZTask *task) {
    EftGndDustEmit *w = task->work;
    EftGndDustPart *p = (EftGndDustPart *)List_GetHead(&w->parts);
    Vec4 color;
    Vec4 pos;
    s32 far;

    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    while (p != NULL) {
        far = 0;
        if (p->flags & EFT_GDUST_PART_FAR) {
            far = 1;
        }
        Vec4_Set(&color, p->color.x * w->arg.bright, p->color.y * w->arg.bright, p->color.z * w->arg.bright,
                 p->color.w * p->alpha);
        Vec3_Add(&pos, &p->pos, &p->base);
        EftGndDust_DrawPiece(&pos, &color, &p->dir, w->arg.blend, (f32)p->scale * w->arg.scale * p->size.x, (f32)p->scale * w->arg.scale * p->size.y, p->angle, far ? w->texFar : w->tex, far);
        p = (EftGndDustPart *)List_GetNext(&p->link);
    }
    Vu0Cur_Pop();
}

/* Kind 2 init: binds to the fighter and emits the first particles at a random angle. */
void EftGndDustDash_Init(EftZTask *task, EftGndDustArg *arg) {
    EftGndDustEmit *w = task->work;

    memset(w, 0, sizeof(EftGndDustEmit));
    w->arg = *arg;
    List_Init(&w->parts);
    w->free = EftGndDust_GetFreeList(arg->pool);
    BtlCharApi_GetPos(arg->chr, &w->pos);
    BtlCharApi_GetDir(arg->chr, &w->dir);
    Vec3_Scale(&w->dir, &w->dir, -1.0f);
    Vec3_Normalize(&w->dir, &w->dir);
    Vec4_Copy(V(&arg->dir), &w->dir);
    EftGndDust_SpawnPiece(w, arg, 0x10, 1.0f, RANDF() * 3.14159265f);
}

/* Kind 2 term: frees the particles and the fighter's slot. */
void EftGndDustDash_Term(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    EftGndDust_FreeParts(w);
    gEftGndDust->dash[w->arg.chr] = NULL;
}

/* Kind 2 reset: kills the task. */
void EftGndDustDash_Reset(EftZTask *task) {
    BtlTask_SetDead(task);
}

/* Kind 2 update: follows the fighter, emits every 6th frame while switched on. Never ends by itself. */
void EftGndDustDash_Update(EftZTask *task) {
    s32 period = 6;
    EftGndDustEmit *w = task->work;
    Vec4 move;

    if (!BtlScene_IsTimeStopped()) {
        BtlCharApi_GetPos(w->arg.chr, &w->pos);
        BtlCharApi_GetDir(w->arg.chr, &w->dir);
        Vec3_Scale(&w->dir, &w->dir, -1.0f);
        Vec3_Normalize(&w->dir, &w->dir);
        w->arg.scale = BtlScene_GetCharScale(w->arg.chr);
        if (2.3f < w->arg.scale) {
            w->arg.scale = 2.3f;
        }
        if (w->arg.scale < 0.8f) {
            w->arg.scale = 0.8f;
        }
        if (w->flags & EFT_GDUST_EMIT) {
            w->tick++;
            if (w->tick % period == 0) {
                Vec4_Copy(V(&w->arg.dir), &w->dir);
                EftGndDust_SpawnPiece(w, &w->arg, 0x10, 1.0f, RANDF() * 3.14159265f);
            }
        }
        BtlCharApi_GetFrameMove(w->arg.chr, &move);
        Vec3_Normalize(&move, &move);
        if (Vec3_Dot(&w->move, &move) < 0.0f) {
            w->partFlags = EFT_GDUST_PART_FOLLOW;
            w->flags |= EFT_GDUST_DROP;
        } else {
            w->flags &= ~EFT_GDUST_DROP;
        }
        Vec4_Copy(&w->move, &move);
        EftGndDust_UpdateParts(w);
    }
    if (List_GetHead(&w->parts) != NULL) {
        w->tex = EftGndDust_GetTex(gEftGndDust->texPtr, w->arg.tex);
    }
}

/* Kind 2 draw: one sprite per particle. */
void EftGndDustDash_Draw(EftZTask *task) {
    EftGndDustEmit *w = task->work;
    EftGndDustPart *p = (EftGndDustPart *)List_GetHead(&w->parts);
    Vec4 color;
    Vec4 pos;

    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    while (p != NULL) {
        Vec4_Set(&color, p->color.x * w->arg.bright, p->color.y * w->arg.bright, p->color.z * w->arg.bright,
                 p->color.w * p->alpha);
        Vec3_Add(&pos, &p->pos, &p->base);
        EftGfx_DrawSprite(&pos, &color, (f32)p->scale * w->arg.scale * p->size.x, (f32)p->scale * w->arg.scale * p->size.y, 0.0f, 0.0f, 1.0f, 1.0f, p->angle, w->arg.blend, 0, w->tex);
        p = (EftGndDustPart *)List_GetNext(&p->link);
    }
    Vu0Cur_Pop();
}

/* Kind 3 init: one burst (0x19A548). */
void EftGndDustBurst_Init(EftZTask *task, EftGndDustArg *arg) {
    EftGndDustEmit *w = task->work;

    memset(w, 0, sizeof(EftGndDustEmit));
    w->arg = *arg;
    List_Init(&w->parts);
    w->free = EftGndDust_GetFreeList(arg->pool);
    EftGndDust_SpawnBodyDust(w, &w->arg, 1.0f, 1.0f);
}

/* Kind 3 term. */
void EftGndDustBurst_Term(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    EftGndDust_FreeParts(w);
}

/* Kind 3 reset: kills the task. */
void EftGndDustBurst_Reset(EftZTask *task) {
    BtlTask_SetDead(task);
}

/* Kind 3 update: follows the fighter; the task ends with its last particle. */
void EftGndDustBurst_Update(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    if (!BtlScene_IsTimeStopped()) {
        BtlCharApi_GetPos(w->arg.chr, &w->pos);
        BtlCharApi_GetDir(w->arg.chr, &w->dir);
        Vec3_Scale(&w->dir, &w->dir, -1.0f);
        Vec3_Normalize(&w->dir, &w->dir);
        EftGndDust_UpdateParts(w);
    }
    if (List_GetHead(&w->parts) != NULL) {
        w->tex = EftGndDust_GetTex(gEftGndDust->texPtr, w->arg.tex);
    } else {
        BtlTask_SetDead(task);
    }
}

/* Kind 3 draw: oriented quads without a direction. */
void EftGndDustBurst_Draw(EftZTask *task) {
    EftGndDustEmit *w = task->work;
    EftGndDustPart *p;
    Vec4 color;
    Vec4 pos;
    Vec4 dir;

    memset(&dir, 0, sizeof(dir));
    dir.w = 1.0f;
    p = (EftGndDustPart *)List_GetHead(&w->parts);
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    while (p != NULL) {
        Vec4_Set(&color, p->color.x * w->arg.bright, p->color.y * w->arg.bright, p->color.z * w->arg.bright,
                 p->color.w * p->alpha);
        Vec3_Add(&pos, &p->pos, &p->base);
        EftGndDust_DrawPiece(&pos, &color, &dir, w->arg.blend, (f32)p->scale * w->arg.scale * p->size.x, (f32)p->scale * w->arg.scale * p->size.y, p->angle, w->tex, (p->flags >> 6) & 1);
        p = (EftGndDustPart *)List_GetNext(&p->link);
    }
    Vu0Cur_Pop();
}
