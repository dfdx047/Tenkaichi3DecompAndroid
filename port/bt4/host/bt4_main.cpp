// Budokai Tenkaichi 4 on Android (port/bt4): libbt4.so, started by Bt4Activity (SDL3's activity) with
//   argv[1] the game folder (bt4/), argv[2] the app's native libraries, argv[3] the disc image (/proc/self/fd/<n>).
//
// Two halves in one process, as in BT3-Recomp's browser build (ps2xRuntime/web), with Dragon Rage's GS in place of
// Play!'s:
//   - the game: Team BT4's mod recompiled (BT3-Recomp + dbz-bt4-recomp), its runtime's own main() on a thread of its
//     own, with no window (the headless raylib, bt3gl_android.cpp). It sends what it draws as a live GS stream
//     (ps2_gs_live.cpp, the format of a PCSX2 GS dump) into a ring in this process's memory;
//   - the picture: this thread (SDL's, the app's main thread for SDL) reads the ring and hands it to Dragon Rage's
//     GS, port/src/gs, exactly the renderer of Budokai Tenkaichi 3 (Vulkan or OpenGL ES, its settings, texture
//     packs, the on-screen controller and the gamepads of gs_input.c), and sends the pad back through the ring.
#include <sched.h>
#include <SDL3/SDL.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <jni.h>

#include <atomic>
#include <cerrno>
#include <map>
#include <mutex>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <pthread.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "ps2_live_ring.h"

// the runtime's disc image reader (ps2_iop.cpp, our patch): the image through its descriptor
int ps2xImageFd(const std::string &path);
bool ps2xImageRead(int fd, uint64_t offset, uint8_t *dst, size_t bytes);

extern "C" {
void Gs_Gif(const uint8_t *p, uint32_t qwc);                   /* gs_core.c: GIF data, whole quadwords */
void Port_GsGifChannel(uint32_t addr, uint32_t qwc, int chain); /* gs_core.c: (here only to start the GS) */
int GsGpu_Enabled(void);
void GsGpu_FrameEnd(void);
void GsGpu_PollEvents(void);
int Port_PadRead(int socket, unsigned char *data);             /* gs_input.c: controller, keyboard, on-screen pad */
void Port_TouchPad(unsigned buttons, int lx, int ly, int rx, int ry);
int Port_Setting(const char *name, int def);
extern int gGsMainFbp;
extern int gGsOtherFbp;
extern unsigned gPortVBlanks;
extern unsigned gGsFrame;                                       /* gs_core.c: the pictures (the texture cache ages by it) */
void Bt4_Quit(void);
int GsGpu_PauseFrame(void);                                    /* gs_gpu.c: the last picture again (and the events) */
int GsGpu_PresentGet(void);
void GsGpu_PresentSet(int mode);
extern int gGsFragDepth;                                       /* gs_draw.c */
void Port_SettingSave(const char *name, int value);
void Port_SettingsWrite(void);
int Bt4_AspectMode(void);                                      /* bt4_shim.c: bt4_aspect, 0 16:9, 1 4:3, 2 the screen */
void Bt4_AspectSet(int mode);
void GsAdpf_Frame(uint64_t workNs, uint64_t targetNs);         /* gs_adpf.c: the system's performance hints */
SDL_Window *GsDraw_WindowCreate(int opengl);                   /* gs_draw.c: the game's window (its shape, full screen) */
int PortInput_ComboPoll(void);                                 /* gs_input.c: START + SELECT held */
extern int gPortPadAlways;                                     /* gs_input.c: the pads without Dragon Rage's renderer */
void Bt4_OpenMenu(void);                                       /* (below) the app's touch menu */
}
void ps2Set60Fps(bool on, const char *sitesPath);              /* BT3-Recomp (ps2_stepcensus.cpp): 60 fps fights */
uint64_t ps2FightTicks();                                      /* (the same): the fight's updates so far */

// BT3-Recomp's pacing table of the 60 fps fights (games/bt3/fps60_sites.txt, which the mod keeps: it has the USA
// layout), built into this library by CMakeLists.txt and written to the game folder for the runtime to load.
static const char kFps60Sites[] =
#include "fps60_sites.inc"
    ;
#include "../../src/gs/ui.h"                                   /* PortVideo, GsGpu_GetSettings / GsGpu_SetSettings */

int ps2x_runtime_main(int argc, char *argv[]); // the runtime's main() (src/main.cpp, renamed by the build)

namespace
{
const char *TAG = "bt4";
std::string g_root, g_image, g_libDir, g_driverDir, g_driverLib;
std::atomic<ps2live::RingHeader *> g_ring{nullptr};
std::atomic<bool> g_quit{false};
std::atomic<bool> g_gsxRunning{false}; // ARMSX2's renderer draws the game (not Dragon Rage's)
FILE *g_log = nullptr;
int g_pipe[2] = {-1, -1};

// ---- the log: stdout and stderr to logcat ("bt4") and to bt3_log.txt in the game folder (the launcher's log
// export and its "the game closed" check read it, as for BT3), the two runs before kept as bt3_log.1/2.txt
void *logThread(void *)
{
    char buf[2048];
    size_t used = 0;
    for (;;)
    {
        ssize_t n = read(g_pipe[0], buf + used, sizeof(buf) - 1 - used);
        if (n <= 0)
        {
            if (n < 0 && errno == EINTR)
                continue;
            break;
        }
        used += static_cast<size_t>(n);
        for (;;)
        {
            char *nl = static_cast<char *>(std::memchr(buf, '\n', used));
            size_t len = nl ? static_cast<size_t>(nl - buf) : used == sizeof(buf) - 1 ? used : 0;
            if (len == 0 && !nl)
                break;
            buf[len] = '\0';
            __android_log_write(ANDROID_LOG_INFO, TAG, buf);
            if (g_log)
            {
                std::fprintf(g_log, "%s\n", buf);
                std::fflush(g_log);
            }
            len += nl != nullptr;
            std::memmove(buf, buf + len, used - len);
            used -= len;
        }
    }
    return nullptr;
}

void redirectOutput(const std::string &dir)
{
    const std::string path = dir + "/bt3_log.txt", old = dir + "/bt3_log.1.txt", older = dir + "/bt3_log.2.txt";
    std::rename(old.c_str(), older.c_str());
    std::rename(path.c_str(), old.c_str());
    g_log = std::fopen(path.c_str(), "w");
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    if (pipe(g_pipe) == 0)
    {
        dup2(g_pipe[1], 1);
        dup2(g_pipe[1], 2);
        pthread_t th;
        pthread_create(&th, nullptr, logThread, nullptr);
        pthread_detach(th);
    }
}

// ---- the ring (the consumer's side; ps2_live_ring.h, as web/play_gs_host.cpp reads it)
struct Reader
{
    ps2live::RingHeader *ring = nullptr;
    uint8_t *data = nullptr;
    uint64_t capacity = 0;
    bool headerDone = false;
    std::vector<uint8_t> packet;
    std::vector<uint8_t> priv = std::vector<uint8_t>(8192u, 0u);
    uint64_t frames = 0, transfers = 0, bytes = 0;

    uint64_t available() const
    {
        const int64_t w = __atomic_load_n(&ring->writePos, __ATOMIC_ACQUIRE);
        return static_cast<uint64_t>(w - ring->readPos);
    }
    void peek(uint64_t skip, void *dst, uint64_t size) const
    {
        uint8_t *out = static_cast<uint8_t *>(dst);
        uint64_t pos = static_cast<uint64_t>(ring->readPos) + skip;
        while (size)
        {
            const uint64_t offset = pos % capacity;
            const uint64_t chunk = std::min(size, capacity - offset);
            std::memcpy(out, data + offset, chunk);
            out += chunk;
            pos += chunk;
            size -= chunk;
        }
    }
    template <typename T> T value(uint64_t skip) const
    {
        T v;
        peek(skip, &v, sizeof(T));
        return v;
    }
    void consume(uint64_t size)
    {
        __atomic_store_n(&ring->readPos, ring->readPos + static_cast<int64_t>(size), __ATOMIC_RELEASE);
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        if (ring->producerWaiting)
            ps2live::eventSet(&ps2live::ringEvents(ring)->space);
    }
};

constexpr uint8_t kTypeTransfer = 0, kTypeVSync = 1, kTypeReadFifo2 = 2, kTypeRegisters = 3;
constexpr uint8_t kPath2 = 1, kPath3 = 2, kPath1New = 3;
constexpr uint64_t kPrivRegsSize = 8192u;

// The shown frame buffer, from the privileged registers (PMODE: which read circuit is on; DISPFB1/2: its FBP).
void applyPriv(const std::vector<uint8_t> &priv)
{
    uint64_t pmode, dispfb1, dispfb2;
    std::memcpy(&pmode, priv.data() + 0x00, 8);
    std::memcpy(&dispfb1, priv.data() + 0x70, 8);
    std::memcpy(&dispfb2, priv.data() + 0x90, 8);
    const uint64_t dispfb = (pmode & 1u) || !(pmode & 2u) ? dispfb1 : dispfb2;
    if (gGsMainFbp >= 0 && gGsMainFbp != static_cast<int>(dispfb & 0x1FFu))
        gGsOtherFbp = gGsMainFbp;
    gGsMainFbp = static_cast<int>(dispfb & 0x1FFu);
}

void sendPad(ps2live::RingHeader *ring)
{
    unsigned char d[18];
    if ((!GsGpu_Enabled() && !gPortPadAlways) || Port_PadRead(0, d) <= 0) // (ARMSX2's renderer: gPortPadAlways)
        return;
    const unsigned buttons = static_cast<unsigned>(~(d[0] | (d[1] << 8))) & 0xFFFFu; // active high, libpad order
    ring->padButtons = buttons;
    ring->padAxes = static_cast<uint32_t>(d[4]) | (static_cast<uint32_t>(d[5]) << 8) | (static_cast<uint32_t>(d[2]) << 16) |
                    (static_cast<uint32_t>(d[3]) << 24); // lx, ly, rx, ry
    ring->padSequence = ring->padSequence + 1;
}

// Whole packets until a VSync (a picture ends: true) or until the data runs out (false).
bool processPackets(Reader &r)
{
    if (!r.headerDone)
    {
        if (r.available() < 8)
            return false;
        const uint32_t crc = r.value<uint32_t>(0), headerSize = r.value<uint32_t>(4);
        uint64_t stateSize = headerSize, headerBytes = 0;
        if (crc == 0xFFFFFFFFu)
        {
            headerBytes = headerSize;
            if (r.available() < 8 + headerBytes)
                return false;
            std::vector<uint8_t> header(headerBytes);
            r.peek(8, header.data(), headerBytes);
            uint32_t dumpStateSize = 0;
            std::memcpy(&dumpStateSize, header.data() + 4, sizeof(dumpStateSize));
            stateSize = dumpStateSize;
        }
        const uint64_t total = 8 + headerBytes + stateSize + kPrivRegsSize;
        if (r.available() < total)
            return false;
        r.peek(8 + headerBytes + stateSize, r.priv.data(), kPrivRegsSize);
        r.consume(total);
        applyPriv(r.priv);
        r.headerDone = true;
        std::fprintf(stderr, "bt4: GS stream connected\n");
    }
    for (;;)
    {
        const uint64_t avail = r.available();
        if (avail < 1)
            return false;
        switch (r.value<uint8_t>(0))
        {
        case kTypeTransfer:
        {
            if (avail < 6)
                return false;
            const uint8_t path = r.value<uint8_t>(1);
            const uint32_t length = r.value<uint32_t>(2);
            if (avail < 6ull + length)
                return false;
            r.packet.resize(length);
            r.peek(6, r.packet.data(), length);
            r.consume(6ull + length);
            if ((path == kPath1New || path == kPath2 || path == kPath3) && length >= 16)
            {
                Gs_Gif(r.packet.data(), length / 16u);
                r.transfers++;
                r.bytes += length;
            }
            break;
        }
        case kTypeVSync:
            if (avail < 2)
                return false;
            r.consume(2);
            return true;
        case kTypeReadFifo2:
            if (avail < 5)
                return false;
            r.consume(5); // (GS -> EE downloads: not answered yet; the game keeps its own data, as in the browser build)
            break;
        case kTypeRegisters:
            if (avail < 1 + kPrivRegsSize)
                return false;
            r.peek(1, r.priv.data(), kPrivRegsSize);
            r.consume(1 + kPrivRegsSize);
            applyPriv(r.priv);
            break;
        default:
            std::fprintf(stderr, "bt4: unknown packet 0x%02X in the GS stream, stopping\n", r.value<uint8_t>(0));
            r.ring->consumerQuit = 1;
            g_quit = true;
            return false;
        }
    }
}

// ---- the touch menu (GameMenu.kt through MenuBridge): the same keys as BT3's (bt3_settings.txt), those this
// engine has. While it is up the stream is not read: the game waits on its full ring, the last picture stays.
std::mutex g_menuLock;
std::atomic<int> g_menuWant{-1}; // asked by the app: 1 open, 0 closed
std::atomic<bool> g_menuOpen{false};
std::vector<std::pair<std::string, int>> g_menuQueue;
std::map<std::string, int> g_menuSnap;
int g_menuGen = 0;
std::string g_gpuName;
int g_gl = 0;
std::string g_fps60Path;

// 60 fps in fights (bt4_fps60): BT3-Recomp's mode (the fight at step 1 with its pacing table), as the mod's Windows and
// Mac builds offer it. The table is written next to the game's data on every start (it comes with this library).
void fps60Prepare()
{
    g_fps60Path = g_root + "/runtime/fps60_sites.txt";
    FILE *f = std::fopen(g_fps60Path.c_str(), "wb");
    if (f)
    {
        std::fwrite(kFps60Sites, 1, sizeof(kFps60Sites) - 1, f);
        std::fclose(f);
    }
    else
        std::fprintf(stderr, "bt4: %s cannot be written: %s\n", g_fps60Path.c_str(), std::strerror(errno));
    const bool on = Port_Setting("bt4_fps60", 0) != 0;
    // on: the table is loaded at the runtime's start (PS2X_HALFSTEP names it) and the runtime's own start applies the
    // mode (PS2X_FPS60, ps2_settings_overlay.cpp). Off: nothing loaded, the game exactly as before; the menu can still
    // turn it on later (ps2Set60Fps with the table's path).
    if (on)
        setenv("PS2X_HALFSTEP", g_fps60Path.c_str(), 1);
    else
        unsetenv("PS2X_HALFSTEP");
    setenv("PS2X_FPS60", on ? "1" : "0", 1);
    std::fprintf(stderr, "bt4: fights at %d fps\n", on ? 60 : 30);
}

// The 60 fps fights stay on only where the device holds them: below 57 of the fight's updates a second for 3 s in
// a row while a fight runs (more than 20 a second: not its pauses, not the menus), they go back to 30 (from the next
// fight, as the runtime switches) and the app says so (Bt4Activity.fps60Dropped). bt4_fps60_dropped=1 keeps it known
// for the settings; turning the option on again clears it. BT4_FPS60_KEEP=1: never (measuring).
void fps60Watch()
{
    std::thread([] {
        uint64_t last = ps2FightTicks();
        auto at = std::chrono::steady_clock::now();
        int slow = 0;
        while (!g_quit)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            const uint64_t now = ps2FightTicks();
            const auto t = std::chrono::steady_clock::now();
            const double rate = (now - last) / std::chrono::duration<double>(t - at).count();
            last = now;
            at = t;
            if (Port_Setting("bt4_fps60", 0) == 0 || std::getenv("BT4_FPS60_KEEP") != nullptr)
            {
                slow = 0;
                continue;
            }
            slow = rate > 20.0 && rate < 57.0 ? slow + 1 : 0;
            if (slow < 3)
                continue;
            std::fprintf(stderr, "bt4: 60 fps fights: %.1f updates a second for 3 s, back to 30\n", rate);
            Port_SettingSave("bt4_fps60", 0);
            Port_SettingSave("bt4_fps60_dropped", 1);
            Port_SettingsWrite();
            ps2Set60Fps(false, g_fps60Path.c_str());
            JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
            jobject act = env ? static_cast<jobject>(SDL_GetAndroidActivity()) : nullptr;
            jclass cls = act ? env->GetObjectClass(act) : nullptr;
            jmethodID m = cls ? env->GetMethodID(cls, "fps60Dropped", "()V") : nullptr;
            if (m)
                env->CallVoidMethod(act, m);
            if (env && env->ExceptionCheck())
                env->ExceptionClear();
            slow = 0;
        }
    }).detach();
}

// The renderer: the player's choice (bt4_renderer) or, with none, the device's: bt4_renderer_auto, which the launcher
// sets to 0 on a weak GPU and armsx2Watch below on a device that does not hold ARMSX2's.
int bt4Renderer()
{
    return Port_Setting("bt4_renderer", Port_Setting("bt4_renderer_auto", 1));
}

// ARMSX2's renderer where the player did not choose it: below 27 of the fight's updates a second for 5 s in a row while
// a fight runs (more than 5 a second: not its pauses, not the menus), Dragon Rage's from the next start, and the app
// says so (Bt4Activity.armsx2Dropped). BT4_RENDERER_KEEP=1: never (measuring).
void armsx2Watch()
{
    if (Port_Setting("bt4_renderer", -1) != -1 || std::getenv("BT4_RENDERER_KEEP") != nullptr)
        return;
    std::thread([] {
        uint64_t last = ps2FightTicks();
        auto at = std::chrono::steady_clock::now();
        int slow = 0;
        while (!g_quit)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            const uint64_t now = ps2FightTicks();
            const auto t = std::chrono::steady_clock::now();
            const double rate = (now - last) / std::chrono::duration<double>(t - at).count();
            last = now;
            at = t;
            if (Port_Setting("bt4_renderer", -1) != -1)
                return; // (the player chose in the game's menu)
            slow = rate > 5.0 && rate < 27.0 ? slow + 1 : 0;
            if (slow < 5)
                continue;
            std::fprintf(stderr, "bt4: ARMSX2's renderer: %.1f updates a second for 5 s, Dragon Rage's from the next start\n", rate);
            Port_SettingSave("bt4_renderer_auto", 0);
            Port_SettingsWrite();
            JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
            jobject act = env ? static_cast<jobject>(SDL_GetAndroidActivity()) : nullptr;
            jclass cls = act ? env->GetObjectClass(act) : nullptr;
            jmethodID m = cls ? env->GetMethodID(cls, "armsx2Dropped", "()V") : nullptr;
            if (m)
                env->CallVoidMethod(act, m);
            if (env && env->ExceptionCheck())
                env->ExceptionClear();
            return;
        }
    }).detach();
}

void menuSnapshot()
{
    PortVideo v;
    GsGpu_GetSettings(&v);
    std::lock_guard<std::mutex> lock(g_menuLock);
    g_menuSnap.clear();
    g_menuSnap["scale4"] = v.scale4;
    g_menuSnap["filter"] = v.filter;
    g_menuSnap["smooth_2d"] = v.smooth2d;
    g_menuSnap["texture_pack"] = v.texPack;
    g_menuSnap["texture_count"] = v.texPackCount;
    g_menuSnap["gpu_api"] = Port_Setting("gpu_api", 0) == 1 ? 1 : 0;
    g_menuSnap["running_gl"] = g_gl;
    g_menuSnap["frag_depth"] = gGsFragDepth;
    g_menuSnap["present"] = g_gl ? 0 : GsGpu_PresentGet();
    g_menuSnap["fx_off"] = v.fxOff;
    g_menuSnap["glow"] = v.glow;
    g_menuSnap["bt4_aspect"] = Bt4_AspectMode();
    g_menuSnap["bt4_fps60"] = Port_Setting("bt4_fps60", 0);
    g_menuSnap["bt4_fps60_dropped"] = Port_Setting("bt4_fps60_dropped", 0);
    g_menuSnap["bt4_renderer"] = bt4Renderer();
    g_menuGen++;
}

void menuApply(const std::string &key, int value)
{
    PortVideo v, was;
    GsGpu_GetSettings(&v);
    was = v;
    if (key == "scale4") v.scale4 = value;
    else if (key == "filter") v.filter = value;
    else if (key == "smooth_2d") v.smooth2d = value;
    else if (key == "texture_pack") v.texPack = value;
    else if (key == "fx_off") v.fxOff = value;
    else if (key == "glow") v.glow = value;
    else if (key == "gpu_api")
    {
        Port_SettingSave("gpu_api", value ? 1 : 0);
        Port_SettingSave("dr_renderer", (value ? 1 : 0) + 1);
        Port_SettingsWrite();
    }
    else if (key == "frag_depth")
    {
        gGsFragDepth = value != 0;
        Port_SettingSave("frag_depth", gGsFragDepth);
        Port_SettingsWrite();
    }
    else if (key == "bt4_aspect")
    {
        Port_SettingSave("bt4_aspect", value);
        Port_SettingsWrite();
        Bt4_AspectSet(value);
    }
    else if (key == "bt4_renderer")
    {
        Port_SettingSave("bt4_renderer", value < 0 ? 0 : value > 2 ? 2 : value); // 0 Dragon Rage, 1 ARMSX2, 2 Nominum (from the next start)
        Port_SettingsWrite();
    }
    else if (key == "bt4_fps60")
    {
        Port_SettingSave("bt4_fps60", value ? 1 : 0);
        if (value)
            Port_SettingSave("bt4_fps60_dropped", 0);
        Port_SettingsWrite();
        ps2Set60Fps(value != 0, g_fps60Path.c_str()); // (the runtime switches at its next frame)
        std::fprintf(stderr, "bt4: fights at %d fps (from the menu)\n", value ? 60 : 30);
    }
    else if (key == "present" && !g_gl)
    {
        GsGpu_PresentSet(value);
        Port_SettingsWrite();
    }
    if (std::memcmp(&v, &was, sizeof(v)) != 0)
        GsGpu_SetSettings(&v);
}

// On this thread, once a loop: the app's requests. true while the menu is up.
bool menuFrame()
{
    const int want = g_menuWant.exchange(-1);
    std::vector<std::pair<std::string, int>> q;
    {
        std::lock_guard<std::mutex> lock(g_menuLock);
        q.swap(g_menuQueue);
    }
    for (auto &e : q)
        menuApply(e.first, e.second);
    if (want == 1)
    {
        g_menuOpen = true;
        std::fprintf(stderr, "bt4: menu open, the game waits\n");
    }
    else if (want == 0 && g_menuOpen)
    {
        g_menuOpen = false;
        std::fprintf(stderr, "bt4: menu closed\n");
    }
    if (want == 1 || !q.empty())
        menuSnapshot();
    return g_menuOpen;
}

void quitToApp()
{
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject act = env ? static_cast<jobject>(SDL_GetAndroidActivity()) : nullptr;
    jclass cls = act ? env->GetObjectClass(act) : nullptr;
    jmethodID m = cls ? env->GetMethodID(cls, "quitToApp", "()V") : nullptr;
    if (!m)
    {
        if (env)
            env->ExceptionClear();
        std::fflush(nullptr);
        _exit(0);
    }
    env->CallVoidMethod(act, m);
    if (env->ExceptionCheck())
        env->ExceptionClear();
}

// ---- the renderer "ARMSX2" (bt4_renderer = 1): the stream drawn by ARMSX2's GS, libbt4gsx.so (port/bt4/gsx). This
// thread keeps the window, its events, the pad and the menu; libbt4gsx.so reads the ring on a thread of its own.
struct GsxArgs
{
    void *ring;
    void *window;
    int width, height;
    const char *root, *ini, *renderer;
    float upscale;
    const char *driverDir, *driverLib, *hookDir;
};
struct Gsx
{
    int (*run)(const GsxArgs *);
    void (*surfaceChanged)(void *, int, int);
    void (*surfaceDestroyed)(void);
    void (*hold)(int);
    void (*stop)(void);
    void (*gsDump)(const char *, int); // (optional: Bt4Gsx_GsDump, BT4_GS_DUMP)
};

// The launcher's texture packs (bt4/textures, one folder per pack): a .dds or .png at any depth below it. A pack
// switched off in the launcher is moved to textures_off, beside it.
static bool hasTexturePack(const std::string &dir, int depth = 0)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return false;
    bool found = false;
    while (dirent *e = readdir(d))
    {
        const std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        const size_t n = name.size();
        if (e->d_type == DT_DIR)
            found = depth < 16 && hasTexturePack(dir + "/" + name, depth + 1);
        else
            found = n > 4 && (strcasecmp(name.c_str() + n - 4, ".dds") == 0 || strcasecmp(name.c_str() + n - 4, ".png") == 0);
        if (found)
            break;
    }
    closedir(d);
    return found;
}

// The GS settings for this start (gsx/dbz.ini), from the launcher's: Team BT4's PCSX2 profile (dbz-bt4-recomp's
// gshost/dbz.ini) with the internal resolution, the shape and the texture offset of that resolution.
float gsxWriteIni(const std::string &path)
{
    const int scale4 = Port_Setting("scale4", 8);
    float upscale = scale4 / 4.0f;
    if (upscale < 1.0f)
        upscale = 1.0f;
    // the texture offset that removes the doubled cel outline, per internal resolution (measured by dbz-bt4-recomp)
    const int s = static_cast<int>(upscale + 0.5f);
    const int tc = s <= 1 ? 0 : s == 2 ? 345 : s <= 5 ? 425 : 500;
    const int aspect = Bt4_AspectMode();
    // (read by ARMSX2 from BT4GSX_TEXTURES, set at the start: its own folder is <textures>/<serial>/replacements)
    const bool packs = hasTexturePack(g_root + "/textures");
    std::fprintf(stderr, "bt4: texture packs for ARMSX2's renderer: %s\n", packs ? "on (bt4/textures)" : "none in bt4/textures");
    FILE *f = std::fopen(path.c_str(), "w");
    if (!f)
        return upscale;
    std::fprintf(f,
                 "; Budokai Tenkaichi 4 on ARMSX2's GS: written by the game at every start from the launcher's settings\n"
                 "[EmuCore/GS]\n"
                 "upscale_multiplier = %g\n"
                 "MaxAnisotropy = 0\n"
                 "filter = 2\n"
                 "fxaa = false\n"
                 "CASMode = 0\n"
                 "AspectRatio = %s\n"
                 "OsdShowFPS = %s\n"
                 "VsyncEnable = 0\n"
                 "pcrtc_antiblur = false\n"
                 "pcrtc_offsets = false\n"
                 "UserHacks = true\n"
                 "UserHacks_merge_pp_sprite = true\n"
                 "UserHacks_CPUCLUTRender = 1\n"
                 "UserHacks_TCOffsetX = %d\n"
                 "UserHacks_TCOffsetY = %d\n"
                 "LoadTextureReplacements = %s\n"
                 "DumpReplaceableTextures = false\n",
                 static_cast<double>(upscale), aspect == 1 ? "4:3" : aspect == 2 ? "Stretch" : "16:9",
                 Port_Setting("meter", 0) ? "true" : "false", tc, tc, packs ? "true" : "false");
    std::fclose(f);
    return upscale;
}

void *gsxWindowHandle(SDL_Window *w, int *pw, int *ph)
{
    SDL_GetWindowSizeInPixels(w, pw, ph);
    return SDL_GetPointerProperty(SDL_GetWindowProperties(w), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
}

bool menuFrame();

// The surface for ARMSX2's GS, from Bt4Activity's own SurfaceHolder.Callback (Java, the UI thread, inside Android's
// surfaceCreated/Changed/Destroyed): the window is handed over and taken back while it is valid. SDL's lifecycle events
// came too late for that (a destroyed surface, then a stale SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER read at the
// foreground event, before SDL had the new one): known bug #12. Without the callback (an older app), the events below.
static std::atomic<Gsx *> g_gsxLive{nullptr};
static std::atomic<bool> g_surfaceHook{false};

extern "C" JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_Bt4Activity_gsxSurface(JNIEnv *env, jobject, jobject surface, jint w, jint h)
{
    g_surfaceHook = true;
    Gsx *g = g_gsxLive.load();
    if (!g)
        return; // (ARMSX2's GS not running: Dragon Rage's renderer has SDL's window)
    if (!surface)
    {
        g->surfaceDestroyed(); // (returns once the GS has dropped the swapchain, the surface still alive)
        return;
    }
    ANativeWindow *win = ANativeWindow_fromSurface(env, surface);
    if (!win)
        return;
    g->surfaceChanged(win, w > 0 ? w : ANativeWindow_getWidth(win), h > 0 ? h : ANativeWindow_getHeight(win)); // (it takes its own reference)
    ANativeWindow_release(win);
}

// Runs the game with ARMSX2's GS: false when it cannot (the library is missing or the stream never came), and the
// caller then uses Dragon Rage's renderer.
bool runArmsx2()
{
    const std::string lib = g_libDir + "/libbt4gsx.so";
    // Nominum Render's libbt4gsx.so (the same exports, ARMSX2 behind it): no game hooks for BT4, everything goes through
    // the GS (its hooks/un5 would take BT4's FRAME writes for Naruto's). Read when the library loads; Dragon Rage's own
    // library ignores it.
    setenv("NOMINUM_GAME", "none", 0);
    void *h = dlopen(lib.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h)
    {
        std::fprintf(stderr, "bt4: ARMSX2's renderer is not in this version (%s): Dragon Rage's is used\n", dlerror());
        return false;
    }
    Gsx g;
    g.run = reinterpret_cast<int (*)(const GsxArgs *)>(dlsym(h, "Bt4Gsx_Run"));
    g.surfaceChanged = reinterpret_cast<void (*)(void *, int, int)>(dlsym(h, "Bt4Gsx_SurfaceChanged"));
    g.surfaceDestroyed = reinterpret_cast<void (*)(void)>(dlsym(h, "Bt4Gsx_SurfaceDestroyed"));
    g.hold = reinterpret_cast<void (*)(int)>(dlsym(h, "Bt4Gsx_Hold"));
    g.stop = reinterpret_cast<void (*)(void)>(dlsym(h, "Bt4Gsx_Stop"));
    g.gsDump = reinterpret_cast<void (*)(const char *, int)>(dlsym(h, "Bt4Gsx_GsDump"));
    if (!g.run || !g.surfaceChanged || !g.surfaceDestroyed || !g.hold || !g.stop)
    {
        std::fprintf(stderr, "bt4: libbt4gsx.so is not the expected one: Dragon Rage's renderer is used\n");
        return false;
    }
    {   // the core is built for one memory page size and does not start on another
        auto pageSize = reinterpret_cast<int (*)(void)>(dlsym(h, "Bt4Gsx_PageSize"));
        const long here = sysconf(_SC_PAGESIZE);
        if (!pageSize || pageSize() != here)
        {
            std::fprintf(stderr, "bt4: ARMSX2's renderer is built for %d-byte memory pages, this device has %ld: Dragon Rage's renderer is used\n",
                         pageSize ? pageSize() : 0, here);
            return false;
        }
    }
    const std::string root = g_root + "/gsx";
    if (access((root + "/resources/shaders").c_str(), R_OK) != 0)
    {
        std::fprintf(stderr, "bt4: %s/resources is missing (the app copies it): Dragon Rage's renderer is used\n", root.c_str());
        return false;
    }
    SDL_Window *win = GsDraw_WindowCreate(0);
    int w = 0, hgt = 0;
    void *native = win ? gsxWindowHandle(win, &w, &hgt) : nullptr;
    if (!native)
    {
        std::fprintf(stderr, "bt4: no window for ARMSX2's renderer (%s)\n", SDL_GetError());
        return false;
    }
    gPortPadAlways = 1;
    g_gsxRunning = true;
    const std::string ini = root + "/dbz.ini";
    const float upscale = gsxWriteIni(ini);
    static std::string sRoot, sIni, sRenderer, sDriverDir, sDriverLib, sHookDir;
    sRoot = root;
    sIni = ini;
    // bt4_renderer 2: Nominum Render's own backend (Vulkan; its GPU self-test falls back to ARMSX2's GS by itself)
    sRenderer = bt4Renderer() == 2 ? "nominum" : Port_Setting("gpu_api", 0) == 1 ? "gl" : "vulkan";
    if (sRenderer == "nominum" && !dlsym(h, "nominum_create"))
    {
        std::fprintf(stderr, "bt4: this libbt4gsx.so is not Nominum Render's: ARMSX2's GS instead\n");
        sRenderer = "vulkan";
    }
    sDriverDir = g_driverDir;
    sDriverLib = g_driverLib;
    sHookDir = g_libDir;
    std::fprintf(stderr, "bt4: GS host: ARMSX2's renderer (%s, %gx, window %dx%d)\n", sRenderer.c_str(), static_cast<double>(upscale), w, hgt);

    // the ring, once the game has made it
    ps2live::RingHeader *ring = nullptr;
    while (!g_quit && !(ring = g_ring.load()))
    {
        SDL_PumpEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!ring)
        return true;
    static GsxArgs a;
    a = {ring, native, w, hgt, sRoot.c_str(), sIni.c_str(), sRenderer.c_str(), upscale,
         sDriverDir.c_str(), sDriverLib.c_str(), sHookDir.c_str()};
    std::atomic<bool> ended{false};
    static Gsx sG;
    sG = g;
    g_gsxLive = &sG;
    std::thread gs([&] {
        const int code = g.run(&a);
        std::fprintf(stderr, "bt4: ARMSX2's renderer ended (%d)\n", code);
        g_gsxLive = nullptr;
        ended = true;
    });

    auto lastPad = std::chrono::steady_clock::now();
    auto lastDumpLook = lastPad;
    bool held = false;
    // BT4_GS_DUMP=<folder> (bt3_env.txt, for Nominum Render): PCSX2 GS dumps of the stream, by scene. A file
    // <bt4 folder>/gsdump_now.txt with "<name> [vsyncs]" (300 by default) asks for <folder>/<name>.gs.zst from the next
    // vsync on; the file is removed when it is taken (port/tools/lab/bt4route.sh writes it at each scene of the route).
    const char *dumpDir = std::getenv("BT4_GS_DUMP");
    if (dumpDir && dumpDir[0])
    {
        mkdir(dumpDir, 0755);
        std::fprintf(stderr, "bt4: GS dumps into %s (%s)\n", dumpDir, g.gsDump ? "on" : "this libbt4gsx.so has no Bt4Gsx_GsDump");
    }
    while (!g_quit && !ended)
    {
        if (dumpDir && dumpDir[0] && g.gsDump && std::chrono::steady_clock::now() - lastDumpLook >= std::chrono::milliseconds(250))
        {
            lastDumpLook = std::chrono::steady_clock::now();
            const std::string ask = g_root + "/gsdump_now.txt";
            if (FILE *f = std::fopen(ask.c_str(), "r"))
            {
                char name[128] = "";
                int frames = 300;
                const int n = std::fscanf(f, "%127s %d", name, &frames);
                std::fclose(f);
                std::remove(ask.c_str());
                if (n >= 1 && name[0])
                {
                    const std::string base = std::string(dumpDir) + "/" + name;
                    std::fprintf(stderr, "bt4: GS dump %s asked (%d vsyncs)\n", base.c_str(), frames);
                    g.gsDump(base.c_str(), frames > 0 ? frames : 300);
                }
            }
        }
        if (PortInput_ComboPoll())
            Bt4_OpenMenu();
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_EVENT_QUIT)
            {
                g.stop();
                std::fflush(nullptr);
                _exit(0);
            }
            // (with Bt4Activity's callback the surface comes and goes there, gsxSurface above: these two are for an older app)
            if (ev.type == SDL_EVENT_WILL_ENTER_BACKGROUND && !g_surfaceHook)
                g.surfaceDestroyed(); // (the surface goes: no presenting into it)
            else if ((ev.type == SDL_EVENT_DID_ENTER_FOREGROUND || ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) && !g_surfaceHook)
            {
                int nw = 0, nh = 0;
                if (void *n = gsxWindowHandle(win, &nw, &nh))
                    g.surfaceChanged(n, nw, nh);
            }
            else if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key == SDLK_AC_BACK)
                Bt4_OpenMenu();
        }
        const bool open = menuFrame();
        if (open != held)
        {
            held = open;
            g.hold(open ? 1 : 0); // the menu is up: the stream waits (the game with it), the last picture stays
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - lastPad >= std::chrono::milliseconds(8))
        {
            if (open)
            {
                ring->padButtons = 0;
                ring->padAxes = 0x80808080u;
                ring->padSequence = ring->padSequence + 1;
            }
            else
                sendPad(ring);
            lastPad = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
    g.stop();
    if (gs.joinable())
        gs.join();
    if (ended && !g_quit)
    {
        std::fprintf(stderr, "bt4: ARMSX2's renderer stopped (see the lines above): choose Dragon Rage's in the settings\n");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Budokai Tenkaichi 4",
                                 "ARMSX2's renderer stopped (the log says why). Choose the Dragon Rage renderer in the settings.", nullptr);
    }
    return true;
}
} // namespace

// The game, on a thread of its own (the runtime's main thread runs the game's fibers' scheduler); once.
static void startGame()
{
    static bool started = false;
    if (started)
        return;
    started = true;
    std::thread([] {
        static char arg0[] = "ps2EntryRunner";
        static char *args[] = {arg0, nullptr};
        const int code = ps2x_runtime_main(1, args);
        std::fprintf(stderr, "bt4: the game ended (%d)\n", code);
        g_quit = true;
    }).detach();
    fps60Watch();
}

// ps2_gs_live.cpp asks for the host once the ring is ready (in the browser build: the page): here it is this thread.
bool ps2xWebStartGsHost(ps2live::RingHeader *header)
{
    header->consumerFlags = 1u;
    header->consumerAlive = 1u;
    g_ring.store(header);
    std::fprintf(stderr, "bt4: GS host: Dragon Rage's renderer\n");
    return true;
}

// The runtime's start (main.cpp calls it first): its folders, the disc, the live stream, its arguments.
void ps2xAndroidPlatformInit(int &argc, char **&argv)
{
    static std::string exeDir = g_root + "/runtime";
    static std::string elf = g_root + "/runtime/data/SLUS_216.78";
    setenv("PS2X_EXEDIR", exeDir.c_str(), 1);   // its settings.toml and memory cards (runtime/savedata)
    setenv("PS2X_GS_LIVE", "1", 1);             // the GS stream to this process's host (above)
    setenv("PS2X_CD_IMAGE", g_image.c_str(), 1);
    setenv("PS2X_LIVE_SERIAL", "SLUS-21978", 0);
    setenv("PS2X_BARPACE", "0", 0);             // no GL in the runtime: its loop is paced by the headless raylib
    // The runtime's log_level 1 (its default) turns on three diagnostics for every player: PS2X_PROFILE (two atomic adds
    // on every branch of the game's code, ~1.7 million a second in a fight, for the [prof] line), PS2X_SCHED_DEBUG (the
    // [sched] [mutex] [wake] lines) and PS2X_MCLOG. Off here; bt3_env.txt can still turn them on (it is read first).
    setenv("PS2X_PROFILE", "0", 0);
    setenv("PS2X_SCHED_DEBUG", "0", 0);
    setenv("PS2X_MCLOG", "", 0);                // (it reads any first character but '\0' as on)
    static char arg0[] = "ps2EntryRunner";
    static char *args[] = {arg0, const_cast<char *>(elf.c_str()), nullptr};
    argc = 2;
    argv = args;
    std::fprintf(stderr, "bt4: game folder %s, disc %s\n", g_root.c_str(), g_image.c_str());
}

// (bt4_shim.c) the renderer's start: its GPU and back end, for the menu's lines
extern "C" void Bt4_UiInit(const char *gpu, int gl)
{
    g_gpuName = gpu ? gpu : "";
    g_gl = gl;
}

// (bt4_shim.c) START + SELECT, or the back key reaching the engine: the app's menu
extern "C" void Bt4_OpenMenu(void)
{
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject act = env ? static_cast<jobject>(SDL_GetAndroidActivity()) : nullptr;
    jclass cls = act ? env->GetObjectClass(act) : nullptr;
    jmethodID m = cls ? env->GetMethodID(cls, "openGameMenu", "()V") : nullptr;
    if (!m)
    {
        if (env)
            env->ExceptionClear();
        return;
    }
    env->CallVoidMethod(act, m);
    if (env->ExceptionCheck())
        env->ExceptionClear();
}

extern "C" void Bt4_Quit(void)
{
    if (ps2live::RingHeader *r = g_ring.load())
        r->consumerQuit = 1;
    g_quit = true;
    quitToApp();
}

// The on-screen controller (TouchBridge, as libmain.so answers it for BT3).
extern "C" JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativeSetPad(JNIEnv *, jclass, jint buttons, jint lx, jint ly,
                                                                                              jint rx, jint ry)
{
    Port_TouchPad(static_cast<unsigned>(buttons), lx, ly, rx, ry);
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativeMenuOpen(JNIEnv *, jclass) { return JNI_FALSE; }
extern "C" JNIEXPORT jint JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativePlayerTeamSize(JNIEnv *, jclass) { return -1; }

// The touch menu (MenuBridge, as libmain.so answers it for BT3).
extern "C" JNIEXPORT jboolean JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeOpen(JNIEnv *, jclass, jboolean open)
{
    g_menuWant = open ? 1 : 0;
    return GsGpu_Enabled() || g_gsxRunning ? JNI_TRUE : JNI_FALSE;
}
extern "C" JNIEXPORT jint JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeGen(JNIEnv *, jclass)
{
    std::lock_guard<std::mutex> lock(g_menuLock);
    return g_menuGen;
}
extern "C" JNIEXPORT jint JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeGet(JNIEnv *env, jclass, jstring key, jint def)
{
    const char *k = key ? env->GetStringUTFChars(key, nullptr) : nullptr;
    jint v = def;
    if (k)
    {
        std::lock_guard<std::mutex> lock(g_menuLock);
        auto it = g_menuSnap.find(k);
        if (it != g_menuSnap.end())
            v = it->second;
    }
    if (k)
        env->ReleaseStringUTFChars(key, k);
    return v;
}
extern "C" JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeSet(JNIEnv *env, jclass, jstring key, jint value)
{
    const char *k = key ? env->GetStringUTFChars(key, nullptr) : nullptr;
    if (!k)
        return;
    {
        std::lock_guard<std::mutex> lock(g_menuLock);
        g_menuQueue.emplace_back(k, value);
        auto it = g_menuSnap.find(k);
        if (it != g_menuSnap.end())
            it->second = value;
    }
    env->ReleaseStringUTFChars(key, k);
}
extern "C" JNIEXPORT jstring JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeText(JNIEnv *env, jclass, jstring key)
{
    const char *k = key ? env->GetStringUTFChars(key, nullptr) : nullptr;
    std::string out;
    if (k && std::strcmp(k, "gpu") == 0)
        out = g_gpuName;
    if (k)
        env->ReleaseStringUTFChars(key, k);
    return env->NewStringUTF(out.c_str());
}
extern "C" JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_MenuBridge_nativeAction(JNIEnv *, jclass, jint action)
{
    if (action == 1)
        g_menuWant = 0;
}

extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char *argv[])
{
    g_root = argc > 1 ? argv[1] : ".";
    g_libDir = argc > 2 ? argv[2] : "";
    g_image = argc > 3 ? argv[3] : "";
    g_driverDir = argc > 4 ? argv[4] : ""; // (the launcher's custom Vulkan driver, for ARMSX2's renderer)
    g_driverLib = argc > 5 ? argv[5] : "";
    if (chdir(g_root.c_str()) != 0)
    {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "cannot use the game folder %s: %s", g_root.c_str(), std::strerror(errno));
        return 1;
    }
    redirectOutput(g_root);
    std::fprintf(stderr, "bt4: Dragon Rage, Budokai Tenkaichi 4 engine\n");
    if (FILE *fp = std::fopen("bt3_env.txt", "r"))
    {   // NAME=value lines, the switches for testing (BT3_GS_VERBOSE=1, PS2X_...): as libbt3's loader does for BT3
        char line[512];
        while (std::fgets(line, sizeof(line), fp))
        {
            line[std::strcspn(line, "\r\n")] = '\0';
            char *eq = std::strchr(line, '=');
            if (line[0] != '#' && eq)
            {
                *eq = '\0';
                setenv(line, eq + 1, 1);
                std::fprintf(stderr, "bt4: env %s=%s\n", line, eq + 1);
            }
        }
        std::fclose(fp);
    }
    if (const char *cpus = std::getenv("BT3_TEST_CPUS"))
    {   // BT3_TEST_CPUS=0-5 (testing): a phone with fewer big cores, on a device without root (libbt3's loader does the same)
        int first, last;
        if (std::sscanf(cpus, "%d-%d", &first, &last) == 2 && first >= 0 && last >= first && last < CPU_SETSIZE)
        {
            cpu_set_t set;
            CPU_ZERO(&set);
            for (; first <= last; first++)
                CPU_SET(first, &set);
            std::fprintf(stderr, "bt4: test: only cpus %s: %d\n", cpus, sched_setaffinity(0, sizeof(set), &set));
        }
    }
    // (the image comes open, as /proc/self/fd/<n>: opening it again by its path may be refused, see ps2xImageFd)
    const int imageFd = ps2xImageFd(g_image);
    uint8_t probe = 0;
    if (g_image.empty() || (imageFd >= 0 ? !ps2xImageRead(imageFd, 0, &probe, 1) : access(g_image.c_str(), R_OK) != 0))
    {
        std::fprintf(stderr, "bt4: the disc image cannot be read (%s): set it up again in the launcher\n", g_image.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Budokai Tenkaichi 4",
                                 "The disc image cannot be read. Set it up again in the launcher.", nullptr);
        quitToApp();
        return 1;
    }

    // the settings by their full path: the game's own code may change the current folder later (its fioChdir), and
    // the renderer reads and writes bt3_settings.txt when it is asked (a setting changed in the menu)
    static std::string settingsPath = g_root + "/bt3_settings.txt";
    setenv("BT3_SETTINGS", settingsPath.c_str(), 1);
    // the launcher's texture packs, by their full path for the same reason: Dragon Rage's renderer looked for
    // "textures" in whatever folder the game was in ("no folder textures") and ARMSX2's in its own folder under
    // the serial, so a pack imported for BT4 was never used by either (bt3_env.txt can still name another folder)
    static std::string texturesPath = g_root + "/textures";
    setenv("BT3_TEXTURES", texturesPath.c_str(), 0);
    setenv("BT4GSX_TEXTURES", texturesPath.c_str(), 0);

    fps60Prepare();

    // the renderer: ARMSX2's (1, the default since Dragon Rage 0.9.0) or Dragon Rage's (0, experimental for BT4:
    // bt4_renderer). A build without libbt4gsx.so falls to Dragon Rage's below.
    std::fprintf(stderr, "bt4: renderer %s (%s)\n", bt4Renderer() == 1 ? "ARMSX2" : bt4Renderer() == 2 ? "Nominum" : "Dragon Rage",
                 Port_Setting("bt4_renderer", -1) != -1 ? "the player's choice" : "this device's default");
    // 1 and 2 both go through libbt4gsx.so: 2 is Nominum Render's own backend in it (a choice only, never automatic)
    if (bt4Renderer() == 1 || bt4Renderer() == 2)
    {
        armsx2Watch();
        startGame();
        if (runArmsx2())
        {
            std::fprintf(stderr, "bt4: leaving\n");
            std::fflush(nullptr);
            quitToApp();
            return 0;
        }
        std::fprintf(stderr, "bt4: ARMSX2's renderer could not start: Dragon Rage's instead\n");
    }

    // Dragon Rage's GS: its window and renderer (Vulkan or OpenGL ES, the launcher's choice), on this thread
    setenv("BT3_GS", "gpu", 1);
    Port_GsGifChannel(0, 0, 0);
    if (!GsGpu_Enabled())
    {
        std::fprintf(stderr, "bt4: the renderer could not start\n");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Budokai Tenkaichi 4", "The renderer could not start (see the log).", nullptr);
        quitToApp();
        return 1;
    }

    startGame();

    Reader r;
    auto lastPump = std::chrono::steady_clock::now();
    auto lastStat = lastPump;
    auto lastFrame = lastPump;
    uint64_t waitedNs = 0; // (this thread's waiting for the game's data since the last picture: not work)
    while (!g_quit)
    {
        ps2live::RingHeader *ring = g_ring.load();
        if (!ring || ring->magic != ps2live::kMagic)
        {
            menuFrame(); // (the menu's values, before the game's stream is there)
            GsGpu_PollEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (!r.ring)
        {
            r.ring = ring;
            r.data = ps2live::ringData(ring);
            r.capacity = ring->capacity;
        }
        if (menuFrame())
        {
            // the menu is up: the last picture again, no pad; the stream waits (the game with it)
            ring->padButtons = 0;
            ring->padAxes = 0x80808080u;
            ring->padSequence = ring->padSequence + 1;
            if (!GsGpu_PauseFrame())
                GsGpu_PollEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            lastPump = std::chrono::steady_clock::now();
            continue;
        }
        if (processPackets(r))
        {
            gPortVBlanks++;
            GsGpu_FrameEnd(); // the picture: drawn and shown (events polled there too)
            gGsFrame++;       // (BT3's GS counts its frames itself; without it the texture cache never let go: white squares)
            sendPad(ring);
            r.frames++;
            lastPump = std::chrono::steady_clock::now();
            {   // the performance hints: this picture's work (the time since the last one, less the waiting)
                const uint64_t span = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(lastPump - lastFrame).count());
                GsAdpf_Frame(span > waitedNs ? span - waitedNs : 0, 16683350ull);
                lastFrame = lastPump;
                waitedNs = 0;
            }
            if (lastPump - lastStat > std::chrono::seconds(10))
            {
                std::fprintf(stderr, "bt4: %llu pictures, %llu transfers, %.1f MB of GS data in the last 10 s\n",
                             static_cast<unsigned long long>(r.frames), static_cast<unsigned long long>(r.transfers), r.bytes / 1048576.0);
                r.frames = r.transfers = r.bytes = 0;
                lastStat = lastPump;
            }
            continue;
        }
        // nothing to draw yet: keep the window alive and the pad flowing, then wait for data
        const auto now = std::chrono::steady_clock::now();
        if (now - lastPump > std::chrono::milliseconds(16))
        {
            GsGpu_PollEvents();
            sendPad(ring);
            lastPump = now;
        }
        ring->consumerWaiting = 1u;
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        if (r.available() == 0)
        {
            const auto w0 = std::chrono::steady_clock::now();
            ps2live::eventWait(&ps2live::ringEvents(ring)->data, 5);
            waitedNs += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - w0).count());
        }
        ring->consumerWaiting = 0u;
    }
    std::fprintf(stderr, "bt4: leaving\n");
    std::fflush(nullptr);
    quitToApp();
    return 0;
}
