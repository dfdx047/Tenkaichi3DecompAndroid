#ifndef SYS_COLOR_FADE_H
#define SYS_COLOR_FADE_H

#include "types.h"

/*
 * A full-screen colour overlay with a linear integer alpha ramp. Source range 0x126B88-0x126EC8
 * (src/sys/color_fade.c). It is separate from the float fades of sys/fade.h and has one global slot.
 * Callers are all in 0x2BE990..0x2BF588 (not decompiled), which call ColorFade_Update and ColorFade_Draw
 * themselves: nothing in the frame code draws it.
 */

#define COLOR_FADE_MAX 0x80 /* opaque for the GS */

/* ColorFade.flags */
#define COLOR_FADE_IN 1       /* alpha is going down: the screen is being uncovered */
#define COLOR_FADE_OUT 2      /* alpha is going up: the screen is being covered */
#define COLOR_FADE_IN_DONE 4  /* a fade in ended (alpha <= 0) */
#define COLOR_FADE_OUT_DONE 8 /* a fade out ended (alpha >= 0x80) */

typedef struct ColorFade {
    /* 0x00 */ s32 flags;
    /* 0x04 */ u8 r;
    /* 0x05 */ u8 g;
    /* 0x06 */ u8 b;
    /* 0x07 */ u8 a;     /* alpha clamped to 0..0x80, written by ColorFade_Draw */
    /* 0x08 */ s32 alpha;
    /* 0x0C */ s32 step; /* 0x80 / frames */
} ColorFade; /* size 0x10 */

extern ColorFade gColorFade;

void ColorFade_Draw(void);
void ColorFade_Update(void);
void ColorFade_StartIn(s32 r, s32 g, s32 b, s32 frames);
void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames);
s32 ColorFade_IsInDone(void);
s32 ColorFade_IsOutDone(void);
s32 ColorFade_IsFadingIn(void);
s32 ColorFade_IsFadingOut(void);

#endif
