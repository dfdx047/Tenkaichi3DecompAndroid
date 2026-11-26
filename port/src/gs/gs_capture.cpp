// Screenshots and video recordings of the game's window (Windows; the SDL_GPU back end).
//
//   F12  a screenshot: the window's picture as it is shown (the fight panels, the settings window if it is open),
//        at the window's size, saved as screenshots/BT3_<date>_<time>.png next to the program (WIC's PNG encoder).
//   F10  starts / stops a recording: videos/BT3_<date>_<time>.mp4 next to the program, H.264 + AAC through Media
//        Foundation (the graphics card's encoder when it has one, so the game keeps its speed). The picture is the
//        window's, scaled down to fit 1920 x 1080 (never up), at most 60 pictures a second; the bit rate is set
//        from the size (about 7 Mbit/s at 1080p60, 3 at 720p): small files that still look clean in motion. The
//        sound is the game's whole mix (SDL's post-mix callback), AAC 192 kbit/s.
//
// The keys can be changed in bt3_settings.txt (key_screenshot, key_record: SDL scancodes). While a recording runs, a
// red square blinks in the window's corner (drawn after the picture is taken: it is not in the video), and the title
// says so.
//
// How a picture is taken without stalling the game: gs_gpu.c calls GsCapture_Encode after the frame's picture and
// the settings window are drawn; it draws them again into a texture of the window's size (as BT3_UI_SHOT does), for
// a recording scales that to the video's size, and queues a download into one of four transfer buffers; the frame is
// submitted with a fence (GsCapture_Submitted). GsCapture_Poll, once a frame, maps the buffers whose fence has
// signalled and hands the pixels to the writer thread, which encodes them. Nothing waits on the graphics card.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <codecapi.h>
#include <direct.h>
#endif
#include <SDL3/SDL.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdlib>

extern "C" {
void Ui_DrawAgain(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target);
int Port_Setting(const char *name, int def);
SDL_AudioDeviceID Port_AudioDevice(void);
}

#ifdef _WIN32
namespace {

// ---- what the keys asked for
std::atomic<int> sShotWanted{0};
std::atomic<int> sRecording{0};      // 1 while a recording runs
std::atomic<int> sStopWanted{0};
Uint64 sLastFrameNs;                 // when the last picture of the recording was taken
Uint64 sRecStartNs;
int sTitleSet;
std::string sOldTitle;

// ---- the textures and the downloads in flight
SDL_GPUTexture *sCapTex, *sEncTex, *sDotTex;
Uint32 sCapW, sCapH, sEncW, sEncH;
SDL_GPUTextureFormat sCapFmt = SDL_GPU_TEXTUREFORMAT_INVALID;
struct Pending {
    SDL_GPUTransferBuffer *tb = nullptr;
    Uint32 size = 0, w = 0, h = 0;
    SDL_GPUFence *fence = nullptr;
    int kind = 0;   // 0 free, 1 queued (fence set after submit), 2 waiting on its fence
    int shot = 0;   // a screenshot (else a picture of the recording; 2: a photo for the game, GsCapture_PhotoCrop)
    int bgr = 0;
    int rx = 0, ry = 0, rw = 0, rh = 0; // where the game's picture is in it
    Uint64 ns = 0;  // when it was taken
};
Pending sPend[4];
int sQueued = -1;
std::mutex sPhotoMu;            // the last photo the game asked for: whole window, BGR or RGB
std::vector<uint8_t> sPhoto;
Uint32 sPhotoW, sPhotoH;
int sPhotoBgr, sPhotoRect[4], sPhotoReady;   // the slot this frame queued (its fence comes with the submit)

std::string base_dir(const char *sub) {
    const char *b = SDL_GetBasePath();
    std::string d = std::string(b != nullptr ? b : "") + sub;
    _mkdir(d.c_str());
    return d;
}

std::string stamp_name(const char *dir, const char *ext) {
    time_t t = time(nullptr);
    struct tm tmv;
    char buf[64];
    localtime_s(&tmv, &t);
    strftime(buf, sizeof(buf), "BT3_%Y-%m-%d_%H-%M-%S", &tmv);
    std::string base = base_dir(dir) + "/" + buf, path = base + ext;
    for (int k = 2; GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES && k < 100; k++) {
        path = base + "_" + std::to_string(k) + ext;
    }
    return path;
}

std::wstring widen(const std::string &s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 0) {
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    }
    return w;
}

// ---- the screenshot: a PNG through WIC (on a thread of its own: encoding a large picture takes a moment)
void save_png(std::vector<uint8_t> px, Uint32 w, Uint32 h, int bgr) {
    std::string path = stamp_name("screenshots", ".png");
    std::thread([px = std::move(px), w, h, bgr, path]() mutable {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IWICImagingFactory *f = nullptr;
        IWICStream *st = nullptr;
        IWICBitmapEncoder *enc = nullptr;
        IWICBitmapFrameEncode *fr = nullptr;
        std::vector<uint8_t> rgb((size_t)w * h * 3);
        for (size_t i = 0, n = (size_t)w * h; i < n; i++) { // (24-bit BGR, the window has no transparency)
            const uint8_t *q = &px[i * 4];
            rgb[i * 3 + 0] = bgr ? q[0] : q[2];
            rgb[i * 3 + 1] = q[1];
            rgb[i * 3 + 2] = bgr ? q[2] : q[0];
        }
        bool ok = false;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))) &&
            SUCCEEDED(f->CreateStream(&st)) && SUCCEEDED(st->InitializeFromFilename(widen(path).c_str(), GENERIC_WRITE)) &&
            SUCCEEDED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) && SUCCEEDED(enc->Initialize(st, WICBitmapEncoderNoCache)) &&
            SUCCEEDED(enc->CreateNewFrame(&fr, nullptr)) && SUCCEEDED(fr->Initialize(nullptr)) && SUCCEEDED(fr->SetSize(w, h))) {
            WICPixelFormatGUID pf = GUID_WICPixelFormat24bppBGR;
            ok = SUCCEEDED(fr->SetPixelFormat(&pf)) && SUCCEEDED(fr->WritePixels(h, w * 3, (UINT)rgb.size(), rgb.data())) && SUCCEEDED(fr->Commit()) &&
                 SUCCEEDED(enc->Commit());
        }
        if (fr) fr->Release();
        if (enc) enc->Release();
        if (st) st->Release();
        if (f) f->Release();
        CoUninitialize();
        fprintf(stderr, "bt3: screenshot %s %s\n", ok ? "saved:" : "could not be saved:", path.c_str());
    }).detach();
}

// ---- the recording: a writer thread fed with pictures and sound
struct Frame {
    std::vector<uint8_t> px;
    Uint64 ns;
};
std::mutex sMu;
std::condition_variable sCv;
std::deque<Frame> sFrames;
std::deque<std::vector<int16_t>> sSound;
std::thread sWriter;
std::atomic<int> sWriterRun{0};
int sAudioRate, sAudioOn;

void SDLCALL postmix(void *, const SDL_AudioSpec *spec, float *buf, int buflen) {
    if (!sRecording.load() || spec->format != SDL_AUDIO_F32 || spec->channels < 1) {
        return;
    }
    int ch = spec->channels, frames = buflen / (int)sizeof(float) / ch;
    std::vector<int16_t> pcm((size_t)frames * 2);
    for (int i = 0; i < frames; i++) {
        float l = buf[i * ch], r = ch > 1 ? buf[i * ch + 1] : l;
        l = l < -1.0f ? -1.0f : l > 1.0f ? 1.0f : l;
        r = r < -1.0f ? -1.0f : r > 1.0f ? 1.0f : r;
        pcm[i * 2] = (int16_t)(l * 32767.0f);
        pcm[i * 2 + 1] = (int16_t)(r * 32767.0f);
    }
    std::lock_guard<std::mutex> g(sMu);
    if (sSound.size() < 400) {
        sSound.push_back(std::move(pcm));
        sCv.notify_one();
    }
}

void writer(std::string path, Uint32 w, Uint32 h, int bgr, int rate) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    IMFAttributes *attr = nullptr;
    IMFSinkWriter *sw = nullptr;
    DWORD vs = 0, as = 0;
    bool ok = false, audio = rate == 44100 || rate == 48000;
    // the bit rate: about 0.058 bits per pixel per picture (7.2 Mbit/s at 1920 x 1080, 60 a second)
    UINT32 bitrate = (UINT32)((double)w * h * 60.0 * 0.058);
    if (bitrate < 1500000) bitrate = 1500000;

    MFCreateAttributes(&attr, 2);
    attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    attr->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
    if (SUCCEEDED(MFCreateSinkWriterFromURL(widen(path).c_str(), nullptr, attr, &sw))) {
        IMFMediaType *out = nullptr, *in = nullptr;
        ok = SUCCEEDED(MFCreateMediaType(&out));
        if (ok) {
            out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
            out->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
            out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
            out->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
            MFSetAttributeSize(out, MF_MT_FRAME_SIZE, w, h);
            MFSetAttributeRatio(out, MF_MT_FRAME_RATE, 60, 1);
            MFSetAttributeRatio(out, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
            ok = SUCCEEDED(sw->AddStream(out, &vs));
            out->Release();
        }
        if (ok && SUCCEEDED(MFCreateMediaType(&in))) { // the pictures as they come: 32-bit BGRx, top line first
            in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
            in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
            in->SetUINT32(MF_MT_DEFAULT_STRIDE, w * 4);
            MFSetAttributeSize(in, MF_MT_FRAME_SIZE, w, h);
            MFSetAttributeRatio(in, MF_MT_FRAME_RATE, 60, 1);
            MFSetAttributeRatio(in, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
            ok = SUCCEEDED(sw->SetInputMediaType(vs, in, nullptr));
            in->Release();
        }
        if (ok && audio) {
            IMFMediaType *ao = nullptr, *ai = nullptr;
            if (SUCCEEDED(MFCreateMediaType(&ao))) {
                ao->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                ao->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
                ao->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                ao->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
                ao->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
                ao->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
                audio = SUCCEEDED(sw->AddStream(ao, &as));
                ao->Release();
            }
            if (audio && SUCCEEDED(MFCreateMediaType(&ai))) {
                ai->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                ai->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
                ai->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                ai->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
                ai->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
                ai->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
                ai->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, rate * 4);
                audio = SUCCEEDED(sw->SetInputMediaType(as, ai, nullptr));
                ai->Release();
            }
        }
        ok = ok && SUCCEEDED(sw->BeginWriting());
    }
    if (attr) attr->Release();
    if (!ok) {
        fprintf(stderr, "bt3: the recording could not start (%s)\n", path.c_str());
        sRecording = 0;
    } else {
        fprintf(stderr, "bt3: recording %ux%u, %.1f Mbit/s%s: %s\n", w, h, bitrate / 1e6, audio ? " with sound" : "", path.c_str());
    }
    LONGLONG soundAt = 0; // in 100 ns
    LONGLONG lastVideo = -1;
    while (ok) {
        std::deque<Frame> frames;
        std::deque<std::vector<int16_t>> sound;
        {
            std::unique_lock<std::mutex> g(sMu);
            sCv.wait_for(g, std::chrono::milliseconds(50), [] { return !sFrames.empty() || !sSound.empty() || !sWriterRun.load(); });
            frames.swap(sFrames);
            sound.swap(sSound);
        }
        for (Frame &f : frames) {
            IMFMediaBuffer *b = nullptr;
            IMFSample *s = nullptr;
            BYTE *p = nullptr;
            DWORD n = w * h * 4;
            LONGLONG t = (LONGLONG)((f.ns - sRecStartNs) / 100);
            if (t <= lastVideo) {
                continue;
            }
            if (SUCCEEDED(MFCreateMemoryBuffer(n, &b)) && SUCCEEDED(b->Lock(&p, nullptr, nullptr))) {
                if (bgr) {
                    memcpy(p, f.px.data(), n);
                } else { // (RGBA from the card: to BGRx)
                    const uint8_t *q = f.px.data();
                    for (DWORD i = 0; i < n; i += 4) {
                        p[i] = q[i + 2];
                        p[i + 1] = q[i + 1];
                        p[i + 2] = q[i];
                        p[i + 3] = 255;
                    }
                }
                b->Unlock();
                b->SetCurrentLength(n);
                if (SUCCEEDED(MFCreateSample(&s))) {
                    s->AddBuffer(b);
                    s->SetSampleTime(t);
                    s->SetSampleDuration(166667);
                    sw->WriteSample(vs, s);
                    s->Release();
                }
                lastVideo = t;
            }
            if (b) b->Release();
        }
        for (std::vector<int16_t> &pcm : sound) {
            if (!audio || pcm.empty()) {
                continue;
            }
            IMFMediaBuffer *b = nullptr;
            IMFSample *s = nullptr;
            BYTE *p = nullptr;
            DWORD n = (DWORD)(pcm.size() * 2);
            LONGLONG d = (LONGLONG)(pcm.size() / 2) * 10000000LL / rate;
            if (SUCCEEDED(MFCreateMemoryBuffer(n, &b)) && SUCCEEDED(b->Lock(&p, nullptr, nullptr))) {
                memcpy(p, pcm.data(), n);
                b->Unlock();
                b->SetCurrentLength(n);
                if (SUCCEEDED(MFCreateSample(&s))) {
                    s->AddBuffer(b);
                    s->SetSampleTime(soundAt);
                    s->SetSampleDuration(d);
                    sw->WriteSample(as, s);
                    s->Release();
                }
            }
            if (b) b->Release();
            soundAt += d;
        }
        if (!sWriterRun.load()) {
            std::lock_guard<std::mutex> g(sMu);
            if (sFrames.empty() && sSound.empty()) {
                break;
            }
        }
    }
    if (sw != nullptr) {
        if (ok) {
            sw->Finalize();
            fprintf(stderr, "bt3: recording saved: %s\n", path.c_str());
        }
        sw->Release();
    }
    MFShutdown();
    CoUninitialize();
}

void title(SDL_Window *win, const char *text) {
    if (win == nullptr) {
        return;
    }
    if (!sTitleSet) {
        const char *t = SDL_GetWindowTitle(win);
        sOldTitle = t != nullptr ? t : "";
        sTitleSet = 1;
    }
    SDL_SetWindowTitle(win, text != nullptr ? (sOldTitle + text).c_str() : sOldTitle.c_str());
}

void start_recording(SDL_Window *win) {
    if (sRecording.load() || sWriter.joinable()) {
        return;
    }
    SDL_AudioSpec spec;
    int frames = 0;
    SDL_AudioDeviceID dev = Port_AudioDevice();
    sAudioRate = dev != 0 && SDL_GetAudioDeviceFormat(dev, &spec, &frames) ? spec.freq : 0;
    sAudioOn = dev != 0 && sAudioRate != 0 && SDL_SetAudioPostmixCallback(dev, postmix, nullptr);
    {
        std::lock_guard<std::mutex> g(sMu);
        sFrames.clear();
        sSound.clear();
    }
    sRecStartNs = SDL_GetTicksNS();
    sLastFrameNs = 0;
    sRecording = 1;
    sStopWanted = 0;
    title(win, "   \xE2\x97\x8F REC (F10 stops)");
}

void stop_recording(SDL_Window *win) {
    if (!sRecording.load()) {
        return;
    }
    sRecording = 0;
    if (sAudioOn) {
        SDL_SetAudioPostmixCallback(Port_AudioDevice(), nullptr, nullptr);
        sAudioOn = 0;
    }
    sWriterRun = 0;
    sCv.notify_one();
    if (sWriter.joinable()) {
        sWriter.join(); // (the last pictures are written and the file closed: a moment)
    }
    title(win, nullptr);
}

SDL_GPUTexture *make_tex(SDL_GPUDevice *dev, SDL_GPUTextureFormat fmt, Uint32 w, Uint32 h) {
    SDL_GPUTextureCreateInfo ci;
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = fmt;
    ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = w;
    ci.height = h;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    return SDL_CreateGPUTexture(dev, &ci);
}

} // namespace

extern "C" {

void GsCapture_Quit(void);

// A key of the window's: 1 when it was one of these.
// A recording started or stopped by the game itself (Z Warriors' filmed showcase, BT3_ZW_SHOW).
extern "C" void Port_CaptureShot(void) { /* (a screenshot the test feed asks for: gs_input.c, BT3_PAD_FILE) */
    sShotWanted = 1;
}

extern "C" void Port_CaptureRecord(int on) {
    if (on && !sRecording.load()) {
        start_recording(nullptr);
    } else if (!on && sRecording.load()) {
        stop_recording(nullptr);
    }
}

int GsCapture_Key(const SDL_Event *ev) {
    static int shotKey = -1, recKey = -1;
    if (shotKey < 0) {
        shotKey = Port_Setting("key_screenshot", SDL_SCANCODE_F12);
        recKey = Port_Setting("key_record", SDL_SCANCODE_F10);
        atexit(GsCapture_Quit); /* (Esc quits with exit(): a recording is still closed properly) */
    }
    if (ev->type != SDL_EVENT_KEY_DOWN || ev->key.repeat) {
        return 0;
    }
    if ((int)ev->key.scancode == shotKey) {
        sShotWanted = 1;
        return 1;
    }
    if ((int)ev->key.scancode == recKey) {
        SDL_Window *win = SDL_GetWindowFromID(ev->key.windowID);
        if (sRecording.load()) {
            stop_recording(win);
        } else {
            start_recording(win);
        }
        return 1;
    }
    return 0;
}

// After the frame's picture and the settings window are drawn into the swapchain texture (bl: how the picture went
// there; sw x sh the window): the pictures this frame must give, queued for download. 1 when the frame has to be
// submitted with a fence (GsCapture_Submitted).
int GsCapture_Encode(SDL_GPUDevice *dev, SDL_GPUCommandBuffer *cmd, const SDL_GPUBlitInfo *bl, Uint32 sw, Uint32 sh, SDL_GPUTextureFormat fmt) {
    { /* BT3_CAPTURE_TEST=<shot>,<start>,<stop>: the keys pressed at those presented frames (for checking without hands) */
        static int at[3] = {-2, 0, 0}, n;
        if (at[0] == -2) {
            at[0] = -1;
            if (getenv("BT3_CAPTURE_TEST") != nullptr) {
                sscanf(getenv("BT3_CAPTURE_TEST"), "%d,%d,%d", &at[0], &at[1], &at[2]);
            }
        }
        n++;
        if (at[0] > 0 && n == at[0]) sShotWanted = 1;
        if (at[0] > 0 && n == at[1]) start_recording(nullptr);
        if (at[0] > 0 && n == at[2]) stop_recording(nullptr);
    }
    int shot = sShotWanted.load(), rec = sRecording.load();
    Uint64 now = SDL_GetTicksNS();
    sQueued = -1;
    if (!shot && rec && sLastFrameNs != 0 && now - sLastFrameNs < 16000000ULL) {
        rec = 0; // (60 a second at most)
    }
    if ((!shot && !rec) || sw == 0 || sh == 0) {
        return 0;
    }
    int slot = -1;
    for (int k = 0; k < 4; k++) {
        if (sPend[k].kind == 0) {
            slot = k;
            break;
        }
    }
    if (slot < 0) {
        return 0; // (all four still in flight: this picture is skipped)
    }
    if (sCapTex == nullptr || sCapW != sw || sCapH != sh || sCapFmt != fmt) {
        if (sCapTex) SDL_ReleaseGPUTexture(dev, sCapTex);
        if (sEncTex) SDL_ReleaseGPUTexture(dev, sEncTex);
        sCapTex = make_tex(dev, fmt, sw, sh);
        sEncTex = nullptr;
        sCapW = sw;
        sCapH = sh;
        sCapFmt = fmt;
    }
    if (sCapTex == nullptr) {
        return 0;
    }
    // the window's picture again, into the capture texture (the same blit and the same settings window)
    SDL_GPUBlitInfo b = *bl;
    b.destination.texture = sCapTex;
    SDL_BlitGPUTexture(cmd, &b);
    Ui_DrawAgain(cmd, sCapTex);

    SDL_GPUTexture *src = sCapTex;
    Uint32 w = sw, h = sh;
    if (!shot) { // the recording: scaled to fit 1920 x 1080, even sizes
        double k = SDL_min(1.0, SDL_min(1920.0 / sw, 1080.0 / sh));
        w = (Uint32)(sw * k) & ~1u;
        h = (Uint32)(sh * k) & ~1u;
        if (sRecording.load() && !sWriter.joinable()) { // (the writer starts with the first picture's size)
            sEncW = w;
            sEncH = h;
            sWriterRun = 1;
            sWriter = std::thread(writer, stamp_name("videos", ".mp4"), w, h,
                                  fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB, sAudioOn ? sAudioRate : 0);
        }
        w = sEncW;
        h = sEncH;
        if (w != sw || h != sh) {
            if (sEncTex == nullptr) {
                sEncTex = make_tex(dev, fmt, w, h);
            }
            if (sEncTex == nullptr) {
                return 0;
            }
            SDL_GPUBlitInfo d;
            SDL_zero(d);
            d.source.texture = sCapTex;
            d.source.w = sw;
            d.source.h = sh;
            d.destination.texture = sEncTex;
            d.destination.w = w;
            d.destination.h = h;
            d.load_op = SDL_GPU_LOADOP_DONT_CARE;
            d.filter = SDL_GPU_FILTER_LINEAR;
            SDL_BlitGPUTexture(cmd, &d);
            src = sEncTex;
        }
    }
    Pending &p = sPend[slot];
    Uint32 size = w * h * 4;
    if (p.tb == nullptr || p.size < size) {
        if (p.tb) SDL_ReleaseGPUTransferBuffer(dev, p.tb);
        SDL_GPUTransferBufferCreateInfo ti;
        SDL_zero(ti);
        ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        ti.size = size;
        p.tb = SDL_CreateGPUTransferBuffer(dev, &ti);
        p.size = p.tb != nullptr ? size : 0;
    }
    if (p.tb == nullptr) {
        return 0;
    }
    SDL_GPUTextureRegion r;
    SDL_zero(r);
    r.texture = src;
    r.w = w;
    r.h = h;
    r.d = 1;
    SDL_GPUTextureTransferInfo t;
    SDL_zero(t);
    t.transfer_buffer = p.tb;
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(cp, &r, &t);
    SDL_EndGPUCopyPass(cp);
    p.w = w;
    p.h = h;
    p.shot = shot;
    p.rx = (int)bl->destination.x;
    p.ry = (int)bl->destination.y;
    p.rw = (int)bl->destination.w;
    p.rh = (int)bl->destination.h;
    p.bgr = fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    p.ns = now;
    p.kind = 1;
    sQueued = slot;
    if (shot == 2) {
        fprintf(stderr, "bt3: hud photo: frame queued (%ux%u, picture at %d,%d %dx%d)\n", w, h, p.rx, p.ry, p.rw, p.rh);
        fflush(stderr);
    }
    if (shot) {
        sShotWanted = 0;
    } else {
        sLastFrameNs = now;
    }
    return 1;
}

// The fence of the frame that queued a picture.
void GsCapture_Submitted(SDL_GPUFence *fence) {
    if (sQueued >= 0 && sPend[sQueued].kind == 1) {
        sPend[sQueued].fence = fence;
        sPend[sQueued].kind = 2;
    } else if (fence != nullptr) {
        // (nothing waits on it)
    }
    sQueued = -1;
}

// Once a frame: the pictures the card has finished go to the PNG writer or the video writer.
void GsCapture_Poll(SDL_GPUDevice *dev) {
    for (int k = 0; k < 4; k++) {
        Pending &p = sPend[k];
        if (p.kind != 2 || p.fence == nullptr || !SDL_QueryGPUFence(dev, p.fence)) {
            continue;
        }
        SDL_ReleaseGPUFence(dev, p.fence);
        p.fence = nullptr;
        const uint8_t *px = (const uint8_t *)SDL_MapGPUTransferBuffer(dev, p.tb, false);
        if (px != nullptr) {
            std::vector<uint8_t> copy(px, px + (size_t)p.w * p.h * 4);
            SDL_UnmapGPUTransferBuffer(dev, p.tb);
            if (p.shot == 2) {
                std::lock_guard<std::mutex> g(sPhotoMu);
                sPhoto = std::move(copy);
                sPhotoW = p.w;
                sPhotoH = p.h;
                sPhotoBgr = p.bgr;
                sPhotoRect[0] = p.rx;
                sPhotoRect[1] = p.ry;
                sPhotoRect[2] = p.rw > 0 ? p.rw : (int)p.w;
                sPhotoRect[3] = p.rh > 0 ? p.rh : (int)p.h;
                sPhotoReady = 1;
                fprintf(stderr, "bt3: hud photo: frame downloaded\n");
                fflush(stderr);
            } else if (p.shot) {
                save_png(std::move(copy), p.w, p.h, p.bgr);
            } else if (sRecording.load() && p.w == sEncW && p.h == sEncH) {
                std::lock_guard<std::mutex> g(sMu);
                if (sFrames.size() < 8) { // (the encoder fell behind: a picture is dropped, the game does not wait)
                    sFrames.push_back(Frame{std::move(copy), p.ns});
                    sCv.notify_one();
                }
            }
        }
        p.kind = 0;
    }
}

// After the capture: the blinking red square in the window's corner while a recording runs (not in the video).
void GsCapture_Overlay(SDL_GPUDevice *dev, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swap, Uint32 sw, Uint32 sh, SDL_GPUTextureFormat fmt) {
    if (!sRecording.load() || ((SDL_GetTicks() / 500) & 1) || sw < 64 || sh < 64) {
        return;
    }
    if (sDotTex == nullptr) {
        sDotTex = make_tex(dev, fmt, 4, 4);
    }
    if (sDotTex == nullptr) {
        return;
    }
    static int filled;
    if (!filled) { // (red: cleared once)
        SDL_GPUColorTargetInfo ti;
        SDL_zero(ti);
        ti.texture = sDotTex;
        ti.load_op = SDL_GPU_LOADOP_CLEAR;
        ti.store_op = SDL_GPU_STOREOP_STORE;
        ti.clear_color.r = 0.9f;
        ti.clear_color.g = 0.08f;
        ti.clear_color.b = 0.08f;
        ti.clear_color.a = 1.0f;
        SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &ti, 1, nullptr));
        filled = 1;
    }
    Uint32 s = sh / 40;
    SDL_GPUBlitInfo b;
    SDL_zero(b);
    b.source.texture = sDotTex;
    b.source.w = 4;
    b.source.h = 4;
    b.destination.texture = swap;
    b.destination.x = s;
    b.destination.y = s;
    b.destination.w = s;
    b.destination.h = s;
    b.load_op = SDL_GPU_LOADOP_LOAD;
    b.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(cmd, &b);
}

// ---- the OpenGL renderer (gs_gl.c): it asks before swapping whether this frame is wanted (1 a screenshot, 2 a picture
// of the recording, 0 none), reads the back buffer and gives it here, bottom row first, RGBA.
int GsCapture_GLWants(void) {
    if (sShotWanted.load()) {
        return 1;
    }
    if (!sRecording.load()) {
        return 0;
    }
    Uint64 now = SDL_GetTicksNS();
    return sLastFrameNs != 0 && now - sLastFrameNs < 16000000ULL ? 0 : 2; // (60 a second at most)
}

void GsCapture_GLGive(const uint8_t *bottomUp, int sw, int sh) {
    if (bottomUp == nullptr || sw <= 0 || sh <= 0) {
        return;
    }
    if (sShotWanted.load()) {
        std::vector<uint8_t> px((size_t)sw * sh * 4);
        for (int y = 0; y < sh; y++) {
            memcpy(&px[(size_t)y * sw * 4], bottomUp + (size_t)(sh - 1 - y) * sw * 4, (size_t)sw * 4);
        }
        sShotWanted = 0;
        save_png(std::move(px), (Uint32)sw, (Uint32)sh, 0);
        return;
    }
    if (!sRecording.load()) {
        return;
    }
    Uint64 now = SDL_GetTicksNS();
    if (!sWriter.joinable()) { // (the writer starts with the first picture's size: fit in 1920 x 1080, even sizes)
        double k = SDL_min(1.0, SDL_min(1920.0 / sw, 1080.0 / sh));
        sEncW = (Uint32)(sw * k) & ~1u;
        sEncH = (Uint32)(sh * k) & ~1u;
        if (sEncW < 2 || sEncH < 2) {
            return;
        }
        sWriterRun = 1;
        sWriter = std::thread(writer, stamp_name("videos", ".mp4"), sEncW, sEncH, 0, sAudioOn ? sAudioRate : 0);
    }
    Uint32 w = sEncW, h = sEncH;
    std::vector<uint8_t> px((size_t)w * h * 4);
    for (Uint32 y = 0; y < h; y++) { // (nearest pixel, the rows turned the right way up)
        const uint8_t *row = bottomUp + (size_t)(sh - 1 - (int)((uint64_t)y * sh / h)) * sw * 4;
        uint8_t *o = &px[(size_t)y * w * 4];
        for (Uint32 x = 0; x < w; x++) {
            memcpy(o + x * 4, row + (size_t)((uint64_t)x * sw / w) * 4, 4);
        }
    }
    sLastFrameNs = now;
    std::lock_guard<std::mutex> g(sMu);
    if (sFrames.size() < 8) {
        sFrames.push_back(Frame{std::move(px), now});
        sCv.notify_one();
    }
}

// The program is closing: a recording is finished properly.
void GsCapture_Quit(void) {
    stop_recording(nullptr);
}

} // extern "C"

#else // (no Windows: nothing)
extern "C" {
int GsCapture_Key(const SDL_Event *) { return 0; }
void Port_CaptureShot(void) {}
int GsCapture_Encode(SDL_GPUDevice *, SDL_GPUCommandBuffer *, const SDL_GPUBlitInfo *, Uint32, Uint32, SDL_GPUTextureFormat) { return 0; }
void GsCapture_Submitted(SDL_GPUFence *) {}
void GsCapture_Poll(SDL_GPUDevice *) {}
void GsCapture_Overlay(SDL_GPUDevice *, SDL_GPUCommandBuffer *, SDL_GPUTexture *, Uint32, Uint32, SDL_GPUTextureFormat) {}
void GsCapture_Quit(void) {}
int GsCapture_GLWants(void) { return 0; }
void GsCapture_GLGive(const uint8_t *, int, int) {}
}
#endif
