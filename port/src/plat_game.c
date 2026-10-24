/* Native BT4 B14 REV2 resource profile. No game assets are embedded here. */
#include "plat_game.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

extern const char *Port_FileRoot(void);
extern void *Port_LowAlloc(size_t size);
static int initialized, bt4, region, spanish;
static unsigned short costumes[250];
static int models[251];
static void *metadata[250][4];
static char names[250][256];
static signed char damageModels[250][16];
static signed char voiceBanks[2][250];
static int damaged_available(int c,int outfit);
static unsigned char extraCustom[250-97][56];
static int customLoaded;

static void fail(const char *message) {
    fprintf(stderr, "native disc: %s\n", message);
    exit(2);
}
static FILE *open_data(const char *relative) {
    char path[1600];
    snprintf(path, sizeof(path), "%s/%s", Port_FileRoot(), relative);
    return fopen(path, "rb");
}
static uint32_t u32(const unsigned char *p) {
    return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void init(void) {
    FILE *f;
    char marker[8192];
    unsigned char table[32768];
    size_t size;
    unsigned offset;
    int i;
    if (initialized) return;
    initialized = 1;
    f = open_data("native-disc.json");
    if (!f) return;
    size = fread(marker, 1, sizeof(marker)-1, f); fclose(f); marker[size] = 0;
    if (strstr(marker, "\"bt3\"") && strstr(marker, "\"bt3-us-canonical-v1\"")) {
        if(strstr(marker,"\"eu\"")) region=1;
        else if(strstr(marker,"\"jp\"")) region=2;
        else if(!strstr(marker,"\"us\"")) fail("Unknown BT3 disc region.");
        fprintf(stderr,"native disc: BT3 %s, canonical resources enabled\n",region==1?"Europe":region==2?"Japan":"USA");
        return;
    }
    if (!strstr(marker, "\"bt4\"") || !strstr(marker, "\"b14-rev2\""))
        fail("Unsupported disc profile. Use the native disc importer.");
    spanish = strstr(marker,"\"es\"") != NULL;
    f = open_data("pzs4us1/00003.bin");
    if (!f) fail("BT4 character table is missing.");
    size = fread(table, 1, sizeof(table), f); fclose(f);
    if (size < 8) fail("BT4 character table is incomplete.");
    offset = u32(table+4) & ~3u;
    if (offset > size || size-offset < 250*60)
        fail("BT4 character table is incomplete.");
    models[0] = 5650;
    for (i=0; i<250; ++i) {
        unsigned c = table[offset+i*60+40] | table[offset+i*60+41]<<8;
        if (c > 16) fail("Unsupported BT4 costume table.");
        costumes[i] = c;
        models[i+1] = models[i] + c*2;
    }
    bt4 = 1;
    f=open_data("native-roster.tsv");
    if(f) {
        char line[600];
        while(fgets(line,sizeof(line),f)) {
            char *name=strchr(line,'\t'),*form;
            int c=atoi(line);
            if(!name || c<0 || c>=250) continue;
            form=strchr(++name,'\t');
            if(!form) continue;
            *form++=0;form[strcspn(form,"\r\n")]=0;
            snprintf(names[c],sizeof(names[c]),"%s%s%s",name,*form?" ":"",form);
        }
        fclose(f);
    }
    fprintf(stderr, "native disc: BT4 B14 REV2, separate resources enabled\n");
}
int PortGame_IsBT4(void) { init(); return bt4; }
int PortGame_Region(void) { init(); return region; }
const char *PortGame_DefaultLanguage(void) { init(); return spanish?"es":"en"; }
int PortGame_VolumeCount(int volume) {
    static const int bt3Counts[]={1,3399,65201};
    static const int bt4Counts[]={50503,3400,32775,30000};
    init();
    if(volume<0 || volume>=(bt4?4:3))return 0;
    return bt4?bt4Counts[volume]-(spanish && volume==2):bt3Counts[volume];
}
int PortGame_CharacterCount(void) { return PortGame_IsBT4() ? 250 : 161; }
int PortGame_IsCharacter(int c) {
    return PortGame_IsBT4() ? c>=0 && c<250 && (c<161 || c>164) && costumes[c]!=0 : c>=0 && c<161;
}
int PortGame_StageCount(void) { return PortGame_IsBT4()?99:36; }
const char *PortGame_CharacterName(int c) {
    return PortGame_IsBT4() && c>=0 && c<250 && names[c][0]?names[c]:NULL;
}
int PortGame_MusicFile(int track) { return (PortGame_IsBT4()?2070:0x10B16)+track; }
int PortGame_Costumes(int c) {
    if(PortGame_IsBT4())return c>=0 && c<250 ? costumes[c] : 0;
    if(c>=0 && c<161) {
        static int loaded;
        if(!loaded) {
            unsigned char table[32768];unsigned offset;size_t size;int i;
            FILE *f=open_data("pzs3us1/00003.bin");
            if(!f)return 0;
            size=fread(table,1,sizeof(table),f);fclose(f);
            if(size<8)return 0;
            offset=u32(table+4)&~3u;
            if(offset>size || size-offset<161*60)return 0;
            for(i=0;i<161;i++)costumes[i]=table[offset+i*60+10] | table[offset+i*60+11]<<8;
            loaded=1;
        }
        return costumes[c];
    }
    return 0;
}
int PortGame_Model(int c, int outfit, int damaged) {
    if (!PortGame_IsBT4()) return 1424+c*10+outfit+(damaged?4:0);
    if (c<0 || c>=250 || !costumes[c] || outfit<0 || outfit>=costumes[c]) {
        fprintf(stderr,"native disc: model request fighter=%d costume=%d damaged=%d\n",c,outfit,damaged);
        fail("Invalid fighter or costume requested.");
    }
    if(damaged) {
        /* Some B14 costumes have no damaged mesh. Preserve the same intact
           costume, rather than bind the empty placeholder as a model. */
        if(!damaged_available(c,outfit)) damaged=0;
    }
    return models[c]+outfit+(damaged?costumes[c]:0);
}
static int damaged_available(int c,int outfit) {
    if(!damageModels[c][outfit]) {
        char path[128];unsigned char header[8];FILE *f;int id=models[c]+outfit+costumes[c];
        snprintf(path,sizeof(path),"pzs4us%d/%05d.bin",PortGame_Partition(id),PortGame_FileIndex(id));
        f=open_data(path);
        if(!f) fail("Missing BT4 damaged costume resource.");
        damageModels[c][outfit]=fread(header,1,8,f)==8 && u32(header)>0 ? 1 : -1;
        fclose(f);
    }
    return damageModels[c][outfit]>0;
}
int PortGame_ModelCharacter(int id) {
    int i;
    if (!PortGame_IsBT4()) return (id-1424)/10;
    for (i=0; i<250; ++i) if (id>=models[i] && id<models[i+1]) return i;
    return -1;
}
/* B14 Majin Time Patroller outfit 2 has an accidentally relocated prefix in
   both body-volume files. Its intact counterpart elsewhere on the same disc
   confirms every byte of this recovery, including the final corner's Y.
   Match the complete original section before touching the in-memory copy. */
void PortGame_RepairModelBody(void *body,int size,int model) {
    unsigned char *p=body;unsigned hash=2166136261u,*words=body;int i;
    if(!PortGame_IsBT4() || !body || size!=9792 || (model!=7205 && model!=7207))return;
    for(i=0;i<size;i++)hash=(hash^p[i])*16777619u;
    if(hash!=0x855116afu)return;
    for(i=0;i<0x314/4;i++)words[i]-=0x1e450u;
    words[0x314/4]=words[0x2b4/4]^0x80000000u;
}
int PortGame_Animation(int c) { return PortGame_IsBT4()?5150+c:1432+c*10; }
int PortGame_Combat(int c) { return PortGame_IsBT4()?5400+c:1433+c*10; }
static int voice_bank_available(int c,int jp) {
    if(!voiceBanks[jp][c]) {
        unsigned char header[20];char path[128];FILE *f;long size;unsigned i,prev;
        int good=0,id=c+(jp?0x1D10C:0x1D012);
        snprintf(path,sizeof(path),"pzs4us%d/%05d.bin",PortGame_Partition(id),PortGame_FileIndex(id));
        f=open_data(path);
        if(f) {
            fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
            good=fread(header,1,20,f)==20 && u32(header)==3;
            prev=20;
            for(i=4;good && i<20;i+=4) {
                unsigned off=u32(header+i);
                if(off<prev || off>(unsigned)size) good=0;
                prev=off;
            }
            fclose(f);
        }
        voiceBanks[jp][c]=good?1:-1;
    }
    return voiceBanks[jp][c]>0;
}
int PortGame_Voice(int c,int jp) {
    if(!PortGame_IsBT4()) return c+(jp?3195:3034);
    if(c<0 || c>=250) fail("Invalid BT4 voice character.");
    jp=jp!=0;
    /* B14's Japanese Time Patroller bank is an invalid placeholder. Its
       English bank still contains the fighter's own combat samples. */
    if(!voice_bank_available(c,jp)) {
        if(!voice_bank_available(c,!jp)) fail("Missing BT4 fighter combat sounds.");
        jp=!jp;
    }
    return c+(jp?0x1D10C:0x1D012);
}
int PortGame_VoiceLine(int c,int line,int jp) {
    return (PortGame_IsBT4()?(jp?0x16E6A:0x10CC2):(jp?0xCC32:0x8D4E))+100*c+line;
}
int PortGame_Member(int c,int side) { return (PortGame_IsBT4()?0xD48:8)+c*2+side; }
int PortGame_Partition(int id) {
    if (PortGame_IsBT4()) return id>=0x20000?3:id>=0x10CC0?0:id>=0xD48?2:id>0?1:0;
    return id<=0?0:id<0xD48?1:2;
}
int PortGame_FileIndex(int id) {
    if (PortGame_IsBT4()) return id>=0x20000?id-0x20000:id>=0x10CC0?id-0x10CC0:id>=0xD48?id-0xD48:id>0?id-1:id;
    return id<=0?id:id<0xD48?id-1:id-0xD48;
}
/* These tables are immutable apart from the game's idempotent relocation of
   the first metadata section. Keep them alive across model changes and menus. */
void *PortGame_Metadata(int c,int kind) {
    char path[128];
    FILE *f;
    long size;
    void *data;
    int id;
    if (!PortGame_IsBT4() || c<0 || c>=250 || kind<0 || kind>=4) return NULL;
    if (metadata[c][kind]) return metadata[c][kind];
    id = 4150+c+kind*250;
    snprintf(path,sizeof(path),"pzs4us%d/%05d.bin",PortGame_Partition(id),PortGame_FileIndex(id));
    f=open_data(path);
    if (!f) fail("Missing BT4 fighter metadata.");
    fseek(f,0,SEEK_END); size=ftell(f); rewind(f);
    if (size<8 || size>2*1024*1024) fail("Invalid BT4 fighter metadata size.");
    data=Port_LowAlloc(size);
    if (!data || fread(data,1,size,f)!=(size_t)size) fail("Could not read BT4 fighter metadata.");
    fclose(f);
    metadata[c][kind]=data;
    return data;
}

static void custom_path(char *path,size_t capacity,const char *suffix) {
    const char *root=getenv("BT3_SAVES");
    snprintf(path,capacity,"%s/native-bt4-custom%s",root?root:"saves",suffix);
}
void *PortGame_CustomSlot(void *original,int slot) {
    if(slot<0 || slot>=(PortGame_IsBT4()?250:97)) fail("Invalid equipment slot.");
    if(slot<97) return (unsigned char *)original+slot*56;
    if(!customLoaded) {
        char path[1600],header[8];FILE *f;
        custom_path(path,sizeof(path),".bin");f=fopen(path,"rb");
        if(f) {
            if(fread(header,1,8,f)!=8 || memcmp(header,"BT4CUST1",8) ||
               fread(extraCustom,1,sizeof(extraCustom),f)!=sizeof(extraCustom) || fgetc(f)!=EOF)
                fail("Invalid BT4 equipment save; restore a known-good copy before continuing.");
            fclose(f);
        }
        customLoaded=1;
    }
    return extraCustom[slot-97];
}
static unsigned char offlineCustom[sizeof(extraCustom)];
static int onlineCustom,offlineCustomLoaded;
void PortGame_OnlineCustom(int enter) {
    if(!PortGame_IsBT4())return;
    if(enter && !onlineCustom) {
        memcpy(offlineCustom,extraCustom,sizeof(extraCustom));offlineCustomLoaded=customLoaded;
        memset(extraCustom,0,sizeof(extraCustom));customLoaded=1;onlineCustom=1;
    } else if(!enter && onlineCustom) {
        memcpy(extraCustom,offlineCustom,sizeof(extraCustom));customLoaded=offlineCustomLoaded;onlineCustom=0;
    }
}
int PortGame_SaveCustom(void) {
    char path[1600],temporary[1600];FILE *f;int good;
    if(!PortGame_IsBT4() || !customLoaded || onlineCustom) return 1;
    custom_path(path,sizeof(path),".bin");custom_path(temporary,sizeof(temporary),".tmp");
    f=fopen(temporary,"wb");if(!f) return 0;
    good=fwrite("BT4CUST1",1,8,f)==8 && fwrite(extraCustom,1,sizeof(extraCustom),f)==sizeof(extraCustom);
    if(fclose(f)) good=0;
    if(!good) return 0;
#ifdef _WIN32
    return MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(temporary,path)==0;
#endif
}

/* BT4 has motions larger than the original 48 KiB work layer. Keep the game's
   object layout intact; give its decoded layers separate, reusable native
   storage. Reset keys when the object table is recreated, not during a form
   change: both animation layers must survive an overlapping transformation. */
static struct { void *key,*data; } animationBuffers[64];
static int animationBufferCount;
void PortGame_ResetAnimations(void) { animationBufferCount=0; }
int PortGame_AnimationCapacity(void) { return PortGame_IsBT4()?0x10000:0xC000; }
void *PortGame_AnimationBuffer(void *original) {
    int i;
    if(!PortGame_IsBT4()) return original;
    for(i=0;i<animationBufferCount;i++) if(animationBuffers[i].key==original)
        return animationBuffers[i].data;
    if(i>=64) fail("Too many native animation layers.");
    if(!animationBuffers[i].data) animationBuffers[i].data=Port_LowAlloc(0x10000);
    if(!animationBuffers[i].data) fail("Could not allocate a native animation layer.");
    animationBuffers[i].key=original;animationBufferCount++;
    return animationBuffers[i].data;
}

/* Some B14 motions contain stale bone offsets, including Kid Gohan's tail in
   motion 38. A malformed track must not reach quaternion unpacking or retarget
   writes. Keep the affected bone at its authored rest pose; preserve every
   valid track and all timing/events. This only edits the decoded work buffer. */
int PortGame_CheckAnimation(void *data,int size) {
    unsigned char *b=(unsigned char*)data;
    unsigned i,j,bad=0,header;
    if(!b || size<148 || size>PortGame_AnimationCapacity()) return -1;
    header=148+16*b[1];
    if(header>(unsigned)size) return -1;
    for(i=0;i<71;i++) {
        unsigned at=6+2*i,off=(b[at]|(unsigned)b[at+1]<<8)*4;
        unsigned n,flags,stride,rot,bytes,invalid=0;
        if(!off) continue;
        if(off<header || off>(unsigned)size-4) invalid=1;
        else {
            flags=b[off]|(unsigned)b[off+1]<<8;
            n=b[off+2]|(unsigned)b[off+3]<<8;
            stride=(flags&1)?8:24;rot=(flags&1)?0:16;
            bytes=n*stride+((flags&1)&&n>1?n*2:0);
            if(!n || bytes>(unsigned)size-off-4) invalid=1;
            else for(j=0;j<n;j++) {
                const unsigned char *key=b+off+4+j*stride;
                if((key[rot+7]>>4)>3) {invalid=1;break;}
                if(!(flags&1) && ((u32(key)&0x7f800000)==0x7f800000 ||
                   (u32(key+4)&0x7f800000)==0x7f800000 ||
                   (u32(key+8)&0x7f800000)==0x7f800000)) {invalid=1;break;}
            }
        }
        if(invalid) {b[at]=b[at+1]=0;bad++;}
    }
    return (int)bad;
}
