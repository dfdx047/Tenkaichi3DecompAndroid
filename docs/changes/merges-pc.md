# Mesclas do PC

Regras e formato: [README](README.md).

### 1.0.0 85cdb232: `origin/Regulus-optimize` (BT2, Dragon Net Online, PC 0.1.38.9)
- **Onde:** pais `e1b9207a` (0.9.0) e `e84c359f`. `--no-ff`, sem squash nem rebase: os commits mantêm os autores.
- **O que e por quê:** traz o BT2, o Dragon Net Online e o PC até 0.1.38.9. `git diff-tree --cc` não lista nenhum arquivo: nenhum conflito precisou de escolha nova no próprio merge, porque o branch já tinha mesclado a 0.9.0 (`33e4bbee`).
- **Medido/provado:** build dos três motores e, depois do merge do Tag Team, a comparação da bateria. Medir antes dessa segunda mescla não foi feito em separado.
- **O que pode quebrar:** o que a revisão listou: relay em socket de partida em curso, launcher sem controle (corrigidos); ver `docs/release-1.0.md`.
- **Como testar de novo:** [`regression-tests.md`](regression-tests.md).

### 1.0.0 cb026e37: `tag-team-android` (Tag Team Mod, PC 0.1.39)
- **Onde:** pais `85cdb232` e `18dad4b3`. `git diff-tree --cc` lista `android/.../engine/GameActivity.kt`, `android/.../touch/TouchPadView.kt` e `src/menu/port_tbo.c` como diferentes dos dois pais; só `port_tbo.c` (`Tb_Vanilla`) mostra um trecho próprio, e `Tb_Vanilla` foi corrigido depois (`online.md`).
- **O que e por quê:** traz o Tag Team Mod e o PC 0.1.39 (lobbies, torneios, entrar por endereço). Regra da ordem de serviço: jogo e PC como no PC 0.1.39; Android como na 0.9.0. O `codespace-*` não foi mesclado.
- **Medido/provado:** o Tag Team ligado por padrão diferia da 0.9.0 nos menus (dica de SELECT, grade de personagens), então ficou desligado por padrão, a pedido do desenvolvedor. Desligado: 0 pixels nos 11 quadros do Cube 1x e nos 5 primeiros do RP5 2x; a demo da tela de título difere no RP5 ([`tag-team.md`](tag-team.md)).
- **O que pode quebrar:** tudo que o mod toca em código comum; os guardas estão em [`tag-team.md`](tag-team.md). A mudança de `src/sys/pad.c` para 4 portas invalida gravações antigas (`rec4.py`).
- **Como testar de novo:** [`regression-tests.md`](regression-tests.md).

## Histórico: 0.9.0 para trás

**Como o PC chegou aqui.** `git log --merges --first-parent e1b9207` lista 18 mesclas, e nenhuma é do PC para o Android:
16 são do decomp, de 05/10 e 06/10, antes de a linha Android existir; `04321a8a` é um PR do Dragon Net; `e1b9207a` é a
própria 0.9.0. A linha Android nasce em `750273dd`, sobre `v0.1.11` (`153b81a3`). Da 0.4.0 à 0.7.3, o código do PC entrou em
commits sem segundo pai, e `git merge-base eefe5d9b v0.1.38.7` ainda dá `153b81a3`. A primeira mescla real é `01224d08`,
no segundo pai de `e1b9207a`, por isso ela não aparece na lista. Abaixo, primeiro essas entradas de verdade, depois as 18.

A versão do PC de cada commit sem pai vem de `PORT_RELEASE` em `port/src/port_release.h` (desde a 0.6.0), do changelog e do
menor `git diff --shortstat <tag> <commit> -- src port/src` entre as tags `v0.1.*`, conferido pela data. As mesclas depois
da 0.9.0 (`85cdb232`, `cb026e37`) estão acima, fora deste histórico. Linhas: lado novo do diff naquele commit; o arquivo de hoje tem outra numeração.

### 0.9.0 01224d08: PC 0.1.38.7, a primeira mescla com dois pais
- **Onde:** pais `eefe5d9b` e `52b675c3` (`v0.1.38.7`). O relatório lista 10 arquivos com conflito: `gs_core.c`, `gs_draw.c` (12 trechos), `gs_draw.h`, `gs_gl.c`, `gs_gpu.c`, `gs_input.c`, `gs_texpack.h`, `net.c`, `snd_adx.c`, `ui.cpp`. `git diff-tree --cc` mostra 18 com resultado diferente dos dois pais: esses e mais `gs_texpack.c`, `shaders/gs.frag`, `ui.h`, `plat_crash.c`, `plat_stub.c`, `tools/android.py`, `tools/undefined.py`, `src/battle/eft_c.c`. `port_release.h` vai a 138.
- **O que e por quê:** feita com `git merge-tree --write-tree --merge-base=v0.1.37 HEAD v0.1.38.7`, porque o ancestral comum real é `153b81a3`, de antes de todas as entradas sem segundo pai; os conflitos foram resolvidos na árvore de trabalho e o commit criado com os dois pais. Regra: código do jogo e comportamento do PC ficam como no PC; ficam como aqui o renderizador e as otimizações Android (passes sem profundidade, refs de stencil, GL persist, ADPF, cache de RAM), `ui_android.inc`, o loader, o launcher, o Dragon Net e os ganchos do BT4. Bits: PC 25 Wb e 26 noAux; nossos 27 Nd e 28 depthless; cache `bt3_pipelines2.txt`. Pacotes lidos pela thread do PC, que também descompacta DXT ou carrega a cópia ASTC no Android. `snd_adx.c`: ver [`audio.md`](audio.md).
- **Medido/provado:** Cube VK 1x, 100%, 0 pixels nos 8 quadros. Cube VK 2x 86,8% → 95,0%; GL 2x 92,3% → 97,7%. RP5 VK 2x 100% → 99,7%; VK 4x 79,8% → 83,5%; GL 2x 100%. Online: `PC: Working(mine=138, searching=3)` no RP5 e `Dragon Net: Working(mine=138, searching=1)`.
- **O que pode quebrar:** o que veio depois mostrou: `libbt4.so` não abria (`6e57077d`); `extern int gsScale` ligado a uma função do jogo (`6e57077d`). Conferir toda mescla com `check_symbols` e `android_abicheck.py`.
- **Como testar de novo:** [`regression-tests.md`](regression-tests.md).

### 0.7.3 2d8a5a72: PC 0.1.37, sem segundo pai
- **Onde:** 84 arquivos; `port/src/gs/gs_gpu.c` `blend_from_key`, `port/src/gs/gs_gl.c` `pipe_decode` e `apply_blend`, `port/src/gs/shaders/gs.frag`, `port/src/plat_mem.c` `map_heap` (esta parte é nossa), `port/src/gs/menu_translation.c`, `port/src/gs/net.c`. `PORT_RELEASE` 137.
- **O que e por quê:** o serviço de partidas passou a exigir a 0.1.37. Blend: alfa de origem acima de 0x80 conta como no PS2 (bit 9, o clarão da Solar Flare). Traduções. Conflitos: não registrado.
- **Medido/provado:** menor diff entre as tags: `v0.1.37`. O device lab tentou isolar o custo do bit 9 no Adreno: "não foi isolado".
- **O que pode quebrar:** `bkey` passou de `0x1FF` a `0x3FF` nos dois back ends; ao mesclar, os dois têm de andar juntos.
- **Como testar de novo:** Solar Flare numa luta; banner no modo PC.

### 0.7.0 1867006b: PC 0.1.36, sem segundo pai
- **Onde:** 68 arquivos; `port/src/port_release.h`; `port/src/gs/gs_astc.cpp` (nosso); `port/src/gs/ui_android.inc` (nosso). `PORT_RELEASE` 136.
- **O que e por quê:** o serviço passou a recusar a 0.1.35. Do PC: itens zerados nas sessões online. Conflitos: não registrado.
- **Medido/provado:** menor diff entre as tags: `v0.1.36`.
- **O que pode quebrar:** a versão recusada para o online inteiro a cada nova do PC, o que levou ao Dragon Net.
- **Como testar de novo:** banner com o serviço do PC.

### 0.6.0 7b99f8ad: PC 0.1.35.x, sem segundo pai
- **Onde:** 204 arquivos, 108.297 linhas; `port/src/gs/gs_draw.c` (1141), `ui.cpp` (1344), `net.c` (623), `gs_input.c` (561), `plat_fprint.c`, `a11y_game.c`; `LICENSE` (AGPL-3.0). `PORT_RELEASE` 135. `0a4b5935` tira o script de patch (`apply_dragonrage_patch10.py`).
- **O que e por quê:** "todas as novidades do port para PC": fontes, profundidade precisa, stencil de sombra plana (D32S8), pedidos assinados ao serviço. Conflitos: não registrado.
- **Medido/provado:** menor diff entre as tags: `v0.1.35.4` (7006 + 345), que também é anterior ao commit no tempo. O stencil custou caro no Adreno: 70,8% contra 76,0% no RP5 VK 4x, medido só na 0.9.0.
- **O que pode quebrar:** é daqui que veio a regressão do Adreno; nenhuma medida foi feita na hora.
- **Como testar de novo:** `BT3_GPU_NO_STENCIL=1` no RP5.

### 0.4.0 928b2413: PC cerca de 0.1.24, sem segundo pai
- **Onde:** 75 arquivos, 18.470 linhas: `port/src/gs/net.c`, `net_match.c`, `net_relay.c`, `state.c`, `ui.cpp`, `plat_verify.c`; `src/battle/`, `src/menu/`. Sem `PORT_RELEASE` ainda.
- **O que e por quê:** online e movimento suave do PC. O changelog só diz "em dia com o port de PC mais recente". Adaptações nossas: `state.c` em arm64, vigia e checagem de dados desligados. Conflitos: não registrado.
- **Medido/provado:** menor diff entre as tags: `v0.1.24` (5290 + 210; `v0.1.23` 5386 + 216; `v0.1.25` 5337 + 1074); a data do commit fica entre as duas tags. É dedução, não registro.
- **O que pode quebrar:** ver [`online.md`](online.md), entrada de `state.c`.
- **Como testar de novo:** Dragon Net entre os aparelhos.

### 0.1.0 750273dd: base: PC v0.1.11
- **Onde:** primeiro commit da linha Android, filho de `153b81a3`.
- **O que e por quê:** ramo do PC com o launcher.
- **Medido/provado:** `git merge-base` com qualquer tag posterior do PC dá `153b81a3`.
- **O que pode quebrar:** ferramentas de mescla que assumam ancestral recente: passar a base à mão, como em `01224d08`.
- **Como testar de novo:** `git merge-base <commit> <tag>`.

**As 18 mesclas de `git log --merges --first-parent e1b9207`.** Nenhuma tem resultado diferente dos dois pais em `git diff-tree --cc`, ou seja, nenhum conflito foi resolvido à mão.
Nenhuma traz medida nem teste no registro. As de decomp são anteriores ao Android.

| Commit | Data | Segundo pai e assunto | Mudança |
|---|---|---|---|
| `e1b9207a` | 2026-10-25 | `1c4a9a16`: a 0.9.0 entra no `main` (não é do PC, mas traz `01224d08`) | 147 arquivos, +108452, -7045 |
| `04321a8a` | 2026-10-19 | `3950958c`: PR 3, Dragon Net multi-servidor (não é do PC) | 4 arquivos, +160, -36 |
| `1332b1e2` | 2026-10-06 | `697d9e08`: decomp "PS2 code only" registrado como mesclado **sem tomar nada** | nenhuma |
| `8c45e164` | 2026-10-05 | `1a77e08c`: StgVu rotations | 2 arquivos, +69 |
| `70ec20a9` | 2026-10-05 | `fc5ad094`: movie UNCACHED | 1 arquivo, +4 |
| `38ec5680` | 2026-10-05 | `246bd190`: listagens VU1 | 19 arquivos, +4301, -4 |
| `3a16927a` | 2026-10-05 | `32cec08e`: marcador de alpha key | 1 arquivo, +9 |
| `acc48d24` | 2026-10-05 | `f03644f3`: `StgCol_TraceSphere`, segmento de scratch | 1 arquivo, +2, -3 |
| `420c66c3` | 2026-10-05 | `1276e7e2`: marcador em `ObjOutline_Draw` | 1 arquivo, +9 |
| `252b91d6` | 2026-10-05 | `4d12f124`: endereços DMA no PC (`DMA_PHYS`) | 2 arquivos, +13, -6 |
| `742ba9c9` | 2026-10-05 | `975ae18f`: gancho `REF_VU0_EXTERN_ARITH` | 1 arquivo, +6 |
| `8579253c` | 2026-10-05 | `72225f14`: `HudNode_Update` passa o nó | 1 arquivo, +1, -1 |
| `87454379` | 2026-10-05 | `4a1fa680`: declarações iguais às definições na ABI | 56 arquivos, +427, -145 |
| `51b27971` | 2026-10-05 | `9f6051fd`: literais de 64 bits em LL/ULL | 19 arquivos, +65, -65 |
| `fd0dd66d` | 2026-10-05 | `4d56fcb2`: controle de canal DMA por gancho | 2 arquivos, +16 |
| `787a773d` | 2026-10-05 | `8fbfbf92`: guardas PORT em COP1/VU0 | 2 arquivos, +10 |
| `08138459` | 2026-10-05 | `23b99fc4`: guardas PORT em espera de hardware | 2 arquivos, +6 |
| `3cc74256` | 2026-10-05 | `8ae6b811`: guarda PORT em `Dma_BeginDirect` | 1 arquivo, +5 |
