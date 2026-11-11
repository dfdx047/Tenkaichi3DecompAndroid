/* ADX loop metadata used by both playback and deterministic stream timing.
 * Version 3 has no initial-history block; mono/stereo version 4 has eight
 * history bytes. Format reference: vgmstream/src/meta/adx.c. */
#ifndef BT4_ADX_LOOP_H
#define BT4_ADX_LOOP_H
#include <stddef.h>
#include <stdint.h>
static uint32_t bt4_adx_be32(const uint8_t *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static int bt4_adx_loop(const uint8_t *h, size_t bytes, uint32_t total, uint32_t *start, uint32_t *end) {
    size_t base, header;
    *start = *end = 0;
    if (bytes < 0x14 || h[0]!=0x80 || h[1]!=0 || h[7]<1 || h[7]>2 || h[0x13]!=0) return 0;
    base = h[0x12]==3 ? 0x14 : h[0x12]==4 ? 0x20 : 0;
    header = ((size_t)h[2]<<8 | h[3]) + 4;
    if (!base || header < base+0x18+6 || bytes < base+0x14 || !bt4_adx_be32(h+base+4)) return 0;
    uint32_t first=bt4_adx_be32(h+base+8), last=bt4_adx_be32(h+base+0x10);
    if (first>=last || last>total) return 0;
    *start=first; *end=last;
    return 1;
}
#endif
