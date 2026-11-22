// Ranked integrity probes against the REAL worker (in-process, D1 shim, fake clock). Each case states the behaviour a competitive
// service must have; a FAIL is a reproduced defect. Run: node --no-warnings ranked_integrity.test.mjs
import { boot } from "./harness.mjs";

let fails = 0, known = 0;
const ok = (c, what, extra = "", knownDefect = false) => {
    console.log((c ? "ok   " : knownDefect ? "KNOWN-DEFECT " : "FAIL ") + what + (c ? "" : "  " + extra));
    if (!c) knownDefect ? known++ : fails++;
};
const KNOWN = process.env.EXPECT_FIXED === "0"; // EXPECT_FIXED=0 runs the cases as they were before the fix (they are then reported, not failed)

const h = boot();
const mk = async (n) => h.register(n + Math.floor(Math.random() * 1e6).toString(36), "198.51.100." + (1 + Math.floor(Math.random() * 200)));
const state = async (p) => h.post("me", h.auth(p));

// ---- baseline: a legitimate ranked battle counts once, and a repeat does not count twice
{
    const A = await mk("Alice"), B = await mk("Bruno");
    const code = await h.rankedRoom(A, B);
    const k = h.rk();
    await h.rep(A, { rkey: k, code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    h.clock.advance(11000);
    const r = await h.rep(B, { rkey: h.rk(), code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    ok(r.state === "settled" && r.delta === -16, "legit battle settles (+16 / -16)", JSON.stringify(r));
    const again = await h.rep(A, { rkey: k, code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    const a = await state(A);
    ok(again.state === "settled" && a.rwins === 1 && a.rating === 1016, "the same report sent again counts nothing twice", JSON.stringify(a));
}

// ---- F1: credit theft. The loser reports first; a THIRD account that was never in the room reports as the winner.
{
    h.clock.advance(3600000);
    const W = await mk("Winner"), L = await mk("Loser"), E = await mk("Eve");
    const code = await h.rankedRoom(W, L);
    await h.rep(L, { rkey: h.rk(), code, n: 1, me: L.name, opp: W.name, side: 1, winner: 0, ranked: true }); // the honest loser tells first
    h.clock.advance(11000);
    const stolen = await h.rep(E, { rkey: h.rk(), code: "", n: 1, me: W.name, opp: L.name, side: 0, winner: 0, ranked: true }); // Eve claims to be the winner
    const e = await state(E), w = await state(W);
    ok(!(stolen.state === "settled" && e.rwins === 1), "a stranger cannot take the winner's medal and points by claiming the winner's name",
       `Eve got: state=${stolen.state} delta=${stolen.delta} her rating=${e.rating} wins=${e.rwins}; the real winner has wins=${w.rwins}`, KNOWN);
}

// ---- F2: cancelling someone's result. The winner has not reported yet; Eve pre-places the contradicting mirrored report.
{
    h.clock.advance(3600000);
    const W = await mk("Champ"), L = await mk("Runner"), E = await mk("Mallory");
    const code = await h.rankedRoom(W, L);
    h.clock.advance(11000);
    // Eve guesses the sides and the battle number (1) and claims the LOSER won: it contradicts whatever the winner will say
    await h.rep(E, { rkey: h.rk(), code: "", n: 1, me: L.name, opp: W.name, side: 1, winner: 1, ranked: true });
    h.clock.advance(11000);
    const rw = await h.rep(W, { rkey: h.rk(), code, n: 1, me: W.name, opp: L.name, side: 0, winner: 0, ranked: true });
    h.clock.advance(11000);
    const rl = await h.rep(L, { rkey: h.rk(), code, n: 1, me: L.name, opp: W.name, side: 1, winner: 0, ranked: true });
    const w = await state(W);
    ok(w.rwins === 1 && rl.state === "settled", "a third account cannot make the real players' agreeing reports count for nothing",
       `winner's report -> ${rw.state}; loser's report -> ${rl.state}; winner's wins=${w.rwins}`, KNOWN);
}

// ---- honest negatives that must keep holding
{
    h.clock.advance(3600000);
    const A = await mk("Pat"), B = await mk("Quin");
    const code = await h.rankedRoom(A, B);
    await h.rep(A, { rkey: h.rk(), code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    h.clock.advance(11000);
    const c = await h.rep(B, { rkey: h.rk(), code, n: 1, me: B.name, opp: A.name, side: 1, winner: 1, ranked: true });
    ok(c.state === "conflict", "disagreeing honest reports count for nobody", JSON.stringify(c));
    h.clock.advance(11000);
    const self = await h.rep(A, { rkey: h.rk(), code, n: 2, me: A.name, opp: A.name, side: 0, winner: 0, ranked: true });
    ok(self.error === "invalid", "reporting against oneself is refused", JSON.stringify(self));
    const room = await h.rep(A, { rkey: h.rk(), code: "NOPE22", n: 3, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    h.clock.advance(11000);
    const casual = await h.rep(B, { rkey: h.rk(), code: "NOPE22", n: 3, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    ok(casual.state === "casual" && casual.medal === "none", "a room the ranked queue did not make gives no points", JSON.stringify([room, casual]));
}
// ---- the codeless wildcard: off by default; RANKED_ALLOW_CODELESS=1 brings back the old behaviour for games that still need it
{
    h.clock.advance(3600000);
    const A = await mk("Rita"), B = await mk("Saul");
    const code = await h.rankedRoom(A, B);
    await h.rep(A, { rkey: h.rk(), code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    h.clock.advance(11000);
    const strict = await h.rep(B, { rkey: h.rk(), code: "", n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    ok(strict.state === "waiting", "default: a ranked report with no room code is not paired with any room's", JSON.stringify(strict));
    h.env.RANKED_ALLOW_CODELESS = "1";
    h.clock.advance(11000);
    const legacy = await h.rep(B, { rkey: h.rk(), code: "", n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    delete h.env.RANKED_ALLOW_CODELESS;
    ok(legacy.state === "settled" && legacy.delta === -16, "RANKED_ALLOW_CODELESS=1: the pre-1.0.3 joiner (no code) is paired again", JSON.stringify(legacy));
}
// ---- a casual report never pairs with a ranked one (it could not score, but it could contradict)
{
    h.clock.advance(3600000);
    const A = await mk("Tess"), B = await mk("Ugo");
    const code = await h.rankedRoom(A, B);
    await h.rep(A, { rkey: h.rk(), code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    h.clock.advance(11000);
    const casualFake = await h.rep(B, { rkey: h.rk(), code, n: 1, me: B.name, opp: A.name, side: 1, winner: 1, ranked: false });
    h.clock.advance(11000);
    const real = await h.rep(B, { rkey: h.rk(), code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    ok(casualFake.state !== "conflict" && real.state === "settled", "a non-ranked report cannot cancel a ranked one", JSON.stringify([casualFake, real]));
}
// ---- migration support (no effect unless configured): the telemetry of codeless ranked reports and the ranked-queue release gate
{
    h.clock.advance(3600000);
    const meta = async (k) => (await h.env.DB.prepare("SELECT v FROM meta WHERE k = ?").bind(k).first())?.v ?? 0;
    const before = await meta("codeless_ranked_reports");
    const A = await mk("Vera"), B = await mk("Walt");
    const code = await h.rankedRoom(A, B);
    await h.rep(A, { rkey: h.rk(), code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    h.clock.advance(11000);
    await h.rep(B, { rkey: h.rk(), code: "", n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    ok((await meta("codeless_ranked_reports")) === before + 1 && (await meta("codeless_ranked_last")) > 0, "a ranked report without a room code is counted (telemetry for the migration)");
    const again = (k) => h.post("quick", { v: 140001, rel: k, name: "Xena" + Math.floor(Math.random() * 1e6), addrs: ["10.9.9.9:7000"], queue: "D20", who: "xxxxxxxx" + Math.floor(Math.random() * 1e6), waited: 0, area: "any" });
    const off = await again(139);
    ok(off.error !== "old", "RANKED_MIN_RELEASE unset: no release gate on the ranked queue (the default does not change)", JSON.stringify(off));
    h.env.RANKED_MIN_RELEASE = "140";
    const refused = await again(139), allowed = await again(140);
    const casual = await h.post("quick", { v: 140001, rel: 139, name: "Yara" + Math.floor(Math.random() * 1e6), addrs: ["10.9.9.8:7000"], queue: "S", who: "yyyyyyyy" + Math.floor(Math.random() * 1e6), waited: 0, area: "any" });
    delete h.env.RANKED_MIN_RELEASE;
    ok(refused.error === "old" && refused.need === 140, "RANKED_MIN_RELEASE=140: a release-139 game is told to update at the ranked search", JSON.stringify(refused));
    ok(allowed.error !== "old", "RANKED_MIN_RELEASE=140: a release-140 game searches normally", JSON.stringify(allowed));
    ok(casual.error !== "old", "the gate is for the ranked queue only (a casual search is untouched)", JSON.stringify(casual));
}
console.log(fails ? `${fails} FAILED` : "no unexpected failures", known ? `(${known} reproduced known defect(s))` : "");
process.exit(fails ? 1 : 0);
