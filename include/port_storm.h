#ifndef PORT_STORM_H
#define PORT_STORM_H
/* The full power storm's state (src/battle/stg_c.c draws it). It lives in port/src/gs/storm_snd.c, outside the
   game's variables: a rollback restores those (port/tools/android.py, the state list), and a storm stepped back would
   flash and thunder twice. */
#define STORM_BOLTS 2
typedef struct PortStormState {
    int on;            /* -1 not read yet, then Port_Setting("storm", 1) */
    float level;       /* 0 clear .. 1 fully clouded */
    unsigned long long last; /* the wall clock of the last step, ms */
    int next;          /* ms to the next strike */
    int flash;         /* ms since the strike, -1: none */
    int bolts;
    int kind[STORM_BOLTS];
    float x[STORM_BOLTS], y[STORM_BOLTS], h[STORM_BOLTS];
    int thunderIn;     /* ms to the thunder of the last strike, -1: none */
    int thunderLast;
    float thunderVol, thunderPan;
    unsigned int seed;
} PortStormState;
PortStormState *PortStorm_State(void);
#endif
