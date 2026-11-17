# BT4 Android — Ranked: temporadas e perfil permanente (especificação)

Estado (17/11): **implementado e testado localmente (worker), NÃO publicado**. Decisões do dono: temporadas **semanais**; medalhas **só no Ranked**; partida + 3 revanches por dia contra o mesmo rival. Vale para o serviço do Android (`port/bt4online-worker`, Cloudflare Worker + D1
`bt4-online`) e para o cliente (`port/src/gs/bt4_profile.c`, `src/menu/port_tbo.c`). O Ranked do PC (servidor do NauTaro,
`docs/ONLINE_RANKED_DESIGN.md`) é outro sistema e não muda.

Princípio: **a temporada reinicia, a história do jogador nunca é apagada.**

---

## 1. Como é hoje (verificado no código, `profile.js`)

| Peça | Hoje |
|---|---|
| Temporada | Não existe tabela. "Temporada" é a **semana**, calculada pela data (`seasonOf(ms)`), virando segunda 00:00 de Brasília. Sem nome nem status. |
| Elo | `season_ratings (season, player, rating, wins, losses, updated)`: começa em 1000 **toda semana**, K 32, mínimo 100. |
| Medalhas | `players.gold / silver / wins / losses`: **um único total da vida toda**, sem separação por temporada. Ouro para quem vence, prata para quem perde, em **qualquer** batalha online válida (normal, revanche e Ranked). |
| Troféus | `trophies (season, place, player, rating)`: os 3 primeiros da semana (com ≥5 partidas pontuadas contra ≥3 rivais). |
| Fechamento | `closeSeasons()` roda "preguiçoso", na primeira requisição depois da virada. Grava os troféus e apaga `reports/settled` com mais de 7 dias. |
| Partidas | **Não há histórico permanente.** `reports` e `settled` são apagados em 7 dias. A tabela `ranked_matches` existente **não é histórico**: é o registro das salas que a fila D20 montou (`rooms.js` linha 817), também apagado em 7 dias. |
| Anti-duplicidade | `settled (first, second)` com `INSERT OR IGNORE` reserva o par de relatos, e só então as atualizações rodam num `batch`. |
| Cliente | `/v1/me` devolve `gold, silver, wins, losses` (vida toda), `rating, rwins, rlosses, rank` (semana), `trophies`, `season, ends, last`. `/v1/top` devolve o ranking da semana atual. |

Problemas em relação às regras novas:

1. Não há medalhas **da temporada**: hoje só existe o total.
2. Não há `season_history`. A posição final de cada jogador não fica guardada (só o top 3, como troféu).
3. Não há histórico permanente de partidas.
4. O ranking de uma semana encerrada pode ser recalculado a partir de `season_ratings`, mas nenhuma API o expõe.
5. A **reserva** (`settled`) e a **gravação** ficam em duas etapas. Se a segunda falhar, a partida fica marcada e não conta para ninguém. Isso não duplica, mas perde a partida.
6. Elo lido e gravado em momentos diferentes: duas partidas do mesmo jogador fechando juntas podem usar um rating velho e errar o Elo em alguns pontos.
7. O nome `ranked_matches` já está ocupado pelo registro de salas.

---

## 2. Modelo de dados

Banco D1 (SQLite). O tempo fica em milissegundos UTC, como hoje. As tabelas novas são criadas com `CREATE TABLE IF NOT EXISTS`, como o
`schema()` já faz.

### 2.1 `seasons`: uma linha por temporada

```sql
CREATE TABLE seasons (
  id          INTEGER PRIMARY KEY,           -- 1, 2, 3 ... (nunca reutilizado)
  slug        TEXT NOT NULL UNIQUE,          -- 'S01'
  name        TEXT NOT NULL,                 -- 'Temporada 01 — Saga dos Saiyajins'
  starts_at   INTEGER NOT NULL,
  ends_at     INTEGER NOT NULL,
  status      TEXT NOT NULL CHECK (status IN ('scheduled','active','closing','closed')),
  closed_at   INTEGER,
  players     INTEGER NOT NULL DEFAULT 0,    -- quantos terminaram com partida pontuada (preenchido ao fechar)
  CHECK (ends_at > starts_at)
);
CREATE UNIQUE INDEX seasons_one_active ON seasons (status) WHERE status = 'active';  -- no máximo UMA ativa
CREATE INDEX seasons_time ON seasons (starts_at);
```

As temporadas são criadas **por script** (`wrangler d1 execute`, ver §6). Não haverá endpoint administrativo exposto na
internet.

### 2.2 `season_stats`: a temporada em andamento (e as passadas, intactas)

```sql
CREATE TABLE season_stats (
  season_id    INTEGER NOT NULL REFERENCES seasons(id),
  player_id    TEXT    NOT NULL REFERENCES players(id),
  rating       INTEGER NOT NULL DEFAULT 1000,
  peak_rating  INTEGER NOT NULL DEFAULT 1000,
  gold         INTEGER NOT NULL DEFAULT 0,   -- medalhas desta temporada
  silver       INTEGER NOT NULL DEFAULT 0,
  wins         INTEGER NOT NULL DEFAULT 0,   -- vitórias Ranked pontuadas desta temporada
  losses       INTEGER NOT NULL DEFAULT 0,
  matches      INTEGER NOT NULL DEFAULT 0,   -- partidas Ranked pontuadas
  updated      INTEGER NOT NULL,
  PRIMARY KEY (season_id, player_id)
);
CREATE INDEX season_stats_rank ON season_stats (season_id, rating DESC, wins DESC, updated);
```

**O "reset" não apaga nada.** A temporada nova é outro `season_id`. A primeira partida do jogador nela cria a linha com
os padrões (1000 / 0 / 0 / 0). As linhas das temporadas passadas ficam como estão, e por isso o ranking delas continua consultável
(regra 3).

### 2.3 `career_stats`: o perfil permanente

```sql
CREATE TABLE career_stats (
  player_id           TEXT PRIMARY KEY REFERENCES players(id),
  gold                INTEGER NOT NULL DEFAULT 0,  -- soma de todas as temporadas
  silver              INTEGER NOT NULL DEFAULT 0,
  wins                INTEGER NOT NULL DEFAULT 0,  -- vitórias da carreira
  losses              INTEGER NOT NULL DEFAULT 0,
  ranked_matches      INTEGER NOT NULL DEFAULT 0,  -- partidas Ranked pontuadas da carreira
  best_rating         INTEGER NOT NULL DEFAULT 1000,
  best_rating_season  INTEGER,                     -- em qual temporada
  seasons_played      INTEGER NOT NULL DEFAULT 0,  -- preenchido ao fechar cada temporada
  updated             INTEGER NOT NULL
);
```

`career_stats` é atualizado **na mesma transação** que `season_stats` (§3), partida a partida. Nunca é recalculado a
partir das temporadas e nunca é zerado. Por isso as medalhas antigas não são contadas de novo (regra 2).

### 2.4 `season_history`: o resultado final congelado de cada temporada

```sql
CREATE TABLE season_history (
  season_id     INTEGER NOT NULL REFERENCES seasons(id),
  player_id     TEXT    NOT NULL REFERENCES players(id),
  final_rank    INTEGER,          -- posição final (NULL: não se classificou, ver regra de elegibilidade)
  final_rating  INTEGER NOT NULL,
  peak_rating   INTEGER NOT NULL,
  gold          INTEGER NOT NULL,
  silver        INTEGER NOT NULL,
  wins          INTEGER NOT NULL,
  losses        INTEGER NOT NULL,
  matches       INTEGER NOT NULL,
  trophy        INTEGER,          -- 1 ouro, 2 prata, 3 bronze, NULL nenhum
  name_then     TEXT NOT NULL,    -- o nome do jogador no fim da temporada (ele pode trocar depois)
  PRIMARY KEY (season_id, player_id)
);
CREATE INDEX season_history_player ON season_history (player_id, season_id);
CREATE INDEX season_history_rank   ON season_history (season_id, final_rank);
```

É escrita **uma vez**, ao fechar a temporada, e nunca mais alterada. O histórico navegável do perfil e o ranking das
temporadas encerradas são lidos daqui.

### 2.5 `ranked_matches`: histórico individual das partidas

O nome que você pediu já é usado pelo registro de salas. Proposta: renomear o registro de salas para `ranked_rooms`
(tabela de vida curta, mudança de 2 linhas em `rooms.js` e em `rankedRoom()`) e usar `ranked_matches` para o histórico:

```sql
ALTER TABLE ranked_matches RENAME TO ranked_rooms;   -- migração (o conteúdo é de no máximo 7 dias)

CREATE TABLE ranked_matches (
  id             INTEGER PRIMARY KEY AUTOINCREMENT,
  match_key      TEXT NOT NULL UNIQUE,     -- '<rid menor>:<rid maior>' dos dois relatos: a trava contra duplicidade
  season_id      INTEGER NOT NULL REFERENCES seasons(id),
  played_at      INTEGER NOT NULL,
  room_code      TEXT NOT NULL,
  battle_no      INTEGER NOT NULL,
  kind           TEXT NOT NULL CHECK (kind IN ('ranked','casual')),
  rated          INTEGER NOT NULL,         -- 1 = mexeu no Elo
  medals         INTEGER NOT NULL,         -- 1 = deu medalhas (limite diário por dupla)
  winner_id      TEXT NOT NULL,
  loser_id       TEXT NOT NULL,
  winner_before  INTEGER, winner_after INTEGER,   -- ratings (NULL se não pontuou)
  loser_before   INTEGER, loser_after  INTEGER
);
CREATE INDEX ranked_matches_winner ON ranked_matches (winner_id, played_at DESC);
CREATE INDEX ranked_matches_loser  ON ranked_matches (loser_id,  played_at DESC);
CREATE INDEX ranked_matches_season ON ranked_matches (season_id);
```

As batalhas casuais também aparecem aqui (`kind = 'casual'`), porque hoje elas dão medalhas e as medalhas precisam de rastro.

### 2.6 Tabelas que continuam e o que muda nelas

| Tabela | Destino |
|---|---|
| `players` | Continua sendo a identidade (nome, segredo). As colunas `gold/silver/wins/losses` **deixam de ser escritas** depois da migração. A verdade passa a ser `career_stats`. Não são apagadas, para quem quiser conferir. |
| `reports`, `settled`, `pair_day` | Continuam (concordância dos dois relatos e limites por dupla por dia). `settled` passa a ser gravada dentro da transação. |
| `season_opponents` | Continua, com `season` → `season_id` (elegibilidade: rivais diferentes). |
| `season_ratings`, `trophies` (semanais) | Congeladas como **legado da pré-temporada** (ver §6). Ninguém mais escreve nelas. |
| `guard` (nova) | `CREATE TABLE guard (v INTEGER NOT NULL)`: fica sempre vazia e serve para **abortar** uma transação (§3). |

---

## 3. Fluxo: fim de uma partida válida

O começo não muda: os dois jogadores relatam e o servidor junta os dois relatos (mesma sala, número da batalha, nomes, lados
opostos e vencedor, dentro de 15 min). O que muda é o fechamento (`settle()`):

```
settle(relatoA, relatoB):
  1. LEITURAS (fora da transação)
     season   ← SELECT id FROM seasons WHERE status='active' AND starts_at <= agora < ends_at
                (nenhuma ativa → resposta "waiting"; o jogo tenta de novo depois, como já faz)
     par      ← pair_day de hoje (limites de medalhas e de partidas pontuadas por dupla)
     ranked   ← a fila D20 montou esta sala com estes 2 nomes (ranked_rooms)
     rW, rL   ← season_stats.rating dos dois nesta temporada (1000 se ainda não há linha)
  2. CÁLCULO
     medals = par.battles < 20 ; rated = ranked && par.rated < 3
     ganho = elo(rW, rL) ; perda = min(ganho, rL - 100)
  3. UMA TRANSAÇÃO (env.DB.batch = tudo ou nada)
     a. INSERT INTO settled (first, second, at)                     -- sem OR IGNORE: repetida → ERRO → nada é gravado
     b. INSERT INTO ranked_matches (match_key, ...)                 -- UNIQUE(match_key): segunda trava
     c. INSERT INTO guard SELECT NULL WHERE NOT EXISTS
          (SELECT 1 FROM seasons WHERE id=:season AND status='active')        -- temporada fechou no meio → aborta
     d. INSERT INTO guard SELECT NULL WHERE
          COALESCE((SELECT rating FROM season_stats WHERE season_id=:s AND player_id=:W),1000) <> :rW
          OR COALESCE((SELECT rating ... :L),1000) <> :rL                     -- rating mudou desde a leitura → aborta
     e. pair_day: battles+1, rated+(rated?1:0)
     f. season_stats (vencedor e perdedor): INSERT ... ON CONFLICT DO UPDATE
          gold+=medals, silver+=medals, wins/losses/matches/rating (se rated), peak_rating=MAX(peak, novo)
     g. career_stats (vencedor e perdedor): INSERT ... ON CONFLICT DO UPDATE
          gold+=medals, silver+=medals, wins/losses+=medals, ranked_matches+=rated,
          best_rating=MAX(best, novo), best_rating_season=(se subiu, :season)
     h. season_opponents (se rated) ; reports.outcome dos dois
  4. SE A TRANSAÇÃO FALHAR
     - por (a)/(b) "UNIQUE": a partida já foi processada → só devolve o resultado gravado (idempotente)
     - por (d) rating mudou: volta ao passo 1 (no máximo 3 vezes)
     - por (c) temporada fechando: devolve "waiting"; o próximo relato do jogo cai na temporada nova
```

Por que isso cumpre as regras:

- **Atômico:** temporada e carreira são gravadas no mesmo `batch`. O D1 executa um batch como transação única: ou tudo
  entra, ou nada entra. Isso também corrige o problema 5 de hoje (partida perdida entre reserva e gravação).
- **Sem duplicidade:** duas travas UNIQUE (`settled` e `ranked_matches.match_key`) dentro da mesma transação. Uma
  repetição (o jogo reenvia, dois pedidos simultâneos) aborta a transação inteira (regra 5).
- **Elo exato:** a trava (d) desfaz a gravação se outro resultado mudou o rating entre a leitura e a escrita. Isso corrige o
  problema 6 de hoje.
- `guard` é uma tabela com coluna `NOT NULL`. Inserir `NULL` nela gera erro de propósito e desfaz o batch inteiro. É a
  forma de "abortar se..." no SQLite sem procedimentos.

Regra de contagem: a partida conta na **temporada ativa no momento do fechamento** (quando o segundo relato chega), como
hoje conta na semana do fechamento.

---

## 4. Fluxo: encerrar uma temporada e abrir a próxima

O disparo acontece de duas formas, e as duas chamam a mesma função idempotente:

- um **Cron Trigger** do Worker (os Cron Triggers são gratuitos na Cloudflare), a cada 15 minutos;
- o caminho "preguiçoso" de hoje, na primeira requisição depois de `ends_at`, como reserva.

```
closeSeason(agora):
  s ← SELECT * FROM seasons WHERE status IN ('active','closing') AND ends_at <= agora
  se não há: retorna
  1. RESERVA: UPDATE seasons SET status='closing', closed_at=agora WHERE id=s.id
              AND (status='active' OR (status='closing' AND closed_at < agora - 10 min))
     changes ≠ 1 → outro pedido já está fechando: retorna
     (closed_at guarda a hora da reserva enquanto 'closing'; uma reserva de 10 min sem terminar pode ser retomada)
     (a partir daqui o passo 3c do settle recusa partidas desta temporada)
  2. UMA TRANSAÇÃO (arquivar ANTES de abrir a próxima)
     a. INSERT INTO season_history (...)
          SELECT ..., CASE WHEN elegível THEN ROW_NUMBER() OVER (ORDER BY rating DESC, wins DESC, updated) END ...
          FROM season_stats WHERE season_id = s.id
          (elegível = ≥5 partidas pontuadas contra ≥3 rivais, a regra dos troféus de hoje;
           os não elegíveis ficam com final_rank NULL, mas com medalhas e números guardados)
     b. UPDATE season_history SET trophy = final_rank WHERE season_id = s.id AND final_rank <= 3
     c. UPDATE career_stats SET seasons_played = seasons_played + 1
          WHERE player_id IN (SELECT player_id FROM season_stats WHERE season_id = s.id)
     d. UPDATE seasons SET status='closed', closed_at=agora, players=(contagem) WHERE id=s.id
     e. UPDATE seasons SET status='active' WHERE id = (a próxima 'scheduled' com starts_at <= agora)
  3. Se a transação falhar: a temporada fica 'closing' e a próxima tentativa (cron ou requisição) a retoma depois de 10 min;
     nada fica pela metade, porque o arquivamento e a abertura da próxima são da mesma transação.
```

- A posição final usa `ROW_NUMBER()` em **uma** instrução. Não há laço por jogador.
- O desempate é o mesmo de hoje: mais pontos, depois mais vitórias, depois quem chegou primeiro.
- Se não houver próxima temporada `scheduled`, fica **sem temporada ativa**. Partidas casuais continuam dando medalhas
  (vão só para `career_stats`, com `season_id` da última temporada no histórico de partidas), e o Ranked mostra
  "Nova temporada em breve" e não pontua.

---

## 5. API (compatível com os jogos já instalados)

Os campos que o jogo lê hoje continuam com **o mesmo nome e o mesmo sentido**. Só entram campos novos:

| Rota | Mudança |
|---|---|
| `POST /v1/me` | Continua igual: `gold, silver, wins, losses` (= carreira, como hoje), `rating, rwins, rlosses, rank, players` (= temporada ativa), `trophies`, `ends`. **Novos:** `season_id, season_name, season_starts`, `season_gold, season_silver`, `best_rating, best_rating_season, ranked_total, seasons_played`. O campo `season` passa a ser o `id` da temporada (o jogo atual só o mostra). |
| `POST /v1/top` | Sem parâmetro: igual, da temporada ativa. **Novo** `{season: id}`: ranking de uma temporada encerrada, lido de `season_history`. |
| `POST /v1/seasons` | **Nova:** lista `[id, slug, name, starts, ends, status, players]`, as mais recentes primeiro, em páginas. |
| `POST /v1/history` | **Nova** (`id, secret, page`): as temporadas do jogador (`season_history`), mais recente primeiro, 10 por página. |
| `POST /v1/matches` | **Nova** (`id, secret, before`): as últimas partidas do jogador, 20 por página, por `played_at`. |

---

## 6. Migração (uma vez, sem perder nada)

1. Criar as tabelas novas e renomear `ranked_matches` → `ranked_rooms`. O `rooms.js` e o `profile.js` passam a usar o
   nome novo **no mesmo deploy**.
2. Inserir a **Temporada 01** como `active`, começando no momento do deploy, com o nome e a data de fim que você escolher.
3. `career_stats` ← `players.gold, silver, wins, losses`: as medalhas de hoje viram o total da carreira, sem perda.
   `best_rating` ← o maior rating já visto em `season_ratings`. `ranked_matches` (carreira) ← soma de `wins + losses`
   de `season_ratings`.
4. As semanas já jogadas viram a **pré-temporada**:
   - `season_ratings` e `trophies` ficam congeladas;
   - os troféus semanais continuam aparecendo no perfil como "Troféus da pré-temporada";
   - **não** são convertidas em `season_history`, porque cada semana teria virado uma "temporada" de 7 dias.
5. Script de verificação, rodado antes e depois da migração: para cada jogador,
   `career_stats.gold = players.gold` (e o mesmo para prata, vitórias e derrotas).

Para criar as próximas temporadas basta uma linha de SQL (`wrangler d1 execute bt4-online --remote`):

```sql
INSERT INTO seasons (id, slug, name, starts_at, ends_at, status)
VALUES (2, 'S02', 'Temporada 02', <início ms>, <fim ms>, 'scheduled');
```

---

## 7. Interface (cliente BT4 Android)

| Onde | O que mostra | De onde vem |
|---|---|---|
| Tela **Ranked** | rating Elo atual, posição atual (e de quantos), **ouro e prata da temporada**, nome da temporada e tempo restante | `/v1/me`: `rating, rank, players, season_gold, season_silver, season_name, ends` |
| **Perfil** | ouro e prata **da carreira**, vitórias e derrotas da carreira, partidas Ranked, maior rating (e em qual temporada) | `/v1/me`: `gold, silver, wins, losses, ranked_total, best_rating, best_rating_season` |
| **Perfil → Temporadas** (página nova) | lista navegável: nome, posição final, rating final, ouro/prata, V/D, troféu | `/v1/history` |
| **Top** | aba da temporada atual e seletor de temporadas encerradas | `/v1/top`, `/v1/seasons` |

Arquivos do cliente:

- `port/src/gs/bt4_profile.c` e `.h`: campos novos em `Bt4ProfileStats` e leitura de `/v1/history`, `/v1/seasons` e `/v1/top {season}`, na mesma thread de rede de hoje.
- `src/menu/port_tbo.c`: tela Ranked, Perfil e página nova de temporadas.
- `port/translations/port/pt-BR.json` (e es): os textos novos.

---

## 8. Componentes a criar ou alterar

| Componente | Arquivo | Tipo |
|---|---|---|
| Esquema e migração | `bt4online-worker/schema.sql`, `profile.js schema()`, `migrate-seasons.sql` (novo) | novo + alteração |
| Temporadas (fechar/abrir, listar) | `bt4online-worker/seasons.js` (novo) | novo módulo |
| Fechamento de partida atômico | `profile.js settle()` | reescrita da função |
| Histórico, ranking passado, partidas | `profile.js` (rotas novas) | novo |
| Cron | `worker.js` (`scheduled()`), `wrangler.jsonc` (`triggers.crons`) | alteração pequena |
| Salas da fila Ranked | `rooms.js` (`ranked_matches` → `ranked_rooms`) | 2 linhas |
| Testes | `test.mjs` (novos casos, §9) | novo |
| Cliente | `bt4_profile.c/.h`, `port_tbo.c`, traduções | alteração |

Fica de fora (não muda): salas, fila, Partida Rápida, concordância dos relatos, limites por dupla, anti-flood do
registro e presença.

---

## 9. Testes obrigatórios antes do deploy (`test.mjs`, `wrangler dev --local`)

1. **Exemplo da especificação:** fechar a T01 com 82 ouro e 61 prata, jogar a T02 com 95 e 70. O perfil mostra 177 e 131,
   e a tela Ranked mostra 95 e 70.
2. A T02 começa com rating 1000 e todos os contadores da temporada zerados. O ranking da T01 continua igual via `/v1/top {season:1}`.
3. O mesmo relato enviado 2 vezes, e dois relatos simultâneos: medalhas e Elo contam **uma** vez.
4. Fechamento chamado 2 vezes ao mesmo tempo (cron e requisição): um único `season_history`, com troféus únicos.
5. Uma partida fechando durante o `closing` não entra na temporada velha e conta depois na nova.
6. Três temporadas seguidas: `career_stats` = soma de `season_history` mais as partidas da temporada ativa (verificação de
   consistência, que também pode rodar no cron como alarme).
7. Partida casual: medalha na temporada e na carreira, sem Elo.
8. Os 19 testes atuais continuam passando.

---

## 10. Limites e escala (plano gratuito da Cloudflare)

- D1 grátis: 5 GB, 5 milhões de linhas lidas e 100 mil linhas escritas por dia. Uma partida grava cerca de 12 linhas,
  o que dá **~8 mil partidas por dia** antes de bater no limite de escrita. Acima disso será preciso o plano pago ou
  agrupar escritas. Hoje o volume é muito menor.
- Posição no ranking: `COUNT` sobre o índice `season_stats_rank`, rápido até dezenas de milhares de jogadores por
  temporada. Acima disso, guardar a posição calculada a cada 5 minutos pelo cron.
- Espaço: cada linha de `season_stats` ou `season_history` tem cerca de 100 bytes. Com 100 mil jogadores × 50 temporadas,
  dá cerca de 1 GB.
- Pronto para crescer depois: uma coluna `mode` ('solo'/'team') na chave de `season_stats`/`season_history` separa filas
  futuras sem refazer o modelo. Hoje o Android tem uma fila só.

---

## 11. Decisões em aberto (precisam da sua escolha antes de implementar)

1. **Duração da temporada.** Hoje o Elo zera **toda semana** e os troféus são semanais. Com temporadas nomeadas, a
   recomendação é que **a temporada substitua a semana**: Elo, ranking e troféus por temporada (ex.: mensal). Manter as
   duas coisas (semanas dentro de temporadas) é possível, mas o Elo semanal entraria em conflito com "Elo zera só no fim
   da temporada".
2. **Quais batalhas dão medalhas da temporada.** Hoje qualquer batalha online válida dá medalha. A recomendação é manter
   (medalha conta na temporada e na carreira) e mostrar as da temporada na tela Ranked. A alternativa é contar medalha só
   em partidas Ranked.
3. **Vitórias e derrotas da carreira.** A recomendação é manter o sentido de hoje (todas as batalhas válidas) e mostrar à
   parte as vitórias e derrotas **Ranked** da carreira.
4. **Semanas já jogadas:** pré-temporada congelada (recomendado) ou convertidas em temporadas curtas.
5. **Nome da tabela:** renomear o registro de salas para `ranked_rooms` (recomendado) para usar `ranked_matches` no histórico.
