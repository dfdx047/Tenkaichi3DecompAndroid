#pragma once
#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include "cpu/gs_command_sink.h"

// One evidence-backed normal GIF configuration request. No hardware timing model.
class TextureGifStartup {
public:
    void supply(std::span<const std::uint8_t> data) {
        if(supplied || data.size()!=224)throw std::runtime_error("Invalid texture GIF evidence");
        std::memcpy(publications.data(),data.data(),160);
        if(publications[0][0]!=1 || publications[1][0]!=0 || publications[2][0]!=0x101 || publications[3][0]!=1
            || publications[4][2]!=0x1FFED40)throw std::runtime_error("Texture GIF loads/return evidence differs");
        for(unsigned i=0;i<4;++i)if(publications[i][1] || publications[i][2]!=0x1FFECE0 || publications[i][3]
            || publications[i][4]!=1 || publications[i][5]!=0x1FFED30 || publications[i][6] || publications[i][7])
            throw std::runtime_error("Texture GIF status evidence differs");
        expected.assign(data.begin()+160,data.end());supplied=true;
    }
    std::uint32_t idle() {require(0);phase=1;return publications[0][0];}
    std::uint32_t pathIdle() {require(1);phase=2;return publications[1][0];}
    void count(std::uint32_t value) {require(2);if(value!=4)fail();qwc=value;phase=3;}
    void address(std::uint32_t value,std::uint32_t expected_address,std::span<const std::uint8_t> ram) {
        require(3);if(value!=expected_address || value%16 || value>ram.size() || expected.size()>ram.size()-value)fail();
        // A packet supplied as zeros was not captured (game/startup_inputs.h): the game's is the reference.
        if(take_game_packets && std::all_of(expected.begin(),expected.end(),[](std::uint8_t b){return b==0;}))
            expected.assign(ram.begin()+value,ram.begin()+value+expected.size());
        if(!std::equal(expected.begin(),expected.end(),ram.begin()+value))fail();madr=value;phase=4;
    }
    void start(std::uint32_t value,std::span<const std::uint8_t> ram,GsCommandSink& gs) {
        require(4);if(value!=0x101 || madr>ram.size() || expected.size()>ram.size()-madr
            || !std::equal(expected.begin(),expected.end(),ram.begin()+madr))fail();
        gs.consumeConfiguration(ram.subspan(madr,expected.size()));madr+=qwc*16;qwc=0;chcr=value;phase=5;++starts;
    }
    std::uint32_t poll() {
        if(phase==5) {phase=6;++polls;return publications[2][0];}
        require(6);phase=7;chcr=1;++polls;++completions;return publications[3][0];
    }
    std::uint32_t pathComplete() {require(7);phase=8;return publications[3][7];}
    bool supplied=false;
    bool take_game_packets=false; // BT2_SELF_STARTUP (address above).
    unsigned phase=0,starts=0,polls=0,completions=0;
    std::uint32_t madr=0,qwc=0,chcr=0;
private:
    std::array<std::array<std::uint32_t,8>,5> publications{};
    std::vector<std::uint8_t> expected;
    [[noreturn]] static void fail() {throw std::runtime_error("Unsupported texture GIF request/order/source");}
    void require(unsigned expected_phase) const {if(!supplied || phase!=expected_phase)fail();}
};
