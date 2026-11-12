#include "common.h"
#include "sys/align.h"

/* Bytes past the previous multiple of 4. */
s32 Mem_Align4Rem(u32 value) {
    return value & 3;
}

/* Bytes past the previous multiple of 8. */
s32 Mem_Align8Rem(u32 value) {
    return value & 7;
}

/* Bytes past the previous multiple of 16. */
s32 Mem_Align16Rem(u32 value) {
    return value & 15;
}

/* Bytes past the previous multiple of 32. */
s32 Mem_Align32Rem(u32 value) {
    return value & 31;
}

/* Bytes past the previous multiple of 64. */
s32 Mem_Align64Rem(u32 value) {
    return value & 63;
}

/* Bytes past the previous multiple of 128. */
s32 Mem_Align128Rem(u32 value) {
    return value & 127;
}

/* Bytes needed to reach the next multiple of 4. */
s32 Mem_Align4Pad(u32 value) {
    return -(value & 3) & 3;
}

/* Bytes needed to reach the next multiple of 8. */
s32 Mem_Align8Pad(u32 value) {
    return -(value & 7) & 7;
}

/* Bytes needed to reach the next multiple of 16. */
s32 Mem_Align16Pad(u32 value) {
    return -(value & 15) & 15;
}

/* Bytes needed to reach the next multiple of 32. */
s32 Mem_Align32Pad(u32 value) {
    return -(value & 31) & 31;
}

/* Bytes needed to reach the next multiple of 64. */
s32 Mem_Align64Pad(u32 value) {
    return -(value & 63) & 63;
}

/* Bytes needed to reach the next multiple of 128. */
s32 Mem_Align128Pad(u32 value) {
    return -(value & 127) & 127;
}

/* Rounds up to a multiple of 4. */
u32 Mem_Align4Up(u32 value) {
    return value + Mem_Align4Pad(value);
}

/* Rounds up to a multiple of 8. */
u32 Mem_Align8Up(u32 value) {
    return value + Mem_Align8Pad(value);
}

/* Rounds up to a multiple of 16. */
u32 Mem_Align16Up(u32 value) {
    return value + Mem_Align16Pad(value);
}

/* Rounds up to a multiple of 32. */
u32 Mem_Align32Up(u32 value) {
    return value + Mem_Align32Pad(value);
}

/* Rounds up to a multiple of 64. */
u32 Mem_Align64Up(u32 value) {
    return value + Mem_Align64Pad(value);
}

/* Rounds up to a multiple of 128. */
u32 Mem_Align128Up(u32 value) {
    return value + Mem_Align128Pad(value);
}
