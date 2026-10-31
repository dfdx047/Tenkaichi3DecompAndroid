/* Minimal match policy for the extracted gameplay code. It does not add a
 * ranked service, UI, network protocol or persistent settings by itself.
 * Compile under port/src and whitelist bt3_gameplay_rules in PORT_STATE. */
#include "bt3_gameplay_rules.h"
#include "bt3_zcounter2.h"

static struct {
    uint32_t active;
    uint32_t mode;
    struct Bt3GameplayRules rules;
} match;

struct Bt3GameplayRules Bt3Gameplay_MakeRules(int zcounter2, int bombardment,
                                            enum Bt3GameplayMode mode) {
    struct Bt3GameplayRules rules;
    rules.revision = BT3_GAMEPLAY_RULESET_REVISION;
    rules.flags = (zcounter2 ? BT3_RULE_ZCOUNTER2 : 0u) |
                  (bombardment ? BT3_RULE_BOMBARDMENT : 0u);
    if (mode == BT3_GAMEPLAY_RANKED) rules.flags = BT3_RULE_ALL;
    return rules;
}

int Bt3Gameplay_BeginMatch(const struct Bt3GameplayRules *rules,
                          enum Bt3GameplayMode mode) {
    if (!rules || match.active || mode < BT3_GAMEPLAY_OFFLINE || mode > BT3_GAMEPLAY_RANKED ||
        rules->revision != BT3_GAMEPLAY_RULESET_REVISION || (rules->flags & ~BT3_RULE_ALL) ||
        (mode == BT3_GAMEPLAY_RANKED && rules->flags != BT3_RULE_ALL)) return 0;
    Bt3ZCounter2_Reset();
    match.rules = *rules;
    match.mode = (uint32_t)mode;
    match.active = 1;
    return 1;
}

void Bt3Gameplay_EndMatch(void) {
    match.active = 0;
    match.mode = BT3_GAMEPLAY_OFFLINE;
    match.rules.revision = BT3_GAMEPLAY_RULESET_REVISION;
    match.rules.flags = 0;
    Bt3ZCounter2_Reset();
}

/* (port: never while a replay is played back, which was recorded with the game's own rules: src/battle/hud_a.c) */
extern int BtlGame_IsReplay(void);

int Bt3Gameplay_ZCounter2Enabled(void) {
    return match.active && (match.rules.flags & BT3_RULE_ZCOUNTER2) != 0 && !BtlGame_IsReplay();
}
int Bt3Gameplay_BombardmentEnabled(void) {
    return match.active && (match.rules.flags & BT3_RULE_BOMBARDMENT) != 0 && !BtlGame_IsReplay();
}
int Bt3Gameplay_MatchActive(void) { return match.active != 0; }
struct Bt3GameplayRules Bt3Gameplay_CurrentRules(void) { return match.rules; }
