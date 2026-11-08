#include "lockstep.h"
#include <algorithm>
#include <cstring>

namespace {
void put32(std::uint8_t* p, std::uint32_t v) {for (int i = 0; i < 4; ++i) p[i] = std::uint8_t(v >> (8 * i));}
void put64(std::uint8_t* p, std::uint64_t v) {for (int i = 0; i < 8; ++i) p[i] = std::uint8_t(v >> (8 * i));}
std::uint32_t get32(const std::uint8_t* p) {return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;}
std::uint64_t get64(const std::uint8_t* p) {return std::uint64_t(get32(p)) | std::uint64_t(get32(p + 4)) << 32;}
}

void Lockstep::begin(int role, unsigned delay) {
    *this = Lockstep{};
    active_ = true;
    role_ = role ? 1 : 0;
    delay_ = std::min(delay, 16u);
    pushedEnd_ = delay_;       // fields before `delay_` need no sample: neutral for both
    remoteNext_ = delay_;
    ackFromPeer_ = delay_;
}

void Lockstep::pushLocal(std::uint32_t field, const Sample& sample) {
    if (!active_) return;
    const std::uint32_t eff = field + delay_;
    if (eff < pushedEnd_) return;                       // (the game waited and asks again for the same field)
    if (eff >= localFloor_ + ring) return;              // (far beyond what is kept: cannot happen while the game waits for `ready`)
    for (std::uint32_t f = pushedEnd_; f < eff; ++f) {L(f).s = neutralSample(); L(f).have = true;}   // (fields the game skipped asking for: neutral)
    L(eff).s = sample;
    L(eff).have = true;
    pushedEnd_ = eff + 1;
}

bool Lockstep::ready(std::uint32_t field) const {
    if (!active_ || field < delay_ || field < floor_) return true;
    return R(field).have;
}

Lockstep::Pair Lockstep::pair(std::uint32_t field) const {
    Sample mine = neutralSample(), theirs = neutralSample();
    if (active_ && field >= delay_) {
        const Slot& l = local_[field % ring];
        const Slot& r = remote_[field % ring];
        if (l.have) mine = l.s;
        if (r.have) theirs = r.s;
    }
    return role_ == 0 ? Pair{mine, theirs} : Pair{theirs, mine};
}

void Lockstep::done(std::uint32_t field) {
    if (!active_ || field <= floor_) return;
    const std::uint32_t to = std::min<std::uint32_t>(field, floor_ + std::uint32_t(ring));
    for (std::uint32_t f = floor_; f < to; ++f) remote_[f % ring] = Slot{};
    floor_ = field;
    trimLocal();
}

// This side's own samples are kept until the other side has them, even when the game is long past: it may be waiting for exactly those.
void Lockstep::trimLocal() {
    const std::uint32_t to = std::min(floor_, ackFromPeer_);
    while (localFloor_ < to) {local_[localFloor_ % ring] = Slot{}; ++localFloor_;}
}

// A sum over the whole packet but its own four bytes: a packet that was changed on the way is refused, not believed.
std::uint32_t Lockstep::check(const std::uint8_t* p, std::size_t size) {
    std::uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < size; ++i) if (i < 28 || i >= head) h = (h ^ p[i]) * 16777619u;
    return h;
}

std::uint32_t Lockstep::unacked() const {return pushedEnd_ > ackFromPeer_ ? pushedEnd_ - ackFromPeer_ : 0;}

// The packet: first (4) | ack (4) | ahead (4) | count (1) | flags (1) | mark (2) | digest field (4) | digest value (8) | sum (4) | count x 6 bytes.
// `first`: the field of the first sample; `ack`: this side holds the other's samples below that field; flags bit 0: the digest is there.
std::size_t Lockstep::makePacket(std::uint8_t* out, std::size_t cap, std::uint32_t at) {
    if (!active_ || cap < head) return 0;
    const std::uint32_t first = ackFromPeer_;
    std::size_t count = pushedEnd_ > first ? pushedEnd_ - first : 0;
    count = std::min({count, max_samples, (cap - head) / 6});
    myAhead_ = std::int32_t(at) - std::int32_t(remoteNext_);
    put32(out, first);
    put32(out + 4, remoteNext_);
    put32(out + 8, std::uint32_t(myAhead_));
    out[12] = std::uint8_t(count);
    out[13] = latestDigest_ != none ? 1 : 0;
    out[14] = std::uint8_t(mark & 0xFF);
    out[15] = std::uint8_t(mark >> 8);
    put32(out + 16, latestDigest_);
    put64(out + 20, latestDigest_ != none ? mine_[(latestDigest_ / digest_every) % 16].value : 0);
    for (std::size_t i = 0; i < count; ++i) {
        const Slot& s = local_[(first + i) % ring];
        std::memcpy(out + head + i * 6, s.have ? s.s.data() : neutralSample().data(), 6);
    }
    put32(out + 28, check(out, head + count * 6));
    return head + count * 6;
}

bool Lockstep::receive(const std::uint8_t* in, std::size_t size) {
    if (!active_ || size < head) return false;
    const std::size_t count = in[12];
    if (count > max_samples || size != head + count * 6 || (in[14] | in[15] << 8) != mark || get32(in + 28) != check(in, size)) return false;
    const std::uint32_t first = get32(in), ack = get32(in + 4);
    // Whatever field the sample is for must fit in what is kept: from this side's floor to half the ring beyond what it already holds.
    if (first > remoteNext_ + ring / 2 || first + count > floor_ + ring) return false;
    if (ack > pushedEnd_) return false;                 // (it cannot hold samples this side has not made)
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t f = first + std::uint32_t(i);
        if (f < remoteNext_ || f < delay_) continue;
        Slot& slot = R(f);
        if (slot.have) continue;                        // (the first copy of a field stays: both copies are the same anyway)
        std::memcpy(slot.s.data(), in + head + i * 6, 6);
        slot.have = true;
    }
    advanceRemote();
    ackFromPeer_ = std::max(ackFromPeer_, ack);
    trimLocal();
    peerAhead_ = std::int32_t(get32(in + 8));
    if (in[13] & 1) {
        const std::uint32_t field = get32(in + 16);
        if (field != none && field % digest_every == 0) {
            Note& n = theirs_[(field / digest_every) % 16];
            if (n.field != field) {n.field = field; n.value = get64(in + 20);}
            compare(field, n.value);
        }
    }
    return true;
}

void Lockstep::advanceRemote() {
    while (remoteNext_ < floor_ + ring && R(remoteNext_).have) ++remoteNext_;
}

void Lockstep::noteDigest(std::uint32_t field, std::uint64_t value) {
    if (!active_ || field == none || field % digest_every != 0) return;
    Note& n = mine_[(field / digest_every) % 16];
    n.field = field;
    n.value = value;
    if (latestDigest_ == none || field > latestDigest_) latestDigest_ = field;
    const Note& t = theirs_[(field / digest_every) % 16];
    if (t.field == field) compare(field, t.value);
}

// (each side holds its own and the other's note for a field until a newer one takes the place: they meet whichever comes first)
void Lockstep::compare(std::uint32_t field, std::uint64_t theirValue) {
    const Note& m = mine_[(field / digest_every) % 16];
    if (m.field != field || field == lastCompared_) return;   // (the check value is in every packet: each field counts once)
    lastCompared_ = field;
    ++compared_;
    if (m.value != theirValue && !desync_) {desync_ = true; desyncField_ = field;}
}
