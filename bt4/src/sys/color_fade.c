#include "common.h"
#include "sys/color_fade.h"
#include "sys/dma.h"
#include "sys/gfx.h"

extern void *memset(void *dst, s32 c, u32 n);
extern void LipSync_Clear(void); /* clears the 0x14 bytes at 0x31E570 (not decompiled) */

/* Queues the overlay as one blended 512x448 sprite, unless it is fully transparent. */
void ColorFade_Draw(void) {
    ColorFade *fade = &gColorFade;
    u64 *p;

    fade->a = fade->alpha > COLOR_FADE_MAX ? COLOR_FADE_MAX : (fade->alpha < 0 ? 0 : fade->alpha);
    if (gColorFade.a != 0) {
        Gfx_AddDefaultEnv();
        p = Dma_BeginDirect();
        p[0] = GIF_TAG(2, 1, 1);
        p[1] = GIF_REG_AD;
        p += 2;
        p[0] = 0x44;
        p[1] = GS_ALPHA_1;
        p += 2;
        p[0] = 0x30000;
        p[1] = GS_TEST_1;
        p += 2;
        p[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 4);
        p[1] = 0x5510;
        p += 2;
        p[0] = 0x46;
        p[1] = (u64)gColorFade.r | ((u64)gColorFade.g << 8) | ((u64)gColorFade.b << 16) | ((u64)gColorFade.a << 24);
        p += 2;
        p[0] = 0x72007000;
        p[1] = 0x8E009000;
        p += 2;
        Dma_EndDirect(p);
    }
}

/* Steps the alpha ramp once; call every frame. */
void ColorFade_Update(void) {
    ColorFade *fade = &gColorFade;

    if (fade->flags & COLOR_FADE_IN) {
        if (fade->alpha > 0) {
            fade->alpha -= fade->step;
        } else {
            fade->flags = COLOR_FADE_IN_DONE;
        }
    }
    if (gColorFade.flags & COLOR_FADE_OUT) {
        if (gColorFade.alpha < gColorFade.step + COLOR_FADE_MAX) {
            gColorFade.alpha += gColorFade.step;
        } else {
            gColorFade.flags = COLOR_FADE_OUT_DONE;
        }
    }
}

/* Starts uncovering the screen from the given colour over `frames` frames. */
void ColorFade_StartIn(s32 r, s32 g, s32 b, s32 frames) {
    memset(&gColorFade, 0, sizeof(ColorFade));
    gColorFade.flags = COLOR_FADE_IN;
    gColorFade.r = r;
    gColorFade.g = g;
    gColorFade.b = b;
    gColorFade.alpha = COLOR_FADE_MAX;
    gColorFade.step = COLOR_FADE_MAX / frames;
    LipSync_Clear();
}

/* Starts covering the screen with the given colour over `frames` frames. */
void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames) {
    memset(&gColorFade, 0, sizeof(ColorFade));
    gColorFade.flags = COLOR_FADE_OUT;
    gColorFade.r = r;
    gColorFade.g = g;
    gColorFade.b = b;
    gColorFade.alpha = 0;
    gColorFade.step = COLOR_FADE_MAX / frames;
}

/* True once a fade in has ended. */
s32 ColorFade_IsInDone(void) {
    return (gColorFade.flags >> 2) & 1;
}

/* True once a fade out has ended. */
s32 ColorFade_IsOutDone(void) {
    return (gColorFade.flags >> 3) & 1;
}

/* True while a fade in is running. */
s32 ColorFade_IsFadingIn(void) {
    return gColorFade.flags & 1;
}

/* True while a fade out is running. */
s32 ColorFade_IsFadingOut(void) {
    return (gColorFade.flags >> 1) & 1;
}
