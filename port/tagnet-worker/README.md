# Tag Team Online: the rooms of the Tag Team mod over the internet

A Cloudflare Worker (`worker.js`) with a D1 database (`schema.sql`). It gives a room a six-digit code, tells the
players in it each other's addresses and gives the room's owner a login for Cloudflare's TURN relay when a player
cannot be reached directly. The game itself goes between the phones over UDP (`port/src/gs/tag_link.c`); the
Worker never carries it. It is separate from Dragon Net (`port/dragonnet`): nothing there changes.

The game's side: hosting a room in the mod's online menu opens it on the network and here (its code is shown under
the room's title); joining takes a code (six digits) or an address. The service's address is built in
(`TAG_ONLINE_URL`, `port/src/gs/tag_link.h`); a file `tag_online_url.txt` (one line) in the game's data folder names
another.

## The service in use

`https://tagteam-online.regulusplay.workers.dev` (`TAG_ONLINE_URL`), put up on 2026-10-29 with wrangler
(`wrangler.example.jsonc`, the D1 database `tagteam-online` and the Durable Object `TagRelay`). No TURN key: players
who cannot reach each other directly go through the room's relay (`/v1/ws`), about 140-160 ms one way from Brazil.
To put a new `worker.js` up: `wrangler login`, then `wrangler deploy --keep-vars` with the config filled in (the
database id: `wrangler d1 list`). The dashboard's editor can replace the code too, but cannot create the Durable
Object's class (it needs the migration in the config).

## Putting it up (Cloudflare dashboard)

1. **Workers & Pages > Create > Worker**, name it (e.g. `tagteam-online`), **Deploy**, then **Edit code**: replace
   everything with `worker.js`, **Deploy**.
2. **Storage & Databases > D1 > Create**, a database (e.g. `tagteam-online`). Its **Console**: paste `schema.sql`, run.
3. The Worker's **Settings > Bindings > Add > D1 database**: variable name `DB`, the database of step 2.
4. Optional, a TURN relay as well (the room's own relay, the Durable Object, needs none):
   **Realtime > TURN Server > Create**: copy the **Key ID** and the **API token**. The Worker's
   **Settings > Variables and Secrets**: secret `TURN_KEY_ID` = the Key ID, secret `TURN_KEY_TOKEN` = the token.
5. Optional variables: `ALLOWED_VERSIONS` = `5` (the mod's network version), `TURN_DAILY_MAX` (relay logins a day,
   default 300), `MAX_MEMBERS` (default 19).
6. The Worker's address (`https://tagteam-online.<your account>.workers.dev`) goes into `TAG_ONLINE_URL`.

With wrangler instead: `wrangler.example.jsonc`.

## Tests

`node test.mjs` (Node 22+, `node:sqlite`; nothing is sent anywhere).

---

**Resumo (PT):** Worker da Cloudflare com banco D1 que dá às salas do Tag Team um código de 6 dígitos, troca os
endereços dos jogadores e entrega ao dono da sala um login do relay TURN quando a conexão direta falha. A luta vai
direto entre os celulares por UDP; o Worker nunca a carrega. Para publicar: criar o Worker com `worker.js`, criar o
banco D1 e rodar `schema.sql` no console, ligar o banco ao Worker com o nome `DB`, criar uma chave TURN (Realtime >
TURN Server) e pôr `TURN_KEY_ID` e `TURN_KEY_TOKEN` como segredos do Worker. O endereço do Worker vai em `TAG_ONLINE_URL`.
