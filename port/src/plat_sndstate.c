/*
 * What the game can see of the port's sound code, kept where the game's state is saved and restored
 * (port/tools/make_state.py counts this file's variables as state; the sound code itself, gs/snd_adx.c, is not).
 *
 * The game creates stream players and asks them whether they are still playing. Which players exist is state: a
 * frame run again after a restore has to be handed the same players. And when the game's state must not depend
 * on the machine (online play; the save / restore test), "still playing" is counted in vertical blanks from the
 * stream's length here, instead of taken from the sound device's own progress.
 */
#include <stdint.h>

#define PORT_ADX_PLAYERS 16
typedef struct PortAdxView {
    int stat;   /* as ADXT_GetStat reports it: 0 stopped, 3 playing, 5 played to the end */
    int paused;
    int left;   /* vertical blanks to the end; -1: the stream loops */
    int vol, pan[2];
    char path[256]; /* the file it was last started with (to start the sound again when the game's state is
                       exchanged for another: Port_AdxResync in gs/snd_adx.c) */
} PortAdxView;

int gPortAdxCount;
PortAdxView gPortAdxView[PORT_ADX_PLAYERS];

/* One vertical blank has passed (Port_VBlank). */
void Port_AdxTick(void) {
    int i;
    for (i = 0; i < PORT_ADX_PLAYERS; i++) {
        PortAdxView *v = &gPortAdxView[i];
        if (v->stat == 3 && !v->paused && v->left > 0 && --v->left == 0) {
            v->stat = 5;
        }
    }
}
