#ifndef SYS_GFX_OT_H
#define SYS_GFX_OT_H

#include "types.h"

/*
 * Ordering table for depth-sorted (translucent) primitives, drawn after the opaque scene.
 * See the notes at the top of src/sys/gfx_ot.c.
 */

#define OT_LAYER_COUNT 2       /* chains per depth slot, each with its own blend equation */
#define OT_Z_SLOTS 0x1000      /* depth slots reached through gOtZ */
#define OT_SLOT_COUNT (OT_Z_SLOTS + 2) /* plus one slot before and one after them */
#define OT_ENTRY_COUNT (OT_SLOT_COUNT * OT_LAYER_COUNT)
#define OT_PRIM_BUF_SIZE 0x73000 /* packet memory handed out through gOtCur */
#define OT_HEAD_SIZE 0x40      /* size of the packet that starts every chain */

/* Header of every packet in a chain: a source-chain DMA tag whose address word links to the next packet. */
typedef struct OtPrim {
    /* 0x00 */ u16 qwc;          /* quadwords of GIF data that follow this header */
    /* 0x02 */ u16 tagId;        /* upper half of the DMA tag (NEXT) */
    /* 0x04 */ struct OtPrim *next;
    /* 0x08 */ u32 vif[2];       /* VIF codes of the tag; not sent (Ot_Send copies only the GIF data) */
} OtPrim; /* 0x10, GIF data follows */

/* One chain: its fixed head packet and the last packet linked so far (== head when empty). */
typedef struct OtEntry {
    /* 0x00 */ OtPrim *head;
    /* 0x04 */ OtPrim *tail;
} OtEntry;

/* One depth slot: a chain per layer. Within a slot, layer 0 is drawn before layer 1. */
typedef struct OtSlot {
    /* 0x00 */ OtEntry layer[OT_LAYER_COUNT];
} OtSlot; /* 0x10 */

/* gOt (0x48 bytes). */
typedef struct Ot {
    /* 0x00 */ OtSlot *slots[3];    /* [0] first slot, [1] the OT_Z_SLOTS depth slots, [2] last slot; copied to gOtFirst / gOtZ / gOtLast */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ u64 alpha1[OT_LAYER_COUNT]; /* GS ALPHA_1 written by each layer's head packets */
    /* 0x20 */ u64 alpha2[OT_LAYER_COUNT]; /* GS ALPHA_2, same */
    /* 0x30 */ void *headBuf[OT_LAYER_COUNT]; /* OT_SLOT_COUNT head packets per layer */
    /* 0x38 */ void *buf;          /* packet memory, followed by the table itself */
    /* 0x3C */ s32 cur;            /* index added to slots[] by Ot_Reset; always 0 */
    /* 0x40 */ s32 bufSize;        /* OT_PRIM_BUF_SIZE */
    /* 0x44 */ s32 unk44;
} Ot;

extern Ot *gOt;
extern OtPrim *gOtHead;   /* first packet of the linked list built by Ot_Link */
extern OtPrim *gOtTail;   /* last one; NULL when nothing was queued */
extern u32 *gOtCur;       /* allocation cursor in the packet memory */
extern OtSlot *gOtFirst;  /* slot drawn before every depth slot */
extern OtSlot *gOtZ;      /* depth slots, drawn in increasing index order */
extern OtSlot *gOtLast;   /* slot drawn after every depth slot */

void Ot_Init(void);
void Ot_Term(void);
void Ot_SetDefaultAlpha(void);
void Ot_ResetCursor(void);
s32 Ot_Reset(void);
s32 Ot_ResetEntries(OtEntry *entry, s32 count);
void Ot_SetLayerAlpha(s32 layer, u64 *alpha);
void Ot_Add(OtPrim *prim, OtEntry *entry);
void Ot_AddChain(OtPrim *first, OtPrim *last, OtEntry *entry);
void Ot_Link(OtEntry *entry, s32 count);
s32 Ot_Draw(void);
s32 Ot_Send(void);
void Ot_AddEnv(s32 test);
void Ot_AddRestoreEnv(void);
s32 Ot_Stub102C60(void);
s32 Ot_Stub102C68(void);
s32 Ot_GetCapacity(void);
void Ot_SetBuffer(void *buf, s32 size);
void Ot_InitLayers(void);
void Ot_FreeLayers(void);
OtPrim *Ot_NewHead(s32 layer);
void Ot_BuildHeads(s32 layer);
s32 Ot_Align64(s32 size);

#endif
