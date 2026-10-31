#include "fight_session.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <random>
#include <sstream>
#include <thread>

bool parseSessionFile(const std::string& text, SessionFile& out) {
    out = SessionFile{};
    bool role = false;
    std::istringstream lines(text);
    for (std::string line; std::getline(lines, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "role") {out.host = value == "host"; role = value == "host" || value == "guest";}
        else if (key == "at") out.at = std::strtoll(value.c_str(), nullptr, 10);
        else if (key == "port") out.port = std::uint16_t(std::strtoul(value.c_str(), nullptr, 10));
        else if (key == "code") out.code = value;
        else if (key == "me") out.myName = value;
        else if (key == "peer") out.peerName = value;
        else if (key == "addr" && !value.empty() && out.addresses.size() < 8) out.addresses.push_back(value);
    }
    return role && !out.code.empty() && !out.addresses.empty();
}

std::string formatSessionFile(const SessionFile& f) {
    std::string t = std::string("role=") + (f.host ? "host" : "guest") + "\nat=" + std::to_string(f.at) + "\nport=" + std::to_string(f.port) + "\ncode=" + f.code + "\nme=" + f.myName + "\npeer=" + f.peerName + "\n";
    for (const auto& a : f.addresses) t += "addr=" + a + "\n";
    return t;
}

std::uint32_t FightSession::defaultNow() {
    return std::uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

void FightSession::sleep(unsigned ms) const {
    if (env_.sleepMs) env_.sleepMs(ms);
    else std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

bool FightSession::begin(const SessionFile& file, Env env) {
    file_ = file;
    env_ = std::move(env);
    if (!link_.open(file.port)) {log("online: the match could not open its socket"); return false;}
    link_.setGiveUpMs(90000);                    // (the other side's engine starts a few seconds either way)
    link_.start(file.code, file.addresses);
    if (!file.host) {ctl_.begin(false, SessionControl::Config{}); ctlBegun_ = true;}
    active_ = true;
    phase_ = Phase::Booting;
    log("online: match " + std::string(file.host ? "(host)" : "(guest)") + " with " + file.peerName + ", room " + file.code + ", socket on port " + std::to_string(link_.port()));
    return true;
}

void FightSession::pump() {
    link_.poll();
    for (const auto& n : link_.takeNotes()) log(n);
    Link::Message m;
    while (link_.popMessage(m)) {
        if (m.type == Lockstep::messageType) {lock_.receive(m.payload.data(), m.payload.size()); ctl_.peerIsRunning();}   // (a packet of the lockstep means the other side is past the meeting)
        else ctl_.onMessage(m.type, m.payload.data(), m.payload.size());
    }
    const std::uint32_t t = now();
    if (!ctlBegun_ && file_.host && link_.state() == Link::Up) {
        // the host offers the match's settings once it knows the line's round trip (a few pings), or after a moment without one
        if (!upSince_) upSince_ = t;
        if (link_.roundTripMs() >= 0 || t - upSince_ > 1500) {
            std::mt19937 rng(std::random_device{}() ^ t);
            cfg_.delay = std::uint8_t(Lockstep::delayFor(link_.roundTripMs()));
            cfg_.hostIsP1 = (rng() & 1) != 0;                 // who is player 1 is drawn
            cfg_.seed = rng();
            ctl_.begin(true, cfg_);
            ctlBegun_ = true;
            if (haveArrival_) ctl_.setArrived(arrivalField_);      // (begin starts the conversation afresh: the game may already be at the character select)
            log("online: the host offers delay " + std::to_string(cfg_.delay) + ", player 1 is " + (cfg_.hostIsP1 ? "the host" : "the guest") + " (round trip " + std::to_string(link_.roundTripMs()) + " ms)");
        }
    }
    if (ctlBegun_) {
        std::uint8_t type, buf[64];
        for (int n; (n = ctl_.next(t, type, buf, sizeof buf)) >= 0;) link_.sendMessage(type, buf, std::size_t(n));
    }
}

void FightSession::sendInputs() {
    std::uint8_t pkt[1100];
    if (const std::size_t n = lock_.makePacket(pkt, sizeof pkt, field_)) link_.sendMessage(Lockstep::messageType, pkt, n);
    lastSend_ = now();
}

void FightSession::end(const std::string& why, bool said) {
    if (phase_ == Phase::Ended) return;
    if (!said) link_.sayBye();
    reason_ = why;
    phase_ = Phase::Ended;
    log("online: the match ends: " + why + " (" + std::to_string(field_) + " fields played, " + std::to_string(waits_) + " waited for)");
    link_.close();
    if (env_.onEnded) env_.onEnded(why);
}

void FightSession::leave(const std::string& why) {
    if (!active_ || phase_ == Phase::Ended) return;
    end(why, false);
}

// A cheap look at the game's memory that must be the same on both sides at the same field: one 64-byte block in eight, a different eighth each time
// (so every byte is looked at within eight checks), about 4 MB read.
std::uint64_t FightSession::digest(std::uint32_t field) const {
    if (!env_.ram || env_.ramSize < 64) return 0;
    std::uint64_t h[4] = {0x9E3779B97F4A7C15ull, 0xC2B2AE3D27D4EB4Full, 0x165667B19E3779F9ull, 0x27D4EB2F165667C5ull};
    const std::size_t blocks = env_.ramSize / 64;
    for (std::size_t b = (field / Lockstep::digest_every) % 8; b < blocks; b += 8) {
        std::uint64_t w[8];
        std::memcpy(w, env_.ram + b * 64, 64);
        for (int i = 0; i < 8; ++i) {h[i & 3] = (h[i & 3] ^ w[i]) * 0x9E3779B97F4A7C15ull; h[i & 3] ^= h[i & 3] >> 29;}
    }
    return h[0] ^ (h[1] << 1) ^ (h[2] << 2) ^ (h[3] << 3) ^ field;
}

int FightSession::padField(const std::uint8_t local[6], std::uint8_t p1[6], std::uint8_t p2[6]) {
    if (!active_ || phase_ == Phase::Ended) return 0;      // (a match that is over: the game goes on with the controller it has)
    if (leaveRequested_.load(std::memory_order_acquire)) {end("the player left", false); return 0;}
    pump();
    const auto lineBad = [&]() -> const char* {
        if (ctl_.versionBad()) return "the other game speaks another version of the match";
        if (link_.peerLeft()) return "the other player left";
        if (link_.state() == Link::Failed) return "the line to the other game could not be made again";
        if (link_.state() == Link::Up && link_.silentMs() > (phase_ == Phase::Running ? 6000 : 20000)) return "the other game stopped answering";
        return nullptr;
    };
    if (const char* bad = lineBad()) {end(bad, link_.peerLeft()); return 0;}
    if (phase_ == Phase::Booting) {
        if (!arrived_.load(std::memory_order_acquire)) {          // (the start-up is the game's own: nothing the player touches counts)
            ++booted_;
            std::memcpy(p1, Lockstep::neutralSample().data(), 6);
            std::memcpy(p2, Lockstep::neutralSample().data(), 6);
            return 1;
        }
        haveArrival_ = true;
        arrivalField_ = booted_;
        if (ctlBegun_) ctl_.setArrived(arrivalField_);
        phase_ = Phase::Meeting;
        log("online: at the character select after " + std::to_string(booted_) + " fields; waiting for the other game");
    }
    if (phase_ == Phase::Meeting) {
        const std::uint32_t since = now();
        while (!ctl_.met()) {                    // the game's time stands still here: the one that arrived first does not go on
            pump();
            if (const char* bad = lineBad()) {end(bad, link_.peerLeft()); return 0;}
            if (now() - since > env_.meetTimeoutMs) {end("the other game did not reach the character select", false); return 0;}
            sleep(1);
        }
        cfg_ = ctl_.config();
        p1_ = file_.host == cfg_.hostIsP1;       // player 1 is the host when the host's draw says so
        lock_.begin(p1_ ? 0 : 1, cfg_.delay);
        phase_ = Phase::Running;
        field_ = 0;
        log("online: both are at the character select (fields " + std::to_string(ctl_.myField()) + " and " + std::to_string(ctl_.peerField()) + (ctl_.fieldsDiffer() ? ": DIFFERENT, the two games may not be in the same state" : ": the same") +
            "); I am player " + (p1_ ? "1" : "2") + ", delay " + std::to_string(cfg_.delay));
        if (env_.onMet) env_.onMet();
    }
    // Running: this field's controller goes out; the game goes on when the other side's for this field is here.
    Lockstep::Sample mine;
    std::memcpy(mine.data(), local, 6);
    lock_.pushLocal(field_, mine);
    sendInputs();
    if (!lock_.ready(field_)) {
        ++waits_;
        while (!lock_.ready(field_)) {
            pump();
            if (const char* bad = lineBad()) {end(bad, link_.peerLeft()); return 0;}
            if (now() - lastSend_ >= 8) sendInputs();          // (a waiting game says its latest again every few milliseconds)
            sleep(1);
        }
    }
    if (field_ % Lockstep::digest_every == 0) lock_.noteDigest(field_, digest(field_));
    if (lock_.desync() && !desyncSaid_) {
        desyncSaid_ = true;
        ++desyncs_;
        log("online: the two games differ at field " + std::to_string(lock_.desyncField()) + " (the check values of the memory are not the same)");
        if (env_.strictDesync) {end("the two games are no longer the same", false); return 0;}
    }
    const Lockstep::Pair pair = lock_.pair(field_);
    std::memcpy(p1, pair.p1.data(), 6);
    std::memcpy(p2, pair.p2.data(), 6);
    lock_.done(field_);
    ++field_;
    return 1;
}
