#include "ipu_device.h"
#include "ipu_vlc.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

// IPU register and command model (see ipu_device.h). Register semantics follow the public IPU
// documentation; commands complete synchronously once their input is available.

namespace {
constexpr std::uint32_t ipu_cmd=0x10002000,ipu_ctrl=0x10002010,ipu_bp=0x10002020,ipu_top=0x10002030,ipu_in_fifo=0x10007010;
constexpr std::uint32_t from_base=0x1000B000,to_base=0x1000B400;
std::string hex(std::uint32_t value) {char text[12];std::snprintf(text,sizeof text,"0x%X",value);return text;}
}

std::optional<std::uint32_t> IpuDevice::read32(std::uint32_t address) {
    switch(address) {
    case ipu_cmd: return command_result_;
    case ipu_cmd+4: return busy()?0x80000000u:0u;              // IPU_CMD BUSY (bit 63).
    // IFC (bits 0-3) is the input FIFO quadword count; BUSY is bit 31.
    case ipu_ctrl: return decode_parameters|status_|input.fifoQwords()|(std::uint32_t(output_.size()<8?output_.size():8)<<4)|(busy()?0x80000000u:0u);
    case ipu_bp: return input.bpRegister();
    case ipu_ctrl+4: case ipu_bp+4: return 0u;                    // Upper words of IPU_CTRL/IPU_BP.
    case ipu_top: return input.available(32)?input.peek(32):0u;  // BSTOP: the next 32 bits.
    case ipu_top+4: return input.available(32)?0u:0x80000000u;  // IPU_TOP BUSY until 32 bits are present.
    }
    if(address==from_base)return from_ipu.chcr;
    if(address==from_base+0x10)return from_ipu.madr;
    if(address==from_base+0x20)return from_ipu.qwc;
    if(address==to_base)return input.channel.chcr;
    if(address==to_base+0x10)return input.channel.madr;
    if(address==to_base+0x20)return input.channel.qwc;
    if(address==to_base+0x30)return input.channel.tadr;
    return std::nullopt;
}

bool IpuDevice::write32(std::uint32_t address,std::uint32_t value) {
    if(address==ipu_ctrl) {
        if(value&0x40000000u) {reset();return true;} // RST.
        // Decode parameters (bits 16-26): IDP, AS, IVF, QST, MP1, PCT. Retained for decode commands. The
        // other fields (FIFO counts, CBP, ECD/SCD, BUSY) are read-only: the game writes back what it read.
        decode_parameters=value&decode_parameter_mask;return true;
    }
    if(address==ipu_cmd) {command(value);return true;}
    if(address==from_base) {
        // Normal mode to memory only (DIR 0, MOD 0, no tag transfer or interrupt bits).
        if((value&0x100u) && ((value&0xFFu) || (from_ipu.chcr&0x100u) || !from_ipu.qwc))
            throw std::runtime_error("Unsupported fromIPU DMA start (CHCR "+hex(value)+", MADR "+hex(from_ipu.madr)+", QWC "+hex(from_ipu.qwc)+")");
        from_ipu.chcr=value;
        if(value&0x100u) {drainOutput();retry();}
        return true;
    }
    if(address==from_base+0x10) {from_ipu.madr=value;return true;}
    if(address==from_base+0x20) {from_ipu.qwc=value&0xFFFF;return true;}
    auto& to=input.channel;
    if(address==to_base) {
        // Clearing STR stops a running transfer; MADR/QWC/TADR and the FIFO contents are kept, and a later
        // start continues from them (a chain start with QWC > 0 first finishes the current tag's data).
        if((to.chcr&0x100u) && (value&0x100u))throw std::runtime_error("toIPU CHCR start "+hex(value)+" while the transfer runs");
        if(value&0x100u) {input.start(value,ram);retry();} else to.chcr=(to.chcr&0xFFFF0000u)|(value&0xFFFFu);
        return true;
    }
    if(to.chcr&0x100u && (address==to_base+0x10 || address==to_base+0x20 || address==to_base+0x30))
        throw std::runtime_error("toIPU register write while the transfer runs");
    if(address==to_base+0x10) {to.madr=value;return true;}
    if(address==to_base+0x20) {to.qwc=value&0xFFFF;return true;}
    if(address==to_base+0x30) {to.tadr=value;return true;}
    return false;
}

bool IpuDevice::writeFifo(std::uint32_t address,const std::array<std::uint32_t,4>& qword) {
    if(address!=ipu_in_fifo)return false;
    std::array<std::uint8_t,16> bytes;std::memcpy(bytes.data(),qword.data(),16);
    input.push(bytes);retry();return true;
}

void IpuDevice::reset() {
    input.clear(0);command_result_=0;pending_=0;decode_parameters=0;status_=0;output_.clear();decoded_=false;converting_=false;++resets;
}

void IpuDevice::command(std::uint32_t value) {
    if(busy())throw std::runtime_error("IPU command "+hex(value)+" while "+hex(pending_)+" is pending");
    status_=0; // Every command clears ECD and SCD.
    if(execute(value))++commands; else pending_=value;
}

bool IpuDevice::execute(std::uint32_t value) {
    const unsigned code=value>>28;
    switch(code) {
    case 0x0: input.clear(value&0x7F);break; // BCLR: clear the input FIFO, set the bit pointer.
    case 0x4: { // FDEC: skip FB bits, then IPU_CMD holds the next 32 bits (not consumed).
        const unsigned fb=value&0x3F;
        if(!input.available(fb+32))return false;
        input.skip(fb);command_result_=input.peek(32);return true;
    }
    case 0x2: { // BDEC: one macroblock to the output FIFO, then the search for a following start code.
        if(!decoded_) {if(!decodeMacroblock(value))return false;decoded_=true;}
        drainOutput();
        if(output_.size()>8)return false; // More than the FIFO holds: wait for fromIPU.
        if(!findStartCode())return false;
        decoded_=false;++macroblocks;break;
    }
    case 0x3: { // VDEC: skip FB bits, decode one header code (TBL), consume it.
        const unsigned fb=value&0x3F;
        if(!input.available(fb+32))return false;
        input.skip(fb);
        const std::uint32_t bits=input.peek(16);
        IpuVlc code;
        switch((value>>26)&3) {
        case 0: code=ipuAddressIncrement(bits,decode_parameters&0x00800000u);break;
        case 1: {const unsigned pct=(decode_parameters>>24)&7;code=ipuMacroblockType(bits,pct?pct:1);break;}
        case 2: code=ipuMotionCode(bits);break;
        case 3: code=ipuDmVector(bits);break;
        }
        input.skip(code.consumed);command_result_=code.result;
        if(!code.result)status_|=0x4000u; // ECD: no valid code.
        ++vlc_decodes;return true;
    }
    case 0x5: { // SETIQ: skip FB bits; bit 27 selects the non-intra table; 64 bytes at the bit pointer.
        const bool nonintra=(value>>27)&1;const unsigned fb=value&0x3F;
        if(!input.available(fb+512))return false;
        input.skip(fb);
        if(!input.takeBytes((nonintra?nonintra_iq:intra_iq).data(),64))return false;
        iq_loaded[nonintra]=true;break;
    }
    case 0x6: {std::array<std::uint8_t,32> bytes{};if(!input.takeBytes(bytes.data(),32))return false;std::memcpy(vq_clut.data(),bytes.data(),32);vq_loaded=true;break;} // SETVQ.
    case 0x7: if(!convert(value))return false;break; // CSC.
    case 0x9: thresholds=value&0x01FF01FFu;break; // SETTH.
    default: {
        static const char* names[16]={"BCLR","IDEC","BDEC","VDEC","FDEC","SETIQ","SETVQ","CSC","PACK","SETTH","?","?","?","?","?","?"};
        throw std::runtime_error(std::string("IPU decode command is not modelled: ")+names[code]+" "+hex(value));
    }
    }
    command_result_=0;return true;
}

bool IpuDevice::decodeMacroblock(std::uint32_t value) {
    const unsigned fb=value&0x3F;
    if(!input.available(fb+1))return false; // Nothing to decode yet.
    if(!iq_loaded[0] || !iq_loaded[1])throw std::runtime_error("IPU BDEC before SETIQ");
    input.skip(fb);
    IpuBlockParameters p;
    p.intra=(value>>27)&1;p.field_dct=(value>>25)&1;
    p.mpeg1=decode_parameters&0x00800000u;p.alternate_scan=decode_parameters&0x00100000u;
    p.intra_table_one=(decode_parameters&0x00200000u) && !p.mpeg1;
    p.dc_precision=(decode_parameters>>16)&3;
    p.quantiser_scale=ipuQuantiserScale((value>>16)&0x1F,decode_parameters&0x00400000u);
    p.intra_matrix=intra_iq.data();p.nonintra_matrix=nonintra_iq.data();
    if((value>>26)&1)dc_predictors_.fill(128<<p.dc_precision); // DCR.
    IpuMacroblock block;
    try {ipuDecodeMacroblock(input,p,dc_predictors_,block);}
    catch(const IpuInputDry&) {throw std::runtime_error("IPU BDEC ran out of input inside a macroblock (resumable decoding is not modelled)");}
    for(unsigned q=0;q<48;++q) {
        std::array<std::uint8_t,16> qword;std::memcpy(qword.data(),block.data()+q*8,16);
        output_.push_back(qword);
    }
    return true;
}

void IpuDevice::drainOutput() {
    while((from_ipu.chcr&0x100u) && !output_.empty()) {
        // MADR bit 31 selects the scratchpad.
        const bool spr=from_ipu.madr&0x80000000u;
        const auto target=spr?output_scratchpad:output_ram;
        const std::uint32_t offset=spr?(from_ipu.madr&0x3FF0u):(from_ipu.madr&0x7FFFFFFFu);
        if(offset%16 || offset+16>target.size())throw std::runtime_error("fromIPU DMA destination outside memory: "+hex(from_ipu.madr));
        std::memcpy(target.data()+offset,output_.front().data(),16);
        output_.pop_front();from_ipu.madr+=16;++output_qwords;
        if(!--from_ipu.qwc)from_ipu.chcr&=~0x100u;
    }
}

// After a macroblock: when the next byte is zero, align and skip zero bytes up to a start code prefix
// (SCD) or other data (ECD). False while the input cannot decide.
bool IpuDevice::findStartCode() {
    if(!input.available(8))return false;
    if(input.peek(8))return true;
    const unsigned misaligned=input.bpRegister()&7;
    if(misaligned) {if(!input.available(8-misaligned))return false;input.skip(8-misaligned);}
    for(;;) {
        if(!input.available(24))return false;
        const auto next=input.peek(24);
        if(next) {status_|=next==1?0x8000u:0x4000u;return true;}
        input.skip(8);
    }
}

// CSC: MBC (bits 0-10) RAW8 macroblocks from the input, each converted to 64 quadwords of RGB32. A
// macroblock is converted once the output FIFO has room; the command ends when the last one fits.
bool IpuDevice::convert(std::uint32_t value) {
    if(!converting_) {
        if(value&0x08000000u)throw std::runtime_error("IPU CSC to RGB16 (OFM) is not modelled: "+hex(value));
        converting_=true;convert_remaining_=value&0x7FF;convert_fill_=0;
    }
    for(;;) {
        drainOutput();
        if(output_.size()>8)return false;
        if(!convert_remaining_)break;
        while(convert_fill_<ipu_raw8_bytes) {
            if(!input.available(8))return false;
            convert_block_[convert_fill_++]=std::uint8_t(input.peek(8));input.skip(8);
        }
        std::array<std::uint8_t,ipu_rgb32_bytes> pixels;
        ipuConvertMacroblock(convert_block_.data(),thresholds,pixels.data());
        for(unsigned q=0;q<ipu_rgb32_bytes/16;++q) {
            std::array<std::uint8_t,16> qword;std::memcpy(qword.data(),pixels.data()+q*16,16);
            output_.push_back(qword);
        }
        convert_fill_=0;--convert_remaining_;++converted_macroblocks;
    }
    converting_=false;return true;
}
