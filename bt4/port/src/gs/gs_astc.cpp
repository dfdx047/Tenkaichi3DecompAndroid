// Texture packs in ASTC for phone GPUs (Android): Mali, Adreno and the others read ASTC natively but not DXT, so a
// pack's .dds replacements are unpacked to plain 32-bit pixels when they load (gs_draw.c), four times the memory of
// DXT and the bandwidth with it. This keeps an ASTC 4x4 copy of each replacement (8 bits a pixel, like DXT5) in
// <game folder>/textures_astc/, made in the background, on one low-priority thread, the first time a replacement is
// used: that time it is shown from the plain pixels as before, from the next load on from the ASTC copy. Nothing of
// the pack itself is changed; the folder can be deleted at any time (it is made again).
//
// A copy (".dra") is: "DRA1", the source file's size (8 bytes, so that a replaced file is converted again), width,
// height, number of levels, block size (4), each level's byte count, then the levels. ARM's astc-encoder
// (port/third_party/astcenc, Apache-2.0) at its fastest preset.
#ifdef __ANDROID__
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <SDL3/SDL.h>
#include "../../third_party/astcenc/astcenc.h"
extern "C" {
#include "gs_texpack.h"
}

extern "C" int gGsAstcTextures; // gs_draw.c: the renderer takes ASTC 4x4 (set by the back end)
extern "C" int gGsAstcOn;       // gs_draw.c: the setting "astc" (1 = make and use the copies)

static const char *kDir = "textures_astc";
static const uint32_t kLevelsMax = TEXPACK_MAX_LEVELS;

static uint64_t fnv(const char *s) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (; *s; s++) {
        h = (h ^ (uint8_t)*s) * 0x100000001B3ull;
    }
    return h;
}

static void copy_path(const char *src, char *out, size_t n) {
    snprintf(out, n, "%s/%016llx.dra", kDir, (unsigned long long)fnv(src));
}

static long long file_size(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 ? (long long)st.st_size : -1;
}

// ---- reading a copy

extern "C" int GsAstc_Load(const char *src, TexPackImage *img) {
    char path[600];
    size_t size = 0;
    uint8_t *f;
    uint32_t w, h, levels, block, at, l;
    uint64_t srcSize;

    if (!gGsAstcTextures || !gGsAstcOn) {
        return 0;
    }
    copy_path(src, path, sizeof(path));
    f = (uint8_t *)SDL_LoadFile(path, &size);
    if (f == NULL) {
        return 0;
    }
    if (size < 28 || memcmp(f, "DRA1", 4) != 0) {
        SDL_free(f);
        return 0;
    }
    memcpy(&srcSize, f + 4, 8);
    memcpy(&w, f + 12, 4);
    memcpy(&h, f + 16, 4);
    memcpy(&levels, f + 20, 4);
    memcpy(&block, f + 24, 4);
    if ((long long)srcSize != file_size(src) || block != 4 || levels == 0 || levels > kLevelsMax || w == 0 || h == 0 || w > 8192 || h > 8192) {
        SDL_free(f);
        return 0;
    }
    memset(img, 0, sizeof(*img));
    at = 28 + levels * 4;
    for (l = 0; l < levels; l++) {
        uint32_t bytes, lw = w >> l ? w >> l : 1, lh = h >> l ? h >> l : 1;
        memcpy(&bytes, f + 28 + l * 4, 4);
        if (bytes != ((lw + 3) / 4) * ((lh + 3) / 4) * 16 || (size_t)at + bytes > size) {
            SDL_free(f);
            return 0;
        }
        img->data[l] = f + at;
        img->bytes[l] = bytes;
        at += bytes;
    }
    img->file = f;
    img->w = w;
    img->h = h;
    img->format = 4;
    img->levels = (int)levels;
    return 1;
}

// ---- making them, in the background

enum { QUEUE = 256 };
static char *sQueue[QUEUE];
static int sHead, sTail, sStarted;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sWake = PTHREAD_COND_INITIALIZER;
static uint64_t sAsked[4096]; // the sources queued in this run (by name), so that each is converted once

static int convert(const char *src, astcenc_context *ctx) {
    TexPackImage img;
    char path[600], tmp[620];
    uint32_t lens[kLevelsMax], l;
    uint8_t *out[kLevelsMax];
    long long srcSize = file_size(src);
    FILE *fp;
    int ok = 1;

    if (srcSize <= 0 || !TexPack_Load(src, &img)) {
        return 0;
    }
    if (!TexPack_Decompress(&img, 2048)) { // (plain pixels; levels above 2048 left out, as when they are shown)
        TexPack_Free(&img);
        return 0;
    }
    memset(out, 0, sizeof(out));
    for (l = 0; l < (uint32_t)img.levels && ok; l++) {
        uint32_t lw = img.w >> l ? img.w >> l : 1, lh = img.h >> l ? img.h >> l : 1;
        astcenc_image im;
        void *slices[1] = {(void *)img.data[l]};
        const astcenc_swizzle swz = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
        lens[l] = ((lw + 3) / 4) * ((lh + 3) / 4) * 16;
        out[l] = (uint8_t *)malloc(lens[l]);
        im.dim_x = lw;
        im.dim_y = lh;
        im.dim_z = 1;
        im.data_type = ASTCENC_TYPE_U8;
        im.data = slices;
        ok = out[l] != NULL && astcenc_compress_image(ctx, &im, &swz, out[l], lens[l], 0) == ASTCENC_SUCCESS;
        astcenc_compress_reset(ctx);
    }
    if (ok) {
        uint32_t head[6] = {0, 0, img.w, img.h, (uint32_t)img.levels, 4};
        uint64_t s = (uint64_t)srcSize;
        copy_path(src, path, sizeof(path));
        snprintf(tmp, sizeof(tmp), "%s.tmp", path);
        mkdir(kDir, 0755);
        fp = fopen(tmp, "wb");
        ok = fp != NULL;
        if (ok) {
            memcpy(head, "DRA1", 4);
            ok = fwrite(head, 4, 1, fp) == 1 && fwrite(&s, 8, 1, fp) == 1 && fwrite(head + 2, 4, 4, fp) == 4 &&
                 fwrite(lens, 4, (size_t)img.levels, fp) == (size_t)img.levels;
            for (l = 0; l < (uint32_t)img.levels && ok; l++) {
                ok = fwrite(out[l], 1, lens[l], fp) == lens[l];
            }
            ok = fclose(fp) == 0 && ok;
            ok = ok && rename(tmp, path) == 0;
            if (!ok) {
                remove(tmp);
            }
        }
    }
    for (l = 0; l < kLevelsMax; l++) {
        free(out[l]);
    }
    TexPack_Free(&img);
    return ok;
}

static void *worker(void *) {
    astcenc_config cfg;
    astcenc_context *ctx = NULL;
    unsigned done = 0;

    setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 19); // (the game's threads first: this only uses spare time)
    if (astcenc_config_init(ASTCENC_PRF_LDR, 4, 4, 1, ASTCENC_PRE_FASTEST, 0, &cfg) != ASTCENC_SUCCESS ||
        astcenc_context_alloc(&cfg, 1, &ctx, NULL) != ASTCENC_SUCCESS) {
        fprintf(stderr, "bt3: ASTC: the encoder could not start; textures stay as they are\n");
        return NULL;
    }
    for (;;) {
        char *src;
        pthread_mutex_lock(&sLock);
        while (sHead == sTail) {
            pthread_cond_wait(&sWake, &sLock);
        }
        src = sQueue[sHead];
        sHead = (sHead + 1) % QUEUE;
        pthread_mutex_unlock(&sLock);
        if (convert(src, ctx) && (++done == 1 || done % 50 == 0)) {
            fprintf(stderr, "bt3: ASTC: %u replacement textures converted for this GPU (textures_astc)\n", done);
        }
        free(src);
    }
    return NULL;
}

extern "C" void GsAstc_Queue(const char *src) {
    uint64_t h = fnv(src);
    if (!gGsAstcTextures || !gGsAstcOn) {
        return;
    }
    pthread_mutex_lock(&sLock);
    if (sAsked[h & 4095] == h) {
        pthread_mutex_unlock(&sLock);
        return;
    }
    if ((sTail + 1) % QUEUE != sHead) {
        sAsked[h & 4095] = h;
        sQueue[sTail] = strdup(src);
        sTail = (sTail + 1) % QUEUE;
        if (!sStarted) {
            pthread_t th;
            pthread_attr_t attr;
            sStarted = 1;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            pthread_create(&th, &attr, worker, NULL);
            pthread_attr_destroy(&attr);
        }
        pthread_cond_signal(&sWake);
    }
    pthread_mutex_unlock(&sLock);
}
#endif
