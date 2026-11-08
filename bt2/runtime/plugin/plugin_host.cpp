#include "plugin/plugin_host.h"
#include "host/frame_pacer.h"
#include "../../plugin/bt2_plugin.h"
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

extern "C" std::uint64_t Bt2Hw_LastFrame(void); // gs/hw/bt3_host.c
extern "C" void Bt2Hw_OverlaySprite(int fbp, int fbw, int psm, int width, int height, std::uint32_t slot, float x0, float y0, float x1, float y1, int alpha);

// One call of the game the library has taken over: what the engine needs to go on with it.
struct Bt2Call {
    std::uint8_t* rdram;
    R5900Context* c;
    PS2Runtime* runtime;
};

namespace {
constexpr int slots = 16;
EeRuntime* engine = nullptr;
std::atomic<const Bt2Plugin*> plugin{nullptr};
Bt2Hook hooks[slots] = {};
PS2Runtime::RecompiledFunction originals[slots] = {};
void (*music_mix)(std::int16_t*, std::size_t, unsigned) = nullptr;
std::string disc, pictures, online;

// The registers the library sees: a copy of what its functions use, in and out around everything it runs.
void toMirror(const R5900Context& c, Bt2Regs& m) {
    for (int i = 0; i < 32; ++i) {m.r[i].low = c.r[i].low; m.r[i].high = c.r[i].high; m.f[i] = c.f[i];}
    m.pc = c.pc;
}
void fromMirror(const Bt2Regs& m, R5900Context& c) {
    for (int i = 0; i < 32; ++i) {c.r[i].low = m.r[i].low; c.r[i].high = m.r[i].high; c.f[i] = m.f[i];}
    c.pc = m.pc;
}

template<int I> void thunk(std::uint8_t* rdram, R5900Context* c, PS2Runtime* runtime) {
    Bt2Regs mirror;
    toMirror(*c, mirror);
    Bt2Call call{rdram, c, runtime};
    hooks[I](rdram, &mirror, &call);
    fromMirror(mirror, *c);
}
template<int... I> constexpr std::array<PS2Runtime::RecompiledFunction, sizeof...(I)> thunks(std::integer_sequence<int, I...>) {return {thunk<I>...};}
const auto thunk_table = thunks(std::make_integer_sequence<int, slots>{});

void apiHook(std::uint32_t address, Bt2Hook fn, int slot) {
    if (slot < 0 || slot >= slots || !fn) return;
    hooks[slot] = fn;
    originals[slot] = engine->replaceFunction(address, thunk_table[static_cast<std::size_t>(slot)]);
}
void apiOriginal(Bt2Call* call, Bt2Regs* m, int slot) {
    if (slot < 0 || slot >= slots || !originals[slot]) return;
    fromMirror(*m, *call->c);
    originals[slot](call->rdram, call->c, call->runtime);
    toMirror(*call->c, *m);
}
void apiCall(Bt2Call* call, Bt2Regs* m, std::uint32_t target, std::uint32_t self) {
    fromMirror(*m, *call->c);
    call->runtime->dispatchGuestBranch(call->rdram, call->c, target, self, self, PS2Runtime::GuestBranchKind::DirectCall, "plugin");
    toMirror(*call->c, *m);
}
void apiReturn(Bt2Call* call, Bt2Regs* m, std::uint32_t ra, std::uint32_t self) {
    fromMirror(*m, *call->c);
    call->c->pc = ra;
    call->runtime->dispatchGuestBranch(call->rdram, call->c, ra, self, 0, PS2Runtime::GuestBranchKind::Return, "JR $ra");
    toMirror(*call->c, *m);
}
unsigned apiStreamPercent() {return engine->audio_mixer.stream_percent;}
void apiSetStreamPercent(unsigned percent) {engine->audio_mixer.stream_percent = percent;}
unsigned apiMasterPercent() {return engine->audio_mixer.master_percent;}
void apiSetMusicMix(void (*mix)(std::int16_t*, std::size_t, unsigned)) {
    music_mix = mix;
    host_music_mix = [](std::vector<std::int16_t>& out) {if (music_mix) music_mix(out.data(), out.size(), engine->audio_mixer.master_percent);};
}
void apiLog(const char* line) {std::fprintf(stderr, "%s\n", line);}
void apiSetSecondPad(int on) {
    engine->pad_second.store(on != 0, std::memory_order_relaxed);
    std::fprintf(stderr, "Plugin: second pad %s\n", on ? "on" : "off");
}
void apiSetFastForward(int on) {bt2_fast_forward.store(on != 0, std::memory_order_relaxed);}
void apiSetMasterPercent(unsigned percent) {engine->audio_mixer.master_percent = percent;}
// The pad service's question once per field (a second pad on): the library's answer, which may wait for the other player.
bool padExchange(const PadInput& local, PadInput& p1, PadInput& p2) {
    const Bt2Plugin* p = plugin.load(std::memory_order_acquire);
    if (!p || p->size < offsetof(Bt2Plugin, pad_field) + sizeof(p->pad_field) || !p->pad_field) return false;
    const std::uint8_t in[6] = {std::uint8_t(local.pressed), std::uint8_t(local.pressed >> 8), local.sticks[0], local.sticks[1], local.sticks[2], local.sticks[3]};
    std::uint8_t a[6] = {}, b[6] = {};
    if (!p->pad_field(in, a, b)) return false;
    p1.pressed = std::uint16_t(a[0] | a[1] << 8);
    p2.pressed = std::uint16_t(b[0] | b[1] << 8);
    for (int i = 0; i < 4; ++i) {p1.sticks[std::size_t(i)] = a[2 + i]; p2.sticks[std::size_t(i)] = b[2 + i];}
    return true;
}
}

bool installPlugin(EeRuntime& runtime, const std::string& library_dir, const std::string& pictures_dir, const std::string& online_dir) {
    static Bt2HostApi api;
    engine = &runtime;
    const std::string path = library_dir + "/libbt2online.so";
    void* library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) {std::fprintf(stderr, "Plugin: no libbt2online.so (%s): the main menu stays the game's\n", dlerror()); return false;}
    const auto init = reinterpret_cast<Bt2PluginInit>(dlsym(library, BT2_PLUGIN_INIT));
    if (!init) {std::fprintf(stderr, "Plugin: libbt2online.so has no %s\n", BT2_PLUGIN_INIT); return false;}
    disc = runtime.local_disc_path;
    pictures = pictures_dir;
    online = online_dir;
    api = {};
    api.api = BT2_PLUGIN_API;
    api.size = sizeof(api);
    api.ram = runtime.memory().data();
    api.disc_image = disc.c_str();
    api.pictures_dir = pictures.c_str();
    api.online_dir = online.c_str();
    api.hook = apiHook;
    api.call_original = apiOriginal;
    api.call = apiCall;
    api.ret = apiReturn;
    api.stream_percent = apiStreamPercent;
    api.set_stream_percent = apiSetStreamPercent;
    api.master_percent = apiMasterPercent;
    api.set_music_mix = apiSetMusicMix;
    api.last_frame = Bt2Hw_LastFrame;
    api.overlay_sprite = Bt2Hw_OverlaySprite;
    api.log = apiLog;
    api.set_second_pad = apiSetSecondPad;
    api.set_fast_forward = apiSetFastForward;
    api.set_master_percent = apiSetMasterPercent;
    runtime.on_pad_exchange = padExchange;
    const Bt2Plugin* given = init(&api);
    if (!given || given->api != BT2_PLUGIN_API) {std::fprintf(stderr, "Plugin: libbt2online.so does not take this engine (API %u)\n", given ? given->api : 0u); return false;}
    plugin.store(given, std::memory_order_release);
    std::fprintf(stderr, "Plugin: libbt2online.so loaded\n");
    return true;
}

// What the renderer and the app ask for: the library's answer, or nothing.
extern "C" int Bt2MenuItem_ArtSwap(unsigned char* rgba, int w, int h) {
    const Bt2Plugin* p = plugin.load(std::memory_order_acquire);
    return p && p->art_swap ? p->art_swap(rgba, w, h) : 0;
}
extern "C" std::uint32_t* Bt2MenuItem_Pixels(std::uint32_t slot, std::uint32_t* version, std::uint32_t* w, std::uint32_t* h) {
    const Bt2Plugin* p = plugin.load(std::memory_order_acquire);
    return p && p->pixels ? p->pixels(slot, version, w, h) : nullptr;
}
extern "C" std::uint32_t Bt2MenuItem_Version(std::uint32_t slot) {
    const Bt2Plugin* p = plugin.load(std::memory_order_acquire);
    return p && p->version ? p->version(slot) : 0;
}
extern "C" void Bt2MenuItem_Overlay(std::uint64_t dispfb, std::uint64_t display) {
    const Bt2Plugin* p = plugin.load(std::memory_order_acquire);
    if (p && p->overlay) p->overlay(dispfb, display);
}
extern "C" int Bt2MenuItem_OnlineState() {
    const Bt2Plugin* p = plugin.load(std::memory_order_acquire);
    return p && p->online_state ? p->online_state() : 0;
}
