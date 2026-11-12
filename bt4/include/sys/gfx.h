#ifndef SYS_GFX_H
#define SYS_GFX_H

#include "types.h"
#include "sys/dma.h"

/* Frame buffer layout (GS page numbers; one page = 8192 pixels = 0x2000 words of VRAM). */
#define GFX_WIDTH 512
#define GFX_HEIGHT 448
#define GFX_FBW 8        /* frame buffer width in units of 64 pixels */
#define GFX_FBP_A 0x00   /* colour buffer drawn while the frame counter is odd */
#define GFX_FBP_B 0x70   /* colour buffer drawn while the frame counter is even */
#define GFX_ZBP 0xE0     /* depth buffer, PSMZ24, shared by both colour buffers */
#define GFX_OFX 0x7000   /* XYOFFSET: (2048 - 256) * 16 */
#define GFX_OFY 0x7200   /* XYOFFSET: (2048 - 224) * 16 */

#define GFX_DISPFB_A (GFX_FBP_A | (GFX_FBW << 9)) /* DISPFB value showing buffer A */
#define GFX_DISPFB_B (GFX_FBP_B | (GFX_FBW << 9))
#define GFX_CLEAR_RGBAQ 0x3F80000000000000 /* black, alpha 0, Q = 1.0f */

/* GS registers used here that sys/dma.h does not define. */
#define GS_PRMODECONT 0x1A
#define GS_SCANMSK 0x22
#define GS_TEXA 0x3B
#define GS_TEST_2 0x48
#define GS_ALPHA_2 0x43
#define GS_PABE 0x49
#define GS_FBA_2 0x4B
#define GS_DTHE 0x45
#define GS_ZBUF_2 0x4F

/* Sony libgraph structures (only the layout matters here; registers are kept as raw 64-bit values). */
typedef struct sceGsDispEnv {
    /* 0x00 */ u64 pmode;
    /* 0x08 */ u64 smode2;
    /* 0x10 */ u64 dispfb;
    /* 0x18 */ u64 display;
    /* 0x20 */ u64 bgcolor;
} sceGsDispEnv;

typedef struct sceGsDrawEnv1 {
    /* 0x00 */ u64 frame1;
    /* 0x08 */ u64 frame1addr;
    /* 0x10 */ u64 zbuf1;
    /* 0x18 */ u64 zbuf1addr;
    /* 0x20 */ u64 xyoffset1;
    /* 0x28 */ u64 xyoffset1addr;
    /* 0x30 */ u64 scissor1;
    /* 0x38 */ u64 scissor1addr;
    /* 0x40 */ u64 prmodecont;
    /* 0x48 */ u64 prmodecontaddr;
    /* 0x50 */ u64 colclamp;
    /* 0x58 */ u64 colclampaddr;
    /* 0x60 */ u64 dthe;
    /* 0x68 */ u64 dtheaddr;
    /* 0x70 */ u64 test1;
    /* 0x78 */ u64 test1addr;
} sceGsDrawEnv1;

/* Screen clear: a full-buffer sprite drawn with the depth test set to ALWAYS, then the test is restored. */
typedef struct sceGsClear {
    /* 0x00 */ u64 testa;
    /* 0x08 */ u64 testaaddr;
    /* 0x10 */ u64 prim;
    /* 0x18 */ u64 primaddr;
    /* 0x20 */ u64 rgbaq;
    /* 0x28 */ u64 rgbaqaddr;
    /* 0x30 */ u64 xyz2a;
    /* 0x38 */ u64 xyz2aaddr;
    /* 0x40 */ u64 xyz2b;
    /* 0x48 */ u64 xyz2baddr;
    /* 0x50 */ u64 testb;
    /* 0x58 */ u64 testbaddr;
} sceGsClear;

/* Double buffer: display + draw + clear settings of each of the two buffers. */
typedef struct sceGsDBuff {
    /* 0x000 */ sceGsDispEnv disp[2];
    /* 0x050 */ u64 giftag0[2];
    /* 0x060 */ sceGsDrawEnv1 draw0;
    /* 0x0E0 */ sceGsClear clear0;
    /* 0x140 */ u64 giftag1[2];
    /* 0x150 */ sceGsDrawEnv1 draw1;
    /* 0x1D0 */ sceGsClear clear1;
} sceGsDBuff; /* 0x230 */

/* gGfx (0x250 bytes, cleared by Gfx_Init). */
typedef struct Gfx {
    /* 0x000 */ sceGsDBuff db; /* 512x448 PSMCT32 double buffer; given to sceGsSwapDBuff every frame */
    /* 0x230 */ s32 field;     /* Sony samples' `odev`: !sceGsSyncV(0) at init, then flipped by every Gfx_EndFrame whatever the
                                  vsync count, so really a frame parity. Read by the movie player (0x125E4C); buffers use `frame` */
    /* 0x234 */ s32 frame;     /* frames ended so far; bit 0 picks the colour buffer (see GFX_FRAME_FBP) */
    /* 0x238 */ u8 unk238[0x18]; /* never referenced */
} Gfx;

extern Gfx gGfx;

/* Page of the colour buffer the display list being built must draw into (and that is on screen meanwhile). */
#define GFX_FRAME_FBP() ((gGfx.frame & 1) ? GFX_FBP_A : GFX_FBP_B)
/* The same as a GS FRAME register value (PSMCT32, no write mask). */
#define GFX_FRAME_REG() \
    (!(gGfx.frame & 1) ? GS_SET_FRAME(GFX_FBP_B, GFX_FBW, 0, 0) : GS_SET_FRAME(GFX_FBP_A, GFX_FBW, 0, 0))

/* libgraph */
void sceGsResetGraph(s16 mode, s16 inter, s16 omode, s16 ffmode);
void sceGsResetPath(void);
s32 sceGsSetDefDBuff(sceGsDBuff *db, s16 psm, s16 w, s16 h, s16 ztest, s16 zpsm, s16 clear);
void sceGsSwapDBuff(sceGsDBuff *db, s32 id);
s32 sceGsSyncV(s32 mode);
s32 sceGsSyncPath(s32 mode, u16 timeout);
s32 sceGsPutDrawEnv(u64 *giftag);

void Gfx_InitDrawEnv(void);
void Gfx_SyncField(void);
void Gfx_SetDisplayRegs(s32 field, s32 blend);
void Gfx_Init(void);
void Gfx_BeginFrame(void);
void Gfx_EndFrame(s32 vsyncs);
s32 Gfx_Stub102118(void);
void Gfx_ClearScreen(s32 fbmsk, u32 rgba);
void Gfx_AddDefaultEnv(void);
void Gfx_PutDefaultEnv(GsQword **pp);
void Gfx_Stub102438(void);
void Gfx_Stub102440(void);
void Gfx_MarkPass(s32 pass);
void Gfx_Stub102450(void);

#endif
