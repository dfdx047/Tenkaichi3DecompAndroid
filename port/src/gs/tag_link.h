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
/* The owner: a player's new connection (its slot), -1 none. */
int TagLink_Accept(void);
int TagLink_State(int slot);
/* As send() / recv() on a stream: bytes taken / read (0: none now), -1: the connection is gone. */
int TagLink_Send(int slot, const void *data, int len);
int TagLink_Recv(int slot, void *out, int max);
void TagLink_Close(int slot);
/* What the online part is doing (a line for the screen), and the room's code once it has one ("" until then). */
void TagLink_Status(char *out, int size, char *code, int codeSize);

#endif
