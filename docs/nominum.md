# Nominum Render no Dragon Rage: notas e pedidos

O código do Nominum (`~/AndroidStudioProjects/Nominum-Render`) não é mudado daqui. O que o Dragon Rage
precisa dele vai como pedido nesta página.

## Estado

| Passo | Estado | Nota |
|---|---|---|
| 1. `libbt4gsx.so` do Nominum com o ARMSX2 por trás (`NOMINUM_GAME=none`) | feito, testado no RP5 sd480p | mesma imagem; luta 20,5 contra 22,1 atualizações/s com a lib do Dragon Rage (−7 %, uma rota cada); a passagem de cada pacote pelo Nominum custa ~7 % de um núcleo a mais |
| 2. Renderizador "Nominum" (backend próprio) como terceiro valor | pendente | |
| Dumps do GS do BT4 em `~/Downloads/gsdumps/bt4/` | pendente | |

## Pedidos ao Nominum

| # | Pedido | Por quê | Estado |
|---|---|---|---|
| N1 | `Bt4Gsx_SurfaceDestroyed` (`vk/armsx2_host.cpp`, e o `NominumArmsx2_SurfaceDestroyed` por trás) **só retornar depois que o GS largou o swapchain e a `VkSurfaceKHR`** (com limite de 2 s), como o `port/bt4/gsx/bt4_gsx.cpp` do Dragon Rage faz agora: a janela vira nula, `Host::RunOnCPUThread` → `MTGS::UpdateDisplayWindow()` → `MTGS::RunOnGSThread` marca "feito", e o laço do `live_next` atende a fila na hora enquanto alguém espera. O mesmo no backend próprio (`s_own`): destruir o swapchain dele antes de retornar | Bug #12 do BT4 (`VK_ERROR_NATIVE_WINDOW_IN_USE_KHR` e crash em `vkCreateAndroidSurfaceKHR` na volta do segundo plano). O host do Dragon Rage agora chama `SurfaceDestroyed`/`SurfaceChanged` de dentro do `surfaceDestroyed`/`surfaceChanged` do Android (o `SurfaceHolder.Callback` do `Bt4Activity`), com a superfície ainda viva: só adianta se a lib esperar | aberto |
| N2 | Exportar `Bt4Gsx_GsDump(const char* base, int frames)` (o `HL_GS_DUMP` do `hl_gsx.cpp` do Hidden Leaf; o Dragon Rage tem a mesma coisa no `bt4_gsx.cpp`) | Os dumps do BT4 para o próprio Nominum saem hoje só com a lib do Dragon Rage; com a do Nominum no APK, o `BT4_GS_DUMP` diz "this libbt4gsx.so has no Bt4Gsx_GsDump" | aberto |

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
