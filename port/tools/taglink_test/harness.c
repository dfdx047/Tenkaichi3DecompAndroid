/* tag_link.c on Linux against the local service (server.mjs): one host, one or more joiners, a stream echoed back,
   with packets dropped on purpose (DROP percent). */
#define _GNU_SOURCE
#include <SDL3/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static int gDrop;
static ssize_t drop_sendto(int s, const void *b, size_t n, int f, const struct sockaddr *a, socklen_t l) {
    if (gDrop && (rand() % 100) < gDrop) return (ssize_t)n;
    return sendto(s, b, n, f, a, l);
}
/* stand-ins for net_match.c, net_relay.c, plat_file.c */
int TagLink_Stun(int sock, char *out, int size) { (void)sock; (void)out; (void)size; return 0; }
int TagLink_Lan(int port, char *out, int size) { snprintf(out, size, "127.0.0.1:%d", port); return 1; }
int TagLink_Http(const char *url, const char *json, char *out, int size) {
    char cmd[2048], file[64]; FILE *f; size_t n;
    const char *path = strstr(url, "/v1/");
    snprintf(file, sizeof(file), "/tmp/claude-1000/tl-%d.json", (int)SDL_GetCurrentThreadID() & 0xffff);
    f = fopen(file, "w"); fputs(json, f); fclose(f);
    snprintf(cmd, sizeof(cmd), "curl -s -X POST -H 'content-type: application/json' --data-binary @%s http://127.0.0.1:8787%s", file, path);
    f = popen(cmd, "r"); n = fread(out, 1, size - 1, f); pclose(f); out[n] = 0;
    return n > 0;
}
const char *Port_FileRoot(void) { return "/tmp/claude-1000"; }
int Relay_Allocate(int a, const char *b, const char *c, const char *d, char *e, int f) { return 0; }
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

#define TOTAL (300 * 1024)
int main(int argc, char **argv) {
    SDL_Init(0);
    gDrop = getenv("DROP") ? atoi(getenv("DROP")) : 0;
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
        TagLink_Stop(); SDL_Delay(1500);
        return 0;
    } else {
        static uint8_t out[TOTAL], back[TOTAL];
        int slot = TagLink_Join(argv[2], "Guest", 5, 0), sent = 0, got = 0, i;
        Uint64 t0 = SDL_GetTicks(), opened = 0;
        for (i = 0; i < TOTAL; i++) out[i] = (uint8_t)(i * 7 + i / 251);
        while (got < TOTAL && SDL_GetTicks() - t0 < 45000) {
            int n;
            TagLink_Pump();
            if (TagLink_State(slot) == TK_LINK_OPEN && !opened) { opened = SDL_GetTicks(); fprintf(stderr, "join: open after %d ms\n", (int)(opened - t0)); }
            if (TagLink_State(slot) == TK_LINK_CLOSED) { char st[160], c[16]; TagLink_Status(st, sizeof st, c, sizeof c); fprintf(stderr, "join: closed: %s\n", st); return 1; }
            if (sent < TOTAL) { n = TagLink_Send(slot, out + sent, (TOTAL - sent) > 4000 ? 4000 : TOTAL - sent); if (n > 0) sent += n; }
            n = TagLink_Recv(slot, back + got, TOTAL - got); if (n > 0) got += n;
            SDL_Delay(1);
        }
        fprintf(stderr, "join: sent %d, got back %d in %d ms, %s\n", sent, got, (int)(SDL_GetTicks() - opened), got == TOTAL && !memcmp(out, back, TOTAL) ? "ALL BYTES MATCH" : "MISMATCH");
        TagLink_Stop(); SDL_Delay(1500);
        return got == TOTAL && !memcmp(out, back, TOTAL) ? 0 : 1;
    }
}
