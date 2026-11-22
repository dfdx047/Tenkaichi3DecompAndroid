# Correções do online — novembro de 2026 (branch 1.0.2-Regulus)

Auditoria e correções da rede de cada projeto, cada um tratado à parte. Um item só conta como resolvido em produção
depois da validação que ele pede (servidor publicado na Cloudflare, partida real entre aparelhos); o que está aqui foi
testado no repositório, com o código real de cada parte e o resto simulado.

## Testes de regressão

| Projeto | Como rodar | O que é real / simulado |
|---|---|---|
| Dragon Net (BT3 1v1, Ranqueado) | `node port/dragonnet/<x>-test.mjs` (dragonnet, versions, ranked, ranked-flow, ranked-cd, ranked-rematch, chat, presence, test) | Workers reais em SQLite; relógio simulado |
| TagTeam BT3 — transporte e serviço | `sh port/tools/taglink_test/run.sh` | `tag_link.c` e `tagnet-worker` reais; rede, STUN, TURN e segundo plano simulados |
| TagTeam BT3 — serviço | `node port/tagnet-worker/test.mjs` | Worker real em SQLite |
| TagTeam BT3 — sala | `sh port/tools/tagnet_test/run.sh` | `tag_net.c` real (TN_FAST); o jogo simulado |
| TagTeam BT4 — sinalização | `sh bt4/port/tests/dragonnet_tag/run.sh` | `dragonnet_tag.cpp` real; HTTP simulado |
| TagTeam BT4 — entradas | `sh bt4/port/tests/tag_input_resend/run.sh` | kernel do anfitrião real (`shared/tag_kernel.c`); canal com perda simulado |
| BT2 online | `bt2/plugin/tests/fight_tests.cpp` (comando no topo do arquivo) | `FightSession` e `Link` reais em UDP local |

## Corrigido no repositório

| Item | Projeto | Resumo |
|---|---|---|
| D2, D3 | Dragon Net | README com `ranked-flow-schema.sql`; testes montam o banco pela linha do README; `test.mjs` acha os módulos pelos imports |
| D1 | Dragon Net | capacidades do Ranqueado pedidas com a versão real; `ALLOWED_VERSIONS` documentado `40001,5001,6001,140001` (autoridade `5001,6001`) |
| V1 | TagTeam BT3 | harness do TagLink compila e roda (`run.sh`) |
| C1 | TagTeam BT3 | erro passageiro do serviço não tira mais o convidado da sala |
| C2 | TagTeam BT3 | poll a cada 15 s em segundo plano; sala e lugar do convidado vivem 5 min sem poll (servidor) |
| P5 | TagTeam BT3 | SYN para outro endereço não apaga a confirmação devida |
| P3 | TagTeam BT3 | a thread da sala anterior não mexe mais na sala nova (busy, socket, relay) |
| P6 | TagTeam BT3 | números de membro dentro de 1..255 (byte do relay); join concorrente não falha |
| P4 | TagTeam BT3 | TN_FAST dividido em datagramas de até 1092 bytes; sem estouro de buffer |
| E1 | BT2 | teste corrigido para o comportamento real do jogo (padField a cada VBlank); pausa acima de 6 s documentada |
| S2 | TagTeam BT4 | requisição sem resposta repetida (1, 2, 4 s) antes de falhar |
| B2 | TagTeam BT4 | cada entrada do convidado vai com as duas anteriores (compatível com anfitriões antigos) |

## Depende de ação fora do repositório

- **Publicar na Cloudflare** `port/tagnet-worker/worker.js` (C2 servidor, P6): sem isso nada muda em produção.
- **Z3 / D1**: conferir no painel do Worker `mitigo-rooms` se `ALLOWED_VERSIONS` tem `6001` e `140001`, e na autoridade
  do Ranqueado `6001`. Sem `6001`, o BT3 atual recebe 409 no Ranqueado, chat, contagem e salas.
- **Z1**: o código do servidor `POST /v1/tag` (TagTeam do BT4 pelo Dragon Net) não está em nenhuma branch deste repositório.
- **Z2**: o workflow diz que o motor BT4 do APK vem de `bt4dec-proto`; S2 e B2 estão em `bt4/` desta branch. Confirmar de
  onde sai o motor publicado.

## Pendentes de validação em partida real (não implementados)

Estes mudam o protocolo ou a simulação de todos os aparelhos; só são implementados com um teste em partida real
(`port/tests/tag_online_runtime.py` no PC e aparelhos):

- **P1 (TagTeam BT3)** — no modo festa, membros sem lutador entram na máscara de entradas da partida
  (`tag_net.c`: `matchMask` a partir de `config.lobby.members[].present`, linha ~1639) e produzem entrada neutra
  (`TagNet_Tick`, linha ~2327). Efeitos: `arb_slow_host` desacelera a sala por um espectador lento; `arb_commit_all`
  remove por "10 s sem entrada"; banda nos datagramas. **Não basta tirar esses membros da máscara**: o convidado sem
  lutador continua com `local_produces` verdadeiro e manda propostas (`arb_produce`), que o anfitrião passaria a
  ignorar em `arb_recv_propose` (ele sai antes de ler `fastAck` quando o membro não está na máscara); sem confirmação,
  em ~RING-64 quadros o convidado encerra com "The host stopped taking this player's inputs". A correção precisa mudar
  juntos: o cálculo da máscara (assentos de `config.owner`, igual em todos os aparelhos), `local_produces` (não produzir
  fora da máscara) e `arb_recv_propose` (ler `fastAck` antes de recusar), e ser validada numa sala real com espectadores.
- **P2 (TagTeam BT3)** — desync decidido sem maioria (`drop_diverged`; no treino, o primeiro digest vira a verdade).
- **O1 (BT3 1v1 e BT4 1v1)** — IPv6 e migração autenticada de endereço; hoje só o endereço e porta iniciais são aceitos
  (`net.c:1667` BT3, `bt4/.../net.c:1675`) e a partida termina com `exit(2)` após 6 s sem pacotes. Cada modo à parte.
- **B1 (TagTeam BT4)** — sem TURN no Android (`tag_team_p2p.cpp`: só STUN); depende do servidor do Z1.

## Ainda não auditado a fundo

`net_lobby.c` (lobby do BT3 1v1), o 1v1 do BT4 além do filtro de endereço, o resto de `tag_kernel.c` e
`tag_team_net.c` do BT4, os testes HTTP do Ranqueado (precisam de um Worker rodando), B3, B4, B5, H1, H2, L1.
