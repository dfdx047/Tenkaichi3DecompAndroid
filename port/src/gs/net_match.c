/*
 * Matchmaking with room codes: the client side of port/matchmaking/worker.js.
 *
 * The lobby (net.c) opens the UDP socket the match will use and hands it over. A thread then:
 *   1. asks a STUN server, from that socket, which address and port the rest of the internet sees it under;
 *   2. tells the Worker (host: "make a room", and then asks every two seconds whether someone has joined; the one who
 *      joins: "join room <code>");
 *   3. ends with the other player's addresses, which the lobby sends its greetings to (both sides at once: that is
 *      what lets the two routers pass each other's packets).
 * The Worker is spoken to over HTTPS with what the system has: WinHTTP on Windows, the curl library on Linux, and
 * failing those the `curl` program (BT3_MATCH_CURL=1 asks for the program). No game data goes that way.
 *
 * BT3_MATCH_URL=<address> names another Worker (one's own; see port/matchmaking/README.md).
 *
 * A program built with a build key (net_sign.c) signs every request to the Worker: three headers, X-BT3-Release,
 * X-BT3-Time and X-BT3-Sign. One built without sends none of them. The request to GitHub (the update check,
 * gs/net_update.c) is never signed.
 */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define MATCH_URL_DEFAULT "https://odd-surf-0af7.bt3decomp.workers.dev"
#include "net_match.h"
#include "../port_release.h"
#include "net_relay.h"
#include "net_sign.h"
#include "net_dragon.h"

static struct {
    SDL_Mutex *lock;
    SDL_Thread *thread;
    SDL_AtomicInt cancel, gen; /* gen: which search is the current one (a thread of an earlier one writes nothing) */
    SDL_AtomicInt sockBusy;    /* the thread is talking to the relay on the match's socket: the lobby keeps off it */
    MatchStatus st;
    int role, sock, localPort, version;
    char code[8], name[20]; /* (role 3, a quick match: `code` is the queue's name until the service gives a room) */
    char queue[8];
    char url[256]; /* o servidor desta busca */
    char area[8];           /* a quick match: what the player chose to look for ("auto", "any", a region) */
    int watch;              /* a host that lets spectators in: they are asked for with every poll, and once the other
                               player has come the room is kept for as long as the match is on (Match_Keep) */
    char watchAddr[MATCH_WATCH_ADDRS][64]; /* the spectators' addresses, all together ("seen=" ones too) */
    int watchCount, watchGen;
    SDL_AtomicInt relayAsk, relayGen;      /* a lobby's owner: a relay address is wanted; how many it has got */
} sM;

/* The spectators of a poll's answer: "watchers":[["a:p","seen=a"],["b:q"]]. Kept when they changed. */
static void json_watchers(const char *json) {
    const char *p = SDL_strstr(json, "\"watchers\":[");
    char addr[MATCH_WATCH_ADDRS][64];
    int n = 0;

    if (p == NULL) {
        return;
    }
    for (p += 12; *p != '\0' && *p != ']' && n < MATCH_WATCH_ADDRS; p++) {
        if (*p == '[') { /* one spectator's list */
            for (p++; *p == '"' && n < MATCH_WATCH_ADDRS;) {
                const char *e = SDL_strchr(p + 1, '"');
                if (e == NULL || (size_t)(e - p - 1) >= sizeof(addr[0])) {
                    return;
                }
                SDL_memcpy(addr[n], p + 1, (size_t)(e - p - 1));
                addr[n][e - p - 1] = '\0';
                n++;
                p = e + 1;
                if (*p == ',') {
                    p++;
                }
            }
        }
    }
    SDL_LockMutex(sM.lock);
    if (n != sM.watchCount || SDL_memcmp(addr, sM.watchAddr, sizeof(addr[0]) * (size_t)n) != 0) {
        SDL_memcpy(sM.watchAddr, addr, sizeof(addr[0]) * (size_t)n);
        sM.watchCount = n;
        sM.watchGen++;
    }
    SDL_UnlockMutex(sM.lock);
}

/* Quick match: what this game calls itself to the service (so that a room it left behind is not given back to
   it): sixteen letters made up once per run. */
static const char *match_who(void) {
    static char who[20];
    if (who[0] == '\0') {
        int i;
        SDL_srand(SDL_GetTicksNS() ^ (Uint64)(uintptr_t)&who);
        for (i = 0; i < 16; i++) {
            who[i] = (char)('a' + SDL_rand(26));
        }
        who[16] = '\0';
    }
    return who;
}

static SDL_TLSID sMine; /* the thread's own search number */
#define STOPPED() (SDL_GetAtomicInt(&sM.cancel) || (int)(intptr_t)SDL_GetTLS(&sMine) != SDL_GetAtomicInt(&sM.gen))

/* A one-line file in the game folder naming another service ("https://...", without the trailing slash): "" if none. */
static void url_file(const char *file, char *out, size_t size) {
    out[0] = '\0';
#ifdef __ANDROID__
    {
        extern const char *Port_FileRoot(void); /* plat_file.c */
        char path[600];
        FILE *f;
        SDL_snprintf(path, sizeof(path), "%s/%s", Port_FileRoot(), file);
        if ((f = fopen(path, "r")) != NULL) {
            if (fgets(out, (int)size, f) != NULL) {
                out[strcspn(out, " \r\n")] = '\0';
                while (out[0] != '\0' && out[strlen(out) - 1] == '/') {
                    out[strlen(out) - 1] = '\0';
                }
                if (SDL_strncmp(out, "https://", 8) != 0) {
                    out[0] = '\0';
                }
            }
            fclose(f);
        }
    }
#else
    (void)file;
    (void)size;
#endif
}

/* Dragon Net's service (net_dragon.h): dragonnet_url.txt, else DBT-NX's. */
const char *DrNet_Url(void) {
    static char fromFile[256];
    static int read;
    if (!read) {
        read = 1;
        url_file("dragonnet_url.txt", fromFile, sizeof(fromFile));
    }
    return fromFile[0] != '\0' ? fromFile : DRNET_URL_DEFAULT;
}

#ifndef DRNET_URL_B
#define DRNET_URL_B "https://dbt-nx-matchmaking.alexyt2021es.workers.dev"
#endif
#ifndef DRNET_URL_RANKED
#define DRNET_URL_RANKED "https://bt-ranked.arielnndrd.workers.dev"
#endif

const char *DrNet_UrlB(void) {
    static char f[256];
    static int read;
    if (!read) {
        read = 1;
        url_file("dragonnet_url_b.txt", f, sizeof(f));
        if (f[0] == '\0') SDL_strlcpy(f, DRNET_URL_B, sizeof(f));
    }
    return f;
}

const char *DrNet_RankedUrl(void) {
    static char f[256];
    static int read;
    if (!read) {
        read = 1;
        url_file("dragonnet_ranked_url.txt", f, sizeof(f));
        if (f[0] == '\0') SDL_strlcpy(f, DRNET_URL_RANKED, sizeof(f));
    }
    return f;
}

#define DRNET_FAILS 3
static SDL_AtomicInt sUseB, sUseBDay, sFails;

static int utc_day(void) {
    return (int)(time(NULL) / 86400);
}

const char *DrNet_ActiveUrl(void) {
    if (SDL_GetAtomicInt(&sUseB)) {
        if (SDL_GetAtomicInt(&sUseBDay) != utc_day() || DrNet_UrlB()[0] == '\0') {
            SDL_SetAtomicInt(&sUseB, 0); /* novo dia UTC: volta ao A */
            SDL_SetAtomicInt(&sFails, 0);
        } else {
            return DrNet_UrlB();
        }
    }
    return DrNet_Url();
}

void DrNet_NoteResult(const char *base, int ok) {
    if (base == NULL || SDL_strcmp(base, DrNet_Url()) != 0) {
        return; /* so o A conta */
    }
    if (ok) {
        SDL_SetAtomicInt(&sFails, 0);
        return;
    }
    if (DrNet_UrlB()[0] == '\0' || SDL_GetAtomicInt(&sUseB)) {
        return;
    }
    if (SDL_AddAtomicInt(&sFails, 1) + 1 >= DRNET_FAILS) {
        SDL_SetAtomicInt(&sUseBDay, utc_day());
        SDL_SetAtomicInt(&sUseB, 1);
        SDL_SetAtomicInt(&sFails, 0);
        fprintf(stderr, "dragonnet: server A failed, using B until 00:00 UTC\n");
    }
}

/* The service rooms and quick matches go through: Dragon Net's (mode 0), or the PC port's (mode 1; Android:
   <game folder>/match_url.txt, one line "https://...", names another Worker of that kind). BT3_MATCH_URL wins. */
static const char *match_url(void) {
    const char *e = SDL_getenv("BT3_MATCH_URL");
    static char fromFile[256];
    static int read;
    if (e != NULL && e[0] != '\0') {
        return e;
    }
    if (DrNet_Mode() == 0) {
        return DrNet_ActiveUrl();
    }
    if (!read) {
        read = 1;
        url_file("match_url.txt", fromFile, sizeof(fromFile));
        fprintf(stderr, "net: matchmaking service against PC %s\n", fromFile[0] != '\0' ? fromFile : MATCH_URL_DEFAULT);
    }
    return fromFile[0] != '\0' ? fromFile : MATCH_URL_DEFAULT;
}

static void set_state(int state, const char *error) {
    if ((int)(intptr_t)SDL_GetTLS(&sMine) != SDL_GetAtomicInt(&sM.gen)) {
        return;
    }
    if (error != NULL && state < 0 && Match_ServiceLimited()) {
        error = MATCH_LIMIT_TEXT; /* (whatever went wrong after it: the limit is why) */
    }
    if (error != NULL && state < 0) {
        fprintf(stderr, "net: %s\n", error); /* (in the log, for a report about online play) */
    }
    SDL_LockMutex(sM.lock);
    sM.st.state = state;
    if (error != NULL) {
        SDL_strlcpy(sM.st.error, error, sizeof(sM.st.error));
    }
    SDL_UnlockMutex(sM.lock);
}

/* ---- the Worker, through curl ---- */
/* (http_post_native: `json` NULL makes it a GET of `url`. `headers`: lines
   "Name: value" to send as well, ended by a NULL, or NULL for none.) */

#ifdef _WIN32
/* Windows: its own HTTPS (WinHTTP), so nothing has to be installed and no other program is started. */
#include <winhttp.h>

static int http_post_native(const char *url, const char *json, const char *const *headers, char *out, size_t size) {
    wchar_t wurl[512], host[256], path[256], whead[512];
    char head[512];
    URL_COMPONENTS uc;
    HINTERNET ses = NULL, con = NULL, req = NULL;
    DWORD got = 0, used = 0, status = 0, len = sizeof(status);
    int ok = 0;

    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 512) == 0) {
        return 0;
    }
    SDL_strlcpy(head, "Content-Type: application/json\r\n", sizeof(head));
    for (; headers != NULL && *headers != NULL; headers++) {
        SDL_strlcat(head, *headers, sizeof(head));
        SDL_strlcat(head, "\r\n", sizeof(head));
    }
    if (MultiByteToWideChar(CP_UTF8, 0, head, -1, whead, 512) == 0) {
        return 0;
    }
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 256;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        return 0;
    }
    ses = WinHttpOpen(L"Tenkaichi3Decomp", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (ses != NULL) {
        WinHttpSetTimeouts(ses, 8000, 8000, 8000, 12000);
        con = WinHttpConnect(ses, host, uc.nPort, 0);
    }
    if (con != NULL) {
        req = WinHttpOpenRequest(con, json != NULL ? L"POST" : L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    }
    if (req != NULL &&
        (json != NULL ? WinHttpSendRequest(req, whead, (DWORD)-1, (LPVOID)json, (DWORD)strlen(json), (DWORD)strlen(json), 0)
                      : WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) &&
        WinHttpReceiveResponse(req, NULL)) {
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
        while (used + 1 < size && WinHttpReadData(req, out + used, (DWORD)(size - 1 - used), &got) && got != 0) {
            used += got;
        }
        out[used] = '\0';
        ok = used > 0;
    }
    if (req != NULL) { WinHttpCloseHandle(req); }
    if (con != NULL) { WinHttpCloseHandle(con); }
    if (ses != NULL) { WinHttpCloseHandle(ses); }
    return ok;
}
#elif defined(__ANDROID__)
/* Android: no curl. The app's Java side does the request (GameActivity.netPost, HttpsURLConnection), called here
   through JNI on this thread (SDL attaches it). */
#include <jni.h>

static int http_post_native(const char *url, const char *json, const char *const *headers, char *out, size_t size) {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject act;
    jclass cls;
    jmethodID m;
    jstring ju, jb, jh, jr;
    char head[512];
    int ok = 0, i;

    if (env == NULL || (act = (jobject)SDL_GetAndroidActivity()) == NULL) {
        return 0;
    }
    head[0] = '\0'; /* the extra header lines, one per line ("Name: value") */
    for (i = 0; headers != NULL && headers[i] != NULL; i++) {
        SDL_strlcat(head, headers[i], sizeof(head));
        SDL_strlcat(head, "\n", sizeof(head));
    }
    cls = (*env)->GetObjectClass(env, act);
    m = cls != NULL ? (*env)->GetMethodID(env, cls, "netRequest", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;") : NULL;
    if (m == NULL) {
        (*env)->ExceptionClear(env);
    } else {
        ju = (*env)->NewStringUTF(env, url);
        jb = json != NULL ? (*env)->NewStringUTF(env, json) : NULL; /* NULL: a GET */
        jh = (*env)->NewStringUTF(env, head);
        jr = (jstring)(*env)->CallObjectMethod(env, act, m, ju, jb, jh);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            jr = NULL;
        }
        if (jr != NULL) {
            const char *r = (*env)->GetStringUTFChars(env, jr, NULL);
            if (r != NULL) {
                SDL_strlcpy(out, r, size);
                ok = out[0] != '\0';
                (*env)->ReleaseStringUTFChars(env, jr, r);
            }
            (*env)->DeleteLocalRef(env, jr);
        }
        (*env)->DeleteLocalRef(env, ju);
        if (jb != NULL) {
            (*env)->DeleteLocalRef(env, jb);
        }
        (*env)->DeleteLocalRef(env, jh);
    }
    if (cls != NULL) {
        (*env)->DeleteLocalRef(env, cls);
    }
    (*env)->DeleteLocalRef(env, act);
    return ok;
}
#else
/* Linux: the system's curl library, if it is there (it nearly always is), loaded when first needed. */
static int http_post_native(const char *url, const char *json, const char *const *headers, char *out, size_t size) {
    typedef void *(*init_fn)(void);
    typedef void *(*append_fn)(void *, const char *);
    typedef void (*freeall_fn)(void *);
    typedef int (*setopt_fn)(void *, int, ...);
    typedef int (*perform_fn)(void *);
    typedef void (*cleanup_fn)(void *);
    static SDL_SharedObject *lib;
    static int tried;
    static init_fn init;
    static setopt_fn setopt;
    static perform_fn perform;
    static cleanup_fn cleanup;
    static append_fn append;
    static freeall_fn freeall;
    void *h, *list = NULL;
    SDL_IOStream *mem;
    FILE *fp;
    int rc;
    long n;

    if (!tried) {
        static const char *const names[] = {"libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.so"};
        size_t i;
        tried = 1;
        for (i = 0; lib == NULL && i < sizeof(names) / sizeof(names[0]); i++) {
            lib = SDL_LoadObject(names[i]);
        }
        if (lib != NULL) {
            init = (init_fn)SDL_LoadFunction(lib, "curl_easy_init");
            setopt = (setopt_fn)SDL_LoadFunction(lib, "curl_easy_setopt");
            perform = (perform_fn)SDL_LoadFunction(lib, "curl_easy_perform");
            cleanup = (cleanup_fn)SDL_LoadFunction(lib, "curl_easy_cleanup");
            append = (append_fn)SDL_LoadFunction(lib, "curl_slist_append");
            freeall = (freeall_fn)SDL_LoadFunction(lib, "curl_slist_free_all");
        }
    }
    (void)mem;
    if (init == NULL || setopt == NULL || perform == NULL || cleanup == NULL || (headers != NULL && (append == NULL || freeall == NULL)) || (h = init()) == NULL) {
        return 0;
    }
    fp = tmpfile(); /* (the library writes the answer with fwrite by default: into a file that is never on disk by name) */
    if (fp == NULL) {
        cleanup(h);
        return 0;
    }
    setopt(h, 10002 /* CURLOPT_URL */, url);
    if (json != NULL) {
        setopt(h, 10015 /* CURLOPT_POSTFIELDS */, json);
    } else { /* a GET (the update check): GitHub answers nothing without a name, and sends on to another address */
        setopt(h, 10018 /* CURLOPT_USERAGENT */, "Tenkaichi3Decomp");
        setopt(h, 52 /* CURLOPT_FOLLOWLOCATION */, 1L);
    }
    for (; headers != NULL && *headers != NULL; headers++) {
        list = append(list, *headers);
    }
    if (list != NULL) {
        setopt(h, 10023 /* CURLOPT_HTTPHEADER */, list);
    }
    setopt(h, 10001 /* CURLOPT_WRITEDATA */, fp);
    setopt(h, 13 /* CURLOPT_TIMEOUT */, 12L);
    setopt(h, 99 /* CURLOPT_NOSIGNAL */, 1L);
    rc = perform(h);
    cleanup(h);
    if (list != NULL) {
        freeall(list);
    }
    n = 0;
    if (rc == 0) {
        rewind(fp);
        n = (long)fread(out, 1, size - 1, fp);
        out[n > 0 ? n : 0] = '\0';
    }
    fclose(fp);
    return rc == 0 && n > 0;
}
#endif

/* The build key (net_sign.c). The service checks the time a request is signed for against its own clock (five
   minutes either way), and a player's clock can be wrong by more: so the game keeps how far the service's clock is
   ahead of this computer's. It learns that from the answer to "hello" (asked when the online screen opens), and
   from a request refused for its time, which is then sent once more. */
static SDL_AtomicInt sClockAhead;

static void clock_learn(const char *answer) {
    const char *p = SDL_strstr(answer, "\"now\":");
    long long now = p != NULL ? SDL_strtoll(p + 6, NULL, 10) : 0;
    if (now > 1000000000LL) {
        SDL_SetAtomicInt(&sClockAhead, (int)(now - (long long)time(NULL)));
    }
}

/* POSTs `json` to <url>/v1/<what>; the answer's text in out. 0 = no answer. The system's own HTTPS first (above); if
   that is not to be had, the `curl` program. */
static int http_post_once(const char *base, const char *what, const char *json, char *out, size_t size) {
    char url[256], path[64], sig[65], head[3][96];
    const char *args[20], *headers[4] = {NULL, NULL, NULL, NULL};
    SDL_Process *p;
    size_t got = 0;
    int code = 0, n = 0, i;
    void *data;
    long long when = (long long)time(NULL) + SDL_GetAtomicInt(&sClockAhead);

    SDL_snprintf(url, sizeof(url), "%s/v1/%s", base, what);
    SDL_snprintf(path, sizeof(path), "/v1/%s", what);
    if (Sign_Request("POST", path, PORT_RELEASE, when, json, sig)) { /* (a program without a key: no headers) */
        SDL_snprintf(head[0], sizeof(head[0]), "X-BT3-Release: %d", PORT_RELEASE);
        SDL_snprintf(head[1], sizeof(head[1]), "X-BT3-Time: %lld", when);
        SDL_snprintf(head[2], sizeof(head[2]), "X-BT3-Sign: %s", sig);
        for (i = 0; i < 3; i++) {
            headers[i] = head[i];
        }
    }
    if (SDL_getenv("BT3_MATCH_CURL") == NULL && http_post_native(url, json, headers[0] != NULL ? headers : NULL, out, size)) {
        return 1;
    }
#ifdef __ANDROID__
    (void)args; (void)p; (void)got; (void)code; (void)n; (void)data;
    return 0; /* (no curl program to fall back on) */
#endif
#ifdef _WIN32
    args[n++] = "curl.exe";
#else
    args[n++] = "curl";
#endif
    args[n++] = "-s";
    args[n++] = "-m";
    args[n++] = "12";
    args[n++] = "-X";
    args[n++] = "POST";
    args[n++] = url;
    args[n++] = "-d";
    args[n++] = json;
    for (i = 0; headers[i] != NULL; i++) {
        args[n++] = "-H";
        args[n++] = headers[i];
    }
    args[n] = NULL;
    p = SDL_CreateProcess(args, true);
    if (p == NULL) {
        return 0;
    }
    data = SDL_ReadProcess(p, &got, &code);
    SDL_DestroyProcess(p);
    if (data == NULL) {
        return 0;
    }
    SDL_strlcpy(out, (const char *)data, size);
    SDL_free(data);
    return got > 0;
}

/* The services' limits (HTTP 429: Cloudflare's daily requests of a free Worker, its error 1027, or a Worker's own
   "busy"): after one, nothing more is asked of that service for a while, 30 s doubling to 10 minutes, and the
   windows say why ({"error":"limit"} or {"error":"busy"} in place of an answer). Everything that asks a service
   goes through here, so polling that keeps going (a room, the Ranked queue) stops asking with it; another service
   (Dragon Net's server B, Ranked's) goes on. A good answer ends the pause. Also counted for the log: the requests
   of each minute with any. */
typedef struct { SDL_AtomicInt hash, until, wait, daily; } Limit; /* until: SDL_GetTicks() / 1000; wait: the last pause, s */
static Limit sLimits[4];
static SDL_AtomicInt sLimitNext, sAsked, sAskedAt;

static int url_hash(const char *base) {
    unsigned h = 2166136261u;
    for (; *base != '\0'; base++) {
        h = (h ^ (unsigned char)*base) * 16777619u;
    }
    return (int)(h | 1u); /* (never 0: an unused slot) */
}

static Limit *limit_of(const char *base, int make) {
    int h = url_hash(base), i;
    for (i = 0; i < 4; i++) {
        if (SDL_GetAtomicInt(&sLimits[i].hash) == h) {
            return &sLimits[i];
        }
    }
    if (!make) {
        return NULL;
    }
    i = SDL_AddAtomicInt(&sLimitNext, 1) & 3;
    SDL_SetAtomicInt(&sLimits[i].wait, 0);
    SDL_SetAtomicInt(&sLimits[i].until, 0);
    SDL_SetAtomicInt(&sLimits[i].hash, h);
    return &sLimits[i];
}

/* a service is in a daily-limit pause now (the lobby's words for its error) */
int Match_ServiceLimited(void) {
    int now = (int)(SDL_GetTicks() / 1000), i;
    for (i = 0; i < 4; i++) {
        if (SDL_GetAtomicInt(&sLimits[i].hash) != 0 && now < SDL_GetAtomicInt(&sLimits[i].until) && SDL_GetAtomicInt(&sLimits[i].daily)) {
            return 1;
        }
    }
    return 0;
}

static int limit_before(const char *base, char *out, size_t size) {
    int now = (int)(SDL_GetTicks() / 1000);
    Limit *l = limit_of(base, 0);
    if (l != NULL && now < SDL_GetAtomicInt(&l->until)) {
        SDL_strlcpy(out, SDL_GetAtomicInt(&l->daily) ? "{\"error\":\"limit\",\"held\":1}" : "{\"error\":\"busy\",\"held\":1}", size);
        return 1;
    }
    if (now - SDL_GetAtomicInt(&sAskedAt) >= 60) {
        int n = SDL_SetAtomicInt(&sAsked, 0);
        if (n > 0) {
            fprintf(stderr, "net: %d requests to the online services in %d s\n", n, now - SDL_GetAtomicInt(&sAskedAt));
        }
        SDL_SetAtomicInt(&sAskedAt, now);
    }
    SDL_AddAtomicInt(&sAsked, 1);
    return 0;
}

/* BT3_NET_TEST_429=1: every request answered as Cloudflare's 1027 would be, without asking (testing the pause) */
static int limit_test(char *out, size_t size) {
    if (getenv("BT3_NET_TEST_429") == NULL) { /* (the C library's: on Android SDL's copy predates bt3_env.txt) */
        return 0;
    }
    SDL_strlcpy(out, "{\"error\":\"limit\",\"http\":429}", size);
    return 1;
}

/* 1: the answer was a 429 (a failure for Dragon Net's switch to server B, as Cloudflare's own page always was) */
static int limit_after(const char *base, const char *what, int ok, const char *out) {
    if (ok && (SDL_strstr(out, "\"http\":429") != NULL || SDL_strstr(out, "error code: 1027") != NULL)) {
        Limit *l = limit_of(base, 1);
        int wait = SDL_GetAtomicInt(&l->wait);
        wait = wait == 0 ? 30 : wait * 2 > 600 ? 600 : wait * 2;
        SDL_SetAtomicInt(&l->wait, wait);
        SDL_SetAtomicInt(&l->daily, SDL_strstr(out, "\"busy\"") == NULL);
        SDL_SetAtomicInt(&l->until, (int)(SDL_GetTicks() / 1000) + wait);
        fprintf(stderr, "net: %s: the service is over its limit (HTTP 429, %s): nothing asked of it for %d s\n", what,
                SDL_strstr(out, "\"busy\"") != NULL ? "the Worker's own" : "its daily requests: Cloudflare 1027", wait);
        if (SDL_GetAtomicInt(&l->daily)) { /* Dragon Net's server A out of its day: server B at once, not after three tries */
            int k;
            for (k = 0; k < DRNET_FAILS; k++) {
                DrNet_NoteResult(base, 0);
            }
        }
        return 1;
    }
    if (ok && out[0] == '{' && SDL_strstr(out, "\"error\"") == NULL) {
        Limit *l = limit_of(base, 0);
        if (l != NULL) {
            SDL_SetAtomicInt(&l->wait, 0);
        }
    }
    return 0;
}

static int http_post_to(const char *base, const char *what, const char *json, char *out, size_t size) {
    int ok, limited;
    if (base == NULL) {
        base = match_url();
    }
    if (limit_before(base, out, size)) {
        return 1;
    }
    ok = limit_test(out, size) || http_post_once(base, what, json, out, size);
    limited = limit_after(base, what, ok, out);
    /* refused for the time it was signed for: the answer says the service's time; once more by that */
    if (ok && Sign_HasKey() && SDL_strstr(out, "\"build key\"") != NULL && SDL_strstr(out, "\"why\":\"time\"") != NULL && SDL_strstr(out, "\"now\":") != NULL) {
        clock_learn(out);
        ok = http_post_once(base, what, json, out, size);
    }
    DrNet_NoteResult(base, ok && out[0] == '{' && !limited);
    return ok;
}

static int http_post(const char *what, const char *json, char *out, size_t size) {
    return http_post_to(NULL, what, json, out, size);
}
#define POSTB(w, j, o, s) http_post_to(base, (w), (j), (o), (s))

/* A code entered on Dragon Net may belong to the other region-independent Worker. Retry only code joins and
   spectator requests; keep the selected service in sM.url once the room is found so its later polls follow it. */
static int retry_code_on_other_dragon_server(char *base, const char *what, const char *body, char *answer, size_t size) {
    const char *other = NULL;
    int dailyLimit;
    if (DrNet_Mode() != 0 || DrRanked_Active() || SDL_getenv("BT3_MATCH_URL") != NULL ||
        (sM.role != 2 && sM.role != 4)) {
        return 0;
    }
    if (SDL_strcmp(base, DrNet_Url()) == 0) {
        other = DrNet_UrlB();
    } else if (SDL_strcmp(base, DrNet_UrlB()) == 0) {
        other = DrNet_Url();
    }
    if (other == NULL || other[0] == '\0' || SDL_strcmp(base, other) == 0) {
        return 0;
    }
    dailyLimit = SDL_strstr(answer, "\"error\":\"limit\"") != NULL ||
                 SDL_strstr(answer, "error code: 1027") != NULL ||
                 (SDL_strstr(answer, "429") != NULL && SDL_strstr(answer, "\"busy\"") == NULL);
    if (SDL_strstr(answer, "no room") == NULL && !dailyLimit) {
        return 0;
    }
    if (!http_post_to(other, what, body, answer, size)) {
        return 0;
    }
    if (SDL_strstr(answer, "\"peer\":") == NULL || SDL_strstr(answer, "\"error\"") != NULL) {
        return 1; /* use the second service's more relevant error, but don't claim the code lives there */
    }
    SDL_strlcpy(base, other, 256);
    SDL_LockMutex(sM.lock);
    SDL_strlcpy(sM.url, other, sizeof(sM.url));
    SDL_UnlockMutex(sM.lock);
    fprintf(stderr, "dragonnet: room code found on the other matchmaking server\n");
    return 1;
}

const char *Match_ServerLabel(void) {
    static char url[256];
    SDL_LockMutex(sM.lock);
    SDL_strlcpy(url, sM.url[0] != '\0' ? sM.url : match_url(), sizeof(url));
    SDL_UnlockMutex(sM.lock);
    if (DrNet_Mode() == 0) {
        if (SDL_strcmp(url, DrNet_Url()) == 0) return "A";
        if (SDL_strcmp(url, DrNet_UrlB()) == 0) return "B";
        return "Custom";
    }
    return "PC";
}

/* What the window says when the service lets only official releases in (its REQUIRE_KEY) and refused this program:
   `plain` for a program without a key, or with one the service does not know. */
static const char *key_refused(const char *answer, const char *plain) {
    return Sign_HasKey() && SDL_strstr(answer, "\"why\":\"time\"") != NULL ? "This computer's clock is wrong: set its date and time, then try again." : plain;
}
#define KEY_REFUSED(answer) (SDL_strstr(answer, "\"build key\"") != NULL)

/* The text of "key":"..." in a flat JSON answer. 0 = not there. */
static int json_str(const char *json, const char *key, char *out, size_t size);

/* The number of "key":n in a flat JSON answer, `def` if not there. */
static int json_int(const char *json, const char *key, int def) {
    char pat[40];
    const char *p;

    SDL_snprintf(pat, sizeof(pat), "\"%s\":", key);
    p = SDL_strstr(json, pat);
    return p != NULL ? SDL_atoi(p + SDL_strlen(pat)) : def;
}

static int json_str(const char *json, const char *key, char *out, size_t size) {
    char pat[40];
    const char *p, *e;

    SDL_snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    p = SDL_strstr(json, pat);
    if (p == NULL) {
        return 0;
    }
    p += SDL_strlen(pat);
    e = SDL_strchr(p, '"');
    if (e == NULL || (size_t)(e - p) >= size) {
        return 0;
    }
    SDL_memcpy(out, p, (size_t)(e - p));
    out[e - p] = '\0';
    return 1;
}

/* The peer of an answer: "peer":{"name":"..","addrs":["..",".."]}. 0 = none ("peer":null, or no such part). */
static int json_peer(const char *json, MatchStatus *st) {
    const char *p = SDL_strstr(json, "\"peer\":{"), *a;
    int n = 0;

    if (p == NULL) {
        return 0;
    }
    json_str(p, "name", st->peerName, sizeof(st->peerName));
    a = SDL_strstr(p, "\"addrs\":[");
    if (a == NULL) {
        return 0;
    }
    a += 9;
    while (*a == '"' && n < MATCH_ADDRS) {
        const char *e = SDL_strchr(a + 1, '"');
        if (e == NULL || (size_t)(e - a - 1) >= sizeof(st->peerAddr[0])) {
            break;
        }
        SDL_memcpy(st->peerAddr[n], a + 1, (size_t)(e - a - 1));
        st->peerAddr[n][e - a - 1] = '\0';
        n++;
        a = e + 1;
        if (*a == ',') {
            a++;
        }
    }
    st->peerCount = n;
    return n > 0;
}

/* ---- STUN (RFC 5389): one question, "which address do you see me under?" ---- */

static int stun_ask(int sock, const char *host, int port, char *out, size_t size) {
    struct addrinfo hints, *res = NULL;
    uint8_t req[20], pkt[512];
    char portText[8];
    int tries, i;

    SDL_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    SDL_snprintf(portText, sizeof(portText), "%d", port);
    if (getaddrinfo(host, portText, &hints, &res) != 0 || res == NULL) {
        return 0;
    }
    SDL_memset(req, 0, sizeof(req));
    req[1] = 0x01; /* binding request, no attributes */
    req[4] = 0x21; req[5] = 0x12; req[6] = 0xA4; req[7] = 0x42; /* the magic cookie */
    for (i = 8; i < 20; i++) {
        req[i] = (uint8_t)SDL_rand(256);
    }
    for (tries = 0; tries < 3 && !STOPPED(); tries++) {
        int waited;
        sendto(sock, (const char *)req, sizeof(req), 0, res->ai_addr, (int)res->ai_addrlen);
        for (waited = 0; waited < 500; waited += 10) {
            struct sockaddr_in from;
            socklen_t fromLen = sizeof(from);
            int n = (int)recvfrom(sock, (char *)pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &fromLen), at;
            if (n < 20 || pkt[0] != 0x01 || pkt[1] != 0x01 || SDL_memcmp(pkt + 8, req + 8, 12) != 0) {
                SDL_Delay(10);
                continue;
            }
            for (at = 20; at + 4 <= n;) {
                int type = pkt[at] << 8 | pkt[at + 1], len = pkt[at + 2] << 8 | pkt[at + 3];
                if (at + 4 + len > n) {
                    break;
                }
                if ((type == 0x0020 || type == 0x0001) && len >= 8 && pkt[at + 5] == 0x01) {
                    const uint8_t *v = &pkt[at + 4];
                    unsigned p = (unsigned)(v[2] << 8 | v[3]);
                    uint8_t ip[4] = {v[4], v[5], v[6], v[7]};
                    if (type == 0x0020) { /* XOR-MAPPED-ADDRESS: with the cookie */
                        p ^= 0x2112;
                        ip[0] ^= 0x21; ip[1] ^= 0x12; ip[2] ^= 0xA4; ip[3] ^= 0x42;
                    }
                    SDL_snprintf(out, size, "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3], p);
                    freeaddrinfo(res);
                    return 1;
                }
                at += 4 + ((len + 3) & ~3);
            }
        }
    }
    freeaddrinfo(res);
    return 0;
}

/* This machine's address on its own network (for two players behind the same router): the address a socket would
   send from, found without sending anything. */
static int lan_address(int localPort, char *out, size_t size) {
    struct sockaddr_in to, me;
    socklen_t len = sizeof(me);
    int s = (int)socket(AF_INET, SOCK_DGRAM, 0), ok = 0;
    char text[32];

    if (s < 0) {
        return 0;
    }
    SDL_memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(9);
    inet_pton(AF_INET, "192.0.2.1", &to.sin_addr); /* (a documentation address: nothing is sent) */
    if (connect(s, (struct sockaddr *)&to, sizeof(to)) == 0 && getsockname(s, (struct sockaddr *)&me, &len) == 0 &&
        inet_ntop(AF_INET, &me.sin_addr, text, sizeof(text)) != NULL && SDL_strcmp(text, "0.0.0.0") != 0) {
        SDL_snprintf(out, size, "%s:%d", text, localPort);
        ok = 1;
    }
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
    return ok;
}

/* ---- the thread ---- */

static int SDLCALL match_thread(void *arg) {
    char pub[64] = "", lan[64] = "", addrs[160], body[512], answer[4096], key[40] = "", you[64] = "", name[20], relayAddr[64] = "";
    int i, n = 0, hosting, searches = 0, again = 0;
    Uint64 began = SDL_GetTicks(); /* (a quick match: when the search began, for `waited`) */

    SDL_SetTLS(&sMine, arg, NULL);
    char base[256];
    SDL_LockMutex(sM.lock);
    SDL_strlcpy(base, sM.url[0] != '\0' ? sM.url : match_url(), sizeof(base));
    SDL_UnlockMutex(sM.lock);
    for (i = 0; sM.name[i] != '\0' && n < (int)sizeof(name) - 1; i++) { /* (a name that needs no escaping) */
        char c = sM.name[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-' || c == '.') {
            name[n++] = c;
        }
    }
    name[n] = '\0';
    if (!stun_ask(sM.sock, "stun.cloudflare.com", 3478, pub, sizeof(pub))) {
        stun_ask(sM.sock, "stun.l.google.com", 19302, pub, sizeof(pub));
    }
    lan_address(sM.localPort, lan, sizeof(lan));
    if (STOPPED()) {
        return 0;
    }
    SDL_LockMutex(sM.lock);
    SDL_strlcpy(sM.st.self, pub, sizeof(sM.st.self));
    SDL_UnlockMutex(sM.lock);
    /* (no public address found: the Worker's view of this machine's address with the local port is tried in its
       place, below, once the Worker has said what it sees) */
    SDL_snprintf(addrs, sizeof(addrs), "%s%s%s%s%s%s%s", pub[0] ? "\"" : "", pub, pub[0] ? "\"" : "", pub[0] && lan[0] ? "," : "", lan[0] ? "\"" : "", lan,
                 lan[0] ? "\"" : "");
    if (addrs[0] == '\0') {
        set_state(-1, "This computer's address could not be found (no network?).");
        return 0;
    }
again:
    hosting = sM.role == 1 || sM.role == 5;
    if (sM.role == 3) {
        /* A quick match: the service puts this player with whoever waits longest in the queue (then this is the
           one who joins), or gives it a room of its own in the queue to wait in (then it is the host). A room
           lasts ten minutes; when it runs out the search simply starts again. */
        MatchStatus st;
        /* (`waited` and `area`: this game knows nearby matching, and searches again from its room when the service
           says `again`; see port/matchmaking/worker.js) */
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"rel\":%d,\"name\":\"%s\",\"addrs\":[%s],\"queue\":\"%s\",\"who\":\"%s\",\"waited\":%d,\"area\":\"%s\"%s}", sM.version, PORT_RELEASE, name, addrs,
                     sM.queue, match_who(), (int)((SDL_GetTicks() - began) / 1000), sM.area, relayAddr[0] != '\0' ? ",\"relay\":false" : "");
        if (!POSTB("quick", body, answer, sizeof(answer))) {
            set_state(-1, "The matchmaking service could not be reached (is the network up?).");
            return 0;
        }
        if (STOPPED()) {
            return 0;
        }
        searches++;
        SDL_memset(&st, 0, sizeof(st));
        if (SDL_strstr(answer, "\"role\":\"join\"") != NULL && json_peer(answer, &st)) {
            SDL_LockMutex(sM.lock);
            SDL_strlcpy(sM.st.peerName, st.peerName, sizeof(sM.st.peerName));
            SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
            sM.st.peerCount = st.peerCount;
            sM.st.role = 2;
            sM.st.state = 3;
            json_str(answer, "region", sM.st.region, sizeof(sM.st.region));
            SDL_UnlockMutex(sM.lock);
            json_str(answer, "code", sM.code, sizeof(sM.code));
            json_str(answer, "key", key, sizeof(key));
            goto joined;
        }
        if (SDL_strstr(answer, "\"role\":\"host\"") == NULL) {
            set_state(-1, SDL_strstr(answer, "\"old\"") != NULL ? "Update available. Online servers are unreachable until you update." :
                          KEY_REFUSED(answer) ? key_refused(answer, "Quick match needs an official release of the game.") :
                          SDL_strstr(answer, "busy") != NULL ? "Too many searches from this address; try again in a few minutes." :
                          SDL_strstr(answer, "bad request") != NULL || SDL_strstr(answer, "error") != NULL ? "The matchmaking service does not know quick match yet." :
                          "The matchmaking service gave no answer.");
            return 0;
        }
        hosting = 1;
        again = json_int(answer, "again", 0);
        SDL_LockMutex(sM.lock);
        sM.st.widen = json_int(answer, "widen", 0);
        SDL_UnlockMutex(sM.lock);
    }
    if (hosting) {
        if (sM.role == 1 || sM.role == 5) {
            SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"rel\":%d,\"name\":\"%s\",\"addrs\":[%s]%s}", sM.version, PORT_RELEASE, name, addrs,
                         sM.role == 5 ? ",\"lobby\":true" : "");
            if (!POSTB("host", body, answer, sizeof(answer))) {
                set_state(-1, "The matchmaking service could not be reached (is the network up?).");
                return 0;
            }
            if (STOPPED()) {
                return 0;
            }
        }
        if (!json_str(answer, "code", sM.code, sizeof(sM.code)) || !json_str(answer, "key", key, sizeof(key))) {
            fprintf(stderr, "net: the service answered: %.200s\n", answer);
            set_state(-1, SDL_strstr(answer, "no lobbies") != NULL ? "This server does not support lobbies yet. Its Worker and database need the lobby update." :
                          SDL_strstr(answer, "\"old\"") != NULL ? "Update available. Online servers are unreachable until you update." :
                          KEY_REFUSED(answer) ? key_refused(answer, "Room codes need an official release of the game. Playing by address still works.") :
                          SDL_strstr(answer, "busy") != NULL ? "Too many rooms from this address; try again in a few minutes." : "The matchmaking service gave no room.");
            return 0;
        }
        if (relayAddr[0] != '\0') { /* (a search begun again: the relay address it holds already goes with the new room) */
            SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\",\"addrs\":[%s,\"relay=%s\"]}", sM.code, key, addrs, relayAddr);
            POSTB("addrs", body, answer, sizeof(answer));
        } else {
            /* A relay address, if the service gives access to one: published with the others, for a player who
               cannot reach this one directly. The code is shown only after this, so nobody joins before. */
            char server[128], user[160], pass[160], relay[64];
#ifdef __ANDROID__
            if (SDL_strstr(answer, "\"turn\"") != NULL || SDL_strstr(answer, "noRelay") != NULL) { /* (Android: always in the log) */
#else
            if (SDL_getenv("BT3_RELAY_LOG") != NULL && (SDL_strstr(answer, "\"turn\"") != NULL || SDL_strstr(answer, "noRelay") != NULL)) {
#endif
                char why[160] = ""; /* (a service that hands a login out with the room: one that is asked later says nothing here) */
                json_str(answer, "noRelay", why, sizeof(why));
                fprintf(stderr, "relay: the service %s%s\n", SDL_strstr(answer, "\"turn\"") != NULL ? "gave access" : "gave no access: ", why);
            }
            if (SDL_getenv("BT3_NO_RELAY") == NULL && json_str(answer, "server", server, sizeof(server)) && json_str(answer, "username", user, sizeof(user)) &&
                json_str(answer, "credential", pass, sizeof(pass)) && Relay_Allocate(sM.sock, server, user, pass, relay, sizeof(relay)) && !STOPPED()) {
                SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\",\"addrs\":[%s,\"relay=%s\"]}", sM.code, key, addrs, relay);
                POSTB("addrs", body, answer, sizeof(answer));
                SDL_strlcpy(relayAddr, relay, sizeof(relayAddr));
            }
        }
        if (STOPPED()) {
            SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", sM.code, key);
            POSTB("leave", body, answer, sizeof(answer));
            return 0;
        }
        SDL_LockMutex(sM.lock);
        SDL_strlcpy(sM.st.code, sM.code, sizeof(sM.st.code)); /* (shown from here on) */
        sM.st.role = 1;
        sM.st.searches = searches;
        sM.st.state = 2;
        SDL_UnlockMutex(sM.lock);
        if (sM.role == 5) {
            goto lobby;
        }
        SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"%s}", sM.code, key, sM.watch ? ",\"watch\":true" : "");
        Uint64 searched = SDL_GetTicks();
        for (i = 0; i < 165 && !STOPPED(); i++) { /* a room lasts ten minutes */
            int k;
            /* asked every two seconds in the first minute, every four after it (the service's daily requests) */
            for (k = 0; k < (i < 20 ? 30 : 60) && !STOPPED(); k++) {
                SDL_Delay(100);
            }
            if (STOPPED()) {
                break;
            }
            if (sM.role == 3 && again > 0 && SDL_GetTicks() - searched >= (Uint64)again * 1000) {
                /* A quick match that waits searches again from its room (every `again` seconds, as the service
                   says): someone nearer may have come, or (Auto) it now looks everywhere, and two who both wait
                   meet this way. The service holds the room meanwhile: someone who has joined it is not lost
                   ("joined": the poll below has them). */
                char ask[768];
                MatchStatus st;
                searched = SDL_GetTicks();
                SDL_snprintf(ask, sizeof(ask), "{\"v\":%d,\"rel\":%d,\"name\":\"%s\",\"addrs\":[%s],\"queue\":\"%s\",\"who\":\"%s\",\"waited\":%d,\"area\":\"%s\",\"code\":\"%s\",\"key\":\"%s\"}",
                             sM.version, PORT_RELEASE, name, addrs, sM.queue, match_who(), (int)((searched - began) / 1000), sM.area, sM.code, key);
                if (POSTB("quick", ask, answer, sizeof(answer)) && !STOPPED()) {
                    SDL_memset(&st, 0, sizeof(st));
                    if (SDL_strstr(answer, "\"role\":\"join\"") != NULL && json_peer(answer, &st)) { /* (its own room is gone) */
                        SDL_LockMutex(sM.lock);
                        SDL_strlcpy(sM.st.peerName, st.peerName, sizeof(sM.st.peerName));
                        SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
                        sM.st.peerCount = st.peerCount;
                        sM.st.role = 2;
                        sM.st.state = 3;
                        json_str(answer, "region", sM.st.region, sizeof(sM.st.region));
                        SDL_UnlockMutex(sM.lock);
                        json_str(answer, "code", sM.code, sizeof(sM.code));
                        json_str(answer, "key", key, sizeof(key));
                        goto joined;
                    }
                    if (SDL_strstr(answer, "\"error\":\"no room\"") != NULL) {
                        break; /* (the room has run out: searched for anew) */
                    }
                }
                if (STOPPED()) {
                    break;
                }
            }
            if (POSTB("poll", body, answer, sizeof(answer))) {
                MatchStatus st;
                SDL_memset(&st, 0, sizeof(st));
                json_watchers(answer);
                if (json_peer(answer, &st)) {
                    if (STOPPED()) {
                        break;
                    }
                    SDL_LockMutex(sM.lock);
                    SDL_strlcpy(sM.st.peerName, st.peerName, sizeof(sM.st.peerName));
                    SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
                    sM.st.peerCount = st.peerCount;
                    sM.st.state = 3;
                    json_str(answer, "region", sM.st.region, sizeof(sM.st.region));
                    SDL_UnlockMutex(sM.lock);
                    if (relayAddr[0] != '\0' || SDL_getenv("BT3_NO_RELAY") != NULL) {
                        if (sM.watch) {
                            goto keep;
                        }
                        POSTB("leave", body, answer, sizeof(answer)); /* the room has done its work: it is given up at once */
                        return 0;
                    }
                    /* No relay address yet (it is asked for only when it is needed). The lobby now tries the direct
                       ways; it ends this search when the two are connected. If that has not happened in a few
                       seconds, a login is asked for, the address is added to the room (the one who joined learns it
                       by its poll), and the lobby is told to let the other player through. The room is kept until
                       the lobby is done, so that the other side can still ask. */
                    {
                        int w, wait = SDL_getenv("BT3_RELAY_ONLY") != NULL ? 2 : 40;
                        char server[128], user[160], pass[160], relay[64];
                        for (w = 0; w < wait && !STOPPED(); w++) {
                            SDL_Delay(100);
                        }
                        if (!STOPPED() && POSTB("relay", body, answer, sizeof(answer))) {
                            if (SDL_getenv("BT3_RELAY_LOG") != NULL) {
                                char why[160] = "";
                                json_str(answer, "noRelay", why, sizeof(why));
                                fprintf(stderr, "relay: asked for when the direct ways stayed silent: the service %s%s\n",
                                        SDL_strstr(answer, "\"turn\"") != NULL ? "gave access" : "gave no access: ", why);
                            }
                            if (json_str(answer, "server", server, sizeof(server)) && json_str(answer, "username", user, sizeof(user)) &&
                                json_str(answer, "credential", pass, sizeof(pass)) && !STOPPED()) {
                                int got;
                                SDL_SetAtomicInt(&sM.sockBusy, 1);
                                SDL_Delay(80); /* (the lobby is between two looks at the socket by then) */
                                got = !STOPPED() && Relay_Allocate(sM.sock, server, user, pass, relay, sizeof(relay));
                                SDL_SetAtomicInt(&sM.sockBusy, 0);
                                if (got && !STOPPED()) {
                                    char more[512];
                                    SDL_snprintf(more, sizeof(more), "{\"code\":\"%s\",\"key\":\"%s\",\"addrs\":[%s,\"relay=%s\"]}", sM.code, key, addrs, relay);
                                    POSTB("addrs", more, answer, sizeof(answer));
                                    SDL_LockMutex(sM.lock);
                                    sM.st.relayGen++;
                                    SDL_UnlockMutex(sM.lock);
                                }
                            }
                        }
                        for (w = 0; w < 250 && !STOPPED() && !sM.watch; w++) { /* (the lobby gives up after 25 seconds at the latest) */
                            SDL_Delay(100);
                        }
                    }
                    if (sM.watch) {
                        goto keep;
                    }
                    POSTB("leave", body, answer, sizeof(answer));
                    return 0;
                }
                if (SDL_strstr(answer, "no room") != NULL) {
                    break;
                }
            }
        }
        if (STOPPED()) {
            POSTB("leave", body, answer, sizeof(answer));
            return 0;
        }
        if (sM.role == 3) { /* (a quick match waits for as long as the player does) */
            POSTB("leave", body, answer, sizeof(answer));
            goto again;
        }
        set_state(-1, "Nobody joined: the room code has run out. Host again for a new one.");
    } else if (sM.role == 4) { /* a spectator: the host's addresses (and the host learns this one's by its poll) */
        MatchStatus st;
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"rel\":%d,\"lobby\":1,\"code\":\"%s\",\"name\":\"%s\",\"addrs\":[%s]}", sM.version, PORT_RELEASE, sM.code, name, addrs);
        if (!POSTB("watch", body, answer, sizeof(answer))) {
            set_state(-1, "The matchmaking service could not be reached (is the network up?).");
            return 0;
        }
        (void)retry_code_on_other_dragon_server(base, "watch", body, answer, sizeof(answer));
        SDL_memset(&st, 0, sizeof(st));
        if (!json_peer(answer, &st)) {
            set_state(-1, SDL_strstr(answer, "\"old\"") != NULL ? "Update available. Online servers are unreachable until you update." :
                          KEY_REFUSED(answer) ? key_refused(answer, "Room codes need an official release of the game. Watching by address still works.") :
                          SDL_strstr(answer, "version") != NULL ? "The host's game is a different version: watching needs the same release as the host's." :
                          SDL_strstr(answer, "full") != NULL ? "Too many have asked to watch that room in the last two minutes; try again soon." :
                          SDL_strstr(answer, "no room") != NULL ? "No room with that code, or the matchmaking service does not take spectators yet. (Codes last ten minutes; a host on 0.1.38.7 or older gives its room up when the match begins.)" :
                          SDL_strstr(answer, "busy") != NULL ? "Too many wrong codes from this address; try again in an hour." :
                          SDL_strstr(answer, "error") != NULL ? "The matchmaking service does not take spectators yet. Watching by address still works." :
                          "The matchmaking service gave no answer.");
            return 0;
        }
        if (STOPPED()) {
            return 0;
        }
        SDL_LockMutex(sM.lock);
        SDL_strlcpy(sM.st.peerName, st.peerName, sizeof(sM.st.peerName));
        SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
        sM.st.peerCount = st.peerCount;
        sM.st.lobby = SDL_strstr(answer, "\"lobby\":true") != NULL;
        sM.st.state = 3;
        SDL_UnlockMutex(sM.lock);
        if (sM.st.lobby) { /* a lobby's code: a member, who asks for the owner's relay address as the one who joins does */
            json_str(answer, "key", key, sizeof(key));
            goto joined;
        }
        return 0;
    } else {
        MatchStatus st;
        /* (`lobby`: this game can be a lobby's member; a code of a lobby is then answered as one) */
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"rel\":%d,\"lobby\":1,\"code\":\"%s\",\"name\":\"%s\",\"addrs\":[%s]}", sM.version, PORT_RELEASE, sM.code, name, addrs);
        if (!POSTB("join", body, answer, sizeof(answer))) {
            set_state(-1, "The matchmaking service could not be reached (is the network up?).");
            return 0;
        }
        (void)retry_code_on_other_dragon_server(base, "join", body, answer, sizeof(answer));
        (void)you;
        SDL_memset(&st, 0, sizeof(st));
        if (!json_peer(answer, &st)) {
            fprintf(stderr, "net: the service answered: %.200s\n", answer);
            set_state(-1, SDL_strstr(answer, "\"old\"") != NULL ? "Update available. Online servers are unreachable until you update." :
                          KEY_REFUSED(answer) ? key_refused(answer, "Room codes need an official release of the game. Playing by address still works.") :
                          SDL_strstr(answer, "version") != NULL ? "The host's game is a different version: both players need the same release." :
                          SDL_strstr(answer, "full") != NULL ? "Someone has already joined that room." :
                          SDL_strstr(answer, "no room") != NULL ? "No room with that code (codes last ten minutes)." :
                          SDL_strstr(answer, "busy") != NULL ? "Too many wrong codes from this address, or too many joining that lobby at once; try again soon." : "The matchmaking service gave no answer.");
            return 0;
        }
        if (STOPPED()) {
            return 0;
        }
        SDL_LockMutex(sM.lock);
        SDL_strlcpy(sM.st.peerName, st.peerName, sizeof(sM.st.peerName));
        SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
        sM.st.peerCount = st.peerCount;
        sM.st.lobby = SDL_strstr(answer, "\"lobby\":true") != NULL; /* (a lobby's code: this game is a member) */
        sM.st.state = 3;
        SDL_UnlockMutex(sM.lock);
        json_str(answer, "key", key, sizeof(key));
        goto joined;
    }
    return 0;
lobby:
    /* A lobby's owner. The room lives as long as the lobby: asked every four seconds (900 requests an hour of lobby)
       for the members who have joined with the code (their addresses, as spectators', Match_Watchers); the lobby
       knocks at each and lets it through its relay. A relay address is asked for when the lobby says a member
       cannot reach it directly (Match_RelayAsk), once a member has come. */
    SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\",\"watch\":true,\"keep\":true}", sM.code, key);
    {
        int first = 1;
        while (!STOPPED()) {
            int w;
            for (w = 0; w < (first ? 1 : 40) && !STOPPED() && SDL_GetAtomicInt(&sM.relayAsk) != 1; w++) {
                SDL_Delay(100);
            }
            first = 0;
            if (STOPPED()) {
                break;
            }
            if (SDL_GetAtomicInt(&sM.relayAsk) == 1) {
                char server[128], user[160], pass[160], relay[64], ask[160];
                SDL_SetAtomicInt(&sM.relayAsk, 2); /* (once) */
                if (relayAddr[0] != '\0' || SDL_getenv("BT3_NO_RELAY") != NULL) {
                    continue;
                }
                SDL_snprintf(ask, sizeof(ask), "{\"code\":\"%s\",\"key\":\"%s\"}", sM.code, key);
                if (POSTB("relay", ask, answer, sizeof(answer))) {
                    if (SDL_getenv("BT3_RELAY_LOG") != NULL) {
                        char why[160] = "";
                        json_str(answer, "noRelay", why, sizeof(why));
                        fprintf(stderr, "relay: the lobby asked for one: the service %s%s\n", SDL_strstr(answer, "\"turn\"") != NULL ? "gave access" : "gave no access: ", why);
                    }
                    if (json_str(answer, "server", server, sizeof(server)) && json_str(answer, "username", user, sizeof(user)) &&
                        json_str(answer, "credential", pass, sizeof(pass)) && !STOPPED()) {
                        int got;
                        SDL_SetAtomicInt(&sM.sockBusy, 1);
                        SDL_Delay(80); /* (the lobby is between two looks at the socket by then) */
                        got = !STOPPED() && Relay_Allocate(sM.sock, server, user, pass, relay, sizeof(relay));
                        SDL_SetAtomicInt(&sM.sockBusy, 0);
                        if (got && !STOPPED()) {
                            char more[512];
                            SDL_snprintf(more, sizeof(more), "{\"code\":\"%s\",\"key\":\"%s\",\"addrs\":[%s,\"relay=%s\"]}", sM.code, key, addrs, relay);
                            POSTB("addrs", more, answer, sizeof(answer));
                            SDL_strlcpy(relayAddr, relay, sizeof(relayAddr));
                            SDL_AddAtomicInt(&sM.relayGen, 1);
                        }
                    }
                }
                continue;
            }
            if (POSTB("poll", body, answer, sizeof(answer))) {
                if (SDL_strstr(answer, "no room") != NULL) {
                    set_state(-1, "The lobby's room code has run out (the matchmaking service no longer knows it). Members by address can still come.");
                    return 0;
                }
                json_watchers(answer);
            }
        }
    }
    SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", sM.code, key);
    POSTB("leave", body, answer, sizeof(answer));
    return 0;
keep:
    /* A host that lets spectators in, once the other player has come: the room is kept while the match is on (the
       session ends this search, Match_Cancel), and asked every five seconds for spectators who have come (the
       service's requests a day: 720 an hour of match). */
    SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\",\"watch\":true,\"keep\":true}", sM.code, key);
    while (!STOPPED()) {
        int w;
        for (w = 0; w < 50 && !STOPPED(); w++) {
            SDL_Delay(100);
        }
        if (STOPPED()) {
            break;
        }
        if (POSTB("poll", body, answer, sizeof(answer))) {
            if (SDL_strstr(answer, "no room") != NULL) {
                return 0;
            }
            json_watchers(answer);
        }
    }
    SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", sM.code, key);
    POSTB("leave", body, answer, sizeof(answer));
    return 0;
joined:
    /* The one who joined. If the host's direct addresses stay silent, the host asks for a relay address and adds it
       to the room: asked for here every two seconds, from the third second on, until the lobby ends this search
       (connected, or given up) or the address has come. */
    if (key[0] != '\0' && sM.code[0] != '\0') {
        MatchStatus st;
        int round, w, known;
        SDL_LockMutex(sM.lock);
        known = sM.st.peerCount;
        SDL_UnlockMutex(sM.lock);
        SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", sM.code, key);
        for (round = 0; round < 11 && !STOPPED(); round++) {
            for (w = 0; w < (round == 0 ? 30 : 20) && !STOPPED(); w++) {
                SDL_Delay(100);
            }
            if (STOPPED() || !POSTB("poll", body, answer, sizeof(answer))) {
                continue;
            }
            if (SDL_strstr(answer, "no room") != NULL) {
                break; /* (the host has given the room up: it is connected, or gone) */
            }
            SDL_memset(&st, 0, sizeof(st));
            if (json_peer(answer, &st) && SDL_strstr(answer, "relay=") != NULL && st.peerCount != known && !STOPPED()) {
                SDL_LockMutex(sM.lock);
                SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
                sM.st.peerCount = st.peerCount;
                sM.st.addrGen++;
                SDL_UnlockMutex(sM.lock);
                break;
            }
        }
    }
    return 0;
}

/* ---- for the lobby (net.c) ---- */

/* Starts looking: role 1 makes a room, role 2 joins room `code`. `sock` is the match's own UDP socket (not blocking),
   bound to `localPort`. */
void Match_Begin(int role, int sock, int localPort, const char *code, const char *name, int version) {
    version = DrNet_WireVersion(version); /* (Dragon Net: the netcode and the simulation, net_dragon.h) */
    int i, n = 0;

    if (sM.lock == NULL) {
        sM.lock = SDL_CreateMutex();
    }
    if (sM.thread != NULL) {
        SDL_DetachThread(sM.thread); /* (it stops by itself: its search number is no longer the current one) */
        sM.thread = NULL;
    }
    SDL_AddAtomicInt(&sM.gen, 1);
    SDL_SetAtomicInt(&sM.cancel, 0);
    SDL_memset(&sM.st, 0, sizeof(sM.st));
    sM.st.state = 1;
    sM.role = role;
    sM.sock = sock;
    sM.localPort = localPort;
    sM.version = version;
    sM.code[0] = '\0';
    for (i = 0; code != NULL && code[i] != '\0' && n < 6; i++) { /* letters and digits, capitals */
        char c = code[i];
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            sM.code[n++] = c;
        }
    }
    sM.code[n] = '\0';
    SDL_strlcpy(sM.queue, role == 3 ? sM.code : "", sizeof(sM.queue));
    SDL_strlcpy(sM.name, name != NULL ? name : "Player", sizeof(sM.name));
    sM.url[0] = '\0';
    if (DrNet_Mode() == 0 && SDL_getenv("BT3_MATCH_URL") == NULL) {
        if (DrRanked_Active()) {
            if (DrNet_RankedUrl()[0] == '\0') {
                SDL_LockMutex(sM.lock);
                sM.st.state = -1;
                SDL_strlcpy(sM.st.error, "Ranked is unavailable right now.", sizeof(sM.st.error));
                SDL_UnlockMutex(sM.lock);
                return;
            }
            SDL_strlcpy(sM.url, DrNet_RankedActiveUrl(), sizeof(sM.url));
        } else {
            SDL_strlcpy(sM.url, DrNet_ActiveUrl(), sizeof(sM.url));
        }
    }
    sM.watchCount = 0;
    sM.watchGen++;
    SDL_SetAtomicInt(&sM.relayAsk, 0);
    sM.thread = SDL_CreateThread(match_thread, "bt3-match", (void *)(intptr_t)SDL_GetAtomicInt(&sM.gen));
    if (sM.thread == NULL) {
        set_state(-1, "The matchmaking could not be started.");
    }
}

/* ---- how many wait in each quick-match queue (for the window): asked in a thread of its own, now and then ---- */
static SDL_AtomicInt sQueueCount[MATCH_QUEUES], sQueueBusy;
static int sQueueVersion;
const char *const kMatchQueues[MATCH_QUEUES] = {"S", "T", "D10", "D15", "D20"};

static int SDLCALL queues_thread(void *arg) {
    char body[64], answer[512];
    int i;

    (void)arg;
    SDL_snprintf(body, sizeof(body), "{\"v\":%d}", sQueueVersion);
    if (http_post("queues", body, answer, sizeof(answer)) && SDL_strstr(answer, "\"waiting\"") != NULL) {
        for (i = 0; i < MATCH_QUEUES; i++) {
            char key[16];
            const char *p;
            SDL_snprintf(key, sizeof(key), "\"%s\":", kMatchQueues[i]);
            p = SDL_strstr(answer, key);
            SDL_SetAtomicInt(&sQueueCount[i], p != NULL ? SDL_atoi(p + SDL_strlen(key)) + 1 : 0);
        }
    }
    SDL_SetAtomicInt(&sQueueBusy, 0);
    return 0;
}

/* (Is there a newer release? That is gs/net_update.c's, the automatic update.) */

/* ---- is this release new enough? asked when the online screen opens ---- */
static SDL_AtomicInt sRelMin, sRelLatest, sRelBusy;

static int SDLCALL hello_thread(void *arg) {
    char body[64], answer[256];
    const char *p;

    (void)arg;
    SDL_snprintf(body, sizeof(body), "{\"rel\":%d}", PORT_RELEASE);
    if (http_post("hello", body, answer, sizeof(answer)) && (p = SDL_strstr(answer, "\"min\":")) != NULL) {
        SDL_SetAtomicInt(&sRelMin, SDL_atoi(p + 6));
        p = SDL_strstr(answer, "\"latest\":");
        SDL_SetAtomicInt(&sRelLatest, p != NULL ? SDL_atoi(p + 9) : 0);
        clock_learn(answer); /* (a service that knows the build key says its time) */
    }
    SDL_SetAtomicInt(&sRelBusy, 0);
    return 0;
}

void Match_HelloAsk(void) {
    SDL_Thread *t;
    if (SDL_GetAtomicInt(&sRelBusy)) {
        return;
    }
    SDL_SetAtomicInt(&sRelBusy, 1);
    t = SDL_CreateThread(hello_thread, "bt3-hello", NULL);
    if (t == NULL) {
        SDL_SetAtomicInt(&sRelBusy, 0);
    } else {
        SDL_DetachThread(t);
    }
}

/* This release's number, and what the service last said: the oldest it lets in and the newest there is (0: not
   said, or not asked yet). */
int Match_Release(int *min, int *latest) {
    *min = SDL_GetAtomicInt(&sRelMin);
    *latest = SDL_GetAtomicInt(&sRelLatest);
    return PORT_RELEASE;
}

/* Asks the service (nothing happens if an answer is still on its way). */
void Match_QueuesAsk(int version) {
    SDL_Thread *t;
    if (SDL_GetAtomicInt(&sQueueBusy)) {
        return;
    }
    SDL_SetAtomicInt(&sQueueBusy, 1);
    sQueueVersion = DrNet_WireVersion(version);
    t = SDL_CreateThread(queues_thread, "bt3-queues", NULL);
    if (t == NULL) {
        SDL_SetAtomicInt(&sQueueBusy, 0);
    } else {
        SDL_DetachThread(t);
    }
}

/* Players waiting in queue i as last heard, -1 if not known. */
int Match_QueueWaiting(int i) {
    return i >= 0 && i < MATCH_QUEUES ? SDL_GetAtomicInt(&sQueueCount[i]) - 1 : -1;
}

/* A quick match: where to look (set before Match_Begin): "auto", "any" or a region ("EU"). */
void Match_Area(const char *area) {
    SDL_strlcpy(sM.area, area != NULL && area[0] != '\0' ? area : "auto", sizeof(sM.area));
}

/* A host: whether it lets spectators in (set before Match_Begin). */
void Match_Watch(int on) {
    sM.watch = on != 0;
}

/* The spectators' addresses as last heard (up to `max`); *gen changes whenever they do. */
int Match_Watchers(char out[][64], int max, int *gen) {
    int n;
    if (sM.lock == NULL) {
        *gen = 0;
        return 0;
    }
    SDL_LockMutex(sM.lock);
    n = sM.watchCount < max ? sM.watchCount : max;
    SDL_memcpy(out, sM.watchAddr, sizeof(sM.watchAddr[0]) * (size_t)n);
    *gen = sM.watchGen;
    SDL_UnlockMutex(sM.lock);
    return n;
}

int Match_SocketBusy(void) {
    return SDL_GetAtomicInt(&sM.sockBusy);
}

void Match_RelayAsk(void) {
    SDL_CompareAndSwapAtomicInt(&sM.relayAsk, 0, 1);
}

int Match_RelayGen(void) {
    return SDL_GetAtomicInt(&sM.relayGen);
}

/* Where it stands. */
void Match_Poll(MatchStatus *out) {
    if (sM.lock == NULL) {
        SDL_memset(out, 0, sizeof(*out));
        return;
    }
    SDL_LockMutex(sM.lock);
    *out = sM.st;
    SDL_UnlockMutex(sM.lock);
}

/* Stops looking (and, for a host, gives the room up). Does not wait for the thread: a request to the Worker that is
   under way ends by itself. */
void Match_Cancel(void) {
    if (sM.thread != NULL) {
        SDL_SetAtomicInt(&sM.cancel, 1);
        SDL_DetachThread(sM.thread);
        sM.thread = NULL;
    }
    if (sM.lock != NULL) {
        SDL_LockMutex(sM.lock);
        sM.st.state = 0;
        SDL_UnlockMutex(sM.lock);
    }
}

/* BT3_SIGN_TEST (called before the game starts, plat_mem.c; the program ends after it): the build key's signature
   for a fixed request, for port/matchmaking/test_sign.py. BT3_SIGN_TEST=post also sends one "hello" the way every
   request to the service goes (so the headers can be looked at), and only to this computer: BT3_MATCH_URL has to
   name http://127.0.0.1 or http://localhost. */
int Match_SignTest(void) {
    const char *mode = SDL_getenv("BT3_SIGN_TEST"), *url = match_url();
    char body[64], answer[512] = "";

    Sign_SelfTest();
    if (mode != NULL && SDL_strcmp(mode, "post") == 0) {
        if (SDL_strncmp(url, "http://127.0.0.1", 16) != 0 && SDL_strncmp(url, "http://localhost", 16) != 0) {
            printf("sign: post: BT3_MATCH_URL does not name this computer, nothing sent\n");
            return 1;
        }
        SDL_snprintf(body, sizeof(body), "{\"rel\":%d}", PORT_RELEASE);
        printf("sign: post %s\n", http_post("hello", body, answer, sizeof(answer)) ? answer : "no answer");
    }
    fflush(stdout);
    return 0;
}

/* ---- Ranked C/D (DBT-NX v1.7): C primeiro; D so depois que o C provou a autoridade E ---- */
#ifndef DRNET_URL_RANKED_D
#define DRNET_URL_RANKED_D "https://bt3-net-rooms.alexsanchez01es.workers.dev"
#endif

static int rk_host_ok(const char *u) {
    const char *p;
    size_t n = SDL_strlen(u);
    if (n < 12 || n > 120 || SDL_strncmp(u, "https://", 8) != 0) {
        return 0;
    }
    for (p = u + 8; *p != '\0'; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) {
            return 0;
        }
    }
    return 1;
}

const char *DrNet_RankedUrlD(void) {
    static char f[256];
    static int read;
    if (!read) {
        read = 1;
        url_file("dragonnet_ranked_url_d.txt", f, sizeof(f));
        if (f[0] == '\0' || !rk_host_ok(f)) {
            SDL_strlcpy(f, DRNET_URL_RANKED_D, sizeof(f));
        }
    }
    return f;
}

static SDL_AtomicInt sRkUseD, sRkDay, sRkFails, sRkShared, sRkVerC, sRkVerD;
static char sRkAuth[72];

static int rk_text(const char *json, const char *key, char *out, size_t cap) {
    char field[64];
    const char *s;
    size_t n = 0;
    SDL_snprintf(field, sizeof(field), "\"%s\":", key);
    s = SDL_strstr(json, field);
    if (s == NULL) {
        return 0;
    }
    s += SDL_strlen(field);
    while (*s == ' ') {
        s++;
    }
    if (*s++ != '"') {
        return 0;
    }
    while (*s != '\0' && *s != '"' && n + 1 < cap) {
        if (*s == '\\') {
            return 0;
        }
        out[n++] = *s++;
    }
    out[n] = '\0';
    return *s == '"';
}

static int rk_quota(const char *out) {
    return out[0] != '{' && (SDL_strstr(out, "1027") != NULL || SDL_strstr(out, "1015") != NULL ||
                             SDL_strstr(out, "Too Many Requests") != NULL);
}

static int rk_bad(int ok, const char *out) {
    return !ok || out[0] != '{' || SDL_strstr(out, "\"ranked unavailable\"") != NULL ||
           SDL_strstr(out, "\"migration incomplete\"") != NULL;
}

static const char *rk_pick(void) {
    if (SDL_GetAtomicInt(&sRkUseD)) {
        if (SDL_GetAtomicInt(&sRkDay) != utc_day()) {
            SDL_SetAtomicInt(&sRkUseD, 0); /* novo dia UTC: volta ao C */
            SDL_SetAtomicInt(&sRkFails, 0);
            fprintf(stderr, "dragonnet: ranked back to C\n");
        } else {
            return DrNet_RankedUrlD();
        }
    }
    return DrNet_RankedUrl();
}

/* so o C conta; o D falhando nao volta ao C no mesmo dia (como o B) */
static void rk_note(const char *base, int ok, int exhausted) {
    if (SDL_strcmp(base, DrNet_RankedUrl()) != 0 || !SDL_GetAtomicInt(&sRkShared) || SDL_GetAtomicInt(&sRkUseD)) {
        return;
    }
    if (ok) {
        SDL_SetAtomicInt(&sRkFails, 0);
        return;
    }
    if (exhausted || SDL_AddAtomicInt(&sRkFails, 1) + 1 >= DRNET_FAILS) {
        SDL_SetAtomicInt(&sRkDay, utc_day());
        SDL_SetAtomicInt(&sRkUseD, 1);
        SDL_SetAtomicInt(&sRkFails, 0);
        fprintf(stderr, "dragonnet: ranked C failed, using D until 00:00 UTC\n");
    }
}

/* 1 = C/D compartilhado, 0 = C antigo (so C), -1 = inalcancavel ou inseguro */
static int rk_verify(const char *base) {
    char url[320], out[2048], a[72], c[256], d[256];
    int ok, isD = SDL_strcmp(base, DrNet_RankedUrl()) != 0;
    if (!SDL_GetAtomicInt(&sRkShared) && DrNet_CdLoad(a, (int)sizeof(a))) {
        SDL_strlcpy(sRkAuth, a, sizeof(sRkAuth));
        SDL_SetAtomicInt(&sRkShared, 1);
    }
    if (isD) {
        if (!SDL_GetAtomicInt(&sRkShared)) {
            return -1;
        }
        if (SDL_GetAtomicInt(&sRkVerD)) {
            return 1;
        }
    } else {
        if (SDL_GetAtomicInt(&sRkVerC) == 1) {
            return 1;
        }
        if (SDL_GetAtomicInt(&sRkVerC) == 2) {
            return 0;
        }
    }
    SDL_snprintf(url, sizeof(url), "%s/v1/ranked/capabilities", base);
    out[0] = '\0';
    ok = http_post_native(url, "{\"v\":5001,\"cd\":1}", NULL, out, sizeof(out));
    if (rk_bad(ok, out)) {
        rk_note(base, 0, rk_quota(out));
        return -1;
    }
    if (SDL_strstr(out, "\"cd\":1") == NULL || SDL_strstr(out, "\"migration_complete\":1") == NULL) {
        if (!isD && !SDL_GetAtomicInt(&sRkShared)) {
            SDL_SetAtomicInt(&sRkVerC, 2); /* C antigo: segue so com o C */
            return 0;
        }
        return -1;
    }
    if (!rk_text(out, "authority", a, sizeof(a)) || !rk_text(out, "url_c", c, sizeof(c)) ||
        !rk_text(out, "url_d", d, sizeof(d)) || SDL_strcmp(c, DrNet_RankedUrl()) != 0 ||
        SDL_strcmp(d, DrNet_RankedUrlD()) != 0) {
        return -1;
    }
    if (SDL_GetAtomicInt(&sRkShared)) {
        if (SDL_strcmp(a, sRkAuth) != 0) {
            fprintf(stderr, "dragonnet: ranked authority differs: refused\n");
            return -1;
        }
    } else if (!isD) {
        if (!DrNet_CdSave(a)) {
            return -1;
        }
        SDL_strlcpy(sRkAuth, a, sizeof(sRkAuth));
        SDL_SetAtomicInt(&sRkShared, 1);
    } else {
        return -1;
    }
    SDL_SetAtomicInt(isD ? &sRkVerD : &sRkVerC, 1);
    fprintf(stderr, "dragonnet: ranked c/d verified (%s)\n", isD ? "D" : "C");
    return 1;
}

/* salas ranqueadas: o D so entra depois de verificado */
const char *DrNet_RankedActiveUrl(void) {
    const char *base = rk_pick();
    if (SDL_strcmp(base, DrNet_RankedUrl()) != 0 && !SDL_GetAtomicInt(&sRkVerD)) {
        return DrNet_RankedUrl();
    }
    return base;
}

/* a partida ranqueada fica no servidor (C ou D) em que o servico a colocou */
#define RK_PINS 2
static char sRkPinMatch[RK_PINS][33];
static char sRkPinBack[RK_PINS]; /* 'C' ou 'D' */
static int sRkPinNext;
static SDL_AtomicInt sRkPinBusy;

static void rk_pin_lock(void) {
    while (!SDL_CompareAndSwapAtomicInt(&sRkPinBusy, 0, 1)) {
        SDL_Delay(1);
    }
}

static void rk_pin_unlock(void) {
    SDL_SetAtomicInt(&sRkPinBusy, 0);
}

/* acha "match":"<32 hex>" no JSON (pedido ou resposta) */
static int rk_match_of(const char *json, char *match) {
    const char *s = SDL_strstr(json, "\"match\":\"");
    int i;
    if (s == NULL) {
        return 0;
    }
    s += 9;
    for (i = 0; i < 32; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return 0;
        }
        match[i] = c;
    }
    if (s[32] != '"') {
        return 0;
    }
    match[32] = '\0';
    return 1;
}

static char rk_pin_get(const char *match) {
    char back = 0;
    int i;
    rk_pin_lock();
    for (i = 0; i < RK_PINS; i++) {
        if (sRkPinBack[i] != 0 && SDL_strcmp(sRkPinMatch[i], match) == 0) {
            back = sRkPinBack[i];
        }
    }
    rk_pin_unlock();
    return back;
}

static void rk_pin_set(const char *match, char back) {
    int i;
    rk_pin_lock();
    for (i = 0; i < RK_PINS; i++) {
        if (sRkPinBack[i] != 0 && SDL_strcmp(sRkPinMatch[i], match) == 0) {
            rk_pin_unlock(); /* a partida fica onde foi colocada */
            return;
        }
    }
    i = sRkPinNext;
    sRkPinNext = (sRkPinNext + 1) % RK_PINS;
    SDL_strlcpy(sRkPinMatch[i], match, sizeof(sRkPinMatch[i]));
    sRkPinBack[i] = back;
    rk_pin_unlock();
    fprintf(stderr, "dragonnet: ranked match pinned to %c\n", back);
}

static int rk_post(const char *what, const char *json, char *out, size_t size) {
    char url[320], a[72], match[33], back[8];
    const char *base;
    char pin = 0;
    int ok, bad, shared;
    out[0] = '\0';
    if (DrNet_RankedUrl()[0] == '\0') {
        return 0; /* sem C: Ranked indisponivel */
    }
    if (rk_match_of(json, match)) {
        pin = rk_pin_get(match);
    }
    base = rk_pick();
    if (pin == 'D') {
        base = DrNet_RankedUrlD();
    } else if (pin == 'C') {
        base = DrNet_RankedUrl();
    }
    shared = rk_verify(base);
    if (shared < 0) {
        return 0;
    }
    SDL_snprintf(url, sizeof(url), "%s/v1/%s", base, what);
    ok = http_post_native(url, json, NULL, out, size);
    bad = rk_bad(ok, out);
    rk_note(base, !bad, rk_quota(out));
    if (!bad && shared && rk_text(out, "authority", a, sizeof(a)) && SDL_strcmp(a, sRkAuth) != 0) {
        out[0] = '\0';
        return 0;
    }
    if (!bad && shared && rk_match_of(out, match) && rk_text(out, "backend", back, sizeof(back)) &&
        (back[0] == 'C' || back[0] == 'D') && back[1] == '\0') {
        rk_pin_set(match, back[0]);
    }
    return ok;
}

/* Dragon Net (net_dragon.c): a POST to its service whatever the mode, <service>/v1/<what>; the answer's text in out
   (also an error's: {"error":...}). 0 = no answer. Never signed (the build key is the PC port's). */
int Match_PostDragon(const char *what, const char *json, char *out, size_t size) {
    char url[320];
    const char *base;
    int ok, presence = SDL_strcmp(what, "presence") == 0;
    out[0] = '\0';
    if (SDL_strncmp(what, "ranked/", 7) == 0) {
        return rk_post(what, json, out, size);
    } else if (presence) {
        base = DrNet_ActiveUrl();
    } else {
        return 0; /* chat e o resto: desligado */
    }
    SDL_snprintf(url, sizeof(url), "%s/v1/%s", base, what);
    if (limit_before(base, out, size)) {
        return 1;
    }
    ok = limit_test(out, size) || http_post_native(url, json, NULL, out, size);
    if (limit_after(base, what, ok, out)) {
        ok = presence ? 0 : ok; /* (server A out of its requests: a failure for the switch to B) */
    }
    if (presence) {
        DrNet_NoteResult(base, ok && out[0] == '{');
    }
    return ok;
}
