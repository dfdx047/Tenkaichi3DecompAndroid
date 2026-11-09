// IPU input side: the toIPU DMA channel (4), the 8-quadword input FIFO and the bit pointer
// (docs/plans/video/IPU_PLAN.md). As on hardware, a running channel keeps the FIFO full: it fills on
// start and refills as commands consume bits, so MADR/QWC/TADR and CHCR STR advance with the stream.
// Bits are read MSB first from the bytes in memory order (MPEG bit order).
#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <span>

class IpuInput {
public:
    struct Channel {std::uint32_t chcr=0,madr=0,qwc=0,tadr=0;};
    Channel channel; // toIPU registers as the game sees them.
    unsigned dma_starts=0,dma_qwords=0;

    // Starts the channel (CHCR write with STR); normal (MOD 0) or source chain (MOD 1) from memory.
    void start(std::uint32_t chcr,std::span<const std::uint8_t> ram);
    bool running() const {return channel.chcr&0x100u;}
    // Pushes a quadword written to IN_FIFO by the CPU.
    void push(const std::array<std::uint8_t,16>& qword);
    // BCLR: drops the FIFO and sets the bit pointer within the first quadword that follows.
    void clear(unsigned bit_pointer);
    // True when at least `bits` bits are available (pulling from the channel as needed).
    bool available(unsigned bits);
    std::uint32_t peek(unsigned bits); // 1..32 bits, MSB first; requires available(bits).
    void skip(unsigned bits);           // Requires available(bits).
    // IPU_BP fields: BP (bit 0-6), IFC (bit 8-11, FIFO quadwords), FP (bit 16-17, quadwords held by the
    // decoder's bit buffer, at most 2).
    std::uint32_t bpRegister() const;
    unsigned fifoQwords() const;
    // Takes `count` bytes at the bit pointer for table commands (SETIQ/SETVQ); false when not all are available.
    bool takeBytes(std::uint8_t* out,unsigned count);

private:
    std::deque<std::array<std::uint8_t,16>> queue_; // Bit buffer (front) and FIFO.
    unsigned bit_=0;                                 // Bit pointer within queue_.front().
    unsigned held_=0;                                // Front quadwords in the decoder's bit buffer (0-2).
    std::span<const std::uint8_t> ram_;
    bool last_tag_=false;
    bool pull(); // Moves one quadword from the channel into the queue; false when none is available.
    void topUp() {while(fifoQwords()<8 && pull()){}} // Keeps the FIFO full while the channel runs.
    unsigned bufferedBits() const {return unsigned(queue_.size())*128-bit_;}
};
