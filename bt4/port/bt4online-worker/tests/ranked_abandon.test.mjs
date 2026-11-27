// Abandoned Ranked battles through the REAL worker (D1 shim, fake clock): who is blamed, what is lost, and that an
// honest or older player is not. Simulated HTTP side only.   node --no-warnings ranked_abandon.test.mjs
import { boot } from "./harness.mjs";

let fails = 0;
const ok = (c, what, extra = "") => { console.log((c ? "ok   " : "FAIL ") + what + (c ? "" : "  " + extra)); if (!c) fails++; };
const h = boot();
h.env.ABANDON_PENALTY = "1";
let u = 0;
const mk = async (n) => h.register(n + (++u) + Math.floor(Math.random() * 1e5).toString(36), "198.51.100." + (1 + (u % 200)));
const me = (p) => h.post("me", h.auth(p));
const match = async () => {
    h.clock.advance(3600000);
    const A = await mk("Aa"), B = await mk("Bb");
    const code = await h.rankedRoom(A, B, true);
    const beatA = () => h.post("beat", { ...h.auth(A), code, n: 1, me: A.name, opp: B.name, side: 0 });
    const beatB = () => h.post("beat", { ...h.auth(B), code, n: 1, me: B.name, opp: A.name, side: 1 });
    const goneA = (self = false) => h.post("gone", { ...h.auth(A), code, n: 1, self });
    const goneB = (self = false) => h.post("gone", { ...h.auth(B), code, n: 1, self });
    const play = async (times = 4) => { for (let i = 0; i < times; i++) { h.clock.advance(3000); await beatA(); await beatB(); } };
    await beatA(); await beatB();
    // as the game asks: every 5 s until answered
    const waitA = async () => { let r; for (let i = 0; i < 31; i++) { h.clock.advance(20000); r = await goneA(); } return r; };
    return { A, B, code, beatA, beatB, goneA, goneB, play, waitA };
};
// one gold medal first, so there is one to lose
const win = async (m, winnerSide) => {
    await h.rep(m.A, { rkey: h.rk(), code: m.code, n: 2, me: m.A.name, opp: m.B.name, side: 0, winner: winnerSide, ranked: true });
    h.clock.advance(11000);
    await h.rep(m.B, { rkey: h.rk(), code: m.code, n: 2, me: m.B.name, opp: m.A.name, side: 1, winner: winnerSide, ranked: true });
    h.clock.advance(11000);
};

{ // the player leaves through its own menus
    const m = await match();
    await win(m, 1);
    const before = await me(m.B);
    await m.play();
    const r = await m.goneB(true);
    const a = await me(m.A), b = await me(m.B);
    ok(r.state === "abandon" && r.medal === "lost" && r.left, "leaving by choice is decided at once", JSON.stringify(r));
    ok(before.gold === 1 && b.gold === 0 && b.losses === 1 && a.gold === 1 && a.wins === 1, "the one who left loses the battle and one gold medal; the other wins", JSON.stringify([before.gold, b, a]));
    const again = await m.goneB(true);
    const asked = await m.goneA();
    ok(again.medal === "lost" && asked.medal === "gold" && (await me(m.B)).losses === 1, "asking again changes nothing; the other hears it won", JSON.stringify([again, asked]));
    await h.rep(m.A, { rkey: h.rk(), code: m.code, n: 1, me: m.A.name, opp: m.B.name, side: 0, winner: 1, ranked: true });
    h.clock.advance(11000);
    const late = await h.rep(m.B, { rkey: h.rk(), code: m.code, n: 1, me: m.B.name, opp: m.A.name, side: 1, winner: 1, ranked: true });
    ok(!late.delta && (await me(m.A)).wins === 1, "late reports cannot turn an abandoned battle over", JSON.stringify(late));
}
{ // the app closed by its player (said when it is opened again, here two days later)
    const m = await match();
    await m.play();
    for (let i = 0; i < 3; i++) { h.clock.advance(5000); await m.beatA(); }
    await m.goneA();
    const w = await m.waitA();
    h.clock.advance(2 * 86400000);
    const still = await m.goneA();
    ok(w.state === "waiting" && still.state === "waiting" && (await me(m.B)).losses === 0, "silence alone is never judged", JSON.stringify([w, still]));
    const r = await h.post("exit", { ...h.auth(m.B), code: m.code, n: 1, why: "closed" });
    const a = await me(m.A), b = await me(m.B);
    ok(r.state === "abandon" && r.left && b.losses === 1 && a.wins === 1, "closed by the player: the loss, when the game says so", JSON.stringify([r, a.wins, b.losses]));
}
{ // an account back online that never says why
    const m = await match();
    await m.play();
    for (let i = 0; i < 3; i++) { h.clock.advance(5000); await m.beatA(); }
    await m.goneA();
    h.clock.advance(3600000);
    await me(m.B);
    const soon = await m.goneA();
    h.clock.advance(660000);
    const r = await m.goneA();
    ok(soon.state === "waiting" && r.state === "abandon" && (await me(m.B)).losses === 1, "back online and never explaining: judged after 10 minutes", JSON.stringify([soon, r]));
}
{ // switched off to escape and on again before the grace ends
    const m = await match();
    await m.play();
    for (let i = 0; i < 5; i++) { h.clock.advance(4000); await m.beatA(); } // B off for 20 s
    await m.goneA();
    h.clock.advance(5000);
    const rb = await m.goneB(); // B back, says the line broke
    ok(rb.state === "abandon" && rb.medal === "lost" && (await me(m.A)).wins === 1, "coming back and saying 'the line broke' does not save the one who was off", JSON.stringify(rb));
}
{ // the shortest switch-off that ends a match (8 s), then on again
    const m = await match();
    await m.play();
    for (let i = 0; i < 3; i++) { h.clock.advance(3000); await m.beatA(); }
    await m.goneA();
    h.clock.advance(500);
    const rb = await m.goneB();
    ok(rb.state === "abandon" && rb.left === true, "even a short switch-off (about 9 s) is seen", JSON.stringify(rb));
}
{ // the line between the two fails, both still reach the service
    const m = await match();
    await m.play();
    await m.play(2);          // each keeps saying it plays while it waits 6 s for the other
    h.clock.advance(1500);
    await m.goneA();
    h.clock.advance(1000);
    const r = await m.goneB();
    const a = await me(m.A), b = await me(m.B);
    ok(r.state === "link" && a.wins === 0 && b.losses === 0, "both online: nobody is punished", JSON.stringify(r));
}
{ // a slow moment early in the battle does not count against a player when, later, the line fails
    const m = await match();
    h.clock.advance(14000); await m.beatA(); await m.beatB(); // B... both slow once
    for (let i = 0; i < 2; i++) { h.clock.advance(3000); await m.beatA(); }
    h.clock.advance(6000); await m.beatA(); await m.beatB();  // B was silent for 12 s here
    await m.play(12);                                           // then 36 s of normal play
    await m.play(2); h.clock.advance(1500);
    await m.goneA(); h.clock.advance(1000);
    const r = await m.goneB();
    ok(r.state === "link" && (await me(m.B)).losses === 0, "an old hiccup is forgotten: the later line failure punishes nobody", JSON.stringify(r));
}
{ // both lose the network
    const m = await match();
    await m.play();
    h.clock.advance(60000);
    await m.goneA(); const r = await m.goneB();
    ok(r.state === "link" && (await me(m.A)).losses === 0 && (await me(m.B)).losses === 0, "both off: not known, nobody is punished", JSON.stringify(r));
}
{ // a false claim while the other is playing; strangers; an older game that never says it plays
    const m = await match();
    await m.play();
    const lie = await m.goneA();
    for (let i = 0; i < 12; i++) { h.clock.advance(4000); await m.beatA(); await m.beatB(); }
    const still = await m.goneA();
    ok(lie.state === "waiting" && still.state === "waiting" && (await me(m.B)).losses === 0, "claiming the other left while it is still playing does nothing", JSON.stringify([lie, still]));
    const S = await mk("St");
    const sb = await h.post("beat", { ...h.auth(S), code: m.code, n: 1, me: S.name, opp: m.B.name, side: 0 });
    const sg = await h.post("gone", { ...h.auth(S), code: m.code, n: 1 });
    ok(sb.error === "not admitted" && sg.error === "not playing", "a stranger with the room code takes no part", JSON.stringify([sb, sg]));
    h.clock.advance(3600000);
    const A = await mk("Oa"), B = await mk("Ob");
    const code = await h.rankedRoom(A, B, true);
    await h.post("beat", { ...h.auth(A), code, n: 1, me: A.name, opp: B.name, side: 0 });
    await h.post("gone", { ...h.auth(A), code, n: 1 });
    h.clock.advance(41000);
    const old = await h.post("gone", { ...h.auth(A), code, n: 1 });
    ok(old.state === "link" && (await me(B)).losses === 0, "an older game that never says it is playing is not blamed", JSON.stringify(old));
}
{ // crash, freeze, memory: nobody loses anything, however late the game is opened again
    const m = await match();
    await win(m, 1);
    await m.play();
    for (let i = 0; i < 3; i++) { h.clock.advance(3000); await m.beatA(); }
    await m.goneA();
    h.clock.advance(5 * 86400000);
    const r = await h.post("exit", { ...h.auth(m.B), code: m.code, n: 1, why: "fault" });
    await me(m.B);
    h.clock.advance(3600000);
    const after = await m.goneA();
    const a = await me(m.A), b = await me(m.B);
    ok(r.state === "crash" && after.state === "crash" && b.losses === 0 && a.wins === 0,
       "a fault costs nobody anything, five days later", JSON.stringify([r, after, a.wins, b.losses]));
}
{ // forgiveness, when the service is set to give it
    h.env.ABANDON_FREE_PER_DAY = "1";
    const m = await match();
    await m.play();
    for (let i = 0; i < 12; i++) { h.clock.advance(4000); await m.beatA(); }
    await m.goneA();
    h.clock.advance(60000); await me(m.B); h.clock.advance(660000);
    const r = await m.goneA();
    const row = (await h.env.DB.prepare("SELECT forgiven FROM abandons WHERE code = ?").bind(m.code).all()).results;
    ok((await me(m.B)).losses === 0 && row.length === 1 && row[0].forgiven === 1 && r.state === "link", "ABANDON_FREE_PER_DAY=1: the first silent exit of the day is forgiven", JSON.stringify([r, row]));
    h.env.ABANDON_FREE_PER_DAY = "0";
}
{ // off by default
    delete h.env.ABANDON_PENALTY;
    const r = await h.post("beat", { id: "x", secret: "y" });
    ok(r.state === "off", "without ABANDON_PENALTY=1 nothing is kept or punished", JSON.stringify(r));
}
console.log(fails ? "\nFAILED " + fails : "\nno failures");
process.exit(fails ? 1 : 0);
