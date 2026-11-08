// Streaming WAV file (16-bit PCM): samples are appended as they are produced and the header's sizes
// are written when the file is closed.
#pragma once
#include <cstdint>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>

class WavWriter {
public:
    WavWriter(const std::string& path,unsigned rate,unsigned channels):file_(path,std::ios::binary),rate_(rate),channels_(channels) {
        if(!file_)throw std::runtime_error("Cannot write "+path);
        header();
    }
    ~WavWriter() {file_.seekp(0);header();}
    void write(std::span<const std::int16_t> samples) {
        file_.write(reinterpret_cast<const char*>(samples.data()),std::streamsize(samples.size()*2));
        bytes_+=std::uint32_t(samples.size()*2);
    }
private:
    void header() {
        const auto u32=[&](std::uint32_t v){file_.write(reinterpret_cast<const char*>(&v),4);};
        const auto u16=[&](std::uint16_t v){file_.write(reinterpret_cast<const char*>(&v),2);};
        file_.write("RIFF",4);u32(36+bytes_);file_.write("WAVEfmt ",8);u32(16);u16(1);u16(std::uint16_t(channels_));
        u32(rate_);u32(rate_*channels_*2);u16(std::uint16_t(channels_*2));u16(16);file_.write("data",4);u32(bytes_);
    }
    std::ofstream file_;unsigned rate_,channels_;std::uint32_t bytes_=0;
};
