#include "ipu_vlc.h"
#include <cstddef>
#include <stdexcept>
#include <string>

namespace {
struct Code {std::uint16_t code;std::uint8_t length;std::uint8_t value;};
// Finds the entry whose `length`-bit code prefixes the 16 bits; nullptr when none does.
template<std::size_t N> const Code* match(const Code (&table)[N],std::uint32_t bits) {
    for(const auto& entry:table)if((bits>>(16-entry.length))==entry.code)return &entry;
    return nullptr;
}
// Table B-1, macroblock_address_increment 1..33.
constexpr Code increments[]={
    {1,1,1},{3,3,2},{2,3,3},{3,4,4},{2,4,5},{3,5,6},{2,5,7},{7,7,8},{6,7,9},
    {11,8,10},{10,8,11},{9,8,12},{8,8,13},{7,8,14},{6,8,15},
    {23,10,16},{22,10,17},{21,10,18},{20,10,19},{19,10,20},{18,10,21},
    {35,11,22},{34,11,23},{33,11,24},{32,11,25},{31,11,26},{30,11,27},{29,11,28},{28,11,29},{27,11,30},{26,11,31},{25,11,32},{24,11,33}};
constexpr std::uint8_t I=ipu_mb_intra,P=ipu_mb_pattern,B=ipu_mb_backward,F=ipu_mb_forward,Q=ipu_mb_quant;
// Tables B-2 (I), B-3 (P) and B-4 (B), macroblock_type.
constexpr Code intra_types[]={{1,1,I},{1,2,std::uint8_t(Q|I)}};
constexpr Code predicted_types[]={{1,1,std::uint8_t(F|P)},{1,2,P},{1,3,F},{3,5,I},{2,5,std::uint8_t(Q|F|P)},{1,5,std::uint8_t(Q|P)},{1,6,std::uint8_t(Q|I)}};
constexpr Code bidirectional_types[]={{2,2,std::uint8_t(F|B)},{3,2,std::uint8_t(F|B|P)},{2,3,B},{3,3,std::uint8_t(B|P)},{2,4,F},{3,4,std::uint8_t(F|P)},
    {3,5,I},{2,5,std::uint8_t(Q|F|B|P)},{3,6,std::uint8_t(Q|F|P)},{2,6,std::uint8_t(Q|B|P)},{1,6,std::uint8_t(Q|I)}};
// Table B-10, motion_code magnitudes 1..16 (the sign bit follows the code).
constexpr Code motion_codes[]={{1,2,1},{1,3,2},{1,4,3},{3,6,4},{5,7,5},{4,7,6},{3,7,7},{11,9,8},{10,9,9},{9,9,10},
    {17,10,11},{16,10,12},{15,10,13},{14,10,14},{13,10,15},{12,10,16}};
}

IpuVlc ipuAddressIncrement(std::uint32_t bits,bool mpeg1) {
    bits&=0xFFFF;
    if(const auto* entry=match(increments,bits))return {std::uint32_t(entry->value)|(std::uint32_t(entry->length)<<16),entry->length};
    if((bits>>5)==8)return {0x000B0023,11};          // macroblock_escape.
    if((bits>>5)==15 && mpeg1)return {0x000B0022,11}; // macroblock_stuffing (MPEG-1 only).
    return {};
}

IpuVlc ipuMacroblockType(std::uint32_t bits,unsigned picture_type) {
    bits&=0xFFFF;
    const Code* entry=nullptr;
    switch(picture_type) {
    case 1: entry=match(intra_types,bits);break;
    case 2: entry=match(predicted_types,bits);break;
    case 3: entry=match(bidirectional_types,bits);break;
    default: throw std::runtime_error("IPU VDEC macroblock type for picture type "+std::to_string(picture_type)+" is not modelled");
    }
    if(!entry)return {};
    std::uint32_t result=entry->value;
    // Frame prediction is implied (frame_pred_frame_dct): P pictures flag it with forward motion, B
    // pictures always. Only B pictures report the code length in the upper half.
    if(picture_type==2 && (result&ipu_mb_forward))result|=ipu_mb_frame_motion;
    if(picture_type==3)result|=ipu_mb_frame_motion|(std::uint32_t(entry->length)<<16);
    return {result,entry->length};
}

IpuVlc ipuMotionCode(std::uint32_t bits) {
    bits&=0xFFFF;
    if(bits&0x8000)return {0x00010000,1}; // '1': zero.
    const auto* entry=match(motion_codes,bits);
    if(!entry)throw std::runtime_error("IPU VDEC motion code is invalid");
    const bool negative=(bits>>(15-entry->length))&1;
    const std::int32_t value=negative?-std::int32_t(entry->value):std::int32_t(entry->value);
    // The length counts the code without its sign bit; a negative value fills the upper half.
    return {std::uint32_t(value)|(std::uint32_t(entry->length)<<16),unsigned(entry->length)+1};
}

IpuVlc ipuDmVector(std::uint32_t bits) {
    bits&=0xFFFF;
    if(!(bits&0x8000))return {0x00010000,1};                         // '0': 0.
    return (bits&0x4000)?IpuVlc{0xFFFFFFFFu,2}:IpuVlc{0x00020001,2}; // '11': -1, '10': 1.
}
