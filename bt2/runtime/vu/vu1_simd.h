// Four-lane helpers of the VU1 model, shared by the interpreter (vu1.cpp) and the native
// translations of microprograms. SSE2 (on ARM through sse2neon, platform/sse; the hottest ones in NEON
// directly). A register is four 32-bit lanes (x, y, z, w) in memory order; an instruction's dest mask
// selects lane n with bit 8>>n.
#pragma once
#include <array>
#include <cstdint>
#include <emmintrin.h>

namespace vu1_simd {
// One float as the model reads it: denormals as signed zero; infinities and NaNs as the largest
// magnitude of their sign.
inline std::uint32_t normal(std::uint32_t bits) {
    const auto exponent=bits&0x7F800000u;
    return !exponent?bits&0x80000000u:exponent==0x7F800000u?(bits&0x80000000u)|0x7F7FFFFFu:bits;
}
// Infinities and NaNs become the largest magnitude of their sign. Denormals are handled by the run's
// float mode (MXCSR: round toward zero, denormals read as zero, tiny results flushed to zero).
// (AArch64: four NEON instructions where the SSE form makes seven: a bit select instead of and / andnot / or,
// bit for bit the same result. It runs three times for every arithmetic instruction of a microprogram.)
#if defined(__aarch64__)
inline __m128 clamp(__m128 v) {
    const uint32x4_t x=vreinterpretq_u32_m128(v),exponent=vdupq_n_u32(0x7F800000u);
    const uint32x4_t special=vceqq_u32(vandq_u32(x,exponent),exponent);
    const uint32x4_t largest=vbslq_u32(vdupq_n_u32(0x80000000u),x,vdupq_n_u32(0x7F7FFFFFu)); // x's sign, the rest 0x7F7FFFFF
    return vreinterpretq_m128_u32(vbslq_u32(special,largest,x));
}
#else
inline __m128 clamp(__m128 v) {
    const __m128i x=_mm_castps_si128(v),exponent=_mm_set1_epi32(0x7F800000);
    const __m128i special=_mm_cmpeq_epi32(_mm_and_si128(x,exponent),exponent);
    const __m128i largest=_mm_or_si128(_mm_and_si128(x,_mm_set1_epi32(int(0x80000000u))),_mm_set1_epi32(0x7F7FFFFF));
    return _mm_castsi128_ps(_mm_or_si128(_mm_andnot_si128(special,x),_mm_and_si128(special,largest)));
}
#endif
inline __m128 lanes(const std::array<std::uint32_t,4>& v) {return _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(v.data())));}
inline __m128i laneMask(unsigned mask) {
    static constexpr auto table=[] {
        std::array<std::array<std::int32_t,4>,16> t{};
        for(unsigned m=0;m<16;++m)for(unsigned lane=0;lane<4;++lane)t[m][lane]=m&(8u>>lane)?-1:0;
        return t;
    }();
    return _mm_loadu_si128(reinterpret_cast<const __m128i*>(table[mask&15].data()));
}
#if defined(__aarch64__) // (one bit select)
inline __m128i select(__m128i keep,__m128i take,__m128i mask) {
    return vreinterpretq_m128i_u32(vbslq_u32(vreinterpretq_u32_m128i(mask),vreinterpretq_u32_m128i(take),vreinterpretq_u32_m128i(keep)));
}
#else
inline __m128i select(__m128i keep,__m128i take,__m128i mask) {return _mm_or_si128(_mm_andnot_si128(mask,keep),_mm_and_si128(mask,take));}
#endif
inline __m128i load4(const void* at) {return _mm_loadu_si128(static_cast<const __m128i*>(at));}
inline void store4(void* at,__m128i v) {_mm_storeu_si128(static_cast<__m128i*>(at),v);}
// `take` into the four lanes at `at`, the lanes of `mask` only (a constant in translated code).
inline void storeMasked(void* at,__m128i take,unsigned mask) {
    if(mask==15)store4(at,take);else store4(at,select(load4(at),take,laneMask(mask)));
}
}
