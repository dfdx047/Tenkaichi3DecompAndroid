#pragma once
#include "ipu_input.h"
#include "ipu_block.h"
#include "ipu_csc.h"
#include <deque>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

// IPU (MPEG decoder) register model (docs/plans/video/IPU_PLAN.md): the public register layout
// (IPU_CMD 0x10002000, IPU_CTRL 0x10002010, IPU_BP 0x10002020, IPU_TOP 0x10002030, IN_FIFO 0x10007010),
// the setup commands (BCLR, SETIQ, SETVQ, SETTH), FDEC, VDEC (ipu_vlc.h), and the toIPU DMA channel (4) feeding the
// input on demand (ipu_input.h). A command waiting for input stays pending with BUSY set and completes
// when data arrives. BDEC (ipu_block.h) decodes one macroblock into the output FIFO, which fromIPU DMA
// (channel 3, normal mode) drains to main RAM or the scratchpad. CSC (ipu_csc.h) converts RAW8 macroblocks
// from the input to RGB32. IDEC, PACK and RGB16 output are refused by name.
class IpuDevice {
public:
    std::array<std::uint8_t,64> intra_iq{},nonintra_iq{}; // SETIQ tables (zigzag order as written).
    std::array<std::uint16_t,16> vq_clut{};               // SETVQ colour table.
    std::uint32_t thresholds=0;                            // SETTH TH0 (bits 0-8), TH1 (bits 16-24).
    bool iq_loaded[2]{},vq_loaded=false;
    std::uint32_t decode_parameters=0; // IPU_CTRL IDP (16-17), AS (20), IVF (21), QST (22), MP1 (23), PCT (24-26).
    static constexpr std::uint32_t decode_parameter_mask=0x07F30000u;
    unsigned resets=0,commands=0,vlc_decodes=0,macroblocks=0,output_qwords=0,converted_macroblocks=0;
    struct Channel {std::uint32_t chcr=0,madr=0,qwc=0,tadr=0;};
    Channel from_ipu;  // DMA channel 3 (IPU_FROM).
    IpuInput input;    // Input FIFO, bit pointer and DMA channel 4 (IPU_TO).
    std::span<const std::uint8_t> ram; // Main RAM for toIPU transfers (set by the runtime).
    std::span<std::uint8_t> output_ram,output_scratchpad; // fromIPU destinations (set by the runtime).

    // Register access; nullopt/false means the address is not an IPU or IPU DMA register.
    std::optional<std::uint32_t> read32(std::uint32_t address);
    bool write32(std::uint32_t address,std::uint32_t value);
    bool writeFifo(std::uint32_t address,const std::array<std::uint32_t,4>& qword);
    bool busy() const {return pending_!=0;}

private:
    std::uint32_t command_result_=0,pending_=0; // pending_: command word waiting for input (0: none).
    std::uint32_t status_=0;                    // IPU_CTRL ECD (bit 14) and SCD (bit 15).
    void reset();
    void command(std::uint32_t value);
    bool execute(std::uint32_t value); // False when the command needs more input.
    void retry() {if(pending_ && execute(pending_)){pending_=0;++commands;}}
    // BDEC state: decoded quadwords not yet taken by fromIPU (the FIFO holds 8; the command stays busy
    // while more wait), the DC predictors, and whether the pending command already decoded its macroblock.
    std::deque<std::array<std::uint8_t,16>> output_;
    std::array<int,3> dc_predictors_{};
    bool decoded_=false;
    bool decodeMacroblock(std::uint32_t value);
    void drainOutput();
    bool findStartCode();
    // CSC state: macroblocks left, and the RAW8 bytes gathered for the current one.
    bool converting_=false;
    unsigned convert_remaining_=0,convert_fill_=0;
    std::array<std::uint8_t,ipu_raw8_bytes> convert_block_{};
    bool convert(std::uint32_t value);
};
