/* Fast base-data identification, once per process. R labels are release metadata. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../include/bt4_online_compat.h"
#include "game_profile.h"
#include "../../include/bt4_dev_rules.h"
/* Local requests are encoded in the normal input history. The simulation never
   reads UI settings directly, including during rollback or on the sending peer. */
static unsigned sDevRequestedFlags;static uint32_t sDevRequestedSequence;
static SDL_SpinLock sDevRuleLock;
unsigned Port_DevRulesRequested(void){
 unsigned flags;SDL_LockSpinlock(&sDevRuleLock);flags=sDevRequestedFlags;SDL_UnlockSpinlock(&sDevRuleLock);return flags;
}
void Port_DevRulesRequest(unsigned flags,int step){
 SDL_LockSpinlock(&sDevRuleLock);
 sDevRequestedFlags=flags&15;if(++sDevRequestedSequence==0)++sDevRequestedSequence;
 if(step)sDevRequestedFlags|=BT4_DEV_PAUSE|BT4_DEV_STEP;
 SDL_UnlockSpinlock(&sDevRuleLock);
}
void Port_DevRulesEncode(unsigned char out[8]){

 unsigned flags;uint32_t seq;
 SDL_LockSpinlock(&sDevRuleLock);flags=sDevRequestedFlags;seq=sDevRequestedSequence;SDL_UnlockSpinlock(&sDevRuleLock);
 if(seq)Bt4Dev_Pack(out,flags,seq);
}
extern FILE *Port_FileOpenHost(const char *,char *,size_t);
static int sChecked,sCompatible;
static char sReason[192];
static unsigned sCrcTable[256];
const char *Port_BuildIdentitySummary(void){return sReason;}
int Port_BuildIdentity(unsigned long long *out){
    unsigned char buffer[65536];char path[2048];size_t i,n;unsigned bytes=0;
    unsigned long long began=SDL_GetPerformanceCounter();
    if(!sChecked){
        unsigned k;int language=Port_GameProfileEnglish();sChecked=1;sCompatible=1;
        for(i=0;i<256;++i){unsigned c=(unsigned)i;for(k=0;k<8;++k)c=(c>>1)^((c&1)?0xedb88320u:0);sCrcTable[i]=c;}
        for(i=0;i<sizeof(kBt4BaseFiles)/sizeof(kBt4BaseFiles[0]);++i){
            const Bt4BaseFile *entry=&kBt4BaseFiles[i];unsigned crc=~0u,size=0;FILE *f=Port_FileOpenHost(entry->path,path,sizeof(path));
            if(!f){snprintf(sReason,sizeof(sReason),"Required BT4 base file is missing: %s",entry->path);sCompatible=0;break;}
            while((n=fread(buffer,1,sizeof(buffer),f))!=0){
                size_t j;size+=(unsigned)n;bytes+=(unsigned)n;
                /* Stop before reading an arbitrarily large replacement. */
                if(size>entry->size[language])break;
                for(j=0;j<n;++j)crc=sCrcTable[(crc^buffer[j])&255]^(crc>>8);
            }
            if(ferror(f)||size!=entry->size[language]||(~crc)!=entry->crc[language]){
                snprintf(sReason,sizeof(sReason),"Unsupported or damaged BT4 base file: %s",entry->path);sCompatible=0;
            }
            fclose(f);if(!sCompatible)break;
        }
        fprintf(stderr,"bt4 online compatibility: family=%s protocol=%08x language=%s base-files=%u bytes=%u elapsed-ms=%.2f revision-marker=ignored status=%s\n",
            BT4_ONLINE_COMPAT_NAME,0x54540009u,language?"en":"es",(unsigned)(i+(!sCompatible)),bytes,
            1000.0*(double)(SDL_GetPerformanceCounter()-began)/(double)SDL_GetPerformanceFrequency(),sCompatible?"compatible":sReason);
    }
    if(!sCompatible)return -1;
    if(out)*out=BT4_ONLINE_COMPAT_ID;
    return 1;
}
