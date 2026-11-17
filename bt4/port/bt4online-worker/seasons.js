// BT4 Android Ranked: the weekly seasons (docs/RANKED_TEMPORADAS_ANDROID.md). A season is a week, from Monday 00:00 in
// Brasilia (03:00 UTC); its id is the week's number since 1970 (the numbers the service used before seasons had rows),
// its name counts from the first season of this service. "The season restarts, the player's history is never erased":
// a new season is a new id, its rows start at 1000 and zero; nothing of an old one is deleted, and its end writes the
// final table into season_history once (with the trophies) before the next season can open.

export const WEEK = 604800000;
export const SEASON_ZERO = 356400000; // Monday 1970-01-05 03:00 UTC (00:00 in Brasilia)
export const FIRST_SEASON = 2967;     // the week of Monday 2026-11-16 (03:00 UTC): "Temporada 1"
export const TROPHY_MIN_RATED = 5;
export const TROPHY_MIN_OPPONENTS = 3;
const CLOSE_RETRY_MS = 10 * 60000;   // a close that did not finish is taken up again after this

export function seasonOf(ms) { return Math.floor((ms - SEASON_ZERO) / WEEK); }
export function seasonStarts(id) { return SEASON_ZERO + id * WEEK; }
export function seasonEnds(id) { return SEASON_ZERO + (id + 1) * WEEK; }
export function seasonNumber(id) { return id - FIRST_SEASON + 1; }
export function seasonName(id) {
    const n = seasonNumber(id);
    return n >= 1 ? "Temporada " + n : "Pre-temporada";
}

// The season's row (made by the first battle of the week that counts). An INSERT that would make a second 'active' row
// fails (index seasons_one_active): the week before has to be closed first.
export function ensureSeasonStatement(env, id) {
    const n = seasonNumber(id);
    return env.DB.prepare("INSERT OR IGNORE INTO seasons (id, slug, name, starts_at, ends_at, status) VALUES (?, ?, ?, ?, ?, 'active')")
        .bind(id, "S" + String(Math.max(n, 0)).padStart(2, "0"), seasonName(id), seasonStarts(id), seasonEnds(id));
}

// Who is in the race for the trophies: TROPHY_MIN_RATED rated battles against TROPHY_MIN_OPPONENTS different players.
const ELIGIBLE = (alias) => `${alias}.matches >= ${TROPHY_MIN_RATED} AND (SELECT COUNT(*) FROM season_opponents o WHERE o.season = ${alias}.season_id AND o.player = ${alias}.player_id) >= ${TROPHY_MIN_OPPONENTS}`;
export const RANK_ORDER = "rating DESC, wins DESC, updated";

// Closes every season whose week is over (idempotent: the cron and any request may call it at the same time).
export async function closeSeasons(env, now) {
    const due = await env.DB.prepare("SELECT id FROM seasons WHERE status IN ('active','closing') AND ends_at <= ? ORDER BY id").bind(now).all();
    for (const { id } of due.results) {
        // the claim: one closer at a time; a claim older than CLOSE_RETRY_MS (a close that failed) can be taken again
        const claim = await env.DB.prepare("UPDATE seasons SET status = 'closing', closed_at = ? WHERE id = ? AND (status = 'active' OR (status = 'closing' AND closed_at < ?))")
            .bind(now, id, now - CLOSE_RETRY_MS).run();
        if (!claim.meta || claim.meta.changes !== 1) continue;
        // archive, trophies, careers and the season's state: one transaction (all of it or none of it)
        await env.DB.batch([
            env.DB.prepare(
                "INSERT OR IGNORE INTO season_history (season_id, player_id, final_rank, final_rating, peak_rating, gold, silver, wins, losses, matches, trophy, name_then) " +
                "SELECT s.season_id, s.player_id, r.rk, s.rating, s.peak_rating, s.gold, s.silver, s.wins, s.losses, s.matches, " +
                "CASE WHEN r.rk <= 3 THEN r.rk END, p.name " +
                "FROM season_stats s JOIN players p ON p.id = s.player_id " +
                "LEFT JOIN (SELECT e.player_id, ROW_NUMBER() OVER (ORDER BY e.rating DESC, e.wins DESC, e.updated) AS rk " +
                "           FROM season_stats e WHERE e.season_id = ? AND " + ELIGIBLE("e") + ") r ON r.player_id = s.player_id " +
                "WHERE s.season_id = ?").bind(id, id),
            env.DB.prepare("INSERT OR IGNORE INTO trophies (season, place, player, rating) SELECT season_id, final_rank, player_id, final_rating FROM season_history WHERE season_id = ? AND final_rank <= 3").bind(id),
            env.DB.prepare("UPDATE career_stats SET seasons_played = seasons_played + 1 WHERE player_id IN (SELECT player_id FROM season_stats WHERE season_id = ? AND matches > 0)").bind(id),
            env.DB.prepare("UPDATE seasons SET status = 'closed', closed_at = ?, players = (SELECT COUNT(*) FROM season_stats WHERE season_id = ? AND matches > 0) WHERE id = ? AND status = 'closing'")
                .bind(now, id, id),
        ]);
    }
}

// /v1/seasons {page}: the seasons, newest first (10 a page): [id, slug, name, starts, ends, status, players]
export async function listSeasons(env, body) {
    const page = Number.isInteger(body.page) && body.page >= 0 && body.page < 10000 ? body.page : 0;
    const rows = await env.DB.prepare("SELECT id, slug, name, starts_at, ends_at, status, players FROM seasons ORDER BY id DESC LIMIT 10 OFFSET ?")
        .bind(page * 10).all();
    return rows.results.map((r) => [r.id, r.slug, r.name, r.starts_at, r.ends_at, r.status, r.players]);
}
