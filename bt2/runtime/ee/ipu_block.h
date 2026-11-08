// MPEG-1/2 macroblock decoding for the IPU's BDEC command (docs/plans/video/IPU_PLAN.md): DC and AC
// coefficient codes, inverse quantisation, inverse DCT. Written from ISO/IEC 11172-2 and 13818-2
// (tables B-9, B-12 to B-15; scans; inverse quantisation arithmetic).
#pragma once
#include "ipu_input.h"
#include <array>
#include <cstdint>
#include <stdexcept>

struct IpuBlockParameters {
    bool intra=false;           // MBI: all six blocks are intra; otherwise a coded block pattern follows.
    bool mpeg1=false;           // MP1.
    bool alternate_scan=false;  // AS.
    bool intra_table_one=false; // IVF (MPEG-2): intra blocks use DCT coefficients table one.
    bool field_dct=false;       // DT: luma blocks hold alternate lines.
    unsigned dc_precision=0;    // IDP: 0-3 for 8-11 bits.
    unsigned quantiser_scale=0; // From QSC (ipuQuantiserScale).
    const std::uint8_t* intra_matrix=nullptr;    // SETIQ tables, in scan order.
    const std::uint8_t* nonintra_matrix=nullptr;
};

// The input ran out inside a macroblock.
struct IpuInputDry : std::runtime_error {IpuInputDry():std::runtime_error("IPU input ran dry inside a macroblock"){}};

// RAW16 macroblock: Y 16x16, Cb 8x8, Cr 8x8, row by row. Intra samples are 0..255, others residuals.
using IpuMacroblock=std::array<std::int16_t,384>;

// Decodes one macroblock at the bit pointer. `dc_predictors` (Y, Cb, Cr) carry over between intra
// macroblocks. Throws IpuInputDry when the input ends, std::runtime_error for an invalid code.
void ipuDecodeMacroblock(IpuInput& input,const IpuBlockParameters& parameters,std::array<int,3>& dc_predictors,IpuMacroblock& out);
// quantiser_scale_code (1-31) to the scale: twice the code, or the non-linear table (QST).
unsigned ipuQuantiserScale(unsigned code,bool nonlinear);
// 8x8 inverse DCT of coefficients in row order (index = row*8 + column), rounded to nearest.
void ipuInverseDct(std::array<int,64>& block);
