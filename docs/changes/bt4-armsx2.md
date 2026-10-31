# BT4, renderizador ARMSX2

Regras e formato: [README](README.md).


## Histórico: 0.9.0 para trás

Linhas: lado novo do diff naquele commit; o arquivo de hoje tem outra numeração. Fontes: mensagens de commit, diffs, `docs/device-lab-2026-10.md`,
`docs/release-0.9.0.md`, `port/bt4/README.md`, `port/bt4/gsx/cache/README.md`. Mais novas primeiro.

### 0.9.0 sem commit: ARMSX2 em OpenGL no Adreno 650 faz 6 a 9 atualizações/s (aberto)
- **Onde:** `port/bt4/host/bt4_main.cpp` (`gpu_api`, `sRenderer`); `data/Renderer.kt`; `docs/release-0.9.0.md`, seção 5.
- **O que e por quê:** RP5, BT4, OpenGL ES, ARMSX2, Duelo: a luta roda a cerca de 25% da velocidade. No Cube o mesmo caminho faz 28,6, e em Vulkan, o padrão, o RP5 faz 30. Não deu para saber se é anterior: o APK de antes da mescla não abre o BT4 com o launcher atual. O "ARMSX2, GL: OK" do device lab no RP5 provavelmente era Vulkan, porque o launcher reescreve `gpu_api` a partir de `dr_renderer` antes de abrir, e o `[vstatus]` do ARMSX2 diz "configured OpenGL" mesmo assim.
- **Medido/provado:** duas vezes: 8,9 e de 6,0 a 7,3 atualizações/s.
- **O que pode quebrar:** quem escolhe OpenGL no Adreno. A sugestão do relatório (esconder a opção ou avisar) não foi feita; nem o launcher nem `bt4_main.cpp` têm hoje condição para Adreno, e o único commit em `port/bt4` depois da 0.9.0 é `a974f119` (shim).
- **Como testar de novo:** RP5, Ajustes → OpenGL ES, não `gpu_api` à mão; `bash port/tools/lab/series_bt4_rp5.sh <versão>`.

### 0.9.0 d6467b24: pacotes de textura no ARMSX2
- **Onde:** `port/bt4/patches/armsx2-bt4.patch`, 118-142 (`BT4GSX_TEXTURES`); `port/bt4/host/bt4_main.cpp` `gsxWriteIni`, 554-581.
- **O que e por quê:** o ARMSX2 procurava em `bt4/gsx/ARMSX2/textures/SLUS-21978/replacements`, com `LoadTextureReplacements = false`. Agora lê `bt4/textures`, e liga as substituições só com pacote instalado; sem pacote, o ini é o mesmo de antes.
- **Medido/provado:** Cube com o pacote do BT3: 18.597 indexadas, 28,7 atualizações/s. Não medido no RP5.
- **O que pode quebrar:** o ARMSX2 reservou até 5,3 GB para texturas descompactadas (metade da RAM). Com um pacote feito para o BT4, a memória numa luta longa não foi medida.
- **Como testar de novo:** pacote em `bt4/textures`, Duelo com o ARMSX2; conferir a memória numa luta longa.

### 0.9.0 1bc64e27: ARMSX2 é o padrão
- **Onde:** `port/bt4/host/bt4_main.cpp` `SDL_main`, 894-896, e `menuSnapshot`, 395-396; `data/EngineSettings.kt`, 74; `ui/screens/SettingsScreen.kt`, 110-115; `engine/GameMenu.kt`, 567-570.
- **O que e por quê:** a imagem mais fiel, e o renderizador Dragon Rage tem o ruído no cenário distante. Dragon Rage fica como "experimental", com uma linha que explica a diferença nas três línguas. Build sem `libbt4gsx.so` cai no Dragon Rage.
- **Medido/provado:** Cube VK 28,5 (2 compilações na luta), RP5 VK 30,0 (0 compilações); Cube com MaliSX2 29,8; Dragon Rage: Cube VK 29,4 e GL 29,7, RP5 30,0.
- **O que pode quebrar:** o valor padrão de `bt4_renderer` em três lugares: `bt4_main.cpp`, `menuSnapshot` e o launcher.
- **Como testar de novo:** instalação limpa: o log diz "started (renderer 1, BT4 renderer ARMSX2...)" (`d138e3a5`).

### 0.9.0 b94a6faf: cache de SPIR-V no APK
- **Onde:** `port/bt4/gsx/merge_cache.py`, novo; `port/bt4/gsx/cache/README.md`; `port/bt4/build_bt4.py` `armsx2`, 253-261; `port/bt4/gsx/bt4_gsx.cpp`, 360-362.
- **O que e por quê:** os cerca de 25 s de tela preta da primeira abertura no Cube eram o cache vazio mais o boot do jogo. O APK traz os caches do Cube (275) e do RP5 (235), juntados em 504 entradas e 1,5 MB. As chaves dependem da GPU: só 6 são comuns. `patches.zip` vazio tira o aviso do OSD. Sem tela de "preparando shaders", porque os cerca de 30 s até a página laranja são o boot e as logos.
- **Medido/provado:** Cube. Sem cache: primeira imagem cerca de 4,5 s, 81 compilações numa sessão de luta. Cache do APK: cerca de 2,5 s, 0 na abertura, 1 numa luta inteira.
- **O que pode quebrar:** o carimbo no nome muda com a revisão do ARMSX2, o shaderc ou os shaders do patch: refazer o cache. Outra GPU compila o que não acha.
- **Como testar de novo:** apagar `bt4/gsx/ARMSX2/cache`, abrir e contar as compilações no log. Refazer: `python3 port/bt4/gsx/merge_cache.py` (README da pasta).

### 0.9.0 eefe5d9b: medidor de FPS conta as imagens do jogo
- **Onde:** `port/bt4/patches/armsx2-bt4.patch`, 14 trechos entre 2 e 310.
- **O que e por quê:** os vsyncs do stream ao vivo não traziam escrita de registro, e o OSD dizia "FPS: N/A". Um vsync conta como quadro quando DISPFB1/2 mudou, ou, num jogo que não troca de buffer, quando algo foi desenhado.
- **Medido/provado:** não registrado no commit.
- **O que pode quebrar:** o valor de FPS mostrado, não a imagem.
- **Como testar de novo:** ligar o medidor de FPS no menu e ver o número numa luta.

### 0.9.0 4c8794ef: readback de PSMCT24 a 3 bytes por pixel
- **Onde:** `port/bt4/patches/bt3recomp-android.patch`, 70-93 e 136-152; `port/bt4/patches/armsx2-bt4.patch`, 119 e 222-232; `port/bt4/gsx/bt4_gsx.cpp`, 356; `port/bt4/host/bt4_main.cpp`, 805-820.
- **O que e por quê:** o jogo travava no início da luta. O readback escrevia 4 bytes por pixel (458.752) num buffer de 0x60000, passava do fim, estragava o cabeçalho do bloco seguinte do heap e o jogo ficava em loop em 0x255018. O host agora responde ao readback (`consumerFlags` bit 1). Sem o OSD de dump ao vivo.
- **Medido/provado:** readback de 344.064 bytes em 2 a 14 ms no RP5 e 3 a 12 ms no Cube; luta a 30 no RP5 e 28,8 quadros/s no Cube.
- **O que pode quebrar:** fade do início da luta; outro formato de readback.
- **Como testar de novo:** `bash port/tools/lab/bt4fight.sh`: a luta tem de começar.

### 0.9.0 950764a1: núcleo de páginas de 4 KB
- **Onde:** `port/bt4/build_bt4.py` `armsx2`, 201-202 e 216; `port/bt4/gsx/bt4_gsx.cpp` `Bt4Gsx_PageSize`, 313-318; `port/bt4/host/bt4_main.cpp` `runArmsx2`, 534-543 e 640-645.
- **O que e por quê:** o núcleo é feito para um tamanho de página e não inicia em outro ("Page size mismatch"). Fica em 4 KB, o dos celulares. Em aparelho de 16 KB o jogo fica no renderizador Dragon Rage. Se o ARMSX2 parar, uma janela manda escolher o outro.
- **Medido/provado:** não registrado; nenhum aparelho de 16 KB nas fontes.
- **O que pode quebrar:** aparelho de 16 KB cai sem aviso no renderizador com ruído.
- **Como testar de novo:** o log diz "built for %d-byte memory pages" num aparelho desses.

### 0.9.0 c9e77091: o renderizador entra
- **Onde:** `port/bt4/gsx/bt4_gsx.cpp` e `bt4gsx.cmake`, novos; `port/bt4/patches/armsx2-bt4.patch`, novo; `port/bt4/host/bt4_main.cpp` `runArmsx2`; `port/src/gs/gs_input.c`.
- **O que e por quê:** o GS do ARMSX2 (PCSX2 com as correções para Mali e Adreno) lê o mesmo stream ao vivo que o GS do Dragon Rage, em `libbt4gsx.so`, no processo do jogo.
- **Medido/provado:** device lab: Cube VK 28,8 quadros/s; Cube com MaliSX2 29,9, GPU de 17 a 23% contra 39 a 41% no driver do sistema; RP5 a 30.
- **O que pode quebrar:** cada mudança no stream ou no `ps2_live_ring.h` afeta os dois renderizadores. Revisão fixada: ARMSX2 `90126bdfea`.
- **Como testar de novo:** `python3 port/bt4/build_bt4.py --armsx2`; `series_bt4_rp5.sh`, `series_bt4_cube.sh`.
