// node test.mjs http://127.0.0.1:8787   (wrangler dev --local): the Ranked rooms, profiles, medals, fair points.
const base = process.argv[2] || "http://127.0.0.1:8787";
const post = async (what, body) => (await fetch(`${base}/v1/${what}`, { method: "POST", body: JSON.stringify(body) })).json();
const rk = () => [...crypto.getRandomValues(new Uint8Array(8))].map((b) => b.toString(16).padStart(2, "0")).join("");
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let fails = 0;
const ok = (c, what, extra = "") => { console.log((c ? "ok   " : "FAIL ") + what + (c ? "" : " " + extra)); if (!c) fails++; };
const tag = Date.now().toString(36).slice(-4);
const auth = (p) => ({ id: p.id, secret: p.secret });
const rep = (p, o) => post("report", { ...auth(p), ...o });

// accounts
const A = await post("register", { name: "Goku" + tag }), B = await post("register", { name: "Vegeta" + tag });
const C = await post("register", { name: "Piccolo" + tag });
ok(A.ok && B.ok && C.ok, "three accounts");
ok((await post("register", { name: ("goku" + tag).toUpperCase() })).error === "taken", "names are unique, any case");
ok((await post("register", { name: "x" })).error === "invalid", "a one-letter name is refused");

// the Ranked queue of the rooms: A waits, B is put with A
const v = 140001;
ok((await post("quick", { v: 5001, name: "x", addrs: ["1.2.3.4:5000"], queue: "D20", who: "zzzzzzzzzz" })).error === "version", "other games' versions are refused");
const qa = await post("quick", { v, rel: 140, name: A.name, addrs: ["10.0.0.1:7000"], queue: "D20", who: "aaaaaaaa" + tag, waited: 0, area: "any" });
ok(qa.role === "host" && qa.code && qa.key, "the first Ranked searcher waits in a room", JSON.stringify(qa));
const qb = await post("quick", { v, rel: 140, name: B.name, addrs: ["10.0.0.2:7000"], queue: "D20", who: "bbbbbbbb" + tag, waited: 0, area: "any" });
ok(qb.role === "join" && qb.code === qa.code && qb.peer && qb.peer.name === A.name, "the second is put with the first", JSON.stringify(qb));
const polled = await post("poll", { code: qa.code, key: qa.key });
ok(polled.peer && polled.peer.name === B.name, "the host's poll finds the other player", JSON.stringify(polled));
const code = qa.code;

// a Ranked battle of that match: points
await rep(A, { rkey: rk(), code, n: 1, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
const r1 = await rep(B, { rkey: rk(), code, n: 1, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
ok(r1.state === "settled" && r1.medal === "silver" && r1.delta === -16 && r1.rating === 984, "Ranked battle of a served match: points", JSON.stringify(r1));

// a "Ranked" battle in a room the Ranked queue did not make: medals, no points
await sleep(10500);
await rep(A, { rkey: rk(), code: "FAKE99", n: 1, me: A.name, opp: C.name, side: 0, winner: 0, ranked: true });
const r2 = await rep(C, { rkey: rk(), code: "FAKE99", n: 1, me: C.name, opp: A.name, side: 1, winner: 0, ranked: true });
ok(r2.state === "settled" && r2.medal === "silver" && r2.delta === undefined && r2.ranked === false, "a room not made by the Ranked queue gives no points", JSON.stringify(r2));
let ma = await post("me", auth(A));
ok(ma.gold === 2 && ma.rating === 1016 && ma.rwins === 1, "A: two golds, one rated win", JSON.stringify(ma));

// the same pair: three rated Ranked battles a day, then medals only
for (let n = 2; n <= 4; n++) {
    await sleep(10500);
    await rep(A, { rkey: rk(), code, n, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
    const r = await rep(B, { rkey: rk(), code, n, me: B.name, opp: A.name, side: 1, winner: 0, ranked: true });
    if (n <= 3) ok(r.delta < 0 && !r.capped, `rated Ranked battle ${n} of the pair today`, JSON.stringify(r));
    else ok(r.delta === undefined && r.capped === true && r.medal === "silver", "the 4th Ranked battle of the same pair today: no points", JSON.stringify(r));
}
ma = await post("me", auth(A));
ok(ma.rwins === 3 && ma.gold === 5 && ma.opponents === 1 && ma.eligible === false, "A: 3 rated wins, 5 golds, 1 opponent, not yet eligible for trophies", JSON.stringify(ma));

// disagreement and wrong names
await sleep(10500);
await rep(A, { rkey: rk(), code, n: 9, me: A.name, opp: B.name, side: 0, winner: 0, ranked: true });
ok((await rep(B, { rkey: rk(), code, n: 9, me: B.name, opp: A.name, side: 1, winner: 1, ranked: true })).state === "conflict", "disagreeing reports count for nobody");
await sleep(10500);
await rep(A, { rkey: rk(), code, n: 10, me: A.name, opp: "Someone", side: 0, winner: 0, ranked: false });
ok((await rep(B, { rkey: rk(), code, n: 10, me: B.name, opp: A.name, side: 1, winner: 0, ranked: false })).state === "waiting", "names must match both ways");
ok((await rep(B, { rkey: rk(), code, n: 11, me: B.name, opp: A.name, side: 1, winner: 0 })).error === "slow", "one report per 10 s");
ok((await post("me", { id: A.id, secret: "0".repeat(64) })).error === "who", "a wrong secret is refused");

// who is online
const w1 = await post("presence", { who: "presence" + tag + "aa" }), w2 = await post("presence", { who: "presence" + tag + "bb" });
ok(w1.ok && w2.online >= 2, "players online are counted", JSON.stringify(w2));
ok((await post("presence", { who: "presence" + tag + "aa", active: false })).online === w2.online - 1, "a game that leaves is not counted");
ok((await post("presence", { who: "x" })).error === "invalid", "a bad word is refused");

// the table
const t = await post("top", {});
ok(t.ok && t.top[0][0] === A.name && t.top[0].length === 9 && t.top[0][4] === 5, "the week's table, with medals", JSON.stringify(t.top[0]));
console.log(fails ? `${fails} FAILED` : "all passed");
process.exit(fails ? 1 : 0);
