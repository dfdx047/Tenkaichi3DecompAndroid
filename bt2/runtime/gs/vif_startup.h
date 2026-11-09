#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>

// Authored configuration-command subset. No unpack, DMA, VU launch or DIRECT.
struct VifStartup {
    std::uint32_t cycle=0,mode=0,mask=0,base=0,offset=0,itops=0,tops=0,code=0,error=0;
    unsigned phase=0,commands=0,words=0,fifo_writes=0,vu_resets=0,gif_resets=0;
    bool pending_mask=false,path3_masked=false,double_buffer=false;
    void consume(std::span<const std::uint32_t> input) {
        auto next=*this; // A rejected packet leaves configuration and stream position intact.
        for(auto word:input) {
            ++next.words;
            if(next.pending_mask) {next.mask=word;next.pending_mask=false;continue;}
            if(word&0x80FF0000u)throw std::runtime_error("Unsupported VIF interrupt/count field");
            const auto command=(word>>24)&127,imm=word&65535;next.code=word;
            switch(command) {
                case 0:if(imm)throw std::runtime_error("Unsupported VIF NOP immediate");break;
                case 1:next.cycle=imm;break;
                case 2:next.offset=imm&1023;next.double_buffer=false;next.tops=next.base;break;
                case 3:next.base=imm&1023;break;
                case 4:next.itops=imm&1023;break;
                case 5:next.mode=imm&3;break;
                case 6:next.path3_masked=(imm&0x8000)!=0;break;
                case 0x20:next.pending_mask=true;break;
                default:throw std::runtime_error("Unsupported VIF startup command");
            }
            ++next.commands;
        }
        ++next.fifo_writes;*this=next;
    }
};
