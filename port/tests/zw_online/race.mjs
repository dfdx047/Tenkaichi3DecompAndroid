// Quick match under concurrent requests (as D1 serves them: other requests run between one request's statements).
// Run: node race.mjs
import { DatabaseSync } from "node:sqlite";
import fs from "node:fs";
import { handle } from "../../zw-worker/worker.js";

function server(seed) {
    const db = new DatabaseSync(":memory:");
    db.exec(fs.readFileSync(new URL("../../zw-worker/schema.sql", import.meta.url), "utf8"));
    let r = seed;
    const turn = async () => { r = (r * 48271) % 2147483647; for (let i = r % 4; i >= 0; i--) await new Promise((k) => setImmediate(k)); };
    const D1 = {
        prepare(sql) {
            return {
                args: [], bind(...a) { this.args = a; return this; },
                async first() { await turn(); return db.prepare(sql).get(...this.args) ?? null; },
                async all() { await turn(); return { results: db.prepare(sql).all(...this.args) }; },
                async run() { await turn(); return this.sync(); },
                sync() { return { meta: { changes: db.prepare(sql).run(...this.args).changes } }; },
            };
        },
        // (D1 runs a batch as one transaction)
        async batch(list) { await turn(); return list.map((s) => s.sync()); },
    };
    return { db, env: { DB: D1 } };
}
let bad = 0;
const runs = 400;
for (let seed = 1; seed <= runs; seed++) {
    const { db, env } = server(seed * 7919);
    const call = async (what, body, ip) => (await handle(new Request("https://t/v1/" + what, { method: "POST", body: JSON.stringify(body), headers: { "cf-connecting-ip": ip } }), env, 1_800_000_000)).json();
    const n = 2 + (seed % 4);
    const t = await Promise.all(Array.from({ length: n }, (_, i) => call("quick", { v: 1, disc: "d", warrior: "ZW1:" + i, addrs: [], made: false, madeOk: true }, "10.0.0." + i)));
    const last = await Promise.all(t.map((x, i) => call("quickpoll", { ticket: x.ticket, key: x.key }, "10.0.0." + i)));
    // every matched player must be in a room that has exactly two players, each pointing back at it, and nobody twice
    let wrong = 0;
    const rooms = db.prepare("SELECT code FROM zw_rooms").all();
    for (const { code } of rooms) {
        const tickets = db.prepare("SELECT COUNT(*) AS n FROM zw_quick WHERE code = ?").get(code).n;
        if (tickets !== 2) wrong++;
    }
    const matched = last.filter((x) => x.state === "matched");
    for (const m of matched) if (matched.filter((o) => o.code === m.code).length > 2) wrong++;
    for (const m of matched) if (!m.peer) wrong++;
    // and nobody is left waiting while another is: after one more poll each, at most the odd one out
    const end = await Promise.all(t.map((x, i) => call("quickpoll", { ticket: x.ticket, key: x.key }, "10.0.0." + i)));
    if (end.filter((x) => x.state === "matched").length !== n - (n % 2)) wrong++;
    if (wrong) bad++;
}
console.log(`quick match concurrent: ${bad} of ${runs} runs left a broken room`);
process.exit(bad ? 1 : 0);
