// The Worker against a real SQLite database standing in for D1 (Node 22+: node:sqlite). Run: node test.mjs
import { DatabaseSync } from "node:sqlite";
import fs from "node:fs";
import assert from "node:assert/strict";
import { handle } from "./worker.js";

const db = new DatabaseSync(":memory:");
db.exec(fs.readFileSync(new URL("./schema.sql", import.meta.url), "utf8"));
const D1 = {
    prepare(sql) {
        return {
            args: [],
            bind(...a) { this.args = a; return this; },
            async first() { return db.prepare(sql).get(...this.args) ?? null; },
            async all() { return { results: db.prepare(sql).all(...this.args) }; },
            async run() { const r = db.prepare(sql).run(...this.args); return { meta: { changes: r.changes } }; },
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
now += 200;
assert.equal((await call("poll", { code: h.code, key: h.key })).error, "no room");
// the owner leaving closes the room
const h2 = await call("host", { v: 5, name: "H2", addrs: [] });
const g2 = await call("join", { v: 5, code: h2.code, name: "G", addrs: [] });
await call("leave", { code: h2.code, key: h2.key });
assert.equal((await call("poll", { code: h2.code, key: g2.key })).error, "no room");
console.log("tagnet worker: all tests passed");
