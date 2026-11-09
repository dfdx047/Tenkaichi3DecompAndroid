// Keeping the game thread fast on a phone (Android only; elsewhere these do nothing).
//
// PerformanceHint: Android's performance hints (ADPF, Android 13 and later). Each field the game thread
// reports how long its work took against the field's length, so the system keeps it on fast cores at a
// fitting clock instead of guessing from load. Looked up at run time: the app also runs on Android 10,
// where there is no such API and nothing is reported.
//
// pinToFastCores: the calling thread is kept off the slowest cluster (the efficiency cores), where a field's
// work cannot finish in time; the scheduler still chooses among the others.
#pragma once
#include <cstdint>
#ifdef __ANDROID__
#include <dlfcn.h>
#include <sched.h>
#include <unistd.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#endif

class PerformanceHint {
public:
    PerformanceHint()=default;
    PerformanceHint(const PerformanceHint&)=delete;
    PerformanceHint& operator=(const PerformanceHint&)=delete;
#ifdef __ANDROID__
    ~PerformanceHint() {if(session_ && close_)close_(session_);}
    // A session for the calling thread, `target_ns` per field (once; nothing when the system has no hints).
    void start(std::int64_t target_ns) {
        if(tried_)return;
        tried_=true;
        void* android=dlopen("libandroid.so",RTLD_NOW|RTLD_NOLOAD);
        if(!android)android=dlopen("libandroid.so",RTLD_NOW);
        if(!android)return;
        const auto manager=reinterpret_cast<void*(*)()>(dlsym(android,"APerformanceHint_getManager"));
        const auto create=reinterpret_cast<void*(*)(void*,const std::int32_t*,std::size_t,std::int64_t)>(dlsym(android,"APerformanceHint_createSession"));
        report_=reinterpret_cast<int(*)(void*,std::int64_t)>(dlsym(android,"APerformanceHint_reportActualWorkDuration"));
        close_=reinterpret_cast<void(*)(void*)>(dlsym(android,"APerformanceHint_closeSession"));
        if(!manager || !create || !report_ || !close_)return; // Android before 13.
        void* hints=manager();
        const std::int32_t thread=std::int32_t(gettid());
        session_=hints?create(hints,&thread,1,target_ns):nullptr;
        std::fprintf(stderr,"Performance hints: %s\n",session_?"on (game thread)":"not available");
    }
    void report(std::int64_t actual_ns) {if(session_ && actual_ns>0)report_(session_,actual_ns);}
private:
    void* session_=nullptr;bool tried_=false;
    int (*report_)(void*,std::int64_t)=nullptr;void (*close_)(void*)=nullptr;
#else
    void start(std::int64_t) {}
    void report(std::int64_t) {}
#endif
};

inline void pinToFastCores() {
#ifdef __ANDROID__
    // Each CPU's top clock (cpufreq); the slowest distinct one is the efficiency cluster.
    std::vector<long> top;
    for(int cpu=0;cpu<CPU_SETSIZE;++cpu) {
        std::ifstream in("/sys/devices/system/cpu/cpu"+std::to_string(cpu)+"/cpufreq/cpuinfo_max_freq");
        long khz=0;
        if(!(in>>khz))break;
        top.push_back(khz);
    }
    if(top.empty())return;
    long slowest=top[0],fastest=top[0];
    for(const long khz:top) {slowest=khz<slowest?khz:slowest;fastest=khz>fastest?khz:fastest;}
    if(slowest==fastest)return; // One cluster: nothing to avoid.
    cpu_set_t set;CPU_ZERO(&set);
    std::string chosen;
    for(std::size_t cpu=0;cpu<top.size();++cpu)
        if(top[cpu]>slowest) {CPU_SET(int(cpu),&set);chosen+=(chosen.empty()?"":",")+std::to_string(cpu);}
    const int result=sched_setaffinity(0,sizeof set,&set);
    std::fprintf(stderr,"Game thread on CPUs %s (efficiency cores at %ld MHz left out)%s\n",chosen.c_str(),slowest/1000,result?": refused":"");
#endif
}
