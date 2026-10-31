#include "link.h"
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr std::uint32_t magic = 0x4E325442; // "BT2N"
constexpr std::uint8_t version = 1;
enum Type : std::uint8_t {hello = 1, hello_ack = 2, ping = 3, pong = 4, bye = 5};
constexpr std::size_t packet_size = 20;
constexpr std::uint32_t hello_every = 100, ping_every = 500, give_up_after = 15000;

void put32(std::uint8_t* p, std::uint32_t v) {for (int i = 0; i < 4; ++i) p[i] = std::uint8_t(v >> (8 * i));}
std::uint32_t get32(const std::uint8_t* p) {return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;}
std::uint32_t fnv(const std::string& text) {
    std::uint32_t h = 2166136261u;
    for (unsigned char c : text) h = (h ^ c) * 16777619u;
    return h;
}
}

std::string Link::text(const Candidate& c) {
    char buffer[INET_ADDRSTRLEN] = "?";
    in_addr ip{};
    ip.s_addr = c.ip;
    ::inet_ntop(AF_INET, &ip, buffer, sizeof buffer);
    return std::string(buffer) + ":" + std::to_string(c.port) + (c.relayed ? " (relay)" : "");
}

std::uint32_t Link::now() {
    return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

Link::~Link() {close();}

bool Link::open(std::uint16_t wanted) {
    close();
    socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) return false;
    ::fcntl(socket_, F_SETFL, ::fcntl(socket_, F_GETFL, 0) | O_NONBLOCK);
    sockaddr_in any{};
    any.sin_family = AF_INET;
    any.sin_addr.s_addr = htonl(INADDR_ANY);
    bool bound = false, fell = false;
    if (wanted) {
        const int yes = 1;
        ::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        any.sin_port = htons(wanted);
        bound = ::bind(socket_, reinterpret_cast<sockaddr*>(&any), sizeof any) == 0;
        if (!bound) {fell = true; any.sin_port = 0;}
    }
    if (!bound && ::bind(socket_, reinterpret_cast<sockaddr*>(&any), sizeof any) != 0) {close(); return false;}
    socklen_t length = sizeof any;
    ::getsockname(socket_, reinterpret_cast<sockaddr*>(&any), &length);
    port_ = ntohs(any.sin_port);
    // this phone's addresses on its networks (not the loopback), each with the socket's port. Wi-Fi and cable first: the other player on the same
    // home network reaches this game by that address, and the service takes only a few; the mobile network's own addresses, the carrier's
    // translation address (192.0.0.x) and link-local ones are of no use to anybody else and are left out.
    local_.clear();
    ifaddrs* list = nullptr;
    if (::getifaddrs(&list) == 0) {
        std::vector<std::string> home, other;
        for (ifaddrs* a = list; a; a = a->ifa_next) {
            if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET || !(a->ifa_flags & IFF_UP) || (a->ifa_flags & IFF_LOOPBACK)) continue;
            const auto* in = reinterpret_cast<sockaddr_in*>(a->ifa_addr);
            const std::uint32_t ip = ntohl(in->sin_addr.s_addr);
            if ((ip >> 24) == 0 || (ip >> 16) == 0xA9FE || (ip >> 8) == 0xC00000) continue;   // 0.x, 169.254.x.x, 192.0.0.x
            char text[INET_ADDRSTRLEN];
            if (!::inet_ntop(AF_INET, &in->sin_addr, text, sizeof text)) continue;
            const std::string name = a->ifa_name ? a->ifa_name : "";
            const bool lan = name.rfind("wlan", 0) == 0 || name.rfind("swlan", 0) == 0 || name.rfind("eth", 0) == 0 || name.rfind("ap", 0) == 0;
            (lan ? home : other).push_back(std::string(text) + ":" + std::to_string(port_));
        }
        ::freeifaddrs(list);
        for (const auto* group : {&home, &other})
            for (const auto& a : *group) if (local_.size() < 3) local_.push_back(a);
    }
    state_ = Open;
    rtt_ = -1;
    peerLeft_ = false;
    sent_ = got_ = foreign_ = 0;
    notes_.clear();
    if (fell) note("online: port " + std::to_string(wanted) + " is not free, " + std::to_string(port_) + " instead (the other side's router knows the old one)");
    openedAt_ = now();
    stun_.begin();
    std::string mine;
    for (const auto& a : local_) mine += " " + a;
    note("online: socket open on port " + std::to_string(port_) + ", own addresses:" + (mine.empty() ? " none" : mine));
    return true;
}

void Link::close() {
    relay_.end(socket_);
    stun_.reset();
    if (socket_ >= 0) ::close(socket_);
    socket_ = -1;
    state_ = Closed;
    candidates_.clear();
    local_.clear();
    rtt_ = -1;
}

std::vector<std::string> Link::localAddresses() const {
    std::vector<std::string> out = stun_.mapped();
    for (const auto& a : local_) out.push_back(a);
    return out;
}

// The router's address is what lets a player on another network reach this game (and lets the service tell a household from strangers), so it
// is worth waiting for: ready when the STUN answers are in, or, with none yet, until 3.5 s have passed (a slow first name lookup on mobile
// data); with one answer already, 1.2 s are enough for the second.
bool Link::addressesReady() const {
    const std::uint32_t waited = now() - openedAt_;
    if (stun_.finished() && !stun_.mapped().empty()) return true;
    if (!stun_.mapped().empty()) return waited > 1200;
    return waited > 3500;
}

void Link::start(const std::string& code, const std::vector<std::string>& addresses) {
    if (state_ != Open) return;
    session_ = fnv(code);
    candidates_.clear();
    for (const auto& a : addresses) {
        const auto colon = a.rfind(':');
        if (colon == std::string::npos) continue;
        in_addr ip{};
        if (a.compare(0, 5, "seen=") == 0) {   // the address the service saw the other player under: the relay lets it through
            if (::inet_pton(AF_INET, a.c_str() + 5, &ip) == 1) relay_.permit(socket_, ip.s_addr);
            continue;
        }
        const std::string text = a.compare(0, 6, "relay=") == 0 ? a.substr(6) : a;   // (the host's relayed address is an ordinary one)
        const auto at = text.rfind(':');
        if (at == std::string::npos) continue;
        const int port = std::atoi(text.c_str() + at + 1);
        if (port <= 0 || port > 65535 || ::inet_pton(AF_INET, text.substr(0, at).c_str(), &ip) != 1) continue;
        candidates_.push_back({ip.s_addr, std::uint16_t(port)});
        relay_.permit(socket_, ip.s_addr);
    }
    started_ = now();
    heard_ = started_;
    lastHello_ = started_ - hello_every;
    lastPing_ = 0;
    number_ = 0;
    lastNote_ = started_;
    state_ = candidates_.empty() ? Failed : Linking;
    std::string to;
    for (const auto& c : candidates_) to += " " + text(c);
    note("online: linking, room " + code + ", " + std::to_string(candidates_.size()) + " address(es) to say hello to:" + (to.empty() ? " none" : to) +
         "; the router looked " + (stun_.symmetric() ? "symmetric" : stun_.mapped().empty() ? "unknown (no STUN answer)" : "cone"));
    if (candidates_.empty()) note("online: FAILED: the other player gave no usable address");
}

void Link::send(std::uint8_t type, std::uint32_t number, std::uint32_t echo, const Candidate& to, const std::uint8_t* payload, std::size_t size) {
    if (socket_ < 0 || size > max_payload) return;
    if (testLoss_ > 0) {
        lossState_ ^= lossState_ << 13; lossState_ ^= lossState_ >> 17; lossState_ ^= lossState_ << 5;
        if (int(lossState_ % 100) < testLoss_) return;
    }
    std::uint8_t p[packet_size + max_payload] = {};
    put32(p, magic);
    p[4] = version;
    p[5] = type;
    put32(p + 8, session_);
    put32(p + 12, number);
    put32(p + 16, echo);
    if (size) std::memcpy(p + packet_size, payload, size);
    const std::size_t total = packet_size + size;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = to.ip;
    a.sin_port = htons(to.port);
    ++sent_;
    if (to.relayed) {relay_.send(socket_, p, total, to.ip, htons(to.port)); return;}
    ::sendto(socket_, p, total, 0, reinterpret_cast<sockaddr*>(&a), sizeof a);
}

bool Link::peerAddress(std::string& out) const {
    if (state_ != Up) return false;
    char text[INET_ADDRSTRLEN];
    in_addr ip{};
    ip.s_addr = remote_.ip;
    if (!::inet_ntop(AF_INET, &ip, text, sizeof text)) return false;
    out = std::string(text) + ":" + std::to_string(remote_.port);
    return true;
}

bool Link::sendMessage(std::uint8_t type, const std::uint8_t* payload, std::size_t size) {
    if (state_ != Up || type < first_message_type || size > max_payload) return false;
    send(type, ++number_, 0, remote_, payload, size);
    return true;
}

bool Link::popMessage(Message& out) {
    if (inbox_.empty()) return false;
    out = std::move(inbox_.front());
    inbox_.pop_front();
    return true;
}

int Link::silentMs() const {return state_ == Up ? static_cast<int>(now() - heard_) : 0;}

void Link::sayBye() {
    if (state_ != Up) return;
    for (int i = 0; i < 3; ++i) send(bye, ++number_, 0, remote_);   // (three, in case one is lost)
}

void Link::poll() {
    if (socket_ >= 0 && relay_.started()) relay_.poll(socket_);
    if (socket_ >= 0) stun_.poll(socket_);
    if (socket_ < 0 || state_ == Closed) return;
    const std::uint32_t t = now();
    if (state_ == Open || state_ == Failed) {
        // (nothing to say yet, or nothing more: answers from the relay are still read below)
    } else if (state_ == Linking) {
        if (t - started_ > giveUp_) {
            state_ = Failed;
            note("online: FAILED after " + std::to_string((t - started_) / 1000) + " s: " + std::to_string(sent_) + " packets sent, " + std::to_string(got_) +
                 " came back from the other game, " + std::to_string(foreign_) + " arrived that were not for this match" +
                 (foreign_ ? " (last from " + text(lastForeign_) + ")" : "") + (got_ == 0 ? "; nothing reached this phone: the other side's router or ours blocks it" : ""));
            return;
        }
        if (t - lastNote_ >= 3000) {
            lastNote_ = t;
            note("online: still linking (" + std::to_string((t - started_) / 1000) + " s): " + std::to_string(sent_) + " sent, " + std::to_string(got_) + " answered, " + std::to_string(foreign_) + " foreign");
        }
        if (t - lastHello_ >= hello_every) {
            lastHello_ = t;
            for (const auto& c : candidates_) send(hello, ++number_, t, c); // (to every address: the one that works answers)
        }
    } else if (t - lastPing_ >= ping_every) {
        lastPing_ = t;
        send(ping, ++number_, t, remote_);
    }
    // what came: at most a few dozen packets a frame
    for (int n = 0; n < 64; ++n) {
        std::uint8_t buffer[1600];
        std::uint8_t* p = buffer;
        sockaddr_in from{};
        socklen_t length = sizeof from;
        ssize_t got = ::recvfrom(socket_, buffer, sizeof buffer, 0, reinterpret_cast<sockaddr*>(&from), &length);
        if (got < 0) break;
        if (stun_.handle(buffer, int(got), from.sin_addr.s_addr, from.sin_port)) continue;
        Candidate source{from.sin_addr.s_addr, ntohs(from.sin_port)};
        if (relay_.isServer(from.sin_addr.s_addr, from.sin_port)) {   // the TURN server: its answers, or the other player's packet inside
            const std::uint8_t* data = nullptr;
            int dataSize = 0;
            std::uint32_t peerIp = 0;
            std::uint16_t peerPort = 0;
            if (!relay_.unwrap(socket_, buffer, int(got), from.sin_addr.s_addr, from.sin_port, &data, &dataSize, &peerIp, &peerPort)) continue;
            p = const_cast<std::uint8_t*>(data);
            got = dataSize;
            source = {peerIp, ntohs(peerPort), true};
        }
        if (state_ != Linking && state_ != Up) continue;
        if (got < static_cast<ssize_t>(packet_size) || get32(p) != magic || p[4] != version || get32(p + 8) != session_) {
            ++foreign_;
            lastForeign_ = source;
            continue;
        }
        ++got_;
        heard_ = t;
        const State before = state_;
        const std::uint32_t echo = get32(p + 16);
        switch (p[5]) {
        case hello:
            send(hello_ack, get32(p + 12), echo, source);
            if (state_ == Linking) {state_ = Up; remote_ = source;}
            break;
        case hello_ack:
            if (state_ == Linking) {state_ = Up; remote_ = source;}
            break;
        case ping:
            send(pong, get32(p + 12), echo, source);
            if (state_ == Linking) {state_ = Up; remote_ = source;} // (the other side is already up and has heard us)
            break;
        case pong: {
            if (state_ == Linking) {state_ = Up; remote_ = source;}
            const int sample = static_cast<int>(t - echo);
            if (sample >= 0 && sample < 10000) rtt_ = rtt_ < 0 ? sample : (rtt_ * 3 + sample) / 4;
            break;
        }
        case bye:
            peerLeft_ = true;
            break;
        default:
            if (p[5] >= first_message_type && got <= static_cast<ssize_t>(packet_size + max_payload)) {
                if (state_ == Linking) {state_ = Up; remote_ = source;}   // (the other side is already talking: it has heard us)
                if (inbox_.size() < 256) inbox_.push_back({p[5], std::vector<std::uint8_t>(p + packet_size, p + got)});
            }
            break;
        }
        if (before == Linking && state_ == Up) note("online: UP via " + text(remote_) + " after " + std::to_string((t - started_)) + " ms, " + std::to_string(sent_) + " sent");
    }
}
