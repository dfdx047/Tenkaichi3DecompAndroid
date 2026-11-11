# BT4 runtime and compatibility profile

The native packages target BT4 B14 Rev2 with Spanish (LATINO) and
English data tables. The underlying BT4decomp engine provenance is in
[UPSTREAM_LOCK.json](UPSTREAM_LOCK.json). Source and runtime licenses remain
in the repository and release package.

## Data and language installation

`port/setup/native.cpp` verifies and extracts the chosen compatible ISO.
`language_install.inc` adds the alternate language when both are requested.
`language_packages.h` pins the exact language release assets, sizes and SHA-256
values. Downloads use the official GitHub CLI and the user's authorization.
No account credential is part of the package. Per-entry CRC checks and a hidden
fight run before an alternate installation is marked complete.

`tbo/languages.ini` records Spanish (1), English (2) or both (3). Runtime profile
selection uses `game_profile.c`; only a two-language installation presents the
startup selector. Text and voices switch together.

## Online compatibility and timing

The compatibility family is `BT4-B14-Rev2-sim2`. The base-data check reads the
three verified program resources once per process, independently of the
displayed revision label. 1 vs 1, lobbies, spectators and Tag Team share this identity. The check covers those resources; it does not audit
the full roster or establish compatibility with other BT4 modifications.

Tag Team includes up to ten actor identities and gameplay state in detailed
sync samples. The Worker and client share the room kernel. When supported by
all connected clients, the selected coordinator commits input at 34.2 simulation steps/second
(114% of the original 30 Hz simulation). Missing input is neutral for that seat;
late packets do not rewrite committed history. The Tag Team FPS meter reports
measured ping. Rendering uses the selected resolution scale.

Network-delay feedback applies only to inputs the client actually submitted.
Frames missed during a local stall cannot increase that player's network margin.
R3 establishes an independent WebRTC data channel to the room host before combat.
When every human validates this connection and the same armed snapshot, one native
host runs the shared room kernel at 34.2 simulation steps/second. Authenticated TLS
carries duplicates of that timeline; it does not create a second simulation clock.
The coordinator has no network hop for its own input. Remote players measure the
active path. CPU-only rooms do not acquire network latency.

If preparation fails or a participant uses an older client, the room retains its
Cloudflare Durable Object authority. Direct connections can use TURN where NAT
requires it. A nearby edge does not establish that the original room authority is
nearby: see Cloudflare's [location documentation](https://developers.cloudflare.com/durable-objects/reference/data-location/).
Losing the active coordinator ends the round; live coordinator migration is not
implemented. Network conditions and geographic routes still affect remote players.

## Maps and presentation

BT4 owns native stage IDs 0 through 98. Offline custom stages append at 99,
with up to 26 slots. A same-name PNG supplies native thumbnail and preview
packets; the map name uses glyphs from the selected ISO's original atlas.

## Validation scope

In local validation, four native mixed-language clients reached 1,200 common simulation frames on
the production service without hash mismatches. A normal ES/EN room-code fight
also passed. Both private language downloads, both directions of alternate
language installation and six profile-selection combinations were checked.
These tests used local clients. They did not reproduce the reported four-PC
disconnection and do not cover every story event or roster combination.

## Lobby and menu validation

Lobby protocol 4 provides a per-match relay
through the lobby owner when the assigned fighters cannot connect directly.
Forwarding checks the current match generation, assigned seats, member ID,
source address and recipient. Direct connections remain the first choice.

Native local tests reached combat in a public 1 vs 1 lobby forced through TURN,
and in a five-entrant tournament with the owner spectating and direct traffic
between fighters blocked. The equivalent tournament without the fallback
reproduced the connection failure. Two native Tag Team clients completed the
result-to-character-selection transition and entered a second fight. Keyboard
tests covered the default and rebound Back key; menu captures covered English
and Spanish. A damaged base program was rejected. These checks do not establish
compatibility with every router, modification or remote PC.

## Transport and platform validation

Windows native tests used four clients in one Tag Team room with 170 ms simulated RTT and a shared presentation skip: 900 common frames had matching hashes. An asymmetric slow-link pair also reached 900 matching frames. A public 1 vs 1 lobby forced through TURN reached combat. Character-viewer captures reproduced the sunken model and showed the restored BT4 pose with alternate costumes. The isolated Windows transport worker benchmark reduced p95 queue-to-send dispatch from 16.0641 ms to 0.0749 ms; this measures local scheduling with a send stub, not Internet latency or game FPS. Native Linux and Apple Silicon builds exercise the production WebSocket transports with fragmentation, invalid frames, voice and certificate-rejection fixtures. Actual gameplay on remote Linux and Mac hardware remains a community validation step.

The input-feedback regression reproduces 100 late-frame notices for inputs never submitted: the old margin rose from 1 to 8; the corrected margin stays at 1. This is a scheduling test, not a network RTT measurement. The BT3_NET_LOSS, BT3_NET_LAG and BT3_NET_LATENCY diagnostic variables require BT4_RENDER_TEST=1.

## Native Tag Team coordinator validation

The native and WebAssembly kernels produced 14,000 byte-identical frames across
seven input, loss, reordering, duplicate and disconnection scenarios. A disconnected
seat now clears its queued future input. Two and four real native game processes
in one controlled room reached 900 matching common frames, including a shared
presentation skip. Rematch negotiated a new epoch and entered the second fight.
An incomplete prepare handshake retained the original room authority.

An auxiliary TLS outage lasting 18.4 seconds preserved the same native host while
RTC continued. The relay resumed from retained history and reached 1,800 common
frames with matching gameplay hashes; the Durable Object frame clock stayed off.
A public staging-room test also reached 900 matching frames without injected delay.
A separate public test forced TURN on both peers and selected relay candidates
on both ends: it reached 900 matching common frames with 7-8 ms active-path RTT,
versus 125-127 ms for the original room route. Both processes ran on this PC;
these values do not measure a connection between remote households. Native Linux and macOS packages also exercise the shipped SCTP
SDK with 512-byte unordered messages and the original TLS, voice and audio probes.
Actual gameplay between remote Linux and Mac systems requires community validation.
