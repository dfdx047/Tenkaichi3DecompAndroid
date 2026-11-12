/*
 * Android's performance hints (ADPF, APerformanceHint: Android 13 and newer), as the ARMSX2 emulator uses them
 * (pcsx2/PerformanceMetrics.cpp): the engine tells the system how long each frame's work took and how long it may
 * take. Without them the CPU governor sees a thread that works for a few milliseconds and then sleeps, and lowers the
 * clocks; the next heavy frame (a special attack, a transformation) then runs slow until the clocks come back. With
 * them the system keeps the clocks where the frames need them, and lowers them where the frames are light.
 *
 * Loaded from libandroid.so by name, so the engine still starts on Android 10 to 12 (there it does nothing). Setting
 * "adpf" (bt3_settings.txt): 1 on (default), 0 off; BT3_ADPF=0 turns it off for a test.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __ANDROID__
#include <dlfcn.h>
#include <sys/syscall.h>
#include <unistd.h>

extern int Port_Setting(const char *name, int def); /* plat_settings.c */

typedef struct APerformanceHintManager APerformanceHintManager;
typedef struct APerformanceHintSession APerformanceHintSession;

static APerformanceHintManager *(*pGetManager)(void);
static APerformanceHintSession *(*pCreateSession)(APerformanceHintManager *, const int32_t *, size_t, int64_t);
static int (*pReportActual)(APerformanceHintSession *, int64_t);
static int (*pUpdateTarget)(APerformanceHintSession *, int64_t);
static APerformanceHintSession *sSession;
static int sState; /* 0 not tried yet, 1 running, -1 not available or off */
static int64_t sTarget;
static int32_t sTid;
static int32_t sRenderTid; /* the render thread's (GsAdpf_AddThread), 0 without it */

/* The render thread (gs_core.c), from itself as it starts: it does half of each frame's work beside the game's
   thread, so the session covers it too, or the governor keeps that thread's core slow. Called before the first
   frame (the game's thread waits for the render thread to start). */
void GsAdpf_AddThread(void) {
    sRenderTid = (int32_t)syscall(SYS_gettid);
}

static void adpf_start(int64_t target) {
    void *lib;
    APerformanceHintManager *m;
    const char *e = getenv("BT3_ADPF");
    sState = -1;
    if ((e != NULL && atoi(e) == 0) || (e == NULL && Port_Setting("adpf", 1) == 0)) {
        fprintf(stderr, "bt3: performance hints: off\n");
        return;
    }
    lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (lib == NULL) {
        return;
    }
    pGetManager = (APerformanceHintManager * (*)(void)) dlsym(lib, "APerformanceHint_getManager");
    pCreateSession = (APerformanceHintSession * (*)(APerformanceHintManager *, const int32_t *, size_t, int64_t))
        dlsym(lib, "APerformanceHint_createSession");
    pReportActual = (int (*)(APerformanceHintSession *, int64_t))dlsym(lib, "APerformanceHint_reportActualWorkDuration");
    pUpdateTarget = (int (*)(APerformanceHintSession *, int64_t))dlsym(lib, "APerformanceHint_updateTargetWorkDuration");
    if (pGetManager == NULL || pCreateSession == NULL || pReportActual == NULL || pUpdateTarget == NULL) {
        fprintf(stderr, "bt3: performance hints: not on this Android version\n");
        return;
    }
    m = pGetManager();
    sTid = (int32_t)syscall(SYS_gettid);
    {
        int32_t tids[2] = {sTid, sRenderTid};
        size_t count = sRenderTid != 0 && sRenderTid != sTid ? 2 : 1;
        sSession = m != NULL ? pCreateSession(m, tids, count, target) : NULL;
        if (sSession == NULL && count == 2) { /* (refused with two: the game's thread alone, as before) */
            sSession = pCreateSession(m, tids, 1, target);
            sRenderTid = 0;
        }
    }
    if (sSession == NULL) {
        fprintf(stderr, "bt3: performance hints: the system gave no session\n");
        return;
    }
    sTarget = target;
    sState = 1;
    if (sRenderTid != 0) {
        fprintf(stderr, "bt3: performance hints: on (threads %d and render %d, %.1f ms a frame)\n", (int)sTid, (int)sRenderTid, (double)target / 1e6);
    } else {
        fprintf(stderr, "bt3: performance hints: on (thread %d, %.1f ms a frame)\n", (int)sTid, (double)target / 1e6);
    }
}

/* One frame's work (without the waiting for the next frame) and the time a frame may take, in nanoseconds. From
   the thread that does the frame's work (always the same one). */
void GsAdpf_Frame(uint64_t workNs, uint64_t targetNs) {
    if (sState == 0) {
        adpf_start((int64_t)targetNs);
    }
    if (sState != 1 || workNs == 0) {
        return;
    }
    if ((int64_t)targetNs != sTarget && targetNs > 0) {
        sTarget = (int64_t)targetNs;
        pUpdateTarget(sSession, sTarget);
    }
    if (workNs > 1000000000ull) {
        workNs = 1000000000ull; /* (a load or a pause: not a frame) */
    }
    pReportActual(sSession, (int64_t)workNs);
}
#else
void GsAdpf_AddThread(void) {
}
void GsAdpf_Frame(uint64_t workNs, uint64_t targetNs) {
    (void)workNs;
    (void)targetNs;
}
#endif
