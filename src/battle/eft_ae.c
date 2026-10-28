#include "common.h"
#include "battle/eft_ae.h"
#include "sys/gfx_ot.h"

/*
 * 0x1AA7E8..0x1AE2A8: the end of the effect code. See include/battle/eft_ae.h for the layouts.
 *
 *   0x1AA7E8            EftSpr_IsOffScreen: the screen test the two sprite writers before it (0x1A9D90, 0x1AA188)
 *                       call; it is the last function of THEIR object.
 *   0x1AA818..0x1AD138  keyframed sprite animations (EftSprAnim_*) and their "V000" pack readers (EftSprPack_*).
 *                       Purely visual. The step draws libc rand() twice per layer per frame.
 *   0x1AD138..0x1ADBA8  the task tree (BtlTask*). Every effect module is a class of tasks in this tree.
 *   0x1ADBA8..0x1AE2A8  effect texture VRAM allocation (EftVram_*) and texture sets (EftTexSet_*); the same
 *                       source file goes on in eft_det_a.c (0x1AE2A8..0x1AE5F8).
 *
 * Callees outside this file are declared here with local views.
 */

/* The effect code's by-value vector: 16-byte aligned, copied by the callee. */
typedef union EftAeVec {
    struct {
        f32 x, y, z, w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftAeVec;

typedef struct EftAeMtx {
    f32 m[4][4];
} __attribute__((aligned(16))) EftAeMtx;

/* A GS screen position as the projection helpers write it. */
typedef struct EftAeScr {
    s32 x, y, z, w;
} EftAeScr;

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftAeView {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ EftAeMtx view;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ EftAeMtx world2screen;
} EftAeView;

/* Head of a packet in the ordering table: DMA tag, two VIF codes, GIF tag. */
typedef struct EftAePktHead {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
} EftAePktHead;

/* A+D packet that sets two registers (CLAMP_1 and CLAMP_2 here). 0x40 bytes. */
typedef struct EftAeRegPkt {
    /* 0x00 */ EftAePktHead h;
    /* 0x20 */ u64 data0;
    /* 0x28 */ u64 reg0;
    /* 0x30 */ u64 data1;
    /* 0x38 */ u64 reg1;
} EftAeRegPkt;

/* GS XYZF2 register value. */
typedef struct EftAeXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftAeXyzf;

typedef struct EftAeGsVtx {
    /* 0x00 */ u8 rgba[4];
    /* 0x04 */ f32 q;
    /* 0x08 */ f32 s;
    /* 0x0C */ f32 t;
    /* 0x10 */ EftAeXyzf xyz;
} EftAeGsVtx; /* 0x18: RGBAQ, ST, XYZF2 */

/* One textured gouraud triangle between two CLAMP writes. 0x90 bytes, 14 registers. */
typedef struct EftAeTriPkt {
    /* 0x00 */ EftAePktHead h;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ u64 clamp;     /* 5: clamp s and t */
    /* 0x38 */ EftAeGsVtx v[3];
    /* 0x80 */ u64 clampEnd;  /* 0: back to repeat */
    /* 0x88 */ u64 pad;
} EftAeTriPkt;

/* A vertex handed to the clipper. */
typedef struct EftAeClipVtx {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 uv;
    /* 0x20 */ Vec4 col;
} EftAeClipVtx; /* 0x30 */

extern void ClipVtx_SetArray(EftAeClipVtx *out, Vec4 *pos, Vec4 *uv, Vec4 *color, s32 n); /* n clip vertices */
extern s32 ClipPoly_ClipPlane(EftAeClipVtx *poly, Vec4 *plane, s32 count);   /* clips in place, new count */
extern void ClipPoly_ProjectCur(EftAeScr *scr, Vec4 *stq, EftAeClipVtx *poly, s32 count); /* projects */
extern void EftGfx_UpdateClipPlanes(void);
extern Vec4 *EftGfx_GetClipPlanes(void);

extern f32 gEftSprGridUv2x2[4][4];
extern f32 gEftSprGridUv4x2[8][4];
extern f32 gEftSprGridUv3x3[9][4];
extern f32 gEftSprGridUv4x4[16][4];
extern s32 gEftSprSubdivCorner[4][4];

extern EftAeView *gBtlCamView;
extern EftSprSlot *gEftSprAnimPool;
extern BtlTaskList *gBtlTaskRoot;
extern BtlTask *gBtlTaskCur;
extern EftAeVram *gEftVram;

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern void *Heap_Alloc(s32 size, s32 align, s32 a2, s32 a3);
extern void Heap_Free(void *p);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Mtx_StoreIdentity(EftAeMtx *m);
extern void Mtx_MulVec4(Vec4 *dst, EftAeMtx *m, Vec4 *v);
extern void Mtx_Mul(EftAeMtx *dst, EftAeMtx *a, EftAeMtx *b);   /* matrix product */
extern void Mtx_InverseRT(EftAeMtx *dst, EftAeMtx *src);             /* transpose (inverse of a rotation) */
extern void Mtx_RotateXYZ(EftAeMtx *dst, EftAeMtx *src, Vec4 *rot);  /* rotate by three angles */
extern void Mtx_ScaleDiag(EftAeMtx *dst, EftAeMtx *src, Vec4 *scale); /* scale */
extern void Vu0Cur_Push(void);                                     /* VU0 matrix stack push */
extern void Vu0Cur_LoadMtx(EftAeMtx *m);                              /* load the matrix */
extern void Vu0Cur_Pop(void);                                     /* pop */
extern s32 Vu0Cur_ProjectPoint(EftAeScr *out, Vec4 *pos);                  /* project to GS screen coordinates */
extern void Vec4_Mul(Vec4 *dst, Vec4 *a, Vec4 *b);              /* per-component product */
extern void Vec4_Lerp(Vec4 *dst, Vec4 *a, Vec4 *b, f32 t);       /* interpolate a..b */
extern void Vec3_Lerp(Vec4 *dst, Vec4 *a, Vec4 *b, f32 t);       /* interpolate b..a */
extern f32 EftMath_WrapAngle(f32 a);
extern void EftVram_Upload(EftAeTex *tex, s32 imageBlock, s32 clutBlock);

s32 EftSprAnim_IsPlaying(EftSprAnim *anim);
void EftSprAnim_Start(EftSprAnim *anim);
void EftSprAnim_Restart(EftSprAnim *anim);
void EftSprAnim_WrapDegrees(Vec4 *v);
s32 EftSprAnim_BuildMatrices(EftSprAnim *anim, EftAeMtx *oriented, EftAeMtx *billboard);
void EftSprAnim_DrawLayer(EftSprLayerDef *def, s32 frame, s32 time, EftAeTexSet32 *tex, EftAeMtx *m,
                          EftAeMtx *oriented, Vec4 *pos, Vec4 *spin, Vec4 *color, s32 mode, s32 rand1, s32 rand2);
void EftSprAnim_DrawLayerAt(EftAeMtx *m, EftAeMtx *oriented, Vec4 *objPos, EftSprLayerDef *def, s32 frame, s32 time,
                            EftAeTexSet32 *tex, Vec4 *spin, Vec4 *color, s32 mode, s32 rand1, s32 rand2);
void EftSprAnim_Jitter(Vec4 *scale, EftSprLayerDef *def, s32 frame, s32 rand1, s32 rand2);
s32 EftSprAnim_GetGridCells(s32 grid);
void EftSprAnim_SetGridUv(Vec4 *uv, s32 grid, s32 cell);
void EftSprAnim_DrawQuad(u64 tex0, Vec4 *corner, Vec4 *uv, Vec4 *color, s32 otLayer, s32 z, s32 mode);
void EftSprAnim_DrawQuadSubdiv(u64 tex0, Vec4 *corner, Vec4 *uv, Vec4 *color, s32 otLayer, s32 z, s32 mode);
EftSprPack *EftSprPack_Get(EftSprPack *pack);
void EftSprPack_Bind(EftSprPack *pack);
s32 EftSprPack_GetLayerCount(EftSprPack *pack);
EftSprLayerDef *EftSprPack_GetLayers(EftSprPack *pack);
s32 EftSprPack_GetLoopEndFrame(EftSprLayerDef *def);
s32 EftSprPack_GetLoopStartFrame(EftSprLayerDef *def);
s32 EftSprPack_FindKey(EftSprLayerDef *def, s32 frame);
f32 EftSprPack_GetKeyT(EftSprLayerDef *def, s32 frame, s32 key);
s32 EftSprPack_IsValid(EftSprPack *pack);
void BtlTaskList_Update(BtlTaskList *list);
BtlTaskList *BtlTaskList_Create(BtlTask *parent, u32 count, u32 workSize);
void BtlTaskList_Destroy(BtlTaskList *list);
BtlTask *BtlTask_AddAfter(BtlTask *task, BtlTaskCls *cls, void *arg);
BtlTask *BtlTask_AddBefore(BtlTask *task, BtlTaskCls *cls, void *arg);
void BtlTask_Init(BtlTask *task, BtlTaskCls *cls, void *arg);
void BtlTaskList_KillAll(BtlTaskList *list);
void BtlTask_SetDead(BtlTask *task);
void BtlTask_Kill(BtlTask *task);
u64 EftVram_AddTex(EftAeTex *tex, s32 tcc, s32 tfx);
void EftTexSet_CheckCount(s32 count, s32 max) __attribute__((const));

/* 1 when a projected point cannot be drawn: behind the camera or outside the GS coordinate range. */
s32 EftSpr_IsOffScreen(s32 x, s32 y, s32 z) {
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

/* Allocates the pool of 32 animation slots (from BtlScene_Init). */
void EftSprAnim_InitPool(void) {
    gEftSprAnimPool = Heap_Alloc(32 * sizeof(EftSprSlot), 0x20, 0, 2);
    memset(gEftSprAnimPool, 0, 32 * sizeof(EftSprSlot));
}

/* Frees the pool. */
void EftSprAnim_TermPool(void) {
    Heap_Free(gEftSprAnimPool);
    gEftSprAnimPool = NULL;
}

/* Takes the first unused slot; NULL when all 32 are in use. */
EftSprSlot *EftSprAnim_AllocSlot(void) {
    s32 *used = &gEftSprAnimPool->used;
    s32 i;

    for (i = 0; i < 32; i++) {
        if (*used == 0) {
            *used = 1;
            return &gEftSprAnimPool[i];
        }
        used = (s32 *)((u8 *)used + sizeof(EftSprSlot));
    }
    return NULL;
}

/* Gives a slot back. */
void EftSprAnim_FreeSlot(EftSprSlot *slot) {
    slot->used = 0;
}

/* Creates an animation object for a pack and a texture set, and starts it. */
void EftSprAnim_Create(EftSprAnim *anim, EftSprPack *pack, EftAeTexSet32 *tex) {
    memset(anim, 0, sizeof(EftSprAnim));
    anim->pack = pack;
    anim->tex = tex;
    anim->slot = EftSprAnim_AllocSlot();
    anim->slot->flag = 1;
    Vec4_Set(&anim->pos, 0.0f, 0.0f, 0.0f, 1.0f);
    Vec4_Set(&anim->dir, 0.0f, 0.0f, -1.0f, 1.0f);
    Vec4_Set(&anim->scale, 1.0f, 1.0f, 1.0f, 1.0f);
    Vec4_Set(&anim->color, 1.0f, 1.0f, 1.0f, 1.0f);
    EftSprPack_Bind(anim->pack);
    EftSprAnim_Start(anim);
}

/* Releases the object's slot. */
void EftSprAnim_Destroy(EftSprAnim *anim) {
    EftSprAnim_FreeSlot(anim->slot);
}

/* Puts every layer at its first frame. */
void EftSprAnim_Start(EftSprAnim *anim) {
    EftSprPack *pack = EftSprPack_Get(anim->pack);
    EftSprLayerDef *def = EftSprPack_GetLayers(pack);
    s32 n = EftSprPack_GetLayerCount(pack);
    EftSprLayer *st = anim->slot->layer;
    s32 i;

    for (i = 0; i < n; i++) {
        st->def = def;
        st->loopStart = 0.0f;
        st->lastFrame = def->length - 1;
        st->fadeLen = def->fade;
        st->fadeLeft = def->fade;
        st->frame = 0.0f;
        st->alpha = 1.0f;
        st->time = 0.0f;
        st->speed = def->speed;
        st->flags = 0;
        st->loops = 0;
        if (def->flags & EFT_SPR_LAYER_LOOP) {
            st->loopStart = EftSprPack_GetLoopStartFrame(def);
            st->lastFrame = EftSprPack_GetLoopEndFrame(st->def);
            st->loops = st->def->loopCount;
        }
        if (st->def->flags & EFT_SPR_LAYER_SPIN) {
            EftSprKey *key = st->def->keys;

            Vec4_Set(&st->rot, 0.0f, 0.0f, 0.0f, 1.0f);
            Vec4_Set(&st->spin, key->rot.x, key->rot.y, key->rot.z, 1.0f);
        }
        st++;
        def++;
    }
}

/* Advances every layer one step; restarts a looping pack when all layers are done. Returns the number of quads
   the object will draw. Draws libc rand() twice per layer. */
s32 EftSprAnim_Step(EftSprAnim *anim) {
    s32 quads = 0;
    s32 i = 0;
    EftSprPack *pack = EftSprPack_Get(anim->pack);
    EftSprLayer *st = anim->slot->layer;

    for (; i < pack->layerCount; i++) {
        st->frame += st->speed + st->speed;
        st->time += st->speed + st->speed;
        st->rand1 = rand();
        st->rand2 = rand();
        Vec3_Add(&st->rot, &st->rot, &st->spin);
        EftSprAnim_WrapDegrees(&st->rot);
        if (st->frame >= st->lastFrame) {
            if (st->loops > 0 || st->loops == -1) {
                st->frame = st->loopStart;
                if (st->loops != -1) {
                    st->loops--;
                }
            } else {
                st->frame = st->lastFrame;
                if (st->fadeLeft > 0.0f) {
                    st->fadeLeft -= 1.0f;
                    st->alpha = st->fadeLeft / st->fadeLen;
                    if (st->fadeLeft <= 0.0f) {
                        st->alpha = 0.0f;
                    }
                }
                if (st->fadeLeft <= 0.0f) {
                    st->flags |= 1;
                }
            }
        }
        if (st->def->flags & EFT_SPR_LAYER_SUBDIV) {
            quads += 4;
        } else {
            quads += 1;
        }
        st++;
    }
    if (pack->loop && !EftSprAnim_IsPlaying(anim)) {
        EftSprAnim_Restart(anim);
    }
    return quads;
}

/* Draws every layer into the ordering table: builds the object's two frames, loads the view's world-to-screen
   matrix and hands each layer to EftSprAnim_DrawLayer with its frame, its two random values and the object's
   colour times the layer's fade. A layer's own flags pick the frame and the draw mode; the object's draw flags
   override them. Original bug kept: the layer pointer only advances when a layer is drawn, so a skipped layer
   (camera-facing while the frames could not be built, which never happens: BuildMatrices always returns 1) would
   be drawn again in place of the next one. */
/* Matching note: the two layer-definition flags have to be tested through an inline predicate that returns
   `!= 0`. Its extra instruction per test disappears in combine, but it is still there when the loop pass counts
   the loop (63 instructions instead of 61), and that count decides which constants get hoisted: with 62..64 the
   1 of EFT_SPR_DRAW_MODE1 is moved out of the loop and the 2 of EFT_SPR_DRAW_MODE2 is not (the pass moves a
   one-instruction constant only while `threshold >= count`, threshold 64 less 3 per move). Which tests went
   through a helper in the original is not known: one or both layer tests, or up to three of the draw-flag
   tests, all give the same code. */
static inline s32 EftSprLayerDef_TestFlag(const EftSprLayerDef *def, s32 flag) {
    return (def->flags & flag) != 0;
}

void EftSprAnim_Draw(EftSprAnim *anim) {
    Vec4 color;
    EftAeMtx oriented;
    EftAeMtx billboard;
    s32 ok;
    EftSprPack *pack;
    EftSprLayer *st;
    EftAeMtx *m;
    s32 mode;
    s32 i = 0;

    Mtx_StoreIdentity(&oriented);
    Mtx_StoreIdentity(&billboard);
    ok = EftSprAnim_BuildMatrices(anim, &oriented, &billboard);
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    pack = EftSprPack_Get(anim->pack);
    st = anim->slot->layer;
    for (; i < pack->layerCount; i++) {
        m = &oriented;
        mode = 0;
        if (EftSprLayerDef_TestFlag(st->def, EFT_SPR_LAYER_BILLBOARD)) {
            m = &billboard;
        }
        if (EftSprLayerDef_TestFlag(st->def, EFT_SPR_LAYER_SLOT_MODE)) {
            mode = anim->slot->flag != 0;
        }
        if (anim->drawFlags & EFT_SPR_DRAW_BILLBOARD) {
            m = &billboard;
        }
        if (anim->drawFlags & EFT_SPR_DRAW_ORIENTED) {
            m = &oriented;
        }
        if (anim->drawFlags & EFT_SPR_DRAW_MODE1) {
            mode = 1;
        }
        if (anim->drawFlags & EFT_SPR_DRAW_MODE0) {
            mode = 0;
        }
        if (anim->drawFlags & EFT_SPR_DRAW_MODE2) {
            mode = 2;
        }
        Vec4_Copy(&color, &anim->color);
        color.w *= st->alpha;
        if (ok || m != &billboard) {
            EftSprAnim_DrawLayer(st->def, st->frame, st->time, anim->tex, m, &oriented, &anim->pos, &st->rot, &color,
                                 mode, st->rand1, st->rand2);
            st++;
        }
    }
    Vu0Cur_Pop();
}

/* 1 while any layer has not finished. */
s32 EftSprAnim_IsPlaying(EftSprAnim *anim) {
    EftSprPack *pack = EftSprPack_Get(anim->pack);
    EftSprLayer *st = anim->slot->layer;
    s32 i;

    for (i = 0; i < pack->layerCount; i++) {
        if (!(st->flags & 1)) {
            return 1;
        }
        st++;
    }
    return 0;
}

/* Sets the object's position. */
void EftSprAnim_SetPos(EftSprAnim *anim, Vec4 *pos) {
    anim->pos = *pos;
    anim->pos.w = 1.0f;
}

/* Sets the object's forward axis. */
void EftSprAnim_SetDir(EftSprAnim *anim, Vec4 *dir) {
    anim->dir = *dir;
    anim->dir.w = 1.0f;
}

/* Sets the object's scale. */
void EftSprAnim_SetScale(EftSprAnim *anim, Vec4 *scale) {
    anim->scale = *scale;
    anim->scale.w = 1.0f;
}

/* Sets the object's colour (0..1 per channel, alpha included). */
void EftSprAnim_SetColor(EftSprAnim *anim, Vec4 *color) {
    anim->color = *color;
}

/* Sets the slot's flag (draw mode of the layers that follow it). No caller. */
void EftSprAnim_SetSlotFlag(EftSprAnim *anim, s32 flag) {
    anim->slot->flag = flag;
}

/* Sets the draw overrides (EFT_SPR_DRAW_*). */
void EftSprAnim_SetDrawFlags(EftSprAnim *anim, s32 flags) {
    anim->drawFlags = flags;
}

/* Gives every texture the layers use VRAM for this frame, once per texture per set. */
void EftSprAnim_KeepTextures(EftSprAnim *anim) {
    EftSprPack *pack = EftSprPack_Get(anim->pack);
    EftAeTexSet32 *tex = anim->tex;
    EftSprLayer *st = anim->slot->layer;
    EftSprLayerDef *def;
    s32 i;

    for (i = 0; i < pack->layerCount; i++) {
        def = st->def;
        st++;
        if (!(tex->kept & (1 << def->tex))) {
            tex->tex[def->tex].tex0 = EftVram_AddTex(&tex->tex[def->tex], 1, 0);
            tex->kept |= 1 << def->tex;
        }
    }
}

/* Puts every layer back at its first frame (as EftSprAnim_Start, keeping the spin). */
void EftSprAnim_Restart(EftSprAnim *anim) {
    EftSprPack *pack = EftSprPack_Get(anim->pack);
    EftSprLayerDef *def = EftSprPack_GetLayers(pack);
    s32 n = EftSprPack_GetLayerCount(pack);
    EftSprLayer *st = anim->slot->layer;
    s32 i;

    for (i = 0; i < n; i++) {
        st->def = def;
        st->loopStart = 0.0f;
        st->lastFrame = def->length - 1;
        st->fadeLen = def->fade;
        st->fadeLeft = def->fade;
        st->frame = 0.0f;
        st->alpha = 1.0f;
        st->speed = def->speed;
        st->flags = 0;
        st->loops = 0;
        if (def->flags & EFT_SPR_LAYER_LOOP) {
            st->loopStart = EftSprPack_GetLoopStartFrame(def);
            st->lastFrame = EftSprPack_GetLoopEndFrame(def);
            st->loops = def->loopCount;
        }
        st++;
        def++;
    }
}

/* Keeps three angles in degrees inside 0..360. */
void EftSprAnim_WrapDegrees(Vec4 *v) {
    if (v->x > 360.0f) {
        v->x -= 360.0f;
    }
    if (v->x < 0.0f) {
        v->x += 360.0f;
    }
    if (v->y > 360.0f) {
        v->y -= 360.0f;
    }
    if (v->y < 0.0f) {
        v->y += 360.0f;
    }
    if (v->z > 360.0f) {
        v->z -= 360.0f;
    }
    if (v->z < 0.0f) {
        v->z += 360.0f;
    }
}

/* Builds the rotation whose third row is dir (first row right, second row up). 0 when dir has no length. */
s32 EftSprAnim_BuildDirMtx(EftAeMtx *out, Vec4 *dir) {
    Vec4 up;
    Vec4 right;
    Vec4 fwd;

    Vec4_Set(&up, 0.0f, -1.0f, 0.0f, 0.0f);
    if (dir->x == 0.0f && dir->y == -1.0f && dir->z == 0.0f) {
        Vec4_Set(&up, 0.0f, 0.0f, -1.0f, 0.0f);
    }
    if (dir->x == 0.0f && dir->y == -1.0f && dir->z == 0.0f) {
        Vec4_Set(&up, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    Vec3_Normalize(&fwd, dir);
    if (fwd.x == 0.0f && fwd.y == 0.0f && fwd.z == 0.0f) {
        return 0;
    }
    Vec3_Cross(&right, &up, &fwd);
    Vec3_Normalize(&right, &right);
    Vec3_Cross(&up, &right, &fwd);
    Vec3_Normalize(&up, &up);
    Mtx_StoreIdentity(out);
    out->m[0][0] = right.x;
    out->m[0][1] = right.y;
    out->m[0][2] = right.z;
    out->m[1][0] = up.x;
    out->m[1][1] = up.y;
    out->m[1][2] = up.z;
    out->m[2][0] = fwd.x;
    out->m[2][1] = fwd.y;
    out->m[2][2] = fwd.z;
    return 1;
}

/* Builds the object's two frames: oriented along its direction, and facing the camera; both scaled. */
s32 EftSprAnim_BuildMatrices(EftSprAnim *anim, EftAeMtx *oriented, EftAeMtx *billboard) {
    EftAeMtx dirM;
    EftAeMtx scaleM;
    EftAeMtx m;

    Mtx_StoreIdentity(oriented);
    Mtx_StoreIdentity(&scaleM);
    Mtx_StoreIdentity(&dirM);
    Mtx_StoreIdentity(&m);
    EftSprAnim_BuildDirMtx(&dirM, &anim->dir);
    Mtx_ScaleDiag(&scaleM, &scaleM, &anim->scale);
    Mtx_Mul(&m, &dirM, &scaleM);
    *oriented = m;
    oriented->m[3][0] = oriented->m[3][1] = oriented->m[3][2] = 0.0f;
    Mtx_InverseRT(billboard, &gBtlCamView->view);
    billboard->m[3][0] = billboard->m[3][1] = billboard->m[3][2] = 0.0f;
    Mtx_Mul(billboard, billboard, &scaleM);
    return 1;
}

/* Draws one layer (argument order of the caller's loop). */
void EftSprAnim_DrawLayer(EftSprLayerDef *def, s32 frame, s32 time, EftAeTexSet32 *tex, EftAeMtx *m,
                          EftAeMtx *oriented, Vec4 *pos, Vec4 *spin, Vec4 *color, s32 mode, s32 rand1, s32 rand2) {
    EftSprAnim_DrawLayerAt(m, oriented, pos, def, frame, time, tex, spin, color, mode, rand1, rand2);
}

/* Draws one layer at a frame: interpolates position, rotation, scale and colour between the two keys around
   the frame, applies jitter and spin, builds a 10 x 10 quad in the layer's frame `m` around the key position
   (rotated by the object's oriented frame), picks the flip-book cell or applies the blink, queues a packet that
   resets CLAMP at the quad's depth and then the quad itself. The depth is the projected centre's z >> 8, or the
   far end (0xFFF) in draw mode 2. rand1 / rand2 are the layer's two random values of this step. */
void EftSprAnim_DrawLayerAt(EftAeMtx *m, EftAeMtx *oriented, Vec4 *objPos, EftSprLayerDef *def, s32 frame, s32 time,
                            EftAeTexSet32 *tex, Vec4 *spin, Vec4 *color, s32 mode, s32 rand1, s32 rand2) {
    Vec4 pos;
    Vec4 rot;
    Vec4 scale;
    Vec4 col;
    Vec4 ca;
    Vec4 cb;
    Vec4 corner[4];
    Vec4 uv[4];
    Vec4 colors[4];
    EftAeMtx local;
    EftAeMtx world;
    Vec4 center;
    EftAeScr scr;
    EftSprKey *k;
    EftSprKey *kn;
    EftAeRegPkt *p;
    OtEntry *e;
    f32 t;
    s32 key;
    s32 cell;
    s32 z;
    s32 otLayer;
    s32 lay;
    s32 i;

    key = EftSprPack_FindKey(def, frame);
    if (key == -1) {
        return;
    }
    t = EftSprPack_GetKeyT(def, frame, key);
    Vec3_Lerp(&pos, &def->keys[key + 1].pos, &def->keys[key].pos, t);
    Vec3_Lerp(&rot, &def->keys[key + 1].rot, &def->keys[key].rot, t);
    Vec3_Lerp(&scale, &def->keys[key + 1].scale, &def->keys[key].scale, t);
    k = (EftSprKey *)((key << 6) + (u32)def->keys);
    kn = k + 1;
    ca.x = kn->color[0];
    ca.y = kn->color[1];
    ca.z = kn->color[2];
    ca.w = kn->color[3];
    cb.x = k[0].color[0];
    cb.y = k[0].color[1];
    cb.z = k[0].color[2];
    cb.w = k[0].color[3];
    Vec4_Lerp(&col, &ca, &cb, t);
    Vec4_Mul(&col, &col, color);
    if (def->flags & (EFT_SPR_LAYER_JITTER_XY | EFT_SPR_LAYER_JITTER_X | EFT_SPR_LAYER_JITTER_Y)) {
        EftSprAnim_Jitter(&scale, def, frame, rand1, rand2);
    }
    if (def->flags & EFT_SPR_LAYER_SPIN) {
        rot = *spin;
    }
    rot.x = EftMath_WrapAngle(rot.x * 3.14159265f / 180.0f);
    rot.y = EftMath_WrapAngle(rot.y * 3.14159265f / 180.0f);
    rot.z = EftMath_WrapAngle(rot.z * 3.14159265f / 180.0f);
    Mtx_StoreIdentity(&local);
    Mtx_ScaleDiag(&local, &local, &scale);
    Mtx_RotateXYZ(&local, &local, &rot);
    Mtx_Mul(&world, m, &local);
    Mtx_MulVec4(&center, oriented, &pos);
    Vec3_Add(&center, &center, objPos);
    Vec4_Set(&corner[0], -5.0f, -5.0f, 0.0f, 1.0f);
    Vec4_Set(&corner[1], 5.0f, -5.0f, 0.0f, 1.0f);
    Vec4_Set(&corner[2], -5.0f, 5.0f, 0.0f, 1.0f);
    Vec4_Set(&corner[3], 5.0f, 5.0f, 0.0f, 1.0f);
    Vec4_Set(&uv[0], 0.0f, 0.0f, 1.0f, 1.0f);
    Vec4_Set(&uv[1], 1.0f, 0.0f, 1.0f, 1.0f);
    Vec4_Set(&uv[2], 0.0f, 1.0f, 1.0f, 1.0f);
    Vec4_Set(&uv[3], 1.0f, 1.0f, 1.0f, 1.0f);
    Vec4_Set(&colors[0], col.x, col.y, col.z, col.w);
    Vec4_Set(&colors[1], col.x, col.y, col.z, col.w);
    Vec4_Set(&colors[2], col.x, col.y, col.z, col.w);
    Vec4_Set(&colors[3], col.x, col.y, col.z, col.w);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&corner[i], &world, &corner[i]);
        Vec3_Add(&corner[i], &corner[i], &center);
        corner[i].w = 1.0f;
    }
    cell = 0;
    if (def->flags & EFT_SPR_LAYER_GRID) {
        s32 on = def->blinkOn;
        s32 off = def->blinkOff;
        s32 step = def->gridStep;
        s32 n;

        if (on + off != 0 && (time >> 1) % (on + off) >= on) {
            return;
        }
        n = EftSprAnim_GetGridCells(def->grid);
        if (def->flags & EFT_SPR_LAYER_GRID_RAND) {
            cell = rand1 % n;
        } else if (step != 0) {
            cell = (time >> 1) / step % n;
        }
        EftSprAnim_SetGridUv(uv, def->grid, cell);
    } else if (def->flags & EFT_SPR_LAYER_BLINK) {
        s32 step = def->gridStep;

        if (step > 0) {
            s32 on = def->blinkOn + rand1 % step;
            s32 off = def->blinkOff + rand2 % step;

            if (on + off != 0 && (frame >> 1) % (on + off) >= on) {
                return;
            }
        }
    }
    Vu0Cur_ProjectPoint(&scr, &center);
    z = scr.z >> 8;
    if (mode == 2) {
        z = 0xFFF;
    }
    otLayer = def->otLayer;
    lay = otLayer;
    if (otLayer >= 2) {
        lay = otLayer - 2;
    }
    if (z < 0) {
        e = &gOtZ[0].layer[lay];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[lay];
    } else {
        e = &gOtZ[z].layer[lay];
    }
    p = (EftAeRegPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    p->h.tag = 0x20000003;
    p->h.vif0 = 0x10000000;
    p->h.vif1 = 0x50000003;
    p->h.gif0 = 0x1000000000008002;
    p->h.gif1 = 0xE;
    p->reg0 = 8;
    p->reg1 = 9;
    p->h.next = 0;
    p->data0 = 0;
    p->data1 = 0;
    e->tail->next = (OtPrim *)p;
    e->tail = (OtPrim *)p;
    if (def->flags & EFT_SPR_LAYER_SUBDIV) {
        EftSprAnim_DrawQuadSubdiv(tex->tex[def->tex].tex0, corner, uv, colors, otLayer, z, mode);
    } else {
        EftSprAnim_DrawQuad(tex->tex[def->tex].tex0, corner, uv, colors, otLayer, z, mode);
    }
}

/* Adds this frame's random jitter to a layer's scale (x and y, x only, y only), on frames that are a multiple
   of the layer's jitter period. */
void EftSprAnim_Jitter(Vec4 *scale, EftSprLayerDef *def, s32 frame, s32 rand1, s32 rand2) {
    s32 on = 0;

    if (def->jitterPeriod > 0) {
        on = frame % def->jitterPeriod == 0;
    }
    if ((def->flags & EFT_SPR_LAYER_JITTER_XY) && def->jitterX > 0 && on) {
        f32 d = (rand1 % def->jitterX) * 0.001f;

        scale->x += d;
        scale->y += d;
    }
    if ((def->flags & EFT_SPR_LAYER_JITTER_X) && def->jitterX > 0 && on) {
        scale->x += (rand1 % def->jitterX) * 0.001f;
    }
    if ((def->flags & EFT_SPR_LAYER_JITTER_Y) && def->jitterY > 0 && on) {
        scale->y += (rand2 % def->jitterY) * 0.001f;
    }
}

/* Number of cells of a texture grid kind. */
s32 EftSprAnim_GetGridCells(s32 grid) {
    s32 n = 0;

    switch (grid) {
    case 0:
        n = 4;
        break;
    case 1:
        n = 8;
        break;
    case 2:
        n = 16;
        break;
    case 3:
        n = 9;
        break;
    }
    return n;
}

/* Writes the four corner uv of one cell of a texture grid (nothing for an unknown grid kind). */
void EftSprAnim_SetGridUv(Vec4 *uv, s32 grid, s32 cell) {
    switch (grid) {
    case 0:
        uv[0].x = gEftSprGridUv2x2[cell][0];
        uv[0].y = gEftSprGridUv2x2[cell][1];
        uv[1].x = gEftSprGridUv2x2[cell][2];
        uv[1].y = gEftSprGridUv2x2[cell][1];
        uv[2].x = gEftSprGridUv2x2[cell][0];
        uv[2].y = gEftSprGridUv2x2[cell][3];
        uv[3].x = gEftSprGridUv2x2[cell][2];
        uv[3].y = gEftSprGridUv2x2[cell][3];
        break;
    case 1:
        uv[0].x = gEftSprGridUv4x2[cell][0];
        uv[0].y = gEftSprGridUv4x2[cell][1];
        uv[1].x = gEftSprGridUv4x2[cell][2];
        uv[1].y = gEftSprGridUv4x2[cell][1];
        uv[2].x = gEftSprGridUv4x2[cell][0];
        uv[2].y = gEftSprGridUv4x2[cell][3];
        uv[3].x = gEftSprGridUv4x2[cell][2];
        uv[3].y = gEftSprGridUv4x2[cell][3];
        break;
    case 2:
        uv[0].x = gEftSprGridUv4x4[cell][0];
        uv[0].y = gEftSprGridUv4x4[cell][1];
        uv[1].x = gEftSprGridUv4x4[cell][2];
        uv[1].y = gEftSprGridUv4x4[cell][1];
        uv[2].x = gEftSprGridUv4x4[cell][0];
        uv[2].y = gEftSprGridUv4x4[cell][3];
        uv[3].x = gEftSprGridUv4x4[cell][2];
        uv[3].y = gEftSprGridUv4x4[cell][3];
        break;
    case 3:
        uv[0].x = gEftSprGridUv3x3[cell][0];
        uv[0].y = gEftSprGridUv3x3[cell][1];
        uv[1].x = gEftSprGridUv3x3[cell][2];
        uv[1].y = gEftSprGridUv3x3[cell][1];
        uv[2].x = gEftSprGridUv3x3[cell][0];
        uv[2].y = gEftSprGridUv3x3[cell][3];
        uv[3].x = gEftSprGridUv3x3[cell][2];
        uv[3].y = gEftSprGridUv3x3[cell][3];
        break;
    }
}

/*
 * EftSprAnim_DrawQuad (0x1ABF48) and EftSprAnim_DrawQuadSubdiv (0x1AC650) are left in assembly. Both end in the
 * same inlined code, read from the disassembly and reproduced by the attempt below except for register
 * allocation and the order of a few constant loads:
 *   - a triangle (three corners with uv and colour) is clipped against the five planes of the view
 *     (EftGfx_UpdateClipPlanes / GetClipPlanes, ClipPoly_ClipPlane), projected (ClipPoly_ProjectCur) and written as a fan;
 *   - each fan triangle is skipped when its three alphas are 0 or less; every projected z is capped at 0xFFFFFF;
 *   - the packet (0x90 bytes at gOtCur) is one gouraud textured alpha-blended triangle between CLAMP = 5 (clamp s
 *     and t) and CLAMP = 0: GIF tag of 14 registers PRIM, TEX0, CLAMP, 3 x (RGBAQ, ST, XYZF2), CLAMP, NOP; the
 *     second GS context is used for layers 2 and 3. Colours are the floats converted to bytes, q and s / t come
 *     from the projection. In draw mode 0 the vertex z is the projected z, in modes 1 and 2 it is 0xFFFFFF (drawn
 *     in front of everything);
 *   - the packet is linked into the ordering table at depth z (clamped to 0..0xFFF), in the chain of the layer
 *     (layer - 2 for layers 2 and 3).
 * DrawQuad sends the two triangles (0, 1, 2) and (1, 2, 3) of the quad. DrawQuadSubdiv cuts the quad into four
 * quarters first (corner 0 and the midpoints towards corners 1, 2, 3, shifted by each of the four half
 * diagonals) and gives every quarter the whole texture, mirrored so the pieces join (gEftSprSubdivCorner).
 * Neither reads anything but its arguments, the clip planes and the display list.
 */
#if 0 /* NON-MATCHING. DrawQuad: the same length and blocks; what differs is (1) the original computes the GS
         context bit as an int, uses it unextended for the TEX0 / first CLAMP register numbers and sign-extended
         from 8 bits for PRIM and the last CLAMP (here one s8 is used for all four), (2) it does so after the
         "i < n" entry test of the fan loop (here before it), (3) the two stq pointers swap registers and
         two header stores swap. DrawQuadSubdiv: the head (quarters, uv, 151 instructions) is identical; the
         rest is the same inlined code with the same differences.
         Second pass (cleanup step 12, about 200 variants, scratch in build/scratch_cleanup2_F/eftad/): the insns
         the original has are reproduced by `s32 ctx = otLayer >= 2;` inside QueueTri with
         prim = (abe << 6) | ((s64)(s8)ctx << 9) | 0x1B (abe an s64 local = 1) and
         gif1 = ((s64)ctx << 4) + ((s64)(s8)ctx << 48) + ((s64)ctx << 8) + 0xF8421421421860: the first loop pass
         hoists the chain into the fan loop's preheader as in the original, but the rerun of the loop pass then
         moves slti / xori (and what depends on them) out of the j loop as well (456 instructions, 98 differ).
         The rerun moves the slti when 64 * 2 * (1 + lifetime of the xori result) >= insns of the j loop (363),
         so it stays only when the xori result has ONE use, in the next insn. Forms with one use (an int copied
         to an s64, s8 taken from the s64) keep the chain in place (450 instructions, 46..51 differ) but add an
         `andi 0xff` (DI to QI truncation) the original does not have; an s8 variable or parameter gives SI
         sll / sra; ctx computed at the call site, at the top of the fan loop body or in an explicit
         `if (i < n) { ...; do { } while }` preheader is hoisted by the rerun all the same. Not found: a form in
         which the int has the two uses the original shows (64-bit shift pair on it, and << 4 / << 8) and is
         still left alone by the rerun. The z caps, the header store order (emitted: vif1, tag, vif0, gif0,
         clamp, next, prim, pad, gif1, clampEnd) and the t7 / s0 swap of the stq pointers were not settled
         either (best order tried: prim, tag, vif1, vif0, gif0, clamp, next, pad, gif1, clampEnd).
         Round 4 (no better attempt; a register-masked structural diff of this one is 37 lines, 35 with the
         header order prim, tag, vif0, vif1, gif0, gif1, clamp, next): what the dumps add to the above.
         (a) `slti` is not moved by the loop pass at all: gcse's PRE inserts `r = otLayer < 2` on the entry edge
         of the fan loop (behind the `i < n` test, where the original has it), so only the `xori` and what hangs
         on it are the loop pass's business. (b) A chain is moved as a whole only through "forces": a register
         used exactly once, by the next movable, hands its savings and lifetime to that one. The fan loop's
         first run therefore moves the xori only when its result lives 6 insns or more inside the fan loop
         (64 * 1 * lifetime >= 338), i.e. the context bit is computed EARLY in the loop body with code that is
         not moved between it and its uses; once hoisted the uses stand right behind it and the j loop's rerun
         sees a short lifetime. (c) In the rerun the slti (now inside the j loop, result used once) forces the
         xori, so the test is 64 * 2 * (1 + lifetime) against 364: lifetime 1 stays (256), lifetime 2 goes
         (384, 20 over). The original's xori result has two RTL uses at most two insns away (a sign extension
         to 64 bits for << 4 and << 8, which is a plain move, and the 8-bit sign extension for << 9 and << 48),
         so its j loop was at least 21 RTL insns longer than this attempt's at that point, or the slti did not
         force (its result used a second time). Neither was reproduced. (d) `li s1,2` sits in the delay slot of
         the ClipPoly_ProjectCur call in the original (here behind it), and the fan's entry test has the j
         reload `lw a0,748(sp)` as a dead slot instruction taken from the loop end. */
/* Queues one triangle (see above). ctx: 1 for the second GS context. */
static inline void EftSprAnim_QueueTri(EftAeScr *v0, EftAeScr *v1, EftAeScr *v2, Vec4 *c0, Vec4 *c1, Vec4 *c2,
                                       Vec4 *uv0, Vec4 *uv1, Vec4 *uv2, s32 otLayer, s32 z, u64 tex0, s32 mode, s8 ctx) {
    s32 abe = 1;
    s32 l;
    EftAeTriPkt *p;
    OtEntry *e;

    if (c0->w <= 0.0f && c1->w <= 0.0f && c2->w <= 0.0f) {
        return;
    }
    p = (EftAeTriPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    p->prim = ((u64)abe << 6) | ((u64)ctx << 9) | 0x1B;
    p->h.gif1 = ((u64)ctx << 4) + ((u64)ctx << 48) + ((u64)ctx << 8) + 0xF8421421421860;
    p->h.vif1 = 0x50000008;
    p->h.tag = 0x20000008;
    p->h.vif0 = 0x10000000;
    p->h.gif0 = 0xE400000000008001;
    p->clamp = 5;
    p->h.next = 0;
    p->pad = 0;
    p->clampEnd = 0;
    p->v[0].rgba[0] = c0->x;
    p->v[0].rgba[1] = c0->y;
    p->v[0].rgba[2] = c0->z;
    p->v[0].rgba[3] = c0->w;
    p->v[0].q = uv0->z;
    p->v[1].rgba[0] = c1->x;
    p->v[1].rgba[1] = c1->y;
    p->v[1].rgba[2] = c1->z;
    p->v[1].rgba[3] = c1->w;
    p->v[1].q = uv1->z;
    p->v[2].rgba[0] = c2->x;
    p->v[2].rgba[1] = c2->y;
    p->v[2].rgba[2] = c2->z;
    p->v[2].rgba[3] = c2->w;
    p->v[2].q = uv2->z;
    p->tex0 = tex0;
    p->v[0].s = uv0->x;
    p->v[0].t = uv0->y;
    p->v[1].s = uv1->x;
    p->v[1].t = uv1->y;
    p->v[2].s = uv2->x;
    p->v[2].t = uv2->y;
    if (mode != 0) {
        p->v[0].xyz.x = v0->x;
        p->v[0].xyz.y = v0->y;
        p->v[0].xyz.z = 0xFFFFFF;
        p->v[0].xyz.f = 0xFF;
        p->v[1].xyz.x = v1->x;
        p->v[1].xyz.y = v1->y;
        p->v[1].xyz.z = 0xFFFFFF;
        p->v[1].xyz.f = 0xFF;
        p->v[2].xyz.x = v2->x;
        p->v[2].xyz.y = v2->y;
        p->v[2].xyz.z = 0xFFFFFF;
        p->v[2].xyz.f = 0xFF;
    } else {
        p->v[0].xyz.x = v0->x;
        p->v[0].xyz.y = v0->y;
        p->v[0].xyz.z = v0->z;
        p->v[0].xyz.f = 0xFF;
        p->v[1].xyz.x = v1->x;
        p->v[1].xyz.y = v1->y;
        p->v[1].xyz.z = v1->z;
        p->v[1].xyz.f = 0xFF;
        p->v[2].xyz.x = v2->x;
        p->v[2].xyz.y = v2->y;
        p->v[2].xyz.z = v2->z;
        p->v[2].xyz.f = 0xFF;
    }
    l = otLayer;
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

/* Clips a triangle against the five planes of the view, projects what is left and queues it as a fan. */
static inline void EftSprAnim_DrawTriClip(EftAeClipVtx *poly, s32 otLayer, s32 z, u64 tex0, s32 mode) {
#ifdef PORT
    /* The triangle is clipped in place and can grow to eight corners, but callers hand over room for three. On
       the PS2 the extra corners land in the caller's other locals; on PC they reach the return address (the
       crash in EftRibbon_DrawStrip). The work is done on a copy with room. */
    EftAeClipVtx portPoly[16];
    __builtin_memcpy(portPoly, poly, 3 * sizeof(EftAeClipVtx));
    poly = portPoly;
#endif
    EftAeScr scr[9];
    Vec4 stq[9];
    Vec4 *plane;
    s32 n = 3;
    s32 i;

    EftGfx_UpdateClipPlanes();
    plane = EftGfx_GetClipPlanes();
    for (i = 0; i < 5; i++) {
        n = ClipPoly_ClipPlane(poly, plane, n);
        plane++;
    }
    if (n != 0) {
        s8 ctx;

        ClipPoly_ProjectCur(scr, stq, poly, n);
        ctx = otLayer >= 2;
        for (i = 2; i < n; i++) {
            if (scr[0].z > 0xFFFFFF) {
                scr[0].z = 0xFFFFFF;
            }
            if (scr[i - 1].z > 0xFFFFFF) {
                scr[i - 1].z = 0xFFFFFF;
            }
            if (scr[i].z > 0xFFFFFF) {
                scr[i].z = 0xFFFFFF;
            }
            EftSprAnim_QueueTri(&scr[0], &scr[i - 1], &scr[i], &poly[0].col, &poly[i - 1].col, &poly[i].col,
                                &stq[0], &stq[i - 1], &stq[i], otLayer, z, tex0, mode, ctx);
        }
    }
}

/* Draws a quad (two triangles from four corners), each clipped against the five planes of the view. */
void EftSprAnim_DrawQuad(u64 tex0, Vec4 *corner, Vec4 *uv, Vec4 *color, s32 otLayer, s32 z, s32 mode) {
    EftAeClipVtx poly[9];
    s32 j;

    memset(poly, 0, sizeof(poly));
    for (j = 0; j < 2; j++) {
        ClipVtx_SetArray(poly, &corner[j], &uv[j], &color[j], 3);
        EftSprAnim_DrawTriClip(poly, otLayer, z, tex0, mode);
    }
}

void EftSprAnim_DrawQuadSubdiv(u64 tex0, Vec4 *corner, Vec4 *uv, Vec4 *color, s32 otLayer, s32 z, s32 mode) {
    Vec4 quarter[4];
    Vec4 sub[4];
    Vec4 subUv[4];
    Vec4 off[4];
    EftAeClipVtx poly[15];
    s32 q;
    s32 k;
    s32 j;

    quarter[0].x = corner[0].x;
    quarter[0].y = corner[0].y;
    quarter[0].z = corner[0].z;
    quarter[0].w = 1.0f;
    quarter[1].x = (corner[0].x + corner[1].x) * 0.5f;
    quarter[1].y = (corner[0].y + corner[1].y) * 0.5f;
    quarter[1].z = (corner[0].z + corner[1].z) * 0.5f;
    quarter[1].w = 1.0f;
    quarter[2].x = (corner[0].x + corner[2].x) * 0.5f;
    quarter[2].y = (corner[0].y + corner[2].y) * 0.5f;
    quarter[2].z = (corner[0].z + corner[2].z) * 0.5f;
    quarter[2].w = 1.0f;
    quarter[3].x = (corner[0].x + corner[3].x) * 0.5f;
    quarter[3].y = (corner[0].y + corner[3].y) * 0.5f;
    quarter[3].z = (corner[0].z + corner[3].z) * 0.5f;
    quarter[3].w = 1.0f;
    off[0].x = 0.0f;
    off[0].y = 0.0f;
    off[0].z = 0.0f;
    off[1].x = quarter[1].x - corner[0].x;
    off[1].y = quarter[1].y - corner[0].y;
    off[1].z = quarter[1].z - corner[0].z;
    off[2].x = quarter[2].x - corner[0].x;
    off[2].y = quarter[2].y - corner[0].y;
    off[2].z = quarter[2].z - corner[0].z;
    off[3].x = quarter[3].x - corner[0].x;
    off[3].y = quarter[3].y - corner[0].y;
    off[3].z = quarter[3].z - corner[0].z;
    for (q = 0; q < 4; q++) {
        for (k = 0; k < 4; k++) {
            Vec3_Add(&sub[k], &quarter[k], &off[q]);
            Vec4_Copy(&subUv[k], &uv[gEftSprSubdivCorner[q][k]]);
            sub[k].w = 1.0f;
            subUv[k].z = 1.0f;
            subUv[k].w = 1.0f;
        }
        for (j = 0; j < 2; j++) {
            ClipVtx_SetArray(poly, &sub[j], &subUv[j], &color[j], 3);
            EftSprAnim_DrawTriClip(poly, otLayer, z, tex0, mode);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/eft_ae", EftSprAnim_DrawQuad);
INCLUDE_ASM("asm/nonmatchings/battle/eft_ae", EftSprAnim_DrawQuadSubdiv);
#endif

/* Returns the pack (after a stripped check of its magic). */
EftSprPack *EftSprPack_Get(EftSprPack *pack) {
    EftSprPack_IsValid(pack);
    return pack;
}

/* Points every layer at its keys, which follow each other from the pack's key offset. */
void EftSprPack_Bind(EftSprPack *pack) {
    EftSprLayerDef *def = EftSprPack_GetLayers(pack);
    EftSprKey *key = (EftSprKey *)((u8 *)pack + pack->keysOfs);
    s32 i;

    for (i = 0; i < pack->layerCount; i++) {
        def->keys = key;
        key += def->keyCount;
        def++;
    }
}

/* Number of layers. */
s32 EftSprPack_GetLayerCount(EftSprPack *pack) {
    EftSprPack_IsValid(pack);
    return pack->layerCount;
}

/* The layer definitions. */
EftSprLayerDef *EftSprPack_GetLayers(EftSprPack *pack) {
    EftSprPack_IsValid(pack);
    return pack->layer;
}

/* Frame of the key a looping layer jumps back from. */
s32 EftSprPack_GetLoopEndFrame(EftSprLayerDef *def) {
    return def->keys[def->loopEndKey].frame;
}

/* Frame of the key a looping layer jumps back to. */
s32 EftSprPack_GetLoopStartFrame(EftSprLayerDef *def) {
    return def->keys[def->loopStartKey].frame;
}

/* Number of keys. No caller. */
s32 EftSprPack_GetKeyCount(EftSprLayerDef *def) {
    return def->keyCount;
}

/* Index of the key whose span contains a frame; -1 when none does. */
s32 EftSprPack_FindKey(EftSprLayerDef *def, s32 frame) {
    s32 found = -1;
    s32 i;

    for (i = 0; i < def->keyCount - 1; i++) {
        if (frame >= def->keys[i].frame && frame <= def->keys[i + 1].frame) {
            found = i;
            break;
        }
    }
    return found;
}

/* Position of a frame inside a key's span, 0..1 (1 for an empty span, 0 for no key). */
f32 EftSprPack_GetKeyT(EftSprLayerDef *def, s32 frame, s32 key) {
    f32 zero = 0.0f;
    f32 t;

    if (key < 0) {
        return zero;
    }
    {
        s32 first = def->keys[key].frame;
        f32 span = def->keys[key + 1].frame - first;
        f32 at = frame - first;

        t = 1.0f;
        if (span != zero) {
            t = at / span;
        }
    }
    return t;
}

/* Frame of the first key. No caller. */
s32 EftSprPack_GetFirstFrame(EftSprLayerDef *def) {
    return def->keys[0].frame;
}

/* Frame of the last key. No caller. */
s32 EftSprPack_GetLastFrame(EftSprLayerDef *def) {
    return def->keys[def->keyCount - 1].frame;
}

/* Whether the pack starts with "V000". */
s32 EftSprPack_IsValid(EftSprPack *pack) {
    return pack->magic == 0x30303056;
}

/* Updates the root list. No caller. */
void BtlTaskList_UpdateRoot(void) {
    BtlTaskList_Update(gBtlTaskRoot);
}

/* Runs the update callback of every task of a list, in list order, each followed by its child list; a task
   whose dead bit is set is removed instead. The task being updated is gBtlTaskCur. */
void BtlTaskList_Update(BtlTaskList *list) {
    BtlTask *task = list->head;
    BtlTask *next;

    while (task != NULL) {
        if (task->cls->update != NULL) {
            gBtlTaskCur = task;
            task->cls->update(task);
            if (task->cls->draw != NULL) {
                BTL_TASK_BITS(task)->updated = 1;
            }
        }
        next = task->next;
        if (BTL_TASK_BITS(task)->dead) {
            BtlTask_Kill(task);
        } else if (task->child != NULL) {
            BtlTaskList_Update(task->child);
        }
        task = next;
    }
}

/* Runs the post-update callbacks, in the same order. Dead tasks are left for the next update. */
void BtlTaskList_PostUpdate(BtlTaskList *list) {
    BtlTask *task = list->head;
    BtlTask *next;

    while (task != NULL) {
        if (task->cls->postUpdate != NULL) {
            task->cls->postUpdate(task);
        }
        next = task->next;
        if (!BTL_TASK_BITS(task)->dead && task->child != NULL) {
            BtlTaskList_PostUpdate(task->child);
        }
        task = next;
    }
}

/* Runs the reset callback of every task (mask 0), or of the tasks tagged with the requested owner/group mask;
   tasks whose dead bit is set afterwards are removed. */
void BtlTaskList_Reset(BtlTaskList *list, s32 mask) {
    BtlTask *task = list->head;
    BtlTask *next;

    while (task != NULL) {
        if (mask == 0 || (task->tag & mask)) {
            if (task->cls->reset != NULL) {
                task->cls->reset(task);
            }
        }
        next = task->next;
        if (BTL_TASK_BITS(task)->dead) {
            BtlTask_Kill(task);
        } else if (task->child != NULL) {
            BtlTaskList_Reset(task->child, mask);
        }
        task = next;
    }
}

/* Runs the draw callbacks of the tasks that have been updated at least once. */
void BtlTaskList_Draw(BtlTaskList *list) {
    BtlTask *task = list->head;
    BtlTask *next;

    while (task != NULL) {
        if (task->cls->draw != NULL && BTL_TASK_BITS(task)->updated) {
            task->cls->draw(task);
        }
        next = task->next;
        if (!BTL_TASK_BITS(task)->dead && task->child != NULL) {
            BtlTaskList_Draw(task->child);
        }
        task = next;
    }
}

/* Creates the root list. No caller (the scene keeps its own root). */
void BtlTaskList_CreateRoot(u32 count, u32 workSize) {
    gBtlTaskRoot = BtlTaskList_Create(NULL, count, workSize);
}

/* Gives a task a child list of `count` tasks with `workSize` bytes of work each. */
BtlTaskList *BtlTask_CreateChildList(BtlTask *task, u32 count, u32 workSize) {
    return task->child = BtlTaskList_Create(task, count, workSize);
}

/* The same for the task being updated; returns the list. No caller. */
BtlTaskList *BtlTask_CreateChildListCur(u32 count, u32 workSize) {
    gBtlTaskCur->child = BtlTaskList_Create(gBtlTaskCur, count, workSize);
    return gBtlTaskCur->child;
}

/* Allocates a list, its tasks and their work blocks from the current battle pool. NULL when the parent already
   has a child list or the pool is full. */
BtlTaskList *BtlTaskList_Create(BtlTask *parent, u32 count, u32 workSize) {
    BtlTaskList *list;
    BtlTask *task;
    u8 *work;
    u16 i;

    if (parent != NULL && parent->child != NULL) {
        return NULL;
    }
    list = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(BtlTaskList));
    if (list == NULL) {
        return NULL;
    }
    task = NULL;
    if (count != 0) {
        task = BtlPool_Alloc(BtlPool_GetCurrent(), count * sizeof(BtlTask));
    }
    if (task == NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), list);
        return NULL;
    }
    workSize = (workSize + 3) / 4 * 4;
    work = NULL;
    if (workSize != 0) {
        work = BtlPool_Alloc(BtlPool_GetCurrent(), workSize * count);
        if (work == NULL) {
            BtlPool_Free(BtlPool_GetCurrent(), task);
            BtlPool_Free(BtlPool_GetCurrent(), list);
            return NULL;
        }
    }
    list->parent = parent;
    list->head = NULL;
    list->tail = NULL;
    list->free = task;
    list->tasks = task;
    list->work = work;
    for (i = 0; i < count; i++) {
        task->index = i;
        task->work = work;
        if (work != NULL) {
            work += workSize;
        }
        task->nextFree = task + 1;
        task->list = list;
        task++;
    }
    list->tasks[count - 1].nextFree = NULL;
    return list;
}

/* Destroys the root list. No caller. */
void BtlTaskList_DestroyRoot(void) {
    BtlTaskList_Destroy(gBtlTaskRoot);
    gBtlTaskRoot = NULL;
}

/* Destroys a task's child list. */
void BtlTask_DestroyChildList(BtlTask *task) {
    BtlTaskList_Destroy(task->child);
    task->child = NULL;
}

/* Kills every task of a list and frees its memory. */
void BtlTaskList_Destroy(BtlTaskList *list) {
    if (list != NULL) {
        BtlTaskList_KillAll(list);
        if (list->work != NULL) {
            BtlPool_Free(BtlPool_GetCurrent(), list->work);
        }
        BtlPool_Free(BtlPool_GetCurrent(), list->tasks);
        BtlPool_Free(BtlPool_GetCurrent(), list);
    }
}

/* Adds a task to an empty list. Does not check that a free task exists. */
BtlTask *BtlTaskList_AddFirst(BtlTaskList *list, BtlTaskCls *cls, void *arg) {
    BtlTask *task = list->free;

    list->tail = task;
    list->head = task;
    task->prev = NULL;
    task->next = NULL;
    list->free = task->nextFree;
    task->nextFree = NULL;
    BtlTask_Init(task, cls, arg);
    return task;
}

/* Adds a task at the head of a list (it is updated first). NULL when the list is full. */
BtlTask *BtlTaskList_AddHead(BtlTaskList *list, BtlTaskCls *cls, void *arg) {
    BtlTask *task;

    if (list->head == NULL) {
        task = BtlTaskList_AddFirst(list, cls, arg);
    } else {
        task = BtlTask_AddBefore(list->head, cls, arg);
    }
    return task;
}

/* Adds a task at the tail of a list (it is updated last). NULL when the list is full. */
BtlTask *BtlTaskList_AddTail(BtlTaskList *list, BtlTaskCls *cls, void *arg) {
    BtlTask *task;

    if (list->tail == NULL) {
        task = BtlTaskList_AddFirst(list, cls, arg);
    } else {
        task = BtlTask_AddAfter(list->tail, cls, arg);
    }
    return task;
}

/* Adds a task right after the one being updated. No caller. */
BtlTask *BtlTask_AddAfterCur(BtlTaskCls *cls, void *arg) {
    return BtlTask_AddAfter(gBtlTaskCur, cls, arg);
}

/* Adds a task right after another, in the same list. NULL when the list is full. */
BtlTask *BtlTask_AddAfter(BtlTask *task, BtlTaskCls *cls, void *arg) {
    BtlTaskList *list = task->list;
    BtlTask *add = list->free;
    BtlTask *next;

    if (add == NULL) {
        return NULL;
    }
    if (list->head == NULL) {
        return BtlTaskList_AddFirst(list, cls, arg);
    }
    next = task->next;
    add->prev = task;
    add->next = next;
    if (task == list->tail) {
        list->tail = add;
    } else {
        task->next->prev = add;
    }
    task->next = add;
    list->free = add->nextFree;
    add->nextFree = NULL;
    BtlTask_Init(add, cls, arg);
    return add;
}

/* Adds a task right before the one being updated. No caller. */
BtlTask *BtlTask_AddBeforeCur(BtlTaskCls *cls, void *arg) {
    return BtlTask_AddBefore(gBtlTaskCur, cls, arg);
}

/* Adds a task right before another, in the same list. NULL when the list is full. */
BtlTask *BtlTask_AddBefore(BtlTask *task, BtlTaskCls *cls, void *arg) {
    BtlTaskList *list = task->list;
    BtlTask *add = list->free;

    if (add == NULL) {
        return NULL;
    }
    if (list->head == NULL) {
        return BtlTaskList_AddFirst(list, cls, arg);
    }
    add->next = task;
    add->prev = task->prev;
    if (task == list->head) {
        list->head = add;
    } else {
        task->prev->next = add;
    }
    task->prev = add;
    list->free = add->nextFree;
    add->nextFree = NULL;
    BtlTask_Init(add, cls, arg);
    return add;
}

/* Clears a task's state, binds its class and runs the class's init callback with the creation argument. */
void BtlTask_Init(BtlTask *task, BtlTaskCls *cls, void *arg) {
    task->bits = ~3;
    task->pos.w = 1.0f;
    task->chr = 0;
    task->child = NULL;
    task->cls = cls;
    task->tag = 0;
    task->unk8 = 0;
    task->unkA = 0;
    task->unkB = 0;
    task->pos.x = task->pos.y = task->pos.z = 0.0f;
    if (cls->init != NULL) {
        cls->init(task, arg);
    }
}

/* Removes every task of a list at once (term callbacks run). */
void BtlTaskList_KillAll(BtlTaskList *list) {
    while (list->head != NULL) {
        BtlTask_Kill(list->head);
    }
}

/* Marks the task being updated as dead. No caller. */
void BtlTask_SetDeadCur(void) {
    BtlTask_SetDead(gBtlTaskCur);
}

/* Marks a task as dead: the list pass that next reaches it (update or reset) removes it. */
void BtlTask_SetDead(BtlTask *task) {
    BTL_TASK_BITS(task)->dead = 1;
}

/* Removes the task being updated at once. No caller. */
void BtlTask_KillCur(void) {
    BtlTask_Kill(gBtlTaskCur);
}

/* Removes a task now: term callback, child list destroyed, unlinked, returned to the list's free tasks. */
void BtlTask_Kill(BtlTask *task) {
    BtlTaskList *list = task->list;

    if (task->cls->term != NULL) {
        task->cls->term(task);
    }
    if (task->child != NULL) {
        BtlTaskList_KillAll(task->child);
        BtlTaskList_Destroy(task->child);
        task->child = NULL;
    }
    if (task == list->head) {
        list->head = task->next;
    } else {
        task->prev->next = task->next;
    }
    if (task == list->tail) {
        list->tail = task->prev;
    } else {
        task->next->prev = task->prev;
    }
    task->nextFree = list->free;
    list->free = task;
}

/* Sets bits of a task's tag word (the hit pass reports its results with it). */
void BtlTask_SetTagBits(BtlTask *task, s32 bits) {
    task->tag |= bits;
}

/* Stores a position in a task (the hit pass reports the contact point with it). */
void BtlTask_SetPos(BtlTask *task, EftAeVec pos) {
    Vec4_Copy(&task->pos, (Vec4 *)&pos);
}

/* Sets bits of a task's tag word; fighter masks come from BtlScene_CharOwnerTag. */
void BtlTask_SetOwnerTag(BtlTask *task, s32 bits) {
    task->tag |= bits;
}

/* The root list. No caller. */
BtlTaskList *BtlTaskList_GetRoot(void) {
    return gBtlTaskRoot;
}

/* The task being updated. No caller. */
BtlTask *BtlTask_GetCurrent(void) {
    return gBtlTaskCur;
}

/* The task that owns the list a task is in. */
BtlTask *BtlTask_GetParent(BtlTask *task) {
    return task->list->parent;
}

/* Allocates the effect VRAM list (from BtlScene_Init). */
void EftVram_Init(void) {
    gEftVram = Heap_Alloc(sizeof(EftAeVram), 0x20, 0, 2);
    memset(gEftVram, 0, sizeof(EftAeVram));
    gEftVram->capacity=128;
    gEftVram->entry=Heap_Alloc(128*sizeof(EftAeVramEntry),0x20,0,2);
    gEftVram->imageNext = 0x2A00;
    gEftVram->clutNext = 0x2A00;
}

/* Frees it. */
void EftVram_Term(void) {
    if (gEftVram != NULL) {
        Heap_Free(gEftVram->entry);
        Heap_Free(gEftVram);
        gEftVram = NULL;
    }
}

/* Sets the two allocation cursors. No caller. */
void EftVram_SetCursor(s32 imageBlock, s32 clutBlock) {
    gEftVram->imageNext = imageBlock;
    gEftVram->clutNext = clutBlock;
}

/* Reads the two allocation cursors. No caller. */
void EftVram_GetCursor(s32 *imageBlock, s32 *clutBlock) {
    *imageBlock = gEftVram->imageNext;
    *clutBlock = gEftVram->clutNext;
}

/* Gives a texture VRAM for this frame (image and CLUT), queues its upload and returns its TEX0 with the new
   TBP0 / CBP and the given TCC / TFX. The entry's own TEX0 keeps the base and palette fields cleared. */
static EftAeVramEntry *EftVram_NextEntry(void) {
    /* The PS2 list had 128 unchecked entries. Concurrent techniques can exceed
       that before rendering, overwriting the count and then arbitrary memory. */
    if(gEftVram->count==gEftVram->capacity) {
        u32 n=gEftVram->capacity*2;
        EftAeVramEntry *entries=Heap_Alloc(n*sizeof(*entries),0x20,0,2);
        __builtin_memcpy(entries,gEftVram->entry,gEftVram->count*sizeof(*entries));
        Heap_Free(gEftVram->entry);
        gEftVram->entry=entries;gEftVram->capacity=n;
    }
    return &gEftVram->entry[gEftVram->count];
}

/* Images and palettes used to grow independently from 0x2B00 and 0x2A00.
   Four beam helpers exceed that 256-block palette gap and upload CLUTs on
   top of the stage's smoke. Reserve both from the same cursor. A texture
   shared by several emitters needs only one upload in this draw frame. */
static EftAeVramEntry *EftVram_Acquire(EftAeTex *tex, s32 flags) {
    EftAeVramEntry *e = NULL;
    u32 i;
    s32 next = gEftVram->imageNext > gEftVram->clutNext ?
               gEftVram->imageNext : gEftVram->clutNext;
    for (i = 0; i < gEftVram->count; i++) {
        if (gEftVram->entry[i].tex->image == tex->image) {
            e = &gEftVram->entry[i];
            break;
        }
    }
    if (e == NULL) {
        e = EftVram_NextEntry();
        e->tex = tex;
        e->flags = 0;
        e->imageBlock = e->clutBlock = -1;
        gEftVram->count++;
    }
    if ((flags & 1) && !(e->flags & 1)) {
        e->imageBlock = next;
        next += tex->image->imageBlocks;
    }
    if ((flags & 2) && !(e->flags & 2)) {
        e->clutBlock = next;
        next += tex->image->clutBlocks;
    }
    e->flags |= flags;
    gEftVram->imageNext = gEftVram->clutNext = next;
    return e;
}

u64 EftVram_AddTex(EftAeTex *tex, s32 tcc, s32 tfx) {
    EftAeVramEntry *e = EftVram_Acquire(tex, 3);
    u64 tex0;

    tex->tex0 &= 0xFFF80003FFFFC000;
    tex0 = ((tex->tex0 | ((u64)tcc << 34)) | (((u64)tfx << 35) | (u32)e->imageBlock)) |
           ((u64)(u32)e->clutBlock << 37);
    return tex0;
}

/* The same for the image only (the CLUT field stays cleared). */
u64 EftVram_AddImage(EftAeTex *tex, s32 tcc, s32 tfx) {
    EftAeVramEntry *e = EftVram_Acquire(tex, 1);
    u64 tex0;

    tex->tex0 &= 0xFFF80003FFFFC000;
    tex0 = (tex->tex0 | ((u64)tcc << 34)) | (((u64)tfx << 35) | (u32)e->imageBlock);
    return tex0;
}

/* Gives a texture's CLUT VRAM for this frame and returns its block. */
u64 EftVram_AddClut(EftAeTex *tex) {
    EftAeVramEntry *e = EftVram_Acquire(tex, 2);
    u64 block;

    block = (u32)e->clutBlock;
    return block;
}

/* Queues a texture's upload at given blocks and returns its TEX0. No caller. */
u64 EftVram_AddTexAt(EftAeTex *tex, s32 imageBlock, s32 clutBlock, s32 tcc, s32 tfx) {
    EftAeVramEntry *e = EftVram_NextEntry();

    e->flags = 3;
    e->imageBlock = imageBlock;
    e->clutBlock = clutBlock;
    e->tex = tex;
    gEftVram->count++;
    tex->tex0 &= 0xFFF80003FFFFC000;
    return ((tex->tex0 | ((u64)tcc << 34)) | (((u64)tfx << 35) | (u32)imageBlock)) | ((u64)clutBlock << 37);
}

/* Gives every texture of a 32-entry set VRAM for this frame. */
void EftTexSet_Keep32(EftAeTexSet32 *set, s32 tcc, s32 tfx) {
    s32 i;

    for (i = 0; i < set->count; i++) {
        set->tex[i].tex0 = EftVram_AddTex(&set->tex[i], tcc, tfx);
    }
}

/* The same for a 4-entry set. */
void EftTexSet_Keep4(EftAeTexSet4 *set, s32 tcc, s32 tfx) {
    s32 i;

    for (i = 0; i < set->count; i++) {
        set->tex[i].tex0 = EftVram_AddTex(&set->tex[i], tcc, tfx);
    }
}

/* The same for an 8-entry set. No caller. */
void EftTexSet_Keep8(EftAeTexSet8 *set, s32 tcc, s32 tfx) {
    s32 i;

    for (i = 0; i < set->count; i++) {
        set->tex[i].tex0 = EftVram_AddTex(&set->tex[i], tcc, tfx);
    }
}

/* The same for a 16-entry set. No caller. */
void EftTexSet_Keep16(EftAeTexSet16 *set, s32 tcc, s32 tfx) {
    s32 i;

    for (i = 0; i < set->count; i++) {
        set->tex[i].tex0 = EftVram_AddTex(&set->tex[i], tcc, tfx);
    }
}

/* Uploads every texture queued this frame (from BtlScene_Draw, once per frame). */
void EftVram_UploadAll(void) {
    u32 i;

    for (i = 0; i < gEftVram->count; i++) {
        EftAeVramEntry *e = &gEftVram->entry[i];
        s32 clutBlock = -1;
        s32 imageBlock = -1;

        if (e->flags & 1) {
            imageBlock = e->imageBlock;
        }
        if (e->flags & 2) {
            clutBlock = e->clutBlock;
        }
        EftVram_Upload(e->tex, imageBlock, clutBlock);
    }
}

/* Empties the list and rewinds the cursors (from BtlScene_Draw, after the upload). */
void EftVram_Reset(void) {
    gEftVram->imageNext = 0x2A00;
    gEftVram->clutNext = 0x2A00;
    gEftVram->count = 0;
}

/* Empty: a stripped assert that a pack's texture count fits the set. */
void EftTexSet_CheckCount(s32 count, s32 max) {
}

/* Builds a texture set of at most 32 entries from a texture pack. */
void EftTexSet_Load32(EftAeTexSet32 *set, void *pack) {
    EftAeTexPack *hdr;
    s32 count;
    s32 i;

    memset(set, 0, sizeof(EftAeTexSet32));
    Res_RelocateOffsets(&hdr, pack, pack);
    count = hdr->count;
    set->count = count;
    EftTexSet_CheckCount(count, 32);
    for (i = 0; i < set->count; i++) {
        set->tex[i].image = &hdr->images[i];
        set->tex[i].tex0 = hdr->images[i].tex0;
    }
}

/* Builds a texture set of at most 4 entries from a texture pack. */
void EftTexSet_Load4(EftAeTexSet4 *set, void *pack) {
    EftAeTexPack *hdr;
    s32 count;
    s32 i;

    memset(set, 0, sizeof(EftAeTexSet4));
    Res_RelocateOffsets(&hdr, pack, pack);
    count = hdr->count;
    set->count = count;
    EftTexSet_CheckCount(count, 4);
    for (i = 0; i < set->count; i++) {
        set->tex[i].image = &hdr->images[i];
        set->tex[i].tex0 = hdr->images[i].tex0;
    }
}
