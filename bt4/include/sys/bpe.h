#ifndef SYS_BPE_H
#define SYS_BPE_H

#include "types.h"

/* Header of a byte-pair-encoded buffer; the compressed blocks follow. */
typedef struct BpeHeader {
    /* 0x00 */ s32 rawSize;  /* size after decoding */
    /* 0x04 */ s32 packSize; /* size of the compressed data that follows */
} BpeHeader;

void *Bpe_Decode(void *src, void *dst, s32 *rawSize);

#endif
