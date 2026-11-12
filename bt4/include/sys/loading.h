#ifndef SYS_LOADING_H
#define SYS_LOADING_H

#include "types.h"

/* Which of the three button-mashing loading screens is shown (LoadScreen.type; file id 0x3C1 + type). */
#define LOAD_TYPE_0 0 /* two-frame animation; a 0..100 counter shown as rows of pips */
#define LOAD_TYPE_1 1 /* pulsing sprite; a 0..999 counter shown as digits */
#define LOAD_TYPE_2 2 /* 30 scattered items picked off one at a time */
#define LOAD_TYPE_COUNT 3

#define LOAD_FILE_FIRST 0x3C1 /* BPE-packed sprite sheet of loading screen type 0 */
#define LOAD_PACK_MAX 0x3000 /* third argument of File_LoadSync (which ignores it) */

#define LOAD_ITEM_COUNT 30
#define LOAD_SPRITE_COUNT 16

/* LoadSprite.flags */
#define LOAD_SPR_DRAW 1   /* textured quad; 0 = skip this entry */
#define LOAD_SPR_END 2    /* end of a sprite run */
#define LOAD_SPR_CLEAR 5  /* full-screen quad in a flat colour (drawn with a NULL resource) */

/* LoadScreen.flags */
#define LOAD_FLAG_TAKEN 1   /* type 2: the current item was just collected */
#define LOAD_FLAG_PRESSED 2 /* the button was pressed this frame */

#define LOAD_BUTTON 0x200 /* bit of Pad.gamePressed that drives the animation */

#define PROGRESS_FLAG_LOADING 0x4000 /* set in Progress.flags while Load_RunBlocking runs */

/* One entry of the list drawn by Sprite_DrawList (0x38 bytes). */
typedef struct LoadSprite {
    /* 0x00 */ s32 flags; /* LOAD_SPR_* */
    /* 0x04 */ s32 x0;
    /* 0x08 */ s32 y0;
    /* 0x0C */ s32 x1;
    /* 0x10 */ s32 y1;
    /* 0x14 */ s32 u0;
    /* 0x18 */ s32 v0;
    /* 0x1C */ s32 u1;
    /* 0x20 */ s32 v1;
    /* 0x24 */ s32 tex;   /* index of the 0x40-byte texture entry in the resource */
    /* 0x28 */ s32 r;
    /* 0x2C */ s32 g;
    /* 0x30 */ s32 b;
    /* 0x34 */ s32 a;
} LoadSprite;

/* Type 2: the words of one scattered item (LoadScreen.items[i]). */
#define LOAD_ITEM_FLAGS 0 /* bit 0: already collected */
#define LOAD_ITEM_X 1
#define LOAD_ITEM_Y 2
#define LOAD_ITEM_WORDS 3

/* Screen rectangle (the x0..y1 words of a LoadSprite). */
typedef struct LoadRect {
    /* 0x00 */ s32 x0;
    /* 0x04 */ s32 y0;
    /* 0x08 */ s32 x1;
    /* 0x0C */ s32 y1;
} LoadRect;

/* Type 1: the sprite that grows and shrinks on a sine wave. */
typedef struct LoadPulse {
    /* 0x00 */ s32 rect[4]; /* resting x0, y0, x1, y1 */
    /* 0x10 */ f32 size;    /* current outward offset of every edge */
    /* 0x14 */ f32 angle;   /* wraps from pi to -pi */
    /* 0x18 */ f32 amp;
    /* 0x1C */ f32 speed;
} LoadPulse;

/* State of the loading screen (gLoadScreen, 0x1AC bytes).
   Declared as a union around the real struct on purpose: the original object code reloads every field after
   any store, i.e. it was compiled without type-based alias analysis (-fno-strict-aliasing). Under the
   project's plain -O2 the union gives the same effect (member accesses get alias set 0). Compiled with
   -fno-strict-aliasing the plain struct matches as well and the union can go. */
typedef union LoadScreen {
    struct {
        /* 0x000 */ s32 frames;       /* counted only by Load_RunFileQueue */
        /* 0x004 */ u32 timer;        /* frames since the last animation step */
        /* 0x008 */ u16 period;       /* animation steps by itself when timer exceeds this */
        /* 0x00A */ u16 count;        /* the player's score */
        /* 0x00C */ s16 presses;      /* button presses left until the next step */
        /* 0x00E */ u16 frame;        /* animation frame, 0 or 1 */
        /* 0x010 */ u16 cycles;       /* full animation cycles left until count goes up */
        /* 0x012 */ u16 type;         /* LOAD_TYPE_* */
        /* 0x014 */ s32 target;       /* type 2: index of the item being worked on */
        /* 0x018 */ void *res;        /* unpacked sprite sheet */
        /* 0x01C */ LoadSprite *sprites; /* LOAD_SPRITE_COUNT entries, = gProgress->loadSprites */
        /* 0x020 */ s32 items[LOAD_ITEM_COUNT * LOAD_ITEM_WORDS]; /* type 2: {flags, x, y} per item */
        /* 0x188 */ s32 flags;        /* LOAD_FLAG_* */
        /* 0x18C */ LoadPulse pulse;  /* type 1 */
    };
    s32 words[0x1AC / 4];
} LoadScreen;

/* The part of the 0x7FC-byte progress struct (gProgress) this module touches. */
typedef struct Progress {
    /* 0x00 */ s32 unk0[2];
    /* 0x08 */ void *loadPack;       /* buffer the packed loading-screen file is read into */
    /* 0x0C */ void *loadRes;        /* buffer it is unpacked into */
    /* 0x10 */ LoadSprite *loadSprites; /* room for LOAD_SPRITE_COUNT sprites */
    /* 0x14 */ s32 flags;
    /* 0x18 */ s32 mode;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 lastLoadType;     /* type of the previous loading screen + 1, so it is not repeated */
} Progress;

extern LoadScreen gLoadScreen;
extern Progress *gProgress;

void Load_RunBlocking(void);
void Load_BuildSprites(void);
void Load_InitScreen(void);
void Load_UpdateScreen(void);
void Load_ReadInput(void);
void Load_DrawScreen(void);
void Load_RunFileQueue(void);

#endif
