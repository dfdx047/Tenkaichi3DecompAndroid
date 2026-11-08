// Draws a menu screen into a frame (docs/plans/launcher/LAUNCHER_PLAN.md, Style), after the console's
// system menu (the user's reference pictures):
// - the first screen: black, a tilted ring of glowing points turning at the left, the entries beside
//   it without a title (the chosen one light blue, the others grey), the game's logo above them, and
//   the buttons at the bottom;
// - the screens below it: a violet haze with a slowly turning fan of translucent bars around a dark
//   sphere and translucent cubes circling at the left, the date and the time in the top corners, the
//   screen's title in yellow above the entries;
// - the chosen entry's value on the line under it, in white, with a small left/right mark when the
//   arrows change it; the other entries show their label only.
// The in-game menu is shown on the dark haze alone. Everything is drawn by this code with a system
// font; no artwork of the console's menu is used.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "host/utf8.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include "host/display_frame.h"
#include "host/logo_image.h"
#include "host/menu_model.h"

class MenuRenderer {
public:
    static constexpr unsigned visible_entries=7;
    ~MenuRenderer() {release();}
    MenuRenderer()=default;
    MenuRenderer(const MenuRenderer&)=delete;
    MenuRenderer& operator=(const MenuRenderer&)=delete;

    // Whether a game disc is in (the launcher: a disc image of the data folder is chosen); the first
    // screen then shows a small disc blinking in its top right corner.
    void setDiscPresent(std::function<bool()> present) {disc_present_=std::move(present);}
    // The logo shown on the first screen (a PNG with transparency); none when the file is missing.
    void setLogo(std::filesystem::path path) {logo_path_=std::move(path);logo_={};}

    // `seconds` moves the background; `root` chooses the first screen's look; `prompt` replaces the
    // note (waiting for a key); `backdrop` is set for the in-game menu (its picture is not shown).
    DisplayFrame draw(const MenuScreen& screen,unsigned width,unsigned height,double seconds,bool root,const std::string& prompt={},
                      const DisplayFrame* backdrop=nullptr) {
        prepare(width,height);
        if(screen.browser)return browser(screen,seconds);
        const bool over_game=backdrop && backdrop->width,first=root && !over_game;
        std::copy(haze_[first || over_game?0:1].begin(),haze_[first || over_game?0:1].end(),bits_);
        if(first) {
            ring(seconds);
            if(disc_present_ && disc_present_()) {if(disc_since_<0)disc_since_=seconds;discMark(seconds-disc_since_);}
            else disc_since_=-1;
        }
        else if(!over_game) {fan(seconds);cubes(seconds,screen.highlighted);}
        SetBkMode(dc_,TRANSPARENT);
        const int w=int(width),h=int(height),unit=h/24;
        if(!first)clock(unit);

        // The entries: a column beside the ring, the chosen one followed by its value on a line of its own.
        const unsigned count=unsigned(screen.entries.size());
        const unsigned shown=std::min(count,visible_entries);
        const unsigned top_entry=count<=visible_entries?0:std::min(screen.highlighted>visible_entries/2?screen.highlighted-visible_entries/2:0u,count-visible_entries);
        const auto& chosen=screen.entries[screen.highlighted];
        const std::string chosen_value=chosen.value?chosen.value():std::string();
        const int row=first?unit*4/3:unit*3/2,left=w*46/100,right=w*94/100;
        const int lines=int(shown)+(chosen_value.empty()?0:1);
        int y=(first?h*55/100:h*52/100)-lines*row/2;
        if(first) {
            logo(w*25/100,h*4/100,w*50/100,h*24/100);
        } else text(screen.title,left,y-row*3/2,title_,RGB(240,222,70));
        const double pulse=0.5+0.5*std::sin(seconds*3.0);
        for(unsigned n=top_entry;n<top_entry+shown;++n,y+=row) {
            const auto& entry=screen.entries[n];
            const bool on=n==screen.highlighted;
            const COLORREF colour=!entry.enabled?RGB(78,78,86):on?RGB(BYTE(96+30*pulse),BYTE(200+20*pulse),255):RGB(172,172,178);
            const int mark=on && entry.adjust && !entry.confirm?unit*3/2:0;
            const std::string label=fit(entry.label,entry_,right-left-mark);
            if(entry.enabled)text(label,left,y,entry_,colour);else plainText(label,left,y,entry_,colour);
            if(mark)arrowsMark(left+extent(label,entry_)+unit*2/3,y+row/2-unit/8,unit);
            if(on && !chosen_value.empty()) {
                y+=row;
                text(fit(chosen_value,entry_,right-left-unit),left+unit,y,entry_,RGB(236,236,240));
            }
        }
        if(top_entry>0)triangle(left-unit,y-int(shown)*row-row/2,unit/3,true,RGB(90,150,255));
        if(top_entry+shown<count)triangle(left-unit,y-row/2,unit/3,false,RGB(90,150,255));
        const std::string& note=!prompt.empty()?prompt:chosen.note;
        if(!note.empty())textCentred(fit(note,note_,w*90/100),w/2,h-unit*9/2,note_,!prompt.empty()?RGB(255,225,120):RGB(150,150,164));
        // The buttons, as on the console: the cross at the left of the middle, the circle to its right.
        const int by=h-unit*3;
        hint(Symbol::Cross,"Enter",w*34/100,by,unit);
        if(!first)hint(Symbol::Circle,"Back",w*58/100,by,unit);
        DisplayFrame frame;frame.width=width;frame.height=height;frame.bgra.assign(bits_,bits_+std::size_t(width)*height);
        return frame;
    }

private:
    enum class Symbol {Cross,Circle};

    // The console's browser: grey, the chosen item's icon, name and details at the top left, the
    // screen's title in yellow at the top right, the items as a row of icons (the chosen one lit and
    // gently bobbing, the others dimmed), what is in use marked under its icon.
    DisplayFrame browser(const MenuScreen& screen,double seconds) {
        std::copy(haze_[2].begin(),haze_[2].end(),bits_);
        SetBkMode(dc_,TRANSPARENT);
        const int w=int(width_),h=int(height_),unit=h/24;
        const auto& chosen=screen.entries[screen.highlighted];
        icon(chosen.icon,w*7/100,h*9/100,h*4/100,seconds,1.0,true);
        text(fit(chosen.label,title_,w*50/100),w*12/100,h*5/100,title_,RGB(240,240,244));
        if(!chosen.note.empty())plainText(fit(chosen.note,note_,w*52/100),w*12/100,h*5/100+unit*4/3,note_,RGB(200,200,206));
        textRight(screen.title,w*93/100,h*5/100,title_,RGB(240,222,70));
        // The icons: up to five in a row, two rows at a time.
        constexpr unsigned per_row=5,rows_shown=2;
        const unsigned count=unsigned(screen.entries.size()),row_of_chosen=screen.highlighted/per_row;
        const unsigned first_row=row_of_chosen<rows_shown?0:row_of_chosen-rows_shown+1;
        const int cell=w*16/100,size=h*7/100,top=h*38/100,row_height=h*25/100;
        for(unsigned n=first_row*per_row;n<count && n<(first_row+rows_shown)*per_row;++n) {
            const auto& entry=screen.entries[n];
            const unsigned in_row=std::min(per_row,count-(n/per_row)*per_row);
            const int x=w/2-int(in_row)*cell/2+int(n%per_row)*cell+cell/2,y=top+int(n/per_row-first_row)*row_height;
            const bool on=n==screen.highlighted;
            const double bob=on?std::sin(seconds*2.4)*h*0.006:0.0;
            icon(entry.icon,x,int(y+bob),on?size*12/10:size,on?seconds:0.0,!entry.enabled?0.35:on?1.0:0.55,on);
            const std::string value=entry.value?entry.value():std::string();
            if(!value.empty())textCentred(fit(value,note_,cell-unit/2),x,y+size*14/10,note_,on?RGB(120,210,255):RGB(170,190,205));
            if(on)textCentred(fit(entry.label,note_,cell*2),x,y+size*14/10+(value.empty()?0:unit),note_,RGB(244,244,248));
        }
        if((first_row+rows_shown)*per_row<count)triangle(w/2,top+int(rows_shown)*row_height-unit,unit/3,false,RGB(50,90,230));
        if(first_row>0)triangle(w/2,top-size*15/10,unit/3,true,RGB(50,90,230));
        const int by=h-unit*3;
        hint(Symbol::Cross,"Enter",w*34/100,by,unit);
        hint(Symbol::Circle,"Back",w*58/100,by,unit);
        DisplayFrame frame;frame.width=width_;frame.height=height_;frame.bgra.assign(bits_,bits_+std::size_t(width_)*height_);
        return frame;
    }
    // An item's icon centred at (cx, cy), `size` its half height; `seconds` turns a disc; `light` 0..1
    // dims it into the background.
    void icon(MenuIcon kind,int cx,int cy,int size,double seconds,double light,bool shadow) {
        if(shadow)glowShadow(cx,cy+size*12/10,size);
        switch(kind) {
        case MenuIcon::Disc:spinningDisc(cx,cy,size,seconds,light);break;
        case MenuIcon::Card:case MenuIcon::CardImage:card(cx,cy,size,light,kind==MenuIcon::CardImage);break;
        case MenuIcon::NewCard:newCard(cx,cy,size,light);break;
        case MenuIcon::NoDisc: {
            // An empty place for a disc: a dashed circle around the hole.
            const COLORREF colour=RGB(BYTE(64+(220-64)*light),BYTE(64+(220-64)*light),BYTE(68+(228-68)*light));
            const HPEN pen=CreatePen(PS_DASH,1,colour);const HGDIOBJ old_pen=SelectObject(dc_,pen),old_brush=SelectObject(dc_,GetStockObject(NULL_BRUSH));
            Ellipse(dc_,cx-size,cy-size,cx+size,cy+size);Ellipse(dc_,cx-size/6,cy-size/6,cx+size/6,cy+size/6);
            SelectObject(dc_,old_pen);SelectObject(dc_,old_brush);DeleteObject(pen);
            break;
        }
        case MenuIcon::None:break;
        }
    }
    // A soft dark patch under a lifted icon.
    void glowShadow(int cx,int cy,int size) {
        const double rx=size*1.1,ry=size*0.28;
        for(int y=int(cy-ry);y<=int(cy+ry);++y)for(int x=int(cx-rx);x<=int(cx+rx);++x) {
            if(x<0 || y<0 || x>=int(width_) || y>=int(height_))continue;
            const double d=std::hypot((x-cx)/rx,(y-cy)/ry);if(d>=1)continue;
            auto& p=bits_[std::size_t(y)*width_+x];p=mix(p,0,0.35*(1-d)*(1-d));
        }
    }
    // A memory card seen from the front: a dark body with a light rim and a label strip; an image of
    // another program is shown with a question mark.
    void card(int cx,int cy,int size,double light,bool unknown) {
        const auto shade=[&](int r,int g,int b){return RGB(BYTE(64+(r-64)*light),BYTE(64+(g-64)*light),BYTE(68+(b-68)*light));};
        const int hw=size*8/10,hh=size;
        const HBRUSH body=CreateSolidBrush(shade(28,28,32));const HPEN rim=CreatePen(PS_SOLID,std::max(size/14,1),shade(150,150,160));
        HGDIOBJ old_brush=SelectObject(dc_,body),old_pen=SelectObject(dc_,rim);
        RoundRect(dc_,cx-hw,cy-hh,cx+hw,cy+hh,size/4,size/4);
        SelectObject(dc_,old_brush);SelectObject(dc_,old_pen);DeleteObject(body);DeleteObject(rim);
        // The label strip and the small coloured mark of a PS2 card.
        const HBRUSH strip=CreateSolidBrush(shade(70,70,78));old_brush=SelectObject(dc_,strip);old_pen=SelectObject(dc_,GetStockObject(NULL_PEN));
        Rectangle(dc_,cx-hw*7/10,cy-hh*6/10,cx+hw*7/10,cy+hh*2/10);
        SelectObject(dc_,old_brush);DeleteObject(strip);
        const HBRUSH mark=CreateSolidBrush(shade(40,90,200));old_brush=SelectObject(dc_,mark);
        Rectangle(dc_,cx-hw*7/10,cy+hh*4/10,cx-hw*2/10,cy+hh*6/10);
        SelectObject(dc_,old_brush);SelectObject(dc_,old_pen);DeleteObject(mark);
        if(unknown)textCentred("?",cx,cy-hh*5/10,title_,shade(230,230,236));
    }
    // "New empty card": the outline of a card with a plus.
    void newCard(int cx,int cy,int size,double light) {
        const COLORREF colour=RGB(BYTE(64+(230-64)*light),BYTE(64+(230-64)*light),BYTE(68+(236-68)*light));
        const int hw=size*8/10,hh=size;
        const HPEN pen=CreatePen(PS_DASH,1,colour);HGDIOBJ old_pen=SelectObject(dc_,pen),old_brush=SelectObject(dc_,GetStockObject(NULL_BRUSH));
        RoundRect(dc_,cx-hw,cy-hh,cx+hw,cy+hh,size/4,size/4);
        SelectObject(dc_,old_pen);DeleteObject(pen);
        const HPEN plus=CreatePen(PS_SOLID,std::max(size/8,2),colour);old_pen=SelectObject(dc_,plus);
        MoveToEx(dc_,cx-hw/2,cy,nullptr);LineTo(dc_,cx+hw/2,cy);MoveToEx(dc_,cx,cy-hw/2,nullptr);LineTo(dc_,cx,cy+hw/2);
        SelectObject(dc_,old_pen);SelectObject(dc_,old_brush);DeleteObject(plus);
    }
    HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;std::uint32_t* bits_=nullptr;unsigned width_=0,height_=0;
    HFONT title_=nullptr,entry_=nullptr,note_=nullptr,hint_=nullptr;
    std::vector<std::uint32_t> haze_[3]; // The still part of the backgrounds: first screen, other screens, browser.
    double disc_since_=-1; // When the disc mark began to show; negative while no disc is in.
    std::filesystem::path logo_path_;LogoImage logo_;unsigned logo_box_w_=0,logo_box_h_=0;
    std::function<bool()> disc_present_;

    void release() {
        for(HFONT font:{title_,entry_,note_,hint_})if(font)DeleteObject(font);
        if(dc_) {SelectObject(dc_,old_);DeleteDC(dc_);}
        if(bitmap_)DeleteObject(bitmap_);
        dc_=nullptr;bitmap_=nullptr;title_=entry_=note_=hint_=nullptr;bits_=nullptr;
    }
    void prepare(unsigned width,unsigned height) {
        if(width==width_ && height==height_ && dc_)return;
        release();
        width_=width;height_=height;
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=LONG(width);bi.bmiHeader.biHeight=-LONG(height);
        bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        dc_=CreateCompatibleDC(nullptr);void* bits=nullptr;
        bitmap_=CreateDIBSection(dc_,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
        if(!dc_ || !bitmap_)throw std::runtime_error("Menu drawing surface could not be created");
        bits_=static_cast<std::uint32_t*>(bits);old_=SelectObject(dc_,bitmap_);
        const auto font=[&](int size,int weight) {return CreateFontA(-size,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");};
        const int unit=int(height)/24;
        title_=font(unit,FW_NORMAL);entry_=font(unit,FW_NORMAL);note_=font(std::max(unit*2/3,11),FW_NORMAL);hint_=font(std::max(unit*5/6,11),FW_NORMAL);
        // The hazes: the first screen almost black with a faint light where the ring turns; the others
        // violet, lighter where the fan turns; both with fine grain.
        for(unsigned kind=0;kind<3;++kind) {
            haze_[kind].resize(std::size_t(width)*height);
            std::uint32_t seed=12345u+kind;
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                const double dx=(double(x)/width-0.30)*double(width)/height,dy=double(y)/height-0.5;
                const double glow=std::max(0.0,1.0-std::sqrt(dx*dx+dy*dy)*1.25);
                seed=seed*1664525u+1013904223u;const double grain=double((seed>>24)&7);
                double r,g,b;
                if(kind==0) {r=8+10*glow+grain;g=8+11*glow+grain;b=11+16*glow+grain;}
                else if(kind==1) {r=36+50*glow+grain;g=28+38*glow+grain;b=70+92*glow+grain;}
                else {
                    // The browser: grey, lightest above the middle of the icons, dark at the edges.
                    const double bx=double(x)/width-0.42,by=double(y)/height-0.30,light=std::max(0.0,1.0-std::sqrt(bx*bx*1.4+by*by)*1.3);
                    r=g=40+120*light*light+grain;b=r+4;
                }
                haze_[kind][std::size_t(y)*width+x]=(std::uint32_t(r)<<16)|(std::uint32_t(g)<<8)|std::uint32_t(b);
            }
        }
    }
    // Text with a soft dark edge below and to the right.
    void text(const std::string& s,int x,int y,HFONT font,COLORREF colour) {
        SelectObject(dc_,font);const int offset=std::max(int(height_)/360,1);
        const std::wstring wide=wideFromUtf8(s);
        SetTextColor(dc_,RGB(0,0,0));TextOutW(dc_,x+offset,y+offset,wide.c_str(),int(wide.size()));
        SetTextColor(dc_,colour);TextOutW(dc_,x,y,wide.c_str(),int(wide.size()));
    }
    void plainText(const std::string& s,int x,int y,HFONT font,COLORREF colour) {
        SelectObject(dc_,font);const std::wstring wide=wideFromUtf8(s);
        SetTextColor(dc_,colour);TextOutW(dc_,x,y,wide.c_str(),int(wide.size()));
    }
    int extent(const std::string& s,HFONT font) {const std::wstring wide=wideFromUtf8(s);SelectObject(dc_,font);SIZE size{};GetTextExtentPoint32W(dc_,wide.c_str(),int(wide.size()),&size);return size.cx;}
    // `s`, or its beginning with three dots, within `width` pixels.
    std::string fit(std::string s,HFONT font,int width) {
        if(extent(s,font)<=width)return s;
        // Whole characters go: a UTF-8 character's continuation bytes with their lead byte.
        while(s.size()>1 && extent(s+"...",font)>width) {while(s.size()>1 && (static_cast<unsigned char>(s.back())&0xC0)==0x80)s.pop_back();s.pop_back();}
        return s+"...";
    }
    void textRight(const std::string& s,int right,int y,HFONT font,COLORREF colour) {text(s,right-extent(s,font),y,font,colour);}
    void textCentred(const std::string& s,int centre,int y,HFONT font,COLORREF colour) {text(s,centre-extent(s,font)/2,y,font,colour);}
    void clock(int unit) {
        const std::time_t now=std::time(nullptr);std::tm local{};localtime_s(&local,&now);
        char date[32],time[32];std::strftime(date,sizeof date,"%m/%d/%Y",&local);std::strftime(time,sizeof time,"%I:%M:%S %p",&local);
        text(date,int(width_)*7/100,unit*3/2,hint_,RGB(222,222,230));
        textRight(time,int(width_)*93/100,unit*3/2,hint_,RGB(222,222,230));
    }
    // The logo, centred in the given box (kept scaled for the box until the window changes size).
    void logo(int x,int y,int box_w,int box_h) {
        if(logo_path_.empty() || box_w<=0 || box_h<=0)return;
        if(logo_box_w_!=unsigned(box_w) || logo_box_h_!=unsigned(box_h)) {
            logo_=LogoImage::load(logo_path_,unsigned(box_w),unsigned(box_h));logo_box_w_=unsigned(box_w);logo_box_h_=unsigned(box_h);
        }
        if(logo_.empty())return;
        const int ox=x+(box_w-int(logo_.width))/2,oy=y+(box_h-int(logo_.height))/2;
        for(unsigned ly=0;ly<logo_.height;++ly) {
            const int py=oy+int(ly);if(py<0 || py>=int(height_))continue;
            for(unsigned lx=0;lx<logo_.width;++lx) {
                const int px=ox+int(lx);if(px<0 || px>=int(width_))continue;
                const std::uint32_t s=logo_.bgra[std::size_t(ly)*logo_.width+lx];const unsigned a=s>>24;
                if(!a)continue;
                auto& d=bits_[std::size_t(py)*width_+px];
                const auto channel=[&](unsigned shift){return std::min(255u,((s>>shift)&255)+((d>>shift)&255)*(255-a)/255)<<shift;};
                d=channel(0)|channel(8)|channel(16);
            }
        }
    }
    void triangle(int cx,int cy,int size,bool up,COLORREF colour) {
        const POINT points[3]={{cx-size,cy+(up?size:-size)},{cx+size,cy+(up?size:-size)},{cx,cy+(up?-size:size)}};
        polygon(points,3,colour);
    }
    void polygon(const POINT* points,int count,COLORREF colour) {
        const HBRUSH brush=CreateSolidBrush(colour);const HGDIOBJ old_brush=SelectObject(dc_,brush),old_pen=SelectObject(dc_,GetStockObject(NULL_PEN));
        Polygon(dc_,points,count);
        SelectObject(dc_,old_brush);SelectObject(dc_,old_pen);DeleteObject(brush);
    }
    // The mark beside an entry the arrows change: a dark grey disc with a light triangle each side.
    void arrowsMark(int cx,int cy,int unit) {
        const int r=unit/2;
        const HBRUSH brush=CreateSolidBrush(RGB(70,70,82));const HGDIOBJ old_brush=SelectObject(dc_,brush),old_pen=SelectObject(dc_,GetStockObject(NULL_PEN));
        Ellipse(dc_,cx-r,cy-r,cx+r,cy+r);
        SelectObject(dc_,old_brush);SelectObject(dc_,old_pen);DeleteObject(brush);
        const int s=r/2;
        const POINT leftward[3]={{cx-s/3-s,cy},{cx-s/3,cy-s},{cx-s/3,cy+s}},rightward[3]={{cx+s/3+s,cy},{cx+s/3,cy-s},{cx+s/3,cy+s}};
        polygon(leftward,3,RGB(210,210,220));polygon(rightward,3,RGB(210,210,220));
    }
    // A button's symbol and its label.
    void hint(Symbol symbol,const char* label,int x,int y,int unit) {
        const int size=unit*3/10,cy=y+unit*3/7;
        const COLORREF colour=symbol==Symbol::Cross?RGB(150,160,235):RGB(240,90,90);
        const HPEN pen=CreatePen(PS_SOLID,std::max(unit/10,2),colour);const HGDIOBJ old_pen=SelectObject(dc_,pen),old_brush=SelectObject(dc_,GetStockObject(NULL_BRUSH));
        if(symbol==Symbol::Cross) {MoveToEx(dc_,x-size,cy-size,nullptr);LineTo(dc_,x+size,cy+size);MoveToEx(dc_,x+size,cy-size,nullptr);LineTo(dc_,x-size,cy+size);}
        else Ellipse(dc_,x-size,cy-size,x+size,cy+size);
        SelectObject(dc_,old_pen);SelectObject(dc_,old_brush);DeleteObject(pen);
        text(label,x+unit*2/3,y-unit/12,hint_,RGB(226,226,232));
    }
    static std::uint32_t mix(std::uint32_t under,std::uint32_t over,double alpha) {
        const auto channel=[&](unsigned shift) {
            const double a=double((under>>shift)&255),b=double((over>>shift)&255);
            return std::uint32_t(a+(b-a)*alpha)<<shift;
        };
        return channel(0)|channel(8)|channel(16);
    }
    // Light added to a pixel: red, green and blue amounts, each stopping at white.
    static std::uint32_t lit(std::uint32_t p,double r,double g,double b) {
        const auto channel=[&](unsigned shift,double add) {return std::uint32_t(std::min(255.0,double((p>>shift)&255)+add))<<shift;};
        return channel(16,r)|channel(8,g)|channel(0,b);
    }
    void glow(double cx,double cy,double radius,double strength) {
        const int x0=std::max(int(cx-radius),0),x1=std::min(int(cx+radius)+1,int(width_)),y0=std::max(int(cy-radius),0),y1=std::min(int(cy+radius)+1,int(height_));
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x) {
            const double d=std::hypot(x-cx,y-cy)/radius;
            if(d>=1.0)continue;
            const double fall=(1.0-d)*(1.0-d),core=d<0.18?1.0:d<0.26?(0.26-d)/0.08:0.0;
            auto& p=bits_[std::size_t(y)*width_+x];
            p=lit(p,strength*(150*fall+105*core),strength*(185*fall+70*core),strength*(255*fall));
        }
    }
    // The first screen: eight points of light on a tilted ring that turns slowly, the far ones dimmer,
    // around a faint dark-blue body.
    void ring(double seconds) {
        const double cx=width_*0.33,cy=height_*0.56,radius=height_*0.15,tilt=0.35;
        const auto at=[&](double angle,double& x,double& y) {
            const double across=std::cos(angle)*radius*0.62,along=std::sin(angle)*radius;
            x=cx+across*std::cos(tilt)-along*std::sin(tilt);y=cy+across*std::sin(tilt)+along*std::cos(tilt);
        };
        glow(cx,cy,radius*0.9,0.05); // The faint body the points circle.
        for(unsigned n=0;n<8;++n) {
            const double angle=seconds*0.35+n*(6.283185307/8.0);
            const double depth=std::cos(angle+0.9); // Towards the viewer when positive.
            double x,y;at(angle,x,y);
            glow(x,y,height_*(0.040+0.010*depth),0.80+0.30*depth);
        }
    }
    // The disc in the top right corner: it blinks about once a second for four seconds after the disc
    // came in, then fades out within half a second and stays away.
    void discMark(double since) {
        static constexpr double blinking=4.0,fading=0.5;
        if(since>=blinking+fading)return;
        const double blink=0.25+0.75*(0.5+0.5*std::cos(since*6.283185307));
        const double fade=since<blinking?1.0:1.0-(since-blinking)/fading;
        spinningDisc(width_*0.92,height_*0.08,height_*0.035,since,blink*fade);
    }
    // A disc seen face on: a silver data side with rainbow reflections, fine grooves that turn with it,
    // a clear inner ring and the hole; `opacity` 0..1.
    void spinningDisc(double cx,double cy,double radius,double seconds,double opacity) {
        const double spin=seconds*5.0,squash=1.0,c=1.0,s=0.0;
        const int x0=std::max(int(cx-radius)-1,0),x1=std::min(int(cx+radius)+2,int(width_)),y0=std::max(int(cy-radius)-1,0),y1=std::min(int(cy+radius)+2,int(height_));
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x) {
            const double dx=x+0.5-cx,dy=y+0.5-cy;
            const double u=(dx*c+dy*s)/squash,v=-dx*s+dy*c; // Back into the disc's own plane.
            const double r=std::hypot(u,v)/radius;
            if(r>1.0 || r<0.13)continue;
            const double angle=std::atan2(v,u);
            const double edge=std::min(1.0,(1.0-r)*radius*0.5)*std::min(1.0,(r-0.13)*radius*0.5); // Soft outer and inner edges.
            auto& p=bits_[std::size_t(y)*width_+x];
            if(r<0.34) {
                // The clear hub: a faint grey ring, lighter at its middle band.
                const double band=r>0.22 && r<0.30?0.55:0.30;
                p=mix(p,0x00B8BCC8u,band*edge*opacity);continue;
            }
            // Silver, lit by two broad reflections fixed to the light, and colour bands that move with
            // the grooves as the disc turns.
            const double light=std::pow(std::abs(std::cos(angle-0.6)),6.0);
            const double grooves=0.5+0.5*std::sin(angle*18.0-spin+r*40.0);
            const double hue=std::fmod(r*3.0+angle/6.283185307+spin*0.02+10.0,1.0)*6.0;
            const double rr=std::clamp(std::abs(hue-3.0)-1.0,0.0,1.0),gg=std::clamp(2.0-std::abs(hue-2.0),0.0,1.0),bb=std::clamp(2.0-std::abs(hue-4.0),0.0,1.0);
            const double base=150+40*grooves*0.3;
            const double rainbow=90*light;
            const auto channel=[](double value){return std::uint32_t(std::clamp(value,0.0,255.0));};
            const std::uint32_t colour=(channel(base+rainbow*rr+60*light)<<16)|(channel(base+6+rainbow*gg+60*light)<<8)|channel(base+18+rainbow*bb+60*light);
            const double rim=r>0.97?0.6:1.0;
            p=mix(p,colour,0.92*edge*rim*opacity);
        }
    }
    // The other screens: a fan of translucent bars turning around a dark sphere.
    void fan(double seconds) {
        const double cx=width_*0.30,cy=height_*0.50,inner=height_*0.085,outer=height_*0.62;
        const double turn=seconds*0.08,slice=6.283185307/12.0;
        const int x0=std::max(int(cx-outer),0),x1=std::min(int(cx+outer)+1,int(width_)),y0=std::max(int(cy-outer),0),y1=std::min(int(cy+outer)+1,int(height_));
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x) {
            const double dx=x-cx,dy=y-cy,r=std::sqrt(dx*dx+dy*dy);
            auto& p=bits_[std::size_t(y)*width_+x];
            if(r<inner) {
                // The sphere: dark, with a rim of light on the upper left.
                const double rim=std::max(0.0,(-dx-dy)/(inner*1.4))*(r/inner)*(r/inner);
                p=mix(p,0x00060612u,0.92);p=lit(p,70*rim,80*rim,130*rim);
                continue;
            }
            if(r>=outer)continue;
            double a=std::fmod(std::atan2(dy,dx)+turn+62.83185307,slice)/slice; // 0..1 across one bar and its gap.
            if(a>0.42)continue;
            const double edge=std::min(a,0.42-a)/0.21,fade=1.0-(r-inner)/(outer-inner);
            const double alpha=0.34*fade*std::min(1.0,edge*3.0);
            p=mix(p,0x004656D8u,alpha);
        }
    }
    // A convex polygon blended over the picture (`alpha` 0..1).
    void blendConvex(const std::array<double,8>& xy,int count,std::uint32_t colour,double alpha) {
        double minx=1e9,maxx=-1e9,miny=1e9,maxy=-1e9;
        for(int i=0;i<count;++i) {minx=std::min(minx,xy[2*i]);maxx=std::max(maxx,xy[2*i]);miny=std::min(miny,xy[2*i+1]);maxy=std::max(maxy,xy[2*i+1]);}
        double area=0;for(int i=0;i<count;++i) {const int j=(i+1)%count;area+=xy[2*i]*xy[2*j+1]-xy[2*j]*xy[2*i+1];}
        const double sign=area>0?1.0:-1.0;
        for(int y=std::max(int(miny),0);y<=std::min(int(maxy),int(height_)-1);++y)for(int x=std::max(int(minx),0);x<=std::min(int(maxx),int(width_)-1);++x) {
            bool inside=true;
            for(int i=0;i<count && inside;++i) {
                const int j=(i+1)%count;
                inside=sign*((xy[2*j]-xy[2*i])*(y+0.5-xy[2*i+1])-(xy[2*j+1]-xy[2*i+1])*(x+0.5-xy[2*i]))>=0;
            }
            if(inside) {auto& p=bits_[std::size_t(y)*width_+x];p=mix(p,colour,alpha);}
        }
    }
    // Five translucent cubes on an arc at the left of the fan, each turning slowly; the one for the
    // chosen entry is light blue.
    void cubes(double seconds,unsigned chosen) {
        const double cx=width_*0.30,cy=height_*0.50,arc=height_*0.36,size=height_*0.058;
        static constexpr int faces[6][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
        for(unsigned n=0;n<5;++n) {
            const double place=3.14159265*(0.74+0.13*n)+0.04*std::sin(seconds*0.3);
            const double ox=cx+std::cos(place)*arc*0.95,oy=cy-height_*0.03+std::sin(place)*arc*1.0;
            const double a=seconds*0.45+n*1.3,b=0.6+n*0.4+seconds*0.2;
            double px[8],py[8],pz[8];
            for(int v=0;v<8;++v) {
                double x=(v&1?1:-1)*size,y=(v&2?1:-1)*size,z=(v&4?1:-1)*size;
                double t=x*std::cos(a)+z*std::sin(a);z=-x*std::sin(a)+z*std::cos(a);x=t;
                t=y*std::cos(b)-z*std::sin(b);z=y*std::sin(b)+z*std::cos(b);y=t;
                px[v]=ox+x;py[v]=oy+y;pz[v]=z;
            }
            const bool on=n==chosen%5;
            const std::uint32_t colour=on?0x0040B0FFu:0x00D8D4F0u;
            // The far faces first, faint; then the near ones.
            int order[6]={0,1,2,3,4,5};
            std::sort(order,order+6,[&](int l,int r){double zl=0,zr=0;for(int k=0;k<4;++k){zl+=pz[faces[l][k]];zr+=pz[faces[r][k]];}return zl>zr;});
            for(int f:order) {
                std::array<double,8> xy{};double depth=0;
                for(int k=0;k<4;++k) {xy[2*k]=px[faces[f][k]];xy[2*k+1]=py[faces[f][k]];depth+=pz[faces[f][k]];}
                // Faces turned up and towards the light are lighter, like the glass cubes of the console.
                const double ux=xy[2]-xy[0],uy=xy[3]-xy[1],vx=xy[6]-xy[0],vy=xy[7]-xy[1];
                const double facing=std::abs(ux*vy-uy*vx)/(size*size*4.0),upward=(xy[1]+xy[3]+xy[5]+xy[7])/4.0<oy?0.10:0.0;
                blendConvex(xy,4,colour,(on?0.30:0.14)+0.22*facing+upward+(depth<0?0.06:0.0));
            }
            // The edges, bright where they face the viewer.
            static constexpr int edges[12][2]={{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
            for(const auto& e:edges) {
                const bool facing_viewer=pz[e[0]]+pz[e[1]]<0;
                const COLORREF line=on?(facing_viewer?RGB(190,235,255):RGB(110,170,230)):(facing_viewer?RGB(236,232,252):RGB(150,146,190));
                const HPEN pen=CreatePen(PS_SOLID,1,line);const HGDIOBJ old=SelectObject(dc_,pen);
                MoveToEx(dc_,int(px[e[0]]),int(py[e[0]]),nullptr);LineTo(dc_,int(px[e[1]]),int(py[e[1]]));
                SelectObject(dc_,old);DeleteObject(pen);
            }
        }
    }
};
#endif
