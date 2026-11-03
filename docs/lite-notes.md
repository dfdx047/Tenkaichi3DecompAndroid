# Dragon Rage Lite: notas de trabalho (2026-10-25)

Branch `lite`, a partir de `release-0.9.0`. Objetivo inicial: BT3 e BT4 a 1x e 30 quadros/s num Mali-G52 MC1.
Decisão do desenvolvedor (25/10): parar de perseguir os 30 no G52 MC1 e fechar um **Modo Lite** (`dr_lite`) no
mesmo APK, com o que foi medido e é seguro.

## Simulação do G52 MC1 no RG Cube (root)

`lowend.sh on 512`: Mali-G57 com 1 núcleo (`core_mask=1`) a 512 MHz, só os A55 (cpu0-3) ligados e presos em
1,846 GHz (os A76 desligados). Equivale mais ou menos a um RK3566 (G52 2EE a 800 MHz, 4x A55 a 1,8 GHz).
`gpu 512` limita só a GPU, `cpu` só a CPU. Luta gravada (`BT3_PAD_PLAY`, `bench.py`), BT3 a 1x, fx_off=48.

## BT3: números (velocidade da luta, 100% = 30 quadros/s)

| Configuração | Tudo limitado | Só GPU | Só CPU |
|---|---|---|---|
| VK, hoje (build 900) | 52,8% | 59,4% | 63,4% |
| GL, hoje | 52,0% | - | - |
| VK + thread de renderização | 59,0% | - | 78,5% |
| VK + cadeias de brilho puladas (`BT3_GLOW_SKIP=1`) | - | 73,7% (passes 58,8 → 27,4) | - |
| VK + thread + brilho pulado | **77,0%** (23 quadros/s) | - | - |
| VK + brilho pulado + SDL modificado | - | 74,3% | - |
| VK + SDL modificado (sem thread) | - | - | 70,3% (draws 14,3 → 11,0 ms) |

Onde está o tempo da GPU (só GPU limitada, brilho pulado, 73,7%, cerca de 45 ms por quadro):
- cópias DATE (14 por quadro): sem elas 88,7% (só teste, imagem errada): **cerca de 20%**, o maior item;
- sprites e camadas 2D: cerca de 13 ms; 3D (cerca de 1.960 draws de programas de vértice): cerca de 12 ms;
- custo fixo por draw na GPU: cerca de 2 µs (draw duplicado: 73,7 → 67,5%);
- shader de fragmento: shader trivial 73,7 → 79,5% (cerca de 8%); mediump no gs.frag: +3,7% (59,4 → 61,6);
- contorno e tintas (fx 1, 2 e 4): +8% (79,5%); página de sombra (`FX_SKIP=1`): +5%;
- área de render pass reduzida (gancho no loader, `BT3_VK_AREA=1`): +1 a 3% (59,4 → 61,2; 73,7 → 74,3).
  A quebra de pass por cópia DATE não custa pelo load/store, e sim pela serialização da GPU.

CPU (perfil simpleperf, luta com A55): da thread do jogo, 53% em libbt3 (Gs_Gif 12%, FPU do PS2 cerca de 30%),
26% no driver do Mali, 11% em libc e 5% no SDL. Draws vizinhos de modelos 3D alternam dois pipelines (A B A B:
bits de blend 1, 4 e 6 e escrita de alfa 19), em cerca de 1.960 de 2.076 pares. O SDL 3.4.12 recria e regrava 4
conjuntos de descritores a cada troca de pipeline: o patch `port/third_party/sdl3-patches/vulkan-keep-descriptor-sets.patch`
mantém os conjuntos quando o layout é o mesmo. O SDL é compilado do código (tag release-3.4.12, mesma revisão do
toolchain) em `port/build/sdl3-build`; `BT3_SDL3=port/build/sdl3-lite` o usa no build.

GL com framebuffer fetch e brilho pulado: 93,3% só GPU, mas 64,4% com tudo limitado (CPU: sem thread; a thread
de renderização no GL dá erro 0x502 e não chega à luta). O fetch já tinha dado imagem errada no G57
(device-lab): fica fora.

## BT4 (ARMSX2 VK, luta, atualizações da luta por segundo; 30 = normal)

| | Tudo limitado | Só GPU | Só CPU |
|---|---|---|---|
| renderizador ARMSX2 | 8,7 | 20,3 | 9,4 |
| renderizador Dragon Rage | 10,0 | 18,4 | 10,6 |

Limitado pela CPU (A55): seria preciso 3x. GameThread: dispatchGuestBranch 18%, `tlsdesc_resolver_dynamic` 13%
(thread_local numa biblioteca carregada por dlopen), getenv 3%. O agrupamento dos thread_local do despachante
(`port/bt4/patches/bt3recomp-lite.patch`, aplicado pelo `build_bt4.py`) compila, mas **não foi medido**.

## O que virou o Modo Lite (fechado em 25/10, ver docs/release-0.9.0.md, seção 9)

- `dr_lite` (Ajustes › Desempenho; o perfil de GPU o oferece para os chips fracos, `GpuProfiles.weakChip`): thread de
  renderização, cadeias de brilho e glare puladas quando estão em 0 (`Port_GlowSkip`), área de render pass só em
  Mali (o `GameActivity` passa a flag ao loader, que liga `BT3_VK_AREA`).
- SDL com os conjuntos de descritores mantidos entre pipelines do mesmo layout: vale para todos (empate no Adreno,
  0 pixels). Em 03/11 o patch virou `vulkan-descriptor-sets.patch` (pools de 1024 conjuntos, desenho pulado em vez de
  fechar o jogo quando não há conjunto; sem o interruptor `DR_SDL_KEEP_SETS`) mais `vulkan-fragment-set-memo.patch`
  (conjunto de texturas já escrito reaproveitado no mesmo command buffer: 4x4 do Tag Team 74% -> 86% no Adreno).
- Fora: TLS agrupado do BT4 (não medido; o patch está no commit dd4581d), GL com fetch, cópias DATE sem quebra de
  pass, shader mediump, ferramentas de medição (estão no dd4581d).

## Ferramentas de medição (só no commit dd4581d; tiradas da versão)

`BT3_DRAW_STAT`, `BT3_PASS_LOG`, `BT3_DATE_LOG`, `BT3_PIPE_STAT`, `BT3_GLOW_SKIP`, `BT3_FS_TRIVIAL`
(shaders/trivial.frag), `BT3_DRAW_DUP`, `BT3_VK_AREA`.
