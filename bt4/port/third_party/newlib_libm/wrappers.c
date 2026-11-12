/* acosf, asinf, atan2f, powf, sqrtf as the PS2 executable has them: plain jumps to the __ieee754 routines
   (newlib's wf_*.c built with _IEEE_LIBM). */
#include "fdlibm.h"
float acosf(float x) { return __ieee754_acosf(x); }
float asinf(float x) { return __ieee754_asinf(x); }
float atan2f(float y, float x) { return __ieee754_atan2f(y, x); }
float powf(float x, float y) { return __ieee754_powf(x, y); }
float sqrtf(float x) { return __ieee754_sqrtf(x); }
