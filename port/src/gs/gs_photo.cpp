// Z Warriors' photos of the model on screen: the portrait (render), the small portrait and the health bar's face are
// taken from the game's own picture (port/src/menu/port_zw.c Ph_*, plat_mods.c Port_WarriorPortraitPhoto / HudPhoto).
//
//   GsCapture_PhotoWant()      asks for the next frame's picture
//   GsCapture_PhotoCrop(...)   once it is downloaded: a part of it (fractions of the game's picture, in ten-thousandths:
//                              the game side is soft-float and hands no floats over) as ow x oh RGBA; 0 while it is not there
//
// The present path (gs_gpu.c) calls GsPhoto_Encode after the picture is in the window's texture, GsPhoto_Submitted with
// the frame's fence, and GsPhoto_Poll once a frame. No screenshot, no recording: only this.
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace {
std::mutex sMu;
int sWanted;                // 1: the next frame is wanted
int sReady;                 // 1: sPhoto holds it
std::vector<uint8_t> sPhoto;
Uint32 sPhotoW, sPhotoH;
int sPhotoBgr, sRect[4];
SDL_GPUTexture *sTex;
Uint32 sTexW, sTexH;
SDL_GPUTextureFormat sTexFmt = SDL_GPU_TEXTUREFORMAT_INVALID;
SDL_GPUTransferBuffer *sTb;
Uint32 sTbSize;
SDL_GPUFence *sFence;
int sQueued;                // 1: downloaded this frame, its fence comes with the submit
int sWaiting;               // 1: waiting on the fence
Uint32 sQW, sQH;
int sQBgr, sQRect[4];
} // namespace

extern "C" {

void GsCapture_PhotoWant(void) {
    std::lock_guard<std::mutex> g(sMu);
    sReady = 0;
    sWanted = 1;
}

int GsCapture_PhotoCrop(int ix0, int iy0, int ix1, int iy1, uint8_t *out, int ow, int oh) {
    float x0 = ix0 / 10000.0f, y0 = iy0 / 10000.0f, x1 = ix1 / 10000.0f, y1 = iy1 / 10000.0f;
    std::lock_guard<std::mutex> g(sMu);
    if (!sReady || sPhoto.empty()) {
        return 0;
    }
    if (!(x0 >= 0.0f && x1 <= 1.0f && x1 > x0 && y0 >= 0.0f && y1 <= 1.0f && y1 > y0)) {
        x0 = 0.0f, y0 = 0.0f, x1 = 1.0f, y1 = 1.0f; // (nonsense asked: the whole picture)
    }
    double px0 = sRect[0] + x0 * sRect[2], py0 = sRect[1] + y0 * sRect[3];
    double sx = (x1 - x0) * sRect[2] / ow, sy = (y1 - y0) * sRect[3] / oh;
    if (sx < 1.0 || sy < 1.0) { // bigger than the screen has it: blended between the screen's pixels (not blocks)
        for (int y = 0; y < oh; y++) {
            for (int x = 0; x < ow; x++) {
                double fx = px0 + (x + 0.5) * sx - 0.5, fy = py0 + (y + 0.5) * sy - 0.5;
                int ix = (int)(fx < 0 ? fx - 1 : fx), iy = (int)(fy < 0 ? fy - 1 : fy);
                double tx = fx - ix, ty = fy - iy;
                int x0 = SDL_clamp(ix, 0, (int)sPhotoW - 1), x1 = SDL_clamp(ix + 1, 0, (int)sPhotoW - 1);
                int y0 = SDL_clamp(iy, 0, (int)sPhotoH - 1), y1 = SDL_clamp(iy + 1, 0, (int)sPhotoH - 1);
                const uint8_t *q00 = &sPhoto[((size_t)y0 * sPhotoW + x0) * 4], *q10 = &sPhoto[((size_t)y0 * sPhotoW + x1) * 4];
                const uint8_t *q01 = &sPhoto[((size_t)y1 * sPhotoW + x0) * 4], *q11 = &sPhoto[((size_t)y1 * sPhotoW + x1) * 4];
                for (int c = 0; c < 3; c++) {
                    int k = sPhotoBgr ? 2 - c : c;
                    double v = (q00[k] * (1 - tx) + q10[k] * tx) * (1 - ty) + (q01[k] * (1 - tx) + q11[k] * tx) * ty;
                    out[(y * ow + x) * 4 + c] = (uint8_t)(v + 0.5);
                }
                out[(y * ow + x) * 4 + 3] = 255;
            }
        }
        sReady = 0;
        sPhoto.clear();
        return 1;
    }
    for (int y = 0; y < oh; y++) {
        for (int x = 0; x < ow; x++) {
            int ax = (int)(px0 + x * sx), ay = (int)(py0 + y * sy), bx = (int)(px0 + (x + 1) * sx), by = (int)(py0 + (y + 1) * sy);
            double sum[3] = {0, 0, 0};
            int n = 0;
            ax = SDL_clamp(ax, 0, (int)sPhotoW - 1);
            ay = SDL_clamp(ay, 0, (int)sPhotoH - 1);
            bx = SDL_clamp(bx, ax + 1, (int)sPhotoW);
            by = SDL_clamp(by, ay + 1, (int)sPhotoH);
            for (int v = ay; v < by; v++) {
                for (int u = ax; u < bx; u++) {
                    const uint8_t *q = &sPhoto[((size_t)v * sPhotoW + u) * 4];
                    sum[0] += q[sPhotoBgr ? 2 : 0];
                    sum[1] += q[1];
                    sum[2] += q[sPhotoBgr ? 0 : 2];
                    n++;
                }
            }
            for (int c = 0; c < 3; c++) {
                out[(y * ow + x) * 4 + c] = (uint8_t)(n > 0 ? sum[c] / n + 0.5 : 0);
            }
            out[(y * ow + x) * 4 + 3] = 255;
        }
    }
    sReady = 0;
    sPhoto.clear();
    return 1;
}

// The size in pixels of the game's picture in the photo waiting to be cropped (0 while there is none): a crop at
// that scale loses nothing.
int GsCapture_PhotoSize(int *w, int *h) {
    std::lock_guard<std::mutex> g(sMu);
    *w = sReady ? sRect[2] : 0;
    *h = sReady ? sRect[3] : 0;
    return sReady;
}

// OpenGL (gs_gl.c): a photo wanted this frame? Then the window's pixels (bottom row first, RGBA) and where the game's
// picture is in them (from the top).
int GsPhoto_GLWanted(void) {
    std::lock_guard<std::mutex> g(sMu);
    return sWanted;
}

void GsPhoto_GLGive(const uint8_t *bottomUp, int w, int h, int rx, int ry, int rw, int rh) {
    std::lock_guard<std::mutex> g(sMu);
    sPhoto.resize((size_t)w * h * 4);
    for (int y = 0; y < h; y++) {
        memcpy(&sPhoto[(size_t)y * w * 4], bottomUp + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
    }
    sPhotoW = (Uint32)w;
    sPhotoH = (Uint32)h;
    sPhotoBgr = 0;
    sRect[0] = rx;
    sRect[1] = ry;
    sRect[2] = rw > 0 ? rw : w;
    sRect[3] = rh > 0 ? rh : h;
    sWanted = 0;
    sReady = 1;
}

// After the frame's picture is drawn into the window's texture (bl: how it went there; sw x sh the window's size): when a
// photo is wanted, the picture again into a texture of our own (no settings window on it) and its download queued.
// 1 when the frame has to be submitted with a fence (GsPhoto_Submitted).
int GsPhoto_Encode(SDL_GPUDevice *dev, SDL_GPUCommandBuffer *cmd, const SDL_GPUBlitInfo *bl, Uint32 sw, Uint32 sh, SDL_GPUTextureFormat fmt) {
    sQueued = 0;
    {
        std::lock_guard<std::mutex> g(sMu);
        if (!sWanted || sWaiting || sw == 0 || sh == 0) {
            return 0;
        }
    }
    if (sTex == nullptr || sTexW != sw || sTexH != sh || sTexFmt != fmt) {
        if (sTex) {
            SDL_ReleaseGPUTexture(dev, sTex);
        }
        SDL_GPUTextureCreateInfo ci;
        SDL_zero(ci);
        ci.type = SDL_GPU_TEXTURETYPE_2D;
        ci.format = fmt;
        ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        ci.width = sw;
        ci.height = sh;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        sTex = SDL_CreateGPUTexture(dev, &ci);
        sTexW = sw;
        sTexH = sh;
        sTexFmt = fmt;
    }
    Uint32 size = sw * sh * 4;
    if (sTb == nullptr || sTbSize < size) {
        if (sTb) {
            SDL_ReleaseGPUTransferBuffer(dev, sTb);
        }
        SDL_GPUTransferBufferCreateInfo ti;
        SDL_zero(ti);
        ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        ti.size = size;
        sTb = SDL_CreateGPUTransferBuffer(dev, &ti);
        sTbSize = sTb != nullptr ? size : 0;
    }
    if (sTex == nullptr || sTb == nullptr) {
        return 0;
    }
    SDL_GPUBlitInfo b = *bl;
    b.destination.texture = sTex;
    SDL_BlitGPUTexture(cmd, &b);
    SDL_GPUTextureRegion r;
    SDL_zero(r);
    r.texture = sTex;
    r.w = sw;
    r.h = sh;
    r.d = 1;
    SDL_GPUTextureTransferInfo t;
    SDL_zero(t);
    t.transfer_buffer = sTb;
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(cp, &r, &t);
    SDL_EndGPUCopyPass(cp);
    sQW = sw;
    sQH = sh;
    sQBgr = fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    sQRect[0] = (int)bl->destination.x;
    sQRect[1] = (int)bl->destination.y;
    sQRect[2] = (int)bl->destination.w > 0 ? (int)bl->destination.w : (int)sw;
    sQRect[3] = (int)bl->destination.h > 0 ? (int)bl->destination.h : (int)sh;
    sQueued = 1;
    {
        std::lock_guard<std::mutex> g(sMu);
        sWanted = 0;
    }
    return 1;
}

// The fence of the frame that queued the photo.
void GsPhoto_Submitted(SDL_GPUFence *fence) {
    if (sQueued && fence != nullptr) {
        sFence = fence;
        sWaiting = 1;
    }
    sQueued = 0;
}

// Once a frame: the photo, when the card has finished it, goes to GsCapture_PhotoCrop.
void GsPhoto_Poll(SDL_GPUDevice *dev) {
    if (!sWaiting || sFence == nullptr || !SDL_QueryGPUFence(dev, sFence)) {
        return;
    }
    SDL_ReleaseGPUFence(dev, sFence);
    sFence = nullptr;
    sWaiting = 0;
    const uint8_t *px = (const uint8_t *)SDL_MapGPUTransferBuffer(dev, sTb, false);
    if (px == nullptr) {
        return;
    }
    std::vector<uint8_t> copy(px, px + (size_t)sQW * sQH * 4);
    SDL_UnmapGPUTransferBuffer(dev, sTb);
    std::lock_guard<std::mutex> g(sMu);
    sPhoto = std::move(copy);
    sPhotoW = sQW;
    sPhotoH = sQH;
    sPhotoBgr = sQBgr;
    memcpy(sRect, sQRect, sizeof(sRect));
    sReady = 1;
}

} // extern "C"
