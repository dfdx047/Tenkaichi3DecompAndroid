/*
 * Texture packs: replacement textures in the naming of PCSX2's texture replacement, so packs made for the game
 * under that emulator work as they are.
 *
 * A file is named <texture hash>-<palette hash>-<bits>.dds (or .png) (two parts, without the palette hash, for a texture
 * without a palette), the hashes in hexadecimal without leading zeros:
 *   texture hash   XXH3 (64 bits) over the texture's raw 256-byte blocks of GS memory, block after block in
 *                  row-major block order over the texture's rectangle: the data as it lies in GS memory
 *   palette hash   XXH3 over the palette's 32-bit entries in index order, from the lowest to the highest index
 *                  the texture uses (some files: over the whole palette; both are looked up)
 *   bits           PSM | log2(width) << 6 | log2(height) << 10 | TCC << 14
 * Found by matching a pack against the textures the game uploads (docs/port/README.md). The files are looked for
 * at any depth below the folder `textures` next to the program (or the one BT3_TEXTURES names). A pack's alpha
 * is the GS's own (0x80 = opaque), like the textures it replaces.
 *
 * Read: DDS with DXT1 / DXT3 / DXT5 data or plain 32-bit pixels, with its smaller copies (mip levels), and PNG
 * (the smaller copies are made here). Where a name exists as both, the first one found is used.
 *
 * The files are read by a thread of their own (TexPack_Get), the way PCSX2 loads them "asynchronously": a file read
 * inside a frame held the picture up for as long as the disk took, 30 to 150 ms when an ultimate's effects first
 * appear, and on a machine whose memory does not keep the pack's files cached every time the game uploads one of
 * its textures again (hundreds of times a fight). Until a file is in, the renderer draws the original texture. What
 * was read stays in memory (TEXPACK_BUDGET, the least recently used files go first). BT3_PACK_SYNC=1: each file is
 * read at once, inside the frame, as before (the same picture in every run: tests).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __ANDROID__
#include <unistd.h> /* getcwd */
#endif
#include <SDL3/SDL.h>
#define XXH_INLINE_ALL
#include "../../third_party/xxhash/xxhash.h"
#include "gs_internal.h"
#include "gs_texpack.h"

typedef struct Entry {
    uint64_t tex, clut;
    uint32_t bits;
    int pal;
    char *path;
    int state;        /* 0 not in memory, 1 waiting for the reader, 2 in memory (img), 3 unreadable */
    unsigned used;    /* when TexPack_Get last asked for it */
    size_t held;      /* the bytes img holds */
    TexPackImage img;
} Entry;

static Entry *sEntries;
static uint32_t sCount, sCap, *sBucket, sBuckets;

#ifdef __ANDROID__
#define TEXPACK_BUDGET ((size_t)256 << 20)  /* (a phone: the memory is the system's and the GPU's too) */
#else
#define TEXPACK_BUDGET ((size_t)1024 << 20) /* the files kept in memory: one session's two fights took 289 MB */
#endif
#define QUEUE 4096                          /* files waiting for the reader (a power of two) */
static SDL_Mutex *sLock;
static SDL_Condition *sWake;
static SDL_Thread *sReader;
static uint32_t sQueue[QUEUE], sHead, sTail; /* the reader takes from sTail */
static uint32_t *sHeld, sHeldCount;          /* the entries in memory (state 2) */
static size_t sHeldBytes, sBudget;
static unsigned sTick;
static int sSync = -1;
static void image_free(TexPackImage *img);

/* Z Warriors' HD pictures (the photos of a warrior: its render, small portrait and health-bar faces). The game's own
   texture is 8-bit and small; its palette carries a tag in entries 252..254 (index order) that no pixel uses:
     252  nonce | 0x03 << 24   (changes with each new photo: the picture is read again)
     253  0x015A5748            ("HWZ", alpha 1)
     254  key | 0x02 << 24      (which warrior and which picture: plat_mods.c Port_ZwHdPath finds its file)
   Such a texture is drawn from that PNG instead, whatever the texture pack setting. */
#define HD_MAX 64
#define HD_BASE 0x40000000
#define HD_MAGIC 0x015A5748u
static Entry sHd[HD_MAX];
static uint32_t sHdKey[HD_MAX], sHdNonce[HD_MAX];
static int sHdN, sStaticOn = 1;
extern int Port_ZwHdPath(uint32_t key, char *out, int max);

static Entry *ent(uint32_t id) {
    return id >= HD_BASE ? &sHd[id - HD_BASE] : &sEntries[id];
}

void TexPack_SetStatic(int on) {
    sStaticOn = on;
}

int TexPack_IsHd(int id) {
    return id >= HD_BASE;
}

/* The tagged texture's HD entry, or -1. */
static int hd_lookup(uint32_t psm, uint32_t cbp, uint32_t cpsm) {
    uint32_t c[3], i, key, nonce;
    int k;
    char path[1024];

    if (psm != 0x13 || cpsm != 0) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        uint32_t n = 252 + i, s = (n & 0xE7) | ((n & 8) << 1) | ((n & 0x10) >> 1);
        c[i] = Gs_VramRead(cbp, 1, cpsm, s & 15, s >> 4);
    }
    if (c[1] != HD_MAGIC || (c[2] >> 24) != 2 || (c[0] >> 24) != 3) {
        return -1;
    }
    key = c[2] & 0xFFFFFF;
    nonce = c[0] & 0xFFFFFF;
    for (k = 0; k < sHdN; k++) {
        if (sHdKey[k] == key) {
            break;
        }
    }
    if (k < sHdN && sHdNonce[k] == nonce && sHd[k].state != 3) {
        return HD_BASE + k;
    }
    if (!Port_ZwHdPath(key, path, (int)sizeof(path))) {
        return -1;
    }
    if (k == sHdN) {
        if (sHdN == HD_MAX) {
            return -1;
        }
        sHdN++;
        memset(&sHd[k], 0, sizeof(sHd[k]));
    }
    if (sLock != NULL) {
        SDL_LockMutex(sLock);
    }
    if (sHd[k].state == 1) { /* (still being read: its old file; asked again later) */
        if (sLock != NULL) {
            SDL_UnlockMutex(sLock);
        }
        return -1;
    }
    if (sHd[k].state == 2) {
        uint32_t j;
        image_free(&sHd[k].img);
        sHeldBytes -= sHd[k].held;
        sHd[k].held = 0;
        for (j = 0; j < sHeldCount; j++) {
            if (sHeld[j] == (uint32_t)(HD_BASE + k)) {
                sHeld[j] = sHeld[--sHeldCount];
                break;
            }
        }
    }
    free(sHd[k].path);
    sHd[k].path = strdup(path);
    sHd[k].state = 0;
    sHdKey[k] = key;
    sHdNonce[k] = nonce;
    if (sLock != NULL) {
        SDL_UnlockMutex(sLock);
    }
    fprintf(stderr, "bt3: HD picture %06x: %s\n", key, path);
    return HD_BASE + k;
}

static uint32_t bucket_of(uint64_t tex, uint64_t clut, uint32_t bits) {
    uint64_t h = (tex ^ (clut * 0x9E3779B97F4A7C15ull) ^ bits) * 0x9FB21C651E98DF25ull;
    return (uint32_t)(h >> 40);
}

static int find(uint64_t tex, uint64_t clut, uint32_t bits, int pal) {
    uint32_t h = bucket_of(tex, clut, bits) & (sBuckets - 1);
    while (sBucket[h] != 0) {
        const Entry *e = &sEntries[sBucket[h] - 1];
        if (e->tex == tex && e->clut == clut && e->bits == bits && e->pal == pal) {
            return (int)sBucket[h] - 1;
        }
        h = (h + 1) & (sBuckets - 1);
    }
    return -1;
}

/* "1dd4c76113969303-56e3d4469d2ad392-00005e54.dds" */
static void add(const char *dir, const char *name) {
    size_t n = strlen(name);
    char part[3][20];
    int parts = 0, len = 0;
    size_t i;
    Entry e;

    if (n < 8 || (SDL_strcasecmp(name + n - 4, ".dds") != 0 && SDL_strcasecmp(name + n - 4, ".png") != 0)) {
        return;
    }
    for (i = 0; i < n - 4; i++) {
        char c = name[i];
        if (c == '-') {
            if (len == 0 || parts == 2) {
                return;
            }
            part[parts++][len] = '\0';
            len = 0;
        } else if (((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) && len < 16) {
            part[parts][len++] = c;
        } else {
            return; /* not one of these names */
        }
    }
    if (len == 0 || parts == 0) {
        return;
    }
    part[parts++][len] = '\0';
    e.tex = strtoull(part[0], NULL, 16);
    e.pal = parts == 3;
    e.clut = e.pal ? strtoull(part[1], NULL, 16) : 0;
    e.bits = (uint32_t)strtoul(part[parts - 1], NULL, 16);
    e.state = 0;
    e.used = 0;
    e.held = 0;
    memset(&e.img, 0, sizeof(e.img));
    e.path = malloc(strlen(dir) + n + 1);
    strcpy(e.path, dir);
    strcat(e.path, name);
    if (sCount == sCap) {
        sCap = sCap ? sCap * 2 : 4096;
        sEntries = realloc(sEntries, sCap * sizeof(Entry));
    }
    sEntries[sCount++] = e;
}

static SDL_EnumerationResult SDLCALL walk(void *userdata, const char *dir, const char *name) {
    size_t n = strlen(dir) + strlen(name) + 2;
    char *path = malloc(n);
    SDL_PathInfo info;

    (void)userdata;
    snprintf(path, n, "%s%s", dir, name);
    if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
        strcat(path, "/");
        SDL_EnumerateDirectory(path, walk, NULL);
    } else {
        add(dir, name);
    }
    free(path);
    return SDL_ENUM_CONTINUE;
}

/* ---- DXT (BC1-BC3) to 8-bit RGBA, for GPUs without DXT (phones: Adreno and Mali under Vulkan and OpenGL ES). The
   colours and alphas are those a GPU's own decoder gives (opaque BC1 texels 255, as sampled). */
static void bc_color(const uint8_t *b, uint8_t pal[4][4], int four) {
    uint32_t c[2], i;
    c[0] = b[0] | (b[1] << 8);
    c[1] = b[2] | (b[3] << 8);
    for (i = 0; i < 2; i++) {
        uint32_t r = (c[i] >> 11) & 31, g = (c[i] >> 5) & 63, bl = c[i] & 31;
        pal[i][0] = (uint8_t)((r << 3) | (r >> 2));
        pal[i][1] = (uint8_t)((g << 2) | (g >> 4));
        pal[i][2] = (uint8_t)((bl << 3) | (bl >> 2));
        pal[i][3] = 255;
    }
    for (i = 0; i < 3; i++) {
        if (four || c[0] > c[1]) {
            pal[2][i] = (uint8_t)((2 * pal[0][i] + pal[1][i] + 1) / 3);
            pal[3][i] = (uint8_t)((pal[0][i] + 2 * pal[1][i] + 1) / 3);
        } else {
            pal[2][i] = (uint8_t)((pal[0][i] + pal[1][i]) / 2);
            pal[3][i] = 0;
        }
    }
    pal[2][3] = 255;
    pal[3][3] = (four || c[0] > c[1]) ? 255 : 0;
}

static void bc_block(int format, const uint8_t *b, uint8_t out[16][4]) {
    uint8_t pal[4][4], alpha[16];
    const uint8_t *cb = format == 1 ? b : b + 8;
    uint32_t idx = cb[4] | (cb[5] << 8) | (cb[6] << 16) | ((uint32_t)cb[7] << 24), i;
    bc_color(cb, pal, format != 1);
    if (format == 2) { /* DXT3: 4 bits of alpha per texel */
        for (i = 0; i < 16; i++) {
            uint32_t a = (b[i / 2] >> ((i & 1) * 4)) & 15;
            alpha[i] = (uint8_t)(a * 17);
        }
    } else if (format == 3) { /* DXT5: two alphas and 3-bit indices between them */
        uint8_t a[8];
        uint64_t bits = 0;
        a[0] = b[0];
        a[1] = b[1];
        if (a[0] > a[1]) {
            for (i = 1; i < 7; i++) { a[i + 1] = (uint8_t)(((7 - i) * a[0] + i * a[1] + 3) / 7); }
        } else {
            for (i = 1; i < 5; i++) { a[i + 1] = (uint8_t)(((5 - i) * a[0] + i * a[1] + 2) / 5); }
            a[6] = 0;
            a[7] = 255;
        }
        for (i = 0; i < 6; i++) { bits |= (uint64_t)b[2 + i] << (8 * i); }
        for (i = 0; i < 16; i++) { alpha[i] = a[(bits >> (3 * i)) & 7]; }
    }
    for (i = 0; i < 16; i++) {
        const uint8_t *c = pal[(idx >> (2 * i)) & 3];
        out[i][0] = c[0];
        out[i][1] = c[1];
        out[i][2] = c[2];
        out[i][3] = format == 1 ? c[3] : alpha[i];
    }
}

/* A DXT image to 8-bit RGBA, all its levels; levels larger than `maxSide` are left out (GPU memory: a phone shares
   it with everything else, and RGBA takes 4 to 8 times what DXT does). 0 when there is no memory for it. */
int TexPack_Decompress(TexPackImage *img, uint32_t maxSide) {
    uint32_t total = 0, w, h, first = 0;
    int l, n = 0;
    uint8_t *buf, *at;
    TexPackImage out;

    if (img->format == 0) {
        return 1;
    }
    while (first + 1 < (uint32_t)img->levels && ((img->w >> first) > maxSide || (img->h >> first) > maxSide)) {
        first++;
    }
    for (l = (int)first; l < img->levels; l++) {
        w = img->w >> l ? img->w >> l : 1;
        h = img->h >> l ? img->h >> l : 1;
        total += w * h * 4;
    }
    if ((buf = SDL_malloc(total)) == NULL) {
        return 0;
    }
    memset(&out, 0, sizeof(out));
    out.file = buf;
    out.w = img->w >> first ? img->w >> first : 1;
    out.h = img->h >> first ? img->h >> first : 1;
    out.format = 0;
    at = buf;
    for (l = (int)first; l < img->levels; l++, n++) {
        uint32_t bw, bh, x, y, blk = img->format == 1 ? 8 : 16;
        const uint8_t *src = img->data[l];
        w = img->w >> l ? img->w >> l : 1;
        h = img->h >> l ? img->h >> l : 1;
        bw = (w + 3) / 4;
        bh = (h + 3) / 4;
        if (img->bytes[l] < bw * bh * blk) { /* (a level cut short in the file: the levels before it are kept) */
            break;
        }
        for (y = 0; y < bh; y++) {
            for (x = 0; x < bw; x++) {
                uint8_t px[16][4];
                uint32_t i, j;
                bc_block(img->format, src + (y * bw + x) * blk, px);
                for (j = 0; j < 4 && y * 4 + j < h; j++) {
                    for (i = 0; i < 4 && x * 4 + i < w; i++) {
                        memcpy(at + ((y * 4 + j) * w + x * 4 + i) * 4, px[j * 4 + i], 4);
                    }
                }
            }
        }
        out.data[n] = at;
        out.bytes[n] = w * h * 4;
        at += w * h * 4;
    }
    if (n == 0) {
        SDL_free(buf);
        return 0;
    }
    out.levels = n;
    SDL_free(img->file);
    *img = out;
    return 1;
}

void TexPack_Init(void) {
    const char *root = getenv("BT3_TEXTURES") != NULL ? getenv("BT3_TEXTURES") : "textures";
    size_t n;
    char *dir;
    uint32_t i, dup = 0;
    SDL_PathInfo info;
#ifdef __ANDROID__
    /* SDL on Android takes a relative path as one in the app's internal folder (/data/user/0/<app>/files), not
       the current folder: "textures" was never the launcher's folder of packs next to gamedata/, and a pack
       imported in the app was never used ("no folder textures" with the pack in place). From the game's folder. */
    static char absRoot[1200];
    char cwd[1024];
    if (root[0] != '\0' && root[0] != '/' && getcwd(cwd, sizeof(cwd)) != NULL) {
        snprintf(absRoot, sizeof(absRoot), "%s/%s", cwd, root);
        root = absRoot;
    }
#endif
    n = strlen(root);
    dir = malloc(n + 2);

    if (root[0] == '\0' || !SDL_GetPathInfo(root, &info) || info.type != SDL_PATHTYPE_DIRECTORY) {
        fprintf(stderr, "bt3: texture pack: no folder %s\n", root);
        free(dir);
        return;
    }
    strcpy(dir, root);
    if (dir[n - 1] != '/' && dir[n - 1] != '\\') {
        strcat(dir, "/");
    }
    SDL_EnumerateDirectory(dir, walk, NULL);
    free(dir);
    if (sCount == 0) {
        /* (in the log: a pack whose files are not named as PCSX2 names them, or one switched off in the launcher) */
        fprintf(stderr, "bt3: texture pack: no replacement textures (PCSX2 names) found in %s\n", root);
        return;
    }
    for (sBuckets = 1024; sBuckets < sCount * 2; sBuckets *= 2) {
    }
    sBucket = calloc(sBuckets, sizeof(uint32_t));
    for (i = 0; i < sCount; i++) {
        const Entry *e = &sEntries[i];
        uint32_t h = bucket_of(e->tex, e->clut, e->bits) & (sBuckets - 1);
        if (find(e->tex, e->clut, e->bits, e->pal) >= 0) {
            dup++; /* the same name in two folders of the pack: the first one found is used */
            continue;
        }
        while (sBucket[h] != 0) {
            h = (h + 1) & (sBuckets - 1);
        }
        sBucket[h] = i + 1;
    }
    fprintf(stderr, "bt3: texture pack: %u replacement textures in %s (%u more with a name already seen)\n", sCount - dup, root, dup);
}

int TexPack_Count(void) {
    return (int)sCount;
}

const char *TexPack_Path(int id) {
    return id >= HD_BASE && id < HD_BASE + sHdN ? sHd[id - HD_BASE].path : id >= 0 && (uint32_t)id < sCount ? sEntries[id].path : NULL;
}

int TexPack_Lookup(uint32_t tbp, uint32_t tbw, uint32_t psm, uint32_t tw, uint32_t th, uint32_t tcc, uint32_t cbp, uint32_t cpsm, uint32_t *maxAlpha) {
    uint32_t bw, bh, x, y, i, lo = 255, hi = 0, bits, wl = 0, hl = 0;
    XXH3_state_t st;
    uint64_t tex;
    int pal = psm == 0x13 || psm == 0x14;

    *maxAlpha = 255;
    {
        int hd = hd_lookup(psm, cbp, cpsm);
        if (hd >= 0) {
            *maxAlpha = 0x80;
            return hd;
        }
    }
    if (sCount == 0 || !sStaticOn) {
        return -1;
    }
    if (psm == 0x13) { bw = 16; bh = 16; }
    else if (psm == 0x14) { bw = 32; bh = 16; }
    else if (psm == 0x00) { bw = 8; bh = 8; }
    else { return -1; }
    if (tw < bw || th < bh) {
        return -1; /* smaller than a block: hashed another way there, which is not worked out */
    }
    XXH3_64bits_reset(&st);
    for (y = 0; y < th; y += bh) {
        for (x = 0; x < tw; x += bw) {
            const uint8_t *p = Gs_BlockPtr(tbp, tbw, psm, x, y);
            XXH3_64bits_update(&st, p, 256);
            if (psm == 0x13) {
                for (i = 0; i < 256; i++) {
                    if (p[i] < lo) { lo = p[i]; }
                    if (p[i] > hi) { hi = p[i]; }
                }
            } else if (psm == 0x14) {
                for (i = 0; i < 256; i++) {
                    uint32_t a = p[i] & 15, b = p[i] >> 4;
                    if (a < lo) { lo = a; }
                    if (b < lo) { lo = b; }
                    if (a > hi) { hi = a; }
                    if (b > hi) { hi = b; }
                }
            }
        }
    }
    tex = XXH3_64bits_digest(&st);
    while ((1u << wl) < tw) { wl++; }
    while ((1u << hl) < th) { hl++; }
    bits = psm | wl << 6 | hl << 10 | (tcc & 1) << 14;
    if (!pal) {
        return find(tex, 0, bits, 0);
    }
    if (cpsm != 0) {
        return -1; /* 16-bit palettes are expanded with TEXA there: not seen in this game */
    }
    {
        uint32_t clut[256], count = psm == 0x13 ? 256 : 16;
        int id;
        for (i = 0; i < count; i++) {
            /* index order: an 8-bit palette is stored with entries 8..15 and 16..23 of every 32 exchanged */
            uint32_t k = psm == 0x13 ? ((i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1)) : i;
            clut[i] = psm == 0x13 ? Gs_VramRead(cbp, 1, cpsm, k & 15, k >> 4) : Gs_VramRead(cbp, 1, cpsm, i & 7, i >> 3);
        }
        /* the largest alpha the ORIGINAL has (over the palette entries it uses): see the header of this function */
        *maxAlpha = 0;
        for (i = lo; i <= hi; i++) {
            if (clut[i] >> 24 > *maxAlpha) {
                *maxAlpha = clut[i] >> 24;
            }
        }
        id = find(tex, XXH3_64bits(&clut[lo], (hi - lo + 1) * 4), bits, 1);
        if (id < 0) {
            id = find(tex, XXH3_64bits(clut, count * 4), bits, 1);
        }
        if (getenv("BT3_TEX_LOG") != NULL) { /* which textures a pack has and which it has not, by the name it would need */
            fprintf(stderr, "texpack: %s %ux%u %u-bit %llx-%llx-%08x%s%s\n", id >= 0 ? "found  " : "missing", tw, th, psm == 0x13 ? 8u : 4u,
                    (unsigned long long)tex, (unsigned long long)XXH3_64bits(&clut[lo], (hi - lo + 1) * 4), bits, id >= 0 ? " " : "",
                    id >= 0 ? sEntries[id].path : "");
        }
        return id;
    }
}

/* A PNG: decoded by SDL, and its smaller copies (mip levels) made here, each the average of 2 x 2 pixels of the one
   before. */
static int load_png(const char *path, TexPackImage *img) {
    SDL_Surface *png = SDL_LoadPNG(path), *rgba;
    uint32_t w, h, total = 0, lw, lh, at = 0, x, y, c;
    int level, levels = 0;
    uint8_t *f;

    if (png == NULL) {
        return 0;
    }
    rgba = SDL_ConvertSurface(png, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(png);
    if (rgba == NULL || rgba->w <= 0 || rgba->h <= 0 || rgba->w > 8192 || rgba->h > 8192) {
        SDL_DestroySurface(rgba);
        return 0;
    }
    w = (uint32_t)rgba->w;
    h = (uint32_t)rgba->h;
    for (lw = w, lh = h; levels < TEXPACK_MAX_LEVELS; lw = lw > 1 ? lw / 2 : 1, lh = lh > 1 ? lh / 2 : 1) {
        total += lw * lh * 4;
        levels++;
        if (lw == 1 && lh == 1) {
            break;
        }
    }
    f = SDL_malloc(total);
    for (y = 0; y < h; y++) {
        memcpy(f + y * w * 4, (const uint8_t *)rgba->pixels + y * (uint32_t)rgba->pitch, w * 4);
    }
    SDL_DestroySurface(rgba);
    img->file = f;
    img->w = w;
    img->h = h;
    img->format = 0;
    img->levels = levels;
    for (level = 0, lw = w, lh = h; level < levels; level++) {
        img->data[level] = f + at;
        img->bytes[level] = lw * lh * 4;
        if (level + 1 < levels) {
            const uint8_t *src = f + at;
            uint32_t nw = lw > 1 ? lw / 2 : 1, nh = lh > 1 ? lh / 2 : 1;
            uint8_t *dst = f + at + lw * lh * 4;
            for (y = 0; y < nh; y++) {
                for (x = 0; x < nw; x++) {
                    uint32_t x0 = x * 2 < lw ? x * 2 : lw - 1, x1 = x * 2 + 1 < lw ? x * 2 + 1 : lw - 1;
                    uint32_t y0 = y * 2 < lh ? y * 2 : lh - 1, y1 = y * 2 + 1 < lh ? y * 2 + 1 : lh - 1;
                    for (c = 0; c < 4; c++) {
                        dst[(y * nw + x) * 4 + c] = (uint8_t)((src[(y0 * lw + x0) * 4 + c] + src[(y0 * lw + x1) * 4 + c] +
                                                               src[(y1 * lw + x0) * 4 + c] + src[(y1 * lw + x1) * 4 + c] + 2) / 4);
                    }
                }
            }
            at += lw * lh * 4;
            lw = nw;
            lh = nh;
        }
    }
    return 1;
}

static int load(const char *path, TexPackImage *img) {
    size_t size = 0;
    uint8_t *f;
    uint32_t w, h, mips, flags, fourcc, bpp, blk, at = 128, level;

    memset(img, 0, sizeof(*img));
    if (strlen(path) > 4 && SDL_strcasecmp(path + strlen(path) - 4, ".png") == 0) {
        return load_png(path, img);
    }
    f = SDL_LoadFile(path, &size);
    if (f == NULL || size < 128 || memcmp(f, "DDS ", 4) != 0) {
        SDL_free(f);
        return 0;
    }
    memcpy(&h, f + 12, 4);
    memcpy(&w, f + 16, 4);
    memcpy(&mips, f + 28, 4);
    memcpy(&flags, f + 80, 4);
    memcpy(&fourcc, f + 84, 4);
    memcpy(&bpp, f + 88, 4);
    if (w == 0 || h == 0 || w > 8192 || h > 8192) {
        SDL_free(f);
        return 0;
    }
    if (mips == 0) {
        mips = 1;
    }
    if ((flags & 4) && fourcc == 0x31545844) { img->format = 1; blk = 8; }        /* DXT1 */
    else if ((flags & 4) && fourcc == 0x33545844) { img->format = 2; blk = 16; }  /* DXT3 */
    else if ((flags & 4) && fourcc == 0x35545844) { img->format = 3; blk = 16; }  /* DXT5 */
    else if (!(flags & 4) && bpp == 32) { img->format = 0; blk = 0; }
    else {
        SDL_free(f);
        return 0;
    }
    img->file = f;
    img->w = w;
    img->h = h;
    for (level = 0; level < mips && level < TEXPACK_MAX_LEVELS; level++) {
        uint32_t lw = w >> level ? w >> level : 1, lh = h >> level ? h >> level : 1;
        uint32_t bytes = blk ? ((lw + 3) / 4) * ((lh + 3) / 4) * blk : lw * lh * 4;
        if (at + bytes > size) {
            break;
        }
        if (blk == 0) { /* plain pixels: B, G, R, A in the file unless the red mask says otherwise */
            uint32_t rmask, i;
            memcpy(&rmask, f + 92, 4);
            if (rmask != 0x000000FF) {
                for (i = 0; i < lw * lh; i++) {
                    uint8_t t = f[at + i * 4];
                    f[at + i * 4] = f[at + i * 4 + 2];
                    f[at + i * 4 + 2] = t;
                }
            }
        }
        img->data[level] = f + at;
        img->bytes[level] = bytes;
        at += bytes;
        if (lw == 1 && lh == 1) {
            level++;
            break;
        }
    }
    img->levels = (int)level;
    if (img->levels == 0) {
        SDL_free(f);
        img->file = NULL;
        return 0;
    }
    return 1;
}

static void image_free(TexPackImage *img) {
    SDL_free(img->file);
    img->file = NULL;
}

int TexPack_Load(const char *path, TexPackImage *img) {
    return load(path, img);
}

void TexPack_Free(TexPackImage *img) {
    image_free(img);
}

/* A file read for this GPU, on the reader's thread: on Android its ASTC copy if gs_astc.cpp made one (and one asked
   for if not), and on a GPU without DXT the DXT unpacked to plain pixels here, not in the frame (gs_draw.c). */
extern int gGsNoBcTextures, gGsAstcTextures;
#ifdef __ANDROID__
extern int GsAstc_Load(const char *src, TexPackImage *img);
extern void GsAstc_Queue(const char *src);
#endif
static int load_for_gpu(const char *path, TexPackImage *img) {
#ifdef __ANDROID__
    const uint32_t maxSide = 2048; /* (levels above it left out: memory) */
    if (GsAstc_Load(path, img)) {
        return 1;
    }
#else
    const uint32_t maxSide = 16384;
#endif
    if (!load(path, img)) {
        return 0;
    }
#ifdef __ANDROID__
    if (gGsAstcTextures && (img->format == 0 || gGsNoBcTextures)) {
        GsAstc_Queue(path); /* an ASTC copy for the next time, made in the background */
    }
#endif
    if (img->format != 0 && gGsNoBcTextures && !TexPack_Decompress(img, maxSide)) {
        image_free(img);
        return 0;
    }
    return 1;
}

static size_t image_bytes(const TexPackImage *img) {
    size_t n = 0;
    int level;
    for (level = 0; level < img->levels; level++) {
        n += img->bytes[level];
    }
    return n;
}

/* Called with sLock held: a file read, into its entry */
static void store(uint32_t id, int ok, const TexPackImage *img) {
    Entry *e = ent(id);
    if (!ok) {
        e->state = 3;
        return;
    }
    e->img = *img;
    e->held = image_bytes(img);
    e->state = 2;
    sHeld[sHeldCount++] = id;
    sHeldBytes += e->held;
}

static int SDLCALL reader(void *unused) {
    (void)unused;
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    for (;;) {
        uint32_t id;
        TexPackImage img;
        int ok;
        SDL_LockMutex(sLock);
        while (sHead == sTail) {
            SDL_WaitCondition(sWake, sLock);
        }
        id = sQueue[sTail++ & (QUEUE - 1)];
        SDL_UnlockMutex(sLock);
        ok = load_for_gpu(ent(id)->path, &img);
        SDL_LockMutex(sLock);
        store(id, ok, &img);
        SDL_UnlockMutex(sLock);
    }
    return 0;
}

/* Over the budget: the files asked for longest ago leave memory, down to three quarters of it (not `keep`, the one
   just asked for). Only this thread frees them; the reader only adds. Called with sLock held. */
static void trim(uint32_t keep) {
    while (sHeldBytes > sBudget / 4 * 3 && sHeldCount > 1) {
        uint32_t i, old = sHeldCount;
        for (i = 0; i < sHeldCount; i++) {
            if (sHeld[i] != keep && (old == sHeldCount || ent(sHeld[i])->used < ent(sHeld[old])->used)) {
                old = i;
            }
        }
        if (old == sHeldCount) {
            return;
        }
        {
            Entry *e = ent(sHeld[old]);
            image_free(&e->img);
            sHeldBytes -= e->held;
            e->held = 0;
            e->state = 0;
            sHeld[old] = sHeld[--sHeldCount];
        }
    }
}

int TexPack_Get(int id, TexPackImage *img) {
    Entry *e;
    int r;

    if (id < 0 || (id < HD_BASE && (uint32_t)id >= sCount) || (id >= HD_BASE && id >= HD_BASE + sHdN)) {
        return -1;
    }
    if (sSync < 0) {
        const char *mb = getenv("BT3_PACK_MB");
        sSync = getenv("BT3_PACK_SYNC") != NULL && atoi(getenv("BT3_PACK_SYNC")) != 0;
        sBudget = mb != NULL && atoi(mb) > 0 ? (size_t)atoi(mb) << 20 : TEXPACK_BUDGET;
        sHeld = malloc((sCount + HD_MAX) * sizeof(uint32_t));
        sLock = SDL_CreateMutex();
        sWake = SDL_CreateCondition();
        if (!sSync && sLock != NULL && sWake != NULL) {
            sReader = SDL_CreateThread(reader, "bt3-texpack", NULL);
        }
        if (sReader == NULL) {
            sSync = 1;
        }
    }
    e = ent((uint32_t)id);
    SDL_LockMutex(sLock);
    e->used = ++sTick;
    if (e->state == 0 && sSync) {
        TexPackImage read;
        int ok = load_for_gpu(e->path, &read);
        store((uint32_t)id, ok, &read);
    } else if (e->state == 0 && sHead - sTail < QUEUE) {
        sQueue[sHead++ & (QUEUE - 1)] = (uint32_t)id;
        e->state = 1;
        SDL_SignalCondition(sWake);
    }
    r = e->state == 2 ? 1 : e->state == 3 ? -1 : 0;
    if (r == 1) {
        *img = e->img;
        trim((uint32_t)id);
    }
    SDL_UnlockMutex(sLock);
    return r;
}
