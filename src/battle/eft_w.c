#include "common.h"
#include "sys/gfx_ot.h"
#include "battle/eft_w.h"

#ifdef PORT
extern f32 Port_WideFactor(void); /* widescreen on PC: 1, or 4/3 for a 16:9 picture (port/src/plat_stub.c) */
#endif

/*
 * Effect tasks, 0x1895E8..0x18D618. Two sprite particle modules of the effect pack library (EftEmit_*, eft_h.c /
 * eft_i.c); see include/battle/eft_w.h. Everything here is presentation: no function touches a fighter, a hit
 * record, a battle object or the stage.
 *
 *   0x1895E8..0x18C190  EftLink_*: the second half of effect pack part kind 15 (first half: eft_v_c.c, which owns
 *                       the manager gEftLink, the task class gEftLinkClass and the Init / Update / Draw
 *                       callbacks). Here: the four sprite draw routines, the key copy, the texture pair, the
 *                       sprite pool (200), sprite init / step / corners, and the handle API the effect pack
 *                       library calls (EftEmit_SpawnType15 in eft_i.c).
 *   0x18C190..0x18D618  EftPart10*: the first half of effect pack part kind 10 (second half: eft_x.c). Here: the
 *                       manager class (0x2C3F68), the six callbacks of the emitter class (0x2C3F80), ring
 *                       emission, the particle step and the ring pool (150).
 */

extern void *memset(void *dst, s32 c, u32 n);
/* memset reached as a plain call (the compiler would expand a small constant size in place). */
extern void *EftW_MemsetCall(void *dst, s32 c, u32 n) __asm__("memset");
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern EftWTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void EftLink_Update(EftWTask *task);
/* One vertex as ClipVtx_Set builds it for the EftGfx_DrawPoly functions. */
typedef struct EftWVert {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 uv;
    /* 0x20 */ Vec4 color;
} EftWVert; /* 0x30 */

/* GS registers of the packets queued here. */
typedef struct EftWXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftWXyzf;

typedef struct EftWRgbaq {
    u8 r, g, b, a;
    f32 q;
} EftWRgbaq;

typedef struct EftWSt {
    f32 s, t;
} EftWSt;

/* DMA tag + REGLIST GIF tag: PRIM, TEX0, four (RGBAQ, ST, XYZF2): a gouraud textured strip. */
typedef struct EftWStripPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000008 */
    /* 0x04 */ struct EftWStripPkt *next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;         /* 0x50000008 */
    /* 0x10 */ u64 gifTag;       /* 0xE400000000008001 */
    /* 0x18 */ u64 regs;         /* 0x42142142142160 */
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    struct {
        /* 0x00 */ EftWRgbaq rgbaq;
        /* 0x08 */ EftWSt st;
        /* 0x10 */ EftWXyzf xyz;
    } v[4];
} EftWStripPkt; /* 0x90 */

/* DMA tag + REGLIST GIF tag: PRIM, TEX0, RGBAQ, four (ST, XYZF2), NOP: a flat textured strip. */
typedef struct EftWQuadPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000007 */
    /* 0x04 */ struct EftWQuadPkt *next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;         /* 0x50000007 */
    /* 0x10 */ u64 gifTag;       /* 0xC400000000008001 */
    /* 0x18 */ u64 regs;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftWRgbaq rgbaq;
    /* 0x38 */ EftWSt st0;
    /* 0x40 */ EftWXyzf xyz0;
    /* 0x48 */ EftWSt st1;
    /* 0x50 */ EftWXyzf xyz1;
    /* 0x58 */ EftWSt st2;
    /* 0x60 */ EftWXyzf xyz2;
    /* 0x68 */ EftWSt st3;
    /* 0x70 */ EftWXyzf xyz3;
    /* 0x78 */ u64 nop;
} EftWQuadPkt; /* 0x80 */

#define V(p) ((Vec4 *)(p))

/* TEX0 of entry i of a texture table (explicit shift: the original operand order). */
#define EFTW_TEX0(tex, i) (*(u64 *)((u32)(tex) + ((i) << 4)))

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern s32 Vu0Cur_ProjectPointsStq(EftWIVec *scr, Vec4 *stq, Vec4 *pos, Vec4 *uv, s32 n);
extern void Vec4_ToInt(EftWIVec *dst, Vec4 *src);
extern void ClipVtx_Set(EftWVert *dst, Vec4 *pos, Vec4 *uv, Vec4 *color);
extern s32 Vu0Cur_ProjectPoint(EftWIVec *scr, Vec4 *pos);
extern void Vec4_Mul(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Mtx_Translate(Mtx44 *dst, Mtx44 *src, Vec4 *scale);
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);
extern void EftGfx_DrawPolyScaledZ(EftWVert *v, s32 layer, s32 a2, s32 a3, s32 noDepth, s32 a5, u64 tex0, f32 zScale);
extern void EftGfx_DrawPolyFixedZ(EftWVert *v, s32 layer, s32 a2, s32 a3, s32 noDepth, s32 a5, u64 tex0, s32 z);
s32 EftLink_IsCornerOffScreen(s32 x, s32 y, s32 z);

extern EftWLinkMgr *gEftLink;
extern EftWView *gBtlCamView;

/* Queues a sprite as a textured strip through its four world-space corners (projected with the loaded
   matrix; dropped when the projection clips), at the average depth of the corners. */
void EftLink_DrawQuad(Vec4 *corner, EftWVec uv0, EftWVec uv1, EftWVec color, s32 layer, s32 texIdx, s32 noDepth,
                      EftWTexEntry *tex) {
    Vec4 uv[4];
    Vec4 stq[4];
    EftWIVec scr[4];
    EftWIVec col;
    EftWStripPkt *p;
    OtEntry *e;
    s32 abe = 1;
    s32 ctx;
    s32 z;
    s32 l;

    Vec4_Set(&uv[0], uv0.x, uv0.y, 1.0f, 1.0f);
    Vec4_Set(&uv[1], uv0.z, uv0.w, 1.0f, 1.0f);
    Vec4_Set(&uv[2], uv1.x, uv1.y, 1.0f, 1.0f);
    Vec4_Set(&uv[3], uv1.z, uv1.w, 1.0f, 1.0f);
    if (!Vu0Cur_ProjectPointsStq(scr, stq, corner, uv, 4)) {
        return;
    }
    p = (EftWStripPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    if (p == NULL) {
        return;
    }
    ctx = layer >= 2;
    p->prim = ((u64)abe << 6) | ((u64)ctx << 9) | 0x1C;
    p->dmaTag = 0x20000008;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000008;
    p->gifTag = 0xE400000000008001;
    p->regs = 0x42142142142160 + (ctx << 4);
    p->next = NULL;
    z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
    if (noDepth) {
        scr[0].z = 0xFFFFFF;
        scr[1].z = 0xFFFFFF;
        scr[2].z = 0xFFFFFF;
        scr[3].z = 0xFFFFFF;
    }
    Vec4_ToInt(&col, V(&color));
    p->v[0].rgbaq.r = col.x;
    p->v[0].rgbaq.g = col.y;
    p->v[0].rgbaq.b = col.z;
    p->v[0].rgbaq.a = col.w;
    p->v[0].rgbaq.q = stq[0].z;
    p->v[1].rgbaq.r = col.x;
    p->v[1].rgbaq.g = col.y;
    p->v[1].rgbaq.b = col.z;
    p->v[1].rgbaq.a = col.w;
    p->v[1].rgbaq.q = stq[1].z;
    p->v[2].rgbaq.r = col.x;
    p->v[2].rgbaq.g = col.y;
    p->v[2].rgbaq.b = col.z;
    p->v[2].rgbaq.a = col.w;
    p->v[2].rgbaq.q = stq[2].z;
    p->v[3].rgbaq.r = col.x;
    p->v[3].rgbaq.g = col.y;
    p->v[3].rgbaq.b = col.z;
    p->v[3].rgbaq.a = col.w;
    p->v[3].rgbaq.q = stq[3].z;
    p->v[0].st.s = stq[0].x;
    p->v[0].st.t = stq[0].y;
    p->v[1].st.s = stq[1].x;
    p->v[1].st.t = stq[1].y;
    p->v[2].st.s = stq[2].x;
    p->v[2].st.t = stq[2].y;
    p->v[3].st.s = stq[3].x;
    p->v[3].st.t = stq[3].y;
    p->v[0].xyz.x = scr[0].x;
    p->v[0].xyz.y = scr[0].y;
    p->v[0].xyz.z = scr[0].z;
    p->v[0].xyz.f = 0xFF;
    p->v[1].xyz.x = scr[1].x;
    p->v[1].xyz.y = scr[1].y;
    p->v[1].xyz.z = scr[1].z;
    p->v[1].xyz.f = 0xFF;
    p->v[2].xyz.x = scr[2].x;
    p->v[2].xyz.y = scr[2].y;
    p->v[2].xyz.z = scr[2].z;
    p->v[2].xyz.f = 0xFF;
    p->v[3].xyz.x = scr[3].x;
    p->v[3].xyz.y = scr[3].y;
    p->v[3].xyz.z = scr[3].z;
    p->v[3].xyz.f = 0xFF;
    p->tex0 = EFTW_TEX0(tex, texIdx);
    l = layer;
    if (l >= 2) {
        l -= 2;
    }
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


/* The same quad through the clipping polygon path: two triangles (corners 0 1 2 and 1 2 3). */
void EftLink_DrawQuadClipped(Vec4 *corner, EftWVec uv0, EftWVec uv1, EftWVec color, s32 layer, s32 texIdx,
                             s32 noDepth, EftWTexEntry *tex) {
    Vec4 c[4];
    Vec4 uv[4];
    EftWVert v[9];

    Vec4_Set(&uv[0], uv0.x, uv0.y, 1.0f, 1.0f);
    Vec4_Set(&uv[1], uv0.z, uv0.w, 1.0f, 1.0f);
    Vec4_Set(&uv[2], uv1.x, uv1.y, 1.0f, 1.0f);
    Vec4_Set(&uv[3], uv1.z, uv1.w, 1.0f, 1.0f);
    Vec4_Copy(&c[0], &corner[0]);
    Vec4_Copy(&c[1], &corner[1]);
    Vec4_Copy(&c[2], &corner[2]);
    Vec4_Copy(&c[3], &corner[3]);
    ClipVtx_Set(&v[0], &c[0], &uv[0], V(&color));
    ClipVtx_Set(&v[1], &c[1], &uv[1], V(&color));
    ClipVtx_Set(&v[2], &c[2], &uv[2], V(&color));
    EftGfx_DrawPolyScaledZ(v, layer, 1, 0, noDepth, 0, EFTW_TEX0(tex, texIdx), 2.0f);
    ClipVtx_Set(&v[0], &c[1], &uv[1], V(&color));
    ClipVtx_Set(&v[1], &c[2], &uv[2], V(&color));
    ClipVtx_Set(&v[2], &c[3], &uv[3], V(&color));
    EftGfx_DrawPolyScaledZ(v, layer, 1, 0, noDepth, 0, EFTW_TEX0(tex, texIdx), 2.0f);
}

/* Copies key i (0..2) of the definition's animated values into the emitter's current values. */
void EftLink_SetKey(EftWLink *w, s32 i) {
    EftWLinkDef *def = w->arg.def;
    EftWLinkKey *key;

    w->rotX = def->rotX[i];
    w->rotXRange = def->rotXRange[i];
    w->rotZ = def->rotZ[i];
    w->rotZRange = def->rotZRange[i];
    key = w->arg.key;
    w->dirAng = def->dirAng[i];
    w->dirAngRange = def->dirAngRange[i];
    w->spin = def->spin[i];
    w->spinRange = def->spinRange[i];
    w->twist = def->twist[i];
    w->ofsX = def->ofsX[i];
    w->ofsY = def->ofsY[i];
    w->unk1A0 = def->unk84[i];
    w->unk1A8 = def->unk90[i];
    w->dist = def->dist[i];
    w->distRange = def->distRange[i];
    w->distBase = def->distBase[i];
    w->size[0] = def->size[i][0];
    w->size[1] = def->size[i][1];
    w->size[2] = def->size[i][2];
    w->sizeRange[0] = def->sizeRange[i][0];
    w->sizeRange[1] = def->sizeRange[i][1];
    w->sizeRange[2] = def->sizeRange[i][2];
    w->life = def->life[i];
    w->lifeRange = def->lifeRange[i];
    w->wait = def->wait[i];
    w->waitRange = def->waitRange[i];
    w->pulse[0] = def->pulse[i][0];
    w->pulse[1] = def->pulse[i][1];
    w->pulseTime = def->pulseTime[i];
    w->mulR[0] = key->mulR[i][0];
    w->mulR[1] = key->mulR[i][1];
    w->mulG[0] = key->mulG[i][0];
    w->mulG[1] = key->mulG[i][1];
    w->mulB[0] = key->mulB[i][0];
    w->mulB[1] = key->mulB[i][1];
    w->mulTime = key->mulTime[i];
    w->fade0 = key->fade0[i];
    w->fade1 = key->fade1[i];
    Vec4_Copy(&w->col0, &key->col0[i]);
    Vec4_Copy(&w->col1, &key->col1[i]);
    Vec4_Copy(&w->col0Range, &key->col0Range[i]);
    Vec4_Copy(&w->col1Range, &key->col1Range[i]);
}

void EftLink_SelectTex(EftWLink *w, EftWTexEntry *tbl, s32 a, s32 b);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern f32 Rand_FloatRange(f32 lo, f32 hi);
extern s32 Rand_IntRange(s32 lo, s32 hi);
extern s32 rand(void);
extern f32 cosf(f32 x);
extern f32 sinf(f32 x);
extern f32 EftMath_WrapAngle(f32 a);
extern u64 EftVram_AddImage(EftWTexEntry *e, s32 a, s32 b);
extern u64 EftVram_AddClut(EftWTexEntry *e);
extern void Vec4_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi);
extern void Vu0Cur_Translate(Vec4 *v);
extern void Vu0Cur_ScaleDiagUniform(f32 s);
extern void Vu0Cur_RotateZ(f32 a);
extern void Vu0Cur_RotateX(f32 a);
extern void Vu0Cur_RotateY(f32 a);
extern void Vu0Cur_MulVec4(Vec4 *dst, Vec4 *src);

void EftLink_InitNode(EftWLinkNode *p, EftWLink *w);
void EftLink_LinkNode(EftWLinkNode **head, EftWLinkNode **tail, EftWLinkNode *p);
void EftLink_UnlinkNode(EftWLinkNode **head, EftWLinkNode **tail, EftWLinkNode *p);
void EftLink_BuildCorners(EftWLinkNode *p, EftWLink *w);
extern void Vu0Cur_Push(void);
extern void Vu0Cur_LoadIdentity(void);
extern void Vu0Cur_Pop(void);

extern EftWLinkMgr *gEftLink;
extern u8 gEftLinkClass[0x18];

/* Takes texture entry a and palette entry b of a pack's texture table; the blended TEX0 is cached in table slot
   a + b (slot b when both are the same). */
void EftLink_SelectTex(EftWLink *w, EftWTexEntry *tbl, s32 a, s32 b) {
    if (a == b) {
        w->texIdx = b;
    } else {
        w->texIdx = a + b;
    }
    w->tex = tbl[a];
    w->clut = tbl[b];
}

/* Builds the TEX0 of the selected texture / palette pair once per texture table slot (bit in the table's mask). */
void EftLink_BuildTex(EftWLink *w) {
    EftWLinkArg *arg = &w->arg;

    if (arg->tex != NULL) {
        if (!(arg->tex->built & (1 << w->texIdx))) {
            u64 t = EftVram_AddImage(&w->tex, 1, 0);

            t |= EftVram_AddClut(&w->clut) << 37;
            arg->tex->e[w->texIdx].tex0 = t;
            arg->tex->built |= 1 << w->texIdx;
        }
    }
}

/* Takes a free sprite from the shared pool of 200 (round robin from the last one taken), initialises it at pos
   and appends it to the emitter's list. t is the sprite's index along the chain (it scales the twist).
   Returns NULL when all 200 are in use. */
EftWLinkNode *EftLink_NewNode(EftWLink *w, EftWVec pos, f32 t) {
    EftWLinkNode *p;
    u8 i;

    if (gEftLink->next >= EFT_WLINK_NODES) {
        gEftLink->next = 0;
    }
    i = gEftLink->next;
    do {
        p = &gEftLink->ptcl[i];
        i++;
        if (i >= EFT_WLINK_NODES) {
            i = 0;
        }
        if (p->flags == 0) {
            EftLink_InitNode(p, w);
            Vec4_Copy(&p->origin, (Vec4 *)&pos);
            p->twist = EftMath_WrapAngle(w->twist * 6.2831853f * t);
            p->next = NULL;
            p->prev = NULL;
            p->flags |= 1;
            EftLink_LinkNode(&w->head, &w->tail, p);
            gEftLink->next = i;
            return p;
        }
    } while (i != gEftLink->next);
    return NULL;
}

/* Fills a new sprite from the emitter's current key values: orientation, drift direction and distance, life,
   start delay, colour ramp, size ramp, optional scale pulse and colour multiplier. 18 draws from the
   VU0 generator (Rand_FloatRange) and up to three from libc rand(): Rand_IntRange with definition flag 0x100
   (spin sign) and 0x80 (start frame of the sheet), rand() with flags 0x20 | 0x40 (colour ramp on or off).
   The mirrored-UV flag 0x80 it sets is cleared again a few lines later (original bug: only the UVs stay mirrored). */
void EftLink_InitNode(EftWLinkNode *p, EftWLink *w) {
    EftWLinkDef *def = w->arg.def;
    s32 flip = 1;
    Vec4 c1;
    f32 sz[3];
    f32 a;
    f32 r = 0.0f; /* dead initialiser: needed for the registers of the angle and the distance */

    Vec4_Set(&p->unk70, 0.0f, 0.0f, 1.0f, 1.0f);
    Vec4_Set(&p->ofs, w->ofsX, w->ofsY, 0.0f, 1.0f);
    a = Rand_FloatRange(w->rotX, w->rotX + w->rotXRange);
    p->rotX = EftMath_WrapAngle(a * 6.2831853f);
    a = Rand_FloatRange(w->rotZ, w->rotZ + w->rotZRange);
    p->rotZ = EftMath_WrapAngle(a * 6.2831853f);
    p->twist = 0.0f;
    p->spin = EftMath_WrapAngle(Rand_FloatRange(w->spin, w->spin + w->spinRange) * 3.14159265f);
    if (def->flags & 0x100) {
        if (w->spin < 0.0f && 0.0f < w->spin + w->spinRange) {
            flip = 0;
        }
        if (0.0f < w->spin && w->spin + w->spinRange < 0.0f) {
            flip = 0;
        }
        if (!(Rand_IntRange(0, 0x400) & 1) && flip) {
            p->spin = -p->spin;
        }
    }
    a = EftMath_WrapAngle(Rand_FloatRange(w->dirAng, w->dirAng + w->dirAngRange) * 6.2831853f);
    r = w->distBase + Rand_FloatRange(w->dist, w->dist + w->distRange);
    p->dir.x = cosf(a) * r;
    p->dir.y = sinf(a) * r;
    p->dir.z = 0.0f;
    p->dir.w = 1.0f;
    if ((def->flags & 8) && p->spin < p->dir.z) {
        Vec4_Set(&p->uv0, 0.984375f, 0.015625f, 0.015625f, 0.015625f);
        Vec4_Set(&p->uv1, 0.984375f, 0.984375f, 0.015625f, 0.984375f);
        p->flags |= 0x80;
    } else {
        Vec4_Set(&p->uv0, 0.015625f, 0.015625f, 0.015625f, 0.984375f);
        Vec4_Set(&p->uv1, 0.984375f, 0.015625f, 0.984375f, 0.984375f);
    }
    if (def->flags & 0x80) {
        p->texFrame = Rand_IntRange(0, w->texFrames - 1.0f);
    }
    p->life = Rand_FloatRange(w->life, w->life + w->lifeRange) * 30.0f;
    p->age = 0.0f;
    p->delay = Rand_FloatRange(w->wait, w->wait + w->waitRange) * 30.0f;
    p->fadeIn = p->life * def->fadeIn;
    p->fadeOut = p->life * (1.0f - def->fadeOut);
    p->flags = 0;
    if (!(def->flags & 0x20)) {
        p->flags = 0x800;
    } else if (def->flags & 0x40) {
        if (!(rand() & 1)) {
            p->flags |= 0x800;
        }
    }
    p->fadeTime = p->life * (w->fade1 - w->fade0);
    p->fadeT = 0.0f;
    p->col0.x = Rand_FloatRange(w->col0.x, w->col0.x + w->col0Range.x);
    p->col0.y = Rand_FloatRange(w->col0.y, w->col0.y + w->col0Range.y);
    p->col0.z = Rand_FloatRange(w->col0.z, w->col0.z + w->col0Range.z);
    p->col0.w = Rand_FloatRange(w->col0.w, w->col0.w + w->col0Range.w);
    c1.x = Rand_FloatRange(w->col1.x, w->col1.x + w->col1Range.x);
    c1.y = Rand_FloatRange(w->col1.y, w->col1.y + w->col1Range.y);
    c1.z = Rand_FloatRange(w->col1.z, w->col1.z + w->col1Range.z);
    c1.w = Rand_FloatRange(w->col1.w, w->col1.w + w->col1Range.w);
    Vec4_Clamp(&p->col0, &p->col0, 0.0f, 255.0f);
    Vec4_Clamp(&c1, &c1, 0.0f, 255.0f);
    Vec3_Sub(&p->colDelta, &c1, &p->col0);
    Vec4_Copy(&p->col, &p->col0);
    sz[0] = Rand_FloatRange(w->size[0], w->size[0] + w->sizeRange[0]);
    sz[1] = Rand_FloatRange(w->size[1], w->size[1] + w->sizeRange[1]);
    sz[2] = Rand_FloatRange(w->size[2], w->size[2] + w->sizeRange[2]);
    p->sizeVel0 = (sz[1] - sz[0]) / (p->life * def->sizeSplit);
    p->sizeVel1 = (sz[2] - sz[1]) / (p->life * (1.0f - def->sizeSplit));
    p->size = sz[0];
    if (def->flags & 0x800) {
        p->pulseTime = w->pulseTime * 30.0f;
        p->pulseT = 0.0f;
        p->pulseD = w->pulse[1] - w->pulse[0];
        p->pulse0 = w->pulse[0];
        p->scale = w->pulse[0];
    } else {
        p->scale = 1.0f;
    }
    if (def->flags & 0x1000) {
        p->mulTime = w->mulTime * 30.0f;
        p->mulT = 0.0f;
        p->mulD[0] = w->mulR[1] - w->mulR[0];
        p->mulD[1] = w->mulG[1] - w->mulG[0];
        p->mulD[2] = w->mulB[1] - w->mulB[0];
        p->mul0[0] = w->mulR[0];
        p->mul0[1] = w->mulG[0];
        p->mul0[2] = w->mulB[0];
    }
}

#define EFTW_CLAMP(x, lo, hi) (((x) < (lo)) ? (lo) : (((hi) < (x)) ? (hi) : (x)))
#define EFTW_CLAMP01(x) EFTW_CLAMP(x, 0.0f, 1.0f)

/* Steps every sprite of the emitter by one frame: start delay, scale pulse, colour multiplier, size, spin, fade
   in / out, colour ramp, sheet frame, corner positions; unlinks the ones whose life is over. alpha scales the
   opacity (the emitter's own fade). */
void EftLink_StepNodes(EftWLink *w, f32 alpha) {
    EftWLinkNode *p = w->head;
    f32 mul[3] = { 1.0f, 1.0f, 1.0f };
    EftWLinkDef *def = w->arg.def;
    f32 t = 0.0f;
    f32 a;
    f32 u2;
    f32 u;

    for (; p != NULL; p = p->next) {
        if (0.0f < p->delay) {
            p->delay -= 1.0f;
            continue;
        }
        t = EFTW_CLAMP01(p->age / p->life);
        u = 0.0f;
        if (def->flags & 0x800) {
            u = EFTW_CLAMP01(p->pulseT / p->pulseTime);
            p->scale = p->pulse0 + p->pulseD * u;
            if (!(p->flags & 0x10000)) {
                p->pulseT = p->pulseT + 1.0f;
            } else {
                p->pulseT = p->pulseT - 1.0f;
            }
            if (p->pulseT < 0.0f || p->pulseTime <= p->pulseT) {
                p->flags ^= 0x10000;
                p->pulseT = EFTW_CLAMP(p->pulseT, 0.0f, p->pulseTime);
            }
        }
        u2 = 0.0f;
        if (def->flags & 0x1000) {
            u2 = EFTW_CLAMP01(p->mulT / p->mulTime);
            mul[0] = p->mul0[0] + p->mulD[0] * u2;
            mul[1] = p->mul0[1] + p->mulD[1] * u2;
            mul[2] = p->mul0[2] + p->mulD[2] * u2;
            if (!(p->flags & 0x20000)) {
                p->mulT = p->mulT + 1.0f;
            } else {
                p->mulT = p->mulT - 1.0f;
            }
            if (p->mulT < 0.0f || p->mulTime <= p->mulT) {
                p->flags ^= 0x20000;
                p->mulT = EFTW_CLAMP(p->mulT, 0.0f, p->mulTime);
            }
        } else {
            mul[0] = 1.0f;
            mul[1] = 1.0f;
            mul[2] = 1.0f;
        }
        if (t < def->sizeSplit) {
            p->size += p->sizeVel0;
        } else {
            p->size += p->sizeVel1;
        }
        p->rotZ += p->spin;
        p->rotZ = EftMath_WrapAngle(p->rotZ);
        if (t < def->fadeIn) {
            a = p->age / p->fadeIn;
        } else {
            a = 1.0f;
        }
        if (def->fadeOut <= t) {
            a = 1.0f - (p->age - (p->life - p->fadeOut)) / p->fadeOut;
        }
        if (p->flags & 0x800) {
            f32 u = EFTW_CLAMP01(p->fadeT / p->fadeTime);
            p->col.x = (p->col0.x + p->colDelta.x * u) * mul[0];
            p->col.y = (p->col0.y + p->colDelta.y * u) * mul[1];
            p->col.z = (p->col0.z + p->colDelta.z * u) * mul[2];
            if (w->fade0 <= t && t < w->fade1) {
                p->fadeT += 1.0f;
            }
        } else {
            p->col.x = p->col0.x * mul[0];
            p->col.y = p->col0.y * mul[1];
            p->col.z = p->col0.z * mul[2];
        }
        p->col.w = p->col0.w * a * alpha;
        Vec4_Clamp(&p->col, &p->col, 0.0f, 255.0f);
        if (w->flags & 0x100) {
            u8 f = 0;

            if (!(p->texFrame < 0.0f)) {
                if (w->texFrames - 1.0f < p->texFrame) {
                    f = w->texFrames - 1.0f;
                } else {
                    f = p->texFrame;
                }
            }
            if (p->flags & 0x80) {
                Vec4_Set(&p->uv0, w->uv[f][2], w->uv[f][1], w->uv[f][0], w->uv[f][1]);
                Vec4_Set(&p->uv1, w->uv[f][2], w->uv[f][3], w->uv[f][0], w->uv[f][3]);
            } else {
                Vec4_Set(&p->uv0, w->uv[f][0], w->uv[f][1], w->uv[f][2], w->uv[f][1]);
                Vec4_Set(&p->uv1, w->uv[f][0], w->uv[f][3], w->uv[f][2], w->uv[f][3]);
            }
            if ((s32)w->frame % def->texStep == 0) {
                p->texFrame += 1.0f;
                if (w->texFrames <= p->texFrame) {
                    p->texFrame = 0.0f;
                }
            }
        }
        if (0.0f < p->col.w && 0.0f < p->size) {
            if (!(def->flags & 0x10)) {
                Vu0Cur_Push();
                Vu0Cur_LoadIdentity();
                EftLink_BuildCorners(p, w);
                Vu0Cur_Pop();
            }
            p->flags |= 0x40;
        } else {
            p->flags &= ~0x40;
        }
        p->age += 1.0f;
        if (p->life <= p->age) {
            s32 old = p->flags;

            p->flags &= ~1;
            if ((w->flags & 4) || !(old & 0x400)) {
                p->flags = 0;
                EftLink_UnlinkNode(&w->head, &w->tail, p);
            }
        }
        p->flags &= ~0x400;
    }
}

/* Builds the four world-space corners of a sprite with the matrix stack: size, its own three rotations, the
   drift offset, the chain's pitch / yaw and the spawn position. */
void EftLink_BuildCorners(EftWLinkNode *p, EftWLink *w) {
    EftWLinkDef *def = w->arg.def;
    Vec4 v;

    Vec4_Set(&p->corner[0], p->size * -0.5f, p->size * -0.5f, 0.0f, 1.0f);
    Vec4_Set(&p->corner[1], p->size * -0.5f, p->size * 0.5f, 0.0f, 1.0f);
    Vec4_Set(&p->corner[2], p->size * 0.5f, p->size * -0.5f, 0.0f, 1.0f);
    Vec4_Set(&p->corner[3], p->size * 0.5f, p->size * 0.5f, 0.0f, 1.0f);
    Vu0Cur_Translate(&p->ofs);
    Vu0Cur_ScaleDiagUniform(w->arg.size * w->scale * p->scale);
    Vu0Cur_RotateZ(p->rotZ);
    Vu0Cur_RotateX(p->rotX);
    Vu0Cur_RotateZ(p->twist);
    if (!(def->flags & 0x2000)) {
        Vec3_Scale(&v, &p->dir, w->scale);
        Vu0Cur_Translate(&v);
    } else {
        Vu0Cur_Translate(&p->dir);
    }
    Vu0Cur_RotateX(w->pitch);
    Vu0Cur_RotateY(w->yaw);
    Vu0Cur_Translate(&p->origin);
    Vu0Cur_MulVec4(&p->corner[0], &p->corner[0]);
    Vu0Cur_MulVec4(&p->corner[1], &p->corner[1]);
    Vu0Cur_MulVec4(&p->corner[2], &p->corner[2]);
    Vu0Cur_MulVec4(&p->corner[3], &p->corner[3]);
}

/* Appends a sprite to an emitter's list. */
void EftLink_LinkNode(EftWLinkNode **head, EftWLinkNode **tail, EftWLinkNode *p) {
    if (*head == NULL) {
        *head = p;
        *tail = p;
    } else {
        p->prev = *tail;
        (*tail)->next = p;
        *tail = p;
    }
}

/* Removes a sprite from an emitter's list. */
void EftLink_UnlinkNode(EftWLinkNode **head, EftWLinkNode **tail, EftWLinkNode *p) {
    if (*head == NULL) {
        return;
    }
    if (p->prev == NULL) {
        p = p->next;
        if (p == NULL) {
            *head = NULL;
            *tail = NULL;
        } else {
            *head = p;
            p->prev = NULL;
        }
    } else if (p->next == NULL) {
        EftWLinkNode *prev = p->prev;

        *tail = prev;
        prev->next = NULL;
    } else {
        p->next->prev = p->prev;
        p->prev->next = p->next;
    }
}

/* Queues a sprite that faces the camera through the clipping polygon path: a w x h rectangle (half sizes)
   stretched by `scale`, turned by `rot` in the view plane and placed at `pos`; two triangles at the depth slot
   of `pos` times zScale. Nothing is drawn when `pos` is behind the near plane. */
void EftLink_DrawBillboardClipped(Vec4 *pos, f32 w, f32 h, Vec4 *color, Vec4 *scale, f32 u0, f32 v0, f32 u1,
                                  f32 v1, f32 rot, s32 layer, s32 noDepth, u64 tex0, f32 zScale) {
    EftWVert v[9];
    Mtx44 m;
    Mtx44 inv;
    Vec4 c[4];
    Vec4 uv[4];
    EftWIVec scr;
    s32 i;
    s32 z;

    Vec4_Set(&c[0], -w, -h, 0.0f, 1.0f);
    Vec4_Set(&c[1], w, -h, 0.0f, 1.0f);
    Vec4_Set(&c[2], -w, h, 0.0f, 1.0f);
    Vec4_Set(&c[3], w, h, 0.0f, 1.0f);
    Mtx_Translate(&m, &gEftLink->camMtx, scale);
    Mtx_RotateZ(&m, &m, rot);
    Mtx_InverseRT(&inv, &gBtlCamView->unk40);
    inv.m[3][0] = 0.0f;
    inv.m[3][1] = 0.0f;
    inv.m[3][2] = 0.0f;
    Mtx_Mul(&m, &inv, &m);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&c[i], &m, &c[i]);
        Vec3_Add(&c[i], &c[i], pos);
        c[i].w = 1.0f;
    }
    Vu0Cur_ProjectPoint(&scr, pos);
    z = scr.z >> 8;
    if (scr.z >= 0) {
        Vec4_Set(&uv[0], u0, v0, 1.0f, 0.0f);
        Vec4_Set(&uv[1], u1, v0, 1.0f, 0.0f);
        Vec4_Set(&uv[2], u0, v1, 1.0f, 0.0f);
        Vec4_Set(&uv[3], u1, v1, 1.0f, 0.0f);
        for (i = 0; i < 2; i++) {
            ClipVtx_Set(&v[0], &c[i], &uv[i], color);
            ClipVtx_Set(&v[1], &c[i + 1], &uv[i + 1], color);
            ClipVtx_Set(&v[2], &c[i + 2], &uv[i + 2], color);
            EftGfx_DrawPolyFixedZ(v, layer, 0, 0, noDepth, 0, tex0, z * zScale);
        }
    }
}

/* Queues a sprite as a screen-aligned textured quad around the projection of `pos`: w x h are half sizes and
   (offX, offY) the centre offset, scaled by the perspective of `pos`; `rot` turns it about the view axis.
   Dropped when smaller than 2 units or when a corner leaves the GS drawing area. */
/* FAKE MATCH: the empty `__asm__("");` behind the offset scaling. It emits nothing but counts as one instruction
   in the live ranges of u1 / v1 and so breaks their allocation tie ($f27 / $f26); it stands in for a source form
   that is one instruction longer somewhere in the function, which was not found (see the note at the statement). */
void EftLink_DrawBillboard(Vec4 *pos, f32 w, f32 h, Vec4 *color, s32 offX, s32 offY, f32 u0, f32 v0, f32 u1,
                           f32 v1, f32 rot, s32 layer, s32 noDepth, u64 tex0) {
    Mtx44 m;
    Vec4 aspect;
    Vec4 a;
    Vec4 b;
    Vec4 c;
    Vec4 d;
    EftWIVec scr;
    EftWIVec ia;
    EftWIVec ib;
    EftWIVec ic;
    EftWIVec id;
    f32 scale = gBtlCamView->screenDist;
    s32 sw;
    s32 sh;
    s32 k;
    s32 oy;
    s32 z;
    s32 zz;
    s32 x0, y0, x1, y1, x2, y2, x3, y3;
    s32 abe = 1;
    s32 ctx;
    s32 l;
    u32 alpha;
    EftWQuadPkt *p;
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
    k = scale;
    sw = (sw * k) >> 12;
    sh = (sh * k) >> 12;
    if (offX >= 0) {
        offX = (offX * k) >> 12;
    } else {
        offX = -((-offX * k) >> 12);
    }
    if (offY >= 0) {
        oy = (offY * k) >> 12;
    } else {
        oy = -((-offY * k) >> 12);
    }
    /* An empty statement that still counts as one instruction for the register allocator: u1 and v1 have the
       same priority (30000 / 309 and 30000 / 307, both 97) and the tie puts u1 in $f26; one instruction more
       in their live range (310 and 308: 96 against 97) gives the original's $f27 / $f26. The original source
       must have differed by one instruction somewhere in the function; that form was not found. */
    __asm__("");
    if (sw < 2) {
        return;
    }
    if (sh < 2) {
        return;
    }
    Vec4_Set(&a, offX - sw, oy - sh, 0.0f, 1.0f);
    Vec4_Set(&b, offX - sw, sh + oy, 0.0f, 1.0f);
    Vec4_Set(&c, sw + offX, oy - sh, 0.0f, 1.0f);
    Vec4_Set(&d, sw + offX, sh + oy, 0.0f, 1.0f);
    Mtx_RotateZ(&m, &gEftLink->camMtx, rot);
    Mtx_MulVec4(&a, &m, &a);
    Mtx_MulVec4(&b, &m, &b);
    Mtx_MulVec4(&c, &m, &c);
    Mtx_MulVec4(&d, &m, &d);
    Vec4_Mul(&a, &a, &aspect);
    Vec4_Mul(&b, &b, &aspect);
    Vec4_Mul(&c, &c, &aspect);
    Vec4_Mul(&d, &d, &aspect);
    Vec4_ToInt(&ia, &a);
    Vec4_ToInt(&ib, &b);
    Vec4_ToInt(&ic, &c);
    Vec4_ToInt(&id, &d);
    if (EftLink_IsCornerOffScreen(scr.x + ia.x, scr.y + ia.y, scr.z)) {
        return;
    }
    if (EftLink_IsCornerOffScreen(scr.x + ib.x, scr.y + ib.y, scr.z)) {
        return;
    }
    if (EftLink_IsCornerOffScreen(scr.x + ic.x, scr.y + ic.y, scr.z)) {
        return;
    }
    if (EftLink_IsCornerOffScreen(scr.x + id.x, scr.y + id.y, scr.z)) {
        return;
    }
    if (noDepth) {
        scr.z = 0xFFFFFF;
    }
    zz = (scr.z >> 8) << 8;
    p = (EftWQuadPkt *)gOtCur;
    x0 = scr.x + ia.x;
    y0 = scr.y + ia.y;
    x1 = scr.x + ib.x;
    y1 = scr.y + ib.y;
    x2 = scr.x + ic.x;
    y2 = scr.y + ic.y;
    x3 = scr.x + id.x;
    gOtCur = (u32 *)(p + 1);
    y3 = scr.y + id.y;
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

/* Module entry of effect pack part kind 15: creates an emitter task (class gEftLinkClass) from the argument block. */
EftWTask *EftLink_Create(EftWLinkArg *arg) {
    if (gEftLink == NULL) {
        return NULL;
    }
    if (arg == NULL) {
        return NULL;
    }
    return BtlTaskList_AddTail(gEftLink->list, gEftLinkClass, arg);
}

/* Asks an emitter to finish: after its stop delay and / or fade-out when it has them, else at once (no new
   sprites; it dies with its last one). Ignored the second time. */
void EftLink_Stop(EftWTask *task) {
    EftWLink *w;
    s32 now = 1;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftLink_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & 0x1000) {
        return;
    }
    if (!(w->flags & 1)) {
        return;
    }
    if (0.0f < w->stopDelay) {
        w->flags |= 8;
        now = 0;
    }
    if (0.0f < w->fadeMax) {
        w->flags |= 0x20;
        now = 0;
    }
    if (now) {
        w->flags |= 4;
    }
    w->flags |= 0x1000;
}

/* Kills an emitter now (flags 0x10 | 4). */
void EftLink_Kill(EftWTask *task) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftLink_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & 1) {
        w->flags |= 0x14;
    }
}

/* Sets the fade-out length in frames. */
void EftLink_SetFade(EftWTask *task, s32 frames) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftLink_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & 1) {
        w->fade = frames;
        w->fadeMax = frames;
    }
}

/* Sets both end points of the chain. No caller in the executable. */
void EftLink_SetEnds(EftWTask *task, EftWVec pos, EftWVec pos2) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftLink_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & 1) {
        Vec4_Copy(&w->arg.pos, (Vec4 *)&pos);
        Vec4_Copy(&w->arg.pos2, (Vec4 *)&pos2);
    }
}

/* Sets the first end point. */
void EftLink_SetPos(EftWTask *task, EftWVec pos) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftLink_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & 1) {
        Vec4_Copy(&w->arg.pos, (Vec4 *)&pos);
    }
}

/* Sets the second end point. */
void EftLink_SetPos2(EftWTask *task, EftWVec pos) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftLink_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & 1) {
        Vec4_Copy(&w->arg.pos2, (Vec4 *)&pos);
    }
}

/* The module's "warp" entry: does nothing. (The effect pack library calls it for EFT_SPAWN_MOVE | EFT_SPAWN_WARP.)
   The original still copies the by-value vector to its stack (ld / ld / sd / sd): the life pass deletes such frame
   stores only in the last basic block, so the body was not empty when flow analysis ran. What reproduces it is a
   float local read from the low half of the vector (x or y), tested and conditionally reassigned, and never used
   (the compare and branch are deleted after the life pass): the remains of stubbed-out code. WHICH test and value
   stood there cannot be recovered (`< 0.0f` / `= 0.0f`, `!= 0.0f` / `= 1.0f`, `> 1.0f` ... all give the same code);
   reading z or w first exchanges v0 / v1. Found by decomp-permuter as `float n; if (n) { n = 1; }`. */
void EftLink_Warp(EftWTask *task, EftWVec pos) {
    f32 x = pos.x;

    if (x < 0.0f) {
        x = 0.0f;
    }
}

/* The module's "set direction" entry: does nothing (the direction comes from the two end points). Same dead
   remains as EftLink_Warp. */
void EftLink_SetDir(EftWTask *task, EftWVec dir) {
    f32 x = dir.x;

    if (x < 0.0f) {
        x = 0.0f;
    }
}

/* Sets the size factor. */
void EftLink_SetSize(EftWTask *task, f32 size) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftLink_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & 1) {
        w->arg.size = size;
    }
}

/* Sets the argument block's rate. No caller in the executable. */
void EftLink_SetRate(EftWTask *task, f32 rate) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftLink_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & 1) {
        w->arg.rate = rate;
    }
}

/* Changes the texture table and the texture / palette pair. No caller in the executable. */
void EftLink_SetTex(EftWTask *task, EftWTexSet *tex, s32 a, s32 b) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] == EftLink_Update) {
        w = task->work;
        if (w != NULL && (w->flags & 1)) {
            w->arg.tex = tex;
            EftLink_SelectTex(w, tex->e, a, b);
        }
    }
}

/* Sets the start delay from a frame count (stored in seconds). */
void EftLink_SetDelay(EftWTask *task, s32 v) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftLink_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & 1) {
        w->delay = (f32)v / 30.0f;
    }
}

/* Sets how many frames a stop request waits before the emitter stops. */
void EftLink_SetStopDelay(EftWTask *task, s32 v) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftLink_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & 1) {
        w->stopDelay = v;
    }
}

/* Sets emitter flag 0x200. Returns 0 only when the handle is not an emitter. No caller in the executable. */
s32 EftLink_SetFlag200(EftWTask *task) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftLink_Update) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & 1) {
        w->flags |= 0x200;
    }
    return 1;
}

/* Stores the caller's tag (the effect pack library passes its arg3). Returns 0 only for a bad handle. */
s32 EftLink_SetType(EftWTask *task, s32 v) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftLink_Update) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & 1) {
        w->type = v;
    }
    return 1;
}

/* 1 while the handle is a live emitter. */
s32 EftLink_IsAlive(EftWTask *task) {
    EftWLink *w;

    if (gEftLink == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftLink_Update) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & 1) {
        return 1;
    }
    return 0;
}

/* 1 when a GS screen position (12.4 fixed point) is outside the drawing area or behind the eye. */
s32 EftLink_IsCornerOffScreen(s32 x, s32 y, s32 z) {
    if (z <= 0) {
        return 1;
    }
    if (x > 0xFFEF) {
        return 1;
    }
    if (x <= 0) {
        return 1;
    }
    if (y > 0xFFEF) {
        return 1;
    }
    if (y <= 0) {
        return 1;
    }
    return 0;
}

extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern void *BtlTask_CreateChildList(EftWTask *task, s32 count, s32 workSize);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Mathf_Asin(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern void BtlTask_SetDead(EftWTask *task);
extern void Vu0Cur_LoadMtx(Mtx44 *m);
extern void EftPart10_InitSpin(EftPart10 *w);
extern void EftPart10_StartKeys(EftPart10 *w);
extern void EftPart10_SetKey(EftPart10 *w, s32 key);
extern void EftPart10_SelectTex(EftPart10 *w, EftWTexSet *tex, s32 a, s32 b);
extern void EftPart10_UpdateKeys(EftPart10 *w);
extern void EftPart10_UpdateSpin(EftPart10 *w);
extern void EftPart10_BuildTex(EftPart10 *w, EftWTexSet *tex);
extern void EftPart10_DrawBillboardClipped(Vec4 *pos, f32 w, f32 h, Vec4 *color, Vec4 *scale, f32 u0, f32 v0, f32 u1,
                                           f32 v1, f32 rot, s32 layer, s32 noDepth, u64 tex0, f32 zScale);
extern void EftPart10_DrawBillboard(Vec4 *pos, Vec4 *color, f32 w, s32 offX, s32 offY, f32 h, f32 u0, f32 v0, s32 layer,
                                    f32 u1, f32 v1, s32 noDepth, u64 tex0, f32 rot);
/* The three vectors are passed by value (hidden pointers); declared as pointers here. */
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftPart10_DrawQuadClipped(Vec4 *corner, PortVec4 uv0, PortVec4 uv1, PortVec4 col, s32 layer, s32 texIdx, s32 noDepth, EftWTexSet *tex);
#define EftPart10_DrawQuadClipped(corner, uv0, uv1, col, layer, texIdx, noDepth, tex) EftPart10_DrawQuadClipped(corner, *(PortVec4 *)(uv0), *(PortVec4 *)(uv1), *(PortVec4 *)(col), layer, texIdx, noDepth, tex)
#else
extern void EftPart10_DrawQuadClipped(Vec4 *corner, Vec4 *uv0, Vec4 *uv1, Vec4 *col, s32 layer, s32 texIdx, s32 noDepth,
                                      EftWTexSet *tex);
#endif
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftPart10_DrawQuad(Vec4 *corner, PortVec4 uv0, PortVec4 uv1, PortVec4 col, s32 layer, s32 texIdx, s32 noDepth, EftWTexSet *tex);
#define EftPart10_DrawQuad(corner, uv0, uv1, col, layer, texIdx, noDepth, tex) EftPart10_DrawQuad(corner, *(PortVec4 *)(uv0), *(PortVec4 *)(uv1), *(PortVec4 *)(col), layer, texIdx, noDepth, tex)
#else
extern void EftPart10_DrawQuad(Vec4 *corner, Vec4 *uv0, Vec4 *uv1, Vec4 *col, s32 layer, s32 texIdx, s32 noDepth,
                               EftWTexSet *tex);
#endif
extern void EftPart10_UnlinkPtcl(EftPart10Ptcl **head, EftPart10Ptcl **tail, EftPart10Ptcl *p);
extern void EftPart10_UnlinkGroup(EftPart10Grp **head, EftPart10Grp **tail, EftPart10Grp *g);
extern void EftPart10_LinkGroup(EftPart10Grp **head, EftPart10Grp **tail, EftPart10Grp *g);
extern void EftPart10_Emit(EftPart10 *w, EftPart10Grp *g, f32 a, f32 b);
extern void EftPart10_BuildCorners(EftPart10Ptcl *p, EftPart10 *w);

void EftPart10_EmitRings(EftPart10 *w);
void EftPart10_Step(f32 alpha, EftPart10 *w);
EftPart10Grp *EftPart10_NewGroup(EftPart10 *w);

extern EftPart10Mgr *gEftPart10Mgr;
extern EftWView *gBtlCamView;

/* Init callback of the manager class: allocates the shared block and a child list of 12 emitter tasks. */
void EftPart10Mgr_Init(EftWTask *task) {
    gEftPart10Mgr = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftPart10Mgr));
    memset(gEftPart10Mgr, 0, sizeof(EftPart10Mgr));
    gEftPart10Mgr->list = BtlTask_CreateChildList(task, 12, sizeof(EftPart10));
    Mtx_StoreIdentity(&gEftPart10Mgr->camMtx);
}

/* Update callback of the manager class: nothing. */
void EftPart10Mgr_Update(EftWTask *task) {
}

/* Reset callback of the manager class: nothing. */
void EftPart10Mgr_Reset(EftWTask *task) {
}

/* Term callback of the manager class: frees the shared block. */
void EftPart10Mgr_Term(EftWTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftPart10Mgr);
    gEftPart10Mgr = NULL;
}

/* Init callback of an emitter: copies the argument block, starts the spin and key animations the definition asks
   for, builds the UV rectangles of a cols x rows sprite sheet and selects the texture. */
void EftPart10_Init(EftWTask *task, EftPart10Arg *arg) {
    EftPart10 *w = task->work;
    EftPart10Def *def = arg->def;
    f32 step[2];
    s32 i;
    s32 j;
    s32 n;

    memset(w, 0, sizeof(EftPart10));
    w->arg = *arg;
    w->arg.def = arg->def;
    w->arg.key = arg->key;
    w->arg.tex = arg->tex;
    w->grpHead = NULL;
    w->grpTail = NULL;
    if (def->flags & 0x40) {
        EftPart10_InitSpin(w);
    } else {
        w->unk258 = 0.0f;
        w->scale = 1.0f;
        w->stopDelay = 0.0f;
        w->count = def->count;
    }
    if (def->flags & 0x200) {
        EftPart10_StartKeys(w);
        w->flags |= 0x40000;
    } else {
        EftPart10_SetKey(w, 2);
    }
    EftW_MemsetCall(step, 0, sizeof(step));
    if (def->cols >= 2 || def->rows >= 2) {
        w->texFrames = def->cols * def->rows;
        step[0] = 1.0f / def->cols;
        step[1] = 1.0f / def->rows;
        n = 0;
        for (j = 0; j < def->rows; j++) {
            for (i = 0; i < def->cols; i++) {
                w->uv[n][0] = step[0] * i;
                w->uv[n][1] = step[1] * j;
                w->uv[n][2] = step[0] * i + step[0];
                w->uv[n][3] = step[1] * j + step[1];
                n++;
            }
        }
        w->flags |= 0x100;
    }
    w->pitch = EftMath_WrapAngle(def->pitch * 3.14159265f);
    w->yaw = EftMath_WrapAngle(def->yaw * 3.14159265f);
    EftPart10_SelectTex(w, w->arg.tex, w->arg.texSel, w->arg.texSel);
    w->flags |= 0x41;
}

/* Update callback of an emitter: nothing while the owner's effects are stopped or the start delay runs; else
   fade-out, key and spin animation, emission (until stopped), particle step, life time and stop handling. The
   task dies when its last ring is gone after a stop, or at once when killed. */
void EftPart10_Update(EftWTask *task) {
    EftPart10 *w = task->work;
    EftPart10Arg *arg = &w->arg;
    EftPart10Def *def = arg->def;
    f32 alpha = 1.0f;

    if (!BtlScene_IsEffectStopped(arg->objId, w->kind)) {
        if (0.0f < w->delay) {
            w->delay -= 1.0f;
        } else {
            if ((w->flags & 0x10) && !(w->flags & 8)) {
                alpha = w->fade / w->fadeTime;
                if (!(w->fade < 0.0f)) {
                    w->fade -= 1.0f;
                } else {
                    w->flags = (w->flags & ~1) | 2;
                }
            }
            if (w->flags & 0x40000) {
                EftPart10_UpdateKeys(w);
            }
            if (def->flags & 0x40) {
                EftPart10_UpdateSpin(w);
            }
            if (!(w->flags & 4)) {
                EftPart10_EmitRings(w);
            }
            EftPart10_Step(alpha, w);
            w->frame += 1.0f;
            if (w->flags & 0x40000) {
                EftPart10_StartKeys(w);
                if (w->frame >= w->unk180) {
                    EftPart10_SetKey(w, 2);
                    w->flags &= ~0x40000;
                }
            }
            if (arg->life * 30.0f <= w->frame && 0.0f < arg->life) {
                if (0.0f < w->fadeTime) {
                    w->flags |= 0x80010;
                } else {
                    w->flags |= 4;
                }
            }
            if (w->flags & 8) {
                if (0.0f < w->timer) {
                    w->timer -= 1.0f;
                } else {
                    w->flags &= ~8;
                    if (!(w->flags & 0x10)) {
                        w->flags |= 4;
                    }
                }
            }
            if (w->grpHead == NULL) {
                if (w->flags & 4) {
                    w->flags |= 2;
                }
            }
            if ((w->flags & 0x20) || (w->flags & 2)) {
                w->flags &= ~1;
                BtlTask_SetDead(task);
            }
        }
    }
    if (!(w->flags & 0x20)) {
        if (!(w->flags & 2)) {
            EftPart10_BuildTex(w, w->arg.tex);
        }
    }
}

/* Post-update callback: nothing. */
void EftPart10_PostUpdate(EftWTask *task) {
}

/* Draw callback: queues every visible particle, as a camera-facing billboard (definition mode 1) or as a quad
   through its four corners, each with or without clipping (definition flag 8). */
void EftPart10_Draw(EftWTask *task) {
    EftPart10 *w = task->work;
    EftPart10Def *def = w->arg.def;
    Vec4 pos;
    EftPart10Grp *grp;
    EftPart10Ptcl *p;

    EftW_MemsetCall(&pos, 0, sizeof(Vec4));
    pos.w = 1.0f;
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->unk140);
    for (grp = w->grpHead; grp != NULL; grp = grp->next) {
        for (p = grp->head; p != NULL; p = p->next) {
            if (p->flags & 0x40) {
                if (def->mode == 1) {
                    f32 half = p->size * w->arg.size * w->scale * p->scale * 0.5f;

                    Vec3_Add(&pos, &p->pos, &w->arg.pos);
                    if (def->flags & 8) {
                        EftPart10_DrawBillboardClipped(&pos, half, half, &p->col, &p->unk20, p->uv0.x, p->uv0.y,
                                                       p->uv1.z, p->uv1.w, p->ang[2], def->blend,
                                                       (w->flags >> 10) & 1, EFTW_TEX0(w->arg.tex, w->texIdx), 2.0f);
                    } else {
                        EftPart10_DrawBillboard(&pos, &p->col, half * 16.0f, (s32)p->unk20.x << 4, (s32)p->unk20.y << 4,
                                                half * 16.0f, p->uv0.x, p->uv0.y, def->blend, p->uv1.z,
                                                p->uv1.w, (w->flags >> 10) & 1, EFTW_TEX0(w->arg.tex, w->texIdx),
                                                p->ang[2]);
                    }
                } else if (def->flags & 8) {
                    EftPart10_DrawQuadClipped(&p->corner[0], &p->uv0, &p->uv1, &p->col, def->blend, w->texIdx,
                                              (w->flags >> 10) & 1, w->arg.tex);
                } else {
                    EftPart10_DrawQuad(&p->corner[0], &p->uv0, &p->uv1, &p->col, def->blend, w->texIdx,
                                       (w->flags >> 10) & 1, w->arg.tex);
                }
            }
        }
    }
    Vu0Cur_Pop();
}

/* Reset callback: kills the task. */
void EftPart10_Reset(EftWTask *task) {
    BtlTask_SetDead(task);
}

/* Term callback: returns every particle and ring to the shared pools. */
void EftPart10_Term(EftWTask *task) {
    EftPart10 *w = task->work;
    EftPart10Grp *grp;
    EftPart10Ptcl *p;

    for (grp = w->grpHead; grp != NULL; grp = grp->next) {
        for (p = grp->head; p != NULL; p = p->next) {
            p->flags = 0;
            EftPart10_UnlinkPtcl(&grp->head, &grp->tail, p);
        }
        if (grp->head == NULL) {
            grp->flags = 0;
            EftPart10_UnlinkGroup(&w->grpHead, &w->grpTail, grp);
        }
    }
    w->flags = 0;
}

/* Keeps the emitter's rings filled: walks (count + 1) x count ring slots between the definition's ringFrom /
   ringTo fractions, creates missing rings and emits into empty ones (EftPart10_Emit) at angles step * i and
   step * j plus a random offset. Two Rand_FloatRange (VU0) draws per emitted ring. */
void EftPart10_EmitRings(EftPart10 *w) {
    EftPart10Def *def = w->arg.def;
    EftPart10Grp *grp = w->grpHead;
    s32 lim[2];
    f32 step = 1.0f / w->count * 6.2831853f;
    s32 i;
    s32 j;
    f32 a;
    f32 b;

    lim[0] = w->count * def->ringFrom;
    lim[1] = w->count * def->ringTo;
    w->twist = 0.0f;
    for (i = 0; i < (s32)w->count + 1; i++) {
        if (i >= lim[0] && i <= lim[1]) {
            for (j = 0; j < (s32)w->count; j++) {
                if (grp == NULL) {
                    grp = EftPart10_NewGroup(w);
                    if (grp == NULL) {
                        continue;
                    }
                }
                if (grp->head == NULL) {
                    w->twist += def->twistStep * 3.14159265f;
                    a = EftMath_WrapAngle(step * i + Rand_FloatRange(w->angA, w->angA + w->angARange) * 3.14159265f);
                    b = EftMath_WrapAngle(step * j + Rand_FloatRange(w->angB, w->angB + w->angBRange) * 3.14159265f);
                    EftPart10_Emit(w, grp, a, b);
                }
                grp->flags |= 0x800;
                grp = grp->next;
            }
        }
    }
}

/* Steps every particle of every ring by one frame: start delay, colour multiplier, scale pulse, distance and
   angles, position (matrix stack), size, colour ramp, fade, sheet frame, corners; frees expired particles and
   rings that were not refilled this frame. Original bug (verified): when the scale pulse turns around, the
   colour multiplier's timer is clamped instead of the pulse timer. */
void EftPart10_Step(f32 alpha, EftPart10 *w) {
    f32 mul[3] = { 1.0f, 1.0f, 1.0f };
    EftPart10Def *def = w->arg.def;
    f32 yaw = 0.0f;
    f32 pitch = 0.0f;
    EftPart10Grp *grp;
    EftPart10Ptcl *p;
    f32 t;
    f32 a;
    f32 u;
    f32 u2;
    f32 v;

    if (def->flags & 1) {
        pitch = EftMath_WrapAngle(Mathf_Asin(-w->arg.dir.y));
        yaw = EftMath_WrapAngle(atan2f(w->arg.dir.x, w->arg.dir.z));
    }
    for (grp = w->grpHead; grp != NULL; grp = grp->next) {
        for (p = grp->head; p != NULL; p = p->next) {
            if (0.0f < p->delay) {
                p->delay -= 1.0f;
                continue;
            }
            t = p->age / p->life;
            u = 0.0f;
            if (def->flags & 0x100) {
                if (0.0f < p->mulTime) {
                    u = EFTW_CLAMP01(p->mulT / p->mulTime);
                } else {
                    u = 1.0f;
                }
                if (p->flags & 0x2000) {
                    mul[0] = p->mul0[0] + p->mulD[0] * u;
                }
                if (p->flags & 0x4000) {
                    mul[1] = p->mul0[1] + p->mulD[1] * u;
                }
                if (p->flags & 0x8000) {
                    mul[2] = p->mul0[2] + p->mulD[2] * u;
                }
                if (p->flags & 0x10000) {
                    p->mulT = p->mulT - 1.0f;
                } else {
                    p->mulT = p->mulT + 1.0f;
                }
                if (p->mulT >= p->mulTime || p->mulT <= 0.0f) {
                    p->flags ^= 0x10000;
                    p->mulT = EFTW_CLAMP(p->mulT, 0.0f, p->mulTime);
                }
            } else {
                mul[0] = 1.0f;
                mul[1] = 1.0f;
                mul[2] = 1.0f;
            }
            u2 = 0.0f;
            if (def->flags & 0x80) {
                if (0.0f < p->pulseTime) {
                    u2 = EFTW_CLAMP01(p->pulseT / p->pulseTime);
                } else {
                    u2 = 1.0f;
                }
                p->scale = p->pulse0 + p->pulseD * u2;
                if (p->flags & 0x1000) {
                    p->pulseT = p->pulseT - 1.0f;
                } else {
                    p->pulseT = p->pulseT + 1.0f;
                }
                if (p->pulseTime <= p->pulseT || p->pulseT <= 0.0f) {
                    p->flags ^= 0x1000;
                    p->mulT = EFTW_CLAMP(p->mulT, 0.0f, p->mulTime);
                }
            }
            p->dist += p->distVel;
            p->ang[0] += p->angVel[0];
            p->ang[1] += p->angVel[1];
            p->ang[2] += p->angVel[2];
            p->ang[0] = EftMath_WrapAngle(p->ang[0]);
            p->ang[1] = EftMath_WrapAngle(p->ang[1]);
            p->ang[2] = EftMath_WrapAngle(p->ang[2]);
            Vec3_Scale(&p->pos, &p->base, p->dist * w->arg.size * w->scale);
            Vu0Cur_Push();
            Vu0Cur_LoadIdentity();
            Vu0Cur_RotateX(w->yaw);
            Vu0Cur_RotateY(w->pitch);
            Vu0Cur_RotateX(EftMath_WrapAngle(p->ang[0] + w->angA0 + pitch));
            Vu0Cur_RotateY(EftMath_WrapAngle(p->ang[1] + w->angB0 + yaw));
            Vu0Cur_MulVec4(&p->pos, &p->pos);
            Vu0Cur_Pop();
            if (t < def->sizeSplit) {
                p->size += p->sizeVel0;
            } else {
                p->size += p->sizeVel1;
            }
            v = a = 0.0f;
            if (p->flags & 0x80) {
                v = EFTW_CLAMP01(p->fadeT / p->fadeTime);

                p->col.x = (p->col0.x + p->colDelta.x * v) * mul[0];
                p->col.y = (p->col0.y + p->colDelta.y * v) * mul[1];
                p->col.z = (p->col0.z + p->colDelta.z * v) * mul[2];
                if (w->fade0 <= t && t < w->fade1) {
                    p->fadeT += 1.0f;
                }
            } else {
                p->col.x = p->col0.x * mul[0];
                p->col.y = p->col0.y * mul[1];
                p->col.z = p->col0.z * mul[2];
            }
            if (t <= def->fadeIn && 0.0f < p->fadeIn) {
                a = p->age / p->fadeIn;
            } else {
                a = 1.0f;
            }
            if (def->fadeOut < t && 0.0f < p->fadeOut) {
                a = 1.0f - (p->age - (p->life - p->fadeOut)) / p->fadeOut;
            }
            p->col.w = p->col0.w * EFTW_CLAMP01(a) * alpha;
            Vec4_Clamp(&p->col, &p->col, 0.0f, 255.0f);
            if (w->flags & 0x100) {
                if (p->flags & 0x200) {
                    Vec4_Set(&p->uv0, w->uv[(s32)p->texFrame][2], w->uv[(s32)p->texFrame][1],
                             w->uv[(s32)p->texFrame][0], w->uv[(s32)p->texFrame][1]);
                    Vec4_Set(&p->uv1, w->uv[(s32)p->texFrame][2], w->uv[(s32)p->texFrame][3],
                             w->uv[(s32)p->texFrame][0], w->uv[(s32)p->texFrame][3]);
                } else {
                    Vec4_Set(&p->uv0, w->uv[(s32)p->texFrame][0], w->uv[(s32)p->texFrame][1],
                             w->uv[(s32)p->texFrame][2], w->uv[(s32)p->texFrame][1]);
                    Vec4_Set(&p->uv1, w->uv[(s32)p->texFrame][0], w->uv[(s32)p->texFrame][3],
                             w->uv[(s32)p->texFrame][2], w->uv[(s32)p->texFrame][3]);
                }
                if ((s32)p->age % def->texStep == 0) {
                    p->texFrame += 1.0f;
                    if (w->texFrames <= p->texFrame) {
                        p->texFrame = 0.0f;
                    }
                }
            }
            if (0.0f < p->col.w && 0.0f < p->size) {
                if (w->arg.def->mode != 1) {
                    Vu0Cur_Push();
                    Vu0Cur_LoadIdentity();
                    EftPart10_BuildCorners(p, w);
                    Vu0Cur_Pop();
                }
                p->flags |= 0x40;
            } else {
                p->flags &= ~0x40;
            }
            p->age += 1.0f;
            if (p->life <= p->age || (w->flags & 0x20)) {
                EftPart10_UnlinkPtcl(&grp->head, &grp->tail, p);
                p->flags = 0;
            }
        }
        if (grp->head == NULL) {
            if (!(grp->flags & 0x800)) {
                EftPart10_UnlinkGroup(&w->grpHead, &w->grpTail, grp);
                grp->flags = 0;
            }
        }
        grp->flags &= ~0x800;
    }
}

/* Takes a free ring from the shared pool of 150 (round robin) and links it to the emitter. NULL when none is free. */
EftPart10Grp *EftPart10_NewGroup(EftPart10 *w) {
    EftPart10Grp *g;
    u8 i;

    if (gEftPart10Mgr->grpNext >= EFT_PART10_GRPS) {
        gEftPart10Mgr->grpNext = 0;
    }
    i = gEftPart10Mgr->grpNext;
    do {
        g = &gEftPart10Mgr->grp[i];
        i++;
        if (i >= EFT_PART10_GRPS) {
            i = 0;
        }
        if (g->flags == 0) {
            g->flags = 1;
            g->head = NULL;
            g->tail = NULL;
            g->next = NULL;
            g->prev = NULL;
            EftPart10_LinkGroup(&w->grpHead, &w->grpTail, g);
            gEftPart10Mgr->grpNext = i;
            return g;
        }
    } while (i != gEftPart10Mgr->grpNext);
    return NULL;
}
