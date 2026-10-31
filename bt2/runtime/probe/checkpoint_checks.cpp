#include "probe_state.h"
#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <algorithm>

namespace {
// Whether the startup clears left a 640 x 448 frame buffer (FBW 10) at page `fbp` black up to its last
// pixel: the last accepted clear covering (639, 447) writes colour 0 with no bit masked.
bool clearedToBlack(const GsCommandSink& gs,unsigned fbp) {
    for(auto it=gs.clears.rbegin();it!=gs.clears.rend();++it)
        if(it->fbp==fbp && it->width==10 && it->x0<=639 && it->x1>639 && it->y0<=447 && it->y1>447)
            return it->color==0 && it->mask==0;
    return false;
}
}

// Checkpoint validation: the state each replayed startup checkpoint must reach at its boundary.
// Later checkpoints are validated per request by their receivers; these checks cover the boundary.

void checkCdContinuation(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop) {
    const auto stopped=stop.stopped;
    const auto stop_selector=stop.selector;
    const auto stop_handler=stop.handler;
    const auto stop_target=stop.target;
    const auto stop_source=stop.source;
    const auto stop_resume=stop.resume;
    require(stopped && !stop_handler && (stop_target || (stop_selector == 0x40 && context.pc == 0x32EC68)),
            "CD continuation did not reach a checked boundary");
    require(runtime.cd_init_calls == 1 && runtime.cd_init_context_preservations == 1
            && runtime.sif_dma_submissions == 3 && runtime.semaphore_deletions == 2
            && runtime.semaphore_waits == 3 && runtime.semaphore_signals == 3,
            "CD-init callback or cleanup effects differ");
    if (stop_target) {
        const bool mapped_stop = (stop_target == 0x320488 && stop_source == 0x1003AC && stop_resume == 0x1003B4
                && runtime.guest_calls == 160 && runtime.returns == 158 && context.r[29].low == runtime.initial_sp - 32)
            || (stop_target == 0x31F6A8 && stop_source == 0x3204A8 && stop_resume == 0x3204B0
                && runtime.guest_calls == 161 && runtime.returns == 158 && context.r[29].low == runtime.initial_sp - 112)
            || (stop_target == 0x32ECB0 && stop_source == 0x31F6CC && stop_resume == 0x31F6D4
                && runtime.guest_calls == 163 && runtime.returns == 159 && context.r[29].low == runtime.initial_sp - 176);
        require(mapped_stop && runtime.cd_semaphores_created == 4 && runtime.semaphores.size() == 7,
                "Reached mapped-entry boundary differs");
    } else if (runtime.cd_semaphores_created) {
        require(runtime.cd_semaphores_created == 4 && runtime.semaphores.size() == 7
                && runtime.semaphore_polls == 1 && runtime.guest_calls == 173 && runtime.returns == 167
                && context.r[29].low == runtime.initial_sp - 288,
                "CD setup and next bind progress differs");
    } else require(runtime.semaphores.size() == 3 && context.r[29].low == runtime.initial_sp - 304,
                   "CD semaphore checkpoint boundary differs");
}

void checkRuntimeBoundary(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop) {
    const auto stopped=stop.stopped;
    const auto stop_address=stop.address;
    const auto stop_size=stop.size;
    const auto stop_selector=stop.selector;
    const auto stop_handler=stop.handler;
    const auto stop_target=stop.target;
    const auto stop_source=stop.source;
    const auto stop_resume=stop.resume;
    // The C library's stdio lock (created lazily after admission) is outside the startup service accounting.
    const auto stdio_locks=static_cast<std::size_t>(std::count_if(runtime.semaphores.begin(),runtime.semaphores.end(),
        [](const auto& sema){return sema.option==0x383988 || EeRuntime::stdioCallSema(sema.option);}));
    const auto semas=runtime.semaphores.size()-stdio_locks;
    const bool pending_mc_bind=runtime.dbc_rpc_calls==4 && semas==11 && stop_selector==0x77 && context.pc==0x32F028
        && runtime.semaphores[10].maximum==1 && runtime.semaphores[10].count==0 && (runtime.semaphores[10].option==0x3838D8 || (runtime.mc_rpc_binds==1 && runtime.semaphores[10].option==0x3838E8));
    const bool pending_mc_query=runtime.mc_rpc_calls==1 && semas==10 && ((stop_selector==0x77 && context.pc==0x32F028) || (stop_target==0x31DD60 && stop_source==0x332354))
        && runtime.read32(runtime.memory().data(),0x3A97DC)==0x3A6F80
        && runtime.read32(runtime.memory().data(),0x3A97E0)==1;
    const bool mc_local_semas=runtime.dbc_rpc_calls==4 && (semas==10 || pending_mc_bind || (runtime.disc_bind_semaphores==1 && semas==11 && (runtime.semaphores[10].option==0x3838D8 || runtime.semaphores[10].option==0x3838E8) && !runtime.semaphores[10].count))
        && runtime.semaphores[8].maximum==1 && runtime.semaphores[8].count==(pending_mc_query?0u:1u-runtime.mc_lock_waits-runtime.mc_lock_polls+runtime.mc_lock_signals)
        && runtime.semaphores[8].option==0x3824C8 && runtime.semaphores[9].maximum==1
        && runtime.semaphores[9].count==0 && runtime.semaphores[9].option==0x3824D8;
    const bool dbc_semas=runtime.dbc_rpc_binds==4 && runtime.dbc_rpc_calls>=1 && (semas==9 || mc_local_semas)
        && runtime.semaphores[7].maximum==127 && runtime.semaphores[7].option==0x381C28;
    require(stopped && !stop_handler && runtime.scmd_bind_calls==1 && runtime.cd_init_calls==1
        // After display setup, later RPCs are validated per request by their receivers.
        && (runtime.display_reset_phase==13 || (runtime.post_cd.supplied ? (semas==7 || semas==8 || dbc_semas)
            : semas==(runtime.post_sif.rpc_initialized && stop_selector==0x77?8u:7u))),"SCMD continuation state differs");
    if (!runtime.scmd_mmode_calls) {
        require(!stop_target && stop_selector == 0x40 && context.pc == 0x32EC68
                && runtime.guest_calls == 190 && runtime.returns == 185
                && context.r[29].low == runtime.initial_sp - 304 && runtime.semaphore_deletions == 3,
                "SCMD call-semaphore boundary differs");
    } else {
        // Pre-admission accounting; after main's admitted priority, helper RPC traffic is
        // validated per request by its receiver instead of by these lifetime totals.
        require(runtime.main_priority_admissions || (runtime.semaphore_deletions == 4+runtime.serviceOperations()-(runtime.mc_rpc_calls==2?1u:0u) && runtime.semaphore_waits == 5+runtime.serviceOperations()-(runtime.mc_rpc_calls==2?1u:0u)+runtime.ee_heap_lock_waits+runtime.dbc_lock_waits+runtime.mc_lock_waits && runtime.semaphore_signals == 6+runtime.serviceOperations()+(runtime.post_cd.scmd_binds?1u:0u)+runtime.ee_heap_lock_signals+runtime.dbc_lock_signals+runtime.mc_lock_signals+runtime.disc_rpc_calls+runtime.ncmd_ready_calls+runtime.ncmd_read_calls
                && (runtime.sif_dma_submissions == 5 || (runtime.iop_reset_pending && runtime.sif_dma_submissions == 6+runtime.post_sif.submissions+runtime.serviceOperations()+runtime.heap_transfers+runtime.ncmd_read_calls+runtime.cri_stream_transfers))), "SCMD media-mode cleanup differs");
        if (stop_target && !runtime.sif_stop_calls) {
            const std::array<std::array<std::uint32_t,2>,9> reached{{
                {0x338158,0x1003B8},{0x332188,0x3381D4},{0x331B08,0x332190},
                {0x32F810,0x331B10},{0x32E9D0,0x32F840},{0x32E990,0x331B20},
                {0x337F88,0x33824C},{0x33A3F0,0x337FA4},{0x32EF30,0x337FB8}}};
            require(std::any_of(reached.begin(),reached.end(),[&](const auto& entry) {
                return entry[0] == stop_target && entry[1] == stop_source && stop_resume == stop_source + 8;
            }), "Unexpected SCMD mapped-entry boundary");
        } else if (!runtime.sif_stop_calls) require(stop_selector == 0x6B && context.pc == 0x32EF38
            && runtime.guest_calls == 214 && runtime.returns == 209
            && context.r[29].low == runtime.initial_sp - 224 && GPR_U32(&context,4) == 1
            && runtime.dmac_disable_calls == 1 && runtime.dmac_removals == 1
            && (runtime.post_sif.supplied || (runtime.dmac_mask == 0x800000 && runtime.dmac_handlers.empty()))
            && runtime.read32(runtime.memory().data(),0x36AA90) == 0
            && runtime.read32(runtime.memory().data(),0x36AA8C) == 0,
            "SIF shutdown and DMA-stop boundary differs");
    }
    if (runtime.sif_stop_calls) {
        require(runtime.sif_stop_calls == 1 && runtime.sif2_chcr == 0 && runtime.sif2_qwc == 0
                && (runtime.iop_reset_pending || context.r[29].low == runtime.initial_sp - 224),
                "SIF stop continuation state differs");
        if (!runtime.sif_reset_subaddr_reads) {
            require(!stop_target && stop_selector == 0x7A && context.pc == 0x32F078
                    && runtime.guest_calls == 215 && runtime.returns == 210,
                    "Post-stop SUBADDR boundary differs");
        } else if (!runtime.reset_smflag_writes) {
            require(runtime.sif_reset_subaddr_reads == 1 && runtime.returns == 212
                    && ((stop_target == 0x32F060 && stop_source == 0x338070 && stop_resume == 0x338078
                        && runtime.guest_calls == 216)
                    || (!stop_target && stop_selector == 0x79 && context.pc == 0x32F068
                        && runtime.guest_calls == 217)), "First reset SMFLAG boundary differs");
        } else if (!runtime.iop_reset_pending) {
            require(runtime.reset_smflag_writes == 1 && runtime.sif_reset_subaddr_reads == 1
                    && runtime.sif_smflag == 0x30000 && !stop_target && stop_selector == 0x77
                    && context.pc == 0x32F028 && runtime.guest_calls == 218 && runtime.returns == 213
                    && GPR_U32(&context,4) == runtime.initial_sp - 224 && GPR_U32(&context,5) == 1,
                    "IOP reboot submission boundary differs");
            const std::array<std::uint32_t,4> descriptor{0x3ACC40,0x19600,104,0x44};
            for (unsigned i=0;i<4;++i) require(runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+i*4)==descriptor[i],
                                            "IOP reboot descriptor differs");
            const std::array<std::uint32_t,6> header{104,0,0x80000003,0,36,0};
            for (unsigned i=0;i<6;++i) require(runtime.read32(runtime.memory().data(),0x3ACC40+i*4)==header[i],
                                            "IOP reboot packet header differs");
            constexpr char boot_argument[] = "rom0:UDNL cdrom0:\\IRX\\IOPRP300.IMG;1";
            require(std::equal(std::begin(boot_argument),std::end(boot_argument),runtime.memory().begin()+0x3ACC58),
                    "IOP reboot path differs");
        } else {
            require(!runtime.prior_iop_services_valid && runtime.iop_epoch == 1
                    && runtime.reset_clear_writes == 4 && runtime.reset_rpcinit == 0
                    && runtime.reset_subaddr == 0 && runtime.sif_smflag == 0
                    && (runtime.main_priority_admissions || runtime.sif_dma_submissions == 6+runtime.post_sif.submissions+runtime.serviceOperations()+runtime.heap_transfers+runtime.ncmd_read_calls+runtime.cri_stream_transfers) && runtime.reset_smflag_writes == 1
                    && runtime.sif_reset_subaddr_reads == 1,
                    "Pending IOP reboot lifetime/register state differs");
            if(runtime.reset_ready_checkpoint_reads) {
                require(runtime.reset_ready_checkpoint_reads==1 && (runtime.post_sif.supplied || !stop_target
                    || (stop_target==0x310548 && stop_source==0x1002B0 && stop_resume==0x1002B8
                        && runtime.gif_dma.starts==1)),
                        "Unexpected ready continuation target/replay");
                if(runtime.post_sif.supplied) {
                    const bool chain_target=stop_target==0x32F040 && stop_source==0x3319FC && stop_resume==0x331A04 && runtime.post_sif.phase==2;
                    const bool sreg_target=stop_target==0x331840 && stop_source==0x332140 && stop_resume==0x332148 && runtime.post_sif.phase==13;
                    const bool service_stop=!stop_target && stop_selector==0x77 && context.pc==0x32F028
                        && runtime.post_sif.phase==14 && runtime.post_sif.rpc_initialized
                        && runtime.guest_calls==305 && runtime.returns==298 && context.r[29].low==runtime.initial_sp-464
                        && GPR_U32(&context,4)==runtime.initial_sp-464 && GPR_U32(&context,5)==1;
                    const bool cd_stop=runtime.post_cd.supplied && !runtime.post_cd.scmd_supplied && !stop_target
                        && stop_selector==0x40 && context.pc==0x32EC68 && runtime.guest_calls==359 && runtime.returns==353
                        && context.r[29].low==runtime.initial_sp-288 && runtime.post_cd.binds==2 && runtime.post_cd.calls==1;
                    const std::array<std::array<std::uint32_t,3>,4> loader_targets{{
                        {0x333B48,0x1003F8,0x100400},{0x337C28,0x10040C,0x100414},
                        {0x337A00,0x337C34,0x337C3C},{0x337228,0x337A30,0x337A38}}};
                    const bool loader_target=runtime.post_cd.scmd_supplied && runtime.serviceOperations()==5
                        && std::any_of(loader_targets.begin(),loader_targets.end(),[&](const auto& t){return stop_target==t[0] && stop_source==t[1] && stop_resume==t[2];});
                    const bool loader_stop=runtime.post_cd.scmd_supplied && !stop_target && stop_selector==0x77 && context.pc==0x32F028
                        && runtime.guest_calls==400 && runtime.returns==391 && context.r[29].low==runtime.initial_sp-528
                        && GPR_U32(&context,4)==runtime.initial_sp-528 && GPR_U32(&context,5)==1;
                    const std::array<std::array<std::uint32_t,3>,3> loadfile_targets{{
                        {0x337328,0x337A40,0x337A48},{0x325098,0x33735C,0x337364},{0x326270,0x337A68,0x337A70}}};
                    const bool loadfile_helper=std::any_of(loadfile_targets.begin(),loadfile_targets.end(),[&](const auto& t){return stop_target==t[0] && stop_source==t[1] && stop_resume==t[2];});
                    const bool loadfile_version_sema=!stop_target && stop_selector==0x40 && context.pc==0x32EC68 && runtime.post_loadfile.binds==1 && !runtime.post_loadfile.versions
                        && runtime.guest_calls==411 && runtime.returns==404 && context.r[29].low==runtime.initial_sp-464;
                    const bool loadfile_version_submit=!stop_target && stop_selector==0x77 && context.pc==0x32F028 && runtime.post_loadfile.binds==1 && !runtime.post_loadfile.versions
                        && runtime.guest_calls==415 && runtime.returns==406 && context.r[29].low==runtime.initial_sp-608;
                    const bool loadfile_module_sema=!stop_target && stop_selector==0x40 && context.pc==0x32EC68 && runtime.post_loadfile.versions==1
                        && runtime.guest_calls==429 && runtime.returns==423 && context.r[29].low==runtime.initial_sp-384;
                    const bool loadfile_module_submit=!stop_target && stop_selector==0x77 && context.pc==0x32F028 && runtime.post_loadfile.versions==1
                        && runtime.guest_calls==433 && runtime.returns==425 && context.r[29].low==runtime.initial_sp-528 && GPR_U32(&context,4)==runtime.initial_sp-528 && GPR_U32(&context,5)==2;
                    const bool native_loader_progress=runtime.native_module_loads>0 && runtime.post_loadfile.versions==1;
                    const bool loader_progress=runtime.post_loadfile.supplied && runtime.post_loadfile.binds==1 && runtime.post_cd.modes==1
                        && (native_loader_progress || loadfile_helper || loadfile_version_sema || loadfile_version_submit || loadfile_module_sema || loadfile_module_submit);
                    require(loader_progress || cd_stop || loader_target || loader_stop || chain_target || sreg_target || service_stop,"Fresh SIF continuation boundary differs");
                    // Main's repeated startup clear adds identical full-frame sprites to the same GS state.
                    const unsigned repeat_clears=runtime.clear_repeat_gif?runtime.clear_repeat_gif->starts:0u;
                    require(runtime.gif_dma.starts==2 && runtime.gif_dma.completions==2
                        && runtime.gif_dma.gs.packets==(runtime.texture_gif.starts?3u:2u)+repeat_clears && runtime.gif_dma.gs.sprites==2+repeat_clears
                        && runtime.gif_dma.gs.pixels==573440+std::uint64_t(repeat_clears)*640*448
                        && runtime.gif_dma.qwc==0 && runtime.gif_dma.madr==runtime.initial_sp-48
                        && runtime.gif_dma.sync_reads==5 && runtime.gif_dma.vu_reads==2 && runtime.gif_dma.packet_verified,
                        "Fresh SIF prior GS effects differ");
                    require(runtime.gs_interrupt.acknowledgements==2 && runtime.gs_interrupt.polls==2 && runtime.gs_interrupt.events==1
                        && !runtime.gs_interrupt.callbackPending() && runtime.gs_csr_reset_writes==1 && runtime.gs_csr_reads==1
                        && runtime.gs_imr_writes==1 && runtime.gs_crt_calls==1
                        && (runtime.graph_startup_phase==4?runtime.graph_halfword_checkpoint:runtime.halfword_writes)==(runtime.graph_startup_phase>=3?8u:runtime.graph_startup_phase>=2?7u:4u),
                        "Fresh SIF prior GS synchronization differs");
                    for(unsigned page : {0u,140u})require(clearedToBlack(runtime.gif_dma.gs,page),"Fresh SIF startup framebuffer edge differs");
                    if(loader_progress || cd_stop || loader_target || loader_stop) {
                        require(runtime.post_sif.phase==14 && runtime.post_sif.rpc_initialized && runtime.post_cd.binds==2 && runtime.post_cd.calls==1
                            && runtime.read32(runtime.memory().data(),0x3A87A8)==0 && runtime.read32(runtime.memory().data(),0x3A87CC)==0x43388
                            && runtime.read32(runtime.memory().data(),0x3A87BC)==0x441B8 && runtime.dmac_mask==0xA00000
                            && runtime.dmac_handlers.size()==1 && runtime.dmac_enable_calls==2,
                            "Fresh CD ownership/completion differs");
                    }
                    // Earlier-phase snapshot; after main's admitted priority the game reuses these clients.
                    if((loader_progress || loader_target || loader_stop) && !runtime.main_priority_admissions) require(runtime.post_cd.scmd_binds==1 && runtime.post_cd.modes==1
                        && runtime.read32(runtime.memory().data(),0x369F40)==0 && runtime.read32(runtime.memory().data(),0x369F64)==0x434F0
                        && runtime.read32(runtime.memory().data(),0x369F54)==0x439A8 && runtime.read32(runtime.memory().data(),0x3695C0)==1,
                        "Fresh SCMD ownership/completion differs");
                    if(loader_stop) {
                        require(runtime.semaphores.size()==8 && runtime.semaphores[7].count==0 && runtime.semaphores[7].maximum==1
                            && runtime.semaphores[7].option==0x3838D8,"LOADFILE bind semaphore differs");
                        const std::array<std::uint32_t,4> d={0x203A97C0,0x1E640,64,0x44};
                        for(unsigned n=0;n<4;++n)require(runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+n*4)==d[n],"LOADFILE descriptor differs");
                        const std::array<std::uint32_t,9> p={64,0,0x80000009,0,5,0x203A97C0,7,0x3ACC00,0x80000006};
                        for(unsigned n=0;n<9;++n)require(runtime.read32(runtime.memory().data(),0x3A97C0+n*4)==p[n],"LOADFILE request differs");
                    }
                    if(loadfile_module_submit) {
                        require(runtime.semaphores.size()==8 && !runtime.semaphores[7].count && runtime.semaphores[7].maximum==1 && runtime.semaphores[7].option==0x3838E8,
                            "LOADFILE module semaphore differs");
                        const std::array<std::uint32_t,8> descriptor={0x3ACA00,0x225B0,512,0,0x203A97C0,0x1E640,64,0x44};
                        for(unsigned n=0;n<8;++n)require(runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+n*4)==descriptor[n],"LOADFILE module descriptor differs");
                        const std::array<std::uint32_t,14> request={0x20040,0x225B0,0x8000000A,0,5,0x203A97C0,9,0x3ACC00,0,512,0x3ACA00,8,1,0x22568};
                        for(unsigned n=0;n<14;++n)require(runtime.read32(runtime.memory().data(),0x3A97C0+n*4)==request[n],"LOADFILE module request differs");
                        constexpr char path[]="cdrom0:\\IRX\\SIO2MAN.IRX;1";
                        require(runtime.read32(runtime.memory().data(),0x3ACA00)==0 && std::memcmp(runtime.memory().data()+0x3ACA08,path,sizeof(path))==0,
                            "LOADFILE module path/arguments differ");
                    }
                    if(service_stop && !runtime.post_cd.supplied) {
                        require(runtime.post_sif.submissions==2 && runtime.post_sif.sreg_responses==1
                            && runtime.post_sif.ee_receive_address==0x3A94C0 && runtime.dmac_mask==0xA00000
                            && runtime.dmac_handlers.size()==1 && runtime.dmac_enable_calls==2
                            && runtime.read32(runtime.memory().data(),0x3A9740)==1
                            && runtime.read32(runtime.memory().data(),0x3A9598+8)==0x1E640,
                            "Fresh SIF ownership/handshake differs");
                        const std::array<std::uint32_t,4> descriptor={0x203A97C0,0x1E640,64,0x44};
                        for(unsigned n=0;n<descriptor.size();++n)require(runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+n*4)==descriptor[n],"Fresh RPC descriptor differs");
                        const std::array<std::uint32_t,9> packet={64,0,0x80000009,0,5,0x203A97C0,2,0x3A87A8,0x80000592};
                        for(unsigned n=0;n<packet.size();++n)require(runtime.read32(runtime.memory().data(),0x3A97C0+n*4)==packet[n],"Fresh RPC bind request differs");
                    }
                }
                if(!runtime.gs_csr_reset_writes) require(stop_address==0x12001000 && stop_size==8
                    && context.pc==0x30FE44 && runtime.guest_calls==230 && runtime.returns==226
                    && context.r[29].low==runtime.initial_sp-688 && !runtime.halfword_writes,
                    "First GS reset boundary differs");
                else if(!runtime.gs_imr_writes) require(runtime.gs_csr_reads==1 && stop_selector==0x71
                    && context.pc==0x32EFA8 && runtime.guest_calls==231 && runtime.returns==226
                    && context.r[29].low==runtime.initial_sp-688 && runtime.halfword_writes==3,
                    "GS IMR boundary differs");
                else if(!runtime.gs_crt_calls) require(stop_selector==2 && context.pc==0x32E868
                    && runtime.guest_calls==231 && runtime.returns==227
                    && context.r[29].low==runtime.initial_sp-608 && runtime.halfword_writes==4,
                    "GS CRT boundary differs");
                else if(runtime.gif_dma.starts && !runtime.post_sif.supplied) {
                    require(runtime.gif_dma.gs.packets==runtime.gif_dma.starts
                        && runtime.gif_dma.gs.sprites==runtime.gif_dma.starts
                        && runtime.gif_dma.gs.pixels==std::uint64_t(runtime.gif_dma.starts)*640*448
                        && runtime.gif_dma.qwc==0 && runtime.gif_dma.packet_verified
                        && runtime.gif_dma.idle_reads==runtime.gif_dma.starts
                        && runtime.gif_dma.staging_writes==runtime.gif_dma.starts*2,
                        "GS command consumption/GIF state differs");
                    if(runtime.gif_dma.starts==1) require(runtime.gif_dma.chcr==0x101
                        && runtime.gif_dma.madr==runtime.initial_sp-288 && !runtime.gif_dma.completions
                        && runtime.returns==252
                        && ((stop_target==0x310548 && context.pc==0x310548 && !stop_address
                                && context.r[29].low==runtime.initial_sp-608 && runtime.guest_calls==255)
                            || (!stop_target && !runtime.gif_dma.syncSupplied()
                                && stop_address==0x10009000 && stop_size==4 && context.pc==0x310560
                                && context.r[29].low==runtime.initial_sp-624 && runtime.guest_calls==256)),
                        "First GS synchronization evidence boundary differs");
                    else require(runtime.gif_dma.starts==2 && runtime.gif_dma.completions==2
                        && runtime.gif_dma.sync_reads==5 && runtime.gif_dma.vu_reads==2
                        && runtime.gif_dma.chcr==1 && runtime.gif_dma.madr==runtime.initial_sp-48
                        && stop_address==0x1000E010 && stop_size==4 && context.pc==0x3319CC
                        && context.r[29].low==runtime.initial_sp-192
                        && runtime.guest_calls==265 && runtime.returns==261 && runtime.dma_status_reads==1,
                        "Post-reset SIF DMA status boundary differs");
                    for(unsigned page : {0u,140u}) {
                        if(page==0 && runtime.gif_dma.starts==1)continue;
                        require(clearedToBlack(runtime.gif_dma.gs,page),"Startup framebuffer clear edge differs");
                    }
                }
                else if(runtime.gs_interrupt.acknowledgements==2 && !runtime.post_sif.supplied) require(runtime.gs_interrupt.polls==2
                    && runtime.gs_interrupt.events==1 && runtime.gs_interrupt.status()==8
                    && !runtime.gs_interrupt.callbackPending() && runtime.gs_csr_reset_writes==1
                    && runtime.gs_csr_reads==1 && runtime.gs_imr_writes==1 && runtime.gs_crt_calls==1
                    && runtime.gs_crt_request==std::array<std::uint32_t,3>{1,2,1} && runtime.halfword_writes==4
                    && ((!runtime.gs_field_reads && stop_address==0x12001000 && stop_size==8
                            && context.pc==0x3104F4 && runtime.guest_calls==254 && runtime.returns==250
                            && context.r[29].low==runtime.initial_sp-640)
                        || (runtime.gs_field_reads==1 && stop_address==0x1000A000 && stop_size==4
                            && (context.pc==0x3100E4 || (context.pc==0x3101A8 && runtime.gif_dma.ready() && GPR_U32(&context,4)==0x101
                                && runtime.gif_dma.idle_reads==1 && runtime.gif_dma.staging_writes==2
                                && runtime.gif_dma.qwc==15 && runtime.gif_dma.madr==runtime.initial_sp-528)) && runtime.guest_calls==255 && runtime.returns==251
                            && context.r[29].low==runtime.initial_sp-624)),
                    "INTC/field/GIF DMA diagnostic boundary differs");
                else if(!runtime.post_sif.supplied) require(runtime.gs_csr_reset_writes==1 && runtime.gs_csr_reads==1 && runtime.gs_imr_writes==1
                    && runtime.gs_crt_calls==1 && runtime.gs_crt_request==std::array<std::uint32_t,3>{1,2,1}
                    && stop_address==0x1000F000 && stop_size==4 && context.pc==0x32F10C
                    && runtime.guest_calls==252 && runtime.returns==247 && context.r[29].low==runtime.initial_sp-656
                    && runtime.halfword_writes==4 && GPR_U32(&context,4)==4,
                    "GS interrupt acknowledgement boundary differs");
            } else
            require((stop_target == 0x338108 && stop_source == 0x1003C8 && stop_resume == 0x1003D0
                        && runtime.guest_calls == 222 && runtime.returns == 220 && context.r[29].low == runtime.initial_sp - 32)
                    || (!stop_target && stop_selector == 0x7A && context.pc == 0x32F078
                        && runtime.guest_calls == 224 && runtime.returns == 220 && context.r[29].low == runtime.initial_sp - 48
                        && GPR_U32(&context,4) == 4), "Post-reboot readiness boundary differs");
        }
    }
}

void checkBindContinuation(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop,const std::vector<std::uint8_t>& kernel_before) {
    const auto stopped=stop.stopped;
    const auto stop_selector=stop.selector;
    const auto stop_handler=stop.handler;
    const auto stop_target=stop.target;
    require(stopped && !stop_target && !stop_handler && runtime.sif_bind_callbacks == 1
            && runtime.sif_bind_context_preservations == 1 && runtime.sif_dma_submissions == 2
            && runtime.semaphore_waits == 2 && runtime.semaphore_signals == 2
            && runtime.semaphore_deletions == 1 && runtime.startup_zero_verified
            && runtime.syscall_registrations == 18 && runtime.custom_syscall_calls == 17
            && runtime.kernel_word_writes == 668 && runtime.kernel_dispatch_validations == 12
            && runtime.timer3_mode_reads == 1 && runtime.timer2_mode_reads == 2
            && runtime.timer2_mode_writes == 1 && runtime.intc_mask == 0x1802
            && runtime.dmac_mask == 0xA00000 && runtime.dmac_enable_calls == 1
            && runtime.dma_status_reads == 1 && runtime.dma5_control_reads == 1
            && runtime.sif_rpc_reads == 1 && runtime.sif_command_reads == 1,
            "RPC bind continuation prerequisites/effects differ");
    require(runtime.read32(runtime.memory().data(), 0x3A87CC) == 0x3F570
            && runtime.read32(runtime.memory().data(), 0x3A87BC) == 0x3FB20
            && runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x100240
            && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0
            && std::equal(kernel_before.begin(), kernel_before.end(), runtime.kernelBaseline().begin()),
            "Bind response/persistent frame/kernel baseline differs");
    if (stop_selector == 0x77) require(context.pc == 0x32F028 && GPR_U32(&context, 5) == 2
            && GPR_U32(&context, 4) == runtime.initial_sp - 544
            && GPR_U32(&context, 29) == runtime.initial_sp - 544
            && runtime.rpc_call_semaphores == 1 && runtime.semaphores.size() == 4
            && runtime.semaphores[3].count == 0 && runtime.semaphores[3].maximum == 1
            && runtime.semaphores[3].option == 0x3838E8
            && runtime.guest_calls == 146 && runtime.returns == 139
            && runtime.word_writes == 1133 && runtime.doubleword_writes == 131
            && runtime.byte_writes == 530 && runtime.quadword_writes == 43632
            && runtime.uncached_word_accesses == 30 && runtime.uncached_byte_accesses == 4
            && runtime.data_cache_lines == 6, "Unexpected first RPC call submission");
    else require(stop_selector == 0x40 && context.pc == 0x32EC68
            && GPR_U32(&context, 4) == runtime.initial_sp - 400
            && runtime.rpc_call_semaphores == 0 && runtime.semaphores.size() == 3,
            "Unexpected RPC call semaphore checkpoint boundary");
    if (stop_selector == 0x77) {
        const std::array<std::uint32_t, 8> descriptors{0x3A8800, 0x3FB20, 4, 0,
            0x203A97C0, 0x19600, 64, 0x44};
        const std::array<std::uint32_t, 16> packet{0x440, 0x3FB20, 0x8000000A, 0,
            5, 0x203A97C0, 3, 0x3A87A8, 0, 4, 0x3695C0, 16, 1, 0x3F570, 0, 0};
        for (unsigned index = 0; index < descriptors.size(); ++index)
            require(runtime.read32(runtime.memory().data(), runtime.initial_sp - 544 + index * 4) == descriptors[index],
                    "Pending first CD-init RPC descriptors differ");
        for (unsigned index = 0; index < packet.size(); ++index)
            require(runtime.read32(runtime.memory().data(), 0x3A97C0 + index * 4) == packet[index],
                    "Pending first CD-init RPC packet differs");
    }
    require(context.hi == 0 && context.lo == 144 && context.hi1 == 0 && context.lo1 == 0
            && context.cop0_status == 0x10000 && GPR_U32(&context, 28) == 0x38BDF0,
            "CPU state changed across RPC continuation");
}

void checkEntryBoundary(EeRuntime& runtime,R5900Context& context,const ProbeModes& modes,const ProbeStop& stop,const std::vector<std::uint8_t>& kernel_before,const std::vector<std::uint8_t>& image,const std::array<std::uint32_t,5>& args) {
    const bool snapshot_mode=modes.snapshot,timer_mode=modes.timer,timer2_mode=modes.timer2,
        intc_mode=modes.intc,readback_mode=modes.readback,osd_mode=modes.osd,
        osd_readback_mode=modes.osd_readback,dma_status_mode=modes.dma_status,sif_rpc_mode=modes.sif_rpc,
        sif_dma_mode=modes.sif_dma,sif_mode=modes.sif,dma5_mode=modes.dma5;
    const auto stopped=stop.stopped;
    constexpr std::uint32_t zero_start=startup_zero_start,zero_end=startup_zero_end;
    require(stopped, "Entry did not stop at the expected checked boundary");
    require(runtime.startup_zero_verified && runtime.startup_quadword_writes == (zero_end - zero_start) / 16
            && runtime.startup_byte_writes == (zero_end - zero_start) % 16,
            "Entry zeroing did not complete before SetupThread");
    require(runtime.memory()[zero_start - 1] == image[zero_start - 1]
            && runtime.memory()[zero_end] == image[zero_end], "Entry zeroing crossed its bounds");
    require(runtime.quadword_writes == (zero_end - zero_start) / 16 + (timer_mode ? 512u : 0u)
            && runtime.byte_writes == (zero_end - zero_start) % 16 + (osd_readback_mode ? 512u : 0u) + (sif_rpc_mode ? 2u : sif_mode ? 1u : 0u), "Unexpected startup/fill store counts");
    require(context.hi == 0 && context.lo == (sif_dma_mode ? 144u : 0u) && context.hi1 == 0 && context.lo1 == 0
            && context.sa == 0 && context.fcr31 == 0 && context.f_acc == 0, "CPU startup state incorrect");
    require(std::all_of(context.f.begin(), context.f.end(), [](float value) { return value == 0; }), "FPR clearing failed");
    require(runtime.thread_arguments == std::array<std::uint32_t, 5>{0x0038BDF0, 0xFFFFFFFF, 0x00100000, 0x0038B980, 0x00100250},
            "SetupThread arguments differ from the reviewed entry");
    require(runtime.thread_initialized && runtime.initial_sp == 0x01FFED60
            && runtime.stack_base == 0x01EFF000 && GPR_U32(&context, 29) == runtime.initial_sp - (sif_rpc_mode ? 464 : sif_dma_mode ? 96 : sif_mode ? 240 : osd_readback_mode ? 192 : readback_mode ? 128 : intc_mode ? 48 : timer2_mode ? 160 : timer_mode ? 112 : snapshot_mode ? 80 : 144),
            "SetupThread stack model was not applied");
    require(GPR_U32(&context, 28) == runtime.thread_arguments[0], "Unexpected gp after SetupThread");
    require(runtime.heap_initialized && runtime.heap_start == 0x0043058F
            && runtime.heap_end == runtime.stack_base,
            "Unexpected SetupHeap state or return");
    if (!snapshot_mode) require(runtime.guest_calls == 9 && runtime.custom_syscall_calls == 1 && runtime.returns == 5 && runtime.doubleword_writes == 10 && runtime.word_writes == 8
            && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0x001001F0,
            "Generated callee did not save the caller return address");
    if (snapshot_mode) {
        require(runtime.guest_calls == (sif_rpc_mode ? 129u : sif_dma_mode ? 115u : sif_mode ? 108u : dma5_mode ? 105u : osd_readback_mode ? 99u : osd_mode ? 59u : readback_mode ? 58u : intc_mode ? 48u : timer2_mode ? 46u : timer_mode ? 43u : 12u) && runtime.custom_syscall_calls == (osd_readback_mode ? 17u : timer_mode ? 10u : 2u) && runtime.returns == (sif_rpc_mode ? 122u : sif_dma_mode ? 111u : sif_mode ? 102u : dma5_mode ? 100u : osd_readback_mode ? 95u : osd_mode ? 54u : readback_mode ? 53u : intc_mode ? 45u : timer_mode ? 41u : 10u)
                && runtime.word_writes == (sif_rpc_mode ? 1093u : sif_dma_mode ? 1062u : sif_mode ? 1040u : dma5_mode ? 1031u : osd_readback_mode ? 1030u : osd_mode ? 701u : readback_mode ? 699u : timer2_mode ? 682u : timer_mode ? 681u : 9u) && runtime.doubleword_writes == (sif_rpc_mode ? 107u : sif_mode ? 76u : dma5_mode ? 69u : osd_readback_mode ? 66u : readback_mode ? 40u : intc_mode ? 26u : timer2_mode ? 24u : timer_mode ? 21u : 14u),
                "Unexpected snapshot startup execution counts");
        require(runtime.read32(runtime.memory().data(), 0x0036AB40) == 0x80014D00,
                "Two generated scans did not converge on the captured table base");
    }
    if (timer_mode) {
        require(runtime.intc_handlers.size() == 2 && runtime.intc_handler_order == std::vector<std::uint32_t>{2, 1}
                && runtime.intc_handlers[0].cause == 12 && runtime.intc_handlers[0].address == 0x80076488
                && runtime.intc_handlers[1].cause == 11 && runtime.intc_handlers[1].address == 0x00339118
                && runtime.intc_handlers[1].arg == 0 && runtime.intc_handlers[1].gp == 0x0038BDF0
                && runtime.read32(runtime.memory().data(), 0x0036BAD8) == 2 && context.cop0_status == (osd_readback_mode ? 0x10000u : 0u),
                "Interrupt registration ID/state or generated status helper differs");
        require(runtime.timer3_mode_reads == 1, "Unexpected Timer 3 replay read count");
        require(runtime.kernel_word_writes == (osd_readback_mode ? 668u : 0x740 / 4) && runtime.cache_requests == (osd_readback_mode ? 6u : 2u) && runtime.instruction_cache_invalidations == (osd_readback_mode ? 2u : 1u),
                "Unexpected copied kernel size/cache request count");
        require(runtime.kernel_dispatch_validations == (osd_readback_mode ? 12u : 6u), "Unexpected copied kernel dispatch count");
        std::array<bool, 4> registered_slots{};
        for (std::uint32_t index = 0; index < 4; ++index) {
            const auto slot = runtime.read32(runtime.memory().data(), 0x80076710 + index * 8);
            require(slot >= 0xFC && slot <= 0xFF && !registered_slots[slot - 0xFC] && runtime.syscall_handlers[slot]
                    == runtime.read32(runtime.memory().data(), 0x80076714 + index * 8),
                    "Generated kernel table lookup/registration mismatch");
            registered_slots[slot - 0xFC] = true;
        }
        require(runtime.read32(runtime.memory().data(), 0x80076730) == 0x12C
                && runtime.read32(runtime.memory().data(), 0x80076734) == runtime.timer3_interrupt_handler
                && runtime.timer3_interrupt_handler == 0x80076488 && runtime.timer3_interrupt_enable_requested
                && runtime.extended_registrations == 1
                && runtime.syscall_handlers[8] == runtime.read32(runtime.memory().data(), 0x8007673C),
                "Completed kernel registrations differ from copied table");
        for (std::uint32_t offset = 0; offset < 0x740; offset += 4)
            require(runtime.read32(runtime.memory().data(), 0x80076000 + offset)
                    == runtime.read32(runtime.memory().data(), 0x0036B328 + offset), "Kernel copy mismatch");
        for (std::uint32_t offset = 0; offset < 0x28; offset += 4)
            require(runtime.read32(runtime.memory().data(), 0x00082000 + offset)
                    == runtime.read32(runtime.memory().data(), 0x0036BA68 + offset), "Main RAM copy mismatch");
    }
    if (osd_readback_mode) {
        for (std::uint32_t offset = 0; offset < 0x330; offset += 4)
            require(runtime.read32(runtime.memory().data(), 0x80075000 + offset)
                    == runtime.read32(runtime.memory().data(), 0x0036BAF8 + offset), "Startup kernel payload copy differs");
        require(runtime.syscall_handlers[0x5B] == 0x80075000, "Startup kernel entry not registered");
    }
    if (osd_readback_mode) {
        require(runtime.semaphore_waits == 1 && runtime.semaphore_signals == 1
                && runtime.read32(runtime.memory().data(), 0x003A8830) == 1
                && runtime.read32(runtime.memory().data(), 0x003A8838) == 0x003A8818
                && runtime.read32(runtime.memory().data(), 0x003A8820) == 0x0036C148,
                "Generated helper/semaphore list state differs");
        for (const auto offset : {0u, 4u, 12u, 16u, 20u})
            require(runtime.read32(runtime.memory().data(), 0x003A8818 + offset) == 0,
                    "Generated helper descriptor differs");
        for (std::uint32_t offset = 0x300; offset < 0x328; offset += 8) {
            const auto selector = runtime.read32(runtime.memory().data(), 0x80075000 + offset);
            const auto handler = runtime.read32(runtime.memory().data(), 0x80075004 + offset);
            require(runtime.syscall_handlers[selector] == handler, "Copied table registration differs");
        }
        require(runtime.read32(runtime.memory().data(), 0x0036BE70) == 0x80075330
                && runtime.read32(runtime.memory().data(), 0x0036BAF0) == 0
                && runtime.read32(runtime.memory().data(), 0x0036BAF4) == 0
                && std::all_of(runtime.memory().begin() + 0x003AF0C8, runtime.memory().begin() + 0x003AF2C8,
                               [](auto byte) { return byte == 0; }), "Generated startup cleanup differs");
    }
    require(runtime.dma_status_reads == (dma_status_mode ? 1u : 0u)
            && runtime.dma5_control_reads == (dma5_mode ? 1u : 0u), "Unexpected DMA checkpoint read counts");
    if (dma5_mode) {
        require(runtime.dmac_handlers.size() == 1 && runtime.dmac_handler_order == std::vector<std::uint32_t>{1}
                && runtime.dmac_handlers[0].channel == 5 && runtime.dmac_handlers[0].address == 0x00331DF0
                && runtime.dmac_handlers[0].arg == 0x20 && runtime.dmac_handlers[0].gp == 0x0038BDF0
                && runtime.dmac_mask == 0x00A00000 && runtime.dmac_enable_calls == 1
                && runtime.read32(runtime.memory().data(), 0x003A9594) == 1,
                "Generated DMA registration/enable state differs");
    } else require(runtime.dmac_handlers.empty() && runtime.dmac_enable_calls == 0,
                   "DMA handler state advanced before checkpoint");
    require(runtime.sif_command_reads == (sif_mode ? 1u : 0u)
            && runtime.data_cache_lines == (sif_rpc_mode ? 2u : sif_mode ? 1u : 0u), "Unexpected SIF/cache checkpoint counts");
    if (sif_mode && !sif_dma_mode) {
        const auto frame = runtime.initial_sp - 240;
        const std::array<std::uint32_t, 4> descriptor{0x003A9580, 0x19600, 20, 0x44};
        for (unsigned index = 0; index < descriptor.size(); ++index)
            require(runtime.read32(runtime.memory().data(), frame + index * 4) == descriptor[index],
                    "Generated SIF DMA descriptor differs");
        require(runtime.read32(runtime.memory().data(), 0x003A95A0) == 0x19600
                && runtime.read32(runtime.memory().data(), 0x003A9580) == 20
                && runtime.read32(runtime.memory().data(), 0x003A9584) == 0
                && runtime.read32(runtime.memory().data(), 0x003A9588) == 0x80000000
                && runtime.read32(runtime.memory().data(), 0x003A9590) == 0x003A94C0
                && runtime.read64(runtime.memory().data(), frame + 112) == 0x00331DA0
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 112) == 0x00332038
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x00100240
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0,
                "Generated SIF packet/current stack differs");
    }
    require(runtime.sif_dma_submissions == (sif_dma_mode ? 1u : 0u)
            && runtime.sif_rpc_reads == (sif_rpc_mode ? 1u : 0u)
            && runtime.uncached_word_accesses == (sif_rpc_mode ? 11u : 0u)
            && runtime.uncached_byte_accesses == (sif_rpc_mode ? 2u : 0u), "Unexpected receive/RPC/alias counts");
    if (sif_dma_mode) {
        const auto iop = runtime.sifIopMemory();
        const std::array<std::uint32_t, 5> consumed{0, 0, 0x80000000, 0, 0x3A94C0};
        require(iop.size() == EeRuntime::sif_iop_size && runtime.sif_ee_receive_address == 0x3A94C0
                && std::memcmp(iop.data() + 0x19600, consumed.data(), 20) == 0
                && std::memcmp(iop.data() + 0x196C8, &consumed[4], 4) == 0,
                "Bounded IOP receive effects differ from capture");
        const std::array<std::uint32_t, 4> handlers{0x3322E8, 0x3326C0, 0x3328E0, 0x3323F8};
        const std::array<unsigned, 4> ids{8, 9, 10, 12};
        for (unsigned index = 0; index < ids.size(); ++index) {
            const auto address = 0x3A95C0 + ids[index] * 12;
            require(runtime.read32(runtime.memory().data(), address) == handlers[index]
                    && runtime.read32(runtime.memory().data(), address + 4) == 0x3AAFC0
                    && runtime.read32(runtime.memory().data(), address + 8) == 0x38BDF0,
                    "Generated command handler registrations differ");
        }
        require(runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x00100240
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0,
                "Persistent main frame changed");
    }
    if (sif_rpc_mode) {
        const auto frame = runtime.initial_sp - 464;
        const std::array<std::uint32_t, 4> descriptor{0x203A97C0, 0x19600, 64, 0x44};
        for (unsigned index = 0; index < descriptor.size(); ++index)
            require(runtime.read32(runtime.memory().data(), frame + index * 4) == descriptor[index],
                    "Pending RPC DMA descriptor differs");
        require(runtime.read32(runtime.memory().data(), 0x3A97C0) == 64
                && runtime.read32(runtime.memory().data(), 0x3A97C8) == 0x80000009
                && runtime.read32(runtime.memory().data(), 0x3A97D8) == 2
                && runtime.read32(runtime.memory().data(), 0x3A97DC) == 0x3A87A8
                && runtime.read32(runtime.memory().data(), 0x3A97E0) == 0x80000592
                && runtime.read32(runtime.memory().data(), 0x3A87B0) == 3
                && runtime.semaphores.size() == 4 && runtime.semaphores[3].count == 0 && runtime.semaphores[3].maximum == 1
                && runtime.semaphores[3].option == 0x3838D8,
                "Pending RPC bind packet/semaphore differs");
        require(runtime.read64(runtime.memory().data(), frame + 112) == 0x331DA0
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 336) == 0x33284C
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 224) == 0x31F984
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 48) == 0x1003AC,
                "Active RPC bind frames differ");
    } else if (sif_dma_mode) require(runtime.read64(runtime.memory().data(), runtime.initial_sp - 48) == 0x1003A4,
                                  "Active RPC initialization frame differs");
    require(std::equal(kernel_before.begin(), kernel_before.end(), runtime.kernelBaseline().begin()),
            "Captured kernel baseline changed");
    if (timer_mode) {
        const auto frame = runtime.initial_sp - (osd_readback_mode ? 80 : readback_mode ? 128 : intc_mode ? 48 : 112);
        if (!sif_mode) require(runtime.read64(runtime.memory().data(), frame + (osd_readback_mode ? 48 : readback_mode ? 32 : intc_mode ? 16 : 80)) == (osd_readback_mode ? 0x00338568 : readback_mode ? 0x00338660 : intc_mode ? 0x00338550 : 0x00338548)
                && (osd_readback_mode
                    ? runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x00100240
                      && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0
                    : runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0x001001F0),
                "Current startup frames did not save return addresses");
        if (!intc_mode) for (const auto offset : {0u, 16u, 32u, 48u, 64u})
            require(runtime.read64(runtime.memory().data(), frame + offset) == 0, "Unexpected saved startup register");
        for (std::uint32_t index = 0; index < 128; ++index) {
            const auto block = 0x003ACCC0 + index * 64;
            require(runtime.read32(runtime.memory().data(), block) == (index == 127 ? 0 : block + 64),
                    "Generated buffer links differ from observed loop");
            require(std::all_of(runtime.memory().begin() + block + 4, runtime.memory().begin() + block + 64,
                               [](auto byte) { return byte == 0; }), "Generated buffer payload is not zero");
        }
    } else {
        const auto frame = runtime.initial_sp - 144;
        require(runtime.read64(runtime.memory().data(), frame + 112) == (snapshot_mode ? 0x00338540 : 0x00338538),
                "Current frame did not save return address");
        for (const auto offset : {0u, 16u, 32u, 48u, 64u, 80u, 96u})
            require(runtime.read64(runtime.memory().data(), frame + offset) == 0, "Unexpected saved callee register");
    }
    require(runtime.syscall_registrations == (osd_readback_mode ? 18u : timer_mode ? 10u : 2u) && runtime.syscall_handlers[0x83] == 0x00338388
            && runtime.syscall_handlers[0x5A] == (osd_readback_mode ? 0x0033A7C8u : timer_mode ? 0x003389B8u : 0x00338350u),
            "Expected syscall registrations missing");
    if (!snapshot_mode) require(args[0] == 0x80000000 && args[1] == 0x80080000 && args[2] == 0x00338388,
            "Unexpected reached guest call arguments");
    require(runtime.semaphores.size() == (sif_rpc_mode ? 4u : readback_mode ? 3u : 2u)
            && runtime.semaphores[0].count == 1 && runtime.semaphores[0].maximum == 1
            && runtime.semaphores[0].option == 0x00383BF0
            && runtime.semaphores[1].count == 1 && runtime.semaphores[1].maximum == 1
            && runtime.semaphores[1].option == 0x00383C00,
            "Unexpected created semaphore state");
    if (readback_mode) {
        require(runtime.threads.size() == 1 && runtime.current_thread_id == 1, "Dormant thread not recorded");
        const auto& thread = runtime.threads[0];
        require(thread.id == 2 && thread.entry == 0x0032F960 && thread.stack_base == 0x003A8840
                && thread.stack_size == 0x400 && thread.gp == 0x0038BDF0 && thread.initial_priority == 0
                && thread.heap_base == runtime.heap_start && thread.context_address == 0x003A89A0
                && thread.initial_sp == 0x003A8C20
                && runtime.read32(runtime.memory().data(), 0x0036AA84) == 2,
                "Generated CreateThread descriptor/return differs");
        for (const auto index : {28u, 29u, 30u, 31u}) {
            const auto expected = index == 28 ? thread.gp : index == 31 ? 0x1FC03000 : thread.initial_sp;
            require(runtime.read32(runtime.memory().data(), thread.context_address + index * 16) == expected,
                    "Dormant thread context initialization differs");
        }
    }
    if (readback_mode) require(runtime.semaphores[2].count == 0 && runtime.semaphores[2].maximum == 255
            && runtime.semaphores[2].option == 0x00383660
            && runtime.read32(runtime.memory().data(), 0x003A8C40) == 2,
            "Generated semaphore creation/result differs");
    require(runtime.read32(runtime.memory().data(), 0x0036AB58) == 0
            && runtime.read32(runtime.memory().data(), 0x0036AB5C) == 1,
            "Generated code did not store semaphore IDs");
}
