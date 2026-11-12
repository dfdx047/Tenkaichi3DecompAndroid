/* Forced into every software-float source: the C maths library goes through the bridge in port/src/plat_libm.c
   (the host's own functions return their result in the float unit, which software-float code does not read). */
#ifndef PORT_LIBM_H
#define PORT_LIBM_H
float Port_sinf(float);
float Port_cosf(float);
float Port_tanf(float);
float Port_asinf(float);
float Port_acosf(float);
float Port_atanf(float);
float Port_sqrtf(float);
float Port_floorf(float);
float Port_ceilf(float);
float Port_fabsf(float);
float Port_expf(float);
float Port_logf(float);
float Port_atan2f(float, float);
float Port_powf(float, float);
float Port_fmodf(float, float);
double Port_pow(double, double); /* the one double function the game calls (the replay list's slot number) */
#define sinf Port_sinf
#define cosf Port_cosf
#define tanf Port_tanf
#define asinf Port_asinf
#define acosf Port_acosf
#define atanf Port_atanf
#define atan2f Port_atan2f
#define sqrtf Port_sqrtf
#define powf Port_powf
#define floorf Port_floorf
#define ceilf Port_ceilf
#define fabsf Port_fabsf
#define fmodf Port_fmodf
#define expf Port_expf
#define logf Port_logf
#define pow Port_pow
#define Ref_atan2f Port_atan2f /* the vector library's hook for the game's atan2f */
/* The PS2 C library's generator (port/src/plat_sys.c), not the host's. */
#define rand Port_Rand
#define srand Port_Srand
#endif
