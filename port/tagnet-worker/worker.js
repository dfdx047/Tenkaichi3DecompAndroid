// Tag Team Online: the rooms of the Tag Team mod over the internet (Dragon Rage, Android). A Cloudflare Worker with a
// D1 database (schema.sql). It never carries the game itself: it gives a room a code, tells the players in it each
// other's addresses (what each game found out about itself: its own network, STUN), and gives the room's owner a login
// for Cloudflare's TURN relay when a player cannot be reached directly. The games then speak to each other over UDP
// (port/src/gs/tag_link.c), through the relay only when they must.
//
// Requests: POST /v1/<what> with a JSON body; answers are JSON (errors: {error: "..."} with a 4xx status).
//   /v1/hello  {v}                                   -> {ok, v}
//   /v1/host   {v, name, addrs}                      -> {code, key, id: 0, you}
//   /v1/join   {v, code, name, addrs, watch}         -> {key, id, you, host: {name, addrs}}       ("no room", "version", "full")
//   /v1/poll   {code, key}                           -> owner: {members: [{id, name, addrs, seen, watch}]}
//                                                       member: {host: {name, addrs}}            ("no room": it is gone)
//   /v1/addrs  {code, key, addrs}                    -> {}   (one's addresses again: the owner's relay address, found later)
//   /v1/relay  {code, key}                           -> {turn: {server, username, credential}} | {noRelay: why}   (the owner only)
//   /v1/leave  {code, key}                           -> {}   (the owner leaving closes the room)
//   GET /v1/ws?code=&key=   (a WebSocket)            the room's relay, for players who cannot reach each other directly:
//                      a room's own Durable Object (TagRelay) passes binary messages between its owner and its members.
//                      A member's message reaches the owner with the member's number before it (one byte); the owner
//                      sends a member's with that number before it. Needs the Durable Object binding RELAY (class TagRelay).
// `v` is the mod's network version (the game's PROTOCOL): rooms only take games of their own. `addrs` is a list of up to
// eight "ip:port" (the owner's relay address as "relay=ip:port"); `seen` is the address this Worker saw a player under
// (the relay must be told whom to let through). A room its owner has not asked about for five minutes is gone.
//
// Settings (Cloudflare dashboard, the Worker's variables and secrets):
//   DB                 the D1 database (binding)
//   TURN_KEY_ID, TURN_KEY_TOKEN   secrets: a TURN key of Cloudflare's (Realtime > TURN), or
//   METERED_APP, METERED_KEY      the relay of Metered (metered.ca, free plan): the app's name and its API key.
//                      Without one of them there is no relay: players who cannot reach each other directly cannot play.
//   TURN_DAILY_MAX     relay logins a day, all rooms together (default 300)
//   ALLOWED_VERSIONS   the network versions taken, "5" or "5,6" (default: any)
//   MAX_MEMBERS        players and spectators a room takes besides its owner (default 19: ten players, ten watching)
//   BUILD_KEYS (secret), REQUIRE_KEY   signed requests: see keyGate below (default: nothing is checked)

// A phone in the background may not poll at all (Android freezes a cached app; the game asks every 15 s while it can):
// the owner sharing the room's code in another app, a player answering a message. With two minutes for a room and 45 s
// for a member, the room was gone (a friend's code: "no room") or the player out of it (its relay refused) when they
// came back. A room and a member's place now outlive five minutes away; the owner's list still shows only the members
// heard from in the last MEMBER_IDLE seconds, and only those count towards a full room.
const ROOM_IDLE = 300;      // seconds a room lives without its owner's poll
const MEMBER_IDLE = 45;     // seconds without a poll after which a member leaves the owner's list (it is told it is gone)
const MEMBER_TOP = 255;     // the largest member number (one byte through the room's relay)
const MEMBER_KEEP = 300;    // seconds without a poll after which a member's place (its key, its relay) is gone
const RELAY_PER_DAY = 300;
// D1's free plan counts every row read and written (100,000 written a day): a poll every 2 s that wrote its room's and
// its member's time each time was most of the writes (68,000 on 2026-11-09). The time is now written only when it is
// older than TOUCH_EVERY, the rooms and members gone quiet are deleted at most every SWEEP_EVERY (each Worker
// instance), and what is read checks the times itself, so nothing gone is ever answered in between.
const TOUCH_EVERY = 15;     // seconds
const SWEEP_EVERY = 30;     // seconds
let lastSweep = 0;

function reply(obj, status = 200) {
    return new Response(JSON.stringify(obj), { status, headers: { "content-type": "application/json" } });
}

function cleanName(s) {
    return typeof s === "string" ? s.replace(/[\u0000-\u001f"\\]/g, "").slice(0, 24) : "";
}

function cleanAddrs(a) {
    if (!Array.isArray(a)) return [];
    return a.filter((x) => typeof x === "string" && /^(relay=)?[0-9a-fA-F.:]{3,60}$/.test(x)).slice(0, 8);
}

function versionAllowed(env, v) {
    if (!Number.isInteger(v)) return false;
    const list = String(env.ALLOWED_VERSIONS ?? "").split(",").map((x) => Number.parseInt(x, 10)).filter(Number.isInteger);
    return list.length === 0 || list.includes(v);
}

function randomKey() {
    const b = new Uint8Array(16);
    crypto.getRandomValues(b);
    return [...b].map((x) => x.toString(16).padStart(2, "0")).join("");
}

function randomCode() {
    const b = new Uint32Array(1);
    crypto.getRandomValues(b);
    return String(100000 + (b[0] % 900000)); // six digits: the game's keypad has only digits
}

async function sweep(env, now) {
    if (now >= lastSweep && now - lastSweep < SWEEP_EVERY) return;
    lastSweep = now;
    await env.DB.batch([
        env.DB.prepare("DELETE FROM tag_members WHERE code IN (SELECT code FROM tag_rooms WHERE touched < ?)").bind(now - ROOM_IDLE),
        env.DB.prepare("DELETE FROM tag_rooms WHERE touched < ?").bind(now - ROOM_IDLE),
        env.DB.prepare("DELETE FROM tag_members WHERE touched < ?").bind(now - MEMBER_KEEP),
    ]);
}

async function whoIs(env, code, key, now) {
    if (typeof code !== "string" || typeof key !== "string") return null;
    const room = await env.DB.prepare("SELECT * FROM tag_rooms WHERE code = ?").bind(code).first();
    if (!room || room.touched < now - ROOM_IDLE) return null;
    if (room.host_key === key) return { room, owner: true };
    const m = await env.DB.prepare("SELECT * FROM tag_members WHERE code = ? AND key = ?").bind(code, key).first();
    return m && m.touched >= now - MEMBER_KEEP ? { room, owner: false, member: m } : null;
}

// The relay of Metered (metered.ca, free plan): METERED_APP is the app's name (<name>.metered.live), METERED_KEY its
// API key. Its credentials answer is a list of {urls, username, credential}; the UDP one is used.
async function meteredLogin(env) {
    try {
        const app = String(env.METERED_APP).replace(/[^0-9a-z-]/gi, "");
        const r = await fetch(`https://${app}.metered.live/api/v1/turn/credentials?apiKey=${encodeURIComponent(env.METERED_KEY)}`);
        if (!r.ok) return { why: `Metered answered ${r.status}` };
        const list = await r.json();
        for (const s of Array.isArray(list) ? list : []) {
            const urls = Array.isArray(s.urls) ? s.urls : [s.urls];
            const udp = urls.find((u) => typeof u === "string" && u.startsWith("turn:") && !u.includes("transport=tcp"));
            if (udp && s.username && s.credential) {
                return { server: udp.slice(5).split("?")[0], username: s.username, credential: s.credential };
            }
        }
        return { why: "Metered's answer had no UDP relay in it" };
    } catch (e) {
        return { why: "Metered could not be asked" };
    }
}

async function relayLogin(env, now) {
    const metered = env.METERED_APP && env.METERED_KEY;
    if (!metered && (!env.TURN_KEY_ID || !env.TURN_KEY_TOKEN)) {
        return { why: "this server has no relay (TURN key not set)" };
    }
    const day = now - (now % 86400);
    const row = await env.DB.prepare("INSERT INTO tag_relay_days (day, n) VALUES (?, 1) ON CONFLICT(day) DO UPDATE SET n = n + 1 RETURNING n").bind(day).first();
    if (row && row.n > (Number(env.TURN_DAILY_MAX) || RELAY_PER_DAY)) {
        return { why: "the relay's logins for today are used up" };
    }
    if (metered) return meteredLogin(env);
    try {
        const r = await fetch(`https://rtc.live.cloudflare.com/v1/turn/keys/${env.TURN_KEY_ID}/credentials/generate-ice-servers`, {
            method: "POST",
            headers: { authorization: `Bearer ${env.TURN_KEY_TOKEN}`, "content-type": "application/json" },
            body: JSON.stringify({ ttl: 10800 }),
        });
        if (!r.ok) return { why: `the TURN service answered ${r.status}` };
        const j = await r.json();
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

// Signed requests (net_sign.c in the game): a release's build sends X-BT3-Release, X-BT3-Time and X-BT3-Sign, an HMAC of
// the request made with that release's key. BUILD_KEYS (a secret) is a JSON object {"<release>": "<key>"}; REQUIRE_KEY is
// "off" (nothing is checked), "count" (every host and join is counted by result, country and version in tag_key_days,
// nobody is refused) or "on" (a request not signed by an official build is refused with 403).
const KEY_SECONDS = 600;
function buildKeys(env) {
    const keys = new Map();
    try {
        const j = JSON.parse(String(env.BUILD_KEYS ?? "{}"));
        for (const [release, key] of Object.entries(j && typeof j === "object" && !Array.isArray(j) ? j : {})) {
            if (/^[1-9]\d{0,5}$/.test(release) && typeof key === "string" && key.length >= 16) keys.set(release, key);
        }
    } catch (e) {
    }
    return keys;
}
function requireKey(env) {
    const mode = String(env.REQUIRE_KEY ?? "off").trim().toLowerCase();
    return mode === "count" || mode === "on" ? mode : "off";
}
function toHex(bytes) {
    return [...new Uint8Array(bytes)].map((b) => b.toString(16).padStart(2, "0")).join("");
}
async function keyCheck(request, keys, path, raw, now) {
    const release = request.headers.get("x-bt3-release") ?? "", time = request.headers.get("x-bt3-time") ?? "", sign = request.headers.get("x-bt3-sign") ?? "";
    if (sign === "") return "none";
    const key = keys.get(release);
    if (key === undefined) return "release";
    if (!/^\d{1,12}$/.test(time) || !/^[0-9a-f]{64}$/.test(sign)) return "sign";
    const enc = new TextEncoder();
    const text = ["BT3-SIGN-1", request.method, path, release, time, toHex(await crypto.subtle.digest("SHA-256", raw))].join("\n");
    const k = await crypto.subtle.importKey("raw", enc.encode(key), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
    const want = new Uint8Array(await crypto.subtle.sign("HMAC", k, enc.encode(text)));
    let differ = 0;
    for (let i = 0; i < 32; i++) differ |= want[i] ^ parseInt(sign.slice(i * 2, i * 2 + 2), 16);
    if (differ !== 0) return "sign";
    return Math.abs(now - Number(time)) > KEY_SECONDS ? "time" : null;
}
async function keyGate(request, env, raw, body, now, what) {
    const mode = requireKey(env);
    if (mode === "off") return null;
    const why = await keyCheck(request, buildKeys(env), new URL(request.url).pathname, raw, now);
    const country = String(request.cf && request.cf.country || "??").replace(/[^A-Z?]/g, "").slice(0, 2);
    const v = Number.isInteger(body.v) ? body.v : 0;
    try {
        await env.DB.prepare("INSERT INTO tag_key_days (day, what, n) VALUES (?, ?, 1) ON CONFLICT (day, what) DO UPDATE SET n = n + 1").bind(now - now % 86400, `${why ?? "ok"} ${what} v${v} ${country}`).run();
    } catch (e) {
    }
    if (why === null || mode !== "on") return null;
    if (why === "time") return reply({ error: "time", now }, 403);
    return reply({ error: "official" }, 403);
}
export async function handle(request, env, now) {
    const url = new URL(request.url);
    if (request.method === "GET" && url.pathname === "/") {
        return new Response("Tag Team Online: rooms for the Tag Team mod of Dragon Rage.\n", { headers: { "content-type": "text/plain; charset=utf-8" } });
    }
    if (request.method === "GET" && url.pathname === "/v1/ws") {
        if (request.headers.get("upgrade") !== "websocket") return reply({ error: "bad request" }, 426);
        if (!env.RELAY) return reply({ error: "no relay" }, 503);
        await sweep(env, now);
        const who = await whoIs(env, url.searchParams.get("code"), url.searchParams.get("key"), now);
        if (!who) return reply({ error: "no room" }, 404);
        const headers = new Headers(request.headers);
        headers.set("x-tag-who", who.owner ? "h" : "m" + who.member.id);
        // The room's relay is made where the room's owner is (the first to connect, at the room's creation): left to
        // Cloudflare it may sit on another continent, 100-150 ms more each way between players in Brazil.
        const hints = { SA: "sam", NA: "enam", EU: "weur", AS: "apac", OC: "oc", AF: "afr" };
        const hint = hints[(request.cf && request.cf.continent) || ""] || "sam";
        const stub = env.RELAY.get(env.RELAY.idFromName(who.room.code), { locationHint: hint });
        return stub.fetch(new Request(request.url, { headers }));
    }
    if (request.method !== "POST" || !url.pathname.startsWith("/v1/")) return reply({ error: "bad request" }, 404);
    let body, raw;
    try {
        raw = await request.arrayBuffer();
        if (raw.byteLength > 2048) return reply({ error: "bad request" }, 413);
        body = JSON.parse(new TextDecoder().decode(raw));
    } catch (e) {
        return reply({ error: "bad request" }, 400);
    }
    if (!body || typeof body !== "object" || Array.isArray(body)) return reply({ error: "bad request" }, 400);
    const what = url.pathname.slice(4);
    const ip = request.headers.get("cf-connecting-ip") ?? "";

    if (what === "hello") return reply({ ok: versionAllowed(env, body.v), v: body.v ?? null, now });
    await sweep(env, now);

    if (what === "host" || what === "join") {
        const refused = await keyGate(request, env, raw, body, now, what);
        if (refused) return refused;
    }

    if (what === "host") {
        if (!versionAllowed(env, body.v)) return reply({ error: "version" }, 409);
        const key = randomKey();
        for (let tries = 0; tries < 8; tries++) {
            const code = randomCode();
            const r = await env.DB.prepare("INSERT OR IGNORE INTO tag_rooms (code, v, host_key, host_name, host_addrs, host_seen, created, touched) VALUES (?, ?, ?, ?, ?, ?, ?, ?)")
                .bind(code, body.v, key, cleanName(body.name), JSON.stringify(cleanAddrs(body.addrs)), ip, now, now).run();
            if (r.meta.changes === 1) return reply({ code, key, id: 0, you: ip });
        }
        return reply({ error: "busy" }, 503);
    }

    if (what === "join") {
        const code = String(body.code ?? "").replace(/\D/g, "");
        const room = await env.DB.prepare("SELECT * FROM tag_rooms WHERE code = ?").bind(code).first();
        if (!room || room.touched < now - ROOM_IDLE) return reply({ error: "no room" }, 404);
        if (room.v !== body.v) return reply({ error: "version" }, 409);
        const max = Number(env.MAX_MEMBERS) || 19;
        const count = await env.DB.prepare("SELECT COUNT(*) FILTER (WHERE touched >= ?) AS n, COALESCE(MAX(id), 0) AS top FROM tag_members WHERE code = ?").bind(now - MEMBER_IDLE, code).first();
        if (count.n >= max) return reply({ error: "full" }, 409);
        // A member's number goes through the room's relay as one byte (TagRelay below, tag_link.c's relay addresses):
        // it stays within 1..MEMBER_TOP. The next number after the largest, as always; past MEMBER_TOP (a long-lived
        // room whose newest member stays), the smallest free one. Two joins at once may want the same number: the one
        // that finds it taken takes another (the members' primary key) instead of failing.
        const key = randomKey();
        let id = 0, candidates = count.top < MEMBER_TOP ? [count.top + 1] : null;
        for (let round = 0; round < 2 && !id; round++) {
            if (candidates === null) {
                const taken = new Set((await env.DB.prepare("SELECT id FROM tag_members WHERE code = ?").bind(code).all()).results.map((r) => r.id));
                candidates = [];
                for (let n = 1; n <= MEMBER_TOP && candidates.length < 4; n++) if (!taken.has(n)) candidates.push(n);
            }
            for (const n of candidates) {
                const r = await env.DB.prepare("INSERT OR IGNORE INTO tag_members (code, id, key, name, addrs, seen, watch, touched) VALUES (?, ?, ?, ?, ?, ?, ?, ?)")
                    .bind(code, n, key, cleanName(body.name), JSON.stringify(cleanAddrs(body.addrs)), ip, body.watch ? 1 : 0, now).run();
                if (r.meta.changes === 1) { id = n; break; }
            }
            candidates = null;
        }
        if (!id) return reply({ error: "full" }, 409);
        await env.DB.prepare("UPDATE tag_rooms SET touched = ? WHERE code = ?").bind(now, code).run();
        return reply({ key, id, you: ip, host: { name: room.host_name, addrs: JSON.parse(room.host_addrs), seen: room.host_seen } });
    }

    const who = await whoIs(env, body.code, body.key, now);
    if (!who) return reply({ error: "no room" }, 404);
    const code = who.room.code;

    if (what === "poll") {
        if (who.owner) {
            if (now - who.room.touched >= TOUCH_EVERY) await env.DB.prepare("UPDATE tag_rooms SET touched = ? WHERE code = ?").bind(now, code).run();
            const rows = await env.DB.prepare("SELECT id, name, addrs, seen, watch FROM tag_members WHERE code = ? AND touched >= ? ORDER BY id").bind(code, now - MEMBER_IDLE).all();
            return reply({ members: rows.results.map((m) => ({ id: m.id, name: m.name, addrs: JSON.parse(m.addrs), seen: m.seen, watch: !!m.watch })) });
        }
        const writes = [];
        if (now - who.member.touched >= TOUCH_EVERY || who.member.seen !== ip) {
            writes.push(env.DB.prepare("UPDATE tag_members SET touched = ?, seen = ? WHERE code = ? AND id = ?").bind(now, ip, code, who.member.id));
        }
        if (now - who.room.touched >= TOUCH_EVERY) writes.push(env.DB.prepare("UPDATE tag_rooms SET touched = ? WHERE code = ?").bind(now, code));
        if (writes.length) await env.DB.batch(writes);
        return reply({ host: { name: who.room.host_name, addrs: JSON.parse(who.room.host_addrs), seen: who.room.host_seen } });
    }

    if (what === "addrs") {
        const addrs = JSON.stringify(cleanAddrs(body.addrs));
        if (who.owner) await env.DB.prepare("UPDATE tag_rooms SET host_addrs = ?, touched = ? WHERE code = ?").bind(addrs, now, code).run();
        else await env.DB.prepare("UPDATE tag_members SET addrs = ?, touched = ? WHERE code = ? AND id = ?").bind(addrs, now, code, who.member.id).run();
        return reply({});
    }

    if (what === "relay") {
        if (!who.owner) return reply({ error: "bad request" }, 403);
        const login = await relayLogin(env, now);
        return reply(login.server ? { turn: { server: login.server, username: login.username, credential: login.credential } } : { noRelay: login.why });
    }

    if (what === "leave") {
        if (who.owner) {
            await env.DB.batch([
                env.DB.prepare("DELETE FROM tag_members WHERE code = ?").bind(code),
                env.DB.prepare("DELETE FROM tag_rooms WHERE code = ?").bind(code),
            ]);
        } else {
            await env.DB.prepare("DELETE FROM tag_members WHERE code = ? AND id = ?").bind(code, who.member.id).run();
        }
        return reply({});
    }
    return reply({ error: "bad request" }, 404);
}

export default {
    async fetch(request, env) {
        try {
            return await handle(request, env, Math.floor(Date.now() / 1000));
        } catch (e) {
            console.error("tagnet: " + (e && e.message));
            return reply({ error: "unavailable" }, 503);
        }
    },
};

// A room's relay (one Durable Object per room code). Its sockets hibernate between messages: a room costs nothing while
// nobody sends. Tags: "h" the owner, "m<number>" a member.
export class TagRelay {
    constructor(state) {
        this.state = state;
    }
    async fetch(request) {
        const who = request.headers.get("x-tag-who") || "";
        if (!/^(h|m\d{1,3})$/.test(who)) return new Response("bad request", { status: 400 });
        const pair = new WebSocketPair();
        const [client, server] = Object.values(pair);
        for (const old of this.state.getWebSockets(who)) {
            try { old.close(1000, "replaced"); } catch (e) {}
        }
        this.state.acceptWebSocket(server, [who]);
        return new Response(null, { status: 101, webSocket: client });
    }
    webSocketMessage(ws, message) {
        if (typeof message === "string" || message.byteLength < 2 || message.byteLength > 2048) return;
        const tag = this.state.getTags(ws)[0] || "";
        const bytes = new Uint8Array(message);
        // (a socket whose player is gone throws on send: each is tried on its own, so one that is closing never keeps a
        // packet from the others, e.g. a player's new connection beside the old one)
        if (tag === "h") {
            for (const m of this.state.getWebSockets("m" + bytes[0])) {
                try { m.send(bytes.subarray(1)); } catch (e) {}
            }
        } else if (tag.startsWith("m")) {
            const out = new Uint8Array(bytes.length + 1);
            out[0] = Number(tag.slice(1)) & 255;
            out.set(bytes, 1);
            for (const h of this.state.getWebSockets("h")) {
                try { h.send(out); } catch (e) {}
            }
        }
    }
    webSocketClose(ws, code) {
        try { ws.close(code === 1005 ? 1000 : code, "closed"); } catch (e) {}
    }
    webSocketError(ws) {
        try { ws.close(1011, "error"); } catch (e) {}
    }
}
