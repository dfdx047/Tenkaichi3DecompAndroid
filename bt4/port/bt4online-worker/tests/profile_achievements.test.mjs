// The ten achievements, the last ten results and the most used characters of /v1/me, through the REAL worker.
import { boot } from "./harness.mjs";

let fails = 0;
const ok = (c, what, extra = "") => { console.log((c ? "ok   " : "FAIL ") + what + (c ? "" : "  " + extra)); if (!c) fails++; };
const h = boot();
const me = (p) => h.post("me", h.auth(p));
const A = await h.register("Conq" + Math.floor(Math.random() * 1e6).toString(36), "198.51.100.1");
const rivals = [];
for (let i = 0; i < 3; i++) rivals.push(await h.register("Riv" + i + Math.floor(Math.random() * 1e6).toString(36), "198.51.100." + (2 + i)));
let r = await me(A);
ok(r.ach === "0000000000" && r.recent === "" && r.used.length === 0, "a new account has none", JSON.stringify([r.ach, r.recent]));
const countries = ["BR", "PT", "JP"];
for (let i = 0; i < 3; i++) await h.env.DB.prepare("UPDATE players SET country = ? WHERE id = ?").bind(countries[i], rivals[i].id).run();
let n = 0;
const fight = async (rival, aWins, room, battle) => {
    n++;
    await h.env.DB.prepare("INSERT INTO ranked_matches (match_key, season_id, played_at, room_code, battle_no, kind, rated, medals, winner_id, loser_id) VALUES (?, 1, ?, ?, ?, 'ranked', 0, 0, ?, ?)")
        .bind("t" + n, 1000 + n, room, battle, aWins ? A.id : rival.id, aWins ? rival.id : A.id).run();
};
await fight(rivals[0], false, "R1", 1);
await fight(rivals[0], true, "R1", 2); // the rematch won
for (let i = 0; i < 9; i++) await fight(rivals[1], true, "R2" + i, 1);
await h.env.DB.prepare("UPDATE career_stats SET wins = 10, ranked_matches = 11 WHERE player_id = ?").bind(A.id).run();
await h.env.DB.prepare("INSERT OR IGNORE INTO career_stats (player_id, wins, losses, ranked_matches, updated) VALUES (?, 10, 1, 11, 0)").bind(A.id).run();
r = await me(A);
ok(r.ach === "1100000100", "first win, ten in a row and the rematch; not yet three continents", r.ach);
ok(r.recent === "VVVVVVVVVV", "the last ten, newest first", r.recent);
await fight(rivals[2], false, "R3", 1);
r = await me(A);
ok(JSON.stringify(r.ach_p) === "[1,10,10,11,0,0,0,1,11,3]", "how far each one is", JSON.stringify(r.ach_p));
ok(r.ach === "1100000101" && r.recent.startsWith("D"), "a rival of a third continent; what was earned stays after a loss", r.ach + " " + r.recent);
await h.env.DB.prepare("INSERT INTO season_history (season_id, player_id, final_rank, final_rating, peak_rating, gold, silver, wins, losses, matches, trophy, name_then) VALUES (1, ?, 7, 1100, 1100, 6, 0, 6, 0, 6, NULL, 'x')").bind(A.id).run();
await h.env.DB.prepare("UPDATE career_stats SET wins = 100, ranked_matches = 500, seasons_played = 5 WHERE player_id = ?").bind(A.id).run();
await h.env.DB.prepare("INSERT INTO char_usage (player_id, chara, battles, name) VALUES (?, 1, 6, 'Trunks'), (?, 2, 3, 'Goku'), (?, 3, 2, 'Vegeta'), (?, 4, 1, 'Cell')").bind(A.id, A.id, A.id, A.id).run();
r = await me(A);
ok(r.ach === "1111111111", "all ten", r.ach);
ok(r.used.length === 3 && r.used[0][0] === "Trunks" && r.used_total === 12, "the three most used and the total", JSON.stringify([r.used, r.used_total]));
console.log(fails ? "\nFAILED " + fails : "\nno failures");
process.exit(fails ? 1 : 0);
