// gs/dragonnet_tag.cpp (TAG TEAM's room service through Dragon Net, BT4) against a stand-in for the HTTP request: a request
// that brings no usable answer (no network, a timeout, a page that is not JSON) is passing trouble and is asked again a
// few times; a refusal the service answers (its {"error"}) is final, as is the end of the tries. sh run.sh
// Simulated: Match_HttpPost (the answers below), and the PC build's Supabase stand-ins; dragonnet_tag.cpp is the real one.
#include <SDL3/SDL.h>
#include <atomic>
#include <string>
extern "C" {
static std::atomic<int> calls{0}, failFirst{0}; static std::string answerText; static char watched[32];
int Match_HttpPost(const char *url, const char *json, char *out, size_t size) {
    char op[48];
    (void)url;
    snprintf(op, sizeof(op), "\"op\":\"%s\"", watched);
    if (!strstr(json, op)) { snprintf(out, size, "{\"events\":[],\"cursor\":0}"); return 1; } // (the room's polls meanwhile)
    int n = ++calls;
    if (n <= failFirst.load()) { out[0] = 0; return 0; } // (nothing came back)
    snprintf(out, size, "%s", answerText.c_str());
    return 1;
}
const char *Match_ServiceUrl(void) { return "https://dragonnet.invalid"; }
// (the PC build's Supabase signaling, not used: BT4_TAG_DRAGONNET is set)
const char *Bt4Signal_Project(void) { return ""; }
int Bt4Signal_Open(void) { return 0; }
void Bt4Signal_Listen(int, int) {}
int Bt4Signal_Command(int, unsigned, const char *, const char *) { return 0; }
int Bt4Signal_Read(int, char *, size_t) { return 0; }
int Bt4Discovery_TagCounts(uint64_t, int out[3]) { out[0] = out[1] = out[2] = 0; return 0; }
}
#include "../../src/gs/dragonnet_tag.cpp"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL " __VA_ARGS__); printf("\n"); } } while (0)

// One request: `fail` answers without anything usable, then `answer`. Returns the reply's body text, "" without one.
static std::string request(const char *op, unsigned serial, int fail, const std::string &answer, Uint64 *took) {
    char buf[30000]; Uint64 t0 = SDL_GetTicks();
    calls = 0; failFirst = fail; answerText = answer; snprintf(watched, sizeof(watched), "%s", op);
    TagSignal_Listen(1, 1);
    if (!TagSignal_Command(1, serial, op, "{\"code\":\"123456\"}")) return "(not queued)";
    while (SDL_GetTicks() - t0 < 30000) {
        int r = TagSignal_Read(1, buf, sizeof(buf));
        if (r > 0) {
            Dj j = Dj::parse(buf, nullptr, false);
            if (j.is_object() && j.value("event", "") == "reply" && j.value("serial", 0u) == serial) { *took = SDL_GetTicks() - t0; return j["body"].dump(); }
            continue;
        }
        struct timespec w = {0, 5000000}; nanosleep(&w, NULL);
    }
    return "";
}

int main() {
    setenv("BT4_TAG_DRAGONNET", "https://dragonnet.invalid", 1);
    Uint64 took = 0;
    std::string ok = "{\"generation\":\"0123abcd-0000-0000-0000-000000000000\",\"self\":2,\"host\":1}";
    std::string r = request("join", 1, 0, ok, &took);
    printf("join, answered at once: %s (%d request(s), %llu ms)\n", r.c_str(), calls.load(), (unsigned long long)took);
    CHECK(r.find("error") == std::string::npos && calls == 1, "a join answered at once");
    r = request("join", 2, 2, ok, &took);
    printf("join, two requests without an answer first: %s (%d request(s), %llu ms)\n", r.c_str(), calls.load(), (unsigned long long)took);
    CHECK(r.find("generation") != std::string::npos && r.find("error") == std::string::npos && calls == 3, "a join after two passing failures");
    r = request("leave", 3, 1, "{}", &took);
    printf("leave, one request without an answer first: %s (%d request(s))\n", r.c_str(), calls.load());
    CHECK(r.find("error") == std::string::npos && calls == 2, "a leave after a passing failure");
    r = request("join", 4, 100, ok, &took);
    printf("join, the service never answers: %s (%d request(s), %llu ms)\n", r.c_str(), calls.load(), (unsigned long long)took);
    CHECK(r.find("SIGNALING_UNAVAILABLE") != std::string::npos && calls >= 2 && calls <= 5 && took < 20000, "a join the service never answers ends, after a few tries");
    r = request("join", 5, 0, "{\"error\":\"no room\"}", &took);
    printf("join, a room that does not exist: %s (%d request(s))\n", r.c_str(), calls.load());
    CHECK(r.find("error") != std::string::npos && calls == 1, "a refusal is final: not asked again");
    printf(failures ? "%d failure(s)\n" : "all passed\n", failures);
    fflush(stdout);
    _Exit(failures ? 1 : 0); // (the service's thread is left running: the process ends with it)
}
