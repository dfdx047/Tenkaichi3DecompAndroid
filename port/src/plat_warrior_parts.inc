/*
 * PC only, included by plat_mods.c (and by the offline check port/build/model/wp_check.c): model parts of the Custom
 * Character System. Each part of a warrior's look can come from another fighter than its body, and another fighter's
 * part can be layered on top of it: hair, head, face, chest, waist, upper arms, forearms, hands, hips and thighs,
 * shins, feet, accessories; the head, hair, hands and feet can be made larger or smaller; where two parts from
 * different fighters meet, the moved part's open edge is stitched onto its neighbour's.
 *
 * The model: a header of 0x90 bytes (+8 record count, +0x4C face texture, +0x4D fade texture, +0x4E/+0x4F the base
 * textures [first, count), +0x50/+0x51 the shading ramps [first, count), +0x54 the texture without slot alpha, +0x6C
 * the records' offset, +0x70 the seam section), then one record per node in depth-first order: +0 size, +4 pop (levels
 * back up after it), +6 last, +8 its vertex count, +0xA node, +0x10 the node's pivot A, +0x20 its parent's pivot B,
 * +0x30 A - B (its place from its parent at rest); a mesh record has its bounds at +0x40 (centre) / +0x50 (half sizes,
 * radius) and a DMA chain from +0x60 of VIF batches, each one triangle strip: a header (TEX0 of the picture at words
 * 4..5, TEX0 of the shading ramp at 8..9, the vertex count at 12), then 12 words per vertex (x y z weight, normal, s t
 * 1 0). Vertices are in the model's space: the renderer blends B * (p - pivotB) and A * (p - pivotA) by the weight
 * (src/gs/shaders/vu0.vert), so a vertex moves onto another skeleton by weight * (A' - A) + (1 - weight) * (B' - B).
 * The seam section (include/sys/gfxm_d_c.h ObjSeamSec): triangles of their own welding nodes together, each vertex on
 * one node, one texture for all of them.
 * The nodes (the same on every fighter): 8 / 12 hips, 9 / 13 thighs, 10 / 14 shins, 11 / 15 feet, 16 waist, 17 belly,
 * 18 / 32 the chest's halves, 19 / 33 upper arms, 20 / 34 forearms, 21 / 35 hands (their fingers under them), 46 neck,
 * 47 skull, 48 face. Nodes from 0x47 up are chains (hair, a halo, a cape, a sword), each the fighter's own: they are
 * taken whole, hung from the same node of the body. Hair is the head's chains and, on the neck, skull and face, the
 * strips painted with the textures those chains use (much of a fighter's hair is on the skull).
 *
 * The texture file (include/sys/gfxm_b_b.h TexFile / TexEntry): a header of 0x20 (count, entries' offset in words,
 * the GS blocks of the pictures, of the palettes), 0x40 per texture (packets' offsets in words, their sizes, the
 * blocks each takes, the block offsets, TEX0), then the packets. The pictures are uploaded at their block offset from
 * the fighter's area of 1920 blocks, the palettes in 256, the shading ramps (256-colour palettes read through a fixed
 * gradient, 4 blocks each) in 64 blocks; a batch's TEX0 holds its picture's block offset (bits 0..13) and palette's
 * (37..50), and its ramp's block offset in the ramp TEX0's palette bits.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { WP_HAIR, WP_HEAD, WP_FACE, WP_CHEST, WP_WAIST, WP_UPPERARMS, WP_FOREARMS, WP_HANDS, WP_HIPS, WP_SHINS, WP_FEET, WP_EXTRAS, WP_SLOTS };
enum { WP_SIZE_HEAD, WP_SIZE_HAIR, WP_SIZE_HANDS, WP_SIZE_FEET, WP_SIZE_ARMS, WP_SIZE_TORSO, WP_SIZE_LEGS, WP_SIZES };
static const uint8_t wp_empty[16]; /* a slot's donor that has nothing: the part taken away (no hair, no accessories) */
#define WP_LAYER WP_SLOTS            /* origins: slot s's main donor s, its layer WP_LAYER + s, the body WP_BODY */
/* The shading (the shading textures, "ramps") each part from another fighter takes (wp_compose). Entry 2s: slot s's
   part from another fighter, 2s + 1: the layer over slot s. Mode: 0 the shading of what it stands for (a layer: the part
   under it, so SSJ3 hair over SSJ4 hair is shaded as the SSJ4 hair; a part: the body's own part there), 1 its own, 2 the
   skin's, 3 the outfit's; -1 as such entries are (a layer 0, a part 1). An edit whose node is WP_SHADE holds an entry's:
   its piece word the entry (-1: the hair's layer), its x stretch the mode, words 4..11 each of its shading textures (the
   most used first) given by hand: 0 the one in the same place, k the k-th, -1 its own. */
#define WP_SHADE (-2001)
/* A piece copied (the workshop's "duplicate"): an edit whose node is WP_DUP, its piece word the copy's number k (the
   copy is then piece WP_DUP_FIRST + k of its source's node, edited as any piece), x stretch and y stretch the source's
   node and piece. The copy's strips go in a tag of their own at the end of the source's record, marked in the tag's
   address word (unused by CNT / RET): WP_DUP_TAG | k. */
#define WP_DUP (-2002)
#define WP_DUP_FIRST 200
#define WP_DUP_TAG 0x44550000u
#define WP_SH_ENTRIES (WP_SLOTS * 2)
static int wp_shMode[WP_SH_ENTRIES];
static int wp_shPick[WP_SH_ENTRIES][8];
/* The glow of each entry's pictures (the WP_SHADE edit's word [2]): 0 its own (what the fighter it came
   from has), 1 none, 2 soft, 3 bright (a Super Saiyan's hair), 4 blazing; entry WP_SH_ENTRIES the rest of the model's
   (the body's own parts, and every entry left at its own). A picture's glow is its slot's alpha (+0x2D of its entry,
   BtlObjMdl_InitAlphaSlots: the glow pass's strength); pictures sharing a slot glow as the brightest of them. */
#define WP_GLOW_ALL WP_SH_ENTRIES
static int wp_shGlow[WP_SH_ENTRIES + 1];
/* The outline group of each entry's pictures (the WP_SHADE edit's word [2] >> 8): 0 the body's on the same part (one
   surface with it: no line where they meet), 1 one of its own (a line all round it), 2 + g group g (0..15: every part
   given g is one surface, a line where it meets anything else). The group is the picture's slot (+0x2C). */
static int wp_shLine[WP_SH_ENTRIES + 1];
/* The shading made for the warrior (the WP_SHADE edit's word [2] >> 16): its shading ramps generated from each
   picture's own colours (as they are after the colours are painted: wp_shade_regen), so a part shades right whatever
   colour it is. 0: entry WP_GLOW_ALL's (the whole warrior's; for that entry: the game's ramps), 1 cel (two tones), 2
   soft, 3 deep, 4 glow (light, for glowing hair), 5 flat (none), 6 the game's ramps. Its words 4..7: the shadow's size
   (0..4), strength (percent, 0..200), tint (0 auto, 1 cool, 2 warm, 3 the colour in word 7: 0x1000000 | 0xBBGGRR). */
static int wp_shStyle[WP_SH_ENTRIES + 1], wp_shSize[WP_SH_ENTRIES + 1], wp_shStrength[WP_SH_ENTRIES + 1];
static int wp_shTintKind[WP_SH_ENTRIES + 1];
static uint32_t wp_shTintRgb[WP_SH_ENTRIES + 1];
static const uint8_t kWpGlow[5] = {0, 0x00, 0x1E, 0x3C, 0x55};
/* The last wp_compose's shading textures of each entry, for the workshop: [0] what it stands for, [1] its own, the most
   used first, with four colours along each (0xBBGGRR). */
static int wp_shInfoN[WP_SH_ENTRIES][2];
static uint32_t wp_shInfoRgb[WP_SH_ENTRIES][2][8][4];

static int wp_forceCompose; /* (made even with nothing swapped: shading edits on the body's own parts, the scan) */

static void wp_shade_reset(void) {
    memset(wp_shMode, 0xFF, sizeof(wp_shMode));
    memset(wp_shPick, 0, sizeof(wp_shPick));
    memset(wp_shGlow, 0, sizeof(wp_shGlow));
    memset(wp_shLine, 0, sizeof(wp_shLine));
    memset(wp_shStyle, 0, sizeof(wp_shStyle));
    memset(wp_shTintKind, 0, sizeof(wp_shTintKind));
    memset(wp_shTintRgb, 0, sizeof(wp_shTintRgb));
}
static int wp_patchLog; /* (the offline check: each bridge said) */
#define WP_BODY (WP_SLOTS * 2)
/* More layers (any number of hairs, a piece of another fighter's): origins after the body, each over the slot
   wp_xSlot gives (-1 none), taken as that slot's layer is (its shading, glow and outline the layer's) */
#define WP_XLAYERS 4
#define WP_XL (WP_BODY + 1)
#define WP_SHF (WP_XL + WP_XLAYERS) /* the shading fighter (shadefrom=): its model read for its shading and glow only */
#define WP_ORIGINS (WP_SLOTS * 2 + 1 + WP_XLAYERS + 1)
static int wp_xSlot[WP_XLAYERS] = {-1, -1, -1, -1};
static int wp_xTail[WP_XLAYERS]; /* 1: extra layer x is a tail (its slot WP_EXTRAS, only the chains of the waist and hips) */

/* The slot an origin is a layer of (a slot's layer, or one of the more layers), -1 when it is not a layer. */
static int wp_layer_slot(int f) {
    return f >= WP_LAYER && f < WP_BODY ? f - WP_LAYER : f >= WP_XL && f < WP_XL + WP_XLAYERS ? wp_xSlot[f - WP_XL] : -1;
}
#define WP_MAX_RECS 320
#define WP_MAX_TEX 128
#define WP_MAX_BATCHES 6000
#define WP_TEX_ROOM 0x780  /* the fighter's picture blocks */
#define WP_CLUT_ROOM 0x100 /* its palette blocks */
#define WP_RAMP_ROOM 0x40  /* its shading ramps' blocks */
#define WP_NODES 162       /* the object's node table */
#define WP_VIF_BATCH 0x6C058000u
#define WP_PUFF 0.03f      /* how far a layer stands off what it lies on */
/* Layers lifted over the base (the old way: they came out bulkier than the body). Now the base sinks under them. */
static int wp_liftLayers;
static int wp_noStitch;    /* (the offline check: without stitching, to compare) */
static int wp_noAttach;    /* (the offline check: pieces' edits not fixed where they touch, to compare) */

typedef struct WpRec {
    uint32_t off, size;
    int pop, last, node, depth, parent; /* parent: the record's index, -1 none */
} WpRec;

typedef struct WpTex {
    uint32_t pixOfs, clutOfs, pixSize, clutSize, tbpStep, cbpStep, pixBlt, clutBlt, tbpOfs, cbpOfs, unk28, unk2C;
    uint64_t tex0;
} WpTex;

static uint32_t wp_u32(const uint8_t *p) {
    uint32_t v;

    memcpy(&v, p, 4);
    return v;
}

static void wp_put32(uint8_t *p, uint32_t v) {
    memcpy(p, &v, 4);
}

/* The slot a skeleton node's mesh belongs to, -1 the body's always. */
static int wp_slot_of(int node) {
    switch (node) {
    case 46: case 47: return WP_HEAD;
    case 48: return WP_FACE;
    case 17: case 18: case 32: return WP_CHEST;
    case 16: return WP_WAIST;
    case 19: case 33: return WP_UPPERARMS;
    case 20: case 34: return WP_FOREARMS;
    case 8: case 9: case 12: case 13: return WP_HIPS;
    case 10: case 14: return WP_SHINS;
    case 11: case 15: return WP_FEET;
    }
    if ((node >= 21 && node <= 31) || (node >= 35 && node <= 45)) {
        return WP_HANDS;
    }
    return -1;
}

static int wp_head_node(int node) {
    return node == 46 || node == 47 || node == 48;
}

/* The records of a model: their count (0 when it is not one). */
static int wp_records(const uint8_t *m, uint32_t n, WpRec *r, int max) {
    uint32_t o;
    int count = 0, depth = 0, stack[64];

    if (n < 0x90 || wp_u32(m) != 0x6C646D70u) {
        return 0;
    }
    o = wp_u32(m + 0x6C);
    while (count < max && o + 0x40 <= n) {
        uint32_t size = wp_u32(m + o);
        uint16_t h[2];

        if (size < 0x40 || o + size > n || depth < 0 || depth >= 64) {
            return 0;
        }
        memcpy(h, m + o + 4, 4);
        r[count].off = o;
        r[count].size = size;
        r[count].pop = h[0];
        r[count].last = h[1];
        r[count].node = *(const uint16_t *)(m + o + 0xA);
        r[count].depth = depth;
        r[count].parent = depth > 0 ? stack[depth - 1] : -1;
        stack[depth] = count;
        depth += 1 - h[0];
        count++;
        if (h[1]) {
            return count;
        }
        o += size;
    }
    return 0;
}

static int wp_find(const WpRec *r, int count, int node) {
    int i;

    for (i = 0; i < count; i++) {
        if (r[i].node == node) {
            return i;
        }
    }
    return -1;
}

/* Whether a chain whose first node is `root`, hanging from skeleton node `anchor`, is hair (the head's) or an
   accessory (a cape, a sword, anything else; the halo, node 111 on every fighter that has one, too). */
static int wp_chain_slot(int root, int anchor) {
    return root != 111 && wp_head_node(anchor) ? WP_HAIR : WP_EXTRAS;
}

/* The slot of chain record i: by its chain's first node and the skeleton node that hangs from. */
static int wp_chain_slot_of(const WpRec *r, int i) {
    while (r[i].parent >= 0 && r[r[i].parent].node >= 0x47) {
        i = r[i].parent;
    }
    return wp_chain_slot(r[i].node, r[i].parent >= 0 ? r[r[i].parent].node : -1);
}

/* The part a chain of a body part goes with (a sash's tails hang from the waist, a cape from the chest: replacing that
   part takes them away and brings the donor's; Gogeta's tails hang from a root node, the waist's then), -1 none (the
   head's, the halo: hair and accessories as they are).
   root, anchor: the chain's first node and the skeleton node it hangs from. */
static int wp_chain_part(int root, int anchor) {
    return root == 111 || anchor < 0 || anchor >= 0x47 || wp_head_node(anchor) ? -1 : anchor < 8 ? WP_WAIST : wp_slot_of(anchor); /* (the root's:
                                                                                                                       the waist's) */
}

static int wp_chain_part_of(const WpRec *r, int i) {
    while (r[i].parent >= 0 && r[r[i].parent].node >= 0x47) {
        i = r[i].parent;
    }
    return wp_chain_part(r[i].node, r[i].parent >= 0 ? r[r[i].parent].node : -1);
}

/* Calls each(ctx, words of the batch header, vertex words, vertex count) for every batch of a mesh record. */
typedef void (*WpBatchFn)(void *ctx, uint32_t *hdr, float *verts, int count);

static int wp_batches(uint8_t *m, const WpRec *r, WpBatchFn each, void *ctx) {
    uint32_t t = r->off + 0x60, end = r->off + r->size;

    if (r->size <= 0x40) {
        return 1;
    }
    while (t + 16 <= end) {
        uint32_t tag = wp_u32(m + t), qwc = tag & 0xFFFF, id = tag >> 28, p = 0, words = (qwc * 16 + 8) / 4;
        uint32_t *w = (uint32_t *)(m + t + 8);

        if (t + 16 + qwc * 16 > end) {
            return 0;
        }
        while (p + 22 <= words && w[p] == WP_VIF_BATCH) {
            int nv = (int)(w[p + 13] & 0x7FFF);

            if (p + 22 + 12 * (uint32_t)nv + 1 > words) {
                return 0;
            }
            each(ctx, w + p + 1, (float *)(w + p + 22), nv);
            p += 22 + 12 * (uint32_t)nv + 1;
        }
        t += 16 + qwc * 16;
        if (id == 6) {
            break;
        }
    }
    return 1;
}

static int wp_textures(const uint8_t *t, uint32_t n, WpTex *e, int max) {
    uint32_t count, at, i;

    if (n < 0x20) {
        return 0;
    }
    count = wp_u32(t);
    at = wp_u32(t + 4) * 4;
    if (count > (uint32_t)max || at + count * 0x40 > n) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        memcpy(&e[i], t + at + i * 0x40, 0x30);
        memcpy(&e[i].tex0, t + at + i * 0x40 + 0x30, 8);
    }
    return (int)count;
}

static uint32_t wp_tex_key(const uint32_t *h) { /* a batch's picture: its block offsets */
    uint64_t a = (uint64_t)h[4] | ((uint64_t)h[5] << 32);

    return (uint32_t)(a & 0x3FFF) | ((uint32_t)((a >> 37) & 0x3FFF) << 16);
}

/* ---- the textures kept: those the result's meshes use, packed again ---- */
typedef struct WpKeep {
    int from, index;
    uint32_t tbp, cbp;
    uint32_t tint; /* a ramp: coloured by hand, 0x1000000 | 0xBBGGRR (its own copy), 0 as it is */
    uint32_t gen, genRgb; /* a ramp generated (wp_gen_ramp): its style word (0 none), its tint colour */
    int genPic;           /* (the picture it shades: its index in pic[]) */
} WpKeep;

typedef struct WpPack {
    const uint8_t *file[WP_ORIGINS];
    const WpTex *ent[WP_ORIGINS];
    int count[WP_ORIGINS], texCount[WP_ORIGINS], rampCount[WP_ORIGINS];
    WpKeep pic[WP_MAX_TEX], ramp[16];
    int pics, ramps;
    uint32_t tbp, cbp, rampEnd;
    const char *why;
    /* the face texture stays where its fighter had it: the face's mouth and eye sets name its block offsets as they are
       (TEX0 0x140 for most), so anywhere else they draw whatever is there (a hand); the others are packed round it */
    int faceFrom;
    uint32_t faceTbp, faceCbp, faceT1, faceC1;
} WpPack;

/* The first origin with the same file (parts from one fighter share its textures). */
static int wp_same(const WpPack *k, int a) {
    int b;

    for (b = 0; b < WP_ORIGINS; b++) {
        if (k->file[b] == k->file[a]) {
            return b;
        }
    }
    return a;
}

/* Keeps origin `from`'s picture at these block offsets, once; its index in pic[], -1 none. */
static int wp_keep_pic(WpPack *k, int from, uint32_t tbp, uint32_t cbp) {
    int i;

    from = wp_same(k, from);
    for (i = 0; i < k->pics; i++) {
        const WpTex *e = &k->ent[k->pic[i].from][k->pic[i].index];

        if (k->pic[i].from == from && e->tbpOfs == tbp && e->cbpOfs == cbp) {
            return i;
        }
    }
    for (i = 0; i <= k->texCount[from]; i++) {
        const WpTex *e;

        if (i == k->texCount[from]) { /* (none at that place: a strip a mod's model kept from the model it was made from, still
                                         at that model's place, is the picture with its palette - the game draws it from
                                         whatever is there) */
            for (i = 0; i < k->texCount[from] && k->ent[from][i].tbpOfs != tbp; i++) { /* (first the same pixels: its shape
                                                                                         right, whatever palette comes with them) */
            }
            if (i < k->texCount[from]) {
                fprintf(stderr, "bt3: warrior parts: origin %d's strip at tbp %u cbp %u takes the picture at that place (cbp %u)\n", from,
                        (unsigned)tbp, (unsigned)cbp, (unsigned)k->ent[from][i].cbpOfs);
                cbp = k->ent[from][i].cbpOfs;
            } else {
                for (i = 0; i < k->texCount[from] && k->ent[from][i].cbpOfs != cbp; i++) {
                }
                if (i == k->texCount[from]) {
                    break;
                }
                fprintf(stderr, "bt3: warrior parts: origin %d's strip at tbp %u cbp %u takes the picture with that palette (tbp %u)\n", from,
                        (unsigned)tbp, (unsigned)cbp, (unsigned)k->ent[from][i].tbpOfs);
                tbp = k->ent[from][i].tbpOfs;
            }
            {
                int j;

                for (j = 0; j < k->pics; j++) { /* (kept already) */
                    const WpTex *f = &k->ent[k->pic[j].from][k->pic[j].index];

                    if (k->pic[j].from == from && f->tbpOfs == tbp && f->cbpOfs == cbp) {
                        return j;
                    }
                }
            }
        }
        e = &k->ent[from][i];
        if (e->tbpOfs == tbp && e->cbpOfs == cbp) {
            int isFace = k->faceFrom >= 0 && from == wp_same(k, k->faceFrom) && tbp == k->faceTbp && cbp == k->faceCbp;

            if (k->pics >= WP_MAX_TEX || k->pics >= 255) {
                k->why = "Too many textures for one model.";
                return -1;
            }
            k->pic[k->pics].from = from;
            k->pic[k->pics].index = i;
            if (isFace) { /* (where it was) */
                k->pic[k->pics].tbp = k->faceTbp;
                k->pic[k->pics].cbp = k->faceCbp;
                return k->pics++;
            }
            if (k->faceFrom >= 0 && k->tbp < k->faceT1 && k->tbp + e->tbpStep > k->faceTbp) {
                k->tbp = k->faceT1; /* (round the face's place) */
            }
            if (k->faceFrom >= 0 && k->cbp < k->faceC1 && k->cbp + e->cbpStep > k->faceCbp) {
                k->cbp = k->faceC1;
            }
            k->pic[k->pics].tbp = k->tbp;
            k->pic[k->pics].cbp = k->cbp;
            k->tbp += e->tbpStep;
            k->cbp += e->cbpStep;
            if (k->tbp > WP_TEX_ROOM || k->cbp > WP_CLUT_ROOM) {
                k->why = "The parts' textures do not fit in a fighter's texture memory: take parts from fewer fighters.";
                return -1;
            }
            return k->pics++;
        }
    }
    fprintf(stderr, "bt3: warrior parts: origin %d has no texture at tbp %u cbp %u (%d textures)\n", from, (unsigned)tbp, (unsigned)cbp, k->texCount[from]);
    for (i = 0; i < k->texCount[from] && i < 24; i++) {
        fprintf(stderr, "bt3:   its texture %d: tbp %u cbp %u\n", i, (unsigned)k->ent[from][i].tbpOfs, (unsigned)k->ent[from][i].cbpOfs);
    }
    k->why = "A part's texture was not found in its fighter's texture file.";
    return -1;
}

/* Origin `from`'s ramp at this block offset coloured by hand (tint 0x1000000 | 0xBBGGRR): a copy of its own, kept;
   its index in ramp[], the ramp as it is when there is no room for another (or tint is 0). */
static int wp_keep_ramp(WpPack *k, int from, uint32_t tbp);
static int wp_keep_ramp_tint(WpPack *k, int from, uint32_t tbp, uint32_t tint) {
    int i, j;

    if (tint == 0) {
        return wp_keep_ramp(k, from, tbp);
    }
    from = wp_same(k, from);
    for (i = k->texCount[from]; i < k->texCount[from] + k->rampCount[from] && i < k->count[from]; i++) {
        const WpTex *e = &k->ent[from][i];

        if (e->tbpOfs != tbp) {
            continue;
        }
        for (j = 0; j < k->ramps; j++) {
            if (k->ramp[j].from == from && k->ramp[j].index == i && k->ramp[j].tint == tint) {
                return j;
            }
        }
        if (k->ramps >= 16 || k->rampEnd + e->tbpStep > WP_RAMP_ROOM) {
            break; /* (no room: as it is) */
        }
        k->ramp[k->ramps].from = from;
        k->ramp[k->ramps].index = i;
        k->ramp[k->ramps].tint = tint;
        k->ramp[k->ramps].tbp = k->rampEnd;
        k->rampEnd += e->tbpStep;
        return k->ramps++;
    }
    return wp_keep_ramp(k, from, tbp);
}

/* ---- the shading fighter (shadefrom=: a Super Saiyan's shading on a warrior made from a base form): for each part,
   the ramp its strips use most and the brightest glow of its pictures; the warrior's parts take those (wp_compose). ---- */
static int wp_shfOn, wp_shfTb[WP_SLOTS], wp_shfGlow[WP_SLOTS], wp_shfPeak[WP_SLOTS];
static uint32_t wp_pal_main(const uint8_t *pal, int n);

typedef struct WpShfCtx {
    int slot, texCount, rampCount;
    const WpTex *ent;
    const uint32_t *hair;
    int hairs;
    int cnt[WP_SLOTS][16];
    int pic[WP_SLOTS][32]; /* (each part's pictures, how much each is drawn) */
} WpShfCtx;

static int wp_is_hair(const uint32_t *hair, int hairs, uint32_t key);
static void wp_shf_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpShfCtx *c = (WpShfCtx *)ctx;
    uint64_t a = (uint64_t)h[4] | ((uint64_t)h[5] << 32), b = (uint64_t)h[8] | ((uint64_t)h[9] << 32);
    uint32_t tbp = (uint32_t)(a & 0x3FFF), cbp = (uint32_t)((a >> 37) & 0x3FFF), ramp = (uint32_t)((b >> 37) & 0x3FFF);
    int slot = c->slot, t, r;

    (void)v;
    if (c->slot == WP_HEAD && wp_is_hair(c->hair, c->hairs, wp_tex_key(h))) {
        slot = WP_HAIR; /* (the hair painted on the skull) */
    }
    if (slot < 0 || slot >= WP_SLOTS) {
        return;
    }
    for (r = 0; r < c->rampCount && r < 16 && c->ent[c->texCount + r].tbpOfs != ramp; r++) {
    }
    if (r < c->rampCount && r < 16) {
        c->cnt[slot][r] += count;
    }
    for (t = 0; t < c->texCount; t++) {
        if (c->ent[t].tbpOfs == tbp && c->ent[t].cbpOfs == cbp) {
            int g = (int)((c->ent[t].unk2C >> 8) & 0xFF);

            wp_shfGlow[slot] = g > wp_shfGlow[slot] ? g : wp_shfGlow[slot];
            if (t < 32) {
                c->pic[slot][t] += count;
            }
        }
    }
}

static int wp_chain_slot_of(const WpRec *r, int i);
static void wp_shf_scan(const uint8_t *m, const WpRec *rec, int nrec, const WpTex *ent, int texCount, int rampCount, const uint32_t *hair, int hairs,
                        const uint8_t *file, uint32_t fileSize) {
    static WpShfCtx c;
    int j, s, r;

    wp_shfOn = 0;
    for (s = 0; s < WP_SLOTS; s++) {
        wp_shfTb[s] = -1;
        wp_shfGlow[s] = -1;
    }
    if (m == NULL || nrec <= 0) {
        return;
    }
    memset(&c, 0, sizeof(c));
    c.ent = ent;
    c.texCount = texCount;
    c.rampCount = rampCount;
    c.hair = hair;
    c.hairs = hairs;
    for (j = 0; j < nrec; j++) {
        c.slot = rec[j].node < 0x47 ? wp_slot_of(rec[j].node) : wp_chain_slot_of(rec, j);
        if (rec[j].node == 46 || rec[j].node == 47) {
            c.slot = WP_HEAD;
        }
        wp_batches((uint8_t *)m, &rec[j], wp_shf_batch, &c);
    }
    for (s = 0; s < WP_SLOTS; s++) {
        int best = -1;

        for (r = 0; r < rampCount && r < 16; r++) {
            if (c.cnt[s][r] > 0 && (best < 0 || c.cnt[s][r] > c.cnt[s][best])) {
                best = r;
            }
        }
        wp_shfTb[s] = best >= 0 ? (int)ent[texCount + best].tbpOfs : -1;
        wp_shfPeak[s] = -1;
        if (best >= 0 && file != NULL) { /* how much darker its shading makes its main colour, at most (0..255) */
            int bp = -1, t, j, top = 0, lum;
            const WpTex *re = &ent[texCount + best];
            uint32_t base;

            for (t = 0; t < texCount && t < 32; t++) {
                if (c.pic[s][t] > 0 && (bp < 0 || c.pic[s][t] > c.pic[s][bp])) {
                    bp = t;
                }
            }
            if (bp >= 0 && ent[bp].clutOfs != 0 && ent[bp].clutOfs * 4 + 0x60 + 64 <= fileSize && re->pixOfs * 4 + 0x60 + 1024 <= fileSize) {
                base = wp_pal_main(file + ent[bp].clutOfs * 4 + 0x60, 16);
                lum = (int)((base & 0xFF) * 30 + ((base >> 8) & 0xFF) * 59 + ((base >> 16) & 0xFF) * 11) / 100;
                for (j = 0; j < 256; j++) {
                    const uint8_t *p = file + re->pixOfs * 4 + 0x60 + j * 4;
                    int l = (p[0] * 30 + p[1] * 59 + p[2] * 11) / 100;

                    top = l > top ? l : top;
                }
                wp_shfPeak[s] = lum > 8 ? (top * 255 * 4 / 5 / lum > 140 ? 140 : top * 255 * 4 / 5 / lum) : -1; /* (softer, never past
                                                                                                                       55% darker) */
            }
        }
    }
    for (s = 0; s < WP_SLOTS; s++) { /* (a part it does not have: as its chest, else its head) */
        if (wp_shfTb[s] < 0) {
            wp_shfTb[s] = wp_shfTb[WP_CHEST] >= 0 ? wp_shfTb[WP_CHEST] : wp_shfTb[WP_HEAD];
        }
    }
    wp_shfOn = 1;
}

/* The part a strip is shaded as: its entry's slot, else its node's (the body's own), a layer's slot. */
static int wp_shf_slot(int e, int f, int node, int hairPic) {
    if (hairPic) {
        return WP_HAIR;
    }
    if (e >= 0 && e < WP_SH_ENTRIES) {
        return e / 2;
    }
    if (wp_layer_slot(f) >= 0) {
        return wp_layer_slot(f);
    }
    if (f >= 0 && f < WP_SLOTS) {
        return f;
    }
    return node >= 0 && node < 0x47 ? wp_slot_of(node) : -1;
}

/* ---- shading ramps generated: the game subtracts a fighter's ramp from its picture where the surface turns from the
   light (entry j of 256: how much it faces the light, j / 255; black takes nothing away). A generated ramp takes away
   what turns the picture's main colour `base` into its shadow colour (the style's darkness, tinted), across the
   entries the style's shadow covers. gen: style (bits 0..2) | size << 3 (0..4) | strength << 6 (percent) | tint
   kind << 14. ---- */
static int wp_gen_amount(int style, int size, int j) { /* how much of the shadow at entry j, percent */
    int t = 128 + (size - 2) * 22;

    switch (style) {
    case 1: return j < t ? 100 : 0;                                                    /* cel */
    case 2: return j < t - 48 ? 100 : j < t ? 62 : j < t + 28 ? 26 : 0;                 /* soft */
    case 3: return j < t - 44 ? 125 : j < t ? 100 : j < t + 44 ? 38 : 0;                /* deep */
    case 4: return j < t - 26 ? 100 : 0;                                               /* glow */
    }
    return 0;                                                                         /* flat */
}

/* Style 8 ("the same shading as another part", its size word the other entry): that entry, else e. */
static int wp_sh_src(int e) {
    if (e >= 0 && e < WP_SH_ENTRIES && wp_shStyle[e] == 8) {
        int s = wp_shSize[e];

        return s >= 0 && s < WP_SH_ENTRIES && s != e && wp_shStyle[s] != 8 ? s : -1;
    }
    return e;
}

static uint32_t wp_gen_word(int e) { /* entry e's generated style word, 0 the game's ramps */
    int s = -1, st;

    e = wp_sh_src(e);
    if (e >= 0 && e < WP_SH_ENTRIES && ((wp_shStyle[e] >= 1 && wp_shStyle[e] <= 6) || wp_shStyle[e] == 7)) {
        s = e;
    } else if ((wp_shStyle[WP_GLOW_ALL] >= 1 && wp_shStyle[WP_GLOW_ALL] <= 5) || wp_shStyle[WP_GLOW_ALL] == 7) {
        s = WP_GLOW_ALL;
    }
    if (s >= 0 && wp_shStyle[s] == 7) { /* (the game's shading, the colour one's own) */
        return 7u | 100u << 6;
    }
    if (s < 0 || wp_shStyle[s] == 6) {
        return 0;
    }
    st = wp_shStrength[s] < 0 ? 0 : wp_shStrength[s] > 200 ? 200 : wp_shStrength[s];
    return (uint32_t)wp_shStyle[s] | (uint32_t)(wp_shSize[s] < 0 ? 0 : wp_shSize[s] > 4 ? 4 : wp_shSize[s]) << 3 | (uint32_t)st << 6 |
           (uint32_t)(wp_shTintKind[s] & 3) << 14;
}

static uint32_t wp_gen_rgb(int e) {
    int e2 = wp_sh_src(e);
    int s = e2 >= 0 && e2 < WP_SH_ENTRIES && ((wp_shStyle[e2] >= 1 && wp_shStyle[e2] <= 5) || wp_shStyle[e2] == 7) ? e2 : WP_GLOW_ALL;

    return wp_shTintKind[s] == 3 || wp_shStyle[s] == 7 ? wp_shTintRgb[s] & 0xFFFFFF : 0;
}

/* Ramp p (256 RGBA) generated for a picture whose main colour is base (0xBBGGRR). */
static void wp_gen_ramp(uint8_t *p, uint32_t gen, uint32_t rgb, uint32_t base) {
    static const int kKeep[6] = {100, 60, 66, 45, 82, 100}; /* the shadow's lightness, percent of the colour */
    int style = (int)(gen & 7), size = (int)((gen >> 3) & 7), str = (int)((gen >> 6) & 0xFF), tk = (int)((gen >> 14) & 3);
    int c[3], tg[3], s[3], ch, j;

    if (style == 7) { /* the game's shading (where it falls, how strong, as the ramp p has it) in the shadow colour given:
                         what is taken away is what turns the picture's colour into that one */
        int top = 0, a;

        for (j = 0; j < 256; j++) {
            for (ch = 0; ch < 3; ch++) {
                top = p[j * 4 + ch] > top ? p[j * 4 + ch] : top;
            }
        }
        for (ch = 0; ch < 3; ch++) {
            int want = (int)((rgb >> (8 * ch)) & 0xFF), have = (int)((base >> (8 * ch)) & 0xFF);

            s[ch] = tk == 1 ? have * (int)(rgb & 0xFF) / 255 /* (another fighter's pattern: at most this much darker) */
                            : have - want > 0 ? have - want : 0;
        }
        for (j = 0; j < 256; j++) {
            int m = 0;

            for (ch = 0; ch < 3; ch++) {
                m = p[j * 4 + ch] > m ? p[j * 4 + ch] : m;
            }
            a = top > 0 ? m * 256 / top : (j < 128 ? 256 : 0); /* (a ramp with no shade at all: the dark half) */
            for (ch = 0; ch < 3; ch++) {
                int v = s[ch] * a / 256;

                p[j * 4 + ch] = (uint8_t)(v > 255 ? 255 : v);
            }
            p[j * 4 + 3] = 0x80;
        }
        return;
    }

    c[0] = (int)(base & 0xFF), c[1] = (int)((base >> 8) & 0xFF), c[2] = (int)((base >> 16) & 0xFF);
    for (ch = 0; ch < 3; ch++) {
        tg[ch] = c[ch] * kKeep[style < 6 ? style : 0] / 100;
    }
    if (tk == 1 || tk == 0) { /* cool (auto: a little cool, as drawn shadows are) */
        int k = tk == 1 ? 2 : 1;

        tg[0] = tg[0] * (100 - 8 * k) / 100;
        tg[1] = tg[1] * (100 - 4 * k) / 100;
        tg[2] = tg[2] * (100 + 7 * k) / 100 + 3 * k;
    } else if (tk == 2) { /* warm */
        tg[0] = tg[0] * 114 / 100 + 6;
        tg[1] = tg[1] * 94 / 100;
        tg[2] = tg[2] * 78 / 100;
    } else { /* the colour given: its hue at the shadow's lightness, half and half */
        int r = (int)(rgb & 0xFF), g = (int)((rgb >> 8) & 0xFF), b = (int)((rgb >> 16) & 0xFF);
        int lt = tg[0] * 30 + tg[1] * 59 + tg[2] * 11, lc = r * 30 + g * 59 + b * 11;

        lc = lc < 1 ? 1 : lc;
        tg[0] = (tg[0] + r * lt / lc) / 2;
        tg[1] = (tg[1] + g * lt / lc) / 2;
        tg[2] = (tg[2] + b * lt / lc) / 2;
    }
    for (ch = 0; ch < 3; ch++) {
        tg[ch] = tg[ch] < 0 ? 0 : tg[ch] > 255 ? 255 : tg[ch];
        s[ch] = c[ch] - tg[ch];
        s[ch] = (s[ch] < 0 ? 0 : s[ch]) * str / 100;
    }
    for (j = 0; j < 256; j++) {
        int a = wp_gen_amount(style, size, j);

        for (ch = 0; ch < 3; ch++) {
            int v = s[ch] * a / 100;

            p[j * 4 + ch] = (uint8_t)(v > 255 ? 255 : v);
        }
        p[j * 4 + 3] = 0x80;
    }
}

/* A palette's likely main colour (no picture read): the average of its solid colours that are not near black. */
static uint32_t wp_pal_main(const uint8_t *pal, int n) {
    int sum[3] = {0, 0, 0}, cnt = 0, i;

    for (i = 0; i < n; i++) {
        const uint8_t *c = pal + i * 4;

        if (c[3] >= 0x40 && c[0] * 30 + c[1] * 59 + c[2] * 11 >= 4000) {
            sum[0] += c[0], sum[1] += c[1], sum[2] += c[2], cnt++;
        }
    }
    if (cnt == 0) {
        return 0x909090;
    }
    return (uint32_t)(sum[0] / cnt) | (uint32_t)(sum[1] / cnt) << 8 | (uint32_t)(sum[2] / cnt) << 16;
}

/* A generated ramp for picture `pic`, made from origin `from`'s ramp at this block offset (its size); the same one
   kept already if there is; when there is no room, one of the same style for another picture, else the ramp as it is. */
static int wp_keep_ramp_gen(WpPack *k, int from, uint32_t tbp, int pic, uint32_t gen, uint32_t rgb) {
    int i, j;

    from = wp_same(k, from);
    for (j = 0; j < k->ramps; j++) {
        if (k->ramp[j].gen == gen && k->ramp[j].genRgb == rgb && k->ramp[j].genPic == pic) {
            return j;
        }
    }
    for (i = k->texCount[from]; i < k->texCount[from] + k->rampCount[from] && i < k->count[from]; i++) {
        const WpTex *e = &k->ent[from][i];

        if (e->tbpOfs != tbp || e->pixSize < 0x60 + 1024) {
            continue;
        }
        if (k->ramps >= 16 || k->rampEnd + e->tbpStep > WP_RAMP_ROOM) {
            break;
        }
        k->ramp[k->ramps].from = from;
        k->ramp[k->ramps].index = i;
        k->ramp[k->ramps].tint = 0;
        k->ramp[k->ramps].gen = gen;
        k->ramp[k->ramps].genRgb = rgb;
        k->ramp[k->ramps].genPic = pic;
        k->ramp[k->ramps].tbp = k->rampEnd;
        k->rampEnd += e->tbpStep;
        return k->ramps++;
    }
    for (j = 0; j < k->ramps; j++) { /* (no room: another picture's of the same style) */
        if (k->ramp[j].gen == gen && k->ramp[j].genRgb == rgb) {
            return j;
        }
    }
    return wp_keep_ramp(k, from, tbp);
}

/* A ramp's colours (256 RGBA, its first entries light to dark, black after) turned to `tint` (0xBBGGRR): the first is
   the colour, the others as much darker than it as they were than theirs. */
static void wp_tint_ramp(uint8_t *p, uint32_t tint) {
    float tr = (float)(tint & 0xFF) / 255.0f, tg = (float)((tint >> 8) & 0xFF) / 255.0f, tb = (float)((tint >> 16) & 0xFF) / 255.0f;
    float light = 0.0f;
    int j, c, g = (int)((tint >> 25) & 15); /* (its gradient: 1 the softest, 5 or 0 as the game's, 9 the hardest) */
    float a = g >= 1 && g <= 9 ? (float)(g - 5) / 4.0f : 0.0f;

    for (j = 0; j < 256; j++) { /* (the lightest of the coloured ones) */
        float v = (float)(p[j * 4] > p[j * 4 + 1] ? (p[j * 4] > p[j * 4 + 2] ? p[j * 4] : p[j * 4 + 2]) : (p[j * 4 + 1] > p[j * 4 + 2] ? p[j * 4 + 1] : p[j * 4 + 2]));

        light = v > light ? v : light;
    }
    for (j = 0; j < 256 && light > 0.0f; j++) {
        float v = (float)(p[j * 4] > p[j * 4 + 1] ? (p[j * 4] > p[j * 4 + 2] ? p[j * 4] : p[j * 4 + 2]) : (p[j * 4 + 1] > p[j * 4 + 2] ? p[j * 4 + 1] : p[j * 4 + 2])) / light;
        float out[3];

        if ((p[j * 4] | p[j * 4 + 1] | p[j * 4 + 2]) == 0) {
            continue;
        }
        if (a > 0.0f) { /* (harder: the shades fall darker sooner) */
            v = v * (1.0f - a) + v * v * a;
        } else if (a < 0.0f) { /* (softer: they stay lighter longer) */
            v = v * (1.0f + a) + (2.0f * v - v * v) * -a;
        }
        out[0] = tr * v, out[1] = tg * v, out[2] = tb * v;
        for (c = 0; c < 3; c++) {
            int x = (int)(out[c] * 255.0f + 0.5f);

            p[j * 4 + c] = (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x);
        }
    }
}

/* Four colours along origin `from`'s shading ramp at this block offset (0xBBGGRR), black when not found. */
static void wp_ramp_rgb(const WpPack *k, int from, uint32_t tbp, uint32_t *out) {
    int i, j;

    out[0] = out[1] = out[2] = out[3] = 0;
    for (i = k->texCount[from]; i < k->texCount[from] + k->rampCount[from] && i < k->count[from]; i++) {
        const WpTex *e = &k->ent[from][i];

        if (e->tbpOfs == tbp && e->pixOfs != 0) {
            const uint8_t *p = k->file[from] + e->pixOfs * 4 + 0x60;
            int last = 0;

            for (j = 0; j < 256; j++) { /* (its colours are its first entries: light, then dark; black after) */
                last = p[j * 4] | p[j * 4 + 1] | p[j * 4 + 2] ? j : last;
            }
            for (j = 0; j < 4; j++) {
                const uint8_t *c = p + (last * j / 3) * 4;

                out[j] = (uint32_t)c[0] | (uint32_t)c[1] << 8 | (uint32_t)c[2] << 16;
            }
            return;
        }
    }
}

/* Keeps origin `from`'s shading ramp at this block offset: the same ramp kept already if there is one (or, when 16
   are, the nearest); its index in ramp[], -1 none. */
static int wp_rampShort; /* (ramps that found no room and borrowed another: Port_WarriorRampShort) */

static int wp_keep_ramp(WpPack *k, int from, uint32_t tbp) {
    int i;

    from = wp_same(k, from);
    for (i = 0; i < k->ramps; i++) {
        if (k->ramp[i].from == from && k->ramp[i].tint == 0 && k->ramp[i].gen == 0 && k->ent[from][k->ramp[i].index].tbpOfs == tbp) {
            return i;
        }
    }
    for (i = k->texCount[from]; i < k->texCount[from] + k->rampCount[from] && i < k->count[from]; i++) {
        const WpTex *e = &k->ent[from][i];

        if (e->tbpOfs == tbp) {
            const uint8_t *mine = k->file[from] + e->pixOfs * 4 + 0x60;
            int best = -1, j, c;
            uint32_t bestD = 0xFFFFFFFFu;

            for (j = 0; j < k->ramps && e->pixOfs != 0; j++) {
                const WpTex *f = &k->ent[k->ramp[j].from][k->ramp[j].index];

                if (k->ramp[j].tint != 0 || k->ramp[j].gen != 0) {
                    continue; /* (one coloured by hand, or made for a picture, is no one else's) */
                }
                const uint8_t *theirs = k->file[k->ramp[j].from] + f->pixOfs * 4 + 0x60;
                uint32_t d = 0;

                if (f->pixOfs == 0 || f->pixSize != e->pixSize) {
                    continue;
                }
                for (c = 0; c < 256 * 4; c++) {
                    d += (uint32_t)(mine[c] > theirs[c] ? mine[c] - theirs[c] : theirs[c] - mine[c]);
                }
                if (d < bestD) {
                    bestD = d;
                    best = j;
                }
            }
            if (best >= 0 && (bestD == 0 || k->ramps >= 16 || k->rampEnd + e->tbpStep > WP_RAMP_ROOM)) {
                wp_rampShort += bestD != 0;
                return best;
            }
            if ((k->ramps >= 16 || k->rampEnd + e->tbpStep > WP_RAMP_ROOM) && k->ramps > 0) {
                wp_rampShort++;
                return 0; /* (no room left: a ramp there is, rather than no model) */
            }
            if (k->ramps >= 16 || k->rampEnd + e->tbpStep > WP_RAMP_ROOM) {
                k->why = "Too many shading ramps for one model (16 at most): take parts from fewer fighters.";
                return -1;
            }
            k->ramp[k->ramps].from = from;
            k->ramp[k->ramps].index = i;
            k->ramp[k->ramps].tint = 0;
            k->ramp[k->ramps].tbp = k->rampEnd;
            k->rampEnd += e->tbpStep;
            return k->ramps++;
        }
    }
    k->why = "A part's shading ramp was not found in its fighter's texture file.";
    return -1;
}

/* ---- the result's records, strip by strip ---- */
typedef struct WpBatch {
    const uint32_t *h; /* the 20 words of its header */
    const float *v;    /* its vertices, 12 words each */
    float *own;        /* (a layer's vertices, moved and lifted already: malloc'ed) */
    int n, from;
    float dA[3], dB[3], puff; /* puff: a layer's (it lies over the base) */
    float pA[3], pB[3], s;    /* fitted: its vertices `s` times as far from its own pivots (pA, pB: the donor's) along */
    float g, u[3], c[3];      /* `g` times as far across its bone (the line through `c` along `u`: the donor's) */
    float uB[3], cB[3], sB;   /* (its parent's bone and stretch, which its vertices weighted to pivot B follow) */
    int even;                 /* a hand or a foot: made larger or smaller evenly about its joints by `s` (pulled to the
                                 skeleton, its fingers were drawn to the wrist's point: spikes) */
} WpBatch;

typedef struct WpOut {
    const uint8_t *head; /* the record whose header (pivots) it has */
    float pd[3];         /* (a donor's chain: its pivots moved by this) */
    int depth, node, from, b0, nb;
} WpOut;

typedef struct WpBuild {
    WpBatch bat[WP_MAX_BATCHES];
    int bats;
    WpOut out[WP_MAX_RECS * 2];
    int outs;
    const char *why;
    /* a read pass: which batches of a record to take */
    int from, want;     /* want: 0 all, 1 not the origin's hair, 2 only its hair */
    const uint32_t *hair;
    int hairs;
    float dA[3], dB[3], puff;
    float pA[3], pB[3], s, g, u[3], c[3], uB[3], cB[3], sB;
    int even;
    const uint8_t *fitM; /* the body, which donors' limbs are fitted to (wp_fit) */
    const WpRec *fitR;
    int fitN;
    const float *faceV; /* a hair laid over the hair: its fighter's own face (x y z each), its strips lying on that face left
                           out (SSJ3's brow ridge is in its hair's picture: over another's face it is a second face) */
    int faceN;
} WpBuild;

static int wp_is_hair(const uint32_t *hair, int hairs, uint32_t key) {
    int i;

    for (i = 0; i < hairs; i++) {
        if (hair[i] == key) {
            return 1;
        }
    }
    return 0;
}

static void wp_take_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpBuild *b = (WpBuild *)ctx;
    WpBatch *n;
    int hair;

    if (b->want != 0) {
        hair = wp_is_hair(b->hair, b->hairs, wp_tex_key(h));
        if ((b->want == 1 && hair) || (b->want == 2 && !hair)) {
            return;
        }
    }
    if (b->faceN > 0 && count > 0) { /* (on its fighter's face: on average within 0.25 of it) */
        float sum = 0.0f;
        int i, j;

        for (i = 0; i < count; i++) {
            float best = 1e9f;

            for (j = 0; j < b->faceN; j++) {
                float dx = v[i * 12] - b->faceV[j * 3], dy = v[i * 12 + 1] - b->faceV[j * 3 + 1], dz = v[i * 12 + 2] - b->faceV[j * 3 + 2], d = dx * dx + dy * dy + dz * dz;

                best = d < best ? d : best;
            }
            sum += sqrtf(best);
        }
        if (sum / (float)count < 0.25f) {
            return;
        }
    }
    if (b->bats >= WP_MAX_BATCHES) {
        b->why = "Too many pieces for one model.";
        return;
    }
    n = &b->bat[b->bats++];
    n->h = h;
    n->v = v;
    n->own = NULL;
    n->n = count;
    n->from = b->from;
    memcpy(n->dA, b->dA, sizeof(n->dA));
    memcpy(n->dB, b->dB, sizeof(n->dB));
    n->puff = b->puff;
    memcpy(n->pA, b->pA, sizeof(n->pA));
    memcpy(n->pB, b->pB, sizeof(n->pB));
    n->s = b->s;
    n->g = b->g;
    memcpy(n->u, b->u, sizeof(n->u));
    memcpy(n->c, b->c, sizeof(n->c));
    memcpy(n->uB, b->uB, sizeof(n->uB));
    memcpy(n->cB, b->cB, sizeof(n->cB));
    n->sB = b->sB;
    n->even = b->even;
}

/* Each origin's skeleton as bone segments (joint to joint, its rest pose), which fitted clothes are made thinner
   towards (wp_moved): pulled to every bone at once, the nearer ones far more, so the pull changes smoothly where one
   bone gives way to the next (a sleeve onto the shoulder, the trousers onto the hips) and nothing tears. */
#define WP_SEGS 11
static float wp_seg[WP_ORIGINS][WP_SEGS][6];
static int wp_segs[WP_ORIGINS];

static void wp_skeleton(int o, const uint8_t *m, const WpRec *r, int n) {
    static const int pair[WP_SEGS][2] = {{16, 46}, {9, 10}, {10, 11}, {13, 14}, {14, 15}, /* (no bone from the waist to the hips: */
                                         {46, 19}, {46, 33}, {19, 20}, {20, 21}, {33, 34}, {34, 35}}; /* it drew the hips up into points) */
    int k, q;

    wp_segs[o] = 0;
    for (k = 0; m != NULL && k < WP_SEGS; k++) {
        int i = wp_find(r, n, pair[k][0]), j = wp_find(r, n, pair[k][1]);

        if (i < 0 || j < 0) {
            continue;
        }
        for (q = 0; q < 3; q++) {
            wp_seg[o][wp_segs[o]][q] = ((const float *)(m + r[i].off + 0x10))[q];
            wp_seg[o][wp_segs[o]][3 + q] = ((const float *)(m + r[j].off + 0x10))[q];
        }
        wp_segs[o]++;
    }
}

/* Where batch `bb`'s vertex `p` goes: onto the body's pivots, fitted (q: x y z): along its bone `s` times as far
   from its pivots, `g` times as near the skeleton. */
static void wp_moved(const WpBatch *bb, const float *p, float *out) {
    float w = p[3], across[3] = {0.0f, 0.0f, 0.0f}, along = 0.0f, alongB = 0.0f, wsum = 0.0f;
    int q, k;

    if (bb->even) { /* (a hand, a foot: evenly about its pivots) */
        for (q = 0; q < 3; q++) {
            out[q] = p[q] + w * bb->dA[q] + (1.0f - w) * bb->dB[q] + (bb->s - 1.0f) * (p[q] - (w * bb->pA[q] + (1.0f - w) * bb->pB[q]));
        }
        return;
    }
    for (q = 0; q < 3; q++) { /* (along its own bone as much as it follows that, along its parent's for the rest) */
        along += (p[q] - bb->pA[q]) * bb->u[q];
        alongB += (p[q] - bb->pB[q]) * bb->uB[q];
    }
    if (bb->g != 1.0f && bb->from >= 0 && bb->from < WP_ORIGINS) {
        for (k = 0; k < wp_segs[bb->from]; k++) {
            const float *a = wp_seg[bb->from][k], *b = a + 3;
            float ab[3], t = 0.0f, ll = 0.0f, d[3], dd = 0.0f, wt;

            for (q = 0; q < 3; q++) {
                ab[q] = b[q] - a[q];
                t += (p[q] - a[q]) * ab[q];
                ll += ab[q] * ab[q];
            }
            t = ll > 1e-8f ? t / ll : 0.0f;
            t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
            for (q = 0; q < 3; q++) {
                d[q] = p[q] - (a[q] + ab[q] * t);
                dd += d[q] * d[q];
            }
            wt = 1.0f / (dd * dd + 1e-6f);
            for (q = 0; q < 3; q++) {
                across[q] += d[q] * wt;
            }
            wsum += wt;
        }
        for (q = 0; q < 3 && wsum > 0.0f; q++) {
            across[q] = across[q] / wsum * (bb->g - 1.0f);
        }
    }
    for (q = 0; q < 3; q++) {
        out[q] = p[q] + w * bb->dA[q] + (1.0f - w) * bb->dB[q] + w * (bb->s - 1.0f) * along * bb->u[q] + (1.0f - w) * (bb->sB - 1.0f) * alongB * bb->uB[q] +
                 across[q];
    }
}

/* The hair textures of a model: those its hair chains use. */
typedef struct WpHairCtx {
    uint32_t *key;
    int n, max;
} WpHairCtx;

static void wp_hair_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpHairCtx *c = (WpHairCtx *)ctx;
    uint32_t key = wp_tex_key(h);

    (void)v;
    (void)count;
    if (!wp_is_hair(c->key, c->n, key) && c->n < c->max) {
        c->key[c->n++] = key;
    }
}

static int wp_hair_textures(const uint8_t *m, const WpRec *r, int n, uint32_t *key, int max) {
    WpHairCtx c, skin;
    uint32_t skinKey[32];
    int i, j, out = 0;

    c.key = key;
    c.n = 0;
    c.max = max;
    skin.key = skinKey;
    skin.n = 0;
    skin.max = 32;
    for (i = 0; i < n; i++) {
        if (r[i].node >= 0x47 && wp_chain_slot_of(r, i) == WP_HAIR) {
            wp_batches((uint8_t *)m, &r[i], wp_hair_batch, &c);
        } else if (r[i].node == 48 || r[i].node < 46) { /* the face's and the body's textures are skin (not the neck's: a mane is on it) */
            wp_batches((uint8_t *)m, &r[i], wp_hair_batch, &skin);
        }
    }
    for (i = 0; i < c.n; i++) { /* (a chain painted like the skin, as antennae are, is not hair on the skull) */
        if (!wp_is_hair(skinKey, skin.n, key[i])) {
            key[out++] = key[i];
        }
    }
    (void)j;
    return out;
}

/* ---- stitching: a part's open edge snapped onto its neighbour's ---- */
#define WP_STITCH_REACH 0.4f  /* an edge further than this from the neighbour's stays as it is */
#define WP_STITCH_BLEND 0.5f  /* how far into the part the snap fades */
#define WP_WELD 0.002f
#define WP_MAX_PTS 4096

typedef struct WpPts {
    float *at[WP_MAX_PTS]; /* each vertex of the record (its x y z) */
    int id[WP_MAX_PTS];    /* the welded point it is */
    int n;
    float pos[WP_MAX_PTS][3]; /* the welded points */
    int pts;
    uint8_t edge[WP_MAX_PTS]; /* 1: on an open edge */
    int strips[512][2];       /* each strip: first vertex, count */
    int nstrips;
} WpPts;

static void wp_pts_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpPts *p = (WpPts *)ctx;
    int i, j;

    (void)h;
    if (p->nstrips >= 512 || p->n + count > WP_MAX_PTS) {
        return;
    }
    p->strips[p->nstrips][0] = p->n;
    p->strips[p->nstrips][1] = count;
    p->nstrips++;
    for (i = 0; i < count; i++) {
        float *q = v + i * 12;

        p->at[p->n] = q;
        for (j = 0; j < p->pts; j++) {
            if (fabsf(p->pos[j][0] - q[0]) < WP_WELD && fabsf(p->pos[j][1] - q[1]) < WP_WELD && fabsf(p->pos[j][2] - q[2]) < WP_WELD) {
                break;
            }
        }
        if (j == p->pts) {
            memcpy(p->pos[j], q, 12);
            p->pts++;
        }
        p->id[p->n++] = j;
    }
}

/* The record's points and which are on an open edge (an edge of one triangle only). */
static void wp_pts_read(uint8_t *m, const WpRec *r, WpPts *p) {
    static uint32_t ekey[WP_MAX_PTS * 3];
    static uint16_t ecount[WP_MAX_PTS * 3];
    int ne = 0, s, i, k;

    memset(p, 0, sizeof(*p));
    wp_batches(m, r, wp_pts_batch, p);
    for (s = 0; s < p->nstrips; s++) {
        for (i = 0; i + 2 < p->strips[s][1]; i++) {
            int a = p->id[p->strips[s][0] + i], b = p->id[p->strips[s][0] + i + 1], c = p->id[p->strips[s][0] + i + 2];
            int e[3][2], t;

            if (a == b || b == c || a == c) {
                continue; /* (a strip's turn) */
            }
            e[0][0] = a, e[0][1] = b, e[1][0] = b, e[1][1] = c, e[2][0] = a, e[2][1] = c;
            for (t = 0; t < 3; t++) {
                uint32_t key = e[t][0] < e[t][1] ? (uint32_t)e[t][0] << 16 | (uint32_t)e[t][1] : (uint32_t)e[t][1] << 16 | (uint32_t)e[t][0];

                for (k = 0; k < ne && ekey[k] != key; k++) {
                }
                if (k == ne) {
                    if (ne >= WP_MAX_PTS * 3) {
                        continue;
                    }
                    ekey[ne] = key;
                    ecount[ne++] = 0;
                }
                ecount[k]++;
            }
        }
    }
    for (k = 0; k < ne; k++) {
        if (ecount[k] == 1) {
            p->edge[ekey[k] >> 16] = 1;
            p->edge[ekey[k] & 0xFFFF] = 1;
        }
    }
}

/* (the other thigh, when a thigh is stitched onto the waist: where the two meet down the middle is no edge of the
   waist's, and pulling it there pinched a crease between the legs) */
static const WpRec *wp_stitchSib;

/* Snaps record a's open edge onto record b's where they are closest, the snap fading into a. */
static void wp_stitch(uint8_t *m, const WpRec *a, const WpRec *b) {
    static WpPts pa, pb, ps;
    static float move[WP_MAX_PTS][3];
    static uint8_t has[WP_MAX_PTS];
    int i, j, q, any = 0;

    wp_pts_read(m, a, &pa);
    wp_pts_read(m, b, &pb);
    ps.pts = 0;
    if (wp_stitchSib != NULL) {
        wp_pts_read(m, wp_stitchSib, &ps);
    }
    memset(has, 0, sizeof(has));
    for (i = 0; i < pa.pts; i++) {
        float best = WP_STITCH_REACH * WP_STITCH_REACH, to[3] = {0.0f, 0.0f, 0.0f};
        int onSib = 0;

        if (!pa.edge[i]) {
            continue;
        }
        for (j = 0; j < ps.pts && !onSib; j++) { /* (an edge point where the other thigh meets it: left as it is) */
            float dx = ps.pos[j][0] - pa.pos[i][0], dy = ps.pos[j][1] - pa.pos[i][1], dz = ps.pos[j][2] - pa.pos[i][2];

            onSib = dx * dx + dy * dy + dz * dz < WP_STITCH_REACH * WP_STITCH_REACH * 0.0625f;
        }
        if (onSib) {
            continue;
        }
        for (j = 0; j < pb.pts; j++) {
            float d[3], dd;

            if (!pb.edge[j]) {
                continue;
            }
            for (q = 0; q < 3; q++) {
                d[q] = pb.pos[j][q] - pa.pos[i][q];
            }
            dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            if (dd < best) {
                best = dd;
                memcpy(to, d, sizeof(to));
            }
        }
        if (best < WP_STITCH_REACH * WP_STITCH_REACH) {
            memcpy(move[i], to, sizeof(to));
            has[i] = 1;
            any = 1;
        }
    }
    if (!any) {
        return;
    }
    for (i = 0; i < pa.pts; i++) { /* the rest follow the nearest snapped edge points, fading */
        float sum[3] = {0.0f, 0.0f, 0.0f}, wsum = 0.0f, closest = WP_STITCH_BLEND;

        if (has[i]) {
            continue;
        }
        for (j = 0; j < pa.pts; j++) {
            float d, dx, dy, dz, w;

            if (!has[j]) {
                continue;
            }
            dx = pa.pos[j][0] - pa.pos[i][0];
            dy = pa.pos[j][1] - pa.pos[i][1];
            dz = pa.pos[j][2] - pa.pos[i][2];
            d = sqrtf(dx * dx + dy * dy + dz * dz);
            if (d >= WP_STITCH_BLEND) {
                continue;
            }
            closest = d < closest ? d : closest;
            w = 1.0f / (d * d + 0.0004f);
            for (q = 0; q < 3; q++) {
                sum[q] += w * move[j][q];
            }
            wsum += w;
        }
        if (wsum > 0.0f) {
            float f = 1.0f - closest / WP_STITCH_BLEND;

            f *= f;
            for (q = 0; q < 3; q++) {
                move[i][q] = sum[q] / wsum * f;
            }
        } else {
            move[i][0] = move[i][1] = move[i][2] = 0.0f;
        }
    }
    for (i = 0; i < pa.n; i++) {
        for (q = 0; q < 3; q++) {
            pa.at[i][q] += move[pa.id[i]][q];
        }
    }
}

/* A layer lifted over the base it lies on: its points (welded, so a point shared by strips moves as one), each along
   its normal just past the base's triangles in front of it within reach, the lifts smoothed between neighbours (never
   below what clears the base), plus WP_PUFF. `base` / `bases`: the base's triangles. */
#define WP_LIFT_REACH 0.9f
#define WP_LIFT_MAX 0.15f

static float wp_line_t(const float *o, const float *d, const float *t) { /* where the line meets the triangle, 1e9 none */
    float e1[3], e2[3], p[3], q[3], s2[3], det, u, v;
    int i;

    for (i = 0; i < 3; i++) {
        e1[i] = t[3 + i] - t[i];
        e2[i] = t[6 + i] - t[i];
    }
    p[0] = d[1] * e2[2] - d[2] * e2[1], p[1] = d[2] * e2[0] - d[0] * e2[2], p[2] = d[0] * e2[1] - d[1] * e2[0];
    det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (det > -1e-7f && det < 1e-7f) {
        return 1e9f;
    }
    for (i = 0; i < 3; i++) {
        s2[i] = o[i] - t[i];
    }
    u = (s2[0] * p[0] + s2[1] * p[1] + s2[2] * p[2]) / det;
    if (u < -0.02f || u > 1.02f) {
        return 1e9f;
    }
    q[0] = s2[1] * e1[2] - s2[2] * e1[1], q[1] = s2[2] * e1[0] - s2[0] * e1[2], q[2] = s2[0] * e1[1] - s2[1] * e1[0];
    v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
    if (v < -0.02f || u + v > 1.04f) {
        return 1e9f;
    }
    return (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
}

static void wp_lift_layers(WpBatch **lb, int nlb, float (*base)[9], int bases) {
    static float pos[8192][3], nrm[8192][3], lift[8192], need[8192];
    static int id[16384];
    static float *at[16384];
    static uint16_t nb[8192][8];
    static uint8_t nbn[8192];
    int pts = 0, nv = 0, i, j, k, pass;

    for (i = 0; i < nlb; i++) { /* weld */
        for (j = 0; j < lb[i]->n; j++) {
            float *p = lb[i]->own + j * 12, nl = sqrtf(p[4] * p[4] + p[5] * p[5] + p[6] * p[6]);

            if (nv >= 16384) {
                return;
            }
            for (k = 0; k < pts; k++) {
                if (fabsf(pos[k][0] - p[0]) < WP_WELD && fabsf(pos[k][1] - p[1]) < WP_WELD && fabsf(pos[k][2] - p[2]) < WP_WELD) {
                    break;
                }
            }
            if (k == pts) {
                if (pts >= 8192) {
                    return;
                }
                memcpy(pos[k], p, 12);
                nrm[k][0] = nrm[k][1] = nrm[k][2] = 0.0f;
                nbn[k] = 0;
                pts++;
            }
            if (nl > 1e-5f) {
                nrm[k][0] += p[4] / nl, nrm[k][1] += p[5] / nl, nrm[k][2] += p[6] / nl;
            }
            at[nv] = p;
            id[nv++] = k;
        }
    }
    for (i = 0, nv = 0; i < nlb; i++) { /* neighbours, from the strips' triangles */
        for (j = 0; j < lb[i]->n; j++, nv++) {
            int c;

            for (c = 1; c <= 2 && j >= c; c++) {
                int a = id[nv], b2 = id[nv - c], t;

                if (a == b2) {
                    continue;
                }
                for (t = 0; t < nbn[a] && nb[a][t] != b2; t++) {
                }
                if (t == nbn[a] && nbn[a] < 8) {
                    nb[a][nbn[a]++] = (uint16_t)b2;
                }
                for (t = 0; t < nbn[b2] && nb[b2][t] != a; t++) {
                }
                if (t == nbn[b2] && nbn[b2] < 8) {
                    nb[b2][nbn[b2]++] = (uint16_t)a;
                }
            }
        }
    }
    for (k = 0; k < pts; k++) { /* how far each must go */
        float nl = sqrtf(nrm[k][0] * nrm[k][0] + nrm[k][1] * nrm[k][1] + nrm[k][2] * nrm[k][2]), best = 0.0f;

        need[k] = lift[k] = 0.0f;
        if (nl < 1e-5f) {
            continue;
        }
        nrm[k][0] /= nl, nrm[k][1] /= nl, nrm[k][2] /= nl;
        for (j = 0; j < bases; j++) {
            const float *tr = base[j];
            float cx = (tr[0] + tr[3] + tr[6]) / 3.0f - pos[k][0], cy = (tr[1] + tr[4] + tr[7]) / 3.0f - pos[k][1], cz = (tr[2] + tr[5] + tr[8]) / 3.0f - pos[k][2];
            float t;

            if (cx * cx + cy * cy + cz * cz > 4.0f) {
                continue;
            }
            t = wp_line_t(pos[k], nrm[k], tr);
            if (t > best && t < WP_LIFT_REACH) {
                best = t;
            }
        }
        need[k] = lift[k] = best;
    }
    for (pass = 0; pass < 4; pass++) { /* smoothed between neighbours, never below what clears the base */
        for (k = 0; k < pts; k++) {
            float sum = lift[k];

            for (j = 0; j < nbn[k]; j++) {
                sum += lift[nb[k][j]];
            }
            sum /= (float)(nbn[k] + 1);
            lift[k] = sum > need[k] ? sum : need[k];
        }
    }
    for (i = 0; i < nv; i++) {
        float l = (wp_liftLayers || lift[id[i]] < WP_LIFT_MAX ? lift[id[i]] : WP_LIFT_MAX) + WP_PUFF; /* (a little only: the base
                                                                                                         sinks the rest, the layer keeps its size) */

        for (k = 0; k < 3; k++) {
            at[i][k] += nrm[id[i]][k] * l;
        }
    }
}

/* What a layer covers is hidden: a base strip's vertex is covered when the line through it along its normal meets
   one of the layer's triangles close by, in front or behind (a baggier base outside the layer goes too); a covered
   vertex whose triangles are all covered goes onto its neighbour, so those triangles vanish and the base's edge under
   the layer's edge stays. */
#define WP_COVER 0.7f
static float (*sLayer)[9]; /* the layers' triangles */
static int sLayers;

static int wp_line_hits(const float *o, const float *d, const float *t, float *along) {
    float e1[3], e2[3], p[3], q[3], s2[3], det, u, v, w;
    int i;

    for (i = 0; i < 3; i++) {
        e1[i] = t[3 + i] - t[i];
        e2[i] = t[6 + i] - t[i];
    }
    p[0] = d[1] * e2[2] - d[2] * e2[1], p[1] = d[2] * e2[0] - d[0] * e2[2], p[2] = d[0] * e2[1] - d[1] * e2[0];
    det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (det > -1e-7f && det < 1e-7f) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        s2[i] = o[i] - t[i];
    }
    u = (s2[0] * p[0] + s2[1] * p[1] + s2[2] * p[2]) / det;
    if (u < -0.02f || u > 1.02f) {
        return 0;
    }
    q[0] = s2[1] * e1[2] - s2[2] * e1[1], q[1] = s2[2] * e1[0] - s2[0] * e1[2], q[2] = s2[0] * e1[1] - s2[1] * e1[0];
    v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
    if (v < -0.02f || u + v > 1.04f) {
        return 0;
    }
    w = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det; /* (along the line) */
    *along = w;
    return w > -WP_LIFT_REACH && w < WP_COVER; /* (a base further out than the layer by as much as a lift would have cleared: under it) */
}

static float wp_tri_nearest(const float *p, const float *t, float *out);

static void wp_cull(float *v, int n) {
    static uint8_t cov[4096], gone[4096];
    static float was[4096][3], sink[4096][3], dep[4096];
    int i, j, t;

    if (n > 4096 || n < 3) {
        return;
    }
    for (i = 0; i < n; i++) {
        float *p = v + i * 12, nl = sqrtf(p[4] * p[4] + p[5] * p[5] + p[6] * p[6]), d[3], deep = 0.0f;

        cov[i] = 0;
        memcpy(was[i], p, 12);
        sink[i][0] = sink[i][1] = sink[i][2] = 0.0f;
        if (nl < 1e-5f) {
            continue;
        }
        d[0] = p[4] / nl, d[1] = p[5] / nl, d[2] = p[6] / nl;
        for (j = 0; j < sLayers; j++) {
            const float *tr = sLayer[j];
            float cx = (tr[0] + tr[3] + tr[6]) / 3.0f - p[0], cy = (tr[1] + tr[4] + tr[7]) / 3.0f - p[1], cz = (tr[2] + tr[5] + tr[8]) / 3.0f - p[2];
            float w;

            if (cx * cx + cy * cy + cz * cz > 9.0f) {
                continue; /* (far away) */
            }
            if (wp_line_hits(p, d, tr, &w)) {
                cov[i] = 1;
                if (!wp_liftLayers && WP_PUFF - w > deep) {
                    deep = WP_PUFF - w; /* (the base at or over the layer here: it goes under it, the layer keeps its size) */
                }
            }
        }
        for (j = 0; j < 3; j++) {
            sink[i][j] = -d[j] * deep;
        }
        dep[i] = deep;
    }
    for (t = 0; t < 2 && !wp_liftLayers; t++) { /* (spread to the strip's neighbours a little: an edge from a sunk point
                                                     to one beside it does not come out through the layer) */
        for (i = 0; i < n; i++) {
            float *p = v + i * 12, nl2 = sqrtf(p[4] * p[4] + p[5] * p[5] + p[6] * p[6]), most = dep[i];
            int q;

            for (q = i - 2; q <= i + 2; q++) {
                if (q >= 0 && q < n && q != i && dep[q] * 0.6f > most) {
                    most = dep[q] * 0.6f;
                }
            }
            if (most > dep[i] && nl2 > 1e-5f) {
                for (j = 0; j < 3; j++) {
                    sink[i][j] = -p[4 + j] / nl2 * most;
                }
                dep[i] = most;
            }
        }
    }
    for (i = 0; i < n; i++) {
        float *p = v + i * 12;

        for (j = 0; j < 3; j++) {
            p[j] += sink[i][j];
            was[i][j] += sink[i][j];
        }
    }
    for (i = 0; i < n; i++) {
        gone[i] = cov[i];
        for (t = i - 2; t <= i && gone[i]; t++) {
            if (t >= 0 && t + 2 < n && cov[t] + cov[t + 1] + cov[t + 2] < 3) {
                gone[i] = 0;
            }
        }
    }
    for (i = 0; i < n; i++) { /* each covered point onto the nearest kept one of its strip (a run of them all onto one
                                 point, so their triangles vanish; it used to go onto the point before, which in a run
                                 only slid the run along and left it showing) */
        int back = i, fwd = i;

        if (!gone[i]) {
            continue;
        }
        while (back >= 0 && gone[back]) {
            back--;
        }
        while (fwd < n && gone[fwd]) {
            fwd++;
        }
        t = back < 0 && fwd >= n ? 0 : back < 0 ? fwd : fwd >= n ? back : i - back <= fwd - i ? back : fwd;
        memcpy(v + i * 12, was[t], 12);
    }
}

/* The build slider a node's thickness follows, -1 none. */
static int wp_build_group(int node) {
    switch (node) {
    case 19: case 20: case 33: case 34: return WP_SIZE_ARMS;
    case 16: case 17: case 18: case 32: return WP_SIZE_TORSO;
    case 8: case 9: case 10: case 12: case 13: case 14: return WP_SIZE_LEGS;
    }
    return -1;
}

/* Per batch: thicker or thinner about the bone through `pivot` along `u`, the length kept. */
typedef struct WpThickCtx {
    float pivot[3], u[3], f;
} WpThickCtx;

static void wp_thick_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpThickCtx *c = (WpThickCtx *)ctx;
    int i, j;

    (void)h;
    for (i = 0; i < count; i++) {
        float *q = v + i * 12, r[3], along;

        for (j = 0; j < 3; j++) {
            r[j] = q[j] - c->pivot[j];
        }
        along = r[0] * c->u[0] + r[1] * c->u[1] + r[2] * c->u[2];
        for (j = 0; j < 3; j++) {
            q[j] = c->pivot[j] + along * c->u[j] + (r[j] - along * c->u[j]) * c->f;
        }
    }
}

/* Per batch: a part made larger or smaller about `pivot`: every vertex (`full`), else as much as it follows the node. */
typedef struct WpScaleCtx {
    float pivot[3], s;
    int full;
} WpScaleCtx;

static void wp_scale_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpScaleCtx *c = (WpScaleCtx *)ctx;
    int i, j;

    (void)h;
    for (i = 0; i < count; i++) {
        float *q = v + i * 12, k = c->full ? c->s - 1.0f : q[3] * (c->s - 1.0f);

        for (j = 0; j < 3; j++) {
            q[j] += k * (q[j] - c->pivot[j]);
        }
    }
}

/* How long a bone is in a model: from node a's joint to node b's (0 unknown). */
static float wp_bone(const uint8_t *m, const WpRec *r, int n, int a, int b) {
    int i = wp_find(r, n, a), j = wp_find(r, n, b), q;
    float d = 0.0f;

    if (i < 0 || j < 0) {
        return 0.0f;
    }
    for (q = 0; q < 3; q++) {
        float e = ((const float *)(m + r[j].off + 0x10))[q] - ((const float *)(m + r[i].off + 0x10))[q];

        d += e * e;
    }
    return sqrtf(d);
}

/* How thick a record is across a bone `u` (from its bounds). */
static float wp_girth(const uint8_t *m, const WpRec *r, const float *u) {
    const float *e = (const float *)(m + r->off + 0x50);
    float g = 0.0f;
    int q;

    if (r->size <= 0x40) {
        return 0.0f;
    }
    for (q = 0; q < 3; q++) {
        g += e[q] * e[q] * (1.0f - u[q] * u[q]);
    }
    return sqrtf(g);
}

/* The line a node's bone lies on in a model (through `c` along `u`): a limb's from its first joint to the next, the
   rest the torso's middle, waist to neck. */
static void wp_line(const uint8_t *m, const WpRec *r, int n, int node, float *c, float *u) {
    int a = 16, b = 46, i, j, q;
    float len = 0.0f;

    switch (node) {
    case 9: case 13: case 19: case 33: case 10: case 14: case 20: case 34: a = node, b = node + 1; break;
    case 11: case 15: case 21: case 35: a = node - 1, b = node; break;
    }
    i = wp_find(r, n, a);
    j = wp_find(r, n, b);
    for (q = 0; q < 3; q++) {
        c[q] = i >= 0 ? ((const float *)(m + r[i].off + 0x10))[q] : 0.0f;
        u[q] = i >= 0 && j >= 0 ? ((const float *)(m + r[j].off + 0x10))[q] - c[q] : (q == 1 ? 1.0f : 0.0f);
        len += u[q] * u[q];
    }
    if (a == 16) {
        c[0] = 0.0f;
    }
    len = sqrtf(len);
    for (q = 0; q < 3; q++) {
        u[q] = len > 1e-4f ? u[q] / len : (q == 1 ? 1.0f : 0.0f);
    }
}

/* How a donor's part on `node` is fitted to the body (a teen's body in a grown man's clothes): along its bone by the
   limb's bone in the body over the donor's (a hand or foot as its limb, the rest as the torso: waist to neck), across
   it by how thick the body's own part is there over the donor's (a little more: clothes lie over the body), never
   thicker than along. *u: the donor's bone. */
static float wp_fit(const uint8_t *bm, const WpRec *br, int bn, const uint8_t *dm, const WpRec *dr, int dn, int node, float *g, float *u, float *c) {
    int a = 16, b = 46, i, j, q;
    float lb, ld, s, len = 0.0f;

    switch (node) {
    case 9: case 13: case 19: case 33: a = node, b = node + 1; break;     /* thighs, upper arms */
    case 10: case 14: case 20: case 34: a = node, b = node + 1; break;    /* shins, forearms */
    case 11: case 15: case 21: case 35: a = node - 1, b = node; break;    /* feet, hands: as the limb */
    }
    if (node >= 22 && node <= 30) { /* (the fingers: as their hand) */
        a = 20, b = 21;
    } else if (node >= 36 && node <= 44) {
        a = 34, b = 35;
    }
    *g = 1.0f;
    u[0] = 0.0f, u[1] = 1.0f, u[2] = 0.0f;
    i = wp_find(dr, dn, a);
    for (q = 0; q < 3; q++) { /* (the bone's line: through the limb's first joint, the torso's middle) */
        c[q] = i >= 0 ? ((const float *)(dm + dr[i].off + 0x10))[q] : 0.0f;
    }
    if (a == 16) {
        c[0] = 0.0f;
    }
    lb = wp_bone(bm, br, bn, a, b);
    ld = wp_bone(dm, dr, dn, a, b);
    if (lb < 1e-3f || ld < 1e-3f) {
        return 1.0f;
    }
    s = lb / ld;
    s = s < 0.6f ? 0.6f : s > 1.5f ? 1.5f : s;
    *g = s;
    i = wp_find(dr, dn, a);
    j = wp_find(dr, dn, b);
    for (q = 0; q < 3; q++) {
        u[q] = ((const float *)(dm + dr[j].off + 0x10))[q] - ((const float *)(dm + dr[i].off + 0x10))[q];
        len += u[q] * u[q];
    }
    len = sqrtf(len);
    for (q = 0; q < 3; q++) {
        u[q] = len > 1e-4f ? u[q] / len : (q == 1 ? 1.0f : 0.0f);
    }
    { /* across: one measure for all the donor's pieces (pieces made thinner apart open at their seams), the torso's:
         the thickest of its pieces in the body over the donor's, never thicker than the torso's length says */
        static const int torso[6] = {8, 12, 16, 17, 18, 32};
        float ut[3], ul = 0.0f, gb = 0.0f, gd = 0.0f, st;
        int t;

        i = wp_find(dr, dn, 16);
        j = wp_find(dr, dn, 46);
        if (i < 0 || j < 0) {
            return s;
        }
        for (q = 0; q < 3; q++) {
            ut[q] = ((const float *)(dm + dr[j].off + 0x10))[q] - ((const float *)(dm + dr[i].off + 0x10))[q];
            ul += ut[q] * ut[q];
        }
        ul = sqrtf(ul);
        for (q = 0; q < 3; q++) {
            ut[q] = ul > 1e-4f ? ut[q] / ul : (q == 1 ? 1.0f : 0.0f);
        }
        for (t = 0; t < 6; t++) {
            int x = wp_find(br, bn, torso[t]), y = wp_find(dr, dn, torso[t]);
            float e = x >= 0 ? wp_girth(bm, &br[x], ut) : 0.0f, f = y >= 0 ? wp_girth(dm, &dr[y], ut) : 0.0f;

            gb = e > gb ? e : gb;
            gd = f > gd ? f : gd;
        }
        lb = wp_bone(bm, br, bn, 16, 46);
        ld = wp_bone(dm, dr, dn, 16, 46);
        st = lb > 1e-3f && ld > 1e-3f ? lb / ld : 1.0f;
        st = st < 0.6f ? 0.6f : st > 1.5f ? 1.5f : st;
        if (gb > 1e-3f && gd > 1e-3f) {
            float r = gb / gd * 1.12f;

            *g = r < 0.55f ? 0.55f : r > st ? st : r;
        } else {
            *g = st;
        }
    }
    return s;
}

/* Takes the batches of origin `from`'s record for `node` (want: 0 all, 1 not its hair, 2 only its hair), moved onto
   the body's record `pivot` (NULL: by `delta`), standing off by `puff`; a donor's clothes and limbs fitted to the
   body (wp_fit). */
static void wp_take(WpBuild *b, const uint8_t *m, const WpRec *r, int n, int node, int from, int want, const uint32_t *hair, int hairs,
                    const uint8_t *pivot, const float *delta, float puff) {
    int i = wp_find(r, n, node), q;

    if (i < 0 || r[i].size <= 0x40) {
        return;
    }
    b->from = from;
    b->want = want;
    b->hair = hair;
    b->hairs = hairs;
    b->puff = puff;
    b->s = b->g = 1.0f;
    b->u[0] = 0.0f, b->u[1] = 1.0f, b->u[2] = 0.0f;
    b->c[0] = b->c[1] = b->c[2] = 0.0f;
    if (pivot != NULL && from != WP_BODY && b->fitM != NULL && wp_slot_of(node) >= WP_CHEST && wp_slot_of(node) <= WP_FEET) {
        b->s = wp_fit(b->fitM, b->fitR, b->fitN, m, r, n, node, &b->g, b->u, b->c);
    }
    b->even = 0;
    if ((node == 11 || node == 15 || (node >= 21 && node <= 30) || (node >= 35 && node <= 44)) && (b->s != 1.0f || b->g != 1.0f)) {
        b->s = (b->s + b->g) * 0.5f; /* (a hand, a foot: one size, between the limb's length and the outfit's thickness) */
        b->g = 1.0f;
        b->even = 1;
    }
    memcpy(b->uB, b->u, sizeof(b->uB));
    memcpy(b->cB, b->c, sizeof(b->cB));
    b->sB = b->s;
    if (b->s != 1.0f || b->g != 1.0f) { /* (the parent's bone and stretch, for what follows the parent) */
        int pn = r[i].parent;

        b->sB = 1.0f;
        if (pn >= 0 && r[pn].node < 0x47 && wp_slot_of(r[pn].node) >= WP_CHEST && wp_slot_of(r[pn].node) <= WP_FEET) {
            float gg, cc[3];

            b->sB = wp_fit(b->fitM, b->fitR, b->fitN, m, r, n, r[pn].node, &gg, b->uB, cc);
        } else if (pn >= 0) {
            wp_line(m, r, n, r[pn].node, b->cB, b->uB);
        }
    }
    for (q = 0; q < 3; q++) {
        b->pA[q] = ((const float *)(m + r[i].off + 0x10))[q];
        b->pB[q] = ((const float *)(m + r[i].off + 0x20))[q];
        if (pivot != NULL) {
            b->dA[q] = ((const float *)(pivot + 0x10))[q] - ((const float *)(m + r[i].off + 0x10))[q];
            b->dB[q] = ((const float *)(pivot + 0x20))[q] - ((const float *)(m + r[i].off + 0x20))[q];
        } else {
            b->dA[q] = b->dB[q] = delta != NULL ? delta[q] : 0.0f;
        }
    }
    wp_batches((uint8_t *)m, &r[i], wp_take_batch, b);
}


#define WP_STRIP_MAX 30
static float wp_area2(const float *a, const float *b, const float *c);
static float wp_d2(const float *a, const float *b);
/* ---- a donor's part with its own edge: the ring of its fighter's triangles around the join ----
 * A part from another fighter leaves the gap its own fighter's neighbour filled (a head without the collar it sat in).
 * With wp_skirts set (the workshop's "patch holes"), the donor's triangles of the neighbouring node that touch the part
 * come with it: their shape the donor's own, their texture place and picture the part's nearest edge point's (the
 * part's edge carried across, not the donor's clothes), following the node they lie on the side of, fitted as the part
 * is. They go in the part's record, in strips of their own. */
static int wp_skirts;
static float *wp_skirtMem[512];
static int wp_skirtMems;

typedef struct WpSkirtPts {
    const float *at[4096];
    const uint32_t *hdr[4096];
    int n;
} WpSkirtPts;

static void wp_skirt_pts(void *ctx, uint32_t *h, float *v, int count) {
    WpSkirtPts *p = (WpSkirtPts *)ctx;
    int i;

    for (i = 0; i < count && p->n < 4096; i++) {
        p->hdr[p->n] = h;
        p->at[p->n++] = v + i * 12;
    }
}

static void wp_skirt_free(void) {
    while (wp_skirtMems > 0) {
        free(wp_skirtMem[--wp_skirtMems]);
    }
}

/* The ring for node `node` of donor `from` (model m, records r): added to b as strips fitted as b's last take is. `src`:
   each node's origin in the result, -1 none. */
static float wp_line_uv(const float *o, const float *d, const float *t9, float *u, float *v);

static void wp_skirt(WpBuild *b, const uint8_t *const *mdl, const uint8_t *m, const WpRec *r, int n, int node, int from, const int *src,
                     const uint8_t *bm, const WpRec *br, int bn) {
    static WpSkirtPts part, nb, own;
    static float ownTri[4000][9];
    static float tri[600][3][12];
    static const uint32_t *triHdr[600];
    int i = wp_find(r, n, node), j, k, t, ntri = 0;

    if (i < 0 || r[i].size <= 0x40) {
        return;
    }
    part.n = 0;
    wp_batches((uint8_t *)m, &r[i], wp_skirt_pts, &part);
    for (j = 0; j < n && part.n > 0; j++) { /* its neighbours across a join: its parent, its children */
        int other = r[j].node;

        if ((j != r[i].parent && r[j].parent != i) || other >= 0x47 || other >= WP_NODES || r[j].size <= 0x40 || src[other] < 0 ||
            mdl[src[other]] == m) {
            continue; /* (not next to it, a chain, or the same fighter's on both sides: no join) */
        }
        nb.n = 0;
        wp_batches((uint8_t *)m, &r[j], wp_skirt_pts, &nb);
        own.n = 0; /* (the body's own neighbour there: where it is, the gap is not) */
        if (src[other] == WP_BODY && wp_find(br, bn, other) >= 0) {
            wp_batches((uint8_t *)bm, &br[wp_find(br, bn, other)], wp_skirt_pts, &own);
        }
        for (k = 0, t = 0; k + 2 < own.n && t < 4000; k++) {
            if (own.hdr[k] == own.hdr[k + 2] && wp_area2(own.at[k], own.at[k + 1], own.at[k + 2]) > 1e-12f) {
                memcpy(ownTri[t], own.at[k], 12);
                memcpy(ownTri[t] + 3, own.at[k + 1], 12);
                memcpy(ownTri[t] + 6, own.at[k + 2], 12);
                t++;
            }
        }
        own.n = t;
        for (k = 0; k + 2 < nb.n && ntri < 600; k++) {
            int touch = 0, v, q, over = 0;

            if (nb.hdr[k] != nb.hdr[k + 2] || wp_area2(nb.at[k], nb.at[k + 1], nb.at[k + 2]) < 1e-12f) {
                continue; /* (across two strips, or flat) */
            }
            for (v = 0; v < 3 && !touch; v++) {
                for (q = 0; q < part.n && !touch; q++) {
                    touch = wp_d2(nb.at[k + v], part.at[q]) < 0.05f * 0.05f;
                }
            }
            for (v = 0; v < 3 && touch; v++) { /* (on the body's own surface, as fitted: not needed there) */
                const float *p = nb.at[k + v];
                float at[3], d[3], dl, u, w;
                int h;

                for (q = 0; q < 3; q++) {
                    at[q] = p[q] + (j == r[i].parent ? b->dB[q] : b->dA[q]);
                    d[q] = p[4 + q];
                }
                dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                for (q = 0; q < 3 && dl > 1e-6f; q++) {
                    d[q] /= dl;
                }
                for (h = 0; dl > 1e-6f && h < own.n; h++) {
                    float tt = wp_line_uv(at, d, ownTri[h], &u, &w);

                    if (tt > -0.12f && tt < 0.12f) {
                        over++;
                        break;
                    }
                }
            }
            if (!touch || over >= 2) {
                continue;
            }
            for (v = 0; v < 3; v++) { /* each corner: the donor's place, the part's nearest point's texture place */
                const float *p = nb.at[k + v];
                float bd = 1e30f;
                int best = 0;

                for (q = 0; q < part.n; q++) {
                    float d = wp_d2(p, part.at[q]);

                    if (d < bd) {
                        bd = d;
                        best = q;
                    }
                }
                memcpy(tri[ntri][v], part.at[best], 48);
                memcpy(tri[ntri][v], p, 12);
                memcpy(tri[ntri][v] + 4, p + 4, 12);
                tri[ntri][v][3] = j == r[i].parent ? 0.0f : 1.0f; /* (the parent's side follows the parent) */
                if (v == 0) {
                    triHdr[ntri] = part.hdr[best];
                }
            }
            ntri++;
        }
    }
    for (t = 0; t < ntri && wp_skirtMems < 512;) { /* the strips: one picture's triangles each, joined by points said twice */
        const uint32_t *h = triHdr[t];
        float *mem = malloc(20 * 4 + WP_STRIP_MAX * 48), *vp;
        uint32_t *hw;
        int cnt = 0;

        if (mem == NULL) {
            return;
        }
        wp_skirtMem[wp_skirtMems++] = mem;
        hw = (uint32_t *)mem;
        vp = mem + 20;
        for (j = t; j < ntri && cnt + (cnt > 0 ? 5 : 3) <= WP_STRIP_MAX; j++) {
            if (triHdr[j] != h) {
                continue;
            }
            if (cnt > 0) {
                memcpy(vp + cnt * 12, vp + (cnt - 1) * 12, 48);
                memcpy(vp + (cnt + 1) * 12, tri[j][0], 48);
                cnt += 2;
            }
            memcpy(vp + cnt * 12, tri[j], 3 * 48);
            cnt += 3;
            triHdr[j] = NULL;
        }
        memcpy(hw, h, 80);
        hw[12] = (hw[12] & ~0x7FFFu) | (uint32_t)cnt;
        hw[16] = (hw[16] & ~0x7FFFu) | (uint32_t)cnt;
        b->from = from;
        b->want = 0;
        wp_take_batch(b, hw, vp, cnt);
        while (t < ntri && triHdr[t] == NULL) {
            t++;
        }
    }
}

/*
 * The body's model and textures with each slot's part from its donor (dm[s] / dt[s], NULL: the body's own) and another
 * fighter's part layered on it (dm[WP_LAYER + s] / dt[WP_LAYER + s], NULL none), the sizes in percent (100 as it is).
 * Writes malloc'ed results to *om / *ot and returns 1; 0 when nothing changes, -1 when it cannot be done (*why says
 * why). Only the textures and ramps the result's meshes use are kept (packed again), the face texture with them: the
 * face's donor's when the face is swapped (its expression sets come with it, plat_mods.c).
 */
/* Where each batch of the last model wp_compose made came from (its origin, in the model's order): the workshop tells
   a layer's pieces from the part's under it by it (wp_pieces). */
static uint8_t wp_lastFrom[WP_MAX_BATCHES];
static int wp_lastFroms;
static int wp_lastBat[WP_MAX_BATCHES];        /* each strip of the model made: its place in the build (b.bat) */
static uint32_t wp_lastBatOff[WP_MAX_BATCHES]; /* and its first vertex's offset in the model */
/* An outfit's pieces taken off (a layer's piece hidden in the workshop: only the outfit's parts kept that are wanted):
   the build's strips left out of what the layers hide, so the body under them stays (the hide edit then puts the
   piece on one point, as any hidden piece). Set by wp_drop_hidden for a second wp_compose. */
static int wp_dropOn;
/* The mouth's shapes (pak parts 3..10, the "mouth A" sets BtlObj_BindTables binds: the face's strips as it talks, the
   open mouth's red inside and teeth): batches as the model's, drawn with the face fighter's textures and ramps at that
   fighter's places. wp_compose keeps those pictures and ramps as it keeps the model's and writes the shapes again
   moved where they are now (copied as they were, the open mouth drew whatever was there: white). In: the shapes of
   origin wp_mouthFrom (NULL none); out: malloc'ed copies (NULL: not made, keep the old). */
/* Shading by texture (shade_tex= lines): every batch drawing a picture (wc_tex_hash) gets its shading ramp coloured by
   hand (wp_keep_ramp_tint), whichever part it is on. In: wp_txKey / wp_txRgb (0x1000000 | 0xBBGGRR). Out, for the
   screen: each picture's ramps as the model is drawn (4 colours of each, light to dark). */
#define WP_TX_MAX 16
#define WP_TXI_MAX 48
static uint32_t wp_txKey[WP_TX_MAX], wp_txRgb[WP_TX_MAX];
static int wp_txN;
static uint32_t wp_txiKey[WP_TXI_MAX], wp_txiTb[WP_TXI_MAX][4], wp_txiRgb[WP_TXI_MAX][4][4];
static int wp_txiFrom[WP_TXI_MAX][4], wp_txiRamps[WP_TXI_MAX], wp_txiN;
static uint32_t wc_tex_hash(const uint8_t *d, uint32_t size, const WpTex *e);
static void wp_ramp_rgb(const WpPack *k, int from, uint32_t tbp, uint32_t *out);

/* The picture hash of kept picture pi (wc_tex_hash, as the colours screen names textures). */
static uint32_t wp_pic_hash(const WpPack *k, int pi) {
    const WpTex *e;

    if (pi < 0 || pi >= k->pics) {
        return 0;
    }
    e = &k->ent[k->pic[pi].from][k->pic[pi].index];
    return wc_tex_hash(k->file[k->pic[pi].from], e->pixOfs * 4 + e->pixSize + 0x60, e);
}

/* What the screen shows: picture `key` drawn with origin `from`'s ramp at tbp. */
static void wp_txi_note(const WpPack *k, uint32_t key, int from, uint32_t tbp) {
    int i, r;

    if (key == 0) {
        return;
    }
    for (i = 0; i < wp_txiN && wp_txiKey[i] != key; i++) {
    }
    if (i == wp_txiN) {
        if (wp_txiN >= WP_TXI_MAX) {
            return;
        }
        wp_txiKey[wp_txiN] = key;
        wp_txiRamps[wp_txiN++] = 0;
    }
    for (r = 0; r < wp_txiRamps[i]; r++) {
        if (wp_txiTb[i][r] == tbp && wp_txiFrom[i][r] == from) {
            return;
        }
    }
    if (r >= 4) {
        return;
    }
    wp_txiTb[i][r] = tbp;
    wp_txiFrom[i][r] = from;
    wp_ramp_rgb(k, from, tbp, wp_txiRgb[i][r]);
    wp_txiRamps[i]++;
}

static const uint8_t *wp_mouthIn[8];
static uint32_t wp_mouthN[8];
static int wp_mouthFrom = -1;
static uint8_t *wp_mouthOut[8];
static uint8_t wp_drop[WP_MAX_BATCHES];
/* Each chain node of the last model wp_compose made (a strand of hair, a tail, a cape's link): the origin it came from
   and its id there (a donor's id taken by another gets a free one), -1 none. The chains' swing (model part 22, the
   parameters per node id) is made from them (plat_mods.c warrior_chain_params). */
static int wp_chainFrom[WP_NODES], wp_chainOrig[WP_NODES];

/* A fighter's face's points (node 48), x y z each, up to max: their count. */
typedef struct WpFacePts {
    float *p;
    int n, max;
} WpFacePts;

static void wp_face_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpFacePts *c = (WpFacePts *)ctx;
    int i;

    (void)h;
    for (i = 0; i < count && c->n < c->max; i++, c->n++) {
        memcpy(c->p + c->n * 3, v + i * 12, 12);
    }
}

static int wp_face_points(const uint8_t *m, const WpRec *r, int n, float *p, int max) {
    WpFacePts c;
    int i;

    c.p = p;
    c.n = 0;
    c.max = max;
    for (i = 0; i < n; i++) {
        if (r[i].node == 48 && r[i].size > 0x40) {
            wp_batches((uint8_t *)m, &r[i], wp_face_batch, &c);
        }
    }
    return c.n;
}

/* The body's pictures' unions (+0x2C's low 16 bits: the slot, the slot's alpha), counted per node by its strips:
   each vote goes to the union's slot (0..15), whose word is kept in unionWord. */
static uint32_t unionWord[16];
typedef struct WpUnionScan {
    const WpTex *ent;
    int count, hairs, hairy;
    const uint32_t *hair;
    int *votes, *hvotes;
} WpUnionScan;

static void wp_union_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpUnionScan *c = (WpUnionScan *)ctx;
    uint32_t key = wp_tex_key(h);
    int i;

    (void)v;
    for (i = 0; i < c->count; i++) {
        if ((c->ent[i].tbpOfs | c->ent[i].cbpOfs << 16) == key) {
            int slot = (int)(c->ent[i].unk2C & 0xF);

            unionWord[slot] = c->ent[i].unk2C & 0xFFFF;
            c->votes[slot] += count;
            if (c->hairy && wp_is_hair(c->hair, c->hairs, key)) {
                c->hvotes[slot] += count;
            }
            return;
        }
    }
}

/* Every point of model m (mn bytes) that was in one place with others in `before` (the same layout) and is not now:
   all of them on their middle again. */
typedef struct WpWeldCtx {
    const uint8_t *base;
    uint32_t *off;
    int n, max;
} WpWeldCtx;

static void wp_weld_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpWeldCtx *c = (WpWeldCtx *)ctx;
    int i;

    (void)h;
    for (i = 0; i < count && c->n < c->max; i++) {
        c->off[c->n++] = (uint32_t)((const uint8_t *)(v + i * 12) - c->base);
    }
}

static const uint8_t *wp_sortBase; /* (wp_by_x's: defined with the patching below) */
static const uint32_t *wp_sortOff;
static int wp_by_x(const void *a, const void *b);

static void wp_weld_again(uint8_t *m, uint32_t mn, const uint8_t *before) {
    static WpRec r[WP_MAX_RECS * 2];
    WpWeldCtx c;
    int *order, nr = wp_records(m, mn, r, WP_MAX_RECS * 2), i, k, q, j, moved = 0;

    c.base = m;
    c.max = (int)(mn / 48) + 16;
    c.n = 0;
    c.off = malloc((size_t)c.max * 4);
    order = malloc((size_t)c.max * 4);
    if (c.off == NULL || order == NULL) {
        free(c.off);
        free(order);
        return;
    }
    for (i = 0; i < nr; i++) {
        if (r[i].size > 0x40) {
            wp_batches(m, &r[i], wp_weld_batch, &c);
        }
    }
    for (i = 0; i < c.n; i++) {
        order[i] = i;
    }
    wp_sortBase = before;
    wp_sortOff = c.off;
    qsort(order, (size_t)c.n, 4, wp_by_x);
    for (k = 0; k < c.n; k = q) {
        const float *a = (const float *)(before + c.off[order[k]]);
        float sum[3] = {0.0f, 0.0f, 0.0f}, spread = 0.0f;
        int grp[64], ng = 0;

        for (q = k; q < c.n && *(const float *)(before + c.off[order[q]]) - a[0] < WP_WELD; q++) {
            const float *b = (const float *)(before + c.off[order[q]]);

            if (fabsf(b[1] - a[1]) < WP_WELD && fabsf(b[2] - a[2]) < WP_WELD && ng < 64) {
                grp[ng++] = order[q];
            }
        }
        if (q == k) {
            q = k + 1;
        }
        if (ng < 2) {
            continue;
        }
        for (i = 0; i < ng; i++) {
            const float *p = (const float *)(m + c.off[grp[i]]);

            for (j = 0; j < 3; j++) {
                sum[j] += p[j] / (float)ng;
            }
        }
        for (i = 0; i < ng; i++) {
            const float *p = (const float *)(m + c.off[grp[i]]);
            float d = (p[0] - sum[0]) * (p[0] - sum[0]) + (p[1] - sum[1]) * (p[1] - sum[1]) + (p[2] - sum[2]) * (p[2] - sum[2]);

            spread = d > spread ? d : spread;
        }
        if (spread < WP_WELD * WP_WELD || spread > 0.6f * 0.6f) {
            continue; /* (together still; or far apart: not a seam the sizes opened) */
        }
        for (i = 0; i < ng; i++) {
            memcpy(m + c.off[grp[i]], sum, 12);
        }
        moved += ng;
    }
    if (wp_patchLog) {
        printf("builds: %d points put back together%c", moved, 10);
    }
    free(c.off);
    free(order);
}

/* Shading textures in use: each block offset, how many strips take it. */
typedef struct WpShList {
    uint32_t tb[8];
    int use[8], n;
} WpShList;

static uint32_t wp_strip_ramp(const uint32_t *h) {
    return (uint32_t)((((uint64_t)h[8] | ((uint64_t)h[9] << 32)) >> 37) & 0x3FFF);
}

static void wp_sh_add(WpShList *l, uint32_t tb) {
    int q;

    for (q = 0; q < l->n && l->tb[q] != tb; q++) {
    }
    if (q == l->n && q < 8) {
        l->tb[q] = tb;
        l->use[q] = 0;
        l->n++;
    }
    if (q < 8) {
        l->use[q]++;
    }
}

static void wp_sh_sort(WpShList *l) { /* (the most used first) */
    int q, j;

    for (q = 1; q < l->n; q++) {
        for (j = q; j > 0 && l->use[j] > l->use[j - 1]; j--) {
            uint32_t t = l->tb[j];
            int u = l->use[j];

            l->tb[j] = l->tb[j - 1], l->use[j] = l->use[j - 1];
            l->tb[j - 1] = t, l->use[j - 1] = u;
        }
    }
}

typedef struct WpShScan {
    WpShList *l;
    const uint32_t *hair;
    int hairs, hairOnly;
} WpShScan;

static void wp_sh_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpShScan *c = (WpShScan *)ctx;

    (void)v, (void)count;
    if (!c->hairOnly || wp_is_hair(c->hair, c->hairs, wp_tex_key(h))) {
        wp_sh_add(c->l, wp_strip_ramp(h));
    }
}

static int wp_compose(const uint8_t *bm, uint32_t bmn, const uint8_t *bt, uint32_t btn, const uint8_t *const *dm, const uint32_t *dmn,
                      const uint8_t *const *dt, const uint32_t *dtn, const int *sizes, uint8_t **om, uint32_t *omn, uint8_t **ot,
                      uint32_t *otn, const char **why) {
    static WpRec rec[WP_ORIGINS][WP_MAX_RECS];
    static WpTex ent[WP_ORIGINS][WP_MAX_TEX];
    static uint32_t hair[WP_ORIGINS][32];
    static WpBuild b;
    static WpPack k;
    static int recFile[WP_MAX_RECS * 2];
    const uint8_t *mdl[WP_ORIGINS];
    int nrec[WP_ORIGINS], hairs[WP_ORIGINS];
    int s, i, j, q, o2, any = 0, face, fade, nextNode = 72, nn, extra, faceFrom, hairFrom, objFrom[4];
    static WpShList shTop[WP_SH_ENTRIES], shRef[WP_SH_ENTRIES];
    static int picNode[WP_MAX_TEX], picHair[WP_MAX_TEX], bodyNodeUnion[WP_NODES];
    uint8_t *beforeSizes = NULL;
    int bodyHairUnion = -1;
    int shRefFrom[WP_SH_ENTRIES], skinFrom = -1, outFrom = -1;
    uint32_t skinTb = 0, outTb = 0;
    uint32_t size, o, texSize, tail, recEnd;

    wp_lastFroms = 0;
    wp_skirt_free();
    uint8_t used[WP_NODES];
    int src[WP_NODES];
    uint8_t *m, *t;
    const WpRec *br;
    int bn;

    *om = *ot = NULL;
    *why = "";
    memset(&k, 0, sizeof(k));
    memset(picNode, 0xFF, sizeof(picNode)); /* (no picture on a part yet: -1) */
    memset(picHair, 0, sizeof(picHair));
    b.bats = b.outs = 0;
    b.why = NULL;
    for (o2 = 0; o2 < WP_ORIGINS; o2++) {
        const uint8_t *mm = o2 == WP_BODY ? bm : dm[o2];
        const uint8_t *tt = o2 == WP_BODY ? bt : dt[o2];
        uint32_t mn = o2 == WP_BODY ? bmn : dmn[o2], tn = o2 == WP_BODY ? btn : dtn[o2];

        mdl[o2] = mm;
        nrec[o2] = hairs[o2] = 0;
        k.file[o2] = tt;
        k.ent[o2] = ent[o2];
        if (mm == NULL || mm == wp_empty) {
            k.file[o2] = (const uint8_t *)&k.file[o2]; /* (no file: like no other) */
            any |= mm == wp_empty;
            continue;
        }
        nrec[o2] = wp_records(mm, mn, rec[o2], WP_MAX_RECS);
        k.count[o2] = wp_textures(tt, tn, ent[o2], WP_MAX_TEX);
        k.texCount[o2] = mm[0x4F];
        k.rampCount[o2] = mm[0x51];
        if (nrec[o2] == 0 || k.count[o2] == 0 || mm[0x4E] != 0 || mm[0x50] != mm[0x4F] || mm[0x4F] + mm[0x51] > k.count[o2]) {
            *why = o2 == WP_BODY ? "The body's model could not be read." : "A part's fighter has a model that cannot be read.";
            return -1;
        }
        hairs[o2] = wp_hair_textures(mm, rec[o2], nrec[o2], hair[o2], 32);
        any |= o2 != WP_BODY;
    }
    wp_shf_scan(mdl[WP_SHF] != NULL && mdl[WP_SHF] != wp_empty ? mdl[WP_SHF] : NULL, rec[WP_SHF], nrec[WP_SHF], ent[WP_SHF], k.texCount[WP_SHF],
                k.rampCount[WP_SHF], hair[WP_SHF], hairs[WP_SHF], dt[WP_SHF], dtn[WP_SHF]);
    for (s = 0; s < WP_SIZES; s++) {
        any |= sizes != NULL && sizes[s] > 0 && sizes[s] != 100;
    }
    if (!any && !wp_forceCompose) {
        return 0;
    }
    { /* (the face texture's place kept: see WpPack) */
        int ff = dm[WP_FACE] != NULL && dm[WP_FACE] != wp_empty ? WP_FACE : WP_BODY;

        k.faceFrom = -1;
        if (mdl[ff][0x4C] < k.count[ff]) {
            const WpTex *fe = &ent[ff][mdl[ff][0x4C]];

            k.faceFrom = ff;
            k.faceTbp = fe->tbpOfs;
            k.faceCbp = fe->cbpOfs;
            k.faceT1 = fe->tbpOfs + fe->tbpStep;
            k.faceC1 = fe->cbpOfs + fe->cbpStep;
        }
    }
    br = rec[WP_BODY];
    bn = nrec[WP_BODY];
    hairFrom = dm[WP_HAIR] != NULL ? WP_HAIR : WP_BODY;
    for (o2 = 0; o2 < WP_ORIGINS; o2++) {
        wp_skeleton(o2, mdl[o2], rec[o2], nrec[o2]);
    }
    b.fitM = bm;
    b.fitR = br;
    b.fitN = bn;
    for (i = 0; i < WP_NODES; i++) { /* (each node's origin: whose part it is) */
        int sl = i < 0x47 ? wp_slot_of(i) : -1;

        src[i] = i >= 0x47 || wp_find(br, bn, i) < 0 ? -1 : sl >= 0 && dm[sl] != NULL && dm[sl] != wp_empty ? sl : WP_BODY;
    }

    /* the special objects (nodes 0x40..0x43: Trunks' sword in his hand and on his back, Tapion's, a hat...), each a
       record hanging from a node of the skeleton (its own, by fighter): taken with the accessories (their main donor's,
       else their layer's) when that fighter has one on the node; the body's on that node is then left out */
    {
        int ob;

        for (ob = 0; ob < 4; ob++) {
            int o3;

            objFrom[ob] = -1;
            for (o3 = WP_EXTRAS; o3 < WP_ORIGINS && objFrom[ob] < 0; o3 = o3 == WP_EXTRAS ? WP_LAYER + WP_EXTRAS : o3 < WP_XL ? WP_XL : o3 + 1) {
                if (o3 >= WP_XL && (o3 >= WP_XL + WP_XLAYERS || wp_xSlot[o3 - WP_XL] != WP_EXTRAS || wp_xTail[o3 - WP_XL])) {
                    continue;
                }
                if (mdl[o3] != NULL && mdl[o3] != wp_empty) {
                    for (j = 0; j < nrec[o3]; j++) {
                        if (rec[o3][j].node == 0x40 + ob && rec[o3][j].size > 0x40 && rec[o3][j].parent >= 0 && rec[o3][rec[o3][j].parent].node < 0x40) {
                            objFrom[ob] = o3;
                        }
                    }
                }
            }
        }
    }
    /* the body's skeleton, each node with its slot's strips (and the hair's on the head), layers on top; the chains
       of the slots kept */
    for (i = 0; i < bn; i++) {
        const WpRec *r = &br[i];
        WpOut *u;

        if (r->node >= 0x40 && r->node <= 0x43 && objFrom[r->node - 0x40] >= 0) {
            for (j = i + 1; j < bn && br[j].depth > r->depth; j++) {
            }
            i = j - 1;
            continue; /* (an object another fighter's comes in its place, below; what hangs from it with it) */
        }
        if (r->node >= 0x47 && (dm[wp_chain_slot_of(br, i)] != NULL || (wp_chain_part_of(br, i) >= 0 && dm[wp_chain_part_of(br, i)] != NULL))) {
            continue; /* (a chain whose slot is another fighter's, or the part it hangs from: a sash's tails go with the waist) */
        }
        if (b.outs >= WP_MAX_RECS * 2) {
            *why = "Too many pieces for one model.";
            return -1;
        }
        u = &b.out[b.outs++];
        u->head = bm + r->off;
        u->pd[0] = u->pd[1] = u->pd[2] = 0.0f;
        u->depth = r->depth;
        u->node = r->node;
        u->from = WP_BODY;
        u->b0 = b.bats;
        if (r->node >= 0x47) {
            wp_take(&b, bm, br, bn, r->node, WP_BODY, 0, NULL, 0, NULL, NULL, 0.0f);
        } else {
            int slot = wp_slot_of(r->node), main = slot >= 0 && dm[slot] != NULL ? slot : WP_BODY;
            int layer = slot >= 0 && dm[WP_LAYER + slot] != NULL ? WP_LAYER + slot : -1;

            u->from = main;
            if (!wp_head_node(r->node)) {
                wp_take(&b, mdl[main], rec[main], nrec[main], r->node, main, 0, NULL, 0, bm + r->off, NULL, 0.0f);
                if (wp_skirts && main != WP_BODY && mdl[main] != wp_empty) {
                    wp_skirt(&b, mdl, mdl[main], rec[main], nrec[main], r->node, main, src, bm, br, bn);
                }
            } else { /* the head's: the part's own strips but its hair, then the hair's */
                wp_take(&b, mdl[main], rec[main], nrec[main], r->node, main, 1, hair[main], hairs[main], bm + r->off, NULL, 0.0f);
                if (wp_skirts && main != WP_BODY && mdl[main] != wp_empty) {
                    wp_skirt(&b, mdl, mdl[main], rec[main], nrec[main], r->node, main, src, bm, br, bn);
                }
                wp_take(&b, mdl[hairFrom], rec[hairFrom], nrec[hairFrom], r->node, hairFrom, 2, hair[hairFrom], hairs[hairFrom], bm + r->off, NULL, 0.0f);
                if (dm[WP_LAYER + WP_HAIR] != NULL) {
                    static float facePts[2048 * 3];

                    b.faceN = wp_face_points(mdl[WP_LAYER + WP_HAIR], rec[WP_LAYER + WP_HAIR], nrec[WP_LAYER + WP_HAIR], facePts, 2048);
                    b.faceV = facePts;
                    wp_take(&b, mdl[WP_LAYER + WP_HAIR], rec[WP_LAYER + WP_HAIR], nrec[WP_LAYER + WP_HAIR], r->node, WP_LAYER + WP_HAIR, 2,
                            hair[WP_LAYER + WP_HAIR], hairs[WP_LAYER + WP_HAIR], bm + r->off, NULL, 0.0f); /* (hair over hair: added as it is, nothing lifted or hidden;
                                                                                                    not what lies on its own face) */
                    b.faceN = 0;
                }
            }
            if (layer >= 0) {
                wp_take(&b, mdl[layer], rec[layer], nrec[layer], r->node, layer, wp_head_node(r->node) ? 1 : 0, hair[layer], hairs[layer],
                        bm + r->off, NULL, WP_PUFF);
            }
            { /* the more layers: over this node's slot as its layer is; hair over the head as the hair's layer is */
                int x;

                for (x = 0; x < WP_XLAYERS; x++) {
                    int xo = WP_XL + x;

                    if (dm[xo] == NULL || mdl[xo] == NULL || mdl[xo] == wp_empty) {
                        continue;
                    }
                    if (wp_xSlot[x] == WP_HAIR && wp_head_node(r->node)) {
                        static float facePts2[2048 * 3];

                        b.faceN = wp_face_points(mdl[xo], rec[xo], nrec[xo], facePts2, 2048);
                        b.faceV = facePts2;
                        wp_take(&b, mdl[xo], rec[xo], nrec[xo], r->node, xo, 2, hair[xo], hairs[xo], bm + r->off, NULL, 0.0f);
                        b.faceN = 0;
                    } else if (wp_xTail[x] && r->node >= 4 && r->node <= 7) { /* a tail layer: the tail's bones' meshes (Kid
                                                                                 Goku's, Super Saiyan 4's, the Great Apes': skinned
                                                                                 to nodes 4..7, which a tailless body has bare) */
                        int tr = wp_find(rec[xo], nrec[xo], r->node);

                        if (tr >= 0) { /* (the tail's bones where the donor has them: a tailless body keeps them on its hips) */
                            u->head = mdl[xo] + rec[xo][tr].off;
                        }
                        wp_take(&b, mdl[xo], rec[xo], nrec[xo], r->node, xo, 0, hair[xo], hairs[xo], u->head, NULL, 0.0f);
                    } else if (wp_xSlot[x] >= 0 && wp_xSlot[x] != WP_HAIR && wp_xSlot[x] == slot) {
                        wp_take(&b, mdl[xo], rec[xo], nrec[xo], r->node, xo, wp_head_node(r->node) ? 1 : 0, hair[xo], hairs[xo], bm + r->off, NULL,
                                wp_xSlot[x] == WP_EXTRAS ? 0.0f : WP_PUFF);
                    }
                }
            }
        }
        u->nb = b.bats - u->b0;
    }
    /* the donors' chains of the chain slots (main and layer), hung from the same node of the body */
    for (o2 = 0; o2 < WP_ORIGINS; o2++) {
        int cs = o2 < WP_BODY ? o2 % WP_SLOTS : o2 >= WP_XL && o2 < WP_XL + WP_XLAYERS ? wp_xSlot[o2 - WP_XL] : -1;

        if (o2 == WP_BODY || cs < 0) {
            continue;
        }

        if (mdl[o2] == NULL || (cs != WP_HAIR && cs != WP_EXTRAS && o2 >= WP_SLOTS)) {
            continue; /* (a part's own donor brings the chains hanging from that part too) */
        }
        for (j = 0; j < nrec[o2]; j++) {
            const WpRec *c = &rec[o2][j];
            int anchor, a, at, end, base;
            float delta[3];

            if (c->node >= 0x40 && c->node <= 0x43 && c->parent >= 0 && rec[o2][c->parent].node < 0x40) { /* an object */
                if (objFrom[c->node - 0x40] != o2) {
                    continue;
                }
            } else if (c->node < 0x47 || c->parent < 0 || rec[o2][c->parent].node >= 0x47) {
                continue; /* (a chain's first record only: the whole chain comes with it) */
            }
            anchor = rec[o2][c->parent].node;
            if (c->node >= 0x40 && c->node <= 0x43) {
                /* (an object: hung from the same node of the body, as a chain is) */
            } else if (cs == WP_HAIR || cs == WP_EXTRAS) {
                int part = wp_chain_part(c->node, anchor);

                if (o2 >= WP_XL && o2 < WP_XL + WP_XLAYERS && wp_xTail[o2 - WP_XL]) {
                    if (wp_chain_slot(c->node, anchor) != WP_EXTRAS || (part != WP_WAIST && part != WP_HIPS)) {
                        continue; /* (a tail layer: only what hangs from the waist and the hips) */
                    }
                } else if (wp_chain_slot(c->node, anchor) != cs || (cs == WP_EXTRAS && part >= 0 && part != WP_EXTRAS && dm[part] != NULL)) {
                    continue; /* (not this slot's; or a part's chain, that part another fighter's: it brings its own) */
                }
            } else if (wp_chain_part(c->node, anchor) != cs) {
                continue; /* (a part's donor: only the chains hanging from that part) */
            }
            for (at = 0; at < b.outs && !(b.out[at].node == anchor && anchor < 0x47); at++) {
            }
            if (at == b.outs) {
                continue; /* (the body has no such node) */
            }
            for (end = j + 1; end < nrec[o2] && rec[o2][end].depth > c->depth; end++) {
            }
            if (b.outs + (end - j) > WP_MAX_RECS * 2) {
                *why = "Too many pieces for one model.";
                return -1;
            }
            for (q = 0; q < 3; q++) { /* (moved as the node it hangs from is) */
                delta[q] = ((const float *)(b.out[at].head + 0x10))[q] - ((const float *)(mdl[o2] + rec[o2][c->parent].off + 0x10))[q];
            }
            memmove(&b.out[at + 1 + (end - j)], &b.out[at + 1], sizeof(WpOut) * (size_t)(b.outs - at - 1));
            base = b.out[at].depth;
            for (a = j; a < end; a++) {
                WpOut *n = &b.out[at + 1 + (a - j)];

                n->head = mdl[o2] + rec[o2][a].off;
                memcpy(n->pd, delta, sizeof(delta));
                n->depth = base + 1 + (rec[o2][a].depth - c->depth);
                n->node = rec[o2][a].node;
                n->from = o2;
                n->b0 = b.bats;
                b.from = o2;
                b.want = 0;
                b.puff = o2 >= WP_LAYER && cs != WP_HAIR && cs != WP_EXTRAS ? WP_PUFF : 0.0f; /* (hair and accessories: added as they are) */
                b.s = b.g = 1.0f;
                b.even = 0;
                memcpy(b.dA, delta, sizeof(delta));
                memcpy(b.dB, delta, sizeof(delta));
                wp_batches((uint8_t *)mdl[o2], &rec[o2][a], wp_take_batch, &b);
                n->nb = b.bats - n->b0;
            }
            b.outs += end - j;
            j = end - 1;
        }
    }
    if (b.why != NULL) {
        *why = b.why;
        return -1;
    }
    memset(used, 0, sizeof(used));
    for (i = 0; i < b.outs; i++) {
        if (b.out[i].node < WP_NODES && (b.out[i].node < 0x47 || b.out[i].from == WP_BODY)) {
            used[b.out[i].node] = 1;
        }
    }
    memset(wp_chainFrom, 0xFF, sizeof(wp_chainFrom));
    memset(wp_chainOrig, 0xFF, sizeof(wp_chainOrig));
    for (i = 0; i < b.outs; i++) { /* a donor's chain node whose id is taken gets a free one */
        int orig = b.out[i].node;

        if (b.out[i].node >= 0x47 && b.out[i].from != WP_BODY && b.out[i].node < WP_NODES && used[b.out[i].node]) {
            while (nextNode < WP_NODES && used[nextNode]) {
                nextNode++;
            }
            if (nextNode >= WP_NODES) {
                *why = "Too many hair and cloth pieces for one model.";
                return -1;
            }
            b.out[i].node = nextNode;
        }
        if (b.out[i].node < WP_NODES) {
            used[b.out[i].node] = 1;
        }
        if (b.out[i].node >= 0x47 && b.out[i].node < WP_NODES) { /* (where its swing comes from) */
            wp_chainFrom[b.out[i].node] = b.out[i].from;
            wp_chainOrig[b.out[i].node] = orig;
        }
    }

    /* the face texture first among the kept: the face's (its sets draw into it), and the fade pass's */
    faceFrom = dm[WP_FACE] != NULL && dm[WP_FACE] != wp_empty ? WP_FACE : WP_BODY;
    face = wp_keep_pic(&k, faceFrom, ent[faceFrom][mdl[faceFrom][0x4C]].tbpOfs, ent[faceFrom][mdl[faceFrom][0x4C]].cbpOfs);
    fade = face < 0 ? -1 : wp_keep_pic(&k, WP_BODY, ent[WP_BODY][bm[0x4D]].tbpOfs, ent[WP_BODY][bm[0x4D]].cbpOfs);
    if (face < 0 || fade < 0) {
        *why = k.why != NULL ? k.why : "The face texture could not be kept.";
        return -1;
    }

    wp_txiN = 0; /* (the pictures' ramps, as this model draws them) */
    /* the mouth's shapes: their pictures and ramps kept, the shapes moved to where those are now */
    for (q = 0; q < 8; q++) {
        uint8_t *mm;
        uint32_t p;
        const char *whyBefore = k.why;

        free(wp_mouthOut[q]);
        wp_mouthOut[q] = NULL;
        if (wp_mouthIn[q] == NULL || wp_mouthN[q] < 0x40 || wp_mouthFrom < 0 || wp_mouthFrom >= WP_ORIGINS || (mm = malloc(wp_mouthN[q])) == NULL) {
            continue;
        }
        memcpy(mm, wp_mouthIn[q], wp_mouthN[q]);
        for (p = 0; p + 44 <= wp_mouthN[q]; p += 4) {
            uint64_t a, s2;
            int pi, ri;

            if (wp_u32(mm + p) != WP_VIF_BATCH) {
                continue;
            }
            memcpy(&a, mm + p + 20, 8);
            memcpy(&s2, mm + p + 36, 8);
            if (((a >> 20) & 63) != 20 && ((a >> 20) & 63) != 19) {
                continue;
            }
            pi = wp_keep_pic(&k, wp_mouthFrom, (uint32_t)(a & 0x3FFF), (uint32_t)((a >> 37) & 0x3FFF));
            ri = pi < 0 ? -1 : wp_keep_ramp(&k, wp_mouthFrom, (uint32_t)((s2 >> 37) & 0x3FFF));
            if (pi < 0 || ri < 0) {
                continue;
            }
            a = (a & ~(uint64_t)0x3FFF & ~((uint64_t)0x3FFF << 37)) | k.pic[pi].tbp | ((uint64_t)k.pic[pi].cbp << 37);
            s2 = (s2 & ~((uint64_t)0x3FFF << 37)) | ((uint64_t)k.ramp[ri].tbp << 37);
            memcpy(mm + p + 20, &a, 8);
            memcpy(mm + p + 36, &s2, 8);
        }
        k.why = whyBefore; /* (a shape's picture not found: that shape left as it was, the warrior still made) */
        wp_mouthOut[q] = mm;
    }

    /* write the model */
    recEnd = br[bn - 1].off + br[bn - 1].size;
    tail = bmn - recEnd;
    size = 0x90 + tail;
    for (i = 0; i < b.outs; i++) {
        uint32_t words = 0;

        for (j = b.out[i].b0; j < b.out[i].b0 + b.out[i].nb; j++) {
            words += 23 + 12 * (uint32_t)b.bat[j].n;
        }
        size += 0x40 + (b.out[i].nb > 0 ? 0x20 + (words / 4 + 2) * 16 * 2 + 16 : 0); /* (room for the tags' padding) */
    }
    if ((m = calloc(1, size)) == NULL) {
        *why = "Out of memory.";
        return -1;
    }
    { /* the layers: moved, lifted over the base, their triangles kept (what they cover is hidden) */
        static float base[60000][9], layer[30000][9];
        static WpBatch *lb[512];
        int nbase = 0, nl = 0, v2, c, any2 = 0;

        for (j = 0; j < b.bats; j++) {
            const WpBatch *bb = &b.bat[j];

            any2 |= bb->puff != 0.0f;
            for (v2 = 0; bb->puff == 0.0f && v2 + 2 < bb->n && nbase < 60000; v2++) {
                for (c = 0; c < 3; c++) {
                    const float *p = bb->v + (v2 + c) * 12;

                    wp_moved(bb, p, &base[nbase][c * 3]);
                }
                nbase++;
            }
        }
        for (i = 0; any2 && i < b.outs; i++) {
            int nlb = 0;

            for (j = b.out[i].b0; j < b.out[i].b0 + b.out[i].nb; j++) {
                WpBatch *bb = &b.bat[j];

                if (bb->puff == 0.0f || nlb >= 512 || (bb->own = malloc((size_t)bb->n * 48)) == NULL) {
                    continue;
                }
                memcpy(bb->own, bb->v, (size_t)bb->n * 48);
                for (v2 = 0; v2 < bb->n; v2++) {
                    float *p = bb->own + v2 * 12, at[3];

                    wp_moved(bb, p, at);
                    memcpy(p, at, 12);
                }
                lb[nlb++] = bb;
            }
            if (nlb > 0) {
                wp_lift_layers(lb, nlb, base, nbase);
            }
        }
        for (j = 0; j < b.bats; j++) {
            const WpBatch *bb = &b.bat[j];

            if (wp_dropOn && j < WP_MAX_BATCHES && wp_drop[j]) {
                continue; /* (taken off: hides nothing) */
            }
            for (v2 = 0; bb->own != NULL && v2 + 2 < bb->n && nl < 30000; v2++) {
                for (c = 0; c < 3; c++) {
                    memcpy(&layer[nl][c * 3], bb->own + (v2 + c) * 12, 12);
                }
                nl++;
            }
        }
        sLayer = layer;
        sLayers = nl;
    }
    { /* the shading each part from another fighter takes: its shading textures, each given the one in the same place of
         what it stands for (the most used for the most used: hair has two or three, its body, its tips, its shadow), one
         picked by hand, or the skin's, the outfit's */
        int at2, e;

        memset(shTop, 0, sizeof(shTop));
        memset(shRef, 0, sizeof(shRef));
        skinFrom = outFrom = -1;
        for (at2 = 0; at2 < b.outs; at2++) {
            for (j = b.out[at2].b0; j < b.out[at2].b0 + b.out[at2].nb; j++) {
                const WpBatch *bb = &b.bat[j];
                int nd = b.out[at2].node, f = bb->from, sl, hairy;
                uint32_t tb = wp_strip_ramp(bb->h);

                if (f < 0 || f >= WP_ORIGINS) {
                    continue;
                }
                hairy = wp_is_hair(hair[f], hairs[f], wp_tex_key(bb->h));
                sl = hairy && (nd >= 0x47 || wp_head_node(nd)) ? WP_HAIR : nd < 0x47 ? wp_slot_of(nd) : -1;
                if (wp_layer_slot(f) >= 0) {
                    wp_sh_add(&shTop[wp_layer_slot(f) * 2 + 1], tb); /* (a layer's) */
                } else {
                    if (f < WP_SLOTS) {
                        wp_sh_add(&shTop[f * 2], tb); /* (a part from another fighter) */
                    } else if (f == WP_BODY && sl >= 0 && (dm[sl] == NULL || dm[sl] == wp_empty)) {
                        wp_sh_add(&shTop[sl * 2], tb); /* (the body's own part) */
                    }
                    if (sl >= 0 && (sl == WP_HAIR ? f == hairFrom : f == (dm[sl] != NULL && dm[sl] != wp_empty ? sl : WP_BODY))) {
                        wp_sh_add(&shRef[sl * 2 + 1], tb); /* (the part a layer over it stands on) */
                    }
                    if (skinFrom < 0 && wp_head_node(nd) && !hairy) {
                        skinFrom = f, skinTb = tb;
                    }
                    if (outFrom < 0 && (nd == 17 || nd == 18)) {
                        outFrom = f, outTb = tb;
                    }
                }
            }
        }
        for (e = 0; e < WP_SH_ENTRIES; e++) {
            int sl = e / 2;

            shRefFrom[e] = e & 1 ? (sl == WP_HAIR ? hairFrom : dm[sl] != NULL && dm[sl] != wp_empty ? sl : WP_BODY) : WP_BODY;
            if (!(e & 1) && shTop[e].n > 0) { /* (a part: what the body has there, read from the body's own model) */
                WpShScan sc;

                sc.l = &shRef[e];
                sc.hair = hair[WP_BODY];
                sc.hairs = hairs[WP_BODY];
                sc.hairOnly = sl == WP_HAIR;
                for (i = 0; i < bn; i++) {
                    int nd = br[i].node, bs = nd < 0x47 ? wp_slot_of(nd) : wp_chain_slot_of(br, i);

                    if (br[i].size > 0x40 && (sl == WP_HAIR ? nd >= 0x47 ? bs == WP_HAIR : wp_head_node(nd) : nd < 0x47 && bs == sl)) {
                        wp_batches((uint8_t *)bm, &br[i], wp_sh_batch, &sc);
                    }
                }
            }
            wp_sh_sort(&shTop[e]);
            wp_sh_sort(&shRef[e]);
            wp_shInfoN[e][0] = shRef[e].n;
            wp_shInfoN[e][1] = shTop[e].n;
            for (i = 0; i < 2; i++) { /* (what the workshop shows of them) */
                const WpShList *l = i ? &shTop[e] : &shRef[e];
                int q;

                for (q = 0; q < l->n; q++) {
                    wp_ramp_rgb(&k, i ? (e & 1 ? WP_LAYER + sl : sl) : shRefFrom[e], l->tb[q], wp_shInfoRgb[e][i][q]);
                }
            }
        }
    }
    if (wp_patchLog) {
        int e, q;

        for (e = 0; e < WP_SH_ENTRIES; e++) {
            if (shTop[e].n > 0) {
                printf("shading entry %d (%s %d): mode %d, its own %d, what it stands for %d (origin %d)%c", e, e & 1 ? "layer" : "part", e / 2, wp_shMode[e],
                       shTop[e].n, shRef[e].n, shRefFrom[e], 10);
                for (q = 0; q < shTop[e].n && wp_patchLog >= 2; q++) {
                    printf("  own %d: block %u used %d%c", q, (unsigned)shTop[e].tb[q], shTop[e].use[q], 10);
                }
                for (q = 0; q < shRef[e].n && wp_patchLog >= 2; q++) {
                    printf("  ref %d: block %u used %d%c", q, (unsigned)shRef[e].tb[q], shRef[e].use[q], 10);
                }
            }
        }
    }
    memcpy(m, bm, 0x90);
    o = 0x90;
    for (i = 0; i < b.outs; i++) {
        WpOut *u = &b.out[i];
        uint8_t *r = m + o;
        uint16_t h[2];
        uint32_t count = 0, w;
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        int next = i + 1 < b.outs ? b.out[i + 1].depth : 0;

        memcpy(r, u->head, 0x40);
        for (q = 0; q < 3; q++) {
            ((float *)(r + 0x10))[q] += u->pd[q];
            ((float *)(r + 0x20))[q] += u->pd[q];
        }
        h[0] = (uint16_t)(i + 1 < b.outs ? u->depth + 1 - next : br[bn - 1].pop + (u->depth - br[bn - 1].depth));
        h[1] = (uint16_t)(i == b.outs - 1);
        memcpy(r + 4, h, 4);
        *(uint16_t *)(r + 0xA) = (uint16_t)u->node;
        recFile[i] = u->node < 0x47 ? wp_same(&k, u->from) : -1;
        if (recFile[i] >= 0 && u->nb > 0 && (fabsf(b.bat[u->b0].s - 1.0f) > 0.01f || fabsf(b.bat[u->b0].g - 1.0f) > 0.01f)) {
            recFile[i] = 100000 + i; /* (fitted to the body: its ends no longer meet its neighbours' exactly, so it is stitched) */
        }
        if (u->nb == 0) {
            *(uint16_t *)(r + 8) = 0;
            wp_put32(r, 0x40);
            o += 0x40;
            continue;
        }
        w = 0x60; /* the strips, in tags of 60000 quadwords at most */
        for (j = u->b0; j < u->b0 + u->nb;) {
            uint32_t tagAt = w, qwBytes;
            int first = j, last;

            w += 8;
            qwBytes = 0;
            while (j < u->b0 + u->nb && qwBytes / 16 + (23 + 12 * (uint32_t)b.bat[j].n) / 4 + 2 < 60000) {
                const WpBatch *bb = &b.bat[j];
                uint32_t *dst = (uint32_t *)(r + w);
                uint64_t a, s2;
                int pi, ri, v;

                dst[0] = WP_VIF_BATCH;
                if (wp_lastFroms < WP_MAX_BATCHES) {
                    wp_lastBat[wp_lastFroms] = j;
                    wp_lastBatOff[wp_lastFroms] = (uint32_t)((uint8_t *)(dst + 22) - m);
                    wp_lastFrom[wp_lastFroms++] = (uint8_t)bb->from;
                }
                memcpy(dst + 1, bb->h, 20 * 4);
                a = (uint64_t)dst[5] | ((uint64_t)dst[6] << 32);
                s2 = (uint64_t)dst[9] | ((uint64_t)dst[10] << 32);
                pi = wp_keep_pic(&k, bb->from, (uint32_t)(a & 0x3FFF), (uint32_t)((a >> 37) & 0x3FFF));
                if (pi >= 0 && pi < WP_MAX_TEX && picNode[pi] < 0 && bb->from >= 0 && bb->from < WP_ORIGINS) { /* (what each picture is on: its union below) */
                    picNode[pi] = u->node;
                    picHair[pi] = wp_is_hair(hair[bb->from], hairs[bb->from], wp_tex_key(bb->h)) && (u->node >= 0x47 || wp_head_node(u->node));
                }
                { /* (each part shaded as its entry says: wp_shMode, wp_shPick) */
                    int f = bb->from, e = -1, mode, q2, pick = 0, hy;
                    uint32_t mine = (uint32_t)((s2 >> 37) & 0x3FFF);

                    if (wp_layer_slot(f) >= 0) {
                        e = wp_layer_slot(f) * 2 + 1;
                    } else if (f >= 0 && f < WP_SLOTS) {
                        e = f * 2;
                    } else if (f == WP_BODY) { /* (the body's own part: which, by its node) */
                        hy = wp_is_hair(hair[f], hairs[f], wp_tex_key(bb->h));
                        q2 = hy && (u->node >= 0x47 || wp_head_node(u->node)) ? WP_HAIR : u->node < 0x47 ? wp_slot_of(u->node) : -1;
                        e = q2 >= 0 && (dm[q2] == NULL || dm[q2] == wp_empty) ? q2 * 2 : -1;
                    }
                    mode = e < 0 ? 1 : wp_shMode[e] >= 0 ? wp_shMode[e] : (e & 1) ? 0 : 1;
                    q2 = 0;
                    if (e >= 0) {
                        for (q2 = 0; q2 < shTop[e].n && shTop[e].tb[q2] != mine; q2++) {
                        }
                        pick = q2 < shTop[e].n && q2 < 8 ? wp_shPick[e][q2] : 0;
                    }
                    ri = -1;
                    { /* (shaded by its texture: a colour of the player's for every batch drawing that picture) */
                        uint32_t hk = pi >= 0 ? wp_pic_hash(&k, pi) : 0;
                        int tq;

                        for (tq = 0; hk != 0 && tq < wp_txN && wp_txKey[tq] != hk; tq++) {
                        }
                        if (hk != 0 && tq < wp_txN && wp_txRgb[tq] >= 0x1000000) {
                            ri = wp_keep_ramp_tint(&k, f, mine, wp_txRgb[tq]);
                        }
                        wp_txi_note(&k, hk, f, mine);
                    }
                    if (pi < 0 || ri >= 0) {
                    } else if (e >= 0 && e < WP_SH_ENTRIES && wp_shStyle[e] == 8 && wp_gen_word(e) == 0 && wp_sh_src(e) >= 0 &&
                               shTop[wp_sh_src(e)].n > 0) { /* (the same shading as another part: its most used ramp) */
                        int src = wp_sh_src(e), sf = (src & 1) ? WP_LAYER + src / 2 : dm[src / 2] != NULL && dm[src / 2] != wp_empty ? src / 2 : WP_BODY;

                        ri = wp_keep_ramp(&k, sf, shTop[src].tb[0]);
                    } else if (wp_gen_word(e) == 0 && wp_shfOn && (q2 = wp_shf_slot(e, f, u->node, picHair[pi >= 0 && pi < WP_MAX_TEX ? pi : 0])) >= 0 &&
                               wp_shfTb[q2] >= 0) { /* (the shading fighter's, of the same part) */
                        ri = wp_shfPeak[q2] >= 0 ? wp_keep_ramp_gen(&k, WP_SHF, (uint32_t)wp_shfTb[q2], pi, 7u | 100u << 6 | 1u << 14, (uint32_t)wp_shfPeak[q2])
                                                 : wp_keep_ramp(&k, WP_SHF, (uint32_t)wp_shfTb[q2]); /* (its pattern, in this part's own colours) */
                    } else if (wp_gen_word(e) != 0) { /* (shading made for it, from its picture's colours) */
                        ri = wp_keep_ramp_gen(&k, f, mine, pi, wp_gen_word(e), wp_gen_rgb(e));
                    } else if (pick >= 0x1000000) { /* (coloured by hand) */
                        ri = wp_keep_ramp_tint(&k, f, mine, (uint32_t)pick);
                    } else if (pick > 0 && shRef[e].n > 0) { /* (one of what it stands for, picked) */
                        ri = wp_keep_ramp(&k, shRefFrom[e], shRef[e].tb[pick - 1 < shRef[e].n ? pick - 1 : shRef[e].n - 1]);
                    } else if (pick == 0 && mode == 0 && e >= 0 && shRef[e].n > 0) { /* (what it stands for, in the same place) */
                        ri = wp_keep_ramp(&k, shRefFrom[e], shRef[e].tb[q2 < shRef[e].n ? q2 : shRef[e].n - 1]);
                    } else if (pick == 0 && mode == 2 && skinFrom >= 0) {
                        ri = wp_keep_ramp(&k, skinFrom, skinTb);
                    } else if (pick == 0 && mode == 3 && outFrom >= 0) {
                        ri = wp_keep_ramp(&k, outFrom, outTb);
                    }
                    if (ri < 0) {
                        ri = pi < 0 ? -1 : wp_keep_ramp(&k, f, mine);
                    }
                }
                if (pi < 0 || ri < 0) {
                    free(m);
                    *why = k.why != NULL ? k.why : "A part's texture could not be kept.";
                    return -1;
                }
                a = (a & ~(uint64_t)0x3FFF & ~((uint64_t)0x3FFF << 37)) | k.pic[pi].tbp | ((uint64_t)k.pic[pi].cbp << 37);
                s2 = (s2 & ~((uint64_t)0x3FFF << 37)) | ((uint64_t)k.ramp[ri].tbp << 37);
                dst[5] = (uint32_t)a;
                dst[6] = (uint32_t)(a >> 32);
                dst[9] = (uint32_t)s2;
                dst[10] = (uint32_t)(s2 >> 32);
                dst[21] = 0x6C008005u | ((uint32_t)(3 * bb->n) << 16);
                memcpy(dst + 22, bb->own != NULL ? bb->own : bb->v, (size_t)bb->n * 48);
                for (v = 0; bb->own == NULL && v < bb->n; v++) {
                    float *p = (float *)(dst + 22 + v * 12), at[3];

                    wp_moved(bb, p, at);
                    memcpy(p, at, 12);
                }
                if (bb->own != NULL) {
                    /* (a layer, lifted already) */
                } else if (bb->puff != 0.0f) { /* a layer stands off what it lies on */
                    for (v = 0; v < bb->n; v++) {
                        float *p = (float *)(dst + 22 + v * 12), nl = sqrtf(p[4] * p[4] + p[5] * p[5] + p[6] * p[6]);

                        for (q = 0; q < 3 && nl > 1e-5f; q++) {
                            p[q] += p[4 + q] / nl * bb->puff;
                        }
                    }
                } else if (sLayers > 0) {
                    wp_cull((float *)(dst + 22), bb->n);
                }
                for (v = 0; v < bb->n; v++) {
                    float *p = (float *)(dst + 22 + v * 12);

                    for (q = 0; q < 3; q++) {
                        lo[q] = p[q] < lo[q] ? p[q] : lo[q];
                        hi[q] = p[q] > hi[q] ? p[q] : hi[q];
                    }
                }
                dst[22 + 12 * bb->n] = 0x17000000u; /* MSCNT */
                w += (23 + 12 * (uint32_t)bb->n) * 4;
                qwBytes += (23 + 12 * (uint32_t)bb->n) * 4;
                count += (uint32_t)bb->n;
                j++;
            }
            last = j >= u->b0 + u->nb;
            while (((w - tagAt) & 15) != 0) { /* (the tag's quadword holds its first 8 bytes of data; the rest whole quadwords) */
                wp_put32(r + w, 0);
                w += 4;
            }
            wp_put32(r + tagAt, (last ? 0x60000000u : 0x10000000u) | ((w - tagAt - 16) / 16));
            wp_put32(r + tagAt + 4, 0);
            (void)first;
        }
        wp_put32(r + w, 0x70000000u);
        wp_put32(r + w + 4, 0);
        wp_put32(r + w + 8, 0);
        wp_put32(r + w + 12, 0);
        w += 16;
        wp_put32(r, w);
        *(uint16_t *)(r + 8) = (uint16_t)count;
        for (q = 0; q < 3; q++) { /* the bounds of what it has now */
            ((float *)(r + 0x40))[q] = (lo[q] + hi[q]) * 0.5f;
            ((float *)(r + 0x50))[q] = (hi[q] - lo[q]) * 0.5f;
        }
        ((float *)(r + 0x40))[3] = 1.0f;
        ((float *)(r + 0x50))[3] = sqrtf(((float *)(r + 0x50))[0] * ((float *)(r + 0x50))[0] + ((float *)(r + 0x50))[1] * ((float *)(r + 0x50))[1] +
                                         ((float *)(r + 0x50))[2] * ((float *)(r + 0x50))[2]);
        o += w;
    }
    memcpy(m + o, bm + recEnd, tail);
    wp_put32(m + 8, (uint32_t)b.outs);
    {
        uint32_t seam = wp_u32(bm + 0x70);

        if (seam != 0 && seam >= recEnd && seam + 16 <= bmn) { /* the body's seams: those on nodes it keeps stay */
            int32_t delta = (int32_t)(o - recEnd), sec[4];
            uint8_t *ss = m + seam + delta;
            int keep = 1;

            memcpy(sec, ss, 16);
            sec[2] += delta;
            sec[3] += delta;
            memcpy(ss, sec, 16);
            wp_put32(m + 0x70, (uint32_t)((int32_t)seam + delta));
            for (i = 0; i < sec[1] && keep; i++) {
                uint8_t *tri = m + sec[3] + i * 0x20;
                int32_t idx[3];
                int off = 0, v;
                uint64_t a, s2;
                int pi, ri;

                memcpy(idx, tri, 12);
                for (v = 0; v < 3; v++) {
                    int node;

                    if (idx[v] < 0 || idx[v] >= sec[0]) {
                        off = 1;
                        break;
                    }
                    memcpy(&node, m + sec[2] + idx[v] * 0x70 + 0x1C, 4);
                    if (node < 0 || node >= 0x47 || (wp_slot_of(node) >= 0 && (dm[wp_slot_of(node)] != NULL || dm[WP_LAYER + wp_slot_of(node)] != NULL)) ||
                        (wp_head_node(node) && dm[WP_HAIR] != NULL)) {
                        off = 1;
                    }
                }
                if (off) { /* (a seam onto a part now another fighter's: nothing) */
                    idx[1] = idx[2] = idx[0] = idx[0] >= 0 && idx[0] < sec[0] ? idx[0] : 0;
                    memcpy(tri, idx, 12);
                }
                memcpy(&a, tri + 0x10, 8);
                memcpy(&s2, tri + 0x18, 8);
                pi = wp_keep_pic(&k, WP_BODY, (uint32_t)(a & 0x3FFF), (uint32_t)((a >> 37) & 0x3FFF));
                ri = pi < 0 ? -1 : wp_keep_ramp(&k, WP_BODY, (uint32_t)((s2 >> 37) & 0x3FFF));
                if (pi < 0 || ri < 0) {
                    keep = 0;
                    break;
                }
                a = (a & ~(uint64_t)0x3FFF & ~((uint64_t)0x3FFF << 37)) | k.pic[pi].tbp | ((uint64_t)k.pic[pi].cbp << 37);
                s2 = (s2 & ~((uint64_t)0x3FFF << 37)) | ((uint64_t)k.ramp[ri].tbp << 37);
                memcpy(tri + 0x10, &a, 8);
                memcpy(tri + 0x18, &s2, 8);
            }
            if (!keep) {
                wp_put32(m + 0x70, 0); /* (no seams rather than wrong ones) */
            }
        }
    }
    size = o + tail;

    if (sizes != NULL) { /* (as it is before the sizes and builds: what touched is put back together after them) */
        beforeSizes = malloc(size);
        if (beforeSizes != NULL) {
            memcpy(beforeSizes, m, size);
        }
    }

    /* the sizes: the head (and all on it), the hair alone, the hands, the feet; each about its node's pivot */
    if (sizes != NULL) {
        static WpRec mr[WP_MAX_RECS * 2];
        int mn = wp_records(m, size, mr, WP_MAX_RECS * 2);
        static const int kRoot[WP_SIZE_ARMS][2] = {{47, -1}, {47, -1}, {21, 35}, {11, 15}};

        for (s = 0; s < WP_SIZE_ARMS && mn > 0; s++) { /* (the builds are below) */
            float f = sizes[s] > 0 ? (float)sizes[s] / 100.0f : 1.0f;

            if (f > 0.995f && f < 1.005f) {
                continue;
            }
            for (j = 0; j < 2; j++) {
                int root = kRoot[s][j] < 0 ? -1 : wp_find(mr, mn, kRoot[s][j]);
                WpScaleCtx sc;

                if (root < 0) {
                    continue;
                }
                memcpy(sc.pivot, m + mr[root].off + 0x10, 12);
                sc.s = f;
                for (i = root; i < mn && (i == root || mr[i].depth > mr[root].depth); i++) {
                    uint8_t *r = m + mr[i].off;
                    int q2;

                    if (s == WP_SIZE_HAIR && (mr[i].node < 0x47 || wp_chain_slot_of(mr, i) != WP_HAIR)) {
                        continue; /* (the hair: the head's chains only) */
                    }
                    sc.full = i != root;
                    if (i != root) { /* its pivots move with it; the root's own stays where the body joins it */
                        for (q2 = 0; q2 < 3; q2++) {
                            ((float *)(r + 0x10))[q2] = sc.pivot[q2] + f * (((float *)(r + 0x10))[q2] - sc.pivot[q2]);
                            ((float *)(r + 0x20))[q2] = sc.pivot[q2] + f * (((float *)(r + 0x20))[q2] - sc.pivot[q2]);
                            ((float *)(r + 0x30))[q2] = ((float *)(r + 0x10))[q2] - ((float *)(r + 0x20))[q2];
                        }
                    }
                    if (mr[i].size > 0x40) {
                        for (q2 = 0; q2 < 3; q2++) {
                            ((float *)(r + 0x40))[q2] = sc.pivot[q2] + f * (((float *)(r + 0x40))[q2] - sc.pivot[q2]);
                            ((float *)(r + 0x50))[q2] *= f;
                        }
                        ((float *)(r + 0x50))[3] *= f;
                        wp_batches(m, &mr[i], wp_scale_batch, &sc);
                    }
                }
            }
        }
    }

    /* the builds: the arms', torso's and legs' thickness, about each node's bone (its pivot towards its child; the torso
       upright), the length kept */
    if (sizes != NULL) {
        static WpRec mr[WP_MAX_RECS * 2];
        int mn = wp_records(m, size, mr, WP_MAX_RECS * 2);

        for (i = 0; i < mn; i++) {
            int g = wp_build_group(mr[i].node), c;
            float f, u[3], ul;
            WpThickCtx tc;

            if (g < 0 || mr[i].size <= 0x40 || sizes[g] <= 0 || sizes[g] == 100) {
                continue;
            }
            f = (float)sizes[g] / 100.0f;
            memcpy(tc.pivot, m + mr[i].off + 0x10, 12);
            if (g == WP_SIZE_TORSO) {
                u[0] = 0.0f, u[1] = 1.0f, u[2] = 0.0f;
            } else {
                for (c = i + 1; c < mn && !(mr[c].parent == i && mr[c].node < 0x47); c++) {
                }
                for (q = 0; q < 3; q++) {
                    u[q] = c < mn ? ((float *)(m + mr[c].off + 0x10))[q] - tc.pivot[q] : tc.pivot[q] - ((float *)(m + mr[i].off + 0x20))[q];
                }
            }
            ul = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
            if (ul < 1e-4f) {
                continue;
            }
            for (q = 0; q < 3; q++) {
                tc.u[q] = u[q] / ul;
            }
            tc.f = f;
            wp_batches(m, &mr[i], wp_thick_batch, &tc);
            for (q = 0; q < 4; q++) {
                ((float *)(m + mr[i].off + 0x50))[q] *= q == 1 && g == WP_SIZE_TORSO ? 1.0f : f > 1.0f ? f : 1.0f;
            }
            recFile[i] = recFile[i] >= 0 ? recFile[i] * 8 + 1 + g : recFile[i]; /* (a junction with another build is stitched) */
        }
    }

    if (beforeSizes != NULL) { /* the sizes and builds pulled seams apart (a thicker torso from its arms, a bigger head
                                  from its neck, the chest's halves down the middle): what touched touches again */
        wp_weld_again(m, size, beforeSizes);
        free(beforeSizes);
        beforeSizes = NULL;
    }

    /* stitching: where a node's strips come from another fighter than its parent's, its open edge goes onto the
       parent's (the hips onto the waist, their sibling) */
    {
        static WpRec mr[WP_MAX_RECS * 2];
        int mn = wp_records(m, size, mr, WP_MAX_RECS * 2);

        for (i = 0; i < mn; i++) {
            int p = mr[i].parent;

            if (mr[i].node >= 0x47 || mr[i].size <= 0x40) {
                continue;
            }
            if (mr[i].node == 8 || mr[i].node == 12) {
                p = wp_find(mr, mn, 16);
            }
            while (p >= 0 && mr[p].size <= 0x40 && mr[p].parent >= 0) { /* (past nodes without a mesh) */
                p = mr[p].parent;
            }
            if (p < 0 || mr[p].size <= 0x40 || mr[p].node >= 0x47 || recFile[i] < 0 || recFile[p] < 0 || recFile[i] == recFile[p]) {
                continue;
            }
            if (!wp_noStitch) {
                int sib = mr[i].node == 8 ? wp_find(mr, mn, 12) : mr[i].node == 12 ? wp_find(mr, mn, 8) : -1;

                wp_stitchSib = sib >= 0 && mr[sib].size > 0x40 ? &mr[sib] : NULL;
                wp_stitch(m, &mr[i], &mr[p]);
                wp_stitchSib = NULL;
            }
        }
    }

    { /* the body's unions: per node, that of the picture most of its strips use; its hair's */
        static int votes[WP_NODES][16], hvotes[16];
        WpUnionScan us;

        memset(votes, 0, sizeof(votes));
        memset(hvotes, 0, sizeof(hvotes));
        us.ent = ent[WP_BODY];
        us.count = k.texCount[WP_BODY];
        us.hair = hair[WP_BODY];
        us.hairs = hairs[WP_BODY];
        us.hvotes = hvotes;
        for (i = 0; i < bn; i++) {
            if (br[i].size > 0x40 && br[i].node < WP_NODES) {
                us.votes = votes[br[i].node];
                us.hairy = br[i].node >= 0x47 || wp_head_node(br[i].node);
                wp_batches((uint8_t *)bm, &br[i], wp_union_batch, &us);
            }
        }
        for (i = 0; i < WP_NODES; i++) {
            int best = -1, bv = 0;

            for (j = 0; j < 16; j++) {
                if (votes[i][j] > bv) {
                    bv = votes[i][j];
                    best = j;
                }
            }
            bodyNodeUnion[i] = best >= 0 ? (int)unionWord[best] : -1;
        }
        {
            int best = -1, bv = 0;

            for (j = 0; j < 16; j++) {
                if (hvotes[j] > bv) {
                    bv = hvotes[j];
                    best = j;
                }
            }
            bodyHairUnion = best >= 0 ? (int)unionWord[best] : -1;
        }
    }
    /* write the texture file: the kept pictures, the kept ramps, then any other entries of the body's */
    extra = k.count[WP_BODY] - k.texCount[WP_BODY] - k.rampCount[WP_BODY];
    nn = k.pics + k.ramps + extra;
    if (nn > WP_MAX_TEX) {
        free(m);
        *why = "Too many textures for one model.";
        return -1;
    }
    texSize = 0x20 + (uint32_t)nn * 0x40;
    for (i = 0; i < nn; i++) {
        const WpTex *e = i < k.pics ? &ent[k.pic[i].from][k.pic[i].index]
                         : i < k.pics + k.ramps ? &ent[k.ramp[i - k.pics].from][k.ramp[i - k.pics].index]
                                                : &ent[WP_BODY][k.texCount[WP_BODY] + k.rampCount[WP_BODY] + (i - k.pics - k.ramps)];

        texSize += ((e->pixOfs ? e->pixSize : 0) + 15) & ~15u;
        texSize += ((e->clutOfs ? e->clutSize : 0) + 15) & ~15u;
    }
    if ((t = calloc(1, texSize)) == NULL) {
        free(m);
        *why = "Out of memory.";
        return -1;
    }
    wp_put32(t, (uint32_t)nn);
    wp_put32(t + 4, 8);
    wp_put32(t + 8, k.tbp);
    wp_put32(t + 12, k.cbp);
    o = 0x20 + (uint32_t)nn * 0x40;
    for (i = 0; i < nn; i++) {
        int from = i < k.pics ? k.pic[i].from : i < k.pics + k.ramps ? k.ramp[i - k.pics].from : WP_BODY;
        WpTex e = i < k.pics ? ent[from][k.pic[i].index]
                  : i < k.pics + k.ramps ? ent[from][k.ramp[i - k.pics].index]
                                         : ent[WP_BODY][k.texCount[WP_BODY] + k.rampCount[WP_BODY] + (i - k.pics - k.ramps)];
        const uint8_t *src = from == WP_BODY ? bt : dt[from];

        if (i < k.pics) {
            e.tbpOfs = k.pic[i].tbp;
            e.cbpOfs = k.pic[i].cbp;
            if (from != WP_BODY) { /* the union (the outline's id: +0x2C, its slot, and +0x2D, the slot's alpha) of the body's
                                      picture on the same part: pieces that meet are one surface again, no line where
                                      another fighter's part (or a layer) joins the body (BtlObjMdl_InitAlphaSlots) */
                int on = picNode[i] < 0 && from == wp_mouthFrom ? 48 : picNode[i]; /* (a picture only the mouth's shapes draw, a
                                                                                 blink's: on the face, one surface with it) */
                int want = picHair[i] ? bodyHairUnion : on >= 0 && on < WP_NODES ? bodyNodeUnion[on] : -1;

                if (want < 0 && picNode[i] >= 0x47) { /* (an accessory's chain: the union of what it hangs from, else its own) */
                    want = -1;
                }
                if (want >= 0) { /* (the slot the body's; the glow, the slot's alpha, its own: a Super Saiyan's hair glows
                                    still, and the slot with it, below) */
                    e.unk2C = (e.unk2C & ~0xFFu) | ((uint32_t)want & 0xFFu);
                }
            }
            { /* the glow asked for: the entry's, else everyone's */
                int en = -1, g;

                if (wp_layer_slot(from) >= 0) {
                    en = wp_layer_slot(from) * 2 + 1;
                } else if (from >= 0 && from < WP_SLOTS) {
                    en = from * 2;
                }
                g = en >= 0 && wp_shGlow[en] > 0 ? wp_shGlow[en] : wp_shGlow[WP_GLOW_ALL];
                if (g > 0 && g < 5) {
                    e.unk2C = (e.unk2C & ~0xFF00u) | ((uint32_t)kWpGlow[g] << 8);
                } else if (wp_shfOn) { /* (the shading fighter's glow, of the same part) */
                    int sl = picHair[i] ? WP_HAIR : picNode[i] >= 0 && picNode[i] < 0x47 ? wp_slot_of(picNode[i]) : wp_layer_slot(from) >= 0 ? wp_layer_slot(from)
                                                                                                         : from >= 0 && from < WP_SLOTS ? from : -1;

                    if (sl >= 0 && sl < WP_SLOTS && wp_shfGlow[sl] >= 0) {
                        e.unk2C = (e.unk2C & ~0xFF00u) | ((uint32_t)wp_shfGlow[sl] << 8);
                    }
                }
            }
        } else if (i < k.pics + k.ramps) {
            e.tbpOfs = k.ramp[i - k.pics].tbp;
        }
        if (e.pixOfs) {
            memcpy(t + o, src + e.pixOfs * 4, e.pixSize);
            if (i >= k.pics && i < k.pics + k.ramps && k.ramp[i - k.pics].tint != 0 && e.pixSize >= 0x60 + 1024) {
                wp_tint_ramp(t + o + 0x60, k.ramp[i - k.pics].tint & 0x1FFFFFFF); /* (a ramp coloured by hand, its gradient) */
            }
            if (i >= k.pics && i < k.pics + k.ramps && k.ramp[i - k.pics].gen != 0 && e.pixSize >= 0x60 + 1024) {
                const WpKeep *gk = &k.ramp[i - k.pics];
                const WpKeep *gp = &k.pic[gk->genPic];
                const WpTex *pe = &ent[gp->from][gp->index];
                const uint8_t *ps = gp->from == WP_BODY ? bt : dt[gp->from];

                wp_gen_ramp(t + o + 0x60, gk->gen, gk->genRgb, pe->clutOfs ? wp_pal_main(ps + pe->clutOfs * 4 + 0x60, 16) : 0x909090);
            }
            e.pixOfs = o / 4;
            o += (e.pixSize + 15) & ~15u;
        }
        if (e.clutOfs) {
            memcpy(t + o, src + e.clutOfs * 4, e.clutSize);
            e.clutOfs = o / 4;
            o += (e.clutSize + 15) & ~15u;
        }
        memcpy(t + 0x20 + i * 0x40, &e, 0x30);
        memcpy(t + 0x20 + i * 0x40 + 0x30, &e.tex0, 8);
        if (i >= k.pics && i < k.pics + k.ramps && k.ramp[i - k.pics].gen != 0) { /* (marked in the words the game sets when it
                                                                                   reads the file: wp_shade_regen) */
            wp_put32(t + 0x20 + i * 0x40 + 0x38, 0x80000000u | ((uint32_t)k.ramp[i - k.pics].genPic & 0xFF) << 16 | (k.ramp[i - k.pics].gen & 0xFFFF));
            wp_put32(t + 0x20 + i * 0x40 + 0x3C, 0xA5000000u | (k.ramp[i - k.pics].genRgb & 0xFFFFFF));
        }
    }
    { /* the outline groups asked for: a group by hand, or one of its own (a slot no other part's picture has) */
        int en, q;

        for (en = 0; en < WP_SH_ENTRIES; en++) {
            int want = wp_shLine[en], used = 0, mine = -1;

            if (want <= 0) {
                continue;
            }
            for (i = 0; i < k.pics; i++) {
                int f = k.pic[i].from, pe = wp_layer_slot(f) >= 0 ? wp_layer_slot(f) * 2 + 1 : f >= 0 && f < WP_SLOTS ? f * 2 : -1;
                uint8_t *u = t + 0x20 + i * 0x40 + 0x2C;

                if (pe == en) {
                    mine = mine < 0 ? u[0] & 15 : mine;
                } else {
                    used |= 1 << (u[0] & 15);
                }
            }
            if (mine < 0) {
                continue; /* (none of its pictures here) */
            }
            if (want >= 2) {
                mine = (want - 2) & 15;
            } else if (used & (1 << mine)) { /* (its own: a slot nobody else's) */
                for (q = 15; q >= 0 && (used & (1 << q)); q--) {
                }
                mine = q >= 0 ? q : mine;
            }
            for (i = 0; i < k.pics; i++) {
                int f = k.pic[i].from, pe = wp_layer_slot(f) >= 0 ? wp_layer_slot(f) * 2 + 1 : f >= 0 && f < WP_SLOTS ? f * 2 : -1;

                if (pe == en) {
                    t[0x20 + i * 0x40 + 0x2C] = (uint8_t)mine;
                }
            }
        }
    }
    { /* each slot's glow: the brightest of its pictures' (the game takes the last's), all of them alike */
        uint8_t most[16];

        memset(most, 0, sizeof(most));
        for (i = 0; i < k.pics; i++) {
            uint8_t *u = t + 0x20 + i * 0x40 + 0x2C;

            most[u[0] & 15] = u[1] > most[u[0] & 15] ? u[1] : most[u[0] & 15];
        }
        for (i = 0; i < k.pics; i++) {
            uint8_t *u = t + 0x20 + i * 0x40 + 0x2C;
            int g = wp_layer_slot(k.pic[i].from) >= 0 ? wp_shGlow[wp_layer_slot(k.pic[i].from) * 2 + 1]
                    : k.pic[i].from >= 0 && k.pic[i].from < WP_SLOTS ? wp_shGlow[k.pic[i].from * 2] : 0;

            g = g > 0 ? g : wp_shGlow[WP_GLOW_ALL];
            u[1] = g == 1 ? 0 : most[u[0] & 15]; /* ("none" asked for: off, even in a glowing slot) */
        }
    }
    m[0x54] = 0; /* the texture whose slot alpha stays 0: where the body's went (none when it went) */
    for (i = 0; bm[0x54] != 0 && i < k.pics; i++) {
        if (k.pic[i].from == WP_BODY && k.pic[i].index == bm[0x54]) {
            m[0x54] = (uint8_t)i;
            break;
        }
    }
    m[0x4C] = (uint8_t)face;
    m[0x4D] = (uint8_t)fade;
    m[0x4F] = (uint8_t)k.pics;
    m[0x50] = (uint8_t)k.pics;
    m[0x51] = (uint8_t)k.ramps;
    for (j = 0; j < b.bats; j++) { /* (the layers' lifted copies) */
        free(b.bat[j].own);
        b.bat[j].own = NULL;
    }
    *om = m;
    *omn = size;
    *ot = t;
    *otn = texSize;
    return 1;
}

/* ---- a part's own colours: its textures copied ----
 * A colour can be for some parts only (colour=...,<parts>). Those parts' strips get copies of the textures they are
 * drawn with (the face texture excepted: its expressions draw into it), so their palettes are their own; a copy is
 * marked in its entry (word 10: WP_SPLIT_MARK | its group + 1) and the colours code keeps its regions apart. Groups
 * are the colour lines' part masks, in order (4 at most). Writes malloc'ed results; 1 made, 0 nothing to do, -1 failed. */
#define WP_SPLIT_MARK 0x5A000000u
#define WP_SPLIT_GROUPS 4

typedef struct WpSplitCtx {
    WpTex *ent;
    int texCount, n, group, failed;
    uint32_t tbp, cbp;
    int copyOf[WP_SPLIT_GROUPS][WP_MAX_TEX]; /* each texture's copy for each group, -1 none */
    int src[WP_MAX_TEX];                     /* each copy's original */
    int face;
} WpSplitCtx;

static void wp_split_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpSplitCtx *c = (WpSplitCtx *)ctx;
    uint64_t a = (uint64_t)h[4] | ((uint64_t)h[5] << 32);
    uint32_t tbp = (uint32_t)(a & 0x3FFF), cbp = (uint32_t)((a >> 37) & 0x3FFF);
    int t, cp;

    (void)v;
    (void)count;
    for (t = 0; t < c->texCount && (c->ent[t].tbpOfs != tbp || c->ent[t].cbpOfs != cbp); t++) {
    }
    if (t >= c->texCount || t == c->face || c->failed) {
        return;
    }
    cp = c->copyOf[c->group][t];
    if (cp < 0) {
        if (c->n >= WP_MAX_TEX || c->tbp + c->ent[t].tbpStep > WP_TEX_ROOM || c->cbp + c->ent[t].cbpStep > WP_CLUT_ROOM) {
            c->failed = 1;
            return;
        }
        cp = c->n++;
        c->src[cp] = t;
        c->copyOf[c->group][t] = cp;
        c->ent[cp] = c->ent[t];
        c->ent[cp].tbpOfs = c->tbp;
        c->ent[cp].cbpOfs = c->cbp;
        c->ent[cp].unk28 = WP_SPLIT_MARK | (uint32_t)(c->group + 1);
        c->tbp += c->ent[t].tbpStep;
        c->cbp += c->ent[t].cbpStep;
    }
    a = (a & ~(uint64_t)0x3FFF & ~((uint64_t)0x3FFF << 37)) | c->ent[cp].tbpOfs | ((uint64_t)c->ent[cp].cbpOfs << 37);
    h[4] = (uint32_t)a;
    h[5] = (uint32_t)(a >> 32);
}

static int wp_split(const uint8_t *m, uint32_t mn, const uint8_t *t, uint32_t tn, const int *groups, int ngroups, uint8_t **om, uint8_t **ot,
                    uint32_t *otn) {
    static WpSplitCtx c;
    static WpRec r[WP_MAX_RECS * 2];
    static WpTex orig[WP_MAX_TEX];
    int nr, ne, i, g, texCount, rampCount, extra, nn;
    uint32_t size, o;
    uint8_t *mm, *tt;

    *om = *ot = NULL;
    if (ngroups <= 0) {
        return 0;
    }
    nr = wp_records(m, mn, r, WP_MAX_RECS * 2);
    ne = wp_textures(t, tn, orig, WP_MAX_TEX);
    texCount = m[0x4F];
    rampCount = m[0x51];
    if (nr == 0 || ne == 0 || texCount + rampCount > ne) {
        return -1;
    }
    if ((mm = malloc(mn)) == NULL) {
        return -1;
    }
    memcpy(mm, m, mn);
    memset(&c, 0, sizeof(c));
    memset(c.copyOf, 0xFF, sizeof(c.copyOf));
    c.ent = orig;
    c.texCount = texCount;
    c.n = texCount;
    c.face = m[0x4C];
    memcpy(&c.tbp, t + 8, 4);
    memcpy(&c.cbp, t + 12, 4);
    for (g = 0; g < ngroups && g < WP_SPLIT_GROUPS; g++) {
        c.group = g;
        for (i = 0; i < nr; i++) {
            int slot = r[i].node < 0x47 ? wp_slot_of(r[i].node) : wp_chain_slot_of(r, i);

            if (slot >= 0 && (groups[g] & (1 << slot))) {
                wp_batches(mm, &r[i], wp_split_batch, &c);
            }
        }
    }
    if (c.failed || c.n == texCount) {
        free(mm);
        return c.failed ? -1 : 0;
    }
    /* the texture file: the pictures, the copies, the ramps, the rest */
    {
        static WpTex ent2[WP_MAX_TEX];
        static int from[WP_MAX_TEX];
        int copies = c.n - texCount;

        extra = ne - texCount - rampCount;
        nn = ne + copies;
        if (nn > WP_MAX_TEX || c.n > 255) {
            free(mm);
            return -1;
        }
        for (i = 0; i < nn; i++) {
            int s2 = i < texCount ? i : i < c.n ? i : i - copies;

            ent2[i] = i < c.n ? c.ent[i] : orig[s2];
            from[i] = i < texCount ? i : i < c.n ? c.src[i] : s2;
        }
        size = 0x20 + (uint32_t)nn * 0x40;
        for (i = 0; i < nn; i++) {
            size += ((ent2[i].pixOfs ? ent2[i].pixSize : 0) + 15) & ~15u;
            size += ((ent2[i].clutOfs ? ent2[i].clutSize : 0) + 15) & ~15u;
        }
        if ((tt = calloc(1, size)) == NULL) {
            free(mm);
            return -1;
        }
        memcpy(tt, t, 16);
        wp_put32(tt, (uint32_t)nn);
        wp_put32(tt + 4, 8);
        wp_put32(tt + 8, c.tbp);
        wp_put32(tt + 12, c.cbp);
        o = 0x20 + (uint32_t)nn * 0x40;
        for (i = 0; i < nn; i++) {
            WpTex e = ent2[i];
            const WpTex *src = &orig[from[i]];

            if (e.pixOfs) {
                memcpy(tt + o, t + src->pixOfs * 4, e.pixSize);
                e.pixOfs = o / 4;
                o += (e.pixSize + 15) & ~15u;
            }
            if (e.clutOfs) {
                memcpy(tt + o, t + src->clutOfs * 4, e.clutSize);
                e.clutOfs = o / 4;
                o += (e.clutSize + 15) & ~15u;
            }
            memcpy(tt + 0x20 + i * 0x40, &e, 0x30);
            memcpy(tt + 0x20 + i * 0x40 + 0x30, &e.tex0, 8);
        }
        mm[0x4F] = (uint8_t)c.n;
        mm[0x50] = (uint8_t)c.n;
        (void)extra;
    }
    *om = mm;
    *ot = tt;
    *otn = size;
    return 1;
}

/* ---- pieces: what a part is made of, each shown or hidden, stretched, moved, turned ----
 * A piece is a connected lump of a node's mesh (its strips joined where they share points): a strand of hair, a lump
 * of hair on the skull, an ear, a boot's cuff. Pieces are numbered per node, the largest first, so the same model
 * gives the same numbers. An edit is 12 numbers: the node (or -1 - slot: the whole part, each node about its joint),
 * the piece (-1 all of the node), hidden, the stretch in percent along x, y, z, the move in hundredths along x, y, z,
 * the turn in degrees about x, y, z. A whole part's edits are mirrored on the right side (nodes 12..15, 32..45), so
 * one edit keeps both sides alike. */
#define WP_MAX_PIECES 640
#define WP_EDIT_WORDS 12
#define WP_PATCH (-2000) /* an edit whose node is this: the holes patched (wp_patch); its x stretch the reach in hundredths */

typedef struct WpPiece {
    int node, comp, slot, rec, first, count; /* the vertices: off[first .. first + count) (byte offsets of their x y z) */
    int layer;                               /* 1: of the layer over its part, 2 + x: of extra layer x (wp_pieces' `from`) */
    float c[3];                              /* its middle */
} WpPiece;

/* Whether piece p is in "whole part" v of an edit (node -1 - v): a part's slot (0..11), the layer over slot s
   (WP_LAYER + s), extra layer x (WP_XL + x). */
static int wp_whole_has(const WpPiece *p, int v) {
    if (v >= WP_XL) {
        return p->layer == 2 + v - WP_XL;
    }
    return p->slot == v % WP_LAYER && p->layer == (v >= WP_LAYER ? 1 : 0);
}

typedef struct WpPieces {
    WpPiece p[WP_MAX_PIECES];
    int n;
    uint32_t *off;
    int offs, cap;
} WpPieces;

typedef struct WpPieceCtx {
    const uint8_t *base;
    float *at[WP_MAX_PTS];
    int id[WP_MAX_PTS], n, pts, strips[512][3], nstrips; /* (each strip: its first vertex, count, batch in the model) */
    float pos[WP_MAX_PTS][3];
    int posDup[WP_MAX_PTS];                              /* (each point's copy, -1 none) */
    uint32_t dupLo[16], dupHi[16];                       /* (the record's copies' tags: offsets in the model) */
    int dupK[16], dups;
} WpPieceCtx;

/* The copies' tags of record r (WP_DUP_TAG): into the context. */
static void wp_dup_tags(const uint8_t *m, const WpRec *r, WpPieceCtx *c) {
    uint32_t t = r->off + 0x60, end = r->off + r->size;

    c->dups = 0;
    while (r->size > 0x40 && t + 16 <= end) {
        uint32_t tag = wp_u32(m + t), addr = wp_u32(m + t + 4);

        if ((addr & 0xFFFF0000u) == WP_DUP_TAG && c->dups < 16) {
            c->dupLo[c->dups] = t;
            c->dupHi[c->dups] = t + 16 + (tag & 0xFFFF) * 16;
            c->dupK[c->dups++] = (int)(addr & 0xFFFF);
        }
        t += 16 + (tag & 0xFFFF) * 16;
        if (tag >> 28 == 6) {
            break;
        }
    }
}

static int wp_pieceBatch; /* (the batches seen so far, all records) */
static void wp_pieces_free(WpPieces *ps);

static void wp_piece_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpPieceCtx *c = (WpPieceCtx *)ctx;
    int i, j;

    int dup = -1;

    wp_pieceBatch++;
    for (i = 0; i < c->dups; i++) {
        uint32_t at = (uint32_t)((const uint8_t *)h - c->base);

        dup = at >= c->dupLo[i] && at < c->dupHi[i] ? c->dupK[i] : dup;
    }
    if (c->nstrips >= 512 || c->n + count > WP_MAX_PTS) {
        return;
    }
    c->strips[c->nstrips][0] = c->n;
    c->strips[c->nstrips][1] = count;
    c->strips[c->nstrips][2] = wp_pieceBatch - 1;
    c->nstrips++;
    for (i = 0; i < count; i++) {
        float *q = v + i * 12;

        c->at[c->n] = q;
        for (j = 0; j < c->pts; j++) {
            if (c->posDup[j] == dup && fabsf(c->pos[j][0] - q[0]) < WP_WELD && fabsf(c->pos[j][1] - q[1]) < WP_WELD && fabsf(c->pos[j][2] - q[2]) < WP_WELD) {
                break;
            }
        }
        if (j == c->pts) {
            memcpy(c->pos[j], q, 12);
            c->posDup[j] = dup;
            c->pts++;
        }
        c->id[c->n++] = j;
    }
}

static int wp_find_root(int *up, int a) {
    while (up[a] != a) {
        up[a] = up[up[a]];
        a = up[a];
    }
    return a;
}

/* The pieces of a model (malloc'ed offsets in ps->off; wp_pieces_free). `from` (NULL: none known): each batch's origin
   (wp_lastFrom of the wp_compose that made it), which marks a layer's pieces. */
static int wp_pieces(uint8_t *m, uint32_t mn, WpPieces *ps, const uint8_t *from, int froms) {
    static WpRec r[WP_MAX_RECS * 2];
    static WpPieceCtx c;
    static int up[WP_MAX_PTS], comp[WP_MAX_PTS], size[WP_MAX_PTS], order[WP_MAX_PTS], dupOf[WP_MAX_PTS];
    int nr = wp_records(m, mn, r, WP_MAX_RECS * 2), i, s, k, q;

    ps->n = 0;
    ps->offs = 0;
    wp_pieceBatch = 0;
    for (i = 0; i < nr; i++) {
        int ncomp = 0;

        if (r[i].size <= 0x40) {
            continue;
        }
        memset(&c, 0, sizeof(c));
        c.base = m;
        wp_dup_tags(m, &r[i], &c);
        wp_batches(m, &r[i], wp_piece_batch, &c);
        for (k = 0; k < c.pts; k++) {
            up[k] = k;
        }
        for (k = 0; k < c.pts; k++) { /* (a copy is one piece, whatever it is made of) */
            int j2;

            for (j2 = 0; c.posDup[k] >= 0 && j2 < k; j2++) {
                if (c.posDup[j2] == c.posDup[k]) {
                    up[wp_find_root(up, k)] = wp_find_root(up, j2);
                    break;
                }
            }
        }
        for (s = 0; s < c.nstrips; s++) { /* joined where a triangle joins them (a flat one too: a strip is one piece
                                             whatever the layers hid of it, so the pieces keep their numbers) */
            for (k = 0; k + 2 < c.strips[s][1]; k++) {
                int a = c.id[c.strips[s][0] + k], b = c.id[c.strips[s][0] + k + 1], d = c.id[c.strips[s][0] + k + 2];

                up[wp_find_root(up, b)] = wp_find_root(up, a);
                up[wp_find_root(up, d)] = wp_find_root(up, a);
            }
        }
        for (k = 0; k < c.pts; k++) {
            comp[k] = -1;
            size[k] = 0;
            dupOf[k] = -1;
        }
        for (k = 0; k < c.n; k++) { /* each lump, with how many vertices */
            int root = wp_find_root(up, c.id[k]);

            if (comp[root] < 0) {
                comp[root] = ncomp++;
            }
            size[comp[root]]++;
        }
        for (k = 0; k < ncomp; k++) {
            order[k] = k;
        }
        for (k = 0; k < c.pts; k++) { /* (each lump's copy, -1 an original) */
            int root = wp_find_root(up, k);

            if (comp[root] >= 0) {
                dupOf[comp[root]] = c.posDup[k];
            }
        }
        for (k = 1; k < ncomp; k++) { /* (the largest first; ties by first seen; the copies after, by number) */
            int v = order[k], j = k - 1;

            while (j >= 0 && (dupOf[v] < 0 ? dupOf[order[j]] >= 0 || size[order[j]] < size[v] : dupOf[order[j]] > dupOf[v])) {
                order[j + 1] = order[j];
                j--;
            }
            order[j + 1] = v;
        }
        for (q = 0; q < ncomp && ps->n < WP_MAX_PIECES; q++) {
            WpPiece *p = &ps->p[ps->n++];
            int want = order[q], j;

            p->node = r[i].node;
            p->comp = dupOf[want] >= 0 ? WP_DUP_FIRST + dupOf[want] : q;
            p->slot = r[i].node < 0x47 ? wp_slot_of(r[i].node) : wp_chain_slot_of(r, i);
            p->rec = i;
            p->first = ps->offs;
            p->count = 0;
            p->layer = 0;
            p->c[0] = p->c[1] = p->c[2] = 0.0f;
            for (k = 0; k < c.n; k++) {
                if (comp[wp_find_root(up, c.id[k])] != want) {
                    continue;
                }
                if (p->count == 0 && from != NULL) { /* (the origin of the batch its first vertex is in) */
                    int st;

                    for (st = 0; st < c.nstrips && k >= c.strips[st][0] + c.strips[st][1]; st++) {
                    }
                    if (st < c.nstrips && c.strips[st][2] < froms) {
                        int o = from[c.strips[st][2]];

                        p->layer = wp_layer_slot(o) < 0 ? 0 : o >= WP_XL && o < WP_XL + WP_XLAYERS ? 2 + o - WP_XL : 1;
                    }
                }
                if (ps->offs >= ps->cap) {
                    uint32_t *grow = realloc(ps->off, (size_t)(ps->cap + 8192) * 4);

                    if (grow == NULL) {
                        return ps->n;
                    }
                    ps->off = grow;
                    ps->cap += 8192;
                }
                ps->off[ps->offs++] = (uint32_t)((uint8_t *)c.at[k] - m);
                for (j = 0; j < 3; j++) {
                    p->c[j] += c.at[k][j];
                }
                p->count++;
            }
            for (j = 0; j < 3 && p->count > 0; j++) {
                p->c[j] /= (float)p->count;
            }
        }
    }
    return ps->n;
}

/* The copies the edits ask for (WP_DUP edits) made on a model: each its source piece's strips again (a piece is whole
   strips), puffed out a little along their normals (on the source, not into it; and not welded to it), at the end of
   the source's record in a tag of its own (WP_DUP_TAG | k: wp_pieces numbers it WP_DUP_FIRST + k). `fix`: the last
   build's strips' origins (wp_lastFrom, wp_lastBat, wp_lastBatOff) made to match, a copy's strips with its source's
   origin. 1 made (malloc'ed *om), 0 nothing to do. */
#define WP_DUP_PUFF 0.004f

typedef struct WpDupCtx {
    const uint8_t *base;
    uint32_t at[512];
    int count[512], n;
} WpDupCtx;

static void wp_dup_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpDupCtx *c = (WpDupCtx *)ctx;

    (void)v;
    if (c->n < 512) {
        c->at[c->n] = (uint32_t)((const uint8_t *)h - c->base) - 4; /* (the batch's first word: WP_VIF_BATCH) */
        c->count[c->n++] = count;
    }
}

static int wp_in_piece(const WpPieces *ps, int pi, uint32_t off) {
    int k;

    for (k = 0; k < ps->p[pi].count; k++) {
        if (ps->off[ps->p[pi].first + k] == off) {
            return 1;
        }
    }
    return 0;
}

static int wp_dups(const uint8_t *m, uint32_t mn, const int *edit, int edits, int fix, uint8_t **om, uint32_t *omn) {
    static WpRec r[WP_MAX_RECS * 2];
    static WpPieces ps;
    static WpDupCtx bc;
    static uint8_t nf[WP_MAX_BATCHES];
    static int nbat[WP_MAX_BATCHES];
    int nr = wp_records(m, mn, r, WP_MAX_RECS * 2), e, i, k, d, nd = 0, result = 0, g = 0, nn = 0;
    int dk[32], dsrc[32];
    uint32_t grow = 0, o;
    uint8_t *out = NULL;

    *om = NULL;
    *omn = 0;
    for (e = 0; e < edits && nd < 32; e++) {
        if (edit[e * WP_EDIT_WORDS] == WP_DUP && edit[e * WP_EDIT_WORDS + 1] >= 0 && edit[e * WP_EDIT_WORDS + 1] < 0xFFFF) {
            dk[nd] = edit[e * WP_EDIT_WORDS + 1];
            dsrc[nd++] = e;
        }
    }
    if (nd == 0 || nr == 0 || wp_pieces((uint8_t *)m, mn, &ps, NULL, 0) <= 0) {
        wp_pieces_free(&ps);
        return 0;
    }
    for (d = 0; d < nd; d++) { /* each copy's source piece (an original: a copy of a copy is of its source) */
        const int *x = edit + dsrc[d] * WP_EDIT_WORDS;

        for (i = 0; i < ps.n && (ps.p[i].node != x[3] || ps.p[i].comp != x[4] || x[4] >= WP_DUP_FIRST); i++) {
        }
        dsrc[d] = i < ps.n ? i : -1;
    }
    for (i = 0; i < nr; i++) { /* the room */
        bc.base = m;
        bc.n = 0;
        wp_batches((uint8_t *)m, &r[i], wp_dup_batch, &bc);
        for (d = 0; d < nd; d++) {
            if (dsrc[d] < 0 || ps.p[dsrc[d]].rec != i) {
                continue;
            }
            grow += 32;
            for (k = 0; k < bc.n; k++) {
                grow += wp_in_piece(&ps, dsrc[d], bc.at[k] + 22 * 4) ? (23 + 12 * (uint32_t)bc.count[k]) * 4 : 0;
            }
        }
    }
    if (grow == 0 || (out = calloc(1, mn + grow)) == NULL) {
        goto done;
    }
    memcpy(out, m, r[0].off);
    o = r[0].off;
    for (i = 0; i < nr; i++) {
        uint32_t t = r[i].off + 0x60, last = 0, end = r[i].off + r[i].size, w, added = 0, lastTag = 0;
        uint8_t *rr = out + o;
        int g0 = g, mine = 0;

        bc.base = m;
        bc.n = 0;
        wp_batches((uint8_t *)m, &r[i], wp_dup_batch, &bc);
        for (k = 0; k < bc.n && nn < WP_MAX_BATCHES; k++, g++) { /* (the origins of its strips as they were) */
            nf[nn] = g < wp_lastFroms ? wp_lastFrom[g] : 0;
            nbat[nn++] = g < wp_lastFroms ? wp_lastBat[g] : -1;
        }
        for (d = 0; d < nd; d++) {
            mine |= dsrc[d] >= 0 && ps.p[dsrc[d]].rec == i;
        }
        if (!mine || r[i].size <= 0x40) { /* (no copy: as it was) */
            memcpy(rr, m + r[i].off, r[i].size);
            o += r[i].size;
            continue;
        }
        while (t + 16 <= end) { /* its last tag */
            uint32_t tag = wp_u32(m + t);

            last = t;
            t += 16 + (tag & 0xFFFF) * 16;
            if (tag >> 28 == 6) {
                break;
            }
        }
        memcpy(rr, m + r[i].off, t - r[i].off);
        wp_put32(rr + (last - r[i].off), (wp_u32(m + last) & 0x0FFFFFFFu) | 0x10000000u); /* (no longer the last) */
        w = t - r[i].off;
        for (d = 0; d < nd; d++) {
            uint32_t tagAt = w;

            if (dsrc[d] < 0 || ps.p[dsrc[d]].rec != i) {
                continue;
            }
            w += 8;
            for (k = 0; k < bc.n; k++) { /* the source's strips, again */
                uint32_t bytes = (23 + 12 * (uint32_t)bc.count[k]) * 4;
                float *v;
                int q;

                if (!wp_in_piece(&ps, dsrc[d], bc.at[k] + 22 * 4)) {
                    continue;
                }
                memcpy(rr + w, m + bc.at[k], bytes);
                v = (float *)(rr + w + 22 * 4);
                for (q = 0; q < bc.count[k]; q++) { /* (puffed out: over the source, not fighting it) */
                    v[q * 12] += v[q * 12 + 4] * WP_DUP_PUFF;
                    v[q * 12 + 1] += v[q * 12 + 5] * WP_DUP_PUFF;
                    v[q * 12 + 2] += v[q * 12 + 6] * WP_DUP_PUFF;
                }
                w += bytes;
                added += (uint32_t)bc.count[k];
                if (nn < WP_MAX_BATCHES) {
                    nf[nn] = g0 + k < wp_lastFroms ? wp_lastFrom[g0 + k] : 0;
                    nbat[nn++] = -1;
                }
            }
            while (((w - tagAt) & 15) != 0) {
                wp_put32(rr + w, 0);
                w += 4;
            }
            wp_put32(rr + tagAt, 0x10000000u | ((w - tagAt - 16) / 16));
            wp_put32(rr + tagAt + 4, WP_DUP_TAG | (uint32_t)dk[d]);
            lastTag = tagAt;
        }
        wp_put32(rr + lastTag, (wp_u32(rr + lastTag) & 0x0FFFFFFFu) | 0x60000000u); /* (the record's last) */
        memcpy(rr + w, m + t, end - t); /* (its end) */
        w += end - t;
        wp_put32(rr, w);
        *(uint16_t *)(rr + 8) = (uint16_t)(*(const uint16_t *)(m + r[i].off + 8) + added);
        o += w;
    }
    {
        uint32_t recEnd = r[nr - 1].off + r[nr - 1].size, seam = wp_u32(m + 0x70);
        int32_t delta = (int32_t)(o - recEnd);

        memcpy(out + o, m + recEnd, mn - recEnd);
        if (seam != 0 && seam >= recEnd && seam + 16 <= mn) { /* (the seams' offsets, after the records) */
            int32_t sec[4];

            memcpy(sec, out + seam + delta, 16);
            sec[2] += delta;
            sec[3] += delta;
            memcpy(out + seam + delta, sec, 16);
            wp_put32(out + 0x70, (uint32_t)((int32_t)seam + delta));
        }
        *om = out;
        *omn = o + (mn - recEnd);
    }
    if (fix && g == wp_lastFroms) { /* the origins, the copies' strips among them */
        static WpRec r2[WP_MAX_RECS * 2];
        int n2 = wp_records(out, *omn, r2, WP_MAX_RECS * 2), b = 0;

        memcpy(wp_lastFrom, nf, (size_t)nn);
        memcpy(wp_lastBat, nbat, (size_t)nn * sizeof(int));
        wp_lastFroms = nn;
        for (i = 0; i < n2; i++) {
            bc.base = out;
            bc.n = 0;
            wp_batches(out, &r2[i], wp_dup_batch, &bc);
            for (k = 0; k < bc.n && b < nn; k++) {
                wp_lastBatOff[b++] = bc.at[k] + 22 * 4;
            }
        }
    }
    result = 1;
done:
    wp_pieces_free(&ps);
    return result;
}

static void wp_pieces_free(WpPieces *ps) {
    free(ps->off);
    ps->off = NULL;
    ps->cap = ps->offs = ps->n = 0;
}

static int wp_right_side(int node) {
    return (node >= 12 && node <= 15) || (node >= 32 && node <= 45);
}

/* The joint at the far end of a node that is a link between two joints (a thigh, a shin, an arm's halves, the torso's
   pieces: their length is the skeleton's), -1 an end (a hand, a foot, the head). */
static int wp_far_joint(int node) {
    switch (node) {
    case 9: case 10: case 13: case 14: case 19: case 20: case 33: case 34: return node + 1;
    case 8: case 12: case 16: case 17: case 18: case 32: return 46;
    }
    return -1;
}

/* Applies the edits to the model `m` whose pieces are `ps` (found on a model of the same layout: offsets alike). */
/* Where piece `p` touches the rest of the model (a strand's root on the scalp, an ear on the head): fall[k] for each of
   its vertices, 0 where it touches, rising to 1 at its far end (1 everywhere when it touches nothing), and *root, the
   middle of where it touches. 1 when it touches. An edit of the piece is then faded in by fall, about root: it bends
   away from where it is fixed instead of tearing off and leaving a hole. */
#define WP_TOUCH 0.03f

static int wp_attached(const uint8_t *layout, const WpPieces *ps, int pi, const float (*box)[6], float *fall, float *root, const float *joint) {
    const WpPiece *p = &ps->p[pi];
    static uint8_t touch[8192];
    int k, q, j, n = p->count < 8192 ? p->count : 8192, touches = 0;
    float far2 = 0.0f;

    memset(touch, 0, (size_t)n);
    root[0] = root[1] = root[2] = 0.0f;
    for (q = 0; q < ps->n && p->comp < WP_DUP_FIRST; q++) { /* (a copy: moved whole, wherever it is put) */
        const WpPiece *o = &ps->p[q];

        if (q == pi || o->count == 0 || o->comp >= WP_DUP_FIRST || box[q][0] > box[pi][3] + WP_TOUCH || box[q][3] < box[pi][0] - WP_TOUCH || box[q][1] > box[pi][4] + WP_TOUCH ||
            box[q][4] < box[pi][1] - WP_TOUCH || box[q][2] > box[pi][5] + WP_TOUCH || box[q][5] < box[pi][2] - WP_TOUCH) {
            continue;
        }
        for (k = 0; k < n; k++) {
            const float *v = (const float *)(layout + ps->off[p->first + k]);

            if (touch[k]) {
                continue;
            }
            for (j = 0; j < o->count; j++) {
                const float *w = (const float *)(layout + ps->off[o->first + j]);

                if (fabsf(v[0] - w[0]) < WP_TOUCH && fabsf(v[1] - w[1]) < WP_TOUCH && fabsf(v[2] - w[2]) < WP_TOUCH) {
                    touch[k] = 1;
                    break;
                }
            }
        }
    }
    if (joint != NULL) { /* only its root holds it: where it touches near the joint it hangs from (a strand lying along the
                            hair touches it all the way down, and was held there: it only stretched) */
        float dmin = 1e30f, dlo = 1e30f, dhi = 0.0f;

        for (k = 0; k < n; k++) {
            const float *v = (const float *)(layout + ps->off[p->first + k]);
            float d = sqrtf((v[0] - joint[0]) * (v[0] - joint[0]) + (v[1] - joint[1]) * (v[1] - joint[1]) + (v[2] - joint[2]) * (v[2] - joint[2]));

            dlo = d < dlo ? d : dlo;
            dhi = d > dhi ? d : dhi;
            if (touch[k]) {
                dmin = d < dmin ? d : dmin;
            }
        }
        for (k = 0; k < n; k++) {
            const float *v = (const float *)(layout + ps->off[p->first + k]);
            float d = sqrtf((v[0] - joint[0]) * (v[0] - joint[0]) + (v[1] - joint[1]) * (v[1] - joint[1]) + (v[2] - joint[2]) * (v[2] - joint[2]));

            if (touch[k] && d > dmin + 0.25f * (dhi - dlo)) {
                touch[k] = 0;
            }
        }
    }
    for (k = 0; k < n; k++) {
        if (touch[k]) {
            const float *v = (const float *)(layout + ps->off[p->first + k]);

            for (q = 0; q < 3; q++) {
                root[q] += v[q];
            }
            touches++;
        }
    }
    if (touches == 0 || touches == n) {
        for (k = 0; k < p->count; k++) {
            fall[k] = 1.0f;
        }
        return 0;
    }
    for (q = 0; q < 3; q++) {
        root[q] /= (float)touches;
    }
    for (k = 0; k < p->count; k++) { /* how far from where it touches */
        const float *v = (const float *)(layout + ps->off[p->first + k]);
        float best = 1e30f;

        for (j = 0; j < n; j++) {
            const float *w = (const float *)(layout + ps->off[p->first + j]);
            float d;

            if (!touch[j]) {
                continue;
            }
            d = (v[0] - w[0]) * (v[0] - w[0]) + (v[1] - w[1]) * (v[1] - w[1]) + (v[2] - w[2]) * (v[2] - w[2]);
            best = d < best ? d : best;
        }
        fall[k] = best;
        far2 = best > far2 ? best : far2;
    }
    for (k = 0; k < p->count; k++) { /* it moves whole; only its first fifth bends, to keep its root on (smoothly) */
        float t = far2 > 1e-8f ? sqrtf(fall[k] / far2) / 0.2f : 1.0f;

        t = t > 1.0f ? 1.0f : t;
        fall[k] = t * t * (3.0f - 2.0f * t);
    }
    return 1;
}

/* ---- what a hidden piece is laid onto: the triangles of the pieces that stay, near it ---- */
#define WP_UNDER_TRIS 12000

typedef struct WpUnder {
    const uint8_t *base;
    const uint32_t *skip; /* (the hidden pieces' vertices, sorted: their triangles are not under anything) */
    int skips, n;
    float lo[3], hi[3];
    float t[WP_UNDER_TRIS][9], nrm[WP_UNDER_TRIS][3];
} WpUnder;

static int wp_skipped(const WpUnder *u, uint32_t off) {
    int a = 0, b = u->skips - 1;

    while (a <= b) {
        int c = (a + b) / 2;

        if (u->skip[c] == off) {
            return 1;
        }
        if (u->skip[c] < off) {
            a = c + 1;
        } else {
            b = c - 1;
        }
    }
    return 0;
}

static void wp_under_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpUnder *u = (WpUnder *)ctx;
    int i, q, c;

    (void)h;
    for (i = 0; i + 2 < count && u->n < WP_UNDER_TRIS; i++) {
        const float *a = v + i * 12, *b = a + 12, *d = a + 24;
        float lo[3], hi[3], e1[3], e2[3], nn[3], l;
        int out = 0;

        for (q = 0; q < 3; q++) {
            lo[q] = a[q] < b[q] ? (a[q] < d[q] ? a[q] : d[q]) : (b[q] < d[q] ? b[q] : d[q]); /* (not fminf: see warrior_fx_hsv) */
            hi[q] = a[q] > b[q] ? (a[q] > d[q] ? a[q] : d[q]) : (b[q] > d[q] ? b[q] : d[q]);
            out |= lo[q] > u->hi[q] || hi[q] < u->lo[q];
        }
        if (out || wp_skipped(u, (uint32_t)((const uint8_t *)a - u->base)) || wp_skipped(u, (uint32_t)((const uint8_t *)b - u->base)) ||
            wp_skipped(u, (uint32_t)((const uint8_t *)d - u->base))) {
            continue;
        }
        for (q = 0; q < 3; q++) {
            e1[q] = b[q] - a[q];
            e2[q] = d[q] - a[q];
        }
        nn[0] = e1[1] * e2[2] - e1[2] * e2[1];
        nn[1] = e1[2] * e2[0] - e1[0] * e2[2];
        nn[2] = e1[0] * e2[1] - e1[1] * e2[0];
        l = sqrtf(nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2]);
        if (l < 1e-9f) {
            continue; /* (a strip's joining triangle) */
        }
        for (c = 0; c < 3; c++) {
            const float *w = c == 0 ? a : c == 1 ? b : d;

            for (q = 0; q < 3; q++) {
                u->t[u->n][c * 3 + q] = w[q];
            }
        }
        { /* (its outside: the way its corners' normals point) */
            float sn = (a[4] + b[4] + d[4]) * nn[0] + (a[5] + b[5] + d[5]) * nn[1] + (a[6] + b[6] + d[6]) * nn[2];

            for (q = 0; q < 3; q++) {
                u->nrm[u->n][q] = nn[q] / l * (sn < 0.0f ? -1.0f : 1.0f);
            }
        }
        u->n++;
    }
}

/* The nearest point to p on triangle t (9 floats): into out, returns the squared distance. */
static float wp_tri_nearest(const float *p, const float *t, float *out) {
    const float *a = t, *b = t + 3, *c = t + 6;
    float ab[3], ac[3], ap[3], d1 = 0, d2 = 0, d3 = 0, d4 = 0, d5 = 0, d6 = 0, va, vb, vc, v, w, den;
    int q;

    for (q = 0; q < 3; q++) {
        ab[q] = b[q] - a[q];
        ac[q] = c[q] - a[q];
        ap[q] = p[q] - a[q];
        d1 += ab[q] * ap[q];
        d2 += ac[q] * ap[q];
    }
    if (d1 <= 0 && d2 <= 0) {
        memcpy(out, a, 12);
        goto done;
    }
    for (q = 0; q < 3; q++) {
        d3 += ab[q] * (p[q] - b[q]);
        d4 += ac[q] * (p[q] - b[q]);
        d5 += ab[q] * (p[q] - c[q]);
        d6 += ac[q] * (p[q] - c[q]);
    }
    if (d3 >= 0 && d4 <= d3) {
        memcpy(out, b, 12);
        goto done;
    }
    if (d6 >= 0 && d5 <= d6) {
        memcpy(out, c, 12);
        goto done;
    }
    vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) {
        v = d1 / (d1 - d3);
        for (q = 0; q < 3; q++) {
            out[q] = a[q] + ab[q] * v;
        }
        goto done;
    }
    vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) {
        w = d2 / (d2 - d6);
        for (q = 0; q < 3; q++) {
            out[q] = a[q] + ac[q] * w;
        }
        goto done;
    }
    va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        for (q = 0; q < 3; q++) {
            out[q] = b[q] + (c[q] - b[q]) * w;
        }
        goto done;
    }
    den = 1.0f / (va + vb + vc);
    v = vb * den;
    w = vc * den;
    for (q = 0; q < 3; q++) {
        out[q] = a[q] + ab[q] * v + ac[q] * w;
    }
done:
    return (out[0] - p[0]) * (out[0] - p[0]) + (out[1] - p[1]) * (out[1] - p[1]) + (out[2] - p[2]) * (out[2] - p[2]);
}

static int wp_u32cmp(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;

    return x < y ? -1 : x > y;
}

static void wp_apply_edits(uint8_t *m, const uint8_t *layout, const WpPieces *ps, const int *edit, int edits) {
    static WpRec r[WP_MAX_RECS * 2];
    static float box[WP_MAX_PIECES][6];
    int nr = wp_records(layout, 1u << 30, r, WP_MAX_RECS * 2), e, i, k, j;
    float *fall = NULL;

    for (i = 0; i < ps->n; i++) { /* each piece's bounds as the look has it */
        box[i][0] = box[i][1] = box[i][2] = 1e30f;
        box[i][3] = box[i][4] = box[i][5] = -1e30f;
        for (k = 0; k < ps->p[i].count; k++) {
            const float *v = (const float *)(layout + ps->off[ps->p[i].first + k]);

            for (j = 0; j < 3; j++) {
                box[i][j] = v[j] < box[i][j] ? v[j] : box[i][j];
                box[i][3 + j] = v[j] > box[i][3 + j] ? v[j] : box[i][3 + j];
            }
        }
    }

    for (e = 0; e < edits; e++) {
        const int *x = edit + e * WP_EDIT_WORDS;
        int whole = x[0] < 0;

        for (i = 0; x[0] != WP_PATCH && x[0] != WP_SHADE && x[0] != WP_DUP && i < ps->n; i++) {
            const WpPiece *p = &ps->p[i];
            float c[3], s[3], t[3], ang[3], cs[3], sn[3], axis[3] = {0.0f, 0.0f, 0.0f}, thick = 1.0f, len = 0.0f, cl[3];
            int mirror, link = 0, tail = 0, up = -1;

            if (whole ? !wp_whole_has(p, -1 - x[0]) : p->node != x[0] || (x[1] >= 0 && p->comp != x[1])) {
                /* (a hand's or foot's piece reaching above its joint, a trouser's cuff on the boot: thinned with the limb) */
                if (!whole || p->layer || -1 - x[0] >= WP_LAYER || x[2] == 1 || (p->node != 11 && p->node != 15 && p->node != 21 && p->node != 35) ||
                    wp_slot_of(p->node - 1) != -1 - x[0] || (up = wp_find(r, nr, p->node - 1)) < 0) {
                    continue;
                }
                tail = 1;
            }
            mirror = whole && wp_right_side(p->node);
            free(fall);
            fall = NULL;
            if (tail) { /* (about the limb's bone) */
                memcpy(c, layout + r[up].off + 0x10, 12);
            } else if (whole && p->rec < nr) { /* (a whole part: all of it about one point, the joint it hangs from, the
                                                     side's own for a pair: the fingers turn and grow with the hand, the
                                                     hair's strands with the head, not each about its own joint) */
                int root = p->rec, side = wp_right_side(r[p->rec].node), up2;

                while ((up2 = r[root].parent) >= 0 && up2 < nr && wp_right_side(r[up2].node) == side &&
                       (r[up2].node < 0x47 ? wp_slot_of(r[up2].node) : wp_chain_slot_of(r, up2)) == p->slot) {
                    root = up2;
                }
                if (r[root].node >= 0x47 && r[root].parent >= 0 && r[root].parent < nr) {
                    root = r[root].parent; /* (a chain's: the joint the chains hang from, one for all the strands) */
                }
                if ((p->slot == WP_HAIR || p->slot == WP_HEAD || p->slot == WP_FACE) && (up2 = wp_find(r, nr, 46)) >= 0) {
                    root = up2; /* (the hair, the head and the face: one point, the head's joint, so turned alike they stay as one) */
                }
                memcpy(c, layout + r[root].off + 0x10, 12);
            } else {
                memcpy(c, p->c, 12);
                if (!wp_noAttach && (fall = malloc((size_t)p->count * sizeof(float))) != NULL) { /* (a piece: turned and grown about its root, where it touches the rest) */
                    wp_attached(layout, ps, i, (const float (*)[6])box, fall, c, p->rec < nr ? (const float *)(layout + r[p->rec].off + 0x10) : NULL);
                    if (c[0] == 0.0f && c[1] == 0.0f && c[2] == 0.0f) {
                        memcpy(c, p->c, 12);
                    }
                }
            }
            for (j = 0; j < 3; j++) {
                s[j] = (x[3 + j] > 0 ? (float)x[3 + j] : 100.0f) / 100.0f;
                t[j] = (float)x[6 + j] / 100.0f;
                ang[j] = tail ? 0.0f : (float)x[9 + j] * 0.0174532925f;
                t[j] = tail ? 0.0f : t[j];
            }
            if (mirror) {
                t[0] = -t[0];
                ang[1] = -ang[1];
                ang[2] = -ang[2];
            }
            for (j = 0; j < 3; j++) {
                cs[j] = cosf(ang[j]);
                sn[j] = sinf(ang[j]);
            }
            memcpy(cl, c, 12);
            if (whole && !tail && p->rec < nr) {
                memcpy(cl, layout + r[p->rec].off + 0x10, 12); /* (a link's own joint: its bone) */
            }
            if (whole && (tail || wp_far_joint(p->node) >= 0)) { /* a link of the skeleton: thicker or thinner about its bone, as long */
                int farRec = tail ? p->rec : wp_find(r, nr, wp_far_joint(p->node));

                if (farRec >= 0) {
                    for (j = 0; j < 3; j++) {
                        axis[j] = ((const float *)(layout + r[farRec].off + 0x10))[j] - cl[j];
                        len += axis[j] * axis[j];
                    }
                    len = sqrtf(len);
                }
                if (len > 1e-3f) {
                    for (j = 0; j < 3; j++) {
                        axis[j] /= len;
                    }
                    thick = (s[0] + s[1] + s[2]) / 3.0f;
                    link = 1;
                }
            }
            for (k = 0; k < p->count; k++) {
                float *v = (float *)(m + ps->off[p->first + k]), a[3], b[3], n[3];

                if (x[2] == 1) { /* hidden: all of it on one point (2: only this one is kept, wp_drop_hidden) */
                    memcpy(v, c, 12);
                    continue;
                }
                for (j = 0; j < 3; j++) {
                    a[j] = (v[j] - c[j]) * (link ? 1.0f : s[j]);
                    n[j] = v[4 + j];
                }
                if (link) { /* (along the bone as it is, across it `thick`; then about the part's point) */
                    float along;

                    for (j = 0; j < 3; j++) {
                        a[j] = v[j] - cl[j];
                    }
                    along = a[0] * axis[0] + a[1] * axis[1] + a[2] * axis[2];
                    if (tail && along >= len) {
                        continue; /* (the hand or foot itself: as it is) */
                    }

                    for (j = 0; j < 3; j++) {
                        a[j] = along * axis[j] + (a[j] - along * axis[j]) * thick + cl[j] - c[j];
                    }
                }
                /* turned about z, then x, then y */
                b[0] = a[0] * cs[2] - a[1] * sn[2], b[1] = a[0] * sn[2] + a[1] * cs[2], b[2] = a[2];
                a[0] = b[0], a[1] = b[1] * cs[0] - b[2] * sn[0], a[2] = b[1] * sn[0] + b[2] * cs[0];
                b[0] = a[0] * cs[1] + a[2] * sn[1], b[1] = a[1], b[2] = -a[0] * sn[1] + a[2] * cs[1];
                for (j = 0; j < 3; j++) {
                    v[j] = c[j] + b[j] + t[j];
                }
                a[0] = n[0] * cs[2] - n[1] * sn[2], a[1] = n[0] * sn[2] + n[1] * cs[2], a[2] = n[2];
                b[0] = a[0], b[1] = a[1] * cs[0] - a[2] * sn[0], b[2] = a[1] * sn[0] + a[2] * cs[0];
                v[4] = b[0] * cs[1] + b[2] * sn[1], v[5] = b[1], v[6] = -b[0] * sn[1] + b[2] * cs[1];
                if (!whole && (t[0] != 0.0f || t[1] != 0.0f || t[2] != 0.0f || ang[0] != 0.0f || ang[1] != 0.0f || ang[2] != 0.0f)) {
                    v[3] = v[3] >= 0.5f ? 1.0f : 0.0f; /* (moved or turned: one bone, the one it mostly followed; blended
                                                          between two pivots it no longer sits between, it stretched) */
                }
            }
        }
    }
    free(fall);
    for (i = 0; edits > 0 && i < nr; i++) { /* the bounds of what the edited records have now (culling) */
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        int any = 0;

        for (k = 0; k < ps->n; k++) {
            int q;

            if (ps->p[k].rec != i) {
                continue;
            }
            for (q = 0; q < ps->p[k].count; q++) {
                const float *v = (const float *)(m + ps->off[ps->p[k].first + q]);

                for (j = 0; j < 3; j++) {
                    lo[j] = v[j] < lo[j] ? v[j] : lo[j];
                    hi[j] = v[j] > hi[j] ? v[j] : hi[j];
                }
                any = 1;
            }
        }
        if (any) {
            float *b = (float *)(m + r[i].off + 0x40);

            for (j = 0; j < 3; j++) {
                b[j] = (lo[j] + hi[j]) * 0.5f;
                b[4 + j] = (hi[j] - lo[j]) * 0.5f + 0.05f;
            }
            b[7] = sqrtf(b[4] * b[4] + b[5] * b[5] + b[6] * b[6]);
        }
    }
}

/* The build's strips (wp_lastBat) of the layers' pieces the edits hide, marked in wp_drop for a second wp_compose
   (wp_dropOn): an outfit's pieces taken off, the rest of it kept. m: the model the first wp_compose made. Their count. */
static uint32_t wp_keepSlots;
static int wp_drop_hidden(uint8_t *m, uint32_t mn, const int *edit, int edits) {
    static WpPieces ps;
    int i, e, k, drops = 0, any = 0;

    for (e = 0; e < edits; e++) {
        any |= edit[e * WP_EDIT_WORDS] != WP_PATCH && edit[e * WP_EDIT_WORDS] != WP_SHADE && edit[e * WP_EDIT_WORDS] != WP_DUP && edit[e * WP_EDIT_WORDS + 2] != 0;
    }
    memset(wp_drop, 0, sizeof(wp_drop));
    if (!any || wp_pieces(m, mn, &ps, wp_lastFrom, wp_lastFroms) <= 0) {
        wp_pieces_free(&ps);
        return 0;
    }
    { /* (the layers' slots with pieces marked "only this one": the rest of them taken off) */
        uint32_t keepSlots = 0;

        for (e = 0; e < edits; e++) {
            const int *x = edit + e * WP_EDIT_WORDS;

            for (i = 0; x[0] >= 0 && x[2] == 2 && i < ps.n; i++) {
                if (ps.p[i].layer && ps.p[i].node == x[0] && (x[1] < 0 || ps.p[i].comp == x[1]) && ps.p[i].slot >= 0) {
                    keepSlots |= 1u << ps.p[i].slot;
                }
            }
        }
        wp_keepSlots = keepSlots;
    }
    for (i = 0; i < ps.n; i++) {
        const WpPiece *p = &ps.p[i];
        int hide = 0, keep = 0;

        for (e = 0; p->layer && e < edits; e++) {
            const int *x = edit + e * WP_EDIT_WORDS;
            int match;

            if (x[0] == WP_PATCH || x[0] == WP_SHADE || x[0] == WP_DUP || x[2] == 0) {
                continue;
            }
            match = x[0] < 0 ? -1 - x[0] >= WP_LAYER && wp_whole_has(p, -1 - x[0]) : p->node == x[0] && (x[1] < 0 || p->comp == x[1]);
            if (match && x[2] == 2 && x[0] >= 0) {
                keep = 1;
            } else if (match && x[2] == 1) {
                hide = 1;
            }
        }
        if (p->layer && !keep && p->slot >= 0 && ((wp_keepSlots >> p->slot) & 1)) {
            hide = 1;
        }
        hide = keep ? 0 : hide;
        for (k = 0; hide && k < p->count; k++) {
            uint32_t at = ps.off[p->first + k];
            int lo = 0, hi = wp_lastFroms - 1, j;

            while (lo < hi) { /* (the strip it is in: the last whose first vertex is at or before it) */
                int mid = (lo + hi + 1) / 2;

                if (wp_lastBatOff[mid] <= at) {
                    lo = mid;
                } else {
                    hi = mid - 1;
                }
            }
            j = wp_lastBat[lo];
            if (j >= 0 && j < WP_MAX_BATCHES && !wp_drop[j]) {
                wp_drop[j] = 1;
                drops++;
            }
        }
    }
    wp_pieces_free(&ps);
    return drops;
}

/* ---- patching: the holes edits and swaps leave, closed with triangles of their own ----
 * An open edge of a piece (an edge only one triangle has) that nothing lies over any more (a strand moved off the
 * skull, another fighter's head on the body) is bridged to the nearest open edge of another piece within `reach`: two
 * triangles an edge, in the piece's own picture, the far side taking the near side's texture place (so the edge's
 * colour runs across) and following the near side's node (its points on the node above follow that one). An open loop
 * that something now hidden or moved used to lie over is closed by a fan to its middle. The new triangles go in strips
 * of their own at the end of the near side's record (30 points a strip at most, as the game's). */
#define WP_OVER 0.05f
static int wp_patchWhy[9]; /* (the offline check: open edges left: covered, nothing near, two lumps, not a gap of ours, the other side's) */
#define WP_PATCH_TRIS 6000

typedef struct WpPatchCtx {
    const uint8_t *base;
    int rec, nb, ni;
    uint32_t *hdr, *off; /* per strip: its header words' offset; per point: its offset */
    int *bRec, *bFirst, *bCount, *iRec, *iBat;
    uint8_t *iDup;       /* per point: 1 a copied piece's (WP_DUP), or the face's: not patched, nor patched onto */
    uint32_t faceKey;    /* the face texture's key: its strips are left as they are */
    int onFace;          /* 1: the record being read is the face's (node 48) */
    uint32_t dupLo[16], dupHi[16];
    int dups;
} WpPatchCtx;

typedef struct WpPatchTri {
    int bat, rec, done; /* the strip whose picture it takes, the record it goes in, written */
    float v[3][12];
} WpPatchTri;

/* The face texture's key (as wp_tex_key gives a strip's), 0 none: model m's, its texture file t. */
static uint32_t wp_face_key(const uint8_t *m, const uint8_t *t, uint32_t tn) {
    static WpTex e[WP_MAX_TEX];
    int n = t != NULL ? wp_textures(t, tn, e, WP_MAX_TEX) : 0;

    return m[0x4C] < n ? e[m[0x4C]].tbpOfs | e[m[0x4C]].cbpOfs << 16 : 0;
}

static float wp_area2(const float *a, const float *b, const float *c) { /* (twice the triangle's area, squared) */
    float e[3], f[3], n[3];
    int i;

    for (i = 0; i < 3; i++) {
        e[i] = b[i] - a[i];
        f[i] = c[i] - a[i];
    }
    n[0] = e[1] * f[2] - e[2] * f[1], n[1] = e[2] * f[0] - e[0] * f[2], n[2] = e[0] * f[1] - e[1] * f[0];
    return n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
}

/* Where the line o + t d meets triangle t9 (its corners' x y z): t (1e9 none), the point's place on it in *u, *v. */
static float wp_line_uv(const float *o, const float *d, const float *t9, float *u, float *v) {
    float e1[3], e2[3], p[3], q[3], s2[3], det;
    int i;

    for (i = 0; i < 3; i++) {
        e1[i] = t9[3 + i] - t9[i];
        e2[i] = t9[6 + i] - t9[i];
        s2[i] = o[i] - t9[i];
    }
    p[0] = d[1] * e2[2] - d[2] * e2[1], p[1] = d[2] * e2[0] - d[0] * e2[2], p[2] = d[0] * e2[1] - d[1] * e2[0];
    det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (det > -1e-9f && det < 1e-9f) {
        return 1e9f;
    }
    *u = (s2[0] * p[0] + s2[1] * p[1] + s2[2] * p[2]) / det;
    q[0] = s2[1] * e1[2] - s2[2] * e1[1], q[1] = s2[2] * e1[0] - s2[0] * e1[2], q[2] = s2[0] * e1[1] - s2[1] * e1[0];
    *v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
    if (*u < -0.05f || *v < -0.05f || *u + *v > 1.1f) {
        return 1e9f;
    }
    return (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
}

static void wp_patch_batch(void *ctx, uint32_t *h, float *v, int count) {
    WpPatchCtx *c = (WpPatchCtx *)ctx;
    int i;

    int dup = 0;

    c->hdr[c->nb] = (uint32_t)((const uint8_t *)h - c->base);
    for (i = 0; i < c->dups; i++) {
        dup |= c->hdr[c->nb] >= c->dupLo[i] && c->hdr[c->nb] < c->dupHi[i];
    }
    if (c->onFace || (c->faceKey != 0 && wp_tex_key(h) == c->faceKey)) { /* (the face: never patched, nor patched onto) */
        dup = 1;
    }
    c->bRec[c->nb] = c->rec;
    c->bFirst[c->nb] = c->ni;
    c->bCount[c->nb] = count;
    for (i = 0; i < count; i++) {
        c->off[c->ni] = (uint32_t)((const uint8_t *)(v + i * 12) - c->base);
        c->iRec[c->ni] = c->rec;
        c->iBat[c->ni] = c->nb;
        if (c->iDup != NULL) {
            c->iDup[c->ni] = (uint8_t)dup;
        }
        c->ni++;
    }
    c->nb++;
}

static const uint8_t *wp_sortBase;
static const uint32_t *wp_sortOff;

static int wp_by_x(const void *a, const void *b) {
    float x = *(const float *)(wp_sortBase + wp_sortOff[*(const int *)a]), y = *(const float *)(wp_sortBase + wp_sortOff[*(const int *)b]);

    return x < y ? -1 : x > y;
}

typedef struct WpEdge {
    int lo, hi, a, b, bat;
} WpEdge;

static int wp_edge_cmp(const void *a, const void *b) {
    const WpEdge *x = (const WpEdge *)a, *y = (const WpEdge *)b;

    return x->lo != y->lo ? (x->lo < y->lo ? -1 : 1) : x->hi != y->hi ? (x->hi < y->hi ? -1 : 1) : 0;
}

static float wp_d2(const float *a, const float *b) {
    return (a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]);
}

/* Whether record `up` is above record r (its parent, or theirs). */
static int wp_above(const WpRec *rec, int r, int up) {
    for (r = rec[r].parent; r >= 0; r = rec[r].parent) {
        if (r == up) {
            return 1;
        }
    }
    return 0;
}

/* Bridge triangles from open edge gen[0] gen[1] (points of strip `bat`) to points farP[0] farP[1] (one when both
   are one point): a b b' and a b' a', or a b a'. Each point is {its own place and normal, the near point whose texture
   place and node it takes}; the far side follows the near side's node, or the node above when it is on that one. */
static void wp_patch_bridge(WpPatchTri *pt, int *npt, const uint8_t *m, const WpPatchCtx *c, const WpRec *r, int bat, const int *gen, const int *farP, int one) {
    int rec = c->bRec[bat], s, v;

    for (s = 0; s < (one ? 1 : 2) && *npt < WP_PATCH_TRIS; s++) {
        WpPatchTri *t = &pt[(*npt)++];
        int pick[3][2];

        pick[0][0] = gen[0], pick[0][1] = gen[0];
        pick[1][0] = s ? farP[1] : gen[1], pick[1][1] = gen[1];
        pick[2][0] = s ? farP[0] : farP[1], pick[2][1] = s ? gen[0] : gen[1];
        if (one) {
            pick[2][0] = farP[0], pick[2][1] = gen[0];
        }
        t->bat = bat;
        t->rec = rec;
        t->done = 0;
        for (v = 0; v < 3; v++) {
            const float *own = (const float *)(m + c->off[pick[v][0]]), *nearV = (const float *)(m + c->off[pick[v][1]]);

            memcpy(t->v[v], nearV, 48);
            memcpy(t->v[v], own, 12);
            memcpy(t->v[v] + 4, own + 4, 12);
            if (pick[v][0] != pick[v][1]) {
                int ro = c->iRec[pick[v][0]];

                t->v[v][3] = ro == rec ? own[3] : ro == r[rec].parent ? 0.0f : nearV[3];
            }
        }
    }
}

/* Model m (after the edits; `orig` the same before them, NULL none) with its holes patched, into *om (malloc'ed, *omn
   bytes): the triangles added, 0 none (*om NULL), -1 out of memory. `from` (NULL none): each strip's origin (wp_lastFrom):
   a gap is only bridged where its sides come from different fighters or have moved apart (a fighter's own gaps, as its
   chest's halves meeting down the middle, are as it was made). faceKey: the face texture's (wp_tex_key), capped only where hair lay right on it. */
/* ---- the morph: before any patch is made, a hole's open sides are stretched onto each other ----
 * First what touched before the edits and was pulled apart goes back together; then each open point that has a partner (what it touched before the edits, else the nearest open point of another lump
 * when the gap is between two fighters' parts or one the edits opened or widened) is moved onto it; when both sides
 * would move onto each other, only the smaller lump moves (a strand onto the skull, not the skull onto the strand).
 * The move fades into the rest of its lump (a few times the gap's length), so the piece bends to meet the other side
 * instead of tearing. A part hanging from the other (the neck from the collar) is the one that moves, and its moved
 * points follow the other's bone as much as they moved (vertex weight towards the parent's pivot), so the join holds
 * when the fighter moves, not only standing still. What is still open afterwards is bridged as before. */
static int wp_morphOn;
static int wp_morphSkip; /* (the offline check: 1 no surface step, 2 no long reach for moved sides) */
#define WP_MORPH_MAX 0.5f      /* how far a side is pulled onto the other */
#define WP_MORPH_MOVED 1.0f    /* and a side the edits moved away (the hair raised): back across as far as this */

static int wp_patch_morph(uint8_t *m, const WpPatchCtx *c, const WpRec *r, int nr, const uint8_t *orig, const uint8_t *from, int froms, float reach,
                          const int *id, int ids, float (*pos)[3], const int *up, const int *used, const int *bnd, const int *match, const int *opair,
                          const uint8_t *opened, const uint8_t *cov) {
    float (*d)[4] = calloc((size_t)ids, 16), (*fin)[3] = calloc((size_t)ids, 12), *fw = calloc((size_t)ids, 4);
    float lim = reach < WP_MORPH_MAX ? reach : WP_MORPH_MAX;
    int *lump = calloc((size_t)ids, 4), *mv = malloc((size_t)ids * 4), *onto = malloc((size_t)ids * 4), *tgt = malloc((size_t)ids * 4);
    int *order = malloc((size_t)c->ni * 4), nmv = 0, welds = 0, surf = 0, i, k, j, q, pass;

    if (d == NULL || fin == NULL || fw == NULL || lump == NULL || mv == NULL || onto == NULL || tgt == NULL || order == NULL) {
        nmv = 0;
        goto out;
    }
    for (i = 0; i < ids; i++) {
        onto[i] = -1; /* (the record of what it was moved onto) */
        tgt[i] = -1;  /* (the point it was moved onto: it ends where that one ends) */
        lump[up[i]] += used[i] ? 1 : 0;
    }
    /* 1. what touched before the edits, together again: the points made one place in the model before the edits (the
       chest's halves down the middle, a strand's root on the skull) that the edits pulled apart go back to one place,
       nearest the one that moved least (a moved strand comes back to the skull that stayed; two halves both thickened
       meet in the middle). Points of hidden pieces (on no triangle any more) take no part. */
    for (k = 0; orig != NULL && k < c->ni; k++) {
        order[k] = k;
    }
    if (orig != NULL) {
        wp_sortBase = orig;
        wp_sortOff = c->off;
        qsort(order, (size_t)c->ni, 4, wp_by_x);
    }
    for (k = 0; orig != NULL && k < c->ni; k = q) {
        const float *a = (const float *)(orig + c->off[order[k]]);
        float sum[3] = {0.0f, 0.0f, 0.0f}, wsum = 0.0f, spread = 0.0f, tp[3];
        int grp[64], ng = 0, g;

        for (q = k; q < c->ni && *(const float *)(orig + c->off[order[q]]) - a[0] < WP_WELD; q++) {
            const float *b = (const float *)(orig + c->off[order[q]]);
            int x = id[order[q]];

            if (fabsf(b[1] - a[1]) >= WP_WELD || fabsf(b[2] - a[2]) >= WP_WELD || !used[x]) {
                continue;
            }
            for (g = 0; g < ng && grp[g] != x; g++) {
            }
            if (g == ng && ng < 64) {
                float moved = sqrtf(wp_d2(pos[x], b)), w = 1.0f / (moved + 0.01f);

                grp[ng++] = x;
                for (j = 0; j < 3; j++) {
                    sum[j] += pos[x][j] * w;
                }
                wsum += w;
            }
        }
        if (q == k) {
            q = k + 1;
        }
        if (ng < 2) {
            continue;
        }
        for (j = 0; j < 3; j++) {
            tp[j] = sum[j] / wsum;
        }
        for (g = 0; g < ng; g++) {
            float dd = wp_d2(pos[grp[g]], tp);

            spread = dd > spread ? dd : spread;
        }
        if (spread < WP_WELD * WP_WELD || spread > 1.0f) {
            continue; /* (together still; or far apart: moved away on purpose, left to the patch) */
        }
        for (g = 0; g < ng; g++) {
            int x = grp[g];
            float dist = sqrtf(wp_d2(pos[x], tp));

            if (dist < 1e-5f || d[x][3] > 0.0f) {
                continue;
            }
            for (j = 0; j < 3; j++) {
                d[x][j] = tp[j] - pos[x][j];
            }
            d[x][3] = dist * 3.0f < 0.08f ? 0.08f : dist * 3.0f > 0.5f ? 0.5f : dist * 3.0f;
            mv[nmv++] = x;
            welds++;
        }
    }
    /* 2. the open points left: onto their partner (what they touched before, else the nearest open point of another
       lump across two fighters' parts or a gap the edits opened or widened) */
    for (i = 0; i < ids; i++) {
        const float *to;
        float dist;
        int t, back, mine, theirs;

        if (bnd[i] < 0 || cov[i] || d[i][3] > 0.0f) {
            continue;
        }
        if (opair[i] >= 0) {
            t = id[opair[i]];
            to = (const float *)(m + c->off[opair[i]]);
            back = bnd[t] >= 0 && opair[t] >= 0 ? up[id[opair[t]]] : -1;
        } else if (match[i] >= 0) {
            int ok;

            k = match[i];
            ok = opened[i];
            if (!ok && from != NULL) { /* (two fighters' parts meeting: a swap's seam) */
                int ba = c->iBat[bnd[i]], bb = c->iBat[bnd[k]];

                ok = ba < froms && bb < froms && from[ba] != from[bb];
            }
            if (!ok && orig != NULL) { /* (pulled apart by the edits) */
                ok = sqrtf(wp_d2(pos[i], pos[k])) > sqrtf(wp_d2((const float *)(orig + c->off[bnd[i]]), (const float *)(orig + c->off[bnd[k]]))) + 0.01f;
            }
            if (!ok) {
                continue; /* (a fighter's own edge near another: as the disc has it) */
            }
            t = k;
            to = pos[k];
            back = match[k] >= 0 ? up[match[k]] : -1;
        } else {
            continue;
        }
        dist = sqrtf(wp_d2(pos[i], to));
        if (dist < 1e-5f || dist > (orig != NULL && wp_d2(pos[i], (const float *)(orig + c->off[bnd[i]])) > 0.1f * 0.1f && !(wp_morphSkip & 2) ? (reach < WP_MORPH_MOVED ? reach : WP_MORPH_MOVED) : lim)) {
            continue;
        }
        mine = c->iRec[bnd[i]];
        theirs = opair[i] >= 0 ? c->iRec[opair[i]] : c->iRec[bnd[t]];
        if (back == up[i]) { /* (both onto each other: one moves; the one the edits moved away, else the one hanging from
                                 the other, else the smaller) */
            int hangs = mine >= 0 && mine < nr && r[mine].parent == theirs, held = theirs >= 0 && theirs < nr && r[theirs].parent == mine;
            float me = orig != NULL ? wp_d2(pos[i], (const float *)(orig + c->off[bnd[i]])) : 0.0f;
            float it = orig != NULL ? wp_d2(pos[t], (const float *)(orig + c->off[bnd[t]])) : 0.0f;

            if (me > 0.15f * 0.15f && it <= 0.02f * 0.02f) {
                hangs = 1, held = 0; /* (I was moved away, it was not: I go back; grown or thinned, the other follows) */
            } else if (it > 0.15f * 0.15f && me <= 0.02f * 0.02f) {
                held = 1; /* (it was moved: it comes back) */
            }
            if (held || (!hangs && (lump[up[i]] > lump[up[t]] || (lump[up[i]] == lump[up[t]] && up[i] > up[t])))) {
                continue;
            }
        }
        onto[i] = theirs;
        tgt[i] = t;
        if (wp_patchLog >= 2) {
            printf("mv %.2f %.2f %.2f -> %.2f %.2f %.2f rec %d node %d onto rec %d node %d pair %d lump %d/%d%c", pos[i][0], pos[i][1], pos[i][2], to[0], to[1], to[2],
                   mine, r[mine].node, theirs, r[theirs].node, opair[i] >= 0, lump[up[i]], lump[up[t]], 10);
        }
        for (j = 0; j < 3; j++) {
            d[i][j] = to[j] - pos[i][j];
        }
        d[i][3] = dist * 3.0f < 0.08f ? 0.08f : dist * 3.0f > 0.5f ? 0.5f : dist * 3.0f;
        mv[nmv++] = i;
    }
    /* 2b. open points still apart: pulled onto the nearest surface of what they hang from (a record above theirs: the
       hair's rim onto the skull, the neck's onto the chest, a glove's cuff onto the forearm), as a modeller drags an
       edge loop over a hole, up to the patch's reach; standing just off it */
    for (i = 0; i < ids; i++) {
        int mine, b3, best = -1, bestLump = -1, moved;
        float bd = (reach < 1.0f ? reach : 1.0f) * (reach < 1.0f ? reach : 1.0f), at[3], bat3[3];

        if (bnd[i] < 0 || cov[i] || d[i][3] > 0.0f || !used[i] || (opair[i] >= 0 && d[id[opair[i]]][3] > 0.0f) || (wp_morphSkip & 1)) {
            continue; /* (one whose old partner is coming to it waits for it) */
        }
        moved = orig != NULL && wp_d2(pos[i], (const float *)(orig + c->off[bnd[i]])) >= 0.1f * 0.1f;
        mine = c->iRec[bnd[i]];
        for (b3 = 0; b3 < c->nb; b3++) {
            int rr = c->bRec[b3], a2 = mine >= 0 && mine < nr ? r[mine].parent : -1, lv, t3;

            for (lv = 0; lv < 8 && a2 >= 0 && a2 != rr && r[a2].node >= 0x47; lv++) {
                a2 = r[a2].parent; /* (up a chain to the skeleton's node it hangs from: that one only, not further) */
            }
            if (a2 != rr || rr < 0 || up[id[c->bFirst[b3]]] == up[i]) {
                continue; /* (not above it, or its own lump) */
            }
            if (!moved && !(wp_morphSkip & 4) && (from == NULL || c->iBat[bnd[i]] >= froms || b3 >= froms || from[c->iBat[bnd[i]]] == from[b3] ||
                                                  !(wp_head_node(r[mine].node) && wp_head_node(r[rr].node)))) {
                continue; /* (left alone by the edits: a fighter's own, as the disc has it; two
                                                    fighters' parts, a swap's seam, the morph's above, but in the head) */
            }
            for (t3 = 0; t3 + 2 < c->bCount[b3]; t3++) {
                float tri[9], dd;
                int e3;

                for (e3 = 0; e3 < 3; e3++) {
                    memcpy(tri + e3 * 3, m + c->off[c->bFirst[b3] + t3 + e3], 12);
                }
                if (wp_area2(tri, tri + 3, tri + 6) < 1e-12f) {
                    continue;
                }
                if (t3 == 0 && wp_d2(pos[i], tri) > 9.0f) {
                    break; /* (a strip far away) */
                }
                dd = wp_tri_nearest(pos[i], tri, at);
                if (dd < bd) {
                    bd = dd;
                    best = rr;
                    bestLump = up[id[c->bFirst[b3]]];
                    memcpy(bat3, at, 12);
                }
            }
        }
        if (best >= 0 && bd > 0.01f * 0.01f) { /* (only where that surface has an opening close by to cover: the skull's
                                                     under raised hair, not the closed nape the hair hangs over) */
            int nearby = 0;

            for (k = 0; k < ids && !nearby; k++) {
                nearby = bnd[k] >= 0 && up[k] == bestLump && wp_d2(pos[k], bat3) < 0.15f * 0.15f;
            }
            if (!nearby) {
                best = -1;
            }
        }
        if (best >= 0 && bd > 0.01f * 0.01f) {
            float dist = sqrtf(bd);

            for (j = 0; j < 3; j++) {
                d[i][j] = bat3[j] + (pos[i][j] - bat3[j]) / dist * 0.01f - pos[i][j];
            }
            d[i][3] = dist * 3.0f < 0.08f ? 0.08f : dist * 3.0f > 0.5f ? 0.5f : dist * 3.0f;
            onto[i] = best;
            mv[nmv++] = i;
            surf++;
            if (wp_patchLog >= 2) { printf("surf %.2f %.2f %.2f rec %d node %d -> rec %d node %d moved %d dist %.2f%c", pos[i][0], pos[i][1], pos[i][2], mine, r[mine].node, best, r[best].node, moved, dist, 10); }
        }
    }
    /* 3. the rest of each moving lump: the moves near it, faded with distance; open points already on another lump stay */
    for (i = 0; nmv > 0 && i < ids; i++) {
        float sum[3] = {0.0f, 0.0f, 0.0f}, wsum = 0.0f, wmax = 0.0f;

        if (!used[i] || (bnd[i] >= 0 && cov[i] && d[i][3] <= 0.0f)) {
            continue;
        }
        if (d[i][3] > 0.0f) {
            memcpy(fin[i], d[i], 12);
            fw[i] = 1.0f;
            continue;
        }
        for (k = 0; k < nmv; k++) {
            int p2 = mv[k];
            float w;

            if (up[p2] != up[i] || (w = 1.0f - sqrtf(wp_d2(pos[i], pos[p2])) / d[p2][3]) <= 0.0f) {
                continue;
            }
            w = w * w * (3.0f - 2.0f * w); /* (smooth) */
            for (j = 0; j < 3; j++) {
                sum[j] += d[p2][j] * w;
            }
            wsum += w;
            if (w > wmax) {
                wmax = w;
                onto[i] = onto[p2];
            }
        }
        for (j = 0; wsum > 0.0f && j < 3; j++) {
            fin[i][j] = sum[j] / wsum * wmax;
        }
        fw[i] = wmax;
    }
    /* 4. each point moved onto another ends where that one ends (the other side faded or moved too: no gap left
       between them; chains followed a few steps) */
    for (pass = 0; pass < 4; pass++) {
        for (i = 0; i < ids; i++) {
            int t = tgt[i];

            if (t < 0 || t == i) {
                continue;
            }
            for (j = 0; j < 3; j++) {
                fin[i][j] = pos[t][j] + fin[t][j] - pos[i][j];
            }
        }
    }
    for (k = 0; nmv > 0 && k < c->ni; k++) {
        float *v = (float *)(m + c->off[k]);
        int own = c->iRec[k], o = onto[id[k]];

        for (j = 0; j < 3; j++) {
            v[j] += fin[id[k]][j];
        }
        if (o >= 0 && own >= 0 && own < nr && r[own].parent == o && fw[id[k]] > 0.0f) {
            v[3] *= 1.0f - fw[id[k]]; /* (onto its parent's surface: follows the parent's bone as much as it moved) */
        }
    }
    if (wp_patchLog) {
        printf("morph: %d points moved (%d put back together, %d onto what they hang from)%c", nmv, welds, surf, 10);
    }
out:
    free(d);
    free(fin);
    free(fw);
    free(lump);
    free(mv);
    free(onto);
    free(tgt);
    free(order);
    return nmv;
}

/* An open edge left for the loops: each end's two neighbours along the opening. */
static void wp_adj_add(int *adj, int ua, int ub) {
    int s;

    for (s = 0; s < 2; s++) {
        int x = s ? ub : ua, y = s ? ua : ub;

        if (adj[x * 2] < 0) {
            adj[x * 2] = y;
        } else if (adj[x * 2 + 1] < 0 && adj[x * 2] != y) {
            adj[x * 2 + 1] = y;
        }
    }
}

static int wp_patch(const uint8_t *m, uint32_t mn, const uint8_t *orig, const uint8_t *from, int froms, uint32_t faceKey, float reach, uint8_t **om,
                    uint32_t *omn) {
    static WpRec r[WP_MAX_RECS * 2];
    static int pairA[256], pairB[256], pairN[256];
    WpPatchCtx c;
    WpEdge *ed = NULL;
    WpPatchTri *pt = NULL;
    int nr = wp_records(m, mn, r, WP_MAX_RECS * 2), maxI = (int)(mn / 48) + 16, maxB = (int)(mn / 92) + 16;
    int *id = NULL, *order = NULL, *up = NULL, *used = NULL, *bnd = NULL, *match = NULL, *adj = NULL, *seen = NULL, *loop = NULL, *opair = NULL;
    float (*pos)[3] = NULL;
    uint8_t *cov = NULL, *opened = NULL, *out;
    int ids = 0, ned = 0, npt = 0, npair = 0, i, k, j, q, result = -1;
    uint32_t grow = 0, o;

    *om = NULL;
    *omn = 0;
    memset(&c, 0, sizeof(c));
    if (nr == 0) {
        return 0;
    }
    c.base = m;
    c.hdr = malloc((size_t)maxB * 4);
    c.bRec = malloc((size_t)maxB * 4);
    c.bFirst = malloc((size_t)maxB * 4);
    c.bCount = malloc((size_t)maxB * 4);
    c.off = malloc((size_t)maxI * 4);
    c.iRec = malloc((size_t)maxI * 4);
    c.iBat = malloc((size_t)maxI * 4);
    c.iDup = calloc((size_t)maxI, 1);
    id = malloc((size_t)maxI * 4);
    order = malloc((size_t)maxI * 4);
    pos = malloc((size_t)maxI * 12);
    up = malloc((size_t)maxI * 4);
    used = calloc((size_t)maxI, 4);
    bnd = malloc((size_t)maxI * 4);
    match = malloc((size_t)maxI * 4);
    opair = malloc((size_t)maxI * 4);
    adj = malloc((size_t)maxI * 8);
    seen = calloc((size_t)maxI, 4);
    loop = malloc((size_t)maxI * 4);
    cov = calloc((size_t)maxI, 1);
    opened = calloc((size_t)maxI, 1);
    ed = malloc((size_t)maxI * 3 * sizeof(WpEdge));
    pt = malloc((size_t)WP_PATCH_TRIS * sizeof(WpPatchTri));
    if (c.hdr == NULL || c.bRec == NULL || c.bFirst == NULL || c.bCount == NULL || c.off == NULL || c.iRec == NULL || c.iBat == NULL || c.iDup == NULL || id == NULL || order == NULL ||
        pos == NULL || up == NULL || used == NULL || bnd == NULL || match == NULL || opair == NULL || adj == NULL || seen == NULL || loop == NULL || cov == NULL || opened == NULL ||
        ed == NULL || pt == NULL) {
        goto done;
    }
    for (i = 0; i < nr; i++) { /* every strip and point */
        static WpPieceCtx dc;

        c.rec = i;
        c.onFace = r[i].node == 48;
        c.faceKey = faceKey;
        if (r[i].size > 0x40) {
            wp_dup_tags(m, &r[i], &dc); /* (its copied pieces' strips) */
            memcpy(c.dupLo, dc.dupLo, sizeof(c.dupLo));
            memcpy(c.dupHi, dc.dupHi, sizeof(c.dupHi));
            c.dups = dc.dups;
            wp_batches((uint8_t *)m, &r[i], wp_patch_batch, &c);
        }
    }
    for (i = 0; i < c.ni; i++) { /* the points, welded by place */
        order[i] = i;
    }
    wp_sortBase = m;
    wp_sortOff = c.off;
    qsort(order, (size_t)c.ni, 4, wp_by_x);
    for (k = 0; k < c.ni; k++) {
        const float *p = (const float *)(m + c.off[order[k]]);

        for (j = k - 1; j >= 0; j--) {
            const float *s = (const float *)(m + c.off[order[j]]);

            if (p[0] - s[0] >= WP_WELD || c.iDup[order[k]]) { /* (a copy's points: each its own, joined to nothing) */
                j = -1;
                break;
            }
            if (!c.iDup[order[j]] && fabsf(p[1] - s[1]) < WP_WELD && fabsf(p[2] - s[2]) < WP_WELD) {
                break;
            }
        }
        if (j >= 0) {
            id[order[k]] = id[order[j]];
        } else {
            memcpy(pos[ids], p, 12);
            up[ids] = ids;
            bnd[ids] = -1;
            id[order[k]] = ids++;
        }
    }
    for (i = 0; i < c.nb; i++) { /* the triangles: the lumps they join, and their edges (a copy's: none) */
        for (k = 0; k + 2 < c.bCount[i] && !c.iDup[c.bFirst[i]]; k++) {
            int t[3], e;
            float a[3], b[3], n[3];

            for (e = 0; e < 3; e++) {
                t[e] = c.bFirst[i] + k + e;
            }
            if (id[t[0]] == id[t[1]] || id[t[1]] == id[t[2]] || id[t[0]] == id[t[2]]) {
                continue;
            }
            for (e = 0; e < 3; e++) {
                a[e] = pos[id[t[1]]][e] - pos[id[t[0]]][e];
                b[e] = pos[id[t[2]]][e] - pos[id[t[0]]][e];
            }
            n[0] = a[1] * b[2] - a[2] * b[1], n[1] = a[2] * b[0] - a[0] * b[2], n[2] = a[0] * b[1] - a[1] * b[0];
            if (n[0] * n[0] + n[1] * n[1] + n[2] * n[2] < 1e-14f) {
                continue; /* (flat: a hidden piece's, all on one point) */
            }
            for (e = 0; e < 3; e++) {
                int x = t[e], y = t[(e + 1) % 3];

                used[id[x]] = 1;
                up[wp_find_root(up, id[y])] = wp_find_root(up, id[x]);
                ed[ned].lo = id[x] < id[y] ? id[x] : id[y];
                ed[ned].hi = id[x] < id[y] ? id[y] : id[x];
                ed[ned].a = x;
                ed[ned].b = y;
                ed[ned].bat = i;
                ned++;
            }
        }
    }
    for (i = 0; i < ids; i++) {
        up[i] = wp_find_root(up, i); /* (each point's lump) */
    }
    qsort(ed, (size_t)ned, sizeof(WpEdge), wp_edge_cmp);
    for (i = k = 0; i < ned; i = j) { /* the open edges: those of one triangle only */
        for (j = i + 1; j < ned && ed[j].lo == ed[i].lo && ed[j].hi == ed[i].hi; j++) {
        }
        if (j == i + 1) {
            ed[k++] = ed[i];
            bnd[id[ed[i].a]] = ed[i].a;
            bnd[id[ed[i].b]] = ed[i].b;
        }
    }
    ned = k;
    for (i = 0; i < ids; i++) { /* an open point another lump lies on is closed already */
        match[i] = -1;
        adj[i * 2] = adj[i * 2 + 1] = -1;
        for (k = 0; bnd[i] >= 0 && k < ids && !cov[i]; k++) {
            cov[i] = used[k] && up[k] != up[i] && wp_d2(pos[i], pos[k]) < WP_OVER * WP_OVER;
        }
    }
    for (i = 0; i < ids; i++) { /* the original's pairing: what each open point touched before the edits, now apart from it
                                   (a strand's root and the skull it sat on: put back together, stretched across) */
        float best = 0.06f * 0.06f;
        const float *was;

        opair[i] = -1;
        if (orig == NULL || bnd[i] < 0 || cov[i]) {
            continue;
        }
        was = (const float *)(orig + c.off[bnd[i]]);
        for (k = 0; k < c.ni; k++) {
            float d;

            if (c.iDup[k] || up[id[k]] == up[i] || !used[id[k]] || (d = wp_d2(was, (const float *)(orig + c.off[k]))) >= best) {
                continue;
            }
            best = d;
            opair[i] = k;
        }
        if (opair[i] >= 0 && wp_d2(pos[i], (const float *)(m + c.off[opair[i]])) < 0.02f * 0.02f) {
            opair[i] = -1; /* (still together) */
        }
    }
    for (i = 0; orig != NULL && i < ids; i++) { /* an open point something lay on before the edits, now hidden or moved
                                                   away: a hole they opened (where a hidden strand's root was) */
        const float *was;

        if (bnd[i] < 0 || cov[i]) {
            continue;
        }
        was = (const float *)(orig + c.off[bnd[i]]);
        for (k = 0; k < c.ni && !opened[i]; k++) {
            const float *o2 = (const float *)(orig + c.off[k]);

            opened[i] = !c.iDup[k] && up[id[k]] != up[i] && wp_d2(was, o2) < 0.15f * 0.15f &&
                        (!used[id[k]] || wp_d2(o2, (const float *)(m + c.off[k])) > 0.02f * 0.02f);
        }
    }
    for (i = 0; i < ids; i++) { /* each open point: the nearest open point of another lump */
        float best = reach * reach;

        for (k = 0; bnd[i] >= 0 && !cov[i] && k < ids; k++) {
            float d;

            if (bnd[k] < 0 || cov[k] || up[k] == up[i] || (d = wp_d2(pos[i], pos[k])) >= best) {
                continue;
            }
            best = d;
            match[i] = k;
        }
        if (match[i] >= 0) { /* (how many each lump has onto each) */
            for (q = 0; q < npair && (pairA[q] != up[i] || pairB[q] != up[match[i]]); q++) {
            }
            if (q == npair && npair < 256) {
                pairA[q] = up[i];
                pairB[q] = up[match[i]];
                pairN[q] = 0;
                npair++;
            }
            if (q < npair) {
                pairN[q]++;
            }
        }
    }
    if (wp_morphOn) { /* the morph: the holes closed by the sides themselves, stretched onto each other */
        result = wp_patch_morph((uint8_t *)m, &c, r, nr, orig, from, froms, reach, id, ids, pos, up, used, bnd, match, opair, opened, cov);
        goto done;
    }
    for (i = 0; i < ned && npt + 2 <= WP_PATCH_TRIS; i++) { /* the bridges */
        const WpEdge *e = &ed[i];
        int ua = id[e->a], ub = id[e->b], ta = match[ua], tb = match[ub], rec = c.bRec[e->bat], recB, mine = 0, theirs = 0, s, gen[2], farP[2];

        if (cov[ua] || cov[ub]) {
            wp_patchWhy[0]++;
            continue;
        }
        if (opair[ua] >= 0 && opair[ub] >= 0) { /* what touched before the edits: joined again */
            int back = id[opair[ua]];

            recB = c.iRec[opair[ua]];
            /* (both sides open and paired with each other: one makes it, the one hanging from the other) */
            if (bnd[back] >= 0 && opair[back] >= 0 && up[id[opair[back]]] == up[ua] && !(recB != rec && wp_above(r, rec, recB)) &&
                (wp_above(r, recB, rec) || up[ua] < up[back])) {
                continue;
            }
            gen[0] = e->a, gen[1] = e->b;
            farP[0] = opair[ua], farP[1] = opair[ub];
            wp_patch_bridge(pt, &npt, m, &c, r, e->bat, gen, farP, id[farP[0]] == id[farP[1]]);
            wp_patchWhy[5]++;
            continue;
        }
        if (ta < 0 && tb < 0) { /* (left for the loops) */
            wp_patchWhy[1]++;
            wp_adj_add(adj, ua, ub);
            continue;
        }
        if (ta < 0 || tb < 0 || up[ta] != up[tb]) {
            wp_patchWhy[2]++;
            wp_adj_add(adj, ua, ub); /* (not bridged: a loop's edge still, if it closes one) */
            continue;
        }
        recB = c.iRec[bnd[ta]];
        if (orig != NULL) { /* gaps the edits opened (sides moved apart): always; any other only when short (a part
                               reaching far to another, many points onto one, or along a fighter's own edge: a sliver) */
            const float *pa = (const float *)(m + c.off[e->a]), *pb = (const float *)(m + c.off[e->b]);
            const float *qa = (const float *)(m + c.off[bnd[ta]]), *qb = (const float *)(m + c.off[bnd[tb]]);
            int ba = c.iBat[e->a], bb = c.iBat[bnd[ta]];
            int other = from != NULL && ba < froms && bb < froms && from[ba] != from[bb];
            float now = sqrtf(wp_d2(pa, qa)), was = sqrtf(wp_d2((const float *)(orig + c.off[e->a]), (const float *)(orig + c.off[bnd[ta]])));
            float ev[3], gv[3], el = 0.0f, gl = 0.0f, dot = 0.0f;

            for (s = 0; s < 3; s++) {
                ev[s] = pb[s] - pa[s];
                gv[s] = (qa[s] + qb[s] - pa[s] - pb[s]) * 0.5f;
                el += ev[s] * ev[s];
                gl += gv[s] * gv[s];
                dot += ev[s] * gv[s];
            }
            if (now < was + 0.02f && !opened[ua] && !opened[ub] && (now > 0.6f || (ta == tb && now > 3.0f * sqrtf(el)) || (!other && fabsf(dot) > 0.6f * sqrtf(el * gl)))) {
                wp_patchWhy[3]++;
                if (wp_patchLog) {
                    printf("skip rec %d node %d -> node %d other %d now %.2f was %.2f cos %.2f one %d len %.2f\n", rec, r[rec].node, r[recB].node, other, now, was,
                           el * gl > 0.0f ? fabsf(dot) / sqrtf(el * gl) : 0.0f, ta == tb, sqrtf(el));
                }
                wp_adj_add(adj, ua, ub);
                continue;
            }
        }
        for (q = 0; q < npair; q++) {
            mine += pairA[q] == up[ua] && pairB[q] == up[ta] ? pairN[q] : 0;
            theirs += pairA[q] == up[ta] && pairB[q] == up[ua] ? pairN[q] : 0;
        }
        /* one side makes the bridge: the one hanging from the other, else the one with more points along it */
        if (!(recB != rec && wp_above(r, rec, recB)) && (wp_above(r, recB, rec) || mine < theirs || (mine == theirs && up[ua] < up[ta]))) {
            wp_patchWhy[4]++;
            continue;
        }
        gen[0] = e->a, gen[1] = e->b;
        farP[0] = bnd[ta], farP[1] = bnd[tb];
        if (wp_patchLog) {
            const float *pa = (const float *)(m + c.off[e->a]), *qa = (const float *)(m + c.off[bnd[ta]]);

            printf("bridge rec %d node %d from %d (%.2f %.2f %.2f) -> rec %d node %d from %d (%.2f %.2f %.2f)\n", rec, r[rec].node,
                   from != NULL && c.iBat[e->a] < froms ? from[c.iBat[e->a]] : -1, pa[0], pa[1], pa[2], recB, r[recB].node,
                   from != NULL && c.iBat[bnd[ta]] < froms ? from[c.iBat[bnd[ta]]] : -1, qa[0], qa[1], qa[2]);
        }
        wp_patch_bridge(pt, &npt, m, &c, r, e->bat, gen, farP, ta == tb);
    }
    for (i = 0; orig != NULL && i < ids && npt < WP_PATCH_TRIS; i++) { /* the loops something used to lie over: fans */
        int n = 0, prev = -1, at = i, under = 0, s, bat = -1, rec;
        float mid[12], nl;

        if (seen[i] || adj[i * 2] < 0 || adj[i * 2 + 1] < 0) {
            continue;
        }
        do {
            int next = adj[at * 2] != prev ? adj[at * 2] : adj[at * 2 + 1];

            seen[at] = 1;
            loop[n++] = at;
            prev = at;
            at = next;
        } while (at >= 0 && at != i && !seen[at] && adj[at * 2 + 1] >= 0 && n < ids);
        if (wp_patchLog >= 2 && (at != i || n < 3)) {
            const float *p0 = (const float *)(m + c.off[bnd[i]]);

            printf("open run %d points, not closed, at %.2f %.2f %.2f node %d%c", n, p0[0], p0[1], p0[2], r[c.iRec[bnd[i]]].node, 10);
        }
        if (at != i || n < 3) {
            continue;
        }
        for (k = 0; k < n; k++) { /* (before the edits, another lump lay on it, or over it: in front of it along its normal,
                                     close; a skull's opening under the hair, not an eye's socket with the eye behind) */
            const float *was = (const float *)(orig + c.off[bnd[loop[k]]]);
            float dn[3], dl = sqrtf(was[4] * was[4] + was[5] * was[5] + was[6] * was[6]);
            int b3, hit = 0;

            for (j = 0; j < c.ni; j++) {
                if (up[id[j]] != up[loop[k]] && wp_d2(was, (const float *)(orig + c.off[j])) < WP_OVER * WP_OVER) {
                    hit = 1;
                    break;
                }
            }
            for (j = 0; j < 3 && dl > 1e-6f; j++) {
                dn[j] = was[4 + j] / dl;
            }
            for (b3 = 0; !hit && dl > 1e-6f && wp_head_node(r[c.iRec[bnd[loop[k]]]].node) && b3 < c.nb; b3++) {
                int t3;

                if (up[id[c.bFirst[b3]]] == up[loop[k]]) {
                    continue;
                }
                for (t3 = 0; t3 + 2 < c.bCount[b3] && !hit; t3++) {
                    float tri[9], w;
                    int e3;

                    for (e3 = 0; e3 < 3; e3++) {
                        memcpy(tri + e3 * 3, orig + c.off[c.bFirst[b3] + t3 + e3], 12);
                    }
                    if (wp_d2(was, tri) > 4.0f) {
                        break; /* (a strip far away) */
                    }
                    w = wp_line_t(was, dn, tri);
                    hit = w > 0.0f && w < 1.0f;
                }
            }
            under += hit;
        }
        if (wp_patchLog >= 2) {
            const float *p0 = (const float *)(m + c.off[bnd[loop[0]]]);

            printf("loop %d points, %d under, at %.2f %.2f %.2f rec %d node %d%c", n, under, p0[0], p0[1], p0[2], c.iRec[bnd[loop[0]]], r[c.iRec[bnd[loop[0]]]].node, 10);
        }
        if (under * 2 < n) {
            continue;
        }
        for (k = 0; k < c.nb && bat < 0; k++) { /* (the strip its first point is in) */
            bat = bnd[loop[0]] >= c.bFirst[k] && bnd[loop[0]] < c.bFirst[k] + c.bCount[k] ? k : -1;
        }
        if (bat < 0) {
            continue;
        }
        rec = c.bRec[bat];
        memset(mid, 0, sizeof(mid));
        for (k = 0; k < n; k++) {
            const float *v = (const float *)(m + c.off[bnd[loop[k]]]);

            for (j = 0; j < 12; j++) {
                mid[j] += v[j] / (float)n;
            }
        }
        nl = sqrtf(mid[4] * mid[4] + mid[5] * mid[5] + mid[6] * mid[6]);
        for (j = 4; j < 7 && nl > 1e-6f; j++) {
            mid[j] /= nl;
        }
        { /* (a little domed, out along the loop's normal: a skull's opening closed round, not flat) */
            float rad = 0.0f;

            for (k = 0; k < n; k++) {
                rad += sqrtf(wp_d2((const float *)(m + c.off[bnd[loop[k]]]), mid)) / (float)n;
            }
            for (j = 0; j < 3 && nl > 1e-6f; j++) {
                mid[j] += mid[4 + j] * rad * 0.35f;
            }
        }
        memcpy(mid + 7, (const float *)(m + c.off[bnd[loop[0]]]) + 7, 4);
        memcpy(mid + 10, (const float *)(m + c.off[bnd[loop[0]]]) + 10, 8);
        for (k = 0; k < n && npt < WP_PATCH_TRIS; k++) {
            WpPatchTri *t = &pt[npt++];

            t->bat = bat;
            t->rec = rec;
            t->done = 0;
            for (s = 0; s < 2; s++) {
                int p = bnd[loop[(k + s) % n]];

                memcpy(t->v[s], m + c.off[p], 48);
                if (c.iRec[p] != rec) {
                    t->v[s][3] = c.iRec[p] == r[rec].parent ? 0.0f : mid[3];
                }
            }
            memcpy(t->v[2], mid, 48);
        }
    }
    if (orig != NULL) { /* hair caps: the skin a hidden strand of hair lay on, covered in that hair's picture */
        static uint32_t hairKey[64];
        static float ht[2000][15]; /* the hidden hair's triangles as they were: corners' x y z, then their s t */
        static int hb[2000];
        int hairs = wp_hair_textures(m, r, nr, hairKey, 64), nh = 0, b2;

        for (b2 = 0; b2 < c.nb; b2++) {
            int rr = c.bRec[b2];
            int hair = r[rr].node >= 0x47 ? wp_chain_slot_of(r, rr) == WP_HAIR
                                          : wp_head_node(r[rr].node) && wp_is_hair(hairKey, hairs, wp_tex_key((const uint32_t *)(m + c.hdr[b2])));

            for (k = 0; hair && k + 2 < c.bCount[b2] && nh < 2000; k++) {
                const float *n0 = (const float *)(m + c.off[c.bFirst[b2] + k]), *n1 = n0 + 12, *n2 = n0 + 24;
                const float *o0 = (const float *)(orig + c.off[c.bFirst[b2] + k]), *o1 = o0 + 12, *o2 = o0 + 24;

                if (wp_area2(n0, n1, n2) > 1e-14f || wp_area2(o0, o1, o2) < 1e-10f) {
                    continue; /* (still shown, or never a triangle) */
                }
                for (j = 0; j < 3; j++) {
                    ht[nh][j] = o0[j], ht[nh][3 + j] = o1[j], ht[nh][6 + j] = o2[j];
                }
                ht[nh][9] = o0[8], ht[nh][10] = o0[9], ht[nh][11] = o1[8], ht[nh][12] = o1[9], ht[nh][13] = o2[8], ht[nh][14] = o2[9];
                hb[nh++] = b2;
            }
        }
        if (wp_patchLog) {
            printf("caps: %d hidden hair triangles\n", nh);
        }
        for (b2 = 0; nh > 0 && b2 < c.nb && npt < WP_PATCH_TRIS; b2++) { /* the skin of the head under them */
            int rr = c.bRec[b2];

            float lie = wp_tex_key((const uint32_t *)(m + c.hdr[b2])) == faceKey ? 0.2f : 0.5f; /* (the face's own picture: only
                                                                                 where the hair lay right on it; its eyes stay) */

            if (!wp_head_node(r[rr].node) || wp_is_hair(hairKey, hairs, wp_tex_key((const uint32_t *)(m + c.hdr[b2])))) {
                continue;
            }
            for (k = 0; k + 2 < c.bCount[b2] && npt < WP_PATCH_TRIS; k++) {
                float cap[3][12], uv[4][2];
                int hit[4], v;

                if (wp_area2((const float *)(m + c.off[c.bFirst[b2] + k]), (const float *)(m + c.off[c.bFirst[b2] + k + 1]),
                             (const float *)(m + c.off[c.bFirst[b2] + k + 2])) < 1e-14f) {
                    continue;
                }
                for (v = 0; v < 4; v++) { /* each corner and the middle: the hidden hair along its normal, lying on
                                             it (not a strand hanging in front of the face) */
                    const float *p3[3];
                    float p[3], d[3] = {0.0f, 0.0f, 0.0f}, dl, best = 1e9f, u, w;
                    int h, q2;

                    for (q2 = 0; q2 < 3; q2++) {
                        p3[q2] = (const float *)(m + c.off[c.bFirst[b2] + k + q2]);
                    }
                    for (j = 0; j < 3; j++) {
                        p[j] = v < 3 ? p3[v][j] : (p3[0][j] + p3[1][j] + p3[2][j]) / 3.0f;
                        d[j] = v < 3 ? p3[v][4 + j] : p3[0][4 + j] + p3[1][4 + j] + p3[2][4 + j];
                    }
                    dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                    hit[v] = -1;
                    for (j = 0; j < 3 && dl > 1e-6f; j++) {
                        d[j] /= dl;
                    }
                    for (h = 0; dl > 1e-6f && h < nh; h++) {
                        float t = wp_line_uv(p, d, ht[h], &u, &w);

                        if (t > -0.1f && t < lie && fabsf(t) < fabsf(best)) {
                            best = t;
                            hit[v] = h;
                            uv[v][0] = ht[h][9] * (1.0f - u - w) + ht[h][11] * u + ht[h][13] * w;
                            uv[v][1] = ht[h][10] * (1.0f - u - w) + ht[h][12] * u + ht[h][14] * w;
                        }
                    }
                    if (v < 3) {
                        memcpy(cap[v], p3[v], 48);
                        for (j = 0; j < 3 && dl > 1e-6f; j++) {
                            cap[v][j] = p3[v][j] + d[j] * WP_PUFF;
                        }
                    }
                }
                for (v = 0; v < 4 && hit[v] < 0; v++) {
                }
                if (v == 4) {
                    continue; /* (no hidden hair over it) */
                }
                for (j = 0; j < 3; j++) { /* a corner with none over it: the texture place of the hidden hair's nearest corner */
                    if (hit[j] < 0) {
                        float bd = 1e30f;
                        int h, q2;

                        for (h = 0; h < nh; h++) {
                            for (q2 = 0; q2 < 3; q2++) {
                                float dd = wp_d2(cap[j], ht[h] + q2 * 3);

                                if (dd < bd && hb[h] == hb[hit[v]]) {
                                    bd = dd;
                                    uv[j][0] = ht[h][9 + q2 * 2];
                                    uv[j][1] = ht[h][10 + q2 * 2];
                                }
                            }
                        }
                    }
                    cap[j][8] = uv[j][0];
                    cap[j][9] = uv[j][1];
                }
                hit[0] = hit[v];
                pt[npt].bat = hb[hit[0]];
                pt[npt].rec = rr;
                pt[npt].done = 0;
                memcpy(pt[npt].v, cap, sizeof(cap));
                npt++;
            }
        }
    }
    if (npt == 0) {
        result = 0;
        goto done;
    }

    for (i = 0; i < nr; i++) { /* the room: each record's new strips in one tag of their own */
        uint32_t bytes = 0;

        for (k = 0; k < npt; k++) {
            bytes += pt[k].rec == i ? 23 * 4 + 5 * 48 : 0; /* (at most: a strip of its own each) */
        }
        grow += bytes > 0 ? bytes + 32 : 0;
    }
    if ((out = calloc(1, mn + grow)) == NULL) {
        goto done;
    }
    memcpy(out, m, r[0].off);
    o = r[0].off;
    for (i = 0; i < nr; i++) {
        uint32_t t = r[i].off + 0x60, last = 0, end = r[i].off + r[i].size, w, tagAt, added = 0;
        uint8_t *rr = out + o;

        for (k = 0; k < npt && pt[k].rec != i; k++) {
        }
        if (k == npt || r[i].size <= 0x40) { /* (nothing new: as it was) */
            memcpy(rr, m + r[i].off, r[i].size);
            o += r[i].size;
            continue;
        }
        while (t + 16 <= end) { /* its last tag */
            uint32_t tag = wp_u32(m + t);

            last = t;
            t += 16 + (tag & 0xFFFF) * 16;
            if (tag >> 28 == 6) {
                break;
            }
        }
        memcpy(rr, m + r[i].off, t - r[i].off);
        wp_put32(rr + (last - r[i].off), (wp_u32(m + last) & 0x0FFFFFFFu) | 0x10000000u); /* (no longer the last) */
        w = t - r[i].off;
        tagAt = w;
        w += 8;
        for (;;) { /* the strips: one source strip's triangles each, joined by points said twice */
            int bat, n = 0, v;
            uint32_t *dst = (uint32_t *)(rr + w), hw[20];
            float *vp = (float *)(dst + 22);

            for (k = 0; k < npt && (pt[k].done || pt[k].rec != i); k++) {
            }
            if (k == npt) {
                break;
            }
            bat = pt[k].bat;
            for (j = k; j < npt && n + (n > 0 ? 5 : 3) <= WP_STRIP_MAX; j++) {
                if (pt[j].done || pt[j].rec != i || pt[j].bat != bat) {
                    continue;
                }
                if (n > 0) { /* (two points said twice: the triangles between are flat, nothing drawn) */
                    memcpy(vp + n * 12, vp + (n - 1) * 12, 48);
                    memcpy(vp + (n + 1) * 12, pt[j].v[0], 48);
                    n += 2;
                }
                for (v = 0; v < 3; v++) {
                    memcpy(vp + (n + v) * 12, pt[j].v[v], 48);
                }
                n += 3;
                pt[j].done = 1;
            }
            memcpy(hw, m + c.hdr[bat], 80);
            hw[12] = (hw[12] & ~0x7FFFu) | (uint32_t)n; /* (the counts of its GIF tags) */
            hw[16] = (hw[16] & ~0x7FFFu) | (uint32_t)n;
            dst[0] = WP_VIF_BATCH;
            memcpy(dst + 1, hw, 80);
            dst[21] = 0x6C008005u | ((uint32_t)(3 * n) << 16);
            dst[22 + 12 * n] = 0x17000000u; /* MSCNT */
            w += (23 + 12 * (uint32_t)n) * 4;
            added += (uint32_t)n;
        }
        while (((w - tagAt) & 15) != 0) {
            wp_put32(rr + w, 0);
            w += 4;
        }
        wp_put32(rr + tagAt, 0x60000000u | ((w - tagAt - 16) / 16));
        wp_put32(rr + tagAt + 4, 0);
        memcpy(rr + w, m + t, end - t); /* (its end) */
        w += end - t;
        wp_put32(rr, w);
        *(uint16_t *)(rr + 8) = (uint16_t)(*(const uint16_t *)(m + r[i].off + 8) + added);
        o += w;
    }
    {
        uint32_t recEnd = r[nr - 1].off + r[nr - 1].size, seam = wp_u32(m + 0x70);
        int32_t delta = (int32_t)(o - recEnd);

        memcpy(out + o, m + recEnd, mn - recEnd);
        if (seam != 0 && seam >= recEnd && seam + 16 <= mn) { /* (the seams' offsets, after the records) */
            int32_t sec[4];

            memcpy(sec, out + seam + delta, 16);
            sec[2] += delta;
            sec[3] += delta;
            memcpy(out + seam + delta, sec, 16);
            wp_put32(out + 0x70, (uint32_t)((int32_t)seam + delta));
        }
        *om = out;
        *omn = o + (mn - recEnd);
    }
    result = npt;
done:
    free(c.hdr);
    free(c.bRec);
    free(c.bFirst);
    free(c.bCount);
    free(c.off);
    free(c.iRec);
    free(c.iBat);
    free(c.iDup);
    free(id);
    free(order);
    free(pos);
    free(up);
    free(used);
    free(bnd);
    free(match);
    free(opair);
    free(adj);
    free(seen);
    free(loop);
    free(cov);
    free(opened);
    free(ed);
    free(pt);
    return result;
}
