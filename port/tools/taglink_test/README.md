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
