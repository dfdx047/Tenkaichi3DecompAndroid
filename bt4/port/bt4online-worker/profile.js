// BT4 Android's online profile (the game's port/src/gs/bt4_profile.c speaks to it): a unique nickname, the medals of
// online battles, the weekly Ranked season and its trophies.
//
//   POST /v1/register {name}                  -> {ok, id, secret, name}            | {error: "taken"|"invalid"|"slow"}
//   POST /v1/rename   {id, secret, name}      -> {ok, name}                        | {error: "taken"|"invalid"|"who"}
//   POST /v1/me       {id, secret}            -> {ok, name, gold, silver, wins, losses, rating, rwins, rlosses, rank,
//                                                 players, trophies: [gold, silver, bronze], season, ends, last: [..],
//                                                 rated, opponents, eligible}
//   POST /v1/report   {id, secret, rkey, code, n, me, opp, side, winner, ranked}
//                                             -> {ok, state: "waiting"|"settled"|"conflict"|"draw", medal, delta, rating}
//   POST /v1/presence {who}                   -> {ok, online}   (`who`: the game's random word; seen in the last 5 minutes)
//   POST /v1/top      {}                      -> {ok, season, ends, total,
//                                                 top: [[name, rating, wins, losses, gold, silver, trophy1, trophy2, trophy3], ...]}
//
// What makes it fair:
//  - A battle counts only when BOTH players tell of it and agree: the same room code, the same battle number of their
//    session, each other's names, opposite sides and the same winner, within 15 minutes. If they disagree, nothing
//    counts. A battle that ends with a disconnection counts for nobody (as BT4decomp R5.4: only a surrender from the
//    pause menu is a loss, and the game reports it as one).
//  - Medals: the winner a gold one, the loser a silver one (normal battles, rematches and Ranked). Between the same
//    two players, at most MEDALS_PER_PAIR_DAY battles a day give medals (two accounts cannot farm them).
//  - Ranked points (Elo, K 32, from 1000 every week) only for a match this service's Ranked queue made with these two
//    players (rooms.js writes them down in ranked_matches): a room made by hand cannot claim points. Between the same
//    two players at most RATED_PER_PAIR_DAY Ranked battles a day move points; later ones still give medals.
//  - Trophies: a week ends on Monday 00:00 in Brasilia (03:00 UTC); its first three get the gold, silver and bronze
//    trophies, among the players with at least TROPHY_MIN_RATED rated battles that week against at least
//    TROPHY_MIN_OPPONENTS different players. Ties: more points, then more wins, then who got there first.

const WEEK = 604800000;
const DAY = 86400000;
const SEASON_ZERO = 356400000; // Monday 1970-01-05 03:00 UTC (00:00 in Brasilia)
const DAY_ZERO = 10800000;     // 1970-01-01 03:00 UTC (a Brasilia day starts at 03:00 UTC)
const PAIR_WINDOW = 15 * 60000;
const RANKED_ROOM_WINDOW = 2 * 3600; // seconds: a Ranked match's battle reported this long after the match was made at most
const REPORT_GAP = 10000;
const START_RATING = 1000, K = 32, MIN_RATING = 100;
const TOP = 50;
const MEDALS_PER_PAIR_DAY = 20;
const RATED_PER_PAIR_DAY = 3;
const TROPHY_MIN_RATED = 5;
const TROPHY_MIN_OPPONENTS = 3;

const json = (o, status = 200) => new Response(JSON.stringify(o), {
    status, headers: { "content-type": "application/json", "cache-control": "no-store" } });

export function seasonOf(ms) { return Math.floor((ms - SEASON_ZERO) / WEEK); }
export function seasonEnds(season) { return SEASON_ZERO + (season + 1) * WEEK; }
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

let schemaReady = false;
export async function schema(env) {
    if (schemaReady) return;
    await env.DB.batch([
        env.DB.prepare("CREATE TABLE IF NOT EXISTS players (id TEXT PRIMARY KEY, secret_hash TEXT NOT NULL, name TEXT NOT NULL, name_key TEXT NOT NULL UNIQUE, made INTEGER NOT NULL, seen INTEGER NOT NULL, ip_key TEXT NOT NULL DEFAULT '', gold INTEGER NOT NULL DEFAULT 0, silver INTEGER NOT NULL DEFAULT 0, wins INTEGER NOT NULL DEFAULT 0, losses INTEGER NOT NULL DEFAULT 0, last_report INTEGER NOT NULL DEFAULT 0)"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS players_ip ON players (ip_key, made)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS season_ratings (season INTEGER NOT NULL, player TEXT NOT NULL, rating INTEGER NOT NULL, wins INTEGER NOT NULL DEFAULT 0, losses INTEGER NOT NULL DEFAULT 0, updated INTEGER NOT NULL, PRIMARY KEY (season, player))"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS season_rank ON season_ratings (season, rating DESC, wins DESC, updated)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS season_opponents (season INTEGER NOT NULL, player TEXT NOT NULL, opp TEXT NOT NULL, PRIMARY KEY (season, player, opp))"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS pair_day (day INTEGER NOT NULL, a TEXT NOT NULL, b TEXT NOT NULL, battles INTEGER NOT NULL DEFAULT 0, rated INTEGER NOT NULL DEFAULT 0, PRIMARY KEY (day, a, b))"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS reports (rid INTEGER PRIMARY KEY AUTOINCREMENT, rkey TEXT NOT NULL, player TEXT NOT NULL, code TEXT NOT NULL, n INTEGER NOT NULL, me TEXT NOT NULL, opp TEXT NOT NULL, side INTEGER NOT NULL, winner INTEGER NOT NULL, ranked INTEGER NOT NULL, at INTEGER NOT NULL, outcome TEXT, UNIQUE (player, rkey))"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS reports_pair ON reports (code, n, at)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS settled (first INTEGER PRIMARY KEY, second INTEGER NOT NULL, at INTEGER NOT NULL)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS trophies (season INTEGER NOT NULL, place INTEGER NOT NULL, player TEXT NOT NULL, rating INTEGER NOT NULL, PRIMARY KEY (season, place))"),
        env.DB.prepare("CREATE INDEX IF NOT EXISTS trophies_player ON trophies (player)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS meta (k TEXT PRIMARY KEY, v INTEGER NOT NULL)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS presence (who TEXT PRIMARY KEY, at INTEGER NOT NULL)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS ranked_matches (code TEXT PRIMARY KEY, made INTEGER NOT NULL, host_name TEXT NOT NULL, join_name TEXT NOT NULL)"),
    ]);
    schemaReady = true;
}

// The season's table as the trophies see it: eligible players first by points, wins, who got there first.
const ELIGIBLE = "s.wins + s.losses >= " + TROPHY_MIN_RATED +
    " AND (SELECT COUNT(*) FROM season_opponents o WHERE o.season = s.season AND o.player = s.player) >= " + TROPHY_MIN_OPPONENTS;

async function closeSeasons(env, now) {
    const current = seasonOf(now);
    const row = await env.DB.prepare("SELECT v FROM meta WHERE k = 'closed'").first();
    const closed = row ? row.v : -1;
    if (closed >= current - 1) return;
    const open = await env.DB.prepare("SELECT DISTINCT season FROM season_ratings WHERE season > ? AND season < ? ORDER BY season")
        .bind(closed, current).all();
    for (const { season } of open.results) {
        const top = await env.DB.prepare("SELECT s.player, s.rating FROM season_ratings s WHERE s.season = ? AND " + ELIGIBLE +
            " ORDER BY s.rating DESC, s.wins DESC, s.updated LIMIT 3").bind(season).all();
        const writes = top.results.map((r, i) => env.DB.prepare("INSERT OR IGNORE INTO trophies (season, place, player, rating) VALUES (?, ?, ?, ?)")
            .bind(season, i + 1, r.player, r.rating));
        if (writes.length) await env.DB.batch(writes);
    }
    await env.DB.prepare("INSERT INTO meta (k, v) VALUES ('closed', ?) ON CONFLICT (k) DO UPDATE SET v = excluded.v WHERE excluded.v > meta.v")
        .bind(current - 1).run();
    await env.DB.batch([
        env.DB.prepare("DELETE FROM reports WHERE at < ?").bind(now - 7 * DAY),
        env.DB.prepare("DELETE FROM settled WHERE at < ?").bind(now - 7 * DAY),
        env.DB.prepare("DELETE FROM pair_day WHERE day < ?").bind(dayOf(now) - 2),
        env.DB.prepare("DELETE FROM ranked_matches WHERE made < ?").bind(Math.floor(now / 1000) - 7 * 86400),
    ]);
}

async function who(env, body) {
    if (typeof body.id !== "string" || typeof body.secret !== "string" || body.id.length !== 32 || body.secret.length !== 64) return null;
    const p = await env.DB.prepare("SELECT * FROM players WHERE id = ?").bind(body.id).first();
    if (!p || p.secret_hash !== await sha256(body.secret)) return null;
    return p;
}

async function register(env, body, ip, now) {
    const name = cleanName(body.name);
    if (!name) return json({ error: "invalid" });
    const ipKey = (await sha256("bt4-online|" + ip)).slice(0, 24);
    const recent = await env.DB.prepare("SELECT COUNT(*) AS n FROM players WHERE ip_key = ? AND made > ?").bind(ipKey, now - 3600000).first();
    if (recent && recent.n >= 30) return json({ error: "slow" }); // (mobile carriers share addresses: generous)
    const id = randomHex(16), secret = randomHex(32);
    try {
        await env.DB.prepare("INSERT INTO players (id, secret_hash, name, name_key, made, seen, ip_key) VALUES (?, ?, ?, ?, ?, ?, ?)")
            .bind(id, await sha256(secret), name, nameKey(name), now, now, ipKey).run();
    } catch (e) {
        if (String(e).includes("UNIQUE")) return json({ error: "taken" });
        throw e;
    }
    return json({ ok: true, id, secret, name });
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

async function me(env, body, now) {
    const p = await who(env, body);
    if (!p) return json({ error: "who" });
    const season = seasonOf(now);
    const [r, trophies, players, last, opps] = await env.DB.batch([
        env.DB.prepare("SELECT rating, wins, losses, updated FROM season_ratings WHERE season = ? AND player = ?").bind(season, p.id),
        env.DB.prepare("SELECT place, COUNT(*) AS n FROM trophies WHERE player = ? GROUP BY place").bind(p.id),
        env.DB.prepare("SELECT COUNT(*) AS n FROM season_ratings WHERE season = ? AND wins + losses > 0").bind(season),
        env.DB.prepare("SELECT t.place, p.name, t.rating FROM trophies t JOIN players p ON p.id = t.player WHERE t.season = ? ORDER BY t.place").bind(season - 1),
        env.DB.prepare("SELECT COUNT(*) AS n FROM season_opponents WHERE season = ? AND player = ?").bind(season, p.id),
    ]);
    const mine = r.results[0];
    let rank = 0;
    if (mine && mine.wins + mine.losses > 0) {
        const above = await env.DB.prepare("SELECT COUNT(*) AS n FROM season_ratings WHERE season = ? AND wins + losses > 0 AND (rating > ? OR (rating = ? AND wins > ?) OR (rating = ? AND wins = ? AND updated < ?))")
            .bind(season, mine.rating, mine.rating, mine.wins, mine.rating, mine.wins, mine.updated).first();
        rank = (above ? above.n : 0) + 1;
    }
    const t = [0, 0, 0];
    for (const row of trophies.results) if (row.place >= 1 && row.place <= 3) t[row.place - 1] = row.n;
    if (now - p.seen > 3600000) await env.DB.prepare("UPDATE players SET seen = ? WHERE id = ?").bind(now, p.id).run();
    const rated = mine ? mine.wins + mine.losses : 0, opponents = opps.results[0] ? opps.results[0].n : 0;
    return json({
        ok: true, name: p.name, gold: p.gold, silver: p.silver, wins: p.wins, losses: p.losses,
        rating: mine ? mine.rating : START_RATING, rwins: mine ? mine.wins : 0, rlosses: mine ? mine.losses : 0,
        rank, players: players.results[0] ? players.results[0].n : 0, trophies: t,
        season, ends: seasonEnds(season), last: last.results.map((x) => [x.place, x.name, x.rating]),
        rated, opponents, eligible: rated >= TROPHY_MIN_RATED && opponents >= TROPHY_MIN_OPPONENTS,
    });
}

async function top(env, now) {
    const season = seasonOf(now);
    const rows = await env.DB.prepare("SELECT p.name, s.rating, s.wins, s.losses, p.gold, p.silver, " +
        "(SELECT COUNT(*) FROM trophies t WHERE t.player = p.id AND t.place = 1) AS t1, " +
        "(SELECT COUNT(*) FROM trophies t WHERE t.player = p.id AND t.place = 2) AS t2, " +
        "(SELECT COUNT(*) FROM trophies t WHERE t.player = p.id AND t.place = 3) AS t3 " +
        "FROM season_ratings s JOIN players p ON p.id = s.player WHERE s.season = ? AND s.wins + s.losses > 0 ORDER BY s.rating DESC, s.wins DESC, s.updated LIMIT ?")
        .bind(season, TOP).all();
    const total = await env.DB.prepare("SELECT COUNT(*) AS n FROM season_ratings WHERE season = ? AND wins + losses > 0").bind(season).first();
    return json({ ok: true, season, ends: seasonEnds(season), total: total ? total.n : 0,
                  top: rows.results.map((r) => [r.name, r.rating, r.wins, r.losses, r.gold, r.silver, r.t1, r.t2, r.t3]) });
}

// Whether a battle's room is a Ranked match this service made, with these two players.
async function rankedRoom(env, code, nameA, nameB, nowMs) {
    if (!code) return false;
    const m = await env.DB.prepare("SELECT * FROM ranked_matches WHERE code = ? AND made >= ?").bind(code, Math.floor(nowMs / 1000) - RANKED_ROOM_WINDOW).first();
    if (!m) return false;
    const h = looseKey(m.host_name), j = looseKey(m.join_name), a = looseKey(nameA), b = looseKey(nameB);
    return h !== "" && j !== "" && h !== j && ((h === a && j === b) || (h === b && j === a));
}

async function settle(env, a, b, now) {
    const first = Math.min(a.rid, b.rid), second = Math.max(a.rid, b.rid);
    const claim = await env.DB.prepare("INSERT OR IGNORE INTO settled (first, second, at) VALUES (?, ?, ?)").bind(first, second, now).run();
    if (!claim.meta || claim.meta.changes !== 1) return;
    const outcome = (rows, o) => env.DB.batch(rows.map((r) => env.DB.prepare("UPDATE reports SET outcome = ? WHERE rid = ?").bind(JSON.stringify(o), r.rid)));
    if (a.winner !== b.winner) return outcome([a, b], { state: "conflict" });
    if (a.winner === 2) return outcome([a, b], { state: "draw" });
    const win = a.side === a.winner ? a : b, lose = win === a ? b : a;
    const day = dayOf(now), pa = win.player < lose.player ? win.player : lose.player, pb = pa === win.player ? lose.player : win.player;
    await env.DB.prepare("INSERT OR IGNORE INTO pair_day (day, a, b) VALUES (?, ?, ?)").bind(day, pa, pb).run();
    const pair = await env.DB.prepare("SELECT battles, rated FROM pair_day WHERE day = ? AND a = ? AND b = ?").bind(day, pa, pb).first();
    const medals = !pair || pair.battles < MEDALS_PER_PAIR_DAY;
    const ranked = a.ranked === 1 && b.ranked === 1 && await rankedRoom(env, a.code, win.me, lose.me, now);
    const rated = ranked && (!pair || pair.rated < RATED_PER_PAIR_DAY);
    const writes = [env.DB.prepare("UPDATE pair_day SET battles = battles + 1, rated = rated + ? WHERE day = ? AND a = ? AND b = ?").bind(rated ? 1 : 0, day, pa, pb)];
    if (medals) {
        writes.push(env.DB.prepare("UPDATE players SET gold = gold + 1, wins = wins + 1 WHERE id = ?").bind(win.player));
        writes.push(env.DB.prepare("UPDATE players SET silver = silver + 1, losses = losses + 1 WHERE id = ?").bind(lose.player));
    }
    let wo = { state: "settled", medal: medals ? "gold" : "none", ranked }, lo = { state: "settled", medal: medals ? "silver" : "none", ranked };
    if (rated) {
        const season = seasonOf(now);
        await env.DB.batch([
            ...[win.player, lose.player].map((id) => env.DB.prepare("INSERT OR IGNORE INTO season_ratings (season, player, rating, updated) VALUES (?, ?, ?, ?)")
                .bind(season, id, START_RATING, now)),
            env.DB.prepare("INSERT OR IGNORE INTO season_opponents (season, player, opp) VALUES (?, ?, ?)").bind(season, win.player, lose.player),
            env.DB.prepare("INSERT OR IGNORE INTO season_opponents (season, player, opp) VALUES (?, ?, ?)").bind(season, lose.player, win.player),
        ]);
        const rows = await env.DB.prepare("SELECT player, rating FROM season_ratings WHERE season = ? AND player IN (?, ?)").bind(season, win.player, lose.player).all();
        const rating = Object.fromEntries(rows.results.map((r) => [r.player, r.rating]));
        const rw = rating[win.player] ?? START_RATING, rl = rating[lose.player] ?? START_RATING;
        const gain = elo(rw, rl), loss = Math.min(gain, Math.max(0, rl - MIN_RATING));
        writes.push(env.DB.prepare("UPDATE season_ratings SET rating = rating + ?, wins = wins + 1, updated = ? WHERE season = ? AND player = ?").bind(gain, now, season, win.player));
        writes.push(env.DB.prepare("UPDATE season_ratings SET rating = rating - ?, losses = losses + 1, updated = ? WHERE season = ? AND player = ?").bind(loss, now, season, lose.player));
        wo = { ...wo, delta: gain, rating: rw + gain };
        lo = { ...lo, delta: -loss, rating: rl - loss };
    } else if (ranked) {
        wo.capped = lo.capped = true; // (a Ranked battle past the day's limit against this player: no points)
    }
    writes.push(env.DB.prepare("UPDATE reports SET outcome = ? WHERE rid = ?").bind(JSON.stringify(wo), win.rid));
    writes.push(env.DB.prepare("UPDATE reports SET outcome = ? WHERE rid = ?").bind(JSON.stringify(lo), lose.rid));
    await env.DB.batch(writes);
}

function answer(row) {
    if (!row || !row.outcome) return json({ ok: true, state: "waiting" });
    return json({ ok: true, ...JSON.parse(row.outcome) });
}

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
    const other = await env.DB.prepare("SELECT * FROM reports WHERE code = ? AND n = ? AND player <> ? AND me = ? AND opp = ? AND side <> ? AND at >= ? AND at <= ? AND outcome IS NULL ORDER BY rid LIMIT 1")
        .bind(mine.code, mine.n, p.id, mine.opp, mine.me, mine.side, mine.at - PAIR_WINDOW, mine.at + PAIR_WINDOW).first();
    if (other) {
        await settle(env, mine, other, now);
        mine = await env.DB.prepare("SELECT * FROM reports WHERE rid = ?").bind(mine.rid).first();
    }
    return answer(mine);
}

// The players with the online screens open: each game says so every two minutes (bt4_profile.c), with a random word of
// its own; counted over the last five minutes. Nothing else about them is kept.
const PRESENCE_MS = 5 * 60000;
async function presence(env, body, now) {
    const who = typeof body.who === "string" && /^[0-9a-z]{8,32}$/.test(body.who) ? body.who : null;
    if (!who) return json({ error: "invalid" });
    if (body.active === false) {
        await env.DB.prepare("DELETE FROM presence WHERE who = ?").bind(who).run();
    } else {
        await env.DB.prepare("INSERT INTO presence (who, at) VALUES (?, ?) ON CONFLICT (who) DO UPDATE SET at = excluded.at").bind(who, now).run();
    }
    if (Math.random() < 0.05) await env.DB.prepare("DELETE FROM presence WHERE at < ?").bind(now - PRESENCE_MS).run();
    const n = await env.DB.prepare("SELECT COUNT(*) AS n FROM presence WHERE at >= ?").bind(now - PRESENCE_MS).first();
    return json({ ok: true, online: n ? n.n : 0 });
}

export const PROFILE_PATHS = new Set(["/v1/register", "/v1/rename", "/v1/me", "/v1/report", "/v1/top", "/v1/presence"]);

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
        await closeSeasons(env, now);
        const ip = request.headers.get("cf-connecting-ip") || "";
        switch (url.pathname) {
        case "/v1/register": return await register(env, body, ip, now);
        case "/v1/rename": return await rename(env, body, now);
        case "/v1/me": return await me(env, body, now);
        case "/v1/report": return await report(env, body, now);
        case "/v1/top": return await top(env, now);
        case "/v1/presence": return await presence(env, body, now);
        default: return json({ error: "path" }, 404);
        }
    } catch (e) {
        console.log("bt4-online profile error", String(e && e.stack || e));
        return json({ error: "server" }, 500);
    }
}
