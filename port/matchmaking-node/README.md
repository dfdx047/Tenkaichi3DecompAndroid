# Node matchmaking service

Run `npm install`, then `npm start`. The service listens only on `127.0.0.1:8787` and stores SQLite data at `DB_PATH` (default `./matchmaking.sqlite`). It applies `../matchmaking/schema.sql` on startup and runs the existing `../matchmaking/worker.js` fetch handler without modifying it. No TURN credentials are configured; relay requests return `noRelay`.

Set `BUILD_KEYS`, `REQUIRE_KEY`, `MIN_RELEASE`, `LATEST_RELEASE`, and `ADMIN_KEY` through the process environment as needed. Put Caddy in front for HTTPS and proxy to loopback. Restrict `/etc/dragonnet-matchmaking.env` to `root:root` mode `0600`; set `DB_PATH` to a writable path for user `dragon`.
