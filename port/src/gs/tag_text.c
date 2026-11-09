/* Mod-owned strings, separate from the player's disc artwork and dialogue.
 * Immutable strings live in the host image (below 4 GiB), outside rollback
 * state. No allocations or changing pointers when a room restores its state. */
#include "../../include/tag_text.h"
#include "menu_translation.h"
#include <string.h>

typedef struct TagTextRow { const char *en,*tr; } TagTextRow;
static const TagTextRow esRows[]={
#include "tag_text_es.inc"
};
/* Portuguese (port/translations/tag/pt.tsv): the game's own menus have no Portuguese, so it follows the app's
   language (dr_lang 1, which the Android app writes before the game starts) rather than the menus' locale. */
static const TagTextRow ptRows[]={
#include "tag_text_pt.inc"
};

static const char *find(const TagTextRow *rows,int count,const char *english) {
    int lo=0,hi=count-1;
    while(lo<=hi) {
        int mid=(lo+hi)/2,cmp=strcmp(english,rows[mid].en);
        if(!cmp)return rows[mid].tr;
        if(cmp<0)hi=mid-1;else lo=mid+1;
    }
    return english;
}

/* The language the mod's menus show: chosen on the dragon radar's page (its switch at the top right) and kept in the
   settings as "tag_lang" (1 English, 2 Portuguese, 3 Spanish; 0: not chosen). Not chosen, as before: Portuguese when
   the app is in Portuguese (dr_lang 1), Spanish when the game's menus are, English otherwise. Only what is shown changes:
   nothing of a room or of the game's state depends on it (the online copies may each show their own language). */
static int sTagLang=-1; /* the choice (0 English, 1 Portuguese, 2 Spanish), -1 not read yet, -2 none */
static int sAppLang=-1; /* (asked once: the settings' search takes a lock, and this runs for every line drawn) */

int Port_TagLang(void) {
    extern int Port_Setting(const char *name,int def); /* plat_settings.c */
    const char *locale;
    int lang=__atomic_load_n(&sTagLang,__ATOMIC_RELAXED);
    if(lang==-1) {
        int saved=Port_Setting("tag_lang",0);
        lang=saved>=1 && saved<=3?saved-1:-2;
        __atomic_store_n(&sTagLang,lang,__ATOMIC_RELAXED);
    }
    if(lang>=0)return lang;
    if(sAppLang<0)sAppLang=Port_Setting("dr_lang",0);
    if(sAppLang==1)return 1;
    locale=MenuTranslation_Locale();
    return locale && !strcmp(locale,"es")?2:0;
}

void Port_TagLangSet(int lang) {
    extern void Port_SettingSave(const char *name,int value),Port_SettingsWrite(void);
    if(lang<0 || lang>2)return;
    __atomic_store_n(&sTagLang,lang,__ATOMIC_RELAXED);
    Port_SettingSave("tag_lang",lang+1);
    Port_SettingsWrite();
}

const char *Port_TagText(const char *english) {
    int lang;
    if(!english)return english;
    lang=Port_TagLang();
    if(lang==1)return find(ptRows,(int)(sizeof(ptRows)/sizeof(*ptRows)),english);
    if(lang==2)return find(esRows,(int)(sizeof(esRows)/sizeof(*esRows)),english);
    return english;
}
