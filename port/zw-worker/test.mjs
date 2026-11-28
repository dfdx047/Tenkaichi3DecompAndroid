// The Worker against SQLite standing in for D1 (Node 22+). Run: node test.mjs
import { DatabaseSync } from "node:sqlite";
import fs from "node:fs";
import assert from "node:assert/strict";
import worker, { handle } from "./worker.js";

const db = new DatabaseSync(":memory:");
db.exec(fs.readFileSync(new URL("./schema.sql", import.meta.url), "utf8"));
const D1 = {
    prepare(sql) {
        return {
            args: [], bind(...a) { this.args = a; return this; },
            async first() { return db.prepare(sql).get(...this.args) ?? null; },
            async all() { return { results: db.prepare(sql).all(...this.args) }; },
            async run() { return { meta: { changes: db.prepare(sql).run(...this.args).changes } }; },
        };
    },
    async batch(list) { const out = []; for (const s of list) out.push(await s.run()); return out; },
};
let now = 1_800_000_000;
const env = { DB: D1 };
async function call(what, body, ip = "203.0.113.1") {
    const r = await handle(new Request("https://t/v1/" + what, { method: "POST", body: JSON.stringify(body), headers: { "cf-connecting-ip": ip } }), env, now);
    return { status: r.status, ...(await r.json()) };
}
const me = (w, extra = {}) => ({ v: 1, disc: "SLUS-21678", name: w, addrs: ["10.0.0.1:5000"], warrior: "ZW1:" + w, ...extra });

// private room: both receive the other's warrior, ready flags, rules, rematch, leaving
const h = await call("host", me("A", { rules: [3, 0, 0, 0, 0, 0] }));
assert.match(h.code, /^[A-Z2-9]{6}$/);
assert.equal((await call("join", me("B", { code: h.code, disc: "other" }))).error, "disc");
assert.equal((await call("join", me("B", { code: h.code, v: 2 }))).error, "version");
const g = await call("join", me("B", { code: h.code.toLowerCase() }), "198.51.100.2");
assert.equal(g.peer.warrior, "ZW1:A");
assert.deepEqual(g.rules, [3, 0, 0, 0, 0, 0]);
assert.equal((await call("join", me("C", { code: h.code }))).error, "full");
let p = await call("poll", { code: h.code, key: h.key, have: 0 });
assert.equal(p.peer.warrior, "ZW1:B");
p = await call("poll", { code: h.code, key: h.key, have: p.peer.rev });
assert.equal(p.peer.warrior, undefined);
await call("ready", { code: h.code, key: g.key, ready: true });
assert.equal((await call("poll", { code: h.code, key: h.key })).peer.ready, true);
await call("rules", { code: h.code, key: h.key, rules: [5, 1, 0, 0, 0, 0] });
assert.equal((await call("poll", { code: h.code, key: h.key })).peer.ready, false);
assert.equal((await call("rules", { code: h.code, key: g.key, rules: [] })).status, 403);
await call("warrior", { code: h.code, key: g.key, warrior: "ZW1:B2" });
assert.equal((await call("poll", { code: h.code, key: h.key, have: 1 })).peer.warrior, "ZW1:B2");
assert.equal((await call("round", { code: h.code, key: h.key })).round, 1);
assert.match((await call("relay", { code: h.code, key: g.key })).noRelay, /TURN key/);
await call("leave", { code: h.code, key: g.key });
assert.equal((await call("poll", { code: h.code, key: h.key })).peer, null);
await call("leave", { code: h.code, key: h.key });
assert.equal((await call("poll", { code: h.code, key: h.key })).error, "no room");
assert.equal((await call("host", me("A", { warrior: "hello" }))).status, 400);

// quick match: created warriors only meet who accepts them
const a = await call("quick", me("Q1", { made: true, madeOk: true }), "1.1.1.1");
assert.equal(a.state, "waiting");
const b = await call("quick", me("Q2", { made: false, madeOk: false }), "2.2.2.2");
assert.equal(b.state, "waiting");
const c = await call("quick", me("Q3", { made: true, madeOk: true }), "3.3.3.3");
assert.equal(c.state, "matched");
assert.equal(c.host, false);
assert.equal(c.peer.warrior, "ZW1:Q1");
const a2 = await call("quickpoll", { ticket: a.ticket, key: a.key });
assert.equal(a2.host, true);
assert.equal(a2.code, c.code);
assert.equal(a2.peer.warrior, "ZW1:Q3");
await call("quickleave", { ticket: b.ticket, key: b.key });
assert.equal((await call("quickpoll", { ticket: b.ticket, key: b.key })).error, "no ticket");

// idle rooms are gone
const z = await call("host", me("Z"));
now += 400;
assert.equal((await call("poll", { code: z.code, key: z.key })).error, "no room");

// hourly limits, and the cron's cleanup
for (let i = 0; i < 30; i++) await call("join", me("W", { code: "AAAAAA" }), "9.9.9.9");
assert.equal((await call("join", me("W", { code: "AAAAAA" }), "9.9.9.9")).status, 429);
for (let i = 0; i < 20; i++) { const r = await call("host", me("H"), "8.8.8.8"); await call("leave", { code: r.code, key: r.key }); }
assert.equal((await call("host", me("H"), "8.8.8.8")).status, 429);
await worker.scheduled({}, env);

// signed requests
env.REQUIRE_KEY = "on";
env.BUILD_KEYS = JSON.stringify({ 139: "0123456789abcdef0123" });
assert.equal((await call("host", me("A"))).error, "official");
console.log("ok");
