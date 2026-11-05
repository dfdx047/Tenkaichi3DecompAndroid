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

## Gargalos por jogo (RP5 com `sd480p`)

Como se mede: BT4 com `port/tools/lab/bt4route.sh` e `bt4stats.py` (linhas `[logicrate]` e `[fps]` do log: atualizações da luta
por segundo, imagens por segundo, e o tempo do código do jogo, `guest_ms`, dentro de cada imagem, `wall_ms`), mais o `top -H` do
processo no meio da luta (CPU de cada thread: 100 = um núcleo). BT3 e Tag Team com `bt3route.sh` (o resumo do `bench.py`: "jogo" é
o código do jogo por imagem; "lista" é a lista de desenho, isto é, decodificar texturas, criar pipelines e enviar), e o mesmo `top -H`.
Os relógios do meio da luta ficam ao lado de cada medida.

| Jogo | Cena | Imagens/s | Luta/s | Código do jogo (EE) | VU1 | GS / GPU | Som | IOP | Gargalo |
|---|---|---|---|---|---|---|---|---|---|
| BT4, ARMSX2 (`armsx2b`) | Goku x Raditz (História), luta inteira | 29,4 (mín. 16) | **20,8** (mín. 18,2) | 30,9 de 58,3 ms por imagem (53 %); thread 52 % | kick worker 20 % | GS do ARMSX2 55 % (CPU), GPU 63 % ocupada a 305 MHz; GIF no runtime 28 %; leitura do anel 10 % | 2 % | 4 % | nenhuma etapa cheia: o tempo se perde em espera entre EE → VU1 → GIF → GS (nenhuma thread chega a 1 núcleo, GPU a 63 %). Custo fixo achado: diagnósticos do runtime ligados para todo jogador (item 5) |
| BT4, Dragon Rage (`dr`) | a mesma | 33,3 (mín. 21,6) | **25,9** (mín. 22,7) | 24,8 de 41,6 ms (60 %); thread 63 % | kick worker 31 % | **GPU 99 % ocupada** (305 MHz); o GS do Dragon Rage lê o anel na `SDLThread` (68 %); GIF no runtime 54 % | 1 % | 4 % | **GS/GPU** (GPU cheia) |
| BT4, ARMSX2 sem diagnósticos (`armsx2p0`) | a mesma | – | **22,1** (mín. 18,0) | 19,7 de 42,6 ms (46 %); thread 41 % | 11 % | GS do ARMSX2 53 %, GPU 60 %; GIF 19 % | 2 % | 3 % | nenhuma etapa cheia (espera entre etapas) |
| BT4, lib do Nominum + ARMSX2 (`nom1`) | a mesma | – | 20,5 (mín. 18,3) | 23,4 de 48,3 ms (48 %) | 24 % | GS 55 %, GPU 59 %; leitura do anel 13 % | 2 % | 4 % | como o ARMSX2, +7 % de um núcleo na leitura |
| BT4, Nominum próprio forçado (`nom2own`) | a mesma | 18,9 (mín. 3,9) | 8,2 (mín. 7,4) | 18,3 de 98,8 ms (18 %) | 8 % | **GPU 99 %** | 1 % | 2 % | **GS/GPU** (cores do cenário erradas) |
| BT3 (`bt3`, luta gravada `fight4.rec`) | menu, seleção, luta, vitória | 30,7 | **100 %** | jogo 6,1 de 10,1 ms de trabalho por quadro (orçamento 33,4) | – (no mesmo código) | lista 4,0 ms; GPU 22 % | – | – | nenhum: folga de 2/3 do quadro |
| Tag 5 x 5 COM x COM (`tag5`, `BT3_DEMO`) | luta | 20,7–21,9 | **69–73 %** | jogo ~11 ms | – | **GPU 99 %** a 305 MHz (na 2ª medida; a 1ª amostra, 9 %, caiu num intervalo); o "presenting" de 22–27 ms é o SDL esperando a GPU; 64 passes e 18 cópias por quadro (BT3: 45 e 3,7) | 1 % | – | **GS/GPU** (10 lutadores, 2x) |
| BT2 (`bt2`, Duel COM x COM, Goku x Kid Gohan, Wasteland) | título, menu, seleção, mapa, luta | 30 (o do jogo) | **100 %** (30 de 30) | thread do jogo (EE e GS por software no mesmo `SDLThread`) 63 % | – | fila de render do paraLLEl-GS (2º `SDLThread`) 22 %; **GPU 64 %** a 305 MHz | 1 % | – | nenhum (folga na CPU e na GPU) |

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
- 08:22 Item 2, Tag Team pelo ajuste do launcher (`dr_tag_team` em `bt3_settings.txt`, o mesmo que a chave do Ajustes grava;
  `port/tools/lab/tagtoggle.sh`), BT3 aberto pelo app: com 1, o SELECT no menu principal abre os "Modos modificados" (Batalha em
  Equipe, Todos Contra Todos, Treino, Desafio, Torneio, Sala Online, Configurações; textos em português, com letras); com 0, o SELECT
  não faz nada e o menu principal é o do jogo. 0 crash nos dois. O padrão continua 0 (`EngineSettings.TAG_TEAM_DEFAULT`, lido no
  `GameActivity.getArguments` e na chave do Ajustes); o teste deixa o RP5 com 0. Imagens em `~/DragonRageRefs/dia/tag/`.

- 08:24 **O perfil `sd480p` tinha se perdido** no RP5: os A77 (policy4) estavam em 2,15 a 2,42 GHz (o perfil põe teto de 1,77 GHz).
  Algo do sistema desfez o teto entre 07:54 e 08:24 (provavelmente o serviço de desempenho do aparelho ao abrir os jogos; os testes
  de BT3 das 08:09 a 08:23 podem ter rodado acima do sd480p, o que não muda as conclusões deles, que comparam builds entre si no
  mesmo estado). Reaplicado (agora com `sched_boost` 0 e piso de 710 MHz nos A77); a rota grava os relógios no meio da luta
  (`<nome>_clocks.txt`) para conferir cada medida.
- 08:29 BT4, rota com o ARMSX2 (`fix12.apk`, `bt4route.sh`): a sequência de teclas da `bt4fight.sh` hoje entra na História Dragón
  (o cursor do menu salvo mudou), e a luta é Goku contra Raditz; serve de rota fixa (menu, seleção, luta até o fim). Medida com os
  A77 **acima** do sd480p (2,15 a 2,42 GHz: o perfil se perde ao abrir o jogo) e a GPU a 305 MHz: luta 24,5 atualizações/s
  (mínimo 21,5), 24,9 imagens/s; o código do jogo é 21,5 de 48,8 ms por imagem (44 %). Threads (CPU, 100 = um núcleo): EE 53,
  GS do ARMSX2 51, leitura do GIF no runtime (`GsThread`) 33, VIF1/VU1 (o kick worker) 23, a thread de CPU do ARMSX2 8, IOP 4,
  som 2. 0 crash, 0 travada. Perfil reaplicado com `HL_STOP_PERF=1` (para os serviços de desempenho do aparelho; o `restore`
  liga de novo) e a rota medida de novo.
- 08:34 BT4 ARMSX2 de novo, **com o sd480p segurando** (A77 a 1,77 GHz no meio da luta, GPU 305 MHz e 63 % ocupada): luta
  **20,8 atualizações/s** (mínimo 18,2), 29,4 imagens/s; o código do jogo é 30,9 de 58,3 ms por imagem (53 %). Threads: GS do
  ARMSX2 55, EE 52, GIF no runtime 28, kick worker (VIF1/VU1) 20, leitura do anel 10, IOP 4, som 2. 0 crash, 0 travada.
  Nenhuma thread cheia e a GPU com folga: o BT4 espera entre as etapas, não num gargalo só do GS. O fps baixo dos aparelhos fracos
  (9 a 11) vem de EE + VU1 + GIF na CPU, não só do GS.
- 08:36 **Item 5, achado:** o `log_level` padrão (1) do runtime do BT3-Recomp (`main.cpp:723`) liga para todo jogador o
  `PS2X_PROFILE` (dois incrementos atômicos a cada desvio do código do jogo, 1,7 milhão por segundo na luta: as linhas `[prof]`),
  o `PS2X_SCHED_DEBUG` (linhas `[sched]`, `[mutex]`, `[wake]`: 2869 numa rota) e o `PS2X_MCLOG`. Desligados no
  `bt4_main.cpp` antes do runtime ler o nível (o `bt3_env.txt` ainda liga). Medida antes/depois a seguir.
- 08:39 BT4 com o renderizador Dragon Rage, mesma rota e perfil: luta **25,9 atualizações/s** (mínimo 22,7), 33,3 imagens/s; jogo
  24,8 de 41,6 ms por imagem; **GPU 99 % ocupada**: aqui o gargalo é o GS/GPU. Threads: leitura do anel e GS na `SDLThread` 68,
  EE 63, GIF no runtime 54, kick worker 31. 0 crash, 0 travada. Mais rápido que o ARMSX2 (20,8) no sd480p, com a imagem do
  Dragon Rage (falhas conhecidas em parte do cenário do BT4).

- 08:44 **Item 5, antes e depois (diagnósticos do runtime do BT4 desligados, `prof0.apk`)**, ARMSX2, mesma rota, sd480p
  segurando (GPU 60 %): luta **20,8 → 22,1 atualizações/s** (+6 %; mínimo 18,2 → 18,0), código do jogo por imagem
  **30,9 → 19,7 ms** (−36 %), thread do EE 52 → 41 % de um núcleo, GIF no runtime 28 → 19 %, kick worker 20 → 11 %;
  linhas `[prof]`/`[sched]` no log 2043 → 1. 0 crash. Fica.
- 08:55 BT4, lib do Nominum (passo 1, renderizador ARMSX2 por trás): luta 20,5 atualizações/s, mesma imagem, 0 crash. Valor 2
  (Nominum): o autoteste do Nominum manda o RP5 (Adreno 650, driver da Qualcomm) para o ARMSX2 pela tabela de drivers; rota
  inteira, 21,3/s, 0 crash. Detalhes em `docs/nominum.md`.

- 08:57 **Bug #12 com a correção** (`prof0.apk`, renderizador ARMSX2, `bt4bg.sh` 4 vezes: HOME e volta pelo launcher): o processo
  `:bt4` é o mesmo nas 4 voltas, a imagem volta (o filme de abertura seguia), 4 linhas `bt4gsx: surface gone: the swapchain was
  dropped first (4 a 11 ms)`, 0 `NATIVE_WINDOW_IN_USE`, 0 crash; START depois das voltas responde. A build de antes (`m3g.apk`) na
  mesma prova a seguir, para comparar.
- 08:57 Logs dos jogadores, `libbt4.so std::recursive_mutex::unlock` (moto g20, Android 11, 0.9.0; 3 vezes seguidas no boot):
  não é o mutex: `Cause: stack pointer is not in a rw map; likely due to stack overflow` na `GameThread`, com só 21 quadros
  (`iopImport` ← `sio2man` ← `SifLoadModule`, no carregamento dos módulos do IOP); o mesmo `sp` nas 3. Sem o aparelho não há
  como repetir; a hipótese é a pilha padrão de `std::thread` (1 MB no bionic) pequena para algum quadro grande nesse caminho. Não
  mudei a thread do runtime sem poder testar no aparelho que falha. Anotado como aberto.
- 08:59 **Bug #12, antes e depois na mesma prova** (`bt4bg.sh`, 4 voltas, renderizador ARMSX2): build de antes (`m3g.apk`):
  `VK_ERROR_NATIVE_WINDOW_IN_USE_KHR` no `vkCreateSwapchainKHR` e no `vkCreateAndroidSurfaceKHR`, "Failed to recreate surface
  after loss", **tela preta nas 4 voltas** (brilho médio 1 de 255) e depois das teclas; o processo não cai, mas o jogo some.
  Com a correção (`prof0.apk`): imagem nas 4 voltas, 0 erro de superfície. **#12 corrigido** (`c94be361`).
- 09:14 **BT3 no sd480p** (rota gravada, perfil segurando): 100 % de velocidade, 30,7 imagens/s, trabalho 10,1 ms por quadro de 33,4,
  GPU 22 %, 0 crash. Nada limita o BT3 no RP5 assim.
- 09:14 **Tag 5 x 5** (`BT3_TAG_TEAM=1 BT3_TAG_ROSTER=5,5 BT3_DEMO=1`): **69–73 %**, 21 imagens/s, 0 crash. O tempo vai em "presenting"
  (`gs_gpu.c:2539`: só a chamada `SDL_SubmitGPUCommandBuffer`, modo mailbox), 22–27 ms por quadro contra 1,7 no BT3, com a GPU a 9 % na
  amostra do meio. O Tag faz 18 cópias e 64 passes por quadro (BT3: 3,7 e 45). Hipótese, não provada: o SDL faz trabalho dentro do
  submit (desfragmentação da memória do Vulkan, liberações pendentes) por causa das imagens criadas e soltas a cada quadro. A rota do
  host do Tag (`chain5`) errou o caminho no menu (o SELECT não abriu os modos); refaço passo a passo.
- 09:14 Corrigido no `bt3route.sh`: o `top` media o processo do app, não o `:game` (as linhas de thread do `bt3` saíram vazias).

- 09:15 Tag 5 x 5, o que o `SDL_SubmitGPUCommandBuffer` faz (`port/build/sdl3-src/src/gpu/vulkan/SDL_gpu_vulkan.c:10816`): pega o
  `submitLock`, desfragmenta a memória se há alocações marcadas (`VULKAN_INTERNAL_DefragmentMemory`, que copia recursos), `vkQueueSubmit`,
  `vkQueuePresentKHR`, e limpa os buffers de comando prontos e as alocações vazias (`checkEmptyAllocations`). Qual dessas leva os 22–27 ms
  só se vê medindo dentro do SDL (recompilar o `sdl3` com tempos). Ficou para depois do BT2; o Tag é desligado por padrão.
- 09:29 **BT2 no sd480p**: a ISO não estava no RP5; copiei para `/sdcard/Download` e escolhi em "Selecionar ISO" (o app só guarda o
  caminho). Rota por teclas (`bt2key.py`): título → New Game → "No" e "Yes" para jogar sem salvar → Main Menu (2 para baixo:
  Dragon Tournament, mais 1: Dueling) → COM VS COM (1 para baixo; 1P vs 2P fica cinza) → Single Battle → 6 X (cores, mapa
  Wasteland) → luta. Luta a 30 imagens/s (o jogo é 30), GPU 64 %, 2 threads (63 % e 22 % de um núcleo), A77 a 1,77 GHz. 0 crash.

- 09:31 BT2: a luta COM x COM terminou sozinha (Goku venceu; tela "Fight Again / Return to…", `end_now.png` em
  `~/DragonRageRefs/dia/bt2/`). Rota completa: menu, seleção, luta, vitória. O laço de fotos foi parado pelo Claude Code por
  falta de memória no PC (não no RP5); a foto da vitória foi tirada à parte.
- 09:37 **Tag 5 x 5, de novo, com `BT3_GS_THREAD=0`** (a thread de render liga por padrão com o Tag no Android, `gs_core.c:1440`):
  72,6 % (igual), "presenting" 15–18 ms, **GPU 99 %** a 305 MHz. Corrige a hipótese das 09:1x: não é trabalho dentro do SDL, é a
  GPU cheia; o submit espera o quadro anterior sair da GPU. A thread de render não ganha nem perde aqui. Gargalo do Tag 5 x 5 no
  sd480p: GS/GPU (10 lutadores, 2x, 18 cópias por quadro). Não é dos gargalos fora do GS do item 5.
- 09:37 **Tag, menu do anfitrião sem letras: não se repete.** RP5, português, Tag ligado pelo launcher: SELECT → Sala Online →
  Criar uma Sala → ENTER na porta → sala com código (903258), "CONECTADO J1 - Anfitrião", todos os modos com texto → Batalha em
  Equipe: as 10 placas com nome e "CPU"/"J1 (VOCÊ) - ESCOLHENDO", botões, "0/1 prontos | 0/10 assistindo" → START em pronto → a
  luta começa (5 x 5 em Namek). Fotos `h5`–`h8` em `~/DragonRageRefs/dia/tag/`. Corrigido por `ba41fe09` (01/11: os trabalhos de
  letras parados eram soltos depois de 30 quadros; antes, os 8 ficavam presos e toda linha nova ficava vazia). Sobra: o título
  "Choose teams. Press Start to take a place." em inglês na tela em português.
- 09:37 **Tag, memória**: a região de 160 MB do port chega a **128 MB** no começo da primeira luta online (5 x 5, sala do anfitrião);
  99 MB na luta 5 x 5 de teste e 112 MB no BT3 normal (rota gravada). O `exit(2)` do `heap.c:244` vem quando ela acaba; uma 2ª luta
  online seguida é o teste que falta (cada luta 5 x 5 leva minutos com COM).

- 09:50 Tag, memória em lutas seguidas (`~/DragonRageRefs/dia/tagmem.sh`, sala do anfitrião): **inconclusivo**. Só a 1ª luta online
  correu; a luta da sala não escreve "battle finished" e as teclas depois dela levaram a uma seleção normal (`mem_room3.png`). A região
  ficou em 128 de 160 MB durante os 10 min (é o ponto mais alto já entregue: não sobe se a luta reusa os blocos). Falta: duas lutas
  online seguidas com dois aparelhos (o Cube e o Odin são da outra sessão) ou um roteiro da volta à sala depois da luta.
- 09:51 Item 7, conferido no APK do dia (`prof0.apk`): `assets/licenses.txt` (156560 bytes, 27 seções: Dragon Rage, SDL3, sse2neon,
  FSR1, SGSR, créditos das telas online, Nominum/PCSX2/ARMSX2/BT3-Recomp/dbz-bt4-recomp/FFmpeg/BT2 Recomp, rcheevos, GPL-3, LGPL-2.1, astcenc,
  fontes, imgui, libadrenotools, miniaudio, miniz, monocypher, newlib, pl_mpeg, xxhash); as licenças das fontes do ARMSX2 junto delas.
- 09:55 **BT4, onde o jogo espera**: `[gslive] … waited for the host ~690 ms/s (ring full 0.0)` (`ps2_gs_live.cpp:686-713`): a cada vsync o
  jogo espera o host ter lido o anel até 2 vsyncs atrás (`PS2X_GSLIVE_MAXLAG`, padrão 2); o host (CPU Thread do ARMSX2 → MTGS com fila de
  2 quadros → thread GS, 53–55 % de um núcleo, ~27 ms de CPU por quadro → GPU 54–60 %) anda a ~20–24 vsyncs/s. Teste com `MAXLAG=4`
  (`lag4`): **22,3** atualizações/s contra 22,1, a espera continua em ~700 ms/s. Folga no anel não ajuda: o limite é a etapa do GS (thread GS
  do ARMSX2 + GPU em série), não o EE, o VU1 nem o GIF. Fica o padrão. O que sobra do BT4 no sd480p é GS (Nominum, itens 4/N4).
- 10:08 **Tag, mensagens da sala em inglês: corrigido.** 45 mensagens do `tag_net.c` (status da sala, entradas e saídas, erros da
  partida) não tinham linha em `port/translations/tag/pt.tsv` nem `es.tsv`; o texto do Tag já passa por `Port_TagText`
  (`port_screen.c:351`). Acrescentadas nas duas e geradas com `make_tag_text.py` (855 linhas, `--check` ok, argumentos de formato iguais).
  Só texto. Prova (`tagtext.apk`): BT3 100 %, 30,7 imagens/s, 0 crash, Dragon Net com código ZFCHNZ, **fightcmp: SAME em 6212 quadros**
  contra o merge 3; sala do anfitrião com "Escolha as equipes. Aperte Start para pegar uma vaga." (`~/DragonRageRefs/dia/tag/t2.png`).
- 10:31 **Logs dos jogadores, #15 `VULKAN_INTERNAL_PrepareTextureSubresourceForWrite`: 1 dos 2 corrigido.** Log de 26/10 (Xclipse 530,
  interp=2): "internal resolution 1.25x" e o crash na linha seguinte, em `interp_show` → `vk_frame_end` → `SDL_BeginGPURenderPass`.
  Causa: no ramo `repeat` de `GsGpu_FrameEnd` (tique sem quadros intermediários, movimento suave ligado), `scale_apply()` solta todos os
  alvos e zera `gsTargetCount` e, logo depois, o ramo arma `sInterpRepeat`, que redesenha a lista guardada sobre os alvos soltos.
  Correção: o tique não repete quando a resolução muda depois dele. Teste novo `BT3_SCALE_FLIP=N` (troca 1x/1,25x a cada N quadros).
  RP5 sd480p, luta gravada com interp=2 e FLIP=7: **antes (flip0.apk) crash na 2ª troca** (signal 5 no vulkan.adreno.so); **depois
  (flip1.apk) 767 trocas, 0 crash**, luta a 100 %. Prova do dia: BT3 100 %, 30,7 imagens/s, 0 crash, Dragon Net com código F3EJHJ, fightcmp SAME
  em 6212 quadros. O outro log (Adreno 630, dev.54, interp=0, ao sair de uma partida online a 1765 %) é outro caminho e continua aberto.
