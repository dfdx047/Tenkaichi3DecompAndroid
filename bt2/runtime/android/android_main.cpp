// libbt2.so's start on Android (Dragon Rage's Bt2GameActivity, android/app/.../engine/Bt2GameActivity.kt):
// SDL's activity calls SDL_main here with
//   argv[1]  the game's folder (<app files>/bt2: data/BT2.iso, saves/, startup/, user/)
//   argv[2]  the app's native library folder
//   argv[3], argv[4]  a custom GPU driver's folder and file ("" for none), loaded through libadrenotools
// and this does what the PC's game program does before it starts the runtime (runtime/game/game_main.cpp):
// the folders, the settings (the disc image and a memory card chosen when they name none), the startup
// inputs in the runtime's argument order, and the environment of a player's run. The runtime's main
// (probe/entry_probe.cpp, built as bt2_entry_main) then runs the game in this process. Its messages go to
// the log (adb logcat -s bt2) and to user/last-run.log. The startup inputs are made here from the
// player's disc image (game/startup_inputs.h: no capture, no BIOS), again whenever the image changes.
//
// The on-screen controller reaches the game through TouchBridge's native functions (host/sdl/touch_pad.h);
// leaving the game (back twice, host/sdl/sdl_player_session.h) brings the app's launcher forward.
#include "game/startup_inputs.h"
#include "gs/gpu/gpu_gs_backend.h"
#include "gs/gpu/gpu_window_presenter.h"
#include "host/game_folders.h"
#include "host/player_session.h"
#include "host/sdl/android_performance.h"
#include "host/sdl/touch_pad.h"
#include <SDL3/SDL.h>
#include <adrenotools/driver.h>
#include <dlfcn.h>
#include <android/log.h>
#include <jni.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unwind.h>
#include <ucontext.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

int bt2_entry_main(int argc,char** argv); // probe/entry_probe.cpp's main (built with -Dmain=bt2_entry_main)

namespace {
namespace fs=std::filesystem;
constexpr const char* tag="bt2";

// The startup inputs for `disc` in `folder`: made when missing or when the disc image (its size and time)
// or the way they are made changed.
void prepareStartupInputs(const fs::path& folder,const fs::path& disc) {
    std::error_code error;
    const auto stamp=folder/".made";
    // (The time as whole seconds: libc++'s file clock counts in a 128-bit type on Android.)
    const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(fs::last_write_time(disc,error).time_since_epoch()).count();
    const std::string made="1 "+std::to_string(static_cast<unsigned long long>(fs::file_size(disc,error)))+" "+std::to_string(static_cast<long long>(seconds));
    {
        std::ifstream in(stamp);std::string line;
        if(std::getline(in,line) && line==made) {
            bool complete=true;
            for(const char* name:startup_input_names)complete=complete && fs::is_regular_file(folder/name,error);
            if(complete)return;
        }
    }
    std::fprintf(stderr,"Making the startup inputs from %s\n",disc.c_str());
    fs::remove_all(folder,error);
    writeStartupInputs(folder,readGameProgram(disc));
    std::ofstream(stamp)<<made<<"\n";
}

// stdout and stderr, line by line, to the log and to a file the player can send.
FILE* log_file=nullptr;int log_pipe[2];
std::atomic<long long> last_output_ms{0};pid_t log_tid=0;
long long nowMs() {timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000LL+t.tv_nsec/1000000;}
void* logThread(void*) {
    log_tid=gettid();
    char buffer[2048];std::size_t used=0;
    for(;;) {
        const ssize_t n=read(log_pipe[0],buffer+used,sizeof buffer-1-used);
        if(n<=0)break;
        used+=std::size_t(n);
        for(;;) {
            char* newline=static_cast<char*>(std::memchr(buffer,'\n',used));
            const std::size_t length=newline?std::size_t(newline-buffer):used==sizeof buffer-1?used:0;
            if(!length && !newline)break;
            buffer[length]='\0';
            __android_log_write(ANDROID_LOG_INFO,tag,buffer);
            if(log_file) {std::fprintf(log_file,"%s\n",buffer);std::fflush(log_file);}
            last_output_ms=nowMs();
            const std::size_t taken=length+(newline?1:0);
            std::memmove(buffer,buffer+taken,used-taken);used-=taken;
        }
    }
    return nullptr;
}
// A stall's evidence, which a phone without root gives no other way: when the log has been quiet for 15
// seconds (the viewer's statistics come every two seconds or so while the game runs), each thread writes
// where it is, as library+offset pairs (llvm-symbolizer with the unstripped libbt2.so names them).
struct Trace {std::uintptr_t pcs[40];int count=0;};
_Unwind_Reason_Code traceStep(_Unwind_Context* context,void* argument) {
    auto& trace=*static_cast<Trace*>(argument);
    if(trace.count>=40)return _URC_END_OF_STACK;
    if(const auto pc=_Unwind_GetIP(context))trace.pcs[trace.count++]=pc;
    return _URC_NO_REASON;
}
void traceThisThread(int,siginfo_t*,void*) {
    Trace trace;_Unwind_Backtrace(traceStep,&trace);
    char name[32]="";
    {char path[64];std::snprintf(path,sizeof path,"/proc/self/task/%d/comm",gettid());
     const int fd=open(path,O_RDONLY);
     if(fd>=0) {const ssize_t n=read(fd,name,sizeof name-1);if(n>0)name[n-1]='\0';close(fd);}}
    char line[3072];int at=std::snprintf(line,sizeof line,"Stall: thread %d (%s):",gettid(),name);
    for(int n=0;n<trace.count && at<int(sizeof line)-96;++n) {
        Dl_info info{};
        if(dladdr(reinterpret_cast<void*>(trace.pcs[n]),&info) && info.dli_fname) {
            const char* file=std::strrchr(info.dli_fname,'/');
            at+=std::snprintf(line+at,sizeof line-at," %s+0x%zx",file?file+1:info.dli_fname,std::size_t(trace.pcs[n]-reinterpret_cast<std::uintptr_t>(info.dli_fbase)));
        } else at+=std::snprintf(line+at,sizeof line-at," 0x%zx",std::size_t(trace.pcs[n]));
    }
    line[at++]='\n';
    (void)!write(2,line,std::size_t(at));
}
void* stallWatch(void*) {
    bool traced=false;
    for(;;) {
        sleep(5);
        // (the in-game menu's pause is no stall)
        if(PlayerSession::paused || nowMs()-last_output_ms<15000) {traced=false;if(PlayerSession::paused)last_output_ms=nowMs();continue;}
        if(traced)continue;
        traced=true;
        std::fprintf(stderr,"Stall: nothing from the game for 15 seconds; where each thread is:\n");
        DIR* tasks=opendir("/proc/self/task");
        if(!tasks)continue;
        while(const dirent* task=readdir(tasks)) {
            const int tid=std::atoi(task->d_name);
            if(tid<=0 || tid==gettid() || tid==log_tid)continue;
            syscall(SYS_tgkill,getpid(),tid,SIGURG);
            usleep(20000);
        }
        closedir(tasks);
    }
}
// A sampling profiler for the game's thread ("sample = 1" in user/android): 500 times a second the thread
// notes where it is; every ten seconds the most frequent places go to the log as libbt2.so offsets, which
// llvm-symbolizer names with the unstripped library. simpleperf needs settings some phones refuse.
std::atomic<pid_t> game_tid{0};
constexpr std::size_t sample_ring=1<<15;
// Each sample: where the game thread was, and where that code returns to (for time spent in the system's libraries:
// which of ours called the sleep, the driver, the copy).
std::uintptr_t samples[sample_ring],sample_callers[sample_ring];std::atomic<std::size_t> sample_count{0};
void takeSample(int,siginfo_t*,void* context) {
    const auto pc=std::uintptr_t(static_cast<ucontext_t*>(context)->uc_mcontext.pc);
    const std::size_t at=sample_count.fetch_add(1,std::memory_order_relaxed)%sample_ring;
    samples[at]=pc;sample_callers[at]=std::uintptr_t(static_cast<ucontext_t*>(context)->uc_mcontext.regs[30]);
}
void* sampler(void*) {
    Dl_info self{};dladdr(reinterpret_cast<void*>(&takeSample),&self);
    const auto base=reinterpret_cast<std::uintptr_t>(self.dli_fbase);
    std::size_t read=0;
    for(;;) {
        std::vector<std::uintptr_t> taken;
        for(unsigned n=0;n<5000;++n) {
            if(const pid_t tid=game_tid)syscall(SYS_tgkill,getpid(),tid,SIGPROF);
            usleep(2000);
        }
        const std::size_t end=sample_count;
        std::vector<std::uintptr_t> callers; // (of the samples outside libbt2.so)
        for(;read<end;++read) {
            taken.push_back(samples[read%sample_ring]);
            Dl_info where{};
            if(dladdr(reinterpret_cast<void*>(samples[read%sample_ring]),&where) && where.dli_fbase!=self.dli_fbase)callers.push_back(sample_callers[read%sample_ring]);
        }
        if(taken.empty())continue;
        std::vector<std::pair<std::uintptr_t,unsigned>> counts;
        std::sort(taken.begin(),taken.end());
        for(std::size_t n=0;n<taken.size();) {std::size_t m=n;while(m<taken.size() && taken[m]==taken[n])++m;counts.push_back({taken[n],unsigned(m-n)});n=m;}
        std::sort(counts.begin(),counts.end(),[](auto& a,auto& b){return a.second>b.second;});
        std::string line="Samples ("+std::to_string(taken.size())+"):";
        for(std::size_t n=0;n<counts.size() && n<400;++n) {
            char part[64];const auto pc=counts[n].first;
            Dl_info info{};
            if(dladdr(reinterpret_cast<void*>(pc),&info) && info.dli_fbase!=self.dli_fbase) {
                const char* file=info.dli_fname?std::strrchr(info.dli_fname,'/'):nullptr;
                std::snprintf(part,sizeof part," %s+0x%zx=%u",file?file+1:"?",std::size_t(pc-reinterpret_cast<std::uintptr_t>(info.dli_fbase)),counts[n].second);
            } else std::snprintf(part,sizeof part," 0x%zx=%u",std::size_t(pc-base),counts[n].second);
            line+=part;
        }
        std::fprintf(stderr,"%s\n",line.c_str());
        // Who called into the system's libraries (the return address of those samples, as offsets in libbt2.so)
        std::sort(callers.begin(),callers.end());
        std::vector<std::pair<std::uintptr_t,unsigned>> by;
        for(std::size_t n=0;n<callers.size();) {std::size_t m=n;while(m<callers.size() && callers[m]==callers[n])++m;by.push_back({callers[n],unsigned(m-n)});n=m;}
        std::sort(by.begin(),by.end(),[](auto& a,auto& b){return a.second>b.second;});
        line="Callers ("+std::to_string(callers.size())+"):";
        for(std::size_t n=0;n<by.size() && n<40;++n) {
            char part[64];Dl_info info{};
            if(dladdr(reinterpret_cast<void*>(by[n].first),&info) && info.dli_fbase!=self.dli_fbase) {
                const char* file=info.dli_fname?std::strrchr(info.dli_fname,'/'):nullptr;
                std::snprintf(part,sizeof part," %s+0x%zx=%u",file?file+1:"?",std::size_t(by[n].first-reinterpret_cast<std::uintptr_t>(info.dli_fbase)),by[n].second);
            } else std::snprintf(part,sizeof part," 0x%zx=%u",std::size_t(by[n].first-base),by[n].second);
            line+=part;
        }
        std::fprintf(stderr,"%s\n",line.c_str());
    }
}
void startSampler() {
    struct sigaction action{};
    action.sa_sigaction=takeSample;action.sa_flags=SA_SIGINFO|SA_RESTART;sigemptyset(&action.sa_mask);
    sigaction(SIGPROF,&action,nullptr);
    pthread_t thread;pthread_create(&thread,nullptr,sampler,nullptr);pthread_detach(thread);
}
void startStallWatch() {
    struct sigaction action{};
    action.sa_sigaction=traceThisThread;action.sa_flags=SA_SIGINFO|SA_RESTART;sigemptyset(&action.sa_mask);
    sigaction(SIGURG,&action,nullptr);
    last_output_ms=nowMs();
    pthread_t thread;pthread_create(&thread,nullptr,stallWatch,nullptr);pthread_detach(thread);
}
void redirectOutput(const fs::path& log) {
    log_file=std::fopen(log.c_str(),"w");
    if(pipe(log_pipe))return;
    setvbuf(stdout,nullptr,_IOLBF,0);setvbuf(stderr,nullptr,_IONBF,0);
    dup2(log_pipe[1],1);dup2(log_pipe[1],2);
    pthread_t thread;pthread_create(&thread,nullptr,logThread,nullptr);pthread_detach(thread);
    startStallWatch();
}

// The activity's quitToApp (GameActivity.kt): the launcher comes forward and this process ends shortly after.
void quitToApp() {
    auto* env=static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto activity=static_cast<jobject>(SDL_GetAndroidActivity());
    if(env && activity) {
        if(jclass type=env->GetObjectClass(activity)) {
            if(jmethodID method=env->GetMethodID(type,"quitToApp","()V"))env->CallVoidMethod(activity,method);
            env->DeleteLocalRef(type);
        }
        if(env->ExceptionCheck())env->ExceptionClear();
        env->DeleteLocalRef(activity);
    }
    for(;;)std::this_thread::sleep_for(std::chrono::seconds(1)); // The activity ends the process.
}

// A custom Vulkan driver chosen in the app (GpuDrivers.kt; BT3's loader.c does the same): loaded through
// libadrenotools with its hooks from the app's native library folder; the renderer then uses its loader.
// When it cannot be loaded, the system's driver is used.
void customDriver(const std::string& libdir,const std::string& dir,const std::string& file) {
    void* handle=adrenotools_open_libvulkan(RTLD_NOW,ADRENOTOOLS_DRIVER_CUSTOM,nullptr,(libdir+"/").c_str(),(dir+"/").c_str(),file.c_str(),nullptr,nullptr);
    void* loader=handle?dlsym(handle,"vkGetInstanceProcAddr"):nullptr;
    if(!loader) {std::fprintf(stderr,"Custom GPU driver %s/%s could not be loaded: the system's driver is used\n",dir.c_str(),file.c_str());return;}
    GpuGsBackend::setVulkanLoader(loader);
    std::fprintf(stderr,"Custom GPU driver: %s/%s\n",dir.c_str(),file.c_str());
}

void setEnv(const char* name,const std::string& value) {if(value.empty())unsetenv(name);else setenv(name,value.c_str(),1);}

// A message for the player, then back to the launcher.
int stop(const std::string& text) {
    std::fprintf(stderr,"%s\n",text.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Budokai Tenkaichi 2",text.c_str(),nullptr);
    quitToApp();
    return 1;
}
}

extern "C" __attribute__((visibility("default"))) int SDL_main(int argc,char* argv[]) {
    const fs::path root=argc>1?fs::path(argv[1]):fs::current_path();
    const std::string libdir=argc>2?argv[2]:"";
    const fs::path user=root/"user",inputs=root/"startup",settings_path=user/"settings";
    std::error_code error;
    fs::create_directories(user,error);
    redirectOutput(user/"last-run.log");
    if(chdir(root.c_str()))std::fprintf(stderr,"Cannot enter %s\n",root.c_str());
    if(argc>4 && *argv[3] && *argv[4])customDriver(libdir,argv[3],argv[4]);
    try {
        const GameFolders folders(root);
        folders.create();
        Settings settings;
        if(auto stored=Settings::load(settings_path.string()))settings=*stored;
        // The disc image the app copied (data/BT2.iso), and a memory card, when the settings name none.
        if(!folders.configured(settings)) {
            const auto discs=folders.discImages();
            if(discs.empty())return stop("No Budokai Tenkaichi 2 disc image in "+utf8(folders.data)+". Choose your ISO in the app.");
            settings.disc=discs.front();
        }
        if(folders.card(settings).empty()) {
            const auto cards=folders.cards();
            settings.memory_card=cards.empty()?folders.newCard():cards.front();
        }
        settings.save(settings_path.string());

        // MEMORY_IMAGE STOP_REPORT inputs... with the disc before the last input, as game_main.cpp builds it.
        prepareStartupInputs(inputs,folders.disc(settings));
        const std::size_t count=startup_input_names.size();
        std::vector<std::string> args{"bt2",(inputs/startup_input_names[0]).string(),(user/"last-run.json").string()};
        for(std::size_t n=1;n<count;++n) {
            if(n==count-1)args.push_back(folders.disc(settings).string());
            args.push_back((inputs/startup_input_names[n]).string());
        }

        // A player's run (game_main.cpp: runGame).
        for(const char* name:{"BT2_VIEWER","BT2_AUDIO_DEVICE","BT2_HOST_INPUT"})setenv(name,"1",1);
        setenv("BT2_GPU_GS","on",1);
        setenv("BT2_SELF_STARTUP","1",1); // The inputs above are made, not captured (game/startup_inputs.h).
        setEnv("BT2_PLAYER_SETTINGS",settings_path.string());
        setEnv("BT2_HD_TEXTURES",settings.hd_textures?folders.texturePack().string():"");
        setEnv("BT2_SUPER_SAMPLING",std::to_string(settings.super_sampling));
        setEnv("BT2_GAME_PATCHES",settings.widescreen?"widescreen":"");
        setEnv("BT2_SIXTY_FPS",settings.sixty_fps?"1":"");
        setEnv("BT2_CARD_FOLDER",folders.card(settings).string());
        // An online match (plugin/online/fight_session.h; the lobby wrote online_session): both games start from the same memory card, a fresh copy of the
        // 100% save the app made (user/online-card), with the same game patch and speed whatever each player has chosen, so that the two simulate the same
        // thing. The player's own card is left alone.
        {
            std::error_code match_error;
            if(fs::exists(user/"online_session",match_error) && fs::is_directory(user/"online-card",match_error)) {
                setEnv("BT2_CARD_FOLDER",(user/"online-card").string());
                setEnv("BT2_SIXTY_FPS","");
                setEnv("BT2_GAME_PATCHES","widescreen");
                std::fprintf(stderr,"Online match: the memory card is user/online-card; 60 fps off, the widescreen patch on\n");
            }
        }
        setEnv("BT2_SHADER_CACHE",(user/"shader-cache.bin").string()); // gs/gpu/gpu_gs_backend.cpp
        // Every 120 flips, the rate and where a frame's time goes (probe/entry_probe.cpp): in the log a player
        // sends, to tell a slow phone's CPU from its GPU.
        setenv("BT2_VIEWER_STATS","1",1);
        for(const char* name:{"BT2_FRAME_LIMIT","BT2_PAD_SCRIPT","BT2_PAD_RECORD","BT2_VIEWER_SCALE","BT2_GPU_GS_SNAPSHOT"})unsetenv(name);
        // Android-only options the app writes beside the engine's settings (Dragon Rage's Bt2Settings.kt), which
        // the settings file cannot hold: "gs_hacks = <list>", paraLLEl-GS's speed-for-accuracy options.
        {
            std::ifstream in(user/"android");std::string line,hacks;
            bool sample=false,render_thread=true;std::string renderer,sync_run;
            while(std::getline(in,line)) {
                if(line.rfind("gs_hacks = ",0)==0)hacks=line.substr(11);
                if(line=="sample = 1")sample=true;
                if(line.rfind("present_mode = ",0)==0)GpuWindowPresenter::present_mode=std::atoi(line.c_str()+15);
                // "renderer = bt3": BT3's renderer (gs/hw/hw_gs_mirror.h) with its own settings file, "renderer"
                if(line=="renderer = bt3")renderer="bt3";
                if(line=="gs_verbose = 1")setenv("BT3_GS_VERBOSE","1",1); // (BT3's renderer's own timing in the log)
                // "perf_baseline = 1": the frame pacer and the palette loads as before their fixes, to measure against
                if(line=="perf_baseline = 1")setenv("BT2_PERF_BASELINE","1",1);
                // "state_memo = 0": every primitive's draw state worked out again (gs/hw/bt3/gs_draw.c), to measure against
                if(line=="state_memo = 0")setenv("BT2_STATE_MEMO_OFF","1",1);
                // "lean_passes = 0": every layer of every render target loaded and stored (gs/hw/bt3/gs_gpu.c), to measure against
                if(line=="lean_passes = 0")setenv("BT2_LEAN_PASSES_OFF","1",1);
                // "render_thread = 0": the GIF packets taken on the game's thread, as before (by default they have a thread of
                // their own, gs/hw/hw_gs_mirror.cpp; Vulkan only: measured 27 -> 30 fps and 90 -> 100% speed in a fight at 1.5x)
                if(line=="render_thread = 0")render_thread=false;
                // "pad_script = <steps>": the buttons played by field from the start (pad_script.h), over the touch
                // controls: the same fight for every measurement (the gate's tools/gate/pad-fight.txt is one)
                if(line.rfind("pad_script = ",0)==0) {setenv("BT2_PAD_SCRIPT",line.c_str()+13,1);setenv("BT2_PAD_SCRIPT_OVER_LIVE","1",1);}
                // "vu1_census = 1": every VU1 program the run starts goes to user/vu1-census, to translate to native
                // code on a PC (tools/vu1/translate_programs.py; the result is the game's, it stays local)
                if(line=="vu1_census = 1") {fs::create_directories(user/"vu1-census",error);setenv("BT2_VU1_CENSUS",(user/"vu1-census").c_str(),1);}
                // "sync_test = <run>": the repeatability check of online play (docs/plans/online/ONLINE_PLAN.md), for developers: run 1 is
                // played by the player and its controller recorded (user/sync-test/input.bin); run 2 plays that recording by itself;
                // both from a copy of the same memory card, with the game's memories hashed every 10 fields (user/sync-test/run-<run>.txt)
                if(line.rfind("sync_test = ",0)==0 && line.size()>12)sync_run=line.substr(12);
            }
            for(const char* name:{"BT2_STATE_HASH","BT2_STATE_HASH_EVERY","BT2_STATE_HASH_PAGES","BT2_INPUT_RECORD","BT2_INPUT_REPLAY"})unsetenv(name);
            if(!sync_run.empty()) {
                const auto dir=user/"sync-test",start=dir/"card-start",card=dir/"card";
                fs::create_directories(dir,error);
                if(sync_run=="1") {fs::remove_all(start,error);fs::copy(folders.card(settings),start,fs::copy_options::recursive,error);}
                fs::remove_all(card,error);fs::copy(start,card,fs::copy_options::recursive,error);
                setEnv("BT2_CARD_FOLDER",card.string());
                setEnv("BT2_STATE_HASH",(dir/("run-"+sync_run+".txt")).string());
                setenv("BT2_STATE_HASH_EVERY","10",1);
                setenv("BT2_STATE_HASH_PAGES","6000",1);
                setEnv(sync_run=="1"?"BT2_INPUT_RECORD":"BT2_INPUT_REPLAY",(dir/"input.bin").string());
                std::fprintf(stderr,"Sync test: run %s\n",sync_run.c_str());
            }
            setEnv("BT2_GS_HACKS",hacks);
            setEnv("BT2_RENDERER",renderer);
            setEnv("BT2_RENDER_THREAD",render_thread?"1":"");
            // BT3's renderer's OpenGL ES back end (the app offers it for Mali GPUs: "gpu_api = 1" in the renderer's
            // settings, gs/hw/bt3/gs_draw.c): the window has to be made for OpenGL ES (host/sdl/sdl_viewer_window.h).
            {
                bool gl=false;std::ifstream settings(user/"renderer");std::string setting;
                while(std::getline(settings,setting))if(setting=="gpu_api = 1")gl=true;
                setEnv("BT2_GL_WINDOW",renderer=="bt3" && gl?"1":"");
            }
            setEnv("BT2_HW_SETTINGS",renderer.empty()?"":(user/"renderer").string());
            // The main menu's Online Mode item (game/main_menu_item.h), with BT3's renderer: its pictures in user/menu-item.
            setenv("BT2_ONLINE_DIR",user.c_str(),1); // (the app and the Online Mode screens talk through files here: game/main_menu_item.cpp)
            setEnv("BT2_MAIN_MENU_ITEM",renderer=="bt3" && fs::is_directory(user/"menu-item",error)?(user/"menu-item").string():"");
            setEnv("BT2_PLUGIN_DIR",renderer=="bt3" && fs::is_directory(user/"menu-item",error)?libdir:""); // (Dragon Rage's library, in the app's native libraries: plugin/plugin_host.cpp)
            if(sample) {startSampler();setenv("BT2_GPU_TIMESTAMPS","1",1);} // And the GPU's time by stage.
        }

        PlayerSession::quit_to_launcher=quitToApp;
        std::vector<char*> pointers;
        for(auto& arg:args)pointers.push_back(arg.data());
        pointers.push_back(nullptr);
        std::fprintf(stderr,"Starting Budokai Tenkaichi 2 from %s\n",root.c_str());
        pinToFastCores(); // The game runs on this thread (host/sdl/android_performance.h).
        game_tid=gettid();
        const int code=bt2_entry_main(int(args.size()),pointers.data());
        std::fprintf(stderr,"The game ended (%d).\n",code);
        if(code)return stop("The game stopped. The messages are in "+utf8(user/"last-run.log")+".");
        quitToApp();
        return 0;
    } catch(const std::exception& e) {
        return stop(e.what());
    }
}

// ---- The on-screen controller (com.dfdx047.dragonrage.engine.TouchBridge).
extern "C" JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativeSetPad(JNIEnv*,jclass,jint buttons,jint lx,jint ly,jint rx,jint ry) {
    TouchPad::set(unsigned(buttons),lx,ly,rx,ry);
}
// The game's own menu (BT3's) is never open here: the controller always stays.
extern "C" JNIEXPORT jboolean JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativeMenuOpen(JNIEnv*,jclass) {return JNI_FALSE;}
// No battle roster is read from BT2 yet: unknown.
// The in-game menu (Bt2GameActivity): the game stands still while it is open, and what it saved in the settings
// file (volumes, scaling) is read again.
extern "C" JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_Bt2Bridge_nativeSetPaused(JNIEnv*,jclass,jboolean paused) {
    PlayerSession::paused=paused==JNI_TRUE;
}
extern "C" void Bt2Hw_RequestReload(void); // gs/hw/bt3_host.c: the BT3 renderer's settings, between two frames
extern "C" JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_Bt2Bridge_nativeReloadSettings(JNIEnv*,jclass) {++PlayerSession::reload;Bt2Hw_RequestReload();}
extern "C" JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_Bt2Bridge_nativeSetPresentMode(JNIEnv*,jclass,jint mode) {
    GpuWindowPresenter::present_mode=mode;
}
// The in-game menu's speed-for-accuracy options (Bt2GameActivity), from the renderers' next frame.
extern "C" JNIEXPORT void JNICALL Java_com_dfdx047_dragonrage_engine_Bt2Bridge_nativeSetGsHacks(JNIEnv* env,jclass,jstring list) {
    const char* text=env->GetStringUTFChars(list,nullptr);
    try {GpuGsBackend::requestHacks(text);} catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());}
    env->ReleaseStringUTFChars(list,text);
}
extern "C" JNIEXPORT jint JNICALL Java_com_dfdx047_dragonrage_engine_TouchBridge_nativePlayerTeamSize(JNIEnv*,jclass) {return -1;}
// The app's performance panel (Bt2PerfOverlay): pictures shown and fields run so far, and the game thread's id (its
// CPU time is read from /proc by the app).
extern "C" int Bt2MenuItem_OnlineState(); // plugin/plugin_host.cpp: the library's, or 0
extern "C" JNIEXPORT jlongArray JNICALL Java_com_dfdx047_dragonrage_engine_Bt2Bridge_nativeStats(JNIEnv* env,jclass) {
    // (a fourth count since the Online Mode screen: which of its pages is open, 0 none; an older app reads three)
    const jlong values[4]={jlong(PlayerSession::pictures_total.load()),jlong(PlayerSession::fields_total.load()),jlong(game_tid.load()),jlong(Bt2MenuItem_OnlineState())};
    jlongArray out=env->NewLongArray(4);
    if(out)env->SetLongArrayRegion(out,0,4,values);
    return out;
}
