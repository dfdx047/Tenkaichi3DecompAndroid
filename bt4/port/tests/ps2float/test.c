/*
 * PS2 float ground truth. Runs every class of float operation the game uses on a fixed set of inputs and prints
 * the result bits on the EE serial port (PCSX2 writes that to its log). The PC build's float model is compared
 * with this output by check.py. Build: build.sh. Inputs come from an integer generator that check.py repeats.
 */
typedef unsigned int u32;
typedef int s32;
typedef float f32;

static void putc_(char c) { *(volatile char *)0x1000F180 = c; }
static void hex(u32 v) { int i; for (i = 28; i >= 0; i -= 4) putc_("0123456789abcdef"[(v >> i) & 15]); putc_(' '); }
static void str(const char *s) { while (*s) putc_(*s++); }

static u32 seed = 12345;
static u32 rnd(void) { seed = seed * 1664525u + 1013904223u; return seed; }

static f32 f(u32 u) { union { u32 u; f32 f; } x; x.u = u; return x.f; }
static u32 u(f32 v) { union { u32 u; f32 f; } x; x.f = v; return x.u; }

static f32 vu[4] __attribute__((aligned(16)));

int main(void) {
    int i;

    str("\nPS2FLOAT BEGIN\n");
    for (i = 0; i < 1500; i++) {
        u32 a = rnd(), b = rnd(), c;
        u32 ea, eb;
        f32 x, y, r;
        s32 n;

        /* exponents near each other in most cases, exactly spaced in the rest */
        ea = 100 + (rnd() >> 8) % 56;
        eb = (i % 3 == 0) ? ea - (rnd() >> 8) % 30 : 100 + (rnd() >> 8) % 56;
        a = (a & 0x807FFFFF) | (ea << 23);
        b = (b & 0x807FFFFF) | (eb << 23);
        if (i % 7 == 0) b = (a & 0xFFFFFFF0) ^ ((rnd() >> 8) & 0x8000000F); /* nearly equal magnitudes */
        x = f(a);
        y = f(b);
        n = (s32)rnd() >> ((rnd() >> 8) % 24);
        hex(a); hex(b); hex((u32)n);
        hex(u(x + y));
        hex(u(x - y));
        hex(u(x * y));
        hex(u(x / y));
        __asm__ volatile("abs.s $f2, %1\n sqrt.s %0, $f2" : "=f"(r) : "f"(x) : "$f2"); hex(u(r));
        __asm__ volatile("mtc1 $0, $f2\n adda.s $f2, %1\n madd.s %0, %1, %2" : "=f"(r) : "f"(x), "f"(y) : "$f2"); hex(u(r));
        hex(u((f32)n));
        hex((u32)(s32)f((a & 0x807FFFFF) | ((120 + (ea & 31)) << 23)));
        /* vector unit, macro mode: x in vf4.x, y in vf5.x */
        __asm__ volatile("mfc1 $8, %0\n qmtc2 $8, $vf4\n mfc1 $8, %1\n qmtc2 $8, $vf5" : : "f"(x), "f"(y) : "$8");
        __asm__ volatile("vadd.x $vf6, $vf4, $vf5\n qmfc2 $8, $vf6\n move %0, $8" : "=r"(c) : : "$8"); hex(c);
        __asm__ volatile("vsub.x $vf6, $vf4, $vf5\n qmfc2 $8, $vf6\n move %0, $8" : "=r"(c) : : "$8"); hex(c);
        __asm__ volatile("vmul.x $vf6, $vf4, $vf5\n qmfc2 $8, $vf6\n move %0, $8" : "=r"(c) : : "$8"); hex(c);
        __asm__ volatile("vdiv $Q, $vf4x, $vf5x\n vwaitq\n vaddq.x $vf6, $vf0, $Q\n qmfc2 $8, $vf6\n move %0, $8" : "=r"(c) : : "$8"); hex(c);
        __asm__ volatile("vsqrt $Q, $vf4x\n vwaitq\n vaddq.x $vf6, $vf0, $Q\n qmfc2 $8, $vf6\n move %0, $8" : "=r"(c) : : "$8"); hex(c);
        __asm__ volatile("vmula.x $ACC, $vf4, $vf5\n vmadd.x $vf6, $vf4, $vf5\n qmfc2 $8, $vf6\n move %0, $8" : "=r"(c) : : "$8"); hex(c);
        __asm__ volatile("move $8, %1\n qmtc2 $8, $vf7\n vitof0.x $vf6, $vf7\n qmfc2 $8, $vf6\n move %0, $8" : "=r"(c) : "r"(n) : "$8"); hex(c);
        __asm__ volatile("vftoi0.x $vf6, $vf4\n qmfc2 $8, $vf6\n move %0, $8" : "=r"(c) : : "$8"); hex(c);
        putc_('\n');
    }
    str("PS2FLOAT END\n");
    return 0;
}
