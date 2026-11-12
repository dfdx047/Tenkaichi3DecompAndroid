#ifndef SYS_PAD_WATCH_H
#define SYS_PAD_WATCH_H

#include "types.h"

/*
 * Controller-removed watcher. Source range 0x267B90-0x268248.
 *
 * Every frame (PadWatch_Update, from Gfx_EndFrame) the connection state of both controller ports
 * is sampled (Pad.status != PAD_STATUS_NONE) and debounced: a reading has to stay the same for
 * 7 consecutive frames before it replaces the accepted state. From the accepted states and the
 * number of controllers the current screen needs, `message` is chosen:
 *   -1  nothing to report        0  controller 1 missing
 *    1  controller 2 missing     2  both missing
 * Two controllers are needed in a split-screen battle (unless BtlGame_IsReplay() is non-zero), or
 * outside a battle when gProgress->mode is 0x27 / 0x28 and gProgress + 0x620 is 1; otherwise only
 * port 1 is looked at.
 *
 * PadWatch_Draw (also from Gfx_EndFrame, after everything else) darkens the whole screen with a
 * black sprite of alpha 0x60 and prints text `message` of the boot file's table at +0x24, centred.
 * Nothing is drawn while disabled (PadWatch_SetEnabled(0), used around loading screens) or in a
 * battle of mode 7.
 *
 * PadWatch_GetMissing is what makes this more than a display: the battle's pause poll
 * (BtlPause_CheckOpen) calls it, and when it returns 1 the battle is paused.
 */

#define PAD_WATCH_PORTS 2
#define PAD_WATCH_STABLE_FRAMES 6 /* extra identical readings needed before a change is accepted */

/* PadWatch.message */
#define PAD_WATCH_MSG_NONE (-1)
#define PAD_WATCH_MSG_PAD1 0
#define PAD_WATCH_MSG_PAD2 1
#define PAD_WATCH_MSG_BOTH 2

typedef struct PadWatch {
    /* 0x00 */ s32 state[PAD_WATCH_PORTS]; /* accepted (debounced) reading: bit 0 = connected */
    /* 0x08 */ s32 raw[PAD_WATCH_PORTS];   /* reading of the previous frame */
    /* 0x10 */ s32 count[PAD_WATCH_PORTS]; /* frames the reading has stayed the same, up to 6 */
    /* 0x18 */ s32 valid[PAD_WATCH_PORTS]; /* 1 once the port has changed or settled at least once */
    /* 0x20 */ s32 disabled;               /* 1: no message is chosen or drawn */
    /* 0x24 */ s32 message;                /* PAD_WATCH_MSG_*, recomputed every frame */
} PadWatch; /* size 0x28 */

extern PadWatch *gPadWatch;

s32 PadWatch_IsConnected(s32 port);
void PadWatch_SetEnabled(s32 enable);
s32 PadWatch_GetMissing(s32 *port);
s32 PadWatch_IsPortMissing(s32 port);
void PadWatch_Update(void);
void PadWatch_DrawMessage(void);
void PadWatch_Init(void);
void PadWatch_Draw(void);
void PadWatch_Term(void);

#endif
