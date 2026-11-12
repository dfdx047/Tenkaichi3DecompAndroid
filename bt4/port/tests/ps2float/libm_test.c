/*
 * Ground truth for the game's own C maths library. This driver is linked at 0x01000000 and packed (pack_libm.py)
 * into one ELF together with the user's game executable, so it can call the ORIGINAL routines at their addresses
 * inside PCSX2 and print the result bits. check_libm.py compares them with the PC build's newlib sources.
 */
typedef unsigned int u32;
typedef float f32;
#define FN1(addr) ((f32 (*)(f32))(addr))
#define FN2(addr) ((f32 (*)(f32, f32))(addr))
static void putc_(char c) { *(volatile char *)0x1000F180 = c; }
static void hex(u32 v) { int i; for (i = 28; i >= 0; i -= 4) putc_("0123456789abcdef"[(v >> i) & 15]); putc_(' '); }
static void str(const char *s) { while (*s) putc_(*s++); }
static u32 seed = 2024;
static u32 rnd(void) { seed = seed * 1664525u + 1013904223u; return seed; }
static f32 f(u32 u) { union { u32 u; f32 f; } x; x.u = u; return x.f; }
static u32 u(f32 v) { union { u32 u; f32 f; } x; x.f = v; return x.u; }

int main(void) {
    int i;

    str("\nPS2LIBM BEGIN\n");
    for (i = 0; i < 1500; i++) {
        u32 a = (rnd() & 0x807FFFFF) | ((112 + (rnd() >> 8) % 20) << 23); /* magnitudes 2^-15 .. 2^4 */
        u32 b = (rnd() & 0x807FFFFF) | ((112 + (rnd() >> 8) % 20) << 23);
        u32 c = (rnd() & 0x807FFFFF) | ((110 + (rnd() >> 8) % 17) << 23); /* below 1 */
        hex(a); hex(b); hex(c);
        hex(u(FN1(0x0028F598)(f(a))));            /* sinf */
        hex(u(FN1(0x0028F3C0)(f(a))));            /* cosf */
        hex(u(FN1(0x0028F680)(f(a))));            /* tanf */
        hex(u(FN1(0x0028F130)(f(a))));            /* atanf */
        hex(u(FN2(0x0028F740)(f(a), f(b))));      /* atan2f */
        hex(u(FN1(0x0028F728)(f(c))));            /* asinf */
        hex(u(FN1(0x0028F710)(f(c))));            /* acosf */
        hex(u(FN1(0x0028F770)(f(a & 0x7FFFFFFF)))); /* sqrtf */
        hex(u(FN2(0x0028F758)(f(a & 0x7FFFFFFF), f(c)))); /* powf */
        hex(u(FN1(0x0028F4C0)(f(a))));            /* floorf */
        putc_('\n');
    }
    str("PS2LIBM END\n");
    return 0;
}
