/* Minimal <math.h> for building the newlib float sources with the PS2 compiler in the test program. */
#ifndef EE_SHIM_MATH_H
#define EE_SHIM_MATH_H
#define _IEEE_LIBM 1
#define _DEFUN(name, arglist, args) name(args)
#define _AND ,
#define _CONST const
#define _PARAMS(paramlist) paramlist
#define _EXFUN(name, proto) name proto
typedef int __int32_t;
typedef unsigned int __uint32_t;
#define HUGE_VAL (1.0e300 * 1.0e300)
extern float sinf(float), cosf(float), tanf(float), atanf(float), floorf(float), fabsf(float), scalbnf(float, int), copysignf(float, float);
#endif
