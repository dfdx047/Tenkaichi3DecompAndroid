// Players on every continent through port/netgeo/match.js: who is put with whom, and when.
// Run: node port/netgeo/test.mjs   (any recent Node; nothing is sent anywhere)
import * as G from "./match.js";

let fails = 0;
const check = (what, ok, extra = "") => {
    console.log((ok ? "ok   " : "FAIL ") + what + (ok ? "" : "  " + extra));
    if (!ok) fails++;
};
// a request as Cloudflare describes it
const at = (latitude, longitude, continent, country) => G.placeOf({ cf: { latitude: String(latitude), longitude: String(longitude), continent, country } });
const P = {
    SaoPaulo: at(-23.55, -46.63, "SA", "BR"), Rio: at(-22.91, -43.17, "SA", "BR"), Recife: at(-8.05, -34.88, "SA", "BR"),
    BuenosAires: at(-34.6, -58.38, "SA", "AR"), Santiago: at(-33.45, -70.67, "SA", "CL"), Bogota: at(4.71, -74.07, "SA", "CO"),
    Miami: at(25.76, -80.19, "NA", "US"), NewYork: at(40.71, -74.0, "NA", "US"), LosAngeles: at(34.05, -118.24, "NA", "US"),
    Mexico: at(19.43, -99.13, "NA", "MX"), Lisbon: at(38.72, -9.14, "EU", "PT"), Madrid: at(40.42, -3.7, "EU", "ES"),
    Frankfurt: at(50.11, 8.68, "EU", "DE"), Dubai: at(25.2, 55.27, "AS", "AE"), Riyadh: at(24.71, 46.68, "AS", "SA"),
    Cairo: at(30.04, 31.24, "AF", "EG"), Lagos: at(6.52, 3.38, "AF", "NG"), Johannesburg: at(-26.2, 28.05, "AF", "ZA"),
    Tokyo: at(35.68, 139.69, "AS", "JP"), Seoul: at(37.57, 126.98, "AS", "KR"), Mumbai: at(19.08, 72.88, "AS", "IN"),
    Sydney: at(-33.87, 151.21, "OC", "AU"), Auckland: at(-36.85, 174.76, "OC", "NZ"),
};
const ms = (a, b) => Math.round(G.estimateMs(P[a], P[b]));

// places: region, country, nothing more than whole degrees
check("Sao Paulo: South America, Brazil, whole degrees", P.SaoPaulo === "-24,-47,SA,,BR" && G.regionOf(P.SaoPaulo) === "SA" && G.countryOf(P.SaoPaulo) === "BR", P.SaoPaulo);
check("Riyadh: the Middle East by its country (SA is Saudi Arabia, not South America)", G.regionOf(P.Riyadh) === "ME" && G.countryOf(P.Riyadh) === "SA");
check("Cairo: the Middle East, not Africa", G.regionOf(P.Cairo) === "ME");
check("Lagos and Johannesburg: Africa; Sydney and Auckland: Oceania", G.regionOf(P.Lagos) === "AF" && G.regionOf(P.Johannesburg) === "AF" && G.regionOf(P.Sydney) === "OC" && G.regionOf(P.Auckland) === "OC");
check("an unknown or Tor country is no country", G.countryOf(G.placeOf({ cf: { continent: "EU", country: "T1" } })) === "" && G.countryOf(G.placeOf({ cf: { continent: "EU", country: "XX" } })) === "");
check("a request Cloudflare says nothing of: nothing known", G.placeOf({}) === ",,,," && G.estimateMs(G.placeOf({}), P.Rio) === Infinity);
check("a room's choice sits in its own slot; the country stays readable", G.choiceOf(G.withChoice(P.Riyadh, "SA")) === "SA" && G.countryOf(G.withChoice(P.Riyadh, "SA")) === "SA" && G.regionOf(G.withChoice(P.Riyadh, "SA")) === "ME");
check("a room of an older game (no country slot): choice read, no country", G.choiceOf("50,9,EU,*") === "*" && G.countryOf("50,9,EU,*") === "");

// estimates: the order the user's examples ask for
console.log(`     estimates from Sao Paulo: Rio ${ms("SaoPaulo", "Rio")}, Buenos Aires ${ms("SaoPaulo", "BuenosAires")}, Bogota ${ms("SaoPaulo", "Bogota")}, Miami ${ms("SaoPaulo", "Miami")}, Lisbon ${ms("SaoPaulo", "Lisbon")}, Tokyo ${ms("SaoPaulo", "Tokyo")} ms`);
check("Brazil-Brazil < Brazil-Argentina < Brazil-United States < Brazil-Europe < Brazil-Japan",
      ms("SaoPaulo", "Rio") < ms("SaoPaulo", "BuenosAires") && ms("SaoPaulo", "BuenosAires") < ms("SaoPaulo", "Miami") && ms("SaoPaulo", "Miami") < ms("SaoPaulo", "Lisbon") && ms("SaoPaulo", "Lisbon") < ms("SaoPaulo", "Tokyo"));
check("estimates near known round trips (Sao Paulo-Buenos Aires 30-45, New York-Los Angeles 60-80, Frankfurt-Madrid 25-40)",
      ms("SaoPaulo", "BuenosAires") >= 30 && ms("SaoPaulo", "BuenosAires") <= 45 && ms("NewYork", "LosAngeles") >= 60 && ms("NewYork", "LosAngeles") <= 80 && ms("Frankfurt", "Madrid") >= 25 && ms("Frankfurt", "Madrid") <= 40);
check("the same pair either way", ms("Lagos", "Sydney") === ms("Sydney", "Lagos"));
check("one region without positions: an estimate, lower within one country", G.estimateMs(",,SA,,BR", ",,SA,,BR") < G.estimateMs(",,SA,,BR", ",,SA,,AR") && G.estimateMs(",,SA,,BR", ",,SA,,AR") <= 70);

// widening: low estimates first, anyone in the end
check("at once: only pairs under 47 ms (about the 2,500 km the rules before took at once)", G.maxMsAfter(0) === 47 && G.maxMsAfter(11) === 47);
check("then 55 (12 s), 120 (18 s), 180 (25 s), anyone (30 s)", G.maxMsAfter(12) === 55 && G.maxMsAfter(18) === 120 && G.maxMsAfter(25) === 180 && G.maxMsAfter(29) === 180 && G.maxMsAfter(30) === Infinity);

// quick match: the rooms that wait, oldest first
const now = 100000;
const room = (name, where, choice = "", age = 0) => ({ name, loc: G.withChoice(P[where], choice), made: now - age, who: "room" + name });
const search = (where, waited = 0, choice = "") => ({ place: P[where], choice, waited, who: "searcher" });
const pick = (rows, me) => { const r = G.pick(rows, me, now); return r ? r.name : null; };

check("by region: Brazil takes Brazil over an older Argentine room", pick([room("ar", "BuenosAires", "", 5), room("br", "Rio")], search("SaoPaulo")) === "br");
check("by continent: no Brazilian, an Argentine at once", pick([room("us", "Miami", "", 5), room("ar", "BuenosAires")], search("SaoPaulo")) === "ar");
check("no one in the region: the United States not at once ...", pick([room("us", "Miami")], search("SaoPaulo")) === null);
check("... but once the search has waited 25 seconds (not at 18)", pick([room("us", "Miami")], search("SaoPaulo", 18)) === null && pick([room("us", "Miami")], search("SaoPaulo", 25)) === "us");
check("Europe from Brazil: at 25 seconds, not at 18", pick([room("pt", "Lisbon")], search("SaoPaulo", 18)) === null && pick([room("pt", "Lisbon")], search("SaoPaulo", 25)) === "pt");
check("the room's own wait counts as well (it waited 25)", pick([room("pt", "Lisbon", "", 25)], search("SaoPaulo", 0)) === "pt");
check("Japan from Brazil: only when anyone is taken (30 s)", pick([room("jp", "Tokyo")], search("SaoPaulo", 29)) === null && pick([room("jp", "Tokyo")], search("SaoPaulo", 30)) === "jp");
check("nobody is left without a match: after 30 s the best of several far rooms", pick([room("jp", "Tokyo"), room("au", "Sydney"), room("ng", "Lagos")], search("SaoPaulo", 30)) === "ng");
check("a Brazilian who comes a few seconds later is preferred to Europe (the Brazilian had waited 10)", pick([room("pt", "Lisbon", "", 12), room("br", "Rio")], search("SaoPaulo", 10)) === "br");
check("Oceania: Sydney takes Auckland at once", pick([room("nz", "Auckland")], search("Sydney")) === "nz");
check("Asia: Tokyo takes Seoul at once, Mumbai later", pick([room("in", "Mumbai"), room("kr", "Seoul")], search("Tokyo")) === "kr" && pick([room("in", "Mumbai")], search("Tokyo")) === null);
check("Africa: Lagos and Johannesburg are far apart (4,500 km, about 78 ms): from 18 s", pick([room("za", "Johannesburg")], search("Lagos", 17)) === null && pick([room("za", "Johannesburg")], search("Lagos", 18)) === "za");
check("the Middle East: Dubai takes Riyadh at once", pick([room("sa", "Riyadh")], search("Dubai")) === "sa");
check("North America: New York takes Miami, not Los Angeles first", pick([room("la", "LosAngeles", "", 9), room("mi", "Miami")], search("NewYork")) === "mi");
{   // two rooms as far from a player near the border, one in its country: the country decides
    const border = at(-30, -51, "SA", "BR"), br = { name: "br", loc: G.withChoice(at(-30, -47, "SA", "BR"), ""), made: now, who: "b" }, ar = { name: "ar", loc: G.withChoice(at(-30, -55, "SA", "AR"), ""), made: now - 5, who: "a" };
    check("one country is worth a little: between two rooms as far, the one in the player's country", G.pick([ar, br], { place: border, choice: "", waited: 0, who: "s" }, now).name === "br");
    check("... but only a little: Sao Paulo takes Buenos Aires (35 ms) over Recife (Brazil, 42 ms)", pick([room("re", "Recife"), room("ar", "BuenosAires")], search("SaoPaulo")) === "ar");
}
check("a region chosen: South America only, at once", pick([room("mi", "Miami"), room("ar", "BuenosAires")], search("SaoPaulo", 0, "SA")) === "ar");
check("a region chosen: not Miami before 40 s, though Auto takes it at 25", pick([room("mi", "Miami")], search("SaoPaulo", 20, "SA")) === null && pick([room("mi", "Miami")], search("SaoPaulo", 40, "SA")) === "mi");
check("a room whose host chose Europe does not take Brazil before 40 s", pick([room("pt", "Lisbon", "EU", 35)], search("SaoPaulo", 35)) === null);
check("Anywhere: the longest waiting, wherever it is", pick([room("jp", "Tokyo", "*", 5), room("br", "Rio", "*")], search("SaoPaulo", 0, "*")) === "jp");
check("an older game's room (no place): only when anyone is taken", pick([{ name: "old", loc: null, made: now, who: "o" }], search("SaoPaulo", 29)) === null && pick([{ name: "old", loc: null, made: now, who: "o" }], search("SaoPaulo", 30)) === "old");
check("one's own room is never one's match", G.pick([{ ...room("br", "Rio"), who: "searcher" }], search("SaoPaulo"), now) === null);

// Ranked: within the rating window, the best pair; a search without a place as before
const rk = (name, where, age = 0) => ({ name, loc: where ? P[where] : null, made: now - age });
const ranked = (rows, where, waited = 0) => { const r = G.pickRanked(rows, where ? P[where] : null, waited, now); return r ? r.name : null; };
check("Ranked: Brazil takes Brazil over an older search in Japan", ranked([rk("jp", "Tokyo", 20), rk("br", "Rio")], "SaoPaulo") === "br");
check("Ranked: Japan is not taken at once, only after 30 s", ranked([rk("jp", "Tokyo")], "SaoPaulo") === null && ranked([rk("jp", "Tokyo", 30)], "SaoPaulo") === "jp");
check("Ranked: a search without a place is taken at once, as before", ranked([rk("old", null)], "SaoPaulo") === "old" && ranked([rk("jp", "Tokyo")], null) === "jp");

// time: picking among CANDIDATES (50) rooms, many times
{
    const names = Object.keys(P), rows = [];
    for (let i = 0; i < 50; i++) rows.push(room("r" + i, names[i % names.length], i % 7 === 0 ? "*" : "", i));
    const t = process.hrtime.bigint();
    for (let i = 0; i < 10000; i++) G.pick(rows, search(names[i % names.length], i % 60), now);
    const us = Number(process.hrtime.bigint() - t) / 1000 / 10000;
    console.log(`     benchmark: one pick among 50 rooms takes ${us.toFixed(1)} µs`);
    check("a pick among 50 rooms well under a millisecond (a Worker's request has 10 ms of CPU)", us < 500);
}

console.log(fails ? fails + " failed" : "all passed");
process.exit(fails ? 1 : 0);
