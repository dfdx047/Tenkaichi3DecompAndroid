// How far a rating can be farmed with several accounts under the Worker's rules (K 32, floor 100, RANKED_PER_PAIR_DAY battles a day per pair of accounts),
// using the Worker's own elo(). The reports of a fabricated battle only need the two accounts to agree: this is the ceiling of that, per week.
//   node --no-warnings farm_sim.mjs
import { elo, RANKED_PER_PAIR_DAY } from "../profile.js";
const MIN = 100;
function farm(k, days) {
    let main = 1000; const sy = Array(k).fill(1000); let wins = 0;
    for (let d = 0; d < days; d++) for (let i = 0; i < k; i++) for (let b = 0; b < RANKED_PER_PAIR_DAY; b++) {
        const g = elo(main, sy[i]); sy[i] -= Math.min(g, Math.max(0, sy[i] - MIN)); main += g; wins++;
    }
    return { main, wins };
}
for (const k of [1, 3, 10, 30]) for (const d of [1, 7]) { const r = farm(k, d); console.log(`accounts=${k} days=${d}: rating ${r.main} after ${r.wins} fabricated wins`); }
