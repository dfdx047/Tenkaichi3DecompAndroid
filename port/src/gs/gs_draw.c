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
#include "menu_text.h"  /* editable labels in the original menu texture slots */
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
extern unsigned long long Port_BlankNs(void); /* plat_stub.c: 16.68 ms, less in an online session at 120% */
extern int Port_AspectMilli(void);
extern void Port_SetAspectMilli(int milli), Port_AudioRefresh(void), Port_SettingsWrite(void);
extern int gPortMenuMode;       /* headless.c: the menus are running */
extern int Port_PageAspectMilli(void); /* plat_stub.c: the shape of the page shown now (4:3 in the menus if asked for) */
extern int gPortMusicPercent, gPortSePercent, gPortVoicePercent, gPortStagePercent;

static GsBackend *sBackend;
static int sSmooth2d = 1; /* the setting "2D filtering" (smooth_2d; BT3_2D_SMOOTH=0/1 wins): 1 smooth */
static int sWholeFrame; /* the draw being set up shows a frame the game read back (the cross-fade) */

/* The one window both back ends share: shape (aspect), full screen and display are the renderer's, not a
   back end's, so the settings window and F11 work the same whichever back end is running. */
static SDL_Window *sWindow;
/* Called once a frame by the back ends: when the check of the game data (plat_verify.c) has found it modified, the
   window's title says so from then on, so that a screenshot of the window carries it. */
void GsGpu_TitlePoll(void) {
    extern int Port_DataState(void);
    static int done;
    if (!done && sWindow != NULL && Port_DataState() == 2) {
        SDL_SetWindowTitle(sWindow, "Tenkaichi3Decomp [modified game data]");
        done = 1;
    }
}
static int sFullscreen;
static float sWantAspect;
static int sDisplaySetting;

int gsScale = 2;
int gsPendingScale;
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
static int sTexPackOn = 1; /* the setting: use the texture pack (if one was found) */
int gsAnchor;
/* A fight's HUD in a picture wider than 4:3 (setting hud_layout, BT3_HUD=<n> for a test):
     0  the side panels at the screen's edges (as since widescreen)
     1  the same, with the health bars drawn longer, up to the timer (hud_b.c, pull request #28)
     2  everything about the middle of the screen: the 4:3 layout as it is, with the scene wider around it */
static int sHudLayout;
static unsigned sHudFrame;
static int sHudSeen;
/* 1 = the HUD's side panels were drawn within the last few frames (a few: frames left out by frame skipping). */
int GsDraw_HudShown(void) {
    return sHudSeen && gGsFrame - sHudFrame <= 6;
}
int Port_HudLayout(void) {
    return sHudLayout;
}
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
    int replaced;  /* 1: texture pack, 2: vector menu labels, 3: packed shared glyphs */
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

/* Tenkaichi Budokai Online: its pictures (include/menu/tbo_art.h). The game draws one as a sprite whose TEX0 has the
   reserved format TBO_PSM and the picture's number in CBP (src/menu/port_tbo.c); it is read from tbo/ui/<name>.png
   next to the program the first time and kept, at the PNG's own size, so it is drawn at full resolution and in the
   game's own order. */
#include "../../../include/menu/tbo_art.h"
#define TBO_NAME(id, name) name,
static const char *const kTboNames[TBO_ART_COUNT] = {TBO_ART_LIST(TBO_NAME)};
#undef TBO_NAME
static GsTex sTboTex[TBO_ART_COUNT];
static unsigned char sTboMissing[TBO_ART_COUNT]; /* not found or not readable: not looked for again */

/* Makes a texture of RGBA pixels (the GS's alpha) with its smaller copies, each half the one before, averaged by
   2 x 2: these pictures are mostly shown smaller than they are (letters, the crest), and one sample of the full
   picture per pixel shimmers. Takes `px`; 0 if no texture could be had. */
static GsTex tbo_upload(uint32_t *px, uint32_t w, uint32_t h) {
    uint32_t *lv[12];
    uint32_t lw[12], lh[12], x, y;
    int levels = 1, k;
    GsTex tex;

    lv[0] = px;
    lw[0] = w;
    lh[0] = h;
    while (px != NULL && levels < 12 && (lw[levels - 1] > 1 || lh[levels - 1] > 1)) {
        uint32_t pw = lw[levels - 1], ph = lh[levels - 1], nw = pw > 1 ? pw / 2 : 1, nh = ph > 1 ? ph / 2 : 1;
        uint32_t *src = lv[levels - 1], *dst = malloc((size_t)nw * nh * 4);
        if (dst == NULL) {
            break;
        }
        for (y = 0; y < nh; y++) {
            for (x = 0; x < nw; x++) {
                uint32_t x0 = x * 2 < pw ? x * 2 : pw - 1, x1 = x * 2 + 1 < pw ? x * 2 + 1 : pw - 1;
                uint32_t y0 = y * 2 < ph ? y * 2 : ph - 1, y1 = y * 2 + 1 < ph ? y * 2 + 1 : ph - 1;
                uint32_t c[4] = {src[y0 * pw + x0], src[y0 * pw + x1], src[y1 * pw + x0], src[y1 * pw + x1]}, out = 0, ch;
                uint32_t aw = 0;
                for (k = 0; k < 4; k++) {
                    aw += c[k] >> 24;
                }
                for (ch = 0; ch < 24; ch += 8) { /* colour weighted by alpha: no dark rims round what is cut out */
                    uint32_t sum = 0;
                    for (k = 0; k < 4; k++) {
                        sum += ((c[k] >> ch) & 0xFF) * ((c[k] >> 24) + (aw == 0 ? 1 : 0));
                    }
                    out |= (aw != 0 ? sum / aw : sum / 4) << ch;
                }
                dst[y * nw + x] = out | ((aw + 2) / 4) << 24;
            }
        }
        lv[levels] = dst;
        lw[levels] = nw;
        lh[levels] = nh;
        levels++;
    }
    if (px != NULL && sBackend->texSlotsLeft() < levels) {
        for (k = 1; k < levels; k++) {
            free(lv[k]);
        }
        levels = 1;
    }
    tex = px != NULL ? sBackend->texCreate(w, h, GS_TEXFMT_RGBA8, levels) : 0;
    if (tex == 0) {
        for (k = 0; k < levels; k++) {
            free(lv[k]);
        }
        return 0;
    }
    for (k = 0; k < levels; k++) {
        sBackend->texUpload(tex, k, lw[k], lh[k], GS_TEXFMT_RGBA8, lv[k], lw[k] * lh[k] * 4);
    }
    return tex;
}

/* The lines of letters (gs/tbo_text.c): each place's texture and what it shows; a texture drawn in this frame is
   kept until the next (the line changed under it: the old one is shown once more). */
extern uint32_t TboText_Version(uint32_t id);
extern uint32_t *TboText_Pixels(uint32_t id, uint32_t *version, uint32_t *w, uint32_t *h);
static struct {
    GsTex tex;
    uint32_t version, frame;
} sTboText[TBO_TEXT_SLOTS];

static GsTex tbo_text_texture(uint32_t id) {
    uint32_t slot = id - TBO_TEXT_BASE, version = TboText_Version(id), w, h, *px;
    GsTex tex;

    if (sTboText[slot].tex != 0 && (sTboText[slot].version == version || sTboText[slot].frame == gGsFrame)) {
        sTboText[slot].frame = gGsFrame;
        return sTboText[slot].tex;
    }
    if (sBackend->texSlotsLeft() <= 0) {
        return sTboText[slot].tex != 0 ? sTboText[slot].tex : gsWhite;
    }
    px = TboText_Pixels(id, &version, &w, &h);
    tex = px != NULL ? tbo_upload(px, w, h) : 0;
    if (tex == 0) {
        return sTboText[slot].tex != 0 ? sTboText[slot].tex : gsWhite;
    }
    if (sTboText[slot].tex != 0) {
        sBackend->texDestroy(sTboText[slot].tex);
    }
    sTboText[slot].tex = tex;
    sTboText[slot].version = version;
    sTboText[slot].frame = gGsFrame;
    return tex;
}

static GsTex tbo_texture(uint32_t id) {
    char path[1024];
    SDL_Surface *png, *rgba;
    uint32_t *px, w, h, x, y;
    GsTex tex;
    const char *base;

    if (id >= TBO_TEXT_BASE && id < TBO_TEXT_BASE + TBO_TEXT_SLOTS) {
        return tbo_text_texture(id);
    }
    if (id >= TBO_ART_COUNT || sTboMissing[id]) {
        return gsWhite;
    }
    if (sTboTex[id] != 0) {
        return sTboTex[id];
    }
    if (sBackend->texSlotsLeft() <= 0) {
        return gsWhite; /* (no room this frame: the next one tries again) */
    }
    base = SDL_GetBasePath();
    snprintf(path, sizeof(path), "%stbo/ui/%s.png", base != NULL ? base : "", kTboNames[id]);
    png = SDL_LoadPNG(path);
    if (png == NULL) {
        snprintf(path, sizeof(path), "tbo/ui/%s.png", kTboNames[id]);
        png = SDL_LoadPNG(path);
    }
    if (png == NULL) { /* a source checkout: the pictures where they are kept (port/tbo/ui) */
        snprintf(path, sizeof(path), "port/tbo/ui/%s.png", kTboNames[id]);
        png = SDL_LoadPNG(path);
    }
    rgba = png != NULL ? SDL_ConvertSurface(png, SDL_PIXELFORMAT_RGBA32) : NULL;
    if (png != NULL) {
        SDL_DestroySurface(png);
    }
    if (rgba == NULL) {
        fprintf(stderr, "bt3: tbo: cannot read %s\n", path);
        sTboMissing[id] = 1;
        return gsWhite;
    }
    w = (uint32_t)rgba->w;
    h = (uint32_t)rgba->h;
    px = malloc((size_t)w * h * 4);
    for (y = 0; px != NULL && y < h; y++) {
        const uint8_t *row = (const uint8_t *)rgba->pixels + (size_t)y * rgba->pitch;
        for (x = 0; x < w; x++) {
            const uint8_t *c = row + x * 4;
            uint32_t a = ((uint32_t)c[3] * 128 + 127) / 255; /* the GS's alpha: 0x80 is opaque */
            px[y * w + x] = (uint32_t)c[0] | ((uint32_t)c[1] << 8) | ((uint32_t)c[2] << 16) | (a << 24);
            /* the window: its inside is bands of two teals, which fall anywhere under the lines at the sizes it is
               drawn (src/menu/port_tbo.c, Tb_Panel). Every teal pixel is the darker one; the frame (grey, nearly
               black) is not teal and stays, its round corners with it. */
            if (id == TBO_KIT_WINDOW && c[3] == 255 && c[0] < 60 && c[1] >= 60 && c[1] <= 125 && c[2] + 10 >= c[1] && c[2] <= c[1] + 15) {
                px[y * w + x] = 27u | (86u << 8) | (92u << 16) | (a << 24);
            }
        }
    }
    SDL_DestroySurface(rgba);
    tex = tbo_upload(px, w, h);
    if (tex == 0) {
        return gsWhite;
    }
    sTboTex[id] = tex;
    fprintf(stderr, "bt3: tbo: %s (%ux%u)\n", kTboNames[id], (unsigned)w, (unsigned)h);
    return tex;
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
    int packFormat = GS_TEXFMT_RGBA8, replaced = 0, menuText = 0, sharedText = 0;

    if (tw > 1024) { tw = 1024; }
    if (th > 1024) { th = 1024; }
    rw = tw; rh = th;
    pages = ((tbw ? tbw : 1) * 64 * th * (bits == 24 ? 32 : bits) / 8 + 8191) / 8192;
    /* `gen` identifies the CONTENT: a hash over the pages the texture and its palette occupy */
    for (i = 0; i <= pages; i++) {
        gen = (gen ^ Gs_PageHash(tbp / 32 + i)) * 16777619u + i;
    }
    if (bits <= 8) {
        gen = (gen ^ clut_hash(cbp, cpsm, bits == 8 ? 256 : 16)) * 16777619u;
    }
    /* a font instead of the game's lettering (menu_text.c): the labels and glyph sheets it draws follow the font
       and the resolution. Nothing is added with both settings on "Original". */
    gen ^= MenuText_TexKey((int)tw, (int)th, bits <= 8, (int)tbp, (int)cbp, MenuText_RasterScale(SCALE));
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
    if (!replaced && px != NULL) {
        int mw, mh;
        uint64_t font_t0 = gpu_now();
        int textScale = MenuText_RasterScale(SCALE);
        uint8_t *text = MenuText_RenderGlyphs((uint8_t *)px, (int)tw, (int)th, (int)tbp, (int)cbp, textScale, &mw, &mh);
        sharedText = text != NULL;
        if (text == NULL) { text = MenuText_Render((uint8_t *)px, (int)tw, (int)th, bits <= 8, textScale, &mw, &mh); }
        if (text != NULL) {
            free(px);
            px = (uint32_t *)text;
            rw = (uint32_t)mw; rh = (uint32_t)mh;
            menuText = 1;
        }
        if (getenv("BT3_FONT_LOG") != NULL) { /* what a font setting costs: every decoded texture, and the time the text took */
            fprintf(stderr, "font: frame %u: %ux%u %s %.3f ms\n", gGsFrame, (unsigned)tw, (unsigned)th, text != NULL ? "drawn" : "left",
                    (double)(gpu_now() - font_t0) / 1e6);
        }
    }
    gGpuTexNs += gpu_now() - tex_t0;
    t = &sTex[gsTexCount];
    t->tex = replaced ? sBackend->texCreate(rw, rh, packFormat, pack.levels) : sBackend->texCreate(rw, rh, GS_TEXFMT_RGBA8, 1);
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
        if (getenv("BT3_TEX_DUMP") != NULL) { /* every texture as it is decoded, raw RGBA with a small header (looking at the game's art) */
            static unsigned n;
            char name[600];
            FILE *df;
            snprintf(name, sizeof(name), "%s/tex_%05u_f%06u_%ux%u.rgba", getenv("BT3_TEX_DUMP"), n++, gGsFrame, (unsigned)rw, (unsigned)rh);
            df = fopen(name, "wb");
            if (df != NULL) {
                fwrite(px, 1, (size_t)rw * rh * 4, df);
                fclose(df);
            }
        }
        sBackend->texUpload(t->tex, 0, rw, rh, GS_TEXFMT_RGBA8, px, rw * rh * 4);
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
    gGpuNewTexPixels += rw * rh;
    sLast = t;
    t->tex0 = t0;
    t->texa = texa;
    t->gen = gen;
    t->last = gGsFrame;
    t->replaced = replaced ? 1 : sharedText ? 3 : menuText ? 2 : 0;
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
   23 the fragment shader without its own depth output, 24 flat-shadow stencil mask. */
static int pipeline_get(int ctx, int topo, int vu, int shadow) {
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
          (uint32_t)shadow << 24;
    {   /* Bit 23: the fragment shader without its own depth output (gsnz.frag; see the end of gs.frag for why
           writing depth in the shader costs). Taken where it changes nothing: a draw that does not write depth
           and tests with "greater or equal" (or not at all). What is in the depth buffer was written through
           the other shader, so it is whole numbers; and z >= N is the same as floor(z) >= N for a whole N.
           ("Greater" is not the same: N + 0.5 > N but floor(N + 0.5) > N is not. Those keep the full shader, and
           so does everything that writes depth.) Measured on an integrated chip at 2x in a fight: 112 pictures a
           second before the depth output existed (0.1.24), 82 with it on every draw (0.1.25 to 0.1.30), and
           with this rule see docs/port/README.md. BT3_NZ=0: every draw with the full shader again (to compare). */
        static int nz = -1;
        if (nz < 0) { nz = getenv("BT3_NZ") != NULL ? atoi(getenv("BT3_NZ")) : 1; }
        if (nz && !zwrite && ztst != 3 && vu != 4) {
            key |= 1u << 23;
        }
    }
    if (vu == 4) { /* asked for by depth_clut: the table pass, which only depends on blending and the write mask */
        key = bkey | wmask << 16 | 1u << 22;
    }
    {
        /* The back end finds a pipeline by going through all it has made (some hundreds) and is asked once for
           every primitive: 6% of a fight frame's processor time in a profile. The answer is remembered here by
           key. A pipeline keeps its number for the whole run (they are only ever added), so nothing goes stale. */
        static struct { uint32_t key; int idx; } cache[4096]; /* idx + 1; 0 = empty */
        uint32_t h = (key * 2654435761u) >> 20;
        int i;
        if (cache[h].idx != 0 && cache[h].key == key) {
            return cache[h].idx - 1;
        }
        i = sBackend->pipeGet(key);
        if (i >= 0) {
            cache[h].key = key;
            cache[h].idx = i + 1;
        }
        return i;
    }
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

static int pipeline_get(int ctx, int topo, int vu, int shadow);

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
        sWholeFrame = tme && tpsm == 1 && sprite && tbp / 32 == zbp; /* the cross-fade's picture (below) */
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
        /* (Not the cross-fade of a fight's scenes: the frame the game read back is uploaded over the depth page as a
           24-bit picture and drawn from there over the new shot, stg_b.c ScrXfade_Draw. That is a picture like any
           other, taken from GS memory; the passes dropped here read the page as depth or as one of its bytes.) */
        if (tme && tpsm != TBO_PSM && !(tpsm == 1 && sprite && tbp / 32 == zbp) &&
            ((tpsm & 0x30) == 0x30 || tbp / 32 == zbp || tpsm == 0x1B || tpsm == 0x24 || tpsm == 0x2C)) {
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
    if (tme && ((t0 >> 20) & 0x3F) == TBO_PSM) {
        d->tex = tbo_texture((uint32_t)((t0 >> 37) & 0x3FFF)); /* a Tenkaichi Budokai Online picture (above texture_get) */
    } else if (tme) {
        int kept = 0;
        src = GsDraw_TargetGet((uint32_t)((t0 & 0x3FFF) / 32), 0);
        if (sWholeFrame) { /* the cross-fade's picture: the frame kept on the card (GsGpu_Snapshot), if there is one */
            int sn = GsDraw_TargetGet(0x1FF, 0);
            if (sn >= 0 && gsTargets[sn].cleared && sn != d->target) {
                src = sn;
                kept = 1;
            }
        }
        /* a frame buffer used as a texture: only if nothing was uploaded over it since it was drawn */
        if (src >= 0 && (kept || ((t0 & 0x1F) == 0 && gsTargets[src].cleared && gsTargets[src].gen == gGsPageGen[gsTargets[src].fbp & 511]))) {
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
                d->rect[0] = (u0 + 0.5f) / (float)GS_W;
                d->rect[1] = (v0 + 0.5f) / (float)GS_H;
                d->rect[2] = (u1 + 0.5f) / (float)GS_W;
                d->rect[3] = (v1 + 0.5f) / (float)GS_H;
            }
            *us = tw / (float)GS_W;
            *vs = th / (float)GS_H;
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
    d->pipeline = pipeline_get(ctx, topo, vu, 0);
    d->mode[0] = !tme ? 0 : d->tex_is_target ? 2 : 1; /* 2: a frame buffer as texture, its alpha is already rescaled */
    /* (BT3_TEX_ALPHA=0 switches the replacement's alpha treatment off, for telling which one a fault comes from) */
    if (d->mode[0] == 1 && sLast != NULL && sLast->tex == d->tex && sLast->replaced == 1 &&
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
    d->blendc = (float)((gGs.alpha[ctx] >> 32) & 0xFF) / 128.0f;
    d->scissor[0] = (int)(sc & 0x7FF) * SCALE;
    d->scissor[1] = (int)((sc >> 32) & 0x7FF) * SCALE;
    d->scissor[2] = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d->scissor[0];
    d->scissor[3] = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d->scissor[1];
    return 1;
}

#define GS_Z_LEAD 2.0 /* depth steps: see put() */
/* BT3_DEPTH_FRAC=1: depth as before the whole-number rule (fractions kept, nothing rounded), to compare. */
static int depth_frac(void) {
    static int v = -1;
    if (v < 0) { v = getenv("BT3_DEPTH_FRAC") != NULL; }
    return v;
}

float gsZMax = 16777215.0f; /* the depth buffer's largest value, as the frame's draws had it (the fragment shader rounds with it) */

static void put(const GsVertex *v, float x, float y, float s, float t, float q, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float zmax) {
    Vtx *o = &gsVerts[gsVertCount++];
    o->x = x;
    o->y = y;
    /* The whole number plus a half (see gs.frag; the vertex programs do the same), and GS_Z_LEAD steps towards the
       eye. What the game transforms itself and lays onto scenery of a vertex program (a waterfall on its rock) is
       in front of it by less than a step: on the console both come out as the same whole number, or the fall's
       one higher. Here the rock's number is computed by the GPU and can come out one higher than the console's,
       this pixel and not that one, differently every picture while the camera moves: found by the user as the
       fall blinking sideways in motion (the part it had been missing came and went). With the lead the fall
       wins wherever the two are within those steps. (Not past the nearest depth there is.) */
    o->z = (float)(((double)v->z + (depth_frac() ? 0.0 : 0.5 + ((double)v->z + GS_Z_LEAD < (double)zmax ? GS_Z_LEAD : 0.0))) / zmax);
    gsZMax = depth_frac() ? 0.0f : zmax;
    o->r = r; o->g = g; o->b = b; o->a = a;
    o->s = s; o->t = t; o->q = q;
}

/* ---- the wall at the edge of the map, as the game hands it over (see interp_wall) ---- */
typedef struct WallMesh {
    const void *key;      /* the game's mesh (one per fighter and kind) */
    int second, culled, nq;
    float S[16], cam[3];  /* the view's world-to-screen matrix and camera position of the tick */
    float v[24][16];      /* EftBoundVtx: position (4), -, s t (4), colour (4) */
    int idx[15][4];
} WallMesh;
#define WALL_MAX 8
/* The last patches handed over, oldest overwritten. Not "this tick's and the one before's": the game hands them
   over while it builds its list, a vertical blank before that list is shown, so a tick's patches come in before
   the previous tick's real picture is out. Which are a picture's is told by the view's matrix (interp_wall). */
#define WALL_RING 32
static WallMesh sWallRing[WALL_RING];
static uint32_t sWallSeq; /* patches handed over so far */
static uint64_t sWallTex0; /* TEX0 of the wall's texture: the triangles drawn with it are the wall's */
static float sWallUs = 1.0f, sWallVs = 1.0f;
extern int gsInterp;

/* From the game (src/battle/eft_g.c, EftBound_DrawMesh / EftBound_DrawMeshCulled), each time it draws a wall mesh:
   the 24 points before anything is cut off, the quads, the view's matrix. */
void Port_WallMesh(const void *key, int second, int culled, const float *screen, const float *cam, const float *vtx, int quadCount,
                   const void *quads, unsigned long long tex0) {
    WallMesh *m;
    int i;

    if (GS_NO_PICTURE || gsInterp <= 0 || quadCount <= 0 || quadCount > 15) {
        return;
    }
    if (getenv("BT3_INTERP_WALLLOG") != NULL && atoi(getenv("BT3_INTERP_WALLLOG")) > 2) {
        extern unsigned gPortVBlanks;
        fprintf(stderr, "wall: hook at frame %u blank %u: key %p second %d culled %d m0 %.4f (have %d)\n", gGsFrame, gPortVBlanks, key, second, culled, screen[0], (int)sWallSeq);
    }
    sWallTex0 = tex0;
    m = &sWallRing[sWallSeq++ % WALL_RING];
    m->key = key;
    m->second = second != 0;
    m->culled = culled;
    m->nq = quadCount;
    memcpy(m->S, screen, sizeof(m->S));
    memcpy(m->cam, cam, sizeof(m->cam));
    memcpy(m->v, vtx, sizeof(m->v));
    for (i = 0; i < quadCount; i++) {
        memcpy(m->idx[i], (const uint8_t *)quads + i * 0x20, sizeof(m->idx[i])); /* EftBoundQuad: four vertex numbers, a normal */
    }
}

/* Two draws can be merged into one call when everything they need is the same. `scissor` is compared as
   four ints (the scissor a draw has recorded). */
static int same_state(const GsDraw *a, const GsDraw *b) {
    return !a->native && a->vu == b->vu && a->wall == b->wall && a->target == b->target && a->tex == b->tex && a->sampler == b->sampler && a->pipeline == b->pipeline &&
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

/* The part of the buffer the draws after snapshot `k` (a native 5) read the copied alpha in: the box around the run
   of alpha-testing draws that follows it, in the buffer's own pixels, a little wider. A back end copies only that.
   (The whole buffer was copied each time: nine times a frame for a fight's HUD, whose pieces are small. On an
   integrated graphics chip at 3x those copies were a quarter of the frame's time: 45 pictures a second with them,
   62 without. Measured 2026-10-12.) A run with a draw whose place is worked out by a vertex program is not known
   here: then the whole buffer, as before. Returns 0 when there is nothing to copy. */
int GsDraw_DateRegion(uint32_t k, int *rx, int *ry, int *rw, int *rh) {
    const GsDraw *c = &gsDraws[k];
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    int W = GS_W * SCALE, H = GS_H * SCALE, ix0, iy0, ix1, iy1, whole = 0, any = 0;
    uint32_t i, j;

    for (i = k + 1; i < gsDrawCount; i++) {
        const GsDraw *d = &gsDraws[i];
        if (d->native || d->misc[1] == 0.0f || d->target != c->target) {
            break;
        }
        any = 1;
        if (d->vu || d->wall) {
            whole = 1;
            break;
        }
        for (j = 0; j < d->count; j++) {
            const Vtx *v = &gsVerts[d->first + j];
            if (!(v->x == v->x) || !(v->y == v->y)) { /* not a number: do not trust the box */
                whole = 1;
                break;
            }
            if (v->x < x0) { x0 = v->x; }
            if (v->x > x1) { x1 = v->x; }
            if (v->y < y0) { y0 = v->y; }
            if (v->y > y1) { y1 = v->y; }
        }
    }
    if (!any) {
        return 0;
    }
    {
        static int all = -1; /* BT3_DATE_WHOLE=1: the whole buffer every time, as it was (to compare pictures) */
        if (all < 0) {
            all = getenv("BT3_DATE_WHOLE") != NULL;
        }
        whole = whole || all;
    }
    if (whole || x1 < x0 || y1 < y0) {
        *rx = 0, *ry = 0, *rw = W, *rh = H;
        return 1;
    }
    x0 = (x0 - 2.0f) * (float)SCALE, y0 = (y0 - 2.0f) * (float)SCALE;
    x1 = (x1 + 2.0f) * (float)SCALE, y1 = (y1 + 2.0f) * (float)SCALE;
    ix0 = x0 < 0.0f ? 0 : x0 > (float)W ? W : (int)x0;
    iy0 = y0 < 0.0f ? 0 : y0 > (float)H ? H : (int)y0;
    ix1 = x1 < 0.0f ? 0 : x1 > (float)W ? W : (int)x1 + 1;
    iy1 = y1 < 0.0f ? 0 : y1 > (float)H ? H : (int)y1 + 1;
    if (ix1 > W) { ix1 = W; }
    if (iy1 > H) { iy1 = H; }
    if (ix1 <= ix0 || iy1 <= iy0) {
        return 0; /* the run is off the buffer: nothing reads the copy */
    }
    *rx = ix0, *ry = iy0, *rw = ix1 - ix0, *rh = iy1 - iy0;
    return 1;
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
    if (GS_NO_PICTURE) { /* a frame that is only being re-run, or one that is skipped: nothing is drawn */
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
    int tbo = (prim >> 4) & 1 && ((t0 >> 20) & 0x3F) == TBO_PSM; /* a Tenkaichi Budokai Online picture */
    if (type == 6 || fst) {
        d.sampler &= 7; /* 2D art: never the smaller copies of a replacement */
    }
    if (tbo) {
        d.sampler |= 1 | 8; /* filtered, with its smaller copies, sampled per output pixel (not per GS pixel) */
    }
    if ((type == 6 || fst) && !d.tex_is_target && !tbo) {
        /* 2D art: sprites, and triangles with whole-texel coordinates (the logo, HUD pieces drawn as quads):
           texture coordinates per GS pixel (gs.frag). Not for a texture pack's replacement: that has several
           texels per GS pixel, and sampling it once per GS pixel would show it at the original's resolution. */
        /* The setting "2D filtering" (sSmooth2d): the game's own 2D art is sampled at every output pixel too, inside
           the piece's rectangle of its sheet (gs.frag has the rule for it, another than the replacement's). Off,
           a GS pixel of 2D art is one block of output pixels, as on the console. */
        if ((sSmooth2d && d.tex != gsWhite) ||
            (sLast != NULL && sLast->tex == d.tex && sLast->replaced && !(getenv("BT3_TEX_2D") != NULL && atoi(getenv("BT3_TEX_2D")) == 0))) {
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
        /* A replacement follows its piece smoothly whatever "2D filtering" says: sharp means the game's own art as
           blocks of a GS pixel, which a replacement of several texels per GS pixel is not; held to the GS's grid
           its letters stepped one after the other while a piece glided (BT3_TEX_GRID=1: the grid rule, to compare). */
        d.orig[3] = getenv("BT3_TEX_GRID") != NULL && atoi(getenv("BT3_TEX_GRID")) != 0 ? 0.0f : 1.0f;
    }
    if (type == 3 && sWallTex0 != 0 && t0 == sWallTex0) { /* the wall at the edge of the map (Port_WallMesh) */
        d.wall = 1;
        sWallUs = us;
        sWallVs = vs;
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
        if (fst && !d.tex_is_target && sLast != NULL && sLast->tex == d.tex && sLast->replaced == 3) {
            float rect[8];
            int tbp = (int)(t0 & 0x3FFF), cbp = (int)((t0 >> 37) & 0x3FFF);
            if (MenuText_MapGlyph(tbp, cbp, (int)tw, (int)th,
                                  v[0].u/16.0f, v[0].v/16.0f, v[1].u/16.0f, v[1].v/16.0f, rect)) {
                float originalHeight = (v[1].v-v[0].v)/16.0f;
                float yScale = (v[1].y-v[0].y)/originalHeight;
                float y0 = v[0].y - rect[4]*yScale, y1 = v[1].y + rect[5]*yScale;
                int k;
                /* The extra space changes only the drawn quad. Font_DrawGlyph still returns the original
                   advance, so wrapping, alignment and the following character's position are unchanged. */
                for (k = 0; k < 6; k++) {
                    Vtx *o = &gsVerts[d.first+k];
                    int right = k == 1 || k == 3 || k == 4, bottom = k == 2 || k == 4 || k == 5;
                    o->y = bottom ? y1 : y0;
                    o->s = (rect[0]+0.5f+(right ? rect[2] : 0))/rect[6];
                    o->t = (rect[1]+0.5f+(bottom ? rect[3] : 0))/rect[7];
                }
                d.rect[0]=rect[0]/rect[6]; d.rect[1]=rect[1]/rect[7];
                d.rect[2]=(rect[0]+rect[2])/rect[6]; d.rect[3]=(rect[1]+rect[3])/rect[7];
                d.orig[0]=rect[6]; d.orig[1]=rect[7]; d.orig[3]=1.0f; d.misc[3]=-(float)SCALE;
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
    /* A piece of a side panel of the HUD (a health bar, a portrait) that can be seen was drawn: the frame it was
       in. What the overlay shows with the HUD (the players' names of an online fight, ui.cpp) asks through
       GsDraw_HudShown, and is not there in an intro or wherever else the game leaves the HUD out. */
    if ((gsAnchor == 1 || gsAnchor == 2 || gsAnchor == 5 || gsAnchor == 6) && (type == 6 || fst) && v[type == 6 ? 1 : 0].a != 0) {
        if (gGsFrame - sHudFrame > 6 || !sHudSeen) { /* BT3_HUD_LOG (testing): when the HUD comes back */
            static int log = -1;
            if (log < 0) { log = getenv("BT3_HUD_LOG") != NULL; }
            if (log) { fprintf(stderr, "hud: drawn from frame %u (last at %u)\n", gGsFrame, sHudSeen ? sHudFrame : 0); }
        }
        sHudFrame = gGsFrame;
        sHudSeen = 1;
    }
    /* (Not the cross-fade's picture: it is the whole frame as it was shown, wide view and all, drawn back over the
       whole frame in strips. Each strip is narrower than the screen, so the rule below took it for a piece of 2D
       art and the old shot was drawn at three quarters of the width over the new one: seen by the user.) */
    if (Port_IsWide() && (!gPortMenuMode || (gsAnchor == 7 && Port_PageAspectMilli() > 1340)) && !sWholeFrame && !d.tex_is_target && !tbo && (type == 6 || fst) && gsTargets[d.target].fbp == (uint32_t)gGsMainFbp) {
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
        } else if (x1 - x0 >= 480.0f && gsAnchor != 5 && gsAnchor != 6) {
            /* as wide as the screen (the bands and streaks of READY / FIGHT, flashes, speed lines): meant to
               reach from edge to edge, so it keeps the full width */
            pivot = -1.0f;
        } else if (gsAnchor == 7) {
            /* Loading animations keep their left margin and sprite proportions, even in menu mode.
               Scattered items already have their positions spread over the wider page by loading.c. */
            pivot = 0.0f;
        } else if (gsAnchor != 0) {
            pivot = (gsAnchor == 1 || gsAnchor == 5) ? 0.0f : (gsAnchor == 2 || gsAnchor == 6) ? 512.0f : 256.0f;
            if (sHudLayout == 2) {
                pivot = 256.0f;
            } else
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
    if (GS_NO_PICTURE) { /* a frame that is only being re-run, or one that is skipped: nothing is drawn */
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
    if (depth_frac()) { u.misc[2] = -u.misc[2]; }
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
    if (GS_NO_PICTURE) { /* a frame that is only being re-run, or one that is skipped: nothing is drawn */
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
    if (depth_frac()) { u.misc[2] = -u.misc[2]; }
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
    if (GS_NO_PICTURE) { /* a frame that is only being re-run, or one that is skipped: nothing is drawn */
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
    if (layer == 2 && d.misc[1] == 1.0f && d.misc[2] != 0.0f) {
        /* TEST.DATE rejects pixels marked by FBA. Stencil also rejects overlap within this
           draw run, which a single snapshot of the alpha buffer cannot see. */
        d.pipeline = pipeline_get(ctx, 0, 1, 1);
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
    if (depth_frac()) { u.misc[2] = -u.misc[2]; }
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
/* The frame the game reads back for its cross-fade, kept on the card at the resolution it was drawn in: a target of
   its own (an address no frame buffer of the game has), which the cross-fade's draws then take as their texture
   (draw_state). Read back and uploaded again as the game does it, the old shot came out at the PS2's 512 x 448,
   blocky over the new one (seen by the user as pixelated outlines in an intro). 0 = not kept (the game's own
   way is used). */
#define SNAP_FBP 0x1FF
int GsGpu_Snapshot(uint32_t fbp) {
    int src = GsDraw_TargetGet(fbp, 0), dst;
    if (src < 0 || !gsTargets[src].cleared) {
        src = gGsMainFbp >= 0 ? GsDraw_TargetGet((uint32_t)gGsMainFbp, 0) : -1;
    }
    if (sBackend == NULL || sBackend->targetCopy == NULL || src < 0 || !gsTargets[src].cleared) {
        return 0;
    }
    dst = GsDraw_TargetGet(SNAP_FBP, 1);
    if (dst < 0 || dst == src) {
        return 0;
    }
    sBackend->targetCopy(src, dst);
    gsTargets[dst].cleared = 1;
    gsTargets[dst].stale = 0;
    gsTargets[dst].gen = gGsPageGen[SNAP_FBP & 511];
    return 1;
}

/* The picture of the frame buffer at `fbp` as the GS has it, 512 x 448 RGBA (the game reads the shown frame back
   for its cross-fade: Gs_StoreImage in gs_core.c). The buffer asked for, or else the frame's own. */
int GsGpu_ReadFrame(uint32_t fbp, uint8_t *rgba) {
    int t = GsDraw_TargetGet(fbp, 0);
    if (t < 0 || !gsTargets[t].cleared) {
        t = gGsMainFbp >= 0 ? GsDraw_TargetGet((uint32_t)gGsMainFbp, 0) : -1;
    }
    if (sBackend == NULL || sBackend->targetRead == NULL || t < 0 || !gsTargets[t].cleared) {
        return 0;
    }
    return sBackend->targetRead(t, rgba);
}

void GsGpu_FbUpload(int second) {
    if (GS_NO_PICTURE) { /* a frame that is only being re-run, or one that is skipped: nothing is drawn */
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
    if (GS_NO_PICTURE) { /* a frame that is only being re-run, or one that is skipped: nothing is drawn */
        return;
    }
    uint64_t sc = gGs.scissor[0];
    GsDraw d;

    /* 1 = outline. 2 (the see-through tint) is drawn by the generic table pass now (depth_clut), from the game's
       own table. */
    if (effect >= 0x15 && effect <= 0x17) {
        /* 5/6: extended life bar, anchored left/right even beyond 480 pixels; 7: loading page, anchored left. */
        gsAnchor = effect - 0x10;
        return;
    }
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
    if (gsPendingScale == 0 || gsPendingScale == gsScale) {
        gsPendingScale = 0;
        return;
    }
    gsScale = gsPendingScale; /* the back end's attachments and copies are rebuilt to this size */
    gsPendingScale = 0;
    sBackend->scaleChanged();
    gsTargetCount = 0;
    fprintf(stderr, "bt3: internal resolution %dx (%d x %d)\n", gsScale, 512 * gsScale, 448 * gsScale);
}

/* The list of menu labels is made in a thread at start when it is not there (menu_labels.c, menu_text.c). What it
   made is taken into use here, between two frames: the settings window and the texture cache see the fonts for the
   menus appear at no other moment. Not under an online session: it waits until that is over. */
static void menu_labels_apply(void) {
    extern int Port_NetSession(void); /* net.c */
    if (!Port_NetSession()) {
        MenuText_Poll();
    }
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
    fprintf(stderr, "bt3: internal resolution %dx (%d x %d)\n", gsScale, 512 * gsScale, 448 * gsScale);
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
/* For the game's code: whether an effect of the settings window is switched off (the bits of fx_off). Asked by the
   one effect that is left out where the game draws it and not here: the blur while the view turns (32,
   src/sys/gfxm_a.c StgPanBlur_Draw), which is several passes through work buffers. */
int Port_FxOff(int bit) {
    return (gsFxOff & (unsigned)bit) != 0;
}

void GsGpu_GetSettings(PortVideo *v) {
    v->scale = gsPendingScale ? gsPendingScale : gsScale;
    v->aspectMilli = Port_AspectMilli();
    v->fullscreen = sFullscreen;
    v->fxOff = (int)gsFxOff;
    v->glow = gsGlowPercent;
    v->music = gPortMusicPercent;
    v->effects = gPortSePercent;
    v->voices = gPortVoicePercent;
    v->stage = gPortStagePercent;
    v->display = sDisplaySetting;
    v->texPack = sTexPackOn;
    v->texPackCount = TexPack_Count();
    v->menuFont = MenuText_GetFont();
    v->textFont = MenuText_GetSharedFont();
    v->smooth2d = sSmooth2d;
    v->vsync = Port_Setting("vsync", 0) != 0;
    v->frameSkip = GsDraw_FrameSkip();
    v->hudLayout = sHudLayout;
}

/* Applies what differs from the current state, all of it at once, and keeps it for the next run. */
void GsGpu_SetSettings(const PortVideo *v) {
    PortVideo now;
    GsGpu_GetSettings(&now);
    if (v->scale != now.scale) {
        gsPendingScale = v->scale < 1 ? 1 : v->scale > 8 ? 8 : v->scale; /* applied between two frames */
    }
    if (v->aspectMilli != now.aspectMilli) {
        Port_SetAspectMilli(v->aspectMilli);
        GsDraw_WindowShape();
    }
    if (v->fullscreen != now.fullscreen) {
        GsDraw_FullscreenSet(v->fullscreen);
    }
    gsFxOff = (unsigned)v->fxOff & 63;
    gsGlowPercent = v->glow;
    if (v->music != now.music || v->effects != now.effects || v->voices != now.voices || v->stage != now.stage) { /* (the streams that are playing follow at once) */
        gPortMusicPercent = v->music;
        gPortSePercent = v->effects;
        gPortVoicePercent = v->voices;
        gPortStagePercent = v->stage;
        Port_AudioRefresh();
    }
    if (v->hudLayout != now.hudLayout) {
        sHudLayout = v->hudLayout < 0 || v->hudLayout > 2 ? 0 : v->hudLayout;
    }
    if (v->frameSkip != now.frameSkip) {
        GsDraw_FrameSkipSet(v->frameSkip);
    }
    if (v->vsync != now.vsync) { /* (whichever back end draws) */
        GsGpu_Vsync(v->vsync);
        GsGl_Vsync(v->vsync);
        Port_SettingSave("vsync", v->vsync);
    }
    sDisplaySetting = v->display;
    sTexPackOn = v->texPack != 0;
    MenuText_SetFont(v->menuFont);
    MenuText_SetSharedFont(v->textFont);
    Port_SettingSave("text_font_id", MenuText_FontId(MenuText_GetSharedFont()));
    Port_SettingSave("menu_font_id", MenuText_FontIdToSave());
    sSmooth2d = v->smooth2d != 0;
    Port_SettingSave("smooth_2d", sSmooth2d);
    Port_SettingSave("texture_pack", sTexPackOn);
    Port_SettingSave("scale", gsPendingScale ? gsPendingScale : gsScale);
    Port_SettingSave("aspect_milli", Port_AspectMilli());
    Port_SettingSave("fullscreen", sFullscreen);
    Port_SettingSave("fx_off", (int)gsFxOff);
    Port_SettingSave("glow", gsGlowPercent);
    Port_SettingSave("music", gPortMusicPercent);
    Port_SettingSave("effects", gPortSePercent);
    Port_SettingSave("voices", gPortVoicePercent);
    Port_SettingSave("hud_layout", sHudLayout);
    Port_SettingSave("stage_sounds", gPortStagePercent);
    Port_SettingSave("display", sDisplaySetting);
    Port_SettingsWrite();
}

int GsGpu_Init(void) {
    const char *api = getenv("BT3_GPU_API");
    GsBackend *sb;

    gsScale = getenv("BT3_SCALE") != NULL ? atoi(getenv("BT3_SCALE")) : Port_Setting("scale", 2);
    gsScale = gsScale < 1 ? 1 : gsScale > 8 ? 8 : gsScale;
    gsPendingScale = 0;
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
    gsFxOff = (unsigned)(getenv("BT3_FX_OFF") != NULL ? atoi(getenv("BT3_FX_OFF")) : Port_Setting("fx_off", 0)) & 63;
    gsGlowPercent = getenv("BT3_GLOW") != NULL ? atoi(getenv("BT3_GLOW")) : Port_Setting("glow", GLOW_DEFAULT);
    MenuText_SetFontId(Port_Setting("menu_font_id", 0));
    MenuText_SetSharedFontId(Port_Setting("text_font_id", 0));
    sTexPackOn = Port_Setting("texture_pack", 1) != 0;
    sSmooth2d = getenv("BT3_2D_SMOOTH") != NULL ? atoi(getenv("BT3_2D_SMOOTH")) != 0 : Port_Setting("smooth_2d", 1) != 0;
    gPortMusicPercent = Port_Setting("music", 100);
    gPortSePercent = Port_Setting("effects", 100);

    sHudLayout = getenv("BT3_HUD") != NULL ? atoi(getenv("BT3_HUD")) : Port_Setting("hud_layout", 0);
    sHudLayout = sHudLayout < 0 || sHudLayout > 2 ? 0 : sHudLayout;
    gPortVoicePercent = Port_Setting("voices", gPortSePercent); /* (one slider before 0.1.30: what it was set to) */
    gPortStagePercent = Port_Setting("stage_sounds", gPortSePercent);
    TexPack_Init();
    return 1;
}

/* ---------------------------------------------------------------------------------- in-between pictures
 * The game computes 30 pictures a second (every loop waits for two vertical blanks). With BT3_INTERP=1 (setting
 * "interp") the renderer shows one more between each two: when the game has finished picture n, the list of n is
 * first replayed with the vertex programs' matrices (bones, pivots, the screen matrix: Vu0Uniform) half way from
 * those of n - 1, and the real n follows one vertical blank later (GsGpu_InterpFlush, from Port_VBlank). The
 * game itself is untouched: same simulation, same inputs, same online play. The cost is that picture n is seen
 * one vertical blank (16.7 ms) later than without.
 *
 * PROTOTYPE. Blended: what the vertex programs draw (fighters, stage, shadows, debris). Not blended: what the
 * game transforms itself (effects, HUD, 2D), which stays that of picture n. A draw of n is paired with the draw
 * of n - 1 at the same place in the list if it is the same kind of draw (signature below); a pair whose model
 * would jump far on screen (a camera cut, a teleport) is not blended, and when most are like that no in-between
 * picture is shown at all. */
int gsInterp = -1;
static uint32_t sInterpBlkView[MAX_VU_UNIFORMS], sInterpPrevBlkView[MAX_VU_UNIFORMS]; /* the view each block is drawn in (a number made of its draws' scissor rectangle) */
static float sInterpPrevRep[MAX_VU_UNIFORMS][3];
static uint8_t sInterpFollow[MAX_VU_UNIFORMS]; /* the block is scenery that goes with the camera (the far backdrop) */
static double sInterpCamNow[3], sInterpCamWas[3]; /* where the camera stands, this picture and the previous one */
static double sInterpSceneWas[16];                /* the scenery's screen matrix of the previous picture */
static int sInterpHaveCam;
static uint16_t sInterpKind[MAX_VU_UNIFORMS], sInterpPrevKind[MAX_VU_UNIFORMS]; /* which vertex program and layer each block is drawn with */
static uint16_t sInterpBucket2[4096], sInterpChain2[MAX_VU_UNIFORMS];
static float sInterpCamMove; /* GS pixels the camera alone moved the scenery in view this tick (the middle value) */
static float sInterpRep[MAX_VU_UNIFORMS][3]; /* the first vertex of each block's mesh (model space) */
/* The camera's change of this picture (interp_build): previous clip position = sInterpD * this clip position, for
   anything that stands still in the world; and the numbers that turn GS pixels into that clip space. */
static float sInterpD[16], sInterpOffX, sInterpOffY, sInterpZMax;
static int sInterpHaveD;
static Vu0Uniform *sInterpPrev, *sInterpBlend;
static uint32_t *sInterpPrevSig, *sInterpSig;
static uint32_t sInterpPrevCount;
static int sInterpNext;  /* the tick's next picture still to be shown (2..sInterpSteps), 0 = none */
static int sInterpSteps = 2;
static unsigned sInterpLastBlank;
static uint64_t sInterpTickNs = 33366700ull; /* how long the tick being shown lasts */
static unsigned gInterpTicks;
static uint64_t sInterpBlendShown, sInterpBlendTook; /* when the in-between picture was handed to the screen; how long replaying its list took */
static struct { uint32_t verts, vuVerts, vuIdx, vuUni, draws; int anchor; unsigned skipped, native; unsigned tdraws[MAX_TARGETS]; } sInterpSaved;
unsigned gInterpShown, gInterpSkipped; /* in-between pictures shown / left out since the start */
static unsigned gInterpCuts, gInterpStatic, gInterpNotMutual;
static unsigned gInterpFollowed, gInterpThrough; /* ticks left without in-between pictures because the camera's way crossed the scenery */
static unsigned gInterpPaired, gInterpUnpaired, gInterpCamera; /* blocks blended / without a partner / of those, moved by the camera's change (BT3_INTERP_LOG) */

#define INTERP_SCISSOR_ID(d) ((((uint32_t)(d)->scissor[0] * 73856093u) ^ ((uint32_t)(d)->scissor[1] * 19349663u) ^ ((uint32_t)(d)->scissor[2] * 83492791u) ^ ((uint32_t)(d)->scissor[3] * 2971215073u)) | 1u)
/* What a uniform block draws, as a number that is the same in the next picture: the kind of each of its draws and
   the first vertices of each (they are in model space: the same mesh has the same numbers every frame, wherever it
   stands in the list; the stage's pieces come and go with what the camera sees, so the place in the list is no
   identity). */
static void interp_signatures(uint32_t *sig) {
    uint32_t n, k, j;

    memset(sig, 0, gsVuUniCount * sizeof(uint32_t));
    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        if (d->vu != 0 && d->uniform >= 0 && (uint32_t)d->uniform < gsVuUniCount) {
            uint32_t h = d->count * 2654435761u ^ (uint32_t)d->vu << 28; /* (not its texture or pipeline: a layer's texture can change every frame) */
            if (sig[d->uniform] == 0 && d->count != 0 && gsVuIdx[d->first] < gsVuVertCount) {
                memcpy(sInterpRep[d->uniform], &gsVuVerts[gsVuIdx[d->first] * 12], 3 * sizeof(float)); /* a point of the mesh */
            }
            for (k = 0; k < 4 && k < d->count; k++) {
                uint32_t v = gsVuIdx[d->first + k];
                if (v < gsVuVertCount) {
                    const uint32_t *w = (const uint32_t *)&gsVuVerts[v * 12];
                    for (j = 0; j < 12; j++) {
                        h = (h ^ w[j]) * 16777619u;
                    }
                }
            }
            sig[d->uniform] = (sig[d->uniform] * 31u + h) | 1u;
            sInterpKind[d->uniform] = (uint16_t)(d->vu * 64 + ((int)gsVuUni[d->uniform].misc[3] & 63));
            sInterpBlkView[d->uniform] = INTERP_SCISSOR_ID(d);
            if (d->vu == 3) {
                /* The ground under a fighter that carries its shadow (vertex program 6): its vertices are the piece
                   of ground the fighter stands over, new numbers every picture, so the vertices are no identity.
                   All of them count as the same kind; which fighter's it is, the nearest matrices decide. */
                sig[d->uniform] = 0x53484457u;
            }
        }
    }
}

/* Where the model's pivot lands on the GS screen (pixels), 0 if behind the eye. */
static int interp_screen_pos(const Vu0Uniform *u, float *x, float *y) {
    float p[4], s[4];
    int i;

    for (i = 0; i < 4; i++) { /* boneA * (0 - pivotA, 1) */
        p[i] = -(u->boneA[i] * u->pivotA[0] + u->boneA[4 + i] * u->pivotA[1] + u->boneA[8 + i] * u->pivotA[2]) + u->boneA[12 + i];
    }
    for (i = 0; i < 4; i++) {
        s[i] = u->screen[i] * p[0] + u->screen[4 + i] * p[1] + u->screen[8 + i] * p[2] + u->screen[12 + i];
    }
    if (!(s[3] > 0.0001f) && !(s[3] < -0.0001f)) {
        return 0;
    }
    *x = s[0] / s[3];
    *y = s[1] / s[3];
    return s[3] > 0.0f ? 1 : -1;
}

/* The camera's change, the part for the picture being built, applied to a screen matrix: in double precision. */
static void interp_camera_screen(const float *screen, float *o);

/* 4x4 matrices as the uniform block has them (columns). */
static void interp_mul(const float *a, const float *b, float *o) {
    int c, r;

    for (c = 0; c < 4; c++) {
        for (r = 0; r < 4; r++) {
            o[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
        }
    }
}

static int interp_inverse(const float *m, float *o) {
    double a[4][8];
    int i, j, k;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            a[i][j] = m[j * 4 + i];
            a[i][4 + j] = i == j;
        }
    }
    for (i = 0; i < 4; i++) {
        int piv = i;
        double d;
        for (k = i + 1; k < 4; k++) {
            if (fabs(a[k][i]) > fabs(a[piv][i])) { piv = k; }
        }
        if (fabs(a[piv][i]) < 1e-12) {
            return 0;
        }
        if (piv != i) {
            for (j = 0; j < 8; j++) { double x = a[i][j]; a[i][j] = a[piv][j]; a[piv][j] = x; }
        }
        d = a[i][i];
        for (j = 0; j < 8; j++) { a[i][j] /= d; }
        for (k = 0; k < 4; k++) {
            if (k != i) {
                d = a[k][i];
                for (j = 0; j < 8; j++) { a[k][j] -= d * a[i][j]; }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            o[j * 4 + i] = (float)a[i][4 + j];
        }
    }
    return 1;
}

/* Fills sInterpBlend for the frame just recorded; 0 = no in-between picture for this one.
   Each block of this picture is paired with the block of the previous picture that draws the same mesh; of several
   that do (a piece of scenery used more than once) the one whose matrices are nearest. */
static uint16_t sInterpBucket[4096];
static uint16_t sInterpChain[MAX_VU_UNIFORMS];
static uint8_t sInterpUsed[MAX_VU_UNIFORMS];
static int16_t sInterpPair[MAX_VU_UNIFORMS]; /* the previous picture's block each of this picture's was paired with, -1 none */

/* A point of the block's mesh (model space) in clip space: screen matrix * bone * (point - pivot). (A real point of
   the mesh, not the pivot or the origin: for scenery and for the sky's dome those can be beside or behind the eye,
   where a position on the screen means nothing.) */
static void interp_clip_pos(const Vu0Uniform *u, const float *v, float *s) {
    float p[4], m[3];
    int i;

    for (i = 0; i < 3; i++) {
        m[i] = v[i] - u->pivotA[i];
    }
    for (i = 0; i < 4; i++) {
        p[i] = u->boneA[i] * m[0] + u->boneA[4 + i] * m[1] + u->boneA[8 + i] * m[2] + u->boneA[12 + i];
    }
    for (i = 0; i < 4; i++) {
        s[i] = u->screen[i] * p[0] + u->screen[4 + i] * p[1] + u->screen[8 + i] * p[2] + u->screen[12 + i];
    }
}

/* The camera's change in parts: sInterpRoot[0] = its square root (half of the move), [1] = a quarter, [2] = an eighth.
   The picture a fraction of a tick before this one is then (the product of some of these) * this picture's screen
   matrix: the camera half way along its turn. (Half way between the two MATRICES is something else: a camera that
   turned by a good angle in one tick gives a picture that is too small half way, the average of two rotations
   being shorter than either. The scenery shrank in the in-between pictures of every fast turn; the sky came away
   from the edge of the screen and the cleared picture showed as a grey veil.) */
static double sInterpRoot[3][16];
static int sInterpRoots; /* they could be worked out */
static double sInterpDd[16], sInterpDtD[16]; /* the camera's change, and the part of it used for the picture being built, in double precision */
static float sInterpDt[16]; /* the camera's change over the part of the tick still to come at the picture being built */
static int sInterpHaveDt;

static int interp_inverse_d(const double *m, double *o) {
    double a[4][8];
    int i, j, k;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            a[i][j] = m[j * 4 + i];
            a[i][4 + j] = i == j;
        }
    }
    for (i = 0; i < 4; i++) {
        int piv = i;
        double d;
        for (k = i + 1; k < 4; k++) {
            if (fabs(a[k][i]) > fabs(a[piv][i])) { piv = k; }
        }
        if (fabs(a[piv][i]) < 1e-300) {
            return 0;
        }
        if (piv != i) {
            for (j = 0; j < 8; j++) { double x = a[i][j]; a[i][j] = a[piv][j]; a[piv][j] = x; }
        }
        d = a[i][i];
        for (j = 0; j < 8; j++) { a[i][j] /= d; }
        for (k = 0; k < 4; k++) {
            if (k != i) {
                d = a[k][i];
                for (j = 0; j < 8; j++) { a[k][j] -= d * a[i][j]; }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            o[j * 4 + i] = a[i][4 + j];
        }
    }
    return 1;
}

static void interp_mul_d(const double *a, const double *b, double *o) {
    int c, r;

    for (c = 0; c < 4; c++) {
        for (r = 0; r < 4; r++) {
            o[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
        }
    }
}

/* The square root of a matrix (Denman and Beavers' iteration): the matrix that, applied twice, is `a`. For the
   camera's change that is half of the camera's move. 0 = it did not settle (a turn of half a circle has none). */
static int interp_sqrt_d(const double *a, double *out) {
    double y[16], z[16], yi[16], zi[16], chk[16], err = 0.0, mag = 0.0;
    int i, it;

    memcpy(y, a, sizeof(y));
    for (i = 0; i < 16; i++) {
        z[i] = i % 5 == 0;
    }
    for (it = 0; it < 24; it++) {
        if (!interp_inverse_d(y, yi) || !interp_inverse_d(z, zi)) {
            return 0;
        }
        for (i = 0; i < 16; i++) {
            y[i] = 0.5 * (y[i] + zi[i]);
            z[i] = 0.5 * (z[i] + yi[i]);
        }
    }
    interp_mul_d(y, y, chk);
    for (i = 0; i < 16; i++) {
        err += fabs(chk[i] - a[i]);
        mag += fabs(a[i]);
        if (!(y[i] == y[i])) {
            return 0;
        }
    }
    if (!(err <= mag * 1e-6)) {
        return 0;
    }
    memcpy(out, y, sizeof(y));
    return 1;
}

static void interp_camera_screen(const float *screen, float *o) {
    double sc[16], r[16];
    int k;

    for (k = 0; k < 16; k++) {
        sc[k] = screen[k];
    }
    interp_mul_d(sInterpDtD, sc, r);
    for (k = 0; k < 16; k++) {
        o[k] = (float)r[k];
    }
}

/* Where the camera of a screen matrix stands in the world: the one point that it sends to "at the eye" (no x, no y,
   no w). 0 = it cannot be said. */
static int interp_camera_centre(const double *screen, double *c) {
    double inv[16];

    if (!interp_inverse_d(screen, inv) || fabs(inv[11]) < 1e-300) {
        return 0;
    }
    c[0] = inv[8] / inv[11];
    c[1] = inv[9] / inv[11];
    c[2] = inv[10] / inv[11];
    return c[0] == c[0] && c[1] == c[1] && c[2] == c[2];
}

/* A bone half way: the average of the two matrices, with each axis given back its length (the average of two
   directions is shorter than either), and, for a bone that holds the place as seen from the camera, the place
   moved around the camera and not through the chord. */
static void interp_bone(const float *p, const float *c, float t, int around, float *o) {
    int col, k;

    for (k = 0; k < 16; k++) {
        o[k] = p[k] + (c[k] - p[k]) * t;
    }
    for (col = 0; col < (around ? 4 : 3); col++) {
        const float *a = &p[col * 4], *b = &c[col * 4];
        float *m = &o[col * 4];
        float la = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]), lb = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
        float lm = sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]), want = la + (lb - la) * t;
        if (lm > 1e-12f && la > 1e-12f && lb > 1e-12f && lm > 0.3f * want) {
            float f = want / lm;
            m[0] *= f;
            m[1] *= f;
            m[2] *= f;
        }
    }
}

/* (of the view being worked on: interp_build_view) */
static uint32_t jumped, nscreens;
static float D[16];
static const float *screens[16];
static int agreed, bonesInView;
static uint32_t sInterpView; /* the view being worked on: the scissor id of its draws */
static int sInterpWasCut;
#define INTERP_CUT 300.0f /* GS pixels the scenery may move in a tick by the camera alone; more is a cut */

static int interp_build_view(float t, int first) {
    uint32_t i, paired = 0;
    float inv[16], chk[16];

    if (!first) {
        goto blend; /* (who goes with whom and how the camera moved was worked out for the tick's first picture) */
    }
    jumped = 0;
    agreed = 0;
    memset(sInterpBucket, 0xFF, sizeof(sInterpBucket));
    memset(sInterpUsed, 0, sInterpPrevCount);
    for (i = sInterpPrevCount; i-- > 0;) { /* (backwards: each chain lists its blocks in the order of the list) */
        uint32_t b = sInterpPrevSig[i] * 2654435761u >> 20;
        sInterpChain[i] = sInterpBucket[b];
        sInterpBucket[b] = (uint16_t)i;
    }
    /* 1. Each block's partner: the block of the previous picture that draws the same mesh. */
    for (i = 0; i < gsVuUniCount; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
        const Vu0Uniform *c = &gsVuUni[i];
        const float *cf = (const float *)c;
        float bestDist = 0.0f, bestDist2 = 0.0f;
        int k, best = -1;
        uint16_t j;

        sInterpBlend[i] = *c;
        sInterpPair[i] = -1;
        if (sInterpSig[i] == 0) {
            continue;
        }
        for (j = sInterpBucket[sInterpSig[i] * 2654435761u >> 20]; j != 0xFFFF; j = sInterpChain[j]) {
            /* (a shadow's strips are several blocks with one fighter's matrices, and their number changes with
               the ground: each takes the nearest of the previous picture, used before or not) */
            if (sInterpPrevSig[j] == sInterpSig[i] && sInterpPrevBlkView[j] == sInterpView && (!sInterpUsed[j] || sInterpSig[i] == 0x53484457u)) {
                /* Nearest first by where the mesh stands (bones and pivots), and only among equals by the screen
                   matrix. One sum over both let the camera decide: the sky is the same slice of a dome many times
                   over, and while the camera turned, the slice NEXT to a slice had the nearer screen matrix. */
                const float *pf = (const float *)&sInterpPrev[j];
                float dist = 0.0f, dist2 = 0.0f;
                for (k = 0; k < 40; k++) { /* bones, pivots */
                    dist += fabsf(pf[k] - cf[k]);
                }
                for (k = 40; k < 56; k++) { /* screen matrix */
                    dist2 += fabsf(pf[k] - cf[k]);
                }
                if (best < 0 || dist < bestDist || (dist == bestDist && dist2 < bestDist2)) {
                    best = j;
                    bestDist = dist;
                    bestDist2 = dist2;
                }
            }
        }
        if (best >= 0) {
            sInterpUsed[best] = 1;
            sInterpPair[i] = (int16_t)best;
        }
    }
    /* 1b. A second chance for a block without a partner. What a piece of scenery draws changes with what the camera
       sees of it (its strips are culled one by one), and then its vertices do not name it. Where it stands does: the
       block of the previous picture with the same program, layer and the very same bones is the same piece, or one
       that stands exactly like it, which blends to the same thing. For what stands nearly as before (the dome of
       the sky follows the camera) the nearest such block. (Left without partners, the backdrop was placed by the
       camera's change, which does not fit what follows the camera: it slid aside and the cleared picture showed
       through as a grey veil.) */
    {
        memset(sInterpBucket2, 0xFF, sizeof(sInterpBucket2));
        for (i = sInterpPrevCount; i-- > 0;) {
            const uint32_t *w = (const uint32_t *)sInterpPrev[i].boneA;
            uint32_t h = sInterpPrevKind[i], k2;
            for (k2 = 0; k2 < 40; k2++) {
                h = (h ^ w[k2]) * 16777619u;
            }
            h = h * 2654435761u >> 20;
            sInterpChain2[i] = sInterpBucket2[h];
            sInterpBucket2[h] = (uint16_t)i;
        }
        for (i = 0; i < gsVuUniCount; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
            const Vu0Uniform *c = &gsVuUni[i];
            const uint32_t *w = (const uint32_t *)c->boneA;
            const float *cf = (const float *)c;
            uint32_t h = sInterpKind[i], k2, j2;
            uint16_t j;
            float bestDist = 0.0f, mag = 0.0f;
            int best = -1;

            /* (not the scenery's program: it has no bones, so "stands the same" says nothing there; see 1c) */
            if (sInterpPair[i] >= 0 || sInterpSig[i] == 0 || sInterpSig[i] == 0x53484457u || (sInterpKind[i] >> 6) == 2) {
                continue;
            }
            for (k2 = 0; k2 < 40; k2++) {
                h = (h ^ w[k2]) * 16777619u;
            }
            for (j = sInterpBucket2[h * 2654435761u >> 20]; j != 0xFFFF; j = sInterpChain2[j]) {
                if (sInterpPrevKind[j] == sInterpKind[i] && sInterpPrevBlkView[j] == sInterpView && memcmp(sInterpPrev[j].boneA, c->boneA, 40 * sizeof(float)) == 0) {
                    best = j;
                    break;
                }
            }
            if (best < 0) {
                for (k2 = 0; k2 < 40; k2++) {
                    mag += fabsf(cf[k2]);
                }
                for (j2 = 0; j2 < sInterpPrevCount; j2++) {
                    const float *pf = (const float *)&sInterpPrev[j2];
                    float dist = 0.0f;
                    if (sInterpUsed[j2] || sInterpPrevKind[j2] != sInterpKind[i] || sInterpPrevBlkView[j2] != sInterpView) {
                        continue;
                    }
                    for (k2 = 0; k2 < 40 && dist <= mag * 0.05f; k2++) {
                        dist += fabsf(pf[k2] - cf[k2]);
                    }
                    if (dist <= mag * 0.05f && (best < 0 || dist < bestDist)) {
                        best = (int)j2;
                        bestDist = dist;
                    }
                }
            }
            if (first && best < 0 && i < 14 && getenv("BT3_INTERP_JUMPS") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_JUMPS"))) {
                float nd = 1e30f; int nj = -1, same = 0;
                for (j2 = 0; j2 < sInterpPrevCount; j2++) {
                    const float *pf = (const float *)&sInterpPrev[j2]; float dist = 0.0f;
                    if (sInterpPrevKind[j2] != sInterpKind[i]) { continue; }
                    same++;
                    for (k2 = 0; k2 < 40; k2++) { dist += fabsf(pf[k2] - cf[k2]); }
                    if (dist < nd) { nd = dist; nj = (int)j2; }
                }
                fprintf(stderr, "no partner for block %u: kind %u, %d of that kind before, nearest %d at distance %.3f (size %.3f), used %d; bone row3 %.2f %.2f %.2f; before %.2f %.2f %.2f\n", i, sInterpKind[i], same, nj, nd, mag,
                        nj >= 0 ? sInterpUsed[nj] : -1, c->boneA[12], c->boneA[13], c->boneA[14], nj >= 0 ? sInterpPrev[nj].boneA[12] : 0.0f, nj >= 0 ? sInterpPrev[nj].boneA[13] : 0.0f, nj >= 0 ? sInterpPrev[nj].boneA[14] : 0.0f);
            }
            if (best >= 0) {
                sInterpUsed[best] = 1;
                sInterpPair[i] = (int16_t)best;
            }
        }
    }
    /* 2. The camera's change. For an object standing still, screen matrix = camera * model, so (previous screen) =
       D * (this screen) with one D for all of them: D = previous * inverse(this) of any still object (a pair whose
       bones did not change). Taken from one such pair and accepted when another agrees; a few are tried. */
    {
        int tries = 0;
        uint32_t a, b;

        for (a = 0; a < gsVuUniCount && !agreed && tries < 8; a++) {
        if (sInterpBlkView[a] != sInterpView) { continue; } /* (one view at a time) */
            const Vu0Uniform *c = &gsVuUni[a], *q;
            if (sInterpPair[a] < 0) {
                continue;
            }
            q = &sInterpPrev[sInterpPair[a]];
            if (memcmp(c->boneA, q->boneA, 40 * sizeof(float)) != 0 || !interp_inverse(c->screen, inv)) {
                continue;
            }
            interp_mul(q->screen, inv, D);
            tries++;
            for (b = a + 1; b < gsVuUniCount; b++) {
        if (sInterpBlkView[b] != sInterpView) { continue; } /* (one view at a time) */
                const Vu0Uniform *c2 = &gsVuUni[b], *q2;
                float err = 0.0f, mag = 0.0f;
                int k;
                if (sInterpPair[b] < 0) {
                    continue;
                }
                q2 = &sInterpPrev[sInterpPair[b]];
                if (memcmp(c2->boneA, q2->boneA, 40 * sizeof(float)) != 0) {
                    continue;
                }
                interp_mul(D, c2->screen, chk);
                for (k = 0; k < 16; k++) {
                    err += fabsf(chk[k] - q2->screen[k]);
                    mag += fabsf(q2->screen[k]);
                }
                if (err <= mag * 0.002f) {
                    agreed = 1;
                    {
                        /* The same once more in double precision, and that is the one used. A screen matrix is badly
                           conditioned (a projection), and its inverse in single precision is good to a part in a
                           thousand at best: enough to tell that two pieces share a camera, not enough to draw with.
                           (Scenery placed with it had corners near the eye on the wrong side of it: sheets over the
                           whole screen, a grey veil.) */
                        double sc[16], sp[16], si[16], dd[16];
                        int k3;
                        for (k3 = 0; k3 < 16; k3++) {
                            sc[k3] = c->screen[k3];
                            sp[k3] = q->screen[k3];
                        }
                        memcpy(sInterpSceneWas, sp, sizeof(sp));
                        sInterpHaveCam = interp_camera_centre(sc, sInterpCamNow) && interp_camera_centre(sp, sInterpCamWas);
                        if (interp_inverse_d(sc, si)) {
                            interp_mul_d(sp, si, dd);
                            for (k3 = 0; k3 < 16; k3++) {
                                sInterpDd[k3] = dd[k3];
                                D[k3] = (float)dd[k3];
                            }
                        } else {
                            for (k3 = 0; k3 < 16; k3++) {
                                sInterpDd[k3] = D[k3];
                            }
                        }
                    }
                    memcpy(sInterpD, D, sizeof(D));
                    sInterpOffX = c->misc[0];
                    sInterpOffY = c->misc[1];
                    sInterpZMax = c->misc[2];
                }
                break; /* (one witness per candidate) */
            }
        }
        sInterpHaveD = agreed && sInterpZMax > 0.0f;
        sInterpRoots = 0;
        if (agreed) {
            sInterpRoots = interp_sqrt_d(sInterpDd, sInterpRoot[0]) && interp_sqrt_d(sInterpRoot[0], sInterpRoot[1]) && interp_sqrt_d(sInterpRoot[1], sInterpRoot[2]);
        }
    }
    /* 2b. Scenery that goes with the camera. The far backdrop is drawn with the scenery's own screen matrix, but its
       vertices are made anew in every picture, around where the camera stands: it has no partner by its vertices,
       and the camera's change, which is right for what stands still, moves it out from around the camera (the
       camera looked at it from outside: a dark sheet over the whole picture, seen whenever it was "placed by the
       camera"). It is known by this: the previous picture has a block of the same kind whose first vertex is this
       one's, less the way the camera went. */
    memset(sInterpFollow, 0, gsVuUniCount);
    if (agreed && sInterpHaveCam) {
        float d[3], dl;
        d[0] = (float)(sInterpCamNow[0] - sInterpCamWas[0]);
        d[1] = (float)(sInterpCamNow[1] - sInterpCamWas[1]);
        d[2] = (float)(sInterpCamNow[2] - sInterpCamWas[2]);
        dl = fabsf(d[0]) + fabsf(d[1]) + fabsf(d[2]);
        for (i = 0; i < gsVuUniCount && dl > 1e-4f; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
            uint32_t j2;
            const float *r = sInterpRep[i];
            if (sInterpPair[i] >= 0 || sInterpSig[i] == 0 || (sInterpKind[i] >> 6) != 2) {
                continue;
            }
            if (i < 30 && getenv("BT3_INTERP_JUMPS") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_JUMPS"))) {
                float be = 1e30f; int bj = -1, cand = 0;
                for (j2 = 0; j2 < sInterpPrevCount; j2++) {
                    const float *q = sInterpPrevRep[j2]; float e;
                    if (sInterpPrevKind[j2] != sInterpKind[i]) { continue; }
                    cand += !sInterpUsed[j2];
                    e = fabsf(r[0] - q[0] - d[0]) + fabsf(r[1] - q[1] - d[1]) + fabsf(r[2] - q[2] - d[2]);
                    if (e < be) { be = e; bj = (int)j2; }
                }
                fprintf(stderr, "follow? block %u first vertex %.3f %.3f %.3f; camera went %.4f %.4f %.4f; %d free blocks of the kind before; nearest (any) %d off by %.4f, its vertex %.3f %.3f %.3f, used %d\n", i, r[0], r[1], r[2], d[0], d[1], d[2], cand, bj, be,
                        bj >= 0 ? sInterpPrevRep[bj][0] : 0.0f, bj >= 0 ? sInterpPrevRep[bj][1] : 0.0f, bj >= 0 ? sInterpPrevRep[bj][2] : 0.0f, bj >= 0 ? sInterpUsed[bj] : -1);
            }
            for (j2 = 0; j2 < sInterpPrevCount; j2++) {
                const float *q = sInterpPrevRep[j2];
                float e;
                if (sInterpUsed[j2] || sInterpPrevKind[j2] != sInterpKind[i] || sInterpPrevBlkView[j2] != sInterpView) {
                    continue;
                }
                e = fabsf(r[0] - q[0] - d[0]) + fabsf(r[1] - q[1] - d[1]) + fabsf(r[2] - q[2] - d[2]);
                if (e < 0.02f * dl + 1e-3f * (fabsf(r[0]) + fabsf(r[1]) + fabsf(r[2])) && e < 0.5f * dl) {
                    sInterpFollow[i] = 1;
                    break;
                }
            }
        }
    }
    /* 3. What moved too far to be the same thing a tick later. With the camera's change known, "far" is measured
       after taking the camera out: where the block's pivot would have been if only the camera had moved, against
       where its partner really was. (Measured on the screen as it is, a fast turn of the camera made everything
       "jump": nothing was blended while spinning, which showed as hitches and as things out of step.) A camera that
       itself moved the scenery by most of the screen is a cut: no in-between pictures for this tick. */
    {
        uint32_t still = 0, far = 0, nmove = 0, movN = 0, movBig = 0;
        static float move[256];
        float cam = 0.0f;

        /* how far the camera alone moved the scenery on the screen, typically: the middle of what the still blocks
           in view moved */
        for (i = 0; agreed && i < gsVuUniCount && nmove < 256; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
            const Vu0Uniform *c = &gsVuUni[i], *q;
            float sc[4], sp[4];
            if (sInterpPair[i] < 0) {
                continue;
            }
            q = &sInterpPrev[sInterpPair[i]];
            if (memcmp(c->boneA, q->boneA, 40 * sizeof(float)) != 0) {
                continue;
            }
            interp_clip_pos(c, sInterpRep[i], sc);
            interp_clip_pos(q, sInterpRep[i], sp);
            if (sc[3] > 1.0f && sp[3] > 1.0f && fabsf(sc[0] / sc[3]) < 4096.0f && fabsf(sc[1] / sc[3]) < 4096.0f) {
                float dx = fabsf(sc[0] / sc[3] - sp[0] / sp[3]), dy = fabsf(sc[1] / sc[3] - sp[1] / sp[3]);
                move[nmove++] = dx > dy ? dx : dy;
            }
        }
        /* The same from the scenery's own vertices: a few hundred of them, those in view now, and where the camera's
           change says they were. (The first vertex of each piece, as above, is seldom in view: with nothing to go
           by, the fighters were asked whether a tick is a cut, and one that dashed past the camera said yes.) */
        if (agreed) {
            uint32_t n2, total = 0, stepV;
            for (n2 = 0; n2 < gsDrawCount; n2++) {
                total += gsDraws[n2].vu == 2 && gsDraws[n2].uniform >= 0 && (uint32_t)gsDraws[n2].uniform < gsVuUniCount && sInterpBlkView[gsDraws[n2].uniform] == sInterpView ? gsDraws[n2].count : 0;
            }
            stepV = total / 400 ? total / 400 : 1;
            nmove = 0;
            for (n2 = 0; n2 < gsDrawCount; n2++) {
                const GsDraw *d = &gsDraws[n2];
                const Vu0Uniform *c;
                uint32_t k2;
                if (d->vu != 2 || d->uniform < 0 || (uint32_t)d->uniform >= gsVuUniCount || sInterpBlkView[d->uniform] != sInterpView) {
                    continue;
                }
                c = &gsVuUni[d->uniform];
                for (k2 = (n2 * 7) % stepV; k2 < d->count; k2 += stepV) {
                    const float *v = &gsVuVerts[gsVuIdx[d->first + k2] * 12];
                    float sc[4], sp[4], x, y;
                    int k3;
                    for (k3 = 0; k3 < 4; k3++) {
                        sc[k3] = c->screen[k3] * v[0] + c->screen[4 + k3] * v[1] + c->screen[8 + k3] * v[2] + c->screen[12 + k3];
                    }
                    if (!(sc[3] > 1.0f)) {
                        continue;
                    }
                    x = sc[0] / sc[3] - c->misc[0];
                    y = sc[1] / sc[3] - c->misc[1];
                    if (x < 0.0f || x > 640.0f || y < 0.0f || y > 448.0f) {
                        continue;
                    }
                    for (k3 = 0; k3 < 4; k3++) {
                        sp[k3] = D[k3] * sc[0] + D[4 + k3] * sc[1] + D[8 + k3] * sc[2] + D[12 + k3] * sc[3];
                    }
                    still++;
                    if (!(sp[3] > 1.0f)) {
                        far++;
                    } else {
                        float dx = fabsf(sc[0] / sc[3] - sp[0] / sp[3]), dy = fabsf(sc[1] / sc[3] - sp[1] / sp[3]);
                        far += dx > INTERP_CUT || dy > INTERP_CUT;
                        if (nmove < 256) {
                            move[nmove++] = dx > dy ? dx : dy;
                        }
                    }
                }
            }
        }
        if (nmove != 0) {
            uint32_t a2, b2;
            for (a2 = 1; a2 < nmove; a2++) { /* (insertion sort: at most 256) */
                float v = move[a2];
                for (b2 = a2; b2 > 0 && move[b2 - 1] > v; b2--) { move[b2] = move[b2 - 1]; }
                move[b2] = v;
            }
            cam = move[nmove / 2];
        }
        sInterpCamMove = cam;

        for (i = 0; i < gsVuUniCount; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
            const Vu0Uniform *c = &gsVuUni[i], *q;
            float sc[4], sp[4], pr[4];
            int k, bad = 0;

            if (sInterpPair[i] < 0) {
                continue;
            }
            q = &sInterpPrev[sInterpPair[i]];
            interp_clip_pos(c, sInterpRep[i], sc);
            interp_clip_pos(q, sInterpRep[i], sp);
            /* (The camera's change D cannot be applied to a fighter's position: the fighters are drawn through
               another projection than the scenery, and D is in the scenery's. Tried: it put them far away.) What
               moves on its own is judged on the screen as it is, with room for what the camera does to the picture:
               a fighter the camera swings past moves as far as the scenery behind it. */
            memcpy(pr, sc, sizeof(pr));
            (void)k;
            if (memcmp(c->boneA, q->boneA, 40 * sizeof(float)) == 0) {
                /* It stands where it stood: it did not jump, whatever the numbers say (the origin of a piece of
                   scenery can be anywhere, also beside or behind the eye, where a position on the screen is a
                   quotient of two small numbers). Those in view now tell how far the camera moved the scenery: one
                   that was behind the eye a tick ago, or most of a screen away, speaks for a cut. */

            } else if (sInterpSig[i] != 0x53484457u && c->misc[3] <= 1.0f && sc[3] > 1.0f) {
                /* a part of a fighter: did it go far, or to quite another distance? */
                movN++;
                movBig += !(sp[3] > 1.0f) || fabsf(sc[0] / sc[3] - sp[0] / sp[3]) > 160.0f || fabsf(sc[1] / sc[3] - sp[1] / sp[3]) > 120.0f || sc[3] > sp[3] * 1.7f || sp[3] > sc[3] * 1.7f;
            }
            if (memcmp(c->boneA, q->boneA, 40 * sizeof(float)) == 0) {
            } else if (sp[3] > 1.0f && pr[3] > 1.0f && sc[3] > 1.0f && sInterpSig[i] != 0x53484457u) {
                /* (a point behind the eye in either picture is not judged: what is there is not seen) */
                /* Most of the screen in one tick. (It was a third of it: a fighter close to the camera that dashes
                   off moves further than that, was taken for a jump with all its parts, and, being half of all
                   blocks, made the whole tick go without in-between pictures: a hitch of everything at each such
                   moment. A short teleport is now blended, as one picture half way.) */
                bad = fabsf(sp[0] / sp[3] - pr[0] / pr[3]) > 350.0f + 1.5f * cam || fabsf(sp[1] / sp[3] - pr[1] / pr[3]) > 300.0f + 1.5f * cam;
            }
            if (bad && first && getenv("BT3_INTERP_JUMPS") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_JUMPS"))) {
                fprintf(stderr, "jump block %u layer %.0f now %.0f,%.0f w %.1f; was %.0f,%.0f w %.1f; by the camera alone %.0f,%.0f w %.1f; pivot %.1f %.1f %.1f point %.1f %.1f %.1f\n", i, c->misc[3],
                        sc[0] / sc[3], sc[1] / sc[3], sc[3], sp[0] / sp[3], sp[1] / sp[3], sp[3], pr[0] / pr[3], pr[1] / pr[3], pr[3], c->pivotA[0], c->pivotA[1], c->pivotA[2],
                        sInterpRep[i][0], sInterpRep[i][1], sInterpRep[i][2]);
            }
            if (bad) {
                sInterpPair[i] = -1;
                jumped++;
            }
        }
        /* A cut: the scenery in view says so; where there is none to ask (a transformation's or an ultimate's own
           scenes before a backdrop), the fighters do. (With scenery in view the fighters are not asked: one that
           dashes past the camera goes far and to another distance in one tick, and that is no cut.) */
        if (getenv("BT3_INTERP_CUTLOG") != NULL) {
            fprintf(stderr, "cut? frame %u: agreed %d, still in view %u, far %u, camera move %.0f; fighter parts %u, big %u; blocks %u, paired %u\n", gGsFrame, agreed, still, far, cam, movN, movBig, gsVuUniCount,
                    (unsigned)0);
        }
        /* (also: the scenery as a whole went by more than a third of the screen, which no turn of the camera does
           in a thirtieth of a second, the pieces in view or not; and nearly every part of the fighters went far
           while the scenery is no witness or moved a good deal too: the shots of a transformation or an ultimate
           with a little scenery in a corner) */
        if ((still >= 8 ? far * 2 > still || cam > 250.0f : movN >= 6 && movBig * 2 > movN) ||
            (movN >= 6 && movBig * 10 >= movN * 9 && cam > 120.0f)) {
            gInterpCuts++;
            sInterpWasCut = 1;
            return 0;
        }
    }
    /* Do the bones of what moves hold places as seen from the camera (the fighters' do: their screen matrix is the
       same in every picture, the camera is in their bones)? Then a place half way is taken around the camera. */
    bonesInView = 0;
    {
        uint32_t same = 0, other = 0;
        for (i = 0; agreed && i < gsVuUniCount; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
            const Vu0Uniform *c = &gsVuUni[i], *q;
            if (sInterpPair[i] < 0 || sInterpSig[i] == 0x53484457u || c->misc[3] > 1.0f) {
                continue;
            }
            q = &sInterpPrev[sInterpPair[i]];
            if (memcmp(c->boneA, q->boneA, 16 * sizeof(float)) != 0) {
                if (memcmp(c->screen, q->screen, 64) == 0) { same++; } else { other++; }
            }
        }
        bonesInView = same > 8 && same > other * 4 && sInterpCamMove > 0.5f;
    }
    /* The screen matrices the standing scenery is drawn with (a stage uses one or a few for all its pieces): what
       the camera's change may be applied to. */
    nscreens = 0;
    for (i = 0; agreed && i < gsVuUniCount && nscreens < 16; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
        const Vu0Uniform *c = &gsVuUni[i];
        uint32_t k2;
        if (sInterpPair[i] < 0 || memcmp(c->boneA, sInterpPrev[sInterpPair[i]].boneA, 40 * sizeof(float)) != 0) {
            continue;
        }
        for (k2 = 0; k2 < nscreens && memcmp(screens[k2], c->screen, 64) != 0; k2++) {
        }
        if (k2 == nscreens) {
            screens[nscreens++] = c->screen;
        }
    }
    /* 3b. Does the camera go THROUGH the scenery between the two pictures? Its two places are both good, but the
       straight way between them need not be: over a rise of the ground, or past the corner of a rock, the camera of
       an in-between picture stands inside the ground and looks at it from below, a dark sheet over the whole picture
       (seen in a low fly-by: the pictures a quarter and nine tenths of the way were right, those between were not).
       So: if the way from the previous place to this one crosses a triangle of the scenery, the tick gets no
       in-between pictures. */
    if (agreed && sInterpHaveCam && getenv("BT3_INTERP_THROUGH") == NULL) {
        float a0[3], d0[3], lo3[3], hi3[3], len;
        uint32_t n2;
        int k3, hit = 0;

        for (k3 = 0; k3 < 3; k3++) {
            a0[k3] = (float)sInterpCamWas[k3];
            d0[k3] = (float)(sInterpCamNow[k3] - sInterpCamWas[k3]);
            lo3[k3] = (a0[k3] < a0[k3] + d0[k3] ? a0[k3] : a0[k3] + d0[k3]) - 0.01f;
            hi3[k3] = (a0[k3] > a0[k3] + d0[k3] ? a0[k3] : a0[k3] + d0[k3]) + 0.01f;
        }
        len = fabsf(d0[0]) + fabsf(d0[1]) + fabsf(d0[2]);
        for (n2 = 0; len > 0.05f && n2 < gsDrawCount && !hit; n2++) {
            const GsDraw *d = &gsDraws[n2];
            uint32_t k2, tri;
            if (d->vu != 2 || d->uniform < 0 || (uint32_t)d->uniform >= gsVuUniCount || sInterpBlkView[d->uniform] != sInterpView) {
                continue;
            }
            for (k2 = 0; k2 < nscreens && memcmp(screens[k2], gsVuUni[d->uniform].screen, 64) != 0; k2++) {
            }
            if (k2 == nscreens) {
                continue;
            }
            for (tri = 0; tri + 3 <= d->count && !hit; tri += 3) {
                const float *v0 = &gsVuVerts[gsVuIdx[d->first + tri] * 12], *v1 = &gsVuVerts[gsVuIdx[d->first + tri + 1] * 12],
                            *v2 = &gsVuVerts[gsVuIdx[d->first + tri + 2] * 12];
                float e1[3], e2[3], pv[3], tv[3], qv[3], det, u, v, tt;
                int out = 0;
                for (k3 = 0; k3 < 3 && !out; k3++) { /* the triangle's box against the way's box */
                    out = (v0[k3] < lo3[k3] && v1[k3] < lo3[k3] && v2[k3] < lo3[k3]) || (v0[k3] > hi3[k3] && v1[k3] > hi3[k3] && v2[k3] > hi3[k3]);
                }
                if (out) {
                    continue;
                }
                for (k3 = 0; k3 < 3; k3++) {
                    e1[k3] = v1[k3] - v0[k3];
                    e2[k3] = v2[k3] - v0[k3];
                    tv[k3] = a0[k3] - v0[k3];
                }
                pv[0] = d0[1] * e2[2] - d0[2] * e2[1];
                pv[1] = d0[2] * e2[0] - d0[0] * e2[2];
                pv[2] = d0[0] * e2[1] - d0[1] * e2[0];
                det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
                if (fabsf(det) < 1e-12f) {
                    continue;
                }
                u = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) / det;
                if (u < 0.0f || u > 1.0f) {
                    continue;
                }
                qv[0] = tv[1] * e1[2] - tv[2] * e1[1];
                qv[1] = tv[2] * e1[0] - tv[0] * e1[2];
                qv[2] = tv[0] * e1[1] - tv[1] * e1[0];
                v = (d0[0] * qv[0] + d0[1] * qv[1] + d0[2] * qv[2]) / det;
                if (v < 0.0f || u + v > 1.0f) {
                    continue;
                }
                tt = (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2]) / det;
                hit = tt > 0.0f && tt < 1.0f;
            }
        }
        if (hit) {
            gInterpThrough++;
            sInterpWasCut = 1;
            return 0;
        }
    }
    /* 4. The blend. */
blend:
    sInterpHaveDt = 0;
    if (agreed && sInterpRoots && getenv("BT3_INTERP_LINEAR") == NULL) {
        /* the camera's change over the rest of the tick, 1 - t of it, from its halves, quarters and eighths */
        float left = (1.0f - t) * 8.0f;
        int m = (int)(left + 0.5f), bit;
        if (m >= 1 && m <= 7 && fabsf(left - (float)m) < 0.01f) {
            double acc[16], tmp[16];
            int k, have = 0;
            for (bit = 0; bit < 3; bit++) {
                if (m & (4 >> bit)) {
                    if (!have) {
                        memcpy(acc, sInterpRoot[bit], sizeof(acc));
                        have = 1;
                    } else {
                        interp_mul_d(acc, sInterpRoot[bit], tmp);
                        memcpy(acc, tmp, sizeof(acc));
                    }
                }
            }
            for (k = 0; k < 16; k++) {
                sInterpDt[k] = (float)acc[k];
                sInterpDtD[k] = acc[k];
            }
            sInterpHaveDt = 1;
        }
    }
    for (i = 0; i < gsVuUniCount; i++) {
        if (sInterpBlkView[i] != sInterpView) { continue; } /* (one view at a time) */
        const Vu0Uniform *c = &gsVuUni[i];
        Vu0Uniform *o = &sInterpBlend[i];
        int k, scenery = 0, part = 0;

        if (agreed) {
            uint32_t k2;
            for (k2 = 0; k2 < nscreens && memcmp(screens[k2], c->screen, 64) != 0; k2++) {
            }
            scenery = k2 < nscreens;
        }

        if (first && i >= 10 && i <= 42 && getenv("BT3_INTERP_JUMPS") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_JUMPS"))) {
            const Vu0Uniform *q = sInterpPair[i] >= 0 ? &sInterpPrev[sInterpPair[i]] : NULL;
            float sc[4], sp[4] = {0, 0, 0, 0};
            uint32_t n2, tris = 0; int pipe = -1, target = -1;
            for (n2 = 0; n2 < gsDrawCount; n2++) {
                if (gsDraws[n2].vu != 0 && (uint32_t)gsDraws[n2].uniform == i) { tris += gsDraws[n2].count / 3; pipe = gsDraws[n2].pipeline; target = gsDraws[n2].target; }
            }
            interp_clip_pos(c, sInterpRep[i], sc);
            if (q != NULL) { interp_clip_pos(q, sInterpRep[i], sp); }
            for (n2 = 0; n2 < gsDrawCount; n2++) {
                if (gsDraws[n2].vu != 0 && (uint32_t)gsDraws[n2].uniform == i) {
                    const float *v0 = &gsVuVerts[gsVuIdx[gsDraws[n2].first] * 12], *v1 = &gsVuVerts[gsVuIdx[gsDraws[n2].first + gsDraws[n2].count - 1] * 12];
                    fprintf(stderr, "  block %u draw %u: %u indices, tex %lx target-tex %d sampler %d mode %d %d %d %d misc %.1f %.1f %.1f %.1f blendc %.2f | v0 %.2f %.2f %.2f %.3f col %.0f %.0f %.0f %.0f | vlast %.2f %.2f %.2f %.3f\n", i, n2, gsDraws[n2].count,
                            (unsigned long)gsDraws[n2].tex, gsDraws[n2].tex_is_target, gsDraws[n2].sampler, gsDraws[n2].mode[0], gsDraws[n2].mode[1], gsDraws[n2].mode[2], gsDraws[n2].mode[3],
                            gsDraws[n2].misc[0], gsDraws[n2].misc[1], gsDraws[n2].misc[2], gsDraws[n2].misc[3], gsDraws[n2].blendc, v0[0], v0[1], v0[2], v0[3], v0[4], v0[5], v0[6], v0[7], v1[0], v1[1], v1[2], v1[3]);
                    break;
                }
            }
            fprintf(stderr, "block %u kind %u sig %08x pair %d scenery %d tris %u pipe %d target %d | now %.0f,%.0f w %.2f | was %.0f,%.0f w %.2f | colour %.0f %.0f %.0f %.0f\n", i, sInterpKind[i], sInterpSig[i], sInterpPair[i], scenery, tris, pipe, target,
                    sc[0] / sc[3], sc[1] / sc[3], sc[3], q ? sp[0] / sp[3] : 0.0f, q ? sp[1] / sp[3] : 0.0f, sp[3], c->color0[0], c->color0[1], c->color0[2], c->color0[3]);
        }
        if (getenv("BT3_INTERP_ONE") != NULL) { /* testing: leave the blocks from..to as picture n has them */
            int lo2 = 0, hi2 = -1;
            sscanf(getenv("BT3_INTERP_ONE"), "%d-%d", &lo2, &hi2);
            if ((int)i >= lo2 && (int)i <= hi2 && getenv("BT3_INTERP_PART") == NULL) {
                continue;
            }
            part = (int)i >= lo2 && (int)i <= hi2 ? atoi(getenv("BT3_INTERP_PART")) : 0;
        }
        if (getenv("BT3_INTERP_SKIP") != NULL) { /* testing: leave a kind of block as picture n has it */
            int what = atoi(getenv("BT3_INTERP_SKIP"));
            if ((what == 1 && sInterpSig[i] == 0x53484457u) || (what == 2 && c->misc[3] == 2.0f) || (what == 3 && c->misc[3] <= 1.0f && sInterpSig[i] != 0x53484457u) ||
                (what == 4 && c->misc[3] >= 3.0f)) {
                continue;
            }
            if ((what == 5 || what == 6) && sInterpPair[i] >= 0 && c->misc[3] <= 1.0f && sInterpSig[i] != 0x53484457u &&
                (what == 6) == (memcmp(c->boneA, sInterpPrev[sInterpPair[i]].boneA, 40 * sizeof(float)) == 0)) {
                continue;
            }
            if (what == 7 && sInterpPair[i] < 0) {
                continue;
            }
        }
        if (sInterpFollow[i]) {
            /* with the camera: the scenery's matrix of this in-between picture, and the piece moved to where that
               picture's camera stands */
            double m[16], ct[3], dd3[3], col[4];
            int ok;
            if (sInterpHaveDt) {
                double sc[16];
                for (k = 0; k < 16; k++) { sc[k] = c->screen[k]; }
                interp_mul_d(sInterpDtD, sc, m);
            } else {
                for (k = 0; k < 16; k++) { m[k] = sInterpSceneWas[k] + ((double)c->screen[k] - sInterpSceneWas[k]) * t; }
            }
            ok = interp_camera_centre(m, ct);
            if (ok) {
                dd3[0] = ct[0] - sInterpCamNow[0];
                dd3[1] = ct[1] - sInterpCamNow[1];
                dd3[2] = ct[2] - sInterpCamNow[2];
                for (k = 0; k < 4; k++) {
                    col[k] = m[k] * dd3[0] + m[4 + k] * dd3[1] + m[8 + k] * dd3[2] + m[12 + k];
                }
                for (k = 0; k < 12; k++) { o->screen[k] = (float)m[k]; }
                for (k = 0; k < 4; k++) { o->screen[12 + k] = (float)col[k]; }
                gInterpFollowed++;
            }
            continue;
        }
        if (sInterpPair[i] >= 0) {
            const Vu0Uniform *q = &sInterpPrev[sInterpPair[i]];
            const float *pf = (const float *)q, *cf = (const float *)c;
            float *of = (float *)o;
            /* boneA, boneB, pivotA, pivotB, screen: the first 56 floats; colours and misc stay picture n's. The four
               after them are a fighter's light direction, which turns with it, and for the scenery's program
               something else: how bright the piece is drawn. A piece of scenery paired with another one that stands
               like it (the second chance above) got the average of two brightnesses: the grey veil over a whole
               in-between picture, whenever the pieces on screen changed. */
            for (k = 0; k < ((sInterpKind[i] >> 6) == 1 && c->misc[3] <= 1.0f ? 60 : 56); k++) {
                of[k] = pf[k] + (cf[k] - pf[k]) * t;
            }
            if (memcmp(c->boneA, q->boneA, 40 * sizeof(float)) == 0) {
                /* it stands as it stood: its bones are this picture's to the last bit (pieces of scenery laid over
                   each other must come out at exactly the same depth) */
                memcpy(o->boneA, c->boneA, 40 * sizeof(float));
            } else if (sInterpSig[i] != 0x53484457u && (c->misc[3] <= 1.0f || c->misc[3] == 5.0f)) { /* (a model's two bones: not the shadow's and other programs' use of these slots) */
                /* Layer 5 is the reflection of a shiny model (program 1's second texture), drawn over the same
                   triangles as its layer 0. With the plain average of the matrices above, its triangles were a
                   little smaller and elsewhere than the layer's under it, in the in-between pictures only: the
                   two fought for the depth and the shine came and went with every picture (Perfect Cell's black
                   parts; seen by the user, reproduced from a recording). Both go the same way now. */
                int around = bonesInView && !scenery && memcmp(c->screen, q->screen, 64) == 0;
                interp_bone(q->boneA, c->boneA, t, around, o->boneA);
                interp_bone(q->boneB, c->boneB, t, around, o->boneB);
            }
            if (scenery && sInterpHaveDt) {
                interp_camera_screen(c->screen, o->screen); /* the camera part of the way, not the matrices' average */
            }
            paired++;
        } else if (agreed && sInterpSig[i] != 0) {
            /* No partner (a piece of scenery that came into view, a mesh whose strips changed, something that
               jumped): it would stand ahead of its blended neighbours, a seam. What moved for it is at least the
               camera. Only for what is drawn with the scenery's own screen matrix: the camera's change is in that
               projection, and a block in another one (a fighter's, an effect's) comes out anywhere. (Seen: a grey
               veil over a whole in-between picture as a fighter flew past the camera.) */
            if (!scenery) {
                continue;
            }
            if (first && getenv("BT3_INTERP_JUMPS") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_JUMPS"))) {
                uint32_t n2, tris = 0, draws = 0;
                int vu = 0, pipe = 0;
                for (n2 = 0; n2 < gsDrawCount; n2++) {
                    if (gsDraws[n2].vu != 0 && (uint32_t)gsDraws[n2].uniform == i) { tris += gsDraws[n2].count / 3; draws++; vu = gsDraws[n2].vu; pipe = gsDraws[n2].pipeline; }
                }
                fprintf(stderr, "camera-placed block %u: program kind %d, layer %.0f, %u draws, %u triangles, pipeline %d, colour %.0f %.0f %.0f %.0f, bone moves %d\n", i, vu, c->misc[3], draws, tris, pipe,
                        c->color0[0], c->color0[1], c->color0[2], c->color0[3], c->boneA[12] != 0.0f || c->boneA[13] != 0.0f);
            }
            if (getenv("BT3_INTERP_ONE") != NULL && (int)i == atoi(getenv("BT3_INTERP_ONE"))) {
                continue;
            }
            if (sInterpHaveDt) {
                interp_camera_screen(c->screen, o->screen);
            } else {
                interp_mul(D, c->screen, chk);
                for (k = 0; k < 16; k++) {
                    o->screen[k] = chk[k] + (c->screen[k] - chk[k]) * t;
                }
            }
            gInterpCamera++;
        }
        if (part == 1) { /* testing: only the screen matrix of these blocks is blended */
            memcpy(o->boneA, c->boneA, 40 * sizeof(float));
            memcpy(o->light, c->light, 16);
        } else if (part == 2) { /* only bones and pivots */
            memcpy(o->screen, c->screen, 64);
            memcpy(o->light, c->light, 16);
        } else if (part == 3) { /* only the four after the screen matrix */
            memcpy(o->boneA, c->boneA, 40 * sizeof(float));
            memcpy(o->screen, c->screen, 64);
        }
    }
    if (first && getenv("BT3_INTERP_FRAMES") != NULL) { /* testing: one line per picture */
        static FILE *fp;
        extern unsigned gPortVBlanks;
        if (fp == NULL) { fp = fopen(getenv("BT3_INTERP_FRAMES"), "w"); }
        if (fp != NULL) {
            fprintf(fp, "%u %u blocks %u paired %u jumped %u previous %u camera %d\n", gPortVBlanks, gGsFrame, gsVuUniCount, paired, jumped, sInterpPrevCount, agreed * 100 + (int)nscreens);
            { uint32_t f2 = 0, i2; for (i2 = 0; i2 < gsVuUniCount; i2++) { f2 += sInterpFollow[i2]; } fprintf(fp, "  follow %u\n", f2); }
        }
    }
    gInterpPaired += paired;
    gInterpUnpaired += gsVuUniCount - paired;
    /* (Whether the tick is a cut was decided from the still scenery above. Only where there is none, a fighter alone
       before a backdrop, do many jumps mean one.) */
    return paired != 0 && (agreed || jumped * 2 <= paired);
}

/* The views of the picture: split screen has two, each with its own camera, and the rendering of a fighter's shadow
   is one of its own. A view is told by the scissor rectangle its draws are made with. Everything above is done for
   one view at a time (sInterpView), with what it found about that view's camera kept from the tick's first picture
   to its later ones. (Done for the picture as a whole, the second view of a split screen was given the first
   view's camera: none of what depends on the camera worked in it.) */
#define INTERP_MAX_VIEWS 8
typedef struct InterpViewState {
    uint32_t id;
    int ok, cut, scenery;
    float D[16], offX, offY, zMax, Dt[16], camMove;
    double root[3][16], Dd[16], DtD[16], camNow[3], camWas[3], sceneWas[16];
    int haveD, roots, haveDt, haveCam, agreed, bonesInView;
    uint32_t jumped, nscreens;
    const float *screens[16];
} InterpViewState;
static InterpViewState sInterpViews[INTERP_MAX_VIEWS];
static int sInterpViewCount, sInterpViewLoaded = -1;

static void interp_view_save(InterpViewState *v) {
    memcpy(v->D, D, sizeof(D));
    v->offX = sInterpOffX; v->offY = sInterpOffY; v->zMax = sInterpZMax; v->camMove = sInterpCamMove;
    memcpy(v->Dt, sInterpDt, sizeof(v->Dt));
    memcpy(v->root, sInterpRoot, sizeof(v->root));
    memcpy(v->Dd, sInterpDd, sizeof(v->Dd));
    memcpy(v->DtD, sInterpDtD, sizeof(v->DtD));
    memcpy(v->camNow, sInterpCamNow, sizeof(v->camNow));
    memcpy(v->camWas, sInterpCamWas, sizeof(v->camWas));
    memcpy(v->sceneWas, sInterpSceneWas, sizeof(v->sceneWas));
    v->haveD = sInterpHaveD; v->roots = sInterpRoots; v->haveDt = sInterpHaveDt; v->haveCam = sInterpHaveCam;
    v->agreed = agreed; v->bonesInView = bonesInView; v->jumped = jumped; v->nscreens = nscreens;
    memcpy(v->screens, screens, sizeof(v->screens));
}

static void interp_view_load(const InterpViewState *v) {
    memcpy(D, v->D, sizeof(D));
    memcpy(sInterpD, v->D, sizeof(sInterpD));
    sInterpOffX = v->offX; sInterpOffY = v->offY; sInterpZMax = v->zMax; sInterpCamMove = v->camMove;
    memcpy(sInterpDt, v->Dt, sizeof(v->Dt));
    memcpy(sInterpRoot, v->root, sizeof(v->root));
    memcpy(sInterpDd, v->Dd, sizeof(v->Dd));
    memcpy(sInterpDtD, v->DtD, sizeof(v->DtD));
    memcpy(sInterpCamNow, v->camNow, sizeof(v->camNow));
    memcpy(sInterpCamWas, v->camWas, sizeof(v->camWas));
    memcpy(sInterpSceneWas, v->sceneWas, sizeof(v->sceneWas));
    sInterpHaveD = v->haveD; sInterpRoots = v->roots; sInterpHaveDt = v->haveDt; sInterpHaveCam = v->haveCam;
    agreed = v->agreed; bonesInView = v->bonesInView; jumped = v->jumped; nscreens = v->nscreens;
    memcpy(screens, v->screens, sizeof(v->screens));
}

/* For the pieces (interp_build_verts): what is known of the camera of view `id`. 0 = the view has no in-between
   picture this tick (a cut in it): its pieces stay this picture's. */
static int interp_view_select(uint32_t id) {
    int k;

    if (sInterpViewLoaded >= 0 && sInterpViews[sInterpViewLoaded].id == id) {
        return !sInterpViews[sInterpViewLoaded].cut;
    }
    for (k = 0; k < sInterpViewCount; k++) {
        if (sInterpViews[k].id == id) {
            interp_view_load(&sInterpViews[k]);
            sInterpViewLoaded = k;
            return !sInterpViews[k].cut;
        }
    }
    /* a view without any block of a vertex program (the HUD's rectangle): no camera */
    sInterpHaveD = sInterpHaveDt = sInterpRoots = sInterpHaveCam = 0;
    sInterpCamMove = 0.0f;
    sInterpViewLoaded = -1;
    return 1;
}

static int interp_build(float t, int first) {
    uint32_t i;
    int k, okAny = 0, cutAny = 0, okScenery = 0;

    if (gsVuUniCount == 0 || sInterpPrevCount == 0) {
        return 0;
    }
    memcpy(sInterpBlend, gsVuUni, gsVuUniCount * sizeof(Vu0Uniform));
    if (first) {
        sInterpViewCount = 0;
        for (i = 0; i < gsVuUniCount; i++) {
            if (sInterpSig[i] == 0) {
                continue;
            }
            for (k = 0; k < sInterpViewCount && sInterpViews[k].id != sInterpBlkView[i]; k++) {
            }
            if (k == sInterpViewCount) {
                if (sInterpViewCount == INTERP_MAX_VIEWS) {
                    sInterpBlkView[i] = sInterpViews[0].id; /* (more views than there is room for: with the first) */
                    k = 0;
                } else {
                    memset(&sInterpViews[k], 0, sizeof(sInterpViews[k]));
                    sInterpViews[k].id = sInterpBlkView[i];
                    sInterpViewCount++;
                }
            }
            sInterpViews[k].scenery |= (sInterpKind[i] >> 6) == 2;
        }
    }
    for (k = 0; k < sInterpViewCount; k++) {
        InterpViewState *v = &sInterpViews[k];
        sInterpView = v->id;
        if (first) {
            sInterpWasCut = 0;
            sInterpHaveD = sInterpHaveDt = sInterpRoots = sInterpHaveCam = 0;
            sInterpCamMove = 0.0f;
            v->ok = interp_build_view(t, 1);
            v->cut = sInterpWasCut;
            interp_view_save(v);
        } else if (v->ok) {
            interp_view_load(v);
            interp_build_view(t, 0);
            interp_view_save(v);
        }
        okAny |= v->ok;
        cutAny |= v->cut;
        okScenery |= v->ok && v->scenery;
    }
    sInterpViewLoaded = -1;
    /* a cut where no view with scenery goes on: no in-between picture at all (the shots of a transformation) */
    return okAny && !(cutAny && !okScenery);
}

/* ---- the primitives the game transformed itself (effects, beams, dust, the HUD): Vtx in GS pixels ----
 * They have no matrix to blend, so their vertices are: each triangle of this picture is paired with a triangle of
 * the previous picture that is drawn the same way (texture, pipeline, target) and lies nearest, looked for around
 * the same place in the run of such triangles (particles keep their order while others are born and die). A
 * triangle whose nearest partner is far away keeps this picture's position (a beam's tip, a new spark, a cut).
 * 2D art (sprites with whole-texel coordinates: HUD, text) must also show the same piece of its texture, or a
 * changing text would slide from the old letters' places. */
typedef struct InterpPrim {
    uint32_t sig, first; /* first: index of its first vertex */
    uint32_t kind;       /* sig without the number of triangles: how it is drawn */
    uint32_t base;       /* kind without the texture (interp_kinds_follow) */
    uint32_t view;       /* the view it is drawn in (INTERP_SCISSOR_ID) */
    uint8_t flat2d, tris; /* tris: how many triangles the piece has (a quad: 2) */
} InterpPrim;
#define INTERP_PIECE_MAX 16 /* triangles of one piece (a longer strip is cut into several) */
#define INTERP_MAX_PRIMS (MAX_VERTS / 3)
#define INTERP_WINDOW 48      /* triangles before and after the same place in the run */
#define INTERP_FAR 90.0f      /* GS pixels a vertex may move in a tick and still be blended */
#define INTERP_FAR_2D 5.0f    /* the same for 2D art (HUD, text) */
static Vtx *sInterpPrevVerts, *sInterpBlendVerts;
static InterpPrim *sInterpPrevPrims, *sInterpPrims;
static uint32_t sInterpPrevPrimCount, sInterpPrimCount, sInterpPrevVertCount;
static uint8_t *sInterpPrimUsed;
static unsigned gInterpTris, gInterpTrisBlended, gInterpTrisCamera;

/* Where a vertex the game transformed itself was one picture earlier if the thing it belongs to stood still: back to
   clip space (its q is 1 / w), through the camera's change, and to pixels again. 0 = it cannot be said. */
static int interp_reproject_with(const float *D, const Vtx *v, float *x, float *y, float *z) {
    float w, c[4], p[4], zs, zadd;
    int i;

    if (!(v->q > 1e-20f) || !(v->q < 1e20f)) {
        return 0;
    }
    /* The depth the game gave the corner: what put() stored is that plus a half and the lead (GS_Z_LEAD). Taken as
       it is, a far corner (depth about 1000 of 16 million, where one step is a long way) was put back in the world
       well in front of where it is, and the camera's sideways movement then moved it too far: a distant waterfall
       jumped sideways in every in-between picture (found by the user). */
    zadd = depth_frac() ? 0.0f : 0.5f;
    zs = v->z * sInterpZMax - zadd;
    if (getenv("BT3_INTERP_ZRAW") != NULL) { /* (testing: the stored depth as it is, the fault this corrects) */
        zs += zadd;
        zadd = 0.0f;
    } else if (!depth_frac() && zs < sInterpZMax - (float)GS_Z_LEAD) {
        zs -= (float)GS_Z_LEAD;
        zadd += (float)GS_Z_LEAD;
    }
    w = 1.0f / v->q;
    c[0] = (v->x + sInterpOffX) * w;
    c[1] = (v->y + sInterpOffY) * w;
    c[2] = zs * w;
    c[3] = w;
    for (i = 0; i < 4; i++) {
        p[i] = D[i] * c[0] + D[4 + i] * c[1] + D[8 + i] * c[2] + D[12 + i] * c[3];
    }
    if (!(p[3] > w * 0.05f)) { /* behind the eye then, or nearly */
        return 0;
    }
    *x = p[0] / p[3] - sInterpOffX;
    *y = p[1] / p[3] - sInterpOffY;
    *z = (p[2] / p[3] + zadd) / sInterpZMax; /* (stored the way put() stores it) */
    return 1;
}

static int interp_reproject(const Vtx *v, float *x, float *y, float *z) {
    return interp_reproject_with(sInterpD, v, x, y, z);
}

static int sInterpTrust; /* the piece is of a kind known to stand still in the world: no further questions */
/* Moves a piece by the camera's change. 0 = not possible (it keeps this picture's place). */
static int interp_piece_camera(const InterpPrim *c, float t) {
    const Vtx *cv = &gsVerts[c->first];
    Vtx *ov = &sInterpBlendVerts[c->first];
    float x[INTERP_PIECE_MAX * 3], y[INTERP_PIECE_MAX * 3], z[INTERP_PIECE_MAX * 3];
    int k, nv = c->tris * 3;

    if (!sInterpTrust) {   /* Not what faces the screen flat (every corner at the same depth: a banner, a flash, a sprite of the HUD
           drawn with perspective numbers): that is laid onto the screen, not standing in the world. (The "Fight!"
           banner slid down with the camera.) */
        float q0 = cv[0].q, q1 = q0;
        for (k = 1; k < nv; k++) {
            if (cv[k].q < q0) { q0 = cv[k].q; }
            if (cv[k].q > q1) { q1 = cv[k].q; }
        }
        if (!(q1 - q0 > q1 * 1e-4f)) {
            return 0;
        }
    }
    for (k = 0; k < nv; k++) {
        if (!interp_reproject(&cv[k], &x[k], &y[k], &z[k]) || fabsf(x[k] - cv[k].x) > 40.0f + 3.0f * sInterpCamMove || fabsf(y[k] - cv[k].y) > 40.0f + 3.0f * sInterpCamMove) {
            return 0;
        }
    }
    if (!sInterpTrust) {
        /* Is that where such a thing was? The camera's change is in the scenery's projection; a piece drawn through
           another one (what belongs to a fighter: its dust, its aura, the dark sheet of a close-up) comes out
           anywhere, also over the whole screen (seen: a grey veil over one in-between picture). So the previous
           picture must have had a piece drawn the same way roughly where this one is said to have been. (Roughly:
           the sky's panels are cut off at the screen's edge differently in every picture.) */
        float x0 = x[0], x1 = x0, y0 = y[0], y1 = y0, area;
        uint32_t j;
        int found = 0;

        for (k = 1; k < nv; k++) {
            if (x[k] < x0) { x0 = x[k]; } if (x[k] > x1) { x1 = x[k]; }
            if (y[k] < y0) { y0 = y[k]; } if (y[k] > y1) { y1 = y[k]; }
        }
        area = (x1 - x0) * (y1 - y0);
        for (j = 0; j < sInterpPrevPrimCount && !found; j++) {
            const InterpPrim *q = &sInterpPrevPrims[j];
            const Vtx *pv;
            float a0, a1, b0, b1, ix, iy, parea;
            int n2, m;
            if (q->kind != c->kind) {
                continue;
            }
            pv = &sInterpPrevVerts[q->first];
            n2 = q->tris * 3;
            a0 = a1 = pv[0].x;
            b0 = b1 = pv[0].y;
            for (m = 1; m < n2; m++) {
                if (pv[m].x < a0) { a0 = pv[m].x; } if (pv[m].x > a1) { a1 = pv[m].x; }
                if (pv[m].y < b0) { b0 = pv[m].y; } if (pv[m].y > b1) { b1 = pv[m].y; }
            }
            ix = (x1 < a1 ? x1 : a1) - (x0 > a0 ? x0 : a0);
            iy = (y1 < b1 ? y1 : b1) - (y0 > b0 ? y0 : b0);
            parea = (a1 - a0) * (b1 - b0);
            /* the two cover much the same part of the screen (a sheet of dust near the camera overlaps anything) */
            found = ix > 0.0f && iy > 0.0f && ix * iy > 0.35f * (area > parea ? area : parea);
        }
        if (!found) {
            return 0;
        }
    }
    for (k = 0; k < nv; k++) {
        float xt, yt, zt;
        if (sInterpHaveDt && interp_reproject_with(sInterpDt, &cv[k], &xt, &yt, &zt)) {
            ov[k].x = xt; /* where the camera, part of its way, sees the corner */
            ov[k].y = yt;
            ov[k].z = zt;
        } else {
            ov[k].x = x[k] + (cv[k].x - x[k]) * t;
            ov[k].y = y[k] + (cv[k].y - y[k]) * t;
            ov[k].z = z[k] + (cv[k].z - z[k]) * t;
        }
    }
    return 1;
}
static unsigned gInterpWhy[5]; /* no like-drawn triangle before; all too far; 2D with another piece of texture; standing still; (spare) */

static int interp_prim_cmp(const void *a, const void *b) {
    const InterpPrim *x = a, *y = b;
    return x->sig != y->sig ? (x->sig < y->sig ? -1 : 1) : x->first < y->first ? -1 : x->first > y->first;
}

/* The triangles of the recorded frame's own-transformed draws, sorted by how they are drawn and then by order. */
static uint32_t interp_prims(InterpPrim *out) {
    uint32_t n, k, count = 0;

    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        uint32_t role;

        /* Not the passes that draw one buffer into another (the glow's and the explosions' smaller copies, the
           blur): their rectangles are in the coordinates of whichever buffer they fill, and a rectangle of one pass
           blended with that of another is a square of garbage. They stay this picture's. */
        if (d->vu != 0 || d->native != 0 || d->count % 3 != 0 || d->tex_is_target) {
            continue;
        }
        /* Which buffer it draws into, in a way that is the same next picture: the game shows its two frame buffers
           in turn, so "the picture's own buffer" is one role; any other buffer is itself. */
        role = d->target >= 0 && d->target < gsTargetCount && gGsMainFbp >= 0 && gsTargets[d->target].fbp == (uint32_t)gGsMainFbp
                   ? 0u : 1u + (d->target >= 0 && d->target < gsTargetCount ? gsTargets[d->target].fbp : 0x3FFu);
        /* The unit that is paired and moved is a PIECE: triangles that follow each other and share an edge (the two
           halves of a sprite or of a puff of smoke, a strip). Paired one triangle at a time, the halves of a quad
           found different partners and the quad came apart into wedges (seen in every explosion). */
        for (k = 0; k + 3 <= d->count && count < INTERP_MAX_PRIMS;) {
            uint32_t tris = 1;
            while (k + (tris + 1) * 3 <= d->count && tris < INTERP_PIECE_MAX) {
                const Vtx *a = &gsVerts[d->first + k + (tris - 1) * 3], *b = a + 3;
                int shared = 0, x, y;
                for (x = 0; x < 3; x++) {
                    for (y = 0; y < 3; y++) {
                        if (a[x].x == b[y].x && a[x].y == b[y].y) {
                            shared++;
                            break;
                        }
                    }
                }
                if (shared < 2) {
                    break;
                }
                tris++;
            }
            out[count].view = INTERP_SCISSOR_ID(d);
            out[count].sig = ((uint32_t)d->tex * 2654435761u) ^ ((uint32_t)d->pipeline * 40503u) ^ (role * 0x9E3779B1u) ^ (tris * 0x85EBCA6Bu) ^ (out[count].view * 0xC2B2AE35u);
            out[count].kind = ((uint32_t)d->tex * 2654435761u) ^ ((uint32_t)d->pipeline * 40503u) ^ (role * 0x9E3779B1u) ^ (out[count].view * 0xC2B2AE35u);
            out[count].base = ((uint32_t)d->pipeline * 40503u) ^ (role * 0x9E3779B1u) ^ (out[count].view * 0xC2B2AE35u);
            out[count].first = d->first + k;
            out[count].flat2d = d->misc[3] != 0.0f;
            out[count].tris = (uint8_t)tris;
            count++;
            k += tris * 3;
        }
    }
    qsort(out, count, sizeof(InterpPrim), interp_prim_cmp);
    return count;
}

/* Which kinds of piece stand still in the world, asked corner by corner and once a tick: a kind does if most of a
   sample of its corners, taken back by the camera's change, lie on corners of the same kind in the previous picture.
   (Asked piece by piece it missed what is cut into pieces differently in every picture: the sheets of a waterfall,
   long strips that are cut at the screen's edge. Their pieces were paired by nearness with other parts of the
   sheet and the fall was pinched or grew a wedge in the in-between pictures.) */
#define INTERP_MAX_KINDS 512
static struct { uint32_t kind, count, seen, tested, landed; uint8_t flat, isStatic; } sInterpKinds[INTERP_MAX_KINDS];
static uint32_t sInterpKindCount;
static uint32_t *sInterpPrevByKind;

static int interp_bykind_cmp(const void *a, const void *b) {
    uint32_t x = sInterpPrevPrims[*(const uint32_t *)a].kind, y = sInterpPrevPrims[*(const uint32_t *)b].kind;
    return x < y ? -1 : x > y;
}

static int interp_kind_find(uint32_t kind, int add) {
    uint32_t k;

    for (k = 0; k < sInterpKindCount; k++) {
        if (sInterpKinds[k].kind == kind) {
            return (int)k;
        }
    }
    if (!add || sInterpKindCount == INTERP_MAX_KINDS) {
        return -1;
    }
    memset(&sInterpKinds[sInterpKindCount], 0, sizeof(sInterpKinds[0]));
    sInterpKinds[sInterpKindCount].kind = kind;
    return (int)sInterpKindCount++;
}

/* A surface whose texture is another one this tick (a waterfall's water is a few pictures shown in turn; a texture
   put into GS memory again) is a kind that was not there a tick ago, and the kind it was is gone. Nothing of it was
   found in the previous picture then, so all of it stayed where this tick has it while the scenery around it was
   drawn part of the way: a distant waterfall a camera's step to the side for one in-between picture in about
   twenty-five (found by the user, on a video of his screen). The previous picture's pieces of the kind that is
   gone are given the new kind's name, where the two are drawn the same way but for the texture. */
static void interp_kinds_follow(void) {
    enum { MAXK = 512 };
    static uint32_t ck[MAXK], cb[MAXK], pk[MAXK], pb[MAXK];
    uint32_t nc = 0, np = 0, i, j, changed = 0;

    for (i = 0; i < sInterpPrimCount; i++) {
        for (j = 0; j < nc && ck[j] != sInterpPrims[i].kind; j++) {
        }
        if (j == nc && nc < MAXK) {
            ck[nc] = sInterpPrims[i].kind;
            cb[nc++] = sInterpPrims[i].base;
        }
    }
    for (i = 0; i < sInterpPrevPrimCount; i++) {
        for (j = 0; j < np && pk[j] != sInterpPrevPrims[i].kind; j++) {
        }
        if (j == np && np < MAXK) {
            pk[np] = sInterpPrevPrims[i].kind;
            pb[np++] = sInterpPrevPrims[i].base;
        }
    }
    for (i = 0; i < nc; i++) {
        uint32_t take = np, a;
        for (j = 0; j < np && pk[j] != ck[i]; j++) {
        }
        if (j < np) {
            continue; /* it was there */
        }
        for (j = 0; j < np; j++) { /* one that is gone, drawn the same way; only if there is exactly one such */
            if (pb[j] != cb[i]) {
                continue;
            }
            for (a = 0; a < nc && ck[a] != pk[j]; a++) {
            }
            if (a < nc) {
                continue; /* (still there) */
            }
            if (take != np) {
                take = np + 1;
                break;
            }
            take = j;
        }
        if (take < np) {
            for (a = 0; a < sInterpPrevPrimCount; a++) {
                if (sInterpPrevPrims[a].kind == pk[take]) {
                    sInterpPrevPrims[a].kind = ck[i];
                    sInterpPrevPrims[a].sig = ck[i] ^ ((uint32_t)sInterpPrevPrims[a].tris * 0x85EBCA6Bu);
                }
            }
            pk[take] = ck[i];
            changed = 1;
        }
    }
    if (changed) {
        qsort(sInterpPrevPrims, sInterpPrevPrimCount, sizeof(InterpPrim), interp_prim_cmp);
    }
}

static void interp_static_kinds(void) {
    uint32_t i, j;

    sInterpKindCount = 0;
    if (sInterpPrevPrimCount == 0) {
        return;
    }
    if (sInterpPrevByKind == NULL) {
        sInterpPrevByKind = malloc(INTERP_MAX_PRIMS * sizeof(uint32_t));
    }
    for (i = 0; i < sInterpPrevPrimCount; i++) {
        sInterpPrevByKind[i] = i;
    }
    qsort(sInterpPrevByKind, sInterpPrevPrimCount, sizeof(uint32_t), interp_bykind_cmp);
    for (i = 0; i < sInterpPrimCount; i++) {
        int e = interp_kind_find(sInterpPrims[i].kind, 1);
        if (e >= 0) {
            sInterpKinds[e].count++;
            sInterpKinds[e].flat |= sInterpPrims[i].flat2d;
        }
    }
    for (i = 0; i < sInterpPrimCount; i++) {
        const InterpPrim *c = &sInterpPrims[i];
        int e = interp_kind_find(c->kind, 0), k, nv = c->tris * 3;
        uint32_t stride, a, b;
        if (e < 0 || sInterpKinds[e].flat) {
            continue;
        }
        if (!interp_view_select(c->view) || !sInterpHaveD) { /* (its view's camera; none, or a cut there: not asked) */
            continue;
        }
        stride = sInterpKinds[e].count / 16 + 1;
        if (sInterpKinds[e].seen++ % stride != 0) {
            continue;
        }
        /* the previous picture's pieces of the kind: a range of the list sorted by kind */
        a = 0;
        b = sInterpPrevPrimCount;
        while (a < b) {
            uint32_t m = (a + b) / 2;
            if (sInterpPrevPrims[sInterpPrevByKind[m]].kind < c->kind) { a = m + 1; } else { b = m; }
        }
        for (k = 0; k < nv; k += 2) { /* (every other corner) */
            const Vtx *cv = &gsVerts[c->first + k];
            float px, py, pz;
            int found = 0;
            if (!interp_reproject(cv, &px, &py, &pz)) {
                continue;
            }
            sInterpKinds[e].tested++;
            for (j = a; j < sInterpPrevPrimCount && !found; j++) {
                const InterpPrim *q = &sInterpPrevPrims[sInterpPrevByKind[j]];
                const Vtx *pv = &sInterpPrevVerts[q->first];
                int m, n2 = q->tris * 3;
                if (q->kind != c->kind) {
                    break;
                }
                for (m = 0; m < n2; m++) {
                    if (fabsf(pv[m].x - px) <= 1.0f && fabsf(pv[m].y - py) <= 1.0f) {
                        found = 1;
                        break;
                    }
                }
            }
            sInterpKinds[e].landed += found;
        }
    }
    for (i = 0; i < sInterpKindCount; i++) {
        sInterpKinds[i].isStatic = sInterpKinds[i].tested >= 2 && sInterpKinds[i].landed * 10 >= sInterpKinds[i].tested * 6;
    }
}

/* ---- the wall at the edge of the map in an in-between picture ------------------------------------------------
 * The wall is not scenery: the game builds it anew every tick as a patch of 6 x 4 points on the map's cylinder,
 * centred on the fighter's bearing and height (EftBound_BuildWall), with a texture that is laid out in the world
 * and scrolls, and cuts its triangles off at the edges of the view before the GS gets them. As pieces moved by the
 * camera's change it tore: the patch travels with the fighter, and its cut pieces are different ones every tick
 * (two attempts on the pieces, "none moves if one cannot" and "all move if most can", both looked worse). So the
 * game hands over the patch itself (Port_WallMesh), and an in-between picture gets the patch part of the way
 * between the two ticks' (points, texture coordinates, colours), seen by the camera part of its way: the tick's
 * own wall triangles are taken out of that picture and these put in their place.
 * The points go through the tick's own world-to-screen matrix and then the camera's change for the part of the
 * way (sInterpDt), which is how every other piece of scenery gets there. Only the plane in front of the eye is
 * cut here; the screen's edges are the GPU's. */
static struct { uint32_t draw, first, count; } sWallPatch[WALL_MAX];
static int sWallPatches;

typedef struct WallPt { float p[4], s, t, col[4]; } WallPt;

static void wall_lerp_pt(WallPt *o, const WallPt *a, const WallPt *b, float k) {
    int i;
    for (i = 0; i < 4; i++) {
        o->p[i] = a->p[i] + (b->p[i] - a->p[i]) * k;
        o->col[i] = a->col[i] + (b->col[i] - a->col[i]) * k;
    }
    o->s = a->s + (b->s - a->s) * k;
    o->t = a->t + (b->t - a->t) * k;
}

/* One triangle of the patch (already at its part of the way, in the world): through the matrices, cut at the eye's
   plane, written as up to two triangles. Returns the vertices written. */
static uint32_t wall_tri(Vtx *out, uint32_t room, const WallMesh *m, const WallPt *w3, float wNear, const float *Dt, float zBias) {
    WallPt in[3], poly[9];
    int i, k, n = 0;
    uint32_t made = 0;
    float fs = (float)(int)w3[0].s, ft = (float)(int)w3[0].t; /* (the game takes the first corner's whole repeats off all three) */

    for (i = 0; i < 3; i++) {
        float c[4];
        in[i] = w3[i];
        in[i].s -= fs;
        in[i].t -= ft;
        for (k = 0; k < 4; k++) {
            c[k] = m->S[k] * w3[i].p[0] + m->S[4 + k] * w3[i].p[1] + m->S[8 + k] * w3[i].p[2] + m->S[12 + k];
        }
        for (k = 0; k < 4; k++) {
            in[i].p[k] = Dt[k] * c[0] + Dt[4 + k] * c[1] + Dt[8 + k] * c[2] + Dt[12 + k] * c[3];
        }
    }
    /* Cut as the game cuts it (EftGfx_DrawPolyScaledZ: five planes, before the division): at the eye's plane and
       at the edges of the view with its margin (seen in what reaches the GS: x from -29.69 to 541.69, y from -26
       to 474). Not only for the eye's plane: the GS shades a triangle straight across the screen, so where the
       corners are decides the colours in between. Left whole, with corners far outside, the wall came out a
       little brighter towards the top than the game's own (the test BT3_INTERP_WALLTEST showed it). */
    n = 3;
    memcpy(poly, in, sizeof(in));
    for (k = 0; k < 5 && n >= 3; k++) {
        WallPt next[9];
        float dist[9];
        int m2 = 0;
        for (i = 0; i < n; i++) {
            const float *q = poly[i].p;
            dist[i] = k == 0 ? q[3] - wNear :
                      k == 1 ? q[0] - (sInterpOffX - 29.69f) * q[3] :
                      k == 2 ? (sInterpOffX + 541.69f) * q[3] - q[0] :
                      k == 3 ? q[1] - (sInterpOffY - 26.0f) * q[3] : (sInterpOffY + 474.0f) * q[3] - q[1];
        }
        for (i = 0; i < n; i++) {
            int j = (i + 1) % n;
            if (dist[i] >= 0.0f) {
                next[m2++] = poly[i];
            }
            if ((dist[i] >= 0.0f) != (dist[j] >= 0.0f)) {
                wall_lerp_pt(&next[m2++], &poly[i], &poly[j], dist[i] / (dist[i] - dist[j]));
            }
        }
        n = m2;
        memcpy(poly, next, sizeof(WallPt) * (size_t)n);
    }
    for (i = 2; i < n && made + 3 <= room; i++) {
        const WallPt *tri[3] = {&poly[0], &poly[i - 1], &poly[i]};
        for (k = 0; k < 3; k++) {
            const WallPt *v = tri[k];
            Vtx *o = &out[made++];
            float q = 1.0f / v->p[3], z = v->p[2] * q;
            int ch;
            uint8_t bytes[4];
            z += zBias + (float)GS_Z_LEAD; /* (the lead every piece the game transforms itself has: put()) */
            z = z < 0.0f ? 0.0f : z > sInterpZMax - 1.0f ? sInterpZMax - 1.0f : z;
            for (ch = 0; ch < 4; ch++) {
                float cc = v->col[ch] < 0.0f ? 0.0f : v->col[ch] > 255.0f ? 255.0f : v->col[ch];
                bytes[ch] = (uint8_t)cc;
            }
            o->x = v->p[0] * q - sInterpOffX;
            o->y = v->p[1] * q - sInterpOffY;
            o->z = (floorf(z) + 0.5f) / sInterpZMax;
            o->s = v->s * q * sWallUs;
            o->t = v->t * q * sWallVs;
            o->q = q;
            o->r = bytes[0]; o->g = bytes[1]; o->b = bytes[2]; o->a = bytes[3];
        }
    }
    return made;
}

static void interp_wall_restore(void) {
    while (sWallPatches > 0) {
        sWallPatches--;
        gsDraws[sWallPatch[sWallPatches].draw].first = sWallPatch[sWallPatches].first;
        gsDraws[sWallPatch[sWallPatches].draw].count = sWallPatch[sWallPatches].count;
    }
    /* (not the vertex count: the back end has emptied the lists, and the tick's own counts are put back for its
       real picture by interp_counts_restore; a count left here read as "the game has drawn already" and the tick's
       real picture and its being remembered were dropped) */
}

static void interp_wall_into(float t, Vtx *buf, int test);

static void interp_wall(float t) {
    interp_wall_into(t, sInterpBlendVerts, getenv("BT3_INTERP_WALLTEST") != NULL);
}

/* The tick's real picture, while in-between pictures are shown: its wall drawn the same way, at the tick itself. */
static void interp_wall_real(void) {
    if (getenv("BT3_INTERP_WALLGAME") == NULL) { /* (the switch: the game's own triangles in the real picture, to compare) */
        interp_wall_into(1.0f, gsVerts, 1);
    }
}

static void interp_wall_into(float t, Vtx *buf, int test) {
    static const float kSame[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    /* BT3_INTERP_WALLTEST=1 (with BT3_INTERP_ONLY): the patch as it is at the tick itself, seen by the tick's own
       camera. The picture must then be the game's own picture of the tick: the test that what is drawn here is
       what the game draws. */
    const float *Dt = test ? kSame : sInterpDt;
    uint32_t ids[4], n;
    int idx0[4], nids = 0, k, i;

    if (test) {
        t = 1.0f;
    }

    sWallPatches = 0;
    if (getenv("BT3_INTERP_WALLLOG") != NULL && sWallSeq != 0) {
        uint32_t wd = 0, wv = 0;
        for (n = 0; n < gsDrawCount; n++) {
            if (gsDraws[n].wall) { wd++; wv += gsDraws[n].count; }
        }
        fprintf(stderr, "wall: frame %u: %d patches handed over so far, %u draws with %u vertices are the wall's (tex0 %llx)\n", gGsFrame, (int)sWallSeq, wd, wv, (unsigned long long)sWallTex0);
    }
    if (sWallSeq == 0 || getenv("BT3_INTERP_NOWALL") != NULL) {
        return;
    }
    for (n = 0; n < gsDrawCount; n++) { /* the views the wall is drawn in, the left one first */
        const GsDraw *d = &gsDraws[n];
        uint32_t id;
        if (!d->wall || d->vu || d->native || d->count < 3) {
            continue;
        }
        id = INTERP_SCISSOR_ID(d);
        for (k = 0; k < nids && ids[k] != id; k++) {
        }
        if (k == nids && nids < 4) {
            for (i = nids; i > 0 && gsDraws[idx0[i - 1]].scissor[0] > d->scissor[0]; i--) {
                ids[i] = ids[i - 1];
                idx0[i] = idx0[i - 1];
            }
            ids[i] = id;
            idx0[i] = (int)n;
            nids++;
        }
    }
    for (k = 0; k < nids; k++) {
        Vtx *out = &buf[gsVertCount];
        uint32_t room = MAX_VERTS - gsVertCount, made = 0;
        float wNear = 0.0f;
        int mi, first = -1, used = 0;

        if (!interp_view_select(ids[k]) || (!sInterpHaveDt && !test)) {
            continue; /* a cut, or no camera known: the wall stays this tick's */
        }
        for (n = 0; n < gsDrawCount; n++) { /* how near the game lets it come to the eye: the nearest corner it drew */
            const GsDraw *d = &gsDraws[n];
            uint32_t j;
            if (d->wall && !d->vu && !d->native && INTERP_SCISSOR_ID(d) == ids[k]) {
                for (j = 0; j < d->count; j++) {
                    float w = gsVerts[d->first + j].q > 0.0f ? 1.0f / gsVerts[d->first + j].q : 0.0f;
                    if (w > 0.0f && (wNear == 0.0f || w < wNear)) {
                        wNear = w;
                    }
                }
            }
        }
        if (!(wNear > 0.0f)) {
            continue;
        }
        for (mi = 0; mi < WALL_RING && mi < (int)sWallSeq; mi++) { /* from the newest back */
            const WallMesh *m = &sWallRing[(sWallSeq - 1 - (uint32_t)mi) % WALL_RING], *p = m;
            float cam[3];
            int q, c, pi;
            /* The game hands the patch over once per camera it draws for (both, in a two-player battle, also when
               only one view is shown): this picture's are those with the matrix its scenery is drawn with. Each
               once (the newest). */
            for (pi = 0; pi < (int)nscreens && memcmp(screens[pi], m->S, sizeof(m->S)) != 0; pi++) {
            }
            if (pi == (int)nscreens) {
                continue;
            }
            for (pi = 0; pi < mi; pi++) {
                const WallMesh *o = &sWallRing[(sWallSeq - 1 - (uint32_t)pi) % WALL_RING];
                if (o->key == m->key && o->culled == m->culled && memcmp(o->S, m->S, sizeof(m->S)) == 0) {
                    break;
                }
            }
            if (pi < mi) {
                continue;
            }
            used++;
            /* the same patch a tick before: the one handed over last before this one for the same camera */
            for (pi = mi + 1; pi < WALL_RING && pi < (int)sWallSeq; pi++) {
                const WallMesh *o = &sWallRing[(sWallSeq - 1 - (uint32_t)pi) % WALL_RING];
                if (o->key == m->key && o->second == m->second && o->culled == m->culled) {
                    p = o;
                    break;
                }
            }
            for (c = 0; c < 3; c++) {
                cam[c] = p->cam[c] + (m->cam[c] - p->cam[c]) * t;
            }
            for (q = 0; q < m->nq; q++) {
                WallPt w[4];
                int j;
                for (j = 0; j < 4; j++) {
                    int vi = m->idx[q][j];
                    const float *a, *b;
                    float as, at;
                    if (vi < 0 || vi >= 24) {
                        break;
                    }
                    a = p->v[vi];
                    b = m->v[vi];
                    as = a[8];
                    at = a[9];
                    /* (the texture's scroll starts again at a whole repeat: the same place, one further) */
                    if (b[8] - as > 0.5f) { as += 1.0f; } else if (b[8] - as < -0.5f) { as -= 1.0f; }
                    if (b[9] - at > 0.5f) { at += 1.0f; } else if (b[9] - at < -0.5f) { at -= 1.0f; }
                    for (c = 0; c < 3; c++) {
                        w[j].p[c] = a[c] + (b[c] - a[c]) * t;
                    }
                    w[j].p[3] = 1.0f;
                    w[j].s = as + (b[8] - as) * t;
                    w[j].t = at + (b[9] - at) * t;
                    for (c = 0; c < 4; c++) {
                        w[j].col[c] = a[12 + c] + (b[12 + c] - a[12 + c]) * t;
                    }
                }
                if (j < 4) {
                    continue;
                }
                if (m->culled) { /* only the quads that face the camera, as the game decides it */
                    float e1[3], e2[3], nrm[3], dv[3];
                    for (c = 0; c < 3; c++) {
                        e1[c] = w[1].p[c] - w[0].p[c];
                        e2[c] = w[2].p[c] - w[1].p[c];
                        dv[c] = w[1].p[c] - cam[c];
                    }
                    nrm[0] = e2[1] * e1[2] - e2[2] * e1[1];
                    nrm[1] = e2[2] * e1[0] - e2[0] * e1[2];
                    nrm[2] = e2[0] * e1[1] - e2[1] * e1[0];
                    if (!(nrm[0] * dv[0] + nrm[1] * dv[1] + nrm[2] * dv[2] < 0.0f)) {
                        continue;
                    }
                }
                {
                    WallPt ta[3] = {w[0], w[1], w[2]}, tb[3] = {w[2], w[1], w[3]};
                    /* Two fighters at the same stretch of the wall: two patches in one surface. In the game, which
                       of them a pixel shows over the other falls to the last bit of its depth, triangle by triangle;
                       drawn here the bits fall differently and the wall changed its shade in triangles from the real
                       picture to the in-between one. So each patch is drawn a few depth steps in front of the one
                       before it (every later one is then drawn over the earlier, everywhere), and the tick's real
                       picture gets the wall drawn this way too (interp_wall_real), so that the pictures agree. */
                    made += wall_tri(out + made, room - made, m, ta, wNear, Dt, 4.0f * (float)(WALL_RING - mi));
                    made += wall_tri(out + made, room - made, m, tb, wNear, Dt, 4.0f * (float)(WALL_RING - mi));
                }
            }
        }
        if (getenv("BT3_INTERP_WALLLOG") != NULL) {
            fprintf(stderr, "wall: frame %u view %d of %d: %d patches are this view's, %u vertices made, nearest w %.2f\n", gGsFrame, k, nids, used, made, wNear);
        }
        if (used == 0) {
            continue; /* none of the patches is this view's: its wall stays this tick's */
        }
        /* the tick's own wall triangles of this view give way: the first of its draws takes the new ones, the
           others (and the first, if there is nothing to show) are drawn as nothing */
        for (n = 0; n < gsDrawCount; n++) {
            GsDraw *d = &gsDraws[n];
            uint32_t j;
            if (!d->wall || d->vu || d->native || d->count < 3 || INTERP_SCISSOR_ID(d) != ids[k]) {
                continue;
            }
            if (first < 0 && made > 0 && sWallPatches < WALL_MAX) {
                first = (int)n;
                sWallPatch[sWallPatches].draw = n;
                sWallPatch[sWallPatches].first = d->first;
                sWallPatch[sWallPatches].count = d->count;
                sWallPatches++;
                d->first = gsVertCount;
                d->count = made;
                gsVertCount += made;
                continue;
            }
            for (j = 1; j < d->count; j++) {
                buf[d->first + j] = buf[d->first];
            }
        }
    }
}

static void interp_build_verts(float t) {
    uint32_t i, lo = 0;
    int staticKind = 0;
    static unsigned forFrame = ~0u;

    if (forFrame != gGsFrame) {
        forFrame = gGsFrame;
        interp_kinds_follow();
        interp_static_kinds();
    }

    memcpy(sInterpBlendVerts, gsVerts, gsVertCount * sizeof(Vtx));
    memset(sInterpPrimUsed, 0, sInterpPrevPrimCount);
    for (i = 0; i < sInterpPrimCount;) {
        uint32_t sig = sInterpPrims[i].sig, end = i, pEnd, r;

        while (end < sInterpPrimCount && sInterpPrims[end].sig == sig) { end++; }
        while (lo < sInterpPrevPrimCount && sInterpPrevPrims[lo].sig < sig) { lo++; }
        pEnd = lo;
        while (pEnd < sInterpPrevPrimCount && sInterpPrevPrims[pEnd].sig == sig) { pEnd++; }
        if (!interp_view_select(sInterpPrims[i].view)) { /* a cut in this piece's view: it stays this picture's */
            i = end;
            lo = pEnd;
            continue;
        }
        gInterpTris += end - i;
        /* Is this kind of piece something that stands still in the world (the translucent wall at the edge of the
           map, drawn as a grid of like tiles; marks on the ground)? Then every piece of it goes by the camera's
           change, exactly. (Paired by nearness, a tile of the wall found its neighbour, and as the tiles shown
           change with where the fighters are, parts of the wall stood a tile off in the in-between pictures: a break
           across it that came and went.) It is known by this: taken back by the camera's change, the pieces land
           on pieces of the previous picture, corner for corner. Only asked while the camera moves: when it stands,
           everything would pass. */
        staticKind = 0;
        if (pEnd > lo && !sInterpPrims[i].flat2d && sInterpHaveD) { /* (also while the camera stands: what lands then did not move) */
            uint32_t stepS = (end - i) / 24 ? (end - i) / 24 : 1, tested = 0, okS = 0, rs, j;
            for (rs = 0; rs < end - i; rs += stepS) {
                const InterpPrim *c = &sInterpPrims[i + rs];
                const Vtx *cv = &gsVerts[c->first];
                float px[INTERP_PIECE_MAX * 3], py[INTERP_PIECE_MAX * 3], pz;
                int k, nv = c->tris * 3, can = 1, found = 0;
                for (k = 0; k < nv && can; k++) {
                    can = interp_reproject(&cv[k], &px[k], &py[k], &pz);
                }
                if (!can) {
                    continue;
                }
                tested++;
                for (j = lo; j < pEnd && !found; j++) {
                    const Vtx *pv = &sInterpPrevVerts[sInterpPrevPrims[j].first];
                    for (k = 0; k < nv; k++) {
                        if (fabsf(pv[k].x - px[k]) > 1.0f || fabsf(pv[k].y - py[k]) > 1.0f) {
                            break;
                        }
                    }
                    found = k == nv;
                }
                okS += found;
            }
            /* (half of them is enough: the tiles of the wall that are shown change from picture to picture, and
               the new ones have nothing to land on) */
            staticKind = okS >= 1 && okS * 2 >= tested;
            if (getenv("BT3_INTERP_DUMP") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_DUMP"))) {
                const InterpPrim *c = &sInterpPrims[i];
                const Vtx *cv = &gsVerts[c->first];
                float bx = 0.0f, by = 0.0f, bz = 0.0f, bd = 1e9f;
                int can = interp_reproject(&cv[0], &bx, &by, &bz);
                for (j = lo; j < pEnd; j++) {
                    const Vtx *pv = &sInterpPrevVerts[sInterpPrevPrims[j].first];
                    float dd = fabsf(pv[0].x - bx) + fabsf(pv[0].y - by);
                    if (dd < bd) { bd = dd; }
                }
                fprintf(stderr, "kind %08x tris %d: %u now, %u before; tested %u, landed %u; first corner %.1f,%.1f q %.5f -> by the camera %.1f,%.1f (%d), nearest first corner before %.1f away\n", c->kind, c->tris, end - i, pEnd - lo, tested, okS,
                        cv[0].x, cv[0].y, cv[0].q, bx, by, can, bd);
            }
        }
        {
            int e = interp_kind_find(sInterpPrims[i].kind, 0);
            int byCorners = e >= 0 && sInterpKinds[e].isStatic;
            if (getenv("BT3_INTERP_DUMP") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_DUMP")) && e >= 0) {
                fprintf(stderr, "  kind %08x by corners: %u pieces, %u corners tested, %u landed -> %d (by pieces %d)\n", sInterpPrims[i].kind, sInterpKinds[e].count, sInterpKinds[e].tested, sInterpKinds[e].landed, byCorners, staticKind);
            }
            if (byCorners && !staticKind) {
                /* all of them, also those with nothing like them before */
                uint32_t r3;
                sInterpTrust = 1;
                for (r3 = 0; r3 < end - i; r3++) {
                    if (interp_piece_camera(&sInterpPrims[i + r3], t)) {
                        gInterpTrisCamera++;
                        gInterpStatic++;
                    }
                }
                sInterpTrust = 0;
                i = end;
                lo = pEnd;
                continue;
            }
        }
        if (pEnd == lo) {
            gInterpWhy[0] += end - i;
            for (r = 0; sInterpHaveD && r < end - i; r++) {
                if (!sInterpPrims[i + r].flat2d && interp_piece_camera(&sInterpPrims[i + r], t)) {
                    gInterpTrisCamera++;
                }
            }
        }
        for (r = 0; pEnd > lo && r < end - i; r++) {
            const InterpPrim *c = &sInterpPrims[i + r];
            const Vtx *cv = &gsVerts[c->first];
            /* the same place in the previous run, counted from the nearer end (births and deaths happen at one) */
            uint32_t n = end - i, m = pEnd - lo;
            uint32_t at = r < n / 2 || n > m + r ? (r < m ? r : m - 1) : m - (n - r);
            uint32_t from = at > INTERP_WINDOW ? at - INTERP_WINDOW : 0, to = at + INTERP_WINDOW + 1 < m ? at + INTERP_WINDOW + 1 : m, j;
            int best = -1, k, sawFar = 0, sawUv = 0, nv = c->tris * 3, big = 0;
            float bestD = 0.0f, ex[INTERP_PIECE_MAX * 3], ey[INTERP_PIECE_MAX * 3];

            if (staticKind) {
                sInterpTrust = 1;
                if (interp_piece_camera(c, t)) {
                    gInterpTrisCamera++;
                    gInterpStatic++;
                }
                sInterpTrust = 0;
                continue;
            }

            /* Big pieces and pieces cut off at the edge of the screen are scenery the game transforms itself (the
               sky's panels): cut differently every picture, so "the nearest like piece" is another panel or another
               cut of it, and panels blended towards different partners leave streaks between them. They stand still
               in the world: the camera's change says exactly where each of their corners was. */
            if (!c->flat2d && sInterpHaveD) {
                float x0 = cv[0].x, x1 = x0, y0 = cv[0].y, y1 = y0;
                for (k = 1; k < nv; k++) {
                    if (cv[k].x < x0) { x0 = cv[k].x; } if (cv[k].x > x1) { x1 = cv[k].x; }
                    if (cv[k].y < y0) { y0 = cv[k].y; } if (cv[k].y > y1) { y1 = cv[k].y; }
                }
                if (x0 < -8.0f || y0 < -8.0f || x1 > 520.0f || y1 > 456.0f || (x1 - x0) * (y1 - y0) > 6000.0f) {
                    int okc = interp_piece_camera(c, t);
                    if (getenv("BT3_INTERP_DUMP") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_DUMP"))) {
                        fprintf(stderr, "big piece kind %08x tris %d at %.0f,%.0f..%.0f,%.0f q %.5f..%.5f: by the camera %d\n", c->kind, c->tris, x0, y0, x1, y1, cv[0].q, cv[nv - 1].q, okc);
                    }
                    if (okc) {
                        gInterpTrisCamera++;
                        continue;
                    }
                }
            }

            /* How far a partner may be: with room for what the camera does to the picture (while it turns fast,
               everything is far on the screen, and nothing was blended). Not by taking the camera's change out of
               each corner: effects that belong to a fighter are in the fighters' projection, which that change does
               not fit. */
            {
                float bx0 = cv[0].x, bx1 = bx0, by0 = cv[0].y, by1 = by0;
                for (k = 0; k < nv; k++) {
                    ex[k] = cv[k].x;
                    ey[k] = cv[k].y;
                    if (cv[k].x < bx0) { bx0 = cv[k].x; } if (cv[k].x > bx1) { bx1 = cv[k].x; }
                    if (cv[k].y < by0) { by0 = cv[k].y; } if (cv[k].y > by1) { by1 = cv[k].y; }
                }
                big = (bx1 - bx0) * (by1 - by0) > 25000.0f;
            }
            for (j = from; j < to; j++) {
                const Vtx *pv;
                float d = 0.0f, far = 0.0f;
                if (sInterpPrimUsed[lo + j]) {
                    continue;
                }
                pv = &sInterpPrevVerts[sInterpPrevPrims[lo + j].first];
                if (!big) { /* (or the partner is such a sheet) */
                    float bx0 = pv[0].x, bx1 = bx0, by0 = pv[0].y, by1 = by0;
                    for (k = 1; k < nv; k++) {
                        if (pv[k].x < bx0) { bx0 = pv[k].x; } if (pv[k].x > bx1) { bx1 = pv[k].x; }
                        if (pv[k].y < by0) { by0 = pv[k].y; } if (pv[k].y > by1) { by1 = pv[k].y; }
                    }
                    if ((bx1 - bx0) * (by1 - by0) > 25000.0f) {
                        continue;
                    }
                }
                for (k = 0; k < nv; k++) {
                    float dx = fabsf(pv[k].x - ex[k]), dy = fabsf(pv[k].y - ey[k]);
                    d += dx + dy;
                    if (dx > far) { far = dx; }
                    if (dy > far) { far = dy; }
                }
                /* 2D art moves only a little or not at all: the digits of a number that changes are the same few
                   glyphs in new places, and the nearest "same glyph" is a different digit of the old number (the
                   damage counter lost digits in the in-between pictures) */
                /* A piece that covers a large part of the screen (a sheet of dust or a flash near the camera) is
                   blended only if it hardly moved: half way between two such sheets is a veil over everything. */
                if (far > (c->flat2d ? INTERP_FAR_2D : big ? 12.0f : INTERP_FAR + 1.5f * sInterpCamMove)) {
                    sawFar = 1;
                    continue;
                }
                if (c->flat2d && (fabsf(pv[0].s - cv[0].s) > 0.01f || fabsf(pv[0].t - cv[0].t) > 0.01f || fabsf(pv[2].s - cv[2].s) > 0.01f ||
                                  fabsf(pv[2].t - cv[2].t) > 0.01f)) {
                    sawUv = 1;
                    continue;
                }
                if (best < 0 || d < bestD) {
                    best = (int)j;
                    bestD = d;

                }
            }
            if (best >= 0 && bestD > 4.0f * (float)nv) {
                /* The partner must want this piece too: if another piece of this picture lies clearly nearer to it,
                   it is that one's, and this piece has none (a tile of the sea that came into view, or one cut off
                   at the screen's edge into another number of triangles: paired with a tile some way off, it stood
                   half way to it for one picture, a blue square on the beach). */
                const Vtx *pv = &sInterpPrevVerts[sInterpPrevPrims[lo + best].first];
                uint32_t n = end - i, r0 = r > INTERP_WINDOW ? r - INTERP_WINDOW : 0, r1 = r + INTERP_WINDOW + 1 < n ? r + INTERP_WINDOW + 1 : n, r2;
                for (r2 = r0; r2 < r1; r2++) {
                    const Vtx *ov2 = &gsVerts[sInterpPrims[i + r2].first];
                    float d2 = 0.0f;
                    if (r2 == r) {
                        continue;
                    }
                    for (k = 0; k < nv && d2 < bestD * 0.6f; k++) {
                        d2 += fabsf(pv[k].x - ov2[k].x) + fabsf(pv[k].y - ov2[k].y);
                    }
                    if (d2 < bestD * 0.6f) {
                        best = -1;
                        gInterpNotMutual++;
                        break;
                    }
                }
            }
            if (best >= 0) {
                const Vtx *pv = &sInterpPrevVerts[sInterpPrevPrims[lo + best].first];
                Vtx *ov = &sInterpBlendVerts[c->first];
                sInterpPrimUsed[lo + best] = 1;
                if (bestD != 0.0f && getenv("BT3_INTERP_DUMP") != NULL && (int)gGsFrame == atoi(getenv("BT3_INTERP_DUMP"))) {
                    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, px0 = 1e9f, px1 = -1e9f, py0 = 1e9f, py1 = -1e9f;
                    for (k = 0; k < nv; k++) {
                        if (cv[k].x < x0) { x0 = cv[k].x; } if (cv[k].x > x1) { x1 = cv[k].x; }
                        if (cv[k].y < y0) { y0 = cv[k].y; } if (cv[k].y > y1) { y1 = cv[k].y; }
                        if (pv[k].x < px0) { px0 = pv[k].x; } if (pv[k].x > px1) { px1 = pv[k].x; }
                        if (pv[k].y < py0) { py0 = pv[k].y; } if (pv[k].y > py1) { py1 = pv[k].y; }
                    }
                    fprintf(stderr, "piece sig %08x tris %d flat %d rank %u/%u<-%u/%u now %.0f,%.0f..%.0f,%.0f before %.0f,%.0f..%.0f,%.0f d %.0f\n", c->sig, c->tris, c->flat2d,
                            r, end - i, (unsigned)best, pEnd - lo, x0, y0, x1, y1, px0, py0, px1, py1, bestD);
                }
                bestD = 0.0f;
                for (k = 0; k < nv; k++) {
                    bestD += fabsf(pv[k].x - cv[k].x) + fabsf(pv[k].y - cv[k].y);
                }
                if (bestD != 0.0f) {
                    for (k = 0; k < nv; k++) {
                        ov[k].x = pv[k].x + (cv[k].x - pv[k].x) * t;
                        ov[k].y = pv[k].y + (cv[k].y - pv[k].y) * t;
                        ov[k].z = pv[k].z + (cv[k].z - pv[k].z) * t;
                        /* (the texture coordinates stay this picture's: a puff of an explosion shows another cell of
                           its animated texture every picture, and half way between two cells is a square of both) */
                    }
                    gInterpTrisBlended++;
                } else {
                    gInterpWhy[3]++;
                }
            } else {
                gInterpWhy[sawUv ? 2 : sawFar ? 1 : 4]++;
                /* no partner (new, or too fast): at least the camera's change is known */
                if (!c->flat2d && sInterpHaveD && interp_piece_camera(c, t)) {
                    gInterpTrisCamera++;
                }
            }
        }
        i = end;
        lo = pEnd;
    }
}

static void interp_remember(void) {
    {
        InterpPrim *swap = sInterpPrevPrims;
        sInterpPrevPrims = sInterpPrims;
        sInterpPrims = swap;
        sInterpPrevPrimCount = sInterpPrimCount;
        memcpy(sInterpPrevVerts, gsVerts, gsVertCount * sizeof(Vtx));
        sInterpPrevVertCount = gsVertCount;
    }
    memcpy(sInterpPrev, gsVuUni, gsVuUniCount * sizeof(Vu0Uniform));
    memcpy(sInterpPrevSig, sInterpSig, gsVuUniCount * sizeof(uint32_t));
    memcpy(sInterpPrevKind, sInterpKind, gsVuUniCount * sizeof(uint16_t));
    memcpy(sInterpPrevBlkView, sInterpBlkView, gsVuUniCount * sizeof(uint32_t));
    memcpy(sInterpPrevRep, sInterpRep, gsVuUniCount * sizeof(sInterpRep[0]));
    sInterpPrevCount = gsVuUniCount;
}

/* BT3_PRESENT_LOG=<file>: one line per picture shown: time in microseconds, kind (B in-between, R the real one
   after it, S a picture shown alone), the vertical blank count. For measuring how evenly the pictures come. */
static void interp_present_log(char kind) {
    static FILE *fp;
    static int tried;
    extern unsigned gPortVBlanks;

    if (!tried) {
        tried = 1;
        fp = getenv("BT3_PRESENT_LOG") != NULL ? fopen(getenv("BT3_PRESENT_LOG"), "w") : NULL;
    }
    if (fp != NULL) {
        fprintf(fp, "%llu %c %u\n", (unsigned long long)(gpu_now() / 1000ull), kind, gPortVBlanks);
    }
}

/* How many pictures a tick is shown as: 1 = the game's own (30 a second), 2 = one in-between picture (60), 4 = three
   (120), 8 = seven (240). The setting "interp" is 0..3. */
static int interp_steps(void) {
    return gsInterp <= 0 ? 1 : gsInterp == 1 ? 2 : gsInterp == 2 ? 4 : 8;
}

static void interp_counts_restore(void) {
    int i;

    gsVertCount = sInterpSaved.verts;
    gsVuVertCount = sInterpSaved.vuVerts;
    gsVuIdxCount = sInterpSaved.vuIdx;
    gsVuUniCount = sInterpSaved.vuUni;
    gsDrawCount = sInterpSaved.draws;
    gsAnchor = sInterpSaved.anchor;
    gsSkipped = sInterpSaved.skipped;
    gsNative = sInterpSaved.native;
    for (i = 0; i < gsTargetCount && i < MAX_TARGETS; i++) {
        gsTargets[i].draws = sInterpSaved.tdraws[i];
    }
}

/* A machine that cannot show the asked number of pictures in the time of a tick gets fewer of them, or none: each
   in-between picture costs what a real one does, so asking for more than fit made the whole game run slowly (on
   an integrated GPU at 8x and "240": a picture every 186 ms, the fight at an eighth of its speed), and every
   in-between picture, its faults included, then stood for that long. sInterpCostNs: what showing one picture
   takes, smoothed; sInterpCap: the most pictures a 33 ms tick may have now (8, 4, 2 or 1). */
static uint64_t sInterpCostNs;
static int sInterpCap = 8;

static void interp_cost(uint64_t ns) {
    static int said;
    int was = sInterpCap;

    {   /* Not the first pictures of a run: they create the pipelines and load what the first screens need, and
           took long enough on any machine to switch the in-between pictures off for a moment and say so (seen in
           the user's log on a fast graphics card: "shows a picture in 12.0 ms" at the logos). */
        static unsigned seen;
        if (seen < 300) {
            seen++;
            return;
        }
    }
    if (ns > 100000000ull) { /* (one long pause, a load or a stall, does not count for more than this) */
        ns = 100000000ull;
    }
    sInterpCostNs = sInterpCostNs == 0 ? ns : (sInterpCostNs * 7 + ns) / 8;
    if (getenv("BT3_INTERP_NOCAP") != NULL) {
        return;
    }
    /* down when the pictures take more than 70% of the tick (the rest is the game's own work), up again when one
       step more would take less than half of it */
    while (sInterpCap > 1 && sInterpCostNs * (uint64_t)sInterpCap > 33366700ull * 7 / 10) {
        sInterpCap /= 2;
    }
    if (sInterpCap == was && sInterpCap < 8 && sInterpCostNs * (uint64_t)sInterpCap * 2 < 33366700ull / 2) {
        sInterpCap *= 2;
    }
    if (sInterpCap < was && sInterpCap < interp_steps() && !said) {
        said = 1;
        fprintf(stderr, "bt3: smooth motion: this machine shows a picture in %.1f ms; fewer in-between pictures are shown while that lasts\n",
                (double)sInterpCostNs / 1000000.0);
    }
}

/* Shows picture `step` of the tick's sInterpSteps: the list replayed `step / steps` of the way from the previous
   tick; the last one is the tick itself. The recorded list has to be in place (interp_counts_restore). */
static void interp_show(int step) {
    if (step < sInterpSteps) {
        Vu0Uniform *real = gsVuUni;
        Vtx *realVerts = gsVerts;
        float t = getenv("BT3_INTERP_T") != NULL ? (float)atof(getenv("BT3_INTERP_T")) : (float)step / (float)sInterpSteps;

        if (step > 1) {
            interp_build(t, 0); /* (the first was built to decide whether the tick can be blended at all) */
        }
        if (getenv("BT3_INTERP_STEPLOG") != NULL) {
            fprintf(stderr, "step %d of %d, t %.3f, frame %u\n", step, sInterpSteps, t, gGsFrame);
        }
        if (getenv("BT3_INTERP_3D") == NULL) { /* (BT3_INTERP_3D=1: the vertex programs' draws only, as the first prototype) */
            interp_build_verts(t);
            interp_wall(t);
            gsVerts = sInterpBlendVerts;
        }
        gsVuUni = sInterpBlend;
        if (getenv("BT3_INTERP_REALONLY") == NULL) { /* (testing: only the ticks' real pictures are shown, numbered as without in-between pictures) */
            uint64_t t0 = gpu_now();
            sBackend->frameEnd();
            interp_cost(gpu_now() - t0);
        } else {
            gsVertCount = gsVuVertCount = gsVuIdxCount = gsVuUniCount = gsDrawCount = 0; /* (as the back end leaves them) */
        }
        interp_wall_restore();
        gsVuUni = real;
        gsVerts = realVerts;
        interp_present_log('B');
        gInterpShown++;
    } else {
        interp_remember(); /* (only now: the in-between pictures were built from the previous tick's copy) */
        interp_wall_real();
        {
            uint64_t t0 = gpu_now();
            sBackend->frameEnd();
            interp_cost(gpu_now() - t0);
        }
        interp_wall_restore();
        interp_present_log('R');
        scale_apply();
        menu_labels_apply();
    }
}

/* The tick's pictures after the first. They are due at even distances over the tick (33.4 ms / steps), counted from
   when the first was shown: that is when the game had finished computing the tick, a millisecond or two after a
   vertical blank, so "at the next blank" came too early (measured with one in-between picture: 15 ms after it, then
   18 ms to the next). Called from Port_VBlank before it waits for the blank (`ahead` = what may be waited for here:
   the pictures that fall before the blank) and after (those at it).
   all = 1: the game is back with a new tick: what is left is dropped, except the tick's real picture. */
void GsGpu_InterpPump(int ahead_ms, int all) {
    if (sInterpNext == 0 || sBackend == NULL) {
        return;
    }
    if (gsDrawCount != 0 || gsVertCount != 0 || gsVuUniCount != 0) {
        /* the game drew before the tick's pictures were shown: its new draws would be mixed into the kept list */
        static int said;
        if (!said) {
            said = 1;
            fprintf(stderr, "bt3: interp: draws recorded before the tick's pictures were shown; they are dropped\n");
        }
        sInterpNext = 0;
        return;
    }
    while (sInterpNext != 0) {
        uint64_t due = sInterpBlendShown + (uint64_t)(sInterpNext - 1) * (sInterpTickNs / (uint64_t)sInterpSteps), now = gpu_now();

        due = due > sInterpBlendTook ? due - sInterpBlendTook : 0; /* (replaying the list takes that long again) */
        if (all) {
            sInterpNext = sInterpSteps;
        } else if (getenv("BT3_UNCAPPED") == NULL) {
            if (ahead_ms < 0) {
                /* before a vertical blank: only a picture that is shown and done with before the blank is due,
                   so that the blank (the game's clock: input, sound) is never made late */
                extern int Port_VBlankUsLeft(void);
                uint64_t left = (uint64_t)Port_VBlankUsLeft() * 1000ull, wait = now < due ? due - now : 0;
                if (wait + sInterpBlendTook + 700000ull > left) {
                    return;
                }
            } else if (now < due && due - now > (uint64_t)ahead_ms * 1000000ull) {
                return; /* not yet: at the next blank */
            }
            if (now < due) {
                SDL_DelayPrecise(due - now);
            }
        }
        if (sInterpNext == sInterpSteps && getenv("BT3_INTERP_ONLY") != NULL) {
            sInterpNext = 0; /* testing: only in-between pictures are shown (so a screenshot holds one) */
            interp_counts_restore();
            interp_remember();
            gsVertCount = gsVuVertCount = gsVuIdxCount = gsVuUniCount = gsDrawCount = 0;
            return;
        }
        interp_counts_restore();
        {
            int step = sInterpNext;
            sInterpNext = step == sInterpSteps ? 0 : step + 1;
            interp_show(step);
        }
    }
}

void GsGpu_InterpFlush(void) { /* (the name Port_VBlank knew it by) */
    GsGpu_InterpPump(6, 0);
}

/* ---- frame skipping ------------------------------------------------------------------------------------------
 * The game advances one step per picture and knows no other clock, so a machine that needs longer than a tick
 * to show a picture ran the game itself slowly. When the game is more than a vertical blank behind its grid as a
 * picture is finished, the NEXT tick is computed without being drawn (its drawing is left out the way a re-run
 * frame's is: GS_NO_PICTURE) and nothing is shown for it; the grid (plat_stub.c: a blank that is already due is
 * not waited for) then catches up. At most kSkipMax ticks in a row, so that something is shown at least every
 * fifth tick; a machine too slow for that slows down as before. Setting "frame_skip" (F1, Video), on by default.
 * Only helps where the showing is what takes the time (a weak graphics chip), not the computing. */
int gsSkipFrame;
static int sSkipOn = -1, sSkipRun;
static unsigned sSkipped; /* (BT3_GS_VERBOSE) */

int GsDraw_FrameSkip(void) {
    if (sSkipOn < 0) {
        sSkipOn = getenv("BT3_FRAME_SKIP") != NULL ? atoi(getenv("BT3_FRAME_SKIP")) != 0 : Port_Setting("frame_skip", 1) != 0;
    }
    return sSkipOn;
}

void GsDraw_FrameSkipSet(int on) {
    sSkipOn = on != 0;
    Port_SettingSave("frame_skip", sSkipOn);
}

static void skip_decide(void) {
    extern int Port_PaceBehindUs(void);        /* plat_stub.c */
    extern volatile int gPortNetWindowClose;   /* (any picture the settings or online window waits for is shown) */
    extern int gPortOverlayOpen;
    int behind = Port_PaceBehindUs();
    int skip = GsDraw_FrameSkip() && getenv("BT3_UNCAPPED") == NULL && getenv("BT3_SHOT") == NULL && !gPortOverlayOpen &&
               behind > (int)(Port_BlankNs() / 1000ull) && sSkipRun < 4;

    if (getenv("BT3_SKIP_FORCE") != NULL) { /* testing: every other tick is left out, whatever the clock says */
        skip = sSkipRun == 0;
    }

    (void)gPortNetWindowClose;
    sSkipRun = skip ? sSkipRun + 1 : 0;
    sSkipped += (unsigned)skip;
    gsSkipFrame = skip;
    if (getenv("BT3_GS_VERBOSE") != NULL && gGsFrame % 120 == 0 && sSkipped != 0) {
        fprintf(stderr, "skip: %u ticks were not shown in the last 120 pictures (the game was %d.%d ms behind just now)\n", sSkipped, behind / 1000, behind / 100 % 10);
        sSkipped = 0;
    }
}

void GsGpu_FrameEnd(void) {
    if (gPortResim) { /* a frame that is only being re-run: nothing was recorded, nothing is shown */
        return;
    }
    if (gsSkipFrame) {
        /* A skipped tick: nothing was recorded. The tick before's last pictures, if any are still waiting, are
           shown now (its real one); nothing can be blended over the gap, so the next tick has no in-between
           picture. */
        if (sInterpNext != 0) {
            GsGpu_InterpPump(0, 1);
        }
        sInterpPrevCount = 0;
        sInterpPrevPrimCount = 0;
        gsVertCount = gsVuVertCount = gsVuIdxCount = gsVuUniCount = gsDrawCount = 0;
        skip_decide();
        return;
    }
    uint64_t t0 = gpu_now();
    if (sBackend != NULL) {
        int i;

        if (sInterpNext != 0) { /* (pictures still waiting: the game did not reach its vertical blanks in between) */
            uint32_t verts = gsVertCount, vuVerts = gsVuVertCount, vuIdx = gsVuIdxCount, vuUni = gsVuUniCount, draws = gsDrawCount;
            /* cannot happen in the order the game works in (the list of the new tick is complete here); if it does,
               the old tick's pictures are gone with its list */
            if (verts != 0 || vuVerts != 0 || vuIdx != 0 || vuUni != 0 || draws != 0) {
                sInterpNext = 0;
            }
        }
        if (gsInterp < 0) {
            gsInterp = getenv("BT3_INTERP") != NULL ? atoi(getenv("BT3_INTERP")) : Port_Setting("interp", 0);
            gsInterp = gsInterp < 0 ? 0 : gsInterp > 3 ? 3 : gsInterp;
            sInterpPrev = malloc(MAX_VU_UNIFORMS * sizeof(Vu0Uniform));
            sInterpBlend = malloc(MAX_VU_UNIFORMS * sizeof(Vu0Uniform));
            sInterpPrevSig = malloc(MAX_VU_UNIFORMS * sizeof(uint32_t));
            sInterpSig = malloc(MAX_VU_UNIFORMS * sizeof(uint32_t));
            sInterpPrevVerts = malloc(MAX_VERTS * sizeof(Vtx));
            sInterpBlendVerts = malloc(MAX_VERTS * sizeof(Vtx));
            sInterpPrevPrims = malloc(INTERP_MAX_PRIMS * sizeof(InterpPrim));
            sInterpPrims = malloc(INTERP_MAX_PRIMS * sizeof(InterpPrim));
            sInterpPrimUsed = malloc(INTERP_MAX_PRIMS);
        }
        {
            /* A scene the game shows at 60 a second (one vertical blank per picture: menus, the character select) has
               half the time per tick and needs half the pictures: none at "60", one at "120". How many blanks this
               tick will last is taken from the last one. */
            extern unsigned gPortVBlanks;
            unsigned blanks = gPortVBlanks - sInterpLastBlank;
            sInterpLastBlank = gPortVBlanks;
            blanks = blanks < 1 ? 1 : blanks > 2 ? 2 : blanks;
            sInterpSteps = (interp_steps() < sInterpCap ? interp_steps() : sInterpCap) * (int)blanks / 2;
            sInterpTickNs = (uint64_t)blanks * Port_BlankNs();
        }
        if (gsInterp > 0) {
            interp_signatures(sInterpSig);
            sInterpPrimCount = interp_prims(sInterpPrims);
        }
        if (gsInterp > 0 && sInterpSteps > 1 && interp_build(getenv("BT3_INTERP_T") != NULL ? (float)atof(getenv("BT3_INTERP_T")) : 1.0f / (float)sInterpSteps, 1)) {
            uint64_t before;

            sInterpSaved.verts = gsVertCount;
            sInterpSaved.vuVerts = gsVuVertCount;
            sInterpSaved.vuIdx = gsVuIdxCount;
            sInterpSaved.vuUni = gsVuUniCount;
            sInterpSaved.draws = gsDrawCount;
            sInterpSaved.anchor = gsAnchor;
            sInterpSaved.skipped = gsSkipped;
            sInterpSaved.native = gsNative;
            for (i = 0; i < gsTargetCount && i < MAX_TARGETS; i++) {
                sInterpSaved.tdraws[i] = gsTargets[i].draws;
            }
            before = gpu_now();
            interp_show(1); /* the first in-between picture; the others and the real one follow (GsGpu_InterpPump) */
            sInterpBlendShown = gpu_now();
            sInterpBlendTook = sInterpBlendShown - before;
            if (sInterpBlendTook > 4000000ull) {
                sInterpBlendTook = 4000000ull;
            }
            sInterpNext = 2;
            if (getenv("BT3_INTERP_LOG") != NULL && (gInterpTicks++ % 300) == 299) {
                fprintf(stderr, "interp: %u in-between pictures shown, %u ticks without; blocks: %u blended, %u not\n", gInterpShown, gInterpSkipped, gInterpPaired, gInterpUnpaired);
                fprintf(stderr, "interp:   of those without a partner, %u moved with the camera\n", gInterpCamera);
                fprintf(stderr, "interp:   effects and 2D: %u triangles, %u moved, %u still; not paired: %u new kind, %u too far, %u other texture piece, %u none free\n",
                        gInterpTris, gInterpTrisBlended, gInterpWhy[3], gInterpWhy[0], gInterpWhy[1], gInterpWhy[2], gInterpWhy[4]);
                fprintf(stderr, "interp:   pieces moved by the camera's change: %u, of them of kinds that stand still: %u\n", gInterpTrisCamera, gInterpStatic);
                gInterpStatic = 0;
                fprintf(stderr, "interp:   ticks where the camera's way went through the scenery: %u; cuts: %u\n", gInterpThrough, gInterpCuts);
                gInterpTrisCamera = 0;
                memset(gInterpWhy, 0, sizeof(gInterpWhy));
                gInterpPaired = gInterpUnpaired = gInterpCamera = gInterpTris = gInterpTrisBlended = 0;
            }
        } else {
            uint64_t t1;
            if (gsInterp > 0) {
                interp_remember();
                gInterpSkipped++;
            }
            t1 = gpu_now();
            sBackend->frameEnd();
            interp_cost(gpu_now() - t1);
            interp_present_log('S');
            scale_apply();
            menu_labels_apply();
        }
    }
    gGpuEndNs += gpu_now() - t0;
    skip_decide();
}
