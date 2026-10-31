/* Core-independent source adaptation of PC Z Counter 2 v0.9.0.
 * Source of truth: Core 0.23.15 full r5, ZCounter2/Source/PC/counter_pc.c.
 *
 * Copy to port/src/. It is compiled by that project's game-facing soft-float
 * pipeline, and must be added to make_state.py's PORT_STATE set.
 * Game objects retain their PS2 layouts. Port_Ptr/Port_Addr translate their
 * 32-bit addresses on Windows/Linux and the macOS address-window build.
 * These are engine-owned objects: unlike the injectable DLL, this module
 * does not probe host virtual-memory permissions or executable signatures.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "port_addr.h"
#include "port/vu0_a.h"
#include "bt3_gameplay_rules.h"
#include "bt3_zcounter2.h"

/* The project declares partial views of fighters in each translation unit.
 * No aggregate passes by value here. The build lowers float calls together
 * with their game definitions; do not compile this as a host-ABI plugin. */
extern void *BtlChar_GetObj(void *);
extern int BtlChar_GetCount(void);
extern void *BtlChar_Get(int);
extern int BtlChar_GetFrame(void);
extern int BtlChar_TestFlag(void *, int);
extern void BtlChar_SetHeldFlag(void *, int);
extern int BtlChar_IsFree(void *);
extern int BtlAnim_GetId(void *);
extern uint32_t BtlAnim_GetFlags(int);
extern int BtlAct_NeutralHandler(void *, int);
extern void BtlMove_UpdateHoverOffset(void *);
extern int BtlMove_IsBlockedByOpponent(void *);
extern void BtlChar_PoseToObj(void *, int);
extern void BtlObjAnim_SamplePose(void *);
extern void BtlObjPose_CalcMatrices(void *);
extern void BtlObjBody_UpdateParts(void *);
extern int BtlOpp_GetPlayer(void *);
extern float BtlOpp_GetYawFromFacing(void *);
extern int BtlHit_IsFromBehind(void *, void *);
extern int BtlAtk_GetId(void *);
extern uint32_t BtlAtk_GetFlags(void *);
extern uint32_t BtlParam_GetFlags2(void *);
extern int BtlParam_GetUnk2(void *);
extern int BtlHit_IsVoid(void *, void *);
extern int BtlHit_CheckGuard(void *, void *, int32_t *);
extern int ColBox_Overlaps(void *, void *);
extern int BtlBodyHit_TestVolumes(void *, void *);

enum { MAX_FIGHTERS = 10, MAX_JOINTS = 121, MAX_BODY_PARTS = 64 };

struct Ghost {
    uint32_t actor, frame, valid, boxes;
    uint8_t master[0x1600];
    uint8_t model[0x1670];
    uint32_t original[MAX_JOINTS];
    uint8_t joint[MAX_JOINTS][0xd0];
    uint8_t body[MAX_BODY_PARTS][0x1d0];
};

struct ZCounterState {
    uint32_t magic, active_master, active_model, busy;
    uint32_t captures, queries, accepted, rejected;
    int32_t guard_full;
    struct Ghost g[MAX_FIGHTERS];
    uint8_t vu0[0x12b8];
    uint32_t live_model[MAX_FIGHTERS];
};

/* ALL of this record is gameplay state. Include it in rollback and savestates;
 * do not reset it when loading a snapshot. It contains game addresses, so raw
 * bytes are suitable only for same-build checks, not a cross-platform net
 * digest (which must use canonical, pointer-independent gameplay fields). */
static struct ZCounterState state;

typedef char vu0_layout_must_match[(sizeof(RefVu0State) == 0x12b8) ? 1 : -1];
typedef char ghost_layout_must_match[(sizeof(struct Ghost) == 0x104b4) ? 1 : -1];
typedef char state_layout_must_match[(sizeof(struct ZCounterState) == 672268) ? 1 : -1];

static uint32_t read_u32(const void *p, size_t o) {
    uint32_t v;
    memcpy(&v, (const uint8_t *)p + o, sizeof v);
    return v;
}
static int32_t read_s32(const void *p, size_t o) { return (int32_t)read_u32(p, o); }
static void write_u32(void *p, size_t o, uint32_t v) {
    memcpy((uint8_t *)p + o, &v, sizeof v);
}
static uint8_t *read_ptr(const void *p, size_t o) { return Port_Ptr(read_u32(p, o)); }

/* This checks representation/range, not host memory permissions. Callers
 * supply live, allocated game objects, as other native game functions do. */
static int game_span(const void *p, size_t bytes) {
    uint32_t a;
    if (!p) return 0;
    a = Port_Addr(p);
    return a != 0 && Port_Ptr(a) == p && bytes <= UINT32_MAX - a;
}
static int roster_count(void) {
    int n = BtlChar_GetCount();
    return n >= 2 && n <= MAX_FIGHTERS ? n : 0;
}
static int fighter_slot(void *fighter) {
    int i, n = roster_count();
    for (i = 0; i < n; ++i) if (BtlChar_Get(i) == fighter) return i;
    return -1;
}
static uint8_t *copied_joint(struct Ghost *g, uint32_t p, int n) {
    int i;
    for (i = 0; i < n; ++i) if (g->original[i] == p) return g->joint[i];
    return NULL;
}

void Bt3ZCounter2_Reset(void) {
    memset(&state, 0, sizeof state);
    state.magic = 0x5a433209;
}

void *Bt3ZCounter2_ModelOverride(void *fighter) {
    if (fighter && state.active_master == Port_Addr(fighter))
        return Port_Ptr(state.active_model);
    return NULL;
}

void Bt3ZCounter2_Capture(void *fighter) {
    struct Ghost *g;
    uint8_t *model, *body;
    int k, count, i, joints, blocked, animation;
    uint8_t held, pulse;
    if (!Bt3Gameplay_ZCounter2Enabled() || state.busy ||
        !game_span(fighter, 0x1600) || !game_span(&state, sizeof state)) return;
    k = fighter_slot(fighter);
    if (k < 0) return;
    g = &state.g[k];
    g->valid = 0;
    if (read_s32(fighter, 0x948) != 11 ||
        (read_u32(fighter, 0x744) & 0x18) != 0x18) return;
    animation = read_s32(fighter, 0x974);
    if (animation != 0 && animation != 1 && animation != 0x26 && animation != 0x27) return;
    model = BtlChar_GetObj(fighter);
    if (!game_span(model, 0x1670)) return;
    body = read_ptr(model, 0xf50);
    count = 0;
    for (; count < MAX_BODY_PARTS; ++count) {
        if (!game_span(body, (size_t)(count + 1) * 0x1d0)) return;
        if (read_u32(body, (size_t)count * 0x1d0) & 1) { ++count; break; }
    }
    if (!count || !(read_u32(body, (size_t)(count - 1) * 0x1d0) & 1)) return;
    memcpy(g->master, fighter, sizeof g->master);
    memcpy(g->model, model, sizeof g->model);
    memcpy(g->body, body, (size_t)count * 0x1d0);
    joints = 0;
    for (i = 0; i < MAX_JOINTS; ++i) {
        uint32_t p = read_u32(model, 0xd6c + 4 * i);
        uint8_t *q;
        if (!p) continue;
        if (!game_span(Port_Ptr(p), 0xd0)) return;
        q = copied_joint(g, p, joints);
        if (!q) {
            if (joints >= MAX_JOINTS) return;
            g->original[joints] = p;
            q = g->joint[joints++];
            memcpy(q, Port_Ptr(p), 0xd0);
        }
        write_u32(g->model, 0xd6c + 4 * i, Port_Addr(q));
    }
    for (i = 0; i < count; ++i) {
        uint8_t *q = copied_joint(g, read_u32(g->body[i], 12), joints);
        if (!q) return;
        write_u32(g->body[i], 12, Port_Addr(q));
    }
    write_u32(g->model, 0xf50, Port_Addr(g->body));
    for (i = 0xfa0; i <= 0xfa4; i += 4) {
        uint32_t p = read_u32(model, i), m = Port_Addr(model);
        if (p >= m && p - m < 0x1670)
            write_u32(g->model, i, Port_Addr(g->model) + p - m);
    }

    state.busy = 1;
    state.active_master = Port_Addr(g->master);
    state.active_model = Port_Addr(g->model);
    memcpy(state.vu0, &gRefVu0, sizeof state.vu0);
    /* A cloned fighter is not in the live roster. Resolve the native blocking
     * answer on the live fighter, then encode it in the private flag bytes. */
    blocked = BtlMove_IsBlockedByOpponent(fighter);
    held = g->master[0x1085 + 11];
    pulse = g->master[0x10ad + 11];
    g->master[0x1085 + 11] = (held & 0x3f) | (blocked ? 0xc0 : 0);
    g->master[0x10ad + 11] = pulse & 0x3f;
    BtlAct_NeutralHandler(g->master, 1);
    g->master[0x1085 + 11] = (g->master[0x1085 + 11] & 0x3f) | (held & 0xc0);
    g->master[0x10ad + 11] = (g->master[0x10ad + 11] & 0x3f) | (pulse & 0xc0);
    BtlMove_UpdateHoverOffset(g->master);
    BtlChar_PoseToObj(g->master, 0);
    BtlObjAnim_SamplePose(g->model);
    BtlObjPose_CalcMatrices(g->model);
    BtlObjBody_UpdateParts(g->model + 0xf50);
    memcpy(&gRefVu0, state.vu0, sizeof state.vu0);
    state.active_master = state.active_model = state.busy = 0;
    g->actor = Port_Addr(fighter);
    g->frame = (uint32_t)BtlChar_GetFrame();
    g->boxes = (uint32_t)count;
    state.live_model[k] = Port_Addr(model);
    g->valid = 1;
    ++state.captures;
}

static int dodge_wins(void *attacker, void *defender, uint32_t attack_flags) {
    if (BtlChar_TestFlag(defender, 0x5a)) return 0;
    if (read_s32(defender, 0x1068) > ((attack_flags & 0x4000000) != 0) && (attack_flags & 1)) return 1;
    if (read_s32(defender, 0xe00) > 0 || read_s32(defender, 0xe04) > 0 ||
        BtlChar_TestFlag(defender, 0x56) || BtlChar_TestFlag(defender, 0x57)) return 1;
    if (BtlChar_TestFlag(defender, 0x4e) && (BtlParam_GetFlags2(defender) & 0x80) &&
        BtlParam_GetUnk2(attacker) != 4 && !BtlHit_IsFromBehind(attacker, defender)) {
        int id = BtlAtk_GetId(attacker);
        if (id >= 0 && id <= 10) return 1;
    }
    return 0;
}

static int can_counter(void *attacker, void *defender, struct Ghost *g) {
    uint8_t *model, *work;
    uint32_t animation_flags, attack_flags;
    float yaw;
    if (read_s32(defender, 0x1070) <= 0 || (read_u32(defender, 0x744) & 0x18) != 0x18) return 0;
    if (read_s32(defender, 0x948) != 13 && read_s32(defender, 0x948) != 14) return 0;
    model = BtlChar_GetObj(attacker);
    if (!game_span(model, 0x1670)) return 0;
    work = read_ptr(model, 0x1660);
    if (!game_span(work, 0x1a0c0) || (read_u32(work, 0x18020) & 0xf000000)) return 0;
    if (BtlChar_TestFlag(attacker, 0x60) || BtlChar_TestFlag(attacker, 0x65) || BtlAtk_GetId(attacker) == -1) return 0;
    if (!BtlChar_IsFree(defender) || ((int8_t *)model)[0xcad] != 0) return 0;
    animation_flags = BtlAnim_GetFlags(BtlAnim_GetId(defender));
    if (!(animation_flags & 0x31)) return 0;
    attack_flags = BtlAtk_GetFlags(attacker);
    yaw = BtlOpp_GetYawFromFacing(attacker);
    /* Exact single-precision constant used by v0.9.0: 0x3fc90fda. */
    if (!(attack_flags & 8) && !(__builtin_fabsf(yaw) < 0x1.921fb4p+0f)) return 0;
    if (BtlHit_IsVoid(attacker, defender) || dodge_wins(attacker, defender, attack_flags) ||
        ((animation_flags & 0x200) && (attack_flags & 2)) ||
        BtlChar_TestFlag(defender, 0x59) || BtlChar_TestFlag(defender, 0x53)) {
        ++state.rejected;
        return 0;
    }
    ++state.queries;
    if (!ColBox_Overlaps(work + 0x18080, g->model + 0xfc0)) return 0;
    return BtlBodyHit_TestVolumes(work, g->model + 0xf50) != 0;
}

void Bt3ZCounter2_BeforeHit(void *attacker) {
    int opponent;
    void *defender;
    struct Ghost *g;
    int ok;
    if (!Bt3Gameplay_ZCounter2Enabled() || state.busy ||
        !game_span(attacker, 0x1600) || fighter_slot(attacker) < 0) return;
    /* Consume the native dispatcher's current pair. Do not scan/retarget the
     * roster or impose a new quota/order on simultaneous contacts. */
    opponent = BtlOpp_GetPlayer(attacker);
    if (opponent < 0 || opponent >= roster_count()) return;
    defender = BtlChar_Get(opponent);
    g = &state.g[opponent];
    if (defender == attacker || !game_span(defender, 0x1600) || !g->valid ||
        g->actor != Port_Addr(defender) || g->frame != (uint32_t)BtlChar_GetFrame() ||
        state.live_model[opponent] != Port_Addr(BtlChar_GetObj(defender))) return;
    memcpy(state.vu0, &gRefVu0, sizeof state.vu0);
    ok = can_counter(attacker, defender, g);
    memcpy(&gRefVu0, state.vu0, sizeof state.vu0);
    if (ok) {
        BtlChar_SetHeldFlag(attacker, 0x60);
        state.guard_full = 1;
        if (BtlHit_CheckGuard(attacker, defender, &state.guard_full) && !state.guard_full)
            ++state.accepted;
    }
    /* Keep this pose for other eligible pairs in the same tick. Native flags
     * suppress duplicate handling; the frame check prevents late reuse. */
}
