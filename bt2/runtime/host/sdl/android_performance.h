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
#include <atomic>
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

// The thread that takes the renderer's GIF packets ("render_thread", gs/hw/hw_gs_mirror.cpp), once it runs: its work
// is on the frame's critical path (the game thread waits for it at every flip), so it joins the game thread's hint
// session. 0: none (or "render_boost = 0" in user/android).
inline std::atomic<int> bt2_render_tid{0};

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
        set_threads_=reinterpret_cast<int(*)(void*,const std::int32_t*,std::size_t)>(dlsym(android,"APerformanceHint_setThreads")); // (Android 14)
        if(!manager || !create || !report_ || !close_)return; // Android before 13.
        void* hints=manager();
        threads_[0]=std::int32_t(gettid());
        added_=bt2_render_tid.load();
        if(added_)threads_[1]=added_;
        session_=hints?create(hints,threads_,added_?2:1,target_ns):nullptr;
        if(!session_ && added_ && hints) { // (the second thread refused: the game's alone, as before)
            session_=create(hints,threads_,1,target_ns);
            added_=-1;
        }
        std::fprintf(stderr,"Performance hints: %s\n",session_?(added_>0?"on (game thread and render thread)":"on (game thread)"):"not available");
    }
    void report(std::int64_t actual_ns) {
        if(!session_)return;
        // The render thread started after the session was made: added where the system allows it (Android 14 and later).
        if(!added_ && set_threads_) {
            if(const int tid=bt2_render_tid.load()) {
                threads_[1]=tid;
                added_=set_threads_(session_,threads_,2)==0?tid:-1;
                std::fprintf(stderr,"Performance hints: render thread %s\n",added_>0?"added":"not added");
            }
        }
        if(actual_ns>0)report_(session_,actual_ns);
    }
private:
    void* session_=nullptr;bool tried_=false;
    std::int32_t threads_[2]={};int added_=0; // the render thread's id once in the session; -1 refused
    int (*report_)(void*,std::int64_t)=nullptr;void (*close_)(void*)=nullptr;
    int (*set_threads_)(void*,const std::int32_t*,std::size_t)=nullptr;
#else
    void start(std::int64_t) {}
    void report(std::int64_t) {}
#endif
};

inline void pinToFastCores(const char* who="Game thread") {
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
    std::fprintf(stderr,"%s on CPUs %s (efficiency cores at %ld MHz left out)%s\n",who,chosen.c_str(),slowest/1000,result?": refused":"");
#endif
}
