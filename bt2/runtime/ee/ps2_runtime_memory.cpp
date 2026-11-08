#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ee/frame_profile.h"
#include "ee_runtime.h"
#include "dma_chain.h"

// EeRuntime guest memory access and memory-mapped I/O (DMA, GIF/VIF1, timers, GS privileged registers).
// Kept out of the header so edits rebuild this unit, not every generated unit that includes it.

void EeRuntime::write32(std::uint8_t* rdram, std::uint32_t address, std::uint32_t value) {
    if(auto* spr=scratchpadAt(address,4)) {std::memcpy(spr,&value,4);return;}
    address = uncachedWordAddress(rdram, address);
    if(address==0x10003000 && vif_startup.phase==5 && value==1 && !vif_startup.pending_mask) {
        checkMemory(rdram,0,0);
        if(gif_dma.starts!=2 || gif_dma.completions!=2 || gif_dma.qwc)throw UnsupportedGuestMemory(address,4);
        ++vif_startup.gif_resets;vif_startup.phase=6;return; // Idle GIF reset, not GS VRAM/drawing-state reset.
    }
    // The generated DMAC setup writes zero to STAT in its return delay slot.
    // STAT is W1C/toggle, so zero preserves the existing interrupt mask.
    if(address==0x1000E010 && dmac_startup_writes==2 && value==0 && heap_frees==1) {
        checkMemory(rdram,0,0);++dmac_startup_writes;return;
    }
    // Observed Timer 2 startup store only; no counter advancement or scheduling.
    if (address == 0xB0001010 && timer2_mode_supplied_ && timer2_mode_reads == 1
            && !timer2_mode_writes && value == 0x382) {
        checkMemory(rdram, 0, 0);
        timer2_mode_after_write = (timer2_mode_ & 0xC00 & ~value) | (value & 0x3FF);
        ++timer2_mode_writes;
        ++word_writes;
        return;
    }
    if (!startup_kernel_payload_.empty() && address >= 0x80075000 && address < 0x80075330) {
        if (memory_.empty() || rdram != memory_.data() || address % 4)
            throw UnsupportedGuestMemory(address, 4);
        const auto offset = address - 0x80075000;
        std::uint32_t expected;
        std::memcpy(&expected, startup_kernel_payload_.data() + offset, 4);
        if (value != expected || kernel_patch_words_.contains(address - 0x80000000))
            throw std::runtime_error("Startup kernel payload store differs or repeats");
        kernel_patch_words_[address - 0x80000000] = value;
        ++startup_kernel_word_writes;
        ++kernel_word_writes;
        ++word_writes;
        return;
    }
    if (address >= kernel_patch_start_ && address < kernel_patch_end_) {
        if (!kernel_patch_end_ || memory_.empty() || rdram != memory_.data() || address % 4
                || kernel_patch_end_ - address < 4) throw UnsupportedGuestMemory(address, 4);
        kernel_patch_words_[address - 0x80000000] = value;
        ++kernel_word_writes;
        ++word_writes;
        return;
    }
    checkMemory(rdram, address, 4);
    if (address % 4) throw std::runtime_error("Unsupported unaligned word store");
    std::memcpy(memory_.data() + address, &value, 4);
    ++word_writes;
}

void EeRuntime::indexedDataCache(std::uint8_t* rdram, R5900Context* ctx,
                          std::uint32_t operation, std::uint32_t address) {
    const auto begin=static_cast<std::uint32_t>(ctx->r[4].low);
    const auto end=static_cast<std::uint32_t>(ctx->r[5].low);
    const auto way=address&1u;
    if ((address&~1u)>0xFC0 || (address&0x3Eu) || begin>end
        || (begin&63u) || (end&63u) || (ctx->cop0_status&0x10000u))
        throw std::runtime_error("Unsupported indexed cache range/status");
    checkMemory(rdram,begin,static_cast<std::size_t>(end)-begin+64u);
    if (operation==0x10u) {
        const auto slot=indexed_cache_tag_reads%128u;
        const auto expected=(slot/2u)*64u+(slot&1u);
        if (indexed_cache_tag_ready || address!=expected
            || (ctx->pc!=(way?0x32F500u:0x32F4C4u) && ctx->pc!=(way?0x32F640u:0x32F604u)))
            throw std::runtime_error("Indexed cache scan order differs");
        indexed_cache_tag_ready=true;
    } else if (operation==0x14u || operation==0x16u) {
        // Invalid coherent tags cannot select a nonzero owned EE range.
        throw std::runtime_error("Unexpected indexed writeback for invalid native tag");
    } else throw std::runtime_error("Unsupported indexed CACHE operation");
    ++indexed_cache_operations;
}

void EeRuntime::frameGifChainStart(std::uint32_t value) {
    // Source chain (MOD 1, no tag transfer): the data of every tag, in order, is PATH3 GIF data.
    // GIF packets may continue across tags, so the data is joined and sent as one GS transaction.
    auto& g=frame_gif;
    if(g.qwc || (value&0xC0) || vif1.r.mskpath3)throw UnsupportedGuestMemory(0x1000A000,4); // Pending MADR data, TTE or TIE; PATH3 masked.
    const auto chain=walkDmaSourceChain(std::span<const std::uint8_t>(memory_.data(),ram_size),g.tadr,"GIF");
    std::vector<std::uint8_t> data;
    for(const auto& c:chain.segments)data.insert(data.end(),c.data.begin(),c.data.end());
    if(!data.empty())gif_dma.gs.transfer(data);
    const auto& last=chain.segments.back();
    std::uint64_t last_tag;std::memcpy(&last_tag,memory_.data()+last.tag,8);
    g.madr=last.data.empty()?g.madr:std::uint32_t(last.data.data()+last.data.size()-memory_.data());
    g.tadr=chain.next_tadr;g.bytes+=data.size();g.chcr=(std::uint32_t(last_tag)&0xFFFF0000u)|(value&0xFEFFu);
    ++g.starts;++g.chain_starts;
}

void EeRuntime::frameVif1Start(std::uint32_t value) {
    auto& v=frame_vif1;
    if(value!=0x145 || v.qwc)throw std::runtime_error("Unsupported VIF1 DMA start");
    // Walk the whole chain first. Each DMA tag contributes its upper 64 bits (two VIF codes) and then
    // its data qwords. The list is checked as a whole, so a code that is not modelled stops at the
    // channel start before any effect; then it runs in order: UNPACK and MPG fill VU1 memory,
    // microprograms run to their end and their XGKICK packets (PATH1) reach the GS between the
    // DIRECT data (PATH2).
    const auto chain=walkDmaSourceChain(std::span<const std::uint8_t>(memory_.data(),ram_size),v.tadr,"VIF1");
    // The list's words and the first word index and address of each tag; the buffers are kept between lists.
    // (Sized once and copied with memcpy: inserting the words one range at a time copied them one word at a time,
    // about 2% of the game thread in a fight on Android, measured. BT2_PERF_BASELINE: as before, to measure against.)
    auto& words=vif1_list_words_;auto& tags=vif1_list_tags_;tags.clear();
    static const bool baseline=std::getenv("BT2_PERF_BASELINE")!=nullptr;
    if(baseline) {
        words.clear();
        const auto append=[&](const std::uint8_t* bytes,std::size_t count) {
            const auto* from=reinterpret_cast<const std::uint32_t*>(bytes);words.insert(words.end(),from,from+count);
        };
        for(const auto& c:chain.segments) {
            tags.emplace_back(words.size(),c.tag);
            append(c.tag_words,2);append(c.data.data(),c.data.size()/4);
        }
    } else {
        std::size_t total=0;
        for(const auto& c:chain.segments)total+=2+c.data.size()/4;
        // (not cleared first: resize from empty wrote zeros into every word, which the copies below then wrote again; now only a longer list
        // than the last one writes zeros, into its new end)
        words.resize(total);
        std::size_t at=0;
        for(const auto& c:chain.segments) {
            tags.emplace_back(at,c.tag);
            std::memcpy(words.data()+at,c.tag_words,8);at+=2;
            const std::size_t count=c.data.size()/4;
            if(count)std::memcpy(words.data()+at,c.data.data(),count*4);
            at+=count;
        }
    }
    if(!v.starts) {
        // The registers as the startup FIFO writes left them.
        auto& r=vif1.r;const auto& s=vif_startup;
        r.cycle=s.cycle;r.mode=s.mode;r.mask=s.mask;r.base=s.base;r.ofst=s.offset;r.itops=s.itops;r.tops=s.tops;
    }
    auto blocked=[&](std::uint32_t code,std::uint32_t tag) {
        v.blocked_code=code;v.blocked_tag=tag;v.blocked_tadr=v.tadr;++v.unsupported;
        throw UnsupportedGuestMemory(0x10009000,4); // Checked boundary at the channel start.
    };
    const auto codes=vif1.codes,directs=vif1.directs;const auto direct_qwords=vif1.direct_qwords;
    const Vu1::Kick to_gs=[this](std::span<const std::uint8_t> data) {FrameProfile::Scope timed(frame_profile.gs),of_vu1(frame_profile.vu1_gs);gif_dma.gs.transfer(data);};
    if(on_vif1_transfer)on_vif1_transfer(words);
    try {FrameProfile::Scope timed(frame_profile.vif1);vif1.check(words);vif1_running=true;struct Done {bool& flag;~Done(){flag=false;}} done{vif1_running};vif1.run(words,to_gs,to_gs);}
    catch(const Vif1Unsupported& u) {
        std::uint32_t tag=v.tadr;
        for(const auto& [first,address]:tags)if(first<=u.index)tag=address;
        std::cerr<<u.what()<<'\n';blocked(u.code,tag);
    } catch(const std::runtime_error& e) {
        // The GS model refused a packet or a microprogram reached an instruction that is not modelled.
        std::cerr<<"VIF1 display list stopped: "<<e.what()<<'\n';blocked(0x50000000,v.tadr);
    }
    v.tadr=chain.next_tadr;v.tags+=unsigned(chain.segments.size());v.codes+=vif1.codes-codes;
    v.directs+=vif1.directs-directs;v.direct_qwords+=vif1.direct_qwords-direct_qwords;
    v.chcr=value&~0x100u;v.cis=true;++v.starts;
}

void EeRuntime::countDisplayFlip() {
    ++gs_display_flips;
    if(on_display_flip)on_display_flip(); // Host presentation of the completed flip (viewer mode).
    // Probe frame budget: stop after this completed flip as a checked boundary.
    if(frame_limit && gs_display_flips==frame_limit) {frame_budget_reached=true;throw UnsupportedGuestMemory(0x120000E0,8);}
}

void EeRuntime::Store64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value) {
    checkMemory(rdram,0,0);
    if(address==0x12001000 && value==0x200) {gsCsrReset();return;} // CSR RESET (ee_gs_privileged.cpp).
    if(frameGifContext() && display_reset_phase==13 && !frame_gs_reset_step && gsReadbackStore64(address,value))return;
    // Privileged GS display registers after display setup (main's thread): PMODE, SMODE2, DISPFB1/2,
    // DISPLAY1/2, EXTWRITE (off only) and BGCOLOR are recorded as display state for presentation.
    // CRT timing registers stay with SetGsCrt. A flip is counted at the library flip's last store
    // (BGCOLOR), or at the VBLANK ending a field whose display update did not include one (the
    // per-field update used while a movie plays).
    if(current_thread_id==1 && !in_interrupt_ && display_reset_phase==13 && address>=0x12000000 && address<=0x120000E0 && !(address&15)) {
        const auto n=(address-0x12000000)/16;
        if(n!=0 && n!=2 && n!=7 && n!=8 && n!=9 && n!=10 && n!=13 && n!=14)throw UnsupportedGuestMemory(address,8);
        if(n==13 && value)throw UnsupportedGuestMemory(address,8); // EXTWRITE: write-back circuit stays off.
        if(!gs_display_flips && context->pc!=0x3118D0 && !gs_display_writes)throw UnsupportedGuestMemory(address,8); // First write is the library flip.
        gs_privileged[n]=value;++gs_display_writes;
        if(n==14)countDisplayFlip(); // The library flip ends with BGCOLOR.
        return;
    }
    throw UnsupportedGuestMemory(address,8);
}

std::uint64_t EeRuntime::Load64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
    checkMemory(rdram,0,0);
    // IPU registers are 64-bit (IPU_CMD/IPU_TOP carry BUSY in bit 63): low word, then high word.
    if(address>=0x10002000 && address<0x10002040 && !(address&15)) {
        std::uint32_t low=0,high=0;
        if(ipuRead(address,low) && ipuRead(address+4,high))return low|(std::uint64_t(high)<<32);
    }
    if(address==0x12001000 && (displayResetStep(context,5,0x3104F4,640) || displayResetStep(context,7,0x3104F4,80))) {
        // Only FIELD (bit 13) is consumed, after this wait's polled VBlank.
        if(polled_vblanks!=(display_reset_phase==5?1u:2u) || (display_intc_stat&4))throw UnsupportedGuestMemory(address,8); // One acknowledged VBlank per wait.
        ++display_reset_phase;if(display_reset_phase==6)clear_repeat_gif=gif_dma.repeat();
        return 0x551B4004|(display_field?0x2000u:0u);
    }
    if(address==0x12001000 && gs_field_supplied_ && gs_interrupt.acknowledgements==2 && !gs_field_reads
        && context->pc==0x3104F4 && context->r[29].low==initial_sp-640) {
        ++gs_field_reads;return 0x551B6004; // Independent captured field, not timed native GS state.
    }
    if(address==0x12001000) {
        if(!gs_csr_cleared)++frame_csr_reads;
        return gsCsrRead(); // ee_gs_privileged.cpp
    }
    throw UnsupportedGuestMemory(address,8);
}

void EeRuntime::Store32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint32_t value) {
    if(auto* ram=ramWord(rdram,address)) {std::memcpy(ram,&value,4);++word_writes;return;} // Main RAM first, as in Load32.
    if(ipuWrite(address,value)) {checkMemory(rdram,0,0);return;}
    if(address==0x10000810 && context->pc==0x28C770 && context->r[29].low==initial_sp-112
        && texture_gif.phase==8 && !timer1_setup_writes && value==3) {
        checkMemory(rdram,0,0);timer1_mode=value;timer1.configure(value);++timer1_setup_writes;return; // Disabled Timer 1 configuration only.
    }
    if(frameGifContext() && (address==0x10009000 || address==0x10009010 || address==0x10009020 || address==0x10009030 || (address==0x1000E010 && value==2))) {
        checkMemory(rdram,0,0);auto& v=frame_vif1;
        if(address==0x1000E010) {v.cis=false;return;} // W1C of CIS1 only.
        if(v.chcr&0x100)throw UnsupportedGuestMemory(address,4);
        if(address==0x10009020) {if(value>0xFFFF)throw UnsupportedGuestMemory(address,4);v.qwc=value;return;}
        if(address==0x10009010) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);v.madr=value&0x0FFFFFFF;return;} // No scratchpad transfers.
        if(address==0x10009030) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);v.tadr=value&0x0FFFFFFF;return;}
        // Normal mode (the GS readback sequence): 0x101 sends VIF codes from memory, 0x100 receives
        // local->host data; a display list is a source chain with tag transfer (0x145).
        if(value==0x101)frameVif1Send(value);else if(value==0x100)frameVif1Receive(value);else frameVif1Start(value);
        return;
    }
    if(frameGifContext() && gsReadbackStore32(address,value)) {checkMemory(rdram,0,0);return;}
    if(frameGifContext() && (address==0x1000A000 || address==0x1000A010 || address==0x1000A020 || address==0x1000A030)) {
        checkMemory(rdram,0,0);auto& g=frame_gif;
        if(g.chcr&0x100)throw UnsupportedGuestMemory(address,4); // Writes while a transfer runs.
        if(address==0x1000A020) {if(value>0xFFFF)throw UnsupportedGuestMemory(address,4);g.qwc=value;return;}
        if(address==0x1000A010) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);g.madr=value&0x0FFFFFFF;return;}
        if(address==0x1000A030) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);g.tadr=value&0x0FFFFFFF;return;}
        // CHCR bits 16-31 (TAG) are read-only: a read-modify-write start carries the last tag back.
        if((value&0xFFFF)==0x105) {frameGifChainStart(value);return;}
        if((value&0xFFFF)!=0x101 || vif1.r.mskpath3 || !g.qwc || g.madr>=ram_size || std::size_t(g.qwc)*16>ram_size-g.madr)throw UnsupportedGuestMemory(address,4);
        gif_dma.gs.transfer(std::span<const std::uint8_t>(memory_.data()+g.madr,std::size_t(g.qwc)*16));
        g.bytes+=std::size_t(g.qwc)*16;g.madr+=g.qwc*16;g.qwc=0;g.chcr=value&0xFEFFu;++g.starts;return;
    }
    // Main's H-BLANK delay helpers (0x28C778 start, 0x28C790 wait >= 400 and stop): logical Timer 1.
    if(timer1_setup_writes==1 && current_thread_id==1 && !in_interrupt_) {
        if(address==0x10000810 && context->pc==0x28C78C && value==(timer1.mode()|0x80)) {checkMemory(rdram,0,0);timer1.writeMode(value);++timer1_starts;timer1_wait_polls=0;return;}
        if(address==0x10000810 && context->pc==0x28C7CC && value==(timer1.mode()&~0x80u) && timer1.counting()) {checkMemory(rdram,0,0);timer1.writeMode(value);++timer1_stops;return;}
        if(address==0x10000800 && context->pc==0x28C7D4 && !value && !timer1.counting()) {checkMemory(rdram,0,0);timer1.writeCount(0);return;}
    }
    // Frame phase: other Timer 1 users (the game's counter allocator 0x2D87E8 sets BUSCLK/256 0x82).
    if(timer1_setup_writes==1 && gs_display_flips>=1 && !in_interrupt_ && (address==0x10000800 || address==0x10000810)) {
        checkMemory(rdram,0,0);
        try {if(address==0x10000810)timer1.reconfigure(value);else timer1.setCount(value);}
        catch(const std::runtime_error&) {throw UnsupportedGuestMemory(address,4);}
        ++timer1_frame_writes;return;
    }
    if(texture_gif.supplied && graph_startup_phase==4 && context->r[29].low==initial_sp-128) {
        if(address==0x1000A020 && context->pc==0x28B96C) {checkMemory(rdram,0,0);texture_gif.count(value);return;}
        if(address==0x1000A010 && context->pc==0x28B978) {checkMemory(rdram,0,0);texture_gif.address(value,initial_sp-112,memory());return;}
        if(address==0x1000A000 && context->pc==0x28B98C) {checkMemory(rdram,0,0);texture_gif.start(value,memory(),gif_dma.gs);return;}
    }
    if(dmac_startup_writes==3 && heap_frees==1 && native_module_loads==14 && context->r[29].low==initial_sp-32) {
        if(address==0x10003C10 && context->pc==0x30FF70 && !vif_startup.phase && value==1) {
            checkMemory(rdram,0,0);vif_startup.phase=1;return;
        }
        if(address==0x10003C20 && context->pc==0x30FF80 && vif_startup.phase==1 && value==2) {
            checkMemory(rdram,0,0);vif_startup.error=value;vif_startup.phase=2;return;
        }
    }
    if(heap_frees==1 && native_module_loads==14 && context->r[29].low==initial_sp-32) {
        if(address==0x1000E000 && context->pc==0x28B2B4 && !dmac_startup_writes && value==1) {
            checkMemory(rdram,0,0);dmac_control=value;++dmac_startup_writes;return;
        }
        if(address==0x1000E020 && context->pc==0x28B2B8 && dmac_startup_writes==1 && value==0x83FF0002) {
            checkMemory(rdram,0,0);dmac_priority=value;++dmac_startup_writes;return;
        }
    }
    if((address==0xB0001010 || address==0xB0001020) && context->pc==0x338B30 && timer2Context(context)) {
        checkMemory(rdram,0,0);if(address==0xB0001010)timer2.writeMode(value);else timer2.writeComp(value);return;
    }
    if(address==0x1000F000 && value==4 && (displayVsyncWait(context,0x32F10C,0) || displayVsyncWait(context,0x32F160,0))) {
        checkMemory(rdram,0,0);display_intc_stat&=~value;return; // Write-one-to-clear VBLANK_START.
    }
    if(clear_repeat_gif && display_reset_phase==6 && context->r[29].low==initial_sp-624 && displayResetStep(context,6,context->pc,624)) {
        if(address==0x1000A000 && context->pc==0x3101A8) {checkMemory(rdram,0,0);clear_repeat_gif->start(value,memory(),gif_dma.gs);return;}
        if(address==0x1000A020 && context->pc==0x310148) {checkMemory(rdram,0,0);clear_repeat_gif->setCount(value);return;}
        if(address==0x1000A010 && context->pc==0x310194) {
            checkMemory(rdram,0,0);clear_repeat_gif->setAddress(value,initial_sp-(clear_repeat_gif->starts?288u:528u),memory());return;
        }
    }
    if(address==0x1000F000 && gs_interrupt_supplied_ && gs_crt_calls==1
        && context->r[29].low==initial_sp-656 && value==4
        && ((context->pc==0x32F10C && !gs_interrupt.acknowledgements)
            || (context->pc==0x32F160 && gs_interrupt.acknowledgements==1))) {
        checkMemory(rdram,0,0);
        if(!gs_interrupt.acknowledgements) {
            if(intc_mask!=0x1802)throw std::runtime_error("INTC mask differs from checkpoint");
            gs_interrupt.arm(12,intc_mask);
        }
        gs_interrupt.acknowledge(value);return;
    }
    if(address==0x1000F000 && mainGameContext()) {
        checkMemory(rdram,0,0);display_intc_stat&=~value;return; // Write-one-to-clear (the VBLANK bits are modelled).
    }
    if(gif_dma_supplied_ && gs_field_reads==1 && context->r[29].low==initial_sp-624) {
        if(address==0x1000A000 && context->pc==0x3101A8) {
            checkMemory(rdram,0,0);gif_dma.start(value,memory());return;
        }
        if(address==0x1000A020 && context->pc==0x310148) {
            checkMemory(rdram,0,0);gif_dma.setCount(value);return;
        }
        if(address==0x1000A010 && context->pc==0x310194) {
            checkMemory(rdram,0,0);gif_dma.setAddress(value,initial_sp-(gif_dma.starts?288u:528u),memory());return;
        }
    }
    write32(rdram, address, value);
}

std::uint32_t EeRuntime::Load32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
    // Main RAM first: none of the device and checkpoint cases below is a RAM address.
    if(const auto* ram=ramWord(rdram,address)) {std::uint32_t value;std::memcpy(&value,ram,4);return value;}
    if(std::uint32_t value=0;ipuRead(address,value)) {checkMemory(rdram,0,0);return value;}
    if(frameGifContext() && (address==0x1000A000 || address==0x1000A010 || address==0x1000A020 || address==0x1000A030)) {
        checkMemory(rdram,0,0);if(context->pc==0x3105A4)++frame_syncs;
        return address==0x1000A000?frame_gif.chcr:address==0x1000A010?frame_gif.madr:address==0x1000A020?frame_gif.qwc:frame_gif.tadr;
    }
    // Frame phase: GIF and VIF1 transfers complete synchronously and no VU1 work is started, so
    // channel and path status reads see idle (VIF1_STAT 0x40 and GIF_STAT 0 as captured at the
    // startup path sync).
    if(frameGifContext() && !(frame_gif.chcr&0x100) && !(frame_vif1.chcr&0x100)) {
        if(address==0x10009000 || address==0x10009020 || address==0x10009030) {
            checkMemory(rdram,0,0);return address==0x10009000?frame_vif1.chcr:address==0x10009020?frame_vif1.qwc:frame_vif1.tadr;
        }
        if(address==0x10003C00) {checkMemory(rdram,0,0);return vif1Status();} // DBF follows the microprogram starts.
        if(address==0x10003020) {checkMemory(rdram,0,0);return 0;}
    }
    if(timer1_setup_writes==1 && current_thread_id==1 && !in_interrupt_) {
        if(address==0x10000810 && (context->pc==0x28C780 || context->pc==0x28C7C0)) {checkMemory(rdram,0,0);return timer1.mode();}
        if(address==0x10000800 && context->pc==0x28C798 && timer1.counting()) {
            // Logical time: each poll of the delay loop waits one line; a crossed field boundary
            // delivers that VBLANK_START first (interrupt handlers and woken threads run).
            checkMemory(rdram,0,0);
            ++timer1_polls;if(++timer1_wait_polls>100000)throw UnsupportedGuestMemory(address,4); // One wait never ends.
            const auto target=timer1.nextLine();
            if(timer1.crossesField(target)) {deliverVblank(rdram,context);++timer1_vblanks;}
            timer1.lineTo(target);return timer1.count();
        }
    }
    if(timer1_setup_writes==1 && gs_display_flips>=1 && !in_interrupt_ && (address==0x10000800 || address==0x10000810)) {
        checkMemory(rdram,0,0);return address==0x10000800?timer1.count():timer1.mode(); // Reads do not advance time.
    }
    if((address==0x10001000 || address==0x10001010) && timer2Context(context)) {
        checkMemory(rdram,0,0);return address==0x10001000?timer2.count():timer2.mode();
    }
    if(address==0x1000F000 && displayVsyncWait(context,0x32F130,0)) {
        checkMemory(rdram,0,0);
        // Logical time: a poll that finds VBLANK_START clear advances one VBlank (START and END latch).
        // Cause 2 is disabled here, so no handler runs; FIELD toggles in interlaced mode.
        if(!(display_intc_stat&4)) {display_intc_stat|=0xC;display_field=!display_field;gs_csr_cleared=false;++polled_vblanks;advanceLogicalField();}
        return display_intc_stat;
    }
    if(clear_repeat_gif && displayResetStep(context,6,context->pc,624)) {
        auto& g=*clear_repeat_gif;
        if(address==0x1000A000 && context->pc==0x3100E4) {checkMemory(rdram,0,0);return g.readIdle();}
        if(address==0x10009000 && context->pc==0x310560) {checkMemory(rdram,0,0);return g.readVifIdle();}
        if(address==0x1000A000 && (context->pc==0x3105A4 || context->pc==0x3105CC)) {checkMemory(rdram,0,0);return g.readStarted(context->pc==0x3105CC);}
        if(address==0x10003C00 && context->pc==0x3105EC) {checkMemory(rdram,0,0);return g.readVifStatus();}
        if(address==0x10003020 && context->pc==0x310664) {
            checkMemory(rdram,0,0);const auto status=g.readGifStatus();
            if(g.starts==2)display_reset_phase=7; // Both clear requests completed.
            return status;
        }
    }
    if(texture_gif.supplied && graph_startup_phase==4 && context->r[29].low==initial_sp-128) {
        if(address==0x1000A000 && context->pc==0x28B928) {checkMemory(rdram,0,0);return texture_gif.idle();}
        if(address==0x10003020 && context->pc==0x28B944) {checkMemory(rdram,0,0);return texture_gif.pathIdle();}
        if(address==0x1000A000 && context->pc==0x28B998) {checkMemory(rdram,0,0);return texture_gif.poll();}
        if(address==0x10003020 && context->pc==0x28B9B4) {checkMemory(rdram,0,0);return texture_gif.pathComplete();}
    }
    if(post_sif.supplied && gif_dma.completions==2 && iop_epoch==1 && context->r[29].low==initial_sp-192) {
        if(address==0x1000E010 && context->pc==0x3319CC) {checkMemory(rdram,0,0);return post_sif.status();}
        if(address==0x1000C000 && context->pc==0x3319EC) {checkMemory(rdram,0,0);return post_sif.channel();}
    }
    if(gif_dma.syncSupplied() && gif_dma.starts>=1 && gif_dma.starts<=2 && context->r[29].low==initial_sp-624) {
        if(address==0x10009000 && context->pc==0x310560) {checkMemory(rdram,0,0);return gif_dma.readVifIdle();}
        if(address==0x1000A000 && (context->pc==0x3105A4 || context->pc==0x3105CC)) {
            checkMemory(rdram,0,0);return gif_dma.readStarted(context->pc==0x3105CC);
        }
        if(address==0x10003020 && context->pc==0x310664) {checkMemory(rdram,0,0);return gif_dma.readGifStatus();}
        if(address==0x10003C00 && context->pc==0x3105EC) {checkMemory(rdram,0,0);return gif_dma.readVifStatus();}
    }
    if(address==0x1000A000 && gif_dma_supplied_ && gs_field_reads==1 && context->pc==0x3100E4
        && context->r[29].low==initial_sp-624) {
        checkMemory(rdram,0,0);return gif_dma.readIdle();
    }
    if(address==0x1000F000 && gs_interrupt_supplied_ && context->pc==0x32F130
        && context->r[29].low==initial_sp-656) {
        checkMemory(rdram,0,0);return gs_interrupt.poll();
    }
    if(address==0x1000F000 && mainGameContext()) {
        // Logical time: main polling for VBLANK_START that finds it clear receives the next VBlank.
        checkMemory(rdram,0,0);if(!(display_intc_stat&4))deliverVblank(rdram,context);return display_intc_stat;
    }
    if (address == 0x10001810 && timer3_mode_supplied_ && !timer3_mode_reads) {
        checkMemory(rdram, 0, 0);
        ++timer3_mode_reads;
        return timer3_mode_;
    }
    if (address == 0x10001010 && timer2_mode_supplied_ && !timer2_mode_reads) {
        checkMemory(rdram, 0, 0);
        ++timer2_mode_reads;
        return timer2_mode_;
    }
    if (address == 0x10001010 && timer2_readback_supplied_ && timer2_mode_reads == 1 && timer2_mode_writes == 1) {
        checkMemory(rdram, 0, 0);
        if (timer2_mode_after_write != timer2_readback_expected_)
            throw std::runtime_error("Timer 2 modeled mode differs from captured readback");
        ++timer2_mode_reads;
        return timer2_mode_after_write;
    }
    return read32(rdram, address);
}

std::uint32_t EeRuntime::read32(std::uint8_t* rdram, std::uint32_t address) {
    if(auto* spr=scratchpadAt(address,4)) {std::uint32_t value;std::memcpy(&value,spr,4);return value;}
    address = uncachedWordAddress(rdram, address);
    if (address == 0x1000E010 && dma_status_supplied_ && !dma_status_reads) {
        checkMemory(rdram, 0, 0);
        ++dma_status_reads;
        return dma_status_word_;
    }
    if (address == 0x1000C000 && dma5_control_supplied_ && dma_status_reads == 1 && !dma5_control_reads) {
        checkMemory(rdram, 0, 0);
        ++dma5_control_reads;
        return dma5_control_word_;
    }
    // Read-only KSEG0 view of locally supplied kernel bytes; no fabricated kernel.
    if (address >= 0x80000000 && address < 0x80000000 + kernel_snapshot_size) {
        const auto offset = address - 0x80000000;
        if (kernel_snapshot_.empty() || memory_.empty() || rdram != memory_.data()
            || offset % 4 || offset > kernel_snapshot_.size() - 4)
            throw UnsupportedGuestMemory(address, 4);
        if (const auto patched = kernel_patch_words_.find(offset); patched != kernel_patch_words_.end())
            return patched->second;
        std::uint32_t value;
        std::memcpy(&value, kernel_snapshot_.data() + offset, 4);
        // BT2_SELF_STARTUP: the baseline is not the BIOS's (game/startup_inputs.h); each word of it the
        // game reads is named once, to learn which of the BIOS's words the game needs.
        if (log_kernel_reads && kernel_reads_logged_.insert(offset).second)
            std::fprintf(stderr, "Kernel baseline word read at 0x%08X (pc 0x%08X): 0x%08X\n", address, lastCallTarget(), value);
        return value;
    }
    checkMemory(rdram, address, 4);
    if (address % 4) throw std::runtime_error("Unsupported unaligned word read");
    std::uint32_t value;
    std::memcpy(&value, memory_.data() + address, sizeof(value));
    return value;
}

bool EeRuntime::ipuRead(std::uint32_t address,std::uint32_t& value) {
    if(!ipuContext())return false;
    if(address==0x1000F520) {value=dmac_enabler;return true;}
    if(const auto spr=scratchpad_dma.read32(address)) {value=*spr;return true;}
    const auto ipu_value=ipu.read32(address);
    if(!ipu_value)return false;
    value=*ipu_value;return true;
}

bool EeRuntime::ipuWrite(std::uint32_t address,std::uint32_t value) {
    if(!ipuContext())return false;
    if(address==0x1000F590) {
        if((value&~0x10000u)!=(dmac_enabler&~0x10000u))throw UnsupportedGuestMemory(address,4);
        if(value&0x10000u)++dmac_suspends;
        dmac_enabler=value;return true; // D_ENABLEW mirrors into D_ENABLER; no channel is running.
    }
    if((address==0x1000D000 || address==0x1000D400) && (value&0x100u)) {scratchpadTransfer(address,value);return true;}
    ipu.ram=std::span<const std::uint8_t>(memory_.data(),ram_size);
    ipu.output_ram=std::span<std::uint8_t>(memory_.data(),ram_size);ipu.output_scratchpad=scratchpad_;
    try {return scratchpad_dma.write32(address,value) || ipu.write32(address,value);}
    catch(const std::runtime_error& e) {std::cerr<<"IPU refused: "<<e.what()<<std::endl;throw UnsupportedGuestMemory(address,4);}
}

// Scratchpad DMA in normal mode (MOD 0), completed synchronously: fromSPR (0x1000D000) copies QWC
// quadwords from SADR to main RAM at MADR; toSPR (0x1000D400) copies the other way, or in source chain
// mode (MOD 1) copies each tag's data from TADR's chain to SADR onwards. Other modes, tag transfer,
// transfers that wrap the 16 KB scratchpad and channel interrupts are refused.
void EeRuntime::scratchpadTransfer(std::uint32_t address,std::uint32_t value) {
    auto& c=scratchpad_dma.channels.at(address);
    const bool from_spr=address==0x1000D000;
    if(!from_spr && (value&0xFFu)==0x05 && !c.qwc) {
        const auto chain=walkDmaSourceChain(std::span<const std::uint8_t>(memory_.data(),ram_size),c.tadr,"toSPR");
        std::uint32_t sadr=c.sadr;
        for(const auto& segment:chain.segments) {
            if(sadr+segment.data.size()>scratchpad_size)throw UnsupportedGuestMemory(address,4);
            std::copy(segment.data.begin(),segment.data.end(),scratchpad_.begin()+sadr);sadr+=std::uint32_t(segment.data.size());
        }
        c.sadr=sadr&0x3FF0u;c.tadr=chain.next_tadr;c.chcr=value&~0x100u;++scratchpad_transfers;
        return;
    }
    const std::uint32_t madr=c.madr&0x0FFFFFF0u,bytes=c.qwc*16;
    if(((value>>2)&3) || (value&0xC0u) || !c.qwc || (c.madr&0x80000000u) || c.madr%16 || madr+bytes>ram_size || c.sadr+bytes>scratchpad_size)
        throw UnsupportedGuestMemory(address,4);
    auto ram=memory().subspan(madr,bytes);auto spr=scratchpad().subspan(c.sadr,bytes);
    if(from_spr)std::copy(spr.begin(),spr.end(),ram.begin());else std::copy(ram.begin(),ram.end(),spr.begin());
    c.madr+=bytes;c.sadr=(c.sadr+bytes)&0x3FF0u;c.qwc=0;c.chcr=value&~0x100u;++scratchpad_transfers;
}
