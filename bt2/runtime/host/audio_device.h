// Host audio output (Win32 waveOut): plays the mixer's fields as they are produced.
// The run is paced by the clock (host/frame_pacer.h), which reads `queuedFrames` to keep this
// device's queue level. The device itself only refuses to fall far behind: while more than
// `queue_frames` are waiting, a write sleeps. A run that is slower than the game lets the device run
// dry, which is heard as gaps and counted in `dry`.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

class AudioDevice {
public:
    unsigned fields=0,dry=0,waits=0; // Fields queued; fields that found the device empty; sleeps while it was full.

    // `queue_frames`: the most sample frames that may wait in the device before the writer blocks.
    AudioDevice(unsigned rate,unsigned channels,unsigned queue_frames):channels_(channels),queue_frames_(queue_frames) {
        WAVEFORMATEX format{};
        format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=WORD(channels);format.nSamplesPerSec=rate;format.wBitsPerSample=16;
        format.nBlockAlign=WORD(channels*2);format.nAvgBytesPerSec=rate*channels*2;
        const auto result=waveOutOpen(&device_,WAVE_MAPPER,&format,0,0,CALLBACK_NULL);
        if(result!=MMSYSERR_NOERROR)throw std::runtime_error("Cannot open the audio device (waveOut error "+std::to_string(result)+")");
        timeBeginPeriod(1); // Sleeps of a millisecond instead of the default granularity of about 15 ms.
    }
    AudioDevice(const AudioDevice&)=delete;
    AudioDevice& operator=(const AudioDevice&)=delete;
    ~AudioDevice() {
        waveOutReset(device_);
        for(auto& b:buffers_)if(b.queued)waveOutUnprepareHeader(device_,&b.header,sizeof b.header);
        waveOutClose(device_);
        timeEndPeriod(1);
    }
    unsigned queuedFrames() {reclaim();return queued_;}
    // Silence up to `frames` queued: at the start, and after fast forward, which writes nothing.
    void fillTo(unsigned frames) {
        const unsigned queued=queuedFrames();
        if(queued<frames)write(std::vector<std::int16_t>(std::size_t(frames-queued)*channels_));
    }

    // Queues one field of interleaved samples; blocks while the device holds more than its queue.
    void write(std::span<const std::int16_t> samples) {
        const unsigned frames=unsigned(samples.size()/channels_);
        if(!frames)return;
        Buffer* free=nullptr;
        for(;;) {
            reclaim();
            free=nullptr;for(auto& b:buffers_)if(!b.queued) {free=&b;break;}
            if(free && queued_<=queue_frames_)break;
            Sleep(1);++waits;
        }
        if(fields && !queued_)++dry;
        free->pcm.assign(samples.begin(),samples.end());
        free->header={};free->header.lpData=reinterpret_cast<LPSTR>(free->pcm.data());free->header.dwBufferLength=DWORD(free->pcm.size()*2);
        if(waveOutPrepareHeader(device_,&free->header,sizeof free->header)!=MMSYSERR_NOERROR || waveOutWrite(device_,&free->header,sizeof free->header)!=MMSYSERR_NOERROR)
            throw std::runtime_error("Audio device write failed");
        free->queued=true;free->frames=frames;queued_+=frames;++fields;
    }

private:
    struct Buffer {WAVEHDR header{};std::vector<std::int16_t> pcm;unsigned frames=0;bool queued=false;};
    void reclaim() {
        for(auto& b:buffers_)if(b.queued && b.header.dwFlags&WHDR_DONE) {
            waveOutUnprepareHeader(device_,&b.header,sizeof b.header);b.queued=false;queued_-=b.frames;
        }
    }
    HWAVEOUT device_=nullptr;
    unsigned channels_,queue_frames_,queued_=0;
    std::array<Buffer,32> buffers_;
};
#elif defined(BT2_SDL)
#include "host/sdl/sdl_audio_device.h"
#endif
