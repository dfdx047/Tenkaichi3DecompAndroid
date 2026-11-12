#ifndef SYS_HEAP_INFO_H
#define SYS_HEAP_INFO_H

#include "types.h"

s32 Heap_CountBlocks(s32 heap);
s32 Heap_CountFreeBlocks(s32 heap);
s32 Heap_CountUsedBlocks(s32 heap);

#endif
