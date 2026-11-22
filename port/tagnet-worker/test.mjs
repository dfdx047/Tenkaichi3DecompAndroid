// The Worker against a real SQLite database standing in for D1 (Node 22+: node:sqlite). Run: node test.mjs
import { DatabaseSync } from "node:sqlite";
import fs from "node:fs";
import assert from "node:assert/strict";
import { handle } from "./worker.js";

let writes = 0; // rows written (D1's free plan counts them)
const db = new DatabaseSync(":memory:");
db.exec(fs.readFileSync(new URL("./schema.sql", import.meta.url), "utf8"));
const D1 = {
    prepare(sql) {
        return {
            args: [],
            bind(...a) { this.args = a; return this; },
            async first() { return db.prepare(sql).get(...this.args) ?? null; },
            async all() { return { results: db.prepare(sql).all(...this.args) }; },
            async run() { const r = db.prepare(sql).run(...this.args); writes += r.changes; return { meta: { changes: r.changes } }; },
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
console.log("tagnet worker: all tests passed (" + minuteWrites + " rows written in the minute of polls)");
