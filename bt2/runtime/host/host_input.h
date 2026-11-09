// Host input for the game and the menus (Win32): the keyboard, the first connected XInput pad and a
// DualSense read directly over HID (USB or Bluetooth, no wrapper needed). `gamepad()` returns the
// XInput pad when one is connected, otherwise the DualSense. A DualSense is read on its own thread,
// because HID reports arrive when the controller sends them.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>
#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
}
#include "host/dualsense_report.h"
#include "host/gamepad.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

class DualSenseReader {
public:
    DualSenseReader():thread_([this]{run();}) {}
    DualSenseReader(const DualSenseReader&)=delete;
    DualSenseReader& operator=(const DualSenseReader&)=delete;
    ~DualSenseReader() {
        stop_=true;
        {std::lock_guard lock(mutex_);if(device_!=INVALID_HANDLE_VALUE)CancelIoEx(device_,nullptr);}
        thread_.join();
    }
    GamepadState state() {std::lock_guard lock(mutex_);return state_;}

private:
    // Opens the first present DualSense (Sony 054C: 0CE6, or the Edge 0DF2).
    HANDLE open(bool& bluetooth,unsigned& report_bytes) {
        GUID hid;HidD_GetHidGuid(&hid);
        const HDEVINFO set=SetupDiGetClassDevsA(&hid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
        if(set==INVALID_HANDLE_VALUE)return INVALID_HANDLE_VALUE;
        HANDLE found=INVALID_HANDLE_VALUE;
        SP_DEVICE_INTERFACE_DATA item{};item.cbSize=sizeof item;
        for(DWORD n=0;found==INVALID_HANDLE_VALUE && SetupDiEnumDeviceInterfaces(set,nullptr,&hid,n,&item);++n) {
            DWORD needed=0;SetupDiGetDeviceInterfaceDetailA(set,&item,nullptr,0,&needed,nullptr);
            std::vector<char> storage(needed);
            auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(storage.data());detail->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
            if(!needed || !SetupDiGetDeviceInterfaceDetailA(set,&item,detail,needed,nullptr,nullptr))continue;
            const HANDLE device=CreateFileA(detail->DevicePath,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
            if(device==INVALID_HANDLE_VALUE)continue;
            HIDD_ATTRIBUTES attributes{};attributes.Size=sizeof attributes;
            PHIDP_PREPARSED_DATA parsed=nullptr;HIDP_CAPS caps{};
            if(HidD_GetAttributes(device,&attributes) && attributes.VendorID==0x054C && (attributes.ProductID==0x0CE6 || attributes.ProductID==0x0DF2)
                && HidD_GetPreparsedData(device,&parsed)) {
                if(HidP_GetCaps(parsed,&caps)==HIDP_STATUS_SUCCESS && caps.InputReportByteLength>=10) {
                    report_bytes=caps.InputReportByteLength;bluetooth=report_bytes>64;
                    // Over Bluetooth the controller sends its full report only after this feature report was read.
                    if(bluetooth) {std::vector<std::uint8_t> feature(caps.FeatureReportByteLength?caps.FeatureReportByteLength:64);feature[0]=0x05;HidD_GetFeature(device,feature.data(),ULONG(feature.size()));}
                    found=device;
                }
                HidD_FreePreparsedData(parsed);
            }
            if(found==INVALID_HANDLE_VALUE)CloseHandle(device);
        }
        SetupDiDestroyDeviceInfoList(set);
        return found;
    }
    void run() {
        while(!stop_) {
            bool bluetooth=false;unsigned report_bytes=0;
            const HANDLE device=open(bluetooth,report_bytes);
            if(device==INVALID_HANDLE_VALUE) {
                for(unsigned n=0;n<10 && !stop_;++n)std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Look again in a second.
                continue;
            }
            {std::lock_guard lock(mutex_);device_=device;}
            std::vector<std::uint8_t> report(report_bytes);
            while(!stop_) {
                DWORD read=0;
                if(!ReadFile(device,report.data(),DWORD(report.size()),&read,nullptr))break; // Unplugged, or cancelled at shutdown.
                if(const auto parsed=parseDualSenseReport(std::span<const std::uint8_t>(report.data(),read),bluetooth)) {std::lock_guard lock(mutex_);state_=*parsed;}
            }
            {std::lock_guard lock(mutex_);device_=INVALID_HANDLE_VALUE;state_={};}
            CloseHandle(device);
        }
    }
    std::mutex mutex_;GamepadState state_;HANDLE device_=INVALID_HANDLE_VALUE;
    std::atomic<bool> stop_{false};
    std::thread thread_; // Last member: it starts in the constructor and uses the others.
};

class HostInput {
public:
    // The first connected XInput pad, otherwise the DualSense, otherwise a disconnected state.
    GamepadState gamepad() {
        for(DWORD user=0;user<XUSER_MAX_COUNT;++user) {
            XINPUT_STATE x{};
            if(XInputGetState(user,&x)!=ERROR_SUCCESS)continue;
            const auto& g=x.Gamepad;GamepadState s;s.connected=true;
            const auto set=[&](GamepadState::Button b,bool on){if(on)s.buttons|=1u<<b;};
            set(GamepadState::South,g.wButtons&XINPUT_GAMEPAD_A);set(GamepadState::East,g.wButtons&XINPUT_GAMEPAD_B);
            set(GamepadState::West,g.wButtons&XINPUT_GAMEPAD_X);set(GamepadState::North,g.wButtons&XINPUT_GAMEPAD_Y);
            set(GamepadState::LeftShoulder,g.wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER);set(GamepadState::RightShoulder,g.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER);
            set(GamepadState::LeftTrigger,g.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD);set(GamepadState::RightTrigger,g.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
            set(GamepadState::Back,g.wButtons&XINPUT_GAMEPAD_BACK);set(GamepadState::Start,g.wButtons&XINPUT_GAMEPAD_START);
            set(GamepadState::LeftStick,g.wButtons&XINPUT_GAMEPAD_LEFT_THUMB);set(GamepadState::RightStick,g.wButtons&XINPUT_GAMEPAD_RIGHT_THUMB);
            set(GamepadState::Up,g.wButtons&XINPUT_GAMEPAD_DPAD_UP);set(GamepadState::Down,g.wButtons&XINPUT_GAMEPAD_DPAD_DOWN);
            set(GamepadState::Left,g.wButtons&XINPUT_GAMEPAD_DPAD_LEFT);set(GamepadState::Right,g.wButtons&XINPUT_GAMEPAD_DPAD_RIGHT);
            // XInput axes are signed with up positive; the neutral form is 0..255 with up at 0.
            const auto axis=[](int v){return std::uint8_t((v+32768)>>8);};
            s.lx=axis(g.sThumbLX);s.ly=axis(-1-g.sThumbLY);s.rx=axis(g.sThumbRX);s.ry=axis(-1-g.sThumbRY);
            return s;
        }
        return dualsense_.state();
    }
    // Whether a key (virtual-key code) is down right now.
    static bool keyDown(std::uint8_t key) {return (GetAsyncKeyState(key)&0x8000)!=0;}

private:
    DualSenseReader dualsense_;
};
#elif defined(BT2_SDL)
#include "host/sdl/sdl_host_input.h"
#endif
