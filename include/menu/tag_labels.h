#ifndef MENU_TAG_LABELS_H
#define MENU_TAG_LABELS_H
#include "../battle/tag_options.h"
/* Native menu artwork made at startup from the user's own disc lettering. */
enum {
    TAG_LABEL_CPU = 9,
    TAG_LABEL_CHOOSE = 10,
    TAG_LABEL_ONLINE = 11,
    TAG_LABEL_GAUNTLET = 12,
    TAG_LABEL_TOURNAMENT = 13,
    TAG_LABEL_LOBBY = 14,
    TAG_LABEL_CLOSE_LOBBY = 15,
    /* These IDs cross the game/renderer boundary. Adding settings must not
       move CPU Only onto an old Back row in an incremental build. */
    TAG_LABEL_SETTINGS = 16,
    TAG_LABEL_OPTIONS = 32,
    TAG_LABEL_PRESETS = TAG_LABEL_OPTIONS + TAG_OPT_COUNT,
    TAG_LABEL_TOURS = TAG_LABEL_PRESETS + 26,
    TAG_LABEL_PREV = TAG_LABEL_TOURS + 5, TAG_LABEL_NEXT,
    TAG_LABEL_COUNT
};
#endif
