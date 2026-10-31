// Generated code's entry points (ps2_runtime.h), forwarded to the full runtime. PS2Runtime exists only
// as the base of EeRuntime, and EeRuntime declares every one of these names itself.
#include "ee_runtime.h"

namespace {
inline EeRuntime* full(PS2Runtime* runtime) {return static_cast<EeRuntime*>(runtime);}
}

void PS2Runtime::SignalException(R5900Context* context, PS2Exception exception) {full(this)->EeRuntime::SignalException(context, exception);}
bool PS2Runtime::dispatchGuestBranch(std::uint8_t* rdram, R5900Context* context, std::uint32_t target, std::uint32_t source, std::uint32_t resume, GuestBranchKind kind, const char* reason) {return full(this)->EeRuntime::dispatchGuestBranch(rdram, context, target, source, resume, kind, reason);}
bool PS2Runtime::eeCheckpointDue() {return full(this)->EeRuntime::eeCheckpointDue();}
void PS2Runtime::handleSyscall(std::uint8_t* rdram, R5900Context* context, std::uint32_t code) {full(this)->EeRuntime::handleSyscall(rdram, context, code);}
void PS2Runtime::handleBreak(std::uint8_t* rdram, R5900Context* context) {full(this)->EeRuntime::handleBreak(rdram, context);}
std::uint8_t PS2Runtime::read8(std::uint8_t* rdram, std::uint32_t address) {return full(this)->EeRuntime::read8(rdram, address);}
std::uint16_t PS2Runtime::read16(std::uint8_t* rdram, std::uint32_t address) {return full(this)->EeRuntime::read16(rdram, address);}
std::uint64_t PS2Runtime::read64(std::uint8_t* rdram, std::uint32_t address) {return full(this)->EeRuntime::read64(rdram, address);}
ProbeGpr PS2Runtime::read128(std::uint8_t* rdram, std::uint32_t address) {return full(this)->EeRuntime::read128(rdram, address);}
void PS2Runtime::write8(std::uint8_t* rdram, std::uint32_t address, std::uint8_t value) {full(this)->EeRuntime::write8(rdram, address, value);}
void PS2Runtime::write16(std::uint8_t* rdram, std::uint32_t address, std::uint16_t value) {full(this)->EeRuntime::write16(rdram, address, value);}
void PS2Runtime::write128(std::uint8_t* rdram, std::uint32_t address, const ProbeGpr& value) {full(this)->EeRuntime::write128(rdram, address, value);}
std::uint64_t PS2Runtime::guestRead64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {return full(this)->EeRuntime::guestRead64(rdram, context, address);}
void PS2Runtime::guestWrite64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value) {full(this)->EeRuntime::guestWrite64(rdram, context, address, value);}
std::uint32_t PS2Runtime::Load32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {return full(this)->EeRuntime::Load32(rdram, context, address);}
void PS2Runtime::Store32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint32_t value) {full(this)->EeRuntime::Store32(rdram, context, address, value);}
std::uint64_t PS2Runtime::Load64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {return full(this)->EeRuntime::Load64(rdram, context, address);}
void PS2Runtime::Store64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value) {full(this)->EeRuntime::Store64(rdram, context, address, value);}
void PS2Runtime::guestWrite128(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, const ProbeGpr& value) {full(this)->EeRuntime::guestWrite128(rdram, context, address, value);}
void PS2Runtime::Store128(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, const ProbeGpr& value) {full(this)->EeRuntime::Store128(rdram, context, address, value);}
void PS2Runtime::dataCacheLine(std::uint8_t* rdram, std::uint32_t address) {full(this)->EeRuntime::dataCacheLine(rdram, address);}
void PS2Runtime::indexedDataCache(std::uint8_t* rdram, R5900Context* context, std::uint32_t operation, std::uint32_t address) {full(this)->EeRuntime::indexedDataCache(rdram, context, operation, address);}
std::uint32_t PS2Runtime::readCacheTagLo(R5900Context* context) {return full(this)->EeRuntime::readCacheTagLo(context);}
bool PS2Runtime::cop2Condition(R5900Context* context) {return full(this)->EeRuntime::cop2Condition(context);}
std::uint32_t PS2Runtime::readVuStatus(R5900Context* context) {return full(this)->EeRuntime::readVuStatus(context);}
std::uint32_t PS2Runtime::readVuStatusFlag(R5900Context* context) {return full(this)->EeRuntime::readVuStatusFlag(context);}
void PS2Runtime::writeVuStatusFlag(R5900Context* context, std::uint32_t value) {full(this)->EeRuntime::writeVuStatusFlag(context, value);}
void PS2Runtime::writeVuFbrst(R5900Context* context, std::uint32_t value) {full(this)->EeRuntime::writeVuFbrst(context, value);}
void PS2Runtime::vu0UpdateFlags(R5900Context* context, std::uint32_t instruction) {full(this)->EeRuntime::vu0UpdateFlags(context, instruction);}
void PS2Runtime::vu0MemoryInstruction(R5900Context* context, std::uint32_t instruction) {full(this)->EeRuntime::vu0MemoryInstruction(context, instruction);}
void PS2Runtime::vu0RandomInstruction(R5900Context* context, std::uint32_t instruction) {full(this)->EeRuntime::vu0RandomInstruction(context, instruction);}
