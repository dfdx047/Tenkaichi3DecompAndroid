/*
 * Graphics Synthesizer, GPU back end (SDL3 GPU API: Vulkan on Linux and Android, Direct3D 12 / Metal elsewhere).
 *
 * gs_core.c still decodes what the game sends and keeps GS memory; this file takes over at "a primitive with the
 * current GS registers": it records every primitive of the frame, and at the end of the frame uploads vertices
 * and new textures and replays the list on the GPU.
 *   render targets   one per GS frame-buffer address in use, GS_W x GS_H GS pixels at SCALE times the PS2's
 *                    resolution, each with its own depth buffer
 *   textures         GS memory decoded to ordinary RGBA textures, cached by TEX0 / TEXA and the upload
 *                    generation of the pages they occupy; a frame buffer used as a texture is sampled directly
 *   blending         the GS equation ((A - B) * C >> 7) + D mapped onto blend factors
 *   alpha            1.0 = 0x80, so that "source alpha" blending needs no extra scaling
 * Not handled yet: a target sampled while it is being drawn to, region clamp / repeat, destination-alpha tests,
 * frame-buffer masks, 16-bit targets' precision.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gs_internal.h"
#include "shaders.h" /* generated: kGsVertSpv, kGsFragSpv */

/* The internal resolution multiplier: BT3_SCALE=1..8 (default 2). Every render target is GS_W x GS_H times this,
   about 9 MB of video memory each at 1x, 36 MB at 2x, 144 MB at 4x; a fight uses 7 to 16 of them. */
extern int Port_Setting(const char *name, int def); /* plat_settings.c: the saved settings */
extern void Port_SettingSave(const char *name, int value);
static int sScale = 2;
#define SCALE sScale
static int sFullscreen;
static float sWantAspect;
#define GS_W 1024
#define GS_H 1024
#define MAX_VERTS (1 << 20)
#define MAX_DRAWS (1 << 16)

typedef struct Vtx {
    float x, y, z;
    uint8_t r, g, b, a;
    float s, t, q;
} Vtx;

typedef struct Target {
    uint32_t fbp;
    SDL_GPUTexture *color, *depth;
    SDL_GPUTexture *aux; /* the GS alpha byte, exact (R8): the colour texture keeps alpha rescaled for blending */
    int cleared;
    unsigned draws; /* this frame */
    uint32_t gen;   /* upload generation of its first page when it was last drawn to */
    int stale;      /* a pass that should have filled it was dropped: its contents are not what the game expects */
    unsigned last;  /* frame it was last asked for */
} Target;

typedef struct Tex {
    uint64_t tex0, texa;
    uint32_t gen;
    SDL_GPUTexture *tex;
    unsigned last; /* frame last used */
    int replaced;  /* from a texture pack: several times the size, so always filtered (texture_get) */
    uint32_t ow, oh, rw, rh; /* then: the original's size and the replacement's */
    uint32_t amax;           /* and the largest alpha byte of the original */
} Tex;

typedef struct Draw {
    int vu;      /* 1: vertices of vertex program 0 (sVuVerts, uniform block `uniform`) */
    int uniform;
    int native; /* 0: primitives; otherwise a native full-screen effect (PORT_FX_*) at this point of the frame;
                   3: a table pass (dclut.frag) with `tex` = its table and `src` = which byte indexes it */
    int src;
    uint32_t first, count;
    int target;
    SDL_GPUTexture *tex;
    int tex_is_target;
    int sampler;
    int pipeline;
    int32_t mode[4];
    float misc[4];
    float rect[4];  /* a frame buffer as texture: the part of it the GS would address (u0, v0, u1, v1 in its uv);
                       a texture pack's replacement drawn as 2D art: the piece's own rectangle of the sheet */
    float orig[4];  /* that replacement: x, y = the original texture's size in texels */
    float blendc;
    SDL_Rect scissor;
} Draw;

typedef struct Pipe {
    uint32_t key;
    SDL_GPUGraphicsPipeline *p;
} Pipe;

static SDL_Window *sWindow;
static SDL_GPUDevice *sDev;
static SDL_GPUShader *sVs, *sFs;
static SDL_GPUBuffer *sVbuf;
static SDL_GPUTransferBuffer *sVxfer;
static SDL_GPUSampler *sSamplers[16]; /* bit 0 filtered, bit 1 / 2 clamped in u / v, bit 3 with the smaller copies (mip levels) */
static SDL_GPUTexture *sWhite;
static SDL_GPUGraphicsPipeline *sOutlinePipe, *sKeyPipe;
static SDL_GPUTexture *sFbTex[2]; /* pictures uploaded into the two display buffers (movies) */
static SDL_GPUShader *sFxVs, *sDclutFs;
static SDL_GPUTexture *sDateCopy; /* the alpha bytes as they were before a run of draws that test them (TEST.DATE) */
static SDL_GPUTexture *sAuxCopy;  /* the alpha bytes, copied so that a pass can read them while it writes them */
/* The top byte of the depth page as the game last left it: 0 = a copy of the frame's alpha (ids), 1 = the fog
   value made from the depth. Follows the two passes that write it (see draw_state). */
static int sDepthByteIsFog;
#define MAX_CLUTS 64
static struct { uint32_t hash, last; SDL_GPUTexture *tex; } sCluts[MAX_CLUTS];
/* Effects the user can switch off (BT3_FX_OFF=<mask>, or F1..F5 while running): 1 outline, 2 see-through tint,
   4 depth tint, 8 glare and object glow, 16 blur of distant things. */
static unsigned sFxOff;
/* Widescreen: which part of the screen the 2D pieces being drawn belong to (markers 0x10..0x13 from the HUD code:
   left panel, right panel, centre, end of the HUD). 0 = no group. */
static int sAnchor;
extern int Port_IsWide(void);   /* plat_stub.c */
extern int Port_AspectMilli(void);
extern int gPortMenuMode;       /* headless.c: the menus are running */
/* Strength of glare and glow in percent of what the game's passes give (BT3_GLOW=<percent>; F6 / F7 change it by
   10). Default 60: the user's choice on 2026-10-06 (100 looked too strong at the glare's peaks). */
#define GLOW_DEFAULT 60
static int sGlowPercent = GLOW_DEFAULT;
static int sTexPackOn = 1; /* the setting: use the texture pack (if one was found) */
#define MAX_TARGETS 24
static Target sTargets[MAX_TARGETS];
static int sTargetCount;
#define MAX_TEX 8192
#define TEX_BUCKETS 32768 /* a power of two, four times MAX_TEX */
static Tex sTex[MAX_TEX];
static uint16_t sTexBucket[TEX_BUCKETS]; /* index + 1 of a cache entry, 0 = empty (open addressing) */
static int sTexCount;
static Tex *sLast; /* the entry the previous lookup returned */
static Pipe sPipes[1024];
static int sPipeCount;
#define MAX_VU_VERTS (1 << 19)
#define MAX_VU_UNIFORMS 8192
typedef struct Vu0Uniform { /* std140 layout of the block in shaders/vu0.vert */
    float boneA[16], boneB[16], pivotA[4], pivotB[4], screen[16], light[4], color0[4], color1[4], misc[4];
    float light2[4]; /* program 1: the y components of 10..13 (the second texture coordinate of its layer 1) */
} Vu0Uniform;
static float *sVuVerts;         /* 12 floats per vertex, as the vertex program gets them */
static uint32_t sVuVertCount;
static Vu0Uniform *sVuUni;
static uint32_t sVuUniCount;
static SDL_GPUBuffer *sVuVbuf;
static SDL_GPUTransferBuffer *sVuXfer;
static SDL_GPUShader *sVu0Vs, *sVu4Vs, *sVu6Vs;
static Vtx *sVerts;
static uint32_t sVertCount;
static Draw *sDraws;
static uint32_t sDrawCount;
static unsigned sNative; /* native effect markers seen this frame */
unsigned gGpuNewTex, gGpuNewTexPixels, gGpuNewPipes; /* created since the front end last cleared them (slow-frame report) */
unsigned gGpuTexReplaced; /* textures taken from a texture pack so far */
uint64_t gGpuTexNs, gGpuPipeNs, gGpuEndNs; /* time spent decoding textures, creating pipelines, in GsGpu_FrameEnd */
static uint64_t gpu_now(void) { return SDL_GetTicksNS(); }
static unsigned sSkipped; /* primitives of PS2-only passes dropped this frame */

/* textures created this frame, to upload in the copy pass */
#define MAX_PENDING 4096
static struct { SDL_GPUTexture *tex; uint32_t w, h; uint32_t *px; uint32_t bytes, level; /* bytes 0: w * h * 4 */ } sPending[MAX_PENDING];
static int sPendingCount;

static SDL_GPUShader *shader(const unsigned char *code, size_t size, SDL_GPUShaderStage stage, int samplers, int ubos) {
    SDL_GPUShaderCreateInfo ci;
    SDL_zero(ci);
    ci.code = code;
    ci.code_size = size;
    ci.entrypoint = "main";
    ci.format = SDL_GPU_SHADERFORMAT_SPIRV;
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

/* ------------------------------------------------------------------------------------------- the settings window
   F1 opens it (ui.cpp, Dear ImGui): mouse-driven, drawn over the finished picture in the window, so nothing of it
   reaches the game's buffers or the screenshots. This side holds what it changes in the renderer. */
#include "ui.h"
#include "gs_texpack.h"
#include <math.h>
extern int gPortMusicPercent, gPortSePercent;
extern void Port_SetAspectMilli(int milli), Port_AudioRefresh(void), Port_SettingsWrite(void);
static int sPendingScale, sDisplaySetting;

static void window_shape(void) {
    float want = (float)Port_AspectMilli() / 1000.0f;
    int w, h;
    sWantAspect = want;
    if (!sFullscreen) {
        SDL_GetWindowSize(sWindow, &w, &h);
        SDL_SetWindowAspectRatio(sWindow, 0.0f, 0.0f);
        SDL_SetWindowSize(sWindow, (int)((float)h * want + 0.5f), h);
        SDL_SetWindowAspectRatio(sWindow, want, want);
    }
}

static void fullscreen_set(int on) {
    sFullscreen = on;
    if (on) { /* the full screen has the display's shape: the picture is centred in it */
        SDL_SetWindowAspectRatio(sWindow, 0.0f, 0.0f);
    }
    SDL_SetWindowFullscreen(sWindow, on);
    if (!on) {
        SDL_SetWindowAspectRatio(sWindow, sWantAspect, sWantAspect);
    }
}

void GsGpu_GetSettings(PortVideo *v) {
    v->scale = sPendingScale ? sPendingScale : sScale;
    v->aspectMilli = Port_AspectMilli();
    v->fullscreen = sFullscreen;
    v->fxOff = (int)sFxOff;
    v->glow = sGlowPercent;
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
    if (v->scale != now.scale) {
        sPendingScale = v->scale < 1 ? 1 : v->scale > 8 ? 8 : v->scale; /* applied between two frames */
    }
    if (v->aspectMilli != now.aspectMilli) {
        Port_SetAspectMilli(v->aspectMilli);
        window_shape();
    }
    if (v->fullscreen != now.fullscreen) {
        fullscreen_set(v->fullscreen);
    }
    sFxOff = (unsigned)v->fxOff & 31;
    sGlowPercent = v->glow;
    gPortSePercent = v->effects;
    if (v->music != now.music) {
        gPortMusicPercent = v->music;
        Port_AudioRefresh();
    }
    sDisplaySetting = v->display;
    sTexPackOn = v->texPack != 0;
    Port_SettingSave("texture_pack", sTexPackOn);
    Port_SettingSave("scale", sPendingScale ? sPendingScale : sScale);
    Port_SettingSave("aspect_milli", Port_AspectMilli());
    Port_SettingSave("fullscreen", sFullscreen);
    Port_SettingSave("fx_off", (int)sFxOff);
    Port_SettingSave("glow", sGlowPercent);
    Port_SettingSave("music", gPortMusicPercent);
    Port_SettingSave("effects", gPortSePercent);
    Port_SettingSave("display", sDisplaySetting);
    Port_SettingsWrite();
}

/* A new resolution multiplier: every render target is dropped and made again at the new size when the game next
   draws to it. Buffers that carry something over from the previous frame start empty for one frame. */
static void scale_apply(void) {
    int i;
    if (sPendingScale == 0 || sPendingScale == sScale) {
        sPendingScale = 0;
        return;
    }
    SDL_WaitForGPUIdle(sDev);
    for (i = 0; i < sTargetCount; i++) {
        SDL_ReleaseGPUTexture(sDev, sTargets[i].color);
        SDL_ReleaseGPUTexture(sDev, sTargets[i].aux);
        SDL_ReleaseGPUTexture(sDev, sTargets[i].depth);
    }
    sTargetCount = 0;
    SDL_ReleaseGPUTexture(sDev, sAuxCopy);
    SDL_ReleaseGPUTexture(sDev, sDateCopy);
    sScale = sPendingScale;
    sPendingScale = 0;
    copies_create();
    fprintf(stderr, "bt3: internal resolution %dx (%d x %d)\n", sScale, 512 * sScale, 448 * sScale);
}

int GsGpu_Init(void) {
    SDL_GPUBufferCreateInfo bi;
    SDL_GPUTransferBufferCreateInfo ti;
    static uint32_t white = 0xFFFFFFFFu;
    int i;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "bt3: SDL_Init: %s\n", SDL_GetError());
        return 0;
    }
    sDev = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, getenv("BT3_GPU_DEBUG") != NULL, NULL);
    {
        /* The window has the picture's shape: 896 lines high, 4:3 or 16:9 wide (1195 x 896 or
           1593 x 896), and keeps that shape when it is resized. BT3_WINDOW=WxH gives another start size. */
        extern int Port_AspectMilli(void);
        float want = (float)Port_AspectMilli() / 1000.0f;
        int h = 896, w = (int)((float)h * want + 0.5f), count = 0, pick = 0;
        SDL_DisplayID *displays = SDL_GetDisplays(&count);
        SDL_PropertiesID props = SDL_CreateProperties();

        sScale = getenv("BT3_SCALE") != NULL ? atoi(getenv("BT3_SCALE")) : Port_Setting("scale", 2);
        sScale = sScale < 1 ? 1 : sScale > 8 ? 8 : sScale;
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
        if (pick >= 1 && pick <= count) {
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(displays[pick - 1]));
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(displays[pick - 1]));
        } else if (pick != 0) {
            fprintf(stderr, "bt3: BT3_DISPLAY=%d: there are %d displays\n", pick, count);
        }
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, sFullscreen);
        sWindow = sDev ? SDL_CreateWindowWithProperties(props) : NULL;
        SDL_DestroyProperties(props);
        SDL_free(displays);
        if (sWindow != NULL && !sFullscreen) {
            SDL_SetWindowAspectRatio(sWindow, want, want);
        }
        fprintf(stderr, "bt3: internal resolution %dx (%d x %d)\n", sScale, 512 * sScale, 448 * sScale);
    }
    if (sWindow == NULL || !SDL_ClaimWindowForGPUDevice(sDev, sWindow)) {
        fprintf(stderr, "bt3: no GPU window: %s\n", SDL_GetError());
        return 0;
    }
    /* The game paces itself at 30 frames per second; waiting for the display's own refresh on top of that
       (VSYNC) costs up to a refresh period per frame. Prefer MAILBOX (no tearing, no wait), then IMMEDIATE. */
    {
        SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
        if (SDL_WindowSupportsGPUPresentMode(sDev, sWindow, SDL_GPU_PRESENTMODE_MAILBOX)) {
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
        } else if (SDL_WindowSupportsGPUPresentMode(sDev, sWindow, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        }
        SDL_SetGPUSwapchainParameters(sDev, sWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
        fprintf(stderr, "bt3: present mode %s\n", mode == SDL_GPU_PRESENTMODE_MAILBOX ? "mailbox" : mode == SDL_GPU_PRESENTMODE_IMMEDIATE ? "immediate" : "vsync");
    }
    sVs = shader(kGsVertSpv, sizeof(kGsVertSpv), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    sFs = shader(kGsFragSpv, sizeof(kGsFragSpv), SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 1);
    if (sVs == NULL || sFs == NULL) {
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
        /* The game's own textures have one level. A texture pack's have smaller copies as well, used (bit 3) where
           the texture is on 3D geometry. Not for 2D art: a smaller copy averages neighbouring texels, and on a
           sheet of HUD pieces those are another piece (the next colour layer of the health bar showed in the bar
           once widescreen drew the HUD narrower and the smaller copies came into use). */
        si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        si.max_lod = (i & 8) ? 16.0f : 0.0f;
        si.address_mode_u = (i & 2) ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_v = (i & 4) ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sSamplers[i] = SDL_CreateGPUSampler(sDev, &si);
    }
    sVu0Vs = shader(kVu0VertSpv, sizeof(kVu0VertSpv), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    sVu4Vs = shader(kVu4VertSpv, sizeof(kVu4VertSpv), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    sVu6Vs = shader(kVu6VertSpv, sizeof(kVu6VertSpv), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    bi.size = MAX_VU_VERTS * 48;
    sVuVbuf = SDL_CreateGPUBuffer(sDev, &bi);
    ti.size = MAX_VU_VERTS * 48;
    sVuXfer = SDL_CreateGPUTransferBuffer(sDev, &ti);
    sVuVerts = malloc(MAX_VU_VERTS * 48);
    sVuUni = malloc(MAX_VU_UNIFORMS * sizeof(Vu0Uniform));
    sVerts = malloc(MAX_VERTS * sizeof(Vtx));
    sDraws = malloc(MAX_DRAWS * sizeof(Draw));
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
        SDL_GPUShader *vs = sFxVs = shader(kFxVertSpv, sizeof(kFxVertSpv), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
        SDL_GPUShader *fs = shader(kOutlineFragSpv, sizeof(kOutlineFragSpv), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
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
        fs = shader(kAlphakeyFragSpv, sizeof(kAlphakeyFragSpv), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);
        cd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        cd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        cd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        ci.fragment_shader = fs;
        sKeyPipe = fs ? SDL_CreateGPUGraphicsPipeline(sDev, &ci) : NULL;
        if (sKeyPipe == NULL) {
            fprintf(stderr, "bt3: alpha key pipeline: %s\n", SDL_GetError());
            return 0;
        }
    }
    sDclutFs = shader(kDclutFragSpv, sizeof(kDclutFragSpv), SDL_GPU_SHADERSTAGE_FRAGMENT, 3, 1);
    copies_create();
    if (!Ui_Init(sWindow, sDev)) {
        fprintf(stderr, "bt3: the settings window could not be set up\n");
    }
    sFxOff = (unsigned)(getenv("BT3_FX_OFF") != NULL ? atoi(getenv("BT3_FX_OFF")) : Port_Setting("fx_off", 0)) & 31;
    sGlowPercent = getenv("BT3_GLOW") != NULL ? atoi(getenv("BT3_GLOW")) : Port_Setting("glow", GLOW_DEFAULT);
    sTexPackOn = Port_Setting("texture_pack", 1) != 0;
    gPortMusicPercent = Port_Setting("music", 100);
    gPortSePercent = Port_Setting("effects", 100);
    fprintf(stderr, "bt3: GPU renderer: %s\n", SDL_GetGPUDeviceDriver(sDev));
    TexPack_Init();
    fprintf(stderr, "bt3: %d logical processors, %d MB of memory\n", SDL_GetNumLogicalCPUCores(), SDL_GetSystemRAM());
    pipelines_preload();
    return 1;
}

/* Gs_StateBack (gs_core.c) moved every page's upload generation on by `delta`, so that nothing decoded from the
   pages in between is taken for them. The buffers the game draws into keep their standing: one that was drawn
   since the last upload over it still is. */
void GsGpu_PagesMoved(uint32_t delta) {
    int i;
    for (i = 0; i < sTargetCount; i++) {
        if (sTargets[i].gen != 0) { /* (0: its page was never uploaded to, and stays 0) */
            sTargets[i].gen += delta;
        }
    }
}

static int target_get(uint32_t fbp, int create) {
    SDL_GPUTextureCreateInfo ci;
    int i, slot;

    for (i = 0; i < sTargetCount; i++) {
        if (sTargets[i].fbp == fbp) {
            sTargets[i].last = gGsFrame;
            return i;
        }
    }
    if (!create) {
        return -1;
    }
    slot = sTargetCount;
    if (sTargetCount == MAX_TARGETS) {
        /* Every buffer address the game has ever drawn to holds three large textures. The menus and each stage
           use their own set, so one that has not been touched for two seconds gives its slot to the new one. */
        slot = -1;
        for (i = 0; i < sTargetCount; i++) {
            if (sTargets[i].last + 120 < gGsFrame && (slot < 0 || sTargets[i].last < sTargets[slot].last)) {
                slot = i;
            }
        }
        if (slot < 0) {
            return -1;
        }
        SDL_ReleaseGPUTexture(sDev, sTargets[slot].color);
        SDL_ReleaseGPUTexture(sDev, sTargets[slot].aux);
        SDL_ReleaseGPUTexture(sDev, sTargets[slot].depth);
    }
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = GS_W * SCALE;
    ci.height = GS_H * SCALE;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    sTargets[slot].color = SDL_CreateGPUTexture(sDev, &ci);
    ci.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    sTargets[slot].aux = SDL_CreateGPUTexture(sDev, &ci);
    ci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER; /* the depth effects read it */
    sTargets[slot].depth = SDL_CreateGPUTexture(sDev, &ci);
    sTargets[slot].fbp = fbp;
    sTargets[slot].cleared = 0;
    sTargets[slot].draws = 0;
    sTargets[slot].stale = 0;
    sTargets[slot].gen = 0;
    sTargets[slot].last = gGsFrame;
    if (slot == sTargetCount) {
        sTargetCount++;
    }
    return slot;
}

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

/* GS memory -> an RGBA texture for the current TEX0 (alpha as stored: 0x80 is opaque). */
static SDL_GPUTexture *texture_get(int ctx) {
    uint64_t t0 = gGs.tex0[ctx] & 0x1FFFFFFFFFFFFFFFull, texa = gGs.texa;
    uint32_t tbp = t0 & 0x3FFF, tbw = (t0 >> 14) & 0x3F, psm = (t0 >> 20) & 0x3F, tw = 1u << ((t0 >> 26) & 15), th = 1u << ((t0 >> 30) & 15);
    uint32_t cbp = (t0 >> 37) & 0x3FFF, cpsm = (t0 >> 51) & 15;
    uint32_t bits = (uint32_t)Gs_PsmBits(psm), gen = 0, pages, i, x, y, *px;
    SDL_GPUTextureCreateInfo ci;
    Tex *t;
    uint64_t tex_t0;
    TexPackImage pack;
    uint32_t packMaxAlpha = 255;
    SDL_GPUTextureFormat packFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    int replaced = 0;

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
    if (sPendingCount == MAX_PENDING) {
        return sWhite;
    }
    if (sTexCount == MAX_TEX) {
        /* full: drop everything not used for two seconds (or, failing that, not used this frame) in one sweep */
        unsigned keep = gGsFrame > 120 ? gGsFrame - 120 : 0;
        int pass, n;
        for (pass = 0; pass < 2 && sTexCount == MAX_TEX; pass++, keep = gGsFrame) {
            for (n = 0, i = 0; i < (uint32_t)sTexCount; i++) {
                if (sTex[i].last >= keep) {
                    sTex[n++] = sTex[i];
                } else {
                    SDL_ReleaseGPUTexture(sDev, sTex[i].tex);
                }
            }
            sTexCount = n;
        }
        sLast = NULL;
        memset(sTexBucket, 0, sizeof(sTexBucket));
        for (i = 0; i < (uint32_t)sTexCount; i++) {
            uint32_t h = tex_bucket(sTex[i].tex0, sTex[i].texa, sTex[i].gen);
            while (sTexBucket[h] != 0) {
                h = (h + 1) & (TEX_BUCKETS - 1);
            }
            sTexBucket[h] = (uint16_t)(i + 1);
        }
        if (sTexCount == MAX_TEX) {
            return sWhite; /* this one frame uses more textures than the cache holds */
        }
    }
    tex_t0 = gpu_now();
    /* a texture pack's replacement for this texture, if there is one (gs_texpack.c) */
    if (sTexPackOn && TexPack_Count() != 0) {
        const char *path = TexPack_Lookup(tbp, tbw, psm, tw, th, (uint32_t)((t0 >> 34) & 1), cbp, cpsm, &packMaxAlpha);
        if (path != NULL && TexPack_Load(path, &pack)) {
            static const SDL_GPUTextureFormat kFormat[4] = {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM,
                                                            SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM, SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM};
            packFormat = kFormat[pack.format];
            if (sPendingCount + pack.levels <= MAX_PENDING &&
                SDL_GPUTextureSupportsFormat(sDev, packFormat, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
                replaced = 1;
                gGpuTexReplaced++;
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
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = tw;
    ci.height = th;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    if (replaced) {
        ci.format = packFormat;
        ci.width = pack.w;
        ci.height = pack.h;
        ci.num_levels = (Uint32)pack.levels;
    }
    gGpuTexNs += gpu_now() - tex_t0;
    t = &sTex[sTexCount++];
    {
        uint32_t h = tex_bucket(t0, texa, gen);
        while (sTexBucket[h] != 0) {
            h = (h + 1) & (TEX_BUCKETS - 1);
        }
        sTexBucket[h] = (uint16_t)sTexCount;
    }
    gGpuNewTex++;
    gGpuNewTexPixels += tw * th;
    sLast = t;
    t->tex0 = t0;
    t->texa = texa;
    t->gen = gen;
    t->last = gGsFrame;
    t->tex = SDL_CreateGPUTexture(sDev, &ci);
    t->replaced = replaced;
    t->ow = tw;
    t->oh = th;
    t->rw = replaced ? pack.w : tw;
    t->rh = replaced ? pack.h : th;
    t->amax = packMaxAlpha;
    if (replaced) {
        int level;
        for (level = 0; level < pack.levels; level++) {
            sPending[sPendingCount].tex = t->tex;
            sPending[sPendingCount].w = pack.w >> level ? pack.w >> level : 1;
            sPending[sPendingCount].h = pack.h >> level ? pack.h >> level : 1;
            sPending[sPendingCount].px = malloc(pack.bytes[level]);
            memcpy(sPending[sPendingCount].px, pack.data[level], pack.bytes[level]);
            sPending[sPendingCount].bytes = pack.bytes[level];
            sPending[sPendingCount].level = (uint32_t)level;
            sPendingCount++;
        }
        TexPack_Free(&pack);
        return t->tex;
    }
    sPending[sPendingCount].tex = t->tex;
    sPending[sPendingCount].w = tw;
    sPending[sPendingCount].h = th;
    sPending[sPendingCount].px = px;
    sPending[sPendingCount].bytes = 0;
    sPending[sPendingCount].level = 0;
    sPendingCount++;
    return t->tex;
}

/* ((A - B) * C >> 7) + D as blend factors. Each of A, B, D is source, destination or zero, so the result is
   ks * Cs + kd * Cd with ks, kd from {0, 1, C, -C, 1 - C, 1 + C}. */
static void blend_of(uint64_t alpha, int abe, SDL_GPUColorTargetBlendState *b, uint32_t *key) {
    int A = alpha & 3, B = (alpha >> 2) & 3, C = (alpha >> 4) & 3, D = (alpha >> 6) & 3, k[2], i;
    SDL_GPUBlendFactor fc = C == 0 ? SDL_GPU_BLENDFACTOR_SRC_ALPHA : C == 1 ? SDL_GPU_BLENDFACTOR_DST_ALPHA : SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
    SDL_GPUBlendFactor fi = C == 0 ? SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA : C == 1 ? SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA : SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
    SDL_GPUBlendFactor f[2];
    int neg[2];

    SDL_zerop(b);
    b->color_write_mask = 0xF;
    b->enable_color_write_mask = false;
    *key = 0;
    if (!abe) {
        return;
    }
    /* k[i]: 0 nothing, 1 one, 2 C, 3 -C, 4 1 - C, 5 1 + C; i = 0 source, 1 destination */
    for (i = 0; i < 2; i++) {
        int c = (A == i) - (B == i), one = D == i;
        k[i] = c == 0 ? one : c > 0 ? (one ? 5 : 2) : (one ? 4 : 3);
        neg[i] = k[i] == 3;
        f[i] = k[i] == 0 ? SDL_GPU_BLENDFACTOR_ZERO : k[i] == 1 || k[i] == 5 ? SDL_GPU_BLENDFACTOR_ONE : k[i] == 4 ? fi : fc;
    }
    b->enable_blend = true;
    b->src_color_blendfactor = f[0];
    b->dst_color_blendfactor = f[1];
    b->color_blend_op = neg[0] ? SDL_GPU_BLENDOP_REVERSE_SUBTRACT : neg[1] ? SDL_GPU_BLENDOP_SUBTRACT : SDL_GPU_BLENDOP_ADD;
    b->src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE; /* the frame buffer keeps the source alpha */
    b->dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    b->alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    *key = 1u | (uint32_t)k[0] << 1 | (uint32_t)k[1] << 4 | (uint32_t)C << 7;
}

/* The blend state of a key made by blend_of (bits 0..8: enabled, k source, k destination, C). */
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
    b->enable_blend = true;
    b->src_color_blendfactor = f[0];
    b->dst_color_blendfactor = f[1];
    b->color_blend_op = k[0] == 3 ? SDL_GPU_BLENDOP_REVERSE_SUBTRACT : k[1] == 3 ? SDL_GPU_BLENDOP_SUBTRACT : SDL_GPU_BLENDOP_ADD;
    b->src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE; /* the frame buffer keeps the source alpha */
    b->dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    b->alpha_blend_op = SDL_GPU_BLENDOP_ADD;
}

/* Where the keys of the pipelines the game has used are remembered between runs. Creating a pipeline takes the
   driver 2 to 10 ms (measured: 46 ms for the ten of the first stage frame), so the known ones are made at start. */
static const char *pipeline_file(void) {
    return getenv("BT3_PIPELINES") != NULL ? getenv("BT3_PIPELINES") : "bt3_pipelines.txt";
}

/* Creates the pipeline of a key (everything a pipeline depends on is in the key):
   bits 0..8 blend, 10..11 depth test, 12 depth write, 13..14 topology, 16..19 colour write mask,
   20..21 vertices: 0 GS vertices, 1 program 0, 2 program 4, 3 program 6. */
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
    blend_from_key(key & 0x1FF, &cds[0].blend_state);
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
    ci.fragment_shader = sFs;
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
    /* the exact alpha byte: never blended, written whenever the frame's alpha is writable */
    cds[1].format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    cds[1].blend_state.color_write_mask = (wmask & SDL_GPU_COLORCOMPONENT_A) ? SDL_GPU_COLORCOMPONENT_R : 0;
    cds[1].blend_state.enable_color_write_mask = true;
    ci.target_info.color_target_descriptions = cds;
    ci.target_info.num_color_targets = 2;
    ci.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
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

static int pipeline_get(int ctx, int topo, int vu) {
    uint64_t test = gGs.test[ctx], zb = gGs.zbuf[ctx];
    int zte = (test >> 16) & 1, ztst = (test >> 17) & 3, zwrite = !((zb >> 32) & 1);
    SDL_GPUColorTargetBlendState unused;
    uint32_t bkey, key, wmask, m = (uint32_t)(gGs.frame[ctx] >> 32);
    int i;

    blend_of(gGs.alpha[ctx], (int)((gGs.prim >> 6) & 1), &unused, &bkey);
    if (!zte) {
        ztst = 1;
    }
    /* FRAME.FBMSK, in the whole-channel forms ordinary drawing uses (alpha only, everything but alpha, ...):
       a channel whose eight mask bits are all set is not written. Partial masks are not representable. */
    wmask = ((m & 0xFF) != 0xFF ? SDL_GPU_COLORCOMPONENT_R : 0) | ((m & 0xFF00) != 0xFF00 ? SDL_GPU_COLORCOMPONENT_G : 0) |
            ((m & 0xFF0000) != 0xFF0000 ? SDL_GPU_COLORCOMPONENT_B : 0) | ((m & 0xFF000000u) != 0xFF000000u ? SDL_GPU_COLORCOMPONENT_A : 0);
    key = bkey | (uint32_t)ztst << 10 | (uint32_t)zwrite << 12 | (uint32_t)topo << 13 | wmask << 16 | (uint32_t)vu << 20;
    if (vu == 4) { /* asked for by depth_clut: the table pass, which only depends on blending and the write mask */
        key = bkey | wmask << 16 | 1u << 22;
    }
    for (i = 0; i < sPipeCount; i++) {
        if (sPipes[i].key == key) {
            return i;
        }
    }
    return pipeline_create(key, 1);
}

static void put(const GsVertex *v, float x, float y, float s, float t, float q, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float zmax) {
    Vtx *o = &sVerts[sVertCount++];
    o->x = x;
    o->y = y;
    o->z = (float)((double)v->z / zmax);
    o->r = r; o->g = g; o->b = b; o->a = a;
    o->s = s; o->t = t; o->q = q;
}

/* The state part of a draw: everything the current GS registers decide (target, texture, sampler, blending,
   tests, scissor). Returns 0 when the primitive belongs to a pass that is not drawn here. `sprite` = the primitive
   is a sprite; `vu` selects the pipeline family (0: GS vertices, 1: vertex program 0's vertices). us / vs: scale
   of texture coordinates when the texture is a frame buffer. */
extern int gGsMainFbp;

/* A 256-colour table at `cbp` (32-bit, the GS's index order undone) as a 256 x 1 texture; alpha as stored. */
static SDL_GPUTexture *clut_texture(uint32_t cbp) {
    uint32_t px[256], hash = 2166136261u, i;
    SDL_GPUTextureCreateInfo ci;
    int k, old = 0;

    for (i = 0; i < 256; i++) {
        uint32_t n = (i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1);
        px[i] = Gs_VramRead(cbp, 1, 0, n & 15, n >> 4);
        hash = (hash ^ px[i]) * 16777619u;
    }
    for (k = 0; k < MAX_CLUTS; k++) {
        if (sCluts[k].tex != NULL && sCluts[k].hash == hash) {
            sCluts[k].last = gGsFrame;
            return sCluts[k].tex;
        }
        if (sCluts[k].tex == NULL || sCluts[k].last < sCluts[old].last) {
            old = sCluts[k].tex == NULL && sCluts[old].tex == NULL ? old : k;
        }
    }
    if (sPendingCount == MAX_PENDING || (sCluts[old].tex != NULL && sCluts[old].last == gGsFrame)) {
        return NULL;
    }
    if (sCluts[old].tex != NULL) {
        SDL_ReleaseGPUTexture(sDev, sCluts[old].tex);
    }
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = 256;
    ci.height = 1;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    sCluts[old].tex = SDL_CreateGPUTexture(sDev, &ci);
    sCluts[old].hash = hash;
    sCluts[old].last = gGsFrame;
    sPending[sPendingCount].tex = sCluts[old].tex;
    sPending[sPendingCount].bytes = sPending[sPendingCount].level = 0;
    sPending[sPendingCount].w = 256;
    sPending[sPendingCount].h = 1;
    sPending[sPendingCount].px = malloc(sizeof(px));
    memcpy(sPending[sPendingCount].px, px, sizeof(px));
    sPendingCount++;
    return sCluts[old].tex;
}

static int pipeline_get(int ctx, int topo, int vu);

/* GfxPost_DrawDepthClut as one full-screen pass (the game sends 16 strips; they become one draw). */
static void depth_clut(int ctx) {
    uint64_t sc = gGs.scissor[ctx];
    uint32_t wm = (uint32_t)(gGs.frame[ctx] >> 32);
    Draw d, *last = sDrawCount ? &sDraws[sDrawCount - 1] : NULL;

    if (sDrawCount == MAX_DRAWS) {
        return;
    }
    memset(&d, 0, sizeof(d));
    d.native = 3;
    d.src = sDepthByteIsFog ? 0 : 1;
    d.target = target_get((uint32_t)(gGs.frame[ctx] & 0x1FF), 0);
    /* switched off by the user: the passes that colour the picture (the ones that only write alpha feed the glow) */
    if (d.target < 0 || ((sFxOff & 4) && d.src == 0 && wm != 0x00FFFFFFu)) {
        return;
    }
    {
        uint32_t cbp = (uint32_t)((gGs.tex0[ctx] >> 37) & 0x3FFF);
        if (cbp == 0x3E8C) {
            return; /* the outline's own passes: the outline is drawn natively at its marker (outline.frag) */
        }
        if (cbp == 0x3E94 && (sFxOff & 2)) {
            return; /* the see-through tint, switched off */
        }
    }
    d.tex = clut_texture((uint32_t)((gGs.tex0[ctx] >> 37) & 0x3FFF));
    if (d.tex == NULL) {
        return;
    }
    d.pipeline = pipeline_get(ctx, 0, 4);
    d.blendc = (float)((gGs.alpha[ctx] >> 32) & 0xFF) / 128.0f;
    d.misc[0] = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    d.scissor.x = (int)(sc & 0x7FF) * SCALE;
    d.scissor.y = (int)((sc >> 32) & 0x7FF) * SCALE;
    d.scissor.w = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d.scissor.x;
    d.scissor.h = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d.scissor.y;
    if (last != NULL && last->native == 3 && last->tex == d.tex && last->target == d.target && last->pipeline == d.pipeline && last->src == d.src) {
        return; /* the next strip of the same pass */
    }
    sDraws[sDrawCount++] = d;
}

static int draw_state(int ctx, int topo, int sprite, int vu, Draw *d, float *us, float *vs) {
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
            sDepthByteIsFog = tpsm == 0x1B;
            if (tpsm != 0x1B) { /* the copy of the ids: taken now, read by the passes that follow */
                Draw c, *last = sDrawCount ? &sDraws[sDrawCount - 1] : NULL;
                memset(&c, 0, sizeof(c));
                c.native = 4;
                c.target = target_get(tbp / 32, 0);
                if (c.target >= 0 && sDrawCount < MAX_DRAWS && !(last != NULL && last->native == 4)) {
                    sDraws[sDrawCount++] = c;
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
                int t = target_get(fbp, 1);
                if (t >= 0) {
                    sTargets[t].stale = 1;
                    sTargets[t].cleared = 1;
                    sTargets[t].gen = gGsPageGen[fbp & 511];
                }
            }
            sSkipped++;
            return 0;
        }
        if (tme && sprite && tpsm == 0x1B && tbp / 32 == zbp) {
            depth_clut(ctx); /* that byte through a table over the screen: a native pass */
            sSkipped++;
            return 0;
        }
        if (tme && ((tpsm & 0x30) == 0x30 || tbp / 32 == zbp || tpsm == 0x1B || tpsm == 0x24 || tpsm == 0x2C)) {
            int t = target_get(fbp, 0);
            if (t >= 0 && fbp != (uint32_t)gGsMainFbp) {
                sTargets[t].stale = 1; /* a work buffer missed a pass */
            }
            sSkipped++;
            return 0;
        }
    }
    memset(d, 0, sizeof(*d));
    d->target = target_get((uint32_t)(gGs.frame[ctx] & 0x1FF), 1);
    if (d->target < 0) {
        return 0;
    }
    d->tex = sWhite;
    if (!tme && sprite) {
        sTargets[d->target].stale = 0; /* cleared by the game: what is drawn into it from here on is real */
    }
    if (tme) {
        src = target_get((uint32_t)((t0 & 0x3FFF) / 32), 0);
        /* a frame buffer used as a texture: only if nothing was uploaded over it since it was drawn */
        if (src >= 0 && (t0 & 0x1F) == 0 && sTargets[src].cleared && sTargets[src].gen == gGsPageGen[sTargets[src].fbp & 511]) {
            uint32_t tp = (uint32_t)((t0 >> 20) & 0x3F);
            if (sTargets[src].stale) {
                /* The buffer was meant to hold a copy or a step of an effect that was dropped; what it holds now
                   is whatever was drawn there before (seen as blocks of noise over the picture). */
                sSkipped++;
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
                ((((gGs.alpha[ctx] & 0xFF) == 0x68) && (sFxOff & 8)) || (((gGs.alpha[ctx] & 0xFF) == 0x54) && (sFxOff & 16)))) {
                sSkipped++;
                return 0;
            }
            if (src == d->target || tp > 1) {
                if (d->target != src && (uint32_t)(gGs.frame[ctx] & 0x1FF) != (uint32_t)gGsMainFbp) {
                    sTargets[d->target].stale = 1; /* a work buffer missed a pass */
                }
                sSkipped++;
                return 0;
            }
            d->tex = sTargets[src].color;
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
    if (d->tex != NULL && sLast != NULL && sLast->tex == d->tex && sLast->replaced) {
        d->sampler |= 1 | 8; /* (GsGpu_Draw takes the 8 off again for 2D art) */
    }

    d->pipeline = pipeline_get(ctx, topo, vu);
    d->mode[0] = !tme ? 0 : d->tex_is_target ? 2 : 1; /* 2: a frame buffer as texture, its alpha is already rescaled */
    /* (BT3_TEX_ALPHA=0 and BT3_TEX_2D=0 switch the two special treatments of replacements off, for telling which one
       a fault comes from) */
    if (d->mode[0] == 1 && sLast != NULL && sLast->tex == d->tex && sLast->replaced && !(getenv("BT3_TEX_ALPHA") != NULL && atoi(getenv("BT3_TEX_ALPHA")) == 0)) {
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
    d->scissor.x = (int)(sc & 0x7FF) * SCALE;
    d->scissor.y = (int)((sc >> 32) & 0x7FF) * SCALE;
    d->scissor.w = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d->scissor.x;
    d->scissor.h = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d->scissor.y;
    return 1;
}

static int same_state(const Draw *a, const Draw *b) {
    return !a->native && a->vu == b->vu && a->target == b->target && a->tex == b->tex && a->sampler == b->sampler && a->pipeline == b->pipeline &&
           memcmp(a->mode, b->mode, sizeof(a->mode)) == 0 && a->misc[0] == b->misc[0] && a->misc[1] == b->misc[1] &&
           a->misc[2] == b->misc[2] && a->misc[3] == b->misc[3] && a->blendc == b->blendc &&
           memcmp(a->rect, b->rect, sizeof(a->rect)) == 0 && memcmp(a->orig, b->orig, sizeof(a->orig)) == 0 &&
           memcmp(&a->scissor, &b->scissor, sizeof(SDL_Rect)) == 0;
}

/* A draw that tests the alpha already in the frame buffer (TEST.DATE: the fill of a HUD bar cut to length by a
   mask, a word shown through a band, and also some of the stage's own layers): at the start of a run of such
   draws, a copy of the alpha bytes is taken for them to read. Called by every way a primitive can be recorded;
   left out of the vertex-program paths at first, the stage's layers tested against the copy the HUD had made the
   frame before, and showed a strip of different-looking scenery where an announcement's band had been. */
static void date_snapshot(const Draw *d) {
    Draw *prev = sDrawCount ? &sDraws[sDrawCount - 1] : NULL;
    Draw c;

    if (d->misc[1] == 0.0f || (prev != NULL && !prev->native && prev->misc[1] != 0.0f && prev->target == d->target)) {
        return;
    }
    memset(&c, 0, sizeof(c));
    c.native = 5;
    c.target = d->target;
    if (sDrawCount + 1 < MAX_DRAWS) {
        sDraws[sDrawCount++] = c;
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
    Draw d, *last;
    int i, packed2d = 0;

    if (sVertCount + 6 > MAX_VERTS || sDrawCount == MAX_DRAWS) {
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
    d.first = sVertCount;
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
                sVerts[d.first + i].s /= q[1];
                sVerts[d.first + i].t /= q[1];
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
        float ds = 0.5f * (sVerts[d.first + 1].s - sVerts[d.first].s) / (v[1].x - v[0].x);
        float dt = 0.5f * (sVerts[d.first + 2].t - sVerts[d.first].t) / (v[1].y - v[0].y);
        uint32_t k;
        for (k = d.first; k < sVertCount; k++) {
            sVerts[k].s -= ds;
            sVerts[k].t -= dt;
        }
        /* the user's strength for the glare and the glow: the pass that adds the blurred buffer to the picture */
        if (sGlowPercent != 100 && (uint32_t)(gGs.frame[ctx] & 0x1FF) == (uint32_t)gGsMainFbp && ((gGs.prim >> 6) & 1) &&
            (gGs.alpha[ctx] & 0xFF) == 0x68) {
            for (k = d.first; k < sVertCount; k++) {
                sVerts[k].r = (uint8_t)(sVerts[k].r * sGlowPercent / 100 > 255 ? 255 : sVerts[k].r * sGlowPercent / 100);
                sVerts[k].g = (uint8_t)(sVerts[k].g * sGlowPercent / 100 > 255 ? 255 : sVerts[k].g * sGlowPercent / 100);
                sVerts[k].b = (uint8_t)(sVerts[k].b * sGlowPercent / 100 > 255 ? 255 : sVerts[k].b * sGlowPercent / 100);
            }
        }
    }
    if (Port_IsWide() && !gPortMenuMode && !d.tex_is_target && (type == 6 || fst) && sTargets[d.target].fbp == (uint32_t)gGsMainFbp) {
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
        float x0 = sVerts[d.first].x, x1 = x0, y0 = sVerts[d.first].y, y1 = y0, pivot = -1.0f;
        uint32_t k;
        for (k = d.first; k < sVertCount; k++) {
            if (sVerts[k].x < x0) { x0 = sVerts[k].x; }
            if (sVerts[k].x > x1) { x1 = sVerts[k].x; }
            if (sVerts[k].y < y0) { y0 = sVerts[k].y; }
            if (sVerts[k].y > y1) { y1 = sVerts[k].y; }
        }
        if (type == 6 && !d.mode[0] && y1 - y0 >= 400.0f) {
            pivot = -1.0f;
        } else if (x1 - x0 >= 480.0f) {
            /* as wide as the screen (the bands and streaks of READY / FIGHT, flashes, speed lines): meant to
               reach from edge to edge, so it keeps the full width */
            pivot = -1.0f;
        } else if (sAnchor != 0) {
            pivot = sAnchor == 1 ? 0.0f : sAnchor == 2 ? 512.0f : 256.0f;
            if (sAnchor == 4) { /* a part without a fixed side (captions: technique names): the side it is on */
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
            for (k = d.first; k < sVertCount; k++) {
                sVerts[k].x = pivot + (sVerts[k].x - pivot) * narrow;
            }
        }
    }
    d.count = sVertCount - d.first;
    sTargets[d.target].draws++;
    sTargets[d.target].gen = gGsPageGen[sTargets[d.target].fbp & 511];
    last = sDrawCount ? &sDraws[sDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d)) {
        last->count += d.count; /* same state as the previous primitive: one draw call */
    } else {
        sDraws[sDrawCount++] = d;
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
    Draw d, *last;
    float us, vs;
    uint32_t i, k, flag;

    if (count < 3 || sVuVertCount + (count - 2) * 3 > MAX_VU_VERTS || sDrawCount == MAX_DRAWS || sVuUniCount == MAX_VU_UNIFORMS) {
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
    d.first = sVuVertCount;
    for (i = 0; i + 2 < count; i++) { /* strip -> list, without the flagged triangles */
        memcpy(&flag, &vertices[(i + 2) * 16 + 3], 4);
        if (flag & 0xFFFF) {
            continue;
        }
        for (k = 0; k < 3; k++) {
            float *o = &sVuVerts[sVuVertCount++ * 12];
            memcpy(o, &vertices[(i + k) * 16], 16);          /* position */
            memcpy(o + 4, &vertices[(i + k) * 16 + 8], 16);  /* colour */
            memset(o + 8, 0, 16);
        }
    }
    d.count = sVuVertCount - d.first;
    if (d.count == 0) {
        return;
    }
    sTargets[d.target].draws++;
    sTargets[d.target].gen = gGsPageGen[sTargets[d.target].fbp & 511];
    last = sDrawCount ? &sDraws[sDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d) && memcmp(&sVuUni[last->uniform], &u, sizeof(u)) == 0) {
        last->count += d.count;
        return;
    }
    d.uniform = (int)sVuUniCount;
    sVuUni[sVuUniCount++] = u;
    sDraws[sDrawCount++] = d;
}

/* Vertex program 4 as a shader: one strip of stage geometry (see shaders/vu4.vert). Vertices: position, colour
   (0..255 floats), texture coordinates, 48 bytes; the screen matrix is VU memory 0..3. */
void GsGpu_DrawVu4(int ctx, const float *vertices, uint32_t count, const float *consts) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    Vu0Uniform u;
    Draw d, *last;
    float us, vs;
    uint32_t i, k;

    if (count < 3 || sVuVertCount + (count - 2) * 3 > MAX_VU_VERTS || sDrawCount == MAX_DRAWS || sVuUniCount == MAX_VU_UNIFORMS) {
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
    d.first = sVuVertCount;
    for (i = 0; i + 2 < count; i++) { /* strip -> list */
        for (k = 0; k < 3; k++) {
            memcpy(&sVuVerts[sVuVertCount++ * 12], &vertices[(i + k) * 12], 48);
        }
    }
    d.count = sVuVertCount - d.first;
    sTargets[d.target].draws++;
    sTargets[d.target].gen = gGsPageGen[sTargets[d.target].fbp & 511];
    last = sDrawCount ? &sDraws[sDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d) && memcmp(&sVuUni[last->uniform], &u, sizeof(u)) == 0) {
        last->count += d.count;
        return;
    }
    d.uniform = (int)sVuUniCount;
    sVuUni[sVuUniCount++] = u;
    sDraws[sDrawCount++] = d;
}

/* Vertex program 0 as a shader: one strip of the fighters' models (see shaders/vu0.vert and gs_vu1.c). The strip
   becomes a triangle list of the program's own 48-byte vertices; the constants go into a uniform block. */
void GsGpu_DrawVu0(int layer, int ctx, const float *vertices, uint32_t count, const float *consts) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    Vu0Uniform u;
    Draw d, *last;
    float us, vs;
    uint32_t i, k;

    if (count < 3 || sVuVertCount + (count - 2) * 3 > MAX_VU_VERTS || sDrawCount == MAX_DRAWS || sVuUniCount == MAX_VU_UNIFORMS) {
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
    d.first = sVuVertCount;
    for (i = 0; i + 2 < count; i++) { /* strip -> list */
        if (layer == 4) { /* debris: a flagged vertex does not complete a triangle */
            uint32_t flag;
            memcpy(&flag, &vertices[(i + 2) * 12 + 3], 4);
            if (flag & 0xFFFF) {
                continue;
            }
        }
        for (k = 0; k < 3; k++) {
            memcpy(&sVuVerts[sVuVertCount++ * 12], &vertices[(i + k) * 12], 48);
        }
    }
    d.count = sVuVertCount - d.first;
    sTargets[d.target].draws++;
    sTargets[d.target].gen = gGsPageGen[sTargets[d.target].fbp & 511];
    last = sDrawCount ? &sDraws[sDrawCount - 1] : NULL;
    if (last != NULL && same_state(last, &d) && memcmp(&sVuUni[last->uniform], &u, sizeof(u)) == 0) {
        last->count += d.count;
        return;
    }
    d.uniform = (int)sVuUniCount;
    sVuUni[sVuUniCount++] = u;
    sDraws[sDrawCount++] = d;
}

/* A marker from the game's display list (port/src/gs_marker.c): draw the native version of an effect here, on
   the frame buffer the current context draws to. 1 = the outline. */
/* The game uploaded pixels straight into a display buffer (a movie frame). Remembers the place in the frame's
   draw order; the pixels are taken from GS memory at the end of the frame (frame_end). */
void GsGpu_FbUpload(int second) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    Draw d;

    if (sDrawCount == MAX_DRAWS) {
        return;
    }
    memset(&d, 0, sizeof(d));
    d.native = 100 + (second != 0);
    d.target = target_get(second ? 0x70 : 0, 1);
    if (d.target >= 0) {
        sDraws[sDrawCount++] = d;
    }
}

void GsGpu_Native(int effect) {
    if (gPortResim) { /* a frame that is only being re-run: nothing is drawn */
        return;
    }
    uint64_t sc = gGs.scissor[0];
    Draw d;

    /* 1 = outline. 2 (the see-through tint) is drawn by the generic table pass now (depth_clut), from the game's
       own table. */
    if (effect >= 0x10 && effect <= 0x14) {
        sAnchor = effect == 0x13 ? 0 : effect == 0x14 ? 4 : effect - 0x0F; /* 1 left, 2 right, 3 centre, 4 by position */
        return;
    }
    if (effect != 1 || sDrawCount == MAX_DRAWS || (sFxOff & 1)) {
        return;
    }
    sNative++;
    memset(&d, 0, sizeof(d));
    d.native = effect;
    d.target = target_get((uint32_t)(gGs.frame[0] & 0x1FF), 0);
    if (d.target < 0) {
        return;
    }
    d.scissor.x = (int)(sc & 0x7FF) * SCALE;
    d.scissor.y = (int)((sc >> 32) & 0x7FF) * SCALE;
    d.scissor.w = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d.scissor.x;
    d.scissor.h = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d.scissor.y;
    sDraws[sDrawCount++] = d;
}

static void frame_end(void);

void GsGpu_FrameEnd(void) {
    uint64_t t0 = gpu_now();
    if (gPortResim) { /* a frame that is only being re-run: nothing was recorded, nothing is shown */
        return;
    }
    frame_end();
    scale_apply();
    gGpuEndNs += gpu_now() - t0;
}

static void frame_end(void) {
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass *copy;
    SDL_GPURenderPass *pass = NULL;
    SDL_GPUTexture *swap = NULL;
    SDL_Event ev;
    Uint32 sw = 0, sh = 0;
    int cur = -1, best = -1, bound = -1, i;
    bool acquired;
    uint64_t sEndT[5];
    int lastPipe = -1, lastUni = -1, lastSampler = -1, haveFu = 0, haveScissor = 0; /* what the pass has set (the replay loop) */
    SDL_GPUTexture *lastTex = NULL;
    struct { int32_t mode[4]; float misc[4]; float rect[4]; float orig[4]; } lastFu;
    SDL_Rect lastScissor;
    float lastBlend = -1.0f;
    uint32_t n;

    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) {
            exit(0);
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key == SDLK_F1) {
            Ui_Toggle();
            continue;
        }
        if (Ui_Event(&ev)) { /* the settings window is open and used it (Esc closes it) */
            continue;
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
            exit(0);
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key == SDLK_F11) {
            fullscreen_set(!sFullscreen);
            Port_SettingSave("fullscreen", sFullscreen);
            Port_SettingsWrite();
        }
    }
    if (getenv("BT3_GS_VERBOSE") != NULL && gGsFrame % 30 == 0) {
        fprintf(stderr, "gpu: frame %u: %u draws, %u vertices, %d targets, %d textures, %d pipelines, %u primitives of PS2-only passes dropped, %u native effects\n",
                gGsFrame, sDrawCount, sVertCount + sVuVertCount, sTargetCount, sTexCount, sPipeCount, sSkipped, sNative);
    }
    if (getenv("BT3_GPU_CUT") != NULL && (int)gGsFrame == atoi(getenv("BT3_GPU_CUT")) && strchr(getenv("BT3_GPU_CUT"), ':') != NULL) {
        /* BT3_GPU_CUT=<frame>:<n>: only the first n draws of that frame (finding the draw that spoils a picture) */
        uint32_t n = (uint32_t)atoi(strchr(getenv("BT3_GPU_CUT"), ':') + 1);
        fprintf(stderr, "cut: frame %u has %u draws, keeping %u\n", gGsFrame, sDrawCount, n < sDrawCount ? n : sDrawCount);
        if (n < sDrawCount) {
            const Draw *d = &sDraws[n];
            fprintf(stderr, "cut: first dropped draw: native %d target %d (fbp %03x) vu %d count %u tex_is_target %d mode %d %d %d %d\n", d->native,
                    d->target, d->target >= 0 ? sTargets[d->target].fbp : 0, d->vu, d->count, d->tex_is_target, d->mode[0], d->mode[1], d->mode[2], d->mode[3]);
            sDrawCount = n;
        }
    }
    if (getenv("BT3_GPU_TAIL") != NULL && (int)gGsFrame == atoi(getenv("BT3_GPU_TAIL"))) { /* the frame's last draws */
        uint32_t k;
        for (k = sDrawCount > 14 ? sDrawCount - 14 : 0; k < sDrawCount; k++) {
            const Draw *d = &sDraws[k];
            fprintf(stderr, "tail: draw %u native %d target %d vu %d first %u count %u pipeline key %08x mode %d %d %d %d misc %.0f %.0f %.0f %.0f x %.1f..\n",
                    k, d->native, d->target, d->vu, d->first, d->count, d->native ? 0 : sPipes[d->pipeline].key, d->mode[0], d->mode[1], d->mode[2],
                    d->mode[3], d->misc[0], d->misc[1], d->misc[2], d->misc[3], d->native || d->vu ? 0.0f : sVerts[d->first].x);
            if (!d->native && !d->vu && d->count <= 12 && k + 3 >= sDrawCount) {
                uint32_t j;
                for (j = 0; j < d->count; j++) {
                    const Vtx *v = &sVerts[d->first + j];
                    fprintf(stderr, "tail:    (%.1f, %.1f) z %.6f st %.4f %.4f q %.3f rgba %u %u %u %u\n", v->x, v->y, v->z, v->s, v->t, v->q, v->r, v->g, v->b, v->a);
                }
            }
        }
    }
    sEndT[0] = gpu_now();
    cmd = SDL_AcquireGPUCommandBuffer(sDev);
    /* uploads: this frame's vertices and the textures decoded for it */
    copy = SDL_BeginGPUCopyPass(cmd);
    if (sVertCount != 0) {
        SDL_GPUTransferBufferLocation src;
        SDL_GPUBufferRegion dst;
        void *p = SDL_MapGPUTransferBuffer(sDev, sVxfer, true);
        memcpy(p, sVerts, sVertCount * sizeof(Vtx));
        SDL_UnmapGPUTransferBuffer(sDev, sVxfer);
        src.transfer_buffer = sVxfer;
        src.offset = 0;
        dst.buffer = sVbuf;
        dst.offset = 0;
        dst.size = sVertCount * sizeof(Vtx);
        SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    }
    if (sVuVertCount != 0) {
        SDL_GPUTransferBufferLocation src;
        SDL_GPUBufferRegion dst;
        void *p = SDL_MapGPUTransferBuffer(sDev, sVuXfer, true);
        memcpy(p, sVuVerts, sVuVertCount * 48);
        SDL_UnmapGPUTransferBuffer(sDev, sVuXfer);
        src.transfer_buffer = sVuXfer;
        src.offset = 0;
        dst.buffer = sVuVbuf;
        dst.offset = 0;
        dst.size = sVuVertCount * 48;
        SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    }
    for (i = 0; i < sPendingCount; i++) {
        SDL_GPUTransferBufferCreateInfo ti;
        SDL_GPUTransferBuffer *tb;
        SDL_GPUTextureTransferInfo src;
        SDL_GPUTextureRegion dst;
        void *p;
        SDL_zero(ti);
        ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        ti.size = sPending[i].bytes ? sPending[i].bytes : sPending[i].w * sPending[i].h * 4;
        tb = SDL_CreateGPUTransferBuffer(sDev, &ti);
        p = SDL_MapGPUTransferBuffer(sDev, tb, false);
        memcpy(p, sPending[i].px, ti.size);
        SDL_UnmapGPUTransferBuffer(sDev, tb);
        SDL_zero(src);
        src.transfer_buffer = tb;
        SDL_zero(dst);
        dst.texture = sPending[i].tex;
        dst.mip_level = sPending[i].level;
        dst.w = sPending[i].w;
        dst.h = sPending[i].h;
        dst.d = 1;
        SDL_UploadToGPUTexture(copy, &src, &dst, false);
        SDL_ReleaseGPUTransferBuffer(sDev, tb);
        free(sPending[i].px);
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
    for (n = 0; n < sDrawCount; n++) {
        const Draw *d = &sDraws[n];
        SDL_GPUBufferBinding vb;
        SDL_GPUTextureSamplerBinding ts;
        SDL_FColor bc;
        struct { int32_t mode[4]; float misc[4]; float rect[4]; float orig[4]; } fu;

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
            bl.destination.texture = sTargets[d->target].color;
            bl.destination.w = GS_W * SCALE;
            bl.destination.h = GS_H * SCALE;
            bl.load_op = SDL_GPU_LOADOP_DONT_CARE;
            bl.filter = SDL_GPU_FILTER_LINEAR;
            if (sFbTex[d->native - 100] != NULL) {
                SDL_BlitGPUTexture(cmd, &bl);
                sTargets[d->target].cleared = 1;
            }
            continue;
        }
        if (d->native == 5) { /* the alpha bytes as they are now, for the draws that test them (TEST.DATE) */
            SDL_GPUCopyPass *cp;
            SDL_GPUTextureLocation from, to;
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
                pass = NULL;
            }
            cur = -1;
            if (!sTargets[d->target].cleared) {
                continue;
            }
            cp = SDL_BeginGPUCopyPass(cmd);
            SDL_zero(from);
            SDL_zero(to);
            from.texture = sTargets[d->target].aux;
            to.texture = sDateCopy;
            SDL_CopyGPUTextureToTexture(cp, &from, &to, GS_W * SCALE, GS_H * SCALE, 1, false);
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
            if (!sTargets[d->target].cleared) {
                continue;
            }
            cp = SDL_BeginGPUCopyPass(cmd);
            SDL_zero(from);
            SDL_zero(to);
            from.texture = sTargets[d->target].aux;
            to.texture = sAuxCopy;
            SDL_CopyGPUTextureToTexture(cp, &from, &to, GS_W * SCALE, GS_H * SCALE, 1, false);
            SDL_EndGPUCopyPass(cp);
            continue;
        }
        if (d->native == 3) { /* a table pass: both attachments, no depth; reads the depth texture or the alpha copy */
            SDL_GPUColorTargetInfo cts[2];
            SDL_GPUTextureSamplerBinding fs[3];
            SDL_FColor bc;
            SDL_Rect sc = d->scissor;
            Target *t = &sTargets[d->target];
            float params[4] = {(float)d->src, d->misc[0], 0.0f, 0.0f};
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
                pass = NULL;
            }
            cur = -1;
            if (!t->cleared) {
                continue;
            }
            SDL_zero(cts);
            cts[0].texture = t->color;
            cts[0].load_op = SDL_GPU_LOADOP_LOAD;
            cts[0].store_op = SDL_GPU_STOREOP_STORE;
            cts[1] = cts[0];
            cts[1].texture = t->aux;
            pass = SDL_BeginGPURenderPass(cmd, cts, 2, NULL);
            SDL_BindGPUGraphicsPipeline(pass, sPipes[d->pipeline].p);
            bc.r = bc.g = bc.b = bc.a = d->blendc;
            SDL_SetGPUBlendConstants(pass, bc);
            fs[0].texture = t->depth;
            fs[0].sampler = sSamplers[6];
            fs[1].texture = sAuxCopy;
            fs[1].sampler = sSamplers[6];
            fs[2].texture = d->tex;
            fs[2].sampler = sSamplers[6];
            SDL_BindGPUFragmentSamplers(pass, 0, fs, 3);
            SDL_PushGPUFragmentUniformData(cmd, 0, params, sizeof(params));
            SDL_SetGPUScissor(pass, &sc);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
            pass = NULL;
            continue;
        }
        if (d->native) { /* a native full-screen effect: its own pass on the colour texture alone, reading the alpha copy */
            SDL_GPUColorTargetInfo ft;
            SDL_GPUTextureSamplerBinding fs;
            SDL_Rect sc;
            float params[4];
            Target *t = &sTargets[d->target];
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
                pass = NULL;
            }
            cur = -1;
            if (!t->cleared) {
                continue;
            }
            SDL_zero(ft);
            ft.texture = t->color;
            ft.load_op = SDL_GPU_LOADOP_LOAD;
            ft.store_op = SDL_GPU_STOREOP_STORE;
            pass = SDL_BeginGPURenderPass(cmd, &ft, 1, NULL);
            SDL_BindGPUGraphicsPipeline(pass, d->native == 2 ? sKeyPipe : sOutlinePipe);
            fs.texture = sAuxCopy; /* the ids as copied by the game, not the live alpha (later passes overwrite it) */
            fs.sampler = sSamplers[6]; /* nearest, clamped */
            SDL_BindGPUFragmentSamplers(pass, 0, &fs, 1);
            params[0] = 1.0f / (float)GS_W;
            params[1] = 1.0f / (float)GS_H;
            params[2] = 100.0f / 255.0f; /* the dark rectangle's 0x64 */
            params[3] = getenv("BT3_FX_DEBUG") != NULL ? 1.0f : 0.0f;
            SDL_PushGPUFragmentUniformData(cmd, 0, params, sizeof(params));
            sc = d->scissor;
            SDL_SetGPUScissor(pass, &sc);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
            pass = NULL;
            continue;
        }
        if (d->target != cur) {
            SDL_GPUColorTargetInfo ct, cts[2];
            SDL_GPUDepthStencilTargetInfo dt;
            Target *t = &sTargets[d->target];
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
            }
            SDL_zero(ct);
            SDL_zero(dt);
            ct.texture = t->color;
            ct.load_op = t->cleared ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
            ct.store_op = SDL_GPU_STOREOP_STORE;
            cts[1] = ct;
            cts[1].texture = t->aux;
            if (getenv("BT3_GPU_MAGENTA") != NULL) { ct.clear_color.r = 1.0f; ct.clear_color.b = 1.0f; ct.clear_color.a = 1.0f; }
            dt.texture = t->depth;
            dt.load_op = t->cleared ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
            dt.store_op = SDL_GPU_STOREOP_STORE;
            dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
            dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
            t->cleared = 1;
            cts[0] = ct;
            pass = SDL_BeginGPURenderPass(cmd, cts, 2, &dt);
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
            bound = d->vu != 0;
        }
        /* Only what changed since the previous draw is set again: most of a frame's 2,000 draws differ from their
           neighbour in one thing (the matrix, or the texture), and each call below costs time in the graphics
           driver (a texture binding makes the library write a new descriptor set). */
        if (d->vu && (int)d->uniform != lastUni) {
            SDL_PushGPUVertexUniformData(cmd, 0, &sVuUni[d->uniform], sizeof(Vu0Uniform));
            lastUni = (int)d->uniform;
        }
        if ((int)d->pipeline != lastPipe) {
            SDL_BindGPUGraphicsPipeline(pass, sPipes[d->pipeline].p);
            lastPipe = (int)d->pipeline;
        }
        if (d->tex != lastTex || (int)d->sampler != lastSampler) {
            SDL_GPUTextureSamplerBinding two[2];
            lastTex = d->tex;
            lastSampler = (int)d->sampler;
            two[0].texture = d->tex;
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
        if (!haveScissor || memcmp(&d->scissor, &lastScissor, sizeof(SDL_Rect)) != 0) {
            SDL_SetGPUScissor(pass, &d->scissor);
            lastScissor = d->scissor;
            haveScissor = 1;
        }
        SDL_DrawGPUPrimitives(pass, d->count, 1, d->first, 0);
    }
    if (pass != NULL) {
        SDL_EndGPURenderPass(pass);
    }
    /* Show the frame's own buffer: the one sceGsSwapDBuff set up for this frame. (Guessing "the buffer with the most
       draws" showed the shadow buffer, a grey fighter on black, on frames where it happened to receive more.) */
    {
        extern int gGsMainFbp;
        best = gGsMainFbp >= 0 ? target_get((uint32_t)gGsMainFbp, 0) : -1;
    }
    for (i = 0; best < 0 && i < sTargetCount; i++) {
        best = i;
    }
    sEndT[2] = gpu_now();
    acquired = SDL_WaitAndAcquireGPUSwapchainTexture(cmd, sWindow, &swap, &sw, &sh);
    sEndT[3] = gpu_now();
    if (acquired && swap != NULL && best >= 0 && sTargets[best].cleared) {
        SDL_GPUBlitInfo bl;
        SDL_zero(bl);
        bl.source.texture = sTargets[best].color;
        bl.source.w = 512 * SCALE;
        bl.source.h = 448 * SCALE;
        bl.destination.texture = swap;
        {
            /* The picture keeps its shape whatever the window's: 4:3, or 16:9 in widescreen, centred, the rest
               black. (The 512 x 448 buffer is not square-pixelled: it always fills a 4:3 or 16:9 screen.) */
            float want = (float)Port_AspectMilli() / 1000.0f;
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
    SDL_SubmitGPUCommandBuffer(cmd);
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
        if (every > 0 && gGsFrame % (unsigned)every == 0 && best >= 0 && sTargets[best].cleared &&
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
            char name[64];
            FILE *fp;

            SDL_zero(ti);
            ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
            ti.size = w * h * 4;
            tb = SDL_CreateGPUTransferBuffer(sDev, &ti);
            SDL_zero(src);
            src.texture = getenv("BT3_SHOT_AUX") != NULL ? sTargets[best].aux : sTargets[best].color;
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
            snprintf(name, sizeof(name), "port/build/shots/gpu_%05u.ppm", gGsFrame);
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
    for (i = 0; i < sTargetCount; i++) {
        sTargets[i].draws = 0;
    }
    sVertCount = 0;
    sVuVertCount = 0;
    sVuUniCount = 0;
    sDrawCount = 0;
    sAnchor = 0;
    sSkipped = 0;
    sNative = 0;
}
