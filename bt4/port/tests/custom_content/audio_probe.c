/* Exercise the production ADX feed without opening a physical audio device. */
#include "../../src/gs/snd_adx.c"
#include <assert.h>
unsigned gGsFrame,gPortVBlanks;int gPortResim,gPortAdxCount;
PortAdxView gPortAdxView[MAX_PLAYERS];
const char *Port_GetEnv(const char *name){return SDL_getenv(name);}
int GsGpu_Enabled(void){return 0;}
int Port_NetActive(void){return 0;}int Port_NetSession(void){return 0;}int Port_TagNetwork(void){return 0;}int Port_NetVoiceAlt(void){return 0;}
float PortA11y_StreamGain(const void *p){(void)p;return 1;}
void PortA11y_StreamStart(const char *p){(void)p;}
int Port_FilePath(int a,int b,const char *c,char *d,int n){(void)a;(void)b;(void)c;(void)d;(void)n;assert(0);return 0;}
int Port_Setting(const char *name,int def){(void)name;return def;}
int PortSongs_IsFile(const char *p){(void)p;return 0;}
#ifndef BT4_REAL_MA
int PortMa_Format(const char *p){(void)p;return 0;}int PortMa_Blanks(const char *p){(void)p;return 0;}
void *PortMa_Start(const char *p,int loop){(void)p;(void)loop;assert(0);return NULL;}
void PortMa_Stop(void *p){(void)p;}void PortMa_SetPan(void *p,int a,int b){(void)p;(void)a;(void)b;}
void PortMa_Pause(void *p,int b){(void)p;(void)b;}int PortMa_AtEnd(void *p){(void)p;return 0;}
void PortMa_SetMono(int m){(void)m;}void PortMa_SetVolume(void *p,float v){(void)p;(void)v;}
void *PortPcm_Open(const char *p,unsigned c,unsigned r,unsigned long long n){(void)p;(void)c;(void)r;(void)n;return NULL;}
void PortPcm_Close(void *p){(void)p;}
#endif
static int speed=1000;
int Port_SpeedMilli(void){return speed;}
float PortAudio_Gain(int actor,int voice){(void)actor;(void)voice;return 1;}
#ifndef BT4_REAL_MA
unsigned PortPcm_Read(void *p,int16_t *out,unsigned n){(void)p;(void)out;(void)n;return 0;}
int PortPcm_Seek(void *p,unsigned n){(void)p;(void)n;return 0;}
#endif

static unsigned render(const char *path,int channel,int loop,int follows,int pace,int16_t *out,int bytes){
    FILE *fp=fopen(path,"rb");long size;Player *p=&sPlayers[channel];SDL_AudioSpec spec={SDL_AUDIO_S16,2,24000};
    assert(fp);fseek(fp,0,SEEK_END);size=ftell(fp);rewind(fp);memset(p,0,sizeof(*p));
    p->file=malloc(size);assert(fread(p->file,1,size,fp)==(size_t)size);fclose(fp);
    assert(be16(p->file)==0x8000);p->data=be16(p->file+2)+4;p->size=(uint32_t)size;
    p->rate=be32(p->file+8);p->channels=p->file[7];p->total=be32(p->file+12);
    p->loop=loop;p->loopStart=0;p->loopEnd=p->total;p->pan[0]=p->pan[1]=PAN_AUTO;
    p->stat=ADXT_STAT_PLAYING;adx_coefs(be16(p->file+16),p->rate,&p->c1,&p->c2);
    spec.freq=(int)p->rate;p->stream=SDL_CreateAudioStream(&spec,&spec);assert(p->stream);
    speed=pace;gPortMusicFollows=follows;
    feed(p,p->stream,bytes,0);SDL_FlushAudioStream(p->stream);
    assert(SDL_GetAudioStreamData(p->stream,out,bytes)==bytes);
    unsigned consumed=p->pos;SDL_DestroyAudioStream(p->stream);free(p->file);p->file=NULL;p->stream=NULL;
    return consumed;
}
int main(int argc,char **argv){
    assert(argc==2);const int bytes=24000*4*5;int16_t *base=malloc(bytes),*candidate=malloc(bytes);int checks=0;
    for(int channel=0;channel<2;channel++)for(int loop=0;loop<2;loop++){
        unsigned normal=render(argv[1],channel,loop,0,1000,base,bytes);
        unsigned original=render(argv[1],channel,loop,0,1140,candidate,bytes);
        assert(normal==original&&!memcmp(base,candidate,bytes));checks++;
        unsigned following=render(argv[1],channel,loop,1,1140,candidate,bytes);
        double ratio=(double)following/normal;
        assert(ratio>1.10&&ratio<1.20);assert(memcmp(base,candidate,bytes));checks++;
        printf("channel=%d loop=%d disabled_pcm_identical=1 enabled_ratio=%.5f\n",channel,loop,ratio);
    }
    /* Voice duration continues to follow simulation speed when music does not. */
    unsigned voice0=render(argv[1],4,0,0,1000,base,bytes);
    unsigned voice1=render(argv[1],4,0,0,1140,candidate,bytes);
    assert((double)voice1/voice0>1.10);checks++;
    free(base);free(candidate);printf("audio_checks=%d physical_audio_devices=0\n",checks);return 0;
}
