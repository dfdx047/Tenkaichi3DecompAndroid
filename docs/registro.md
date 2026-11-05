# Registro do dia 5/11 (release-1.0-dia)

Branch `release-1.0-dia`, a partir da `release-1.0` (`dd201117`). Commits só locais, sem push.
Aparelho: só o Retroid Pocket 5 (`192.168.1.89:5555`), com o perfil `sd480p` do Hidden Leaf
(`tools/device-profile.sh -s 192.168.1.89:5555 limit sd480p`; `restore` no fim do dia).
Odin 3 e Cube (`192.168.1.134`, `192.168.1.183`) são da sessão do Naruto e não são usados.

## Estado

| # | Item | Estado | Nota |
|---|---|---|---|
| 1 | Merges das branches da Regulus CiberSec | feito | 3 merges (`282f207e`, `a8f06ebd`, `b00a397d`), cada um com build e teste no RP5; `ranked-flow-v2` não é dela (ver Decisões) |
| 2 | Tag Team liga/desliga no launcher | feito | testado nos dois sentidos depois dos merges; padrão 0 (`TAG_TEAM_DEFAULT`) |
| 3 | Rota e medidas por jogo no RP5 sd480p | pendente | |
| 4 | Nominum no BT4 | pendente | |
| 5 | Outros gargalos | pendente | |
| 6 | Bugs conhecidos | pendente | |
| 7 | licenses.txt do APK | feito (`39897130`) | conferir o `licenses.txt` no APK final |

## Decisões

| Hora | Decisão | Por quê |
|---|---|---|
| 07:55 | As 3 branches da Regulus CiberSec são `Regulus-optimize` (10 de 10 commits dela), `bt3-espectadores-lobbies` (10 de 13) e `tag-team-android` (34 de 37). A `ranked-flow-v2` **não é dela** (22 commits de Ariel R Rodrigues / Androidxxt01) e não foi mesclada | A ordem mandava conferir pelo autor. A `ranked-flow-v2` muda o fluxo do Ranked e o servidor (flow:2, Ranked C/D): é online e precisa de dois aparelhos e do serviço para testar. Fica para decisão do dono |
| 08:05 | Merge 3, conflitos: `GameActivity.kt` fica o nosso (o mesmo `onCreate`); `gs_core.c` e `gs_input.c` com os dois lados (o `gsOnRenderThread` do Tag; o `#ifdef` do Android e o `sPadLock`); `menu_labels.c` e `plat_game.c` com a leitura da 1.0 (`Port_AfsSpanShared` e `Port_FileReadAt`, já testadas, que corrigem o mesmo defeito que a versão da branch); `tag_link.c`: o relay que se reabre (o mais novo, da branch) **e** o ritmo da 1.0 (2 s, recuo no 429, nada em segundo plano); o ritmo adaptativo da branch (1 s só enquanto alguém conecta, 15 s com conexão direta) fica; `L` sem inicializador (16 MB fora do arquivo), com `sock` e `sock6` em -1 | Online: o comportamento mais novo, sem perder o limite de pedidos que a 1.0 pôs por causa do 429 |

## Histórico

- 07:50 Início. Branch criada. RP5 com a 0.9.0 instalada (`versionCode` 55), sem perfil ativo.
- 07:52 Merge 1, `Regulus-optimize`: sem conflito. As contas novas (`plat_fastvec.c`, `softfloat_ps2.c`, `vu0_a.c`) conferidas
  bit a bit com `port/tools/fastvec_check.c`: 20 milhões de matrizes, 0 diferenças no PC (NEON e genérico) e no RP5. A comparação
  nova é a mesma fórmula do `RefVu0_LtBits`; o int→float troca o laço pelo `clz` (igual para todo u≠0). O online não muda por elas.
- 07:54 Perfil `sd480p` aplicado no RP5: cpu0-5 ligados (cpu6-7 desligados), policy0 até 1,80 GHz, A77 até 1,77 GHz, GPU 305 MHz.
  Não pegou: `sched_boost` ficou 1 (pedido 0), o piso da policy0 ficou 1,17 GHz e `max_pwrlevel` ficou 1 (o teto em Hz pegou).
- 07:56 SDL3 3.4.12 compilada aqui com os dois patches novos da `Regulus-optimize` (`vulkan-descriptor-sets`,
  `vulkan-fragment-set-memo`) em `port/build/sdl3-dia/` (o CI só a terá quando a toolchain rodar de novo). Os builds do dia usam
  `BT3_SDL3=port/build/sdl3-dia` e `BT2_SDL3_DIR=port/build/sdl3-dia`.
- 07:58 Licenças do APK (item 7), `39897130`: `port/licenses/bundled-gpl.txt` (Nominum, PCSX2/ARMSX2, BT3-Recomp/PS2Recomp,
  dbz-bt4-recomp, FFmpeg n7.1 sem `--enable-gpl`, BT2 Recomp), `rcheevos.txt`, `GPL-3.0.txt`, `LGPL-2.1.txt`.
- 08:00 Merge 2, `bt3-espectadores-lobbies` (`a8f06ebd`): o conflito do `GameActivity.kt` é o mesmo `onCreate` que veio no merge 1;
  ficou o nosso. O novo são os diagnósticos de lobby do Androidxxt01: só log (`[lobby] ...` no lugar de `bt3: lobby: ...`, e as
  últimas 200 linhas em `lobby_log.txt`, que o Exportar log leva). Nenhuma regra do online mudou.
- 08:03 Base (1.0.0, `DragonRage-1.0.0.apk`) no RP5 sd480p, `dia_smoke.sh base`: BT3 2x Vulkan 99,9%, 30,8 imagens/s, trabalho
  15,0 ms (jogo 2,9, lista 12,1), 0 crash; Dragon Net: lobby feito pelo serviço, código `F2HUSB`.
- 08:09 Merge 1 testado (`m1.apk`): BT3 2x 100%, 30,7 imagens/s, trabalho 15,4 ms (jogo 2,8, lista 12,6), 0 crash; Dragon Net:
  código `H3N84R`. Igual à base dentro do ruído.
- 08:05 Merge 3, `tag-team-android`: 6 conflitos resolvidos (decisões na tabela). A branch reescreve `BtlHit_TestHit` e o laço de
  `BtlColl_Update` (acertos), `BtlChars_IsTimeStoppedFor` e põe `Port_RandNote` em `Rand_Next`, em código que também roda no BT3
  normal. Para não quebrar o Dragon Net contra a 1.0, o teste do merge 3 compara o hash da luta (`BT3_HASH`, colunas da luta:
  vida, posições, relógio, sorteio) com o do merge 2, na mesma luta gravada.
- 08:13 Merge 2 testado (`m2.apk`, agora com o hash da luta gravado): BT3 100 %, 30,7 imagens/s, 0 crash. O teste do Dragon Net
  dele saiu vazio porque eu instalei o APK do merge 3 durante essa fase (erro meu, não do merge). O merge 3 contém o merge 2 e passou
  no mesmo teste (abaixo).
- 08:19 Merge 3 testado (`m3.apk`) e commitado (`b00a397d`): BT3 100 %, 30,7 imagens/s, 0 crash; Dragon Net: sala criada
  (`U88GPZ`, linhas `[lobby]`); **a luta gravada dá o mesmo hash da luta que o merge 2 nos 6213 blanks**
  (`port/tools/lab/fightcmp.py`): a reescrita dos acertos não muda o BT3 normal nem a simulação do online contra a 1.0.
  O "jogo" de 9,6 a 9,9 ms nos testes m2/m3 contra 2,8 no m1 é o `BT3_HASH` (somar a memória a cada blank), não os merges.
- **O que um aparelho só não testa:** uma partida Dragon Net entre dois jogadores (só a criação da sala pelo serviço), o Tag Team
  online (`tag_link.c`, relay) e a `ranked-flow-v2`. O hash igual cobre a simulação; a conexão entre dois aparelhos fica para o
  teste com dois aparelhos antes da 1.0.
- 08:23 Item 2, Tag Team pelo ajuste do launcher (`dr_tag_team` em `bt3_settings.txt`, o mesmo que a chave do Ajustes grava;
  `port/tools/lab/tagtoggle.sh`), BT3 aberto pelo app: com 1, o SELECT no menu principal abre os "Modos modificados" (Batalha em
  Equipe, Todos Contra Todos, Treino, Desafio, Torneio, Sala Online, Configurações; textos em português, com letras); com 0, o SELECT
  não faz nada e o menu principal é o do jogo. 0 crash nos dois. O padrão continua 0 (`EngineSettings.TAG_TEAM_DEFAULT`, lido no
  `GameActivity.getArguments` e na chave do Ajustes); o teste deixa o RP5 com 0. Imagens em `~/DragonRageRefs/dia/tag/`.

