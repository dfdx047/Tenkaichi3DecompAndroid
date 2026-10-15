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
 * X-BT3-Time and X-BT3-Sign. One built without sends none of them. The request to GitHub (the update check) is
 * never signed.
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

static struct {
    SDL_Mutex *lock;
    SDL_Thread *thread;
    SDL_AtomicInt cancel, gen; /* gen: which search is the current one (a thread of an earlier one writes nothing) */
    SDL_AtomicInt sockBusy;    /* the thread is talking to the relay on the match's socket: the lobby keeps off it */
    MatchStatus st;
    int role, sock, localPort, version;
    char code[8], name[20]; /* (role 3, a quick match: `code` is the queue's name until the service gives a room) */
    char queue[8];
} sM;

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

static const char *match_url(void) {
    const char *e = SDL_getenv("BT3_MATCH_URL");
    return e != NULL && e[0] != '\0' ? e : MATCH_URL_DEFAULT;
}

static void set_state(int state, const char *error) {
    if ((int)(intptr_t)SDL_GetTLS(&sMine) != SDL_GetAtomicInt(&sM.gen)) {
        return;
    }
    SDL_LockMutex(sM.lock);
    sM.st.state = state;
    if (error != NULL) {
        SDL_strlcpy(sM.st.error, error, sizeof(sM.st.error));
    }
    SDL_UnlockMutex(sM.lock);
}

/* ---- the Worker, through curl ---- */
/* (http_post_native: `json` NULL makes it a GET of `url`, for the update check further down. `headers`: lines
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
static int http_post_once(const char *what, const char *json, char *out, size_t size) {
    char url[256], path[64], sig[65], head[3][96];
    const char *args[20], *headers[4] = {NULL, NULL, NULL, NULL};
    SDL_Process *p;
    size_t got = 0;
    int code = 0, n = 0, i;
    void *data;
    long long when = (long long)time(NULL) + SDL_GetAtomicInt(&sClockAhead);

    SDL_snprintf(url, sizeof(url), "%s/v1/%s", match_url(), what);
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

static int http_post(const char *what, const char *json, char *out, size_t size) {
    int ok = http_post_once(what, json, out, size);
    /* refused for the time it was signed for: the answer says the service's time; once more by that */
    if (ok && Sign_HasKey() && SDL_strstr(out, "\"build key\"") != NULL && SDL_strstr(out, "\"why\":\"time\"") != NULL && SDL_strstr(out, "\"now\":") != NULL) {
        clock_learn(out);
        ok = http_post_once(what, json, out, size);
    }
    return ok;
}

/* What the window says when the service lets only official releases in (its REQUIRE_KEY) and refused this program:
   `plain` for a program without a key, or with one the service does not know. */
static const char *key_refused(const char *answer, const char *plain) {
    return Sign_HasKey() && SDL_strstr(answer, "\"why\":\"time\"") != NULL ? "This computer's clock is wrong: set its date and time, then try again." : plain;
}
#define KEY_REFUSED(answer) (SDL_strstr(answer, "\"build key\"") != NULL)

/* The text of "key":"..." in a flat JSON answer. 0 = not there. */
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
    char pub[64] = "", lan[64] = "", addrs[160], body[512], answer[1024], key[40] = "", you[64] = "", name[20], relayAddr[64] = "";
    int i, n = 0, hosting, searches = 0;

    SDL_SetTLS(&sMine, arg, NULL);
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
    /* (no public address found: the Worker's view of this machine's address with the local port is tried in its
       place, below, once the Worker has said what it sees) */
    SDL_snprintf(addrs, sizeof(addrs), "%s%s%s%s%s%s%s", pub[0] ? "\"" : "", pub, pub[0] ? "\"" : "", pub[0] && lan[0] ? "," : "", lan[0] ? "\"" : "", lan,
                 lan[0] ? "\"" : "");
    if (addrs[0] == '\0') {
        set_state(-1, "This computer's address could not be found (no network?).");
        return 0;
    }
again:
    hosting = sM.role == 1;
    if (sM.role == 3) {
        /* A quick match: the service puts this player with whoever waits longest in the queue (then this is the
           one who joins), or gives it a room of its own in the queue to wait in (then it is the host). A room
           lasts ten minutes; when it runs out the search simply starts again. */
        MatchStatus st;
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"rel\":%d,\"name\":\"%s\",\"addrs\":[%s],\"queue\":\"%s\",\"who\":\"%s\"%s}", sM.version, PORT_RELEASE, name, addrs, sM.queue,
                     match_who(), relayAddr[0] != '\0' ? ",\"relay\":false" : "");
        if (!http_post("quick", body, answer, sizeof(answer))) {
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
    }
    if (hosting) {
        if (sM.role == 1) {
            SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"rel\":%d,\"name\":\"%s\",\"addrs\":[%s]}", sM.version, PORT_RELEASE, name, addrs);
            if (!http_post("host", body, answer, sizeof(answer))) {
                set_state(-1, "The matchmaking service could not be reached (is the network up?).");
                return 0;
            }
            if (STOPPED()) {
                return 0;
            }
        }
        if (!json_str(answer, "code", sM.code, sizeof(sM.code)) || !json_str(answer, "key", key, sizeof(key))) {
            set_state(-1, SDL_strstr(answer, "\"old\"") != NULL ? "Update available. Online servers are unreachable until you update." :
                          KEY_REFUSED(answer) ? key_refused(answer, "Room codes need an official release of the game. Playing by address still works.") :
                          SDL_strstr(answer, "busy") != NULL ? "Too many rooms from this address; try again in a few minutes." : "The matchmaking service gave no room.");
            return 0;
        }
        if (relayAddr[0] != '\0') { /* (a search begun again: the relay address it holds already goes with the new room) */
            SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\",\"addrs\":[%s,\"relay=%s\"]}", sM.code, key, addrs, relayAddr);
            http_post("addrs", body, answer, sizeof(answer));
        } else {
            /* A relay address, if the service gives access to one: published with the others, for a player who
               cannot reach this one directly. The code is shown only after this, so nobody joins before. */
            char server[128], user[160], pass[160], relay[64];
            if (SDL_getenv("BT3_RELAY_LOG") != NULL && (SDL_strstr(answer, "\"turn\"") != NULL || SDL_strstr(answer, "noRelay") != NULL)) {
                char why[160] = ""; /* (a service that hands a login out with the room: one that is asked later says nothing here) */
                json_str(answer, "noRelay", why, sizeof(why));
                fprintf(stderr, "relay: the service %s%s\n", SDL_strstr(answer, "\"turn\"") != NULL ? "gave access" : "gave no access: ", why);
            }
            if (SDL_getenv("BT3_NO_RELAY") == NULL && json_str(answer, "server", server, sizeof(server)) && json_str(answer, "username", user, sizeof(user)) &&
                json_str(answer, "credential", pass, sizeof(pass)) && Relay_Allocate(sM.sock, server, user, pass, relay, sizeof(relay)) && !STOPPED()) {
                SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\",\"addrs\":[%s,\"relay=%s\"]}", sM.code, key, addrs, relay);
                http_post("addrs", body, answer, sizeof(answer));
                SDL_strlcpy(relayAddr, relay, sizeof(relayAddr));
            }
        }
        if (STOPPED()) {
            SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", sM.code, key);
            http_post("leave", body, answer, sizeof(answer));
            return 0;
        }
        SDL_LockMutex(sM.lock);
        SDL_strlcpy(sM.st.code, sM.code, sizeof(sM.st.code)); /* (shown from here on) */
        sM.st.role = 1;
        sM.st.searches = searches;
        sM.st.state = 2;
        SDL_UnlockMutex(sM.lock);
        SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", sM.code, key);
        for (i = 0; i < 165 && !STOPPED(); i++) { /* a room lasts ten minutes */
            int k;
            /* asked every two seconds in the first minute, every four after it (the service's daily requests) */
            for (k = 0; k < (i < 30 ? 20 : 40) && !STOPPED(); k++) {
                SDL_Delay(100);
            }
            if (STOPPED()) {
                break;
            }
            if (http_post("poll", body, answer, sizeof(answer))) {
                MatchStatus st;
                SDL_memset(&st, 0, sizeof(st));
                if (json_peer(answer, &st)) {
                    if (STOPPED()) {
                        break;
                    }
                    SDL_LockMutex(sM.lock);
                    SDL_strlcpy(sM.st.peerName, st.peerName, sizeof(sM.st.peerName));
                    SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
                    sM.st.peerCount = st.peerCount;
                    sM.st.state = 3;
                    SDL_UnlockMutex(sM.lock);
                    if (relayAddr[0] != '\0' || SDL_getenv("BT3_NO_RELAY") != NULL) {
                        http_post("leave", body, answer, sizeof(answer)); /* the room has done its work: it is given up at once */
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
                        if (!STOPPED() && http_post("relay", body, answer, sizeof(answer))) {
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
                                    http_post("addrs", more, answer, sizeof(answer));
                                    SDL_LockMutex(sM.lock);
                                    sM.st.relayGen++;
                                    SDL_UnlockMutex(sM.lock);
                                }
                            }
                        }
                        for (w = 0; w < 250 && !STOPPED(); w++) { /* (the lobby gives up after 25 seconds at the latest) */
                            SDL_Delay(100);
                        }
                    }
                    http_post("leave", body, answer, sizeof(answer));
                    return 0;
                }
                if (SDL_strstr(answer, "no room") != NULL) {
                    break;
                }
            }
        }
        if (STOPPED()) {
            http_post("leave", body, answer, sizeof(answer));
            return 0;
        }
        if (sM.role == 3) { /* (a quick match waits for as long as the player does) */
            http_post("leave", body, answer, sizeof(answer));
            goto again;
        }
        set_state(-1, "Nobody joined: the room code has run out. Host again for a new one.");
    } else {
        MatchStatus st;
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"rel\":%d,\"code\":\"%s\",\"name\":\"%s\",\"addrs\":[%s]}", sM.version, PORT_RELEASE, sM.code, name, addrs);
        if (!http_post("join", body, answer, sizeof(answer))) {
            set_state(-1, "The matchmaking service could not be reached (is the network up?).");
            return 0;
        }
        (void)you;
        SDL_memset(&st, 0, sizeof(st));
        if (!json_peer(answer, &st)) {
            set_state(-1, SDL_strstr(answer, "\"old\"") != NULL ? "Update available. Online servers are unreachable until you update." :
                          KEY_REFUSED(answer) ? key_refused(answer, "Room codes need an official release of the game. Playing by address still works.") :
                          SDL_strstr(answer, "version") != NULL ? "The host's game is a different version: both players need the same release." :
                          SDL_strstr(answer, "full") != NULL ? "Someone has already joined that room." :
                          SDL_strstr(answer, "no room") != NULL ? "No room with that code (codes last ten minutes)." :
                          SDL_strstr(answer, "busy") != NULL ? "Too many wrong codes from this address; try again in an hour." : "The matchmaking service gave no answer.");
            return 0;
        }
        if (STOPPED()) {
            return 0;
        }
        SDL_LockMutex(sM.lock);
        SDL_strlcpy(sM.st.peerName, st.peerName, sizeof(sM.st.peerName));
        SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
        sM.st.peerCount = st.peerCount;
        sM.st.state = 3;
        SDL_UnlockMutex(sM.lock);
        json_str(answer, "key", key, sizeof(key));
        goto joined;
    }
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
            if (STOPPED() || !http_post("poll", body, answer, sizeof(answer))) {
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

/* ---- is there a newer release? asked of GitHub, once per run, when the window is there ("Check for updates") ----
   One GET of the repository's newest published release (drafts and pre-releases are not it); nothing is sent but
   the request itself. The answer is long; the tag's name is near its start, and only the start is kept. */
static SDL_AtomicInt sUpdLatest, sUpdBusy;

static int SDLCALL update_thread(void *arg) {
    static char answer[4096];
    const char *url = "https://api.github.com/repos/" PORT_UPDATE_REPO "/releases/latest", *p;
    int ok;

    (void)arg;
    ok = SDL_getenv("BT3_MATCH_CURL") == NULL && http_post_native(url, NULL, NULL, answer, sizeof(answer));
    if (!ok) { /* the `curl` program, as for the service */
#ifdef _WIN32
        const char *args[] = {"curl.exe", "-s", "-L", "-m", "12", "-A", "Tenkaichi3Decomp", url, NULL};
#else
        const char *args[] = {"curl", "-s", "-L", "-m", "12", "-A", "Tenkaichi3Decomp", url, NULL};
#endif
        SDL_Process *pr = SDL_CreateProcess(args, true);
        size_t got = 0;
        int code = 0;
        void *data = pr != NULL ? SDL_ReadProcess(pr, &got, &code) : NULL;
        if (pr != NULL) {
            SDL_DestroyProcess(pr);
        }
        if (data != NULL) {
            SDL_strlcpy(answer, (const char *)data, sizeof(answer));
            SDL_free(data);
            ok = got > 0;
        }
    }
    if (ok && (p = SDL_strstr(answer, "\"tag_name\"")) != NULL && (p = SDL_strchr(p + 10, 'v')) != NULL) {
        int a = 0, b = 0, c = 0;
        if (SDL_sscanf(p, "v%d.%d.%d", &a, &b, &c) == 3 && a == 0 && b >= 0 && c >= 0 && c < 100) {
            SDL_SetAtomicInt(&sUpdLatest, b * 100 + c); /* as PORT_RELEASE counts: 0.1.33 is 133 */
            fprintf(stderr, "bt3: update check: the newest release is 0.%d.%d, this is 0.%d.%d\n", b, c, PORT_RELEASE / 100, PORT_RELEASE % 100);
        }
    }
    if (SDL_GetAtomicInt(&sUpdLatest) == 0) {
        fprintf(stderr, "bt3: update check: no answer\n");
    }
    SDL_SetAtomicInt(&sUpdBusy, 0);
    return 0;
}

void Match_UpdateAsk(void) {
    SDL_Thread *t;
    if (SDL_GetAtomicInt(&sUpdBusy)) {
        return;
    }
    SDL_SetAtomicInt(&sUpdBusy, 1);
    t = SDL_CreateThread(update_thread, "bt3-update", NULL);
    if (t == NULL) {
        SDL_SetAtomicInt(&sUpdBusy, 0);
    } else {
        SDL_DetachThread(t);
    }
}

/* The newest published release's number (0: not asked, no answer yet, or none). */
int Match_UpdateLatest(void) {
    return SDL_GetAtomicInt(&sUpdLatest);
}

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
    sQueueVersion = version;
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

int Match_SocketBusy(void) {
    return SDL_GetAtomicInt(&sM.sockBusy);
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
