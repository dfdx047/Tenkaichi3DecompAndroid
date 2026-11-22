#ifndef MENU_PORT_SELFLOW_H
#define MENU_PORT_SELFLOW_H

/* The ORDER of a character select (the map first, or the fighters and then the map) is part of the game's state: the machine
   that runs it is driven by the two pads, and online both copies run the same one from the same input. It also decides how
   the battle that follows is entered (the way to the battle, or the game's VS and the stage's introduction), so how many
   blanks pass before the first blow.

   So it may depend only on what is the same on every machine of an online match: never on which SCREEN this machine shows.
   `newScreen` (the setting select_classic) and `settingStageFirst` (select_stage_first) are this device's: they decide the
   order only offline.

   Pure on purpose: port/tests/select_flow/ compiles this very file and checks every pair of screens against each other. */
/* The order ONLINE. It has to be the game's own (the fighters, then the map, then the game's VS of 60 blanks and the stage's
   introduction): that is what every earlier release does, what the classic screen does, and what a PC copy that does not know the
   new screen does. "Map first" online was only ever in test builds (from 21/11/2026); with it a classic copy and a new-screen copy
   did not agree, and neither did a new-screen copy and a PC one. Offline the new screen still has it (select_stage_first).
   1 here would put the map first online for everybody; it must then be so for every client that plays with this one. */
#define PORT_SELFLOW_ONLINE_STAGE_FIRST 0

static inline int PortSelFlow_StageFirst(int online, int tagNetwork, int newScreen, int settingStageFirst) {
    if (tagNetwork) {
        return 0; /* (the Tag Team network select has its own order, as before) */
    }
    if (online) {
        return PORT_SELFLOW_ONLINE_STAGE_FIRST; /* the same on every copy, whichever screen draws it */
    }
    return newScreen && settingStageFirst ? 1 : 0;
}

#endif
