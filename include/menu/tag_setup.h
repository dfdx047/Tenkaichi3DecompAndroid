#ifndef MENU_TAG_SETUP_H
#define MENU_TAG_SETUP_H
#include "types.h"
void TagSetup_Reset(void);
void TagSetup_Draw(void);
/* 1: continue to fighter selection, -1: back, 0: still choosing. */
s32 TagSetup_Input(void);
#endif
