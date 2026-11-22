# Auditoria: tela de seleção nova x clássica no online (BT4), 22/11/2026

Branch `auditoria-selecao-online` (a partir de `1.0.2-Regulus` d9021b8f). Nada foi integrado à principal por este trabalho.
**Nível de validação alcançado: 1 (código) e 2 (automatizado). Os níveis 3 (aparelhos) e 4 (crossplay) estão PENDENTES e nada aqui os comprova.**
Roteiro dos níveis 3 e 4: `docs/ROTEIRO-TESTES-FISICOS.md`. Suíte offline: `port/tests/select_flow/run_all.sh`.

## 1. Causa original
Online é *lockstep*: as duas cópias rodam o MESMO jogo, movido só pelos controles trocados (`port/src/gs/net.c`). A seleção é uma máquina de
estados do jogo. `sPortStageFirst` (mapa primeiro, ou lutadores e depois o mapa) era calculado com `PortCharSel_On()`, a preferência local
`select_classic`: tela nova => mapa primeiro; clássica => lutadores primeiro. Em sala mista o 1º "confirmar" escolhia o mapa de um lado e um
lutador do outro, e o fim da seleção diferia (filme de 296 quadros + `StageIntro` passo 6, contra o VS de 60 quadros + apresentação do mapa).

## 2. Decisão de política (REVERTE a minha escolha anterior, commit 3283fdd2)
Primeira correção: online = mapa primeiro para todos. Ela deixava a tela clássica e uma cópia de PC (que não conhece a tela nova e joga a
ordem original) em ordens DIFERENTES. Evidência de que a ordem original é o único denominador comum: `git log -S` mostra que o mapa primeiro online
só existe desde `fce59cdf` (21/11/2026, builds de teste); todo cliente anterior, a tela clássica e o ramo `else` das duas seleções (o código decompilado
original) usam lutadores > mapa > VS de 60 quadros > `StageIntro`. Por isso `PORT_SELFLOW_ONLINE_STAGE_FIRST` é **0**: online, qualquer tela segue o fluxo
original; offline, a tela nova mantém o mapa primeiro. **Custo:** online a tela nova perde o mapa primeiro e o filme "caminho até a luta" (offline continuam).
Para voltar a ter: mudar a constante para 1 EM TODOS os clientes que jogam juntos (inclusive o PC). Clientes de teste de 21/11 com a tela nova (APKs até a v10) NÃO são
compatíveis com os atuais online; `NET_VERSION` não foi alterado (decisão sua: bumpar separa os dois grupos pelo handshake; ver seção 9).

## 3. Outras dependências da interface investigadas (nível 1)
| Ponto | Resultado |
|---|---|
| A tela nova escreve no estado do jogo? | Não: recebe `const CsView` (`port_charsel.h`); `static_audit.py` R3 confere que `port_charsel.c` não escreve em `gCharSel/gTeamSel/gProgress/gPad`. |
| A tela nova usa aleatório? | Não (R3). O visualizador 3D roda num "mundo" isolado (`Port_WorldEnter/Leave`, `view_a_d.c`): variáveis do jogo, heap, arquivos e números aleatórios são restaurados. R6 confere no build que `gPortRandNext` (libc rand), `gRandIndex/gRandState/gRandMag01` (twister) caem em regiões que o mundo salva e devolve. |
| Preferências locais na lógica | Só `select_stage_first`, dentro de `PortSelFlow_StageFirst`, e só offline (R1, R2). `select_classic`, `select_native_ui`, `sNarrow`, `sNoCine`, `s3D` só mudam desenho. |
| Chamadas da lógica à tela nova | Lista revisada (R4): qualquer chamada nova falha a auditoria até ser revisada. `CinemaStart/SetStage` só são alcançáveis na ordem "mapa primeiro" (R5), que online não existe mais. |
| Estrutura dos pacotes, ordem, IDs, sementes | Não leem `select_classic`; a semente única do host (`seed_draw`) é usada igual. `NET_VERSION` inalterado. Nenhum campo novo na rede. |
| Lista de mapas/músicas online | Só as embutidas (`Port_NetSession()` zera extras e substituições) nas duas telas. |
| O digest de sincronização (`Port_NetDigest`, `headless.c`) | NÃO inclui a seleção: cobre só hp, posição, relógio e `seq` da luta (a partir do estado 2). Uma divergência de escolha só aparece depois que a luta começa. Por isso há o hash canônico (abaixo), só em log. Não foi alterado: mudar o digest quebraria clientes atuais. |
| Quadros por etapa | Com a ordem original nas duas telas: seleção por entrada dos controles; `fl_vs` 60 quadros (`timer = 60`); `StageIntro` passos 0-5. Iguais, pois a lógica é a mesma código para as duas telas. |

## 4. Estado canônico
Não foi preciso uma camada de tradução no fluxo de dados: as duas telas já terminam no MESMO estado do jogo (`CSPROG->side[].pick`, `gTsProgress->team[]`,
mapa, música, jogadores), pois a tela nova só lê. Em vez disso foi criada a visão canônica `PortSelCanon` (`include/menu/port_selcanon.h`): por lutador, forma, placa, costume,
personagem e 4 palavras de itens; lados; mapa; música; jogadores. Sem ponteiros, sem padding, sem posição de cursor (não é escolha); hash FNV-1a com cada campo
serializado em little endian. É preenchida ao sair da seleção (`menu_c_e.c` `CharSel_Run`, `menu_e_b.c` `TeamSel_Run`) e só usada pelo diagnóstico: o gameplay e a rede
continuam lendo o estado original do jogo, sem passar por ela (menos risco para a lógica).

## 5. Diagnóstico (opcional, `BT3_SELECT_DIAG=1` ou a configuração `select_diag`; desligado)
Imprime `bt3: select-diag: flow <duel|team> online=… stageFirst=… screen=…`, `… leave … vblank=… canon=<hash> players stage bgm side0=… side1=…` e
`… battle introduction begins vblank=…`. As duas máquinas devem imprimir o mesmo `canon`. O desync da luta já é reportado por `sync_check` (`net_desync_end=1` encerra). Só imprime; a lista
de variáveis salvas do jogo ficou do mesmo tamanho (1 449 241 bytes).

## 6. Pergunta 7: a tela clássica com mapa primeiro
Tornou-se inalcançável: o mapa primeiro só ocorre com a tela nova offline (`PortSelFlow_StageFirst`), e online a tela clássica e a nova rodam o ramo original. Respostas pelo código sobre o
estado ANTERIOR, que já não existe: o filme `fl_map` era do próprio jogo; a espera de ~5 s (296 quadros contados em `CineBlank`) não era só visual: era contada no laço da lógica (`continue` durante ela), e os
controles só serviam para pular; o lockstep seguia avançando por `Gfx_EndFrame`. Nada disso foi executado em aparelho e a apresentação visual **não está aprovada**; foi eliminada em vez de verificada.

## 7. Testes (nível 2) — ver `port/tests/select_flow/run_all.sh`
| Teste | Resultado |
|---|---|
| `select_flow_test.c`: 32 pares online de tela x tela x configuração dão a mesma ordem (a original); offline idêntico à regra antiga; a regra antiga errava 8 de 32 | passou (48) |
| `select_canon_test.c`: determinismo, sem padding, cada campo de escolha cobre o hash, cursor não, lados/times/espelho, hash de referência fixo (`4c7ea245eb2ad24c`) | passou (98) |
| `static_audit.py` R1–R6 (teste de mutação: 4 violações plantadas, 4 detectadas) | passou |
| Sintaxe das 3 unidades alteradas com `gcc` do host (alvo PC) | passou |
| Compilação e ligação Android (`menu_c_e`, `menu_e_b`, `btl_seq`) | passou |
| Simulação de sequências de controles comparando estados das duas telas, entrada em sala, carregamento, transformações, habilidades, fim | **NÃO executada**: exige rodar a lógica do jogo (sem dados do jogo, sem build de PC: faltam cmake e SDL3). O que foi feito no lugar: provar por auditoria estática que a única diferença entre as telas é o desenho |
| Build de PC completo | **NÃO possível** neste PC (sem cmake, SDL3 nem dados) |

## 8. Fluxo clássico do PC (comparação estrutural)
Sem binário de PC para comparar. O que se verificou no código desta árvore: o ramo "lutadores, depois mapa" é o código decompilado original (`CharSel_Update`, estado `CHARSEL_ST_DONE` → `fl_map` → `fl_vs` 60 quadros); a estrutura de dados (`CharSelPick`, `TsMember` de 12 palavras) é a do jogo e a tela
nova não a altera. A suposição "um PC sem a tela nova joga a ordem original" é compatível com o histórico (o mapa primeiro online nasceu em 21/11), mas **não foi verificada contra um cliente de PC**.

## 9. Protocolo
Nenhuma mudança: `NET_VERSION`, pacotes e digest intactos. Decisão pendente sua: bumpar `NET_VERSION` separaria os clientes de teste de 21/11 (mapa primeiro online) dos atuais pelo handshake, em vez de deixá-los entrar em sala e divergir; só deveria
ser feito se esses APKs de teste estiverem na mão de outros jogadores.

## 10. Riscos que não podem ser descartados
- A ordem original online com a tela nova (fase do mapa desenhada pela clássica, `fl_vs`) é o caminho do `select_stage_first=0` offline: não foi visto em aparelho online.
- Isolamento do mundo do visualizador (RNG, heap) confirmado por comentário de projeto, pelas regiões do build e por auditoria, mas não exercitado numa partida.
- Clientes de 21/11 (até a v10) divergem dos atuais online.
- O digest não cobre a seleção: uma escolha divergente só é vista quando a luta começa (use `BT3_SELECT_DIAG`).
- Nada disso foi comparado com um cliente de PC real.
