#ifndef MENU_PORT_SELCANON_H
#define MENU_PORT_SELCANON_H

#include <stdint.h>

/* The canonical result of a character select: only what the game takes into the battle (the picks it keeps in gProgress, the stage
   cell, the music), as plain 32-bit words. Both screens (the classic and the new one) end in the same game state (the new one only
   reads it: include/menu/port_charsel.h, CsView is const), so this is the one view of it that diagnostics, tests and a PC copy's
   logs can compare. It holds no pointers, no addresses and nothing of the screen (which one it was, its settings, the layout).

   The hash serialises each field explicitly, little endian, so struct padding and uninitialised bytes never reach it. */
#define PORT_SELCANON_MEMBERS 5      /* a team's side; a duel's side is one */
#define PORT_SELCANON_MEMBER_WORDS 8 /* form, plate, costume, character, 4 words of items */
#define PORT_SELCANON_WORDS (PORT_SELCANON_MEMBERS * PORT_SELCANON_MEMBER_WORDS)

typedef struct PortSelCanon {
    int32_t kind;    /* 1 duel (CharSel), 2 team (TeamSel) */
    int32_t players; /* 0 pad against CPU, 1 pad against pad, 2 CPU against CPU */
    int32_t stage;   /* the stage grid's cell: col + row * 6 */
    int32_t bgm;     /* the music id */
    int32_t words[2][PORT_SELCANON_WORDS];
} PortSelCanon;

/* One chosen fighter from the game's 12-word layout, the same in CharSelSide/CharSelPick and TsMember:
   col, row, form, customCol, customRow, plate, costume, character, items[4]. What was CHOSEN is kept; the four cursor positions are not
   (where the cursor rests is not a choice, and a PC copy that reached the same fighter another way must still compare equal). */
static inline void PortSelCanon_PutMember(PortSelCanon *c, int side, int slot, const int32_t *raw12) {
    int32_t *w = &c->words[side][slot * PORT_SELCANON_MEMBER_WORDS];
    int i;

    w[0] = raw12[2]; /* form */
    w[1] = raw12[5]; /* plate (item set) */
    w[2] = raw12[6]; /* costume */
    w[3] = raw12[7]; /* the character */
    for (i = 0; i < 4; i++) {
        w[4 + i] = raw12[8 + i];
    }
}

static inline void PortSelCanon_Clear(PortSelCanon *c) {
    int32_t *p = (int32_t *)c;
    unsigned i;

    for (i = 0; i < sizeof(*c) / sizeof(int32_t); i++) {
        p[i] = 0;
    }
}

static inline uint64_t PortSelCanon_Hash(const PortSelCanon *c) {
    uint64_t h = 0xCBF29CE484222325ull; /* FNV-1a, as net.c's check values */
    int32_t head[4];
    int s, i, b;

    head[0] = c->kind;
    head[1] = c->players;
    head[2] = c->stage;
    head[3] = c->bgm;
#define PORT_SELCANON_MIX(v) do { uint32_t u_ = (uint32_t)(v); for (b = 0; b < 4; b++) { h = (h ^ ((u_ >> (8 * b)) & 0xFF)) * 0x100000001B3ull; } } while (0)
    for (i = 0; i < 4; i++) {
        PORT_SELCANON_MIX(head[i]);
    }
    for (s = 0; s < 2; s++) {
        for (i = 0; i < PORT_SELCANON_WORDS; i++) {
            PORT_SELCANON_MIX(c->words[s][i]);
        }
    }
#undef PORT_SELCANON_MIX
    return h;
}

#endif
