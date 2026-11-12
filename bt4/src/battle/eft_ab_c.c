#include "common.h"
#include "battle/eft_ab.h"

/*
 * 0x1A0E58..0x1A62C8, two files merged at integration. First part: ribbon helpers, 0x1A0E58..0x1A21A8, the first
 * twelve functions of the "ribbon" module (effect pack part kind 17). Second part (formerly eft_ac.c,
 * 0x1A21A8..0x1A62C8; include/battle/eft_ac.h documents the module): the ribbon's task callbacks, manager and
 * entry points, then the swirl lines `EftZap_*`. One translation unit in the original (the second part's callers
 * only match with EftRibbon_PlaceStrip / EftRibbon_SetTexPair / the two draw functions defined above them).
 *
 * A ribbon is a list of nodes from a pool of 500 (gEftRibbonMgr); kind 0 spreads them evenly between two points
 * every frame (EftRibbon_PlaceStrip), kinds 1 and 2 shift a history of the end point through them
 * (EftRibbon_PlaceTrail1 / 2). Colour, colour pulse, width and scroll come from three keys in the pack
 * (EftRibbon_LoadKey for one key, EftRibbon_UpdateKeys between two). EftRibbon_DrawStrip draws kind 0.
 *
 * Drawing only. Random draws: one libc rand() per ribbon at init when the pack asks for a random mirror.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);

extern void Vec4_Copy(EftRbnVec *dst, EftRbnVec *src);
extern void Vec4_Sub(EftRbnVec *dst, EftRbnVec *a, EftRbnVec *b);
extern void Vec3_Add(EftRbnVec *dst, EftRbnVec *a, EftRbnVec *b);
extern void Vec3_Sub(EftRbnVec *dst, EftRbnVec *a, EftRbnVec *b);
extern void Vec3_Scale(EftRbnVec *dst, EftRbnVec *src, f32 s);
extern void Vec3_Cross(EftRbnVec *dst, EftRbnVec *a, EftRbnVec *b);
extern void Vec3_Normalize(EftRbnVec *dst, EftRbnVec *src);
extern void Vec3_Div(EftRbnVec *dst, EftRbnVec *src, f32 d);   /* dst.xyz = src.xyz / d */
extern void Vec3_Copy(EftRbnVec *dst, EftRbnVec *src);          /* copies x, y, z */

extern u64 EftVram_AddImage(EftAbTexEntry *tex, s32 a, s32 b);         /* uploads the image, returns its TEX0 */
extern u64 EftVram_AddClut(EftAbTexEntry *tex);                       /* uploads the palette, returns its block */

/* Takes a free node from the pool (round-robin) and appends it to the ribbon's list. */
EftRbnNode *EftRibbon_AllocNode(EftRbn *w) {
    EftRbnNode *node = NULL;
    s32 i = gEftRibbonMgr->next;
    s32 n;

    for (n = 0; n < EFT_RBN_NODES; n++) {
        if (gEftRibbonMgr->nodes[i].flags == 0) {
            node = &gEftRibbonMgr->nodes[i];
            memset(node, 0, sizeof(EftRbnNode));
            gEftRibbonMgr->next = i + 1;
            if (gEftRibbonMgr->next >= EFT_RBN_NODES) {
                gEftRibbonMgr->next = 0;
            }
            break;
        }
        i++;
        if (i >= EFT_RBN_NODES) {
            i = 0;
        }
    }
    if (node != NULL) {
        if (w->head == NULL) {
            w->head = node;
        }
        if (w->tail == NULL) {
            w->tail = node;
        } else {
            node->prev = w->tail;
            w->tail->next = node;
            w->tail = node;
        }
    }
    return node;
}

/* Gives every node of the ribbon back to the pool. */
void EftRibbon_FreeNodes(EftRbn *w) {
    EftRbnNode *n;
    EftRbnNode *next;

    if (w->head != NULL) {
        n = w->head;
        do {
            next = n->next;
            n->flags = 0;
            n = next;
        } while (next != NULL);
    }
    w->head = NULL;
    w->tail = NULL;
}

/* Binds texture `slot` of the ribbon to an image and a palette entry of a texture table. */
void EftRibbon_SetTexPair(EftRbn *w, s32 slot, EftAbTexEntry *tex, s32 image, s32 palette) {
    (*(w->frame + slot))[0] = tex[image];
    (*(w->frame + slot))[1] = tex[palette];
    w->arg.texBase = palette;
}

/* Builds the ribbon's TEX0 values once per frame per texture table entry, or takes the ones already built. */
void EftRibbon_UpdateTex(EftRbnPrm *prm, EftRbn *w, EftRbnArg *arg) {
    s32 i;

    if (!(arg->tex->ready & (1 << arg->texBase))) {
        for (i = 0; i < w->numTex; i++) {
            w->tex0[i] = EftVram_AddImage(&w->frame[i][0], 1, 0);
            w->tex0[i] |= (u64)EftVram_AddClut(&w->frame[i][1]) << 37;
            arg->tex->entry[arg->texBase + i].tex0 = w->tex0[i];
        }
        arg->tex->ready |= 1 << arg->texBase;
    } else {
        for (i = 0; i < w->numTex; i++) {
            w->tex0[i] = arg->tex->entry[arg->texBase + i].tex0;
        }
    }
}

/* Fills the animated values from one key of the pack. */
void EftRibbon_LoadKey(EftRbnCur *cur, EftRbnArg *arg, s32 key) {
    EftRbnVec r;
    EftRbnVec g;
    EftRbnVec b;
    EftRbnPrm *prm = arg->prm;
    EftRbnAnim *anim = arg->anim;
    s32 i;

    Vec4_Copy(&cur->color, &anim->color[key]);
    for (i = 0; i < 2; i++) {
        r.v[i] = anim->pulseR[key][i];
        g.v[i] = anim->pulseG[key][i];
        b.v[i] = anim->pulseB[key][i];
    }
    cur->pulse[0] = r.v[0];
    cur->pulse[1] = g.v[0];
    cur->pulse[2] = b.v[0];
    cur->pulseAmp[0] = r.v[1] - r.v[0];
    cur->pulseAmp[1] = g.v[1] - g.v[0];
    cur->pulseAmp[2] = b.v[1] - b.v[0];
    cur->pulseTime = anim->pulseTime[key] * 30.0f;
    cur->width = prm->width[key];
    r.v[0] = prm->widthLo[key];
    r.v[1] = prm->widthHi[key];
    cur->widthBase = r.v[0];
    cur->widthAmp = r.v[1] - r.v[0];
    cur->widthTime = prm->widthTime[key] * 30.0f;
    if (prm->flags & 4) {
        cur->alpha = prm->alpha[key];
        cur->scroll = prm->scroll[key];
    }
}

/* Fills the animated values between two keys: keys 0..1 before animSplit, 1..2 after. */
#if 0
/* NON-MATCHING: 228 instructions against 233; not worked on beyond the first version (the statement order follows the
   disassembly). */
void EftRibbon_UpdateKeys(EftRbn *w) {
    EftRbnVec d;
    EftRbnVec r;
    EftRbnVec g;
    EftRbnVec b;
    EftRbnArg *arg = &w->arg;
    EftRbnCur *cur = &w->cur;
    EftRbnAnim *anim = arg->anim;
    EftRbnPrm *prm = arg->prm;
    f32 t;
    s32 k0;
    s32 k1;
    s32 i;

    if (w->animFrame < w->animSplit) {
        t = w->animFrame / w->animSplit;
        k0 = 0;
        k1 = 1;
    } else {
        t = (w->animFrame - w->animSplit) / (w->animTime - w->animSplit);
        k0 = 1;
        k1 = 2;
    }
    Vec4_Sub(&d, &anim->color[k1], &anim->color[k0]);
    cur->color.x = anim->color[k0].x + d.x * t;
    cur->color.y = anim->color[k0].y + d.y * t;
    cur->color.z = anim->color[k0].z + d.z * t;
    cur->color.w = anim->color[k0].w + d.w * t;
    for (i = 0; i < 2; i++) {
        d.v[0] = anim->pulseR[k1][i] - anim->pulseR[k0][i];
        d.v[1] = anim->pulseG[k1][i] - anim->pulseG[k0][i];
        d.v[2] = anim->pulseB[k1][i] - anim->pulseB[k0][i];
        r.v[i] = anim->pulseR[k0][i] + d.v[0] * t;
        g.v[i] = anim->pulseG[k0][i] + d.v[1] * t;
        b.v[i] = anim->pulseB[k0][i] + d.v[2] * t;
    }
    cur->pulse[0] = r.v[0];
    cur->pulse[1] = g.v[0];
    cur->pulse[2] = b.v[0];
    cur->pulseAmp[0] = r.v[1] - r.v[0];
    cur->pulseAmp[1] = g.v[1] - g.v[0];
    cur->pulseAmp[2] = b.v[1] - b.v[0];
    d.v[0] = anim->pulseTime[k1] - anim->pulseTime[k0];
    cur->pulseTime = (anim->pulseTime[k0] + d.v[0] * t) * 30.0f;
    d.v[0] = prm->width[k1] - prm->width[k0];
    cur->width = prm->width[k0] + d.v[0] * t;
    d.v[0] = prm->widthLo[k1] - prm->widthLo[k0];
    d.v[1] = prm->widthHi[k1] - prm->widthHi[k0];
    d.v[2] = prm->widthTime[k1] - prm->widthTime[k0];
    r.v[0] = prm->widthLo[k0] + d.v[0] * t;
    r.v[1] = prm->widthHi[k0] + d.v[1] * t;
    cur->widthBase = r.v[0];
    cur->widthAmp = r.v[1] - r.v[0];
    cur->widthTime = (prm->widthTime[k0] + d.v[2] * t) * 30.0f;
    if (prm->flags & 4) {
        d.v[0] = prm->alpha[k1] - prm->alpha[k0];
        cur->alpha = prm->alpha[k0] + d.v[0] * t;
        d.v[0] = prm->scroll[k1] - prm->scroll[k0];
        cur->scroll = prm->scroll[k0] + d.v[0] * t;
    }
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/eft_ab_c", EftRibbon_UpdateKeys);

/* Appends a node at the previous last node's position, or at pos for the first one. Returns 0 when the pool is
   empty. (The list's tail is already the new node here, so the first copy copies the node onto itself.) */
s32 EftRibbon_AddNode(EftRbn *w, EftRbnVec *pos) {
    EftRbnNode *n = EftRibbon_AllocNode(w);

    if (n != NULL) {
        n->flags |= 1;
        if (w->tail != NULL) {
            Vec4_Copy(&n->pos, &w->tail->pos);
        } else {
            Vec4_Copy(&n->pos, pos);
        }
        return 1;
    }
    return 0;
}

/* Second half of the task's init: flags and fade times from the pack, the texture count and node limit by kind. */
void EftRibbon_InitNodes(EftRbn *w, EftRbnArg *arg) {
    EftRbnPrm *prm = arg->prm;
    EftRbnCur *cur = &w->cur;
    s32 i;

    if (prm->flags & 4) {
        w->flags |= 0x20;
    }
    if (prm->flags & 2) {
        if (!(rand() & 1)) {
            w->flags |= 0x40;
        }
    }
    Vec4_Copy(&w->end, &arg->pos);
    w->width = cur->width;
    Vec3_Copy(&w->color, &cur->color);
    w->color.w = 0.0f;
    w->fadeInTime = prm->fadeIn * 30.0f;
    if (w->fadeInTime <= w->color.w) {
        w->flags |= 0x80;
    }
    w->fadeOutTime = prm->fadeOut * 30.0f;
    if (w->fadeOutTime <= w->color.w) {
        w->flags |= 0x100;
    } else {
        w->fadeOutFrame = w->fadeOutTime;
    }
    if (w->flags & 0x20) {
        w->alpha = cur->alpha;
        w->scroll = cur->scroll;
    }
    if (prm->kind == 0) {
        if (w->flags & 0x20) {
            w->numTex = 4;
        } else {
            w->numTex = 3;
        }
        w->maxNodes = 0x20;
    } else if (prm->kind == 1) {
        w->numTex = 1;
        w->maxNodes = 0x10;
    } else {
        w->numTex = 4;
        w->maxNodes = 0x14;
    }
    for (i = 0; i < w->numTex; i++) {
        EftRibbon_SetTexPair(w, i, arg->tex->entry, arg->texBase + i, arg->texBase + i);
    }
}

/* Kind 0: spreads the nodes evenly from start to the ribbon's end point; the first, middle and last segments use
   textures 0, 1 and 2. */
/* FAKE MATCH (permuter): `r = w;` a second time inside the loop, in front of the step, with the two reads behind
   it going through `r`. The original copies `w` into $a1 behind the first calls (move a1,s1) and reads the list
   head and the colour address through the copy; with a single `r = w` the copy propagation of gcse folds `r`
   into `w` (60 of 87 instructions, mostly by that one-instruction shift). A second assignment that reaches the
   colour read of the next pass leaves that read with two reaching definitions, so `r` stays a register of its
   own up to the loop; the assignment itself emits nothing. It stands in for whatever kept the original's copy
   alive: initialising `r` (to NULL or to `w`) at its declaration, a block-local or second copy for the step,
   the same assignment at the flags test or without a read through `r` do not do it. Same trick, other place, in
   EftRibbon_PlaceTrail2; EftBlade_GetColor (eft_aa.c) needed only an initialiser. */
void EftRibbon_PlaceStrip(EftRbnPrm *prm, EftRbn *w, EftRbnVec *start) {
    EftRbnVec pos;
    EftRbnVec step;
    EftRbn *r;
    EftRbnNode *n;
    EftRbnNode *next;
    s32 i = 0;

    Vec4_Copy(&pos, start);
    Vec3_Sub(&step, &w->end, &pos);
    Vec3_Div(&step, &step, w->numNodes);
    r = w;
    step.w = 0.0f;
    n = r->head;
    if (n != NULL) {
        do {
            Vec3_Copy(&n->pos, &pos);
            if (i == 0) {
                n->tex = 0;
            } else if (i < w->numNodes - 2) {
                n->tex = 1;
            } else {
                n->tex = 2;
            }
            Vec4_Copy(&n->color, &r->color);
            if (w->flags & 0x20) {
                Vec4_Copy(&n->color2, &w->color2);
                if (i == 0 || n->next == NULL) {
                    n->color2.w = 0.0f;
                }
            }
            r = w;
            if (i < r->numNodes - 2) {
                Vec3_Add(&pos, &pos, &step);
            } else {
                Vec3_Copy(&pos, &r->end);
            }
            next = n->next;
            i++;
            n = next;
        } while (next != NULL);
    }
}

/* Kind 1: pushes the end point into the head of the list and shifts every older position one node down. */
void EftRibbon_PlaceTrail1(EftRbnPrm *prm, EftRbn *w, EftRbnArg *arg) {
    EftRbnVec cur;
    EftRbnVec old;
    EftRbnNode *n;
    EftRbnNode *next;

    Vec4_Copy(&cur, &w->end);
    n = w->head;
    if (n != NULL) {
        do {
            Vec4_Copy(&old, &n->pos);
            Vec4_Copy(&n->pos, &cur);
            Vec4_Copy(&cur, &old);
            n->tex = 0;
            next = n->next;
            n = next;
        } while (next != NULL);
    }
}

/* Kind 2: the same shift, with textures 2, 1 and 0 for the first, middle and last segments and the colour
   refreshed. */
/* FAKE MATCH (permuter): `r = w;` a second time inside the loop (in the arm that tests the node count), as in
   EftRibbon_PlaceStrip: the colour read behind the if / else then has two reaching definitions of `r`, the copy
   propagation of gcse cannot fold `r` into `w`, and the original's `move a1,s3` with the list head and the hoisted
   `r + 0xB0` read through $a1 comes out. With a single `r = w` the code differs in 9 of 59 instructions (no copy,
   and $a0 set before $a1 for the first Vec4_Copy). The assignment must be on a path that does not cover the
   whole loop body: at the loop top, at its end or in front of the colour copy it does nothing or worse; `while`
   / `for` forms of the loop give 24. */
void EftRibbon_PlaceTrail2(EftRbnPrm *prm, EftRbn *w, EftRbnArg *arg) {
    EftRbnVec cur;
    EftRbnVec old;
    EftRbn *r;
    EftRbnNode *n;
    EftRbnNode *next;
    s32 i = 0;

    Vec4_Copy(&cur, &w->end);
    r = w;
    n = r->head;
    if (n != NULL) {
        do {
            Vec3_Copy(&old, &n->pos);
            Vec3_Copy(&n->pos, &cur);
            if (i == 0) {
                n->tex = 2;
            } else {
                r = w;
                if (i < r->numNodes - 2) {
                    n->tex = 1;
                } else {
                    n->tex = 0;
                }
            }
            Vec4_Copy(&n->color, &r->color);
            i++;
            Vec3_Copy(&cur, &old);
            next = n->next;
            n = next;
        } while (next != NULL);
    }
}

/* A vertex as ClipVtx_Set fills it (eft_a.h EftGfxVert). */
typedef struct EftRbnVert {
    /* 0x00 */ EftRbnVec pos;
    /* 0x10 */ EftRbnVec color;
    /* 0x20 */ EftRbnVec uv;
} EftRbnVert; /* 0x30 */

typedef struct EftRbnView {
    /* 0x000 */ u8 unk0[0x220];
    /* 0x220 */ EftRbnVec pos;     /* camera position */
} EftRbnView;
extern EftRbnView *gBtlCamView;

extern void ClipVtx_Set(EftRbnVert *out, EftRbnVec *pos, EftRbnVec *uv, EftRbnVec *color);
extern void EftGfx_DrawPolyScaledZ(EftRbnVert *verts, s32 blend, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex0,
                                   f32 zScale);

/* Draws a kind 0 ribbon: one camera-facing quad (two triangles) per pair of nodes, the side vector being the cross
   product of the segment and the direction from the camera; each quad starts on the previous quad's end points.
   A scrolling ribbon (flag 0x20) is drawn a second time with its fourth texture, the scrolled texture rectangle and
   the nodes' second colour. (The second layer's own corner points, built with EftRbn.alpha as the width, are
   computed but the same corner points as the first layer are drawn.) */
ASM_STUB_BEGIN(); /* compiled so that EftRibbon_Draw sees the definition; the assembler skips it */
/* NON-MATCHING: 415 instructions against 414, 238 differ; first version only, written from the disassembly. */
void EftRibbon_DrawStrip(EftRbn *w, EftRbnArg *arg, EftRbnPrm *prm) {
    EftRbnVec uvA[4] = {
        { { 0.02f, 0.0f, 1.0f, 0.0f } }, { { 0.02f, 0.98f, 1.0f, 0.0f } }, { { 0.98f, 0.0f, 1.0f, 0.0f } }, { { 0.98f, 0.98f, 1.0f, 0.0f } },
    };
    EftRbnVec uvB[4] = {
        { { 0.02f, 0.98f, 1.0f, 0.0f } }, { { 0.02f, 0.0f, 1.0f, 0.0f } }, { { 0.98f, 0.98f, 1.0f, 0.0f } }, { { 0.98f, 0.0f, 1.0f, 0.0f } },
    };
    EftRbnVec quad[4];
    EftRbnVec prev[2];
    EftRbnVec quad2[4];
    EftRbnVec prev2[2];
    EftRbnVec cam;
    EftRbnVec side;
    EftRbnVec toCam;
    EftRbnVec uv[4];
    EftRbnVec uv2[4];
    EftRbnVec col[4];
    EftRbnVec half = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    EftRbnVert verts[3];
    s32 first = 1;
    EftRbnNode *n;
    EftRbnNode *next;
    s32 i;

    Vec4_Copy(&cam, &gBtlCamView->pos);
    if (!(w->flags & 0x40)) {
        for (i = 0; i < 4; i++) {
            Vec4_Copy(&uv[i], &uvA[i]);
        }
        if (w->flags & 0x20) {
            for (i = 0; i < 4; i++) {
                Vec4_Copy(&uv2[i], &uvA[i]);
                uv2[i].x += w->scrollPos;
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            Vec4_Copy(&uv[i], &uvB[i]);
        }
        if (w->flags & 0x20) {
            for (i = 0; i < 4; i++) {
                Vec4_Copy(&uv2[i], &uvB[i]);
                uv2[i].x += w->scrollPos;
            }
        }
    }
    for (n = w->head; n != NULL; n = n->next) {
        next = n->next;
        if (next != NULL) {
            Vec4_Sub(&side, &next->pos, &n->pos);
            Vec4_Sub(&toCam, &n->pos, &cam);
            Vec3_Cross(&side, &side, &toCam);
            Vec3_Normalize(&side, &side);
            Vec3_Scale(&half, &side, w->width);
            if (first) {
                Vec3_Sub(&quad[0], &n->pos, &half);
                Vec3_Add(&quad[1], &n->pos, &half);
                quad[0].w = 1.0f;
                quad[1].w = 1.0f;
            } else {
                Vec4_Copy(&quad[0], &prev[0]);
                Vec4_Copy(&quad[1], &prev[1]);
            }
            Vec3_Sub(&quad[2], &next->pos, &half);
            Vec3_Add(&quad[3], &next->pos, &half);
            quad[2].w = 1.0f;
            quad[3].w = 1.0f;
            Vec4_Copy(&prev[0], &quad[2]);
            Vec4_Copy(&prev[1], &quad[3]);
            if (w->flags & 0x20) {
                if (first) {
                    Vec3_Scale(&half, &side, w->alpha);
                    Vec3_Sub(&quad2[0], &n->pos, &half);
                    Vec3_Add(&quad2[1], &n->pos, &half);
                    quad2[0].w = 1.0f;
                    quad2[1].w = 1.0f;
                } else {
                    Vec4_Copy(&quad2[0], &prev2[0]);
                    Vec4_Copy(&quad2[1], &prev2[1]);
                }
                Vec3_Scale(&half, &side, w->alpha);
                Vec3_Sub(&quad2[2], &next->pos, &half);
                Vec3_Add(&quad2[3], &next->pos, &half);
                quad2[2].w = 1.0f;
                quad2[3].w = 1.0f;
                Vec4_Copy(&prev2[0], &quad2[2]);
                Vec4_Copy(&prev2[1], &quad2[3]);
            }
            Vec4_Copy(&col[0], &n->color);
            Vec4_Copy(&col[1], &n->color);
            first = 0;
            Vec4_Copy(&col[2], &next->color);
            Vec4_Copy(&col[3], &next->color);
            for (i = 0; i < 2; i++) {
                ClipVtx_Set(&verts[0], &quad[i], &uv[i], &col[i]);
                ClipVtx_Set(&verts[1], &quad[i + 1], &uv[i + 1], &col[i + 1]);
                ClipVtx_Set(&verts[2], &quad[i + 2], &uv[i + 2], &col[i + 2]);
                EftGfx_DrawPolyScaledZ(verts, prm->blend, 0, 0, 0, 0, w->tex0[n->tex], 1.0f);
            }
            if (w->flags & 0x20) {
                Vec4_Copy(&col[0], &n->color2);
                Vec4_Copy(&col[1], &n->color2);
                Vec4_Copy(&col[2], &next->color2);
                Vec4_Copy(&col[3], &next->color2);
                for (i = 0; i < 2; i++) {
                    ClipVtx_Set(&verts[0], &quad[i], &uv2[i], &col[i]);
                    ClipVtx_Set(&verts[1], &quad[i + 1], &uv2[i + 1], &col[i + 1]);
                    ClipVtx_Set(&verts[2], &quad[i + 2], &uv2[i + 2], &col[i + 2]);
                    EftGfx_DrawPolyScaledZ(verts, prm->blend, 1, 0, 0, 0, w->tex0[3], 1.0f);
                }
            }
        }
    }
}
ASM_STUB_END();
INCLUDE_RODATA("asm/nonmatchings/battle/eft_ab_c", D_002ED510);
INCLUDE_RODATA("asm/nonmatchings/battle/eft_ab_c", D_002ED550);
INCLUDE_ASM("asm/nonmatchings/battle/eft_ab_c", EftRibbon_DrawStrip);

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly eft_ac.c), with its own header and view types. Names the first part already declared with
 * other types are reached through cast macros (the generated code is the same).
 * ------------------------------------------------------------------------------------------------------------ */
#include "battle/eft_ac.h"

/*
 * Effect pack part modules, 0x1A21A8..0x1A62C8. See include/battle/eft_ac.h.
 */

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftAcView {
    /* 0x000 */ Mtx44 world2view;
    /* 0x040 */ Mtx44 world2view2;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 world2screen;
    /* 0x180 */ u8 unk180[0xA0];
    /* 0x220 */ Vec4 eye;       /* camera position (inferred: the ribbons take their view direction from it) */
} EftAcView;

#define V(p) ((Vec4 *)(p))

/* GS TEX0 of entry idx of a texture table (entries of 0x10 bytes; the shift is needed for the match). */
#define EFT_TEX0(tbl, idx) (*(u64 *)((u8 *)(tbl) + ((idx) << 4)))

#define gBtlCamView ((EftAcView *)gBtlCamView)
#ifdef PORT /* assigned to: a cast is not an lvalue for a modern compiler */
#define gEftRibbonMgr (*(EftRibbonMgr **)&gEftRibbonMgr)
#else
#define gEftRibbonMgr ((EftRibbonMgr *)gEftRibbonMgr)
#endif
extern void *gEftRibbonTasks;
extern void *gEftRibbonClass[6];
extern EftZapPool *gEftZapMgr;

extern void *memset(void *dst, s32 c, u32 n);
extern f32 sqrtf(f32 x);
#define Vec4_Copy ((void (*)(Vec4 *dst, Vec4 *src))Vec4_Copy)
#define Vec4_Sub ((void (*)(Vec4 *dst, Vec4 *a, Vec4 *b))Vec4_Sub)
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
#define Vec3_Add ((void (*)(Vec4 *dst, Vec4 *a, Vec4 *b))Vec3_Add)
#define Vec3_Sub ((void (*)(Vec4 *dst, Vec4 *a, Vec4 *b))Vec3_Sub)
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
#define Vec3_Scale ((void (*)(Vec4 *dst, Vec4 *src, f32 s))Vec3_Scale)
#define Vec3_Cross ((void (*)(Vec4 *dst, Vec4 *a, Vec4 *b))Vec3_Cross)
#define Vec3_Normalize ((void (*)(Vec4 *dst, Vec4 *src))Vec3_Normalize)
extern f32 Vec3_Dist(Vec4 *a, Vec4 *b);                 /* distance between two points */
extern s32 Vu0Cur_ProjectPoints(EftAcScr *out, Vec4 *pos, s32 count); /* projects count points */
#define ClipVtx_Set ((void (*)(EftAcVert *vtx, Vec4 *pos, Vec4 *uv, Vec4 *col))ClipVtx_Set)
#define EftGfx_DrawPolyScaledZ ((void (*)(EftAcVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex, f32 zScale))EftGfx_DrawPolyScaledZ)
extern void Vu0Cur_Push(void);       /* VU0 matrix stack push */
extern void Vu0Cur_LoadMtx(Mtx44 *m);   /* load the matrix */
extern void Vu0Cur_Pop(void);       /* pop */
extern void EftGfx_DrawSprite(Vec4 *pos, Vec4 *color, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 roll,
                              s32 layer, s32 front, u64 tex0);
extern void EftPrim_DrawQuadDepth(Vec4 *pos, Vec4 *color, s32 layer, s32 front, u64 tex0, f32 w, f32 h, f32 u0, f32 v0,
                                  f32 u1, f32 v1, f32 roll);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern s32 Vu0Cur_ProjectPointsStq(EftAcScr *xyz, Vec4 *stq, Vec4 *pos, Vec4 *uv, s32 n); /* projects n points with the loaded matrix */
extern void Vec4_ToInt(EftAcScr *dst, Vec4 *src);                            /* float to fixed vector */
extern u8 *gOtCur;
extern EftAcOtSlot *gOtZ;
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 BtlScene_IsEffectStopped(s32 chr, s32 type);
extern s32 BtlScene_IsEffectHidden(s32 chr, s32 type);
extern void *BtlTask_CreateChildList(EftAcTask *task, s32 count, s32 workSize);
extern void *BtlTaskList_AddTail(void *list, void **cls, void *arg);
extern void BtlTask_SetDead(EftAcTask *task);  /* kills the task */
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern s32 rand(void);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 EftMath_WrapAngle(f32 angle);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle);       /* rotate about X */
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle);       /* rotate about Y */
extern void Mtx_Translate(Mtx44 *dst, Mtx44 *src, Vec4 *v);         /* translate */
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);                  /* copy */
extern void Vec4_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi);    /* clamps each component */
extern f32 Rand_FloatRange(f32 lo, f32 hi);
extern s32 Rand_IntRange(s32 lo, s32 hi);

/* The rest of the zap module (the file after this one, eft_ad). */
extern void EftZap_LoadKey(EftZapWork *w, s32 n);
extern s32 EftZap_AddNode(EftZapStrand *line);                       /* adds a point; 0 when the pool is empty */
extern void EftZap_SetTexPair(EftZapWork *w, EftAcTex *tex, s32 a, s32 b);
extern void EftZap_LoadTex(EftZapWork *w);

/* The first part of the ribbon module (the file before this one, eft_ab). EftRibbon_SetEnd and
   EftRibbon_SetTexFrame match only when EftRibbon_PlaceStrip / EftRibbon_SetTexPair are DEFINED earlier in the same translation
   unit (a branch-likely / delay-slot choice changes otherwise), and so does EftRibbon_Update (one instruction):
   this file is the tail of that source file. Merged at integration; where the function the callers need is still
   INCLUDE_ASM (EftRibbon_DrawStrip, EftRibbon_DrawKind1), its attempt is compiled inside
   ASM_STUB_BEGIN / ASM_STUB_END (include/include_asm.h) so that the compiler has seen a definition. */
#define EftRibbon_FreeNodes ((void (*)(EftRibbon *w))EftRibbon_FreeNodes)                                       /* frees the nodes */
#define EftRibbon_SetTexPair ((void (*)(EftRibbon *w, s32 slot, void *uv, s32 a, s32 b))EftRibbon_SetTexPair)     /* sets a texture frame */
#define EftRibbon_UpdateTex ((void (*)(EftRibbonPrm *prm, EftRibbon *w, EftRibbonArg *arg))EftRibbon_UpdateTex) /* steps the nodes */
#define EftRibbon_LoadKey ((void (*)(EftRibbonCur *cur, EftRibbonArg *arg, s32 mode))EftRibbon_LoadKey)     /* reads the animated values */
#define EftRibbon_UpdateKeys ((void (*)(EftRibbon *w))EftRibbon_UpdateKeys)                                       /* steps the texture animation */
#define EftRibbon_AddNode ((s32 (*)(EftRibbon *w, EftRibbonArg *arg))EftRibbon_AddNode)                     /* adds a node */
#define EftRibbon_InitNodes ((void (*)(EftRibbon *w, EftRibbonArg *arg))EftRibbon_InitNodes)                    /* second half of the init */
#define EftRibbon_PlaceStrip ((void (*)(EftRibbonPrm *prm, EftRibbon *w, EftRibbonArg *arg))EftRibbon_PlaceStrip) /* places the nodes, kind 0 */
#define EftRibbon_PlaceTrail1 ((void (*)(EftRibbonPrm *prm, EftRibbon *w, EftRibbonArg *arg))EftRibbon_PlaceTrail1) /* kind 1 */
#define EftRibbon_PlaceTrail2 ((void (*)(EftRibbonPrm *prm, EftRibbon *w, EftRibbonArg *arg))EftRibbon_PlaceTrail2) /* kind 2 */
#define EftRibbon_DrawStrip ((void (*)(EftRibbon *w, EftRibbonArg *arg, EftRibbonPrm *prm))EftRibbon_DrawStrip) /* draws kind 0 */

/* Draws a kind 1 ribbon: one camera-facing quad (two clipped triangles) per pair of nodes, the texture's u running
   from 1 at the head to 0 at the tail. The quad's side vector is the cross product of the segment and the direction
   from the camera; it is flipped when it turns against the previous one, and each quad starts on the previous
   quad's end points. */
ASM_STUB_BEGIN(); /* compiled so that EftRibbon_Draw sees the definition; the assembler skips it */
/* NON-MATCHING: 15 of 302 instructions differ: the node pointer and the address of prev[0] swap s6 / s7; everything else
   (including the frame layout) is identical.
   Permuter round 2: its "score 0" candidate (no `cam`; the eye copied into `prevSide`) is NOT a match and NOT the
   same behaviour: prevSide is overwritten at the end of every pass, so the direction to the camera would be wrong
   from the second quad on, and the frame is 16 bytes off (66 of 302 by fdiff; the permuter's score ignores stack
   offsets). The original has `cam` at sp + 224 as here. It does show that the swap depends on the pseudos that
   hold local addresses in front of the loop. Without effect (15): `for` instead of `while` for the link walk,
   declaration order of link / node, `np` at function scope, a local for node->next; a node-pointer walk gives 27;
   `cam` declared anywhere else gives 18 to 38. */
void EftRibbon_DrawKind1(EftRibbon *w, EftRibbonArg *arg, EftRibbonPrm *prm) {
    EftAcVec uvA[4] = { { 1.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f, 0.0f } };
    EftAcVec uvB[4] = { { 1.0f, 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };
    EftAcVec pos[4];
    EftAcVec prev[2];
    EftAcVec cam;
    EftAcVec side;
    EftAcVec prevSide;
    EftAcVec toCam;
    EftAcVec uv[4];
    EftAcVec col;
    EftAcVec unk170;
    EftAcVec half;
    EftAcVec unk190;
    EftAcScr scr[4];
    EftAcVert verts[9];
    EftRibbonNode **link;
    EftRibbonNode *node;
    s32 first = 1;
    s32 idx = 0;
    s32 i;
    f32 u = 1.0f;
    f32 u2;
    f32 du;

    Vec4_Copy(V(&cam), &gBtlCamView->eye);
    du = -1.0f / (f32)w->numNodes;
    u2 = du + u;
    if (!(w->flags & EFT_RIBBON_MIRROR)) {
        for (i = 0; i < 4; i++) {
            Vec4_Copy(V(&uv[i]), V(&uvA[i]));
        }
    } else {
        for (i = 0; i < 4; i++) {
            Vec4_Copy(V(&uv[i]), V(&uvB[i]));
        }
    }
    Vec4_Copy(V(&col), &w->scaleA);
    link = &w->head;
    while (*link != NULL) {
        node = *link;
        if (node->next != NULL) {
            Vec4 *np = &node->next->pos;

            Vec4_Sub(V(&side), np, &node->pos);
            Vec4_Sub(V(&toCam), &node->pos, V(&cam));
            Vec3_Cross(V(&side), V(&side), V(&toCam));
            Vec3_Normalize(V(&side), V(&side));
            Vec4_Scale(V(&half), V(&side), w->width);
            if (first) {
                Vec3_Add(V(&pos[0]), &node->pos, V(&half));
                first = 0;
                pos[0].w = 1.0f;
                Vec3_Sub(V(&pos[1]), &node->pos, V(&half));
                pos[1].w = 1.0f;
            } else {
                if (Vec3_Dot(V(&side), V(&prevSide)) < 0.0f) {
                    Vec3_Scale(V(&half), V(&half), -1.0f);
                    Vec3_Scale(V(&side), V(&side), -1.0f);
                }
                if (Vec3_Dot(V(&side), V(&prevSide)) < 0.82f) {
                    Vec3_Dist(&node->pos, np);
                }
                Vec4_Copy(V(&pos[0]), V(&prev[0]));
                Vec4_Copy(V(&pos[1]), V(&prev[1]));
            }
            Vec3_Add(V(&pos[2]), np, V(&half));
            pos[2].w = 1.0f;
            Vec3_Sub(V(&pos[3]), np, V(&half));
            pos[3].w = 1.0f;
            Vec4_Copy(V(&prev[0]), V(&pos[2]));
            Vec4_Copy(V(&prev[1]), V(&pos[3]));
            Vec4_Copy(V(&prevSide), V(&side));
            Vu0Cur_ProjectPoints(scr, V(pos), 4);
            uv[0].x = u;
            uv[1].x = u;
            uv[2].x = u2;
            uv[3].x = u2;
            for (i = 0; i < 2; i++) {
                ClipVtx_Set(&verts[0], V(&pos[i]), V(&uv[i]), V(&col));
                ClipVtx_Set(&verts[1], V(&pos[i + 1]), V(&uv[i + 1]), V(&col));
                ClipVtx_Set(&verts[2], V(&pos[i + 2]), V(&uv[i + 2]), V(&col));
                EftGfx_DrawPolyScaledZ(verts, prm->blend, 0, 0, 0, 0, w->tex[node->tex], 1.0f);
            }
        }
        u = du + u;
        u2 += du;
        if (!(idx < w->numNodes - 2)) {
            u2 = 0.0f;
        }
        idx++;
        link = &node->next;
    }
}
ASM_STUB_END();
LIT4_WORD(D_002FCEC4, 0x3F51EB85); /* 0.82f */
INCLUDE_RODATA("asm/nonmatchings/battle/eft_ab_c", D_002ED590);
INCLUDE_RODATA("asm/nonmatchings/battle/eft_ab_c", D_002ED5D0);
INCLUDE_ASM("asm/nonmatchings/battle/eft_ab_c", EftRibbon_DrawKind1);

/* Draws a kind 2 ribbon: like kind 1 with per-node colours and a fixed texture strip (u 0.02..0.98). Where the
   ribbon bends sharply (side vectors less than 0.82 aligned and the nodes more than 0.8 apart) three sprites
   (node, middle, next node) replace the quad. */
void EftRibbon_DrawKind2(EftRibbon *w, EftRibbonArg *arg, EftRibbonPrm *prm) {
    EftAcVec uvA[4] = { { 0.98f, 0.0f, 1.0f, 0.0f }, { 0.98f, 0.98f, 1.0f, 0.0f }, { 0.02f, 0.0f, 1.0f, 0.0f }, { 0.02f, 0.98f, 1.0f, 0.0f } };
    EftAcVec uvB[4] = { { 0.98f, 0.98f, 1.0f, 0.0f }, { 0.98f, 0.0f, 1.0f, 0.0f }, { 0.02f, 0.98f, 1.0f, 0.0f }, { 0.02f, 0.0f, 1.0f, 0.0f } };
    EftAcVec pos[4];
    EftAcVec prevA;
    EftAcVec prevB;
    EftAcVec cam;
    EftAcVec side;
    EftAcVec prevSide;
    EftAcVec toCam;
    EftAcVec uv[4];
    EftAcVec col[4];
    EftAcVec sprCol;
    EftAcVec mid;
    EftAcVec half = { 0.0f, 0.0f, 0.0f, 0.0f };
    EftAcVert verts[9];
    EftRibbonNode **link;
    EftRibbonNode *node;
    EftRibbonNode *next;
    s32 first = 1;
    s32 sprite;
    s32 idx = 0;
    s32 i;
    f32 size;
    f32 size2;
    f32 a;

    Vec4_Copy(V(&cam), &gBtlCamView->eye);
    if (!(w->flags & EFT_RIBBON_MIRROR)) {
        for (i = 0; i < 4; i++) {
            Vec4_Copy(V(&uv[i]), V(&uvA[i]));
        }
    } else {
        for (i = 0; i < 4; i++) {
            Vec4_Copy(V(&uv[i]), V(&uvB[i]));
        }
    }
    size = w->width * 9.5f;
    link = &w->head;
    while (*link != NULL) {
        node = *link;
        next = node->next;
        if (next != NULL) {
            sprite = 0;
            Vec4_Sub(V(&side), &next->pos, &node->pos);
            Vec4_Sub(V(&toCam), &node->pos, V(&cam));
            Vec3_Cross(V(&side), V(&side), V(&toCam));
            Vec3_Normalize(V(&side), V(&side));
            Vec3_Scale(V(&half), V(&side), w->width);
            if (first) {
                Vec3_Sub(V(&pos[0]), &node->pos, V(&half));
                pos[0].w = 1.0f;
                Vec3_Add(V(&pos[1]), &node->pos, V(&half));
                pos[1].w = 1.0f;
            } else {
                if (Vec3_Dot(V(&side), V(&prevSide)) < 0.0f) {
                    Vec3_Scale(V(&half), V(&half), -1.0f);
                    Vec3_Scale(V(&side), V(&side), -1.0f);
                }
                if (Vec3_Dot(V(&side), V(&prevSide)) < 0.82f && Vec3_Dist(&node->pos, &next->pos) > 0.8f) {
                    sprite = 1;
                    mid.x = (node->pos.x + next->pos.x) * 0.5f;
                    mid.y = (node->pos.y + next->pos.y) * 0.5f;
                    mid.z = (node->pos.z + next->pos.z) * 0.5f;
                    mid.w = 1.0f;
                    a = idx / w->numNodes;
                    Vec4_Copy(V(&sprCol), &node->color);
                    a = 1.0f - a;
                    size2 = size * a;
                    sprCol.w *= a;
                    EftGfx_DrawSprite(&node->pos, V(&sprCol), size2, size2, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, prm->blend, 0,
                                      w->tex[3]);
                    EftGfx_DrawSprite(V(&mid), V(&sprCol), size2, size2, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, prm->blend, 0,
                                      w->tex[3]);
                    EftGfx_DrawSprite(&next->pos, V(&sprCol), size2, size2, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, prm->blend, 0,
                                      w->tex[3]);
                }
                Vec4_Copy(V(&pos[0]), V(&prevA));
                Vec4_Copy(V(&pos[1]), V(&prevB));
            }
            Vec3_Sub(V(&pos[2]), &next->pos, V(&half));
            first = 0;
            pos[2].w = 1.0f;
            Vec3_Add(V(&pos[3]), &next->pos, V(&half));
            pos[3].w = 1.0f;
            Vec4_Copy(V(&prevA), V(&pos[2]));
            Vec4_Copy(V(&prevB), V(&pos[3]));
            Vec4_Copy(V(&prevSide), V(&side));
            if (!sprite) {
                Vec4_Copy(V(&col[0]), &node->color);
                Vec4_Copy(V(&col[1]), &node->color);
                Vec4_Copy(V(&col[2]), &next->color);
                Vec4_Copy(V(&col[3]), &next->color);
                for (i = 0; i < 2; i++) {
                    ClipVtx_Set(&verts[0], V(&pos[i]), V(&uv[i]), V(&col[i]));
                    ClipVtx_Set(&verts[1], V(&pos[i + 1]), V(&uv[i + 1]), V(&col[i + 1]));
                    ClipVtx_Set(&verts[2], V(&pos[i + 2]), V(&uv[i + 2]), V(&col[i + 2]));
                    EftGfx_DrawPolyScaledZ(verts, prm->blend, 0, 0, 0, 0, w->tex[node->tex], 1.0f);
                }
            }
        }
        idx++;
        link = &node->next;
    }
}

/* Task init: copies the argument block and reads the first animated values. */
void EftRibbon_Init(EftAcTask *task, EftRibbonArg *arg) {
    EftRibbon *w = task->work;
    EftRibbonPrm *prm = arg->prm;
    EftRibbonCur *cur;

    memset(w, 0, sizeof(EftRibbon));
    cur = &w->cur;
    w->arg = *arg;
    w->flags |= EFT_RIBBON_ALIVE;
    w->kind = prm->kind;
    w->unkD8 = arg->unk28;
    if (arg->life <= 0.0f) {
        w->flags |= EFT_RIBBON_NO_LIFE;
    } else {
        w->life = arg->life * 30.0f;
    }
    if (prm->flags & 1) {
        w->animTime = prm->animTime * 30.0f;
        w->unk100 = w->animTime * prm->unk14;
        EftRibbon_LoadKey(cur, &w->arg, 0);
    } else {
        EftRibbon_LoadKey(cur, &w->arg, 2);
    }
    EftRibbon_InitNodes(w, arg);
}

/* Task term: frees the nodes. */
void EftRibbon_Term(EftAcTask *task) {
    EftRibbon *w = task->work;

    w->flags = 0;
    EftRibbon_FreeNodes(w);
}

/* Task update. While the effect is not stopped: counts the start delay down, then each frame takes the animated
   width / alpha / scroll, applies the width and scale pulses (ping-pong), the size factor, the fade-in and (after a
   stop) the fade-out, sets the wanted node count (kind 0: distance between the two points / 60 + 4; else one more
   per frame, up to the limit), adds nodes, places them by kind, steps the texture animation and counts the life
   down (which stops the ribbon). A stopped ribbon dies when its fade delay and width have run out. */
void EftRibbon_Update(EftAcTask *task) {
    EftRibbon *w = task->work;
    EftRibbonArg *arg = &w->arg;
    EftRibbonPrm *prm = arg->prm;
    EftRibbonCur *cur;
    Vec4 d;
    s32 n;
    f32 t;
    f32 k;

    if (!BtlScene_IsEffectStopped(arg->chr, arg->type)) {
        if (w->delay <= 0.0f) {
            cur = &w->cur;
            w->width = cur->width;
            w->alpha = cur->alpha;
            w->scroll = cur->scroll;
            if ((prm->flags & 8) && cur->widthTime > 0.0f) {
                t = w->widthFrame / cur->widthTime;
                k = cur->widthBase + cur->widthAmp * t;

                w->width *= k;
                w->alpha *= k;
                if (!(w->flags & EFT_RIBBON_WIDTH_DN)) {
                    w->widthFrame += 1.0f;
                    if (cur->widthTime <= w->widthFrame) {
                        w->widthFrame = cur->widthTime;
                        w->flags |= EFT_RIBBON_WIDTH_DN;
                    }
                } else {
                    w->widthFrame -= 1.0f;
                    if (w->widthFrame <= 0.0f) {
                        w->widthFrame = 0.0f;
                        w->flags &= ~EFT_RIBBON_WIDTH_DN;
                    }
                }
            }
            w->width *= arg->size;
            w->alpha *= arg->size;
            if (!(w->flags & EFT_RIBBON_FADED_IN)) {
                t = w->fadeInFrame / w->fadeInTime;

                w->fadeInFrame += 1.0f;
                if (w->fadeInTime <= w->fadeInFrame) {
                    w->flags |= EFT_RIBBON_FADED_IN;
                } else {
                    w->width *= t;
                    w->alpha *= t;
                }
            }
            if (w->flags & EFT_RIBBON_STOP) {
                if (w->fadeDelay <= 0.0f) {
                    if (!(w->flags & EFT_RIBBON_FADED_OUT)) {
                        t = w->fadeOutFrame / w->fadeOutTime;

                        w->fadeOutFrame -= 1.0f;
                        w->width *= t;
                        w->alpha *= t;
                        if (w->fadeOutFrame <= 0.0f) {
                            w->flags |= EFT_RIBBON_FADED_OUT;
                            w->width = 0.0f;
                            w->alpha = 0.0f;
                        }
                    } else {
                        w->width = 0.0f;
                        w->alpha = 0.0f;
                    }
                }
            }
            Vec4_Copy(&w->scaleA, &cur->scale);
            Vec4_Copy(&w->scaleB, &cur->scale);
            if ((prm->flags & 0x10) && cur->pulseTime > 0.0f) {
                t = w->pulseFrame / cur->pulseTime;
                k = cur->pulse[0] + cur->pulseAmp[0] * t;

                w->scaleA.x *= k;
                w->scaleB.x *= k;
                k = cur->pulse[1] + cur->pulseAmp[1] * t;
                w->scaleA.y *= k;
                w->scaleB.y *= k;
                k = cur->pulse[2] + cur->pulseAmp[2] * t;
                w->scaleA.z *= k;
                w->scaleB.z *= k;
                if (!(w->flags & EFT_RIBBON_PULSE_DN)) {
                    w->pulseFrame += 1.0f;
                    if (cur->pulseTime <= w->pulseFrame) {
                        w->pulseFrame = cur->pulseTime;
                        w->flags |= EFT_RIBBON_PULSE_DN;
                    }
                } else {
                    w->pulseFrame -= 1.0f;
                    if (w->pulseFrame <= 0.0f) {
                        w->pulseFrame = 0.0f;
                        w->flags &= ~EFT_RIBBON_PULSE_DN;
                    }
                }
            }
            if (w->flags & EFT_RIBBON_SCROLL) {
                w->scrollPos += w->scroll;
                if (1.0f <= w->scrollPos) {
                    w->scrollPos -= 1.0f;
                } else if (w->scrollPos <= -1.0f) {
                    w->scrollPos += 1.0f;
                }
            }
            if (w->scaleA.w <= 0.0f || w->width <= 0.0f) {
                w->flags &= ~EFT_RIBBON_VISIBLE;
            } else {
                w->flags |= EFT_RIBBON_VISIBLE;
            }
            if (prm->kind == 0) {
                Vec4_Sub(&d, &w->end, (Vec4 *)&arg->pos);
                w->wantNodes = sqrtf(Vec3_Dot(&d, &d)) / 60.0f + 4.0f;
            } else {
                w->wantNodes++;
            }
            if (w->wantNodes > w->maxNodes) {
                w->wantNodes = w->maxNodes;
            }
            if (w->numNodes < w->wantNodes) {
                for (n = w->wantNodes - w->numNodes; n != 0; n--) {
                    if (EftRibbon_AddNode(w, arg)) {
                        w->numNodes++;
                    } else {
                        break;
                    }
                }
            }
            if (prm->kind == 0) {
                EftRibbon_PlaceStrip(prm, w, arg);
            } else if (prm->kind == 1) {
                EftRibbon_PlaceTrail1(prm, w, arg);
            } else {
                EftRibbon_PlaceTrail2(prm, w, arg);
            }
            if ((prm->flags & 1) && w->animFrame <= w->animTime) {
                EftRibbon_UpdateKeys(w);
                w->animFrame += 1.0f;
                if (w->animTime <= w->animFrame) {
                    EftRibbon_LoadKey(cur, arg, 2);
                }
            }
            if (!(w->flags & EFT_RIBBON_NO_LIFE)) {
                w->life -= 1.0f;
                if (w->life <= 0.0f) {
                    w->flags |= EFT_RIBBON_STOP;
                    if (w->fadeTime > 0.0f) {
                        w->flags |= EFT_RIBBON_TIMED_OUT;
                    }
                }
            }
        } else {
            w->delay -= 1.0f;
        }
        if (w->flags & EFT_RIBBON_STOP) {
            if (w->fadeDelay > 0.0f) {
                w->fadeDelay -= 1.0f;
            }
            if (w->fadeDelay <= 0.0f && w->width <= 0.0f) {
                w->flags |= EFT_RIBBON_DEAD;
            }
        }
    }
    if (w->flags & EFT_RIBBON_DEAD) {
        BtlTask_SetDead(task);
    } else {
        EftRibbon_UpdateTex(prm, w, arg);
    }
}

/* Task post-update: nothing. */
void EftRibbon_PostUpdate(EftAcTask *task) {
}

/* Task reset: kills the task. */
void EftRibbon_Reset(EftAcTask *task) {
    BtlTask_SetDead(task);
}

/* Task draw: loads the world-to-screen matrix and draws the ribbon by kind. */
void EftRibbon_Draw(EftAcTask *task) {
    EftRibbon *w = task->work;
    EftRibbonArg *arg = &w->arg;
    EftRibbonPrm *prm = arg->prm;

    if (BtlScene_IsEffectHidden(arg->chr, arg->type)) {
        return;
    }
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    if (w->kind == 0) {
        EftRibbon_DrawStrip(w, arg, prm);
    } else if (w->kind == 1) {
        EftRibbon_DrawKind1(w, arg, prm);
    } else if (w->kind == 2) {
        EftRibbon_DrawKind2(w, arg, prm);
    }
    Vu0Cur_Pop();
}

/* Manager init: the manager block, the node buffer and the list of 40 ribbon tasks. */
void EftRibbonMgr_Init(EftAcTask *task) {
    gEftRibbonMgr = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftRibbonMgr));
    memset(gEftRibbonMgr, 0, sizeof(EftRibbonMgr));
    gEftRibbonMgr->nodes = BtlPool_Alloc(BtlPool_GetCurrent(), 40000);
    memset(gEftRibbonMgr->nodes, 0, 40000);
    gEftRibbonTasks = BtlTask_CreateChildList(task, 40, sizeof(EftRibbon));
}

/* Manager term. */
void EftRibbonMgr_Term(EftAcTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftRibbonMgr->nodes);
    BtlPool_Free(BtlPool_GetCurrent(), gEftRibbonMgr);
    gEftRibbonMgr = NULL;
}

/* Manager update: nothing. */
void EftRibbonMgr_Update(EftAcTask *task) {
}

/* Manager reset: nothing. */
void EftRibbonMgr_Reset(EftAcTask *task) {
}

/* Creates a ribbon task; returns it (NULL when all 40 are in use). */
void *EftRibbon_Create(EftRibbonArg *arg) {
    EftRibbonArg a = *arg;

    return BtlTaskList_AddTail(gEftRibbonTasks, gEftRibbonClass, &a);
}

/* Starts the fade-out. */
s32 EftRibbon_Stop(EftAcTask *task) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_RIBBON_STOP;
    return 1;
}

/* Makes the next update kill the task. */
s32 EftRibbon_Kill(EftAcTask *task) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_RIBBON_DEAD;
    return 1;
}

/* Both of the above. No caller. */
s32 EftRibbon_StopAndKill(EftAcTask *task) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_RIBBON_STOP | EFT_RIBBON_DEAD;
    return 1;
}

#define IS_RIBBON(task) ((task)->cls[0] == (void *)EftRibbon_Update)

/* True when the task is a live ribbon. */
s32 EftRibbon_IsAlive(EftAcTask *task) {
    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    if (((EftRibbon *)task->work)->flags & EFT_RIBBON_ALIVE) {
        return 1;
    }
    return 0;
}

/* Writes the first point. */
s32 EftRibbon_SetPos(EftAcTask *task, Vec4 *pos) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    Vec4_Copy((Vec4 *)&w->arg.pos, pos);
    return 1;
}

/* Writes the second point; with `place`, a kind 0 ribbon lays its nodes out again at once. */
s32 EftRibbon_SetEnd(EftAcTask *task, Vec4 *pos, s32 place) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    Vec4_Copy(&w->end, pos);
    if (place) {
        EftRibbonArg *arg = &w->arg;
        EftRibbonPrm *prm = arg->prm;

        if (prm->kind == 0) {
            EftRibbon_PlaceStrip(prm, w, arg);
        }
    }
    return 1;
}

/* Writes the size factor. */
s32 EftRibbon_SetSize(EftAcTask *task, f32 size) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    w->arg.size = size;
    return 1;
}

/* Writes the node limit (3 at least). */
s32 EftRibbon_SetMaxNodes(EftAcTask *task, s32 count) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    if (count < 3) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    w->maxNodes = count;
    return 1;
}

/* Writes the start delay in frames. */
s32 EftRibbon_SetDelay(EftAcTask *task, f32 frames) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    w->delay = frames;
    return 1;
}

/* Writes the delay between the stop and the fade, in frames. */
s32 EftRibbon_SetFadeDelay(EftAcTask *task, f32 frames) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    w->fadeDelay = frames;
    return 1;
}

/* Writes the fade-out length in frames. */
s32 EftRibbon_SetFadeTime(EftAcTask *task, f32 frames) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    w->fadeTime = frames;
    if (frames > 0.0f) {
        w->fadeOutTime = frames;
        if (frames <= 0.0f) {
            w->flags |= EFT_RIBBON_FADED_OUT;
        } else {
            w->fadeOutFrame = frames;
        }
    }
    return 1;
}

/* Sets one texture frame of the ribbon. No caller. */
s32 EftRibbon_SetTexFrame(EftAcTask *task, s32 slot, void *uv, s32 a, s32 b) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    EftRibbon_SetTexPair(w, slot, uv, a, b);
    return 1;
}

/* Writes +0xD8. */
s32 EftRibbon_SetUnkD8(EftAcTask *task, s32 value) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    w->unkD8 = value;
    return 1;
}

/* Writes the effect type (what BtlScene_IsEffectStopped / Hidden are asked with). No caller. */
s32 EftRibbon_SetType(EftAcTask *task, s32 type) {
    EftRibbon *w;

    if (task == NULL) {
        return 0;
    }
    if (!IS_RIBBON(task)) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RIBBON_ALIVE)) {
        return 0;
    }
    w->arg.type = type;
    return 1;
}

/* ---- zap (part kind 12) -------------------------------------------------------------------------------------- */

/* Manager init: the manager block and the list of 5 emitter tasks. */
void EftZapMgr_Init(EftAcTask *task) {
    gEftZapMgr = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftZapPool));
    memset(gEftZapMgr, 0, sizeof(EftZapPool));
    gEftZapMgr->tasks = BtlTask_CreateChildList(task, 5, sizeof(EftZapWork));
    Mtx_StoreIdentity(&gEftZapMgr->identity);
}

/* Manager update: nothing. */
void EftZapMgr_Update(EftAcTask *task) {
}

/* Manager draw: nothing. */
void EftZapMgr_Draw(EftAcTask *task) {
}

/* Manager reset: nothing. */
void EftZapMgr_Reset(EftAcTask *task) {
}

/* Manager term. */
void EftZapMgr_Term(EftAcTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftZapMgr);
    gEftZapMgr = NULL;
}

void EftZap_InitKeys(EftAcTask *task);
void EftZap_StepKeys(EftAcTask *task);
void EftZap_DrawQuad(EftAcVec *pos, EftAcVec color, f32 u0, f32 v0, f32 u1, f32 v1, f32 u2, f32 v2, f32 u3, f32 v3,
                     s32 layer, s32 texIdx, s32 front, EftAcTex *tex);
void EftZap_SpawnLine(EftZapWork *w);
void EftZap_UpdateLines(EftZapWork *w);

/* Task init: copies the argument block and starts the emitter. */
void EftZap_Init(EftAcTask *task, EftZapInit *arg) {
    EftZapWork *w = task->work;

    memset(w, 0, sizeof(EftZapWork));
    w->arg = *arg;
    if (w->arg.def->flags & 0x10) {
        EftZap_InitKeys(task);
        w->flags |= EFT_ZAPF_KEYED;
    } else {
        EftZap_LoadKey(w, 2);
    }
    if (w->arg.life > 0.0f) {
        w->life = w->arg.life * 30.0f;
    } else {
        w->flags |= EFT_ZAPF_NO_LIFE;
    }
    w->flags |= EFT_ZAPF_ALIVE;
    EftZap_SetTexPair(w, w->arg.tex, w->arg.unk38, w->arg.unk38);
}

/* Task update: steps the emitter's timers, spawns a burst of lines every `interval` frames and moves the lines. */
void EftZap_Update(EftAcTask *task) {
    f32 one = 1.0f;
    EftZapWork *w = task->work;
    EftZapInit *arg = &w->arg;
    EftZapDef *def = arg->def;
    s32 i;

    if (!BtlScene_IsEffectStopped(arg->objId, w->arg.type)) {
        if (w->unk160 > 0.0f) {
            w->unk160 -= one;
        }
        if ((w->flags & EFT_ZAPF_FADE) && !(w->flags & EFT_ZAPF_HOLD)) {
            f32 t = w->fadeFrame / w->fadeTime;

            /* the clamped ratio is not used: only a compare is left of it */
            if (t < 0.0f) {
                t = 0.0f;
            } else if (t > 1.0f) {
                t = 1.0f;
            }
            w->fadeFrame -= 1.0f;
            if (w->fadeFrame < 0.0f) {
                w->flags &= ~EFT_ZAPF_ALIVE;
                w->flags |= EFT_ZAPF_DEAD;
            }
        }
        {
            EftAcVec ofs = { 0.0f, 0.0f, 0.0f, 0.0f };

            ofs.w = 1.0f;
            Vec3_Scale(V(&ofs), V(&arg->dir), def->offset);
            Vec3_Add(V(&w->origin), V(&arg->pos), V(&ofs));
        }
        if (w->flags & EFT_ZAPF_KEYED) {
            EftZap_StepKeys(task);
        }
        if (!(w->flags & EFT_ZAPF_STOPPED)) {
            if ((s32)w->frame % arg->def->interval == 0) {
                for (i = 0; i < arg->def->count; i++) {
                    EftZap_SpawnLine(w);
                }
            }
        }
        EftZap_UpdateLines(w);
        if (!(w->flags & EFT_ZAPF_NO_LIFE)) {
            w->life -= 1.0f;
            if (w->life < 0.0f) {
                if (w->fadeFrame > 0.0f) {
                    w->flags |= EFT_ZAPF_FADE;
                } else {
                    if (def->flags & 0x100) {
                        w->flags |= EFT_ZAPF_20;
                    }
                    w->flags |= EFT_ZAPF_STOPPED;
                }
            }
        }
        if (w->flags & EFT_ZAPF_HOLD) {
            w->holdTime -= 1.0f;
            if (w->holdTime < 0.0f) {
                w->flags &= ~EFT_ZAPF_HOLD;
                if (!(w->flags & EFT_ZAPF_FADE)) {
                    w->flags |= EFT_ZAPF_STOPPED;
                }
            }
        }
        if (w->head == NULL) {
            if (w->flags & EFT_ZAPF_STOPPED) {
                w->flags &= ~EFT_ZAPF_ALIVE;
                w->flags |= EFT_ZAPF_DEAD;
            }
        }
        if ((w->flags & EFT_ZAPF_KILL) || (w->flags & EFT_ZAPF_DEAD)) {
            BtlTask_SetDead(task);
        }
        w->frame += 1.0f;
    }
    if (w->flags & EFT_ZAPF_KEYED) {
        EftZap_InitKeys(task);
        if (w->frame >= w->keyTime) {
            EftZap_LoadKey(w, 2);
            w->flags &= ~EFT_ZAPF_KEYED;
        }
    }
    if (!(w->flags & EFT_ZAPF_KILL)) {
        if (!(w->flags & EFT_ZAPF_DEAD)) {
            EftZap_LoadTex(w);
        }
    }
}

/* Task post-update: nothing. */
void EftZap_PostUpdate(EftAcTask *task) {
}

/* Task draw. Per visible line: with definition flag 0x40 one sprite per point (clipped quad with flag 0x80,
   else EftGfx_DrawSprite at 16 times the width); otherwise a ribbon like EftRibbon_DrawKind1 through the points
   transformed by the line's matrix, the side vector taken against the camera (facing 0) or against the emitter's
   origin (facing 1), drawn as clipped triangles (flag 0x80) or as one GS quad (EftZap_DrawQuad). */
/* FAKE MATCH (permuter): the local function pointer `set` (= ClipVtx_Set), the first declaration of the block,
   used for the three vertex calls. The calls come out as plain `jal ClipVtx_Set` and the pointer leaves no
   instruction, but its initialisation (a symbol address) is still an instruction when the first scheduling pass
   runs, right behind the Vu0Cur_LoadMtx call, and that pass issues two instructions per cycle: one more in front
   decides the order in which the address / length arguments of the three initialiser memsets (cam, half, verts)
   are loaded and so their temporaries (a2 / a3 / t0 as in the original; without it a3 / t0 / v0 and the second
   memset loads its length behind the address: 9 of 420 instructions). Any symbol address works there (a pointer
   to Vu0Cur_ProjectPoints does too); a constant in a variable, an initialised local, a camera-view or eye
   pointer do not (propagated away earlier, or they change the code). So the original had one more instruction
   in front of the initialisers that left no trace; which is not known. The permuter's second change
   (`0.0f > dot`) is not needed. The 8-byte initialiser of u[] is D_002FEAD8 in .sdata. */
void EftZap_Draw(EftAcTask *task) {
    EftZapWork *w = task->work;
    EftZapDef *def = w->arg.def;

    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    {
    void (*set)(EftAcVert *vtx, Vec4 *pos, Vec4 *uv, Vec4 *col) = ClipVtx_Set;
    EftAcScr scr[4];
    EftAcVec seg;
    EftAcVec view;
    EftAcVec side;
    EftAcVec prevSide;
    EftAcVec pos[4];
    EftAcVec prevA;
    EftAcVec prevB;
    EftAcVec p0;
    EftAcVec p1 = { 0.0f, 0.0f, 0.0f, 0.0f };
    EftAcVec cam = { 0.0f, 0.0f, 0.0f, 0.0f };
    EftAcVec half = { 0.0f, 0.0f, 0.0f, 0.0f };
    EftAcVec uv[4] = { { 1.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f, 0.0f } };
    EftAcVert verts[9] = { { { 0.0f } } };
    f32 u[2] = { 1.0f, 0.0f };
    EftZapStrand *line;
    EftZapPt *pt;
    s32 idx;
    s32 first;
    s32 i;
    f32 width;
    f32 du;

    Vec4_Copy(V(&cam), &gBtlCamView->eye);
    for (line = w->head; line != NULL; line = line->next) {
        if (line->flags & 0x80) {
            width = line->width * 0.5f * w->arg.size;
            pt = line->head;
            if (def->flags & 0x40) {
                for (; pt != NULL; pt = pt->next) {
                    Mtx_MulVec4(V(&p0), &line->mtx, V(&pt->pos));
                    if (def->flags & 0x80) {
                        EftPrim_DrawQuadDepth(V(&p0), V(&line->color), def->blend, (w->flags >> 17) & 1, EFT_TEX0(w->arg.tex, w->texIdx), width, width, 0.0f, 0.0f,
                                              1.0f, 1.0f, 0.0f);
                    } else {
                        EftGfx_DrawSprite(V(&p0), V(&line->color), width * 16.0f, width * 16.0f, 0.0f, 0.0f, 1.0f, 1.0f,
                                          0.0f, def->blend, (w->flags >> 17) & 1, EFT_TEX0(w->arg.tex, w->texIdx));
                    }
                }
            } else {
                first = 1;
                du = 0.0f;
                u[1] = du;
                u[0] = du;
                idx = 0;
                du = -1.0f / (f32)line->numPts;
                u[1] = du + u[1];
                for (; pt != NULL; pt = pt->next) {
                    if (pt->next != NULL) {
                        Mtx_MulVec4(V(&p0), &line->mtx, V(&pt->pos));
                        Mtx_MulVec4(V(&p1), &line->mtx, V(&pt->next->pos));
                        Vec4_Sub(V(&seg), V(&p1), V(&p0));
                        switch (def->facing) {
                        case 0:
                            Vec4_Sub(V(&view), V(&p0), V(&cam));
                            break;
                        case 1:
                            Vec3_Sub(V(&view), V(&p0), V(&w->origin));
                            break;
                        }
                        Vec3_Cross(V(&side), V(&view), V(&seg));
                        Vec3_Normalize(V(&side), V(&side));
                        Vec3_Scale(V(&half), V(&side), width);
                        if (first) {
                            Vec3_Add(V(&pos[0]), V(&p0), V(&half));
                            Vec3_Sub(V(&pos[1]), V(&p0), V(&half));
                            first = 0;
                            pos[1].w = 1.0f;
                            pos[0].w = 1.0f;
                        } else {
                            if (Vec3_Dot(V(&side), V(&prevSide)) < 0.0f) {
                                Vec3_Scale(V(&side), V(&side), -1.0f);
                                Vec3_Scale(V(&half), V(&half), -1.0f);
                            }
                            Vec4_Copy(V(&pos[0]), V(&prevA));
                            Vec4_Copy(V(&pos[1]), V(&prevB));
                        }
                        Vec3_Add(V(&pos[2]), V(&p1), V(&half));
                        Vec3_Sub(V(&pos[3]), V(&p1), V(&half));
                        pos[3].w = 1.0f;
                        pos[2].w = 1.0f;
                        Vec4_Copy(V(&prevA), V(&pos[2]));
                        Vec4_Copy(V(&prevB), V(&pos[3]));
                        Vec4_Copy(V(&prevSide), V(&side));
                        uv[0].x = u[0];
                        uv[1].x = u[0];
                        uv[2].x = u[1];
                        uv[3].x = u[1];
                        if (def->flags & 0x80) {
                            Vu0Cur_ProjectPoints(scr, V(pos), 4);
                            for (i = 0; i < 2; i++) {
                                set(&verts[0], V(&pos[i]), V(&uv[i]), V(&line->color));
                                set(&verts[1], V(&pos[i + 1]), V(&uv[i + 1]), V(&line->color));
                                set(&verts[2], V(&pos[i + 2]), V(&uv[i + 2]), V(&line->color));
                                EftGfx_DrawPolyScaledZ(verts, def->blend, 0, 0, (w->flags >> 17) & 1, 0,
                                                       EFT_TEX0(w->arg.tex, w->texIdx), 2.0f);
                            }
                        } else {
                            EftZap_DrawQuad(pos, line->color, uv[0].x, uv[0].y, uv[1].x, uv[1].y, uv[2].x, uv[2].y,
                                            uv[3].x, uv[3].y, def->blend, w->texIdx, (w->flags >> 17) & 1, w->arg.tex);
                        }
                    }
                    u[0] += du;
                    u[1] += du;
                    if (!(idx < line->numPts - 2)) {
                        u[1] = 0.0f;
                    }
                    idx++;
                }
            }
        }
    }
    }
    Vu0Cur_Pop();
}

/* Task reset: kills the task. */
void EftZap_Reset(EftAcTask *task) {
    BtlTask_SetDead(task);
}

/* Unlinks `x` from the doubly linked list `l` (head / tail). */
#define EFT_LIST_REMOVE(T, l, x)                 \
    if ((l)->head != NULL) {                     \
        T *prev_ = (x)->prev;                    \
                                                 \
        if (prev_ == NULL) {                     \
            T *next_ = (x)->next;                \
                                                 \
            if (next_ == NULL) {                 \
                (l)->head = NULL;                \
                (l)->tail = NULL;                \
            } else {                             \
                (l)->head = next_;               \
                next_->prev = NULL;              \
            }                                    \
        } else {                                 \
            T *next_ = (x)->next;                \
                                                 \
            if (next_ == NULL) {                 \
                (l)->tail = prev_;               \
                prev_->next = NULL;              \
            } else {                             \
                next_->prev = prev_;             \
                (x)->prev->next = (x)->next;     \
            }                                    \
        }                                        \
    }

/* Task term: gives every line and point back to the manager's pools. */
void EftZap_Term(EftAcTask *task) {
    EftZapWork *w = task->work;
    EftZapStrand *line;
    EftZapPt *pt;

    for (line = w->head; line != NULL; line = line->next) {
        for (pt = line->head; pt != NULL; pt = pt->next) {
            pt->flags = 0;
            EFT_LIST_REMOVE(EftZapPt, line, pt);
        }
        line->flags = 0;
        EFT_LIST_REMOVE(EftZapStrand, w, line);
    }
    w->flags = 0;
}

/* Starts a segment of the emitter's own key animation: the differences between two keys. */
void EftZap_InitKeys(EftAcTask *task) {
    EftZapWork *w = task->work;
    EftZapKeyTbl *k = w->arg.keys;
    EftZapDef *def = w->arg.def;
    s32 n = 0;

    if (w->frame <= 0.0f) {
        n = 1;
        w->keyTime = def->keyTime * 30.0f;
        w->keySplit = w->keyTime * def->keySplit;
    }
    if (w->keySplit <= w->frame) {
        if (!(w->flags & EFT_ZAPF_KEY2)) {
            w->flags |= EFT_ZAPF_KEY2;
            n = 2;
        }
    }
    if (n != 0) {
        Vec4_Sub(V(&w->dA), V(&k->a[n]), V(&k->a[n - 1]));
        Vec4_Sub(V(&w->dB), V(&k->b[n]), V(&k->b[n - 1]));
        Vec4_Sub(V(&w->dC), V(&k->c[n]), V(&k->c[n - 1]));
        Vec4_Sub(V(&w->dD), V(&k->d[n]), V(&k->d[n - 1]));
        w->tintR.delta[0] = k->e[n][0] - k->e[n - 1][0];
        w->tintR.delta[1] = k->e[n][1] - k->e[n - 1][1];
        w->tintG.delta[0] = k->f[n][0] - k->f[n - 1][0];
        w->tintG.delta[1] = k->f[n][1] - k->f[n - 1][1];
        w->tintB.delta[0] = k->g[n][0] - k->g[n - 1][0];
        w->tintB.delta[1] = k->g[n][1] - k->g[n - 1][1];
        w->tintTime.delta = k->h[n] - k->h[n - 1];
        w->colorStart.delta = k->i[n] - k->i[n - 1];
        w->colorEnd.delta = k->j[n] - k->j[n - 1];
    }
}

/* Interpolates the emitter's animated values inside the current key segment. */
void EftZap_StepKeys(EftAcTask *task) {
    EftZapWork *w = task->work;
    EftZapKeyTbl *k = w->arg.keys;
    s32 n = 0;
    f32 t = 0.0f;
    EftAcVec v = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (!(w->flags & EFT_ZAPF_KEY2)) {
        if (w->keySplit > 0.0f) {
            t = w->frame / w->keySplit;
        }
    } else {
        n = 1;
        t = (w->frame - w->keySplit) / (w->keyTime - w->keySplit);
    }
    Vec4_Scale(V(&v), V(&w->dA), t);
    Vec4_Add(V(&w->curA), V(&k->a[n]), V(&v));
    Vec4_Scale(V(&v), V(&w->dC), t);
    Vec4_Add(V(&w->curC), V(&k->c[n]), V(&v));
    Vec4_Scale(V(&v), V(&w->dB), t);
    Vec4_Add(V(&w->curB), V(&k->b[n]), V(&v));
    Vec4_Scale(V(&v), V(&w->dD), t);
    Vec4_Add(V(&w->curD), V(&k->d[n]), V(&v));
    w->tintR.cur[0] = k->e[n][0] + w->tintR.delta[0] * t;
    w->tintR.cur[1] = k->e[n][1] + w->tintR.delta[1] * t;
    w->tintG.cur[0] = k->f[n][0] + w->tintG.delta[0] * t;
    w->tintG.cur[1] = k->f[n][1] + w->tintG.delta[1] * t;
    w->tintB.cur[0] = k->g[n][0] + w->tintB.delta[0] * t;
    w->tintB.cur[1] = k->g[n][1] + w->tintB.delta[1] * t;
    w->tintTime.cur = k->h[n] + w->tintTime.delta * t;
    w->colorStart.cur = k->i[n] + w->colorStart.delta * t;
    w->colorEnd.cur = k->j[n] + w->colorEnd.delta * t;
}

s32 EftZap_InitLine(EftZapStrand *line, EftZapWork *w);

#define ZAP_CLAMP01(x) ((x) < 0.0f ? 0.0f : (1.0f < (x) ? 1.0f : (x)))

/* Sets a line flag by a mode byte of the definition: 0 always, 2 at random. */
#define ZAP_MODE(mode, bit)                                \
    if ((mode) == 0 || ((mode) == 2 && !(rand() & 1))) {   \
        line->flags |= (bit);                              \
    }

/* Takes the next free line of the manager (round robin), starts it and appends it to the emitter's list. */
void EftZap_SpawnLine(EftZapWork *w) {
    EftZapStrand *line;
    u8 i;

    if (gEftZapMgr->nextLine >= EFT_ZAP_STRANDS) {
        gEftZapMgr->nextLine = 0;
    }
    i = gEftZapMgr->nextLine;
    do {
        line = &gEftZapMgr->line[i];
        i++;
        if (i >= EFT_ZAP_STRANDS) {
            i = 0;
        }
        if (line->flags == 0) {
            if (EftZap_InitLine(line, w)) {
                line->flags |= 1;
                if (w->head == NULL) {
                    w->head = line;
                    w->tail = line;
                } else {
                    line->prev = w->tail;
                    w->tail->next = line;
                    w->tail = line;
                }
                gEftZapMgr->nextLine = i;
            }
            return;
        }
    } while (i != gEftZapMgr->nextLine);
}

/* Starts a line: draws its life, point count, start delay, the three keys of each of the four animated values
   (C, B, D = angle step, A = width), its start angle, tilt and colours from the definition's base + range pairs,
   places its axis and takes its points from the pool. Returns 0 (after giving the points back) when the life or
   the point count comes out as 0 or the pool runs dry. */
#if 0
/* NON-MATCHING: 7 of 567 instructions differ: in the keyA block the registers of def->split and of the constant 1.0f are
   swapped (f3 / f2); the keyB and keyC blocks, written the same way, match.
   Cleanup E: the difference from the matching blocks is the memset of colEnd in the same basic block: its four
   argument instructions lie inside the live ranges and change the local allocation priorities
   (log2(refs) * refs / length). The original order (1.0f before split, and the second split load last) needs one
   more instruction between the second `def->split` load and its subtraction than this C has; statement orders,
   an explicit memset and the position of the colEnd declaration do not give it. Without the block-local `life`
   f0 / f1 are swapped as well (13). */
s32 EftZap_InitLine(EftZapStrand *line, EftZapWork *w) {
    EftZapDef *def = w->arg.def;
    f32 fps = 30.0f;
    f32 key[3] = { 0.0f, 0.0f, 0.0f };
    s32 i;

    line->life = Rand_FloatRange(def->lifeMin, def->lifeMin + def->lifeRange) * fps;
    line->age = 0.0f;
    if (line->life <= line->age) {
        return 0;
    }
    line->head = NULL;
    line->tail = NULL;
    line->next = NULL;
    line->prev = NULL;
    line->numPts = Rand_IntRange(def->ptsMin, def->ptsMin + def->ptsRange);
    if (line->numPts <= 0) {
        return 0;
    }
    line->fadeInTime = line->life * def->fadeIn;
    line->fadeLeft = line->fadeOutTime = line->life * (1.0f - def->fadeOut);
    line->colorStart = w->colorStart.cur;
    line->colorEnd = w->colorEnd.cur;
    line->colorFrame = 0.0f;
    line->colorTime = line->life * (line->colorEnd - line->colorStart);
    line->delay = Rand_FloatRange(def->delayMin, def->delayMin + def->delayRange) * fps;
    Mtx_Copy(&line->mtx, &gEftZapMgr->identity);
    ZAP_MODE(def->modeRise, 0x200);
    key[0] = Rand_FloatRange(def->riseKey[0], def->riseKey[0] + def->riseKeyRange[0]);
    key[1] = Rand_FloatRange(def->riseKey[1], def->riseKey[1] + def->riseKeyRange[1]);
    key[2] = Rand_FloatRange(def->riseKey[2], def->riseKey[2] + def->riseKeyRange[2]);
    if (def->flags & 1) {
        key[1] = (key[0] + key[2]) * 0.5f;
    }
    line->riseStep[0] = (key[1] - key[0]) / (line->life * def->split);
    line->riseStep[1] = (key[2] - key[1]) / (line->life * (1.0f - def->split));
    line->rise = key[0];
    ZAP_MODE(def->modeRadius, 0x800);
    key[0] = Rand_FloatRange(def->radiusKey[0], def->radiusKey[0] + def->radiusKeyRange[0]);
    key[1] = Rand_FloatRange(def->radiusKey[1], def->radiusKey[1] + def->radiusKeyRange[1]);
    key[2] = Rand_FloatRange(def->radiusKey[2], def->radiusKey[2] + def->radiusKeyRange[2]);
    if (def->flags & 4) {
        key[1] = (key[0] + key[2]) * 0.5f;
    }
    line->radiusStep[0] = (key[1] - key[0]) / (line->life * def->split);
    line->radiusStep[1] = (key[2] - key[1]) / (line->life * (1.0f - def->split));
    line->radius = key[0];
    ZAP_MODE(def->modeSpin, 0x1000);
    if (def->modeReverse == 1 || (def->modeReverse == 2 && !(rand() & 1))) {
        line->flags |= 0x2000;
    }
    line->angle = EftMath_WrapAngle(Rand_FloatRange(def->angle, def->angle + def->angleRange) * 6.2831853f);
    key[0] = Rand_FloatRange(def->spinKey[0], def->spinKey[0] + def->spinKeyRange[0]) * 3.14159265f;
    key[1] = Rand_FloatRange(def->spinKey[1], def->spinKey[1] + def->spinKeyRange[1]) * 3.14159265f;
    key[2] = Rand_FloatRange(def->spinKey[2], def->spinKey[2] + def->spinKeyRange[2]) * 3.14159265f;
    if (def->flags & 8) {
        key[1] = (key[0] + key[2]) * 0.5f;
    }
    line->spinStep[0] = EftMath_WrapAngle((key[1] - key[0]) / (line->life * def->split));
    line->spinStep[1] = EftMath_WrapAngle((key[2] - key[1]) / (line->life * (1.0f - def->split)));
    line->spin = EftMath_WrapAngle(key[0]);
    ZAP_MODE(def->modeWidth, 0x400);
    key[0] = Rand_FloatRange(def->widthKey[0], def->widthKey[0] + def->widthKeyRange[0]);
    key[1] = Rand_FloatRange(def->widthKey[1], def->widthKey[1] + def->widthKeyRange[1]);
    key[2] = Rand_FloatRange(def->widthKey[2], def->widthKey[2] + def->widthKeyRange[2]);
    if (def->flags & 2) {
        key[1] = (key[0] + key[2]) * 0.5f;
    }
    {
        f32 life = line->life;

        line->widthStep[0] = (key[1] - key[0]) / (life * def->split);
        line->widthStep[1] = (key[2] - key[1]) / (life * (1.0f - def->split));
        line->width = key[0];
    }
    {
        EftAcVec colEnd = { 0.0f, 0.0f, 0.0f, 0.0f };

        ZAP_MODE(def->modeColor, 0x100);
        line->color0.x = Rand_FloatRange(w->curA.x, w->curA.x + w->curB.x);
        line->color0.y = Rand_FloatRange(w->curA.y, w->curA.y + w->curB.y);
        line->color0.z = Rand_FloatRange(w->curA.z, w->curA.z + w->curB.z);
        line->color0.w = Rand_FloatRange(w->curA.w, w->curA.w + w->curB.w);
        colEnd.x = Rand_FloatRange(w->curC.x, w->curC.x + w->curD.x);
        colEnd.y = Rand_FloatRange(w->curC.y, w->curC.y + w->curD.y);
        colEnd.z = Rand_FloatRange(w->curC.z, w->curC.z + w->curD.z);
        colEnd.w = 0.0f;
        Vec4_Clamp(V(&line->color0), V(&line->color0), 0.0f, 255.0f);
        Vec4_Clamp(V(&colEnd), V(&colEnd), 0.0f, 255.0f);
        Vec4_Copy(V(&line->color), V(&line->color0));
        line->color.w = 0.0f;
        Vec3_Sub(V(&line->colorStep), V(&colEnd), V(&line->color));
    }
    if (def->flags & 0x20) {
        line->tintTime = w->tintTime.cur * 30.0f;
        line->tintFrame = 0.0f;
        if (line->tintTime <= 0.0f) {
            return 0;
        }
        line->tintStep[0] = w->tintR.cur[1] - w->tintR.cur[0];
        line->tintStep[1] = w->tintG.cur[1] - w->tintG.cur[0];
        line->tintStep[2] = w->tintB.cur[1] - w->tintB.cur[0];
        line->tint[0] = w->tintR.cur[0];
        line->tint[1] = w->tintG.cur[0];
        line->tint[2] = w->tintB.cur[0];
    }
    switch (def->facing) {
    case 0:
        line->axis.x = cosf(line->angle) * line->radius * w->arg.size;
        line->axis.y = sinf(line->angle) * line->radius * w->arg.size;
        line->axis.z = 0.0f;
        line->axis.w = 1.0f;
        break;
    case 1:
        line->tilt = EftMath_WrapAngle(Rand_FloatRange(def->tiltMin, def->tiltMax) * 3.14159265f);
        line->axis.x = cosf(line->angle) * (line->radius * cosf(line->tilt)) * w->arg.size;
        line->axis.y = sinf(line->angle) * (line->radius * cosf(line->tilt)) * w->arg.size;
        line->axis.z = sinf(line->tilt) * line->radius * w->arg.size;
        line->axis.w = 1.0f;
        break;
    }
    Vec3_Normalize(V(&line->axisN), V(&line->axis));
    for (i = 0; i < line->numPts; i++) {
        if (!EftZap_AddNode(line)) {
            EftZapPt *pt;

            for (pt = line->head; pt != NULL; pt = pt->next) {
                pt->flags = 0;
                EFT_LIST_REMOVE(EftZapPt, line, pt);
            }
            return 0;
        }
    }
    return 1;
}
#else
LIT4_WORD(D_002FCED0, 0x40490FDA); /* 3.14159265f */
LIT4_WORD(D_002FCED4, 0x40C90FDA); /* 6.2831853f */
LIT4_WORD(D_002FCED8, 0x40490FDA); /* 3.14159265f */
INCLUDE_ASM("asm/nonmatchings/battle/eft_ab_c", EftZap_InitLine);
#endif

/* Steps every line of the emitter: start delay, the four animated values by key segment, the angle, the axis
   point (radius * cos / sin of the angle, scaled by the emitter's size), the line's matrix (pitch and yaw of the
   emitter's direction, translated to its origin), the width keys, the colour and the alpha (fade-in, fade-out,
   emitter fade), the visible flag; then shifts the points (the head takes the new axis point, each point its
   predecessor's place) and frees the line when its life is over or it was flagged dead. */
void EftZap_UpdateLines(EftZapWork *w) {
    EftZapInit *arg = &w->arg;
    EftZapDef *def = arg->def;
    f32 t = 0.0f;
    EftAcVec tmp = { 0.0f, 0.0f, 0.0f, 0.0f };
    EftAcVec head = { 0.0f, 0.0f, 0.0f, 0.0f };
    f32 wk[3] = { 1.0f, 1.0f, 1.0f };
    f32 pitch = 0.0f;
    f32 fade = 1.0f;
    f32 yaw = 0.0f;
    EftZapStrand *line;
    EftZapPt *pt;
    f32 a;
    f32 b;
    f32 c;
    f32 k;

    switch (def->facing) {
    case 0:
        pitch = EftMath_WrapAngle(Mathf_Asin(-arg->dir.y));
        yaw = EftMath_WrapAngle(atan2f(arg->dir.x, arg->dir.z));
        break;
    case 1:
        pitch = EftMath_WrapAngle(Mathf_Asin(-arg->dir.y) + def->dirOfs[0] * 3.14159265f);
        yaw = EftMath_WrapAngle(atan2f(arg->dir.x, arg->dir.z) + def->dirOfs[1] * 3.14159265f);
        break;
    }
    for (line = w->head; line != NULL; line = line->next) {
        if (line->delay > 0.0f) {
            line->delay -= 1.0f;
            continue;
        }
        t = ZAP_CLAMP01(line->age / line->life);
        if (t < arg->def->split) {
            if (line->flags & 0x1000) {
                line->spin += line->spinStep[0];
            }
            if (line->flags & 0x800) {
                line->radius += line->radiusStep[0];
            }
            if (line->flags & 0x200) {
                line->rise += line->riseStep[0];
            }
            if (line->flags & 0x400) {
                line->width += line->widthStep[0];
            }
        } else {
            if (line->flags & 0x1000) {
                line->spin += line->spinStep[1];
            }
            if (line->flags & 0x800) {
                line->radius += line->radiusStep[1];
            }
            if (line->flags & 0x200) {
                line->rise += line->riseStep[1];
            }
            if (line->flags & 0x400) {
                line->width += line->widthStep[1];
            }
        }
        if (line->flags & 0x2000) {
            f32 d = -line->spin;

            line->angle += d;
        } else {
            line->angle += line->spin;
        }
        line->angle = EftMath_WrapAngle(line->angle);
        switch (def->facing) {
        case 0:
            line->axis.x = cosf(line->angle) * line->radius * arg->size;
            line->axis.y = sinf(line->angle) * line->radius * arg->size;
            line->axis.z += line->rise;
            line->axis.w = 1.0f;
            break;
        case 1:
            line->axis.x = cosf(line->angle) * (line->radius * cosf(line->tilt)) * arg->size;
            line->axis.y = sinf(line->angle) * (line->radius * cosf(line->tilt)) * arg->size;
            line->axis.z = sinf(line->tilt) * line->radius * arg->size;
            line->axis.w = 1.0f;
            break;
        }
        Vec4_Copy(V(&head), V(&line->axis));
        Mtx_RotateX(&line->mtx, &gEftZapMgr->identity, pitch);
        Mtx_RotateY(&line->mtx, &line->mtx, yaw);
        Mtx_Translate(&line->mtx, &line->mtx, V(&w->origin));
        k = 0.0f;
        if (def->flags & 0x20) {
            k = ZAP_CLAMP01(line->tintFrame / line->tintTime);
            wk[0] = line->tint[0] + line->tintStep[0] * k;
            wk[1] = line->tint[1] + line->tintStep[1] * k;
            wk[2] = line->tint[2] + line->tintStep[2] * k;
            if (!(line->flags & 0x10000)) {
                line->tintFrame += 1.0f;
            } else {
                line->tintFrame += -1.0f;
            }
            if (line->tintFrame < 0.0f || line->tintFrame >= line->tintTime) {
                line->flags ^= 0x10000;
                line->tintFrame =
                    line->tintFrame < 0.0f ? 0.0f : (line->tintTime < line->tintFrame ? line->tintTime : line->tintFrame);
            }
        } else {
            wk[0] = 1.0f;
            wk[1] = 1.0f;
            wk[2] = 1.0f;
        }
        b = 1.0f;
        a = 0.0f;
        if (t < arg->def->fadeIn) {
            if (line->fadeInTime > 0.0f) {
                a = line->age / line->fadeInTime;
            } else {
                a = b;
            }
        } else {
            a = b;
        }
        if (arg->def->fadeOut <= t) {
            if (line->fadeOutTime > 0.0f) {
                a = 1.0f - (line->age - (line->life - line->fadeOutTime)) / line->fadeOutTime;
            } else {
                a = 0.0f;
            }
        }
        if (w->flags & EFT_ZAPF_20) {
            b = ZAP_CLAMP01(line->fadeLeft / line->fadeOutTime);
            line->fadeLeft -= 1.0f;
            if (line->fadeLeft <= 0.0f) {
                line->flags |= 8;
            }
        }
        if (line->colorTime > 0.0f) {
            c = ZAP_CLAMP01(line->colorFrame / line->colorTime);
        } else {
            c = 1.0f;
        }
        if (line->flags & 0x100) {
            line->color.x = (line->color0.x + line->colorStep.x * c) * wk[0];
            line->color.y = (line->color0.y + line->colorStep.y * c) * wk[1];
            line->color.z = (line->color0.z + line->colorStep.z * c) * wk[2];
        } else {
            line->color.x = line->color0.x * c * wk[0];
            line->color.y = line->color0.y * c * wk[1];
            line->color.z = line->color0.z * c * wk[2];
        }
        if (line->colorStart <= t && t < line->colorEnd) {
            line->colorFrame += 1.0f;
        }
        line->color.w = line->color0.w * a * b * fade;
        if (line->color.w <= 0.0f && (w->flags & EFT_ZAPF_20)) {
            line->flags |= 8;
        }
        if (line->color.w > 0.0f && line->width > 0.0f) {
            line->flags |= 0x80;
        } else {
            line->flags &= ~0x80;
        }
        for (pt = line->head; pt != NULL; pt = pt->next) {
            Vec4_Copy(V(&tmp), V(&pt->pos));
            Vec4_Copy(V(&pt->pos), V(&head));
            Vec4_Copy(V(&head), V(&tmp));
        }
        line->age += 1.0f;
        if (line->life <= line->age || (line->flags & 8)) {
            for (pt = line->head; pt != NULL; pt = pt->next) {
                pt->flags = 0;
                EFT_LIST_REMOVE(EftZapPt, line, pt);
            }
            line->flags = 0;
            EFT_LIST_REMOVE(EftZapStrand, w, line);
        }
    }
}

/* Queues one gouraud textured quad (4-vertex strip) of the current matrix: projects the four points with their
   texture coordinates (Vu0Cur_ProjectPointsStq), converts the colour to bytes and links a 0x90-byte GS packet into the
   ordering table at the mean depth of the four points (>> 10), in layer `layer` (2 and 3 = layers 0 and 1 with GS
   context 2). With `front` the vertices are drawn at the nearest depth. Nothing is drawn when the projection
   clips the quad. */
#if 0
/* NON-MATCHING: 19 of 274 instructions differ (was 45). The packet header now matches: `regs` written as
   `K + (ctx << 4)` (int shift, constant first), the form of the guide's GS packet header lesson. What is left is
   behind the Vec4_ToInt call: the original loads the bit-field constant 0xFFFFFF (a1) first, then the first colour
   byte, `l = layer`, the 0xFF000000FFFFFFFF mask (a0), -1 (a2), and computes `texIdx << 4` last (behind the layer
   adjustment); here the shift comes first and the two masks are in a0 / a1 the other way round (11 lines there
   plus the 8 `and` that use them). Moving the texture pointer behind the layer adjustment, or reading it inline
   at the tex0 store, puts the shift right but exchanges tex / texIdx in s5 / s6 (21). */
void EftZap_DrawQuad(EftAcVec *pos, EftAcVec color, f32 u0, f32 v0, f32 u1, f32 v1, f32 u2, f32 v2, f32 u3, f32 v3,
                     s32 layer, s32 texIdx, s32 front, EftAcTex *tex) {
    EftAcVec uv[4];
    EftAcVec stq[4];
    EftAcScr scr[4];
    EftAcScr icol;
    EftAcQuadPkt *p;
    EftAcOtEntry *e;
    s32 abe = 1;
    s32 ctx;
    s32 l;
    s32 z;
    u64 *t;

    Vec4_Set(V(&uv[0]), u0, v0, 1.0f, 1.0f);
    Vec4_Set(V(&uv[1]), u1, v1, 1.0f, 1.0f);
    Vec4_Set(V(&uv[2]), u2, v2, 1.0f, 1.0f);
    Vec4_Set(V(&uv[3]), u3, v3, 1.0f, 1.0f);
    if (!Vu0Cur_ProjectPointsStq(scr, V(stq), V(pos), V(uv), 4)) {
        return;
    }
    p = (EftAcQuadPkt *)gOtCur;
    gOtCur = (u8 *)(p + 1);
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
    if (front) {
        scr[0].z = 0xFFFFFF;
        scr[1].z = 0xFFFFFF;
        scr[2].z = 0xFFFFFF;
        scr[3].z = 0xFFFFFF;
    }
    Vec4_ToInt(&icol, V(&color));
    t = (u64 *)((u8 *)tex + (texIdx << 4));
    l = layer;
    if (l >= 2) {
        l -= 2;
    }
    p->v[0].rgbaq.r = icol.x;
    p->v[0].rgbaq.g = icol.y;
    p->v[0].rgbaq.b = icol.z;
    p->v[0].rgbaq.a = icol.w;
    p->v[0].rgbaq.q = stq[0].z;
    p->v[1].rgbaq.r = icol.x;
    p->v[1].rgbaq.g = icol.y;
    p->v[1].rgbaq.b = icol.z;
    p->v[1].rgbaq.a = icol.w;
    p->v[1].rgbaq.q = stq[1].z;
    p->v[2].rgbaq.r = icol.x;
    p->v[2].rgbaq.g = icol.y;
    p->v[2].rgbaq.b = icol.z;
    p->v[2].rgbaq.a = icol.w;
    p->v[2].rgbaq.q = stq[2].z;
    p->v[3].rgbaq.r = icol.x;
    p->v[3].rgbaq.g = icol.y;
    p->v[3].rgbaq.b = icol.z;
    p->v[3].rgbaq.a = icol.w;
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
    p->tex0 = *t;
    if (z < 0) {
        e = &gOtZ[0].layer[l];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[l];
    } else {
        e = &gOtZ[z].layer[l];
    }
    e->tail->next = p;
    e->tail = (EftAcOtPrim *)p;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/eft_ab_c", EftZap_DrawQuad);
#endif
