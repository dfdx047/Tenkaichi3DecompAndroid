// A D1 stand-in over node:sqlite (Node 24) so the real worker.js/profile.js/rooms.js run in-process, with a fake clock, no wrangler and no network.
// Faithful where the worker depends on it: batch() is ONE transaction (all or nothing), constraint errors carry SQLite's own text
// ("UNIQUE constraint failed", "NOT NULL constraint failed"), first()/all()/run() return the D1 shapes.
import { DatabaseSync } from "node:sqlite";

export function makeD1() {
    const db = new DatabaseSync(":memory:");
    const stmt = (sql, args = []) => ({
        sql, args,
        bind(...a) { return stmt(sql, a); },
        async first() { const r = db.prepare(sql).get(...args); return r === undefined ? null : { ...r }; },
        async all() { return { results: db.prepare(sql).all(...args).map((r) => ({ ...r })), success: true, meta: {} }; },
        async run() { const r = db.prepare(sql).run(...args); return { success: true, meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } }; },
        _exec() { // inside a batch
            const s = db.prepare(sql);
            if (/^\s*(SELECT|PRAGMA|WITH)/i.test(sql)) return { results: s.all(...args).map((r) => ({ ...r })), success: true, meta: {} };
            const r = s.run(...args);
            return { results: [], success: true, meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } };
        },
    });
    let batches = 0;
    return {
        prepare: (sql) => stmt(sql),
        async batch(list) {
            batches++;
            db.exec("BEGIN");
            try {
                const out = list.map((s) => s._exec());
                db.exec("COMMIT");
                return out;
            } catch (e) {
                db.exec("ROLLBACK");
                throw e;
            }
        },
        raw: db,
        get batches() { return batches; },
    };
}

// A fake clock: the worker reads Date.now(); tests move it instead of sleeping.
export function fakeClock(start = Date.UTC(2026, 10, 18, 15, 0, 0)) {
    let t = start;
    const real = Date.now;
    Date.now = () => t;
    return { advance(ms) { t += ms; }, set(v) { t = v; }, now: () => t, restore() { Date.now = real; } };
}
