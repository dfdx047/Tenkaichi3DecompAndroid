#include <cstdio>
#include <cstring>
#include "production_voice_ws.inc"

static int check(unsigned backpressure) {
    testClock=testSent=testSendAttempts=testReceives=testWaits=testWritableWaits=0;
    testExpected=200;
    testAgain=backpressure;
    VoiceWs ws("https://example.invalid", "test-capability", 0, "test", true);
    ws.status=1;
    for (unsigned i=0; i<testExpected; ++i) {
        unsigned char packet[528]={};
        memcpy(packet, &i, sizeof(i));
        ws.send(packet, sizeof(packet), true);
    }
    ws.run();
    bool okay=testSent==testExpected && ws.out.empty() &&
        testSendAttempts==testExpected+backpressure && testReceives==testSendAttempts &&
        testWaits==backpressure && testWritableWaits==backpressure &&
        testClock==backpressure*20;
    printf("backpressure=%u sent=%u attempts=%u rx-polls=%u waits=%u writable-waits=%u synthetic-ms=%llu result=%s\n",
        backpressure,testSent,testSendAttempts,testReceives,testWaits,testWritableWaits,
        (unsigned long long)testClock,okay?"PASS":"FAIL");
    return okay?0:1;
}

int main() { int failed=check(0); failed+=check(7); return failed; }
