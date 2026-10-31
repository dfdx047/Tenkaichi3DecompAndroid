#ifndef BT3_NATIVE_BOMBARDMENT_H
#define BT3_NATIVE_BOMBARDMENT_H

#include "battle/btl_act_d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native Bombardment v2.2, based on the v2.1 extraction from Core package r5.
 * This module has no Core or loader dependency. Compile it through the
 * port's GAME-SOURCE pipeline, with the same ABI as the battle sources.
 *
 * enabled is the immutable, agreed rule for the current match. Do not feed
 * a live local F1 toggle directly into these simulation calls online.
 * State is solely chr->work[2], already cleared on native action changes.
 */

/* Replace GetPrev only in BtlAct_DashMoveHandler's PHASE_ENTER switch.
 * Must run before BtlAnim_Play replaces the completed blast's animation.
 * Returns the original previous action, unchanged.
 */
s32 Bt3Bombardment_GetPrevForDashEnter(BtlActDChr *chr, s32 enabled);

/* Replace GetId only in BtlAct_DashMoveHandler's PHASE_DECIDE switch.
 * A queued contact stop is consumed here even if contact has cleared.
 * Returns the original animation id, unchanged.
 */
s32 Bt3Bombardment_GetAnimationForDashDecide(BtlActDChr *chr, s32 enabled);

/* Replace only the contact branch's Request(chr, 0xE, 0.15f) in dash
 * PHASE_DECIDE / animation 0xD. Retain the following native speed = 0.
 * Do not replace the adjacent, independent input-driven stop request.
 * A completed-shot continuation delays its first contact stop only while
 * BLAST is held at native input age 5 and CHARGE/RUSH are not held. Other
 * contact stops expire the allowance and request the native stop immediately.
 */
void Bt3Bombardment_RequestContactStop(BtlActDChr *chr, s32 enabled);

#ifdef __cplusplus
}
#endif

#endif
