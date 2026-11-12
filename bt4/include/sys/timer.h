#ifndef SYS_TIMER_H
#define SYS_TIMER_H

#include "types.h"

/* EE hardware timers 0 and 1 used as a stopwatch, src/sys/timer.c = 0x11F190..0x11F548. */

#define TIMER_COUNT(ch) (*(volatile u32 *)(0x10000000 + (ch) * 0x800)) /* Tn_COUNT */
#define TIMER_MODE(ch) (*(volatile u32 *)(0x10000010 + (ch) * 0x800))  /* Tn_MODE */
#define TIMER_COMP(ch) (*(volatile u32 *)(0x10000020 + (ch) * 0x800))  /* Tn_COMP */

/* Tn_MODE bits */
#define TIMER_MODE_CLK_HBLANK 0x003 /* count horizontal blanks */
#define TIMER_MODE_ZRET 0x040       /* clear the counter when it reaches Tn_COMP */
#define TIMER_MODE_CUE 0x080        /* counting enabled */
#define TIMER_MODE_CMPE 0x100       /* compare interrupt enable */
#define TIMER_MODE_OVFE 0x200       /* overflow interrupt enable */
#define TIMER_MODE_EQUF 0x400       /* compare reached; write 1 to clear */

#define TIMER_HBLANK_HZ 15734 /* NTSC line rate the conversion assumes */

void Timer_SetMode(s32 ch, u32 value, u8 op);
u32 Timer_GetMode(s32 ch);
void Timer_SetCompare(s32 ch, u32 value);
void Timer_SetCount(s32 ch, u32 value);
s32 Timer_ReadMicros(s32 ch);
s32 Timer_Stub(void);
void Timer_Init(s32 ch);
void Timer_Start(s32 ch);
void Timer_Stop(s32 ch);
void Timer_Reset(s32 ch);
s32 Timer_GetMicros(s32 ch);
f32 Timer_GetFrames(s32 ch);

#endif
