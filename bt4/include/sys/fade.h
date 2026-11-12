#ifndef SYS_FADE_H
#define SYS_FADE_H

#include "types.h"
#include "sys/ramp.h"

/*
 * Screen fades. Source range 0x2527B0-0x252F68.
 *
 * gFade holds FADE_COUNT (3) slots of 0x60 bytes. A slot is a full-screen rectangle whose colour is
 * interpolated between the two entries of gFadeColors[slot]:
 *   slot 0  black  (0,0,0)        drawn by Fade_DrawScreen  boot, loading screen, movies, battle end
 *   slot 1  white  (255,255,255)  drawn by Fade_DrawScreen
 *   slot 2  white  (255,255,255)  drawn by Fade_DrawSlot2 (from inside the battle scene's own drawing,
 *                                 so below whatever is drawn after it); battle stage change
 * Only alpha differs between the two colours of a slot: 0 (clear) and 128 (opaque for the GS).
 *
 * Fade_Start(slot, dir, seconds): FADE_OUT ramps clear -> opaque, FADE_IN opaque -> clear; any other
 * `dir` keeps the slot's flags and colours and only restarts the ramp. The ramp (0x267AC8) runs
 * from 0 to 1 in seconds * 30 frames, and Fade_Start already performs the first step.
 *
 * Per frame (Fade_UpdateAll, called by Gfx_BeginFrame), for each slot that is on and not paused:
 *   color = from + (to - from) * ramp.value        (so the colour is one step behind the ramp)
 *   if FADE_FLAG_DONE is not set: step the ramp (0x267B00); when it reports the end, set FADE_FLAG_DONE
 *   else if (s32)alpha == 0: turn the slot off (a finished fade in)
 * A finished fade out stays on and keeps covering the screen until Fade_Reset / Fade_ResetAll or a
 * fade in. Fade_IsDone() is true from the frame FADE_FLAG_DONE is set, and for a slot that is off.
 *
 * Drawing (Fade_DrawRect): one Dma_BeginDirect / Dma_EndDirect packet: the common 2D state of
 * 0x102208, then TEST, ZBUF, PRIM (alpha-blended sprite) and RGBAQ, then 16 sprites of 32x448 pixels.
 * Fade_DrawScreen is called by Gfx_EndFrame, so slots 0 and 1 cover everything drawn in the frame.
 */

#define FADE_COUNT 3

/* Fade_Start direction. Any other value restarts the ramp between the colours already in the slot. */
#define FADE_OUT 0 /* screen -> slot colour (alpha 0 -> 128) */
#define FADE_IN 1  /* slot colour -> screen (alpha 128 -> 0) */

/* Fade.flags */
#define FADE_FLAG_OUT 1    /* started with FADE_OUT */
#define FADE_FLAG_IN 2     /* started with FADE_IN */
#define FADE_FLAG_DONE 4   /* the ramp reached its end */
#define FADE_FLAG_PAUSED 8 /* Fade_Update skips the slot (it is still drawn) */

/* The slot's linear ramp (sys/ramp.h): value goes 0 -> 1 over the fade. */
typedef Ramp FadeRamp;

/* One full-screen colour overlay. Colours are r, g, b, a as floats (0-255, alpha 0-128). */
typedef struct Fade {
    /* 0x00 */ f32 color[4]; /* current colour: from + delta * ramp.value */
    /* 0x10 */ f32 from[4];
    /* 0x20 */ f32 to[4];
    /* 0x30 */ f32 delta[4]; /* to - from */
    /* 0x40 */ FadeRamp ramp;
    /* 0x58 */ s32 flags;    /* 0: slot off */
    /* 0x5C */ s32 unk5C;
} Fade; /* size 0x60 */

/* The two end colours of a slot. */
typedef struct FadeColors {
    /* 0x00 */ f32 clear[4];  /* alpha 0 */
    /* 0x10 */ f32 opaque[4]; /* alpha 128 */
} FadeColors; /* size 0x20 */

extern Fade gFade[FADE_COUNT];
extern FadeColors gFadeColors[FADE_COUNT];

void Fade_DrawRect(Fade *fade);
void FadeSlot_Reset(Fade *fade);
void FadeSlot_SetPause(Fade *fade, s32 pause);
void FadeSlot_CalcColor(Fade *fade);
s32 FadeSlot_IsPaused(Fade *fade);
s32 FadeSlot_IsActive(Fade *fade);
s32 FadeSlot_IsDone(Fade *fade);
void FadeSlot_Update(Fade *fade);
void FadeSlot_Draw(Fade *fade);
void Fade_Init(void);
void Fade_Term(void);
void Fade_ResetAll(void);
void Fade_Reset(s32 idx);
void Fade_PauseAll(s32 pause);
void Fade_Pause(s32 idx, s32 pause);
s32 Fade_IsPaused(s32 idx);
s32 Fade_IsActive(s32 idx);
s32 Fade_IsDone(s32 idx);
void Fade_Start(s32 idx, s32 dir, f32 seconds);
void Fade_UpdateAll(void);
void Fade_DrawScreen(void);
void Fade_DrawSlot2(void);

#endif
