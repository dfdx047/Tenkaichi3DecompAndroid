#pragma once
#include "ee/ee_runtime.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

// Shared state of the entry probe (entry_probe.cpp runs the stages below in order): the replayed
// checkpoint selected by the number of evidence arguments, and the boundary where the run stopped.

// Each optional evidence argument enables one more replayed startup checkpoint (see the usage text).
struct ProbeModes {
    int argc=0;
    bool snapshot,timer,timer2,intc,readback,osd,osd_readback,dma_status,dma5,sif,sif_dma,sif_rpc,bind,cd;
    static ProbeModes fromArgc(int argc) {
        return {argc,argc>=4,argc>=5,argc>=6,argc>=7,argc>=8,argc>=9,argc>=10,argc>=11,argc>=12,argc>=13,argc>=14,argc>=15,argc>=16,argc>=17};
    }
};

// The checked boundary where the generated entry stopped.
struct ProbeStop {
    bool stopped=false,execution_budget_exhausted=false;
    std::uint32_t address=0;
    std::size_t size=0;
    std::uint32_t selector=0,handler=0,target=0,source=0,resume=0;
};

// The observed startup zero region, poisoned before the run so zero stores are observable.
inline constexpr std::uint32_t startup_zero_start=0x00387E80,startup_zero_end=0x0043058F;

inline void require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}

// Evidence inputs (probe_inputs.cpp): captured words and bundles replayed into the runtime, in the
// order the startup reaches them. loadStartupEvidence returns the OSD configuration word.
std::uint32_t loadStartupEvidence(EeRuntime& runtime,const ProbeModes& modes,char** argv);
void loadContinuationEvidence(EeRuntime& runtime,const ProbeModes& modes,char** argv);
void loadScmdEvidence(EeRuntime& runtime,char** argv);

// Generated-code bindings (startup_bindings.cpp).
void registerStartupFunctions(EeRuntime& runtime);
void registerKernelPayloads(EeRuntime& runtime,const ProbeModes& modes);
void registerSelectedStartup(EeRuntime& runtime);  // selected_bindings.cpp
void registerSelectedOverlays(EeRuntime& runtime); // overlay_bindings.cpp

// Checkpoint validation (checkpoint_checks.cpp): each throws if the reached state differs.
void checkCdContinuation(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop);
void checkRuntimeBoundary(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop);
void checkBindContinuation(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop,const std::vector<std::uint8_t>& kernel_before);
void checkEntryBoundary(EeRuntime& runtime,R5900Context& context,const ProbeModes& modes,const ProbeStop& stop,
    const std::vector<std::uint8_t>& kernel_before,const std::vector<std::uint8_t>& image,const std::array<std::uint32_t,5>& args);

// Reports (probe_reports.cpp): the private JSON stop report at argv[2] and its evidence sidecars.
void writeCdReport(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop,char** argv);
void writeRuntimeBoundaryReport(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop,char** argv);
void writeEvidenceDumps(EeRuntime& runtime,char** argv);
void writeBindReport(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop,char** argv);
void writeEntryReport(EeRuntime& runtime,R5900Context& context,const ProbeModes& modes,const ProbeStop& stop,
    const std::array<std::uint32_t,5>& args,char** argv);
void printEntrySummary(R5900Context& context,const ProbeModes& modes,const ProbeStop& stop);
