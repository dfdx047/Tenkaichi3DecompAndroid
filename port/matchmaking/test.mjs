// The Worker run against a small stand-in for its database: the address rules and the limits.
// Run: node port/matchmaking/test.mjs   (any recent Node; nothing is sent anywhere)
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import nodeCrypto from "node:crypto";
const src = fs.readFileSync(new URL("./worker.js", import.meta.url), "utf8").replace("export default", "const worker =") + "\nexport { worker, network, isPrivate, sameNetwork, shown };";
const copy = path.join(fs.mkdtempSync(path.join(os.tmpdir(), "worker-test-")), "worker.mjs");
fs.writeFileSync(copy, src);
const { worker, network, sameNetwork } = await import(copy);
const rooms = new Map(), used = new Map();
let rowids = 0;
const DB = { prepare(sql) { return { args: [], bind(...a) { this.args = a; return this; },
    async all() { const a = this.args;
        if (sql.startsWith("SELECT queue, COUNT")) { const m = new Map(); for (const r of rooms.values()) { if (r.queue !== null && r.version === a[0] && r.join_key === null && r.made >= a[1]) m.set(r.queue, (m.get(r.queue) ?? 0) + 1); } return { results: [...m].map(([queue, n]) => ({ queue, n })) }; }
        throw new Error("all: " + sql); },
    async first() { const a = this.args;
        if (sql.startsWith("SELECT COUNT(*) AS n FROM rooms WHERE queue") && sql.includes("rowid <")) { const mine = rooms.get(a[3]); return { n: [...rooms.values()].filter((r) => r.queue === a[0] && r.version === a[1] && r.join_key === null && r.made >= a[2] && mine && r.rowid < mine.rowid).length }; }
        if (sql.startsWith("SELECT COUNT(*) AS n FROM rooms WHERE queue")) { return { n: [...rooms.values()].filter((r) => r.queue === a[0] && r.version === a[1] && r.join_key === null && r.made >= a[2]).length }; }
        if (sql.startsWith("SELECT * FROM rooms WHERE queue")) { const l = [...rooms.values()].filter((r) => r.queue === a[0] && r.version === a[1] && r.join_key === null && r.made >= a[2]).sort((x, y) => x.rowid - y.rowid); return l[0] ?? null; }
        if (sql.startsWith("SELECT COUNT")) { return { n: [...rooms.values()].filter((r) => r.ip === a[0]).length }; }
        if (sql.startsWith("SELECT * FROM rooms")) { const r = rooms.get(a[0]); return r && r.made >= a[1] ? r : null; }
        if (sql.startsWith("SELECT n FROM used")) { const n = used.get(a[0] + "|" + a[1]); return n ? { n } : null; }
        if (sql.startsWith("INSERT INTO used")) { const k = a[0] + "|" + a[1]; used.set(k, (used.get(k) ?? 0) + 1); return { n: used.get(k) }; }
        throw new Error("first: " + sql); },
    async run() { const a = this.args;
        if (sql.startsWith("DELETE FROM rooms WHERE made")) { for (const [k, r] of rooms) { if (r.made < a[0]) rooms.delete(k); } return {}; }
        if (sql.startsWith("DELETE FROM used")) { return {}; }
        if (sql.startsWith("INSERT INTO used")) { used.set(a[0] + "|" + a[1], a[2]); return {}; }
        if (sql.startsWith("INSERT INTO rooms")) { if (rooms.has(a[0])) throw new Error("taken"); rooms.set(a[0], { code: a[0], made: a[1], version: a[2], ip: a[3], host_key: a[4], join_key: null, host_name: a[5], join_name: null, host_addrs: a[6], join_addrs: null, queue: a[7] ?? null, who: a[8] ?? null, rowid: ++rowids }); return {}; }
        if (sql.startsWith("DELETE FROM rooms WHERE who")) { for (const [k, r] of rooms) { if (r.who === a[0] && r.queue !== null && r.join_key === null) rooms.delete(k); } return {}; }
        if (sql.startsWith("DELETE FROM rooms WHERE code = ? AND join_key IS NULL")) { const r = rooms.get(a[0]); if (r && r.join_key === null) { rooms.delete(a[0]); return { meta: { changes: 1 } }; } return { meta: { changes: 0 } }; }
        if (sql.startsWith("UPDATE rooms SET join_key")) { const r = rooms.get(a[3]); if (!r || r.join_key !== null) return { meta: { changes: 0 } }; r.join_key = a[0]; r.join_name = a[1]; r.join_addrs = a[2]; return { meta: { changes: 1 } }; }
        if (sql.startsWith("UPDATE rooms SET host_addrs")) { rooms.get(a[1]).host_addrs = a[0]; return {}; }
        if (sql.startsWith("DELETE FROM rooms WHERE code")) { rooms.delete(a[0]); return {}; }
        throw new Error("run: " + sql); } }; } };
const env = { DB }; // (no TURN key: no logins are asked of Cloudflare)
async function post(what, ip, body) {
    const r = await worker.fetch(new Request("https://x/v1/" + what, { method: "POST", headers: { "cf-connecting-ip": ip }, body: JSON.stringify(body) }), env);
    return r.json();
}
let fails = 0;
function check(name, ok) { console.log((ok ? "ok   " : "FAIL ") + name); if (!ok) fails++; }
check("IPv6 network part", network("2001:db8:1:2:aaaa::1") === network("2001:db8:1:2::ffff") && network("2001:db8:1:3::1") !== network("2001:db8:1:2::1"));
// strangers: the local address is not handed over, either way
let h = await post("host", "198.51.100.1", { v: 3, name: "H", addrs: ["198.51.100.1:7000", "192.168.1.10:7000"] });
let j = await post("join", "203.0.113.9", { v: 3, code: h.code, name: "J", addrs: ["203.0.113.9:8000", "10.0.0.5:8000"] });
check("stranger joins: host's local address hidden", JSON.stringify(j.peer.addrs) === JSON.stringify(["198.51.100.1:7000"]));
let p = await post("poll", "198.51.100.1", { code: h.code, key: h.key });
check("host is told the stranger's public address and seen address only", JSON.stringify(p.peer.addrs) === JSON.stringify(["203.0.113.9:8000", "seen=203.0.113.9"]));
p = await post("poll", "203.0.113.9", { code: h.code, key: j.key });
check("the stranger's later poll: still hidden", JSON.stringify(p.peer.addrs) === JSON.stringify(["198.51.100.1:7000"]));
// one household, same public address
h = await post("host", "198.51.100.1", { v: 3, name: "H", addrs: ["198.51.100.1:7000", "192.168.1.10:7000"] });
await post("addrs", "198.51.100.1", { code: h.code, key: h.key, addrs: ["198.51.100.1:7000", "192.168.1.10:7000", "relay=104.30.1.1:5000"] });
j = await post("join", "198.51.100.1", { v: 3, code: h.code, name: "J", addrs: ["198.51.100.1:8000", "192.168.1.11:8000"] });
check("same address: local and relay addresses handed over", JSON.stringify(j.peer.addrs) === JSON.stringify(["198.51.100.1:7000", "192.168.1.10:7000", "relay=104.30.1.1:5000"]));
p = await post("poll", "198.51.100.1", { code: h.code, key: h.key });
check("host is told the housemate's local address (and no marker)", JSON.stringify(p.peer.addrs) === JSON.stringify(["198.51.100.1:8000", "192.168.1.11:8000", "seen=198.51.100.1"]));
// one household, one seen by IPv6, one by IPv4, same public IPv4 found by STUN
h = await post("host", "2001:db8:1:2::10", { v: 3, name: "H", addrs: ["198.51.100.1:7000", "192.168.1.10:7000"] });
j = await post("join", "198.51.100.1", { v: 3, code: h.code, name: "J", addrs: ["198.51.100.1:8000", "192.168.1.11:8000"] });
check("IPv6 and IPv4 of one household: local address handed over", j.peer.addrs.includes("192.168.1.10:7000"));
check("sameNetwork: two IPv6 of one network", sameNetwork("2001:db8:1:2::10", [], "2001:db8:1:2:5::1", []));
// wrong codes
let last;
for (let i = 0; i < 31; i++) { last = await post("join", "192.0.2.77", { v: 3, code: "ZZZZZ" + "23456789ABCDEFGHJKMNPQRSTUVWXYZ"[i], name: "X", addrs: ["192.0.2.77:1"] }); }
check("the 31st wrong code in an hour is refused as busy", last.error === "busy");
h = await post("host", "198.51.100.2", { v: 3, name: "H", addrs: ["198.51.100.2:7000"] });
last = await post("join", "192.0.2.77", { v: 3, code: h.code, name: "X", addrs: ["192.0.2.77:1"] });
check("... and so is a right code from that address", last.error === "busy");
// rooms an hour (20), with the room given up each time
for (let i = 0; i < 21; i++) { last = await post("host", "192.0.2.50", { v: 3, name: "H", addrs: ["192.0.2.50:1"] }); if (last.code) await post("leave", "192.0.2.50", { code: last.code, key: last.key }); }
check("the 21st room in an hour from one address is refused", last.error === "busy");
check("no relay without a key says why", typeof h.noRelay === "string");
// the relay's use measured: Cloudflare's analytics and its TURN service stood in for
const cap = { DB, TURN_KEY_ID: "abc123", TURN_KEY_TOKEN: "t", CF_ACCOUNT_ID: "0123abcd", CF_ANALYTICS_TOKEN: "a", ADMIN_KEY: "0123456789abcdef0123" };
let monthBytes = 10e9, analyticsFail = false;
const revoked = [];
const realFetch = globalThis.fetch;
globalThis.fetch = async (u, init) => {
    u = String(u);
    if (u.includes("/graphql")) {
        if (analyticsFail) { return new Response(JSON.stringify({ data: null, errors: [{ message: "not allowed" }] }), { status: 200 }); }
        return new Response(JSON.stringify({ data: { viewer: { accounts: [{ month: [{ sum: { egressBytes: monthBytes / 2, ingressBytes: monthBytes / 2 }, dimensions: { keyId: "abc123" } }],
            logins: [{ sum: { egressBytes: 1.5e9, ingressBytes: 0.5e9 }, dimensions: { username: "heavy" } }, { sum: { egressBytes: 5e7, ingressBytes: 5e7 }, dimensions: { username: "light" } }] }] } } }));
    }
    if (u.endsWith("/revoke")) { revoked.push(u.split("/credentials/")[1].split("/")[0]); return new Response(null, { status: 204 }); }
    if (u.includes("generate-ice-servers")) { return new Response(JSON.stringify({ iceServers: [{ urls: ["turn:turn.example:3478?transport=udp"], username: "u", credential: "c" }] })); }
    return realFetch(u, init);
};
async function postCap(what, ip, body) {
    const r = await worker.fetch(new Request("https://x/v1/" + what, { method: "POST", headers: { "cf-connecting-ip": ip }, body: JSON.stringify(body) }), cap);
    return r.json();
}
h = await postCap("host", "192.0.2.90", { v: 3, name: "H", addrs: ["192.0.2.90:1"] });
check("measuring set up but never done: no relay login", typeof h.noRelay === "string" && h.noRelay.includes("measured"));
check("status needs the admin key", (await postCap("status", "192.0.2.90", { admin: "wrong" })).error === "bad request");
analyticsFail = true;
let st = await postCap("status", "192.0.2.90", { admin: cap.ADMIN_KEY });
check("analytics refusing: reported, nothing measured", st.measured === false && st.why.includes("not allowed"));
analyticsFail = false;
st = await postCap("status", "192.0.2.90", { admin: cap.ADMIN_KEY });
check("under the limit: relay on, 10 GB counted", st.measured === true && st.relay === "on" && st.monthGB === 10);
check("the login that passed 2 GB is ended, the light one is not", JSON.stringify(revoked) === JSON.stringify(["heavy"]));
h = await postCap("host", "192.0.2.91", { v: 3, name: "H", addrs: ["192.0.2.91:1"] });
check("after a measurement: a relay login is given", h.turn && h.turn.username === "u");
st = await postCap("status", "192.0.2.90", { admin: cap.ADMIN_KEY });
check("a login is ended once only", revoked.length === 1);
monthBytes = 600e9;
st = await postCap("status", "192.0.2.90", { admin: cap.ADMIN_KEY });
check("past the month's limit: relay off, every login in use ended", st.relay === "off until next month" && revoked.includes("light"));
h = await postCap("host", "192.0.2.92", { v: 3, name: "H", addrs: ["192.0.2.92:1"] });
check("... and no more logins, rooms still made", typeof h.code === "string" && h.noRelay === "the month's relay allowance is used up");
await worker.scheduled({}, cap, { waitUntil: (p) => p });
check("the Cron Trigger's entry runs", true);
// quick match
rooms.clear();
let q1 = await post("quick", "198.51.100.20", { v: 3, name: "A", addrs: ["198.51.100.20:7000"], queue: "D15", who: "aaaaaaaaaaaa" });
check("quick match, nobody waiting: a room of one's own", q1.role === "host" && typeof q1.code === "string" && typeof q1.key === "string");
let qs = await post("queues", "198.51.100.99", { v: 3 });
check("the queue's count shows the one waiting", qs.waiting.D15 === 1 && qs.waiting.S === 0);
let q2 = await post("quick", "203.0.113.30", { v: 3, name: "B", addrs: ["203.0.113.30:8000", "10.0.0.9:8000"], queue: "S", who: "bbbbbbbbbbbb" });
check("another queue is another queue", q2.role === "host");
let q3 = await post("quick", "203.0.113.31", { v: 3, name: "C", addrs: ["203.0.113.31:8000", "10.0.0.7:8000"], queue: "D15", who: "cccccccccccc" });
check("the same queue: put with the one who waits", q3.role === "join" && q3.code === q1.code && q3.peer.name === "A" && JSON.stringify(q3.peer.addrs) === JSON.stringify(["198.51.100.20:7000"]));
p = await post("poll", "198.51.100.20", { code: q1.code, key: q1.key });
check("... who is told of it by its poll, without the stranger's local address", p.peer && p.peer.name === "C" && JSON.stringify(p.peer.addrs) === JSON.stringify(["203.0.113.31:8000", "seen=203.0.113.31"]));
let q4 = await post("quick", "203.0.113.32", { v: 3, name: "D", addrs: ["203.0.113.32:8000"], queue: "D15", who: "dddddddddddd" });
check("a room that has its two is not given again", q4.role === "host" && q4.code !== q1.code);
let q5 = await post("quick", "203.0.113.32", { v: 3, name: "D", addrs: ["203.0.113.32:8000"], queue: "D15", who: "dddddddddddd" });
check("searching again: not put with one's own room left behind", q5.role === "host" && !rooms.has(q4.code));
let q6 = await post("quick", "203.0.113.33", { v: 4, name: "E", addrs: ["203.0.113.33:8000"], queue: "D15", who: "eeeeeeeeeeee" });
check("another netcode version is not matched", q6.role === "host");
check("an unknown queue is refused", (await post("quick", "203.0.113.34", { v: 3, name: "F", addrs: ["203.0.113.34:1"], queue: "X", who: "ffffffffffff" })).error === "bad request");
check("no name for the game: refused", (await post("quick", "203.0.113.34", { v: 3, name: "F", addrs: ["203.0.113.34:1"], queue: "S" })).error === "bad request");
// two who search in the same moment: both made a room before either looked again
rooms.clear();
{
    const first = await post("quick", "198.51.100.40", { v: 3, name: "G", addrs: ["198.51.100.40:1"], queue: "T", who: "gggggggggggg" });
    // the second one's first look finds nobody (as it would have, a moment earlier): stood in for by hiding the room
    const kept = rooms.get(first.code); rooms.delete(first.code);
    const realPrepare = DB.prepare;
    let put = false;
    DB.prepare = (sql) => { const st = realPrepare(sql); if (!put && sql.startsWith("INSERT INTO rooms")) { put = true; rooms.set(first.code, kept); } return st; };
    const second = await post("quick", "203.0.113.41", { v: 3, name: "H", addrs: ["203.0.113.41:1"], queue: "T", who: "hhhhhhhhhhhh" });
    DB.prepare = realPrepare;
    check("two at once: the later one gives its room up and joins the earlier", second.role === "join" && second.code === first.code && rooms.size === 1);
}
// a relay login only when it is needed (0.1.29 and later)
{
    const later = await postCap("host", "192.0.2.120", { v: 3, rel: 129, name: "H", addrs: ["192.0.2.120:1"] });
    check("a game that asks later gets no login with its room", typeof later.code === "string" && !later.turn && !later.noRelay);
    check("... and none before anyone has joined", (await postCap("relay", "192.0.2.120", { code: later.code, key: later.key })).error === "bad request");
    const jn = await postCap("join", "192.0.2.121", { v: 3, rel: 129, code: later.code, name: "J", addrs: ["192.0.2.121:1"] });
    check("... nor for the one who joined", (await postCap("relay", "192.0.2.121", { code: later.code, key: jn.key })).error === "bad request");
    const asked = await postCap("relay", "192.0.2.120", { code: later.code, key: later.key });
    check("the host asks once someone has joined: answered (a login, or why not)", (asked.turn && typeof asked.turn.username === "string") || typeof asked.noRelay === "string");
    await postCap("addrs", "192.0.2.120", { code: later.code, key: later.key, addrs: ["192.0.2.120:1", "relay=104.30.1.9:5000"] });
    const again = await postCap("poll", "192.0.2.121", { code: later.code, key: jn.key });
    check("the one who joined learns the relay address by its poll", again.peer.addrs.includes("relay=104.30.1.9:5000"));
    const old = await postCap("host", "192.0.2.122", { v: 3, name: "H", addrs: ["192.0.2.122:1"] });
    check("an older game is answered as before (a login or a reason with its room)", !!old.turn || typeof old.noRelay === "string");
}
// only the newest release
{
    const strict = { DB, MIN_RELEASE: "129" };
    const ask = async (what, body) => (await worker.fetch(new Request("https://x/v1/" + what, { method: "POST", headers: { "cf-connecting-ip": "198.51.100.77" }, body: JSON.stringify(body) }), strict)).json();
    check("a game that does not say its release is refused when a minimum is set", (await ask("host", { v: 3, name: "O", addrs: ["198.51.100.77:1"] })).error === "old");
    check("an older release is refused", (await ask("quick", { v: 3, rel: 128, name: "O", addrs: ["198.51.100.77:1"], queue: "S", who: "oooooooooooo" })).error === "old");
    const ok = await ask("host", { v: 3, rel: 129, name: "N", addrs: ["198.51.100.77:1"] });
    check("the release named, or a newer one, is let in", typeof ok.code === "string" && typeof (await ask("host", { v: 3, rel: 130, name: "N", addrs: ["198.51.100.77:1"] })).code === "string");
    check("a room under way is not touched (its poll is answered)", (await ask("poll", { code: ok.code, key: ok.key })).peer === null);
    const hi = await (await worker.fetch(new Request("https://x/v1/hello", { method: "POST", headers: { "cf-connecting-ip": "198.51.100.77" }, body: JSON.stringify({ rel: 128 }) }), { DB, MIN_RELEASE: "129", LATEST_RELEASE: "131" })).json();
    check("hello says the minimum and the newest", hi.min === 129 && hi.latest === 131 && (await post("hello", "198.51.100.78", { rel: 128 })).min === 0);
    check("no minimum set: everyone as before", typeof (await post("host", "198.51.100.78", { v: 3, name: "O", addrs: ["198.51.100.78:1"] })).code === "string");
}
// the build key: only official releases, in stages (every key here is made up for the test; no release has them)
{
    const KEY = "dummy-build-key-for-tests-0001-not-real", OTHER = "dummy-build-key-for-tests-0002-not-real";
    const KEYS = JSON.stringify({ 134: KEY, 135: OTHER });
    const nowS = () => Math.floor(Date.now() / 1000);
    const sha = (text) => nodeCrypto.createHash("sha256").update(text).digest("hex");
    const sign = (key, path, release, time, text) => nodeCrypto.createHmac("sha256", key).update(["BT3-SIGN-1", "POST", path, release, time, sha(text)].join("\n")).digest("hex");
    let n = 0;
    // one request: o.key signs it (o.release, o.time, o.path: what is signed for, if not what is sent; o.sent: the
    // body that is really sent; o.sign: a signature given as it is). Every request comes from another address.
    const send = async (e, what, body, o = {}) => {
        n++;
        const text = JSON.stringify(body), headers = { "cf-connecting-ip": o.ip ?? `198.18.${Math.floor(n / 250)}.${(n % 250) + 1}` };
        if (o.key || o.sign) {
            const release = String(o.release ?? body.rel ?? 134), time = String(o.time ?? nowS());
            headers["x-bt3-release"] = release;
            headers["x-bt3-time"] = time;
            headers["x-bt3-sign"] = o.sign ?? sign(o.key, o.path ?? "/v1/" + what, release, time, text);
        }
        const r = await worker.fetch(new Request("https://x/v1/" + what, { method: "POST", headers, body: o.sent ?? text }), e);
        return { status: r.status, ...(await r.json()) };
    };
    let who = 0;
    const quick = () => ({ v: 4, rel: 134, name: "K", addrs: ["198.18.9.9:7000"], queue: "S", who: "keytest" + String(100000 + who++) });
    const host = () => ({ v: 4, rel: 134, name: "K", addrs: ["198.18.9.9:7000"] });
    const refused = (r, why) => r.status === 403 && r.error === "build key" && r.why === why && Math.abs(r.now - nowS()) <= 2 && r.code === undefined;
    const letIn = (r) => r.status === 200 && typeof r.code === "string";
    const logged = [], realLog = console.log;
    console.log = (...a) => { logged.push(a.join(" ")); };
    const results = [];
    const chk = (name, ok) => results.push([name, ok]); // (printed once the log is given back)
    rooms.clear();

    // no keys told to the Worker: as it always was, whatever is or is not sent
    chk("no BUILD_KEYS: an unsigned search is answered as before", letIn(await send({ DB }, "quick", quick())));
    chk("no BUILD_KEYS: REQUIRE_KEY alone refuses nothing", letIn(await send({ DB, REQUIRE_KEY: "all" }, "quick", quick())) && letIn(await send({ DB, REQUIRE_KEY: "all" }, "host", host())));
    chk("no BUILD_KEYS: a signed request is answered too (the headers are not looked at)", letIn(await send({ DB }, "host", host(), { key: KEY })));
    chk("no BUILD_KEYS: nothing was logged", logged.length === 0);

    // everything needs a key
    const all = { DB, BUILD_KEYS: KEYS, REQUIRE_KEY: "all" };
    rooms.clear();
    chk("all: signed with the release's key: a search is let in", letIn(await send(all, "quick", quick(), { key: KEY })));
    const h = await send(all, "host", host(), { key: KEY });
    chk("all: ... and a room", letIn(h));
    chk("all: ... and its poll (a request that names no release in its body)", (await send(all, "poll", { code: h.code, key: h.key }, { key: KEY })).peer === null);
    chk("all: ... and a join", typeof (await send(all, "join", { v: 4, rel: 134, code: h.code, name: "J", addrs: ["198.18.9.8:7000"] }, { key: KEY })).peer === "object");
    chk("all: another release with its own key is let in", letIn(await send(all, "host", { ...host(), rel: 135 }, { key: OTHER })));
    chk("all: unsigned: a search is refused", refused(await send(all, "quick", quick()), "none"));
    chk("all: unsigned: a room, a join, a poll, the queues' counts are refused", refused(await send(all, "host", host()), "none") && refused(await send(all, "join", { v: 4, rel: 134, code: h.code, name: "J", addrs: ["198.18.9.8:7000"] }), "none") &&
        refused(await send(all, "poll", { code: h.code, key: h.key }), "none") && refused(await send(all, "queues", { v: 4 }), "none"));
    chk("all: hello is still answered, and says the Worker's time", Math.abs((await send(all, "hello", { rel: 134 })).now - nowS()) <= 2);
    chk("wrong key: refused", refused(await send(all, "quick", quick(), { key: "dummy-build-key-for-tests-0003-not-real" }), "sign"));
    chk("the key of another release: refused", refused(await send(all, "quick", quick(), { key: OTHER }), "sign"));
    chk("a release the Worker has no key for: refused", refused(await send(all, "quick", { ...quick(), rel: 133 }, { key: KEY }), "release"));
    chk("the header's release is not the body's: refused", refused(await send(all, "quick", quick(), { key: OTHER, release: 135 }), "release"));
    chk("a time 301 seconds old: refused, and the answer says the Worker's time", refused(await send(all, "quick", quick(), { key: KEY, time: nowS() - 301 }), "time"));
    chk("a time 301 seconds ahead: refused", refused(await send(all, "quick", quick(), { key: KEY, time: nowS() + 301 }), "time"));
    chk("a time 200 seconds old: let in", letIn(await send(all, "quick", quick(), { key: KEY, time: nowS() - 200 })));
    chk("a stale time with a wrong key: refused as a wrong signature (the time is told only to a holder of the key)", refused(await send(all, "quick", quick(), { key: OTHER, time: nowS() - 900 }), "sign"));
    {
        const first = quick(), changed = { ...first, name: "Changed" };
        chk("sent again with a changed body: refused", refused(await send(all, "quick", first, { key: KEY, sent: JSON.stringify(changed) }), "sign"));
        chk("... also when only a space was added", refused(await send(all, "quick", first, { key: KEY, sent: JSON.stringify(first) + " " }), "sign"));
        chk("signed for another path: refused", refused(await send(all, "quick", first, { key: KEY, path: "/v1/host" }), "sign"));
        chk("a signature that is no signature: refused", refused(await send(all, "quick", first, { sign: "zz" }), "sign") && refused(await send(all, "quick", first, { sign: "0".repeat(64) }), "sign"));
    }

    // only quick match needs a key
    const onlyQuick = { DB, BUILD_KEYS: KEYS, REQUIRE_KEY: "quick" };
    rooms.clear();
    chk("quick: an unsigned search is refused, one with a wrong key too", refused(await send(onlyQuick, "quick", quick()), "none") && refused(await send(onlyQuick, "quick", quick(), { key: OTHER }), "sign"));
    chk("quick: a signed search is let in", letIn(await send(onlyQuick, "quick", quick(), { key: KEY })));
    const open = await send(onlyQuick, "host", host());
    chk("quick: an unsigned room with a code is made", letIn(open));
    chk("quick: ... joined, asked and its relay asked for, all unsigned", typeof (await send(onlyQuick, "join", { v: 4, rel: 134, code: open.code, name: "J", addrs: ["198.18.9.8:7000"] })).peer === "object" &&
        (await send(onlyQuick, "poll", { code: open.code, key: open.key })).peer.name === "J" && (await send(onlyQuick, "relay", { code: open.code, key: open.key })).error !== "build key" &&
        typeof (await send(onlyQuick, "queues", { v: 4 })).waiting === "object");
    chk("quick: a room with a code and a wrong key is made too (logged, not refused)", letIn(await send(onlyQuick, "host", host(), { key: OTHER })));
    chk("REQUIRE_KEY is read whatever its case and spaces", refused(await send({ DB, BUILD_KEYS: KEYS, REQUIRE_KEY: " Quick " }, "quick", quick()), "none"));

    // nothing is refused: the checks are only logged and counted
    rooms.clear();
    for (const mode of [undefined, "off", "something else"]) {
        const e = mode === undefined ? { DB, BUILD_KEYS: KEYS } : { DB, BUILD_KEYS: KEYS, REQUIRE_KEY: mode };
        chk(`REQUIRE_KEY ${mode ?? "not set"}: unsigned, wrong key, stale and rightly signed are all let in`, letIn(await send(e, "quick", quick())) && letIn(await send(e, "host", host(), { key: OTHER })) &&
            letIn(await send(e, "host", host(), { key: KEY, time: nowS() - 5000 })) && letIn(await send(e, "quick", quick(), { key: KEY })));
    }

    // with a minimum release
    {
        const strict = { DB, BUILD_KEYS: KEYS, REQUIRE_KEY: "all", MIN_RELEASE: "135" };
        const old = await send(strict, "quick", quick(), { key: KEY });
        chk("a rightly signed release below MIN_RELEASE is told to update, not that it is no release", old.status === 426 && old.error === "old");
        chk("the key of a release below MIN_RELEASE under a newer number in the body: refused", refused(await send(strict, "quick", { ...quick(), rel: 135 }, { key: KEY, release: 134 }), "release"));
        chk("... and for a request that names no release in its body", refused(await send(strict, "queues", { v: 4 }, { key: KEY, release: 134 }), "release"));
        chk("the release at the minimum, with its key, is let in", letIn(await send(strict, "quick", { ...quick(), rel: 135 }, { key: OTHER })));
    }

    // a secret that cannot be read
    {
        const broken = { DB, BUILD_KEYS: "{134: not json", REQUIRE_KEY: "quick", ADMIN_KEY: "0123456789abcdef0123" };
        chk("BUILD_KEYS that is not JSON: no key is known, a signed search is refused", refused(await send(broken, "quick", quick(), { key: KEY }), "release"));
        chk("... rooms with codes go on", letIn(await send(broken, "host", host())));
        chk("... with REQUIRE_KEY off nothing is refused", letIn(await send({ ...broken, REQUIRE_KEY: "off" }, "quick", quick(), { key: KEY })));
        const st = await send(broken, "status", { admin: broken.ADMIN_KEY });
        chk("... and the status says so", st.buildKey && typeof st.buildKey.problem === "string" && st.buildKey.releases.length === 0);
        const short = await send({ DB, BUILD_KEYS: JSON.stringify({ 134: KEY, 135: "short", x: OTHER }), ADMIN_KEY: broken.ADMIN_KEY }, "status", { admin: broken.ADMIN_KEY });
        chk("a key that is too short and a release that is no number are left out, and said", JSON.stringify(short.buildKey.releases) === "[134]" && typeof short.buildKey.problem === "string");
    }

    // what the keeper is told
    {
        const e = { ...all, ADMIN_KEY: "0123456789abcdef0123" };
        const st = await send(e, "status", { admin: e.ADMIN_KEY });
        chk("status: the releases with a key, the stage, today's checks", st.buildKey && JSON.stringify(st.buildKey.releases) === "[134,135]" && st.buildKey.require === "all" && st.buildKey.today.ok > 0 && st.buildKey.today.none > 0 &&
            st.buildKey.today.sign > 0 && st.buildKey.today.time > 0 && st.buildKey.today.release > 0);
        chk("status: no key in it", !JSON.stringify(st).includes(KEY) && !JSON.stringify(st).includes(OTHER));
        chk("status without BUILD_KEYS says nothing is checked", typeof (await send({ DB, ADMIN_KEY: e.ADMIN_KEY }, "status", { admin: e.ADMIN_KEY })).buildKey.keys === "string");
    }
    chk("refusals were logged, and what was let in although it failed", logged.some((l) => l.includes("refused")) && logged.some((l) => l.includes("let in")));
    chk("no key and no signature in the log", !logged.some((l) => l.includes(KEY) || l.includes(OTHER) || /[0-9a-f]{64}/.test(l)));

    // the game's own fixed request (net_sign.c, BT3_SIGN_TEST; the same in test_sign.py): the signature the game
    // prints for it is accepted here, at the time it is signed for
    {
        const realNow = Date.now;
        Date.now = () => 1791900000 * 1000;
        const fixed = { DB, BUILD_KEYS: JSON.stringify({ 134: "dummy-test-key-0123456789-not-a-secret" }), REQUIRE_KEY: "all" };
        const body = '{"v":4,"rel":134,"name":"Test","addrs":["203.0.113.5:7000"],"queue":"S","who":"abcdefghijklmnop"}';
        const game = "1f7af5fd0b8c1a7039a80794265bc3ec4d684ef7f4ed35b39291ba8ebe54a613";
        chk("the fixed request's body hashes as the game says", sha(body) === "893a2fd1a953d800df5c609bc8c208c5a5b9f4340eacb491fe2fb301fc7ee1c4");
        chk("the signature the game prints for its fixed request is accepted", letIn(await send(fixed, "quick", JSON.parse(body), { sign: game, release: 134, time: 1791900000, sent: body })));
        chk("... and not with one digit changed", refused(await send(fixed, "quick", JSON.parse(body), { sign: "2" + game.slice(1), release: 134, time: 1791900000, sent: body }), "sign"));
        Date.now = realNow;
    }
    console.log = realLog;
    for (const [name, ok] of results) {
        check(name, ok);
    }
}
console.log(fails === 0 ? "all passed" : fails + " failed");
process.exit(fails === 0 ? 0 : 1);
