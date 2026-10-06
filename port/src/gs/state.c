/*
 * The game's state as one number: a checksum of everything the game itself owns in memory. For online play two
 * machines have to stay bit for bit the same; this is how that is checked, first of all between two runs on one
 * machine.
 *
 * What it covers: the game's global variables (the address ranges port/tools/make_state.py took from the linker's
 * map, in <program>.mem next to the program), the game's heap and the scratchpad. Not covered: the port's own
 * memory (renderer, sound, input), and the game thread's stack and registers.
 *
 *   BT3_HASH=<file>       one line per vertical blank: its number, the checksum of everything, and during a fight
 *                         the checksum of the fight's own values (health, positions, clock, generator) and
 *                         the values themselves
 *   BT3_HASH_AT=<n>       at that vertical blank, also <file>.pages (a checksum per 4 KB of every region) and
 *                         <file>.dump (the regions themselves), for finding WHERE two runs differ
 *                         (port/tools/compare_hash.py)
 *
 * The numbers of two runs of the SAME program can be compared. Not those of different builds: the state holds
 * addresses of functions and variables, which differ from build to build.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#define XXH_INLINE_ALL
#include "../../third_party/xxhash/xxhash.h"
#include "gs_internal.h"

extern void Port_HeapRegion(uint8_t **base, uint32_t *size); /* plat_mem.c */

#define MAX_REGIONS 64
static struct { const uint8_t *p; size_t n; const char *what; } sRegion[MAX_REGIONS];
static int sRegions = -1;

static void regions_init(void) {
    char exe[1024], path[1100];
    uint8_t *heap;
    uint32_t heapSize;
    FILE *fp;
#ifdef _WIN32
    long n = (long)GetModuleFileNameA(NULL, exe, sizeof(exe) - 1);
#else
    long n = (long)readlink("/proc/self/exe", exe, sizeof(exe) - 1);
#endif

    sRegions = 0;
    if (n > 0 && n < (long)sizeof(exe) - 1) {
        unsigned long long a, size;
        exe[n] = '\0';
        snprintf(path, sizeof(path), "%s.mem", exe);
        fp = fopen(path, "r");
        if (fp == NULL) {
            fprintf(stderr, "bt3: state: %s is missing (written by port/tools/make_state.py at link time)\n", path);
        } else {
            while (sRegions < MAX_REGIONS - 2 && fscanf(fp, "%llx %llx", &a, &size) == 2) {
                sRegion[sRegions].p = (const uint8_t *)(uintptr_t)a;
                sRegion[sRegions].n = (size_t)size;
                sRegion[sRegions].what = "globals";
                sRegions++;
            }
            fclose(fp);
        }
    }
    Port_HeapRegion(&heap, &heapSize);
    sRegion[sRegions].p = heap;
    sRegion[sRegions].n = heapSize;
    sRegion[sRegions].what = "heap";
    sRegions++;
    sRegion[sRegions].p = (const uint8_t *)(uintptr_t)0x70000000u;
    sRegion[sRegions].n = 0x4000;
    sRegion[sRegions].what = "scratchpad";
    sRegions++;
}

uint64_t Port_StateHash(void) {
    XXH3_state_t st;
    int i;

    if (sRegions < 0) {
        regions_init();
    }
    XXH3_64bits_reset(&st);
    for (i = 0; i < sRegions; i++) {
        XXH3_64bits_update(&st, sRegion[i].p, sRegion[i].n);
    }
    return XXH3_64bits_digest(&st);
}

/* Called at every vertical blank (Port_Trace, headless.c). */
void Port_StateLog(unsigned vblank, const void *fight, unsigned fightSize) {
    static FILE *fp;
    static int mode = -1, at = -1;

    if (mode < 0) {
        const char *path = getenv("BT3_HASH");
        mode = 0;
        if (path != NULL && (fp = fopen(path, "w")) != NULL) {
            mode = 1;
            at = getenv("BT3_HASH_AT") != NULL ? atoi(getenv("BT3_HASH_AT")) : -1;
        }
    }
    if (mode != 1) {
        return;
    }
    /* blank, checksum of everything, checksum of the fight's own values (0 outside a fight) and those values */
    fprintf(fp, "%u %016llx %016llx", vblank, (unsigned long long)Port_StateHash(), fight != NULL ? (unsigned long long)XXH3_64bits(fight, fightSize) : 0ull);
    if (fight != NULL) {
        const int *w = fight;
        fprintf(fp, " hp %d %d pos %08x %08x %08x  %08x %08x %08x clock %d rnd %08x%08x", w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], w[11], w[10]);
    }
    fputc('\n', fp);
    if ((int)vblank == at) {
        char path[1100];
        FILE *pg;
        int i;
        snprintf(path, sizeof(path), "%s.pages", getenv("BT3_HASH"));
        pg = fopen(path, "w");
        if (pg != NULL) {
            for (i = 0; i < sRegions; i++) {
                size_t off;
                for (off = 0; off < sRegion[i].n; off += 4096) {
                    size_t n = sRegion[i].n - off < 4096 ? sRegion[i].n - off : 4096;
                    fprintf(pg, "%s %llx %016llx\n", sRegion[i].what, (unsigned long long)(uintptr_t)(sRegion[i].p + off),
                            (unsigned long long)XXH3_64bits(sRegion[i].p + off, n));
                }
            }
            fclose(pg);
        }
        /* and <file>.dump: the regions themselves, each after a line "what address size", for a byte-wise look */
        snprintf(path, sizeof(path), "%s.dump", getenv("BT3_HASH"));
        pg = fopen(path, "wb");
        if (pg != NULL) {
            for (i = 0; i < sRegions; i++) {
                fprintf(pg, "%s %llx %llx\n", sRegion[i].what, (unsigned long long)(uintptr_t)sRegion[i].p, (unsigned long long)sRegion[i].n);
                fwrite(sRegion[i].p, 1, sRegion[i].n, pg);
            }
            fclose(pg);
        }
        fflush(fp);
    }
    if (vblank % 600 == 0) {
        fflush(fp);
    }
}

/* ---------------------------------------------------------------------------------------------------------------
 * Saving and restoring the game's state, and the test of it. (64-bit Linux so far.)
 *
 * A snapshot is: the regions the checksum covers, the port's own memory that belongs to the game's state
 * (Port_StateExtra), the used part of the game thread's stack, and the processor's registers at the moment of
 * saving. Restoring copies all of it back and continues from the moment of saving: the function that saved
 * returns a second time. The copy back is done from another stack (the game thread's own is being overwritten).
 *
 *   BT3_SYNCTEST=1   every vertical blank: save; run to the next blank; note the checksum; restore; run to the
 *                    next blank AGAIN; compare. A game that cannot be rewound and replayed to the same state shows
 *                    here, with the place in memory that came out differently. Every frame runs twice.
 *                    (=<n>: start at blank n.)
 * ------------------------------------------------------------------------------------------------------------- */
#if defined(__x86_64__) && !defined(_WIN32)
#include <ucontext.h>

extern int Port_StateExtra(void **p, size_t *n, int max); /* plat_mem.c */
extern uint8_t *Port_GameStackTop(void);
extern long Port_PadPlayPos(void);                         /* plat_stub.c */
extern void Port_PadPlaySeek(long pos);

#define MAX_PARTS (MAX_REGIONS + 8)
typedef struct Snapshot {
    struct { uint8_t *at; size_t n; uint8_t *copy; size_t cap; } part[MAX_PARTS];
    int parts;
    ucontext_t ctx;
    long padPos;
    volatile int loaded;
} Snapshot;

static void part_save(Snapshot *s, uint8_t *at, size_t n) {
    int k = s->parts++;
    if (s->part[k].cap < n) {
        free(s->part[k].copy);
        s->part[k].copy = malloc(n + 4096);
        s->part[k].cap = n + 4096;
    }
    s->part[k].at = at;
    s->part[k].n = n;
    memcpy(s->part[k].copy, at, n);
}

/* Returns 0 after saving, 1 when it returns the second time (the state was restored). */
static int __attribute__((noinline)) state_save(Snapshot *s) {
    void *xp[8];
    size_t xn[8];
    uint8_t *sp = (uint8_t *)__builtin_frame_address(0) - 1024, *top = Port_GameStackTop();
    int i, extra;

    if (sRegions < 0) {
        regions_init();
    }
    s->parts = 0;
    for (i = 0; i < sRegions; i++) {
        part_save(s, (uint8_t *)sRegion[i].p, sRegion[i].n);
    }
    extra = Port_StateExtra(xp, xn, 8);
    for (i = 0; i < extra; i++) {
        part_save(s, xp[i], xn[i]);
    }
    s->padPos = Port_PadPlayPos();
    s->loaded = 0;
    part_save(s, sp, (size_t)(top - sp)); /* the stack last: this frame's own contents as they are now */
    getcontext(&s->ctx);
    return s->loaded;
}

static Snapshot *sLoading;
static ucontext_t sLoaderCtx;

static void loader(void) {
    Snapshot *s = sLoading;
    int k;
    for (k = 0; k < s->parts; k++) {
        memcpy(s->part[k].at, s->part[k].copy, s->part[k].n);
    }
    Port_PadPlaySeek(s->padPos);
    s->loaded = 1;
    setcontext(&s->ctx);
}

static void state_load(Snapshot *s) {
    static uint8_t *stack;
    if (stack == NULL) {
        stack = malloc(1 << 20);
    }
    sLoading = s;
    getcontext(&sLoaderCtx);
    sLoaderCtx.uc_stack.ss_sp = stack;
    sLoaderCtx.uc_stack.ss_size = 1 << 20;
    sLoaderCtx.uc_link = NULL;
    makecontext(&sLoaderCtx, loader, 0);
    setcontext(&sLoaderCtx);
}

/* a checksum of a snapshot's memory, the stack left out (the last part) */
static uint64_t parts_hash(void) {
    XXH3_state_t st;
    void *xp[8];
    size_t xn[8];
    int i, extra = Port_StateExtra(xp, xn, 8);
    XXH3_64bits_reset(&st);
    for (i = 0; i < sRegions; i++) {
        XXH3_64bits_update(&st, sRegion[i].p, sRegion[i].n);
    }
    for (i = 0; i < extra; i++) {
        XXH3_64bits_update(&st, xp[i], xn[i]);
    }
    return XXH3_64bits_digest(&st);
}

void Port_SyncTest(unsigned vblank) {
    static struct Ctl { int mode, phase; unsigned from, frames, bad; uint64_t h1; Snapshot snap, first; } *c;

    if (c == NULL) {
        const char *e = getenv("BT3_SYNCTEST");
        c = calloc(1, sizeof(*c)); /* (outside everything a snapshot restores) */
        c->mode = e != NULL;
        c->from = e != NULL ? (unsigned)atoi(e) : 0;
    }
    if (!c->mode || vblank < c->from) {
        return;
    }
    if (sRegions < 0) {
        regions_init();
    }
    if (c->phase == 1) {
        /* the frame has run once: keep what it produced, go back and run it again */
        c->h1 = parts_hash();
        c->phase = 2;
        if (state_save(&c->first) == 0) { /* (kept only to say WHERE a difference is) */
            state_load(&c->snap);
        }
        return; /* not reached: the load continues in the save below, one frame ago */
    }
    if (c->phase == 2) {
        uint64_t h2 = parts_hash();
        c->frames++;
        if (h2 != c->h1) {
            int k, shown = 0;
            c->bad++;
            if (c->bad <= 8) {
                fprintf(stderr, "sync: blank %u: the frame run again from the saved state ends differently\n", vblank);
                for (k = 0; k + 1 < c->first.parts && shown < 6; k++) { /* (not the stack) */
                    const uint8_t *a = c->first.part[k].copy, *b = c->first.part[k].at;
                    size_t n = c->first.part[k].n, i;
                    for (i = 0; i < n && shown < 6; i++) {
                        if (a[i] != b[i]) {
                            size_t j = i;
                            while (j < n && a[j] != b[j]) {
                                j++;
                            }
                            fprintf(stderr, "sync:   %s %p, %u bytes: %02x %02x %02x %02x -> %02x %02x %02x %02x\n",
                                    k < sRegions ? sRegion[k].what : "port", (void *)(b + i), (unsigned)(j - i), a[i], a[i + 1], a[i + 2],
                                    a[i + 3], b[i], b[i + 1], b[i + 2], b[i + 3]);
                            shown++;
                            i = j + 64;
                        }
                    }
                }
            }
        }
        if (c->frames % 600 == 0) {
            fprintf(stderr, "sync: %u frames each run twice from a saved state, %u ended differently\n", c->frames, c->bad);
        }
    }
    c->phase = 1;
    if (state_save(&c->snap) != 0) {
        /* back here after the restore: the same frame runs a second time (phase 2 was set before the load) */
    }
}
#else
void Port_SyncTest(unsigned vblank) { (void)vblank; }
#endif
