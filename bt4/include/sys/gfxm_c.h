#ifndef SYS_GFXM_C_H
#define SYS_GFXM_C_H

#include "types.h"
#include "sys/gfxm_b_c.h"
#include "sys/gfxm_b_b.h"

/*
 * The movie player (0x10AD58..0x10EC18, src/sys/gfxm_c.c). Movie files are converted SWF files.
 *
 * File: 'F' 'O' 'D' 0x11 "LIT\0", then tags: u8 code, u8, u16 record count, u32 size, data; code 0 ends.
 * Records inside a tag have the same 8-byte header (their own size at +4). Tags the player reads:
 *   2  sprite list: record i = header + u16 id (the record of tag 6 that holds sprite i's frames)
 *   3  images: header + u16 width + u16 height; image i draws with Flash.tex[i]
 *   4  shapes: header + u16 quad count + FlashShapeQuad[count]
 *   6  sprite frames: one record per sprite; the header's count field is the sprite's number of depths
 *   7  root frames: one record, same layout
 * A frame record is a list of named blocks: the frame's label (NUL-terminated, usually empty), u16 size, then the
 * frame's tags: u16 SWF tag code + data, ended by code 1 (ShowFrame). A block of size 0 ends the timeline.
 *   4  PlaceObject:   u16 depth, u8 kind (4 shape, 6 sprite), u16 id, matrix, (sprite only) colour transform
 *   5  RemoveObject:  u16 depth, u8 kind, u16 id
 *   12 DoAction:      SWF actions until 0: 0x81 GotoFrame u16, 4 NextFrame, 5 PrevFrame, 6 Play, 7 Stop,
 *                     0x83 GetURL url target (two strings), 0x8A WaitForFrame (3 bytes, ignored),
 *                     0x8B SetTarget name, 0x8C GotoLabel label
 *   26 PlaceObject2:  u8 flags (2 new, 4 matrix, 8 colour transform, 0x20 name), u16 depth,
 *                     [u8 kind, u16 id], [matrix], [colour transform], [name]
 *   28 RemoveObject2: u16 depth
 * Multi-byte fields are unaligned. Matrix: see Flash_ReadMtx. Colour transform: see Flash_ReadCxform.
 * GetURL is the movie's channel to the game: "pad" "true" / "false" (Flash.flags bit 2), "trig" "n" and "se" "n"
 * (bit n of Flash.trig / Flash.se for one tick), "trigger" "end" (Flash.flags bit 8 for one tick).
 */

struct Flash;
struct FlashClip;

/* The textures of one image: the entry table of a texture file (see TexEntry). Entry n carries palette n; when
   it has no pixels of its own the pixels are entry 0's. */
typedef struct FlashTex {
    TexEntry ent[1];
} FlashTex;

/* Colour transform: channel = channel * mul + add, clamped to 0..255. Order r, g, b, a. */
typedef struct FlashCxform {
    /* 0x00 */ f32 mul[4];
    /* 0x10 */ s16 add[4];
} FlashCxform; /* 0x18 */

/* A placed shape (one slot of the shape pool). */
typedef struct FlashShape {
    /* 0x00 */ u16 used;
    /* 0x02 */ u16 id;      /* record of tag 4 */
    /* 0x04 */ FlashMtx mtx;
    /* 0x28 */ FlashCxform cx;
} FlashShape; /* 0x40 */

/* The shape pool of a movie. */
typedef struct FlashShapePool {
    /* 0x00 */ FlashShape *items;
    /* 0x04 */ u32 count;
} FlashShapePool; /* 8 */

/* One depth of a timeline: what is placed there. */
typedef struct FlashSlot {
    /* 0x00 */ FlashShape *shape;
    /* 0x04 */ struct FlashClip *clip;
} FlashSlot; /* 8 */

/* FlashTl.state */
#define FLASH_TL_PLAY 0x01    /* advance one frame per tick */
#define FLASH_TL_HOLD 0x02    /* a NextFrame / PrevFrame action moved the play head: it no longer steps by itself */
#define FLASH_TL_GOTO 0x04    /* `frame` was set by a jump: rebuild the display list up to it */
#define FLASH_TL_SEEK 0x08    /* replaying earlier frames: actions are skipped */
#define FLASH_TL_KEEP 0x10    /* the jump continues from `prev` instead of from frame 0 */

/* A timeline: the root movie or one sprite instance. */
typedef struct FlashTl {
    /* 0x00 */ struct Flash *owner;
    /* 0x04 */ FlashSlot *slots;  /* `depths` entries; depth n is slots[n - 1] */
    /* 0x08 */ u8 *frames;        /* the named frame blocks, after the 8-byte record header */
    /* 0x0C */ u32 frame;         /* frame to run next */
    /* 0x10 */ u32 prev;          /* frame the last label jump left */
    /* 0x14 */ u8 unk14;
    /* 0x15 */ u8 state;          /* FLASH_TL_ */
    /* 0x16 */ u16 depths;
} FlashTl; /* 0x18 */

/* FlashProp.flags: which overrides the game has set on a clip. */
#define FLASH_OV_TEX 0x0001      /* texture index */
#define FLASH_OV_POS 0x0002      /* add pos */
#define FLASH_OV_UV 0x0004       /* replace the texture rectangle (and index 0) */
#define FLASH_OV_ALPHA 0x0008    /* multiply alpha */
#define FLASH_OV_SCALE 0x0010    /* scale */
#define FLASH_OV_COLOR 0x0020    /* multiply r, g, b */
#define FLASH_OV_FLIPX 0x0040    /* quad flag 0x01 */
#define FLASH_OV_FLIPY 0x0080    /* quad flag 0x02 */
#define FLASH_OV_REPEAT 0x0100   /* quad flag 0x04 */
#define FLASH_OV_NO_OFS 0x0200   /* do not add the movie's screen offset */
#define FLASH_OV_ADD 0x0400      /* quad flag 0x08 */
#define FLASH_OV_SUB 0x0800      /* quad flag 0x10 */
#define FLASH_OV_MASK 0x1000     /* quad flag 0x20; inherited by children */
#define FLASH_OV_MASK_SET 0x2000 /* quad flag 0x40; inherited by children */

/* Texture rectangle of a quad, in texels, and which texture of the image's file. */
typedef struct FlashUv {
    /* 0x00 */ s32 u0;
    /* 0x04 */ s32 v0;
    /* 0x08 */ s32 u1;
    /* 0x0C */ s32 v1;
    /* 0x10 */ s32 tex;
} FlashUv; /* 0x14 */

/* Drawing state of a clip: the game's overrides, then matrix and colour transform. Children get the product. */
typedef struct FlashProp {
    /* 0x00 */ u32 flags;      /* FLASH_OV_ */
    /* 0x04 */ f32 x;
    /* 0x08 */ f32 y;
    /* 0x0C */ f32 alpha;      /* 1 by default */
    /* 0x10 */ f32 scaleX;
    /* 0x14 */ f32 scaleY;
    /* 0x18 */ f32 color;
    /* 0x1C */ FlashUv uv;
    /* 0x30 */ FlashMtx mtx;
    /* 0x54 */ FlashCxform cx;
} FlashProp; /* 0x6C */

/* FlashClip.flags */
#define FLASH_CLIP_USED 1
#define FLASH_CLIP_VISIBLE 2
#define FLASH_CLIP_FREE_RUN 8 /* keeps advancing while its parent is stopped */

/* A sprite instance. One exists for every sprite definition of the file (record of tag 2). */
typedef struct FlashClip {
    /* 0x00 */ char name[0x40];  /* instance name from the place tag */
    /* 0x40 */ u16 id;           /* record of tag 6 (the sprite's frames) */
    /* 0x42 */ u16 index;        /* own index in the list */
    /* 0x44 */ u32 flags;        /* FLASH_CLIP_ */
    /* 0x48 */ FlashProp prop;
    /* 0xB4 */ void (*preDraw)(void *arg);
    /* 0xB8 */ void (*postDraw)(void *arg);
    /* 0xBC */ void *preArg;
    /* 0xC0 */ void *postArg;
    /* 0xC4 */ void (*drawOver)(void *arg, FlashProp *prop); /* called after the children are drawn */
    /* 0xC8 */ void *overArg;
    /* 0xCC */ FlashTl tl;
} FlashClip; /* 0xE4 */

/* The sprite instances of a movie. */
typedef struct FlashClipList {
    /* 0x00 */ FlashClip *items;
    /* 0x04 */ u32 count;
    /* 0x08 */ u32 depths;       /* sum of the sprites' depth counts */
} FlashClipList; /* 0xC */

/* Flash.flags */
#define FLASH_PLAY 1        /* Flash_Advance runs the timelines */
#define FLASH_PAD 2         /* set by the action url "pad" "true", cleared by "pad" "false" */
#define FLASH_HIDE 4        /* Flash_Draw draws nothing */
#define FLASH_END 8         /* set by the action url "trigger" "end"; cleared by every Flash_Advance */

/* A movie instance (the animation object the menus and the HUD embed). */
typedef struct Flash {
    /* 0x00 */ u8 *data;         /* the tag list (file + 8) */
    /* 0x04 */ FlashTex **tex;   /* one per image record (tag 3); NULL: quads using it are not drawn */
    /* 0x08 */ u32 flags;        /* FLASH_ */
    /* 0x0C */ u32 trig;         /* bit n set by the action url "trig" "n" this tick */
    /* 0x10 */ u32 se;           /* bit n set by the action url "se" "n" this tick */
    /* 0x14 */ s32 speed;        /* frames per Flash_Advance */
    /* 0x18 */ s32 ofs[2];       /* screen offset added to every quad */
    /* 0x20 */ FlashTl *root;
    /* 0x24 */ FlashClipList *clips;
    /* 0x28 */ FlashShapePool *shapes;
} Flash; /* 0x2C */

/* What Flash_FindLabel returns: the first clip instance with a name and how many more share it. */
typedef struct FlashRef {
    /* 0x00 */ s32 index;        /* -1 when not found */
    /* 0x04 */ s32 more;
} FlashRef;

/* FlashQuad.flags */
#define FLASH_QUAD_FLIPX 0x01
#define FLASH_QUAD_FLIPY 0x02
#define FLASH_QUAD_REPEAT 0x04   /* texture wraps instead of clamping */
#define FLASH_QUAD_ADD 0x08
#define FLASH_QUAD_SUB 0x10
#define FLASH_QUAD_MASK 0x20
#define FLASH_QUAD_MASK_SET 0x40

/* One quad on its way to the GS. */
typedef struct FlashQuad {
    /* 0x00 */ u32 flags;        /* FLASH_QUAD_ */
    /* 0x04 */ f32 pt[4][2];     /* top left, top right, bottom left, bottom right */
    /* 0x24 */ FlashUv uv;
    /* 0x38 */ s32 rgba[4];
} FlashQuad; /* 0x48 */

/* One quad of a shape record (tag 4): after the 8-byte record header a u16 count, then these. */
typedef struct FlashShapeQuad {
    /* 0x00 */ s16 image;        /* record of tag 3, or negative for a flat quad */
    /* 0x02 */ u16 unk02;
    /* 0x04 */ u32 rgba;         /* r in the top byte */
    /* 0x08 */ s32 x0;
    /* 0x0C */ s32 x1;
    /* 0x10 */ s32 y0;
    /* 0x14 */ s32 y1;
} FlashShapeQuad; /* 0x18 */

void Flash_CxformIdentity(FlashCxform *cx);
void Flash_ReadCxform(u8 *data, FlashCxform *cx, s32 *ofs);
void Flash_Create(Flash *flash, u8 *file, FlashTex **tex);
void Flash_Destroy(Flash *flash);
void Flash_Advance(Flash *flash);
void Flash_Draw(Flash *flash);
void Flash_Reset(Flash *flash, s32 keepClips);
void Flash_Play(Flash *flash, s32 speed);
void Flash_Stop(Flash *flash);
void Flash_SetOffset(Flash *flash, s32 x, s32 y);
void Flash_SetFlag(Flash *flash, u32 mask, u8 on);
void Flash_GotoLabel(Flash *flash, char *label, u8 fromStart);
void Flash_StepFrames(Flash *flash, s32 step);
void Flash_FindLabel(Flash *flash, char *parent, char *name, FlashRef *out);
void Flash_ClipGotoLabel(Flash *flash, FlashRef *ref, char *label);
void Flash_ClipSetCallbackA(Flash *flash, FlashRef *ref, void *fn, void *arg);
void Flash_ClipSetCallbackB(Flash *flash, FlashRef *ref, void *fn, void *arg);
void Flash_ClipSetCallbackC(Flash *flash, FlashRef *ref, void *fn, void *arg);
s32 Flash_ClipSetFlags(Flash *flash, FlashRef *ref, s32 props, u8 on); /* returns nothing meaningful */
void Flash_ClipSetOffset(Flash *flash, FlashRef *ref, s32 x, s32 y);
void Flash_ClipSetScale(Flash *flash, FlashRef *ref, f32 x, f32 y);
void Flash_ClipSetAlpha(Flash *flash, FlashRef *ref, f32 alpha);
void Flash_ClipSetColor(Flash *flash, FlashRef *ref, f32 color);
void Flash_ClipSetTex(Flash *flash, FlashRef *ref, s32 tex);
void Flash_ClipSetUv(Flash *flash, FlashRef *ref, void *rect);
void Flash_ClipGetPos(Flash *flash, FlashRef *ref, s32 *x, s32 *y);
f32 Flash_ClipGetAlpha(Flash *flash, FlashRef *ref);

/* Flash_ClipSetFlags `props` bits. */
#define FLASH_PROP_PLAY 0x001
#define FLASH_PROP_VISIBLE 0x002
#define FLASH_PROP_FLIPX 0x004
#define FLASH_PROP_FLIPY 0x008
#define FLASH_PROP_NO_OFS 0x010
#define FLASH_PROP_ADD 0x020
#define FLASH_PROP_SUB 0x040
#define FLASH_PROP_MASK 0x080
#define FLASH_PROP_MASK_SET 0x100
#define FLASH_PROP_REPEAT 0x200
#define FLASH_PROP_FREE_RUN 0x400

#endif
