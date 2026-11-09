#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

// A separate diagnostic transport lifetime. It owns no old RPC services.
class PostResetSif {
public:
    void supply(std::span<const std::uint8_t> evidence) {
        constexpr std::array<std::uint32_t,34> expected={
            0,0,0x1FFED40,0x800044,0x84,0x3A9500,0x70000,0,0x1E640,
            1,0,0x1FFED40,0xA00044,0x184,0x3A94E0,0x70000,0,0x1E640,
            0x1E640,0x1E6C0,0,0x1E730,0x1E640,0x1E6C0,0x3A94C0,0x1E730,
            0,0,0x80000001,0,0,1,0,1};
        if(supplied || evidence.size()!=sizeof(expected) || std::memcmp(evidence.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("Invalid fresh SIF transport evidence");
        supplied=true;
    }
    void advance(unsigned expected) {
        if(!supplied || phase!=expected)throw std::runtime_error("Unsupported fresh SIF order");
        ++phase;
    }
    std::uint32_t status() {advance(0);return 0x800044;}
    std::uint32_t channel() {advance(1);return 0x84;}
    void chain() {advance(2);}
    void handler() {advance(3);}
    void enable() {advance(4);}
    std::uint32_t get(std::uint32_t reg) {
        if(phase==5 && reg==0x80000000) {advance(5);return 0;}
        if(phase==6 && reg==4) {advance(6);return 0x70000;}
        if(phase==7 && reg==2) {advance(7);return 0x1E640;}
        if(phase==11 && reg==0x80000002) {advance(11);return 0;}
        throw std::runtime_error("Unsupported fresh SIF query");
    }
    void set(std::uint32_t reg,std::uint32_t value) {
        if(phase==8 && reg==0x80000000 && value==0x1E640) {advance(8);return;}
        if(phase==9 && reg==0x80000001 && value==0x3A9598) {advance(9);return;}
        if(phase==13 && reg==0x80000002 && value==1) {advance(13);rpc_initialized=true;return;}
        throw std::runtime_error("Unsupported fresh SIF register mutation");
    }
    // INIT_CMD opt 0 installs EE receive ownership; opt 1 requests RPC init.
    // IDs are authored nonzero logical tokens, not captured BIOS DMA IDs.
    std::uint32_t submit(std::span<const std::uint8_t> packet) {
        const unsigned opt=phase==10?0u:1u;
        if(!supplied || (phase!=10 && phase!=12) || packet.size()!=(opt?16u:20u))
            throw std::runtime_error("Unsupported fresh INIT_CMD submission");
        std::array<std::uint32_t,5> words{};std::memcpy(words.data(),packet.data(),packet.size());
        if(words[0]!=packet.size() || words[1]!=0 || words[2]!=0x80000002 || words[3]!=opt
            || (!opt && words[4]!=0x3A94C0))throw std::runtime_error("Fresh INIT_CMD payload differs");
        advance(opt?12:10);++submissions;
        if(!opt)ee_receive_address=0x3A94C0;
        else ++sreg_responses;
        return 0x10000u+submissions;
    }
    bool supplied=false,rpc_initialized=false;
    unsigned phase=0,submissions=0,sreg_responses=0;
    std::uint32_t ee_receive_address=0;
};

// Diagnostic service ownership for epoch 1, independent of pre-reset clients.
class PostResetCd {
public:
    void supply(std::span<const std::uint8_t> evidence) {
        std::array<std::uint32_t,85> expected{};
        for(unsigned n : {0u,16u}) {
            expected[n+2]=0x80000008;expected[n+5]=0x203A97C0;
            expected[n+7]=0x3A87A8;expected[n+8]=0x80000009;
        }
        expected[25]=0x43388;expected[26]=0x441B8;
        expected[32]=0x1000;expected[33]=0x3695C0;expected[34]=0x80000008;
        expected[39]=0x3A87A8;expected[40]=0x8000000A;
        expected[48]=1;expected[49]=expected[50]=0x225;
        expected[54]=0x80000008;expected[57]=0x203A97C0;expected[59]=0x369F40;expected[60]=0x80000009;
        expected[61]=0x434F0;expected[62]=0x439A8;
        expected[68]=0x400;expected[69]=0x3695C0;expected[70]=0x80000008;expected[75]=0x369F40;expected[76]=0x8000000A;expected[84]=1;
        if(supplied || (evidence.size()!=208 && evidence.size()!=340) || std::memcmp(evidence.data(),expected.data(),evidence.size()))
            throw std::runtime_error("Fresh CD evidence differs");
        std::copy(evidence.begin(),evidence.end(),replies.begin());supplied=true;scmd_supplied=evidence.size()==340;
    }
    bool supplied=false;
    bool scmd_supplied=false;
    unsigned binds=0,calls=0,scmd_binds=0,modes=0;
    unsigned operations() const {return binds+calls+scmd_binds+modes;}
    std::array<std::uint8_t,340> replies{};
};

// Separate LOADFILE binding/version diagnostic; module loading is never acknowledged.
class PostResetLoadfile {
public:
    void supply(std::span<const std::uint8_t> evidence) {
        std::array<std::uint32_t,33> expected{};
        expected[2]=0x80000008;expected[5]=0x203A97C0;expected[7]=0x3ACC00;
        expected[8]=0x80000009;expected[9]=0x22568;expected[10]=0x225B0;
        expected[16]=0x400;expected[17]=0x3ACA00;expected[18]=0x80000008;
        expected[23]=0x3ACC00;expected[24]=0x8000000A;expected[32]=0x30303033;
        if(supplied || evidence.size()!=sizeof(expected) || std::memcmp(evidence.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("LOADFILE evidence differs");
        std::copy(evidence.begin(),evidence.end(),replies.begin());supplied=true;
    }
    bool supplied=false;
    unsigned binds=0,versions=0;
    unsigned operations() const {return binds+versions;}
    std::array<std::uint8_t,132> replies{};
};
