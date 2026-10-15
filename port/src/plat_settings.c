/*
 * The player's settings, kept between runs in a small text file of `name=number` lines:
 *
 *     bt3_settings.txt in the current directory, or the path in $BT3_SETTINGS (an empty $BT3_SETTINGS: no file)
 *
 * The settings overlay (port/src/gs/gs_gpu.c) writes it whenever a setting changes. At start an environment
 * variable (BT3_SCALE, BT3_ASPECT, ...) still wins over the file, so a single run can be started differently.
 * Names: scale, aspect_milli, fullscreen, fx_off, glow, music, effects, voices, stage_sounds, hud_layout, game_speed, music_follows_speed, display; the bindings of port/src/gs/gs_input.c
 * (key_p1_cross = an SDL scancode, pad_p1_cross = a controller button, padslot_p1 = which controller,
 * deadzone_p1_ls / deadzone_p1_rs = a stick's deadzone, vibration_p1 and vibration_p1_strength).
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SETTINGS 200
static struct { char name[24]; int value; } sSet[MAX_SETTINGS];
/* The few settings that are text (the player's online name): `name="text"` in the file, after the numbers. A
   release from before these reads past such a line. */
#define MAX_TEXTS 8
static struct { char name[24]; char value[64]; } sText[MAX_TEXTS];
static int sTextCount;
static int sDirty;
static int sCount;
static pthread_once_t sOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;

static const char *path(void) {
    const char *p = getenv("BT3_SETTINGS");
    return p == NULL ? "bt3_settings.txt" : p[0] == '\0' ? NULL : p;
}

static void load(void) {
    char line[128], name[24];
    int value;
    FILE *fp = path() != NULL ? fopen(path(), "r") : NULL;

    while (fp != NULL && fgets(line, sizeof(line), fp) != NULL && sCount < MAX_SETTINGS) {
        char text[64];
        if (sscanf(line, " %23[a-z0-9_]=\"%63[^\"\n]", name, text) == 2) {
            if (sTextCount < MAX_TEXTS) {
                strcpy(sText[sTextCount].name, name);
                strcpy(sText[sTextCount++].value, text);
            }
        } else if (sscanf(line, " %23[a-z0-9_]=%d", name, &value) == 2) {
            strcpy(sSet[sCount].name, name);
            sSet[sCount++].value = value;
        }
    }
    if (fp != NULL) {
        fclose(fp);
    }
}

/* The saved value of a setting, or `def` when the file does not have it. */
int Port_Setting(const char *name, int def) {
    int i, v = def;

    pthread_once(&sOnce, load);
    pthread_mutex_lock(&sLock);
    for (i = 0; i < sCount; i++) {
        if (strcmp(sSet[i].name, name) == 0) {
            v = sSet[i].value;
        }
    }
    pthread_mutex_unlock(&sLock);
    return v;
}

/* Stores a setting; Port_SettingsWrite puts the changes into the file. */
void Port_SettingSave(const char *name, int value) {
    int i;

    pthread_once(&sOnce, load);
    pthread_mutex_lock(&sLock);
    for (i = 0; i < sCount && strcmp(sSet[i].name, name) != 0; i++) {
    }
    if (i < MAX_SETTINGS) {
        if (i == sCount) {
            snprintf(sSet[sCount++].name, sizeof(sSet[0].name), "%s", name);
            sDirty = 1;
        }
        if (sSet[i].value != value) {
            sSet[i].value = value;
            sDirty = 1;
        }
    }
    pthread_mutex_unlock(&sLock);
}

/* A saved text setting copied into out (size bytes), or `def` when the file does not have it. */
void Port_SettingText(const char *name, const char *def, char *out, int size) {
    int i;

    pthread_once(&sOnce, load);
    pthread_mutex_lock(&sLock);
    snprintf(out, (size_t)size, "%s", def);
    for (i = 0; i < sTextCount; i++) {
        if (strcmp(sText[i].name, name) == 0) {
            snprintf(out, (size_t)size, "%s", sText[i].value);
        }
    }
    pthread_mutex_unlock(&sLock);
}

/* Stores a text setting (a double quote or a line break in it becomes an apostrophe or a space: the file's form). */
void Port_SettingTextSave(const char *name, const char *value) {
    char clean[64];
    int i;

    snprintf(clean, sizeof(clean), "%s", value);
    for (i = 0; clean[i] != '\0'; i++) {
        clean[i] = clean[i] == '"' ? '\'' : clean[i] == '\n' || clean[i] == '\r' ? ' ' : clean[i];
    }
    pthread_once(&sOnce, load);
    pthread_mutex_lock(&sLock);
    for (i = 0; i < sTextCount && strcmp(sText[i].name, name) != 0; i++) {
    }
    if (i < MAX_TEXTS) {
        if (i == sTextCount) {
            snprintf(sText[i].name, sizeof(sText[i].name), "%s", name);
            sText[i].value[0] = '\0';
            sTextCount++;
            sDirty = 1;
        }
        if (strcmp(sText[i].value, clean) != 0) {
            strcpy(sText[i].value, clean);
            sDirty = 1;
        }
    }
    pthread_mutex_unlock(&sLock);
}

void Port_SettingsWrite(void) {
    FILE *fp;
    int i;

    pthread_mutex_lock(&sLock);
    fp = sDirty && path() != NULL ? fopen(path(), "w") : NULL;
    if (fp != NULL) {
        for (i = 0; i < sCount; i++) {
            fprintf(fp, "%s=%d\n", sSet[i].name, sSet[i].value);
        }
        for (i = 0; i < sTextCount; i++) {
            fprintf(fp, "%s=\"%s\"\n", sText[i].name, sText[i].value);
        }
        fclose(fp);
    }
    sDirty = 0;
    pthread_mutex_unlock(&sLock);
}
