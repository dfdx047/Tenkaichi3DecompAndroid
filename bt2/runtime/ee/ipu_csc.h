// IPU colour space conversion (CSC command, docs/plans/video/IPU_PLAN.md): one RAW8 macroblock
// (Y 16x16, Cb 8x8, Cr 8x8 bytes) to 16x16 RGB32 pixels (R, G, B, A bytes), with the SETTH alpha
// thresholds. Uses the IPU's fixed coefficients (Y 0x95, Cr->R 0xCC, Cr->G -0x68, Cb->G -0x32,
// Cb->B 0x102, six fraction bits, luma bias 16).
#pragma once
#include <cstdint>

inline constexpr unsigned ipu_raw8_bytes=384,ipu_rgb32_bytes=1024;

// `thresholds`: TH0 in bits 0-8, TH1 in bits 16-24. A pixel with all components below TH0 becomes
// transparent black; otherwise one with all components below TH1 gets alpha 0x40; others 0x80.
void ipuConvertMacroblock(const std::uint8_t* raw8,std::uint32_t thresholds,std::uint8_t* rgb32);
