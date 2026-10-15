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
#include "port_addr.h"
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
int sceSifBindRpc(void *client, int id, int mode) { (void)id; (void)mode; *(uint32_t *)((uint8_t *)client + 0x24) = Port_Addr(client); /* a 4-byte pointer field of the game */ return 0; }
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
/* Reading pixels back from the GS (the screen cross-fade of the fight's scenes reads the frame being shown: stg_b.c).
   SetDef says what: the place in GS memory (in 64-pixel units), the size; Exec delivers it, 3 bytes a pixel. */
static int sStoreBp, sStoreW, sStoreH;
int sceGsSetDefStoreImage(void *si, int sbp, int sbw, int spsm, int x, int y, int w, int h) {
    (void)si; (void)sbw; (void)spsm; (void)x; (void)y;
    sStoreBp = sbp & 0xFFFF;
    sStoreW = w & 0xFFFF;
    sStoreH = h & 0xFFFF;
    return 0;
}
int sceGsExecStoreImage(void *si, void *dst) {
    extern void Gs_StoreImage(unsigned bp, unsigned w, unsigned h, unsigned char *rgb); /* gs/gs_core.c */
    (void)si;
    Gs_StoreImage((unsigned)sStoreBp, (unsigned)sStoreW, (unsigned)sStoreH, (unsigned char *)dst);
    return 0;
}
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
/* Vertical blanks the game has really gone through (not the ones run again after a rollback): host data. */
PORT_HOST unsigned gPortLiveBlanks = 0;

PORT_HOST static unsigned long long sPaceNext = 0; /* when the coming vertical blank is due */
#define next sPaceNext
/* How long until the coming vertical blank, in microseconds (0: it is due or past, or there is no grid yet). For
   what wants to do something before it without making it late (gs/gs_draw.c: in-between pictures). */
int Port_VBlankUsLeft(void) {
    unsigned long long t = now_ns();
    return sPaceNext > t && sPaceNext - t < 100000000ull ? (int)((sPaceNext - t) / 1000ull) : 0;
}

/* How far behind its grid the game is, in microseconds: the coming vertical blank was due that long ago (0: it
   is not due yet, or there is no grid). For gs/gs_draw.c, which leaves pictures out when the machine is behind. */
int Port_PaceBehindUs(void) {
    unsigned long long t = now_ns();
    return sPaceNext != 0 && t > sPaceNext && t - sPaceNext < 100000000ull ? (int)((t - sPaceNext) / 1000ull) : 0;
}

/* The game's pace, 1000 = its own. Online it is the match's and nothing else (gs/net.c: 1000 or 1200, the same on
   both copies); otherwise the player's own "Game speed" of the Cheats tab (setting game_speed, percent, 50..200),
   which an online match never reads. Like the match's speed it is the vertical blanks' pace and the pitch of what
   is heard: every step of the game is computed as always. (PORT_HOST: not the game's state.) */
extern int Port_NetSpeedMilli(void);
extern int Port_NetSession(void);
extern int Port_NetActive(void);
extern int Port_Setting(const char *name, int def); /* plat_settings.c */
extern void Port_SettingSave(const char *name, int value);
PORT_HOST static int sSpeedPercent = -1;
int Port_GameSpeed(void) {
    if (sSpeedPercent < 0) {
        int v = Port_Setting("game_speed", 100);
        sSpeedPercent = v < 50 ? 50 : v > 200 ? 200 : v;
    }
    return sSpeedPercent;
}
void Port_GameSpeedSet(int percent) {
    percent = percent < 50 ? 50 : percent > 200 ? 200 : percent;
    if (percent != Port_GameSpeed()) {
        sSpeedPercent = percent;
        Port_SettingSave("game_speed", percent);
    }
}
int Port_SpeedMilli(void) {
    return Port_NetSession() || Port_NetActive() ? Port_NetSpeedMilli() : Port_GameSpeed() * 10;
}
/* BT3_LAG_LOG=1: when things happen in a tick, in microseconds since the start: V a vertical blank (after its wait),
   R a pad read, E the window's events taken (which is when a controller's state is renewed), S a picture handed to
   the window. For measuring how old the input is that the game reads, and how long until its picture is shown. */
void Port_LagMark(int what) {
    PORT_HOST static int on = -1;
    PORT_HOST static unsigned long long first = 0;
    unsigned long long t;
    if (on < 0) {
        on = getenv("BT3_LAG_LOG") != NULL;
    }
    if (!on) {
        return;
    }
    t = now_ns();
    if (first == 0) {
        first = t;
    }
    {
        extern unsigned gGsFrame; /* gs/gs_draw.c: the pictures so far */
        fprintf(stderr, "lag %c %u %u\n", what, (unsigned)((t - first) / 1000ull), gGsFrame);
    }
}

int Port_LateInput(void);

/* A vertical blank's length: 16.68 ms at the game's own pace. */
#define BLANK_NS (16683350ull * 1000ull / (unsigned long long)Port_SpeedMilli())
unsigned long long Port_BlankNs(void) { return BLANK_NS; }

static void vblank_wait(void) {
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
        next += BLANK_NS;
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
        next += BLANK_NS;
    } else {
        after = t;
        next = t + BLANK_NS; /* the first one, or more than a tenth of a second behind: a new grid from now */
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

#undef next
static unsigned char sPadLast[2][18]; /* (defined with the pads below) */

/* ---- "Low input lag" (setting late_input, F1 > Video; BT3_LATE_INPUT=0/1) ------------------------------------
 * The game reads the pads, computes the tick, and THEN waits for its vertical blanks (two in a fight) before the
 * picture is shown: what is seen is 33 ms old in a fight although computing and drawing it took a few (measured
 * with BT3_LAG_LOG: 1 ms and 2 ms on a fast machine). With this on, a blank the game asks for is not waited for
 * there but owed, and waited for when the game next reads the pads: the tick's length is the same, the pads are
 * read at its end instead of its start, and the picture follows the read by the time the work takes.
 * Nothing of the game's state changes: the same reads, the same blanks, in the same order (a recording replays
 * the same). Not in an online session and not with in-between pictures, which keep the original order.
 * (PORT_HOST: real time, not the game's state.) */
PORT_HOST static int sLateOwed = 0, sLateOn = -1;

int Port_LateInput(void) {
    if (sLateOn < 0) {
        sLateOn = getenv("BT3_LATE_INPUT") != NULL ? atoi(getenv("BT3_LATE_INPUT")) != 0 : Port_Setting("late_input", 0) != 0;
    }
    return sLateOn;
}

void Port_LateInputSet(int on) {
    extern void Port_SettingSave(const char *name, int value);
    sLateOn = on != 0;
    Port_SettingSave("late_input", sLateOn);
}

/* The blanks owed are waited for now: called before the first pad is read. */
static void late_wait(void) {
    extern void PortInput_Fresh(void); /* gs/gs_input.c */
    if (sLateOwed == 0) {
        return;
    }
    while (sLateOwed > 0) {
        sLateOwed--;
        vblank_wait();
    }
    Port_LagMark('L');
    if (GsGpu_Enabled()) {
        PortInput_Fresh(); /* the controllers and keys as they are now, not as they were when the last picture was shown */
    }
}
void Port_VBlank(void) {
    Port_LagMark('W'); /* (the game has come to wait for a blank) */
    {
        extern void Port_SessionPoll(void); /* gs/state.c: the state at the first blank is kept; an online session
                                               begins or ends here */
        Port_SessionPoll();
    }
    {
        extern void Port_LobbyAuto(void); /* gs/net.c: BT3_LOBBY, the room-code lobby without its window (tests) */
        Port_LobbyAuto();
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
        {
            /* gs/gs_draw.c: the pictures of the tick that follow its first (in-between pictures and the real one):
               those due before this blank now, those due at it after the wait */
            extern void GsGpu_InterpPump(int ahead_ms, int all);
            extern int gsInterp; /* gs/gs_draw.c: in-between pictures (their timing counts from the blanks) */
            int timed = (GsGpu_Enabled() || getenv("BT3_PACED") != NULL) && getenv("BT3_UNCAPPED") == NULL && !warp && !again;
            /* (not while blanks are run again after a rollback or run ahead for an online session: those are
               not shown and not timed, and waiting for a picture here would slow the catching up) */
            if (timed && Port_LateInput() && !Port_NetSession() && gsInterp <= 0 && sLateOwed < 2) {
                /* "Low input lag": the wait is owed and made before the next pad read instead (scePad2Read), so
                   that the tick is computed and shown right after its input was read, not a tick's wait later.
                   At most two blanks are owed (a fight's tick has two); where the game reads no pad (loading)
                   the blanks are waited for here as always. */
                sLateOwed++;
            } else {
            if (GsGpu_Enabled() && !warp && !again) {
                GsGpu_InterpPump(-1, 0); /* (-1: what can be finished before the blank) */
            }
            if (timed) {
                vblank_wait();
            }
            }
            if (GsGpu_Enabled() && !warp && !again) {
                GsGpu_InterpPump(6, 0);
            }
        }
    }
    {
        extern void PortA11y_GameTick(void); /* a11y_game.c: what the screen reader says (off without its library) */
        PortA11y_GameTick();
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
    Port_LagMark('V');
    gPortVBlanks++;
    {
        extern int gPortResim;
        if (!gPortResim) {
            gPortLiveBlanks++; /* (the meter: the game's speed is this per second, of 60) */
        }
    }
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

    if (socket == 0) {
        late_wait();
        Port_LagMark('R');
    }
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
/* The game's motor command for a port, sent every frame (Pad_SendActuator, src/sys/pad.c): two bytes, bit 0 = the
   small motor, bits 1..7 = the large motor's power. To the player's controller (gs/gs_input.c). Only read here:
   nothing of it goes back into the game. */
int sceVibSetActParam(int socket, int profileSize, unsigned char *profile, int dataSize, unsigned char *data) {
    extern void Port_PadVibrate(int socket, int small, int large);
    (void)profileSize; (void)profile;
    if (dataSize >= 1 && data != NULL) {
        Port_PadVibrate(socket, data[0] & 1, data[0] >> 1);
    }
    return 0;
}

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
/* (PORT_HOST: the player's setting, not the game's state. Without it the shape was saved and put back with the
   game: a session starts by going back to the state of the program's first moments, so a shape chosen in the
   settings window during the run went back to what it was at start, 4:3 on a first run. Reported as "joining a
   match resets the screen to 4:3".) */
PORT_HOST static int sAspectMilli = -1;
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
    {
        /* BT3_ASPECT_AT=<frame>:<w>:<h> (testing): from that frame on the shape is w:h, as if it had been changed in
           the settings window at that moment (a run without a window can then show a change made during a fight). */
        static int at = -2, to;
        extern unsigned gGsFrame; /* gs_core.c */
        if (at == -2) {
            int f = 0, w = 0, h = 0;
            const char *e = getenv("BT3_ASPECT_AT");
            at = -1;
            if (e != NULL && sscanf(e, "%d:%d:%d", &f, &w, &h) == 3 && w > 0 && h > 0) {
                at = f;
                to = w * 1000 / h;
            }
        }
        if (at >= 0 && (int)gGsFrame >= at) {
            return to < 1333 ? 1333 : to > 4000 ? 4000 : to;
        }
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
/* "Menus in 4:3" (F1, UI; setting menus_43): the menus' pages are shown at the shape they were composed for, in the
   middle with black at the sides, instead of stretched to the picture's width. A fight is not changed.
   Port_PageAspectMilli is the shape of the page being shown now: what the present step and the loading screens
   go by. (PORT_HOST: the player's setting, not the game's state.) */
PORT_HOST static int sMenus43 = -1;
int Port_Menus43(void) {
    if (sMenus43 < 0) {
        sMenus43 = Port_Setting("menus_43", 0) != 0;
    }
    return sMenus43;
}
void Port_Menus43Set(int on) {
    sMenus43 = on != 0;
}
int Port_PageAspectMilli(void) {
    return gPortMenuMode && Port_Menus43() ? 1333 : Port_AspectMilli();
}
