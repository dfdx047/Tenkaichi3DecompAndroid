/*
 * Graphics Synthesizer, reference implementation in software.
 *
 * The game's drawing code runs unchanged and produces what it produced on the PS2: one DMA chain per frame on the
 * VIF1 channel, carrying GIF packets (register writes, primitives, texture uploads) for the GS and data for the
 * VU1 vertex programs. This file interprets the chain and draws the GS part into an ordinary RGBA image:
 *     DMA chain  ->  VIF1 commands  ->  GIF packets  ->  GS registers and vertices  ->  rasteriser.
 * It is the readable, dependency-free reference: screenshots for tests, and the specification by example for the
 * GPU renderer, which will take over from the "GS registers and vertices" stage.
 *
 * Not here yet: the VU1 vertex programs (3D models: their data is skipped), reading the frame buffer back as a
 * texture, mip-mapping, fog, dithering, bilinear filtering.
 * GS memory is emulated with its real layout (4 MB, pages / blocks / columns per pixel format): the game uploads
 * 4-bit and 8-bit textures as 32-bit images whose bytes are already arranged for the indexed formats, and it
 * draws into memory that it later uses as a texture, so uploads, textures, palettes and frame buffers must all
 * address the same memory the same way.
 *
 * Compiled with the host's float unit (this is not simulation code). BT3_GS=1 (or any value) enables the
 * software rasteriser; BT3_GS=gpu sends the primitives to the GPU back end (gs_gpu.c) and opens a window.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include "gs_internal.h"

#define SURF_W 1024
#define SURF_H 1024

/* ------------------------------------------------------------------------------------------------ GS state */

typedef struct Target {
    uint32_t fbp;
    uint32_t *z;    /* depth, one word per pixel (kept outside GS memory) */
    unsigned drawn; /* pixels written since the last frame end */
} Target;

GsState gGs;
uint32_t gGsPageGen[512];
unsigned gGsFbUploads; /* bit 0 / 1: pixels were uploaded into display buffer 0 / 0x70 since the back end last looked */
unsigned gGsFrame;
#define gs gGs
#define Vertex GsVertex
#define sFrame gGsFrame
static int sGpu; /* 1: the GPU back end draws (BT3_GS=gpu), 0: the software rasteriser below */

static Target sTargets[16];
static int sTargetCount;
static unsigned sMissingTex;
static unsigned sPix[5]; /* per frame: outside scissor, failed alpha test, failed depth test, colour written, depth written */
static int sDrawFrom;       /* BT3_GS_FROM=<frame>: draw only from this frame on (the reference is slow) */
static int sProbeX = -1, sProbeY = -1;
static int sDumpFrame = -1; /* BT3_GS_DUMP=<frame>: list that frame's primitives */
static unsigned sStat[8]; /* per frame: DMA tags, VIF codes, DIRECT quadwords, GIF tags, register writes, vertices, image quadwords, unknown VIF */

/* ---- GS local memory: 4 MB. A buffer pointer counts 256-byte blocks, a buffer width counts 64 pixels. Where a
   pixel lives depends on its format: pages of 32 blocks, blocks in a format-specific order inside the page,
   "columns" inside the block. The tables are the hardware's; the two indexed ones are generated from their first
   column (odd columns exchange the two halves of every group of eight). ---- */
static uint32_t sVram[1 << 20];
static const uint8_t kBlock32[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
static const uint8_t kBlock16[8][4] = {{0, 2, 8, 10}, {1, 3, 9, 11}, {4, 6, 12, 14}, {5, 7, 13, 15}, {16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31}};
static const uint8_t kBlock16S[8][4] = {{0, 2, 16, 18}, {1, 3, 17, 19}, {8, 10, 24, 26}, {9, 11, 25, 27}, {4, 6, 20, 22}, {5, 7, 21, 23}, {12, 14, 28, 30}, {13, 15, 29, 31}};
static const uint8_t kCol32[8][8] = {{0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}, {16, 17, 20, 21, 24, 25, 28, 29}, {18, 19, 22, 23, 26, 27, 30, 31},
                                     {32, 33, 36, 37, 40, 41, 44, 45}, {34, 35, 38, 39, 42, 43, 46, 47}, {48, 49, 52, 53, 56, 57, 60, 61}, {50, 51, 54, 55, 58, 59, 62, 63}};
static const uint8_t kCol16[8][16] = {
    {0, 2, 8, 10, 16, 18, 24, 26, 1, 3, 9, 11, 17, 19, 25, 27}, {4, 6, 12, 14, 20, 22, 28, 30, 5, 7, 13, 15, 21, 23, 29, 31},
    {32, 34, 40, 42, 48, 50, 56, 58, 33, 35, 41, 43, 49, 51, 57, 59}, {36, 38, 44, 46, 52, 54, 60, 62, 37, 39, 45, 47, 53, 55, 61, 63},
    {64, 66, 72, 74, 80, 82, 88, 90, 65, 67, 73, 75, 81, 83, 89, 91}, {68, 70, 76, 78, 84, 86, 92, 94, 69, 71, 77, 79, 85, 87, 93, 95},
    {96, 98, 104, 106, 112, 114, 120, 122, 97, 99, 105, 107, 113, 115, 121, 123}, {100, 102, 108, 110, 116, 118, 124, 126, 101, 103, 109, 111, 117, 119, 125, 127}};
static const uint8_t kCol8First[4][16] = {{0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54}, {8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
                                          {33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23}, {41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31}};
static const uint8_t kCol4First[4][32] = {
    {0, 8, 32, 40, 64, 72, 96, 104, 2, 10, 34, 42, 66, 74, 98, 106, 4, 12, 36, 44, 68, 76, 100, 108, 6, 14, 38, 46, 70, 78, 102, 110},
    {16, 24, 48, 56, 80, 88, 112, 120, 18, 26, 50, 58, 82, 90, 114, 122, 20, 28, 52, 60, 84, 92, 116, 124, 22, 30, 54, 62, 86, 94, 118, 126},
    {65, 73, 97, 105, 1, 9, 33, 41, 67, 75, 99, 107, 3, 11, 35, 43, 69, 77, 101, 109, 5, 13, 37, 45, 71, 79, 103, 111, 7, 15, 39, 47},
    {81, 89, 113, 121, 17, 25, 49, 57, 83, 91, 115, 123, 19, 27, 51, 59, 85, 93, 117, 125, 21, 29, 53, 61, 87, 95, 119, 127, 23, 31, 55, 63}};
static uint8_t sCol8[16][16];
static uint16_t sCol4[16][32];

/* For the uploads: where the pixel (x, y) lies inside its page, as an index of pixels of its own size. A page is
   64 x 32 pixels of 32 bits, 64 x 64 of 16, 128 x 64 of 8, 128 x 128 of 4; the page's own number comes on top. */
static uint16_t sIn32[32][64], sIn16[2][64][64], sIn8[64][128], sIn4[128][128];

static void tables_init(void) {
    int y, x;
    for (y = 0; y < 128; y++) {
        for (x = 0; x < 128; x++) {
            if (y < 32 && x < 64) {
                sIn32[y][x] = (uint16_t)((kBlock32[(y >> 3) & 3][(x >> 3) & 7] << 6) + kCol32[y & 7][x & 7]);
            }
            if (y < 64 && x < 64) {
                sIn16[0][y][x] = (uint16_t)((kBlock16[(y >> 3) & 7][(x >> 4) & 3] << 7) + kCol16[y & 7][x & 15]);
                sIn16[1][y][x] = (uint16_t)((kBlock16S[(y >> 3) & 7][(x >> 4) & 3] << 7) + kCol16[y & 7][x & 15]);
            }
        }
    }
    for (y = 0; y < 16; y++) {
        int col = y >> 2;
        for (x = 0; x < 16; x++) {
            int sx = (col & 1) ? (x ^ 4) : x;
            sCol8[y][x] = (uint8_t)(kCol8First[y & 3][sx] + col * 64);
        }
        for (x = 0; x < 32; x++) {
            int sx = (col & 1) ? (x ^ 4) : x;
            sCol4[y][x] = (uint16_t)(kCol4First[y & 3][sx] + col * 128);
        }
    }
    for (y = 0; y < 128; y++) { /* (after the two tables above, which these use) */
        for (x = 0; x < 128; x++) {
            if (y < 64) {
                sIn8[y][x] = (uint16_t)((kBlock32[(y >> 4) & 3][(x >> 4) & 7] << 8) + sCol8[y & 15][x & 15]);
            }
            sIn4[y][x] = (uint16_t)((kBlock16[(y >> 4) & 7][(x >> 5) & 3] << 9) + sCol4[y & 15][x & 31]);
        }
    }
}

/* Reads (write = 0) or writes the pixel (x, y) of a buffer at bp, width bw, format psm. Value: the stored bits. */
static uint32_t vram_rw(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y, int write, uint32_t v) {
    uint32_t a, *w;

    uint32_t zx = 0; /* the depth formats (0x30..0x3A) are the colour formats with the blocks of a page in another order */

    if (bw == 0) {
        bw = 1;
    }
    if ((psm & 0x30) == 0x30) {
        zx = 0x18;
        psm &= 0x0F;
    }
    switch (psm) {
    case 0x02: case 0x0A: /* 16 bits */
        a = ((bp + ((y >> 1) & ~0x1Fu) * bw + ((x >> 1) & ~0x1Fu) +
              ((psm == 0x0A ? kBlock16S : kBlock16)[(y >> 3) & 7][(x >> 4) & 3] ^ zx)) << 7) + kCol16[y & 7][x & 15];
        w = &sVram[(a >> 1) & 0xFFFFF];
        if (write) { *w = (*w & ~(0xFFFFu << ((a & 1) * 16))) | (v & 0xFFFF) << ((a & 1) * 16); return 0; }
        return (*w >> ((a & 1) * 16)) & 0xFFFF;
    case 0x13: /* 8 bits */
        a = ((bp + ((y >> 1) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock32[(y >> 4) & 3][(x >> 4) & 7]) << 8) + sCol8[y & 15][x & 15];
        w = &sVram[(a >> 2) & 0xFFFFF];
        if (write) { *w = (*w & ~(0xFFu << ((a & 3) * 8))) | (v & 0xFF) << ((a & 3) * 8); return 0; }
        return (*w >> ((a & 3) * 8)) & 0xFF;
    case 0x14: /* 4 bits */
        a = ((bp + ((y >> 2) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock16[(y >> 4) & 7][(x >> 5) & 3]) << 9) + sCol4[y & 15][x & 31];
        w = &sVram[(a >> 3) & 0xFFFFF];
        if (write) { *w = (*w & ~(0xFu << ((a & 7) * 4))) | (v & 0xF) << ((a & 7) * 4); return 0; }
        return (*w >> ((a & 7) * 4)) & 0xF;
    default: /* 32 bits, and the formats that live inside a 32-bit pixel */
        a = ((bp + (y & ~0x1Fu) * bw + ((x >> 1) & ~0x1Fu) + (kBlock32[(y >> 3) & 3][(x >> 3) & 7] ^ zx)) << 6) + kCol32[y & 7][x & 7];
        w = &sVram[a & 0xFFFFF];
        switch (psm) {
        case 0x01: if (write) { *w = (*w & 0xFF000000u) | (v & 0xFFFFFF); return 0; } return *w & 0xFFFFFF;
        case 0x1B: if (write) { *w = (*w & 0x00FFFFFFu) | v << 24; return 0; } return *w >> 24;
        case 0x24: if (write) { *w = (*w & 0xF0FFFFFFu) | (v & 15) << 24; return 0; } return (*w >> 24) & 15;
        case 0x2C: if (write) { *w = (*w & 0x0FFFFFFFu) | (v & 15) << 28; return 0; } return *w >> 28;
        default: if (write) { *w = v; return 0; } return *w;
        }
    }
}

const uint8_t *Gs_BlockPtr(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y) {
    uint32_t bn;

    if (bw == 0) {
        bw = 1;
    }
    if (psm == 0x13) {
        bn = bp + ((y >> 1) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock32[(y >> 4) & 3][(x >> 4) & 7];
    } else if (psm == 0x14) {
        bn = bp + ((y >> 2) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock16[(y >> 4) & 7][(x >> 5) & 3];
    } else {
        bn = bp + (y & ~0x1Fu) * bw + ((x >> 1) & ~0x1Fu) + kBlock32[(y >> 3) & 3][(x >> 3) & 7];
    }
    return (const uint8_t *)sVram + ((bn & 0x3FFF) << 8);
}

/* The emulated GS as the game left it: its memory and registers. A snapshot of the game's state (gs/state.c) does
   not hold these, and a game put back to an earlier moment would find the textures of whatever ran in between in
   GS memory (it only uploads a menu's textures when the menu is loaded). Kept when the game switches to an
   online session, brought back when it returns. */
static struct { uint32_t *vram; GsState gs; uint32_t gen[512]; int valid; } sKept;

static uint32_t sMoved;

void Gs_StateKeep(void) {
    extern void GsVu1_StateKeep(int restore); /* gs_vu1.c: the vertex unit's memory and programs */
    if (sKept.vram == NULL) {
        sKept.vram = malloc(sizeof(sVram));
    }
    memcpy(sKept.vram, sVram, sizeof(sVram));
    sKept.gs = gGs;
    memcpy(sKept.gen, gGsPageGen, sizeof(sKept.gen));
    sKept.valid = 1;
    GsVu1_StateKeep(0);
}

void Gs_StateBack(void) {
    extern void GsVu1_StateKeep(int restore);
    int i;
    if (!sKept.valid) {
        return;
    }
    memcpy(sVram, sKept.vram, sizeof(sVram));
    gGs = sKept.gs;
    /* The pages' upload generations as they were, all moved on by one amount that no page has had: nothing
       decoded from the pages in between is taken for them. A page that was never uploaded to stays at 0, which
       is how the frame buffers are told from texture memory (the rectangle case of the drawing code: with every
       page moved, a full-screen pass went into GS memory instead of the picture, which came back darker). */
    sMoved += 0x01000000u;
    for (i = 0; i < 512; i++) {
        gGsPageGen[i] = sKept.gen[i] != 0 ? sKept.gen[i] + sMoved : 0;
    }
    GsVu1_StateKeep(1);
    if (sGpu) {
        extern void GsGpu_PagesMoved(uint32_t delta); /* gs_gpu.c */
        GsGpu_PagesMoved(0x01000000u);
    }
}

uint32_t Gs_VramRead(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y) {
    return vram_rw(bp, bw, psm, x, y, 0, 0);
}

/* A hash of one 8 KB page of GS memory, recomputed only after an upload touched the page. The GPU back end keys
   its texture cache on these, so a texture the game uploads again unchanged (it streams the stage's textures to
   one address many times per frame) is recognised instead of decoded again. */
uint32_t Gs_PageHash(uint32_t page) {
    static uint32_t hash[512], seen[512];
    static uint8_t valid[512];
    uint32_t h = 2166136261u, i;
    const uint32_t *w;

    page &= 511;
    if (valid[page] && seen[page] == gGsPageGen[page]) {
        return hash[page];
    }
    w = &sVram[page * 2048];
    {   /* four independent 64-bit lanes (the processor runs them side by side), folded at the end */
        uint64_t l[4] = {0x9E3779B97F4A7C15ull, 0xC2B2AE3D27D4EB4Full, 0x165667B19E3779F9ull, 0x27D4EB2F165667C5ull}, q;
        for (i = 0; i < 2048; i += 8) {
            uint32_t j;
            for (j = 0; j < 4; j++) {
                memcpy(&q, &w[i + j * 2], 8);
                l[j] = (l[j] ^ q) * 0x9FB21C651E98DF25ull;
                l[j] ^= l[j] >> 32;
            }
        }
        q = (l[0] ^ (l[1] << 13 | l[1] >> 51) ^ (l[2] << 29 | l[2] >> 35) ^ (l[3] << 47 | l[3] >> 17)) * 0x9FB21C651E98DF25ull;
        h = (uint32_t)(q ^ q >> 32);
    }
    hash[page] = h;
    seen[page] = gGsPageGen[page];
    valid[page] = 1;
    return h;
}

static Target *target_get(uint32_t fbp) {
    int i;
    for (i = 0; i < sTargetCount; i++) {
        if (sTargets[i].fbp == fbp) {
            return &sTargets[i];
        }
    }
    if (sTargetCount == 16) {
        return &sTargets[0];
    }
    sTargets[sTargetCount].fbp = fbp;
    sTargets[sTargetCount].z = calloc(SURF_W * SURF_H, 4);
    return &sTargets[sTargetCount++];
}

/* ------------------------------------------------------------------------------------------------ textures */

int Gs_PsmBits(uint32_t psm) {
    if ((psm & 0x30) == 0x30) {
        psm &= 0x0F; /* a depth buffer read as a texture: 32, 24 or 16 bits like the colour formats */
    }
    switch (psm) {
    case 0x00: return 32;
    case 0x01: return 24;
    case 0x02: case 0x0A: return 16;
    case 0x13: case 0x1B: return 8;
    case 0x14: case 0x24: case 0x2C: return 4;
    default: return 32;
    }
}

/* A stored colour of format psm as R, G, B, A (TEXA supplies alpha where the format has none or one bit). */
uint32_t Gs_Expand(uint32_t c, uint32_t psm) {
    uint32_t ta0 = gs.texa & 0xFF, ta1 = (gs.texa >> 32) & 0xFF, aem = (gs.texa >> 15) & 1;

    if ((psm & 0x30) == 0x30) {
        psm &= 0x0F;
    }

    if (psm == 0x00) {
        return c;
    }
    if (psm == 0x01) {
        c &= 0xFFFFFF;
        return c | ((aem && c == 0 ? 0 : ta0) << 24);
    }
    {
        uint32_t r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
        uint32_t a = (c & 0x8000) ? ta1 : (aem && (c & 0x7FFF) == 0 ? 0 : ta0);
        return r | g << 8 | b << 16 | a << 24;
    }
}

static uint32_t texel(int ctx, int u, int v) {
    uint64_t t0 = gs.tex0[ctx], cl = gs.clamp[ctx];
    uint32_t tbp = t0 & 0x3FFF, tbw = (t0 >> 14) & 0x3F, psm = (t0 >> 20) & 0x3F, tw = 1u << ((t0 >> 26) & 15), th = 1u << ((t0 >> 30) & 15);
    uint32_t cbp = (t0 >> 37) & 0x3FFF, cpsm = (t0 >> 51) & 15, csa = (t0 >> 56) & 31;
    uint32_t wms = cl & 3, wmt = (cl >> 2) & 3;
    int minu = (cl >> 4) & 0x3FF, maxu = (cl >> 14) & 0x3FF, minv = (cl >> 24) & 0x3FF, maxv = (cl >> 34) & 0x3FF;
    uint32_t c;

    if (wms == 0) { u &= (int)tw - 1; } else if (wms == 1) { u = u < 0 ? 0 : u >= (int)tw ? (int)tw - 1 : u; }
    else if (wms == 2) { u = u < minu ? minu : u > maxu ? maxu : u; } else { u = (u & minu) | maxu; }
    if (wmt == 0) { v &= (int)th - 1; } else if (wmt == 1) { v = v < 0 ? 0 : v >= (int)th ? (int)th - 1 : v; }
    else if (wmt == 2) { v = v < minv ? minv : v > maxv ? maxv : v; } else { v = (v & minv) | maxv; }
    c = vram_rw(tbp, tbw, psm, (uint32_t)u, (uint32_t)v, 0, 0);
    if (Gs_PsmBits(psm) > 8) {
        return Gs_Expand(c, psm);
    }
    /* Palette (CSM1): a 16-colour palette is an 8 x 2 block at CBP, a 256-colour one 16 x 16 read in the GS's order.
       CSA only moves a 16-colour palette inside the GS's internal buffer, not in memory. */
    (void)csa;
    if (Gs_PsmBits(psm) == 8) {
        c = (c & 0xE7) | ((c & 8) << 1) | ((c & 0x10) >> 1);
        return Gs_Expand(vram_rw(cbp, 1, cpsm, c & 15, c >> 4, 0, 0), cpsm);
    }
    return Gs_Expand(vram_rw(cbp, 1, cpsm, c & 7, c >> 3, 0, 0), cpsm);
}

/* ------------------------------------------------------------------------------------------------ pixels */

static void pixel(Target *sf, int ctx, int x, int y, uint32_t z, uint32_t src) {
    uint64_t test = gs.test[ctx], al = gs.alpha[ctx], zb = gs.zbuf[ctx], pr = gs.prim;
    uint64_t sc = gs.scissor[ctx], fr = gs.frame[ctx];
    uint32_t fbp = (fr & 0x1FF) << 5, fbw = (fr >> 16) & 0x3F, fpsm = (fr >> 24) & 0x3F;
    uint32_t d, sr, sg, sb, sa, out[3], zbp, zpsm, zmax;
    int i, write_rgb = 1, write_z = !((zb >> 32) & 1);

    /* BT3_GS_PROBE=x,y: in the BT3_GS_DUMP frame, report every primitive that reaches this pixel */
    if (sDumpFrame == (int)sFrame && x == sProbeX && y == sProbeY) {
        uint64_t t0 = gs.tex0[ctx];
        fprintf(stderr, "  probe: prim %llx z %08x src %08x | tex %04x psm %02x %ux%u tcc %u tfx %u cbp %04x | alpha %llx test %llx frame %llx zbuf %llx clamp %llx | depth there %08x\n",
                (unsigned long long)gs.prim, z, src, (unsigned)(t0 & 0x3FFF), (unsigned)((t0 >> 20) & 0x3F), 1u << ((t0 >> 26) & 15),
                1u << ((t0 >> 30) & 15), (unsigned)((t0 >> 34) & 1), (unsigned)((t0 >> 35) & 3), (unsigned)((t0 >> 37) & 0x3FFF),
                (unsigned long long)gs.alpha[ctx], (unsigned long long)gs.test[ctx], (unsigned long long)gs.frame[ctx],
                (unsigned long long)gs.zbuf[ctx], (unsigned long long)gs.clamp[ctx],
                vram_rw((uint32_t)(gs.zbuf[ctx] & 0x1FF) << 5, (uint32_t)((gs.frame[ctx] >> 16) & 0x3F), 0x30 | (uint32_t)((gs.zbuf[ctx] >> 24) & 15), (uint32_t)x, (uint32_t)y, 0, 0));
    }

    if (x < (int)(sc & 0x7FF) || x > (int)((sc >> 16) & 0x7FF) || y < (int)((sc >> 32) & 0x7FF) || y > (int)((sc >> 48) & 0x7FF) ||
        x < 0 || y < 0 || x >= SURF_W || y >= SURF_H) {
        sPix[0]++;
        return;
    }
    sa = src >> 24;
    if (test & 1) { /* alpha test */
        uint32_t ref = (test >> 4) & 0xFF;
        int pass;
        switch ((test >> 1) & 7) {
        case 0: pass = 0; break;
        case 1: pass = 1; break;
        case 2: pass = sa < ref; break;
        case 3: pass = sa <= ref; break;
        case 4: pass = sa == ref; break;
        case 5: pass = sa >= ref; break;
        case 6: pass = sa > ref; break;
        default: pass = sa != ref; break;
        }
        if (!pass) {
            switch ((test >> 12) & 3) {
            case 0: sPix[1]++; return;             /* keep */
            case 1: write_z = 0; break;            /* frame buffer only */
            case 2: write_rgb = 0; break;          /* z only */
            default: write_z = 0; break;           /* RGB only */
            }
        }
    }
    /* The depth buffer lives in GS memory like everything else (the game clears it by drawing into it as a frame
       buffer and reads it as a texture): ZBUF.ZBP, the frame's width, format 0x30 + ZBUF.PSM. */
    zbp = (uint32_t)(zb & 0x1FF) << 5;
    zpsm = 0x30 | (uint32_t)((zb >> 24) & 15);
    zmax = Gs_PsmBits(zpsm) == 32 ? 0xFFFFFFFFu : Gs_PsmBits(zpsm) == 24 ? 0xFFFFFFu : 0xFFFFu;
    if (z > zmax) {
        z = zmax;
    }
    if ((test >> 16) & 1) { /* depth test */
        uint32_t zd = vram_rw(zbp, fbw, zpsm, (uint32_t)x, (uint32_t)y, 0, 0);
        switch ((test >> 17) & 3) {
        case 0: sPix[2]++; return;
        case 1: break;
        case 2: if (z < zd) { sPix[2]++; return; } break;
        default: if (z <= zd) { sPix[2]++; return; } break;
        }
    }
    d = vram_rw(fbp, fbw, fpsm, (uint32_t)x, (uint32_t)y, 0, 0);
    if (Gs_PsmBits(fpsm) == 16) {
        d = (d & 31) << 3 | ((d >> 5) & 31) << 11 | ((d >> 10) & 31) << 19 | ((d & 0x8000) ? 0x80000000u : 0);
    } else if (fpsm == 1) {
        d |= 0x80000000u;
    }
    sr = src & 0xFF; sg = (src >> 8) & 0xFF; sb = (src >> 16) & 0xFF;
    out[0] = sr; out[1] = sg; out[2] = sb;
    if ((pr >> 6) & 1) { /* blending: ((A - B) * C >> 7) + D */
        uint32_t s[3] = {sr, sg, sb};
        uint32_t c = ((al >> 4) & 3) == 0 ? sa : ((al >> 4) & 3) == 1 ? d >> 24 : (uint32_t)((al >> 32) & 0xFF);
        for (i = 0; i < 3; i++) {
            int dc = (int)((d >> (8 * i)) & 0xFF);
            int a = (al & 3) == 0 ? (int)s[i] : (al & 3) == 1 ? dc : 0;
            int b = ((al >> 2) & 3) == 0 ? (int)s[i] : ((al >> 2) & 3) == 1 ? dc : 0;
            int dd = ((al >> 6) & 3) == 0 ? (int)s[i] : ((al >> 6) & 3) == 1 ? dc : 0;
            int v = (((a - b) * (int)c) >> 7) + dd;
            out[i] = v < 0 ? 0 : v > 255 ? 255 : (uint32_t)v;
        }
    }
    if (write_rgb) {
        /* FRAME.FBMSK: bits that are set are NOT written. The game depends on it (it moves single channels
           between the depth buffer, the frame's alpha byte and work buffers, sometimes through a 16-bit view
           of 32-bit memory). The mask is given for 32-bit pixels and narrows with the format. */
        uint32_t c = out[0] | out[1] << 8 | out[2] << 16 | sa << 24, m = (uint32_t)(fr >> 32), old;
        if (Gs_PsmBits(fpsm) == 16) {
            c = out[0] >> 3 | (out[1] >> 3) << 5 | (out[2] >> 3) << 10 | (sa & 0x80) << 8;
            m = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 16) & 0x8000);
        }
        if (m != 0) {
            old = vram_rw(fbp, fbw, fpsm, (uint32_t)x, (uint32_t)y, 0, 0);
            c = (old & m) | (c & ~m);
        }
        vram_rw(fbp, fbw, fpsm, (uint32_t)x, (uint32_t)y, 1, c);
        sf->drawn++;
        sPix[3]++;
    }
    if (write_z) {
        vram_rw(zbp, fbw, zpsm, (uint32_t)x, (uint32_t)y, 1, z);
    }
}

/* Vertex colour and texture combined (TEX0.TFX / TCC). */
static uint32_t shade(int ctx, uint32_t vc, float s, float t, float q, int u, int v) {
    uint64_t pr = gs.prim, t0 = gs.tex0[ctx];
    uint32_t tc, c[4], out = 0, tfx = (t0 >> 35) & 3, tcc = (t0 >> 34) & 1;
    int i;

    if (!((pr >> 4) & 1)) {
        return vc;
    }
    if ((pr >> 8) & 1) {
        tc = texel(ctx, u >> 4, v >> 4);
    } else {
        float w = q != 0.0f ? 1.0f / q : 1.0f;
        tc = texel(ctx, (int)(s * w * (float)(1u << ((t0 >> 26) & 15))), (int)(t * w * (float)(1u << ((t0 >> 30) & 15))));
    }
    for (i = 0; i < 3; i++) {
        uint32_t cv = (vc >> (8 * i)) & 0xFF, ct = (tc >> (8 * i)) & 0xFF, av = vc >> 24;
        c[i] = tfx == 0 ? (ct * cv) >> 7 : tfx == 1 ? ct : ((ct * cv) >> 7) + av;
        if (c[i] > 255) { c[i] = 255; }
        out |= c[i] << (8 * i);
    }
    {
        uint32_t av = vc >> 24, at = tc >> 24;
        c[3] = !tcc ? av : tfx == 0 ? (at * av) >> 7 : tfx == 2 ? at + av : at;
        if (c[3] > 255) { c[3] = 255; }
    }
    return out | c[3] << 24;
}

/* ------------------------------------------------------------------------------------------------ primitives */

static uint32_t vcol(const Vertex *v) {
    return v->r | v->g << 8 | v->b << 16 | (uint32_t)v->a << 24;
}

static void draw_sprite(Target *sf, int ctx, const Vertex *a, const Vertex *b) {
    float x0 = a->x, x1 = b->x, y0 = a->y, y1 = b->y;
    int xa, xb, ya, yb, x, y;
    uint32_t col = vcol(b);

    if (x0 > x1) { const Vertex *t = a; a = b; b = t; x0 = a->x; x1 = b->x; }
    xa = (int)(x0 + 0.9375f); xb = (int)(x1 + 0.9375f);
    if (y0 <= y1) { ya = (int)(y0 + 0.9375f); yb = (int)(y1 + 0.9375f); } else { ya = (int)(y1 + 0.9375f); yb = (int)(y0 + 0.9375f); }
    for (y = ya; y < yb; y++) {
        float fy = y1 != y0 ? ((float)y - y0) / (y1 - y0) : 0.0f;
        for (x = xa; x < xb; x++) {
            float fx = x1 != x0 ? ((float)x - x0) / (x1 - x0) : 0.0f;
            pixel(sf, ctx, x, y, b->z, shade(ctx, col, a->s + (b->s - a->s) * fx, a->t + (b->t - a->t) * fy, b->q,
                                             (int)((float)a->u + (float)(b->u - a->u) * fx), (int)((float)a->v + (float)(b->v - a->v) * fy)));
        }
    }
}

static float edge(const Vertex *a, const Vertex *b, float x, float y) {
    return (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x);
}

static void draw_triangle(Target *sf, int ctx, const Vertex *a, const Vertex *b, const Vertex *c) {
    float area = edge(a, b, c->x, c->y);
    int gouraud = (gs.prim >> 3) & 1;
    float minx = a->x, maxx = a->x, miny = a->y, maxy = a->y;
    int x, y;

    if (area == 0.0f) {
        return;
    }
    if (b->x < minx) { minx = b->x; } if (c->x < minx) { minx = c->x; }
    if (b->x > maxx) { maxx = b->x; } if (c->x > maxx) { maxx = c->x; }
    if (b->y < miny) { miny = b->y; } if (c->y < miny) { miny = c->y; }
    if (b->y > maxy) { maxy = b->y; } if (c->y > maxy) { maxy = c->y; }
    if (minx < 0) { minx = 0; } if (miny < 0) { miny = 0; }
    if (maxx > SURF_W - 1) { maxx = SURF_W - 1; } if (maxy > SURF_H - 1) { maxy = SURF_H - 1; }
    for (y = (int)miny; y <= (int)maxy; y++) {
        for (x = (int)minx; x <= (int)maxx; x++) {
            /* weights in double precision and results rounded, not truncated: with float weights that sum to just
               under 1 a flat triangle's depth came out one unit low on some pixels, and the fighters' black outline
               hull (drawn at almost the same depth as the body) showed through as speckles */
            double w0 = (double)edge(b, c, (float)x, (float)y) / (double)area, w1 = (double)edge(c, a, (float)x, (float)y) / (double)area, w2 = 1.0 - w0 - w1;
            uint32_t col;

            if (w0 < 0.0 || w1 < 0.0 || w2 < -1e-9) {
                continue;
            }
            if (gouraud) {
                col = (uint32_t)(a->r * w0 + b->r * w1 + c->r * w2 + 0.5) | (uint32_t)(a->g * w0 + b->g * w1 + c->g * w2 + 0.5) << 8 |
                      (uint32_t)(a->b * w0 + b->b * w1 + c->b * w2 + 0.5) << 16 | (uint32_t)(a->a * w0 + b->a * w1 + c->a * w2 + 0.5) << 24;
            } else {
                col = vcol(c);
            }
            pixel(sf, ctx, x, y, (uint32_t)((double)a->z * w0 + (double)b->z * w1 + (double)c->z * w2 + 0.5),
                  shade(ctx, col, (float)(a->s * w0 + b->s * w1 + c->s * w2), (float)(a->t * w0 + b->t * w1 + c->t * w2),
                        (float)(a->q * w0 + b->q * w1 + c->q * w2), (int)(a->u * w0 + b->u * w1 + c->u * w2),
                        (int)(a->v * w0 + b->v * w1 + c->v * w2)));
        }
    }
}

static void draw_line(Target *sf, int ctx, const Vertex *a, const Vertex *b) {
    float dx = b->x - a->x, dy = b->y - a->y;
    int n = (int)((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy)), i;

    for (i = 0; i <= n; i++) {
        float f = n ? (float)i / (float)n : 0.0f;
        pixel(sf, ctx, (int)(a->x + dx * f), (int)(a->y + dy * f), b->z, vcol(b));
    }
}

/* A vertex arrives (XYZ2 / XYZF2 with kick = 1, XYZ3 / XYZF3 with kick = 0). */
static void vertex(uint32_t x, uint32_t y, uint32_t z, int kick) {
    int ctx = (gs.prim >> 9) & 1, type = gs.prim & 7;
    Target *sf;
    Vertex v;

    sStat[5]++;
    v.x = ((float)(int)x - (float)(gs.xyoffset[ctx] & 0xFFFF)) / 16.0f;
    v.y = ((float)(int)y - (float)((gs.xyoffset[ctx] >> 32) & 0xFFFF)) / 16.0f;
    v.z = z;
    v.r = gs.rgbaq & 0xFF; v.g = (gs.rgbaq >> 8) & 0xFF; v.b = (gs.rgbaq >> 16) & 0xFF; v.a = (gs.rgbaq >> 24) & 0xFF;
    memcpy(&v.s, &gs.st, 4);
    { uint32_t tt = (uint32_t)(gs.st >> 32); memcpy(&v.t, &tt, 4); }
    v.q = gs.q;
    v.u = gs.uv & 0x3FFF; v.v = (gs.uv >> 16) & 0x3FFF;
    if (gs.vcount < 3) {
        gs.vtx[gs.vcount++] = v;
    } else {
        gs.vtx[0] = gs.vtx[1]; gs.vtx[1] = gs.vtx[2]; gs.vtx[2] = v;
    }
    gs.strip++;
    sf = target_get(gs.frame[ctx] & 0x1FF);
    {   /* BT3_GS_STOP=<n>: in the frame named by BT3_GS_FROM, draw only the first n primitives (to find what covers what) */
        static int stop = -1;
        static unsigned count, frame = 0xFFFFFFFFu;
        if (stop < 0) { stop = getenv("BT3_GS_STOP") != NULL ? atoi(getenv("BT3_GS_STOP")) : 0; }
        if (frame != sFrame) { frame = sFrame; count = 0; }
        /* counted in complete primitives, the unit of the BT3_GS_DUMP listing */
        if (kick && gs.vcount >= (type == 6 || type == 1 || type == 2 ? 2 : type == 0 ? 1 : 3)) { count++; }
        if (stop > 0 && (int)sFrame == sDrawFrom && count > (unsigned)stop) { kick = 0; }
    }
    if ((int)sFrame < sDrawFrom) {
        kick = 0; /* BT3_GS_FROM: uploads and state are processed, nothing is drawn yet */
    }
    if (sDumpFrame == (int)sFrame && kick && gs.vcount >= (type == 6 || type == 1 || type == 2 ? 2 : type == 0 ? 1 : 3)) {
        const Vertex *p0 = &gs.vtx[0], *p1 = &gs.vtx[gs.vcount - 1];
        fprintf(stderr, "  prim %d ctx %d fbp %03x abe %d tme %d fst %d | (%.1f,%.1f)-(%.1f,%.1f) z %08x uv (%d,%d)-(%d,%d) st (%.3f,%.3f) q %.3f "
                        "rgba %02x%02x%02x%02x | tbp %04x psm %02x tw %d th %d cbp %04x | xyoff %d,%d scis %d..%d,%d..%d alpha %llx test %llx frame %llx zbuf %llx\n",
                type, ctx, (unsigned)(gs.frame[ctx] & 0x1FF), (int)((gs.prim >> 6) & 1), (int)((gs.prim >> 4) & 1), (int)((gs.prim >> 8) & 1),
                p0->x, p0->y, p1->x, p1->y, p1->z, p0->u >> 4, p0->v >> 4, p1->u >> 4, p1->v >> 4, p1->s, p1->t, p1->q, p1->r, p1->g, p1->b, p1->a,
                (unsigned)(gs.tex0[ctx] & 0x3FFF), (unsigned)((gs.tex0[ctx] >> 20) & 0x3F), 1 << ((gs.tex0[ctx] >> 26) & 15),
                1 << ((gs.tex0[ctx] >> 30) & 15), (unsigned)((gs.tex0[ctx] >> 37) & 0x3FFF),
                (int)(gs.xyoffset[ctx] & 0xFFFF) >> 4, (int)((gs.xyoffset[ctx] >> 32) & 0xFFFF) >> 4,
                (int)(gs.scissor[ctx] & 0x7FF), (int)((gs.scissor[ctx] >> 16) & 0x7FF), (int)((gs.scissor[ctx] >> 32) & 0x7FF),
                (int)((gs.scissor[ctx] >> 48) & 0x7FF), (unsigned long long)gs.alpha[ctx], (unsigned long long)gs.test[ctx],
                (unsigned long long)gs.frame[ctx], (unsigned long long)gs.zbuf[ctx]);
        if ((gs.prim >> 4) & 1) {
            uint32_t tx = texel(ctx, 8, 8), cb = (uint32_t)((gs.tex0[ctx] >> 37) & 0x3FFF), cp = (uint32_t)((gs.tex0[ctx] >> 51) & 15);
            fprintf(stderr, "    tex: tbw %u cpsm %x csm %u csa %u tcc %u tfx %u texa %llx | texel(8,8) %08x | clut[0..3] %08x %08x %08x %08x\n",
                    (unsigned)((gs.tex0[ctx] >> 14) & 0x3F), cp, (unsigned)((gs.tex0[ctx] >> 55) & 1), (unsigned)((gs.tex0[ctx] >> 56) & 31),
                    (unsigned)((gs.tex0[ctx] >> 34) & 1), (unsigned)((gs.tex0[ctx] >> 35) & 3), (unsigned long long)gs.texa, tx,
                    vram_rw(cb, 1, cp, 0, 0, 0, 0), vram_rw(cb, 1, cp, 1, 0, 0, 0), vram_rw(cb, 1, cp, 2, 0, 0, 0), vram_rw(cb, 1, cp, 3, 0, 0, 0));
        }
    }
    /* BT3_GS_ZDUMP=<frame>: when that frame first draws through the depth tint's table (CLUT 0x3E64), write the
       depth page (512 x 448 words: depth in the low 24 bits, the byte the effects index on top) to zdump.bin */
    if (kick && type == 6 && gs.vcount == 2 && getenv("BT3_GS_ZDUMP") != NULL && (int)sFrame == atoi(getenv("BT3_GS_ZDUMP")) &&
        ((gs.tex0[ctx] >> 37) & 0x3FFF) == 0x3E64) {
        static int done;
        if (!done) {
            FILE *f = fopen("zdump.bin", "wb");
            uint32_t x, y;
            done = 1;
            for (y = 0; y < 448; y++) {
                for (x = 0; x < 512; x++) {
                    uint32_t w = Gs_VramRead(0xE0 * 32, 8, 0, x, y);
                    fwrite(&w, 4, 1, f);
                }
            }
            fclose(f);
        }
    }
    switch (type) {
    case 0: if (kick) { if (sGpu) { GsGpu_Draw(0, ctx, &v); } else { pixel(sf, ctx, (int)v.x, (int)v.y, v.z, vcol(&v)); } } gs.vcount = 0; break;
    case 1: if (gs.vcount == 2) { if (kick) { if (sGpu) { GsGpu_Draw(1, ctx, gs.vtx); } else { draw_line(sf, ctx, &gs.vtx[0], &gs.vtx[1]); } } gs.vcount = 0; } break;
    case 2: if (gs.vcount == 2) { if (kick) { if (sGpu) { GsGpu_Draw(1, ctx, gs.vtx); } else { draw_line(sf, ctx, &gs.vtx[0], &gs.vtx[1]); } } gs.vtx[0] = gs.vtx[1]; gs.vcount = 1; } break;
    case 3: if (gs.vcount == 3) { if (kick) { if (sGpu) { GsGpu_Draw(3, ctx, gs.vtx); } else { draw_triangle(sf, ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]); } } gs.vcount = 0; } break;
    case 4: if (gs.vcount == 3 && kick) { if (sGpu) { GsGpu_Draw(3, ctx, gs.vtx); } else { draw_triangle(sf, ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]); } } break;
    case 5:
        if (gs.vcount == 3) {
            if (kick) { if (sGpu) { GsGpu_Draw(3, ctx, gs.vtx); } else { draw_triangle(sf, ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]); } }
            gs.vtx[1] = gs.vtx[2]; /* fan: keep the first vertex */
            gs.vcount = 2;
        }
        break;
    case 6:
        if (gs.vcount == 2) {
            /* A rectangle drawn into TEXTURE memory (a page the game uploads to, addressed as a 64-pixel-wide
               buffer): the game paints over its fighters' palettes this way, to give them all one alpha before the
               see-through pass. That has to happen in GS memory, where the GPU back end reads palettes from. */
            uint32_t fb = (uint32_t)(gs.frame[ctx] & 0x1FF);
            if (kick && sGpu && ((gs.frame[ctx] >> 16) & 0x3F) == 1 && gGsPageGen[fb] != 0) {
                draw_sprite(sf, ctx, &gs.vtx[0], &gs.vtx[1]);
                gGsPageGen[fb]++;
                gGsPageGen[(fb + 1) & 511]++;
            } else if (kick) {
                if (sGpu) { GsGpu_Draw(6, ctx, gs.vtx); } else { draw_sprite(sf, ctx, &gs.vtx[0], &gs.vtx[1]); }
            }
            gs.vcount = 0;
        }
        break;
    default: break;
    }
}

/* ------------------------------------------------------------------------------------------------ registers */

static void transfer_begin(void) {
    if (sDumpFrame == (int)sFrame && (gs.trxdir & 3) != 0) {
        fprintf(stderr, "  transfer dir %u: sbp %04x sbw %u spsm %02x -> dbp %04x dbw %u dpsm %02x, from %u,%u to %u,%u size %ux%u\n",
                (unsigned)(gs.trxdir & 3), (unsigned)(gs.bitbltbuf & 0x3FFF), (unsigned)((gs.bitbltbuf >> 16) & 0x3F),
                (unsigned)((gs.bitbltbuf >> 24) & 0x3F), (unsigned)((gs.bitbltbuf >> 32) & 0x3FFF), (unsigned)((gs.bitbltbuf >> 48) & 0x3F),
                (unsigned)((gs.bitbltbuf >> 56) & 0x3F), (unsigned)(gs.trxpos & 0x7FF), (unsigned)((gs.trxpos >> 16) & 0x7FF),
                (unsigned)((gs.trxpos >> 32) & 0x7FF), (unsigned)((gs.trxpos >> 48) & 0x7FF), (unsigned)(gs.trxreg & 0xFFF),
                (unsigned)((gs.trxreg >> 32) & 0xFFF));
    }
    gs.transfer = (gs.trxdir & 3) == 0; /* only host -> GS */
    gs.tx = (gs.trxpos >> 32) & 0x7FF;
    gs.ty = (gs.trxpos >> 48) & 0x7FF;
    gs.tw = gs.trxreg & 0xFFF;
    gs.th = (gs.trxreg >> 32) & 0xFFF;
    gs.tpos = 0;
    if (sDumpFrame == (int)sFrame) {
        fprintf(stderr, "  upload dbp %04x dbw %u psm %02x at %u,%u size %ux%u\n", (unsigned)((gs.bitbltbuf >> 32) & 0x3FFF),
                (unsigned)((gs.bitbltbuf >> 48) & 0x3F), (unsigned)((gs.bitbltbuf >> 56) & 0x3F), gs.tx, gs.ty, gs.tw, gs.th);
    }
}

static void transfer_data(const uint8_t *p, uint32_t bytes) {
    uint32_t dbp = (gs.bitbltbuf >> 32) & 0x3FFF, dbw = (gs.bitbltbuf >> 48) & 0x3F, dpsm = (gs.bitbltbuf >> 56) & 0x3F;
    uint32_t bits, total, i;

    if (!gs.transfer || gs.tw == 0) {
        return;
    }
    /* A picture uploaded straight into a display buffer (the movies: page 0, 512 wide, the second buffer 448 lines
       down): the GPU back end has to copy it from GS memory into that buffer's texture at the end of the frame. */
    if (dbp == 0 && dbw == 8 && Gs_PsmBits(dpsm) == 32) {
        unsigned bit = gs.ty < 448 ? 1u : 2u;
        if (!(gGsFbUploads & bit) && sGpu) {
            GsGpu_FbUpload(bit == 2); /* its place among the frame's draws: after the clear, before a fade */
        }
        gGsFbUploads |= bit;
    }
    bits = (uint32_t)Gs_PsmBits(dpsm);
    total = gs.tw * gs.th;
    /* The formats textures and palettes come in, a row at a time with the tables (the game uploads the stage's
       textures again and again in every frame: this loop was a quarter of the renderer's time). The same writes
       as vram_rw makes. */
    i = 0;
    if (dpsm == 0x00 || dpsm == 0x02 || dpsm == 0x0A || dpsm == 0x13 || dpsm == 0x14) {
        uint32_t bw = dbw ? dbw : 1, count = bytes * 8 / bits;
        uint8_t *vram8 = (uint8_t *)sVram; /* (little-endian hosts only, as everything here) */
        uint16_t *vram16 = (uint16_t *)sVram;
        while (i < count && gs.tpos < total) {
            uint32_t col = gs.tpos % gs.tw, y = gs.ty + gs.tpos / gs.tw, x = gs.tx + col, run = gs.tw - col, j;
            if (run > count - i) {
                run = count - i;
            }
            if (dpsm == 0x13) {
                const uint16_t *in = sIn8[y & 63];
                uint32_t base = (dbp + (y >> 6) * 32 * (bw >> 1)) << 8;
                for (j = 0; j < run; j++, x++) {
                    vram8[(base + ((x >> 7) << 13) + in[x & 127]) & 0x3FFFFF] = p[i + j];
                }
            } else if (dpsm == 0x14) {
                const uint16_t *in = sIn4[y & 127];
                uint32_t base = (dbp + (y >> 7) * 32 * (bw >> 1)) << 9;
                for (j = 0; j < run; j++, x++) {
                    uint32_t a = base + ((x >> 7) << 14) + in[x & 127], k = i + j;
                    uint8_t *b = &vram8[(a >> 1) & 0x3FFFFF];
                    uint32_t c = (p[k >> 1] >> ((k & 1) * 4)) & 15;
                    *b = (a & 1) ? (uint8_t)((*b & 0x0F) | c << 4) : (uint8_t)((*b & 0xF0) | c);
                }
            } else if (dpsm == 0x00) {
                const uint16_t *in = sIn32[y & 31];
                uint32_t base = (dbp + (y >> 5) * 32 * bw) << 6;
                for (j = 0; j < run; j++, x++) {
                    memcpy(&sVram[(base + ((x >> 6) << 11) + in[x & 63]) & 0xFFFFF], &p[(i + j) * 4], 4);
                }
            } else {
                const uint16_t *in = sIn16[dpsm == 0x0A][y & 63];
                uint32_t base = (dbp + (y >> 6) * 32 * bw) << 7;
                for (j = 0; j < run; j++, x++) {
                    memcpy(&vram16[(base + ((x >> 6) << 12) + in[x & 63]) & 0x1FFFFF], &p[(i + j) * 2], 2);
                }
            }
            i += run;
            gs.tpos += run;
        }
    }
    for (; i * bits < bytes * 8 && gs.tpos < total; gs.tpos++, i++) {
        uint32_t x = gs.tx + gs.tpos % gs.tw, y = gs.ty + gs.tpos / gs.tw, c;

        switch (bits) {
        case 32: c = p[i * 4] | p[i * 4 + 1] << 8 | p[i * 4 + 2] << 16 | (uint32_t)p[i * 4 + 3] << 24; break;
        case 24: c = p[i * 3] | p[i * 3 + 1] << 8 | p[i * 3 + 2] << 16; break;
        case 16: c = p[i * 2] | p[i * 2 + 1] << 8; break;
        case 8: c = p[i]; break;
        default: c = (p[i / 2] >> ((i & 1) * 4)) & 15; break;
        }
        vram_rw(dbp, dbw, dpsm, x, y, 1, c);
    }
    /* which pages changed: every page the rectangle can touch (pages are 8 KB = 32 blocks) */
    {
        uint32_t rowbytes = (dbw ? dbw : 1) * 64 * bits / 8, first = dbp / 32 + (gs.ty * rowbytes) / 8192;
        uint32_t last = dbp / 32 + ((gs.ty + gs.th) * rowbytes + 8191) / 8192;
        for (i = first; i <= last; i++) {
            gGsPageGen[i & 511]++;
        }
    }
}

/* PRMODECONT.AC = 1: the attributes (shading, texturing, fogging, blending, ...) are PRIM's own; AC = 0: they come
   from PRMODE and PRIM only gives the primitive type. The game switches to PRMODE for its shadow passes. */
static void prim_update(void) {
    gs.prim = (gs.prmodecont & 1) ? gs.primRaw : (gs.primRaw & 7) | (gs.prmode & 0x7F8);
}

static void reg_write(uint32_t addr, uint64_t d) {
    sStat[4]++;
    if (sDumpFrame == (int)sFrame && (addr == 0x4C || addr == 0x4D || addr == 0x18 || addr == 0x19 || addr == 0x40 || addr == 0x41 ||
                                       addr == 0x4E || addr == 0x4F || addr == 0x1A || addr == 0x1B)) {
        fprintf(stderr, "  reg %02x = %llx\n", addr, (unsigned long long)d);
    }
    switch (addr) {
    case 0x00: gs.primRaw = d; prim_update(); gs.vcount = 0; gs.strip = 0; break;
    case 0x01: gs.rgbaq = d; { uint32_t q = (uint32_t)(d >> 32); memcpy(&gs.q, &q, 4); } break;
    case 0x02: gs.st = d; break;
    case 0x03: gs.uv = d; break;
    case 0x04: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)((d >> 32) & 0xFFFFFF), 1); break;
    case 0x05: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)(d >> 32), 1); break;
    case 0x06: case 0x07: gs.tex0[addr - 6] = d; break;
    case 0x08: case 0x09: gs.clamp[addr - 8] = d; break;
    case 0x0C: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)((d >> 32) & 0xFFFFFF), 0); break;
    case 0x0D: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)(d >> 32), 0); break;
    case 0x14: case 0x15: gs.tex1[addr - 0x14] = d; break;
    case 0x16: case 0x17: gs.tex0[addr - 0x16] = (gs.tex0[addr - 0x16] & ~0x1FFFFFE003F00000ull) | (d & 0x1FFFFFE003F00000ull); break; /* TEX2: PSM and CLUT fields */
    case 0x18: case 0x19: gs.xyoffset[addr - 0x18] = d; break;
    case 0x1A: gs.prmodecont = d; prim_update(); break;
    case 0x1B: gs.prmode = d; prim_update(); break;
    case 0x3B: gs.texa = d; break;
    case 0x40: case 0x41: gs.scissor[addr - 0x40] = d; break;
    case 0x42: case 0x43: gs.alpha[addr - 0x42] = d; break;
    case 0x47: case 0x48: gs.test[addr - 0x47] = d; break;
    case 0x4A: case 0x4B: gs.fba[addr - 0x4A] = d & 1; break;
    case 0x4C: case 0x4D: gs.frame[addr - 0x4C] = d; break;
    case 0x4E: case 0x4F: gs.zbuf[addr - 0x4E] = d; break;
    case 0x50: gs.bitbltbuf = d; break;
    case 0x51: gs.trxpos = d; break;
    case 0x52: gs.trxreg = d; break;
    case 0x53: gs.trxdir = d; transfer_begin(); break;
    case 0x7F: if (sGpu) { GsGpu_Native((int)d); } break; /* not a GS register: the port's marker (gs_marker.c) */
    default: break;
    }
}

void Gs_RegWrite(uint32_t addr, uint64_t d) {
    reg_write(addr, d);
}

int GsGpu_Enabled(void) {
    return sGpu;
}

/* ------------------------------------------------------------------------------------------------ GIF */

/* One run of GIF data (whole quadwords). */
void Gs_Gif(const uint8_t *p, uint32_t qwc) {
    static uint64_t tag_lo, tag_hi; /* current tag: persists across runs */
    static uint32_t loops, reg;     /* loops left, register index within the loop */
    const uint64_t *q = (const uint64_t *)p;

    while (qwc != 0) {
        uint32_t flg, nreg;

        if (loops == 0) {
            sStat[3]++;
            tag_lo = q[0];
            tag_hi = q[1];
            q += 2;
            qwc--;
            loops = tag_lo & 0x7FFF;
            reg = 0;
            if (loops != 0 && ((tag_lo >> 46) & 1) && ((tag_lo >> 58) & 3) == 0) {
                reg_write(0, (tag_lo >> 47) & 0x7FF);
            }
            continue;
        }
        flg = (tag_lo >> 58) & 3;
        nreg = (tag_lo >> 60) & 15;
        if (nreg == 0) {
            nreg = 16;
        }
        if (flg == 0) { /* PACKED: one quadword per register */
            uint32_t r = (tag_hi >> (4 * reg)) & 15;
            uint64_t lo = q[0], hi = q[1];

            switch (r) {
            case 0x0: reg_write(0, lo & 0x7FF); break;
            case 0x1: reg_write(1, (lo & 0xFF) | ((lo >> 32) & 0xFF) << 8 | (hi & 0xFF) << 16 | ((hi >> 32) & 0xFF) << 24 |
                                   (uint64_t)*(uint32_t *)&gs.q << 32); break;
            case 0x2: gs.st = lo; { uint32_t qq = (uint32_t)hi; memcpy(&gs.q, &qq, 4); } break;
            case 0x3: reg_write(3, (lo & 0x3FFF) | ((lo >> 32) & 0x3FFF) << 16); break;
            case 0x4: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)((hi >> 4) & 0xFFFFFF), !((hi >> 47) & 1)); break;
            case 0x5: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)hi, !((hi >> 47) & 1)); break;
            case 0xC: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)((hi >> 4) & 0xFFFFFF), 0); break;
            case 0xD: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)hi, 0); break;
            case 0xE: reg_write((uint32_t)(hi & 0xFF), lo); break;
            case 0xF: break;
            default: reg_write(r, lo); break;
            }
            q += 2;
            qwc--;
            if (++reg == nreg) {
                reg = 0;
                loops--;
            }
        } else if (flg == 1) { /* REGLIST: 64 bits per register */
            int half;
            for (half = 0; half < 2 && loops != 0; half++) {
                uint32_t r = (tag_hi >> (4 * reg)) & 15;
                if (r != 0xE && r != 0xF) {
                    reg_write(r, q[half]);
                }
                if (++reg == nreg) {
                    reg = 0;
                    loops--;
                }
            }
            q += 2;
            qwc--;
        } else { /* IMAGE */
            uint32_t n = loops < qwc ? loops : qwc;
            sStat[6] += n;
            transfer_data((const uint8_t *)q, n * 16);
            q += 2 * n;
            qwc -= n;
            loops -= n;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ VIF1 */

/* One run of VIF data (32-bit words): GIF data (DIRECT) goes to the GS, everything else to VU1 (gs_vu1.c).
   A command's data may continue in the next run (the command word often sits in the DMA tag, its data behind):
   DIRECT data is passed on as it comes, the data of the other commands is collected first. */
static void vif(const uint32_t *w, uint32_t count) {
    static uint32_t direct;                    /* quadwords of GIF data still to come */
    static uint32_t pend, need, have, buf[2048]; /* a command waiting for `need` words of data */
    uint32_t i = 0;

    while (i < count) {
        uint32_t code, cmd, num, imm;

        if (direct != 0) {
            uint32_t qwc = (count - i) / 4 < direct ? (count - i) / 4 : direct;
            if (qwc == 0) {
                break; /* misaligned run: drop it */
            }
            sStat[2] += qwc;
            Gs_Gif((const uint8_t *)&w[i], qwc);
            i += qwc * 4;
            direct -= qwc;
            continue;
        }
        if (need != 0) {
            uint32_t n = count - i < need - have ? count - i : need - have;
            if (have + n <= 2048) {
                memcpy(&buf[have], &w[i], n * 4);
            }
            have += n;
            i += n;
            if (have == need) {
                uint32_t c = (pend >> 24) & 0x7F, pn = (pend >> 16) & 0xFF;
                if (need <= 2048) {
                    if (c == 0x20) { GsVu1_SetMask(buf[0]); }
                    else if (c == 0x30) { GsVu1_SetRow(buf); }
                    else if (c == 0x31) { GsVu1_SetCol(buf); }
                    else if (c == 0x4A) { GsVu1_Program(pend & 0xFFFF, buf, need); }
                    else { GsVu1_Unpack(c, pn, pend & 0xFFFF, buf); }
                }
                need = have = 0;
            }
            continue;
        }
        code = w[i++];
        cmd = (code >> 24) & 0x7F;
        num = (code >> 16) & 0xFF;
        imm = code & 0xFFFF;
        sStat[1]++;
        pend = code;
        if (sDumpFrame == (int)sFrame && cmd != 0 && cmd != 0x50) {
            fprintf(stderr, "  vif %02x num %3u imm %04x\n", cmd, num, imm);
        }
        switch (cmd) {
        case 0x01: GsVu1_SetCycle(imm & 0xFF, (imm >> 8) & 0xFF); break;
        case 0x02: GsVu1_SetOffset(imm); break;
        case 0x03: GsVu1_SetBase(imm); break;
        case 0x04: GsVu1_SetItop(imm); break;
        case 0x05: GsVu1_SetMode(imm); break;
        case 0x14: case 0x15: GsVu1_Call((int)imm); break;
        case 0x17: GsVu1_Call(-1); break;
        case 0x20: need = 1; break;
        case 0x30: case 0x31: need = 4; break;
        case 0x4A: need = (num ? num : 256) * 2; break;
        case 0x50: case 0x51: direct = imm ? imm : 65536; break;
        default:
            if (cmd >= 0x60) {
                need = GsVu1_UnpackWords(cmd, num);
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ DMA and frames */

static void screenshot(void) {
    static int every = -1;
    static uint64_t shown; /* FRAME register of the buffer that received the most pixels */
    Target *best = NULL;
    char name[64];
    FILE *fp;
    int i, x, y;

    if (every < 0) {
        every = getenv("BT3_SHOT") != NULL ? atoi(getenv("BT3_SHOT")) : 0;
    }
    {
        extern int gGsMainFbp; /* the frame's own buffer (sceGsSwapDBuff), else the one that received most pixels */
        for (i = 0; i < sTargetCount; i++) {
            if (gGsMainFbp >= 0 ? sTargets[i].fbp == (uint32_t)gGsMainFbp : (best == NULL || sTargets[i].drawn > best->drawn)) {
                best = &sTargets[i];
            }
        }
    }
    if (every > 0 && best != NULL && !sGpu && sFrame % (unsigned)every == 0) {
        uint64_t fr = (gs.frame[0] & 0x1FF) == best->fbp ? gs.frame[0] : (gs.frame[1] & 0x1FF) == best->fbp ? gs.frame[1] : shown;
        uint32_t fbw = (fr >> 16) & 0x3F, fpsm = (fr >> 24) & 0x3F;

        shown = fr;
        snprintf(name, sizeof(name), "port/build/shots/frame_%05u.ppm", sFrame);
        fp = fopen(name, "wb");
        if (fp != NULL) {
            fprintf(fp, "P6\n640 448\n255\n");
            for (y = 0; y < 448; y++) {
                for (x = 0; x < 640; x++) {
                    uint32_t c = x < (int)(fbw ? fbw : 10) * 64 ? vram_rw(best->fbp << 5, fbw ? fbw : 10, fpsm, (uint32_t)x, (uint32_t)y, 0, 0) : 0;
                    if (Gs_PsmBits(fpsm) == 16) {
                        c = (c & 31) << 3 | ((c >> 5) & 31) << 11 | ((c >> 10) & 31) << 19;
                    }
                    fwrite(&c, 1, 3, fp);
                }
            }
            fclose(fp);
        }
    }
    for (i = 0; i < sTargetCount; i++) {
        sTargets[i].drawn = 0;
    }
    if (!gPortResim) { /* (a frame that is only being re-run is not a frame of the picture) */
        sFrame++;
    }
}

/* ---- The render thread (GPU back end only).
   On the PS2 the frame's list is executed by DMA while the CPU already runs the next frame's game logic; the game
   is built for that (two list buffers, and it waits for the previous list before sending the next one). The PC
   build does the same: the game thread hands a finished list to this thread and goes on; anything else that
   touches the GS (uploads on the GIF channel, the per-frame clear) first waits until the thread is idle, which
   is the game's own sceGsSyncPath point. The window and all drawing belong to this thread. ---- */
static pthread_t sThread;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sWake = PTHREAD_COND_INITIALIZER, sDone = PTHREAD_COND_INITIALIZER;
static int sThreaded;
static int sJob;            /* 0 none, 1 start the GPU back end, 2 run a list */
static uint32_t sJobTadr;
static int sJobTte, sInitResult;
static void run_chain(uint32_t tadr, int tte);

/* BT3_GS_RACE=1 (with the render thread): finds data of a frame's list that the game changes while the list is
   being drawn. The list's blocks are hashed when the game hands the list over and again when the drawing is
   done; a block that differs is reported with its address. */
static struct { uint32_t tag, data, qwc, id; uint64_t hash; } sRace[2][8192];
static uint32_t sRaceCount[2];

static void race_walk(uint32_t tadr, int which) {
    uint32_t stack[2], sp = 0, n = 0;
    int guard;

    for (guard = 0; guard < 8192; guard++) {
        const uint32_t *tag = (const uint32_t *)(uintptr_t)tadr, *data;
        uint32_t qwc = tag[0] & 0xFFFF, id = (tag[0] >> 28) & 7, addr = tag[1], i;
        uint64_t h = 1469598103934665603ull;
        int end = 0;

        switch (id) {
        case 0: data = (const uint32_t *)(uintptr_t)addr; end = 1; break;
        case 1: data = tag + 4; tadr += 16 + qwc * 16; break;
        case 2: data = tag + 4; tadr = addr; break;
        case 3: case 4: data = (const uint32_t *)(uintptr_t)addr; tadr += 16; break;
        case 5: data = tag + 4; if (sp < 2) { stack[sp++] = tadr + 16 + qwc * 16; } tadr = addr; break;
        case 6: data = tag + 4; if (sp > 0) { tadr = stack[--sp]; } else { end = 1; } break;
        default: data = tag + 4; end = 1; break;
        }
        for (i = 0; i < 4; i++) {
            h = (h ^ tag[i]) * 1099511628211ull;
        }
        for (i = 0; i < qwc * 4; i++) {
            h = (h ^ data[i]) * 1099511628211ull;
        }
        sRace[which][n].tag = (uint32_t)(uintptr_t)tag;
        sRace[which][n].data = (uint32_t)(uintptr_t)data;
        sRace[which][n].qwc = qwc;
        sRace[which][n].id = id;
        sRace[which][n].hash = h;
        n++;
        if (end) {
            break;
        }
    }
    sRaceCount[which] = n;
}

static void race_compare(void) {
    uint32_t i, shown = 0;
    if (sRaceCount[0] != sRaceCount[1]) {
        fprintf(stderr, "race: frame %u: the list had %u blocks when handed over and %u after drawing\n", sFrame, sRaceCount[0], sRaceCount[1]);
    }
    for (i = 0; i < sRaceCount[0] && i < sRaceCount[1] && shown < 6; i++) {
        if (sRace[0][i].hash != sRace[1][i].hash || sRace[0][i].tag != sRace[1][i].tag) {
            fprintf(stderr, "race: frame %u: block %u of %u changed while drawn: tag at %08x id %u, %u quadwords of data at %08x\n", sFrame, i,
                    sRaceCount[0], sRace[0][i].tag, sRace[0][i].id, sRace[0][i].qwc, sRace[0][i].data);
            shown++;
        }
    }
}

static void *render_thread(void *arg) {
    (void)arg;
    pthread_mutex_lock(&sLock);
    for (;;) {
        while (sJob == 0) {
            pthread_cond_wait(&sWake, &sLock);
        }
        if (sJob == 1) {
            sInitResult = GsGpu_Init();
        } else {
            uint32_t tadr = sJobTadr;
            int tte = sJobTte;
            pthread_mutex_unlock(&sLock);
            run_chain(tadr, tte);
            if (getenv("BT3_GS_RACE") != NULL) {
                race_walk(tadr, 1);
                race_compare();
            }
            pthread_mutex_lock(&sLock);
        }
        sJob = 0;
        pthread_cond_broadcast(&sDone);
    }
    return NULL;
}

/* Blocks until the render thread has nothing to do. */
static void render_wait(void) {
    pthread_mutex_lock(&sLock);
    while (sJob != 0) {
        pthread_cond_wait(&sDone, &sLock);
    }
    pthread_mutex_unlock(&sLock);
}

static void render_post(int job, uint32_t tadr, int tte) {
    pthread_mutex_lock(&sLock);
    sJob = job;
    sJobTadr = tadr;
    sJobTte = tte;
    pthread_cond_signal(&sWake);
    pthread_mutex_unlock(&sLock);
}

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* getenv for the renderer's files (gs_internal.h): remembered per name. The names are string literals, so the
   same call site always passes the same pointer and the search is a few pointer comparisons. */
#undef getenv
const char *Port_GetEnv(const char *name) {
    static struct { const char *name, *value; } known[256];
    static volatile int count;
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    const char *value;
    int i, n = count;

    for (i = 0; i < n; i++) {
        if (known[i].name == name) {
            return known[i].value;
        }
    }
    value = getenv(name);
    pthread_mutex_lock(&lock);
    if (count < 256) {
        known[count].name = name;
        known[count].value = value;
        count++; /* published last: a reader never sees a half-written entry */
    }
    pthread_mutex_unlock(&lock);
    return value;
}
#define getenv(name) Port_GetEnv(name)

/* Which renderer: BT3_GS = "gpu" (the window), "1" (the software reference), "none" (nothing is drawn: tests).
   Without the variable a release program opens its window, so that it can be started by a double click; a
   developer's build draws nothing, as the checks expect. */
static const char *gs_mode(void) {
    extern volatile uint32_t gPortDataStripped; /* plat_mem.c: 1 in a release program */
    const char *m = getenv("BT3_GS");

    if (m == NULL) {
        return gPortDataStripped == 1 ? "gpu" : NULL;
    }
    return strcmp(m, "none") == 0 || strcmp(m, "off") == 0 || strcmp(m, "0") == 0 ? NULL : m;
}

static int gs_on(void) {
    static int on = -1;

    if (on < 0) {
        on = gs_mode() != NULL;
        tables_init();
        gs.prmodecont = 1; /* the GS starts with the attributes in PRIM */
        sDrawFrom = getenv("BT3_GS_FROM") != NULL ? atoi(getenv("BT3_GS_FROM")) : 0;
        sDumpFrame = getenv("BT3_GS_DUMP") != NULL ? atoi(getenv("BT3_GS_DUMP")) : -1;
        if (getenv("BT3_GS_PROBE") != NULL) {
            sscanf(getenv("BT3_GS_PROBE"), "%d,%d", &sProbeX, &sProbeY);
        }
        if (on && strcmp(gs_mode(), "gpu") == 0) {
            /* BT3_GS_THREAD=1: the back end and the list run on the render thread (experimental: the game was seen
               to change data a list still refers to, which shows as a jumping camera). Default: this thread. */
            sThreaded = getenv("BT3_GS_THREAD") != NULL;
            if (sThreaded) {
                pthread_create(&sThread, NULL, render_thread, NULL);
                render_post(1, 0, 0);
                render_wait();
                sGpu = sInitResult;
            } else {
                sGpu = GsGpu_Init();
            }
        }
    }
    return on;
}

/* A transfer on the GIF channel itself (the game uses it for texture uploads outside the frame's list):
   `qwc` quadwords of GIF data at `addr`, or with chain = 1 a source chain of them starting at `addr`. */
void Port_GsGifChannel(uint32_t addr, uint32_t qwc, int chain) {
    int guard;

    if (!gs_on()) {
        return;
    }
    if (sThreaded) {
        render_wait(); /* the GS is one device: this transfer goes after the list that is being drawn */
    }
    if (!chain) {
        Gs_Gif((const uint8_t *)(uintptr_t)addr, qwc);
        return;
    }
    for (guard = 0; guard < 100000; guard++) {
        const uint32_t *tag = (const uint32_t *)(uintptr_t)addr;
        uint32_t n = tag[0] & 0xFFFF, id = (tag[0] >> 28) & 7, ref = tag[1];

        if (id == 0 || id == 3 || id == 4) {
            Gs_Gif((const uint8_t *)(uintptr_t)ref, n);
            addr += 16;
        } else {
            Gs_Gif((const uint8_t *)(tag + 4), n);
            addr = id == 2 ? ref : addr + 16 + n * 16;
        }
        if (id == 0 || id == 7) {
            break;
        }
    }
}

/* Carries out a source-chain transfer on VIF1 that starts at `tadr` (PS2 address = host address). */
void Port_GsVif1Chain(uint32_t tadr, int tte) {
    static uint64_t next, start, sum, worst, sumGame, sumList, sumTex, sumPipe, sumEnd, perMin = ~0ull, perMax;
    static unsigned n, over;
    uint64_t t, tChain;

    if (!gs_on()) {
        return;
    }
    if (!sGpu) {
        run_chain(tadr, tte);
        return;
    }
    tChain = now_ns();
    if (sThreaded) {
        render_wait(); /* the previous list must be finished before this one starts (the PS2's sceGsSyncPath) */
    } else {
        run_chain(tadr, tte);
    }
    /* The frame's work ends here. Time it (BT3_GS_VERBOSE), then keep 30 frames per second. */
    t = now_ns();
    if (start != 0) {
        extern unsigned long long gPortSleptNs;
        uint64_t waited = (uint64_t)gPortSleptNs;
        uint64_t work = t - start > waited ? t - start - waited : 0; /* without the waiting for the vertical blank */
        gPortSleptNs = 0;
        {   /* a frame over budget: where the time went and what was created in it */
            extern unsigned gGpuNewTex, gGpuNewTexPixels, gGpuNewPipes;
            extern uint64_t gGpuTexNs, gGpuPipeNs, gGpuEndNs;
            if (work > 33366700ull && getenv("BT3_GS_VERBOSE") != NULL) {
                fprintf(stderr, "slow: frame %u: %.1f ms = game %.1f ms + display list %.1f ms; %u new textures (%u pixels), %u new pipelines\n",
                        sFrame, (double)work / 1e6, (double)(tChain - start) / 1e6, (double)(t - tChain) / 1e6, gGpuNewTex, gGpuNewTexPixels,
                        gGpuNewPipes);
                fprintf(stderr, "slow:   of the display list time: texture decoding %.1f ms, pipeline creation %.1f ms, submitting the frame %.1f ms\n",
                        (double)gGpuTexNs / 1e6, (double)gGpuPipeNs / 1e6, (double)gGpuEndNs / 1e6);
            }
            /* per second: where the work goes (the game's own code, walking the list, and of that the GPU side) */
            sumGame += tChain - start > waited ? tChain - start - waited : 0;
            sumList += t - tChain;
            sumTex += gGpuTexNs;
            sumPipe += gGpuPipeNs;
            sumEnd += gGpuEndNs;
            gGpuNewTex = gGpuNewTexPixels = gGpuNewPipes = 0;
            gGpuTexNs = gGpuPipeNs = gGpuEndNs = 0;
        }
        if (getenv("BT3_BURN_MS") != NULL) { /* testing: a slower machine, by burning time in every frame */
            uint64_t until = now_ns() + (uint64_t)atoi(getenv("BT3_BURN_MS")) * 1000000ull;
            while (now_ns() < until) {
            }
        }
        sum += work;
        if (work > worst) { worst = work; }
        if (work > 33366700ull) { over++; }
        if (++n == 60) {
            if (getenv("BT3_GS_VERBOSE") != NULL) {
                fprintf(stderr, "time: frame %u: work per frame: average %.1f ms, worst %.1f ms, %u of 60 over the 33.4 ms budget\n",
                        sFrame, (double)sum / 60e6, (double)worst / 1e6, over);
                fprintf(stderr, "time:   of the average: game code %.1f ms, display list %.1f ms (of which: texture decoding %.1f, "
                                "pipeline creation %.1f, ending and submitting the frame %.1f)\n",
                        (double)sumGame / 60e6, (double)sumList / 60e6, (double)sumTex / 60e6, (double)sumPipe / 60e6, (double)sumEnd / 60e6);
            }
            if (getenv("BT3_GS_VERBOSE") != NULL) {
                fprintf(stderr, "time:   frames lasted %.1f to %.1f ms (33.4 in a fight, 16.7 in the menus)\n", (double)perMin / 1e6, (double)perMax / 1e6);
            }
            n = 0; sum = 0; worst = 0; over = 0;
            sumGame = sumList = sumTex = sumPipe = sumEnd = 0;
            perMin = ~0ull;
            perMax = 0;
        }
    }
    /* pacing is done at the vertical blank (Port_VBlank in plat_stub.c): 60 per second, the game waits for one
       per menu frame and two per battle frame */
    (void)next;
    t = now_ns();
    if (start != 0) { /* the frame from start to start, waiting included */
        if (t - start < perMin) { perMin = t - start; }
        if (t - start > perMax) { perMax = t - start; }
    }
    start = t;
    if (sThreaded) {
        if (getenv("BT3_GS_RACE") != NULL) {
            race_walk(tadr, 0);
        }
        render_post(2, tadr, tte);
    }
}

/* Executes one frame's list: the DMA chain at `tadr` on VIF1, then the end-of-frame work of the back end. */
static void run_chain(uint32_t tadr, int tte) {
    uint32_t stack[2], sp = 0;
    int guard;

    for (guard = 0; guard < 100000; guard++) {
        const uint32_t *tag = (const uint32_t *)(uintptr_t)tadr;
        uint32_t qwc = tag[0] & 0xFFFF, id = (tag[0] >> 28) & 7, addr = tag[1]; /* a host pointer: all 32 bits */
        const uint32_t *data;
        int end = 0;

        switch (id) {
        case 0: data = (const uint32_t *)(uintptr_t)addr; end = 1; break;                 /* REFE */
        case 1: data = tag + 4; tadr += 16 + qwc * 16; break;                             /* CNT */
        case 2: data = tag + 4; tadr = addr; break;                                       /* NEXT */
        case 3: case 4: data = (const uint32_t *)(uintptr_t)addr; tadr += 16; break;      /* REF, REFS */
        case 5: data = tag + 4; if (sp < 2) { stack[sp++] = tadr + 16 + qwc * 16; } tadr = addr; break; /* CALL */
        case 6: data = tag + 4; if (sp > 0) { tadr = stack[--sp]; } else { end = 1; } break;          /* RET */
        default: data = tag + 4; end = 1; break;                                          /* END */
        }
        if (sDumpFrame == (int)sFrame) {
            fprintf(stderr, "  dma tag at %08x: id %u qwc %u addr %08x | %08x %08x (depth %u)\n", (unsigned)(uintptr_t)tag, id, qwc, addr, tag[2], tag[3], sp);
        }
        sStat[0]++;
        if (tte) {
            vif(tag + 2, 2);
        }
        vif(data, qwc * 4);
        if (end) {
            break;
        }
    }
    if (getenv("BT3_GS_VERBOSE") != NULL && (sFrame < 4 || sFrame % 60 == 0)) {
        fprintf(stderr, "gs: frame %u: %u DMA tags, %u VIF codes, %u DIRECT qwords, %u GIF tags, %u register writes, %u vertices, "
                        "%u image qwords, %d targets\n", sFrame, sStat[0], sStat[1], sStat[2], sStat[3], sStat[4], sStat[5],
                sStat[6], sTargetCount);
    }
    if (sDumpFrame == (int)sFrame) {
        static const int pt[5][2] = {{360, 330}, {362, 331}, {100, 100}, {400, 250}, {364, 300}};
        int k;
        for (k = 0; k < 5; k++) {
            fprintf(stderr, "  end of frame at (%d,%d): frame %08x, depth memory %08x\n", pt[k][0], pt[k][1],
                    vram_rw(0, 8, 0, (uint32_t)pt[k][0], (uint32_t)pt[k][1], 0, 0), vram_rw(0x1C00, 8, 0x30, (uint32_t)pt[k][0], (uint32_t)pt[k][1], 0, 0));
        }
    }
    if (sDumpFrame == (int)sFrame) {
        fprintf(stderr, "  pixels: %u outside scissor, %u failed alpha test, %u failed depth test, %u written\n", sPix[0], sPix[1], sPix[2], sPix[3]);
    }
    memset(sPix, 0, sizeof(sPix));
    memset(sStat, 0, sizeof(sStat));
    GsVu1_FrameEnd();
    if (sGpu) {
        GsGpu_FrameEnd();
    }
    screenshot();
    if (sMissingTex != 0 && getenv("BT3_GS_VERBOSE") != NULL) {
        fprintf(stderr, "gs: frame %u: %u texel reads from textures that were never uploaded\n", sFrame, sMissingTex);
    }
    sMissingTex = 0;
}
