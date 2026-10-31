// FightSession (fight_session.cpp) as the plugin uses it: two games in two threads on real UDP sockets. Each one "boots" for a number of fields, says it
// reached the character select, and then asks for its controllers once per field like the pad service. What both are given must be the two controllers
// the players made, whoever is player 1; the one that arrives first must wait for the other; a game that leaves must end the other's match.
//   g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I plugin/online plugin/tests/fight_tests.cpp plugin/online/{fight_session,link,relay,stun,session,lockstep}.cpp -o fight_tests -pthread && ./fight_tests
#include "fight_session.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace {
int failures = 0, checks = 0;
std::mutex printing;
#define CHECK(cond, ...) do {++checks; if (!(cond)) {++failures; std::lock_guard<std::mutex> l(printing); std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n");}} while (0)

using Sample = Lockstep::Sample;

struct Game {
    FightSession fs;
    std::vector<Sample> script;                 // what this player makes, field by field of the lockstep
    std::vector<Lockstep::Pair> seen;
    std::atomic<int> endedWith{0};              // 1: the match ended while it was running
    std::string reason;
    bool began = false;
    unsigned bootFields = 40, arriveAt = 30;
    std::thread th;
};

std::vector<Sample> makeScript(unsigned seed, unsigned n) {
    std::mt19937 r(seed);
    std::vector<Sample> v;
    for (unsigned i = 0; i < n; ++i) {Sample s{}; for (auto& b : s) b = std::uint8_t(r()); v.push_back(s);}
    return v;
}

void play(Game& g, const SessionFile& file, unsigned fields, int lossPercent, unsigned startDelayMs, unsigned leaveAfter) {
    std::this_thread::sleep_for(std::chrono::milliseconds(startDelayMs));
    FightSession::Env env;
    env.log = [&](const std::string& t) {if (std::getenv("FIGHT_LOG")) {std::lock_guard<std::mutex> l(printing); std::printf("[%s] %s\n", file.host ? "host" : "guest", t.c_str()); std::fflush(stdout);}};
    env.onEnded = [&](const std::string& why) {g.reason = why;};
    g.began = g.fs.begin(file, env);
    if (!g.began) return;
    g.fs.link().setTestLoss(lossPercent);
    unsigned boot = 0;
    for (;;) {                                  // the game: one call per field
        std::uint8_t p1[6], p2[6], local[6] = {};
        if (g.fs.field() < g.script.size()) std::memcpy(local, g.script[g.fs.field()].data(), 6);
        if (boot == g.arriveAt) g.fs.arrived();
        const std::uint32_t before = g.fs.field();
        const int r = g.fs.padField(local, p1, p2);
        if (r && g.fs.field() == before) ++boot;            // (the game walking its menus: neutral controllers)
        else if (r) {
            Lockstep::Pair p;
            std::memcpy(p.p1.data(), p1, 6); std::memcpy(p.p2.data(), p2, 6);
            g.seen.push_back(p);
            (void)before;
            if (g.seen.size() >= fields) break;
            if (leaveAfter && g.seen.size() == leaveAfter) {g.fs.leave("the player left"); break;}
        } else if (g.fs.ended()) {g.endedWith = 1; break;}
        std::this_thread::sleep_for(std::chrono::microseconds(500));   // (a field's work)
    }
}

SessionFile fileFor(bool host, std::uint16_t myPort, std::uint16_t peerPort, const std::string& code) {
    SessionFile f;
    f.host = host; f.port = myPort; f.code = code; f.myName = host ? "Host" : "Guest"; f.peerName = host ? "Guest" : "Host";
    f.addresses = {"127.0.0.1:" + std::to_string(peerPort)};
    return f;
}

// One match of `fields` fields. Returns whether both finished.
bool match(unsigned seed, int loss, unsigned fields, unsigned hostStartMs, unsigned guestStartMs, unsigned leaveAfter = 0) {
    // Ports: two sockets are opened first to learn two free ports, then closed and taken again by the matches (as a restarted engine takes the lobby's port).
    Link a, b;
    a.open(); b.open();
    const std::uint16_t pa = a.port(), pb = b.port();
    a.close(); b.close();
    Game host, guest;
    host.script = makeScript(seed * 2 + 1, fields + 64);
    guest.script = makeScript(seed * 2 + 2, fields + 64);
    const std::string code = "FIGHT" + std::to_string(seed);
    host.th = std::thread(play, std::ref(host), fileFor(true, pa, pb, code), fields, loss, hostStartMs, leaveAfter);
    guest.th = std::thread(play, std::ref(guest), fileFor(false, pb, pa, code), fields, loss, guestStartMs, 0u);
    host.th.join(); guest.th.join();
    CHECK(host.began && guest.began, "seed %u: a match could not open its socket", seed);
    if (leaveAfter) {
        CHECK(host.seen.size() == leaveAfter, "seed %u: the leaving side played %zu fields", seed, host.seen.size());
        CHECK(guest.endedWith == 1, "seed %u: the other side's match did not end when this one left", seed);
        CHECK(guest.reason.find("left") != std::string::npos, "seed %u: the reason was \"%s\"", seed, guest.reason.c_str());
        return true;
    }
    CHECK(host.seen.size() == fields && guest.seen.size() == fields, "seed %u loss %d: the games played %zu and %zu of %u fields", seed, loss, host.seen.size(), guest.seen.size(), fields);
    if (host.seen.size() != fields || guest.seen.size() != fields) return false;
    CHECK(host.fs.iAmPlayer1() != guest.fs.iAmPlayer1(), "seed %u: both think they are player %d", seed, host.fs.iAmPlayer1() ? 1 : 2);
    const unsigned delay = host.fs.delay();
    CHECK(delay == guest.fs.delay() && delay >= 1 && delay <= 8, "seed %u: delays %u and %u", seed, delay, guest.fs.delay());
    const Game& first = host.fs.iAmPlayer1() ? host : guest;      // player 1's game
    const Game& second = host.fs.iAmPlayer1() ? guest : host;
    for (unsigned f = 0; f < fields; ++f) {
        Sample w1 = Lockstep::neutralSample(), w2 = Lockstep::neutralSample();
        if (f >= delay) {w1 = first.script[f - delay]; w2 = second.script[f - delay];}
        for (const Game* g : {&host, &guest})
            if (g->seen[f].p1 != w1 || g->seen[f].p2 != w2) {CHECK(false, "seed %u loss %d: a game was given other controllers at field %u", seed, loss, f); return false;}
    }
    CHECK(host.fs.desyncs() == 0 && guest.fs.desyncs() == 0, "seed %u: a false difference of the state checks", seed);
    return true;
}
}

int main() {
    // the session file
    {
        SessionFile f, g;
        f.host = true; f.port = 39550; f.code = "ABC123"; f.myName = "Eu"; f.peerName = "Voce"; f.addresses = {"177.1.2.3:34599", "192.168.1.16:39550"};
        CHECK(parseSessionFile(formatSessionFile(f), g) && g.host && g.port == 39550 && g.code == "ABC123" && g.peerName == "Voce" && g.addresses.size() == 2, "the session file does not read back");
        CHECK(!parseSessionFile("garbage", g) && !parseSessionFile("role=host\ncode=X\n", g), "a session file without addresses or role was taken");
    }
    // a clean line, the guest's engine starts a moment after the host's
    match(1, 0, 400, 0, 400);
    // either one first, lost packets
    match(2, 20, 300, 300, 0);
    for (unsigned seed = 3; seed < 8; ++seed) match(seed, 25, 250, seed * 20 % 200, seed * 70 % 500);
    // a game that leaves ends the other's match
    match(20, 0, 300, 0, 100, 120);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
