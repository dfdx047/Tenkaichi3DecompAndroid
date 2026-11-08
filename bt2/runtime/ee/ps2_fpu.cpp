// Authored R5900 COP1 single-precision semantics for generated probes.
// The EE FPU is not IEEE 754: it rounds toward zero only, has no infinities, NaNs or
// denormals (denormal operands/results become signed zero; overflow clamps to the largest
// finite magnitude) and float-to-word conversion truncates and saturates. Reached operations
// only; flag (O/U/I/D) and accumulator side effects other than ACC writes are not modeled.
#include "ps2_fpu.h"
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <immintrin.h>
#pragma fenv_access(on)

namespace {
float normalize(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto exponent = bits & 0x7F800000u, sign = bits & 0x80000000u;
    if (!exponent) return std::bit_cast<float>(sign);
    if (exponent == 0x7F800000u) return std::bit_cast<float>(sign | 0x7F7FFFFFu);
    return value;
}
template <class Operation>
float chop(Operation operation) {
    // Round toward zero for the one operation, set in MXCSR directly: the C library's rounding
    // functions cost several times the operation itself.
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(saved | 0x6000u);
    const volatile float result = operation();
    _mm_setcsr(saved);
    return normalize(result);
}

#if defined(__aarch64__)
// AArch64: the same in FPCR itself. On ARM the MXCSR calls above go through the x86 intrinsics' emulation
// (sse2neon): fegetround and fesetround in libm and four FPCR writes per operation, about 3% of the game
// thread in a fight on Android (measured). Here: one read, the rounding mode set to toward zero (RMode,
// bits 22-23, = 3), the operation, the register put back; nothing else in FPCR changes (MXCSR's flush-to-zero
// bit was written back as it was, too). The operands pass through the empty asm statements after the mode is
// set, and the result before it is put back, so the compiler keeps the operation inside.
// (BT2_PERF_BASELINE: the MXCSR path, to measure against.)
bool directRounding() {static const bool on = std::getenv("BT2_PERF_BASELINE") == nullptr;return on;}
struct TowardZero {
    std::uint64_t saved;
    TowardZero() {
        __asm__ __volatile__("mrs %0, fpcr" : "=r"(saved));
        __asm__ __volatile__("msr fpcr, %0" ::"r"(saved | (std::uint64_t(3) << 22)) : "memory");
    }
    ~TowardZero() {__asm__ __volatile__("msr fpcr, %0" ::"r"(saved) : "memory");}
};
template <class T> T inside(T value) {
    if constexpr (std::is_floating_point_v<T>) __asm__ __volatile__("" : "+w"(value));
    else __asm__ __volatile__("" : "+r"(value));
    return value;
}
template <class Operation, class... Operands>
float chopOperands(Operation operation, Operands... operands) {
    if (!directRounding()) return chop([&] { return operation(operands...); });
    float result;
    {
        const TowardZero mode;
        result = inside(operation(inside(operands)...));
    }
    return normalize(result);
}
#else
template <class Operation, class... Operands>
float chopOperands(Operation operation, Operands... operands) {
    return chop([&] { return operation(operands...); });
}
#endif
}

float ps2_fpu_add(float left, float right) {
    return chopOperands([](float a, float b) { return a + b; }, normalize(left), normalize(right));
}
float ps2_fpu_sub(float left, float right) {
    return chopOperands([](float a, float b) { return a - b; }, normalize(left), normalize(right));
}
float ps2_fpu_sqrt(float value) {
    // SQRT.S uses |value| (a negative operand only raises the unmodeled invalid flag).
    const float a = std::bit_cast<float>(std::bit_cast<std::uint32_t>(normalize(value)) & 0x7FFFFFFFu);
    return chopOperands([](float x) { return std::sqrt(x); }, a);
}
float ps2_fpu_mul(float left, float right) {
    return chopOperands([](float a, float b) { return a * b; }, normalize(left), normalize(right));
}
float ps2_fpu_cvt_s_w(std::int32_t value) {
    return chopOperands([](std::int32_t x) { return static_cast<float>(x); }, value);
}
std::int32_t ps2_fpu_cvt_w_s(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    // Magnitudes below 2^31 truncate; larger (including clamped extremes) saturate by sign.
    if ((bits & 0x7F800000u) <= 0x4E800000u) return static_cast<std::int32_t>(normalize(value));
    return (bits & 0x80000000u) ? INT32_MIN : INT32_MAX;
}
bool ps2_fpu_c_eq(float left, float right) {
    return normalize(left) == normalize(right);
}
bool ps2_fpu_c_lt(float left, float right) {
    return normalize(left) < normalize(right);
}
bool ps2_fpu_c_le(float left, float right) {
    return normalize(left) <= normalize(right);
}
