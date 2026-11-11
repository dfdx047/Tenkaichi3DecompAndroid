#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cassert>
extern "C" uint32_t *Bt4MenuHD_Upscale(const uint32_t *, int, int, int);
int main() {
    int checks = 0;
    for (uint32_t c : {0x800000FFu, 0x80FF0000u, 0x4000FF00u, 0x7F123456u, 0u}) {
        std::vector<uint32_t> source(32 * 24, c);
        for (int factor = 2; factor <= 6; ++factor) {
            auto out = Bt4MenuHD_Upscale(source.data(), 32, 24, factor);
            assert(out);
            /* xBRZ intentionally feathers image borders into transparency. Check the interior. */
            for (int y = 4 * factor; y < 20 * factor; ++y) {
                for (int x = 4 * factor; x < 28 * factor; ++x) { assert(out[y * 32 * factor + x] == c); }
            }
            std::free(out); ++checks;
        }
    }
    std::vector<uint32_t> art(32 * 32);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) { art[y * 32 + x] = x > y ? 0x800000FFu : 0x80FF0000u; }
    }
    auto out = Bt4MenuHD_Upscale(art.data(), 32, 32, 4);
    assert(out);
    int blended = 0;
    for (int i = 0; i < 128 * 128; ++i) {
        assert((out[i] >> 24) <= 128);
        if ((out[i] & 255) > 0 && ((out[i] >> 16) & 255) > 0) { ++blended; }
    }
    assert(blended > 0);
    std::free(out); ++checks;
    art[0] = 0xF9000000u;
    assert(Bt4MenuHD_Upscale(art.data(), 32, 32, 4) == nullptr); ++checks;
    assert(Bt4MenuHD_Upscale(nullptr, 32, 32, 4) == nullptr); ++checks;
    assert(Bt4MenuHD_Upscale(art.data(), 32, 32, 7) == nullptr); ++checks;
    printf("{\"passed\":true,\"checks\":%d,\"edge_blended_pixels\":%d}\n", checks, blended);
}
