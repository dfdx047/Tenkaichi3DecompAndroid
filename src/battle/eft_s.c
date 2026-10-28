#include "battle/btl_effect_owner.h"
#include "common.h"
#include "battle/eft_s.h"
#include "sys/gfx_ot.h"

/*
 * Effect tasks, 0x178AB0..0x17EE68. Written as two files and merged at integration (EftChain_SetRes only matches
 * with EftChain_SetTex, 0x1793A8, defined above it): the first part, 0x178AB0..0x17CB40, see
 * include/battle/eft_s.h; the second part (formerly eft_t.c, 0x17CB40..0x17EE68: the chain module's tail and the
 * ray burst), see include/battle/eft_t.h and the comment at its start.
 */

#define RAND_MAX_F 2147483647.0f
#define RANDF() ((f32)rand() / RAND_MAX_F)
#define V(p) ((Vec4 *)(p))

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftSView {
    /* 0x000 */ u8 unk0[0x220];
    /* 0x220 */ EftVec pos;    /* camera position */
} EftSView;

extern EftSView *gBtlCamView;
/* gOtCur read as a volatile object (same symbol). The original never moves the load `pkt = gOtCur` into the delay
   slot of the branch in front of it, while the store `gOtCur = pkt + 1` does go into one: a volatile read
   reproduces exactly that (see EftChain_DrawStrand). */
extern u32 *volatile gOtCurRead __asm__("gOtCur");
extern EftArcPool *gEftChain;
extern void *D_002C3E18[6]; /* task class of the prop ki blast */

extern s32 rand(void);
extern void *memset(void *dst, s32 c, u32 n);
extern f32 sqrtf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);
extern f32 EftMath_WrapAngle(f32 angle);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Copy(Vec4 *dst, Vec4 *src);            /* copies x, y, z */
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec4_ToInt(s32 *dst, Vec4 *src);              /* float vector to integer vector */
extern void Vec4_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi); /* clamps each component */
extern void Vec3_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi); /* clamps x, y, z */
extern void Vec3_Add4(Vec4 *dst, Vec4 *a, Vec4 *b, Vec4 *c, Vec4 *d); /* a + b + c + d */
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern void Mtx_SetTrans(Mtx44 *m, Vec4 *pos);             /* sets the translation row */
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);  /* matrix product */
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Z */
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about X */
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Y */
extern s32 Vu0Cur_ProjectPoints(void *out, Vec4 *pos, s32 count);  /* project count points; 0 when clipped */
extern void ClipVtx_Set(void *vtx, Vec4 *pos, Vec4 *uv, Vec4 *col);
extern void EftGfx_DrawPolyScaledZ(void *verts, s32 layer, s32 a2, s32 a3, s32 front, s32 flip, u64 tex0,
                                   f32 zScale);
/* The same callee as in eft_m.c; this argument order (registers are assigned per class, so it is the same call)
   is the one that reproduces the order the arguments are set up in. */
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void EftSpr_DrawRot(u8 r, u8 g, u8 b, u8 a, f32 x, f32 y, f32 z, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot, s32 t0, s32 t1, s32 w, s32 h, s32 s0, u32 size, s32 s2, s32 s3, void *tex);
#define EftSpr_DrawRot(x, y, z, r, g, b, a, t0, t1, w, h, u0, v0, u1, v1, rot, s0, size, s2, s3, tex) EftSpr_DrawRot(r, g, b, a, x, y, z, u0, v0, u1, v1, rot, t0, t1, w, h, s0, size, s2, s3, tex)
#else
extern void EftSpr_DrawRot(f32 x, f32 y, f32 z, u8 r, u8 g, u8 b, u8 a, s32 t0, s32 t1, s32 w, s32 h, f32 u0, f32 v0,
                          f32 u1, f32 v1, f32 rot, s32 s0, u32 size, s32 s2, s32 s3, void *tex);
#endif
extern u64 EftVram_AddImage(void *tex, s32 a, s32 b);
extern u64 EftVram_AddClut(void *tex);
extern void EftTexSet_Load4(void *tex, s32 *entry);
extern void BtlTask_SetDead(EftTask *task);                   /* kills the task */
extern void EftObj_SetMtx(s32 obj, Mtx44 *m);               /* sets a model object's matrix */
extern void EftObj_SetVisible(s32 obj, s32 show);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 *BtlScene_GetPackEntry(s32 *base, s32 idx);
extern s32 *BtlScene_GetCharPackEntry(s32 side, s32 idx);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);
extern void *BtlTask_CreateChildList(EftTask *task, s32 count, s32 workSize);
extern void *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void EftChar_SetList(s32 side, s32 slot, void *list);  /* registers a character's child list */
extern void *EftChar_GetList(s32 side, s32 slot);             /* ... and returns it */
extern void EftKiObj_Turn(EftTask *task, EftKiProp *w);     /* deflect: new owner, new velocity */
extern void EftKiObj_Update(EftTask *task);                   /* update callback of the prop ki blast */

/* ---- prop ki blast (continued from the previous file) ----------------------------------------------------- */

/* Reset callback of the prop ki blast: kills the task. */
void EftKiObj_Reset(EftTask *task) {
    BtlTask_SetDead(task);
}

/* Post-update callback. */
void EftKiObj_PostUpdateCb(EftTask *task) {
    EftKiObj_PostUpdate(task);
}

/* Draw callback. */
void EftKiObj_DrawCb(EftTask *task) {
    EftKiObj_Draw(task);
}

/* Manager init: loads the character's blast pack (model and fragment texture) and creates the list of ten
   blast tasks, registered as the character's list 1. */
void EftKiObjMgr_Init(EftTask *task, s32 *arg) {
    EftKiPropMgr *mgr = task->work;
    EftKiPropRes *res;
    void *list;

    mgr->res = NULL;
    mgr->res = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftKiPropRes));
    memset(mgr->res, 0, sizeof(EftKiPropRes));
    res = mgr->res;
    res->pack = BtlScene_GetCharPackEntry(arg[0], 3);
    res->model = BtlScene_GetPackEntry(res->pack, 1);
    EftTexSet_Load4(res->tex, BtlScene_GetPackEntry(res->pack, 2));
    list = BtlTask_CreateChildList(task, 10, sizeof(EftKiProp));
    EftChar_SetList(arg[0], arg[1], list);
}

/* Manager term: frees the resources. */
void EftKiObjMgr_Term(EftTask *task) {
    EftKiPropMgr *mgr = task->work;

    if (mgr->res != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), mgr->res);
        mgr->res = NULL;
    }
}

/* Manager update: the fragment texture has to be rebuilt this frame. */
void EftKiObjMgr_Update(EftTask *task) {
    EftKiPropMgr *mgr = task->work;

    mgr->res->texReady = 0;
}

/* Creates a prop ki blast in the owner's list. Called by the fighter's ki blast launcher for blast type 2. */
void EftKiObj_Create(EftKiPropArg arg) {
    void *list = EftChar_GetList(arg.owner, 1);

    if (list != NULL) {
        BtlTaskList_AddTail(list, D_002C3E18, &arg);
    }
}

/* Stores a 0x40-byte block in a live prop ki blast. No caller in the main executable. */
void EftKiObj_SetBlock(EftTask *task, EftKiPropBlock *src) {
    if (task != NULL && !(task->flags & 1) && task->cls[0] == EftKiObj_Update) {
        ((EftKiProp *)task->work)->block = *src;
    }
}

/* One frame of a prop ki blast: moves it under gravity, turns it, steps the fragments. */
s32 EftKiObj_Step(EftTask *task) {
    EftKiProp *w = task->work;
    s32 period;

    w->frame++;
    if (w->modelObj == -1) {
        w->flags |= EFT_KIPROP_DEAD;
    }
    if (!(w->flags & EFT_KIPROP_BROKEN)) {
        if (w->flags & EFT_KIPROP_MOVED) {
            Vec4_Copy(V(&w->prevPos), V(&w->pos));
        }
        w->flags |= EFT_KIPROP_MOVED;
        Vec3_Add(V(&w->pos), V(&w->pos), V(&w->vel));
        w->vel.y += w->gravity;
    }
    period = 360;
    w->spin = EftMath_WrapAngle((f32)(w->frame * (s32)w->spinRate % period) * 3.14159265f / 180.0f);
    if (w->fragTimer <= 0) {
        w->flags |= EFT_KIPROP_DEAD;
    }
    EftKiObj_StepFrags(task);
    return 0;
}

/* After the hit pass: shows or hides the model; a hit breaks the blast at the hit position; a deflection hands
   it to the other fighter. */
void EftKiObj_PostUpdate(EftTask *task) {
    EftKiProp *w = task->work;

    if (w->flags & EFT_KIPROP_SHOWN) {
        if (!BtlScene_IsEffectHidden(w->arg.owner, 0)) {
            EftObj_SetVisible(w->modelObj, 1);
        } else {
            EftObj_SetVisible(w->modelObj, 0);
        }
    } else {
        EftObj_SetVisible(w->modelObj, 0);
    }
    if (w->modelObj != -1) {
        if (task->result & 0x3F) {
            w->life = 0;
            if (!(w->flags & EFT_KIPROP_BROKEN)) {
                Vec3_Copy(V(&w->pos), V(&task->hitPos));
                EftKiObj_Break(task, &w->pos);
                w->flags |= EFT_KIPROP_BROKEN;
                w->flags &= ~EFT_KIPROP_SHOWN;
                w->flags &= ~EFT_KIPROP_HITTING;
            }
        } else if (task->result & 0xC0) {
            if (!(w->flags & EFT_KIPROP_DEFLECTED)) {
                EftKiObj_Turn(task, w);
                w->pos.x = task->hitPos.x;
                w->pos.y = task->hitPos.y;
                w->pos.z = task->hitPos.z;
                w->flags |= EFT_KIPROP_HITMOVED;
                task->result &= ~0xC0;
            }
        }
    }
}

/* Places the model (scale, spin about X, position) and draws the fragments. */
void EftKiObj_Draw(EftTask *task) {
    Mtx44 m;
    Mtx44 s;
    EftVec pos;
    EftKiProp *w = task->work;

    if (w->modelObj != -1) {
        Mtx_StoreIdentity(&m);
        Mtx_StoreIdentity(&s);
        s.m[0][0] = w->scale;
        s.m[1][1] = w->scale;
        s.m[2][2] = w->scale;
        Mtx_Mul(&m, &m, &s);
        Mtx_RotateX(&m, &m, EftMath_WrapAngle(-w->spin));
        Vec4_Set(V(&pos), w->pos.x, w->pos.y, w->pos.z, 1.0f);
        Mtx_SetTrans(&m, V(&pos));
        EftObj_SetMtx(w->modelObj, &m);
        if (!BtlScene_IsEffectHidden(w->arg.objId, 0)) {
            EftKiObj_DrawFrags(task);
        }
    }
}

/* Breaks the blast: five fragments at pos with random upward directions (speed 4, vertical part x 1.3) and a
   random size factor. */
void EftKiObj_Break(EftTask *task, EftVec *pos) {
    EftKiProp *w = task->work;
    EftKiPropFrag *f;
    s32 i;

    f = w->frag;
    for (i = 0; i < EFT_KIPROP_FRAGS; i++) {
        Vec4_Copy(V(&f->pos), V(pos));
        f->vel.x = RANDF() * 2.0f - 1.0f;
        f->vel.y = -RANDF();
        f->vel.z = RANDF() * 2.0f - 1.0f;
        f->vel.w = 1.0f;
        Vec3_Normalize(V(&f->vel), V(&f->vel));
        Vec3_Scale(V(&f->vel), V(&f->vel), 4.0f);
        f->vel.y *= 1.3f;
        f->vel.w = RANDF() * 0.5f + 0.5f;
        f++;
    }
    w->flags |= EFT_KIPROP_FRAGS_ON;
}

/* Moves the fragments under gravity until their timer runs out. */
void EftKiObj_StepFrags(EftTask *task) {
    EftKiProp *w = task->work;
    EftKiPropFrag *f;
    s32 i;

    if (w->flags & EFT_KIPROP_FRAGS_ON) {
        f = w->frag;
        w->fragTimer--;
        if (w->fragTimer <= 0) {
            w->fragTimer = 0;
            w->flags &= ~EFT_KIPROP_FRAGS_ON;
        } else {
            for (i = 0; i < EFT_KIPROP_FRAGS; i++) {
                Vec3_Add(V(&f->pos), V(&f->pos), V(&f->vel));
                f->vel.y += w->gravity;
                f++;
            }
        }
    }
}

/* Draws the fragments as sprites turned by the blast's spin. */
void EftKiObj_DrawFrags(EftTask *task) {
    EftKiProp *w = task->work;
    EftKiPropFrag *f;
    s32 i;

    f = w->frag;
    if (w->flags & EFT_KIPROP_FRAGS_ON) {
        for (i = 0; i < EFT_KIPROP_FRAGS; i++, f++) {
            EftSpr_DrawRot(f->pos.x, f->pos.y, f->pos.z, 0x80, 0x80, 0x80, 0x80, 0, 0, 0x40, 0x40, 0.0f, 0.0f, 1.0f,
                          1.0f, w->spin, 0, (u32)(w->scale * 0.4f * 4096.0f * f->vel.w), 0, 0, w->tex);
        }
    }
}

/* ---- chain effect (EftChain; its class callbacks and API continue in the second part)  ---------------------------------- */

/* Picks the two texture entries of the effect from its set. */
void EftChain_SetTex(EftArc *w, EftArcTexSet *set, s32 a, s32 b) {
    w->texA = ((EftSTexEntry *)set)[a];
    w->texB = ((EftSTexEntry *)set)[b];
    w->arg.texIdx = b;
}

/* Builds the effect's TEX0 value, once per frame per set entry (the set remembers the built ones). */
void EftChain_BuildTex(EftArc *dst, EftArc *src) {
    s32 one = 1;

    if (!(src->arg.texSet->built & (one << src->arg.texIdx))) {
        dst->tex0 = EftVram_AddImage(&dst->texA, 1, 0);
        dst->tex0 |= (u64)EftVram_AddClut(&dst->texB) << 37;
        src->arg.texSet->entry[src->arg.texIdx].tex0 = dst->tex0;
        src->arg.texSet->built |= one << src->arg.texIdx;
    } else {
        dst->tex0 = src->arg.texSet->entry[src->arg.texIdx].tex0;
    }
}

/* Loads one key (0..2) of the parameters into `out`: seconds become frames, the angle rows are scaled by pi. */
void EftChain_SetKey(EftArcKey *out, EftArc *w, s32 key) {
    EftVec a;
    EftVec b;
    EftVec c;
    EftArcKeys *keys = w->arg.keys;
    EftArcParam *p = w->arg.param;
    s32 j;

    Vec4_Copy(V(&out->colBase), V(&keys->colBase[key]));
    Vec4_Copy(V(&out->colRange), V(&keys->colRange[key]));
    Vec4_Copy(V(&out->col2Base), V(&keys->col2Base[key]));
    Vec4_Copy(V(&out->col2Range), V(&keys->col2Range[key]));
    for (j = 0; j < 2; j++) {
        a.v[j] = keys->mulR[key][j];
        b.v[j] = keys->mulG[key][j];
        c.v[j] = keys->mulB[key][j];
    }
    out->mulBase[0] = a.v[0];
    out->mulBase[1] = b.v[0];
    out->mulBase[2] = c.v[0];
    out->mulRange[0] = a.v[1] - a.v[0];
    out->mulRange[1] = b.v[1] - b.v[0];
    out->mulRange[2] = c.v[1] - c.v[0];
    out->mulTime = keys->mulTime[key] * 30.0f;
    out->spawnWait = p->r0[key] * 30.0f;
    out->f60 = p->r1[key] * 3.14159265f;
    out->f64 = p->r2[key] * 3.14159265f;
    out->f68 = p->r3[key] * 3.14159265f;
    out->f6C = p->r4[key] * 3.14159265f;
    out->f70 = p->r5[key] * 3.14159265f;
    out->f74 = p->r6[key] * 3.14159265f;
    out->nodeCount = p->r7[key];
    out->growRate = p->r8[key];
    out->shrinkRate = p->r9[key];
    out->lifeBase = p->r10[key] * 30.0f;
    out->lifeRange = p->r11[key] * 30.0f;
    out->f8C = p->r12[key];
    out->widthBase = p->r13[key];
    out->widthRange = p->r14[key];
    a.v[0] = p->r15[key];
    a.v[1] = p->r16[key];
    out->pulseBase = a.v[0];
    out->pulseRange = a.v[1] - a.v[0];
    out->pulseTime = p->r17[key] * 30.0f;
    out->fA4 = p->r18[key];
    out->endWidth = p->r19[key];
    out->endAlpha = p->r20[key];
    out->radBase = p->r21[key];
    out->radRange = p->r22[key];
    out->rad2Base = p->r23[key];
    out->rad2Range = p->r24[key];
    out->jitter = p->r25[key];
    out->fC4 = p->r26[key];
    out->turnBase = p->r27[key];
    out->turnRange = p->r28[key];
    out->stepXBase = p->r29[key] * 3.14159265f;
    out->stepXRange = p->r30[key] * 3.14159265f;
    out->stepYBase = p->r31[key] * 3.14159265f;
    out->stepYRange = p->r32[key] * 3.14159265f;
    out->crawlXBase = p->r33[key] * 3.14159265f;
    out->crawlXRange = p->r34[key] * 3.14159265f;
    out->crawlYBase = p->r35[key] * 3.14159265f;
    out->crawlYRange = p->r36[key] * 3.14159265f;
    out->fadeIn = p->r37[key] * 30.0f;
    out->fadeOut = p->r38[key] * 30.0f;
}

#define ROW(n, k) (p->r##n[k])

/* Blends the current parameters between two keys by the key timer (keys 0..1 up to keyMid, then 1..2).
   As in the original: rows 2, 4 and 6 use the delta of the row before them, and the step / crawl angle rows
   are not scaled by pi here (EftChain_SetKey scales them). */
#if 0 /* not matched: same operations, but register allocation differs through the whole 600-instruction block after the loop (the original keeps `out` in s6 and has a 0x180 frame; this C gets a 0x1A0 frame with `out` in t9 saved around the calls). The part up to the end of the pair loop is the same instruction sequence.
Cleanup W1 (analysis only, no new form found): the 0x20 of frame is the caller-save slot of `out` plus spill slots. In
the original the long block behind the loop keeps only SEVEN block-local address temporaries in saved registers
(s0-s4, s7, s8), so global allocation still finds s5 for `p` and s6 for `out`; about fifty other address temporaries
(set once, used once, long-lived) go to stack slots 0x40..0x110(sp). In this attempt local-alloc gives EIGHT of those
long-lived temporaries a saved register (s0-s5, s7, s8: lengths 170..270 instructions, 2 or 3 references each), which
leaves one saved register for `p` and pushes `out` into t9. So the cause is one more address temporary alive across
the block than in the original, not the struct view: the rows are already separate members, and the original's
address shapes agree with that (bases p+4 / p+8 / p+12, i.e. row offset modulo 16, plus k * 4 + a multiple of 16).
Worth trying next: the original forms the k1 side of a row as `off16(k1*4 + (p + rest))` with the sum SHARED between
the rows of equal rest (t7 = k1*4 + p + 4 serves 0x14, 0x44, ...), and the k0 side as a fresh `(p + rest) + (k0*4 +
off16)` per row; here both sides come out in the per-row form, which is where the extra temporaries come from. */
void EftChain_BlendKeys(EftArc *w) {
    EftVec d;
    EftVec a;
    EftVec b;
    EftVec c;
    EftArcKey *out = &w->cur;
    EftArcParam *p = w->arg.param;
    EftArcKeys *keys = w->arg.keys;
    f32 t;
    f32 mid;
    s32 k0;
    s32 k1;
    s32 j;

    t = w->keyT;
    mid = w->keyMid;
    if (t < mid) {
        t = t / mid;
        k0 = 0;
        k1 = 1;
    } else {
        t = (t - mid) / (w->keyEnd - mid);
        k0 = 1;
        k1 = 2;
    }
    Vec4_Sub(V(&d), V(&keys->colBase[k1]), V(&keys->colBase[k0]));
    Vec4_Scale(V(&d), V(&d), t);
    Vec4_Add(V(&out->colBase), V(&keys->colBase[k0]), V(&d));
    Vec4_Sub(V(&d), V(&keys->colRange[k1]), V(&keys->colRange[k0]));
    Vec4_Scale(V(&d), V(&d), t);
    Vec4_Add(V(&out->colRange), V(&keys->colRange[k0]), V(&d));
    Vec4_Sub(V(&d), V(&keys->col2Base[k1]), V(&keys->col2Base[k0]));
    Vec4_Scale(V(&d), V(&d), t);
    Vec4_Add(V(&out->col2Base), V(&keys->col2Base[k0]), V(&d));
    Vec4_Sub(V(&d), V(&keys->col2Range[k1]), V(&keys->col2Range[k0]));
    Vec4_Scale(V(&d), V(&d), t);
    Vec4_Add(V(&out->col2Range), V(&keys->col2Range[k0]), V(&d));
    for (j = 0; j < 2; j++) {
        d.v[0] = keys->mulR[k1][j] - keys->mulR[k0][j];
        d.v[1] = keys->mulG[k1][j] - keys->mulG[k0][j];
        d.v[2] = keys->mulB[k1][j] - keys->mulB[k0][j];
        a.v[j] = keys->mulR[k0][j] + d.v[0] * t;
        b.v[j] = keys->mulG[k0][j] + d.v[1] * t;
        c.v[j] = keys->mulB[k0][j] + d.v[2] * t;
    }
    out->mulBase[0] = a.v[0];
    out->mulBase[1] = b.v[0];
    out->mulBase[2] = c.v[0];
    out->mulRange[0] = a.v[1] - a.v[0];
    out->mulRange[1] = b.v[1] - b.v[0];
    out->mulRange[2] = c.v[1] - c.v[0];
    d.v[0] = keys->mulTime[k1] - keys->mulTime[k0];
    out->mulTime = (keys->mulTime[k0] + d.v[0] * t) * 30.0f;
    d.v[0] = ROW(0, k1) - ROW(0, k0);
    out->spawnWait = (ROW(0, k0) + d.v[0] * t) * 30.0f;
    d.v[0] = ROW(1, k1) - ROW(1, k0);
    d.v[1] = ROW(2, k1) - ROW(2, k0);
    out->f60 = (ROW(1, k0) + d.v[0] * t) * 3.14159265f;
    out->f64 = (ROW(2, k0) + d.v[0] * t) * 3.14159265f;
    d.v[0] = ROW(3, k1) - ROW(3, k0);
    d.v[1] = ROW(4, k1) - ROW(4, k0);
    out->f68 = (ROW(3, k0) + d.v[0] * t) * 3.14159265f;
    out->f6C = (ROW(4, k0) + d.v[0] * t) * 3.14159265f;
    d.v[0] = ROW(5, k1) - ROW(5, k0);
    d.v[1] = ROW(6, k1) - ROW(6, k0);
    out->f70 = (ROW(5, k0) + d.v[0] * t) * 3.14159265f;
    out->f74 = (ROW(6, k0) + d.v[0] * t) * 3.14159265f;
    d.v[0] = ROW(7, k1) - ROW(7, k0);
    d.v[1] = ROW(8, k1) - ROW(8, k0);
    d.v[2] = ROW(9, k1) - ROW(9, k0);
    out->nodeCount = ROW(7, k0) + d.v[0] * t;
    out->growRate = ROW(8, k0) + d.v[1] * t;
    out->shrinkRate = ROW(9, k0) + d.v[2] * t;
    d.v[0] = ROW(10, k1) - ROW(10, k0);
    d.v[1] = ROW(11, k1) - ROW(11, k0);
    d.v[2] = ROW(12, k1) - ROW(12, k0);
    out->lifeBase = (ROW(10, k0) + d.v[0] * t) * 30.0f;
    out->lifeRange = (ROW(11, k0) + d.v[1] * t) * 30.0f;
    out->f8C = ROW(12, k0) + d.v[2] * t;
    d.v[0] = ROW(13, k1) - ROW(13, k0);
    d.v[1] = ROW(14, k1) - ROW(14, k0);
    out->widthBase = ROW(13, k0) + d.v[0] * t;
    out->widthRange = ROW(14, k0) + d.v[1] * t;
    d.v[0] = ROW(15, k1) - ROW(15, k0);
    d.v[1] = ROW(16, k1) - ROW(16, k0);
    d.v[2] = ROW(17, k1) - ROW(17, k0);
    a.v[0] = ROW(15, k0) + d.v[0] * t;
    a.v[1] = ROW(16, k0) + d.v[1] * t;
    out->pulseBase = a.v[0];
    out->pulseRange = a.v[1] - a.v[0];
    out->pulseTime = (ROW(17, k0) + d.v[2] * t) * 30.0f;
    d.v[0] = ROW(18, k1) - ROW(18, k0);
    d.v[1] = ROW(19, k1) - ROW(19, k0);
    d.v[2] = ROW(9, k1) - ROW(9, k0); /* sic: the original uses row 9's delta for row 20 (found by the differential test) */
    out->fA4 = ROW(18, k0) + d.v[0] * t;
    out->endWidth = ROW(19, k0) + d.v[1] * t;
    out->endAlpha = ROW(20, k0) + d.v[2] * t;
    d.v[0] = ROW(21, k1) - ROW(21, k0);
    d.v[1] = ROW(22, k1) - ROW(22, k0);
    d.v[2] = ROW(23, k1) - ROW(23, k0);
    d.v[3] = ROW(24, k1) - ROW(24, k0);
    out->radBase = ROW(21, k0) + d.v[0] * t;
    out->radRange = ROW(22, k0) + d.v[1] * t;
    out->rad2Base = ROW(23, k0) + d.v[2] * t;
    out->rad2Range = ROW(24, k0) + d.v[3] * t;
    d.v[0] = ROW(25, k1) - ROW(25, k0);
    d.v[1] = ROW(26, k1) - ROW(26, k0);
    out->jitter = ROW(25, k0) + d.v[0] * t;
    out->fC4 = ROW(26, k0) + d.v[1] * t;
    d.v[0] = ROW(27, k1) - ROW(27, k0);
    d.v[1] = ROW(28, k1) - ROW(28, k0);
    out->turnBase = ROW(27, k0) + d.v[0] * t;
    out->turnRange = ROW(28, k0) + d.v[1] * t;
    d.v[0] = ROW(29, k1) - ROW(29, k0);
    d.v[1] = ROW(30, k1) - ROW(30, k0);
    d.v[2] = ROW(31, k1) - ROW(31, k0);
    d.v[3] = ROW(32, k1) - ROW(32, k0);
    out->stepXBase = ROW(29, k0) + d.v[0] * t;
    out->stepXRange = ROW(30, k0) + d.v[1] * t;
    out->stepYBase = ROW(31, k0) + d.v[2] * t;
    out->stepYRange = ROW(32, k0) + d.v[3] * t;
    d.v[0] = ROW(33, k1) - ROW(33, k0);
    d.v[1] = ROW(34, k1) - ROW(34, k0);
    d.v[2] = ROW(35, k1) - ROW(35, k0);
    d.v[3] = ROW(36, k1) - ROW(36, k0);
    out->crawlXBase = ROW(33, k0) + d.v[0] * t;
    out->crawlXRange = ROW(34, k0) + d.v[1] * t;
    out->crawlYBase = ROW(35, k0) + d.v[2] * t;
    out->crawlYRange = ROW(36, k0) + d.v[3] * t;
    d.v[0] = ROW(37, k1) - ROW(37, k0);
    d.v[1] = ROW(38, k1) - ROW(38, k0);
    out->fadeIn = (ROW(37, k0) + d.v[0] * t) * 30.0f;
    out->fadeOut = (ROW(38, k0) + d.v[1] * t) * 30.0f;
}
#else
LIT4_WORD(D_002FCC54, 0x40490FDA); /* the function's pi */
INCLUDE_ASM("asm/nonmatchings/battle/eft_s", EftChain_BlendKeys);
#endif

/* Takes up to `count` free nodes from the shared pool (round robin from its cursor) and links them as the
   chain. */
s32 EftChain_AllocNodes(EftArcChain *ch, s32 count) {
    s32 n = 0;
    EftArcNode *prev = NULL;
    s32 tries = 0;
    s32 idx = gEftChain->cursor;
    EftArcNode *node;

    for (; tries < EFT_ARC_NODES; tries++) {
        node = (EftArcNode *)(idx * sizeof(EftArcNode) + (u32)gEftChain->node);
        if (node->flags == 0) {
            memset(node, 0, sizeof(EftArcNode));
            if (prev == NULL) {
                ch->head = node;
            } else {
                prev->next = node;
            }
            gEftChain->cursor = idx + 1;
            prev = node;
            if (gEftChain->cursor >= EFT_ARC_NODES) {
                gEftChain->cursor = 0;
            }
            n++;
            if (n >= count) {
                break;
            }
        }
        idx++;
        if (idx >= EFT_ARC_NODES) {
            idx = 0;
        }
    }
    ch->count = n;
    return 1;
}

/* Returns the chain's nodes to the pool. */
void EftChain_FreeNodes(EftArcChain *ch) {
    EftArcNode **link = &ch->head;
    EftArcNode *node;

    while (*link != NULL) {
        node = *link;
        link = &node->next;
        node->flags = 0;
    }
    ch->head = NULL;
}

/* Lays out a new chain: node radii from a random start to a random end radius along the chain's direction
   (which turns by stepX / stepY per node), random jitter in proportion to the node spacing, random widths
   (the two end nodes at a fifth). */
void EftChain_InitStrand(EftTask *task, EftArcChain *ch) {
    Mtx44 m;
    Mtx44 base;
    EftVec dir;
    EftVec d;
    EftVec prev;
    EftArc *w = task->work;
    EftArcKey *key = &w->cur;
    EftArcNode *n;
    EftArcNode **link;
    s32 i = 0;
    f32 r;
    f32 step;

    r = key->radBase + key->radRange * RANDF();
    step = key->rad2Base + key->rad2Range * RANDF();
    step = (step - r) / (f32)ch->count;
    ch->radStep = step;
    r *= w->arg.scale;
    step *= w->arg.scale;
    Mtx_StoreIdentity(&base);
    link = &ch->head;
    while (*link != NULL) {
        n = *link;
        n->flags |= 1;
        Vec4_Set(V(&dir), 0.0f, 0.0f, 1.0f, 1.0f);
        Mtx_RotateX(&m, &base, ch->rotX);
        Mtx_RotateY(&m, &m, ch->rotY);
        Mtx_RotateZ(&m, &m, ch->rotZ);
        Mtx_MulVec4(V(&dir), &m, V(&dir));
        if (w->flags & EFT_ARC_ORIENTED) {
            Mtx_MulVec4(V(&dir), &w->mtx, V(&dir));
        }
        n->radius = r;
        Vec3_Scale(V(&n->pos), V(&dir), r);
        n->pos.w = 1.0f;
        if (i == 0) {
            Vec4_Set(V(&n->jit), 0.0f, 0.0f, 0.0f, 1.0f);
            n->amp = 0.0f;
        } else {
            Vec3_Sub(V(&d), V(&n->pos), V(&prev));
            d.w = 1.0f;
            n->amp = sqrtf(Vec3_Dot(V(&d), V(&d))) * key->jitter;
            n->jit.x = n->amp * (RANDF() - RANDF());
            n->jit.y = n->amp * (RANDF() - RANDF());
            n->jit.z = n->amp * (RANDF() - RANDF());
            n->jit.w = 1.0f;
        }
        Vec4_Copy(V(&prev), V(&n->pos));
        n->widthA = key->widthBase + key->widthRange * RANDF();
        n->widthB = key->widthBase + key->widthRange * RANDF();
        if (i == 0 || n->next == NULL) {
            n->widthA *= 0.2f;
            n->widthB *= 0.2f;
        }
        Vec4_Copy(V(&n->color), V(&ch->color));
        r += step;
        i++;
        ch->rotX += ch->stepX;
        ch->rotX = EftMath_WrapAngle(ch->rotX);
        ch->rotY += ch->stepY;
        ch->rotY = EftMath_WrapAngle(ch->rotY);
        link = &n->next;
    }
    Vec3_Copy(V(&ch->drift), V(&d));
}

/* One frame of a chain's nodes: shows the first `shown` nodes; once the chain is fully grown, kind 1 crawls
   (every node takes its successor's place and the last one gets a new position), the other kinds drift and
   re-roll the jitter; then world position and colour of every node (kinds 0 and 2 fade the head).
   As in the original, the crawl adds crawlY to the wrapped rotX to get rotY. */
void EftChain_UpdateStrand(EftTask *task, EftArcChain *ch) {
    Mtx44 m;
    EftVec old;
    EftVec d;
    EftVec dir;
    EftArc *w = task->work;
    EftArcParam *p = w->arg.param;
    EftArcKey *key = &w->cur;
    EftArcNode *n;
    EftArcNode *next;
    EftArcNode **link;
    s32 i = 0;
    f32 fi;

    link = &ch->head;
    while (*link != NULL) {
        n = *link;
        fi = (f32)i;
        next = n->next;
        if (fi < ch->shown) {
            n->flags |= 2;
        } else {
            n->flags &= ~2;
        }
        if (ch->flags & EFT_ARCCH_GROWN) {
            if (p->kind == 1) {
                if (next != NULL) {
                    Vec4_Copy(V(&n->pos), V(&next->pos));
                    Vec4_Copy(V(&n->jit), V(&next->jit));
                } else {
                    Vec4_Copy(V(&old), V(&n->pos));
                    Vec4_Set(V(&dir), 0.0f, 0.0f, 1.0f, 1.0f);
                    Mtx_StoreIdentity(&m);
                    Mtx_RotateX(&m, &m, ch->rotX);
                    Mtx_RotateY(&m, &m, ch->rotY);
                    Mtx_MulVec4(V(&dir), &m, V(&dir));
                    Vec3_Scale(V(&n->pos), V(&dir), n->radius);
                    n->pos.w = 1.0f;
                    Vec3_Sub(V(&d), V(&old), V(&n->pos));
                    d.w = 1.0f;
                    n->amp = sqrtf(Vec3_Dot(V(&d), V(&d))) * key->jitter;
                    n->jit.x = n->amp * (RANDF() - RANDF());
                    n->jit.y = n->amp * (RANDF() - RANDF());
                    n->jit.z = n->amp * (RANDF() - RANDF());
                    n->jit.w = 1.0f;
                    ch->rotX += ch->crawlX;
                    ch->rotY = EftMath_WrapAngle(ch->rotX) + ch->crawlY;
                    ch->rotY = EftMath_WrapAngle(ch->rotY);
                }
            } else {
                Vec3_Add(V(&n->pos), V(&n->pos), V(&ch->drift));
                n->jit.x = n->amp * (RANDF() - RANDF());
                n->jit.y = n->amp * (RANDF() - RANDF());
                n->jit.z = n->amp * (RANDF() - RANDF());
                n->jit.w = 1.0f;
            }
        }
        Vec3_Add4(V(&n->world), V(&w->arg.pos), V(&ch->pos), V(&n->pos), V(&n->jit));
        n->world.w = 1.0f;
        Vec4_Copy(V(&n->color), V(&ch->color));
        n->color.w *= ch->alpha;
        if (n->color.w > 255.0f) {
            n->color.w = 255.0f;
        }
        if (p->kind == 0 || p->kind == 2) {
            if (ch->shown - 1.0f <= fi) {
                n->color.w *= 0.0f;
            } else if (ch->shown - 2.0f <= fi) {
                n->color.w *= 0.5f;
            }
        }
        i++;
        link = &n->next;
    }
}

/* A GS screen position as Vu0Cur_ProjectPoints writes it. */
typedef struct EftSScr {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ s32 w;
} EftSScr; /* 0x10 */

/* GS XYZF2 register value. */
typedef struct EftSXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftSXyzf;

typedef struct EftSGsVtx {
    /* 0x00 */ u8 rgba[4];
    /* 0x04 */ f32 q;
    /* 0x08 */ f32 s;
    /* 0x0C */ f32 t;
    /* 0x10 */ EftSXyzf xyz;
} EftSGsVtx; /* 0x18: RGBAQ, ST, XYZF2 */

/* Packet of one ribbon segment: REGLIST of PRIM, TEX0_1 and four vertices (a triangle strip). */
typedef struct EftArcPkt {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftSGsVtx v[4];
} EftArcPkt; /* 0x90 */

/* Vertex handed to the shared polygon clipper. */
typedef struct EftSClipVtx {
    /* 0x00 */ f32 pos[4];
    /* 0x10 */ f32 st[4];
    /* 0x20 */ f32 col[4];
} EftSClipVtx; /* 0x30 */

/* Links a packet into the chain of a depth slot (clamped to 0..0xFFF). */
static inline void EftChain_OtAdd(OtPrim *p, s32 z, s32 layer) {
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

/* Draws a chain as a ribbon facing the camera: one textured quad per pair of shown nodes, widened across the
   segment by the nodes' widths, queued by its average depth. A clipped quad is skipped. */
void EftChain_DrawStrand(EftArc *w, EftArc *w2, EftArcChain *ch) {
    EftVec uv[4] = { { { 0.0f, 0.0f, 1.0f, 0.0f } }, { { 0.0f, 1.0f, 1.0f, 0.0f } }, { { 1.0f, 0.0f, 1.0f, 0.0f } }, { { 1.0f, 1.0f, 1.0f, 0.0f } } };
    EftVec quad[4];
    EftVec prevEnd[2];
    EftVec st[4];
    EftVec cam;
    EftVec side;
    EftVec toCam;
    EftVec offA;
    EftVec offB;
    EftSScr scr[4];
    s32 colA[4];
    s32 colB[4];
    EftArcParam *p;
    s32 first;
    EftArcNode **link;
    EftArcNode *n;
    EftArcNode *next;
    EftArcPkt *pkt;
    s32 z;
    s32 j;

    first = 1;
    p = w->arg.param;
    Vec4_Copy(V(&cam), V(&gBtlCamView->pos));
    for (link = &ch->head; *link != NULL; link = &n->next) {
        n = *link;
        next = n->next;
        if (next != NULL && (n->flags & 2)) {
            Vec3_Sub(V(&side), V(&next->world), V(&n->world));
            Vec3_Sub(V(&toCam), V(&n->world), V(&cam));
            Vec3_Cross(V(&side), V(&side), V(&toCam));
            Vec3_Normalize(V(&side), V(&side));
            if (first) {
                Vec3_Scale(V(&offA), V(&side), n->widthA * ch->width);
                Vec3_Scale(V(&offB), V(&side), n->widthB * ch->width);
                Vec3_Add(V(&quad[0]), V(&n->world), V(&offA));
                Vec3_Sub(V(&quad[1]), V(&n->world), V(&offB));
            } else {
                Vec3_Copy(V(&quad[0]), V(&prevEnd[0]));
                Vec3_Copy(V(&quad[1]), V(&prevEnd[1]));
            }
            first = 0;
            Vec3_Scale(V(&offA), V(&side), next->widthA * ch->width);
            Vec3_Scale(V(&offB), V(&side), next->widthB * ch->width);
            Vec3_Add(V(&quad[2]), V(&next->world), V(&offA));
            Vec3_Sub(V(&quad[3]), V(&next->world), V(&offB));
            Vec3_Copy(V(&prevEnd[0]), V(&quad[2]));
            Vec3_Copy(V(&prevEnd[1]), V(&quad[3]));
            if (Vu0Cur_ProjectPoints(scr, V(quad), 4) != 0) {
                pkt = (EftArcPkt *)gOtCurRead;
                gOtCur = (u32 *)(pkt + 1);
                if (pkt == NULL) {
                    return;
                }
                if (p->layer < 2) {
                    pkt->prim = 0x5C;
                    pkt->tag = 0x20000008;
                    pkt->vif0 = 0x10000000;
                    pkt->vif1 = 0x50000008;
                    pkt->gif0 = 0xE400000000008001;
                    pkt->gif1 = 0x42142142142160;
                    pkt->next = 0;
                } else {
                    pkt->prim = 0x25C;
                    pkt->tag = 0x20000008;
                    pkt->vif0 = 0x10000000;
                    pkt->vif1 = 0x50000008;
                    pkt->gif0 = 0xE400000000008001;
                    pkt->gif1 = 0x42142142142170;
                    pkt->next = 0;
                }
                for (j = 0; j < 4; j++) {
                    f32 q = 1.0f / (f32)scr[j].w;

                    Vec3_Scale(V(&st[j]), V(&uv[j]), q);
                }
                Vec4_ToInt(colA, V(&n->color));
                Vec4_ToInt(colB, V(&next->color));
                z = (scr[0].z + scr[2].z + (scr[3].z + scr[1].z)) >> 10;
                if (w2->flags & EFT_ARC_WHITE) {
                    for (j = 3; j >= 0; j--) {
                        scr[j].z = 0xFFFFFF;
                    }
                }
                pkt->v[0].rgba[0] = colA[0];
                pkt->v[0].rgba[1] = colA[1];
                pkt->v[0].rgba[2] = colA[2];
                pkt->v[0].rgba[3] = colA[3];
                pkt->v[0].q = st[0].z;
                pkt->v[1].rgba[0] = colA[0];
                pkt->v[1].rgba[1] = colA[1];
                pkt->v[1].rgba[2] = colA[2];
                pkt->v[1].rgba[3] = colA[3];
                pkt->v[1].q = st[1].z;
                pkt->v[2].rgba[0] = colB[0];
                pkt->v[2].rgba[1] = colB[1];
                pkt->v[2].rgba[2] = colB[2];
                pkt->v[2].rgba[3] = colB[3];
                pkt->v[2].q = st[2].z;
                pkt->v[3].rgba[0] = colB[0];
                pkt->v[3].rgba[1] = colB[1];
                pkt->v[3].rgba[2] = colB[2];
                pkt->v[3].rgba[3] = colB[3];
                pkt->v[3].q = st[3].z;
                pkt->v[0].s = st[0].x;
                pkt->v[0].t = st[0].y;
                pkt->v[1].s = st[1].x;
                pkt->v[1].t = st[1].y;
                pkt->v[2].s = st[2].x;
                pkt->v[2].t = st[2].y;
                pkt->v[3].s = st[3].x;
                pkt->v[3].t = st[3].y;
                pkt->v[0].xyz.x = scr[0].x;
                pkt->v[0].xyz.y = scr[0].y;
                pkt->v[0].xyz.z = scr[0].z;
                pkt->v[0].xyz.f = 0xFF;
                pkt->v[1].xyz.x = scr[1].x;
                pkt->v[1].xyz.y = scr[1].y;
                pkt->v[1].xyz.z = scr[1].z;
                pkt->v[1].xyz.f = 0xFF;
                pkt->v[2].xyz.x = scr[2].x;
                pkt->v[2].xyz.y = scr[2].y;
                pkt->v[2].xyz.z = scr[2].z;
                pkt->v[2].xyz.f = 0xFF;
                pkt->v[3].xyz.x = scr[3].x;
                pkt->v[3].xyz.y = scr[3].y;
                pkt->v[3].xyz.z = scr[3].z;
                pkt->v[3].xyz.f = 0xFF;
                pkt->tex0 = w2->tex0;
                EftChain_OtAdd((OtPrim *)pkt, z, p->layer);
            }
        }
    }
}

/* The same ribbon through the shared polygon clipper: two triangles per segment. */
void EftChain_DrawStrandClipped(EftArc *w, EftArc *w2, EftArcChain *ch) {
    EftVec uv[4] = { { { 0.0f, 0.0f, 1.0f, 0.0f } }, { { 0.0f, 1.0f, 1.0f, 0.0f } }, { { 1.0f, 0.0f, 1.0f, 0.0f } }, { { 1.0f, 1.0f, 1.0f, 0.0f } } };
    EftVec quad[4];
    EftVec prevEnd[2];
    EftVec cam;
    EftVec side;
    EftVec toCam;
    EftVec offA;
    EftVec offB;
    EftVec col[4];
    EftSClipVtx poly[9];
    EftArcParam *p;
    s32 first;
    EftArcNode **link;
    EftArcNode *n;
    EftArcNode *next;
    s32 j;

    first = 1;
    p = w->arg.param;
    Vec4_Copy(V(&cam), V(&gBtlCamView->pos));
    for (link = &ch->head; *link != NULL; link = &n->next) {
        n = *link;
        next = n->next;
        if (next != NULL && (n->flags & 2)) {
            Vec3_Sub(V(&side), V(&next->world), V(&n->world));
            Vec3_Sub(V(&toCam), V(&n->world), V(&cam));
            Vec3_Cross(V(&side), V(&side), V(&toCam));
            Vec3_Normalize(V(&side), V(&side));
            if (first) {
                Vec3_Scale(V(&offA), V(&side), n->widthA * ch->width);
                Vec3_Scale(V(&offB), V(&side), n->widthB * ch->width);
                Vec3_Add(V(&quad[0]), V(&n->world), V(&offA));
                Vec3_Sub(V(&quad[1]), V(&n->world), V(&offB));
            } else {
                Vec3_Copy(V(&quad[0]), V(&prevEnd[0]));
                Vec3_Copy(V(&quad[1]), V(&prevEnd[1]));
            }
            first = 0;
            Vec3_Scale(V(&offA), V(&side), next->widthA * ch->width);
            Vec3_Scale(V(&offB), V(&side), next->widthB * ch->width);
            Vec3_Add(V(&quad[2]), V(&next->world), V(&offA));
            Vec3_Sub(V(&quad[3]), V(&next->world), V(&offB));
            Vec3_Copy(V(&prevEnd[0]), V(&quad[2]));
            Vec3_Copy(V(&prevEnd[1]), V(&quad[3]));
            Vec4_Copy(V(&col[0]), V(&n->color));
            Vec4_Copy(V(&col[1]), V(&n->color));
            Vec4_Copy(V(&col[2]), V(&next->color));
            Vec4_Copy(V(&col[3]), V(&next->color));
            for (j = 0; j < 2; j++) {
                ClipVtx_Set(&poly[0], V(&quad[j]), V(&uv[j]), V(&col[j]));
                ClipVtx_Set(&poly[1], V(&quad[j + 1]), V(&uv[j + 1]), V(&col[j + 1]));
                ClipVtx_Set(&poly[2], V(&quad[j + 2]), V(&uv[j + 2]), V(&col[j + 2]));
                EftGfx_DrawPolyScaledZ(poly, p->layer, 0, 0, (w2->flags >> 4) & 1, 0, w2->tex0, 1.0f);
            }
        }
    }
}

/* Starts a chain in a free slot: nodes from the pool (fewer than 3: gives up), random life, colours, direction
   (kind 2: random pitch, and a yaw that advances per chain; other kinds: the effect's slot rotation), turn
   steps mirrored on odd slots, then the node layout. */
s32 EftChain_StartStrand(EftTask *task, EftArcChain *ch) {
    EftArc *w = task->work;
    EftArcParam *p = w->arg.param;
    EftArcKey *key = &w->cur;
    f32 r;
    f32 life;
    f32 cx;
    f32 cy;

    ch->head = NULL;
    EftChain_AllocNodes(ch, w->nodeCount);
    if (ch->count < 3) {
        EftChain_FreeNodes(ch);
        return 0;
    }
    ch->flags |= EFT_ARCCH_ON;
    life = key->lifeBase + key->lifeRange * RANDF();
    ch->time = 0.0f;
    ch->life = life;
    ch->f7C = life * key->f8C;
    ch->endAt = life * key->fA4;
    ch->fadeAt = life - key->fadeOut;
    if (key->shrinkRate <= ch->time) { /* the field read back, not a local (a local puts the rate in f1, not f2) */
        ch->flags |= EFT_ARCCH_SHRUNK;
    } else {
        r = (f32)ch->count / key->shrinkRate;
        ch->shrinkAt = life - r;
    }
    ch->shown = 0.0f;
    ch->width = 1.0f;
    r = RANDF();
    ch->colA.x = key->colBase.x + key->colRange.x * r;
    ch->colA.y = key->colBase.y + key->colRange.y * r;
    ch->colA.z = key->colBase.z + key->colRange.z * r;
    ch->colA.w = key->colBase.w + key->colRange.w * r;
    r = RANDF();
    ch->colB.x = key->col2Base.x + key->col2Range.x * r;
    ch->colB.y = key->col2Base.y + key->col2Range.y * r;
    ch->colB.z = key->col2Base.z + key->col2Range.z * r;
    ch->colB.w = key->col2Base.w + key->col2Range.w * r;
    Vec4_Sub(V(&ch->colB), V(&ch->colB), V(&ch->colA));
    Vec4_Copy(V(&ch->color), V(&ch->colA));
    if (p->kind == 2) {
        f32 a;
        f32 b;

        a = key->f60 + key->f64 * RANDF();
        b = key->f70 + key->f74 * RANDF();
        ch->rotX = a;
        ch->rotX = EftMath_WrapAngle(ch->rotX);
        ch->rotY = 0.0f;
        ch->rotZ = w->rotZ;
        w->rotZ += b;
        w->rotZ = EftMath_WrapAngle(w->rotZ);
    } else {
        ch->rotX = w->rotX[w->slot];
        ch->rotY = w->rotY[w->slot];
        ch->rotZ = w->rotZ;
    }
    if (!(w->slot & 1)) {
        ch->stepX = key->stepXBase + key->stepXRange * RANDF();
        ch->stepY = key->stepYBase + key->stepYRange * RANDF();
    } else {
        ch->stepX = -(key->stepXBase + key->stepXRange * RANDF());
        ch->stepY = -(key->stepYBase + key->stepYRange * RANDF());
    }
    EftChain_InitStrand(task, ch);
    cx = key->crawlXBase + key->crawlXRange * RANDF();
    cy = key->crawlYBase + key->crawlYRange * RANDF();
    if (!(w->slot & 1)) {
        ch->crawlX = cx;
        ch->crawlY = cy;
    } else {
        ch->crawlX = -cx;
        ch->crawlY = -cy;
    }
    r = key->turnBase + key->turnRange * RANDF();
    ch->crawlX *= r;
    ch->crawlY *= r;
    Vec3_Scale(V(&ch->drift), V(&ch->drift), r);
    switch (p->offsetMode) {
    case 0:
        ch->flags |= EFT_ARCCH_DRIFT;
        break;
    case 1:
        if (!(rand() & 1)) {
            ch->flags |= EFT_ARCCH_DRIFT;
        }
        break;
    case 2:
        break;
    }
    ch->fadeIn = key->fadeIn;
    if (ch->fadeIn <= 0.0f) {
        ch->flags |= EFT_ARCCH_FADED_IN;
    }
    ch->fadeOut = key->fadeOut;
    if (ch->fadeOut <= 0.0f) {
        ch->flags |= EFT_ARCCH_FADED_OUT;
    } else {
        ch->fadeOutT = ch->fadeOut;
    }
    Vec3_Scale(V(&ch->drift), V(&ch->drift), 1.0f);
    w->live++;
    return 1;
}

/* One frame of every chain: growth, shrinking, the end phase, width and colour pulses, colour drift, fades,
   visibility, the nodes; a chain whose life is over and whose alpha reached 0 is freed on the next frame. */
void EftChain_UpdateStrands(EftTask *task) {
    EftArc *w = task->work;
    EftArcParam *p = w->arg.param;
    EftArcKey *key = &w->cur;
    EftArcChain *ch;
    s32 i;
    f32 t;
    f32 u;
    f32 m;

    for (i = 0; i < EFT_ARC_CHAINS; i++) {
        ch = &w->chain[i];
        if (ch->flags & EFT_ARCCH_ON) {
            if (!(ch->flags & EFT_ARCCH_DEAD)) {
                t = ch->time / ch->life;
                if (t > 1.0f) {
                    t = 1.0f;
                }
                if (!(ch->flags & EFT_ARCCH_F40) && ch->f7C <= ch->time) {
                    ch->flags |= EFT_ARCCH_F40;
                }
                if (!(ch->flags & EFT_ARCCH_GROWN)) {
                    ch->shown += key->growRate;
                    if ((f32)ch->count <= ch->shown) {
                        ch->shown = (f32)ch->count;
                        ch->flags |= EFT_ARCCH_GROWN;
                    }
                }
                if (!(ch->flags & EFT_ARCCH_SHRUNK) && ch->shrinkAt <= ch->time) {
                    ch->shown -= key->shrinkRate;
                    if (ch->shown <= 0.0f) {
                        ch->shown = 0.0f;
                        ch->flags |= EFT_ARCCH_SHRUNK;
                    }
                }
                if (!(ch->flags & EFT_ARCCH_ENDING)) {
                    if (ch->endAt <= ch->time) {
                        ch->width = key->endWidth;
                        ch->alpha = key->endAlpha;
                        ch->flags |= EFT_ARCCH_ENDING;
                    }
                } else {
                    ch->width = 1.0f;
                    ch->alpha = 1.0f;
                }
                if ((p->flags & 4) && key->pulseTime > 0.0f) {
                    u = ch->pulseT / key->pulseTime;
                    m = key->pulseBase + key->pulseRange * u;
                    ch->width *= m;
                    if (!(ch->flags & EFT_ARCCH_PULSE_DOWN)) {
                        ch->pulseT += 1.0f;
                        if (key->pulseTime <= ch->pulseT) {
                            ch->pulseT = key->pulseTime;
                            ch->flags |= EFT_ARCCH_PULSE_DOWN;
                        }
                    } else {
                        ch->pulseT -= 1.0f;
                        if (ch->pulseT <= 0.0f) {
                            ch->pulseT = 0.0f;
                            ch->flags &= ~EFT_ARCCH_PULSE_DOWN;
                        }
                    }
                }
                if (ch->flags & EFT_ARCCH_DRIFT) {
                    Vec4_Scale(V(&ch->color), V(&ch->colB), t);
                    Vec4_Add(V(&ch->color), V(&ch->color), V(&ch->colA));
                } else {
                    Vec4_Copy(V(&ch->color), V(&ch->colA));
                }
                Vec4_Clamp(V(&ch->color), V(&ch->color), 0.0f, 255.0f);
                if (!(ch->flags & EFT_ARCCH_FADED_IN)) {
                    u = ch->fadeInT / ch->fadeIn;
                    ch->fadeInT += 1.0f;
                    if (ch->fadeIn <= ch->fadeInT) {
                        ch->flags |= EFT_ARCCH_FADED_IN;
                    } else {
                        ch->color.w *= u;
                    }
                }
                if (ch->time >= ch->fadeAt) {
                    if (!(ch->flags & EFT_ARCCH_FADED_OUT)) {
                        u = ch->fadeOutT / ch->fadeOut;
                        ch->fadeOutT -= 1.0f;
                        if (!(ch->fadeOutT <= 0.0f)) {
                            ch->color.w *= u;
                        } else {
                            ch->color.w = 0.0f;
                            ch->flags |= EFT_ARCCH_FADED_OUT;
                        }
                    } else {
                        ch->color.w = 0.0f;
                    }
                }
                if ((p->flags & 8) && key->mulTime > 0.0f) {
                    u = ch->mulT / key->mulTime;
                    m = key->mulBase[0] + key->mulRange[0] * u;
                    ch->color.x *= m;
                    m = key->mulBase[1] + key->mulRange[1] * u;
                    ch->color.y *= m;
                    m = key->mulBase[2] + key->mulRange[2] * u;
                    ch->color.z *= m;
                    if (!(ch->flags & EFT_ARCCH_MUL_DOWN)) {
                        ch->mulT += 1.0f;
                        if (key->mulTime <= ch->mulT) {
                            ch->mulT = key->mulTime;
                            ch->flags |= EFT_ARCCH_MUL_DOWN;
                        }
                    } else {
                        ch->mulT -= 1.0f;
                        if (ch->mulT <= 0.0f) {
                            ch->mulT = 0.0f;
                            ch->flags &= ~EFT_ARCCH_MUL_DOWN;
                        }
                    }
                }
                if (ch->color.w <= 0.0f || ch->width <= 0.0f || ch->alpha <= 0.0f) {
                    ch->flags &= ~EFT_ARCCH_VISIBLE;
                } else {
                    ch->flags |= EFT_ARCCH_VISIBLE;
                }
                EftChain_UpdateStrand(task, ch);
                ch->time += 1.0f;
                if (ch->life <= ch->time) {
                    if (ch->color.w <= 0.0f) {
                        ch->flags |= EFT_ARCCH_DEAD;
                    }
                }
            } else {
                EftChain_FreeNodes(ch);
                ch->flags = 0;
                w->live--;
            }
        }
    }
}

/* Draws every visible chain, as clipped polygons or as plain quads. */
void EftChain_DrawStrands(EftTask *task) {
    EftArc *w = task->work;
    EftArcParam *p = w->arg.param;
    EftArcChain *ch;
    s32 i;

    ch = w->chain;
    for (i = 0; i < EFT_ARC_CHAINS; i++, ch++) {
        if (ch->flags & (EFT_ARCCH_VISIBLE | EFT_ARCCH_ON)) {
            if (p->flags & 2) {
                EftChain_DrawStrandClipped(w, w, ch);
            } else {
                EftChain_DrawStrand(w, w, ch);
            }
        }
    }
}

/* Starts bursts of chains while the spawn timer is due: countBase (+ a random part) chains per burst, each in a
   free slot; advances the rotation of the current slot (mirrored on odd slots) and steps the slot 0..3. */
void EftChain_Spawn(EftTask *task) {
    EftArc *w = task->work;
    EftArcKey *key = &w->cur;
    EftArcParam *p = w->arg.param;
    s32 full = 0;
    EftArcChain *ch;
    s32 n;
    s32 i;
    f32 a;
    f32 b;

    if (w->spawnWait <= 0.0f) {
        do {
            n = p->countBase;
            if (p->countRange >= 2) {
                n += rand() % p->countRange;
            }
            for (; n > 0; n--) {
                full = 1;
                ch = NULL;
                for (i = 0; i < EFT_ARC_CHAINS; i++) {
                    if (w->chain[i].flags == 0) {
                        ch = &w->chain[i];
                        memset(ch, 0, sizeof(EftArcChain));
                        full = 0;
                        break;
                    }
                }
                if (full) {
                    break;
                }
                if (!EftChain_StartStrand(task, ch)) {
                    break;
                }
                a = key->f60 + key->f64 * RANDF();
                b = key->f68 + key->f6C * RANDF();
                if (!(w->slot & 1)) {
                    w->rotX[w->slot] += a;
                    w->rotY[w->slot] += b;
                } else {
                    w->rotX[w->slot] -= a;
                    w->rotY[w->slot] -= b;
                }
                w->rotX[w->slot] = EftMath_WrapAngle(w->rotX[w->slot]);
                w->rotY[w->slot] = EftMath_WrapAngle(w->rotY[w->slot]);
                w->slot++;
                if (w->slot >= 4) {
                    w->slot = 0;
                }
            }
            w->spawnWait += key->spawnWait;
        } while (!full && w->spawnWait <= 0.0f);
    }
}

/* Init callback: copies the argument, builds the matrix that turns +Z onto the direction, presets the four slot
   rotations, loads key 0 (animated) or key 2 and picks the texture. */
void EftChain_Init(EftTask *task, EftArcArg *arg) {
    EftArc *w = task->work;
    EftArcParam *p = arg->param;
    EftArcKey *key = &w->cur;
    f32 life;

    memset(w, 0, sizeof(EftArc));
    w->arg = *arg;
    w->flags |= EFT_ARC_ON;
    if (p->kind == 2 || (p->flags & 0x10)) {
        w->flags |= EFT_ARC_ORIENTED;
    }
    life = arg->life;
    if (life <= 0.0f) {
        w->flags |= EFT_ARC_ENDLESS;
    } else {
        w->time = 0.0f;
        w->life = life * 30.0f;
    }
    Vec3_Clamp(V(&arg->dir), V(&arg->dir), -1.0f, 1.0f);
    w->pitch = Mathf_Asin(-arg->dir.y);
    w->yaw = atan2f(arg->dir.x, arg->dir.z);
    Mtx_StoreIdentity(&w->mtx);
    Mtx_RotateX(&w->mtx, &w->mtx, w->pitch);
    Mtx_RotateY(&w->mtx, &w->mtx, w->yaw);
    w->rotX[0] = 0.0f;
    w->rotY[0] = 0.0f;
    w->rotX[1] = 3.14159265f;
    w->rotY[1] = 0.0f;
    w->rotX[2] = 0.0f;
    w->rotY[2] = 3.14159265f;
    w->rotX[3] = 3.14159265f;
    w->rotY[3] = 3.14159265f;
    if (p->flags & 1) {
        w->keyEnd = p->keyTime * 30.0f;
        w->keyMid = w->keyEnd * p->keyMid;
        EftChain_SetKey(key, w, 0);
    } else {
        EftChain_SetKey(key, w, 2);
    }
    w->nodeCount = (s32)key->nodeCount;
    EftChain_SetTex(w, arg->texSet, arg->texIdx, arg->texIdx);
}

/* Term callback: frees every chain's nodes. */
void EftChain_Term(EftTask *task) {
    EftArc *w = task->work;
    EftArcChain *ch;
    s32 i;

    w->flags = 0;
    ch = w->chain;
    for (i = 0; i < EFT_ARC_CHAINS; i++) {
        EftChain_FreeNodes(ch);
        ch->flags = 0;
        ch++;
    }
}

/* Update callback: start delay, bursts (until the effect is told to end), chains, key animation, life; once
   ending and every chain is gone the task is killed. Otherwise builds this frame's TEX0. */
void EftChain_Update(EftTask *task) {
    EftArc *w = task->work;
    s32 busy = 0;
    EftArcParam *p = w->arg.param;
    EftArcKey *key;
    s32 i;

    if (!BtlScene_IsEffectStopped(w->arg.objId, w->stopKind)) {
        if (w->delay <= 0.0f) {
            key = &w->cur;
            if (!(w->flags & EFT_ARC_ENDING) || w->endWait > 0.0f) {
                if (w->spawnWait <= 0.0f) {
                    EftChain_Spawn(task);
                } else {
                    w->spawnWait -= 1.0f;
                }
            }
            EftChain_UpdateStrands(task);
            if ((p->flags & 1) && w->keyT <= w->keyEnd) {
                EftChain_BlendKeys(w);
                w->keyT += 1.0f;
                if (w->keyEnd <= w->keyT) {
                    EftChain_SetKey(key, w, 2);
                }
            }
            if (!(w->flags & EFT_ARC_ENDLESS)) {
                w->time += 1.0f;
                if (w->life <= w->time) {
                    w->flags |= EFT_ARC_ENDING;
                }
            }
        } else {
            w->delay -= 1.0f;
        }
        if (w->flags & EFT_ARC_ENDING) {
            if (w->endWait > 0.0f) {
                w->endWait -= 1.0f;
            }
            if (w->endWait <= 0.0f) {
                for (i = 0; i < EFT_ARC_CHAINS; i++) {
                    if (w->chain[i].flags != 0) {
                        busy = 1;
                        break;
                    }
                }
                if (!busy) {
                    w->flags |= EFT_ARC_DEAD;
                }
            }
        }
    }
    if (w->flags & EFT_ARC_DEAD) {
        BtlTask_SetDead(task);
    } else {
        EftChain_BuildTex(w, w);
    }
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly eft_t.c), with its own header and view types. Names the first part already declared with
 * other types are reached through cast macros (the generated code is the same).
 * ------------------------------------------------------------------------------------------------------------ */
#include "battle/eft_t.h"
#include "sys/gfx_ot.h"

/*
 * 0x17CB40..0x17EE68. Two drawing-only effect modules (see include/battle/eft_t.h):
 *
 * 1. 0x17CB40..0x17D290: the tail of the "chain" module, effect pack part kind 18. Its task code (init 0x17C698,
 *    term 0x17C8A8, update 0x17C8F8, the drawing at 0x17C358) is in the file before this one; here are the last
 *    three callbacks of the task class 0x2C3E48, the manager class 0x2C3E30 and the interface the effect pack
 *    library (EftEmit_SpawnType18, EftEmit_KillAll, EftEmit_UpdateAlive) calls with a task handle. Every entry
 *    checks the handle by comparing the task's update callback.
 *
 * 2. 0x17D290..0x17EE68: the ray burst, effect pack part kind 0 ("light" in eft_h.h): up to 48 thin quads
 *    radiating from a point, in the world facing the camera or directly on the screen. Also started by the
 *    fighter effect layer (requests 0x15 and 0x18). Manager class 0x2C3E60, task class 0x2C3E78.
 *
 * Nothing here writes a fighter, a battle object or a hit record. Random numbers: libc rand() only, in
 * EftRay_Setup (5 per ray, 6 for a screen burst) and EftRay_Update (1 + one per ray, every frame, modes 1..3).
 *
 * All of this part is C (EftChain_SetRes matches only with 0x1793A8 defined in the same file). Its .lit4
 * (0x2FCC84..0x2FCCAC) and .rodata (0x2ECDE0..0x2ECE60, the two tables of EftRay_DrawRays and EftRay_DrawQuad2D
 * included) come out identical to the original.
 */

#define gBtlCamView ((EftTCamView *)gBtlCamView)

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);    /* matrix product */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);            /* inverse of a rotation + translation matrix */
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Z */
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about X */
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Y */
extern void Vu0Cur_Push(void);                              /* saves the VU0 matrix state */
extern void Vu0Cur_Pop(void);                              /* restores it */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                          /* loads a matrix into VU0 */
extern void ClipVtx_SetArray(EftTClipVtx *out, Vec4 *pos, Vec4 *st, Vec4 *col, s32 n); /* n clip vertices */
extern s32 ClipPoly_ClipPlane(EftTClipVtx *poly, EftTVec *plane, s32 count); /* clips in place, new count */
extern void ClipPoly_ProjectCur(EftTIVec *scr, EftTVec *st, EftTClipVtx *poly, s32 count); /* projects */
extern void Vec3_Div(Vec4 *dst, Vec4 *src, f32 div);     /* dst = src / div */
extern void Vec3_Copy(Vec4 *dst, Vec4 *src);              /* copies x, y, z */
#define Vec4_ToInt ((void (*)(EftTIVec *dst, Vec4 *src))Vec4_ToInt)          /* float vector to integer vector */
extern s32 Mtx_ProjectPoint(EftTIVec *out, Mtx44 *m, Vec4 *pos); /* projects one point */
#define Vec3_Add4 ((void (*)(Vec4 *dst, Vec4 *a, Mtx44 *m, Vec4 *b, Vec4 *c))Vec3_Add4)

extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern s32 BtlScene_IsCharInView(s32 objId);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 type);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 type);
#define BtlTask_CreateChildList ((void *(*)(EftTTask *task, s32 count, s32 workSize))BtlTask_CreateChildList)
#define BtlTaskList_AddTail ((EftTTask *(*)(void *list, void *cls, void *arg))BtlTaskList_AddTail)
#define BtlTask_SetDead ((void (*)(EftTTask *task))BtlTask_SetDead)                    /* kills the task */
extern void BtlTask_SetOwnerTag(EftTTask *task, s32 flags);         /* ors into the task flags */
extern void EftTexSet_Keep32(void *tex, s32 a1, s32 a2);         /* steps a texture set's animation */
extern void EftTexSet_Load32(void *tex, s32 *entry);             /* builds a texture set from a pack entry */

extern s32 EftCam_IsActive(void);
#ifdef PORT
extern s32 EftCam_IsFighterActive(s32 objId);
#else
#define EftCam_IsFighterActive(objId) EftCam_IsActive()
#endif
extern f32 EftMath_WrapAngle(f32 angle);
extern EftTVec *EftGfx_GetClipPlanes(void);
extern void EftPrim_DrawTriangle(EftTIVec *p0, EftTIVec *p1, EftTIVec *p2, EftTVec *c0, EftTVec *c1, EftTVec *c2,
                                 EftTVec *st0, EftTVec *st1, EftTVec *st2, s32 a9, s32 a10, s32 blend, s32 z,
                                 u64 tex0);

extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern s32 BtlCharApi_IsInTechnique(s32 objId);
extern s32 BtlCharApi_IsInClashA(s32 objId);

#define EftChain_SetTex ((void (*)(EftChainWork *w, s32 a1, s32 a2, s32 a3))EftChain_SetTex)
#define EftChain_DrawStrands ((void (*)(EftTTask *task))EftChain_DrawStrands)                    /* draws the strands */
#define EftChain_Update ((void (*)(EftTTask *task))EftChain_Update)                    /* update callback of the chain task class */

extern void *D_002C3E48[6]; /* chain task class */
extern void *gEftRayClass[6]; /* ray task class */

#ifdef PORT /* assigned to: a cast is not an lvalue for a modern compiler */
#define gEftChain (*(EftChainMgr **)&gEftChain)
#else
#define gEftChain ((EftChainMgr *)gEftChain)
#endif
extern void *gEftChainList;
extern EftRayMgr *gEftRay;

void EftRay_Update(EftTTask *task);
EftTTask *EftRay_StartHit(s32 objId, EftTVec pos, s32 delay, s32 fade, s32 unused, f32 life);
EftTTask *EftRay_CreateByValue(EftRayArg arg);
void EftRay_StepTexture(void);
void EftRay_Setup(EftRayWork *w, EftRayArg *arg);
void EftRay_DrawRays(EftRayWork *w);
void EftRay_DrawQuad3D(Vec4 *corner, u8 r0, u8 g0, u8 b0, u8 a0, u8 r1, u8 g1, u8 b1, u8 a1, s32 blend, EftTTex *tex,
                       f32 segs);
void EftRay_DrawQuad2D(Vec4 *corner, s32 z, u8 r0, u8 g0, u8 b0, u8 a0, u8 r1, u8 g1, u8 b1, u8 a1, s32 blend,
                       EftTTex *tex, f32 segs);
void EftRay_DrawClipped(EftTClipVtx *poly, s32 blend, u64 tex0);

/* ---- chain module ----------------------------------------------------------------------------------------- */

/* Post-update callback of the chain task class: nothing. */
void EftChain_PostUpdate(EftTTask *task) {
}

/* Reset callback: the effect does not survive a scene reset. */
void EftChain_Reset(EftTTask *task) {
    BtlTask_SetDead(task);
}

/* Draw callback: draws the strands unless the effect is hidden or belongs to the other view. */
void EftChain_Draw(EftTTask *task) {
    EftChainWork *w = task->work;

    if (BtlScene_IsEffectHidden(w->arg.objId, w->type)) {
        return;
    }
    if (w->flags & EFT_CHAIN_VIEW_ONLY) {
        if (!BtlScene_IsCharInView(w->arg.objId)) {
            return;
        }
    }
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->screen);
    EftChain_DrawStrands(task);
    Vu0Cur_Pop();
}

/* Init callback of the manager class 0x2C3E30: the link buffer and a list of 10 tasks. */
void EftChainMgr_Init(EftTTask *task) {
    gEftChain = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftChainMgr));
    memset(gEftChain, 0, sizeof(EftChainMgr));
    gEftChain->buf = BtlPool_Alloc(BtlPool_GetCurrent(), 0xBB80);
    memset(gEftChain->buf, 0, 0xBB80);
    gEftChainList = BtlTask_CreateChildList(task, 10, sizeof(EftChainWork));
}

/* Term callback of the manager class. */
void EftChainMgr_Term(EftTTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftChain->buf);
    BtlPool_Free(BtlPool_GetCurrent(), gEftChain);
    gEftChain = NULL;
}

/* Update callback of the manager class: nothing. */
void EftChainMgr_Update(EftTTask *task) {
}

/* Reset callback of the manager class: nothing. */
void EftChainMgr_Reset(EftTTask *task) {
}

/* Starts a chain effect; returns its task (the handle of the functions below). */
EftTTask *EftChain_Create(EftChainArg arg) {
    return BtlTaskList_AddTail(gEftChainList, D_002C3E48, &arg);
}

/* The strand at 0x140 past a walking pointer: the original steps a pointer from the work's start. */
#define EFT_CHAIN_STRAND(p) ((EftChainStrand *)((u8 *)(p) + 0x140))
#define IS_CHAIN(task) ((task)->cls[0] == EftChain_Update)

/* Lets the effect finish. */
s32 EftChain_Stop(EftTTask *task) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_CHAIN_STOP;
    return 1;
}

/* Ends the effect now (part kind 18's "kill" entry). */
s32 EftChain_Kill(EftTTask *task) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_CHAIN_KILL;
    return 1;
}

/* Part kind 18's "is alive" entry. */
s32 EftChain_IsAlive(EftTTask *task) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_CHAIN_ALIVE) {
        return 1;
    }
    return 0;
}

/* Moves the effect's origin; links already out stay where they are. */
s32 EftChain_SetPos(EftTTask *task, Vec4 *pos) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    Vec3_Copy((Vec4 *)&w->arg.pos, pos);
    return 1;
}

/* Moves the origin and re-places every link of every strand relative to it. */
s32 EftChain_Warp(EftTTask *task, Vec4 *pos) {
    EftChainWork *w;
    EftChainStrand *st;
    s32 i;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    Vec3_Copy((Vec4 *)&w->arg.pos, pos);
    st = (EftChainStrand *)w;
    for (i = 0; i < 16; i++, st++) {
        Mtx44 *m = &EFT_CHAIN_STRAND(st)->mtx;
        EftChainLink *link;

        if (EFT_CHAIN_STRAND(st)->flags & 1) {
            EftChainLink **pp = &EFT_CHAIN_STRAND(st)->head;

            while (*pp != NULL) {
                link = *pp;
                Vec3_Add4(&link->pos, (Vec4 *)&w->arg.pos, m, &link->unk0, &link->unk10);
                link->pos.w = 1.0f;
                pp = &link->next;
            }
        }
    }
    return 1;
}

/* Sets the direction and rebuilds the rotation from it (pitch about X, then yaw about Y). */
s32 EftChain_SetDir(EftTTask *task, Vec4 *dir) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    Vec3_Copy((Vec4 *)&w->arg.dir, dir);
    w->pitch = Mathf_Asin(-w->arg.dir.y);
    w->yaw = atan2f(w->arg.dir.x, w->arg.dir.z);
    Mtx_StoreIdentity(&w->rot);
    Mtx_RotateX(&w->rot, &w->rot, w->pitch);
    Mtx_RotateY(&w->rot, &w->rot, w->yaw);
    return 1;
}

/* Sets the size. */
s32 EftChain_SetSize(EftTTask *task, f32 size) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    w->arg.size = size;
    return 1;
}

/* Sets the part's parameter 3, clamped to 3..7. */
s32 EftChain_SetParam3(EftTTask *task, s32 v) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    w->unkE90 = v;
    if (v < 3) {
        w->unkE90 = 3;
    } else if (v >= 8) {
        w->unkE90 = 7;
    }
    return 1;
}

/* Sets the part's parameter 5. */
s32 EftChain_SetParam5(EftTTask *task, f32 v) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    w->unkEA0 = v;
    return 1;
}

/* Sets the part's parameter 6. */
s32 EftChain_SetParam6(EftTTask *task, f32 v) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    w->unkEA4 = v;
    return 1;
}

/* Hands three arguments to the strand setup (0x1793A8). No caller. */
/* NON-MATCHING: 2 of 22 instructions (a `bne` that comes out `bnel`, and the branch target of the "not alive" test).
   The original shape appears as soon as EftChain_SetTex is DEFINED earlier in the same file (checked in a scratch
   file with a dummy definition): this function belongs to the object that holds 0x1793A8, i.e. this file is the
   tail of the file before it and will match once the two are merged. */
s32 EftChain_SetRes(EftTTask *task, s32 a1, s32 a2, s32 a3) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    EftChain_SetTex(w, a1, a2, a3);
    return 1;
}

/* Restricts drawing to the view that shows the owner. */
s32 EftChain_SetViewOnly(EftTTask *task) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_CHAIN_VIEW_ONLY;
    return 1;
}

/* Stores the owner's effect type. */
s32 EftChain_SetType(EftTTask *task, s32 type) {
    EftChainWork *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_CHAIN(task)) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (!(w->flags & EFT_CHAIN_ALIVE)) {
        return 0;
    }
    w->type = type;
    return 1;
}

/* ---- ray burst -------------------------------------------------------------------------------------------- */

#define IS_RAY(task) ((task)->cls[0] == EftRay_Update)

/* Starts a ray burst; returns its task. */
EftTTask *EftRay_Create(EftRayArg *arg) {
    EftRayArg a;

    if (gEftRay == NULL) {
        return NULL;
    }
    a = *arg;
    return BtlTaskList_AddTail(gEftRay->list, gEftRayClass, &a);
}

/* Moves the centre, unless the fade is over. */
s32 EftRay_SetPos(EftTTask *task, EftTVec pos) {
    EftRayWork *w;

    if (gEftRay == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->flags & 1) {
        return 0;
    }
    if (!IS_RAY(task)) {
        return 0;
    }
    w = task->work;
    if ((w->flags & EFT_RAY_FADED) || (w->flags & EFT_RAY_DEAD)) {
        return 0;
    }
    Vec4_Copy((Vec4 *)&w->pos, (Vec4 *)&pos);
    return 1;
}

/* Starts the fade and lets the task end itself after it (part kind 0's "kill" entry). */
s32 EftRay_Kill(EftTTask *task) {
    EftRayWork *w;

    if (gEftRay == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->flags & 1) {
        return 0;
    }
    if (!IS_RAY(task)) {
        return 0;
    }
    w = task->work;
    w->flags |= EFT_RAY_FADING | EFT_RAY_AUTOKILL;
    return 1;
}

/* Stores the owner's effect type. */
s32 EftRay_SetType(EftTTask *task, s32 type) {
    EftRayWork *w;

    if (gEftRay == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->flags & 1) {
        return 0;
    }
    if (!IS_RAY(task)) {
        return 0;
    }
    w = task->work;
    w->type = type;
    return 1;
}

/* Part kind 0's "is alive" entry. A burst whose fade is over is reported dead once and then ends itself. */
s32 EftRay_IsAlive(EftTTask *task) {
    EftRayWork *w;

    if (gEftRay == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->flags & 1) {
        return 0;
    }
    if (!IS_RAY(task)) {
        return 0;
    }
    w = task->work;
    if (w->flags & EFT_RAY_FADED) {
        w->flags |= EFT_RAY_DEAD;
        return 0;
    }
    return 1;
}

/* The hit burst with its default timing: no delay, a 10-frame fade after 0.5 s. No caller. */
EftTTask *EftRay_StartHitDefault(s32 objId, EftTVec pos) {
    return EftRay_StartHit(objId, pos, 0, 10, 0, 0.5f);
}

/* Fighter effect request 0x18: 36 pale blue rays in the world at pos, re-rolled every frame. */
EftTTask *EftRay_StartHit(s32 objId, EftTVec pos, s32 delay, s32 fade, s32 unused, f32 life) {
    EftRayArg arg;

    Vec4_Copy((Vec4 *)&arg.pos, (Vec4 *)&pos);
    arg.color[0] = 0xC0;
    arg.delay = (f32)delay;
    arg.fadeFrames = (f32)fade;
    arg.color[1] = 0xC0;
    arg.color[2] = 0xC0;
    arg.color[3] = 0x70;
    arg.length = 60.0f;
    arg.width = 0.3f;
    arg.inner = -1.2f;
    arg.life = life;
    arg.objId = objId;
    arg.jitter = 0.3f;
    arg.mode = EFT_RAY_MODE_SPIN;
    arg.count = 36;
    arg.blend = 1;
    arg.space = 0;
    arg.autoKill = 1;
    arg.pos.x = pos.x;
    arg.pos.y = pos.y;
    arg.pos.z = pos.z;
    arg.pos.w = pos.w;
    return EftRay_Create(&arg);
}

/* A screen-filling burst at the fighter (node 3) that lasts until a technique camera cut starts. No caller. */
EftTTask *EftRay_StartScreen(s32 objId, s32 delay, s32 fade, f32 life) {
    EftRayArg arg;

    Vec4_Set((Vec4 *)&arg.pos, 0.0f, 0.0f, 0.0f, 1.0f);
    arg.color[0] = 0xC0;
    arg.color[1] = 0xC0;
    arg.color[2] = 0xC0;
    arg.color[3] = 0x80;
    arg.life = life;
    arg.length = 800.0f;
    arg.width = 1.5f;
    arg.inner = 0.0f;
    arg.jitter = 0.0f;
    arg.mode = EFT_RAY_MODE_CUT;
    arg.count = 36;
    arg.objId = objId;
    arg.blend = 1;
    arg.space = 1;
    arg.delay = (f32)delay;
    arg.fadeFrames = (f32)fade;
    arg.autoKill = 1;
    return EftRay_CreateByValue(arg);
}

/* EftRay_Create with the argument block by value (fighter effect request 0x15, effect pack kind 0 variant 1). */
EftTTask *EftRay_CreateByValue(EftRayArg arg) {
    return EftRay_Create(&arg);
}

/* Init callback of the manager class 0x2C3E60: the texture and a list of 4 tasks. */
void EftRayMgr_Init(EftTTask *task) {
    gEftRay = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftRayMgr));
    memset(gEftRay, 0, sizeof(EftRayMgr));
    EftTexSet_Load32(gEftRay, BtlScene_GetCommonEntry(10));
    gEftRay->list = BtlTask_CreateChildList(task, 4, sizeof(EftRayWork));
}

/* Term callback of the manager class. */
void EftRayMgr_Term(EftTTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftRay);
    gEftRay = NULL;
}

/* Update callback of the manager class: steps the texture animation while a burst exists. */
void EftRayMgr_Update(EftTTask *task) {
    if (task->children->count != 0) {
        EftRay_StepTexture();
    }
}

/* Init callback of the task class 0x2C3E78. A burst owned by character 0 / 1 is tagged as such. */
void EftRay_Init(EftTTask *task, EftRayArg *arg) {
    EftRayWork *w = task->work;

    memset(w, 0, sizeof(EftRayWork));
    w->type = 2;
    EftRay_Setup(w, arg);
    if ((u32)arg->objId < (u32)BtlScene_GetCharCount()) {
        BtlTask_SetOwnerTag(task, BtlScene_CharOwnerTag(arg->objId));
    }
}

/* Term callback. */
void EftRay_Term(EftTTask *task) {
    memset(task->work, 0, sizeof(EftRayWork));
}

/* Gives every ray a new alpha and turns the whole burst by a random angle. */
#define EFT_RAY_REROLL(w)                                                             \
    turn = EftMath_WrapAngle((f32)(rand() % 360) * 3.14159265f / 180.0f);             \
    ray = (w)->ray;                                                                   \
    for (i = 0; i < (w)->count; i++, ray++) {                                         \
        ray->a = rand() % (w)->alphaRange;                                            \
        ray->angle = EftMath_WrapAngle(ray->angle + turn);                            \
    }

/* Update callback: delay, life, fade, then the mode's animation. Not stepped while its owner's effects are
   stopped. */
void EftRay_Update(EftTTask *task) {
    EftRayWork *w = task->work;
    EftRay *ray;
    f32 turn;
    s32 i;

    if (BtlScene_IsEffectStopped(w->objId, w->type)) {
        return;
    }
    if (w->delay > 0) {
        w->delay--;
        return;
    }
    if (w->flags & EFT_RAY_TIMED) {
        w->life -= 1.0f / 30.0f;
        if (w->life < 0.0f) {
            w->life = 0.0f;
            w->flags |= EFT_RAY_FADING;
        }
    }
    if (w->flags & EFT_RAY_FADING) {
        w->alpha -= w->alphaStep;
        if (w->alpha < 0.0f) {
            w->alpha = 0.0f;
            w->flags |= EFT_RAY_FADED;
            if (w->flags & EFT_RAY_AUTOKILL) {
                w->flags |= EFT_RAY_DEAD;
            }
        }
    }
    if (w->flags & EFT_RAY_DEAD) {
        BtlTask_SetDead(task);
        return;
    }
    switch (w->mode) {
        case EFT_RAY_MODE_SHRINK:
            w->scale.x -= w->shrink;
            if (w->scale.x < 0.0f) {
                w->scale.x = 0.0f;
            }
            break;
        case EFT_RAY_MODE_SPIN:
            EFT_RAY_REROLL(w);
            break;
        case EFT_RAY_MODE_TECH: {
            s32 n = BtlCharApi_IsInTechnique(w->objId);

            if (BtlCharApi_GetOpponentObjId(w->objId) >= 0) {
                n += BtlCharApi_IsInTechnique(BtlCharApi_GetOpponentObjId(w->objId));
            }
            if (n != 0) {
                w->life = -1.0f;
                w->alpha = -1.0f;
            }
            EFT_RAY_REROLL(w);
            break;
        }
        case EFT_RAY_MODE_CUT:
            if (EftCam_IsFighterActive(w->objId)) {
                w->life = -1.0f;
                w->alpha = -1.0f;
            }
            EFT_RAY_REROLL(w);
            break;
    }
}

/* Reset callback: the burst does not survive a scene reset. */
void EftRay_Reset(EftTTask *task) {
    BtlTask_SetDead(task);
}

/* Draw callback. */
void EftRay_Draw(EftTTask *task) {
    EftRayWork *w = task->work;

    if (!BtlScene_IsEffectHidden(w->objId, w->type)) {
        EftRay_DrawRays(w);
    }
}

/* Steps the animation of the ray texture. */
void EftRay_StepTexture(void) {
    EftTexSet_Keep32(gEftRay, 1, 0);
}

/* Fills the work from the create argument and rolls the rays: evenly spread angles with up to 4 degrees of
   noise, alpha, length, width and offset at random. */
void EftRay_Setup(EftRayWork *w, EftRayArg *arg) {
    EftRay *ray;
    f32 step;
    s32 i;

    Vec4_Copy((Vec4 *)&w->pos, (Vec4 *)&arg->pos);
    w->pos.w = 1.0f;
    Mtx_StoreIdentity(&w->mtx);
    Vec4_Set((Vec4 *)&w->scale, 1.0f, 1.0f, 1.0f, 1.0f);
    w->life = arg->life;
    w->shrink = 1.0f / (arg->life * 0.9f * 30.0f);
    w->alphaRange = arg->color[3];
    w->alpha = 1.0f;
    w->alphaStep = 1.0f / (f32)arg->fadeFrames;
    w->mode = arg->mode;
    w->inner = arg->inner;
    w->jitter = arg->jitter;
    w->count = arg->count;
    w->blend = arg->blend;
    w->space = arg->space;
    w->objId = arg->objId;
    w->delay = arg->delay;
    if (0.0f < w->life) {
        w->flags |= EFT_RAY_TIMED;
    }
    if (arg->autoKill != 0) {
        w->flags |= EFT_RAY_AUTOKILL;
    }
    ray = w->ray;
    step = 360.0f / (f32)arg->count;
    for (i = 0; i < arg->count; i++, ray++) {
        f32 angle = EftMath_WrapAngle(step * (f32)i * 3.14159265f / 180.0f + (f32)(rand() % 5) * 3.14159265f / 180.0f);

        ray->r = arg->color[0];
        ray->g = arg->color[1];
        ray->b = arg->color[2];
        ray->a = arg->color[3] - rand() % 16;
        ray->angle = angle;
        ray->length = arg->length + (f32)rand() / 2147483647.0f * 20.0f;
        ray->width = arg->width * ((f32)rand() / 2147483647.0f * 5.0f);
        if (ray->width < 0.4f) {
            ray->width = 0.4f;
        }
        if (w->space == 1) {
            ray->width = arg->width + (f32)rand() / 2147483647.0f * 4.0f;
        }
        ray->offset = (f32)rand() / 2147483647.0f * w->jitter;
    }
}

/* Draws every ray: a quad from the near end (inner + offset from the centre) outwards, turned by the ray's angle
   about the view axis. World bursts are hidden unless the owner's view is drawn (or a camera cut runs) and
   while the owner is in a beam clash; screen bursts of modes 2 / 3 centre on the opponent's node 3. */
void EftRay_DrawRays(EftRayWork *w) {
    Vec4 off;
    Vec4 corner[4];
    Mtx44 m;
    EftTVec base[4] = { { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, -1.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f },
                     { 1.0f, 0.0f, 0.0f, 1.0f } };
    Vec4 node;
    EftTIVec scr;
    Vec4 v;
    Vec4 v2;
    EftRay *ray;
    Vec4 *c;
    EftTVec *bp;
    s32 i;
    s32 j;

    if (w->objId >= 0) {
        if (!EftCam_IsActive()) {
            if (!BtlScene_IsCharInView(w->objId)) {
                return;
            }
        }
        if (BtlCharApi_IsInClashA(w->objId)) {
            return;
        }
    }
    switch (w->space) {
        case 0:
            Mtx_StoreIdentity(&m);
            Mtx_StoreIdentity(&w->mtx);
            Mtx_InverseRT(&m, &gBtlCamView->view);
            m.m[3][0] = 0.0f;
            m.m[3][1] = 0.0f;
            m.m[3][2] = 0.0f;
            m.m[3][3] = 1.0f;
            Mtx_Mul(&w->mtx, &w->mtx, &m);
            Vec4_Copy((Vec4 *)w->mtx.m[3], (Vec4 *)&w->pos);
            break;
        case 1:
            if (w->mode == EFT_RAY_MODE_TECH || w->mode == EFT_RAY_MODE_CUT) {
                BtlCharApi_GetNodePos(BtlCharApi_GetOpponentObjId(w->objId), 3, &node);
                Mtx_ProjectPoint(&scr, &gBtlCamView->screen, &node);
                if (scr.z < 0) {
                    return;
                }
                w->pos.x = (scr.x - 0x7000) >> 4;
                w->pos.y = (scr.y - 0x7200) >> 4;
                w->pos.z = 0.0f;
                w->pos.w = 1.0f;
            }
            Mtx_StoreIdentity(&w->mtx);
            Vec4_Copy((Vec4 *)w->mtx.m[3], (Vec4 *)&w->pos);
            break;
    }
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->screen);
    ray = w->ray;
    for (i = 0; i < w->count; i++, ray++) {
        for (j = 0, bp = base, c = corner; j < 4; ) {
            v.x = bp->x * ray->width * w->scale.x;
            v.y = bp->y * ray->length * w->scale.y;
            v.z = bp->z;
            v.w = bp->w;
            Vec4_Set(&off, 0.0f, -(w->inner + ray->offset), 0.0f, 0.0f);
            Vec4_Add(&v, &v, &off);
            Mtx_StoreIdentity(&m);
            Mtx_RotateZ(&m, &m, ray->angle);
            Mtx_MulVec4(&v2, &m, &v);
            Mtx_MulVec4(c, &w->mtx, &v2);
            j++;
            bp++;
            c++;
            if (j >= 4) {
                break;
            }
        }
        if (w->space == 0) {
            EftRay_DrawQuad3D(corner, ray->r, ray->g, ray->b, (u32)(ray->a * w->alpha), ray->r, ray->g, ray->b,
                              (u32)(ray->a * w->alpha), w->blend, gEftRay->tex, 4.0f);
        } else {
            EftRay_DrawQuad2D(corner, 0, ray->r, ray->g, ray->b, (u32)(ray->a * w->alpha), ray->r, ray->g, ray->b,
                              (u32)(ray->a * w->alpha), w->blend, gEftRay->tex, 4.0f);
        }
    }
    Vu0Cur_Pop();
}

/* A quad in the world: two triangles, each clipped against the view and queued by depth. */
void EftRay_DrawQuad3D(Vec4 *corner, u8 r0, u8 g0, u8 b0, u8 a0, u8 r1, u8 g1, u8 b1, u8 a1, s32 blend, EftTTex *tex,
                       f32 segs) {
    EftTClipVtx poly[9];
    Vec4 st[4];
    Vec4 col[4];

    Vec4_Set(&st[0], 0.0f, 1.0f, 1.0f, 1.0f);
    Vec4_Set(&st[1], 1.0f, 1.0f, 1.0f, 1.0f);
    Vec4_Set(&st[2], 0.0f, 0.0f, 1.0f, 1.0f);
    Vec4_Set(&st[3], 1.0f, 0.0f, 1.0f, 1.0f);
    Vec4_Set(&col[0], r0, g0, b0, a0);
    Vec4_Set(&col[1], r0, g0, b0, a0);
    Vec4_Set(&col[2], r1, g1, b1, a1);
    Vec4_Set(&col[3], r1, g1, b1, a1);
    ClipVtx_SetArray(poly, &corner[0], &st[0], &col[0], 3);
    EftRay_DrawClipped(poly, blend, tex->tex0);
    ClipVtx_SetArray(poly, &corner[1], &st[1], &col[1], 3);
    EftRay_DrawClipped(poly, blend, tex->tex0);
}

/* Links a packet into the chain of a depth slot (clamped to 0..0xFFF); layers 2 and 3 are 0 and 1. */
static inline void EftTOt_Add(OtPrim *p, s32 z, s32 layer) {
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

/* A quad on the screen (corners in pixels), cut into `segs` strips along its length with the alpha running from
   a0 to a1; queued at depth slot z with the GS depth at the far limit. */
void EftRay_DrawQuad2D(Vec4 *corner, s32 z, u8 r0, u8 g0, u8 b0, u8 a0, u8 r1, u8 g1, u8 b1, u8 a1, s32 blend,
                       EftTTex *tex, f32 segs) {
    Vec4 tv[4];
    Vec4 p[4];
    Vec4 d[2];
    EftTIVec scr[4];
    EftTVec uv[4] = { { 0.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },
                   { 1.0f, 0.0f, 1.0f, 1.0f } };
    s32 aStep;
    f32 t = 1.0f / segs;
    f32 inv = t;
    f32 vStep0;
    f32 vStep1;
    s32 aNext;
    s32 aCur;
    EftTQuadPkt *q;

    Vec4_Sub(&d[0], &corner[2], &corner[0]);
    Vec4_Sub(&d[1], &corner[3], &corner[1]);
    Vec3_Div(&d[0], &d[0], segs);
    Vec3_Div(&d[1], &d[1], segs);
    Vec4_Copy(&p[0], &corner[0]);
    Vec4_Copy(&p[1], &corner[1]);
    tv[0].y = uv[0].y;
    tv[1].y = uv[1].y;
    vStep0 = (uv[2].y - uv[0].y) / segs;
    vStep1 = (uv[3].y - uv[1].y) / segs;
    aStep = (f32)(a1 - a0) / segs;
    p[3].w = 1.0f;
    p[2].w = 1.0f;
    p[1].w = 1.0f;
    p[0].w = 1.0f;
    for (aNext = a0, aCur = aNext; !(1.0f < t); t += inv) {
        Vec3_Add(&p[2], &p[0], &d[0]);
        Vec3_Add(&p[3], &p[1], &d[1]);
        aNext += aStep;
        tv[2].y = tv[0].y + vStep0;
        tv[3].y = tv[1].y + vStep1;
        Vec4_ToInt(&scr[0], &p[0]);
        Vec4_ToInt(&scr[1], &p[1]);
        Vec4_ToInt(&scr[2], &p[2]);
        Vec4_ToInt(&scr[3], &p[3]);
        q = (EftTQuadPkt *)gOtCurRead;
        gOtCur = (u32 *)(q + 1);
        q->prim = 0x5C;
        q->tag = 0x20000008;
        q->vif0 = 0x10000000;
        q->vif1 = 0x50000008;
        q->gif0 = 0xE400000000008001;
        q->gif1 = 0x42142142142160;
        q->next = 0;
        q->v[0].rgba[0] = r0;
        q->v[0].rgba[1] = g0;
        q->v[0].rgba[2] = b0;
        q->v[0].rgba[3] = aCur;
        q->v[0].q = 1.0f;
        q->v[1].rgba[0] = r0;
        q->v[1].rgba[1] = g0;
        q->v[1].rgba[2] = b0;
        q->v[1].rgba[3] = aCur;
        q->v[1].q = 1.0f;
        q->v[2].rgba[0] = r1;
        q->v[2].rgba[1] = g1;
        q->v[2].rgba[2] = b1;
        q->v[2].rgba[3] = aNext;
        q->v[2].q = 1.0f;
        q->v[3].rgba[0] = r1;
        q->v[3].rgba[1] = g1;
        q->v[3].rgba[2] = b1;
        q->v[3].rgba[3] = aNext;
        q->v[3].q = 1.0f;
        q->v[0].s = uv[0].x;
        q->v[0].t = tv[0].y;
        q->v[1].s = uv[1].x;
        q->v[1].t = tv[1].y;
        q->v[2].s = uv[2].x;
        q->v[2].t = tv[2].y;
        q->v[3].s = uv[3].x;
        q->v[3].t = tv[3].y;
        q->v[0].xyz.x = (u16)scr[0].x * 16 + 0x7000;
        q->v[0].xyz.y = (u16)scr[0].y * 16 + 0x7200;
        q->v[0].xyz.z = 0xFFFFFF;
        q->v[0].xyz.f = 0xFF;
        q->v[1].xyz.x = (u16)scr[1].x * 16 + 0x7000;
        q->v[1].xyz.y = (u16)scr[1].y * 16 + 0x7200;
        q->v[1].xyz.z = 0xFFFFFF;
        q->v[1].xyz.f = 0xFF;
        q->v[2].xyz.x = (u16)scr[2].x * 16 + 0x7000;
        q->v[2].xyz.y = (u16)scr[2].y * 16 + 0x7200;
        q->v[2].xyz.z = 0xFFFFFF;
        q->v[2].xyz.f = 0xFF;
        q->v[3].xyz.x = (u16)scr[3].x * 16 + 0x7000;
        q->v[3].xyz.y = (u16)scr[3].y * 16 + 0x7200;
        q->v[3].xyz.z = 0xFFFFFF;
        q->v[3].xyz.f = 0xFF;
        q->tex0 = tex->tex0;
        EftTOt_Add((OtPrim *)q, z, blend);
        Vec4_Copy(&p[0], &p[2]);
        aCur = aNext;
        Vec4_Copy(&p[1], &p[3]);
        tv[0].y = tv[2].y;
        tv[1].y = tv[3].y;
    }
}

/* Clips a triangle against the five planes of the view, projects what is left and queues it as a fan with every
   GS depth at the far limit, sorted by the average depth. */
void EftRay_DrawClipped(EftTClipVtx *poly, s32 blend, u64 tex0) {
#ifdef PORT
    /* The triangle is clipped in place and can grow to eight corners, but callers hand over room for three. On
       the PS2 the extra corners land in the caller's other locals; on PC they reach the return address (the
       crash in EftRibbon_DrawStrip). The work is done on a copy with room. */
    EftTClipVtx portPoly[16];
    __builtin_memcpy(portPoly, poly, 3 * sizeof(EftTClipVtx));
    poly = portPoly;
#endif
    EftTIVec scr[9];
    EftTVec st[9];
    EftTVec *plane;
    s32 n = 3;
    s32 i;
    s32 z;

    plane = EftGfx_GetClipPlanes();
    for (i = 0; i < 5; i++) {
        n = ClipPoly_ClipPlane(poly, plane, n);
        plane++;
    }
    if (n != 0) {
        ClipPoly_ProjectCur(scr, st, poly, n);
        for (i = 2; i < n; i++) {
            z = (scr[0].z + scr[i - 1].z + scr[i].z) / 3;
            scr[0].z = 0xFFFFFF;
            scr[i - 1].z = 0xFFFFFF;
            scr[i].z = 0xFFFFFF;
            EftPrim_DrawTriangle(&scr[0], &scr[i - 1], &scr[i], &poly[0].col, &poly[i - 1].col, &poly[i].col, &st[0],
                                 &st[i - 1], &st[i], 0, 0, blend, z >> 8, tex0);
        }
    }
}
