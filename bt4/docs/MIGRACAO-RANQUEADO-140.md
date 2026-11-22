# Migração do ranqueado: release 140 (PORT_RELEASE) e o portão `RANKED_MIN_RELEASE`

`PORT_RELEASE` passou de **139 para 140** (`port/src/port_release.h`) com a identidade conta-sala (commits `4f6f320e` servidor e `32c24471` cliente). Antes, todos os builds diziam 139 (13/11 a 22/11), por isso o
serviço não distinguia o cliente que manda a conta na busca ranqueada dos que não mandam. **O número só vale a partir de uma versão publicada com ele.**

## O que muda e o que NÃO muda no servidor
- `RANKED_ALLOW_CODELESS=0` (padrão) e **não há exceção automática** para versões antigas.
- `RANKED_MIN_RELEASE` continua **desligado** (0). Ligá-lo recusa, na busca ranqueada, o jogo com `rel` menor, com o "old, need N" que o jogo já mostra como atualização.
- `RANKED_REQUIRE_ACCOUNT=1` (também desligado) faz o ranqueado exigir a conta nos dois lugares; só depois de a migração terminar.

## Antes de publicar o 140 (operacional, não feito aqui)
1. **Chave de build do release 140:** o Worker confere a assinatura `X-BT3-Sign` por release (`rooms.js` `keyCheck`). Se `BUILD_KEYS`/`REQUIRE_KEY` estão ligados, um 140 sem chave registrada é recusado. Procedimento do README do matchmaking, item (e): nova chave, segredo do GitHub, linha `{"139":"…","140":"…"}` em `BUILD_KEYS`, tag, conferir `build key: given` no log do workflow e `BT3_SIGN_TEST=1` no programa.
2. **Publicar e testar** a versão com o 140 em aparelhos: busca ranqueada entre dois aparelhos novos, e um novo contra um 139 (deve funcionar como jogo antigo para o servidor: a conta só do lado novo; ver limite abaixo).
3. **Aplicar a migração SQL** `20261122000001_ranked_pair_day_cap.sql` no Supabase (autoridade do PC/R5) antes do ranqueado novo, e conferir `bt4_private.ranked_duration_stats` depois de algumas partidas.

## Depois de publicada, testada e reconhecida pelo servidor (decisão sua, uma etapa por vez)
1. Acompanhar o log do cron: `ranked reports without a room code so far` (contador `codeless_ranked_reports`) deve parar de crescer.
2. `RANKED_MIN_RELEASE=140`: o cliente antigo passa a ser recusado na busca em vez de jogar uma partida que não conta.
3. Quando todos os lugares forem ligados a contas: `RANKED_REQUIRE_ACCOUNT=1`.
4. `RANKED_ALLOW_CODELESS` permanece 0.

## Limite conhecido enquanto houver cliente 139
Uma sala com só um lugar ligado (o outro é um 139): o lugar solto não se distingue de um estranho; o servidor ainda exige que a conta do lugar ligado seja uma das duas que pontuam. O passo 3 acima fecha isso.
