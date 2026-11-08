#pragma once
#include <algorithm>
#include <cstdint>
#include <map>

// Native IOP heap: first-fit with 256-byte granularity from 0xCE500, the first address the original
// boot heap handed out. This reproduces every captured placement (0xCE500, 0x14FD00, 0x150600,
// 0x191700, 0x192000, 0x196100, 0x196400), including the reuse after frees. Addresses are opaque
// handles; the bytes behind them live in the native IOP memory store.
class IopHeap {
public:
    static constexpr std::uint32_t base=0xCE500,end=0x1F0000,granule=0x100;
    // Returns the block address, or 0 when no block fits.
    std::uint32_t allocate(std::uint32_t size) {
        if(!size)return 0;
        const auto rounded=(size+granule-1)&~(granule-1);
        for(std::uint32_t at=base;at+rounded<=end;) {
            const auto clash=std::find_if(blocks_.begin(),blocks_.end(),[&](const auto& b){return b.first<at+rounded && at<b.first+b.second;});
            if(clash==blocks_.end()){blocks_[at]=rounded;return at;}
            at=clash->first+clash->second;
        }
        return 0;
    }
    bool release(std::uint32_t address) {return blocks_.erase(address)>0;}
    bool isBlock(std::uint32_t address) const {return blocks_.contains(address);}
    // Rounded size of the block at `address`, or 0.
    std::uint32_t size(std::uint32_t address) const {const auto found=blocks_.find(address);return found==blocks_.end()?0:found->second;}
    // True when [address, address+size) lies inside one allocated block.
    bool covers(std::uint32_t address,std::uint32_t size) const {
        const auto block=blocks_.upper_bound(address);
        if(block==blocks_.begin())return false;
        const auto& [start,length]=*std::prev(block);
        return address+size<=start+length;
    }
private:
    std::map<std::uint32_t,std::uint32_t> blocks_; // Address -> rounded size.
};
