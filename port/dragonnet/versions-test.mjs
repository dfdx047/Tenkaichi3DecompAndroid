// The games' versions against the service's documented settings: every `v` today's games send must be taken by a
// Worker set up as wrangler-ranked.example.jsonc (and the Ranked authority as wrangler-ranked-authority.example.jsonc)
// says, an older game's version still too, and anything else refused. The versions are read from the games' sources,
// so a new NET_VERSION or DRNET_SIM that the settings do not list fails here. node versions-test.mjs
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import fs from 'node:fs';
import {newDatabase} from './new-database.mjs';
import {rankedFetch} from './ranked.js';
import {chatFetch} from './chat.js';
import {presenceFetch} from './presence.js';
import service from './worker.js';

const read = (rel) => fs.readFileSync(new URL(rel, import.meta.url), 'utf8');
const number = (text, re, what) => { const m = text.match(re); assert.ok(m, 'not found: ' + what); return Number(m[1]); };
// BT3 (Dragon Rage): rooms, quick match, Ranked, chat and presence all say NET_VERSION * 1000 + DRNET_SIM (net_dragon.c)
const bt3 = number(read('../src/gs/net.c'), /NET_VERSION = (\d+)/, 'NET_VERSION') * 1000 +
            number(read('../src/gs/net_dragon.h'), /#define DRNET_SIM (\d+)/, 'DRNET_SIM');
// BT4 (Dragon Rage): its 1 vs 1 rooms and quick match on Dragon Net say DRNET_BT4_VERSION (its Ranked has a service of its own)
const bt4 = number(read('../../bt4/port/src/gs/net_match.c'), /#define DRNET_BT4_VERSION (\d+)/, 'DRNET_BT4_VERSION');
const allowed = (file) => { const m = read(file).match(/"ALLOWED_VERSIONS":\s*"([0-9,]+)"/); assert.ok(m, file); return m[1]; };
const worker = allowed('./wrangler-ranked.example.jsonc'), authority = allowed('./wrangler-ranked-authority.example.jsonc');
console.log(`games: BT3 ${bt3}, BT4 ${bt4}; Worker ALLOWED_VERSIONS "${worker}", Ranked authority "${authority}"`);

// The client asks the Ranked authority's capabilities with its own version, not a fixed one (net_match.c).
assert.ok(!/\\"v\\":5001,\\"cd\\":1/.test(read('../src/gs/net_match.c')), 'net_match.c asks the capabilities with a fixed version');

const sql = newDatabase(DatabaseSync);
const DB = {prepare(text){return {values:[],bind(...v){return {...this,values:v};},async first(){return sql.prepare(text).get(...this.values)??null;},async run(){const r=sql.prepare(text).run(...this.values);return {meta:{changes:Number(r.changes)}};},async all(){return {results:sql.prepare(text).all(...this.values)};}};},
    async batch(list){const out=[];for(const s of list)out.push(await s.run());return out;}};
const env = {DB, RANKED_ENABLED: '1', CHAT_ENABLED: '1', ALLOWED_VERSIONS: worker};
let now = 1800000000, ip = 0;
const nextIp = () => '198.51.100.' + (++ip);

async function ranked(v) {
    const reg = await rankedFetch(null, env, 'register', {v, name: 'P' + v}, now, nextIp());
    if (reg.status !== 200) return reg.status;
    const p = await reg.json();
    const q = await rankedFetch(null, env, 'queue', {v, id: p.id, token: p.token, rules: 'solo-inf-v1'}, now, nextIp());
    if (q.status !== 200) return q.status;
    const chat = await chatFetch(env, {v, id: p.id, token: p.token, after: 0}, now);
    const presence = await presenceFetch(env, {v, id: p.id.slice(0, 32)}, now, nextIp());
    return chat.status === 200 && presence.status === 200 ? 200 : Math.max(chat.status, presence.status);
}
async function rooms(v) {
    const r = await service.fetch(new Request('https://t/v1/queues', {method: 'POST', body: JSON.stringify({v}), headers: {'cf-connecting-ip': nextIp()}}), env);
    return r.status;
}

// today's games: Ranked, chat, presence and the rooms' queues
assert.equal(await ranked(bt3), 200, `BT3's ${bt3}: Ranked, chat and presence`);
assert.equal(await rooms(bt3), 200, `BT3's ${bt3}: rooms`);
assert.equal(await rooms(bt4), 200, `BT4's ${bt4}: rooms`);
// older games still served: DBT-NX before the merge (40001) and Dragon Rage before 0.1.39's netcode (5001)
for (const v of [40001, 5001]) {
    assert.equal(await ranked(v), 200, `older ${v}: Ranked, chat and presence`);
    assert.equal(await rooms(v), 200, `older ${v}: rooms`);
}
// anything else is refused
for (const v of [4, 0, -1, 5000, 99999, '6001', null]) {
    assert.equal(await ranked(v), 409, `version ${JSON.stringify(v)} refused by Ranked`);
    assert.equal(await rooms(v), 409, `version ${JSON.stringify(v)} refused by the rooms`);
}
// the Ranked authority (Ranked C/D) takes today's BT3 games and refuses others
{
    const {default: auth} = await import('./ranked_authority.js');
    const authEnv = {DB, AUTHORITY_ID: 'test-authority-unique-00001', URL_C: 'https://c.invalid', URL_D: 'https://d.invalid',
        MIGRATION_COMPLETE: '1', ALLOWED_VERSIONS: authority, GATEWAY_C_KEY: 'c'.repeat(64), GATEWAY_D_KEY: 'd'.repeat(64)};
    // (as a gateway calls it: ranked_gateway.js's dispatch, gateway C, with its key; the routes' tables of ranked-cd-schema.sql)
    sql.exec(read('./ranked-cd-schema.sql'));
    const ask = async (v) => (await auth.fetch(new Request('https://authority.invalid/_ranked/dispatch', {method: 'POST',
        headers: {'content-type': 'application/json', authorization: 'Bearer ' + authEnv.GATEWAY_C_KEY},
        body: JSON.stringify({gateway: 'C', path: 'ranked/capabilities', body: {v, cd: 1}, ip: nextIp()})}), authEnv)).status;
    assert.equal(await ask(bt3), 200, `the Ranked authority's capabilities for BT3's ${bt3}`);
    assert.equal(await ask(4), 409, 'the Ranked authority refuses another version');
}
console.log('PASS versions: today\'s games (BT3 ' + bt3 + ', BT4 ' + bt4 + ') and the older ones are taken by the documented settings, others refused');
