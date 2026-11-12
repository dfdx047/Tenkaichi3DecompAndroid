/*
 * The three Sony graphics-library calls that actually put something into GS memory. The game leaves the
 * per-frame CLEAR of the colour and depth buffers to the library: sceGsSetDefDBuff builds, for each of the two
 * buffers, a GIF packet "drawing environment + clear sprite" inside the sceGsDBuff structure (the game then
 * patches the sprite's corners and colour), and sceGsSwapDBuff sends the packet of the buffer about to be drawn.
 * Without them nothing clears the depth buffer between frames (seen as smears trailing behind camera pans).
 * The layout is libgraph's (include/sys/gfx.h in the game sources documents it).
 */
#include <stdint.h>
#include <string.h>
#include "../port_addr.h"

extern void Port_GsGifChannel(uint32_t addr, uint32_t qwc, int chain);
int gGsMainFbp = -1; /* frame-buffer address of the frame being drawn (from sceGsSwapDBuff); -1 before the first swap */

typedef struct { uint64_t v, addr; } Reg;                      /* one A+D register write */
typedef struct { Reg frame, zbuf, xyoffset, scissor, prmodecont, colclamp, dthe, test; } DrawEnv;
typedef struct { Reg testa, prim, rgbaq, xyz2a, xyz2b, testb; } Clear;
typedef struct {
    uint64_t disp[2][5];
    uint64_t giftag0[2];
    DrawEnv draw0;
    Clear clear0;
    uint64_t giftag1[2];
    DrawEnv draw1;
    Clear clear1;
} DBuff;

static void set(Reg *r, uint64_t v, uint64_t addr) {
    r->v = v;
    r->addr = addr;
}

static void fill(uint64_t *tag, DrawEnv *d, Clear *c, uint32_t fbp, uint32_t zbp, int psm, int w, int h, int ztest, int zpsm, int clear) {
    uint64_t test = ztest ? (1ull << 16) | ((uint64_t)(ztest & 3) << 17) : 0;

    tag[0] = (uint64_t)(clear ? 14 : 8) | (1ull << 15) | (1ull << 60); /* NLOOP, EOP, one register per loop */
    tag[1] = 0xE;                                                      /* A+D */
    set(&d->frame, fbp | (uint64_t)((w + 63) / 64) << 16 | (uint64_t)psm << 24, 0x4C);
    set(&d->zbuf, zbp | (uint64_t)(zpsm & 15) << 24 | (uint64_t)(ztest == 0) << 32, 0x4E);
    set(&d->xyoffset, (uint64_t)((2048 - w / 2) << 4) | (uint64_t)((2048 - h / 2) << 4) << 32, 0x18);
    set(&d->scissor, (uint64_t)(w - 1) << 16 | (uint64_t)(h - 1) << 48, 0x40);
    set(&d->prmodecont, 1, 0x1A);
    set(&d->colclamp, 1, 0x46);
    set(&d->dthe, (psm & 2) ? 1 : 0, 0x45);
    set(&d->test, test, 0x47);
    set(&c->testa, (1ull << 16) | (1ull << 17), 0x47); /* depth test ALWAYS: the sprite writes every pixel's depth */
    set(&c->prim, 6, 0x00);
    set(&c->rgbaq, 0, 0x01);
    set(&c->xyz2a, (uint64_t)((2048 - w / 2) << 4) | (uint64_t)((2048 - h / 2) << 4) << 16, 0x05);
    set(&c->xyz2b, (uint64_t)((2048 + w / 2) << 4) | (uint64_t)((2048 + h / 2) << 4) << 16, 0x05);
    set(&c->testb, test, 0x47);
}

int sceGsSetDefDBuff(void *p, int psm, int w, int h, int ztest, int zpsm, int clear) {
    DBuff *db = p;
    uint32_t bytes = (psm & 2) ? 2 : 4, pages = ((uint32_t)w * (uint32_t)h * bytes + 8191) / 8192;

    memset(db, 0, sizeof(*db));
    fill(db->giftag0, &db->draw0, &db->clear0, 0, pages * 2, psm, w, h, ztest, zpsm, clear);
    fill(db->giftag1, &db->draw1, &db->clear1, pages, pages * 2, psm, w, h, ztest, zpsm, clear);
    return 0;
}

int sceGsPutDrawEnv(uint64_t *giftag) {
    Port_GsGifChannel(Port_Addr(giftag), 1 + (uint32_t)(giftag[0] & 0x7FFF), 0);
    return 0;
}

int sceGsSwapDBuff(void *p, int id) {
    DBuff *db = p;
    /* the buffer this frame is drawn into: what the renderer shows when the frame is finished */
    gGsMainFbp = (int)(((id & 1) ? db->draw1.frame.v : db->draw0.frame.v) & 0x1FF);
    return sceGsPutDrawEnv((id & 1) ? db->giftag1 : db->giftag0);
}
