#include "gs/hw/hw_gs_mirror.h"
#include "gs/cpu/gs_command_sink.h"
#include "host/sdl/sdl_viewer_window.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
int Bt2Hw_Start(void);
void Bt2Hw_Gif(const std::uint8_t* p,std::uint32_t qwc);
void Bt2Hw_FrameEnd(void);
void Bt2Hw_Field(void);
void Bt2Hw_SetLateUs(int us);
extern int gGsMainFbp;
void Bt2MenuItem_Overlay(std::uint64_t dispfb,std::uint64_t display); // (game/main_menu_item.cpp)
extern int gsScale4; // (gs/hw/bt3/gs_draw.c: the internal resolution in quarters)
}

namespace {
ViewerWindow* viewer_window=nullptr;

// "render_thread = 1" (BT2_RENDER_THREAD; the Vulkan back end only): the game's GIF packets are taken by a thread of
// their own (decoding, the vertices, each primitive's draw state: about 10 ms of a fight's frame) while the game
// thread goes on emulating; the game thread only copies each packet into the queue. At each flip the game thread
// waits for the queue to be empty and then ends the frame itself (the GPU's command buffer and the swapchain stay
// on the window's thread, as SDL requires). The two never use SDL's GPU at the same time: the worker can make
// textures and pipelines while it decodes, the game thread uses it only in the frame's end, with the worker idle.
// (OpenGL ES keeps everything on one thread: its context belongs to it.)
struct RenderQueue {
    std::mutex lock;
    std::condition_variable wake,idle;
    std::vector<std::uint8_t> pending,work; // packets: their size in quadwords (4 bytes), then the bytes
    bool busy=false;
    // Who is asleep: the worker on `wake` (nothing pending), the game thread on `idle` (a flip waiting for the queue to empty). A packet wakes the
    // worker only when it sleeps, and the worker tells the game thread only when it waits: thousands of packets a frame each made a futex call
    // to wake a thread that was already working (measured on Android: the unlock and the wake were about 2% of the samples of a fight).
    bool sleeping=false,draining=false;
    double waited_seconds=0; // the game thread's waits at the flips (statistics)
    void push(std::span<const std::uint8_t> data) {
        const std::uint32_t qwc=std::uint32_t(data.size()/16);
        const auto* size=reinterpret_cast<const std::uint8_t*>(&qwc);
        bool wake_worker;
        {
            std::lock_guard<std::mutex> hold(lock);
            // (appended, not resized and then copied over: resize wrote zeros into every byte the copy then wrote again)
            pending.insert(pending.end(),size,size+4);
            pending.insert(pending.end(),data.begin(),data.end());
            wake_worker=sleeping;
            sleeping=false;
        }
        if(wake_worker)wake.notify_one();
    }
    void run() {
        for(;;) {
            {
                std::unique_lock<std::mutex> hold(lock);
                busy=false;
                if(draining && pending.empty())idle.notify_all();
                while(pending.empty()) {sleeping=true;wake.wait(hold);}
                sleeping=false;
                pending.swap(work);
                busy=true;
            }
            const auto begin=std::chrono::steady_clock::now();
            for(std::size_t at=0;at+4<=work.size();) {
                std::uint32_t qwc;std::memcpy(&qwc,work.data()+at,4);
                Bt2Hw_Gif(work.data()+at+4,qwc);
                at+=4+std::size_t(qwc)*16;
            }
            work.clear();
            HwGs::gif_seconds.store(HwGs::gif_seconds.load()+std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count());
        }
    }
    void drain() {
        const auto begin=std::chrono::steady_clock::now();
        std::unique_lock<std::mutex> hold(lock);
        draining=true;
        idle.wait(hold,[&]{return pending.empty() && !busy;});
        draining=false;
        waited_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    }
};
RenderQueue* render_queue=nullptr; // (made once and never freed: the thread runs until the process ends)
}

// What the renderer's copy asks of BT2 (gs/hw/bt3/gs_draw.c, gs_gpu.c, gs_gl.c).
extern "C" SDL_Window* Bt2Hw_ExistingWindow(void) {return viewer_window?viewer_window->ensureWindow():nullptr;}
extern "C" void Bt2Hw_Event(const SDL_Event* event) {if(viewer_window && event)viewer_window->handle(*event);}

namespace HwGs {
std::atomic<double> gif_seconds{0};

bool wanted() {
    const char* renderer=std::getenv("BT2_RENDERER");
    return renderer && std::string_view(renderer)=="bt3";
}

bool attach(GsCommandSink& sink,ViewerWindow& viewer) {
    viewer_window=&viewer;
    setenv("BT3_GS","gpu",1); // (the copy's own switch: its GPU back end, in the window)
    if(!Bt2Hw_Start()) {
        std::fprintf(stderr,"BT3 renderer: no GPU device; drawing with paraLLEl-GS\n");
        viewer_window=nullptr;
        return false;
    }
    if(std::getenv("BT2_RENDER_THREAD") && !std::getenv("BT2_GL_WINDOW")) {
        render_queue=new RenderQueue;
        std::thread([]{render_queue->run();}).detach();
        sink.on_accepted=[](std::span<const std::uint8_t> data) {render_queue->push(data);};
        std::fprintf(stderr,"BT3 renderer: GIF packets taken by a thread of their own\n");
    } else if(std::getenv("BT3_GS_VERBOSE")) {
        // (each packet timed only with "gs_verbose" on: two clock reads a packet, thousands of packets a frame, were
        // most of the clock's 0.7% of the game thread for a number only the log shows)
        sink.on_accepted=[](std::span<const std::uint8_t> data) {
            const auto begin=std::chrono::steady_clock::now();
            Bt2Hw_Gif(data.data(),std::uint32_t(data.size()/16));
            gif_seconds.store(gif_seconds.load()+std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count());
        };
    } else {
        sink.on_accepted=[](std::span<const std::uint8_t> data) {Bt2Hw_Gif(data.data(),std::uint32_t(data.size()/16));};
    }
    std::fprintf(stderr,"BT3 renderer: drawing with the GPU's rasteriser\n");
    return true;
}

double scale() {return gsScale4*0.25;}
void field() {Bt2Hw_Field();}
void setLate(double seconds) {Bt2Hw_SetLateUs(int(seconds*1e6));}
double renderWaitSeconds() {return render_queue?render_queue->waited_seconds:0;}
bool renderThread() {return render_queue!=nullptr;}
void flip(const std::array<std::uint64_t,15>& privileged) {
    if(render_queue)render_queue->drain(); // (the frame's packets all taken before anything of the frame's end)
    // The read circuit that is on (PMODE EN2, else EN1) and the frame buffer it shows (DISPFB.FBP, in the same
    // 2048-word units as FRAME.FBP).
    const std::uint64_t pmode=privileged[0],dispfb=(pmode&2)?privileged[9]:privileged[7];
    if(pmode&3)gGsMainFbp=int(dispfb&0x1FF);
    if(pmode&3)Bt2MenuItem_Overlay(dispfb,(pmode&2)?privileged[10]:privileged[8]); // (BT2's own screens over the frame)
    Bt2Hw_FrameEnd();
}

void readFifo(std::span<std::uint8_t> out) {std::memset(out.data(),0,out.size());}
}
