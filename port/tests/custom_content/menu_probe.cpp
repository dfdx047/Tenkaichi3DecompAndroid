#include "../../src/gs/menu_hd.cpp"
#include <cassert>
#include <cstdio>
#include <vector>

int main() {
    std::vector<uint32_t> source(128 * 128);
    for (unsigned i = 0; i < source.size(); ++i) source[i] = 0x80000000u | ((i & 63) * 3) | ((i / 128) << 8);
    for (int factor : {2, 4}) {
        uint64_t key = 0x12340000u + factor;
        uint32_t *output = nullptr;
        assert(Bt4MenuHD_Request(source.data(), 128, 128, factor, key, nullptr) == 0);
        uint32_t *expected = Bt4MenuHD_Upscale(source.data(), 128, 128, factor);
        assert(expected);
        uint64_t start = SDL_GetTicks();
        while (Bt4MenuHD_Request(nullptr, 128, 128, factor, key, &output) == 0 && SDL_GetTicks() - start < 10000) SDL_Delay(1);
        assert(output && !std::memcmp(expected, output, source.size() * factor * factor * 4));
        std::free(output); std::free(expected);
        // A cached request keeps the exact original pixels and does not enqueue another job.
        assert(Bt4MenuHD_Request(source.data(), 128, 128, factor, key, nullptr) == 1);
    }
    assert(Bt4MenuHD_Request(nullptr, 0, 128, 4, 3, nullptr) == -1);
    assert(Bt4MenuHD_Request(nullptr, 128, 128, 7, 3, nullptr) == -1);
    source[0] = 0xff000000u; // GS pass identifiers must stay untouched.
    assert(Bt4MenuHD_Request(source.data(), 128, 128, 4, 5, nullptr) == 0);
    uint64_t start = SDL_GetTicks();
    int status;
    while ((status = Bt4MenuHD_Request(nullptr, 128, 128, 4, 5, nullptr)) == 0 && SDL_GetTicks() - start < 10000) SDL_Delay(1);
    assert(status == -1);
    SDL_LockMutex(artLock); assert(artBytes <= artBudget); SDL_UnlockMutex(artLock);
    std::puts("Menu artwork: async output matches xBRZ at 2x/4x; pass masks and bounds preserved.");
}
