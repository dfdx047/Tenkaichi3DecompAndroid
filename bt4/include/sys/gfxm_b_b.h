#ifndef SYS_GFXM_B_B_H
#define SYS_GFXM_B_B_H

#include "types.h"

/* One block of a chunked upload file: 0x10 bytes of header followed by its data. */
typedef struct TexChunkHdr {
    /* 0x00 */ u64 tag;  /* copied to the list entry; meaning not used here */
    /* 0x08 */ s32 next; /* bytes of data when another chunk follows; 0 on the last chunk */
    /* 0x0C */ s32 size; /* bytes of data on the last chunk */
} TexChunkHdr;

/* One entry of the list TexChain_Build makes for a chunked file. */
typedef struct TexChainEnt {
    /* 0x00 */ s32 size;   /* bytes of data */
    /* 0x04 */ u32 *data;  /* the chunk's data (a ready GS packet; its word 9 is the BITBLTBUF destination) */
    /* 0x08 */ u64 tag;
} TexChainEnt;

/* A chunked upload file turned into a DMA chain (0x10 bytes, passed and returned by value). */
typedef struct TexChain {
    /* 0x00 */ TexChainEnt *list; /* inside the file, after the chunks, 64-byte aligned */
    /* 0x04 */ u32 *tags;         /* DMA tags: one REF per chunk and an END, after the list */
    /* 0x08 */ u32 count;
    /* 0x0C */ void *file;
} TexChain;

/* One texture of a texture file (0x40 bytes). */
typedef struct TexEntry {
    /* 0x00 */ u32 pixOfs;   /* pixel packet, in words from the file's base; 0 = none */
    /* 0x04 */ u32 clutOfs;  /* CLUT packet, same */
    /* 0x08 */ s32 pixSize;  /* bytes */
    /* 0x0C */ s32 clutSize;
    /* 0x10 */ s32 tbpStep;  /* GS blocks the pixels take */
    /* 0x14 */ s32 cbpStep;  /* GS blocks the CLUT takes */
    /* 0x18 */ u32 pixBlt;   /* bits 48..63 of BITBLTBUF for the pixels (DBW, DPSM) */
    /* 0x1C */ u32 clutBlt;  /* the same for the CLUT */
    /* 0x20 */ s32 tbpOfs;   /* block offset of the pixels from the file's first texture */
    /* 0x24 */ s32 cbpOfs;
    /* 0x28 */ u8 unk28[8];
    /* 0x30 */ u64 tex0;     /* TEX0 without the block pointers */
    /* 0x38 */ void *pix;    /* set by Res_RelocateOffsets */
    /* 0x3C */ void *clut;
} TexEntry;

/* Header of a texture file. */
typedef struct TexFile {
    /* 0x00 */ u32 count;
    /* 0x04 */ u32 entOfs;   /* entry table, in words from the file's base */
    /* 0x08 */ u8 unk08[8];
    /* 0x10 */ TexEntry *ent; /* set by Res_RelocateOffsets */
} TexFile;

/* Packet in front of each upload: BITBLTBUF, whose value is patched in place (0x30 bytes, at 0x2C3410). */
typedef struct TexBltPacket {
    /* 0x00 */ u32 dmaTag[4];
    /* 0x10 */ u64 gifTag[2];
    /* 0x20 */ u64 bitbltbuf;
    /* 0x28 */ u64 reg;
} TexBltPacket;

extern TexBltPacket gTexBltPacket;

s32 Tex_Log2Size(s32 size);
void Res_RelocateOffsets(TexFile **out, u8 *base, TexFile *hdr);
void Tex_UploadPixels(TexEntry *e, s32 tbp);
void Tex_UploadClut(TexEntry *e, s32 cbp);
void TexFile_UploadPacked(TexFile *file, s32 tbp, s32 cbp);
void TexFile_UploadOne(TexFile *file, s32 index, s32 tbp, s32 cbp);
void Tex_Upload(TexEntry *e, s32 tbp, s32 cbp);
void TexFile_UploadRangePacked(TexFile *file, s32 first, s32 count, s32 tbp, s32 cbp);
void TexFile_UploadRange(TexFile *file, s32 first, s32 count, s32 tbp, s32 cbp);
void TexFile_UploadAll(TexFile *file, s32 tbp, s32 cbp);
void TexFile_UploadAt(TexFile *file, s32 index, s32 tbp, s32 cbp);

#endif
