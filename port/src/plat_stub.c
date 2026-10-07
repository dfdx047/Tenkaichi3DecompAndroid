/*
 * Headless stand-ins: sound, the GS, pads, movies. Everything here does nothing and reports "idle / done", which
 * is what the first milestone (the fight simulation without picture or sound) needs. Each group gets a real
 * implementation later and then moves to its own file. Arguments are ignored (the callers clean the stack).
 */
#include <stdint.h>
#include "port_host.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <time.h>
extern void Port_StateTouch(void *p, unsigned long n); /* gs/state.c */

/* ---- second processor (IOP): heap and remote calls ---- */
int sceSifInitIopHeap() { return 0; }
extern void *Port_LowAlloc(size_t size); /* plat_mem.c: the game keeps these addresses in 4 bytes */
extern void Port_LowFree(void *addr);
void *sceSifAllocIopHeap(int size) { return Port_LowAlloc((size_t)size); } /* "IOP memory": ordinary memory */
int sceSifFreeIopHeap(void *addr) { Port_LowFree(addr); return 0; }
int sceSifQueryTotalFreeMemSize() { return 0x100000; }
int sceSifQueryMaxFreeMemSize() { return 0x100000; }
/* The bind is answered at once: sceSifClientData.serve (offset 0x24) becomes non-NULL. */
int sceSifBindRpc(void *client, int id, int mode) { (void)id; (void)mode; *(uint32_t *)((uint8_t *)client + 0x24) = (uint32_t)(uintptr_t)client; /* a 4-byte pointer field of the game */ return 0; }
int func_002B4AF0() { return 0; }      /* sceSifCheckStatRpc: never busy */
/* sceSifCallRpc, sceSifSetDma, sceSdRemote: the sound effects, port/src/gs/snd_se.c */
int sceSifDmaStat() { return -1; }     /* transfer finished */

/* ---- sound driver ---- */
int sceSdRemoteInit() { return 0; }
int func_00296B48() { return 0; }      /* sceSdRemoteCallbackInit */

/* ---- CRI ADXT stream players ---- */
/* ---- CRI ADXT (streamed music and voices): port/src/gs/snd_adx.c ---- */

/* ---- GS ---- */
static int (*sVsyncHandler)(int);
void sceGsResetGraph() {}
void sceGsResetPath() {}
void sceGsSetDefStoreImage() {}
int sceGsExecStoreImage() { return 0; }
int sceGsSyncPath() { return 0; }
void *sceGsSyncVCallback(int (*handler)(int)) { void *old = (void *)sVsyncHandler; sVsyncHandler = handler; return old; }
/* One vertical blank: runs the game's VBlank handler, as the interrupt would. */
extern void Port_Trace(unsigned vblanks);
unsigned gPortVBlanks; /* vertical blanks since start: the headless build's clock */
/* With a window, a vertical blank is also the clock: one every 1/59.94 s. The game waits for one per frame in
   the menus (60 frames per second) and two in a battle (30), as on the console. Headless runs do not wait. */
extern int GsGpu_Enabled(void);
PORT_HOST unsigned long long gPortSleptNs = 0; /* time spent waiting here (the renderer's frame timing subtracts it) */

static unsigned long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
}

/* (real time, not game state: PORT_HOST) */
PORT_HOST static unsigned long long sPacePrev = 0, sPaceMin = ~0ull, sPaceMax = 0;
PORT_HOST static unsigned sPaceCount = 0, sPaceOff = 0, sPaceLate = 0, sPaceReset = 0;

/* Online play (gs/net.c): this copy runs ahead of the other one; the coming blanks are to come this much later
   in all (the grid moves, once). */
PORT_HOST unsigned gPortPaceShiftNs = 0;

static void vblank_wait(void) {
    PORT_HOST static unsigned long long next = 0;
    struct timespec ts;
    unsigned long long t = now_ns(), after;

    if (gPortPaceShiftNs != 0) {
        if (next != 0) {
            next += gPortPaceShiftNs;
        }
        gPortPaceShiftNs = 0;
    }

    if (next != 0 && t >= next && t - next < 100000000ull) {
        /* Late, but not hopelessly: this blank has passed already, so do not wait, and keep the grid. A fight
           frame waits for two blanks; when its work takes longer than one period (16.7 ms) but less than two, the
           first wait is skipped here and the second one ends on time, so the frame still lasts 33.4 ms. (Starting
           a new grid instead, as this did before, added a full period to every such frame: a machine that needed
           20 ms per frame ran the fight at 27 frames per second and unevenly, though it was within the budget.) */
        after = t;
        next += 16683350ull;
        sPaceLate++;
    } else if (next > t && next - t < 100000000ull) {
#ifdef _WIN32
        /* Windows' sleep is only good to about a millisecond, and only once the timer resolution has been asked
           for: sleep to within a millisecond and a half of the moment, then give the rest away in small slices. */
        static int asked;
        if (!asked) {
            asked = 1;
            timeBeginPeriod(1);
        }
        if (next - t > 1500000ull) {
            Sleep((DWORD)((next - t - 1500000ull) / 1000000ull));
        }
        while (now_ns() < next) {
            Sleep(0);
        }
#else
        ts.tv_sec = 0;
        ts.tv_nsec = (long)(next - t);
        nanosleep(&ts, NULL);
#endif
        after = now_ns();
        gPortSleptNs += after - t; /* the time really spent waiting (a sleep can return early or late) */
        next += 16683350ull;
    } else {
        after = t;
        next = t + 16683350ull; /* the first one, or more than a tenth of a second behind: a new grid from now */
        sPaceReset++;
    }
    /* BT3_GS_VERBOSE: once a second, how evenly the vertical blanks came (they should be 16.68 ms apart) */
    if (sPacePrev != 0) {
        unsigned long long d = after - sPacePrev;
        if (d < sPaceMin) { sPaceMin = d; }
        if (d > sPaceMax) { sPaceMax = d; }
        if (d > 18000000ull || d < 15400000ull) { sPaceOff++; }
    }
    sPacePrev = after;
    if (++sPaceCount == 60) {
        if (getenv("BT3_GS_VERBOSE") != NULL) {
            /* (whole numbers: this file is built with the game's software float, which has no 64-bit conversions) */
            fprintf(stderr, "pace: 60 vertical blanks: %u.%02u to %u.%02u ms apart, %u reached late (not waited for), %u new grids\n",
                    (unsigned)(sPaceMin / 1000000ull), (unsigned)(sPaceMin / 10000ull % 100), (unsigned)(sPaceMax / 1000000ull),
                    (unsigned)(sPaceMax / 10000ull % 100), sPaceLate, sPaceReset);
        }
        sPaceCount = sPaceOff = sPaceLate = sPaceReset = 0;
        sPaceMin = ~0ull;
        sPaceMax = 0;
    }
}

static unsigned char sPadLast[2][18]; /* (defined with the pads below) */
void Port_VBlank(void) {
    {
        extern void Port_SessionPoll(void); /* gs/state.c: the state at the first blank is kept; an online session
                                               begins or ends here */
        Port_SessionPoll();
    }
    /* BT3_PACED=1: real-time pacing without a window too (sound tests) */
    {
        extern int Port_NetSession(void), Port_NetWarp(void); /* gs/net.c */
        extern int gPortResim;                                /* gs/state.c: no picture, no sound */
        int warp = Port_NetWarp();
        extern int Port_NetResim(void); /* blanks being run again after a rollback: not shown, not timed either */
        int again = Port_NetResim();
        if (Port_NetSession() || again) {
            gPortResim = warp || again; /* an online session's start-up up to the versus menu is not shown and not timed */
        }
        if ((GsGpu_Enabled() || getenv("BT3_PACED") != NULL) && getenv("BT3_UNCAPPED") == NULL && !warp && !again) {
            vblank_wait();
        }
    }
    {   /* BT3_PAD_TABLE=<file>: what each pad read during the blank that ends here, 36 bytes per blank (the input
           of a session by blank and by pad: gs/net.c plays one player's column of it for its tests) */
        PORT_HOST static FILE *table = NULL;
        PORT_HOST static int tried = 0;
        if (!tried) {
            tried = 1;
            table = getenv("BT3_PAD_TABLE") != NULL ? fopen(getenv("BT3_PAD_TABLE"), "wb") : NULL;
        }
        if (table != NULL) {
            fwrite(sPadLast, 1, sizeof(sPadLast), table);
            fflush(table);
        }
    }
    gPortVBlanks++;
    {
        extern void Port_NetBeginTick(unsigned tick); /* gs/net.c: online play */
        Port_NetBeginTick(gPortVBlanks);
    }
    {
        extern void Port_SyncTest(unsigned vblank); /* gs/state.c: BT3_SYNCTEST */
        extern void Port_AdxTick(void);             /* plat_sndstate.c */
        Port_AdxTick();
        Port_SyncTest(gPortVBlanks);
    }
    Port_Trace(gPortVBlanks);
    if (sVsyncHandler != NULL) {
        sVsyncHandler(0);
    }
}
int sceGsSyncV() { Port_VBlank(); return 0; }

/* ---- pads ---- */
int sceDbcInit() { return 1; }
int scePad2Init() { return 1; }
static int sPadSockets;
int scePad2CreateSocket() { return sPadSockets++; }
int scePad2GetState() { return 1; }            /* connected and ready */
/* A DualShock 2's button profile: every digital button, both sticks, every pressure-sensitive button. */
int scePad2GetButtonProfile(int socket, unsigned char *profile) {
    (void)socket;
    profile[0] = 0xFF; profile[1] = 0xFF; profile[2] = 0xFF; profile[3] = 0x03;
    return 4;
}
/* With a window: keyboard and gamepads (port/src/gs/gs_input.c). Headless: nobody touches the pad. Buttons are
   active-low; the sticks rest at 0x80. */
extern int Port_PadRead(int socket, unsigned char *data);

/* Recording and playing back a whole session's controller input:
       BT3_PAD_REC=<file>    every pad read of this run is appended to the file (18 bytes per read, both ports)
       BT3_PAD_PLAY=<file>   the reads are answered from the file instead; after its end the pads are idle
   The game reads the pads a fixed number of times per vertical blank and everything else is deterministic, so a
   recording made from the title screen replays the same menus and the same fight, with or without a window
   (given the same save folder to start from). For reproducing what a player saw. */
PORT_HOST static FILE *sPadRec = NULL, *sPadPlay = NULL; /* (the position in the playback file IS restored: Port_PadPlaySeek) */
PORT_HOST static int sPadFilesTried = 0;

/* Where the playback of a pad recording stands, and going back there (the state save / restore test re-runs
   frames: they have to get the same input again). */
long Port_PadPlayPos(void) { return sPadPlay != NULL ? ftell(sPadPlay) : -1; }
/* (a position of -1 was taken before the file was open: its start) */
void Port_PadPlaySeek(long pos) { if (sPadPlay != NULL) { fseek(sPadPlay, pos >= 0 ? pos : 0, SEEK_SET); } }

/* The pads as the game last read them (for BT3_PAD_TABLE, below). */
static unsigned char sPadLast[2][18] = {{0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80}, {0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80}};
static int pad_read(int socket, unsigned char *data);

int scePad2Read(int socket, unsigned char *data) {
    extern int Port_NetActive(void);                       /* gs/net.c: online play */
    extern void Port_NetInput(int player, unsigned char *data);
    int n;

    if (Port_NetActive()) {
        Port_NetInput(socket, data); /* both pads come from the exchange with the other player */
        n = 18;
    } else {
        n = pad_read(socket, data);
    }
    if (socket >= 0 && socket < 2) {
        memcpy(sPadLast[socket], data, 18);
    }
    return n;
}

static int pad_read(int socket, unsigned char *data) {
    int i;

    if (!sPadFilesTried) {
        sPadFilesTried = 1;
        if (getenv("BT3_PAD_PLAY") != NULL) {
            sPadPlay = fopen(getenv("BT3_PAD_PLAY"), "rb");
        } else if (getenv("BT3_PAD_REC") != NULL) {
            sPadRec = fopen(getenv("BT3_PAD_REC"), "wb");
        }
    }
    if (sPadPlay != NULL) {
        /* After the recording's end the pads are idle (below), for the rest of the run: the keyboard and the
           controllers are not read. (They used to be, from the end on: two runs of one recording then differed
           by whatever a controller happened to report, which looked like the game not repeating itself.) The
           file stays open: a state restored to before the end reads the recording's last part again. */
        PORT_HOST static int said = 0;
        Port_StateTouch(data, 18);
        if (fread(data, 1, 18, sPadPlay) == 18) {
            return 18;
        }
        if (!said) {
            said = 1;
            fprintf(stderr, "bt3: the recorded input has ended\n");
        }
    } else if (Port_PadRead(socket, data)) {
        if (sPadRec != NULL) {
            fwrite(data, 1, 18, sPadRec);
            fflush(sPadRec);
        }
        return 18;
    }
    data[0] = 0xFF; data[1] = 0xFF;
    for (i = 2; i < 6; i++) { data[i] = 0x80; }
    for (i = 6; i < 18; i++) { data[i] = 0; }
    if (sPadRec != NULL) {
        fwrite(data, 1, 18, sPadRec);
        fflush(sPadRec);
    }
    return 18;
}
int sceVibGetProfile() { return 0; }
int sceVibSetActParam() { return 0; }

/* ---- MPEG movies: port/src/plat_movie.c ---- */

/* ---- widescreen ----
   The picture's shape, as width over height times 1000 (1333 = 4:3, the console's). Wider shapes keep the height
   of view and add to the sides:
       BT3_ASPECT=21:9 (or 16:9, 32:9, ...)    the shape asked for
       BT3_WIDE=1                               16:9          (BT3_WIDE=0: 4:3 whatever else is said)
       BT3_WINDOW=3440x1440                     a window wider than 3:2 asks for its own shape
   The game's projection, culling and effect proportions take Port_WideFactor() (src/battle/btl_cam.c, stg_a.c,
   eft_*.c, all #ifdef PORT); the renderer keeps 2D art in proportion and shows the picture at this shape
   (port/src/gs/gs_gpu.c). */
static int sAspectMilli = -1;
extern int Port_Setting(const char *name, int def); /* plat_settings.c */
int Port_AspectMilli(void) {
    if (sAspectMilli < 0) {
        int a = 0, b = 0, w = 0, h = 0;
        sAspectMilli = Port_Setting("aspect_milli", 1333); /* the saved choice; the variables below win */
        if (getenv("BT3_WIDE") != NULL && atoi(getenv("BT3_WIDE")) == 0) {
            sAspectMilli = 1333; /* 4:3 */
        } else if (getenv("BT3_ASPECT") != NULL && sscanf(getenv("BT3_ASPECT"), "%d:%d", &a, &b) == 2 && a > 0 && b > 0) {
            sAspectMilli = a * 1000 / b;
        } else if (getenv("BT3_WIDE") != NULL) {
            sAspectMilli = 1778;
        } else if (getenv("BT3_WINDOW") != NULL && sscanf(getenv("BT3_WINDOW"), "%dx%d", &w, &h) == 2 && h > 0 && w * 2 > h * 3) {
            sAspectMilli = w * 1000 / h;
        }
        if (sAspectMilli < 1333) { sAspectMilli = 1333; }
        if (sAspectMilli > 4000) { sAspectMilli = 4000; }
    }
    return sAspectMilli;
}
/* The settings overlay changes the shape while the game runs: the game asks every frame. */
void Port_SetAspectMilli(int milli) {
    sAspectMilli = milli < 1333 ? 1333 : milli > 4000 ? 4000 : milli;
}
int Port_IsWide(void) {
    return Port_AspectMilli() > 1340;
}
/* The picture's width over the console's 4:3 width (1 = not wide). */
/* In the menus it is 1: their pages are composed for 4:3 (backdrops, scrolling scenery, 2D and 3D placed against
   each other), so they are shown as they are, stretched to the picture's width (the user's choice). */
extern int gPortMenuMode; /* headless.c */
float Port_WideFactor(void) {
    return Port_IsWide() && !gPortMenuMode ? (float)Port_AspectMilli() * 3.0f / 4000.0f : 1.0f;
}
