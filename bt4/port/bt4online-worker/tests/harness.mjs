import worker from "../worker.js";
import { makeD1, fakeClock } from "./d1shim.mjs";

export function boot() {
    const clock = fakeClock();
    const env = { DB: makeD1() };
    const ctx = { waitUntil() {} };
    const post = async (what, body, ip = "203.0.113.7") => {
        const res = await worker.fetch(new Request(`http://worker.local/v1/${what}`, { method: "POST", body: JSON.stringify(body), headers: { "cf-connecting-ip": ip } }), env, ctx);
        return res.json();
    };
    let tag = 0;
    const rk = () => (++tag).toString(16).padStart(16, "0");
    const register = async (name, ip) => { const r = await post("register", { name }, ip); if (!r.ok) throw new Error("register " + name + ": " + JSON.stringify(r)); return r; };
    const auth = (p) => ({ id: p.id, secret: p.secret });
    const rep = (p, o) => post("report", { ...auth(p), ...o });
    // a Ranked match as the service's own queue makes it: the first searcher hosts, the second joins
    const v = 140001;
    let q = 0;
    // bind: the two games send their account in the search (what a client of this release does); otherwise as a legacy game (no account)
    const rankedRoom = async (A, B, bind = false, extra = {}) => {
        q++;
        const acct = (p) => (bind ? { acct: { id: p.id, secret: p.secret } } : {});
        const qa = await post("quick", { v, rel: 140, name: A.name, addrs: [`10.0.${q}.1:7000`], queue: "D20", who: "aaaaaaaa" + q, waited: 0, area: "any", ...acct(A), ...(extra.a || {}) }, `100.64.${(q >> 8) & 255}.${(q & 255) * 2}`);
        const qb = await post("quick", { v, rel: 140, name: B.name, addrs: [`10.0.${q}.2:7000`], queue: "D20", who: "bbbbbbbb" + q, waited: 0, area: "any", ...acct(B), ...(extra.b || {}) }, `100.64.${(q >> 8) & 255}.${(q & 255) * 2 + 1}`);
        if (!(qa.role === "host" && qb.role === "join" && qb.code === qa.code)) throw new Error("no ranked room: " + JSON.stringify([qa, qb]));
        return extra.full ? { code: qa.code, host: qa, join: qb } : qa.code;
    };
    return { worker, env, clock, post, rk, register, auth, rep, rankedRoom };
}
