/* The order of the character select must not depend on which screen a machine shows.
   Build and run:  gcc -Wall -Werror -I../../../include -o /tmp/select_flow_test select_flow_test.c && /tmp/select_flow_test
   It compiles include/menu/port_selflow.h itself (the one the game uses) and compares it with the rule the game had before. */
#include <stdio.h>
#include "menu/port_selflow.h"

/* what menu_c_e.c / menu_e_b.c computed before (the screen decided): kept here only to show the bug the test guards */
static int legacy(int online, int tagNet, int newScreen, int setting) {
    return newScreen && !tagNet && (online || setting) ? 1 : 0;
}

static int fails, runs;
#define CHECK(c, ...) do { runs++; if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
    int online, tag, uiA, uiB, setA, setB, legacyMixedDiffers = 0, legacyMixedCases = 0;

    /* 1. online: whatever the two machines' screens and settings, both run the same order (the two pairs: 4 combinations of screen
          x 4 of setting, as the host A and the guest B, then the other way round) */
    for (tag = 0; tag < 2; tag++)
    for (uiA = 0; uiA < 2; uiA++) for (uiB = 0; uiB < 2; uiB++)
    for (setA = 0; setA < 2; setA++) for (setB = 0; setB < 2; setB++) {
        int a = PortSelFlow_StageFirst(1, tag, uiA, setA);
        int b = PortSelFlow_StageFirst(1, tag, uiB, setB);
        CHECK(a == b, "online mix: A(screen %d, setting %d)=%d B(screen %d, setting %d)=%d tag %d", uiA, setA, a, uiB, setB, b, tag);
        if (legacy(1, tag, uiA, setA) != legacy(1, tag, uiB, setB)) {
            legacyMixedDiffers++;
        }
        legacyMixedCases++;
    }
    /* 2. the same screen on both: the result is the one that was already (new screen online: the map first, as released) */
    CHECK(PortSelFlow_StageFirst(1, 0, 1, 1) == 1 && legacy(1, 0, 1, 1) == 1, "new screen on both: unchanged");
    CHECK(PortSelFlow_StageFirst(1, 0, 1, 0) == 1 && legacy(1, 0, 1, 0) == 1, "new screen on both, setting off: unchanged (the map first online)");
    /* 3. offline is exactly what it was, for every input */
    for (online = 0; online < 1; online++)
    for (tag = 0; tag < 2; tag++) for (uiA = 0; uiA < 2; uiA++) for (setA = 0; setA < 2; setA++) {
        CHECK(PortSelFlow_StageFirst(online, tag, uiA, setA) == legacy(online, tag, uiA, setA),
              "offline changed: tag %d screen %d setting %d", tag, uiA, setA);
    }
    /* 4. the Tag network select keeps its own order */
    for (uiA = 0; uiA < 2; uiA++) CHECK(PortSelFlow_StageFirst(1, 1, uiA, 1) == 0, "tag network");
    /* 5. the old rule really was broken for a mixed pair (the test would not guard anything if it were not) */
    CHECK(legacyMixedDiffers > 0, "the old rule never differed: the test is not testing the bug");
    printf("%d checks, %d failed. (the old rule gave a different order to the two machines in %d of %d online pairings)\n",
           runs, fails, legacyMixedDiffers, legacyMixedCases);
    return fails != 0;
}
