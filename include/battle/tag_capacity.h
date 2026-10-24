#ifndef TAG_CAPACITY_H
#define TAG_CAPACITY_H
/* Battles still select at most ten fighters. The practice map reserves nine
   guest bodies plus six dummies; the host has no body there. */
#define TAG_ACTOR_CAPACITY 15
#define TAG_SHADOW_CAPACITY (TAG_ACTOR_CAPACITY + 2)
/* Two music streams, then one voice and one streamed effect per actor. */
#define TAG_ADX_CAPACITY (2 + 2 * TAG_ACTOR_CAPACITY)
#endif
