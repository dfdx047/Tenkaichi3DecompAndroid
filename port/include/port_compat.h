/*
 * Forced into every game source of the PC build (-include port_compat.h), in front of the game's own headers.
 * The game sources under src/ and include/ are shared with the matching decompilation and are not edited for the
 * port unless there is no other way; differences between the PS2 compiler and a host compiler are bridged here.
 */
#ifndef PORT_COMPAT_H
#define PORT_COMPAT_H

#define PORT 1
#include "plat_game.h"
#include "battle/tag_capacity.h"
#define PORT_FIGHTER_CAPACITY TAG_ACTOR_CAPACITY
extern void TagContact_Clear(int actor);
extern void TagContact_Record(int attacker, int defender);
extern int TagContact_Has(int attacker, int defender);
extern int TagTeam_Active(void);
extern int TagTeam_CheckEnd(int timeUp);
extern int TagTeam_Count(void);
extern int TagTeam_Team(int fighter);
extern int TagTeam_Target(int fighter);
extern int TagTeam_IsEnemy(int a, int b);
extern int TagTeam_ViewCount(void);
extern int TagTeam_ViewActor(int view);
extern int TagTeam_Player(int actor);

/* The game's types: on the PS2 `long` is 64 bits. Take over include/types.h. */
#define TYPES_H
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef long long s64;
typedef unsigned long long u64;
typedef float f32;

/* Functions the matching build takes from assembly: the port supplies them elsewhere (see docs/port/). */
#define INCLUDE_ASM_H
#define INCLUDE_ASM(FOLDER, NAME)
#define INCLUDE_RODATA(FOLDER, NAME)
#define RODATA_ALIGN16()
#define LIT4_WORD(NAME, HEX)
#define ASM_STUB_BEGIN() extern int asm_stub_begin_
#define ASM_STUB_END() extern int asm_stub_end_

/* C library calls of the game that must not go to the host's versions (see port/src/plat_mem.c). */
#define malloc Port_Malloc

#endif
