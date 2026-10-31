#include "probe_state.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <initializer_list>

// Entry-probe evidence inputs: each captured word or bundle is size-checked, then supplied to the
// runtime service that replays it.

namespace {
// A whole evidence file whose size must be one of `sizes`. `invalid` reports a missing file or an
// unexpected size, `failed` a short read.
std::vector<std::uint8_t> readEvidence(const char* path,std::initializer_list<std::size_t> sizes,const char* invalid,const char* failed) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file || std::find(sizes.begin(),sizes.end(),static_cast<std::size_t>(file.tellg()))==sizes.end())
        throw std::runtime_error(invalid);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file.tellg()));
    file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error(failed);
    return bytes;
}
std::uint32_t readWord(const char* path,const char* invalid,const char* failed) {
    const auto bytes=readEvidence(path,{4},invalid,failed);
    std::uint32_t word;std::memcpy(&word,bytes.data(),4);return word;
}
template<std::size_t N> std::array<std::uint8_t,N> readFixed(const char* path,const char* invalid,const char* failed) {
    const auto bytes=readEvidence(path,{N},invalid,failed);
    std::array<std::uint8_t,N> fixed;std::copy(bytes.begin(),bytes.end(),fixed.begin());return fixed;
}
}

std::uint32_t loadStartupEvidence(EeRuntime& runtime,const ProbeModes& modes,char** argv) {
    const int argc=modes.argc;
    if (modes.snapshot)
        runtime.loadKernelSnapshot(readEvidence(argv[3],{EeRuntime::kernel_snapshot_size},"Invalid kernel snapshot size","Could not read kernel snapshot"));
    if (modes.timer) {
        const auto word=readWord(argv[4],"Invalid Timer 3 capture word","Timer capture read failed");
        require(word == 0x83, "Timer capture differs from the validated startup observation");
        runtime.supplyTimer3Mode(word);
        runtime.enableKernelPatch(0x80076000, 0x740);
    }
    if (modes.timer2) {
        const auto word=readWord(argv[5],"Invalid Timer 2 capture word","Timer 2 capture read failed");
        require(word == 0x80, "Timer 2 capture differs from the validated startup observation");
        runtime.supplyTimer2Mode(word);
    }
    if (modes.intc) {
        const auto word=readWord(argv[6],"Invalid INTC checkpoint word","INTC checkpoint read failed");
        require(word == 0x1002, "INTC checkpoint differs from validated startup capture");
        runtime.supplyIntcMask(word);
    }
    if (modes.readback) {
        const auto word=readWord(argv[7],"Invalid Timer 2 readback word","Timer 2 readback word read failed");
        require(word == 0x382, "Readback capture differs from validated startup observation");
        runtime.supplyTimer2Readback(word);
    }
    std::uint32_t osd_word = 0;
    if (modes.osd) {
        osd_word=readWord(argv[8],"Invalid OSD checkpoint word","OSD checkpoint read failed");
        runtime.supplyOsdConfig(osd_word);
    }
    if (modes.osd_readback) {
        runtime.supplyOsdReadback(readWord(argv[9],"Invalid OSD readback word","OSD readback read failed"));
        runtime.enableStartupKernelPayload(std::span<const std::uint8_t>(runtime.memory()).subspan(0x0036BAF8, 0x330));
    }
    for (int index = 10; index < std::min(argc, 12); ++index) {
        const auto word=readWord(argv[index],"Invalid DMA checkpoint word","DMA checkpoint read failed");
        if (index == 10) runtime.supplyDmaStatus(word);
        else runtime.supplyDma5Control(word);
    }
    if (modes.sif)
        runtime.supplySifCommand(readWord(argv[12],"Invalid SIF checkpoint word","SIF checkpoint read failed"));
    if (modes.sif_dma) {
        // The IOP receive area followed by the captured result word.
        auto bytes=readEvidence(argv[13],{EeRuntime::sif_iop_size+4},"Invalid SIF DMA checkpoint bundle","SIF DMA checkpoint read failed");
        std::uint32_t result;std::memcpy(&result,bytes.data()+EeRuntime::sif_iop_size,4);
        bytes.resize(EeRuntime::sif_iop_size);
        runtime.supplySifDma(bytes, result);
    }
    if (modes.sif_rpc)
        runtime.supplySifRpcState(readWord(argv[14],"Invalid SIF RPC checkpoint word","SIF RPC checkpoint read failed"));
    if(argc>=19)runtime.supplyHeapBind(readFixed<64>(argv[18],"Invalid IOP heap bind evidence","IOP heap evidence read failed"));
    if(argc>=20)runtime.supplyHeapAllocation(readEvidence(argv[19],{68,88},"Invalid heap allocation evidence","Heap allocation evidence read failed"));
    if(argc>=21)runtime.texture_gif.supply(readFixed<224>(argv[20],"Invalid texture GIF evidence","Texture evidence read failed"));
    if(argc>=22)runtime.supplySoundThreadStart(readFixed<56>(argv[21],"Invalid EE thread-start evidence","EE thread-start evidence read failed"));
    if(argc>=25)runtime.supplyNcmdBind(readFixed<64>(argv[24],"Invalid command bind bundle","Command bind read failed"));
    if(argc>=24)runtime.local_disc_path=argv[23];
    if(argc>=23)runtime.supplyDiscBind(readFixed<64>(argv[22],"Invalid disc bind checkpoint","Disc bind checkpoint read failed"));
    return osd_word;
}

void loadContinuationEvidence(EeRuntime& runtime,const ProbeModes& modes,char** argv) {
    if (modes.bind)
        runtime.supplySifBind(readEvidence(argv[15],{EeRuntime::sif_iop_size+68,EeRuntime::sif_iop_size+96},
            "Invalid RPC bind capture bundle","RPC bind bundle read failed"));
    if (modes.cd)
        runtime.supplyCdInit(readEvidence(argv[16],{88,128},"Invalid CD-init bundle size","Could not read CD-init bundle"));
}

void loadScmdEvidence(EeRuntime& runtime,char** argv) {
    runtime.supplyScmdBind(readEvidence(argv[17],{88,184,188,192,204,224,228,236,244,252,316,324,616,988,1124,1332,1464,1596},
        "Invalid SCMD bundle size","Invalid SCMD bind bundle size"));
}
