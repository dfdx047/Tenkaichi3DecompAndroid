// The Worker against a real SQLite database standing in for D1 (Node 22+: node:sqlite). Run: node test.mjs
import { DatabaseSync } from "node:sqlite";
import fs from "node:fs";
import assert from "node:assert/strict";
import { handle } from "./worker.js";

let writes = 0;
let raceNextJoin = false; // rows written (D1's free plan counts them)
const db = new DatabaseSync(":memory:");
db.exec(fs.readFileSync(new URL("./schema.sql", import.meta.url), "utf8"));
const D1 = {
    prepare(sql) {
        return {
            args: [],
            bind(...a) { this.args = a; return this; },
            async first() { return db.prepare(sql).get(...this.args) ?? null; },
            async all() { return { results: db.prepare(sql).all(...this.args) }; },
            async run() {
                if (raceNextJoin && /INTO tag_members/.test(sql)) { // (another join takes the same number between this one's look and its write)
                    raceNextJoin = false;
                    db.prepare("INSERT INTO tag_members (code, id, key, name, addrs, seen, watch, touched) VALUES (?, ?, ?, ?, ?, ?, ?, ?)").run(this.args[0], this.args[1], "other-join-key", "B", "[]", "", 0, now);
                }
                const r = db.prepare(sql).run(...this.args); writes += r.changes; return { meta: { changes: r.changes } };
            },
        };
    },
    async batch(list) { return Promise.all(list.map((s) => s.run())); },
};
let now = 1_800_000_000;
const env = { DB: D1, ALLOWED_VERSIONS: "5" };
async function call(what, body, ip = "203.0.113.1") {
    const r = await handle(new Request("https://t/v1/" + what, { method: "POST", body: JSON.stringify(body), headers: { "cf-connecting-ip": ip } }), env, now);
    return { status: r.status, ...(await r.json()) };
}

let h = await call("host", { v: 4, name: "A", addrs: [] });
assert.equal(h.error, "version");
h = await call("host", { v: 5, name: "Host", addrs: ["192.168.0.2:7001", "198.51.100.7:40000", "bad addr"] });
assert.match(h.code, /^\d{6}$/);
const m1 = await call("join", { v: 5, code: h.code, name: "Guest", addrs: ["10.0.0.3:5000"] }, "198.51.100.9");
assert.equal(m1.id, 1);
assert.deepEqual(m1.host.addrs, ["192.168.0.2:7001", "198.51.100.7:40000"]);
const w = await call("join", { v: 5, code: h.code, name: "Watcher", addrs: [], watch: true }, "198.51.100.10");
assert.equal(w.id, 2);
let p = await call("poll", { code: h.code, key: h.key });
assert.equal(p.members.length, 2);
assert.equal(p.members[0].seen, "198.51.100.9");
assert.equal(p.members[1].watch, true);
assert.equal((await call("join", { v: 6, code: h.code, name: "Old", addrs: [] })).error, "version");
assert.equal((await call("join", { v: 5, code: "000000", name: "X", addrs: [] })).error, "no room");
await call("addrs", { code: h.code, key: h.key, addrs: ["198.51.100.7:40000", "relay=162.159.1.1:3478"] });
p = await call("poll", { code: m1.code ?? h.code, key: m1.key });
assert.deepEqual(p.host.addrs, ["198.51.100.7:40000", "relay=162.159.1.1:3478"]);
assert.match((await call("relay", { code: h.code, key: h.key })).noRelay, /TURN key/);
assert.equal((await call("relay", { code: h.code, key: m1.key })).status, 403);
assert.equal((await call("poll", { code: h.code, key: "nope" })).error, "no room");
// a member who stops polling is gone; a room nobody asks about is gone
now += 50;
p = await call("poll", { code: h.code, key: h.key });
assert.equal(p.members.length, 0);
now += 310; // (310 s after the owner's last poll, above: past ROOM_IDLE)
assert.equal((await call("poll", { code: h.code, key: h.key })).error, "no room");
// the owner leaving closes the room
const h2 = await call("host", { v: 5, name: "H2", addrs: [] });
const g2 = await call("join", { v: 5, code: h2.code, name: "G", addrs: [] });
await call("leave", { code: h2.code, key: h2.key });
assert.equal((await call("poll", { code: h2.code, key: g2.key })).error, "no room");
// a room in use for a minute, both polling every 2 s: the two stay in, and few rows are written
const h3 = await call("host", { v: 5, name: "H3", addrs: [] });
const g3 = await call("join", { v: 5, code: h3.code, name: "G3", addrs: [] });
writes = 0;
for (let t = 0; t < 30; t++) {
    now += 2;
    assert.equal((await call("poll", { code: h3.code, key: g3.key })).status, 200);
    p = await call("poll", { code: h3.code, key: h3.key });
    assert.equal(p.members.length, 1);
}
assert.ok(writes <= 12, "rows written in a minute of polls: " + writes);
const minuteWrites = writes;
// a member who stops polling still leaves the owner's list after MEMBER_IDLE, whatever the sweep's pace
now += 46;
assert.equal((await call("poll", { code: h3.code, key: h3.key })).members.length, 0);
// A phone in the background (C2, 2026-11-22): its app may not poll at all meanwhile (Android freezes a cached app; the
// game polls every 15 s while it can). The owner sharing the code in another app for a few minutes, or a player
// answering a message, must find the room and their place in it when they come back.
{
    const relayEnv = { ...env, RELAY: { idFromName: () => 1, get: () => ({ fetch: async () => new Response("relay", { status: 200 }) }) } };
    const ws = async (code, key) => (await handle(new Request(`https://t/v1/ws?code=${code}&key=${key}`, { headers: { upgrade: "websocket" } }), relayEnv, now)).status;
    for (const away of [100, 110, 280]) { // the owner away: a friend joins meanwhile, the owner comes back to its room
        const o = await call("host", { v: 5, name: "Away", addrs: [] });
        now += 14; await call("poll", { code: o.code, key: o.key }); // (its last poll before it went)
        now += away;
        const f = await call("join", { v: 5, code: o.code, name: "Friend", addrs: [] });
        assert.equal(f.status, 200, `owner away ${away} s: a friend's join`);
        now += 1;
        const back = await call("poll", { code: o.code, key: o.key });
        assert.equal(back.error, undefined, `owner away ${away} s: its poll on its return`);
        assert.equal(back.members.length, 1, `owner away ${away} s: the friend is in its list`);
    }
    { // past the room's limit it is gone, as before
        const o = await call("host", { v: 5, name: "Gone", addrs: [] });
        now += 310;
        assert.equal((await call("join", { v: 5, code: o.code, name: "Late", addrs: [] })).error, "no room");
    }
    for (const away of [50, 120, 280]) { // a player away: its place, its poll and the room's relay are still there
        const o = await call("host", { v: 5, name: "Owner", addrs: [] });
        const g = await call("join", { v: 5, code: o.code, name: "Player", addrs: [] });
        for (let t = 0; t < away; t += 2) { now += 2; await call("poll", { code: o.code, key: o.key }); } // (the owner stays)
        const list = await call("poll", { code: o.code, key: o.key });
        assert.equal(list.members.length, away > 45 ? 0 : 1, `player away ${away} s: out of the owner's list meanwhile`);
        assert.equal((await call("poll", { code: o.code, key: g.key })).error, undefined, `player away ${away} s: its poll on its return`);
        assert.equal(await ws(o.code, g.key), 200, `player away ${away} s: the room's relay takes it again`);
        assert.equal((await call("poll", { code: o.code, key: o.key })).members.length, 1, `player away ${away} s: back in the owner's list`);
    }
    { // owner and player taking turns in the background for 20 minutes: the room and both places live on
        const o = await call("host", { v: 5, name: "Turns", addrs: [] });
        const g = await call("join", { v: 5, code: o.code, name: "Turns2", addrs: [] });
        for (let round = 0; round < 12; round++) {
            now += 100;
            assert.equal((await call("poll", { code: o.code, key: round % 2 ? o.key : g.key })).error, undefined, `turns, round ${round}`);
        }
        assert.equal((await call("poll", { code: o.code, key: o.key })).error, undefined);
        assert.equal((await call("poll", { code: o.code, key: g.key })).error, undefined);
    }
}
// Member numbers travel as one byte through the room's relay (TagRelay: the owner's messages start with the member's
// number & 255, tag_link.c answers by it). A long-lived room whose newest member always stays (people coming and going
// around one who stays) went past 255: member 256 got the owner's answers for nobody (its byte is 0) and 257 shared
// member 1's. Numbers stay within 1..255 and unique among the room's members.
{
    const o = await call("host", { v: 5, name: "Long", addrs: [] });
    let last = await call("join", { v: 5, code: o.code, name: "M", addrs: [] });
    const ids = [last.id];
    for (let k = 0; k < 300; k++) { // the next one arrives, then the one before leaves
        now += 1;
        const next = await call("join", { v: 5, code: o.code, name: "M" + k, addrs: [] });
        assert.equal(next.status, 200, `join ${k}`);
        ids.push(next.id);
        await call("leave", { code: o.code, key: last.key });
        last = next;
        await call("poll", { code: o.code, key: o.key }); // (the owner stays)
    }
    assert.ok(ids.every((id) => id >= 1 && id <= 255), "member numbers stay within one byte: largest " + Math.max(...ids));
    assert.ok(ids.includes(254) && ids.includes(255), "numbers up to 255 are given");
    assert.ok(ids[ids.indexOf(255) + 1] < 255, "after 255, the smallest free number: " + ids[ids.indexOf(255) + 1]);
    // the members present at once all have different bytes
    const together = [];
    for (let k = 0; k < 10; k++) { now += 1; together.push((await call("join", { v: 5, code: o.code, name: "T" + k, addrs: [] })).id); }
    together.push(last.id);
    assert.equal(new Set(together.map((id) => id & 255)).size, together.length, "members present together share a relay byte: " + together.join(","));
    assert.ok(together.every((id) => id >= 1 && id <= 255), "members present together within one byte: " + together.join(","));
}
{ // two joins at once want the same next number: the one that finds it taken takes another instead of failing
    const o = await call("host", { v: 5, name: "Race", addrs: [] });
    const first = await call("join", { v: 5, code: o.code, name: "A", addrs: [] });
    raceNextJoin = true;
    let second;
    try { second = await call("join", { v: 5, code: o.code, name: "C", addrs: [] }); } catch (e) { second = { status: "exception: " + e.message }; }
    assert.equal(second.status, 200, "a join whose number was just taken");
    assert.notEqual(second.id, first.id + 1);
    assert.ok(second.id >= 1 && second.id <= 255);
}
// Signed requests (keyGate): REQUIRE_KEY off / count / on, as the live service runs them. The signature is the game's
// (net_sign.c): HMAC-SHA256 with the release's key over "BT3-SIGN-1", the method, the path, the release, the time and the
// body's SHA-256, one per line.
{
    const KEY = "k".repeat(32), hex = (b) => [...new Uint8Array(b)].map((x) => x.toString(16).padStart(2, "0")).join("");
    const sign = async (path, bodyText, time, release = "139", key = KEY) => {
        const digest = hex(await crypto.subtle.digest("SHA-256", new TextEncoder().encode(bodyText)));
        const k = await crypto.subtle.importKey("raw", new TextEncoder().encode(key), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
        return hex(await crypto.subtle.sign("HMAC", k, new TextEncoder().encode(["BT3-SIGN-1", "POST", path, release, String(time), digest].join("\n"))));
    };
    const ask = async (mode, what, bodyObj, headers = {}, keys = { 139: KEY }) => {
        const e = { ...env, REQUIRE_KEY: mode, BUILD_KEYS: JSON.stringify(keys) };
        if (mode === undefined) delete e.REQUIRE_KEY;
        const r = await handle(new Request("https://t/v1/" + what, { method: "POST", body: JSON.stringify(bodyObj), headers: { "cf-connecting-ip": "203.0.113.50", ...headers } }), e, now);
        return { status: r.status, ...(await r.json()) };
    };
    const rows = () => db.prepare("SELECT what, n FROM tag_key_days ORDER BY what").all().map((r) => `${r.what}=${r.n}`).join(",");
    const signed = async (what, bodyObj, over = {}) => {
        const text = JSON.stringify(bodyObj), time = over.time ?? now;
        return { "x-bt3-release": over.release ?? "139", "x-bt3-time": String(time), "x-bt3-sign": over.sign ?? await sign("/v1/" + what, text, time, over.release ?? "139", over.key ?? KEY) };
    };
    db.exec("DELETE FROM tag_key_days");
    const body = { v: 5, name: "K", addrs: [] };
    assert.equal((await ask(undefined, "host", body)).status, 200, "no REQUIRE_KEY: nothing checked");
    assert.equal((await ask("off", "host", body)).status, 200);
    assert.equal(rows(), "", "off: nothing counted");
    assert.equal((await ask("count", "host", body)).status, 200, "count: an unsigned host is let in");
    assert.equal((await ask("count", "host", body, await signed("host", body))).status, 200);
    assert.equal((await ask("count", "host", body, await signed("host", body, { sign: "0".repeat(64) }))).status, 200, "count: a wrong signature is let in");
    assert.equal(rows(), "none host v5 ??=1,ok host v5 ??=1,sign host v5 ??=1", "count: counted by result");
    assert.equal((await ask("on", "host", body)).error, "official", "on: unsigned refused");
    assert.equal((await ask("on", "host", body, await signed("host", body, { sign: "0".repeat(64) }))).error, "official", "on: a wrong signature refused");
    assert.equal((await ask("on", "host", body, await signed("host", body, { release: "140" }))).error, "official", "on: an unknown release refused");
    assert.equal((await ask("on", "host", body, await signed("host", body, { key: "x".repeat(32) }))).error, "official", "on: another key refused");
    assert.equal((await ask("on", "host", body, { "x-bt3-release": "139", "x-bt3-time": "abc", "x-bt3-sign": "f".repeat(64) })).error, "official", "on: a malformed time refused");
    const late = await ask("on", "host", body, await signed("host", body, { time: now - 700 }));
    assert.equal(late.status, 403); assert.equal(late.error, "time"); assert.equal(late.now, now, "on: an old time is told the Worker's time");
    const ok = await ask("on", "host", body, await signed("host", body));
    assert.equal(ok.status, 200, "on: a signed request is let in"); assert.ok(ok.code && ok.key);
    const join = { v: 5, code: ok.code, name: "J", addrs: [] };
    assert.equal((await ask("on", "join", join)).error, "official", "on: an unsigned join refused");
    assert.equal((await ask("on", "join", join, await signed("join", join))).status, 200, "on: a signed join let in");
    assert.equal((await ask("on", "poll", { code: ok.code, key: ok.key })).status, 200, "on: polls are not signed and are let in");
    assert.equal((await ask("on", "hello", { v: 5 })).ok, true);
    assert.equal((await ask("on", "hello", { v: 5 })).now, now, "hello says the Worker's time");
    assert.equal((await ask("on", "host", body, await signed("host", body), "{}")).error, "official", "on: no keys at all refuses");
    assert.equal((await ask("on", "host", body, await signed("host", body), { 139: "short" })).error, "official", "on: a key under 16 characters is not taken");
    assert.equal((await ask("bogus", "host", body)).status, 200, "an unknown mode is off");
    // the counting never stops a request: a database without the table
    db.exec("ALTER TABLE tag_key_days RENAME TO tag_key_days_away");
    assert.equal((await ask("count", "host", body)).status, 200, "count without its table: the request still goes through");
    db.exec("ALTER TABLE tag_key_days_away RENAME TO tag_key_days");
}
{
// the quick 2 vs 2: four tickets make a room of two teams; fewer wait; a ticket that stops polling is dropped
const q = [];
for (let i = 0; i < 3; i++) {
    const r = await call("quick", { v: 5, name: "P" + i, addrs: ["10.0.0." + i + ":5000"] }, "198.51.100." + (20 + i));
    assert.equal(r.state, "waiting");
    assert.equal(r.waiting, i + 1);
    q.push(r);
}
assert.equal((await call("quick", { v: 4, name: "Old", addrs: [] }, "198.51.100.30")).error, "version");
let qp = await call("quickpoll", { ticket: q[0].ticket, key: q[0].key }, "198.51.100.20");
assert.equal(qp.state, "waiting");
assert.equal((await call("quickpoll", { ticket: q[0].ticket, key: "nope" })).error, "no ticket");
const last = await call("quick", { v: 5, name: "P3", addrs: ["10.0.0.3:5000"] }, "198.51.100.23");
q.push(last);
assert.equal(last.state, "matched");
const polled = [];
for (const t of q) polled.push(await call("quickpoll", { ticket: t.ticket, key: t.key }));
for (const p of polled) assert.equal(p.state, "matched", JSON.stringify(p));
assert.equal(new Set(polled.map((p) => p.code)).size, 1);
assert.deepEqual(polled.map((p) => p.role).sort(), [0, 1, 2, 3]);
assert.equal(polled.filter((p) => p.team === 0).length, 2);
assert.equal(polled.filter((p) => p.team === 1).length, 2);
assert.equal(polled[0].players.length, 4);
const owner = polled.find((p) => p.role === 0);
assert.equal(owner.id, 0);
assert.deepEqual((await call("poll", { code: owner.code, key: owner.roomKey })).members.map((m) => m.id), [1, 2, 3]); // (the others are its room's members)
const mate = polled.find((p) => p.role === 3);
assert.equal((await call("poll", { code: mate.code, key: mate.roomKey })).host.name, owner.players.find((x) => x.role === 0).name);
// the line goes on: a fifth waits alone; a ticket nobody polls is dropped
const fifth = await call("quick", { v: 5, name: "P4", addrs: [] }, "198.51.100.24");
assert.equal(fifth.state, "waiting");
assert.equal(fifth.waiting, 1);
now += 15; // (within QUICK_IDLE: this must not depend on when the last sweep happened to run)
assert.equal((await call("quickpoll", { ticket: fifth.ticket, key: fifth.key })).state, "waiting"); // (asked in time: it is kept: touched)
now += 70; // (past QUICK_IDLE and SWEEP_EVERY)
assert.equal((await call("quickpoll", { ticket: fifth.ticket, key: fifth.key })).error, "no ticket");
const left = await call("quick", { v: 5, name: "P5", addrs: [] }, "198.51.100.25");
await call("quickleave", { ticket: left.ticket, key: left.key });
assert.equal((await call("quickpoll", { ticket: left.ticket, key: left.key })).error, "no ticket");
// one address does not fill a line alone
for (let i = 0; i < 2; i++) assert.equal((await call("quick", { v: 5, name: "S" + i, addrs: [] }, "203.0.113.99")).state, "waiting");
assert.equal((await call("quick", { v: 5, name: "S2", addrs: [] }, "203.0.113.99")).error, "busy");
}
console.log("tagnet worker: all tests passed (" + minuteWrites + " rows written in the minute of polls)");
