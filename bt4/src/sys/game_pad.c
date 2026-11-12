#include "common.h"
#include "sys/pad.h"
#include "sys/game_pad.h"

/* Translates gPad[pad].held (PAD_*) into the game button word (PADG_*), derives pressed/repeat, copies the sticks. */
void Pad_UpdateGameButtons(s32 pad) {
    Pad *p = &gPad[pad];
    u32 btn = 0;

    if (Pad_IsHeld(pad, PAD_LEFT)) {
        btn |= PADG_LEFT;
    }
    if (Pad_IsHeld(pad, PAD_RIGHT)) {
        btn |= PADG_RIGHT;
    }
    if (Pad_IsHeld(pad, PAD_DOWN)) {
        btn |= PADG_DOWN;
    }
    if (Pad_IsHeld(pad, PAD_UP)) {
        btn |= PADG_UP;
    }
    if (Pad_IsHeld(pad, PAD_RSTICK_LEFT)) {
        btn |= PADG_RSTICK_LEFT;
    }
    if (Pad_IsHeld(pad, PAD_RSTICK_RIGHT)) {
        btn |= PADG_RSTICK_RIGHT;
    }
    if (Pad_IsHeld(pad, PAD_RSTICK_DOWN)) {
        btn |= PADG_RSTICK_DOWN;
    }
    if (Pad_IsHeld(pad, PAD_RSTICK_UP)) {
        btn |= PADG_RSTICK_UP;
    }
    if (Pad_IsHeld(pad, PAD_CIRCLE)) {
        btn |= PADG_CIRCLE;
    }
    if (Pad_IsHeld(pad, PAD_CROSS)) {
        btn |= PADG_CROSS;
    }
    if (Pad_IsHeld(pad, PAD_TRIANGLE)) {
        btn |= PADG_TRIANGLE;
    }
    if (Pad_IsHeld(pad, PAD_SQUARE)) {
        btn |= PADG_SQUARE;
    }
    if (Pad_IsHeld(pad, PAD_START)) {
        btn |= PADG_START;
    }
    if (Pad_IsHeld(pad, PAD_SELECT)) {
        btn |= PADG_SELECT;
    }
    if (Pad_IsHeld(pad, PAD_L1)) {
        btn |= PADG_L1;
    }
    if (Pad_IsHeld(pad, PAD_L2)) {
        btn |= PADG_L2;
    }
    if (Pad_IsHeld(pad, PAD_R1)) {
        btn |= PADG_R1;
    }
    if (Pad_IsHeld(pad, PAD_R2)) {
        btn |= PADG_R2;
    }
    if (Pad_IsHeld(pad, PAD_L3)) {
        btn |= PADG_L3;
    }
    if (Pad_IsHeld(pad, PAD_R3)) {
        btn |= PADG_R3;
    }
    if (Pad_IsHeld(pad, 0)) {
        btn |= 0x100000;
    }
    if (Pad_IsHeld(pad, PAD_LEFT)) {
        btn |= PADG_LEFT2;
    }
    if (Pad_IsHeld(pad, PAD_RIGHT)) {
        btn |= PADG_RIGHT2;
    }
    if (Pad_IsHeld(pad, PAD_DOWN)) {
        btn |= PADG_DOWN2;
    }
    if (Pad_IsHeld(pad, PAD_UP)) {
        btn |= PADG_UP2;
    }

    p->gamePressed = btn & ~p->gameHeld;
    p->gameHeld = btn;
    gPad[pad].gameRepeat = Pad_CalcRepeat(btn, p->gamePressed, &p->gameRepeatTimer, &p->gameRepeatLast,
                                          p->repeatDelay, p->repeatInterval);
    Pad_GetSticks(pad, p->gameLeft, p->gameRight);
    if (p->status < 4) {
        p->lastStatus = p->status;
    }
}

/* Auto-repeat: returns pressed if any, else held each time the timer (delay, then interval) runs out. */
u32 Pad_CalcRepeat(u32 held, u32 pressed, s32 *timer, u32 *last, s32 delay, s32 interval) {
    u32 result = 0;

    if (held == *last && held != 0) {
        if (--*timer < 0) {
            *timer = interval;
            result = held;
        }
    } else {
        *last = held;
        *timer = delay;
    }
    if (pressed != 0) {
        result = pressed;
    }
    return result;
}

/* Sets the auto-repeat delay and interval (in frames) of both pads. */
void Pad_SetRepeat(s32 delay, s32 interval) {
    s32 i;

    for (i = 0; i < PAD_COUNT; i++) {
        gPad[i].repeatDelay = delay;
        gPad[i].repeatInterval = interval;
    }
}

/* Returns the port's connection status (0 = usable pad, PAD_STATUS_NONE = none). */
s32 Pad_GetStatus(s32 pad) {
    return gPad[pad].status;
}

/* Returns the last status below 4 the port had (in practice 0 once a pad was ever read). */
s32 Pad_GetLastStatus(s32 pad) {
    return gPad[pad].lastStatus;
}
