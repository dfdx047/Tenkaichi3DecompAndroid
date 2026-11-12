/*
 * Forced in front of the newlib 1.10.0 float maths sources (the library the PS2 executable was linked with:
 * same functions, thresholds and constants in its disassembly). The sources are unmodified; this supplies the
 * few newlib-internal macros and types they expect, so they build with the host's headers. They are compiled
 * with software float like the game, so their arithmetic is the PS2 float model.
 */
#ifndef NEWLIB_SHIM_H
#define NEWLIB_SHIM_H
#include <stdint.h>
#include <math.h>
#include "port_libm.h"
#define _IEEE_LIBM 1
#define _DEFUN(name, arglist, args) name(args)
#define _AND ,
#define _CONST const
#define _PARAMS(paramlist) paramlist
#define _EXFUN(name, proto) name proto
#ifndef __int32_t
#define __int32_t int32_t
#define __uint32_t uint32_t
#endif
/* internal names used across the files */
#define fabsf Port_fabsf
#define scalbnf Port_scalbnf
#define copysignf Port_copysignf
#endif
