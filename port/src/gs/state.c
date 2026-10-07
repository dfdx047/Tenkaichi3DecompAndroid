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
#include <SDL3/SDL.h>
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

int gPortResim; /* gs_internal.h: frames are being re-run, without picture or sound */

#define MAX_REGIONS 64
static struct { const uint8_t *p; size_t n; const char *what; } sRegion[MAX_REGIONS];
static int sRegions = -1;
static int sListed; /* the list of the game's variables was found: without it a saved state is not whole */

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
            sListed = sRegions > 0;
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
    {
        extern long Port_PadPlayPos(void); /* plat_stub.c: how far a pad recording has been played */
        long pos = Port_PadPlayPos();
        if (pos >= 0) {
            fprintf(fp, " pad %ld", pos);
        }
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
 * Saving and restoring the game's state, and the test of it. (The 64-bit programs: Linux and Windows.)
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
 *   BT3_SYNCTEST_DEPTH=<n>  rewind n blanks at a time instead of one (up to 64): save, run n blanks noting each
 *                    one's checksum, restore, run the n again and compare each. What online play does when an
 *                    input arrives late: several frames are undone and replayed.
 * ------------------------------------------------------------------------------------------------------------- */
#if defined(__x86_64__)
#ifdef _WIN32
/* Windows: the registers are kept with the compiler's own minimal setjmp (frame, stack pointer and the place to
   continue; everything else counts as lost over it, which is what a restore is), and the copy back runs on another
   stack through the port's stack switch (plat_mem.c). */
extern void Port_CallOnStack(void (*fn)(void), void *top);
#else
#include <ucontext.h>
#endif

extern int Port_StateExtra(void **p, size_t *n, int max); /* plat_mem.c */
extern uint8_t *Port_GameStackTop(void);
extern long Port_PadPlayPos(void);                         /* plat_stub.c */
extern void Port_PadPlaySeek(long pos);

#define MAX_PARTS (MAX_REGIONS + 8)
typedef struct Snapshot {
    struct { uint8_t *at; size_t n; uint8_t *copy; size_t cap; } part[MAX_PARTS];
    int parts;
#ifdef _WIN32
    void *jb[5];
#else
    ucontext_t ctx;
#endif
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
static int sSaveSmall; /* state_save leaves out the regions the page tracking covers (the roll ring keeps those) */
static int tracked_part(const uint8_t *p);

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
        if (!(sSaveSmall && tracked_part(sRegion[i].p))) {
            part_save(s, (uint8_t *)sRegion[i].p, sRegion[i].n);
        }
    }
    extra = Port_StateExtra(xp, xn, 8);
    for (i = 0; i < extra; i++) {
        if (!(sSaveSmall && tracked_part(xp[i]))) {
            part_save(s, xp[i], xn[i]);
        }
    }
    s->padPos = Port_PadPlayPos();
    s->loaded = 0;
    part_save(s, sp, (size_t)(top - sp)); /* the stack last: this frame's own contents as they are now */
#ifdef _WIN32
    if (__builtin_setjmp(s->jb) != 0) {
        return 1;
    }
    return 0;
#else
    getcontext(&s->ctx);
    return s->loaded;
#endif
}

static Snapshot *sLoading;
#ifndef _WIN32
static ucontext_t sLoaderCtx;
#endif

static void loader(void) {
    Snapshot *s = sLoading;
    int k;
    for (k = 0; k < s->parts; k++) {
        memcpy(s->part[k].at, s->part[k].copy, s->part[k].n);
    }
    Port_PadPlaySeek(s->padPos);
    s->loaded = 1;
#ifdef _WIN32
    __builtin_longjmp(s->jb, 1);
#else
    setcontext(&s->ctx);
#endif
}

static void roll_forget(void);

static void state_load(Snapshot *s) {
    static uint8_t *stack;
    if (stack == NULL) {
        stack = malloc(1 << 20);
    }
    roll_forget();
    sLoading = s;
#ifdef _WIN32
    Port_CallOnStack(loader, stack + (1 << 20) - 64); /* (does not come back: the loader continues at the save) */
#else
    getcontext(&sLoaderCtx);
    sLoaderCtx.uc_stack.ss_sp = stack;
    sLoaderCtx.uc_stack.ss_size = 1 << 20;
    sLoaderCtx.uc_link = NULL;
    makecontext(&sLoaderCtx, loader, 0);
    setcontext(&sLoaderCtx);
#endif
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

/* ---------------------------------------------------------------------------------------------------------------
 * The roll ring: a saved state per vertical blank, cheap enough to keep many (rollback).
 *
 * Nearly all of a state is the game's heap and the port's block region (30 MB and more), of which a blank changes
 * about one page in a hundred. So those two are not copied: the system is asked which pages were written
 * (Linux: the pages are read-only and the first write to one arrives here as a fault, which notes the page and
 * opens it; Windows: the regions are allocated with write tracking and GetWriteWatch lists the pages), a copy of
 * both regions as of the latest save is kept (the shadow), and at each save the pages written since the save
 * before go, as they were then (from the shadow), to that earlier save's own list. Going back k saves: the pages
 * written since the latest save come back from the shadow, then each save's list is put back, newest first.
 * The small rest (the game's variables, the stack, the registers) is copied whole per save, as before.
 *
 * A read() into a read-only page does not fault, it fails: whatever lets the system write file data straight
 * into the game's memory calls Port_StateTouch first.
 * ------------------------------------------------------------------------------------------------------------- */
#define PG 4096u
#define ROLL_MAX 64

typedef struct Track { uint8_t *base, *shadow; uint32_t pages, first; } Track; /* first: its first global page number */
static Track sTrack[2];
static int sTracks, sTracking;
static uint32_t sTrackPages;       /* global page numbers in use */
static uint32_t *sDirty;           /* global page numbers written since the last collect */
static volatile uint32_t sDirtyCount;
static volatile uint8_t *sDirtyMark; /* per global page: in sDirty already */
static uint32_t *sStamp;           /* per global page: the save whose list holds it already */
static uint32_t sArenaPages;       /* pages of the block region in use at the last collect */

typedef struct Roll {
    Snapshot small;
    uint32_t *pg;
    uint8_t *data;
    uint32_t n, cap, id;
} Roll;
static Roll *sRoll[ROLL_MAX];
static int sRolls;
static uint32_t sRollId;

static int tracked_part(const uint8_t *p) {
    int t;
    for (t = 0; t < sTracks; t++) {
        if (p == sTrack[t].base) {
            return 1;
        }
    }
    return 0;
}

static inline uint8_t *page_at(uint32_t g, uint8_t **shadow) {
    Track *t = &sTrack[g >= sTrack[1].first && sTracks > 1];
    *shadow = t->shadow + (size_t)(g - t->first) * PG;
    return t->base + (size_t)(g - t->first) * PG;
}

static inline void dirty_note(uint32_t g) {
    if (!__atomic_exchange_n(&sDirtyMark[g], 1, __ATOMIC_SEQ_CST)) {
        sDirty[__atomic_fetch_add(&sDirtyCount, 1, __ATOMIC_SEQ_CST)] = g;
    }
}

#ifndef _WIN32
#include <signal.h>
#include <sys/mman.h>
static struct sigaction sOldSegv;

static void on_write_fault(int sig, siginfo_t *si, void *uc) {
    uintptr_t a = (uintptr_t)si->si_addr;
    int t;
    for (t = 0; t < sTracks; t++) {
        uintptr_t b = (uintptr_t)sTrack[t].base;
        if (a >= b && a < b + (uintptr_t)sTrack[t].pages * PG) {
            uint32_t page = (uint32_t)((a - b) / PG);
            dirty_note(sTrack[t].first + page);
            mprotect((void *)(b + (uintptr_t)page * PG), PG, PROT_READ | PROT_WRITE);
            return;
        }
    }
    /* not ours: to whoever handled it before, or to the default (the write is tried again and ends the program) */
    if (sOldSegv.sa_flags & SA_SIGINFO) {
        sOldSegv.sa_sigaction(sig, si, uc);
    } else if (sOldSegv.sa_handler != SIG_DFL && sOldSegv.sa_handler != SIG_IGN) {
        sOldSegv.sa_handler(sig);
    } else {
        sigaction(SIGSEGV, &sOldSegv, NULL);
    }
}

static void *shadow_alloc(size_t n) {
    void *p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
#else
static void *shadow_alloc(size_t n) {
    return VirtualAlloc(NULL, n, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}
#endif

/* A page of a tracked region is about to be written by other means than an ordinary store. */
static void touch_page(uint32_t g) {
    if (!sDirtyMark[g]) {
#ifndef _WIN32
        uint8_t *sh, *m = page_at(g, &sh);
        mprotect(m, PG, PROT_READ | PROT_WRITE);
#endif
        dirty_note(g);
    }
}

void Port_StateTouch(void *p, unsigned long n) {
    uintptr_t a = (uintptr_t)p;
    int t;
    if (!sTracking || n == 0) {
        return;
    }
    for (t = 0; t < sTracks; t++) {
        uintptr_t b = (uintptr_t)sTrack[t].base, e = b + (uintptr_t)sTrack[t].pages * PG;
        if (a + n > b && a < e) {
            uintptr_t lo = a < b ? b : a, hi = a + n > e ? e : a + n, q;
            for (q = (lo - b) / PG; q <= (hi - 1 - b) / PG; q++) {
                touch_page(sTrack[t].first + (uint32_t)q);
            }
        }
    }
}

static void track_start(void) {
    void *xp[8];
    size_t xn[8];
    uint8_t *heap;
    uint32_t heapSize;
    int extra, t;

    if (sRegions < 0) {
        regions_init();
    }
    Port_HeapRegion(&heap, &heapSize);
    sTrack[0].base = heap;
    sTrack[0].pages = heapSize / PG;
    sTrack[0].first = 0;
    sTracks = 1;
    sTrackPages = sTrack[0].pages;
    extra = Port_StateExtra(xp, xn, 8);
    if (extra >= 4) { /* the block region: all of its address range, used or not yet */
        extern uint32_t Port_LowRegionSize(void); /* plat_mem.c */
        sTrack[1].base = xp[3];
        sTrack[1].pages = Port_LowRegionSize() / PG;
        sTrack[1].first = sTrackPages;
        sTrackPages += sTrack[1].pages;
        sTracks = 2;
        sArenaPages = (uint32_t)((xn[3] + PG - 1) / PG);
    }
    sDirty = malloc(sizeof(uint32_t) * sTrackPages);
    sDirtyMark = calloc(1, sTrackPages);
    sStamp = calloc(sizeof(uint32_t), sTrackPages);
    for (t = 0; t < sTracks; t++) {
        size_t used = (size_t)(t == 0 ? sTrack[t].pages : sArenaPages) * PG;
        sTrack[t].shadow = shadow_alloc((size_t)sTrack[t].pages * PG);
        if (sTrack[t].shadow == NULL) {
            fprintf(stderr, "bt3: state: no memory for the copy of the game's memory\n");
            exit(2);
        }
        memcpy(sTrack[t].shadow, sTrack[t].base, used);
    }
#ifndef _WIN32
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = on_write_fault;
        sa.sa_flags = SA_SIGINFO | SA_NODEFER;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGSEGV, &sa, &sOldSegv);
        mprotect(sTrack[0].base, (size_t)sTrack[0].pages * PG, PROT_READ);
        if (sTracks > 1 && sArenaPages != 0) {
            mprotect(sTrack[1].base, (size_t)sArenaPages * PG, PROT_READ);
        }
    }
#else
    for (t = 0; t < sTracks; t++) {
        ResetWriteWatch(sTrack[t].base, (size_t)(t == 0 ? sTrack[t].pages : sArenaPages) * PG);
    }
#endif
    sTracking = 1;
}

/* The pages written since the last collect, into sDirty (each once). Returns how many. */
static uint32_t dirty_collect(void) {
    void *xp[8];
    size_t xn[8];
    uint32_t used = sArenaPages, g;

    if (sTracks > 1 && Port_StateExtra(xp, xn, 8) >= 4) {
        used = (uint32_t)((xn[3] + PG - 1) / PG);
    }
#ifdef _WIN32
    {
        static void **addr;
        int t;
        if (addr == NULL) {
            addr = malloc(sizeof(void *) * sTrackPages);
        }
        for (t = 0; t < sTracks; t++) {
            ULONG_PTR count = sTrackPages;
            DWORD gran = 0;
            size_t len = (size_t)(t == 0 ? sTrack[t].pages : (used > sArenaPages ? used : sArenaPages)) * PG;
            ULONG_PTR i;
            UINT rc = len != 0 ? GetWriteWatch(WRITE_WATCH_FLAG_RESET, sTrack[t].base, len, addr, &count, &gran) : 0;
            if (rc != 0) {
                static int said;
                if (!said++) {
                    fprintf(stderr, "bt3: state: the system does not list the written pages of %p + %u MB (error %lu)\n", (void *)sTrack[t].base,
                            (unsigned)(len >> 20), (unsigned long)GetLastError());
                }
            }
            if (len != 0 && rc == 0) {
                for (i = 0; i < count; i++) {
                    dirty_note(sTrack[t].first + (uint32_t)(((uintptr_t)addr[i] - (uintptr_t)sTrack[t].base) / PG));
                }
            }
        }
    }
#endif
    /* the block region grew: its new pages count as written (they were nothing before: the shadow's zeros) */
    for (g = sArenaPages; g < used; g++) {
        dirty_note(sTrack[1].first + g);
    }
    if (used > sArenaPages) {
        sArenaPages = used;
    }
    return sDirtyCount;
}

static int cmp_u32(const void *a, const void *b);

/* After a collect has been dealt with: nothing counts as written, every page is watched again. */
static void dirty_rearm(void) {
    uint32_t i, n = sDirtyCount;
#ifndef _WIN32
    /* read-only again, neighbours in one call (the list is in the order of the writes: sort it first) */
    uint32_t j;
    qsort(sDirty, n, sizeof(sDirty[0]), cmp_u32);
    for (i = 0; i < n; i = j) {
        uint8_t *sh, *m = page_at(sDirty[i], &sh);
        int second = sDirty[i] >= sTrack[1].first && sTracks > 1;
        for (j = i + 1; j < n && sDirty[j] == sDirty[j - 1] + 1 && (sDirty[j] >= sTrack[1].first && sTracks > 1) == second; j++) {
        }
        mprotect(m, (size_t)(j - i) * PG, PROT_READ);
    }
#endif
    for (i = 0; i < n; i++) {
        sDirtyMark[sDirty[i]] = 0;
    }
    sDirtyCount = 0;
}

/* A whole state is about to be put back by other means than roll_back (state_load): the ring's saves do not
   lead to it. (Its pages arrive as written at the next collect, and the shadow follows.) */
static int sRollLoading;
static void roll_forget(void) {
    if (!sRollLoading) {
        sRolls = 0;
    }
    sRollLoading = 0;
}

static int cmp_u32(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

/* Saves the state as the newest of the ring. Returns 0 after saving, 1 when a roll_back has come back to it. */
static int __attribute__((noinline)) roll_save(void) {
    Roll *r, *prev;
    uint32_t i, n;
    int back;

    if (!sTracking) {
        track_start();
    }
    n = dirty_collect();
    prev = sRolls != 0 ? sRoll[sRolls - 1] : NULL;
    for (i = 0; i < n; i++) {
        uint32_t g = sDirty[i];
        uint8_t *sh, *m = page_at(g, &sh);
        if (prev != NULL && sStamp[g] != prev->id) { /* as the page was at the save before: into that save's list */
            if (prev->n == prev->cap) {
                prev->cap = prev->cap != 0 ? prev->cap * 2 : 256;
                prev->pg = realloc(prev->pg, sizeof(uint32_t) * prev->cap);
                prev->data = realloc(prev->data, (size_t)prev->cap * PG);
            }
            prev->pg[prev->n] = g;
            memcpy(prev->data + (size_t)prev->n * PG, sh, PG);
            prev->n++;
            sStamp[g] = prev->id;
        }
        memcpy(sh, m, PG);
    }
    dirty_rearm();
    if (sRolls == ROLL_MAX) { /* the oldest goes; its storage is used for the new one */
        r = sRoll[0];
        memmove(&sRoll[0], &sRoll[1], sizeof(sRoll[0]) * (ROLL_MAX - 1));
        sRolls--;
    } else if (sRoll[sRolls] != NULL) {
        r = sRoll[sRolls];
    } else {
        r = calloc(1, sizeof(*r));
    }
    sRoll[sRolls++] = r;
    r->n = 0;
    r->id = ++sRollId;
    sSaveSmall = 1;
    back = state_save(&r->small);
    sSaveSmall = 0;
    return back;
}

/* Back to the state of k saves ago (1: the newest). Does not return: roll_save of that save returns 1. */
static void roll_back(int k) {
    int target = sRolls - k, j;
    uint32_t i, n;
    Roll *r;

    if (target < 0) {
        fprintf(stderr, "bt3: state: asked to go back %d saves, the ring holds %d\n", k, sRolls);
        exit(2);
    }
    /* what was written since the newest save: back from the shadow (the pages stay noted as written) */
    n = dirty_collect();
    for (i = 0; i < n; i++) {
        uint8_t *sh, *m = page_at(sDirty[i], &sh);
        touch_page(sDirty[i]);
        memcpy(m, sh, PG);
    }
    /* then each save's own pages, from the one before the newest down to the target */
    for (j = sRolls - 2; j >= target; j--) {
        r = sRoll[j];
        for (i = 0; i < r->n; i++) {
            uint8_t *sh, *m = page_at(r->pg[i], &sh);
            touch_page(r->pg[i]);
            memcpy(m, r->data + (size_t)i * PG, PG);
            memcpy(sh, r->data + (size_t)i * PG, PG);
        }
    }
    r = sRoll[target];
    for (j = target + 1; j < sRolls; j++) { /* the newer saves are gone (their storage is kept for reuse) */
        sRoll[j]->n = 0;
    }
    sRolls = target + 1;
    for (i = 0; i < r->n; i++) { /* the target's list is still right: the pages in it are not noted a second time */
        sStamp[r->pg[i]] = r->id;
    }
    sRollLoading = 1;
    state_load(&r->small);
}

#define SYNC_MAX_DEPTH 64

void Port_SyncTest(unsigned vblank) {
    static struct Ctl {
        int mode, phase, depth, step, roll;
        unsigned from, frames, bad;
        uint64_t h1[SYNC_MAX_DEPTH + 1];
        uint64_t tPrev, nsSilent, nsLoud, nsSave, nsLoad, tLoad; /* how long things take (reported with the count) */
        unsigned nSilent, nLoud, nSave, nLoad;
        Snapshot snap, first;
    } *c;
    uint64_t tNow = SDL_GetTicksNS();

    if (c == NULL) {
        const char *e = getenv("BT3_SYNCTEST"), *d = getenv("BT3_SYNCTEST_DEPTH");
        c = calloc(1, sizeof(*c)); /* (outside everything a snapshot restores) */
        c->mode = e != NULL;
        c->from = e != NULL ? (unsigned)atoi(e) : 0;
        c->depth = d != NULL ? atoi(d) : 1;
        c->roll = getenv("BT3_SYNCTEST_ROLL") != NULL; /* with the roll ring (pages written, not whole copies) */
        if (c->depth < 1) { c->depth = 1; }
        if (c->depth > SYNC_MAX_DEPTH) { c->depth = SYNC_MAX_DEPTH; }
    }
    if (!c->mode || vblank < c->from) {
        return;
    }
    if (sRegions < 0) {
        regions_init();
    }
    if (c->phase == 1) {
        /* first pass: note what each blank of the stretch ended with; after the last, go back to its start */
        c->nsSilent += tNow - c->tPrev;
        c->nSilent++;
        c->h1[++c->step] = parts_hash();
        c->tPrev = SDL_GetTicksNS();
        if (c->step < c->depth) {
            if (c->roll) { /* a save at every blank, as rollback makes them */
                tNow = SDL_GetTicksNS();
                roll_save();
                c->nsSave += SDL_GetTicksNS() - tNow;
                c->nSave++;
                c->tPrev = SDL_GetTicksNS();
            }
            return;
        }
        c->phase = 2;
        c->step = 0;
        if (c->roll) {
            c->tLoad = SDL_GetTicksNS();
            roll_back(c->depth);
        }
        if (c->depth > 1 || state_save(&c->first) == 0) { /* (depth 1: kept to say WHERE a difference is) */
            c->tLoad = SDL_GetTicksNS();
            state_load(&c->snap);
        }
        return; /* not reached: the load continues in the save below, `depth` blanks ago */
    }
    if (c->phase == 2) {
        uint64_t h2;
        c->nsLoud += tNow - c->tPrev;
        c->nLoud++;
        h2 = parts_hash();
        c->tPrev = SDL_GetTicksNS();
        c->step++;
        if (h2 != c->h1[c->step]) {
            int k, shown = 0;
            c->bad++;
            if (c->bad <= 8) {
                fprintf(stderr, "sync: blank %u (step %d of %d after the restore): run again from the saved state it ends differently\n",
                        vblank, c->step, c->depth);
                for (k = 0; c->depth == 1 && k + 1 < c->first.parts && shown < 6; k++) { /* (not the stack) */
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
        if (c->step < c->depth) {
            if (c->roll) {
                roll_save();
                c->tPrev = SDL_GetTicksNS();
            }
            return;
        }
        c->frames += (unsigned)c->depth;
        if (c->frames / 600 != (c->frames - (unsigned)c->depth) / 600) {
            fprintf(stderr, "sync: %u blanks each run twice, rewinding %d at a time; %u ended differently\n", c->frames, c->depth, c->bad);
            if (getenv("BT3_GS_VERBOSE") != NULL && c->nSilent != 0 && c->nLoud != 0 && c->nSave != 0 && c->nLoad != 0) {
                fprintf(stderr, "sync:   per blank: re-run without output %.2f ms, with output %.2f ms; saving the state %.2f ms, restoring it %.2f ms\n",
                        (double)c->nsSilent / c->nSilent / 1e6, (double)c->nsLoud / c->nLoud / 1e6, (double)c->nsSave / c->nSave / 1e6,
                        (double)c->nsLoad / c->nLoad / 1e6);
            }
            c->nsSilent = c->nsLoud = c->nsSave = c->nsLoad = 0;
            c->nSilent = c->nLoud = c->nSave = c->nLoad = 0;
        }
    }
    c->phase = 1;
    c->step = 0;
    tNow = SDL_GetTicksNS();
    if ((c->roll ? roll_save() : state_save(&c->snap)) != 0) {
        c->nsLoad += SDL_GetTicksNS() - c->tLoad;
        c->nLoad++;
        c->tPrev = SDL_GetTicksNS();
        /* back here after the restore: the same stretch runs a second time (phase 2 was set before the load),
           this time with its picture and sound */
        gPortResim = 0;
    } else {
        /* the pass that will be undone runs without picture and sound (BT3_SYNCTEST_LOUD=1: with them, as the
           test first did): what online play does with the frames it re-runs. It has to leave the same state. */
        gPortResim = getenv("BT3_SYNCTEST_LOUD") == NULL;
        c->nsSave += SDL_GetTicksNS() - tNow;
        c->nSave++;
        c->tPrev = SDL_GetTicksNS();
    }
}

/* ---------------------------------------------------------------------------------------------------------------
 * Exchanging the game's state: into an online session and back, without starting the program again.
 *
 * Two copies of the game stay in step only if they start from the same state. So a snapshot is taken at the very
 * first vertical blank of every run (sBoot: the same on every machine with this program); to begin a session the
 * game's present state is kept (sOwn, with the emulated GS's memory, which is not in a snapshot) and the game is
 * put back to that first blank, now as a session (gs/net.c); when the session ends, the kept state is put back
 * and the player is where they were, with their own save. The sound is made to fit the state each time.
 * ------------------------------------------------------------------------------------------------------------- */
extern void Gs_StateKeep(void), Gs_StateBack(void);                       /* gs_core.c */
extern void Port_AdxResync(void);                                         /* snd_adx.c */
extern void Port_NetSessionBegin(int role, const char *address, int port); /* net.c */
extern void Port_NetSessionEnd(void);
volatile int gPortNetWindowClose; /* for the overlay (ui.cpp): back from a session, the online window closes.
                                     (Here and not with the game's variables: it is not part of the state.) */

static Snapshot sBoot, sOwn;
static int sBootTaken, sReqRole, sReqPort;
static volatile int sReq;
static char sReqAddr[128];

void Port_SessionReturn(void);

int Port_SessionCan(void) {
    return sBootTaken > 0;
}

void Port_SessionRequest(int role, const char *address, int port) {
    sReqRole = role;
    sReqPort = port;
    snprintf(sReqAddr, sizeof(sReqAddr), "%s", address != NULL ? address : "");
    sReq = 1;
}

/* At the top of every vertical blank, on the game's thread (Port_VBlank). */
void Port_SessionPoll(void) {
    if (sBootTaken < 0) {
        return;
    }
    if (!sBootTaken) {
        if (sRegions < 0) {
            regions_init();
        }
        if (!sListed) { /* no whole state without the list: sessions start the program again, as before */
            sBootTaken = -1;
            return;
        }
        sBootTaken = 1;
        if (state_save(&sBoot) != 0) {
            Port_AdxResync(); /* a session begins: this is the game's first blank again; whatever sounded stops */
        }
        return;
    }
    {
        /* testing: BT3_SESSION_TEST=<role>:<address>:<port>:<blank> asks for a session at that blank, and
           BT3_SESSION_LEAVE=<n> leaves it n blanks after it began; BT3_SESSION_AGAIN=<n> asks for the next one n
           blanks after each return */
        static unsigned blank, began;
        static int role, port, at = -1, leave;
        static char addr[128];
        if (at < 0) {
            const char *t = getenv("BT3_SESSION_TEST");
            at = 0;
            if (t != NULL && sscanf(t, "%d:%127[^:]:%d:%d", &role, addr, &port, &at) != 4) {
                at = 0;
            }
            leave = getenv("BT3_SESSION_LEAVE") != NULL ? atoi(getenv("BT3_SESSION_LEAVE")) : 0;
        }
        blank++;
        if (at > 0 && blank == (unsigned)at) {
            fprintf(stderr, "bt3: session test: asked for at blank %u\n", blank);
            began = blank;
            Port_SessionRequest(role, addr, port);
        } else if (began != 0 && leave > 0 && blank == began + (unsigned)leave) {
            fprintf(stderr, "bt3: session test: leaving\n");
            began = 0;
            if (getenv("BT3_SESSION_AGAIN") != NULL) { /* and another one, that many blanks later */
                at = (int)blank + atoi(getenv("BT3_SESSION_AGAIN"));
            }
            Port_SessionReturn();
        }
    }
    if (sReq) {
        sReq = 0;
        Gs_StateKeep();
        if (state_save(&sOwn) == 0) {
            Port_NetSessionBegin(sReqRole, sReqAddr, sReqPort); /* connects (waits for the other side) */
            state_load(&sBoot);
        } else {
            /* back from the session, at the blank it was asked for */
            gPortResim = 0; /* (a session left before its start-up was over was still running without picture) */
            Gs_StateBack();
            Port_AdxResync();
            gPortNetWindowClose = 1;
        }
    }
}

/* The session is over: the kept state comes back (does not return). */
void Port_SessionReturn(void) {
    Port_NetSessionEnd();
    state_load(&sOwn);
}
#else
void Port_StateTouch(void *p, unsigned long n) { (void)p; (void)n; }
volatile int gPortNetWindowClose;
void Port_SyncTest(unsigned vblank) { (void)vblank; }
int Port_SessionCan(void) { return 0; }
void Port_SessionRequest(int role, const char *address, int port) { (void)role; (void)address; (void)port; }
void Port_SessionPoll(void) {}
void Port_SessionReturn(void) {}
#endif
