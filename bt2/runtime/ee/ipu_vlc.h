// MPEG-1/2 variable-length header codes as the IPU's VDEC command reports them
// (docs/plans/video/IPU_PLAN.md). Code tables: ISO/IEC 13818-2 Annex B (B-1 to B-4, B-10, B-11).
// The reported words follow the behaviour of PCSX2's IPU (reference, not copied), which runs this game.
#pragma once
#include <cstdint>

struct IpuVlc {
    std::uint32_t result=0; // IPU_CMD value; 0 reports a code error (ECD).
    unsigned consumed=0;    // Bits to remove from the stream.
};

// Each function takes the next 16 stream bits (MSB first, in the low 16 bits of `bits`).
IpuVlc ipuAddressIncrement(std::uint32_t bits,bool mpeg1); // 1-33; escape reports 35, MPEG-1 stuffing 34.
IpuVlc ipuMacroblockType(std::uint32_t bits,unsigned picture_type); // PCT 1 (I), 2 (P), 3 (B); flags below.
IpuVlc ipuMotionCode(std::uint32_t bits);                  // -16..16, sign-extended to 32 bits.
IpuVlc ipuDmVector(std::uint32_t bits);                    // -1, 0, 1.

// Macroblock type flags; bit 7 marks frame-based prediction.
enum : std::uint32_t {ipu_mb_intra=1,ipu_mb_pattern=2,ipu_mb_backward=4,ipu_mb_forward=8,ipu_mb_quant=16,ipu_mb_frame_motion=0x80};
