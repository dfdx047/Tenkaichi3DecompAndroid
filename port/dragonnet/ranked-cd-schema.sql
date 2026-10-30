-- Additive migration, AFTER ranked-schema.sql. SPDX-License-Identifier: MIT
CREATE TABLE IF NOT EXISTS ranked_service_route (
 id INTEGER PRIMARY KEY CHECK(id=1), day INTEGER NOT NULL,
 backend TEXT NOT NULL CHECK(backend IN ('C','D')), epoch INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS ranked_match_route (
 match_id TEXT PRIMARY KEY, backend TEXT NOT NULL CHECK(backend IN ('C','D')),
 epoch INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS ranked_route_backend ON ranked_match_route(backend,match_id);
-- Import existing C assignments without changing identities, scores or Elo.
INSERT OR IGNORE INTO ranked_match_route(match_id,backend,epoch)
 SELECT id,'C',0 FROM ranked_matches;
