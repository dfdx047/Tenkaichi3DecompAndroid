#include "common.h"

/*
 * Debug stubs, 0x262FF0..0x263098: the first functions of the stripped debug module, which goes on in
 * sys/debug.c (linked from 0x263098). Twenty functions with nothing left in them; only the two profiler
 * hooks have callers (the battle, the character viewer and the dragon scene bracket their update and draw
 * passes with them).
 */

/* Empty stub, no callers. */
void Dbg_Stub262FF0(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub262FF8(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263000(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263008(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263010(void) {
}

/* Unused stub that returns 0. */
s32 Dbg_Stub263018(void) {
    return 0;
}

/* Empty stub, no callers. */
void Dbg_Stub263020(void) {
}

/* Empty profiler hook: called with gBattleProf at the start of a pass. */
void Dbg_ProfMark(void *prof) {
}

/* Empty profiler hook: called with gBattleProf and a bar colour at the end of a pass. All that is left
   of it is a float result of 0. */
f32 Dbg_ProfColor(void *prof, u32 color) {
    return 0.0f;
}

/* Empty stub, no callers. */
void Dbg_Stub263040(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263048(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263050(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263058(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263060(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263068(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263070(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263078(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263080(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263088(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub263090(void) {
}
