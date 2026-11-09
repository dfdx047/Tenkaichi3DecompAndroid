// Tests of plugin/online/session.cpp: the settings reach the other side, the two meet only when both have arrived, whatever the line
// loses, repeats or delays; damaged messages are refused.
//   g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I plugin/online plugin/tests/session_tests.cpp plugin/online/session.cpp -o session_tests && ./session_tests
#include "session.h"
#include <cstdio>
#include <deque>
#include <random>
#include <vector>

namespace {
int failures = 0, checks = 0;
#define CHECK(cond, ...) do {++checks; if (!(cond)) {++failures; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n");}} while (0)

struct Msg {long due; std::uint8_t type; std::vector<std::uint8_t> bytes;};
struct Line {
    std::deque<Msg> q;
    std::mt19937 rng;
    int loss = 0, dup = 0, corrupt = 0, minDelay = 1, maxDelay = 1;
    explicit Line(unsigned seed) : rng(seed) {}
    void send(long now, std::uint8_t type, const std::uint8_t* p, std::size_t n) {
        if (int(rng() % 100) < loss) return;
        for (int c = 1 + (int(rng() % 100) < dup); c > 0; --c) {
            Msg m{now + minDelay + long(rng() % (maxDelay - minDelay + 1)), type, std::vector<std::uint8_t>(p, p + n)};
            if (int(rng() % 100) < corrupt && !m.bytes.empty()) m.bytes[rng() % m.bytes.size()] ^= std::uint8_t(1 + rng() % 255);
            q.push_back(std::move(m));
        }
    }
};

struct Outcome {bool metA = false, metB = false; long metAt = 0; bool sameConfig = false, differ = false, versionBad = false; unsigned configsSent = 0;};

// Side A is the host. `arriveA` / `arriveB`: the tick at which each game reaches the character select (field numbers `fieldA` / `fieldB`).
Outcome run(unsigned seed, int loss, int dup, int corrupt, long arriveA, long arriveB, std::uint32_t fieldA, std::uint32_t fieldB) {
    SessionControl s[2];
    SessionControl::Config offer;
    offer.delay = 4; offer.hostIsP1 = (seed & 1) != 0; offer.seed = 0xC0FFEE00u + seed;
    s[0].begin(true, offer);
    s[1].begin(false, SessionControl::Config{});
    Line toB(seed * 3 + 1), toA(seed * 3 + 2);
    for (Line* l : {&toA, &toB}) {l->loss = loss; l->dup = dup; l->corrupt = corrupt; l->minDelay = 1; l->maxDelay = 1 + int(seed % 5);}
    Line* in[2] = {&toA, &toB};
    Line* out[2] = {&toB, &toA};
    Outcome o;
    for (long now = 0; now < 4000; ++now) {
        const std::uint32_t ms = std::uint32_t(now * 17);          // a tick is a field: 16.7 ms
        for (int i = 0; i < 2; ++i)
            for (std::size_t k = 0; k < in[i]->q.size();) {
                if (in[i]->q[k].due <= now) {s[i].onMessage(in[i]->q[k].type, in[i]->q[k].bytes.data(), in[i]->q[k].bytes.size()); in[i]->q.erase(in[i]->q.begin() + long(k));}
                else ++k;
            }
        if (now == arriveA) s[0].setArrived(fieldA);
        if (now == arriveB) s[1].setArrived(fieldB);
        for (int i = 0; i < 2; ++i) {
            std::uint8_t type, buf[64];
            for (int n; (n = s[i].next(ms, type, buf, sizeof buf)) >= 0;) {
                if (i == 0 && type == SessionControl::tConfig) ++o.configsSent;
                out[i]->send(now, type, buf, std::size_t(n));
            }
        }
        // Before both have arrived nobody may have met.
        for (int i = 0; i < 2; ++i)
            if (s[i].met()) CHECK(now >= std::max(arriveA, arriveB), "side %d met at tick %ld, before both had arrived (%ld, %ld)", i, now, arriveA, arriveB);
        if (s[0].met() && s[1].met()) {o.metA = o.metB = true; o.metAt = now; break;}
    }
    o.sameConfig = s[1].configKnown() && s[1].config().delay == offer.delay && s[1].config().hostIsP1 == offer.hostIsP1 && s[1].config().seed == offer.seed;
    o.differ = s[0].fieldsDiffer() || s[1].fieldsDiffer();
    o.versionBad = s[0].versionBad() || s[1].versionBad();
    return o;
}
}

int main() {
    // 1. A clean line: the settings arrive intact, both meet soon after the later one arrives, the fields agree.
    {
        const Outcome o = run(1, 0, 0, 0, 30, 90, 1500, 1500);
        CHECK(o.metA && o.metB, "clean line: did not meet");
        CHECK(o.sameConfig, "clean line: the guest has other settings than the host offered");
        CHECK(!o.differ, "clean line: fields reported as different");
        CHECK(o.metAt >= 90 && o.metAt < 110, "clean line: met at tick %ld (the later one arrived at 90)", o.metAt);
        CHECK(o.configsSent <= 3, "clean line: the host sent its settings %u times", o.configsSent);
    }
    // 2. The two reached different fields: they meet, and it is said.
    {
        const Outcome o = run(2, 0, 0, 0, 40, 40, 1500, 1512);
        CHECK(o.metA && o.metB && o.differ, "different fields: met %d %d, said %d", o.metA, o.metB, o.differ);
    }
    // 3. Either one first, any line: 100 seeds, 10 to 40% lost, repeated, reordered, late.
    unsigned met = 0, total = 0;
    for (unsigned seed = 1; seed <= 100; ++seed) {
        const int loss = 10 + int(seed % 4) * 10;
        const long a = 20 + long(seed * 7 % 200), b = 20 + long(seed * 13 % 300);
        const Outcome o = run(seed, loss, 20, 0, a, b, 1500, 1500);
        ++total; met += o.metA && o.metB;
        CHECK(o.metA && o.metB, "seed %u loss %d (arrive %ld, %ld): did not meet", seed, loss, a, b);
        CHECK(o.sameConfig, "seed %u: settings differ", seed);
        CHECK(!o.differ, "seed %u: fields differ", seed);
    }
    std::printf("lossy lines: %u of %u sessions met\n", met, total);
    // 4. Damaged messages: refused (the sum), so the session still settles and never with wrong settings.
    for (unsigned seed = 1; seed <= 60; ++seed) {
        const Outcome o = run(seed, 10, 10, 15, 30, 60, 1500, 1500);
        CHECK(o.metA && o.metB, "damaged, seed %u: did not meet", seed);
        CHECK(o.sameConfig, "damaged, seed %u: wrong settings taken", seed);
    }
    // 5. Another version of what goes over the line: said, not taken.
    {
        SessionControl a, b;
        a.begin(true, SessionControl::Config{});
        b.begin(false, SessionControl::Config{});
        std::uint8_t type, buf[64];
        const int n = a.next(0, type, buf, sizeof buf);
        CHECK(n == 16 && type == SessionControl::tConfig, "the host's first message: %d, %d", n, type);
        buf[0] = 99;                                            // another version: the sum no longer fits ...
        CHECK(!b.onMessage(type, buf, std::size_t(n)), "a changed message was taken");
        std::uint32_t h = 2166136261u;                           // ... so make one of another version that is whole
        for (int i = 0; i < 12; ++i) h = (h ^ buf[i]) * 16777619u;
        for (int i = 0; i < 4; ++i) buf[12 + i] = std::uint8_t(h >> (8 * i));
        CHECK(b.onMessage(type, buf, std::size_t(n)) && b.versionBad() && !b.configKnown(), "another version was not told apart");
    }
    // 6. Random bytes: nothing taken, no crash.
    {
        SessionControl a;
        a.begin(false, SessionControl::Config{});
        std::mt19937 rng(9);
        std::uint8_t buf[40];
        unsigned taken = 0;
        for (int i = 0; i < 300000; ++i) {
            const std::size_t n = rng() % sizeof buf;
            for (std::size_t k = 0; k < n; ++k) buf[k] = std::uint8_t(rng());
            taken += a.onMessage(std::uint8_t(6 + rng() % 3), buf, n);
        }
        CHECK(taken == 0, "random bytes taken as messages %u times", taken);
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
