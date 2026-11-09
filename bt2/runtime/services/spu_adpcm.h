// SPU2 ADPCM (the sound chip's sample format, as documented publicly): 16-byte blocks of a header byte
// (shift in the low nibble, predictor filter in the high nibble), a flag byte and 28 four-bit samples.
// Flags: bit 0 ends the sample, bit 1 with bit 0 repeats from the loop start, bit 2 marks the loop start.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

struct SpuSample {
    std::vector<std::int16_t> pcm;
    bool loops=false;          // Ends with a repeat flag.
    std::size_t loop_start=0;  // First sample of the block that carries the loop-start flag.
};

// Decodes the sample that starts at `data` (block-aligned) up to its end flag. A sample that runs off
// the data without an end flag is refused.
inline SpuSample decodeSpuAdpcm(std::span<const std::uint8_t> data) {
    static constexpr std::array<std::array<int,2>,5> filters{{{0,0},{60,0},{115,-52},{98,-55},{122,-60}}};
    SpuSample out;int older=0,old=0;
    for(std::size_t at=0;;at+=16) {
        if(at+16>data.size())throw std::runtime_error("SPU sample without an end flag");
        const unsigned shift=data[at]&15,filter=data[at]>>4,flags=data[at+1];
        if(filter>4 || shift>12)throw std::runtime_error("SPU sample block with an undefined filter or shift");
        if(flags&4)out.loop_start=out.pcm.size();
        for(unsigned n=0;n<28;++n) {
            const unsigned nibble=(data[at+2+n/2]>>(n&1?4:0))&15;
            const int value=(std::int16_t(nibble<<12)>>shift)+(old*filters[filter][0]+older*filters[filter][1])/64;
            const int sample=std::clamp(value,-32768,32767);
            out.pcm.push_back(std::int16_t(sample));older=old;old=sample;
        }
        if(flags&1) {out.loops=(flags&2)!=0;return out;}
    }
}
