// CRI ADX stream channel buffers (0x880 bytes): word 0 is the command count, command records follow
// at 0x10-byte steps ({code, IOP object, arguments...}), and the last word is the EE's sequence counter.
// The IOP consumes the commands and returns the buffer with the count cleared and the records unchanged
// (PCSX2 captures rpc-cri-stream-reply, 2026-10-11: commands 8 and 5 on one object; 2026-10-12: the
// movie audio start, commands 8, 4 (48000 Hz) and 9 twice (per-channel pan), and on the second channel
// the audio data command 0x100 ({object, IOP address, byte count}) once per audio channel, then command 2 ({object, 1}) on the first channel; the 24 kHz stream after the
// movies adds command 3 ({object, 1, 0x65}; meaning unknown) and command 4 with rate 24000).
#pragma once
#include "cri_audio.h"
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>

constexpr std::size_t cri_stream_size=0x880;

// Validates the commands in a returned buffer and marks them consumed. Only the reached command codes
// are accepted; any other stops with a named error. The audio flow receives the player start (2) and
// the data announcements (0x100) of stream channel `channel`.
inline unsigned consumeCriStreamCommands(std::span<std::uint8_t> buffer,CriAudioFlow* audio=nullptr,unsigned channel=0) {
    if(buffer.size()!=cri_stream_size)throw std::runtime_error("CRI stream buffer size");
    std::uint32_t count;std::memcpy(&count,buffer.data(),4);
    if(count>(cri_stream_size-0x14)/0x10)throw std::runtime_error("CRI stream command count out of range");
    for(std::uint32_t i=0;i<count;++i) {
        std::uint32_t code;std::memcpy(&code,buffer.data()+0x10+i*0x10,4);
        if(code!=2 && code!=3 && code!=4 && code!=5 && code!=8 && code!=9 && code!=0x100) {
            // Name the whole upload: every record's four words.
            std::string text="Unsupported CRI stream command "+std::to_string(code)+" (record "+std::to_string(i)+" of "+std::to_string(count)+"):";
            for(std::uint32_t r=0;r<count;++r)for(unsigned w=0;w<4;++w) {
                std::uint32_t word;std::memcpy(&word,buffer.data()+0x10+r*0x10+w*4,4);
                text+=(w?" ":" | ")+std::to_string(word);
            }
            throw std::runtime_error(text);
        }
    }
    if(audio)for(std::uint32_t i=0;i<count;++i) {
        std::uint32_t words[4];std::memcpy(words,buffer.data()+0x10+i*0x10,16);
        if(words[0]!=0x100)audio->note(words[0],words[2],words[3]);
        if(words[0]==2)audio->play(words[1],words[2]);
        else if(words[0]==4)audio->rate(words[1],words[2]);
        else if(words[0]==0x100)audio->dataWritten(words[1],words[2],words[3],channel);
    }
    std::memset(buffer.data(),0,4);
    return count;
}

// Puts the IOP's messages into a returned buffer: the count in word 0 and one record per message,
// {0, EE object, address, bytes}.
inline void writeCriStreamMessages(std::span<std::uint8_t> buffer,const std::vector<CriAudioFlow::Message>& messages) {
    if(messages.size()>(cri_stream_size-0x50)/0x10)throw std::runtime_error("Too many CRI stream messages");
    const auto count=std::uint32_t(messages.size());std::memcpy(buffer.data(),&count,4);
    for(std::size_t i=0;i<messages.size();++i) {
        const std::uint32_t record[4]={0,messages[i].ee_object,messages[i].address,messages[i].bytes};
        std::memcpy(buffer.data()+0x10+i*0x10,record,16);
    }
}
