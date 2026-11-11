#ifndef BT4_BODY_CHANGE_H
#define BT4_BODY_CHANGE_H
#include "types.h"
void Bt4BodyChange_Reset(void);
void Bt4BodyChange_Queue(s32 attacker,s32 victim);
void Bt4BodyChange_Update(void);
s32 Bt4BodyChange_Active(s32 actor);
s32 Bt4BodyChange_OnModelLoaded(void *fighter);
void Bt4BodyChange_Action(void *fighter,s32 phase);
#endif
