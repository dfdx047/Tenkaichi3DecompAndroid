/*
 * The gameplay fingerprint: one number for the data a fight is computed from.
 *
 * Online play exchanges button presses only, so both games must compute the same thing; a player whose disc has
 * other character stats computes another fight. The fingerprint says whether two data folders agree in what the
 * game COMPUTES with, and in nothing else: pictures of any kind (HUD, portraits, menu art, character skins,
 * stage and effect textures), music, voices, texture packs and added songs do not enter it.
 *
 * What is hashed, of which files and in which order, is defined in docs/port/README.md ("The gameplay
 * fingerprint") and written a second time in port/tools/make_fingerprint.py, which also makes the table of the
 * original disc's values (plat_fprint_tab.h). In short:
 *   - the CONTENTS of the entries of the pack files, never their offsets: a picture replaced by a larger one
 *     moves every entry behind it and leaves the fingerprint alone;
 *   - a block is an "item", hashed with SHA-256 (64 bits kept); a file is a list of items; a component (tables,
 *     characters, animations, stages, effects, scripts, menus) is the hash of its files' lists; the fingerprint
 *     is the hash of the seven components;
 *   - what is hashed is what the game would LOAD: a file in <root>/mods/ in place of the one it replaces
 *     (Port_FileOpenHost, plat_fcache.c: the loader's own function).
 *
 * It runs on the data check's thread (plat_verify.c), is printed in the log, and is kept per file in
 * <root>/.fingerprint with the file's size and time, so a later start only looks at those. Nothing is enforced
 * here and nothing is sent anywhere: Port_DataFingerprint() is what later steps ask.
 * (Its variables are not part of the game's saved state: port/tools/make_state.py lists the files that are.)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "plat_fprint_tab.h"

extern const char *Port_FileRoot(void);                              /* plat_file.c */
extern FILE *Port_FileOpenHost(const char *rel, char *path, size_t n); /* plat_fcache.c */
extern int PortStages_Count(void);                                   /* plat_stages.c */

#define FP_VERSION 1 /* the definition's version: part of the hash, of the table and of the cache */
#if FP_TAB_VERSION != FP_VERSION
#error "plat_fprint_tab.h was made for another definition: run port/tools/make_fingerprint.py --write"
#endif

enum { C_TABLES, C_CHARS, C_ANIMS, C_STAGES, C_EFFECTS, C_SCRIPTS, C_MENUS, COMPS };
static const char *const kCompName[COMPS] = { "tables", "characters", "animations", "stages", "effects", "scripts", "menus" };
enum { K_TABLES, K_MODEL, K_ANIM, K_STAGE, K_TRANSITION, K_GENERIC, K_SCRIPT, K_MENU };
enum { CHARS = 161, STAGES = 35, MAX_DEPTH = 4, MAX_ENTRIES = 4096 };
enum { I_LIPS = 0x100, I_ST_NODES = 0x101, I_ST_COLL = 0x102, I_ST_BOUNDS = 0x103, I_ST_OBJS = 0x104, I_WHOLE = 0xFFFF };

/* ---- SHA-256 ---------------------------------------------------------------------------------------------------- */

typedef struct Sha {
    uint32_t h[8];
    uint64_t len;
    uint8_t buf[64];
    unsigned n;
} Sha;

static const uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_block(Sha *s, const uint8_t *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    }
    for (; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + kK[i] + w[i];
        t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha_init(Sha *s) {
    static const uint32_t init[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(s->h, init, sizeof(init));
    s->len = 0;
    s->n = 0;
}

static void sha_put(Sha *s, const void *data, size_t n) {
    const uint8_t *p = data;

    s->len += n;
    if (s->n != 0) {
        size_t k = 64 - s->n < n ? 64 - s->n : n;
        memcpy(s->buf + s->n, p, k);
        s->n += (unsigned)k;
        p += k;
        n -= k;
        if (s->n < 64) {
            return;
        }
        sha_block(s, s->buf);
        s->n = 0;
    }
    for (; n >= 64; p += 64, n -= 64) {
        sha_block(s, p);
    }
    memcpy(s->buf, p, n);
    s->n = (unsigned)n;
}

/* The first 64 bits of the hash, as a number (big-endian, as the hash is printed). */
static uint64_t sha_end(Sha *s) {
    uint64_t bits = s->len * 8;
    uint8_t pad[72];
    size_t k = (s->n < 56 ? 56 : 120) - s->n;
    int i;

    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    for (i = 0; i < 8; i++) {
        pad[k + i] = (uint8_t)(bits >> (56 - i * 8));
    }
    sha_put(s, pad, k + 8);
    return (uint64_t)s->h[0] << 32 | s->h[1];
}

static void put8(Sha *s, int c) {
    uint8_t b = (uint8_t)c;
    sha_put(s, &b, 1);
}
static void put32(Sha *s, uint32_t x) {
    uint8_t b[4] = { (uint8_t)x, (uint8_t)(x >> 8), (uint8_t)(x >> 16), (uint8_t)(x >> 24) };
    sha_put(s, b, 4);
}
static void put64be(Sha *s, uint64_t x) {
    uint8_t b[8];
    int i;
    for (i = 0; i < 8; i++) {
        b[i] = (uint8_t)(x >> (56 - i * 8));
    }
    sha_put(s, b, 8);
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int32_t rds32(const uint8_t *p) { return (int32_t)rd32(p); }
static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }

/* ---- the three shapes ------------------------------------------------------------------------------------------- */

/* A pack: word 0 = count N, words 1..N+1 = byte offsets; entry k (1..N) is [w[k] & ~3, w[k+1] & ~3). Returns N, or
   0 when the block is not one. `strict` is for a block only GUESSED to be a pack: the bytes outside its entries
   must then be zero, so that taking something else for a pack cannot hide data. */
static uint32_t pack_count(const uint8_t *d, size_t n, int strict) {
    uint64_t prev, x;
    uint32_t N, i;
    size_t k;

    if (n < 12) {
        return 0;
    }
    N = rd32(d);
    if (N < 1 || N > MAX_ENTRIES || 4 * ((uint64_t)N + 2) > n) {
        return 0;
    }
    prev = 4 * ((uint64_t)N + 2);
    for (i = 0; i <= N; i++) {
        x = rd32(d + 4 + 4 * i) & ~3u;
        if (x < prev || x > n) {
            return 0;
        }
        prev = x;
    }
    if (strict) {
        for (k = 4 * ((size_t)N + 2); k < (rd32(d + 4) & ~3u); k++) {
            if (d[k] != 0) {
                return 0;
            }
        }
        for (k = (size_t)prev; k < n; k++) {
            if (d[k] != 0) {
                return 0;
            }
        }
    }
    return N;
}
/* A FILE the game reads as a pack. The game takes entry k at w[k] & ~3 and calls it empty when w[k+1] == w[k];
   that is all it asks of the table, so nothing more is asked here (packs rebuilt by modding tools have offsets
   that point past the end, or a last word that is zero): an entry runs from its offset to the next larger offset
   of the table, or to the end of the file. */
typedef struct Pack {
    const uint8_t *d;
    size_t n;
    uint32_t N;
    int starts;
    uint32_t start[MAX_ENTRIES + 2]; /* the offsets inside the file, in order, without repeats; then n */
} Pack;
enum { ENT_EMPTY, ENT_DATA, ENT_OUT };

static int u32_cmp(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

static int pack_open(Pack *p, const uint8_t *d, size_t n) {
    uint32_t i;
    int k, m = 0;

    p->d = d;
    p->n = n;
    p->N = 0;
    if (n < 12 || n > 0xFFFFFFFFu) {
        return 0;
    }
    p->N = rd32(d);
    if (p->N < 1 || p->N > MAX_ENTRIES || 4 * ((uint64_t)p->N + 2) > n) {
        p->N = 0;
        return 0;
    }
    for (i = 0; i <= p->N; i++) {
        uint32_t o = rd32(d + 4 + 4 * i) & ~3u;
        if (o < n) {
            p->start[m++] = o;
        }
    }
    qsort(p->start, (size_t)m, sizeof(uint32_t), u32_cmp);
    for (i = 0, k = 0; i < (uint32_t)m; i++) {
        if (k == 0 || p->start[k - 1] != p->start[i]) {
            p->start[k++] = p->start[i];
        }
    }
    p->start[k] = (uint32_t)n;
    p->starts = k + 1;
    return 1;
}

/* Entry k (1..N): ENT_EMPTY, ENT_OUT (its offset lies outside the file) or ENT_DATA with its bytes. */
static int pack_entry(const Pack *p, uint32_t k, const uint8_t **e, size_t *len) {
    uint32_t w = rd32(p->d + 4 * k), o = w & ~3u;
    int lo = 0, hi = p->starts;

    *e = p->d;
    *len = 0;
    if (rd32(p->d + 4 * k + 4) == w) {
        return ENT_EMPTY;
    }
    if (o >= p->n) {
        return ENT_OUT;
    }
    while (lo < hi) { /* the first start larger than o */
        int mid = (lo + hi) / 2;
        if (p->start[mid] > o) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    *e = p->d + o;
    *len = p->start[lo] - o;
    return ENT_DATA;
}

#define ENT_START(d, k) ((size_t)(rd32((d) + 4 * (k)) & ~3u))
#define ENT_END(d, k) ((size_t)(rd32((d) + 4 * (k) + 4) & ~3u))
#define ENT_FLAG(d, k) ((int)(rd32((d) + 4 * (k)) & 3))

/* A texture file (what Res_RelocateOffsets takes): count, entry table offset in words, 0x40-byte entries with the
   pixel and palette packets' offsets (words) and sizes; each packet starts with a VIF DIRECT code that spans it. */
static int is_tex(const uint8_t *d, size_t n) {
    uint64_t cnt, eo, lo, i;
    int k;

    if (n < 0x60) {
        return 0;
    }
    cnt = rd32(d);
    eo = rd32(d + 4);
    if (cnt < 1 || cnt > 1024 || eo < 4 || eo > 0x1000 || eo * 4 + cnt * 0x40 > n) {
        return 0;
    }
    lo = eo * 4 + cnt * 0x40;
    for (i = 0; i < cnt; i++) {
        const uint8_t *e = d + eo * 4 + i * 0x40;
        if (rd32(e) == 0 && rd32(e + 4) == 0) {
            return 0;
        }
        for (k = 0; k < 2; k++) {
            uint64_t o = rd32(e + k * 4);
            int64_t s = rds32(e + 8 + k * 4);
            const uint8_t *p;
            if (o == 0) {
                continue;
            }
            if (o > 0x4000000 || s < 0x20 || s % 16 != 0 || o * 4 < lo || o * 4 + (uint64_t)s > n) {
                return 0;
            }
            p = d + o * 4;
            if (rd32(p) != 0 || rd32(p + 4) != 0 || rd32(p + 8) != 0 || (rd32(p + 12) >> 24) != 0x50 ||
                (rd32(p + 12) & 0xFFFF) != (uint32_t)(s / 16 - 1)) {
                return 0;
            }
        }
    }
    return 1;
}

static void put_out(Sha *s) { put8(s, 'X'); }

static void put_data(Sha *s, const uint8_t *d, size_t n) {
    if (n == 0) {
        put8(s, 'E');
        return;
    }
    put8(s, 'D');
    put32(s, (uint32_t)n);
    sha_put(s, d, n);
}

/* The generic walk: an empty block, a texture file (its content does not count), a pack of blocks, or data. */
static void put_generic(Sha *s, const uint8_t *d, size_t n, int depth) {
    uint32_t N, k;

    if (n == 0) {
        put8(s, 'E');
        return;
    }
    if (is_tex(d, n)) {
        put8(s, 'T');
        return;
    }
    if (depth < MAX_DEPTH && (N = pack_count(d, n, 1)) != 0) {
        put8(s, 'P');
        put32(s, N);
        for (k = 1; k <= N; k++) {
            put8(s, ENT_FLAG(d, k));
            put_generic(s, d + ENT_START(d, k), ENT_END(d, k) - ENT_START(d, k), depth + 1);
        }
        return;
    }
    put_data(s, d, n);
}

/* ---- items ------------------------------------------------------------------------------------------------------ */

typedef struct Item {
    uint32_t id;
    uint64_t hash;
} Item;
enum { MAX_ITEMS = MAX_ENTRIES + 8 };
static Item sItems[MAX_ITEMS];
static int sItemCount;

static void item_add(uint32_t id, Sha *s) {
    if (sItemCount < MAX_ITEMS) {
        sItems[sItemCount].id = id;
        sItems[sItemCount].hash = sha_end(s);
        sItemCount++;
    }
}

static void item_whole(const uint8_t *d, size_t n) {
    Sha s;
    sha_init(&s);
    put8(&s, 'W');
    put32(&s, (uint32_t)n);
    sha_put(&s, d, n);
    item_add(I_WHOLE, &s);
}

static void item_header(const Pack *p) {
    Sha s;
    uint32_t k;
    sha_init(&s);
    put8(&s, 'H');
    put32(&s, p->N);
    for (k = 1; k <= p->N; k++) {
        put8(&s, ENT_FLAG(p->d, k));
    }
    item_add(0, &s);
}

/* An entry as data: 'X' when it lies outside the file, 'E' when empty. */
static void put_entry(Sha *s, int kind, const uint8_t *e, size_t len) {
    if (kind == ENT_OUT) {
        put_out(s);
    } else {
        put_data(s, e, len);
    }
}

static void item_data(uint32_t id, int kind, const uint8_t *e, size_t len) {
    Sha s;
    sha_init(&s);
    put_entry(&s, kind, e, len);
    item_add(id, &s);
}

static void item_generic(uint32_t id, int kind, const uint8_t *e, size_t len) {
    Sha s;
    sha_init(&s);
    if (kind == ENT_OUT) {
        put_out(&s);
    } else {
        put_generic(&s, e, len, 1);
    }
    item_add(id, &s);
}

/* Common file 2 (fighter tables, AI data, the lip tracks objects share) and common file 4 (character and item
   tables): every entry; entry 1 of file 2 is the fighters' light texture. */
static void items_tables(const uint8_t *d, size_t n, int fid) {
    Pack p;
    uint32_t k;
    Sha lips;

    if (!pack_open(&p, d, n)) {
        item_whole(d, n);
        return;
    }
    item_header(&p);
    sha_init(&lips);
    put8(&lips, 'L');
    for (k = 1; k <= p.N; k++) {
        const uint8_t *e;
        size_t len;
        int kind = pack_entry(&p, k, &e, &len);
        if (fid == 2 && k == 1) {
            Sha s;
            sha_init(&s);
            if (kind == ENT_DATA && is_tex(e, len)) {
                put8(&s, 'T');
            } else {
                put_entry(&s, kind, e, len);
            }
            item_add(k, &s);
        } else if (fid == 2 && k >= 14 && k <= 213) {
            put_entry(&lips, kind, e, len);
        } else {
            item_data(k, kind, e, len);
        }
    }
    if (fid == 2) {
        item_add(I_LIPS, &lips);
    }
}

/* Entry 3 of a model file ("pmdl"): the header's first 0x28 bytes and, of every record of the node chain, what
   places the node (bytes 4..0x40) and its bounds (0x40..0x60, in records longer than 0x40). Not the records'
   lengths (they grow with the mesh), the vertex streams behind them, the texture slots or the seam section.
   Returns 0 when the entry does not have the shape. */
static int put_skeleton(Sha *s, const uint8_t *e, size_t n) {
    int pass, count = 0;

    if (n < 0x70 || memcmp(e, "pmdl", 4) != 0) {
        return 0;
    }
    for (pass = 0; pass < 2; pass++) {
        int64_t o = rd32(e + 0x6C);
        int i, done = 0;
        if (pass == 1) {
            put8(s, 'M');
            sha_put(s, e, 0x28);
            put32(s, (uint32_t)count);
        }
        for (i = 0; i < 4096 && !done; i++) {
            int32_t next;
            uint32_t last;
            if (o < 0x70 || o + 0x40 > (int64_t)n) {
                return 0;
            }
            next = rds32(e + o);
            last = rd16(e + o + 6);
            if (next > 0x40) {
                if (next < 0x60 || o + 0x60 > (int64_t)n) {
                    return 0;
                }
                if (pass == 1) {
                    put8(s, 1);
                    sha_put(s, e + o + 4, 0x5C);
                }
            } else if (next == 0x40 || (last != 0 && next >= 0)) {
                if (pass == 1) {
                    put8(s, 0);
                    sha_put(s, e + o + 4, 0x3C);
                }
            } else {
                return 0;
            }
            if (pass == 0) {
                count++;
            }
            if (last != 0) {
                done = 1;
            }
            o += next;
        }
        if (!done) {
            return 0;
        }
    }
    return 1;
}

/* A character's (or partner object's) model file. Counted: 2 body volumes, 3 skeleton, 0x12..0x2C the parameter
   tables, the motions kept in the model file and the camera animations, 0x35..0xFC the lip tracks (the face code
   draws from the generator the fight uses). Not counted: 1 HUD face, 4..0x11 mouth / body / eye textures,
   0x2D..0x34 skill names. */
static void items_model(const uint8_t *d, size_t n) {
    Pack p;
    uint32_t k;
    Sha lips;

    if (!pack_open(&p, d, n)) {
        item_whole(d, n);
        return;
    }
    item_header(&p);
    sha_init(&lips);
    put8(&lips, 'L');
    for (k = 1; k <= p.N; k++) {
        const uint8_t *e;
        size_t len;
        int kind = pack_entry(&p, k, &e, &len);
        if (k == 3) {
            Sha s;
            sha_init(&s);
            if (kind != ENT_DATA || !put_skeleton(&s, e, len)) {
                sha_init(&s);
                put_entry(&s, kind, e, len);
            }
            item_add(3, &s);
        } else if (k == 2 || (k >= 0x12 && k <= 0x2C)) {
            item_data(k, kind, e, len);
        } else if (k >= 0x35 && k <= 0xFC) {
            put_entry(&lips, kind, e, len);
        }
    }
    item_add(I_LIPS, &lips);
}

/* A character's animation file: every motion (frame counts, root motion, hit windows and hit shapes). */
static void items_anim(const uint8_t *d, size_t n) {
    Pack p;
    uint32_t k;
    Sha s;

    if (!pack_open(&p, d, n)) {
        item_whole(d, n);
        return;
    }
    sha_init(&s);
    put8(&s, 'P');
    put32(&s, p.N);
    for (k = 1; k <= p.N; k++) {
        const uint8_t *e;
        size_t len;
        int kind = pack_entry(&p, k, &e, &len);
        put8(&s, ENT_FLAG(d, k));
        put_entry(&s, kind, e, len);
    }
    item_add(0, &s);
}

/* Effect packs (common file 3, a character's effect file): every entry by the generic walk. */
static void items_generic(const uint8_t *d, size_t n) {
    Pack p;
    uint32_t k;

    if (!pack_open(&p, d, n)) {
        item_whole(d, n);
        return;
    }
    item_header(&p);
    for (k = 1; k <= p.N; k++) {
        const uint8_t *e;
        size_t len;
        int kind = pack_entry(&p, k, &e, &len);
        item_generic(k, kind, e, len);
    }
}

/* Entry 1 of a stage file is the stage object (BtlStage; relocated by BtlStage_Relocate, src/sys/gfxm_e_d.c):
   a header of counts and offsets, every offset in WORDS from the entry's start. The stage's texture file lies in
   the middle of the entry, the collision behind it, so nothing here may say where a table is. Four items:
     nodes       the timers and the transform nodes (with the debris keys)
     collision   the zones with their neighbours, each zone's collision mesh and the meshes of its objects
     bounds      radius, top, bottom
     objects     the destructible objects with the position, radius and node of their parts
   Left out: the draw groups, materials, meshes, the visibility table, the culling tree, the texture file. */
typedef struct StageWalk {
    const uint8_t *e;
    size_t n;
    int bad;
    int64_t nodes;
    uint32_t nodeCount;
} StageWalk;

/* Byte position of `size` bytes at word offset `off`; marks the walk bad when they are not inside the entry. */
static size_t st_at(StageWalk *w, int64_t off, int64_t size) {
    if (off < 0 || size < 0 || off * 4 + size > (int64_t)w->n) {
        w->bad = 1;
        return 0;
    }
    return (size_t)(off * 4);
}

static void st_mesh(StageWalk *w, Sha *s, int32_t off) {
    const uint8_t *e = w->e;
    size_t m, no, po, vo;
    int32_t nodeN, polyN, top = -1, i, k;

    if (off == 0) {
        put8(s, 0);
        return;
    }
    m = st_at(w, off, 0x18);
    if (w->bad) {
        return;
    }
    nodeN = rds32(e + m);
    polyN = rds32(e + m + 4);
    if (nodeN < 0 || polyN < 0 || nodeN > 0x100000 || polyN > 0x100000) {
        w->bad = 1;
        return;
    }
    no = st_at(w, rds32(e + m + 0xC), (int64_t)nodeN * 0x20);
    po = st_at(w, rds32(e + m + 0x10), (int64_t)polyN * 0x20);
    if (w->bad) {
        return;
    }
    for (i = 0; i < polyN; i++) {
        for (k = 0; k < 3; k++) {
            int32_t v = rds32(e + po + (size_t)i * 0x20 + 4 + k * 4);
            if (v < 0) {
                w->bad = 1;
                return;
            }
            if (v > top) {
                top = v;
            }
        }
    }
    vo = st_at(w, rds32(e + m + 0x14), ((int64_t)top + 1) * 0x10);
    if (w->bad) {
        return;
    }
    put8(s, 1);
    sha_put(s, e + m, 0xC);
    sha_put(s, e + no, (size_t)nodeN * 0x20);
    sha_put(s, e + po, (size_t)polyN * 0x20);
    put32(s, (uint32_t)(top + 1));
    sha_put(s, e + vo, ((size_t)top + 1) * 0x10);
}

static void st_part(StageWalk *w, Sha *s, int32_t off) {
    const uint8_t *e = w->e;
    size_t p;
    int32_t node, index = -1;

    if (off == 0) {
        put8(s, 0);
        return;
    }
    p = st_at(w, off, 0x20);
    if (w->bad) {
        return;
    }
    node = rds32(e + p + 8);
    if (node != 0) {
        int64_t rel = ((int64_t)node - w->nodes) * 4;
        if (rel < 0 || rel % 0x50 != 0 || rel / 0x50 >= (int64_t)w->nodeCount) {
            w->bad = 1;
            return;
        }
        index = (int32_t)(rel / 0x50);
    }
    put8(s, 1);
    sha_put(s, e + p + 2, 2);
    put32(s, (uint32_t)index);
    sha_put(s, e + p + 0xC, 0x14);
}

/* Returns 0 (and adds nothing) when the entry does not have the shape. */
static int items_stage_object(const uint8_t *e, size_t n) {
    StageWalk w;
    Sha sn, sc, sb, so;
    uint32_t timerCount, zoneCount, objCount, i;
    int32_t timers, zones, bounds, objs, k;
    size_t t, nb, zb, b, ob;

    if (n < 0x60) {
        return 0;
    }
    w.e = e;
    w.n = n;
    w.bad = 0;
    timerCount = rd32(e + 0x10);
    timers = rds32(e + 0x14);
    w.nodeCount = rd32(e + 0x30);
    w.nodes = rds32(e + 0x34);
    zoneCount = rd32(e + 0x38);
    zones = rds32(e + 0x3C);
    bounds = rds32(e + 0x40);
    objCount = rd32(e + 0x44);
    objs = rds32(e + 0x48);
    if (timerCount > 0x10000 || w.nodeCount > 0x10000 || zoneCount > 0x10000 || objCount > 0x10000) {
        return 0;
    }

    sha_init(&sn);
    put8(&sn, 'N');
    put32(&sn, timerCount);
    t = st_at(&w, timers, (int64_t)timerCount * 0x10);
    if (w.bad) {
        return 0;
    }
    for (i = 0; i < timerCount; i++) {
        sha_put(&sn, e + t + i * 0x10, 4);
        sha_put(&sn, e + t + i * 0x10 + 8, 8);
    }
    put32(&sn, w.nodeCount);
    nb = st_at(&w, w.nodes, (int64_t)w.nodeCount * 0x50);
    if (w.bad) {
        return 0;
    }
    for (i = 0; i < w.nodeCount; i++) {
        const uint8_t *o = e + nb + (size_t)i * 0x50;
        int32_t keyCount = rds32(o + 0x40), keys = rds32(o + 0x44);
        sha_put(&sn, o, 0x44);
        sha_put(&sn, o + 0x48, 8);
        if (keyCount > 0 && keys != 0) {
            size_t kp;
            if (keyCount > 0x10000) {
                return 0;
            }
            kp = st_at(&w, keys, (int64_t)keyCount * 0x20);
            if (w.bad) {
                return 0;
            }
            sha_put(&sn, e + kp, (size_t)keyCount * 0x20);
        }
    }

    sha_init(&sc);
    put8(&sc, 'C');
    put32(&sc, zoneCount);
    zb = st_at(&w, zones, (int64_t)zoneCount * 0x40);
    if (w.bad) {
        return 0;
    }
    for (i = 0; i < zoneCount; i++) {
        const uint8_t *o = e + zb + (size_t)i * 0x40;
        int32_t nearCount = rds32(o + 8), near = rds32(o + 0xC), recCount = rds32(o + 0x14), recs = rds32(o + 0x18);
        if (nearCount < 0 || nearCount > 0x10000 || recCount < 0 || recCount > 0x10000) {
            return 0;
        }
        sha_put(&sc, o, 4);
        put32(&sc, (uint32_t)nearCount);
        sha_put(&sc, o + 0x10, 8);
        sha_put(&sc, o + 0x1C, 0x24);
        if (nearCount != 0) {
            size_t q = st_at(&w, near, (int64_t)nearCount * 4);
            if (w.bad) {
                return 0;
            }
            sha_put(&sc, e + q, (size_t)nearCount * 4);
        }
        st_mesh(&w, &sc, rds32(o + 4));
        if (w.bad) {
            return 0;
        }
        if (recCount != 0) {
            size_t r = st_at(&w, recs, (int64_t)recCount * 0x10);
            if (w.bad) {
                return 0;
            }
            for (k = 0; k < recCount; k++) {
                const uint8_t *rec = e + r + (size_t)k * 0x10;
                sha_put(&sc, rec, 8);
                sha_put(&sc, rec + 0xC, 4);
                st_mesh(&w, &sc, rds32(rec + 8));
                if (w.bad) {
                    return 0;
                }
            }
        }
    }

    b = st_at(&w, bounds, 12);
    if (w.bad) {
        return 0;
    }
    sha_init(&sb);
    put8(&sb, 'B');
    sha_put(&sb, e + b, 12);

    sha_init(&so);
    put8(&so, 'O');
    put32(&so, objCount);
    ob = st_at(&w, objs, (int64_t)objCount * 0x50);
    if (w.bad) {
        return 0;
    }
    for (i = 0; i < objCount; i++) {
        const uint8_t *o = e + ob + (size_t)i * 0x50;
        uint32_t pieceCount = rd32(o + 0x24), p;
        int32_t pieces = rds32(o + 0x28);
        sha_put(&so, o, 0xC);
        for (k = 0; k < 6; k++) {
            st_part(&w, &so, rds32(o + 0xC + k * 4));
        }
        if (w.bad || pieceCount > 0x10000) {
            return 0;
        }
        put32(&so, pieceCount);
        if (pieceCount != 0) {
            size_t q = st_at(&w, pieces, (int64_t)pieceCount * 4);
            if (w.bad) {
                return 0;
            }
            for (p = 0; p < pieceCount; p++) {
                st_part(&w, &so, rds32(e + q + (size_t)p * 4));
            }
            if (w.bad) {
                return 0;
            }
        }
        sha_put(&so, o + 0x2C, 0x24);
    }
    item_add(I_ST_NODES, &sn);
    item_add(I_ST_COLL, &sc);
    item_add(I_ST_BOUNDS, &sb);
    item_add(I_ST_OBJS, &so);
    return 1;
}

/* A stage file: entry 1 as above; every other entry by the generic walk (the parameter block, the camera
   animations, the object animations, the AI's way points, the path table ... count; the texture files do not). */
static void items_stage(const uint8_t *d, size_t n) {
    Pack p;
    uint32_t k;

    if (!pack_open(&p, d, n)) {
        item_whole(d, n);
        return;
    }
    item_header(&p);
    for (k = 1; k <= p.N; k++) {
        const uint8_t *e;
        size_t len;
        int kind = pack_entry(&p, k, &e, &len);
        if (k == 1) {
            if (kind != ENT_DATA || !items_stage_object(e, len)) {
                item_data(1, kind, e, len);
            }
        } else {
            item_generic(k, kind, e, len);
        }
    }
}

/* The stage-change scene: entry 1 is its camera animation (the load waits for it); 2 is a model, 3 its textures. */
static void items_transition(const uint8_t *d, size_t n) {
    Pack p;
    const uint8_t *e;
    size_t len;
    int kind;

    if (!pack_open(&p, d, n)) {
        item_whole(d, n);
        return;
    }
    item_header(&p);
    kind = pack_entry(&p, 1, &e, &len);
    item_data(1, kind, e, len);
}

/* Philip Gage's byte pair "expand", as src/sys/bpe.c does it, with every read and write checked. Returns the
   unpacked data (to free) or NULL. */
static uint8_t *bpe_decode(const uint8_t *src, size_t len, size_t *outSize) {
    uint8_t left[256], right[256], stack[4100], *out;
    int32_t size, packed;
    size_t i, end, got = 0;

    if (len < 8) {
        return NULL;
    }
    size = rds32(src);
    packed = rds32(src + 4);
    if (size < 0 || size > 0x4000000 || packed < 0 || 8 + (size_t)packed > len) {
        return NULL;
    }
    out = malloc((size_t)size + 1);
    if (out == NULL) {
        return NULL;
    }
    i = 8;
    end = 8 + (size_t)packed;
#define NEED(k) if (i + (k) > len) goto fail
    while (i < end) {
        int count, c = 0, k, sp = 0;
        size_t block, stop;
        for (k = 0; k < 256; k++) {
            left[k] = (uint8_t)k;
            right[k] = 0;
        }
        NEED(1);
        count = src[i++];
        for (;;) {
            if (count > 127) {
                c += count - 127;
                count = 0;
            }
            if (c == 256) {
                break;
            }
            for (k = 0; k <= count; k++, c++) {
                if (c > 255) {
                    goto fail;
                }
                NEED(1);
                left[c] = src[i++];
                if (c != left[c]) {
                    NEED(1);
                    right[c] = src[i++];
                }
            }
            if (c == 256) {
                break;
            }
            NEED(1);
            count = src[i++];
        }
        NEED(2);
        block = ((size_t)src[i] << 8) + src[i + 1];
        i += 2;
        stop = i + block;
        if (stop > end) {
            goto fail;
        }
        for (;;) {
            if (sp != 0) {
                c = stack[--sp];
            } else if (i < stop) {
                c = src[i++];
            } else {
                break;
            }
            if (c == left[c]) {
                if (got >= (size_t)size) {
                    goto fail;
                }
                out[got++] = (uint8_t)c;
            } else {
                if (sp > 4096) {
                    goto fail;
                }
                stack[sp++] = right[c];
                stack[sp++] = left[c];
            }
        }
    }
#undef NEED
    if (got != (size_t)size) {
        goto fail;
    }
    *outSize = got;
    return out;
fail:
    free(out);
    return NULL;
}

/* The versus menus' archive: section 2 (the character select's and the team select's resources) unpacked, every
   entry by the generic walk: its tables (which character, stage, song a cursor position stands for) and its
   animations count, its pictures do not. */
static void items_menu(const uint8_t *d, size_t n) {
    static Pack p, q; /* (large; one thread) */
    const uint8_t *sec;
    uint8_t *raw;
    size_t len, rawLen = 0;
    uint32_t k;

    if (!pack_open(&p, d, n) || p.N < 2 || pack_entry(&p, 2, &sec, &len) == ENT_OUT) {
        item_whole(d, n);
        return;
    }
    raw = bpe_decode(sec, len, &rawLen);
    if (raw == NULL || !pack_open(&q, raw, rawLen)) {
        item_whole(sec, len);
    } else {
        item_header(&q);
        for (k = 1; k <= q.N; k++) {
            const uint8_t *e;
            size_t elen;
            int kind = pack_entry(&q, k, &e, &elen);
            item_generic(k, kind, e, elen);
        }
    }
    free(raw);
}

static void items_script(const uint8_t *d, size_t n) {
    Sha s;
    sha_init(&s);
    put8(&s, 'S');
    put32(&s, (uint32_t)n);
    sha_put(&s, d, n);
    item_add(0, &s);
}

static void items_of(int kind, int fid, const uint8_t *d, size_t n) {
    sItemCount = 0;
    switch (kind) {
    case K_TABLES: items_tables(d, n, fid); break;
    case K_MODEL: items_model(d, n); break;
    case K_ANIM: items_anim(d, n); break;
    case K_STAGE: items_stage(d, n); break;
    case K_TRANSITION: items_transition(d, n); break;
    case K_GENERIC: items_generic(d, n); break;
    case K_SCRIPT: items_script(d, n); break;
    default: items_menu(d, n); break;
    }
}

/* ---- the list of files, in the order they are hashed -------------------------------------------------------------- */

typedef struct FpFile {
    uint16_t fid;   /* the game's file id: pzs3us1/(fid - 1) */
    uint8_t comp, kind;
    /* what was hashed (the cache's record) */
    uint64_t size, mtime;
    uint32_t pathHash;
    int32_t count;  /* items; -1 = the file is missing; -2 = nothing known */
    uint32_t first; /* index into sAll */
} FpFile;

#define FILES ((int)(sizeof(kFpFile) / sizeof(kFpFile[0])))
static FpFile sFile[sizeof(kFpFile) / sizeof(kFpFile[0])];
static Item *sAll; /* every file's items, one after the other */
static uint32_t sAllCount, sAllCap;

static void list_build(void) {
    int n = 0, c, k, i;
#define ADD(co, id, ki) do { if (n < FILES) { sFile[n].fid = (uint16_t)(id); sFile[n].comp = (co); sFile[n].kind = (ki); sFile[n].count = -2; } n++; } while (0)
    ADD(C_TABLES, 2, K_TABLES);
    ADD(C_TABLES, 4, K_TABLES);
    for (c = 0; c < CHARS; c++) {
        for (k = 0; k < 8; k++) {
            ADD(C_CHARS, 0x590 + c * 10 + k, K_MODEL);
        }
    }
    for (i = 0xD1C; i < 0xD48; i++) {
        ADD(C_CHARS, i, K_MODEL);
    }
    for (c = 0; c < CHARS; c++) {
        ADD(C_ANIMS, 0x598 + c * 10, K_ANIM);
    }
    for (i = 0; i < STAGES; i++) {
        ADD(C_STAGES, 0x171 + i, K_STAGE);
    }
    for (i = 0; i < STAGES; i++) {
        ADD(C_STAGES, 0x198 + i, K_STAGE);
    }
    ADD(C_STAGES, 0x1BF, K_TRANSITION);
    ADD(C_STAGES, 0x1C0, K_TRANSITION);
    ADD(C_EFFECTS, 3, K_GENERIC);
    for (c = 0; c < CHARS; c++) {
        ADD(C_EFFECTS, 0x599 + c * 10, K_GENERIC);
    }
    for (i = 0x1FF; i < 0x231; i++) {
        ADD(C_SCRIPTS, i, K_SCRIPT);
    }
    ADD(C_MENUS, 0x1C1, K_MENU);
#undef ADD
    if (n != FILES) {
        fprintf(stderr, "bt3: gameplay data: the table has %d files, the list %d\n", FILES, n);
        abort();
    }
}

static uint64_t mtime_of(const struct stat *st) {
#if defined(__linux__)
    return (uint64_t)st->st_mtim.tv_sec * 1000000000ull + (uint64_t)st->st_mtim.tv_nsec;
#else
    return (uint64_t)st->st_mtime;
#endif
}

static uint32_t str_hash(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s != '\0'; s++) {
        h = (h ^ (uint8_t)*s) * 16777619u;
    }
    return h;
}

static int all_reserve(uint32_t more) {
    if (sAllCount + more > sAllCap) {
        uint32_t cap = (sAllCount + more) * 2 + 1024;
        Item *p = realloc(sAll, cap * sizeof(Item));
        if (p == NULL) {
            return 0;
        }
        sAll = p;
        sAllCap = cap;
    }
    return 1;
}

/* ---- the cache: <root>/.fingerprint ------------------------------------------------------------------------------- */

static void w32(FILE *f, uint32_t x) {
    uint8_t b[4] = { (uint8_t)x, (uint8_t)(x >> 8), (uint8_t)(x >> 16), (uint8_t)(x >> 24) };
    fwrite(b, 1, 4, f);
}
static void w64(FILE *f, uint64_t x) {
    w32(f, (uint32_t)x);
    w32(f, (uint32_t)(x >> 32));
}
static int r32(FILE *f, uint32_t *x) {
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) {
        return 0;
    }
    *x = rd32(b);
    return 1;
}
static int r64(FILE *f, uint64_t *x) {
    uint32_t lo, hi;
    if (!r32(f, &lo) || !r32(f, &hi)) {
        return 0;
    }
    *x = (uint64_t)hi << 32 | lo;
    return 1;
}

static void cache_path(char *out, size_t n) {
    snprintf(out, n, "%s/.fingerprint", Port_FileRoot());
}

static void cache_load(void) {
    char path[700];
    uint32_t magic, ver, files, fid, count, k;
    FILE *f;
    int n;

    cache_path(path, sizeof(path));
    f = fopen(path, "rb");
    if (f == NULL) {
        return;
    }
    if (r32(f, &magic) && r32(f, &ver) && r32(f, &files) && magic == 0x46335442 && ver == FP_VERSION && files == (uint32_t)FILES) {
        for (n = 0; n < FILES; n++) {
            FpFile *x = &sFile[n];
            if (!r32(f, &fid) || fid != x->fid || !r64(f, &x->size) || !r64(f, &x->mtime) || !r32(f, &x->pathHash) || !r32(f, &count)) {
                break;
            }
            if (count == 0xFFFFFFFFu) { /* it was missing: looked for again */
                continue;
            }
            if (count > MAX_ITEMS || !all_reserve(count)) {
                break;
            }
            x->first = sAllCount;
            for (k = 0; k < count; k++) {
                if (!r32(f, &sAll[sAllCount + k].id) || !r64(f, &sAll[sAllCount + k].hash)) {
                    break;
                }
            }
            if (k != count) {
                break;
            }
            sAllCount += count;
            x->count = (int32_t)count;
        }
    }
    fclose(f);
}

static void cache_save(void) {
    char path[700], tmp[720];
    FILE *f;
    int n, k;

    cache_path(path, sizeof(path));
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "wb");
    if (f == NULL) {
        return; /* (a data folder that cannot be written: computed again next time) */
    }
    w32(f, 0x46335442);
    w32(f, FP_VERSION);
    w32(f, (uint32_t)FILES);
    for (n = 0; n < FILES; n++) {
        const FpFile *x = &sFile[n];
        w32(f, x->fid);
        w64(f, x->count < 0 ? 0 : x->size);
        w64(f, x->count < 0 ? 0 : x->mtime);
        w32(f, x->count < 0 ? 0 : x->pathHash);
        w32(f, x->count < 0 ? 0xFFFFFFFFu : (uint32_t)x->count);
        for (k = 0; k < x->count; k++) {
            w32(f, sAll[x->first + k].id);
            w64(f, sAll[x->first + k].hash);
        }
    }
    if (fclose(f) == 0) {
        remove(path);
        if (rename(tmp, path) != 0) {
            remove(tmp);
        }
    } else {
        remove(tmp);
    }
}

/* ---- the run ---------------------------------------------------------------------------------------------------- */

enum { FP_OFF = -1, FP_RUNNING = 0, FP_READY = 1 };
static volatile int sState = FP_OFF;
static uint64_t sMain, sComp[COMPS];
static int sOriginal;
static char sText[512];

/* 1: *out is the fingerprint. 0: not ready yet (it is being computed). -1: not computed in this run (no data
   check: BT3_GS=none without BT3_VERIFY=1). */
int Port_DataFingerprint(unsigned long long *out) {
    int state = sState;
    if (state == FP_READY && out != NULL) {
        *out = sMain;
    }
    return state;
}

/* The same for one part (0 tables, 1 characters, 2 animations, 3 stages, 4 effects, 5 scripts, 6 menus), and
   whether the whole is the original disc's (1), not (0). */
int Port_DataFingerprintPart(int part, unsigned long long *out) {
    if (sState != FP_READY || part < 0 || part >= COMPS) {
        return sState == FP_READY ? FP_OFF : sState;
    }
    *out = sComp[part];
    return FP_READY;
}
int Port_DataFingerprintOriginal(void) { return sState == FP_READY ? sOriginal : -1; }
/* The log's line without its "bt3: " (for a crash report, a lobby window); "" until it is known. */
const char *Port_DataFingerprintText(void) { return sState == FP_READY ? sText : ""; }

/* The file as the game reads it: in whole sectors, the rest of the last one zero (ADXF_ReadNw). */
#define SECTOR 2048
#define PADDED(size) (((size) + SECTOR - 1) / SECTOR * SECTOR)
static uint8_t *read_all(FILE *f, size_t size) {
    uint8_t *d = calloc(PADDED(size) + SECTOR, 1);
    if (d != NULL && fread(d, 1, size, f) != size) {
        free(d);
        d = NULL;
    }
    return d;
}

/* The items of file n as it would be loaded now: from the cache when the file is the one hashed last time. */
static int file_update(int n) {
    FpFile *x = &sFile[n];
    char rel[64], path[700];
    struct stat st;
    uint8_t *d;
    FILE *f;

    snprintf(rel, sizeof(rel), "pzs3us1/%05d.bin", x->fid - 1);
    f = Port_FileOpenHost(rel, path, sizeof(path));
    if (f == NULL || stat(path, &st) != 0) {
        if (f != NULL) {
            fclose(f);
        }
        x->count = -1;
        return 1;
    }
    if (x->count >= 0 && x->size == (uint64_t)st.st_size && x->mtime == mtime_of(&st) && x->pathHash == str_hash(path)) {
        fclose(f);
        return 0;
    }
    d = read_all(f, (size_t)st.st_size);
    fclose(f);
    if (d == NULL) {
        x->count = -1;
        return 1;
    }
    items_of(x->kind, x->fid, d, PADDED((size_t)st.st_size));
    free(d);
    if (!all_reserve((uint32_t)sItemCount)) {
        x->count = -1;
        return 1;
    }
    memcpy(&sAll[sAllCount], sItems, (size_t)sItemCount * sizeof(Item));
    x->first = sAllCount;
    x->count = sItemCount;
    sAllCount += (uint32_t)sItemCount;
    x->size = (uint64_t)st.st_size;
    x->mtime = mtime_of(&st);
    x->pathHash = str_hash(path);
    return 1;
}

static const char *item_name(const FpFile *x, uint32_t id, char *buf, size_t n) {
    if (id == I_WHOLE) {
        return "not readable as a pack";
    }
    if (id == I_LIPS) {
        return "lip tracks";
    }
    if (id == I_ST_NODES) {
        return "stage nodes";
    }
    if (id == I_ST_COLL) {
        return "stage collision";
    }
    if (id == I_ST_BOUNDS) {
        return "stage bounds";
    }
    if (id == I_ST_OBJS) {
        return "stage objects";
    }
    if (id == 0) {
        return x->kind == K_ANIM ? "motions" : x->kind == K_SCRIPT ? "script" : "entry count";
    }
    if (x->kind == K_MODEL && id == 3) {
        return "skeleton";
    }
    snprintf(buf, n, "entry 0x%X", (unsigned)id);
    return buf;
}

/* What file n is, for a reader of the log. */
static void file_name(const FpFile *x, char *out, size_t n) {
    int fid = x->fid;
    if (x->kind == K_MODEL && fid < 0xBDA) {
        snprintf(out, n, "character %d costume %d", (fid - 0x590) / 10, (fid - 0x590) % 10);
    } else if (x->kind == K_MODEL) {
        snprintf(out, n, "object %d", fid - 0xD1C);
    } else if (x->kind == K_ANIM) {
        snprintf(out, n, "character %d animations", (fid - 0x598) / 10);
    } else if (x->kind == K_GENERIC && fid != 3) {
        snprintf(out, n, "character %d effects", (fid - 0x599) / 10);
    } else if (x->kind == K_GENERIC) {
        snprintf(out, n, "common effects");
    } else if (x->kind == K_STAGE) {
        snprintf(out, n, fid >= 0x198 ? "stage %d, two players" : "stage %d", fid >= 0x198 ? fid - 0x198 : fid - 0x171);
    } else if (x->kind == K_TRANSITION) {
        snprintf(out, n, "stage change scene");
    } else if (x->kind == K_SCRIPT) {
        snprintf(out, n, "battle script %d", fid - 0x1FF);
    } else if (x->kind == K_MENU) {
        snprintf(out, n, "versus menus");
    } else {
        snprintf(out, n, fid == 2 ? "fighter tables" : "character and item tables");
    }
}

/* Whether file n's items are the table's; `what` gets the names of the first ones that are not. */
static int file_differs(int n, char *what, size_t size) {
    const FpFile *x = &sFile[n];
    const unsigned int *want = &kFpItem[kFpFile[n][1] * 2];
    int wantCount = (int)kFpFile[n][2], k, bad = 0;
    size_t used = 0;
    char buf[32];

    what[0] = '\0';
    if (x->count < 0) {
        snprintf(what, size, "missing");
        return 1;
    }
    for (k = 0; k < x->count || k < wantCount; k++) {
        const Item *it = k < x->count ? &sAll[x->first + k] : NULL;
        if (it != NULL && k < wantCount && want[k * 2] == it->id && want[k * 2 + 1] == (uint32_t)(it->hash >> 32)) {
            continue;
        }
        if (bad < 4 && used < size) {
            used += (size_t)snprintf(what + used, size - used, "%s%s", bad ? ", " : "",
                                     item_name(x, it != NULL ? it->id : want[k * 2], buf, sizeof(buf)));
        } else if (bad == 4 && used < size) {
            used += (size_t)snprintf(what + used, size - used, ", ...");
        }
        bad++;
    }
    return bad != 0;
}

/* Stages added from outside the disc (plat_stages.c) carry their own collision, so they are gameplay data; an
   online session does not offer them (the stage selects), so they are not part of the fingerprint. They are
   hashed the same way and printed, for a log that says what the game ran with. */
static void added_stages(void) {
    int count = PortStages_Count(), i, k, got = 0;
    Sha s;

    if (count <= 0) {
        return;
    }
    sha_init(&s);
    for (i = 0; i < count; i++) {
        char rel[64], path[700];
        struct stat st;
        uint8_t *d = NULL;
        FILE *f;

        snprintf(rel, sizeof(rel), "pzs3us2/%05d.bin", 70000 + i);
        f = Port_FileOpenHost(rel, path, sizeof(path));
        if (f != NULL && stat(path, &st) == 0) {
            d = read_all(f, (size_t)st.st_size);
        }
        if (f != NULL) {
            fclose(f);
        }
        put32(&s, (uint32_t)i);
        if (d == NULL) {
            put32(&s, 0xFFFFFFFFu);
            continue;
        }
        items_of(K_STAGE, 0, d, PADDED((size_t)st.st_size));
        free(d);
        put32(&s, (uint32_t)sItemCount);
        for (k = 0; k < sItemCount; k++) {
            put32(&s, sItems[k].id);
            put64be(&s, sItems[k].hash);
        }
        got++;
    }
    fprintf(stderr, "bt3: gameplay data: %d added stage%s %08x (not offered online, not counted)\n", got, got == 1 ? "" : "s",
            (unsigned)(sha_end(&s) >> 32));
}

/* Computes the fingerprint and prints it. `detail`: every differing file is named, not the first four. Called on
   the data check's thread, once. */
void PortFp_Run(int detail) {
    Sha comp[COMPS], all;
    int n, k, c, changed = 0, differ = 0, shown = 0;
    int compDiffers[COMPS];
    size_t used;
    struct timespec t0, t1;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    sState = FP_RUNNING;
    list_build();
    cache_load();
    for (n = 0; n < FILES; n++) {
        changed += file_update(n);
    }
    for (c = 0; c < COMPS; c++) {
        sha_init(&comp[c]);
    }
    for (n = 0; n < FILES; n++) {
        const FpFile *x = &sFile[n];
        Sha *s = &comp[x->comp];
        put32(s, x->fid);
        if (x->count < 0) {
            put32(s, 0xFFFFFFFFu);
            continue;
        }
        put32(s, (uint32_t)x->count);
        for (k = 0; k < x->count; k++) {
            put32(s, sAll[x->first + k].id);
            put64be(s, sAll[x->first + k].hash);
        }
    }
    sha_init(&all);
    sha_put(&all, "BT3FP", 5);
    put32(&all, FP_VERSION);
    for (c = 0; c < COMPS; c++) {
        sComp[c] = sha_end(&comp[c]);
        put64be(&all, sComp[c]);
        compDiffers[c] = sComp[c] != kFpComp[c];
    }
    sMain = sha_end(&all);
    sOriginal = sMain == kFpMain;
    if (changed) {
        cache_save();
    }

    used = (size_t)snprintf(sText, sizeof(sText), "gameplay data: %08x (", (unsigned)(sMain >> 32));
    for (c = 0; c < COMPS; c++) {
        used += (size_t)snprintf(sText + used, sizeof(sText) - used, "%s%s %08x", c ? " " : "", kCompName[c], (unsigned)(sComp[c] >> 32));
    }
    used += (size_t)snprintf(sText + used, sizeof(sText) - used, "): ");
    if (sOriginal) {
        snprintf(sText + used, sizeof(sText) - used, "the original disc's");
    } else {
        used += (size_t)snprintf(sText + used, sizeof(sText) - used, "modified:");
        for (c = 0, k = 0; c < COMPS; c++) {
            if (compDiffers[c] && used < sizeof(sText)) {
                used += (size_t)snprintf(sText + used, sizeof(sText) - used, "%s %s", k++ ? "," : "", kCompName[c]);
            }
        }
        if (used < sizeof(sText)) {
            snprintf(sText + used, sizeof(sText) - used, " differ%s", k == 1 ? "s" : "");
        }
    }
    fprintf(stderr, "bt3: %s\n", sText);
    if (!sOriginal) {
        for (n = 0; n < FILES; n++) {
            char what[160], name[64];
            if (!file_differs(n, what, sizeof(what))) {
                continue;
            }
            differ++;
            if (detail || shown < 4) {
                file_name(&sFile[n], name, sizeof(name));
                fprintf(stderr, "bt3: gameplay data:   %s: pzs3us1/%05d.bin (%s): %s\n", kCompName[sFile[n].comp], sFile[n].fid - 1, name, what);
                shown++;
            }
        }
        if (differ > shown) {
            fprintf(stderr, "bt3: gameplay data:   ... and %d more files (BT3_FINGERPRINT=1 names them all)\n", differ - shown);
        }
    }
    added_stages();
    if (changed) {
        clock_gettime(CLOCK_MONOTONIC, &t1);
        fprintf(stderr, "bt3: gameplay data: %d of %d files read, %ld ms%s\n", changed, FILES,
                (long)((t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000), changed < FILES ? " (the others are known from last time)" : "");
    }
    free(sAll);
    sAll = NULL;
    sAllCount = sAllCap = 0;
    sState = FP_READY;
}
