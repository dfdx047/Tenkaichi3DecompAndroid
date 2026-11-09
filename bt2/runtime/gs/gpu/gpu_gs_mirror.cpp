#include "gs/gpu/gpu_gs_mirror.h"
#include <iostream>

void GpuGsMirror::attach(GsCommandSink& cpu,unsigned super_sampling) {
    gpu_.init(super_sampling);
    cpu.on_accepted=[this](std::span<const std::uint8_t> data){FrameProfile::Scope timed(frame_profile.renderer);gpu_.transfer(data);};
}

void GpuGsMirror::attachShared(GsCommandSink& cpu,GpuGsMirror& primary,unsigned super_sampling) {
    gpu_.initShared(primary.gpu_,super_sampling);
    cpu.on_accepted=[this](std::span<const std::uint8_t> data){gpu_.transfer(data);};
}

DisplayFrame GpuGsMirror::scanout(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field) {
    vsync(privileged,crt_mode,field);
    return gpu_.readback();
}

std::pair<unsigned,unsigned> GpuGsMirror::vsync(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field) {
    if(crt_mode!=2 && crt_mode!=3)throw std::runtime_error("GPU GS: unsupported SetGsCrt video mode");
    for(unsigned n=0;n<privileged.size();++n)gpu_.privileged(n,privileged[n]);
    gpu_.privileged(1,(32u<<3)|(crt_mode<<13)); // SMODE1: analog line clock (LC 32), CMOD NTSC/PAL.
    return gpu_.vsync(field);
}

GpuGsMirror::~GpuGsMirror() {
    if(const auto hd=gpu_.hdTextureSummary();!hd.empty())std::cerr<<hd<<std::endl;
    if(const auto times=gpu_.timestampSummary();!times.empty())std::cerr<<times<<std::endl;
}
