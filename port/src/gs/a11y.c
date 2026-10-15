/*
 * Screen reader output (accessibility): speech and braille through Prism (https://github.com/ethindp/prism,
 * MPL-2.0), loaded at run time from prism.dll (libprism.so elsewhere) in the program's folder. Without that
 * library, and without $BT3_A11Y_LOG, all of this stays off: no thread, no file, no key, nothing said.
 *
 * One speaker for the whole program. port/src/a11y_game.c (the game's thread, once per blank) says what changed on
 * the game's screens; the render thread's event loops pass their events here for the repeat key and button. The
 * library is only called from a thread of its own, so a slow screen reader never holds up a frame.
 *
 * A message either interrupts (it cuts what is being said and drops what is queued) or is queued after the one
 * before. "Repeat" says again the last interrupting message and the ones queued after it.
 *
 *   bt3_settings.txt  a11y=0            off, even with the library there
 *                     a11y_key_repeat   the repeat key, an SDL scancode (default F2); the controller's touchpad
 *                                       click repeats too, except in a fight
 *                     a11y_key_gauges1  the keys that say player 1's and player 2's gauges in a fight (default F3
 *                     a11y_key_gauges2  and F4); the touchpad click says them too, by the half it is clicked on
 *                     a11y_log=1        the log below, into a11y_log.txt in the program's folder (for a report)
 *                     a11y_cue_volume   the fight's cues (a11y_game.c: a transformation or the Ultimate Blast within
 *                                       reach), in percent: default 60, 0 turns them off
 *                     a11y_lang="pt"    the language to speak, a code of two or three letters (default: the
 *                                       system's preferred language)
 *                     a11y_missing=1    out of English, adds each line said without a translation, once, to
 *                                       a11y_untranslated.txt in the program's folder (for whoever translates)
 *   $BT3_A11Y_LOG=<file>                writes what is said, one line each with the blank (tests without a screen
 *                                       reader: the headless build has no library), and notes on what changed on
 *                                       the screen, marked "#"
 *   $BT3_A11Y_ASK=<n>                   asks for the gauges every n blanks, player 1's and player 2's in turn, as
 *                                       their keys would (tests: without a window no key can be pressed)
 *   $BT3_A11Y_LANG=<code>               the language, over a11y_lang
 *   a11y_<code>.txt                     the lines to say instead of the English ones (see "translations" below);
 *                                       a11y_es.txt comes with the program
 *   a11y_<code>_own.txt                 the player's own lines, which win over those
 *   $BT3_A11Y_MISSING=<file>            the lines without a translation into this file instead, a new one each run
 *
 * The screen reader is chosen by trying them: NVDA, then JAWS, then the others in Prism's order. A screen reader
 * is used only when it starts (prism_backend_initialize), not because it is installed: Prism's own "best" choice
 * took NVDA while NVDA was not running (0.16.5).
 *
 * Prism's version: written against release 0.18.3, the one to put beside the program. Its header is not in the
 * repository; the eleven functions used are declared below by hand and found by name, and a library without one of
 * them is not used. What a later release could change, and what is done about it:
 *   - PrismConfig growing (it is returned by value, into memory of the caller's): the memory given is
 *     PRISM_CONFIG_ROOM bytes, several times the struct of 0.18.3, and none of its fields is read or written here
 *     but the first byte, its `version`, which goes to the log. It is handed back to prism_init as it came.
 *   - not guarded: a change in the parameters of one of the eleven functions, in the feature bits or in the ids of
 *     NVDA and JAWS; a PrismConfig beyond PRISM_CONFIG_ROOM; prism_config_init no longer returning the struct by
 *     value. None of the functions declared here tells the library's version, so there is no check of it at load
 *     time: with another release than 0.18.3 the line "a11y: prism config version" in the output is the only sign.
 */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

extern int Port_Setting(const char *name, int def); /* plat_settings.c */
extern void Port_SettingText(const char *name, const char *def, char *out, int size);

/* fopen for a path in UTF-8 (SDL's: the program's folder), which the C library of Windows would take for one in the
   system's code page: a folder with letters outside it would not open. */
static FILE *open_utf8(const char *path, const char *mode) {
#ifdef _WIN32
    wchar_t wpath[1024], wmode[8];

    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 1024) == 0 || MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 8) == 0) {
        return NULL;
    }
    return _wfopen(wpath, wmode);
#else
    return fopen(path, mode);
#endif
}

#define A11Y_INTERRUPT 1 /* (the same in a11y_game.c) */
#define A11Y_TEXT 1024
#define A11Y_QUEUE 8

/* ---- Prism's C interface: the part used here (include/prism.h of release 0.18.3) ---- */

#ifdef _WIN32
#define PRISM_CALL __cdecl
#define PRISM_LIB "prism.dll"
#else
#define PRISM_CALL
#define PRISM_LIB "libprism.so"
#endif

typedef struct PrismContext PrismContext;
typedef struct PrismBackend PrismBackend;
#define PRISM_CONFIG_ROOM 512
/* PrismConfig as 0.18.3 has it, inside the room a later and larger one may take. A struct of this size is returned
   through a pointer the caller passes, in the Windows and the System V conventions alike, so the library writes as
   many bytes as its own PrismConfig has into this memory, whatever is declared here. */
typedef union {
    struct {
        uint8_t version;
        void *registry;
        void *availability_callback, *availability_userdata;
        uint32_t availability_poll_interval_ms, availability_debounce_samples, availability_backoff_max_ms;
        bool availability_auto_power_manage;
        void *availability_baseline_callback;
    } v0_18_3;
    unsigned char room[PRISM_CONFIG_ROOM];
    uint64_t align;
} PrismConfig;

#define PRISM_OK 0
#define PRISM_SUPPORTS_SPEAK (1ULL << 2)
#define PRISM_SUPPORTS_OUTPUT (1ULL << 5)
#define PRISM_NVDA 0x89CC19C5C4AC1A56ULL
#define PRISM_JAWS 0xAC3D60E9BD84B53EULL

static struct {
    PrismConfig(PRISM_CALL *config_init)(void);
    PrismContext *(PRISM_CALL *init)(PrismConfig *cfg);
    size_t(PRISM_CALL *count)(PrismContext *ctx);
    uint64_t(PRISM_CALL *id_at)(PrismContext *ctx, size_t index);
    const char *(PRISM_CALL *name)(PrismContext *ctx, uint64_t id);
    PrismBackend *(PRISM_CALL *create)(PrismContext *ctx, uint64_t id);
    void(PRISM_CALL *free_backend)(PrismBackend *backend);
    uint64_t(PRISM_CALL *features)(PrismBackend *backend);
    int(PRISM_CALL *initialize)(PrismBackend *backend);
    int(PRISM_CALL *output)(PrismBackend *backend, const char *text, bool interrupt);
    int(PRISM_CALL *speak)(PrismBackend *backend, const char *text, bool interrupt);
} P;

static SDL_SharedObject *sLib;

static int load_prism(void) {
    static const char *const names[] = {"prism_config_init", "prism_init", "prism_registry_count",
                                        "prism_registry_id_at", "prism_registry_name", "prism_registry_create",
                                        "prism_backend_free", "prism_backend_get_features",
                                        "prism_backend_initialize", "prism_backend_output", "prism_backend_speak"};
    SDL_FunctionPointer *slot = (SDL_FunctionPointer *)&P;
    char path[1024];
    const char *base = SDL_GetBasePath(); /* (the program's folder only: not the current one, not the search path) */
    size_t i;

    if (base == NULL) {
        return 0;
    }
    snprintf(path, sizeof(path), "%s%s", base, PRISM_LIB);
    sLib = SDL_LoadObject(path);
    if (sLib == NULL) {
        return 0;
    }
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        slot[i] = SDL_LoadFunction(sLib, names[i]);
        if (slot[i] == NULL) {
            fprintf(stderr, "a11y: %s has no %s\n", path, names[i]);
            SDL_UnloadObject(sLib);
            sLib = NULL;
            return 0;
        }
    }
    return 1;
}

/* The screen reader `id` if it starts and can speak, or NULL. */
static PrismBackend *try_backend(PrismContext *ctx, uint64_t id) {
    PrismBackend *b = P.create(ctx, id);

    if (b == NULL) {
        return NULL;
    }
    if (P.initialize(b) != PRISM_OK || !(P.features(b) & (PRISM_SUPPORTS_OUTPUT | PRISM_SUPPORTS_SPEAK))) {
        P.free_backend(b);
        return NULL;
    }
    return b;
}

static PrismBackend *choose_backend(PrismContext *ctx) {
    static const uint64_t first[] = {PRISM_NVDA, PRISM_JAWS};
    PrismBackend *b;
    uint64_t id;
    size_t i, n = P.count(ctx);

    for (i = 0; i < 2; i++) {
        if ((b = try_backend(ctx, first[i])) != NULL) {
            fprintf(stderr, "a11y: speaking through %s\n", P.name(ctx, first[i]));
            return b;
        }
    }
    for (i = 0; i < n; i++) {
        id = P.id_at(ctx, i);
        if (id != PRISM_NVDA && id != PRISM_JAWS && (b = try_backend(ctx, id)) != NULL) {
            fprintf(stderr, "a11y: speaking through %s\n", P.name(ctx, id));
            return b;
        }
    }
    fprintf(stderr, "a11y: no screen reader or voice started\n");
    return NULL;
}

/* ---- the speaker ---- */

static volatile int sOn; /* set once by the game's thread before the speaker starts; read by the render thread */
static int sRepeatKey, sGaugeKey[2], sCueVolume;
static int sTranslate;         /* the language is not English */
static char sLang[4] = "en";   /* its code: two or three small letters */
static int sFrame; /* blanks since the start (only for the log) */
static SDL_AtomicInt sAsk;   /* the gauges asked for and not said yet: 1 + the player's side, 0 none */
static SDL_AtomicInt sFight; /* 1 while a fight is on with no menu over it (a11y_game.c says so every blank) */
static float sTouchX = -1;   /* where a finger last touched the controller's touchpad, 0 left to 1 right (render
                                thread only; -1: none yet) */
static FILE *sLog;
static SDL_Mutex *sLock;
static SDL_Condition *sWake;
static struct {
    char text[A11Y_TEXT];
    int interrupt, frame;
} sQueue[A11Y_QUEUE];
static int sHead, sCount;
static char sLast[A11Y_TEXT * 2]; /* what "repeat" says */

/* Adds a message to the queue (the lock is held). */
static void push(const char *text, int interrupt) {
    int k;

    if (interrupt) {
        sCount = 0;
    }
    if (sCount == A11Y_QUEUE) {
        return;
    }
    k = (sHead + sCount) % A11Y_QUEUE;
    snprintf(sQueue[k].text, sizeof(sQueue[k].text), "%s", text);
    sQueue[k].interrupt = interrupt;
    sQueue[k].frame = sFrame;
    sCount++;
    SDL_SignalCondition(sWake);
}

static int SDLCALL speaker(void *unused) {
    PrismContext *ctx = NULL;
    PrismBackend *b = NULL;
    PrismConfig cfg;
    char text[A11Y_TEXT];
    int interrupt, frame;

    (void)unused;
    if (sLib != NULL) {
        cfg = P.config_init();
        fprintf(stderr, "a11y: prism config version %u (written against Prism 0.18.3)\n", (unsigned)cfg.v0_18_3.version);
        ctx = P.init(&cfg);
        b = ctx != NULL ? choose_backend(ctx) : NULL;
    }
    for (;;) {
        SDL_LockMutex(sLock);
        while (sCount == 0) {
            SDL_WaitCondition(sWake, sLock);
        }
        memcpy(text, sQueue[sHead].text, sizeof(text));
        interrupt = sQueue[sHead].interrupt;
        frame = sQueue[sHead].frame;
        sHead = (sHead + 1) % A11Y_QUEUE;
        sCount--;
        SDL_UnlockMutex(sLock);
        if (b != NULL && P.output(b, text, interrupt != 0) != PRISM_OK) {
            (void)P.speak(b, text, interrupt != 0); /* (a voice without braille) */
        }
        if (sLog != NULL) {
            fprintf(sLog, "%d %s%s\n", frame, interrupt ? "" : "+ ", text);
            fflush(sLog);
        }
    }
    return 0;
}

/* ---- translations ----
 *
 * Out of English, a11y_<code>.txt gives the lines to say instead of the English ones, one per line: the FNV-1a hash
 * (64 bits, 16 hex digits) of the English line as it would be said (UTF-8, on one line: one of the game's own as
 * PortA11y_Utf8 makes it, or a label as a11y_game.c has it), a tab, and the line to say. Lines starting with '#' are
 * notes. Only the hash of the English is needed, so the file need not carry the game's text. A label with numbers or
 * names put in it (%d, %s) keeps them, in the same order, or the English is said. A label that some language says
 * two ways (Spanish "Aleatorio" for a stage, "Aleatoria" for a song) has a second key, the English, a bar and the
 * sense ("Random|song"; trc in a11y_game.c): its line is taken when the file has one, else the plain key's.
 *
 * Every language goes this way, Spanish too: nothing but English is in the code. port/lang/a11y_es.txt has all the
 * labels of a11y_game.c, each key above its line, and is the list to translate from; a release carries it beside
 * the program (port/tools/package.py).
 *
 * Two files are read: a11y_<code>.txt, and a11y_<code>_own.txt, the player's own, whose lines win and which no
 * release replaces: for where the translation of a disc did not reach (the memory card's messages, the main menu
 * guides' lines) or a line one would rather hear another way. Both are looked for in the program's folder; the
 * first also in ../lang/ from there (the repository's port/lang, for a program run from port/build).
 *
 * Either file may also have lines "~word<tab>words": in the game's own lines (not the labels) that whole word is
 * said as the words after the tab. A full stop at the word's end ("~Abbr.<tab>Abbreviation") is taken with the word
 * when one follows it. For a disc whose text abbreviates, or spells a name otherwise than the player wants. */

typedef struct {
    uint64_t hash;
    const char *text;
    int rank; /* 0: from the player's own file, 1: from the language's */
} A11yLine;

typedef struct {
    const char *from, *to;
    int dot; /* a full stop right after the word goes with it */
} A11yWord;

static A11yLine *sLines; /* by hash, the player's own first */
static int sLineCount;
static A11yWord *sWords;
static int sWordCount;
static FILE *sMissing;
static uint64_t sMissed[4096]; /* the lines already in sMissing */
static int sMissedCount;

static uint64_t fnv1a(const char *s) {
    uint64_t h = 0xCBF29CE484222325ULL;

    for (; *s != '\0'; s++) {
        h = (h ^ (uint8_t)*s) * 0x100000001B3ULL;
    }
    return h;
}

static int by_hash(const void *a, const void *b) {
    const A11yLine *x = a, *y = b;

    return x->hash < y->hash ? -1 : x->hash > y->hash ? 1 : x->rank - y->rank;
}

/* Reads one file of lines (kept for the whole run: the lines point into it); 1 when it was there. */
static int load_file(const char *path, int rank) {
    char *data = SDL_LoadFile(path, NULL), *s, *next, *tab; /* (ends in a terminator) */
    void *lines, *words;
    int max = 1;
    size_t len;

    if (data == NULL) {
        return 0;
    }
    for (s = data; *s != '\0'; s++) {
        max += *s == '\n';
    }
    lines = SDL_realloc(sLines, (size_t)(sLineCount + max) * sizeof(*sLines));
    sLines = lines != NULL ? lines : sLines;
    words = SDL_realloc(sWords, (size_t)(sWordCount + max) * sizeof(*sWords));
    sWords = words != NULL ? words : sWords;
    if (lines == NULL || words == NULL) {
        SDL_free(data);
        return 0;
    }
    s = data;
    if ((uint8_t)s[0] == 0xEF && (uint8_t)s[1] == 0xBB && (uint8_t)s[2] == 0xBF) {
        s += 3; /* (the mark Notepad writes) */
    }
    for (; s != NULL; s = next) {
        if ((next = strchr(s, '\n')) != NULL) {
            *next++ = '\0';
        }
        if ((len = strlen(s)) > 0 && s[len - 1] == '\r') {
            s[len - 1] = '\0';
        }
        tab = strchr(s, '\t');
        if (tab == NULL || tab[1] == '\0') {
            continue;
        }
        if (s[0] == '~') {
            *tab = '\0';
            sWords[sWordCount].dot = tab > s + 1 && tab[-1] == '.';
            if (sWords[sWordCount].dot) {
                tab[-1] = '\0';
            }
            sWords[sWordCount].from = s + 1;
            sWords[sWordCount].to = tab + 1;
            sWordCount += s[1] != '\0';
        } else if (s[0] != '#' && tab - s == 16) {
            sLines[sLineCount].hash = strtoull(s, NULL, 16);
            sLines[sLineCount].text = tab + 1;
            sLines[sLineCount].rank = rank;
            sLineCount++;
        }
    }
    return 1;
}

/* Reads a11y_<code>_own.txt and a11y_<code>.txt. */
static void load_lines(void) {
    char path[1024];
    const char *base = SDL_GetBasePath();

    if (base == NULL) {
        return;
    }
    snprintf(path, sizeof(path), "%sa11y_%s_own.txt", base, sLang);
    load_file(path, 0);
    snprintf(path, sizeof(path), "%sa11y_%s.txt", base, sLang);
    if (!load_file(path, 1)) {
        snprintf(path, sizeof(path), "%s../lang/a11y_%s.txt", base, sLang);
        load_file(path, 1);
    }
    if (sLineCount > 0) {
        qsort(sLines, sLineCount, sizeof(*sLines), by_hash);
    }
}

/* The line the files have for an English one (or for a label's key), or NULL; then, with `report`, the English goes
   to sMissing, once. */
static const char *find_line(const char *en, int report) {
    uint64_t hash = fnv1a(en);
    int lo = 0, hi = sLineCount, mid, i;

    while (lo < hi) { /* the first of that hash: the player's own, when both files have it */
        mid = lo + (hi - lo) / 2;
        if (sLines[mid].hash < hash) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < sLineCount && sLines[lo].hash == hash) {
        return sLines[lo].text;
    }
    if (report && sMissing != NULL) {
        SDL_LockMutex(sLock); /* (the render thread asks too: PortA11y_Label) */
        for (i = 0; i < sMissedCount && sMissed[i] != hash; i++) {
        }
        if (i == sMissedCount && sMissedCount < (int)(sizeof(sMissed) / sizeof(sMissed[0]))) {
            sMissed[sMissedCount++] = hash;
            fprintf(sMissing, "%016llx\t%s\n", (unsigned long long)hash, en);
            fflush(sMissing);
        }
        SDL_UnlockMutex(sLock);
    }
    return NULL;
}

/* Out of English: the line from a11y_<code>.txt for `out` (one of the game's lines, n bytes of UTF-8) into out, if
   there is one. Returns the length. */
static int translated_line(char *out, int cap, int n) {
    const char *text;

    if (!sTranslate || n == 0 || (text = find_line(out, 1)) == NULL) {
        return n;
    }
    n = (int)strlen(text);
    if (n >= cap) { /* (cut between letters, not inside one) */
        for (n = cap - 1; n > 0 && ((uint8_t)text[n] & 0xC0) == 0x80; n--) {
        }
    }
    memcpy(out, text, n);
    out[n] = '\0';
    return n;
}

/* The next of a format's conversions (%d, %02d, %s, ...) from s on, or NULL ("%%" is none). */
static const char *next_conversion(const char *s) {
    for (; (s = strchr(s, '%')) != NULL; s += 2) {
        if (s[1] != '%') {
            return s;
        }
    }
    return NULL;
}

/* 1: a and b have the same conversions in the same order, so one can be the format for the other's arguments. */
static int same_conversions(const char *a, const char *b) {
    size_t n;

    for (;;) {
        a = next_conversion(a);
        b = next_conversion(b);
        if (a == NULL || b == NULL) {
            return a == b;
        }
        n = 1 + strspn(a + 1, "-+ #0123456789.hlz");
        if (strchr("cdiouxXs", a[n]) == NULL || a[n] == '\0' || strncmp(a, b, n + 1) != 0) {
            return 0;
        }
        a += n + 1;
        b += n + 1;
    }
}

/* A label of a11y_game.c (UTF-8, perhaps a format), in the language spoken: the line the files have for it when it
   keeps the same conversions, else the English. With a sense, the line of the key "label|sense" first. */
static const char *label_line(const char *label, const char *sense) {
    char key[512];
    const char *text;

    if (!sTranslate || label == NULL || label[0] == '\0') {
        return label;
    }
    if (sense != NULL && (size_t)snprintf(key, sizeof(key), "%s|%s", label, sense) < sizeof(key) &&
        (text = find_line(key, 0)) != NULL && same_conversions(label, text)) {
        return text;
    }
    if ((text = find_line(label, 1)) != NULL && same_conversions(label, text)) {
        return text;
    }
    return label;
}

const char *PortA11y_Tr(const char *label) {
    return label_line(label, NULL);
}

/* The same for the program's other windows (gs/ui.cpp, on the render thread): the label as it is until the speaker
   is on, which is when the files have been read. */
const char *PortA11y_Label(const char *label) {
    return sOn ? label_line(label, NULL) : label;
}

/* The same for a label that some language says another way in this sense ("song": a feminine noun in Spanish). */
const char *PortA11y_TrSense(const char *sense, const char *label) {
    return label_line(label, sense);
}

/* sMissing for a11y_missing=1: a11y_untranslated.txt, added to, with the lines it has already counted as written. */
static FILE *open_missing(const char *base) {
    char path[1024], line[64];
    int start = 1; /* line[] is the start of a line of the file (a long one comes in pieces) */
    FILE *fp;

    snprintf(path, sizeof(path), "%sa11y_untranslated.txt", base);
    if ((fp = open_utf8(path, "r")) != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL && sMissedCount < (int)(sizeof(sMissed) / sizeof(sMissed[0]))) {
            if (start && strlen(line) > 16 && line[16] == '\t') {
                sMissed[sMissedCount++] = strtoull(line, NULL, 16);
            }
            start = strchr(line, '\n') != NULL;
        }
        fclose(fp);
    }
    return open_utf8(path, "a");
}

/* A byte of a word: an ASCII letter or any byte of a letter outside ASCII. */
static int word_byte(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (uint8_t)c >= 0x80;
}

/* The files' word replacements ("~word<tab>words") on `out` (n bytes, room for cap with the terminator); returns the
   new length. A replacement that does not fit is left out. */
static int own_words(char *out, int n, int cap) {
    size_t from, to;
    char *p;
    int k;

    if (cap <= 0) {
        return n;
    }
    for (k = 0; k < sWordCount; k++) {
        to = strlen(sWords[k].to);
        for (p = strstr(out, sWords[k].from); p != NULL; p = strstr(p, sWords[k].from)) {
            from = strlen(sWords[k].from);
            if ((p > out && word_byte(p[-1])) || word_byte(p[from])) {
                p++;
                continue;
            }
            if (sWords[k].dot && p[from] == '.') {
                from++;
            }
            if (n + (int)to - (int)from >= cap) {
                break;
            }
            memmove(p + to, p + from, n - (size_t)(p - out) - from + 1);
            memcpy(p, sWords[k].to, to);
            n += (int)to - (int)from;
            p += to;
        }
    }
    return n;
}

/* The language from a code like "pt-BR" or "es_AR": its first letters, lowercase, at most three. The code becomes
   part of a file name, so nothing but letters is taken; "en" stays when it has none. */
static void set_lang(const char *code) {
    int i;
    char c;

    for (i = 0; i < (int)sizeof(sLang) - 1; i++) {
        c = (char)(code[i] | 0x20);
        if (c < 'a' || c > 'z') {
            break;
        }
        sLang[i] = c;
    }
    if (i > 0) {
        sLang[i] = '\0';
    }
}

static void start(void) {
    const char *lang = getenv("BT3_A11Y_LANG"), *base;
    char path[1024], code[8];
    SDL_Locale **loc;
    int n = 0;

    if (!Port_Setting("a11y", 1)) {
        return;
    }
    if (getenv("BT3_A11Y_LOG") != NULL) {
        sLog = fopen(getenv("BT3_A11Y_LOG"), "w");
    } else if (Port_Setting("a11y_log", 0) && (base = SDL_GetBasePath()) != NULL) {
        snprintf(path, sizeof(path), "%sa11y_log.txt", base);
        sLog = open_utf8(path, "w");
    }
    if (!load_prism() && sLog == NULL) {
        return;
    }
    Port_SettingText("a11y_lang", "", code, sizeof(code));
    if (lang == NULL && code[0] != '\0') {
        lang = code;
    }
    if (lang != NULL) {
        set_lang(lang);
    } else if ((loc = SDL_GetPreferredLocales(&n)) != NULL) {
        if (n > 0 && loc[0]->language != NULL) {
            set_lang(loc[0]->language);
        }
        SDL_free(loc);
    }
    sTranslate = strcmp(sLang, "en") != 0;
    if (sTranslate) {
        load_lines();
        if (getenv("BT3_A11Y_MISSING") != NULL) {
            sMissing = fopen(getenv("BT3_A11Y_MISSING"), "w");
        } else if (Port_Setting("a11y_missing", 0) && (base = SDL_GetBasePath()) != NULL) {
            sMissing = open_missing(base);
        }
    }
    sRepeatKey = Port_Setting("a11y_key_repeat", SDL_SCANCODE_F2);
    sGaugeKey[0] = Port_Setting("a11y_key_gauges1", SDL_SCANCODE_F3);
    sGaugeKey[1] = Port_Setting("a11y_key_gauges2", SDL_SCANCODE_F4);
    sCueVolume = Port_Setting("a11y_cue_volume", 60);
    sCueVolume = sCueVolume < 0 ? 0 : sCueVolume > 100 ? 100 : sCueVolume;
    sLock = SDL_CreateMutex();
    sWake = SDL_CreateCondition();
    if (sLock == NULL || sWake == NULL || SDL_CreateThread(speaker, "a11y", NULL) == NULL) {
        fprintf(stderr, "a11y: the speaker did not start: %s\n", SDL_GetError());
        return;
    }
    sOn = 1;
}

/* Once per blank, before anything is said in it: 1 while the accessibility is on. The first call starts it. */
int PortA11y_Begin(void) {
    static int tried;

    if (!tried) {
        tried = 1;
        start();
    }
    sFrame++;
    return sOn;
}

/* Says a message (UTF-8). flags: A11Y_INTERRUPT, or 0 to queue it after the one before. */
void PortA11y_Say(const char *text, int flags) {
    size_t len;

    if (!sOn || text == NULL || text[0] == '\0') {
        return;
    }
    SDL_LockMutex(sLock);
    if (flags & A11Y_INTERRUPT) {
        snprintf(sLast, sizeof(sLast), "%s", text);
    } else if ((len = strlen(sLast)) + strlen(text) + 3 < sizeof(sLast)) {
        /* (a pause between the two when said again: a full stop unless the first ends a sentence already) */
        snprintf(sLast + len, sizeof(sLast) - len, "%s%s", len > 0 && strchr(".!?:", sLast[len - 1]) == NULL ? ". " : " ",
                 text);
    }
    push(text, flags & A11Y_INTERRUPT);
    SDL_UnlockMutex(sLock);
}

/* Writes a note to the log only ("# text" after the blank): what changed on the screen, to follow a report. */
void PortA11y_Note(const char *text) {
    if (sOn && sLog != NULL) {
        fprintf(sLog, "%d # %s\n", sFrame, text); /* (stdio locks the file against the speaker's lines) */
        fflush(sLog);
    }
}

/* snd_adx.c, apply_volume: the gain on top of the game's own for one of its players. a11y_game.c decides it as a
   percent: it is built soft-float, so it cannot hand a float to this side. */
float PortA11y_StreamGain(const void *player) {
    extern int PortA11y_VoicePercent(const void *player);
    int percent = PortA11y_VoicePercent(player);

    return percent < 0 ? 1.0f : (float)percent / 100.0f;
}

/* Says again what was said last. */
void PortA11y_Repeat(void) {
    if (!sOn) {
        return;
    }
    SDL_LockMutex(sLock);
    if (sLast[0] != '\0') {
        push(sLast, 1);
    }
    SDL_UnlockMutex(sLock);
}

/* The render thread's events (gs_gpu.c, gs_gl.c): the repeat key, the gauge keys and the controller's touchpad. Its
   click repeats, but in a fight it asks for the gauges of the player on the half it was clicked (left: player 1),
   which a11y_game.c says at the next blank. Nothing is taken from the game: none of them is bound to a button of it. */
void PortA11y_Event(const SDL_Event *ev) {
    char note[64];
    int side;

    if (!sOn) {
        return;
    }
    if (ev->type == SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN || ev->type == SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION) {
        if (sTouchX < 0) {
            PortA11y_Note("touchpad: a finger is seen");
        }
        sTouchX = ev->gtouchpad.x;
    } else if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
        if ((int)ev->key.scancode == sRepeatKey) {
            PortA11y_Repeat();
        } else if ((int)ev->key.scancode == sGaugeKey[0] || (int)ev->key.scancode == sGaugeKey[1]) {
            SDL_SetAtomicInt(&sAsk, (int)ev->key.scancode == sGaugeKey[0] ? 1 : 2);
        }
    } else if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && ev->gbutton.button == SDL_GAMEPAD_BUTTON_TOUCHPAD) {
        side = sTouchX >= 0.5f ? 1 : 0;
        snprintf(note, sizeof(note), "touchpad click at %d%%", sTouchX < 0 ? -1 : (int)(sTouchX * 100));
        PortA11y_Note(note);
        if (SDL_GetAtomicInt(&sFight)) {
            SDL_SetAtomicInt(&sAsk, 1 + side);
        } else {
            PortA11y_Repeat();
        }
    }
}

/* For a11y_game.c, once per blank: whether a fight is on with no menu over it (the touchpad click asks for the gauges
   then), and the gauges asked for since the last call (1 + the player's side, 0 none). */
int PortA11y_GaugesAsked(int fight) {
    static int every = -1;
    int ask;

    if (every < 0) {
        every = getenv("BT3_A11Y_ASK") != NULL ? atoi(getenv("BT3_A11Y_ASK")) : 0;
    }
    SDL_SetAtomicInt(&sFight, fight);
    ask = SDL_SetAtomicInt(&sAsk, 0);
    if (ask == 0 && every > 0 && sFrame % every == 0) {
        ask = 1 + (sFrame / every & 1);
    }
    return ask;
}

/* ---- the fight's cues ---- */

#define CUE_RATE 48000
#define CUE_MAX (CUE_RATE * 450 / 1000) /* the longest cue */
#define CUE_EDGE (CUE_RATE * 5 / 1000)  /* (no cue starts at full strength: no click) */
#define CUE_TAU 6.2831853f

extern SDL_AudioDeviceID Port_AudioDevice(void); /* snd_adx.c */

static SDL_AudioStream *sCue;
static int sCueTried;

/* The sound of a cue, -1..1, into out (CUE_MAX samples); returns how many. Two that cannot be taken for each other:
   kind 0 (a transformation) a tone that slides up from E4 to B5 and holds a moment, like powering up; kind 1 (the
   Ultimate Blast) a small bell struck twice on the same note, A5. */
static int cue_wave(int kind, float *out) {
    int len = kind == 0 ? CUE_RATE * 320 / 1000 : CUE_RATE * 420 / 1000, glide = CUE_RATE * 240 / 1000, i, k, t;
    float phase = 0.0f, s, x, f;

    for (i = 0; i < len; i++) {
        if (kind == 0) {
            /* three times the pitch over the glide, then held; the phase is summed since the pitch moves */
            f = 329.63f * powf(3.0f, (float)(i < glide ? i : glide) / glide);
            phase += CUE_TAU * f / CUE_RATE;
            phase -= phase > CUE_TAU ? CUE_TAU : 0.0f;
            s = 0.5f * (sinf(phase) + 0.3f * sinf(2.0f * phase) + 0.1f * sinf(3.0f * phase));
        } else {
            /* a bell: its partials (1, 2.76 and 5.40 times the note) fall away, the high ones faster */
            s = 0.0f;
            for (k = 0; k < 2; k++) {
                if ((t = i - k * (CUE_RATE * 120 / 1000)) < 0) {
                    continue;
                }
                x = (float)t / CUE_RATE;
                s += (t < CUE_EDGE ? (float)t / CUE_EDGE : 1.0f) *
                     (expf(-9.0f * x) * sinf(CUE_TAU * 880.0f * x) + 0.35f * expf(-18.0f * x) * sinf(CUE_TAU * 2428.8f * x) +
                      0.15f * expf(-30.0f * x) * sinf(CUE_TAU * 4752.0f * x));
            }
            s *= 0.7f;
        }
        /* in over the first 5 ms and out over the last 50 */
        s *= (i < CUE_EDGE ? (float)i / CUE_EDGE : 1.0f) * (len - i < CUE_EDGE * 10 ? (float)(len - i) / (CUE_EDGE * 10) : 1.0f);
        out[i] = s > 1.0f ? 1.0f : s < -1.0f ? -1.0f : s;
    }
    return len;
}

/* The cue as it is played: 16-bit stereo at CUE_RATE, on the left for player 1 (side 0) and on the right for player
   2, the other side at a fifth; into out (CUE_MAX frames). Returns the frames. */
static int cue_pcm(int side, int kind, int16_t *out) {
    static float wave[CUE_MAX];
    float loud = 0.25f * (float)sCueVolume / 100.0f, soft = loud * 0.2f;
    int len = cue_wave(kind, wave), i;

    for (i = 0; i < len; i++) {
        out[i * 2] = (int16_t)(wave[i] * (side == 0 ? loud : soft) * 32767.0f);
        out[i * 2 + 1] = (int16_t)(wave[i] * (side == 0 ? soft : loud) * 32767.0f);
    }
    return len;
}

/* A cue for a11y_game.c (the game's thread), made here, not one of the game's sounds: kind 0, one transformation
   more can be taken; kind 1, the Ultimate Blast can be fired. Two at the same time sound one after the other. */
void PortA11y_Cue(int side, int kind) {
    static int16_t buf[CUE_MAX * 2];
    SDL_AudioDeviceID dev;
    SDL_AudioSpec spec;

    if (!sOn || sCueVolume <= 0 || (unsigned)kind > 1) {
        return;
    }
    if (sCue == NULL && !sCueTried) {
        if ((dev = Port_AudioDevice()) == 0) {
            return; /* (no sound device, or none yet) */
        }
        sCueTried = 1;
        spec.format = SDL_AUDIO_S16;
        spec.channels = 2;
        spec.freq = CUE_RATE;
        if ((sCue = SDL_CreateAudioStream(&spec, NULL)) != NULL && !SDL_BindAudioStream(dev, sCue)) {
            SDL_DestroyAudioStream(sCue);
            sCue = NULL;
        }
    }
    if (sCue != NULL) {
        SDL_PutAudioStreamData(sCue, buf, cue_pcm(side, kind, buf) * 4);
    }
}

/* One of the game's strings (UTF-16 in the machine's order, U+FEFF first, '\n' between lines) as UTF-8 on one
   line, into out (cap bytes with the terminator); out of English, the line from a11y_<code>.txt instead when it has one.
   Returns the length. */
int PortA11y_Utf8(char *out, int cap, const uint16_t *str) {
    int n = 0, i, space = 0, need;
    uint32_t c;

    for (i = 0; str[i] != 0 && i < 4096; i++) {
        c = str[i];
        if (c >= 0xD800 && c < 0xDC00 && str[i + 1] >= 0xDC00 && str[i + 1] < 0xE000) {
            c = 0x10000 + ((c - 0xD800) << 10) + (str[i + 1] - 0xDC00);
            i++;
        } else if (c == 0xFEFF || (c >= 0xD800 && c < 0xE000)) {
            continue;
        }
        if (c <= ' ') { /* line breaks and spaces: one space between words */
            space = n > 0;
            continue;
        }
        need = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
        if (n + space + need >= cap) {
            break;
        }
        if (space) {
            out[n++] = ' ';
            space = 0;
        }
        if (need == 1) {
            out[n++] = (char)c;
        } else if (need == 2) {
            out[n++] = (char)(0xC0 | c >> 6);
            out[n++] = (char)(0x80 | (c & 0x3F));
        } else if (need == 3) {
            out[n++] = (char)(0xE0 | c >> 12);
            out[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        } else {
            out[n++] = (char)(0xF0 | c >> 18);
            out[n++] = (char)(0x80 | (c >> 12 & 0x3F));
            out[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    if (cap > 0) {
        out[n] = '\0';
    }
    return own_words(out, translated_line(out, cap, n), cap);
}
