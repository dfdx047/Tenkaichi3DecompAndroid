# Validação do ranqueado: chaves, testes, reversão (quarta passada, 22/11/2026)

Branch `auditoria-selecao-online`. Sem merge, `NET_VERSION` intacto. **Nada foi testado em aparelho nem em rede real:** os testes simulam o lado HTTP dos jogos contra o Worker real e o SQL real.

## 1. Configurações novas: todas desligadas até a validação
| Chave | Camada | Padrão | O que faz quando ligada | Como ligar | Reverter |
|---|---|---|---|---|---|
| `RANKED_ALLOW_CODELESS` | Worker (variável) | **0** (decisão sua, não mudar) | 1 devolveria o coringa de relato sem código (a brecha F1/F2) | — | — |
| `RANKED_MIN_RELEASE` | Worker | **desligado** (0) | recusa na BUSCA ranqueada o jogo com `rel` menor (`old, need N`) | variável do Worker, só depois de o 140 estar publicado, testado e com chave de build | apagar a variável |
| `RANKED_REQUIRE_ACCOUNT` | Worker | **desligado** | o ranqueado exige a conta nos dois lugares (sem isso a batalha vale zero) | só quando todos os jogos mandarem a conta | apagar a variável |
| `ranked_config.pair_day_cap` | SQL | **0** (as regras de antes) | 1..4: batalhas do par por dia que pontuam (4 = a mesma regra do Worker) | `update bt4_private.ranked_config set pair_day_cap=4;` depois de validar | `…=0` |
Já ativo, sem chave, e por quê: (a) **uma liquidação por luta** (índice único + verificação) corrige um erro (a mesma luta contada duas vezes); (b) **lugares da sala** só passam a existir quando um cliente 140 manda a conta; um jogo antigo continua como antes;
(c) **telemetria** (`meta.codeless_ranked_reports`, `duration_ms`, `ranked_duration_stats`) e a **visão de revisão de farm** são só leitura. O limite de 4 por par por dia **do Worker** já existia e não mudou.

## 2. Testes (todos passam)
| Suíte | Verificações | O que cobre |
|---|---|---|
| `port/bt4online-worker/tests/run_all.sh` | 60 | os quatro arquivos abaixo |
| ↳ `ranked_integrity` | 15 | pareamento por código, cancelamento, migração (contador e portão) |
| ↳ `ranked_identity` | 14 | identidade, unicidade, corrida real da liquidação, segredo em logs |
| ↳ `ranked_e2e` | 20 | **dois jogadores, dois espectadores, reconexões, relatos duplicados e concorrência de 10 pares** |
| ↳ `rules_guard` | 11 | Elo, temporadas e medalhas intactos |
| `supabase/tests/pglite` (`npm test`) | 50 | SQL (S1-S14, chave desligada e ligada), contrato Worker×SQL, **reversão** |
| `supabase/tests/bench/cap_race.sh` | 3 execuções | Postgres 16 real: 8 sessões, 24 partidas do mesmo par ao mesmo tempo |
**Espectadores e reconexões (E1/E2):** os dois espectadores veem a sala e o host os lista, e nenhum consegue relatar; o host que reconecta mantém sala e lugar; outra conta com a chave da sala não troca o lugar; o joiner entra e o host é avisado; um estranho que entra por código não ganha lugar;
um relato reenviado espera e o do outro lado, em ordem inversa, liquida uma vez; a mesma luta com chaves novas não conta; a recuperação de conta no meio da luta recusa o aparelho velho e aceita o novo; dois relatos do mesmo lado não pareiam.
**Concorrência e duplicados (E3, D3, cap_race):** 10 pares, 3 lutas cada, todos os relatos em voo juntos, cada um repetido e reenviado com chaves novas: nenhuma luta pontua duas vezes, nenhum par passa de 4, os ratings batem com as liquidações, nenhum relato fica preso, cada par pontua exatamente as suas 3 lutas.
**Sensibilidade comprovada por mutação:** com a proteção de unicidade desligada o E3 falha em 3 verificações (40 liquidações em vez de 30).
**Limites:** no Worker a "concorrência" é a de `await` dentro de um processo, não o paralelismo real da Cloudflare; no SQL é um par e uma máquina fraca. A reconexão simulada é a do protocolo HTTP (busca de novo, repetição, recuperação), **não** a queda de uma rede ou de um app.

## 3. Migrações SQL reversíveis
`supabase/migrations_down/` (fora de `migrations/`, para o CLI não aplicar): `…0002_ranked_farm_review.down.sql`, depois `…0001_ranked_pair_day_cap.down.sql`. A reversão **arquiva antes de apagar** (contadores, flags `rated/capped/duration_ms`, valor da chave) em `bt4_private.*_archive`, restaura a função anterior (texto literal da migração de 08/11) e deixa rating, vitórias, derrotas, histórico, medalhas e temporadas intactos; rodar a migração de novo restaura o arquivo.
**Se uma migração falha no meio**, nada muda (uma transação, `lock_timeout`): testado injetando uma divisão por zero. **Estratégia de recuperação:** reverter na ordem inversa; se a reversão também falhar, os arquivos `*_archive` e `ranked_rollback_config` guardam o necessário e a migração é idempotente (`if not exists`).
Testado em PGlite (Postgres compilado para WebAssembly) e, a corrida, em Postgres 16 real; **não** no projeto Supabase hospedado. Durante uma reversão longa, as batalhas liquidadas não entram no contador do par (a função antiga não o tem): religar a chave num dia calmo.

## 4. Regras de Elo, temporadas e medalhas: inalteradas
`rules_guard` fixa o sha256 de `seasons.js` (idêntico ao de antes da auditoria), as constantes, a fórmula e oito ganhos de Elo, as fronteiras de temporada do Worker e do SQL, ouro/prata/±16 e o ganho de Elo igual nas duas camadas. Nada em medalhas ou temporadas foi alterado; o limite diário do SQL é uma regra já existente no Worker, aplicada ao SQL com a chave **desligada**.

## 5. O que continua dependendo de aparelho ou de operação
- Dois aparelhos 140 na busca ranqueada, 140 contra 139, queda real de rede e de app durante a busca e a luta, espectador real assistindo uma partida ranqueada.
- Chave de build do release 140 em `BUILD_KEYS`, publicação, aplicar as migrações no Supabase real, e só então ligar as chaves, uma por vez (ver `docs/MIGRACAO-RANQUEADO-140.md`).
- O cliente de PC e o formato de `capped` (sem `rating`/`delta`).
