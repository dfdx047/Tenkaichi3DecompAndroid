// The x86 SIMD intrinsics on ARM (Android arm64): sse2neon (v1.9.1, MIT, LICENSE.sse2neon) with its
// precise variants, since the PS2's results are compared bit for bit with captures of the original.
// The headers beside this one stand in for the compiler's x86 headers; the ARM build puts this folder
// first on the include path (android/CMakeLists.txt), so neither the runtime nor the generated code
// changes. x86 builds never see this folder.
#pragma once
#if !defined(__aarch64__) && !defined(__arm__)
#error "platform/sse is for ARM builds only"
#endif
#ifndef SSE2NEON_PRECISE_MINMAX
#define SSE2NEON_PRECISE_MINMAX 1
#endif
#ifndef SSE2NEON_PRECISE_DIV
#define SSE2NEON_PRECISE_DIV 1
#endif
#ifndef SSE2NEON_PRECISE_SQRT
#define SSE2NEON_PRECISE_SQRT 1
#endif
#ifndef SSE2NEON_PRECISE_DP
#define SSE2NEON_PRECISE_DP 1
#endif
#include "sse2neon.h"
