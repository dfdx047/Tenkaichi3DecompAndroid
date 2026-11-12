#ifndef BATTLE_COL_C_H
#define BATTLE_COL_C_H

#include "types.h"

/*
 * The text printer (0x239EA0..0x23C310) and its inline icon tags (0x23C310..0x23D1E8).
 * The file stem is historical: the range was expected to be collision code. Nothing here is
 * simulation; everything ends in GS packets.
 */

#define FONT_SLOT_COUNT 7
#define FONT_STACK_DEPTH 5

/* Kinds of a queued print command. */
#define FONT_CMD_STR16 0   /* 16-bit characters, 0-terminated; the only kind that expands tags */
#define FONT_CMD_STR8 1    /* 8-bit characters */
#define FONT_CMD_CHAR16 2  /* one character */
#define FONT_CMD_CHAR8 3   /* one character (passed as a signed byte) */
#define FONT_CMD_NUMBER 4  /* decimal number with a fixed digit count */

/* FontStyle.align */
#define FONT_ALIGN_LEFT 0
#define FONT_ALIGN_CENTER 1
#define FONT_ALIGN_RIGHT 2

/* FontStyle.shadowMode */
#define FONT_SHADOW_NONE 0
#define FONT_SHADOW_DROP 1     /* one copy at (+dx, +dy) */
#define FONT_SHADOW_OUTLINE8 2 /* eight copies around the glyph */
#define FONT_SHADOW_OUTLINE4 3 /* four diagonal copies */

/* A glyph of a font: 6 bytes, sorted by code for the binary search. */
typedef struct FontGlyph {
    /* 0x00 */ u16 code;
    /* 0x02 */ u16 u; /* bits 0..9 texel x, bits 10..14 width, bit 15 low bit of the CLUT index */
    /* 0x04 */ u16 v; /* bits 0..9 texel y, bits 10..14 height, bit 15 high bit of the CLUT index */
} FontGlyph;

/* The texture header a font or icon sheet points at (only what this module reads). */
typedef struct FontTex {
    /* 0x00 */ s32 pixels;  /* offset, then pointer */
    /* 0x04 */ s32 clut;    /* offset, then pointer */
    /* 0x08 */ u8 unk08[0x18];
    /* 0x20 */ s32 tbp;     /* icon sheets: texture base (blocks) */
    /* 0x24 */ s32 cbp;     /* icon sheets: CLUT base (blocks) */
    /* 0x28 */ u8 unk28[8];
    /* 0x30 */ u64 tex0;    /* GS TEX0 without the base pointers */
    /* 0x38 */ s32 pixelsPtr;
    /* 0x3C */ s32 clutPtr;
} FontTex;

/* A font file (members 1 and 2 of the boot file). */
typedef struct FontData {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ FontTex *tex;      /* offset in the file, made a pointer by Font_SetFont */
    /* 0x10 */ s32 unk10;
    /* 0x14 */ FontGlyph *glyphs; /* offset in the file, made a pointer by Font_SetFont */
    /* 0x18 */ s32 glyphCount;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 lineHeight;
} FontData;

/* One of the seven font slots (0x14 bytes). */
typedef struct FontSlot {
    /* 0x00 */ FontData *data; /* NULL: slot unused */
    /* 0x04 */ u32 tbp;        /* 0x2A08 + slot * 0x208 */
    /* 0x08 */ s32 cbp;        /* 0x2A00 + slot * 0x208 */
    /* 0x0C */ s32 unk0C;      /* data->unk1C */
    /* 0x10 */ s32 lineHeight; /* data->lineHeight */
} FontSlot;

/* Home and current print position (0x10 bytes at 0x31C1C0). */
typedef struct FontCursor {
    /* 0x00 */ s32 homeX;
    /* 0x04 */ s32 homeY;
    /* 0x08 */ s32 x;
    /* 0x0C */ s32 y;
} FontCursor;

struct FontCmd;

/* Called once per queued command when the queue is flushed; returns the VRAM blocks it used. */
typedef s32 (*FontBeginFn)(s32 vramBase);
/* Called in front of every character of a 16-bit string while it is drawn; it may consume a tag. */
typedef void (*FontTagFn)(u64 **pkt, struct FontCmd *cmd, struct FontCmd *saved, s32 *x,
                          s32 *y, u16 **str, s32 vramBase, s32 draw);

/* The current style (0x48 bytes at 0x31C1D0); copied into every command. */
typedef struct FontStyle {
    /* 0x00 */ f32 scaleX;
    /* 0x04 */ f32 scaleY;
    /* 0x08 */ f32 scale;       /* multiplies both */
    /* 0x0C */ u32 color;       /* RGBA, 0x80 = opaque */
    /* 0x10 */ u32 color2;      /* copied to the command, never read by the printer */
    /* 0x14 */ s32 spacingX;    /* extra pixels after each glyph */
    /* 0x18 */ s32 spacingY;    /* extra pixels between lines */
    /* 0x1C */ u32 align;       /* FONT_ALIGN_* */
    /* 0x20 */ s32 flags;       /* bit 0: stop at the first line break */
    /* 0x24 */ u32 shadowMode;  /* FONT_SHADOW_* */
    /* 0x28 */ u32 shadowColor;
    /* 0x2C */ u16 shadowDx;
    /* 0x2E */ u16 shadowDy;
    /* 0x30 */ u16 clipX0;
    /* 0x32 */ u16 clipY0;
    /* 0x34 */ u16 clipX1;
    /* 0x36 */ u16 clipY1;
    /* 0x38 */ s32 userFlags;
    /* 0x3C */ FontBeginFn begin;
    /* 0x40 */ FontTagFn tag;
    /* 0x44 */ void *tagArg;
} FontStyle;

/* A queued print command (0x54 bytes). */
typedef struct FontCmd {
    /* 0x00 */ s32 kind;        /* FONT_CMD_* */
    /* 0x04 */ s32 arg;         /* string pointer, character or number */
    /* 0x08 */ s16 x;
    /* 0x0A */ s16 y;
    /* 0x0C */ s16 homeX;       /* where a new line restarts */
    /* 0x0E */ s16 digits;
    /* 0x10 */ f32 scaleX;      /* style.scaleX * style.scale */
    /* 0x14 */ f32 scaleY;
    /* 0x18 */ u16 spacingX;
    /* 0x1A */ u16 spacingY;
    /* 0x1C */ u32 color;
    /* 0x20 */ u32 color2;
    /* 0x24 */ u32 shadowMode;
    /* 0x28 */ u32 shadowColor;
    /* 0x2C */ u16 shadowDx;
    /* 0x2E */ u16 shadowDy;
    /* 0x30 */ u16 clipX0;
    /* 0x32 */ u16 clipY0;
    /* 0x34 */ u16 clipX1;
    /* 0x36 */ u16 clipY1;
    /* 0x38 */ s32 userFlags;
    /* 0x3C */ u32 align;
    /* 0x40 */ s32 flags;
    /* 0x44 */ FontBeginFn begin;
    /* 0x48 */ FontTagFn tag;
    /* 0x4C */ s32 vramBase;    /* -1 until the flush hands the begin callback its VRAM base */
    /* 0x50 */ void *tagArg;
} FontCmd;

/* Printer entry points used by the icon tags (col_c_b.c). */
s32 Font_DrawCharNow(u64 **pkt, FontCmd *cmd, s32 *x, s32 *y, u16 ch);
void Font_SetBeginCallback(FontBeginFn fn);
void Font_SetTagCallback(FontTagFn fn, void *arg);

/*
 * Inline icons (0x23C310..0x23D1E8): controller button pictures drawn inside 16-bit strings.
 */

#define FONT_ICON_PAD_TYPES 4

/* The characters that stand for an icon inside a tag argument (0xC bytes). */
typedef struct FontIconName {
    /* 0x00 */ u16 chars[4];
    /* 0x08 */ s32 len;
} FontIconName;

/* An icon picture (0xC bytes). */
typedef struct FontIconDef {
    /* 0x00 */ u8 tex;        /* index of its texture header in the icon file */
    /* 0x01 */ u8 size;       /* cell size in texels: 0x20, 0x40 or 0x80 */
    /* 0x02 */ u8 advance;    /* width in pixels at scale 1 */
    /* 0x03 */ u8 unk03;
    /* 0x04 */ u16 frameCount;
    /* 0x06 */ s8 frameTime[6]; /* ticks each frame is shown */
} FontIconDef;

/* One row of a controller type's icon table (0xC bytes; the table ends with name == NULL). */
typedef struct FontIconEntry {
    /* 0x00 */ FontIconName *name;
    /* 0x04 */ FontIconDef *icon;
    /* 0x08 */ f32 scale;
} FontIconEntry;

/* The icon file (member 8 of the boot file), as far as it is read here. */
typedef struct FontIconRes {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ FontTex *tex; /* array of 0x40-byte texture headers */
} FontIconRes;

struct FontTagDef;
typedef void (*FontTagHandler)(struct FontTagDef *def, u64 **pkt, FontCmd *cmd, FontCmd *saved, s32 *x,
                               s32 *y, u16 **str, s32 vramBase, s32 draw);

/* A tag "<NAME...>" (0x10 bytes; four of them at 0x2C6450). */
typedef struct FontTagDef {
    /* 0x00 */ u16 *name;
    /* 0x04 */ u16 len;
    /* 0x08 */ FontTagHandler handler;
    /* 0x0C */ s32 hasArg; /* 1: the name is followed by '=', else by '>' */
} FontTagDef;

#endif
