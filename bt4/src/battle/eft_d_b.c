#include "common.h"
#include "battle/eft_d.h"
#include "sys/dma.h"
#include "sys/gfx.h"

/*
 * Stage-change transition: 0x13C300..0x13F430. First part (to 0x13EA00): how a particle is drawn (three
 * routines), the model drawn around the camera, the six particle initialisers and their seven per-frame updates.
 * Second part (merged in at integration from the head of eft_e.c): the task that owns them (EftBurst_Init /
 * Update / Draw / Start, 0x13EA00..0x13F3D8) and the two screen-position tests.
 *
 * One original source file, 0x13C300..0x13F430:
 * - EftBurst_Update only matches with EftBurst_InitFlash / InitRing / InitDebris defined above it;
 * - the two non-static inline functions the object emits at its end, Eft_IsScreenPosVisible (0x13F3D8) and
 *   Eft_IsScreenPosInFront (0x13F420), are the tests inlined in EftBurst_DrawModel and in the three particle
 *   draw routines (`scr[i][2] > 0`);
 * - the file's small data starts at gEftBurst (0x2FE9C8) and gEftBurstStreakAngle (0x2FE9CC), followed by the
 *   three -0.0f constants of InitFlash / InitRing / InitGlow (0x2FE9D0..0x2FE9DC), which this compiler puts in
 *   .sdata because `li.s` cannot express -0.0f.
 *
 * Nothing here touches the simulation. Everything is in camera space (the particles live around z = -50 in
 * front of the camera and are projected with the view's matrix at +0x140), so the transition looks the same
 * wherever the camera is. All randomness is libc rand(), consumed only when a particle is created.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 sqrtf(f32 x);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void Tex_Upload(EftBurstTex *tex, s32 x, s32 y); /* uploads a texture entry; the two numbers are GS block pointers (tbp, cbp), not a position */
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b); /* matrix product */
extern void Mtx_Transpose(Mtx44 *dst, Mtx44 *src);
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotation about one axis */
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotation about another axis */
extern void IVec4_Set(f32 *out, s32 a, s32 b, s32 c, s32 d); /* three packed 16-bit values to a vector */
extern void IVec4_ToFloat12(f32 *out, f32 *in);
extern void IVec4_ToFloat(f32 *out, s32 *in); /* integer vector to float vector */
extern void Vec4_ToInt(s32 *dst, Vec4 *src); /* float vector to integer vector */
extern void Vec4_Clamp(f32 *out, f32 *in, f32 min, f32 max); /* clamps a vector */
extern s32 Mtx_ProjectPoint(EftScrPos *out, Mtx44 *m, Vec4 *v); /* projects one point */
extern void Mtx_ProjectPointStq(s32 *scr, f32 *st, Mtx44 *m, f32 *pos, f32 *nrm); /* projects, makes env-map st */

/* Local vectors as plain arrays (sceVu0FVECTOR / sceVu0IVECTOR style): 16-byte aligned, so the compiler
   copies them with 64-bit loads. */
typedef f32 EftFVec[4] __attribute__((aligned(16)));
typedef s32 EftIVec[4] __attribute__((aligned(16)));

/* The same as a struct, for an initialised local that is built in place. */
typedef struct EftVec16 {
    f32 v[4];
} __attribute__((aligned(16))) EftVec16;

/* The part of the camera view (gBtlCamView) the transition reads. */
typedef struct EftView {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ Mtx44 unk40;    /* DrawSprite: its transform (Mtx_Transpose) is applied to the sprite's corners */
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 proj;     /* camera space to screen */
} EftView;

extern EftView *gBtlCamView;

/* Draws a particle as a camera-facing quad whose alpha rises and falls over its life. */
void EftBurst_DrawQuad(EftBurstPtcl *p) {
    EftFVec v;
    EftFVec corner[4] = {
        { -1.0f, -1.0f, 0.0f, 0.0f },
        { 1.0f, -1.0f, 0.0f, 0.0f },
        { -1.0f, 1.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f, 0.0f },
    };
    EftFVec st[4];
    EftIVec scr[4];
    u64 *pkt;
    s32 vis;
    s32 i;
    f32 scale;
    s32 life;
    s32 lifeMax;
    EftFVec *pc;
    EftIVec *ps;

    vis = 0;
    memset(st, 0, sizeof(st));
    st[1][0] = 1.0f;
    st[2][1] = 1.0f;
    st[3][0] = 1.0f;
    st[3][1] = 1.0f;
    pc = corner;
    scale = p->scale;
    ps = scr;
    life = p->life;
    lifeMax = p->lifeMax;
    for (i = 0; i < 4; i++) {
        Vec4_Scale((Vec4 *)v, (Vec4 *)pc[i], scale);
        Vec4_Add((Vec4 *)v, (Vec4 *)v, &p->pos);
        v[3] = 1.0f;
        Mtx_ProjectPoint((EftScrPos *)ps[i], &gBtlCamView->proj, (Vec4 *)v);
        vis += scr[i][2] > 0;
    }
    if (vis != 0) {
        EftBurst_SetTexture(p->tex, 0x2D00, 0x2A00);
        pkt = Dma_BeginDirect();
        pkt[0] = 0x1000000000008004;
        pkt[1] = 0xE;
        pkt += 2;
        pkt[0] = 0x1310000E0;
        pkt[1] = 0x4E; /* ZBUF_1: no depth writes */
        pkt += 2;
        pkt[0] = 0x48;
        pkt[1] = 0x42; /* ALPHA_1: additive */
        pkt += 2;
        pkt[0] = p->tex->tex0 | 0x5400400002D00;
        pkt[1] = 6; /* TEX0_1 */
        pkt += 2;
        pkt[0] = 0x73003;
        pkt[1] = 0x47; /* TEST_1 */
        pkt += 2;
        pkt[0] = 0x2400000000008001;
        pkt[1] = 0x10;
        pkt += 2;
        pkt[0] = 0x54; /* PRIM: textured, blended triangle strip */
        if (lifeMax < life * 2) {
            pkt[1] = ((s64)((lifeMax - life) * 16 / lifeMax) << 24) | 0x3F80000000808080;
        } else {
            pkt[1] = ((s64)(life * 16 / lifeMax) << 24) | 0x3F80000000808080;
        }
        pkt += 2;
        pkt[0] = 0x8400000000008001;
        pkt[1] = 0x52525252;
        pkt += 2;
        for (i = 0; i < 4; i++) {
            pkt[0] = (u64)((u32 *)st[i])[0] | ((u64)((u32 *)st[i])[1] << 32);
            pkt[1] = (s64)ps[i][0] | ((s64)ps[i][1] << 16) | ((s64)ps[i][2] << 32);
            pkt += 2;
        }
        Dma_EndDirect(pkt);
    }
}

/* Draws a particle as a quad that can be scaled per axis, rotated by its angles and tinted. The tables are
   D_002EC800 (corners) and D_002EC840 (st) in .rodata. `ps` (as in EftBurst_DrawQuad) is needed only for the
   Mtx_ProjectPoint call: with scr[i] there, the copy of the two tables uses other temporaries. */
void EftBurst_DrawQuadRot(EftBurstPtcl *p) {
    EftFVec v;
    EftFVec corner[4] = {
        { -1.0f, -1.0f, 0.0f, 0.0f },
        { 1.0f, -1.0f, 0.0f, 0.0f },
        { -1.0f, 1.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f, 0.0f },
    };
    EftFVec st[4] = {
        { 0.0f, 0.0f, 1.0f, 1.0f },
        { 1.0f, 0.0f, 1.0f, 1.0f },
        { 0.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
    };
    EftIVec scr[4];
    Mtx44 m;
    Mtx44 m2;
    u64 *pkt;
    s32 i;
    s32 flags;
    f32 scale;
    s32 life;
    s32 vis;
    s32 lifeMax;
    s32 alpha;
    s32 scaleY;
    s32 rotate;
    s32 scaleX;
    EftIVec *ps;

    vis = 0;
    ps = scr;
    flags = p->flags;
    scaleX = flags & EFT_BURST_F_SCALE_X;
    scaleY = flags & EFT_BURST_F_SCALE_Y;
    rotate = flags & EFT_BURST_F_ROTATE;
    scale = p->scale;
    life = p->life;
    lifeMax = p->lifeMax;
    alpha = p->alpha;
    for (i = 0; i < 4; i++) {
        Vec4_Copy((Vec4 *)v, (Vec4 *)corner[i]);
        if (scaleX) {
            v[0] *= scale;
        }
        if (scaleY) {
            v[1] *= scale;
        }
        if (rotate) {
            Mtx_StoreIdentity(&m);
            Mtx_RotateY(&m, &m, p->rot.y);
            Mtx_StoreIdentity(&m2);
            Mtx_RotateZ(&m2, &m2, p->rot.z);
            Mtx_Mul(&m, &m2, &m);
            Mtx_StoreIdentity(&m2);
            Mtx_RotateY(&m2, &m2, p->rot.x);
            Mtx_Mul(&m, &m, &m2);
            Mtx_MulVec4((Vec4 *)v, &m, (Vec4 *)v);
        }
        Vec4_Add((Vec4 *)v, (Vec4 *)v, &p->pos);
        v[3] = 1.0f;
        Mtx_ProjectPoint((EftScrPos *)ps[i], &gBtlCamView->proj, (Vec4 *)v);
        vis += scr[i][2] > 0;
    }
    if (vis != 0) {
        EftBurst_SetTexture(p->tex, 0x2D00, 0x2A00);
        pkt = Dma_BeginDirect();
        pkt[0] = 0x1000000000008004;
        pkt[1] = 0xE;
        pkt += 2;
        pkt[0] = 0x1310000E0;
        pkt[1] = 0x4E; /* ZBUF_1: no depth writes */
        pkt += 2;
        if (p->flags & EFT_BURST_F_ADD) {
            pkt[0] = 0x44;
            pkt[1] = 0x42; /* ALPHA_1 */
            pkt += 2;
        } else {
            pkt[0] = 0x48;
            pkt[1] = 0x42;
            pkt += 2;
        }
        pkt[0] = p->tex->tex0 | 0x5400400002D00;
        pkt[1] = 6; /* TEX0_1 */
        pkt += 2;
        pkt[0] = 0x73003;
        pkt[1] = 0x47; /* TEST_1 */
        pkt += 2;
        pkt[0] = 0x2400000000008001;
        pkt[1] = 0x10;
        pkt += 2;
        pkt[0] = 0x54; /* PRIM: textured, blended triangle strip */
        if (flags & EFT_BURST_F_COLOR) {
            pkt[1] = (u64)p->color | ((s64)(alpha * life / lifeMax) << 24) | 0x3F80000000000000;
        } else {
            pkt[1] = ((s64)(alpha * life / lifeMax) << 24) | 0x3F80000000805050;
        }
        pkt += 2;
        pkt[0] = 0x2400000000008004;
        pkt[1] = 0x52;
        pkt += 2;
        for (i = 0; i < 4; i++) {
            pkt[0] = (u64)((u32 *)st[i])[0] | ((u64)((u32 *)st[i])[1] << 32);
            pkt[1] = (s64)scr[i][0] | ((s64)scr[i][1] << 16) | ((s64)scr[i][2] << 32);
            pkt += 2;
        }
        Dma_EndDirect(pkt);
    }
}

/* Draws a particle as an axis-aligned sprite; its two corners are turned by the view's matrix at +0x40. */
void EftBurst_DrawSprite(EftBurstPtcl *p) {
    EftFVec v;
    EftFVec corner[2] = {
        { -1.0f, -1.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f, 0.0f },
    };
    EftFVec st[2] = {
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f, 0.0f },
    };
    EftIVec scr[2];
    Mtx44 m;
    u64 *pkt;
    s32 i;
    s32 vis;
    f32 scale;
    s32 life;
    s32 alpha;
    s32 lifeMax;
    s32 flags;

    vis = 0;
    scale = p->scale;
    life = p->life;
    lifeMax = p->lifeMax;
    alpha = p->alpha;
    flags = p->flags;
    Mtx_Transpose(&m, &gBtlCamView->unk40);
    for (i = 0; i < 2; i++) {
        Mtx_MulVec4((Vec4 *)v, &m, (Vec4 *)corner[i]);
        Vec4_Scale((Vec4 *)v, (Vec4 *)v, scale);
        Vec4_Add((Vec4 *)v, (Vec4 *)v, &p->pos);
        v[3] = 1.0f;
        Mtx_ProjectPoint((EftScrPos *)scr[i], &gBtlCamView->proj, (Vec4 *)v);
        vis += scr[i][2] > 0;
    }
    if (vis != 0) {
        EftBurst_SetTexture(p->tex, 0x2D00, 0x2A00);
        pkt = Dma_BeginDirect();
        pkt[0] = 0x1000000000008004;
        pkt[1] = 0xE;
        pkt += 2;
        pkt[0] = 0x1310000E0;
        pkt[1] = 0x4E; /* ZBUF_1: no depth writes */
        pkt += 2;
        if (flags & EFT_BURST_F_ADD) {
            pkt[0] = 0x44;
            pkt[1] = 0x42; /* ALPHA_1 */
            pkt += 2;
        } else if (flags & EFT_BURST_F_SUB) {
            pkt[0] = 0x42;
            pkt[1] = 0x42;
            pkt += 2;
        } else {
            pkt[0] = 0x48;
            pkt[1] = 0x42;
            pkt += 2;
        }
        pkt[0] = p->tex->tex0 | 0x5400400002D00;
        pkt[1] = 6; /* TEX0_1 */
        pkt += 2;
        pkt[0] = 0x33003;
        pkt[1] = 0x47; /* TEST_1 */
        pkt += 2;
        pkt[0] = 0x6400000000008001;
        pkt[1] = 0x52525210;
        pkt += 2;
        pkt[0] = 0x56; /* PRIM: textured, blended sprite */
        if (flags & EFT_BURST_F_COLOR) {
            pkt[1] = (u64)p->color | ((s64)alpha << 24) | 0x3F80000000000000;
        } else {
            pkt[1] = ((s64)(life * 80 / lifeMax) << 24) | 0x3F80000000505050;
        }
        pkt += 2;
        pkt[0] = (u64)((u32 *)st[0])[0] | ((u64)((u32 *)st[0])[1] << 32);
        pkt[1] = (s64)scr[0][0] | ((s64)scr[0][1] << 16) | ((s64)scr[0][2] << 32);
        pkt += 2;
        pkt[0] = (u64)((u32 *)st[1])[0] | ((u64)((u32 *)st[1])[1] << 32);
        pkt[1] = (s64)scr[1][0] | ((s64)scr[1][1] << 16) | ((s64)scr[1][2] << 32);
        pkt += 2;
        Dma_EndDirect(pkt);
    }
}

/* Turns the offsets inside the burst model file into pointers (once). */
void EftBurst_RelocateModel(EftBurstModel *model) {
    EftBurstGroup *grp;
    EftBurstTex *tex;
    s32 i;
    s32 n = model->groupCount;

    if (model->relocated != 1) {
        model->relocated = 1;
        model->groups = (EftBurstGroup *)((u8 *)model->groups + (s32)model);
        grp = model->groups;
        if (n > 0) {
            i = n;
            do {
                grp->verts = (s16 *)((u8 *)grp->verts + (s32)model);
                grp->tris = (s16 *)((u8 *)grp->tris + (s32)model);
                grp++;
            } while (--i != 0);
        }
        model->tex = (EftBurstTex *)((u8 *)model->tex + (s32)model);
        tex = model->tex;
        for (i = 0; i < model->texCount; i++) {
            tex->ptr0 = tex->ofs0 + (s32)model;
            tex->ptr4 = tex->ofs4 + (s32)model;
            tex++;
        }
    }
}

/* GS state for the model: TEX1, TEST, ZBUF, ALPHA, COLCLAMP and FRAME (colour only) for both contexts. */
u64 *EftBurst_PutModelEnv(u64 *p) {
    p[0] = 0x1000000000008006;
    p[1] = 0xE;
    p += 2;
    p[0] = 0x60;
    p[1] = 0x14; /* TEX1_1 */
    p += 2;
    p[0] = 0x5000D;
    p[1] = 0x47; /* TEST_1 */
    p += 2;
    p[0] = 0x310000E0;
    p[1] = 0x4E; /* ZBUF_1 */
    p += 2;
    p[0] = 0x44;
    p[1] = 0x42; /* ALPHA_1 */
    p += 2;
    p[0] = 1;
    p[1] = 0x46; /* COLCLAMP */
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0xFF00000000080070 : 0xFF00000000080000;
    p[1] = 0x4C; /* FRAME_1 */
    p += 2;
    p[0] = 0x1000000000008006;
    p[1] = 0xE;
    p += 2;
    p[0] = 0x60;
    p[1] = 0x15; /* TEX1_2 */
    p += 2;
    p[0] = 0x5000D;
    p[1] = 0x48; /* TEST_2 */
    p += 2;
    p[0] = 0x310000E0;
    p[1] = 0x4F; /* ZBUF_2 */
    p += 2;
    p[0] = 0x44;
    p[1] = 0x43; /* ALPHA_2 */
    p += 2;
    p[0] = 1;
    p[1] = 0x46; /* COLCLAMP */
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
    p[1] = 0x4D; /* FRAME_2 */
    p += 2;
    return p;
}

/* GS state after the model: TEX1, TEST (always pass), ZBUF, COLCLAMP, FRAME with every channel writable. */
u64 *EftBurst_PutModelEnvEnd(u64 *p) {
    p[0] = 0x1000000000008005;
    p[1] = 0xE;
    p += 2;
    p[0] = 0;
    p[1] = 0x14; /* TEX1_1 */
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x47; /* TEST_1 */
    p += 2;
    p[0] = 0x310000E0;
    p[1] = 0x4E; /* ZBUF_1 */
    p += 2;
    p[0] = 1;
    p[1] = 0x46; /* COLCLAMP */
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
    p[1] = 0x4C; /* FRAME_1 */
    p += 2;
    return p;
}

/* Fills the alpha plane of the frame buffer with 0x80 (one full-screen sprite through context 2). */
void EftBurst_ClearAlphaPlane(void) {
    u64 *p = Dma_BeginDirect();

    p[0] = 0x1000000000008009;
    p[1] = 0xE;
    p += 2;
    p[0] = 0x44;
    p[1] = 0x43; /* ALPHA_2 */
    p += 2;
    p[0] = 0x60;
    p[1] = 0x15; /* TEX1_2 */
    p += 2;
    p[0] = 0;
    p[1] = 0x4B; /* FBA_2 */
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x48; /* TEST_2 */
    p += 2;
    p[0] = 5;
    p[1] = 9; /* CLAMP_2 */
    p += 2;
    p[0] = 0x1310000E0;
    p[1] = 0x4F; /* ZBUF_2: no depth writes */
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0xFFFFFF00080070 : 0xFFFFFF00080000;
    p[1] = 0x4D; /* FRAME_2: alpha only */
    p += 2;
    p[0] = 0x720000007000;
    p[1] = 0x19; /* XYOFFSET_2 */
    p += 2;
    p[0] = 0x1BF000001FF0000;
    p[1] = 0x41; /* SCISSOR_2 */
    p += 2;
    p[0] = 0x4400000000008001;
    p[1] = 0x5510;
    p += 2;
    p[0] = 0x206;
    p[1] = 0x3F80000000000000;
    p += 2;
    p[0] = 0x72007000;
    p[1] = 0x8E009000;
    p += 2;
    Dma_EndDirect(p);
}

#if 0 /* ATTEMPT: not matching: 597 instructions against 599, 44 differ in an aligned listing, all of them register
   choice and instruction order in three places of the vertex loop; the control flow, the frame (0x240), every stack
   slot, every call and every packet word are the original's. What differs:
   - the colour unpack: the original reads rgba[0] and rgba[2] into $v1 and the two high bytes into $v0, and
     takes the three low parts of the first RGBAQ word from those registers (`move a1,v1`, `move a2,v0 /
     dsll a2,a2,8`, `move a0,v1 / dsll a0,a0,16`), reloading only rgba[3] from the stack; here the registers
     alternate the other way round and rgba[2] is reloaded too (2 instructions short). The moves are sign
     extending loads of rgba[i] that the post-reload CSE turned into copies, so the first scheduling pass of the
     original must have put the reads of rgba[0] and rgba[1] behind the store of rgba[1] and in front of the load
     of the third byte, not into the load-delay gaps where they land here. Reading / storing through a pointer
     that the compiler cannot resolve (`c = rgba;` set outside the vertex loop) gives the original's register
     pattern but costs two instructions elsewhere; a `do { } while (0)` around the four stores does too but breaks
     the loop's induction pointers.
   - the copies of the two constant vectors: the original loads the address of D_002EC8D0 into $v1 and copies it
     to $t0 for the second half (`move t0,v1 ... ld v0,0(v1) / ld v1,8(t0)`), with the halves in $a3/$a2 and
     $v0/$v1; here both addresses go through $t0 and the halves sit in $a2/$a3 and $v1/$v0.
   - the last two argument moves in front of Mtx_ProjectPointStq are swapped (`move t0,s0 / move a3,v0`).
   Ruled out (about 250 variants): every element type / alignment for the vectors, `static const` tables copied by
   struct assignment, memcpy, u64 pairs (u128 gives lq / sq and is the only form that keeps the lui / addiu in the
   block), colour bytes as u8, bit fields, temporaries, one u32, casts, statement permutations, the position of
   `env`, `pos[3]`, `vis`.
   What the rewrite established (and the previous attempt had wrong in shape, not in behaviour):
   - a triangle record is a structure, { s16 vtx[3]; s16 nrm[3][2]; u16 col[3][2]; }: the colour is read as two
     16-bit words split with `& 0xFF` and `>> 8` (lbu / lhu + srl), and the multiple-of-16 split of the member
     offsets gives the original's two extra walking pointers (tri + 2, tri + 6);
   - `((x >> 8) & 0xFF)` with the redundant mask is required: without it the vertex loop is one instruction
     shorter for the loop pass and `tri->vtx[k]` becomes a walking pointer as well (the test is
     `lifetime * threshold * benefit < insn_count`, here 264 against 264);
   - the packing casts are (u64), not (s64): this compiler reassociates `a | b << 8 | c << 16 | d << 24 | q << 32`
     differently for the two ((a | q) | (c << 16 | b << 8)) | d << 24 is the (u64) form);
   - TEX0_2 is `tex0 | (y << 37 | x | 0x400000000)` with the parentheses;
   - the group loop is `for (i = 0; i < n; i++)` (reversed by the compiler into the counter at sp+0x18C), not a
     loop on n itself; the triangles are written out without a loop, two words at a time;
   - the visibility test is the inline function Eft_IsScreenPosVisible used as a value (`vis += ...`);
   - the light vectors are block-scope initialised arrays ({0, 0, 0, 1} is "mostly zero", so it becomes
     memset + one store). */
typedef struct EftBurstTri {
    /* 0x00 */ s16 vtx[3];
    /* 0x06 */ s16 nrm[3][2];   /* two packed angles per corner (env-map normal) */
    /* 0x12 */ u16 col[3][2];   /* r | g << 8, b | a << 8 */
} EftBurstTri; /* 0x1E */

/* Eft_IsScreenPosVisible (0x13F3D8, emitted at the end of the object) as the compiler inlined it here. */
static inline s32 EftBurst_IsVisible(s32 *pos) {
    s32 *p = pos;

    if (p[2] <= 0) {
        return 0;
    }
    if (p[0] > 0x97FF) {
        return 0;
    }
    if (p[0] < 0x6801) {
        return 0;
    }
    if (p[1] > 0x94FF) {
        return 0;
    }
    if (p[1] < 0x6B01) {
        return 0;
    }
    return 1;
}

/* Draws the burst's model in camera space: every vertex is three 16-bit angles / a radius turned into a
   point around (0, 0, -50), every triangle is sent as RGBAQ / ST / XYZ2. Groups with flag 0x800 get a
   second, environment-mapped pass through GS context 2 whose colour is a light that fades with the
   distance to the centre and grows with the frame counter. */
void EftBurst_DrawModel(EftBurstModel *model, s32 unused) {
    EftFVec origin;
    EftFVec d;
    EftFVec ang;
    EftFVec pos;
    EftIVec scr[3];
    EftFVec nrm[3];
    EftFVec st[3];
    EftFVec st2[3];
    u64 rgbaq[3][2];
    EftIVec rgba;
    EftBurstGroup *grp;
    EftBurstTex *texs;
    EftBurstTex *envTex;
    s32 i;
    s16 *verts;
    EftBurstTri *tri;
    EftBurstTex *tex;
    s32 t;
    s32 vis;
    s32 env;
    u64 *pkt;
    s32 flags;
    s32 k;
    s32 n;
    s32 x;
    s32 y;
    f32 fade;
    f32 dist;
    f32 scale;

    grp = model->groups;
    envTex = &gEftBurstRes.texPack->tex[6];
    texs = model->tex;
    memset(origin, 0, sizeof(origin));
    origin[2] = -50.0f;
    n = model->groupCount;
    fade = (f32)gEftBurst->frame * (1.0f / 60.0f);
    dist = gEftBurst->fadeDist;
    if (fade > 1.0f) {
        fade = 1.0f;
    }
    EftBurst_SetTexture(NULL, 0, 0);
    EftBurst_ClearAlphaPlane();
    Dma_EndDirect(EftBurst_PutModelEnv(Dma_BeginDirect()));
    for (i = 0; i < n; i++, grp++) {
        verts = grp->verts;
        tri = (EftBurstTri *)grp->tris;
        tex = &texs[grp->texIdx];
        flags = grp->flags;
        scale = grp->scale;
        x = 0x2D00;
        y = 0x2A00;
        EftBurst_SetTexture(tex, 0x2D00, 0x2A00);
        tex->vramX = x;
        tex->vramY = y;
        x += tex->w;
        y += tex->h;
        EftBurst_SetTexture(envTex, x, y);
        envTex->vramX = x;
        envTex->vramY = y;
        pkt = Dma_BeginDirect();
        pkt[0] = 0x1000000000008004;
        pkt[1] = 0xE;
        pkt += 2;
        pkt[0] = 0;
        pkt[1] = 8; /* CLAMP_1 */
        pkt += 2;
        pkt[0] = 0;
        pkt[1] = 9; /* CLAMP_2 */
        pkt += 2;
        pkt[0] = 0x44;
        pkt[1] = 0x42; /* ALPHA_1 */
        pkt += 2;
        pkt[0] = 0x44;
        pkt[1] = 0x43; /* ALPHA_2 */
        pkt += 2;
        for (t = 0; t < grp->triCount; t++, tri++) {
            vis = 0;
            env = flags & 0x800;
            for (k = 0; k < 3; k++) {
                s16 *v = &verts[tri->vtx[k] * 3];

                IVec4_Set(ang, v[0], v[1], v[2], 0);
                IVec4_ToFloat12(pos, ang);
                IVec4_Set(ang, tri->nrm[k][0], tri->nrm[k][1], 0, 0);
                IVec4_ToFloat12(nrm[k], ang);
                Vec4_Scale((Vec4 *)pos, (Vec4 *)pos, scale);
                Vec4_Sub((Vec4 *)d, (Vec4 *)origin, (Vec4 *)pos);
                pos[3] = 1.0f;
                Mtx_ProjectPointStq(scr[k], st[k], &gBtlCamView->proj, pos, nrm[k]);
                rgba[0] = tri->col[k][0] & 0xFF;
                rgba[1] = (tri->col[k][0] >> 8) & 0xFF;
                rgba[2] = tri->col[k][1] & 0xFF;
                rgba[3] = (tri->col[k][1] >> 8) & 0xFF;
                rgbaq[k][0] = (u64)rgba[0] | ((u64)rgba[1] << 8) | ((u64)rgba[2] << 16) | ((u64)rgba[3] << 24) |
                              ((u64)((u32 *)st[k])[2] << 32);
                if (env) {
                    Vec4_Copy((Vec4 *)st2[k], (Vec4 *)st[k]);
                    st2[k][0] *= 8.0f;
                    st2[k][1] *= 8.0f;
                    if (gEftBurst->frame >= 11) {
                        EftFVec light = { 0.0f, 0.0f, 0.0f, 1.0f };
                        EftFVec lightDir = { 1.0f, -0.2f, -0.8f, 0.0f };
                        EftFVec ambient = { 127.0f, 127.0f, 127.0f, 0.0f };
                        EftFVec colF;
                        f32 len;
                        f32 lit;

                        len = sqrtf(Vec3_Dot((Vec4 *)d, (Vec4 *)d));
                        lit = 255.0f - len / dist * 255.0f;
                        if (lit > 255.0f) {
                            lit = 255.0f;
                        }
                        IVec4_ToFloat(colF, rgba);
                        Vec4_Scale((Vec4 *)light, (Vec4 *)light, fade * 0.6f * lit);
                        Vec4_Add((Vec4 *)ambient, (Vec4 *)light, (Vec4 *)ambient);
                        Vec4_Clamp(ambient, ambient, 0.0f, 255.0f);
                        Vec4_ToInt(rgba, (Vec4 *)ambient);
                        rgbaq[k][1] = (u64)rgba[0] | ((u64)rgba[1] << 8) | ((u64)rgba[2] << 16) |
                                      ((u64)rgba[3] << 24) | ((u64)((u32 *)st[k])[2] << 32);
                        if (len < dist) {
                            Vec4_Scale((Vec4 *)lightDir, (Vec4 *)lightDir, lit);
                            Vec4_Add((Vec4 *)ambient, (Vec4 *)lightDir, (Vec4 *)colF);
                            Vec4_Clamp(ambient, ambient, 0.0f, 255.0f);
                            Vec4_ToInt(rgba, (Vec4 *)ambient);
                            rgbaq[k][0] = (u64)rgba[0] | ((u64)rgba[1] << 8) | ((u64)rgba[2] << 16) |
                                          ((u64)rgba[3] << 24) | ((u64)((u32 *)st[k])[2] << 32);
                        }
                    } else {
                        rgbaq[k][1] = (u64)((u32 *)st[k])[2] << 32;
                    }
                }
                vis += EftBurst_IsVisible(scr[k]);
            }
            if (vis != 0) {
                pkt[0] = 0x1000000000008002;
                pkt[1] = 0xE;
                pkt += 2;
                pkt[0] = tex->tex0 | 0x5400400002D00;
                pkt[1] = 6; /* TEX0_1 */
                pkt += 2;
                pkt[0] = 0x5B; /* PRIM: blended, textured, gouraud triangle */
                pkt[1] = 0;
                pkt += 2;
                pkt[0] = 0x3400000000008003;
                pkt[1] = 0x521; /* RGBAQ, ST, XYZ2 */
                pkt += 2;
                pkt[0] = rgbaq[0][0];
                pkt[1] = (u64)((u32 *)st[0])[0] | ((u64)((u32 *)st[0])[1] << 32);
                pkt += 2;
                pkt[0] = (s64)scr[0][0] | ((s64)scr[0][1] << 16) | ((s64)scr[0][2] << 32);
                pkt[1] = rgbaq[1][0];
                pkt += 2;
                pkt[0] = (u64)((u32 *)st[1])[0] | ((u64)((u32 *)st[1])[1] << 32);
                pkt[1] = (s64)scr[1][0] | ((s64)scr[1][1] << 16) | ((s64)scr[1][2] << 32);
                pkt += 2;
                pkt[0] = rgbaq[2][0];
                pkt[1] = (u64)((u32 *)st[2])[0] | ((u64)((u32 *)st[2])[1] << 32);
                pkt += 2;
                pkt[0] = (s64)scr[2][0] | ((s64)scr[2][1] << 16) | ((s64)scr[2][2] << 32);
                pkt[1] = 0;
                pkt += 2;
                if (env) {
                    pkt[0] = 0x1000000000008002;
                    pkt[1] = 0xE;
                    pkt += 2;
                    pkt[0] = envTex->tex0 | (((u64)(u32)envTex->vramY << 37) | (u64)(u32)envTex->vramX | 0x400000000);
                    pkt[1] = 7; /* TEX0_2 */
                    pkt += 2;
                    pkt[0] = 0x25B; /* the same PRIM through context 2 */
                    pkt[1] = 0;
                    pkt += 2;
                    pkt[0] = 0x3400000000008003;
                    pkt[1] = 0x521;
                    pkt += 2;
                    pkt[0] = rgbaq[0][1];
                    pkt[1] = (u64)((u32 *)st2[0])[0] | ((u64)((u32 *)st2[0])[1] << 32);
                    pkt += 2;
                    pkt[0] = (s64)scr[0][0] | ((s64)scr[0][1] << 16) | ((s64)scr[0][2] << 32);
                    pkt[1] = rgbaq[1][1];
                    pkt += 2;
                    pkt[0] = (u64)((u32 *)st2[1])[0] | ((u64)((u32 *)st2[1])[1] << 32);
                    pkt[1] = (s64)scr[1][0] | ((s64)scr[1][1] << 16) | ((s64)scr[1][2] << 32);
                    pkt += 2;
                    pkt[0] = rgbaq[2][1];
                    pkt[1] = (u64)((u32 *)st2[2])[0] | ((u64)((u32 *)st2[2])[1] << 32);
                    pkt += 2;
                    pkt[0] = (s64)scr[2][0] | ((s64)scr[2][1] << 16) | ((s64)scr[2][2] << 32);
                    pkt[1] = 0;
                    pkt += 2;
                }
            }
        }
        Dma_EndDirect(pkt);
    }
    Dma_EndDirect(EftBurst_PutModelEnvEnd(Dma_BeginDirect()));
}
#endif
INCLUDE_RODATA("asm/nonmatchings/battle/eft_d_b", D_002EC8C0);
INCLUDE_RODATA("asm/nonmatchings/battle/eft_d_b", D_002EC8D0);
LIT4_WORD(D_002FC474, 0x3C888888); /* 1.0f / 60.0f */
LIT4_WORD(D_002FC478, 0x3F199999); /* 0.6f */
INCLUDE_ASM("asm/nonmatchings/battle/eft_d_b", EftBurst_DrawModel);

/* Uploads a texture unless it is already the current one. */
void EftBurst_SetTexture(EftBurstTex *tex, s32 x, s32 y) {
    EftBurstWork *w = gEftBurst;

    if (w->curTex != tex) {
        w->curTex = tex;
        if (tex != NULL) {
            Tex_Upload(tex, x, y);
        }
    }
}

/* Returns the first free particle, NULL when all 350 are in use. */
EftBurstPtcl *EftBurst_AllocPtcl(void) {
    EftBurstPtcl *p = gEftBurst->ptcl;
    s32 i;

    i = EFT_BURST_PTCL_MAX;
    do {
        if (p->life == 0) {
            return p;
        }
        p++;
    } while (--i != 0);
    return NULL;
}

/* Spark: starts on the burst's axis and flies towards the camera in a narrow cone. */
void EftBurst_InitSpark(EftBurstPtcl *p, s32 idx) {
    f32 fi = idx;

    Vec4_Set(&p->pos, 0.0f, 0.0f, -40.0f - fi, 0.0f);
    p->vel.x = (f32)((rand() & 0xFF) - 0x7F) * 0.1f * (1.0f / 128.0f);
    p->vel.y = (f32)((rand() & 0xFF) - 0x7F) * 0.1f * (1.0f / 128.0f);
    p->vel.z = -(f32)(rand() & 0x7F) * (1.0f / 128.0f) - 1.0f;
    p->vel.w = 0.0f;
    Vec3_Normalize(&p->vel, &p->vel);
    Vec4_Scale(&p->vel, &p->vel, 1.0f);
    p->vel.z -= 1.0f;
    p->tex = &gEftBurstRes.texPack->tex[5];
    p->scale = 17.0f - fi * 0.4f + gEftBurst->sway;
    p->flags = EFT_BURST_F_COLOR | EFT_BURST_F_TIMED;
    p->life = p->lifeMax = 0x19 - idx;
    p->color = 0x808080;
    p->alpha = 0x10;
    if (p->life <= 0) {
        p->life = 1;
    }
    p->update = EftBurst_UpdateSpark;
    p->draw = EftBurst_DrawSprite;
}

/* Streak: a rotated quad on a circle around the axis, pulled back towards it; re-created when it ends. */
void EftBurst_InitStreak(EftBurstPtcl *p) {
    f32 t;
    f32 ang;
    f32 r;

    EftBurst_InitDebris(p);
    p->flags = 0xF2;
    p->tex = &gEftBurstRes.texPack->tex[3];
    p->lifeMax = p->life = (rand() & 0xF) + 7;
    p->scale = (f32)(rand() & 0x1F) * (1.0f / 32.0f) * 1.5f + 2.5f;
    Vec4_Set(&p->rotVel, 0.0f, 0.0f, 0.0f, 0.0f);
    Vec4_Set(&p->rot, 0.0f, 1.57f, 0.0f, 0.0f);
    t = (f32)gEftBurst->frame * 0.0016666667f;
    ang = gEftBurstStreakAngle * 3.14f / 180.0f;
    if (t > 1.0f) {
        t = 1.0f;
    }
    r = (f32)(rand() & 0x1F) * (1.0f / 32.0f) * (t + 2.0f) + 10.0f;
    {
        EftVec16 v = { { cosf(ang) * r, sinf(ang) * r, -50.0f, 0.0f } };

        Vec4_Copy(&p->pos, (Vec4 *)&v);
    Vec4_Scale((Vec4 *)&v, (Vec4 *)&v, 0.04f);
    v.v[2] = (f32)(rand() & 0x1F) / 15.0f * -0.6f + t * 0.2f;
        Vec4_Copy(&p->vel, (Vec4 *)&v);
    }
    Vec4_Set(&p->rot, -5.0f * 3.14f / 180.0f, 1.57f, gEftBurstStreakAngle * 3.14f / 180.0f, 0.0f);
    if (rand() & 1) {
        p->color = 0x30;
        p->alpha = 0x50;
        p->flags |= EFT_BURST_F_ADD;
    } else {
        p->color = 0x40;
        p->alpha = 0x50;
    }
    gEftBurstStreakAngle += (f32)(rand() & 0x3F);
    if (gEftBurstStreakAngle > 180.0f) {
        gEftBurstStreakAngle -= 360.0f;
    }
    p->update = EftBurst_UpdateStreak;
    p->draw = EftBurst_DrawQuadRot;
}

/* Flash: a quad on the axis that grows by 20% a frame for ten frames. */
void EftBurst_InitFlash(EftBurstPtcl *p) {
    p->flags = 0x7A;
    p->tex = &gEftBurstRes.texPack->tex[0];
    p->life = 10;
    p->lifeMax = 10;
    p->scale = 10.0f;
    Vec4_Set(&p->pos, 0.0f, 0.0f, -55.0f, 0.0f);
    Vec4_Set(&p->vel, 0.0f, 0.0f, -0.0f, 0.0f);
    p->alpha = 0x80;
    p->color = 0x808080;
    p->update = EftBurst_UpdateFlash;
    p->draw = EftBurst_DrawQuadRot;
}

/* Ring: a quad that swells and shrinks over eight frames. */
void EftBurst_InitRing(EftBurstPtcl *p) {
    p->flags = 0x4A;
    p->tex = &gEftBurstRes.texPack->tex[1];
    p->life = 8;
    p->lifeMax = 8;
    p->scale = 0.1f;
    p->color = 0x708080;
    p->alpha = 0x40;
    p->size = 70.0f;
    Vec4_Set(&p->pos, -5.0f, 0.0f, -51.0f, 0.0f);
    Vec4_Set(&p->vel, 0.0f, 0.0f, -0.0f, 0.0f);
    p->update = EftBurst_UpdateRing;
    p->draw = EftBurst_DrawQuadRot;
}

/* Glow: a quad on the axis that grows slowly for 47..62 frames. */
void EftBurst_InitGlow(EftBurstPtcl *p) {
    Vec4_Set(&p->pos, 0.0f, 0.0f, -51.0f, 0.0f);
    Vec4_Set(&p->vel, 0.0f, 0.0f, -0.0f, 0.0f);
    p->tex = &gEftBurstRes.texPack->tex[4];
    p->scale = 40.0f;
    p->flags = EFT_BURST_F_FADE | EFT_BURST_F_TIMED;
    p->life = (rand() & 0xF) + 0x2F;
    p->lifeMax = p->life;
    p->scale = 10.0f;
    p->draw = EftBurst_DrawQuad;
    p->update = EftBurst_UpdateGlow;
}

/* Debris: a sprite scattered around the axis, drifting away from it. */
void EftBurst_InitDebris(EftBurstPtcl *p) {
    Vec4 c;

    p->flags = 0x4B;
    p->scale = (f32)(rand() & 0x7F) * 2.0f * (1.0f / 128.0f) + 4.0f;
    p->pos.x = (f32)((rand() & 0xFF) - 0x7F) * 8.0f * (1.0f / 128.0f);
    p->pos.y = (f32)((rand() & 0xFF) - 0x7F) * 8.0f * (1.0f / 128.0f);
    p->pos.z = -(f32)(rand() & 0x7F) * 2.0f * (1.0f / 128.0f) - 51.0f;
    p->pos.w = 0.0f;
    p->vel.x = (f32)((rand() & 0xFF) - 0x7F) * 0.1f * (1.0f / 128.0f);
    p->vel.y = (f32)((rand() & 0xFF) - 0x7F) * 0.1f * (1.0f / 128.0f);
    p->vel.z = -(f32)(rand() & 0x7F) * 0.2f * (1.0f / 128.0f);
    p->vel.w = 0.0f;
    memset(&c, 0, sizeof(c));
    c.z = 45.0f;
    Vec4_Sub(&c, &c, &p->pos);
    c.z = 0.0f;
    Vec3_Normalize(&c, &c);
    Vec4_Scale(&c, &c, -0.1f);
    Vec4_Add(&p->vel, &p->vel, &c);
    p->life = p->lifeMax = (rand() & 3) + 0x1F;
    p->tex = &gEftBurstRes.texPack->tex[3];
    p->update = EftBurst_UpdateDebris;
    p->draw = EftBurst_DrawSprite;
    p->color = 0x606060;
    p->alpha = 0;
}

/* Flash: moves, slows down, grows; ends when its life runs out. */
s32 EftBurst_UpdateFlash(EftBurstPtcl *p) {
    Vec4_Add(&p->pos, &p->pos, &p->vel);
    Vec4_Scale(&p->vel, &p->vel, 30.0f / 31.0f);
    p->scale = (p->scale + 0.1f) * 1.2f;
    if (p->flags & EFT_BURST_F_TIMED) {
        if (p->life != 0) {
            p->life--;
        } else {
            return 0;
        }
    }
    return 1;
}

/* Streak: spirals in towards the axis while its angles advance; starts over when its life ends. */
s32 EftBurst_UpdateStreak(EftBurstPtcl *p) {
    Vec4 center;
    Vec4 d;
    s32 i;
    Vec4 *pos = &p->pos;
    Vec4 *vel = &p->vel;
    Vec4 *rot = &p->rot;
    Vec4 *rotVel = &p->rotVel;

    memset(&center, 0, sizeof(center));
    Vec4_Add(pos, pos, vel);
    Vec4_Scale(vel, vel, 30.0f / 31.0f);
    Vec4_Sub(&d, &center, pos);
    Vec3_Normalize(&d, &d);
    Vec4_Scale(&d, &d, 0.98f / 15.0f);
    Vec4_Add(&p->vel, &p->vel, &d);
    p->vel.w = 0.0f;
    Vec4_Add(rot, rot, rotVel);
    rotVel->x += -0.5f * 3.14f / 180.0f;
    for (i = 0; i < 3; i++) {
        if ((&rot->x)[i] > 3.14f) {
            (&rot->x)[i] = -3.14f;
        }
        if ((&rot->x)[i] < -3.14f) {
            (&rot->x)[i] = 3.14f;
        }
    }
    if (p->flags & EFT_BURST_F_TIMED) {
        if (p->life != 0) {
            if (--p->life == 0) {
                EftBurst_InitStreak(p);
            }
        } else {
            return 0;
        }
    }
    return 1;
}

/* Spark: moves, slows down, shrinks, fades over its last ten frames. */
s32 EftBurst_UpdateSpark(EftBurstPtcl *p) {
    Vec4_Add(&p->pos, &p->pos, &p->vel);
    Vec4_Scale(&p->vel, &p->vel, 0.98f);
    p->scale -= 0.3f;
    if (p->life < 10) {
        p->alpha = p->life;
    }
    if (p->flags & EFT_BURST_F_TIMED) {
        if (p->life != 0) {
            p->life--;
        } else {
            return 0;
        }
    }
    return 1;
}

/* Moves and slows down; ends when its life runs out. Not referenced. */
s32 EftBurst_UpdateMove(EftBurstPtcl *p) {
    Vec4_Add(&p->pos, &p->pos, &p->vel);
    Vec4_Scale(&p->vel, &p->vel, 30.0f / 31.0f);
    if (p->flags & EFT_BURST_F_TIMED) {
        if (p->life != 0) {
            p->life--;
        } else {
            return 0;
        }
    }
    return 1;
}

/* Ring: scale rises to size / 2 + 10 at half life and falls back. */
s32 EftBurst_UpdateRing(EftBurstPtcl *p) {
    Vec4_Add(&p->pos, &p->pos, &p->vel);
    Vec4_Scale(&p->vel, &p->vel, 30.0f / 31.0f);
    p->scale = (f32)(p->lifeMax - p->life) / (f32)p->lifeMax;
    if (p->scale > 0.5f) {
        p->scale = 1.0f - p->scale;
    }
    p->scale = p->scale * p->size + 10.0f;
    if (p->flags & EFT_BURST_F_TIMED) {
        if (p->life != 0) {
            p->life--;
        } else {
            return 0;
        }
    }
    return 1;
}

/* Glow: moves, slows down, grows by 0.35 a frame. */
s32 EftBurst_UpdateGlow(EftBurstPtcl *p) {
    Vec4_Add(&p->pos, &p->pos, &p->vel);
    Vec4_Scale(&p->vel, &p->vel, 30.0f / 31.0f);
    p->scale += 0.35f;
    if (p->flags & EFT_BURST_F_TIMED) {
        if (p->life != 0) {
            p->life--;
        } else {
            return 0;
        }
    }
    return 1;
}

/* Debris: moves, slows down; alpha is a triangle wave over its life (peak 24). */
s32 EftBurst_UpdateDebris(EftBurstPtcl *p) {
    Vec4_Add(&p->pos, &p->pos, &p->vel);
    Vec4_Scale(&p->vel, &p->vel, 0.98f);
    p->alpha = p->life * 48 / p->lifeMax;
    if (p->alpha >= 25) {
        p->alpha = 48 - p->alpha;
    }
    if (p->flags & EFT_BURST_F_TIMED) {
        if (p->life != 0) {
            p->life--;
        } else {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly the head of eft_e.c, 0x13EA00..0x13F430), with its own header and view types. Names the first part already declared with
 * other types are reached through cast macros (the generated code is the same).
 * ------------------------------------------------------------------------------------------------------------ */
/* eft_e.h declares the transition's globals and the view with its own types: hide those declarations and reach
 * the globals through casts. */
#define EftView EftViewE
#define gEftBurstRes gEftBurstRes_eDecl
#define gEftBurst gEftBurst_eDecl
#include "battle/eft_e.h"
#undef gEftBurstRes
#undef gEftBurst
#define gEftBurstRes (*(EftTransRes *)&gEftBurstRes)
#ifdef PORT /* assigned to: a cast is not an lvalue for a modern compiler */
#define gEftBurst (*(EftTransWork **)&gEftBurst)
#else
#define gEftBurst ((EftTransWork *)gEftBurst)
#endif
#include "sys/gfx_ot.h"

/*
 * Effect code 0x13EA00..0x13F430 (the declarations below are the common preamble of the former eft_e.c, whose
 * rest, 0x13F430..0x142CA0, is still eft_e.c). See include/battle/eft_e.h for the layouts.
 *
 *   0x13EA00..0x13F3D8  EftBurst_*: the stage-change transition (task side; particles and model are above).
 *   0x13F3D8..0x13F430  two screen-position tests, no callers.
 *
 * Other modules are declared locally with this file's own view types (the integrator was linking while this was
 * written). Names of the task system (BtlTask*), EftCam_*, EftStage_*, EftHit_*, EftUtil_*, EftRec_*, BtlStage_*
 * and the EftWater* pool functions are the ones recorded in config/symbols by their owners.
 *
 * Random numbers: everything here draws from the C library rand() (EFT_RANDF) except EftSteam_Emit, which uses
 * the VU0 generator through Rand_FloatRange. No draw reaches a fighter, a hit record or a battle object.
 */

extern void *Heap_Alloc(s32 size, u32 align, s32 fromTail, s32 heap);
extern void Heap_Free(void *ptr);
extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 sinf(f32 x);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern void *BtlTask_CreateChildList(void *task, s32 capacity, s32 workSize);
extern void *BtlTaskList_AddTail(void *list, BtlTaskClass *cls, void *arg);
extern void BtlTask_SetDead(void *task);
extern void EftCam_Start(s32 *arg);
extern void EftCam_SetHold(s32 arg);
extern void EftCam_Stop(void);
extern void Gfx_AddDefaultEnv(void);
#define EftBurst_DrawModel ((void (*)(void *model, s32 arg))EftBurst_DrawModel)
#define EftBurst_SetTexture ((void (*)(void *tex, s32 x, s32 y))EftBurst_SetTexture)
#define EftBurst_RelocateModel ((void (*)(void *model))EftBurst_RelocateModel)
extern void Res_RelocateOffsets(void *dst, void *base, void *table);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);

extern s32 BtlStage_GetFxResC(void);
extern s32 BtlStage_GetFxResA(void);
extern EftStageMarker *BtlStage_GetFxResC2(void);
extern void EftTexSet_Load8(EftSteamTex *tex, s32 *data);
extern u64 EftVram_AddTex(u64 *tex0, s32 a1, s32 a2);
extern s32 EftStage_IsDrawOn(void);
extern void Vu0Cur_Push(void);
extern void Vu0Cur_LoadMtx(Mtx44 *mtx);
extern void Vu0Cur_Pop(void);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern f32 Rand_FloatRange(f32 a, f32 b);

/* The stage state (gp 0x2FEBE0); only the flag word. */
typedef struct EftStageState {
    s32 unk0[2];
    s32 flags; /* bit 0: not drawable (BtlStage_IsReady tests it too) */
} EftStageState;
extern EftStageState *gBtlStage;

/* The view being drawn (btl_cam.h); only the matrices and position used here. */
typedef struct EftView {
    /* 0x000 */ Mtx44 world2view;
    /* 0x040 */ Mtx44 world2view2;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 world2screen;
    /* 0x180 */ u8 unk180[0xA0];
    /* 0x220 */ Vec4 pos;
} EftView;
#define gBtlCamView ((EftView *)gBtlCamView)

extern f32 sqrtf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern s32 BtlStage_GetWaterLevel(f32 *y);
extern s32 BtlStage_GetId(void);
extern s32 Battle_IsSplitScreen(void);
extern s32 BtlStage_IsReady(void);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern f32 BtlCharApi_GetGroundY(s32 objId);
extern void BtlCharApi_GetPos(s32 objId, Vec4 *out);
extern void BtlCharApi_GetDir(s32 objId, Vec4 *out);
extern f32 BtlCharApi_GetSpeed(s32 objId);
extern s32 BtlCharApi_IsInTechnique(s32 objId);
extern void BtlCharApi_PlaySoundAt(Vec4 *pos, s32 kind, s32 id, f32 near, f32 far);
extern f32 EftHit_GetRadiusA(EftWaterBlast *rec);
extern void EftUtil_ClipSegToWater(EftEVec *out, Vec4 *a, Vec4 *b);
extern s32 EftRec_GetDefClass(EftWaterBlast *rec);
extern void EftTexSet_Load32(u8 *tex, s32 *data);
extern void EftWater_UpdateTextures(s32 a0, s32 a1);
extern void EftWaterSplash_UpdateList(EftWaterSplash **head, EftWaterSplash **tail);
extern void EftWaterSplash_DrawList(void *head);
extern void EftWaterTrail_Spawn(void **head, void **tail, s32 objId, EftWaterBlast *rec);
extern void EftWaterTrail_UpdateList(void **head, void **tail);
extern void EftWaterTrail_DrawList(void *head);
extern void EftWaterDrop_UpdateList(void **head, void **tail);
extern void EftWaterDrop_DrawList(void *head, EftEVec origin);
extern void EftWaterRing_UpdateList(void **head, void **tail);
extern void EftWaterRing_DrawList(void *head);
extern void EftWaterSpray_UpdateList(void **head, void **tail);
extern void EftWaterSpray_DrawList(void *head, EftEVec origin);
extern void EftWaterMist_UpdateList(void **head, void **tail);
extern void EftWaterMist_DrawList(void *head);

/* Billboard: yaw / pitch / speed of its flight, rot, size and its growth, gravity, life, start delay. */
extern void EftWaterDrop_Spawn(void **head, void **tail, EftEVec pos, f32 yaw, f32 pitch, f32 speed, f32 rot, f32 rotVel,
                          f32 size, f32 sizeVel, f32 sizeDamp, f32 gravity, f32 life, f32 delay, u8 r, u8 g, u8 b,
                          u8 tex, u8 layer, s32 flags);
/* Ring lying on the surface. */
extern void EftWaterRing_Spawn(void **head, void **tail, EftEVec pos, f32 rot, f32 size, f32 sizeVel, f32 sizeDamp,
                          f32 alphaMax, f32 time, f32 delay, u8 r, u8 g, u8 b, u8 tex, u8 layer, s32 flags);
/* Streak thrown out along a direction. */
extern void EftWaterSpray_Spawn(void **head, void **tail, EftEVec pos, f32 dist, f32 height, f32 yaw, f32 pitch, f32 roll,
                          f32 tilt, f32 tiltEnd, f32 speed, f32 size, f32 sizeVel, f32 sizeDamp, f32 gravity,
                          f32 nearScale, f32 widthScale, f32 life, f32 delay, u8 r, u8 g, u8 b, u8 tex, u8 layer,
                          s32 flags);
/* Streak that orbits. */
extern void EftWaterMist_Spawn(void **head, void **tail, EftEVec pos, f32 radius, f32 height, f32 yaw, f32 yawVel,
                          f32 yawDamp, f32 pitch, f32 roll, f32 tilt, f32 tiltEnd, f32 speed, f32 size, f32 sizeVel,
                          f32 sizeDamp, f32 gravity, f32 nearScale, f32 widthScale, f32 life, f32 delay, u8 r, u8 g,
                          u8 b, u8 tex, u8 layer);

#define EFT_RANDF() ((f32)rand() / 2147483647.0f)
#define EFT_UNIT(objId) (BtlCharApi_GetHeight(objId) * 0.05f)
#define EFT_SCALE(objId, scale) (BtlCharApi_GetHeight(objId) * 0.05f * (scale))
#define EFT_PAUSED() (Battle_GetWork()->flags & 0x100)

/* The battle work (battle.h); only the flag word. */
typedef struct EftBattleWork {
    u8 unk0[0x19F0];
    u64 flags; /* 0x100: pause */
} EftBattleWork;
extern EftBattleWork *Battle_GetWork(void);

extern EftEVec D_002C3630; /* (0, 0, 0, 1) */

#define EftBurst_AllocPtcl ((EftTransPart *(*)(void))EftBurst_AllocPtcl)
#define EftBurst_InitSpark ((void (*)(EftTransPart *part, s32 idx))EftBurst_InitSpark)
#define EftBurst_InitStreak ((void (*)(EftTransPart *part))EftBurst_InitStreak)
#define EftBurst_InitFlash ((void (*)(EftTransPart *part))EftBurst_InitFlash)
#define EftBurst_InitRing ((void (*)(EftTransPart *part))EftBurst_InitRing)
#define EftBurst_InitGlow ((void (*)(EftTransPart *part))EftBurst_InitGlow)
#define EftBurst_InitDebris ((void (*)(EftTransPart *part))EftBurst_InitDebris)
extern s32 Snd_PlaySeEx(u32 mask, s32 id, s32 volume, s32 pan, s32 pitch);

extern f32 EftStage_GetTintScale(void);
extern void EftMath_MtxFromDir(Mtx44 *out, Vec4 *dir, f32 angle);
extern void Mtx_MulVec4(Vec4 *out, Mtx44 *mtx, Vec4 *v);

/* A projected point as Vu0Cur_ProjectPoints writes it: GS x, y in 12.4 fixed point, z, and a fourth word. */
typedef struct EftSteamScr {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;
    /* 0x0C */ s32 unkC;
} EftSteamScr; /* size 0x10 */
extern void Vu0Cur_ProjectPoints(EftSteamScr *out, Vec4 *in, s32 count);

/* GS XYZF2 register value. */
typedef struct EftSteamXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftSteamXyzf;

/* The packet of one particle quad: REGLIST of PRIM, TEX0_1, RGBAQ and four (ST, XYZF2). */
typedef struct EftSteamPkt {
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
        /* 0x00 */ f32 s;
        /* 0x04 */ f32 t;
        /* 0x08 */ EftSteamXyzf xyz;
    } v[4];
    /* 0x78 */ u64 pad;
} EftSteamPkt; /* size 0x80 */


typedef struct EftCommonRes {
    u8 unk0[0x34];
    s32 *transition;
} EftCommonRes;
extern EftCommonRes *gCommonRes;

/* Layer 4 init: creates the list that will hold the one transition task. */
void EftBurstLayer_Init(void *task) {
    gEftBurstRes.list = BtlTask_CreateChildList(task, 1, 4);
    gEftBurstRes.startReq = 0;
    gEftBurstRes.endReq = 0;
}

/* Layer 4 term: nothing. */
void EftBurstLayer_Term(void) {
}

/* Layer 4 update: creates the transition task once EftBurst_Start has asked for it. */
void EftBurstLayer_Update(void) {
    s32 arg;

    if (gEftBurstRes.startReq != 0) {
        arg = 0;
        BtlTaskList_AddTail(gEftBurstRes.list, &gEftBurstClass, &arg);
        gEftBurstRes.startReq = 0;
    }
}

/* Transition task init: allocates the work, clears the particles and starts the camera animation. */
void EftBurst_Init(void) {
    s32 i;
    s32 off;

    gEftBurst = Heap_Alloc(sizeof(EftTransWork), 0x20, 0, 2);
    memset(gEftBurst, 0, sizeof(EftTransWork));
    gEftBurstRes.endReq = 0;
    for (i = 0, off = 0; i < EFT_TRANS_PART_COUNT; i++, off += sizeof(EftTransPart)) {
        memset((u8 *)gEftBurst + off + 0x240, 0, sizeof(EftTransPart));
    }
    gEftBurst->fadeDist = 8.0f;
    gEftBurst->unkC = 20.0f;
    gEftBurst->frame = 0;
    gEftBurst->seWait = 60;
    gEftBurst->camArg[0] = (s32)gEftBurstRes.file1;
    gEftBurst->camArg[1] = -1;
    gEftBurst->camArg[2] = -1;
    EftCam_Start(gEftBurst->camArg);
    EftCam_SetHold(1);
}

/* Transition task term: frees the work. */
void EftBurst_Term(void) {
    if (gEftBurst != NULL) {
        Heap_Free(gEftBurst);
        gEftBurst = NULL;
    }
}

/* Transition task update. On the frame EftBurst_End was called: ends the task and stops the camera animation.
   Otherwise, unless the battle is paused: spawns particles on a fixed schedule (frame 0 two rings, 7 and 30 a
   flash, 10 a burst of 240 streaks, 32 sparks and 16 debris, from 9 a glow every 5..8 frames, from 11 one spark
   and one debris per frame), counts the frame to 151, widens the fade distance, advances the sway (rand()), runs
   every particle, and plays the sounds (4 / 0x4A at frames 7 and 30 and then every 60..139 frames (rand()),
   4 / 0x47 with a 20 % chance (rand()) every 60th frame).
   Matches since the integration, for two reasons that were outside the former eft_e.c:
   - it needs EftBurst_InitFlash, EftBurst_InitRing and EftBurst_InitDebris defined earlier in the same file
     (four `beqz` come out as `beqzl` otherwise), hence the merge into this file;
   - the prelude used to leave a hazard nop after `mfhi / mtc1` (five places: the `% 360` terms); __gp_forget
     now uses `.set mips64` instead of `.set mips4`, which removes them and changes no other linked object. */
void EftBurst_Update(void *task) {
    s32 i;
    s32 j;

    if (gEftBurstRes.endReq != 0) {
        BtlTask_SetDead(task);
        EftCam_Stop();
        return;
    }
    if (Battle_GetWork()->flags & 0x100) {
        return;
    }
    if (gEftBurst->frame == 0) {
        {
            EftTransPart *part = EftBurst_AllocPtcl();

            if (part != NULL) {
                EftBurst_InitRing(part);
                part->flags |= 0x10;
                part->size = 70.0f;
                part->alpha = 0x20;
            }
        }
        {
            EftTransPart *part = EftBurst_AllocPtcl();

            if (part != NULL) {
                EftBurst_InitRing(part);
                part->flags |= 0x10;
                part->size = 50.0f;
                part->alpha = 0x40;
            }
        }
    }
    if (gEftBurst->frame == 7) {
        {
            EftTransPart *part = EftBurst_AllocPtcl();

            if (part != NULL) {
                EftBurst_InitFlash(part);
                part->alpha = 0x45;
            }
        }
    }
    if (gEftBurst->frame == 10) {
        for (i = 0; i < 240; i++) {
            {
                EftTransPart *part = EftBurst_AllocPtcl();

                if (part != NULL) {
                    EftBurst_InitStreak(part);
                }
            }
        }
    }
    if (gEftBurst->frame == 30) {
        {
            EftTransPart *part = EftBurst_AllocPtcl();

            if (part != NULL) {
                EftBurst_InitFlash(part);
                part->alpha = 0x20;
            }
        }
    }
    if (gEftBurst->frame >= 9) {
        if (gEftBurst->spawnWait != 0) {
            gEftBurst->spawnWait--;
        } else {
            {
                EftTransPart *part = EftBurst_AllocPtcl();

                if (part != NULL) {
                    EftBurst_InitGlow(part);
                }
            }
            gEftBurst->spawnWait = (rand() & 3) + 5;
        }
    }
    if (gEftBurst->frame == 10) {
        for (i = 0; i < 32; i++) {
            {
                EftTransPart *part = EftBurst_AllocPtcl();

                if (part != NULL) {
                    EftBurst_InitSpark(part, i);
                }
            }
        }
        for (i = 0; i < 16; i++) {
            {
                EftTransPart *part = EftBurst_AllocPtcl();

                if (part != NULL) {
                    EftBurst_InitDebris(part);
                }
            }
        }
    }
    if (gEftBurst->frame >= 11) {
        {
            EftTransPart *part = EftBurst_AllocPtcl();

            if (part != NULL) {
                EftBurst_InitSpark(part, 0);
            }
        }
        {
            EftTransPart *part = EftBurst_AllocPtcl();

            if (part != NULL) {
                EftBurst_InitDebris(part);
            }
        }
    }
    if (gEftBurst->frame < 151) {
        gEftBurst->frame++;
    }
    if (gEftBurst->fadeDist < 50.0f) {
        gEftBurst->fadeDist += 0.4f;
    }
    gEftBurst->unkC *= 0.995f;
    gEftBurst->phase += (rand() & 7) + 3;
    if (gEftBurst->phase > 360) {
        gEftBurst->phase -= 360;
    }
    gEftBurst->sway = (sinf(gEftBurst->phase * 3.14159265f / 180.0f) +
                       sinf((gEftBurst->phase * 2 % 360) * 3.14159265f / 180.0f) +
                       sinf((gEftBurst->phase * 3 % 360) * 3.14159265f / 180.0f) +
                       sinf((gEftBurst->phase * 4 % 360) * 3.14159265f / 180.0f) +
                       sinf((gEftBurst->phase * 5 % 360) * 3.14159265f / 180.0f) +
                       sinf((gEftBurst->phase * 6 % 360) * 3.14159265f / 180.0f)) /
                      6.0f;
    {
        EftTransPart *part = gEftBurst->part;

        for (j = 0; j < EFT_TRANS_PART_COUNT; j++, part++) {
            part->visible = 0;
            if (part->update != NULL) {
                if (part->update(part)) {
                    part->visible = 1;
                }
            }
        }
    }
    if (gEftBurst->frame == 7) {
        Snd_PlaySeEx(4, 0x4A, 0x7F, 0x40, -0x352);
    }
    if (gEftBurst->frame == 30) {
        Snd_PlaySeEx(4, 0x4A, 0x7F, 0x40, -0x352);
    }
    if (gEftBurst->frame >= 31) {
        if (gEftBurst->frame % 60 == 0) {
            if (rand() % 100 < 20) {
                Snd_PlaySeEx(4, 0x47, 0x7F, 0x40, -500);
            }
        }
        gEftBurst->seWait--;
        if (gEftBurst->seWait <= 0) {
            Snd_PlaySeEx(4, 0x4A, 0x7F, 0x40, -0x352);
            gEftBurst->seWait = rand() % 80 + 60;
        }
    }
}

/* Transition task draw: the model, then every particle that was visible in the last update. */
void EftBurst_Draw(void) {
    s32 i;
    EftTransPart *part;

    if (gEftBurstRes.endReq == 0) {
        Gfx_AddDefaultEnv();
        EftBurst_DrawModel(gEftBurstRes.file2, 0);
        EftBurst_SetTexture(NULL, 0, 0);
        part = gEftBurst->part;
        for (i = 0; i < EFT_TRANS_PART_COUNT; i++, part++) {
            if (part->visible != 0) {
                part->draw(part);
            }
        }
    }
}

/* Transition task post-update: nothing. */
void EftBurst_PostUpdate(void) {
}

/* Transition task reset (battle restart): stops the camera animation and ends the task. */
void EftBurst_Reset(void *task) {
    EftCam_Stop();
    BtlTask_SetDead(task);
}

/* Called by the stage swap once the transition file is loaded: resolves its entries and asks for the task. */
void EftBurst_Start(void) {
    s32 *pack = gCommonRes->transition;

    gEftBurstRes.file2 = BtlScene_GetPackEntry(pack, 2);
    EftBurst_RelocateModel(gEftBurstRes.file2);
    gEftBurstRes.file3 = BtlScene_GetPackEntry(pack, 3);
    Res_RelocateOffsets(&gEftBurstRes.file3, gEftBurstRes.file3, gEftBurstRes.file3);
    gEftBurstRes.file1 = BtlScene_GetPackEntry(pack, 1);
    gEftBurstRes.startReq = 1;
    gEftBurstRes.endReq = 0;
}

/* 1 while the transition is in its first 150 frames (the stage swap waits for 0 before fading out). */
s32 EftBurst_IsBusy(void) {
    if (gEftBurst != NULL && gEftBurst->frame < 150) {
        return 1;
    }
    return 0;
}

/* Asks the transition task to end (the stage swap calls it once the fade has covered the screen). */
void EftBurst_End(void) {
    gEftBurstRes.endReq = 1;
}

/* 1 when a projected point (GS 12.4 coordinates) is in front of the camera and inside the guard area. */
s32 Eft_IsScreenPosVisible(s32 *pos) {
    s32 *p = pos;

    if (p[2] <= 0) {
        return 0;
    }
    if (p[0] > 0x97FF) {
        return 0;
    }
    if (p[0] < 0x6801) {
        return 0;
    }
    if (p[1] > 0x94FF) {
        return 0;
    }
    if (p[1] < 0x6B01) {
        return 0;
    }
    return 1;
}

/* 1 when a projected point is in front of the camera. */
s32 Eft_IsScreenPosInFront(s32 *pos) {
    return pos[2] > 0;
}
