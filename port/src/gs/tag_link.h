/* The Tag Team mod's rooms over the internet (tag_link.c): a room code from the Tag Team Online service and reliable
   connections over UDP, read and written as tag_net.c's sockets were. */
#ifndef TAG_LINK_H
#define TAG_LINK_H

/* The service built in (port/tagnet-worker); <game folder>/tag_online_url.txt names another. */
#ifndef TAG_ONLINE_URL
#define TAG_ONLINE_URL "https://tagteam-online.regulusplay.workers.dev"

#endif

#define TL_SLOTS 24
enum { TK_LINK_FREE, TK_LINK_CONNECTING, TK_LINK_OPEN, TK_LINK_CLOSED };

/* Opens an online room (its code comes a moment later: TagLink_Status). 0: could not. */
int TagLink_Host(const char *name, int version);
/* Joins the room of `code`: its owner's connection is the slot returned (-1: could not). */
int TagLink_Join(const char *code, const char *name, int version, int watch);
/* Closes everything (the room is left; the owner's closes it). */
void TagLink_Stop(void);
int TagLink_Active(void);
/* Once a frame (with the room's sockets): packets in and out, the relay, the routers opened. */
void TagLink_Pump(void);
void TagLink_FlushAcks(void); /* the end of a pump: the acknowledgements nothing carried back yet */
/* The owner: a player's new connection (its slot), -1 none. */
int TagLink_Accept(void);
int TagLink_State(int slot);
/* As send() / recv() on a stream: bytes taken / read (0: none now), -1: the connection is gone. */
int TagLink_Send(int slot, const void *data, int len);
int TagLink_Recv(int slot, void *out, int max);
void TagLink_Close(int slot);
int TagLink_SendDatagram(int slot, const void *data, int len); /* (prototype: outside the stream) */
int TagLink_RecvDatagram(int slot, void *out, int max);
int TagLink_RttMs(int slot); /* (prototype: the measured round trip, -1 unknown) */
/* What the online part is doing (a line for the screen), and the room's code once it has one ("" until then). */
void TagLink_Status(char *out, int size, char *code, int codeSize);


/* The quick 2 vs 2 (the service's /v1/quick): a search for four players that makes a room of two teams of two. phase: 0 none, 1 searching,
   2 found (the room: code, roomKey, id, role, team, the four names), 3 failed (error). */
typedef struct TagQuickState {
    int phase, waiting, need;
    int role, team, id;
    char code[16], roomKey[48];
    char names[4][28];
    int have[4];
    char error[96];
} TagQuickState;
int TagQuick_Start(const char *name, int version);
void TagQuick_Poll(TagQuickState *out);
void TagQuick_Cancel(void);
void TagQuick_Clear(void);
/* Takes the room the search found (before TagLink_Host for its owner, role 0, or TagLink_Join with its code for the others). */
void TagLink_Adopt(const char *code, const char *key, int id);


#endif
