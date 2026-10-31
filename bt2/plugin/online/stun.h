#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// What the other player must be told to reach this game from another network: the address the home's router shows to the internet.
// A STUN server (public and free: no account) answers a request with "you appear to me as ip:port". The request goes out of the
// line's own socket, so the answer is the address and port the router gave THAT socket, the one the other game will send to.
//
// Two servers are asked. The same port from both means the router keeps one port for the socket whoever is on the other side
// (a "cone" router: the hole punch works). Different ports mean one port per destination ("symmetric": the other game cannot
// know the port in advance, and only a relay will do).
class Stun {
public:
    void begin();                                        // the names are looked up on threads of their own, one each, which nobody waits for
    void reset();
    void poll(int socket);                               // sends the requests that are due
    // A packet that came in: true when it was a STUN answer (consumed).
    bool handle(const std::uint8_t* packet, int size, std::uint32_t fromIp, std::uint16_t fromPort);
    bool finished() const;                               // every server answered, or the tries are used up
    std::vector<std::string> mapped() const;             // the distinct "ip:port" seen, in the order they came
    bool symmetric() const;                              // two answers with different ports

private:
    // A name lookup that takes long (no internet, a slow resolver) must never hold the game up: the lookup belongs to its own thread, which
    // keeps the server's record alive by itself and writes into it; the game only reads the records whose lookup is done.
    struct Server {
        std::string host, port;
        std::atomic<bool> ready{false};                  // the lookup is over (ip 0: the name did not resolve)
        std::uint32_t ip = 0;
        std::uint16_t portNet = 0;
        std::uint8_t transaction[12] = {};
        bool answered = false;
        std::uint32_t mappedIp = 0;
        std::uint16_t mappedPort = 0;
    };
    std::vector<std::shared_ptr<Server>> servers_;
    bool began_ = false;
    int rounds_ = 0;
    std::uint32_t lastAsk_ = 0;
};
