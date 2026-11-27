// Game functions replaced by host code where it pays off (docs/plans/ee/RESPONSIVENESS_PLAN.md step 5): the
// same memory, registers and logical time as the generated code, faster. Each replacement is installed only
// when the guest code it stands for hashes as expected (no game bytes are kept here), and runs the
// generated function whenever its fast path does not apply.
#pragma once
#include <cstdint>
#include <span>

class EeRuntime;

// Installs the replacements whose guest code matches (call after the image is loaded and the generated
// functions are registered); returns how many were installed.
unsigned registerNativeFunctions(EeRuntime& runtime);

// FNV-1a over guest code bytes: what a replacement checks the code it stands for against.
std::uint64_t nativeCodeHash(std::span<const std::uint8_t> code);

// Tests (native_function_tests.cpp): the memcpy replacement over a registered stand-in for the generated
// function, without the code check; its address.
std::uint32_t installNativeMemcpyForTest(EeRuntime& runtime);
