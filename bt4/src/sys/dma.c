#include "common.h"
#include "sys/dma.h"
#include "sys/heap.h"

/* libc */
extern void *memcpy(void *dst, const void *src, u32 size);

/* Sony kernel */
extern void FlushCache(s32 mode);
extern u32 sceSifSetDma(SifDmaData *data, s32 count);
extern s32 sceSifDmaStat(u32 id);

extern u32 gDmaBuf[2];    /* addresses of the two buffers */
extern u32 gDmaBufEnd[2]; /* address just past each buffer */
extern s32 gDmaBufSize;
extern s32 gDmaBufIdx;
extern u32 *gDmaCur;
extern s32 gDmaVif1Started;

/* Allocates the two 1 MB display-list buffers and points the write cursor at the first. */
void Dma_InitBuffers(void) {
    gDmaBuf[0] = (u32)Heap_Alloc(DMA_BUF_SIZE, 0x20, 0, HEAP_ANY);
    gDmaBuf[1] = (u32)Heap_Alloc(DMA_BUF_SIZE, 0x20, 0, HEAP_ANY);
    gDmaBufEnd[0] = gDmaBuf[0] + DMA_BUF_SIZE;
    gDmaBufEnd[1] = gDmaBuf[1] + DMA_BUF_SIZE;
    gDmaBufSize = DMA_BUF_SIZE;
    gDmaCur = (u32 *)gDmaBuf[0];
    gDmaBufIdx = 0;
}

/* Copies `size` bytes (whole quadwords) of ready-made packet data to the display list. */
void Dma_AddData(void *src, s32 size) {
    memcpy(gDmaCur, src, size);
    gDmaCur = (u32 *)((u8 *)gDmaCur + size / 16 * 16);
}

/* Appends a REF tag: sends `size` bytes that live at `addr` without copying them. */
void Dma_AddRef(void *addr, s32 size) {
    u32 *tag = gDmaCur;

    tag[0] = DMA_TAG_REF + size / 16;
    tag[1] = (u32)addr;
    tag[2] = 0;
    tag[3] = 0;
    tag += 4;
    gDmaCur = tag;
}

/* Appends a NEXT tag that jumps to the chain at `addr`, and makes `retTag` point back to what follows. */
void Dma_AddNext(void *addr, u32 *retTag) {
    u32 *tag = gDmaCur;

    tag[0] = DMA_TAG_NEXT;
    tag[1] = (u32)addr;
    tag[2] = 0;
    tag[3] = 0;
    tag += 4;
    retTag[1] = (u32)tag;
    gDmaCur = tag;
}

/* Closes the display list with a GS FINISH and an END tag, sends it on VIF1 and switches to the other buffer. */
void Dma_Flush(void) {
    u32 *p = gDmaCur;

    p[0] = DMA_TAG_CNT | 2;
    p[1] = 0;
    p[2] = VIF_FLUSHE;
    p[3] = VIF_DIRECT | 2;
    p += 4;
    ((u64 *)p)[0] = GIF_TAG(1, 1, 1);
    ((u64 *)p)[1] = GIF_REG_AD;
    p += 4;
    ((u64 *)p)[0] = 0;
    ((u64 *)p)[1] = GS_FINISH;
    p += 4;
    p[0] = DMA_TAG_END;
    p[1] = 0;
    p[2] = 0;
    p[3] = 0;
    Dma_SendVif1(gDmaBuf[gDmaBufIdx]);
    gDmaBufIdx ^= 1;
    gDmaCur = (u32 *)gDmaBuf[gDmaBufIdx];
}

/* Reserves `size` bytes (whole quadwords) in the display list and returns where they start. */
void *Dma_Alloc(s32 size) {
    u32 *p = gDmaCur;

    gDmaCur = (u32 *)((u8 *)gDmaCur + size / 16 * 16);
    return p;
}

/* Starts a GS packet: clears the quadword that will hold its DMA tag and returns the first data quadword. */
u64 *Dma_BeginDirect(void) {
    u128 *tag = (u128 *)gDmaCur;

#ifdef PORT
    tag->lo = 0;
    tag->hi = 0;
#else
    *tag = 0;
#endif
    return (u64 *)(tag + 1);
}

/* Ends a packet started by Dma_BeginDirect: writes its CNT tag + VIF DIRECT code and moves the cursor to `end`. */
void Dma_EndDirect(u64 *end) {
    u32 *tag = gDmaCur;
    s32 qwc;

    if ((u32 *)end != tag + 4) {
        qwc = (((u8 *)end - (u8 *)tag) >> 4) - 1;
        tag[0] = DMA_TAG_CNT | qwc;
        tag[1] = 0;
        tag[2] = VIF_FLUSHE;
        tag[3] = VIF_DIRECT | qwc;
        FlushCache(0);
        gDmaCur = (u32 *)end;
    }
}

/* Makes both buffers an empty list (a lone END tag) and rewinds the cursor to the current one. */
void Dma_ResetBuffers(void) {
    ((u32 *)gDmaBuf[0])[0] = DMA_TAG_END;
    ((u32 *)gDmaBuf[0])[1] = 0;
    ((u32 *)gDmaBuf[0])[2] = 0;
    ((u32 *)gDmaBuf[0])[3] = 0;
    ((u32 *)gDmaBuf[1])[0] = DMA_TAG_END;
    ((u32 *)gDmaBuf[1])[1] = 0;
    ((u32 *)gDmaBuf[1])[2] = 0;
    ((u32 *)gDmaBuf[1])[3] = 0;
    gDmaCur = (u32 *)gDmaBuf[gDmaBufIdx];
}

/* Returns the usable size of one display-list buffer. */
s32 Dma_GetBufSize(void) {
    return gDmaBufSize;
}

/* Returns how many bytes of the current buffer are filled. */
s32 Dma_GetUsedSize(void) {
    return (u32)gDmaCur - gDmaBuf[gDmaBufIdx];
}

/* Returns how many bytes are left in the current buffer. */
s32 Dma_GetFreeSize(void) {
    return Dma_GetBufSize() - Dma_GetUsedSize();
}

/* Shrinks both buffers to 0x51000 bytes and hands their upper 0xAF000 bytes to the caller. */
void Dma_LendBufferTails(u32 *tail0, u32 *tail1, s32 *size) {
    gDmaBufSize = 0x51000;
    gDmaBufEnd[0] = gDmaBuf[0] + 0x51000;
    gDmaBufEnd[1] = gDmaBuf[1] + 0x51000;
    *tail0 = gDmaBufEnd[0];
    *tail1 = gDmaBufEnd[1];
    *size = DMA_BUF_SIZE - 0x51000;
}

/* Gives the buffers their full 1 MB back. */
void Dma_ReclaimBufferTails(void) {
    gDmaBufSize = DMA_BUF_SIZE;
    gDmaBufEnd[0] = gDmaBuf[0] + DMA_BUF_SIZE;
    gDmaBufEnd[1] = gDmaBuf[1] + DMA_BUF_SIZE;
}

/* Enables the DMA controller, sets the channel condition/interrupt mask and clears the status. */
void Dma_InitController(void) {
    *D_CTRL = 1;
    *D_PCR = 0x83FF0002;
    *D_STAT = 0;
}

/* Spins until the DMA condition flag (CPCOND0: the channels selected in D_PCR have finished) is set. */
void Dma_WaitCond(void) {
#ifndef PORT /* PC build: nothing to wait for */
    __asm__ volatile(
        ".set noreorder\n"
        "0:\n"
        "nop\nnop\nnop\nnop\nnop\n"
        "bc0f 0b\n"
        "nop\n"
        ".set reorder\n");
#endif
}

/* Spins while the COP2 condition flag is set (VU1 still running a micro program). */
void Dma_WaitVu1(void) {
#ifndef PORT /* PC build: nothing to wait for */
    __asm__ volatile(
        ".set noreorder\n"
        "0:\n"
        "nop\nnop\nnop\nnop\nnop\n"
        "bc2t 0b\n"
        "nop\n"
        ".set reorder\n");
#endif
}

/* Starts a source-chain transfer of `chain` on DMA channel 1 (VIF1) once VIF1, VU1 and the GIF path are idle. */
void Dma_SendVif1(u32 chain) {
    if (gDmaVif1Started) {
        Dma_WaitCond();
        gDmaVif1Started = 1;
    }
    while (*VIF1_STAT & 0x1F000003) {
    }
    Dma_WaitVu1();
    while (*GIF_STAT & 0xC00) {
    }
    *D1_QWC = 0;
    *D1_TADR = DMA_PHYS(chain);
    *D_STAT = 2;
    FlushCache(0);
    *D1_CHCR = 0x145;
}

/* Like Dma_SendVif1, but always waits for the previous transfer first and for this one to finish. */
void Dma_SendVif1Sync(u32 chain) {
    Dma_WaitCond();
    while (*VIF1_STAT & 0x1F000003) {
    }
    Dma_WaitVu1();
    while (*GIF_STAT & 0xC00) {
    }
    *D1_QWC = 0;
    *D1_TADR = DMA_PHYS(chain);
    *D_STAT = 2;
    FlushCache(0);
    *D1_CHCR = 0x145;
    Dma_WaitCond();
    while (*VIF1_STAT & 0x1F000003) {
    }
    Dma_WaitVu1();
    while (*GIF_STAT & 0xC00) {
    }
}

/* Starts a VIF1 chain transfer of a list that lives in the scratchpad (address bit 31 set). */
void Dma_SendVif1Spr(u32 chain) {
    Dma_WaitCond();
    while (*VIF1_STAT & 0x1F000003) {
    }
    Dma_WaitVu1();
    while (*GIF_STAT & 0xC00) {
    }
    *D1_QWC = 0;
    *D1_TADR = chain | 0x80000000;
    *D_STAT = 2;
    FlushCache(0);
    *D1_CHCR = 0x145;
}

/* Dma_SendVif1Spr, then waits for the transfer to finish. */
void Dma_SendVif1SprSync(u32 chain) {
    Dma_WaitCond();
    while (*VIF1_STAT & 0x1F000003) {
    }
    Dma_WaitVu1();
    while (*GIF_STAT & 0xC00) {
    }
    *D1_QWC = 0;
    *D1_TADR = chain | 0x80000000;
    *D_STAT = 2;
    FlushCache(0);
    *D1_CHCR = 0x145;
    Dma_WaitCond();
    while (*VIF1_STAT & 0x1F000003) {
    }
    Dma_WaitVu1();
    while (*GIF_STAT & 0xC00) {
    }
}

/* Starts a source-chain transfer of `chain` on DMA channel 2 (GIF) once the channel and the GIF path are idle. */
void Dma_SendGifChain(u32 chain) {
    do {
        while (*D2_CHCR & 0x100) {
        }
    } while (*GIF_STAT & 0xC00);
    *D2_QWC = 0;
    *D2_TADR = DMA_PHYS(chain);
    *D_STAT = 4;
    FlushCache(0);
    *D2_CHCR = 0x145;
}

/* Dma_SendGifChain, then waits for the transfer to finish. */
void Dma_SendGifChainSync(u32 chain) {
    do {
        while (*D2_CHCR & 0x100) {
        }
    } while (*GIF_STAT & 0xC00);
    *D2_QWC = 0;
    *D2_TADR = DMA_PHYS(chain);
    *D_STAT = 4;
    FlushCache(0);
    *D2_CHCR = 0x145;
    do {
        while (*D2_CHCR & 0x100) {
        }
    } while (*GIF_STAT & 0xC00);
}

/* Starts a plain (normal mode) transfer of `qwc` quadwords at `addr` on the GIF channel. */
void Dma_SendGifNoWait(u32 addr, u32 qwc) {
    do {
        while (*D2_CHCR & 0x100) {
        }
    } while (*GIF_STAT & 0xC00);
    *D2_QWC = qwc;
    *D2_MADR = DMA_PHYS(addr);
    FlushCache(0);
    *D2_CHCR = 0x101;
}

/* Sends `qwc` quadwords at `addr` on the GIF channel and waits for the transfer to finish. */
void Dma_SendGif(u32 addr, u32 qwc) {
    do {
        while (*D2_CHCR & 0x100) {
        }
    } while (*GIF_STAT & 0xC00);
    *D2_QWC = qwc;
    *D2_MADR = DMA_PHYS(addr);
    FlushCache(0);
    *D2_CHCR = 0x101;
    do {
        while (*D2_CHCR & 0x100) {
        }
    } while (*GIF_STAT & 0xC00);
}

/* Waits until the GIF channel and the GIF path are idle. */
void Dma_WaitGif(void) {
    do {
        while (*D2_CHCR & 0x100) {
        }
    } while (*GIF_STAT & 0xC00);
}

/* Copies `size` bytes at `data` to IOP memory at `iopAddr` over SIF DMA and waits for completion. */
void Dma_SendToIop(void *iopAddr, void *data, s32 size) {
    SifDmaData dma;
    u32 id;

    dma.addr = iopAddr;
    dma.data = data;
    dma.size = size;
    dma.mode = 0;
    FlushCache(0);
    id = sceSifSetDma(&dma, 1);
    while (sceSifDmaStat(id) >= 0) {
    }
}

/* Queues a GS TEXFLUSH (wait for texture writes before the next primitive). */
void Dma_AddTexFlush(void) {
    u32 pkt[12] = {
        DMA_TAG_CNT | 2, 0, VIF_FLUSHE, VIF_DIRECT | 2,
        GIF_EOP | 1, 0x10000000, GIF_REG_AD, 0,
        0, 0, GS_TEXFLUSH, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues GS FRAME_1 and FRAME_2: frame buffer page, width (in 64-pixel units) and write mask; pixel format 0 (32-bit). */
void Dma_AddFrame(s32 fbp, s32 fbw, s32 fbmsk) {
    u64 frame = GS_SET_FRAME(fbp, fbw, 0, fbmsk);
    u32 pkt[16] = {
        DMA_TAG_CNT | 3, 0, VIF_FLUSHE, VIF_DIRECT | 3,
        GIF_EOP | 2, 0x10000000, GIF_REG_AD, 0,
        frame, frame >> 32, GS_FRAME_1, 0,
        frame, frame >> 32, GS_FRAME_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues GS CLAMP_1 and CLAMP_2: texture wrap modes and the clamp/repeat region. */
void Dma_AddClamp(s32 wms, s32 wmt, s32 minu, s32 maxu, s32 minv, s32 maxv) {
    u64 clamp = GS_SET_CLAMP(wms, wmt, minu, maxu, minv, maxv);
    u32 pkt[16] = {
        DMA_TAG_CNT | 3, 0, VIF_FLUSHE, VIF_DIRECT | 3,
        GIF_EOP | 2, 0x10000000, GIF_REG_AD, 0,
        clamp, clamp >> 32, GS_CLAMP_1, 0,
        clamp, clamp >> 32, GS_CLAMP_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues GS SCISSOR_1 and SCISSOR_2: the drawing window in pixels. */
void Dma_AddScissor(s32 x0, s32 x1, s32 y0, s32 y1) {
    u64 scissor = GS_SET_SCISSOR(x0, x1, y0, y1);
    u32 pkt[16] = {
        DMA_TAG_CNT | 3, 0, VIF_FLUSHE, VIF_DIRECT | 3,
        GIF_EOP | 2, 0x10000000, GIF_REG_AD, 0,
        scissor, scissor >> 32, GS_SCISSOR_1, 0,
        scissor, scissor >> 32, GS_SCISSOR_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues GS XYOFFSET_1 and XYOFFSET_2: the primitive coordinate offset, given in pixels. */
void Dma_AddXyOffset(s32 x, s32 y) {
    u32 pkt[16] = {
        DMA_TAG_CNT | 3, 0, VIF_FLUSHE, VIF_DIRECT | 3,
        GIF_EOP | 2, 0x10000000, GIF_REG_AD, 0,
        GS_SET_XYOFFSET(x << 4, y << 4), GS_SET_XYOFFSET(x << 4, y << 4) >> 32, GS_XYOFFSET_1, 0,
        GS_SET_XYOFFSET(x << 4, y << 4), GS_SET_XYOFFSET(x << 4, y << 4) >> 32, GS_XYOFFSET_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues GS ZBUF_1: depth buffer page (24-bit format) and the "do not write depth" flag. */
void Dma_AddZbuf(s32 zbp, s32 zmsk) {
    u32 pkt[12] = {
        DMA_TAG_CNT | 2, 0, VIF_FLUSHE, VIF_DIRECT | 2,
        GIF_EOP | 1, 0x10000000, GIF_REG_AD, 0,
        GS_SET_ZBUF(zbp, GS_PSMZ24, zmsk), GS_SET_ZBUF(zbp, GS_PSMZ24, zmsk) >> 32, GS_ZBUF_1, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues GS TEX0_1 with a ready-made register value. */
void Dma_AddTex0(u64 tex0) {
    u32 pkt[12] = {
        DMA_TAG_CNT | 2, 0, VIF_FLUSHE, VIF_DIRECT | 2,
        GIF_EOP | 1, 0x10000000, GIF_REG_AD, 0,
        tex0, tex0 >> 32, GS_TEX0_1, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues GS TEX1_1 with a ready-made register value. */
void Dma_AddTex1(u64 tex1) {
    u32 pkt[12] = {
        DMA_TAG_CNT | 2, 0, VIF_FLUSHE, VIF_DIRECT | 2,
        GIF_EOP | 1, 0x10000000, GIF_REG_AD, 0,
        tex1, tex1 >> 32, GS_TEX1_1, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues GS FBA_1 (forces the alpha bit of written pixels). */
void Dma_AddFba(s32 fba) {
    u32 pkt[12] = {
        DMA_TAG_CNT | 2, 0, VIF_FLUSHE, VIF_DIRECT | 2,
        GIF_EOP | 1, 0x10000000, GIF_REG_AD, 0,
        (u64)fba, (u64)fba >> 32, GS_FBA_1, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues GS COLCLAMP (clamp or wrap colour after blending). */
void Dma_AddColClamp(u64 clamp) {
    u32 pkt[12] = {
        DMA_TAG_CNT | 2, 0, VIF_FLUSHE, VIF_DIRECT | 2,
        GIF_EOP | 1, 0x10000000, GIF_REG_AD, 0,
        clamp, clamp >> 32, GS_COLCLAMP, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues GS ALPHA_1 (blend equation) with a ready-made register value. */
void Dma_AddAlpha(u64 alpha) {
    u32 pkt[12] = {
        DMA_TAG_CNT | 2, 0, 0, VIF_DIRECT | 2,
        GIF_EOP | 1, 0x10000000, GIF_REG_AD, 0,
        0, 0, GS_ALPHA_1, 0,
    };

    pkt[8] = alpha;
    pkt[9] = alpha >> 32;
    Dma_AddData(pkt, sizeof(pkt));
}

/* Unused stub that returns 0. */
s32 Dma_Stub101870(void) {
    return 0;
}

/* Queues a flat-colour rectangle drawn as 32-pixel-wide sprites (depth test off, plain alpha blend). */
void Dma_AddFillRect(s32 x, s32 y, s32 w, s32 h, u32 rgba) {
    s32 n = w / 32;
    u64 *p = Dma_BeginDirect();
    u64 *tag;
    s32 i;

    p[0] = GIF_TAG(5, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x44;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = 0x46;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = (u64)rgba | (0x3F800000ULL << 32);
    p[1] = GS_RGBAQ;
    p += 2;
    tag = p;
    p += 2;
    for (i = 0; i < n; i++) {
        p[0] = GS_SET_XYZ((x << 4) + (i * 32 << 4), y << 4, 0);
        p[1] = GS_SET_XYZ((x << 4) + ((i * 32 + 32) << 4), (y + h) << 4, 0);
        p += 2;
    }
    tag[0] = GIF_TAG_EX(n, 1, GIF_FLG_REGLIST, 2);
    tag[1] = GS_REG_XYZ2 | (GS_REG_XYZ2 << 4);
    Dma_EndDirect(p);
}

/* Writes a textured rectangle into the caller's packet as 32-texel-wide sprites, so that each one stays inside the
 * GS texture cache page. `blend` selects alpha blending. */
void Dma_PutTexStrips(GsQword **pp, s32 x0, s32 y0, s32 x1, s32 y1, s32 xFrac, s32 yFrac, s32 u0, s32 v0, s32 u1, s32 v1,
                      s32 uFrac, s32 vFrac, u32 rgba, s32 blend) {
    s32 step = 32;
    s32 n = (u1 - u0) / step;
    s32 w = x1 - x0;
    s32 i;

    (*pp)->d[0] = GIF_TAG(2, 0, 1);
    (*pp)->d[1] = GIF_REG_AD;
    (*pp)++;
    (*pp)->d[0] = blend ? 0x156 : 0x116;
    (*pp)->d[1] = GS_PRIM;
    (*pp)++;
    (*pp)->d[0] = (u64)rgba | (0x3F800000ULL << 32);
    (*pp)->d[1] = GS_RGBAQ;
    (*pp)++;
    (*pp)->d[0] = GIF_TAG_EX(n, 1, GIF_FLG_REGLIST, 4);
    (*pp)->d[1] = 0x5353;
    (*pp)++;
    if (w % n == 0) {
        s32 dx = w / n;

        for (i = 0; i < n; i++) {
            (*pp)->d[0] = GS_SET_UV(((u0 + i * step) << 4) + uFrac, (v0 << 4) + vFrac);
            (*pp)->d[1] = GS_SET_XYZ(((x0 + i * dx) << 4) + xFrac + 0x7000, ((y0 + 0x720) << 4) + yFrac, 0);
            (*pp)++;
            (*pp)->d[0] = GS_SET_UV(((u0 + (i + 1) * step) << 4) + uFrac, (v1 << 4) + vFrac);
            (*pp)->d[1] = GS_SET_XYZ(((x0 + (i + 1) * dx) << 4) + xFrac + 0x7000, ((y1 + 0x720) << 4) + yFrac, 0);
            (*pp)++;
        }
    } else {
        f32 dx = (f32)(w << 4) / (f32)n;

        for (i = 0; i < n; i++) {
            (*pp)->d[0] = GS_SET_UV(((u0 + i * step) << 4) + uFrac, (v0 << 4) + vFrac);
            (*pp)->d[1] = GS_SET_XYZ(x0 + (xFrac + (s32)(dx * (f32)i)) + 0x7000, ((y0 + 0x720) << 4) + yFrac, 0);
            (*pp)++;
            (*pp)->d[0] = GS_SET_UV(((u0 + (i + 1) * step) << 4) + uFrac, (v1 << 4) + vFrac);
            (*pp)->d[1] = GS_SET_XYZ(x0 + (xFrac + (s32)(dx * (f32)(i + 1))) + 0x7000, ((y1 + 0x720) << 4) + yFrac, 0);
            (*pp)++;
        }
    }
}
