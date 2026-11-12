// Windows voice devices are independent of the game's SDL audio streams.
// Fixed prepared buffers stay alive until reset returns them to this owner.
#include <mmsystem.h>
class VoiceAudio {
    static constexpr unsigned buffers=6, samples=320;
    HWAVEIN input=nullptr;
    HWAVEOUT output=nullptr;
    WAVEHDR headers[buffers]={};
    short pcm[buffers][samples]={};
    unsigned cursor=0;
    bool recording, silent=false;
    static UINT device(bool recording,const std::string &name){
        if(name.empty())return WAVE_MAPPER;
        wchar_t wanted[256]={};
        if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.c_str(),-1,wanted,256))return UINT_MAX-1;
        UINT found=UINT_MAX-1,count=recording?waveInGetNumDevs():waveOutGetNumDevs();
        for(UINT i=0;i<count;++i){
            wchar_t text[MAXPNAMELEN]={};MMRESULT result;
            if(recording){WAVEINCAPSW caps={};result=waveInGetDevCapsW(i,&caps,sizeof(caps));if(!result)wcscpy(text,caps.szPname);}
            else{WAVEOUTCAPSW caps={};result=waveOutGetDevCapsW(i,&caps,sizeof(caps));if(!result)wcscpy(text,caps.szPname);}
            if(result)continue;
            // WinMM may truncate the friendly name to MAXPNAMELEN-1 characters.
            if(!wcscmp(text,wanted)||(wcslen(text)==MAXPNAMELEN-1&&!wcsncmp(text,wanted,MAXPNAMELEN-1))){
                if(found!=UINT_MAX-1)return UINT_MAX-1;
                found=i;
            }
        }
        return found;
    }
public:
    explicit VoiceAudio(bool recording):recording(recording){}
    ~VoiceAudio(){close();}
    VoiceAudio(const VoiceAudio &)=delete;
    VoiceAudio &operator=(const VoiceAudio &)=delete;
    explicit operator bool()const{return input||output||silent;}
    unsigned played()const{
        MMTIME position={};position.wType=TIME_SAMPLES;
        return output&&!waveOutGetPosition(output,&position,sizeof(position))&&position.wType==TIME_SAMPLES?position.u.sample:0;
    }
    bool open(const std::string &name){
        close();if(recording&&name=="@none")return false;
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_VOICE_AUDIO_FAIL"))return false;
        if(getenv("BT4_RENDER_TEST")&&getenv("SDL_AUDIO_DRIVER")&&!strcmp(getenv("SDL_AUDIO_DRIVER"),"dummy")){silent=!recording;return silent;}
        UINT id=device(recording,name);if(id==UINT_MAX-1)return false;
        WAVEFORMATEX format={};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;format.nSamplesPerSec=16000;
        format.wBitsPerSample=16;format.nBlockAlign=2;format.nAvgBytesPerSec=32000;
        MMRESULT result=recording?waveInOpen(&input,id,&format,0,0,CALLBACK_NULL):waveOutOpen(&output,id,&format,0,0,CALLBACK_NULL);
        if(result){input=nullptr;output=nullptr;return false;}
        for(unsigned i=0;i<buffers;++i){
            auto &h=headers[i];h.lpData=(LPSTR)pcm[i];h.dwBufferLength=sizeof(pcm[i]);
            result=recording?waveInPrepareHeader(input,&h,sizeof(h)):waveOutPrepareHeader(output,&h,sizeof(h));
            if(result){close();return false;}
            if(recording&&waveInAddBuffer(input,&h,sizeof(h))){close();return false;}
        }
        if(recording&&waveInStart(input)){close();return false;}
        return true;
    }
    int read(short *dst){
        if(!input)return 0;
        auto &h=headers[cursor];
        if(!(((volatile WAVEHDR *)&h)->dwFlags&WHDR_DONE))return 0;
        unsigned bytes=std::min<unsigned>(h.dwBytesRecorded,sizeof(pcm[0]));
        memcpy(dst,pcm[cursor],bytes);
        h.dwBytesRecorded=0;
        if(waveInAddBuffer(input,&h,sizeof(h))){close();return 0;}
        cursor=(cursor+1)%buffers;
        return (int)bytes;
    }
    void write(const short *src){
        if(!output)return;
        auto &h=headers[cursor];
        if(((volatile WAVEHDR *)&h)->dwFlags&WHDR_INQUEUE)return;
        memcpy(pcm[cursor],src,sizeof(pcm[0]));
        if(waveOutWrite(output,&h,sizeof(h))){close();return;}
        cursor=(cursor+1)%buffers;
    }
    void close(){
        if(input)waveInReset(input);
        if(output)waveOutReset(output);
        for(auto &h:headers)if(h.dwFlags&WHDR_PREPARED){
            if(input)waveInUnprepareHeader(input,&h,sizeof(h));
            if(output)waveOutUnprepareHeader(output,&h,sizeof(h));
        }
        if(input)waveInClose(input);
        if(output)waveOutClose(output);
        input=nullptr;output=nullptr;silent=false;cursor=0;
        memset(headers,0,sizeof(headers));
    }
};
