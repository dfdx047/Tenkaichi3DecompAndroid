#ifndef SYS_ALIGN_H
#define SYS_ALIGN_H

#include "types.h"

s32 Mem_Align4Rem(u32 value);
s32 Mem_Align8Rem(u32 value);
s32 Mem_Align16Rem(u32 value);
s32 Mem_Align32Rem(u32 value);
s32 Mem_Align64Rem(u32 value);
s32 Mem_Align128Rem(u32 value);
s32 Mem_Align4Pad(u32 value);
s32 Mem_Align8Pad(u32 value);
s32 Mem_Align16Pad(u32 value);
s32 Mem_Align32Pad(u32 value);
s32 Mem_Align64Pad(u32 value);
s32 Mem_Align128Pad(u32 value);
u32 Mem_Align4Up(u32 value);
u32 Mem_Align8Up(u32 value);
u32 Mem_Align16Up(u32 value);
u32 Mem_Align32Up(u32 value);
u32 Mem_Align64Up(u32 value);
u32 Mem_Align128Up(u32 value);

#endif
