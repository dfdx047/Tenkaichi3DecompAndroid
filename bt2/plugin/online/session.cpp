#include "session.h"
#include <cstring>

namespace {
void put32(std::uint8_t* p, std::uint32_t v) {for (int i = 0; i < 4; ++i) p[i] = std::uint8_t(v >> (8 * i));}
std::uint32_t get32(const std::uint8_t* p) {return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;}
constexpr std::size_t config_size = 16, ack_size = 8, arrived_size = 12;
}

std::uint32_t SessionControl::sum(const std::uint8_t* p, std::size_t n) {
    std::uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

void SessionControl::begin(bool host, const Config& offer) {
    *this = SessionControl{};
    begun_ = true;
    host_ = host;
    if (host) cfg_ = offer;
}

void SessionControl::setArrived(std::uint32_t field) {
    if (!begun_ || arrived_) return;
    arrived_ = true;
    myField_ = field;
}

int SessionControl::next(std::uint32_t now, std::uint8_t& type, std::uint8_t* out, std::size_t cap) {
    if (!begun_ || cap < config_size) return -1;
    if (ackDue_) {                                       // (the other side offered its settings: this is the answer, at once)
        ackDue_ = false;
        std::uint8_t c[12];
        put32(c, version); c[4] = cfg_.delay; c[5] = cfg_.hostIsP1; c[6] = c[7] = 0; put32(c + 8, cfg_.seed);
        type = tConfigAck;
        put32(out, version);
        put32(out + 4, sum(c, sizeof c));
        return int(ack_size);
    }
    if (replyDue_) {
        replyDue_ = false;
        type = tArrived;
        put32(out, myField_); out[4] = 1; out[5] = out[6] = out[7] = 0;
        put32(out + 8, sum(out, 8));
        return int(arrived_size);
    }
    if (host_ && !acked_ && (!sentConfig_ || now - lastConfig_ >= every)) {
        sentConfig_ = true;
        lastConfig_ = now;
        type = tConfig;
        put32(out, version); out[4] = cfg_.delay; out[5] = cfg_.hostIsP1; out[6] = out[7] = 0; put32(out + 8, cfg_.seed);
        put32(out + 12, sum(out, 12));
        return int(config_size);
    }
    if (arrived_ && !peerArrived_ && (!sentArrived_ || now - lastArrived_ >= every)) {
        sentArrived_ = true;
        lastArrived_ = now;
        type = tArrived;
        put32(out, myField_); out[4] = 0; out[5] = out[6] = out[7] = 0;
        put32(out + 8, sum(out, 8));
        return int(arrived_size);
    }
    return -1;
}

bool SessionControl::onMessage(std::uint8_t t, const std::uint8_t* p, std::size_t n) {
    if (!begun_) return false;
    if (t == tConfig && n == config_size && !host_) {
        if (get32(p + 12) != sum(p, 12)) return false;
        if (get32(p) != version) {versionBad_ = true; return true;}
        cfg_.delay = p[4];
        cfg_.hostIsP1 = p[5] != 0;
        cfg_.seed = get32(p + 8);
        haveConfig_ = true;
        ackDue_ = true;                                   // (said again at every repeat: the host stops when one gets through)
        return true;
    }
    if (t == tConfigAck && n == ack_size && host_) {
        std::uint8_t c[12];
        put32(c, version); c[4] = cfg_.delay; c[5] = cfg_.hostIsP1; c[6] = c[7] = 0; put32(c + 8, cfg_.seed);
        if (get32(p) != version) {versionBad_ = true; return true;}
        if (get32(p + 4) != sum(c, sizeof c)) return false;
        acked_ = true;
        return true;
    }
    if (t == tArrived && n == arrived_size) {
        if (get32(p + 8) != sum(p, 8)) return false;
        peerField_ = get32(p);
        peerArrived_ = true;
        if (arrived_ && p[4] == 0) replyDue_ = true;      // (it is still waiting: tell it, it does not know this side is here)
        return true;
    }
    return false;
}
