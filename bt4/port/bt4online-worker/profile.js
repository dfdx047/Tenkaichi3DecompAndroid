// BT4 Android's online profile (the game's port/src/gs/bt4_profile.c speaks to it): a unique nickname, its recovery
// code, the Ranked medals and rating of the weekly seasons and the permanent career (docs/RANKED_TEMPORADAS_ANDROID.md).
//
//   POST /v1/register {name}                  -> {ok, id, secret, name, recovery}  | {error: "taken"|"invalid"|"slow"}
//   POST /v1/rename   {id, secret, name}      -> {ok, name}                        | {error: "taken"|"invalid"|"who"}
//   POST /v1/recover  {name, code}            -> {ok, id, secret, name}            | {error: "wrong"|"slow"|"invalid"}
//   POST /v1/recovery {id, secret}            -> {ok, recovery}  (a new recovery code; the old one stops working)
//   POST /v1/me       {id, secret}            -> career: gold, silver, wins, losses, ranked_total, best_rating, best_rating_season,
//                                                 seasons_played, trophies: [gold, silver, bronze];
//                                                 season: season (id), season_name, ends, rating, rwins, rlosses, rank, players,
//                                                 season_gold, season_silver, peak, rated, opponents, eligible, last: [..]
//   POST /v1/report   {id, secret, rkey, code, n, me, opp, side, winner, ranked}
//                                             -> {ok, state: "waiting"|"settled"|"casual"|"conflict"|"draw", medal, delta, rating, ranked,
//                                                 capped, rematches_left}
//   POST /v1/top      {season?}               -> {ok, season, name, ends, total, closed,
//                                                 top: [[name, rating, wins, losses, gold, silver, trophy1, trophy2, trophy3], ...]}
//   POST /v1/seasons  {page}                  -> {ok, seasons: [[id, slug, name, starts, ends, status, players], ...]}
//   POST /v1/history  {id, secret, page}      -> {ok, history: [[season, name, rank, rating, peak, gold, silver, wins, losses, trophy], ...]}
//   POST /v1/matches  {id, secret, before}    -> {ok, matches: [[at, season, opp, won, rated, delta, rating_after], ...]}
//   POST /v1/presence {who}                   -> {ok, online}
//
// The rules:
//  - A battle counts only when BOTH players tell of it and agree: the same room, the same battle number of their session,
//    each other's names, opposite sides and the same winner, within 15 minutes. (Games before 1.0.3 sent the room code
//    from the host only: a report without a code pairs with the other's.) A disconnection counts for nobody.
//  - Medals and points only in Ranked: a match this service's Ranked queue made with these two players (ranked_rooms).
//    The winner gets a gold medal and Elo points (K 32, from 1000 every season), the loser a silver medal.
//  - With the same rival, RANKED_PER_PAIR_DAY battles a day count (the match and its three rematches); later ones count
//    for nothing.
//  - A battle's season and career rows, its history row and its claim are written in ONE transaction (env.DB.batch): all
//    or nothing, never twice (UNIQUE claims), and the rating it read is still the rating it writes (guard).
//  - Seasons are weeks (seasons.js): Monday 00:00 in Brasilia. Their end archives the table (season_history, trophies for
//    the first three among those with 5 rated battles against 3 rivals) before the next season opens.

import { seasonOf, seasonEnds, seasonName, ensureSeasonStatement, closeSeasons, listSeasons, RANK_ORDER, TROPHY_MIN_RATED, TROPHY_MIN_OPPONENTS } from "./seasons.js";

const DAY = 86400000;
const DAY_ZERO = 10800000;     // 1970-01-01 03:00 UTC (a Brasilia day starts at 03:00 UTC)
const PAIR_WINDOW = 15 * 60000;
const RANKED_ROOM_WINDOW = 2 * 3600 * 1000; // a Ranked match's battle is reported at most this long after the match was made
const REPORT_GAP = 10000;
const START_RATING = 1000, K = 32, MIN_RATING = 100;
const TOP = 50;
export const RANKED_PER_PAIR_DAY = 4; // the match and three rematches
const RECOVER_PER_HOUR = 10;

const json = (o, status = 200) => new Response(JSON.stringify(o), {
    status, headers: { "content-type": "application/json", "cache-control": "no-store" } });
const dayOf = (ms) => Math.floor((ms - DAY_ZERO) / DAY);

export function cleanName(raw) {
    if (typeof raw !== "string") return null;
    const name = raw.replace(/\s+/g, " ").trim();
    if (name.length < 2 || name.length > 16) return null;
    if (!/^[\x20-\x7E]+$/.test(name) || /["\\<>]/.test(name)) return null;
    return name;
}
const nameKey = (name) => name.toLowerCase().replace(/\s+/g, "");
// names as two services may have kept them (the rooms keep printable letters, 16 at most): compared by this
const looseKey = (name) => String(name ?? "").toLowerCase().replace(/[^a-z0-9]/g, "").slice(0, 16);

export function elo(winner, loser) {
    const expected = 1 / (1 + Math.pow(10, (loser - winner) / 400));
    return Math.max(1, Math.round(K * (1 - expected)));
}

async function sha256(text) {
    const d = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
    return [...new Uint8Array(d)].map((b) => b.toString(16).padStart(2, "0")).join("");
}
function randomHex(bytes) {
    const a = new Uint8Array(bytes);
    crypto.getRandomValues(a);
    return [...a].map((b) => b.toString(16).padStart(2, "0")).join("");
}
// A recovery code: 16 letters and digits (80 bits) without the ones read alike (0/O, 1/I/L), as XXXX-XXXX-XXXX-XXXX.
const CODE_ALPHABET = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
function newRecoveryCode() {
    const a = new Uint8Array(16);
    crypto.getRandomValues(a);
    const s = [...a].map((b) => CODE_ALPHABET[b % CODE_ALPHABET.length]).join("");
    return s.slice(0, 4) + "-" + s.slice(4, 8) + "-" + s.slice(8, 12) + "-" + s.slice(12);
}
const cleanCode = (raw) => typeof raw === "string" ? raw.toUpperCase().replace(/[^0-9A-Z]/g, "").replace(/O/g, "0").replace(/[IL]/g, "1") : "";
const recoveryHash = (code) => sha256("bt4-recovery|" + cleanCode(code));

let schemaReady = false;
export async function schema(env) {
    if (schemaReady) return;
    // the Ranked queue's room ledger was "ranked_matches" before seasons: it is "ranked_rooms" now (rooms.js)
    const old = await env.DB.prepare("SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'ranked_matches'").first();
    if (old && String(old.sql).includes("host_name")) {
        await env.DB.prepare("ALTER TABLE ranked_matches RENAME TO ranked_rooms").run();
    }
    await env.DB.batch([
        env.DB.prepare("CREATE TABLE IF NOT EXISTS players (id TEXT PRIMARY KEY, secret_hash TEXT NOT NULL, name TEXT NOT NULL, name_key TEXT NOT NULL UNIQUE, made INTEGER NOT NULL, seen INTEGER NOT NULL, ip_key TEXT NOT NULL DEFAULT '', gold INTEGER NOT NULL DEFAULT 0, silver INTEGER NOT NULL DEFAULT 0, wins INTEGER NOT NULL DEFAULT 0, losses INTEGER NOT NULL DEFAULT 0, last_report INTEGER NOT NULL DEFAULT 0)"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS players_ip ON players (ip_key, made)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS season_opponents (season INTEGER NOT NULL, player TEXT NOT NULL, opp TEXT NOT NULL, PRIMARY KEY (season, player, opp))"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS pair_day (day INTEGER NOT NULL, a TEXT NOT NULL, b TEXT NOT NULL, battles INTEGER NOT NULL DEFAULT 0, rated INTEGER NOT NULL DEFAULT 0, PRIMARY KEY (day, a, b))"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS reports (rid INTEGER PRIMARY KEY AUTOINCREMENT, rkey TEXT NOT NULL, player TEXT NOT NULL, code TEXT NOT NULL, n INTEGER NOT NULL, me TEXT NOT NULL, opp TEXT NOT NULL, side INTEGER NOT NULL, winner INTEGER NOT NULL, ranked INTEGER NOT NULL, at INTEGER NOT NULL, outcome TEXT, UNIQUE (player, rkey))"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS reports_pair ON reports (code, n, at)"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS reports_open ON reports (at) WHERE outcome IS NULL"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS settled (first INTEGER PRIMARY KEY, second INTEGER NOT NULL, at INTEGER NOT NULL)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS trophies (season INTEGER NOT NULL, place INTEGER NOT NULL, player TEXT NOT NULL, rating INTEGER NOT NULL, PRIMARY KEY (season, place))"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS trophies_player ON trophies (player)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS meta (k TEXT PRIMARY KEY, v INTEGER NOT NULL)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS presence (who TEXT PRIMARY KEY, at INTEGER NOT NULL)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS ranked_rooms (code TEXT PRIMARY KEY, made INTEGER NOT NULL, host_name TEXT NOT NULL, join_name TEXT NOT NULL)"),
        // seasons and the permanent profile
        env.DB.prepare("CREATE TABLE IF NOT EXISTS seasons (id INTEGER PRIMARY KEY, slug TEXT NOT NULL UNIQUE, name TEXT NOT NULL, starts_at INTEGER NOT NULL, ends_at INTEGER NOT NULL, status TEXT NOT NULL CHECK (status IN ('scheduled','active','closing','closed')), closed_at INTEGER, players INTEGER NOT NULL DEFAULT 0, CHECK (ends_at > starts_at))"),
        env.DB.prepare("CREATE UNIQUE INDEX IF NOT EXISTS seasons_one_active ON seasons (status) WHERE status = 'active'"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS season_stats (season_id INTEGER NOT NULL, player_id TEXT NOT NULL, rating INTEGER NOT NULL DEFAULT 1000, peak_rating INTEGER NOT NULL DEFAULT 1000, gold INTEGER NOT NULL DEFAULT 0, silver INTEGER NOT NULL DEFAULT 0, wins INTEGER NOT NULL DEFAULT 0, losses INTEGER NOT NULL DEFAULT 0, matches INTEGER NOT NULL DEFAULT 0, updated INTEGER NOT NULL, PRIMARY KEY (season_id, player_id))"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS season_stats_rank ON season_stats (season_id, rating DESC, wins DESC, updated)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS career_stats (player_id TEXT PRIMARY KEY, gold INTEGER NOT NULL DEFAULT 0, silver INTEGER NOT NULL DEFAULT 0, wins INTEGER NOT NULL DEFAULT 0, losses INTEGER NOT NULL DEFAULT 0, ranked_matches INTEGER NOT NULL DEFAULT 0, best_rating INTEGER NOT NULL DEFAULT 1000, best_rating_season INTEGER, seasons_played INTEGER NOT NULL DEFAULT 0, updated INTEGER NOT NULL)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS season_history (season_id INTEGER NOT NULL, player_id TEXT NOT NULL, final_rank INTEGER, final_rating INTEGER NOT NULL, peak_rating INTEGER NOT NULL, gold INTEGER NOT NULL, silver INTEGER NOT NULL, wins INTEGER NOT NULL, losses INTEGER NOT NULL, matches INTEGER NOT NULL, trophy INTEGER, name_then TEXT NOT NULL, PRIMARY KEY (season_id, player_id))"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS season_history_player ON season_history (player_id, season_id)"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS season_history_rank ON season_history (season_id, final_rank)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS ranked_matches (id INTEGER PRIMARY KEY AUTOINCREMENT, match_key TEXT NOT NULL UNIQUE, season_id INTEGER NOT NULL, played_at INTEGER NOT NULL, room_code TEXT NOT NULL, battle_no INTEGER NOT NULL, kind TEXT NOT NULL CHECK (kind IN ('ranked','casual')), rated INTEGER NOT NULL, medals INTEGER NOT NULL, winner_id TEXT NOT NULL, loser_id TEXT NOT NULL, winner_before INTEGER, winner_after INTEGER, loser_before INTEGER, loser_after INTEGER)"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS ranked_matches_winner ON ranked_matches (winner_id, played_at DESC)"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS ranked_matches_loser ON ranked_matches (loser_id, played_at DESC)"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS ranked_matches_season ON ranked_matches (season_id)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS guard (v INTEGER NOT NULL)"), // always empty: a NULL put in it aborts a transaction
        env.DB.prepare("CREATE TABLE IF NOT EXISTS recover_tries (ip_key TEXT NOT NULL, at INTEGER NOT NULL)"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS recover_tries_ip ON recover_tries (ip_key, at)"),
    ]);
    const cols = await env.DB.prepare("PRAGMA table_info(players)").all();
    if (!cols.results.some((c) => c.name === "recovery_hash")) {
        await env.DB.prepare("ALTER TABLE players ADD COLUMN recovery_hash TEXT").run();
    }
    // the career of the players from before seasons: what players kept (all 0 when seasons came; kept for the record)
    await env.DB.prepare("INSERT OR IGNORE INTO career_stats (player_id, gold, silver, wins, losses, updated) SELECT id, gold, silver, wins, losses, ? FROM players")
        .bind(Date.now()).run();
    schemaReady = true;
}

async function who(env, body) {
    if (typeof body.id !== "string" || typeof body.secret !== "string" || body.id.length !== 32 || body.secret.length !== 64) return null;
    const p = await env.DB.prepare("SELECT * FROM players WHERE id = ?").bind(body.id).first();
    if (!p || p.secret_hash !== await sha256(body.secret)) return null;
    return p;
}
const ipKeyOf = async (ip) => (await sha256("bt4-online|" + ip)).slice(0, 24);

async function register(env, body, ip, now) {
    const name = cleanName(body.name);
    if (!name) return json({ error: "invalid" });
    const ipKey = await ipKeyOf(ip);
    const recent = await env.DB.prepare("SELECT COUNT(*) AS n FROM players WHERE ip_key = ? AND made > ?").bind(ipKey, now - 3600000).first();
    if (recent && recent.n >= 30) return json({ error: "slow" }); // (mobile carriers share addresses: generous)
    const id = randomHex(16), secret = randomHex(32), recovery = newRecoveryCode();
    try {
        await env.DB.batch([
            env.DB.prepare("INSERT INTO players (id, secret_hash, name, name_key, made, seen, ip_key, recovery_hash) VALUES (?, ?, ?, ?, ?, ?, ?, ?)")
                .bind(id, await sha256(secret), name, nameKey(name), now, now, ipKey, await recoveryHash(recovery)),
            env.DB.prepare("INSERT OR IGNORE INTO career_stats (player_id, updated) VALUES (?, ?)").bind(id, now),
        ]);
    } catch (e) {
        if (String(e).includes("UNIQUE")) return json({ error: "taken" });
        throw e;
    }
    return json({ ok: true, id, secret, name, recovery });
}

async function rename(env, body, now) {
    const p = await who(env, body);
    if (!p) return json({ error: "who" });
    const name = cleanName(body.name);
    if (!name) return json({ error: "invalid" });
    if (name === p.name) return json({ ok: true, name });
    try {
        await env.DB.prepare("UPDATE players SET name = ?, name_key = ?, seen = ? WHERE id = ?").bind(name, nameKey(name), now, p.id).run();
    } catch (e) {
        if (String(e).includes("UNIQUE")) return json({ error: "taken" });
        throw e;
    }
    return json({ ok: true, name });
}

// A new recovery code for this account (the old one stops working). The game keeps it with the account and shows it.
async function recovery(env, body) {
    const p = await who(env, body);
    if (!p) return json({ error: "who" });
    const code = newRecoveryCode();
    await env.DB.prepare("UPDATE players SET recovery_hash = ? WHERE id = ?").bind(await recoveryHash(code), p.id).run();
    return json({ ok: true, recovery: code });
}

// The account back on a new installation: its name and recovery code. The account gets a new secret (an old copy of the
// game with the old one is signed out); the recovery code stays. RECOVER_PER_HOUR tries an hour from one address.
async function recover(env, body, ip, now) {
    const name = cleanName(body.name);
    const code = cleanCode(body.code);
    if (!name || code.length !== 16) return json({ error: "invalid" });
    const ipKey = await ipKeyOf(ip);
    const tries = await env.DB.prepare("SELECT COUNT(*) AS n FROM recover_tries WHERE ip_key = ? AND at > ?").bind(ipKey, now - 3600000).first();
    if (tries && tries.n >= RECOVER_PER_HOUR) return json({ error: "slow" });
    await env.DB.prepare("INSERT INTO recover_tries (ip_key, at) VALUES (?, ?)").bind(ipKey, now).run();
    const p = await env.DB.prepare("SELECT id, name, recovery_hash FROM players WHERE name_key = ?").bind(nameKey(name)).first();
    if (!p || !p.recovery_hash || p.recovery_hash !== await recoveryHash(code)) return json({ error: "wrong" });
    const secret = randomHex(32);
    await env.DB.prepare("UPDATE players SET secret_hash = ?, seen = ? WHERE id = ?").bind(await sha256(secret), now, p.id).run();
    return json({ ok: true, id: p.id, secret, name: p.name });
}

async function me(env, body, now) {
    const p = await who(env, body);
    if (!p) return json({ error: "who" });
    const season = seasonOf(now);
    const [r, trophies, players, last, opps, career] = await env.DB.batch([
        env.DB.prepare("SELECT rating, peak_rating, gold, silver, wins, losses, matches, updated FROM season_stats WHERE season_id = ? AND player_id = ?").bind(season, p.id),
        env.DB.prepare("SELECT place, COUNT(*) AS n FROM trophies WHERE player = ? GROUP BY place").bind(p.id),
        env.DB.prepare("SELECT COUNT(*) AS n FROM season_stats WHERE season_id = ? AND matches > 0").bind(season),
        env.DB.prepare("SELECT final_rank AS place, name_then AS name, final_rating AS rating FROM season_history WHERE season_id = ? AND final_rank <= 3 ORDER BY final_rank").bind(season - 1),
        env.DB.prepare("SELECT COUNT(*) AS n FROM season_opponents WHERE season = ? AND player = ?").bind(season, p.id),
        env.DB.prepare("SELECT * FROM career_stats WHERE player_id = ?").bind(p.id),
    ]);
    const mine = r.results[0], c = career.results[0] || {};
    let rank = 0;
    if (mine && mine.matches > 0) {
        const above = await env.DB.prepare("SELECT COUNT(*) AS n FROM season_stats WHERE season_id = ? AND matches > 0 AND (rating > ? OR (rating = ? AND wins > ?) OR (rating = ? AND wins = ? AND updated < ?))")
            .bind(season, mine.rating, mine.rating, mine.wins, mine.rating, mine.wins, mine.updated).first();
        rank = (above ? above.n : 0) + 1;
    }
    const t = [0, 0, 0];
    for (const row of trophies.results) if (row.place >= 1 && row.place <= 3) t[row.place - 1] = row.n;
    if (now - p.seen > 3600000) await env.DB.prepare("UPDATE players SET seen = ? WHERE id = ?").bind(now, p.id).run();
    const rated = mine ? mine.matches : 0, opponents = opps.results[0] ? opps.results[0].n : 0;
    return json({
        ok: true, name: p.name, has_recovery: !!p.recovery_hash,
        // the career (never reset)
        gold: c.gold ?? 0, silver: c.silver ?? 0, wins: c.wins ?? 0, losses: c.losses ?? 0, ranked_total: c.ranked_matches ?? 0,
        best_rating: c.best_rating ?? START_RATING, best_rating_season: c.best_rating_season ?? null, seasons_played: c.seasons_played ?? 0,
        trophies: t,
        // this season
        season, season_name: seasonName(season), ends: seasonEnds(season),
        rating: mine ? mine.rating : START_RATING, peak: mine ? mine.peak_rating : START_RATING,
        rwins: mine ? mine.wins : 0, rlosses: mine ? mine.losses : 0, season_gold: mine ? mine.gold : 0, season_silver: mine ? mine.silver : 0,
        rank, players: players.results[0] ? players.results[0].n : 0,
        last: last.results.map((x) => [x.place, x.name, x.rating]),
        rated, opponents, eligible: rated >= TROPHY_MIN_RATED && opponents >= TROPHY_MIN_OPPONENTS,
    });
}

const TROPHY_COLUMNS = "(SELECT COUNT(*) FROM trophies t WHERE t.player = p.id AND t.place = 1) AS t1, " +
    "(SELECT COUNT(*) FROM trophies t WHERE t.player = p.id AND t.place = 2) AS t2, " +
    "(SELECT COUNT(*) FROM trophies t WHERE t.player = p.id AND t.place = 3) AS t3";

async function top(env, body, now) {
    const current = seasonOf(now);
    const season = Number.isInteger(body.season) && body.season > 0 && body.season < current ? body.season : current;
    if (season !== current) { // a season that is over: its archived table
        const s = await env.DB.prepare("SELECT name, ends_at, status, players FROM seasons WHERE id = ?").bind(season).first();
        if (!s || s.status !== "closed") return json({ ok: true, season, name: seasonName(season), ends: seasonEnds(season), total: 0, closed: true, top: [] });
        const rows = await env.DB.prepare("SELECT h.name_then AS name, h.final_rating AS rating, h.wins, h.losses, h.gold, h.silver, " + TROPHY_COLUMNS +
            " FROM season_history h JOIN players p ON p.id = h.player_id WHERE h.season_id = ? AND h.matches > 0 " +
            "ORDER BY h.final_rank IS NULL, h.final_rank, h.final_rating DESC LIMIT ?").bind(season, TOP).all();
        return json({ ok: true, season, name: s.name, ends: s.ends_at, total: s.players, closed: true,
                      top: rows.results.map((r) => [r.name, r.rating, r.wins, r.losses, r.gold, r.silver, r.t1, r.t2, r.t3]) });
    }
    const rows = await env.DB.prepare("SELECT p.name, s.rating, s.wins, s.losses, s.gold, s.silver, " + TROPHY_COLUMNS +
        " FROM season_stats s JOIN players p ON p.id = s.player_id WHERE s.season_id = ? AND s.matches > 0 ORDER BY s." + RANK_ORDER.replace(/, /g, ", s.") + " LIMIT ?")
        .bind(season, TOP).all();
    const total = await env.DB.prepare("SELECT COUNT(*) AS n FROM season_stats WHERE season_id = ? AND matches > 0").bind(season).first();
    return json({ ok: true, season, name: seasonName(season), ends: seasonEnds(season), total: total ? total.n : 0, closed: false,
                  top: rows.results.map((r) => [r.name, r.rating, r.wins, r.losses, r.gold, r.silver, r.t1, r.t2, r.t3]) });
}

async function history(env, body) {
    const p = await who(env, body);
    if (!p) return json({ error: "who" });
    const page = Number.isInteger(body.page) && body.page >= 0 && body.page < 10000 ? body.page : 0;
    const rows = await env.DB.prepare("SELECT h.season_id, s.name, h.final_rank, h.final_rating, h.peak_rating, h.gold, h.silver, h.wins, h.losses, h.trophy " +
        "FROM season_history h JOIN seasons s ON s.id = h.season_id WHERE h.player_id = ? ORDER BY h.season_id DESC LIMIT 10 OFFSET ?").bind(p.id, page * 10).all();
    return json({ ok: true, history: rows.results.map((r) => [r.season_id, r.name, r.final_rank, r.final_rating, r.peak_rating, r.gold, r.silver, r.wins, r.losses, r.trophy]) });
}

async function matches(env, body) {
    const p = await who(env, body);
    if (!p) return json({ error: "who" });
    const before = Number.isInteger(body.before) && body.before > 0 ? body.before : Number.MAX_SAFE_INTEGER;
    const rows = await env.DB.prepare(
        "SELECT m.played_at, m.season_id, m.rated, m.winner_id, m.winner_before, m.winner_after, m.loser_before, m.loser_after, " +
        "(SELECT name FROM players WHERE id = CASE WHEN m.winner_id = ? THEN m.loser_id ELSE m.winner_id END) AS opp " +
        "FROM ranked_matches m WHERE (m.winner_id = ? OR m.loser_id = ?) AND m.played_at < ? ORDER BY m.played_at DESC LIMIT 20")
        .bind(p.id, p.id, p.id, before).all();
    return json({ ok: true, matches: rows.results.map((m) => {
        const won = m.winner_id === p.id;
        const b = won ? m.winner_before : m.loser_before, a = won ? m.winner_after : m.loser_after;
        return [m.played_at, m.season_id, m.opp ?? "?", won, m.rated === 1, a != null && b != null ? a - b : null, a];
    }) });
}

// Whether a battle's room is a Ranked match this service made, with these two players, around the battle's time.
async function rankedRoom(env, code, nameA, nameB, atMs) {
    if (!code) return false;
    const m = await env.DB.prepare("SELECT * FROM ranked_rooms WHERE code = ?").bind(code).first();
    if (!m || m.made * 1000 < atMs - RANKED_ROOM_WINDOW || m.made * 1000 > atMs + 60000) return false;
    const h = looseKey(m.host_name), j = looseKey(m.join_name), a = looseKey(nameA), b = looseKey(nameB);
    return h !== "" && j !== "" && h !== j && ((h === a && j === b) || (h === b && j === a));
}

// The two agreeing reports a and b of one battle: counted once, in one transaction (see the rules above).
async function settle(env, a, b, now) {
    const first = Math.min(a.rid, b.rid), second = Math.max(a.rid, b.rid), matchKey = first + ":" + second;
    const claim = () => env.DB.prepare("INSERT INTO settled (first, second, at) VALUES (?, ?, ?)").bind(first, second, now); // (no OR IGNORE: twice = error = nothing)
    const outcome = (row, o) => env.DB.prepare("UPDATE reports SET outcome = ? WHERE rid = ?").bind(JSON.stringify(o), row.rid);
    if (a.winner !== b.winner || a.winner === 2) {
        const o = { state: a.winner !== b.winner ? "conflict" : "draw" };
        try { await env.DB.batch([claim(), outcome(a, o), outcome(b, o)]); } catch (e) { if (!String(e).includes("UNIQUE")) throw e; }
        return;
    }
    const win = a.side === a.winner ? a : b, lose = win === a ? b : a;
    const code = a.code || b.code, at = Math.min(a.at, b.at);
    const ranked = a.ranked === 1 && b.ranked === 1 && await rankedRoom(env, code, win.me, lose.me, at);
    for (let attempt = 0; attempt < 3; attempt++) {
        const day = dayOf(at), pa = win.player < lose.player ? win.player : lose.player, pb = pa === win.player ? lose.player : win.player;
        const season = seasonOf(now);
        const [pairRow, ratingRows] = await env.DB.batch([
            env.DB.prepare("SELECT battles, rated FROM pair_day WHERE day = ? AND a = ? AND b = ?").bind(day, pa, pb),
            env.DB.prepare("SELECT player_id, rating FROM season_stats WHERE season_id = ? AND player_id IN (?, ?)").bind(season, win.player, lose.player),
        ]);
        const pair = pairRow.results[0] || { battles: 0, rated: 0 };
        const rated = ranked && pair.rated < RANKED_PER_PAIR_DAY;
        const rating = Object.fromEntries(ratingRows.results.map((r) => [r.player_id, r.rating]));
        const rw = rating[win.player] ?? START_RATING, rl = rating[lose.player] ?? START_RATING;
        const gain = rated ? elo(rw, rl) : 0, loss = rated ? Math.min(gain, Math.max(0, rl - MIN_RATING)) : 0;
        const left = Math.max(0, RANKED_PER_PAIR_DAY - pair.rated - (rated ? 1 : 0));
        // (a battle that is not Ranked: "casual", no medal. Games before 1.0.3 read "settled" with medal "none" as the day's
        // limit reached and said so after every casual battle; any other state they take in silence.)
        const state = ranked ? "settled" : "casual";
        let wo = { state, medal: rated ? "gold" : "none", ranked, rematches_left: ranked ? left : undefined };
        let lo = { state, medal: rated ? "silver" : "none", ranked, rematches_left: ranked ? left : undefined };
        if (rated) {
            wo = { ...wo, delta: gain, rating: rw + gain };
            lo = { ...lo, delta: -loss, rating: rl - loss };
        } else if (ranked) {
            wo.capped = lo.capped = true; // (past the day's battles against this rival: nothing counts)
        }
        const writes = [
            claim(),
            env.DB.prepare("INSERT INTO ranked_matches (match_key, season_id, played_at, room_code, battle_no, kind, rated, medals, winner_id, loser_id, winner_before, winner_after, loser_before, loser_after) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                .bind(matchKey, season, at, code, a.n, ranked ? "ranked" : "casual", rated ? 1 : 0, rated ? 1 : 0, win.player, lose.player,
                      rated ? rw : null, rated ? rw + gain : null, rated ? rl : null, rated ? rl - loss : null),
            // the day's count for this pair is still what was read (else: read again)
            env.DB.prepare("INSERT INTO guard SELECT NULL WHERE COALESCE((SELECT rated FROM pair_day WHERE day = ? AND a = ? AND b = ?), 0) <> ?").bind(day, pa, pb, pair.rated),
            env.DB.prepare("INSERT INTO pair_day (day, a, b, battles, rated) VALUES (?, ?, ?, 1, ?) ON CONFLICT (day, a, b) DO UPDATE SET battles = battles + 1, rated = rated + excluded.rated")
                .bind(day, pa, pb, rated ? 1 : 0),
        ];
        if (rated) {
            writes.push(
                ensureSeasonStatement(env, season),
                // the season is open (a week that is closing takes no more battles: they count in the next)
                env.DB.prepare("INSERT INTO guard SELECT NULL WHERE NOT EXISTS (SELECT 1 FROM seasons WHERE id = ? AND status = 'active')").bind(season),
                // the ratings are still the ones read (another battle of one of them settled meanwhile: read again)
                env.DB.prepare("INSERT INTO guard SELECT NULL WHERE COALESCE((SELECT rating FROM season_stats WHERE season_id = ? AND player_id = ?), 1000) <> ? OR COALESCE((SELECT rating FROM season_stats WHERE season_id = ? AND player_id = ?), 1000) <> ?")
                    .bind(season, win.player, rw, season, lose.player, rl),
                ...[[win.player, rw + gain, 1, 0, 1, 0], [lose.player, rl - loss, 0, 1, 0, 1]].map(([id, r, g, s, w, l]) => [
                    env.DB.prepare("INSERT INTO season_stats (season_id, player_id, rating, peak_rating, gold, silver, wins, losses, matches, updated) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 1, ?) " +
                        "ON CONFLICT (season_id, player_id) DO UPDATE SET rating = excluded.rating, peak_rating = MAX(peak_rating, excluded.rating), gold = gold + excluded.gold, " +
                        "silver = silver + excluded.silver, wins = wins + excluded.wins, losses = losses + excluded.losses, matches = matches + 1, updated = excluded.updated")
                        .bind(season, id, r, Math.max(START_RATING, r), g, s, w, l, now),
                    env.DB.prepare("INSERT INTO career_stats (player_id, gold, silver, wins, losses, ranked_matches, best_rating, best_rating_season, updated) VALUES (?, ?, ?, ?, ?, 1, ?, ?, ?) " +
                        "ON CONFLICT (player_id) DO UPDATE SET gold = gold + excluded.gold, silver = silver + excluded.silver, wins = wins + excluded.wins, losses = losses + excluded.losses, " +
                        "ranked_matches = ranked_matches + 1, best_rating_season = CASE WHEN excluded.best_rating > best_rating THEN excluded.best_rating_season ELSE best_rating_season END, " +
                        "best_rating = MAX(best_rating, excluded.best_rating), updated = excluded.updated")
                        .bind(id, g, s, w, l, Math.max(START_RATING, r), season, now),
                ]).flat(),
                env.DB.prepare("INSERT OR IGNORE INTO season_opponents (season, player, opp) VALUES (?, ?, ?)").bind(season, win.player, lose.player),
                env.DB.prepare("INSERT OR IGNORE INTO season_opponents (season, player, opp) VALUES (?, ?, ?)").bind(season, lose.player, win.player),
            );
        }
        writes.push(outcome(win, wo), outcome(lose, lo));
        try {
            await env.DB.batch(writes);
            return;
        } catch (e) {
            const s = String(e);
            if (s.includes("UNIQUE")) return; // (counted already: the outcome is in the reports)
            if (!s.includes("NOT NULL")) throw e; // a guard: read again, at most three times
            if (attempt === 2) return; // (still moving: the reports stay open; the next report or the sweep settles them)
            await closeSeasons(env, now);
        }
    }
}

function answer(row) {
    if (!row || !row.outcome) return json({ ok: true, state: "waiting" });
    return json({ ok: true, ...JSON.parse(row.outcome) });
}

// The other report of the same battle: the same room (or, from a game that sent none, any room), battle number, names
// both ways, the other side, within PAIR_WINDOW, not counted yet.
const OTHER = "SELECT * FROM reports WHERE n = ? AND player <> ? AND me = ? AND opp = ? AND side <> ? AND at >= ? AND at <= ? AND outcome IS NULL " +
    "AND (code = ? OR code = '' OR ? = '') ORDER BY rid LIMIT 1";
const findOther = (env, mine) => env.DB.prepare(OTHER).bind(mine.n, mine.player, mine.opp, mine.me, mine.side, mine.at - PAIR_WINDOW, mine.at + PAIR_WINDOW, mine.code, mine.code).first();

async function report(env, body, now) {
    const p = await who(env, body);
    if (!p) return json({ error: "who" });
    const rkey = typeof body.rkey === "string" && /^[0-9a-f]{16,32}$/.test(body.rkey) ? body.rkey : null;
    const code = typeof body.code === "string" ? body.code.replace(/[^A-Za-z0-9]/g, "").slice(0, 12).toUpperCase() : "";
    const n = Number.isInteger(body.n) && body.n > 0 && body.n < 100000 ? body.n : 0;
    const side = body.side === 0 || body.side === 1 ? body.side : -1;
    const winner = body.winner === 0 || body.winner === 1 || body.winner === 2 ? body.winner : -1;
    const meName = typeof body.me === "string" ? body.me.slice(0, 24) : "", opp = typeof body.opp === "string" ? body.opp.slice(0, 24) : "";
    if (!rkey || !n || side < 0 || winner < 0 || !meName || !opp || meName === opp) return json({ error: "invalid" });
    let mine = await env.DB.prepare("SELECT * FROM reports WHERE player = ? AND rkey = ?").bind(p.id, rkey).first();
    if (!mine) {
        if (now - p.last_report < REPORT_GAP) return json({ error: "slow" });
        await env.DB.batch([
            env.DB.prepare("INSERT OR IGNORE INTO reports (rkey, player, code, n, me, opp, side, winner, ranked, at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                .bind(rkey, p.id, code, n, meName, opp, side, winner, body.ranked ? 1 : 0, now),
            env.DB.prepare("UPDATE players SET last_report = ? WHERE id = ?").bind(now, p.id),
        ]);
        mine = await env.DB.prepare("SELECT * FROM reports WHERE player = ? AND rkey = ?").bind(p.id, rkey).first();
    }
    if (mine.outcome) return answer(mine);
    const other = await findOther(env, mine);
    if (other) {
        await settle(env, mine, other, now);
        mine = await env.DB.prepare("SELECT * FROM reports WHERE rid = ?").bind(mine.rid).first();
    }
    return answer(mine);
}

// Reports that agree but were never put together (games before 1.0.3: the joiner's report had no room code), and pairs
// a guard left open: settled now. From the cron and, now and then, from requests.
export async function sweep(env, now) {
    const open = await env.DB.prepare("SELECT * FROM reports WHERE outcome IS NULL AND at > ? ORDER BY rid LIMIT 40").bind(now - 6 * DAY).all();
    for (const mine of open.results) {
        const fresh = await env.DB.prepare("SELECT outcome FROM reports WHERE rid = ?").bind(mine.rid).first();
        if (!fresh || fresh.outcome) continue;
        const other = await findOther(env, mine);
        if (other) await settle(env, mine, other, now);
    }
}

// The players with the online screens open: each game says so every two minutes (bt4_profile.c), with a random word of
// its own; counted over the last five minutes. Nothing else about them is kept.
const PRESENCE_MS = 5 * 60000;
async function presence(env, body, now) {
    const w = typeof body.who === "string" && /^[0-9a-z]{8,32}$/.test(body.who) ? body.who : null;
    if (!w) return json({ error: "invalid" });
    if (body.active === false) {
        await env.DB.prepare("DELETE FROM presence WHERE who = ?").bind(w).run();
    } else {
        await env.DB.prepare("INSERT INTO presence (who, at) VALUES (?, ?) ON CONFLICT (who) DO UPDATE SET at = excluded.at").bind(w, now).run();
    }
    if (Math.random() < 0.05) await env.DB.prepare("DELETE FROM presence WHERE at < ?").bind(now - PRESENCE_MS).run();
    const n = await env.DB.prepare("SELECT COUNT(*) AS n FROM presence WHERE at >= ?").bind(now - PRESENCE_MS).first();
    return json({ ok: true, online: n ? n.n : 0 });
}

// Housekeeping: seasons that are over, then (rarely) old reports and claims. The history tables are never pruned.
export async function maintain(env, now) {
    await closeSeasons(env, now);
    await env.DB.batch([
        env.DB.prepare("DELETE FROM reports WHERE at < ?").bind(now - 7 * DAY),
        env.DB.prepare("DELETE FROM settled WHERE at < ?").bind(now - 7 * DAY),
        env.DB.prepare("DELETE FROM pair_day WHERE day < ?").bind(dayOf(now) - 2),
        env.DB.prepare("DELETE FROM ranked_rooms WHERE made < ?").bind(Math.floor(now / 1000) - 7 * 86400),
        env.DB.prepare("DELETE FROM recover_tries WHERE at < ?").bind(now - 2 * 3600000),
    ]);
}

export const PROFILE_PATHS = new Set(["/v1/register", "/v1/rename", "/v1/recover", "/v1/recovery", "/v1/me", "/v1/report", "/v1/top",
                                      "/v1/seasons", "/v1/history", "/v1/matches", "/v1/presence"]);

export async function profileFetch(request, env) {
    const url = new URL(request.url);
    const now = Date.now();
    try {
        await schema(env);
        if (request.method !== "POST") return json({ error: "method" }, 405);
        const text = await request.text();
        if (text.length > 4096) return json({ error: "size" }, 413);
        let body;
        try { body = JSON.parse(text || "{}"); } catch { return json({ error: "json" }, 400); }
        if (!body || typeof body !== "object") return json({ error: "json" }, 400);
        await closeSeasons(env, now); // (the cron does it too; a request after Monday 00:00 must not wait for it)
        const ip = request.headers.get("cf-connecting-ip") || "";
        switch (url.pathname) {
        case "/v1/register": return await register(env, body, ip, now);
        case "/v1/rename": return await rename(env, body, now);
        case "/v1/recover": return await recover(env, body, ip, now);
        case "/v1/recovery": return await recovery(env, body);
        case "/v1/me": return await me(env, body, now);
        case "/v1/report": return await report(env, body, now);
        case "/v1/top": return await top(env, body, now);
        case "/v1/seasons": return json({ ok: true, seasons: await listSeasons(env, body) });
        case "/v1/history": return await history(env, body);
        case "/v1/matches": return await matches(env, body);
        case "/v1/presence": return await presence(env, body, now);
        default: return json({ error: "path" }, 404);
        }
    } catch (e) {
        console.log("bt4-online profile error", String(e && e.stack || e));
        return json({ error: "server" }, 500);
    }
}
