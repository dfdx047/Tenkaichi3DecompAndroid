#include "ee/ee_runtime.h"
#include <algorithm>

// Native IOP memory: the IOP heap service (0x80000003), raw EE->IOP SIF DMA uploads and their
// sceSifDmaStat polls (heap placement: iop_heap.h). Uploaded bytes are kept in a plain store (no IOP code runs) so native
// services can read them; the IOP addresses the EE receives are opaque handles.
//
namespace {
constexpr std::uint32_t heap_alloc=1,heap_free=2;
// The EE executable's embedded IOP module image, copied to the IOP before LOADFILE loads it from there.
constexpr std::uint32_t embedded_module_image=0x369F90;
}

bool EeRuntime::nativeHeapCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(r.send_size!=4 || r.recv_size!=4)throw UnsupportedSyscall(0x77,context->pc);
    const auto argument=read32(rdram,r.send_source);
    if(r.rpc_number==heap_alloc) {
        const auto pointer=native_heap_.allocate(argument);
        if(!pointer)throw UnsupportedSyscall(0x77,context->pc);
        write32(rdram,r.receive,pointer);completeNativeCall(rdram,context,r,server);
        ++heap_calls;++sif_dma_submissions;context->r[2].low=0x40000+heap_binds+heap_calls;return true;
    }
    if(r.rpc_number==heap_free) {
        if(!native_heap_.release(argument))throw UnsupportedSyscall(0x77,context->pc);
        write32(rdram,r.receive,0);completeNativeCall(rdram,context,r,server);
        ++heap_frees;++sif_dma_submissions;context->r[2].low=0x40000+heap_binds+heap_calls+heap_frees;return true;
    }
    throw UnsupportedSyscall(0x77,context->pc);
}

bool EeRuntime::handleNativeRawDma(std::uint8_t* rdram,R5900Context* context) {
    // One descriptor {EE source, IOP destination, size, attribute 0}: data, not a command packet.
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(context->r[5].low!=1 || read32(rdram,d+12))return false;
    const auto source=read32(rdram,d),destination=read32(rdram,d+4),size=read32(rdram,d+8);
    if(!size || size%16 || destination+size>native_iop_ram_.size())throw UnsupportedSyscall(0x77,context->pc);
    checkMemory(rdram,source,size);
    std::copy_n(memory_.begin()+source,size,native_iop_ram_.begin()+destination);
    if(nativeCriStreamUpload(rdram,context,source,destination,size))return true;
    // Uploads go into blocks the heap handed out.
    if(!native_heap_.covers(destination,size))throw UnsupportedSyscall(0x77,context->pc);
    if(source==embedded_module_image) {
        ++heap_transfers;++sif_dma_submissions;context->r[2].low=0x50000+heap_transfers;
        native_dma_tokens_[static_cast<std::uint32_t>(context->r[2].low)]=&EeRuntime::heap_dma_polls;
    } else {
        ++iop_uploads;++sif_dma_submissions;context->r[2].low=0x5A0000+iop_uploads;
        native_dma_tokens_[static_cast<std::uint32_t>(context->r[2].low)]=&EeRuntime::iop_upload_polls;
    }
    return true;
}

bool EeRuntime::handleNativeDmaStat(R5900Context* context) {
    // sceSifDmaStat on a native transfer: copies complete at submission, so the status is done (-1).
    const auto token=native_dma_tokens_.find(static_cast<std::uint32_t>(context->r[4].low));
    if(token==native_dma_tokens_.end())return false;
    ++(this->*token->second);context->r[2].low=UINT64_MAX;return true;
}
