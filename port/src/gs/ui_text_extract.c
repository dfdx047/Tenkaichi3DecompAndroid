/* Extract the game's UTF-16 text tables from menu archives at startup. */
#include "ui_text_extract.h"
#include "../../include/plat_game.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#endif

#define UT_MAX_FILE (16u * 1024 * 1024)
#define UT_MAX_UNPACKED (64u * 1024 * 1024)
#define UT_MAX_LINES 16384u

typedef struct UtBuf { const uint8_t *p; size_t n; } UtBuf;
typedef struct UtJob { char data[1200], output[1200]; } UtJob;
/* A line as the walk found it (collected, not written): its ID, its UTF-16 bytes with the byte-order mark and the
   closing zero, and how many lines its table has (for matching the two discs' tables, UiText_EuropeanOnStartup). */
typedef struct UtLine { char *id; uint8_t *text; size_t n; unsigned tableLines; } UtLine;
/* Where the walk's lines go: written as the catalog's JSON (fp), or collected (fp NULL). */
typedef struct UtWriter { FILE *fp; unsigned count; int file; UtLine *lines; unsigned nLines, capLines; int failed; } UtWriter;
static pthread_mutex_t sUtLock = PTHREAD_MUTEX_INITIALIZER;
static int sUtStarted;
static const int kUtMenuFiles[] = {5, 448, 449, 450, 451, 452, 453, 459, 460, 461, 463, 472};

static uint32_t ut_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint16_t ut_u16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)p[1] << 8; }
static int ut_offset(UtBuf b, uint32_t i, uint32_t *out) {
    if ((uint64_t)i * 4 + 4 > b.n) return 0;
    *out = ut_u32(b.p + (size_t)i * 4) & ~3u;
    return *out <= b.n;
}
static int ut_archive(UtBuf b, uint32_t *count) {
    uint32_t n, prev, off, i;
    if (b.n < 12 || (n = ut_u32(b.p)) == 0 || n > UT_MAX_LINES ||
        (uint64_t)(n + 2) * 4 > b.n || !ut_offset(b, 1, &prev) || prev < (n + 2) * 4) return 0;
    for (i = 2; i <= n + 1; i++) {
        if (!ut_offset(b, i, &off) || off < prev) return 0;
        prev = off;
    }
    *count = n;
    return 1;
}
static int ut_section(UtBuf b, uint32_t i, UtBuf *out) {
    uint32_t first, last;
    if (!ut_offset(b, i, &first) || !ut_offset(b, i + 1, &last) || last < first) return 0;
    out->p = b.p + first;
    out->n = last - first;
    return 1;
}
/* Same bounded byte-pair stream format as menu_labels.c's ml_unpack_bytes. */
static int ut_unpack(UtBuf b, UtBuf *out) {
    size_t p = 8, end, n = 0;
    uint32_t size, packed;
    uint8_t *dst;
    if (b.n < 8 || (size = ut_u32(b.p)) == 0 || size > UT_MAX_UNPACKED ||
        (packed = ut_u32(b.p + 4)) == 0 || packed > b.n - 8) return 0;
    end = 8 + packed;
    dst = malloc(size);
    if (dst == NULL) return 0;
    while (p < end) {
        uint8_t left[256], right[256], stack[256];
        size_t block, k;
        int ch = 0, i, sp;
        for (i = 0; i < 256; i++) { left[i] = (uint8_t)i; right[i] = 0; }
        while (ch < 256) {
            int count;
            if (p >= end) goto fail;
            count = b.p[p++];
            if (count > 127) { ch += count - 127; count = 0; }
            if (ch == 256) break;
            for (i = 0; i <= count; i++) {
                if (ch >= 256 || p >= end) goto fail;
                left[ch] = b.p[p++];
                if (ch != left[ch]) { if (p >= end) goto fail; right[ch] = b.p[p++]; }
                ch++;
            }
        }
        if (end - p < 2) goto fail;
        block = (size_t)b.p[p] * 256 + b.p[p + 1]; p += 2;
        if (end - p < block) goto fail;
        for (k = 0; k < block; k++) {
            sp = 0; stack[sp++] = b.p[p + k];
            while (sp) {
                uint8_t v = stack[--sp];
                if (left[v] == v) { if (n >= size) goto fail; dst[n++] = v; }
                else { if (sp + 2 > 256) goto fail; stack[sp++] = right[v]; stack[sp++] = left[v]; }
            }
        }
        p += block;
    }
    if (p != end || n != size) goto fail;
    out->p = dst; out->n = n;
    return 1;
fail:
    free(dst);
    return 0;
}
static int ut_line(UtBuf b) {
    size_t p;
    if (b.n < 4 || b.p[0] != 0xff || b.p[1] != 0xfe) return 0;
    for (p = 2; p + 1 < b.n; p += 2) {
        uint16_t c = ut_u16(b.p + p);
        if (c == 0) return 1;
        if (c >= 0xd800 && c <= 0xdbff) {
            if (p + 3 >= b.n || ut_u16(b.p + p + 2) < 0xdc00 || ut_u16(b.p + p + 2) > 0xdfff) return 0;
            p += 2;
        } else if (c >= 0xdc00 && c <= 0xdfff) return 0;
    }
    return 0;
}
static void ut_json_string(FILE *fp, UtBuf b) {
    size_t p;
    fputc('"', fp);
    for (p = 2; p + 1 < b.n; p += 2) {
        uint32_t c = ut_u16(b.p + p);
        if (!c) break;
        if (c >= 0xd800 && c <= 0xdbff) { c = 0x10000 + ((c - 0xd800) << 10) + ut_u16(b.p + p + 2) - 0xdc00; p += 2; }
        if (c == '"' || c == '\\') { fputc('\\', fp); fputc((int)c, fp); }
        else if (c < 0x20) fprintf(fp, "\\u%04x", c);
        else if (c < 0x80) fputc((int)c, fp);
        else if (c < 0x800) { fputc(0xc0 | (c >> 6), fp); fputc(0x80 | (c & 63), fp); }
        else if (c < 0x10000) { fputc(0xe0 | (c >> 12), fp); fputc(0x80 | ((c >> 6) & 63), fp); fputc(0x80 | (c & 63), fp); }
        else { fputc(0xf0 | (c >> 18), fp); fputc(0x80 | ((c >> 12) & 63), fp); fputc(0x80 | ((c >> 6) & 63), fp); fputc(0x80 | (c & 63), fp); }
    }
    fputc('"', fp);
}
/* A line found: into the catalog, or kept (a copy: the walk frees what it unpacked). */
static void ut_emit(UtWriter *w, const char *id, UtBuf line, unsigned tableLines) {
    UtLine *l;
    size_t n;
    if (w->fp != NULL) {
        fprintf(w->fp, "%s    \"%s\": {\"original\": ", w->count++ ? ",\n" : "", id);
        ut_json_string(w->fp, line);
        fputs(", \"text\": ", w->fp);
        ut_json_string(w->fp, line);
        fputc('}', w->fp);
        return;
    }
    if (w->failed) return;
    for (n = 2; n + 1 < line.n && ut_u16(line.p + n) != 0; n += 2) {}
    n += 2; /* (the closing zero: ut_line has seen it) */
    if (w->nLines == w->capLines) {
        unsigned cap = w->capLines ? w->capLines * 2 : 4096;
        UtLine *more = realloc(w->lines, cap * sizeof(*more));
        if (more == NULL) { w->failed = 1; return; }
        w->lines = more;
        w->capLines = cap;
    }
    l = &w->lines[w->nLines];
    l->id = strdup(id);
    l->text = malloc(n);
    if (l->id == NULL || l->text == NULL) { free(l->id); free(l->text); w->failed = 1; return; }
    memcpy(l->text, line.p, n);
    l->n = n;
    l->tableLines = tableLines;
    w->nLines++;
}
static void ut_walk(UtWriter *w, UtBuf b, const char *path, int depth) {
    uint32_t n, i, lines = 0;
    UtBuf line;
    if (depth > 4 || !ut_archive(b, &n)) return;
    for (i = 1; i <= n; i++) if (!ut_section(b, i, &line) || !ut_line(line)) break;
    if (i > n) {
        for (i = 1; i <= n; i++) {
            char id[128];
            ut_section(b, i, &line);
            if (snprintf(id, sizeof(id), "%s/%u", path, i - 1) >= (int)sizeof(id)) return;
            ut_emit(w, id, line, n);
        }
        return;
    }
    for (i = 1; i <= n; i++) lines += ut_section(b, i, &line) && ut_line(line);
    for (i = 1; i <= n; i++) {
        UtBuf part, unpacked;
        uint32_t childCount;
        char child[128];
        if (ut_section(b, i, &part) && ut_line(part)) { /* a line among other sections (the HUD's file 5: the pause menu) */
            char id[128];
            if (snprintf(id, sizeof(id), "%s/%u", path, i - 1) >= (int)sizeof(id)) continue;
            ut_emit(w, id, part, lines);
            continue;
        }
        if (!ut_section(b, i, &part) || part.n < 12 ||
            snprintf(child, sizeof(child), "%s/%u", path, i) >= (int)sizeof(child)) continue;
        if (ut_archive(part, &childCount)) { ut_walk(w, part, child, depth + 1); }
        else if (ut_unpack(part, &unpacked)) { ut_walk(w, unpacked, child, depth + 1); free((void *)unpacked.p); }
    }
}
static uint8_t *ut_read(const char *path, size_t *size) {
    FILE *fp = fopen(path, "rb");
    long n;
    uint8_t *data;
    if (fp == NULL) return NULL;
    if (fseek(fp, 0, SEEK_END) || (n = ftell(fp)) < 12 || n > UT_MAX_FILE || fseek(fp, 0, SEEK_SET)) { fclose(fp); return NULL; }
    data = malloc((size_t)n);
    if (data == NULL) { fclose(fp); return NULL; }
    if (fread(data, 1, (size_t)n, fp) != (size_t)n) { free(data); fclose(fp); return NULL; }
    fclose(fp); *size = (size_t)n; return data;
}
/* What an extracted catalog says it is. Version 2 has the lines among other sections too (the pause menu). */
#define UT_SOURCE "extracted USA game text, version 2"
/* An English catalog this program extracted in an earlier version (never one a person wrote or edited: those name
   another source). */
static int ut_stale(const char *path) {
    char head[512];
    size_t n;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return 0;
    n = fread(head, 1, sizeof(head) - 1, fp);
    fclose(fp);
    head[n] = 0;
    return strstr(head, "\"source\": \"extracted USA game text\"") != NULL;
}
static void *ut_worker(void *arg) {
    UtJob *job = arg;
    char temp[1280], path[1280];
    UtWriter w = {0};
    size_t size;
    int i;
    if (snprintf(temp, sizeof(temp), "%s.part", job->output) >= (int)sizeof(temp)) goto done;
    w.fp = fopen(temp, "wb");
    if (w.fp == NULL) goto done;
    fputs("{\n  \"schema_version\": 1,\n  \"labels_version\": 1,\n  \"translation_version\": 1,\n  \"locale\": \"en\",\n  \"name\": \"English\",\n  \"source\": \"" UT_SOURCE "\",\n  \"labels\": {\n", w.fp);
    for (i = 0; i < (int)(sizeof(kUtMenuFiles) / sizeof(kUtMenuFiles[0])); i++) {
        uint8_t *data;
        UtBuf b;
        if (snprintf(path, sizeof(path), "%s/%s/%05d.bin", job->data, PortGame_IsBT4()?"pzs4us1":"pzs3us1", kUtMenuFiles[i]) >= (int)sizeof(path)) continue;
        data = ut_read(path, &size);
        if (data == NULL) continue;
        b.p = data; b.n = size; w.file = kUtMenuFiles[i];
        { char id[32]; snprintf(id, sizeof(id), "pzs3us1/%05d", w.file); ut_walk(&w, b, id, 0); }
        free(data);
    }
    fprintf(w.fp, "\n  },\n  \"labels_count\": %u\n}\n", w.count);
    if (fclose(w.fp) == 0 && w.count > 0) {
#ifdef _WIN32
        if (access(job->output, 0) != 0 && rename(temp, job->output) == 0) goto done;
#else
        /* link is atomic and cannot overwrite a user's catalog created meanwhile. */
        if (link(temp, job->output) == 0) { remove(temp); goto done; }
#endif
    }
    remove(temp);
done:
    free(job);
    return NULL;
}
void UiText_ExtractOnStartup(const char *gameTextDir) {
    const char *data = getenv("BT3_DATA");
    UtJob *job;
    pthread_t thread;
    char parent[1200], dir[1200];
    if (gameTextDir == NULL) return;
    pthread_mutex_lock(&sUtLock);
    if (sUtStarted) { pthread_mutex_unlock(&sUtLock); return; }
    sUtStarted = 1;
    pthread_mutex_unlock(&sUtLock);
    job = calloc(1, sizeof(*job));
    if (job == NULL) return;
    if (snprintf(job->data, sizeof(job->data), "%s", data && data[0] ? data : "gamedata") >= (int)sizeof(job->data) ||
        snprintf(parent, sizeof(parent), "%s/translations", gameTextDir) >= (int)sizeof(parent) ||
        snprintf(dir, sizeof(dir), "%s/ui", parent) >= (int)sizeof(dir) ||
        snprintf(job->output, sizeof(job->output), "%s/en.json", dir) >= (int)sizeof(job->output)) { free(job); return; }
#ifdef _WIN32
    _mkdir(gameTextDir); _mkdir(parent); _mkdir(dir);
#else
    mkdir(gameTextDir, 0755); mkdir(parent, 0755); mkdir(dir, 0755);
#endif
    if (ut_stale(job->output)) remove(job->output); /* (made by an older release: made again, with what it lacked) */
    if (access(job->output, 0) == 0 || pthread_create(&thread, NULL, ut_worker, job) != 0) { free(job); return; }
    pthread_detach(thread);
}

/* ---- the European disc's languages ------------------------------------------------------------------------------
   The European release (SLES-54945) has the menus, the battle HUD and their text once per language: English,
   Spanish, German, French, Italian. The setup (or port/tools/extract_eu.py) copies those archive entries into
   gamedata/pzs3eu1 as they are; from them the catalogs ui/es.json, de.json, fr.json and it.json are made here, keyed
   by the USA disc's IDs. A line is the same line on both discs where the European English is the USA English, or
   where its table has as many lines on both (the European English is then the USA line reworded: bullets added, a
   typo fixed, "Fight Again?"). Lines without their like stay English. */
#define UT_EU_SOURCE "European disc, version 1"
#define UT_EU_STRIDE 31  /* the menu archives: USA 448 + k is European 451 + k + 31 * language */
#define UT_EU_FIRST 451
#define UT_EU_HUD 5      /* the HUD: USA 5 is European 5 + language */
static const struct { const char *locale, *name; } kUtEuLanguages[4] = {
    {"es", "Espa\xc3\xb1ol"}, {"de", "Deutsch"}, {"fr", "Fran\xc3\xa7" "ais"}, {"it", "Italiano"}};

static void ut_lines_free(UtWriter *w) {
    unsigned i;
    for (i = 0; i < w->nLines; i++) { free(w->lines[i].id); free(w->lines[i].text); }
    free(w->lines);
    memset(w, 0, sizeof(*w));
}
static int ut_line_order(const void *a, const void *b) {
    return strcmp(((const UtLine *)a)->id, ((const UtLine *)b)->id);
}
static const UtLine *ut_line_find(const UtWriter *w, const char *id) {
    UtLine key;
    key.id = (char *)id;
    return bsearch(&key, w->lines, w->nLines, sizeof(*w->lines), ut_line_order);
}
static int ut_line_same(const UtLine *a, const UtLine *b) {
    return a->n == b->n && memcmp(a->text, b->text, a->n) == 0;
}
/* The lines of one disc: the USA one's (bank -1, gamedata/pzs3us1) or a European language bank's (0 English ...). */
static int ut_collect(const char *data, int bank, UtWriter *w) {
    int i;
    for (i = 0; i < (int)(sizeof(kUtMenuFiles) / sizeof(kUtMenuFiles[0])); i++) {
        char path[1300], id[32];
        size_t size;
        uint8_t *bytes;
        UtBuf b;
        int usa = kUtMenuFiles[i];
        int entry = bank < 0 ? usa : usa == UT_EU_HUD ? UT_EU_HUD + bank : UT_EU_FIRST + (usa - 448) + bank * UT_EU_STRIDE;
        if (snprintf(path, sizeof(path), "%s/%s/%05d.bin", data, bank < 0 ? "pzs3us1" : "pzs3eu1", entry) >= (int)sizeof(path)) return 0;
        if ((bytes = ut_read(path, &size)) == NULL) return 0;
        b.p = bytes; b.n = size;
        snprintf(id, sizeof(id), "pzs3us1/%05d", usa);
        ut_walk(w, b, id, 0);
        free(bytes);
    }
    return !w->failed && w->nLines > 0;
}
/* A catalog of ours that is current (made by this version from the disc), or one that is someone else's (written by
   a person, or by an older tool): left as it is. Missing, or ours from another version: made. */
static int ut_eu_wanted(const char *path) {
    char head[512];
    size_t n;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return 1;
    n = fread(head, 1, sizeof(head) - 1, fp);
    fclose(fp);
    head[n] = 0;
    return strstr(head, "\"source\": \"European disc, version ") != NULL && strstr(head, "\"source\": \"" UT_EU_SOURCE "\"") == NULL;
}
int UiText_EuropeanOnStartup(const char *gameTextDir, char *why, size_t whyLen) {
    const char *env = getenv("BT3_DATA");
    char data[1100], dir[1300], path[1400], temp[1420];
    UtWriter usa, eu[5];
    unsigned i, matched = 0;
    int k, want = 0, made = 0, result = -1;

    snprintf(why, whyLen, "%s", "");
    snprintf(data, sizeof(data), "%s", env != NULL && env[0] ? env : "gamedata");
    if (gameTextDir == NULL || snprintf(path, sizeof(path), "%s/pzs3eu1/%05d.bin", data, UT_EU_HUD) >= (int)sizeof(path) || access(path, 0) != 0 ||
        snprintf(dir, sizeof(dir), "%s/translations/ui", gameTextDir) >= (int)sizeof(dir)) {
        return 0; /* (no European disc taken) */
    }
    for (k = 0; k < 4; k++) {
        snprintf(path, sizeof(path), "%s/%s.json", dir, kUtEuLanguages[k].locale);
        want |= ut_eu_wanted(path) << k;
    }
    if (want == 0) return 0;
    memset(&usa, 0, sizeof(usa));
    memset(eu, 0, sizeof(eu));
    if (!ut_collect(data, -1, &usa)) {
        snprintf(why, whyLen, "the USA disc's menu archives cannot be read from %s/pzs3us1", data);
        goto done;
    }
    for (k = 0; k < 5; k++) {
        if (!ut_collect(data, k, &eu[k])) {
            snprintf(why, whyLen, "the European disc's archives in %s/pzs3eu1 are incomplete or damaged", data);
            goto done;
        }
        qsort(eu[k].lines, eu[k].nLines, sizeof(*eu[k].lines), ut_line_order);
    }
    for (i = 0; i < usa.nLines; i++) {
        const UtLine *e = ut_line_find(&eu[0], usa.lines[i].id);
        matched += e != NULL && ut_line_same(e, &usa.lines[i]);
    }
    if (matched * 10 < usa.nLines * 7) { /* (another disc's archives, numbered otherwise) */
        snprintf(why, whyLen, "the archives in %s/pzs3eu1 do not match the USA disc's (%u of %u lines)", data, matched, usa.nLines);
        goto done;
    }
#ifdef _WIN32
    { char parent[1300]; snprintf(parent, sizeof(parent), "%s/translations", gameTextDir); _mkdir(gameTextDir); _mkdir(parent); _mkdir(dir); }
#else
    { char parent[1300]; snprintf(parent, sizeof(parent), "%s/translations", gameTextDir); mkdir(gameTextDir, 0755); mkdir(parent, 0755); mkdir(dir, 0755); }
#endif
    for (k = 0; k < 4; k++) {
        FILE *fp;
        unsigned written = 0, translated = 0;
        if (!(want >> k & 1)) continue;
        snprintf(path, sizeof(path), "%s/%s.json", dir, kUtEuLanguages[k].locale);
        snprintf(temp, sizeof(temp), "%s.part", path);
        if ((fp = fopen(temp, "wb")) == NULL) {
            snprintf(why, whyLen, "%s cannot be written", temp);
            goto done;
        }
        fprintf(fp, "{\n  \"schema_version\": 1,\n  \"labels_version\": 1,\n  \"translation_version\": 1,\n  \"locale\": \"%s\",\n"
                    "  \"name\": \"%s\",\n  \"source\": \"" UT_EU_SOURCE "\",\n  \"labels\": {\n",
                kUtEuLanguages[k].locale, kUtEuLanguages[k].name);
        for (i = 0; i < usa.nLines; i++) {
            const UtLine *u = &usa.lines[i], *e0 = ut_line_find(&eu[0], u->id), *e = ut_line_find(&eu[k + 1], u->id);
            UtBuf original = {u->text, u->n}, text = original;
            if (e != NULL && e0 != NULL && (ut_line_same(e0, u) || e0->tableLines == u->tableLines)) {
                text.p = e->text;
                text.n = e->n;
                translated += !ut_line_same(e, u);
            }
            fprintf(fp, "%s    \"%s\": {\"original\": ", written++ ? ",\n" : "", u->id);
            ut_json_string(fp, original);
            fputs(", \"text\": ", fp);
            ut_json_string(fp, text);
            fputc('}', fp);
        }
        fprintf(fp, "\n  },\n  \"labels_count\": %u\n}\n", written);
        if (fclose(fp) != 0) {
            remove(temp);
            snprintf(why, whyLen, "%s cannot be written", temp);
            goto done;
        }
        remove(path); /* (Windows' rename does not replace) */
        if (rename(temp, path) != 0) {
            remove(temp);
            snprintf(why, whyLen, "%s cannot be written", path);
            goto done;
        }
        made++;
        fprintf(stderr, "bt3: European disc: %s, %u of %u lines translated\n", path, translated, written);
    }
    result = made;
    snprintf(why, whyLen, "%d languages made", made);
done:
    ut_lines_free(&usa);
    for (k = 0; k < 5; k++) ut_lines_free(&eu[k]);
    return result;
}
