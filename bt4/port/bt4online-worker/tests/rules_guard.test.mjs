// A guard: the competitive rules that were NOT to be changed (Elo, seasons, medals) are pinned. If this test fails, someone changed a rule: it needs the owner's authorization.
//   node --no-warnings rules_guard.test.mjs
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { elo, RANKED_PER_PAIR_DAY } from "../profile.js";
import { seasonOf } from "../seasons.js";
import { boot } from "./harness.mjs";

let fails = 0;
const ok = (c, what, extra = "") => { console.log((c ? "ok   " : "FAIL ") + what + (c ? "" : "  " + extra)); if (!c) fails++; };
const here = new URL("../", import.meta.url).pathname;

// seasons.js is byte for byte what it was before the audit began (sha256 of the file at commit d9021b8f)
const sha = createHash("sha256").update(readFileSync(here + "seasons.js")).digest("hex");
ok(sha === "5e05d52c8b18e36f0dcaaa85a59a5f840393c6cc9f1662f75049d8449e659cdd", "seasons.js is unchanged (weekly seasons, Monday 00:00 Brasilia)", sha);
const src = readFileSync(here + "profile.js", "utf8");
ok(/const START_RATING = 1000, K = 32, MIN_RATING = 100;/.test(src) && /export const RANKED_PER_PAIR_DAY = 4;/.test(src), "start rating 1000, K 32, floor 100 and 4 battles a day per pair are the same constants");
ok(RANKED_PER_PAIR_DAY === 4, "the Worker's limit is still 4");
const body = src.slice(src.indexOf("export function elo"), src.indexOf("async function sha256"));
ok(/Math\.max\(1, Math\.round\(K \* \(1 - expected\)\)\)/.test(body) && /Math\.pow\(10, \(loser - winner\) \/ 400\)/.test(body), "elo() is the same formula");

// the Elo table
const golden = [[1000, 1000, 16], [1016, 984, 15], [1200, 1000, 8], [1000, 1200, 24], [1500, 1480, 15], [900, 1100, 24], [1800, 100, 1], [100, 1800, 32]];
ok(golden.every(([a, b, g]) => elo(a, b) === g), "the Elo gains for eight pairs of ratings are the same", JSON.stringify(golden.map(([a, b]) => [a, b, elo(a, b)])));

// seasons: the boundary and the season's identity
const mon = Date.UTC(2026, 10, 23, 3, 0, 0);
ok(seasonOf(mon) === seasonOf(mon + 6 * 86400000 + 23 * 3600000) && seasonOf(mon - 1000) === seasonOf(mon) - 1, "a season is a week that starts Monday 00:00 in Brasilia (03:00 UTC)");

// medals and points through the real flow: gold and silver, +16/-16, nothing for a casual or a capped battle
const h = boot();
const A = await h.register("GuardA" + Math.floor(Math.random() * 1e5), "198.51.100.1"), B = await h.register("GuardB" + Math.floor(Math.random() * 1e5), "198.51.100.2");
const code = await h.rankedRoom(A, B, true);
const rep = async (n, winner) => { await h.rep(A, { rkey: h.rk(), code, n, me: A.name, opp: B.name, side: 0, winner, ranked: true }); h.clock.advance(11000); const r = await h.rep(B, { rkey: h.rk(), code, n, me: B.name, opp: A.name, side: 1, winner, ranked: true }); h.clock.advance(11000); return r; };
const r1 = await rep(1, 0);
ok(r1.state === "settled" && r1.medal === "silver" && r1.delta === -16, "the loser gets the silver medal and -16; the winner gold and +16", JSON.stringify(r1));
const ma = await h.post("me", h.auth(A));
ok(ma.gold === 1 && ma.silver === 0 && ma.rating === 1016, "the winner's profile: one gold, rating 1016", JSON.stringify([ma.gold, ma.rating]));
const r2 = await rep(2, 0), r3 = await rep(3, 1), r4 = await rep(4, 0), r5 = await rep(5, 0);
ok(r2.delta === -15 && r3.medal === "gold" && r4.delta < 0 && r5.capped === true && r5.medal === "none", "battles 2-4 move points, the 5th of the pair's day is capped with no medal", JSON.stringify([r2.delta, r3.medal, r4.delta, r5]));

// the SQL: the same Elo through the real function, and its season boundary
const {db} = await (await import("../../../supabase/tests/pglite/harness.mjs")).boot();
const q = async (s, p) => (await db.query(s, p)).rows;
await db.exec(`update bt4_private.ranked_config set enabled=true,authority_backend=1; update bt4_private.route_config set backend_id=1,admission_open=true; select bt4_private.ranked_rollover();`);
const sid = (await q(`select (bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date s`))[0].s;
let sqlOk = true, sqlDetail = "";
for (const [a, b] of [[1000, 1000], [1016, 984], [1200, 1000], [1000, 1200], [1500, 1480], [900, 1100]]) {
    const ua = (await q("insert into auth.users default values returning id"))[0].id, ub = (await q("insert into auth.users default values returning id"))[0].id;
    for (const [u, n, r] of [[ua, "A", a], [ub, "B", b]]) {
        await q(`insert into bt4_private.ranked_players(ruleset,user_id,name,rating) values('solo',$1,$2,$3)`, [u, n + u.slice(0, 6), r]);
        await q(`insert into bt4_private.ranked_profiles(season,ruleset,user_id,name,rating) values($1,'solo',$2,$3,$4)`, [sid, u, n + u.slice(0, 6), r]);
    }
    const m = crypto.randomUUID();
    await q(`insert into bt4_private.ranked_matches(id,season,ruleset,player_a,player_b) values($1,$2,'solo',$3,$4)`, [m, sid, ua, ub]);
    const api = async (u, args) => { await db.exec(`select set_config('request.jwt.claim.sub','${u}',false)`); return (await q("select public.bt4_ranked_api('report',$1::jsonb) r", [JSON.stringify(args)]))[0].r; };
    await api(ua, { match: m, outcome: 0, digest: "a".repeat(64) }); await api(ub, { match: m, outcome: 0, digest: "a".repeat(64) });
    const got = (await q(`select rating from bt4_private.ranked_players where user_id=$1`, [ua]))[0].rating - a;
    if (got !== elo(a, b)) { sqlOk = false; sqlDetail += ` (${a},${b}): sql ${got} vs worker ${elo(a, b)}`; }
}
ok(sqlOk, "the SQL's Elo gain equals the Worker's for six pairs of ratings", sqlDetail);
const sb = await q(`select bt4_private.ranked_season_at('2026-11-22 23:58:59-03'::timestamptz) a, bt4_private.ranked_season_at('2026-11-22 23:59:00-03'::timestamptz) b`);
ok(new Date(sb[0].b) - new Date(sb[0].a) === 7 * 86400000, "the SQL's season boundary is still Sunday 23:59 in Buenos Aires (one week apart across it)", JSON.stringify(sb[0]));
console.log(fails ? `${fails} FAILED` : "rules unchanged");
process.exit(fails ? 1 : 0);
