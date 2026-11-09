#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// Two games kept in step by exchanging the players' input, one sample per field (a vertical blank): BT3's netplay (port/src/gs/net.c)
// done for BT2, without rewinding. Each side has ONE local player; the host is player 1 (port 0), the one who joined is player 2
// (port 1). The input a player makes at field F takes effect at field F + delay on BOTH sides, which gives the other side's copy
// time to arrive before it is needed; a game that has not got the other's input for the field it is at waits (`ready` is false).
//
// This file knows nothing of the network or of the game: the plugin feeds it the packets that come (`receive`) and sends the ones
// it makes (`makePacket`), and asks `ready` / `pair` once per field. That keeps it testable on a computer, with lost, late,
// repeated and damaged packets (plugin/tests/lockstep_tests.cpp).
//
// A packet carries every sample of this side that the other has not yet said it holds (at most `max_samples`), so a lost packet
// is covered by the next, however many are lost in a row, up to that bound. It also says up to which field it holds the other's
// samples (`ack`), how far beyond the other's input this side is (`ahead`, for keeping the two clocks together) and, now and then,
// a check value of the game's state at a field (`digest`): two sides that differ at the same field have diverged.
class Lockstep {
public:
    using Sample = std::array<std::uint8_t, 6>;          // the controller as the pad service takes it: buttons (2 bytes), sticks (4)
    struct Pair {Sample p1, p2;};
    static constexpr std::size_t max_samples = 64;       // most samples in one packet
    static constexpr std::size_t digest_every = 32;      // fields between two check values
    static constexpr std::uint32_t none = 0xFFFFFFFFu;
    static Sample neutralSample() {return Sample{0, 0, 0x80, 0x80, 0x80, 0x80};}   // no button, the four sticks at rest (0x80)
    static constexpr std::uint8_t messageType = 9;       // the line's message type for these packets (link.h; session.h uses 6 to 8)

    // The delay for a line whose round trip is `rttMs`: the one-way time in fields (a field is 16.7 ms), rounded up, and one more for the
    // line's jitter; 1 to 8. (45 ms: 3 fields.) The host measures the round trip and tells the other (BT3: T_CONFIG).
    static unsigned delayFor(int rttMs) {
        if (rttMs < 0) return 3;
        const unsigned oneWay = unsigned((rttMs * 1000 / 2 + 16666) / 16667);     // (rounded up)
        return oneWay + 1 < 1 ? 1 : oneWay + 1 > 8 ? 8 : oneWay + 1;
    }
    // role 0: host (player 1), 1: the one who joined (player 2). `delay`: fields (0 to 16). Field numbers count from 0 at the
    // moment both sides start the exchange (the meeting at the character select).
    void begin(int role, unsigned delay);
    bool active() const {return active_;}
    int role() const {return role_;}
    unsigned delay() const {return delay_;}

    // The local player's sample for the field the game is at. Only the first call for a field counts (a game that waited asks again).
    void pushLocal(std::uint32_t field, const Sample& sample);
    // Whether the other side's sample for `field` is here (or `field` is before the first one that counts: neutral on both).
    bool ready(std::uint32_t field) const;
    // Both players' samples for `field` (call only when `ready`); player 1's is the host's, player 2's the joined one's.
    Pair pair(std::uint32_t field) const;
    // The game went on past `field`: what is older is dropped.
    void done(std::uint32_t field);

    // The packet to send now: its length, 0 when there is nothing to say. `at`: the field the game is at; `cap` bytes of room.
    std::size_t makePacket(std::uint8_t* out, std::size_t cap, std::uint32_t at);
    // A packet that came. False: it is not one of this exchange (too short, damaged, from before `begin`); nothing was taken from it.
    bool receive(const std::uint8_t* in, std::size_t size);

    // The state check: the game's digest at `field` (a multiple of digest_every, the field already run with the other's real input).
    void noteDigest(std::uint32_t field, std::uint64_t value);
    bool desync() const {return desync_;}
    std::uint32_t desyncField() const {return desyncField_;}
    unsigned compared() const {return compared_;}          // check values of the two sides compared so far

    // What the meter shows, and what keeps the two clocks together: how far beyond the other's input each side is (BT3's `ahead`; on an
    // even line the two numbers are equal, and the side whose clock runs ahead has the larger one).
    std::int32_t peerAhead() const {return peerAhead_;}
    std::int32_t myAhead() const {return myAhead_;}
    std::uint32_t nextFromPeer() const {return remoteNext_;}   // the first field whose sample of the other is still missing
    std::uint32_t unacked() const;                             // samples of this side the other has not said it holds

private:
    static constexpr std::size_t ring = 1024;            // fields kept (a power of two)
    static constexpr std::size_t head = 32;              // the packet's fixed part (see makePacket)
    static constexpr std::uint16_t mark = 0xB2C7;
    struct Slot {Sample s{}; bool have = false;};
    Slot local_[ring], remote_[ring];
    struct Note {std::uint32_t field = none; std::uint64_t value = 0;};
    Note mine_[16], theirs_[16];

    bool active_ = false;
    int role_ = 0;
    unsigned delay_ = 0;
    std::uint32_t pushedEnd_ = 0;     // one past the last field a local sample is held for (fields before `delay_` are neutral)
    std::uint32_t remoteNext_ = 0;    // the first field the other's sample is missing for
    std::uint32_t ackFromPeer_ = 0;   // the other side holds this side's samples below this field
    std::uint32_t floor_ = 0;         // the game is past these fields: the other's samples below are dropped
    std::uint32_t localFloor_ = 0;    // this side's own samples below are dropped: only when the game is past them AND the other holds them
    std::int32_t peerAhead_ = 0;
    std::int32_t myAhead_ = 0;
    std::uint32_t latestDigest_ = none;
    bool desync_ = false;
    std::uint32_t desyncField_ = none;
    unsigned compared_ = 0;
    std::uint32_t lastCompared_ = none;

    Slot& L(std::uint32_t f) {return local_[f % ring];}
    Slot& R(std::uint32_t f) {return remote_[f % ring];}
    const Slot& R(std::uint32_t f) const {return remote_[f % ring];}
    void advanceRemote();
    void trimLocal();
    static std::uint32_t check(const std::uint8_t* packet, std::size_t size);
    void compare(std::uint32_t field, std::uint64_t value);
};
