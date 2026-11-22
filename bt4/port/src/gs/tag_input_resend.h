/* A guest's TAG TEAM inputs, sent again (tag_team_p2p.cpp). A guest's TTW_INPUT goes once, on the room's unreliable,
 * unordered input channel; one lost packet was a lost frame of input: the owner's kernel filled it with the last pad
 * held (or neutral after three frames) and dropped one-frame commands (shared/tag_kernel.c Kernel_DeadlineFrame).
 * Each input packet now goes with the last kResendInputs before it, unchanged: the kernel takes a frame's first copy,
 * answers the same copy again with TT_OK and an old frame with TT_STALE (port/src/tag_team.c Tt_InputPut), so an owner
 * of any version takes them as they are. A packet is 528 bytes on the wire: the guest's upload goes from one to three
 * a frame. */
#ifndef BT4_TAG_INPUT_RESEND_H
#define BT4_TAG_INPUT_RESEND_H
#include <array>
#include <cstddef>
#include <cstring>

template <std::size_t Bytes, std::size_t Before = 2>
class TagInputResend {
public:
    using Packet = std::array<unsigned char, Bytes>;
    static constexpr std::size_t kResendInputs = Before;
    /* `packet` (a TTW_INPUT of round `epoch`) goes now: `send` gets it, then the earlier ones of the same round, newest first. */
    template <class Send> void send(const Packet &packet, unsigned epoch, Send &&sendOne) {
        if (count_ && epoch != epoch_) count_ = 0; /* (a new round: the last one's inputs are of no use) */
        epoch_ = epoch;
        sendOne(packet);
        if constexpr (Before > 0) {
            for (std::size_t i = 0; i < count_; ++i) sendOne(recent_[(head_ + Before - 1 - i) % Before]);
            recent_[head_] = packet;
            head_ = (head_ + 1) % Before;
            if (count_ < Before) ++count_;
        }
    }
    void clear() { count_ = 0; }

private:
    std::array<Packet, Before ? Before : 1> recent_{};
    std::size_t head_ = 0, count_ = 0;
    unsigned epoch_ = 0;
};
#endif
