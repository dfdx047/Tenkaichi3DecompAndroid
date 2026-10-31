// The GPU renderer as the runtime uses it: every GIF transfer the GS command sink accepts goes to
// paraLLEl-GS, which draws every picture (docs/plans/graphics/GPU_RENDERING_PLAN.md).
#pragma once
#include "gs/gpu/gpu_gs_backend.h"
#include "gs/cpu/gs_command_sink.h"
#include "ee/frame_profile.h"
#include "host/display_frame.h"
#include <array>

class GpuGsMirror {
public:
    void attach(GsCommandSink& cpu,unsigned super_sampling=1);
    // A second renderer on `primary`'s device fed by `cpu` (the in-between pictures of 60 fps battles).
    void attachShared(GsCommandSink& cpu,GpuGsMirror& primary,unsigned super_sampling=1);
    void shareHdTextures(const GpuGsMirror& primary,GsTextureNames& names) {gpu_.shareHdTextures(primary.gpu_,names);}
    // The window shows `source`'s pictures (null: this renderer's).
    void showPictureOf(const GpuGsMirror* source) {gpu_.showPictureOf(source?&source->gpu_:nullptr);}
    void flip() {FrameProfile::Scope timed(frame_profile.flush);gpu_.flush();}
    void keepState(unsigned slot) {gpu_.keepState(slot);}
    void takeState(const GpuGsMirror& primary,unsigned slot) {gpu_.takeState(primary.gpu_,slot);}
    void submit() {gpu_.submit();} // The work so far, without ending the frame (gpu_gs_backend.h).
    void readFifo(std::span<std::uint8_t> out) {gpu_.readFifo(out);} // Local->host data, GPU-only mode.
    void enableTimestamps() {gpu_.enableTimestamps();}
    std::string timestamps() const {return gpu_.timestampSummary();} // Empty without enableTimestamps.
    void enableHdTextures(const std::filesystem::path& pack,std::uint64_t budget_bytes,GsTextureNames& names) {gpu_.enableHdTextures(pack,budget_bytes,names);}
    // The GPU image of the displayed circuits, from the privileged registers the game last wrote and
    // the video mode of its SetGsCrt request (2 NTSC, 3 PAL), which sets SMODE1 on hardware.
    DisplayFrame scanout(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field);
    // The same scanout kept on the GPU (its size; {0, 0} when nothing is displayed), for the window
    // presenter; readback() copies it to the processor.
    std::pair<unsigned,unsigned> vsync(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field);
    DisplayFrame readback() {return gpu_.readback();}
    WindowPresenter& windowPresenter() {return gpu_.windowPresenter();}
    ~GpuGsMirror(); // Prints the summary.
private:
    GpuGsBackend gpu_;
};
