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
    snprintf(path, sizeof(path), "%s/bt3_log.txt", dir);
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
   its symbols). A rough backtrace, but enough to see where the game waits. Off with BT3_WATCHDOG=0. */
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

__attribute__((visibility("default"))) VkVoidFn vkGetInstanceProcAddr(void *instance, const char *name) {
    return sDriverGipa != NULL ? sDriverGipa(instance, name) : NULL;
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
        if (getenv("BT3_WATCHDOG") == NULL || strcmp(getenv("BT3_WATCHDOG"), "0") != 0) {
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

JNIEXPORT jboolean JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativeMenuOpen(JNIEnv *env, jclass cls) {
    static int (*fn)(void);
    (void)env;
    (void)cls;
    if (fn == NULL && sEngine != NULL) {
        fn = (int (*)(void))dlsym(sEngine, "Ui_IsOpen");
    }
    return fn != NULL && fn() ? JNI_TRUE : JNI_FALSE;
}
