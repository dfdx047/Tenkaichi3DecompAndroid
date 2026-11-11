# BT4 room and voice service

The existing Worker handles room control, game-link negotiation and voice.
It shares the room state machine with the native client.

| File | Responsibility |
| --- | --- |
| `worker.mjs` | Routes, connection rate limits and Durable Object bindings |
| `session.mjs` | Membership, load barriers, synchronization and legacy frame coordination |
| `tag_link.mjs` | Authenticated WebRTC signaling, single-host negotiation and TLS backup history |
| `voice.mjs` | Voice signaling, bounded Opus relay and idle duel-room hibernation |
| `kernel.c`, `kernel.wasm` | C room authority compiled for WebAssembly |

The native authority compiles the same C kernel. Shared room and wire definitions
are in `port/src/tag_team.c` and `port/include/tag_team_wire.h`.

## Deployment

Copy `wrangler.example.json` to a private deployment configuration and set your
own account, D1 database and Durable Object bindings. Run the idempotent schema
before deploying. Account credentials and TURN tokens belong in Worker secrets,
never in this repository.

```sh
python cloudflare/build_kernel.py
npx wrangler d1 execute YOUR_DATABASE --remote --file cloudflare/schema.sql
npx wrangler deploy --config path/to/your/private-wrangler.json
```

## Game transport

Tag Team supports ten seats, load barriers and detailed state comparisons.
Compatible clients negotiate a peer game path before the round. A unanimous
snapshot handshake selects one native host authority; the Durable Object then
stops its frame clock. RTC and authenticated TLS transport the same timeline.
TLS retains bounded frame history for recovery without changing the host or epoch.

When negotiation is unavailable, the room retains the server frame path.
Missing input affects that seat in committed history; late input cannot rewrite
past frames. D1 stores service metadata and rate counters, not one write per frame.

Voice signaling is separate from the fight kernel. TURN credentials are issued
temporarily using Worker secrets. Proximity attenuation is computed on each
client from fighter positions and applies across both teams. This deployment
supports RTC media with TURN traversal and a TLS Opus relay fallback; it does
not use an SFU. Idle duel voice sockets use the WebSocket Hibernation API and
an automatic heartbeat response. Tag Team retains its live room state and
recovery history while a round is active.

## Connection lifecycle and service limits

Room maintenance retires unjoined sockets and connections that stop sending
traffic. Active clients send heartbeats; a quiet lobby with live heartbeats
remains connected. Room shutdown closes game, auxiliary and voice sockets.

Cloudflare meters Durable Object duration separately from network bandwidth.
Hibernation can reduce eligible idle duration; it does not restore an already
exhausted account quota. See [Cloudflare's pricing and limits](https://developers.cloudflare.com/durable-objects/platform/pricing/).

## Validation

```sh
node --test cloudflare/tests/tag_link.test.mjs cloudflare/tests/voice_hibernation.test.mjs
```

Recompile the kernel and validate client/server protocol behavior together
after changing the shared room state machine. Deployment is not a side effect
of a source push or repository checks.
