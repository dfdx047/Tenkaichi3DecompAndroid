/*
 * libmain.so: what SDL's Android activity calls (SDL_main). It starts the engine, libbt3.so.
 *
 * The engine is the PC port's 64-bit program built for arm64 (port/tools/android.py): the game keeps addresses in
 * 4 bytes, so its code and data must lie where they were linked, at 0x03000000, below 4 GB (and below the Java
 * heap, which an app's process has from 0x12C00000 up). The program is linked
 * at that address and marked as a shared object; this reserves the address range and asks Android's own loader
 * to load the program exactly there (android_dlopen_ext with ANDROID_DLEXT_RESERVED_ADDRESS), so the loader's
 * load bias is zero and every address in the program is the one it was linked with. The loader also resolves the
 * program's imports (SDL3, the C library) and runs its constructors (plat_mem.c maps the game's memory there).
 *
 * Arguments (GameActivity.getArguments): the folder of the game's files (gamedata/, textures/, saves/, ...), and
 * the folder of the app's native libraries.
 *
 * Everything the engine prints (stdout, stderr) goes to the log (adb logcat -s bt3) and to bt3_log.txt in the
 * game's folder, so a player can send it.
 */
#define _GNU_SOURCE 1 /* process_vm_readv */
#include <android/api-level.h>
#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/system_properties.h>
#include <dirent.h>
#include <sys/uio.h>
#include <signal.h>
#include <ucontext.h>
#include <adrenotools/driver.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define TAG "bt3"

static FILE *sLog;
static int sPipe[2];

static void *log_thread(void *arg) {
    char buf[1024];
    size_t used = 0;
    (void)arg;
    for (;;) {
        ssize_t n = read(sPipe[0], buf + used, sizeof(buf) - 1 - used);
        if (n <= 0) {
            break;
        }
        used += (size_t)n;
        for (;;) { /* whole lines to the log; a line longer than the buffer goes out in pieces */
            char *nl = memchr(buf, '\n', used);
            size_t len = nl != NULL ? (size_t)(nl - buf) : used == sizeof(buf) - 1 ? used : 0;
            if (len == 0 && nl == NULL) {
                break;
            }
            buf[len] = '\0';
            __android_log_write(ANDROID_LOG_INFO, TAG, buf);
            if (sLog != NULL) {
                fprintf(sLog, "%s\n", buf);
                fflush(sLog);
            }
            len += nl != NULL;
            memmove(buf, buf + len, used - len);
            used -= len;
        }
    }
    return NULL;
}

static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *fmt, ...) {
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    __android_log_write(ANDROID_LOG_INFO, TAG, line);
    if (sLog != NULL) {
        fprintf(sLog, "%s\n", line);
        fflush(sLog);
    }
}

static void redirect_output(const char *dir) {
    char path[1024];
    pthread_t th;
    char older[1024], old[1024];
    /* the two runs before this one are kept (bt3_log.1.txt, bt3_log.2.txt): a problem is often seen one run late */
    snprintf(path, sizeof(path), "%s/bt3_log.txt", dir);
    snprintf(old, sizeof(old), "%s/bt3_log.1.txt", dir);
    snprintf(older, sizeof(older), "%s/bt3_log.2.txt", dir);
    rename(old, older);
    rename(path, old);
    sLog = fopen(path, "w");
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (pipe(sPipe) == 0) {
        dup2(sPipe[1], 1);
        dup2(sPipe[1], 2);
        pthread_create(&th, NULL, log_thread, NULL);
        pthread_detach(th);
    }
}

/* What is mapped below 4 GB in this process: the first thing to look at when the game's memory does not fit. */
static void log_low_maps(void) {
    char line[512];
    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        return;
    }
    say("mappings below 4 GB:");
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long lo = strtoul(line, NULL, 16);
        if (lo < 0x100000000ul) {
            line[strcspn(line, "\n")] = '\0';
            say("  %s", line);
        }
    }
    fclose(fp);
}

/* The address range the program's segments take, from its program headers. */
static int program_span(const char *path, uintptr_t *lo, uintptr_t *hi) {
    Elf64_Ehdr eh;
    Elf64_Phdr ph;
    int fd = open(path, O_RDONLY | O_CLOEXEC), i;
    *lo = UINTPTR_MAX;
    *hi = 0;
    if (fd < 0) {
        return 0;
    }
    if (pread(fd, &eh, sizeof(eh), 0) != sizeof(eh) || memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0) {
        close(fd);
        return 0;
    }
    for (i = 0; i < eh.e_phnum; i++) {
        if (pread(fd, &ph, sizeof(ph), (off_t)(eh.e_phoff + (uint64_t)i * eh.e_phentsize)) != sizeof(ph)) {
            break;
        }
        if (ph.p_type == PT_LOAD) {
            if (ph.p_vaddr < *lo) {
                *lo = ph.p_vaddr;
            }
            if (ph.p_vaddr + ph.p_memsz > *hi) {
                *hi = ph.p_vaddr + ph.p_memsz;
            }
        }
    }
    close(fd);
    return *hi > *lo;
}

/* ---- The watchdog: every few seconds, where each thread of the process is, by name, in the log. Each thread's
   stack pointer and program counter come from /proc/self/task/<id>/syscall (it is in a system call when it waits);
   the stack above that is scanned for addresses inside the engine's code, named with dladdr (the engine exports
   its symbols). A rough backtrace, but enough to see where the game waits. On with BT3_WATCHDOG=1 (bt3_env.txt): it
   costs a pass over every thread every 10 s on all devices, so it is not on by default. */
static uintptr_t sCodeLo, sCodeHi;
static unsigned *sVBlanks;

static void name_of(uintptr_t pc, char *out, size_t size) {
    Dl_info info;
    if (dladdr((void *)pc, &info) != 0 && info.dli_sname != NULL) {
        snprintf(out, size, "%s+0x%lx", info.dli_sname, (unsigned long)(pc - (uintptr_t)info.dli_saddr));
    } else if (dladdr((void *)pc, &info) != 0 && info.dli_fname != NULL) {
        const char *b = strrchr(info.dli_fname, '/');
        snprintf(out, size, "%s+0x%lx", b != NULL ? b + 1 : info.dli_fname, (unsigned long)(pc - (uintptr_t)info.dli_fbase));
    } else {
        snprintf(out, size, "0x%lx", (unsigned long)pc);
    }
}

static void *sEngine;

/* ---- Early reservation of the game's fixed addresses.
   The engine needs fixed places below 4 GB (its own code at 0x03000000, the PS2 heap from 0x003BE000, the game
   thread's stack, the port's region, the hardware registers). Anything else that loads into the process first (the GPU
   driver, the audio stack, a vendor library) might take one of them. So they are claimed, as address space only, the
   moment this library is loaded (before SDL starts the GPU and audio), and handed to the engine: BT3_LOW_RESERVED lists
   them, and plat_mem.c maps over its own reservation. A range already taken is left out and logged. */
static struct { uintptr_t lo, hi; int ok; } sEarly[6];
static char sEarlyList[256];

static int early_reserve(int i, uintptr_t lo, uintptr_t hi) {
    void *p = mmap((void *)lo, hi - lo, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    sEarly[i].lo = lo;
    sEarly[i].hi = hi;
    sEarly[i].ok = p == (void *)lo;
    if (p != MAP_FAILED && p != (void *)lo) {
        munmap(p, hi - lo);
    }
    if (sEarly[i].ok) {
        size_t n = strlen(sEarlyList);
        snprintf(sEarlyList + n, sizeof(sEarlyList) - n, "%s%lx-%lx", n ? ";" : "", (unsigned long)lo, (unsigned long)hi);
    } else {
        __android_log_print(ANDROID_LOG_WARN, TAG, "early: 0x%lx-0x%lx is already taken (%s)", (unsigned long)lo, (unsigned long)hi,
                            strerror(errno));
    }
    return sEarly[i].ok;
}

static uintptr_t sEngineLo, sEngineHi; /* the engine's range, reserved early (0: not) */
static intptr_t sBias;                   /* where the engine was loaded less where it was linked */

__attribute__((constructor)) static void reserve_early(void) {
    uintptr_t page = (uintptr_t)sysconf(_SC_PAGESIZE);
    Dl_info info;
    early_reserve(0, 0x003BE000u & ~(page - 1), 0x03000000u); /* PS2 heap, then the game thread's stack */
    early_reserve(1, 0x05000000u, 0x0F000000u);                 /* the port's region */
    if (dladdr((void *)reserve_early, &info) != 0 && info.dli_fname != NULL) {
        char engine[1024];
        const char *slash = strrchr(info.dli_fname, '/');
        uintptr_t lo, hi;
        snprintf(engine, sizeof(engine), "%.*s/libbt3.so", slash != NULL ? (int)(slash - info.dli_fname) : 1, slash != NULL ? info.dli_fname : ".");
        if (program_span(engine, &lo, &hi)) {
            lo &= ~(page - 1);
            hi = (hi + page - 1) & ~(page - 1);
            if (early_reserve(4, lo, hi)) {
                sEngineLo = lo;
                sEngineHi = hi;
            }
        }
    }
    setenv("BT3_LOW_RESERVED", sEarlyList, 1);
}

/* A crash: where it happened, in the log (logcat -s bt3 and bt3_log.txt), with the engine's function names, then
   Android's own handler (the tombstone) as usual. */
static struct sigaction sOldAct[32];

static void on_crash(int sig, siginfo_t *si, void *ctx) {
    ucontext_t *uc = ctx;
    char nm[160];
    uintptr_t fp, frame[2];
    int i;

    name_of((uintptr_t)uc->uc_mcontext.pc, nm, sizeof(nm));
    say("CRASH: signal %d (code %d) at %s, fault address 0x%lx", sig, si->si_code, nm, (unsigned long)(uintptr_t)si->si_addr);
    name_of((uintptr_t)uc->uc_mcontext.regs[30], nm, sizeof(nm));
    say("  lr %s", nm);
    for (i = 0; i < 31; i += 4) {
        say("  x%-2d %016llx %016llx %016llx %016llx", i, (unsigned long long)uc->uc_mcontext.regs[i],
            (unsigned long long)(i + 1 < 31 ? uc->uc_mcontext.regs[i + 1] : 0), (unsigned long long)(i + 2 < 31 ? uc->uc_mcontext.regs[i + 2] : 0),
            (unsigned long long)(i + 3 < 31 ? uc->uc_mcontext.regs[i + 3] : 0));
    }
    fp = (uintptr_t)uc->uc_mcontext.regs[29];
    for (i = 0; i < 24 && fp != 0; i++) { /* the frame-pointer chain: [fp] = caller's fp, [fp + 8] = return address */
        struct iovec lo = {frame, sizeof(frame)}, re = {(void *)fp, sizeof(frame)};
        if (process_vm_readv(getpid(), &lo, 1, &re, 1, 0) != (ssize_t)sizeof(frame) || frame[1] == 0) {
            break;
        }
        name_of(frame[1], nm, sizeof(nm));
        say("  #%02d %s", i, nm);
        if (frame[0] <= fp) {
            break;
        }
        fp = frame[0];
    }
    if (sLog != NULL) {
        fflush(sLog);
    }
    sigaction(sig, &sOldAct[sig], NULL); /* returning runs the instruction again, now into Android's handler */
}

static void crash_handlers(void) {
    static const int sigs[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};
    struct sigaction sa;
    size_t i;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_crash;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) {
        sigaction(sigs[i], &sa, &sOldAct[sigs[i]]);
    }
}

/* BT3_PEEK: game variables in the watchdog's report. Comma-separated items "name+off" (the 32-bit word at the
   symbol plus off) or "name*off" (the symbol holds a 4-byte game pointer: the word at that pointer plus off). */
static void peek(void) {
    const char *spec = getenv("BT3_PEEK");
    char buf[1024], line[2048], nm[160], *item, *save = NULL;
    size_t len = 0;
    if (spec == NULL || spec[0] == '\0') {
        return; /* (nothing asked for) */
    }
    snprintf(buf, sizeof(buf), "%s", spec);
    line[0] = '\0';
    for (item = strtok_r(buf, ",", &save); item != NULL && len < sizeof(line) - 200; item = strtok_r(NULL, ",", &save)) {
        char name[96];
        char *op = strpbrk(item, "+*");
        unsigned long off = op != NULL ? strtoul(op + 1, NULL, 0) : 0;
        uint32_t *sym, v = 0;
        uintptr_t at;
        snprintf(name, sizeof(name), "%.*s", op != NULL ? (int)(op - item) : (int)strlen(item), item);
        sym = (uint32_t *)dlsym(sEngine, name);
        if (sym == NULL) {
            len += (size_t)snprintf(line + len, sizeof(line) - len, " %s=?", item);
            continue;
        }
        at = (uintptr_t)sym + off;
        if (op != NULL && *op == '*') {
            uint32_t p32 = 0;
            struct iovec lo = {&p32, 4}, re = {sym, 4};
            if (process_vm_readv(getpid(), &lo, 1, &re, 1, 0) != 4) {
                continue;
            }
            at = (uintptr_t)p32 + off;
        }
        if (at < 0x1000 || at >= 0x100000000ul) {
            len += (size_t)snprintf(line + len, sizeof(line) - len, " %s=(null)", item);
            continue;
        }
        {   /* read through the kernel: a pointer that is stale or not yet set must not bring the game down */
            struct iovec lo = {&v, 4}, re = {(void *)at, 4};
            if (process_vm_readv(getpid(), &lo, 1, &re, 1, 0) != 4) {
                len += (size_t)snprintf(line + len, sizeof(line) - len, " %s=(unmapped 0x%lx)", item, (unsigned long)at);
                continue;
            }
        }
        if (v >= sCodeLo && v < sCodeHi) {
            name_of(v, nm, sizeof(nm));
            len += (size_t)snprintf(line + len, sizeof(line) - len, " %s=%s", item, nm);
        } else {
            len += (size_t)snprintf(line + len, sizeof(line) - len, " %s=0x%x(%d)", item, v, (int)v);
        }
    }
    say("peek:%s", line);
}

static void dump_threads(void) {
    DIR *d = opendir("/proc/self/task");
    struct dirent *e;
    if (d == NULL) {
        return;
    }
    say("watchdog: game blank %u", sVBlanks != NULL ? *sVBlanks : 0);
    peek();
    while ((e = readdir(d)) != NULL) {
        char path[128], comm[64] = "?", sys[512] = "", line[2048], nm[160];
        unsigned long v[9];
        FILE *fp;
        int n, k, found = 0;
        size_t len;
        if (e->d_name[0] == '.') {
            continue;
        }
        snprintf(path, sizeof(path), "/proc/self/task/%s/comm", e->d_name);
        if ((fp = fopen(path, "r")) != NULL) {
            if (fgets(comm, sizeof(comm), fp) != NULL) {
                comm[strcspn(comm, "\n")] = '\0';
            }
            fclose(fp);
        }
        snprintf(path, sizeof(path), "/proc/self/task/%s/syscall", e->d_name);
        if ((fp = fopen(path, "r")) != NULL) {
            if (fgets(sys, sizeof(sys), fp) == NULL) {
                sys[0] = '\0';
            }
            fclose(fp);
        }
        n = sscanf(sys, "%lu %lx %lx %lx %lx %lx %lx %lx %lx", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8]);
        if (n < 9) { /* running (not in a system call) or unreadable */
            say("  [%s %s] %s", e->d_name, comm, sys[0] != '\0' ? "running" : "?");
            continue;
        }
        name_of(v[8], nm, sizeof(nm));
        len = (size_t)snprintf(line, sizeof(line), "  [%s %s] syscall %lu at %s; engine frames:", e->d_name, comm, v[0], nm);
        {   /* the stack from sp up, read without risking a fault */
            static uint64_t stack[8192];
            struct iovec local = {stack, sizeof(stack)}, remote = {(void *)v[7], sizeof(stack)};
            ssize_t got = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);
            if (got < 0) { /* the top of a stack: try less */
                remote.iov_len = local.iov_len = 4096;
                got = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);
            }
            for (k = 0; got > 0 && k < (int)(got / 8) && found < 14 && len < sizeof(line) - 200; k++) {
                uintptr_t a = (uintptr_t)stack[k];
                if (a >= sCodeLo && a < sCodeHi) {
                    name_of(a, nm, sizeof(nm));
                    len += (size_t)snprintf(line + len, sizeof(line) - len, " %s", nm);
                    found++;
                }
            }
        }
        say("%s", line);
    }
    closedir(d);
}

static void *watchdog(void *arg) {
    (void)arg;
    sleep(8);
    for (;;) {
        dump_threads();
        sleep(10);
    }
    return NULL;
}

/* A custom Vulkan driver (Turnip, a newer Qualcomm driver), loaded through libadrenotools. SDL is pointed at this
   library (SDL_VULKAN_LIBRARY) and finds its vkGetInstanceProcAddr here, which hands every call to the driver's. */
typedef void (*VkVoidFn)(void);
static VkVoidFn (*sDriverGipa)(void *instance, const char *name);

/* The render area of the engine's next render pass (Lite mode on a Mali, BT3_VK_AREA): gs_gpu.c names the rectangle its
   draws stay in, and vkCmdBeginRenderPass gets it instead of the whole attachment. A tile-based GPU (Mali) then loads
   and stores only those tiles: a pass that draws a life bar, or a 64 x 64 step of the glow, no longer moves the
   whole 512 x 512 picture through memory twice. The pixels outside are kept as they are (Vulkan: load and store act
   on the render area alone). SDL is pointed at this library for that, as for a custom driver. */
typedef struct { int32_t x, y; uint32_t w, h; } DrRect;
typedef struct { uint32_t sType; const void *next; void *renderPass, *framebuffer; DrRect area; uint32_t clears; const void *values; } DrPassBegin;
typedef void (*DrBeginFn)(void *cmd, const DrPassBegin *begin, uint32_t contents);
static VkVoidFn (*sDeviceGdpa)(void *device, const char *name);
static DrBeginFn sBegin;
static __thread int sAreaSet;
static __thread DrRect sArea;
static unsigned long sAreaUsed;

__attribute__((visibility("default"))) void Dr_VkNextRenderArea(int x, int y, int w, int h) {
    sArea.x = x, sArea.y = y, sArea.w = (uint32_t)w, sArea.h = (uint32_t)h;
    sAreaSet = w > 0 && h > 0;
}

static void begin_pass(void *cmd, const DrPassBegin *begin, uint32_t contents) {
    if (sAreaSet) {
        DrPassBegin b = *begin;
        int32_t x0 = sArea.x > b.area.x ? sArea.x : b.area.x, y0 = sArea.y > b.area.y ? sArea.y : b.area.y;
        int32_t x1 = (int32_t)(sArea.x + sArea.w), y1 = (int32_t)(sArea.y + sArea.h);
        if (x1 > (int32_t)(b.area.x + b.area.w)) { x1 = (int32_t)(b.area.x + b.area.w); }
        if (y1 > (int32_t)(b.area.y + b.area.h)) { y1 = (int32_t)(b.area.y + b.area.h); }
        sAreaSet = 0;
        if (x1 > x0 && y1 > y0) {
            b.area.x = x0, b.area.y = y0, b.area.w = (uint32_t)(x1 - x0), b.area.h = (uint32_t)(y1 - y0);
            if (sAreaUsed++ == 0) {
                say("render areas: on (first %d,%d %ux%u)", x0, y0, b.area.w, b.area.h);
            }
            sBegin(cmd, &b, contents);
            return;
        }
    }
    sBegin(cmd, begin, contents);
}

static VkVoidFn device_proc(void *device, const char *name) {
    VkVoidFn f = sDeviceGdpa(device, name);
    if (f != NULL && strcmp(name, "vkCmdBeginRenderPass") == 0) {
        sBegin = (DrBeginFn)f;
        return (VkVoidFn)begin_pass;
    }
    return f;
}

static int sAreaOn;

__attribute__((visibility("default"))) VkVoidFn vkGetInstanceProcAddr(void *instance, const char *name) {
    VkVoidFn f = sDriverGipa != NULL ? sDriverGipa(instance, name) : NULL;
    if (sAreaOn && f != NULL) {
        if (strcmp(name, "vkGetDeviceProcAddr") == 0) {
            sDeviceGdpa = (VkVoidFn(*)(void *, const char *))f;
            return (VkVoidFn)device_proc;
        }
        if (strcmp(name, "vkCmdBeginRenderPass") == 0) {
            sBegin = (DrBeginFn)f;
            return (VkVoidFn)begin_pass;
        }
    }
    return f;
}

/* The system's driver behind vkGetInstanceProcAddr above, for the render areas (when no custom driver is loaded). */
static void area_driver(const char *libdir) {
    char self[1100];
    void *h;
    sAreaOn = getenv("BT3_VK_AREA") != NULL && atoi(getenv("BT3_VK_AREA")) != 0;
    if (!sAreaOn || sDriverGipa != NULL) {
        return;
    }
    h = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    sDriverGipa = h != NULL ? (VkVoidFn(*)(void *, const char *))dlsym(h, "vkGetInstanceProcAddr") : NULL;
    if (sDriverGipa == NULL) {
        sAreaOn = 0;
        say("render areas: no libvulkan.so");
        return;
    }
    snprintf(self, sizeof(self), "%s/libmain.so", libdir);
    setenv("SDL_VULKAN_LIBRARY", self, 1);
}

static void custom_driver(const char *libdir, const char *driverDir, const char *driverLib) {
    char hooks[1024], drv[1024], self[1100];
    void *h;
    snprintf(hooks, sizeof(hooks), "%s/", libdir);
    snprintf(drv, sizeof(drv), "%s/", driverDir);
    h = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, NULL, hooks, drv, driverLib, NULL, NULL);
    if (h == NULL) {
        say("custom GPU driver %s%s could not be loaded: the system's driver is used", drv, driverLib);
        return;
    }
    sDriverGipa = (VkVoidFn(*)(void *, const char *))dlsym(h, "vkGetInstanceProcAddr");
    if (sDriverGipa == NULL) {
        say("custom GPU driver %s has no vkGetInstanceProcAddr: the system's driver is used", driverLib);
        return;
    }
    snprintf(self, sizeof(self), "%s/libmain.so", libdir);
    setenv("SDL_VULKAN_LIBRARY", self, 1);
    say("custom GPU driver: %s%s", drv, driverLib);
}


/* ---- Moving the engine.
   The engine is linked at 0x03000000; where that range is taken (an app's Java heap sits at different places below
   4 GB on different devices) it is loaded in another free place below 4 GB and its own addresses are moved by the
   difference: libbt3.so.rel (port/tools/android.py, relocations()) lists every 4-byte word that holds one. Its
   initialisers are hidden from Android's loader and called here, after the move. */
typedef struct { uint32_t base, count, initAddr, initSize; uint32_t *words; } EngineRel;
static EngineRel sRel;

static int read_rel(const char *path, EngineRel *r) {
    FILE *fp = fopen(path, "rb");
    char magic[4];
    uint32_t hdr[5];
    memset(r, 0, sizeof(*r));
    if (fp == NULL) {
        return 0;
    }
    if (fread(magic, 1, 4, fp) != 4 || memcmp(magic, "BT3R", 4) != 0 || fread(hdr, 4, 5, fp) != 5 || hdr[0] != 1) {
        fclose(fp);
        return 0;
    }
    r->base = hdr[1];
    r->count = hdr[2];
    r->initAddr = hdr[3];
    r->initSize = hdr[4];
    r->words = malloc((size_t)r->count * 4);
    if (r->words == NULL || fread(r->words, 4, r->count, fp) != r->count) {
        free(r->words);
        r->words = NULL;
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

/* A free place below 4 GB for `size` bytes, 64 KB aligned, reserved (address space only). 0 if none. */
static uintptr_t find_low(size_t size) {
    uintptr_t at;
    for (at = 0x02000000u; at + size <= 0xF0000000u; at += 0x00100000u) /* (above the PS2 heap) */ {
        void *p = mmap((void *)at, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == (void *)at) {
            return at;
        }
        if (p != MAP_FAILED) {
            munmap(p, size);
        }
    }
    return 0;
}

/* Adds `bias` to the listed words; the pages are made writable for it and given back their segment's protection. */
static void move_engine(const char *engine, const EngineRel *r, intptr_t bias) {
    uintptr_t page = (uintptr_t)sysconf(_SC_PAGESIZE);
    Elf64_Ehdr eh;
    Elf64_Phdr ph[32];
    int fd = open(engine, O_RDONLY | O_CLOEXEC), n = 0, i;
    uint32_t k;
    if (fd >= 0 && pread(fd, &eh, sizeof(eh), 0) == sizeof(eh) && eh.e_phnum <= 32 &&
        pread(fd, ph, (size_t)eh.e_phnum * sizeof(Elf64_Phdr), (off_t)eh.e_phoff) == (ssize_t)(eh.e_phnum * sizeof(Elf64_Phdr))) {
        n = eh.e_phnum;
    }
    if (fd >= 0) {
        close(fd);
    }
    for (i = 0; i < n; i++) { /* every loaded segment writable for the moment */
        if (ph[i].p_type == PT_LOAD) {
            uintptr_t lo = (ph[i].p_vaddr + bias) & ~(page - 1), hi = (ph[i].p_vaddr + ph[i].p_memsz + bias + page - 1) & ~(page - 1);
            mprotect((void *)lo, hi - lo, PROT_READ | PROT_WRITE | ((ph[i].p_flags & PF_X) ? PROT_EXEC : 0));
        }
    }
    for (k = 0; k < r->count; k++) {
        uint32_t *w = (uint32_t *)(uintptr_t)(r->words[k] + bias);
        *w = (uint32_t)(*w + (uint32_t)bias);
    }
    for (i = 0; i < n; i++) { /* the segments' own protection, and the read-only part after relocation (RELRO) */
        if (ph[i].p_type == PT_LOAD) {
            uintptr_t lo = (ph[i].p_vaddr + bias) & ~(page - 1), hi = (ph[i].p_vaddr + ph[i].p_memsz + bias + page - 1) & ~(page - 1);
            mprotect((void *)lo, hi - lo, ((ph[i].p_flags & PF_R) ? PROT_READ : 0) | ((ph[i].p_flags & PF_W) ? PROT_WRITE : 0) |
                                              ((ph[i].p_flags & PF_X) ? PROT_EXEC : 0));
        }
    }
    for (i = 0; i < n; i++) {
        if (ph[i].p_type == PT_GNU_RELRO) {
            uintptr_t lo = (ph[i].p_vaddr + bias) & ~(page - 1), hi = (ph[i].p_vaddr + ph[i].p_memsz + bias) & ~(page - 1);
            if (hi > lo) {
                mprotect((void *)lo, hi - lo, PROT_READ);
            }
        }
    }
}

static void run_initialisers(const EngineRel *r, intptr_t bias) {
    extern char **environ;
    uint32_t i;
    for (i = 0; i < r->initSize / 8; i++) {
        void (*fn)(int, char **, char **) = *(void (**)(int, char **, char **))(uintptr_t)(r->initAddr + bias + i * 8);
        if (fn != NULL && (uintptr_t)fn != (uintptr_t)-1) {
            fn(0, NULL, environ);
        }
    }
}

int SDL_main(int argc, char *argv[]) {
    const char *dir = argc > 1 ? argv[1] : ".";
    const char *libdir = argc > 2 ? argv[2] : ".";
    char engine[1024], exe[1024], prop[PROP_VALUE_MAX];
    uintptr_t lo, hi, page = (uintptr_t)sysconf(_SC_PAGESIZE);
    void *region, *handle;
    int (*entry)(int, char **);
    static char *args[] = {"bt3", NULL};

    if (chdir(dir) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "cannot use the game folder %s: %s", dir, strerror(errno));
        return 1;
    }
    redirect_output(dir);
    __system_property_get("ro.product.model", prop);
    say("Dragon Rage engine loader: device %s, Android %d, page size %lu", prop, android_get_device_api_level(), (unsigned long)page);
    say("game folder %s", dir);

    snprintf(engine, sizeof(engine), "%s/libbt3.so", libdir);
    snprintf(exe, sizeof(exe), "%s/libbt3.so", dir); /* plat_mem.c reads <BT3_EXE>.dat: the app put it there */
    setenv("BT3_EXE", exe, 1);
    setenv("BT3_GS", "gpu", 0);
    setenv("BT3_TAG_TEAM", "1", 0); /* the Tag Team mod: SELECT in the main menu opens its modes (port/src/tagteam_trace.c) */
    {   /* bt3_env.txt in the game folder: NAME=value lines, the engine's switches for testing (BT3_GS_VERBOSE=1, ...) */
        char line[512];
        FILE *fp = fopen("bt3_env.txt", "r");
        while (fp != NULL && fgets(line, sizeof(line), fp) != NULL) {
            char *eq = strchr(line, '=');
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] != '#' && eq != NULL) {
                *eq = '\0';
                setenv(line, eq + 1, 1);
                say("env %s=%s", line, eq + 1);
            }
        }
        if (fp != NULL) {
            fclose(fp);
        }
    }
    {   /* an image that is not the unmodified USA release (the app's installer says so): the engine takes its data */
        char line[128];
        FILE *fp = fopen("gamedata/.installed", "r");
        while (fp != NULL && fgets(line, sizeof(line), fp) != NULL) {
            if (strncmp(line, "modified=1", 10) == 0) {
                setenv("BT3_MODDED_DISC", "1", 0);
                say("modified game image");
            }
        }
        if (fp != NULL) {
            fclose(fp);
        }
    }
    if (argc > 4 && argv[3][0] != '\0' && argv[4][0] != '\0') {
        custom_driver(libdir, argv[3], argv[4]); /* (chosen in the app: GameActivity passes its folder and file) */
    }
    if (argc > 5 && strcmp(argv[5], "1") == 0) {
        setenv("BT3_VK_AREA", "1", 0); /* Lite mode on a Mali (GameActivity): the render areas below and in gs_gpu.c */
        say("lite mode on a Mali");
    }
    area_driver(libdir);
    setenv("SDL_VIDEO_DRIVER", "android", 0);
    setenv("SDL_ANDROID_TRAP_BACK_BUTTON", "1", 0); /* the back button opens the settings (gs_gpu.c), it does not end the game */

    if (!program_span(engine, &lo, &hi)) {
        say("cannot read the engine %s", engine);
        return 1;
    }
    lo &= ~(page - 1);
    hi = (hi + page - 1) & ~(page - 1);
    say("early reservations: %s", sEarlyList[0] ? sEarlyList : "(none)");
    {
        char relpath[1100];
        int haveRel;
        snprintf(relpath, sizeof(relpath), "%s/libbt3.so.rel", dir);
        haveRel = read_rel(relpath, &sRel);
        if (sEngineLo == lo && sEngineHi == hi) {
            region = (void *)lo; /* (claimed when this library was loaded) */
        } else {
            region = mmap((void *)lo, hi - lo, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        }
        if (region == (void *)lo && getenv("BT3_FORCE_MOVE") != NULL && atoi(getenv("BT3_FORCE_MOVE")) != 0) {
            /* (testing the move on a device where the usual range is free: bt3_env.txt BT3_FORCE_MOVE=1) */
            void *keep = region; /* held while another place is found, so it is not found again */
            region = (void *)find_low(hi - lo);
            munmap(keep, hi - lo);
            say("BT3_FORCE_MOVE: the engine goes to %p", region);
            if (region == NULL || !haveRel) {
                say("cannot move the engine");
                return 1;
            }
        } else if (region != (void *)lo) {
            if (region != MAP_FAILED) {
                munmap(region, hi - lo);
            }
            say("the engine's usual range 0x%lx-0x%lx is taken in this process", (unsigned long)lo, (unsigned long)hi);
            log_low_maps();
            if (!haveRel) {
                say("no relocation list (%s): the engine cannot be moved", relpath);
                return 1;
            }
            region = (void *)find_low(hi - lo);
            if (region == NULL) {
                say("no free place below 4 GB for the engine (%lu KB)", (unsigned long)((hi - lo) >> 10));
                return 1;
            }
        }
        sBias = (intptr_t)((uintptr_t)region - lo);
    }
    if (sBias == 0) {
        log_low_maps(); /* (what the game's own fixed regions will have to fit around: plat_mem.c) */
    }
    {
        android_dlextinfo info;
        char biasText[32];
        memset(&info, 0, sizeof(info));
        info.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
        info.reserved_addr = region;
        info.reserved_size = hi - lo;
        say("loading the engine at 0x%lx (%lu KB)%s", (unsigned long)(uintptr_t)region, (unsigned long)((hi - lo) >> 10),
            sBias != 0 ? ", moved" : "");
        handle = android_dlopen_ext(engine, RTLD_NOW | RTLD_GLOBAL, &info);
        snprintf(biasText, sizeof(biasText), "%ld", (long)sBias);
        setenv("BT3_ENGINE_BIAS", biasText, 1); /* plat_mem.c: where the game's data tables are now */
    }
    if (handle == NULL) {
        say("the engine did not load: %s", dlerror());
        log_low_maps();
        return 1;
    }
    if (sRel.words != NULL) {
        if (sBias != 0) {
            move_engine(engine, &sRel, sBias);
            say("moved %u addresses of the engine by 0x%lx", sRel.count, (unsigned long)sBias);
        }
        run_initialisers(&sRel, sBias);
    }
    if (getenv("BT3_CRASHLOG") == NULL || atoi(getenv("BT3_CRASHLOG")) != 0) {
        crash_handlers();
    }
    entry = (int (*)(int, char **))dlsym(handle, "__wrap_main");
    if (entry == NULL) {
        say("the engine has no entry point: %s", dlerror());
        return 1;
    }
    {   /* the engine's code range (its executable segment) for the watchdog */
        Elf64_Ehdr eh;
        Elf64_Phdr ph;
        int fd = open(engine, O_RDONLY | O_CLOEXEC), i;
        if (fd >= 0 && pread(fd, &eh, sizeof(eh), 0) == sizeof(eh)) {
            for (i = 0; i < eh.e_phnum; i++) {
                if (pread(fd, &ph, sizeof(ph), (off_t)(eh.e_phoff + (uint64_t)i * eh.e_phentsize)) == sizeof(ph) &&
                    ph.p_type == PT_LOAD && (ph.p_flags & PF_X)) {
                    sCodeLo = ph.p_vaddr + sBias;
                    sCodeHi = ph.p_vaddr + ph.p_memsz + sBias;
                }
            }
        }
        if (fd >= 0) {
            close(fd);
        }
        sVBlanks = (unsigned *)dlsym(handle, "gPortVBlanks");
        sEngine = handle;
        if (getenv("BT3_WATCHDOG") != NULL && strcmp(getenv("BT3_WATCHDOG"), "0") != 0) { /* (off unless asked for) */
            pthread_t th;
            pthread_create(&th, NULL, watchdog, NULL);
            pthread_detach(th);
        }
    }
    say("starting the game");
    {
        int r = entry(1, args);
        say("the game ended (%d)", r);
        return r;
    }
}

/* ---- The on-screen controller (com.dfdx047.dragonrage.engine.TouchBridge): its state into the engine's pad
   (gs_input.c), and whether the engine's own menu is open (then the controller steps aside). */
#include <jni.h>

JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativeSetPad(JNIEnv *env, jclass cls, jint buttons, jint lx, jint ly,
                                                                                   jint rx, jint ry) {
    static void (*fn)(unsigned, int, int, int, int);
    (void)env;
    (void)cls;
    if (fn == NULL && sEngine != NULL) {
        fn = (void (*)(unsigned, int, int, int, int))dlsym(sEngine, "Port_TouchPad");
    }
    if (fn != NULL) {
        fn((unsigned)buttons, lx, ly, rx, ry);
    }
}

/* A tap between the controls, in window pixels: the Tag Team menus press what is under it (gs_input.c). */
JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativeTap(JNIEnv *env, jclass cls, jint x, jint y) {
    static void (*fn)(int, int);
    (void)env;
    (void)cls;
    if (fn == NULL && sEngine != NULL) {
        fn = (void (*)(int, int))dlsym(sEngine, "Port_TouchTap");
    }
    if (fn != NULL) {
        fn(x, y);
    }
}

JNIEXPORT jboolean JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativeMenuOpen(JNIEnv *env, jclass cls) {
    static int (*fn)(void);
    (void)env;
    (void)cls;
    if (fn == NULL && sEngine != NULL) {
        fn = (int (*)(void))dlsym(sEngine, "Ui_IsOpen");
    }
    return fn != NULL && fn() ? JNI_TRUE : JNI_FALSE;
}

/* The touch menu (engine/GameMenu.kt through MenuBridge): the engine's Ui_Nm* (port/src/gs/ui_android.inc). */
static void *menu_fn(const char *name) {
    return sEngine != NULL ? dlsym(sEngine, name) : NULL;
}

JNIEXPORT jboolean JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeOpen(JNIEnv *env, jclass cls, jboolean open) {
    int (*fn)(int) = (int (*)(int))menu_fn("Ui_NmOpen");
    (void)env;
    (void)cls;
    return fn != NULL && fn(open ? 1 : 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeGen(JNIEnv *env, jclass cls) {
    int (*fn)(void) = (int (*)(void))menu_fn("Ui_NmGen");
    (void)env;
    (void)cls;
    return fn != NULL ? fn() : -1;
}

JNIEXPORT jint JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeGet(JNIEnv *env, jclass cls, jstring key, jint def) {
    int (*fn)(const char *, int) = (int (*)(const char *, int))menu_fn("Ui_NmGet");
    const char *k;
    jint v = def;
    (void)cls;
    if (fn == NULL || key == NULL) {
        return def;
    }
    k = (*env)->GetStringUTFChars(env, key, NULL);
    if (k != NULL) {
        v = fn(k, def);
        (*env)->ReleaseStringUTFChars(env, key, k);
    }
    return v;
}

JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeSet(JNIEnv *env, jclass cls, jstring key, jint value) {
    void (*fn)(const char *, int) = (void (*)(const char *, int))menu_fn("Ui_NmSet");
    const char *k;
    (void)cls;
    if (fn == NULL || key == NULL) {
        return;
    }
    k = (*env)->GetStringUTFChars(env, key, NULL);
    if (k != NULL) {
        fn(k, value);
        (*env)->ReleaseStringUTFChars(env, key, k);
    }
}

JNIEXPORT jstring JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeText(JNIEnv *env, jclass cls, jstring key) {
    int (*fn)(const char *, char *, int) = (int (*)(const char *, char *, int))menu_fn("Ui_NmText");
    char out[1100];
    const char *k;
    (void)cls;
    out[0] = '\0';
    if (fn != NULL && key != NULL) {
        k = (*env)->GetStringUTFChars(env, key, NULL);
        if (k != NULL) {
            fn(k, out, (int)sizeof(out));
            (*env)->ReleaseStringUTFChars(env, key, k);
        }
    }
    return (*env)->NewStringUTF(env, out);
}

JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeAction(JNIEnv *env, jclass cls, jint action) {
    void (*fn)(int) = (void (*)(int))menu_fn("Ui_NmAction");
    (void)env;
    (void)cls;
    if (fn != NULL) {
        fn(action);
    }
}

/* Read the player's actual side from BattleWork. In some battle setups the human side is side 1,
   so assuming sides[0] hides the team controls even though the touch buttons exist in the editor.
   BattleSide offsets are verified in include/battle/battle.h: sides at +0xC0, size 0x270,
   memberCount at +0, control at +0x208. BattleWork.running is +0x19F8. */
JNIEXPORT jint JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativePlayerTeamSize(JNIEnv *env, jclass cls) {
    typedef void *(*BattleGetWorkFn)(void);
    typedef int32_t (*BattleGetHumanSideFn)(void);
    typedef int32_t (*BattleSideGetMemberCountFn)(int32_t);
    typedef int32_t (*BattleSideGetControlFn)(int32_t);
    static BattleGetWorkFn get_work;
    static BattleGetHumanSideFn get_human_side;
    static BattleSideGetMemberCountFn get_member_count;
    static BattleSideGetControlFn get_control;
    static int accessors_resolved;
    static uint32_t last_magic = UINT32_MAX;
    static int32_t last_running = INT32_MIN;
    static int32_t last_count0 = INT32_MIN, last_control0 = INT32_MIN;
    static int32_t last_count1 = INT32_MIN, last_control1 = INT32_MIN;
    static int32_t last_side = INT32_MIN, last_result = INT32_MIN;
    uintptr_t work;
    void *work_ptr;
    uint32_t magic;
    int32_t running, count0, control0, count1, control1;
    int32_t result = 0, side = -1;
    const char *source = "idle";
    static int logged_engine_unavailable = 0;
    (void)env;
    (void)cls;
    if (sEngine == NULL) {
        if (!logged_engine_unavailable) {
            __android_log_print(ANDROID_LOG_WARN, TAG, "touch roster: engine handle unavailable; team state unknown");
            logged_engine_unavailable = 1;
        }
        return -1; /* unknown, not a single-character battle */
    }
    logged_engine_unavailable = 0;
    if (!accessors_resolved) {
        /* These are exported by libbt3.so (--export-dynamic); use the engine's own accessors first. */
        get_work = (BattleGetWorkFn)dlsym(sEngine, "Battle_GetWork");
        get_human_side = (BattleGetHumanSideFn)dlsym(sEngine, "Battle_GetHumanSide");
        get_member_count = (BattleSideGetMemberCountFn)dlsym(sEngine, "BattleSide_GetMemberCount");
        get_control = (BattleSideGetControlFn)dlsym(sEngine, "BattleSide_GetControl");
        accessors_resolved = 1;
        __android_log_print(ANDROID_LOG_INFO, TAG,
            "touch roster: engine accessors work=%s humanSide=%s memberCount=%s control=%s",
            get_work ? "yes" : "no", get_human_side ? "yes" : "no",
            get_member_count ? "yes" : "no", get_control ? "yes" : "no");
    }
    work_ptr = get_work != NULL ? get_work() : (void *)(uintptr_t)((intptr_t)0x00331DC8 + sBias);
    if (work_ptr == NULL) {
        __android_log_print(ANDROID_LOG_WARN, TAG, "touch roster: Battle_GetWork returned null; team state unknown");
        return -1;
    }
    work = (uintptr_t)work_ptr;
    memcpy(&magic, (const void *)work, sizeof(magic));
    memcpy(&running, (const void *)(work + 0x19F8), sizeof(running));
    memcpy(&count0, (const void *)(work + 0x0C0), sizeof(count0));
    memcpy(&control0, (const void *)(work + 0x2C8), sizeof(control0));
    memcpy(&count1, (const void *)(work + 0x330), sizeof(count1));
    memcpy(&control1, (const void *)(work + 0x538), sizeof(control1));

    {   /* the menus running (headless.c): the story mode's map, its talks and the screens after a fight leave the
           last fight's BattleWork as it was ("running" still 1), and the controller stayed in its fight layout */
        static volatile int *inMenus;
        static int looked;
        if (!looked) {
            inMenus = (volatile int *)dlsym(sEngine, "gPortInMenus");
            looked = 1;
        }
        if (inMenus != NULL && *inMenus) {
            running = 0;
        }
    }
    {   /* a Tag Team battle (the mod: src/battle/tag_team.c): every fighter is its own actor, the sides' member
           counts say nothing about it; L3 (aim at an attacker / unlock) and the target button are used there */
        typedef int32_t (*TagTeamActiveFn)(void);
        static TagTeamActiveFn tag_active;
        static int tag_looked;
        if (!tag_looked) {
            tag_active = (TagTeamActiveFn)dlsym(sEngine, "TagTeam_Active");
            tag_looked = 1;
        }
        if (running == 1 && tag_active != NULL && tag_active()) {
            result = 2;
            source = "tag-team";
            running = 2; /* (skips the side counts below) */
        }
    }
    if (running == 2) {
        /* (the Tag Team answer, above) */
    } else if (running != 1) {
        result = 0; /* no active battle: don't show team controls in menus */
        source = "idle";
    } else if (magic != 0x736C7462u) {
        result = -1; /* active battle but unexpected state layout: show controls and retain diagnostics */
        source = "unknown-magic";
    } else {
        if (get_human_side != NULL) {
            side = get_human_side();
        }
        if (side >= 0 && side <= 1 && get_member_count != NULL) {
            int32_t count = get_member_count(side);
            int32_t control = get_control != NULL ? get_control(side) : (side == 0 ? control0 : control1);
            if (control == 0 && count >= 1 && count <= 5) {
                result = count;
                source = "engine-accessor";
            }
        }
        /* Fall back to the verified BattleWork layout if a symbol is unavailable or reports an invalid count. */
        if (result < 1 || result > 5) {
            if (control0 == 0 && count0 >= 1 && count0 <= 5) {
                result = count0;
                side = 0;
                source = "memory-side0";
            } else if (control1 == 0 && count1 >= 1 && count1 <= 5) {
                result = count1;
                side = 1;
                source = "memory-side1";
            } else if (count0 >= 1 && count0 <= 5 && count1 >= 1 && count1 <= 5) {
                /* Fallback only if the side-control accessor cannot identify the human side. */
                result = count0 > count1 ? count0 : count1;
                source = "memory-max-fallback";
            } else {
                result = -1;
                source = "active-unknown";
            }
        }
    }

    /* Emit a compact event when battle state changes; the Java overlay also logs the sampled result. */
    if (magic != last_magic || running != last_running || count0 != last_count0 || control0 != last_control0 ||
        count1 != last_count1 || control1 != last_control1 || side != last_side || result != last_result) {
        __android_log_print(ANDROID_LOG_INFO, TAG,
            "touch roster: source=%s magic=%08x running=%d side0(control=%d,count=%d) side1(control=%d,count=%d) humanSide=%d playerTeam=%d bias=%+ld",
            source, magic, running, control0, count0, control1, count1, side, result, (long)sBias);
        last_magic = magic;
        last_running = running;
        last_count0 = count0;
        last_control0 = control0;
        last_count1 = count1;
        last_control1 = control1;
        last_side = side;
        last_result = result;
    }
    return (jint)result;
}
