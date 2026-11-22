/* tag_link.c on Linux against the local service (server.mjs): one host, one or more joiners, a stream echoed back,
   with packets dropped on purpose (DROP percent). */
#define _GNU_SOURCE
#include <SDL3/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
static int gDrop;
static Uint64 gCutAt, gOpenAt; /* CUT_MS: from this time on the direct way loses everything (the room relay still works) */
static ssize_t drop_sendto(int s, const void *b, size_t n, int f, const struct sockaddr *a, socklen_t l) {
    if (gDrop && (rand() % 100) < gDrop) return (ssize_t)n;
    if (gCutAt && SDL_GetTicks() >= gCutAt) return (ssize_t)n;
    if (gOpenAt && SDL_GetTicks() < gOpenAt) return (ssize_t)n; /* OPEN_MS: the direct way works only from then on */
    if (getenv("CUT4") && a && a->sa_family == AF_INET) return (ssize_t)n; /* CUT4: no IPv4 at all (operators' NAT) */
    if (getenv("CUT6") && a && a->sa_family == AF_INET6) return (ssize_t)n;
    return sendto(s, b, n, f, a, l);
}
/* Stand-ins for what tag_link.c takes from other files of the game (these are simulated; tag_link.c itself is the real
   one, #included below):
     net_match.c   TagLink_Stun (no STUN: the joiner is told the host's loopback address), TagLink_Lan, TagLink_Http
                   (the local service, server.mjs, through curl), gPortInBackground (the app in the background: never here)
     net_relay.c   the TURN relay: not used (Relay_Active is 0, Relay_AllocateVia gives no address)
     plat_file.c   Port_FileRoot: the harness's folder (TAGLINK_TEST_DIR, default /tmp/taglink-test) */
static const char *test_dir(void) {
    static char dir[512];
    if (!dir[0]) {
        snprintf(dir, sizeof(dir), "%s", getenv("TAGLINK_TEST_DIR") ? getenv("TAGLINK_TEST_DIR") : "/tmp/taglink-test");
        mkdir(dir, 0700);
    }
    return dir;
}
volatile int gPortInBackground;
int TagLink_Stun(int sock, char *out, int size) { (void)sock; (void)out; (void)size; if (getenv("STUN_MS")) SDL_Delay((Uint32)atoi(getenv("STUN_MS"))); return 0; } /* (STUN_MS: a slow STUN server) */
int TagLink_Lan(int port, char *out, int size) { snprintf(out, size, "127.0.0.1:%d", port); return 1; }
/* FAIL_POLLS="2:503,4:timeout": the service's answer to those polls of this game (counted from 1) is replaced:
     503      {"error":"unavailable"} (the Worker's own error, a database hiccup)
     500      a Cloudflare error page (not JSON)
     timeout  no answer (after 2 s)
     noroom   {"error":"no room"} (the room, or this player's place in it, is gone)
   At the end the calls are counted: CALLS polls P (after the first replaced one A), leaves L (before the game's own end E). */
static int gPolls, gLeaves, gEarlyLeaves, gPollsAfterFault, gFaulted, gEnding, gBackgroundPolls;
static void calls_report(void) {
    if (getenv("FAIL_POLLS") || getenv("COUNT_CALLS"))
        fprintf(stderr, "CALLS polls %d (after the first replaced one %d), leaves %d (before the game's own end %d), in the background %d\n",
                gPolls, gPollsAfterFault, gLeaves, gEarlyLeaves, gBackgroundPolls);
}
static const char *poll_fault(int n) {
    const char *list = getenv("FAIL_POLLS"), *p;
    char want[16];
    if (list == NULL) return NULL;
    snprintf(want, sizeof(want), "%d:", n);
    for (p = list; p != NULL && *p; p = strchr(p, ',') ? strchr(p, ',') + 1 : NULL)
        if (strncmp(p, want, strlen(want)) == 0) return p + strlen(want);
    return NULL;
}
int TagLink_Http(const char *url, const char *json, char *out, int size) {
    char cmd[2048], file[600]; FILE *f; size_t n;
    const char *path = strstr(url, "/v1/");
    static int registered;
    if (!registered) { registered = 1; atexit(calls_report); }
    if (strstr(url, "/v1/leave")) { gLeaves++; if (!gEnding) gEarlyLeaves++; }
    if (strstr(url, "/v1/relay") && getenv("FAKE_RELAY")) { snprintf(out, size, "{\"turn\":{\"server\":\"127.0.0.1:3478\",\"username\":\"u\",\"credential\":\"p\"}}"); return 1; }
    if (strstr(url, "/v1/poll") && gPortInBackground) gBackgroundPolls++;
    if (strstr(url, "/v1/poll")) {
        const char *fault = poll_fault(++gPolls);
        if (gFaulted) gPollsAfterFault++;
        if (fault != NULL) {
            gFaulted = 1;
            if (!strncmp(fault, "503", 3)) { snprintf(out, size, "{\"error\":\"unavailable\"}"); return 1; }
            if (!strncmp(fault, "500", 3)) { snprintf(out, size, "<html><body>error code: 500</body></html>"); return 1; }
            if (!strncmp(fault, "noroom", 6)) { snprintf(out, size, "{\"error\":\"no room\"}"); return 1; }
            if (!strncmp(fault, "timeout", 7)) { SDL_Delay(2000); out[0] = 0; return 0; }
        }
    }
    snprintf(file, sizeof(file), "%s/tl-%d-%d.json", test_dir(), (int)getpid(), (int)SDL_GetCurrentThreadID() & 0xffff);
    f = fopen(file, "w"); fputs(json, f); fclose(f);
    snprintf(cmd, sizeof(cmd), "curl -s -X POST -H 'content-type: application/json' --data-binary @%s http://127.0.0.1:8787%s", file, path);
    f = popen(cmd, "r"); n = fread(out, 1, size - 1, f); pclose(f); out[n] = 0;
    return n > 0;
}
const char *Port_FileRoot(void) { return test_dir(); }
int Relay_Allocate(int a, const char *b, const char *c, const char *d, char *e, int f) { return 0; }
/* RELAY_ALLOC_MS: an allocation that takes that long (as the real one's ten rounds of 0.7 s), "sending" on its socket
   every 100 ms; it notes when that socket number has become the current room's own while it was still at work. */
static int gAllocSock = -1, gAllocOnOtherRoomSocket;
static int gen_now(void);
static int sock_now(void);
int Relay_AllocateVia(int sock, const char *server, const char *username, const char *password, char *out, int size,
                      int (*recvfn)(uint8_t *buf, int max, struct sockaddr_in *from)) {
    int startGen = gen_now(), t;
    (void)server; (void)username; (void)password; (void)out; (void)size; (void)recvfn;
    gAllocSock = sock;
    for (t = 0; getenv("RELAY_ALLOC_MS") && t < atoi(getenv("RELAY_ALLOC_MS")); t += 100) {
        if (gen_now() != startGen && sock == sock_now()) gAllocOnOtherRoomSocket++;
        SDL_Delay(100);
    }
    return 0;
}
void Relay_Permit(int s, const struct sockaddr_in *p) {}
int Relay_Active(void) { return 0; }
void Relay_Tick(int s) {}
int Relay_Unwrap(uint8_t *pkt, int n, struct sockaddr_in *from, uint8_t **data, int *len) { *data = pkt; *len = n; return 2; }
int Relay_IsPeer(const struct sockaddr_in *to) { return 0; }
void Relay_Send(int s, const void *d, int l, const struct sockaddr_in *t) {}
void Relay_SawDirect(const struct sockaddr_in *f) {}
void Relay_Close(int s) {}
#define sendto(a,b,c,d,e,f) drop_sendto(a,b,c,d,e,f)
#define TAG_ONLINE_URL "https://test.local"
#include "tag_link.c"
#undef sendto

static int background_thread(void *arg) {
    const unsigned *bg = arg;
    SDL_Delay(bg[0]);
    gPortInBackground = 1;
    fprintf(stderr, "harness: the app is in the background for %u ms\n", bg[1]);
    SDL_Delay(bg[1]);
    gPortInBackground = 0;
    fprintf(stderr, "harness: the app is back\n");
    return 0;
}

#define TOTAL (300 * 1024)
static int gen_now(void) { return SDL_GetAtomicInt(&L.gen); }
static int sock_now(void) { return L.sock; }

/* `harness restart` (P3): the owner leaves a room while its service thread is still at work (asking the relay for an
   address) and opens another at once, whose STUN takes a while. The old thread must not touch the new room: not its
   "busy" (the new STUN's answer would then be read by the game thread and lost), not its socket. */
static int restart_test(void) {
    char st[160], code[16], body[256], answer[1024], url[300];
    Uint64 t0;
    int oldSock, newSock, clobbered = 0, sampled = 0, stunMs = getenv("STUN_MS") ? atoi(getenv("STUN_MS")) : 0;
    TagLink_Host("A", 5);
    for (t0 = SDL_GetTicks(); SDL_GetTicks() - t0 < 15000; SDL_Delay(5)) { TagLink_Pump(); TagLink_Status(st, sizeof st, code, sizeof code); if (code[0]) break; }
    if (!code[0]) { fprintf(stderr, "restart: no room code\n"); return 2; }
    SDL_snprintf(url, sizeof(url), "%s/v1/join", L.url); /* a member who never connects: after 4 s the owner wants the relay */
    SDL_snprintf(body, sizeof(body), "{\"v\":5,\"code\":\"%s\",\"name\":\"Away\",\"addrs\":[\"127.0.0.1:9\"]}", code);
    TagLink_Http(url, body, answer, sizeof(answer));
    for (t0 = SDL_GetTicks(); SDL_GetTicks() - t0 < 20000 && !SDL_GetAtomicInt(&sAllocating); SDL_Delay(5)) TagLink_Pump();
    if (!SDL_GetAtomicInt(&sAllocating)) { fprintf(stderr, "restart: the owner never asked the relay\n"); return 2; }
    SDL_Delay(300);
    oldSock = L.sock;
    TagLink_Host("B", 5); /* (TagLink_Stop of A, then B) */
    newSock = L.sock;
    for (t0 = SDL_GetTicks(); SDL_GetTicks() - t0 < (Uint64)stunMs + 3000; SDL_Delay(5)) {
        TagLink_Pump();
        if (SDL_GetTicks() - t0 < (Uint64)stunMs - 200) { sampled++; if (!SDL_GetAtomicInt(&L.busy)) clobbered++; }
    }
    { /* the old room's socket: closed by its thread as it ended (the old allocation ended 2.7 s after the change) */
        int oldOpen = fcntl(oldSock, F_GETFD) != -1 && oldSock != L.sock && oldSock != L.sock6;
        fprintf(stderr, "RESTART old socket %d, new socket %d, the allocation's socket %d; new room's busy cleared during its STUN %d of %d samples; the old allocation on the new room's socket %d times; the old socket still open %d\n",
                oldSock, newSock, gAllocSock, clobbered, sampled, gAllocOnOtherRoomSocket, oldOpen);
        gEnding = 1; TagLink_Stop(); SDL_Delay(500);
        return clobbered || gAllocOnOtherRoomSocket || oldSock == newSock || oldOpen ? 1 : 0;
    }
}

/* `harness cycle` (P3): twenty rooms opened and left in a row, 150 ms apart (each one's thread still in its STUN or
   its requests when the next begins); then no thread may hold a socket, nor any lock, and a last room still works. */
static int open_fds(void) { int n = 0, fd; for (fd = 0; fd < 1024; fd++) if (fcntl(fd, F_GETFD) != -1) n++; return n; }
static int cycle_test(void) {
    char st[160], code[16];
    int before = open_fds(), after, i;
    Uint64 t0;
    for (i = 0; i < 20; i++) { TagLink_Host("C", 5); TagLink_Pump(); SDL_Delay(150); }
    TagLink_Stop();
    SDL_Delay((getenv("STUN_MS") ? atoi(getenv("STUN_MS")) : 0) + 4000); /* (every thread ends: its STUN, its requests) */
    after = open_fds();
    TagLink_Host("Last", 5);
    for (t0 = SDL_GetTicks(), code[0] = 0; SDL_GetTicks() - t0 < 15000 && !code[0]; SDL_Delay(5)) { TagLink_Pump(); TagLink_Status(st, sizeof st, code, sizeof code); }
    fprintf(stderr, "CYCLE descriptors open before %d, after twenty rooms %d; the last room's code %s\n", before, after, code[0] ? code : "(none)");
    gEnding = 1; TagLink_Stop(); SDL_Delay(300);
    return after == before && code[0] ? 0 : 1;
}

/* `harness unit`: tag_link.c's own functions on a connection set up by hand, without the service. */
static int unitFailures;
#define UNIT_CHECK(c, ...) do { if (!(c)) { unitFailures++; fprintf(stderr, "FAIL unit: " __VA_ARGS__); fprintf(stderr, "\n"); } } while (0)
static TlAddr loopback(int port) { TlAddr a; memset(&a, 0, sizeof(a)); a.v4.sin_family = AF_INET; a.v4.sin_port = htons((unsigned short)port); a.v4.sin_addr.s_addr = htonl(0x7f000001); return a; }
static int unit_tests(void) {
    int peer = (int)socket(AF_INET, SOCK_DGRAM, 0), n;
    struct sockaddr_in any; socklen_t al = sizeof(any);
    uint8_t got[2048];
    Conn *c = &L.conn[0];
    TlAddr unreachable[3];
    memset(&any, 0, sizeof(any)); any.sin_family = AF_INET; any.sin_addr.s_addr = htonl(0x7f000001);
    bind(peer, (struct sockaddr *)&any, sizeof(any)); getsockname(peer, (struct sockaddr *)&any, &al);
    { struct timeval tv = {0, 300000}; setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); }
    L.sock = (int)socket(AF_INET, SOCK_DGRAM, 0); L.sock6 = -1; L.active = 1; L.tag = 123456;
    conn_reset(c); c->state = TK_LINK_OPEN; c->id = 77; c->member = 1; c->lastIn = SDL_GetTicks();
    c->to = loopback(ntohs(any.sin_port));
    unreachable[0] = loopback(9); unreachable[1] = loopback(10); unreachable[2] = loopback(11);
    /* P5: a DATA came (an acknowledgement is owed); a SYN then goes to the owner's other addresses, which do not answer
       (a guest on the relay tries the direct way every second). Those packets never reach the other side: the
       acknowledgement must still be owed, and the next flush must carry it to the connection's own way. */
    take_data(c, 0, (const uint8_t *)"hello", 5);
    c->ackOwed = 1;
    for (n = 0; n < 3; n++) send_packet(c, T_SYN, 1, NULL, 0, &unreachable[n]);
    UNIT_CHECK(c->ackOwed == 1, "P5: a SYN to another address took the owed acknowledgement away");
    while (recv(peer, got, sizeof(got), MSG_DONTWAIT) > 0) {}
    TagLink_FlushAcks();
    n = (int)recv(peer, got, sizeof(got), 0);
    UNIT_CHECK(n >= TL_HEAD && got[4] == T_ACK && get32(got + 13) == 1, "P5: the flush sent no acknowledgement of piece 0 to the connection's way (got %d bytes, type %d)", n, n > 4 ? got[4] : -1);
    UNIT_CHECK(c->ackOwed == 0, "P5: the acknowledgement is still owed after the flush");
    /* a packet to the connection's own way carries the acknowledgement: nothing more owed */
    take_data(c, 1, (const uint8_t *)"again", 5); c->ackOwed = 1;
    send_packet(c, T_SYN, 1, NULL, 0, &c->to);
    UNIT_CHECK(c->ackOwed == 0, "P5: a packet to the connection's own address left the acknowledgement owed");
    take_data(c, 2, (const uint8_t *)"third", 5); c->ackOwed = 1;
    send_packet(c, T_ACK, 0, NULL, 0, NULL);
    UNIT_CHECK(c->ackOwed == 0, "P5: a packet by the connection's way left the acknowledgement owed");
    { /* same_addr: what counts as the connection's own way */
        TlAddr a6, b6, r1 = relay_addr(1), r2 = relay_addr(2);
        memset(&a6, 0, sizeof(a6)); a6.v6.sin6_family = AF_INET6; a6.v6.sin6_port = htons(5000); inet_pton(AF_INET6, "2804:5c::1", &a6.v6.sin6_addr);
        b6 = a6;
        UNIT_CHECK(same_addr(&a6, &b6), "same_addr: one IPv6 address");
        b6.v6.sin6_port = htons(5001);
        UNIT_CHECK(!same_addr(&a6, &b6), "same_addr: IPv6, another port");
        UNIT_CHECK(!same_addr(&a6, &unreachable[0]), "same_addr: IPv6 against IPv4");
        UNIT_CHECK(!same_addr(&unreachable[0], &unreachable[1]), "same_addr: IPv4, another port");
        UNIT_CHECK(same_addr(&r1, &r1) && !same_addr(&r1, &r2), "same_addr: the room relay's players");
    }
    close(peer); close(L.sock); L.sock = -1; conn_reset(c); L.active = 0;
    fprintf(stderr, unitFailures ? "unit: %d failure(s)\n" : "unit: all passed\n", unitFailures);
    return unitFailures ? 1 : 0;
}

int main(int argc, char **argv) {
    SDL_Init(0);
    if (argc > 1 && !strcmp(argv[1], "unit")) return unit_tests();
    if (argc > 1 && !strcmp(argv[1], "restart")) return restart_test();
    if (argc > 1 && !strcmp(argv[1], "cycle")) return cycle_test();
    if (getenv("BG")) { /* BG=<from ms>:<for ms>: the app goes to the background then (gPortInBackground), as onPause says */
        static unsigned bg[2]; static SDL_Thread *t;
        sscanf(getenv("BG"), "%u:%u", &bg[0], &bg[1]);
        t = SDL_CreateThread(background_thread, "bg", bg);
        if (t != NULL) SDL_DetachThread(t);
    }
    gDrop = getenv("DROP") ? atoi(getenv("DROP")) : 0;
    if (getenv("CUT_MS")) gCutAt = SDL_GetTicks() + (Uint64)atoi(getenv("CUT_MS"));
    if (getenv("OPEN_MS")) gOpenAt = SDL_GetTicks() + (Uint64)atoi(getenv("OPEN_MS"));
    if (argc > 1 && !strcmp(argv[1], "host")) {
        static uint8_t buf[8192];
        int slots[8], n = 0, i, echoed = 0;
        Uint64 t0 = SDL_GetTicks();
        TagLink_Host("Host", 5);
        for (;;) {
            char st[160], code[16];
            int s;
            TagLink_Pump();
            TagLink_Status(st, sizeof(st), code, sizeof(code));
            static int shown; if (code[0] && !shown) { shown = 1; printf("CODE %s\n", code); fflush(stdout); }
            while ((s = TagLink_Accept()) >= 0) { slots[n++] = s; fprintf(stderr, "host: player in slot %d\n", s); }
            for (i = 0; i < n; i++) {
                int got = TagLink_Recv(slots[i], buf, sizeof(buf));
                if (got > 0) { int off = 0; while (off < got) { int w = TagLink_Send(slots[i], buf + off, got - off); if (w < 0) break; off += w; if (w == 0) { TagLink_Pump(); SDL_Delay(1); } } echoed += got; }
            }
            if (SDL_GetTicks() - t0 > 60000) { fprintf(stderr, "host: done, echoed %d\n", echoed); break; }
            SDL_Delay(2);
        }
        gEnding = 1; TagLink_Stop(); SDL_Delay(1500);
        return 0;
    } else {
        static uint8_t out[TOTAL], back[TOTAL];
        int slot = TagLink_Join(argv[2], "Guest", 5, 0), sent = 0, got = 0, i;
        if (getenv("PING")) { /* PING=<seconds>: a match's inputs instead: 16 bytes every 16 ms, echoed; the round trips */
            int secs = atoi(getenv("PING")), n = 0, over100 = 0, over250 = 0; double sum = 0; Uint64 worst = 0, next = 0, start = 0, t0 = SDL_GetTicks();
            uint8_t msg[16], in[16]; int have = 0;
            while (SDL_GetTicks() - t0 < (Uint64)(secs + 20) * 1000) {
                Uint64 now;
                TagLink_Pump();
                now = SDL_GetTicks();
                if (TagLink_State(slot) == TK_LINK_CLOSED) { fprintf(stderr, "join: closed\n"); return 1; }
                if (TagLink_State(slot) == TK_LINK_OPEN) {
                    if (!start) start = now + 2000; /* (the first 2 s: the ways settle) */
                    if (getenv("TRACE")) { static Uint64 lt; if (now - lt >= 500) { Conn *cc = &L.conn[slot]; lt = now;
                        fprintf(stderr, "  t %u: srtt %d rto %d var %d unacked %u to %s\n", (unsigned)(now - t0), cc->srtt, cc->rto, cc->rttvar, cc->sndNext - cc->sndUna, is_relay(&cc->to) ? "relay" : "direct"); } }
                    if (now >= next && now < start + (Uint64)secs * 1000) { memset(msg, 0, 16); memcpy(msg, &now, 8); TagLink_Send(slot, msg, 16); next = now + 16; }
                    for (;;) { int r = TagLink_Recv(slot, in + have, 16 - have); if (r <= 0) break; have += r;
                        if (have == 16) { Uint64 t; memcpy(&t, in, 8); have = 0;
                            if (t >= start) { Uint64 d = now - t; n++; sum += (double)d; if (d > worst) worst = d; if (d > 100) over100++; if (getenv("TRACE") && (d > 150 || n % 60 == 0)) fprintf(stderr, "  at %u ms: echo %u ms\n", (unsigned)(now - start), (unsigned)d); if (d > 250) over250++; } } }
                    if (now > start + (Uint64)secs * 1000 + 3000) break;
                }
                SDL_Delay(1);
            }
            { Conn *cc = &L.conn[slot]; fprintf(stderr, "join: link srtt %d rto %d, unacked %u, inbox %d\n", cc->srtt, cc->rto, cc->sndNext - cc->sndUna, cc->inN); }
            fprintf(stderr, "join: PING %d echoes, average %.1f ms, worst %u ms, over 100 ms %d, over 250 ms %d\n", n, n ? sum / n : 0, (unsigned)worst, over100, over250);
            gEnding = 1; TagLink_Stop(); SDL_Delay(500);
            return 0;
        }
        Uint64 t0 = SDL_GetTicks(), opened = 0;
        for (i = 0; i < TOTAL; i++) out[i] = (uint8_t)(i * 7 + i / 251);
        while (got < TOTAL && SDL_GetTicks() - t0 < 45000) {
            int n;
            TagLink_Pump();
            if (TagLink_State(slot) == TK_LINK_OPEN && !opened) { opened = SDL_GetTicks(); fprintf(stderr, "join: open after %d ms\n", (int)(opened - t0)); }
            if (TagLink_State(slot) == TK_LINK_CLOSED) { char st[160], c[16]; TagLink_Status(st, sizeof st, c, sizeof c); fprintf(stderr, "join: closed: %s\n", st); return 1; }
            /* PACE=<bytes per ms>: a stream spread over time, as a match's inputs are (to cut a way in its middle) */
            if (sent < TOTAL && (!getenv("PACE") || !opened || sent < (int)(SDL_GetTicks() - opened) * atoi(getenv("PACE")))) { n = TagLink_Send(slot, out + sent, (TOTAL - sent) > 4000 ? 4000 : TOTAL - sent); if (n > 0) sent += n; }
            n = TagLink_Recv(slot, back + got, TOTAL - got); if (n > 0) got += n;
            SDL_Delay(1);
        }
        fprintf(stderr, "join: sent %d, got back %d in %d ms, %s\n", sent, got, (int)(SDL_GetTicks() - opened), got == TOTAL && !memcmp(out, back, TOTAL) ? "ALL BYTES MATCH" : "MISMATCH");
        gEnding = 1; TagLink_Stop(); SDL_Delay(1500);
        return got == TOTAL && !memcmp(out, back, TOTAL) ? 0 : 1;
    }
}
