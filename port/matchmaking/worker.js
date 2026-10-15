// Matchmaking for Tenkaichi3Decomp's online play: room codes.
//
// What it does: lets two players find each other without typing addresses. The host makes a room and gets a short
// code; the other player joins with the code; each learns the addresses the other can be reached at, and the two
// games then connect to each other directly. No game data passes through here.
//
// What it stores (a D1 database bound as DB, table `rooms`, see schema.sql), for at most ten minutes per room: the
// code, the two player names, the addresses each game reported, and the host's address as Cloudflare saw it.
//
// Requests are JSON over POST:
//   /v1/host   {v, name, addrs}        -> {code, key, you, turn?}
//   /v1/addrs  {code, key, addrs}      -> {}       (the host, once it has more addresses: its relay address)
//   /v1/join   {v, code, name, addrs}  -> {key, you, peer: {name, addrs}}
//   /v1/poll   {code, key}             -> {peer: {name, addrs} | null}     (the host asks until someone has joined)
//   /v1/leave  {code, key}             -> {}
//   /v1/relay  {code, key}             -> {turn} | {noRelay}   (the host, once someone has joined and the two did not
//       reach each other directly: a relay login, asked for only then. A game that sends `rel` >= 129 gets none
//       with its room; an older one does, as before. The one who joined learns the relay address by its poll.)
//   /v1/quick  {v, name, addrs, queue, who, relay?}  -> {role: "join", code, key, you, peer} | {role: "host", code, key, you, turn?}
//       quick match: the caller is put with whoever waits longest in `queue` ("S" single, "T" team, "D10" / "D15" /
//       "D20" DP battle), or, if nobody waits, is given a room of its own in that queue and waits there as a host
//       does (poll, addrs, leave as for any room). `who` names the caller's game (a random text it made up), so that
//       a room it left behind is not the one it is given. relay: false = it holds a relay address already.
//   /v1/queues {v}                     -> {waiting: {S: n, T: n, D10: n, D15: n, D20: n}}
//   /v1/hello  {rel}                   -> {min, latest, now}   (the variables MIN_RELEASE and LATEST_RELEASE, 0 if not
//       set; `now` is this Worker's clock, seconds since 1970, for the build key's time below)
// `turn` (only if the Worker has a TURN key: the secrets TURN_KEY_ID and TURN_KEY_TOKEN) is what the host needs to
// get a relay address from Cloudflare's TURN service: {server, username, credential}, good for three hours. The
// host publishes that address as "relay=ip:port" among its addresses; the other player's addresses carry
// "seen=ip", the address this Worker saw them under (the relay must be told whom to let through).
// `v` is the game's netcode version, `addrs` a list of "ip:port" strings (what the game found out about itself),
// `you` the caller's address as seen here (the game uses it when it could not find out its own). Errors come back
// as {error: "..."} with a 4xx status: "version" (the two games differ), "no room", "full", "busy", "bad request",
// "old" (the Worker's keeper lets only newer releases in: `rel` in the request, the variable MIN_RELEASE here),
// "build key" (status 403, with `why` and `now`: only official releases are let in, see below).
//
// The build key. An official release of the game is built with a key of its own and signs every request with it,
// in three headers:
//   X-BT3-Release: 134             the release (as `rel`)
//   X-BT3-Time:    1791900000      seconds since 1970
//   X-BT3-Sign:    <64 hex digits> HMAC-SHA256(key, text), text being these six lines joined by "\n":
//                                      BT3-SIGN-1 / POST / the path ("/v1/quick") / the release / the time /
//                                      SHA-256 of the body exactly as sent, in hex
// The secret BUILD_KEYS holds the keys this Worker knows, as JSON: {"134":"<key of 0.1.34>","135":"<key of 0.1.35>"}.
// Without it nothing is checked and every request is answered as it always was. With it, each request is checked
// (hello and status apart), and the variable REQUIRE_KEY says what a failed check means:
//   unset, "off"   nothing is refused: the result is only logged and counted (see /v1/status)
//   "quick"        /v1/quick is refused; rooms with codes and the relay stay open to everyone
//   "all"          every request is refused (but hello, which tells an old game to update)
// A check fails for: "none" (no signature sent), "release" (no key for that release, or the release is below
// MIN_RELEASE, or is not the body's `rel`), "sign" (the signature is not the key's for this request), "time" (the
// time is more than KEY_SECONDS from this Worker's clock; the answer says the Worker's `now`, and the game asks
// once more by it: players' clocks are often wrong). A request can be sent again unchanged within that time (no
// record of signatures is kept); it cannot be changed. Keys and signatures are never logged.

const ROOM_SECONDS = 600;       // a room nobody joined disappears after ten minutes
const ROOMS_PER_ADDRESS = 4;    // rooms one address may have open at a time
const WRONG_CODES_PER_HOUR = 30; // codes that name no room, from one address in an hour
const HOSTS_PER_HOUR = 20;      // rooms one address may make in an hour
const QUICK_PER_HOUR = 60;      // quick-match searches from one address in an hour (a search that waits asks again every ten minutes)
const QUEUES = ["S", "T", "D10", "D15", "D20"];
const RELAY_PER_DAY_ADDRESS = 10; // relay logins one address may be given in a day
const RELAY_PER_DAY = 500;      // relay logins given in a day, all told (the variable TURN_DAILY_MAX overrides it): the
                                // ceiling on what the relay can be made to cost, whatever anyone sends
// The relay's use is measured (see relayCheck) when the Worker is given the means: the variable CF_ACCOUNT_ID and the
// secret CF_ANALYTICS_TOKEN (an API token with "Account Analytics: Read"), and a Cron Trigger every five minutes.
const RELAY_MONTH_GB = 500;     // bytes through the relay in a calendar month after which no more logins are given
                                // and those in use are ended (the variable TURN_MONTHLY_GB overrides it). Cloudflare
                                // charges after 1,000 GB.
const RELAY_LOGIN_GB = 1;       // one login that has passed this much in three hours is ended (a match is about
                                // 0.05 GB an hour; the variable TURN_LOGIN_GB overrides it)
const CHECK_STALE = 2 * 3600;   // with measuring set up: no logins when no measurement has succeeded for this long
const KEY_SECONDS = 300;        // the build key: how far a request's time may be from this Worker's clock, either way
const ALPHABET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"; // no I, L, O, 0, 1: they are misread

function reply(body, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json", "cache-control": "no-store" } });
}

function randomText(length, alphabet) {
    const bytes = new Uint8Array(length);
    crypto.getRandomValues(bytes);
    let out = "";
    for (const b of bytes) {
        out += alphabet[b % alphabet.length];
    }
    return out;
}

// A name as the game sends it: printable ASCII, at most 16 characters.
function cleanName(name) {
    return String(name ?? "Player").replace(/[^\x20-\x7e]/g, "").slice(0, 16) || "Player";
}

// The addresses a game reports: up to four "ip:port" strings.
function cleanAddrs(addrs) {
    if (!Array.isArray(addrs)) {
        return null;
    }
    const out = [];
    for (const a of addrs.slice(0, 5)) {
        if (typeof a === "string" && /^(relay=)?[0-9a-fA-F:.\[\]]{3,64}:\d{1,5}$/.test(a)) {
            out.push(a);
        }
    }
    return out.length > 0 ? out : null;
}

// An address of a private network ("192.168.1.20:7000"): of use only to someone on that network.
function isPrivate(a) {
    const m = /^(\d+)\.(\d+)\./.exec(a);
    if (!m) {
        return false;
    }
    const x = Number(m[1]), y = Number(m[2]);
    return x === 10 || x === 127 || (x === 172 && y >= 16 && y <= 31) || (x === 192 && y === 168) || (x === 169 && y === 254);
}

// What tells two machines of one household apart from strangers: the IPv4 address as it is, of an IPv6 address
// the first four groups (the network's part).
function network(ip) {
    if (!ip.includes(":")) {
        return ip;
    }
    const [head, tail] = ip.split("::");
    const h = head ? head.split(":") : [], t = tail ? tail.split(":") : [];
    const all = tail === undefined ? h : [...h, ...Array(Math.max(0, 8 - h.length - t.length)).fill("0"), ...t];
    return all.slice(0, 4).map((g) => parseInt(g, 16) || 0).join(":");
}

// The public IPv4 addresses among what a game said about itself.
function publicIps(addrs) {
    const out = [];
    for (const a of addrs) {
        const m = /^(\d+\.\d+\.\d+\.\d+):/.exec(a);
        if (m && !isPrivate(a)) {
            out.push(m[1]);
        }
    }
    return out;
}

// Whether the two are on one network: seen by this Worker under the same address (or IPv6 network), or, when one
// came by IPv4 and the other by IPv6, with the same public IPv4 address in what they found out themselves.
function sameNetwork(ipA, addrsA, ipB, addrsB) {
    if (ipA !== "" && network(ipA) === network(ipB)) {
        return true;
    }
    const b = publicIps(addrsB);
    return publicIps(addrsA).some((x) => b.includes(x));
}

// A player's addresses as the other player gets them: those of a private network only for someone on it.
function shown(addrs, same) {
    return addrs.filter((a) => a !== "same=1" && (same || !isPrivate(a)));
}

// How many uses of `what` the slot has so far (0 if none or not known).
async function usesSoFar(env, what, slot) {
    try {
        const row = await env.DB.prepare("SELECT n FROM used WHERE what = ? AND slot = ?").bind(what, slot).first();
        return row ? row.n : 0;
    } catch (e) {
        return 0;
    }
}

// Counts one more use of `what` in the time slot that starts at `slot` (seconds) and says how many that makes; 0 if
// the count could not be kept (the table "used" is missing: see schema.sql).
async function countUse(env, what, slot) {
    try {
        const row = await env.DB.prepare("INSERT INTO used (what, slot, n) VALUES (?, ?, 1) ON CONFLICT(what, slot) DO UPDATE SET n = n + 1 RETURNING n")
            .bind(what, slot).first();
        return row ? row.n : 0;
    } catch (e) {
        return 0;
    }
}

// A kept number: the value of `what` in `slot` (null if there is none), and setting it.
async function getValue(env, what, slot) {
    try {
        const row = await env.DB.prepare("SELECT n FROM used WHERE what = ? AND slot = ?").bind(what, slot).first();
        return row ? row.n : null;
    } catch (e) {
        return null;
    }
}

async function setValue(env, what, slot, n) {
    await env.DB.prepare("INSERT INTO used (what, slot, n) VALUES (?, ?, ?) ON CONFLICT(what, slot) DO UPDATE SET n = excluded.n").bind(what, slot, n).run();
}

function monthStart(now) {
    const d = new Date(now * 1000);
    return Math.floor(Date.UTC(d.getUTCFullYear(), d.getUTCMonth(), 1) / 1000);
}

function measuring(env) {
    return Boolean(env.CF_ACCOUNT_ID && env.CF_ANALYTICS_TOKEN && env.TURN_KEY_ID && env.TURN_KEY_TOKEN);
}

// Ends one relay login at once.
async function revoke(env, username) {
    try {
        const r = await fetch(`https://rtc.live.cloudflare.com/v1/turn/keys/${env.TURN_KEY_ID}/credentials/${encodeURIComponent(username)}/revoke`, {
            method: "POST",
            headers: { authorization: `Bearer ${env.TURN_KEY_TOKEN}` },
        });
        return r.ok;
    } catch (e) {
        return false;
    }
}

// Measures what has gone through the relay (Cloudflare's analytics, minutes behind) and acts on it: a login that has
// passed more than a match ever would is ended; past the month's limit every login in use is ended and no more are
// given until the next month. Run by the Cron Trigger, and by /v1/status. Returns what it found.
async function relayCheck(env, now) {
    if (!measuring(env)) {
        return { measured: false, why: "CF_ACCOUNT_ID and CF_ANALYTICS_TOKEN are not both set" };
    }
    const account = String(env.CF_ACCOUNT_ID).replace(/[^0-9a-f]/gi, ""), keyId = String(env.TURN_KEY_ID).replace(/[^0-9a-z]/gi, "");
    const month = monthStart(now), iso = (t) => new Date(t * 1000).toISOString().slice(0, 19) + "Z";
    const part = (name, from, extra) => `${name}: callsTurnUsageAdaptiveGroups(filter: { datetime_geq: "${iso(from)}", datetime_leq: "${iso(now)}", keyId: "${keyId}" }, ${extra}) { sum { egressBytes ingressBytes } dimensions { ${name === "logins" ? "username" : "keyId"} } }`;
    const query = `query { viewer { accounts(filter: { accountTag: "${account}" }) { ${part("month", month, "limit: 10")} ${part("logins", now - 3 * 3600, "limit: 500, orderBy: [sum_egressBytes_DESC]")} } } }`;
    let data;
    try {
        const r = await fetch("https://api.cloudflare.com/client/v4/graphql", {
            method: "POST",
            headers: { authorization: `Bearer ${env.CF_ANALYTICS_TOKEN}`, "content-type": "application/json" },
            body: JSON.stringify({ query }),
        });
        const j = await r.json();
        if (!r.ok || (j.errors && j.errors.length) || !j.data) {
            return { measured: false, why: "the analytics answered " + r.status + ": " + JSON.stringify(j.errors ?? j).slice(0, 300) };
        }
        data = j.data.viewer.accounts[0];
    } catch (e) {
        return { measured: false, why: "the analytics could not be asked: " + String(e).slice(0, 200) };
    }
    if (!data) {
        return { measured: false, why: "the analytics know no such account (CF_ACCOUNT_ID)" };
    }
    // both directions are counted: more than Cloudflare may charge for, so the limit is reached early rather than late
    const bytes = (g) => (g.sum.egressBytes ?? 0) + (g.sum.ingressBytes ?? 0);
    const monthBytes = (data.month ?? []).reduce((n, g) => n + bytes(g), 0);
    const monthLimit = (Number(env.TURN_MONTHLY_GB) || RELAY_MONTH_GB) * 1e9, loginLimit = (Number(env.TURN_LOGIN_GB) || RELAY_LOGIN_GB) * 1e9;
    const over = monthBytes >= monthLimit;
    let ended = 0;
    for (const g of data.logins ?? []) {
        const name = g.dimensions.username;
        if (name && (over || bytes(g) >= loginLimit) && (await getValue(env, "ended:" + name, 0)) === null) {
            if (await revoke(env, name)) {
                await setValue(env, "ended:" + name, 0, now);
                ended++;
            }
        }
    }
    if (over) {
        await setValue(env, "relayoff", month, 1);
    }
    await setValue(env, "checked", 0, now);
    return { measured: true, monthGB: Math.round(monthBytes / 1e7) / 100, limitGB: monthLimit / 1e9, relay: over ? "off until next month" : "on", loginsSeen: (data.logins ?? []).length, loginsEnded: ended };
}

// Whether this address may be given a relay login now (and counts it), or why not.
async function relayAllowed(env, ip, now) {
    if ((await getValue(env, "relayoff", monthStart(now))) !== null) {
        return "the month's relay allowance is used up";
    }
    if (env.CF_ACCOUNT_ID || env.CF_ANALYTICS_TOKEN) {
        const checked = await getValue(env, "checked", 0);
        if (checked === null || now - checked > CHECK_STALE) {
            return "the relay's use could not be measured lately: no logins until it can (see /v1/status)";
        }
    }
    const day = now - (now % 86400);
    const all = await countUse(env, "relay", day);
    if (all === 0) {
        return "the Worker cannot count relay logins (the table 'used' is missing): none are given";
    }
    if (all > (Number(env.TURN_DAILY_MAX) || RELAY_PER_DAY)) {
        return "the day's relay logins are used up";
    }
    if ((await countUse(env, "relay:" + ip, day)) > RELAY_PER_DAY_ADDRESS) {
        return "too many relay logins for this address today";
    }
    return null;
}

// Short-lived access to Cloudflare's TURN service for one host: {server, username, credential}, or {why} saying what
// stood in the way (never anything secret: for whoever sets the Worker up).
async function turnAccess(env) {
    if (!env.TURN_KEY_ID || !env.TURN_KEY_TOKEN) {
        return { why: `no key: the secret ${!env.TURN_KEY_ID ? "TURN_KEY_ID" : "TURN_KEY_TOKEN"} is not set` };
    }
    try {
        const r = await fetch(`https://rtc.live.cloudflare.com/v1/turn/keys/${env.TURN_KEY_ID}/credentials/generate-ice-servers`, {
            method: "POST",
            headers: { authorization: `Bearer ${env.TURN_KEY_TOKEN}`, "content-type": "application/json" },
            body: JSON.stringify({ ttl: 10800 }),
        });
        if (!r.ok) {
            return { why: `the TURN service answered ${r.status} (401 or 403: the key ID or the token is not right)` };
        }
        const j = await r.json();
        // iceServers is a list of entries, or (older answers) one entry
        const list = Array.isArray(j.iceServers) ? j.iceServers : j.iceServers ? [j.iceServers] : [];
        for (const s of list) {
            const urls = Array.isArray(s.urls) ? s.urls : [s.urls];
            const udp = urls.find((u) => typeof u === "string" && u.startsWith("turn:") && u.includes("transport=udp"));
            if (udp && s.username && s.credential) {
                return { server: udp.slice(5).split("?")[0], username: s.username, credential: s.credential };
            }
        }
        return { why: "the TURN service's answer had no UDP relay in it" };
    } catch (e) {
        return { why: "the TURN service could not be asked" };
    }
}

// Puts a player into a room nobody has joined yet. null if someone else got the place first.
async function joinRoom(env, room, ip, addrs, name) {
    const key = randomText(24, ALPHABET);
    const hostAddrs = JSON.parse(room.host_addrs);
    const same = sameNetwork(ip, addrs, room.ip, hostAddrs);
    // what the host will be told of this player (kept as it will be shown; "same=1" remembers the finding)
    const kept = shown(addrs, same);
    if (/^[0-9.]{7,15}$/.test(ip)) {
        kept.push("seen=" + ip); // for the host's relay: whom to let through
    }
    if (same) {
        kept.push("same=1");
    }
    // only if nobody has joined yet (the update tells whether it was this request that got the place)
    const done = await env.DB.prepare("UPDATE rooms SET join_key = ?, join_name = ?, join_addrs = ? WHERE code = ? AND join_key IS NULL")
        .bind(key, cleanName(name), JSON.stringify(kept), room.code).run();
    if (!done.meta || done.meta.changes !== 1) {
        return null;
    }
    return { key, you: ip, peer: { name: room.host_name, addrs: shown(hostAddrs, same) } };
}

// Makes a room (queue: null for one with a code to hand on). null if no code could be found for it.
async function makeRoom(env, ip, now, v, name, addrs, queue, who, wantRelay) {
    const key = randomText(24, ALPHABET);
    for (let attempt = 0; attempt < 5; attempt++) {
        const code = randomText(6, ALPHABET);
        try {
            await env.DB.prepare("INSERT INTO rooms (code, made, version, ip, host_key, host_name, host_addrs, queue, who) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)")
                .bind(code, now, v, ip, key, cleanName(name), JSON.stringify(addrs), queue, who).run();
            if (!wantRelay) {
                return { code, key, you: ip };
            }
            const refused = await relayAllowed(env, ip, now);
            const turn = refused ? { why: refused } : await turnAccess(env);
            return turn.server ? { code, key, you: ip, turn } : { code, key, you: ip, noRelay: turn.why };
        } catch (e) {
            // the code is taken: another one
        }
    }
    return null;
}

// The build keys this Worker knows: null if the secret BUILD_KEYS is not set (nothing is checked), else {keys: a Map
// of release -> key, problem: what is wrong with the secret or null}. A secret that cannot be read gives no keys, so
// that every check fails: with REQUIRE_KEY at "quick" or "all" that refuses everyone, which is why the first stage
// is "off" (and /v1/status says what is wrong, never a key).
function buildKeys(env) {
    if (env.BUILD_KEYS === undefined || env.BUILD_KEYS === null || String(env.BUILD_KEYS).trim() === "") {
        return null;
    }
    const keys = new Map();
    let problem = null;
    try {
        const j = JSON.parse(String(env.BUILD_KEYS));
        if (j === null || typeof j !== "object" || Array.isArray(j)) {
            throw new Error("not a map");
        }
        for (const [release, key] of Object.entries(j)) {
            if (/^[1-9]\d{0,5}$/.test(release) && typeof key === "string" && key.length >= 16) {
                keys.set(release, key);
            } else {
                problem = "BUILD_KEYS: an entry was left out (a release is a number like 134, a key a text of 16 characters or more)";
            }
        }
    } catch (e) {
        problem = 'BUILD_KEYS is not JSON of the form {"134":"<key>","135":"<key>"}';
    }
    return { keys, problem };
}

// What REQUIRE_KEY says: "off", "quick" or "all" (anything else is taken as "off").
function requireKey(env) {
    const mode = String(env.REQUIRE_KEY ?? "off").trim().toLowerCase();
    return mode === "quick" || mode === "all" ? mode : "off";
}

function toHex(bytes) {
    return [...new Uint8Array(bytes)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

// Checks a request's signature: null if it is right, else why not ("none", "release", "sign", "time"). `raw` is the
// body as it came (an ArrayBuffer), `rel` the release the body names (if it names one).
async function keyCheck(request, keys, path, raw, rel, now, minRelease) {
    const release = request.headers.get("x-bt3-release") ?? "", time = request.headers.get("x-bt3-time") ?? "", sign = request.headers.get("x-bt3-sign") ?? "";
    if (sign === "") {
        return "none";
    }
    const key = keys.get(release);
    if (key === undefined || Number(release) < minRelease || (rel !== undefined && rel !== Number(release))) {
        return "release";
    }
    if (!/^\d{1,12}$/.test(time) || !/^[0-9a-f]{64}$/.test(sign)) {
        return "sign";
    }
    const enc = new TextEncoder();
    const text = ["BT3-SIGN-1", request.method, path, release, time, toHex(await crypto.subtle.digest("SHA-256", raw))].join("\n");
    const k = await crypto.subtle.importKey("raw", enc.encode(key), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
    const want = new Uint8Array(await crypto.subtle.sign("HMAC", k, enc.encode(text)));
    let differ = 0; // compared whole, whatever differs: the time taken says nothing about where
    for (let i = 0; i < 32; i++) {
        differ |= want[i] ^ parseInt(sign.slice(i * 2, i * 2 + 2), 16);
    }
    if (differ !== 0) {
        return "sign";
    }
    return Math.abs(now - Number(time)) > KEY_SECONDS ? "time" : null; // (last: only a holder of the key is told the time is off)
}

// For /v1/status: what the Worker knows of build keys (never a key), and today's checks of new rooms, joins and searches.
async function keyStatus(env, now) {
    const known = buildKeys(env);
    if (known === null) {
        return { keys: "BUILD_KEYS is not set: nothing is checked" };
    }
    const day = now - (now % 86400), today = {};
    for (const r of ["ok", "none", "release", "sign", "time"]) {
        today[r] = (await getValue(env, "key:" + r, day)) ?? 0;
    }
    const out = { releases: [...known.keys.keys()].map(Number).sort((a, b) => a - b), require: requireKey(env), today };
    if (known.problem) {
        out.problem = known.problem;
    }
    if (String(env.REQUIRE_KEY ?? "off").trim().toLowerCase() !== out.require) {
        out.problem = (out.problem ? out.problem + "; " : "") + 'REQUIRE_KEY is not "off", "quick" or "all": taken as "off"';
    }
    return out;
}

// A game of 0.1.29 or later asks for a relay login only when a match turns out to need one (/v1/relay), so it is
// not given one with its room. (Given with every room, a day's logins were used up by searches that never played
// through the relay: found on the first day of quick match, and the relay was off for everyone until midnight.)
function asksLater(body) {
    return Number.isInteger(body.rel) && body.rel >= 129;
}

export default {
    async fetch(request, env) {
        const url = new URL(request.url);
        if (request.method === "GET" && url.pathname === "/") {
            return new Response("Tenkaichi3Decomp matchmaking: room codes. See the game's repository, port/matchmaking.\n");
        }
        if (request.method !== "POST" || !url.pathname.startsWith("/v1/")) {
            return reply({ error: "bad request" }, 404);
        }
        let body, raw; // (raw: the body's bytes as they came, for the build key's signature)
        try {
            raw = await request.arrayBuffer();
            body = JSON.parse(new TextDecoder().decode(raw));
        } catch (e) {
            return reply({ error: "bad request" }, 400);
        }
        const ip = request.headers.get("cf-connecting-ip") ?? "";
        const now = Math.floor(Date.now() / 1000);
        const what = url.pathname.slice(4);

        if (what === "status") { // for whoever runs the Worker: measures now and says what it found (no secrets in it)
            if (!env.ADMIN_KEY || String(env.ADMIN_KEY).length < 16 || body.admin !== env.ADMIN_KEY) {
                return reply({ error: "bad request" }, 404);
            }
            const day = now - (now % 86400);
            return reply({ ...(await relayCheck(env, now)), loginsToday: (await getValue(env, "relay", day)) ?? 0, lastMeasured: await getValue(env, "checked", 0), buildKey: await keyStatus(env, now) });
        }
        // Only the newest release, if the Worker's keeper says so: the variable MIN_RELEASE (a number: 129 is release
        // 0.1.29) refuses a new room, a join or a search from a game that says it is older, or that does not say
        // (0.1.28 and earlier do not). Rooms that are under way are not touched. An older game shows its own words
        // for an answer it does not know ("gave no room", "gave no answer"); a newer one says to get the new release.
        const minRelease = Number(env.MIN_RELEASE ?? 0) || 0;
        if (what === "hello") { // a game asks, when its online screen opens, whether it is new enough (and what the newest is)
            return reply({ min: minRelease, latest: Number(env.LATEST_RELEASE ?? 0) || 0, now });
        }
        const entry = what === "host" || what === "join" || what === "quick";
        if (minRelease > 0 && entry && !(Number.isInteger(body.rel) && body.rel >= minRelease)) {
            return reply({ error: "old", need: minRelease }, 426);
        }
        // Only official releases, if the Worker's keeper says so: the build key (see the top of this file). After the
        // check above, so that an official release that is too old is told to update, not that it is no release.
        const known = buildKeys(env);
        if (known !== null) {
            const mode = requireKey(env), needed = mode === "all" || (mode === "quick" && what === "quick");
            const why = await keyCheck(request, known.keys, url.pathname, raw, Number.isInteger(body.rel) ? body.rel : undefined, now, minRelease);
            if (entry) { // (counted for new rooms, joins and searches only: a room under way asks every two seconds)
                await countUse(env, "key:" + (why ?? "ok"), now - (now % 86400));
            }
            if (why !== null && (needed || entry)) {
                console.log(`build key: ${what} from release ${String(request.headers.get("x-bt3-release") ?? body.rel ?? "?").replace(/[^0-9?]/g, "").slice(0, 8)}: ${why}, ${needed ? "refused" : "let in"}`);
            }
            if (why !== null && needed) {
                return reply({ error: "build key", why, now }, 403);
            }
        }
        if (what === "host") {
            const addrs = cleanAddrs(body.addrs);
            if (!Number.isInteger(body.v) || addrs === null) {
                return reply({ error: "bad request" }, 400);
            }
            await env.DB.prepare("DELETE FROM rooms WHERE made < ?").bind(now - ROOM_SECONDS).run();
            const open = await env.DB.prepare("SELECT COUNT(*) AS n FROM rooms WHERE ip = ?").bind(ip).first();
            if (open && open.n >= ROOMS_PER_ADDRESS) {
                return reply({ error: "busy" }, 429);
            }
            if ((await countUse(env, "host:" + ip, now - (now % 3600))) > HOSTS_PER_HOUR) {
                return reply({ error: "busy" }, 429);
            }
            if (now % 50 === 0) { // now and then: counts of past days are thrown away
                await env.DB.prepare("DELETE FROM used WHERE slot < ? AND slot > 0 AND what != 'relayoff'").bind(now - 2 * 86400).run().catch(() => {});
                await env.DB.prepare("DELETE FROM used WHERE slot = 0 AND what LIKE 'ended:%' AND n < ?").bind(now - 2 * 86400).run().catch(() => {});
            }
            const made = await makeRoom(env, ip, now, body.v, body.name, addrs, null, null, !asksLater(body));
            return made ? reply(made) : reply({ error: "busy" }, 503);
        }
        if (what === "queues") { // how many wait in each queue (for the window)
            const waiting = {};
            for (const q of QUEUES) {
                waiting[q] = 0;
            }
            const rows = await env.DB.prepare("SELECT queue, COUNT(*) AS n FROM rooms WHERE queue IS NOT NULL AND version = ? AND join_key IS NULL AND made >= ? GROUP BY queue")
                .bind(Number.isInteger(body.v) ? body.v : 0, now - ROOM_SECONDS).all();
            for (const row of (rows && rows.results) || []) {
                if (row.queue in waiting) {
                    waiting[row.queue] = row.n;
                }
            }
            return reply({ waiting });
        }
        if (what === "quick") {
            const addrs = cleanAddrs(body.addrs);
            const queue = String(body.queue ?? ""), who = String(body.who ?? "").replace(/[^A-Za-z0-9]/g, "").slice(0, 24);
            if (!Number.isInteger(body.v) || addrs === null || !QUEUES.includes(queue) || who.length < 8) {
                return reply({ error: "bad request" }, 400);
            }
            if ((await countUse(env, "quick:" + ip, now - (now % 3600))) > QUICK_PER_HOUR) {
                return reply({ error: "busy" }, 429);
            }
            await env.DB.prepare("DELETE FROM rooms WHERE made < ?").bind(now - ROOM_SECONDS).run();
            // a room this same game left behind in a queue (it searched, then searched again): gone
            await env.DB.prepare("DELETE FROM rooms WHERE who = ? AND queue IS NOT NULL AND join_key IS NULL").bind(who).run();
            for (let attempt = 0; attempt < 3; attempt++) {
                // whoever waits longest in this queue
                const waiting = await env.DB.prepare("SELECT * FROM rooms WHERE queue = ? AND version = ? AND join_key IS NULL AND made >= ? ORDER BY rowid LIMIT 1")
                    .bind(queue, body.v, now - ROOM_SECONDS).first();
                if (waiting) {
                    const joined = await joinRoom(env, waiting, ip, addrs, body.name);
                    if (joined) {
                        return reply({ role: "join", code: waiting.code, ...joined });
                    }
                    continue; // someone else got that place: the next one
                }
                const open = await env.DB.prepare("SELECT COUNT(*) AS n FROM rooms WHERE ip = ?").bind(ip).first();
                if (open && open.n >= ROOMS_PER_ADDRESS) {
                    return reply({ error: "busy" }, 429);
                }
                const made = await makeRoom(env, ip, now, body.v, body.name, addrs, queue, who, body.relay !== false && !asksLater(body));
                if (!made) {
                    return reply({ error: "busy" }, 503);
                }
                // Two who search in the same moment both find nobody and both make a room. The one whose room came
                // second sees the first one's here, gives its own up (unless someone is in it already) and joins.
                const earlier = await env.DB.prepare("SELECT COUNT(*) AS n FROM rooms WHERE queue = ? AND version = ? AND join_key IS NULL AND made >= ? AND rowid < (SELECT rowid FROM rooms WHERE code = ?)")
                    .bind(queue, body.v, now - ROOM_SECONDS, made.code).first();
                if (earlier && earlier.n > 0) {
                    const gone = await env.DB.prepare("DELETE FROM rooms WHERE code = ? AND join_key IS NULL").bind(made.code).run();
                    if (gone.meta && gone.meta.changes === 1) {
                        continue;
                    }
                }
                return reply({ role: "host", ...made });
            }
            return reply({ error: "busy" }, 503);
        }

        const code = String(body.code ?? "").toUpperCase().replace(/[^A-Z0-9]/g, "");
        if (code.length !== 6) {
            return reply({ error: "no room" }, 404);
        }
        const hour = now - (now % 3600);
        if (what === "join" && (await usesSoFar(env, "miss:" + ip, hour)) >= WRONG_CODES_PER_HOUR) {
            return reply({ error: "busy" }, 429); // (codes are not to be found by trying)
        }
        const room = await env.DB.prepare("SELECT * FROM rooms WHERE code = ? AND made >= ?").bind(code, now - ROOM_SECONDS).first();
        if (!room) {
            if (what === "join") {
                await countUse(env, "miss:" + ip, hour);
            }
            return reply({ error: "no room" }, 404);
        }

        if (what === "join") {
            const addrs = cleanAddrs(body.addrs);
            if (!Number.isInteger(body.v) || addrs === null) {
                return reply({ error: "bad request" }, 400);
            }
            if (body.v !== room.version) {
                return reply({ error: "version" }, 409);
            }
            const joined = await joinRoom(env, room, ip, addrs, body.name);
            return joined ? reply(joined) : reply({ error: "full" }, 409);
        }

        const isHost = body.key === room.host_key, isJoin = room.join_key !== null && body.key === room.join_key;
        if (!isHost && !isJoin) {
            return reply({ error: "no room" }, 404);
        }
        if (what === "poll") {
            if (isHost) {
                return reply({ peer: room.join_key !== null ? { name: room.join_name, addrs: shown(JSON.parse(room.join_addrs), true) } : null });
            }
            return reply({ peer: { name: room.host_name, addrs: shown(JSON.parse(room.host_addrs), JSON.parse(room.join_addrs).includes("same=1")) } });
        }
        if (what === "relay") { // the host: the two could not reach each other directly, a relay login is needed after all
            if (!isHost || room.join_key === null) {
                return reply({ error: "bad request" }, 400); // (only once someone has joined: nobody waits with a login)
            }
            const refused = await relayAllowed(env, ip, now);
            const turn = refused ? { why: refused } : await turnAccess(env);
            return reply(turn.server ? { turn } : { noRelay: turn.why });
        }
        if (what === "addrs") { // the host has found out more about how it can be reached
            const addrs = cleanAddrs(body.addrs);
            if (!isHost || addrs === null) {
                return reply({ error: "bad request" }, 400);
            }
            await env.DB.prepare("UPDATE rooms SET host_addrs = ? WHERE code = ?").bind(JSON.stringify(addrs), code).run();
            return reply({});
        }
        if (what === "leave") {
            await env.DB.prepare("DELETE FROM rooms WHERE code = ?").bind(code).run();
            return reply({});
        }
        return reply({ error: "bad request" }, 404);
    },

    // the Cron Trigger: the relay's use is measured
    async scheduled(event, env, ctx) {
        ctx.waitUntil(relayCheck(env, Math.floor(Date.now() / 1000)));
    },
};
