import http from "node:http";
import fs from "node:fs";
import { DatabaseSync } from "node:sqlite";
const W = process.argv[2];
const { handle } = await import(W + "/worker.js");
const db = new DatabaseSync(":memory:");
db.exec(fs.readFileSync(W + "/schema.sql", "utf8"));
const D1 = { prepare(sql) { return { args: [], bind(...a) { this.args = a; return this; },
    async first() { return db.prepare(sql).get(...this.args) ?? null; }, async all() { return { results: db.prepare(sql).all(...this.args) }; },
    async run() { const r = db.prepare(sql).run(...this.args); return { meta: { changes: r.changes } }; } }; },
    async batch(l) { return Promise.all(l.map((s) => s.run())); } };
http.createServer(async (req, res) => {
    const chunks = []; for await (const c of req) chunks.push(c);
    const r = await handle(new Request("http://x" + req.url, { method: req.method, body: req.method === "POST" ? Buffer.concat(chunks) : undefined, headers: { "cf-connecting-ip": "127.0.0.1" } }), { DB: D1 }, Math.floor(Date.now() / 1000));
    res.writeHead(r.status, { "content-type": "application/json" }); res.end(await r.text());
}).listen(8787, () => console.log("server up"));
