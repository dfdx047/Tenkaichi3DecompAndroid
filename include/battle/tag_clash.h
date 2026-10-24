#ifndef BATTLE_TAG_CLASH_H
#define BATTLE_TAG_CLASH_H
#include "types.h"

/* Participant ownership is recorded by the collision that started the clash.
 * A selected target is not evidence that two actors have collided. */
void BtlClash_Reset(void);
s32 BtlClash_Join(s32 a, s32 b);
s32 BtlClash_Opponent(s32 actor);
s32 BtlClash_Side(s32 actor);
void *BtlClash_Exchange(s32 actor);
f32 BtlClash_Bias(s32 actor);
s32 BtlClash_State(s32 actor);
s32 BtlClash_SequenceActive(s32 actor);
s32 BtlClash_SequenceInput(s32 actor);
s32 BtlClash_SequencePrompt(s32 actor);
#endif
