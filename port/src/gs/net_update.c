/*
 * The automatic update: from a GitHub release to the new version running. Based on PR #107 by JosephLlacchua (the
 * in-game download with its progress, miniz to unpack, the rename trick, the restart), with the player's choice of
 * when, and the checks, on top.
 *
 *   1. A few seconds after start (gs/ui.cpp, unless "Check for updates" is off) one GET of the repository's newest
 *      published release (api.github.com .../releases/latest; drafts and pre-releases are not it). Its tag is compared
 *      with this build's full number (PORT_VERSION, generated from the release workflow's tag: "0.1.38.9"; a local
 *      build is "dev" and is never offered anything). Nothing is sent but the request itself.
 *   2. Newer: the main menu and the online screens say so; F1 has Update now, Later and What's new (the release's
 *      notes). Update now downloads this system's zip into update/ beside the program, in a thread, while the game
 *      goes on; no new online session starts meanwhile (Update_BlocksOnline).
 *   3. The zip is checked: its size and SHA-256 against what GitHub lists for it, and its Ed25519 signature (the
 *      .sig beside it in the release, made by the release workflow with the private key) against the public key
 *      compiled in (port/src/update_key.h; Monocypher, gs/update_crypto.c). A release without a .sig, or a build
 *      without a key, is only announced (with its page); a wrong signature: nothing is installed, and the game says
 *      so and shows the release page.
 *   4. Restart now (the player's choice, never during an online session) installs it, in two steps, with the
 *      program's own folder as the base of every path (not the working folder: a shortcut can start the game from
 *      anywhere):
 *        a. every file of the zip is written beside its place as <name>.new (folders made as needed). The player's
 *           own files are never written, even if a zip had them (user_file: saves, settings, gamedata, mods, texture
 *           packs, songs, stages, logs...); fonts/fonts.tsv takes the new list with the player's own lines added
 *           back. A failure here: the .new files go, nothing else was touched.
 *        b. each file in place is renamed to <name>.old and its .new to its name. A running program and its loaded
 *           libraries can be renamed (Windows does not let them be written or deleted, but renamed, yes): that is
 *           the trick that lets the game replace itself. A failure here: every swap done so far is undone (the
 *           .old back to its name), and the old version goes on as it was.
 *      Then update/result.txt says the new number, the new program is started and this one closes. The new one says
 *      "Updated to 0.1.38.10" and removes the .old files (Update_Cleanup, by the list in update/cleanup.txt).
 *      Nothing is ever deleted but those .old files and the update's own downloads.
 *
 * HTTPS as the rest of the port has it (gs/net_match.c): WinHTTP on Windows, the system's curl library on Linux, and
 * failing those the `curl` program (BT3_MATCH_CURL=1 asks for the program).
 * Testing: BT3_UPDATE_API=<http://host:port> asks there instead of https://api.github.com (a local stand-in that
 * serves .../releases/latest and the files it names); BT3_UPDATE_FAIL_AT=<n> makes the n-th swap of step b fail.
 */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "net_update.h"
#include "update_sign.h"
#include "port_version.h" /* (generated: PORT_VERSION) */
#include "../port_release.h"
#include "../update_key.h"

int UpdCrypto_Check(const uint8_t sig[64], const uint8_t pub[32], const uint8_t *msg, size_t size); /* update_crypto.c */

/* miniz (port/third_party/miniz, MIT): only its reading of zips, from memory */
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "../../third_party/miniz/miniz.h"
#include "../../third_party/miniz/miniz.c"

#define GAME "Tenkaichi3Decomp"
#ifdef _WIN32
#define ASSET GAME "-windows-x64.zip"
#define GAME_EXE GAME ".exe"
#else
#define ASSET GAME "-linux-x64.zip"
#define GAME_EXE GAME
#endif
#define JSON_MAX (4 << 20) /* the release's description, notes and list of files */
#define GAME_DAT GAME ".dat"
#define SIG_MAX 4096

typedef struct {
    char url[1024];
    long long size;
    char sha[65]; /* "" when GitHub did not say */
} Asset;

static struct {
    SDL_Mutex *lock;
    SDL_AtomicInt state, busy;
    char latest[32], page[256], why[512], cannot[256];
    char *notes;
    Asset zip, sig;
    int canInstall;
    char root[1024], work[1100], part[1200], file[1200], sha[65];
    long long got;
    Uint64 gotAt;
} sU;

const char *Update_Version(void) {
    return PORT_VERSION;
}

static void lock(void) {
    if (sU.lock == NULL) {
        sU.lock = SDL_CreateMutex();
    }
    SDL_LockMutex(sU.lock);
}

static void unlock(void) {
    SDL_UnlockMutex(sU.lock);
}

static void fail(const char *why) {
    lock();
    SDL_strlcpy(sU.why, why, sizeof(sU.why));
    unlock();
    fprintf(stderr, "bt3: update: %s\n", why);
    SDL_SetAtomicInt(&sU.state, UPD_FAILED);
}

static const char *api_base(void) {
    const char *b = SDL_getenv("BT3_UPDATE_API");
    return b != NULL && b[0] != '\0' ? b : "https://api.github.com";
}

/* ---- HTTP GET, into memory or into a file ---- */
typedef struct {
    SDL_IOStream *io; /* a file, or */
    char *buf;        /* memory (NUL-terminated), at most `max` bytes */
    size_t len, cap, max;
    int bad;
} Sink;

static int sink_put(Sink *s, const void *data, size_t n) {
    if (s->bad) {
        return 0;
    }
    if (s->io != NULL) {
        if (SDL_WriteIO(s->io, data, n) != n) {
            s->bad = 1;
        }
        return !s->bad;
    }
    if (s->len + n + 1 > s->max) {
        s->bad = 1;
        return 0;
    }
    if (s->len + n + 1 > s->cap) {
        size_t cap = s->cap ? s->cap : 65536;
        char *b;
        while (cap < s->len + n + 1) {
            cap *= 2;
        }
        b = (char *)SDL_realloc(s->buf, cap);
        if (b == NULL) {
            s->bad = 1;
            return 0;
        }
        s->buf = b;
        s->cap = cap;
    }
    memcpy(s->buf + s->len, data, n);
    s->len += n;
    s->buf[s->len] = '\0';
    return 1;
}

#ifdef _WIN32
/* Windows: WinHTTP (redirects, as GitHub's downloads make, are followed by itself). */
static int http_native(const char *url, int json, Sink *s) {
    wchar_t *wurl = NULL, host[256], path[2048], extra[2048], full[4096];
    URL_COMPONENTS uc;
    HINTERNET ses = NULL, con = NULL, req = NULL;
    DWORD status = 0, len = sizeof(status), got = 0;
    int n = MultiByteToWideChar(CP_UTF8, 0, url, -1, NULL, 0), ok = 0;
    static char chunk[65536];

    if (n <= 0 || (wurl = (wchar_t *)SDL_malloc(sizeof(wchar_t) * (size_t)n)) == NULL) {
        return 0;
    }
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, n);
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = 2048;
    if (WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        _snwprintf(full, 4096, L"%ls%ls", path, extra);
        full[4095] = 0;
        ses = WinHttpOpen(L"Tenkaichi3Decomp", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (ses != NULL) {
        WinHttpSetTimeouts(ses, 15000, 15000, 30000, 60000);
        con = WinHttpConnect(ses, host, uc.nPort, 0);
    }
    if (con != NULL) {
        req = WinHttpOpenRequest(con, L"GET", full, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    }
    if (req != NULL &&
        WinHttpSendRequest(req, json ? L"Accept: application/vnd.github+json" : WINHTTP_NO_ADDITIONAL_HEADERS, json ? (DWORD)-1 : 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, NULL)) {
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            ok = 1;
            for (;;) {
                if (!WinHttpReadData(req, chunk, sizeof(chunk), &got)) {
                    ok = 0; /* (cut off) */
                    break;
                }
                if (got == 0) {
                    break;
                }
                if (!sink_put(s, chunk, got)) {
                    ok = 0;
                    break;
                }
            }
        } else {
            fprintf(stderr, "bt3: update: %s answered %lu\n", url, (unsigned long)status);
        }
    }
    if (req != NULL) { WinHttpCloseHandle(req); }
    if (con != NULL) { WinHttpCloseHandle(con); }
    if (ses != NULL) { WinHttpCloseHandle(ses); }
    SDL_free(wurl);
    return ok && !s->bad;
}
#else
/* Linux: the system's curl library, loaded when first needed (as gs/net_match.c does). */
static size_t curl_write(char *data, size_t size, size_t count, void *user) {
    return sink_put((Sink *)user, data, size * count) ? size * count : 0;
}

static int http_native(const char *url, int json, Sink *s) {
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
    int rc;

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
    if (init == NULL || setopt == NULL || perform == NULL || cleanup == NULL || append == NULL || freeall == NULL || (h = init()) == NULL) {
        return 0;
    }
    setopt(h, 10002 /* CURLOPT_URL */, url);
    setopt(h, 10018 /* CURLOPT_USERAGENT */, "Tenkaichi3Decomp");
    setopt(h, 52 /* CURLOPT_FOLLOWLOCATION */, 1L);
    setopt(h, 45 /* CURLOPT_FAILONERROR */, 1L);
    setopt(h, 99 /* CURLOPT_NOSIGNAL */, 1L);
    setopt(h, 78 /* CURLOPT_CONNECTTIMEOUT */, 20L);
    setopt(h, 19 /* CURLOPT_LOW_SPEED_LIMIT */, 100L); /* (a stalled download ends; a slow one does not) */
    setopt(h, 20 /* CURLOPT_LOW_SPEED_TIME */, 60L);
    if (json) {
        setopt(h, 13 /* CURLOPT_TIMEOUT */, 30L);
        list = append(NULL, "Accept: application/vnd.github+json");
        setopt(h, 10023 /* CURLOPT_HTTPHEADER */, list);
    }
    setopt(h, 20011 /* CURLOPT_WRITEFUNCTION */, curl_write);
    setopt(h, 10001 /* CURLOPT_WRITEDATA */, s);
    rc = perform(h);
    cleanup(h);
    if (list != NULL) {
        freeall(list);
    }
    if (rc != 0) {
        fprintf(stderr, "bt3: update: %s: curl error %d\n", url, rc);
    }
    return rc == 0 && !s->bad;
}
#endif

/* The `curl` program (Windows 10 and later have curl.exe): into the file `path`, or its output into memory. */
static int http_program(const char *url, int json, const char *path, Sink *s) {
    const char *args[24];
    int n = 0, code = -1;
    SDL_Process *p;
#ifdef _WIN32
    args[n++] = "curl.exe";
#else
    args[n++] = "curl";
#endif
    args[n++] = "-sfL";
    args[n++] = "--connect-timeout";
    args[n++] = "20";
    args[n++] = "-y";
    args[n++] = "60";
    args[n++] = "-Y";
    args[n++] = "100";
    args[n++] = "-A";
    args[n++] = "Tenkaichi3Decomp";
    if (json) {
        args[n++] = "-H";
        args[n++] = "Accept: application/vnd.github+json";
    }
    if (path != NULL) {
        args[n++] = "-o";
        args[n++] = path;
    }
    args[n++] = url;
    args[n] = NULL;
    p = SDL_CreateProcess(args, path == NULL);
    if (p == NULL) {
        return 0;
    }
    if (path == NULL) {
        size_t got = 0;
        void *data = SDL_ReadProcess(p, &got, &code);
        if (data != NULL) {
            if (code != 0 || !sink_put(s, data, got)) {
                code = code != 0 ? code : -1;
            }
            SDL_free(data);
        }
    } else {
        SDL_WaitProcess(p, true, &code);
    }
    SDL_DestroyProcess(p);
    return code == 0;
}

/* GET `url` into the file `path`, or (path NULL) into *mem (NUL-terminated, SDL_free it; at most `max` bytes). */
static int http_get(const char *url, int json, const char *path, char **mem, size_t *memLen, size_t max) {
    Sink s;
    int ok = 0;

    memset(&s, 0, sizeof(s));
    s.max = max;
    if (SDL_getenv("BT3_MATCH_CURL") == NULL) {
        if (path != NULL) {
            s.io = SDL_IOFromFile(path, "wb");
        }
        if (path == NULL || s.io != NULL) {
            ok = http_native(url, json, &s);
        }
        if (s.io != NULL && !SDL_CloseIO(s.io)) {
            ok = 0;
        }
        s.io = NULL;
    }
    if (!ok) {
        SDL_free(s.buf);
        memset(&s, 0, sizeof(s));
        s.max = max;
        ok = http_program(url, json, path, &s);
    }
    if (ok && path == NULL) {
        if (s.buf == NULL) {
            s.buf = (char *)SDL_calloc(1, 1);
        }
        *mem = s.buf;
        if (memLen != NULL) {
            *memLen = s.len;
        }
        return *mem != NULL;
    }
    SDL_free(s.buf);
    return ok;
}

/* ---- just enough JSON for GitHub's answer ---- */
static const char *js_ws(const char *p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
        p++;
    }
    return p;
}

static const char *js_skip(const char *p) { /* past one value; NULL if it is broken */
    p = js_ws(p);
    if (*p == '"') {
        for (p++; *p != '"'; p++) {
            if (*p == '\0') {
                return NULL;
            }
            if (*p == '\\' && p[1] != '\0') {
                p++;
            }
        }
        return p + 1;
    }
    if (*p == '{' || *p == '[') {
        char close = *p == '{' ? '}' : ']';
        p = js_ws(p + 1);
        if (*p == close) {
            return p + 1;
        }
        for (;;) {
            if (close == '}') {
                p = js_skip(p); /* the key */
                if (p == NULL || *(p = js_ws(p)) != ':') {
                    return NULL;
                }
                p++;
            }
            p = js_skip(p);
            if (p == NULL) {
                return NULL;
            }
            p = js_ws(p);
            if (*p == ',') {
                p = js_ws(p + 1);
            } else if (*p == close) {
                return p + 1;
            } else {
                return NULL;
            }
        }
    }
    if (*p == '\0') {
        return NULL;
    }
    while (*p != '\0' && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
        p++;
    }
    return p;
}

/* The value of `key` in the object at `obj` (its '{'), NULL if it has none. */
static const char *js_get(const char *obj, const char *key) {
    const char *p = js_ws(obj);
    size_t klen = strlen(key);
    if (*p != '{') {
        return NULL;
    }
    p = js_ws(p + 1);
    while (*p == '"') {
        const char *k = p + 1, *end = js_skip(p);
        if (end == NULL || *(end = js_ws(end)) != ':') {
            return NULL;
        }
        p = js_ws(end + 1);
        if ((size_t)(end - k) >= klen + 1 && strncmp(k, key, klen) == 0 && k[klen] == '"') {
            return p;
        }
        p = js_skip(p);
        if (p == NULL) {
            return NULL;
        }
        p = js_ws(p);
        if (*p != ',') {
            return NULL;
        }
        p = js_ws(p + 1);
    }
    return NULL;
}

static void put_utf8(char *out, size_t size, size_t *n, unsigned c) {
    char b[4];
    int k = 0, i;
    if (c < 0x80) {
        b[k++] = (char)c;
    } else if (c < 0x800) {
        b[k++] = (char)(0xC0 | c >> 6);
        b[k++] = (char)(0x80 | (c & 63));
    } else if (c < 0x10000) {
        b[k++] = (char)(0xE0 | c >> 12);
        b[k++] = (char)(0x80 | (c >> 6 & 63));
        b[k++] = (char)(0x80 | (c & 63));
    } else {
        b[k++] = (char)(0xF0 | c >> 18);
        b[k++] = (char)(0x80 | (c >> 12 & 63));
        b[k++] = (char)(0x80 | (c >> 6 & 63));
        b[k++] = (char)(0x80 | (c & 63));
    }
    if (*n + (size_t)k + 1 > size) {
        return;
    }
    for (i = 0; i < k; i++) {
        out[(*n)++] = b[i];
    }
}

static unsigned hex4(const char *p) {
    unsigned v = 0;
    int i;
    for (i = 0; i < 4; i++) {
        char c = p[i];
        v = v * 16 + (unsigned)(c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0);
        if (c == '\0') {
            return 0xFFFD;
        }
    }
    return v;
}

/* The string at `p` (its '"') into out, its escapes undone. 0 if `p` is not a string. */
static int js_string(const char *p, char *out, size_t size) {
    size_t n = 0;
    if (p == NULL || *(p = js_ws(p)) != '"' || size == 0) {
        return 0;
    }
    for (p++; *p != '"' && *p != '\0'; p++) {
        unsigned c = (unsigned char)*p;
        if (c == '\\') {
            p++;
            switch (*p) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'u':
                c = hex4(p + 1);
                p += 4;
                if (c >= 0xD800 && c < 0xDC00 && p[1] == '\\' && p[2] == 'u') {
                    unsigned lo = hex4(p + 3);
                    if (lo >= 0xDC00 && lo < 0xE000) {
                        c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                        p += 6;
                    }
                }
                if (c >= 0xD800 && c < 0xE000) {
                    c = 0xFFFD;
                }
                put_utf8(out, size, &n, c);
                continue;
            case '\0': p--; continue;
            default: c = (unsigned char)*p; break; /* \" \\ \/ */
            }
        }
        if (n + 1 < size) {
            out[n++] = (char)c;
        }
    }
    out[n] = '\0';
    return 1;
}

/* One of the release's files, by name: its address, size and SHA-256 ("digest": "sha256:..."). */
static int js_asset(const char *json, const char *name, Asset *out) {
    const char *list = js_get(json, "assets"), *p;
    char text[256];

    memset(out, 0, sizeof(*out));
    if (list == NULL || *(p = js_ws(list)) != '[') {
        return 0;
    }
    p = js_ws(p + 1);
    while (*p == '{') {
        if (js_string(js_get(p, "name"), text, sizeof(text)) && strcmp(text, name) == 0) {
            const char *size = js_get(p, "size");
            if (!js_string(js_get(p, "browser_download_url"), out->url, sizeof(out->url)) || size == NULL) {
                return 0;
            }
            out->size = SDL_strtoll(size, NULL, 10);
            if (js_string(js_get(p, "digest"), text, sizeof(text)) && strncmp(text, "sha256:", 7) == 0 && strlen(text) == 71) {
                SDL_strlcpy(out->sha, text + 7, sizeof(out->sha));
            }
            return 1;
        }
        p = js_skip(p);
        if (p == NULL) {
            return 0;
        }
        p = js_ws(p);
        if (*p != ',') {
            break;
        }
        p = js_ws(p + 1);
    }
    return 0;
}

/* ---- the folder ---- */
static int key_set(uint8_t key[32]) { /* the public key compiled in (all zeros: none) */
    int i, any = 0;
    if (!Upd_HexBytes(PORT_UPDATE_KEY, key, 32)) {
        return 0;
    }
    for (i = 0; i < 32; i++) {
        any |= key[i];
    }
    return any != 0;
}

static int is_file(const char *path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

/* Can the game install an update here: a release's folder (the program and its data list), one it may write to. */
static int folder_ready(char *why, size_t size) {
    char path[1300];
    SDL_IOStream *io;

    SDL_snprintf(path, sizeof(path), "%s" GAME_DAT, sU.root);
    if (!is_file(path)) {
        SDL_snprintf(why, size, "This copy is not a release's folder (no " GAME_DAT " beside the program): update it by hand.");
        return 0;
    }
    SDL_snprintf(path, sizeof(path), "%s" GAME_EXE, sU.root);
    if (!is_file(path)) {
        SDL_snprintf(why, size, "The program is not named " GAME_EXE ": update it by hand.");
        return 0;
    }
    SDL_CreateDirectory(sU.work);
    SDL_snprintf(path, sizeof(path), "%swrite-test.tmp", sU.work);
    io = SDL_IOFromFile(path, "wb");
    if (io == NULL) {
        SDL_snprintf(why, size, "The game's folder cannot be written to (%s): update it by hand.", sU.root);
        return 0;
    }
    SDL_CloseIO(io);
    SDL_RemovePath(path);
    return 1;
}

/* ---- the check ---- */
static int SDLCALL check_thread(void *arg) {
    char url[512], tag[64], cannot[256] = "";
    char *json = NULL, *notes;
    size_t len = 0;
    int cmp, can;
    uint8_t key[32];
    Asset zip, sig;

    (void)arg;
    SDL_snprintf(url, sizeof(url), "%s/repos/" PORT_UPDATE_REPO "/releases/latest", api_base());
    if (!http_get(url, 1, NULL, &json, &len, JSON_MAX) || !js_string(js_get(json, "tag_name"), tag, sizeof(tag))) {
        fprintf(stderr, "bt3: update check: no answer\n");
        SDL_free(json);
        SDL_SetAtomicInt(&sU.state, UPD_NONE);
        return 0;
    }
    cmp = UpdateVer_Compare(PORT_VERSION, tag);
    fprintf(stderr, "bt3: update check: the newest release is %s, this is %s%s\n", tag, PORT_VERSION,
            cmp == -1 ? ": an update is available" : cmp == -2 ? " (no update is offered)" : "");
    if (cmp != -1) {
        SDL_free(json);
        SDL_SetAtomicInt(&sU.state, UPD_NONE);
        return 0;
    }
    notes = (char *)SDL_malloc(len + 1);
    if (notes == NULL || !js_string(js_get(json, "body"), notes, len + 1)) {
        if (notes != NULL) {
            notes[0] = '\0';
        }
    }
    js_asset(json, ASSET, &zip);
    js_asset(json, ASSET ".sig", &sig);
    can = 1;
#ifdef __APPLE__
    can = 0;
    SDL_strlcpy(cannot, "There is no release for macOS: build the new version from its source.", sizeof(cannot));
#endif
    if (can && !key_set(key)) {
        can = 0;
        SDL_strlcpy(cannot, "This build has no update key: it cannot install updates itself. Download the release by hand.", sizeof(cannot));
    } else if (can && zip.url[0] == '\0') {
        can = 0;
        SDL_strlcpy(cannot, "The release has no " ASSET ": download it by hand.", sizeof(cannot));
    } else if (can && sig.url[0] == '\0') {
        can = 0;
        SDL_strlcpy(cannot, "The release is not signed (no " ASSET ".sig), so the game will not install it. Download it by hand.", sizeof(cannot));
    } else if (can && !folder_ready(cannot, sizeof(cannot))) {
        can = 0;
    }
    if (!can) {
        fprintf(stderr, "bt3: update: %s\n", cannot);
    }
    lock();
    {
        const char *t = tag[0] == 'v' || tag[0] == 'V' ? tag + 1 : tag;
        SDL_strlcpy(sU.latest, t, sizeof(sU.latest));
    }
    if (!js_string(js_get(json, "html_url"), sU.page, sizeof(sU.page)) || SDL_strncmp(sU.page, "https://", 8) != 0) {
        SDL_snprintf(sU.page, sizeof(sU.page), "https://github.com/" PORT_UPDATE_REPO "/releases/latest");
    }
    SDL_free(sU.notes);
    sU.notes = notes;
    sU.zip = zip;
    sU.sig = sig;
    sU.canInstall = can;
    SDL_strlcpy(sU.cannot, cannot, sizeof(sU.cannot));
    unlock();
    SDL_free(json);
    SDL_SetAtomicInt(&sU.state, UPD_AVAILABLE);
    return 0;
}

static void folders(void) {
    if (sU.root[0] == '\0') {
        const char *base = SDL_GetBasePath();
        SDL_strlcpy(sU.root, base != NULL ? base : "./", sizeof(sU.root));
        SDL_snprintf(sU.work, sizeof(sU.work), "%supdate/", sU.root);
        SDL_snprintf(sU.part, sizeof(sU.part), "%s" ASSET ".part", sU.work);
        SDL_snprintf(sU.file, sizeof(sU.file), "%s" ASSET, sU.work);
    }
}

void Update_Check(void) {
    SDL_Thread *t;
    if (SDL_GetAtomicInt(&sU.state) != UPD_IDLE) {
        return;
    }
    folders();
    if (UpdateVer_Compare(PORT_VERSION, PORT_VERSION) == -2) { /* a "dev" build: nothing to compare with */
        fprintf(stderr, "bt3: update check: this is a development build (%s): no updates are offered\n", PORT_VERSION);
        SDL_SetAtomicInt(&sU.state, UPD_NONE);
        return;
    }
    SDL_SetAtomicInt(&sU.state, UPD_CHECKING);
    t = SDL_CreateThread(check_thread, "bt3-update", NULL);
    if (t == NULL) {
        SDL_SetAtomicInt(&sU.state, UPD_NONE);
    } else {
        SDL_DetachThread(t);
    }
}

/* ---- the download and its checks ---- */
static int SDLCALL download_thread(void *arg) {
    char why[512], hex[65], *sigText = NULL;
    uint8_t sig[64], key[32];
    size_t size = 0;
    void *data;
    SDL_PathInfo info;
    Asset zip, sigAsset;

    (void)arg;
    lock();
    zip = sU.zip;
    sigAsset = sU.sig;
    unlock();
    SDL_CreateDirectory(sU.work);
    SDL_RemovePath(sU.part);
    SDL_RemovePath(sU.file);
    fprintf(stderr, "bt3: update: downloading %s\n", zip.url);
    if (!http_get(sigAsset.url, 0, NULL, &sigText, NULL, SIG_MAX) || !Upd_HexBytes(js_ws(sigText), sig, 64)) {
        SDL_free(sigText);
        fail("The release's signature (" ASSET ".sig) could not be downloaded or read. Try again later.");
        return 0;
    }
    SDL_free(sigText);
    sU.got = 0;
    if (!http_get(zip.url, 0, sU.part, NULL, NULL, 0)) {
        SDL_RemovePath(sU.part);
        fail("The download failed (no connection, or it was cut off). Try again later.");
        return 0;
    }
    SDL_SetAtomicInt(&sU.state, UPD_VERIFYING);
    if (!SDL_GetPathInfo(sU.part, &info) || (long long)info.size != zip.size) {
        SDL_snprintf(why, sizeof(why), "The download is incomplete (%lld of %lld bytes). Try again later.", (long long)info.size, zip.size);
        SDL_RemovePath(sU.part);
        fail(why);
        return 0;
    }
    data = SDL_LoadFile(sU.part, &size);
    if (data == NULL) {
        SDL_RemovePath(sU.part);
        fail("The downloaded archive could not be read back.");
        return 0;
    }
    UpdSha256_Hex(data, size, hex);
    if (zip.sha[0] != '\0' && SDL_strcasecmp(hex, zip.sha) != 0) {
        SDL_free(data);
        SDL_RemovePath(sU.part);
        fail("The download is damaged: its SHA-256 is not the one GitHub lists for it. Try again later.");
        return 0;
    }
    if (!key_set(key) || !UpdCrypto_Check(sig, key, (const uint8_t *)data, size)) {
        SDL_free(data);
        SDL_RemovePath(sU.part);
        fail("The archive's signature is NOT valid: it was not signed with this game's release key, so nothing was "
             "installed. Download the release from its page only if you trust where it comes from.");
        return 0;
    }
    SDL_free(data);
    if (!SDL_RenamePath(sU.part, sU.file)) {
        SDL_RemovePath(sU.part);
        fail("The downloaded archive could not be kept in the update folder.");
        return 0;
    }
    lock();
    SDL_strlcpy(sU.sha, hex, sizeof(sU.sha));
    unlock();
    fprintf(stderr, "bt3: update: %s downloaded and verified (signature good, sha256 %s)\n", ASSET, hex);
    SDL_SetAtomicInt(&sU.state, UPD_READY);
    return 0;
}

void Update_Download(void) {
    SDL_Thread *t;
    int st = SDL_GetAtomicInt(&sU.state);
    if ((st != UPD_AVAILABLE && st != UPD_FAILED) || !sU.canInstall) {
        return;
    }
    folders();
    SDL_SetAtomicInt(&sU.state, UPD_DOWNLOADING);
    t = SDL_CreateThread(download_thread, "bt3-update-get", NULL);
    if (t == NULL) {
        fail("The download could not be started.");
    } else {
        SDL_DetachThread(t);
    }
}

void Update_Status(UpdateStatus *out) {
    memset(out, 0, sizeof(*out));
    out->current = PORT_VERSION;
    out->state = SDL_GetAtomicInt(&sU.state);
    if (out->state < UPD_AVAILABLE) {
        return;
    }
    lock();
    SDL_strlcpy(out->latest, sU.latest, sizeof(out->latest));
    SDL_strlcpy(out->page, sU.page, sizeof(out->page));
    SDL_strlcpy(out->why, sU.why, sizeof(out->why));
    SDL_strlcpy(out->cannot, sU.cannot, sizeof(out->cannot));
    out->canInstall = sU.canInstall;
    out->total = sU.zip.size;
    unlock();
    if (out->state == UPD_DOWNLOADING) { /* how far: the file's size (whichever way it is downloaded), now and then */
        Uint64 now = SDL_GetTicks();
        if (now - sU.gotAt >= 250) {
            SDL_PathInfo info;
            sU.gotAt = now;
            sU.got = SDL_GetPathInfo(sU.part, &info) ? (long long)info.size : 0;
        }
        out->got = sU.got;
    } else if (out->state >= UPD_VERIFYING) {
        out->got = out->total;
    }
}

const char *Update_Notes(void) {
    return sU.notes != NULL ? sU.notes : "";
}

int Update_BlocksOnline(void) {
    int st = SDL_GetAtomicInt(&sU.state);
    return st == UPD_DOWNLOADING || st == UPD_VERIFYING || st == UPD_READY || st == UPD_INSTALLING || st == UPD_INSTALLED;
}

const char *Update_OnlineNote(void) {
    return SDL_GetAtomicInt(&sU.state) >= UPD_READY ? "An update is ready: online play is off until it is installed (F1 > Restart now)."
                                                     : "An update is being downloaded: online play is off until it is installed.";
}

/* ---- the install: the rename trick ---- */

/* The player's own files and folders, never written by an update even if a zip had them (a release ships none of
   them; the list is the guarantee). `rel` is the path in the game's folder, '/' between its parts. */
static int user_file(const char *rel) {
    static const char *const dirs[] = {
        "saves/",       /* the memory cards */
        "gamedata/",    /* the game's data unpacked from the player's disc; its mods/, the menu text made from the disc,
                           the translations copied there and edited */
        "mods/",        /* (mods kept beside the folder by some players) */
        "update/",      /* the update's own folder */
        "bars/",        /* bars/faces.txt: the player's own faces of the fight panels */
        "net_session/", /* what an online session leaves for the next start */
        "screenshots/", "replays/", "shots/",
    };
    static const char *const files[] = {
        "bt3_settings.txt", "bt3_pipelines.txt", "online_name.txt", "zdump.bin", "a11y_log.txt", "a11y_untranslated.txt",
        "prism.dll", "libprism.so", /* the screen reader's library, placed there by the player */
    };
    static const char *const own[] = {"textures/", "songs/", "stages/"}; /* packs, added songs and stages */
    char low[512];
    size_t i, n = strlen(rel);

    if (n >= sizeof(low)) {
        return 1;
    }
    for (i = 0; i <= n; i++) { /* (Windows does not tell BT3_Settings.txt from bt3_settings.txt) */
        low[i] = (char)(rel[i] >= 'A' && rel[i] <= 'Z' ? rel[i] - 'A' + 'a' : rel[i]);
    }
    for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        if (strncmp(low, dirs[i], strlen(dirs[i])) == 0) {
            return 1;
        }
    }
    for (i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        if (strcmp(low, files[i]) == 0) {
            return 1;
        }
    }
    if (strchr(low, '/') == NULL && (strncmp(low, "bt3_log", 7) == 0 || strncmp(low, "bt3_crash", 9) == 0 ||
                                     (strncmp(low, "a11y_", 5) == 0 && n > 8 && strcmp(low + n - 8, "_own.txt") == 0))) {
        return 1; /* the logs, crash reports, the screen reader's own lines */
    }
    for (i = 0; i < sizeof(own) / sizeof(own[0]); i++) {
        size_t k = strlen(own[i]);
        if (strncmp(low, own[i], k) == 0) {
            return strcmp(low + k, "readme.txt") != 0; /* (only the note a release has in each may be replaced) */
        }
    }
    return 0;
}

/* fonts/fonts.tsv: the new list, then every line of the player's whose font (the first column) the new one does not
   have (fonts the player added to the folder and listed). A new buffer (SDL_free it), or NULL: the new list as is. */
static char *fonts_merge(const char *fresh, size_t freshSize, const char *mine, size_t mineSize, size_t *outSize) {
    char *out = (char *)SDL_malloc(freshSize + mineSize + 64), *at;
    const char *p, *end = mine + mineSize;
    int added = 0;

    if (out == NULL) {
        return NULL;
    }
    memcpy(out, fresh, freshSize);
    at = out + freshSize;
    if (freshSize > 0 && fresh[freshSize - 1] != '\n') {
        *at++ = '\n';
    }
    for (p = mine; p < end;) {
        const char *nl = memchr(p, '\n', (size_t)(end - p)), *stop = nl != NULL ? nl : end, *tab;
        size_t len = (size_t)(stop - p), key;
        const char *q;
        int known = 0;
        if (len > 0 && p[len - 1] == '\r') {
            len--;
        }
        tab = memchr(p, '\t', len);
        key = tab != NULL ? (size_t)(tab - p) : len;
        if (len > 0 && p[0] != '#' && key > 0) {
            for (q = fresh; q < fresh + freshSize && !known;) { /* is that font in the new list (a line starting with it)? */
                const char *qn = memchr(q, '\n', (size_t)(fresh + freshSize - q));
                known = (size_t)((qn != NULL ? qn : fresh + freshSize) - q) > key && memcmp(q, p, key) == 0 && q[key] == '\t';
                q = qn != NULL ? qn + 1 : fresh + freshSize;
            }
            for (q = out + freshSize; q < at && !known;) { /* (or added already) */
                const char *qn = memchr(q, '\n', (size_t)(at - q));
                known = qn != NULL && (size_t)(qn - q) > key && memcmp(q, p, key) == 0 && (q[key] == '\t' || q[key] == '\n');
                q = qn != NULL ? qn + 1 : at;
            }
            if (!known) {
                if (!added) {
                    static const char note[] = "# Your own fonts (kept by the update):\n";
                    memcpy(at, note, sizeof(note) - 1);
                    at += sizeof(note) - 1;
                    added = 1;
                }
                memcpy(at, p, len);
                at += len;
                *at++ = '\n';
            }
        }
        p = nl != NULL ? nl + 1 : end;
    }
    if (!added) {
        SDL_free(out);
        return NULL;
    }
    *outSize = (size_t)(at - out);
    return out;
}

typedef struct {
    char rel[512];
    unsigned index, mode;
    int existed, staged, swapped;
} Item;

static void path_of(char *out, size_t size, const char *rel, const char *suffix) {
    SDL_snprintf(out, size, "%s%s%s", sU.root, rel, suffix);
}

/* Folders a path needs, made; those made are listed in `made` (for undoing). */
static int make_dirs(const char *rel, char (*made)[512], int *count, int max) {
    char part[512];
    const char *p;
    for (p = strchr(rel, '/'); p != NULL; p = strchr(p + 1, '/')) {
        char path[1600];
        SDL_PathInfo info;
        SDL_strlcpy(part, rel, (size_t)(p - rel) + 1);
        path_of(path, sizeof(path), part, "");
        if (SDL_GetPathInfo(path, &info)) {
            if (info.type != SDL_PATHTYPE_DIRECTORY) {
                return 0;
            }
            continue;
        }
        if (!SDL_CreateDirectory(path)) {
            return 0;
        }
        if (*count < max) {
            SDL_strlcpy(made[(*count)++], path, 512);
        }
    }
    return 1;
}

static int write_file(const char *path, const void *data, size_t size, unsigned mode) {
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    int ok;
    if (io == NULL) {
        return 0;
    }
    ok = SDL_WriteIO(io, data, size) == size;
    ok = SDL_CloseIO(io) && ok;
#ifndef _WIN32
    if (ok) {
        chmod(path, (mode_t)(mode & 07777));
    }
#else
    (void)mode;
#endif
    return ok;
}

static void undo(Item *items, int count, char (*made)[512], int madeCount) {
    char path[1600], old[1600];
    int i;
    for (i = count - 1; i >= 0; i--) {
        path_of(path, sizeof(path), items[i].rel, "");
        path_of(old, sizeof(old), items[i].rel, ".old");
        if (items[i].swapped) {
            if (items[i].existed) { /* the new file goes, the old one comes back */
                SDL_RemovePath(path);
                if (!SDL_RenamePath(old, path)) {
                    fprintf(stderr, "bt3: update: could NOT put back %s (it is %s.old): %s\n", items[i].rel, items[i].rel, SDL_GetError());
                }
            } else {
                SDL_RemovePath(path); /* (a file the update had added) */
            }
        }
        if (items[i].staged) {
            path_of(path, sizeof(path), items[i].rel, ".new");
            SDL_RemovePath(path);
        }
    }
    for (i = madeCount - 1; i >= 0; i--) {
        SDL_RemovePath(made[i]); /* (only when empty) */
    }
}

static int SDLCALL install_thread(void *arg) {
    enum { MAX_ITEMS = 4096, MAX_DIRS = 256 };
    static Item items[MAX_ITEMS];
    static char made[MAX_DIRS][512];
    char path[1600], other[1600], why[700] = "", hex[65];
    int count = 0, madeCount = 0, kept = 0, haveGame = 0, failAt = -1, i, n;
    size_t zipSize = 0;
    void *zipData;
    mz_zip_archive zip;
    SDL_IOStream *list;

    (void)arg;
    if (SDL_getenv("BT3_UPDATE_FAIL_AT") != NULL) {
        failAt = SDL_atoi(SDL_getenv("BT3_UPDATE_FAIL_AT")); /* (testing: that swap fails) */
    }
    zipData = SDL_LoadFile(sU.file, &zipSize);
    if (zipData != NULL) {
        UpdSha256_Hex(zipData, zipSize, hex);
    }
    memset(&zip, 0, sizeof(zip));
    if (zipData == NULL || SDL_strcasecmp(hex, sU.sha) != 0 || !mz_zip_reader_init_mem(&zip, zipData, zipSize, 0)) {
        SDL_free(zipData);
        fail("The downloaded archive could not be read again, or it changed since it was checked. Nothing was changed.");
        return 0;
    }
    /* the plan: what the zip has, where it goes */
    n = (int)mz_zip_reader_get_num_files(&zip);
    for (i = 0; i < n && why[0] == '\0'; i++) {
        mz_zip_archive_file_stat st;
        const char *rel, *part;
        unsigned unixMode;
        if (!mz_zip_reader_file_stat(&zip, (mz_uint)i, &st)) {
            SDL_strlcpy(why, "The archive's list of files is damaged.", sizeof(why));
            break;
        }
        if (strncmp(st.m_filename, GAME "/", strlen(GAME "/")) != 0) {
            SDL_snprintf(why, sizeof(why), "The archive is not a release of the game (a file outside its folder: %s).", st.m_filename);
            break;
        }
        rel = st.m_filename + strlen(GAME "/");
        if (rel[0] == '\0' || st.m_is_directory) {
            continue;
        }
        if (strchr(rel, '\\') != NULL || strchr(rel, ':') != NULL || rel[0] == '/' || strlen(rel) >= sizeof(items[0].rel) - 8) {
            SDL_snprintf(why, sizeof(why), "The archive has a file that would land outside the game's folder: %s", st.m_filename);
            break;
        }
        for (part = rel; part != NULL; part = strchr(part, '/') != NULL ? strchr(part, '/') + 1 : NULL) { /* no "." or ".." */
            size_t len = strchr(part, '/') != NULL ? (size_t)(strchr(part, '/') - part) : strlen(part);
            if ((len == 1 && part[0] == '.') || (len == 2 && part[0] == '.' && part[1] == '.')) {
                SDL_snprintf(why, sizeof(why), "The archive has a file that would land outside the game's folder: %s", st.m_filename);
                break;
            }
        }
        if (why[0] != '\0') {
            break;
        }
        if (user_file(rel)) {
            fprintf(stderr, "bt3: update: kept yours (the archive has it, it is not written): %s\n", rel);
            kept++;
            continue;
        }
        if (count == MAX_ITEMS) {
            SDL_strlcpy(why, "The archive has too many files.", sizeof(why));
            break;
        }
        memset(&items[count], 0, sizeof(items[count]));
        SDL_strlcpy(items[count].rel, rel, sizeof(items[count].rel));
        items[count].index = (unsigned)i;
        unixMode = (st.m_version_made_by >> 8) == 3 ? (unsigned)(st.m_external_attr >> 16) & 07777 : 0;
        items[count].mode = unixMode != 0 ? unixMode : 0644;
        if (strcmp(rel, GAME_EXE) == 0 || strcmp(rel, GAME "-setup") == 0 || strstr(rel, ".so") != NULL) {
            items[count].mode |= 0755; /* (the programs stay programs, whatever the archive says) */
        }
        haveGame = haveGame || strcmp(rel, GAME_EXE) == 0;
        count++;
    }
    if (why[0] == '\0' && !haveGame) {
        SDL_strlcpy(why, "The archive is not a release for this system (it has no " GAME_EXE ").", sizeof(why));
    }
    /* a. every new file beside its place, as <name>.new */
    for (i = 0; i < count && why[0] == '\0'; i++) {
        Item *it = &items[i];
        SDL_PathInfo info;
        size_t size = 0, mergedSize = 0;
        void *data;
        char *merged = NULL;
        path_of(path, sizeof(path), it->rel, "");
        if (SDL_GetPathInfo(path, &info)) {
            if (info.type != SDL_PATHTYPE_FILE) {
                SDL_snprintf(why, sizeof(why), "A folder is in the place of a file of the update: %s.", it->rel);
                break;
            }
            it->existed = 1;
        }
        if (!make_dirs(it->rel, made, &madeCount, MAX_DIRS)) {
            SDL_snprintf(why, sizeof(why), "The folder of %s could not be made (%s).", it->rel, SDL_GetError());
            break;
        }
        data = mz_zip_reader_extract_to_heap(&zip, it->index, &size, 0);
        if (data == NULL) {
            SDL_snprintf(why, sizeof(why), "The archive's %s could not be unpacked.", it->rel);
            break;
        }
        if (it->existed && SDL_strcasecmp(it->rel, "fonts/fonts.tsv") == 0) {
            size_t mineSize = 0;
            void *mine = SDL_LoadFile(path, &mineSize);
            if (mine != NULL) {
                merged = fonts_merge((const char *)data, size, (const char *)mine, mineSize, &mergedSize);
                SDL_free(mine);
            }
        }
        path_of(other, sizeof(other), it->rel, ".new");
        it->staged = 1;
        if (!write_file(other, merged != NULL ? merged : data, merged != NULL ? mergedSize : size, it->mode)) {
            SDL_snprintf(why, sizeof(why), "%s could not be written (%s).", other, SDL_GetError());
        }
        SDL_free(merged);
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    SDL_free(zipData);
    /* b. the swap: <name> -> <name>.old, <name>.new -> <name> */
    for (i = 0; i < count && why[0] == '\0'; i++) {
        Item *it = &items[i];
        path_of(path, sizeof(path), it->rel, "");
        path_of(other, sizeof(other), it->rel, ".old");
        if (i == failAt) {
            SDL_snprintf(why, sizeof(why), "%s could not be replaced (BT3_UPDATE_FAIL_AT, a test).", it->rel);
            break;
        }
        if (it->existed) {
            SDL_RemovePath(other); /* (one left by an update before, if the cleanup could not take it) */
            if (!SDL_RenamePath(path, other)) {
                SDL_snprintf(why, sizeof(why), "%s could not be renamed (%s).", it->rel, SDL_GetError());
                break;
            }
        }
        it->swapped = 1;
        path_of(other, sizeof(other), it->rel, ".new");
        if (!SDL_RenamePath(other, path)) {
            SDL_snprintf(why, sizeof(why), "%s could not be put in place (%s).", it->rel, SDL_GetError());
            break;
        }
        it->staged = 0;
    }
    if (why[0] != '\0') {
        undo(items, count, made, madeCount);
        fprintf(stderr, "bt3: update: install failed, everything put back: %s\n", why);
        SDL_strlcat(why, " Everything was put back: this version goes on as it was.", sizeof(why));
        fail(why);
        return 0;
    }
    /* done: the .old files to remove at the next start, and what to say then */
    SDL_snprintf(path, sizeof(path), "%scleanup.txt", sU.work);
    list = SDL_IOFromFile(path, "ab"); /* (SDL: the folder's path may have any letters; '\n' alone on every system) */
    for (i = 0; i < count; i++) {
        if (items[i].existed && list != NULL) {
            SDL_IOprintf(list, "%s.old\n", items[i].rel);
        }
    }
    if (list != NULL) {
        SDL_CloseIO(list);
    }
    SDL_snprintf(path, sizeof(path), "%sresult.txt", sU.work);
    lock();
    SDL_snprintf(other, sizeof(other), "ok %s\n", sU.latest);
    unlock();
    write_file(path, other, strlen(other), 0644);
    SDL_RemovePath(sU.file); /* (the download is not needed any more) */
    fprintf(stderr, "bt3: update: installed %s: %d files (%d of the player's left alone)\n", sU.latest, count, kept);
    SDL_SetAtomicInt(&sU.state, UPD_INSTALLED);
    return 0;
}

void Update_Install(void) {
    SDL_Thread *t;
    if (SDL_GetAtomicInt(&sU.state) != UPD_READY) {
        return;
    }
    SDL_SetAtomicInt(&sU.state, UPD_INSTALLING);
    t = SDL_CreateThread(install_thread, "bt3-update-install", NULL);
    if (t == NULL) {
        fail("The install could not be started.");
    } else {
        SDL_DetachThread(t);
    }
}

/* The new version, started from the same place (its own folder's program; the working folder stays the one this
   game was started in). */
int Update_Relaunch(void) {
    char exe[1300];
    const char *args[2];
    SDL_PropertiesID props;
    SDL_Process *p;

    if (SDL_GetAtomicInt(&sU.state) != UPD_INSTALLED) {
        return 0;
    }
    SDL_snprintf(exe, sizeof(exe), "%s" GAME_EXE, sU.root);
    args[0] = exe;
    args[1] = NULL;
    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_INHERITED);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_INHERITED);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    p = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (p == NULL) {
        fprintf(stderr, "bt3: update: the new version could not be started (%s): start it yourself\n", SDL_GetError());
        return 0;
    }
    fprintf(stderr, "bt3: update: started the new version (pid %lld); this one closes\n",
            (long long)SDL_GetNumberProperty(SDL_GetProcessProperties(p), SDL_PROP_PROCESS_PID_NUMBER, 0));
    SDL_DestroyProcess(p);
    return 1;
}

/* At start: the .old files the last update left (by its list), and the leftovers of a download. */
void Update_Cleanup(void) {
    char path[1300], *data, *line, *next, *keep;
    size_t size = 0, kept = 0;

    folders();
    SDL_RemovePath(sU.part);
    SDL_snprintf(path, sizeof(path), "%scleanup.txt", sU.work);
    data = (char *)SDL_LoadFile(path, &size);
    keep = data != NULL ? (char *)SDL_malloc(size + 1) : NULL;
    if (keep == NULL) {
        SDL_free(data);
        return;
    }
    for (line = data; line < data + size; line = next) {
        char old[1600];
        size_t len;
        next = memchr(line, '\n', (size_t)(data + size - line));
        next = next != NULL ? next + 1 : data + size;
        len = (size_t)(next - line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
            len--; /* (also a list written by hand on Windows) */
        }
        if (len <= 4 || len >= 512 || memcmp(line + len - 4, ".old", 4) != 0) {
            continue; /* (only the update's own .old files, by their place in the folder) */
        }
        memcpy(old, line, len);
        old[len] = '\0';
        if (strstr(old, "..") != NULL) {
            continue;
        }
        path_of(path, sizeof(path), old, "");
        if (!SDL_RemovePath(path) && is_file(path)) { /* still in use: next time */
            memcpy(keep + kept, old, len);
            kept += len;
            keep[kept++] = '\n';
        }
    }
    SDL_snprintf(path, sizeof(path), "%scleanup.txt", sU.work);
    if (kept == 0) {
        SDL_RemovePath(path);
    } else {
        SDL_SaveFile(path, keep, kept);
    }
    SDL_free(keep);
    SDL_free(data);
}

/* ---- what happened the last time ---- */
int Update_LastResult(const char **version) {
    static int read, result;
    static char ver[32];

    if (!read) {
        char path[1300], seen[1300];
        size_t size = 0;
        char *data;
        read = 1;
        folders();
        SDL_snprintf(path, sizeof(path), "%sresult.txt", sU.work);
        data = (char *)SDL_LoadFile(path, &size);
        if (data != NULL) {
            if (strncmp(data, "ok ", 3) == 0) {
                result = 1;
                SDL_strlcpy(ver, data + 3, sizeof(ver));
                ver[strcspn(ver, "\r\n ")] = '\0';
                fprintf(stderr, "bt3: update: updated to %s (this is %s)\n", ver, PORT_VERSION);
            }
            SDL_free(data);
            SDL_snprintf(seen, sizeof(seen), "%sresult_seen.txt", sU.work);
            SDL_RenamePath(path, seen); /* (told once) */
        }
    }
    if (version != NULL) {
        *version = ver;
    }
    return result;
}
