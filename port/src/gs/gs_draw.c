/*
 * Graphics Synthesizer, the capture side of the GPU back ends (gs_gpu.c: Vulkan, gs_gl.c: OpenGL).
 *
 * gs_core.c still decodes what the game sends and keeps GS memory; this file takes over at "a primitive with
 * the current GS registers": it records every primitive of the frame as a GsDraw (what to draw, where, with
 * which texture and which pipeline), decodes GS memory into ordinary RGBA textures and keeps them cached.
 * The back end replays the list at the end of the frame.
 *   render targets   one per GS frame-buffer address in use, GS_W x GS_H GS pixels at SCALE times the PS2's
 *                    resolution; the shared table holds the GS state, each back end its attachments
 *   textures         GS memory decoded to ordinary RGBA textures, cached by TEX0 / TEXA and the upload
 *                    generation of the pages they occupy; a frame buffer used as a texture is sampled directly
 *   blending         the GS equation ((A - B) * C >> 7) + D mapped onto blend factors, as a key of pure GS
 *                    state bits the back end turns into its own blend state
 *   alpha            1.0 = 0x80, so that "source alpha" blending needs no extra scaling
 * Not handled yet: a target sampled while it is being drawn to, region clamp / repeat, destination-alpha tests,
 * frame-buffer masks, 16-bit targets' precision.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL3/SDL.h>   /* the one window both back ends share */
#include "gs_internal.h"
#include "gs_draw.h"
#include "ui.h"         /* PortVideo and the window helpers */
#include "gs_texpack.h" /* texture packs (replacement textures) */
#include <math.h>

/* Stage-name overlay: defined here (C) so both the renderer (gs_gpu.c) and the game's menu (menu_c_e.c) can
   reach them without C++ name mangling. Where they mean is in ui.h. */
volatile int gUiPresentX, gUiPresentY, gUiPresentW, gUiPresentH;
volatile int gUiNameX, gUiNameY, gUiNameW, gUiNameH;
volatile int gUiNameIdx = -1;
volatile int gUiNameReady;
volatile int gUiSongX, gUiSongY, gUiSongW, gUiSongH;
volatile int gUiSongIdx = -1;
volatile int gUiSongReady;
volatile int gUiSongLit; /* the added song's name is the one of the open music list (the game's second name style) */

extern int Port_Setting(const char *name, int def); /* plat_settings.c: the saved settings */
extern void Port_SettingSave(const char *name, int value);
extern int Port_IsWide(void);   /* plat_stub.c */
extern int Port_AspectMilli(void);
extern void Port_SetAspectMilli(int milli), Port_AudioRefresh(void), Port_SettingsWrite(void);
extern int gPortMenuMode;       /* headless.c: the menus are running */
extern int gPortMusicPercent, gPortSePercent;

static GsBackend *sBackend;

/* The one window both back ends share: shape (aspect), full screen and display are the renderer's, not a
   back end's, so the settings window and F11 work the same whichever back end is running. */
static SDL_Window *sWindow;
static int sFullscreen;
static float sWantAspect;
static int sDisplaySetting;

int gsScale4 = 8;
int gsTargetW = GS_W, gsTargetH = GS_H; /* the back end's render targets in GS pixels (the Vulkan one: 512) */
int gsPendingScale4;
GsTex gsWhite;
GsDraw *gsDraws;
uint32_t gsDrawCount;
Vtx *gsVerts;
uint32_t gsVertCount;
float *gsVuVerts;
uint32_t gsVuVertCount;
uint32_t *gsVuIdx;
uint32_t gsVuIdxCount;
Vu0Uniform *gsVuUni;
uint32_t gsVuUniCount;
GsTarget gsTargets[MAX_TARGETS];
int gsTargetCount;
int gsTexCount;
unsigned gsFxOff;
int gsGlowPercent = GLOW_DEFAULT;
int gsFilter;
static int sTexPackOn = 1; /* the setting: use the texture pack (if one was found) */
int gsAnchor;
int gsDepthByteIsFog;
unsigned gsNative;
unsigned gsSkipped;
unsigned gGpuNewTex, gGpuNewTexPixels, gGpuNewPipes; /* created since the front end last cleared them (slow-frame report) */
unsigned gGpuTexReplaced; /* textures taken from a texture pack so far */
uint64_t gGpuTexNs, gGpuPipeNs, gGpuEndNs; /* time spent decoding textures, creating pipelines, in GsGpu_FrameEnd */

static uint64_t gpu_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

/* GS memory -> an RGBA texture, cached by TEX0 / TEXA and the content of the pages it occupies. */
typedef struct Tex {
    uint64_t tex0, texa;
    uint32_t gen;
    GsTex tex;
    unsigned last; /* frame last used */
    int replaced;  /* from a texture pack: several times the size, so always filtered (texture_get) */
    uint32_t ow, oh, rw, rh; /* then: the original's size and the replacement's */
    uint32_t amax;           /* and the largest alpha byte of the original */
} Tex;
static Tex sTex[MAX_TEX];
static uint16_t sTexBucket[TEX_BUCKETS]; /* index + 1 of a cache entry, 0 = empty (open addressing) */
static Tex *sLast; /* the entry the previous lookup returned */

/* The 256-colour tables as textures, by hash of the palette entries they use (64 slots). */
static struct { uint32_t hash, last; GsTex tex; } sCluts[MAX_CLUTS];

static uint32_t tex_bucket(uint64_t t0, uint64_t texa, uint32_t gen) {
    uint64_t h = (t0 ^ (texa * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)gen << 17)) * 0xD6E8FEB86659FD93ull;
    return (uint32_t)(h >> 40) & (TEX_BUCKETS - 1);
}

/* A hash of the palette entries a texture uses (16 or 256 at `cbp`), remembered until one of the palette's pages is
   written again. The page hashes are too coarse here: a page holds up to 32 palettes, and the game rewrites some
   of them every frame (lit palettes, the fighters' palettes painted for the see-through pass), which made every
   texture with a palette in the same page count as new and be decoded again, hundreds per frame on some stages. */
static uint32_t clut_hash(uint32_t cbp, uint32_t cpsm, uint32_t count) {
    static struct { uint32_t key, gen0, gen1, hash; } memo[1024];
    uint32_t key = cbp << 5 | cpsm << 1 | (count == 256), slot = (key * 2654435761u) >> 22, h = 2166136261u, i;
    uint32_t g0 = gGsPageGen[(cbp / 32) & 511], g1 = gGsPageGen[(cbp / 32 + 1) & 511];

    if (memo[slot].key == key + 1 && memo[slot].gen0 == g0 && memo[slot].gen1 == g1) {
        return memo[slot].hash;
    }
    for (i = 0; i < count; i++) {
        h = (h ^ Gs_VramRead(cbp, 1, cpsm, i & 15, i >> 4)) * 16777619u;
    }
    memo[slot].key = key + 1;
    memo[slot].gen0 = g0;
    memo[slot].gen1 = g1;
    memo[slot].hash = h;
    return h;
}

/* GS memory -> an RGBA texture for the current TEX0 (alpha as stored: 0x80 is opaque). The back end makes
   the texture and takes the decoded pixels (freed after it has uploaded them). */
static GsTex texture_get(int ctx) {
    uint64_t t0 = gGs.tex0[ctx] & 0x1FFFFFFFFFFFFFFFull, texa = gGs.texa;
    uint32_t tbp = t0 & 0x3FFF, tbw = (t0 >> 14) & 0x3F, psm = (t0 >> 20) & 0x3F, tw = 1u << ((t0 >> 26) & 15), th = 1u << ((t0 >> 30) & 15);
    uint32_t cbp = (t0 >> 37) & 0x3FFF, cpsm = (t0 >> 51) & 15;
    uint32_t bits = (uint32_t)Gs_PsmBits(psm), gen = 0, pages, i, x, y, *px;
    Tex *t;
    uint64_t tex_t0;
    TexPackImage pack;
    uint32_t packMaxAlpha = 255, rw = tw, rh = th;
    int packFormat = GS_TEXFMT_RGBA8, replaced = 0;

    if (tw > 1024) { tw = 1024; }
    if (th > 1024) { th = 1024; }
    pages = ((tbw ? tbw : 1) * 64 * th * (bits == 24 ? 32 : bits) / 8 + 8191) / 8192;
    /* `gen` identifies the CONTENT: a hash over the pages the texture and its palette occupy */
    for (i = 0; i <= pages; i++) {
        gen = (gen ^ Gs_PageHash(tbp / 32 + i)) * 16777619u + i;
    }
    if (bits <= 8) {
        gen = (gen ^ clut_hash(cbp, cpsm, bits == 8 ? 256 : 16)) * 16777619u;
    }
    if (sTexPackOn && TexPack_Count() != 0) {
        gen ^= 0x5BD1E995u; /* with the pack and without it are different textures: switching it takes effect at once */
    }
    if (sLast != NULL && sLast->tex0 == t0 && sLast->texa == texa && sLast->gen == gen) {
        sLast->last = gGsFrame;
        return sLast->tex; /* the common case: the same texture as the previous primitive */
    }
    {
        uint32_t h = tex_bucket(t0, texa, gen);
        while (sTexBucket[h] != 0) {
            Tex *c = &sTex[sTexBucket[h] - 1];
            if (c->tex0 == t0 && c->texa == texa && c->gen == gen) {
                c->last = gGsFrame;
                sLast = c;
                return c->tex;
            }
            h = (h + 1) & (TEX_BUCKETS - 1);
        }
    }
    if (sBackend->texSlotsLeft() <= 0) {
        return gsWhite;
    }
    if (gsTexCount == MAX_TEX) {
        /* full: drop everything not used for two seconds (or, failing that, not used this frame) in one sweep */
        unsigned keep = gGsFrame > 120 ? gGsFrame - 120 : 0;
        int pass, n;
        for (pass = 0; pass < 2 && gsTexCount == MAX_TEX; pass++, keep = gGsFrame) {
            for (n = 0, i = 0; i < (uint32_t)gsTexCount; i++) {
                if (sTex[i].last >= keep) {
                    sTex[n++] = sTex[i];
                } else {
                    sBackend->texDestroy(sTex[i].tex);
                }
            }
            gsTexCount = n;
        }
        sLast = NULL;
        memset(sTexBucket, 0, sizeof(sTexBucket));
        for (i = 0; i < (uint32_t)gsTexCount; i++) {
            uint32_t h = tex_bucket(sTex[i].tex0, sTex[i].texa, sTex[i].gen);
            while (sTexBucket[h] != 0) {
                h = (h + 1) & (TEX_BUCKETS - 1);
            }
            sTexBucket[h] = (uint16_t)(i + 1);
        }
        if (gsTexCount == MAX_TEX) {
            return gsWhite; /* this one frame uses more textures than the cache holds */
        }
    }
    tex_t0 = gpu_now();
    /* a texture pack's replacement for this texture, if there is one (gs_texpack.c) */
    if (sTexPackOn && TexPack_Count() != 0) {
        const char *path = TexPack_Lookup(tbp, tbw, psm, tw, th, (uint32_t)((t0 >> 34) & 1), cbp, cpsm, &packMaxAlpha);
        if (path != NULL && TexPack_Load(path, &pack)) {
            packFormat = pack.format & 3;
            if (sBackend->texSlotsLeft() >= pack.levels) {
                replaced = 1;
                gGpuTexReplaced++;
                rw = pack.w;
                rh = pack.h;
            } else {
                TexPack_Free(&pack);
            }
        }
    }
    px = replaced ? NULL : malloc((size_t)tw * th * 4);
    for (y = 0; !replaced && y < th; y++) {
        for (x = 0; x < tw; x++) {
            uint32_t c = Gs_VramRead(tbp, tbw, psm, x, y), a;
            if (bits > 8) {
                c = Gs_Expand(c, psm);
            } else if (bits == 8) {
                c = (c & 0xE7) | ((c & 8) << 1) | ((c & 0x10) >> 1);
                c = Gs_Expand(Gs_VramRead(cbp, 1, cpsm, c & 15, c >> 4), cpsm);
            } else {
                c = Gs_Expand(Gs_VramRead(cbp, 1, cpsm, c & 7, c >> 3), cpsm);
            }
            (void)a; /* alpha is kept as the GS has it (0x80 = opaque, up to 0xFF): the shader rescales it. Values
                        above 0x80 matter: the see-through pass gives palettes alpha 0xF9 to write an id */
            px[y * tw + x] = c;
        }
    }
    gGpuTexNs += gpu_now() - tex_t0;
    t = &sTex[gsTexCount];
    t->tex = replaced ? sBackend->texCreate(rw, rh, packFormat, pack.levels) : sBackend->texCreate(tw, th, GS_TEXFMT_RGBA8, 1);
    if (t->tex == 0) {
        free(px);
        if (replaced) {
            TexPack_Free(&pack);
        }
        return gsWhite;
    }
    if (replaced) {
        int level;
        for (level = 0; level < pack.levels; level++) {
            uint32_t lw = pack.w >> level ? pack.w >> level : 1, lh = pack.h >> level ? pack.h >> level : 1;
            void *buf = malloc(pack.bytes[level]);
            memcpy(buf, pack.data[level], pack.bytes[level]);
            sBackend->texUpload(t->tex, level, lw, lh, packFormat, buf, pack.bytes[level]);
        }
        TexPack_Free(&pack);
    } else {
        sBackend->texUpload(t->tex, 0, tw, th, GS_TEXFMT_RGBA8, px, tw * th * 4);
        if (getenv("BT3_TEX_DUMP") != NULL) { /* every texture as it is decoded, raw RGBA with a small header (looking at the game's art) */
            static unsigned n;
            char name[600];
            FILE *df;
            snprintf(name, sizeof(name), "%s/tex_%05u_f%06u_%ux%u.rgba", getenv("BT3_TEX_DUMP"), n++, gGsFrame, (unsigned)tw, (unsigned)th);
            df = fopen(name, "wb");
            if (df != NULL) {
                fwrite(px, 1, (size_t)tw * th * 4, df);
                fclose(df);
            }
        }
    }
    gsTexCount++;
    {
        uint32_t h = tex_bucket(t0, texa, gen);
        while (sTexBucket[h] != 0) {
            h = (h + 1) & (TEX_BUCKETS - 1);
        }
        sTexBucket[h] = (uint16_t)gsTexCount;
    }
    gGpuNewTex++;
    gGpuNewTexPixels += tw * th;
    sLast = t;
    t->tex0 = t0;
    t->texa = texa;
    t->gen = gen;
    t->last = gGsFrame;
    t->replaced = replaced;
    t->ow = tw;
    t->oh = th;
    t->rw = rw;
    t->rh = rh;
    t->amax = replaced ? packMaxAlpha : 255u;
    return t->tex;
}

/* A 256-colour table at `cbp` (32-bit, the GS's index order undone) as a 256 x 1 texture; alpha as stored. */
static GsTex clut_texture(uint32_t cbp) {
    uint32_t px[256], hash = 2166136261u, i;
    GsTex tex;
    uint32_t *upload;
    int k, old = 0;

    for (i = 0; i < 256; i++) {
        uint32_t n = (i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1);
        px[i] = Gs_VramRead(cbp, 1, 0, n & 15, n >> 4);
        hash = (hash ^ px[i]) * 16777619u;
    }
    for (k = 0; k < MAX_CLUTS; k++) {
        if (sCluts[k].tex != 0 && sCluts[k].hash == hash) {
            sCluts[k].last = gGsFrame;
            return sCluts[k].tex;
        }
        if (sCluts[k].tex == 0 || sCluts[k].last < sCluts[old].last) {
            old = sCluts[k].tex == 0 && sCluts[old].tex == 0 ? old : k;
        }
    }
    if (sBackend->texSlotsLeft() <= 0 || (sCluts[old].tex != 0 && sCluts[old].last == gGsFrame)) {
        return 0;
    }
    if (sCluts[old].tex != 0) {
        sBackend->texDestroy(sCluts[old].tex);
    }
    upload = malloc(sizeof(px));
    memcpy(upload, px, sizeof(px));
    tex = sBackend->texCreate(256, 1, GS_TEXFMT_RGBA8, 1);
    if (tex == 0) {
        free(upload);
        return 0;
    }
    sBackend->texUpload(tex, 0, 256, 1, GS_TEXFMT_RGBA8, upload, sizeof(px));
    sCluts[old].tex = tex;
    sCluts[old].hash = hash;
    sCluts[old].last = gGsFrame;
    return tex;
}

/* ((A - B) * C >> 7) + D as the pipeline key's blend bits: ks * Cs + kd * Cd with ks, kd from
   {0, 1, C, -C, 1 - C, 1 + C}. Each of A, B, D is source, destination or zero, so bit i of A/B decides
   which side is scaled. Returns the key of pure GS state (bits 0..8); the back end turns it back into
   blend factors. k[i]: 0 nothing, 1 one, 2 C, 3 -C, 4 1 - C, 5 1 + C; i = 0 source, 1 destination. */
static uint32_t blend_key(uint64_t alpha, int abe) {
    int A = alpha & 3, B = (alpha >> 2) & 3, C = (alpha >> 4) & 3, D = (alpha >> 6) & 3, k[2], i;

    if (!abe) {
        return 0;
    }
    for (i = 0; i < 2; i++) {
        int c = (A == i) - (B == i), one = D == i;
        k[i] = c == 0 ? one : c > 0 ? (one ? 5 : 2) : (one ? 4 : 3);
    }
    return 1u | (uint32_t)k[0] << 1 | (uint32_t)k[1] << 4 | (uint32_t)C << 7;
}

/* The pipeline a draw uses: everything a pipeline depends on is in the key (the back end makes and caches
   one pipeline per key, so this is pure GS state -> an index):
   bits 0..8 blend, 10..11 depth test, 12 depth write, 13..14 topology, 16..19 colour write mask,
   20..21 vertices: 0 GS vertices, 1 program 0, 2 program 4, 3 program 6, 22 full-screen table pass,
   23 the draw never discards a pixel (no alpha test, no destination alpha test): the fragment shader without
   `discard`, which lets tile-based GPUs (Mali, Adreno) drop hidden pixels before shading them. */
static int pipeline_get(int ctx, int topo, int vu, int nodiscard) {
    uint64_t test = gGs.test[ctx], zb = gGs.zbuf[ctx];
    int zte = (test >> 16) & 1, ztst = (test >> 17) & 3, zwrite = !((zb >> 32) & 1);
    uint32_t bkey, key, wmask, m = (uint32_t)(gGs.frame[ctx] >> 32);

    bkey = blend_key(gGs.alpha[ctx], (int)((gGs.prim >> 6) & 1));
    if (!zte) {
        ztst = 1;
    }
    /* FRAME.FBMSK, in the whole-channel forms ordinary drawing uses (alpha only, everything but alpha, ...):
       a channel whose eight mask bits are all set is not written. Partial masks are not representable. */
    wmask = ((m & 0xFF) != 0xFF ? GS_CC_R : 0) | ((m & 0xFF00) != 0xFF00 ? GS_CC_G : 0) |
            ((m & 0xFF0000) != 0xFF0000 ? GS_CC_B : 0) | ((m & 0xFF000000u) != 0xFF000000u ? GS_CC_A : 0);
    key = bkey | (uint32_t)ztst << 10 | (uint32_t)zwrite << 12 | (uint32_t)topo << 13 | wmask << 16 | (uint32_t)vu << 20 |
          (nodiscard ? 1u << 23 : 0);
    if (vu == 4) { /* asked for by depth_clut: the table pass, which only depends on blending and the write mask */
        key = bkey | wmask << 16 | 1u << 22;
    }
    return sBackend->pipeGet(key);
}

/* The shared target table's entry for a frame-buffer address. The attachments are the back end's: a new
   slot asks it to make them (GsBackend.targetEnsure), an evicted one to release them (targetDrop). */
int GsDraw_TargetGet(uint32_t fbp, int create) {
    int i, slot;

    for (i = 0; i < gsTargetCount; i++) {
        if (gsTargets[i].fbp == fbp) {
            gsTargets[i].last = gGsFrame;
            return i;
        }
    }
    if (!create) {
        return -1;
    }
    slot = gsTargetCount;
    if (gsTargetCount == MAX_TARGETS) {
        /* Every buffer address the game has ever drawn to holds three large textures. The menus and each stage
           use their own set, so one that has not been touched for two seconds gives its slot to the new one. */
        slot = -1;
        for (i = 0; i < gsTargetCount; i++) {
            if (gsTargets[i].last + 120 < gGsFrame && (slot < 0 || gsTargets[i].last < gsTargets[slot].last)) {
                slot = i;
            }
        }
        if (slot < 0) {
            return -1;
        }
        sBackend->targetDrop(slot);
    }
    sBackend->targetEnsure(slot);
    gsTargets[slot].fbp = fbp;
    gsTargets[slot].cleared = 0;
    gsTargets[slot].draws = 0;
    gsTargets[slot].stale = 0;
    gsTargets[slot].gen = 0;
    gsTargets[slot].last = gGsFrame;
    if (slot == gsTargetCount) {
        gsTargetCount++;
    }
    return slot;
}

static int pipeline_get(int ctx, int topo, int vu, int nodiscard);

/* GfxPost_DrawDepthClut as one full-screen pass (the game sends 16 strips; they become one draw). */
static void depth_clut(int ctx) {
    uint64_t sc = gGs.scissor[ctx];
    uint32_t wm = (uint32_t)(gGs.frame[ctx] >> 32);
    GsDraw d, *last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;

    if (gsDrawCount == MAX_DRAWS) {
        return;
    }
    memset(&d, 0, sizeof(d));
    d.native = 3;
    d.src = gsDepthByteIsFog ? 0 : 1;
    d.target = GsDraw_TargetGet((uint32_t)(gGs.frame[ctx] & 0x1FF), 0);
    /* switched off by the user: the passes that colour the picture (the ones that only write alpha feed the glow) */
    if (d.target < 0 || ((gsFxOff & 4) && d.src == 0 && wm != 0x00FFFFFFu)) {
        return;
    }
    {
        uint32_t cbp = (uint32_t)((gGs.tex0[ctx] >> 37) & 0x3FFF);
        if (cbp == 0x3E8C) {
            return; /* the outline's own passes: the outline is drawn natively at its marker (outline.frag) */
        }
        if (cbp == 0x3E94 && (gsFxOff & 2)) {
            return; /* the see-through tint, switched off */
        }
    }
    d.tex = clut_texture((uint32_t)((gGs.tex0[ctx] >> 37) & 0x3FFF));
    if (d.tex == 0) {
        return;
    }
    d.pipeline = pipeline_get(ctx, 0, 4, 0);
    d.blendc = (float)((gGs.alpha[ctx] >> 32) & 0xFF) / 128.0f;
    d.misc[0] = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    d.scissor[0] = (int)(sc & 0x7FF) * SCALE;
    d.scissor[1] = (int)((sc >> 32) & 0x7FF) * SCALE;
    d.scissor[2] = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d.scissor[0];
    d.scissor[3] = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d.scissor[1];
    if (last != NULL && last->native == 3 && last->tex == d.tex && last->target == d.target && last->pipeline == d.pipeline && last->src == d.src) {
        return; /* the next strip of the same pass */
    }
    gsDraws[gsDrawCount++] = d;
}

static int draw_state(int ctx, int topo, int sprite, int vu, GsDraw *d, float *us, float *vs) {
    uint64_t prim = gGs.prim, t0 = gGs.tex0[ctx], test = gGs.test[ctx], cl = gGs.clamp[ctx], sc = gGs.scissor[ctx];
    int tme = (prim >> 4) & 1, src;
    float tw = (float)(1u << ((t0 >> 26) & 15)), th = (float)(1u << ((t0 >> 30) & 15));

    *us = *vs = 1.0f;
    /* Passes that only make sense on the PS2's memory layout are not drawn here: they belong to full-screen
       effects that get native versions (see docs/port/README.md). Dropped:
         - drawing through a 16-bit view of the frame buffer (the "channel shuffle" between halves of a pixel),
         - drawing INTO the depth buffer's memory as if it were a picture,
         - sampling the depth buffer's memory, or the top byte of a buffer as an 8-bit index (PSMT8H / T4HL / T4HH). */
    {
        uint32_t fpsm = (uint32_t)((gGs.frame[ctx] >> 24) & 0x3F), fbp = (uint32_t)(gGs.frame[ctx] & 0x1FF), zbp = (uint32_t)(gGs.zbuf[ctx] & 0x1FF);
        uint32_t tpsm = (uint32_t)((t0 >> 20) & 0x3F), tbp = (uint32_t)(t0 & 0x3FFF);
        if (Gs_PsmBits(fpsm) != 16 && fbp == zbp && tme) {
            /* The two passes that fill the depth page's spare byte: from the frame's top byte through the fog
               ramp (GfxDepthFog_Draw), or a plain copy of the frame's alpha (GfxPost_CopyAlphaToDepth). */
            gsDepthByteIsFog = tpsm == 0x1B;
            if (tpsm != 0x1B) { /* the copy of the ids: taken now, read by the passes that follow */
                GsDraw c, *last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
                memset(&c, 0, sizeof(c));
                c.native = 4;
                c.target = GsDraw_TargetGet(tbp / 32, 0);
                if (c.target >= 0 && gsDrawCount < MAX_DRAWS && !(last != NULL && last->native == 4)) {
                    gsDraws[gsDrawCount++] = c;
                }
            }
        }
        if (Gs_PsmBits(fpsm) == 16 || fbp == zbp) {
            if (fbp != zbp && fbp != (uint32_t)gGsMainFbp) {
                /* A work buffer drawn through a 16-bit view (the outline's edge image): not drawable here, but
                   the game has now written the buffer, so whatever samples it next means this buffer and not the
                   stage textures that share its address. Without this the outline's later passes read those
                   textures as its mask whenever nothing else had drawn into the buffer since the last upload
                   (split screen: blocks of noise over the picture). */
                int t = GsDraw_TargetGet(fbp, 1);
                if (t >= 0) {
                    gsTargets[t].stale = 1;
                    gsTargets[t].cleared = 1;
                    gsTargets[t].gen = gGsPageGen[fbp & 511];
                }
            }
            gsSkipped++;
            return 0;
        }
        if (tme && sprite && tpsm == 0x1B && tbp / 32 == zbp) {
            depth_clut(ctx); /* that byte through a table over the screen: a native pass */
            gsSkipped++;
            return 0;
        }
        if (tme && ((tpsm & 0x30) == 0x30 || tbp / 32 == zbp || tpsm == 0x1B || tpsm == 0x24 || tpsm == 0x2C)) {
            int t = GsDraw_TargetGet(fbp, 0);
            if (t >= 0 && fbp != (uint32_t)gGsMainFbp) {
                gsTargets[t].stale = 1; /* a work buffer missed a pass */
            }
            gsSkipped++;
            return 0;
        }
    }
    memset(d, 0, sizeof(*d));
    d->target = GsDraw_TargetGet((uint32_t)(gGs.frame[ctx] & 0x1FF), 1);
    if (d->target < 0) {
        return 0;
    }
    d->tex = gsWhite;
    if (!tme && sprite) {
        gsTargets[d->target].stale = 0; /* cleared by the game: what is drawn into it from here on is real */
    }
    if (tme) {
        src = GsDraw_TargetGet((uint32_t)((t0 & 0x3FFF) / 32), 0);
        /* a frame buffer used as a texture: only if nothing was uploaded over it since it was drawn */
        if (src >= 0 && (t0 & 0x1F) == 0 && gsTargets[src].cleared && gsTargets[src].gen == gGsPageGen[gsTargets[src].fbp & 511]) {
            uint32_t tp = (uint32_t)((t0 >> 20) & 0x3F);
            if (gsTargets[src].stale) {
                /* The buffer was meant to hold a copy or a step of an effect that was dropped; what it holds now
                   is whatever was drawn there before (seen as blocks of noise over the picture). */
                gsSkipped++;
                return 0;
            }
            /* One buffer drawn into another is how the game builds its screen effects: the glare and the object
               glow (the picture shrunk by its alpha into the 256 x 256 work buffer, down to 64 x 64, blurred
               between two buffers, added back), the blur by depth, the pan blur, the haze. They are ordinary
               textured sprites and strips and are drawn as such. Not drawable: a buffer sampled while it is the
               target, and a buffer read in another pixel format than it was drawn in (the 16-bit views). */
            /* The user's switches act on the pass that puts a work buffer back over the picture: added (the
               glare and the object glow, blend "source + destination") or mixed in by the destination's alpha
               (the blur of distant things). */
            if (sprite && (uint32_t)(gGs.frame[ctx] & 0x1FF) == (uint32_t)gGsMainFbp && ((prim >> 6) & 1) &&
                ((((gGs.alpha[ctx] & 0xFF) == 0x68) && (gsFxOff & 8)) || (((gGs.alpha[ctx] & 0xFF) == 0x54) && (gsFxOff & 16)))) {
                gsSkipped++;
                return 0;
            }
            if (src == d->target || tp > 1) {
                if (d->target != src && (uint32_t)(gGs.frame[ctx] & 0x1FF) != (uint32_t)gGsMainFbp) {
                    gsTargets[d->target].stale = 1; /* a work buffer missed a pass */
                }
                gsSkipped++;
                return 0;
            }
            d->tex = sBackend->targetColor(src);
            d->tex_is_target = 1;
            {
                /* The buffer is a corner of a larger texture here, so the GS's own edge handling has to be done
                   by hand: CLAMP gives the texel range per axis (0 repeat and 1 clamp: the texture's size; 2 and
                   3: the region MINU..MAXU). Sampling is held inside it, or the blur steps pull in whatever lies
                   next to the buffer (seen as a box with an edge in the glare). */
                int wms = (int)(cl & 3), wmt = (int)((cl >> 2) & 3);
                float u0 = 0.0f, u1 = tw - 1.0f, v0 = 0.0f, v1 = th - 1.0f;
                if (wms >= 2) { u0 = (float)((cl >> 4) & 0x3FF); u1 = (float)((cl >> 14) & 0x3FF); }
                if (wmt >= 2) { v0 = (float)((cl >> 24) & 0x3FF); v1 = (float)((cl >> 34) & 0x3FF); }
                d->rect[0] = (u0 + 0.5f) / (float)gsTargetW;
                d->rect[1] = (v0 + 0.5f) / (float)gsTargetH;
                d->rect[2] = (u1 + 0.5f) / (float)gsTargetW;
                d->rect[3] = (v1 + 0.5f) / (float)gsTargetH;
            }
            *us = tw / (float)gsTargetW;
            *vs = th / (float)gsTargetH;
        } else {
            d->tex = texture_get(ctx);
        }
    }
    d->sampler = (int)((gGs.tex1[ctx] >> 5) & 1) | ((cl & 3) ? 2 : 0) | (((cl >> 2) & 3) ? 4 : 0);
    /* A replacement is several times the original's size: with the nearest-texel sampling the game asks for on
       its text and 2D art (right for a texture drawn 1:1) it is shown smaller than it is, and its edges come out
       jagged. Replacements are always sampled with filtering. */
    if (d->tex != 0 && sLast != NULL && sLast->tex == d->tex && sLast->replaced) {
        d->sampler |= 1 | 8; /* (GsGpu_Draw takes the 8 off again for 2D art) */
    }
    d->mode[0] = !tme ? 0 : d->tex_is_target ? 2 : 1; /* 2: a frame buffer as texture, its alpha is already rescaled */
    /* (BT3_TEX_ALPHA=0 switches the replacement's alpha treatment off, for telling which one a fault comes from) */
    if (d->mode[0] == 1 && sLast != NULL && sLast->tex == d->tex && sLast->replaced &&
        !(getenv("BT3_TEX_ALPHA") != NULL && atoi(getenv("BT3_TEX_ALPHA")) == 0)) {
        d->mode[0] = 3; /* a texture pack's replacement: the alpha test allows for its filtered, compressed alpha (gs.frag) */
        d->orig[2] = (float)sLast->amax / 128.0f; /* its alpha is kept at or below the original's largest (1.0 = 0x80) */
    }
    d->mode[1] = (int32_t)((t0 >> 35) & 3);
    d->mode[2] = (int32_t)((t0 >> 34) & 1);
    d->mode[3] = (test & 1) && ((test >> 12) & 3) == 0 ? (int32_t)((test >> 1) & 7) + 1 : 0;
    d->misc[0] = (float)((test >> 4) & 0xFF);
    d->misc[1] = ((test >> 14) & 1) ? (float)(1 + (int)((test >> 15) & 1)) : 0.0f; /* DATE, DATM */
    d->misc[2] = (float)(gGs.fba[ctx] & 1);
    {   /* alpha test off or "always", no destination alpha test: nothing this draw draws is ever discarded
            (BT3_GPU_DISCARD=1: the shader with discard for every draw, as before) */
        static int always = -1;
        if (always < 0) {
            always = getenv("BT3_GPU_DISCARD") != NULL;
        }
        d->pipeline = pipeline_get(ctx, topo, vu, !always && (d->mode[3] == 0 || d->mode[3] == 2) && d->misc[1] == 0.0f);
    }
    d->blendc = (float)((gGs.alpha[ctx] >> 32) & 0xFF) / 128.0f;
    d->scissor[0] = (int)(sc & 0x7FF) * SCALE;
    d->scissor[1] = (int)((sc >> 32) & 0x7FF) * SCALE;
    d->scissor[2] = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d->scissor[0];
    d->scissor[3] = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d->scissor[1];
    return 1;
}

static void put(const GsVertex *v, float x, float y, float s, float t, float q, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float zmax) {
    Vtx *o = &gsVerts[gsVertCount++];
    o->x = x;
    o->y = y;
    o->z = (float)((double)v->z / zmax);
    /* A Z above the depth format's largest value (text and menus are sent at 0xFFFFFFFF with a 24-bit buffer)
       lands past 1.0: a GPU with depth clamping clamps it, one without (Mali) clips the primitive away, and the
       post-fight menu and the memory card messages lost their letters. Clamped here for every GPU. */
    if (o->z > 1.0f) {
        o->z = 1.0f;
    }
    o->r = r; o->g = g; o->b = b; o->a = a;
    o->s = s; o->t = t; o->q = q;
}

/* Two draws can be merged into one call when everything they need is the same. `scissor` is compared as
   four ints (the scissor a draw has recorded). */
static int same_state(const GsDraw *a, const GsDraw *b) {
    return !a->native && a->vu == b->vu && a->target == b->target && a->tex == b->tex && a->sampler == b->sampler && a->pipeline == b->pipeline &&
           memcmp(a->mode, b->mode, sizeof(a->mode)) == 0 && a->misc[0] == b->misc[0] && a->misc[1] == b->misc[1] &&
           a->misc[2] == b->misc[2] && a->misc[3] == b->misc[3] && a->blendc == b->blendc &&
           memcmp(a->rect, b->rect, sizeof(a->rect)) == 0 && memcmp(a->orig, b->orig, sizeof(a->orig)) == 0 &&
           memcmp(a->scissor, b->scissor, sizeof(a->scissor)) == 0;
}

/* A draw that tests the alpha already in the frame buffer (TEST.DATE: the fill of a HUD bar cut to length by a
   mask, a word shown through a band, and also some of the stage's own layers): at the start of a run of such
   draws, a copy of the alpha bytes is taken for them to read. Called by every way a primitive can be recorded;
   left out of the vertex-program paths at first, the stage's layers tested against the copy the HUD had made the
   frame before, and showed a strip of different-looking scenery where an announcement's band had been. */
static void date_snapshot(const GsDraw *d) {
    GsDraw *prev = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
    GsDraw c;

    if (d->misc[1] == 0.0f || (prev != NULL && !prev->native && prev->misc[1] != 0.0f && prev->target == d->target)) {
        return;
    }
    memset(&c, 0, sizeof(c));
    c.native = 5;
    c.target = d->target;
    if (gsDrawCount + 1 < MAX_DRAWS) {
        gsDraws[gsDrawCount++] = c;
    }
}

/* Gs_StateBack (gs_core.c) moved every page's upload generation on by `delta`, so that nothing decoded from the
   pages in between is taken for them. The buffers the game draws into keep their standing: one that was drawn
   since the last upload over it still is. */
void GsGpu_PagesMoved(uint32_t delta) {
    int i;
    for (i = 0; i < gsTargetCount; i++) {
        if (gsTargets[i].gen != 0) { /* (0: its page was never uploaded to, and stays 0) */
            gsTargets[i].gen += delta;
        }
    }
}

void GsGpu_Draw(int type, int ctx, const GsVertex *v) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    uint64_t prim = gGs.prim, t0 = gGs.tex0[ctx];
    int fst = (prim >> 8) & 1, gouraud = (prim >> 3) & 1, n = type == 6 ? 2 : type == 3 ? 3 : type == 1 ? 2 : 1;
    float tw = (float)(1u << ((t0 >> 26) & 15)), th = (float)(1u << ((t0 >> 30) & 15)), us, vs;
    float zmax = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    float s[3], t[3], q[3];
    const GsVertex *flat = &v[n - 1];
    GsDraw d, *last;
    int i, packed2d = 0;

    if (gsVertCount + 6 > MAX_VERTS || gsDrawCount == MAX_DRAWS) {
        return;
    }
    if (!draw_state(ctx, type == 1 ? 1 : type == 0 ? 2 : 0, type == 6, 0, &d, &us, &vs)) {
        return;
    }
    if (type == 6 || fst) {
        d.sampler &= 7; /* 2D art: never the smaller copies of a replacement */
    }
    if ((type == 6 || fst) && !d.tex_is_target) {
        /* 2D art: sprites, and triangles with whole-texel coordinates (the logo, HUD pieces drawn as quads):
           texture coordinates per GS pixel (gs.frag). Not for a texture pack's replacement: that has several
           texels per GS pixel, and sampling it once per GS pixel would show it at the original's resolution. */
        if (sLast != NULL && sLast->tex == d.tex && sLast->replaced && !(getenv("BT3_TEX_2D") != NULL && atoi(getenv("BT3_TEX_2D")) == 0)) {
            packed2d = 1; /* its rectangle is worked out below, from the vertices */
        } else {
            d.misc[3] = (float)SCALE;
        }
    }
    date_snapshot(&d);
    for (i = 0; i < n; i++) {
        if (fst) {
            s[i] = (float)v[i].u / 16.0f / tw * us;
            t[i] = (float)v[i].v / 16.0f / th * vs;
            q[i] = 1.0f;
        } else {
            s[i] = v[i].s * us;
            t[i] = v[i].t * vs;
            q[i] = v[i].q != 0.0f ? v[i].q : 1.0f;
        }
    }
    if (packed2d) {
        /* A texture pack's replacement as 2D art: sampled per output pixel (it has several texels per original
           texel), but only inside the rectangle of the sheet this piece shows on the console: the original texels
           from the one its first GS pixel takes to the one its last GS pixel takes. Without the limit the
           filtering reaches the neighbouring picture of the sheet (lines along HUD panels). */
        int lo[2] = {0, 0}, hi[2] = {0, 0}, k, axis;
        for (axis = 0; axis < 2; axis++) {
            float size = axis ? th : tw, pmin = 0, pmax = 0, cmin = 0, cmax = 0, cAtMin = 0, cAtMax = 0, step, first, lastc;
            for (k = 0; k < n; k++) {
                /* (a sprite's coordinates are both divided by the SECOND vertex's Q, as below) */
                float pos = axis ? v[k].y : v[k].x, c = (axis ? t[k] : s[k]) / (type == 6 ? q[1] : q[k]) * size;
                if (k == 0 || pos < pmin) { pmin = pos; cAtMin = c; }
                if (k == 0 || pos > pmax) { pmax = pos; cAtMax = c; }
                if (k == 0 || c < cmin) { cmin = c; }
                if (k == 0 || c > cmax) { cmax = c; }
            }
            step = pmax > pmin ? (cmax - cmin) / (pmax - pmin) : 0.0f; /* texels per GS pixel */
            first = cAtMax >= cAtMin ? cmin : cmin + step;             /* lowest coordinate a GS pixel takes */
            lastc = cAtMax >= cAtMin ? cmax - step : cmax;             /* highest */
            lo[axis] = (int)floorf(first + 1.0f / 64.0f);
            /* A mirrored piece (coordinates falling along the screen) starts exactly ON the boundary to the next
               picture of the sheet (16.0 down to 8.0): one GS pixel's worth on the console, but the filtered
               replacement blends that neighbour in over a visible stretch when the piece is drawn enlarged
               (a strip of the yellow layer at the end of the second player's health bar). The boundary itself
               does not count as inside. */
            hi[axis] = (int)floorf(lastc + (cAtMax >= cAtMin ? 1.0f / 64.0f : -1.0f / 64.0f)) + 1;
            if (hi[axis] <= lo[axis]) {
                hi[axis] = lo[axis] + 1;
            }
        }
        d.misc[3] = -(float)SCALE;
        d.rect[0] = (float)lo[0] / tw;
        d.rect[1] = (float)lo[1] / th;
        d.rect[2] = (float)hi[0] / tw;
        d.rect[3] = (float)hi[1] / th;
        d.orig[0] = tw;
        d.orig[1] = th;
    }
    d.first = gsVertCount;
    if (type == 6) { /* sprite: two corners, flat colour and depth of the second vertex */
        const GsVertex *a = &v[0], *b = &v[1];
        put(b, a->x, a->y, s[0], t[0], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, b->x, a->y, s[1], t[0], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, a->x, b->y, s[0], t[1], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, b->x, a->y, s[1], t[0], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, b->x, b->y, s[1], t[1], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, a->x, b->y, s[0], t[1], 1.0f, b->r, b->g, b->b, b->a, zmax);
        if (!fst) { /* a sprite's texture coordinates are not divided by Q per pixel: do it here */
            for (i = 0; i < 6; i++) {
                gsVerts[d.first + i].s /= q[1];
                gsVerts[d.first + i].t /= q[1];
            }
        }
    } else {
        for (i = 0; i < n; i++) {
            const GsVertex *c = gouraud ? &v[i] : flat;
            put(&v[i], v[i].x, v[i].y, s[i], t[i], q[i], c->r, c->g, c->b, c->a, zmax);
        }
    }
    if (d.tex_is_target && type == 6 && v[1].x != v[0].x && v[1].y != v[0].y) {
        /* The GS evaluates texture coordinates at whole pixel positions, a GPU at pixel centres, half a pixel
           later. For one buffer drawn into another that half pixel is not cosmetic: the blur steps of the glow
           chain sample one texel apart on purpose (each step should average two texels and move the picture
           back and forth by half a texel); evaluated at pixel centres they do not blur and move it a whole texel
           every round, which showed as a displaced copy of the scenery in the sky. The coordinates are taken
           half a pixel back instead. (Moving the sprite itself left its first row and column undrawn: a line
           of the wrong brightness along the top of the picture.) */
        float ds = 0.5f * (gsVerts[d.first + 1].s - gsVerts[d.first].s) / (v[1].x - v[0].x);
        float dt = 0.5f * (gsVerts[d.first + 2].t - gsVerts[d.first].t) / (v[1].y - v[0].y);
        uint32_t k;
        for (k = d.first; k < gsVertCount; k++) {
            gsVerts[k].s -= ds;
            gsVerts[k].t -= dt;
        }
        /* the user's strength for the glare and the glow: the pass that adds the blurred buffer to the picture */
        if (gsGlowPercent != 100 && (uint32_t)(gGs.frame[ctx] & 0x1FF) == (uint32_t)gGsMainFbp && ((gGs.prim >> 6) & 1) &&
            (gGs.alpha[ctx] & 0xFF) == 0x68) {
            for (k = d.first; k < gsVertCount; k++) {
                gsVerts[k].r = (uint8_t)(gsVerts[k].r * gsGlowPercent / 100 > 255 ? 255 : gsVerts[k].r * gsGlowPercent / 100);
                gsVerts[k].g = (uint8_t)(gsVerts[k].g * gsGlowPercent / 100 > 255 ? 255 : gsVerts[k].g * gsGlowPercent / 100);
                gsVerts[k].b = (uint8_t)(gsVerts[k].b * gsGlowPercent / 100 > 255 ? 255 : gsVerts[k].b * gsGlowPercent / 100);
            }
        }
    }
    if (Port_IsWide() && !gPortMenuMode && !d.tex_is_target && (type == 6 || fst) && gsTargets[d.target].fbp == (uint32_t)gGsMainFbp) {
        /* Widescreen. The 3D scene is projected for a 16:9 picture by the game itself; 2D art is laid out for
           4:3 and would come out a third too wide. Each 2D piece is narrowed to 3/4 about a fixed point:
             (not in the menus: their pages are shown whole, stretched to the width; see Port_WideFactor)
             - in a fight, the left edge, the right edge or the middle for the HUD's left panel, right panel and
               centre parts (markers from the HUD code), so the panels sit at the screen's edges;
             - any other 2D piece, the middle of the screen (see below).
           Only what is drawn into the picture itself: the work buffers of the effects and of the shadow are
           filled with 2D rectangles too, and narrowing those left unfilled bands and stripes in them.
           Left alone: full-screen fills and fades (untextured, full height), and in a fight sprites as wide as
           the screen (flashes, speed lines) and 2D triangles outside the HUD. */
        float x0 = gsVerts[d.first].x, x1 = x0, y0 = gsVerts[d.first].y, y1 = y0, pivot = -1.0f;
        uint32_t k;
        for (k = d.first; k < gsVertCount; k++) {
            if (gsVerts[k].x < x0) { x0 = gsVerts[k].x; }
            if (gsVerts[k].x > x1) { x1 = gsVerts[k].x; }
            if (gsVerts[k].y < y0) { y0 = gsVerts[k].y; }
            if (gsVerts[k].y > y1) { y1 = gsVerts[k].y; }
        }
        if (type == 6 && !d.mode[0] && y1 - y0 >= 400.0f) {
            pivot = -1.0f;
        } else if (x1 - x0 >= 480.0f) {
            /* as wide as the screen (the bands and streaks of READY / FIGHT, flashes, speed lines): meant to
               reach from edge to edge, so it keeps the full width */
            pivot = -1.0f;
        } else if (gsAnchor != 0) {
            pivot = gsAnchor == 1 ? 0.0f : gsAnchor == 2 ? 512.0f : 256.0f;
            if (gsAnchor == 4) { /* a part without a fixed side (captions: technique names): the side it is on */
                float cx = (x0 + x1) * 0.5f;
                pivot = x1 <= 300.0f || cx < 180.0f ? 0.0f : x0 >= 212.0f || cx > 332.0f ? 512.0f : 256.0f;
            }
        } else {
            /* 2D outside the HUD (the pause menu, messages): one page about the middle of the screen. (Narrowing
               each piece about its own middle pulled panels apart and spread the letters of the text.) A marker
               the game places over a fighter is pulled a little towards the middle by this: known, not handled.
               Sprites as wide as the screen (flashes, speed lines) are left alone. */
            pivot = 256.0f;
        }
        if (pivot >= 0.0f) {
            float narrow = 1333.333f / (float)Port_AspectMilli(); /* 3/4 at 16:9, 9/16 at 21:9 */
            for (k = d.first; k < gsVertCount; k++) {
                gsVerts[k].x = pivot + (gsVerts[k].x - pivot) * narrow;
            }
        }
    }
    d.count = gsVertCount - d.first;
    gsTargets[d.target].draws++;
    gsTargets[d.target].gen = gGsPageGen[gsTargets[d.target].fbp & 511];
    last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d)) {
        last->count += d.count; /* same state as the previous primitive: one draw call */
    } else {
        gsDraws[gsDrawCount++] = d;
    }
}

/* Vertex program 6 as a shader: one strip of the ground under a fighter, textured with the shadow page (see
   shaders/vu6.vert). Vertices: four quadwords each (position with an integer "no draw" flag in w, normal, colour
   as floats, a slot the original fills with s, t). A flagged vertex means: the triangle that ends here is not
   drawn (it only joins two real triangles of the strip). */
void GsGpu_DrawVu6(int ctx, const float *vertices, uint32_t count, const float *consts) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    Vu0Uniform u;
    GsDraw d, *last;
    float us, vs;
    uint32_t i, k, flag;

    if (count < 3 || gsVuVertCount + count > MAX_VU_VERTS || gsVuIdxCount + (count - 2) * 3 > MAX_VU_IDX || gsDrawCount == MAX_DRAWS ||
        gsVuUniCount == MAX_VU_UNIFORMS) {
        return;
    }
    if (!draw_state(ctx, 0, 0, 3, &d, &us, &vs)) {
        return;
    }
    date_snapshot(&d);
    memset(&u, 0, sizeof(u));
    memcpy(u.boneA, &consts[0x0C * 4], 64);  /* world -> shadow camera */
    u.pivotA[0] = consts[0x10 * 4];          /* texture scale */
    u.pivotB[0] = us;
    u.pivotB[1] = vs;
    memcpy(u.screen, &consts[0], 64);
    u.misc[0] = (float)(gGs.xyoffset[ctx] & 0xFFFF) / 16.0f;
    u.misc[1] = (float)((gGs.xyoffset[ctx] >> 32) & 0xFFFF) / 16.0f;
    u.misc[2] = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    d.vu = 3;
    d.first = gsVuIdxCount;
    for (i = 0; i + 2 < count; i++) { /* strip -> triangles, without the flagged ones */
        memcpy(&flag, &vertices[(i + 2) * 16 + 3], 4);
        if (flag & 0xFFFF) {
            continue;
        }
        for (k = 0; k < 3; k++) {
            gsVuIdx[gsVuIdxCount++] = gsVuVertCount + i + k;
        }
    }
    d.count = gsVuIdxCount - d.first;
    if (d.count == 0) {
        return;
    }
    for (i = 0; i < count; i++) {
        float *o = &gsVuVerts[gsVuVertCount++ * 12];
        memcpy(o, &vertices[i * 16], 16);          /* position */
        memcpy(o + 4, &vertices[i * 16 + 8], 16);  /* colour */
        memset(o + 8, 0, 16);
    }
    gsTargets[d.target].draws++;
    gsTargets[d.target].gen = gGsPageGen[gsTargets[d.target].fbp & 511];
    last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d) && memcmp(&gsVuUni[last->uniform], &u, sizeof(u)) == 0) {
        last->count += d.count;
        return;
    }
    d.uniform = (int)gsVuUniCount;
    gsVuUni[gsVuUniCount++] = u;
    gsDraws[gsDrawCount++] = d;
}

/* Vertex program 4 as a shader: one strip of stage geometry (see shaders/vu4.vert). Vertices: position, colour
   (0..255 floats), texture coordinates, 48 bytes; the screen matrix is VU memory 0..3. */
void GsGpu_DrawVu4(int ctx, const float *vertices, uint32_t count, const float *consts) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    Vu0Uniform u;
    GsDraw d, *last;
    float us, vs;
    uint32_t i, k;

    if (count < 3 || gsVuVertCount + count > MAX_VU_VERTS || gsVuIdxCount + (count - 2) * 3 > MAX_VU_IDX || gsDrawCount == MAX_DRAWS ||
        gsVuUniCount == MAX_VU_UNIFORMS) {
        return;
    }
    if (!draw_state(ctx, 0, 0, 2, &d, &us, &vs)) {
        return;
    }
    date_snapshot(&d);
    memset(&u, 0, sizeof(u));
    memcpy(u.screen, &consts[0], 64);
    u.misc[0] = (float)(gGs.xyoffset[ctx] & 0xFFFF) / 16.0f;
    u.misc[1] = (float)((gGs.xyoffset[ctx] >> 32) & 0xFFFF) / 16.0f;
    u.misc[2] = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    d.vu = 2;
    d.first = gsVuIdxCount;
    for (i = 0; i + 2 < count; i++) { /* strip -> triangles */
        for (k = 0; k < 3; k++) {
            gsVuIdx[gsVuIdxCount++] = gsVuVertCount + i + k;
        }
    }
    d.count = gsVuIdxCount - d.first;
    memcpy(&gsVuVerts[gsVuVertCount * 12], vertices, count * 48);
    gsVuVertCount += count;
    gsTargets[d.target].draws++;
    gsTargets[d.target].gen = gGsPageGen[gsTargets[d.target].fbp & 511];
    last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d) && memcmp(&gsVuUni[last->uniform], &u, sizeof(u)) == 0) {
        last->count += d.count;
        return;
    }
    d.uniform = (int)gsVuUniCount;
    gsVuUni[gsVuUniCount++] = u;
    gsDraws[gsDrawCount++] = d;
}

/* Vertex program 0 as a shader: one strip of the fighters' models (see shaders/vu0.vert and gs_vu1.c). The strip
   becomes a triangle list of the program's own 48-byte vertices; the constants go into a uniform block. */
void GsGpu_DrawVu0(int layer, int ctx, const float *vertices, uint32_t count, const float *consts) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    Vu0Uniform u;
    GsDraw d, *last;
    float us, vs;
    uint32_t i, k;

    if (count < 3 || gsVuVertCount + count > MAX_VU_VERTS || gsVuIdxCount + (count - 2) * 3 > MAX_VU_IDX || gsDrawCount == MAX_DRAWS ||
        gsVuUniCount == MAX_VU_UNIFORMS) {
        return;
    }
    if (!draw_state(ctx, 0, 0, 1, &d, &us, &vs)) {
        return;
    }
    date_snapshot(&d);
    memcpy(u.boneA, &consts[0], 64);
    memcpy(u.boneB, &consts[16], 64);
    memcpy(u.pivotA, &consts[32], 16);
    memcpy(u.pivotB, &consts[36], 16);
    memcpy(u.screen, &consts[56], 64);
    u.light[0] = consts[40]; u.light[1] = consts[44]; u.light[2] = consts[48]; u.light[3] = consts[52];
    memcpy(u.color0, &consts[88], 16);
    memcpy(u.color1, &consts[92], 16);
    u.misc[0] = (float)(gGs.xyoffset[ctx] & 0xFFFF) / 16.0f;
    u.misc[1] = (float)((gGs.xyoffset[ctx] >> 32) & 0xFFFF) / 16.0f;
    u.misc[2] = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    u.misc[3] = (float)layer;
    u.light2[0] = consts[41]; u.light2[1] = consts[45]; u.light2[2] = consts[49]; u.light2[3] = consts[53];
    d.vu = 1;
    d.first = gsVuIdxCount;
    for (i = 0; i + 2 < count; i++) { /* strip -> triangles */
        if (layer == 4) { /* debris: a flagged vertex does not complete a triangle */
            uint32_t flag;
            memcpy(&flag, &vertices[(i + 2) * 12 + 3], 4);
            if (flag & 0xFFFF) {
                continue;
            }
        }
        for (k = 0; k < 3; k++) {
            gsVuIdx[gsVuIdxCount++] = gsVuVertCount + i + k;
        }
    }
    d.count = gsVuIdxCount - d.first;
    memcpy(&gsVuVerts[gsVuVertCount * 12], vertices, count * 48);
    gsVuVertCount += count;
    gsTargets[d.target].draws++;
    gsTargets[d.target].gen = gGsPageGen[gsTargets[d.target].fbp & 511];
    last = gsDrawCount ? &gsDraws[gsDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d) && memcmp(&gsVuUni[last->uniform], &u, sizeof(u)) == 0) {
        last->count += d.count;
        return;
    }
    d.uniform = (int)gsVuUniCount;
    gsVuUni[gsVuUniCount++] = u;
    gsDraws[gsDrawCount++] = d;
}

/* The game uploaded pixels straight into a display buffer (a movie frame). Remembers the place in the frame's
   draw order; the pixels are taken from GS memory at the end of the frame (frame_end). */
void GsGpu_FbUpload(int second) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    GsDraw d;

    if (gsDrawCount == MAX_DRAWS) {
        return;
    }
    memset(&d, 0, sizeof(d));
    d.native = 100 + (second != 0);
    d.target = GsDraw_TargetGet(second ? 0x70 : 0, 1);
    if (d.target >= 0) {
        gsDraws[gsDrawCount++] = d;
    }
}

void GsGpu_Native(int effect) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    uint64_t sc = gGs.scissor[0];
    GsDraw d;

    /* 1 = outline. 2 (the see-through tint) is drawn by the generic table pass now (depth_clut), from the game's
       own table. */
    if (effect >= 0x10 && effect <= 0x14) {
        gsAnchor = effect == 0x13 ? 0 : effect == 0x14 ? 4 : effect - 0x0F; /* 1 left, 2 right, 3 centre, 4 by position */
        return;
    }
    if (effect != 1 || gsDrawCount == MAX_DRAWS || (gsFxOff & 1)) {
        return;
    }
    gsNative++;
    memset(&d, 0, sizeof(d));
    d.native = effect;
    d.target = GsDraw_TargetGet((uint32_t)(gGs.frame[0] & 0x1FF), 0);
    if (d.target < 0) {
        return;
    }
    d.scissor[0] = (int)(sc & 0x7FF) * SCALE;
    d.scissor[1] = (int)((sc >> 32) & 0x7FF) * SCALE;
    d.scissor[2] = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d.scissor[0];
    d.scissor[3] = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d.scissor[1];
    gsDraws[gsDrawCount++] = d;
}

/* A new resolution multiplier: every render target is dropped and made again at the new size when the game next
   draws to it. Buffers that carry something over from the previous frame start empty for one frame. */
static void scale_apply(void) {
    if (gsPendingScale4 == 0 || gsPendingScale4 == gsScale4) {
        gsPendingScale4 = 0;
        return;
    }
    gsScale4 = gsPendingScale4; /* the back end's attachments and copies are rebuilt to this size */
    gsPendingScale4 = 0;
    sBackend->scaleChanged();
    gsTargetCount = 0;
    fprintf(stderr, "bt3: internal resolution %gx (%d x %d)\n", (double)SCALE, (int)(512 * SCALE), (int)(448 * SCALE));
}

/* ------------------------------------------------------------------------------------------- the one window */
/* Created (with SDL's video subsystem) before the device / the GL context: everything the user can choose
   about it (BT3_WINDOW, BT3_DISPLAY, BT3_FULLSCREEN, the saved settings, the aspect ratio) is decided here,
   so both back ends show the same window. `opengl` requests a window a GL context can be made current on. */
SDL_Window *GsDraw_WindowCreate(int opengl) {
    extern int Port_AspectMilli(void);
    float want = (float)Port_AspectMilli() / 1000.0f;
    int h = 896, w = (int)((float)h * want + 0.5f), count = 0, pick = 0;
    SDL_DisplayID *displays;
    SDL_PropertiesID props;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "bt3: SDL_Init: %s\n", SDL_GetError());
        return NULL;
    }
    displays = SDL_GetDisplays(&count);
    props = SDL_CreateProperties();
    /* The window has the picture's shape: 896 lines high, 4:3 or 16:9 wide (1195 x 896 or 1593 x 896), and
       keeps that shape when it is resized. BT3_WINDOW=WxH gives another start size. */
    if (getenv("BT3_WINDOW") != NULL) {
        sscanf(getenv("BT3_WINDOW"), "%dx%d", &w, &h);
    }
    /* BT3_DISPLAY=n: the n-th display (1 = first) for the window or the full screen; without it the desktop
       places the window. Some desktops (Wayland) ignore a window's position but honour the display of a
       full-screen window. BT3_FULLSCREEN=1 starts in borderless full screen; F11 switches. */
    for (pick = 0; pick < count; pick++) {
        SDL_Rect r;
        SDL_GetDisplayBounds(displays[pick], &r);
        fprintf(stderr, "bt3: display %d: %s, %d x %d\n", pick + 1, SDL_GetDisplayName(displays[pick]), r.w, r.h);
    }
    sDisplaySetting = Port_Setting("display", 0);
    pick = getenv("BT3_DISPLAY") != NULL ? atoi(getenv("BT3_DISPLAY")) : sDisplaySetting;
    sFullscreen = (getenv("BT3_FULLSCREEN") != NULL ? atoi(getenv("BT3_FULLSCREEN")) : Port_Setting("fullscreen", 0)) != 0;
#ifdef __ANDROID__
    sFullscreen = 1; /* (the whole screen, without the system's bars) */
#endif
    sWantAspect = want;
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Tenkaichi3Decomp");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, w);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, h);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    if (opengl) {
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    }
    if (pick >= 1 && pick <= count) {
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(displays[pick - 1]));
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(displays[pick - 1]));
    } else if (pick != 0) {
        fprintf(stderr, "bt3: BT3_DISPLAY=%d: there are %d displays\n", pick, count);
    }
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, sFullscreen);
    sWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    SDL_free(displays);
    if (sWindow != NULL && !sFullscreen) {
        SDL_SetWindowSize(sWindow, (int)((float)896 * want + 0.5f), 896);
    }
    fprintf(stderr, "bt3: internal resolution %gx (%d x %d)\n", (double)SCALE, (int)(512 * SCALE), (int)(448 * SCALE));
    return sWindow;
}

SDL_Window *GsDraw_Window(void) { return sWindow; }

void GsDraw_WindowShape(void) {
    float want = (float)Port_AspectMilli() / 1000.0f;
    int w, h;
    sWantAspect = want;
    if (!sFullscreen && sWindow != NULL) {
        SDL_GetWindowSize(sWindow, &w, &h);
        SDL_SetWindowSize(sWindow, (int)((float)h * want + 0.5f), h);
    }
}

void GsDraw_FullscreenSet(int on) {
    if (sWindow == NULL) {
        return;
    }
    sFullscreen = on;
    SDL_SetWindowFullscreen(sWindow, on);
}

void GsDraw_FullscreenToggle(void) {
    GsDraw_FullscreenSet(!sFullscreen);
    Port_SettingSave("fullscreen", sFullscreen);
    Port_SettingsWrite();
}

/* ------------------------------------------------------------------------------------------- the settings window
   F1 opens it (ui.cpp, Dear ImGui). What the user changes there reaches the renderer through these two: the
   scale, the effects switched off and the glow are shared state; the shape, the full screen and the display
   are the window's. */
void GsGpu_GetSettings(PortVideo *v) {
    v->scale4 = gsPendingScale4 ? gsPendingScale4 : gsScale4;
    v->aspectMilli = Port_AspectMilli();
    v->fullscreen = sFullscreen;
    v->fxOff = (int)gsFxOff;
    v->glow = gsGlowPercent;
    v->filter = gsFilter;
    v->music = gPortMusicPercent;
    v->effects = gPortSePercent;
    v->display = sDisplaySetting;
    v->texPack = sTexPackOn;
    v->texPackCount = TexPack_Count();
}

/* Applies what differs from the current state, all of it at once, and keeps it for the next run. */
void GsGpu_SetSettings(const PortVideo *v) {
    PortVideo now;
    GsGpu_GetSettings(&now);
    if (v->scale4 != now.scale4) {
        gsPendingScale4 = v->scale4 < 4 ? 4 : v->scale4 > 32 ? 32 : v->scale4; /* applied between two frames */
    }
    if (v->aspectMilli != now.aspectMilli) {
        Port_SetAspectMilli(v->aspectMilli);
        GsDraw_WindowShape();
    }
    if (v->fullscreen != now.fullscreen) {
        GsDraw_FullscreenSet(v->fullscreen);
    }
    gsFxOff = (unsigned)v->fxOff & 31;
    gsGlowPercent = v->glow;
    gsFilter = v->filter < 0 || v->filter > 4 ? 0 : v->filter;
    gPortSePercent = v->effects;
    if (v->music != now.music) {
        gPortMusicPercent = v->music;
        Port_AudioRefresh();
    }
    sDisplaySetting = v->display;
    sTexPackOn = v->texPack != 0;
    Port_SettingSave("texture_pack", sTexPackOn);
    {   /* "scale4" in quarters; "scale" the whole multiplier, for the launcher of an earlier version */
        int q = gsPendingScale4 ? gsPendingScale4 : gsScale4;
        Port_SettingSave("scale4", q);
        Port_SettingSave("scale", (q + 2) / 4);
    }
    Port_SettingSave("aspect_milli", Port_AspectMilli());
    Port_SettingSave("fullscreen", sFullscreen);
    Port_SettingSave("fx_off", (int)gsFxOff);
    Port_SettingSave("glow", gsGlowPercent);
    Port_SettingSave("filter", gsFilter);
    Port_SettingSave("music", gPortMusicPercent);
    Port_SettingSave("effects", gPortSePercent);
    Port_SettingSave("display", sDisplaySetting);
    Port_SettingsWrite();
}

int GsGpu_Init(void) {
    const char *api = getenv("BT3_GPU_API");
    GsBackend *sb;

    /* BT3_SCALE=1.5 and the like; the setting "scale4" in quarters, else the older whole "scale" */
    if (getenv("BT3_SCALE") != NULL) {
        gsScale4 = (int)(atof(getenv("BT3_SCALE")) * 4.0 + 0.5);
    } else {
        gsScale4 = Port_Setting("scale4", 0);
        if (gsScale4 <= 0) {
            gsScale4 = Port_Setting("scale", 2) * 4;
        }
    }
    gsScale4 = gsScale4 < 4 ? 4 : gsScale4 > 32 ? 32 : gsScale4;
    gsPendingScale4 = 0;
    gsVerts = malloc(MAX_VERTS * sizeof(Vtx));
    gsVuVerts = malloc(MAX_VU_VERTS * 48);
    gsVuIdx = malloc(MAX_VU_IDX * sizeof(uint32_t));
    gsVuUni = malloc(MAX_VU_UNIFORMS * sizeof(Vu0Uniform));
    gsDraws = malloc(MAX_DRAWS * sizeof(GsDraw));
    /* BT3_GPU_API=vulkan|gl|auto picks the back end. Both are complete renderers of the same list, so a
       picture from either should be the same (see the frame-by-frame compare in the port's docs). */
    if (api == NULL || strcmp(api, "auto") == 0) {
        /* the settings window's choice (Renderer; Vulkan unless OpenGL was picked there) */
        sb = Port_Setting("gpu_api", 0) == 1 ? &sGlBackend : &sVulkanBackend;
        api = NULL;
    } else if (strcmp(api, "vk") == 0 || strcmp(api, "vulkan") == 0) {
        sb = &sVulkanBackend;
    } else if (strcmp(api, "gl") == 0 || strcmp(api, "opengl") == 0) {
        sb = &sGlBackend;
    } else {
        fprintf(stderr, "bt3: unknown BT3_GPU_API=%s (try vulkan, gl or auto)\n", api);
        return 0;
    }
#ifdef __ANDROID__
    /* Android: the OpenGL back end is desktop GL 3.3 core, which Android does not have (GLES only): Vulkan always */
    sb = &sVulkanBackend;
    api = "vulkan";
#endif
    sBackend = sb;
    if (!sb->init()) {
        /* The chosen renderer cannot start (no Vulkan driver, an old graphics card): the other one is tried,
           unless one was asked for by name. */
        GsBackend *other = sb == &sVulkanBackend ? &sGlBackend : &sVulkanBackend;
        sBackend = NULL;
        if (api != NULL) {
            return 0;
        }
        fprintf(stderr, "bt3: the %s renderer could not start; trying %s\n", sb == &sVulkanBackend ? "Vulkan" : "OpenGL",
                other == &sVulkanBackend ? "Vulkan" : "OpenGL");
        sb = other;
        sBackend = sb;
        if (!sb->init()) {
            sBackend = NULL;
            return 0;
        }
    }
    gsWhite = sb->whiteTex();
    gsFxOff = (unsigned)(getenv("BT3_FX_OFF") != NULL ? atoi(getenv("BT3_FX_OFF")) : Port_Setting("fx_off", 0)) & 31;
    gsGlowPercent = getenv("BT3_GLOW") != NULL ? atoi(getenv("BT3_GLOW")) : Port_Setting("glow", GLOW_DEFAULT);
    gsFilter = getenv("BT3_FILTER") != NULL ? atoi(getenv("BT3_FILTER")) : Port_Setting("filter", 0);
    gsFilter = gsFilter < 0 || gsFilter > 4 ? 0 : gsFilter;
    sTexPackOn = Port_Setting("texture_pack", 1) != 0;
    gPortMusicPercent = Port_Setting("music", 100);
    gPortSePercent = Port_Setting("effects", 100);
    TexPack_Init();
    return 1;
}

void GsGpu_FrameEnd(void) {
    if (gPortResim) { /* a frame that is only being re-run: nothing was recorded, nothing is shown */
        return;
    }
    uint64_t t0 = gpu_now();
    if (sBackend != NULL) {
        sBackend->frameEnd();
        scale_apply();
    }
    gGpuEndNs += gpu_now() - t0;
}
