/*
 * The automatic update's stand-alone pieces (update_sign.h): release numbers and SHA-256, for gs/net_update.c.
 */
#include "update_sign.h"
#include <SDL3/SDL.h>
#include <string.h>

int UpdateVer_Parse(const char *text, int out[4]) {
    int n = 0, i;
    const char *p = text;

    for (i = 0; i < 4; i++) {
        out[i] = 0;
    }
    if (p == NULL) {
        return 0;
    }
    if (*p == 'v' || *p == 'V') {
        p++;
    }
    for (;;) {
        long v = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9' && digits < 9) {
            v = v * 10 + (*p++ - '0');
            digits++;
        }
        if (digits == 0 || n == 4 || (*p >= '0' && *p <= '9')) {
            return 0;
        }
        out[n++] = (int)v;
        if (*p == '\0') {
            break;
        }
        if (*p != '.') {
            return 0; /* "0.1.39-rc1", "dev": not a release's number */
        }
        p++;
    }
    return n >= 2;
}

int UpdateVer_Compare(const char *a, const char *b) {
    int x[4], y[4], i;
    if (!UpdateVer_Parse(a, x) || !UpdateVer_Parse(b, y)) {
        return -2;
    }
    for (i = 0; i < 4; i++) {
        if (x[i] != y[i]) {
            return x[i] < y[i] ? -1 : 1;
        }
    }
    return 0;
}

/* ---- SHA-256 (FIPS 180-4) ---- */
static const uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_block(UpdSha256 *s, const uint8_t *b) {
    uint32_t w[64], v[8], t1, t2;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = (uint32_t)b[i * 4] << 24 | (uint32_t)b[i * 4 + 1] << 16 | (uint32_t)b[i * 4 + 2] << 8 | b[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    for (i = 0; i < 8; i++) {
        v[i] = s->h[i];
    }
    for (i = 0; i < 64; i++) {
        t1 = v[7] + (ROR(v[4], 6) ^ ROR(v[4], 11) ^ ROR(v[4], 25)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) + kK[i] + w[i];
        t2 = (ROR(v[0], 2) ^ ROR(v[0], 13) ^ ROR(v[0], 22)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
        v[7] = v[6];
        v[6] = v[5];
        v[5] = v[4];
        v[4] = v[3] + t1;
        v[3] = v[2];
        v[2] = v[1];
        v[1] = v[0];
        v[0] = t1 + t2;
    }
    for (i = 0; i < 8; i++) {
        s->h[i] += v[i];
    }
}

void UpdSha256_Init(UpdSha256 *s) {
    static const uint32_t h0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(s->h, h0, sizeof(h0));
    s->bytes = 0;
    s->used = 0;
}

void UpdSha256_Add(UpdSha256 *s, const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    s->bytes += size;
    if (s->used != 0) {
        size_t take = 64 - s->used < size ? 64 - s->used : size;
        memcpy(s->block + s->used, p, take);
        s->used += take;
        p += take;
        size -= take;
        if (s->used < 64) {
            return;
        }
        sha_block(s, s->block);
        s->used = 0;
    }
    for (; size >= 64; p += 64, size -= 64) {
        sha_block(s, p);
    }
    memcpy(s->block, p, size);
    s->used = size;
}

void UpdSha256_End(UpdSha256 *s, uint8_t out[32]) {
    uint64_t bits = s->bytes * 8;
    uint8_t pad[72];
    size_t n = (s->used < 56 ? 56 : 120) - s->used;
    int i;

    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    for (i = 0; i < 8; i++) {
        pad[n + i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    UpdSha256_Add(s, pad, n + 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(s->h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(s->h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(s->h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)s->h[i];
    }
}

static void to_hex(const uint8_t *b, size_t n, char *hex) {
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) {
        hex[i * 2] = digits[b[i] >> 4];
        hex[i * 2 + 1] = digits[b[i] & 15];
    }
    hex[n * 2] = '\0';
}

void UpdSha256_Hex(const void *data, size_t size, char hex[65]) {
    UpdSha256 s;
    uint8_t d[32];
    UpdSha256_Init(&s);
    UpdSha256_Add(&s, data, size);
    UpdSha256_End(&s, d);
    to_hex(d, 32, hex);
}

int UpdSha256_File(const char *path, char hex[65]) {
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    static uint8_t buf[1 << 16]; /* (one caller at a time: the update's thread) */
    UpdSha256 s;
    uint8_t d[32];
    size_t got;

    hex[0] = '\0';
    if (io == NULL) {
        return 0;
    }
    UpdSha256_Init(&s);
    while ((got = SDL_ReadIO(io, buf, sizeof(buf))) > 0) {
        UpdSha256_Add(&s, buf, got);
    }
    if (SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR) {
        SDL_CloseIO(io);
        return 0;
    }
    SDL_CloseIO(io);
    UpdSha256_End(&s, d);
    to_hex(d, 32, hex);
    return 1;
}

static int hex_digit(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

int Upd_HexBytes(const char *hex, uint8_t *out, size_t n) {
    size_t i;
    if (hex == NULL) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        int hi = hex_digit(hex[i * 2]), lo = hi < 0 ? -1 : hex_digit(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return 0;
        }
        out[i] = (uint8_t)(hi << 4 | lo);
    }
    return hex_digit(hex[n * 2]) < 0;
}
