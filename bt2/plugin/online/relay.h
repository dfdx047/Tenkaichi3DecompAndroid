#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

// A relay address for the host of a match: a small TURN client (RFC 5766 over UDP, long-term credentials), for the pairs of players
// that cannot send to each other directly (routers that give every destination another port, mobile networks).
//
// A TURN server has a public address. The host asks it for a "relayed address" and tells the other player that one (the app
// publishes it to the matchmaking service as "relay=ip:port"). What the other player sends there, the server hands to the host in
// a "data indication"; what the host sends, it gives the server in a "send indication". Only the host speaks TURN: for the other
// player the relayed address is an ordinary address, so the joiner's Link needs nothing of this.
//
// Asked: Allocate (first without a login: the server answers 401 with its realm and a nonce, and the request is made again signed
// with the login), CreatePermission (the other player's address may send to us; five minutes, renewed), Refresh (the allocation
// lives ten minutes, renewed). A stale nonce (438) brings a new one. Nothing here waits: the Link calls poll() every frame and the
// socket is the Link's own (the relay's answers come in on it from the server's address; see Link::poll).
class Relay {
public:
    ~Relay();
    // `server` is "host:port" (the name is resolved on a thread of its own); the login comes from the matchmaking service.
    void begin(const std::string& server, const std::string& username, const std::string& credential);
    void end(int socket);                                // gives the address up (a Refresh of lifetime 0) and forgets everything
    void poll(int socket);                               // sends what is due
    bool active() const {return state_ == Active;}
    bool failed() const {return state_ == Failed;}
    bool started() const {return state_ != Idle;}
    const std::string& relayedAddress() const {return relayed_;}   // "ip:port" once active
    // The other player's addresses (IPv4 as network-order words) that the server must let through.
    void permit(int socket, std::uint32_t ip);
    // A packet that came from `from` (ip, port in network order): the server's own answer (consumed: false), or a data indication
    // (true; `data`/`length` and `peerIp`/`peerPort` are the other player's packet and where it came from).
    bool unwrap(int socket, const std::uint8_t* packet, int size, std::uint32_t fromIp, std::uint16_t fromPort,
                const std::uint8_t** data, int* length, std::uint32_t* peerIp, std::uint16_t* peerPort);
    bool isServer(std::uint32_t ip, std::uint16_t port) const {return resolved_ && ip == serverIp_ && port == serverPort_;}
    // Sends `length` bytes to the other player through the server.
    void send(int socket, const void* data, int length, std::uint32_t ip, std::uint16_t port);

private:
    enum State {Idle, Resolving, Asking, Active, Failed};
    int message(std::uint8_t* m, unsigned type);
    int attribute(std::uint8_t* m, int at, unsigned type, const void* value, int length);
    int address(std::uint8_t* m, int at, unsigned type, std::uint32_t ip, std::uint16_t port);
    int sign(std::uint8_t* m, int at);
    void toServer(int socket, const std::uint8_t* m, int length);
    void takeLogin(const std::uint8_t* m, int size);
    void ask(int socket);
    void sendPermit(int socket, std::uint32_t ip);

    State state_ = Idle;
    std::thread resolver_;
    bool resolved_ = false;
    std::atomic<int> resolvedFlag_{0};
    std::string host_, port_;
    std::uint32_t serverIp_ = 0;
    std::uint16_t serverPort_ = 0;                       // network order, like the ip
    std::string username_, credential_, realm_, nonce_, relayed_;
    std::uint8_t key_[16] = {};
    bool haveLogin_ = false;
    std::uint8_t request_[768] = {};
    int requestSize_ = 0;
    bool fresh_ = true;
    std::uint32_t startedAt_ = 0, lastAsk_ = 0, lastPermit_ = 0, lastRefresh_ = 0;
    int rounds_ = 0;
    std::vector<std::uint32_t> permits_;
};
