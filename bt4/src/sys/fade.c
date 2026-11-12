#include "common.h"
#include "sys/dma.h"
#include "sys/fade.h"

extern void *memset(void *dst, s32 c, u32 n);
extern void Vec4_Copy(f32 *dst, f32 *src);
extern void Vec4_Add(f32 *dst, f32 *a, f32 *b);
extern void Vec4_Sub(f32 *dst, f32 *a, f32 *b);
extern void Vec4_Scale(f32 *dst, f32 *src, f32 scale);
extern void Gfx_PutDefaultEnv(u64 **pp);  /* appends the common 2D GS state (13 quadwords) at *pp and advances it */

/* The rectangle in GS primitive coordinates: the 512x448 screen centred on 2048,2048. */
#define FADE_X 1792
#define FADE_Y 1824
#define FADE_H 448
#define FADE_STRIPS 16

/* Queues the slot's colour as a blended 512x448 rectangle (16 sprites 32 pixels wide). */
void Fade_DrawRect(Fade *fade) {
    u64 *p;
    u32 rgba;
    s32 i;

    p = Dma_BeginDirect();
    Gfx_PutDefaultEnv(&p);
    rgba = ((u32)fade->color[0] & 0xFF) | (((u32)fade->color[1] & 0xFF) << 8) | (((u32)fade->color[2] & 0xFF) << 16) |
           ((u32)fade->color[3] << 24);
    p[0] = GIF_TAG(4, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x30000; /* alpha and depth tests always pass */
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1); /* no depth writes */
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = 0x46; /* sprite, alpha blended */
    p[1] = GS_PRIM;
    p += 2;
    p[0] = (u64)rgba | (0x3F800000ULL << 32);
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = GIF_TAG_EX(FADE_STRIPS, 1, GIF_FLG_REGLIST, 2);
    p[1] = GS_REG_XYZ2 | (GS_REG_XYZ2 << 4);
    p += 2;
    for (i = 0; i < FADE_STRIPS; i++) {
        p[0] = ((i * 32 + FADE_X) << 4) | ((FADE_Y << 4) << 16);
        p[1] = GS_SET_XYZ((i * 32 + FADE_X + 32) << 4, (FADE_Y + FADE_H) << 4, 0);
        p += 2;
    }
    Dma_EndDirect(p);
}

/* Turns a slot off and stops its ramp. */
void FadeSlot_Reset(Fade *fade) {
    fade->flags = 0;
    Ramp_Stop(&fade->ramp);
}

/* Sets or clears the pause flag of a slot that is on. */
void FadeSlot_SetPause(Fade *fade, s32 pause) {
    if (FadeSlot_IsActive(fade)) {
        if (pause) {
            fade->flags |= FADE_FLAG_PAUSED;
        } else {
            fade->flags &= ~FADE_FLAG_PAUSED;
        }
    }
}

/* color = from + delta * ramp value. */
void FadeSlot_CalcColor(Fade *fade) {
    Vec4_Scale(fade->color, fade->delta, fade->ramp.value);
    Vec4_Add(fade->color, fade->color, fade->from);
}

/* Returns the pause flag of a slot. */
s32 FadeSlot_IsPaused(Fade *fade) {
    return (fade->flags >> 3) & 1;
}

/* Returns 1 when the slot is on (fading or holding its end colour). */
s32 FadeSlot_IsActive(Fade *fade) {
    return fade->flags != 0;
}

/* Returns 1 when the slot's ramp has ended or the slot is off. */
s32 FadeSlot_IsDone(Fade *fade) {
    s32 done = 0;

    if (fade->flags & FADE_FLAG_DONE || fade->flags == 0) {
        done = 1;
    }
    return done;
}

/* One frame of a slot: recomputes the colour, steps the ramp, and turns the slot off once it has ended transparent. */
void FadeSlot_Update(Fade *fade) {
    if (FadeSlot_IsActive(fade) && !FadeSlot_IsPaused(fade)) {
        FadeSlot_CalcColor(fade);
        if (fade->flags & FADE_FLAG_DONE) {
            if ((s32)fade->color[3] == 0) {
                FadeSlot_Reset(fade);
            }
        } else if (Ramp_Step(&fade->ramp)) {
            fade->flags |= FADE_FLAG_DONE;
        }
        do {
        } while (0); /* needed: an empty statement here keeps FadeSlot_Reset from becoming a tail call */
    }
}

/* Draws a slot if it is on. */
void FadeSlot_Draw(Fade *fade) {
    if (FadeSlot_IsActive(fade)) {
        Fade_DrawRect(fade);
    }
}

/* Boot-time set-up: clears the first slot and turns every slot off. */
void Fade_Init(void) {
    memset(gFade, 0, sizeof(Fade));
    Fade_ResetAll();
}

/* Empty. */
void Fade_Term(void) {
}

/* Turns every slot off. */
void Fade_ResetAll(void) {
    s32 i;

    for (i = 0; i < FADE_COUNT; i++) {
        FadeSlot_Reset(&gFade[i]);
    }
}

/* Turns one slot off. */
void Fade_Reset(s32 idx) {
    FadeSlot_Reset(&gFade[idx]);
}

/* Pauses or resumes every slot that is on. */
void Fade_PauseAll(s32 pause) {
    s32 i;

    for (i = 0; i < FADE_COUNT; i++) {
        FadeSlot_SetPause(&gFade[i], pause);
    }
}

/* Pauses or resumes one slot. */
void Fade_Pause(s32 idx, s32 pause) {
    FadeSlot_SetPause(&gFade[idx], pause);
}

/* Returns the pause flag of one slot. */
s32 Fade_IsPaused(s32 idx) {
    return FadeSlot_IsPaused(&gFade[idx]);
}

/* Returns 1 when the slot is on. */
s32 Fade_IsActive(s32 idx) {
    return FadeSlot_IsActive(&gFade[idx]);
}

/* Returns 1 when the slot's fade has ended or the slot is off. */
s32 Fade_IsDone(s32 idx) {
    return FadeSlot_IsDone(&gFade[idx]);
}

/* Starts a fade of `seconds` on a slot: FADE_OUT covers the screen with the slot's colour, FADE_IN uncovers it. */
void Fade_Start(s32 idx, s32 dir, f32 seconds) {
    Fade *fade = &gFade[idx];

    switch (dir) {
    case FADE_OUT:
        fade->flags = FADE_FLAG_OUT;
        Vec4_Copy(fade->from, gFadeColors[idx].clear);
        Vec4_Copy(fade->to, gFadeColors[idx].opaque);
        break;
    case FADE_IN:
        fade->flags = FADE_FLAG_IN;
        Vec4_Copy(fade->from, gFadeColors[idx].opaque);
        Vec4_Copy(fade->to, gFadeColors[idx].clear);
        break;
    }
    Vec4_Sub(fade->delta, fade->to, fade->from);
    Ramp_Start(&fade->ramp, seconds, 0.0f, 1.0f);
    FadeSlot_Update(fade);
}

/* Steps every slot; once per frame. */
void Fade_UpdateAll(void) {
    s32 i;

    for (i = 0; i < FADE_COUNT; i++) {
        FadeSlot_Update(&gFade[i]);
    }
}

/* Draws slots 0 and 1 over the finished frame. */
void Fade_DrawScreen(void) {
    Fade *fade = gFade;

    FadeSlot_Draw(fade);
    fade++;
    FadeSlot_Draw(fade);
    fade++; /* needed: without a statement after the call it becomes a tail call */
}

/* Draws slot 2. */
void Fade_DrawSlot2(void) {
    FadeSlot_Draw(&gFade[2]);
}
