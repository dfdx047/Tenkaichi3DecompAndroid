// The presentation window through SDL3 (Android, Linux), with the Win32 window's interface
// (host/viewer_window.h). Pictures are shown only on the GPU (host/window_presenter.h): paraLLEl-GS is
// the port's only renderer, and the presenter makes its swapchain on this window's Vulkan surface. On a
// phone the window is the whole screen; layout and scale settings only choose how the picture fits.
//
// Android's lifecycle: when the app goes to the background SDL's event pump blocks inside pump() until
// it comes back (SDL_HINT_ANDROID_BLOCK_ON_PAUSE), so the game stands still meanwhile; the window's
// surface goes away with it, so the presenter lets its swapchain go first and makes a new one at the
// next picture. The back key (Android's, or the on-screen controller's menu button) is kept for the
// player's session (backPressed).
#pragma once
#include <cstdlib>
#include "host/display_frame.h"
#include "host/window_presenter.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

class ViewerWindow {
public:
    enum class Scaling {Integer,Stretch};
    explicit ViewerWindow(Scaling scaling=Scaling::Integer):scaling_(scaling) {}
    ~ViewerWindow() {if(window_)SDL_DestroyWindow(window_);}
    ViewerWindow(const ViewerWindow&)=delete;
    ViewerWindow& operator=(const ViewerWindow&)=delete;

    void present(const DisplayFrame& frame,unsigned flip) {
        present(frame,"BT2 Recomp - frame "+std::to_string(flip)+(frame.unwritten?" (unwritten pixels)":""));
    }
    void present(const DisplayFrame& frame,const std::string& title) {
        if(closed_)return;
        if(!window_)create();
        setTitle(title);
        if(presenter_)showOnGpu(&frame,frame.width,frame.height);
        pump();
    }
    // The renderer's last scanout (width x height), shown by the presenter.
    void presentRenderer(unsigned width,unsigned height,unsigned flip) {
        if(closed_)return;
        if(!presenter_)throw std::runtime_error("Viewer: the renderer's picture needs a GPU presenter");
        if(!window_)create();
        setTitle("BT2 Recomp - frame "+std::to_string(flip));
        showOnGpu(nullptr,width,height);
        pump();
    }
    void setPresenter(WindowPresenter* presenter) {presenter_=presenter;}
    bool gpuPresented() const {return presenter_!=nullptr;}
    void setScaling(Scaling scaling) {scaling_=scaling;}
    void setWidescreen(bool widescreen) {widescreen_=widescreen;}
    void setStatus(std::string text) {status_=std::move(text);} // Not drawn yet on SDL (no text renderer here).
    unsigned largestScale() const {
        const auto [width,height]=clientSize();
        return nearestScale(int(width),int(height),int(pictureWidth(640,448)),448);
    }
    void setLayout(unsigned scale,bool fullscreen) {
        scale_=scale;fullscreen_=fullscreen;
        if(window_)applyLayout();
    }
    std::pair<unsigned,unsigned> clientSize() const {
        int width=int(pictureWidth(640*scale_,448*scale_)),height=int(448*scale_);
        if(window_)SDL_GetWindowSizeInPixels(window_,&width,&height);
        return {unsigned(std::max(width,1)),unsigned(std::max(height,1))};
    }
    void pump() {
        SDL_Event event;
        while(SDL_PollEvent(&event))handle(event);
    }
    // One event: the window's state (also given the events the BT3 renderer reads itself, gs/hw/hw_gs_mirror.cpp).
    void handle(const SDL_Event& event) {
        {
            switch(event.type) {
            case SDL_EVENT_QUIT:case SDL_EVENT_WINDOW_CLOSE_REQUESTED:closed_=true;break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:focused_=true;break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:focused_=false;break;
            case SDL_EVENT_WILL_ENTER_BACKGROUND:case SDL_EVENT_DID_ENTER_BACKGROUND:
                if(presenter_)presenter_->lost(); // Before the surface goes.
                background_=true;break;
            case SDL_EVENT_DID_ENTER_FOREGROUND: // A new surface: the old swapchain (if the pump blocked before it was let go) is of no use.
                if(presenter_)presenter_->lost();
                background_=false;break;
            case SDL_EVENT_KEY_DOWN:
                if(!event.key.repeat && (event.key.scancode==SDL_SCANCODE_AC_BACK || event.key.scancode==SDL_SCANCODE_ESCAPE))back_=true;
                break;
            default:break;
            }
        }
    }
    void waitUntilClosed() {
        while(window_ && !closed_) {SDL_WaitEventTimeout(nullptr,100);pump();}
    }
    bool closed() const {return closed_;}
    bool focused() const {return window_ && focused_ && !background_;} // Input is taken only then.
    bool opened() const {return window_!=nullptr;}
    SDL_Window* handle() const {return window_;}
    SDL_Window* ensureWindow() {if(!window_)create();return window_;} // (the BT3 renderer draws into it)
    // Whether the back key was pressed since the last call.
    bool backPressed() {const bool pressed=back_;back_=false;return pressed;}

private:
    Scaling scaling_;SDL_Window* window_=nullptr;bool closed_=false,focused_=true,background_=false,back_=false;
    unsigned scale_=2;bool fullscreen_=false,widescreen_=false;std::string title_,status_;
    WindowPresenter* presenter_=nullptr;DisplayFrame no_status_;

    void setTitle(const std::string& title) {if(title!=title_) {title_=title;SDL_SetWindowTitle(window_,title.c_str());}}
    WindowPresenter::Rect placement(int client_width,int client_height,unsigned width,unsigned height) const {
        const int shown_width=int(pictureWidth(width,height)),shown_height=int(height);
        int w,h;
        if(scaling_==Scaling::Integer && client_height>=shown_height) {
            int scale=std::max(1,client_height/shown_height);
            while(scale>1 && int(pictureWidth(width*scale,height*scale))>client_width)--scale;
            w=int(pictureWidth(width*scale,height*scale));h=shown_height*scale;
        } else if(std::int64_t(client_width)*shown_height<=std::int64_t(client_height)*shown_width) {
            w=std::max(1,client_width);h=std::max(1,int(std::int64_t(client_width)*shown_height/shown_width));
        } else {
            h=std::max(1,client_height);w=std::max(1,int(std::int64_t(client_height)*shown_width/shown_height));
        }
        return {(client_width-w)/2,(client_height-h)/2,w,h};
    }
    void showOnGpu(const DisplayFrame* frame,unsigned width,unsigned height) {
        if(background_)return; // No surface to show on.
        int client_width=0,client_height=0;SDL_GetWindowSizeInPixels(window_,&client_width,&client_height);
        if(client_width<=0 || client_height<=0 || !width || !height)return;
        const auto at=placement(client_width,client_height,width,height);
        const bool smooth=at.width%int(width) || at.height%int(height);
        presenter_->show(window_,frame,at,unsigned(client_width),unsigned(client_height),smooth,no_status_,{});
    }
    unsigned pictureWidth(unsigned width,unsigned height) const {return widescreen_?(height*16+4)/9:width;}
    static unsigned nearestScale(int width,int height,int picture_width,int picture_height) {
        const bool by_width=std::int64_t(width)*picture_height<=std::int64_t(height)*picture_width;
        const int have=by_width?width:height,unit=by_width?picture_width:picture_height;
        return unsigned(std::max(1,(have+unit/2)/unit));
    }
    void applyLayout() {
#ifndef __ANDROID__
        SDL_SetWindowFullscreen(window_,fullscreen_);
        if(!fullscreen_)SDL_SetWindowSize(window_,int(pictureWidth(640*scale_,448*scale_)),int(448*scale_));
#endif
    }
    void create() {
        if(!SDL_InitSubSystem(SDL_INIT_VIDEO))throw std::runtime_error(std::string("Viewer: SDL video: ")+SDL_GetError());
#ifdef __ANDROID__
        // No SDL_WINDOW_VULKAN: the presenter makes the surface from the native window with the renderer's own
        // loader (a custom driver included), so SDL loads no Vulkan library of its own.
        SDL_WindowFlags flags=SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_RESIZABLE|SDL_WINDOW_FULLSCREEN;
        // BT2_GL_WINDOW (android_main.cpp, BT3's renderer with "gpu_api = 1": its OpenGL ES back end, the option for
        // Mali GPUs): the window is made for OpenGL ES 3.2, which on Android is part of the window's EGL
        // configuration and has to be asked for before it is made (gs/hw/bt3/gs_gl.c then makes its context in it).
        if(std::getenv("BT2_GL_WINDOW")) {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,2);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);
            SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,0);
            flags|=SDL_WINDOW_OPENGL;
        }
#else
        const SDL_WindowFlags flags=SDL_WINDOW_VULKAN|SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_RESIZABLE;
#endif
        window_=SDL_CreateWindow("BT2 Recomp",int(pictureWidth(640*scale_,448*scale_)),int(448*scale_),flags);
        if(!window_)throw std::runtime_error(std::string("Viewer window creation failed: ")+SDL_GetError());
        if(fullscreen_)applyLayout();
    }
};
