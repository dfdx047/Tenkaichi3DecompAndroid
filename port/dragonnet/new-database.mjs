// The schemas a NEW Dragon Net database is made of, in order, read from README.md's line "A new database: `a.sql`, ...".
// The tests build their databases from this list, so a schema the README forgets (ranked-flow-schema.sql was, and every
// Ranked search of a Worker set up by the README failed with "no such table: ranked_flow") fails the tests too.
import fs from 'node:fs';

export function newDatabaseSchemas() {
    const readme = fs.readFileSync(new URL('./README.md', import.meta.url), 'utf8');
    const line = readme.split('\n').find((l) => l.startsWith('A new database:'));
    if (!line) throw new Error('README.md has no "A new database:" line');
    const files = [...line.matchAll(/`([a-z0-9-]+\.sql)`/g)].map((m) => m[1]);
    if (!files.length) throw new Error('README.md\'s "A new database:" line names no schema');
    return files;
}

// A fresh in-memory SQLite database made the way the README says (node:sqlite's DatabaseSync).
export function newDatabase(DatabaseSync) {
    const sql = new DatabaseSync(':memory:');
    for (const f of newDatabaseSchemas()) sql.exec(fs.readFileSync(new URL('./' + f, import.meta.url), 'utf8'));
    return sql;
}
