// Stream audio mixer (docs/plans/sound/SOUND_OUTPUT_PLAN.md): the PCM the CRI player reports as played
// (cri_audio.h) becomes 48 kHz stereo output, one field at a time. Each voice is one CRI source, mono
// 16-bit, routed left, right or (a one-channel stream) to both. The player hands data over in blocks of 512 output samples while a
// field takes 800.8, so a voice starts with one block of silence: its queue then never runs dry
// while the player keeps playing. Time is logical: one field per VBLANK.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

class AudioMixer {
public:
    static constexpr unsigned rate=48000,block_samples=512;
    // Level of a stream voice on the output, 25/64 (0.39, -8.2 dB). Measured, not derived: against a
    // PCSX2 recording of the boot movies the unscaled PCM is 2.53 to 2.58 times too loud at every
    // frequency (SOUND_OUTPUT_PLAN.md). Where the original applies it (voice or master volume) is open.
    static constexpr std::int32_t stream_level_num=25,stream_level_den=64;
    unsigned underruns=0;            // Samples a started voice could not supply.
    // The player's volumes in percent: the stream voices, the further sources (`extra`) and the whole
    // mix. At 100 the output is the measured level, sample for sample.
    unsigned stream_percent=100,extra_percent=100,master_percent=100;
    std::uint64_t voice_samples=0;   // Output samples taken from voices.

    // `bytes` of 16-bit little-endian mono PCM at `hertz` for voice `key`, heard on `side` (0 left, 1 right, 2 both).
    void push(std::uint32_t key,unsigned side,unsigned hertz,std::span<const std::uint8_t> bytes) {
        if(side>2 || !hertz || rate%hertz || bytes.size()%2)throw std::runtime_error("Audio voice format is not modelled: "+std::to_string(hertz)+" Hz, side "+std::to_string(side));
        auto& v=voices_[key];v.side=side;
        if(v.queue.empty() && !v.running) {v.queue.insert(v.queue.end(),block_samples,std::int16_t(0));v.running=true;}
        // Integer upsampling with linear interpolation from the voice's last sample.
        const unsigned factor=rate/hertz;
        for(std::size_t n=0;n<bytes.size();n+=2) {
            std::int16_t sample;std::memcpy(&sample,bytes.data()+n,2);
            for(unsigned k=1;k<=factor;++k)v.queue.push_back(std::int16_t(v.last+(std::int32_t(sample)-v.last)*std::int32_t(k)/std::int32_t(factor)));
            v.last=sample;
        }
    }

    // One field of output: interleaved stereo. A field is 800.8 samples (48,000 Hz at 59.94 fields/s);
    // the fraction is carried, so five fields give 4,004 samples.
    // `extra` adds further sources (the effects sampler) to the field's mix before it is clamped.
    std::vector<std::int16_t> field(const std::function<void(std::span<std::int32_t>)>& extra={}) {
        ticks_+=4004;const std::size_t count=ticks_/5;ticks_%=5;
        std::vector<std::int32_t> mix(count*2);
        for(auto& [key,v]:voices_) {
            if(!v.running)continue;
            const auto have=std::min(count,v.queue.size());
            for(std::size_t n=0;n<have;++n) {
                const std::int32_t sample=v.queue[n]*stream_level_num/stream_level_den*std::int32_t(stream_percent)/100;
                if(v.side==2) {mix[n*2]+=sample;mix[n*2+1]+=sample;} else mix[n*2+v.side]+=sample;
            }
            v.queue.erase(v.queue.begin(),v.queue.begin()+std::ptrdiff_t(have));
            voice_samples+=have;
            // A voice that runs dry has stopped (or stalled): it starts again with a block of silence.
            if(have<count) {v.running=false;v.last=0;underruns+=unsigned(count-have);}
        }
        if(extra && extra_percent==100)extra(mix);
        else if(extra) {
            std::vector<std::int32_t> added(mix.size());extra(added);
            for(std::size_t n=0;n<mix.size();++n)mix[n]+=added[n]*std::int32_t(extra_percent)/100;
        }
        if(master_percent!=100)for(auto& sample:mix)sample=std::int32_t(std::int64_t(sample)*master_percent/100);
        std::vector<std::int16_t> out(mix.size());
        for(std::size_t n=0;n<mix.size();++n)out[n]=std::int16_t(std::clamp(mix[n],-32768,32767));
        return out;
    }

    // A voice whose source is gone: queued samples are dropped.
    void drop(std::uint32_t key) {voices_.erase(key);}

private:
    struct Voice {unsigned side=0;bool running=false;std::int16_t last=0;std::deque<std::int16_t> queue;};
    std::map<std::uint32_t,Voice> voices_;
    unsigned ticks_=0;
};
