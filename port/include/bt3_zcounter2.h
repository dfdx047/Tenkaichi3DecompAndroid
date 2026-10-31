#ifndef BT3_ZCOUNTER2_H
#define BT3_ZCOUNTER2_H

/* Source integration API; compile bt3_zcounter2.c with the port's normal
 * game-facing soft-float pipeline. No Core or dynamic hooks are used. */
#ifdef __cplusplus
extern "C" {
#endif

/* Call after BtlDecide_Main's (mask & 1) && TEST(1) succeeds, before
 * selecting movement action 0xD/0xE. The helper checks the match rule. */
void Bt3ZCounter2_Capture(void *fighter);

/* At the start of BtlChar_GetObj: return this value if non-null, otherwise
 * execute the original BtlObj_Get(chr->objId). */
void *Bt3ZCounter2_ModelOverride(void *fighter);

/* At BtlHit_TestHit entry, before its native flag/geometry checks.
 * Always continue into the original function body afterwards. */
void Bt3ZCounter2_BeforeHit(void *attacker);

/* Match start/end and roster reset, never after a rollback restore. */
void Bt3ZCounter2_Reset(void);

#ifdef __cplusplus
}
#endif
#endif
