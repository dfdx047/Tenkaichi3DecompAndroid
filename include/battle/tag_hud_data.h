#ifndef TAG_HUD_DATA_H
#define TAG_HUD_DATA_H
/* A value-only snapshot crosses the game/render thread boundary. No fighter,
 * model, or renderer pointers are shared. Coordinates use the game's 512x448 page. */
#include "tag_capacity.h"
#define TAG_HUD_MAX TAG_ACTOR_CAPACITY
typedef struct TagHudPoint { float x, y, scale; int visible; } TagHudPoint;
/* Homogeneous projection of the body's centre and its three world axes.
 * Copying this basis is much smaller than sending every ribbon vertex. */
typedef struct TagHudRing {
    float centre[4], x[4], y[4], z[4];
    int visible;
} TagHudRing;
typedef struct TagHudActor {
    int chara, costume, team, player, target;
    int hp, hpMax, ki, kiMax, stocks, stockMax, hits, damage;
    int down, eliminated, helper, reviveTicks, prompt;
    int fusedInto, fusionSeconds, fusionRequest;
    int fusionBody, fusionDuration, fusionController, fusionNext, fusionSwitchSeconds;
    int assistAlly, assistReady, assistPower, attacking, absent;
    char practiceLabel[32];
} TagHudActor;
typedef struct TagHudView {
    int actor, controlActor, takeover, visible, spectating, rect[4];
    int reviveBody;
    TagHudPoint heads[TAG_HUD_MAX];
    TagHudPoint warnings[TAG_HUD_MAX];
    TagHudRing rings[TAG_HUD_MAX];
} TagHudView;
typedef struct TagHudState {
    int active, count, views, shared, shown, paused, timeLeft, frame;
    int reviveEnabled, reviveDuration, reviveCost;
    TagHudActor actors[TAG_HUD_MAX];
    TagHudView view[TAG_HUD_MAX];
} TagHudState;
#endif
