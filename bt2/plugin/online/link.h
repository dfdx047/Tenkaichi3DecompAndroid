#pragma once
#include <cstdint>
#include <deque>
#include <string>
#include <vector>
#include "relay.h"
#include "stun.h"

// The direct line between the two games of an online match (UDP). The matchmaking service (the app talks to it) only tells each
// side the other's name and addresses; this is what makes sure the other game is really there and how long a packet takes.
//
// Both sides open a socket before they ask the service for a room, and give it their addresses ("ip:port"). Once each has the
// other's, both send a HELLO to every address at the same time, over and over: a router that only lets in what the home sent
// out lets the other side's HELLO in after its own was sent (the "hole punch"). The first HELLO or HELLO_ACK that comes back
// from the other game, with the room's session number, makes the line UP; after that a PING goes every half second and its PONG
// gives the round trip. Without an answer in 15 seconds the line has FAILED. A host whose network does not let the other side in
// asks a TURN server for a relayed address (relay.h) on this same socket; the other side sends to it as to any address, and what
// comes back from the server is unwrapped here, so the rest of the game sees one line either way.
//
// A packet is 20 bytes, little-endian: magic "BT2N", version 1, type, 2 reserved, the session (a hash of the room's code), a
// number, and the sender's clock in milliseconds (echoed back by a PONG).
class Link {
public:
    enum State {Closed, Open, Linking, Up, Failed};

    ~Link();
    // The socket; `wanted` is the port to take if it is free (a match restarts the engine, and the other side's router only knows the
    // port this phone used before), else any port. False: no socket could be made.
    bool open(std::uint16_t wanted = 0);
    void close();
    // This game's addresses for the other side: the one the router shows to the internet first (from STUN, stun.h), then the home's own.
    std::vector<std::string> localAddresses() const;
    bool addressesReady() const;                         // the STUN answers are in (or given up on, or 1.2 s have passed)
    bool symmetricRouter() const {return stun_.symmetric();}
    // Starts the exchange with the other game: `code` names the session, `addresses` are the other side's "ip:port" strings.
    void start(const std::string& code, const std::vector<std::string>& addresses);
    void poll();                                         // once a frame: sends what is due, reads what came
    State state() const {return state_;}
    int roundTripMs() const {return rtt_;}               // smoothed, milliseconds; -1 before the first PONG
    bool peerAddress(std::string& out) const;            // where the other game answered from, once UP
    bool peerLeft() const {return peerLeft_;}            // the other game said goodbye (a BYE packet)
    int silentMs() const;                                // milliseconds since anything came from the other game (while UP)
    void sayBye();                                       // tells the other game this one is leaving
    bool viaRelay() const {return remote_.relayed;}      // the line goes through the TURN server
    // The host's relay: `server` is "host:port", the login comes from the matchmaking service. Asked once the socket is open.
    void beginRelay(const std::string& server, const std::string& user, const std::string& credential) {relay_.begin(server, user, credential);}
    bool relayStarted() const {return relay_.started();}
    bool relayActive() const {return relay_.active();}
    bool relayFailed() const {return relay_.failed();}
    const std::string& relayAddress() const {return relay_.relayedAddress();}
    // Messages of the game's own conversation (session.h, lockstep.h) travel on the same line, once it is UP: a type from 6 up and a payload.
    // Like everything here they may be lost, repeated or late: whoever sends them repeats them as long as it needs.
    struct Message {std::uint8_t type = 0; std::vector<std::uint8_t> payload;};
    static constexpr std::uint8_t first_message_type = 6;
    static constexpr std::size_t max_payload = 1200;
    bool sendMessage(std::uint8_t type, const std::uint8_t* payload, std::size_t size);   // false: the line is not UP, or the message is too big
    bool popMessage(Message& out);                       // the oldest message that came, if any
    std::uint16_t port() const {return port_;}           // the socket's port on this phone
    void setGiveUpMs(std::uint32_t ms) {giveUp_ = ms;}   // how long the hellos go on without an answer (15 s; a match that restarts the engine waits for the other side's start)
    void setTestLoss(int percent) {testLoss_ = percent;} // development and tests: that share of what is sent is dropped (BT3: BT3_NET_LOSS)
    // What happened on the line, in words, for the game's log (the plugin takes them each frame): the addresses, the hellos sent and what
    // came back, why it failed. Without this a failed match leaves nothing to tell a blocked router from a wrong address.
    std::vector<std::string> takeNotes() {auto out = std::move(notes_); notes_.clear(); return out;}

private:
    struct Candidate {std::uint32_t ip = 0;std::uint16_t port = 0;bool relayed = false;};
    void send(std::uint8_t type, std::uint32_t number, std::uint32_t echo, const Candidate& to, const std::uint8_t* payload = nullptr, std::size_t size = 0);
    static std::uint32_t now();

    void note(const std::string& text) {if (notes_.size() < 64) notes_.push_back(text);}
    static std::string text(const Candidate& c);

    int socket_ = -1;
    std::vector<std::string> notes_;
    std::deque<Message> inbox_;
    int testLoss_ = 0;
    std::uint32_t giveUp_ = 15000;
    std::uint32_t lossState_ = 2463534242u;
    std::uint32_t sent_ = 0, got_ = 0, foreign_ = 0, lastNote_ = 0;
    Candidate lastForeign_;
    State state_ = Closed;
    std::uint16_t port_ = 0;
    std::uint32_t session_ = 0, number_ = 0, started_ = 0, lastHello_ = 0, lastPing_ = 0;
    std::vector<std::string> local_;
    std::vector<Candidate> candidates_;
    Candidate remote_;
    int rtt_ = -1;
    bool peerLeft_ = false;
    std::uint32_t heard_ = 0;
    Relay relay_;
    Stun stun_;
    std::uint32_t openedAt_ = 0;
};
