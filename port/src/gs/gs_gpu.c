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
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gs_internal.h"
#include "gs_draw.h"
#include "shaders.h" /* generated: kGsVertSpv, kGsFragSpv */
#include "ui.h"

extern int Port_Setting(const char *name, int def); /* plat_settings.c: the saved settings */
extern int Port_AspectMilli(void);

static SDL_Window *sWindow; /* the shared window (GsDraw_WindowCreate) */
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
} Pipe;
static Pipe sPipes[1024];
static int sPipeCount;

/* textures created this frame, to upload in the copy pass */
#define MAX_PENDING 4096
static struct { SDL_GPUTexture *tex; uint32_t w, h; uint32_t *px; uint32_t bytes, level; /* bytes 0: w * h * 4 */ } sPending[MAX_PENDING];
static int sPendingCount;

static uint64_t gpu_now(void) { return SDL_GetTicksNS(); }

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

/* --- the attachments of the shared targets ------------------------------------------------------------ */

static void target_ensure(int i) {
    SDL_GPUTextureCreateInfo ci;
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = GS_W * SCALE;
    ci.height = GS_H * SCALE;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    sTgCol[i] = SDL_CreateGPUTexture(sDev, &ci);
    ci.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    sTgAux[i] = SDL_CreateGPUTexture(sDev, &ci);
    ci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER; /* the depth effects read it */
    sTgDep[i] = SDL_CreateGPUTexture(sDev, &ci);
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
static GsTex tex_create(uint32_t w, uint32_t h, int format, int levels) {
    SDL_GPUTextureCreateInfo ci;
    SDL_GPUTexture *tex;

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
    tex = SDL_CreateGPUTexture(sDev, &ci);
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
    sPendingCount++;
    return 1;
}

static void tex_destroy(GsTex tex) {
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
    SDL_ReleaseGPUTexture(sDev, sAuxCopy);
    SDL_ReleaseGPUTexture(sDev, sDateCopy);
    copies_create();
}

/* The settings window (F1, ui.cpp) and the shared window it lives in are gs_draw.c's: what the user changes
   there reaches this back end through gsScale, gsFxOff and gsGlowPercent. */

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
    b->enable_blend = true;
    b->src_color_blendfactor = f[0];
    b->dst_color_blendfactor = f[1];
    b->color_blend_op = k[0] == 3 ? SDL_GPU_BLENDOP_REVERSE_SUBTRACT : k[1] == 3 ? SDL_GPU_BLENDOP_SUBTRACT : SDL_GPU_BLENDOP_ADD;
    b->src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE; /* the frame buffer keeps the source alpha */
    b->dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    b->alpha_blend_op = SDL_GPU_BLENDOP_ADD;
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

/* The pipeline of a key gs_draw.c derived from GS state: cached here, created on first sight. */
static int pipe_get(uint32_t key) {
    int i;
    for (i = 0; i < sPipeCount; i++) {
        if (sPipes[i].key == key) {
            return i;
        }
    }
    return pipeline_create(key, 1);
}

/* --- init --------------------------------------------------------------------------------------- */

static int vk_init(void) {
    SDL_GPUBufferCreateInfo bi;
    SDL_GPUTransferBufferCreateInfo ti;
    static uint32_t white = 0xFFFFFFFFu;
    int i;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "bt3: SDL_Init: %s\n", SDL_GetError());
        return 0;
    }
    sDev = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, getenv("BT3_GPU_DEBUG") != NULL, NULL);
    sWindow = GsDraw_WindowCreate(0); /* the shared window: shape, display and full screen decided there */    if (sWindow == NULL || !SDL_ClaimWindowForGPUDevice(sDev, sWindow)) {
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
    sVu0Vs = shader(kVu0VertSpv, sizeof(kVu0VertSpv), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    sVu4Vs = shader(kVu4VertSpv, sizeof(kVu4VertSpv), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    sVu6Vs = shader(kVu6VertSpv, sizeof(kVu6VertSpv), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
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
    if (!Ui_Init(sWindow, sDev, NULL)) {
        fprintf(stderr, "bt3: the settings window could not be set up\n");
    }
    fprintf(stderr, "bt3: GPU renderer: %s\n", SDL_GetGPUDeviceDriver(sDev));
    fprintf(stderr, "bt3: %d logical processors, %d MB of memory\n", SDL_GetNumLogicalCPUCores(), SDL_GetSystemRAM());
    pipelines_preload();
    return 1;
}

/* --- the frame ---------------------------------------------------------------------------------- */

static void vk_frame_end(void) {
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
    int lastScissor[4];
    float lastBlend = -1.0f;
    uint32_t n;

    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) {
            exit(0);
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && (ev.key.key == SDLK_F1 || ev.key.key == SDLK_AC_BACK)) {
            Ui_Toggle(); /* (Android: the back button, which the loader asks SDL to hand over: android/.../loader.c) */
            continue;
        }
        if (Ui_Event(&ev)) { /* the settings window is open and used it (Esc closes it) */
            continue;
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
            exit(0);
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key == SDLK_F11) {
            GsDraw_FullscreenToggle();
        }
    }
#ifdef __ANDROID__
    if (gGsFrame % 300 == 0) { /* a heartbeat in the log (adb logcat -s bt3): is the game running, and drawing? */
        extern unsigned gPortVBlanks;
        fprintf(stderr, "bt3: alive: frame %u, game blank %u, %u draws, %u vertices, %d targets, %d textures\n", gGsFrame, gPortVBlanks,
                gsDrawCount, gsVertCount + gsVuVertCount, gsTargetCount, gsTexCount);
    }
#endif
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
    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
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
            if (!gsTargets[d->target].cleared) {
                continue;
            }
            cp = SDL_BeginGPUCopyPass(cmd);
            SDL_zero(from);
            SDL_zero(to);
            from.texture = sTgAux[d->target];
            to.texture = sAuxCopy;
            SDL_CopyGPUTextureToTexture(cp, &from, &to, GS_W * SCALE, GS_H * SCALE, 1, false);
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
            params[0] = 1.0f / (float)GS_W;
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
            dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
            dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
            gsTargets[d->target].cleared = 1;
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
        if ((int)d->pipeline != lastPipe) {
            SDL_BindGPUGraphicsPipeline(pass, sPipes[d->pipeline].p);
            lastPipe = (int)d->pipeline;
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
        if (d->vu) {
            SDL_DrawGPUIndexedPrimitives(pass, d->count, 1, d->first, 0, 0);
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
#ifdef __ANDROID__
    if (gGsFrame % 300 == 0) { /* (with the heartbeat: why a frame would not reach the screen) */
        fprintf(stderr, "bt3: present: acquired %d, swapchain %s %ux%u, main fbp %d, target %d, cleared %d, targets %d\n", (int)acquired,
                swap != NULL ? "yes" : "NO", (unsigned)sw, (unsigned)sh, (int)gGsMainFbp, best,
                best >= 0 ? (int)gsTargets[best].cleared : -1, gsTargetCount);
        if (!acquired) {
            fprintf(stderr, "bt3: present: %s\n", SDL_GetError());
        }
    }
#endif
#ifdef __ANDROID__
    /* (testing on Android: show the frame's buffer even when it was never cleared, so the settings window and
       whatever the game drew reach the screen; the log above says which condition fails) */
    if (acquired && swap != NULL && best >= 0) {
#else
    if (acquired && swap != NULL && best >= 0 && gsTargets[best].cleared) {
#endif
        SDL_GPUBlitInfo bl;
        SDL_zero(bl);
        bl.source.texture = sTgCol[best];
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
            char name[64];
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
};
