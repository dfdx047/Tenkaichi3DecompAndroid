#include "relay.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <netdb.h>
#include <random>
#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr std::uint8_t cookie[4] = {0x21, 0x12, 0xA4, 0x42};
constexpr std::uint32_t ask_every = 700, permit_every = 60000, refresh_every = 120000;
constexpr int give_up_rounds = 12;

std::uint32_t nowMs() {
    return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
void put16(std::uint8_t* p, unsigned v) {p[0] = std::uint8_t(v >> 8); p[1] = std::uint8_t(v);}
unsigned get16(const std::uint8_t* p) {return unsigned(p[0]) << 8 | p[1];}
std::uint32_t rol(std::uint32_t x, int n) {return x << n | x >> (32 - n);}

// MD5 (the login's key) and SHA-1 (the messages' integrity), for short inputs only.
void md5(const std::uint8_t* msg, std::size_t length, std::uint8_t out[16]) {
    static const std::uint8_t s[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                                       4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    static const std::uint32_t k[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    std::uint32_t h[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
    std::uint8_t buf[256 + 72] = {};
    if (length > 256) length = 256;
    std::memcpy(buf, msg, length);
    buf[length] = 0x80;
    const std::size_t total = (length + 9 + 63) & ~std::size_t(63);
    for (int i = 0; i < 8; ++i) buf[total - 8 + i] = std::uint8_t(std::uint64_t(length) * 8 >> (8 * i));
    for (std::size_t off = 0; off < total; off += 64) {
        std::uint32_t w[16], a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 16; ++i) w[i] = std::uint32_t(buf[off + i * 4]) | std::uint32_t(buf[off + i * 4 + 1]) << 8 | std::uint32_t(buf[off + i * 4 + 2]) << 16 | std::uint32_t(buf[off + i * 4 + 3]) << 24;
        for (int i = 0; i < 64; ++i) {
            std::uint32_t f;
            int g;
            if (i < 16) {f = (b & c) | (~b & d); g = i;}
            else if (i < 32) {f = (d & b) | (~d & c); g = (5 * i + 1) & 15;}
            else if (i < 48) {f = b ^ c ^ d; g = (3 * i + 5) & 15;}
            else {f = c ^ (b | ~d); g = (7 * i) & 15;}
            const std::uint32_t t = d;
            d = c;
            c = b;
            b = b + rol(a + f + k[i] + w[g], s[i]);
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    for (int i = 0; i < 16; ++i) out[i] = std::uint8_t(h[i / 4] >> (8 * (i & 3)));
}

void sha1(const std::uint8_t* msg, std::size_t length, std::uint8_t out[20]) {
    std::uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    std::vector<std::uint8_t> buf(msg, msg + length);
    buf.push_back(0x80);
    while (buf.size() % 64 != 56) buf.push_back(0);
    for (int i = 7; i >= 0; --i) buf.push_back(std::uint8_t((std::uint64_t(length) * 8) >> (8 * i)));
    for (std::size_t off = 0; off < buf.size(); off += 64) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i) w[i] = std::uint32_t(buf[off + i * 4]) << 24 | std::uint32_t(buf[off + i * 4 + 1]) << 16 | std::uint32_t(buf[off + i * 4 + 2]) << 8 | buf[off + i * 4 + 3];
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            std::uint32_t f, kk;
            if (i < 20) {f = (b & c) | (~b & d); kk = 0x5A827999u;}
            else if (i < 40) {f = b ^ c ^ d; kk = 0x6ED9EBA1u;}
            else if (i < 60) {f = (b & c) | (b & d) | (c & d); kk = 0x8F1BBCDCu;}
            else {f = b ^ c ^ d; kk = 0xCA62C1D6u;}
            const std::uint32_t t = rol(a, 5) + f + e + kk + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    for (int i = 0; i < 20; ++i) out[i] = std::uint8_t(h[i / 4] >> (24 - 8 * (i & 3)));
}

void hmacSha1(const std::uint8_t* key, std::size_t keyLength, const std::uint8_t* msg, std::size_t length, std::uint8_t out[20]) {
    std::uint8_t k[64] = {};
    std::memcpy(k, key, keyLength > 64 ? 64 : keyLength);
    std::vector<std::uint8_t> inner(64 + length), outer(64 + 20);
    for (int i = 0; i < 64; ++i) {inner[i] = k[i] ^ 0x36; outer[i] = k[i] ^ 0x5C;}
    std::memcpy(inner.data() + 64, msg, length);
    sha1(inner.data(), inner.size(), outer.data() + 64);
    sha1(outer.data(), outer.size(), out);
}

const std::uint8_t* find(const std::uint8_t* m, int size, unsigned type, int* length) {
    for (int at = 20; at + 4 <= size;) {
        const unsigned t = get16(m + at);
        const int l = int(get16(m + at + 2));
        if (at + 4 + l > size) break;
        if (t == type) {*length = l; return m + at + 4;}
        at += 4 + ((l + 3) & ~3);
    }
    return nullptr;
}

// An XOR-MAPPED style address (IPv4): ip and port stay in network order, as the sockets hold them.
bool xorAddress(const std::uint8_t* v, int length, std::uint32_t* ip, std::uint16_t* port) {
    if (!v || length < 8 || v[1] != 1) return false;
    std::uint8_t b[4] = {std::uint8_t(v[4] ^ cookie[0]), std::uint8_t(v[5] ^ cookie[1]), std::uint8_t(v[6] ^ cookie[2]), std::uint8_t(v[7] ^ cookie[3])};
    std::memcpy(ip, b, 4);
    const std::uint8_t p[2] = {std::uint8_t(v[2] ^ cookie[0]), std::uint8_t(v[3] ^ cookie[1])};
    std::memcpy(port, p, 2);
    return true;
}

int errorCode(const std::uint8_t* m, int size) {
    int l = 0;
    const std::uint8_t* e = find(m, size, 0x0009, &l);
    return e && l >= 4 ? (e[2] & 7) * 100 + e[3] : 0;
}
}

Relay::~Relay() {if (resolver_.joinable()) resolver_.join();}

void Relay::begin(const std::string& server, const std::string& username, const std::string& credential) {
    if (state_ != Idle) return;
    const auto colon = server.rfind(':');
    if (colon == std::string::npos) {state_ = Failed; return;}
    host_ = server.substr(0, colon);
    port_ = server.substr(colon + 1);
    username_ = username;
    credential_ = credential;
    state_ = Resolving;
    startedAt_ = nowMs();
    resolver_ = std::thread([this] {
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (::getaddrinfo(host_.c_str(), port_.c_str(), &hints, &res) == 0 && res) {
            const auto* a = reinterpret_cast<sockaddr_in*>(res->ai_addr);
            serverIp_ = a->sin_addr.s_addr;
            serverPort_ = a->sin_port;
            resolvedFlag_ = 1;
        } else resolvedFlag_ = -1;
        if (res) ::freeaddrinfo(res);
    });
}

void Relay::end(int socket) {
    if (state_ == Active && socket >= 0) {
        std::uint8_t m[768];
        int at = message(m, 0x0004);
        const std::uint8_t life[4] = {0, 0, 0, 0};
        at = attribute(m, at, 0x000D, life, 4);
        at = sign(m, at);
        toServer(socket, m, at);
    }
    if (resolver_.joinable()) resolver_.join();
    state_ = Idle;
    resolved_ = false;
    resolvedFlag_ = 0;
    haveLogin_ = false;
    realm_.clear(); nonce_.clear(); relayed_.clear();
    permits_.clear();
    fresh_ = true;
    rounds_ = 0;
}

int Relay::message(std::uint8_t* m, unsigned type) {
    static std::mt19937 random{std::random_device{}() ^ nowMs()};
    put16(m, type);
    put16(m + 2, 0);
    std::memcpy(m + 4, cookie, 4);
    for (int i = 8; i < 20; ++i) m[i] = std::uint8_t(random());
    return 20;
}

int Relay::attribute(std::uint8_t* m, int at, unsigned type, const void* value, int length) {
    put16(m + at, type);
    put16(m + at + 2, unsigned(length));
    std::memcpy(m + at + 4, value, std::size_t(length));
    std::memset(m + at + 4 + length, 0, std::size_t((4 - (length & 3)) & 3));
    return at + 4 + ((length + 3) & ~3);
}

int Relay::address(std::uint8_t* m, int at, unsigned type, std::uint32_t ip, std::uint16_t port) {
    std::uint8_t ipb[4], pb[2], v[8] = {0, 1};
    std::memcpy(ipb, &ip, 4);
    std::memcpy(pb, &port, 2);
    v[2] = pb[0] ^ cookie[0]; v[3] = pb[1] ^ cookie[1];
    for (int i = 0; i < 4; ++i) v[4 + i] = ipb[i] ^ cookie[i];
    return attribute(m, at, type, v, 8);
}

int Relay::sign(std::uint8_t* m, int at) {
    if (haveLogin_) {
        at = attribute(m, at, 0x0006, username_.data(), int(username_.size()));
        at = attribute(m, at, 0x0014, realm_.data(), int(realm_.size()));
        at = attribute(m, at, 0x0015, nonce_.data(), int(nonce_.size()));
        put16(m + 2, unsigned(at - 20 + 24));          // (the length as it will be with the integrity attribute)
        std::uint8_t mac[20];
        hmacSha1(key_, 16, m, std::size_t(at), mac);
        at = attribute(m, at, 0x0008, mac, 20);
    }
    put16(m + 2, unsigned(at - 20));
    return at;
}

void Relay::toServer(int socket, const std::uint8_t* m, int length) {
    if (!resolved_ || socket < 0) return;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = serverIp_;
    a.sin_port = serverPort_;
    ::sendto(socket, m, std::size_t(length), 0, reinterpret_cast<sockaddr*>(&a), sizeof a);
}

void Relay::takeLogin(const std::uint8_t* m, int size) {
    int rl = 0, nl = 0;
    const std::uint8_t* realm = find(m, size, 0x0014, &rl);
    const std::uint8_t* nonce = find(m, size, 0x0015, &nl);
    if (nonce && nl < 256) nonce_.assign(reinterpret_cast<const char*>(nonce), std::size_t(nl));
    if (realm && rl < 128) realm_.assign(reinterpret_cast<const char*>(realm), std::size_t(rl));
    if (!realm_.empty() && !nonce_.empty()) {
        const std::string text = username_ + ":" + realm_ + ":" + credential_;
        md5(reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), key_);
        haveLogin_ = true;
    }
}

void Relay::ask(int socket) {
    if (fresh_) {   // (a request that got no answer is sent again as it was, with the same transaction number)
        const std::uint8_t udp[4] = {17, 0, 0, 0};
        requestSize_ = message(request_, 0x0003);
        requestSize_ = attribute(request_, requestSize_, 0x0019, udp, 4);
        requestSize_ = sign(request_, requestSize_);
        fresh_ = false;
    }
    toServer(socket, request_, requestSize_);
    lastAsk_ = nowMs();
    ++rounds_;
}

void Relay::sendPermit(int socket, std::uint32_t ip) {
    std::uint8_t m[768];
    int at = message(m, 0x0008);
    at = address(m, at, 0x0012, ip, 0);
    at = sign(m, at);
    toServer(socket, m, at);
}

void Relay::permit(int socket, std::uint32_t ip) {
    for (auto p : permits_) if (p == ip) return;
    if (permits_.size() >= 8) return;
    permits_.push_back(ip);
    if (state_ == Active) sendPermit(socket, ip);
}

void Relay::poll(int socket) {
    const std::uint32_t t = nowMs();
    if (state_ == Resolving) {
        if (resolvedFlag_ == 1) {resolved_ = true; state_ = Asking; rounds_ = 0; fresh_ = true; ask(socket);}
        else if (resolvedFlag_ < 0 || t - startedAt_ > 8000) state_ = Failed;
    } else if (state_ == Asking) {
        if (rounds_ > give_up_rounds) state_ = Failed;
        else if (t - lastAsk_ >= ask_every) ask(socket);
    } else if (state_ == Active) {
        if (t - lastPermit_ > permit_every) {
            lastPermit_ = t;
            for (auto p : permits_) sendPermit(socket, p);
        }
        if (t - lastRefresh_ > refresh_every) {
            const std::uint8_t life[4] = {0, 0, 0x02, 0x58};   // 600 seconds
            std::uint8_t m[768];
            int at = message(m, 0x0004);
            lastRefresh_ = t;
            at = attribute(m, at, 0x000D, life, 4);
            at = sign(m, at);
            toServer(socket, m, at);
        }
    }
}

bool Relay::unwrap(int socket, const std::uint8_t* p, int size, std::uint32_t, std::uint16_t, const std::uint8_t** data, int* length,
                   std::uint32_t* peerIp, std::uint16_t* peerPort) {
    if (size < 20) return false;
    const unsigned type = get16(p);
    if (state_ == Asking) {
        if (std::memcmp(p + 8, request_ + 8, 12) != 0) return false;
        if (type == 0x0103) {
            int l = 0;
            const std::uint8_t* v = find(p, size, 0x0016, &l);
            std::uint32_t ip;
            std::uint16_t port;
            char text[INET_ADDRSTRLEN];
            if (!xorAddress(v, l, &ip, &port) || !::inet_ntop(AF_INET, &ip, text, sizeof text)) {state_ = Failed; return false;}
            relayed_ = std::string(text) + ":" + std::to_string(ntohs(port));
            state_ = Active;
            lastRefresh_ = lastPermit_ = nowMs();
            for (auto a : permits_) sendPermit(socket, a);
        } else if (type == 0x0113) {
            const int code = errorCode(p, size);
            if (code == 401 || code == 438) {takeLogin(p, size); fresh_ = true; ask(socket);}
            else if (code == 437 && haveLogin_) {   // an address is held already (an earlier run of this port): given up, asked anew
                const std::uint8_t life[4] = {0, 0, 0, 0};
                std::uint8_t m[768];
                int at = message(m, 0x0004);
                at = attribute(m, at, 0x000D, life, 4);
                at = sign(m, at);
                toServer(socket, m, at);
                fresh_ = true;
            } else state_ = Failed;
        }
        return false;
    }
    if (state_ != Active) return false;
    if (type == 0x0017) {   // a data indication: the other player's packet
        int l = 0;
        const std::uint8_t* a = find(p, size, 0x0012, &l);
        if (!xorAddress(a, l, peerIp, peerPort)) return false;
        const std::uint8_t* d = find(p, size, 0x0013, &l);
        if (!d) return false;
        *data = d;
        *length = l;
        return true;
    }
    if ((type & 0x0110) == 0x0110) {   // an error answer: a stale nonce brings a new one, the next poll asks again
        const int code = errorCode(p, size);
        if (code == 438 || code == 401) {takeLogin(p, size); lastPermit_ = lastRefresh_ = 0;}
    }
    return false;
}

void Relay::send(int socket, const void* data, int length, std::uint32_t ip, std::uint16_t port) {
    if (state_ != Active || length > 1400) return;
    std::uint8_t m[1600];
    int at = message(m, 0x0016);
    at = address(m, at, 0x0012, ip, port);
    at = attribute(m, at, 0x0013, data, length);
    put16(m + 2, unsigned(at - 20));
    toServer(socket, m, at);
}
