#ifndef SYS_MEM_READ_H
#define SYS_MEM_READ_H

#include "types.h"

/* Typed reads from a data pointer, src/sys/mem_read.c = 0x11F968..0x11FA10. */

s32 Mem_LoadWord(const void *p);
f32 Mem_LoadFloat(const void *p);
u16 Mem_LoadHalf(const void *p);
s32 Mem_ReadS32(const void *p);
u32 Mem_ReadU32(const void *p);
f32 Mem_ReadF32(const void *p);
s32 Mem_ReadU16(const void *p);
s16 Mem_ReadS16(const void *p);

#endif
