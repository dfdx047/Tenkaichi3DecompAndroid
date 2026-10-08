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
    setenv("SDL_VIDEO_DRIVER", "android", 0);
    setenv("SDL_ANDROID_TRAP_BACK_BUTTON", "1", 0); /* the back button opens the settings (gs_gpu.c), it does not end the game */

    if (!program_span(engine, &lo, &hi)) {
        say("cannot read the engine %s", engine);
        return 1;
    }
    lo &= ~(page - 1);
    hi = (hi + page - 1) & ~(page - 1);
    region = mmap((void *)lo, hi - lo, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (region != (void *)lo) {
        say("the engine's address range 0x%lx-0x%lx is taken in this process (%s)", (unsigned long)lo, (unsigned long)hi,
            region == MAP_FAILED ? strerror(errno) : "mapped elsewhere");
        log_low_maps();
        return 1;
    }
    log_low_maps(); /* (what the game's own fixed regions will have to fit around: plat_mem.c) */
    {
        android_dlextinfo info;
        memset(&info, 0, sizeof(info));
        info.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
        info.reserved_addr = region;
        info.reserved_size = hi - lo;
        say("loading the engine at 0x%lx (%lu KB)", (unsigned long)lo, (unsigned long)((hi - lo) >> 10));
        handle = android_dlopen_ext(engine, RTLD_NOW | RTLD_GLOBAL, &info);
    }
    if (handle == NULL) {
        say("the engine did not load: %s", dlerror());
        log_low_maps();
        return 1;
    }
    entry = (int (*)(int, char **))dlsym(handle, "__wrap_main");
    if (entry == NULL) {
        say("the engine has no entry point: %s", dlerror());
        return 1;
    }
    say("starting the game");
    {
        int r = entry(1, args);
        say("the game ended (%d)", r);
        return r;
    }
}
