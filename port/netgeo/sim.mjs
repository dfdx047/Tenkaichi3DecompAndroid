// Quick match, the rules of match.js against the rules before them, with players arriving at random from a
// Brazilian-majority community (C below): the round trip estimate of the pairs made and how long they waited.
// Run: git show <a commit before port/netgeo>:port/dragonnet/worker.js | sed -n '/^function placeOf/,/^async function waitingRooms/p' | sed '$d' > old.js
//      node port/netgeo/sim.mjs old.js            (a few minutes; nothing is sent anywhere)
// The estimate is also the measure here: this compares the rules, it does not replace measured pings.
import fs from "node:fs";
const N = await import(process.argv[3] ? new URL(process.argv[3], "file://" + process.cwd() + "/").href : "./match.js");
// the old rules exactly as deployed (from 1.0.2-Regulus), with their constants
const OLD = new Function("NEAR_KM","WIDEN_SECONDS","REGION_WIDEN_SECONDS","REGIONS","MIDDLE_EAST", fs.readFileSync(process.argv[2],"utf8") + "\nreturn {placeOf, pick};")(2500,25,40,N.REGIONS,N.MIDDLE_EAST);
// players by where they are (a Brazilian-majority community), cities with a spread
const C = [
 [0.30,-23.5,-46.6,"SA","BR"],[0.10,-22.9,-43.2,"SA","BR"],[0.06,-8.0,-34.9,"SA","BR"],[0.05,-30.0,-51.2,"SA","BR"],[0.04,-3.1,-60.0,"SA","BR"],
 [0.06,-34.6,-58.4,"SA","AR"],[0.03,-33.4,-70.7,"SA","CL"],[0.03,4.7,-74.1,"SA","CO"],[0.03,-12.0,-77.0,"SA","PE"],
 [0.06,19.4,-99.1,"NA","MX"],[0.05,25.8,-80.2,"NA","US"],[0.03,34.0,-118.2,"NA","US"],
 [0.04,38.7,-9.1,"EU","PT"],[0.04,40.4,-3.7,"EU","ES"],[0.02,48.9,2.3,"EU","FR"],
 [0.02,25.2,55.3,"AS","AE"],[0.01,30.0,31.2,"AF","EG"],[0.01,6.5,3.4,"AF","NG"],[0.01,35.7,139.7,"AS","JP"],[0.01,-33.9,151.2,"OC","AU"],
];
let seed = 7; const rnd = () => ((seed = (seed * 1103515245 + 12345) % 2147483648) / 2147483648);
function city(){ let r = rnd()*C.reduce((a,c)=>a+c[0],0); for(const c of C){ if((r-=c[0])<=0) return c; } return C[0]; }
const cf = (c) => ({ cf: { latitude: String(c[1] + (rnd()-0.5)*2), longitude: String(c[2] + (rnd()-0.5)*2), continent: c[3], country: c[4] } });
function run(rules, rate, seconds, s0){
  seed = s0; const rooms = []; const pairs = []; let id = 0; const searchers = [];
  for (let t = 0; t < seconds; t++){
    // arrivals
    let k = 0; let p = Math.exp(-rate), F = p, u = rnd(); while (u > F) { k++; p *= rate/(k); F += p; }
    for (let i = 0; i < k; i++) searchers.push({ req: cf(city()), began: t, who: "w" + (id++), room: null, next: t });
    for (const s of searchers){
      if (s.done || s.next > t) continue;
      const waited = t - s.began;
      const isNew = rules === "new";
      const place = isNew ? N.placeOf(s.req) : OLD.placeOf(s.req);
      const me = { place, choice: "", waited, who: s.who };
      const rows = rooms.filter(r => !r.taken && r.who !== s.who);
      const got = isNew ? N.pick(rows, me, t) : OLD.pick(rows, me, t);
      if (got){ got.taken = true; s.done = true; got.owner.done = true; if (s.room) s.room.taken = true;
        const ms = N.estimateMs(N.placeOf(s.req), N.placeOf(got.owner.req));
        pairs.push({ ms, wait: Math.max(t - s.began, t - got.owner.began) }); continue; }
      if (!s.room){ s.room = { loc: isNew ? N.withChoice(place, "") : place + ",", made: t, who: s.who, owner: s }; rooms.push(s.room); }
      s.next = t + 10;  // a waiting game searches again from its room every ten seconds
      if (t - s.began > 600) { s.done = true; s.room.taken = true; }
    }
  }
  pairs.sort((a,b)=>a.ms-b.ms);
  const avg = (f) => pairs.reduce((a,p)=>a+f(p),0)/pairs.length;
  const pct = (q) => pairs[Math.floor(q*(pairs.length-1))].ms;
  return { n: pairs.length, ms: avg(p=>p.ms), p90: pct(0.9), over150: 100*pairs.filter(p=>p.ms>150).length/pairs.length, wait: avg(p=>p.wait), wait90: [...pairs].sort((a,b)=>a.wait-b.wait)[Math.floor(0.9*(pairs.length-1))].wait };
}
for (const [label, rate] of [["poucos jogadores (1 a cada 30 s)", 1/30], ["médio (1 a cada 10 s)", 0.1], ["movimentado (1 a cada 3 s)", 1/3], ["pico (1 por segundo)", 1]]){
  for (const rules of ["old", "new"]){
    const r = [1,2,3,4,5].map(s => run(rules, rate, 20000, s*99991)).reduce((a,b)=>({n:a.n+b.n,ms:a.ms+b.ms/5,p90:a.p90+b.p90/5,over150:a.over150+b.over150/5,wait:a.wait+b.wait/5,wait90:a.wait90+b.wait90/5}),{n:0,ms:0,p90:0,over150:0,wait:0,wait90:0});
    console.log(`${label.padEnd(34)} ${rules==="old"?"ATUAL":"NOVA "}  pares ${String(r.n).padStart(6)}  ping médio ${r.ms.toFixed(0).padStart(3)} ms  p90 ${r.p90.toFixed(0).padStart(3)} ms  >150ms ${r.over150.toFixed(1).padStart(5)}%  espera média ${r.wait.toFixed(1).padStart(5)} s  p90 ${r.wait90.toFixed(0).padStart(3)} s`);
  }
}
