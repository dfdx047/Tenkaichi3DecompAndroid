#ifndef TAG_CLASH_SEQUENCE_H
#define TAG_CLASH_SEQUENCE_H
/* Stored inside the game state so rollback restores the prompt, score and
   penalty together. One edge per update; simultaneous buttons are a mistake. */
typedef struct TagClashSequence {
    unsigned int random;
    int button, score, blockedUntil, frame, accepted;
} TagClashSequence;
static unsigned int TagSequence_Random(TagClashSequence *s) {
    s->random ^= s->random << 13;
    s->random ^= s->random >> 17;
    s->random ^= s->random << 5;
    return s->random;
}
static void TagSequence_Init(TagClashSequence *s, unsigned int seed) {
    s->random = seed ? seed : 0x917afb35u;
    s->button = TagSequence_Random(s) % 4;
    s->score = s->blockedUntil = s->accepted = 0;
    s->frame = -1;
}
static int TagSequence_Press(TagClashSequence *s, int frame, unsigned int pressed) {
    if (s->frame == frame) return s->accepted;
    s->frame = frame; s->accepted = 0;
    pressed &= 15;
    if (frame < s->blockedUntil || !pressed) return 0;
    if (pressed != (1u << s->button)) { s->blockedUntil = frame + 12; return 0; }
    s->score++;
    s->button = (s->button + 1 + TagSequence_Random(s) % 3) % 4;
    return s->accepted = 1;
}
#endif
