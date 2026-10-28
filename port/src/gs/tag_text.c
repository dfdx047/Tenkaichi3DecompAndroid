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

const char *Port_TagText(const char *english) {
    extern int Port_Setting(const char *name,int def); /* plat_settings.c */
    static int appLang=-1; /* (asked once: the settings' search takes a lock, and this runs for every line drawn) */
    const char *locale=MenuTranslation_Locale();
    if(!english)return english;
    if(appLang<0)appLang=Port_Setting("dr_lang",0);
    if(appLang==1)return find(ptRows,(int)(sizeof(ptRows)/sizeof(*ptRows)),english);
    if(!locale || strcmp(locale,"es"))return english;
    return find(esRows,(int)(sizeof(esRows)/sizeof(*esRows)),english);
}
