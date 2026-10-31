// A copy of the Dragon Net matchmaking service for developing BT2's online play (port/dragonnet/worker.js, unchanged),
// on this computer: nobody else's game, and not the official service, is touched. The database is in memory (gone when
// this stops), like the service's own tests (port/dragonnet/test.mjs).
//
//   node bt2/tools/dev_server/serve.mjs [port] [versions]        (Node 22+: node:sqlite)
//   adb reverse tcp:8787 tcp:8787                                 (the phone reaches it as http://127.0.0.1:8787)
//
// `versions` is the service's ALLOWED_VERSIONS (default "6001,5001,40001"); no build key is required (BUILD_KEYS unset).
import {DatabaseSync} from 'node:sqlite';
import fs from 'node:fs';
import http from 'node:http';
import worker from '../../../port/dragonnet/worker.js';

const port = Number(process.argv[2] || 8787);
const dir = new URL('../../../port/dragonnet/', import.meta.url);
const sql = new DatabaseSync(':memory:');
for (const f of ['schema.sql', 'ranked-schema.sql', 'chat-schema.sql', 'presence-schema.sql']) sql.exec(fs.readFileSync(new URL(f, dir), 'utf8'));
const DB = {
  prepare(text) {
    return {
      values: [],
      bind(...v) { return {...this, values: v}; },
      async first() { return sql.prepare(text).get(...this.values) ?? null; },
      async run() { const r = sql.prepare(text).run(...this.values); return {meta: {changes: Number(r.changes)}}; },
      async all() { return {results: sql.prepare(text).all(...this.values)}; },
    };
  },
  async batch(statements) {
    sql.exec('BEGIN');
    try { const out = []; for (const st of statements) out.push(await st.run()); sql.exec('COMMIT'); return out; }
    catch (e) { sql.exec('ROLLBACK'); throw e; }
  },
};
const env = {DB, ALLOWED_VERSIONS: process.argv[3] || '6001,5001,40001', RANKED_ENABLED: '0', CHAT_ENABLED: '0'};

http.createServer(async (req, res) => {
  const chunks = [];
  for await (const c of req) chunks.push(c);
  const body = Buffer.concat(chunks);
  const headers = {...req.headers, 'cf-connecting-ip': req.socket.remoteAddress};
  const request = new Request(`http://${req.headers.host}${req.url}`, {method: req.method, headers, body: ['GET', 'HEAD'].includes(req.method) ? undefined : body});
  try {
    const r = await worker.fetch(request, env, {waitUntil() {}});
    res.writeHead(r.status, Object.fromEntries(r.headers));
    res.end(Buffer.from(await r.arrayBuffer()));
    console.log(req.method, req.url, r.status);
  } catch (e) {
    console.error(req.url, e);
    res.writeHead(500);
    res.end(String(e));
  }
}).listen(port, '127.0.0.1', () => console.log(`Dragon Net (development copy) on http://127.0.0.1:${port}, versions ${env.ALLOWED_VERSIONS}`));
