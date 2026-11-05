# Nominum Render no Dragon Rage: notas e pedidos

O código do Nominum (`~/AndroidStudioProjects/Nominum-Render`) não é mudado daqui. O que o Dragon Rage
precisa dele vai como pedido nesta página.

## Estado

| Passo | Estado | Nota |
|---|---|---|
| 1. `libbt4gsx.so` do Nominum com o ARMSX2 por trás (`NOMINUM_GAME=none`) | feito, testado no RP5 sd480p | mesma imagem; luta 20,5 contra 22,1 atualizações/s com a lib do Dragon Rage (−7 %, uma rota cada); a passagem de cada pacote pelo Nominum custa ~7 % de um núcleo a mais |
| 2. Renderizador "Nominum" (backend próprio) como terceiro valor | pendente | |
| Dumps do GS do BT4 em `~/Downloads/gsdumps/bt4/` | feito: 6 cenas | ver o histórico |

## Pedidos ao Nominum

| # | Pedido | Por quê | Estado |
|---|---|---|---|
| N1 | `Bt4Gsx_SurfaceDestroyed` (`vk/armsx2_host.cpp`, e o `NominumArmsx2_SurfaceDestroyed` por trás) **só retornar depois que o GS largou o swapchain e a `VkSurfaceKHR`** (com limite de 2 s), como o `port/bt4/gsx/bt4_gsx.cpp` do Dragon Rage faz agora: a janela vira nula, `Host::RunOnCPUThread` → `MTGS::UpdateDisplayWindow()` → `MTGS::RunOnGSThread` marca "feito", e o laço do `live_next` atende a fila na hora enquanto alguém espera. O mesmo no backend próprio (`s_own`): destruir o swapchain dele antes de retornar | Bug #12 do BT4 (`VK_ERROR_NATIVE_WINDOW_IN_USE_KHR` e crash em `vkCreateAndroidSurfaceKHR` na volta do segundo plano). O host do Dragon Rage agora chama `SurfaceDestroyed`/`SurfaceChanged` de dentro do `surfaceDestroyed`/`surfaceChanged` do Android (o `SurfaceHolder.Callback` do `Bt4Activity`), com a superfície ainda viva: só adianta se a lib esperar | aberto |
| N2 | Exportar `Bt4Gsx_GsDump(const char* base, int frames)` (o `HL_GS_DUMP` do `hl_gsx.cpp` do Hidden Leaf; o Dragon Rage tem a mesma coisa no `bt4_gsx.cpp`) | Os dumps do BT4 para o próprio Nominum saem hoje só com a lib do Dragon Rage; com a do Nominum no APK, o `BT4_GS_DUMP` diz "this libbt4gsx.so has no Bt4Gsx_GsDump" | aberto |
| N3 | BT4 no backend próprio: cores do cenário erradas (céu verde, chão azul com faixas) com personagens e HUD certos; ver as fotos acima e um dump de `~/Downloads/gsdumps/bt4/` | aberto |
| N4 | BT4 no backend próprio no Adreno 650 a 305 MHz: GPU 99 %, 8,2 atualizações/s (ARMSX2 22,1 com GPU 60 %) | aberto |

## Histórico

- 07:55 Página criada.
- 08:09 `libbt4gsx.so` do Nominum compilada pelo `tools/build_android.sh` dele, a partir de um `git archive` do `9940425`
  (em `~/DragonRageRefs/dia/nominum-src`, build em `~/DragonRageRefs/dia/nominum-build`; o repositório do Nominum tinha
  `core/backend.h` e `core/stream.cpp` mudados por outra sessão, e esse trabalho não entrou). 1818 passos, 2 min 40 s,
  35 983 488 bytes sem debug. Exporta os 6 `Bt4Gsx_*` que o `bt4_main.cpp` usa e mais `NativeOn`, `PresentRate`, `SetVideo`,
  14 `nominum_*`; nenhum `vk*` indefinido. **Não tem o `Bt4Gsx_GsDump`** do `hl_gsx.cpp` do Hidden Leaf.
- 08:40 ReadFIFO2 com PSMCT24 no backend próprio, lido no código (`9940425`): `vk/nominum_host.cpp:228` responde cada ReadFIFO2
  com `nominum_flush` + `nominum_read_local` no anel; `NominumBackend::finishReadback` (`vk/nominum_backend.cpp:2211`) lê cada pixel
  pelo `spsm` da transferência e grava `gsBitsPerPixel` bits: 24 para `PSMCT24`/`Z24`, em 3 bytes por pixel (o laço `b = 0, 8, 16`).
  Bate com o que o patch do ARMSX2 faz. A prova em tela é a rota do passo 2 (o fade do começo da luta e a luta seguem?).
- 08:40 O backend próprio já solta o swapchain dentro do `surface(NOMINUM_SURFACE_WINDOW)` ("the old one goes now"): o pedido N1
  vale para o caminho com o ARMSX2 por trás (`NominumArmsx2_SurfaceDestroyed`, que só põe na fila).
- 08:49 **Passo 1 no RP5 (sd480p segurando)**, `prof0nom.apk` (a lib do Nominum no APK por `port/bt4/gsx/use_nominum.sh`; o
  `bt4_main.cpp` põe `NOMINUM_GAME=none` antes do `dlopen`), renderizador ARMSX2, rota fixa do BT4 (História, Goku x Raditz):
  o log diz `hl_native: no game hooks (NOMINUM_GAME): every draw goes to the GS` e `bt4gsx: ARMSX2's GS, vulkan, 2x`; imagem igual à
  da lib do Dragon Rage (luta, HUD, contagem de golpes); 0 crash, 0 travada. Luta **20,5 atualizações/s** (mínimo 18,3) contra 22,1
  (mínimo 18,0) com a lib do Dragon Rage na mesma build; a thread que lê o anel (`CPU Thread`) 13 % contra 6 % de um núcleo
  (o `nominum_submit_gif`/`next_gif` de cada pacote); GPU 59 %. Comportamento igual; um pouco mais lento.

- 08:55 **Passo 2 no RP5**, `bt4_renderer=2` ("Nominum (teste)"), `prof0nom.apk`: o autoteste do Nominum escolheu o ARMSX2
  (`bt4gsx: Nominum's own backend did not start: ARMSX2's GS instead`) pela tabela de drivers: Adreno 650 com o driver da
  Qualcomm até 0746 está listado por `VK_ERROR_DEVICE_LOST` na campanha do Hidden Leaf. A rota rodou inteira no ARMSX2 (21,3
  atualizações/s, 0 crash; o "crash: 4" do resumo era o texto da tabela, que cita `DEVICE_LOST`: contagem do `bt4route.sh`
  corrigida). O valor 2 cai com segurança no ARMSX2 neste aparelho. A seguir, só como teste, a mesma rota com
  `NOMINUM_SELFTEST=force NOMINUM_SELFTEST_IGNORE_TABLE=1` (o veredito é apagado depois).
- 09:04 **Passo 2 forçado no RP5** (`NOMINUM_SELFTEST=force NOMINUM_SELFTEST_IGNORE_TABLE=1`, só teste; `selftest.txt` apagado
  depois, a próxima abertura volta a seguir a tabela): `nominum: self-test ran: Adreno (TM) 650: Nominum, alternatives none`, backend
  próprio, swapchain 1080x1920 girado 90. Rota inteira, **0 crash, 0 DEVICE_LOST** nesta rota (a tabela cita a campanha do HL).
  - **ReadFIFO2 com PSMCT24**: `[gslive] readback 344064 bytes` = 3 bytes por pixel (114688 pixels; em 32 bits seriam 458752).
    A primeira em 456,7 ms, a segunda em 4,4 ms. A luta começa e segue (o fade passa).
  - **Imagem errada no cenário**: céu verde e amarelo, montanhas e chão azuis e roxos com faixas e moiré; personagens, HUD e
    legendas certos (`nom2own_4fight.png`, `nom2own_5end.png` em `~/DragonRageRefs/dia/bt4/`). Cara de paleta (CLUT) ou de
    formato de textura do cenário, não de geometria.
  - **Lento**: luta **8,2 atualizações/s** (mínimo 7,4) com a GPU a 99 % (305 MHz), contra 22,1 do ARMSX2 e 25,9 do Dragon Rage na
    mesma rota. A CPU sobra (EE 18 %, GIF 26 %).

- 09:20 **Dumps do GS do BT4** em `~/Downloads/gsdumps/bt4/` (lib do Dragon Rage, renderizador ARMSX2, RP5, rota fixa; `zstd -t` ok em todos):
  `rp5_titulo` (120 vsyncs, 0,1 MB), `rp5_menu` (120, 63 MB), `rp5_selecao` (120, 3 MB), `rp5_luta_inicio` (400, 219 MB: o fade
  com o ReadFIFO2 PSMCT24), `rp5_luta` (300, 326 MB), `rp5_fim` (300, 325 MB). Formato do PCSX2 (`GSDumpBase::CreateZstDump`,
  serial SLUS-21978, estado de 4194813 bytes, foto 640x480). Para N3: o cenário de `rp5_luta` no backend próprio.
