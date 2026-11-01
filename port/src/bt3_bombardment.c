/* Native Bombardment v2.2: current-input-gated contact-stop delay.
 * Based on the v2.1 extraction; a previous shot alone never delays contact.
 * Source target: z3xox/Tenkaichi3Decomp
 * 7b43a75bd8b0aeabcce0a22c45e1ad66162ac9d7.
 *
 * Keep this file in the port's game-source compile/lowering pipeline.
 * Do not compile it as a Windows DLL or as an independently linked host-ABI
 * plugin. No fixed addresses, patching, Core SDK, or installation is used.
 */
#include "bt3_bombardment.h"
#include "battle/btl_char_action.h"
#include "battle/btl_char_flag.h"
#include "battle/btl_char_status.h"
#include "battle/btl_input.h"

enum {
    BOMBARDMENT_NONE = 0,
    BOMBARDMENT_ARMED = 1,
    BOMBARDMENT_PENDING_STOP = 2
};

/* Exact original v2.1 bits 0x3E199999, including PS2 constant truncation.
 * 0.15f compiled directly by an ordinary host compiler is one ULP higher.
 */
#define BOMBARDMENT_CONTACT_STOP_BLEND 0x1.333332p-3f

/* Read the same effective battle buttons and counter as native input query
 * 0x5A, one update before its age == 6 condition. Do not call the query with
 * a forged age, change the counter, or queue/fire a blast here. Native attack
 * eligibility and resources are checked by BtlDecide_Attack on the next
 * update. The caller is the existing body-contact branch of dash animation D.
 */
static s32 next_smash_input_is_one_update_away(BtlActDChr *chr) {
    BtlInputChr *input_chr = (BtlInputChr *)chr;
    return !BtlInput_IsHeld(input_chr, BTLB_CHARGE | BTLB_RUSH) &&
           BtlInput_IsHeld(input_chr, BTLB_BLAST) &&
           BtlInput_GetFramesSincePress(input_chr, 2) == 5;
}

s32 Bt3Bombardment_GetPrevForDashEnter(BtlActDChr *chr, s32 enabled) {
    s32 previous = BtlAct_GetPrev((BtlActChr *)chr);
    s32 animation;

    chr->work[2] = BOMBARDMENT_NONE;
    if (!enabled) {
        return previous;
    }

    animation = BtlAnim_GetId((BtlStatChr *)chr);
    if (previous == 0xB1 &&
        (animation == 0x86 || animation == 0x89 || animation == 0x8C) &&
        BtlChar_TestFlag((BtlFlagChr *)chr, 0x31)) {
        chr->work[2] = BOMBARDMENT_ARMED;
    }
    return previous;
}

s32 Bt3Bombardment_GetAnimationForDashDecide(BtlActDChr *chr, s32 enabled) {
    s32 animation = BtlAnim_GetId((BtlStatChr *)chr);

    if (!enabled) {
        chr->work[2] = BOMBARDMENT_NONE;
        return animation;
    }

    if (chr->work[2] == BOMBARDMENT_PENDING_STOP) {
        chr->work[2] = BOMBARDMENT_NONE;
        if (animation == 0x0D) {
            BtlAnim_Request((BtlStatChr *)chr, 0x0E,
                            BOMBARDMENT_CONTACT_STOP_BLEND);
        }
    }
    return animation;
}

void Bt3Bombardment_RequestContactStop(BtlActDChr *chr, s32 enabled) {
    if (!enabled) {
        chr->work[2] = BOMBARDMENT_NONE;
    } else if (chr->work[2] == BOMBARDMENT_ARMED &&
               next_smash_input_is_one_update_away(chr)) {
        chr->work[2] = BOMBARDMENT_PENDING_STOP;
        return;
    }
    /* First contact without the frame-five input stops normally and expires
     * the continuation allowance. A previous shot is not a banked delay. */
    chr->work[2] = BOMBARDMENT_NONE;
    BtlAnim_Request((BtlStatChr *)chr, 0x0E,
                    BOMBARDMENT_CONTACT_STOP_BLEND);
}
