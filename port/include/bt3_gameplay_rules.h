#ifndef BT3_GAMEPLAY_RULES_H
#define BT3_GAMEPLAY_RULES_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BT3_GAMEPLAY_RULESET_REVISION = 2,
    BT3_RULE_ZCOUNTER2 = 1u << 0,
    BT3_RULE_BOMBARDMENT = 1u << 1,
    BT3_RULE_ALL = BT3_RULE_ZCOUNTER2 | BT3_RULE_BOMBARDMENT
};
enum Bt3GameplayMode {
    BT3_GAMEPLAY_OFFLINE = 0,
    BT3_GAMEPLAY_CASUAL = 1,
    BT3_GAMEPLAY_RANKED = 2
};
struct Bt3GameplayRules {
    uint32_t revision;
    uint32_t flags;
};

/* All stateful API calls, including the getters below, belong to the
 * simulation thread. The upstream F1 UI uses a separate render thread:
 * publish effective status through the port's synchronized UI data path.
 * Do not let that thread read/write this rollback-owned state directly. */

/* Compose preferences for offline/casual, or the mandatory ranked pair.
 * Send revision and flags through the port's explicit packet/replay encoding;
 * do not send a raw C struct or infer agreement from local preferences. */
struct Bt3GameplayRules Bt3Gameplay_MakeRules(int zcounter2, int bombardment,
                                            enum Bt3GameplayMode mode);

/* Call on the simulation thread at a verified match-start boundary, AFTER
 * its bootstrap snapshot has been restored and BEFORE battle simulation.
 * Online callers must already have negotiated identical rules with the peer.
 * The mode must come from the authoritative match context.
 * Returns 0 for an active match, unsupported revision/flags/mode, or an
 * invalid ranked ruleset. Invalid requests leave existing state untouched. */
int Bt3Gameplay_BeginMatch(const struct Bt3GameplayRules *rules,
                          enum Bt3GameplayMode mode);

/* Call at a verified match end/abort, on the simulation thread. Never use
 * BeginMatch/EndMatch to handle rollback restores or change a live match. */
void Bt3Gameplay_EndMatch(void);

int Bt3Gameplay_ZCounter2Enabled(void);
int Bt3Gameplay_BombardmentEnabled(void);
int Bt3Gameplay_MatchActive(void);
struct Bt3GameplayRules Bt3Gameplay_CurrentRules(void);

#ifdef __cplusplus
}
#endif
#endif
