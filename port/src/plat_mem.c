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
            void *p = VirtualAlloc((void *)base, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
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
    uint8_t *p = VirtualAlloc((void *)(uintptr_t)base, HEAP_END - base, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

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

extern int __real_main(int argc, char **argv);
extern void Port_CallOnStack(void (*fn)(void), void *top);
static int sArgc, sResult;
static char **sArgv;

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

static void map(uint32_t addr, uint32_t size, const char *what) {
    void *p = mmap((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                   -1, 0);

    if (p != (void *)(uintptr_t)addr) {
        fprintf(stderr, "bt3: cannot map %s at 0x%08X\n", what, addr);
        exit(2);
    }
}

static void map_heap(void) {
    map(HEAP_BASE, HEAP_END - HEAP_BASE, "the game heap");
}

#ifdef __x86_64__
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
static int sArgc, sResult;
static char **sArgv;

static void *game_thread(void *arg) {
    (void)arg;
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
    long n = (long)readlink("/proc/self/exe", exe, sizeof(exe) - 1);
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

#if defined(__x86_64__) && !defined(_WIN32)
/* (between the GS registers at 0x12000000 and the program at 0x20000000: see the note at STACK_BASE) */
#define LOW_ARENA_BASE 0x13000000u
#define LOW_ARENA_SIZE 0x0C000000u /* address space only: pages are taken as they are first used */
enum { LOW_SIZES = 256 };
static uint8_t *sLowArena, *sLowNext;
static struct { uint64_t size; void *head; } sLowList[LOW_SIZES];
static int sLowSizes;
static volatile int sLowLock;
#endif

/* For saving and restoring the game's state (port/src/gs/state.c): the port's own memory that belongs to it. The
   blocks of Port_LowAlloc hold what the game was handed by address (file handles, sound buffers), and the
   allocator's own variables say which of them are in use. Also where the game thread's stack ends. */
int Port_StateExtra(void **p, size_t *n, int max) {
    int k = 0;
#if defined(__x86_64__) && !defined(_WIN32)
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
#if defined(__x86_64__) && !defined(_WIN32)
    return (uint8_t *)(uintptr_t)(STACK_BASE + STACK_SIZE);
#else
    return NULL;
#endif
}

/* Where the game's heap is in this process (at its PS2 address unless that was taken) and how large. */
void Port_HeapRegion(uint8_t **base, uint32_t *size) {
    *base = (uint8_t *)(uintptr_t)(sHeapEnd - (HEAP_END - HEAP_BASE));
    *size = HEAP_END - HEAP_BASE;
}

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
#if defined(__x86_64__) && !defined(_WIN32)
    mallopt(M_MMAP_MAX, 0);
    mallopt(M_ARENA_MAX, 1);
    map(STACK_BASE, STACK_SIZE, "the game thread's stack");
    /* the region of Port_LowAlloc, taken now: later (once the graphics libraries are loaded) the address is
       sometimes in use, and the blocks' addresses, which end up in the game's memory, changed from run to run */
    sLowArena = mmap((void *)(uintptr_t)LOW_ARENA_BASE, LOW_ARENA_SIZE, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (sLowArena == MAP_FAILED || sLowArena != (uint8_t *)(uintptr_t)LOW_ARENA_BASE) {
        if (getenv("BT3_MEM_DEBUG") != NULL) {
            char line[256];
            FILE *maps = fopen("/proc/self/maps", "r");
            fprintf(stderr, "bt3: the region at 0x%08X was not free (got %p). Mappings below 0x50000000:\n", LOW_ARENA_BASE, (void *)sLowArena);
            while (maps != NULL && fgets(line, sizeof(line), maps) != NULL) {
                if (strtoull(line, NULL, 16) < 0x50000000ull) {
                    fputs(line, stderr);
                }
            }
        }
        sLowArena = NULL; /* Port_LowAlloc takes one where it can */
    } else {
        sLowNext = sLowArena;
    }
#endif
    map_heap();
    map(0x10000000u, 0x10000, "the hardware registers");
    map(0x12000000u, 0x2000, "the GS registers");
    map(0x70000000u, 0x4000, "the scratchpad");
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
#ifdef _WIN32
enum { LOW_POOL = 0x100000, LOW_SMALL = 0x1000 };
static void *sLowFreed[LOW_SMALL / 256];
#endif


void *Port_LowAlloc(size_t size) {
#ifdef _WIN32
    /* small blocks from a pool (file handles come and go by the thousand; a reservation of its own would cost
       64 KB of address space each), large ones reserved singly. Freed small blocks are reused by size. */
    static uint8_t *pool, *poolEnd;
    void **freed = sLowFreed;
    size_t total = (size + 16 + 15) & ~(size_t)15;
    uint64_t *p;

    if (total > LOW_SMALL) {
        p = low_alloc(total);
    } else {
        size_t cls = (total - 1) / 256; /* 256-byte steps up to LOW_SMALL */
        total = (cls + 1) * 256;
        if (freed[cls] != NULL) {
            p = freed[cls];
            freed[cls] = *(void **)p;
            memset(p, 0, total);
        } else {
            if (pool == NULL || (size_t)(poolEnd - pool) < total) {
                pool = low_alloc(LOW_POOL);
                poolEnd = pool != NULL ? pool + LOW_POOL : NULL;
            }
            p = (uint64_t *)pool;
            pool = pool != NULL ? pool + total : NULL;
        }
    }
    if (p == NULL) {
        fprintf(stderr, "bt3: no memory below 2 GB for %u bytes\n", (unsigned)size);
        exit(2);
    }
    p[0] = total;
    return p + 2;
#elif defined(__x86_64__)
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
        sLowArena = mmap((void *)(uintptr_t)LOW_ARENA_BASE, LOW_ARENA_SIZE, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (sLowArena == MAP_FAILED || sLowArena != (uint8_t *)(uintptr_t)LOW_ARENA_BASE) { /* taken: anywhere below 4 GB */
            sLowArena = mmap(NULL, LOW_ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_32BIT, -1, 0);
        }
        if (sLowArena == MAP_FAILED) {
            fprintf(stderr, "bt3: no memory below 4 GB for the port's own blocks\n");
            exit(2);
        }
        sLowNext = sLowArena;
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
    }
    p[0] = total;
    __sync_lock_release(&sLowLock);
    return p + 2;
#else
    return calloc(1, size);
#endif
}

void Port_LowFree(void *addr) {
#ifdef _WIN32
    if (addr != NULL) {
        uint64_t *p = (uint64_t *)addr - 2;
        if (p[0] > LOW_SMALL) {
            VirtualFree(p, 0, MEM_RELEASE);
        } else { /* back to the list of its size */
            size_t cls = (size_t)(p[0] - 1) / 256;
            *(void **)p = sLowFreed[cls];
            sLowFreed[cls] = p;
        }
    }
#elif defined(__x86_64__)
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
