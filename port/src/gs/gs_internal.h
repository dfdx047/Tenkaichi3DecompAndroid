/* Shared between the GS front end (gs_core.c: DMA chain -> VIF1 -> GIF -> registers, GS memory, software
   rasteriser) and the GPU back end (gs_gpu.c). */
#ifndef GS_INTERNAL_H
#define GS_INTERNAL_H
#include <stdint.h>

typedef struct GsVertex {
    float x, y;       /* frame-buffer pixels */
    uint32_t z;
    float s, t, q;    /* texture coordinates when PRIM.FST = 0 */
    int u, v;         /* texel coordinates, 12.4, when PRIM.FST = 1 */
    uint8_t r, g, b, a;
} GsVertex;

typedef struct GsState {
    uint64_t prim;    /* the EFFECTIVE primitive attributes: PRIM, or PRIM's type with PRMODE's attributes (PRMODECONT) */
    uint64_t primRaw; /* PRIM as written */
    uint64_t rgbaq, st, uv, texa, prmodecont, prmode;
    uint64_t tex0[2], tex1[2], clamp[2], xyoffset[2], scissor[2], alpha[2], test[2], frame[2], zbuf[2];
    uint64_t fba[2]; /* FBA: 1 = every pixel written gets alpha bit 7 set (the masks of the HUD bars) */
    uint64_t bitbltbuf, trxpos, trxreg, trxdir;
    float q;
    GsVertex vtx[3];
    int vcount; /* vertices in the queue */
    int strip;  /* vertices seen since the last PRIM write */
    /* host -> GS transfer in progress */
    int transfer;
    uint32_t tx, ty, tw, th, tpos;
} GsState;

extern GsState gGs;
extern uint32_t gGsPageGen[512]; /* bumped whenever an upload writes into that 8 KB page of GS memory */
extern unsigned gGsFrame;

uint32_t Gs_PageHash(uint32_t page); /* content hash of an 8 KB page of GS memory */
uint32_t Gs_VramRead(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y);
/* The 256-byte block of GS memory that holds the pixel (x, y) of that buffer (formats 0x00, 0x13, 0x14). */
const uint8_t *Gs_BlockPtr(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y);
extern unsigned gGsFbUploads;
void GsGpu_FbUpload(int second);
int Gs_PsmBits(uint32_t psm);
uint32_t Gs_Expand(uint32_t c, uint32_t psm); /* stored colour -> R | G << 8 | B << 16 | A << 24 */

void Gs_Gif(const uint8_t *p, uint32_t qwc); /* GIF data for the GS (from VIF1 DIRECT or a VU1 kick) */

/* VU1 (gs_vu1.c): what VIF1 does to it, and running its programs. */
void GsVu1_SetCycle(uint32_t cl, uint32_t wl);
void GsVu1_SetBase(uint32_t v);
void GsVu1_SetOffset(uint32_t v);
void GsVu1_SetItop(uint32_t v);
void GsVu1_SetMode(uint32_t v);
void GsVu1_SetMask(uint32_t v);
void GsVu1_SetRow(const uint32_t *v);
void GsVu1_SetCol(const uint32_t *v);
void GsVu1_Program(uint32_t addr, const uint32_t *words, uint32_t count);
uint32_t GsVu1_UnpackWords(uint32_t cmd, uint32_t num);
void GsVu1_Unpack(uint32_t cmd, uint32_t num, uint32_t imm, const uint32_t *data);
void GsVu1_Call(int addr);
void GsVu1_FrameEnd(void);

/* GPU back end (gs_gpu.c). Primitive = PRIM type: 0 point, 1 line, 3 triangle, 6 sprite (strips and fans arrive
   as single lines / triangles). The state to draw with is gGs. */
int GsGpu_Init(void);
void GsGpu_Draw(int type, int ctx, const GsVertex *v);
/* A vertex program run as a shader: `count` strip vertices of 48 bytes (position + weight, normal, texture
   coordinates) with the program's constants (VU memory quadwords 0..23); layer 0 or 1. The GS state is gGs. */
int GsGpu_Enabled(void);
void GsGpu_DrawVu0(int layer, int ctx, const float *vertices, uint32_t count, const float *consts);
void GsGpu_DrawVu4(int ctx, const float *vertices, uint32_t count, const float *consts);
void GsGpu_DrawVu6(int ctx, const float *vertices, uint32_t count, const float *consts);
void Gs_RegWrite(uint32_t addr, uint64_t d);
void GsGpu_Native(int effect); /* marker register 0x7F: a native effect goes here */
void GsGpu_FrameEnd(void);
/* The game is re-running frames it has already shown (online play after a late input; the save / restore test):
   nothing of them is to be seen or heard. The frame's list is still walked, because it also carries the texture
   uploads and vertex programs that later frames count on, but no draw is recorded, nothing is shown, and no
   sound starts. (gs/state.c) */
extern int gPortResim;

/* Settings are read from the environment all over the renderer, some of them for every batch a model sends
   (tens of thousands of times per frame). The C library's getenv is cheap on Linux and slow on Windows (a lock
   and a case-insensitive search of a much larger environment): there it cost more than half of a frame. In the
   renderer's files getenv is this instead: the answer for a name is looked up once and remembered. (Nothing here
   changes the environment while the game runs.) */
#include <stdlib.h>
const char *Port_GetEnv(const char *name);
#define getenv(name) Port_GetEnv(name)

#endif
