#include "common.h"
#include "sys/bpe.h"
#include "sys/heap.h"

/* Expands byte-pair-encoded data (Philip Gage's "expand"); allocates the output when dst is NULL. Returns the output. */
void *Bpe_Decode(void *src, void *dst, s32 *rawSize) {
    u8 left[256];
    u8 right[256];
    u8 stack[256];
    s32 size;
    s32 packSize;
    u8 *in;
    u8 *out;
    s16 c;
    s16 count;
    s16 i;
    s16 blockSize;
    s32 hi;
    void *buf;

    size = *(s32 *)src;
    src = (s32 *)src + 1;
    packSize = *(s32 *)src;
    src = (s32 *)src + 1;
    if (rawSize != NULL) {
        *rawSize = size;
    }
    if (dst != NULL) {
        buf = dst;
    } else {
        buf = Heap_Alloc(size, 0x20, 0, HEAP_ANY);
    }
    out = buf;
    in = src;
    while (in - (u8 *)src != packSize) {
        count = *in++;
        for (i = 0; i < 256; i++) {
            left[i] = i;
        }
        c = 0;
        for (;;) {
            if (count > 127) {
                c += count - 127;
                count = 0;
            }
            if (c == 256) {
                break;
            }
            for (i = 0; i <= count; i++, c++) {
                left[c] = *in++;
                if (c != left[c]) {
                    right[c] = *in++;
                }
            }
            if (c == 256) {
                break;
            }
            count = *in++;
        }
        hi = *in++;
        blockSize = *in++ + (hi << 8);
        i = 0;
        for (;;) {
            if (i != 0) {
                c = stack[--i];
            } else {
                if (blockSize-- == 0) {
                    break;
                }
                c = *in++;
            }
            if (c == left[c]) {
                *out++ = c;
            } else {
                stack[i++] = right[c];
                stack[i++] = left[c];
            }
        }
    }
    return buf;
}
