// The on-screen controller of the Android app (Dragon Rage's TouchBridge, android/app/.../touch): player
// 1's PS2 buttons, active-high in the order of `PadScript::names` (the bits of BT3's TouchConfig are the
// same), and its sticks, 0x80 at rest. Written by the app's thread (runtime/android/android_main.cpp),
// read once per field by the player's session; added to what the gamepad and the keyboard give.
#pragma once
#include "services/devices/pad_input.h"
#include <atomic>
#include <cstdint>

class TouchPad {
public:
    static void set(unsigned buttons,int lx,int ly,int rx,int ry) {
        state().store(pack(buttons,lx,ly,rx,ry),std::memory_order_relaxed);
    }
    // The controller's state; a stick at rest leaves the gamepad's stick as it was.
    static PadInput get() {
        const std::uint64_t word=state().load(std::memory_order_relaxed);
        PadInput out;out.pressed=std::uint16_t(word);
        out.sticks={std::uint8_t(word>>40),std::uint8_t(word>>48),std::uint8_t(word>>24),std::uint8_t(word>>32)}; // RX, RY, LX, LY
        return out;
    }
    // `touch` over `pad`: its buttons added, each of its sticks in place of the pad's when moved.
    static PadInput merge(PadInput pad,const PadInput& touch) {
        pad.pressed|=touch.pressed;
        for(unsigned n=0;n<4;n+=2)
            if(touch.sticks[n]!=0x80 || touch.sticks[n+1]!=0x80) {pad.sticks[n]=touch.sticks[n];pad.sticks[n+1]=touch.sticks[n+1];}
        return pad;
    }

private:
    static std::uint8_t byte(int v) {return std::uint8_t(v<0?0:v>255?255:v);}
    static std::uint64_t pack(unsigned buttons,int lx,int ly,int rx,int ry) {
        return std::uint64_t(buttons&0xFFFF)|std::uint64_t(byte(lx))<<24|std::uint64_t(byte(ly))<<32|std::uint64_t(byte(rx))<<40|std::uint64_t(byte(ry))<<48;
    }
    static std::atomic<std::uint64_t>& state() {
        static std::atomic<std::uint64_t> word{pack(0,0x80,0x80,0x80,0x80)};
        return word;
    }
};
