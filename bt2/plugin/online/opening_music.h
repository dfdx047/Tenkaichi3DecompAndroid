#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// The music of the game's opening (DATA/ZS2USOP.SFD, the opening movie of the player's own disc) for Dragon Rage's
// Online Mode screen (main_menu_item.cpp): the movie's audio stream (MPEG program stream, stream 0xC0) is CRI ADX;
// a thread of its own takes it out of the movie while it plays, and each field's share is decoded and added to the
// field's sound (Bt2HostApi.set_music_mix), at 48 kHz, in a loop, faded in and out. Nothing of it is kept anywhere.
class OpeningMusic {
public:
    ~OpeningMusic();
    void start(const std::string& disc_image); // from its beginning, fading in
    void stop();                               // fading out
    // Adds this field's samples (stereo, interleaved) at `percent` of the game's own music level.
    void mix(std::int16_t* field, std::size_t samples, unsigned percent);

private:
    void demux(std::string disc_image);
    bool header();
    void decodeFrame();

    std::thread thread_;
    std::atomic<bool> quit_{false},complete_{false};
    std::mutex lock_;
    std::vector<std::uint8_t> adx_; // the stream so far
    bool playing_=false,stopping_=false,parsed_=false;
    unsigned channels_=0,rate_=0,data_=0,frame_bytes_=18;
    std::int32_t coef1_=0,coef2_=0;
    std::size_t at_=0;             // the next frame's offset
    std::int32_t hist_[2][2]{};
    std::vector<std::int16_t> pcm_; // decoded, not yet played (interleaved at the stream's rate)
    std::size_t pcm_at_=0;
    double phase_=0;
    int gain_=0;                    // the fade: 0 to half a second's samples
};

