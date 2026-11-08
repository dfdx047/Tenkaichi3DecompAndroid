// Host audio output through SDL3 (Android, Linux): the same device as the Win32 one (host/audio_device.h)
// on an SDL audio stream. The stream's queue is the device's queue: the clock (host/frame_pacer.h)
// reads `queuedFrames` to keep it level, and a write sleeps while more than `queue_frames` wait.
#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

class AudioDevice {
public:
    unsigned fields=0,dry=0,waits=0; // Fields queued; fields that found the device empty; sleeps while it was full.

    AudioDevice(unsigned rate,unsigned channels,unsigned queue_frames):channels_(channels),queue_frames_(queue_frames) {
        if(!SDL_InitSubSystem(SDL_INIT_AUDIO))throw std::runtime_error(std::string("Cannot start SDL audio: ")+SDL_GetError());
        const SDL_AudioSpec spec{SDL_AUDIO_S16,int(channels),int(rate)};
        stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
        if(!stream_) {SDL_QuitSubSystem(SDL_INIT_AUDIO);throw std::runtime_error(std::string("Cannot open the audio device: ")+SDL_GetError());}
        SDL_ResumeAudioStreamDevice(stream_);
    }
    AudioDevice(const AudioDevice&)=delete;
    AudioDevice& operator=(const AudioDevice&)=delete;
    ~AudioDevice() {SDL_DestroyAudioStream(stream_);SDL_QuitSubSystem(SDL_INIT_AUDIO);}

    unsigned queuedFrames() {
        const int bytes=SDL_GetAudioStreamQueued(stream_);
        return bytes>0?unsigned(bytes)/(channels_*2):0;
    }
    // Silence up to `frames` queued: at the start, and after fast forward, which writes nothing.
    void fillTo(unsigned frames) {
        const unsigned queued=queuedFrames();
        if(queued<frames)write(std::vector<std::int16_t>(std::size_t(frames-queued)*channels_));
    }
    // Queues one field of interleaved samples; blocks while the device holds more than its queue.
    void write(std::span<const std::int16_t> samples) {
        const unsigned frames=unsigned(samples.size()/channels_);
        if(!frames)return;
        while(queuedFrames()>queue_frames_) {SDL_Delay(1);++waits;}
        if(fields && !queuedFrames())++dry;
        if(!SDL_PutAudioStreamData(stream_,samples.data(),int(samples.size_bytes())))
            throw std::runtime_error(std::string("Audio device write failed: ")+SDL_GetError());
        ++fields;
    }

private:
    SDL_AudioStream* stream_=nullptr;
    unsigned channels_,queue_frames_;
};
