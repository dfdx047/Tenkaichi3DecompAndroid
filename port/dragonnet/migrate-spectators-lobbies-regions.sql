-- Spectators, lobbies and nearby quick matches (the PC port's 0.1.38.8 / 0.1.38.9, merged into worker.js): run ONCE in the D1 console of a Dragon Net
-- database made before them (schema.sql has them for a new one). Without these the Worker still answers every older game as before: "no lobbies" for a
-- lobby, rooms matched after the widening without a place, and spectators refused (port/matchmaking/README.md says the same for the PC's service).
ALTER TABLE rooms ADD COLUMN lobby INTEGER;
ALTER TABLE rooms ADD COLUMN loc TEXT;
ALTER TABLE rooms ADD COLUMN join_region TEXT;
CREATE TABLE IF NOT EXISTS watchers (key TEXT PRIMARY KEY, code TEXT NOT NULL, made INTEGER NOT NULL, name TEXT NOT NULL, addrs TEXT NOT NULL);
