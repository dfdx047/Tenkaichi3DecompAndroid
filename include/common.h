#ifndef COMMON_H
#define COMMON_H

#include "include_asm.h"
#include "types.h"

#define NULL 0

/* Resource IDs differ between discs; never remap arbitrary file IDs globally. */
#ifdef PORT
#define DISC_RESOURCE(bt3, bt4) (PortGame_IsBT4() ? (bt4) : (bt3))
#else
#define DISC_RESOURCE(bt3, bt4) (bt3)
#endif

/* Preserve the console save layout. Larger native rosters keep extra
   equipment slots in a separate file beside that profile's memory card. */
#ifdef PORT
#include "../port/include/tag_text.h"
#define SAVE_CUSTOM_AT(save, slot) (*(__typeof__(&(save)->custom[0]))PortGame_CustomSlot((save)->custom, (slot)))
#else
#define SAVE_CUSTOM_AT(save, slot) ((save)->custom[(slot)])
#endif

#endif
