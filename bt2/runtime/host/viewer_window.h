#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <thread>
#include <utility>
#include "display_frame.h"
#include "window_presenter.h"

// Minimal Win32 presentation window: shows the latest decoded frame in the client area, either at the
// largest integer scale that fits (pixel-exact) or at the largest size of the picture's proportions
// that fits; centred, with black borders. In widescreen (the game's widescreen patch, ee/game_patches.h)
// the picture is shown 16:9: as wide as its height times 16/9, whole multiples counted vertically.
// Messages are pumped by the caller at each presentation; closing the window sets closed(). The
// window does not pace the run (host/frame_pacer.h does). The window's size is a multiple of the game's
// picture (640 x 448) or the whole screen without a border; both, the scaling and a short status
// text in the corner (the frame rate display) can change while it is open. With a presenter
// (host/window_presenter.h) the pictures are shown on the GPU; the renderer's own picture then
// reaches the window without a copy through the processor (presentRenderer).
class ViewerWindow {
public:
    enum class Scaling {Integer,Stretch};
    explicit ViewerWindow(Scaling scaling=Scaling::Integer):scaling_(scaling) {}
    ~ViewerWindow() {if(hwnd_)DestroyWindow(hwnd_);}
    void present(const DisplayFrame& frame,unsigned flip) {
        present(frame,"BT2 Recomp - frame "+std::to_string(flip)+(frame.unwritten?" (unwritten pixels)":""));
    }
    void present(const DisplayFrame& frame,const std::string& title) {
        if(closed_)return;
        if(!hwnd_)create();
        setTitle(title);
        if(presenter_) {showOnGpu(&frame,frame.width,frame.height);pump();return;}
        frame_=frame;
        InvalidateRect(hwnd_,nullptr,FALSE);UpdateWindow(hwnd_);
        pump();
    }
    // The renderer's last scanout (width x height), shown by the presenter.
    void presentRenderer(unsigned width,unsigned height,unsigned flip) {
        if(closed_)return;
        if(!presenter_)throw std::runtime_error("Viewer: the renderer's picture needs a GPU presenter");
        if(!hwnd_)create();
        setTitle("BT2 Recomp - frame "+std::to_string(flip));
        showOnGpu(nullptr,width,height);
        pump();
    }
    void setPresenter(WindowPresenter* presenter) {presenter_=presenter;}
    bool gpuPresented() const {return presenter_!=nullptr;}
    void setScaling(Scaling scaling) {scaling_=scaling;}
    void setWidescreen(bool widescreen) {
        if(widescreen==widescreen_)return;
        widescreen_=widescreen;
        if(hwnd_)applyLayout();
    }
    void setStatus(std::string text) {status_=std::move(text);} // Drawn in the top left corner; empty for none.
    // The multiple of 640 x 448 nearest to the screen the window is on (the main screen before it
    // exists): the largest that fits, or one more when that is closer, which cuts a little off the
    // picture's edges.
    unsigned largestScale() const {
        MONITORINFO monitor{};monitor.cbSize=sizeof monitor;
        LONG width=GetSystemMetrics(SM_CXSCREEN),height=GetSystemMetrics(SM_CYSCREEN);
        if(hwnd_ && GetMonitorInfoA(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&monitor)) {width=monitor.rcMonitor.right-monitor.rcMonitor.left;height=monitor.rcMonitor.bottom-monitor.rcMonitor.top;}
        return nearestScale(width,height,LONG(pictureWidth(640,448)),448);
    }
    // The window's size as a multiple of 640 x 448, or the whole screen without a border.
    void setLayout(unsigned scale,bool fullscreen) {
        if(scale==scale_ && fullscreen==fullscreen_)return;
        scale_=scale;fullscreen_=fullscreen;
        if(hwnd_)applyLayout();
    }
    // The client area in pixels (640 x 448 times the scale before the window exists).
    std::pair<unsigned,unsigned> clientSize() const {
        RECT client{0,0,LONG(pictureWidth(640*scale_,448*scale_)),LONG(448*scale_)};
        if(hwnd_)GetClientRect(hwnd_,&client);
        return {unsigned(std::max<LONG>(client.right,1)),unsigned(std::max<LONG>(client.bottom,1))};
    }
    void pump() {
        MSG msg;
        while(PeekMessageA(&msg,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&msg);DispatchMessageA(&msg);}
    }
    void waitUntilClosed() {
        MSG msg;
        while(hwnd_ && !closed_ && GetMessageA(&msg,nullptr,0,0)>0) {TranslateMessage(&msg);DispatchMessageA(&msg);}
    }
    bool closed() const {return closed_;}
    bool focused() const {return hwnd_ && GetForegroundWindow()==hwnd_;} // Input is taken only then.
    bool opened() const {return hwnd_!=nullptr;}
    HWND handle() const {return hwnd_;} // For dialogs that belong to the window.
private:
    Scaling scaling_;HWND hwnd_=nullptr;bool closed_=false;DisplayFrame frame_;
    unsigned scale_=2;bool fullscreen_=false,widescreen_=false;std::string title_,status_;
    WindowPresenter* presenter_=nullptr;
    std::string status_drawn_;DisplayFrame status_image_; // The status text as a picture, for the presenter.
    void setTitle(const std::string& title) {if(title!=title_) {title_=title;SetWindowTextA(hwnd_,title.c_str());}}
    // Where a picture of width x height goes in a client area: whole multiples of its shown size, or
    // the largest size of its shown proportions that fits; centred.
    WindowPresenter::Rect placement(LONG client_width,LONG client_height,unsigned width,unsigned height) const {
        const LONG shown_width=LONG(pictureWidth(width,height)),shown_height=LONG(height);
        int w,h;
        if(scaling_==Scaling::Integer) {
            // A window holds whole pictures; the whole screen takes the nearest multiple. In widescreen the
            // width of a multiple is 16:9 of its height, so a super-sampled picture (1280 x 896) fills the
            // same window as the game's (640 x 448) at half the multiple.
            int scale=std::max(1,int(client_height/shown_height));
            while(scale>1 && LONG(pictureWidth(width*scale,height*scale))>client_width)--scale;
            if(fullscreen_)scale=int(nearestScale(client_width,client_height,shown_width,shown_height));
            w=int(pictureWidth(width*scale,height*scale));h=int(shown_height)*scale;
        } else if(std::int64_t(client_width)*shown_height<=std::int64_t(client_height)*shown_width) {
            w=std::max(1,int(client_width));h=std::max(1,int(std::int64_t(client_width)*shown_height/shown_width));
        } else {
            h=std::max(1,int(client_height));w=std::max(1,int(std::int64_t(client_height)*shown_width/shown_height));
        }
        return {int(client_width-w)/2,int(client_height-h)/2,w,h};
    }
    void showOnGpu(const DisplayFrame* frame,unsigned width,unsigned height) {
        RECT client;GetClientRect(hwnd_,&client);
        if(client.right<=0 || client.bottom<=0 || !width || !height)return; // Minimized.
        const auto at=placement(client.right,client.bottom,width,height);
        if(status_!=status_drawn_) {status_drawn_=status_;status_image_=drawStatus(status_);}
        // Whole multiples keep pixels as blocks; any other stretch (widescreen, scaling off) is filtered.
        const bool smooth=at.width%int(width) || at.height%int(height);
        presenter_->show(hwnd_,frame,at,unsigned(client.right),unsigned(client.bottom),smooth,status_image_,
            {at.x+6,at.y+4,int(status_image_.width),int(status_image_.height)});
    }
    // The status text as the GDI path draws it (yellow on black), as a picture.
    static DisplayFrame drawStatus(const std::string& text) {
        DisplayFrame image;
        if(text.empty())return image;
        HDC screen=GetDC(nullptr);HDC dc=CreateCompatibleDC(screen);ReleaseDC(nullptr,screen);
        SIZE size{};GetTextExtentPoint32A(dc,text.c_str(),int(text.size()),&size);
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=std::max<LONG>(size.cx,1);
        bi.bmiHeader.biHeight=-std::max<LONG>(size.cy,1);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
        if(!bitmap) {DeleteDC(dc);return image;}
        const auto old=SelectObject(dc,bitmap);
        SetBkMode(dc,OPAQUE);SetBkColor(dc,RGB(0,0,0));SetTextColor(dc,RGB(255,255,0));
        TextOutA(dc,0,0,text.c_str(),int(text.size()));GdiFlush();
        image.width=unsigned(bi.bmiHeader.biWidth);image.height=unsigned(-bi.bmiHeader.biHeight);
        const auto* pixels=static_cast<const std::uint32_t*>(bits);
        image.bgra.assign(pixels,pixels+std::size_t(image.width)*image.height);
        for(auto& p:image.bgra)p|=0xFF000000u;
        SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
        return image;
    }
    // The shown width of a picture: its own (one pixel for one pixel at scale 1), or 16:9 of its height.
    unsigned pictureWidth(unsigned width,unsigned height) const {return widescreen_?(height*16+4)/9:width;}
    static unsigned nearestScale(LONG width,LONG height,LONG picture_width,LONG picture_height) {
        // The tighter of the two directions, rounded to the nearest whole number.
        const bool by_width=std::int64_t(width)*picture_height<=std::int64_t(height)*picture_width;
        const LONG have=by_width?width:height,unit=by_width?picture_width:picture_height;
        return unsigned(std::max<LONG>(1,(have+unit/2)/unit));
    }
    void applyLayout() {
        if(fullscreen_) {
            MONITORINFO monitor{};monitor.cbSize=sizeof monitor;GetMonitorInfoA(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&monitor);
            SetWindowLongPtrA(hwnd_,GWL_STYLE,WS_POPUP|WS_VISIBLE);
            SetWindowPos(hwnd_,HWND_TOP,monitor.rcMonitor.left,monitor.rcMonitor.top,monitor.rcMonitor.right-monitor.rcMonitor.left,
                monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_FRAMECHANGED);
        } else {
            RECT r{0,0,LONG(pictureWidth(640*scale_,448*scale_)),LONG(448*scale_)};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
            SetWindowLongPtrA(hwnd_,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE);
            RECT at;GetWindowRect(hwnd_,&at);
            SetWindowPos(hwnd_,HWND_TOP,std::max<LONG>(at.left,0),std::max<LONG>(at.top,0),r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED);
        }
    }
    void create() {
        SetProcessDPIAware(); // One picture pixel is a whole number of screen pixels; the system does not rescale the window.
        WNDCLASSA wc{};wc.lpfnWndProc=&ViewerWindow::proc;wc.hInstance=GetModuleHandleA(nullptr);
        wc.lpszClassName="BT2RecompViewer";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        RegisterClassA(&wc);
        RECT r{0,0,LONG(pictureWidth(640*scale_,448*scale_)),LONG(448*scale_)};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
        hwnd_=CreateWindowA(wc.lpszClassName,"BT2 Recomp",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,
            r.right-r.left,r.bottom-r.top,nullptr,nullptr,wc.hInstance,this);
        if(!hwnd_)throw std::runtime_error("Viewer window creation failed");
        SetWindowLongPtrA(hwnd_,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(this));
        if(fullscreen_)applyLayout();
    }
    void paint(HDC dc) {
        if(presenter_)return; // The presenter draws the window.
        RECT client;GetClientRect(hwnd_,&client);
        if(frame_.bgra.empty()) {FillRect(dc,&client,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));return;}
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=LONG(frame_.width);
        bi.bmiHeader.biHeight=-LONG(frame_.height);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        // The picture keeps its shape in every window: whole multiples of its size, or the largest
        // size of the same proportions that fits; the rest of the window is black.
        const auto at=placement(client.right,client.bottom,frame_.width,frame_.height);
        const int x=at.x,y=at.y,w=at.width,h=at.height;
        // Black only the borders around the frame: clearing the whole client area first made every
        // presented frame flash black briefly (flicker).
        const auto black=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        const RECT borders[4]={{0,0,client.right,y},{0,y+h,client.right,client.bottom},{0,y,x,y+h},{x+w,y,client.right,y+h}};
        for(const auto& border:borders)if(border.right>border.left && border.bottom>border.top)FillRect(dc,&border,black);
        // Pixels are copied, also where the stretch is not a whole multiple (widescreen, scaling off):
        // GDI's filtered stretch (HALFTONE) took about 12 ms per picture at 1592 x 896.
        SetStretchBltMode(dc,COLORONCOLOR);
        StretchDIBits(dc,x,y,w,h,0,0,int(frame_.width),int(frame_.height),frame_.bgra.data(),&bi,DIB_RGB_COLORS,SRCCOPY);
        if(!status_.empty()) {
            SetBkMode(dc,OPAQUE);SetBkColor(dc,RGB(0,0,0));SetTextColor(dc,RGB(255,255,0));
            TextOutA(dc,x+6,y+4,status_.c_str(),int(status_.size()));
        }
    }
    static LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
        auto* self=reinterpret_cast<ViewerWindow*>(GetWindowLongPtrA(hwnd,GWLP_USERDATA));
        switch(message) {
        case WM_PAINT: {PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);if(self)self->paint(dc);EndPaint(hwnd,&ps);return 0;}
        case WM_ERASEBKGND: return 1;
        case WM_GETMINMAXINFO: { // A window may be larger than the screen (a size chosen as a multiple).
            auto* limits=reinterpret_cast<MINMAXINFO*>(l);limits->ptMaxTrackSize.x=limits->ptMaxTrackSize.y=32000;return 0;
        }
        case WM_CLOSE: if(self)self->closed_=true;DestroyWindow(hwnd);return 0;
        case WM_DESTROY: if(self)self->hwnd_=nullptr;return 0;
        default: return DefWindowProcA(hwnd,message,w,l);
        }
    }
};
#elif defined(BT2_SDL)
#include "host/sdl/sdl_viewer_window.h"
#endif
