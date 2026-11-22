# Auditoria: tela de seleção nova x clássica no online (BT4), 22/11/2026

Branch `auditoria-selecao-online` (a partir de `1.0.2-Regulus` d9021b8f). Nada foi integrado à branch principal.

## 1. Existia risco real de desync? Sim, e certo, não eventual.
Online é *lockstep*: as duas cópias rodam o MESMO jogo, movido só pelos controles trocados (`port/src/gs/net.c`, topo). A seleção é uma
máquina de estados do jogo (`CharSel_Update`, `TeamSel_Update`); qualquer ramo local que mude a ordem ou a duração dos quadros faz as duas
cópias interpretarem os mesmos botões de formas diferentes.

## 2. Causa técnica
`sPortStageFirst` (a ordem: mapa primeiro, ou lutadores e depois o mapa) era calculado com `PortCharSel_On()`, que é a preferência local
`select_classic`:
```
sPortStageFirst = PortCharSel_On() && ... (Port_NetSession() || Port_NetActive() || Port_Setting("select_stage_first", 1))
```
Online, tela nova => 1 (mapa primeiro); clássica => 0 (lutadores primeiro). Em uma sala mista:
- o 1º "confirmar" do jogador A escolhe o MAPA, o do B escolhe um LUTADOR: os estados divergem já na seleção;
- o fim também difere: com mapa primeiro o jogo usa `PortCharSel_CinemaStart` (296 quadros: `CS_FX_RISE 26 + CS_CINE_END 270`) e a luta
  entra por `BtlSeqStageIntro_Enter` no passo 6 (`btl_seq.c`, `PortCharSel_CineBattle()`); sem ele usa o VS do jogo (`fl_vs`, 60 quadros) e a
  apresentação do mapa (passos 0 a 5). A luta começaria em quadros diferentes nas duas cópias.

## 3. Arquivos e funções
- `src/menu/menu_c_e.c` `CharSel_Run` (seleção 1x1, linha ~2273) e `src/menu/menu_e_b.c` `TeamSel_Run` (equipes, ~2534): a decisão.
- `src/menu/port_charsel.c`: `PortCharSel_On`, `PortCharSel_CinemaStart` (online já começava sempre o filme: quem não o desenha mostra a tela
  enquanto espera), `PortCharSel_CineBattle` (usado por `src/battle/btl_seq.c`).
- Novo: `include/menu/port_selflow.h` (`PortSelFlow_StageFirst`, função pura) e `port/tests/select_flow/select_flow_test.c`.

## 4. O que está certo (verificado lendo o código; não precisou de correção)
- A tela nova recebe `const CsView`: só LÊ o estado do jogo (`CharSel_PortFill`/`TeamSel_PortFill`); não escreve em `gCharSel`/`gTeamSel`, nem
  em pads, nem usa `Rand`. IDs de lutador, costume, formas (fusões/transformações), times, lados, repetidos, tudo vem do estado do jogo.
- `PortCharSel_FormRowSize/FormOtherRow` (usadas pela lógica) são funções puras dos argumentos que o jogo passa.
- A lista de mapas e músicas online é só a embutida (`Port_NetSession()` zera extras e substituições), igual nas duas telas.
- Rede: a estrutura dos pacotes, a ordem dos dados, os IDs e as sementes aleatórias não leem `select_classic`; `NET_VERSION` não muda.
- Ajustes só de desenho (`select_native_ui`, `sNarrow`, `sNoCine`, `s3D`) não mudam o fluxo: online o filme é sempre contado (`CinemaStart`).

## 5. Correção
Online, a ordem depende só de "estar online": `PortSelFlow_StageFirst(online, tagNet, newScreen, settingStageFirst)`: tag network => 0 (como antes);
online => 1; offline => como antes (`newScreen && setting`). A tela clássica fica intacta: ela roda o próprio filme `fl_map` do jogo, só que antes.
Offline nada mudou (o teste compara com a regra antiga para todas as entradas). O protocolo de rede NÃO mudou.

## 6. Testes
| Teste | Resultado |
|---|---|
| `port/tests/select_flow` (gcc, no PC): 32 pares online (2 telas x 2 telas x 2 configurações x 2 configurações x tag) dão a mesma ordem; offline idêntico à regra antiga; a regra antiga errava em 8 dos 32 pares | **passou** (45 verificações, 0 falhas) |
| Compilação Android (`menu_c_e`, `menu_e_b`, ligação) | **passou** |
| Matriz com dois aparelhos: (1) clássica+clássica, (2) nova+nova, (3) A nova / B clássica, (4) A clássica / B nova, (5) alternando entre partidas; sala, sincronização, início, carregamento, primeiros segundos, transformações/trocas, habilidades, fim | **NÃO executada**: exige dois celulares; o PC não roda o jogo (sem dados do jogo, só build Android) |
| Comparar estados internos das duas telas | **não executado** (o mesmo motivo). Aplica-se `sync_check` (net.c): com `net_desync_end=1` a partida avisa/encerra se divergir |
| Verificação visual da tela clássica com o mapa primeiro e do filme de espera | **não executada**: na tela clássica o filme de 296 quadros não é desenhado, ela mostra o próprio jogo enquanto espera (como já ocorre em tela estreita) |

## 7. Riscos que restam
- A tela clássica nunca tinha rodado o mapa primeiro: a lógica é a do jogo e o desenho é o do filme `fl_map` original, mas falta ver em aparelho.
- Espera de ~5 s com a tela clássica parada ao fim da seleção (a nova mostra o filme). Melhorar o desenho dessa espera é cosmético.
