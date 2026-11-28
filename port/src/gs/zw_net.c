/* Z Warriors Online's line to its server (port/zw-worker): one signed POST at a time, on a thread of its own, so the
   screens never wait. Android only (the game's HttpURLConnection, through net_match.c). The key of the release comes
   from ZW_BUILD_KEY in the build's environment (port/tools/buildkey.py), as net_sign.c's does; never in the sources. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../port_release.h"
#include "net_sign.h"

#define ZW_URL "https://zwarriors-online.zw-worker.workers.dev"
extern int Match_HttpPost(const char *url, const char *json, const char *const *headers, char *out, size_t size);

static char sPath[48], sBody[20000], sAnswer[40000];
static SDL_AtomicInt sState; /* 0 free, 1 asking, 2 answered, 3 no answer */
static long long sClockAhead;
static char sPublic[64]; /* this phone as the internet sees it, "ip:port" */
static char sLocal[64];  /* and on its own network */
extern int TagLink_Stun(int sock, char *out, int size); /* net_match.c */
extern int TagLink_Lan(int port, char *out, int size);

const char *ZwHttp_Public(void) {
    return sPublic;
}

const char *ZwHttp_Local(void) {
    return sLocal;
}

/* The "@" of the body's address list: this phone's own network and what STUN sees from the fight's port. */
static void zw_addresses(void) {
    static char out[sizeof(sBody)];
    char *at = SDL_strstr(sBody, "[@]"), lan[64] = "", list[150];
    struct sockaddr_in me;
    int sock, yes = 1;

    if (at == NULL) {
        return;
    }
    sPublic[0] = '\0';
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        SDL_memset(&me, 0, sizeof(me));
        me.sin_family = AF_INET;
        me.sin_port = htons(7777);
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (bind(sock, (struct sockaddr *)&me, sizeof(me)) == 0 && !TagLink_Stun(sock, sPublic, sizeof(sPublic))) {
            sPublic[0] = '\0';
        }
        close(sock);
    }
    if (!TagLink_Lan(7777, lan, sizeof(lan))) {
        lan[0] = '\0';
    }
    SDL_strlcpy(sLocal, lan, sizeof(sLocal));
    SDL_snprintf(list, sizeof(list), "[\"%s\"%s%s%s]", lan[0] ? lan : "0.0.0.0:7777", sPublic[0] ? ",\"" : "", sPublic, sPublic[0] ? "\"" : "");
    *at = '\0';
    SDL_snprintf(out, sizeof(out), "%s%s%s", sBody, list, at + 3);
    SDL_strlcpy(sBody, out, sizeof(sBody));
}

static int zw_key(uint8_t *out) {
#ifdef BT3_BUILD_KEY_DATA
    static const uint8_t data[] = {BT3_BUILD_KEY_DATA};
    static volatile uint32_t seed = BT3_BUILD_KEY_SEED;
    uint32_t x = seed;
    int i, n = (int)sizeof(data) > 128 ? 128 : (int)sizeof(data);

    for (i = 0; i < n; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        out[i] = (uint8_t)(data[i] ^ (x >> 11));
    }
    return n;
#else
    (void)out;
    return 0;
#endif
}

static int SDLCALL zw_thread(void *arg) {
    static char url[128], head[3][96], canon[512];
    const char *headers[4] = {NULL, NULL, NULL, NULL};
    uint8_t key[128], mac[32];
    long long when;
    int n = zw_key(key), len, i;
    const char *now;

    (void)arg;
    zw_addresses();
    when = (long long)time(NULL) + sClockAhead;
    SDL_snprintf(url, sizeof(url), "%s%s", ZW_URL, sPath);
    len = Sign_Canonical("POST", sPath, PORT_RELEASE, when, sBody, canon, sizeof(canon));
    if (n > 0 && len > 0) {
        Sign_Hmac256(key, (size_t)n, (const uint8_t *)canon, (size_t)len, mac);
        SDL_snprintf(head[0], sizeof(head[0]), "X-BT3-Release: %d", PORT_RELEASE);
        SDL_snprintf(head[1], sizeof(head[1]), "X-BT3-Time: %lld", when);
        SDL_strlcpy(head[2], "X-BT3-Sign: ", sizeof(head[2]));
        for (i = 0; i < 32; i++) {
            SDL_snprintf(head[2] + 12 + i * 2, 3, "%02x", mac[i]);
        }
        headers[0] = head[0], headers[1] = head[1], headers[2] = head[2];
    }
    SDL_memset(key, 0, sizeof(key));
    sAnswer[0] = '\0';
    i = Match_HttpPost(url, sBody, headers[0] != NULL ? headers : NULL, sAnswer, sizeof(sAnswer));
    if (i && (now = SDL_strstr(sAnswer, "\"now\":")) != NULL) {
        sClockAhead = SDL_strtoll(now + 6, NULL, 10) - (long long)time(NULL);
    }
    SDL_SetAtomicInt(&sState, i && sAnswer[0] == '{' ? 2 : 3);
    return 0;
}

/* 1 begun, 0 the line is taken. */
int ZwHttp_Start(const char *what, const char *json) {
    SDL_Thread *t;

    if (SDL_GetAtomicInt(&sState) != 0) {
        return 0;
    }
    SDL_snprintf(sPath, sizeof(sPath), "/v1/%s", what);
    SDL_strlcpy(sBody, json, sizeof(sBody));
    SDL_SetAtomicInt(&sState, 1);
    t = SDL_CreateThread(zw_thread, "zw-online", NULL);
    if (t == NULL) {
        SDL_SetAtomicInt(&sState, 3);
    } else {
        SDL_DetachThread(t);
    }
    return 1;
}

/* 1 the answer is in `out`, -1 there was none, 0 nothing yet. */
int ZwHttp_Take(char *out, int size) {
    int s = SDL_GetAtomicInt(&sState);

    if (s < 2) {
        return 0;
    }
    if (s == 2) {
        SDL_strlcpy(out, sAnswer, (size_t)size);
    }
    SDL_SetAtomicInt(&sState, 0);
    return s == 2 ? 1 : -1;
}

/* The fight asked from a Z Warriors room: both warriors' codes (side 1 the host's), the stage and the time limit.
   Kept here, outside the game's memory, which a session puts back to its first moment. */
static char sFightP1[17000], sFightP2[17000];
static int sFightState, sFightStage, sFightTime; /* state: 0 none, 1 to be set up, 2 set up */

void ZwFight_Put(const char *p1, const char *p2, int stage, int time) {
    SDL_strlcpy(sFightP1, p1, sizeof(sFightP1));
    SDL_strlcpy(sFightP2, p2, sizeof(sFightP2));
    sFightStage = stage;
    sFightTime = time;
    sFightState = 1;
}

/* The state as it was; from 1 the codes are given and it becomes 2. */
int ZwFight_Take(char *p1, char *p2, int size, int *stage, int *time) {
    int was = sFightState;

    if (was == 1) {
        SDL_strlcpy(p1, sFightP1, (size_t)size);
        SDL_strlcpy(p2, sFightP2, (size_t)size);
        *stage = sFightStage;
        *time = sFightTime;
        sFightState = 2;
    }
    return was;
}

void ZwFight_Clear(void) {
    sFightState = 0;
}
