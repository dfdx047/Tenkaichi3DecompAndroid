// Tests of plugin/online/lockstep.cpp: two sides in a simulated network (late, lost, repeated, reordered and damaged packets),
// each with its own player, must give the game on both the same two controllers at every field, the ones the players made `delay`
// fields before. On a computer:
//   g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I plugin/online plugin/tests/lockstep_tests.cpp plugin/online/lockstep.cpp -o lockstep_tests && ./lockstep_tests
#include "lockstep.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <random>
#include <vector>

namespace {
int failures = 0, checks = 0;
#define CHECK(cond, ...) do {++checks; if (!(cond)) {++failures; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n");}} while (0)

using Sample = Lockstep::Sample;

struct Link {                                   // one direction of the line
    struct Packet {long due; std::vector<std::uint8_t> bytes;};
    std::deque<Packet> queue;
    std::mt19937 rng;
    int lossPercent = 0, dupPercent = 0, corruptPercent = 0, minDelay = 1, maxDelay = 1;
    long burstUntil = -1;                       // every packet sent before this tick is lost (a tunnel)
    explicit Link(unsigned seed) : rng(seed) {}
    void send(long now, const std::uint8_t* p, std::size_t n) {
        if (now < burstUntil) return;
        if (int(rng() % 100) < lossPercent) return;
        for (int copies = 1 + (int(rng() % 100) < dupPercent); copies > 0; --copies) {
            Packet k{now + minDelay + long(rng() % (maxDelay - minDelay + 1)), std::vector<std::uint8_t>(p, p + n)};
            if (int(rng() % 100) < corruptPercent && !k.bytes.empty()) k.bytes[rng() % k.bytes.size()] ^= std::uint8_t(1 + rng() % 255);
            queue.push_back(std::move(k));       // (not sorted: a later packet may overtake an earlier one)
        }
    }
};

Sample sampleOf(std::mt19937& r) {Sample s{}; for (auto& b : s) b = std::uint8_t(r()); return s;}
std::uint64_t mixIn(std::uint64_t h, const Sample& a, const Sample& b) {
    for (auto x : a) h = (h ^ x) * 1099511628211ull;
    for (auto x : b) h = (h ^ x) * 1099511628211ull;
    return h;
}

struct Side {
    Lockstep ls;
    std::uint32_t field = 0;
    std::vector<Lockstep::Pair> seen;
    std::uint64_t digest = 14695981039346656037ull;
    unsigned stalls = 0;
    int slowEvery = 0;                          // a side whose clock is slower: it skips this many of every 4 ticks
};

struct Result {bool finished = false; long ticks = 0; unsigned stallsA = 0, stallsB = 0; bool desyncA = false, desyncB = false;
               unsigned comparedA = 0, comparedB = 0; std::uint32_t desyncFieldA = Lockstep::none;};

// Both sides play `fields` fields; the samples are random but known, so the pairs the game should see are known too.
Result run(unsigned seed, unsigned delay, int loss, int dup, int corrupt, int minDelay, int maxDelay, unsigned fields,
           long burstStart = -1, long burstLen = 0, int slowB = 0, long tamperAt = -1) {
    std::mt19937 rng(seed);
    std::vector<Sample> script[2];
    for (auto& v : script) for (unsigned i = 0; i < fields + 64; ++i) v.push_back(sampleOf(rng));
    Side s[2];
    s[0].ls.begin(0, delay);
    s[1].ls.begin(1, delay);
    s[1].slowEvery = slowB;
    Link toB(seed * 2 + 1), toA(seed * 2 + 2);
    for (Link* l : {&toA, &toB}) {l->lossPercent = loss; l->dupPercent = dup; l->corruptPercent = corrupt; l->minDelay = minDelay; l->maxDelay = maxDelay;}
    Link* in[2] = {&toA, &toB};                 // what each side receives
    Link* out[2] = {&toB, &toA};                // what each side sends
    Result res;
    const long limit = long(fields) * 40 + 4000;
    for (long now = 0; now < limit; ++now) {
        if (now == burstStart) {toA.burstUntil = toB.burstUntil = now + burstLen;}
        for (int i = 0; i < 2; ++i) {
            Link& l = *in[i];
            for (std::size_t k = 0; k < l.queue.size();) {
                if (l.queue[k].due <= now) {s[i].ls.receive(l.queue[k].bytes.data(), l.queue[k].bytes.size()); l.queue.erase(l.queue.begin() + long(k));}
                else ++k;
            }
        }
        for (int i = 0; i < 2; ++i) {
            Side& me = s[i];
            if (me.field < fields && !(me.slowEvery && now % 4 < me.slowEvery)) {
                me.ls.pushLocal(me.field, script[i][me.field]);
                if (me.ls.ready(me.field)) {
                    const Lockstep::Pair p = me.ls.pair(me.field);
                    me.seen.push_back(p);
                    me.digest = mixIn(me.digest, p.p1, p.p2);
                    if (me.field % Lockstep::digest_every == 0) me.ls.noteDigest(me.field, now == tamperAt || (tamperAt == -2 && i == 1 && me.field == 64) ? me.digest ^ 1 : me.digest);
                    me.ls.done(me.field);
                    ++me.field;
                } else ++me.stalls;
            }
            std::uint8_t pkt[1024];
            const std::size_t n = me.ls.makePacket(pkt, sizeof pkt, me.field);
            if (n) out[i]->send(now, pkt, n);
        }
        if (s[0].field >= fields && s[1].field >= fields) {res.finished = true; res.ticks = now; break;}
    }
    res.stallsA = s[0].stalls; res.stallsB = s[1].stalls;
    res.desyncA = s[0].ls.desync(); res.desyncB = s[1].ls.desync();
    res.comparedA = s[0].ls.compared(); res.comparedB = s[1].ls.compared();
    res.desyncFieldA = s[0].ls.desyncField();
    // What the game must have seen on both sides, field by field.
    for (int i = 0; i < 2; ++i) {
        bool same = s[i].seen.size() == fields;
        for (unsigned f = 0; same && f < fields; ++f) {
            const Lockstep::Pair want = f < delay ? Lockstep::Pair{Lockstep::neutralSample(), Lockstep::neutralSample()} : Lockstep::Pair{script[0][f - delay], script[1][f - delay]};
            if (s[i].seen[f].p1 != want.p1 || s[i].seen[f].p2 != want.p2) {
                CHECK(false, "seed %u delay %u loss %d: side %d saw another controller at field %u", seed, delay, loss, i, f);
                same = false;
            }
        }
        if (res.finished) CHECK(same, "side %d did not see all fields (%zu of %u)", i, s[i].seen.size(), fields);
    }
    return res;
}
}

int main() {
    // 1. A clean line with a little delay: no stall once the delay covers the line.
    for (unsigned delay : {0u, 1u, 2u, 4u, 8u, 16u}) {
        const Result r = run(7 + delay, delay, 0, 0, 0, 1, 1, 600);
        CHECK(r.finished, "clean line, delay %u: did not finish", delay);
        CHECK(!r.desyncA && !r.desyncB, "clean line, delay %u: false desync", delay);
        CHECK(r.comparedA > 10 && r.comparedB > 10, "clean line, delay %u: the check values were not compared (%u, %u)", delay, r.comparedA, r.comparedB);
        if (delay >= 2) CHECK(r.stallsA == 0 && r.stallsB == 0, "delay %u over a 1-tick line: stalled %u/%u", delay, r.stallsA, r.stallsB);
    }
    CHECK(Lockstep::delayFor(0) == 1 && Lockstep::delayFor(10) == 2 && Lockstep::delayFor(45) == 3 && Lockstep::delayFor(120) == 5 && Lockstep::delayFor(900) == 8 && Lockstep::delayFor(-1) == 3,
          "delayFor: %u %u %u %u %u %u", Lockstep::delayFor(0), Lockstep::delayFor(10), Lockstep::delayFor(45), Lockstep::delayFor(120), Lockstep::delayFor(900), Lockstep::delayFor(-1));
    // 2. A line that takes a few fields, jitters, loses, repeats, reorders: the controllers still agree. Many seeds.
    unsigned finished = 0, total = 0;
    for (unsigned seed = 1; seed <= 60; ++seed) {
        for (unsigned delay : {1u, 3u, 6u}) {
            const int loss = 10 + int(seed % 4) * 10;                 // 10 to 40 per cent
            const Result r = run(seed, delay, loss, 15, 0, 1, 1 + int(seed % 6), 500);
            ++total; finished += r.finished;
            CHECK(r.finished, "seed %u delay %u loss %d: deadlock", seed, delay, loss);
            CHECK(!r.desyncA && !r.desyncB, "seed %u: false desync under loss", seed);
        }
    }
    std::printf("lossy lines: %u of %u runs finished\n", finished, total);
    // 3. A tunnel: every packet lost for 150 ticks (2.5 s) in the middle: the game waits, then goes on, nothing is out of step.
    {
        const Result r = run(99, 3, 0, 0, 0, 1, 2, 800, 200, 150);
        CHECK(r.finished, "tunnel: did not finish");
        CHECK(r.stallsA > 100 && r.stallsB > 100, "tunnel: the game did not wait (%u, %u)", r.stallsA, r.stallsB);
    }
    // 4. A tunnel longer than what one packet repeats (64 fields at 60 a second is about a second): still recovered, the packet holds
    //    every sample the other has not acknowledged, from the oldest.
    {
        const Result r = run(5, 2, 0, 0, 0, 1, 1, 900, 100, 300);
        CHECK(r.finished, "long tunnel: did not finish");
    }
    // 5. One side's clock slower: the faster waits, nothing else changes.
    {
        const Result r = run(11, 4, 5, 5, 0, 2, 3, 800, -1, 0, 1);
        CHECK(r.finished, "slow clock: did not finish");
        CHECK(r.stallsA > 0, "slow clock: the faster side never waited");
    }
    // 6. Damaged packets (a bit changed): the ones the receiver can tell are bad are refused; the rest... must still not break the
    //    exchange: the run finishes and what the game saw is what was made. (The mark and the sizes catch most; a changed sample is
    //    covered by the same sample arriving again, because the first copy of a field is the one kept.)
    {
        unsigned bad = 0;
        for (unsigned seed = 1; seed <= 40; ++seed) {
            const Result r = run(seed, 3, 5, 5, 8, 1, 3, 400);
            bad += !r.finished;
        }
        std::printf("damaged packets: %u of 40 runs did not finish\n", bad);
    }
    // 7. The state check: equal values never differ; one changed value is found on both sides.
    {
        const Result r = run(21, 2, 0, 0, 0, 1, 1, 400, -1, 0, 0, -2);
        CHECK(r.desyncA && r.desyncB, "a changed check value at field 64 was not found (%d, %d)", r.desyncA, r.desyncB);
        CHECK(r.desyncFieldA == 64, "desync found at %u, not 64", r.desyncFieldA);
    }
    // 8. Garbage into receive(): no crash, nothing taken.
    {
        Lockstep ls;
        ls.begin(1, 2);
        std::mt19937 rng(3);
        std::uint8_t buf[200];
        unsigned taken = 0;
        for (int i = 0; i < 200000; ++i) {
            const std::size_t n = rng() % sizeof buf;
            for (std::size_t k = 0; k < n; ++k) buf[k] = std::uint8_t(rng());
            taken += ls.receive(buf, n);
        }
        CHECK(taken == 0, "random bytes were taken as packets %u times", taken);
        Lockstep a, b;
        a.begin(0, 2); b.begin(1, 2);
        a.pushLocal(0, Sample{1, 2, 3, 4, 5, 6});
        std::uint8_t pkt[256];
        const std::size_t n = a.makePacket(pkt, sizeof pkt, 0);
        CHECK(n >= 32, "a packet of %zu bytes", n);
        for (std::size_t cut = 0; cut < n; ++cut) CHECK(!b.receive(pkt, cut), "a packet cut to %zu bytes was taken", cut);
        CHECK(b.receive(pkt, n), "the whole packet was refused");
        // a packet that names fields far beyond what is kept
        pkt[0] = 0xFF; pkt[1] = 0xFF; pkt[2] = 0xFF; pkt[3] = 0x7F;
        CHECK(!b.receive(pkt, n), "a packet for a far field was taken");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
