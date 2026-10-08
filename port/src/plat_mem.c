/*
 * Memory of the PC build.
 *
 * The game was written for a machine with 32 MB at fixed addresses and it stores pointers in 32-bit fields, so the
 * 32-bit PC build gives it memory at PS2 addresses:
 *   0x003BE730..0x02000000  the game heap. The game takes it with ONE malloc at start-up (Heap_Create,
 *                           0x1EFB000 minus the end of the menu overlay); Port_Malloc returns the address the
 *                           PS2's malloc returns (0x3BE730, read from a save state), so heap pointers have the
 *                           same values as on the console and memory can be compared with an emulator dump.
 *   0x10000000, 0x12000000  hardware registers (timers, DMA, GS): plain memory here, so reads give what was last
 *                           written (0 at start: "transfer finished").
 *   0x70000000              the 16 KB scratchpad.
 */
#include "port_host.h"
/* The 64-bit builds (x86-64 PC, arm64 Android): the game's pointers stay 4 bytes (port/tools/ptr32.py), so all of
   its memory, the program included, has to lie below 4 GB. */
#if defined(__x86_64__) || defined(__aarch64__)
#define PORT_LP64 1
#else
#define PORT_LP64 0
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEAP_BASE 0x003BE000u  /* page that holds the first address */
#define HEAP_FIRST 0x003BE730u /* what the PS2's malloc returns for the game heap (gHeapStart in a PCSX2 save state) */
#define HEAP_END 0x02000000u

static uint32_t sHeapNext = HEAP_FIRST, sHeapEnd = HEAP_END;

/* How far the start got, for the crash report (plat_crash.c). */
const char *volatile gPortStage = "starting";

/* Where the game's own part of its stack ends (the stack the game's main() runs on). Saving and restoring the
   game's state covers the stack up to here. On Linux the thread library's own data for the thread lies above it
   (its control block and thread-local variables, the C library's per-thread memory cache among them), which a
   restore must leave alone. */
static uint8_t *sGameStackTop;

#ifdef _WIN32
#include <direct.h> /* chdir */
/* ------------------------------------------------------------------------------------------------------ Windows
   The same needs, with Windows' means (the 64-bit build only; its pointers are 4 bytes wide, see ptr32.py):
     - the program is linked at 0x20000000 and not relocated (port/tools/link.py);
     - the hardware-register areas and the scratchpad are reserved at their PS2 addresses, which a 64-bit Windows
       process leaves free;
     - the game heap: at its PS2 address if that is free, else anywhere below 2 GB. A Windows process has its first
       stack and heap in that low area, so the PS2 address is often taken; nothing in the game depends on it (only
       comparing memory with a console dump does), and Port_Malloc just starts from wherever the region is;
     - the game's stack: if the process's own stack is above 4 GB, main() runs on a stack reserved low. */
#include <windows.h>

/* Reserves `size` bytes somewhere between 0x30000000 and 2 GB (clear of the program at 0x20000000). */
static void *low_alloc(size_t size) {
    uintptr_t addr = 0x30000000u;
    MEMORY_BASIC_INFORMATION mbi;

    size = (size + 0xFFFF) & ~(size_t)0xFFFF;
    while (addr < 0x7F000000u && VirtualQuery((void *)addr, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = ((uintptr_t)mbi.BaseAddress + 0xFFFF) & ~(uintptr_t)0xFFFF;
        uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (mbi.State == MEM_FREE && base + size <= end && base + size <= 0x7F000000u) {
            void *p = VirtualAlloc((void *)base, size, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE); /* (the heap may land here) */
            if (p != NULL) {
                return p;
            }
        }
        addr = end > addr ? end : addr + 0x10000;
    }
    return NULL;
}

static void map(uint32_t addr, uint32_t size, const char *what) {
    uint32_t base = addr & ~0xFFFFu; /* reservations start on 64 KB boundaries */

    if (VirtualAlloc((void *)(uintptr_t)base, size + (addr - base), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) != (void *)(uintptr_t)base) {
        fprintf(stderr, "bt3: cannot reserve %s at 0x%08X (error %lu)\n", what, addr, (unsigned long)GetLastError());
        exit(2);
    }
}

static void map_heap(void) {
    uint32_t base = HEAP_BASE & ~0xFFFFu;
    /* (write tracking: gs/state.c asks which of the heap's pages were written; costs nothing until it does) */
    uint8_t *p = VirtualAlloc((void *)(uintptr_t)base, HEAP_END - base, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);

    if (p != NULL) {
        return; /* at the PS2's address */
    }
    p = low_alloc(HEAP_END - HEAP_BASE);
    if (p == NULL) {
        fprintf(stderr, "bt3: no room for the game heap (%u MB) below 2 GB\n", (HEAP_END - HEAP_BASE) >> 20);
        exit(2);
    }
    sHeapNext = (uint32_t)(uintptr_t)p + (HEAP_FIRST - HEAP_BASE);
    sHeapEnd = (uint32_t)(uintptr_t)p + (HEAP_END - HEAP_BASE);
}

/* The same search, reserving the address range only (the region of Port_LowAlloc commits its pages as it grows). */
static void *low_reserve(size_t size) {
    uintptr_t addr = 0x30000000u;
    MEMORY_BASIC_INFORMATION mbi;

    size = (size + 0xFFFF) & ~(size_t)0xFFFF;
    while (addr < 0x7F000000u && VirtualQuery((void *)addr, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = ((uintptr_t)mbi.BaseAddress + 0xFFFF) & ~(uintptr_t)0xFFFF;
        uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (mbi.State == MEM_FREE && base + size <= end && base + size <= 0x7F000000u) {
            void *p = VirtualAlloc((void *)base, size, MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE);
            if (p != NULL) {
                return p;
            }
        }
        addr = end > addr ? end : addr + 0x10000;
    }
    return NULL;
}

extern int __real_main(int argc, char **argv);
extern void Port_CallOnStack(void (*fn)(void), void *top);
PORT_HOST static int sArgc = 0, sResult = 0; /* (the process's own arguments: an address on its stack, not game state) */
PORT_HOST static char **sArgv = NULL;

/* fn() with the stack pointer at `top` (Windows x64 calling convention: fn in rcx, top in rdx). */
__asm__(".text\n.globl Port_CallOnStack\nPort_CallOnStack:\n"
        "    pushq %rbp\n    movq %rsp, %rbp\n    movq %rdx, %rsp\n    subq $32, %rsp\n    callq *%rcx\n"
        "    movq %rbp, %rsp\n    popq %rbp\n    retq\n");

static void game_main(void) {
    sResult = __real_main(sArgc, sArgv);
}

int __wrap_main(int argc, char **argv) {
    enum { STACK_SIZE = 0x01000000 };
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    void **dealloc = (void **)((uint8_t *)tib + 0x1478); /* TEB.DeallocationStack */
    void *base = tib->StackBase, *limit = tib->StackLimit, *old = *dealloc;
    uint8_t *stack;

    sArgc = argc;
    sArgv = argv;
    if (getenv("BT3_CRASH_TEST") != NULL) { /* testing the crash report */
        *(volatile int *)(uintptr_t)8 = 0;
    }
    if (getenv("BT3_STACK_TEST") != NULL) { /* testing the move to a low stack where the process's own is low already */
        tib = (NT_TIB *)NtCurrentTeb();
    } else
    if ((uintptr_t)&stack < 0xFFF00000u) {
        sGameStackTop = (uint8_t *)__builtin_frame_address(0);
        gPortStage = "the game is running (on the process's own stack)";
        return __real_main(argc, argv); /* the process's own stack is low already */
    }
    gPortStage = "moving to a stack below 4 GB";
    stack = low_alloc(STACK_SIZE);
    if (stack == NULL) {
        fprintf(stderr, "bt3: no room for the game's stack below 2 GB\n");
        return 2;
    }
    tib->StackBase = stack + STACK_SIZE; /* the system checks these on exceptions and when the stack grows */
    tib->StackLimit = stack;
    *dealloc = stack;
    sGameStackTop = stack + STACK_SIZE;
    gPortStage = "the game is running (on a stack of its own below 4 GB)";
    Port_CallOnStack(game_main, stack + STACK_SIZE);
    tib->StackBase = base;
    tib->StackLimit = limit;
    *dealloc = old;
    return sResult;
}

#else
/* -------------------------------------------------------------------------------------------------------- Linux */
#include <sys/mman.h>
#include <unistd.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
static void map(uint32_t addr, uint32_t size, const char *what) {
    /* whole pages around the region (the system's pages may be 16 KB: an Android device's) */
    uint32_t page = (uint32_t)sysconf(_SC_PAGESIZE), base = addr & ~(page - 1), len = (addr - base + size + page - 1) & ~(page - 1);
    void *p = mmap((void *)(uintptr_t)base, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                   -1, 0);

    if (p != (void *)(uintptr_t)base) {
        fprintf(stderr, "bt3: cannot map %s at 0x%08X\n", what, addr);
#ifdef __ANDROID__
        /* (straight to the system log too: the process ends before the app's log thread reads stderr) */
        __android_log_print(6 /* ANDROID_LOG_ERROR */, "bt3", "cannot map %s at 0x%08X", what, addr);
#endif
        exit(2);
    }
}

static void map_heap(void) {
    map(HEAP_BASE, HEAP_END - HEAP_BASE, "the game heap");
}

#if PORT_LP64
/* The 64-bit build: the game's pointers are still 4 bytes wide (port/tools/ptr32.py), so everything the game can
   point at has to lie below 4 GB.
     - the program itself: linked at 0x20000000 (port/tools/link.py), clear of the regions above;
     - the C library's heap: kept in the break area that follows the program (no mmap, one arena), because
       handles the port gives to the game (files, sound players) come from it;
     - the stack of the thread that runs the game: the game stores addresses of local variables, so its main()
       runs on a thread whose stack is mapped right above the heap (the process's own stack is far above 4 GB).
       Below the program at 0x20000000 on purpose: the C library's own heap starts at a random place in the
       gigabyte ABOVE the program, and a fixed address there (this stack used to be at 0x60000000) is sometimes
       taken, which stopped the program at start now and then. */
#include <malloc.h>
#include <pthread.h>
#define STACK_BASE 0x02000000u
#define STACK_SIZE 0x01000000u
extern int __real_main(int argc, char **argv);
PORT_HOST static int sArgc = 0, sResult = 0; /* (the process's own arguments: an address on its stack, not game state) */
PORT_HOST static char **sArgv = NULL;

/* Where the game's own part of the thread's stack ends: above this frame lie the thread library's own data for
   the thread (its control block and thread-local variables are at the top of the stack it was given, among them
   the C library's per-thread memory cache). Saving and restoring the game's state must leave those alone. */
static void *game_thread(void *arg) {
    (void)arg;
    sGameStackTop = (uint8_t *)__builtin_frame_address(0);
    sResult = __real_main(sArgc, sArgv);
    return NULL;
}

int __wrap_main(int argc, char **argv) {
    pthread_attr_t attr;
    pthread_t th;

    sArgc = argc;
    sArgv = argv;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, (void *)(uintptr_t)STACK_BASE, STACK_SIZE);
    if (pthread_create(&th, &attr, game_thread, NULL) != 0) {
        fprintf(stderr, "bt3: cannot start the game thread\n");
        return 2;
    }
    pthread_join(th, NULL);
    return sResult;
}
#endif
#endif

/* ---- The game's own data, fetched from the user's disc (release builds).
   A program made by port/tools/strip_data.py has the data tables of the game's two programs and the VU1
   microprograms set to zero, and a list next to it (<program>.dat) of where each run of bytes comes from: the
   user's SLUS_216.78 (as a flat image from 0x100000) or BIN/DBZP.BIN. This copies them in before anything else
   runs. A program without that list still has the data linked in (the developer's build) and nothing happens. */
#define ROM_BASE 0x100000u
#define ROM_END 0x2FF180u

/* Set to 1 in the file by strip_data.py: this program does not work without its list. (2, not 0: it has to be in
   the file, not in the zero-filled part of memory.) */
volatile uint32_t gPortDataStripped = 2;

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *fp = fopen(path, "rb");
    uint8_t *buf;
    long n;

    if (fp == NULL) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = malloc((size_t)n + 1);
    if (buf == NULL || fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        fclose(fp);
        free(buf);
        return NULL;
    }
    fclose(fp);
    *size = (size_t)n;
    return buf;
}

static void data_fail(const char *what, const char *path) {
    fprintf(stderr, "bt3: %s%s%s\n     The game's data comes from your own disc image; run the setup (Tenkaichi3Decomp-setup) to unpack it.\n", what,
            path != NULL ? ": " : "", path != NULL ? path : "");
    exit(2);
}

static void Port_LoadGameData(void) {
    char exe[1024], path[1200];
    const char *root = getenv("BT3_DATA") != NULL ? getenv("BT3_DATA") : "gamedata";
    uint8_t *list, *elf, *src[2] = {NULL, NULL};
    size_t listSize = 0, elfSize = 0, srcSize[2] = {0, 0};
    uint64_t hash = 0xCBF29CE484222325ull, want;
    uint32_t count, i, k;
#ifdef _WIN32
    long n = (long)GetModuleFileNameA(NULL, exe, sizeof(exe) - 1);
#else
    long n = getenv("BT3_EXE") != NULL ? (long)snprintf(exe, sizeof(exe), "%s", getenv("BT3_EXE"))
                                        : (long)readlink("/proc/self/exe", exe, sizeof(exe) - 1);
#endif

    if (n <= 0 || n >= (long)sizeof(exe) - 1) {
        return;
    }
    exe[n] = '\0';
    if (gPortDataStripped == 1 && getenv("BT3_DATA") == NULL) {
        /* A release program started from another folder (a double click, a shortcut): its data, saves and
           settings are next to it. If there is no gamedata here but there is next to the program, work there. */
        char dir[1024], probe[1200];
        char *slash;
        FILE *here = fopen("gamedata/disc/SLUS_216.78", "rb");
        if (here != NULL) {
            fclose(here);
        } else {
            snprintf(dir, sizeof(dir), "%s", exe);
            slash = strrchr(dir, '/');
#ifdef _WIN32
            if (strrchr(dir, '\\') > slash) {
                slash = strrchr(dir, '\\');
            }
#endif
            if (slash != NULL) {
                *slash = '\0';
                snprintf(probe, sizeof(probe), "%s/gamedata/disc/SLUS_216.78", dir);
                here = fopen(probe, "rb");
                if (here != NULL) {
                    fclose(here);
                    if (chdir(dir) != 0) {
                    }
                }
            }
        }
    }
    if (n > 4 && (strcmp(exe + n - 4, ".exe") == 0 || strcmp(exe + n - 4, ".EXE") == 0)) {
        exe[n - 4] = '\0'; /* Name.exe keeps its list in Name.dat */
    }
    snprintf(path, sizeof(path), "%s.dat", exe);
    list = read_file(path, &listSize);
    if (list == NULL) {
        if (gPortDataStripped == 1) {
            data_fail("this program needs the file that came with it", path);
        }
        return; /* no list: the data is linked in */
    }
    if (listSize < 16 || memcmp(list, "BT3D", 4) != 0) {
        data_fail("damaged file", path);
    }
    memcpy(&count, list + 4, 4);
    memcpy(&want, list + 8, 8);
    if (listSize < 16 + (size_t)count * 16) {
        data_fail("damaged file", path);
    }
    snprintf(path, sizeof(path), "%s/disc/SLUS_216.78", root);
    elf = read_file(path, &elfSize);
    if (elf == NULL || elfSize < 0x34 || memcmp(elf, "\177ELF", 4) != 0) {
        data_fail("the game's program was not found", path);
    }
    {   /* the loaded sections as one image from 0x100000 */
        uint32_t shoff, sh[6];
        uint16_t shentsize, shnum;
        memcpy(&shoff, elf + 0x20, 4);
        memcpy(&shentsize, elf + 0x2E, 2);
        memcpy(&shnum, elf + 0x30, 2);
        srcSize[0] = ROM_END - ROM_BASE;
        src[0] = calloc(1, srcSize[0]);
        for (i = 0; i < shnum && (size_t)shoff + (size_t)(i + 1) * shentsize <= elfSize; i++) {
            memcpy(sh, elf + shoff + (size_t)i * shentsize, sizeof(sh)); /* name, type, flags, addr, offset, size */
            if ((sh[2] & 2) && sh[1] != 8 && sh[5] != 0 && sh[3] >= ROM_BASE && sh[3] + sh[5] <= ROM_END &&
                (size_t)sh[4] + sh[5] <= elfSize) {
                memcpy(src[0] + (sh[3] - ROM_BASE), elf + sh[4], sh[5]);
            }
        }
        free(elf);
    }
    snprintf(path, sizeof(path), "%s/disc/BIN/DBZP.BIN", root);
    src[1] = read_file(path, &srcSize[1]);
    if (src[1] == NULL) {
        data_fail("the game's menu program was not found", path);
    }
    for (i = 0; i < count; i++) {
        uint32_t r[4]; /* address in this program, source, offset in it, length */
        const uint8_t *from;
        memcpy(r, list + 16 + (size_t)i * 16, 16);
        if (r[1] > 1 || (size_t)r[2] + r[3] > srcSize[r[1]]) {
            data_fail("damaged file (list of the game's data)", NULL);
        }
        from = src[r[1]] + r[2];
        memcpy((void *)(uintptr_t)r[0], from, r[3]);
        for (k = 0; k < r[3]; k++) {
            hash = (hash ^ from[k]) * 0x100000001B3ull;
        }
    }
    free(src[0]);
    free(src[1]);
    free(list);
    if (hash != want) {
        data_fail("the game's programs in your game data are not the unmodified USA release (SLUS-21678)", NULL);
    }
}

#if PORT_LP64
/* (between the GS registers at 0x12000000 and the program at 0x20000000: see the note at STACK_BASE) */
#ifdef __ANDROID__
/* Android: an app's Java heap is reserved from 0x12C00000 up, so the engine (linked at 0x03000000,
   port/tools/android.py) and this region sit in the free space below it: 0x05000000..0x0F000000. */
#define LOW_ARENA_BASE 0x05000000u
#define LOW_ARENA_SIZE 0x0A000000u
#else
#define LOW_ARENA_BASE 0x13000000u
#define LOW_ARENA_SIZE 0x0C000000u /* address space only: pages are taken as they are first used */
#endif
enum { LOW_SIZES = 256 };
PORT_HOST static uint8_t *sLowArena = NULL; /* where the region is: fixed once it is taken, not game state */
static uint8_t *sLowNext;
#ifdef _WIN32
PORT_HOST static uint8_t *sLowCommitted = NULL; /* how far the region's pages are committed (never given back) */
#endif
static struct { uint64_t size; void *head; } sLowList[LOW_SIZES];
static int sLowSizes;
static volatile int sLowLock;
#endif

/* For saving and restoring the game's state (port/src/gs/state.c): the port's own memory that belongs to it. The
   blocks of Port_LowAlloc hold what the game was handed by address (file handles, sound buffers), and the
   allocator's own variables say which of them are in use. Also where the game thread's stack ends. */
uint32_t Port_LowRegionSize(void) {
#if PORT_LP64
    return LOW_ARENA_SIZE;
#else
    return 0;
#endif
}

void *Port_LowAlloc(size_t size);
void Port_LowFree(void *addr);

/* A large block for the game that does not come out of its own heap (28 MB, sized for the console): the buffer of
   a stage larger than the disc's (battle_load.c). From the port's region, which is below 4 GB (the game's
   pointers are 32 bits) and is part of every saved state. As an address, not a pointer: the game's files are
   compiled with 32-bit pointers and must not be handed a host function's pointer result. 0: not available
   (the 32-bit program, whose blocks are the C library's and cannot be told from the heap's by address). */
unsigned Port_GameBigAlloc(int size) {
#if PORT_LP64
    return (unsigned)(uintptr_t)Port_LowAlloc((size_t)size);
#else
    (void)size;
    return 0;
#endif
}

/* 1 if `addr` was such a block (it is given back); 0: it is the game heap's (Heap_Free goes on). */
int Port_GameBigFree(unsigned addr) {
#if PORT_LP64
    if (sLowArena != NULL && addr >= (unsigned)(uintptr_t)sLowArena && addr - (unsigned)(uintptr_t)sLowArena < LOW_ARENA_SIZE) {
        Port_LowFree((void *)(uintptr_t)addr);
        return 1;
    }
#else
    (void)addr;
#endif
    return 0;
}

int Port_StateExtra(void **p, size_t *n, int max) {
    int k = 0;
#if PORT_LP64
    if (max >= 4 && sLowArena != NULL) {
        p[k] = &sLowNext; n[k++] = sizeof(sLowNext);
        p[k] = sLowList; n[k++] = sizeof(sLowList);
        p[k] = &sLowSizes; n[k++] = sizeof(sLowSizes);
        p[k] = sLowArena; n[k++] = (size_t)(sLowNext - sLowArena);
    }
#else
    (void)p; (void)n; (void)max;
#endif
    return k;
}
uint8_t *Port_GameStackTop(void) {
#if PORT_LP64
    return sGameStackTop;
#else
    return NULL;
#endif
}

/* Where the game's heap is in this process (at its PS2 address unless that was taken) and how large. */
void Port_HeapRegion(uint8_t **base, uint32_t *size) {
    *base = (uint8_t *)(uintptr_t)(sHeapEnd - (HEAP_END - HEAP_BASE));
    *size = HEAP_END - HEAP_BASE;
}

#if PORT_LP64
static void low_region_take(void);
#endif
__attribute__((constructor)) static void Port_MapMemory(void) {
#ifdef _WIN32
    /* The program only works at the address it was linked for (the game's pointers are 4 bytes and absolute). It
       carries no relocation table, so Windows has to load it there; should it be somewhere else all the same (a
       system set to move every program, "Mandatory ASLR", moved builds that still had the table: a crash at the
       first write, before anything was printed), say so instead of crashing. */
    if ((uintptr_t)GetModuleHandleA(NULL) != 0x20000000u) {
        static const char msg[] = "Tenkaichi3Decomp was loaded at a different address than the one it needs (0x20000000) and cannot run.\n"
                                  "If \"Force randomization for images (Mandatory ASLR)\" is switched on in Windows Security > App & browser "
                                  "control > Exploit protection, switch it off for this program.\n";
        HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
        DWORD done;
        if (err != NULL && err != INVALID_HANDLE_VALUE) {
            WriteFile(err, msg, sizeof(msg) - 1, &done, NULL);
        } else {
            MessageBoxA(NULL, msg, "Tenkaichi3Decomp", MB_OK | MB_ICONERROR);
        }
        ExitProcess(4);
    }
#endif
    gPortStage = "reading the game's programs from gamedata";
    Port_LoadGameData();
    gPortStage = "reserving the game's memory";
#if PORT_LP64 && !defined(_WIN32)
#ifndef __ANDROID__
    mallopt(M_MMAP_MAX, 0);
    mallopt(M_ARENA_MAX, 1);
#endif
    map(STACK_BASE, STACK_SIZE, "the game thread's stack");
    low_region_take(); /* the region of Port_LowAlloc, now (see there) */
#endif
#if PORT_LP64 && defined(_WIN32)
    low_region_take();
#endif
    map_heap();
    map(0x10000000u, 0x10000, "the hardware registers");
    map(0x12000000u, 0x2000, "the GS registers");
#ifdef __ANDROID__
    /* The game addresses no memory there itself (its 0x70000000 words are DMA tags), and in an Android app's process
       the system's boot image is often mapped around that address: taken if free, not required. */
    if (mmap((void *)(uintptr_t)0x70000000u, 0x4000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) !=
        (void *)(uintptr_t)0x70000000u) {
        fprintf(stderr, "bt3: the scratchpad's address is taken (not needed)\n");
    }
#else
    map(0x70000000u, 0x4000, "the scratchpad");
#endif
    gPortStage = "memory ready, before the program's main";
}

/* The game's malloc (include/port_compat.h renames it): a bump allocator over the heap region, 16-byte aligned. */
void *Port_Malloc(uint32_t size) {
    uint32_t p = (sHeapNext + 15) & ~15u;

    if (size > sHeapEnd - p) {
        fprintf(stderr, "bt3: malloc(0x%X) does not fit in the game heap\n", size);
        exit(2);
    }
    sHeapNext = p + size;
    return (void *)(uintptr_t)p;
}

/* Memory the port hands to the game by address (file handles, the "second processor's" memory): zeroed, and below
   4 GB in the 64-bit build, where the game keeps such an address in 4 bytes. The C library's malloc is not safe
   for this there: it moves to mappings far above 4 GB whenever the break area cannot grow. */
#if PORT_LP64
/* Takes the region of Port_LowAlloc. At the program's start (Port_MapMemory), before anything else can sit at its
   address and before any state is saved: a state restored to before the region existed would otherwise take it a
   second time, somewhere else. */
static void low_region_take(void) {
#ifdef _WIN32
        /* address space only (reserved); pages are committed as the region is used, below */
        sLowArena = VirtualAlloc((void *)(uintptr_t)LOW_ARENA_BASE, LOW_ARENA_SIZE, MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE);
        if (sLowArena == NULL) {
            sLowArena = low_reserve(LOW_ARENA_SIZE);
        }
        if (sLowArena == NULL) {
            fprintf(stderr, "bt3: no room below 2 GB for the port's own blocks\n");
            exit(2);
        }
        sLowCommitted = sLowArena;
#else
        sLowArena = mmap((void *)(uintptr_t)LOW_ARENA_BASE, LOW_ARENA_SIZE, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (sLowArena == MAP_FAILED || sLowArena != (uint8_t *)(uintptr_t)LOW_ARENA_BASE) { /* taken: anywhere below 4 GB */
#ifdef MAP_32BIT
            sLowArena = mmap(NULL, LOW_ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_32BIT, -1, 0);
#else
            /* (no MAP_32BIT on arm64: try fixed addresses below 4 GB, 16 MB apart) */
            uintptr_t at;
            sLowArena = MAP_FAILED;
            for (at = 0x30000000u; at + LOW_ARENA_SIZE <= 0xFF000000u && sLowArena == MAP_FAILED; at += 0x01000000u) {
                sLowArena = mmap((void *)at, LOW_ARENA_SIZE, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
                if (sLowArena != MAP_FAILED && sLowArena != (uint8_t *)at) {
                    munmap(sLowArena, LOW_ARENA_SIZE);
                    sLowArena = MAP_FAILED;
                }
            }
#endif
        }
        if (sLowArena == MAP_FAILED) {
            fprintf(stderr, "bt3: no memory below 4 GB for the port's own blocks\n");
            exit(2);
        }
#endif
        sLowNext = sLowArena;
}

#endif

void *Port_LowAlloc(size_t size) {
#if PORT_LP64
    /* From one region at a fixed address, handed out in order, freed blocks reused by size (last freed first).
       The addresses end up in the game's memory (file handles, sound buffers); with the system choosing them they
       differed from run to run, and so did the game's state. This way they depend only on the order of the
       requests. */
    size_t total = (size + 16 + 255) & ~(size_t)255;
    uint64_t *p = NULL;
    int i;

    if (total > 4096) {
        total = (total + 4095) & ~(size_t)4095;
    }
    while (__sync_lock_test_and_set(&sLowLock, 1)) {
    }
    if (sLowArena == NULL) {
        low_region_take();
    }
    for (i = 0; i < sLowSizes; i++) {
        if (sLowList[i].size == total && sLowList[i].head != NULL) {
            p = sLowList[i].head;
            sLowList[i].head = *(void **)p;
            memset(p, 0, total);
            break;
        }
    }
    if (p == NULL) {
        if ((size_t)(sLowArena + LOW_ARENA_SIZE - sLowNext) < total) {
            fprintf(stderr, "bt3: no memory below 4 GB for %zu bytes\n", size);
            exit(2);
        }
        p = (uint64_t *)sLowNext;
        sLowNext += total;
#ifdef _WIN32
        if (sLowNext > sLowCommitted) { /* commit what is new, a megabyte at a time */
            size_t grow = ((size_t)(sLowNext - sLowCommitted) + 0xFFFFF) & ~(size_t)0xFFFFF;
            if ((size_t)(sLowArena + LOW_ARENA_SIZE - sLowCommitted) < grow) {
                grow = (size_t)(sLowArena + LOW_ARENA_SIZE - sLowCommitted);
            }
            if (VirtualAlloc(sLowCommitted, grow, MEM_COMMIT, PAGE_READWRITE) == NULL) {
                fprintf(stderr, "bt3: no memory for the port's own blocks\n");
                exit(2);
            }
            sLowCommitted += grow;
        }
#endif
        /* cleared: after the game's state was restored to an earlier moment, the memory above the restored end
           of the region still holds the blocks of the frames that were undone (a file handle with its read
           position, which made the re-run frame's read loop spin for ever) */
        memset(p, 0, total);
    }
    p[0] = total;
    __sync_lock_release(&sLowLock);
    return p + 2;
#else
    return calloc(1, size);
#endif
}

void Port_LowFree(void *addr) {
#if PORT_LP64
    if (addr != NULL) {
        uint64_t *p = (uint64_t *)addr - 2, total = p[0];
        int i;
        while (__sync_lock_test_and_set(&sLowLock, 1)) {
        }
        for (i = 0; i < sLowSizes && sLowList[i].size != total; i++) {
        }
        if (i == sLowSizes && sLowSizes < LOW_SIZES) {
            sLowList[sLowSizes].size = total;
            sLowList[sLowSizes].head = NULL;
            sLowSizes++;
        }
        if (i < sLowSizes) { /* (with the table of sizes full the block is simply not reused) */
            *(void **)p = sLowList[i].head;
            sLowList[i].head = p;
        }
        __sync_lock_release(&sLowLock);
    }
#else
    free(addr);
#endif
}
