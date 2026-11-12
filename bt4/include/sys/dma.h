#ifndef SYS_DMA_H
#define SYS_DMA_H

#include "types.h"

#ifdef PORT
/* PC build: no 128-bit integer on a 32-bit host; the game only copies and stores these. */
typedef struct { u64 lo, hi; } __attribute__((aligned(16))) s128;
typedef struct { u64 lo, hi; } __attribute__((aligned(16))) u128;
#else
typedef int s128 __attribute__((mode(TI)));
typedef unsigned int u128 __attribute__((mode(TI)));
#endif

#define DMA_BUF_SIZE 0x100000

/* Source-chain DMA tag ids (bits 28-30 of the first word; the low 16 bits are the quadword count). */
#define DMA_TAG_CNT 0x10000000  /* data follows the tag, next tag after the data */
#define DMA_TAG_NEXT 0x20000000 /* data follows the tag, next tag at the tag's address */
#define DMA_TAG_REF 0x30000000  /* data at the tag's address, next tag after this one */
#define DMA_TAG_END 0x70000000  /* data follows the tag, then the transfer ends */

/* VIF codes carried in the upper half of a DMA tag. */
#define VIF_FLUSHE 0x10000000
#define VIF_DIRECT 0x50000000 /* low 16 bits: number of quadwords sent on to the GIF */

/* GIF tag pieces. */
#define GIF_EOP 0x8000
#define GIF_FLG_PACKED 0
#define GIF_FLG_REGLIST 1
/* First doubleword of a GIF tag: loop count, end-of-packet flag, data format and registers per loop (PRE/PRIM unused). */
#define GIF_TAG_EX(nloop, eop, flg, nreg) \
    ((u64)(nloop) | ((u64)(eop) << 15) | ((u64)(flg) << 58) | ((u64)(nreg) << 60))
#define GIF_TAG(nloop, eop, nreg) GIF_TAG_EX(nloop, eop, GIF_FLG_PACKED, nreg)
#define GS_REG_XYZ2 0x5 /* register descriptor: vertex position, draws */
#define GIF_REG_AD 0xE /* register descriptor "A+D": each quadword is {value, register number} */

/* GS registers written through A+D packets here. */
#define GS_PRIM 0x00
#define GS_RGBAQ 0x01
#define GS_TEX0_1 0x06
#define GS_CLAMP_1 0x08
#define GS_CLAMP_2 0x09
#define GS_TEX1_1 0x14
#define GS_XYOFFSET_1 0x18
#define GS_XYOFFSET_2 0x19
#define GS_TEXFLUSH 0x3F
#define GS_SCISSOR_1 0x40
#define GS_SCISSOR_2 0x41
#define GS_ALPHA_1 0x42
#define GS_COLCLAMP 0x46
#define GS_TEST_1 0x47
#define GS_FBA_1 0x4A
#define GS_FRAME_1 0x4C
#define GS_FRAME_2 0x4D
#define GS_ZBUF_1 0x4E
#define GS_FINISH 0x61

/* GS register values (same layouts as Sony's SCE_GS_SET_* macros). */
#define GS_SET_FRAME(fbp, fbw, psm, fbmsk) \
    ((u64)(fbp) | ((u64)(fbw) << 16) | ((u64)(psm) << 24) | ((u64)(fbmsk) << 32))
#define GS_SET_ZBUF(zbp, psm, zmsk) ((u64)(zbp) | ((u64)(psm) << 24) | ((u64)(zmsk) << 32))
#define GS_SET_XYOFFSET(ofx, ofy) ((u64)(ofx) | ((u64)(ofy) << 32))
#define GS_SET_SCISSOR(x0, x1, y0, y1) ((u64)(x0) | ((u64)(x1) << 16) | ((u64)(y0) << 32) | ((u64)(y1) << 48))
#define GS_SET_CLAMP(wms, wmt, minu, maxu, minv, maxv) \
    ((u64)(wms) | ((u64)(wmt) << 2) | ((u64)(minu) << 4) | ((u64)(maxu) << 14) | ((u64)(minv) << 24) | \
     ((u64)(maxv) << 34))
#define GS_SET_XYZ(x, y, z) ((u64)(x) | ((u64)(y) << 16) | ((u64)(z) << 32))
#define GS_SET_UV(u, v) ((u64)(u) | ((u64)(v) << 16))
#define GS_PSMZ24 0x31

/* EE hardware registers. */
#ifdef PORT
extern volatile u32 *Port_DmaChcr(s32 channel);
#endif
/* What goes into a DMA address register: the physical address on the PS2 (28 bits); on PC the pointer itself
   (a PC stack or global address does not fit in 28 bits). */
#ifdef PORT
#define DMA_PHYS(a) (a)
#else
#define DMA_PHYS(a) ((a) & 0x0FFFFFFF)
#endif
#define VIF1_STAT ((volatile u32 *)0x10003C00)
#define GIF_STAT ((volatile u32 *)0x10003020)
#ifdef PORT /* PC build: every access first completes a transfer that was started (port/src/plat_gs.c) */
#define D1_CHCR Port_DmaChcr(1)
#else
#define D1_CHCR ((volatile u32 *)0x10009000)
#endif
#define D1_QWC ((volatile u32 *)0x10009020)
#define D1_TADR ((volatile u32 *)0x10009030)
#ifdef PORT /* PC build: every access first completes a transfer that was started (port/src/plat_gs.c) */
#define D2_CHCR Port_DmaChcr(2)
#else
#define D2_CHCR ((volatile u32 *)0x1000A000)
#endif
#define D2_MADR ((volatile u32 *)0x1000A010)
#define D2_QWC ((volatile u32 *)0x1000A020)
#define D2_TADR ((volatile u32 *)0x1000A030)
#define D_CTRL ((volatile u32 *)0x1000E000)
#define D_STAT ((volatile u32 *)0x1000E010)
#define D_PCR ((volatile u32 *)0x1000E020)

/* One quadword of GS packet data seen as a whole, as two doublewords or as four words. */
typedef union GsQword {
    u128 q;
    u64 d[2];
    u32 w[4];
} GsQword;

/* Argument of sceSifSetDma (Sony sceSifDmaData). */
typedef struct SifDmaData {
    /* 0x00 */ void *data; /* EE address */
    /* 0x04 */ void *addr; /* IOP address */
    /* 0x08 */ s32 size;
    /* 0x0C */ s32 mode;
} SifDmaData;

void Dma_InitBuffers(void);
void Dma_AddData(void *src, s32 size);
void Dma_AddRef(void *addr, s32 size);
void Dma_AddNext(void *addr, u32 *retTag);
void Dma_Flush(void);
void *Dma_Alloc(s32 size);
u64 *Dma_BeginDirect(void);
void Dma_EndDirect(u64 *end);
void Dma_ResetBuffers(void);
s32 Dma_GetBufSize(void);
s32 Dma_GetUsedSize(void);
s32 Dma_GetFreeSize(void);
void Dma_LendBufferTails(u32 *tail0, u32 *tail1, s32 *size);
void Dma_ReclaimBufferTails(void);
void Dma_InitController(void);
void Dma_WaitCond(void);
void Dma_WaitVu1(void);
void Dma_SendVif1(u32 chain);
void Dma_SendVif1Sync(u32 chain);
void Dma_SendVif1Spr(u32 chain);
void Dma_SendVif1SprSync(u32 chain);
void Dma_SendGifChain(u32 chain);
void Dma_SendGifChainSync(u32 chain);
void Dma_SendGifNoWait(u32 addr, u32 qwc);
void Dma_SendGif(u32 addr, u32 qwc);
void Dma_WaitGif(void);
void Dma_SendToIop(void *iopAddr, void *data, s32 size);
void Dma_AddTexFlush(void);
void Dma_AddFrame(s32 fbp, s32 fbw, s32 fbmsk);
void Dma_AddClamp(s32 wms, s32 wmt, s32 minu, s32 maxu, s32 minv, s32 maxv);
void Dma_AddScissor(s32 x0, s32 x1, s32 y0, s32 y1);
void Dma_AddTex0(u64 tex0);
void Dma_AddTex1(u64 tex1);
void Dma_AddColClamp(u64 clamp);
void Dma_AddAlpha(u64 alpha);
void Dma_AddXyOffset(s32 x, s32 y);
void Dma_AddZbuf(s32 zbp, s32 zmsk);
void Dma_AddFba(s32 fba);
s32 Dma_Stub101870(void);
void Dma_AddFillRect(s32 x, s32 y, s32 w, s32 h, u32 rgba);
void Dma_PutTexStrips(GsQword **pp, s32 x0, s32 y0, s32 x1, s32 y1, s32 xFrac, s32 yFrac, s32 u0, s32 v0, s32 u1, s32 v1,
                      s32 uFrac, s32 vFrac, u32 rgba, s32 blend);

#endif
