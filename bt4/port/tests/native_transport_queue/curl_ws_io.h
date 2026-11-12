/* Deterministic socket/clock seam for the production VoiceWs worker. */
#include <cstdint>
using Uint64 = uint64_t;
struct SDL_Thread {};
static Uint64 testClock;
static unsigned testSent, testSendAttempts, testReceives, testWaits, testWritableWaits;
static unsigned testAgain, testExpected;
static Uint64 SDL_GetTicks() { return testClock; }
static SDL_Thread *SDL_CreateThread(int (*)(void *), const char *, void *) { return (SDL_Thread *)1; }
static void SDL_WaitThread(SDL_Thread *, void *) {}
struct curl_slist {};
struct Bt4CurlWs { void *easy = nullptr; };
static curl_slist *curl_slist_append(curl_slist *, const char *) { return (curl_slist *)1; }
static void curl_slist_free_all(curl_slist *) {}
static int Bt4CurlWs_Open(Bt4CurlWs *, const char *, curl_slist *) { return 1; }
static int Bt4CurlWs_Connect(Bt4CurlWs *) { return 1; }
static int Bt4CurlWs_Wait(Bt4CurlWs *, int writable, int timeout) {
    ++testWaits;
    if (writable) ++testWritableWaits;
    testClock += timeout;
    return 1;
}
static void Bt4CurlWs_Wake(Bt4CurlWs *) {}
static void Bt4CurlWs_Close(Bt4CurlWs *) {}
enum CURLcode { CURLE_OK, CURLE_AGAIN, CURLE_FAILED };
enum { CURLWS_BINARY=1, CURLWS_TEXT=2, CURLWS_CLOSE=4, CURLWS_PING=8, CURLWS_PONG=16, CURLWS_CONT=32 };
struct curl_ws_frame { int flags; size_t bytesleft; };
static CURLcode curl_ws_send(void *, const void *data, size_t size, size_t *sent, int, int) {
    ++testSendAttempts;
    if (testAgain) { --testAgain; *sent=0; return CURLE_AGAIN; }
    // Every packet is distinct. A retry must preserve its original payload.
    unsigned sequence=0;
    if (size != 528) return CURLE_FAILED;
    memcpy(&sequence, data, sizeof(sequence));
    if (sequence != testSent) return CURLE_FAILED;
    ++testSent;
    *sent=size;
    return CURLE_OK;
}
static CURLcode curl_ws_recv(void *, void *, size_t, size_t *, const curl_ws_frame **) {
    ++testReceives;
    // Stop the real worker only after all queued application packets arrived.
    return testSent == testExpected ? CURLE_FAILED : CURLE_AGAIN;
}
