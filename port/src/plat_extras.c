/*
 * Where stages and songs from outside the disc are kept, and which files are there (plat_stages.c, plat_songs.c).
 *
 * A folder next to the game, like `textures`: `stages` for stage models (.unk), `songs` for music (.adx). A file
 * dropped in is used; its name in the menus is the file's name. The optional list in the folder (maps.txt,
 * songs.txt: "file|Name In The Menu" per line) gives other names and an order; files it does not mention follow,
 * by name. $BT3_STAGES / $BT3_SONGS name another folder. The pull request's first place, a folder of that name
 * inside the game's data folder, is still looked at when there is none next to the game.
 */
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

/* The folder for `kind` ("stages" or "songs") into `out`. Returns 0 if there is none. */
int PortExtras_Dir(const char *kind, const char *env, char *out, unsigned size) {
    const char *e = getenv(env), *data = getenv("BT3_DATA");

    if (e != NULL && e[0] != '\0') {
        snprintf(out, size, "%s", e);
        return is_dir(out);
    }
    snprintf(out, size, "%s", kind);
    if (is_dir(out)) {
        return 1;
    }
    snprintf(out, size, "%s/%s", data != NULL ? data : "gamedata", kind);
    return is_dir(out);
}

static int name_cmp(const void *a, const void *b) {
    const unsigned char *x = a, *y = b;
    for (; *x != '\0' && tolower(*x) == tolower(*y); x++, y++) {
    }
    return tolower(*x) != tolower(*y) ? tolower(*x) - tolower(*y) : strcmp(a, b);
}

/* The files of `dir` that end in `ext` (".unk"), by name without regard to case, into names[][128]. The order is
   the same on every machine, which a list both players of an online match must agree on would need. */
int PortExtras_Scan(const char *dir, const char *ext, char names[][128], int max) {
    DIR *d = opendir(dir);
    struct dirent *de;
    size_t el = strlen(ext);
    int n = 0;

    if (d == NULL) {
        return 0;
    }
    while ((de = readdir(d)) != NULL && n < max) {
        size_t l = strlen(de->d_name);
        size_t k;
        if (l <= el || l >= 128) {
            continue;
        }
        for (k = 0; k < el && tolower((unsigned char)de->d_name[l - el + k]) == ext[k]; k++) {
        }
        if (k == el) {
            snprintf(names[n++], 128, "%s", de->d_name);
        }
    }
    closedir(d);
    qsort(names, (size_t)n, 128, name_cmp);
    return n;
}

/* "my_cool-map.unk" -> "my cool-map": the name shown for a file the list does not name. */
void PortExtras_NameFromFile(const char *file, char *name, unsigned size) {
    const char *dot = strrchr(file, '.');
    unsigned i, n = (unsigned)(dot != NULL ? dot - file : (long)strlen(file));

    for (i = 0; i < n && i + 1 < size; i++) {
        name[i] = file[i] == '_' ? ' ' : file[i];
    }
    name[i] = '\0';
}
