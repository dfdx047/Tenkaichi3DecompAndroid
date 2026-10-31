#include "services/audio_mixer.h"
#include "services/sound_sampler.h"
#include <cmath>
#include <functional>
#include "services/iop_heap.h"
#include "services/pad_block.h"
#include "services/cri_stream.h"
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

// Native service logic that runs without game code: IOP heap placement and the pad block layout.
// The full request paths run in the startup probe.

namespace {
void require(bool condition,const char* what) {
    if(!condition){std::cerr<<"native services test failed: "<<what<<std::endl;std::exit(1);}
}
}

int main() {
    // Heap: the captured allocation/free sequence of the original boot heap.
    IopHeap heap;
    const auto first=heap.allocate(0x360);
    require(first==0xCE500,"first allocation");
    require(heap.release(first) && !heap.release(first),"free and double free");
    const std::vector<std::pair<std::uint32_t,std::uint32_t>> sequence={
        {0x81800,0xCE500},{0x8D0,0x14FD00},{0x41040,0x150600},{0x8D0,0x191700},{0x4040,0x192000},{0x360,0x196100}};
    for(const auto& [size,expected]:sequence)require(heap.allocate(size)==expected,"captured placement");
    require(heap.release(0x196100),"reload block free");
    require(heap.allocate(0x2C0)==0x196100 && heap.allocate(0x180)==0x196400,"bank blocks reuse the freed block");
    require(heap.covers(0xCE500,0x16640) && !heap.covers(0x196400,0x201) && !heap.covers(0xCE400,16),"upload coverage");
    require(!heap.allocate(0) && !heap.allocate(0x200000),"empty and oversized requests");

    // Pad block: first transfer zero; neutral, Left and Cross as DBCMAN wrote them.
    DualShock2 pad;
    for(auto w:nativePadBlock(pad,0))require(!w,"first transfer is zero");
    auto block=nativePadBlock(pad,1);
    require(block[0]==0x04121601 && block[1]==1 && block[7]==0x8080FFFF && block[8]==0x8080 && block[9]==0 && block[10]==0
        && block[11]==0xFFFF0000 && block[12]==0xFFFF && block[31]==1,"neutral block");
    pad.buttons=0xFF7F; // Left.
    block=nativePadBlock(pad,2);
    require(block[7]==0x8080FF7F && block[8]==0xFF008080 && block[31]==2,"left block");
    pad.buttons=0xBFFF; // Cross.
    block=nativePadBlock(pad,3);
    require(block[7]==0x8080BFFF && block[8]==0x8080 && block[10]==0xFF,"cross block");

    // CRI stream: the captured command pair (8 and 5 on one object) comes back with the count cleared
    // and the records unchanged; an empty buffer stays empty; unknown codes are refused.
    std::vector<std::uint8_t> stream(cri_stream_size);
    const auto put=[&](std::size_t offset,std::uint32_t value){std::memcpy(stream.data()+offset,&value,4);};
    put(0,2);put(0x10,8);put(0x14,0xC6D5C);put(0x18,1);put(0x20,5);put(0x24,0xC6D5C);put(0x2C,0x65);
    auto before=stream;before[0]=0;
    require(consumeCriStreamCommands(stream)==2 && stream==before,"CRI commands consumed");
    require(consumeCriStreamCommands(stream)==0 && stream==before,"CRI empty buffer");
    // The movie audio start: 8, 4 (rate), 9 twice (pan) are echoed the same way.
    put(0,4);put(0x10,8);put(0x18,0);put(0x20,4);put(0x28,0xBB80);put(0x30,9);put(0x34,0xC6D5C);put(0x3C,0xFFFFFFF1);
    put(0x40,9);put(0x44,0xC6D5C);put(0x48,1);put(0x4C,0xF);
    before=stream;before[0]=0;
    require(consumeCriStreamCommands(stream)==4 && stream==before,"CRI audio start consumed");
    // Audio data: 0x100 with the IOP address and size, one record per audio channel.
    put(0,2);put(0x10,0x100);put(0x14,0xCD578);put(0x18,0x179140);put(0x1C,0xA00);put(0x20,0x100);put(0x24,0xCD58C);put(0x28,0x17D240);put(0x2C,0xA00);
    before=stream;before[0]=0;
    require(consumeCriStreamCommands(stream)==2 && stream==before,"CRI audio data consumed");
    put(0,1);put(0x10,2);put(0x14,0xC6D5C);put(0x18,1);put(0x1C,0);
    before=stream;before[0]=0;
    require(consumeCriStreamCommands(stream)==1 && stream==before,"CRI command 2 consumed");
    put(0,1);put(0x10,7);bool refused=false;
    try{consumeCriStreamCommands(stream);}catch(const std::runtime_error&){refused=true;}
    require(refused,"unknown CRI command refused");

    {
        // Audio flow as logged from the original: two sources of one player, data announced each field,
        // and after the start the played blocks per field are 2,1,2,1,2,1,2,2,1,2,1,2,1,2,2,...
        CriAudioFlow flow;flow.sourceCreated(0xA,0x1A,0x3659C8);flow.sourceCreated(0xB,0x1B,0x3659DC);flow.playerCreated(0x50,0x1A,0x1B);
        std::vector<std::uint8_t> buffer(cri_stream_size);
        const auto word=[&](std::size_t offset,std::uint32_t value){std::memcpy(buffer.data()+offset,&value,4);};
        const auto at=[&](std::size_t offset){std::uint32_t value;std::memcpy(&value,buffer.data()+offset,4);return value;};
        std::uint32_t write_a=0x179140,write_b=0x17D240;
        const auto announce=[&](std::uint32_t bytes) {
            word(0,2);word(0x10,0x100);word(0x14,0xA);word(0x18,write_a);word(0x1C,bytes);word(0x20,0x100);word(0x24,0xB);word(0x28,write_b);word(0x2C,bytes);
            consumeCriStreamCommands(buffer,&flow,1);write_a+=bytes;write_b+=bytes;
        };
        announce(0xA00);flow.field();require(flow.played(1).empty(),"no messages before the start");
        std::vector<std::uint8_t> command(cri_stream_size);const std::uint32_t play[5]={1,0,0,0,2};
        std::memcpy(command.data(),play,4);std::memcpy(command.data()+0x10,&play[4],4);const std::uint32_t args[2]={0x50,1};std::memcpy(command.data()+0x14,args,8);
        consumeCriStreamCommands(command,&flow,0);
        const unsigned expected[]={2,1,2,1,2,1,2,2,1,2,1,2,1,2,2,1,2,1,2,1,2,1,2,2};
        std::uint32_t read_a=0x179140;
        for(const auto blocks:expected) {
            announce(0xA00);flow.field();
            const auto messages=flow.played(1);
            require(messages.size()==2 && messages[0].ee_object==0x3659C8 && messages[0].address==read_a && messages[0].bytes==blocks*0x400
                && messages[1].ee_object==0x3659DC && messages[1].address==read_a+0x4100 && messages[1].bytes==blocks*0x400,"played blocks per field");
            read_a+=blocks*0x400;
            writeCriStreamMessages(buffer,messages);
            require(at(0)==2 && at(0x10)==0 && at(0x14)==0x3659C8 && at(0x1C)==blocks*0x400 && at(0x24)==0x3659DC,"messages in the returned buffer");
        }
        {
            // A 24 kHz player (command 4) plays half-size blocks at the same block rate, as logged:
            // 0x400, 0x200, 0x400, 0x200 ... bytes per field.
            CriAudioFlow slow;slow.sourceCreated(1,0x11,0x365900);slow.playerCreated(9,0x11,0x12);
            std::vector<std::uint8_t> setup(cri_stream_size);const std::uint32_t records[8]={4,9,24000,0xFFFFFFF1u,2,9,1,0};
            const std::uint32_t two=2;std::memcpy(setup.data(),&two,4);std::memcpy(setup.data()+0x10,records,32);
            consumeCriStreamCommands(setup,&slow,0);
            slow.dataWritten(1,0x150740,0x2000,1);
            const unsigned halves[]={0x400,0x200,0x400,0x200};
            for(const auto bytes:halves) {slow.field();const auto m=slow.played(1);require(m.size()==1 && m[0].bytes==bytes,"24 kHz blocks");}
            bool odd=false;try{slow.rate(9,44100);}catch(const std::runtime_error&){odd=true;}
            require(odd,"unmodelled rate refused");
        }
        // Writes that are not contiguous (the ring wraps) are reported as separate stretches.
        CriAudioFlow wrap;wrap.sourceCreated(1,0x11,0x100);wrap.sourceCreated(2,0x12,0x200);wrap.playerCreated(9,0x11,0x12);
        wrap.dataWritten(1,0x17CD40,0x400,1);wrap.dataWritten(1,0x179140,0x400,1);wrap.dataWritten(2,0x180E40,0x400,1);wrap.dataWritten(2,0x17D240,0x400,1);
        wrap.play(9,1);wrap.field();
        const auto split=wrap.played(1);
        require(split.size()==4 && split[0].address==0x17CD40 && split[1].address==0x179140 && split[2].ee_object==0x200 && split[3].address==0x17D240,"wrapped stretches");
        // A player without announced data reports nothing; later data plays at the block rate from then on.
        wrap.field();require(wrap.played(1).empty(),"a starved player reports nothing");
        wrap.dataWritten(1,0x179540,0xA00,1);wrap.dataWritten(2,0x17D640,0xA00,1);wrap.field();
        const auto resumed=wrap.played(1);
        require(resumed.size()==2 && resumed[0].address==0x179540 && resumed[0].bytes<=0x800 && resumed[0].bytes==resumed[1].bytes,"data after starvation");
        // Stop ends playback and reports everything still announced, split where the ring wraps (as
        // logged: {17C540, C00} then {179140, 2000}); other arguments are refused.
        CriAudioFlow stop;stop.sourceCreated(1,0x11,0x100);stop.playerCreated(9,0x11,0x12);stop.play(9,1);
        stop.dataWritten(1,0x17C540,0xC00,1);stop.dataWritten(1,0x179140,0x2000,1);stop.play(9,0);stop.field();
        const auto flushed=stop.played(1);
        require(flushed.size()==2 && flushed[0].address==0x17C540 && flushed[0].bytes==0xC00 && flushed[1].address==0x179140 && flushed[1].bytes==0x2000,"stop reports the remaining data");
        stop.field();require(stop.played(1).empty(),"a stopped player reports nothing more");
        {
            // A start still queued when its player was destroyed (a skipped film) is dropped; a start
            // for a player that never existed is refused. Paired sources are told from single ones.
            CriAudioFlow gone;gone.sourceCreated(1,0x11,0x100);gone.sourceCreated(2,0x12,0x200);gone.sourceCreated(3,0x21,0x300);
            gone.playerCreated(9,0x11,0x12);gone.playerCreated(8,0x21,0x22);gone.sourceCreated(4,0x22,0x400);
            gone.dataWritten(1,0x1000,0x400,1);gone.dataWritten(2,0x2000,0x400,1);gone.dataWritten(3,0x3000,0x400,1);
            require(gone.paired(1) && gone.paired(2) && !gone.paired(77),"two sources on one player given data are a pair");
            // A one-channel stream: its partner source on the player's other buffer is never given data.
            require(!gone.paired(3),"a source whose partner gets no data is one channel");
            for(unsigned n=0;n<61;++n)gone.field();
            require(!gone.paired(1),"a partner silent for a second is no longer a pair");
            gone.playerDestroyed(9);gone.play(9,1);gone.play(9,0);
            require(gone.commands_for_destroyed_players==2,"commands for a destroyed player are dropped");
            bool never=false;try{gone.play(7,1);}catch(const std::runtime_error&){never=true;}
            require(never,"a start for a player that never existed is refused");
        }
        {
            // Sound output: played blocks are passed on with source, side and rate; what a stop only
            // reports is not, and a destroyed player silences its sources.
            CriAudioFlow heard;heard.sourceCreated(1,0x11,0x100);heard.sourceCreated(2,0x12,0x200);heard.playerCreated(9,0x11,0x12);heard.rate(9,24000);
            struct Piece {std::uint32_t source;unsigned side;std::uint32_t hertz,address,bytes;};std::vector<Piece> pieces;std::vector<std::uint32_t> silenced;
            heard.on_audible=[&](std::uint32_t source,unsigned side,std::uint32_t hertz,std::uint32_t address,std::uint32_t bytes){pieces.push_back({source,side,hertz,address,bytes});};
            heard.on_silenced=[&](std::uint32_t source){silenced.push_back(source);};
            heard.dataWritten(1,0x1000,0x1000,1);heard.dataWritten(2,0x3000,0x1000,1);heard.play(9,1);heard.field();
            const auto first=heard.played(1);
            require(first.size()==2 && pieces.size()==2 && pieces[0].source==1 && pieces[0].side==0 && pieces[0].hertz==24000 && pieces[0].address==0x1000
                && pieces[0].bytes==first[0].bytes && pieces[1].source==2 && pieces[1].side==1 && pieces[1].address==0x3000,"audible stretches");
            heard.play(9,0);heard.field();
            require(heard.played(1).size()==2 && pieces.size()==2,"a stop's report is not heard");
            // A one-channel stream on another player (its second source never given data) is heard in both ears.
            heard.sourceCreated(5,0x31,0x500);heard.sourceCreated(6,0x32,0x600);heard.playerCreated(10,0x31,0x32);
            heard.dataWritten(5,0x5000,0x400,1);heard.play(10,1);heard.field();(void)heard.played(1);
            require(pieces.size()==3 && pieces[2].source==5 && pieces[2].side==2,"a one-channel stream is heard on both sides");
            heard.play(9,1);heard.dataWritten(1,0x1000,0x400,1);heard.playerDestroyed(9);
            require(silenced==std::vector<std::uint32_t>{1,2},"a destroyed player silences its sources");
        }
        {
            // Mixer: a voice starts with one block of silence; 24 kHz is doubled with interpolation;
            // sides are routed left and right; five fields give 4,004 samples; a dry voice counts
            // its missing samples and starts over.
            AudioMixer mixer;
            std::vector<std::uint8_t> pcm(4000);
            for(std::size_t n=0;n<pcm.size();n+=2){const std::int16_t v=std::int16_t(1000+n);std::memcpy(pcm.data()+n,&v,2);}
            mixer.push(7,0,48000,pcm);
            std::vector<std::uint8_t> slow(4);const std::int16_t pair[2]={200,400};std::memcpy(slow.data(),pair,4);
            mixer.push(8,1,24000,slow);
            const auto out=mixer.field();
            require(out.size()==800*2 && out[0]==0 && out[511*2]==0 && out[512*2]==390 && out[513*2]==391,"left voice after its silent block, at the stream level");
            require(out[511*2+1]==0 && out[512*2+1]==39 && out[513*2+1]==78 && out[514*2+1]==117 && out[515*2+1]==156 && out[516*2+1]==0,"right voice doubled with interpolation");
            require(mixer.underruns==800-516,"the short voice ran dry");
            std::size_t samples=out.size()/2;for(unsigned n=0;n<4;++n)samples+=mixer.field().size()/2;
            require(samples==4004,"five fields are 4,004 samples");
            {
                AudioMixer both;std::vector<std::uint8_t> one(2048,0);const std::int16_t v=6400;for(std::size_t n=0;n<one.size();n+=2)std::memcpy(one.data()+n,&v,2);
                both.push(3,2,48000,one);const auto o=both.field();
                require(o[600*2]==2500 && o[600*2+1]==2500,"a voice on both sides is heard equally in each");
            }
            std::vector<std::uint8_t> loud(2);const std::int16_t top=30000;std::memcpy(loud.data(),&top,2);
            AudioMixer sum;sum.push(1,0,48000,loud);sum.push(2,0,48000,loud);
            AudioMixer three;for(unsigned key=1;key<=3;++key)three.push(key,0,48000,loud);
            require(sum.field()[512*2]==2*(30000*25/64) && three.field()[512*2]==32767,"voices on one side add and saturate");
            bool refused=false;try{sum.push(3,0,44100,loud);}catch(const std::runtime_error&){refused=true;}
            require(refused,"a rate that does not divide 48 kHz is refused");
            sum.drop(1);
            // Further sources are added before clamping.
            AudioMixer plus;require(plus.field([](std::span<std::int32_t> mix){mix[0]=70000;mix[1]=-5;})[0]==32767,"extra sources join the mix");
            // The player's volumes: streams, further sources and the whole mix, each in percent.
            const auto level=[&](unsigned stream,unsigned extra,unsigned master) {
                AudioMixer m;m.stream_percent=stream;m.extra_percent=extra;m.master_percent=master;
                m.push(1,0,48000,loud);
                return m.field([](std::span<std::int32_t> mix){mix[512*2]+=1000;})[512*2];
            };
            require(level(100,100,100)==30000*25/64+1000,"full volume is the measured level");
            require(level(50,100,100)==30000*25/64/2+1000 && level(100,50,100)==30000*25/64+500,"streams and further sources separately");
            require(level(100,100,50)==(30000*25/64+1000)/2 && level(0,0,100)==0,"the whole mix; silence at zero");
        }
        {
            // SPU2 ADPCM: shift and filter per block; the end flag stops, the repeat flag marks a loop.
            std::vector<std::uint8_t> block(16);block[0]=0x0C;block[1]=1;block[2]=0x21;block[3]=0x0F;
            auto sample=decodeSpuAdpcm(block);
            require(sample.pcm.size()==28 && sample.pcm[0]==1 && sample.pcm[1]==2 && sample.pcm[2]==-1 && sample.pcm[3]==0 && !sample.loops,"plain block");
            block[0]=0x10;block[2]=0x01;block[3]=0;sample=decodeSpuAdpcm(block);
            require(sample.pcm[0]==4096 && sample.pcm[1]==3840 && sample.pcm[2]==3600,"first-order filter");
            std::vector<std::uint8_t> two(32);two[1]=4;two[16+1]=3;sample=decodeSpuAdpcm(two);
            require(sample.pcm.size()==56 && sample.loops && sample.loop_start==0,"loop flags");
            bool refused=false;two[16+1]=0;try{decodeSpuAdpcm(two);}catch(const std::runtime_error&){refused=true;}
            require(refused,"a sample without an end flag is refused");
            refused=false;block[0]=0x50;try{decodeSpuAdpcm(block);}catch(const std::runtime_error&){refused=true;}
            require(refused,"an undefined filter is refused");
        }
        {
            // A synthetic bank in the layout of the game's banks: two samples, two timbres, two sounds.
            const auto put=[](std::vector<std::uint8_t>& d,std::size_t at,std::uint32_t v){std::memcpy(d.data()+at,&v,4);};
            constexpr std::uint32_t scei=0x53434549;
            std::vector<std::uint8_t> head(0x200),sequences(0x100);
            put(head,0,scei);put(head,4,0x56657273);put(head,0x10,scei);put(head,0x14,0x48656164);
            put(head,0x20,0x40);for(std::size_t at=0x24;at<0x30;at+=4)put(head,at,UINT32_MAX);
            put(head,0x30,0x50);put(head,0x34,0x80);
            put(head,0x50,scei);put(head,0x54,0x56616769);put(head,0x5C,1);put(head,0x60,0x18);put(head,0x64,0x20);
            put(head,0x68,0x10);put(head,0x6C,0xFF00BB80);          // Sample 0: offset 0x10, 48,000 Hz.
            put(head,0x70,0x20);put(head,0x74,0xFF015DC0);          // Sample 1: offset 0x20, 24,000 Hz, looping.
            put(head,0x80,scei);put(head,0x84,0x53657462);put(head,0x90,0x14);
            head[0x94+12]=0x32;head[0x94+14]=1;                      // Two timbres of 0x32 bytes from set + 16.
            head[0xA4+0]=0;head[0xA4+3]=100;head[0xA4+4]=64;
            head[0xA4+0x32]=1;head[0xA4+0x32+3]=100;head[0xA4+0x32+4]=64;
            put(sequences,0,scei);put(sequences,4,0x56657273);put(sequences,0x10,scei);put(sequences,0x14,0x53657175);put(sequences,0x28,0x30);
            put(sequences,0x30,scei);put(sequences,0x34,0x53657371);sequences[0x44]=127;sequences[0x45]=64;put(sequences,0x54,2);
            put(sequences,0x64,0x50);put(sequences,0x68,0x70);put(sequences,0x6C,UINT32_MAX); // Sound 2 is absent.
            for(unsigned n=0;n<2;++n) {
                const std::size_t at=0x80+n*0x20;put(sequences,at,0x10);sequences[at+4]=100;sequences[at+5]=64;put(sequences,at+8,7);
                const std::uint8_t data[7]={0,0x90,0,std::uint8_t(n),100,0,0xFF};std::memcpy(sequences.data()+at+0x10,data,7);
            }
            const auto bank=SoundBank::parse(head,sequences);
            require(bank.sample_bytes==0x40 && bank.sounds.size()==3 && bank.sounds[0] && bank.sounds[1] && !bank.sounds[2],"bank sounds");
            require(bank.sounds[0]->sample_offset==0x10 && bank.sounds[0]->rate==48000 && !bank.sounds[0]->loop && bank.sounds[0]->set_volume==127
                && bank.sounds[0]->sequence_volume==100 && bank.sounds[0]->velocity==100 && bank.sounds[0]->timbre_volume==100
                && bank.sounds[1]->sample_offset==0x20 && bank.sounds[1]->rate==24000 && bank.sounds[1]->loop,"bank fields");
            auto broken=sequences;broken[0x80+0x11]=0x80; // Not a note-on.
            bool refused=false;try{SoundBank::parse(head,broken);}catch(const std::runtime_error&){refused=true;}
            require(refused,"a sequence that is not a single note-on is refused");
            auto programs=head;put(programs,0x24,0x100);refused=false;try{SoundBank::parse(programs,sequences);}catch(const std::runtime_error&){refused=true;}
            require(refused,"a bank with program chunks is refused");

            // Sampler: the bank's samples lie in sound memory; a request plays a sound at the product
            // of its volumes and half scale, panned with constant power.
            SoundSampler sampler;
            std::vector<std::uint8_t> memory(0x40);memory[0x10]=0x10;memory[0x11]=1;memory[0x12]=0x01; // 4096, 3840, 3600, ...
            memory[0x20]=0x10;memory[0x21]=3;memory[0x22]=0x01;
            sampler.upload(0x1000,memory);sampler.load(0x1000,head,sequences);
            require(sampler.banks()==1 && !sampler.voices(),"bank loaded");
            sampler.play(0,0,127,64,0);
            std::vector<std::int32_t> mix(8);sampler.render(mix);
            const double level=100.0/127*100/127*100/127*0.5;
            require(std::abs(mix[0]-4096*level*std::cos(64/127.0*1.5707963267948966))<=1 && mix[1]>mix[0] && mix[1]-mix[0]<12
                && std::abs(double(mix[2])/mix[0]-3840.0/4096)<0.01 && sampler.played==1 && sampler.voices()==1,"voice level, pan and order");
            sampler.play(0,0,127,0,1200); // Hard left, one octave up: every second sample.
            std::vector<std::int32_t> left(4);SoundSampler solo;solo.upload(0x1000,memory);solo.load(0x1000,head,sequences);solo.play(0,0,127,0,1200);solo.render(left);
            require(left[1]==0 && std::abs(left[0]-4096*level)<=1 && std::abs(left[2]-3600*level)<=1,"pan to one side and pitch");
            std::vector<std::int32_t> rest(200);sampler.render(rest);require(!sampler.voices(),"voices end with their sample");
            // Sound 1 loops: it plays until its kept entry is stopped, then fades out.
            sampler.play(0x80,1,127,0,0);
            std::vector<std::int32_t> loop(400);sampler.render(loop);
            require(sampler.voices()==1 && sampler.looping==1 && sampler.kept==1 && loop[0]!=0 && loop[112]==loop[0] && loop[398]!=0,"a looping sample repeats from its loop start");
            sampler.play(0x40,0,0,0,0);require(sampler.stops_unmatched==1 && sampler.voices()==1,"a stop for a sound that is not kept changes nothing");
            sampler.play(0x40,1,0,0,0);require(sampler.stopped==1,"a stop finds the kept sound");
            std::vector<std::int32_t> tail(2*SoundSampler::release_samples+8);sampler.render(tail);
            require(!sampler.voices() && tail[0]!=0 && std::abs(tail[2*SoundSampler::release_samples-56])<std::abs(tail[0])/4 && tail[2*SoundSampler::release_samples]==0,"a stopped voice fades and ends");
            sampler.play(0x40,1,0,0,0);require(sampler.stops_unmatched==2,"a second stop finds nothing");
            // Slot volume and silencing.
            sampler.play(0,1,127,0,0);sampler.setVolume(1,0x80);
            std::vector<std::int32_t> half(2);sampler.render(half);require(std::abs(half[0]-loop[0]/2)<=1,"the slot volume scales playing voices");
            sampler.setVolume(2,0);sampler.render(half);require(half[0]!=0,"other slots' volumes do not apply");
            sampler.silence(2);require(sampler.silenced==0,"silencing another slot changes nothing");
            sampler.silence(1);sampler.render(tail);require(sampler.silenced==1 && !sampler.voices(),"a silenced slot's voices end");
            sampler.setVolume(0xFF,SoundSampler::full_volume);
            refused=false;try{sampler.play(0xC0,1,127,64,0);}catch(const std::runtime_error&){refused=true;}
            require(refused,"keep and stop together are refused");
            for(const auto& bad:{std::function<void()>([&]{sampler.play(1,0,127,64,0);}),std::function<void()>([&]{sampler.play(0,2,127,64,0);}),std::function<void()>([&]{sampler.play(0,0,128,64,0);})}) {
                refused=false;try{bad();}catch(const std::runtime_error&){refused=true;}
                require(refused,"requests outside the loaded banks are refused");
            }
            // Replacing a slot's bank (SOUNDS 0x80): the old bank's voices end, the count stays.
            sampler.play(0,1,127,64,0);sampler.replace(0,0x1000,head,sequences);
            require(sampler.banks()==1 && !sampler.voices(),"replacement keeps the count and ends the old bank's voices");
            sampler.play(0,0,127,64,0);require(sampler.voices()==1,"the replaced slot plays its new bank");
            refused=false;try{sampler.replace(1,0x1000,head,sequences);}catch(const std::runtime_error&){refused=true;}
            require(refused,"replacing a slot that holds no bank is refused");
            sampler.play(0,0,127,64,0);sampler.unload();require(!sampler.banks() && !sampler.voices(),"unload removes the bank and its voices");
            refused=false;try{sampler.unload();}catch(const std::runtime_error&){refused=true;}
            require(refused,"unload without a bank is refused");
        }
        wrap.play(9,0);
        // A player destroyed while playing (a skipped movie) hands its announced data back once, as a
        // stop does, and then reports nothing; queued commands for it are dropped.
        wrap.play(9,1);wrap.dataWritten(1,0x179540,0x800,1);wrap.playerDestroyed(9);wrap.field();
        const auto returned=wrap.played(1);
        std::uint32_t handed_back=0;for(const auto& m:returned)if(m.address<=0x179540 && m.address+m.bytes>=0x179540+0x800)handed_back=0x800;
        require(handed_back==0x800,"a destroyed player's data is reported back");
        wrap.field();require(wrap.played(1).empty(),"and nothing after that");
        wrap.play(9,0);
        wrap.play(9,1);wrap.field(); // A start queued before the destruction is dropped (seen when a film is skipped).
        require(wrap.played(1).empty() && wrap.commands_for_destroyed_players==2,"a destroyed player does not start");
        wrap.playerCreated(9,0x11,0x12);
        bool refused_argument=false;try{wrap.play(9,2);}catch(const std::runtime_error&){refused_argument=true;}
        require(refused_argument,"unmodelled player argument refused");
    }
    std::cout<<"Native IOP heap, pad block and CRI stream checks passed."<<std::endl;
}
