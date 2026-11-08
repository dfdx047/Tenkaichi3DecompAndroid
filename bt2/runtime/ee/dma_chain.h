// EE DMA source chain mode (CHCR MOD=1): DMA tags are walked from TADR until REFE or END.
// Shared by the channels that reach chain mode (GIF channel 2, VIF1 channel 1).
#pragma once
#include <cstdint>
#include <span>
#include <vector>

struct DmaChainSegment {
    std::uint32_t tag=0;                  // Tag address.
    const std::uint8_t* tag_words=nullptr; // Upper 64 bits of the tag (sent when TTE is set).
    std::span<const std::uint8_t> data;   // The tag's data qwords (may be empty).
};

struct DmaChain {
    std::vector<DmaChainSegment> segments;
    std::uint32_t next_tadr=0; // TADR after the transfer ends.
};

// Walks the chain synchronously. CNT, NEXT, REF, REFS, REFE, END, CALL and RET (two return levels, as
// the channel's address stack) are supported; tag IRQs throw (not reached). The channel name prefixes every error.
DmaChain walkDmaSourceChain(std::span<const std::uint8_t> ram,std::uint32_t tadr,const char* channel);
