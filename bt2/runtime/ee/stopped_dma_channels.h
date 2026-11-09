#pragma once
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

// EE DMA channels accepted only while stopped: their registers (CHCR, MADR, QWC, TADR and the
// scratchpad address SADR at +0x80) are retained, and setting STR (CHCR bit 8) is refused because no
// transfer is modelled. Used for the scratchpad channels 8 (fromSPR) and 9 (toSPR).
class StoppedDmaChannels {
public:
    struct Channel {std::uint32_t chcr=0,madr=0,qwc=0,tadr=0,sadr=0;};
    std::map<std::uint32_t,Channel> channels;
    StoppedDmaChannels(std::initializer_list<std::uint32_t> bases) {for(const auto base:bases)channels[base];}

    std::optional<std::uint32_t> read32(std::uint32_t address) const {
        const auto* field=find(address);
        if(!field)return std::nullopt;
        return *field;
    }
    bool write32(std::uint32_t address,std::uint32_t value) {
        auto* field=find(address);
        if(!field)return false;
        if((address&0xFF)==0 && (value&0x100u))
            {const auto& c=channels.at(address);
            throw std::runtime_error("DMA start is not modelled (channel at "+std::to_string(address)+", chcr "+std::to_string(value)
                +", madr "+std::to_string(c.madr)+", qwc "+std::to_string(c.qwc)+", sadr "+std::to_string(c.sadr)+", tadr "+std::to_string(c.tadr)+")");}
        *field=(address&0xFF)==0x20?(value&0xFFFFu):(address&0xFF)==0x80?(value&0x3FF0u):value;
        return true;
    }

private:
    std::uint32_t* find(std::uint32_t address) {
        const auto channel=channels.find(address&~0xFFu);
        if(channel==channels.end())return nullptr;
        auto& c=channel->second;
        switch(address&0xFF) {
        case 0x00: return &c.chcr;
        case 0x10: return &c.madr;
        case 0x20: return &c.qwc;
        case 0x30: return &c.tadr;
        case 0x80: return &c.sadr;
        }
        return nullptr;
    }
    const std::uint32_t* find(std::uint32_t address) const {return const_cast<StoppedDmaChannels*>(this)->find(address);}
};
