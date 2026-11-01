/*
 * Graphics Synthesizer, Vulkan back end (SDL3 GPU API: Vulkan on Linux and Android, Direct3D 12 / Metal elsewhere).
 *
 * gs_draw.c still records the frame (every primitive as a GsDraw, the vertices, the decoded textures); this
 * file owns the GPU objects and at the end of the frame uploads what is new, replays the recorded list on
 * the GPU, presents it, draws the settings window over it and writes the screenshots.
 *   render targets   one per GS frame-buffer address in use, GS_W x GS_H GS pixels at SCALE times the PS2's
 *                    resolution, each with its own depth buffer; the colour / aux / depth attachments of
 *                    gsTargets[i] live in sTgCol / sTgAux / sTgDep[i]
 *   textures         made from what gs_draw.c decoded, uploaded in the copy pass of the frame that needed them
 *   blending         the key gs_draw.c built out of the GS alpha register (bits 0..8) back into SDL blend
 *                    factors; the rest of the key (depth, topology, write mask, vertices) is in the same
 *                    pipeline creation call
 *   alpha            1.0 = 0x80, so that "source alpha" blending needs no extra scaling
 * Not handled yet: a target sampled while it is being drawn to, region clamp / repeat, destination-alpha tests,
 * frame-buffer masks, 16-bit targets' precision.
 */
#include <pthread.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "gs_internal.h"
#include "gs_draw.h"
#include "shaders.h" /* generated: kGsVertSpv, kGsFragSpv */
#include "ui.h"

extern int Port_Setting(const char *name, int def); /* plat_settings.c: the saved settings */
extern int Port_AspectMilli(void);

static SDL_Window *sWindow; /* the shared window (GsDraw_WindowCreate) */
static SDL_GPUDevice *sDev;
static SDL_GPUTextureFormat sDepthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
static int sShadowStencil;
static SDL_GPUShader *sVs, *sFs, *sFsNz, *sFsWb; /* (Nz: gs.frag without its depth output, key bit 23; Wb: and with the glow chain's sampling, bit 25) */
static SDL_GPUBuffer *sVbuf;
static SDL_GPUTransferBuffer *sVxfer;
static SDL_GPUSampler *sSamplers[16]; /* bit 0 filtered, bit 1 / 2 clamped in u / v, bit 3 with the smaller copies (mip levels) */
static SDL_GPUTexture *sWhite;
static SDL_GPUGraphicsPipeline *sOutlinePipe, *sKeyPipe;
/* A script mod's look pass (native effect 6, shaders/grade.frag; gs_draw.c grade_add): its pipeline and the copy of
   the picture it reads, and the scene's average colour followed slowly (avg.frag) for its colour balance. */
static SDL_GPUGraphicsPipeline *sGradePipe, *sAvgPipe;
static SDL_GPUTexture *sGradeCopy, *sBalance; /* (the copy has mipmaps; sBalance: 1 x 1) */
static SDL_GPUSampler *sGradeSampler;         /* linear, between mipmaps too */
static Uint32 sGradeW, sGradeH, sGradeLevels;
static int sBalanceFresh = 1;
static SDL_GPUTexture *sFbTex[2]; /* pictures uploaded into the two display buffers (movies) */
static SDL_GPUShader *sFxVs, *sDclutFs;
static SDL_GPUTexture *sDateCopy; /* the alpha bytes as they were before a run of draws that test them (TEST.DATE) */
static SDL_GPUTexture *sAuxCopy;  /* the alpha bytes, copied so that a pass can read them while it writes them */
/* The attachments of gsTargets[i]: what the shared table holds is GS state only, this is the GPU side of it. */
static SDL_GPUTexture *sTgCol[MAX_TARGETS], *sTgAux[MAX_TARGETS], *sTgDep[MAX_TARGETS];
static SDL_GPUBuffer *sVuVbuf;
static SDL_GPUTransferBuffer *sVuXfer;
static SDL_GPUBuffer *sVuIbuf;          /* gsVuIdx */
static SDL_GPUTransferBuffer *sVuIXfer;
static SDL_GPUShader *sVu0Vs, *sVu4Vs, *sVu6Vs;

typedef struct Pipe {
    uint32_t key;
    SDL_GPUGraphicsPipeline *p;
    int noAux; /* 1 + the index of the same pipeline without the alpha byte (key bit 26), 0 = not looked up yet */
} Pipe;
static Pipe sPipes[1024];
static int sPipeCount;

/* textures created this frame, to upload in the copy pass */
#define MAX_PENDING 4096
static struct { SDL_GPUTexture *tex; uint32_t w, h; uint32_t *px; uint32_t bytes, level; /* bytes 0: w * h * 4 */ } sPending[MAX_PENDING];
static int sPendingCount;

static uint64_t gpu_now(void) { return SDL_GetTicksNS(); }

#ifdef __APPLE__ /* Metal: the shaders as MSL (undefined.py translates the SPIR-V with spirv-cross, entry point main0) */
#define SHADER_FORMAT SDL_GPU_SHADERFORMAT_MSL
#define SHADER_ENTRY "main0"
#define SHADER_CODE(name) name##Msl, sizeof(name##Msl) - 1
#else
#define SHADER_FORMAT SDL_GPU_SHADERFORMAT_SPIRV
#define SHADER_ENTRY "main"
#define SHADER_CODE(name) name##Spv, sizeof(name##Spv)
#endif
static SDL_GPUShader *shader(const unsigned char *code, size_t size, SDL_GPUShaderStage stage, int samplers, int ubos) {
    SDL_GPUShaderCreateInfo ci;
    SDL_zero(ci);
    ci.code = code;
    ci.code_size = size;
    ci.entrypoint = SHADER_ENTRY;
    ci.format = SHADER_FORMAT;
    ci.stage = stage;
    ci.num_samplers = (Uint32)samplers;
    ci.num_uniform_buffers = (Uint32)ubos;
    return SDL_CreateGPUShader(sDev, &ci);
}

static void pipelines_preload(void);

/* The two helper textures of the alpha bytes, as large as a render target. */
static void copies_create(void) {
    SDL_GPUTextureCreateInfo ci;
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    ci.width = GS_W * SCALE;
    ci.height = GS_H * SCALE;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    sAuxCopy = SDL_CreateGPUTexture(sDev, &ci);
    sDateCopy = SDL_CreateGPUTexture(sDev, &ci);
}

/* --- the attachments of the shared targets ------------------------------------------------------------ */

/* Spare render targets, made in quiet frames before they are needed: a target is the PS2 screen's size times the
   internal resolution in three images (colour, the helper, depth: about 130 MB at 8x), and making one took 6.5 ms;
   the first big effect of a fight asked for four new ones in one frame (26 ms). As many as a session has needed
   at most (the setting gpu_targets_peak, kept from one run to the next) and are not there yet, 4 at most; one a
   frame, after a frame drawn in under 6 ms (targets_refill). No more memory than the game takes at its peak, only
   sooner. BT3_TARGET_SPARES=0: none. */
#define TARGET_SPARES 4
static SDL_GPUTexture *sSpareCol[TARGET_SPARES], *sSpareAux[TARGET_SPARES], *sSpareDep[TARGET_SPARES];
static int sSpareCount, sTargetPeak = -1, sSpareOn = -1;

static void target_make(SDL_GPUTexture **col, SDL_GPUTexture **aux, SDL_GPUTexture **dep) {
    SDL_GPUTextureCreateInfo ci;
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = GS_W * SCALE;
    ci.height = GS_H * SCALE;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    *col = SDL_CreateGPUTexture(sDev, &ci);
    ci.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    *aux = SDL_CreateGPUTexture(sDev, &ci);
    ci.format = sDepthFormat;
    ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER; /* the depth effects read it */
    *dep = SDL_CreateGPUTexture(sDev, &ci);
}

static void target_ensure(int i) {
    extern int Port_Setting(const char *name, int def);
    extern void Port_SettingSave(const char *name, int value);
    if (sSpareCount > 0) {
        sSpareCount--;
        sTgCol[i] = sSpareCol[sSpareCount];
        sTgAux[i] = sSpareAux[sSpareCount];
        sTgDep[i] = sSpareDep[sSpareCount];
    } else {
        target_make(&sTgCol[i], &sTgAux[i], &sTgDep[i]);
    }
    if (sTargetPeak < 0) {
        sTargetPeak = Port_Setting("gpu_targets_peak", 0);
    }
    if (i + 1 > sTargetPeak) { /* (a new most, kept for the next run: written now, as nothing writes the settings at the end) */
        extern void Port_SettingsWrite(void);
        sTargetPeak = i + 1;
        Port_SettingSave("gpu_targets_peak", sTargetPeak);
        Port_SettingsWrite();
    }
}

static void targets_refill(uint64_t drawn) {
    extern int Port_Setting(const char *name, int def);
    if (sSpareOn < 0) {
        sSpareOn = getenv("BT3_TARGET_SPARES") == NULL || atoi(getenv("BT3_TARGET_SPARES")) != 0;
    }
    if (sTargetPeak < 0) {
        sTargetPeak = Port_Setting("gpu_targets_peak", 0);
    }
    if (!sSpareOn || drawn > 6000000ull || sSpareCount == TARGET_SPARES || gsTargetCount + sSpareCount >= sTargetPeak) {
        return;
    }
    target_make(&sSpareCol[sSpareCount], &sSpareAux[sSpareCount], &sSpareDep[sSpareCount]);
    sSpareCount++;
}

static void target_drop(int i) {
    SDL_ReleaseGPUTexture(sDev, sTgCol[i]);
    SDL_ReleaseGPUTexture(sDev, sTgAux[i]);
    SDL_ReleaseGPUTexture(sDev, sTgDep[i]);
}

static GsTex target_color(int i) { return (GsTex)(uintptr_t)sTgCol[i]; }
static GsTex target_aux(int i) { return (GsTex)(uintptr_t)sTgAux[i]; }
static GsTex target_depth(int i) { return (GsTex)(uintptr_t)sTgDep[i]; }

/* --- textures ------------------------------------------------------------------- */

/* The format a texture pack's replacement can be in (gs_draw.h's GS_TEXFMT_*), as SDL GPU wants it. */
static const SDL_GPUTextureFormat kTexFormat[4] = {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM,
                                                   SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM, SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM};

/* Makes a texture object; the levels' pixels come with tex_upload (the copy pass uploads them). 0 = unsupported. */
/* Spare images, made ahead in frames with time to spare: a fight's first big effects asked for 100 and more new
   textures in one frame, and making their images took 28 ms of it (about 0.23 ms each on an NVIDIA card under
   Linux). Only the textures' own kind (RGBA8, one level, a power of two up to 1024 a side), by size: of each up to
   256 x 256, 32 once it has been asked for; of a larger one, as many as the most one frame has asked for so far and
   4 more (at most 32); 96 MB at most in all. Made at the end of a
   frame drawn in under 10 ms, for 1 ms at most (pool_refill). An image let go is not taken back (the graphics chip
   may still read it). BT3_TEX_POOL=0: none. */
#define POOL_SIZES 24
#define POOL_MAX 32
static struct { uint32_t w, h; SDL_GPUTexture *spare[POOL_MAX]; int count, want, burst; unsigned frame; } sPool[POOL_SIZES];
static int sPoolCount, sPoolOn = -1;
static uint64_t sPoolBytes;

static SDL_GPUTexture *image_make(uint32_t w, uint32_t h, int format, int levels) {
    SDL_GPUTextureCreateInfo ci;
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = kTexFormat[format & 3];
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = w;
    ci.height = h;
    ci.layer_count_or_depth = 1;
    ci.num_levels = (Uint32)(levels < 1 ? 1 : levels);
    return SDL_CreateGPUTexture(sDev, &ci);
}

/* The pool of w x h (asked for now: counted in its burst), or -1 for a texture of another kind. */
static int pool_find(uint32_t w, uint32_t h, int format, int levels) {
    extern unsigned gGsFrame;
    int i;
    if (sPoolOn < 0) {
        sPoolOn = getenv("BT3_TEX_POOL") == NULL || atoi(getenv("BT3_TEX_POOL")) != 0;
    }
    if (!sPoolOn || format != GS_TEXFMT_RGBA8 || levels > 1 || w > 1024 || h > 1024 || (w & (w - 1)) || (h & (h - 1))) {
        return -1;
    }
    for (i = 0; i < sPoolCount && (sPool[i].w != w || sPool[i].h != h); i++) {
    }
    if (i == sPoolCount) {
        if (sPoolCount == POOL_SIZES) {
            return -1;
        }
        sPool[sPoolCount].w = w;
        sPool[sPoolCount].h = h;
        sPoolCount++;
    }
    if (sPool[i].frame != gGsFrame) {
        sPool[i].frame = gGsFrame;
        sPool[i].burst = 0;
    }
    if (w * h <= 65536) { /* (up to 256 x 256: 32 of them are 8 MB at most) */
        sPool[i].want = POOL_MAX;
    } else if (++sPool[i].burst + 4 > sPool[i].want) {
        sPool[i].want = sPool[i].burst + 4 < POOL_MAX ? sPool[i].burst + 4 : POOL_MAX;
    }
    return i;
}

static void pool_refill(uint64_t drawn) {
    uint64_t until;
    int i;
    if (sPoolOn != 1 || drawn > 10000000ull) {
        return;
    }
    until = gpu_now() + 1000000ull;
    for (i = 0; i < sPoolCount; i++) {
        uint64_t bytes = (uint64_t)sPool[i].w * sPool[i].h * 4;
        while (sPool[i].count < sPool[i].want && sPoolBytes + bytes <= 96ull << 20) {
            SDL_GPUTexture *t;
            if (gpu_now() >= until) {
                return;
            }
            t = image_make(sPool[i].w, sPool[i].h, GS_TEXFMT_RGBA8, 1);
            if (t == NULL) {
                return;
            }
            sPool[i].spare[sPool[i].count++] = t;
            sPoolBytes += bytes;
        }
    }
}

static GsTex tex_create(uint32_t w, uint32_t h, int format, int levels) {
    SDL_GPUTextureCreateInfo ci;
    SDL_GPUTexture *tex;
    int pool = pool_find(w, h, format, levels);

    if (pool >= 0 && sPool[pool].count > 0) {
        extern unsigned gDrawImgPooled; /* BT3_DRAW_LOG */
        gDrawImgPooled++;
        sPoolBytes -= (uint64_t)w * h * 4;
        return (GsTex)(uintptr_t)sPool[pool].spare[--sPool[pool].count];
    }

    if (!SDL_GPUTextureSupportsFormat(sDev, kTexFormat[format & 3], SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        return 0;
    }
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = kTexFormat[format & 3];
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = w;
    ci.height = h;
    ci.layer_count_or_depth = 1;
    ci.num_levels = (Uint32)(levels < 1 ? 1 : levels);
    {
        extern unsigned gDrawImgMade; /* BT3_DRAW_LOG */
        extern uint64_t gDrawImgNs;
        uint64_t t0 = gpu_now();
        tex = SDL_CreateGPUTexture(sDev, &ci);
        gDrawImgNs += gpu_now() - t0;
        gDrawImgMade++;
    }
    return (GsTex)(uintptr_t)tex;
}

/* Queues a level's pixels for the copy pass (which frees them). Takes ownership of `px`; 0 = dropped. */
static int tex_upload(GsTex handle, int level, uint32_t w, uint32_t h, int format, const void *px, uint32_t bytes) {
    (void)format; /* a Vulkan texture is made with its format; the pixels arrive as they are */
    if (sPendingCount == MAX_PENDING) {
        free((void *)px);
        return 0;
    }
    sPending[sPendingCount].tex = (SDL_GPUTexture *)(uintptr_t)handle;
    sPending[sPendingCount].w = w;
    sPending[sPendingCount].h = h;
    sPending[sPendingCount].px = (uint32_t *)px;
    sPending[sPendingCount].bytes = bytes;
    sPending[sPendingCount].level = (uint32_t)level;
    {
        extern uint64_t gDrawUpBytes; /* BT3_DRAW_LOG */
        gDrawUpBytes += bytes;
    }
    sPendingCount++;
    return 1;
}

static void tex_destroy(GsTex tex) {
    extern unsigned gDrawImgFreed; /* BT3_DRAW_LOG */
    gDrawImgFreed++;
    SDL_ReleaseGPUTexture(sDev, (SDL_GPUTexture *)(uintptr_t)tex);
}

static int tex_slots_left(void) { return MAX_PENDING - sPendingCount; }

static GsTex white_tex(void) { return (GsTex)(uintptr_t)sWhite; }

/* gsScale just changed: the shared table is emptied after this returns; every attachment and helper sized
   to the old resolution is dropped here and the helpers are made again at the new size. */
static void scale_changed(void) {
    int i;
    SDL_WaitForGPUIdle(sDev);
    for (i = 0; i < gsTargetCount; i++) {
        target_drop(i);
    }
    while (sSpareCount > 0) { /* (the spares are the old size) */
        sSpareCount--;
        SDL_ReleaseGPUTexture(sDev, sSpareCol[sSpareCount]);
        SDL_ReleaseGPUTexture(sDev, sSpareAux[sSpareCount]);
        SDL_ReleaseGPUTexture(sDev, sSpareDep[sSpareCount]);
    }
    SDL_ReleaseGPUTexture(sDev, sAuxCopy);
    SDL_ReleaseGPUTexture(sDev, sDateCopy);
    copies_create();
}

/* The settings window (F1, ui.cpp) and the shared window it lives in are gs_draw.c's: what the user changes
   there reaches this back end through gsScale, gsFxOff and gsGlowPercent. */

/* --- measuring which kind of pass costs what (branch native-fx) -------------------------------------
   Every draw is one of: 1 into another buffer than the picture (the glow chain, the blur by depth, the haze, the
   shadow page), 2 onto the picture reading a buffer drawn before (an effect put back), 4 onto the picture through a
   vertex program (the fighters, the stage, 3D effects), 8 onto the picture from GS vertices (sprites: auras,
   flashes, rays, smoke, particles, the HUD). BT3_FX_SKIP=<bits>: those are left out
   (the picture is wrong: only for the time it takes). BT3_FX_STAT=<n>: every n frames, per kind and per buffer, the
   draws and the pixels of their scissors. */
static struct { double px[4]; unsigned draws[4]; unsigned fbp[64]; double fpx[64]; unsigned fdraws[64]; int nf; unsigned frames; } sFx;
static int fx_kind(const GsDraw *d) {
    uint32_t fbp = gsTargets[d->target].fbp;
    if (fbp != 0x000 && fbp != 0x070) {
        return 0;
    }
    if (d->tex_is_target) {
        return 1;
    }
    return d->vu ? 2 : 3;
}
static int fx_measure(const GsDraw *d) {
    static int skip = -1, stat = -1;
    int k;
    if (skip < 0) {
        skip = getenv("BT3_FX_SKIP") != NULL ? atoi(getenv("BT3_FX_SKIP")) : 0;
        stat = getenv("BT3_FX_STAT") != NULL ? atoi(getenv("BT3_FX_STAT")) : 0;
    }
    if (skip == 0 && stat == 0) {
        return 0;
    }
    k = fx_kind(d);
    if (stat) {
        double px = (double)d->scissor[2] * d->scissor[3];
        int i;
        sFx.px[k] += px;
        sFx.draws[k]++;
        if (k == 0) {
            for (i = 0; i < sFx.nf && sFx.fbp[i] != gsTargets[d->target].fbp; i++) {
            }
            if (i == sFx.nf && sFx.nf < 64) {
                sFx.fbp[sFx.nf++] = gsTargets[d->target].fbp;
            }
            if (i < 64) {
                sFx.fpx[i] += px;
                sFx.fdraws[i]++;
            }
        }
    }
    return (skip >> k) & 1;
}
static void fx_frame(void) {
    static const char *const names[4] = {"other buffers", "effects put back", "3D (vertex programs)", "sprites (GS vertices)"};
    int k;
    if (getenv("BT3_FX_STAT") == NULL || ++sFx.frames < (unsigned)atoi(getenv("BT3_FX_STAT"))) {
        return;
    }
    fprintf(stderr, "fx: frame %u, per frame over %u (scissor pixels in millions at this scale)\n", gGsFrame, sFx.frames);
    for (k = 0; k < 4; k++) {
        fprintf(stderr, "fx:   %-24s %7.1f draws %8.2f Mpx\n", names[k], sFx.draws[k] / (double)sFx.frames, sFx.px[k] / 1e6 / sFx.frames);
    }
    for (k = 0; k < sFx.nf; k++) {
        fprintf(stderr, "fx:     buffer %03x %7.1f draws %8.2f Mpx\n", sFx.fbp[k], sFx.fdraws[k] / (double)sFx.frames, sFx.fpx[k] / 1e6 / sFx.frames);
    }
    memset(&sFx, 0, sizeof(sFx));
}

/* BT3_LAG_LOG: when the graphics chip has finished a frame handed over ("lag G", after the frame's "lag S"): a thread
   of its own waits for each frame's fence in turn. Only with the log. */
static SDL_GPUFence *sLagFences[16];
static unsigned sLagHead, sLagTail;
static pthread_mutex_t sLagLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sLagWake = PTHREAD_COND_INITIALIZER;
static void *lag_waiter(void *unused) {
    extern void Port_LagMark(int what);
    (void)unused;
    for (;;) {
        SDL_GPUFence *f;
        pthread_mutex_lock(&sLagLock);
        while (sLagHead == sLagTail) {
            pthread_cond_wait(&sLagWake, &sLagLock);
        }
        f = sLagFences[sLagTail++ % 16];
        pthread_mutex_unlock(&sLagLock);
        if (f != NULL) {
            SDL_WaitForGPUFences(sDev, true, &f, 1);
            Port_LagMark('G');
            SDL_ReleaseGPUFence(sDev, f);
        }
    }
    return NULL;
}
static void lag_fence(SDL_GPUFence *f) {
    static int started;
    pthread_mutex_lock(&sLagLock);
    if (!started) {
        pthread_t t;
        started = 1;
        pthread_create(&t, NULL, lag_waiter, NULL);
    }
    if (sLagHead - sLagTail < 16) {
        sLagFences[sLagHead++ % 16] = f;
        pthread_cond_signal(&sLagWake);
    } else {
        SDL_ReleaseGPUFence(sDev, f); /* (the waiter is behind: this one is not timed) */
    }
    pthread_mutex_unlock(&sLagLock);
}

/* --- pipelines ---------------------------------------------------------------------------------- */

/* Where the keys of the pipelines the game has used are remembered between runs. Creating a pipeline takes the
   driver 2 to 10 ms (measured: 46 ms for the ten of the first stage frame), so the known ones are made at start. */
static const char *pipeline_file(void) {
    return getenv("BT3_PIPELINES") != NULL ? getenv("BT3_PIPELINES") : "bt3_pipelines.txt";
}

/* The blend state of a key made by gs_draw.c's blend_key (bits 0..8: enabled, k source, k destination, C). */
static void blend_from_key(uint32_t bkey, SDL_GPUColorTargetBlendState *b) {
    int k[2] = {(int)((bkey >> 1) & 7), (int)((bkey >> 4) & 7)}, C = (int)((bkey >> 7) & 3), i;
    SDL_GPUBlendFactor fc = C == 0 ? SDL_GPU_BLENDFACTOR_SRC_ALPHA : C == 1 ? SDL_GPU_BLENDFACTOR_DST_ALPHA : SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
    SDL_GPUBlendFactor fi = C == 0 ? SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA : C == 1 ? SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA : SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
    SDL_GPUBlendFactor f[2];

    SDL_zerop(b);
    if (!(bkey & 1)) {
        return;
    }
    for (i = 0; i < 2; i++) {
        f[i] = k[i] == 0 ? SDL_GPU_BLENDFACTOR_ZERO : k[i] == 1 || k[i] == 5 ? SDL_GPU_BLENDFACTOR_ONE : k[i] == 4 ? fi : fc;
    }
    if (bkey & (1u << 9)) {
        f[0] = SDL_GPU_BLENDFACTOR_ONE; /* (the shader scaled the source by its alpha: gs_draw.c, blend_key) */
    }
    b->enable_blend = true;
    b->src_color_blendfactor = f[0];
    b->dst_color_blendfactor = f[1];
    b->color_blend_op = k[0] == 3 ? SDL_GPU_BLENDOP_REVERSE_SUBTRACT : k[1] == 3 ? SDL_GPU_BLENDOP_SUBTRACT : SDL_GPU_BLENDOP_ADD;
    b->src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE; /* the frame buffer keeps the source alpha */
    b->dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    b->alpha_blend_op = SDL_GPU_BLENDOP_ADD;
}

/* Creates the pipeline of a key (everything a pipeline depends on is in the key):
   bits 0..9 blend, 10..11 depth test, 12 depth write, 13..14 topology, 16..19 colour write mask,
   20..21 vertices: 0 GS vertices, 1 program 0, 2 program 4, 3 program 6, 23 gs.frag without its depth output,
   24 the flat-shadow stencil test, 25 gswb.frag, 26 the alpha byte left alone (aux_plan). */
static int pipeline_create(uint32_t key, int remember) {
    int ztst = (int)((key >> 10) & 3), zwrite = (int)((key >> 12) & 1), topo = (int)((key >> 13) & 3), vu = (int)((key >> 20) & 3);
    int fx = (int)((key >> 22) & 1); /* a full-screen table pass (dclut.frag): no vertices, no depth attachment */
    uint32_t wmask = (key >> 16) & 15;
    SDL_GPUColorTargetDescription cds[2];
    SDL_GPUGraphicsPipelineCreateInfo ci;
    SDL_GPUVertexBufferDescription vb;
    SDL_GPUVertexAttribute at[3];
    uint64_t t0;

    if (sPipeCount == 1024) {
        return 0;
    }
    SDL_zero(ci);
    SDL_zero(vb);
    SDL_zero(at);
    SDL_zero(cds);
    cds[0].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    blend_from_key(key & 0x3FF, &cds[0].blend_state);
    cds[0].blend_state.color_write_mask = (SDL_GPUColorComponentFlags)wmask;
    cds[0].blend_state.enable_color_write_mask = true;
    vb.slot = 0;
    vb.pitch = sizeof(Vtx);
    vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    at[0].location = 0; at[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; at[0].offset = 0;
    at[1].location = 1; at[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM; at[1].offset = 12;
    at[2].location = 2; at[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; at[2].offset = 16;
    if (vu) { /* the vertex program's own vertices: three float quadwords */
        vb.pitch = 48;
        at[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4; at[0].offset = 0;
        at[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4; at[1].offset = 16;
        at[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4; at[2].offset = 32;
    }
    ci.vertex_shader = vu == 3 ? sVu6Vs : vu == 2 ? sVu4Vs : vu ? sVu0Vs : sVs;
    ci.fragment_shader = (key >> 25) & 1 ? sFsWb : (key >> 23) & 1 ? sFsNz : sFs;
    ci.vertex_input_state.vertex_buffer_descriptions = &vb;
    ci.vertex_input_state.num_vertex_buffers = 1;
    ci.vertex_input_state.vertex_attributes = at;
    ci.vertex_input_state.num_vertex_attributes = 3;
    ci.primitive_type = topo == 0 ? SDL_GPU_PRIMITIVETYPE_TRIANGLELIST : topo == 1 ? SDL_GPU_PRIMITIVETYPE_LINELIST : SDL_GPU_PRIMITIVETYPE_POINTLIST;
    ci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    ci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    ci.depth_stencil_state.enable_depth_test = true;
    ci.depth_stencil_state.enable_depth_write = zwrite;
    ci.depth_stencil_state.compare_op = ztst == 0 ? SDL_GPU_COMPAREOP_NEVER : ztst == 1 ? SDL_GPU_COMPAREOP_ALWAYS :
                                        ztst == 2 ? SDL_GPU_COMPAREOP_GREATER_OR_EQUAL : SDL_GPU_COMPAREOP_GREATER;
    if (key & (1u << 24)) {
        SDL_GPUStencilOpState op;
        SDL_zero(op);
        op.fail_op = SDL_GPU_STENCILOP_KEEP;
        op.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        op.pass_op = SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP;
        op.compare_op = SDL_GPU_COMPAREOP_EQUAL;
        ci.depth_stencil_state.front_stencil_state = op;
        ci.depth_stencil_state.back_stencil_state = op;
        ci.depth_stencil_state.compare_mask = 0xff;
        ci.depth_stencil_state.write_mask = 0xff;
        ci.depth_stencil_state.enable_stencil_test = true;
    }
    /* the exact alpha byte: never blended, written whenever the frame's alpha is writable */
    cds[1].format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    cds[1].blend_state.color_write_mask = (wmask & SDL_GPU_COLORCOMPONENT_A) && !(key & (1u << 26)) ? SDL_GPU_COLORCOMPONENT_R : 0;
    cds[1].blend_state.enable_color_write_mask = true;
    ci.target_info.color_target_descriptions = cds;
    ci.target_info.num_color_targets = 2;
    ci.target_info.depth_stencil_format = sDepthFormat;
    ci.target_info.has_depth_stencil_target = true;
    if (fx) {
        ci.vertex_shader = sFxVs;
        ci.fragment_shader = sDclutFs;
        ci.vertex_input_state.num_vertex_buffers = 0;
        ci.vertex_input_state.num_vertex_attributes = 0;
        ci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        ci.depth_stencil_state.enable_depth_test = false;
        ci.depth_stencil_state.enable_depth_write = false;
        ci.target_info.has_depth_stencil_target = false;
    }
    sPipes[sPipeCount].key = key;
    t0 = gpu_now();
    sPipes[sPipeCount].p = SDL_CreateGPUGraphicsPipeline(sDev, &ci);
    gGpuPipeNs += gpu_now() - t0;
    {
        extern uint64_t gDrawPipeNs;
        gDrawPipeNs += gpu_now() - t0;
    }
    if (sPipes[sPipeCount].p == NULL) {
        fprintf(stderr, "bt3: pipeline %08x: %s\n", key, SDL_GetError());
        return 0;
    }
    if (remember) { /* first seen in this run: known from the next start on */
        FILE *f = fopen(pipeline_file(), "a");
        if (f != NULL) {
            fprintf(f, "%08x\n", key);
            fclose(f);
        }
        gGpuNewPipes++;
    }
    return sPipeCount++;
}

/* Creates every pipeline an earlier run used. */
static void pipelines_preload(void) {
    FILE *f = fopen(pipeline_file(), "r");
    unsigned key;
    int i, n = 0;
    uint64_t t0 = gpu_now();

    if (f == NULL) {
        return;
    }
    while (fscanf(f, "%x", &key) == 1) {
        if (!sShadowStencil) { key &= ~(1u << 24); }
        for (i = 0; i < sPipeCount && sPipes[i].key != key; i++) {
        }
        if (i == sPipeCount && (key & ~0x7FF7FFFu) == 0) {
            pipeline_create(key, 0);
            n++;
        }
    }
    fclose(f);
    fprintf(stderr, "bt3: %d pipelines of earlier runs created in %.0f ms (%s)\n", n, (double)(gpu_now() - t0) / 1e6, pipeline_file());
}

/* The pipeline of a key gs_draw.c derived from GS state: cached here, created on first sight. */
static int pipe_get(uint32_t key) {
    int i;
    if (!sShadowStencil) { key &= ~(1u << 24); }
    for (i = 0; i < sPipeCount; i++) {
        if (sPipes[i].key == key) {
            return i;
        }
    }
    return pipeline_create(key, 1);
}

/* How a finished picture reaches the display. on = 0: without waiting for it (MAILBOX: no tearing, no wait; else
   IMMEDIATE); on = 1: at its refresh (VSYNC, the settings window's "V-sync"). */
static int sVsyncPending = -1; /* asked for from the settings window: -1 nothing, else 0 / 1 */

static void vsync_apply(int on);

/* From the settings window, which is drawn inside a frame: the swapchain's picture of that frame is already taken,
   and changing how it presents then made the driver crash in the window's own drawing (found by the user: ticking
   "V-sync" closed the game). Kept until the next frame starts (frame_end, before anything of it is acquired). */
void GsGpu_Vsync(int on) {
    sVsyncPending = on != 0;
}

static void vsync_apply(int on) {
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (sDev == NULL || sWindow == NULL) {
        return;
    }
    SDL_WaitForGPUIdle(sDev); /* (nothing of an earlier frame still on its way to the old swapchain) */
    if (!on && SDL_WindowSupportsGPUPresentMode(sDev, sWindow, SDL_GPU_PRESENTMODE_MAILBOX)) {
        mode = SDL_GPU_PRESENTMODE_MAILBOX;
    } else if (!on && SDL_WindowSupportsGPUPresentMode(sDev, sWindow, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
        mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    }
    SDL_SetGPUSwapchainParameters(sDev, sWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
    fprintf(stderr, "bt3: present mode %s\n", mode == SDL_GPU_PRESENTMODE_MAILBOX ? "mailbox" : mode == SDL_GPU_PRESENTMODE_IMMEDIATE ? "immediate" : "vsync");
}

/* --- init --------------------------------------------------------------------------------------- */

static int vk_init(void) {
    SDL_GPUBufferCreateInfo bi;
    SDL_GPUTransferBufferCreateInfo ti;
    static uint32_t white = 0xFFFFFFFFu;
    const char *gpuName = NULL;
    int i;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "bt3: SDL_Init: %s\n", SDL_GetError());
        return 0;
    }
    sDev = SDL_CreateGPUDevice(SHADER_FORMAT, getenv("BT3_GPU_DEBUG") != NULL, NULL);
    sShadowStencil = sDev != NULL && SDL_GPUTextureSupportsFormat(sDev, SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
        SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    sDepthFormat = sShadowStencil ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT : SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    if (sDev != NULL && !sShadowStencil) {
        fprintf(stderr, "bt3: shadow stencil unavailable; flat shadows may show overlapping model parts\n");
    }
    sWindow = GsDraw_WindowCreate(0); /* the shared window: shape, display and full screen decided there */    if (sWindow == NULL || !SDL_ClaimWindowForGPUDevice(sDev, sWindow)) {
        fprintf(stderr, "bt3: no GPU window: %s\n", SDL_GetError());
        return 0;
    }
    /* The game paces itself at 30 frames per second; waiting for the display's own refresh on top of that
       (VSYNC) costs up to a refresh period per frame. Prefer MAILBOX (no tearing, no wait), then IMMEDIATE. */
    vsync_apply(Port_Setting("vsync", 0)); /* (the setting "V-sync" asks for the wait all the same) */
    sVs = shader(SHADER_CODE(kGsVert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    sFs = shader(SHADER_CODE(kGsFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 1);
    sFsNz = shader(SHADER_CODE(kGsnzFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 1);
    sFsWb = shader(SHADER_CODE(kGswbFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 1);
    if (sVs == NULL || sFs == NULL || sFsNz == NULL || sFsWb == NULL) {
        fprintf(stderr, "bt3: shaders: %s\n", SDL_GetError());
        return 0;
    }
    SDL_zero(bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bi.size = MAX_VERTS * sizeof(Vtx);
    sVbuf = SDL_CreateGPUBuffer(sDev, &bi);
    SDL_zero(ti);
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    ti.size = MAX_VERTS * sizeof(Vtx);
    sVxfer = SDL_CreateGPUTransferBuffer(sDev, &ti);
    for (i = 0; i < 16; i++) {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter = si.mag_filter = (i & 1) ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        /* The game's own textures have one level. A texture pack's have smaller copies as well, used (bit 3)
           where the texture is on 3D geometry. Not for 2D art: a smaller copy averages neighbouring texels, and
           on a sheet of HUD pieces those are another piece (a strip of the next colour layer showed in a health
           bar once widescreen drew the HUD narrower and the smaller copies came into use). */
        si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        si.max_lod = (i & 8) ? 16.0f : 0.0f;
        si.address_mode_u = (i & 2) ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_v = (i & 4) ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sSamplers[i] = SDL_CreateGPUSampler(sDev, &si);
    }
    sVu0Vs = shader(SHADER_CODE(kVu0Vert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    sVu4Vs = shader(SHADER_CODE(kVu4Vert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    sVu6Vs = shader(SHADER_CODE(kVu6Vert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    bi.size = MAX_VU_VERTS * 48;
    sVuVbuf = SDL_CreateGPUBuffer(sDev, &bi);
    ti.size = MAX_VU_VERTS * 48;
    sVuXfer = SDL_CreateGPUTransferBuffer(sDev, &ti);
    bi.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    bi.size = MAX_VU_IDX * sizeof(uint32_t);
    sVuIbuf = SDL_CreateGPUBuffer(sDev, &bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    ti.size = MAX_VU_IDX * sizeof(uint32_t);
    sVuIXfer = SDL_CreateGPUTransferBuffer(sDev, &ti);
    {
        SDL_GPUTextureCreateInfo ci;
        SDL_zero(ci);
        ci.type = SDL_GPU_TEXTURETYPE_2D;
        ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        ci.width = ci.height = 1;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        sWhite = SDL_CreateGPUTexture(sDev, &ci);
        sPending[sPendingCount].tex = sWhite;
        sPending[sPendingCount].bytes = sPending[sPendingCount].level = 0;
        sPending[sPendingCount].w = sPending[sPendingCount].h = 1;
        sPending[sPendingCount].px = malloc(4);
        memcpy(sPending[sPendingCount].px, &white, 4);
        sPendingCount++;
    }
    {   /* native outline: destination minus source on the colour channels */
        SDL_GPUShader *vs = sFxVs = shader(SHADER_CODE(kFxVert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
        SDL_GPUShader *fs = shader(SHADER_CODE(kOutlineFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
        SDL_GPUColorTargetDescription cd;
        SDL_GPUGraphicsPipelineCreateInfo ci;
        SDL_zero(cd);
        SDL_zero(ci);
        cd.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        cd.blend_state.enable_blend = true;
        cd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        cd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        cd.blend_state.color_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
        cd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        cd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        cd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        cd.blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B;
        cd.blend_state.enable_color_write_mask = true;
        ci.vertex_shader = vs;
        ci.fragment_shader = fs;
        ci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        ci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        ci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        ci.target_info.color_target_descriptions = &cd;
        ci.target_info.num_color_targets = 1;
        sOutlinePipe = vs && fs ? SDL_CreateGPUGraphicsPipeline(sDev, &ci) : NULL;
        if (sOutlinePipe == NULL) {
            fprintf(stderr, "bt3: outline pipeline: %s\n", SDL_GetError());
            return 0;
        }
        /* native alpha key: the tint blended by its own alpha, colour channels only */
        fs = shader(SHADER_CODE(kAlphakeyFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);
        cd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        cd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        cd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        ci.fragment_shader = fs;
        sKeyPipe = fs ? SDL_CreateGPUGraphicsPipeline(sDev, &ci) : NULL;
        if (sKeyPipe == NULL) {
            fprintf(stderr, "bt3: alpha key pipeline: %s\n", SDL_GetError());
            return 0;
        }
        /* the look pass: the picture's colours replaced, alpha (the objects' numbers) kept. Without it the picture
           stays as the game draws it. */
        fs = shader(SHADER_CODE(kGradeFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 3, 1);
        cd.blend_state.enable_blend = false;
        ci.fragment_shader = fs;
        sGradePipe = fs ? SDL_CreateGPUGraphicsPipeline(sDev, &ci) : NULL;
        if (fs) { SDL_ReleaseGPUShader(sDev, fs); }
        if (sGradePipe == NULL) {
            fprintf(stderr, "bt3: look pipeline: %s (the colours stay as they are)\n", SDL_GetError());
        }
        {   /* the scene's average, followed slowly: new * k + old * (1 - k), k the blend constant */
            SDL_GPUColorTargetDescription ad = cd;
            SDL_GPUGraphicsPipelineCreateInfo ai = ci;
            SDL_GPUSamplerCreateInfo si;
            SDL_GPUTextureCreateInfo tci;
            ad.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
            ad.blend_state.enable_blend = true;
            ad.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
            ad.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
            ad.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            ad.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            ad.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            ad.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            ad.blend_state.enable_color_write_mask = false;
            ai.target_info.color_target_descriptions = &ad;
            fs = shader(SHADER_CODE(kAvgFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
            ai.fragment_shader = fs;
            sAvgPipe = fs ? SDL_CreateGPUGraphicsPipeline(sDev, &ai) : NULL;
            if (fs) { SDL_ReleaseGPUShader(sDev, fs); }
            SDL_zero(si);
            si.min_filter = SDL_GPU_FILTER_LINEAR;
            si.mag_filter = SDL_GPU_FILTER_LINEAR;
            si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            si.max_lod = 1000.0f;
            sGradeSampler = SDL_CreateGPUSampler(sDev, &si);
            SDL_zero(tci);
            tci.type = SDL_GPU_TEXTURETYPE_2D;
            tci.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
            tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
            tci.width = tci.height = 1;
            tci.layer_count_or_depth = 1;
            tci.num_levels = 1;
            sBalance = SDL_CreateGPUTexture(sDev, &tci);
            if (sAvgPipe == NULL || sGradeSampler == NULL || sBalance == NULL) {
                fprintf(stderr, "bt3: look balance: %s (the colours stay as they are)\n", SDL_GetError());
                if (sGradePipe) { SDL_ReleaseGPUGraphicsPipeline(sDev, sGradePipe); sGradePipe = NULL; }
            }
        }
        cd.blend_state.enable_blend = true;
    }
    sDclutFs = shader(SHADER_CODE(kDclutFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 3, 1);
    copies_create();
#if SDL_VERSION_ATLEAST(3, 4, 0)
    {   /* the graphics card's name, for F1 > Video */
        SDL_PropertiesID props = SDL_GetGPUDeviceProperties(sDev);
        if (props != 0) {
            gpuName = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_NAME_STRING, NULL);
        }
    }
#endif
    if (!Ui_Init(sWindow, sDev, NULL, gpuName)) {
        fprintf(stderr, "bt3: the settings window could not be set up\n");
    }
    {   /* which graphics chip draws the game, and its driver (as F1 > Video shows): on a laptop with two the slower
           one can be the one used */
        const char *name = gpuName != NULL ? gpuName : "", *driver = "";
#if SDL_VERSION_ATLEAST(3, 4, 0)
        driver = SDL_GetStringProperty(SDL_GetGPUDeviceProperties(sDev), SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING, "");
#endif
        fprintf(stderr, "bt3: GPU renderer: %s%s%s%s%s\n", SDL_GetGPUDeviceDriver(sDev), name[0] ? ", " : "", name,
                driver[0] ? ", driver " : "", driver);
    }
    fprintf(stderr, "bt3: %d logical processors, %d MB of memory\n", SDL_GetNumLogicalCPUCores(), SDL_GetSystemRAM());
    pipelines_preload();
    return 1;
}

/* --- the frame ---------------------------------------------------------------------------------- */

/* The frame's clear, as the game draws it: an untextured, unblended black rectangle over the whole of its scissor,
   written whatever the depth test, at one depth (the first draw of a fight's picture: two triangles at the depth
   just above the farthest). Drawn, it is a pass of colour, alpha byte and depth over the whole picture through the
   shader that writes its own depth (8% of a fight frame on an integrated chip at 2x); the same values cleared by the
   render pass cost next to nothing. `depth`: what gs.frag would have written (its rounding to the depth buffer's
   whole numbers, worked out the same way). The whole texture is cleared, not only the scissor: nothing of a
   picture lies outside it (every draw on it is held inside), and outside it only the depth differs, which nothing
   reads. BT3_FAST_CLEAR=0: drawn as before. */
static int clear_draw(const GsDraw *d, float *depth) {
    static int on = -1;
    uint32_t key, j;
    const Vtx *v;
    float x0, y0, x1, y1, z;

    if (on < 0) { on = getenv("BT3_FAST_CLEAR") != NULL ? atoi(getenv("BT3_FAST_CLEAR")) : 1; }
    if (!on || d->native || d->vu || d->wall || d->mode[0] != 0 || d->mode[3] != 0 || d->misc[1] != 0.0f || d->misc[2] != 0.0f ||
        d->count != 6 || d->scissor[0] != 0 || d->scissor[1] != 0) {
        return 0;
    }
    key = sPipes[d->pipeline].key;
    /* no blending, depth test "always" (1), depth written, all four channels, GS vertices, no stencil */
    if ((key & 0x3FFu) != 0 || ((key >> 10) & 3) != 1 || !((key >> 12) & 1) || ((key >> 16) & 0xF) != 0xF || ((key >> 20) & 3) != 0 ||
        (key & (1u << 24)) != 0) {
        return 0;
    }
    v = &gsVerts[d->first];
    z = v[0].z;
    x0 = y0 = 1e9f;
    x1 = y1 = -1e9f;
    for (j = 0; j < 6; j++) {
        if (v[j].r != 0 || v[j].g != 0 || v[j].b != 0 || v[j].a != 0 || v[j].z != z) {
            return 0;
        }
        if (v[j].x < x0) { x0 = v[j].x; }
        if (v[j].x > x1) { x1 = v[j].x; }
        if (v[j].y < y0) { y0 = v[j].y; }
        if (v[j].y > y1) { y1 = v[j].y; }
    }
    /* the two triangles of the rectangle (0,0)..(x1,y1), and that rectangle is the scissor and the whole screen at
       least (a view of the split screen clears its half: a clear of the texture would take the other half too) */
    if (x0 != 0.0f || y0 != 0.0f || x1 < 512.0f || y1 < 448.0f ||
        x1 * (float)SCALE != (float)d->scissor[2] || y1 * (float)SCALE != (float)d->scissor[3] ||
        !(v[0].x != v[1].x || v[0].y != v[1].y) || (v[0].x == v[1].x && v[1].x == v[2].x) || (v[3].x == v[4].x && v[4].x == v[5].x)) {
        return 0;
    }
    {   /* the area of the two triangles together is the rectangle's: they do not overlap and leave nothing out */
        float a = 0.0f;
        int t;
        for (t = 0; t < 2; t++) {
            const Vtx *p = &v[t * 3];
            a += fabsf((p[1].x - p[0].x) * (p[2].y - p[0].y) - (p[2].x - p[0].x) * (p[1].y - p[0].y)) * 0.5f;
        }
        if (a != x1 * y1) {
            return 0;
        }
    }
    if (gsZMax > 0.0f && gsZMax < 2.0e7f) { /* (gs.frag, its end) */
        float zn = floorf(z * gsZMax), zq = zn / gsZMax;
        *depth = zq > z ? (zn - 1.0f) / gsZMax : zq;
    } else {
        *depth = z;
    }
    return 1;
}

/* The part of the alpha bytes that copy `k` (a native 4) has to hold: what the passes reading the copy (the table
   passes, the outline and the see-through tint) read before the next copy is taken. Each of them draws inside its
   scissor and reads its own place, the outline also up to 8 texels across and one PS2 pixel up and down: the
   scissors together, with that much more on every side. The whole buffer was copied, 2048 x 2048 at 2x, for a
   picture of 1024 x 896. 0 = no pass reads it: then the whole buffer, as before (BT3_AUX_COPY_WHOLE=1: always). */
static int aux_region(uint32_t k, int *rx, int *ry, int *rw, int *rh) {
    static int whole = -1;
    int W = GS_W * SCALE, H = GS_H * SCALE, m = 8 + 2 * SCALE, x0 = W, y0 = H, x1 = 0, y1 = 0, any = 0;
    uint32_t i;

    if (whole < 0) { whole = getenv("BT3_AUX_COPY_WHOLE") != NULL; }
    for (i = k + 1; !whole && i < gsDrawCount && gsDraws[i].native != 4; i++) {
        const GsDraw *d = &gsDraws[i];
        if (d->native == 3 || d->native == 1 || d->native == 2) {
            if (d->scissor[0] < x0) { x0 = d->scissor[0]; }
            if (d->scissor[1] < y0) { y0 = d->scissor[1]; }
            if (d->scissor[0] + d->scissor[2] > x1) { x1 = d->scissor[0] + d->scissor[2]; }
            if (d->scissor[1] + d->scissor[3] > y1) { y1 = d->scissor[1] + d->scissor[3]; }
            any = 1;
        }
    }
    if (whole || !any) {
        return 0;
    }
    x0 = x0 - m < 0 ? 0 : x0 - m;
    y0 = y0 - m < 0 ? 0 : y0 - m;
    x1 = x1 + m > W ? W : x1 + m;
    y1 = y1 + m > H ? H : y1 + m;
    if (x1 <= x0 || y1 <= y0) {
        return 0;
    }
    *rx = x0, *ry = y0, *rw = x1 - x0, *rh = y1 - y0;
    return 1;
}

/* Where each draw's alpha bytes are read again (aux_plan). A draw writes them as the GS does, into their own
   buffer next to the colour (sTgAux); they are read only by the copies a native 4 (for the outline and the
   see-through tint) and a native 5 (the destination alpha test) take, each of a known part of the buffer
   (aux_region, GsDraw_DateRegion). Past the last copy that covers a place, what a draw writes there is never
   read: the next frame of that buffer begins with its clear. An ultimate's flash, its rays and the aura's
   glow are drawn there, after the last copy of the whole screen (only the HUD's bars are tested later): some
   fifty additive layers over the whole picture in the heaviest frames, each pixel of them read and written
   again, the byte with it. So those draws, or the parts of them outside the places still read, are drawn with
   a pipeline that leaves the byte alone (key bit 26). The picture is the same. sAuxNeed: the rectangle (at
   SCALE) of each draw where its byte is read later, empty when nowhere. BT3_AUX_ALL=1: every draw writes it. */
static int16_t sAuxNeed[MAX_DRAWS][4];
static uint8_t sAuxBegan[MAX_TARGETS]; /* the previous frame of the buffer began with the clear */
static int aux_plan_on(void) { static int on = -1; if (on < 0) { on = getenv("BT3_AUX_ALL") == NULL; } return on; }
static void aux_plan(void) {
    int W = GS_W * SCALE, H = GS_H * SCALE, u[MAX_TARGETS][4], first[MAX_TARGETS], t;
    float z;
    uint32_t i;

    for (t = 0; t < MAX_TARGETS; t++) { first[t] = -1, u[t][0] = 0, u[t][1] = 0, u[t][2] = W, u[t][3] = H; }
    for (i = 0; i < gsDrawCount; i++) {
        const GsDraw *d = &gsDraws[i];
        if (d->native == 0 && first[d->target] < 0) { first[d->target] = (int)i; }
    }
    /* A buffer that does not begin with the clear, or did not the frame before: its bytes may be read in a frame
       to come before anything covers them, so everything is kept. */
    for (t = 0; t < MAX_TARGETS; t++) {
        int began;
        if (first[t] < 0) { continue; }
        began = clear_draw(&gsDraws[first[t]], &z);
        if (began && sAuxBegan[t]) { u[t][0] = W, u[t][1] = H, u[t][2] = 0, u[t][3] = 0; }
        sAuxBegan[t] = (uint8_t)began;
    }
    for (i = gsDrawCount; i-- > 0;) {
        const GsDraw *d = &gsDraws[i];
        int *r = u[d->target], x0, y0, x1, y1;
        if (d->native == 4 || d->native == 5) {
            int rx, ry, rw, rh;
            if (d->native == 4 ? aux_region(i, &rx, &ry, &rw, &rh) : GsDraw_DateRegion(i, &rx, &ry, &rw, &rh)) {
                if (rx < r[0]) { r[0] = rx; }
                if (ry < r[1]) { r[1] = ry; }
                if (rx + rw > r[2]) { r[2] = rx + rw; }
                if (ry + rh > r[3]) { r[3] = ry + rh; }
            } else if (d->native == 4 && getenv("BT3_AUX_COPY_WHOLE") != NULL) { /* (the whole buffer copied: aux_region) */
                r[0] = 0, r[1] = 0, r[2] = W, r[3] = H;
            }
            continue;
        }
        if (d->native) {
            continue;
        }
        x0 = d->scissor[0], y0 = d->scissor[1], x1 = d->scissor[0] + d->scissor[2], y1 = d->scissor[1] + d->scissor[3];
        if (!d->vu) { /* its vertices' box (GS pixels), a pixel wider on every side */
            float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
            uint32_t j;
            for (j = 0; j < d->count; j++) {
                const Vtx *v = &gsVerts[d->first + j];
                if (v->x < bx0) { bx0 = v->x; }
                if (v->x > bx1) { bx1 = v->x; }
                if (v->y < by0) { by0 = v->y; }
                if (v->y > by1) { by1 = v->y; }
            }
            if (d->count > 0 && bx0 > -1e6f && bx1 < 1e6f && by0 > -1e6f && by1 < 1e6f) {
                int b0 = (int)floorf(bx0 * (float)SCALE) - SCALE, c0 = (int)floorf(by0 * (float)SCALE) - SCALE;
                int b1 = (int)ceilf(bx1 * (float)SCALE) + SCALE, c1 = (int)ceilf(by1 * (float)SCALE) + SCALE;
                if (b0 > x0) { x0 = b0; }
                if (c0 > y0) { y0 = c0; }
                if (b1 < x1) { x1 = b1; }
                if (c1 < y1) { y1 = c1; }
            }
        }
        if (r[0] > x0) { x0 = r[0]; }
        if (r[1] > y0) { y0 = r[1]; }
        if (r[2] < x1) { x1 = r[2]; }
        if (r[3] < y1) { y1 = r[3]; }
        if (x1 <= x0 || y1 <= y0) { x0 = y0 = x1 = y1 = 0; }
        sAuxNeed[i][0] = (int16_t)x0, sAuxNeed[i][1] = (int16_t)y0, sAuxNeed[i][2] = (int16_t)x1, sAuxNeed[i][3] = (int16_t)y1;
        if (clear_draw(d, &z)) { /* before the clear nothing of this buffer is read again */
            r[0] = W, r[1] = H, r[2] = 0, r[3] = 0;
        }
    }
}
static int pipe_no_aux(int p) {
    if (sPipes[p].noAux == 0) {
        int q = pipe_get(sPipes[p].key | 1u << 26);
        sPipes[p].noAux = q >= 0 && sPipes[q].key == (sPipes[p].key | 1u << 26) ? q + 1 : p + 1;
    }
    return sPipes[p].noAux - 1;
}

static void vk_frame_end(void) {
    fx_frame();
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass *copy;
    SDL_GPURenderPass *pass = NULL;
    SDL_GPUTexture *swap = NULL;
    SDL_Event ev;
    Uint32 sw = 0, sh = 0;
    int cur = -1, best = -1, bound = -1, i;
    bool acquired;
    uint64_t sEndT[5];
    int lastPipe = -1, lastUni = -1, lastSampler = -1, haveFu = 0, haveScissor = 0, pipe, split; /* what the pass has set (the replay loop) */
    SDL_GPUTexture *lastTex = NULL;
    struct { int32_t mode[4]; float misc[4]; float rect[4]; float orig[4]; float extra[4]; } lastFu;
    int lastScissor[4];
    float lastBlend = -1.0f;
    uint32_t n;

    GsGpu_TitlePoll();
    if (sVsyncPending >= 0) {
        vsync_apply(sVsyncPending);
        sVsyncPending = -1;
    }
    {   /* "Low input lag" (plat_stub.c): no more than one picture may be on its way to the screen at a time (the
           library allows two by default, which is a picture more of delay whenever the graphics chip or V-sync
           is what paces the game). BT3_FRAMES_IN_FLIGHT=<1..3> overrides. */
        extern int Port_LateInput(void);
        static int have = -1;
        int want = getenv("BT3_FRAMES_IN_FLIGHT") != NULL ? atoi(getenv("BT3_FRAMES_IN_FLIGHT")) : Port_LateInput() ? 1 : 2;
        want = want < 1 ? 1 : want > 3 ? 3 : want;
        if (want != have) {
            have = want;
            SDL_SetGPUAllowedFramesInFlight(sDev, (Uint32)want);
        }
    }
    { extern void Port_LagMark(int what); Port_LagMark('E'); }
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) {
            exit(0);
        }
        { extern void PortA11y_Event(const SDL_Event *ev); PortA11y_Event(&ev); } /* a11y.c: the repeat key and button */
        if (PortInput_Hotkey(&ev)) { /* F1, START + SELECT, or what the settings window's list says */
            Ui_Toggle();
            continue;
        }
        if (Ui_Event(&ev)) { /* the settings window is open and used it (Esc closes it) */
            continue;
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
            if (!ev.key.repeat && !Ui_QuitAsk()) { exit(0); } /* (asks first) */
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key == SDLK_F9) { /* in-between pictures on / off */
            extern int gsInterp;
            extern void Port_SettingSave(const char *name, int value);
            extern void Port_SettingsWrite(void);
            static int was = 1; /* (the same setting as F1, Video, Smooth motion: off, or back to what it was) */
            if (gsInterp > 0) { was = gsInterp; gsInterp = 0; } else { gsInterp = was; }
            Port_SettingSave("interp", gsInterp);
            Port_SettingsWrite();
            fprintf(stderr, "bt3: smooth motion %s\n", gsInterp == 0 ? "off" : gsInterp == 1 ? "60" : gsInterp == 2 ? "120" : "240");
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key == SDLK_F11) {
            GsDraw_FullscreenToggle();
        }
    }
    if (getenv("BT3_GS_VERBOSE") != NULL && gGsFrame % 30 == 0) {
        fprintf(stderr, "gpu: frame %u: %u draws, %u vertices, %d targets, %d textures, %d pipelines, %u primitives of PS2-only passes dropped, %u native effects\n",
                gGsFrame, gsDrawCount, gsVertCount + gsVuVertCount, gsTargetCount, gsTexCount, sPipeCount, gsSkipped, gsNative);
    }
    if (getenv("BT3_GPU_CUT") != NULL && (int)gGsFrame == atoi(getenv("BT3_GPU_CUT")) && strchr(getenv("BT3_GPU_CUT"), ':') != NULL) {
        /* BT3_GPU_CUT=<frame>:<n>: only the first n draws of that frame (finding the draw that spoils a picture) */
        uint32_t n = (uint32_t)atoi(strchr(getenv("BT3_GPU_CUT"), ':') + 1);
        fprintf(stderr, "cut: frame %u has %u draws, keeping %u\n", gGsFrame, gsDrawCount, n < gsDrawCount ? n : gsDrawCount);
        if (n < gsDrawCount) {
            const GsDraw *d = &gsDraws[n];
            fprintf(stderr, "cut: first dropped draw: native %d target %d (fbp %03x) vu %d count %u tex_is_target %d mode %d %d %d %d\n", d->native,
                    d->target, d->target >= 0 ? gsTargets[d->target].fbp : 0, d->vu, d->count, d->tex_is_target, d->mode[0], d->mode[1], d->mode[2], d->mode[3]);
            gsDrawCount = n;
        }
    }
    if (getenv("BT3_GPU_DUMP") != NULL && (int)gGsFrame == atoi(getenv("BT3_GPU_DUMP")) && strchr(getenv("BT3_GPU_DUMP"), ':') != NULL) {
        /* BT3_GPU_DUMP=<frame>:<n>: every vertex of that frame's draw n (one the game transformed itself) */
        uint32_t k = (uint32_t)atoi(strchr(getenv("BT3_GPU_DUMP"), ':') + 1), j;
        if (k < gsDrawCount && !gsDraws[k].native && !gsDraws[k].vu) {
            const GsDraw *d = &gsDraws[k];
            fprintf(stderr, "dump: draw %u: %u vertices, pipeline key %08x, mode %d %d %d %d\n", k, d->count, sPipes[d->pipeline].key, d->mode[0], d->mode[1], d->mode[2], d->mode[3]);
            for (j = 0; j < d->count; j++) {
                const Vtx *v = &gsVerts[d->first + j];
                fprintf(stderr, "dump: %3u (%.2f, %.2f) z %.9f st %.4f %.4f q %.5f rgba %u %u %u %u\n", j, v->x, v->y, v->z, v->s, v->t, v->q, v->r, v->g, v->b, v->a);
            }
        }
    }
    if (getenv("BT3_GPU_TAIL") != NULL && (int)gGsFrame == atoi(getenv("BT3_GPU_TAIL"))) { /* the frame's last draws */
        uint32_t k;
        for (k = gsDrawCount > 14 ? gsDrawCount - 14 : 0; k < gsDrawCount; k++) {
            const GsDraw *d = &gsDraws[k];
            fprintf(stderr, "tail: draw %u native %d target %d vu %d first %u count %u pipeline key %08x mode %d %d %d %d misc %.0f %.0f %.0f %.0f x %.1f..\n",
                    k, d->native, d->target, d->vu, d->first, d->count, d->native ? 0 : sPipes[d->pipeline].key, d->mode[0], d->mode[1], d->mode[2],
                    d->mode[3], d->misc[0], d->misc[1], d->misc[2], d->misc[3], d->native || d->vu ? 0.0f : gsVerts[d->first].x);
            if (!d->native && !d->vu && d->count <= 12 && k + 3 >= gsDrawCount) {
                uint32_t j;
                for (j = 0; j < d->count; j++) {
                    const Vtx *v = &gsVerts[d->first + j];
                    fprintf(stderr, "tail:    (%.1f, %.1f) z %.6f st %.4f %.4f q %.3f rgba %u %u %u %u\n", v->x, v->y, v->z, v->s, v->t, v->q, v->r, v->g, v->b, v->a);
                }
            }
        }
    }
    sEndT[0] = gpu_now();
    cmd = SDL_AcquireGPUCommandBuffer(sDev);
    /* uploads: this frame's vertices and the textures decoded for it */
    copy = SDL_BeginGPUCopyPass(cmd);
    if (gsVertCount != 0) {
        SDL_GPUTransferBufferLocation src;
        SDL_GPUBufferRegion dst;
        void *p = SDL_MapGPUTransferBuffer(sDev, sVxfer, true);
        memcpy(p, gsVerts, gsVertCount * sizeof(Vtx));
        SDL_UnmapGPUTransferBuffer(sDev, sVxfer);
        src.transfer_buffer = sVxfer;
        src.offset = 0;
        dst.buffer = sVbuf;
        dst.offset = 0;
        dst.size = gsVertCount * sizeof(Vtx);
        SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    }
    if (gsVuVertCount != 0) {
        SDL_GPUTransferBufferLocation src;
        SDL_GPUBufferRegion dst;
        void *p = SDL_MapGPUTransferBuffer(sDev, sVuXfer, true);
        memcpy(p, gsVuVerts, gsVuVertCount * 48);
        SDL_UnmapGPUTransferBuffer(sDev, sVuXfer);
        src.transfer_buffer = sVuXfer;
        src.offset = 0;
        dst.buffer = sVuVbuf;
        dst.offset = 0;
        dst.size = gsVuVertCount * 48;
        SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    }
    if (gsVuIdxCount != 0) {
        SDL_GPUTransferBufferLocation src;
        SDL_GPUBufferRegion dst;
        void *p = SDL_MapGPUTransferBuffer(sDev, sVuIXfer, true);
        memcpy(p, gsVuIdx, gsVuIdxCount * sizeof(uint32_t));
        SDL_UnmapGPUTransferBuffer(sDev, sVuIXfer);
        src.transfer_buffer = sVuIXfer;
        src.offset = 0;
        dst.buffer = sVuIbuf;
        dst.offset = 0;
        dst.size = gsVuIdxCount * sizeof(uint32_t);
        SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    }
    if (sPendingCount != 0) {
        /* The frame's textures through one upload buffer, each level at its own offset (16-byte aligned), mapped
           once: a buffer made, mapped and let go for every level (a pack's picture has up to 11) cost the frame
           and the driver's hand-over after it most of a large pack's time. The buffer stays and grows; SDL hands a
           fresh one (cycle) while the last frame's is still being read. */
        static SDL_GPUTransferBuffer *tb;
        static uint32_t tbSize;
        uint32_t total = 0, at = 0;
        uint8_t *p;
        for (i = 0; i < sPendingCount; i++) {
            total += ((sPending[i].bytes ? sPending[i].bytes : sPending[i].w * sPending[i].h * 4) + 15) & ~15u;
        }
        if (tb == NULL || total > tbSize) {
            SDL_GPUTransferBufferCreateInfo ti;
            if (tb != NULL) {
                SDL_ReleaseGPUTransferBuffer(sDev, tb);
            }
            SDL_zero(ti);
            ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            ti.size = tbSize = total > (4u << 20) ? (total + 0xFFFFF) & ~0xFFFFFu : 4u << 20;
            tb = SDL_CreateGPUTransferBuffer(sDev, &ti);
        }
        p = tb != NULL ? SDL_MapGPUTransferBuffer(sDev, tb, true) : NULL;
        for (i = 0; i < sPendingCount; i++) {
            uint32_t bytes = sPending[i].bytes ? sPending[i].bytes : sPending[i].w * sPending[i].h * 4;
            if (p != NULL) {
                SDL_GPUTextureTransferInfo src;
                SDL_GPUTextureRegion dst;
                memcpy(p + at, sPending[i].px, bytes);
                SDL_zero(src);
                src.transfer_buffer = tb;
                src.offset = at;
                SDL_zero(dst);
                dst.texture = sPending[i].tex;
                dst.mip_level = sPending[i].level;
                dst.w = sPending[i].w;
                dst.h = sPending[i].h;
                dst.d = 1;
                SDL_UploadToGPUTexture(copy, &src, &dst, false);
                at += (bytes + 15) & ~15u;
            }
            free(sPending[i].px);
        }
        if (p != NULL) {
            SDL_UnmapGPUTransferBuffer(sDev, tb);
        }
    }
    sPendingCount = 0;
    /* A picture the game uploaded straight into a display buffer (a movie frame): from GS memory into a 512 x 448
       texture here, then scaled into that buffer's texture before the frame's draws (a fade goes on top). */
    for (i = 0; i < 2; i++) {
        SDL_GPUTransferBufferCreateInfo ti;
        SDL_GPUTextureTransferInfo src;
        SDL_GPUTextureRegion dst;
        SDL_GPUTransferBuffer *tb;
        uint32_t *px, x, y, fbp = i ? 0x70 : 0;
        if (!(gGsFbUploads & (1u << i))) {
            continue;
        }
        if (sFbTex[i] == NULL) {
            SDL_GPUTextureCreateInfo ci;
            SDL_zero(ci);
            ci.type = SDL_GPU_TEXTURETYPE_2D;
            ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            ci.width = GS_W;
            ci.height = GS_H;
            ci.layer_count_or_depth = 1;
            ci.num_levels = 1;
            sFbTex[i] = SDL_CreateGPUTexture(sDev, &ci);
        }
        SDL_zero(ti);
        ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        ti.size = GS_W * GS_H * 4;
        tb = SDL_CreateGPUTransferBuffer(sDev, &ti);
        px = SDL_MapGPUTransferBuffer(sDev, tb, false);
        for (y = 0; y < GS_H; y++) {
            for (x = 0; x < GS_W; x++) {
                px[y * GS_W + x] = Gs_VramRead(fbp * 32, 8, 0, x, y) | 0xFF000000u;
            }
        }
        SDL_UnmapGPUTransferBuffer(sDev, tb);
        SDL_zero(src);
        src.transfer_buffer = tb;
        SDL_zero(dst);
        dst.texture = sFbTex[i];
        dst.w = GS_W;
        dst.h = GS_H;
        dst.d = 1;
        SDL_UploadToGPUTexture(copy, &src, &dst, false);
        SDL_ReleaseGPUTransferBuffer(sDev, tb);
    }
    SDL_EndGPUCopyPass(copy);
    gGsFbUploads = 0;
    sEndT[1] = gpu_now();
    /* replay the frame */
    lastPipe = lastUni = lastSampler = -1;
    if (aux_plan_on()) {
        aux_plan();
    }
    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        SDL_GPUBufferBinding vb;
        SDL_GPUTextureSamplerBinding ts;
        SDL_FColor bc;
        struct { int32_t mode[4]; float misc[4]; float rect[4]; float orig[4]; float extra[4]; } fu;
        int shadowStart;

        if (d->native >= 100) { /* a picture uploaded into this display buffer (GsGpu_FbUpload): scaled into its texture */
            SDL_GPUBlitInfo bl;
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
                pass = NULL;
            }
            cur = -1;
            SDL_zero(bl);
            bl.source.texture = sFbTex[d->native - 100];
            bl.source.w = GS_W;
            bl.source.h = GS_H;
            bl.destination.texture = sTgCol[d->target];
            bl.destination.w = GS_W * SCALE;
            bl.destination.h = GS_H * SCALE;
            bl.load_op = SDL_GPU_LOADOP_DONT_CARE;
            bl.filter = SDL_GPU_FILTER_LINEAR;
            if (sFbTex[d->native - 100] != NULL) {
                SDL_BlitGPUTexture(cmd, &bl);
                gsTargets[d->target].cleared = 1;
            }
            continue;
        }
        if (d->native == 5) { /* the alpha bytes as they are now, for the draws that test them (TEST.DATE) */
            SDL_GPUCopyPass *cp;
            SDL_GPUTextureLocation from, to;
            int rx, ry, rw, rh;
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
                pass = NULL;
            }
            cur = -1;
            if (!gsTargets[d->target].cleared) {
                continue;
            }
            if (!GsDraw_DateRegion(n, &rx, &ry, &rw, &rh)) { /* only what the draws that follow read (gs_draw.c) */
                continue;
            }
            cp = SDL_BeginGPUCopyPass(cmd);
            SDL_zero(from);
            SDL_zero(to);
            from.texture = sTgAux[d->target];
            from.x = to.x = (Uint32)rx;
            from.y = to.y = (Uint32)ry;
            to.texture = sDateCopy;
            SDL_CopyGPUTextureToTexture(cp, &from, &to, (Uint32)rw, (Uint32)rh, 1, false);
            SDL_EndGPUCopyPass(cp);
            continue;
        }
        if (d->native == 4) { /* the ids as they are now (GfxPost_CopyAlphaToDepth): kept for the passes that index them */
            SDL_GPUCopyPass *cp;
            SDL_GPUTextureLocation from, to;
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
                pass = NULL;
            }
            cur = -1;
            if (!gsTargets[d->target].cleared) {
                continue;
            }
            cp = SDL_BeginGPUCopyPass(cmd);
            SDL_zero(from);
            SDL_zero(to);
            from.texture = sTgAux[d->target];
            to.texture = sAuxCopy;
            {
                int rx, ry, rw, rh;
                if (aux_region(n, &rx, &ry, &rw, &rh)) { /* only what the passes reading it read */
                    from.x = to.x = (Uint32)rx;
                    from.y = to.y = (Uint32)ry;
                    SDL_CopyGPUTextureToTexture(cp, &from, &to, (Uint32)rw, (Uint32)rh, 1, false);
                } else {
                    SDL_CopyGPUTextureToTexture(cp, &from, &to, GS_W * SCALE, GS_H * SCALE, 1, false);
                }
            }
            SDL_EndGPUCopyPass(cp);
            continue;
        }
        if (d->native == 3) { /* a table pass: both attachments, no depth; reads the depth texture or the alpha copy */
            SDL_GPUColorTargetInfo cts[2];
            SDL_GPUTextureSamplerBinding fs[3];
            SDL_FColor bc;
            SDL_Rect sc = { d->scissor[0], d->scissor[1], d->scissor[2], d->scissor[3] };
            float params[4] = {(float)d->src, d->misc[0], 0.0f, 0.0f};
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
                pass = NULL;
            }
            cur = -1;
            if (!gsTargets[d->target].cleared) {
                continue;
            }
            SDL_zero(cts);
            cts[0].texture = sTgCol[d->target];
            cts[0].load_op = SDL_GPU_LOADOP_LOAD;
            cts[0].store_op = SDL_GPU_STOREOP_STORE;
            cts[1] = cts[0];
            cts[1].texture = sTgAux[d->target];
            pass = SDL_BeginGPURenderPass(cmd, cts, 2, NULL);
            SDL_BindGPUGraphicsPipeline(pass, sPipes[d->pipeline].p);
            bc.r = bc.g = bc.b = bc.a = d->blendc;
            SDL_SetGPUBlendConstants(pass, bc);
            fs[0].texture = sTgDep[d->target];
            fs[0].sampler = sSamplers[6];
            fs[1].texture = sAuxCopy;
            fs[1].sampler = sSamplers[6];
            fs[2].texture = (SDL_GPUTexture *)(uintptr_t)d->tex;
            fs[2].sampler = sSamplers[6];
            SDL_BindGPUFragmentSamplers(pass, 0, fs, 3);
            SDL_PushGPUFragmentUniformData(cmd, 0, params, sizeof(params));
            SDL_SetGPUScissor(pass, &sc);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
            pass = NULL;
            continue;
        }
        if (d->native == 6) { /* a script mod's look: the picture copied, then drawn back over itself, changed */
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
                pass = NULL;
            }
            cur = -1;
            if (sGradePipe != NULL && gsTargets[d->target].cleared) {
                Uint32 w = GS_W * SCALE, h = GS_H * SCALE;
                if (sGradeCopy == NULL || sGradeW != w || sGradeH != h) {
                    SDL_GPUTextureCreateInfo tci;
                    Uint32 m = w > h ? w : h;
                    if (sGradeCopy != NULL) { SDL_ReleaseGPUTexture(sDev, sGradeCopy); }
                    for (sGradeLevels = 1; (m >> sGradeLevels) > 0; sGradeLevels++) {
                    }
                    SDL_zero(tci);
                    tci.type = SDL_GPU_TEXTURETYPE_2D;
                    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
                    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
                    tci.width = w;
                    tci.height = h;
                    tci.layer_count_or_depth = 1;
                    tci.num_levels = sGradeLevels; /* (the blur of the energy light and the scene's average) */
                    sGradeCopy = SDL_CreateGPUTexture(sDev, &tci);
                    sGradeW = w;
                    sGradeH = h;
                }
                if (sGradeCopy != NULL) {
                    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
                    SDL_GPUTextureLocation from, to;
                    SDL_GPUColorTargetInfo ft;
                    SDL_GPUTextureSamplerBinding fs[3];
                    float params[16 + GS_LIGHTS * 8];
                    SDL_zero(from);
                    SDL_zero(to);
                    from.texture = sTgCol[d->target];
                    to.texture = sGradeCopy;
                    SDL_CopyGPUTextureToTexture(cp, &from, &to, w, h, 1, false);
                    SDL_EndGPUCopyPass(cp);
                    SDL_GenerateMipmapsForGPUTexture(cmd, sGradeCopy);
                    {   /* the scene's average into the slow one (the first time: taken as it is) */
                        SDL_GPUColorTargetInfo bt;
                        SDL_GPUTextureSamplerBinding bs;
                        SDL_FColor k;
                        /* a level where the picture (512 x 448 GS pixels of the 1024 x 1024 buffer) is ~8 texels across */
                        float lod[4] = {6.0f, 512.0f / (float)GS_W, 448.0f / (float)GS_H, 0.0f};
                        for (Uint32 sc3 = SCALE; sc3 > 1; sc3 >>= 1) { lod[0] += 1.0f; }
                        SDL_zero(bt);
                        bt.texture = sBalance;
                        bt.load_op = sBalanceFresh ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
                        bt.store_op = SDL_GPU_STOREOP_STORE;
                        pass = SDL_BeginGPURenderPass(cmd, &bt, 1, NULL);
                        SDL_BindGPUGraphicsPipeline(pass, sAvgPipe);
                        k.r = k.g = k.b = k.a = sBalanceFresh ? 1.0f : 0.06f;
                        SDL_SetGPUBlendConstants(pass, k);
                        bs.texture = sGradeCopy;
                        bs.sampler = sGradeSampler;
                        SDL_BindGPUFragmentSamplers(pass, 0, &bs, 1);
                        SDL_PushGPUFragmentUniformData(cmd, 0, lod, sizeof(lod));
                        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
                        SDL_EndGPURenderPass(pass);
                        pass = NULL;
                        sBalanceFresh = 0;
                    }
                    SDL_zero(ft);
                    ft.texture = sTgCol[d->target];
                    ft.load_op = SDL_GPU_LOADOP_LOAD;
                    ft.store_op = SDL_GPU_STOREOP_STORE;
                    pass = SDL_BeginGPURenderPass(cmd, &ft, 1, NULL);
                    SDL_BindGPUGraphicsPipeline(pass, sGradePipe);
                    fs[0].texture = sGradeCopy;
                    fs[0].sampler = sGradeSampler; /* (level 0 at its own size; the blurs from the mipmaps) */
                    fs[1].texture = sTgDep[d->target]; /* (the ambient shadows read the depth) */
                    fs[1].sampler = sSamplers[6];
                    fs[2].texture = sBalance;
                    fs[2].sampler = sSamplers[6];
                    SDL_BindGPUFragmentSamplers(pass, 0, fs, 3);
                    memcpy(params, d->misc, sizeof(float) * 4);
                    memcpy(params + 4, d->rect, sizeof(float) * 4);
                    memcpy(params + 8, d->orig, sizeof(float) * 4);
                    params[12] = d->blendc;                  /* (the depth buffer's maximum / 65536) */
                    params[13] = (float)d->mode[0] / 100.0f; /* balance */
                    params[14] = (float)d->mode[1] / 100.0f; /* energy light */
                    {   /* the frame's energy lights (gs_draw.c light_add) */
                        int set = d->src >= 0 && d->src < GS_LIGHT_SETS ? d->src : 0, n = gsGradeLightCount[set];
                        params[15] = (float)n;
                        for (int li = 0; li < GS_LIGHTS; li++) {
                            const GsLight *l = &gsGradeLights[set][li];
                            float *o = params + 16 + li * 8;
                            if (li >= n) { memset(o, 0, sizeof(float) * 8); continue; }
                            o[0] = l->x; o[1] = l->y; o[2] = l->rad; o[3] = l->z;
                            o[4] = l->r; o[5] = l->g; o[6] = l->b; o[7] = l->w;
                        }
                    }
                    SDL_PushGPUFragmentUniformData(cmd, 0, params, sizeof(params));
                    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
                    SDL_EndGPURenderPass(pass);
                    pass = NULL;
                }
            }
            continue;
        }
        if (d->native) { /* a native full-screen effect: its own pass on the colour texture alone, reading the alpha copy */
            SDL_GPUColorTargetInfo ft;
            SDL_GPUTextureSamplerBinding fs;
            SDL_Rect sc;
            float params[4];
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
                pass = NULL;
            }
            cur = -1;
            if (!gsTargets[d->target].cleared) {
                continue;
            }
            SDL_zero(ft);
            ft.texture = sTgCol[d->target];
            ft.load_op = SDL_GPU_LOADOP_LOAD;
            ft.store_op = SDL_GPU_STOREOP_STORE;
            pass = SDL_BeginGPURenderPass(cmd, &ft, 1, NULL);
            SDL_BindGPUGraphicsPipeline(pass, d->native == 2 ? sKeyPipe : sOutlinePipe);
            fs.texture = sAuxCopy; /* the ids as copied by the game, not the live alpha (later passes overwrite it) */
            fs.sampler = sSamplers[6]; /* nearest, clamped */
            SDL_BindGPUFragmentSamplers(pass, 0, &fs, 1);
            extern int Port_PageAspectMilli(void); /* plat_stub.c: the shape of the page shown now */
            params[0] = 1333.333f / (float)Port_PageAspectMilli() / (float)GS_W; /* one PS2 pixel across, as wide as at 4:3 (outline.frag) */
            params[1] = 1.0f / (float)GS_H;
            params[2] = 100.0f / 255.0f; /* the dark rectangle's 0x64 */
            params[3] = getenv("BT3_FX_DEBUG") != NULL ? 1.0f : 0.0f;
            SDL_PushGPUFragmentUniformData(cmd, 0, params, sizeof(params));
            sc = (SDL_Rect){ d->scissor[0], d->scissor[1], d->scissor[2], d->scissor[3] };
            SDL_SetGPUScissor(pass, &sc);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
            pass = NULL;
            continue;
        }
        {
            float clearDepth;
            if (clear_draw(d, &clearDepth)) { /* the frame's clear: the render pass clears instead (clear_draw) */
                SDL_GPUColorTargetInfo cts[2];
                SDL_GPUDepthStencilTargetInfo dt;
                if (pass != NULL) {
                    SDL_EndGPURenderPass(pass);
                }
                SDL_zero(cts);
                SDL_zero(dt);
                cts[0].texture = sTgCol[d->target];
                cts[0].load_op = SDL_GPU_LOADOP_CLEAR;
                cts[0].store_op = SDL_GPU_STOREOP_STORE;
                cts[1] = cts[0];
                cts[1].texture = sTgAux[d->target];
                dt.texture = sTgDep[d->target];
                dt.load_op = SDL_GPU_LOADOP_CLEAR;
                dt.store_op = SDL_GPU_STOREOP_STORE;
                dt.clear_depth = clearDepth;
                dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
                dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
                gsTargets[d->target].cleared = 1;
                pass = SDL_BeginGPURenderPass(cmd, cts, 2, &dt);
                if (sShadowStencil) { SDL_SetGPUStencilReference(pass, 0); }
                cur = d->target;
                bound = -1;
                lastPipe = lastUni = lastSampler = -1;
                lastTex = NULL;
                haveFu = haveScissor = 0;
                lastBlend = -1.0f;
                continue;
            }
        }
        shadowStart = (sPipes[d->pipeline].key & (1u << 24)) != 0 &&
            (n == 0 || gsDraws[n - 1].native || gsDraws[n - 1].target != d->target ||
             (sPipes[gsDraws[n - 1].pipeline].key & (1u << 24)) == 0);
        if (shadowStart && pass != NULL) {
            SDL_EndGPURenderPass(pass);
            pass = NULL;
            cur = -1;
        }
        if (d->target != cur) {
            SDL_GPUColorTargetInfo ct, cts[2];
            SDL_GPUDepthStencilTargetInfo dt;
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
            }
            SDL_zero(ct);
            SDL_zero(dt);
            ct.texture = sTgCol[d->target];
            ct.load_op = gsTargets[d->target].cleared ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
            ct.store_op = SDL_GPU_STOREOP_STORE;
            cts[1] = ct;
            cts[1].texture = sTgAux[d->target];
            if (getenv("BT3_GPU_MAGENTA") != NULL) { ct.clear_color.r = 1.0f; ct.clear_color.b = 1.0f; ct.clear_color.a = 1.0f; }
            dt.texture = sTgDep[d->target];
            dt.load_op = gsTargets[d->target].cleared ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
            dt.store_op = SDL_GPU_STOREOP_STORE;
            /* Shadow batches stay in this pass. The mask has no use after it, so avoid
               loading or storing stencil on all the other render passes. */
            dt.stencil_load_op = shadowStart ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_DONT_CARE;
            dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
            gsTargets[d->target].cleared = 1;
            cts[0] = ct;
            pass = SDL_BeginGPURenderPass(cmd, cts, 2, &dt);
            if (sShadowStencil) { SDL_SetGPUStencilReference(pass, 0); }
            cur = d->target;
            bound = -1;
            lastPipe = lastUni = lastSampler = -1; /* a new pass: everything is set again */
            lastTex = NULL;
            haveFu = haveScissor = 0;
            lastBlend = -1.0f;
        }
        if (bound != (d->vu != 0)) {
            vb.buffer = d->vu ? sVuVbuf : sVbuf;
            vb.offset = 0;
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
            if (d->vu) {
                SDL_GPUBufferBinding ib;
                ib.buffer = sVuIbuf;
                ib.offset = 0;
                SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            }
            bound = d->vu != 0;
        }
        /* Only what changed since the previous draw is set again: most of a frame's 2,000 draws differ from their
           neighbour in one thing (the matrix, or the texture), and each call below costs time in the graphics
           driver (a texture binding makes the library write a new descriptor set). */
        if (d->vu && (int)d->uniform != lastUni) {
            SDL_PushGPUVertexUniformData(cmd, 0, &gsVuUni[d->uniform], sizeof(Vu0Uniform));
            lastUni = (int)d->uniform;
        }
        /* the alpha bytes: kept where they are read later, left alone elsewhere (aux_plan) */
        pipe = (int)d->pipeline;
        split = 0;
        if (aux_plan_on() && (sPipes[pipe].key >> 16 & SDL_GPU_COLORCOMPONENT_A)) {
            const int16_t *r = sAuxNeed[n];
            if (r[2] <= r[0]) {
                pipe = pipe_no_aux(pipe);
            } else if (!d->vu && (r[0] > d->scissor[0] || r[1] > d->scissor[1] || r[2] < d->scissor[0] + d->scissor[2] ||
                                  r[3] < d->scissor[1] + d->scissor[3]) && pipe_no_aux(pipe) != pipe) {
                split = 1; /* (a vertex program's draw is not split: its vertices would be worked out again) */
            }
        }
        if (pipe != lastPipe) {
            SDL_BindGPUGraphicsPipeline(pass, sPipes[pipe].p);
            lastPipe = pipe;
        }
        if ((SDL_GPUTexture *)(uintptr_t)d->tex != lastTex || (int)d->sampler != lastSampler) {
            SDL_GPUTextureSamplerBinding two[2];
            lastTex = (SDL_GPUTexture *)(uintptr_t)d->tex;
            lastSampler = (int)d->sampler;
            two[0].texture = lastTex;
            two[0].sampler = sSamplers[d->sampler];
            two[1].texture = sDateCopy;
            two[1].sampler = sSamplers[6];
            SDL_BindGPUFragmentSamplers(pass, 0, two, 2);
        }
        memset(&fu, 0, sizeof(fu));
        memcpy(fu.mode, d->mode, sizeof(fu.mode));
        memcpy(fu.misc, d->misc, sizeof(fu.misc));
        memcpy(fu.rect, d->rect, sizeof(fu.rect));
        memcpy(fu.orig, d->orig, sizeof(fu.orig));
        fu.extra[0] = d->vu ? (gsVuUni[d->uniform].misc[2] > 0.0f ? gsVuUni[d->uniform].misc[2] : 0.0f) : gsZMax;
        fu.extra[1] = (sPipes[d->pipeline].key & (1u << 9)) ? 1.0f : 0.0f; /* the source scaled by its alpha */
        if (!haveFu || memcmp(&fu, &lastFu, sizeof(fu)) != 0) {
            SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));
            memcpy(&lastFu, &fu, sizeof(fu));
            haveFu = 1;
        }
        if (d->blendc != lastBlend) {
            bc.r = bc.g = bc.b = bc.a = d->blendc;
            SDL_SetGPUBlendConstants(pass, bc);
            lastBlend = d->blendc;
        }
        if (!haveScissor || memcmp(d->scissor, lastScissor, sizeof(d->scissor)) != 0) {
            SDL_Rect r = { d->scissor[0], d->scissor[1], d->scissor[2], d->scissor[3] };
            SDL_SetGPUScissor(pass, &r);
            memcpy(lastScissor, d->scissor, sizeof(lastScissor));
            haveScissor = 1;
        }
        if (fx_measure(d)) {
            continue;
        }
        if (d->vu) {
            SDL_DrawGPUIndexedPrimitives(pass, d->count, 1, d->first, 0, 0);
        } else if (split) {
            /* inside the part read later with the byte, the rest of its scissor without: four bands around it */
            const int16_t *r = sAuxNeed[n];
            int sx0 = d->scissor[0], sy0 = d->scissor[1], sx1 = sx0 + d->scissor[2], sy1 = sy0 + d->scissor[3], b;
            SDL_Rect in = { r[0], r[1], r[2] - r[0], r[3] - r[1] }, out[4];
            out[0] = (SDL_Rect){ sx0, sy0, sx1 - sx0, r[1] - sy0 };
            out[1] = (SDL_Rect){ sx0, r[3], sx1 - sx0, sy1 - r[3] };
            out[2] = (SDL_Rect){ sx0, r[1], r[0] - sx0, r[3] - r[1] };
            out[3] = (SDL_Rect){ r[2], r[1], sx1 - r[2], r[3] - r[1] };
            SDL_SetGPUScissor(pass, &in);
            SDL_DrawGPUPrimitives(pass, d->count, 1, d->first, 0);
            lastPipe = pipe_no_aux(pipe);
            SDL_BindGPUGraphicsPipeline(pass, sPipes[lastPipe].p);
            for (b = 0; b < 4; b++) {
                if (out[b].w > 0 && out[b].h > 0) {
                    SDL_SetGPUScissor(pass, &out[b]);
                    SDL_DrawGPUPrimitives(pass, d->count, 1, d->first, 0);
                }
            }
            haveScissor = 0;
        } else {
            SDL_DrawGPUPrimitives(pass, d->count, 1, d->first, 0);
        }
    }
    if (pass != NULL) {
        SDL_EndGPURenderPass(pass);
    }
    /* Show the frame's own buffer: the one sceGsSwapDBuff set up for this frame. (Guessing "the buffer with the most
       draws" showed the shadow buffer, a grey fighter on black, on frames where it happened to receive more.) */
    best = gGsMainFbp >= 0 ? GsDraw_TargetGet((uint32_t)gGsMainFbp, 0) : -1;
    for (i = 0; best < 0 && i < gsTargetCount; i++) {
        best = i;
    }
    sEndT[2] = gpu_now();
    acquired = SDL_WaitAndAcquireGPUSwapchainTexture(cmd, sWindow, &swap, &sw, &sh);
    sEndT[3] = gpu_now();
    if (acquired && swap != NULL && best >= 0 && gsTargets[best].cleared) {
        SDL_GPUBlitInfo bl;
        SDL_zero(bl);
        bl.source.texture = sTgCol[best];
        bl.source.w = 512 * SCALE;
        bl.source.h = 448 * SCALE;
        bl.destination.texture = swap;
        {
            /* The picture keeps its shape whatever the window's: 4:3, or 16:9 in widescreen, centred, the rest
               black. (The 512 x 448 buffer is not square-pixelled: it always fills a 4:3 or 16:9 screen.) */
            extern int Port_PageAspectMilli(void); /* plat_stub.c: 4:3 while the menus run, if asked for */
            float want = (float)Port_PageAspectMilli() / 1000.0f;
            Uint32 w = sw, h = sh;
            if ((float)sw > (float)sh * want) {
                w = (Uint32)((float)sh * want + 0.5f);
            } else {
                h = (Uint32)((float)sw / want + 0.5f);
            }
            bl.destination.x = (sw - w) / 2;
            bl.destination.y = (sh - h) / 2;
            bl.destination.w = w;
            bl.destination.h = h;
            bl.clear_color.a = 1.0f;
        }
        bl.load_op = SDL_GPU_LOADOP_CLEAR;
        bl.filter = SDL_GPU_FILTER_LINEAR;
        SDL_BlitGPUTexture(cmd, &bl);
        gUiPresentX = (int)bl.destination.x; /* the stage-name overlay maps game pixels through this rectangle */
        gUiPresentY = (int)bl.destination.y;
        gUiPresentW = (int)bl.destination.w;
        gUiPresentH = (int)bl.destination.h;
        Ui_Draw(cmd, swap);
        {   /* BT3_UI_SHOT=<frame>:<file.ppm>: the window's picture with the settings window on it, for checking
                the settings window without a person or a screen capture (drawn a second time into a texture) */
            static int frame = -1;
            static const char *file;
            if (frame < 0) {
                frame = 0;
                if (getenv("BT3_UI_SHOT") != NULL && (file = strchr(getenv("BT3_UI_SHOT"), ':')) != NULL) {
                    frame = atoi(getenv("BT3_UI_SHOT"));
                    file++;
                }
            }
            if (frame > 0 && gGsFrame >= (unsigned)frame) {
                SDL_GPUTextureCreateInfo ci;
                SDL_GPUTransferBufferCreateInfo ti;
                SDL_GPUTransferBuffer *tb;
                SDL_GPUTextureRegion src;
                SDL_GPUTextureTransferInfo dst;
                SDL_GPUCommandBuffer *c2;
                SDL_GPUCopyPass *cp;
                SDL_GPUFence *fence;
                SDL_GPUTexture *tex;
                SDL_GPUTextureFormat fmt = SDL_GetGPUSwapchainTextureFormat(sDev, sWindow);
                int bgr = fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
                uint8_t *px;
                FILE *fp;
                Uint32 x, y;

                frame = 0;
                SDL_zero(ci);
                ci.type = SDL_GPU_TEXTURETYPE_2D;
                ci.format = fmt;
                ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
                ci.width = sw;
                ci.height = sh;
                ci.layer_count_or_depth = 1;
                ci.num_levels = 1;
                tex = SDL_CreateGPUTexture(sDev, &ci);
                c2 = SDL_AcquireGPUCommandBuffer(sDev);
                bl.destination.texture = tex;
                SDL_BlitGPUTexture(c2, &bl);
                Ui_DrawAgain(c2, tex);
                SDL_zero(ti);
                ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
                ti.size = sw * sh * 4;
                tb = SDL_CreateGPUTransferBuffer(sDev, &ti);
                SDL_zero(src);
                src.texture = tex;
                src.w = sw;
                src.h = sh;
                src.d = 1;
                SDL_zero(dst);
                dst.transfer_buffer = tb;
                cp = SDL_BeginGPUCopyPass(c2);
                SDL_DownloadFromGPUTexture(cp, &src, &dst);
                SDL_EndGPUCopyPass(cp);
                fence = SDL_SubmitGPUCommandBufferAndAcquireFence(c2);
                SDL_WaitForGPUFences(sDev, true, &fence, 1);
                SDL_ReleaseGPUFence(sDev, fence);
                px = SDL_MapGPUTransferBuffer(sDev, tb, false);
                fp = fopen(file, "wb");
                if (fp != NULL && px != NULL) {
                    fprintf(fp, "P6\n%u %u\n255\n", sw, sh);
                    for (y = 0; y < sh; y++) {
                        for (x = 0; x < sw; x++) {
                            const uint8_t *q = &px[(y * sw + x) * 4];
                            uint8_t rgb[3] = {q[bgr ? 2 : 0], q[1], q[bgr ? 0 : 2]};
                            fwrite(rgb, 1, 3, fp);
                        }
                    }
                    fclose(fp);
                }
                SDL_UnmapGPUTransferBuffer(sDev, tb);
                SDL_ReleaseGPUTransferBuffer(sDev, tb);
                SDL_ReleaseGPUTexture(sDev, tex);
            }
        }
    }
    sEndT[4] = gpu_now();
    if (getenv("BT3_LAG_LOG") != NULL) { /* (and when the graphics chip has finished it: lag_fence) */
        SDL_GPUFence *f = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        extern void Port_LagMark(int what);
        Port_LagMark('S');
        lag_fence(f);
    } else {
        SDL_SubmitGPUCommandBuffer(cmd); { extern void Port_LagMark(int what); Port_LagMark('S'); }
    }
    {   /* BT3_DRAW_LOG=<ms>: each picture whose drawing took longer, from the start of its list to the driver, in parts:
           to find what makes a drawing slow now and then (a laptop: 1 in 10 took 10 to 20 ms instead of 2) */
        static int limit = -1;
        extern uint64_t gDrawStartNs, gDrawTexNs, gDrawPipeNs, gDrawUpBytes, gDrawImgNs, gDrawLookNs, gDrawVuNs, gDrawXferNs;
        extern unsigned gDrawNewTex, gDrawImgMade, gDrawImgFreed, gDrawImgPooled, gGsFrame;
        extern int gDrawLogged;
        if (limit < 0) {
            limit = getenv("BT3_DRAW_LOG") != NULL ? atoi(getenv("BT3_DRAW_LOG")) : 0;
        }
        if (limit > 0 && !gDrawLogged && gDrawStartNs != 0) {
            uint64_t now = gpu_now(), total = now - gDrawStartNs;
            gDrawLogged = 1;
            if (total >= (uint64_t)limit * 1000000ull) {
                uint64_t walk = sEndT[0] > gDrawStartNs ? sEndT[0] - gDrawStartNs : 0;
                fprintf(stderr, "draw: frame %u: %.1f ms: the list %.1f (of it vertex programs %.1f, pictures into GS memory %.1f, texture lookups %.1f, %u new textures decoded %.1f, pipelines made %.1f), "
                                "uploads %.1f, issuing the draws %.1f, waiting for the screen's buffer %.1f, showing %.1f, handing over %.1f; "
                                "images made %u (%.1f ms) and %u spares taken, released %u, uploaded %u KB\n",
                        gGsFrame, (double)total / 1e6, (double)walk / 1e6, (double)gDrawVuNs / 1e6, (double)gDrawXferNs / 1e6, (double)gDrawLookNs / 1e6, gDrawNewTex, (double)gDrawTexNs / 1e6, (double)gDrawPipeNs / 1e6,
                        (double)(sEndT[1] - sEndT[0]) / 1e6, (double)(sEndT[2] - sEndT[1]) / 1e6, (double)(sEndT[3] - sEndT[2]) / 1e6,
                        (double)(sEndT[4] - sEndT[3]) / 1e6, (double)(now - sEndT[4]) / 1e6, gDrawImgMade, (double)gDrawImgNs / 1e6, gDrawImgPooled, gDrawImgFreed,
                        (unsigned)(gDrawUpBytes / 1024));
            }
        }
    }
    {   /* spare images for coming bursts, when this picture left time (the drawing from its list's start) */
        extern uint64_t gDrawStartNs;
        pool_refill(gDrawStartNs != 0 ? gpu_now() - gDrawStartNs : ~0ull);
        targets_refill(gDrawStartNs != 0 ? gpu_now() - gDrawStartNs : ~0ull); /* (and spare render targets) */
    }
    {   /* BT3_GS_VERBOSE: once a second, where the end of the frame spends its time */
        static uint64_t sum[5];
        static int count;
        uint64_t now = gpu_now();
        sum[0] += sEndT[1] - sEndT[0];
        sum[1] += sEndT[2] - sEndT[1];
        sum[2] += sEndT[3] - sEndT[2];
        sum[3] += sEndT[4] - sEndT[3];
        sum[4] += now - sEndT[4];
        if (++count == 60) {
            if (getenv("BT3_GS_VERBOSE") != NULL) {
                fprintf(stderr, "time:   of ending the frame: uploads %.2f ms, issuing the draws %.2f, waiting for the screen's buffer %.2f, "
                                "showing %.2f, handing over to the driver %.2f\n",
                        (double)sum[0] / 60e6, (double)sum[1] / 60e6, (double)sum[2] / 60e6, (double)sum[3] / 60e6, (double)sum[4] / 60e6);
            }
            count = 0;
            memset(sum, 0, sizeof(sum));
        }
    }
    /* BT3_SHOT=<n>: every n frames, read the shown buffer back and write port/build/shots/gpu_NNNNN.ppm */
    {
        static int every = -1;
        if (every < 0) {
            every = getenv("BT3_SHOT") != NULL ? atoi(getenv("BT3_SHOT")) : 0;
        }
        {   /* BT3_SHOT_VBLANK=<n>: one screenshot at the first frame shown at or after that vertical blank (the
                clock of the headless traces, so a console save state's tick can be matched) */
            extern unsigned gPortVBlanks;
            static int done;
            if (!done && getenv("BT3_SHOT_VBLANK") != NULL && gPortVBlanks >= (unsigned)atoi(getenv("BT3_SHOT_VBLANK"))) {
                done = 1;
                every = 1;
                fprintf(stderr, "bt3: screenshot at vertical blank %u = frame %u\n", gPortVBlanks, gGsFrame);
            } else if (getenv("BT3_SHOT_VBLANK") != NULL) {
                every = 0;
            }
        }
        /* BT3_SHOT_FROM / BT3_SHOT_TO limit the frames */
        if (every > 0 && gGsFrame % (unsigned)every == 0 && best >= 0 && gsTargets[best].cleared &&
            (getenv("BT3_SHOT_FROM") == NULL || (int)gGsFrame >= atoi(getenv("BT3_SHOT_FROM"))) &&
            (getenv("BT3_SHOT_TO") == NULL || (int)gGsFrame <= atoi(getenv("BT3_SHOT_TO")))) {
            SDL_GPUTransferBufferCreateInfo ti;
            SDL_GPUTransferBuffer *tb;
            SDL_GPUTextureRegion src;
            SDL_GPUTextureTransferInfo dst;
            SDL_GPUCommandBuffer *c2 = SDL_AcquireGPUCommandBuffer(sDev);
            SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(c2);
            SDL_GPUFence *fence;
            Uint32 w = 512 * SCALE, h = 448 * SCALE, x, y;
            uint8_t *px;
            char name[600];
            FILE *fp;

            SDL_zero(ti);
            ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
            ti.size = w * h * 4;
            tb = SDL_CreateGPUTransferBuffer(sDev, &ti);
            SDL_zero(src);
            src.texture = getenv("BT3_SHOT_AUX") != NULL ? sTgAux[best] : sTgCol[best];
            src.w = w;
            src.h = h;
            src.d = 1;
            SDL_zero(dst);
            dst.transfer_buffer = tb;
            SDL_DownloadFromGPUTexture(cp, &src, &dst);
            SDL_EndGPUCopyPass(cp);
            fence = SDL_SubmitGPUCommandBufferAndAcquireFence(c2);
            SDL_WaitForGPUFences(sDev, true, &fence, 1);
            SDL_ReleaseGPUFence(sDev, fence);
            px = SDL_MapGPUTransferBuffer(sDev, tb, false);
            if (getenv("BT3_SHOT_DIR") != NULL) {
                /* BT3_SHOT_DIR=<folder>: there instead, the vertical blank in the name too (several copies at once;
                   two copies' pictures of the same blank compared) */
                extern unsigned gPortVBlanks;
                snprintf(name, sizeof(name), "%s/gpu_%05u_v%06u.ppm", getenv("BT3_SHOT_DIR"), gGsFrame, gPortVBlanks);
            } else {
                snprintf(name, sizeof(name), "port/build/shots/gpu_%05u.ppm", gGsFrame);
            }
            fp = fopen(name, "wb");
            if (fp != NULL && px != NULL) {
                fprintf(fp, "P6\n%u %u\n255\n", w, h);
                for (y = 0; y < h; y++) {
                    for (x = 0; x < w; x++) {
                        if (getenv("BT3_SHOT_AUX") != NULL) { /* one byte per pixel: show it as grey, times 16 to see small ids */
                            uint8_t g = (uint8_t)(px[y * w + x] * 16), t[3] = {g, g, px[y * w + x]};
                            fwrite(t, 1, 3, fp);
                        } else {
                            fwrite(&px[(y * w + x) * 4], 1, 3, fp);
                        }
                    }
                }
                fclose(fp);
            }
            SDL_UnmapGPUTransferBuffer(sDev, tb);
            SDL_ReleaseGPUTransferBuffer(sDev, tb);
        }
    }
    for (i = 0; i < gsTargetCount; i++) {
        gsTargets[i].draws = 0;
    }
    gsVertCount = 0;
    gsVuVertCount = 0;
    gsVuIdxCount = 0;
    gsVuUniCount = 0;
    gsDrawCount = 0;
    gsAnchor = 0;
    gsSkipped = 0;
    gsNative = 0;
}

/* Target i's picture, reduced to the GS's 512 x 448 (the nearest pixel of each: the cross-fade it is for is a
   half-second blend). A copy from the card, waited for: a few times in a fight's intro. */
static int vk_target_read(int i, uint8_t *rgba) {
    SDL_GPUTransferBufferCreateInfo ti;
    SDL_GPUTransferBuffer *tb;
    SDL_GPUTextureRegion src;
    SDL_GPUTextureTransferInfo dst;
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass *cp;
    SDL_GPUFence *fence;
    Uint32 w = 512 * SCALE, h = 448 * SCALE, x, y; /* (the picture's part of the target, as the screenshot's) */
    const uint8_t *px;

    if (sDev == NULL || sTgCol[i] == NULL) {
        return 0;
    }
    SDL_zero(ti);
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    ti.size = w * h * 4;
    tb = SDL_CreateGPUTransferBuffer(sDev, &ti);
    if (tb == NULL) {
        return 0;
    }
    cmd = SDL_AcquireGPUCommandBuffer(sDev);
    cp = SDL_BeginGPUCopyPass(cmd);
    SDL_zero(src);
    src.texture = sTgCol[i];
    src.w = w;
    src.h = h;
    src.d = 1;
    SDL_zero(dst);
    dst.transfer_buffer = tb;
    SDL_DownloadFromGPUTexture(cp, &src, &dst);
    SDL_EndGPUCopyPass(cp);
    fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd); { extern void Port_LagMark(int what); Port_LagMark('S'); }
    SDL_WaitForGPUFences(sDev, true, &fence, 1);
    SDL_ReleaseGPUFence(sDev, fence);
    px = SDL_MapGPUTransferBuffer(sDev, tb, false);
    if (px != NULL) {
        for (y = 0; y < 448; y++) {
            for (x = 0; x < 512; x++) {
                memcpy(rgba + (y * 512 + x) * 4, px + ((size_t)(y * SCALE + SCALE / 2) * w + x * SCALE + SCALE / 2) * 4, 4);
            }
        }
        SDL_UnmapGPUTransferBuffer(sDev, tb);
    }
    SDL_ReleaseGPUTransferBuffer(sDev, tb);
    return px != NULL;
}

static void vk_target_copy(int src, int dst) {
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUBlitInfo bl;
    if (sDev == NULL || sTgCol[src] == NULL || sTgCol[dst] == NULL) {
        return;
    }
    cmd = SDL_AcquireGPUCommandBuffer(sDev);
    SDL_zero(bl);
    bl.source.texture = sTgCol[src];
    bl.source.w = GS_W * SCALE;
    bl.source.h = GS_H * SCALE;
    bl.destination.texture = sTgCol[dst];
    bl.destination.w = GS_W * SCALE;
    bl.destination.h = GS_H * SCALE;
    bl.load_op = SDL_GPU_LOADOP_DONT_CARE;
    bl.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(cmd, &bl);
    SDL_SubmitGPUCommandBuffer(cmd); { extern void Port_LagMark(int what); Port_LagMark('S'); }
}

GsBackend sVulkanBackend = {
    "vulkan",
    vk_init,
    vk_frame_end,
    scale_changed,
    white_tex,
    tex_create,
    tex_upload,
    tex_destroy,
    tex_slots_left,
    target_ensure,
    target_drop,
    target_color,
    target_aux,
    target_depth,
    pipe_get,
    vk_target_read,
    vk_target_copy,
};
