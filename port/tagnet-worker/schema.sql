-- Tag Team Online (worker.js): run once in the D1 database's console.
CREATE TABLE IF NOT EXISTS tag_rooms (
    code TEXT PRIMARY KEY,
    v INTEGER NOT NULL,
    host_key TEXT NOT NULL,
    host_name TEXT NOT NULL DEFAULT '',
    host_addrs TEXT NOT NULL DEFAULT '[]',
    host_seen TEXT NOT NULL DEFAULT '',
    created INTEGER NOT NULL,
    touched INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS tag_members (
    code TEXT NOT NULL,
    id INTEGER NOT NULL,
    key TEXT NOT NULL,
    name TEXT NOT NULL DEFAULT '',
    addrs TEXT NOT NULL DEFAULT '[]',
    seen TEXT NOT NULL DEFAULT '',
    watch INTEGER NOT NULL DEFAULT 0,
    touched INTEGER NOT NULL,
    PRIMARY KEY (code, id)
);
CREATE INDEX IF NOT EXISTS tag_members_key ON tag_members (code, key);
CREATE TABLE IF NOT EXISTS tag_relay_days (
    day INTEGER PRIMARY KEY,
    n INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS tag_key_days (day INTEGER NOT NULL, what TEXT NOT NULL, n INTEGER NOT NULL, PRIMARY KEY (day, what));
-- the quick 2 vs 2 queue (worker.js "quick"): a ticket a player holds while it waits; once four are together the room is made
-- and each ticket holds the room's code, the player's key and place in it
CREATE TABLE IF NOT EXISTS tag_quick (
    ticket TEXT PRIMARY KEY,
    key TEXT NOT NULL,
    v INTEGER NOT NULL,
    name TEXT NOT NULL DEFAULT '',
    addrs TEXT NOT NULL DEFAULT '[]',
    seen TEXT NOT NULL DEFAULT '',
    created INTEGER NOT NULL,
    touched INTEGER NOT NULL,
    code TEXT,
    mkey TEXT,
    id INTEGER,
    role INTEGER
);
CREATE INDEX IF NOT EXISTS tag_quick_wait ON tag_quick (v, code, created);
