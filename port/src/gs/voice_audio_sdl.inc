/* SDL converts each independent voice stream to/from 16 kHz mono PCM.
 * Recording backlog and playback queues stay bounded to four 20 ms frames. */
class VoiceAudio {
    SDL_AudioStream *stream = nullptr;
    bool recording;
    unsigned submitted = 0;
    static constexpr int bytesPerFrame = 640;
    static SDL_AudioDeviceID device(bool recording, const std::string &name) {
        if (recording && name == "@none") return 0;
        if (name.empty()) return recording ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
        int count = 0;
        auto *ids = recording ? SDL_GetAudioRecordingDevices(&count) : SDL_GetAudioPlaybackDevices(&count);
        SDL_AudioDeviceID result = 0;
        for (int i = 0; i < count; ++i) {
            const char *label = SDL_GetAudioDeviceName(ids[i]);
            if (label && name == label) { result = ids[i]; break; }
        }
        SDL_free(ids);
        return result;
    }
public:
    explicit VoiceAudio(bool recording) : recording(recording) {}
    ~VoiceAudio() { close(); }
    VoiceAudio(const VoiceAudio &) = delete;
    VoiceAudio &operator=(const VoiceAudio &) = delete;
    explicit operator bool() const { return stream != nullptr; }
    unsigned played() const {
        int queued = stream ? SDL_GetAudioStreamQueued(stream) : 0;
        unsigned pending = queued > 0 ? static_cast<unsigned>(queued / 2) : 0;
        return submitted > pending ? submitted - pending : 0;
    }
    bool open(const std::string &name) {
        close();
        if (getenv("BT4_RENDER_TEST") && getenv("BT4_VOICE_AUDIO_FAIL")) return false;
        SDL_AudioDeviceID id = device(recording, name);
        if (!id) return false;
        SDL_AudioSpec spec = {SDL_AUDIO_S16, 1, 16000};
        stream = SDL_OpenAudioDeviceStream(id, &spec, nullptr, nullptr);
        if (!stream) return false;
        if (!SDL_ResumeAudioStreamDevice(stream)) { close(); return false; }
        return true;
    }
    int read(short *dst) {
        if (!stream) return 0;
        int available = SDL_GetAudioStreamAvailable(stream);
        short dropped[320];
        while (available > 4 * bytesPerFrame) {
            if (SDL_GetAudioStreamData(stream, dropped, bytesPerFrame) != bytesPerFrame) return 0;
            available -= bytesPerFrame;
        }
        return available >= bytesPerFrame ? SDL_GetAudioStreamData(stream, dst, bytesPerFrame) : 0;
    }
    void write(const short *src) {
        if (!stream) return;
        if (SDL_GetAudioStreamQueued(stream) >= 4 * bytesPerFrame) return;
        if (!SDL_PutAudioStreamData(stream, src, bytesPerFrame)) { close(); return; }
        submitted += 320;
    }
    void close() {
        if (stream) SDL_DestroyAudioStream(stream);
        stream = nullptr; submitted = 0;
    }
};
