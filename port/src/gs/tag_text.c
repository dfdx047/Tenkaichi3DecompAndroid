/* Mod-owned strings, separate from the player's disc artwork and dialogue.
 * Immutable strings live in the host image (below 4 GiB), outside rollback
 * state. No allocations or changing pointers when a room restores its state. */
#include "../../include/tag_text.h"
#include "menu_translation.h"
#include <string.h>

typedef struct TagTextRow { const char *en,*es; } TagTextRow;
static const TagTextRow rows[]={
#include "tag_text_es.inc"
};

const char *Port_TagText(const char *english) {
    const char *locale=MenuTranslation_Locale();
    int lo=0,hi=(int)(sizeof(rows)/sizeof(*rows))-1;
    if(!english || !locale || strcmp(locale,"es"))return english;
    while(lo<=hi) {
        int mid=(lo+hi)/2,cmp=strcmp(english,rows[mid].en);
        if(!cmp)return rows[mid].es;
        if(cmp<0)hi=mid-1;else lo=mid+1;
    }
    return english;
}
