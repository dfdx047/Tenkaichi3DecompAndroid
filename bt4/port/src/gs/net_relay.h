/* A relay address at a TURN server for the host of a match (net_relay.c). */
#ifndef NET_RELAY_H
#define NET_RELAY_H
#include <stdint.h>

struct sockaddr_in;

/* Asks `server` ("host:port") for a relay address, from socket `sock` (not blocking), with the name and password the
   matchmaking service handed out. Waits up to a few seconds. 1 = got one: `out` is "ip:port". */
int Relay_Allocate(int sock, const char *server, const char *username, const char *password, char *out, int size);
/* Lets packets from this address through (the other player's; the port does not matter). Asked again by Relay_Tick. */
void Relay_Permit(int sock, const struct sockaddr_in *peer);
/* Whether a relay address is held. */
int Relay_Active(void);
/* Keeps the relay address and the permissions alive: call often (it does something every few seconds at most). */
void Relay_Tick(int sock);
/* A packet just received from `from`. Returns 2: not the relay's, an ordinary packet. 1: a packet of the other
   player that came through the relay: *data / *len are its bytes inside pkt, and *from is now that player's
   address. 0: the relay's own business, nothing for the caller. */
int Relay_Unwrap(uint8_t *pkt, int n, struct sockaddr_in *from, uint8_t **data, int *len);
/* Whether packets to this address go through the relay (it is the address the relay last delivered from, and no
   packet has come from it directly since). */
int Relay_IsPeer(const struct sockaddr_in *to);
/* Sends `len` bytes to `to` through the relay. */
void Relay_Send(int sock, const void *data, int len, const struct sockaddr_in *to);
/* A packet came from this address directly: it is not (or no longer) reached through the relay. */
void Relay_SawDirect(const struct sockaddr_in *from);
/* Gives the relay address up (the end of a match or of the lobby). */
void Relay_Close(int sock);

#endif
