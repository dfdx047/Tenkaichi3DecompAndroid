/*
 * The decoder for the game's two movies (DATA/ZS3USOP.PSS, the opening, and ZS3USED.PSS, the ending).
 *
 * They are MPEG-2 program streams with one video stream (the sound is a separate ADX file) made for the PS2's
 * picture decoder: every picture is an intra picture (no motion prediction), 512 x 448, 4:2:0, progressive, frame
 * pictures. That is all this decodes: an MPEG-2 video decoder for intra pictures (ISO/IEC 13818-2), about a
 * tenth of a full one. Measured over both files: 2,810 and 4,200 pictures, all of type I, all with
 * intra_dc_precision 0, q_scale_type 1, intra_vlc_format 1, zigzag scan, the default quantiser matrix; the other
 * values of those options are implemented as the standard gives them but have never met a stream here.
 * A picture of another kind (P or B) is shown as the previous picture.
 *
 *   Mpeg2 *Mpeg2_Open(path)          NULL when the file cannot be read
 *   int Mpeg2_Next(m, rgba)          decodes the next picture into width * height * 4 bytes (R, G, B, 255);
 *                                    0 at the end of the stream
 *   Mpeg2_Width / Mpeg2_Height       valid after the first Mpeg2_Next
 *   Mpeg2_Close
 *
 * Integer arithmetic only (this file is built with the game's software float like the rest of port/src).
 * Test: gcc -O2 -DMPEG2_TEST port/src/plat_mpeg2.c -o mpeg2test && ./mpeg2test movie.PSS out.yuv [frames]
 *       writes planar 4:2:0 pictures, to be compared with `ffmpeg -i movie.PSS -f rawvideo -pix_fmt yuv420p`.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Mpeg2 {
    FILE *fp;
    /* the video elementary stream, as far as it has been taken out of the file */
    uint8_t *es;
    size_t esSize, esCap;
    uint8_t pack[65536 + 16];
    int eof;
    /* bit reader over the unit being decoded */
    const uint8_t *p, *end;
    uint64_t bits;
    int nbits;
    /* sequence */
    int width, height, mbw, mbh;
    uint8_t intraMatrix[64]; /* by position in the block (it is transmitted in zigzag order) */
    /* picture */
    int pictureType, dcPrecision, qScaleType, intraVlc, altScan, pictureStructure, framePredFrameDct;
    uint8_t *y, *cb, *cr;
    int havePicture;
} Mpeg2;

/* ---- tables of the standard ---- */
static const uint8_t kZigzag[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
static const uint8_t kAlternate[64] = {
    0, 8, 16, 24, 1, 9, 2, 10, 17, 25, 32, 40, 48, 56, 57, 49, 41, 33, 26, 18, 3, 11, 4, 12, 19, 27, 34, 42, 50, 58, 35, 43,
    51, 59, 20, 28, 5, 13, 6, 14, 21, 29, 36, 44, 52, 60, 37, 45, 53, 61, 22, 30, 7, 15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63};
static const uint8_t kDefaultIntra[64] = { /* raster order */
    8, 16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37, 19, 22, 26, 27, 29, 34, 34, 38, 22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32, 35, 40, 48, 58, 26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83};
static const uint8_t kNonLinearScale[32] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, 22, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96, 104, 112};

/* DCT coefficients: code (without the sign bit), run, level. Table B-14 and the codes of table B-15 that differ;
   run 64 = end of block, run 65 = escape. */
typedef struct Code { const char *bits; uint8_t run, level; } Code;
static const Code kB14[] = {
    {"10", 64, 0}, {"11", 0, 1}, {"011", 1, 1}, {"0100", 0, 2}, {"0101", 2, 1}, {"00101", 0, 3}, {"00111", 3, 1}, {"00110", 4, 1},
    {"000110", 1, 2}, {"000111", 5, 1}, {"000101", 6, 1}, {"000100", 7, 1}, {"0000110", 0, 4}, {"0000100", 2, 2}, {"0000111", 8, 1},
    {"0000101", 9, 1}, {"000001", 65, 0}, {"00100110", 0, 5}, {"00100001", 0, 6}, {"00100101", 1, 3}, {"00100100", 3, 2},
    {"00100111", 10, 1}, {"00100011", 11, 1}, {"00100010", 12, 1}, {"00100000", 13, 1}, {"0000001010", 0, 7}, {"0000001100", 1, 4},
    {"0000001011", 2, 3}, {"0000001111", 4, 2}, {"0000001001", 5, 2}, {"0000001110", 14, 1}, {"0000001101", 15, 1}, {"0000001000", 16, 1},
    {"000000011101", 0, 8}, {"000000011000", 0, 9}, {"000000010011", 0, 10}, {"000000010000", 0, 11}, {"000000011011", 1, 5},
    {"000000010100", 2, 4}, {"000000011100", 3, 3}, {"000000010010", 4, 3}, {"000000011110", 6, 2}, {"000000010101", 7, 2},
    {"000000010001", 8, 2}, {"000000011111", 17, 1}, {"000000011010", 18, 1}, {"000000011001", 19, 1}, {"000000010111", 20, 1},
    {"000000010110", 21, 1}, {"0000000011010", 0, 12}, {"0000000011001", 0, 13}, {"0000000011000", 0, 14}, {"0000000010111", 0, 15},
    {"0000000010110", 1, 6}, {"0000000010101", 1, 7}, {"0000000010100", 2, 5}, {"0000000010011", 3, 4}, {"0000000010010", 5, 3},
    {"0000000010001", 9, 2}, {"0000000010000", 10, 2}, {"0000000011111", 22, 1}, {"0000000011110", 23, 1}, {"0000000011101", 24, 1},
    {"0000000011100", 25, 1}, {"0000000011011", 26, 1}, {"00000000011111", 0, 16}, {"00000000011110", 0, 17}, {"00000000011101", 0, 18},
    {"00000000011100", 0, 19}, {"00000000011011", 0, 20}, {"00000000011010", 0, 21}, {"00000000011001", 0, 22}, {"00000000011000", 0, 23},
    {"00000000010111", 0, 24}, {"00000000010110", 0, 25}, {"00000000010101", 0, 26}, {"00000000010100", 0, 27}, {"00000000010011", 0, 28},
    {"00000000010010", 0, 29}, {"00000000010001", 0, 30}, {"00000000010000", 0, 31}, {"000000000011000", 0, 32}, {"000000000010111", 0, 33},
    {"000000000010110", 0, 34}, {"000000000010101", 0, 35}, {"000000000010100", 0, 36}, {"000000000010011", 0, 37}, {"000000000010010", 0, 38},
    {"000000000010001", 0, 39}, {"000000000010000", 0, 40}, {"000000000011111", 1, 8}, {"000000000011110", 1, 9}, {"000000000011101", 1, 10},
    {"000000000011100", 1, 11}, {"000000000011011", 1, 12}, {"000000000011010", 1, 13}, {"000000000011001", 1, 14}, {"0000000000010011", 1, 15},
    {"0000000000010010", 1, 16}, {"0000000000010001", 1, 17}, {"0000000000010000", 1, 18}, {"0000000000010100", 6, 3}, {"0000000000011010", 11, 2},
    {"0000000000011001", 12, 2}, {"0000000000011000", 13, 2}, {"0000000000010111", 14, 2}, {"0000000000010110", 15, 2}, {"0000000000010101", 16, 2},
    {"0000000000011111", 27, 1}, {"0000000000011110", 28, 1}, {"0000000000011101", 29, 1}, {"0000000000011100", 30, 1}, {"0000000000011011", 31, 1},
};
/* Table B-15 (intra_vlc_format = 1): these codes; every (run, level) not listed here keeps its B-14 code. */
static const Code kB15[] = {
    {"0110", 64, 0}, {"10", 0, 1}, {"010", 1, 1}, {"110", 0, 2}, {"00101", 2, 1}, {"0111", 0, 3}, {"00111", 3, 1}, {"000110", 4, 1},
    {"00110", 1, 2}, {"000111", 5, 1}, {"0000110", 6, 1}, {"0000100", 7, 1}, {"11100", 0, 4}, {"0000111", 2, 2}, {"0000101", 8, 1},
    {"1111000", 9, 1}, {"000001", 65, 0}, {"11101", 0, 5}, {"000101", 0, 6}, {"1111001", 1, 3}, {"00100110", 3, 2}, {"1111010", 10, 1},
    {"00100001", 11, 1}, {"00100101", 12, 1}, {"00100100", 13, 1}, {"000100", 0, 7}, {"00100111", 1, 4}, {"11111100", 2, 3},
    {"11111101", 4, 2}, {"000000100", 5, 2}, {"000000101", 14, 1}, {"000000111", 15, 1}, {"0000001101", 16, 1}, {"1111011", 0, 8},
    {"1111100", 0, 9}, {"00100011", 0, 10}, {"00100010", 0, 11}, {"00100000", 1, 5}, {"0000001100", 2, 4}, {"11111010", 0, 12},
    {"11111011", 0, 13}, {"11111110", 0, 14}, {"11111111", 0, 15},
};

typedef struct Vlc { uint8_t run, level, len; } Vlc;
static Vlc sVlc[2][65536];
static int sVlcReady;

static void vlc_add(Vlc *t, const Code *c) {
    int len = (int)strlen(c->bits), v = 0, i;
    for (i = 0; i < len; i++) {
        v = v << 1 | (c->bits[i] - '0');
    }
    for (i = 0; i < 1 << (16 - len); i++) {
        Vlc *e = &t[(v << (16 - len)) + i];
        e->run = c->run;
        e->level = c->level;
        e->len = (uint8_t)len;
    }
}

static void vlc_init(void) {
    size_t i, k;
    for (i = 0; i < sizeof(kB14) / sizeof(kB14[0]); i++) {
        vlc_add(sVlc[0], &kB14[i]);
    }
    /* B-15: its own codes, and the B-14 code of every pair it does not list */
    for (i = 0; i < sizeof(kB14) / sizeof(kB14[0]); i++) {
        for (k = 0; k < sizeof(kB15) / sizeof(kB15[0]); k++) {
            if (kB15[k].run == kB14[i].run && kB15[k].level == kB14[i].level) {
                break;
            }
        }
        if (k == sizeof(kB15) / sizeof(kB15[0])) {
            vlc_add(sVlc[1], &kB14[i]);
        }
    }
    for (k = 0; k < sizeof(kB15) / sizeof(kB15[0]); k++) {
        vlc_add(sVlc[1], &kB15[k]);
    }
    sVlcReady = 1;
}

/* ---- bits ---- */
static void refill(Mpeg2 *m) {
    while (m->nbits <= 56) {
        m->bits |= (uint64_t)(m->p < m->end ? *m->p : 0) << (56 - m->nbits);
        m->p++;
        m->nbits += 8;
    }
}

static uint32_t peek(Mpeg2 *m, int n) {
    if (m->nbits < n) {
        refill(m);
    }
    return (uint32_t)(m->bits >> (64 - n));
}

static void skip(Mpeg2 *m, int n) {
    if (m->nbits < n) {
        refill(m);
    }
    m->bits <<= n;
    m->nbits -= n;
}

static uint32_t get(Mpeg2 *m, int n) {
    uint32_t v = peek(m, n);
    skip(m, n);
    return v;
}

static void start_bits(Mpeg2 *m, const uint8_t *p, const uint8_t *end) {
    m->p = p;
    m->end = end;
    m->bits = 0;
    m->nbits = 0;
}

/* ---- inverse DCT: the fixed-point Chen-Wang transform of the MPEG reference decoder ---- */
#define W1 2841
#define W2 2676
#define W3 2408
#define W5 1609
#define W6 1108
#define W7 565

static void idct_row(int32_t *b) {
    int32_t x0, x1 = b[4] << 11, x2 = b[6], x3 = b[2], x4 = b[1], x5 = b[7], x6 = b[5], x7 = b[3], x8;

    if (!(x1 | x2 | x3 | x4 | x5 | x6 | x7)) {
        b[0] = b[1] = b[2] = b[3] = b[4] = b[5] = b[6] = b[7] = b[0] << 3;
        return;
    }
    x0 = (b[0] << 11) + 128;
    x8 = W7 * (x4 + x5); x4 = x8 + (W1 - W7) * x4; x5 = x8 - (W1 + W7) * x5;
    x8 = W3 * (x6 + x7); x6 = x8 - (W3 - W5) * x6; x7 = x8 - (W3 + W5) * x7;
    x8 = x0 + x1; x0 -= x1;
    x1 = W6 * (x3 + x2); x2 = x1 - (W2 + W6) * x2; x3 = x1 + (W2 - W6) * x3;
    x1 = x4 + x6; x4 -= x6; x6 = x5 + x7; x5 -= x7;
    x7 = x8 + x3; x8 -= x3; x3 = x0 + x2; x0 -= x2;
    x2 = (181 * (x4 + x5) + 128) >> 8; x4 = (181 * (x4 - x5) + 128) >> 8;
    b[0] = (x7 + x1) >> 8; b[1] = (x3 + x2) >> 8; b[2] = (x0 + x4) >> 8; b[3] = (x8 + x6) >> 8;
    b[4] = (x8 - x6) >> 8; b[5] = (x0 - x4) >> 8; b[6] = (x3 - x2) >> 8; b[7] = (x7 - x1) >> 8;
}

static void idct_col(int32_t *b) {
    int32_t x0, x1 = b[8 * 4] << 8, x2 = b[8 * 6], x3 = b[8 * 2], x4 = b[8 * 1], x5 = b[8 * 7], x6 = b[8 * 5], x7 = b[8 * 3], x8;

    if (!(x1 | x2 | x3 | x4 | x5 | x6 | x7)) {
        b[0] = b[8] = b[16] = b[24] = b[32] = b[40] = b[48] = b[56] = (b[0] + 32) >> 6;
        return;
    }
    x0 = (b[0] << 8) + 8192;
    x8 = W7 * (x4 + x5) + 4; x4 = (x8 + (W1 - W7) * x4) >> 3; x5 = (x8 - (W1 + W7) * x5) >> 3;
    x8 = W3 * (x6 + x7) + 4; x6 = (x8 - (W3 - W5) * x6) >> 3; x7 = (x8 - (W3 + W5) * x7) >> 3;
    x8 = x0 + x1; x0 -= x1;
    x1 = W6 * (x3 + x2) + 4; x2 = (x1 - (W2 + W6) * x2) >> 3; x3 = (x1 + (W2 - W6) * x3) >> 3;
    x1 = x4 + x6; x4 -= x6; x6 = x5 + x7; x5 -= x7;
    x7 = x8 + x3; x8 -= x3; x3 = x0 + x2; x0 -= x2;
    x2 = (181 * (x4 + x5) + 128) >> 8; x4 = (181 * (x4 - x5) + 128) >> 8;
    b[8 * 0] = (x7 + x1) >> 14; b[8 * 1] = (x3 + x2) >> 14; b[8 * 2] = (x0 + x4) >> 14; b[8 * 3] = (x8 + x6) >> 14;
    b[8 * 4] = (x8 - x6) >> 14; b[8 * 5] = (x0 - x4) >> 14; b[8 * 6] = (x3 - x2) >> 14; b[8 * 7] = (x7 - x1) >> 14;
}

/* ---- one intra block: DC difference, AC coefficients, inverse quantisation, inverse DCT, into the plane ---- */
static int dc_size(Mpeg2 *m, int chroma) {
    uint32_t v = peek(m, 10);
    int size, len;

    if (!chroma) { /* table B-12 */
        if (v < 0x200) { size = v < 0x100 ? 1 : 2; len = 2; }                 /* 00, 01 */
        else if (v < 0x280) { size = 0; len = 3; }                             /* 100 */
        else if (v < 0x300) { size = 3; len = 3; }                             /* 101 */
        else if (v < 0x380) { size = 4; len = 3; }                             /* 110 */
        else { /* 1110, 11110, ...: the number of ones after "111" */
            int ones = 0;
            while (ones < 6 && (v & (0x40 >> ones))) { ones++; }
            if (ones == 6) { /* 111111111 */
                skip(m, 9);
                return 11;
            }
            size = 5 + ones; len = 4 + ones;
        }
    } else { /* table B-13 */
        if (v < 0x300) { size = (int)(v >> 8); len = 2; }                      /* 00, 01, 10 */
        else {
            int ones = 0;
            while (ones < 7 && (v & (0x80 >> ones))) { ones++; }
            if (ones == 7) { /* 1111111110, 1111111111 */
                skip(m, 10);
                return (v & 1) ? 11 : 10;
            }
            size = 3 + ones; len = 3 + ones;
        }
    }
    skip(m, len);
    return size;
}

static int block(Mpeg2 *m, int chroma, int *dcPred, int qscale, uint8_t *dst, int stride) {
    int32_t c[64];
    const uint8_t *scan = m->altScan ? kAlternate : kZigzag;
    const Vlc *table = sVlc[m->intraVlc];
    int size = dc_size(m, chroma), diff = 0, i = 0, sum, x, y;

    memset(c, 0, sizeof(c));
    if (size != 0) {
        diff = (int)get(m, size);
        if (diff < 1 << (size - 1)) {
            diff -= (1 << size) - 1;
        }
    }
    *dcPred += diff;
    c[0] = *dcPred * (8 >> m->dcPrecision);
    sum = c[0];
    for (;;) {
        const Vlc *e = &table[peek(m, 16)];
        int run, level, sign, pos, v;

        if (e->len == 0) {
            return 0; /* not a code: damaged data */
        }
        skip(m, e->len);
        if (e->run == 64) {
            break;
        }
        if (e->run == 65) { /* escape: 6 bits of run, 12 bits of level in two's complement */
            run = (int)get(m, 6);
            level = (int)get(m, 12);
            sign = level >= 2048;
            if (sign) {
                level = 4096 - level;
            }
            if (level == 0) {
                return 0;
            }
        } else {
            run = e->run;
            level = e->level;
            sign = (int)get(m, 1);
        }
        i += run + 1;
        if (i > 63) {
            return 0;
        }
        pos = scan[i];
        v = (2 * level * m->intraMatrix[pos] * qscale) / 32;
        if (v > 2047) {
            v = 2047 + sign; /* -2048 is allowed */
        }
        c[pos] = sign ? -v : v;
        sum += c[pos];
    }
    if (c[0] > 2047) { c[0] = 2047; } /* (cannot happen for valid data) */
    if (!(sum & 1)) { /* mismatch control */
        c[63] ^= 1;
    }
    for (y = 0; y < 8; y++) {
        idct_row(&c[y * 8]);
    }
    for (x = 0; x < 8; x++) {
        idct_col(&c[x]);
    }
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            int v = c[y * 8 + x];
            dst[y * stride + x] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
    }
    return 1;
}

/* ---- a slice of an intra picture: one or more macroblocks of a row ---- */
static void slice(Mpeg2 *m, int row) {
    int code = (int)get(m, 5), qscale, dc[3], mbx = -1, k;

    if (row >= m->mbh) {
        return;
    }
    if (get(m, 1)) { /* intra_slice_flag: intra_slice and seven reserved bits, then extra information bytes */
        skip(m, 8);
        while (get(m, 1)) {
            skip(m, 8);
        }
    }
    qscale = m->qScaleType ? kNonLinearScale[code] : code * 2;
    dc[0] = dc[1] = dc[2] = 1 << (7 + m->dcPrecision);
    for (;;) {
        int inc = 0;
        /* macroblock_address_increment: "1" = 1; an intra picture has no skipped macroblocks, so within a slice
           it is always 1, but the first one gives the column the slice starts at */
        for (;;) {
            uint32_t v = peek(m, 11);
            if (v == 0x008) { skip(m, 11); inc += 33; continue; }   /* escape */
            if (v == 0x00F) { skip(m, 11); continue; }              /* stuffing (MPEG-1) */
            if (v >= 0x400) { skip(m, 1); inc += 1; break; }
            if (v >= 0x300) { skip(m, 3); inc += 2; break; }
            if (v >= 0x200) { skip(m, 3); inc += 3; break; }
            if (v >= 0x180) { skip(m, 4); inc += 4; break; }
            if (v >= 0x100) { skip(m, 4); inc += 5; break; }
            if (v >= 0x0C0) { skip(m, 5); inc += 6; break; }
            if (v >= 0x080) { skip(m, 5); inc += 7; break; }
            if (v >= 0x070) { skip(m, 7); inc += 8; break; }
            if (v >= 0x060) { skip(m, 7); inc += 9; break; }
            if (v >= 0x030) { skip(m, 8); inc += 15 - (int)((v >> 3) - 6); break; }       /* 10..15 */
            if (v >= 0x024) { skip(m, 10); inc += 21 - (int)((v >> 1) - 18); break; }     /* 16..21 */
            if (v >= 0x018) { skip(m, 11); inc += 33 - (int)(v - 24); break; }            /* 22..33 */
            return; /* not a code: the end of the slice's data */
        }
        mbx += inc;
        if (mbx >= m->mbw) {
            return;
        }
        /* macroblock_type of an intra picture: "1" intra, "01" intra with a new quantiser scale */
        if (get(m, 1) == 0) {
            if (get(m, 1) == 0) {
                return;
            }
            code = (int)get(m, 5);
            qscale = m->qScaleType ? kNonLinearScale[code] : code * 2;
        }
        if (!m->framePredFrameDct) {
            skip(m, 1); /* dct_type: frame or field DCT. Field DCT is not handled (never present here) */
        }
        for (k = 0; k < 4; k++) {
            if (!block(m, 0, &dc[0], qscale, m->y + (row * 16 + (k >> 1) * 8) * m->mbw * 16 + mbx * 16 + (k & 1) * 8, m->mbw * 16)) {
                return;
            }
        }
        if (!block(m, 1, &dc[1], qscale, m->cb + row * 8 * m->mbw * 8 + mbx * 8, m->mbw * 8) ||
            !block(m, 1, &dc[2], qscale, m->cr + row * 8 * m->mbw * 8 + mbx * 8, m->mbw * 8)) {
            return;
        }
        if (peek(m, 23) == 0) {
            return; /* a start code (or the end of the data) follows: the slice is over */
        }
    }
}

/* ---- headers ---- */
static void sequence_header(Mpeg2 *m) {
    int i, w = (int)get(m, 12), h = (int)get(m, 12);

    skip(m, 4 + 4 + 18 + 1 + 10 + 1); /* aspect, frame rate, bit rate, marker, buffer size, constrained flag */
    if (get(m, 1)) {
        for (i = 0; i < 64; i++) {
            m->intraMatrix[kZigzag[i]] = (uint8_t)get(m, 8);
        }
    } else {
        memcpy(m->intraMatrix, kDefaultIntra, 64);
    }
    if (w != m->width || h != m->height) {
        m->width = w;
        m->height = h;
        m->mbw = (w + 15) / 16;
        m->mbh = (h + 15) / 16;
        free(m->y);
        m->y = calloc(1, (size_t)m->mbw * m->mbh * 384);
        m->cb = m->y + (size_t)m->mbw * m->mbh * 256;
        m->cr = m->cb + (size_t)m->mbw * m->mbh * 64;
        memset(m->cb, 128, (size_t)m->mbw * m->mbh * 128);
    }
    /* MPEG-1 values, until a picture coding extension says otherwise */
    m->dcPrecision = 0;
    m->qScaleType = 0;
    m->intraVlc = 0;
    m->altScan = 0;
    m->framePredFrameDct = 1;
    m->pictureStructure = 3;
}

static void extension(Mpeg2 *m) {
    int id = (int)get(m, 4), i;

    if (id == 8) { /* picture coding extension */
        skip(m, 16); /* f_codes */
        m->dcPrecision = (int)get(m, 2);
        m->pictureStructure = (int)get(m, 2);
        skip(m, 1); /* top_field_first */
        m->framePredFrameDct = (int)get(m, 1);
        skip(m, 1); /* concealment_motion_vectors */
        m->qScaleType = (int)get(m, 1);
        m->intraVlc = (int)get(m, 1);
        m->altScan = (int)get(m, 1);
    } else if (id == 3) { /* quant matrix extension */
        if (get(m, 1)) {
            for (i = 0; i < 64; i++) {
                m->intraMatrix[kZigzag[i]] = (uint8_t)get(m, 8);
            }
        }
    }
}

/* Decodes the unit [p, end): everything from one picture start code up to the next. */
static void decode_unit(Mpeg2 *m, const uint8_t *p, const uint8_t *end) {
    while (p + 4 <= end) {
        const uint8_t *next;
        int code;

        if (!(p[0] == 0 && p[1] == 0 && p[2] == 1)) {
            p++;
            continue;
        }
        code = p[3];
        for (next = p + 4; next + 3 <= end && !(next[0] == 0 && next[1] == 0 && next[2] == 1); next++) {
        }
        if (next + 3 > end) {
            next = end;
        }
        start_bits(m, p + 4, next);
        if (code == 0xB3) {
            sequence_header(m);
        } else if (code == 0xB5) {
            extension(m);
        } else if (code == 0x00) {
            skip(m, 10); /* temporal_reference */
            m->pictureType = (int)get(m, 3);
        } else if (code >= 0x01 && code <= 0xAF && m->y != NULL && m->pictureType == 1 && m->pictureStructure == 3) {
            slice(m, code - 1);
            m->havePicture = 1;
        }
        p = next;
    }
}

/* ---- the program stream: pack headers, and packets of which the video ones carry the elementary stream ---- */
static int more(Mpeg2 *m) {
    uint8_t h[14];

    while (!m->eof) {
        if (fread(h, 1, 4, m->fp) != 4) {
            break;
        }
        while (!(h[0] == 0 && h[1] == 0 && h[2] == 1)) { /* resynchronise */
            int c = fgetc(m->fp);
            if (c == EOF) {
                m->eof = 1;
                return 0;
            }
            h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = (uint8_t)c;
        }
        if (h[3] == 0xB9) { /* program end */
            break;
        }
        if (h[3] == 0xBA) { /* pack header: 10 more bytes, the last one counts stuffing bytes */
            if (fread(h + 4, 1, 10, m->fp) != 10) {
                break;
            }
            fseek(m->fp, h[13] & 7, SEEK_CUR);
            continue;
        }
        if (fread(h + 4, 1, 2, m->fp) != 2) {
            break;
        }
        {
            size_t len = (size_t)h[4] << 8 | h[5], skipBytes;
            if (fread(m->pack, 1, len, m->fp) != len) {
                break;
            }
            if (h[3] < 0xE0 || h[3] > 0xEF || len < 3) {
                continue; /* system header, padding, other streams */
            }
            skipBytes = 3 + (size_t)m->pack[2]; /* two flag bytes, the header length, the header */
            if (skipBytes > len) {
                continue;
            }
            if (m->esSize + len > m->esCap) {
                m->esCap = (m->esSize + len) * 2 + 65536;
                m->es = realloc(m->es, m->esCap);
            }
            memcpy(m->es + m->esSize, m->pack + skipBytes, len - skipBytes);
            m->esSize += len - skipBytes;
            return 1;
        }
    }
    m->eof = 1;
    return 0;
}

/* The offset of the first picture start code at or after `from`, or -1. */
static long find_picture(const Mpeg2 *m, size_t from) {
    size_t i;
    for (i = from; i + 4 <= m->esSize; i++) {
        if (m->es[i] == 0 && m->es[i + 1] == 0 && m->es[i + 2] == 1 && m->es[i + 3] == 0) {
            return (long)i;
        }
    }
    return -1;
}

Mpeg2 *Mpeg2_Open(const char *path) {
    Mpeg2 *m;
    FILE *fp = fopen(path, "rb");

    if (fp == NULL) {
        return NULL;
    }
    if (!sVlcReady) {
        vlc_init();
    }
    m = calloc(1, sizeof(Mpeg2));
    m->fp = fp;
    return m;
}

void Mpeg2_Close(Mpeg2 *m) {
    if (m != NULL) {
        fclose(m->fp);
        free(m->es);
        free(m->y);
        free(m);
    }
}

int Mpeg2_Width(const Mpeg2 *m) { return m->width; }
int Mpeg2_Height(const Mpeg2 *m) { return m->height; }

/* Decodes the next picture into the planes. 0 at the end. */
static int next_planes(Mpeg2 *m) {
    long first, second;
    size_t searched = 0;

    /* the stream from here to the second picture start code is one picture with the headers in front of it
       (and the headers of the next picture behind it, which only set state) */
    for (;;) {
        first = find_picture(m, 0);
        if (first >= 0) {
            break;
        }
        if (!more(m)) {
            return 0;
        }
    }
    searched = (size_t)first + 4;
    for (;;) {
        second = find_picture(m, searched);
        if (second >= 0) {
            break;
        }
        searched = m->esSize >= 3 ? m->esSize - 3 : 0;
        if (searched < (size_t)first + 4) {
            searched = (size_t)first + 4;
        }
        if (!more(m)) {
            second = (long)m->esSize;
            break;
        }
    }
    m->havePicture = 0;
    decode_unit(m, m->es, m->es + second);
    memmove(m->es, m->es + second, m->esSize - (size_t)second);
    m->esSize -= (size_t)second;
    return m->y != NULL;
}

int Mpeg2_Next(Mpeg2 *m, uint8_t *rgba) {
    int x, y, w, h, stride;

    if (!next_planes(m)) {
        return 0;
    }
    w = m->width;
    h = m->height;
    stride = m->mbw * 16;
    /* ITU-R BT.601, 16..235 to 0..255, chroma samples repeated (16.16 fixed point) */
    for (y = 0; y < h; y++) {
        const uint8_t *py = m->y + y * stride, *pb = m->cb + (y >> 1) * (stride >> 1), *pr = m->cr + (y >> 1) * (stride >> 1);
        uint8_t *o = rgba + (size_t)y * w * 4;
        for (x = 0; x < w; x++) {
            int c = 76309 * (py[x] - 16) + 32768, d = pb[x >> 1] - 128, e = pr[x >> 1] - 128;
            int r = (c + 104597 * e) >> 16, g = (c - 25675 * d - 53279 * e) >> 16, b = (c + 132201 * d) >> 16;
            o[x * 4 + 0] = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
            o[x * 4 + 1] = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
            o[x * 4 + 2] = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
            o[x * 4 + 3] = 255;
        }
    }
    return 1;
}

#ifdef MPEG2_TEST
int main(int argc, char **argv) {
    Mpeg2 *m = argc > 2 ? Mpeg2_Open(argv[1]) : NULL;
    FILE *out = argc > 2 ? fopen(argv[2], "wb") : NULL;
    int limit = argc > 3 ? atoi(argv[3]) : 1 << 30, n = 0, y;

    if (m == NULL || out == NULL) {
        fprintf(stderr, "usage: mpeg2test movie.PSS out.yuv [frames]\n");
        return 1;
    }
    while (n < limit && next_planes(m)) {
        for (y = 0; y < m->height; y++) { fwrite(m->y + y * m->mbw * 16, 1, (size_t)m->width, out); }
        for (y = 0; y < m->height / 2; y++) { fwrite(m->cb + y * m->mbw * 8, 1, (size_t)m->width / 2, out); }
        for (y = 0; y < m->height / 2; y++) { fwrite(m->cr + y * m->mbw * 8, 1, (size_t)m->width / 2, out); }
        n++;
    }
    fclose(out);
    printf("%d pictures of %d x %d\n", n, m->width, m->height);
    Mpeg2_Close(m);
    return 0;
}
#endif
