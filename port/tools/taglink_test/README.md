# tag_link.c on a PC, without phones

`server.mjs` runs the Tag Team Online Worker (port/tagnet-worker) on http://127.0.0.1:8787 with a SQLite stand-in for D1;
`harness.c` includes port/src/gs/tag_link.c with stand-ins for the network service, STUN and the TURN relay, and plays one
room's owner (`host`) and a player (`join <code>`): 300 KB sent through the reliable stream and echoed back, `DROP=20`
throwing away a fifth of the packets.

    node server.mjs ../../tagnet-worker &
    cc -O1 -w -I<SDL3 include> -I../../src/gs harness.c <a desktop libSDL3.so> -o harness
    ./harness host            # prints CODE nnnnnn
    DROP=20 ./harness join nnnnnn

(A desktop SDL3: e.g. a copy of the Flatpak runtime's libSDL3.so.0 beside the program, linked with -Wl,-rpath,'$ORIGIN';
not the runtime's whole folder, its libc does not match.)

A way going mute in the middle (the "Input from a player stopped arriving" of 2026-11-02): `relay.mjs` stands in for
the room's relay (the Worker's Durable Object) over UDP, and `CUT_MS` makes one side's direct way lose everything from
that time on; `PACE` (bytes per ms) spreads the stream over a few seconds so the cut falls in its middle.

    LATE=600 node relay.mjs 8790 &                    # (LATE: the relay passes nothing for 600 ms, so they connect directly first)
    TAGLINK_TEST_RELAY=8790 ./harness host
    TAGLINK_TEST_RELAY=8790 PACE=100 CUT_MS=1000 ./harness join nnnnnn

Start the relay first and wait for its "relay up" (a game whose first message went nowhere is unknown to it).
