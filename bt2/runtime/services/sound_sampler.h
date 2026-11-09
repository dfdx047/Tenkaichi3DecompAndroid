// Native sound effects (docs/plans/sound/SOUND_OUTPUT_PLAN.md): what the SOUNDS module and the sound
// driver do audibly, at the request boundary. The driver's block transfers fill sound memory; a bank
// load adds a bank on top of the loaded ones and an unload removes the top one; a play request names
// sounds by bank slot and number with a volume, a pan and a pitch offset.
// A voice plays one decoded sample at its rate (shifted by the pitch offset in cents) with linear
// interpolation; a sample that ends with a repeat flag loops from its loop start. Its level is the
// product of the request's, the sequence set's, the sequence's, the note's and the timbre's volumes
// (each of 127), the chip's half-scale voice volume and the slot's volume (of 0x100); the pan is
// constant-power.
// The slot byte of a request carries two flags, as the module's play handler reads them: 0x80 keeps
// the sound in a table of 48 so it can be stopped, 0x40 stops the kept sound of that slot and number.
// Not modelled: the envelope (a stopped voice fades over `release_samples` in place of its release),
// reverb and the chip's interpolation.
#pragma once
#include "services/sound_bank.h"
#include "services/spu_adpcm.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

class SoundSampler {
public:
    static constexpr std::size_t memory_bytes=2*1024*1024,max_voices=48,max_kept=48,slots=32;
    static constexpr unsigned output_rate=48000,full_volume=0x100,release_samples=480;
    unsigned played=0,looping=0,kept=0,kept_refused=0,stopped=0,stops_unmatched=0,silenced=0,stolen=0,peak_voices=0;

    // Sound driver block transfer: `data` to sound memory at `address`.
    void upload(std::uint32_t address,std::span<const std::uint8_t> data) {
        if(address>memory_.size() || data.size()>memory_.size()-address)throw std::runtime_error("Sound memory transfer out of range");
        std::copy(data.begin(),data.end(),memory_.begin()+address);
    }
    // SOUNDS bank load: the bank's samples lie at `address` in sound memory.
    void load(std::uint32_t address,std::span<const std::uint8_t> head,std::span<const std::uint8_t> sequences) {
        auto bank=SoundBank::parse(head,sequences);
        if(address>memory_.size() || bank.sample_bytes>memory_.size()-address)throw std::runtime_error("Sound bank samples outside sound memory");
        banks_.push_back({address,std::move(bank),next_bank_++});
    }
    // SOUNDS bank replacement: the bank in `slot` (already loaded) becomes the given one; the old bank's
    // voices end and the number of banks stays.
    void replace(std::size_t slot,std::uint32_t address,std::span<const std::uint8_t> head,std::span<const std::uint8_t> sequences) {
        if(slot>=banks_.size())throw std::runtime_error("Sound bank replacement of slot "+std::to_string(slot)+", which holds no bank");
        auto bank=SoundBank::parse(head,sequences);
        if(address>memory_.size() || bank.sample_bytes>memory_.size()-address)throw std::runtime_error("Sound bank samples outside sound memory");
        const auto id=banks_[slot].id;
        std::erase_if(voices_,[&](const Voice& v){return v.bank==id;});
        banks_[slot]={address,std::move(bank),next_bank_++};
    }
    // SOUNDS bank unload: the last loaded bank goes, with its voices.
    void unload() {
        if(banks_.empty())throw std::runtime_error("Sound bank unload without a bank");
        const auto id=banks_.back().id;banks_.pop_back();
        std::erase_if(voices_,[&](const Voice& v){return v.bank==id;});
    }
    std::size_t banks() const {return banks_.size();}
    std::size_t voices() const {return voices_.size();}

    // One sound of a play request. `slot` is the request's bank byte (slot in the low five bits, the
    // keep and stop flags above), `volume` and `pan` are 0..127 (pan 64 is the centre), `cents`
    // shifts the pitch.
    void play(unsigned slot,unsigned number,unsigned volume,unsigned pan,int cents) {
        const bool keep=slot&0x80,stop=slot&0x40;slot&=0x1F;
        if(keep && stop)throw std::runtime_error("Sound request keeps and stops at once: slot "+std::to_string(slot)+" number "+std::to_string(number));
        if(stop) {
            for(auto& v:voices_)if(v.kept && v.slot==slot && v.number==number) {v.kept=false;v.releasing=true;++stopped;return;}
            ++stops_unmatched;return; // The kept sound has ended by itself.
        }
        if(slot>=banks_.size() || volume>127 || pan>127)throw std::runtime_error("Sound request out of range: slot "+std::to_string(slot)+" volume "+std::to_string(volume)+" pan "+std::to_string(pan));
        const auto& loaded=banks_[slot];
        if(number>=loaded.bank.sounds.size() || !loaded.bank.sounds[number])throw std::runtime_error("Sound request for a sound the bank does not have: slot "+std::to_string(slot)+" number "+std::to_string(number));
        if(keep && std::size_t(std::count_if(voices_.begin(),voices_.end(),[](const Voice& v){return v.kept;}))>=max_kept) {++kept_refused;return;} // A full table drops the sound.
        const auto& s=*loaded.bank.sounds[number];
        Voice v;v.bank=loaded.id;v.slot=slot;v.number=number;v.kept=keep;
        const auto from=loaded.address+s.sample_offset;
        v.sample=std::make_shared<SpuSample>(decodeSpuAdpcm(std::span<const std::uint8_t>(memory_.data()+from,loaded.address+loaded.bank.sample_bytes-from)));
        v.step=double(s.rate)/output_rate*std::pow(2.0,cents/1200.0);
        const double level=volume/127.0*s.set_volume/127.0*s.sequence_volume/127.0*s.velocity/127.0*s.timbre_volume/127.0*0.5;
        const int position=std::clamp(int(pan)+int(s.set_pan)-64+int(s.sequence_pan)-64+int(s.timbre_pan)-64,0,127);
        const double angle=position/127.0*1.5707963267948966;
        v.left=level*std::cos(angle);v.right=level*std::sin(angle);
        if(voices_.size()>=max_voices) {voices_.erase(voices_.begin());++stolen;} // The oldest voice gives way.
        ++played;if(keep)++kept;if(v.sample->loops)++looping;
        voices_.push_back(std::move(v));
        peak_voices=std::max(peak_voices,unsigned(voices_.size()));
    }
    // Volume of the slots in `mask`, of `full_volume`. Playing voices follow it.
    void setVolume(std::uint32_t mask,unsigned volume) {
        if(volume>full_volume)throw std::runtime_error("Sound slot volume out of range: "+std::to_string(volume));
        for(unsigned n=0;n<slots;++n)if(mask>>n&1)volumes_[n]=volume;
    }
    // Ends every voice of the slots in `mask`.
    void silence(std::uint32_t mask) {
        for(auto& v:voices_)if(mask>>v.slot&1 && !v.releasing) {v.kept=false;v.releasing=true;++silenced;}
    }

    // Adds the playing voices to `mix` (interleaved stereo); finished voices end.
    void render(std::span<std::int32_t> mix) {
        for(auto& v:voices_) {
            const auto& pcm=v.sample->pcm;
            const double volume=double(volumes_[v.slot])/full_volume;
            for(std::size_t n=0;n+1<mix.size();n+=2) {
                if(v.position>=double(pcm.size())) {
                    if(!v.sample->loops || v.sample->loop_start>=pcm.size()) {v.done=true;break;}
                    v.position-=double(pcm.size()-v.sample->loop_start);
                }
                const auto index=std::size_t(v.position);
                const double next=index+1<pcm.size()?pcm[index+1]:v.sample->loops?pcm[v.sample->loop_start]:0;
                double value=(pcm[index]+(next-pcm[index])*(v.position-double(index)))*volume;
                if(v.releasing) {
                    if(v.released>=release_samples) {v.done=true;break;}
                    value*=1.0-double(v.released++)/release_samples;
                }
                mix[n]+=std::int32_t(std::lround(value*v.left));mix[n+1]+=std::int32_t(std::lround(value*v.right));
                v.position+=v.step;
            }
        }
        std::erase_if(voices_,[](const Voice& v){return v.done;});
    }

private:
    struct Loaded {std::uint32_t address;SoundBank bank;unsigned id;};
    struct Voice {
        unsigned bank=0,slot=0,number=0,released=0;std::shared_ptr<SpuSample> sample;double position=0,step=1,left=0,right=0;
        bool kept=false,releasing=false,done=false;
    };
    std::vector<std::uint8_t> memory_=std::vector<std::uint8_t>(memory_bytes);
    std::vector<Loaded> banks_;
    std::vector<Voice> voices_;
    std::array<unsigned,slots> volumes_=[]{std::array<unsigned,slots> v;v.fill(full_volume);return v;}();
    unsigned next_bank_=0;
};
