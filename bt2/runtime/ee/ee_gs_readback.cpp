#include "ee/frame_profile.h"
#include "ee/ee_runtime.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

// GS readback in the frame phase (docs/plans/graphics/GS_READBACK_PLAN.md): the library's store-image
// sequence. A GIF packet sent through VIF1 in normal DMA mode sets up a local->host transfer and
// writes FINISH; the EE waits for the FINISH event in CSR, turns the VIF1 FIFO (VIF1_STAT.FDR) and
// the GS bus (BUSDIR) around, and takes the pixels by a VIF1 DMA to memory and by FIFO reads.
// Everything completes synchronously, so the status reads see each step already done.

namespace {
std::vector<std::uint32_t> wordsAt(std::span<const std::uint8_t> memory,std::uint32_t address,std::size_t qwords) {
    std::vector<std::uint32_t> words(qwords*4);
    std::memcpy(words.data(),memory.data()+address,qwords*16);
    return words;
}
}

void EeRuntime::frameVif1Codes(std::span<const std::uint32_t> words) {
    // VIF codes outside a display list (normal-mode DMA, FIFO writes): checked as a whole, then run.
    const Vu1::Kick to_gs=[this](std::span<const std::uint8_t> data) {FrameProfile::Scope timed(frame_profile.gs),of_vu1(frame_profile.vu1_gs);gif_dma.gs.transfer(data);};
    std::vector<std::uint32_t> copy;
    if(on_vif1_transfer) {copy.assign(words.begin(),words.end());on_vif1_transfer(copy);words=copy;}
    try {FrameProfile::Scope timed(frame_profile.vif1);vif1.check(words);vif1_running=true;struct Done {bool& flag;~Done(){flag=false;}} done{vif1_running};vif1.run(words,to_gs,to_gs);}
    catch(const std::runtime_error& e) {std::cerr<<"VIF1 transfer stopped: "<<e.what()<<'\n';throw UnsupportedGuestMemory(0x10009000,4);}
}

void EeRuntime::frameVif1Send(std::uint32_t value) {
    // Normal mode, from memory (CHCR 0x101): QWC quadwords at MADR are VIF codes and their data.
    auto& v=frame_vif1;
    if(!v.qwc || gs_readback.fdr || v.madr>=ram_size || std::size_t(v.qwc)*16>ram_size-v.madr)throw UnsupportedGuestMemory(0x10009000,4);
    frameVif1Codes(wordsAt(std::span<const std::uint8_t>(memory_.data(),ram_size),v.madr,v.qwc));
    v.madr+=v.qwc*16;v.qwc=0;v.chcr=value&~0x100u;v.cis=true;++v.sends;
}

void EeRuntime::frameVif1Receive(std::uint32_t value) {
    // Normal mode, to memory (CHCR 0x100): QWC quadwords of the local->host transfer go to MADR.
    auto& v=frame_vif1;
    if(!v.qwc || !gs_readback.fdr || !gs_readback.busdir || v.qwc>gif_dma.gs.downloadQwords()
        || v.madr>=ram_size || std::size_t(v.qwc)*16>ram_size-v.madr)throw UnsupportedGuestMemory(0x10009000,4);
    gsDownload(std::span<std::uint8_t>(memory_.data()+v.madr,std::size_t(v.qwc)*16));
    gs_readback.dma_qwords+=v.qwc;
    v.madr+=v.qwc*16;v.qwc=0;v.chcr=value&~0x100u;v.cis=true;
}

void EeRuntime::gsDownload(std::span<std::uint8_t> out) {
    // The CPU model advances the transfer (and supplies the pixels when it draws); in GPU-only mode
    // the renderer holds VRAM and supplies them.
    try {gif_dma.gs.readDownload(out);if(gs_download_source)gs_download_source(out);}
    catch(const std::runtime_error& e) {std::cerr<<"GS readback stopped: "<<e.what()<<'\n';throw UnsupportedGuestMemory(0x10005000,16);}
}

bool EeRuntime::gsReadbackStore32(std::uint32_t address,std::uint32_t value) {
    if(address!=0x10003C00)return false;
    // VIF1_STAT is read-only except FDR (bit 23), the FIFO direction.
    if(value&~0x800000u)throw UnsupportedGuestMemory(address,4);
    gs_readback.fdr=value!=0;
    return true;
}

bool EeRuntime::gsReadbackStore64(std::uint32_t address,std::uint64_t value) {
    if(address==0x12001000 && value==2) {gs_readback.finishes_cleared=gif_dma.gs.state.finishes;return true;} // CSR: FINISH is write-one-to-clear.
    if(address!=0x12001040)return false;
    // BUSDIR: 1 while the GS drives the bus for a local->host transfer.
    if(value>1 || (value && !gif_dma.gs.state.download))throw UnsupportedGuestMemory(address,8);
    if(value && !gs_readback.busdir)++gs_readback.downloads;
    gs_readback.busdir=value!=0;
    return true;
}

std::uint32_t EeRuntime::vif1Status() const {
    // Idle VIF1 as captured at the startup path sync (0x40), DBF, FDR, and the quadwords the FIFO
    // holds (FQC, at most 16) while a local->host transfer has data left.
    const auto left=gs_readback.fdr && gs_readback.busdir?gif_dma.gs.downloadQwords():0;
    return 0x40u|(vif1.r.dbf?0x80u:0u)|(gs_readback.fdr?0x800000u:0u)|(std::uint32_t(std::min<std::size_t>(left,16))<<24);
}

ProbeGpr EeRuntime::vif1FifoRead() {
    if(!gs_display_flips || !gs_readback.fdr || !gs_readback.busdir || !gif_dma.gs.downloadQwords())throw UnsupportedGuestMemory(0x10005000,16);
    ProbeGpr value;std::array<std::uint8_t,16> bytes;
    gsDownload(bytes);std::memcpy(&value,bytes.data(),16);++gs_readback.fifo_qwords;
    return value;
}

void EeRuntime::vif1FifoWrite(const ProbeGpr& value) {
    if(gs_readback.fdr)throw UnsupportedGuestMemory(0x10005000,16);
    std::array<std::uint32_t,4> words;std::memcpy(words.data(),&value,16);
    frameVif1Codes(words);
}
