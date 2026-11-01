/* The fight panels: what the side bars show of a fight (gs/ui.cpp draws them). port/src/fight_panels.c reads the
   fight once per blank on the game's thread; the render thread takes a copy. C and C++. */
#ifndef PORT_FIGHT_PANELS_H
#define PORT_FIGHT_PANELS_H
#ifdef __cplusplus
extern "C" {
#endif

#define FP_MEMBERS 5 /* a side's member slots (include/battle/btl_char_member.h: BTL_MEMBER_SLOTS) */
#define FP_PATH 260

typedef struct FpMember {
    int present;  /* the slot holds a member */
    int chara;    /* character id (the active member's: the one on screen, after a transformation) */
    int costume;  /* costume code (8 and up: a skin of the mod grid) */
    int hp, hpMax; /* 10000 per bar */
    char path[FP_PATH]; /* host path of its large portrait ("" = none found) */
} FpMember;

typedef struct FpSide {
    int found;        /* the side has a fighter */
    int active;       /* the member that is fighting */
    int count;        /* member slots in use */
    int next;         /* the member a switch would bring in, -1 none */
    int ki, kiMax;    /* the active member's: 20000 per bar */
    int blast, blastMax; /* 100000 per stock */
    int maxPower;     /* 0..30000: the powered-up mode's timer */
    int poweredUp;
    int comboHits, comboDamage, comboTimer; /* the combo this side is landing on the other */
    /* what the game's HUD was given for this side (src/battle/hud_*.c, Port_HudFeed): */
    int statMask, statUp; /* status icons: bits 0..3 = attack, speed, defence, ki changed; set in statUp = raised */
    int hudHits;          /* the combo counter (below 2: not shown) */
    int hudDamage;        /* the combo damage (negative: not shown) */
    unsigned hitSeq;      /* counts each new hit count given */
    int msg;              /* the last event message (0..4) */
    unsigned msgSeq;      /* counts the messages given */
    FpMember m[FP_MEMBERS];
} FpSide;

typedef struct FpState {
    int battle;  /* a battle's fighters are there */
    int gauges;  /* the fight is on with its gauges shown */
    int timer;      /* what the timer shows: seconds left (-1: no limit), or a count */
    int timerClock; /* 1 = seconds, 0 = a count */
    int timerSet;   /* the HUD was given a timer this fight */
    FpSide side[2];
} FpState;

void PortPanels_GameTick(void);      /* the game's thread, once per blank */
int PortPanels_Read(FpState *out);   /* any thread: a consistent copy; 0 = nothing read yet */

#ifdef __cplusplus
}
#endif
#endif
