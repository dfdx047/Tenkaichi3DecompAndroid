#pragma once
#include "link.h"
#include "lockstep.h"
#include "session.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The engine process of an online match (BT3 starts the program again for a session: net.c, relaunch). The lobby (the Online Mode screens) has
// the line up and the other player's addresses; it writes them to `online_session` and the app starts the engine afresh. In that fresh
// process this class makes the same line again (on the port the lobby used, so the other side's router still knows it), tells the other side the
// match's settings, waits for the game to walk its menus by itself to the character select, waits there for the other side, and from then on
// runs the lockstep (lockstep.h) once per field from the pad service.
//
// It runs on the game's own thread: `padField` is called by the pad service at every vertical blank and may wait inside (that is how the game
// waits for the other player: its time stands still). Nothing here knows the game; the plugin gives it the memory for the state check.
struct SessionFile {
    bool host = false;                          // created the room (player 1 or 2 is decided by the host: SessionControl::Config::hostIsP1)
    std::uint16_t port = 0;                     // the UDP port of the lobby's socket
    std::string code, myName, peerName;
    std::vector<std::string> addresses;         // where the other game was reached
    std::int64_t at = 0;                        // when the lobby wrote it (seconds since 1970): an old file is not a match to start
};
bool parseSessionFile(const std::string& text, SessionFile& out);
std::string formatSessionFile(const SessionFile& file);

class FightSession {
public:
    struct Env {
        std::function<void(const std::string&)> log;
        const std::uint8_t* ram = nullptr;      // the game's memory, for the state check (null: none)
        std::size_t ramSize = 0;
        std::function<void()> onMet;            // both sides are at the character select: picture and sound come back
        std::function<void(const std::string& why)> onEnded;   // the match is over (once)
        std::function<std::uint32_t()> nowMs;   // (tests: another clock)
        std::function<void(unsigned)> sleepMs;  // (tests: another wait)
        bool strictDesync = false;              // a difference of the state checks ends the match (else it is only said)
        unsigned meetTimeoutMs = 180000;        // how long a game that is at the character select waits for the other
    };
    enum class Phase {Booting, Meeting, Running, Ended};

    bool begin(const SessionFile& file, Env env);
    bool active() const {return active_;}
    // The game's walk through its menus reached the character select (the plugin sees it; any thread).
    void arrived() {arrived_.store(true, std::memory_order_release);}
    // The pad service's call once per field: neutral controllers while the game walks its menus, then the two players'; 0 when the match is over.
    int padField(const std::uint8_t local[6], std::uint8_t p1[6], std::uint8_t p2[6]);
    Phase phase() const {return phase_;}
    bool running() const {return phase_ == Phase::Running;}
    bool ended() const {return phase_ == Phase::Ended;}
    const std::string& endReason() const {return reason_;}
    const std::string& myName() const {return file_.myName;}
    const std::string& peerName() const {return file_.peerName;}
    bool iAmPlayer1() const {return p1_;}
    unsigned delay() const {return cfg_.delay;}
    std::uint32_t field() const {return field_;}         // lockstep fields run
    int roundTripMs() const {return link_.roundTripMs();}
    unsigned waits() const {return waits_;}              // fields the game had to wait for the other's controller
    unsigned desyncs() const {return desyncs_;}
    void leave(const std::string& why);                  // this side ends it (says goodbye); on the game's thread
    void requestLeave() {leaveRequested_.store(true, std::memory_order_release);}   // from any thread: the next field ends the match
    Link& link() {return link_;}                         // (tests)

private:
    void pump();
    void sendInputs();
    void end(const std::string& why, bool said);
    std::uint64_t digest(std::uint32_t field) const;
    std::uint32_t now() const {return env_.nowMs ? env_.nowMs() : defaultNow();}
    void sleep(unsigned ms) const;
    static std::uint32_t defaultNow();
    void log(const std::string& text) const {if (env_.log) env_.log(text);}

    SessionFile file_;
    Env env_;
    Link link_;
    SessionControl ctl_;
    Lockstep lock_;
    SessionControl::Config cfg_;
    bool active_ = false, p1_ = false, ctlBegun_ = false, desyncSaid_ = false, endSaid_ = false;
    std::atomic<bool> arrived_{false}, leaveRequested_{false};
    std::atomic<Phase> phase_{Phase::Booting};
    std::string reason_;
    std::uint32_t booted_ = 0;                           // fields called before the character select
    bool haveArrival_ = false;                           // the game is at the character select (kept here: the host's conversation may not have begun yet)
    std::uint32_t arrivalField_ = 0;
    std::uint32_t field_ = 0;
    std::uint32_t upSince_ = 0, lastSend_ = 0, lastNote_ = 0;
    unsigned waits_ = 0, desyncs_ = 0;
};
