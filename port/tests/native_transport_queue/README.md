Run `python port/tests/native_transport_queue/run.py --cxx /path/to/c++`.

The fixture compiles the production POSIX `VoiceWs` implementation with a fake
socket and monotonic clock. It sends 200 distinct queued 528-byte game packets
with no incoming messages. It requires no wait while the socket accepts data,
and exactly one writable-socket wait for each of seven simulated `CURLE_AGAIN`
retries. The receive path runs between sends. Packet order, payload preservation,
queue exhaustion, retry count and wait count are checked. Execution has a
three-second wall-time limit to catch accidental spins.

This is a queue scheduling regression test. It does not measure physical network
latency, TLS fragmentation, RTC traffic or operating-system socket behavior.
