// The whole online stack on real UDP sockets, two sides in one process: the line (link.cpp), the session (session.cpp) and the lockstep
// (lockstep.cpp) as the plugin uses them. The games are two loops that make a controller each field; what each one is given must be the
// two controllers the players made `delay` fields before, whatever the line loses. Needs no internet (the STUN lookups fail quietly).
//   g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I plugin/online plugin/tests/stack_tests.cpp plugin/online/{link,relay,stun,session,lockstep}.cpp -o stack_tests -pthread && ./stack_tests
#include "link.h"
#include "lockstep.h"
#include "session.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <thread>
#include <vector>

namespace {
int failures = 0, checks = 0;
#define CHECK(cond, ...) do {++checks; if (!(cond)) {++failures; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n");}} while (0)

using Sample = Lockstep::Sample;
std::uint32_t nowMs() {return std::uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}

struct Game {                                   // what the plugin does in one phone
    Link link;
    SessionControl session;
    Lockstep lock;
    std::vector<Lockstep::Pair> seen;
    std::vector<Sample> script;
    std::uint32_t field = 0;
    bool arrivedSet = false, begun = false;
    long arriveAtMs = 0;
    int id = 0;

    void pump(std::uint32_t t) {
        link.poll();
        Link::Message m;
        while (link.popMessage(m)) {
            if (m.type == Lockstep::messageType) {if (lock.receive(m.payload.data(), m.payload.size())) session.peerIsRunning();}
            else session.onMessage(m.type, m.payload.data(), m.payload.size());
        }
        std::uint8_t type, buf[64];
        for (int n; (n = session.next(t, type, buf, sizeof buf)) >= 0;) link.sendMessage(type, buf, std::size_t(n));
    }
    // One field of the game, when it can go on.
    void tick(unsigned fields, std::uint32_t t) {
        if (!begun) {
            if (session.met()) {
                const bool p1 = session.isHost() == session.config().hostIsP1;
                lock.begin(p1 ? 0 : 1, session.config().delay);
                begun = true;
            }
            return;
        }
        if (field >= fields) return;
        lock.pushLocal(field, script[field]);
        if (lock.ready(field)) {
            seen.push_back(lock.pair(field));
            lock.done(field);
            ++field;
        }
        std::uint8_t pkt[1200];
        if (const std::size_t n = lock.makePacket(pkt, sizeof pkt, field)) link.sendMessage(Lockstep::messageType, pkt, n);
        (void)t;
    }
};

bool play(unsigned seed, int lossPercent, unsigned fields, bool hostIsP1, unsigned delay, long arriveAms, long arriveBms) {
    Game g[2];
    std::mt19937 rng(seed);
    for (int i = 0; i < 2; ++i) {
        g[i].id = i;
        for (unsigned f = 0; f < fields + 64; ++f) {Sample s{}; for (auto& b : s) b = std::uint8_t(rng()); g[i].script.push_back(s);}
        if (!g[i].link.open()) {std::printf("no socket\n"); return false;}
        g[i].link.setTestLoss(lossPercent);
    }
    const std::string code = "TEST" + std::to_string(seed);
    g[0].link.start(code, {"127.0.0.1:" + std::to_string(g[1].link.port())});
    g[1].link.start(code, {"127.0.0.1:" + std::to_string(g[0].link.port())});
    SessionControl::Config offer;
    offer.delay = std::uint8_t(delay); offer.hostIsP1 = hostIsP1; offer.seed = seed;
    g[0].session.begin(true, offer);
    g[1].session.begin(false, SessionControl::Config{});
    g[0].arriveAtMs = arriveAms; g[1].arriveAtMs = arriveBms;
    const std::uint32_t start = nowMs();
    bool done = false;
    while (nowMs() - start < 40000) {
        const std::uint32_t t = nowMs() - start;
        for (int i = 0; i < 2; ++i) {
            if (g[i].link.state() == Link::Up && !g[i].arrivedSet && long(t) >= g[i].arriveAtMs) {g[i].session.setArrived(1500); g[i].arrivedSet = true;}
            g[i].pump(t);
            g[i].tick(fields, t);
        }
        if (g[0].field >= fields && g[1].field >= fields) {done = true; break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CHECK(done, "seed %u loss %d: did not finish (A at %u, B at %u, links %d/%d, met %d/%d)", seed, lossPercent, g[0].field, g[1].field,
          int(g[0].link.state()), int(g[1].link.state()), g[0].session.met(), g[1].session.met());
    if (!done) return false;
    for (int i = 0; i < 2; ++i) {
        const bool p1 = (i == 0) == hostIsP1;                      // this side's controller is player 1's
        bool same = g[i].seen.size() == fields;
        for (unsigned f = 0; same && f < fields; ++f) {
            Sample want1 = Lockstep::neutralSample(), want2 = Lockstep::neutralSample();
            if (f >= delay) {
                want1 = g[hostIsP1 ? 0 : 1].script[f - delay];     // player 1's controller, made `delay` fields before
                want2 = g[hostIsP1 ? 1 : 0].script[f - delay];
            }
            if (g[i].seen[f].p1 != want1 || g[i].seen[f].p2 != want2) {CHECK(false, "seed %u loss %d side %d: field %u differs", seed, lossPercent, i, f); same = false;}
            (void)p1;
        }
        CHECK(same, "side %d did not see every field", i);
    }
    // The goodbye: the other side learns of it.
    g[0].link.sayBye();
    for (int k = 0; k < 200 && !g[1].link.peerLeft(); ++k) {g[1].link.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    CHECK(g[1].link.peerLeft() || lossPercent > 0, "the goodbye did not arrive");
    return true;
}
}

int main() {
    // The line goes up, the settings pass, both meet, and the fields agree: clean, then with a quarter of the packets lost each way.
    play(1, 0, 400, true, 3, 50, 220);
    play(2, 0, 400, false, 2, 300, 40);
    for (unsigned seed = 10; seed < 16; ++seed) play(seed, 25, 300, (seed & 1) != 0, 3 + seed % 3, 30 + long(seed * 37 % 250), 30 + long(seed * 91 % 400));
    play(30, 40, 200, true, 4, 60, 60);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
