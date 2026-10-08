/*
 * DMA transfers on PC. On the PS2 the game starts a transfer by setting bit 0x100 of a channel's control register
 * (Dn_CHCR) and the hardware clears the bit when the data has gone out. Here the game's register macros call
 * Port_DmaChcr on every access: a transfer that was started is carried out first and the bit cleared, so the
 * game's "wait until the channel is idle" loops end at once.
 *
 * Channels: 1 = VIF1 (models through the VU1 microprograms), 2 = GIF (GS packets), 4 = IPU (movies).
 * Headless build: the data is dropped. A renderer takes over in Port_DmaTransfer.
 */
#include <stdint.h>

#ifdef __ANDROID__
extern uint8_t gPortHwRegs[]; /* plat_mem.c: the registers live in the engine (irfix.py points the game there) */
#define REG(a) ((volatile uint32_t *)(gPortHwRegs + ((uint32_t)(a) - 0x10000000u)))
#else
#define REG(a) ((volatile uint32_t *)(uintptr_t)(a))
#endif

extern void Port_GsVif1Chain(uint32_t tadr, int tte);
extern void Port_GsGifChannel(uint32_t addr, uint32_t qwc, int chain);

static void Port_DmaTransfer(int channel) {
    /* Channel 1 in source-chain mode (CHCR mode bits = 1) is the frame's display list: D1_TADR points at it. */
    if (channel == 1 && ((*REG(0x10009000u) >> 2) & 3) == 1) {
        Port_GsVif1Chain(*REG(0x10009030u), (int)((*REG(0x10009000u) >> 6) & 1));
    }
    /* Channel 2 is the GIF channel: normal mode sends QWC quadwords at MADR, chain mode a chain at TADR. */
    if (channel == 2) {
        int chain = ((*REG(0x1000A000u) >> 2) & 3) == 1;
        Port_GsGifChannel(chain ? *REG(0x1000A030u) : *REG(0x1000A010u), *REG(0x1000A020u), chain);
    }
}

volatile uint32_t *Port_DmaChcr(int channel) {
    volatile uint32_t *chcr = REG(0x10008000u + 0x1000u * (uint32_t)channel - (channel == 4 ? 0xC00u : 0u));

    if (*chcr & 0x100u) {
        Port_DmaTransfer(channel);
        *chcr &= ~0x100u;
    }
    return chcr;
}
