/*
 * Text between the two players of an online match: a newcomer can be shown the ropes, a server can be passed on.
 *
 * Nothing of it touches the game. A line travels in its own packet (net.c, T_CHAT: the sender's number for the line,
 * its length, its bytes) on the match's socket, and the check value the two copies compare (Port_NetDigest) knows
 * nothing of it, so it cannot make the two games differ. A copy without the chat ignores the packet as it ignores
 * any type it does not know. UDP may lose a packet: each line is sent three times, a little apart, and the other
 * side keeps the first that arrives (the numbers of the last few lines heard).
 *
 * The overlay (ui.cpp) says and reads lines on the render thread; the connection (net.c) takes what is to be sent
 * and gives what was heard on the game thread. Both go through this file, under its lock.
 *
 * What comes in is cleaned before it is kept: no control characters (bytes below 32, DEL, the C1 range), only whole
 * and well-formed UTF-8 characters, at most CHAT_TEXT bytes. It is only ever drawn as text (ImGui::TextUnformatted),
 * never formatted. A line said less than a second after the last is not sent, and the other side's lines are taken
 * no faster than an honest copy sends them.
 */
#include <SDL3/SDL.h>
#include <string.h>
#include "net_chat.h"

extern int Port_Setting(const char *name, int def); /* plat_settings.c */
extern void Port_SettingSave(const char *name, int value), Port_SettingsWrite(void);
extern const char *Port_NetName(int player);         /* gs/net.c */
extern int Port_NetMe(void);

enum { LINES = 32, OUTBOX = 4, SENDS = 3, RESEND_MS = 120, SAY_EVERY_MS = 1000, HEAR_EVERY_MS = 500, SEEN = 8 };

typedef struct {
    uint32_t seq, next; /* next: when it is sent again (ms, net.c's clock) */
    int left, len;      /* sends still to go; 0 = a free place */
    int fresh;          /* not sent yet: due at once */
    char text[CHAT_TEXT];
} Outgoing;

static SDL_SpinLock sLock;
static PortChatLine sLines[LINES];
static unsigned sCount;
static Outgoing sOut[OUTBOX];
static uint32_t sSeq;            /* the number of the last line this player said */
static uint32_t sSeen[SEEN];     /* the numbers of the last lines heard (0 = none) */
static unsigned sSeenAt;
static uint64_t sSaidMs, sHeardMs;
static int sSaid, sHeard;        /* (whether there was one: the clock may still read 0) */
static volatile int sEnabled = -1, sMuted;

/* Copies `len` bytes of `in` into `out` (CHAT_TEXT + 1) as described above, without spaces at either end. Returns the
   length kept. */
static int clean(const char *in, int len, char *out) {
    const unsigned char *s = (const unsigned char *)in;
    int i = 0, n = 0, end;

    while (i < len && s[i] == ' ') {
        i++;
    }
    while (i < len) {
        unsigned c = s[i];
        int k = c < 0x80 ? 1 : c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 0;
        int ok = k != 0 && i + k <= len, j;
        for (j = 1; ok && j < k; j++) {
            ok = (s[i + j] & 0xC0) == 0x80;
        }
        if (ok && k == 1) {
            ok = c >= 0x20 && c != 0x7F;
        } else if (ok && k == 2) {
            ok = !(c == 0xC2 && s[i + 1] < 0xA0); /* (U+0080..U+009F: the C1 controls) */
        } else if (ok && k == 3) {
            ok = !(c == 0xE0 && s[i + 1] < 0xA0) && !(c == 0xED && s[i + 1] >= 0xA0); /* (too long a form; the surrogates) */
        } else if (ok && k == 4) {
            ok = !(c == 0xF0 && s[i + 1] < 0x90) && !(c == 0xF4 && s[i + 1] >= 0x90); /* (too long a form; past U+10FFFF) */
        }
        if (!ok) {
            i++; /* a byte that does not begin a whole character is left out */
            continue;
        }
        if (n + k > CHAT_TEXT) {
            break;
        }
        memcpy(out + n, s + i, (size_t)k);
        n += k;
        i += k;
    }
    for (end = n; end > 0 && out[end - 1] == ' '; end--) {
    }
    out[end] = '\0';
    return end;
}

/* Who says a line: looked up before the lock is taken (net.c's names are not this file's to hold it over). The two
   players may have the same name: this player's lines then say so ("(you)"), not only by their colour. */
static const char *sender_of(int mine) {
    static char both[sizeof(((PortChatLine *)0)->sender)];
    const char *name = Port_NetName(mine ? Port_NetMe() : !Port_NetMe());
    const char *other = Port_NetName(mine ? !Port_NetMe() : Port_NetMe());

    if (name == NULL || name[0] == '\0') {
        return mine ? "You" : "Rival";
    }
    if (mine && other != NULL && strcmp(name, other) == 0) {
        SDL_snprintf(both, sizeof(both), "%s (you)", name);
        return both;
    }
    return name;
}

static void keep(const char *text, int mine, const char *sender) { /* (under the lock) */
    PortChatLine *l = &sLines[sCount % LINES];
    SDL_strlcpy(l->text, text, sizeof(l->text));
    SDL_strlcpy(l->sender, sender, sizeof(l->sender));
    l->atMs = SDL_GetTicks();
    l->mine = mine;
    sCount++;
}

/* A mute stays (until the program ends): a rematch with the same player is a new session. */
void PortChat_Reset(void) {
    SDL_LockSpinlock(&sLock);
    sCount = 0;
    sSeq = 0;
    sSeenAt = 0;
    sSaid = 0;
    sHeard = 0;
    sSaidMs = 0;
    sHeardMs = 0;
    memset(sLines, 0, sizeof(sLines));
    memset(sOut, 0, sizeof(sOut));
    memset(sSeen, 0, sizeof(sSeen));
    SDL_UnlockSpinlock(&sLock);
}

int PortChat_Enabled(void) {
    if (sEnabled < 0) {
        sEnabled = Port_Setting("chat", 1) != 0;
    }
    return sEnabled;
}

void PortChat_EnabledSet(int on) {
    sEnabled = on != 0;
    Port_SettingSave("chat", sEnabled);
    Port_SettingsWrite();
}

int PortChat_Muted(void) {
    return sMuted;
}

void PortChat_MutedSet(int on) {
    sMuted = on != 0;
}

int PortChat_Say(const char *text) {
    char line[CHAT_TEXT + 1];
    uint64_t now = SDL_GetTicks();
    int len, k, slot = 0;
    const char *name;

    if (text == NULL || !PortChat_Enabled()) {
        return 0;
    }
    len = clean(text, (int)strlen(text), line);
    if (len == 0 || (sSaid && now - sSaidMs < SAY_EVERY_MS)) {
        return 0;
    }
    sSaid = 1;
    sSaidMs = now;
    name = sender_of(1);
    SDL_LockSpinlock(&sLock);
    keep(line, 1, name);
    for (k = 0; k < OUTBOX; k++) { /* a free place, else the oldest line's */
        if (sOut[k].left == 0) {
            slot = k;
            break;
        }
        if ((int32_t)(sOut[k].seq - sOut[slot].seq) < 0) {
            slot = k;
        }
    }
    if (sSeq == 0) { /* (a random start: the other side's memory of numbers is not mistaken across sessions) */
        sSeq = (uint32_t)(SDL_GetPerformanceCounter() ^ SDL_GetTicksNS()) >> 4;
    }
    if (++sSeq == 0) {
        sSeq = 1;
    }
    sOut[slot].seq = sSeq;
    sOut[slot].left = SENDS;
    sOut[slot].fresh = 1;
    sOut[slot].len = len;
    memcpy(sOut[slot].text, line, (size_t)len);
    SDL_UnlockSpinlock(&sLock);
    return 1;
}

unsigned PortChat_Count(void) {
    unsigned n;
    SDL_LockSpinlock(&sLock);
    n = sCount;
    SDL_UnlockSpinlock(&sLock);
    return n;
}

int PortChat_Line(unsigned index, PortChatLine *out) {
    int ok;
    SDL_LockSpinlock(&sLock);
    ok = index < sCount && sCount - index <= LINES;
    if (ok) {
        *out = sLines[index % LINES];
    }
    SDL_UnlockSpinlock(&sLock);
    return ok;
}

int PortChat_Outgoing(uint32_t nowMs, uint32_t *seq, char *text) {
    int k, len = -1;
    SDL_LockSpinlock(&sLock);
    for (k = 0; k < OUTBOX && len < 0; k++) {
        Outgoing *o = &sOut[k];
        if (o->left > 0 && (o->fresh || (int32_t)(nowMs - o->next) >= 0)) {
            o->fresh = 0;
            o->left--;
            o->next = nowMs + RESEND_MS;
            *seq = o->seq;
            len = o->len;
            memcpy(text, o->text, (size_t)len);
        }
    }
    SDL_UnlockSpinlock(&sLock);
    return len;
}

void PortChat_Heard(uint32_t seq, const char *text, int len) {
    char line[CHAT_TEXT + 1];
    uint64_t now = SDL_GetTicks();
    int k;
    const char *name;

    if (seq == 0 || len <= 0 || !PortChat_Enabled() || sMuted) {
        return;
    }
    name = sender_of(0);
    SDL_LockSpinlock(&sLock);
    for (k = 0; k < SEEN; k++) {
        if (sSeen[k] == seq) { /* a line already heard, sent again */
            SDL_UnlockSpinlock(&sLock);
            return;
        }
    }
    sSeen[sSeenAt++ % SEEN] = seq;
    if ((!sHeard || now - sHeardMs >= HEAR_EVERY_MS) && clean(text, len, line) > 0) {
        sHeard = 1;
        sHeardMs = now;
        keep(line, 0, name);
    }
    SDL_UnlockSpinlock(&sLock);
}
