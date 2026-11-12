/* POSIX TLS signaling and bounded Opus relay. One I/O owner per connection. */
#include <deque>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <memory>
#include <array>
#include "curl_ws_io.h"
struct VoiceWsMessage { std::vector<unsigned char> bytes; bool binary; Uint64 at; };
class VoiceWs {
    std::mutex lock;
    std::deque<VoiceWsMessage> in, out;
    SDL_Thread *worker = nullptr;
    std::atomic<bool> stop{false};
    Bt4CurlWs wire = {};
    std::string url, cap, name;
    int seat;
    bool orderedFrames;
    void (*notify)(void*);void *notifyContext;
    static int entry(void *p) { return static_cast<VoiceWs *>(p)->run(); }
    int run() {
        std::string encoded;
        for (unsigned char c : name) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) encoded += c;
            else { char hex[4]; snprintf(hex, sizeof(hex), "%%%02X", c); encoded += hex; }
        }
        curl_slist *headers = nullptr;
        if (!cap.empty()) headers = curl_slist_append(headers, ("Authorization: Bearer " + cap).c_str());
        headers = curl_slist_append(headers, ("X-BT4-Seat: " + std::to_string(seat)).c_str());
        headers = curl_slist_append(headers, ("X-BT4-Name: " + encoded).c_str());
        bool opened;
        { std::lock_guard<std::mutex> g(lock); opened = Bt4CurlWs_Open(&wire, url.c_str(), headers) != 0; }
        int connected = 0;
        while (opened && !stop && connected == 0) {
            connected = Bt4CurlWs_Connect(&wire);
            if (connected == 0 && !Bt4CurlWs_Wait(&wire, 0, 50)) connected = -1;
        }
        if (!opened || connected < 1) { if (!stop) status = -1; }
        else status = 1;
        VoiceWsMessage pending = {};
        std::vector<unsigned char> received;
        bool binary = false, have = false;
        size_t offset = 0;
        while (!stop && status == 1) {
            if (!have) {
                std::lock_guard<std::mutex> g(lock);
                while (!out.empty()) {
                    pending = std::move(out.front()); out.pop_front();
                    if (!orderedFrames && pending.binary && SDL_GetTicks() - pending.at > 160) continue;
                    have = true; offset = 0; break;
                }
            }
            if (have) {
                size_t sent = 0;
                CURLcode result = curl_ws_send(wire.easy, pending.bytes.data() + offset,
                    pending.bytes.size() - offset, &sent, 0, pending.binary ? CURLWS_BINARY : CURLWS_TEXT);
                if (result != CURLE_OK && result != CURLE_AGAIN) { status = -1; break; }
                offset += sent;
                if (offset == pending.bytes.size()) have = false;
            }
            for (int i = 0; i < 32; ++i) {
                unsigned char bytes[4096]; size_t count = 0;
                const curl_ws_frame *meta = nullptr;
                CURLcode result = curl_ws_recv(wire.easy, bytes, sizeof(bytes), &count, &meta);
                if (result == CURLE_AGAIN) break;
                if (result != CURLE_OK || !meta || (meta->flags & CURLWS_CLOSE)) { status = -1; break; }
                if (meta->flags & (CURLWS_PING | CURLWS_PONG)) continue;
                if (received.empty()) binary = (meta->flags & CURLWS_BINARY) != 0;
                if (received.size() + count > 16384) { status = -1; break; }
                received.insert(received.end(), bytes, bytes + count);
                if (meta->bytesleft == 0 && !(meta->flags & CURLWS_CONT)) {
                    std::lock_guard<std::mutex> g(lock);
                    if (in.size() >= (orderedFrames?2048u:64u)) {if(orderedFrames){status=-1;break;}in.pop_front();}
                    in.push_back({std::move(received), binary, SDL_GetTicks()}); received.clear();
                    if(notify)notify(notifyContext);
                }
            }
            // A completed send is not a reason to sleep with more packets
            // already queued. Backpressure still waits for socket readiness.
            bool queued;
            {std::lock_guard<std::mutex> g(lock);queued=!out.empty();}
            if(status==1&&!have&&queued)continue;
            if (status == 1 && !Bt4CurlWs_Wait(&wire, have, 20)) status = -1;
        }
        { std::lock_guard<std::mutex> g(lock); Bt4CurlWs_Close(&wire); }
        curl_slist_free_all(headers);
        return 0;
    }
public:
    std::atomic<int> status{0};
    VoiceWs(const std::string &u, const std::string &c, int s, const std::string &n,bool game=false,void (*wake)(void*)=nullptr,void *context=nullptr) : url(u), cap(c), name(n), seat(s), orderedFrames(game),notify(wake),notifyContext(context) {
        worker = SDL_CreateThread(entry, "BT4 voice TLS", this);
        if (!worker) status = -1;
    }
    ~VoiceWs() {
        stop = true;
        { std::lock_guard<std::mutex> g(lock); Bt4CurlWs_Wake(&wire); }
        if (worker) SDL_WaitThread(worker, nullptr);
    }
    bool read(VoiceWsMessage &message) {
        std::lock_guard<std::mutex> g(lock);
        if (in.empty()) return false;
        message = std::move(in.front()); in.pop_front(); return true;
    }
    void send(const void *data, size_t size, bool binary) {
        if (status < 0 || size > 16384 || !size) return;
        std::lock_guard<std::mutex> g(lock);
        if (binary&&!orderedFrames) {
            unsigned count = 0; for (const auto &message : out) if (message.binary) ++count;
            if (count >= 8) for (auto i = out.begin(); i != out.end(); ++i) if (i->binary) { out.erase(i); break; }
        }
        if (out.size() >= (orderedFrames?2048u:64u)) {if(orderedFrames)status=-1;return;}
        const auto *bytes = static_cast<const unsigned char *>(data);
        out.push_back({std::vector<unsigned char>(bytes, bytes + size), binary, SDL_GetTicks()});
        Bt4CurlWs_Wake(&wire);
    }
    void text(const std::string &value) { send(value.data(), value.size(), false); }
};
