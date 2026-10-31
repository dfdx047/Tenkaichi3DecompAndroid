// Holds the run at the game's field rate by the host's clock. Every field has a deadline one period
// after the previous one; the run waits for it once per field, at the moment the field's picture is
// shown (or at the field's end when it shows none), so pictures appear at even intervals whatever
// the work before them took. A field that finishes late is not waited for, and after a long stall
// the deadlines start again from now instead of being caught up in a burst.
// The period can be nudged by a small factor, which is how the sound device's queue is kept level:
// the two clocks differ by a tiny amount, and running the fields a fraction of a percent faster or
// slower follows the device without touching the sound.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <chrono>

class FramePacer {
public:
    using Clock=std::chrono::steady_clock;
    static constexpr double field_seconds=1001.0/60000.0; // 59.94 fields per second.
    double waited_seconds=0; // Time spent waiting, for statistics: a field's work is its length minus this.
    unsigned late_fields=0;  // Fields whose deadline had passed when they asked.
    bool free_run=false;     // Fast forward: nothing is waited for; pacing starts afresh when it ends.
    bool fast() const {return free_run;}

    FramePacer() {
        timeBeginPeriod(1);
        // A timer that wakes within a fraction of a millisecond where the system has one (Windows 10 1803 on).
        timer_=CreateWaitableTimerExW(nullptr,nullptr,0x00000002 /* high resolution */,TIMER_ALL_ACCESS);
    }
    FramePacer(const FramePacer&)=delete;
    FramePacer& operator=(const FramePacer&)=delete;
    ~FramePacer() {if(timer_)CloseHandle(timer_);timeEndPeriod(1);}

    // A new field begins: its deadline is one period after the last one. `factor` scales the period
    // (within 1%).
    void fieldStarted(double factor=1.0) {
        const auto period=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(field_seconds*std::clamp(factor,0.99,1.01)));
        const auto now=Clock::now();
        if(!started_ || now-deadline_>4*period) {deadline_=now;started_=true;} // First field, or a stall: start over.
        deadline_+=period;waited_=false;
    }
    // Waits for the current field's deadline, once per field (a second call in the same field returns at once).
    // How far past the current field's deadline the run is now (0 when it is not due yet): BT3's renderer leaves
    // a picture out when this is over a field (gs/hw/hw_gs_mirror.h).
    double lateSeconds() const {
        if(!started_)return 0;
        const auto now=Clock::now();
        return now>deadline_?std::chrono::duration<double>(now-deadline_).count():0;
    }
    void wait() {
        if(fast()) {started_=false;return;}
        if(!started_ || waited_)return;
        waited_=true;
        const auto begin=Clock::now();
        if(begin>=deadline_) {++late_fields;return;}
        for(auto now=begin;now<deadline_;now=Clock::now()) {
            const auto left=deadline_-now;
            if(timer_ && left>std::chrono::microseconds(300)) {
                LARGE_INTEGER due;due.QuadPart=-std::chrono::duration_cast<std::chrono::nanoseconds>(left-std::chrono::microseconds(200)).count()/100;
                if(SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer_,INFINITE);
            } else if(!timer_ && left>std::chrono::milliseconds(2))Sleep(1);
            else YieldProcessor(); // The last fraction of a millisecond.
        }
        waited_seconds+=std::chrono::duration<double>(Clock::now()-begin).count();
    }
    // Whether this field's picture is shown: always at the game's speed; in fast forward only when a
    // field period has passed since the last one shown, so the run does not spend its time reading
    // back and drawing pictures nobody can see.
    bool pictureDue() {
        if(!fast())return true;
        const auto now=Clock::now();
        if(now-last_picture_<std::chrono::duration<double>(field_seconds))return false;
        last_picture_=now;return true;
    }

private:
    HANDLE timer_=nullptr;Clock::time_point deadline_{},last_picture_{};bool started_=false,waited_=false;
};
#else
#include "host/sdl/android_performance.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <atomic>

// The same clock elsewhere (Android, Linux): the system's sleep is already fine-grained, so the wait
// sleeps to within a millisecond of the deadline and yields for the rest. On Android each field's work
// (its length less the wait) is reported to the system's performance hints (host/sdl/android_performance.h).
// Fast forward asked from outside the pacer: the online library runs a session's start-up without waiting for the clock.
inline std::atomic<bool> bt2_fast_forward{false};

class FramePacer {
public:
    using Clock=std::chrono::steady_clock;
    static constexpr double field_seconds=1001.0/60000.0; // 59.94 fields per second.
    double waited_seconds=0;
    unsigned late_fields=0;
    bool free_run=false;
    bool fast() const {return free_run || bt2_fast_forward.load(std::memory_order_relaxed);}

    void fieldStarted(double factor=1.0) {
        const auto period=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(field_seconds*std::clamp(factor,0.99,1.01)));
        const auto now=Clock::now();
        hint_.start(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(field_seconds)).count());
        if(field_start_!=Clock::time_point{})
            hint_.report(std::chrono::duration_cast<std::chrono::nanoseconds>(now-field_start_-field_waited_).count());
        field_start_=now;field_waited_={};
        // More than four fields behind: the debt is held at four fields (no longer burst than that to catch up
        // after a load), not dropped. Dropped, the fields after a slow one were paced as if the run were on time:
        // in a fight's slow stretches the game thread slept 1 to 4 ms a frame while the game ran at 70 to 90% of
        // its speed (measured), and the time dropped was never made up. Held, a slow field is followed by fields
        // that do not wait until the run is back on the grid. The game's own timing is untouched: it still counts
        // fields; only when the host waits changes. (BT2_PERF_BASELINE: the earlier rule, the debt dropped and the
        // field due a whole field from now, to measure against.)
        static const bool baseline=std::getenv("BT2_PERF_BASELINE")!=nullptr;
        if(!started_) {deadline_=now-period;started_=true;}
        else if(now-deadline_>4*period) deadline_=baseline?now:now-4*period;
        deadline_+=period;waited_=false;
    }
    // How far past the current field's deadline the run is now (0 when it is not due yet): BT3's renderer leaves
    // a picture out when this is over a field (gs/hw/hw_gs_mirror.h).
    double lateSeconds() const {
        if(!started_)return 0;
        const auto now=Clock::now();
        return now>deadline_?std::chrono::duration<double>(now-deadline_).count():0;
    }
    void wait() {
        if(fast()) {started_=false;return;}
        if(!started_ || waited_)return;
        waited_=true;
        const auto begin=Clock::now();
        if(begin>=deadline_) {++late_fields;return;}
        if(deadline_-begin>std::chrono::milliseconds(1))std::this_thread::sleep_until(deadline_-std::chrono::milliseconds(1));
        while(Clock::now()<deadline_)std::this_thread::yield();
        const auto end=Clock::now();
        field_waited_+=end-begin;
        waited_seconds+=std::chrono::duration<double>(end-begin).count();
    }
    bool pictureDue() {
        if(!fast())return true;
        const auto now=Clock::now();
        if(now-last_picture_<std::chrono::duration<double>(field_seconds))return false;
        last_picture_=now;return true;
    }

private:
    Clock::time_point deadline_{},last_picture_{};bool started_=false,waited_=false;
    PerformanceHint hint_;Clock::time_point field_start_{};Clock::duration field_waited_{};
};
#endif
