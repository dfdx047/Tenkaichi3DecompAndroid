// Z Warriors Online: one against one, each with a warrior of their own (Dragon Rage, Android). A Cloudflare Worker with
// a D1 database (schema.sql). It never carries the fight: it keeps a room (code, rules, the two players' addresses and
// warriors' share codes, who is ready) and, when the two phones cannot reach each other, a relay for them.
//
// POST /v1/<what>, JSON body; answers JSON ({error} with a 4xx status). v: the app's network version; disc: the game
// disc's fingerprint; both must be equal for two players to meet. warrior: the "ZW1:..." share code.
//   hello      {v}                                         -> {ok, now}
//   host       {v, disc, name, addrs, warrior, rules}      -> {code, key, you}
//   join       {v, disc, code, name, addrs, warrior}       -> {key, you, rules, peer}   (no room, version, disc, full)
//   poll       {code, key, have}                           -> {peer, rules, round}      peer: {name, addrs, seen, ready,
//                                                              rev, warrior (only when rev != have)} or null
//   ready      {code, key, ready}                          -> {}
//   warrior    {code, key, warrior}                        -> {rev}      (both are no longer ready)
//   rules      {code, key, rules}                          -> {}         (the host only; the guest is no longer ready)
//   round      {code, key}                                 -> {round}    (the host: a fight ended, nobody ready)
//   addrs      {code, key, addrs}                          -> {}
//   relay      {code, key}                                 -> {turn} | {noRelay}
//   leave      {code, key}                                 -> {}         (the host leaving closes the room)
//   quick      {v, disc, name, addrs, warrior, made, madeOk} -> {ticket, key, state: "waiting"} or {state: "matched", code,
//                  roomKey, host: bool, rules, peer}. made: the warrior is a created one; madeOk: created rivals accepted.
//   quickpoll, quickleave {ticket, key}
//   GET /v1/ws?code=&key=   WebSocket relay of the room (Durable Object ZwRelay, binding RELAY)
// Settings: DB; secrets TURN_KEY_ID, TURN_KEY_TOKEN (or METERED_APP, METERED_KEY), BUILD_KEYS; vars REQUIRE_KEY
// (off | count | on), TURN_DAILY_MAX, ALLOWED_VERSIONS.

const ROOM_IDLE = 300;    // seconds a room lives without a poll of its host
const GUEST_IDLE = 60;    // seconds without a poll before the guest's place is free
const TOUCH_EVERY = 15;   // (D1's free plan counts rows written: times are written only this often)
const SWEEP_EVERY = 30;
const QUICK_IDLE = 20;
const QUICK_KEEP = 90;
const ROOMS_PER_IP = 4;
const BODY_MAX = 20000;
const RELAY_PER_DAY = 500;
// the ranked server's (bt4online-worker/rooms.js) limits against abuse, for one address in an hour
const WRONG_CODES_PER_HOUR = 30; // codes that name no room
const HOSTS_PER_HOUR = 20;
const QUICK_PER_HOUR = 60;       // searches begun
let lastSweep = 0;

function reply(obj, status = 200) {
    return new Response(JSON.stringify(obj), { status, headers: { "content-type": "application/json" } });
}
const cleanName = (s) => (typeof s === "string" ? s.replace(/[\u0000-\u001f"\\]/g, "").slice(0, 24) : "");
const cleanAddrs = (a) => (Array.isArray(a) ? a.filter((x) => typeof x === "string" && /^(relay=)?[0-9a-fA-F.:]{3,60}$/.test(x)).slice(0, 8) : []);
const cleanWarrior = (s) => (typeof s === "string" && s.length <= 16384 && /^ZW1:[ -~]+$/.test(s) ? s : null);
const cleanDisc = (s) => (typeof s === "string" && /^[0-9A-Za-z_.-]{1,64}$/.test(s) ? s : null);
// rules: stage (-1 drawn), time, items, health, who may join, fighters allowed: six small integers
const cleanRules = (r) => (Array.isArray(r) && r.length === 6 && r.every((x) => Number.isInteger(x) && x >= -1 && x < 256) ? r : [-1, 0, 0, 0, 0, 0]);

function versionAllowed(env, v) {
    if (!Number.isInteger(v)) return false;
    const list = String(env.ALLOWED_VERSIONS ?? "").split(",").map((x) => Number.parseInt(x, 10)).filter(Number.isInteger);
    return list.length === 0 || list.includes(v);
}
function randomHex() {
    return [...crypto.getRandomValues(new Uint8Array(16))].map((x) => x.toString(16).padStart(2, "0")).join("");
}
const ABC = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // (32: no 0/O, 1/I)
const randomCode = () => [...crypto.getRandomValues(new Uint8Array(6))].map((x) => ABC[x & 31]).join("");

// What an address did this hour (stored hashed, not the address itself). add 0: only read.
async function hourly(env, what, ip, now, add) {
    const h = await crypto.subtle.digest("SHA-256", new TextEncoder().encode("zw\n" + ip));
    const who = [...new Uint8Array(h, 0, 12)].map((x) => x.toString(16).padStart(2, "0")).join("");
    const hour = now - (now % 3600);
    const row = add
        ? await env.DB.prepare("INSERT INTO zw_limits (hour, what, who, n) VALUES (?, ?, ?, 1) ON CONFLICT (hour, what, who) DO UPDATE SET n = n + 1 RETURNING n").bind(hour, what, who).first()
        : await env.DB.prepare("SELECT n FROM zw_limits WHERE hour = ? AND what = ? AND who = ?").bind(hour, what, who).first();
    return row ? row.n : 0;
}
async function cleanup(env, now) {
    lastSweep = 0;
    await sweep(env, now);
    await env.DB.batch([
        env.DB.prepare("DELETE FROM zw_limits WHERE hour < ?").bind(now - 7200),
        env.DB.prepare("DELETE FROM zw_relay_days WHERE day < ?").bind(now - 40 * 86400),
        env.DB.prepare("DELETE FROM zw_key_days WHERE day < ?").bind(now - 90 * 86400),
    ]);
}

async function sweep(env, now) {
    if (now >= lastSweep && now - lastSweep < SWEEP_EVERY) return;
    lastSweep = now;
    await env.DB.batch([
        env.DB.prepare("DELETE FROM zw_rooms WHERE touched < ?").bind(now - ROOM_IDLE),
        env.DB.prepare("DELETE FROM zw_players WHERE code NOT IN (SELECT code FROM zw_rooms)"),
        env.DB.prepare("DELETE FROM zw_quick WHERE (code IS NULL AND touched < ?) OR (code IS NOT NULL AND touched < ?)").bind(now - QUICK_IDLE, now - QUICK_KEEP),
    ]);
}

function addPlayer(env, code, slot, key, p, ip, now) {
    return env.DB.prepare("INSERT OR REPLACE INTO zw_players (code, slot, key, name, addrs, seen, warrior, rev, ready, touched) VALUES (?, ?, ?, ?, ?, ?, ?, 1, 0, ?)")
        .bind(code, slot, key, p.name, p.addrs, ip, p.warrior, now);
}
async function newRoom(env, v, disc, rules, ip, now) {
    for (let tries = 0; tries < 8; tries++) {
        const code = randomCode();
        const r = await env.DB.prepare("INSERT OR IGNORE INTO zw_rooms (code, v, disc, rules, round, ip, created, touched) VALUES (?, ?, ?, ?, 0, ?, ?, ?)")
            .bind(code, v, disc, JSON.stringify(rules), ip, now, now).run();
        if (r.meta.changes === 1) return code;
    }
    return null;
}
async function peerOf(env, code, slot, have, now) {
    const p = await env.DB.prepare("SELECT * FROM zw_players WHERE code = ? AND slot = ?").bind(code, 1 - slot).first();
    if (!p || (p.slot === 1 && p.touched < now - GUEST_IDLE)) return null;
    const out = { name: p.name, addrs: JSON.parse(p.addrs), seen: p.seen, ready: !!p.ready, rev: p.rev };
    if (p.rev !== have) out.warrior = p.warrior;
    return out;
}

// Two waiting tickets of the same version and disc that accept each other make a room; the older one hosts it.
async function quickMatch(env, t, now) {
    const other = await env.DB.prepare(
        "SELECT * FROM zw_quick WHERE code IS NULL AND ticket != ? AND v = ? AND disc = ? AND touched >= ? AND (made = 0 OR ? = 1) AND (? = 0 OR made_ok = 1) ORDER BY created LIMIT 1")
        .bind(t.ticket, t.v, t.disc, now - QUICK_IDLE, t.made_ok, t.made).first();
    if (!other) return;
    const pair = other.created <= t.created ? [other, t] : [t, other];
    const code = await newRoom(env, t.v, t.disc, [-1, 0, 0, 0, 0, 0], pair[0].seen, now);
    if (!code) return;
    const writes = [];
    pair.forEach((p, slot) => {
        const key = randomHex();
        writes.push(addPlayer(env, code, slot, key, p, p.seen, now));
        writes.push(env.DB.prepare("UPDATE zw_quick SET code = ?, mkey = ?, slot = ?, touched = ? WHERE ticket = ?").bind(code, key, slot, now, p.ticket));
    });
    await env.DB.batch(writes);
}
async function quickState(env, ticket, now) {
    const t = await env.DB.prepare("SELECT * FROM zw_quick WHERE ticket = ?").bind(ticket).first();
    if (!t.code) {
        const n = await env.DB.prepare("SELECT COUNT(*) AS n FROM zw_quick WHERE code IS NULL AND v = ? AND touched >= ?").bind(t.v, now - QUICK_IDLE).first();
        return { state: "waiting", waiting: n.n };
    }
    const room = await env.DB.prepare("SELECT rules FROM zw_rooms WHERE code = ?").bind(t.code).first();
    if (!room) return { state: "gone" };
    return { state: "matched", code: t.code, roomKey: t.mkey, host: t.slot === 0, rules: JSON.parse(room.rules), peer: await peerOf(env, t.code, t.slot, 0, now) };
}

async function whoIs(env, code, key, now) {
    if (typeof code !== "string" || typeof key !== "string") return null;
    const room = await env.DB.prepare("SELECT * FROM zw_rooms WHERE code = ?").bind(code.toUpperCase()).first();
    if (!room || room.touched < now - ROOM_IDLE) return null;
    const me = await env.DB.prepare("SELECT * FROM zw_players WHERE code = ? AND key = ?").bind(room.code, key).first();
    return me ? { room, me } : null;
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
    const row = await env.DB.prepare("INSERT INTO zw_relay_days (day, n) VALUES (?, 1) ON CONFLICT(day) DO UPDATE SET n = n + 1 RETURNING n").bind(day).first();
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
// "off" (nothing is checked), "count" (every host and join is counted by result, country and version in zw_key_days,
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
        await env.DB.prepare("INSERT INTO zw_key_days (day, what, n) VALUES (?, ?, 1) ON CONFLICT (day, what) DO UPDATE SET n = n + 1").bind(now - now % 86400, `${why ?? "ok"} ${what} v${v} ${country}`).run();
    } catch (e) {
    }
    if (why === null || mode !== "on") return null;
    if (why === "time") return reply({ error: "time", now }, 403);
    return reply({ error: "official" }, 403);
}
export async function handle(request, env, now) {
    const url = new URL(request.url);
    if (request.method === "GET" && url.pathname === "/") {
        return new Response("Z Warriors Online: rooms for Dragon Rage.\n", { headers: { "content-type": "text/plain; charset=utf-8" } });
    }
    if (request.method === "GET" && url.pathname === "/v1/ws") {
        if (request.headers.get("upgrade") !== "websocket") return reply({ error: "bad request" }, 426);
        if (!env.RELAY) return reply({ error: "no relay" }, 503);
        const who = await whoIs(env, url.searchParams.get("code"), url.searchParams.get("key"), now);
        if (!who) return reply({ error: "no room" }, 404);
        const headers = new Headers(request.headers);
        headers.set("x-tag-who", who.me.slot === 0 ? "h" : "m1");
        const hints = { SA: "sam", NA: "enam", EU: "weur", AS: "apac", OC: "oc", AF: "afr" };
        const hint = hints[(request.cf && request.cf.continent) || ""] || "sam";
        return env.RELAY.get(env.RELAY.idFromName(who.room.code), { locationHint: hint }).fetch(new Request(request.url, { headers }));
    }
    if (request.method !== "POST" || !url.pathname.startsWith("/v1/")) return reply({ error: "bad request" }, 404);
    let body, raw;
    try {
        raw = await request.arrayBuffer();
        if (raw.byteLength > BODY_MAX) return reply({ error: "bad request" }, 413);
        body = JSON.parse(new TextDecoder().decode(raw));
    } catch (e) {
        return reply({ error: "bad request" }, 400);
    }
    if (!body || typeof body !== "object" || Array.isArray(body)) return reply({ error: "bad request" }, 400);
    const what = url.pathname.slice(4);
    const ip = request.headers.get("cf-connecting-ip") ?? "";
    if (what === "hello") return reply({ ok: versionAllowed(env, body.v), now });
    await sweep(env, now);

    const entering = what === "host" || what === "join" || (what === "quick" && !body.ticket);
    let mine = null;
    if (entering) {
        const refused = await keyGate(request, env, raw, body, now, what);
        if (refused) return refused;
        if (!versionAllowed(env, body.v)) return reply({ error: "version" }, 409);
        mine = { name: cleanName(body.name), addrs: JSON.stringify(cleanAddrs(body.addrs)), warrior: cleanWarrior(body.warrior), disc: cleanDisc(body.disc) };
        if (!mine.warrior || !mine.disc) return reply({ error: "bad request" }, 400);
    }

    if (what === "host") {
        if ((await hourly(env, "host", ip, now, 1)) > HOSTS_PER_HOUR) return reply({ error: "busy" }, 429);
        const open = await env.DB.prepare("SELECT COUNT(*) AS n FROM zw_rooms WHERE ip = ? AND touched >= ?").bind(ip, now - ROOM_IDLE).first();
        if (open.n >= ROOMS_PER_IP) return reply({ error: "busy" }, 429);
        const code = await newRoom(env, body.v, mine.disc, cleanRules(body.rules), ip, now);
        if (!code) return reply({ error: "busy" }, 503);
        const key = randomHex();
        await addPlayer(env, code, 0, key, mine, ip, now).run();
        return reply({ code, key, you: ip });
    }
    if (what === "join") {
        if ((await hourly(env, "wrong", ip, now, 0)) >= WRONG_CODES_PER_HOUR) return reply({ error: "busy" }, 429);
        const room = await env.DB.prepare("SELECT * FROM zw_rooms WHERE code = ?").bind(String(body.code ?? "").toUpperCase()).first();
        if (!room || room.touched < now - ROOM_IDLE) {
            await hourly(env, "wrong", ip, now, 1);
            return reply({ error: "no room" }, 404);
        }
        if (room.v !== body.v) return reply({ error: "version" }, 409);
        if (room.disc !== mine.disc) return reply({ error: "disc" }, 409);
        const key = randomHex();
        // the guest's place: free, or left quiet (one statement, so two joining at once cannot both take it)
        const r = await env.DB.prepare(
            "INSERT INTO zw_players (code, slot, key, name, addrs, seen, warrior, rev, ready, touched) VALUES (?, 1, ?, ?, ?, ?, ?, 1, 0, ?) " +
            "ON CONFLICT (code, slot) DO UPDATE SET key = excluded.key, name = excluded.name, addrs = excluded.addrs, seen = excluded.seen, warrior = excluded.warrior, rev = zw_players.rev + 1, ready = 0, touched = excluded.touched WHERE zw_players.touched < ?")
            .bind(room.code, key, mine.name, mine.addrs, ip, mine.warrior, now, now - GUEST_IDLE).run();
        if (r.meta.changes !== 1) return reply({ error: "full" }, 409);
        return reply({ key, you: ip, rules: JSON.parse(room.rules), peer: await peerOf(env, room.code, 1, 0, now) });
    }
    if (what === "quick" || what === "quickpoll" || what === "quickleave") {
        let t = typeof body.ticket === "string" && typeof body.key === "string"
            ? await env.DB.prepare("SELECT * FROM zw_quick WHERE ticket = ? AND key = ?").bind(body.ticket, body.key).first() : null;
        if (!t && what !== "quick") return reply({ error: "no ticket" }, 404);
        if (what === "quickleave") {
            await env.DB.prepare("DELETE FROM zw_quick WHERE ticket = ?").bind(t.ticket).run();
            return reply({});
        }
        let key = body.key;
        if (!t) {
            if (!mine) return reply({ error: "no ticket" }, 404);
            if ((await hourly(env, "quick", ip, now, 1)) > QUICK_PER_HOUR) return reply({ error: "busy" }, 429);
            const open = await env.DB.prepare("SELECT COUNT(*) AS n FROM zw_quick WHERE seen = ? AND code IS NULL AND touched >= ?").bind(ip, now - QUICK_IDLE).first();
            if (open.n >= 2) return reply({ error: "busy" }, 429);
            const ticket = randomHex();
            key = randomHex();
            await env.DB.prepare("INSERT INTO zw_quick (ticket, key, v, disc, name, addrs, seen, warrior, made, made_ok, created, touched) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                .bind(ticket, key, body.v, mine.disc, mine.name, mine.addrs, ip, mine.warrior, body.made ? 1 : 0, body.madeOk ? 1 : 0, now, now).run();
            t = await env.DB.prepare("SELECT * FROM zw_quick WHERE ticket = ?").bind(ticket).first();
        } else if (!t.code && now - t.touched >= TOUCH_EVERY / 2) {
            await env.DB.prepare("UPDATE zw_quick SET touched = ? WHERE ticket = ?").bind(now, t.ticket).run();
        }
        if (!t.code) await quickMatch(env, t, now);
        return reply({ ticket: t.ticket, key, ...(await quickState(env, t.ticket, now)) });
    }

    const who = await whoIs(env, body.code, body.key, now);
    if (!who) return reply({ error: "no room" }, 404);
    const { room, me } = who;
    const code = room.code, host = me.slot === 0;
    const unready = (slot) => env.DB.prepare("UPDATE zw_players SET ready = 0 WHERE code = ?" + (slot === undefined ? "" : " AND slot = " + slot)).bind(code);

    if (what === "poll") {
        const writes = [];
        if (now - me.touched >= TOUCH_EVERY || me.seen !== ip) writes.push(env.DB.prepare("UPDATE zw_players SET touched = ?, seen = ? WHERE code = ? AND slot = ?").bind(now, ip, code, me.slot));
        if (host && now - room.touched >= TOUCH_EVERY) writes.push(env.DB.prepare("UPDATE zw_rooms SET touched = ? WHERE code = ?").bind(now, code));
        if (writes.length) await env.DB.batch(writes);
        return reply({ peer: await peerOf(env, code, me.slot, body.have, now), rules: JSON.parse(room.rules), round: room.round });
    }
    if (what === "ready") {
        await env.DB.prepare("UPDATE zw_players SET ready = ?, touched = ? WHERE code = ? AND slot = ?").bind(body.ready ? 1 : 0, now, code, me.slot).run();
        return reply({});
    }
    if (what === "warrior") {
        const w = cleanWarrior(body.warrior);
        if (!w) return reply({ error: "bad request" }, 400);
        await env.DB.batch([env.DB.prepare("UPDATE zw_players SET warrior = ?, rev = rev + 1, touched = ? WHERE code = ? AND slot = ?").bind(w, now, code, me.slot), unready()]);
        return reply({ rev: me.rev + 1 });
    }
    if (what === "rules" || what === "round") {
        if (!host) return reply({ error: "bad request" }, 403);
        if (what === "rules") {
            await env.DB.batch([env.DB.prepare("UPDATE zw_rooms SET rules = ?, touched = ? WHERE code = ?").bind(JSON.stringify(cleanRules(body.rules)), now, code), unready(1)]);
            return reply({});
        }
        await env.DB.batch([env.DB.prepare("UPDATE zw_rooms SET round = round + 1, touched = ? WHERE code = ?").bind(now, code), unready()]);
        return reply({ round: room.round + 1 });
    }
    if (what === "addrs") {
        await env.DB.prepare("UPDATE zw_players SET addrs = ?, touched = ? WHERE code = ? AND slot = ?").bind(JSON.stringify(cleanAddrs(body.addrs)), now, code, me.slot).run();
        return reply({});
    }
    if (what === "relay") {
        const login = await relayLogin(env, now);
        return reply(login.server ? { turn: { server: login.server, username: login.username, credential: login.credential } } : { noRelay: login.why });
    }
    if (what === "leave") {
        if (host) {
            await env.DB.batch([env.DB.prepare("DELETE FROM zw_players WHERE code = ?").bind(code), env.DB.prepare("DELETE FROM zw_rooms WHERE code = ?").bind(code)]);
        } else {
            await env.DB.batch([env.DB.prepare("DELETE FROM zw_players WHERE code = ? AND slot = 1").bind(code), unready(0)]);
        }
        return reply({});
    }
    return reply({ error: "bad request" }, 404);
}

export default {
    async scheduled(event, env) {
        await cleanup(env, Math.floor(Date.now() / 1000));
    },
    async fetch(request, env) {
        try {
            return await handle(request, env, Math.floor(Date.now() / 1000));
        } catch (e) {
            console.error("zwnet: " + (e && e.message));
            return reply({ error: "unavailable" }, 503);
        }
    },
};

// A room's relay (one Durable Object per room code). Its sockets hibernate between messages: a room costs nothing while
// nobody sends. Tags: "h" the owner, "m<number>" a member.
export class ZwRelay {
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
