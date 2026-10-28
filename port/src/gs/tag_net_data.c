/* Compatibility checks for native rooms. BT3 reuses its gameplay fingerprint.
 * BT4 has a different resource catalogue, so compare its extracted volumes in
 * full. Cache individual content hashes by size/time/source; never identify a
 * player's game by their local path or send their files to another player. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "../../include/plat_game.h"
#define XXH_INLINE_ALL
#include "../../third_party/xxhash/xxhash.h"
extern const char *Port_FileRoot(void);
extern int Port_DataFingerprint(unsigned long long *);
extern void Gs_HoldFrame(void);

typedef struct FileHash {
    uint64_t size,modified,created,hash;
    uint32_t override,reserved;
} FileHash;

#ifdef _WIN32
/* Enumerating once avoids over 200,000 slow stat() calls on Windows. */
static int inventory(const char *root,unsigned part,unsigned count,int overlay,FileHash *out) {
    char pattern[1600];WIN32_FIND_DATAA item;HANDLE scan;
    snprintf(pattern,sizeof(pattern),"%s/%spzs4us%u/*.bin",root,overlay?"mods/":"",part);
    scan=FindFirstFileA(pattern,&item);
    if(scan==INVALID_HANDLE_VALUE)return overlay;
    do {
        char tail;unsigned id;
        if(item.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
        if(sscanf(item.cFileName,"%5u.bin%c",&id,&tail)!=1 || id>=count)continue;
        out[id].size=(uint64_t)item.nFileSizeHigh<<32|item.nFileSizeLow;
        out[id].modified=(uint64_t)item.ftLastWriteTime.dwHighDateTime<<32|item.ftLastWriteTime.dwLowDateTime;
        out[id].created=(uint64_t)item.ftCreationTime.dwHighDateTime<<32|item.ftCreationTime.dwLowDateTime;
        out[id].override=overlay;out[id].reserved=1;
    }while(FindNextFileA(scan,&item));
    FindClose(scan);return 1;
}
#endif

unsigned long long TagNet_DataIdentity(void) {
    unsigned count[4],TOTAL=0;
    const char *root=Port_FileRoot();
    unsigned part,id,n=0,changed=0;uint64_t marker=0;int oldCache=0;
    FileHash *cache,*files=NULL;XXH3_state_t all;FILE *f;char path[1600],cachePath[1600];
    unsigned char *buffer;Uint64 began=SDL_GetTicks();
    if(!PortGame_IsBT4()) {
        unsigned long long h=0;
        while(Port_DataFingerprint(&h)!=1 && SDL_GetTicks()-began<120000){Gs_HoldFrame();SDL_Delay(10);}
        return h;
    }
    for(part=0;part<4;part++){count[part]=PortGame_VolumeCount(part);TOTAL+=count[part];}
    cache=calloc(TOTAL,sizeof(*cache));buffer=malloc(1024*1024);
    if(!cache || !buffer){free(cache);free(buffer);return 0;}
    snprintf(cachePath,sizeof(cachePath),"%s/.tag-online-bt4-v3-%u",root,TOTAL);
#ifdef _WIN32
    files=calloc(TOTAL,sizeof(*files));if(!files)goto error;
    for(part=0;part<4;part++) {
        if(!inventory(root,part,count[part],0,files+n))goto error;
        inventory(root,part,count[part],1,files+n);n+=count[part];
    }
    n=0;
#endif
    f=fopen(cachePath,"rb");
    if(f){
        if(fread(&marker,8,1,f)==1 && marker==0x544e425434000001ull) {
            if(fread(cache,sizeof(*cache),TOTAL,f)!=TOTAL)memset(cache,0,TOTAL*sizeof(*cache));
        }
        fclose(f);
    }
    XXH3_64bits_reset(&all);
    for(part=0;part<4;part++)for(id=0;id<count[part];id++,n++) {
        if(id==0)fprintf(stderr,"tagnet: checking BT4 volume %u/4\n",part+1);
        struct stat st;FileHash current;int override=1;
        memset(&current,0,sizeof(current));
#ifdef _WIN32
        current=files[n];
        if(!current.reserved)goto error;
        snprintf(path,sizeof(path),"%s/%spzs4us%u/%05u.bin",root,current.override?"mods/":"",part,id);
#else
        snprintf(path,sizeof(path),"%s/mods/pzs4us%u/%05u.bin",root,part,id);
        if(stat(path,&st)) {
            override=0;snprintf(path,sizeof(path),"%s/pzs4us%u/%05u.bin",root,part,id);
            if(stat(path,&st)){fprintf(stderr,"tagnet: missing BT4 resource pzs4us%u/%05u.bin\n",part,id);goto error;}
        }
        current.size=st.st_size;current.modified=st.st_mtime;current.created=st.st_ctime;current.override=override;
#endif
        if(oldCache && cache[n].hash) {
            cache[n].modified=(cache[n].modified+11644473600ull)*10000000ull;
            cache[n].created=(cache[n].created+11644473600ull)*10000000ull;
            /* The previous Windows CRT cache stored whole seconds. */
            if(current.modified/10000000==cache[n].modified/10000000)cache[n].modified=current.modified;
            if(current.created/10000000==cache[n].created/10000000)cache[n].created=current.created;
        }
        if(cache[n].hash && current.size==cache[n].size && current.modified==cache[n].modified &&
           current.created==cache[n].created && current.override==cache[n].override)current.hash=cache[n].hash;
        else {
            XXH3_state_t hash;size_t got;uint64_t read=0;
            f=fopen(path,"rb");if(!f)goto error;
            XXH3_64bits_reset(&hash);
            while((got=fread(buffer,1,1024*1024,f))!=0){XXH3_64bits_update(&hash,buffer,got);read+=got;}
            if(ferror(f) || read!=current.size){fclose(f);goto error;}
            fclose(f);current.hash=XXH3_64bits_digest(&hash);changed++;
        }
        cache[n]=current;
        /* Only content enters the network identity, not times or overrides. */
        XXH3_64bits_update(&all,&current.hash,8);
        if(n%512==0)Gs_HoldFrame();
    }
    if(changed || oldCache) {
        /* Concurrent local test clients may calculate the same cache. A short
         * or interrupted cache is discarded in full on the next launch. */
        f=fopen(cachePath,"wb");if(f){marker=0x544e425434000001ull;fwrite(&marker,8,1,f);fwrite(cache,sizeof(*cache),TOTAL,f);fclose(f);}
    }
    marker=XXH3_64bits_digest(&all);
    fprintf(stderr,"tagnet: BT4 resources %016llx (%u refreshed, %llu ms)\n",(unsigned long long)marker,changed,(unsigned long long)(SDL_GetTicks()-began));
    free(files);free(cache);free(buffer);return marker;
error:
    free(files);free(cache);free(buffer);return 0;
}
