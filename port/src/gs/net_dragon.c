/*
 * Dragon Net: the online play of Dragon Rage (Android) and DBT-NX (Switch) on a service of their own
 * (port/dragonnet: DBT-NX's Worker, MIT, by Unknown, updated to take several versions), apart from the PC port's.
 * A new release of the PC port does not stop it: games meet when their netcode AND their simulation are the same
 * (DrNet_WireVersion), not their release number. Against the PC the PC port's service goes on being used (mode 1).
 *
 * On top of rooms and quick matches (net.c and net_match.c, the same as against PC), the service has:
 *   Ranked   an Elo rating per player (a device identity: id and secret in dragonnet-device.txt in the game folder,
 *            never sent anywhere else); a search pairs players of a near rating; the two games report the fight's
 *            result and it counts only when both reports agree (the service settles it once);
 *   chat     the general chat, the last messages of everyone on the service;
 *   presence an approximate count of the players with the online screen open (an anonymous id of its own).
 * The protocol is DBT-NX's (port/switch/online/ranked.c there), so Ranked profiles, the chat and the count are the
 * same for both games. All of it runs on one thread of its own; the window reads its state (DrRanked_Poll, ...).
 *
 * A fight's result (DrNet_BattleFinished, from BattleResult_Finish) is written to dragonnet-result.txt before it is
 * sent, and sent again until the service has settled it (the app may be closed in between). Leaving a ranked
 * match before its fight is over (the pause menu, closing the game) is a loss for the one who leaves, as on the
 * Switch; the other side going away gives nobody points.
 */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_dragon.h"
#include "net_match.h"

extern int Port_Setting(const char *name, int def);
extern void Port_SettingSave(const char *name, int value);
extern void Port_SettingsWrite(void);
extern const char *Port_FileRoot(void);
extern int Port_NetActive(void);
extern int gPortResim;

/* ---- the mode and the version ---- */

static int sNetMode = -1;

int DrNet_Mode(void) {
    if (sNetMode < 0) {
        const char *e = SDL_getenv("BT3_NET_MODE");
        sNetMode = e != NULL ? (SDL_atoi(e) != 0) : (Port_Setting("net_mode", 0) != 0);
    }
    return sNetMode;
}

void DrNet_SetMode(int mode) {
    sNetMode = mode != 0;
    Port_SettingSave("net_mode", sNetMode);
    Port_SettingsWrite();
}

int DrNet_WireVersion(int netVersion) {
    return DrNet_Mode() == 0 ? netVersion * 1000 + DRNET_SIM : netVersion;
}

/* the `v` of Dragon Net's own requests (Ranked, chat, presence), whatever the mode */
static int wire(void) {
    extern int Port_NetVersion(void); /* net.c */
    return Port_NetVersion() * 1000 + DRNET_SIM;
}

/* ---- small things ---- */

static void path_of(const char *name, char *out, size_t size) {
    SDL_snprintf(out, size, "%s/%s", Port_FileRoot(), name);
}

static int read_text(const char *name, char *out, size_t size) {
    char path[600];
    FILE *f;
    size_t n;
    path_of(name, path, sizeof(path));
    out[0] = '\0';
    if ((f = fopen(path, "rb")) == NULL) {
        return 0;
    }
    n = fread(out, 1, size - 1, f);
    out[n] = '\0';
    fclose(f);
    return 1;
}

/* written whole or not at all (a temporary file, then renamed) */
static int write_text(const char *name, const char *text) {
    char path[600], tmp[620];
    FILE *f;
    int ok;
    path_of(name, path, sizeof(path));
    SDL_snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if ((f = fopen(tmp, "wb")) == NULL) {
        return 0;
    }
    ok = fwrite(text, 1, strlen(text), f) == strlen(text);
    ok = fclose(f) == 0 && ok;
    if (ok) {
        remove(path);
        ok = rename(tmp, path) == 0;
    }
    if (!ok) {
        remove(tmp);
    }
    return ok;
}

static void remove_file(const char *name) {
    char path[600];
    path_of(name, path, sizeof(path));
    remove(path);
}

static int is_hex(const char *s, size_t n) {
    size_t i;
    if (strlen(s) != n) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) {
            return 0;
        }
    }
    return 1;
}

static void random_hex(char *out, int n) {
    static const char d[] = "0123456789abcdef";
    unsigned char b[64];
    FILE *f = fopen("/dev/urandom", "rb");
    int i, got = f != NULL ? (int)fread(b, 1, (size_t)(n / 2), f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    for (i = got; i < n / 2; i++) {
        b[i] = (unsigned char)SDL_rand(256);
    }
    for (i = 0; i < n / 2; i++) {
        out[2 * i] = d[b[i] >> 4];
        out[2 * i + 1] = d[b[i] & 15];
    }
    out[n] = '\0';
}

/* the service's names: letters, digits, space and _.- (as it cleans them), at most 19 */
static void clean_name(const char *in, char *out, size_t size) {
    size_t k = 0;
    for (; in != NULL && *in != '\0' && k + 1 < size && k < 19; in++) {
        char c = *in;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '.' || c == '-') {
            out[k++] = c;
        }
    }
    out[k] = '\0';
    if (k == 0) {
        SDL_strlcpy(out, "Player", size);
    }
}

static int text_value(const char *json, const char *key, char *out, size_t size) {
    char find[48];
    const char *s;
    size_t k = 0;
    SDL_snprintf(find, sizeof(find), "\"%s\":", key);
    out[0] = '\0';
    if ((s = strstr(json, find)) == NULL) {
        return 0;
    }
    s += strlen(find);
    while (*s == ' ') {
        s++;
    }
    if (*s++ != '"') {
        return 0;
    }
    while (*s != '\0' && *s != '"' && k + 1 < size) {
        if (*s == '\\') {
            return 0;
        }
        out[k++] = *s++;
    }
    out[k] = '\0';
    return *s == '"';
}

static int number_value(const char *json, const char *key) {
    char find[48];
    const char *s;
    SDL_snprintf(find, sizeof(find), "\"%s\":", key);
    s = strstr(json, find);
    return s != NULL ? SDL_atoi(s + strlen(find)) : 0;
}

static void hex_encode(const char *in, char *out, size_t size) {
    static const char d[] = "0123456789abcdef";
    size_t k = 0;
    for (; *in != '\0' && k + 3 < size; in++) {
        unsigned char c = (unsigned char)*in;
        out[k++] = d[c >> 4];
        out[k++] = d[c & 15];
    }
    out[k] = '\0';
}

static void hex_decode(const char *in, size_t len, char *out, size_t size) {
    size_t k = 0, i;
    for (i = 0; i + 1 < len + 1 && in[i] != '\0' && in[i + 1] != '\0' && k + 1 < size; i += 2) {
        int hi = in[i] <= '9' ? in[i] - '0' : in[i] - 'a' + 10, lo = in[i + 1] <= '9' ? in[i + 1] - '0' : in[i + 1] - 'a' + 10;
        unsigned char c = (unsigned char)(hi << 4 | lo);
        out[k++] = (char)(c < 32 ? ' ' : c);
    }
    out[k] = '\0';
}

/* ---- the shared state (one lock) ---- */

static SDL_Mutex *sLock;
static SDL_Mutex *sOutboxLock; /* result queue file */
static SDL_Mutex *sOutboxLock; /* result queue file */
static SDL_AtomicInt sStarted;
static SDL_Condition *sWake;

static void lock_make(void) {
    static SDL_AtomicInt made;
    if (SDL_GetAtomicInt(&made) == 2) {
        return;
    }
    if (SDL_CompareAndSwapAtomicInt(&made, 0, 1)) {
        sLock = SDL_CreateMutex();
        sOutboxLock = SDL_CreateMutex();
        sOutboxLock = SDL_CreateMutex();
        sWake = SDL_CreateCondition();
        SDL_SetAtomicInt(&made, 2);
    }
    while (SDL_GetAtomicInt(&made) != 2) {
        SDL_Delay(1);
    }
}

static DrRanked sView;
static char sName[20] = "Player";
static char sId[33], sToken[65]; /* the device identity (the thread's) */
static int sRefresh, sWantSearch, sWantCancel, sWantBoard;
static unsigned sSearchGen;
static char sCancelMatch[33], sRoomPending[8], sHandedOver[33];

static int sChatOpen, sChatState;
static unsigned sChatAfter;
static DrChat sChat;
static char sChatOut[4][200]; /* messages waiting to be sent (hex comes later) */
static int sChatOutN;

static int sPresenceOn, sPresenceLeave, sOnline = -1;
static Uint64 sSearchAt; /* when the search under way began (the queue is asked less often as it goes on) */
static Uint64 sPresenceAt;

static void message(int msg, int state) {
    SDL_LockMutex(sLock);
    sView.msg = msg;
    if (state != 99) {
        sView.state = state;
    }
    SDL_UnlockMutex(sLock);
}

/* ---- the service ---- */

/* a Ranked request with the identity: "ranked/<what>" */
static int post_ranked(const char *what, const char *extra, char *answer, size_t size) {
    char body[1100], path[48];
    SDL_snprintf(path, sizeof(path), "ranked/%s", what);
    SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"id\":\"%s\",\"token\":\"%s\"%s}", wire(), sId, sToken, extra != NULL ? extra : "");
    if (!Match_PostDragon(path, body, answer, size)) {
        return 0;
    }
    if (strstr(answer, "\"error\":\"limit\"") != NULL) {
        message(DRMSG_LIMIT, 99); /* (a search goes on, without asking, until the pause is over: net_match.c) */
        return 0;
    }
    if (strstr(answer, "\"error\":\"version\"") != NULL) {
        message(DRMSG_VERSION, -1);
        return 0;
    }
    return strstr(answer, "\"error\"") == NULL;
}

/* the device identity: read, or made once by the service (then kept: deleting the file makes a new profile) */
static int identity(void) {
    char file[160], answer[1024], body[200], name[20];
    /* after a failed registration the next try waits (15 s, doubling to 5 minutes): a service out of its daily
       requests answered 429 and the worker asked again every 70 ms while the window kept refreshing */
    static Uint64 retryAt, retryWait;
    if (sId[0] != '\0') {
        return 1;
    }
    if (retryAt != 0 && SDL_GetTicks() < retryAt) {
        return 0;
    }
    if (read_text("dragonnet-device.txt", file, sizeof(file)) && sscanf(file, "%32s %64s", sId, sToken) == 2 && is_hex(sId, 32) && is_hex(sToken, 64)) {
        return 1;
    }
    sId[0] = sToken[0] = '\0';
    if (file[0] != '\0') { /* a damaged identity is never replaced by a new rating by itself */
        fprintf(stderr, "dragonnet: dragonnet-device.txt is damaged: Ranked is off until it is fixed or removed\n");
        message(DRMSG_REGISTER_FAILED, -1);
        return 0;
    }
    SDL_LockMutex(sLock);
    SDL_strlcpy(name, sName, sizeof(name));
    SDL_UnlockMutex(sLock);
    SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"name\":\"%s\"}", wire(), name);
    if (!Match_PostDragon("ranked/register", body, answer, sizeof(answer)) || !text_value(answer, "id", sId, sizeof(sId)) ||
        !text_value(answer, "token", sToken, sizeof(sToken)) || !is_hex(sId, 32) || !is_hex(sToken, 64)) {
        sId[0] = sToken[0] = '\0';
        retryWait = retryWait == 0 ? 15000 : retryWait * 2 > 300000 ? 300000 : retryWait * 2;
        retryAt = SDL_GetTicks() + retryWait;
        message(strstr(answer, "\"version\"") != NULL ? DRMSG_VERSION : strstr(answer, "\"limit\"") != NULL ? DRMSG_LIMIT : DRMSG_REGISTER_FAILED, -1);
        return 0;
    }
    SDL_snprintf(file, sizeof(file), "%s %s\n", sId, sToken);
    if (!write_text("dragonnet-device.txt", file)) {
        sId[0] = sToken[0] = '\0';
        message(DRMSG_REGISTER_FAILED, -1);
        return 0;
    }
    fprintf(stderr, "dragonnet: a Ranked profile was made for this device\n");
    return 1;
}

/* ---- the results waiting to be sent: dragonnet-result.txt, one line "match score digest round" ---- */

static int outbox_first_nl(char *match, int *score, char *digest) {
    char text[1024];
    unsigned round;
    if (!read_text("dragonnet-result.txt", text, sizeof(text)) || text[0] == '\0') {
        return 0;
    }
    return sscanf(text, "%32s %d %8s %u", match, score, digest, &round) >= 3 && is_hex(match, 32) && is_hex(digest, 8);
}

#define OUTBOX_MAX 1024

/* adds one match to the end of the queue (no overwrite) */
static int outbox_push_nl(const char *line) {
    char all[OUTBOX_MAX], id[33];
    size_t n;
    int nl;
    SDL_strlcpy(id, line, sizeof(id));
    all[0] = '\0';
    read_text("dragonnet-result.txt", all, sizeof(all));
    if (all[0] != '\0' && strstr(all, id) != NULL) {
        return 1; /* already queued */
    }
    n = strlen(all);
    nl = (n > 0 && all[n - 1] != '\n');
    if (n + nl + strlen(line) + 1 > sizeof(all)) {
        fprintf(stderr, "dragonnet: the result queue is full\n");
        return 0;
    }
    if (nl) {
        all[n++] = '\n';
    }
    SDL_strlcpy(all + n, line, sizeof(all) - n);
    return write_text("dragonnet-result.txt", all);
}

/* settles the first match of the queue only */
static void outbox_done_nl(void) {
    char all[OUTBOX_MAX];
    const char *rest;
    if (!read_text("dragonnet-result.txt", all, sizeof(all))) {
        return;
    }
    rest = strchr(all, '\n');
    if (rest == NULL || rest[1] == '\0') {
        remove_file("dragonnet-result.txt");
        return;
    }
    write_text("dragonnet-result.txt", rest + 1);
}

/* the queue file is shared by the game thread (push) and the net thread
   (first/done): one lock around each read-modify-write */
static int outbox_first_nl(char *match, int *score, char *digest) {
    int r;
    SDL_LockMutex(sOutboxLock);
    r = outbox_first_nl(match, score, digest);
    SDL_UnlockMutex(sOutboxLock);
    return r;
}

static int outbox_push_nl(const char *line) {
    int r;
    SDL_LockMutex(sOutboxLock);
    r = outbox_push_nl(line);
    SDL_UnlockMutex(sOutboxLock);
    return r;
}

static void outbox_done_nl(void) {
    SDL_LockMutex(sOutboxLock);
    outbox_done_nl();
    SDL_UnlockMutex(sOutboxLock);
}

/* the queue file is shared by the game thread (push) and the net thread
   (first/done): one lock around each read-modify-write */
static int outbox_first(char *match, int *score, char *digest) {
    int r;
    SDL_LockMutex(sOutboxLock);
    r = outbox_first_nl(match, score, digest);
    SDL_UnlockMutex(sOutboxLock);
    return r;
}

static int outbox_push(const char *line) {
    int r;
    SDL_LockMutex(sOutboxLock);
    r = outbox_push_nl(line);
    SDL_UnlockMutex(sOutboxLock);
    return r;
}

static void outbox_done(void) {
    SDL_LockMutex(sOutboxLock);
    outbox_done_nl();
    SDL_UnlockMutex(sOutboxLock);
}

/* the ranked match being played: dragonnet-session.txt, "match role mode sent" */
static int session_read(char *match, int *role, int *mode, int *sent) {
    char text[160];
    *role = *mode = *sent = 0;
    if (!read_text("dragonnet-session.txt", text, sizeof(text))) {
        return 0;
    }
    return sscanf(text, "%32s %d %d %d", match, role, mode, sent) >= 3 && is_hex(match, 32) && (*role == 1 || *role == 2);
}

static void session_write(const char *match, int role, int mode, int sent) {
    char text[160];
    SDL_snprintf(text, sizeof(text), "%s %d %d %d\n", match, role, mode, sent);
    write_text("dragonnet-session.txt", text);
}

/* ---- the thread ---- */

static void update_match(const char *answer, unsigned gen) {
    char state[24], assigned[33];
    text_value(answer, "state", state, sizeof(state));
    text_value(answer, "match", assigned, sizeof(assigned));
    SDL_LockMutex(sLock);
    if (gen != sSearchGen || !sWantSearch) { /* cancelled meanwhile: that search is given up (not a handed-over one) */
        int keep = strcmp(assigned, sHandedOver) == 0;
        SDL_UnlockMutex(sLock);
        if (!keep && is_hex(assigned, 32)) {
            char extra[64], ignored[512];
            SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\"", assigned);
            post_ranked("cancel", extra, ignored, sizeof(ignored));
        }
        return;
    }
    SDL_strlcpy(sView.match, assigned, sizeof(sView.match));
    text_value(answer, "code", sView.code, sizeof(sView.code));
    text_value(answer, "peer", sView.peer, sizeof(sView.peer));
    sView.role = number_value(answer, "role");
    sView.state = strcmp(state, "paired") == 0 ? 3 : strcmp(state, "waiting") == 0 ? 2 : 0;
    if (sView.state == 0) {
        sView.searching = sWantSearch = 0;
        sView.msg = strcmp(state, "final") == 0 ? DRMSG_CONFIRMED : DRMSG_ENDED_NO_POINTS;
    } else {
        sView.msg = sView.state == 3 ? DRMSG_FOUND : DRMSG_SEARCHING_LEVEL;
    }
    SDL_UnlockMutex(sLock);
}

static void do_result(int *refresh) {
    char match[33], digest[9], extra[160], answer[2048], state[24];
    int score;
    if (!outbox_first(match, &score, digest)) {
        return;
    }
    if (score < 0) {
        SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"round\":1", match);
    } else {
        SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"score\":%d,\"digest\":\"%s\"", match, score, digest);
    }
    if (post_ranked(score < 0 ? "forfeit" : "result", extra, answer, sizeof(answer))) {
        text_value(answer, "state", state, sizeof(state));
        fprintf(stderr, "dragonnet: ranked result: %s\n", state);
        if (strcmp(state, "final") == 0 || strcmp(state, "disputed") == 0 || strcmp(state, "cancelled") == 0 || strcmp(state, "expired") == 0) {
            outbox_done();
            *refresh = 1;
            message(strcmp(state, "final") == 0 ? DRMSG_CONFIRMED : strcmp(state, "disputed") == 0 ? DRMSG_DISPUTED : DRMSG_VOID, 0);
        } else {
            message(DRMSG_WAIT_RIVAL, 99);
        }
    } else if (strstr(answer, "\"error\"") != NULL) { /* refused (a match that is over, or not this device's): its state */
        SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\"", match);
        if (post_ranked("match", extra, answer, sizeof(answer)) &&
            (strstr(answer, "\"state\":\"cancelled\"") || strstr(answer, "\"state\":\"expired\"") || strstr(answer, "\"state\":\"disputed\"") ||
             strstr(answer, "\"state\":\"final\""))) {
            outbox_done();
            *refresh = 1;
            message(strstr(answer, "\"state\":\"final\"") != NULL ? DRMSG_CONFIRMED : DRMSG_VOID, 0);
        } else if (strstr(answer, "\"error\":\"match\"") != NULL) {
            outbox_done(); /* (not known to the service: nothing to wait for) */
        }
    }
}

static void do_chat(void) {
    char body[900], answer[8192], hex[420], nonce[33], out[200];
    const char *row;
    int n;
    SDL_LockMutex(sLock);
    out[0] = '\0';
    if (sChatOutN > 0) {
        SDL_strlcpy(out, sChatOut[0], sizeof(out));
    }
    SDL_UnlockMutex(sLock);
    if (out[0] != '\0') {
        hex_encode(out, hex, sizeof(hex) > 321 ? 321 : sizeof(hex));
        random_hex(nonce, 32);
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"id\":\"%s\",\"token\":\"%s\",\"after\":%u,\"text_hex\":\"%s\",\"nonce\":\"%s\"}", wire(), sId, sToken,
                     sChatAfter, hex, nonce);
    } else {
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"id\":\"%s\",\"token\":\"%s\",\"after\":%u}", wire(), sId, sToken, sChatAfter);
    }
    if (!Match_PostDragon("chat/sync", body, answer, sizeof(answer)) || strstr(answer, "\"rows\"") == NULL) {
        SDL_LockMutex(sLock);
        sChat.state = strstr(answer, "\"version\"") != NULL ? -2 : -1;
        if (strstr(answer, "slow down") != NULL && sChatOutN > 0) { /* (too many at once: this one waits) */
            sChat.state = 2;
        }
        SDL_UnlockMutex(sLock);
        return;
    }
    SDL_LockMutex(sLock);
    if (out[0] != '\0' && sChatOutN > 0) {
        memmove(sChatOut[0], sChatOut[1], sizeof(sChatOut[0]) * (size_t)(sChatOutN - 1));
        sChatOutN--;
    }
    sChat.state = 2;
    sChat.sending = sChatOutN;
    for (row = strstr(answer, "{\"seq\":"); row != NULL; row = strstr(row + 1, "{\"seq\":")) {
        unsigned seq = (unsigned)number_value(row, "seq");
        char nh[64], th[400];
        if (seq <= sChatAfter && sChatAfter != 0) {
            continue;
        }
        text_value(row, "name_hex", nh, sizeof(nh));
        text_value(row, "text_hex", th, sizeof(th));
        if (sChat.count == 24) {
            memmove(sChat.name[0], sChat.name[1], sizeof(sChat.name[0]) * 23);
            memmove(sChat.text[0], sChat.text[1], sizeof(sChat.text[0]) * 23);
            memmove(&sChat.mine[0], &sChat.mine[1], sizeof(sChat.mine[0]) * 23);
            sChat.count = 23;
        }
        n = sChat.count++;
        hex_decode(nh, strlen(nh), sChat.name[n], sizeof(sChat.name[n]));
        hex_decode(th, strlen(th), sChat.text[n], sizeof(sChat.text[n]));
        {
            const char *next = strstr(row + 1, "{\"seq\":"), *mine = strstr(row, "\"mine\":true");
            sChat.mine[n] = mine != NULL && (next == NULL || mine < next);
        }
        if (seq > sChatAfter) {
            sChatAfter = seq;
        }
    }
    SDL_UnlockMutex(sLock);
}

static void do_presence(void) {
    char file[80], id[33], body[160], answer[512];
    if (!read_text("dragonnet-presence.txt", file, sizeof(file)) || sscanf(file, "%32s", id) != 1 || !is_hex(id, 32)) {
        random_hex(id, 32);
        SDL_snprintf(file, sizeof(file), "%s\n", id);
        write_text("dragonnet-presence.txt", file);
    }
    SDL_LockMutex(sLock);
    int leave = sPresenceLeave;
    sPresenceLeave = 0;
    SDL_UnlockMutex(sLock);
    SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"id\":\"%s\"%s}", wire(), id, leave ? ",\"action\":\"leave\"" : "");
    if (Match_PostDragon("presence", body, answer, sizeof(answer)) && strstr(answer, "\"online\":") != NULL) {
        SDL_LockMutex(sLock);
        sOnline = number_value(answer, "online");
        SDL_UnlockMutex(sLock);
    } else if (!leave) {
        SDL_LockMutex(sLock);
        sOnline = -2;
        SDL_UnlockMutex(sLock);
    }
}

static int SDLCALL worker(void *unused) {
    Uint64 chatAt = 0, resultAt = 0, resultWait = 0; /* (a result the rival has not reported yet: sent again in 5 s,
                                                         doubling to a minute, not every round) */
    (void)unused;
    for (;;) {
        char answer[4096], extra[256], name[20], match[33], code[8], cancelled[33];
        int refresh, search, cancel, board, mode, chat, presence, leave, urgent, i;
        unsigned gen;
        Uint64 now = SDL_GetTicks();

        SDL_LockMutex(sLock);
        SDL_strlcpy(name, sName, sizeof(name));
        refresh = sRefresh;
        sRefresh = 0;
        search = sWantSearch;
        cancel = sWantCancel;
        sWantCancel = 0;
        board = sWantBoard;
        sWantBoard = 0;
        gen = sSearchGen;
        mode = sView.mode;
        SDL_strlcpy(cancelled, sCancelMatch, sizeof(cancelled));
        sCancelMatch[0] = '\0';
        SDL_strlcpy(match, sView.match, sizeof(match));
        SDL_strlcpy(code, sRoomPending, sizeof(code));
        sRoomPending[0] = '\0';
        chat = DRNET_CHAT && sChatOpen && (now - chatAt > 3000 || sChatOutN > 0);
        presence = (sPresenceOn && (sPresenceAt == 0 || now - sPresenceAt > 300000)) || sPresenceLeave;
        leave = sPresenceLeave;
        SDL_UnlockMutex(sLock);

        if (presence) {
            do_presence();
            if (!leave) {
                sPresenceAt = now;
            }
        }
        {
            char om[33], od[16];
            int os;
            urgent = outbox_first(om, &os, od) && now >= resultAt; /* (a result still to send) */
            if (!urgent && resultAt != 0 && !outbox_first(om, &os, od)) {
                resultAt = resultWait = 0;
            }
        }
        if ((refresh || search || cancel || board || chat || urgent) && identity()) {
            int pendingRefresh = refresh;
            if (urgent || refresh) {
                char om[33], od[16];
                int os;
                do_result(&pendingRefresh);
                if (outbox_first(om, &os, od)) {
                    resultWait = resultWait == 0 ? 5000 : resultWait * 2 > 60000 ? 60000 : resultWait * 2;
                    resultAt = SDL_GetTicks() + resultWait;
                } else {
                    resultAt = resultWait = 0;
                }
            }
            if (cancel && is_hex(cancelled, 32)) {
                SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\"", cancelled);
                post_ranked("cancel", extra, answer, sizeof(answer));
            }
            if (pendingRefresh) {
                SDL_snprintf(extra, sizeof(extra), ",\"name\":\"%s\"", name);
                if (post_ranked("profile", extra, answer, sizeof(answer))) {
                    SDL_LockMutex(sLock);
                    sView.rating = number_value(answer, "rating");
                    sView.wins = number_value(answer, "wins");
                    sView.losses = number_value(answer, "losses");
                    sView.draws = number_value(answer, "draws");
                    sView.position = number_value(answer, "position");
                    text_value(answer, "rank", sView.rank, sizeof(sView.rank));
                    sView.profile = 1;
                    if (!sView.searching && sView.state < 0 && sView.msg != DRMSG_VERSION) {
                        sView.state = 0;
                        sView.msg = DRMSG_UPDATED;
                    }
                    SDL_UnlockMutex(sLock);
                } else if (sView.msg == DRMSG_LIMIT) {
                    message(DRMSG_LIMIT, -1); /* (said on the screen as the server being busy, not as a failure) */
                } else if (sView.msg != DRMSG_VERSION) {
                    message(DRMSG_PROFILE_FAILED, -1);
                }
            }
            if (board) {
                int ok = post_ranked("leaderboard", "", answer, sizeof(answer));
                SDL_LockMutex(sLock);
                if (ok) {
                    const char *row = answer;
                    sView.boardCount = 0;
                    for (i = 0; i < 9; i++) {
                        row = strstr(row, "\"name\":");
                        if (row == NULL) {
                            break;
                        }
                        text_value(row, "name", sView.boardName[i], sizeof(sView.boardName[i]));
                        sView.boardRating[i] = number_value(row, "rating");
                        sView.boardCount++;
                        row++;
                    }
                }
                sView.board = ok ? 2 : -1;
                SDL_UnlockMutex(sLock);
            }
            if (search) {
                if (code[0] != '\0' && is_hex(match, 32)) {
                    SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"code\":\"%s\"", match, code);
                    if (post_ranked("room", extra, answer, sizeof(answer))) {
                        update_match(answer, gen);
                    } else {
                        message(DRMSG_ROOM_FAILED, -1);
                    }
                } else {
                    /* (one fight per match: rematches 0. DBT-NX's rounds can come later, when both games score them) */
                    SDL_snprintf(extra, sizeof(extra), ",\"rematches\":0,\"rules\":\"%s\"", mode ? "team3-inf-v1" : "solo-inf-v1");
                    if (post_ranked("queue", extra, answer, sizeof(answer))) {
                        update_match(answer, gen);
                    } else if (sView.msg != DRMSG_VERSION && sView.msg != DRMSG_LIMIT) {
                        message(strstr(answer, "busy") != NULL ? DRMSG_BUSY : DRMSG_NO_ANSWER, 99);
                    }
                }
            }
            if (chat) {
                chatAt = SDL_GetTicks();
                do_chat();
            }
        } else if (chat) {
            chatAt = SDL_GetTicks();
            SDL_LockMutex(sLock);
            sChat.state = -1;
            SDL_UnlockMutex(sLock);
        }
        /* The next round: at once when asked for something; a search asks the queue every 4 s in its first half
           minute, every 8 s to two minutes, then every 15 s (nobody there: the service's requests are a daily
           allowance shared by every player); paired, every 3 s (the room code is coming). Nothing searched: the
           loop only looks at what the window asked for, with no request. */
        {
            Uint64 waited = SDL_GetTicks() - sSearchAt;
            int paired, rounds;
            SDL_LockMutex(sLock);
            paired = sView.state == 3;
            SDL_UnlockMutex(sLock);
            rounds = !search ? 100 : paired ? 150 : waited < 30000 ? 200 : waited < 120000 ? 400 : 750;
            for (i = 0; i < rounds; i++) {
                SDL_LockMutex(sLock);
                urgent = sRefresh || sWantCancel || sWantBoard || sRoomPending[0] != '\0' || sChatOutN > 0 || sPresenceLeave;
                SDL_UnlockMutex(sLock);
                if (urgent) {
                    break;
                }
                SDL_Delay(20);
            }
        }
    }
    return 0;
}

static void start(const char *name) {
    lock_make();
    SDL_LockMutex(sLock);
    if (name != NULL) {
        clean_name(name, sName, sizeof(sName));
    }
    SDL_UnlockMutex(sLock);
    if (SDL_CompareAndSwapAtomicInt(&sStarted, 0, 1)) {
        SDL_Thread *t = SDL_CreateThread(worker, "dragonnet", NULL);
        if (t != NULL) {
            SDL_DetachThread(t);
        } else {
            SDL_SetAtomicInt(&sStarted, 0);
        }
    }
}

/* ---- the window's side ---- */

void DrNet_PresenceActive(int on) {
    start(NULL);
    SDL_LockMutex(sLock);
    if (on && !sPresenceOn) {
        sPresenceAt = 0;
    }
    if (!on && sPresenceOn) {
        sPresenceLeave = 1;
    }
    sPresenceOn = on != 0;
    SDL_UnlockMutex(sLock);
}

int DrNet_Online(void) {
    int n;
    lock_make();
    SDL_LockMutex(sLock);
    n = sOnline;
    SDL_UnlockMutex(sLock);
    return n;
}

void DrRanked_Open(const char *name) {
    start(name);
    SDL_LockMutex(sLock);
    sRefresh = 1;
    SDL_UnlockMutex(sLock);
}

void DrRanked_Search(const char *name, int mode) {
    char m[33], d[9];
    int s;
    start(name);
    if (outbox_first(m, &s, d)) {
        message(DRMSG_FINISH_FIRST, -1);
        return;
    }
    SDL_LockMutex(sLock);
    sSearchGen++;
    sHandedOver[0] = '\0';
    sView.mode = mode != 0;
    sWantSearch = sView.searching = 1;
    sSearchAt = SDL_GetTicks();
    sView.state = 1;
    sView.match[0] = sView.code[0] = sView.peer[0] = sRoomPending[0] = '\0';
    sView.msg = DRMSG_SEARCHING;
    SDL_UnlockMutex(sLock);
}

void DrRanked_Cancel(void) {
    lock_make();
    SDL_LockMutex(sLock);
    if (sHandedOver[0] != '\0' || (!sWantSearch && !sView.searching)) {
        SDL_UnlockMutex(sLock);
        return;
    }
    SDL_strlcpy(sCancelMatch, sView.match, sizeof(sCancelMatch));
    sSearchGen++;
    sWantSearch = sView.searching = 0;
    sView.state = 0;
    sView.msg = DRMSG_NONE;
    sView.match[0] = sView.code[0] = sRoomPending[0] = '\0';
    sWantCancel = 1;
    SDL_UnlockMutex(sLock);
}

void DrRanked_Board(void) {
    start(NULL);
    SDL_LockMutex(sLock);
    sWantBoard = 1;
    sView.board = 1;
    SDL_UnlockMutex(sLock);
}

void DrRanked_Poll(DrRanked *out) {
    static Uint64 lastRefresh;
    lock_make();
    SDL_LockMutex(sLock);
    *out = sView;
    if (sView.profile && !sView.searching && SDL_GetTicks() - lastRefresh > 60000) { /* (the profile kept fresh while shown) */
        lastRefresh = SDL_GetTicks();
        sRefresh = 1;
    }
    SDL_UnlockMutex(sLock);
}

void DrRanked_SetRoom(const char *code) {
    lock_make();
    SDL_LockMutex(sLock);
    if (sWantSearch && sView.role == 1 && strcmp(code, sView.code) != 0) {
        SDL_strlcpy(sRoomPending, code, sizeof(sRoomPending));
    }
    SDL_UnlockMutex(sLock);
}

int DrRanked_Handover(void) {
    DrRanked v;
    DrRanked_Poll(&v);
    if (v.state != 3 || !is_hex(v.match, 32) || (v.role != 1 && v.role != 2)) {
        return 0;
    }
    session_write(v.match, v.role, v.mode, 0);
    SDL_LockMutex(sLock);
    SDL_strlcpy(sHandedOver, v.match, sizeof(sHandedOver));
    sSearchGen++;
    sWantSearch = sView.searching = 0;
    SDL_UnlockMutex(sLock);
    fprintf(stderr, "dragonnet: ranked match handed over (role %d)\n", v.role);
    return 1;
}

int DrRanked_InSession(void) {
    char match[33];
    int role, mode, sent;
    return session_read(match, &role, &mode, &sent);
}

/* The fight is over (BattleResult_Finish): its result, as both games see it, for the service. The host is side 0.
   The digest is DBT-NX's (FNV-1a over the flags, the reason and the battle clock's ticks): the two reports of one
   fight are the same unless the two games disagreed about it. */
void DrNet_BattleFinished(int flags, int reason, int clock) {
    char match[33], line[128], digest[9];
    int role, mode, sent, score, i;
    uint32_t words[3], hash = 2166136261u;
    if (gPortResim || !Port_NetActive() || !session_read(match, &role, &mode, &sent) || sent || (flags & 8)) {
        return;
    }
    score = (flags & 3) == 1 ? 2 : (flags & 3) == 2 ? 0 : (flags & 4) ? 1 : -1;
    if (score < 0) {
        return;
    }
    words[0] = (uint32_t)flags;
    words[1] = (uint32_t)reason;
    words[2] = (uint32_t)clock;
    for (i = 0; i < (int)sizeof(words); i++) {
        hash = (hash ^ ((unsigned char *)words)[i]) * 16777619u;
    }
    SDL_snprintf(digest, sizeof(digest), "%08x", hash);
    SDL_snprintf(line, sizeof(line), "%s %d %s 1\n", match, score, digest);
    if (!outbox_push(line)) {
        fprintf(stderr, "dragonnet: the ranked result could not be saved\n");
        return;
    }
    session_write(match, role, mode, 1);
    fprintf(stderr, "dragonnet: ranked result saved: score %d (host's view), digest %s\n", score, digest);
    start(NULL); /* (sent by the thread, now or the next time the online screen is open) */
}

/* The online session ends. `mine`: this player left (the pause menu, the game closed); 0: the other side went away
   or the two games disagreed. Leaving a ranked match before its fight was over is this player's loss. */
void DrNet_SessionEnding(int mine) {
    char match[33], line[128];
    int role, mode, sent;
    if (!session_read(match, &role, &mode, &sent)) {
        return;
    }
    if (!sent && mine) {
        SDL_snprintf(line, sizeof(line), "%s -1 00000000 1\n", match);
        outbox_push(line);
        fprintf(stderr, "dragonnet: left the ranked match before its end: a loss\n");
    }
    remove_file("dragonnet-session.txt");
}

void DrChat_Open(const char *name) {
    (void)name;
    lock_make();
    SDL_LockMutex(sLock);
    sChat.state = -1; /* chat desligado */
    SDL_UnlockMutex(sLock);
}

void DrChat_Close(void) {
    lock_make();
    SDL_LockMutex(sLock);
    sChatOpen = 0;
    SDL_UnlockMutex(sLock);
}

void DrChat_Send(const char *text) {
    char clean[200];
    size_t k = 0;
    lock_make();
    for (; *text != '\0' && k + 1 < sizeof(clean) && k < 160; text++) {
        clean[k++] = (unsigned char)*text < 32 ? ' ' : *text;
    }
    clean[k] = '\0';
    if (k == 0) {
        return;
    }
    SDL_LockMutex(sLock);
    if (sChatOutN < 4) {
        SDL_strlcpy(sChatOut[sChatOutN++], clean, sizeof(sChatOut[0]));
    }
    sChat.sending = sChatOutN;
    SDL_UnlockMutex(sLock);
}

void DrChat_Poll(DrChat *out) {
    lock_make();
    SDL_LockMutex(sLock);
    *out = sChat;
    SDL_UnlockMutex(sLock);
}

int DrRanked_Active(void) {
    int a;
    lock_make();
    SDL_LockMutex(sLock);
    a = sView.searching;
    SDL_UnlockMutex(sLock);
    return a;
}
