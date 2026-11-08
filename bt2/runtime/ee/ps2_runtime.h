#pragma once

// What generated code sees of the runtime: the CPU context and the entry points it calls (directly or
// through ps2_runtime_macros.h). Every generated unit includes this header, so an edit recompiles all
// of them; keep it to this interface. The implementation is EeRuntime (ee_runtime.h), which derives
// from PS2Runtime; guest_interface.cpp forwards each entry point to it.
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

struct alignas(16) ProbeGpr {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    operator __m128i() const {
        __m128i value;
        std::memcpy(&value, this, sizeof(value));
        return value;
    }
};

struct R5900Context {
    std::array<ProbeGpr, 32> r{};
    std::uint32_t pc = 0;
    std::uint64_t hi = 0, lo = 0, hi1 = 0, lo1 = 0;
    std::uint32_t sa = 0, fcr31 = 0, cop0_status = 0, branch_pc = 0;
    std::uint32_t cop0_epc = 0, cop0_errorepc = 0;
    // Generated ABI storage only; VIF/VU reset execution remains gated at first MMIO.
    std::uint32_t vu0_fbrst = 0;
    bool ll_bit = false;
    bool in_delay_slot = false;
    std::array<float, 32> f{};
    float f_acc = 0;
    // VU0 float registers for COP2 macro-mode instructions in generated code (x in the lowest lane).
    // VF0 is the constant (0, 0, 0, 1).
    __m128 vu0_vf[32] = {_mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f)};
    __m128 vu0_acc = _mm_setzero_ps(); // ACC, the multiply-accumulate register.
    float vu0_q = 0;                   // Q, the divide/square-root result (written before the game reads it).
    __m128 vu0_r = _mm_setzero_ps();   // R, the random number register.
    std::uint16_t vi[16] = {};         // VU0 integer registers.
    std::uint32_t vu0_clip_flags = 0;  // VCLIP results (computed by generated code).
    // Generated ABI storage for MTC0 Wired in the game's unreached TLB setup; no TLB is modelled.
    std::uint32_t cop0_wired = 0, cop0_random = 47;
};

enum PS2Exception {EXCEPTION_INTEGER_OVERFLOW=0x0C};

class PS2Runtime {
public:
    enum class GuestBranchKind { Return, DirectCall, IndirectCall, TailJump, IndirectJump=TailJump, DirectJump=TailJump };
    using RecompiledFunction = void (*)(std::uint8_t*, R5900Context*, PS2Runtime*);
    [[noreturn]] void SignalException(R5900Context* context, PS2Exception exception);
    bool dispatchGuestBranch(std::uint8_t* rdram, R5900Context* context, std::uint32_t target, std::uint32_t source, std::uint32_t resume, GuestBranchKind kind, const char* reason);
    bool eeCheckpointDue();
    void handleSyscall(std::uint8_t* rdram, R5900Context* context, std::uint32_t code);
    void handleBreak(std::uint8_t* rdram, R5900Context* context);
    std::uint8_t read8(std::uint8_t* rdram, std::uint32_t address);
    std::uint16_t read16(std::uint8_t* rdram, std::uint32_t address);
    std::uint64_t read64(std::uint8_t* rdram, std::uint32_t address);
    ProbeGpr read128(std::uint8_t* rdram, std::uint32_t address);
    void write8(std::uint8_t* rdram, std::uint32_t address, std::uint8_t value);
    void write16(std::uint8_t* rdram, std::uint32_t address, std::uint16_t value);
    void write128(std::uint8_t* rdram, std::uint32_t address, const ProbeGpr& value);
    std::uint64_t guestRead64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address);
    void guestWrite64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value);
    void guestWrite128(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, const ProbeGpr& value);
    std::uint32_t Load32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address);
    void Store32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint32_t value);
    std::uint64_t Load64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address);
    void Store64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value);
    void Store128(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, const ProbeGpr& value);
    void dataCacheLine(std::uint8_t* rdram, std::uint32_t address);
    void indexedDataCache(std::uint8_t* rdram, R5900Context* context, std::uint32_t operation, std::uint32_t address);
    std::uint32_t readCacheTagLo(R5900Context* context);
    bool cop2Condition(R5900Context* context);
    std::uint32_t readVuStatus(R5900Context* context);
    std::uint32_t readVuStatusFlag(R5900Context* context);
    void writeVuStatusFlag(R5900Context* context, std::uint32_t value);
    void writeVuFbrst(R5900Context* context, std::uint32_t value);
    void vu0UpdateFlags(R5900Context* context, std::uint32_t instruction);
    void vu0MemoryInstruction(R5900Context* context, std::uint32_t instruction);
    void vu0RandomInstruction(R5900Context* context, std::uint32_t instruction);
    void clearLLBit(R5900Context* context) { context->ll_bit = false; }
    // Vector register stores (VU0 macro mode: SQC2) pass the lanes as an integer vector.
    void write128(std::uint8_t* rdram, std::uint32_t address, __m128i value) { write128(rdram, address, std::bit_cast<ProbeGpr>(value)); }
    void guestWrite128(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, __m128i value) { guestWrite128(rdram, context, address, std::bit_cast<ProbeGpr>(value)); }
protected:
    PS2Runtime()=default; // Only as the base of EeRuntime.
    ~PS2Runtime()=default;
};
