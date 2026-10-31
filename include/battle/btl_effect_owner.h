#ifndef BTL_EFFECT_OWNER_H
#define BTL_EFFECT_OWNER_H
#include "common.h"

/* Keep the original two owner bits and stage-reset group separate. */
s32 BtlScene_CharOwnerTag(s32 objId);
void BtlScene_ResetCharacter(s32 objId);

#endif
