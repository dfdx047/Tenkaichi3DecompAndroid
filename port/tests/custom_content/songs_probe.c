#include "../../include/plat_songs.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
int PortStages_Alias(const char *p,char *o,unsigned n){(void)p;(void)o;(void)n;return 0;}
long Port_FileSize(const char *p){(void)p;assert(0);return -1;}
size_t Port_FileReadAt(const char *p,long o,void *b,size_t n){(void)p;(void)o;(void)b;(void)n;assert(0);return 0;}
void Port_TestSlow(int a,int b){(void)a;(void)b;}
void Port_VBlank(void){assert(0);}
void *Port_LowAlloc(size_t n){return malloc(n);}void Port_LowFree(void *p){free(p);}
extern int Port_FilePath(int,int,const char *,char *,int);
int main(void){
    char path[512],rel[80];PortSongs_Init();assert(PortSongs_Count()>0);
    for(int i=0;i<PortSongs_Count();i++){
        int id=PORT_BGM_FILE(PortSongs_Offset(i));assert(id>=0x20000);
        snprintf(rel,sizeof(rel),"pzs4us3/%05d.bin",id-0x20000);
        assert(PortSongs_Alias(rel,path,sizeof(path)));assert(PortSongs_IsFile(path));
        assert(Port_FilePath(3,id-0x20000,NULL,path,sizeof(path)));
        FILE *f=fopen(path,"rb");assert(f);fclose(f);
        printf("song=%s global_id=%x path=%s\n",PortSongs_Name(i),id,path);
    }
    printf("song_checks=%d\n",PortSongs_Count());return 0;
}
