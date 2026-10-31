#pragma once
#include "services/devices/dualshock2.h"
#include <array>
#include <cstdint>

// The 128-byte pad block DBCMAN writes to a slot buffer each VBlank (traced 2026-10-11): a connected
// DualShock 2 in pressure mode, in the DualShock 2 report layout. Word 31 counts transfers; the very
// first transfer (detection still settling) is all zero.
inline std::array<std::uint32_t,32> nativePadBlock(const DualShock2& pad,unsigned transfer) {
    std::array<std::uint32_t,32> block{};
    if(!transfer)return block;
    // Pressure bytes: right, left, up, down, triangle, circle, cross, square, L1, R1, L2, R2.
    constexpr std::array<unsigned,12> pressure_bits{5,7,4,6,12,13,14,15,10,11,8,9};
    std::array<std::uint8_t,12> pressure;
    for(unsigned i=0;i<12;++i)pressure[i]=(pad.buttons>>pressure_bits[i])&1?0:0xFF;
    const auto bytes4=[](std::uint8_t a,std::uint8_t b,std::uint8_t c,std::uint8_t d){return a|b<<8|c<<16|std::uint32_t(d)<<24;};
    block[0]=0x04121601;block[1]=1; // Connected DualShock 2, pressure mode.
    block[7]=bytes4(pad.buttons&0xFF,pad.buttons>>8,pad.sticks[0],pad.sticks[1]); // Buttons (active low), right stick.
    block[8]=bytes4(pad.sticks[2],pad.sticks[3],pressure[0],pressure[1]);         // Left stick, pressures.
    block[9]=bytes4(pressure[2],pressure[3],pressure[4],pressure[5]);
    block[10]=bytes4(pressure[6],pressure[7],pressure[8],pressure[9]);
    block[11]=bytes4(pressure[10],pressure[11],0xFF,0xFF);block[12]=0xFFFF;
    block[31]=transfer;
    return block;
}
