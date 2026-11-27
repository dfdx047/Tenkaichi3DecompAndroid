#include "ee/native_functions.h"
#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <cstring>

namespace {
// The C library's memcpy in the main executable.
constexpr std::uint32_t memcpy_address=0x32512C,memcpy_size=0xB0;
constexpr std::uint64_t memcpy_hash=0xFCB403FF6C9A6111;
PS2Runtime::RecompiledFunction generated_memcpy=nullptr;

std::uint64_t s32(std::uint32_t value) {return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(value)));}

// memcpy(a0 destination, a1 source, a2 count), returning the destination. With both addresses 16-byte
// aligned and 32 bytes or more it copies rounds of two quadwords while 32 remain, then doublewords while 8
// remain; then (and otherwise for everything) bytes. Each chunk is read before it is written, as the
// generated loops do, so an overlapping copy gives the same bytes. Each loop's back-edge is one checkpoint.
// The registers end as the loops leave them: v0 and t0 the destination, a0 and a2 -1, a1 and v1 past the
// ends, a3 past the doublewords on the aligned path; lq loads all 128 bits, so v1 and v0 keep the upper
// halves of the last round's two quadwords.
void nativeMemcpy(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    auto& rt=*static_cast<EeRuntime*>(base);
    const auto destination=GPR_U32(ctx,4),source=GPR_U32(ctx,5),count=GPR_U32(ctx,6);
    const bool aligned=count>=32 && !((destination|source)&15);
    const std::uint32_t rounds=aligned?count/32:0,doublewords=aligned?count%32/8:0,bytes=aligned?count%8:count;
    const std::uint64_t checkpoints=(rounds?rounds-1:0)+(doublewords?doublewords-1:0)+(bytes?bytes-1:0);
    // The fast path: plain main RAM addresses, no watched quadword, and the checkpoints without effect.
    const bool in_ram=rdram==rt.memory().data() && std::uint64_t(destination)+count<=EeRuntime::ram_size
        && std::uint64_t(source)+count<=EeRuntime::ram_size;
    if(ctx->pc!=memcpy_address || !in_ram || rt.on_watched_quadword || !rt.takeCheckpoints(checkpoints)) {
        generated_memcpy(rdram,ctx,base);return;
    }
    const bool overlapping=destination<source+count && source<destination+count;
    if(!overlapping)std::memcpy(rdram+destination,rdram+source,count);
    else {
        std::uint32_t at=0;
        const auto chunk=[&](unsigned size){std::uint8_t data[16];std::memcpy(data,rdram+source+at,size);std::memcpy(rdram+destination+at,data,size);at+=size;};
        for(std::uint32_t n=0;n<rounds;++n){chunk(16);chunk(16);}
        for(std::uint32_t n=0;n<doublewords;++n)chunk(8);
        for(std::uint32_t n=0;n<bytes;++n)chunk(1);
    }
    rt.quadword_writes+=2ull*rounds;rt.doubleword_writes+=doublewords;rt.byte_writes+=bytes;
    const auto original_destination=ctx->r[4].low,original_a1=ctx->r[5].low;
    if(aligned) {
        // The last round's quadwords as copied (the destination holds them now).
        const auto last=destination+(rounds-1)*32;
        std::uint64_t high1,high0;std::memcpy(&high1,rdram+last+8,8);std::memcpy(&high0,rdram+last+24,8);
        ctx->r[3].high=high1;ctx->r[2].high=high0;
        ctx->r[7].low=s32(destination+rounds*32+doublewords*8);
    }
    ctx->r[3].low=aligned || bytes?s32(destination+count):original_destination;
    ctx->r[5].low=count?s32(source+count):original_a1;
    ctx->r[4].low=~0ull;ctx->r[6].low=~0ull;
    ctx->r[2].low=original_destination;ctx->r[8].low=original_destination;
    ctx->pc=GPR_U32(ctx,31);
}
}

std::uint64_t nativeCodeHash(std::span<const std::uint8_t> code) {
    std::uint64_t hash=0xCBF29CE484222325ull;
    for(const auto byte:code)hash=(hash^byte)*0x100000001B3ull;
    return hash;
}

std::uint32_t installNativeMemcpyForTest(EeRuntime& runtime) {
    generated_memcpy=runtime.replaceFunction(memcpy_address,nativeMemcpy);
    return memcpy_address;
}

unsigned registerNativeFunctions(EeRuntime& runtime) {
    const auto ram=runtime.memory();
    if(ram.size()<memcpy_address+memcpy_size || nativeCodeHash(ram.subspan(memcpy_address,memcpy_size))!=memcpy_hash)return 0;
    generated_memcpy=runtime.replaceFunction(memcpy_address,nativeMemcpy);
    return 1;
}
