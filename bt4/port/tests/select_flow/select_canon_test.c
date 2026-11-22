/* The canonical selection (include/menu/port_selcanon.h): the hash is deterministic, covers every chosen field, and ignores what is not a choice.
   gcc -Wall -Werror -I../../../include -o /tmp/select_canon_test select_canon_test.c && /tmp/select_canon_test */
#include <stdio.h>
#include <string.h>
#include "menu/port_selcanon.h"

static int fails, runs;
#define CHECK(c, ...) do { runs++; if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

/* the game's 12-word layout of one fighter (CharSelSide / TsMember): col,row,form,customCol,customRow,plate,costume,character,items[4] */
static void member(int32_t *raw, int col, int row, int form, int plate, int costume, int chara, int item0) {
    memset(raw, 0, 12 * sizeof(int32_t));
    raw[0] = col; raw[1] = row; raw[2] = form; raw[5] = plate; raw[6] = costume; raw[7] = chara; raw[8] = item0;
}

static uint64_t duel(int c0, int r0, int f0, int p0, int k0, int ch0, int c1, int r1, int f1, int p1, int k1, int ch1, int stage, int bgm, int players) {
    PortSelCanon c;
    int32_t a[12], b[12];

    member(a, c0, r0, f0, p0, k0, ch0, 0);
    member(b, c1, r1, f1, p1, k1, ch1, 0);
    PortSelCanon_Clear(&c);
    c.kind = 1; c.players = players; c.stage = stage; c.bgm = bgm;
    PortSelCanon_PutMember(&c, 0, 0, a);
    PortSelCanon_PutMember(&c, 1, 0, b);
    return PortSelCanon_Hash(&c);
}

int main(void) {
    PortSelCanon x, y;
    int32_t m[12];
    int s, i;
    uint64_t base;

    /* 1. no padding: the structure is words only, so the hash is of the fields and nothing else */
    CHECK(sizeof(PortSelCanon) == (4 + 2 * PORT_SELCANON_WORDS) * sizeof(int32_t), "the structure has padding");
    /* 2. determinism, and a structure that held garbage before is the same once cleared */
    memset(&x, 0xFF, sizeof(x)); PortSelCanon_Clear(&x);
    memset(&y, 0x00, sizeof(y)); PortSelCanon_Clear(&y);
    CHECK(PortSelCanon_Hash(&x) == PortSelCanon_Hash(&y), "garbage reaches the hash");
    CHECK(PortSelCanon_Hash(&x) == PortSelCanon_Hash(&x), "not deterministic");
    /* 3. the same choice reached with the cursor elsewhere (another way to it: another screen, a PC copy) is the same hash */
    base = duel(3, 1, 0, 0, 2, 17, 5, 2, 1, 0, 0, 40, 12, 7, 1);
    CHECK(base == duel(0, 0, 0, 0, 2, 17, 6, 0, 1, 0, 0, 40, 12, 7, 1), "cursor positions reach the hash");
    /* 4. everything that is a choice changes it */
    CHECK(base != duel(3, 1, 1, 0, 2, 17, 5, 2, 1, 0, 0, 40, 12, 7, 1), "form not covered");
    CHECK(base != duel(3, 1, 0, 1, 2, 17, 5, 2, 1, 0, 0, 40, 12, 7, 1), "plate not covered");
    CHECK(base != duel(3, 1, 0, 0, 3, 17, 5, 2, 1, 0, 0, 40, 12, 7, 1), "costume not covered");
    CHECK(base != duel(3, 1, 0, 0, 2, 18, 5, 2, 1, 0, 0, 40, 12, 7, 1), "character not covered");
    CHECK(base != duel(3, 1, 0, 0, 2, 17, 5, 2, 0, 0, 0, 40, 12, 7, 1), "side 1 form not covered");
    CHECK(base != duel(3, 1, 0, 0, 2, 17, 5, 2, 1, 0, 1, 40, 12, 7, 1), "side 1 costume not covered");
    CHECK(base != duel(3, 1, 0, 0, 2, 17, 5, 2, 1, 0, 0, 41, 12, 7, 1), "side 1 character not covered");
    CHECK(base != duel(3, 1, 0, 0, 2, 17, 5, 2, 1, 0, 0, 40, 13, 7, 1), "stage not covered");
    CHECK(base != duel(3, 1, 0, 0, 2, 17, 5, 2, 1, 0, 0, 40, 12, 8, 1), "music not covered");
    CHECK(base != duel(3, 1, 0, 0, 2, 17, 5, 2, 1, 0, 0, 40, 12, 7, 0), "players not covered");
    /* the sides are not interchangeable: A with Goku and B with Vegeta is another match from the other way round */
    CHECK(duel(0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 40, 1, 1, 1) != duel(0, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0, 17, 1, 1, 1), "the sides are interchangeable");
    /* 5. repeated fighters: both sides with the same one are valid and differ from one side with each */
    CHECK(duel(0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 17, 1, 1, 1) != duel(0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 40, 1, 1, 1), "mirror match equals another");
    /* 6. every word of a team (5 slots x 8 words x 2 sides) is covered, the ones a member does not use too (items) */
    PortSelCanon_Clear(&x); x.kind = 2;
    base = PortSelCanon_Hash(&x);
    for (s = 0; s < 2; s++) for (i = 0; i < PORT_SELCANON_WORDS; i++) {
        PortSelCanon_Clear(&y); y.kind = 2; y.words[s][i] = 1;
        CHECK(PortSelCanon_Hash(&y) != base, "team word %d of side %d not covered", i, s);
    }
    /* 7. a team member put in slot k lands in its own words: slot 2 is not slot 3 */
    member(m, 9, 9, 1, 0, 2, 77, 5);
    PortSelCanon_Clear(&x); PortSelCanon_PutMember(&x, 0, 2, m);
    PortSelCanon_Clear(&y); PortSelCanon_PutMember(&y, 0, 3, m);
    CHECK(PortSelCanon_Hash(&x) != PortSelCanon_Hash(&y), "team slots are interchangeable");
    /* 8. a fixed vector: the value must not drift between builds or compilers (a log of a PC copy is compared with an Android one) */
    CHECK(duel(0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 40, 1, 1, 1) == 0x4c7ea245eb2ad24cull, "the reference hash drifted");
    printf("reference hash (duel, fighters 17 vs 40, stage 1, music 1, pad vs pad): %016llx\n",
           (unsigned long long)duel(0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 40, 1, 1, 1));
    printf("%d checks, %d failed\n", runs, fails);
    return fails != 0;
}
