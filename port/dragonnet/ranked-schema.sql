CREATE TABLE IF NOT EXISTS ranked_players (
 id TEXT PRIMARY KEY, token_hash TEXT NOT NULL, name TEXT NOT NULL,
 rating INTEGER NOT NULL DEFAULT 1000, wins INTEGER NOT NULL DEFAULT 0,
 losses INTEGER NOT NULL DEFAULT 0, draws INTEGER NOT NULL DEFAULT 0,
 active_match TEXT, created INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS ranked_matches (
 id TEXT PRIMARY KEY, host_id TEXT NOT NULL, guest_id TEXT, made INTEGER NOT NULL,
 state TEXT NOT NULL DEFAULT 'waiting', room_code TEXT,
 host_score INTEGER, guest_score INTEGER, host_digest TEXT, guest_digest TEXT,
 host_rating INTEGER, guest_rating INTEGER, delta INTEGER, settlement TEXT,
 rule_set TEXT NOT NULL DEFAULT 'legacy', rematch_caps INTEGER NOT NULL DEFAULT 0,
 version INTEGER NOT NULL DEFAULT 40001,
 loc TEXT
);
CREATE INDEX IF NOT EXISTS ranked_waiting ON ranked_matches(state,made);
CREATE INDEX IF NOT EXISTS ranked_rating ON ranked_players(rating DESC,id);
CREATE INDEX IF NOT EXISTS ranked_rules_waiting ON ranked_matches(state,rule_set,made);
CREATE TABLE IF NOT EXISTS ranked_rounds (
 match_id TEXT NOT NULL, round_no INTEGER NOT NULL CHECK(round_no>=2 AND round_no<=256),
 state TEXT NOT NULL DEFAULT 'paired', made INTEGER NOT NULL,
 host_score INTEGER, guest_score INTEGER, host_digest TEXT, guest_digest TEXT,
 host_rating INTEGER NOT NULL, guest_rating INTEGER NOT NULL, delta INTEGER, settlement TEXT,
 PRIMARY KEY(match_id,round_no)
);
CREATE INDEX IF NOT EXISTS ranked_version_waiting ON ranked_matches(state,rule_set,version,made);
