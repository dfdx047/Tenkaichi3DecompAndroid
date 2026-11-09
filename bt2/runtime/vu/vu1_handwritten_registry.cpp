// Hand-written native VU1 programs (docs/plans/ee/PERFORMANCE_PLAN.md, "Hand-written native
// programs"). Each is one of the game's microprograms written out as what it does: values in SSE
// registers, no per-pair bookkeeping. It sends the same packets and leaves the same state behind
// as the interpreter (data memory, the registers, ACC, Q, I, the clipping flags, the pair count);
// `BT2_VU1_VERIFY=1` checks that on every start. A program is chosen by its key (vu1_program.h).
//
// Why plain SSE operations are enough: a run computes with the float mode of vu1.cpp (round toward
// zero, denormals read as zero, tiny results flushed). In that mode sums and products of finite
// operands are finite, so only values loaded from data memory or left in registers by an earlier
// program need the model's treatment of infinities and NaNs (`clamp`), once, where they are read.
//
// The shared helpers are in vu1_handwritten_helpers.h; the programs in vu1_skinned_vertex_programs.cpp, vu1_triangle_strip_program.cpp and
// vu1_triangle_clipper.cpp. This file is the registry: which key runs which program.
#include "vu1_handwritten_helpers.h"

std::span<const Vu1::NativeEntry> Vu1::handWrittenPrograms() {
    static constexpr NativeEntry programs[]={
        {0x8324022f7a30fcd8ull,&Vu1Hand::skinnedTwoListsJump},
        {0xa3c20ea041a949a6ull,&Vu1Hand::skinnedTwoListsDirect},
        {0xde35423c212c6658ull,&Vu1Hand::skinnedOneListJump},
        {0x47b403bacf5a8a1eull,&Vu1Hand::skinnedOneListDirect},
        {0x1657b0b5391ac242ull,&Vu1Hand::clippedStripJump},
        {0xa728e08140ca9392ull,&Vu1Hand::clippedStripDirect},
    };
    return programs;
}
