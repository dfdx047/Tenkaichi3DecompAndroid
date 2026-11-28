/* Z Warriors Online: the room as the screens see it (port/src/plat_zwnet.c). */
#ifndef PORT_ZW_NET_H
#define PORT_ZW_NET_H

enum { ZN_IDLE, ZN_CONNECTING, ZN_ROOM, ZN_SEARCHING, ZN_FAILED };
enum { ZNE_NONE, ZNE_NET, ZNE_NOROOM, ZNE_FULL, ZNE_VERSION, ZNE_DISC, ZNE_BUSY, ZNE_OFFICIAL, ZNE_TIME, ZNE_OTHER, ZNE_WARRIOR };

typedef struct ZwNetInfo {
    int state, error, host, ready;
    char code[8];
    int peerHere, peerReady;
    int peer;         /* the rival's warrior, made here: its place in the library, -1 not yet */
    int libraryGen;   /* raised when a warrior was made (the screens read the library again) */
    int rules[6];
    int waiting;
    int quick;        /* the room is a quick match's */
} ZwNetInfo;

void Port_ZwNetHost(int warrior, const int *rules);
void Port_ZwNetJoin(int warrior, const char *code);
void Port_ZwNetQuick(int warrior, int madeOk);
void Port_ZwNetReady(int ready);
void Port_ZwNetWarrior(int warrior);
void Port_ZwNetRules(const int *rules);
void Port_ZwNetLeave(void);
void Port_ZwNetStart(int now); /* the host: the fight */
void Port_ZwNetCopyCode(void); /* the room's code onto the clipboard */
void Port_ZwNetTick(void); /* once a frame while the online screens are open (and after leaving, to send it) */
void Port_ZwNetGet(ZwNetInfo *out);
#endif
