#ifndef SYS_MATHF_H
#define SYS_MATHF_H

#include "types.h"

/* Scalar float maths helpers, src/sys/mathf.c = 0x11F548..0x11F7D8. See the comment at the top of mathf.c. */

#define MATHF_PI 3.14159265f      /* 0x40490FDA */
#define MATHF_HALF_PI 1.57079633f /* 0x3FC90FDA */
#define MATHF_TWO_PI 6.2831853f   /* 0x40C90FDA */

f32 Mathf_WrapAngle(f32 angle, f32 half);
f32 Mathf_Sin(f32 angle);
f32 Mathf_SinFast(f32 angle);
f32 Mathf_Cos(f32 angle);
f32 Mathf_CosFast(f32 angle);
f32 Mathf_Tan(f32 angle);
f32 Mathf_Sqrt(f32 x);
void Mathf_SinCos(f32 *out, f32 angle);
f32 Mathf_Asin(f32 x);
f32 Mathf_Acos(f32 x);
f32 Mathf_Atan(f32 x);

#endif
