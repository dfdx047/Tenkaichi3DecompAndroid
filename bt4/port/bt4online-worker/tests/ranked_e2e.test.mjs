// Two players, spectators, reconnections, duplicate and concurrent reports, through the REAL worker (rooms + profile), in process (D1 shim, fake clock).
// It simulates the HTTP side of what the games do; it does not run a game, a network or a device.   node --no-warnings ranked_e2e.test.mjs
import { boot } from "./harness.mjs";

let fails = 0;
const ok = (c, what, extra = "") => { console.log((c ? "ok   " : "FAIL ") + what + (c ? "" : "  " + extra)); if (!c) fails++; };
const h = boot();
let u = 0;
const mk = async (n) => h.register(n + (++u) + Math.floor(Math.random() * 1e5).toString(36), "198.51.100." + (1 + (u % 200)));
const me = (p) => h.post("me", h.auth(p));
const tick = (ms = 11000) => h.clock.advance(ms);
const V = 140001;
const rows = async (sql, ...a) => (await h.env.DB.prepare(sql).bind(...a).all()).results;

// ---- E1: two players and spectators
{
    const A = await mk("Aa"), B = await mk("Bb"), S = await mk("Sp"), S2 = await mk("Sq");
    const r = await h.rankedRoom(A, B, true, { full: true });
    const poll = await h.post("poll", { code: r.code, key: r.host.key, watch: true, keep: true });
    ok(poll.peer && poll.peer.name === B.name, "the host's poll finds the joiner", JSON.stringify(poll));
    const w = await h.post("watch", { v: V, code: r.code, name: S.name, addrs: ["10.1.1.9:7000"] }, "203.0.113.50");
    const w2 = await h.post("watch", { v: V, code: r.code, name: S2.name, addrs: ["10.1.1.8:7000"] }, "203.0.113.51");
    ok(w.key && w2.key, "two spectators can watch the ranked room (they get the host's addresses, as before)", JSON.stringify([w, w2]));
    const poll2 = await h.post("poll", { code: r.code, key: r.host.key, watch: true, keep: true });
    ok(Array.isArray(poll2.watchers) && poll2.watchers.length === 2, "the host's poll lists both spectators", JSON.stringify(poll2.watchers));
    // the spectators try to take part in the result, every way they can
    const attempts = [];
    for (const [sp, side] of [[S, 0], [S2, 1]]) {
        attempts.push(await h.rep(sp, { rkey: h.rk(), code: r.code, n: 1, me: side ? B.name : A.name, opp: side ? A.name : B.name, side, winner: 0, ranked: true }));
        tick();
    }
    ok(attempts.every((x) => x.error === "not admitted"), "neither spectator can report the battle (they hold the code and the names)", JSON.stringify(attempts));
    await h.rep(A, { rkey: h.rk(), code: r.code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    tick();
    const done = await h.rep(B, { rkey: h.rk(), code: r.code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    ok(done.state === "settled" && done.delta === -16, "the two players' reports settle the battle once the spectators were turned away", JSON.stringify(done));
    ok((await me(S)).rwins === 0 && (await me(S)).rlosses === 0 && (await me(S2)).rating === 1000, "the spectators' accounts did not move");
}

// ---- E2: reconnections
{
    h.clock.advance(3600000);
    const A = await mk("Ra"), B = await mk("Rb"), E = await mk("Re");
    const host = await h.post("quick", { v: V, rel: 140, name: A.name, addrs: ["10.2.2.1:7000"], queue: "D20", who: "reconhost" + u, waited: 0, area: "any", acct: { id: A.id, secret: A.secret } }, "203.0.113.60");
    // (a) the host's connection drops and it searches again from its room with its key: the same room, the same seat
    const again = await h.post("quick", { v: V, rel: 140, name: A.name, addrs: ["10.2.2.1:7000"], queue: "D20", who: "reconhost" + u, waited: 12, area: "any", code: host.code, key: host.key, acct: { id: A.id, secret: A.secret } }, "203.0.113.60");
    const seats1 = await rows("SELECT seat, player FROM ranked_seats WHERE code = ?", host.code);
    ok(again.role === "host" && again.code === host.code && seats1.length === 1 && seats1[0].player === A.id, "a host that reconnects keeps its room and its seat", JSON.stringify([again, seats1]));
    // (b) another account holding the host's key cannot take the seat over while the host is away
    const hijack = await h.post("quick", { v: V, rel: 140, name: A.name, addrs: ["10.2.2.1:7000"], queue: "D20", who: "reconhost" + u, waited: 20, area: "any", code: host.code, key: host.key, acct: { id: E.id, secret: E.secret } }, "203.0.113.61");
    ok(hijack.error === "identity", "another account cannot take the host's seat during a reconnection", JSON.stringify(hijack));
    // (c) the joiner arrives; the host reconnects AFTER someone joined: told so, nothing changes
    const join = await h.post("quick", { v: V, rel: 140, name: B.name, addrs: ["10.2.2.2:7000"], queue: "D20", who: "reconjoin" + u, waited: 0, area: "any", acct: { id: B.id, secret: B.secret } }, "203.0.113.62");
    const late = await h.post("quick", { v: V, rel: 140, name: A.name, addrs: ["10.2.2.1:7000"], queue: "D20", who: "reconhost" + u, waited: 30, area: "any", code: host.code, key: host.key, acct: { id: A.id, secret: A.secret } }, "203.0.113.60");
    ok(join.role === "join" && late.joined === true, "the joiner is put in; the reconnecting host is told someone has joined", JSON.stringify([join.role, late]));
    const seats2 = await rows("SELECT seat, player FROM ranked_seats WHERE code = ? ORDER BY seat", host.code);
    ok(seats2.length === 2 && seats2.find((s) => s.seat === "host").player === A.id && seats2.find((s) => s.seat === "join").player === B.id, "the two seats hold the two admitted accounts", JSON.stringify(seats2));
    // (d) a stranger asks to join the same room by its code: the room is taken, nothing is bound to the stranger
    const stranger = await h.post("join", { v: V, code: host.code, name: E.name, addrs: ["10.2.2.3:7000"], acct: { id: E.id, secret: E.secret } }, "203.0.113.63");
    const seats3 = await rows("SELECT seat, player FROM ranked_seats WHERE code = ?", host.code);
    ok(seats3.length === 2 && !seats3.some((s) => s.player === E.id), "a stranger joining the room by code does not get a seat", JSON.stringify([stranger.error, seats3.length]));
    // (e) B's app restarts after the fight: it sends the SAME report again (the first answer was lost), then the host does; settles once
    const key = h.rk();
    await h.rep(B, { rkey: key, code: host.code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    const resent = await h.rep(B, { rkey: key, code: host.code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    tick();
    const done = await h.rep(A, { rkey: h.rk(), code: host.code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true }); // the host reports AFTER the joiner (reverse order)
    ok(resent.state === "waiting" && done.state === "settled" && done.delta === 16, "a resent report waits, the other side's report (in the reverse order) settles once", JSON.stringify([resent, done]));
    // (f) after the fight the restarted app sends a fresh report of the SAME fight (new key): not counted again
    tick();
    const replay = await h.rep(B, { rkey: h.rk(), code: host.code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    tick();
    const replayA = await h.rep(A, { rkey: h.rk(), code: host.code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    ok(!replay.delta && !replayA.delta && (await me(A)).rwins === 1, "a replay of the same fight under new keys is not counted again", JSON.stringify([replay, replayA, (await me(A)).rwins]));
}
// (g) the account's secret changes between the two reports (recovery on another device): the old device is refused, the new one settles
{
    h.clock.advance(3600000);
    const A = await mk("Sa"), B = await mk("Sb");
    const code = await h.rankedRoom(A, B, true);
    await h.rep(A, { rkey: h.rk(), code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    const back = await h.post("recover", { name: B.name, code: B.recovery });
    tick();
    const old = await h.rep(B, { rkey: h.rk(), code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    const fresh = await h.rep({ id: back.id, secret: back.secret }, { rkey: h.rk(), code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    ok(old.error === "who" && fresh.state === "settled", "after a recovery the old device's secret is refused and the new one settles the same battle", JSON.stringify([old, fresh]));
}
// (h) both claim the same side, or contradict the winner: nobody scores
{
    h.clock.advance(3600000);
    const A = await mk("Ta"), B = await mk("Tb");
    const code = await h.rankedRoom(A, B, true);
    await h.rep(A, { rkey: h.rk(), code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    tick();
    const same = await h.rep(B, { rkey: h.rk(), code, n: 1, me: B.name, opp: A.name, side: 0, winner: 0, ranked: true });
    ok(same.state === "waiting" && (await me(A)).rwins === 0, "two reports of the same side do not pair", JSON.stringify(same));
}

// ---- E3: concurrency and duplicates, many pairs at once. Invariants checked against the database itself.
{
    h.clock.advance(86400000 * 2);
    const PAIRS = 10, FIGHTS = 3;
    const pairs = [];
    for (let i = 0; i < PAIRS; i++) {
        const A = await mk("Pa" + i), B = await mk("Pb" + i);
        pairs.push({ A, B, code: await h.rankedRoom(A, B, true) });
    }
    // each pair fights 3 battles (under the daily limit, so a duplicate that scored would show); ALL reports of ALL pairs are in flight together, every report sent 3 times, half of the pairs also resend each fight under new keys
    const jobs = [];
    for (const [i, p] of pairs.entries()) {
        for (let n = 1; n <= FIGHTS; n++) {
            const winner = (i + n) % 2; // sides alternate so ratings move both ways
            const ka = h.rk(), kb = h.rk();
            const repA = () => h.rep(p.A, { rkey: ka, code: p.code, n, me: p.A.name, opp: p.B.name, side: 0, winner, ranked: true });
            const repB = () => h.rep(p.B, { rkey: kb, code: p.code, n, me: p.B.name, opp: p.A.name, side: 1, winner, ranked: true });
            jobs.push(repA(), repA(), repA(), repB(), repB(), repB());
        }
    }
    await Promise.all(jobs);
    // a second wave: the same fights again under NEW report keys (a restarted app, a replay)
    tick(); tick();
    const wave2 = [];
    for (const [i, p] of pairs.entries()) {
        if (i % 2) continue;
        for (let n = 1; n <= FIGHTS; n++) {
            const winner = (i + n) % 2;
            wave2.push(h.rep(p.A, { rkey: h.rk(), code: p.code, n, me: p.A.name, opp: p.B.name, side: 0, winner, ranked: true }), h.rep(p.B, { rkey: h.rk(), code: p.code, n, me: p.B.name, opp: p.A.name, side: 1, winner, ranked: true }));
        }
    }
    // (the 10 s gap between an account's NEW reports would serialise this wave: lifted for the test, as if each report came from another device of the account)
    await h.env.DB.prepare("UPDATE players SET last_report = 0").run();
    const wave3 = [];
    for (const [i, p] of pairs.entries()) {
        for (let n = 1; n <= FIGHTS; n++) {
            const winner = (i + n) % 2;
            for (let k = 0; k < 2; k++) wave3.push(h.rep(p.A, { rkey: h.rk(), code: p.code, n, me: p.A.name, opp: p.B.name, side: 0, winner, ranked: true }), h.rep(p.B, { rkey: h.rk(), code: p.code, n, me: p.B.name, opp: p.A.name, side: 1, winner, ranked: true }));
        }
    }
    await Promise.all([...wave2, ...wave3]);
    // the per-account 10 s gap makes some NEW keys "slow": send what is left once more after the gap, one wave at a time, until nothing changes
    for (let round = 0; round < 8; round++) {
        tick();
        for (const [i, p] of pairs.entries()) for (let n = 1; n <= FIGHTS; n++) {
            const winner = (i + n) % 2;
            await h.rep(p.A, { rkey: "a" + i.toString(16) + n.toString(16) + "00000000000000", code: p.code, n, me: p.A.name, opp: p.B.name, side: 0, winner, ranked: true });
            await h.rep(p.B, { rkey: "b" + i.toString(16) + n.toString(16) + "00000000000000", code: p.code, n, me: p.B.name, opp: p.A.name, side: 1, winner, ranked: true });
        }
        tick(11000);
    }
    const codes = pairs.map((p) => p.code), ph = codes.map(() => "?").join(",");
    const ratedPerFight = await rows(`SELECT room_code, battle_no, COUNT(*) AS n FROM ranked_matches WHERE rated = 1 AND room_code IN (${ph}) GROUP BY room_code, battle_no HAVING n > 1`, ...codes);
    ok(ratedPerFight.length === 0, "no fight was scored twice (room, battle) with reports repeated and in flight together", JSON.stringify(ratedPerFight.slice(0, 3)));
    const perPair = await rows(`SELECT COUNT(*) AS n FROM ranked_matches WHERE rated = 1 AND room_code IN (${ph}) GROUP BY MIN(winner_id, loser_id), MAX(winner_id, loser_id)`, ...codes);
    ok(perPair.every((r) => r.n <= 4), "no pair has more than 4 rated battles in the day", JSON.stringify(perPair.filter((r) => r.n > 4)));
    // ratings are exactly what the settlements say: 1000 + the sum of each account's deltas, wins and losses the counts of its rated rows
    let consistent = true, detail = "";
    for (const p of pairs) for (const who of [p.A, p.B]) {
        const m = await rows("SELECT winner_id, loser_id, winner_before, winner_after, loser_before, loser_after FROM ranked_matches WHERE rated = 1 AND (winner_id = ? OR loser_id = ?) ORDER BY id", who.id, who.id);
        let rating = 1000, w = 0, l = 0;
        for (const x of m) { if (x.winner_id === who.id) { rating += x.winner_after - x.winner_before; w++; } else { rating += x.loser_after - x.loser_before; l++; } }
        const s = await me(who);
        if (s.rating !== rating || s.rwins !== w || s.rlosses !== l) { consistent = false; detail = `${who.name}: rating ${s.rating} vs ${rating}, wins ${s.rwins} vs ${w}, losses ${s.rlosses} vs ${l}`; }
    }
    ok(consistent, "every account's rating, wins and losses equal what its rated settlements say", detail);
    const open = await rows(`SELECT COUNT(*) AS n FROM reports WHERE outcome IS NULL AND code IN (${ph})`, ...codes);
    ok(open[0].n === 0, "no report is left open when both sides reported (nothing stuck)", JSON.stringify(open));
    const totals = await rows(`SELECT COUNT(*) AS n FROM ranked_matches WHERE rated = 1 AND room_code IN (${ph})`, ...codes);
    ok(totals[0].n === PAIRS * FIGHTS, `each of the ${PAIRS} pairs scored exactly its ${FIGHTS} battles, once`, String(totals[0].n));
}
console.log(fails ? `${fails} FAILED` : "no failures");
process.exit(fails ? 1 : 0);
