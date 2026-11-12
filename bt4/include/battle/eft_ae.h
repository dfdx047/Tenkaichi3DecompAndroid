#ifndef BATTLE_EFT_AE_H
#define BATTLE_EFT_AE_H

#include "types.h"
#include "sys/math3d.h"

/*
 * 0x1AA7E8..0x1AE2A8: the end of the effect code. Four groups (see src/battle/eft_ae.c):
 *
 *   EftSpr_IsOffScreen   0x1AA7E8            screen test of the sprite writers of the file before
 *   EftSprAnim_*         0x1AA818..0x1ACF00  keyframed sprite animation: an object that plays a "V000" pack
 *                                            (up to 16 layers, each a textured quad animated by keys). VISUAL.
 *   EftSprPack_*         0x1ACF00..0x1AD138  readers of the "V000" pack. VISUAL.
 *   BtlTask*             0x1AD138..0x1ADBA8  the task tree every effect module is built on (lists of 0x40-byte
 *                                            tasks with a six-callback class). Core: it decides the order in
 *                                            which all effect tasks, simulation ones included, are updated.
 *   EftVram_* / EftTexSet_*  0x1ADBA8..0x1AE2A8  per-frame VRAM allocation of effect textures and the
 *                                            "texture set from a pack" loaders (continues in eft_det_a.c). VISUAL.
 */

/* ---- tasks ---------------------------------------------------------------------------------------------- */

struct BtlTask;
struct BtlTaskList;

/* A task class: the six callbacks of one kind of task (BtlTaskClass in battle/btl_scene.h). */
typedef struct BtlTaskCls {
    /* 0x00 */ void (*update)(struct BtlTask *task);            /* BtlTaskList_Update */
    /* 0x04 */ void (*init)(struct BtlTask *task, void *arg);   /* when the task is added */
    /* 0x08 */ void (*term)(struct BtlTask *task);              /* BtlTask_Kill */
    /* 0x0C */ void (*postUpdate)(struct BtlTask *task);        /* BtlTaskList_PostUpdate */
    /* 0x10 */ void (*reset)(struct BtlTask *task);             /* BtlTaskList_Reset */
    /* 0x14 */ void (*draw)(struct BtlTask *task);              /* BtlTaskList_Draw */
} BtlTaskCls; /* size 0x18 */

#define BTL_TASK_TAG_CHAR0 0x800  /* belongs to object id 0: BtlTaskList_Reset(root, 0x800) resets it */
#define BTL_TASK_TAG_CHAR1 0x1000 /* belongs to any other object id */
#define BTL_TASK_TAG_2000 0x2000  /* third reset group */

/* The first byte of a task. The original reads it as bit fields of a byte (lbu / andi / andi 0xFF); a view
   with byte alignment reproduces that. */
typedef struct BtlTaskBits {
    u8 dead : 1;    /* removed by the next list pass (update or reset) that reaches the task */
    u8 updated : 1; /* set after an update of a task that has a draw callback; the draw pass needs it */
    s8 rest : 6;    /* all ones from BtlTask_Init, never read */
} BtlTaskBits;

#define BTL_TASK_BITS(task) ((BtlTaskBits *)(task))

typedef struct BtlTask {
    /* 0x00 */ u8 bits;         /* BtlTaskBits */
    /* 0x01 */ u8 chr;          /* free for the class (the shot layer stores the character index) */
    /* 0x02 */ u16 index;       /* index in the list's task array */
    /* 0x04 */ u32 tag;         /* BTL_TASK_TAG_* and the hit feedback bits of battle/eft_a.h */
    /* 0x08 */ s16 unk8;
    /* 0x0A */ u8 unkA;
    /* 0x0B */ u8 unkB;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ Vec4 pos;        /* (0, 0, 0, 1) at creation; written by BtlTask_SetPos (hit feedback position) */
    /* 0x20 */ struct BtlTaskList *list;
    /* 0x24 */ struct BtlTaskList *child;
    /* 0x28 */ BtlTaskCls *cls;
    /* 0x2C */ struct BtlTask *prev;
    /* 0x30 */ struct BtlTask *next;
    /* 0x34 */ struct BtlTask *nextFree;
    /* 0x38 */ void *work;      /* this task's work block (the size given when the list was created) */
    /* 0x3C */ s32 unk3C;
} BtlTask; /* size 0x40 */

typedef struct BtlTaskList {
    /* 0x00 */ BtlTask *parent; /* the task that owns this list; NULL for the root */
    /* 0x04 */ BtlTask *head;
    /* 0x08 */ BtlTask *tail;
    /* 0x0C */ BtlTask *free;   /* unused tasks, linked through nextFree */
    /* 0x10 */ BtlTask *tasks;  /* the array */
    /* 0x14 */ void *work;      /* count * work size bytes, or NULL */
} BtlTaskList; /* size 0x18 */

/* ---- effect VRAM and texture sets ----------------------------------------------------------------------- */

/* Image record of a texture pack (0x40 bytes). */
typedef struct EftAeImage {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s32 imageBlocks; /* VRAM blocks of the image */
    /* 0x14 */ s32 clutBlocks;  /* VRAM blocks of the CLUT */
    /* 0x18 */ u8 unk18[0x18];
    /* 0x30 */ u64 tex0;
    /* 0x38 */ u8 unk38[8];
} EftAeImage; /* size 0x40 */

/* One texture of a set. */
typedef struct EftAeTex {
    /* 0x0 */ u64 tex0;         /* GS TEX0 with this frame's TBP0 / CBP */
    /* 0x8 */ EftAeImage *image;
    /* 0xC */ s32 unkC;
} EftAeTex; /* size 0x10 */

typedef struct EftAeTexSet4 {
    /* 0x00 */ EftAeTex tex[4];
    /* 0x40 */ s32 count;
} EftAeTexSet4; /* size 0x48 */

typedef struct EftAeTexSet8 {
    /* 0x00 */ EftAeTex tex[8];
    /* 0x80 */ s32 count;
} EftAeTexSet8;

typedef struct EftAeTexSet16 {
    /* 0x000 */ EftAeTex tex[16];
    /* 0x100 */ s32 count;
} EftAeTexSet16;

typedef struct EftAeTexSet32 {
    /* 0x000 */ EftAeTex tex[32];
    /* 0x200 */ s32 count;
    /* 0x204 */ u32 kept;       /* EftSprAnim_KeepTextures: bit per texture already given VRAM this frame */
} EftAeTexSet32; /* size 0x208 */

/* Header of a texture pack once relocated. */
typedef struct EftAeTexPack {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ EftAeImage *images;
} EftAeTexPack;

/* A texture that wants VRAM this frame. */
typedef struct EftAeVramEntry {
    /* 0x0 */ EftAeTex *tex;
    /* 0x4 */ s32 imageBlock;
    /* 0x8 */ s32 clutBlock;
    /* 0xC */ s32 flags;        /* 1 upload the image, 2 upload the CLUT */
} EftAeVramEntry; /* size 0x10 */

/* The effect VRAM allocator (gEftVram): a list rebuilt every frame. */
typedef struct EftAeVram {
    /* 0x000 */ EftAeVramEntry entry[128];
    /* 0x800 */ u32 count;
    /* 0x804 */ s32 imageNext;  /* next free block for images, 0x2B00 at the start of a frame (grows up) */
    /* 0x808 */ s32 clutNext;   /* next free block for CLUTs, 0x2A00 at the start of a frame (grows up) */
} EftAeVram; /* size 0x80C */

/* ---- "V000" sprite animation pack ----------------------------------------------------------------------- */

/* A key of a layer. */
typedef struct EftSprKey {
    /* 0x00 */ Vec4 pos;        /* offset from the object, in the object's frame */
    /* 0x10 */ Vec4 rot;        /* degrees; with layer flag 0x40 the first key's rot is a spin per frame */
    /* 0x20 */ Vec4 scale;
    /* 0x30 */ u8 color[4];     /* 0..255 per channel, multiplied with the object's colour */
    /* 0x34 */ s32 frame;
    /* 0x38 */ s32 unk38[2];
} EftSprKey; /* size 0x40 */

#define EFT_SPR_LAYER_LOOP 0x1       /* loops between two keys loopCount times (-1: for ever) */
#define EFT_SPR_LAYER_SLOT_MODE 0x2  /* draw mode follows the slot's flag (EftSprAnim_SetSlotFlag) */
#define EFT_SPR_LAYER_BILLBOARD 0x4  /* faces the camera */
#define EFT_SPR_LAYER_SUBDIV 0x8     /* drawn as four quads (finer clipping) */
#define EFT_SPR_LAYER_GRID 0x10      /* the texture is a grid of cells played as a flip book */
#define EFT_SPR_LAYER_GRID_RAND 0x20 /* the cell is random each frame */
#define EFT_SPR_LAYER_SPIN 0x40      /* the rotation is the accumulated spin, not the keys' */
#define EFT_SPR_LAYER_BLINK 0x80     /* blinks with a random phase */
#define EFT_SPR_LAYER_JITTER_XY 0x100
#define EFT_SPR_LAYER_JITTER_X 0x200
#define EFT_SPR_LAYER_JITTER_Y 0x400

typedef struct EftSprLayerDef {
    /* 0x00 */ u8 tex;          /* index into the texture set */
    /* 0x01 */ u8 unk1;
    /* 0x02 */ u8 otLayer;      /* ordering-table layer; >= 2 selects the second GS context */
    /* 0x03 */ u8 speed;        /* frames advance by speed * 2 per step */
    /* 0x04 */ u8 keyCount;
    /* 0x05 */ u8 loopEndKey;
    /* 0x06 */ u8 loopStartKey;
    /* 0x07 */ s8 loopCount;
    /* 0x08 */ s16 length;      /* frames */
    /* 0x0A */ s16 jitterPeriod;
    /* 0x0C */ s8 grid;         /* 0: 2x2, 1: 4x2, 2: 4x4, 3: 3x3 */
    /* 0x0D */ s8 blinkOn;
    /* 0x0E */ s8 blinkOff;
    /* 0x0F */ s8 gridStep;     /* frames per cell; also the blink's random range */
    /* 0x10 */ u32 flags;       /* EFT_SPR_LAYER_* */
    /* 0x14 */ EftSprKey *keys; /* bound by EftSprPack_Bind */
    /* 0x18 */ s32 fade;        /* frames of fade-out after the last frame */
    /* 0x1C */ s16 jitterX;     /* random range, in 1/1000 */
    /* 0x1E */ s16 jitterY;
} EftSprLayerDef; /* size 0x20 */

typedef struct EftSprPack {
    /* 0x00 */ u32 magic;       /* "V000" */
    /* 0x04 */ u8 layerCount;
    /* 0x05 */ u8 loop;         /* restart when every layer has finished */
    /* 0x06 */ u8 unk6[2];
    /* 0x08 */ s32 keysOfs;     /* offset of the first layer's keys */
    /* 0x0C */ u8 unkC[0x14];
    /* 0x20 */ EftSprLayerDef layer[1];
} EftSprPack;

/* Play state of one layer. */
typedef struct EftSprLayer {
    /* 0x00 */ f32 loopStart;
    /* 0x04 */ f32 frame;
    /* 0x08 */ f32 time;        /* frames since the start, not rewound by loops */
    /* 0x0C */ f32 lastFrame;
    /* 0x10 */ f32 fadeLen;
    /* 0x14 */ f32 fadeLeft;
    /* 0x18 */ f32 speed;
    /* 0x1C */ s32 loops;
    /* 0x20 */ s32 rand1;       /* two libc rand() values drawn every step */
    /* 0x24 */ s32 rand2;
    /* 0x28 */ s32 flags;       /* 1: finished */
    /* 0x2C */ f32 alpha;       /* fade-out factor */
    /* 0x30 */ Vec4 rot;        /* accumulated spin, degrees */
    /* 0x40 */ Vec4 spin;
    /* 0x50 */ EftSprLayerDef *def;
    /* 0x54 */ s32 unk54[3];
} EftSprLayer; /* size 0x60 */

/* A slot of the pool gEftSprAnimPool (32 slots). */
typedef struct EftSprSlot {
    /* 0x000 */ EftSprLayer layer[16];
    /* 0x600 */ s32 used;
    /* 0x604 */ s32 flag;       /* 1 at creation; picks the draw mode of layers with EFT_SPR_LAYER_SLOT_MODE */
    /* 0x608 */ s32 unk608[2];
} EftSprSlot; /* size 0x610 */

#define EFT_SPR_DRAW_BILLBOARD 0x1
#define EFT_SPR_DRAW_ORIENTED 0x2
#define EFT_SPR_DRAW_MODE1 0x4
#define EFT_SPR_DRAW_MODE0 0x8
#define EFT_SPR_DRAW_MODE2 0x10 /* depth forced to the far end of the ordering table */

/* The object the users embed in their work block. */
typedef struct EftSprAnim {
    /* 0x00 */ EftSprPack *pack;
    /* 0x04 */ EftAeTexSet32 *tex;
    /* 0x08 */ EftSprSlot *slot;
    /* 0x0C */ s32 drawFlags;   /* EFT_SPR_DRAW_*: overrides of the layers' own flags */
    /* 0x10 */ Vec4 pos;
    /* 0x20 */ Vec4 dir;        /* the object's forward axis; (0, 0, -1) at creation */
    /* 0x30 */ Vec4 scale;
    /* 0x40 */ Vec4 color;      /* 0..1 per channel */
} EftSprAnim; /* size 0x50 */

#endif
