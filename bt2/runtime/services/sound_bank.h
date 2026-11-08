// Sound effect banks as the game uploads them for SOUNDS (docs/plans/sound/SOUND_OUTPUT_PLAN.md): two
// blocks in Sony's chunked sound-data layout, read from BT2's own banks (2026-10-13).
// - Header block: "SCEI" chunks Vers, Head, Vagi and Setb. Head names the sample data's size and the
//   offsets of Vagi (per sample: offset into the sample data, sampling rate, loop flag) and Setb (one
//   set of 0x32-byte timbres: sample index, volume, pan and parameters that are the same in every
//   timbre seen).
// - Sequence block: chunks Vers, Sequ and Sesq. Sesq holds one short sequence per sound: a volume, a
//   pan and seven data bytes, a single note-on naming a timbre with a velocity.
// A sound number is a sequence; it plays one timbre, which plays one sample. Anything outside this
// regular shape is refused by name, so a bank the sampler cannot play as recorded is not loaded.
#pragma once
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

struct SoundBank {
    struct Sound {
        std::uint32_t sample_offset=0; // Into the bank's sample data.
        unsigned rate=0;
        bool loop=false;
        // Volumes of 0..127 that multiply: the sequence set's, the sequence's, the note's velocity, the timbre's.
        std::uint8_t set_volume=0,sequence_volume=0,velocity=0,timbre_volume=0;
        std::uint8_t set_pan=64,sequence_pan=64,timbre_pan=64; // 64 is the centre.
    };
    std::uint32_t sample_bytes=0;
    std::vector<std::optional<Sound>> sounds; // By sound number; absent where the bank has no sequence.

    static SoundBank parse(std::span<const std::uint8_t> head,std::span<const std::uint8_t> sequences) {
        const auto refuse=[](const char* what)->void {throw std::runtime_error(std::string("Sound bank: ")+what);};
        const auto u32=[&](std::span<const std::uint8_t> d,std::size_t at) {
            if(at+4>d.size())refuse("table outside its block");
            std::uint32_t v;std::memcpy(&v,d.data()+at,4);return v;
        };
        const auto u8=[&](std::span<const std::uint8_t> d,std::size_t at) {if(at>=d.size())refuse("table outside its block");return d[at];};
        constexpr std::uint32_t scei=0x53434549,vers=0x56657273,head_tag=0x48656164,vagi_tag=0x56616769,setb_tag=0x53657462,sequ_tag=0x53657175,sesq_tag=0x53657371;
        const auto chunk=[&](std::span<const std::uint8_t> d,std::size_t at,std::uint32_t tag,const char* what) {if(u32(d,at)!=scei || u32(d,at+4)!=tag)refuse(what);};

        SoundBank bank;
        chunk(head,0,vers,"header block has no version chunk");chunk(head,0x10,head_tag,"header block has no head chunk");
        bank.sample_bytes=u32(head,0x20);
        // Only the sample table and the timbre table are used by the game's banks.
        if(u32(head,0x24)!=UINT32_MAX || u32(head,0x28)!=UINT32_MAX || u32(head,0x2C)!=UINT32_MAX)refuse("program or sample-set chunks are not modelled");
        const std::size_t vagi=u32(head,0x30),setb=u32(head,0x34);
        chunk(head,vagi,vagi_tag,"no sample table");chunk(head,setb,setb_tag,"no timbre table");
        struct Sample {std::uint32_t offset;unsigned rate;bool loop;};
        std::vector<Sample> samples(std::size_t(u32(head,vagi+12))+1);
        if(samples.size()>4096)refuse("sample count out of range");
        for(std::size_t n=0;n<samples.size();++n) {
            const std::size_t at=vagi+u32(head,vagi+16+n*4);
            samples[n]={u32(head,at),unsigned(u32(head,at+4)&0xFFFF),u8(head,at+6)!=0};
            if(samples[n].offset>=bank.sample_bytes || samples[n].offset%16 || !samples[n].rate)refuse("sample entry out of range");
        }
        if(u32(head,setb+12)!=0)refuse("more than one timbre set");
        const std::size_t set=setb+u32(head,setb+16);
        if(u8(head,set+12)!=0x32)refuse("timbre record size differs");
        struct Timbre {unsigned sample;std::uint8_t volume,pan;};
        std::vector<Timbre> timbres(std::size_t(u8(head,set+14))+1);
        for(std::size_t n=0;n<timbres.size();++n) {
            const std::size_t at=set+16+n*0x32;
            timbres[n]={unsigned(u8(head,at))|unsigned(u8(head,at+1))<<8,u8(head,at+3),u8(head,at+4)};
            if(timbres[n].sample>=samples.size() || timbres[n].volume>127 || timbres[n].pan>127)refuse("timbre out of range");
        }

        chunk(sequences,0,vers,"sequence block has no version chunk");chunk(sequences,0x10,sequ_tag,"sequence block has no sequence chunk");
        const std::size_t sesq=u32(sequences,0x28);
        chunk(sequences,sesq,sesq_tag,"no effect sequence table");
        if(u32(sequences,sesq+12)!=0)refuse("more than one sequence set");
        const std::uint8_t set_volume=u8(sequences,sesq+0x14),set_pan=u8(sequences,sesq+0x15);
        const std::size_t count=std::size_t(u32(sequences,sesq+0x24))+1;
        if(count>4096 || set_volume>127 || set_pan>127)refuse("sequence set out of range");
        bank.sounds.resize(count);
        for(std::size_t n=0;n<count;++n) {
            const auto offset=u32(sequences,sesq+0x34+n*4);
            if(offset==UINT32_MAX)continue;
            const std::size_t at=sesq+offset,data=at+u32(sequences,at);
            // One note-on at time zero and the end mark: 00 90 <timbre, two bytes> <velocity> 00 FF.
            if(u32(sequences,at+8)!=7 || u8(sequences,data)!=0 || u8(sequences,data+1)!=0x90 || u8(sequences,data+5)!=0 || u8(sequences,data+6)!=0xFF)
                refuse("a sequence is not a single note-on");
            const unsigned timbre=unsigned(u8(sequences,data+2))<<8|u8(sequences,data+3);
            Sound s;s.set_volume=set_volume;s.set_pan=set_pan;s.sequence_volume=u8(sequences,at+4);s.sequence_pan=u8(sequences,at+5);s.velocity=u8(sequences,data+4);
            if(timbre>=timbres.size() || s.sequence_volume>127 || s.sequence_pan>127 || s.velocity>127)refuse("sequence out of range");
            const auto& t=timbres[timbre];const auto& sample=samples[t.sample];
            s.timbre_volume=t.volume;s.timbre_pan=t.pan;s.sample_offset=sample.offset;s.rate=sample.rate;s.loop=sample.loop;
            bank.sounds[n]=s;
        }
        return bank;
    }
};
