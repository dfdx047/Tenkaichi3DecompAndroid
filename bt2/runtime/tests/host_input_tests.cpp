// Host input: the mapping from keys and a gamepad to the game's controller, the DualSense report
// layouts, and live input on the DualShock 2 device (synthetic data only).
#include "host/dualsense_report.h"
#include "host/input_mapping.h"
#include "services/devices/dualshock2.h"
#include <iostream>
#include <set>
#include <vector>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"Host input check failed: "<<what<<'\n';std::exit(1);}}
unsigned bit(std::string_view name) {
    for(unsigned n=0;n<PadScript::names.size();++n)if(PadScript::names[n]==name)return n;
    std::cerr<<"No such pad button\n";std::exit(1);
}
}

int main() {
    const InputMapping mapping;
    const auto none=[](std::uint8_t){return false;};
    // Nothing held, no gamepad: neutral.
    check(mapInput(mapping,GamepadState{},none)==PadInput{},"neutral without input");
    // Keys: Z is cross, the up arrow is up.
    const std::set<std::uint8_t> keys{'Z',0x26};
    const auto held=[&](std::uint8_t key){return keys.contains(key);};
    check(mapInput(mapping,GamepadState{},held).pressed==(1u<<bit("cross")|1u<<bit("up")),"keys press their buttons");
    // Gamepad: the south button is cross, the left trigger L2; the sticks pass through as RX, RY, LX, LY.
    GamepadState pad;pad.connected=true;pad.buttons=1u<<GamepadState::South|1u<<GamepadState::LeftTrigger;pad.lx=0x10;pad.ly=0xF0;pad.rx=0x90;pad.ry=0x40;
    const auto mapped=mapInput(mapping,pad,none);
    check(mapped.pressed==(1u<<bit("cross")|1u<<bit("l2")),"gamepad buttons press theirs");
    check(mapped.sticks==std::array<std::uint8_t,4>{0x80,0x40,0x10,0xF0},"sticks in wire order, a small offset is rest");
    check(mapInput(mapping,pad,held).pressed==(1u<<bit("cross")|1u<<bit("up")|1u<<bit("l2")),"keys and gamepad combine");
    // Stick sensitivity scales the distance from the centre and stops at the ends; rest stays rest.
    InputMapping twice;twice.stick_sensitivity=200;
    check(mapInput(twice,pad,none).sticks==std::array<std::uint8_t,4>{0x80,0x00,0x00,0xFF},"double sensitivity reaches the ends early");
    InputMapping half;half.stick_sensitivity=50;
    check(mapInput(half,pad,none).sticks==std::array<std::uint8_t,4>{0x80,0x60,0x48,0xB8},"half sensitivity halves the distance");
    // A state that is not connected presses nothing, whatever its bits say.
    pad.connected=false;check(mapInput(mapping,pad,none)==PadInput{},"a disconnected gamepad is ignored");
    // Every PS2 button has its own key and its own gamepad button.
    check(std::set<std::uint8_t>(mapping.keys.begin(),mapping.keys.end()).size()==16
        && std::set<unsigned>(mapping.buttons.begin(),mapping.buttons.end()).size()==16,"default mapping has no duplicates");
    // A remapped button.
    InputMapping swapped;swapped.buttons[bit("cross")]=GamepadState::East;swapped.buttons[bit("circle")]=GamepadState::South;
    pad=GamepadState{};pad.connected=true;pad.buttons=1u<<GamepadState::South;
    check(mapInput(swapped,pad,none).pressed==1u<<bit("circle"),"a changed mapping applies");

    // DualSense over USB: report 0x01, sticks at 1..4, buttons at 8..10.
    std::vector<std::uint8_t> usb(64);usb[0]=0x01;usb[1]=0x11;usb[2]=0x22;usb[3]=0x33;usb[4]=0x44;
    usb[8]=0x20|0x01;usb[9]=0x20|0x02;usb[10]=0x01; // Cross, d-pad up-right; options, R1; PS.
    auto state=parseDualSenseReport(usb,false);
    check(state && state->connected && state->lx==0x11 && state->ly==0x22 && state->rx==0x33 && state->ry==0x44,"USB sticks");
    check(state->buttons==(1u<<GamepadState::South|1u<<GamepadState::Up|1u<<GamepadState::Right|1u<<GamepadState::Start|1u<<GamepadState::RightShoulder|1u<<GamepadState::Guide),"USB buttons");
    usb[8]=0x08;usb[9]=0;usb[10]=0;state=parseDualSenseReport(usb,false);check(state && !state->buttons,"d-pad value 8 is no direction");
    usb[8]=0x06;state=parseDualSenseReport(usb,false);check(state && state->buttons==1u<<GamepadState::Left,"d-pad left");
    usb[8]=0x07;state=parseDualSenseReport(usb,false);check(state && state->buttons==(1u<<GamepadState::Left|1u<<GamepadState::Up),"d-pad up-left");
    // Over Bluetooth: report 0x31, everything one byte later.
    std::vector<std::uint8_t> bt(78);bt[0]=0x31;bt[2]=0x55;bt[3]=0x66;bt[4]=0x77;bt[5]=0x88;bt[9]=0x80|0x08;bt[10]=0x10|0x40;
    state=parseDualSenseReport(bt,true);
    check(state && state->lx==0x55 && state->ry==0x88 && state->buttons==(1u<<GamepadState::North|1u<<GamepadState::Back|1u<<GamepadState::LeftStick),"Bluetooth full report");
    // Bluetooth before the full report: report 0x01 with buttons at 5..7.
    std::vector<std::uint8_t> simple(78);simple[0]=0x01;simple[1]=0x80;simple[5]=0x10|0x04;simple[6]=0x04;
    state=parseDualSenseReport(simple,true);
    check(state && state->lx==0x80 && state->buttons==(1u<<GamepadState::West|1u<<GamepadState::Down|1u<<GamepadState::LeftTrigger),"Bluetooth simple report");
    // Other reports and short ones are not input.
    usb[0]=0x02;check(!parseDualSenseReport(usb,false),"another report id is refused");
    check(!parseDualSenseReport(std::vector<std::uint8_t>{0x01,0,0},false) && !parseDualSenseReport({},false),"a short report is refused");

    // The pad device: live input replaces the script's buttons and sets the sticks; a poll reports them.
    DualShock2 device;device.script=PadScript::parse("1:cross");
    device.advanceField();check(device.buttons==std::uint16_t(~(1u<<bit("cross"))),"the script holds its button");
    PadInput live;live.pressed=std::uint16_t(1u<<bit("start"));live.sticks={1,2,3,4};
    device.hold(live);
    check(device.buttons==std::uint16_t(~(1u<<bit("start"))) && device.sticks==live.sticks,"live input replaces it");
    const auto reply=device.transfer({1,0x42,0,0,0});
    check(reply[3]==std::uint8_t(~(1u<<bit("start"))) && reply[4]==0xFF,"a poll reports the live buttons");
    std::cout<<"Host input checks passed.\n";
}
