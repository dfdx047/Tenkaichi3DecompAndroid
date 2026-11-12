#include "common.h"
#include "sys/timer.h"

/*
 * EE hardware timers 0 and 1 as a stopwatch, 0x11F190..0x11F548.
 *
 * The timer counts horizontal blanks and clears itself at 0xFFFF (ZRET). The wrap is not handled by an interrupt:
 * each read checks the "compare reached" flag, adds 0xFFFF to a software count and clears the flag, so a timer has
 * to be read at least once per 65535 lines (about 4.2 s). Elapsed time is lines * 1000000 / 15734 microseconds.
 * Only the movie player uses it (Movie_Run and 0x1263D8).
 */

extern u32 gTimerLines[2];

/* Writes Tn_MODE: op '=' stores the value, '&' masks with it, '|' sets its bits. */
void Timer_SetMode(s32 ch, u32 value, u8 op) {
    switch (ch) {
    case 0:
        switch (op) {
        case '=':
            TIMER_MODE(0) = value;
            break;
        case '&':
            TIMER_MODE(0) &= value;
            break;
        case '|':
            TIMER_MODE(0) |= value;
            break;
        }
        break;
    case 1:
        switch (op) {
        case '=':
            TIMER_MODE(1) = value;
            break;
        case '&':
            TIMER_MODE(1) &= value;
            break;
        case '|':
            TIMER_MODE(1) |= value;
            break;
        }
        break;
    }
}

/* Reads Tn_MODE (0 for any other channel). */
u32 Timer_GetMode(s32 ch) {
    u32 mode = 0;

    switch (ch) {
    case 0:
        mode = TIMER_MODE(0);
        break;
    case 1:
        mode = TIMER_MODE(1);
        break;
    }
    return mode;
}

/* Writes Tn_COMP. */
void Timer_SetCompare(s32 ch, u32 value) {
    switch (ch) {
    case 0:
        TIMER_COMP(0) = value;
        break;
    case 1:
        TIMER_COMP(1) = value;
        break;
    }
}

/* Writes Tn_COUNT and clears the software line count. */
void Timer_SetCount(s32 ch, u32 value) {
    switch (ch) {
    case 0:
        TIMER_COUNT(0) = value;
        break;
    case 1:
        TIMER_COUNT(1) = value;
        break;
    }
    gTimerLines[ch] = 0;
}

/* Elapsed microseconds: (counter + accumulated wraps) * 1000000 / 15734, truncated to 32 bits. */
s32 Timer_ReadMicros(s32 ch) {
    u64 lines = 0;

    switch (ch) {
    case 0:
        lines = TIMER_COUNT(0);
        break;
    case 1:
        lines = TIMER_COUNT(1);
        break;
    }
    if (Timer_GetMode(ch) & TIMER_MODE_EQUF) {
        gTimerLines[ch] += 0xFFFF;
        Timer_SetMode(ch, TIMER_MODE_EQUF, '|');
    }
    lines += gTimerLines[ch];
    return lines * 1000000 / TIMER_HBLANK_HZ;
}

/* Returns 0. */
s32 Timer_Stub(void) {
    return 0;
}

/* Sets the timer up to count horizontal blanks and wrap at 0xFFFF; it stays stopped. */
void Timer_Init(s32 ch) {
    Timer_SetMode(ch, TIMER_MODE_CLK_HBLANK | TIMER_MODE_ZRET | TIMER_MODE_CMPE | TIMER_MODE_OVFE, '=');
    Timer_SetCompare(ch, 0xFFFF);
}

/* Starts counting from zero. */
void Timer_Start(s32 ch) {
    Timer_SetMode(ch, TIMER_MODE_CUE, '|');
    Timer_Reset(ch);
}

/* Stops counting. */
void Timer_Stop(s32 ch) {
    Timer_SetMode(ch, ~TIMER_MODE_CUE, '&');
}

/* Zeroes the counter and the software line count. */
void Timer_Reset(s32 ch) {
    Timer_SetCount(ch, 0);
}

/* Elapsed microseconds since the last reset. */
s32 Timer_GetMicros(s32 ch) {
    return Timer_ReadMicros(ch);
}

/* Elapsed time in 1/60 s units: (u32)(micros * 60) / 1000000.0f. */
f32 Timer_GetFrames(s32 ch) {
    return (u32)(Timer_GetMicros(ch) * 60) / 1000000.0f;
}
