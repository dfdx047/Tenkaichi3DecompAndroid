/* Host services only: no PS2 state, input barrier or game timing. */
#ifdef _WIN32
#include <windows.h>
extern "C" BOOLEAN WINAPI SystemFunction036(PVOID, ULONG);
using VoiceModule = HMODULE;
static VoiceModule voiceLibrary(const char *windows, const char *unixName, const char *macName) {
    (void)unixName; (void)macName;
    wchar_t name[128];
    if (!MultiByteToWideChar(CP_UTF8, 0, windows, -1, name, 128)) return nullptr;
    return LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
}
static void *voiceFunction(VoiceModule module, const char *name) {
    return reinterpret_cast<void *>(GetProcAddress(module, name));
}
static bool voiceRandom(unsigned char out[32]) { return SystemFunction036(out, 32) != 0; }
#else
using VoiceModule = SDL_SharedObject *;
static VoiceModule voiceLibrary(const char *windows, const char *unixName, const char *macName) {
    (void)windows;
#ifdef __APPLE__
    const char *name = macName;
#else
    (void)macName;
    const char *name = unixName;
#endif
    const char *base = SDL_GetBasePath();
    if (base) {
        std::string path = std::string(base) + "lib/" + name;
        if (auto module = SDL_LoadObject(path.c_str())) return module;
    }
    return SDL_LoadObject(name);
}
static void *voiceFunction(VoiceModule module, const char *name) {
    return reinterpret_cast<void *>(SDL_LoadFunction(module, name));
}
static bool voiceRandom(unsigned char out[32]) {
    FILE *file = fopen("/dev/urandom", "rb");
    if (!file) return false;
    bool ok = fread(out, 1, 32, file) == 32;
    fclose(file);
    if (!ok) memset(out, 0, 32);
    return ok;
}
#endif
