/*
 * "Sparking style" (branch bt3-bt4-estilo-sparking): the battle camera placed higher and farther behind the fighter,
 * a short push-in with a tilted horizon on strong hits (src/battle/btl_char_cam_modes.c) and, on the strongest, a
 * high-contrast black-and-white impact frame on the screen (gs_gpu.c, present.frag). All our own code: nothing is
 * taken from another game.
 *
 * Setting sz_style (bt3_settings.txt): 1 on (the default here), 0 the game's own camera. BT3_SZ_STYLE overrides it.
 * Off in an online match: the camera's stage test is read by the fighter code (btl_char_cam.h, cam->hit), and both
 * players' games must compute the same match.
 */
#include <stdlib.h>

extern int Port_Setting(const char *name, int def); /* plat_settings.c */
extern int Port_NetActive(void);                    /* net.c: 1 in an online match */

static int sOn = -1;
static volatile int sFlash; /* pictures left to show as the impact frame */

int Port_SzStyle(void) {
    if (sOn < 0) {
        const char *e = getenv("BT3_SZ_STYLE");
        sOn = (e != NULL ? atoi(e) : Port_Setting("sz_style", 1)) != 0;
    }
    return sOn && !Port_NetActive();
}

/* Percent of the body lean into fast movement (src/battle/bobj_a.c); negative leans the other way, 0 none. */
int Port_SzLean(void) {
    static int lean = -1000;
    if (lean == -1000) {
        const char *e = getenv("BT3_SZ_LEAN");
        lean = e != NULL ? atoi(e) : Port_Setting("sz_lean", 100);
    }
    return lean;
}

/* The next `pictures` shown pictures are drawn as the impact frame (a longer request wins over a shorter one). */
void Port_SzFlash(int pictures) {
    if (Port_SzStyle() && Port_Setting("sz_flash", 1) != 0 && pictures > sFlash) {
        sFlash = pictures;
    }
}

/* gs_gpu.c, once per shown picture: 1 if this one is the impact frame. */
int Port_SzFlashTake(void) {
    if (sFlash <= 0) {
        return 0;
    }
    sFlash--;
    return 1;
}
