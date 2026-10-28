/* Runtime lookup for the disc's UTF-16 UI and dialog strings. The source catalog retains
   the original table IDs; each locale supplies its own text for those IDs. */
#include "ui_text_translate.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Font routines in the 64-bit port still store text pointers in 32-bit fields: a translated line must lie below
   4 GB. It must also stay out of the game's state. Port_LowAlloc's region is part of every saved state, and an
   online session puts the game back to its first blank, from before this catalog was loaded: its lines were
   overwritten and the pause menu read "!! !" and "dd". This file's variables are not part of the state
   (port/tools/make_state.py leaves the gs_ files out) and the program lies below 4 GB, so the lines go into a
   region of this file's own: handed out in order, given back as a whole (ui_clear). About 2.6 MB a language with the mode and story tables. */
#ifndef UI_TEXT_HOST_ALLOC
#define UI_ARENA (16u * 1024 * 1024)
static uint8_t sArena[UI_ARENA] __attribute__((aligned(16)));
static size_t sArenaUsed;
static void *ui_translated_alloc(size_t size) {
    void *p;
    size = (size + 15) & ~(size_t)15;
    if (size > UI_ARENA - sArenaUsed) return NULL;
    p = sArena + sArenaUsed;
    sArenaUsed += size;
    return p;
}
static void ui_translated_free(void *p) { (void)p; } /* (all of it at once: ui_clear) */
#else
#define ui_translated_alloc malloc
#define ui_translated_free free
#endif

#define UI_MAX_FILE (32u * 1024 * 1024)
/* The full USA catalog has 47,533 rows. Keep word slots above the row bound so probing always terminates. */
#define UI_MAX_ROWS 65536
#define UI_MAX_GROUPS 1024
#define UI_MAX_LINE 16384
#define UI_MAX_UNITS 4096
#define UI_WORD_SLOTS 131072

typedef struct UiParser { const char *p, *end; } UiParser;
typedef struct UiRow { char *id; uint16_t *original, *translated; } UiRow;
typedef struct UiGroup {
    char *id;
    UiRow **rows;
    int count;
    uint64_t hash;
} UiGroup;
typedef struct UiWord {
    const uint16_t *original, *translated;
    uint64_t hash;
    int ambiguous;
} UiWord;

static UiRow **sRows;
static UiGroup *sGroups;
static UiWord *sWords;
static const uint16_t **sOwn;
static int sRowCount, sGroupCount, sOwnCount, sReady;

static void ui_space(UiParser *p) {
    while (p->p < p->end && (*p->p == ' ' || *p->p == '\n' || *p->p == '\r' || *p->p == '\t')) p->p++;
}
static int ui_take(UiParser *p, char c) {
    ui_space(p);
    if (p->p >= p->end || *p->p != c) return 0;
    p->p++;
    return 1;
}
static int ui_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int ui_u4(UiParser *p, unsigned *out) {
    int i, h;
    *out = 0;
    for (i = 0; i < 4; i++) {
        if (p->p >= p->end || (h = ui_hex(*p->p++)) < 0) return 0;
        *out = (*out << 4) | (unsigned)h;
    }
    return 1;
}
static char *ui_string(UiParser *p) {
    char *out, *w;
    const char *q;
    if (!ui_take(p, '"')) return NULL;
    /* As much as the string can need: up to its closing quote (an escape keeps the next character from closing it;
       decoded, an escape is never longer than written). It was the rest of the file: with 20,000 lines kept, tens of
       gigabytes asked for in all. Linux and Wine give such memory only when it is written to; Windows counts all of
       it against memory and the page file, and on a machine with less of them the catalog was refused ("invalid UI
       translation catalog": the memory card box in English). */
    for (q = p->p; q < p->end && *q != '"'; q += *q == '\\' && q + 1 < p->end ? 2 : 1) {
    }
    out = malloc((size_t)(q - p->p) + 1);
    if (out == NULL) return NULL;
    w = out;
    while (p->p < p->end) {
        unsigned char c = (unsigned char)*p->p++;
        if (c == '"') { *w = 0; return out; }
        if (c < 32) break;
        if (c == '\\') {
            unsigned u, low;
            if (p->p >= p->end) break;
            c = (unsigned char)*p->p++;
            if (c == 'u') {
                if (!ui_u4(p, &u)) break;
                if (u >= 0xd800 && u <= 0xdbff) {
                    if (p->end - p->p < 2 || p->p[0] != '\\' || p->p[1] != 'u') break;
                    p->p += 2;
                    if (!ui_u4(p, &low) || low < 0xdc00 || low > 0xdfff) break;
                    u = 0x10000 + ((u - 0xd800) << 10) + low - 0xdc00;
                } else if (u >= 0xdc00 && u <= 0xdfff) break;
                if (!u) break;
                if (u < 0x80) *w++ = (char)u;
                else if (u < 0x800) { *w++ = (char)(0xc0 | (u >> 6)); *w++ = (char)(0x80 | (u & 63)); }
                else if (u < 0x10000) { *w++ = (char)(0xe0 | (u >> 12)); *w++ = (char)(0x80 | ((u >> 6) & 63)); *w++ = (char)(0x80 | (u & 63)); }
                else { *w++ = (char)(0xf0 | (u >> 18)); *w++ = (char)(0x80 | ((u >> 12) & 63)); *w++ = (char)(0x80 | ((u >> 6) & 63)); *w++ = (char)(0x80 | (u & 63)); }
                continue;
            }
            if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
            else if (c == 'b') c = '\b'; else if (c == 'f') c = '\f';
            else if (c != '"' && c != '\\' && c != '/') break;
        }
        *w++ = (char)c;
    }
    free(out);
    return NULL;
}
static int ui_skip(UiParser *p, int depth) {
    char *s, close;
    if (depth > 16) return 0;
    ui_space(p);
    if (p->p >= p->end) return 0;
    if (*p->p == '"') { s = ui_string(p); if (!s) return 0; free(s); return 1; }
    if (*p->p == '{' || *p->p == '[') {
        close = *p->p++ == '{' ? '}' : ']';
        if (ui_take(p, close)) return 1;
        do {
            if (close == '}') { s = ui_string(p); if (!s || !ui_take(p, ':')) { free(s); return 0; } free(s); }
            if (!ui_skip(p, depth + 1)) return 0;
            if (ui_take(p, close)) return 1;
        } while (ui_take(p, ','));
        return 0;
    }
    { const char *start = p->p;
      while (p->p < p->end && *p->p != ',' && *p->p != '}' && *p->p != ']' && *p->p != ' ' && *p->p != '\n' && *p->p != '\r') p->p++;
      return p->p > start; }
}
static uint16_t *ui_utf16(const char *s, int translated) {
    const unsigned char *p = (const unsigned char *)s;
    uint16_t *out, *w;
    size_t bytes = strlen(s);
    if (bytes > UI_MAX_UNITS * 3) return NULL;
    out = translated ? ui_translated_alloc((bytes + 3) * sizeof(*out)) : malloc((bytes + 3) * sizeof(*out));
    if (!out) return NULL;
    w = out; *w++ = 0xfeff;
    while (*p) {
        uint32_t c;
        if (*p < 0x80) c = *p++;
        else {
            int extra, i;
            uint32_t min;
            if ((*p & 0xe0) == 0xc0) { c = *p++ & 0x1f; extra = 1; min = 0x80; }
            else if ((*p & 0xf0) == 0xe0) { c = *p++ & 0x0f; extra = 2; min = 0x800; }
            else if ((*p & 0xf8) == 0xf0) { c = *p++ & 7; extra = 3; min = 0x10000; }
            else goto fail;
            for (i = 0; i < extra; i++) { if (!*p || (*p & 0xc0) != 0x80) goto fail; c = (c << 6) | (*p++ & 0x3f); }
            if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) goto fail;
        }
        if ((size_t)(w - out) >= UI_MAX_UNITS || c == 0xfeff) goto fail;
        if (c < 0x10000) *w++ = (uint16_t)c;
        else { c -= 0x10000; *w++ = (uint16_t)(0xd800 | (c >> 10)); *w++ = (uint16_t)(0xdc00 | (c & 0x3ff)); }
    }
    *w = 0;
    return out;
fail:
    if (translated) ui_translated_free(out); else free(out);
    return NULL;
}
static int ui_equal(const uint16_t *a, const uint16_t *b) {
    int i;
    if (!a || !b) return a == b;
    for (i = 0; i < UI_MAX_UNITS; i++) { if (a[i] != b[i]) return 0; if (!a[i]) return 1; }
    return 0;
}
static uint64_t ui_hash_char(uint64_t h, uint16_t c) {
    h = (h ^ (c & 255)) * UINT64_C(1099511628211);
    return (h ^ (c >> 8)) * UINT64_C(1099511628211);
}
static uint64_t ui_hash_line(uint64_t h, const uint16_t *s) {
    int i;
    if (!s || *s != 0xfeff) return 0;
    for (i = 0; i < UI_MAX_UNITS; i++) {
        uint16_t c = s[i]; h = ui_hash_char(h, c);
        if (!c) return h;
    }
    return 0;
}
static UiGroup *ui_group(const char *id, size_t len) {
    int i;
    UiGroup *g;
    for (i = 0; i < sGroupCount; i++) if (strlen(sGroups[i].id) == len && strncmp(sGroups[i].id, id, len) == 0) return &sGroups[i];
    if (sGroupCount >= UI_MAX_GROUPS) return NULL;
    g = &sGroups[sGroupCount++];
    g->id = malloc(len + 1);
    if (!g->id) return NULL;
    memcpy(g->id, id, len); g->id[len] = 0;
    return g;
}
static int ui_add_row(char *id, uint16_t *original, uint16_t *translated) {
    const char *slash = strrchr(id, '/');
    char *end;
    long index;
    UiGroup *g;
    UiRow *r;
    UiRow **larger;
    int count;
    if (!slash || slash == id || !slash[1] || sRowCount >= UI_MAX_ROWS) return 0;
    index = strtol(slash + 1, &end, 10);
    if (*end || index < 0 || index >= UI_MAX_LINE) return 0;
    g = ui_group(id, (size_t)(slash - id));
    if (!g) return 0;
    if (index >= g->count) {
        count = (int)index + 1;
        larger = realloc(g->rows, (size_t)count * sizeof(*larger));
        if (!larger) return 0;
        memset(larger + g->count, 0, (size_t)(count - g->count) * sizeof(*larger));
        g->rows = larger; g->count = count;
    }
    if (g->rows[index]) return 0;
    r = malloc(sizeof(*r));
    if (!r) return 0;
    r->id = id; r->original = original; r->translated = translated;
    g->rows[index] = r;
    sRows[sRowCount++] = r;
    return 1;
}
static int ui_parse_row(UiParser *p, char *id) {
    char *key, *original = NULL, *text = NULL;
    uint16_t *a = NULL, *b = NULL;
    int closed = 0;
    if (!ui_take(p, '{')) goto fail;
    if (ui_take(p, '}')) closed = 1;
    else do {
        key = ui_string(p);
        if (!key || !ui_take(p, ':')) { free(key); goto fail; }
        if (!strcmp(key, "original")) { free(original); original = ui_string(p); if (!original) { free(key); goto fail; } }
        else if (!strcmp(key, "text")) { free(text); text = ui_string(p); if (!text) { free(key); goto fail; } }
        else if (!ui_skip(p, 0)) { free(key); goto fail; }
        free(key);
        if (ui_take(p, '}')) { closed = 1; break; }
    } while (ui_take(p, ','));
    if (!closed || !original || !text || !(a = ui_utf16(original, 0))) goto fail;
    if (*text && strcmp(text, original) != 0 && !(b = ui_utf16(text, 1))) goto fail;
    free(original); free(text);
    original = text = NULL; /* (a row that cannot be added, a duplicate ID: freed once, below) */
    if (ui_add_row(id, a, b)) return 1;
fail:
    free(id); free(original); free(text); free(a); ui_translated_free(b);
    return 0;
}
static int ui_parse(const char *data, size_t size, const char *locale) {
    UiParser p = {data, data + size};
    char *key, *id;
    int schema = 0, matching = 0, labels = 0, closed;
    if (!ui_take(&p, '{')) return 0;
    if (!ui_take(&p, '}')) do {
        key = ui_string(&p);
        if (!key || !ui_take(&p, ':')) { free(key); return 0; }
        if (!strcmp(key, "labels")) {
            if (!ui_take(&p, '{')) { free(key); return 0; }
            closed = ui_take(&p, '}');
            if (!closed) do {
                id = ui_string(&p);
                if (!id || !ui_take(&p, ':')) { free(id); free(key); return 0; }
                if (!ui_parse_row(&p, id)) { free(key); return 0; }
                if (ui_take(&p, '}')) { closed = 1; break; }
            } while (ui_take(&p, ','));
            if (!closed) { free(key); return 0; }
            labels = sRowCount > 0;
        } else if (!strcmp(key, "locale")) {
            char *value = ui_string(&p);
            matching = value && !strcmp(value, locale);
            free(value);
        } else if (!strcmp(key, "schema_version")) {
            ui_space(&p);
            schema = p.p < p.end && *p.p == '1';
            if (schema) p.p++; else { free(key); return 0; }
        } else if (!ui_skip(&p, 0)) { free(key); return 0; }
        free(key);
        if (ui_take(&p, '}')) { ui_space(&p); return schema && matching && labels && p.p == p.end; }
    } while (ui_take(&p, ','));
    return 0;
}
static void ui_clear(void) {
    int i;
    for (i = 0; i < sRowCount; i++) {
        free(sRows[i]->id); free(sRows[i]->original); ui_translated_free(sRows[i]->translated); free(sRows[i]);
    }
    for (i = 0; i < sGroupCount; i++) { free(sGroups[i].id); free(sGroups[i].rows); }
    free(sRows); free(sGroups); free(sWords); free(sOwn);
    sRows = NULL; sGroups = NULL; sWords = NULL; sOwn = NULL;
    sRowCount = sGroupCount = sOwnCount = 0;
#ifndef UI_TEXT_HOST_ALLOC
    sArenaUsed = 0;
#endif
    __atomic_store_n(&sReady, 0, __ATOMIC_RELEASE);
}
static int ui_ptr_compare(const void *a, const void *b) {
    uintptr_t x = (uintptr_t)*(const void *const *)a, y = (uintptr_t)*(const void *const *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}
static int ui_prepare(void) {
    int i, j;
    sWords = calloc(UI_WORD_SLOTS, sizeof(*sWords));
    sOwn = malloc((size_t)sRowCount * sizeof(*sOwn));
    if (!sWords || !sOwn) return 0;
    for (i = 0; i < sGroupCount; i++) {
        UiGroup *g = &sGroups[i];
        uint64_t h = UINT64_C(1469598103934665603);
        h = ui_hash_char(h, (uint16_t)g->count);
        h = ui_hash_char(h, (uint16_t)(g->count >> 16));
        for (j = 0; j < g->count; j++) {
            if (!g->rows[j] || !(h = ui_hash_line(h, g->rows[j]->original))) { h = 0; break; }
        }
        g->hash = h;
    }
    for (i = 0; i < sRowCount; i++) {
        UiRow *r = sRows[i];
        uint64_t h = ui_hash_line(UINT64_C(1469598103934665603), r->original);
        UiWord *w;
        unsigned slot;
        if (!h) return 0;
        slot = (unsigned)h & (UI_WORD_SLOTS - 1);
        while (sWords[slot].original && !ui_equal(sWords[slot].original, r->original)) slot = (slot + 1) & (UI_WORD_SLOTS - 1);
        w = &sWords[slot];
        if (!w->original) { w->original = r->original; w->translated = r->translated; w->hash = h; }
        else if (!ui_equal(w->translated ? w->translated : w->original, r->translated ? r->translated : r->original)) w->ambiguous = 1;
        if (r->translated) sOwn[sOwnCount++] = r->translated;
    }
    qsort(sOwn, (size_t)sOwnCount, sizeof(*sOwn), ui_ptr_compare);
    return 1;
}
int UiText_Load(const char *gameTextDir, const char *locale) {
    char path[1200], *data;
    FILE *fp;
    long n;
    int ok;
    if (!gameTextDir || !locale || strlen(locale) > 15 ||
        strspn(locale, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-") != strlen(locale) ||
        snprintf(path, sizeof(path), "%s/translations/ui/%s.json", gameTextDir, locale) >= (int)sizeof(path)) return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) || (n = ftell(fp)) < 1 || n > UI_MAX_FILE || fseek(fp, 0, SEEK_SET)) { fclose(fp); return 0; }
    data = malloc((size_t)n + 1);
    if (!data) { fclose(fp); return 0; }
    if (fread(data, 1, (size_t)n, fp) != (size_t)n) { free(data); fclose(fp); return 0; }
    fclose(fp); data[n] = 0;
    ui_clear();
    sRows = calloc(UI_MAX_ROWS, sizeof(*sRows));
    sGroups = calloc(UI_MAX_GROUPS, sizeof(*sGroups));
    ok = sRows && sGroups && ui_parse(data, (size_t)n, locale) && ui_prepare();
    free(data);
    if (!ok) { ui_clear(); fprintf(stderr, "bt3: invalid UI translation catalog: %s\n", path); return 0; }
    if (sOwnCount == 0) { ui_clear(); return 1; }
    __atomic_store_n(&sReady, 1, __ATOMIC_RELEASE);
    return 1;
}
const uint16_t *UiText_TranslateString(const uint16_t *original) {
    uint64_t h;
    unsigned slot;
    UiWord *w;
    if (!__atomic_load_n(&sReady, __ATOMIC_ACQUIRE) || !original || *original != 0xfeff) return original;
    if (sOwnCount && bsearch(&original, sOwn, (size_t)sOwnCount, sizeof(*sOwn), ui_ptr_compare)) return original;
    h = ui_hash_line(UINT64_C(1469598103934665603), original);
    if (!h) return original;
    slot = (unsigned)h & (UI_WORD_SLOTS - 1);
    while (sWords[slot].original && !ui_equal(sWords[slot].original, original)) slot = (slot + 1) & (UI_WORD_SLOTS - 1);
    w = &sWords[slot];
    return w->original && !w->ambiguous && w->translated ? w->translated : original;
}
const uint16_t *UiText_TranslateLine(const uint32_t *table, int line, const uint16_t *original) {
    uint32_t count, i;
    uint64_t h;
    const uint16_t *candidate = NULL;
    int found = 0;
    if (!__atomic_load_n(&sReady, __ATOMIC_ACQUIRE) || !table || line < 0 || !original) return original;
    count = table[0];
    if (count == 0 || count > UI_MAX_LINE || (uint32_t)line >= count) return UiText_TranslateString(original);
    h = ui_hash_char(UINT64_C(1469598103934665603), (uint16_t)count);
    h = ui_hash_char(h, (uint16_t)(count >> 16));
    for (i = 0; i < count; i++) {
        const uint16_t *s = (const uint16_t *)((const uint8_t *)table + (table[i + 1] & ~3u));
        if (!(h = ui_hash_line(h, s))) return UiText_TranslateString(original);
    }
    for (i = 0; i < (uint32_t)sGroupCount; i++) {
        UiGroup *g = &sGroups[i];
        UiRow *r;
        const uint16_t *text;
        if (g->count != (int)count || g->hash != h || !(r = g->rows[line]) || !ui_equal(r->original, original)) continue;
        text = r->translated ? r->translated : r->original;
        if (found && !ui_equal(candidate, text)) return original;
        candidate = text;
        found = 1;
    }
    if (found) return ui_equal(candidate, original) ? original : candidate;
    return UiText_TranslateString(original);
}
