#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "pad_input.h"
#include "pad_script.h"

// Authored DualShock 2 on the SIO2 pad bus: one whole packet in, one reply of the same length out.
// Protocol facts follow the public pad protocol (adapted from PCSX2's documented DualShock 2
// behaviour, not copied): reply byte 1 is the mode ID (0x41 digital, 0x73 analog, 0x79 DualShock 2,
// 0xF3 while in config), byte 2 is 0x5A, data follows. Byte 0 reads 0xFF as in the captured
// startup reply. Input is neutral (no buttons, sticks centred) unless a pad script or live input presses buttons;
// pressed buttons report full pressure in DualShock 2 mode.
class DualShock2 {
public:
    enum Mode : std::uint8_t {Digital=0x41,Analog=0x73,Pressure=0x79,Config=0xF3};
    std::uint16_t buttons=0xFFFF;                 // Active-low button bits.
    std::array<std::uint8_t,4> sticks{0x80,0x80,0x80,0x80}; // RX, RY, LX, LY.
    Mode mode=Digital;bool config=false,locked=false,led=false;
    std::uint8_t small_motor_map=0xFF,large_motor_map=0xFF;
    std::uint32_t response_mask=0;
    unsigned packets=0;
    std::vector<std::vector<std::uint8_t>> history; // Recent packets, for refusal diagnostics.
    PadScript script;unsigned field=0,script_steps=0;

    // One logical field: a script step starting at this field sets the held buttons.
    void advanceField() {
        ++field;
        if(const auto pressed=script.at(field)) {buttons=std::uint16_t(~*pressed);++script_steps;}
    }
    // Live input for the field that begins: replaces what the script holds.
    void hold(const PadInput& input) {buttons=std::uint16_t(~input.pressed);sticks=input.sticks;}

    std::vector<std::uint8_t> transfer(const std::vector<std::uint8_t>& tx) {
        history.push_back(tx);if(history.size()>6)history.erase(history.begin());
        try {return transferPacket(tx);}
        catch(const std::runtime_error& e) {
            std::string log;for(const auto& h:history){log+=" [";for(auto b:h){static const char* d="0123456789ABCDEF";log+=d[b>>4];log+=d[b&15];}log+="]";}
            throw std::runtime_error(std::string(e.what())+"; mode "+std::to_string(mode)+(config?" config":"")+"; packets"+log);
        }
    }
private:
    std::vector<std::uint8_t> transferPacket(const std::vector<std::uint8_t>& tx) {
        if(tx.size()<5 || tx[0]!=1)throw std::runtime_error("Unsupported pad packet");
        const std::uint8_t command=tx[1];
        const auto at=[&](std::size_t i){return i<tx.size()?tx[i]:std::uint8_t(0);};
        std::vector<std::uint8_t> rx(tx.size(),0);
        rx[0]=0xFF;rx[1]=config?std::uint8_t(Config):std::uint8_t(mode);rx[2]=0x5A;
        if(command!=0x42 && command!=0x43 && !config)throw std::runtime_error("Pad config command outside config mode");
        if(command==0x42 || (command==0x43 && !config)) {
            // Poll, or config entry from normal mode (which also reports the current input). The pad
            // answers as many bytes as the bus clocks: a short poll (the driver reads the mode ID first)
            // gets a truncated reply.
            rx[3]=std::uint8_t(buttons&0xFF);rx[4]=std::uint8_t(buttons>>8);
            if(mode!=Digital)for(unsigned i=0;i<4 && 5+i<rx.size();++i)rx[5+i]=sticks[i];
            // Pressure bytes (DualShock 2 mode): right, left, up, down, triangle, circle, cross, square,
            // L1, R1, L2, R2; a pressed button reports full pressure.
            constexpr std::array<unsigned,12> pressure_bits{5,7,4,6,12,13,14,15,10,11,8,9};
            if(mode==Pressure)for(unsigned i=0;i<12 && 9+i<rx.size();++i)rx[9+i]=(buttons>>pressure_bits[i])&1?0:0xFF;
            if(command==0x43)config=at(3)!=0;
        } else {
            if(tx.size()!=9)throw std::runtime_error("Pad config packet length differs");
            switch(command) {
            case 0x43: config=at(3)!=0;break; // Exit (0) or repeated entry while in config.
            case 0x44: led=at(3)!=0;mode=led?Analog:Digital;locked=at(4)==3;break; // Mode switch.
            case 0x45: rx[3]=0x03;rx[4]=0x02;rx[5]=led?1:0;rx[6]=0x02;rx[7]=0x01;break; // Status/model.
            case 0x46: if(at(3)){rx[5]=1;rx[6]=1;rx[7]=1;rx[8]=0x14;}else{rx[5]=1;rx[6]=2;rx[8]=0x0A;}break;
            case 0x47: rx[5]=0x02;rx[7]=0x01;break;
            case 0x4C: rx[6]=at(3)?0x07:0x04;break;
            case 0x4D: // Motor map: report the previous mapping, take the new one.
                rx[3]=small_motor_map;rx[4]=large_motor_map;for(unsigned i=5;i<9;++i)rx[i]=0xFF;
                small_motor_map=at(3);large_motor_map=at(4);break;
            case 0x4F: // Response bytes: 0x3F analog, 0x3FFFF DualShock 2 (pressure).
                response_mask=at(3)|(std::uint32_t(at(4))<<8)|(std::uint32_t(at(5))<<16);
                led=response_mask==0x3F || response_mask==0x3FFFF;
                mode=response_mask==0x3FFFF?Pressure:led?Analog:Digital;rx[8]=0x5A;break;
            case 0x41: if(mode!=Digital){rx[3]=0xFF;rx[4]=0xFF;rx[5]=0x03;rx[8]=0x5A;}break; // Pressure mask query.
            case 0x40: rx[5]=0x02;rx[8]=0x5A;break;
            default: throw std::runtime_error("Unsupported pad command");
            }
        }
        ++packets;return rx;
    }
};
