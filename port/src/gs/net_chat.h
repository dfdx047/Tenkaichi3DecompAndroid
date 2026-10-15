/* Text between the two players of an online match (net_chat.c): what was said, kept for the overlay (ui.cpp), and
   what waits to go out (net.c, T_CHAT). C and C++. */
#ifndef NET_CHAT_H
#define NET_CHAT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CHAT_TEXT 160 /* bytes of a line, without its terminator (whole UTF-8 characters) */

typedef struct PortChatLine {
    char text[CHAT_TEXT + 1];
    char sender[32];
    uint64_t atMs; /* SDL_GetTicks() when it was said or heard */
    int mine;      /* 1 = this player said it */
} PortChatLine;

/* The overlay (render thread). */
int PortChat_Say(const char *text); /* cleans it, keeps it and sends it. 0 = nothing to send, or said too soon after the last */
unsigned PortChat_Count(void);      /* lines kept since the start (the newest is Count - 1) */
int PortChat_Line(unsigned index, PortChatLine *out); /* 0 = no longer kept */
int PortChat_Enabled(void);         /* setting "chat" (on by default) */
void PortChat_EnabledSet(int on);
int PortChat_Muted(void);           /* the other player's lines are not kept (until the program ends) */
void PortChat_MutedSet(int on);
void PortChat_Reset(void);          /* clears all lines, outbox, and sequence tracking for a new session */

/* The connection (game thread, net.c). */
int PortChat_Outgoing(uint32_t nowMs, uint32_t *seq, char *text); /* a line due to be sent now: its length, -1 = none */
void PortChat_Heard(uint32_t seq, const char *text, int len);     /* a line of the other player came in */

#ifdef __cplusplus
}
#endif
#endif
