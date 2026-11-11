/* Convert only the native BT4 music bank. No desktop/audio device is opened.
 * PCM math matches snd_adx.c; libFLAC verifies every encoded block. */
#include <FLAC/stream_encoder.h>
#include <FLAC/metadata.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif
#include "../src/gs/music_cache.h"
static uint32_t u16(const unsigned char *p){return ((uint32_t)p[0]<<8)|p[1];}
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static int convert(const char *input,const char *output,int dump){
    FILE *f=fopen(input,"rb");unsigned char *data;long size;uint32_t total,rate,ch,offset,pos=0;int c1,c2,hist[2][2]={{0}},ok=1;
    FLAC__StreamEncoder *enc=NULL;FLAC__StreamMetadata *comment=NULL;FLAC__int32 pcm[4096*2];
    if(!f)return 2;fseek(f,0,SEEK_END);size=ftell(f);rewind(f);data=(unsigned char *)malloc(size>0?(size_t)size:1);
    if(!data||fread(data,1,size,f)!=(size_t)size){fclose(f);free(data);return 3;}fclose(f);
    if(size<64||u16(data)!=0x8000||data[4]!=3||data[5]!=18||data[6]!=4||(data[19]&8)){free(data);return 4;}
    offset=u16(data+2)+4;ch=data[7];rate=u32(data+8);total=u32(data+12);
    if(ch<1||ch>2||rate<8000||rate>192000||!total||offset>(uint32_t)size||((uint64_t)total+31)/32*18*ch>(uint64_t)size-offset){free(data);return 5;}
    {double z=cos(2.0*3.14159265358979323846*u16(data+16)/rate),a=1.41421356237309504880-z,b=1.41421356237309504880-1.0,c=(a-sqrt((a+b)*(a-b)))/b;c1=(int)(c*8192.0);c2=(int)(c*c*-4096.0);}
    f=dump?fopen(output,"wb"):NULL;
    if(dump&&!f){free(data);return 6;}
    if(!dump){
        enc=FLAC__stream_encoder_new();if(!enc){free(data);return 7;}
        FLAC__stream_encoder_set_verify(enc,1);FLAC__stream_encoder_set_compression_level(enc,8);
        FLAC__stream_encoder_set_channels(enc,ch);FLAC__stream_encoder_set_bits_per_sample(enc,16);
        FLAC__stream_encoder_set_sample_rate(enc,rate);FLAC__stream_encoder_set_total_samples_estimate(enc,total);
        comment=FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
        if(comment){
            char value[80];FLAC__StreamMetadata_VorbisComment_Entry e;
            const char *names[]={"BT4_CACHE_VERSION","SOURCE_FNV64","LOOPSTART","LOOPEND"};
            int k;for(k=0;k<4;k++){
                if(k==0)snprintf(value,sizeof(value),"1");
                if(k==1)snprintf(value,sizeof(value),"%016llx",(unsigned long long)bt4_music_key(data,size));
                if(k>=2)snprintf(value,sizeof(value),"%u",data[18]==4&&u32(data+36)?u32(data+(k==2?40:48)):0);
                if(FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&e,names[k],value))FLAC__metadata_object_vorbiscomment_append_comment(comment,e,0);
            }
            FLAC__stream_encoder_set_metadata(enc,&comment,1);
        }
        if(FLAC__stream_encoder_init_file(enc,output,NULL,NULL)!=FLAC__STREAM_ENCODER_INIT_STATUS_OK)ok=0;
    }
    while(ok&&pos<total){
        unsigned n=0;
        while(n<4096&&pos<total){
            unsigned k,i;int16_t block[2][32];unsigned count=total-pos<32?total-pos:32;
            for(k=0;k<ch;k++){
                const unsigned char *src=data+offset+(size_t)(pos/32)*18*ch+k*18;
                int scale=u16(src)+1,h1=hist[k][0],h2=hist[k][1];
                for(i=0;i<32;i++){
                    int nib=(src[2+i/2]>>((i&1)?0:4))&15,s;if(nib&8)nib-=16;
                    s=nib*scale+((c1*h1+c2*h2)>>12);if(s>32767)s=32767;if(s< -32768)s=-32768;
                    block[k][i]=(int16_t)s;h2=h1;h1=s;
                }hist[k][0]=h1;hist[k][1]=h2;
            }
            for(i=0;i<count;i++)for(k=0;k<ch;k++)pcm[(n+i)*ch+k]=block[k][i];
            pos+=count;n+=count;
        }
        if(dump){unsigned i;for(i=0;i<n*ch;i++){int16_t s=(int16_t)pcm[i];if(fwrite(&s,2,1,f)!=1)ok=0;}}
        else if(!FLAC__stream_encoder_process_interleaved(enc,pcm,n))ok=0;
    }
    if(f&&fclose(f))ok=0;
    if(enc){if(!FLAC__stream_encoder_finish(enc))ok=0;FLAC__stream_encoder_delete(enc);}
    if(comment)FLAC__metadata_object_delete(comment);free(data);
    if(!ok){remove(output);return 8;}return 0;
}
static int library(const char *root,const char *output,const char *language){
    char path[2000],dest[2000],temp[2048],lock[2000],complete[2000];int i,count=0,failed=0,skipped=0;
#ifdef _WIN32
    HANDLE guard;
    _mkdir(output);snprintf(lock,sizeof(lock),"%s/.lock",output);
    guard=CreateFileA(lock,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_HIDDEN,NULL);
    if(guard==INVALID_HANDLE_VALUE)return 9;
    SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
    SetPriorityClass(GetCurrentProcess(),PROCESS_MODE_BACKGROUND_BEGIN);
#endif
    for(i=2069;i<=2324;i++){
        FILE *f;unsigned char *data;long size;uint64_t key;
        snprintf(path,sizeof(path),"%s/pzs4us1/%05d.bin",root,i);f=fopen(path,"rb");if(!f){failed++;continue;}
        fseek(f,0,SEEK_END);size=ftell(f);rewind(f);data=(unsigned char *)malloc(size>0?(size_t)size:1);
        if(!data||fread(data,1,size,f)!=(size_t)size){free(data);fclose(f);failed++;continue;}fclose(f);
        if(size<64||u16(data)!=0x8000||!u32(data+12)){free(data);failed++;continue;}
        key=bt4_music_key(data,size);free(data);snprintf(dest,sizeof(dest),"%s/%016llx.flac",output,(unsigned long long)key);
        f=fopen(dest,"rb");if(f){fclose(f);skipped++;fprintf(stdout,"music file=%d key=%016llx status=existing\n",i,(unsigned long long)key);continue;}
        snprintf(temp,sizeof(temp),"%s.partial",dest);int r=convert(path,temp,0);
        if(!r&&rename(temp,dest)==0)count++;else {failed++;remove(temp);}
        fprintf(stdout,"music file=%d key=%016llx status=%d converted=%d failed=%d\n",i,(unsigned long long)key,r,count,failed);fflush(stdout);
    }
    fprintf(stdout,"music complete converted=%d existing=%d failed=%d\n",count,skipped,failed);
    if(!failed){snprintf(complete,sizeof(complete),"%s/.complete-%s",output,language);FILE *f=fopen(complete,"wb");if(f){fputs("BT4 native music cache v1\n",f);fclose(f);}}
#ifdef _WIN32
    CloseHandle(guard);
#endif
    return failed?10:0;
}
int main(int argc,char **argv){
    if((argc==4||argc==5)&&!strcmp(argv[1],"--library"))return library(argv[2],argv[3],argc==5&&!strcmp(argv[4],"en")?"en":"es");
    if(argc==4&&!strcmp(argv[1],"--pcm"))return convert(argv[2],argv[3],1);
    if(argc==3)return convert(argv[1],argv[2],0);
    fprintf(stderr,"BT4 music converter: --library DATA_ROOT OUTPUT_FOLDER | INPUT_ADX OUTPUT_FLAC\n");return 1;
}
