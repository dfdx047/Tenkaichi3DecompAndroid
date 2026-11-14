# Dragon Net: the matchmaking service for Android and Switch

The service Dragon Rage (Android) and DBT-NX (Switch) play online through, apart from the PC port's: a new release of
the PC port does not stop it. It is DBT-NX's Worker (rooms, quick match, Ranked, general chat, players online; by
Unknown, MIT) with one change: it serves several **netcode versions** at once and keeps them apart.

## Versions

Every request carries `v`, the game's netcode version. `ALLOWED_VERSIONS` lists the ones this service takes:

| `v` | Games |
|---|---|
| `40001` | DBT-NX before its merge with the PC port's 0.1.37 |
| `5001` | Dragon Net: Dragon Rage 0.8+ and DBT-NX after the merge. `NET_VERSION * 1000 + DRNET_SIM`: the PC port's netcode (5, of 0.1.37) and the simulation version (1) |

Games of different versions never meet: rooms, quick-match queues and Ranked searches only pair the same `v`.
Profiles (Elo, wins, losses), the general chat and the count of players online are shared.

`DRNET_SIM` (`port/src/gs/net_dragon.h`) goes up only when something that decides a fight changes (the PC port's
releases mostly do not: menus, text, video). Then both games move to the new `v` together, and the old one can stay
in `ALLOWED_VERSIONS` until its players have updated.

## The official server

`https://mitigo-rooms.minetivacraft.workers.dev` (the Tenkaichi3DecompAndroidOnline fork's Worker): Dragon Rage and DBT-NX both point there. It needs this
code (below) for Ranked, the chat and the players online, and `ALLOWED_VERSIONS` with `5001`.

## Updating a Worker to this one (Cloudflare dashboard)

1. D1 console of the Worker's database: run the applicable additive migrations once, including
   `ranked-migrate-version.sql` and `ranked-migrate-series.sql` (the latter adds server-owned series IDs and
   idempotent per-match series results; existing profile/rating/match history is retained).
2. Replace the Worker's code with `worker.js`, `ranked.js`, `chat.js`, `presence.js` and the new `versions.js`
   (with wrangler: `wrangler deploy` with `wrangler-ranked.example.jsonc` as a model).
3. Variable `ALLOWED_VERSIONS` = `40001,5001` (the old `ALLOWED_VERSION` is read only when this one is missing).
   `RANKED_ENABLED` = `1`, `CHAT_ENABLED` = `1`, `MIN_RELEASE` = `0`.

A new database: `schema.sql`, `ranked-schema.sql`, `chat-schema.sql`, `presence-schema.sql`.

## Tests

`node test.mjs`, `node ranked-test.mjs`, `node ranked-rematch-test.mjs`, `node ranked-series-local-test.mjs`, `node chat-test.mjs`,
`node presence-test.mjs`, `node dragonnet-test.mjs` (Node 22: `node:sqlite`, no other package).

---

**Resumen (ES):** servidor oficial: `https://mitigo-rooms.minetivacraft.workers.dev` (el Worker del fork Tenkaichi3DecompAndroidOnline); Dragon
Rage y DBT-NX apuntan ahí. Es el servicio de DBT-NX con un cambio: acepta varias versiones de netcode (`ALLOWED_VERSIONS`, p. ej.
`40001,5001`) y nunca empareja versiones distintas (salas, partida rápida y Clasificatoria). Perfiles, Elo, chat y
contador se comparten. Para actualizar: ejecutar `ranked-migrate-version.sql` y `ranked-migrate-series.sql` en D1, subir los cinco `.js` y poner
`ALLOWED_VERSIONS`. La versión `5001` = netcode 5 del PC 0.1.37 + simulación 1; solo cambia cuando cambia algo que
decide la pelea, no con cada versión del PC.
