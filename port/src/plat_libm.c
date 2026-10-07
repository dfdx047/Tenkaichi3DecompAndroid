/*
 * The only file of the PC build compiled with the host's float unit. It bridges the software-float game code
 * (floats as 32-bit patterns, doubles as 64-bit patterns, both in integer registers) to:
 *   - the few C maths functions that are not built from the PS2's library sources (see below);
 *   - double arithmetic, which is IEEE software arithmetic on the PS2 too, so the host's is the same.
 * Every function takes and returns integers, so the two calling conventions cannot be confused.
 */
#include <math.h>
#include <stdint.h>
#include <string.h>

static float f(uint32_t u) { float x; __builtin_memcpy(&x, &u, 4); return x; }
static uint32_t u(float x) { uint32_t v; __builtin_memcpy(&v, &x, 4); return v; }
static double d(uint64_t v) { double x; __builtin_memcpy(&x, &v, 8); return x; }
static uint64_t q(double x) { uint64_t v; __builtin_memcpy(&v, &x, 8); return v; }

#define F1(name) uint32_t Port_##name(uint32_t a) { return u(name(f(a))); }
#define F2(name) uint32_t Port_##name(uint32_t a, uint32_t b) { return u(name(f(a), f(b))); }
/* sinf, cosf, tanf, asinf, acosf, atanf, atan2f, sqrtf, powf, floorf, fabsf are the PS2's own (newlib 1.10.0,
   port/third_party/newlib_libm, compiled with software float). What is left here is not called by the simulation. */
F1(ceilf) F1(expf) F1(logf)
F2(fmodf)

uint64_t __adddf3(uint64_t a, uint64_t b) { return q(d(a) + d(b)); }
uint64_t __subdf3(uint64_t a, uint64_t b) { return q(d(a) - d(b)); }
uint64_t __muldf3(uint64_t a, uint64_t b) { return q(d(a) * d(b)); }
uint64_t __divdf3(uint64_t a, uint64_t b) { return q(d(a) / d(b)); }
uint64_t __negdf2(uint64_t a) { return a ^ 0x8000000000000000ull; }
uint64_t __extendsfdf2(uint32_t a) { return q((double)f(a)); }
uint32_t __truncdfsf2(uint64_t a) { return u((float)d(a)); }
int32_t __fixdfsi(uint64_t a) { return (int32_t)d(a); }
uint32_t __fixunsdfsi(uint64_t a) { return (uint32_t)d(a); }
uint64_t __floatsidf(int32_t v) { return q((double)v); }
uint64_t __floatunsidf(uint32_t v) { return q((double)v); }
static int dcmp(uint64_t a, uint64_t b) { return d(a) < d(b) ? -1 : d(a) > d(b) ? 1 : 0; }
int __eqdf2(uint64_t a, uint64_t b) { return dcmp(a, b); }
int __nedf2(uint64_t a, uint64_t b) { return dcmp(a, b); }
int __ltdf2(uint64_t a, uint64_t b) { return dcmp(a, b); }
int __ledf2(uint64_t a, uint64_t b) { return dcmp(a, b); }
int __gtdf2(uint64_t a, uint64_t b) { return dcmp(a, b); }
int __gedf2(uint64_t a, uint64_t b) { return dcmp(a, b); }
int __unorddf2(uint64_t a, uint64_t b) { return d(a) != d(a) || d(b) != d(b); } /* clang asks for it; gcc does not */
