/* Edge-preserving menu artwork enlargement. Host renderer only: never changes game data or netplay state. */
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <SDL3/SDL.h>
#include "../../third_party/xbrz/xbrz.cpp"

extern "C" uint32_t *Bt4MenuHD_Upscale(const uint32_t *pixels, int w, int h, int factor) {
    if (!pixels || w < 2 || h < 2 || w > 1024 || h > 1024 || factor < 2 || factor > 6) { return nullptr; }
    const size_t count = static_cast<size_t>(w) * h;
    /* Alpha above 0x80 can carry GS pass identifiers. Do not reinterpret those textures. */
    for (size_t i = 0; i < count; ++i) { if ((pixels[i] >> 24) > 128) { return nullptr; } }
    uint32_t *source = static_cast<uint32_t *>(std::malloc(count * sizeof(uint32_t)));
    uint32_t *result = static_cast<uint32_t *>(std::malloc(count * factor * factor * sizeof(uint32_t)));
    if (!source || !result) { std::free(source); std::free(result); return nullptr; }
    for (size_t i = 0; i < count; ++i) {
        uint32_t c = pixels[i], a = c >> 24;
        /* GS RGBA, opaque=128 -> xBRZ ARGB, opaque=255. */
        source[i] = ((a * 255 + 64) / 128 << 24) | ((c & 255) << 16) | (c & 0xFF00) | ((c >> 16) & 255);
    }
    /* Use xBRZ's direct colour-distance implementation. The buffered variant
       creates a 64 MiB process-global table on this worker, whose destructor
       can run before the earlier atexit worker join. Direct distance needs
       no shared table or lifetime dependency; enlargement stays off-thread. */
    xbrz::scale(factor, source, result, w, h, xbrz::ColorFormat::ARGB_UNBUFFERED);
    std::free(source);
    for (size_t i = 0; i < count * factor * factor; ++i) {
        uint32_t c = result[i], a = c >> 24;
        result[i] = ((a * 128 + 127) / 255 << 24) | ((c & 255) << 16) | (c & 0xFF00) | ((c >> 16) & 255);
    }
    return result;
}

/* Menu selection must not wait for xBRZ. Only this worker enlarges artwork;
   the render thread keeps the original until a bounded upload can replace it.
   Input is an owned copy, never a pointer into GS or game scene memory. */
namespace {
struct Artwork {
    uint64_t key, used;
    int w, h, factor, state; // 0 free, 1 queued, 2 running, 3 ready, 4 unsupported
    size_t bytes;
    uint32_t *source, *pixels;
};
Artwork artwork[64]{};
SDL_Mutex *artLock;
SDL_Condition *artWake;
SDL_Thread *artWorker;
size_t artBytes;
uint64_t artTick;
bool artStopping;
constexpr size_t artBudget = size_t(64) << 20;

void discard(Artwork &item) {
    std::free(item.source); std::free(item.pixels);
    artBytes -= item.bytes;
    item = Artwork{};
}
int SDLCALL enlarge(void *) {
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    SDL_LockMutex(artLock);
    for (;;) {
        Artwork *item = nullptr;
        for (auto &candidate : artwork) if (candidate.state == 1) { item = &candidate; break; }
        if (artStopping) break;
        if (!item) { SDL_WaitCondition(artWake, artLock); continue; }
        item->state = 2;
        SDL_UnlockMutex(artLock);
        uint32_t *pixels = Bt4MenuHD_Upscale(item->source, item->w, item->h, item->factor);
        SDL_LockMutex(artLock);
        item->pixels = pixels;
        item->state = pixels ? 3 : 4;
        std::free(item->source); item->source = nullptr;
        size_t sourceBytes = size_t(item->w) * item->h * 4;
        item->bytes -= sourceBytes; artBytes -= sourceBytes;
    }
    SDL_UnlockMutex(artLock);
    return 0;
}
void stop_artwork() {
    SDL_LockMutex(artLock); artStopping = true; SDL_BroadcastCondition(artWake); SDL_UnlockMutex(artLock);
    SDL_WaitThread(artWorker, nullptr);
    // Locks remain valid until process shutdown, including a late renderer poll.
}
bool start_artwork() {
    static bool tried;
    if (!tried) {
        tried = true;
        artLock = SDL_CreateMutex(); artWake = SDL_CreateCondition();
        if (artLock && artWake) artWorker = SDL_CreateThread(enlarge, "bt4-menu-art", nullptr);
        if (artWorker) std::atexit(stop_artwork);
    }
    return artWorker != nullptr;
}
}

/* pixels provided: enqueue once. Null pixels: poll an existing request only.
   A null output pointer probes readiness without allocating an upload copy. */
extern "C" int Bt4MenuHD_Request(const uint32_t *pixels, int w, int h, int factor, uint64_t key, uint32_t **output) {
    if (output) *output = nullptr;
    if (!key || w < 2 || h < 2 || w > 1024 || h > 1024 || factor < 2 || factor > 6 || !start_artwork()) return -1;
    size_t sourceBytes = size_t(w) * h * 4, resultBytes = sourceBytes * factor * factor;
    SDL_LockMutex(artLock);
    if (artStopping) { SDL_UnlockMutex(artLock); return -1; }
    for (auto &item : artwork) {
        if (item.state && item.key == key && item.w == w && item.h == h && item.factor == factor) {
            item.used = ++artTick;
            int ready = item.state == 3 ? 1 : item.state == 4 ? -1 : 0;
            if (ready == 1 && output) {
                *output = static_cast<uint32_t *>(std::malloc(resultBytes));
                if (*output) std::memcpy(*output, item.pixels, resultBytes);
                else ready = -1;
            }
            SDL_UnlockMutex(artLock); return ready;
        }
    }
    if (!pixels || sourceBytes + resultBytes > artBudget) { SDL_UnlockMutex(artLock); return -1; }
    Artwork *slot = nullptr;
    for (;;) {
        for (auto &item : artwork) if (!item.state) { slot = &item; break; }
        if (slot && artBytes + sourceBytes + resultBytes <= artBudget) break;
        Artwork *old = nullptr;
        for (auto &item : artwork) if (item.state >= 3 && (!old || item.used < old->used)) old = &item;
        if (!old) { SDL_UnlockMutex(artLock); return -1; }
        discard(*old); slot = nullptr;
    }
    slot->source = static_cast<uint32_t *>(std::malloc(sourceBytes));
    if (!slot->source) { SDL_UnlockMutex(artLock); return -1; }
    std::memcpy(slot->source, pixels, sourceBytes);
    slot->key = key; slot->w = w; slot->h = h; slot->factor = factor;
    slot->used = ++artTick; slot->state = 1;
    slot->bytes = sourceBytes + resultBytes; artBytes += slot->bytes;
    SDL_SignalCondition(artWake); SDL_UnlockMutex(artLock);
    return 0;
}
