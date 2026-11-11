#ifndef GS_INDEX_BOUNDS_H
#define GS_INDEX_BOUNDS_H
#include <stdint.h>
#if defined(__SSE2__)
#include <emmintrin.h>
#endif

/* A GS block is exactly 256 bytes. This computes the same index interval as
   the scalar decoder, without adding texture residency or another cache. */
static inline void Gs_IndexBounds(const uint8_t *p, int fourBit,
                                 uint32_t *lo, uint32_t *hi) {
    uint32_t low = 255, high = 0, i;
#if defined(__SSE2__)
    __m128i mn = _mm_set1_epi8((char)255), mx = _mm_setzero_si128();
    const __m128i mask = _mm_set1_epi8(15);
    uint8_t mins[16], maxs[16];
    for (i = 0; i < 256; i += 16) {
        __m128i value = _mm_loadu_si128((const __m128i *)(p + i));
        if (fourBit) {
            __m128i a = _mm_and_si128(value, mask);
            __m128i b = _mm_and_si128(_mm_srli_epi16(value, 4), mask);
            mn = _mm_min_epu8(mn, _mm_min_epu8(a, b));
            mx = _mm_max_epu8(mx, _mm_max_epu8(a, b));
        } else {
            mn = _mm_min_epu8(mn, value);
            mx = _mm_max_epu8(mx, value);
        }
    }
    _mm_storeu_si128((__m128i *)mins, mn);
    _mm_storeu_si128((__m128i *)maxs, mx);
    for (i = 0; i < 16; ++i) {
        if (mins[i] < low) low = mins[i];
        if (maxs[i] > high) high = maxs[i];
    }
#else
    for (i = 0; i < 256; ++i) {
        uint32_t a = fourBit ? p[i] & 15u : p[i];
        if (a < low) low = a;
        if (a > high) high = a;
        if (fourBit) {
            uint32_t b = p[i] >> 4;
            if (b < low) low = b;
            if (b > high) high = b;
        }
    }
#endif
    if (low < *lo) *lo = low;
    if (high > *hi) *hi = high;
}
#endif
