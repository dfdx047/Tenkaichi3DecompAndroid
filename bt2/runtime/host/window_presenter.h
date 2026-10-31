// Showing pictures in the viewer window on the GPU (gs/gpu/gpu_gs_backend.h) instead of with GDI:
// the renderer's picture goes to the window without being read back to the processor, and a stretch
// is filtered on the GPU (docs/plans/ee/PERFORMANCE_PLAN.md, presenting without readback).
#pragma once
#include "host/display_frame.h"

class WindowPresenter {
public:
    struct Rect {int x=0,y=0,width=0,height=0;};
    virtual ~WindowPresenter()=default;
    // Shows a picture in the window's client area (client_width x client_height), black around it:
    // `frame` (a picture drawn by the processor, such as a menu), or with nullptr the renderer's last
    // scanout. `smooth`: filtered scaling, else each pixel a block. `status` (may be empty) is drawn
    // one pixel for one at `status_at`'s corner.
    virtual void show(void* window,const DisplayFrame* frame,Rect at,unsigned client_width,unsigned client_height,bool smooth,
                      const DisplayFrame& status,Rect status_at)=0;
    // The window's surface is gone or replaced (Android's app in the background and back): the swapchain
    // is let go, and the next picture makes a new one.
    virtual void lost() {}
};
