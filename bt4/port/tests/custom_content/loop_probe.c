/* The production player and cache decoder. Only SDL's dummy device is used. */
#define BT4_REAL_MA
#define main tempo_probe_main
#include "audio_probe.c"
#undef main

int main(int argc,char **argv) {
    assert(argc==2||argc==3);
    int cached=argc==3&&!strcmp(argv[2],"--cached"),loops=0,versions[5]={0};
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
    if(cached)SDL_unsetenv_unsafe("BT4_MUSIC_NO_CACHE");else SDL_setenv_unsafe("BT4_MUSIC_NO_CACHE","1",1);
    assert(SDL_Init(SDL_INIT_AUDIO));
    sTried=1;sDevice=SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,NULL);assert(sDevice);
    assert(!strcmp(SDL_GetCurrentAudioDriver(),"dummy"));assert(SDL_PauseAudioDevice(sDevice));
    for(int i=0;i<(cached?2:227);++i) {
        char path[2048];Player *p=sPlayers;int16_t first[1024],again[1024];
        SDL_snprintf(path,sizeof(path),"%s/pzs4us1/%05d.bin",argv[1],0x815+i);
        start(p,path);assert(p->stream&&p->loop&&p->loopStart==0);
        assert(adx_blanks(path)==-1&&disc_blanks(path)==-1);
        if(cached)assert(p->lossless);
        SDL_UnbindAudioStream(p->stream);
        SDL_AudioSpec spec={SDL_AUDIO_S16,2,(int)p->rate};
        assert(SDL_SetAudioStreamFormat(p->stream,&spec,&spec));
        assert(decode(p,first,16)==512);
        if(cached) {
            char reference[64];SDL_snprintf(reference,sizeof(reference),"music-%d.raw",i);
            FILE *f=fopen(reference,"rb");assert(f&&fread(again,4,512,f)==512);fclose(f);
            assert(!memcmp(first,again,sizeof(first)));
        }
        /* Each loop resumes with the original predictor history or FLAC seek. */
        for(int n=0;n<20;++n) {
            p->pos=p->loopEnd;
            assert(decode(p,again,16)==512&&!memcmp(first,again,sizeof(first)));loops++;
        }
        /* Exercise the callback across the boundary, at normal and online speed. */
        for(int pace=1000;pace<=1140;pace+=140) {
            SDL_ClearAudioStream(p->stream);p->pos=p->loopEnd-32;
            p->stN=p->stHasTail=0;p->stSkip=0;speed=pace;gPortMusicFollows=1;
            feed(p,p->stream,8192,0);
            assert(p->stat==ADXT_STAT_PLAYING&&SDL_GetAudioStreamQueued(p->stream)>=8192);
        }
        versions[p->file[0x12]]++;
        if(!cached&&argc==3&&i<2) {
            char output[2048];SDL_snprintf(output,sizeof(output),"%s/music-%d.raw",argv[2],i);
            FILE *f=fopen(output,"wb");assert(f);p->loop=0;p->pos=0;memset(p->hist,0,sizeof(p->hist));
            int n;while((n=decode(p,again,16))>0)assert(fwrite(again,4,n,f)==(size_t)n);
            fclose(f);
            printf("cache-fixture index=%d key=%016llx rate=%u channels=2\n",i,(unsigned long long)bt4_music_key(p->file,p->size),p->rate);
        }
        stop(p);
    }
    /* A real short voice/effect stream must still finish. */
    char voice[2048];SDL_snprintf(voice,sizeof(voice),"%s/pzs4us1/00333.bin",argv[1]);
    Player *p=sPlayers+4;start(p,voice);assert(p->stream&&!p->loop);
    SDL_UnbindAudioStream(p->stream);SDL_AudioSpec spec={SDL_AUDIO_S16,2,(int)p->rate};
    assert(SDL_SetAudioStreamFormat(p->stream,&spec,&spec));p->pos=p->total;
    feed(p,p->stream,1024,0);assert(p->stat==ADXT_STAT_PLAYEND);stop(p);
    SDL_CloseAudioDevice(sDevice);sDevice=0;SDL_Quit();
    printf("Real music loop boundary checks PASS: v3=%d v4=%d repeats=%d cached=%d physical_devices=0\n",versions[3],versions[4],loops,cached);
    return 0;
}
