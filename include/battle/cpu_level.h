#ifndef BATTLE_CPU_LEVEL_H
#define BATTLE_CPU_LEVEL_H
#include "types.h"
/* The AI's discrete response tables contain five entries. BT4 Z Hard (99)
 * still uses the fastest tier; its continuous rate curves retain level 99. */
static inline s32 CpuLevel_TableIndex(s32 level) {
    return level<0?0:level>=24?4:level/6;
}
#endif
