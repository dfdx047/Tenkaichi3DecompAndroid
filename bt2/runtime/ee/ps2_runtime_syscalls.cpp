#include "ee_runtime.h"
#include <cstdlib>

// EeRuntime EE syscall and SIF RPC dispatch (handleSyscall): admits each reached kernel call and
// routes RPC requests to the owned service handlers.
// Kept out of the header so edits rebuild this unit, not every generated unit that includes it.

void EeRuntime::handleSyscall(std::uint8_t* rdram, R5900Context* context, std::uint32_t code) {
    const auto selector = static_cast<std::uint32_t>(context->r[3].low);
    if (code == 0 && selector < syscall_handlers.size() && syscall_handlers[selector]) {
        const auto handler = syscall_handlers[selector];
        if (!functions_.contains(handler)) throw UnsupportedSyscall(selector, context->pc, handler);
        // Narrow host callback model: preserve wrapper RA, validate native callback return.
        // Does not emulate BIOS exception entry, privilege transitions or ERET.
        const auto saved_ra = context->r[31];
        const auto resume = context->pc;
        constexpr std::uint32_t callback_return = 0xFFFFFFF8;
        context->r[31].low = callback_return;
        ++custom_syscall_calls;
        try {
            dispatchGuestBranch(rdram, context, handler, resume, callback_return,
                                GuestBranchKind::DirectCall, "registered syscall callback");
        } catch (...) { context->r[31] = saved_ra; throw; }
        context->r[31] = saved_ra;
        context->pc = resume;
        return;
    }
    if(code==0 && selector==0x73 && !in_interrupt_) {
        // SetVSyncFlag: at the next VBLANK_START the kernel writes 1 to the flag word and the GS CSR to the
        // doubleword at csr (deliverVblank); a later call replaces a pending one.
        const auto flag=static_cast<std::uint32_t>(context->r[4].low),csr=static_cast<std::uint32_t>(context->r[5].low);
        if((flag&3) || (csr&7))throw UnsupportedSyscall(selector,context->pc);
        checkMemory(rdram,flag,4);checkMemory(rdram,csr,8);
        vsync_flag=flag;vsync_csr=csr;return; // Void BIOS API preserves V0 in the original capture.
    }
    if(code==0 && in_interrupt_) {
        if(handleInterruptSyscall(rdram,selector,context))return;
        throw UnsupportedSyscall(selector,context->pc); // Interrupt context fails closed.
    }
    // Thread calls (ee_thread_scheduler.cpp): main on its own stack, or a worker the scheduler runs.
    if(code==0 && selector==0x29 && ee_sound_dispatch_probe && current_thread_id==1 && context->r[4].low==1 && context->r[5].low==24)
        probeReadySoundThread(rdram,context); // Diagnostic (BT2_EE_SOUND_DISPATCH_PROBE); always stops.
    // The sound thread's first SleepThread is the startup's sound dispatch (below), not yet a scheduler wait.
    if(code==0 && (current_thread_id==1 ? !startup_scheduler_active : active_ee_stack!=nullptr)
        && !(selector==0x32 && current_thread_id==3 && !startup_scheduler_active) && handleThreadSyscall(rdram,selector,context)) return;
    if(code==0 && main_priority_admissions && !startup_scheduler_active && current_thread_id==1
        && handleAdmittedIntcSyscall(rdram,selector,context)) return;
    // Native SIF RPC services (runtime/services/) answer the game's requests and DMA status polls.
    if(code==0 && selector==0x77 && handleNativeSifDma(rdram,context))return;
    if(code==0 && selector==0x76 && handleNativeDmaStat(context))return;
    // Captured replies for the CD and LOADFILE requests around the IOP reset.
    if(code==0 && selector==0x77 && post_cd.supplied && post_sif.rpc_initialized && iop_epoch==1
        && (post_cd.binds<2 || !post_cd.calls)) {receiveFreshCd(rdram,context);return;}
    if(code==0 && selector==0x77 && post_loadfile.supplied && post_cd.modes==1 && post_sif.rpc_initialized && iop_epoch==1
        && (!post_loadfile.binds || !post_loadfile.versions)) {receiveFreshLoadfile(rdram,context);return;}
    if(code==0 && selector==0x77 && post_cd.scmd_supplied && post_cd.calls==1
        && (!post_cd.scmd_binds || !post_cd.modes)) {receiveFreshScmd(rdram,context);return;}
    if(code==0 && post_sif.supplied && gif_dma.completions==2 && iop_epoch==1) {
        if(selector==0x78 && post_sif.phase==2) {
            post_sif.chain();return; // SDK declares void, retain v0.
        }
        if(selector==0x7A) {context->r[2].low=post_sif.get(static_cast<std::uint32_t>(context->r[4].low));return;}
        if(selector==0x79) {post_sif.set(static_cast<std::uint32_t>(context->r[4].low),static_cast<std::uint32_t>(context->r[5].low));context->r[2].low=context->r[5].low;return;}
        if(selector==0x77 && (post_sif.phase==10 || post_sif.phase==12)) {
            auto d=static_cast<std::uint32_t>(context->r[4].low);checkMemory(rdram,d,16);
            auto source=read32(rdram,d),count=read32(rdram,d+8);
            if(context->r[5].low!=1 || read32(rdram,d+4)!=0x1E640 || read32(rdram,d+12)!=0x44 || source!=(post_sif.phase==10?0x3A9580u:0x3A9800u))
                throw UnsupportedSyscall(selector,context->pc);
            checkMemory(rdram,source,count);
            auto result=post_sif.submit(memory().subspan(source,count));
            sif_ee_receive_address=post_sif.ee_receive_address;++sif_dma_submissions;
            if(post_sif.sreg_responses) {
                // Captured SET_SREG index 0/value 1 response, logical delivery.
                constexpr std::array<std::uint32_t,6> reply={0,0,0x80000001,0,0,1};
                for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
                write32(rdram,0x3A9740,1);
            }
            context->r[2].low=result;return;
        }
    }
    if (code == 0 && selector == 0x77 && sif_dma_submissions == 4 && scmd_mmode_supplied_) {
        receiveScmdMmode(rdram,context); return;
    }
    if (code == 0 && selector == 0x77 && sif_dma_submissions == 3 && scmd_bind_supplied_) {
        receiveScmdBind(rdram, context);
        return;
    }
    if (code == 0 && selector == 0x77 && sif_dma_submissions == 2 && cd_init_supplied_) {
        receiveFirstCdInit(rdram, context);
        return;
    }
    if (code == 0 && selector == 0x77 && sif_dma_submissions == 1 && !sif_bind_iop_.empty()) {
        receiveFirstRpcBind(rdram, context);
        return;
    }
    if (code == 0 && selector == 0x77 && reset_submission_supplied_ && sif_dma_submissions == 5) {
        if (iop_reset_pending || reset_smflag_writes != 1 || sif_reset_subaddr_reads != 1
                || context->r[4].low != initial_sp-224 || context->r[29].low != context->r[4].low
                || context->r[5].low != 1) throw UnsupportedSyscall(selector,context->pc);
        const std::array<std::uint32_t,4> descriptor{0x3ACC40,0x19600,104,0x44};
        const std::array<std::uint32_t,6> header{104,0,0x80000003,0,36,0};
        for(unsigned i=0;i<4;++i) if(read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+i*4)!=descriptor[i])throw UnsupportedSyscall(selector,context->pc);
        for(unsigned i=0;i<6;++i) if(read32(rdram,0x3ACC40+i*4)!=header[i])throw UnsupportedSyscall(selector,context->pc);
        constexpr char path[]="rom0:UDNL cdrom0:\\IRX\\IOPRP300.IMG;1";
        if(!std::equal(std::begin(path),std::end(path),memory_.begin()+0x3ACC58))throw UnsupportedSyscall(selector,context->pc);
        // Submission acknowledgement only. Retain the request and invalidate
        // old service lifetime; no receive, image execution or ready flag invented.
        std::copy_n(memory_.data()+0x3ACC40,104,reset_request_.data());
        iop_reset_pending=true; prior_iop_services_valid=false; ++iop_epoch;
        ++sif_dma_submissions;context->r[2].low=0xE01;return;
    }
    if (code == 0 && selector == 0x77) {
        if (sif_iop_.empty() || sif_dma_submissions || sif_command_reads != 1 || data_cache_lines != 1
                || context->r[5].low != 1 || context->r[4].low > UINT32_MAX)
            throw UnsupportedSyscall(selector, context->pc);
        const auto descriptor = static_cast<std::uint32_t>(context->r[4].low);
        if (descriptor & 15) throw UnsupportedGuestMemory(descriptor, 16);
        checkMemory(rdram, descriptor, 16);
        const auto source = read32(rdram, descriptor), destination = read32(rdram, descriptor + 4);
        if (source != 0x3A9580 || destination != 0x19600 || read32(rdram, descriptor + 8) != 20
                || read32(rdram, descriptor + 12) != 0x44)
            throw UnsupportedSyscall(selector, context->pc);
        checkMemory(rdram, source, 20);
        // Observed CHANGE_SADDR command only; unknown commands fail before mutation.
        if (read32(rdram, source) != 20 || read32(rdram, source + 4) != 0
                || read32(rdram, source + 8) != 0x80000000 || read32(rdram, source + 12) != 0
                || read32(rdram, source + 16) != 0x3A94C0)
            throw UnsupportedSyscall(selector, context->pc);
        std::copy_n(memory_.data() + source, 20, sif_iop_.data() + destination);
        // Model only the validated logical receive effects, synchronously.
        // The service consumes psize and stores the new EE receive address.
        sif_iop_[destination] = 0;
        sif_ee_receive_address = 0x3A94C0;
        std::memcpy(sif_iop_.data() + 0x196C8, &sif_ee_receive_address, 4);
        ++sif_dma_submissions;
        context->r[2].low = sif_dma_result_;
        return;
    }
    if (code == 0 && selector == 0x79 && iop_reset_pending) {
        const auto reg=static_cast<std::uint32_t>(context->r[4].low);
        const auto value=static_cast<std::uint32_t>(context->r[5].low);
        const std::array<std::uint32_t,4> regs{4,4,0x80000002,0x80000000};
        const std::array<std::uint32_t,4> values{0x10000,0x20000,0,0};
        if(!reset_submission_supplied_ || reset_clear_writes>=4 || reg!=regs[reset_clear_writes] || value!=values[reset_clear_writes]
                || context->r[29].low!=initial_sp-224)throw UnsupportedSyscall(selector,context->pc);
        if(reset_clear_writes<2) { sif_smflag&=~value;context->r[2].low=sif_smflag; }
        else { if(reset_clear_writes==2) reset_rpcinit=0;else reset_subaddr=0;context->r[2].low=0; }
        ++reset_clear_writes;return;
    }
    if (code == 0 && selector == 0x79 && sif_stop_calls) {
        if (!reset_smflag_supplied_ || reset_smflag_writes || sif_reset_subaddr_reads != 1
                || context->r[4].low != 4 || context->r[5].low != 0x40000
                || context->r[29].low != initial_sp - 224 || sif_smflag != 0x30000)
            throw UnsupportedSyscall(selector, context->pc);
        // Captured kernel behavior: EE SMFLAG write clears selected bits,
        // returning the readback value (different from the pinned Play! return).
        sif_smflag &= ~0x40000u; ++reset_smflag_writes;
        context->r[2].low = sif_smflag;
        return;
    }
    if (code == 0 && selector == 0x6B) {
        if (!sif_stop_supplied_ || sif_stop_calls || scmd_mmode_calls != 1 || sif_dma_submissions != 5
                || dmac_removals != 1 || dmac_mask != 0x800000 || !dmac_handlers.empty()
                || context->r[4].low != 1 || context->r[29].low != initial_sp - 224)
            throw UnsupportedSyscall(selector, context->pc);
        // Local kernel handler writes zero to SIF2 CHCR/QWC and returns QWC.
        // All supported logical submissions have finished; active DMA is unsupported.
        sif2_chcr = 0; sif2_qwc = 0; ++sif_stop_calls; context->r[2].low = 0;
        return;
    }
    if(code==0 && (selector==0x70 || selector==0x71)) {
        // GsGetIMR / GsPutIMR (ee_gs_privileged.cpp): the GS interrupt mask; PutIMR returns the previous mask.
        const auto mask=selector==0x71?gsPutImr(context->r[4].low,context->pc):gs_imr;
        context->r[2].low=mask;context->r[2].high=0;return;
    }
    if(code==0 && selector==2) {
        // SetGsCrt (ee_gs_privileged.cpp): the display mode. Void in the SDK; the captures return 0.
        gsSetCrt(context->r[4].low,context->r[5].low,context->r[6].low,context->pc);context->r[2].low=0;return;
    }
    if (code == 0 && selector == 0x7A && iop_reset_pending && reset_ready_supplied_) {
        if(reset_ready_checkpoint_reads || reset_clear_writes!=4 || context->pc!=0x32F078
                || context->r[4].low!=4 || context->r[29].low!=initial_sp-48
                || context->r[31].low!=0x338118)throw UnsupportedSyscall(selector,context->pc);
        // Explicit reference checkpoint replay: this caller tests only bit 0x40000.
        // This does not model full SMFLAG or execute/complete the native IOP boot.
        context->r[2].low=0x40000;++reset_ready_checkpoint_reads;return;
    }
    if (code == 0 && selector == 0x7A && sif_stop_calls) {
        if (!sif_reset_subaddr_supplied_ || sif_reset_subaddr_reads
                || static_cast<std::uint32_t>(context->r[4].low) != 0x80000000 || context->r[29].low != initial_sp - 224)
            throw UnsupportedSyscall(selector, context->pc);
        ++sif_reset_subaddr_reads; context->r[2].low = 0x19600;
        return;
    }
    if (code == 0 && selector == 0x7A && static_cast<std::uint32_t>(context->r[4].low) == 0x80000002) {
        if (!sif_rpc_supplied_ || sif_rpc_reads || sif_dma_submissions != 1)
            throw UnsupportedSyscall(selector, context->pc);
        context->r[2].low = 1;
        ++sif_rpc_reads;
        return;
    }
    if (code == 0 && selector == 0x7A) {
        if (!sif_command_supplied_ || sif_command_reads || static_cast<std::uint32_t>(context->r[4].low) != 0x80000000
                || dmac_enable_calls != 1) throw UnsupportedSyscall(selector, context->pc);
        context->r[2].low = static_cast<std::uint64_t>(static_cast<std::int64_t>(
            std::bit_cast<std::int32_t>(sif_command_word_)));
        ++sif_command_reads;
        return;
    }
    if (code == 0 && selector == 0x4A) {
        if (!osd_readback_supplied || !((osd_config_reads == 1 && osd_config_sets == 0)
                || (osd_config_reads == 2 && osd_config_sets == 1)) || context->r[4].low > UINT32_MAX)
            throw UnsupportedSyscall(selector, context->pc);
        const auto address = static_cast<std::uint32_t>(context->r[4].low);
        if (address & 3) throw UnsupportedGuestMemory(address, 4);
        checkMemory(rdram, address, 4);
        // Capture established only the same-value write; mutations fail.
        if (read32(rdram, address) != osd_config_word) throw UnsupportedSyscall(selector, context->pc);
        ++osd_config_sets;
        return;
    }
    if (code == 0 && selector == 0x4B) {
        if (!osd_config_supplied || (osd_config_reads && !(osd_readback_supplied && osd_config_reads == 1 && osd_config_sets == 1)) || context->r[4].low > UINT32_MAX)
            throw UnsupportedSyscall(selector, context->pc);
        const auto address = static_cast<std::uint32_t>(context->r[4].low);
        if (address & 3) throw UnsupportedGuestMemory(address, 4);
        checkMemory(rdram, address, 4);
        write32(rdram, address, osd_config_word);
        ++osd_config_reads;
        return; // SDK declares void; preserve the result register.
    }
    if (code == 0 && selector == 0x32 && active_ee_stack && current_thread_id==3) {
        if(context->pc!=0x32EB88 || context->r[29].low!=0x3A6C80
            || context->r[28].low!=0x38BDF0 || context->r[16].low!=0x3A6D00
            || read32(rdram,0x3A6C90)!=0x100250
            || read32(rdram,0x3A6D00)!=3 || read32(rdram,0x3A6D08)!=0x3A6D18
            || read32(rdram,0x3A6D18)!=0x80000704 || read32(rdram,0x3A6D1C)!=0x3153F0
            || read32(rdram,0x3A6D20)!=0x3A6CC0)
            throw UnsupportedSyscall(selector,context->pc);
        if(ee_queued_sound_rpc) {
            if(sound_thread_waiting || !queued_sounds_bind.pending) throw UnsupportedSyscall(selector,context->pc);
            sound_thread_waiting=true;threads.at(1).ready=false;threads.at(1).waiting=true;threads.at(1).context=*context;
        } else ++ee_sound_probe_waits;
        ee_sound_probe_frontier=*context;
        active_ee_stack->yield();
        // No owned wake transaction exists yet.
        throw std::runtime_error("Unvalidated EE sound wake");
    }
    if (code == 0 && selector == 0x14) {
        // Observed cause only; explicit mask checkpoint avoids a fabricated BIOS default.
        if (!intc_mask_supplied_ || static_cast<std::uint32_t>(context->r[4].low) != 11)
            throw UnsupportedSyscall(selector, context->pc);
        constexpr std::uint32_t bit = 1u << 11;
        context->r[2].low = (intc_mask & bit) ? 0 : 1;
        intc_mask |= bit;
        ++intc_enable_calls;
        return;
    }
    if (code == 0 && selector == 0x10) {
        const auto cause = static_cast<std::uint32_t>(context->r[4].low);
        const auto address = static_cast<std::uint32_t>(context->r[5].low);
        // Observed startup contract only. No interrupt delivery or removal yet.
        if (cause != 11 || !address || address % 4 || context->r[6].low != 0 || context->r[7].low != 0)
            throw UnsupportedSyscall(selector, context->pc);
        checkMemory(rdram, address, 4);
        const auto id = addIntcHandler(cause, address, 0, static_cast<std::uint32_t>(context->r[28].low));
        context->r[2].low = id;
        return;
    }
    if (code == 0 && selector == 0x12) {
        const auto channel = static_cast<std::uint32_t>(context->r[4].low);
        const auto address = static_cast<std::uint32_t>(context->r[5].low);
        if (channel != 5 || !address || address % 4 || context->r[6].low != 0)
            throw UnsupportedSyscall(selector, context->pc);
        checkMemory(rdram, address, 4);
        if (dmac_handlers.size() == 128) {
            context->r[2].low = UINT64_MAX;
            return;
        }
        if(post_sif.supplied && gif_dma.completions==2) {
            if(address!=0x331DF0 || !dmac_handlers.empty())throw UnsupportedSyscall(selector,context->pc);
            post_sif.handler();
        }
        dmac_handlers.push_back({channel, address, static_cast<std::uint32_t>(context->r[7].low),
                                 static_cast<std::uint32_t>(context->r[28].low)});
        const auto id = static_cast<std::uint32_t>(dmac_handlers.size());
        dmac_handler_order.insert(dmac_handler_order.begin(), id);
        context->r[2].low = id;
        return;
    }
    if (code == 0 && selector == 0x17) {
        // Reached SIF shutdown: toggle only the recorded channel-5 mask bit.
        if (context->r[4].low != 5 || scmd_mmode_calls != 1 || dmac_handlers.size() != 1)
            throw UnsupportedSyscall(selector, context->pc);
        const auto bit = 0x10000u << 5;
        const bool changed = (dmac_mask & bit) != 0;
        dmac_mask &= ~bit;
        ++dmac_disable_calls;
        context->r[2].low = changed ? 1 : 0;
        return;
    }
    if (code == 0 && selector == 0x13) {
        // Sole registered dormant handler; no dispatch or wait queue exists.
        if (context->r[4].low != 5 || context->r[5].low != 1 || dmac_disable_calls != 1
                || dmac_mask != 0x800000 || dmac_handlers.size() != 1
                || dmac_handlers[0].channel != 5 || dmac_handler_order != std::vector<std::uint32_t>{1})
            throw UnsupportedSyscall(selector, context->pc);
        dmac_handlers.clear(); dmac_handler_order.clear(); ++dmac_removals;
        context->r[2].low = 0; // Remaining handlers on this channel.
        return;
    }
    if (code == 0 && selector == 0x16) {
        // Captured mask bits only. No status acknowledgements, DMA or interrupt delivery.
        if (context->r[4].low != 5 || dma_status_reads != 1 || dma5_control_reads != 1
                || dmac_handlers.empty()) throw UnsupportedSyscall(selector, context->pc);
        if(post_sif.supplied && gif_dma.completions==2)post_sif.enable();
        const auto bit = 0x10000u << 5;
        const bool changed = !(dmac_mask & bit);
        dmac_mask |= bit;
        ++dmac_enable_calls;
        context->r[2].low = changed ? 1 : 0;
        return;
    }
    if (code == 0 && selector == 0x74) {
        const auto number = static_cast<std::uint32_t>(context->r[4].low);
        const auto address = static_cast<std::uint32_t>(context->r[5].low);
        const bool timer_interrupt = number == 0x12C;
        if ((!timer_interrupt && number >= syscall_handlers.size()) || address % 4
                || (timer_interrupt && (timer3_interrupt_handler || address < 0x80000000)))
            throw UnsupportedSyscall(selector, context->pc);
        if (address >= 0x80000000) {
            // Register only a word actually written into the explicitly enabled patch.
            // Registration does not make these bytes a compiled native function.
            const bool primary_patch = address >= kernel_patch_start_ && address < kernel_patch_end_;
            bool startup_payload = address == 0x80075000 && startup_kernel_word_writes == 0x330 / 4;
            if (!startup_kernel_payload_.empty() && startup_kernel_word_writes == 0x330 / 4
                    && address >= 0x80075000 && address < 0x80075330) {
                // Exact selector/address pairs from the fully validated copied table.
                for (std::size_t offset = 0x300; offset < 0x330; offset += 8) {
                    std::uint32_t table_selector, table_address;
                    std::memcpy(&table_selector, startup_kernel_payload_.data() + offset, 4);
                    std::memcpy(&table_address, startup_kernel_payload_.data() + offset + 4, 4);
                    if (number == table_selector && address == table_address) startup_payload = true;
                }
            }
            if (memory_.empty() || rdram != memory_.data() || (!primary_patch && !startup_payload)
                    || !kernel_patch_words_.contains(address - 0x80000000))
                throw UnsupportedGuestMemory(address, 4);
        } else if (address) checkMemory(rdram, address, 4);
        if (timer_interrupt) {
            // Pinned Play! maps this extended selector to INTC line 12.
            // Record one startup registration/enable request; no interrupt delivery.
            if (addIntcHandler(12, address, 0, 0) == UINT64_MAX)
                throw std::runtime_error("Timer 3 interrupt handler pool exhausted");
            timer3_interrupt_handler = address;
            timer3_interrupt_enable_requested = true;
            ++extended_registrations;
        } else {
            syscall_handlers[number] = address;
            // BT2_SELF_STARTUP (no BIOS kernel): the BIOS keeps the handler in its syscall table at 0x80014D00,
            // where FUN_00338410 searches for its handlers of 0x83 and 0x5A to find the table. The entry goes
            // to the kernel overlay; the baseline stays as it was.
            static const bool self_startup = std::getenv("BT2_SELF_STARTUP") != nullptr;
            if (self_startup && !kernel_snapshot_.empty()) kernel_patch_words_[0x14D00 + number * 4] = address;
        }
        ++syscall_registrations;
        // PS2SDK declares void; preserve v0 rather than inventing a result.
        return;
    }
    if (code == 0 && selector == 0x3C) {
        // Bootstrap model follows pinned Play! sc_SetupThread; no scheduler yet.
        const auto stack = static_cast<std::uint32_t>(context->r[5].low);
        const auto size = static_cast<std::uint32_t>(context->r[6].low);
        const auto args = static_cast<std::uint32_t>(context->r[7].low);
        constexpr std::uint32_t top = static_cast<std::uint32_t>(ram_size) - 4096;
        if (thread_initialized || stack != UINT32_MAX || size < 0x2A0 || size > top
            || size % 16 || args % 4)
            throw std::runtime_error("Unsupported bootstrap SetupThread contract");
        checkMemory(rdram, args, 8);
        if (startup_zero_end_) {
            for (auto address = startup_zero_start_; address < startup_zero_end_; ++address)
                if (memory_[address]) throw std::runtime_error("Startup zero region was not cleared before SetupThread");
            startup_zero_verified = true;
            startup_quadword_writes = quadword_writes;
            startup_byte_writes = byte_writes;
        }
        stack_base = top - size;
        initial_sp = top - 0x2A0;
        // Explicit empty launch: argc=0, argv[0]=nullptr.
        std::memset(memory_.data() + args, 0, 8);
        thread_arguments = {static_cast<std::uint32_t>(context->r[4].low), stack, size,
                            args, static_cast<std::uint32_t>(context->r[8].low)};
        context->r[2].low = initial_sp;
        current_thread_id = 1;
        thread_initialized = true;
        return;
    }
    if (code == 0 && selector == 0x3D) {
        // Pinned Play! default heap end is the initialized main thread's stack base.
        const auto start = static_cast<std::uint32_t>(context->r[4].low);
        const auto size = static_cast<std::uint32_t>(context->r[5].low);
        if (!thread_initialized || heap_initialized || size != UINT32_MAX || start >= stack_base)
            throw std::runtime_error("Unsupported bootstrap SetupHeap contract");
        checkMemory(rdram, start, 1);
        heap_start = start;
        heap_end = stack_base;
        context->r[2].low = heap_end;
        heap_initialized = true;
        return;
    }
    if(code==0 && selector==0x3E) {
        if(!thread_initialized || !heap_initialized || current_thread_id!=1)throw UnsupportedSyscall(selector,context->pc);
        context->r[2].low=heap_end;++heap_end_queries;return;
    }
    if(code==0 && selector==0x7C && sound_rpc_calls==1 && native_module_loads==14) {
        if(context->pc!=0x32F098 || context->r[4].low!=16 || context->r[5].low!=context->r[29].low)
            throw UnsupportedSyscall(selector,context->pc);
        const auto pointer=read32(rdram,static_cast<std::uint32_t>(context->r[5].low));
        std::string message;bool terminated=false;
        for(unsigned n=0;n<4096;++n) {
            const auto ch=read8(rdram,pointer+n);if(!ch){terminated=true;break;}message+=static_cast<char>(ch);
        }
        if(!terminated)throw UnsupportedSyscall(selector,context->pc);
        ee_console_messages.push_back(std::move(message));context->r[2].low=UINT64_MAX;return;
    }
    if (code == 0 && selector == 0x40) {
        const auto address = static_cast<std::uint32_t>(context->r[4].low);
        checkMemory(rdram, address, 24);
        if (address % 4) throw std::runtime_error("Unaligned CreateSema parameters");
        // Input count/wait_threads are output fields, not initial state.
        const auto maximum = read32(rdram, address + 4);
        const auto initial = read32(rdram, address + 8);
        const auto attributes = read32(rdram, address + 16);
        const auto option = read32(rdram, address + 20);
        // The kernel keeps a semaphore's count, maximum and option and ignores attr (Play!'s CreateSema
        // stores init/max/option only; library callers leave attr as uninitialized stack residue), so any
        // attr is accepted. The conditions below only count the startup's own semaphores, which the
        // startup checks and the comparisons with the original read.
        const bool rpc_call_sema = sif_call_sema_supplied_ && sif_bind_callbacks == 1 && semaphore_deletions == 1
            && semaphores.size() == 3 && address == initial_sp - 400
            && maximum == 1 && initial == 0 && option == 0x3838E8;
        const bool cd_sema = cd_semaphores_supplied_ && cd_init_calls == 1 && semaphore_deletions == 2
            && cd_semaphores_created < 4 && semaphores.size() == 3 + cd_semaphores_created
            && address == initial_sp - 304 && maximum == 1
            && initial == (cd_semaphores_created == 3 ? 0u : 1u)
            && option == 0x3825B8 + cd_semaphores_created * 16;
        const bool disc_bind_sema=mc_rpc_calls==2 && mc_rpc_binds==1 && semaphores.size()==10
            && !disc_bind_semaphores && address==initial_sp-592 && maximum==1 && initial==0 && option==0x3838D8
            && context->pc==0x32EC68 && context->r[31].low==0x332814
            && context->r[17].low==0x3A8780 && static_cast<std::uint32_t>(context->r[19].low)==0x80000597
            && read32(rdram,0x3A8780)==0x203A97C0 && read32(rdram,0x3A8784)==42;
        const bool ncmd_bind_sema=disc_rpc_calls==1 && !ncmd_bind_semaphores && semaphores.size()==10
            && address==initial_sp-608 && maximum==1 && initial==0 && option==0x3838D8
            && context->pc==0x32EC68 && context->r[31].low==0x332814 && context->r[17].low==0x369590
            && static_cast<unsigned>(context->r[19].low)==0x80000595 && read32(rdram,0x369594)==44;
        if(ncmd_bind_sema)++ncmd_bind_semaphores;
        if (disc_bind_sema) ++disc_bind_semaphores;
        if (cd_sema) ++cd_semaphores_created;
        if (rpc_call_sema) { ++rpc_call_semaphores; rpc_call_sema_attributes = attributes; }
        if (!maximum || maximum > INT32_MAX || initial > maximum || semaphores.size() == 256) {
            context->r[2].low = UINT64_MAX;
            return;
        }
        // The lowest free id, as the kernel allocates them.
        std::uint32_t id=0;
        while(id<semaphores.size() && !semaphores[id].deleted)++id;
        if(id==semaphores.size())semaphores.push_back({initial,maximum,option});
        else semaphores[id]={initial,maximum,option};
        context->r[2].low = id;
        return;
    }
    if (code == 0 && selector == 0x45) {
        // Pinned nonblocking PollSema: never sleeps or reschedules.
        const auto id = static_cast<std::uint32_t>(context->r[4].low);
        ++semaphore_polls;
        if (id >= semaphores.size() || semaphores[id].deleted || semaphores[id].count == 0) {
            context->r[2].low = UINT64_MAX;
            return;
        }
        --semaphores[id].count;
        if(id==8 && semaphores[id].option==0x3824C8 && context->pc==0x32ECB8 && context->r[31].low==0x31DE18)++mc_lock_polls;
        context->r[2].low = id;
        return;
    }
    if (code == 0 && selector == 0x41) {
        const auto id = static_cast<std::uint32_t>(context->r[4].low);
        if (id >= semaphores.size() || semaphores[id].deleted) { context->r[2].low = UINT64_MAX; return; }
        // Any existing semaphore can be deleted; no thread waits on one in this model, so nothing is
        // released. Its id becomes free (the newest is removed, an older one is marked deleted).
        if(id+1==semaphores.size()) {
            semaphores.pop_back();
            while(!semaphores.empty() && semaphores.back().deleted)semaphores.pop_back();
        } else semaphores[id].deleted=true;
        ++semaphore_deletions;
        context->r[2].low = id;
        return;
    }
    if (code == 0 && (selector == 0x44 || selector == 0x42
            || (selector == 0xFFFFFFBD && rpc_end_callback_active_))) {
        const auto id = static_cast<std::uint32_t>(context->r[4].low);
        if (id >= semaphores.size() || semaphores[id].deleted) {
            context->r[2].low = UINT64_MAX;
            return;
        }
        auto& semaphore = semaphores[id];
        if (selector == 0x44) {
            if(!semaphore.count && queued_sounds_bind.pending) finishQueuedSoundsBind(rdram,context);
            if (!semaphore.count) throw UnsupportedSyscall(selector, context->pc);
            --semaphore.count;
            ++semaphore_waits;
            if(id==8 && semaphore.option==0x3824C8 && context->pc==0x32ECA8 && (context->r[31].low==0x31DCFC || context->r[31].low==0x31D304))++mc_lock_waits;
            if(id==7 && semaphore.option==0x381C28 && context->pc==0x32ECA8 && context->r[31].low==0x3135B8)++dbc_lock_waits;
            if(context->pc==0x32ECA8 && context->r[31].low==0x3252F0)++ee_heap_lock_waits;
        } else {
            // No waiting threads exist in this bounded model. Overflow is unverified.
            if (semaphore.count >= semaphore.maximum) throw UnsupportedSyscall(selector, context->pc);
            ++semaphore.count;
            ++semaphore_signals;
            if(id==8 && semaphore.option==0x3824C8 && context->pc==0x32EC88 && (context->r[31].low==0x31DD34 || context->r[31].low==0x31D3E0))++mc_lock_signals;
            if(id==7 && semaphore.option==0x381C28 && context->pc==0x32EC88)++dbc_lock_signals;
            if(context->pc==0x32EC88 && texture_gif.phase==8 && id==read32(rdram,0x36AB58))++ee_heap_lock_signals;
        }
        context->r[2].low = id;
        return;
    }
    if (code != 0 || static_cast<std::uint32_t>(context->r[3].low) != 0x64)
        throw UnsupportedSyscall(static_cast<std::uint32_t>(context->r[3].low), context->pc);
    const auto operation = static_cast<std::uint32_t>(context->r[4].low);
    if (operation != 0 && operation != 2) throw UnsupportedSyscall(selector, context->pc);
    // Pinned PS2SDK: 0=writeback D-cache, 2=invalidate I-cache (void).
    // Stores are immediately visible and dispatch uses fixed native functions.
    // No guest instruction cache exists; this does not translate patched kernel code.
    if (operation == 2) ++instruction_cache_invalidations;
    ++cache_requests;
    syscall_resume_pc = context->pc;
}
