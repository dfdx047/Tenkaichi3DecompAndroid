-- BT4 Android online (worker.js makes these by itself on its first requests; this file is for a look in the D1 console)
CREATE TABLE IF NOT EXISTS rooms (code TEXT PRIMARY KEY, made INTEGER NOT NULL, version INTEGER NOT NULL, ip TEXT NOT NULL, host_key TEXT NOT NULL, join_key TEXT, host_name TEXT NOT NULL, join_name TEXT, host_addrs TEXT NOT NULL, join_addrs TEXT, queue TEXT, who TEXT, lobby INTEGER, loc TEXT, join_region TEXT);
CREATE TABLE IF NOT EXISTS watchers (key TEXT PRIMARY KEY, code TEXT NOT NULL, made INTEGER NOT NULL, name TEXT NOT NULL, addrs TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS used (what TEXT NOT NULL, slot INTEGER NOT NULL, n INTEGER NOT NULL, PRIMARY KEY (what, slot));
CREATE TABLE IF NOT EXISTS ranked_matches (code TEXT PRIMARY KEY, made INTEGER NOT NULL, host_name TEXT NOT NULL, join_name TEXT NOT NULL);
-- (profile.js schema(): players, season_ratings, season_opponents, pair_day, reports, settled, trophies, meta)
