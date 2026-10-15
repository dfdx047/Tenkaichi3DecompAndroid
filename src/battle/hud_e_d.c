#include "common.h"
#include "battle/hud_e.h"
#include "battle/battle.h"
#include "battle/battle_setup.h"
#include "sys/pad.h"
#include "sys/pad_watch.h"
#include "sys/adx.h"
#include "sys/snd.h"

/*
 * Pause request, 0x22F998-0x22FC40. Not part of the HUD: it remembers how many pads may pause the battle (2 in
 * split screen, else 1) and which pad did. BtlPause_CheckOpen / BtlPause_CheckStart are transition conditions of
 * the battle sequence tables (gBtlSeqTbl*); they read the pads' game button words directly.
 */

#define BTL_PAUSE_START 0x1000 /* START in Pad.gamePressed */

extern BtlPause gBtlPause;

/* Returns the work. */
BtlPause *BtlPause_GetWork(void) {
    return &gBtlPause;
}

/* Round reset: pad 0 owns the pause. */
void BtlPause_Reset(void) {
    BtlPause_GetWork()->pad = 0;
}

/* Battle start: sets the number of pads that may pause. */
void BtlPause_Init(s32 padCount) {
    BtlPause_GetWork()->padCount = padCount;
    BtlPause_Reset();
}

/* Battle end: nothing. */
void BtlPause_Term(void) {
}

/* Sequence condition "open the pause menu": 1 when one of the pads pressed START (that pad becomes the owner).
   Otherwise, outside mode 7, a pad that was pulled out (PadWatch_GetMissing) becomes the owner and the battle is
   paused at once (BATTLE_FLAG_PAUSE | BATTLE_FLAG_PAUSE_MENU, the streams and sound group 4 are paused), but 0 is
   returned. In mode 7 the pad watch is not asked; pad 0's START counts once more (the owner is not
   changed). */
s32 BtlPause_CheckOpen(void) {
    BtlPause *work = BtlPause_GetWork();
    s32 port;
    s32 i;

    for (i = 0; i < work->padCount; i++) {
        if (gPad[i].gamePressed & BTL_PAUSE_START) {
            work->pad = i;
            return 1;
        }
    }
    if (Battle_GetMode() != 7) {
#ifdef PORT
        /* (online nothing stops the fight, btl_seq.c: a pad pulled out does not either) */
        extern int Port_NetActive(void);
        if (Port_NetActive()) {
            return 0;
        }
#endif
        if (PadWatch_GetMissing(&port)) {
            work->pad = port;
            Battle_GetWork()->flags |= BATTLE_FLAG_PAUSE | BATTLE_FLAG_PAUSE_MENU;
            Adx_PauseSeVoice();
            Snd_SetPause(4, 1);
            return 0;
        }
    } else if (gPad[0].gamePressed & BTL_PAUSE_START) {
        return 1;
    }
    return 0;
}

/* Sequence condition "START pressed": while a loaded replay plays only pad 0 counts, else any pad of the list. */
s32 BtlPause_CheckStart(void) {
    BtlPause *work = BtlPause_GetWork();
    s32 i;

    if (BattleReplay_IsActive() && BattleReplay_IsLoaded()) {
        if (gPad[0].gamePressed & BTL_PAUSE_START) {
            return 1;
        }
        return 0;
    }
    for (i = 0; i < work->padCount; i++) {
        if (gPad[i].gamePressed & BTL_PAUSE_START) {
            return 1;
        }
    }
    return 0;
}

/* Returns the number of pads that may pause. */
s32 BtlPause_GetPadCount(void) {
    return BtlPause_GetWork()->padCount;
}

/* Sets the pad that owns the pause. */
void BtlPause_SetPad(s32 pad) {
    BtlPause_GetWork()->pad = pad;
}

/* The pad that drives the pause menu: the owner, or 0 while a loaded replay plays. */
s32 BtlPause_GetMenuPad(void) {
    BtlPause *work = BtlPause_GetWork();

    if (BattleReplay_IsActive() && BattleReplay_IsLoaded()) {
        return 0;
    }
    return work->pad;
}

/* The pad that owns the pause. */
s32 BtlPause_GetPad(void) {
    return BtlPause_GetWork()->pad;
}
