#ifndef BATTLE_EFT_FIGHTER_CAPACITY_H
#define BATTLE_EFT_FIGHTER_CAPACITY_H
/* Persistent effect state belongs to an actor, not to one of the two teams. */
#ifdef PORT
#define EFT_FIGHTER_CAPACITY PORT_FIGHTER_CAPACITY
#else
#define EFT_FIGHTER_CAPACITY 2
#endif
#endif
