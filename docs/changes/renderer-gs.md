# Renderizador GS (Dragon Rage)

Regras e formato: [README](README.md).


## Histórico: 0.9.0 para trás

Linhas: lado novo do diff naquele commit; o arquivo de hoje tem outra numeração. Fontes: mensagens de commit, diffs,
`docs/device-lab-2026-10.md`, `docs/lite-notes.md`, `docs/release-0.9.0.md` e os changelogs de cada tag.
Mais novas primeiro. "Não registrado" quer dizer que nenhuma dessas fontes traz o dado.

### 0.9.0 2d6d0531: Modo Lite: thread, cadeias de brilho puladas, área de render pass no Mali
- **Onde:** `port/src/gs/gs_draw.c` `Port_GlowSkip`, linhas 167-175; `src/sys/gfxm_a.c` `GfxPost_DrawGlow`, 637-638; `port/src/gs/gs_core.c` `render_thread_wanted`, 1423; `android/app/src/main/cpp/loader.c` `SDL_main`, 678-681; `engine/GameActivity.kt` `getArguments`, 445-452. As medições e sondas ficaram só em `dd4581d7` (`BT3_DRAW_STAT`, `BT3_PASS_LOG`, `BT3_DATE_LOG`, `BT3_PIPE_STAT`, `BT3_FS_TRIVIAL`, `BT3_DRAW_DUP`).
- **O que e por quê:** o pedido era 30 quadros/s a 1x num Mali-G52 MC1. Em 25/10 o desenvolvedor decidiu parar de perseguir os 30 e fechar o que estava medido. `dr_lite=1`: thread de renderização; cadeia de brilho e de reflexo do céu puladas quando a força delas é 0; área de render pass reduzida, só em Mali e só no BT3. Com `dr_lite=0` o motor segue o caminho anterior.
- **Medido/provado:** G52 MC1 simulado no Cube (`lowend.sh on 512`), VK 1x, `fx_off=48`: 52,8% → 77,0%. Cube VK 2x: 94,8% → 100%, present 18,6 → 2,9 ms. RP5 sem mudança. Pixels (quadros 6300 a 7350): Lite desligado contra o APK 900, 0 nos dois aparelhos. Brilho e reflexo em 0, Lite desligado contra ligado: 0 em 7 quadros; no quadro 7350, 11 pixels com 1 nível, vindos do passe que soma o brilho com força 0. Área de render pass: +1 a 3%.
- **O que pode quebrar:** Lite em OpenGL: `lite-notes.md` diz que a thread no GL dá erro 0x502 e não chega à luta; por isso `render_thread_wanted` recusa com `gpu_api=1`. Essa recusa olha o ajuste, não o renderizador que abriu: Vulkan recusado pelo driver cai em GL na thread (ver Bateria). `Port_GlowSkip` só vale com força 0: subir o brilho volta a desenhar a cadeia.
- **Como testar de novo:** `bash port/tools/lab/series_low.sh <versão>`; `compare_env.sh` com e sem Lite; `python3 port/tools/lab/ppmcmp.py`. Com Lite e MaliSX2 no Cube: a luta tem de abrir.

### 0.9.0 01224d08: bits de pipeline remanejados na mescla, cache novo
- **Onde:** `port/src/gs/gs_gpu.c` `pipeline_create` e `pipe_depthless`; `port/src/gs/gs_gl.c` `pipe_decode` (`key >> 27`), 487, e `set_pipe`, 706; cache `bt3_pipelines2.txt`.
- **O que e por quê:** o PC passou a usar o bit 25 (gswb, glow na resolução do PS2) e o 26 (sem byte de alfa), que eram nossos. Os nossos foram para 27 (sem discard) e 28 (sem profundidade). O nome do cache mudou porque os bits antigos significavam outra coisa.
- **Medido/provado:** antes → depois, luta gravada: Cube VK 2x 86,8% → 95,0%; Cube GL 2x 92,3% → 97,7%; RP5 VK 4x 79,8% → 83,5%; RP5 VK 2x 100% → 99,7%. Pixels: Cube VK 1x, 0 nos 8 quadros. RP5 VK 2x: 169 mil a 774 mil por quadro, quase todos de 1 a 16 níveis, atribuídos às mudanças intencionais do PC (halos de glow e bordas).
- **O que pode quebrar:** qualquer mescla futura do PC que use bits acima de 24; os dois renderizadores têm de decodificar o mesmo bit. Detalhes da mescla em [`merges-pc.md`](merges-pc.md).
- **Como testar de novo:** `bash port/tools/lab/series_base.sh <versão> cube`, depois `rp5`; `ppmcmp.py` contra a referência.

### 0.9.0 23fa0efe: Vulkan recusado não deixa janela para trás
- **Onde:** `port/src/gs/gs_gpu.c` `vk_init`, linhas 595-599 e 602-607.
- **O que e por quê:** sem dispositivo Vulkan a janela era criada mesmo assim, e o GL de reserva não conseguia criar a sua (o Android só tem uma). O jogo rodava com tela preta. Visto com MaliSX2 0.2.3 no Mali-G57, que o SDL_GPU recusa.
- **Medido/provado:** Cube com MaliSX2: GL 2x a 90,3% no device lab e 97,7% no relatório da 0.9.0, sem tela preta.
- **O que pode quebrar:** qualquer driver que o SDL_GPU recuse; a ordem de `SDL_DestroyWindow` e `SDL_DestroyGPUDevice` no caminho de erro.
- **Como testar de novo:** MaliSX2 no Cube, BT3 em Vulkan: tem de cair em GL com imagem. Voltar ao driver do sistema.

### 0.9.0 2e894c0c: upload de 24 bits marca todas as páginas
- **Onde:** `port/src/gs/gs_core.c` `transfer_data`, linhas 780-785.
- **O que e por quê:** PSMCT24, 8H, 4HL e 4HH ocupam 32 bits na memória, mas o rastreio de páginas contava pelos bits do formato. As últimas páginas do retângulo não eram marcadas, e uma textura ali podia ficar velha. Achado ao investigar o ruído do BT4, que não resolveu.
- **Medido/provado:** BT3 depois da mudança: 100%, 55,9 quadros/s, igual a antes.
- **O que pode quebrar:** cache de texturas: mais páginas marcadas, mais recarga. Formatos de upload novos precisam entrar na condição de `mbits`.
- **Como testar de novo:** `series_base.sh` e `ppmcmp.py`.

### 0.9.0 c2b26ba6: passes sem profundidade
- **Onde:** `port/src/gs/gs_gpu.c` `pipeline_create`, 337-342; `pipe_depthless`, 407-420; `vk_frame_end`, 1518-1550 e o bind de pipeline. `port/src/gs/gs_gl.c` `target_ensure`, `bind_target` e `frame_end` (cerca de 1355-1374).
- **O que e por quê:** são cerca de 60 passes por quadro, e o Mali carregava e gravava a profundidade em todos. Um pass cujos desenhos não testam nem escrevem profundidade agora roda sem ela. O bit era o 26 aqui; hoje é o 28.
- **Medido/provado:** Cube VK 2x, com profundidade 73,3% → sem 86,8%, present 26,9 → 20,9 ms. Cube VK 3x 39,0% → 48,2%. RP5 VK 4x 76,0% → 79,7%. RP5 GL 4x 54,6% → 60,3%. Cube GL 2x 92,5% com e sem: o driver já fazia isso sozinho. Pixels: 0 no Cube a 1x, VK e GL.
- **O que pode quebrar:** efeito que passe a usar profundidade num pass classificado como sem ela. O primeiro pass de um alvo mantém a profundidade porque a limpa. Com a tabela de 1024 pipelines cheia, `pipe_depthless` devolve o pipeline com profundidade.
- **Como testar de novo:** `compare_env.sh` com `BT3_GPU_DEPTHLESS=0`; ajuste `depthless_passes=2`. O log diz a linha "render passes without the depth buffer".

### 0.9.0 c2b26ba6: stencil da sombra: refs por sequência, desligado no Adreno
- **Onde:** `port/src/gs/gs_gpu.c` `pipeline_create`, 319-322; `vk_init`, 574-592; `vk_frame_end` (`shadowRef`, `passStencil`), 1490-1493 e 1518-1550.
- **O que e por quê:** o D32S8 veio com a 0.6.0 (`7b99f8ad`, do PC) e foi a causa principal da regressão do Vulkan no Adreno. A primeira versão sem quebra de pass derrubou o RP5; a final usa uma referência por sequência de sombras e limpa o stencil só nos passes que desenham sombra plana.
- **Medido/provado:** RP5 VK 4x: 70,8% → 76,0% desligando só o stencil. Primeira versão, stencil por desenho: 44,8%. Mali-G57: sem diferença (73%). Base (`950764a1`) contra final: 70,8% → 79,7%.
- **O que pode quebrar:** no Adreno, partes sobrepostas de uma sombra plana podem sair mais escuras (como antes da 0.6.0). Passam de 255 sequências num pass: quebra o pass.
- **Como testar de novo:** `BT3_GPU_NO_STENCIL=1`, ajuste `shadow_stencil` (0 automático, 1 ligado, 2 desligado), `BT3_GPU_SHADOW_BREAK=1`. O log diz "shadow stencil off".

### 0.9.0 c2b26ba6: GL com anel persistente também no Adreno
- **Onde:** `port/src/gs/gs_gl.c` `ubo_persist_init` (`c2b26ba6`, 581-583); o anel vem de `9ecce978`, linhas 543-606 e 619-639.
- **O que e por quê:** `9ecce978` trouxe o anel de `EXT_buffer_storage` só para Mali, como a tabela de drivers do ARMSX2. No Adreno 650, o map por desenho travava o driver: GL 2x levava cerca de 4 s por quadro.
- **Medido/provado:** RP5 GL 2x: cerca de 4 s por quadro → 100%, 14 ms de trabalho. RP5 GL 4x sem profundidade: 60,3%. Cube GL 2x, com e sem persist: 92,5% e 93,8%, ruído.
- **O que pode quebrar:** GL em qualquer Adreno e Mali; a espera de fence quando o anel dá a volta; driver sem a extensão cai no órfão por quadro, e o log diz qual.
- **Como testar de novo:** `compare_env.sh` com `BT3_GL_PERSIST=0`; ajuste `gl_persist` (0 automático, 1 sempre, 2 nunca).

### 0.9.0 9ecce978: ADPF e modo sustentado
- **Onde:** `port/src/gs/gs_adpf.c`, novo, linhas 1-91; `port/src/gs/gs_core.c` `Port_GsVif1Chain`, 1326-1329; `engine/GameActivity.kt` (`sustained`), 71 e 78-84.
- **O que e por quê:** o governador vê uma thread que trabalha e dorme, e baixa o clock; o próximo quadro pesado sai lento. O jogo passa a informar o trabalho por quadro, como o ARMSX2. Carregado por nome de `libandroid.so`, para abrir no Android 10 a 12.
- **Medido/provado:** só contra a regressão, não como ganho. RP5 VK 4x com ADPF desligado: 75,8%, dentro do ruído, então não é a causa do Adreno. Ganho em luta longa com aquecimento: não registrado.
- **O que pode quebrar:** alvo fixo de 33,3667 ms mesmo com 60 fps; sessão recusada pelo sistema só escreve no log.
- **Como testar de novo:** `BT3_ADPF=0` ou ajuste `adpf=0`. O log diz "performance hints".

### 0.9.0 c2b26ba6: DATE no Mali: o que foi medido e não entrou
- **Onde:** `port/src/gs/gs_gpu.c` `vk_frame_end`, 1340-1365 (cópia só com o que copiar); `port/src/gs/gs_gl.c` `gl_init` (fetch desligado). DATE com stencil e fetch não entraram.
- **O que e por quê:** as cerca de 14,5 cópias por quadro são o que sobra do gargalo do Mali. Fetch desenha errado. `SetupDATE` do PCSX2 também fecha o pass, então não daria o ganho; manter o bit de alfa num bit de stencil ao desenhar foi julgado grande e arriscado demais para a 0.9.0.
- **Medido/provado:** Cube VK 2x sem cópias (só teste, imagem errada): 99,0%, present 20,9 → 11,1 ms. Cube GL 2x com fetch: 100%, cópias 14,5 → 1, mas 15 mil a 174 mil pixels diferentes por quadro e blocos do tamanho de um tile. G52 simulado, só GPU: sem cópias 88,7% contra 73,7%, cerca de 20%, o maior item. A quebra de pass custa pela serialização da GPU, não pelo load/store.
- **O que pode quebrar:** `BT3_GPU_DATE_BREAK` e `BT3_GPU_NO_DATE_COPY` são só para teste. Barras do HUD e camadas do cenário dependem da cópia.
- **Como testar de novo:** `compare_env.sh` com `BT3_GPU_NO_DATE_COPY=1` para ver o custo; a imagem sai errada de propósito.

### 0.7.2 02338589: ASTC desligado por padrão
- **Onde:** `port/src/gs/gs_draw.c` `gGsAstcOn`, 98, e `GsGpu_Init`, 2015; `ui/screens/SettingsScreen.kt`, 182. A conversão nasceu em `1867006b`, `port/src/gs/gs_astc.cpp`.
- **O que e por quê:** a 0.7.0 convertia cada substituição nova para ASTC na CPU com o jogo rodando. Em celulares rápidos custava quadros a 60 e 120 por segundo.
- **Medido/provado:** números não registrados. O changelog da 0.7.2 só afirma que sem a opção os pacotes carregam como na 0.6.0.
- **O que pode quebrar:** pacotes de textura com `astc=1`; a pasta `textures_astc`.
- **Como testar de novo:** `BT3_ASTC=1` com um pacote; conferir a pasta e o tempo de quadro.

### 0.7.1 70441e5f: framebuffer fetch desligado: blocos pretos e GPU travada
- **Onde:** `port/src/gs/gs_gl.c` `gl_init`, 816-823; `ui/screens/SettingsScreen.kt`, 169-174. Nasceu em `1867006b`, `gs_gl.c`, 812-832.
- **O que e por quê:** regressão da 0.7.0. O fetch evitava as cópias DATE, mas em Mali-G52 (drivers r32p1 e r49p1; Moto g05, Redmi 12C) desenhava blocos pretos, rodava a 2 quadros/s e parava de devolver imagens em 6 a 30 s. Voltou às cópias da 0.6.0. A opção `gl_fetch` ficou para teste.
- **Medido/provado:** relatos de jogadores nos dois aparelhos. Depois, no G57 (device lab): sem travar, mas imagem errada; ver a entrada de DATE.
- **O que pode quebrar:** ligar o fetch por padrão em qualquer Mali.
- **Como testar de novo:** `BT3_GL_FETCH=1` com GL e `ppmcmp.py` contra a mesma build sem ele. Só em aparelho de teste.

### 0.6.0 7b99f8ad: Automático passa a ser Vulkan em toda GPU; anel de uniforms no GL
- **Onde:** `data/Renderer.kt`, 10-13 e 34-36; `port/src/gs/gs_gl.c` `ubo_orphan`, `ubo_frame`, `ubo_put`, a partir de 517.
- **O que e por quê:** a 0.5.0 escolhia GL ES para Mali, e as lutas de um Mali-G52 ficaram a 2 ou 3 quadros/s. Um único buffer de uniforms era reescrito cerca de 2.000 vezes por quadro; agora há um anel. O D32S8 do stencil de sombra veio junto, com a 0.1.35 do PC.
- **Medido/provado:** comentário em `Renderer.kt`: G52 de 2 a 3 quadros/s em GL contra cerca de 40 em Vulkan. Depois do anel persistente (`9ecce978`), o G52 simulado deu VK 52,8% e GL 52,0%, e o Cube GL 2x passou o VK 2x (97,8% contra 94,8%): a vantagem do Vulkan no Mali não vale mais como antes.
- **O que pode quebrar:** a escolha padrão de todo Mali. O aviso `renderer_mali_gl` ainda diz que o GL é várias vezes mais lento.
- **Como testar de novo:** `series_base.sh` nas duas APIs; ver a linha "uniform blocks" no log.

### 0.5.0 bbd4c2b5: GL ES 3.2, DXT descompactado, limites do cache, superfície ao voltar
- **Onde:** `port/src/gs/gs_gl.c` (36 trechos), `port/src/gs/gl_translate.h`, novo; `port/src/gs/gs_gpu.c` 400-442 e `poll_events` 638-647; `port/src/gs/gs_draw.c` 93-108 e `texture_get` 254-270; `data/Renderer.kt`, novo.
- **O que e por quê:** o back end GL do PC era desktop 3.3 e `413fcbf2` tinha forçado Vulkan por isso; agora roda como ES 3.2. `.dds` saía branco porque as GPUs de celular não leem DXT, e o jogo relia o arquivo a cada quadro: agora descompacta uma vez. Cache: 768 MB e 3.072 texturas em vez de 8.192. A superfície Vulkan é solta em segundo plano, o que resolve a tela preta ao voltar ("queueBuffer failed").
- **Medido/provado:** números do cache e da tela preta não registrados. Logs de jogadores no changelog: Adreno 830/825 e Mali-G720 a velocidade cheia em 2x e 3x, Mali-G68 a cerca de um quarto em 2x.
- **O que pode quebrar:** o teto de 3.072 só foi problema quando o BT4 não contava quadros (`069c31a6`: tela branca). `9ecce978` trocou os 768 MB por um valor pela RAM (`tex_bytes_cap`, `BT3_TEX_MB`). Sair e voltar do app em todos os renderizadores.
- **Como testar de novo:** ir para segundo plano no meio da luta e voltar: o log diz "the window's surface is let go" e "has a new surface". Pacote `.dds` numa GPU sem DXT.

### 0.2.0 7427cc37: shader sem discard, submissão em duas partes
- **Onde:** `port/src/gs/gs_draw.c` `draw_state`, 609-616; `port/src/gs/gs_gpu.c` `vk_frame_end`, 1024-1037; `port/src/gs/shaders/gs.frag`; `port/tools/android.py` `shaders`, 174-180.
- **O que e por quê:** GPU de tiles não descarta pixel escondido com `discard` no shader. Desenhos sem teste de alfa usam a variante sem ele. O bit foi 23, depois 25, e hoje é 27.
- **Medido/provado:** no commit, não registrado. Depois, no device lab, RP5 VK 4x: submit único 75,8% e discard no shader 74,7%, contra 76,0%; Cube VK 2x, submit único entre 73,0 e 73,7%. Dentro do ruído: o ganho esperado não aparece nesses dois aparelhos.
- **O que pode quebrar:** qualquer desenho classificado sem discard que dependa dele: alfa de destino, chaves de alfa.
- **Como testar de novo:** `compare_env.sh` com `BT3_GPU_DISCARD=1`, 0 pixels; `BT3_PRESENT` escolhe o modo de apresentação.

### 0.2.0 5ad3f1b9: alvos de 512x512
- **Onde:** `port/src/gs/gs_gpu.c` (23 trechos), 34-39 e 359-364; `port/src/gs/gs_draw.c` (16 trechos); `gs_draw.h`.
- **O que e por quê:** um quarto da memória e do tráfego de tile. Os desenhos mantêm a página de 1024x1024 por um viewport desse tamanho, porque o jogo não desenha além de 512x512 num buffer dele. Scissor limitado ao alvo.
- **Medido/provado:** ganho não registrado.
- **O que pode quebrar:** cópias e regiões que usem o tamanho da página em vez de `sTW`/`sTH`: `c2b26ba6` teve de cortar o retângulo da cópia DATE por isso.
- **Como testar de novo:** `compare_env.sh` com `BT3_GPU_BIG_TARGETS=1`: 0 pixels.

### 0.2.0 ecf319f3: Vulkan no Mali sem depth clamp, e o z limitado a 1
- **Onde:** `port/src/gs/gs_gpu.c` `vk_init`, 355-374, e `pipeline_create`, 268; `7dc28ce7`: `port/src/gs/gs_draw.c` `put`, 630-635.
- **O que e por quê:** o SDL exige `depthClamp`, `drawIndirectFirstInstance`, `shaderClipDistance` e `samplerAnisotropy`, e o Mali de um Unisoc T820 não tinha todos: "Invalid GPU device", cai em GL. Agora tenta de novo sem eles. Sem clamp, texto com z 0xFFFFFFFF era cortado: o menu pós-luta e as mensagens do memory card perdiam as letras, então `7dc28ce7` limita o z em toda GPU.
- **Medido/provado:** mensagem do commit: antes caía em OpenGL "at half speed". Sem número depois.
- **O que pode quebrar:** o que chega muito perto da câmera é cortado em vez de limitado, em GPU sem clamp.
- **Como testar de novo:** `BT3_GPU_LITE=1` em Vulkan, em qualquer aparelho: menu pós-luta e memory card com as letras.
