#include "ee_runtime.h"

// EeRuntime owned SIF/RPC service handlers: request receipt, evidence supply and RPC/command completion.
// Kept out of the header so edits rebuild this unit, not every generated unit that includes it.

void EeRuntime::supplySifDma(std::span<const std::uint8_t> before, std::uint32_t result) {
    if (!sif_command_supplied_ || !sif_iop_.empty() || before.size() != sif_iop_size || result != 0x701)
        throw std::runtime_error("SIF DMA checkpoint size/return/order differs");
    std::uint32_t buffer, system_buffer, old_ee;
    std::memcpy(&buffer, before.data() + 0x196C0, 4);
    std::memcpy(&system_buffer, before.data() + 0x196C4, 4);
    std::memcpy(&old_ee, before.data() + 0x196C8, 4);
    if (buffer != 0x19600 || system_buffer != 0x19680 || old_ee != 0x93640)
        throw std::runtime_error("SIF IOP checkpoint table differs");
    sif_iop_.assign(before.begin(), before.end());
    sif_dma_result_ = result;
    sif_ee_receive_address = old_ee;
}

void EeRuntime::supplySifBind(std::span<const std::uint8_t> bundle) {
    if (!sif_rpc_supplied_ || !sif_bind_iop_.empty() || (bundle.size() != sif_iop_size + 68 && bundle.size() != sif_iop_size + 96))
        throw std::runtime_error("Invalid/repeated RPC bind capture bundle");
    auto word = [&](std::size_t offset) { std::uint32_t value; std::memcpy(&value, bundle.data() + offset, 4); return value; };
    const std::array<std::uint32_t, 16> reply{0, 0, 0x80000008, 0, 0, 0x203A97C0, 0,
        0x3A87A8, 0x80000009, 0x3F570, 0x3FB20, 0, 0, 0, 0, 0};
    if (word(sif_iop_size + 64) != 0x801 || word(0x3F570) != 0x80000592
            || word(0x3F578) != 0x3FB20 || word(0x3F584) != 0
            || word(0x3F5B0) != 0x3F558 || word(0x3F560) != 0x3F570
            || std::memcmp(bundle.data() + sif_iop_size, reply.data(), 64))
        throw std::runtime_error("RPC bind service/queue/response capture differs");
    if (bundle.size() == sif_iop_size + 96) {
        const std::array<std::uint32_t, 7> semaphore{0, 1, 0, 0, 0, 0x3838E8, 3};
        if (std::memcmp(bundle.data() + sif_iop_size + 68, semaphore.data(), 28))
            throw std::runtime_error("RPC call semaphore checkpoint differs");
        sif_call_sema_supplied_ = true;
    }
    sif_bind_iop_.assign(bundle.begin(), bundle.begin() + sif_iop_size);
    std::copy_n(bundle.data() + sif_iop_size, 64, sif_bind_reply_.data());
}

void EeRuntime::supplyCdInit(std::span<const std::uint8_t> bundle) {
    if (sif_bind_iop_.empty() || !sif_call_sema_supplied_ || cd_init_supplied_ || (bundle.size() != 88 && bundle.size() != 128))
        throw std::runtime_error("Invalid/repeated CD-init completion bundle");
    const std::array<std::uint32_t, 22> expected{0, 0x902,
        0x1000, 0x3695C0, 0x80000008, 0, 0, 0, 0, 0x3A87A8, 0x8000000A, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0};
    if (std::memcmp(bundle.data(), expected.data(), 88))
        throw std::runtime_error("CD-init captured mode/result/reply differs");
    if (bundle.size() == 128) {
        const std::array<std::uint32_t, 10> expected_semaphores{0x370000, 1, 0, 0, 0x370000, 0x3825E8, 3, 4, 5, 6};
        if (std::memcmp(bundle.data() + 88, expected_semaphores.data(), 40))
            throw std::runtime_error("CD semaphore setup checkpoint differs");
        cd_semaphores_supplied_ = true;
    }
    std::copy_n(bundle.begin(), 88, cd_init_bundle_.begin());
    cd_init_supplied_ = true;
}

void EeRuntime::supplyScmdBind(std::span<const std::uint8_t> bundle) {
    const std::array<std::uint32_t, 22> expected{0x370000,1,0,0,2,0x3838D8,
        0,0,0x80000008,0,0,0x203A97C0,0,0x369F40,0x80000009,0x3F690,0x3F6D8,0,0,0,0,0};
    if (!cd_semaphores_supplied_ || scmd_bind_supplied_ || (bundle.size() != 88 && bundle.size() != 184 && bundle.size() != 188 && bundle.size() != 192 && bundle.size() != 204 && bundle.size() != 224 && bundle.size() != 228 && bundle.size() != 236 && bundle.size() != 244 && bundle.size() != 252 && bundle.size() != 316 && bundle.size() != 324 && bundle.size() != 616 && bundle.size() != 988 && bundle.size() != 1124 && bundle.size() != 1332 && bundle.size() != 1464 && bundle.size() != 1596)
            || std::memcmp(bundle.data(), expected.data(), 88))
        throw std::runtime_error("Invalid SCMD bind checkpoint");
    if (bundle.size() >= 184) {
        const std::array<std::uint32_t,24> mmode{0,1,0,0,0x370000,0x3838E8,
            0x400,0x3695C0,0x80000008,0,0,0,0,0x369F40,0x8000000A,0,0,0,0,0,0,0,2,0};
        if(std::memcmp(bundle.data()+88,mmode.data(),96)) throw std::runtime_error("Invalid media-mode checkpoint");
        std::copy_n(bundle.data()+112,64,scmd_mmode_reply_.data());scmd_mmode_supplied_=true;
    }
    if (bundle.size() >= 188) {
        std::uint32_t stop_word; std::memcpy(&stop_word,bundle.data()+184,4);
        if (stop_word) throw std::runtime_error("SIF stop checkpoint differs");
        sif_stop_supplied_=true;
    }
    if (bundle.size() >= 192) {
        std::uint32_t subaddr; std::memcpy(&subaddr,bundle.data()+188,4);
        if (subaddr!=0x19600) throw std::runtime_error("Reset SUBADDR checkpoint differs");
        sif_reset_subaddr_supplied_=true;
    }
    if (bundle.size() >= 204) {
        const std::array<std::uint32_t,3> flags{0x30000,0x30000,0x30000};
        if (std::memcmp(bundle.data()+192,flags.data(),12)) throw std::runtime_error("Reset SMFLAG checkpoint differs");
        sif_smflag=0x30000; reset_smflag_supplied_=true;
    }
    if(bundle.size()>=224) {
        const std::array<std::uint32_t,5> completion{0xE01,0x20000,0,0,0};
        if(std::memcmp(bundle.data()+204,completion.data(),20))throw std::runtime_error("Invalid reboot submission/clear checkpoints");
        reset_submission_supplied_=true;
    }
    if(bundle.size()>=228) {
        std::uint32_t bit;std::memcpy(&bit,bundle.data()+224,4);
        if(bit!=0x40000)throw std::runtime_error("Invalid ready-bit checkpoint");
        reset_ready_supplied_=true;
    }
    if(bundle.size()>=236) {
        std::uint64_t csr;std::memcpy(&csr,bundle.data()+228,8);
        if(csr!=0x551B4000)throw std::runtime_error("Invalid GS CSR checkpoint");
    }
    if(bundle.size()>=244) {
        std::uint64_t result;std::memcpy(&result,bundle.data()+236,8);
        if(result!=0xFF00)throw std::runtime_error("Invalid GS IMR checkpoint");
    }
    if(bundle.size()>=252) {
        std::uint64_t result;std::memcpy(&result,bundle.data()+244,8);
        if(result)throw std::runtime_error("Invalid GS CRT checkpoint");
    }
    if(bundle.size()>=316) {
        const std::array<std::uint32_t,16> evidence{12,0x1802,0x1FFEAD0,1,8,0x1802,0x1FFEAD0,1,
            12,0x1802,0x1FFEAD0,4,8,0x1802,0x1FFEAD0,1};
        if(std::memcmp(bundle.data()+252,evidence.data(),64))throw std::runtime_error("INTC evidence differs");
        gs_interrupt_supplied_=true;
    }
    if(bundle.size()>=324) {
        std::uint64_t field;std::memcpy(&field,bundle.data()+316,8);
        if(field!=0x551B6004)throw std::runtime_error("GS field checkpoint differs");
        gs_field_supplied_=true;
    }
    if(bundle.size()>=616) {
        const std::array<std::uint32_t,9> idle{0,0,0x1FFEAF0,0,0,0,1,0x800040,0x1201};
        const std::array<std::uint64_t,2> results{15,0};
        if(std::memcmp(bundle.data()+324,idle.data(),36) || std::memcmp(bundle.data()+360,results.data(),16))
            throw std::runtime_error("GIF idle/staging/reference return evidence differs");
        gif_dma.supply(bundle.subspan(376,240));gif_dma_supplied_=true;
        if(bundle.size()>=988)gif_dma.supplySync(bundle.subspan(616,372));
        if(bundle.size()>=1124)post_sif.supply(bundle.subspan(988,136));
        if(bundle.size()>=1332)post_cd.supply(bundle.subspan(1124,bundle.size()>=1464?340:208));
        if(bundle.size()>=1596)post_loadfile.supply(bundle.subspan(1464));
    }
    std::copy_n(bundle.data()+24,64,scmd_reply_.data()); scmd_bind_supplied_=true;
}

void EeRuntime::receiveFirstRpcBind(std::uint8_t* rdram, R5900Context* context) {
    if (sif_rpc_reads != 1 || sif_bind_callbacks || data_cache_lines != 2
            || context->r[4].low != initial_sp - 464 || context->r[5].low != 1
            || context->r[29].low != context->r[4].low || rpc_end_callback_active_)
        throw UnsupportedSyscall(0x77, context->pc);
    const auto descriptor = static_cast<std::uint32_t>(context->r[4].low);
    const std::array<std::uint32_t, 4> expected_descriptor{0x203A97C0, 0x19600, 64, 0x44};
    const std::array<std::uint32_t, 16> request{64, 0, 0x80000009, 0, 5, 0x203A97C0, 2,
        0x3A87A8, 0x80000592, 0, 0, 0, 0, 0, 0, 0};
    for (unsigned index = 0; index < expected_descriptor.size(); ++index)
        if (read32(rdram, descriptor + index * 4) != expected_descriptor[index])
            throw UnsupportedSyscall(0x77, context->pc);
    for (unsigned index = 0; index < request.size(); ++index)
        if (read32(rdram, 0x3A97C0 + index * 4) != request[index])
            throw UnsupportedSyscall(0x77, context->pc);
    if (sif_ee_receive_address != 0x3A94C0 || read32(rdram, 0x3A87A8) != 0x203A97C0
            || read32(rdram, 0x3A87AC) != 2 || read32(rdram, 0x3A87B0) != 3
            || semaphores.size() != 4 || semaphores[3].count != 0 || semaphores[3].maximum != 1
            || read32(rdram, 0x3A9620) != 0x3322E8 || read32(rdram, 0x3A9624) != 0x3AAFC0
            || read32(rdram, 0x3A9628) != 0x38BDF0 || !functions_.contains(0x3322E8)
            || !functions_.contains(0x332258) || !functions_.contains(0x32EC90))
        throw UnsupportedSyscall(0x77, context->pc);
    // One checked logical service lookup/response. No IOP instruction execution,
    // hardware DMA padding, interrupt entry, cycles or general RPC scheduler.
    std::copy_n(memory_.data() + 0x3A97C0, 64, sif_iop_.data() + 0x19600);
    sif_iop_[0x19600] = 0;
    std::copy(sif_bind_reply_.begin(), sif_bind_reply_.end(), memory_.begin() + 0x3A94C0);
    deliverRpcEnd(rdram, context);
    ++sif_bind_context_preservations;
    ++sif_bind_callbacks;
    ++sif_dma_submissions;
    context->r[2].low = 0x801;
}

void EeRuntime::deliverEeCommand(std::uint8_t* rdram,R5900Context* context,std::uint32_t handler,std::uint32_t argument) {
    std::array<std::uint8_t,sizeof(R5900Context)> saved_cpu;std::memcpy(saved_cpu.data(),context,saved_cpu.size());
    auto callback=*context;callback.r[4].low=0x3A94C0;callback.r[5].low=argument;callback.r[28].low=0x38BDF0;
    constexpr std::uint32_t callback_return=0xFFFFFFF8;callback.r[31].low=callback_return;
    const auto scratch=static_cast<std::uint32_t>(callback.r[29].low)-128;checkMemory(rdram,scratch,128);
    std::array<std::uint8_t,128> saved_stack;std::copy_n(memory_.data()+scratch,128,saved_stack.data());
    rpc_end_callback_active_=true;
    try {dispatchGuestBranch(rdram,&callback,handler,context->pc,callback_return,GuestBranchKind::DirectCall,"FILEIO SIF command");}
    catch(...) {rpc_end_callback_active_=false;std::copy(saved_stack.begin(),saved_stack.end(),memory_.begin()+scratch);throw;}
    rpc_end_callback_active_=false;std::copy(saved_stack.begin(),saved_stack.end(),memory_.begin()+scratch);
    if(callback.r[29].low!=context->r[29].low || std::memcmp(saved_cpu.data(),context,saved_cpu.size()))
        throw std::runtime_error("FILEIO SIF command handler changed interrupted CPU state");
}

void EeRuntime::deliverRpcEnd(std::uint8_t* rdram, R5900Context* context,
                       std::uint32_t client, unsigned semaphore,
                       std::uint32_t server, std::uint32_t buffer) {
    std::array<std::uint8_t, sizeof(R5900Context)> saved_cpu;
    std::memcpy(saved_cpu.data(), context, saved_cpu.size());
    auto callback = *context;
    callback.r[4].low = 0x3A94C0;
    callback.r[5].low = 0x3AAFC0;
    callback.r[28].low = 0x38BDF0;
    constexpr std::uint32_t callback_return = 0xFFFFFFF8;
    callback.r[31].low = callback_return;
    // Verified handler uses a 64-byte frame below the caller SP. Preserve
    // that scratch window and execute on a CPU copy, retaining every register.
    const auto scratch = static_cast<std::uint32_t>(callback.r[29].low) - 64;
    checkMemory(rdram, scratch, 64);
    std::array<std::uint8_t, 64> saved_stack;
    std::copy_n(memory_.data() + scratch, 64, saved_stack.data());
    rpc_end_callback_active_ = true;
    try {
        dispatchGuestBranch(rdram, &callback, 0x3322E8, context->pc, callback_return,
            GuestBranchKind::DirectCall, "checked logical RPC_END");
    } catch (...) {
        rpc_end_callback_active_ = false;
        std::copy(saved_stack.begin(), saved_stack.end(), memory_.begin() + scratch);
        throw;
    }
    rpc_end_callback_active_ = false;
    std::copy(saved_stack.begin(), saved_stack.end(), memory_.begin() + scratch);
    if (callback.r[29].low != context->r[29].low || (semaphore ? semaphores[semaphore].count != 1 :
                read32(rdram,client+8) != UINT32_MAX)
            || read32(rdram, client) != 0 || read32(rdram, client + 36) != server
            || read32(rdram, client + 20) != buffer || read32(rdram, 0x3A97D0) != 4
            || read32(rdram, 0x3A97D8) != 0)
        throw std::runtime_error("Generated RPC_END effects differ from capture");
    if (std::memcmp(saved_cpu.data(), context, saved_cpu.size()))
        throw std::runtime_error("RPC_END changed interrupted CPU state");
}

void EeRuntime::receiveFreshLoadfile(std::uint8_t* rdram,R5900Context* context) {
    const bool bind=!post_loadfile.binds;const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(context->r[29].low!=d || d!=initial_sp-(bind?528:608) || context->r[5].low!=1
        || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1
        || semaphores[7].option!=(bind?0x3838D8u:0x3838E8u) || prior_iop_services_valid)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,4> desc={0x203A97C0,0x1E640,64,0x44};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,9> request={64,0,bind?0x80000009u:0x8000000Au,0,5,0x203A97C0,bind?7u:8u,0x3ACC00,bind?0x80000006u:255u};
    for(unsigned n=0;n<9;++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3ACC00)!=0x203A97C0 || read32(rdram,0x3ACC04)!=(bind?7u:8u) || read32(rdram,0x3ACC08)!=7)throw UnsupportedSyscall(0x77,context->pc);
    if(!bind) {
        const std::array<std::uint32_t,5> args={0,0x3ACA00,4,1,0x22568};
        for(unsigned n=0;n<5;++n)if(read32(rdram,0x3A97C0+36+n*4)!=args[n])throw UnsupportedSyscall(0x77,context->pc);
        std::copy_n(post_loadfile.replies.data()+128,4,memory_.data()+0x3ACA00);
    }
    std::copy_n(post_loadfile.replies.data()+(bind?0:64),64,memory_.data()+0x3A94C0);
    deliverRpcEnd(rdram,context,0x3ACC00,7,0x22568,0x225B0);
    if(bind)++post_loadfile.binds;else ++post_loadfile.versions;
    ++sif_dma_submissions;context->r[2].low=0x30000+post_loadfile.operations();
}

void EeRuntime::receiveFreshScmd(std::uint8_t* rdram,R5900Context* context) {
    const bool bind=!post_cd.scmd_binds;const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(context->r[29].low!=d || d!=initial_sp-(bind?432:448) || context->r[5].low!=(bind?1u:2u)
        || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1
        || semaphores[7].option!=(bind?0x3838D8u:0x3838E8u) || iop_epoch!=1 || prior_iop_services_valid)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,4> desc={0x203A97C0,0x1E640,64,0x44};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+(bind?0:16)+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,9> request={bind?64u:0x440u,bind?0u:0x439A8u,bind?0x80000009u:0x8000000Au,0,5,0x203A97C0,bind?5u:6u,0x369F40,bind?0x80000593u:34u};
    for(unsigned n=0;n<9;++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x369F40)!=0x203A97C0 || read32(rdram,0x369F44)!=(bind?5u:6u) || read32(rdram,0x369F48)!=7)
        throw UnsupportedSyscall(0x77,context->pc);
    if(!bind) {
        const std::array<std::uint32_t,4> payload={0x369A00,0x439A8,4,0};
        const std::array<std::uint32_t,5> args={4,0x3695C0,4,1,0x434F0};
        for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=payload[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<5;++n)if(read32(rdram,0x3A97C0+36+n*4)!=args[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x369A00)!=2)throw UnsupportedSyscall(0x77,context->pc);
        write32(rdram,0x3695C0,1);
    }
    std::copy_n(post_cd.replies.data()+(bind?208:272),64,memory_.data()+0x3A94C0);
    deliverRpcEnd(rdram,context,0x369F40,7,0x434F0,0x439A8);
    if(bind)++post_cd.scmd_binds;else ++post_cd.modes;
    ++sif_dma_submissions;context->r[2].low=0x20000+post_cd.operations();
}

void EeRuntime::receiveFreshCd(std::uint8_t* rdram,R5900Context* context) {
    const bool bind=post_cd.binds<2;
    auto d=static_cast<std::uint32_t>(context->r[4].low);checkMemory(rdram,d,bind?16:32);
    if(context->r[29].low!=d || d!=initial_sp-(bind?464:544) || static_cast<std::uint32_t>(context->r[5].low)!=(bind?1u:2u)
        || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1
        || semaphores[7].option!=(bind?0x3838D8u:0x3838E8u) || prior_iop_services_valid)
        throw UnsupportedSyscall(0x77,context->pc);
    const unsigned packet_offset=bind?0u:16u;
    const std::array<std::uint32_t,4> desc={0x203A97C0,0x1E640,64,0x44};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+packet_offset+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    const unsigned rpc_id=bind?2+post_cd.binds:4;
    const std::array<std::uint32_t,9> request={bind?64u:0x440u,bind?0u:0x441B8u,bind?0x80000009u:0x8000000Au,0,5,0x203A97C0,rpc_id,0x3A87A8,bind?0x80000592u:0u};
    for(unsigned n=0;n<request.size();++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3A87A8)!=0x203A97C0 || read32(rdram,0x3A87AC)!=rpc_id || read32(rdram,0x3A87B0)!=7)
        throw UnsupportedSyscall(0x77,context->pc);
    if(!bind) {
        const std::array<std::uint32_t,4> payload={0x3A8800,0x441B8,4,0};
        const std::array<std::uint32_t,5> args={4,0x3695C0,16,1,0x43388};
        for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=payload[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<5;++n)if(read32(rdram,0x3A97C0+36+n*4)!=args[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x3A8800)!=0)throw UnsupportedSyscall(0x77,context->pc);
        std::copy_n(post_cd.replies.data()+192,16,memory_.data()+0x3695C0);
    }
    const unsigned reply_offset=bind?post_cd.binds*64:128;
    std::copy_n(post_cd.replies.data()+reply_offset,64,memory_.data()+0x3A94C0);
    const bool bound=!bind || post_cd.binds==1;
    deliverRpcEnd(rdram,context,0x3A87A8,7,bound?0x43388:0,bound?0x441B8:0);
    if(bind)++post_cd.binds;else ++post_cd.calls;
    ++sif_dma_submissions;
    context->r[2].low=0x20000+post_cd.binds+post_cd.calls; // authored logical token
}

void EeRuntime::receiveFirstCdInit(std::uint8_t* rdram, R5900Context* context) {
    if (cd_init_calls || sif_bind_callbacks != 1 || sif_dma_submissions != 2
            || context->r[4].low != initial_sp - 544 || context->r[29].low != context->r[4].low
            || context->r[5].low != 2 || data_cache_lines != 6 || semaphores.size() != 4
            || semaphores[3].count != 0 || semaphores[3].maximum != 1
            || semaphores[3].option != 0x3838E8 || read32(rdram, 0x3A8800) != 0
            || read32(rdram, 0x3A87A8) != 0x203A97C0 || read32(rdram, 0x3A87AC) != 3
            || read32(rdram, 0x3A87B0) != 3 || read32(rdram, 0x3A87C4) != 0
            || read32(rdram, 0x3A87C8) != 0)
        throw UnsupportedSyscall(0x77, context->pc);
    const std::array<std::uint32_t, 8> descriptors{0x3A8800, 0x3FB20, 4, 0,
        0x203A97C0, 0x19600, 64, 0x44};
    const std::array<std::uint32_t, 16> request{0x440, 0x3FB20, 0x8000000A, 0,
        5, 0x203A97C0, 3, 0x3A87A8, 0, 4, 0x3695C0, 16, 1, 0x3F570, 0, 0};
    for (unsigned index = 0; index < descriptors.size(); ++index)
        if (read32(rdram, static_cast<std::uint32_t>(context->r[4].low) + index * 4) != descriptors[index])
            throw UnsupportedSyscall(0x77, context->pc);
    for (unsigned index = 0; index < request.size(); ++index)
        if (read32(rdram, 0x3A97C0 + index * 4) != request[index])
            throw UnsupportedSyscall(0x77, context->pc);
    // One explicit standalone-ELF mode-0 completion checkpoint. Does not
    // execute retail IOP code, access a disc or provide general CD status.
    std::copy_n(memory_.data() + 0x3A8800, 4, sif_iop_.data() + 0x3FB20);
    std::copy_n(memory_.data() + 0x3A97C0, 64, sif_iop_.data() + 0x19600);
    sif_iop_[0x19600] = 0;
    std::copy_n(cd_init_bundle_.data() + 8, 64, memory_.data() + 0x3A94C0);
    std::copy_n(cd_init_bundle_.data() + 72, 16, memory_.data() + 0x3695C0);
    deliverRpcEnd(rdram, context);
    ++cd_init_calls;
    ++cd_init_context_preservations;
    ++sif_dma_submissions;
    context->r[2].low = 0x902;
}

void EeRuntime::receiveScmdBind(std::uint8_t* rdram, R5900Context* context) {
    if (scmd_bind_calls || cd_init_calls != 1 || cd_semaphores_created != 4
            || context->r[4].low != initial_sp - 432 || context->r[29].low != context->r[4].low
            || context->r[5].low != 1 || semaphores.size() != 8 || semaphores[7].count != 0
            || semaphores[7].maximum != 1 || semaphores[7].option != 0x3838D8
            || read32(rdram,0x369F40) != 0x203A97C0 || read32(rdram,0x369F44) != 4
            || read32(rdram,0x369F48) != 7)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,4> descriptor{0x203A97C0,0x19600,64,0x44};
    const std::array<std::uint32_t,16> request{64,0,0x80000009,0,5,0x203A97C0,4,0x369F40,
        0x80000593,4,0x3695C0,16,1,0x3F570,0,0};
    for (unsigned i=0;i<4;++i) if(read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+i*4)!=descriptor[i])
        throw UnsupportedSyscall(0x77,context->pc);
    for (unsigned i=0;i<16;++i) if(read32(rdram,0x3A97C0+i*4)!=request[i])
        throw UnsupportedSyscall(0x77,context->pc);
    std::copy_n(memory_.data()+0x3A97C0,64,sif_iop_.data()+0x19600); sif_iop_[0x19600]=0;
    std::copy(scmd_reply_.begin(),scmd_reply_.end(),memory_.begin()+0x3A94C0);
    deliverRpcEnd(rdram,context,0x369F40,7,0x3F690,0x3F6D8);
    ++scmd_bind_calls; ++sif_dma_submissions;
    // Authored positive logical submission handle; not a captured hardware DMA ID.
    // The synchronous bind consumes only the sign before waiting on its semaphore.
    context->r[2].low=1;
}

void EeRuntime::receiveScmdMmode(std::uint8_t* rdram,R5900Context* context) {
    if(scmd_mmode_calls || scmd_bind_calls!=1 || semaphore_deletions!=3 || semaphores.size()!=8
        || semaphores[7].count!=0 || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838E8
        || context->r[4].low!=initial_sp-448 || context->r[29].low!=context->r[4].low || context->r[5].low!=2
        || read32(rdram,0x369A00)!=2 || read32(rdram,0x369F40)!=0x203A97C0
        || read32(rdram,0x369F44)!=5 || read32(rdram,0x369F48)!=7
        || read32(rdram,0x369F5C)!=0 || read32(rdram,0x369F60)!=0)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,8> descriptors{0x369A00,0x3F6D8,4,0,0x203A97C0,0x19600,64,0x44};
    const std::array<std::uint32_t,16> request{0x440,0x3F6D8,0x8000000A,0,5,0x203A97C0,5,0x369F40,34,4,0x3695C0,4,1,0x3F690,0,0};
    for(unsigned i=0;i<8;++i)if(read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+i*4)!=descriptors[i])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned i=0;i<16;++i)if(read32(rdram,0x3A97C0+i*4)!=request[i])throw UnsupportedSyscall(0x77,context->pc);
    std::copy_n(memory_.data()+0x369A00,4,sif_iop_.data()+0x3F6D8);
    std::copy_n(memory_.data()+0x3A97C0,64,sif_iop_.data()+0x19600);sif_iop_[0x19600]=0;
    std::copy(scmd_mmode_reply_.begin(),scmd_mmode_reply_.end(),memory_.begin()+0x3A94C0);
    std::fill_n(memory_.data()+0x3695C0,4,std::uint8_t{0});
    deliverRpcEnd(rdram,context,0x369F40,7,0x3F690,0x3F6D8);
    ++scmd_mmode_calls;++sif_dma_submissions;context->r[2].low=1;
}
