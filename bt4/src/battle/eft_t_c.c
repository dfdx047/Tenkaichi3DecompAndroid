#include "common.h"
#include "battle/eft_t.h"

/*
 * 0x1809C0..0x182CE8: the module of task class 0x2C3EC0 (EftShotFx_*), the teleport ("vanish") lines: black
 * vertical streaks on a fighter's body and in a column around it. Written as two files and merged at
 * integration (one object in the original: its .rodata is tables, jump table, constants in that order): this
 * first part is the head, 0x1809C0..0x180BF8; the second part (formerly eft_u.c, 0x180BF8..0x182CE8) has the
 * other callbacks and all of the drawing (include/battle/eft_u.h has the full work layout).
 * Here: the request entry the fighter effect layer calls (requests 0xC..0xF give kinds 0..3, and object
 * animation event bit 42 gives kind 0), its slot finder, and the class's init callback.
 * Drawing only: a request is an object id and two frame counters.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern void EftTexSet_Load4(EftTTex *tex, s32 *entry); /* builds a texture set from a pack entry */

extern EftTShotFxMgr *gEftShotFx;

/* The module's two file-scope tables (0x2ECEC0 body segments, 0x2ED0E8 column slices; layouts in
 * include/battle/eft_u.h). They come first in this object's .rodata, in front of the jump table of
 * EftShotFx_Request (0x2ED180), so they belong to this file. The second part reads them through non-const
 * `extern` declarations (as const data defined here its loads change), so they are emitted from the original. */
INCLUDE_RODATA("asm/nonmatchings/battle/eft_t_c", gEftShotFxSegs);
INCLUDE_RODATA("asm/nonmatchings/battle/eft_t_c", gEftShotFxRows);

/* Returns a free request slot (flags == 0) of the two, or NULL. */
EftTShotFxReq *EftShotFx_FindFree(void) {
    EftTShotFxReq *req = gEftShotFx->req;
    s32 i;

    for (i = 0; i < 2; i++, req++) {
        if (req->flags == 0) {
            return req;
        }
    }
    return NULL;
}

/* Starts the lines on a fighter. Kinds 0 / 1 start sequence A / B (limb lines plus a column, flags 0x21 /
   0x12), kinds 2 / 3 the same with the burst bit 0x80, kind 4 claims a slot and clears it at once. Returns 0
   only when the module is not loaded or both slots are busy. */
s32 EftShotFx_Start(s32 objId, u8 kind) {
    EftTShotFxReq *req;

    if (gEftShotFx == NULL) {
        return 0;
    }
    req = EftShotFx_FindFree();
    if (req == NULL) {
        return 0;
    }
    switch (kind) {
        case 0:
            req->objId = objId;
            req->frameA = 0;
            req->flags |= 0x21;
            break;
        case 2:
            req->objId = objId;
            req->frameA = 0;
            req->flags |= 0xA1;
            break;
        case 1:
            req->objId = objId;
            req->frameB = 0;
            req->flags |= 0x12;
            break;
        case 3:
            req->objId = objId;
            req->frameB = 0;
            req->flags |= 0x92;
            break;
        case 4:
            req->objId = objId;
            req->flags = 0;
            break;
    }
    return 1;
}

/* Init callback of the task class 0x2C3EC0: the work, two request slots, 200 lines and the texture set. Each
   block is only allocated when missing. */
void EftShotFxMgr_Init(EftTTask *task) {
    EftTTex *tex;

    if (gEftShotFx == NULL) {
        EftTShotFxMgr *w = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftTShotFxMgr));

        gEftShotFx = w;
        memset(w, 0, sizeof(EftTShotFxMgr));
    }
    if (gEftShotFx->req == NULL) {
        gEftShotFx->req = BtlPool_Alloc(BtlPool_GetCurrent(), 2 * sizeof(EftTShotFxReq));
        memset(gEftShotFx->req, 0, 2 * sizeof(EftTShotFxReq));
    }
    if (gEftShotFx->lines == NULL) {
        gEftShotFx->lines = BtlPool_Alloc(BtlPool_GetCurrent(), 0x4B00);
        memset(gEftShotFx->lines, 0, 0x4B00);
    }
    if (gEftShotFx->tex == NULL) {
        gEftShotFx->tex = BtlPool_Alloc(BtlPool_GetCurrent(), 0x48);
        memset(gEftShotFx->tex, 0, 0x48);
    }
    tex = gEftShotFx->tex;
    EftTexSet_Load4(tex, BtlScene_GetCommonEntry(0x237));
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly eft_u.c), with its own header and view types. Names the first part already declared with
 * other types are reached through cast macros (the generated code is the same).
 * ------------------------------------------------------------------------------------------------------------ */
#include "battle/eft_u.h"
#include "sys/gfx_ot.h"

/*
 * Vanish lines (module EftShotFx), 0x180BF8..0x182CE8: the rest of the module that starts at 0x1809C0 in
 * the first part. See include/battle/eft_u.h. The black vertical streaks of a teleport: on the body and in a column
 * around it, for fighter effect requests 0xC..0xF.
 * Drawing only: reads fighters through BtlCharApi_GetHeight / BtlCharApi_GetNodePos, writes nothing outside its
 * own pool and the display list. Random values come from libc rand().
 */

#define RAND_MAX_F 2147483647.0f

typedef struct EftUBattleWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags; /* 0x100 = paused */
} EftUBattleWork;

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftUView {
    /* 0x000 */ Mtx44 world2view;
    /* 0x040 */ Mtx44 world2view2;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 world2screen;
} EftUView;

/* GS XYZF2 register value. */
typedef struct EftUXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftUXyzf;

typedef struct EftUGsVtx {
    /* 0x00 */ u8 rgba[4];
    /* 0x04 */ f32 q;
    /* 0x08 */ f32 s;
    /* 0x0C */ f32 t;
    /* 0x10 */ EftUXyzf xyz;
} EftUGsVtx; /* 0x18: RGBAQ, ST, XYZF2 */

/* Triangle strip of four full vertices. 0x90 bytes. */
typedef struct EftUQuadPkt {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftUGsVtx v[4];
} EftUQuadPkt;

#define V(p) ((Vec4 *)(p))

/* The "up" vector constant of three functions. The original object has three copies of it in .rodata (0x2ED1A0,
   0x2ED1B0, 0x2ED1C0); this compiler merges identical initialisers of one type, so each function gets a type of
   its own to keep them apart. */
typedef struct EftUUpA {
    f32 x, y, z, w;
} __attribute__((aligned(16))) EftUUpA;
typedef struct EftUUpB {
    f32 x, y, z, w;
} __attribute__((aligned(16))) EftUUpB;
typedef struct EftUUpC {
    f32 x, y, z, w;
} __attribute__((aligned(16))) EftUUpC;

extern EftUView *gBtlCamView;
#ifdef PORT /* assigned to: a cast is not an lvalue for a modern compiler */
#define gEftShotFx (*(EftShotFxWork **)&gEftShotFx)
#else
#define gEftShotFx ((EftShotFxWork *)gEftShotFx)
#endif
extern EftShotFxSeg gEftShotFxSegs[23];
extern EftShotFxRow gEftShotFxRows[6];

extern s32 rand(void);
extern void *memset(void *dst, s32 c, u32 n);
extern f32 Mathf_SinFast(f32 angle);
extern f32 Mathf_CosFast(f32 angle);
extern f32 EftMath_WrapAngle(f32 angle);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern f32 Vec3_Length(Vec4 *v);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);          /* copy */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);          /* inverse of a rotation + translation matrix */
extern void Vu0Cur_Push(void);                            /* VU0 matrix stack push */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                        /* load the matrix */
extern void Vu0Cur_Pop(void);                            /* pop */
extern void Vu0Cur_ProjectPointsStq(EftUIVec *xyz, Vec4 *stq, Vec4 *pos, Vec4 *uv, s32 n); /* projects with the loaded matrix */
extern void EftTexSet_Keep4(EftShotFxTex *tex, s32 a, s32 b);
extern EftUBattleWork *Battle_GetWork(void);
extern s32 BtlPool_GetCurrent(void);
extern void BtlPool_Free(s32 slot, void *ptr);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);

/* Task term: frees the textures, the request slots, the pool and the work. */
void EftShotFx_Term(void) {
    if (gEftShotFx->tex != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftShotFx->tex);
        gEftShotFx->tex = NULL;
    }
    if (gEftShotFx->req != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftShotFx->req);
        gEftShotFx->req = NULL;
    }
    if (gEftShotFx->lines != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftShotFx->lines);
        gEftShotFx->lines = NULL;
    }
    if (gEftShotFx != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftShotFx);
        gEftShotFx = NULL;
    }
}

/* Task update: the request slots, then the lines. */
void EftShotFx_Update(void) {
    EftShotFx_UpdateReqs();
    EftShotFx_UpdateLines();
}

/* Task post-update: keeps the textures referenced. */
void EftShotFx_PostUpdate(void) {
    EftShotFx_UpdateTexture(1, 0);
}

/* Task draw: takes the camera rotation of the view being drawn, then draws the lines under its world-to-screen matrix. */
void EftShotFx_Draw(void) {
    Mtx_InverseRT(&gEftShotFx->camRot, &gBtlCamView->world2view2);
    gEftShotFx->camRot.m[3][0] = gEftShotFx->camRot.m[3][1] = gEftShotFx->camRot.m[3][2] = 0.0f;
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    EftShotFx_DrawLines();
    Vu0Cur_Pop();
}

/* Returns a zeroed free line, or NULL when all 200 are in use. */
EftShotFxLine *EftShotFx_Alloc(void) {
    EftShotFxLine *p = gEftShotFx->lines;
    s32 i;

    for (i = 0; i < 200; i++, p++) {
        if (p->flags == 0) {
            memset(p, 0, sizeof(EftShotFxLine));
            return p;
        }
    }
    return NULL;
}

/* Adds a line at the list tail. `life` is in seconds; the alpha ramp (flags 0x10 / 0x20) spans the whole life. */
void EftShotFx_Add(EftUVec pos, EftUVec off, f32 toward, f32 len, f32 growth, f32 accelDiv, f32 width, f32 alpha,
                     f32 life, u8 tex, u8 blend, s32 flags, s32 delay) {
    EftShotFxLine *p = EftShotFx_Alloc();

    if (p != NULL) {
        if (gEftShotFx->head == NULL) {
            gEftShotFx->head = p;
            p->prev = NULL;
            p->next = NULL;
        } else {
            EftShotFxLine *last = gEftShotFx->tail;

            p->next = NULL;
            p->prev = last;
        }
        if (gEftShotFx->tail != NULL) {
            gEftShotFx->tail->next = p;
        }
        gEftShotFx->tail = p;
        Vec4_Copy(&p->pos, V(&pos));
        Vec4_Copy(&p->off, V(&off));
        p->toward = toward;
        p->len = len;
        p->growth = growth;
        p->accel = (accelDiv != 0.0f) ? 1.0f / accelDiv : 0.0f;
        p->width = width;
        p->tex = tex;
        p->blend = blend;
        p->delay = delay;
        p->flags = flags;
        p->life = life * 30.0f;
        if (flags & EFT_SHOTFX_LINE_FADE_IN) {
            p->alphaEnd = alpha;
            p->alpha = 0.0f;
            p->alphaStep = alpha / p->life;
        } else if (flags & EFT_SHOTFX_LINE_FADE_OUT) {
            p->alphaEnd = 0.0f;
            p->alpha = alpha;
            p->alphaStep = alpha / p->life;
        }
        gEftShotFx->count++;
    }
}

/* Steps every line (not while paused) and unlinks the finished ones. */
void EftShotFx_UpdateLines(void) {
    EftShotFxLine *p;
    EftShotFxLine *cur;
    s32 done;

    if (!(Battle_GetWork()->flags & 0x100)) {
        p = gEftShotFx->head;
        while (p != NULL) {
            cur = p;
            done = EftShotFx_Step(p);
            p = p->next;
            if (done) {
                cur->flags = 0;
                if (cur->prev == NULL) {
                    gEftShotFx->head = cur->next;
                } else {
                    cur->prev->next = cur->next;
                }
                if (cur->next == NULL) {
                    gEftShotFx->tail = cur->prev;
                } else {
                    cur->next->prev = cur->prev;
                }
                cur->prev = NULL;
                cur->next = NULL;
                gEftShotFx->count--;
            }
        }
    }
}

/* One frame of a line: start delay, life, length change, alpha ramp. Returns 1 when its life is over. */
s32 EftShotFx_Step(EftShotFxLine *p) {
    if (p->flags & EFT_SHOTFX_LINE_WAIT) {
        if (--p->delay <= 0) {
            p->flags &= ~EFT_SHOTFX_LINE_WAIT;
            p->flags |= EFT_SHOTFX_LINE_ACTIVE;
        }
    }
    if (p->flags & EFT_SHOTFX_LINE_ACTIVE) {
        p->life -= 1.0f;
        if (p->life <= 0.0f) {
            p->flags |= EFT_SHOTFX_LINE_DONE;
        }
        if (p->flags & EFT_SHOTFX_LINE_GROW) {
            p->len += p->growth;
            p->growth += p->growth * p->accel;
        } else if (p->flags & EFT_SHOTFX_LINE_SHRINK) {
            p->len -= p->growth;
            p->growth += p->growth * p->accel;
        }
        if (p->flags & EFT_SHOTFX_LINE_FADE_IN) {
            p->alpha += p->alphaStep;
            if (p->alpha >= p->alphaEnd) {
                p->alpha = p->alphaEnd;
                p->flags &= ~EFT_SHOTFX_LINE_FADE_IN;
            }
        } else if (p->flags & EFT_SHOTFX_LINE_FADE_OUT) {
            p->alpha -= p->alphaStep;
            if (p->alpha <= p->alphaEnd) {
                p->alpha = p->alphaEnd;
                p->flags &= ~EFT_SHOTFX_LINE_FADE_OUT;
            }
        }
    }
    if (p->flags & EFT_SHOTFX_LINE_DONE) {
        return 1;
    }
    return 0;
}

/* Draws every active line, black with its alpha. */
void EftShotFx_DrawLines(void) {
    EftUMtx mtx;
    Vec4 quad[4];
    EftShotFxLine *p;

    Mtx_Copy((Mtx44 *)&mtx, &gBtlCamView->world2screen);
    for (p = gEftShotFx->head; p != NULL; p = p->next) {
        if (p->flags & EFT_SHOTFX_LINE_ACTIVE) {
            EftShotFx_BuildQuad(quad, p);
            EftShotFx_DrawQuad(quad, mtx, 0, 0, 0, (u32)p->alpha, &gEftShotFx->tex[p->tex].tex0, p->blend);
        }
    }
}

/* Steps the two request slots (not while paused). A sequence runs for five updates (frame 0..4):
   kind 0 (vanish): body lines on frames 0 and 1, the growing column on frame 1;
   kind 1 (reappear): the shrinking column on frame 0, body lines on frames 2, 3 and 4;
   kinds 2 / 3: four sets of body lines at once (frame 0 / frame 2), started 0..3 frames apart, and no column. */
void EftShotFx_UpdateReqs(void) {
    EftShotFxReq *r;
    s32 i;
    s32 j;

    if (!(Battle_GetWork()->flags & 0x100)) {
        for (i = 0; i < 2; i++) {
            r = &gEftShotFx->req[i];
            if (r->flags == 0) {
                continue;
            }
            if (r->flags & EFT_SHOTFX_REQ_HOLD) {
                continue;
            }
            if (r->flags & (EFT_SHOTFX_REQ_BODY_A | EFT_SHOTFX_REQ_BODY_B)) {
                if (r->flags & EFT_SHOTFX_REQ_BURST) {
                    if (!(r->flags & EFT_SHOTFX_REQ_BURST_DONE)) {
                        for (j = 0; j < 4; j++) {
                            EftShotFx_SpawnBodyLines(r->objId, 0.1f, j);
                        }
                        r->flags |= EFT_SHOTFX_REQ_BURST_DONE;
                    }
                } else {
                    EftShotFx_SpawnBodyLines(r->objId, 0.1f, 0);
                }
            }
            if (r->flags & EFT_SHOTFX_REQ_A) {
                if (r->frameA == 1) {
                    r->flags |= EFT_SHOTFX_REQ_VANISH_COLUMN;
                }
                if (r->frameA == 4) {
                    r->flags &= ~EFT_SHOTFX_REQ_A;
                    if (!(r->flags & EFT_SHOTFX_REQ_B)) {
                        r->flags = 0;
                    }
                }
                if (r->flags & EFT_SHOTFX_REQ_VANISH_COLUMN) {
                    if (!(r->flags & EFT_SHOTFX_REQ_BURST)) {
                        EftShotFx_SpawnVanishColumn(r->objId, 0.12f);
                    }
                    r->flags &= ~EFT_SHOTFX_REQ_VANISH_COLUMN;
                    r->flags &= ~EFT_SHOTFX_REQ_BODY_A;
                }
                r->frameA++;
            }
            if (r->flags & EFT_SHOTFX_REQ_B) {
                if (r->frameB == 1) {
                    r->flags |= EFT_SHOTFX_REQ_BODY_B;
                }
                if (r->frameB == 4) {
                    r->flags &= ~EFT_SHOTFX_REQ_B;
                    r->flags &= ~EFT_SHOTFX_REQ_BODY_B;
                    if (!(r->flags & EFT_SHOTFX_REQ_A)) {
                        r->flags = 0;
                    }
                }
                if (r->flags & EFT_SHOTFX_REQ_APPEAR_COLUMN) {
                    if (!(r->flags & EFT_SHOTFX_REQ_BURST)) {
                        EftShotFx_SpawnAppearColumn(r->objId, 0.1f);
                    }
                    r->flags &= ~EFT_SHOTFX_REQ_APPEAR_COLUMN;
                    r->flags &= ~EFT_SHOTFX_REQ_BODY_B;
                }
                r->frameB++;
            }
        }
    }
}

/* Keeps the textures referenced while any line is alive. */
void EftShotFx_UpdateTexture(s32 a, s32 b) {
    if (gEftShotFx->count != 0) {
        EftTexSet_Keep4(gEftShotFx->tex, a, b);
    }
}

/* Corners of a line: centre = anchor pulled towards the camera plus the camera-space offset; the two ends lie
   half a length above and below it and are widened across the line of sight. */
void EftShotFx_BuildQuad(Vec4 *out, EftShotFxLine *p) {
    Vec4 eye;
    Vec4 mid;
    Vec4 off;
    Vec4 toEye;
    Vec4 side;
    Vec4 top;
    Vec4 bottom;
    EftUUpA up = { 0.0f, -1.0f, 0.0f, 1.0f };
    Mtx44 inv;
    Vec4 *out3 = &out[3];
    Vec4 *out1 = &out[1];

    Mtx_InverseRT(&inv, &gBtlCamView->world2view2);
    Vec4_Sub(&mid, (Vec4 *)inv.m[3], &p->pos);
    Vec3_Normalize(&mid, &mid);
    Vec4_Scale(&mid, &mid, p->toward);
    Vec4_Add(&mid, &mid, &p->pos);
    Mtx_MulVec4(&off, &gEftShotFx->camRot, &p->off);
    Vec4_Add(&mid, &mid, &off);
    Vec3_Scale(&side, V(&up), p->len * 0.5f);
    Vec4_Add(&top, &mid, &side);
    Vec4_Sub(&bottom, &mid, &side);
    Vec4_Copy(&eye, (Vec4 *)inv.m[3]);
    Vec4_Sub(&toEye, &eye, &top);
    Vec3_Normalize(&toEye, &toEye);
    Vec3_Cross(&side, V(&up), &toEye);
    Vec3_Normalize(&side, &side);
    Vec4_Scale(&side, &side, p->width * 0.5f);
    Vec4_Add(out, &top, &side);
    Vec4_Sub(out1, &top, &side);
    out1->w = 1.0f;
    out->w = 1.0f;
    Vec4_Sub(&toEye, &eye, &bottom);
    Vec3_Normalize(&toEye, &toEye);
    Vec3_Cross(&side, V(&up), &toEye);
    Vec3_Normalize(&side, &side);
    Vec4_Scale(&side, &side, p->width * 0.5f);
    Vec4_Add(&out[2], &bottom, &side);
    Vec4_Sub(out3, &bottom, &side);
    out3->w = 1.0f;
    out[2].w = 1.0f;
}

/* Queues the four corners as one textured triangle strip at their average depth; dropped when a corner is
   outside the GS coordinate range. The matrix argument is not used (the loaded one projects). */
void EftShotFx_DrawQuad(Vec4 *quad, EftUMtx mtx, u8 r, u8 g, u8 b, u8 a, u64 *tex, u8 blend) {
    Vec4 stq[4];
    Vec4 uv[4];
    EftUIVec scr[4];
    EftUQuadPkt *p;
    OtEntry *e;
    s32 layer;
    s32 z;
    s32 i;

    Vec4_Set(&uv[0], 0.0f, 0.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[1], 1.0f, 0.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[2], 0.0f, 1.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[3], 1.0f, 1.0f, 1.0f, 0.0f);
    Vu0Cur_ProjectPointsStq(scr, stq, quad, uv, 4);
    for (i = 0; i < 4; i++) {
        if (!EftShotFx_IsOnScreen(&scr[i], 4)) {
            return;
        }
    }
    p = (EftUQuadPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    if (p == NULL) {
        return;
    }
    p->prim = 0x5C;
    p->tag = 0x20000008;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000008;
    p->gif0 = 0xE400000000008001;
    p->gif1 = 0x42142142142160;
    p->next = 0;
    p->v[0].rgba[0] = r;
    p->v[0].rgba[1] = g;
    p->v[0].rgba[2] = b;
    p->v[0].rgba[3] = a;
    p->v[0].q = stq[0].z;
    p->v[1].rgba[0] = r;
    p->v[1].rgba[1] = g;
    p->v[1].rgba[2] = b;
    p->v[1].rgba[3] = a;
    p->v[1].q = stq[1].z;
    p->v[2].rgba[0] = r;
    p->v[2].rgba[1] = g;
    p->v[2].rgba[2] = b;
    p->v[2].rgba[3] = a;
    p->v[2].q = stq[2].z;
    p->v[3].rgba[0] = r;
    p->v[3].rgba[1] = g;
    p->v[3].rgba[2] = b;
    p->v[3].rgba[3] = a;
    p->v[3].q = stq[3].z;
    p->v[0].s = stq[0].x;
    p->v[0].t = stq[0].y;
    p->v[1].s = stq[1].x;
    p->v[1].t = stq[1].y;
    p->v[2].s = stq[2].x;
    p->v[2].t = stq[2].y;
    p->v[3].s = stq[3].x;
    p->v[3].t = stq[3].y;
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
    z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
    p->tex0 = *tex;
    layer = blend;
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
    e->tail->next = (OtPrim *)p;
    e->tail = (OtPrim *)p;
}

/* Lines on the body segments of a fighter (table gEftShotFxSegs, up to the row whose node is 4): rings of `count`
   lines every `spacing` along each segment, at a random start angle, each pulled towards the camera, fading out
   over `life` seconds after `delay` frames. Draws 1 + 3 * count libc rand() per ring. */
void EftShotFx_SpawnBodyLines(s32 objId, f32 life, s32 delay) {
    EftUVec pos;
    EftUVec start;
    EftUVec step;
    EftUVec end;
    EftUVec off;
    f32 toward;
    f32 angStep;
    f32 angBase;
    f32 len;
    f32 dist;
    f32 radius;
    f32 ang;
    u32 i;
    s32 steps;
    s32 j;
    s32 k;

    toward = BtlCharApi_GetHeight(objId) * 0.05f * 5.0f;
    for (i = 0; i < 23 && gEftShotFxSegs[i].node != 4; i++) {
        BtlCharApi_GetNodePos(objId, gEftShotFxSegs[i].node, V(&pos));
        Vec4_Copy(V(&start), V(&pos));
        if (gEftShotFxSegs[i].nodeEnd >= 0) {
            BtlCharApi_GetNodePos(objId, gEftShotFxSegs[i].nodeEnd, V(&end));
            Vec4_Sub(V(&end), V(&end), V(&pos));
            dist = Vec3_Length(V(&end));
            if (dist > gEftShotFxSegs[i].spacing) {
                steps = dist / gEftShotFxSegs[i].spacing + 1.0f;
                Vec3_Scale(V(&step), V(&end), 1.0f / steps);
            } else {
                steps = 1;
                Vec4_Set(V(&step), 0.0f, 0.0f, 0.0f, 1.0f);
            }
        } else {
            steps = 1;
            Vec4_Set(V(&step), 0.0f, 0.0f, 0.0f, 1.0f);
        }
        for (j = 0; j < steps; j++) {
            angStep = 6.2831853f / gEftShotFxSegs[i].count;
            angBase = (f32)rand() / RAND_MAX_F * 6.2831853f;
            pos.x = start.x + j * step.x;
            pos.y = start.y + j * step.y;
            pos.z = start.z + j * step.z;
            for (k = 0; k < gEftShotFxSegs[i].count; k++) {
                len = ((f32)rand() / RAND_MAX_F + gEftShotFxSegs[i].lenBase) * (BtlCharApi_GetHeight(objId) * 0.05f);
                radius = ((f32)rand() / RAND_MAX_F * 0.5f + gEftShotFxSegs[i].radiusBase) *
                         (BtlCharApi_GetHeight(objId) * 0.05f);
                ang = EftMath_WrapAngle(angBase + angStep * k);
                off.x = Mathf_SinFast(ang);
                off.y = Mathf_CosFast(ang);
                off.z = 0.0f;
                off.w = 1.0f;
                Vec4_Scale(V(&off), V(&off), radius * 1.5f);
                EftShotFx_Add(pos, off, toward, len * 2.0f, 0.0f, 0.0f, 0.3f, 0x60 - rand() % 16, life, 0, 0,
                                EFT_SHOTFX_LINE_WAIT | EFT_SHOTFX_LINE_FADE_OUT, delay);
            }
        }
    }
}

/* Rings of lines on a vertical column centred on node 3, half the fighter's height tall (table gEftShotFxRows): the
   lines grow, by 10 % less each frame, while fading out. Same random draws as the body lines. */
void EftShotFx_SpawnVanishColumn(s32 objId, f32 life) {
    EftUUpB up = { 0.0f, -1.0f, 0.0f, 1.0f };
    EftUVec top;
    EftUVec bottom;
    EftUVec base;
    EftUVec axis;
    EftUVec pos;
    EftUVec start;
    EftUVec step;
    EftUVec end;
    EftUVec off;
    f32 height;
    f32 toward;
    f32 growth;
    f32 angStep;
    f32 angBase;
    f32 len;
    f32 dist;
    f32 radius;
    f32 ang;
    u32 i;
    s32 steps;
    s32 j;
    s32 k;

    height = BtlCharApi_GetHeight(objId) * 0.5f;
    BtlCharApi_GetNodePos(objId, 3, V(&base));
    Vec4_Scale(V(&top), V(&up), height * 0.5f);
    Vec4_Add(V(&top), V(&top), V(&base));
    Vec4_Scale(V(&bottom), V(&up), -height * 0.5f);
    Vec4_Add(V(&bottom), V(&bottom), V(&base));
    Vec4_Sub(V(&axis), V(&bottom), V(&top));
    Vec3_Normalize(V(&axis), V(&axis));
    toward = BtlCharApi_GetHeight(objId) * 0.05f * 5.0f;
    growth = BtlCharApi_GetHeight(objId) * 0.05f * 7.0f;
    for (i = 0; i < 6; i++) {
        Vec4_Scale(V(&pos), V(&axis), height * gEftShotFxRows[i].from);
        Vec4_Add(V(&pos), V(&top), V(&pos));
        Vec4_Copy(V(&start), V(&pos));
        if (gEftShotFxRows[i].to >= 0.0f) {
            Vec4_Scale(V(&end), V(&axis), height * gEftShotFxRows[i].to);
            Vec4_Add(V(&end), V(&top), V(&end));
            Vec4_Sub(V(&end), V(&end), V(&pos));
            dist = Vec3_Length(V(&end));
            if (dist > gEftShotFxRows[i].spacing) {
                steps = dist / gEftShotFxRows[i].spacing + 1.0f;
                Vec3_Scale(V(&step), V(&end), 1.0f / steps);
            } else {
                steps = 1;
                Vec4_Set(V(&step), 0.0f, 0.0f, 0.0f, 1.0f);
            }
        } else {
            steps = 1;
            Vec4_Set(V(&step), 0.0f, 0.0f, 0.0f, 1.0f);
        }
        for (j = 0; j < steps; j++) {
            angStep = 6.2831853f / gEftShotFxRows[i].count;
            angBase = (f32)rand() / RAND_MAX_F * 6.2831853f;
            pos.x = start.x + j * step.x;
            pos.y = start.y + j * step.y;
            pos.z = start.z + j * step.z;
            for (k = 0; k < gEftShotFxRows[i].count; k++) {
                len = ((f32)rand() / RAND_MAX_F + gEftShotFxRows[i].lenBase) * (BtlCharApi_GetHeight(objId) * 0.05f);
                radius = ((f32)rand() / RAND_MAX_F * 0.5f + gEftShotFxRows[i].radiusBase) *
                         (BtlCharApi_GetHeight(objId) * 0.05f);
                ang = EftMath_WrapAngle(angBase + angStep * k);
                off.x = Mathf_SinFast(ang);
                off.y = Mathf_CosFast(ang);
                off.z = 0.0f;
                off.w = 1.0f;
                Vec4_Scale(V(&off), V(&off), radius * 1.2f);
                EftShotFx_Add(pos, off, toward, len * 2.0f, growth, -10.0f, 0.3f, 0x60 - rand() % 16, life, 0, 0,
                                EFT_SHOTFX_LINE_ACTIVE | EFT_SHOTFX_LINE_FADE_OUT | EFT_SHOTFX_LINE_GROW, 0);
            }
        }
    }
}

/* The same column with lines five times as long that shrink, by 12.5 % less each frame, while fading in. */
void EftShotFx_SpawnAppearColumn(s32 objId, f32 life) {
    EftUUpC up = { 0.0f, -1.0f, 0.0f, 1.0f };
    EftUVec top;
    EftUVec bottom;
    EftUVec base;
    EftUVec axis;
    EftUVec pos;
    EftUVec start;
    EftUVec step;
    EftUVec end;
    EftUVec off;
    f32 height;
    f32 toward;
    f32 growth;
    f32 angStep;
    f32 angBase;
    f32 len;
    f32 dist;
    f32 radius;
    f32 ang;
    u32 i;
    s32 steps;
    s32 j;
    s32 k;

    height = BtlCharApi_GetHeight(objId) * 0.5f;
    BtlCharApi_GetNodePos(objId, 3, V(&base));
    Vec4_Scale(V(&top), V(&up), height * 0.5f);
    Vec4_Add(V(&top), V(&top), V(&base));
    Vec4_Scale(V(&bottom), V(&up), -height * 0.5f);
    Vec4_Add(V(&bottom), V(&bottom), V(&base));
    Vec4_Sub(V(&axis), V(&bottom), V(&top));
    Vec3_Normalize(V(&axis), V(&axis));
    toward = BtlCharApi_GetHeight(objId) * 0.05f * 5.0f;
    growth = BtlCharApi_GetHeight(objId) * 0.05f * 7.0f;
    for (i = 0; i < 6; i++) {
        Vec4_Scale(V(&pos), V(&axis), height * gEftShotFxRows[i].from);
        Vec4_Add(V(&pos), V(&top), V(&pos));
        Vec4_Copy(V(&start), V(&pos));
        if (gEftShotFxRows[i].to >= 0.0f) {
            Vec4_Scale(V(&end), V(&axis), height * gEftShotFxRows[i].to);
            Vec4_Add(V(&end), V(&top), V(&end));
            Vec4_Sub(V(&end), V(&end), V(&pos));
            dist = Vec3_Length(V(&end));
            if (dist > gEftShotFxRows[i].spacing) {
                steps = dist / gEftShotFxRows[i].spacing + 1.0f;
                Vec3_Scale(V(&step), V(&end), 1.0f / steps);
            } else {
                steps = 1;
                Vec4_Set(V(&step), 0.0f, 0.0f, 0.0f, 1.0f);
            }
        } else {
            steps = 1;
            Vec4_Set(V(&step), 0.0f, 0.0f, 0.0f, 1.0f);
        }
        for (j = 0; j < steps; j++) {
            angStep = 6.2831853f / gEftShotFxRows[i].count;
            angBase = (f32)rand() / RAND_MAX_F * 6.2831853f;
            pos.x = start.x + j * step.x;
            pos.y = start.y + j * step.y;
            pos.z = start.z + j * step.z;
            for (k = 0; k < gEftShotFxRows[i].count; k++) {
                len = ((f32)rand() / RAND_MAX_F + gEftShotFxRows[i].lenBase) * (BtlCharApi_GetHeight(objId) * 0.05f);
                radius = ((f32)rand() / RAND_MAX_F * 0.5f + gEftShotFxRows[i].radiusBase) *
                         (BtlCharApi_GetHeight(objId) * 0.05f);
                ang = EftMath_WrapAngle(angBase + angStep * k);
                off.x = Mathf_SinFast(ang);
                off.y = Mathf_CosFast(ang);
                off.z = 0.0f;
                off.w = 1.0f;
                Vec4_Scale(V(&off), V(&off), radius * 1.2f);
                EftShotFx_Add(pos, off, toward, len * 10.0f, growth, -8.0f, 0.3f, 0x60 - rand() % 16, life, 0, 0,
                                EFT_SHOTFX_LINE_ACTIVE | EFT_SHOTFX_LINE_FADE_IN | EFT_SHOTFX_LINE_SHRINK, 0);
            }
        }
    }
}

/* 1 when a projected vertex (GS coordinates, 12.4 fixed, centre 2048) is in front of the camera and within a
   box around the screen centre: margin 0 = the 512 x 448 screen, 1..3 = wider boxes, else the whole GS range. */
s32 EftShotFx_IsOnScreen(EftUIVec *v, s32 margin) {
    switch (margin) {
    case 0:
        if (v->z <= 0) {
            return 0;
        }
        if (v->x > 0x8FFF) {
            return 0;
        }
        if (v->x <= 0x7000) {
            return 0;
        }
        if (v->y > 0x8DFF) {
            return 0;
        }
        if (v->y <= 0x7200) {
            return 0;
        }
        break;
    case 1:
        if (v->z <= 0) {
            return 0;
        }
        if (v->x > 0x93FF) {
            return 0;
        }
        if (v->x <= 0x6C00) {
            return 0;
        }
        if (v->y > 0x917F) {
            return 0;
        }
        if (v->y <= 0x6E80) {
            return 0;
        }
        break;
    case 2:
        if (v->z <= 0) {
            return 0;
        }
        if (v->x > 0x97FF) {
            return 0;
        }
        if (v->x <= 0x6800) {
            return 0;
        }
        if (v->y > 0x94FF) {
            return 0;
        }
        if (v->y <= 0x6B00) {
            return 0;
        }
        break;
    case 3:
        if (v->z <= 0) {
            return 0;
        }
        if (v->x > 0x9FFF) {
            return 0;
        }
        if (v->x <= 0x6000) {
            return 0;
        }
        if (v->y > 0x9BFF) {
            return 0;
        }
        if (v->y <= 0x6400) {
            return 0;
        }
        break;
    default:
        if (v->z <= 0) {
            return 0;
        }
        if (v->x > 0xFFEF) {
            return 0;
        }
        if (v->x <= 0) {
            return 0;
        }
        if (v->y > 0xFFEF) {
            return 0;
        }
        if (v->y <= 0) {
            return 0;
        }
        break;
    }
    return 1;
}
