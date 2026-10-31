// GS rendering on the GPU through paraLLEl-GS (docs/plans/graphics/GPU_RENDERING_PLAN.md).
// Built only with BT2_GPU_GS; Vulkan and Granite stay behind this interface.
#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>
#include <atomic>
#include <string>
#include <utility>

struct DisplayFrame;
class GsTextureNames;
class WindowPresenter;

class GpuGsBackend {
public:
    GpuGsBackend();
    ~GpuGsBackend();
    // The Vulkan loader's vkGetInstanceProcAddr to use instead of the system's (Android: a custom driver such
    // as Turnip loaded through libadrenotools, runtime/android/android_main.cpp); null for the system's.
    static void setVulkanLoader(void* get_instance_proc_addr);
    // Creates the Vulkan device and the GS; throws a named error when no usable device exists, naming
    // what the GPU's driver lacks when it does not meet paraLLEl-GS's requirements. BT2_GS_HACKS (a comma
    // list of blend_demote, unsynced_readbacks, backbuffer_promotion, no_mipmaps) turns on paraLLEl-GS's
    // speed-for-accuracy options (gs_interface.hpp, Hacks) for weak GPUs.
    // `super_sampling` (1, 2, 4, 8 or 16 samples per pixel): with 4 or more the scanout is at twice the
    // resolution, built from the samples (docs/plans/graphics/IMAGE_QUALITY_PLAN.md).
    void init(unsigned super_sampling=1);
    // A second renderer on `primary`'s device (the in-between pictures of 60 fps battles), with its own
    // GS state; its pictures can be shown in `primary`'s window (showPictureOf).
    void initShared(GpuGsBackend& primary,unsigned super_sampling=1);
    // The window shows `source`'s last scanout (null or this: its own).
    void showPictureOf(const GpuGsBackend* source);
    // GIF data in 128-bit units, as the GIF receives it on PATH3.
    void transfer(std::span<const std::uint8_t> data);
    // Completes queued work and returns GS memory (4 MB, GS address order) for comparison.
    std::span<const std::uint32_t> readVram();
    // The renderer's state on the GPU (GS memory with its super-samples, and the CLUT) kept in one of two
    // slots after a flush (keepState), and a second renderer on the same device set to a kept state
    // (takeState; its cached textures are dropped): the in-between pictures of 60 fps battles start from
    // the main renderer's state. Copies on the GPU in submission order; neither waits for the GPU
    // (tools/graphics/gpu/parallel_gs_vram_backup.patch).
    void keepState(unsigned slot);
    void takeState(const GpuGsBackend& primary,unsigned slot);
    // The next quadwords of the local->host transfer the last TRXDIR started (zero past its end).
    void readFifo(std::span<std::uint8_t> out);
    // Submits queued work at a frame boundary (each flip) and advances the frame context.
    void flush();
    // Submits the work taken so far without ending the frame. Between flips the renderer holds a command
    // buffer, and the device's next frame context (a readback, the window presenter) waits for it.
    void submit();
    // Called once on the thread that drives a second renderer (initShared): its own command pools.
    static void registerWorkerThread();
    // Privileged register by its index in the 0x12000000 block (address offset / 16).
    void privileged(unsigned index,std::uint64_t value);
    // Scans out the displayed circuits for one field and reads the image back (empty when nothing is displayed).
    DisplayFrame scanout(unsigned field);
    // Scans out the displayed circuits for one field and keeps the image: its size, {0, 0} when nothing
    // is displayed. readback() copies the kept image to the processor; the window presenter shows it.
    std::pair<unsigned,unsigned> vsync(unsigned field);
    DisplayFrame readback();
    // Shows pictures in a window on this device (host/window_presenter.h); made at the first call.
    WindowPresenter& windowPresenter();
    // HD textures (docs/plans/graphics/HD_TEXTURES_PLAN.md): each texture the renderer caches is named by
    // `names` (kept in step with the renderer by the GS sink) and drawn with the pack's replacement when
    // it has one. The summary line says what was found, loaded and dropped.
    void enableHdTextures(const std::filesystem::path& pack,std::uint64_t budget_bytes,GsTextureNames& names);
    std::string hdTextureSummary() const;
    // The second renderer draws with `primary`'s pack (one copy in video memory), named by its own `names`.
    void shareHdTextures(const GpuGsBackend& primary,GsTextureNames& names);
    // GPU time by the renderer's stages (paraLLEl-GS timestamps), switched on before the first transfer;
    // the summary gives milliseconds per frame (per flush).
    void enableTimestamps();
    // paraLLEl-GS's speed-for-accuracy options (BT2_GS_HACKS's list) for every renderer from its next frame:
    // Android's in-game menu changes them while the game runs. Any thread.
    static void requestHacks(const std::string& list);
    // Set by the run before each flip: the frame that ends took longer than its fields on the display (frame
    // skipping, "frame_skip", then does not draw the next one).
    inline static std::atomic<bool> frame_late{false};
    std::string timestampSummary() const;
private:
    void copyDrawnPalettes(); // After a flush: the drawn palettes back to the texture names.
    void replaceTextures(GsTextureNames& names);
    struct State;
    std::unique_ptr<State> state_;
};
