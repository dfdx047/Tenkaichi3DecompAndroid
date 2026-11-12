#include "common.h"
/*
 * Low-level readers of the Flash-like movie data (0x10A6E0..0x10AD58): header check, tag and named-block walks,
 * a bit reader and the bit-packed matrix. The object very likely continues at 0x10AD58 (Flash_Create and the rest
 * of the player, another range); split from gfxm_b_b.c because it has nothing to do with textures.
 * Proper name: flash_data.c (or the head of flash.c).
 */
#include "sys/gfxm_b_c.h"

extern void *memcpy(void *dst, const void *src, u32 size);
extern void *memset(void *dst, s32 c, u32 n);
extern s32 strcmp(const char *a, const char *b);
extern u32 strlen(const char *s);

/* True when the data starts with 'F' 'O' 'D' 0x11 "LIT". */
s32 Flash_CheckHeader(u8 *data) {
    FlashHeader hdr;
    u8 ver;

    memset(&hdr, 0, sizeof(hdr));
    memcpy(&hdr, data, sizeof(hdr));
    if (hdr.magic[0] != 'F' || hdr.magic[1] != 'O' || hdr.magic[2] != 'D') {
        return 0;
    }
    ver = hdr.magic[3];
    if (ver != 0x11) {
        return 0;
    }
    return strcmp(hdr.kind, "LIT") == 0;
}

/* Walks the tag list and returns the first tag with the given code, or NULL when the end tag (code 0) comes first. */
u8 *Flash_FindTag(u8 *data, u8 code) {
    s32 ofs = 0;
    u8 *found = NULL;
    FlashTag tag;

    do {
        memset(&tag, 0, sizeof(tag));
        memcpy(&tag, data + ofs, sizeof(tag));
        if (tag.code == code) {
            found = data + ofs;
            break;
        }
        ofs = ofs + tag.size + 8;
    } while (tag.code != 0);
    return found;
}

/* Returns the address of record `count` of a tag: skips the tag header and `count` sized records. */
u8 *Flash_SkipRecords(u8 *tag, u16 count) {
    FlashTag hdr;
    FlashTag rec;
    s32 n = count;

    memset(&hdr, 0, sizeof(hdr));
    memcpy(&hdr, tag, sizeof(hdr));
    tag += 8;
    while (n != 0) {
        memset(&rec, 0, sizeof(rec));
        n--;
        memcpy(&rec, tag, sizeof(rec));
        tag += 8;
        tag += rec.size;
    }
    return tag;
}

/* Returns record `index` of the first tag with the given code. */
u8 *Flash_GetRecord(u8 *data, u8 code, u16 index) {
    return Flash_SkipRecords(Flash_FindTag(data, code), index);
}

/* Returns the record count of the first tag with the given code, 0 when there is none. */
s32 Flash_GetRecordCount(u8 *data, u8 code) {
    s32 count = 0;
    u8 *tag = Flash_FindTag(data, code);

    if (tag != NULL) {
        count = ((FlashTag *)tag)->count;
    }
    return count;
}

/* Skips `count` named blocks (a NUL-terminated name, a 16-bit size, the data); NULL when a block has size 0. */
u8 *Flash_SkipNamed(u8 *p, u32 count) {
    u32 i;
    s32 ofs = 0;
    u16 size;

    for (i = 0; i < count; i++) {
        while (p[ofs++] != 0) {
        }
        memcpy(&size, p + ofs, 2);
        ofs += 2;
        if (size != 0) {
            ofs += size;
        } else {
            return NULL;
        }
    }
    return p + ofs;
}

/* Returns the index of the named block called `name` in a list of named blocks, or -1. */
s32 Flash_FindName(u8 *p, char *name) {
    char buf[0x100];
    s32 ofs = 0;
    u16 size = 0;
    s32 i = 0;

    do {
        ofs += size;
        memset(buf, 0, 0xFF);
        Flash_ReadString(buf, p, &ofs);
        if (strcmp(buf, name) == 0) {
            return i;
        }
        i++;
        memcpy(&size, p + ofs, 2);
        ofs += 2;
    } while (size != 0);
    return -1;
}

/* Copies the NUL-terminated string at p + *ofs and moves *ofs past it. */
void Flash_ReadString(char *dst, u8 *p, s32 *ofs) {
    char *src = (char *)(p + *ofs);
    s32 n = strlen(src) + 1;

    memcpy(dst, src, n);
    *ofs += n;
}

/* Reads `bits` bits, most significant first, at bit position *bitPos and advances it. */
u32 Flash_ReadBits(u8 *data, u32 *bitPos, u8 bits) {
    u32 pos = *bitPos;
    u32 value = 0;
    u32 byte = pos >> 3;
    u32 bit = pos & 7;
    s32 i;

    if (bits == 0) {
        return 0;
    }
    for (i = 0; i < bits; i++) {
        value <<= 1;
        if (data[byte] & (0x80 >> bit)) {
            value |= 1;
        }
        bit++;
        if (bit >= 8) {
            byte++;
            bit = 0;
        }
    }
    *bitPos = pos + bits;
    return value;
}

/* Reads `bits` bits as a two's complement number. */
s32 Flash_ReadSBits(u8 *data, u32 *bitPos, u8 bits) {
    s32 value = Flash_ReadBits(data, bitPos, bits);

    if (bits == 0) {
        return 0;
    }
    if ((value >> (bits - 1)) & 1) {
        value |= -1 << bits;
    }
    return value;
}

/* Sets a matrix to the identity. */
void Flash_MtxIdentity(FlashMtx *m) {
    m->m[0] = 1.0f;
    m->m[1] = 0.0f;
    m->m[2] = 0.0f;
    m->m[3] = 0.0f;
    m->m[4] = 1.0f;
    m->m[5] = 0.0f;
    m->m[6] = 0.0f;
    m->m[7] = 0.0f;
    m->m[8] = 1.0f;
}

/* Reads a bit-packed matrix at data + *ofs: a flag byte, then for each present pair a 5-bit width and one or two
   signed fields (scale and skew in 16.16, translation in twentieths). Leaves *ofs on the next whole byte. */
void Flash_ReadMtx(u8 *data, FlashMtx *m, s32 *ofs) {
    u32 bitPos = 0;
    u8 flags;
    u8 bits;

    Flash_MtxIdentity(m);
    flags = data[*ofs];
    *ofs += 1;
    if (flags & 0x11) {
        bits = Flash_ReadBits(data + *ofs, &bitPos, 5);
        if ((u8)(flags & 1)) {
            m->m[0] = Flash_ReadSBits(data + *ofs, &bitPos, bits) * (1.0f / 65536.0f);
        }
        if (flags & 0x10) {
            m->m[4] = Flash_ReadSBits(data + *ofs, &bitPos, bits) * (1.0f / 65536.0f);
        }
    }
    if (flags & 0xA) {
        bits = Flash_ReadBits(data + *ofs, &bitPos, 5);
        if (flags & 2) {
            m->m[1] = Flash_ReadSBits(data + *ofs, &bitPos, bits) * (1.0f / 65536.0f);
        }
        if (flags & 8) {
            m->m[3] = Flash_ReadSBits(data + *ofs, &bitPos, bits) * (1.0f / 65536.0f);
        }
    }
    if (flags & 0x24) {
        bits = Flash_ReadBits(data + *ofs, &bitPos, 5);
        if (flags & 4) {
            m->m[2] = Flash_ReadSBits(data + *ofs, &bitPos, bits) * 0.05f;
        }
        if (flags & 0x20) {
            m->m[5] = Flash_ReadSBits(data + *ofs, &bitPos, bits) * 0.05f;
        }
    }
    *ofs += bitPos >> 3;
    if (bitPos & 7) {
        *ofs += 1;
    }
}
