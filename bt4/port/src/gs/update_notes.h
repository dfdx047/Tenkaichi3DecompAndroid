#ifndef BT4_UPDATE_NOTES_H
#define BT4_UPDATE_NOTES_H
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Release authors can provide concise localized notes in hidden HTML comments:
   <!-- bt4-update:es\n...\n-->. The visible release body remains the fallback. */
static void UpdNotes_Select(const char *body,const char *locale,char *out,size_t cap) {
    const char *start,*end;
    char marker[40];
    size_t n;
    if(!cap)return;
    out[0]=0;
    if(!body)return;
    snprintf(marker,sizeof(marker),"<!-- bt4-update:%s",locale);
    start=strstr(body,marker);
    if(start && (start[strlen(marker)]=='\n'||start[strlen(marker)]=='\r') &&
       (end=strstr(start+strlen(marker),"-->"))!=NULL) {
        start+=strlen(marker);
        if(*start=='\r')++start;
        if(*start=='\n')++start;
    } else {
        start=body;
        end=strstr(body,"<!-- bt4-update:");
        if(!end)end=body+strlen(body);
    }
    while(end>start&&(end[-1]=='\n'||end[-1]=='\r'||end[-1]==' '))--end;
    n=(size_t)(end-start);
    if(n>=cap){
        n=cap-1;
        while(n>0&&(((unsigned char)start[n]&0xC0)==0x80))--n;
    }
    memcpy(out,start,n);out[n]=0;
}

/* Actual rendered menu pages, independent of the progress mode left over from
   the previous menu during a fight. Character/course selectors are distinct. */
static int UpdUi_MenuAllowed(int inMenu,int page) {
    if(!inMenu||page<2||page==50||page==51)return 0;
    switch(page){
    case 3: case 15: case 19: case 23: case 24:return 0;
    default:return 1;
    }
}
#endif
