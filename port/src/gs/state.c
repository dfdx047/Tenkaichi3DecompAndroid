/*
 * The game's state as one number: a checksum of everything the game itself owns in memory. For online play two
 * machines have to stay bit for bit the same; this is how that is checked, first of all between two runs on one
 * machine.
 *
 * What it covers: the game's global variables (the address ranges port/tools/make_state.py took from the linker's
 * map, in <program>.mem next to the program), the game's heap and the scratchpad. Not covered: the port's own
 * memory (renderer, sound, input), and the game thread's stack and registers.
 *
 *   BT3_HASH=<file>       one line per vertical blank: its number and the checksum
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
void Port_StateLog(unsigned vblank) {
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
    fprintf(fp, "%u %016llx\n", vblank, (unsigned long long)Port_StateHash());
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
