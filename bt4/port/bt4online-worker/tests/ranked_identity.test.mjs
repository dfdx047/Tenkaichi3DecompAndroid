// Identity and uniqueness of ranked results against the REAL worker (in-process, D1 shim, fake clock). A FAIL is a reproduced defect.
//   node --no-warnings ranked_identity.test.mjs        (EXPECT_FIXED=0: report the known defects instead of failing on them)
import { boot } from "./harness.mjs";

let fails = 0, known = 0;
const KNOWN = process.env.EXPECT_FIXED === "0";
const ok = (c, what, extra = "", knownDefect = false) => {
    console.log((c ? "ok   " : knownDefect ? "KNOWN-DEFECT " : "FAIL ") + what + (c ? "" : "  " + extra));
    if (!c) knownDefect ? known++ : fails++;
};
const h = boot();
let u = 0;
const mk = async (n) => h.register(n + (++u) + Math.floor(Math.random() * 1e5).toString(36), "198.51.100." + (1 + (u % 200)));
const me = (p) => h.post("me", h.auth(p));
const tick = (ms = 11000) => h.clock.advance(ms);
const battle = async (A, B, code, n, winnerSide = 0) => { // both report the same battle (A is side 0, B is side 1)
    await h.rep(A, { rkey: h.rk(), code, n, me: A.name, opp: B.name, side: 0, winner: winnerSide, ranked: true });
    tick();
    const r = await h.rep(B, { rkey: h.rk(), code, n, me: B.name, opp: A.name, side: 1, winner: winnerSide, ranked: true });
    tick();
    return r;
};

// ---- D1: the same battle (room, number) reported again with new report keys must not count twice
{
    const A = await mk("Dup"), B = await mk("Lic");
    const code = await h.rankedRoom(A, B);
    const first = await battle(A, B, code, 1);
    const again = await battle(A, B, code, 1); // new rkeys: a restarted client, a replay, a script
    const a = await me(A);
    ok(first.state === "settled" && a.rwins === 1 && a.rating === 1016 && !again.delta,
       "one fight (room, battle 1) is settled once even when both sides report it again with new keys",
       `second settlement: ${JSON.stringify(again)}; winner wins=${a.rwins} rating=${a.rating}`, KNOWN);
}

// ---- D2: the daily limit per pair holds under concurrent settlements (five different battles reported at the same instant)
{
    h.clock.advance(86400000 * 2);
    const A = await mk("Con"), B = await mk("Cur");
    const code = await h.rankedRoom(A, B);
    for (let n = 1; n <= 6; n++) await h.rep(A, { rkey: h.rk(), code, n, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true }), tick();
    // B's six reports arrive together: each would settle with A's report of the same number
    const results = await Promise.all([1, 2, 3, 4, 5, 6].map(async (n) => { return h.rep(B, { rkey: h.rk(), code, n, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true }); }));
    tick(); tick();
    const a = await me(A);
    ok(a.rwins <= 4, "the 4-a-day limit per pair is not exceeded when settlements race", `A rated wins=${a.rwins} of 6 concurrent battles; answers ${results.map((r) => r.state || r.error).join(",")}`);
}

// ---- I1: a spectator (or any account) that knows the room code cannot be a party of the battle
{
    h.clock.advance(3600000);
    const W = await mk("Win"), L = await mk("Los"), S = await mk("Spy");
    const code = await h.rankedRoom(W, L, true); // the two players searched with their accounts
    await h.rep(L, { rkey: h.rk(), code, n: 1, me: L.name, opp: W.name, side: 1, winner: 0, ranked: true });
    tick();
    const steal = await h.rep(S, { rkey: h.rk(), code, n: 1, me: W.name, opp: L.name, side: 0, winner: 0, ranked: true }); // Spy knows the code AND the names
    const s = await me(S);
    ok(steal.error === "not admitted" && s.rwins === 0, "an account that was not admitted to the room cannot report its battle, even knowing the code and the names", JSON.stringify([steal, s.rwins]), KNOWN);
    tick();
    const real = await h.rep(W, { rkey: h.rk(), code, n: 1, me: W.name, opp: L.name, side: 0, winner: 0, ranked: true });
    ok(real.state === "settled" && real.delta === 16, "and the real winner's report still settles with the loser's", JSON.stringify(real));
}
{ // I2: the spy cannot cancel it either (a contradicting report placed first)
    h.clock.advance(3600000);
    const W = await mk("Wi2"), L = await mk("Lo2"), S = await mk("Sp2");
    const code = await h.rankedRoom(W, L, true);
    tick();
    await h.rep(S, { rkey: h.rk(), code, n: 1, me: L.name, opp: W.name, side: 1, winner: 1, ranked: true });
    tick();
    const rw = await h.rep(W, { rkey: h.rk(), code, n: 1, me: W.name, opp: L.name, side: 0, winner: 0, ranked: true });
    tick();
    const rl = await h.rep(L, { rkey: h.rk(), code, n: 1, me: L.name, opp: W.name, side: 1, winner: 0, ranked: true });
    ok(rl.state === "settled" && rw.state !== "conflict", "an unadmitted account's contradicting report cannot cancel the real result", JSON.stringify([rw, rl]), KNOWN);
}
// ---- I3: the reporters must be the two admitted accounts, not merely accounts that use the right names
{
    h.clock.advance(3600000);
    const A = await mk("Ad3"), B = await mk("Bd3"), X = await mk("Xd3");
    const code = await h.rankedRoom(A, B, true);
    // X reports with B's name, A reports honestly: X is not a seat, so the pair (A, X) must not score
    await h.rep(A, { rkey: h.rk(), code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    tick();
    const r = await h.rep(X, { rkey: h.rk(), code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    const x = await me(X);
    ok(r.state !== "settled" && x.rlosses === 0, "an account using a seat's name, but not that seat's account, does not settle", JSON.stringify(r), KNOWN);
}
// ---- I4: nobody can play against their own other account/device through the ranked queue
{
    h.clock.advance(3600000);
    const A = await mk("Slf");
    const host = await h.post("quick", { v: 140001, rel: 140, name: A.name, addrs: ["10.7.7.1:7000"], queue: "D20", who: "selfhost" + u, waited: 0, area: "any", acct: { id: A.id, secret: A.secret } });
    const join = await h.post("quick", { v: 140001, rel: 140, name: A.name, addrs: ["10.7.7.2:7000"], queue: "D20", who: "selfjoin" + u, waited: 0, area: "any", acct: { id: A.id, secret: A.secret } }, "203.0.113.99");
    ok(host.role === "host" && join.role === "host" && join.code !== host.code, "one account's two searches are not put together (each waits in its own room)", JSON.stringify([host, join]), KNOWN);
}
// ---- I5: invalid credentials in the search are refused (never silently downgraded to anonymous); an old secret after recovery is dead
{
    h.clock.advance(3600000);
    const A = await mk("Cr5");
    const bad = await h.post("quick", { v: 140001, rel: 140, name: A.name, addrs: ["10.7.8.1:7000"], queue: "D20", who: "badcredx" + u, waited: 0, area: "any", acct: { id: A.id, secret: "0".repeat(64) } });
    ok(bad.error === "account", "a wrong secret in the ranked search is refused", JSON.stringify(bad), KNOWN);
    const back = await h.post("recover", { name: A.name, code: A.recovery });
    const stale = await h.post("quick", { v: 140001, rel: 140, name: A.name, addrs: ["10.7.8.2:7000"], queue: "D20", who: "stalexxx" + u, waited: 0, area: "any", acct: { id: A.id, secret: A.secret } });
    ok(back.ok && stale.error === "account", "the secret from before a recovery no longer binds a seat", JSON.stringify([back.ok, stale]), KNOWN);
}
// ---- I6: the host that searches again from its room cannot change the account that holds the seat
{
    h.clock.advance(3600000);
    const A = await mk("Hs6"), E = await mk("Ev6");
    const host = await h.post("quick", { v: 140001, rel: 140, name: A.name, addrs: ["10.7.9.1:7000"], queue: "D20", who: "host6host" + u, waited: 0, area: "any", acct: { id: A.id, secret: A.secret } });
    const swap = await h.post("quick", { v: 140001, rel: 140, name: A.name, addrs: ["10.7.9.1:7000"], queue: "D20", who: "host6host" + u, waited: 5, area: "any", code: host.code, key: host.key, acct: { id: E.id, secret: E.secret } });
    ok(swap.error === "identity", "the seat's account cannot be replaced by another account that holds the room's key", JSON.stringify(swap), KNOWN);
}
// ---- L1: legacy games (no account in the search) keep working as before; RANKED_REQUIRE_ACCOUNT=1 turns their battles into unrated ones
{
    h.clock.advance(3600000);
    const A = await mk("Lg1"), B = await mk("Lg2");
    const code = await h.rankedRoom(A, B, false);
    const r = await battle(A, B, code, 1);
    ok(r.state === "settled" && r.delta === -16, "legacy (no account in the search): the battle settles as before", JSON.stringify(r));
    h.clock.advance(3600000);
    const C = await mk("Lg3"), D = await mk("Lg4");
    const code2 = await h.rankedRoom(C, D, false);
    h.env.RANKED_REQUIRE_ACCOUNT = "1";
    const q = await battle(C, D, code2, 1);
    delete h.env.RANKED_REQUIRE_ACCOUNT;
    ok(q.state === "casual" && !q.delta, "RANKED_REQUIRE_ACCOUNT=1: a battle of a room whose seats are not bound to accounts scores nothing", JSON.stringify(q), KNOWN);
}
// ---- D3: the same fight reported under two sets of keys at the SAME instant: exactly one rated settlement
{
    h.clock.advance(86400000 * 2);
    const A = await mk("Rac"), B = await mk("Rcb");
    const code = await h.rankedRoom(A, B, true);
    tick();
    const mkr = () => [h.rk(), h.rk()];
    const [a1, a2] = mkr(), [b1, b2] = mkr();
    // (the 10 s per-account gap applies to a NEW report key; the first of each account passes, the second is "slow": so the second set goes in after the gap, in parallel with the first's pairing)
    await Promise.all([
        h.rep(A, { rkey: a1, code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true }),
        h.rep(B, { rkey: b1, code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true }),
    ]);
    tick();
    await Promise.all([
        h.rep(A, { rkey: a2, code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true }),
        h.rep(B, { rkey: b2, code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true }),
    ]);
    const n = (await h.env.DB.prepare("SELECT COUNT(*) AS n FROM ranked_matches WHERE room_code = ? AND battle_no = 1 AND rated = 1").bind(code).first()).n;
    const a = await me(A);
    ok(n === 1 && a.rwins === 1, "two sets of reports of one fight, racing: one rated settlement", `rated rows=${n}, A wins=${a.rwins}`);
}
// ---- S1: no secret reaches the logs or any answer (the account secret travels in the search and in every report)
{
    const logs = [];
    const real = console.log; console.log = (...a) => logs.push(a.join(" "));
    h.clock.advance(3600000);
    const A = await mk("Sec"), B = await mk("Sed"), E = await mk("See");
    const answers = [];
    const code = await h.rankedRoom(A, B, true);
    answers.push(await h.post("quick", { v: 140001, rel: 140, name: A.name, addrs: ["10.5.5.5:7000"], queue: "D20", who: "secretxx" + u, waited: 0, area: "any", acct: { id: A.id, secret: "f".repeat(64) } }));
    answers.push(await h.rep(E, { rkey: h.rk(), code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true }));
    answers.push(await h.rep(A, { rkey: "zz", code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true }));
    answers.push(await h.post("report", { id: A.id, secret: A.secret, rkey: h.rk(), ranked: true, n: "x" }));
    console.log = real;
    const all = JSON.stringify(answers) + logs.join("\n");
    const leaked = [A, B, E].some((p) => all.includes(p.secret)) || all.includes("f".repeat(64));
    ok(!leaked, "no account secret appears in a log line or in any answer, including refusals", all.slice(0, 200));
}
console.log(fails ? `${fails} FAILED` : "no unexpected failures", known ? `(${known} reproduced known defect(s))` : "");
process.exit(fails ? 1 : 0);
