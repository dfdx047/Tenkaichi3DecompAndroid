// 60 fps battles (docs/plans/graphics/HIGH_FRAME_RATE_PLAN.md, way 4): a picture between each two fight
// frames, drawn from the game's own commands with the camera and the bones halfway. The game keeps
// stepping 30 times a second exactly as before; this only draws and never touches guest state.
//
// Each frame's GS input is recorded in order: VIF1 transfers (with the GS data their run produced) and
// all other GIF data. A second renderer on the main one's GPU device follows the game one frame behind:
// at each flip it draws the in-between picture of the last two frames (the newer frame's input with its
// VIF1 lists blended against the older frame's, run on a copy of the VIF1/VU1 state at the newer frame's
// start, and from the main renderer's GS state on the GPU as it was then, copied at each flip), and
// scans it out. A frame without an in-between picture (not a fight, or lists that do not align) is
// replayed instead, so the second renderer still takes every register the game writes. The frame recorded up to flip F is on screen at flip F + 1 (checked pixel for pixel), so
// the picture between F - 1 and F can be shown a field after flip F, before F's own: no delay is added.
// The drawing runs on a worker thread, from the end of one flip's work to the field after it; the game's
// thread hands it a frame (start) and waits for it only where it needs the result or the device (wait).
#pragma once
#include "gs/cpu/gs_command_sink.h"
#include "gs/gpu/gpu_gs_mirror.h"
#include "gs/textures/gs_texture_names.h"
#include "vu/vif1.h"
#include <array>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <memory>
#include <span>
#include <vector>

class InBetweenFrames {
public:
    // `vif1` is the VIF1 state now (the first frame's start). With HD textures the second renderer names
    // textures itself and draws with the primary's pack.
    InBetweenFrames(GpuGsMirror& primary,const Vif1& vif1,unsigned super_sampling,bool hd_textures);
    ~InBetweenFrames();

    // Recording, from the runtime's hooks: a VIF1 transfer before it runs, and each GS input (`from_vif1`:
    // produced by the VIF1 transfer that runs now).
    void vif1Transfer(std::span<const std::uint32_t> words);
    void gsInput(std::span<const std::uint8_t> data,bool from_vif1);

    // After a display flip's own work (the main renderer's flip and picture): the frame recorded since
    // the last flip is complete; `fight` says it was a fight frame (it waited for the fight's 30 fps step).
    // `vif1` is the VIF1 state as the next frame starts; the main renderer's state as the newer frame
    // started is in its keepState slot `older_slot`. The worker draws the in-between picture of the last
    // two frames when both were fight frames whose lists pair, scanning it out with `privileged` (display
    // registers that show the newer frame's buffer); otherwise it replays the newer frame.
    void start(bool fight,const Vif1& vif1,const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field,unsigned older_slot);
    // Waits for the worker's frame (rethrowing its failure); returns whether an in-between picture is
    // ready. Called before the main renderer's flip (the shared device and HD pack), before showing or
    // reading the second renderer's picture, and harmless when nothing is running.
    bool wait();
    // Whether the worker's frame is done (without waiting) and its in-between picture is ready.
    bool ready();
    bool busy(); // Whether the worker is still drawing (without waiting).
    std::pair<unsigned,unsigned> pictureSize() const {return picture_;}
    GpuGsMirror& renderer() {return mirror_;}
    // For checks: when set, the frames are only replayed (no in-between pictures) and each replay is
    // scanned out, so the second renderer's picture can be compared with the main one's.
    bool replay_only=false;
    // For checks: the "in-between" picture is drawn all the way to the newer frame (blend 1), so it must
    // equal the main renderer's picture of that frame.
    bool scan_replay=false;
    unsigned drawn=0,unpaired=0,not_fights=0,shown=0,camera_cuts=0,late=0; // shown and late: counted by the caller.
    double waited_seconds=0; // The game's thread waiting for the worker.

private:
    // An event's GS data: the transfers one after another, `ends` where each ends.
    struct Event {
        bool vif1=false;std::vector<std::uint32_t> words;std::vector<std::uint8_t> output;std::vector<std::size_t> ends;
        template<class Each> void eachOutput(Each each) const {
            std::size_t from=0;
            for(const auto end:ends) {each(std::span<const std::uint8_t>(output.data()+from,end-from));from=end;}
        }
    };
    // Frames are reused, so their buffers keep their size from frame to frame: `used` events are this frame's.
    struct Frame {std::vector<Event> events;std::size_t used=0;std::unique_ptr<Vif1> start;bool fight=false;Event& next(bool vif1);};
    Frame recording_,previous_,spare_;
    // The worker's job: the finished frame and how to scan it out.
    struct Job {Frame frame;std::array<std::uint64_t,15> privileged{};unsigned crt_mode=0,field=0,older_slot=0;};
    Job job_;bool pending_=false,stop_=false,ready_=false;std::exception_ptr failure_;
    std::mutex lock_;std::condition_variable changed_;
    std::thread worker_; // Last: it starts in the constructor and uses the members above.
    void run();
    bool draw(Frame finished,const Job& job);
    GpuGsMirror& primary_;
    GsCommandSink sink_;
    GsTextureNames names_;
    GpuGsMirror mirror_;
    std::pair<unsigned,unsigned> picture_{0,0};
    void send(std::span<const std::uint8_t> data);
    bool drawInBetween(const Frame& older,const Frame& newer,unsigned older_slot);
};
