import Database from "better-sqlite3";
import { readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const schemaPath = resolve(dirname(fileURLToPath(import.meta.url)), "../matchmaking/schema.sql");

export function createDatabase(path) {
    const db = new Database(resolve(path));
    db.pragma("journal_mode = WAL");
    db.exec(readFileSync(schemaPath, "utf8"));

    const api = {
        prepare(sql) {
            let statement;
            let params = [];
            const bound = {
                bind(...values) {
                    params = values;
                    return bound;
                },
                async first() {
                    const row = statement.get(...params);
                    return row === undefined ? null : row;
                },
                async all() {
                    return { results: statement.all(...params) };
                },
                async run() {
                    const info = statement.run(...params);
                    return { meta: { changes: info.changes, last_row_id: Number(info.lastInsertRowid) } };
                },
            };
            statement = db.prepare(sql);
            return bound;
        },
        async batch(statements) {
            return db.transaction(() => statements.map((statement) => statement.run()))();
        },
        exec(sql) {
            return db.exec(sql);
        },
        close() {
            db.close();
        },
    };
    return api;
}
