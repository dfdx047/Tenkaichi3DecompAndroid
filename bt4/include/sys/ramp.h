#ifndef SYS_RAMP_H
#define SYS_RAMP_H

#include "types.h"

/*
 * Linear ramp / frame timer. Source range 0x267AB8-0x267B90.
 *
 * Ramp_Start(ramp, seconds, from, to) makes `value` go from `from` to `to` in seconds * 30 steps
 * (the game logic runs at 30 steps per second of ramp time; one Ramp_Step per frame).
 * Ramp_Step adds `step` to `value` once per call. There is no easing: the change is linear, value
 * is clamped so it never passes `target`, and on the call where the frame counter reaches zero
 * value is set to exactly `target` and 1 is returned. Every later call keeps returning 1 (the
 * counter keeps decreasing; value stays at target). A stopped ramp (Ramp_Stop, state -1) also
 * returns 1 and leaves value alone.
 *
 * With the usual (from, to) = (0, 1) the struct is used both as an interpolation factor (screen
 * fades: colour = from + delta * value) and as a plain time-out (battle sequence: wait until
 * Ramp_Step returns 1).
 *
 * This is the single definition: FadeRamp (sys/fade.h) and BtlSeqTimer (battle/btl_seq.h) are
 * typedefs of it.
 */

#define RAMP_FPS 30.0f

#define RAMP_RUNNING 0
#define RAMP_STOPPED (-1)

typedef struct Ramp {
    /* 0x00 */ s32 unk0;   /* never touched by the three functions */
    /* 0x04 */ s32 state;  /* RAMP_RUNNING after Ramp_Start, RAMP_STOPPED after Ramp_Stop */
    /* 0x08 */ f32 frames; /* steps left: seconds * 30, minus 1 per Ramp_Step (goes negative after the end) */
    /* 0x0C */ f32 step;   /* (to - from) / frames */
    /* 0x10 */ f32 value;  /* current value: from -> to */
    /* 0x14 */ f32 target; /* to */
} Ramp; /* size 0x18 */

void Ramp_Stop(Ramp *ramp);
void Ramp_Start(Ramp *ramp, f32 seconds, f32 from, f32 to);
s32 Ramp_Step(Ramp *ramp);

#endif
