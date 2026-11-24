// Who plays whom: the nearest-first rules shared by Dragon Net's Worker (port/dragonnet: BT3 1v1, BT4 1v1 and BT3
// Ranked) and BT4's Ranked rooms (bt4/port/bt4online-worker/rooms.js). Plain functions, no I/O: tested by
// port/netgeo/test.mjs with players on every continent.
//
// Where a player is: what Cloudflare says about the request (request.cf), never what the game says, so a game cannot
// claim another place (a VPN can, as for any service). Kept only as whole degrees of latitude and longitude (about
// 100 km), the region (REGIONS) and the country, for as long as a search lasts; the other player is told the region
// only. No GPS, no address.
//
// How good a pair is: an estimate of the round trip in milliseconds (estimateMs), from the distance and whether the
// two are in the same region. It is an estimate: routes between continents often go round (South America to Europe
// through the United States), which is what CROSS_REGION_MS stands for. A search takes, at first, only pairs under
// a low estimate and widens step by step as it waits (STEPS), so a nearby player who comes a few seconds later is
// still preferred to one on another continent; after ANY_SECONDS it takes anyone, the best estimate first, so
// nobody is left without a match because nobody near is online.

export const REGIONS = ["NA", "SA", "EU", "ME", "AF", "AS", "OC"];
// Countries placed in the Middle East rather than in Cloudflare's continent (Egypt, Turkey and Cyprus with it, as
// games' regions usually have them).
export const MIDDLE_EAST = ["AE", "BH", "CY", "EG", "IL", "IQ", "IR", "JO", "KW", "LB", "OM", "PS", "QA", "SA", "SY", "TR", "YE"];

export const BASE_MS = 10;          // two players in the same city: the last mile of each, both ways
export const MS_PER_KM = 0.015;     // round trip over fibre with ordinary routing (Sao Paulo-Buenos Aires about 35 ms)
export const CROSS_REGION_MS = 30;  // pairs in two regions: the detours of intercontinental routes
export const SAME_REGION_MS = 60;   // the same region, one position unknown (Cloudflare left it out)
export const COUNTRY_BONUS_MS = 15; // in a choice between two, the one in the player's own country is worth this much
// [round trip estimate in ms, accepted once the longer of the two waits has reached these seconds]
export const STEPS = [[70, 0], [110, 10], [160, 20], [230, 30]];
export const ANY_SECONDS = 45;      // ... and anyone, the best estimate first
export const REGION_WIDEN_SECONDS = 40; // a player who chose a region: players placed there at once, anyone after this

// Where a request comes from: "lat,lon,RR,,CC" (whole degrees, region, an empty slot, country), empty parts for what is
// not known. The fourth slot is where a quick-match room keeps what its host chose (withChoice), as rooms always have;
// the country is always the fifth, so a country code never reads as a region ("SA": Saudi Arabia, South America).
export function placeOf(request) {
    const cf = (request && request.cf) || {};
    const la = Number.parseFloat(cf.latitude), lo = Number.parseFloat(cf.longitude);
    const known = Number.isFinite(la) && Number.isFinite(lo) && Math.abs(la) <= 90 && Math.abs(lo) <= 180;
    const country = typeof cf.country === "string" && /^[A-Z]{2}$/.test(cf.country) && cf.country !== "XX" && cf.country !== "T1" ? cf.country : "";
    const region = MIDDLE_EAST.includes(country) ? "ME" : REGIONS.includes(cf.continent) ? cf.continent : "";
    return (known ? Math.round(la) + "," + Math.round(lo) : ",") + "," + region + ",," + country;
}

// A place with a quick-match room's choice in its fourth slot ("" Auto, "*" Anywhere, a region).
export function withChoice(place, choice) {
    const p = parts(place);
    while (p.length < 5) {
        p.push("");
    }
    p[3] = choice;
    return p.slice(0, 5).join(",");
}

// A kept place: "lat,lon,RR,choice,CC" (rooms made before the country have only "lat,lon,RR,choice").
function parts(place) {
    return typeof place === "string" ? place.split(",") : [];
}
export function regionOf(place) {
    const c = parts(place)[2] ?? "";
    return REGIONS.includes(c) ? c : "";
}
export function countryOf(place) {
    const c = parts(place)[4] ?? "";
    return /^[A-Z]{2}$/.test(c) ? c : "";
}
// What a quick-match room's host chose: "" Auto, "*" Anywhere, a region; null for a room of an older game (no place).
export function choiceOf(loc) {
    if (typeof loc !== "string") {
        return null;
    }
    const c = parts(loc)[3] ?? "";
    return c === "*" || REGIONS.includes(c) ? c : "";
}
// The choice a request makes (`area`): "" Auto (also when it says nothing it knows), "*" Anywhere, or a region.
export function choiceIn(area) {
    return area === "any" ? "*" : REGIONS.includes(area) ? area : "";
}

// Great-circle distance in km; NaN when a position is not known.
export function distanceKm(a, b) {
    const [la1, lo1] = parts(a).map((x) => (x === "" ? NaN : Number(x)));
    const [la2, lo2] = parts(b).map((x) => (x === "" ? NaN : Number(x)));
    if (![la1, lo1, la2, lo2].every(Number.isFinite)) {
        return NaN;
    }
    const r = Math.PI / 180, h = Math.sin(((la2 - la1) * r) / 2) ** 2 + Math.cos(la1 * r) * Math.cos(la2 * r) * Math.sin(((lo2 - lo1) * r) / 2) ** 2;
    return 2 * 6371 * Math.asin(Math.min(1, Math.sqrt(h)));
}

// The round trip two places are estimated at, in ms (Infinity: nothing known of one of them, a room of an older game).
export function estimateMs(a, b) {
    const ra = regionOf(a), rb = regionOf(b), km = distanceKm(a, b);
    if (Number.isFinite(km)) {
        return BASE_MS + MS_PER_KM * km + (ra !== "" && rb !== "" && ra !== rb ? CROSS_REGION_MS : 0);
    }
    if (ra !== "" && ra === rb) {
        return countryOf(a) !== "" && countryOf(a) === countryOf(b) ? BASE_MS + 20 : SAME_REGION_MS;
    }
    return Infinity;
}

// The highest estimate a search takes after `waited` seconds (Infinity: anyone).
export function maxMsAfter(waited) {
    if (waited >= ANY_SECONDS) {
        return Infinity;
    }
    let max = STEPS[0][0];
    for (const [ms, s] of STEPS) {
        if (waited >= s) {
            max = ms;
        }
    }
    return max;
}

// Whether one side takes the other: `choice` its own ("" Auto, "*" Anywhere, a region; null an older game, as
// Anywhere), `region` where the other is placed, `ms` the pair's estimate, `waited` the longer of the two waits.
export function accepts(choice, region, ms, waited) {
    if (choice === null || choice === "*") {
        return true;
    }
    if (choice === "") {
        return ms <= maxMsAfter(waited);
    }
    return region === choice || waited >= REGION_WIDEN_SECONDS;
}

// How a pair ranks among those both sides take: lower is better (the estimate, less COUNTRY_BONUS_MS in one country).
export function score(a, b) {
    const ms = estimateMs(a, b), ca = countryOf(a);
    return ms - (ca !== "" && ca === countryOf(b) ? COUNTRY_BONUS_MS : 0);
}

// Quick match: of the rooms that wait (oldest first: {loc, made, who}), the one the searcher `me` ({place, choice,
// waited, who}) is put with, or null. Both sides must take each other, the wait being the longer of the searcher's
// and the room's (choiceOf(loc): what the room's host chose). Auto and a region take the best pair; Anywhere and
// an older game whoever waits longest.
export function pick(rows, me, now) {
    let best = null, bestScore = Infinity;
    const mine = me.place ? regionOf(me.place) : "";
    for (const r of rows) {
        if (r.who === me.who) {
            continue;
        }
        const ms = estimateMs(me.place, r.loc), waited = Math.max(me.waited, now - r.made);
        if (!accepts(me.choice, regionOf(r.loc), ms, waited) || !accepts(choiceOf(r.loc), mine, ms, waited)) {
            continue;
        }
        if (me.choice === null || me.choice === "*") {
            return r;
        }
        const s = score(me.place, r.loc);
        if (best === null || s < bestScore) {
            best = r;
            bestScore = s;
        }
    }
    return best;
}

// Ranked: of the searches within the rating window (oldest first: {loc, made}), the best for one whose place is
// `place` and which has waited `waited` seconds, or null. A search without a place (made before this, or Cloudflare
// said nothing) is taken as before: at once, the oldest first, so nothing waits longer than it did.
export function pickRanked(rows, place, waited, now) {
    let best = null, bestScore = Infinity;
    for (const r of rows) {
        const known = typeof r.loc === "string" && r.loc !== "" && typeof place === "string" && place !== "";
        if (!known) {
            if (best === null) {
                return r;
            }
            continue;
        }
        const ms = estimateMs(place, r.loc);
        if (ms > maxMsAfter(Math.max(waited, now - r.made))) {
            continue;
        }
        const s = score(place, r.loc);
        if (best === null || s < bestScore) {
            best = r;
            bestScore = s;
        }
    }
    return best;
}
