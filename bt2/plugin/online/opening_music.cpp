#include "opening_music.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace {
constexpr unsigned out_rate=48000;
// The level of the game's own streams on the output (services/audio_mixer.h: stream_level_num / stream_level_den).
constexpr std::int32_t level_num=25,level_den=64;
constexpr int fade_samples=out_rate/2; // half a second

std::uint32_t be16(const std::uint8_t* p) {return std::uint32_t(p[0])<<8|p[1];}
std::uint32_t be32(const std::uint8_t* p) {return be16(p)<<16|be16(p+2);}
std::uint32_t le32(const std::uint8_t* p) {return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}

// A file's extent in an ISO 9660 image: the path's directories from the root, then the file (names as the disc has
// them, ";1" included). {0, 0}: not there.
std::pair<std::uint64_t,std::uint64_t> isoFile(std::ifstream& in,const std::vector<std::string>& path) {
    std::vector<std::uint8_t> sector(2048);
    in.seekg(16*2048);if(!in.read(reinterpret_cast<char*>(sector.data()),2048) || std::memcmp(sector.data()+1,"CD001",5))return {0,0};
    std::uint64_t lba=le32(sector.data()+156+2),size=le32(sector.data()+156+10);
    for(const auto& name:path) {
        std::vector<std::uint8_t> dir(size);
        in.seekg(std::streamoff(lba*2048));if(size>16*1024*1024 || !in.read(reinterpret_cast<char*>(dir.data()),std::streamsize(size)))return {0,0};
        bool found=false;
        for(std::size_t at=0;at<dir.size();) {
            const unsigned n=dir[at];
            if(!n){at=(at/2048+1)*2048;continue;}
            if(at+n>dir.size() || n<34)break;
            if(std::string(reinterpret_cast<char*>(dir.data()+at+33),dir[at+32])==name) {lba=le32(dir.data()+at+2);size=le32(dir.data()+at+10);found=true;break;}
            at+=n;
        }
        if(!found)return {0,0};
    }
    return {lba*2048,size};
}
}

OpeningMusic::~OpeningMusic() {quit_=true;if(thread_.joinable())thread_.join();}

void OpeningMusic::start(const std::string& disc_image) {
    if(!thread_.joinable() && !disc_image.empty())thread_=std::thread(&OpeningMusic::demux,this,disc_image);
    std::lock_guard<std::mutex> hold(lock_);
    playing_=true;stopping_=false;gain_=0;
    at_=data_;pcm_.clear();pcm_at_=0;phase_=0;std::memset(hist_,0,sizeof hist_);
}

void OpeningMusic::stop() {std::lock_guard<std::mutex> hold(lock_);stopping_=true;}

// The movie's audio stream, out of its MPEG-1 program stream: packs (0xBA), and packets with a 16-bit length; the
// payload of stream 0xC0 after its header (stuffing, buffer size, time stamps).
void OpeningMusic::demux(std::string disc_image) {
    std::ifstream in(disc_image,std::ios::binary); // (a handle of its own: the game's disc reads are not held up)
    const auto [offset,size]=isoFile(in,{"DATA","ZS2USOP.SFD;1"});
    if(!size) {std::fprintf(stderr,"Online Mode: no opening movie on the disc, no music\n");complete_=true;return;}
    std::vector<std::uint8_t> buffer;std::uint64_t read=0;std::size_t at=0;
    const auto have=[&](std::size_t bytes) {
        while(buffer.size()-at<bytes && read<size && !quit_) {
            buffer.erase(buffer.begin(),buffer.begin()+std::ptrdiff_t(at));at=0;
            const std::size_t chunk=std::size_t(std::min<std::uint64_t>(1<<20,size-read));const std::size_t old=buffer.size();
            buffer.resize(old+chunk);in.seekg(std::streamoff(offset+read));
            if(!in.read(reinterpret_cast<char*>(buffer.data()+old),std::streamsize(chunk))){buffer.resize(old);read=size;break;}
            read+=chunk;
        }
        return buffer.size()-at>=bytes;
    };
    while(!quit_ && have(6)) {
        const std::uint8_t* p=buffer.data()+at;
        if(p[0]||p[1]||p[2]!=1){++at;continue;}
        const unsigned id=p[3];
        if(id==0xB9)break;
        if(id==0xBA){if(!have(14))break;p=buffer.data()+at;at+=(p[4]&0xC0)==0x40?14+(p[13]&7):12;continue;}
        if(id<0xBB){++at;continue;}
        const std::size_t length=be16(p+4);
        if(!have(6+length))break;
        p=buffer.data()+at;
        if(id==0xC0) {
            std::size_t h=6;
            if((p[h]&0xC0)==0x80) h+=3+p[h+2]; // (an MPEG-2 header)
            else {
                while(h<6+length && p[h]==0xFF)++h;
                if((p[h]&0xC0)==0x40)h+=2;
                if((p[h]&0xF0)==0x20)h+=5; else if((p[h]&0xF0)==0x30)h+=10; else if(p[h]==0x0F)++h;
            }
            if(h<6+length) {std::lock_guard<std::mutex> hold(lock_);adx_.insert(adx_.end(),p+h,p+6+length);}
        }
        at+=6+length;
    }
    complete_=true;
    std::lock_guard<std::mutex> hold(lock_);
    std::fprintf(stderr,"Online Mode: the opening's music, %zu bytes of ADX\n",adx_.size());
}

// The ADX header: 0x8000, the data's offset - 4, encoding 3, frames of 18 bytes of 4-bit samples, the channels, the
// rate, the high-pass frequency the prediction is made from.
bool OpeningMusic::header() {
    if(parsed_)return true;
    if(adx_.size()<32)return false;
    const std::uint8_t* h=adx_.data();
    const unsigned start=be16(h+2)+4;
    if(h[0]!=0x80 || h[1] || adx_.size()<start)return false;
    channels_=h[7];rate_=be32(h+8);frame_bytes_=h[5];
    const double cutoff=be16(h+16);
    if(h[4]!=3 || frame_bytes_!=18 || h[6]!=4 || !channels_ || channels_>2 || !rate_ || rate_>out_rate) {
        std::fprintf(stderr,"Online Mode: the opening's music is not plain ADX (encoding %u, %u channels, %u Hz)\n",h[4],channels_,rate_);
        channels_=0;parsed_=true;return true;
    }
    const double a=std::sqrt(2.0)-std::cos(2.0*M_PI*cutoff/rate_),b=std::sqrt(2.0)-1.0,c=(a-std::sqrt((a+b)*(a-b)))/b;
    coef1_=std::int32_t(std::floor(c*8192.0));coef2_=std::int32_t(std::floor(c*c*-4096.0));
    data_=start;at_=start;parsed_=true;
    std::fprintf(stderr,"Online Mode: the opening's music, %u channel(s) at %u Hz\n",channels_,rate_);
    return true;
}

// One frame of each channel: 32 samples, a 16-bit scale, then nibbles; each sample the scaled nibble plus the
// prediction from the two before it. The end of the stream (a frame whose scale has its top bit, or no more data
// once all of it is in) goes back to the beginning.
void OpeningMusic::decodeFrame() {
    if(at_+channels_*frame_bytes_>adx_.size() || (be16(adx_.data()+at_)&0x8000)) {
        if(!complete_)return; // (not out of the movie yet)
        at_=data_;std::memset(hist_,0,sizeof hist_);
        if(at_+channels_*frame_bytes_>adx_.size())return;
    }
    const std::size_t base=pcm_.size();pcm_.resize(base+32*channels_);
    for(unsigned ch=0;ch<channels_;++ch) {
        const std::uint8_t* f=adx_.data()+at_+ch*frame_bytes_;
        const std::int32_t scale=std::int32_t(be16(f)&0x1FFF);
        for(unsigned i=0;i<32;++i) {
            std::int32_t n=(f[2+i/2]>>(i&1?0:4))&15;if(n>=8)n-=16;
            std::int32_t s=n*scale+((coef1_*hist_[ch][0]+coef2_*hist_[ch][1])>>12);
            s=std::clamp(s,-32768,32767);hist_[ch][1]=hist_[ch][0];hist_[ch][0]=s;
            pcm_[base+i*channels_+ch]=std::int16_t(s);
        }
    }
    at_+=channels_*frame_bytes_;
}

void OpeningMusic::mix(std::int16_t* field,std::size_t samples,unsigned percent) {
    std::lock_guard<std::mutex> hold(lock_);
    if(!playing_ || !header() || !channels_)return;
    const double step=double(rate_)/out_rate;
    for(std::size_t n=0;n+1<samples;n+=2) {
        while(pcm_.size()/channels_<pcm_at_+2) {
            const std::size_t before=pcm_.size();decodeFrame();
            if(pcm_.size()==before)return; // (nothing to play yet)
        }
        gain_=std::clamp(gain_+(stopping_?-1:1),0,fade_samples);
        if(stopping_ && !gain_){playing_=false;return;}
        const double frac=phase_;
        for(unsigned side=0;side<2;++side) {
            const unsigned ch=channels_==2?side:0;
            const double a=pcm_[pcm_at_*channels_+ch],b=pcm_[(pcm_at_+1)*channels_+ch];
            const std::int32_t sample=std::int32_t(a+(b-a)*frac)*level_num/level_den*std::int32_t(percent)/100*gain_/fade_samples;
            field[n+side]=std::int16_t(std::clamp(std::int32_t(field[n+side])+sample,-32768,32767));
        }
        phase_+=step;
        while(phase_>=1.0){phase_-=1.0;++pcm_at_;}
        if(pcm_at_>4096) {pcm_.erase(pcm_.begin(),pcm_.begin()+std::ptrdiff_t(pcm_at_*channels_));pcm_at_=0;}
    }
}
