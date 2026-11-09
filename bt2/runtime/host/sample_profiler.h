// A sampling profiler for the runner's own thread (diagnostic, BT2_SAMPLE_PROFILE=<path>): a helper
// thread stops the sampled thread about a thousand times per second, notes where it is executing
// and lets it go on. At the end it writes one line per place: the module's file name, the offset in
// it, the number of samples and, for a place in another module, the program's code that called it. `tools/perf/profile_report.py` turns the offsets into function
// names with the linker's map file. Callers inside the program are not noted.
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
#include <atomic>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

class SampleProfiler {
public:
    // Samples the calling thread from now until destruction.
    explicit SampleProfiler(std::string path):path_(std::move(path)) {
        DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&target_,THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,0);
        const auto* image=reinterpret_cast<const std::uint8_t*>(GetModuleHandleA(nullptr));
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(image+reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew);
        program_begin_=reinterpret_cast<std::uint64_t>(image);program_end_=program_begin_+nt->OptionalHeader.SizeOfImage;
        samples_.reserve(capacity); // Filled without allocating: the sampled thread may hold the heap's lock while it is stopped.
        sampler_=std::thread([this]{run();});
    }
    SampleProfiler(const SampleProfiler&)=delete;
    SampleProfiler& operator=(const SampleProfiler&)=delete;
    ~SampleProfiler() {
        stop_=true;sampler_.join();CloseHandle(target_);
        // One line per place: module file name, offset in it, samples, and for a place outside the
        // program the offset of the program's code that called into it (0 when none was found).
        std::map<std::tuple<std::string,std::uint64_t,std::uint64_t>,unsigned> places;
        for(const auto& sample:samples_) {
            HMODULE module=nullptr;
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(sample.at),&module) && module) {
                char full[MAX_PATH];const auto length=GetModuleFileNameA(module,full,MAX_PATH);
                const std::string file(full,length);const auto slash=file.find_last_of("\\/");
                const bool own=sample.at>=program_begin_ && sample.at<program_end_;
                // Outside the program only the module and the caller matter, not the exact place.
                ++places[{slash==std::string::npos?file:file.substr(slash+1),own?sample.at-program_begin_:0,own || !sample.caller?0:sample.caller-program_begin_}];
            } else ++places[{"?",sample.at,0}];
        }
        std::ofstream out(path_);
        out<<"samples "<<samples_.size()<<"\n";
        for(const auto& [place,count]:places)out<<std::get<0>(place)<<' '<<std::hex<<std::get<1>(place)<<std::dec<<' '<<count<<' '<<std::hex<<std::get<2>(place)<<std::dec<<"\n";
    }

private:
    struct Sample {std::uint64_t at,caller;};
    static constexpr std::size_t capacity=4'000'000;
    void run() {
        timeBeginPeriod(1); // Millisecond sleeps: about a thousand samples per second.
        while(!stop_ && samples_.size()<capacity) {
            Sleep(1);
            if(SuspendThread(target_)==DWORD(-1))continue;
            CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL|CONTEXT_INTEGER;
            if(GetThreadContext(target_,&context)) {
                Sample sample{context.Rip,0};
                // Outside the program: unwind to the first frame inside it.
                for(unsigned depth=0;depth<24 && !(context.Rip>=program_begin_ && context.Rip<program_end_);++depth) {
                    DWORD64 base=0;
                    auto* function=RtlLookupFunctionEntry(context.Rip,&base,nullptr);
                    if(!function) { // A leaf: the return address is on top of the stack.
                        if(IsBadReadPtr(reinterpret_cast<const void*>(context.Rsp),8))break;
                        context.Rip=*reinterpret_cast<const DWORD64*>(context.Rsp);context.Rsp+=8;
                    } else {
                        void* handler_data=nullptr;DWORD64 frame=0;
                        RtlVirtualUnwind(UNW_FLAG_NHANDLER,base,context.Rip,function,&context,&handler_data,&frame,nullptr);
                    }
                    if(!context.Rip)break;
                }
                if(sample.at!=context.Rip && context.Rip>=program_begin_ && context.Rip<program_end_)sample.caller=context.Rip;
                samples_.push_back(sample);
            }
            ResumeThread(target_);
        }
        timeEndPeriod(1);
    }
    std::string path_;HANDLE target_=nullptr;
    std::uint64_t program_begin_=0,program_end_=0;
    std::vector<Sample> samples_;
    std::atomic<bool> stop_{false};std::thread sampler_;
};
#else
#include <string>
// No sampler outside Windows yet: BT2_SAMPLE_PROFILE is ignored there.
class SampleProfiler {
public:
    explicit SampleProfiler(std::string) {}
};
#endif
