// A guest's TAG TEAM inputs through a lossy channel to the owner's kernel (the real shared/tag_kernel.c, kernel_fixture.c),
// sent as tag_team_p2p.cpp sends them: with TagInputResend (gs/tag_input_resend.h) and, for comparison, without it (one
// copy, as before). The owner takes each frame at its tick; what had not come by then is filled by its deadline (the last
// pad held, a one-frame command dropped). Counted: the guest's frames filled so, and its one-frame commands lost; every
// frame the owner took from the guest must be exactly the guest's. sh run.sh
#include "tag_input_resend.h"
#include <cstdio>
#include <cstdint>
#include <random>
#include <vector>
extern "C" {
#include "tag_team_wire.h"
unsigned fixture_fight(unsigned delay);
int fixture_input(unsigned member, const unsigned char *packet);
int fixture_tick(unsigned *late, unsigned char pads[TT_MAX_PLAYERS][TT_PAD_BYTES]);
unsigned fixture_next(void);
}
using Packet = std::array<unsigned char, 512>;
static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; std::printf("FAIL " __VA_ARGS__); std::printf("\n"); } } while (0)

struct Result { unsigned frames = 0, filled = 0, taps = 0, tapsLost = 0, wrong = 0, mismatches = 0, sent = 0; };

// The guest (seat 1, member 102) sends frame f at tick f - delay; a packet arrives `latency` ticks later (plus up to
// `jitter`), or is lost with `loss` percent. The other five players' inputs arrive on time.
template <std::size_t Before>
static Result run(int loss, unsigned latency, unsigned jitter, unsigned seed) {
    const unsigned delay = 4, ticks = 3000, guest = 1;
    Result r;
    std::mt19937 rnd(seed);
    TagInputResend<512, Before> resend;
    std::vector<std::vector<Packet>> arriving(ticks + 64);
    std::vector<std::array<unsigned char, TT_PAD_BYTES>> made(ticks + delay + 64);
    unsigned epoch = fixture_fight(delay);
    CHECK(epoch != 0, "the fight did not start");
    for (unsigned t = 0; t < ticks; ++t) {
        const unsigned f = t + delay; // (the frame the inputs made now are for)
        for (unsigned s = 0; s < 6; ++s) {
            Packet pk{};
            unsigned char pad[TT_PAD_BYTES] = {0, 0, 0, 0, 127, 127};
            pad[0] = (unsigned char)((f * 7 + s) & 0xFF); pad[4] = (unsigned char)(f * 3); // (buttons and stick changing)
            if (s == guest && f % 10 == 0) { pad[6] = 1 + f % 7; ++r.taps; }          // (a one-frame command: a target change)
            Ttw_Request(pk.data(), TTW_INPUT, epoch); Ttw_Write(pk.data() + 12, f); std::memcpy(pk.data() + 16, pad, TT_PAD_BYTES);
            if (s != guest) { fixture_input(101 + s, pk.data()); continue; }
            std::memcpy(made[f].data(), pad, TT_PAD_BYTES);
            resend.send(pk, epoch, [&](const Packet &q) {
                ++r.sent;
                if ((int)(rnd() % 100) < loss) return;
                unsigned at = t + latency + (jitter ? rnd() % (jitter + 1) : 0);
                if (at < arriving.size()) arriving[at].push_back(q);
            });
        }
        for (const Packet &q : arriving[t]) if (fixture_input(102, q.data()) == TT_MISMATCH) ++r.mismatches;
        unsigned late = 0; unsigned char pads[TT_MAX_PLAYERS][TT_PAD_BYTES];
        const unsigned frame = fixture_next();
        if (!fixture_tick(&late, pads)) { CHECK(false, "the owner could not take frame %u", frame); break; }
        if (frame < delay) continue; // (the start's neutral frames)
        ++r.frames;
        if (late & (1u << guest)) { ++r.filled; if (made[frame][6]) ++r.tapsLost; }
        else if (std::memcmp(pads[guest], made[frame].data(), TT_PAD_BYTES)) ++r.wrong;
    }
    return r;
}

int main() {
    struct Case { int loss; unsigned latency, jitter; } cases[] = {{0, 1, 0}, {5, 1, 0}, {10, 1, 1}, {20, 1, 1}};
    for (const Case &c : cases) {
        Result before = run<0>(c.loss, c.latency, c.jitter, 7), after = run<2>(c.loss, c.latency, c.jitter, 7);
        std::printf("loss %2d%%, %u tick(s) +%u: frames filled by the deadline %u -> %u of %u, one-frame commands lost %u -> %u of %u, packets %u -> %u\n",
                    c.loss, c.latency, c.jitter, before.filled, after.filled, after.frames, before.tapsLost, after.tapsLost, after.taps, before.sent, after.sent);
        CHECK(after.wrong == 0 && before.wrong == 0, "loss %d: a frame the owner took differs from the guest's (%u, %u)", c.loss, before.wrong, after.wrong);
        CHECK(after.mismatches == 0, "loss %d: the kernel saw two different copies of one frame (%u)", c.loss, after.mismatches);
        if (c.loss == 0) CHECK(after.filled == 0 && before.filled == 0, "no loss: frames filled (%u, %u)", before.filled, after.filled);
        else CHECK(after.filled * 5 <= before.filled, "loss %d: sent again, still %u frames filled of %u lost before", c.loss, after.filled, before.filled);
    }
    std::printf(failures ? "%d failure(s)\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
