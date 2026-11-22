# Ranqueado: identidade conta-sala, limite por par no banco, múltiplas contas (terceira passada, 22/11/2026)

Branch `auditoria-selecao-online`. Sem merge, `NET_VERSION` intacto, Elo/K/forfeit de 16/medalhas/temporadas intactos, `RANKED_ALLOW_CODELESS=0`, `RANKED_MIN_RELEASE` e `RANKED_REQUIRE_ACCOUNT` **desligados**.
**Nada aqui foi testado em aparelho nem com uma partida real.** O cliente foi compilado para Android e não executado.

## 1. Como li a decisão "o SQL é a autoridade"
O Android pontua no Worker (D1/SQLite) e o PC/R5 no Supabase (Postgres): são dois bancos de dados, e eu não os unifiquei nem fiz o Worker liquidar no Postgres (isso seria uma mudança de arquitetura e de operação).
Apliquei a decisão assim: as regras de elegibilidade, limite, unicidade e liquidação ficam **no banco, atomicamente, em cada autoridade**, e as duas são iguais; o Worker continua validando entradas e respondendo ao cliente.
Se a sua intenção era o Worker chamar o Supabase para liquidar, diga: é outra etapa, com chave de serviço, falhas de rede entre os dois e migração de dados.

## 2. Regras agora iguais nas duas camadas (provado por teste de contrato)
| Regra | Worker (D1) | SQL (Postgres) |
|---|---|---|
| 4 batalhas por par por dia (dia de Brasília); a 5ª em diante é gravada e não muda nada | `pair_day` + guarda, já existia | **`ranked_pair_day`, upsert que só sobe abaixo de 4** (migração `20261122000001`) |
| Uma liquidação por luta | **índice único (sala, batalha) + verificação prévia** | uma por `match id` (já existia) |
| Só as contas admitidas relatam | **lugares da sala (`ranked_seats`)** | `player_a`/`player_b` fixados na admissão (já existia) |
| Elo K 32 e ganhos | iguais passo a passo no cenário (7 batalhas, ratings 1016/984 → 1027/973) | `tests/pglite/contract.test.mjs` |
Diferenças que **permanecem** e não toquei: piso do rating (Worker 100, SQL 0), forfeit de 16 (só SQL), contas anônimas do Supabase x contas do Worker.

## 3. Identidade conta-sala
**Admissão (cliente → servidor):** a busca ranqueada (`/v1/quick`, fila `D20`) leva `acct: {id, secret}` (opcional). O servidor valida a conta e a prende ao lugar que a busca recebe: **host** ao criar a sala, **joiner** ao entrar (`ranked_seats`, chave `(code, seat)`).
**Relatos:** com os dois lugares ligados, só essas duas contas relatam (`error: "not admitted"`, nada é gravado); o combate só pontua se as contas que o liquidam são as dos lugares; `RANKED_REQUIRE_ACCOUNT=1` exige os dois ligados.
| Ameaça pedida | Proteção | Teste |
|---|---|---|
| Espectador se passando por jogador (sabe o código e os nomes) | não é um lugar: relato recusado; não cancela nem rouba | I1, I2, I3 |
| Reutilização de credenciais | segredo errado → 401 `account` (nunca rebaixado a anônimo); o segredo anterior a uma recuperação não vale | I5 |
| Partidas antigas / liquidação duplicada | uma luta (sala, batalha) pontua uma vez, também sob corrida; janelas de 15 min/2 h já existentes; o código que se repete recomeça sem lugares | D1, D3 |
| Troca de identidade em reconexão | o host que busca de novo da sua sala não troca a conta do lugar (`403 identity`) | I6 |
| Relatos de contas não admitidas | recusados na entrada | I1, I3 |
| Repetição de requisições | `rkey` idempotente, reivindicação única, índice único | integridade + D1 |
| Mesma conta contra si mesma | duas buscas da mesma conta não são pareadas | I4 |
| Segredo em logs | o servidor nunca grava nem devolve o segredo; o cliente só o põe no corpo | S1 (varre logs e respostas) |

**Limites (o que não está resolvido):** (a) com **um só lado ligado** (o outro é um jogo 139) o lugar solto não se distingue de um estranho; só `RANKED_REQUIRE_ACCOUNT=1`, depois da migração, fecha isso. (b) Quem tem o segredo de uma conta é aquela conta: a recuperação por apelido + código de 80 bits é o que protege. (c) O servidor confia na conta, não verifica que a luta aconteceu (seção 6).
**Cliente (não testado em aparelho):** `bt4_profile.c` `Bt4Profile_QueueAccount` e `net_match.c` `queue_account` (os dois montadores do `quick`); pior caso medido do corpo: 466 + 125 bytes, buffer 768.

## 4. Falhas reproduzidas antes e resultado depois
Antes (o código de `af702f67`, o estado anterior desta etapa): **10 casos** do arquivo `ranked_identity.test.mjs` falhavam: a mesma luta liquidada duas vezes (vencedor 1031 com 2 vitórias), espectador roubando o crédito, espectador cancelando o resultado, conta com o nome do lugar, mesma conta contra si, segredo errado aceito, segredo antigo aceito, troca da conta do lugar, `REQUIRE_ACCOUNT` sem efeito, e o resultado legítimo que dependia disso. Depois: **0**.

## 5. Testes executados (todos passam)
| Suíte | Verificações | O que cobre |
|---|---|---|
| `port/bt4online-worker/tests/ranked_integrity.test.mjs` | 15 | pareamento por código, cancelamento, migração (contador e portão) |
| `.../ranked_identity.test.mjs` | 14 | identidade, unicidade, corrida real de liquidação, segredo em logs |
| `supabase/tests/pglite` SQL (S1-S13) | 31 | liquidação única, disputa, abandono, limite por par, duração, revisão de farm |
| `.../contract.test.mjs` | 8 | Worker x SQL, passo a passo |
| `supabase/tests/bench/cap_race.sh` (Postgres 16 real, Docker) | 3 execuções | 8 sessões, 24 partidas do mesmo par ao mesmo tempo |
| `port/tests/select_flow`, `net_host` (fuzz) | regressão | sem mudança |
**Concorrência (cap_race):** nas três execuções, **exatamente 4 pontuaram** e 20 ficaram `capped`, ratings somando 2000, 8 linhas de histórico, nenhum deadlock e nenhum erro, **com** a trava global e **sem** ela (variante só para teste). Isso mostra que o limite se apoia no upsert do contador, não na trava; **não** prova que remover a trava é seguro (o impasse de ordem oposta de jogadores não apareceu, o que não é prova de que não existe).
Limites da concorrência: no Worker a corrida é a de `await` dentro de um processo com um D1 de teste (não é paralelismo real do Cloudflare); no SQL é um par, uma máquina fraca, sem a camada HTTP.

## 6. Múltiplas contas e uma duração verificável
**O que se mediu:** com o limite de 4 por par por dia, 30 contas falsas levam uma conta a 2241 em uma semana (840 vitórias fabricadas) e 10 a 1661 (`tests/farm_sim.mjs`, `elo()` real). Passar pela fila ranqueada e relatar por HTTP não exige luta.
**Não inventei duração mínima.** Proposta verificável, por etapas:
1. **Medir (feito, só telemetria):** o SQL grava `duration_ms` = do primeiro heartbeat mais tardio até a liquidação, **pelo relógio do servidor**, e a view `ranked_duration_stats` dá os percentis p1/p5/p50/p95 das partidas reais. Nada é imposto.
2. Depois de algumas semanas de dados, fixar `D_min` abaixo do p1 observado (com folga) e **exigir os dois heartbeats** (a prova de presença que o cliente não forja em tempo, só atrasando a chamada) e `liquidação - início ≥ D_min`.
3. No Worker não há heartbeat: precisaria de um por lugar (cliente novo).
**O que isso não resolve:** um farm pode esperar `D_min` por batalha (com 30 contas e 120 batalhas por dia, `D_min = 60 s` custa 2 h sequenciais, ou minutos em paralelo). Ela encarece o farm "por script"; não o impede. Impedi-lo exigiria verificar a luta (reexecutar a simulação, ou atestado do aparelho), fora do escopo.
**Auxílio de revisão (feito, só leitura):** `bt4_private.ranked_farm_review` (migração `…0002`) lista contas com ≥12 vitórias pontuadas em 7 dias cuja ≥80% vem de 3 rivais (teste S13: o farm de 24 vitórias contra 3 contas é sinalizado, 12 vitórias contra 12 rivais não). É pergunta para um humano: amigos que só jogam entre si aparecem igual.
**Outras medidas, que mudam regra e portanto precisam da sua decisão:** teto diário de batalhas pontuadas por conta; ganho decrescente contra o mesmo rival na semana; atrito no registro (limite por aparelho/atestado).

## 7. Migração de versão
`PORT_RELEASE` 139 → 140 (`90616201`); detalhes e o que é operacional em `docs/MIGRACAO-RANQUEADO-140.md`. **Atenção:** o Worker confere uma chave de build **por release**; o 140 precisa estar em `BUILD_KEYS` antes de publicar, senão é recusado se a exigência de chave estiver ligada.

## 8. Commits desta etapa
Identidade: `4f6f320e` (servidor), `32c24471` (cliente). Banco: `63fc0c89` (limite por par e duração), `3dab8a59` (contrato), `08b6f5b8` (revisão de farm). Versão: `90616201`.

## 9. Riscos restantes e o que depende de teste físico
- **Aparelhos:** busca ranqueada entre dois aparelhos com o 140; um 140 contra um 139; o corpo do `quick` com a conta no aparelho; a mensagem nova de recusa; reconexão de rede durante a busca.
- O `capped` do SQL devolve `{"state":"applied","capped":true}` sem `rating`/`delta`: o cliente de PC (Supabase) não foi lido para ver como o exibe.
- Uma base D1 de produção que já tenha duas liquidações pontuadas da mesma luta não aceita o índice único (o código registra e segue com a verificação prévia); a corrida só fica totalmente fechada com o índice.
- Um 139 com um lado ligado e outro não (seção 3); o contador `codeless_ranked_reports` e o `RANKED_MIN_RELEASE` só ajudam depois do 140 publicado.
- A migração SQL `…0001`/`…0002` precisa ser aplicada no Supabase; foi testada em PGlite e Postgres 16, não no projeto real.
