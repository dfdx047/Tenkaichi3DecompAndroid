#ifndef BATTLE_TAG_REVIVE_H
#define BATTLE_TAG_REVIVE_H
#include "types.h"

/* A KO-only floor at the current stage's lower flight boundary. No terrain or
 * additional persistent state is changed, so rollback restores it naturally. */
s32 TagRevive_FallenSupported(s32 actor);
void TagRevive_CatchFallen(s32 actor);
#endif
