// BT4 Android online service (Dragon Rage): the Ranked rooms (rooms.js, Dragon Net's rooms code) and the players'
// profiles, medals, weekly Ranked seasons and trophies (profile.js), one Worker and one D1 database (binding DB).
// The game: port/src/gs/net_match.c (the Ranked queue's rooms) and port/src/gs/bt4_profile.c (the profile).
import rooms from "./rooms.js";
import { PROFILE_PATHS, profileFetch, schema } from "./profile.js";

let roomsReady = false;
async function roomsSchema(env) {
    if (roomsReady) return;
    await env.DB.batch([
        env.DB.prepare("CREATE TABLE IF NOT EXISTS rooms (code TEXT PRIMARY KEY, made INTEGER NOT NULL, version INTEGER NOT NULL, ip TEXT NOT NULL, host_key TEXT NOT NULL, join_key TEXT, host_name TEXT NOT NULL, join_name TEXT, host_addrs TEXT NOT NULL, join_addrs TEXT, queue TEXT, who TEXT, lobby INTEGER, loc TEXT, join_region TEXT)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS watchers (key TEXT PRIMARY KEY, code TEXT NOT NULL, made INTEGER NOT NULL, name TEXT NOT NULL, addrs TEXT NOT NULL)"),
        env.DB.prepare("CREATE TABLE IF NOT EXISTS used (what TEXT NOT NULL, slot INTEGER NOT NULL, n INTEGER NOT NULL, PRIMARY KEY (what, slot))"),
    ]);
    await schema(env); // (ranked_matches, which rooms.js writes, is the profile's)
    roomsReady = true;
}

export default {
    async fetch(request, env, ctx) {
        const url = new URL(request.url);
        if (request.method === "GET" && url.pathname === "/v1/health") {
            return new Response(JSON.stringify({ ok: true }), { headers: { "content-type": "application/json" } });
        }
        if (PROFILE_PATHS.has(url.pathname)) {
            return profileFetch(request, env);
        }
        try {
            await roomsSchema(env);
        } catch (e) {
            return new Response(JSON.stringify({ error: "unavailable" }), { status: 503, headers: { "content-type": "application/json" } });
        }
        return rooms.fetch(request, env, ctx);
    },
};
