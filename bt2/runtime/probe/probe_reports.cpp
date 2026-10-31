#include "probe_state.h"
#include "game_starter.h"
#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// Entry-probe reports: the private JSON stop report for each checkpoint, the RAM/GS evidence
// sidecars, and the console summary.

namespace {
std::string jsonString(std::string_view value) {
    std::string result="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:value) {
        if(c=='"' || c=='\\') {result+='\\';result+=static_cast<char>(c);}
        else if(c<32) {result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}
        else result+=static_cast<char>(c);
    }
    return result+'"';
}
std::string jsonEEAddresses(const std::set<std::uint32_t>& addresses) {
    std::string out="[";bool first=true;
    for(auto address:addresses)if(address<0x80000000) {if(!first)out+=",";out+=std::to_string(address);first=false;}
    return out+"]";
}

// The interpreter bridge: instructions run interpreted, and each unmapped entry that ran (ee_interpreter.cpp).
std::string interpreterReport(const EeRuntime& runtime) {
    std::string out=",\"ee_interpreted_instructions\":"+std::to_string(runtime.interpreted_instructions)
        +",\"ee_interpret_all_instructions\":"+std::to_string(runtime.interpret_all_instructions)+",\"ee_interpreted_entries\":[";
    bool first=true;
    for(const auto& [entry,record]:runtime.interpreted_entries) {
        out+=std::string(first?"":",")+"{\"entry\":"+std::to_string(entry)+",\"dispatches\":"+std::to_string(record.dispatches)
            +",\"instructions\":"+std::to_string(record.instructions)+",\"first_word\":"+std::to_string(record.first_word)+"}";
        first=false;
    }
    // The system call numbers the game installed its own handlers for (SetSyscall), which the runtime runs.
    out+="],\"installed_syscalls\":[";
    first=true;
    for(std::size_t number=0;number<runtime.syscall_handlers.size();++number)
        if(runtime.syscall_handlers[number]){out+=std::string(first?"":",")+std::to_string(number);first=false;}
    return out+"]";
}

std::string jsonStartupThreads(const EeRuntime& runtime) {
    std::ostringstream out;out<<"[";bool first=true;
    for(const auto& t:runtime.threads)if(t.id>=4 && t.id<=7) {
        if(!first)out<<",";first=false;
        out<<"["<<t.id<<","<<t.entry<<","<<t.ready<<","<<t.suspended<<","<<t.current_priority
           <<","<<t.context.r[28].low<<","<<t.context.r[29].low<<","<<t.context.r[31].low<<","<<t.waiting<<"]";
    }
    out<<"]";return out.str();
}
}

void writeCdReport(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop,char** argv) {
    const auto stop_selector=stop.selector;
    const auto stop_target=stop.target;
    const auto stop_source=stop.source;
    const auto stop_resume=stop.resume;
    std::ofstream report(argv[2]);
    if (!report) throw std::runtime_error("Could not write CD continuation report");
    report << "{\"status\":\"blocked at unsupported syscall\",\"entry_executed\":true,\"game_booted\":false,\"pc\":" << context.pc << ",\"selector\":" << stop_selector
        << ",\"target\":" << stop_target << ",\"source_pc\":" << stop_source
        << ",\"resume_pc\":" << stop_resume << ",\"calls\":" << runtime.guest_calls
        << ",\"distinct_ee_entered\":" << std::count_if(runtime.entered_functions.begin(),runtime.entered_functions.end(),[](auto a){return a<0x80000000;})
        << ",\"distinct_ee_completed\":" << std::count_if(runtime.completed_functions.begin(),runtime.completed_functions.end(),[](auto a){return a<0x80000000;})
        << ",\"ee_entered_addresses\":" << jsonEEAddresses(runtime.entered_functions)
        << ",\"ee_completed_addresses\":" << jsonEEAddresses(runtime.completed_functions)
        << ",\"distinct_kernel_entered\":" << std::count_if(runtime.entered_functions.begin(),runtime.entered_functions.end(),[](auto a){return a>=0x80000000;})
        << ",\"distinct_kernel_completed\":" << std::count_if(runtime.completed_functions.begin(),runtime.completed_functions.end(),[](auto a){return a>=0x80000000;})
        << ",\"returns\":" << runtime.returns << ",\"words\":" << runtime.word_writes
        << ",\"doublewords\":" << runtime.doubleword_writes << ",\"bytes\":" << runtime.byte_writes
        << ",\"quads\":" << runtime.quadword_writes << ",\"cache_lines\":" << runtime.data_cache_lines
        << ",\"aliases\":" << runtime.uncached_word_accesses << ",\"byte_aliases\":" << runtime.uncached_byte_accesses
        << ",\"waits\":" << runtime.semaphore_waits << ",\"signals\":" << runtime.semaphore_signals
        << ",\"semas\":" << runtime.semaphores.size() << ",\"sp\":" << context.r[29].low
        << ",\"lo\":" << context.lo << ",\"cd_init_calls\":" << runtime.cd_init_calls
        << ",\"deletions\":" << runtime.semaphore_deletions
        << ",\"polls\":" << runtime.semaphore_polls << ",\"cd_semaphores\":" << runtime.cd_semaphores_created
        << ",\"arguments\":[" << GPR_U32(&context, 4) << "," << GPR_U32(&context, 5) << "]"
        << ",\"descriptor_words\":[";
    for (unsigned index = 0; index < (stop_target ? 0u : 6u); ++index)
        report << (index ? "," : "") << runtime.read32(runtime.memory().data(), GPR_U32(&context, 4) + index * 4);
    report << "]}\n";
}

void writeRuntimeBoundaryReport(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop,char** argv) {
    const auto execution_budget_exhausted=stop.execution_budget_exhausted;
    const auto stop_address=stop.address;
    const auto stop_size=stop.size;
    const auto stop_selector=stop.selector;
    const auto stop_target=stop.target;
    const auto stop_source=stop.source;
    const auto stop_resume=stop.resume;
    std::ofstream report(argv[2]);
    if (!report) throw std::runtime_error("Could not write SCMD report");
    report << "{\"status\":\"blocked at checked runtime boundary\",\"entry_executed\":true,\"game_booted\":false,\"pc\":" << context.pc
        << ",\"selector\":" << stop_selector << ",\"target\":" << stop_target << ",\"source_pc\":" << stop_source
        << ",\"resume_pc\":" << stop_resume << ",\"overlay_dispatch_validations\":" << runtime.overlay_dispatch_validations
        << ",\"interior_entry_dispatches\":" << runtime.interior_entry_dispatches
        << interpreterReport(runtime)
        << ",\"sound_tick_boundaries\":" << runtime.sound_tick_boundaries << ",\"timer1_starts\":" << runtime.timer1_starts << ",\"frame_budget_reached\":" << (runtime.frame_budget_reached?1:0) << ",\"timer1_counting\":" << (runtime.timer1.counting()?1:0) << ",\"timer1_mode\":" << runtime.timer1.mode() << ",\"timer1_polls\":" << runtime.timer1_polls
        << ",\"frame_gif_starts\":" << runtime.frame_gif.starts << ",\"frame_gif_chain_starts\":" << runtime.frame_gif.chain_starts << ",\"frame_gif_bytes\":" << runtime.frame_gif.bytes << ",\"frame_syncs\":" << runtime.frame_syncs
        << ",\"vif1_unpacks\":" << runtime.vif1.unpacks << ",\"vif1_programs\":" << runtime.vif1.programs << ",\"vu1_runs\":" << runtime.vif1.vu.runs
        << ",\"vu1_instructions\":" << runtime.vif1.vu.instructions << ",\"vu1_native_runs\":" << runtime.vif1.vu.native_runs << ",\"vu1_hand_written_runs\":" << runtime.vif1.vu.hand_written_runs << ",\"vu1_verified_runs\":" << runtime.vif1.vu.verified_runs << ",\"vu1_kicks\":" << runtime.vif1.vu.kicks << ",\"vu1_kick_hash\":\"" << std::hex << runtime.vif1.vu.kick_hash << std::dec << "\""
        << ",\"vu1_early_q_reads\":" << runtime.vif1.vu.early_q_reads << ",\"vu1_early_clip_reads\":" << runtime.vif1.vu.early_clip_reads
        << ",\"vif1_starts\":" << runtime.frame_vif1.starts << ",\"vif1_tags\":" << runtime.frame_vif1.tags << ",\"vif1_codes\":" << runtime.frame_vif1.codes << ",\"vif1_directs\":" << runtime.frame_vif1.directs << ",\"vif1_direct_qwords\":" << runtime.frame_vif1.direct_qwords << ",\"vif1_blocked_code\":" << runtime.frame_vif1.blocked_code << ",\"vif1_blocked_tag\":" << runtime.frame_vif1.blocked_tag << ",\"vif1_blocked_tadr\":" << runtime.frame_vif1.blocked_tadr
        << ",\"gs_transfers\":" << runtime.gif_dma.gs.transfers << ",\"gs_register_writes\":" << runtime.gif_dma.gs.register_writes
        << ",\"frame_gs_resets\":" << runtime.frame_gs_resets << ",\"frame_gs_reset_step\":" << runtime.frame_gs_reset_step << ",\"frame_gs_crt_request\":[" << runtime.frame_gs_crt_request[0] << "," << runtime.frame_gs_crt_request[1] << "," << runtime.frame_gs_crt_request[2] << "],\"frame_vsync_handler_removed\":" << (runtime.frame_vsync_handler_removed?1:0) << ",\"frame_vsync_handler\":" << runtime.frame_vsync_handler
                << ",\"gs_display_flips\":" << runtime.gs_display_flips << ",\"gs_display_writes\":" << runtime.gs_display_writes
        << ",\"gs_pmode\":" << runtime.gs_privileged[0] << ",\"gs_dispfb2\":" << runtime.gs_privileged[9] << ",\"gs_display2\":" << runtime.gs_privileged[10] << ",\"gs_dispfb1\":" << runtime.gs_privileged[7] << ",\"gs_display1\":" << runtime.gs_privileged[8]
        << ",\"execution_budget_exhausted\":" << (execution_budget_exhausted?1:0)
        << ",\"frame_vsync_deliveries\":" << runtime.frame_vsync_deliveries
        << ",\"timer1_stops\":" << runtime.timer1_stops << ",\"timer1_count\":" << runtime.timer1.count()
        << ",\"calls\":" << runtime.guest_calls << ",\"distinct_ee_entered\":" << std::count_if(runtime.entered_functions.begin(),runtime.entered_functions.end(),[](auto a){return a<0x80000000;})
        << ",\"distinct_ee_completed\":" << std::count_if(runtime.completed_functions.begin(),runtime.completed_functions.end(),[](auto a){return a<0x80000000;})
        << ",\"ee_entered_addresses\":" << jsonEEAddresses(runtime.entered_functions)
        << ",\"ee_completed_addresses\":" << jsonEEAddresses(runtime.completed_functions)
        << ",\"distinct_kernel_entered\":" << std::count_if(runtime.entered_functions.begin(),runtime.entered_functions.end(),[](auto a){return a>=0x80000000;})
        << ",\"distinct_kernel_completed\":" << std::count_if(runtime.completed_functions.begin(),runtime.completed_functions.end(),[](auto a){return a>=0x80000000;})
        << ",\"returns\":" << runtime.returns
        << ",\"words\":" << runtime.word_writes << ",\"doublewords\":" << runtime.doubleword_writes
        << ",\"bytes\":" << runtime.byte_writes << ",\"quads\":" << runtime.quadword_writes
        << ",\"waits\":" << runtime.semaphore_waits << ",\"signals\":" << runtime.semaphore_signals
        << ",\"deletions\":" << runtime.semaphore_deletions << ",\"dma_disables\":" << runtime.dmac_disable_calls
        << ",\"dma_removals\":" << runtime.dmac_removals << ",\"media_mode_calls\":" << runtime.scmd_mmode_calls
        << ",\"sif_stops\":" << runtime.sif_stop_calls << ",\"reset_subaddr_reads\":" << runtime.sif_reset_subaddr_reads
        << ",\"smflag_writes\":" << runtime.reset_smflag_writes
        << ",\"reset_clear_writes\":" << runtime.reset_clear_writes << ",\"iop_epoch\":" << runtime.iop_epoch
        << ",\"halfwords\":" << runtime.halfword_writes << ",\"gs_reset_writes\":" << runtime.gs_csr_reset_writes
        << ",\"gs_imr_writes\":" << runtime.gs_imr_writes << ",\"gs_crt_calls\":" << runtime.gs_crt_calls
        << ",\"native_module_loads\":" << runtime.native_module_loads
        << ",\"native_module_request\":" << std::quoted(runtime.native_module_request)
        << ",\"heap_transfers\":" << runtime.heap_transfers << ",\"heap_dma_polls\":" << runtime.heap_dma_polls
        << ",\"console_messages\":" << runtime.ee_console_messages.size()
        << ",\"console_first_message\":" << jsonString(runtime.ee_console_messages.empty()?std::string{}:runtime.ee_console_messages.front());
    report << ",\"console_messages_text\":[";
    for(unsigned n=0;n<runtime.ee_console_messages.size();++n){if(n)report<<",";report<<jsonString(runtime.ee_console_messages[n]);}
    report << "]"
        << ",\"indexed_cache_operations\":" << runtime.indexed_cache_operations
        << ",\"indexed_cache_tag_reads\":" << runtime.indexed_cache_tag_reads
        << ",\"ee_status\":" << context.cop0_status
        << ",\"ee_thread_count\":" << runtime.threads.size()
        << ",\"sound_thread_starts\":" << runtime.sound_thread_starts
        << ",\"dbc_rpc_binds\":" << runtime.dbc_rpc_binds
        << ",\"mc_rpc_binds\":" << runtime.mc_rpc_binds
        << ",\"mc_rpc_calls\":" << runtime.mc_rpc_calls
        << ",\"mc_frame_queries\":" << runtime.mc_frame_queries
        << ",\"dbc_rpc_calls\":" << runtime.dbc_rpc_calls
        << ",\"dbc_lock_waits\":" << runtime.dbc_lock_waits
        << ",\"dbc_lock_signals\":" << runtime.dbc_lock_signals
        << ",\"disc_bind_semaphores\":" << runtime.disc_bind_semaphores << ",\"disc_rpc_binds\":" << runtime.disc_rpc_binds << ",\"disc_rpc_calls\":" << runtime.disc_rpc_calls << ",\"ncmd_ready_calls\":" << runtime.ncmd_ready_calls << ",\"ncmd_read_calls\":" << runtime.ncmd_read_calls << ",\"cri_rpc_binds\":" << runtime.cri_rpc_binds << ",\"cri_rpc_calls\":" << runtime.cri_rpc_calls 
        << ",\"probe_initial_sp\":" << runtime.initial_sp << ",\"probe_semaphore_count\":" << runtime.semaphores.size() << ",\"ncmd_rpc_binds\":" << runtime.ncmd_rpc_binds << ",\"ncmd_bind_semaphores\":" << runtime.ncmd_bind_semaphores << ",\"disc_result\":" << runtime.read32(runtime.memory().data(),0x3A8740)
        << ",\"stop_returns\":[" << [&]{std::string s;for(auto a:runtime.stop_returns){if(!s.empty())s+=',';s+=std::to_string(a);}return s;}() << "]"
        << ",\"stop_ra\":" << GPR_U32(&context,31) << ",\"stop_s1\":" << GPR_U32(&context,17) << ",\"stop_s3\":" << GPR_U32(&context,19)
        << ",\"sounds_rpc_binds\":" << runtime.sounds_rpc_binds
        << ",\"sounds_rpc_calls\":" << runtime.sounds_rpc_calls
        << ",\"sound_rpc_binds\":" << runtime.sound_rpc_binds
        << ",\"sound_rpc_calls\":" << runtime.sound_rpc_calls
        << ",\"sound_client_words\":[";
    for(unsigned i=0;i<10;++i)report<<(i?",":"")<<runtime.read32(runtime.memory().data(),0x3A5C80+i*4);
    report<<"],\"sound_payload_words\":[";
    for(unsigned i=0;i<16;++i)report<<(i?",":"")<<runtime.read32(runtime.memory().data(),0x3A5C40+i*4);
    report<<"],\"sound_thread_words\":[";
    report<<"],\"sound_iop_context\":[";
    report<<"],\"cri_thread_words\":[";
    report<<"],\"spu2_auto_dma_words\":[";
    report<<"],\"ee_sound_context\":[";
    if(runtime.sound_thread_starts) {
        const auto& t=runtime.threads.at(1);const auto& c=t.context;
        report<<t.id<<","<<t.ready<<","<<c.pc<<","<<c.r[28].low<<","<<c.r[29].low<<","<<c.r[31].low<<","<<c.r[4].low<<","<<c.cop0_status;
    }
    report<<"],\"iop_rpc_servers\":[";
    report<<"],\"dbc_client_words\":[";
    for(const auto client:std::array<std::uint32_t,4>{0x3A03C0,0x3A0410,0x3A0438,0x3A0460}) {
        if(client!=0x3A03C0)report<<",";report<<"[";
        for(unsigned n=0;n<10;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),client+n*4);report<<"]";
    }
    report<<"],\"dbc_dma_ee_words\":[";
    for(unsigned n=0;n<32;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A27C0+n*4);
    report<<"],\"dbc_dma_ee_at_transfer_words\":[";
    report<<"],\"dbc_dma_payload_words\":[";
    report<<"],\"mc_dma_ee_words\":[";
    for(unsigned n=0;n<48;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A7480+n*4);
    report<<"],\"pending_ee_bind\":[";
    report<<"],\"mc_dma_payload_words\":[";
    report<<"],\"sio2_dma_channels\":[";
    report<<"],\"sio2_card_transfer_counts\":[";
    report<<"],\"sio2_card_controls\":[";
    report<<"],\"native_iop_import_arguments\":[";
    report<<"],\"mc_client_words\":[";
    for(unsigned n=0;n<10;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A6F80+n*4);
    report<<"],\"mc_input_words\":[";
    for(unsigned n=0;n<12;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A7000+n*4);
    report<<"],\"mc_result_words\":[";
    for(unsigned n=0;n<3;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A8540+n*4);
    report<<"],\"dbc_result_words\":[";
    for(unsigned n=0;n<36;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A04C0+n*4);
    report<<"],\"sounds_client_words\":[";
    if(runtime.sounds_rpc_binds)for(unsigned n=0;n<10;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x6351C0+n*4);
    report<<"],\"sounds_result_words\":[";
    if(runtime.sounds_rpc_calls)for(unsigned n=0;n<16;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x635180+n*4);
    report<<"],\"heap_calls\":" << runtime.heap_calls
        << ",\"heap_frees\":" << runtime.heap_frees
        << ",\"dmac_startup_writes\":" << runtime.dmac_startup_writes
        << ",\"dmac_control\":" << runtime.dmac_control << ",\"dmac_priority\":" << runtime.dmac_priority
        << ",\"ee_pool_pointers\":[" << runtime.read32(runtime.memory().data(),0x38A628) << "," << runtime.read32(runtime.memory().data(),0x38A62C) << "," << runtime.read32(runtime.memory().data(),0x38A630) << "," << runtime.read32(runtime.memory().data(),0x38A634) << "]"
        << ",\"startup_worker_dispatches\":" << runtime.startup_worker_dispatches
        << ",\"startup_worker_status_queries\":" << runtime.startup_worker_status_queries
        << ",\"startup_worker_waits\":" << runtime.startup_worker_waits
        << ",\"main_priority_admissions\":" << runtime.main_priority_admissions << ",\"main_priority_previous\":" << runtime.main_priority_previous
        << ",\"startup_scheduler_active\":" << runtime.startup_scheduler_active << ",\"current_thread_id\":" << runtime.current_thread_id
        << ",\"cri_stream_commands\":[" << [&]{std::string s;for(const auto& [key,count]:runtime.native_cri_audio.commands){if(!s.empty())s+=',';s+="["+std::to_string(key[0])+","+std::to_string(key[1])+","+std::to_string(key[2])+","+std::to_string(count)+"]";}return s;}() << "]"
        << "," << gameStarterReport() << ",\"sounds_bank_replacements\":" << runtime.sounds_bank_replacements << ",\"sound_effects_played\":" << runtime.sound_sampler.played << ",\"sound_effects_looping\":" << runtime.sound_sampler.looping << ",\"sound_effects_kept\":" << runtime.sound_sampler.kept << ",\"sound_effects_kept_refused\":" << runtime.sound_sampler.kept_refused
        << ",\"sound_effects_stopped\":" << runtime.sound_sampler.stopped << ",\"sound_effects_stops_unmatched\":" << runtime.sound_sampler.stops_unmatched << ",\"sound_effects_silenced\":" << runtime.sound_sampler.silenced
        << ",\"sound_voices_peak\":" << runtime.sound_sampler.peak_voices << ",\"sound_voices_stolen\":" << runtime.sound_sampler.stolen
        << ",\"audio_fields\":" << runtime.audio_fields << ",\"audio_stream_bytes\":" << runtime.audio_stream_bytes << ",\"audio_single_stream_bytes\":" << runtime.audio_single_stream_bytes << ",\"audio_voice_samples\":" << runtime.audio_mixer.voice_samples << ",\"audio_underruns\":" << runtime.audio_mixer.underruns
        << ",\"gs_downloads\":" << runtime.gs_readback.downloads << ",\"gs_download_qwords\":" << runtime.gs_readback.dma_qwords+runtime.gs_readback.fifo_qwords << ",\"vif1_sends\":" << runtime.frame_vif1.sends
        << ",\"busy_main_vblanks\":" << runtime.busy_main_vblanks << ",\"ipu_commands\":" << runtime.ipu.commands << ",\"ipu_vlc_decodes\":" << runtime.ipu.vlc_decodes << ",\"ipu_macroblocks\":" << runtime.ipu.macroblocks << ",\"ipu_converted_macroblocks\":" << runtime.ipu.converted_macroblocks << ",\"ipu_output_qwords\":" << runtime.ipu.output_qwords << ",\"ipu_input_qwords\":" << runtime.ipu.input.dma_qwords << ",\"scratchpad_transfers\":" << runtime.scratchpad_transfers
        << ",\"scheduled_priority_changes\":" << runtime.scheduled_priority_changes << ",\"scheduled_resumes\":" << runtime.scheduled_resumes << ",\"scheduled_wakes\":" << runtime.scheduled_wakes << ",\"main_yields\":" << runtime.main_yields << ",\"native_disc_requests\":" << runtime.native_disc_requests << ",\"native_fileio_requests\":" << runtime.native_fileio_requests << ",\"native_card_requests\":" << runtime.native_card_requests << ",\"native_pad_requests\":" << runtime.native_pad_requests << ",\"native_pad_transfers\":" << runtime.native_pad_transfers << ",\"native_cri_requests\":" << runtime.native_cri_requests << ",\"native_cri_returns\":" << runtime.native_cri_returns << ",\"native_cri_commands\":" << runtime.native_cri_commands << ",\"main_sleeps\":" << runtime.main_sleeps << ",\"main_wakes\":" << runtime.main_wakes << ",\"main_sleep_fields\":" << runtime.main_sleep_fields << ",\"frame_csr_reads\":" << runtime.frame_csr_reads
        << ",\"scheduled_suspends\":" << runtime.scheduled_suspends << ",\"cri_stream_transfers\":" << runtime.cri_stream_transfers
        << ",\"file_load_idles\":" << runtime.file_load_idles << ",\"fileio_binds\":" << runtime.fileio_binds << ",\"fileio_calls\":" << runtime.fileio_calls << ",\"fileio_completions\":" << runtime.fileio_completions << ",\"iop_uploads\":" << runtime.iop_uploads << ",\"iop_upload_polls\":" << runtime.iop_upload_polls << ",\"timer2_fields\":" << runtime.timer2.fields << ",\"timer2_wraps\":" << runtime.timer2.wraps << ",\"timer2_deliveries\":" << runtime.timer2_deliveries << ",\"display_reset_phase\":" << runtime.display_reset_phase << ",\"polled_vblanks\":" << runtime.polled_vblanks << ",\"vblank_handler_registrations\":" << runtime.vblank_handler_registrations << ",\"vblank_enable_calls\":" << runtime.vblank_enable_calls
        << ",\"vblank_disable_calls\":" << runtime.vblank_disable_calls << ",\"load_poll_idles\":" << runtime.load_poll_idles << ",\"asset_poll_idles\":" << runtime.asset_poll_idles << ",\"timer1_vblanks\":" << runtime.timer1_vblanks << ",\"dbc_port_calls\":" << runtime.dbc_port_calls << ",\"dbc_main_calls\":" << runtime.dbc_main_calls << ",\"scmd_calls\":" << runtime.scmd_calls
        << ",\"vblank_deliveries\":" << runtime.vblank_deliveries << ",\"worker_wakes\":" << runtime.worker_wakes
        << ",\"worker_resumes\":" << runtime.worker_resumes << ",\"worker_sleep_returns\":" << runtime.worker_sleep_returns
        << ",\"startup_dispatch_parent_pc\":" << runtime.startup_dispatch_parent.pc
        << ",\"queued_sound_submissions\":" << runtime.queued_sound_submissions
        << ",\"queued_sound_completions\":" << runtime.queued_sound_completions
        << ",\"sound_thread_waiting\":" << runtime.sound_thread_waiting
        << ",\"sound_dispatch_bind_id\":" << runtime.sound_dispatch_bind_id
        << ",\"sound_dispatch_wait_ra\":" << runtime.sound_dispatch_wait_ra
        << ",\"sound_dispatch_wait_sp\":" << runtime.sound_dispatch_wait_sp
        << ",\"ee_sound_probe_attempted\":" << runtime.ee_sound_probe_attempted
        << ",\"ee_sound_probe_waits\":" << runtime.ee_sound_probe_waits
        << ",\"ee_sound_probe_parent_pc\":" << runtime.ee_sound_probe_parent.pc
        << ",\"ee_sound_probe_frontier\":[" << runtime.ee_sound_probe_frontier.pc << "," << runtime.ee_sound_probe_frontier.r[28].low
        << "," << runtime.ee_sound_probe_frontier.r[29].low << "," << runtime.ee_sound_probe_frontier.r[31].low
        << "," << runtime.ee_sound_probe_frontier.r[16].low << "," << runtime.ee_sound_probe_frontier.cop0_status << "]"
        << ",\"main_thread_priority\":" << runtime.main_thread_priority << ",\"main_thread_queries\":" << runtime.main_thread_queries << ",\"startup_worker_state\":" << jsonStartupThreads(runtime)
        << ",\"startup_worker_queries\":" << runtime.startup_worker_queries << ",\"startup_worker_starts\":" << runtime.startup_worker_starts
        << ",\"startup_ready_queries\":" << runtime.startup_ready_queries
        << ",\"startup_worker_suspends\":" << runtime.startup_worker_suspends << ",\"startup_worker_priority_changes\":" << runtime.startup_worker_priority_changes
        << ",\"heap_end_queries\":" << runtime.heap_end_queries
        << ",\"ee_heap_lock_waits\":" << runtime.ee_heap_lock_waits
        << ",\"ee_heap_lock_signals\":" << runtime.ee_heap_lock_signals
        << ",\"timer1_setup_writes\":" << runtime.timer1_setup_writes
        << ",\"timer1_mode\":" << runtime.timer1_mode
        << ",\"texture_gif_phase\":" << runtime.texture_gif.phase
        << ",\"texture_gif_starts\":" << runtime.texture_gif.starts
        << ",\"texture_gif_completions\":" << runtime.texture_gif.completions
        << ",\"texture_gif_polls\":" << runtime.texture_gif.polls
        << ",\"tex0\":[" << runtime.gif_dma.gs.tex0[0] << "," << runtime.gif_dma.gs.tex0[1] << "]"
        << ",\"pabe\":" << runtime.gif_dma.gs.pabe
        << ",\"vif_phase\":" << runtime.vif_startup.phase << ",\"vif_commands\":" << runtime.vif_startup.commands
        << ",\"graph_startup_phase\":" << runtime.graph_startup_phase
        << ",\"vif_words\":" << runtime.vif_startup.words << ",\"vif_fifo_writes\":" << runtime.vif_startup.fifo_writes
        << ",\"vu_resets\":" << runtime.vif_startup.vu_resets << ",\"gif_resets\":" << runtime.vif_startup.gif_resets
        << ",\"vif_registers\":[" << runtime.vif_startup.error << ',' << runtime.vif_startup.cycle << ',' << runtime.vif_startup.mode
        << ',' << runtime.vif_startup.mask << ',' << runtime.vif_startup.code << ',' << runtime.vif_startup.itops
        << ',' << runtime.vif_startup.base << ',' << runtime.vif_startup.offset << ',' << runtime.vif_startup.tops << ']'
        << ",\"heap_binds\":" << runtime.heap_binds
        << ",\"loadfile_binds\":" << runtime.post_loadfile.binds << ",\"loadfile_versions\":" << runtime.post_loadfile.versions
        << ",\"post_cd_binds\":" << runtime.post_cd.binds << ",\"post_cd_calls\":" << runtime.post_cd.calls << ",\"post_scmd_binds\":" << runtime.post_cd.scmd_binds << ",\"post_scmd_modes\":" << runtime.post_cd.modes
        << ",\"post_sif_phase\":" << runtime.post_sif.phase
        << ",\"post_sif_submissions\":" << runtime.post_sif.submissions
        << ",\"post_sif_responses\":" << runtime.post_sif.sreg_responses
        << ",\"gif_completions\":" << runtime.gif_dma.completions
        << ",\"gif_sync_reads\":" << runtime.gif_dma.sync_reads
        << ",\"gs_packets\":" << runtime.gif_dma.gs.packets
        << ",\"gs_sprites\":" << runtime.gif_dma.gs.sprites
        << ",\"gs_pixels\":" << runtime.gif_dma.gs.pixels
        << ",\"gif_starts\":" << runtime.gif_dma.starts
        << ",\"gif_idle_reads\":" << runtime.gif_dma.idle_reads
        << ",\"gif_staging_writes\":" << runtime.gif_dma.staging_writes
        << ",\"gif_qwc\":" << runtime.gif_dma.qwc
        << ",\"gif_madr\":" << runtime.gif_dma.madr
        << ",\"gif_reserved_difference_words\":" << runtime.gif_dma.reserved_difference_words
        << ",\"gif_packet_verified\":" << (runtime.gif_dma.packet_verified ? "true" : "false")
        << ",\"intc_acks\":" << runtime.gs_interrupt.acknowledgements
        << ",\"intc_polls\":" << runtime.gs_interrupt.polls
        << ",\"intc_events\":" << runtime.gs_interrupt.events
        << ",\"gs_field_reads\":" << runtime.gs_field_reads
        << ",\"gs_csr_reads\":" << runtime.gs_csr_reads
        << ",\"memory_address\":" << stop_address << ",\"memory_size\":" << stop_size
        << ",\"reset_ready_checkpoint_reads\":" << runtime.reset_ready_checkpoint_reads
        << ",\"iop_reset_pending\":" << (runtime.iop_reset_pending ? "true" : "false")
        << ",\"prior_iop_services_valid\":" << (runtime.prior_iop_services_valid ? "true" : "false")
        << ",\"sp\":" << context.r[29].low << ",\"scmd_binds\":" << runtime.scmd_bind_calls << ",\"arguments\":[" << GPR_U32(&context,4) << "," << GPR_U32(&context,5) << "]"
        << ",\"descriptor_words\":[";
    for(unsigned i=0;i<((stop_selector==0x40 || stop_selector==0x77)?8u:0u);++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+i*4);
    report << "],\"packet_words\":[";
    for(unsigned i=0;i<16;++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),0x3A97C0+i*4);
    report << "],\"heap_client_words\":[";
    for(unsigned i=0;i<10;++i)report << (i?",":"") << runtime.read32(runtime.memory().data(),0x3AC840+i*4);
    report << "],\"reset_packet_words\":[";
    for(unsigned i=0;i<26;++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),0x3ACC40+i*4);
    report << "]}\n";
}

void writeEvidenceDumps(EeRuntime& runtime,char** argv) {
    if(runtime.heap_calls) {
        if(runtime.ncmd_read_calls) { // After the first disc read.
            std::ofstream ee_dump(std::string(argv[2])+".ee.bin",std::ios::binary);
            ee_dump.write(reinterpret_cast<const char*>(runtime.memory().data()),runtime.memory().size());
            if(!ee_dump)throw std::runtime_error("EE evidence dump failed");
        }
        const auto& iop=runtime.nativeIopMemory(); // Native IOP memory store (uploaded data).
        std::ofstream dump(std::string(argv[2])+".iop.bin",std::ios::binary);
        if(!dump.write(reinterpret_cast<const char*>(iop.data()),iop.size()))throw std::runtime_error("IOP checkpoint write failed");
    }
}

void writeBindReport(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop,char** argv) {
    const auto stop_address=stop.address;
    const auto stop_selector=stop.selector;
    std::ofstream report(argv[2]);
    if (!report) throw std::runtime_error("Could not write bind continuation report");
    report << "{\"status\":\"blocked at unsupported syscall\",\"entry_executed\":true,\"game_booted\":false,\"pc\":" << context.pc << ",\"selector\":" << stop_selector
        << ",\"target\":" << stop_address << ",\"calls\":" << runtime.guest_calls
        << ",\"distinct_ee_entered\":" << std::count_if(runtime.entered_functions.begin(),runtime.entered_functions.end(),[](auto a){return a<0x80000000;})
        << ",\"distinct_ee_completed\":" << std::count_if(runtime.completed_functions.begin(),runtime.completed_functions.end(),[](auto a){return a<0x80000000;})
        << ",\"ee_entered_addresses\":" << jsonEEAddresses(runtime.entered_functions)
        << ",\"ee_completed_addresses\":" << jsonEEAddresses(runtime.completed_functions)
        << ",\"distinct_kernel_entered\":" << std::count_if(runtime.entered_functions.begin(),runtime.entered_functions.end(),[](auto a){return a>=0x80000000;})
        << ",\"distinct_kernel_completed\":" << std::count_if(runtime.completed_functions.begin(),runtime.completed_functions.end(),[](auto a){return a>=0x80000000;})
        << ",\"returns\":" << runtime.returns << ",\"words\":" << runtime.word_writes
        << ",\"doublewords\":" << runtime.doubleword_writes << ",\"bytes\":" << runtime.byte_writes
        << ",\"quads\":" << runtime.quadword_writes << ",\"cache_lines\":" << runtime.data_cache_lines
        << ",\"aliases\":" << runtime.uncached_word_accesses << ",\"byte_aliases\":" << runtime.uncached_byte_accesses
        << ",\"waits\":" << runtime.semaphore_waits << ",\"signals\":" << runtime.semaphore_signals
        << ",\"semas\":" << runtime.semaphores.size() << ",\"sp\":" << context.r[29].low
        << ",\"lo\":" << context.lo << ",\"bind_callbacks\":" << runtime.sif_bind_callbacks
        << ",\"bind_context_preservations\":" << runtime.sif_bind_context_preservations
        << ",\"semaphore_deletions\":" << runtime.semaphore_deletions
        << ",\"call_semaphore_unused_attr\":" << runtime.rpc_call_sema_attributes
        << ",\"arguments\":[" << GPR_U32(&context, 4) << "," << GPR_U32(&context, 5) << "]"
        << ",\"descriptor_words\":[";
    for (unsigned index = 0; index < (stop_selector == 0x77 ? 8u : 6u); ++index)
        report << (index ? "," : "") << runtime.read32(runtime.memory().data(), GPR_U32(&context, 4) + index * 4);
    report << "],\"packet_words\":[";
    for (unsigned index = 0; index < 16; ++index)
        report << (index ? "," : "") << runtime.read32(runtime.memory().data(), 0x3A97C0 + index * 4);
    report << "]}\n";
}

void writeEntryReport(EeRuntime& runtime,R5900Context& context,const ProbeModes& modes,const ProbeStop& stop,const std::array<std::uint32_t,5>& args,char** argv) {
    const bool snapshot_mode=modes.snapshot,timer_mode=modes.timer,timer2_mode=modes.timer2,
        intc_mode=modes.intc,readback_mode=modes.readback,osd_readback_mode=modes.osd_readback,
        dma5_mode=modes.dma5;
    const auto stop_address=stop.address;
    const auto stop_size=stop.size;
    const auto stop_selector=stop.selector;
    const auto stop_handler=stop.handler;
    const auto stop_target=stop.target;
    const auto stop_source=stop.source;
    const auto stop_resume=stop.resume;
    std::ofstream report(argv[2]);
    if (!report) throw std::runtime_error("Could not write entry stop report");
    report << "{\"status\":\"" << (dma5_mode ? "blocked at unsupported syscall" : osd_readback_mode ? "blocked at unsupported guest memory" : readback_mode ? "blocked at unsupported syscall" : intc_mode ? "blocked at unsupported guest memory" : timer2_mode ? "blocked at unsupported syscall" : "blocked at unsupported guest memory") << "\",\"entry_executed\":true,"
           << "\"target\":" << stop_target << ",\"source_pc\":" << stop_source << ",\"resume_pc\":" << stop_resume << ","
           << "\"selector\":" << stop_selector << ","
           << "\"kernel_word_writes\":" << runtime.kernel_word_writes << ",\"access\":\"" << (dma5_mode ? "syscall" : osd_readback_mode ? "read" : readback_mode ? "syscall" : intc_mode ? "read" : timer2_mode ? "syscall" : "read") << "\",\"address\":" << stop_address << ",\"size\":" << stop_size
           << ",\"timer3_interrupt_handler\":" << runtime.timer3_interrupt_handler
           << ",\"timer3_interrupt_enable_requested\":" << (runtime.timer3_interrupt_enable_requested ? "true" : "false")
           << ",\"extended_registrations\":" << runtime.extended_registrations
           << ",\"startup_zero_verified\":" << (runtime.startup_zero_verified ? "true" : "false")
           << ",\"kernel_dispatch_validations\":" << runtime.kernel_dispatch_validations
           << ",\"overlay_dispatch_validations\":" << runtime.overlay_dispatch_validations
           << ",\"interior_entry_dispatches\":" << runtime.interior_entry_dispatches
        << interpreterReport(runtime)
           << ",\"sif_dma_submissions\":" << runtime.sif_dma_submissions << ",\"sif_rpc_reads\":" << runtime.sif_rpc_reads
           << ",\"uncached_word_accesses\":" << runtime.uncached_word_accesses << ",\"uncached_byte_accesses\":" << runtime.uncached_byte_accesses
           << ",\"sif_command_reads\":" << runtime.sif_command_reads << ",\"data_cache_lines\":" << runtime.data_cache_lines
           << ",\"dma_status_reads\":" << runtime.dma_status_reads << ",\"dma5_control_reads\":" << runtime.dma5_control_reads
           << ",\"dmac_handlers\":" << runtime.dmac_handlers.size() << ",\"dmac_mask\":" << runtime.dmac_mask
           << ",\"dmac_enable_calls\":" << runtime.dmac_enable_calls
           << ",\"cache_requests\":" << runtime.cache_requests << ",\"instruction_cache_invalidations\":" << runtime.instruction_cache_invalidations << ",\"syscall_selector\":" << (osd_readback_mode ? stop_selector : GPR_U32(&context, 3))
           << ",\"write_value\":" << (osd_readback_mode ? 0 : runtime.timer2_mode_writes ? 0x382 : 0)
           << ",\"osd_config_reads\":" << runtime.osd_config_reads << ",\"osd_config_sets\":" << runtime.osd_config_sets
           << ",\"ee_sound_probe_attempted\":" << runtime.ee_sound_probe_attempted
            << ",\"ee_sound_probe_waits\":" << runtime.ee_sound_probe_waits
            << ",\"ee_sound_probe_parent_pc\":" << runtime.ee_sound_probe_parent.pc
            << ",\"ee_sound_probe_frontier\":[" << runtime.ee_sound_probe_frontier.pc << "," << runtime.ee_sound_probe_frontier.r[28].low
            << "," << runtime.ee_sound_probe_frontier.r[29].low << "," << runtime.ee_sound_probe_frontier.r[31].low
            << "," << runtime.ee_sound_probe_frontier.r[16].low << "," << runtime.ee_sound_probe_frontier.cop0_status << "]"
            << ",\"main_thread_priority\":" << runtime.main_thread_priority << ",\"main_thread_queries\":" << runtime.main_thread_queries << ",\"startup_worker_state\":" << jsonStartupThreads(runtime)
            << ",\"startup_worker_queries\":" << runtime.startup_worker_queries << ",\"startup_worker_starts\":" << runtime.startup_worker_starts
            << ",\"startup_ready_queries\":" << runtime.startup_ready_queries
            << ",\"startup_worker_suspends\":" << runtime.startup_worker_suspends << ",\"startup_worker_priority_changes\":" << runtime.startup_worker_priority_changes << ",\"priority_changes\":" << runtime.priority_changes
           << ",\"intc_mask\":" << runtime.intc_mask << ",\"intc_enable_calls\":" << runtime.intc_enable_calls
           << ",\"timer2_mode_writes\":" << runtime.timer2_mode_writes
           << ",\"timer2_mode_after_write\":" << runtime.timer2_mode_after_write
           << ",\"timer2_mode_reads\":" << runtime.timer2_mode_reads
           << ",\"timer3_mode_reads\":" << runtime.timer3_mode_reads << ",\"kernel_snapshot_loaded\":" << (snapshot_mode ? "true" : "false") << ",\"handler\":" << (timer_mode ? stop_handler : runtime.syscall_handlers[0x83]) << ",\"pc\":" << context.pc << ",\"arguments\":[";
    for (unsigned index = 0; index < args.size(); ++index) report << (index ? "," : "") << args[index];
    report << "],\"quadword_writes\":" << runtime.quadword_writes
           << ",\"guest_calls\":" << runtime.guest_calls << ",\"doubleword_writes\":" << runtime.doubleword_writes
           << ",\"semaphore_waits\":" << runtime.semaphore_waits << ",\"semaphore_signals\":" << runtime.semaphore_signals
           << ",\"semaphores_created\":" << runtime.semaphores.size() << ",\"distinct_ee_entered\":" << std::count_if(runtime.entered_functions.begin(),runtime.entered_functions.end(),[](auto a){return a<0x80000000;})
            << ",\"distinct_ee_completed\":" << std::count_if(runtime.completed_functions.begin(),runtime.completed_functions.end(),[](auto a){return a<0x80000000;})
            << ",\"ee_entered_addresses\":" << jsonEEAddresses(runtime.entered_functions)
            << ",\"ee_completed_addresses\":" << jsonEEAddresses(runtime.completed_functions)
            << ",\"distinct_kernel_entered\":" << std::count_if(runtime.entered_functions.begin(),runtime.entered_functions.end(),[](auto a){return a>=0x80000000;})
            << ",\"distinct_kernel_completed\":" << std::count_if(runtime.completed_functions.begin(),runtime.completed_functions.end(),[](auto a){return a>=0x80000000;})
            << ",\"returns\":" << runtime.returns
           << ",\"syscall_registrations\":" << runtime.syscall_registrations
           << ",\"word_writes\":" << runtime.word_writes
           << ",\"byte_writes\":" << runtime.byte_writes << ",\"game_booted\":false}\n";
    report.close();
    require(!report.fail(), "Entry stop report write failed");
}

void printEntrySummary(R5900Context& context,const ProbeModes& modes,const ProbeStop& stop) {
    const bool snapshot_mode=modes.snapshot,timer_mode=modes.timer,timer2_mode=modes.timer2,
        intc_mode=modes.intc,readback_mode=modes.readback,osd_mode=modes.osd,
        osd_readback_mode=modes.osd_readback,dma_status_mode=modes.dma_status,sif_rpc_mode=modes.sif_rpc,
        sif_dma_mode=modes.sif_dma,sif_mode=modes.sif,dma5_mode=modes.dma5;
    const auto stopped=stop.stopped;
    const auto stop_address=stop.address;
    std::cout << "BT2 entry CPU initialization, zeroing and bootstrap state verified.\n";
    if (sif_rpc_mode) std::cout << "Generated initialization returned; native RPC bind packet reached unsupported IOP service 0x80000592.\n";
    else if (sif_dma_mode) std::cout << "Validated CHANGE_SADDR receive effects completed; reached RPCINIT query selector 0x80000002.\n";
    else if (sif_mode) std::cout << "Captured SIF query returned; generated descriptor/cache path reached SifSetDma syscall 0x77.\n";
    else if (dma5_mode) std::cout << "DMA checkpoints replayed; channel 5 handler/enable request recorded; reached SifGetReg selector 0x80000000.\n";
    else if (dma_status_mode) std::cout << "Captured D_STAT replayed; reached channel 5 control read 0x1000C000.\n";
    else if (osd_readback_mode) std::cout << "Generated helper tail chain returned; DMA initialization reached unsupported D_STAT read 0x1000E010.\n";
    else if (osd_mode) std::cout << "Captured OSD configuration returned; reached SetOsdConfigParam syscall 0x4A.\n";
    else if (readback_mode) std::cout << "Dormant thread ID 2 created; generated setup helper returned; main priority changed to 1; reached GetOsdConfigParam syscall 0x4B.\n";
    else if (intc_mode) std::cout << "EnableIntc changed cause 11; timer startup returned; stopped at a second Timer 2 read, PC 0x00338DD8.\n";
    else if (timer2_mode) std::cout << "Timer 2 startup mode write completed; generated ErrorEPC/ERET returned; stopped at EnableIntc syscall 0x14, cause 11.\n";
    else if (timer_mode) std::cout << "Interrupt handler ID 2 stored; generated status helper returned; stopped at Timer 2 MMIO read 0x10001010 at PC 0x00338C58.\n";
    else if (snapshot_mode) std::cout << "Snapshot scans returned; FUN_00338A00 reached unsupported MMIO read 0x"
                                 << std::hex << stop_address << " at PC 0x" << context.pc << std::dec << "\n";
    else std::cout << "No kernel snapshot supplied; stopped at first kernel read 0x80000000.\n";
    std::cout << "Game boot remains incomplete.\n";
}
