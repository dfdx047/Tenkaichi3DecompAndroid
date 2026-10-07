/*
 * VU1: the vertex unit. The game uploads nine vertex programs (its own machine code for this unit) and, per
 * object, a block of matrices and vertices; the program transforms, lights and clips them and "kicks" finished
 * GIF packets to the GS. This file is an interpreter for the unit, so the ORIGINAL programs run unchanged:
 *     VIF1 UNPACK / MPG / MSCAL  ->  VU1 memory and program  ->  interpreter  ->  XGKICK  ->  GIF (gs_core.c)
 * It is the reference for the 3D scene, as gs_core.c's rasteriser is for the GS: once the picture is right, each
 * program can be replaced by a GPU vertex shader that is checked against it.
 *
 * Implemented: the instructions the nine programs use (see docs/port/README.md for the survey) and a few
 * neighbours; anything else is counted and reported once. Timing: the Q register's latency and the four-step
 * delay of the clipping flags are modelled (the programs read them), MAC / status flags are not (never read).
 * Arithmetic is the host's float unit with infinities and NaNs clamped: this is drawing, not simulation.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gs_internal.h"

typedef union Reg {
    float f[4];
    uint32_t u[4];
    int32_t i[4];
} Reg;

static struct {
    Reg vf[32], acc;
    uint16_t vi[16];
    float q, i, p;
    float qPending;
    uint32_t qReady, cycle;
    uint32_t clip;                      /* what the flag instructions see */
    uint32_t clipLatest;                /* clipping results including those still in the pipeline */
    struct { uint32_t cycle, value; } clipQueue[8];
    int clipHead, clipCount;
    uint32_t pc;                        /* in instructions */
    uint8_t mem[0x4000], micro[0x4000];
    /* VIF1 side */
    uint32_t top, tops, base, ofst, dbf, itop, itops, mode, mask, cl, wl, row[4], col[4];
} vu;

/* See Gs_StateKeep (gs_core.c): the vertex unit's memory and loaded programs, kept (0) or brought back (1). */
void GsVu1_StateKeep(int restore) {
    static unsigned char *kept;
    if (!restore) {
        if (kept == NULL) {
            kept = malloc(sizeof(vu));
        }
        memcpy(kept, &vu, sizeof(vu));
    } else if (kept != NULL) {
        memcpy(&vu, kept, sizeof(vu));
    }
}

static unsigned sUnknown[2][128];
static unsigned sStatRuns, sStatInstr, sStatKicks;
static unsigned sProgSize;
static unsigned sHle; /* program calls served by shader versions this frame */
static struct { unsigned size, runs, instr, kicks; } sProg[16]; /* per loaded program, this frame */

static inline float clampf(float v) {
    uint32_t u;
    memcpy(&u, &v, 4);
    if ((u & 0x7F800000u) == 0x7F800000u) {
        u = (u & 0x80000000u) | 0x7F7FFFFFu;
        memcpy(&v, &u, 4);
    }
    return v;
}

/* ---------------------------------------------------------------------------------------------- VIF1 side */

void GsVu1_SetCycle(uint32_t cl, uint32_t wl) { vu.cl = cl; vu.wl = wl; }
void GsVu1_SetBase(uint32_t v) { vu.base = v & 0x3FF; }
void GsVu1_SetOffset(uint32_t v) { vu.ofst = v & 0x3FF; vu.dbf = 0; vu.tops = vu.base; }
void GsVu1_SetItop(uint32_t v) { vu.itops = v & 0x3FF; }
void GsVu1_SetMode(uint32_t v) { vu.mode = v & 3; }
void GsVu1_SetMask(uint32_t v) { vu.mask = v; }
void GsVu1_SetRow(const uint32_t *v) { memcpy(vu.row, v, 16); }
void GsVu1_SetCol(const uint32_t *v) { memcpy(vu.col, v, 16); }

void GsVu1_Program(uint32_t addr, const uint32_t *words, uint32_t count) {
    uint32_t i;
    /* which program is loaded: identified by its size in instructions (an upload at 0 starts a new one) */
    sProgSize = addr == 0 ? count / 2 : sProgSize + count / 2;
    for (i = 0; i < count; i++) {
        memcpy(&vu.micro[((addr * 8) + i * 4) & 0x3FFC], &words[i], 4);
    }
}

/* Words of data an UNPACK command with these parameters is followed by. */
uint32_t GsVu1_UnpackWords(uint32_t cmd, uint32_t num) {
    uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, n = num ? num : 256;
    uint32_t bits = vn == 3 && vl == 3 ? 16 : (32u >> vl) * (vn + 1);

    if (vu.wl > vu.cl && vu.wl != 0) {
        n = vu.cl * (n / vu.wl) + (n % vu.wl > vu.cl ? vu.cl : n % vu.wl);
    }
    return (n * bits + 31) / 32;
}

void GsVu1_Unpack(uint32_t cmd, uint32_t num, uint32_t imm, const uint32_t *data) {
    uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, masked = (cmd >> 4) & 1, usn = (imm >> 14) & 1;
    uint32_t addr = (imm & 0x3FF) + ((imm >> 15) & 1 ? vu.tops : 0), n = num ? num : 256;
    uint32_t cl = vu.cl ? vu.cl : 1, wl = vu.wl ? vu.wl : 1, cyc = 0, bitpos = 0, k, c;
    const uint8_t *src = (const uint8_t *)data;

    /* The usual case by far (every vertex of every model): no mask, no row arithmetic, consecutive vectors. The
       same result as the general loop below, one tight loop per format. */
    if (!masked && vu.mode == 0 && cl == wl && !(vn == 3 && vl == 3) && vn != 0 && vl != 3) {
        uint32_t comps = vn + 1;
        if (vn == 3 && vl == 0) { /* four words per vector, which is all the game's models and effects send: a copy */
            for (k = 0; k < n; k++) {
                uint32_t at = (addr + k) & 0x3FF, run = 0x400 - at;
                if (run > n - k) {
                    run = n - k;
                }
                memcpy(&vu.mem[at * 16], src + k * 16, run * 16);
                k += run - 1;
            }
            return;
        }
        for (k = 0; k < n; k++) {
            uint32_t out[4] = {0, 0, 0, 0};
            if (vl == 0) {
                for (c = 0; c < comps; c++) {
                    memcpy(&out[c], src + c * 4, 4);
                }
                src += comps * 4;
            } else if (vl == 1) {
                for (c = 0; c < comps; c++) {
                    uint16_t w;
                    memcpy(&w, src + c * 2, 2);
                    out[c] = usn ? (uint32_t)w : (uint32_t)(int32_t)(int16_t)w;
                }
                src += comps * 2;
            } else {
                for (c = 0; c < comps; c++) {
                    out[c] = usn ? (uint32_t)src[c] : (uint32_t)(int32_t)(int8_t)src[c];
                }
                src += comps;
            }
            memcpy(&vu.mem[((addr + k) & 0x3FF) * 16], out, 16);
        }
        return;
    }
    for (k = 0; k < n; k++) {
        uint32_t v[4] = {0, 0, 0, 0}, out[4];
        int have = !(wl > cl && cyc >= cl); /* filling write: vectors beyond CL come from the registers only */

        if (have) {
            if (vn == 3 && vl == 3) {
                uint32_t w = src[bitpos / 8] | src[bitpos / 8 + 1] << 8;
                v[0] = (w & 31) << 3; v[1] = ((w >> 5) & 31) << 3; v[2] = ((w >> 10) & 31) << 3; v[3] = ((w >> 15) & 1) << 7;
                bitpos += 16;
            } else {
                uint32_t size = 32u >> vl;
                for (c = 0; c <= vn; c++) {
                    uint32_t w = 0;
                    memcpy(&w, &src[bitpos / 8], size / 8);
                    if (!usn && size == 16) { w = (uint32_t)(int32_t)(int16_t)w; }
                    if (!usn && size == 8) { w = (uint32_t)(int32_t)(int8_t)w; }
                    v[c] = w;
                    bitpos += size;
                }
                if (vn == 0) {
                    v[1] = v[2] = v[3] = v[0];
                }
            }
        }
        for (c = 0; c < 4; c++) {
            uint32_t m = masked ? (vu.mask >> (2 * ((cyc > 3 ? 3 : cyc) * 4 + c))) & 3 : 0;
            if (m == 0) {
                out[c] = v[c];
                if (vu.mode == 1) {
                    out[c] = v[c] + vu.row[c];
                } else if (vu.mode == 2) {
                    vu.row[c] += v[c];
                    out[c] = vu.row[c];
                }
            } else if (m == 1) {
                out[c] = vu.row[c];
            } else if (m == 2) {
                out[c] = vu.col[cyc > 3 ? 3 : cyc];
            } else {
                memcpy(&out[c], &vu.mem[((addr & 0x3FF) * 16) + c * 4], 4);
            }
        }
        memcpy(&vu.mem[(addr & 0x3FF) * 16], out, 16);
        addr++;
        if (++cyc == (wl > cl ? wl : wl)) {
            if (cl > wl) {
                addr += cl - wl;
            }
            cyc = 0;
        }
    }
}

/* ---------------------------------------------------------------------------------------------- interpreter */

static void kick(uint32_t addr) {
    uint8_t buf[0x4000];
    uint32_t a = (addr & 0x3FF) * 16, len = 0;

    sStatKicks++;
    /* the packet may wrap around the end of VU memory; hand the GIF interpreter one tag at a time until EOP */
    for (;;) {
        uint64_t lo, hi;
        uint32_t nloop, flg, nreg, qw;
        memcpy(&lo, &vu.mem[a & 0x3FFF], 8);
        memcpy(&hi, &vu.mem[(a + 8) & 0x3FFF], 8);
        nloop = lo & 0x7FFF;
        flg = (lo >> 58) & 3;
        nreg = (lo >> 60) & 15;
        if (nreg == 0) { nreg = 16; }
        qw = 1 + (flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop);
        if (len + qw * 16 > sizeof(buf)) {
            break;
        }
        for (; qw != 0; qw--, a += 16, len += 16) {
            memcpy(&buf[len], &vu.mem[a & 0x3FFF], 16);
        }
        if ((lo >> 15) & 1) {
            break;
        }
    }
    Gs_Gif(buf, len / 16);
}

#define DEST(n) ((dest >> (3 - (n))) & 1) /* field order in the instruction: x y z w */

/* Runs the program at instruction `start` until its end (E bit). */
static void run(uint32_t start) {
    int ending = 0, branch = 0;
    uint32_t target = 0, guard;

    unsigned slot, before = sStatInstr, kbefore = sStatKicks;

    vu.pc = start & 0x7FF;
    sStatRuns++;
    for (slot = 0; slot < 15 && sProg[slot].size != 0 && sProg[slot].size != sProgSize; slot++) {
    }
    sProg[slot].size = sProgSize;
    sProg[slot].runs++;
    for (guard = 0; guard < 4000000; guard++) {
        uint32_t lo, hi, dest, ft, fs, fd, op, bc, c;
        Reg res, *wr = NULL;
        uint32_t wmask = 0;
        int wacc = 0, lower = 1;
        uint32_t next = (vu.pc + 1) & 0x7FF;

        memcpy(&lo, &vu.micro[vu.pc * 8], 4);
        memcpy(&hi, &vu.micro[vu.pc * 8 + 4], 4);
        vu.cycle++;
        sStatInstr++;
        if (vu.qReady != 0 && vu.cycle >= vu.qReady) {
            vu.q = vu.qPending;
            vu.qReady = 0;
        }
        while (vu.clipCount != 0 && vu.clipQueue[vu.clipHead].cycle <= vu.cycle) {
            vu.clip = vu.clipQueue[vu.clipHead].value;
            vu.clipHead = (vu.clipHead + 1) & 7;
            vu.clipCount--;
        }
        dest = (hi >> 21) & 15;
        ft = (hi >> 16) & 31;
        fs = (hi >> 11) & 31;
        fd = (hi >> 6) & 31;
        op = hi & 0x3F;
        bc = hi & 3;
        if (hi & 0x80000000u) { /* I bit: the lower word is a float for the I register */
            memcpy(&vu.i, &lo, 4);
            lower = 0;
        }
        /* ---- upper instruction: result into `res`, committed after the lower one has read its operands ---- */
#define A(c) vu.vf[fs].f[c]
#define B(c) vu.vf[ft].f[c]
#define BC vu.vf[ft].f[bc]
#define EACH for (c = 0; c < 4; c++) if (DEST(c))
#define TO_FD wr = &vu.vf[fd]; wmask = dest
#define TO_ACC wacc = 1; wmask = dest
        if (hi == 0x000002FFu) {
            goto upper_done; /* NOP: three quarters of the programs' upper instructions */
        }
        res = op >= 0x3C ? vu.acc : vu.vf[fd];
        if (op < 0x3C) {
            switch (op) {
            case 0x00: case 0x01: case 0x02: case 0x03: EACH res.f[c] = clampf(A(c) + BC); TO_FD; break;
            case 0x04: case 0x05: case 0x06: case 0x07: EACH res.f[c] = clampf(A(c) - BC); TO_FD; break;
            case 0x08: case 0x09: case 0x0A: case 0x0B: EACH res.f[c] = clampf(vu.acc.f[c] + A(c) * BC); TO_FD; break;
            case 0x0C: case 0x0D: case 0x0E: case 0x0F: EACH res.f[c] = clampf(vu.acc.f[c] - A(c) * BC); TO_FD; break;
            case 0x10: case 0x11: case 0x12: case 0x13: EACH res.f[c] = A(c) > BC ? A(c) : BC; TO_FD; break;
            case 0x14: case 0x15: case 0x16: case 0x17: EACH res.f[c] = A(c) < BC ? A(c) : BC; TO_FD; break;
            case 0x18: case 0x19: case 0x1A: case 0x1B: EACH res.f[c] = clampf(A(c) * BC); TO_FD; break;
            case 0x1C: EACH res.f[c] = clampf(A(c) * vu.q); TO_FD; break;
            case 0x1D: EACH res.f[c] = A(c) > vu.i ? A(c) : vu.i; TO_FD; break;
            case 0x1E: EACH res.f[c] = clampf(A(c) * vu.i); TO_FD; break;
            case 0x1F: EACH res.f[c] = A(c) < vu.i ? A(c) : vu.i; TO_FD; break;
            case 0x20: EACH res.f[c] = clampf(A(c) + vu.q); TO_FD; break;
            case 0x21: EACH res.f[c] = clampf(vu.acc.f[c] + A(c) * vu.q); TO_FD; break;
            case 0x22: EACH res.f[c] = clampf(A(c) + vu.i); TO_FD; break;
            case 0x23: EACH res.f[c] = clampf(vu.acc.f[c] + A(c) * vu.i); TO_FD; break;
            case 0x24: EACH res.f[c] = clampf(A(c) - vu.q); TO_FD; break;
            case 0x25: EACH res.f[c] = clampf(vu.acc.f[c] - A(c) * vu.q); TO_FD; break;
            case 0x26: EACH res.f[c] = clampf(A(c) - vu.i); TO_FD; break;
            case 0x27: EACH res.f[c] = clampf(vu.acc.f[c] - A(c) * vu.i); TO_FD; break;
            case 0x28: EACH res.f[c] = clampf(A(c) + B(c)); TO_FD; break;
            case 0x29: EACH res.f[c] = clampf(vu.acc.f[c] + A(c) * B(c)); TO_FD; break;
            case 0x2A: EACH res.f[c] = clampf(A(c) * B(c)); TO_FD; break;
            case 0x2B: EACH res.f[c] = A(c) > B(c) ? A(c) : B(c); TO_FD; break;
            case 0x2C: EACH res.f[c] = clampf(A(c) - B(c)); TO_FD; break;
            case 0x2D: EACH res.f[c] = clampf(vu.acc.f[c] - A(c) * B(c)); TO_FD; break;
            case 0x2E: /* OPMSUB: cross product, second half */
                res.f[0] = clampf(vu.acc.f[0] - A(1) * B(2)); res.f[1] = clampf(vu.acc.f[1] - A(2) * B(0));
                res.f[2] = clampf(vu.acc.f[2] - A(0) * B(1)); wr = &vu.vf[fd]; wmask = 0xE; break;
            case 0x2F: EACH res.f[c] = A(c) < B(c) ? A(c) : B(c); TO_FD; break;
            default: sUnknown[0][op]++; break;
            }
        } else {
            uint32_t sp = (fd << 2) | bc;
            switch (sp) {
            case 0x00: case 0x01: case 0x02: case 0x03: EACH res.f[c] = clampf(A(c) + BC); TO_ACC; break;
            case 0x04: case 0x05: case 0x06: case 0x07: EACH res.f[c] = clampf(A(c) - BC); TO_ACC; break;
            case 0x08: case 0x09: case 0x0A: case 0x0B: EACH res.f[c] = clampf(vu.acc.f[c] + A(c) * BC); TO_ACC; break;
            case 0x0C: case 0x0D: case 0x0E: case 0x0F: EACH res.f[c] = clampf(vu.acc.f[c] - A(c) * BC); TO_ACC; break;
            case 0x10: res = vu.vf[ft]; EACH res.f[c] = (float)vu.vf[fs].i[c]; wr = &vu.vf[ft]; wmask = dest; break;
            case 0x11: res = vu.vf[ft]; EACH res.f[c] = (float)vu.vf[fs].i[c] * (1.0f / 16.0f); wr = &vu.vf[ft]; wmask = dest; break;
            case 0x12: res = vu.vf[ft]; EACH res.f[c] = (float)vu.vf[fs].i[c] * (1.0f / 4096.0f); wr = &vu.vf[ft]; wmask = dest; break;
            case 0x13: res = vu.vf[ft]; EACH res.f[c] = (float)vu.vf[fs].i[c] * (1.0f / 32768.0f); wr = &vu.vf[ft]; wmask = dest; break;
            case 0x14: case 0x15: case 0x16: case 0x17: {
                static const float k[4] = {1.0f, 16.0f, 4096.0f, 32768.0f};
                res = vu.vf[ft];
                EACH {
                    double v = (double)A(c) * k[sp - 0x14];
                    res.i[c] = v >= 2147483647.0 ? 0x7FFFFFFF : v <= -2147483648.0 ? (int32_t)0x80000000u : (int32_t)v;
                }
                wr = &vu.vf[ft]; wmask = dest; break;
            }
            case 0x18: case 0x19: case 0x1A: case 0x1B: EACH res.f[c] = clampf(A(c) * BC); TO_ACC; break;
            case 0x1C: EACH res.f[c] = clampf(A(c) * vu.q); TO_ACC; break;
            case 0x1D: res = vu.vf[ft]; EACH res.u[c] = vu.vf[fs].u[c] & 0x7FFFFFFFu; wr = &vu.vf[ft]; wmask = dest; break;
            case 0x1E: EACH res.f[c] = clampf(A(c) * vu.i); TO_ACC; break;
            case 0x1F: { /* CLIP: fs.xyz against +/-|ft.w| */
                float w = fabsf(B(3));
                uint32_t f = (A(0) > w) | (A(0) < -w) << 1 | (A(1) > w) << 2 | (A(1) < -w) << 3 | (A(2) > w) << 4 | (A(2) < -w) << 5;
                int slot = (vu.clipHead + vu.clipCount) & 7;
                vu.clipLatest = ((vu.clipLatest << 6) | f) & 0xFFFFFF;
                vu.clipQueue[slot].cycle = vu.cycle + 4;
                vu.clipQueue[slot].value = vu.clipLatest;
                if (vu.clipCount < 8) { vu.clipCount++; }
                break;
            }
            case 0x20: EACH res.f[c] = clampf(A(c) + vu.q); TO_ACC; break;
            case 0x21: EACH res.f[c] = clampf(vu.acc.f[c] + A(c) * vu.q); TO_ACC; break;
            case 0x22: EACH res.f[c] = clampf(A(c) + vu.i); TO_ACC; break;
            case 0x23: EACH res.f[c] = clampf(vu.acc.f[c] + A(c) * vu.i); TO_ACC; break;
            case 0x24: EACH res.f[c] = clampf(A(c) - vu.q); TO_ACC; break;
            case 0x26: EACH res.f[c] = clampf(A(c) - vu.i); TO_ACC; break;
            case 0x28: EACH res.f[c] = clampf(A(c) + B(c)); TO_ACC; break;
            case 0x29: EACH res.f[c] = clampf(vu.acc.f[c] + A(c) * B(c)); TO_ACC; break;
            case 0x2A: EACH res.f[c] = clampf(A(c) * B(c)); TO_ACC; break;
            case 0x2C: EACH res.f[c] = clampf(A(c) - B(c)); TO_ACC; break;
            case 0x2D: EACH res.f[c] = clampf(vu.acc.f[c] - A(c) * B(c)); TO_ACC; break;
            case 0x2E: /* OPMULA: cross product, first half */
                res.f[0] = clampf(A(1) * B(2)); res.f[1] = clampf(A(2) * B(0)); res.f[2] = clampf(A(0) * B(1)); wacc = 1; wmask = 0xE; break;
            case 0x2F: break; /* NOP */
            default: sUnknown[0][64 + (sp & 63)]++; break;
            }
        }
upper_done:
        /* ---- lower instruction ---- */
        if (lower && lo != 0x8000033Cu) { /* 0x8000033C: the lower NOP (move vf0, vf0) */
            uint32_t lop = lo >> 25, it = (lo >> 16) & 31, is = (lo >> 11) & 31, id = (lo >> 6) & 31, ld = (lo >> 21) & 15;
            int32_t imm11 = (int32_t)(lo << 21) >> 21;
            uint32_t imm15 = ((lo >> 10) & 0x7800) | (lo & 0x7FF);
#define LDEST(n) ((ld >> (3 - (n))) & 1)
#define MEM(a) (&vu.mem[((uint32_t)(a) & 0x3FF) * 16])
#define SETVI(r, v) do { if ((r) & 15) { vu.vi[(r) & 15] = (uint16_t)(v); } } while (0)
            switch (lop) {
            case 0x00: { Reg m; memcpy(&m, MEM((int32_t)vu.vi[is & 15] + imm11), 16); for (c = 0; c < 4; c++) if (LDEST(c) && it) vu.vf[it].u[c] = m.u[c]; break; }
            case 0x01: { uint8_t *m = MEM((int32_t)vu.vi[it & 15] + imm11); for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(m + c * 4, &vu.vf[is].u[c], 4); break; }
            case 0x04: { uint8_t *m = MEM((int32_t)vu.vi[is & 15] + imm11); for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w; memcpy(&w, m + c * 4, 4); SETVI(it, w); } break; }
            case 0x05: { uint8_t *m = MEM((int32_t)vu.vi[is & 15] + imm11); for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w = vu.vi[it & 15]; memcpy(m + c * 4, &w, 4); } break; }
            case 0x08: SETVI(it, vu.vi[is & 15] + imm15); break;
            case 0x09: SETVI(it, vu.vi[is & 15] - imm15); break;
            case 0x10: SETVI(1, (vu.clip & 0xFFFFFF) == (lo & 0xFFFFFF)); break;
            case 0x11: vu.clip = vu.clipLatest = lo & 0xFFFFFF; vu.clipCount = 0; break;
            case 0x12: SETVI(1, (vu.clip & lo & 0xFFFFFF) != 0); break;
            case 0x13: SETVI(1, ((vu.clip | lo) & 0xFFFFFF) == 0xFFFFFF); break;
            case 0x1C: SETVI(it, vu.clip & 0xFFF); break;
            case 0x20: branch = 2; target = (vu.pc + 1 + (uint32_t)imm11) & 0x7FF; break;
            case 0x21: SETVI(it, (vu.pc + 2) & 0x7FF); branch = 2; target = (vu.pc + 1 + (uint32_t)imm11) & 0x7FF; break;
            case 0x24: branch = 2; target = vu.vi[is & 15] & 0x7FF; break; /* JR: the register holds an instruction number */
            case 0x25: { uint32_t t = vu.vi[is & 15] & 0x7FF; SETVI(it, (vu.pc + 2) & 0x7FF); branch = 2; target = t; break; }
            case 0x28: if (vu.vi[it & 15] == vu.vi[is & 15]) { branch = 2; target = (vu.pc + 1 + (uint32_t)imm11) & 0x7FF; } break;
            case 0x29: if (vu.vi[it & 15] != vu.vi[is & 15]) { branch = 2; target = (vu.pc + 1 + (uint32_t)imm11) & 0x7FF; } break;
            case 0x2C: if ((int16_t)vu.vi[is & 15] < 0) { branch = 2; target = (vu.pc + 1 + (uint32_t)imm11) & 0x7FF; } break;
            case 0x2D: if ((int16_t)vu.vi[is & 15] > 0) { branch = 2; target = (vu.pc + 1 + (uint32_t)imm11) & 0x7FF; } break;
            case 0x2E: if ((int16_t)vu.vi[is & 15] <= 0) { branch = 2; target = (vu.pc + 1 + (uint32_t)imm11) & 0x7FF; } break;
            case 0x2F: if ((int16_t)vu.vi[is & 15] >= 0) { branch = 2; target = (vu.pc + 1 + (uint32_t)imm11) & 0x7FF; } break;
            case 0x40: {
                uint32_t f = lo & 0x3F;
                if (f < 0x3C) {
                    switch (f) {
                    case 0x30: SETVI(id, vu.vi[is & 15] + vu.vi[it & 15]); break;
                    case 0x31: SETVI(id, vu.vi[is & 15] - vu.vi[it & 15]); break;
                    case 0x32: SETVI(it, vu.vi[is & 15] + (uint32_t)((int32_t)(lo << 21) >> 27)); break;
                    case 0x34: SETVI(id, vu.vi[is & 15] & vu.vi[it & 15]); break;
                    case 0x35: SETVI(id, vu.vi[is & 15] | vu.vi[it & 15]); break;
                    default: sUnknown[1][f]++; break;
                    }
                } else {
                    uint32_t sp = (id << 2) | (lo & 3), fsf = (lo >> 21) & 3, ftf = (lo >> 23) & 3;
                    switch (sp) {
                    case 0x30: if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) vu.vf[it].u[c] = vu.vf[is].u[c]; } break; /* MOVE */
                    case 0x31: if (it) { Reg s = vu.vf[is]; if (LDEST(0)) vu.vf[it].u[0] = s.u[1]; if (LDEST(1)) vu.vf[it].u[1] = s.u[2];
                                         if (LDEST(2)) vu.vf[it].u[2] = s.u[3]; if (LDEST(3)) vu.vf[it].u[3] = s.u[0]; } break; /* MR32 */
                    case 0x34: { Reg m; memcpy(&m, MEM(vu.vi[is & 15]), 16); if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) vu.vf[it].u[c] = m.u[c]; }
                                 SETVI(is, vu.vi[is & 15] + 1); break; } /* LQI */
                    case 0x35: { uint8_t *m = MEM(vu.vi[it & 15]); for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(m + c * 4, &vu.vf[is].u[c], 4);
                                 SETVI(it, vu.vi[it & 15] + 1); break; } /* SQI */
                    case 0x36: { Reg m; SETVI(is, vu.vi[is & 15] - 1); memcpy(&m, MEM(vu.vi[is & 15]), 16);
                                 if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) vu.vf[it].u[c] = m.u[c]; } break; } /* LQD */
                    case 0x37: { uint8_t *m; SETVI(it, vu.vi[it & 15] - 1); m = MEM(vu.vi[it & 15]);
                                 for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(m + c * 4, &vu.vf[is].u[c], 4); break; } /* SQD */
                    case 0x38: { float d = vu.vf[it].f[ftf], n = vu.vf[is].f[fsf]; /* DIV */
                                 vu.qPending = d != 0.0f ? clampf(n / d) : ((n < 0.0f) != (d < 0.0f) || (n < 0.0f && d == 0.0f) ? -3.4028235e38f : 3.4028235e38f);
                                 vu.qReady = vu.cycle + 7; break; }
                    case 0x39: vu.qPending = sqrtf(fabsf(vu.vf[it].f[ftf])); vu.qReady = vu.cycle + 7; break;
                    case 0x3A: { float d = sqrtf(fabsf(vu.vf[it].f[ftf])); vu.qPending = d != 0.0f ? clampf(vu.vf[is].f[fsf] / d) : 3.4028235e38f;
                                 vu.qReady = vu.cycle + 13; break; }
                    case 0x3B: if (vu.qReady != 0) { vu.q = vu.qPending; vu.qReady = 0; } break; /* WAITQ */
                    case 0x3C: SETVI(it, vu.vf[is].u[fsf]); break;                                /* MTIR */
                    case 0x3D: if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) vu.vf[it].i[c] = (int16_t)vu.vi[is & 15]; } break; /* MFIR */
                    case 0x3E: { uint8_t *m = MEM(vu.vi[is & 15]); for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w; memcpy(&w, m + c * 4, 4); SETVI(it, w); } break; } /* ILWR */
                    case 0x3F: { uint8_t *m = MEM(vu.vi[is & 15]); for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w = vu.vi[it & 15]; memcpy(m + c * 4, &w, 4); } break; } /* ISWR */
                    case 0x64: if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) vu.vf[it].f[c] = vu.p; } break; /* MFP */
                    case 0x68: SETVI(it, vu.top); break;   /* XTOP */
                    case 0x69: SETVI(it, vu.itop); break;  /* XITOP */
                    case 0x6C: kick(vu.vi[is & 15]); break; /* XGKICK */
                    case 0x7B: break;                       /* WAITP */
                    default: sUnknown[1][64 + (sp & 63)]++; break;
                    }
                }
                break;
            }
            default: sUnknown[1][lop & 63]++; break;
            }
        }
        /* ---- commit the upper instruction's result ---- */
        if (wr != NULL && wr != &vu.vf[0]) {
            for (c = 0; c < 4; c++) if ((wmask >> (3 - c)) & 1) wr->u[c] = res.u[c];
        } else if (wacc) {
            for (c = 0; c < 4; c++) if ((wmask >> (3 - c)) & 1) vu.acc.u[c] = res.u[c];
        }
        vu.vf[0].f[0] = vu.vf[0].f[1] = vu.vf[0].f[2] = 0.0f;
        vu.vf[0].f[3] = 1.0f;
        /* ---- next instruction: branches take effect after one more instruction; so does the end ---- */
        if (ending) {
            vu.pc = next;
            sProg[slot].instr += sStatInstr - before;
            sProg[slot].kicks += sStatKicks - kbefore;
            return;
        }
        if (hi & 0x40000000u) {
            ending = 1;
        }
        if (branch == 1) {
            vu.pc = target;
            branch = 0;
        } else {
            if (branch == 2) {
                branch = 1;
            }
            vu.pc = next;
        }
    }
    {
        static int reported;
        if (!reported++) {
            uint32_t k, lo2, hi2;
            { FILE *fp = fopen("port/build/vu1_micro.bin", "wb"); if (fp) { fwrite(vu.micro, 1, sizeof(vu.micro), fp); fclose(fp); }
              fp = fopen("port/build/vu1_mem.bin", "wb"); if (fp) { fwrite(vu.mem, 1, sizeof(vu.mem), fp); fclose(fp); } }
            fprintf(stderr, "vu1: program at %u did not end; pc %u, vi:", start, vu.pc);
            for (k = 0; k < 16; k++) { fprintf(stderr, " %04x", vu.vi[k]); }
            fprintf(stderr, "\n  top %u tops %u base %u ofst %u itop %u; code around pc:\n", vu.top, vu.tops, vu.base, vu.ofst, vu.itop);
            for (k = (vu.pc > 24 ? vu.pc - 24 : 0); k < vu.pc + 12; k++) {
                memcpy(&lo2, &vu.micro[(k & 0x7FF) * 8], 4);
                memcpy(&hi2, &vu.micro[(k & 0x7FF) * 8 + 4], 4);
                fprintf(stderr, "  %4u: %08x %08x\n", k, hi2, lo2);
            }
        }
    }
}

/* ---------------------------------------------------------------------------------------------- shader versions */

/* Program 0 (the fighters' models; 127 instructions uploaded) without the interpreter: the batch at TOP is two
   GIF headers and one strip; both layers go to the GPU back end with the constants, which does the vertex work in
   shaders/vu0.vert. Layout of the batch (verified on dumps): +0 GIF tag "one A+D register", +1 TEX0_1 of the
   texture layer, +2 TEX0_2 of the toon layer, +3 / +4 the primitive tags of the two layers (NLOOP = vertices,
   PRIM in the tag), +5 the vertices, three quadwords each. */
static int hle_program0(void) {
    uint8_t pkt[32];
    uint64_t tag[2];
    uint32_t top = vu.top & 0x3FF, count;
    int layer;

    memcpy(tag, &vu.mem[(top + 3) * 16], 16);
    count = (uint32_t)(tag[0] & 0x7FFF);
    if (count < 3 || top + 5 + count * 3 > 1024) {
        return 0;
    }
    for (layer = 0; layer < 2; layer++) {
        memcpy(pkt, &vu.mem[top * 16], 16);
        memcpy(pkt + 16, &vu.mem[(top + 1 + (uint32_t)layer) * 16], 16);
        Gs_Gif(pkt, 2); /* the layer's TEX0 */
        memcpy(tag, &vu.mem[(top + 3 + (uint32_t)layer) * 16], 16);
        Gs_RegWrite(0, (tag[0] >> 47) & 0x7FF); /* PRIM from the tag */
        GsGpu_DrawVu0(layer, (int)((tag[0] >> 56) & 1), (const float *)&vu.mem[(top + 5) * 16], count, (const float *)vu.mem);
    }
    sStatKicks++;
    return 1;
}

/* Program 1 (124 instructions): a second pass over some fighters' parts. The batch is program 0's. Layer 0 is
   program 0's layer 0 (model texture, colour from constant 22); layer 1 takes its texture from constant 26 and
   its coordinates from the normal in a camera-aligned frame (vu0.vert layer 5). A batch whose first normal has
   0 in the low 16 bits of its 4th word is not drawn. */
static int hle_program1(void) {
    uint8_t pkt[32];
    uint64_t tag[2];
    uint32_t top = vu.top & 0x3FF, count, mark;
    int layer;

    memcpy(tag, &vu.mem[(top + 3) * 16], 16);
    count = (uint32_t)(tag[0] & 0x7FFF);
    if (count < 3 || top + 5 + count * 3 > 1024) {
        return 0;
    }
    memcpy(&mark, &vu.mem[(top + 6) * 16 + 12], 4);
    if ((mark & 0xFFFF) == 0) {
        return 1;
    }
    for (layer = 0; layer < 2; layer++) {
        memcpy(pkt, &vu.mem[top * 16], 16);
        memcpy(pkt + 16, layer == 0 ? &vu.mem[(top + 1) * 16] : &vu.mem[26 * 16], 16);
        Gs_Gif(pkt, 2); /* the layer's TEX0 */
        memcpy(tag, &vu.mem[(top + 3 + (uint32_t)layer) * 16], 16);
        Gs_RegWrite(0, (tag[0] >> 47) & 0x7FF); /* PRIM from the tag */
        GsGpu_DrawVu0(layer == 0 ? 0 : 5, (int)((tag[0] >> 56) & 1), (const float *)&vu.mem[(top + 5) * 16], count, (const float *)vu.mem);
    }
    sStatKicks++;
    return 1;
}

/* Program 4 (the stage; 399 instructions uploaded). After MSCALF 0 the first MSCNT only takes a one-quadword
   object header; every later MSCNT is a batch: +0 GIF tag "one A+D register" with EOP, +1 its data (the texture),
   +2 the primitive tag (NLOOP = vertices, PRIM in the tag, registers ST, RGBAQ, XYZ2), +3 the vertices, three
   quadwords each (position, colour as floats, texture coordinates). One batch is one strip. */
static int sP4Batches; /* MSCNTs since the last MSCALF */

static int hle_program4(void) {
    uint64_t tag[2];
    uint32_t top = vu.top & 0x3FF, count;

    memcpy(tag, &vu.mem[(top + 2) * 16], 16);
    count = (uint32_t)(tag[0] & 0x7FFF);
    if (count < 3 || top + 3 + count * 3 > 1024) {
        return 0;
    }
    Gs_Gif(&vu.mem[top * 16], 2);
    Gs_RegWrite(0, (tag[0] >> 47) & 0x7FF);
    GsGpu_DrawVu4((int)((tag[0] >> 56) & 1), (const float *)&vu.mem[(top + 3) * 16], count, (const float *)vu.mem);
    sStatKicks++;
    return 1;
}

/* Programs 2a / 2b (99 / 101 instructions; a fighter model in one flat colour: the flat shadow and the silhouette
   drawn into the shadow page; decomp src/vu1/prog2a.vsm, prog2b.vsm). The chains are the fighters' own, so the
   batch is program 0's: +0 A+D tag, +1 its data, +4 the primitive tag (count in the low 15 bits), vertices from
   +5, three quadwords each (position with the bone weight in w, normal (not used), s t 1). Constants: 0..3 bone
   A, 4..7 bone B, 8 / 9 pivots, 10 the colour, 11..14 the screen matrix. That is program 0's first layer with
   the constants in other places, so the same shader draws it. (2a also drops triangles with a vertex outside
   the view volume; the GPU clips them instead.) */
static int hle_program2(void) {
    float consts[96];
    uint64_t tag[2];
    uint32_t top = vu.top & 0x3FF, count;

    memcpy(tag, &vu.mem[(top + 4) * 16], 16);
    count = (uint32_t)(tag[0] & 0x7FFF);
    if (count < 3 || top + 5 + count * 3 > 1024) {
        return 0;
    }
    memset(consts, 0, sizeof(consts));
    memcpy(&consts[0], &vu.mem[0], 10 * 16);          /* bones and pivots: as in program 0 */
    memcpy(&consts[56], &vu.mem[11 * 16], 4 * 16);    /* screen matrix: program 0 has it at 14..17 */
    memcpy(&consts[88], &vu.mem[10 * 16], 16);        /* colour: program 0's layer 0 colour at 22 */
    Gs_Gif(&vu.mem[top * 16], 2);
    Gs_RegWrite(0, (tag[0] >> 47) & 0x7FF); /* PRIM from the tag */
    /* the context is the one of the PRIM in force, not the tag's: the shadow passes run with PRMODECONT.AC = 0,
       where the attributes (texturing, blending, context) come from PRMODE and the tag only gives the type */
    GsGpu_DrawVu0(2, (int)((gGs.prim >> 9) & 1), (const float *)&vu.mem[(top + 5) * 16], count, consts);
    sStatKicks++;
    return 1;
}

/* Program 6 (451 instructions; the ground shadow: decomp src/vu1/prog6.vsm). After MSCALF 0 the first MSCNT only
   takes one quadword (the tag for the clipper's fans); every later MSCNT is a batch: +0 / +1 a TEX0 tag and data
   that the program never sends, +2 the primitive tag (PRE set, NLOOP = vertices), +3 the vertices, four
   quadwords each. */
/* Programs 8 (animated stage objects; 94 instructions) and 7 (debris; 104). Both take the stage's batch: +0 GIF
   tag "one A+D register", +1 the texture, +2 the primitive tag, +3 the vertices (position, colour as floats,
   texture coordinates). 8 is the fighters' two-matrix skinning with the colour of each vertex; 7 is rigid, takes
   its alpha from the mesh and does not draw a triangle whose last vertex carries a flag (low 16 bits of the
   position's 4th word). Both go through the fighter shader (vu0.vert, layers 3 and 4). */
static int hle_program78(int layer) {
    float consts[96];
    uint64_t tag[2];
    uint32_t top = vu.top & 0x3FF, count;

    memcpy(tag, &vu.mem[(top + 2) * 16], 16);
    count = (uint32_t)(tag[0] & 0x7FFF);
    if (count < 3 || top + 3 + count * 3 > 1024) {
        return 0;
    }
    memset(consts, 0, sizeof(consts));
    if (layer == 3) {
        memcpy(&consts[0], &vu.mem[0], 10 * 16);        /* matrices A, B and their pivots */
        memcpy(&consts[56], &vu.mem[10 * 16], 4 * 16);  /* screen matrix */
    } else {
        memcpy(&consts[0], &vu.mem[0], 4 * 16);         /* the mesh's matrix, for both halves of the blend */
        memcpy(&consts[16], &vu.mem[0], 4 * 16);
        memcpy(&consts[56], &vu.mem[4 * 16], 4 * 16);   /* screen matrix */
        memcpy(&consts[91], &vu.mem[12 * 16 + 12], 4);  /* the mesh's alpha */
    }
    Gs_Gif(&vu.mem[top * 16], 2);
    Gs_RegWrite(0, (tag[0] >> 47) & 0x7FF);
    GsGpu_DrawVu0(layer, (int)((tag[0] >> 56) & 1), (const float *)&vu.mem[(top + 3) * 16], count, consts);
    sStatKicks++;
    return 1;
}

static int sP6Batches;

static int hle_program6(void) {
    uint64_t tag[2];
    uint32_t top = vu.top & 0x3FF, count;

    memcpy(tag, &vu.mem[(top + 2) * 16], 16);
    count = (uint32_t)(tag[0] & 0x7FFF);
    if (count < 3 || top + 3 + count * 4 > 1024) {
        return 0;
    }
    Gs_RegWrite(0, (tag[0] >> 47) & 0x7FF);
    GsGpu_DrawVu6((int)((gGs.prim >> 9) & 1), (const float *)&vu.mem[(top + 3) * 16], count, (const float *)vu.mem);
    sStatKicks++;
    return 1;
}

/* MSCAL / MSCALF (addr = instruction address) or MSCNT (addr < 0: continue where the last run stopped). */
void GsVu1_Call(int addr) {
    vu.top = vu.tops;
    vu.itop = vu.itops;
    if (vu.dbf) {
        vu.tops = vu.base;
        vu.dbf = 0;
    } else {
        vu.tops = vu.base + vu.ofst;
        vu.dbf = 1;
    }
    if (gPortResim) {
        /* A frame that is only being re-run (rollback, an online session's silent start-up): the program is not
           run. A vertex program's whole output is primitives for the GS, none of which would be drawn; what a
           later frame's programs need (their code, the data unpacked for them, the registers above) is kept up
           by the rest of the list, which is still walked. */
        return;
    }
    if (getenv("BT3_VU_DUMP") != NULL && (int)gGsFrame == atoi(getenv("BT3_VU_DUMP")) && sProgSize == 127) {
        static int shown;
        if (shown < 3 && addr < 0) {
            uint32_t q, w[4];
            float f[4];
            shown++;
            fprintf(stderr, "vu1 dump: program %u, MSCNT, top %u, itop %u\n", sProgSize, vu.top, vu.itop);
            for (q = 0; q < 24; q++) {
                memcpy(w, &vu.mem[q * 16], 16); memcpy(f, w, 16);
                fprintf(stderr, "  const %2u: %08x %08x %08x %08x  %10.4g %10.4g %10.4g %10.4g\n", q, w[0], w[1], w[2], w[3], f[0], f[1], f[2], f[3]);
            }
            for (q = 0; q < 11; q++) {
                memcpy(w, &vu.mem[((vu.top + q) & 0x3FF) * 16], 16); memcpy(f, w, 16);
                fprintf(stderr, "  top+%2u: %08x %08x %08x %08x  %10.4g %10.4g %10.4g %10.4g\n", q, w[0], w[1], w[2], w[3], f[0], f[1], f[2], f[3]);
            }
        }
    }
    if (getenv("BT3_VU_CALLS") != NULL) { /* which entry addresses each program is called with (-1 = MSCNT) */
        static struct { unsigned size; int addr; unsigned n; } seen[64];
        static unsigned total;
        int k;
        for (k = 0; k < 63 && seen[k].n != 0 && !(seen[k].size == sProgSize && seen[k].addr == addr); k++) {
        }
        seen[k].size = sProgSize;
        seen[k].addr = addr;
        seen[k].n++;
        if (++total % 20000 == 0) {
            for (k = 0; k < 64 && seen[k].n != 0; k++) {
                fprintf(stderr, "vucalls: program of %u instructions, entry %d: %u calls\n", seen[k].size, seen[k].addr, seen[k].n);
            }
        }
    }
    if (sProgSize == 399 && GsGpu_Enabled() && getenv("BT3_VU_INTERP") == NULL) {
        if (addr >= 0) {          /* MSCALF 0: constants only */
            sP4Batches = 0;
            sHle++;
            return;
        }
        if (sP4Batches++ == 0) {  /* the object header: only the clipper used it */
            sHle++;
            return;
        }
        if (hle_program4()) {
            sHle++;
            return;
        }
        /* a batch the shader path cannot take: the interpreter needs the state the skipped calls would have left */
        run(0);
        vu.pc = 21;
    }
    if (sProgSize == 451 && GsGpu_Enabled() && getenv("BT3_VU_INTERP") == NULL) {
        if (addr >= 0) {          /* MSCALF 0: constants to registers only */
            sP6Batches = 0;
            sHle++;
            return;
        }
        if (sP6Batches++ == 0 || hle_program6()) { /* the first MSCNT: the fan tag, only the clipper uses it */
            sHle++;
            return;
        }
        /* a batch the shader path cannot take (fewer than 3 vertices): nothing to draw either */
        sHle++;
        return;
    }
    if ((sProgSize == 94 || sProgSize == 104) && GsGpu_Enabled() && getenv("BT3_VU_INTERP") == NULL) {
        /* MSCALF 0 only loads registers; every MSCNT is one batch */
        if (addr >= 0 || hle_program78(sProgSize == 94 ? 3 : 4)) {
            sHle++;
            return;
        }
        run(0); /* a batch the shader path cannot take: give the interpreter the registers the setup loads */
        vu.pc = sProgSize == 94 ? 15 : 52;
    }
    if ((sProgSize == 99 || sProgSize == 101) && GsGpu_Enabled() && getenv("BT3_VU_INTERP") == NULL) {
        /* MSCALF 0 only loads registers; every MSCNT is one batch */
        if (addr >= 0 || hle_program2()) {
            sHle++;
            return;
        }
        run(0); /* a batch the shader path cannot take: give the interpreter the registers the setup loads */
        vu.pc = 16;
    }
    if (sProgSize == 124 && GsGpu_Enabled() && getenv("BT3_VU_INTERP") == NULL) {
        /* MSCALF 0 only loads registers; every MSCNT is one batch */
        if (addr >= 0 || hle_program1()) {
            sHle++;
            return;
        }
        run(0);
        vu.pc = 15;
    }
    if (sProgSize == 127 && GsGpu_Enabled() && getenv("BT3_VU_INTERP") == NULL) {
        /* MSCALF 0 only prepares derived constants (the shader derives them itself); every MSCNT is one batch */
        if (addr >= 0 || hle_program0()) {
            sHle++;
            return;
        }
    }
    run(addr < 0 ? vu.pc : (uint32_t)addr);
}

void GsVu1_FrameEnd(void) {
    int k, i;

    if (getenv("BT3_GS_VERBOSE") != NULL && (gGsFrame < 4 || gGsFrame % 60 == 0)) {
        fprintf(stderr, "vu1: frame %u: %u interpreted runs, %u instructions, %u kicks; %u calls served by shaders\n", gGsFrame, sStatRuns, sStatInstr, sStatKicks, sHle);
        for (k = 0; k < 16 && sProg[k].size != 0; k++) {
            fprintf(stderr, "vu1:   program of %u instructions: %u runs, %u instructions (%u%%), %u kicks\n", sProg[k].size, sProg[k].runs,
                    sProg[k].instr, sStatInstr ? sProg[k].instr * 100 / sStatInstr : 0, sProg[k].kicks);
        }
    }
    sStatRuns = sStatInstr = sStatKicks = 0;
    sHle = 0;
    memset(sProg, 0, sizeof(sProg));
    for (k = 0; k < 2; k++) {
        for (i = 0; i < 128; i++) {
            if (sUnknown[k][i] != 0) {
                fprintf(stderr, "vu1: unimplemented %s instruction %s%02x, %u times\n", k ? "lower" : "upper", i >= 64 ? "special " : "", i & 63, sUnknown[k][i]);
                sUnknown[k][i] = 0;
            }
        }
    }
}
