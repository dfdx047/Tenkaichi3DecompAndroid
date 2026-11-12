#include "common.h"
#include "battle/eft_n.h"
#include "sys/gfx_ot.h"

/*
 * Effect tasks, 0x1637A0..0x167E68. See include/battle/eft_n.h.
 *
 * 45 of 47 functions are C. Two are INCLUDE_ASM with the attempt in `#if 0` above them: EftBolt_Shape and
 * EftBolt_Draw (EftAura_DrawFlames matches since cleanup W1). Their float constants are emitted in place with LIT4_WORD, so the file's .lit4
 * is the whole of 0x2FC97C..0x2FCAAC whichever way they are built.
 *
 * Nothing here is simulation: no hit record, fighter, battle object or battle event is written. The fighter is
 * only read through BtlCharApi_* getters. libc rand() is drawn by the lightning (EftBolt_Shape, EftBolt_Step,
 * EftBolt_UpdateAll, EftBolt_AddFlash, EftBolt_Spawn), at a rate that depends on the aura state.
 */

#define RAND_MAX_F 2147483647.0f
#define RANDF() ((f32)rand() / RAND_MAX_F)
#define EFT_DEG(x) ((x) / 180.0f * 3.14159265f)

typedef struct EftNBattleWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags; /* 0x100 = paused */
} EftNBattleWork;

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftNView {
    /* 0x000 */ Mtx44 world2view;
    /* 0x040 */ Mtx44 world2view2;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 world2screen;
    /* 0x180 */ u8 unk180[0xA0];
    /* 0x220 */ Vec4 pos;
} EftNView;

/* Effect task (include/battle/eft_g.h). */
typedef struct EftNTask {
    /* 0x00 */ u8 flags;
    /* 0x01 */ u8 unk1[0x37];
    /* 0x38 */ void *work;
} EftNTask;

/* A GS screen position as Vu0Cur_ProjectPoint writes it. */
typedef struct EftNScr {
    /* 0x0 */ u32 x;
    /* 0x4 */ u32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ s32 w;
} EftNScr;

/* One vertex of the quad packets built here: colour, texture coordinates, position. */
typedef struct EftNVtx {
    /* 0x00 */ u8 r;
    /* 0x01 */ u8 g;
    /* 0x02 */ u8 b;
    /* 0x03 */ u8 a;
    /* 0x04 */ f32 q;
    /* 0x08 */ f32 s;
    /* 0x0C */ f32 t;
    /* 0x10 */ u64 x : 16;
    /* 0x12 */ u64 y : 16;
    /* 0x14 */ u64 z : 24;
    /* 0x17 */ u64 f : 8;
} EftNVtx; /* 0x18 */

/* A 4-vertex textured, gouraud-shaded triangle strip (0x90 bytes at gOtCur). */
typedef struct EftNQuadPkt {
    /* 0x00 */ u32 tag;
    /* 0x04 */ OtPrim *next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftNVtx v[4];
} EftNQuadPkt; /* 0x90 */

extern EftNView *gBtlCamView;
extern u8 gBattleProf[];
extern void *gEftAuraTaskList;
extern void *gEftAuraTaskClass[];

#define V(p) ((Vec4 *)(p))
#define gPool ((EftAuraMgr *)gEftAuraPool)
#define gData ((EftAuraData *)gEftAuraCfg)

extern s32 rand(void);
extern void *memset(void *dst, s32 c, u32 n);
extern f32 sqrtf(f32 x);
extern f32 asinf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern f32 Vec3_LengthSq(Vec4 *v);                          /* squared length */
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Z */
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about X */
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Y */
extern void Vu0Cur_Push(void);                            /* VU0 matrix stack push */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                        /* load the matrix */
extern void Vu0Cur_Pop(void);                            /* pop */
extern s32 Vu0Cur_ProjectPoint(EftNScr *out, Vec4 *pos);          /* project to GS screen coordinates; returns a value */
extern void IVec4_Set(s32 *out, s32 x, s32 y, s32 z, s32 w);
extern void EftSpr_DrawRot(u8 r, u8 g, u8 b, u8 a, f32 x, f32 y, f32 z, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot,
                          s32 t0, s32 t1, s32 w, s32 h, s32 s0, u32 size, s32 s2, s32 s3, void *tex);
extern void BtlTask_SetDead(EftNTask *task);                  /* kills the task */
extern u64 EftVram_AddTex(EftNTexEntry *tex, s32 a, s32 b);  /* TEX0 of an entry */
extern u64 EftVram_AddImage(EftNTexEntry *tex, s32 a, s32 b);  /* TEX0 without the palette */
extern u64 EftVram_AddClut(EftNTexEntry *tex);                /* palette base of an entry */
extern void EftTexSet_Load32(EftNTexSet *set, s32 *data);
extern void EftBolt_Enable(s32 objId);                       /* creates the fighter's lightning task (eft_o) */
extern void EftBolt_Disable(s32 objId);                       /* asks it to stop */
extern s32 EftGlow_IsActive(s32 objId);
extern void Dbg_ProfMark(void *prof);
extern void Dbg_ProfColor(void *prof, u32 color);
extern EftNBattleWork *Battle_GetWork(void);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern s32 BtlScene_GetCharCount(void);
extern s32 BtlScene_IsStageFlagOn(void);
extern s32 BtlStage_IsReady(void);
extern void *BtlTask_CreateChildList(EftNTask *task, s32 count, s32 workSize);
extern EftNTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern s32 BtlCharApi_ObjGetParamFlags0(s32 objId);
extern s32 BtlCharApi_GetAuraType(s32 objId);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern s32 BtlCharApi_IsModelNew(s32 objId);
extern s32 BtlCharApi_IsHidden(s32 objId);
extern s32 BtlCharApi_IsCamShown(s32 objId);
extern s32 BtlCharApi_ObjTestFlagBit21(s32 objId);
extern s32 BtlCharApi_IsInTechnique(s32 objId);
extern s32 BtlCharApi_IsInRushSequence(s32 objId);

/* Alpha factor (0.5..1) of a flame corner: it falls off when the corner is close to the body, measured by the
   distance to the two nearest of seven model nodes. */
f32 EftAura_GetNodeFade(s32 objId, Vec4 *pos, Vec4 *nodes, f32 scale) {
    Vec4 d;
    f32 best[2] = { -1.0f, -1.0f };
    f32 dist;
    f32 ratio;
    f32 limit;
    s32 i;

    dist = 0.0f;
    ratio = 1.0f;
    for (i = 0; i < 7; i++) {
        Vec4_Sub(&d, pos, &nodes[i]);
        if (i == 0) {
            dist = Vec3_LengthSq(&d);
        } else {
            f32 t = Vec3_LengthSq(&d);
            if (t < dist) {
                dist = t;
            }
        }
        if (best[0] < 0.0f) {
            best[0] = dist;
        } else if (best[1] < 0.0f) {
            if (dist < best[0]) {
                best[1] = best[0];
                best[0] = dist;
            } else {
                best[1] = dist;
            }
        } else if (dist < best[0]) {
            best[1] = best[0];
            best[0] = dist;
        } else if (dist < best[1]) {
            best[1] = dist;
        }
    }
    best[0] = sqrtf(best[0]);
    best[1] = sqrtf(best[1]);
    dist = best[0] * gEftAuraPrm->unk18 + best[1] * (1.0f - gEftAuraPrm->unk18);
    limit = scale * 4.5f;
    if (dist < limit) {
        ratio = dist / limit;
        limit = ratio * ratio;
        limit *= ratio;
        ratio = limit * limit;
    }
    return ratio * 0.5f + 0.5f;
}

/* Draws the fighter's flames: each is a quad that stands along the flame's direction, leaning with the camera
   (EftAura_BuildFlameMtx), its two far corners stretched by the flame's end ratio. Corner alpha falls off near the
   body (EftAura_GetNodeFade), and the two near corners get a tenth of it. Flames with flag 0x800 are skipped.
   The quad goes into the order table slot of its average depth, in the layer given by the flame's `alt`.
   Matched in cleanup W1. What it took: Vu0Cur_ProjectPoint returns a value (declared `void`, the three off-screen
   tests came out with x / limit / reload register permuted); the order-table insert is the usual inline helper
   called with layer 1 or `f->alt` (it reads gOtZ in every arm); the loop is
   `for (link = &head; *link != NULL; link = &f->next) { f = *link; ...` (the test loads through `link`, the body
   assigns `f` from it again: gcse's PRE makes that a register copy, which gives `lw v0 / bnez v0 / move s1,v0`;
   with `f = f->next` or `(f = *link) != NULL` cse folds load and assignment into one `lw s1`); the depth is the plain sum of the
   four z; header stores in the order prim, tag, vif0, vif1, gif0, gif1, next; `sparkTex` is assigned after the
   camera vector; and the three flag tests inside the loop go through inline predicates written
   `if (flags & bit) return 1; return 0;`: their extra RTL instructions make the flame loop 449 instructions long
   in the second loop pass, one more than the 448 at which the 0.5f of the lean would be hoisted (it must stay in
   the loop: f4), and that form keeps the texture test's branch prediction. */
static inline s32 EftAura_FlameFlagInl(EftAuraFlame *f, u32 bit) {
    if (f->flags & bit) {
        return 1;
    }
    return 0;
}

static inline s32 EftAura_WorkFlagInl(EftAuraWork *aura, u32 bit) {
    if (aura->flags & bit) {
        return 1;
    }
    return 0;
}

/* Links a packet into the chain of a depth slot (clamped to 0..0xFFF); the same helper as EftOt_Add (eft_g.c). */
static inline void EftAura_OtAdd(OtPrim *p, s32 z, s32 layer) {
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

void EftAura_DrawFlames(EftAuraWork *aura, s32 objId, f32 alpha) {
    Mtx44 mtx;
    Vec4 *corner[2];
    Vec4 *uv[4];
    Vec4 st[4];
    Vec4 dir;
    Vec4 ndir;
    Vec4 p;
    Vec4 c;
    Vec4 camFwd;
    Vec4 away;
    u8 rgb[3];
    EftNScr scr[4];
    f32 fade[4];
    u8 al[4];
    Vec4 node[7];
    EftNTexSet *sparkTex;
    s32 clipped;
    EftAuraFlame *f;
    EftAuraFlame **link;
    EftNQuadPkt *pkt;
    s32 i;
    s32 z;
    f32 a;
    f32 w;
    f32 h;
    f32 d;

    camFwd.x = gBtlCamView->world2view2.m[0][2];
    camFwd.y = gBtlCamView->world2view2.m[1][2];
    camFwd.z = gBtlCamView->world2view2.m[2][2];
    camFwd.w = 1.0f;
    sparkTex = &gPool->grp[1].set;
    if (aura->flags & 0x80) {
        corner[0] = gData->cornerB[0];
        corner[1] = gData->cornerB[1];
    } else {
        corner[0] = gData->corner[0];
        corner[1] = gData->corner[1];
    }
    uv[0] = gData->uv[0];
    uv[1] = gData->uv[1];
    uv[2] = gData->uv[2];
    uv[3] = gData->uv[3];
    for (i = 0; i < 7; i++) {
        BtlCharApi_GetNodePos(objId, gData->fadeNode[i], &node[i]);
    }
    a = aura->alpha * aura->unk50 * alpha;
    Dbg_ProfMark(gBattleProf);
    for (link = &gPool->flameUsed; *link != NULL; link = &f->next) {
        f = *link;
        if (f->objId != objId) {
            continue;
        }
        dir.x = f->dir.x;
        clipped = 0;
        dir.y = f->dir.y;
        dir.z = f->dir.z;
        dir.w = 1.0f;
        if (gData->part[f->part].nodeRef >= 0) {
            d = __builtin_fabsf(Vec3_Dot(&camFwd, &dir));
            Vec4_Sub(&away, &aura->partRef[f->part], &aura->pos);
            away.y = 0.0f;
            Vec3_Normalize(&away, &away);
            dir.x += away.x * d * 0.5f;
            dir.z += away.z * d * 0.5f;
        }
        Vec3_Normalize(&ndir, &dir);
        EftAura_BuildFlameMtx(&mtx, &ndir, &f->pos);
        w = f->stretch * f->scale * aura->unk58;
        h = f->size * f->scale * aura->unk58;
        for (i = 0; i < 4; i++) {
            c.x = corner[f->unk4][i].x * w;
            c.y = corner[f->unk4][i].y * f->scale;
            c.z = corner[f->unk4][i].z * h;
            c.w = 1.0f;
            if (i >= 2) {
                c.z *= f->end;
            }
            Mtx_MulVec4(&p, &mtx, &c);
            Vu0Cur_ProjectPoint(&scr[i], &p);
            if (scr[i].x > 0xFFF0) {
                clipped = 1;
                break;
            }
            if (scr[i].y > 0xFFF0) {
                clipped = 1;
                break;
            }
            if (scr[i].z < 0) {
                clipped = 1;
                break;
            }
            Vec4_Scale(&st[i], &uv[f->unk4 * 2 + f->flip][i], 1.0f / (f32)scr[i].w);
            fade[i] = EftAura_GetNodeFade(f->objId, &p, node, aura->scale);
            if (i < 2) {
                fade[i] *= 0.1f;
            }
        }
        if (clipped) {
            continue;
        }
        z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
        rgb[0] = (u32)(f->color.x * 255.0f);
        rgb[1] = (u32)(f->color.y * 255.0f);
        rgb[2] = (u32)(f->color.z * 255.0f);
        for (i = 0; i < 4; i++) {
            al[i] = (u32)(fade[i] * (f->color.w * 255.0f * a));
        }
        if (EftAura_FlameFlagInl(f, 0x800)) {
            continue;
        }
        pkt = (EftNQuadPkt *)gOtCur;
        gOtCur = (u32 *)(pkt + 1);
        if (pkt == NULL) {
            return;
        }
        pkt->prim = 0x5C;
        pkt->tag = 0x20000008;
        pkt->vif0 = 0x10000000;
        pkt->vif1 = 0x50000008;
        pkt->gif0 = 0xE400000000008001;
        pkt->gif1 = 0x42142142142160;
        pkt->next = NULL;
        pkt->v[0].r = rgb[0];
        pkt->v[0].g = rgb[1];
        pkt->v[0].b = rgb[2];
        pkt->v[0].a = al[0];
        pkt->v[0].q = st[0].z;
        pkt->v[1].r = rgb[0];
        pkt->v[1].g = rgb[1];
        pkt->v[1].b = rgb[2];
        pkt->v[1].a = al[1];
        pkt->v[1].q = st[1].z;
        pkt->v[2].r = rgb[0];
        pkt->v[2].g = rgb[1];
        pkt->v[2].b = rgb[2];
        pkt->v[2].a = al[2];
        pkt->v[2].q = st[2].z;
        pkt->v[3].r = rgb[0];
        pkt->v[3].g = rgb[1];
        pkt->v[3].b = rgb[2];
        pkt->v[3].a = al[3];
        pkt->v[3].q = st[3].z;
        pkt->v[0].s = st[0].x;
        pkt->v[0].t = st[0].y;
        pkt->v[1].s = st[1].x;
        pkt->v[1].t = st[1].y;
        pkt->v[2].s = st[2].x;
        pkt->v[2].t = st[2].y;
        pkt->v[3].s = st[3].x;
        pkt->v[3].t = st[3].y;
        pkt->v[0].x = scr[0].x;
        pkt->v[0].y = scr[0].y;
        pkt->v[0].z = scr[0].z;
        pkt->v[0].f = 0xFF;
        pkt->v[1].x = scr[1].x;
        pkt->v[1].y = scr[1].y;
        pkt->v[1].z = scr[1].z;
        pkt->v[1].f = 0xFF;
        pkt->v[2].x = scr[2].x;
        pkt->v[2].y = scr[2].y;
        pkt->v[2].z = scr[2].z;
        pkt->v[2].f = 0xFF;
        pkt->v[3].x = scr[3].x;
        pkt->v[3].y = scr[3].y;
        pkt->v[3].z = scr[3].z;
        pkt->v[3].f = 0xFF;
        if (EftAura_FlameFlagInl(f, 0x800)) {
            pkt->tex0 = sparkTex->entry[0].tex0;
        } else if (EftAura_WorkFlagInl(aura, 0x80)) {
            pkt->tex0 = aura->tex[f->tex + 1];
        } else {
            pkt->tex0 = aura->tex[0];
        }
        if (BtlScene_IsStageFlagOn()) {
            EftAura_OtAdd((OtPrim *)pkt, z, 1);
        } else {
            EftAura_OtAdd((OtPrim *)pkt, z, f->alt);
        }
    }
    Dbg_ProfColor(gBattleProf, 0x8000FFFF);
}

/* GS TEX0 values for this frame: the aura's flame sheet(s), and once per frame every entry of the spark set. */
void EftAura_UpdateTextures(EftAuraWork *aura) {
    EftNTexSet *set = &gPool->grp[0].set;
    s32 *ready;
    u64 base;
    s32 i;

    if (aura->flags & 0x80) {
        base = EftVram_AddImage(&set->entry[8], 1, 0);
        for (i = aura->texFirst; i < aura->texFirst + aura->texCount; i++) {
            ((EftAuraWork *)((u8 *)aura + 8))->tex[i - aura->texFirst] = base | ((u64)EftVram_AddClut(&set->entry[i]) << 37);
        }
    } else {
        aura->tex[0] = EftVram_AddImage(&set->entry[0], 1, 0);
        aura->tex[0] |= (u64)EftVram_AddClut(&set->entry[aura->type]) << 37;
    }
    ready = &gPool->grp[1].ready;
    if (!*ready) {
        set = &gPool->grp[1].set;
        for (i = 0; i < set->count; i++) {
            set->entry[i].tex0 = EftVram_AddTex(&set->entry[i], 1, 0);
        }
        *ready = 1;
    }
}

/* Starts the fighter's body lightning when its character parameter flags have bit 4 or 8, else stops it. */
void EftAura_UpdateLightning(s32 objId) {
    if (BtlCharApi_ObjGetParamFlags0(objId) & 0xC) {
        EftBolt_Enable(objId);
    } else {
        EftBolt_Disable(objId);
    }
}

/* Selects the texture pair and the colours for an aura type. Types 8..10 use two textures and the second colour
   table.
   Notes for the match: `colorEnd.w = 0.0f` stands in both arms of the helper (the compiler merges the two
   tails, and with them the second Vec4_Scale call); the colour count is decided by testing the flag word
   itself and `multi` is taken from it again afterwards (the original tests the same register twice in a row);
   the three plain stores are in this order. */
/* Converts a 0..255 colour to 0..1. */
#define EftAura_ScaleColor(dst, src) Vec4_Scale(dst, src, 1.0f / 255.0f)

/* Copies the colours of the aura's type from the parameter file. */
static inline void EftAura_LoadColors(EftAuraWork *aura, s32 alt, s32 multi) {
    s32 i;
    s32 j;

    if (multi) {
        EftAura_ScaleColor(&aura->colorStart, &gData->colorB[alt][0]);
        Vec4_Scale(&aura->colorEnd, &gData->colorB[alt][0], 1.0f / 255.0f);
        aura->colorEnd.w = 0.0f;
    } else {
        EftAura_ScaleColor(&aura->colorStart, &gData->color[aura->type][0]);
        Vec4_Scale(&aura->colorEnd, &gData->color[aura->type][0], 1.0f / 255.0f);
        aura->colorEnd.w = 0.0f;
    }
    for (i = 0; i < 4; i++) {
        if (aura->flags & 0x80) {
            EftAura_ScaleColor(&aura->colorAlt[i], &gData->colorB[alt][1 + i]);
            EftAura_ScaleColor(&aura->sparkColor[i], &gData->colorB[alt][5 + i]);
            EftAura_ScaleColor(&aura->sparkColorAlt[i], &gData->colorB[alt][9 + i]);
        } else {
            if (aura->type < 8) {
                j = 0;
            } else {
                j = i;
            }
            EftAura_ScaleColor(&aura->colorAlt[i], &gData->color[aura->type][1 + j]);
            EftAura_ScaleColor(&aura->sparkColor[i], &gData->color[aura->type][5 + j]);
            EftAura_ScaleColor(&aura->sparkColorAlt[i], &gData->color[aura->type][9 + j]);
        }
    }
    alt = 0;
}

void EftAura_SetType(EftAuraWork *aura, s32 objId, s32 type, s32 paramFlags) {
    s32 alt = 0;
    s32 multi;

    aura->flags &= ~0x80;
    if ((u32)(type - 8) < 3) {
        aura->flags |= 0x80;
        aura->texFirst = 8;
        aura->texCount = 2;
    }
    switch (type) {
    case 8:
        aura->texFirst = 8;
        aura->texCount = 2;
        break;
    case 9:
        aura->texFirst = 10;
        aura->texCount = 2;
        alt = 2;
        break;
    case 10:
        aura->texFirst = 12;
        aura->texCount = 2;
        alt = 3;
        break;
    }
    aura->type = type;
    aura->unk14 = type;
    aura->colorCount = 1;
    if (aura->flags & 0x80) {
        aura->colorCount = 2;
    }
    multi = aura->flags & 0x80;
    EftAura_LoadColors(aura, alt, multi);
}

/* Reads the fighter's aura type and height: called at creation and again every frame. */
void EftAura_Setup(EftAuraWork *aura, s32 objId) {
    s32 flags = BtlCharApi_ObjGetParamFlags0(objId);

    EftAura_SetType(aura, objId, BtlCharApi_GetAuraType(objId), flags);
    aura->scale = BtlCharApi_GetHeight(objId) / 19.35f;
    if (aura->scale < 0.7f) {
        aura->scale = 0.7f;
    }
}

/* Task init: clears the work, keeps the argument block and starts the aura with a fade-in. */
void EftAuraTask_Init(EftNTask *task, EftAuraArg *arg) {
    EftAuraWork *aura = task->work;

    memset(aura, 0, sizeof(EftAuraWork));
    aura->arg = *arg;
    aura->level = 1.0f;
    aura->alpha = 0.0f;
    aura->flags |= 8;
    aura->partsStarted = 0;
    aura->flameCount = 0;
    EftAura_Setup(aura, arg->objId);
    EftAura_Start((EftAura *)aura, (s32 *)arg, 1);
}

/* Task term: returns the fighter's flames and sparks to the pools and clears its entry of the task table. */
void EftAuraTask_Term(EftNTask *task) {
    EftAuraWork *aura = task->work;
    s32 *objId;

    aura->flags = 0;
    objId = &aura->arg.objId;
    EftAura_FreeFlames(*objId);
    EftAura_FreeSparks(*objId);
    gPool->tasks[*objId] = NULL;
}

/* Task update: samples the fighter's model nodes, steps the state machines and particles (not while paused), and
   kills the task once the aura has finished. */
void EftAuraTask_Update(EftNTask *task) {
    EftAuraWork *aura = task->work;
    s32 *objId = &aura->arg.objId;
    s32 i;

    if (BtlCharApi_IsModelNew(*objId)) {
        aura->flags |= 0x40;
    }
    EftAura_Setup(aura, *objId);
    aura->flags &= ~0x40;
    if (!(Battle_GetWork()->flags & 0x100)) {
        for (i = 0; i < EFT_AURA_SPARKS; i++) {
            BtlCharApi_GetNodePos(*objId, gData->spark[i].node, &aura->sparkPos[i]);
        }
        BtlCharApi_GetNodePos(*objId, 3, &aura->pos);
        BtlCharApi_GetNodePos(*objId, 0x30, &aura->unk310);
        BtlCharApi_GetNodePos(*objId, 0x11, &aura->axis);
        for (i = 0; i < EFT_AURA_PARTS; i++) {
            BtlCharApi_GetNodePos(*objId, gData->part[i].node, &aura->part[i]);
        }
        for (i = 0; i < EFT_AURA_PARTS; i++) {
            BtlCharApi_GetNodePos(*objId, gData->part[i].nodeEnd, &aura->partEnd[i]);
        }
        for (i = 0; i < EFT_AURA_PARTS; i++) {
            BtlCharApi_GetNodePos(*objId, gData->part[i].nodeRef, &aura->partRef[i]);
        }
        if (aura->flags & 8) {
            Vec4_Copy(&aura->prevPos, &aura->pos);
            for (i = 0; i < EFT_AURA_PARTS; i++) {
                Vec4_Copy(&aura->partPrev[i], &aura->part[i]);
            }
            aura->flags &= ~8;
        }
        EftAura_UpdateState((EftAura *)aura, *objId);
        EftAura_UpdateSparks((EftAura *)aura, *objId, &aura->sparkState);
        Vec4_Copy(&aura->prevPos, &aura->pos);
        for (i = 0; i < EFT_AURA_PARTS; i++) {
            Vec4_Copy(&aura->partPrev[i], &aura->part[i]);
        }
        aura->frame++;
    }
    if (aura->flags & 2) {
        BtlTask_SetDead(task);
    } else {
        EftAura_UpdateTextures(aura);
    }
}

/* Task post-update: nothing (it only fetches the battle work). */
void EftAuraTask_PostUpdate(EftNTask *task) {
    if (Battle_GetWork()->flags & 0x100) {
    }
}

/* Task reset: kills the task. */
void EftAuraTask_Reset(EftNTask *task) {
    BtlTask_SetDead(task);
}

/* Task draw: the flames, unless the fighter is hidden, in a technique or rush sequence, or the aura is hidden or of
   the undrawn variant. At 30% alpha when the fighter is the transparent one in front of the camera. */
void EftAuraTask_Draw(EftNTask *task) {
    f32 alpha = 1.0f;
    EftAuraWork *aura = task->work;
    s32 *objId = &aura->arg.objId;

    if (BtlCharApi_IsHidden(*objId)) {
        return;
    }
    if (aura->flags & 4) {
        return;
    }
    if (BtlCharApi_IsCamShown(*objId) && BtlCharApi_ObjTestFlagBit21(*objId)) {
        alpha = 0.3f;
    }
    if (BtlCharApi_IsInTechnique(*objId)) {
        return;
    }
    if (BtlCharApi_IsInRushSequence(*objId)) {
        return;
    }
    if (EftGlow_IsActive(*objId)) {
        return;
    }
    if (aura->flags & 0x20) {
        return;
    }
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    EftAura_DrawFlames(aura, *objId, alpha);
    Vu0Cur_Pop();
}

/* Manager init: the pool header, 50 flames and 18 sparks per character, the task table, the two texture sets
   (common entries 3 and 2), the parameter file (common entry 1) and the list the aura tasks live in. */
void EftAuraMgr_Init(EftNTask *task) {
    s32 i;

    gEftAuraPool = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftAuraMgr));
    memset(gEftAuraPool, 0, sizeof(EftAuraMgr));
    gPool->count = BtlScene_GetCharCount();
    gPool->flameMax = gPool->count * 50;
    gPool->sparkMax = gPool->count * 18;
    gPool->flameFree = NULL;
    gPool->flameUsed = NULL;
    gPool->flameNext = 0;
    gPool->flames = BtlPool_Alloc(BtlPool_GetCurrent(), gPool->flameMax * sizeof(EftAuraFlame));
    memset(gPool->flames, 0, gPool->flameMax * sizeof(EftAuraFlame));
    gPool->sparkFree = NULL;
    gPool->sparkUsed = NULL;
    gPool->sparkNext = 0;
    gPool->sparks = BtlPool_Alloc(BtlPool_GetCurrent(), gPool->sparkMax * sizeof(EftAuraSpark));
    memset(gPool->sparks, 0, gPool->sparkMax * sizeof(EftAuraSpark));
    gPool->tasks = BtlPool_Alloc(BtlPool_GetCurrent(), gPool->count * 4);
    memset(gPool->tasks, 0, gPool->count * 4);
    gPool->grp[0].data = BtlScene_GetCommonEntry(3);
    gPool->grp[1].data = BtlScene_GetCommonEntry(2);
    for (i = 0; i < 2; i++) {
        EftTexSet_Load32(&gPool->grp[i].set, gPool->grp[i].data);
    }
    gPool->data = BtlScene_GetCommonEntry(1);
    gEftAuraPrm = (EftAuraPrm *)(gEftAuraCfg = gPool->data); /* 0x2FEA08 is stored first */
    for (i = 0; i < gPool->count; i++) {
        gPool->tasks[i] = NULL;
    }
    gEftAuraTaskList = BtlTask_CreateChildList(task, gPool->count, sizeof(EftAuraWork));
}

/* Manager term: frees everything the init allocated. */
void EftAuraMgr_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gPool->tasks);
    BtlPool_Free(BtlPool_GetCurrent(), gPool->flames);
    BtlPool_Free(BtlPool_GetCurrent(), gPool->sparks);
    BtlPool_Free(BtlPool_GetCurrent(), gEftAuraPool);
    gEftAuraPool = NULL;
}

/* Manager update: marks the texture sets stale, and starts or stops each character's body lightning on the first
   frame, on the first frame after the stage became ready again, and whenever a character's model changed. */
void EftAuraMgr_Update(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        gPool->grp[i].ready = 0;
    }
    if (!BtlStage_IsReady()) {
        gPool->waitStage = 1;
    }
    if (gPool->waitStage) {
        if (BtlStage_IsReady()) {
            gPool->started = 0;
            gPool->waitStage = 0;
        }
    }
    for (i = 0; i < gPool->count; i++) {
        if (!gPool->started || BtlCharApi_IsModelNew(i)) {
            EftAura_UpdateLightning(i);
        }
    }
    gPool->started = 1;
}

/* Empty task callback. */
void EftAuraMgr_Stub(void) {
}

/* Creates the fighter's aura task, or restarts the aura of the existing one with the new variant (without a new
   fade-in). Returns 0 only when no task could be created. */
s32 EftAura_Request(s32 objId, s32 variant) {
    EftAuraArg arg;
    EftNTask *task;
    EftAuraWork *aura;

    if (gPool->tasks[objId] == NULL) {
        arg.objId = objId;
        arg.variant = variant;
        task = BtlTaskList_AddTail(gEftAuraTaskList, gEftAuraTaskClass, &arg);
        if (task != NULL) {
            gPool->tasks[objId] = task;
        } else {
            return 0;
        }
    } else {
        aura = gPool->tasks[objId]->work;
        aura->arg.variant = variant;
        EftAura_Start((EftAura *)aura, (s32 *)&aura->arg, 0);
    }
    return 1;
}

/* Variant 0: starts the final fade-out (the task ends with it) unless the aura is of the other variant. Other
   variants only clear flag 0x20. */
s32 EftAura_Stop(s32 objId, s32 variant) {
    EftNTask *task = gPool->tasks[objId];
    EftAuraWork *aura;

    if (task != NULL) {
        aura = task->work;
        if (!(aura->flags & 1)) {
            return 0;
        }
        if (aura->fade == 3) {
            return 0;
        }
        if (variant == 0) {
            if (!(aura->flags & 0x20)) {
                aura->fade = 3;
                aura->burst = 0;
                aura->burstTime = 0.0f;
            }
        } else {
            if (aura->flags & 0x20) {
                aura->flags &= ~0x20;
            }
        }
    }
    return 1;
}

/* Ends the aura at once: its task dies on its next update. */
s32 EftAura_Kill(s32 objId) {
    EftNTask *task = gPool->tasks[objId];
    EftAuraWork *aura;

    if (task != NULL) {
        aura = task->work;
        if (!(aura->flags & 1)) {
            return 0;
        }
        aura->flags = 2;
    }
    return 1;
}

/* Sets the aura strength (the fighter's ki ratio). */
void EftAura_SetLevel(s32 objId, f32 level) {
    EftNTask *task;

    if (gEftAuraPool != NULL) {
        task = gPool->tasks[objId];
        if (task != NULL) {
            ((EftAuraWork *)task->work)->level = level;
        }
    }
}

/* 1 when the fighter has an active aura that is not fully faded out. */
s32 EftAura_IsVisible(s32 objId) {
    s32 ret = 0;
    EftNTask *task;
    EftAuraWork *aura;

    if (gEftAuraPool == NULL) {
        return 0;
    }
    task = gPool->tasks[objId];
    if (task != NULL) {
        aura = task->work;
        if (aura->flags & 1) {
            if (!(aura->alpha <= 0.0f)) {
                ret = 1;
            }
        }
    }
    return ret;
}

/* 1 when the fighter's visible aura is in the rising phase of a burst. */
s32 EftAura_IsBurstRising(s32 objId) {
    s32 ret = 0;
    EftNTask *task;
    EftAuraWork *aura;

    if (gEftAuraPool == NULL) {
        return 0;
    }
    task = gPool->tasks[objId];
    if (task != NULL) {
        aura = task->work;
        if (aura->flags & 1) {
            if (!(aura->alpha <= 0.0f)) {
                ret = aura->burst == 1;
            }
        }
    }
    return ret;
}

/* Starts a burst (burst state 1). */
s32 EftAura_StartBurst(s32 objId) {
    EftNTask *task = gPool->tasks[objId];
    EftAuraWork *aura;

    if (task != NULL) {
        aura = task->work;
        if (!(aura->flags & 1)) {
            return 0;
        }
        aura->burstTime = 0.0f;
        aura->burst = 1;
    }
    return 1;
}

/* Ends a burst (burst state 4). */
s32 EftAura_EndBurst(s32 objId) {
    EftNTask *task = gPool->tasks[objId];
    EftAuraWork *aura;

    if (task != NULL) {
        aura = task->work;
        if (!(aura->flags & 1)) {
            return 0;
        }
        aura->burstTime = 0.0f;
        aura->burst = 4;
    }
    return 1;
}

/* Jumps to the peak of a burst (state 2, full burst level, flame life x 1.2). */
s32 EftAura_PeakBurst(s32 objId) {
    EftNTask *task = gPool->tasks[objId];
    EftAuraWork *aura;

    if (task != NULL) {
        aura = task->work;
        if (!(aura->flags & 1)) {
            return 0;
        }
        aura->lifeScale = 1.2f;
        aura->burstLevel = 1.0f;
        aura->burstTime = 0.0f;
        aura->burst = 2;
    }
    return 1;
}

/* Starts a fade-in, unless the final fade-out is running. */
s32 EftAura_FadeIn(s32 objId) {
    EftNTask *task = gPool->tasks[objId];
    EftAuraWork *aura;

    if (task != NULL) {
        aura = task->work;
        if (!(aura->flags & 1)) {
            return 0;
        }
        if (aura->fade == 3) {
            return 0;
        }
        aura->fade = 1;
    }
    return 1;
}

/* Starts a fade-out that keeps the task, unless the final fade-out is running. */
s32 EftAura_FadeOut(s32 objId) {
    EftNTask *task = gPool->tasks[objId];
    EftAuraWork *aura;

    if (task != NULL) {
        aura = task->work;
        if (!(aura->flags & 1)) {
            return 0;
        }
        if (aura->fade == 3) {
            return 0;
        }
        aura->fade = 2;
    }
    return 1;
}

/* What the fighter effect layer calls: 0 start, 1 stop, 2 burst start, 3 burst end, 4 burst peak, 5 fade in,
   6 fade out, 7 kill. */
s32 EftAura_Command(s32 objId, s32 cmd) {
    if (gEftAuraPool == NULL) {
        return 0;
    }
    switch (cmd) {
    case 0:
        EftAura_Request(objId, 0);
        break;
    case 1:
        EftAura_Stop(objId, 0);
        break;
    case 2:
        EftAura_StartBurst(objId);
        break;
    case 3:
        EftAura_EndBurst(objId);
        break;
    case 4:
        EftAura_PeakBurst(objId);
        break;
    case 5:
        EftAura_FadeIn(objId);
        break;
    case 6:
        EftAura_FadeOut(objId);
        break;
    case 7:
        EftAura_Kill(objId);
        break;
    }
    return 1;
}

/* Clears the hidden flag of the aura of fighter 0 or 1. */
void EftAura_Show(s32 objId) {
    EftNTask *task;
    EftAuraWork *aura;

    if (gEftAuraPool != NULL && objId < 2) {
        task = gPool->tasks[objId];
        if (task != NULL) {
            aura = task->work;
            if (aura->flags & 1) {
                aura->flags &= ~4;
            }
        }
    }
}

/* Sets the hidden flag: the flames keep updating but are not drawn. */
void EftAura_Hide(s32 objId) {
    EftNTask *task;
    EftAuraWork *aura;

    if (gEftAuraPool != NULL && objId < 2) {
        task = gPool->tasks[objId];
        if (task != NULL) {
            aura = task->work;
            if (aura->flags & 1) {
                aura->flags |= 4;
            }
        }
    }
}

/* Sets flag 0x40 (the update sets the same flag when the fighter's model changed, and clears it again). */
void EftAura_MarkModelNew(s32 objId) {
    EftNTask *task;
    EftAuraWork *aura;

    if (gEftAuraPool != NULL && objId < 2) {
        task = gPool->tasks[objId];
        if (task != NULL) {
            aura = task->work;
            if (aura->flags & 1) {
                aura->flags |= 0x40;
            }
        }
    }
}

/* Changes the type of the aura of fighter 0 or 1. Not void in the original (the call is not a tail call).
   Matches only while EftAura_SetType is defined in C above it (with only a prototype, three branches come out
   as branch-likely): that is what the ASM_STUB_BEGIN / ASM_STUB_END around its attempt is for. */
s32 EftAura_ChangeType(s32 objId, s32 type, s32 paramFlags) {
    EftNTask *task;

    if (objId < 2) {
        if (gEftAuraPool != NULL && type >= 0) {
            task = gPool->tasks[objId];
            if (task != NULL) {
                EftAura_SetType(task->work, objId, type, paramFlags);
            }
        }
    }
}

/* Colour table entry of an aura type: the second table when the parameter flags have any of 0x5E. */
EftAuraColors *EftAura_GetColors(s32 type, s32 paramFlags) {
    if (gEftAuraPool == NULL) {
        return NULL;
    }
    if (type < 0) {
        return NULL;
    }
    if (!(paramFlags & 0x5E)) {
        if (type >= 9) {
            return NULL;
        }
        return (EftAuraColors *)gData->color[type];
    }
    if (type >= 5) {
        return NULL;
    }
    return (EftAuraColors *)gData->colorB[type];
}

/* Sets the finished flag: the task dies on its next update. */
s32 EftAura_Finish(s32 objId) {
    EftNTask *task = gPool->tasks[objId];

    if (task != NULL) {
        ((EftAuraWork *)task->work)->flags |= 2;
    }
    return 1;
}

/* A second dispatcher, for the variant-1 aura: 0 request, 1 stop, 2 finish. */
s32 EftAura_CommandAlt(s32 objId, s32 cmd) {
    switch (cmd) {
    case 0:
        EftAura_Request(objId, 1);
        break;
    case 1:
        EftAura_Stop(objId, cmd);
        break;
    case 2:
        EftAura_Finish(objId);
        break;
    }
    return 1;
}

/* Takes up to `count` free joints from the pool (searching once around the ring from the last position) and chains
   them into the bolt. */
s32 EftBolt_AllocSegs(EftBolt *bolt, s32 count) {
    EftBoltSeg *prev = NULL;
    s32 first = 1;
    s32 tries;
    s32 got = 0;
    s32 idx;
    EftBoltSeg *seg;

    idx = gEftBoltPool->segNext;
    for (tries = 0; tries < gEftBoltPool->segMax; tries++) {
        seg = (EftBoltSeg *)(idx * sizeof(EftBoltSeg) + (u32)gEftBoltPool->segs);
        if (seg->flags == 0) {
            seg->next = NULL;
            if (first) {
                bolt->head = seg;
                first = 0;
            } else {
                prev->next = seg;
            }
            prev = seg;
            gEftBoltPool->segNext = idx + 1;
            if (!(gEftBoltPool->segNext < gEftBoltPool->segMax)) {
                gEftBoltPool->segNext = 0;
            }
            got++;
            if (got >= count) {
                break;
            }
        }
        idx++;
        if (idx >= gEftBoltPool->segMax) {
            idx = 0;
        }
    }
    bolt->count = got;
    bolt->shown = 0;
    return 1;
}

/* Returns the bolt's joints to the pool. */
void EftBolt_FreeSegs(EftBolt *bolt) {
    EftBoltSeg *seg;
    EftBoltSeg *next;

    if (bolt->head != NULL) {
        seg = bolt->head;
        do {
            next = seg->next;
            seg->flags = 0;
            seg = next;
        } while (next != NULL);
    }
    bolt->head = NULL;
}

/* Lays the bolt's joints out between `start` and `end` (relative to the bolt's origin): a random zigzag across the
   bolt's normal that bulges along it, and the colour, width and alpha of every joint.
   Not matching (196 instructions out of place after alignment): the statements, the frame and the order of calls
   are right; the callee-saved float registers are assigned differently (the original keeps `amp` in $f22, the
   rand() divisor in $f24 and uses eleven of them, this attempt twelve) and the vector sums are scheduled in another
   order. The if / else with identical arms is deliberate: the original has a `c.lt.s amp, 0` whose branch is gone. */
#if 0
void EftBolt_Shape(EftBolt *bolt, EftVec start, EftVec end, f32 scale) {
    Vec4 p0;
    Vec4 p1;
    Vec4 off;
    Vec4 step;
    Vec4 side;
    Vec4 up;
    Vec4 dir;
    f32 amp = 1.0f;
    f32 bright;
    f32 r1;
    f32 r2;
    f32 t;
    s32 count;
    s32 thick = 1;
    s32 n = 0;
    EftBoltSeg *seg;
    EftBoltSeg **link;

    r1 = rand();
    r2 = rand();
    count = bolt->count;
    r1 /= RAND_MAX_F;
    Vec4_Sub(&dir, V(&end), V(&start));
    Vec3_Normalize(&dir, &dir);
    Vec3_Cross(&side, &dir, &bolt->normal);
    r2 /= RAND_MAX_F;
    Vec3_Cross(&up, &side, &dir);
    r1 += amp;
    Vec4_Copy(&p0, V(&start));
    r1 *= scale;
    r2 += amp;
    Vec4_Copy(&p1, V(&end));
    bright = 1.0f;
    amp = scale;
    r2 *= scale;
    p1.x += up.x * r1;
    p1.y += up.y * r1;
    p1.z += up.z * r1;
    off.x = up.x * r2;
    off.y = up.y * r2;
    off.z = up.z * r2;
    off.w = bright;
    link = &bolt->head;
    while (*link != NULL) {
        seg = *link;
        seg->flags |= 1;
        if (n == 0) {
            t = (RANDF() - RANDF()) * 0.2f * scale;
            off.x += side.x * t;
            off.y += side.y * t;
            off.z += side.z * t;
        } else {
            count--;
            t = (RANDF() * 0.5f + 0.3f) * amp;
            amp = -amp;
            if (count <= 0) {
                count = 1;
            }
            off.x += side.x * t;
            off.y += side.y * t;
            off.z += side.z * t;
            Vec4_Add(&step, &p0, &off);
            Vec4_Sub(&step, &p1, &step);
            step.x /= count;
            step.y /= count;
            step.z /= count;
            t = (1.0f - (f32)((count - 1) / bolt->count)) * 0.6f + 0.4f;
            t *= RANDF() * 0.2f + 0.85f;
            off.x += step.x * t;
            off.y += step.y * t;
            off.z += step.z * t;
        }
        seg->pos.x = off.x;
        seg->pos.y = off.y;
        seg->pos.z = off.z;
        seg->pos.w = 1.0f;
        t = (f32)(count / bolt->count);
        r1 = RANDF();
        if (amp < 0.0f) {
            r1 = (r1 * 0.5f + 0.3f) * t * amp;
            off.x += side.x * r1;
            off.y += side.y * r1;
            off.z += side.z * r1;
            r1 = (RANDF() * 0.4f + 0.1f) * t * amp;
            off.x += up.x * r1;
            off.y += up.y * r1;
            off.z += up.z * r1;
        } else {
            r1 = (r1 * 0.5f + 0.3f) * t * amp;
            off.x += side.x * r1;
            off.y += side.y * r1;
            off.z += side.z * r1;
            r1 = (RANDF() * 0.4f + 0.1f) * t * amp;
            off.x += up.x * r1;
            off.y += up.y * r1;
            off.z += up.z * r1;
        }
        r1 = (RANDF() * 0.4f + 0.6f) * scale;
        off.x += up.x * r1;
        off.y += up.y * r1;
        off.z += up.z * r1;
        seg->r = bright * 50.0f / 255.0f;
        seg->g = bright * 30.0f / 255.0f;
        seg->b = bright * 150.0f / 255.0f;
        seg->alphaMax = 150.0f / 255.0f;
        seg->alpha = 0.0f;
        if (n != 0 && seg->next != NULL) {
            thick++;
            seg->flags |= 0x20;
            if (thick == 1) {
                seg->widthA = RANDF() * 0.03f + 0.05f;
                seg->widthB = RANDF() * 0.03f + 0.05f;
            } else {
                seg->widthA = RANDF() * 0.05f + 0.11f;
                seg->widthB = RANDF() * 0.05f + 0.11f;
            }
            if (thick >= 3) {
                thick = 0;
            }
        } else {
            seg->widthA = seg->widthB = 0.02f;
        }
        n++;
        seg->widthA *= scale * 3.0f;
        seg->widthB *= scale * 3.0f;
        link = &seg->next;
    }
}
#else
LIT4_WORD(D_002FC9B8, 0x4EFFFFFF);
LIT4_WORD(D_002FC9BC, 0x3D4CCCCC);
LIT4_WORD(D_002FC9C0, 0x3ECCCCCC);
LIT4_WORD(D_002FC9C4, 0x3CF5C28F);
LIT4_WORD(D_002FC9C8, 0x3DE147AE);
LIT4_WORD(D_002FC9CC, 0x3E4CCCCC);
LIT4_WORD(D_002FC9D0, 0x3E999999);
LIT4_WORD(D_002FC9D4, 0x3F199999);
LIT4_WORD(D_002FC9D8, 0x3E4CCCCC);
LIT4_WORD(D_002FC9DC, 0x3F599999);
LIT4_WORD(D_002FC9E0, 0x4EFFFFFF);
LIT4_WORD(D_002FC9E4, 0x3E999999);
LIT4_WORD(D_002FC9E8, 0x3DCCCCCC);
LIT4_WORD(D_002FC9EC, 0x4EFFFFFF);
LIT4_WORD(D_002FC9F0, 0x3F199999);
LIT4_WORD(D_002FC9F4, 0x3F169696);
LIT4_WORD(D_002FC9F8, 0x3CA3D70A);
INCLUDE_ASM("asm/nonmatchings/battle/eft_n", EftBolt_Shape);
#endif

/* One frame of a bolt's joints: reveals them (from the start, or towards the end once the bolt is retracting),
   gives each a two-frame flash, then lets it drift and fade. Returns 1 when the last joint has faded. */
s32 EftBolt_Step(EftBolt *bolt, f32 scale) {
    Vec4 d;
    EftBoltSeg *seg;
    EftBoltSeg *next;
    EftBoltSeg **link;
    s32 n = 0;
    s32 done = 0;
    s32 flags;
    f32 t;
    f32 limit;

    memset(&d, 0, sizeof(Vec4));
    link = &bolt->head;
    while (*link != NULL) {
        seg = *link;
        next = seg->next;
        if (bolt->flags & 2) {
            seg->flags |= 2;
            if (!(seg->flags & 0x10)) {
                if (!(bolt->count - bolt->shown < n)) {
                    seg->flags |= 0x10;
                }
            }
        } else {
            if (!(bolt->shown < n)) {
                seg->flags |= 2;
            }
        }
        flags = seg->flags;
        if (!(flags & 8)) {
            if (flags & 2) {
                if (!(flags & 4)) {
                    seg->alpha = seg->alphaMax * 0.35f;
                    if (flags & 0x20) {
                        if (bolt->kind < 4) {
                            seg->widthA *= 5.5f;
                            seg->widthB *= 5.5f;
                        } else {
                            seg->widthA *= 2.75f;
                            seg->widthB *= 2.75f;
                        }
                    }
                    seg->flags = flags | 4;
                } else {
                    seg->alpha = seg->alphaMax;
                    if (flags & 0x20) {
                        if (bolt->kind < 4) {
                            seg->widthA *= 0.3f;
                            seg->widthB *= 0.3f;
                        } else {
                            seg->widthA *= 0.61f;
                            seg->widthB *= 0.61f;
                        }
                    }
                    seg->flags = flags | 8;
                }
            }
        } else if (flags & 0x10) {
            seg->widthA *= 0.8f;
            seg->widthB *= 0.8f;
            if (next != NULL) {
                Vec4_Sub(&d, &next->pos, &seg->pos);
            } else {
                t = RANDF() * 0.45f;
                d.x = bolt->dir.x * t;
                d.y = bolt->dir.y * t;
                d.z = bolt->dir.z * t;
                t = RANDF() * 0.35f - RANDF() * 0.35f - 0.3f;
                d.x += bolt->normal.x * t;
                d.y += bolt->normal.y * t;
                d.z += bolt->normal.z * t;
                Vec3_Normalize(&d, &d);
                Vec4_Scale(&d, &d, scale * 1.1f);
            }
            seg->pos.x += d.x;
            seg->pos.y += d.y;
            seg->pos.z += d.z;
            seg->alpha -= seg->alphaMax * 0.25f;
            limit = seg->alphaMax * 0.1f;
            if (seg->alpha <= limit) {
                seg->alpha = limit;
                if (!(n < bolt->count - 1)) {
                    done = 1;
                }
            }
        }
        link = &seg->next;
        n++;
    }
    return done;
}

/* Draws a bolt: for each shown joint that has a successor, a camera-facing quad from this joint's width to the
   next one's, coloured per joint, sharing its far edge with the next quad.
   Not matching: 479 instructions against 481, 42 out of place after a register-masked alignment (61 before cleanup
   W1). Now in the attempt, all taken from the matched EftAura_DrawFlames: Vu0Cur_ProjectPoint returns a value; the
   order-table insert is the inline helper with layer 1; the loop is `for (link = &head; *link != NULL; link =
   &seg->next) { seg = *link; ...` with `continue` (gives the `lw v0,0x30(s6) / bnez v0 / move s6,v0` step); the
   depth is the plain sum `scr[0].z + scr[1].z + scr[2].z + scr[2].w` (the last term is the original's typo for
   scr[3].z); the uv stores are in plain field order.
   What still differs: (1) the corner loop. The original walks TWO reduced pointers, s1 = &scr[i] (the call
   argument, .w at 12(s1), .x at 0(s1), advanced right behind the x test) and s2 = &scr[i].z (.y at -4(s2), .z at
   0(s2), advanced behind the z test), increments i at the TOP of the loop (`addiu s4,s4,1` first, with i * 16
   already in s0 for quad / st / uv) and computes `i < 4` in front of the three tests (`slti v1,s4,4` before the
   x read): the increment is in front of the tests in the source. This attempt indexes scr and increments at the
   bottom; `ps = scr; ... ps++` in the loop header is far worse (115). (2) &prev[0] (sp + 64) is recomputed at each
   use in the original and &prev[1] (s8 = sp + 80) is set in both arms of `if (first)`; here both are hoisted into
   saved registers, which also swaps s5 / s6 between `seg` and &side. (3) the `bnel` in front of `pkt = gOtCur`. */
#if 0
void EftBolt_Draw(EftBoltWork *work, EftBolt *bolt, f32 alpha) {
    Vec4 quad[4];
    Vec4 prev[2];
    Vec4 st[4];
    Vec4 uv[4];
    Vec4 org;
    Vec4 p;
    Vec4 cam;
    Vec4 side;
    Vec4 toCam;
    Vec4 tmp;
    EftNScr scr[4];
    s32 col0[4];
    s32 col1[4];
    s32 clipped;
    s32 first;
    s32 n;
    s32 n1;
    EftBoltSeg *seg;
    EftBoltSeg *next;
    EftBoltSeg **link;
    EftNQuadPkt *pkt;
    s32 i;
    s32 z;

    Vec4_Copy(&cam, &gBtlCamView->pos);
    Vec4_Add(&org, &bolt->pos, &bolt->start);
    first = 1;
    n = 0;
    for (link = &bolt->head; *link != NULL; link = &seg->next) {
        seg = *link;
        next = seg->next;
        if (next == NULL) {
            continue;
        }
        if (!(seg->flags & 2)) {
            continue;
        }
        Vec4_Sub(&side, &next->pos, &seg->pos);
        clipped = 0;
        Vec4_Add(&p, &org, &seg->pos);
        p.w = 1.0f;
        Vec4_Sub(&toCam, &p, &cam);
        Vec3_Cross(&side, &side, &toCam);
        Vec3_Normalize(&side, &side);
        if (first) {
            Vec3_Scale(&tmp, &side, seg->widthB);
            Vec4_Add(&quad[0], &p, &tmp);
            quad[0].w = 1.0f;
            Vec3_Scale(&tmp, &side, -seg->widthA);
            Vec4_Add(&quad[1], &p, &tmp);
            quad[1].w = 1.0f;
        } else {
            Vec4_Copy(&quad[0], &prev[0]);
            Vec4_Copy(&quad[1], &prev[1]);
        }
        n1 = n + 1;
        Vec3_Scale(&tmp, &side, next->widthB);
        Vec4_Add(&quad[2], &p, &tmp);
        quad[2].w = 1.0f;
        first = 0;
        Vec3_Scale(&tmp, &side, -next->widthA);
        Vec4_Add(&quad[3], &p, &tmp);
        quad[3].w = 1.0f;
        Vec4_Copy(&prev[0], &quad[2]);
        Vec4_Copy(&prev[1], &quad[3]);
        uv[0].x = n % 2;
        uv[0].y = n1 % 2;
        uv[0].z = 1.0f;
        uv[0].w = 0.0f;
        uv[1].x = n % 2;
        uv[1].y = n % 2;
        uv[1].z = 1.0f;
        uv[1].w = 0.0f;
        uv[2].x = n1 % 2;
        uv[2].y = n1 % 2;
        uv[2].z = 1.0f;
        uv[2].w = 0.0f;
        uv[3].x = n1 % 2;
        uv[3].y = n % 2;
        uv[3].z = 1.0f;
        uv[3].w = 0.0f;
        for (i = 0; i < 4; i++) {
            Vu0Cur_ProjectPoint(&scr[i], &quad[i]);
            Vec4_Scale(&st[i], &uv[i], 1.0f / (f32)scr[i].w);
            if (scr[i].x > 0xFFF0) {
                clipped = 1;
                break;
            }
            if (scr[i].y > 0xFFF0) {
                clipped = 1;
                break;
            }
            if (scr[i].z < 0) {
                clipped = 1;
                break;
            }
        }
        if (!clipped) {
            pkt = (EftNQuadPkt *)gOtCur;
            gOtCur = (u32 *)(pkt + 1);
            if (pkt == NULL) {
                return;
            }
            pkt->tag = 0x20000008;
            pkt->vif1 = 0x50000008;
            pkt->prim = 0x5C;
            pkt->vif0 = 0x10000000;
            pkt->gif0 = 0xE400000000008001;
            pkt->gif1 = 0x42142142142160;
            pkt->next = NULL;
            IVec4_Set(col0, seg->r * 255.0f, seg->g * 255.0f, seg->b * 255.0f, seg->alpha * 255.0f * alpha);
            IVec4_Set(col1, next->r * 255.0f, next->g * 255.0f, next->b * 255.0f,
                          next->alpha * 255.0f * alpha);
            pkt->v[0].r = col0[0];
            pkt->v[0].g = col0[1];
            pkt->v[0].b = col0[2];
            pkt->v[0].a = col0[3];
            pkt->v[0].q = st[0].z;
            pkt->v[1].r = col0[0];
            pkt->v[1].g = col0[1];
            pkt->v[1].b = col0[2];
            pkt->v[1].a = col0[3];
            pkt->v[1].q = st[1].z;
            pkt->v[2].r = col1[0];
            pkt->v[2].g = col1[1];
            pkt->v[2].b = col1[2];
            pkt->v[2].a = col1[3];
            pkt->v[2].q = st[2].z;
            pkt->v[3].r = col1[0];
            pkt->v[3].g = col1[1];
            pkt->v[3].b = col1[2];
            pkt->v[3].a = col1[3];
            pkt->v[3].q = st[3].z;
            pkt->v[0].s = st[0].x;
            pkt->v[0].t = st[0].y;
            pkt->v[1].s = st[1].x;
            pkt->v[1].t = st[1].y;
            pkt->v[2].s = st[2].x;
            pkt->v[2].t = st[2].y;
            pkt->v[3].s = st[3].x;
            pkt->v[3].t = st[3].y;
            pkt->v[0].x = scr[0].x;
            pkt->v[0].y = scr[0].y;
            pkt->v[0].z = scr[0].z;
            pkt->v[0].f = 0xFF;
            pkt->v[1].x = scr[1].x;
            pkt->v[1].y = scr[1].y;
            pkt->v[1].z = scr[1].z;
            pkt->v[1].f = 0xFF;
            pkt->v[2].x = scr[2].x;
            pkt->v[2].y = scr[2].y;
            pkt->v[2].z = scr[2].z;
            pkt->v[2].f = 0xFF;
            pkt->v[3].x = scr[3].x;
            pkt->v[3].y = scr[3].y;
            pkt->v[3].z = scr[3].z;
            pkt->v[3].f = 0xFF;
            pkt->tex0 = work->tex0;
            /* the fourth term is scr[2].w, not scr[3].z: a typo in the original */
            z = (scr[0].z + scr[1].z + scr[2].z + scr[2].w) >> 10;
            EftAura_OtAdd((OtPrim *)pkt, z, 1);
        }
        n = n1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/eft_n", EftBolt_Draw);
#endif

/* Sets a bolt up between two model nodes: its origin, end points along the two given directions, axis and normal,
   and a chain of joints sized to its length (at most 10). Returns 0 when fewer than 3 joints were free. */
s32 EftBolt_Start(EftBoltWork *work, EftBolt *bolt, s32 objId, s32 kind, s32 nodeA, s32 nodeB, EftVec dirA,
                  EftVec dirB, f32 length, f32 follow) {
    Vec4 p0;
    Vec4 p1;
    Vec4 tmp;
    Vec4 d;
    f32 one = 1.0f;
    f32 t;
    f32 x;
    f32 y;
    f32 z;
    s32 n;

    bolt->kind = kind;
    bolt->nodeB = nodeB;
    bolt->flags |= 1;
    bolt->nodeA = nodeA;
    bolt->life = 30.0f;
    BtlCharApi_GetNodePos(objId, nodeA, &bolt->pos);
    bolt->start.x = dirA.x * length;
    bolt->start.y = dirA.y * length;
    bolt->start.z = one;
    bolt->end.x = dirB.x * length;
    bolt->end.y = dirB.y * length;
    bolt->end.z = one;
    bolt->follow = follow;
    BtlCharApi_GetNodePos(objId, bolt->nodeB, &tmp);
    Vec4_Sub(&tmp, &tmp, &bolt->pos);
    x = bolt->pos.x + tmp.x * bolt->follow;
    y = bolt->pos.y + tmp.y * bolt->follow;
    z = bolt->pos.z + tmp.z * bolt->follow;
    p0.x = x + bolt->start.x;
    p0.y = y + bolt->start.y;
    p0.z = z + bolt->start.z;
    p0.w = one;
    p1.x = x + bolt->end.x;
    p1.y = y + bolt->end.y;
    p1.z = z + bolt->end.z;
    p1.w = one;
    Vec4_Sub(&d, &p1, &p0);
    Vec3_Normalize(&bolt->dir, &d);
    bolt->normal.x = (dirA.x + dirB.x) * 0.5f;
    bolt->normal.y = (dirA.y + dirB.y) * 0.5f;
    bolt->normal.z = (dirA.z + dirB.z) * 0.5f;
    bolt->normal.w = one;
    Vec3_Normalize(&bolt->normal, &bolt->normal);
    t = work->scale * 0.6f;
    bolt->start.x -= bolt->normal.x * t;
    bolt->start.y -= bolt->normal.y * t;
    bolt->start.z -= bolt->normal.z * t;
    bolt->end.x -= bolt->normal.x * t;
    bolt->end.y -= bolt->normal.y * t;
    bolt->end.z -= bolt->normal.z * t;
    n = sqrtf(Vec3_Dot(&d, &d)) / work->scale + 3.0f;
    if (n > 10) {
        n = 10;
    }
    EftBolt_AllocSegs(bolt, n);
    if (bolt->count >= 3) {
        EftBolt_Shape(bolt, *(EftVec *)&p0, *(EftVec *)&p1, work->scale);
    } else {
        EftBolt_FreeSegs(bolt);
        return 0;
    }
    work->boltCount++;
    return 1;
}

/* Steps every live bolt of the fighter: follows its nodes, steps the joints, advances the reveal / retract counter
   and the 30-frame life, and frees the bolts that have finished. */
void EftBolt_UpdateAll(EftBoltWork *work, s32 objId) {
    Vec4 tmp;
    EftBolt *bolt;
    s32 i;

    for (i = 0; i < EFT_BOLT_COUNT; i++) {
        bolt = &work->bolt[i];
        if (bolt->flags & 1) {
            if (!(bolt->flags & 4)) {
                BtlCharApi_GetNodePos(objId, bolt->nodeA, &bolt->pos);
                BtlCharApi_GetNodePos(objId, bolt->nodeB, &tmp);
                Vec4_Sub(&tmp, &tmp, &bolt->pos);
                bolt->pos.x += tmp.x * bolt->follow;
                bolt->pos.y += tmp.y * bolt->follow;
                bolt->pos.z += tmp.z * bolt->follow;
                if (EftBolt_Step(bolt, work->scale)) {
                    bolt->flags |= 4;
                }
                if (!(bolt->flags & 2)) {
                    if (bolt->shown < bolt->count) {
                        bolt->shown += bolt->count;
                        if (!(bolt->shown < bolt->count)) {
                            bolt->shown = bolt->count;
                            bolt->flags |= 2;
                        }
                    }
                }
                if (bolt->flags & 2) {
                    if (bolt->shown > 0) {
                        bolt->shown = bolt->shown - rand() % 2 - 2;
                        if (bolt->shown < 0) {
                            bolt->shown = 0;
                        }
                    }
                }
                bolt->life -= 1.0f;
                if (bolt->life <= 0.0f) {
                    bolt->flags |= 4;
                }
            } else {
                EftBolt_FreeSegs(bolt);
                bolt->flags = 0;
                work->boltCount--;
            }
        }
    }
}

/* Draws every live bolt of the fighter. */
void EftBolt_DrawAll(EftBoltWork *work, f32 alpha) {
    EftBolt *bolt;
    s32 i;

    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    bolt = work->bolt;
    for (i = 0; i < EFT_BOLT_COUNT; i++, bolt++) {
        if (bolt->flags & 1) {
            EftBolt_Draw(work, bolt, alpha);
        }
    }
    Vu0Cur_Pop();
}

/* Starts a flash sprite between two model nodes. Kind 0 is the violet flash at a bolt's root (6 frames); other
   kinds are grey and last 3 frames. Returns 0 when all 20 are in use. */
s32 EftBolt_AddFlash(EftBoltWork *work, s32 kind, s32 nodeA, s32 nodeB, EftVec vel, f32 size, f32 follow) {
    EftBoltFlash *f = NULL;
    s32 i;

    for (i = 0; i < EFT_BOLT_FLASHES; i++) {
        if (work->flash[i].flags == 0) {
            f = &work->flash[i];
            break;
        }
    }
    if (f == NULL) {
        return 0;
    }
    f->flags |= 1;
    f->kind = kind;
    if (kind == 0) {
        f->life = 6.0f;
        f->r = 50.0f / 255.0f;
        f->g = 30.0f / 255.0f;
        f->a = f->b = 150.0f / 255.0f;
    } else {
        f->life = 3.0f;
        f->b = f->g = f->r = 128.0f / 255.0f;
        f->a = 96.0f / 255.0f;
    }
    f->nodeA = nodeA;
    f->nodeB = nodeB;
    Vec4_Copy(&f->vel, V(&vel));
    f->follow = follow;
    f->size = size;
    f->sizeRate = -(size * 0.2f) / f->life;
    f->alphaRate = -(f->a / (f->life + 1.0f));
    f->frame = rand() % 4;
    f->rot = RANDF() * 6.2831853f;
    if (f->rot >= 3.14159265f) {
        f->rot -= 6.2831853f;
    }
    return 1;
}

/* Steps the fighter's flash sprites: they follow their nodes, drift, shrink, fade and cycle through the four
   cells of their sheet. */
void EftBolt_UpdateFlashes(EftBoltWork *work, s32 objId) {
    Vec4 tmp;
    EftBoltFlash *f;
    s32 i;

    for (i = 0; i < EFT_BOLT_FLASHES; i++) {
        f = &work->flash[i];
        if (f->flags & 1) {
            BtlCharApi_GetNodePos(objId, f->nodeA, &f->pos);
            BtlCharApi_GetNodePos(objId, f->nodeB, &tmp);
            Vec4_Sub(&tmp, &tmp, &f->pos);
            f->pos.x += tmp.x * f->follow;
            f->pos.y += tmp.y * f->follow;
            f->pos.z += tmp.z * f->follow;
            Vec4_Add(&f->pos, &f->pos, &f->vel);
            f->a += f->alphaRate;
            f->size += f->sizeRate;
            if (f->a < 0.0f) {
                f->a = 0.0f;
            }
            f->uv.x = (f32)(f->frame % 2) * 0.5f;
            f->uv.y = (f32)(f->frame / 2) * 0.5f;
            f->uv.z = f->uv.x + 0.5f;
            f->uv.w = f->uv.y + 0.5f;
            f->frame++;
            if (f->frame >= 4) {
                f->frame = 0;
            }
            f->life -= 1.0f;
            if (f->life < 0.0f) {
                f->flags = 0;
            }
        }
    }
}

/* Draws the fighter's flash sprites as camera-facing quads. */
void EftBolt_DrawFlashes(EftBoltWork *work, f32 alpha) {
    EftNTexSet *tex = &gEftBoltPool->flashTex;
    EftBoltFlash *f;
    s32 i;

    for (i = 0; i < EFT_BOLT_FLASHES; i++) {
        f = &work->flash[i];
        if (f->flags & 1) {
            EftSpr_DrawRot((u32)(f->r * 255.0f), (u32)(f->g * 255.0f), (u32)(f->b * 255.0f),
                          (u32)(f->a * 255.0f * alpha), f->pos.x, f->pos.y, f->pos.z, f->uv.x, f->uv.y, f->uv.z,
                          f->uv.w, f->rot, 0, 0, 0x40, 0x40, 0, (u32)(f->size * 4096.0f), 0, 1,
                          &tex->entry[f->kind]);
        }
    }
}

/* Decides whether new bolts appear this frame and starts up to three, alternating between the two sides of the
   body, each with a flash at its root. How often depends on the aura: every frame while the aura is in the rising
   part of a burst or no bolt is alive, otherwise 1 frame in 12 with a visible aura, 1 in 6 while the body glow of the powered-up look is active (EftGlow_IsActive) and
   1 in 18 without; parameter flag 8 adds another 1 in 20. */
/* The node pairs as an array inside a structure: the original adds a member's offset to the table pointer
   before the index (`(pairs + 0x10) + kind * 0x14`, visible as one address computation per member read), which
   this compiler does for an array member reached through a structure, not for pointer arithmetic. */
typedef struct EftBoltPairTbl {
    /* 0x00 */ EftBoltPair p[8];
} EftBoltPairTbl;
#define BOLTPAIRS (((EftBoltPairTbl *)gEftBoltPool->pairs)->p)

/* The work block's bolts as 16-byte aligned records (EftBolt holds vectors; the Vec4 of this file's header is
   not aligned). The free-slot search reads `flags` as 12(&work->bolt[0]) in the original, the form the compiler
   uses when the record's alignment is larger than the member's. */
typedef struct EftBoltA {
    /* 0x00 */ s32 unk0[3];
    /* 0x0C */ s32 flags;
    /* 0x10 */ u8 unk10[0x80];
} __attribute__((aligned(16))) EftBoltA;
typedef struct EftBoltWorkA {
    /* 0x000 */ u8 pad[0x10];
    /* 0x010 */ EftBoltA bolt[EFT_BOLT_COUNT];
} EftBoltWorkA;
#define BOLTS_A (((EftBoltWorkA *)work)->bolt)
void EftBolt_Spawn(EftBoltWork *work, s32 objId) {
    s32 kinds[3];
    s32 pick[4];
    Mtx44 m;
    Vec4 a;
    Vec4 b;
    Vec4 dirA;
    Vec4 dirB;
    Vec4 posA;
    Vec4 posB;
    Vec4 d;
    Vec4 rot;
    Vec4 vel;
    s32 n = 0;
    s32 count = 0;
    s32 side = 0;
    s32 spawned = 0;
    EftBolt *bolt;
    EftBoltPair *pair;
    s32 *node;
    f32 t;
    s32 kind;
    s32 nodeA;
    s32 nodeB;
    s32 i;
    s32 left;
    s32 *out;
    f32 ang;
    f32 dAng;
    f32 turn;
    f32 follow;
    f32 length;
    f32 size;

    if (EftAura_IsVisible(objId)) {
        if (EftAura_IsBurstRising(objId) || work->boltCount <= 0 || rand() % 12 == 0) {
            count = 3;
        }
    } else if (EftGlow_IsActive(objId)) {
        if (work->boltCount <= 0 || rand() % 6 == 0) {
            count = 3;
        }
    } else {
        if (work->boltCount <= 0 || rand() % 18 == 0) {
            count = 3;
        }
    }
    if (work->paramFlags & 8) {
        if (rand() % 20 == 0) {
            count = 3;
        }
    }
    if (count > 0) {
        spawned = 1;
        side = work->side;
        work->side = side ^ 1;
        if (work->orderPos == 0) {
            for (i = 0; i < 4; i++) {
                pick[i] = i;
            }
            for (left = 4, out = work->order; left != 0; left--) {
                n = rand() % left;
                *out = pick[n];
                for (i = n; i < left - 1; i++) {
                    pick[i] = pick[i + 1];
                }
                out++;
            }
        }
        kinds[0] = work->order[work->orderPos];
        if (side != 0) {
            kinds[1] = 5;
            kinds[2] = 6;
        } else {
            kinds[1] = 4;
            kinds[2] = 7;
        }
        n = 0;
    }
    while (count != 0) {
        bolt = NULL;
        for (i = 0; i < EFT_BOLT_COUNT; i++) {
            if (BOLTS_A[i].flags == 0) {
                bolt = &work->bolt[i];
                break;
            }
        }
        if (bolt == NULL) {
            break;
        }
        kind = kinds[n % 3];
        n++;
        if (side == 0) {
            ang = work->angle[0];
            dAng = -(RANDF() * 0.35f + 0.05f) * 3.14159265f;
            turn = -(RANDF() * EFT_DEG(36.0f) + EFT_DEG(90.0f));
        } else {
            ang = work->angle[1];
            dAng = (RANDF() * 0.35f + 0.05f) * 3.14159265f;
            turn = RANDF() * EFT_DEG(36.0f) + EFT_DEG(90.0f);
        }
        nodeA = BOLTPAIRS[kind].nodeA;
        nodeB = BOLTPAIRS[kind].nodeB;
        follow = BOLTPAIRS[kind].follow + BOLTPAIRS[kind].followRange * RANDF();
        if (EftGlow_IsActive(objId)) {
            length = BOLTPAIRS[kind].length * 1.4f * work->scale;
        } else {
            length = BOLTPAIRS[kind].length * work->scale;
        }
        a.x = 0.0f;
        a.y = 1.0f;
        if (kind < 4) {
            a.z = RANDF() * 0.3f + -0.9f;
        } else {
            a.z = RANDF() * 0.1f + -0.6f;
        }
        a.w = 1.0f;
        ang += dAng;
        Vec3_Normalize(&a, &a);
        Mtx_StoreIdentity(&m);
        if (ang >= 3.14159265f) {
            ang -= 6.2831853f;
        } else if (ang <= -3.14159265f) {
            ang += 6.2831853f;
        }
        Mtx_RotateZ(&m, &m, ang);
        Mtx_MulVec4(&dirA, &m, &a);
        b.x = 0.0f;
        b.y = 1.0f;
        if (kind < 4) {
            b.z = 0.4f - RANDF() * 0.25f;
        } else {
            b.z = 0.5f - RANDF() * 0.2f;
        }
        b.w = 1.0f;
        ang += turn;
        Vec3_Normalize(&b, &b);
        Mtx_StoreIdentity(&m);
        if (ang >= 3.14159265f) {
            ang -= 6.2831853f;
        } else if (ang <= -3.14159265f) {
            ang += 6.2831853f;
        }
        Mtx_RotateZ(&m, &m, ang);
        Mtx_MulVec4(&dirB, &m, &b);
        BtlCharApi_GetNodePos(objId, nodeA, &posA);
        BtlCharApi_GetNodePos(objId, nodeB, &posB);
        Vec4_Sub(&d, &posA, &posB);
        Vec3_Normalize(&d, &d);
        Mtx_StoreIdentity(&m);
        rot.x = asinf(d.y);
        Mtx_RotateX(&m, &m, rot.x);
        rot.y = atan2f(d.x, d.z) + 3.14159265f;
        if (rot.y >= 3.14159265f) {
            rot.y -= 6.2831853f;
        }
        Mtx_RotateY(&m, &m, rot.y);
        Mtx_MulVec4(&dirA, &m, &dirA);
        Vec3_Normalize(&dirA, &dirA);
        Mtx_MulVec4(&dirB, &m, &dirB);
        Vec3_Normalize(&dirB, &dirB);
        if (!EftBolt_Start(work, bolt, objId, kind, nodeA, nodeB, *(EftVec *)&dirA, *(EftVec *)&dirB, length,
                           follow)) {
            break;
        }
        Vec4_Scale(&vel, &dirA, length * 0.8f);
        if (EftGlow_IsActive(objId)) {
            size = (RANDF() * 0.25f + 0.7f) * 0.42f * work->scale * 1.4f;
        } else {
            size = (RANDF() * 0.25f + 0.7f) * 0.42f * work->scale;
        }
        EftBolt_AddFlash(work, 0, nodeA, nodeB, *(EftVec *)&vel, size, follow);
        if (side == 0) {
            work->angle[0] = ang;
        } else {
            work->angle[1] = ang;
        }
        count--;
        side ^= 1;
    }
    if (spawned) {
        work->orderPos++;
        if (work->orderPos >= 4) {
            work->orderPos = 0;
        }
    }
}
