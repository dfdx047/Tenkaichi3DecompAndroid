#include "ipu_input.h"
#include <cstring>
#include <stdexcept>
#include <string>

void IpuInput::start(std::uint32_t chcr,std::span<const std::uint8_t> ram) {
    const unsigned mode=(chcr>>2)&3;
    if((chcr&1)!=1 || mode>1 || (chcr&0xC0))throw std::runtime_error("Unsupported toIPU DMA mode, CHCR "+std::to_string(chcr));
    ram_=ram;last_tag_=mode==0;
    if(mode==0 && !channel.qwc)throw std::runtime_error("toIPU normal transfer without QWC");
    // A chain start with QWC 0 reads its first tag at TADR; the TAG field keeps its last value.
    channel.chcr=(channel.chcr&0xFFFF0000u)|(chcr&0xFFFFu);
    ++dma_starts;topUp();
}

void IpuInput::push(const std::array<std::uint8_t,16>& qword) {
    if(fifoQwords()>=8)throw std::runtime_error("IPU input FIFO overflow");
    queue_.push_back(qword);
}

void IpuInput::clear(unsigned bit_pointer) {queue_.clear();bit_=bit_pointer&0x7F;held_=0;topUp();}

bool IpuInput::pull() {
    if(!running())return false;
    while(!channel.qwc) {
        if(last_tag_) {channel.chcr&=~0x100u;return false;} // Transfer complete.
        const auto tag=channel.tadr;
        if(tag%16 || tag+16>ram_.size())throw std::runtime_error("toIPU DMA tag outside RAM");
        std::uint64_t lo;std::memcpy(&lo,ram_.data()+tag,8);
        const std::uint32_t qwc=std::uint32_t(lo&0xFFFF),id=std::uint32_t((lo>>28)&7),addr=std::uint32_t((lo>>32)&0x7FFFFFF0);
        if(lo&0x80000000u)throw std::runtime_error("toIPU DMA tag IRQ is not modelled");
        if(id==5 || id==6)throw std::runtime_error("toIPU DMA call/ret tags are not modelled");
        channel.chcr=(channel.chcr&0xFFFFu)|(std::uint32_t(lo)&0xFFFF0000u);channel.qwc=qwc;
        switch(id) {
        case 0: channel.madr=addr;channel.tadr=tag+16;last_tag_=true;break;          // REFE
        case 1: channel.madr=tag+16;channel.tadr=tag+16+qwc*16;break;              // CNT
        case 2: channel.madr=tag+16;channel.tadr=addr;break;                       // NEXT
        case 3: case 4: channel.madr=addr;channel.tadr=tag+16;break;               // REF, REFS
        case 7: channel.madr=tag+16;channel.tadr=tag+16+qwc*16;last_tag_=true;break; // END
        }
    }
    if(channel.madr%16 || channel.madr+16>ram_.size())throw std::runtime_error("toIPU DMA data outside RAM");
    std::array<std::uint8_t,16> qword;std::memcpy(qword.data(),ram_.data()+channel.madr,16);
    queue_.push_back(qword);channel.madr+=16;--channel.qwc;++dma_qwords;
    if(!channel.qwc && last_tag_)channel.chcr&=~0x100u;
    return true;
}

bool IpuInput::available(unsigned bits) {
    while(bufferedBits()<bits)if(fifoQwords()>=8 || !pull())return false;
    // Reading moves the quadwords that hold these bits from the FIFO into the bit buffer.
    const unsigned needed=(bit_+bits+127)/128;
    if(needed>held_)held_=needed<2?needed:2;
    return true;
}

std::uint32_t IpuInput::peek(unsigned bits) {
    std::uint64_t value=0;unsigned at=bit_;std::size_t q=0;
    for(unsigned n=0;n<bits;++n) {
        const auto byte=queue_[q][at/8];value=(value<<1)|((byte>>(7-at%8))&1);
        if(++at==128){at=0;++q;}
    }
    return std::uint32_t(value);
}

void IpuInput::skip(unsigned bits) {
    bit_+=bits;
    while(bit_>=128){queue_.pop_front();bit_-=128;if(held_)--held_;}
    topUp();
}

unsigned IpuInput::fifoQwords() const {return unsigned(queue_.size())-held_;}

std::uint32_t IpuInput::bpRegister() const {
    return bit_|(fifoQwords()<<8)|(held_<<16);
}

bool IpuInput::takeBytes(std::uint8_t* out,unsigned count) {
    if(!available(count*8))return false;
    for(unsigned n=0;n<count;++n){out[n]=std::uint8_t(peek(8));skip(8);}
    return true;
}
