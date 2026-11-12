#include "common.h"
#include "sys/ramp.h"

/* Stops the ramp: Ramp_Step reports "ended" without changing the value. */
void Ramp_Stop(Ramp *ramp) {
    ramp->state = RAMP_STOPPED;
}

/* Starts a linear ramp from `from` to `to` lasting seconds * 30 steps. */
void Ramp_Start(Ramp *ramp, f32 seconds, f32 from, f32 to) {
    ramp->state = RAMP_RUNNING;
    ramp->frames = seconds * RAMP_FPS;
    ramp->value = from;
    ramp->target = to;
    ramp->step = (to - from) / ramp->frames;
}

/* Advances the ramp by one step; returns 1 when it has ended (or is stopped), else 0. */
s32 Ramp_Step(Ramp *ramp) {
    if (ramp->state == RAMP_STOPPED) {
        return 1;
    }
    ramp->frames -= 1.0f;
    if (ramp->frames <= 0.0001f) {
        ramp->value = ramp->target;
        return 1;
    }
    ramp->value += ramp->step;
    if (0.0f < ramp->step) {
        if (ramp->target < ramp->value) {
            ramp->value = ramp->target;
        }
    } else {
        if (ramp->value < ramp->target) {
            ramp->value = ramp->target;
        }
    }
    return 0;
}
