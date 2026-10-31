# Build e CI

Regras e formato: [README](README.md).


### 1.0.0 (este commit): versionCode de release vem do nome, também no CI
- **Onde:** `.github/workflows/android.yml`, passo "Version from the tag"; `android/app/build.gradle.kts`, comentário; `docs/release-1.0.md`, comandos de publicar.
- **O que e por quê:** com o número da execução no CI, o código de uma release podia cair: 0.8.1 tinha 801 (PC) e 42 (CI), a 0.9.0 saiu com 55 e o Android recusou por cima do 801. Tag agora não passa `DR_VERSION_CODE`, então 1.0.0 vira 10000 no CI e no PC. Nightly continua com o número da execução, que `AppUpdates.check` compara.
- **Medido/provado:** `adb install -r`, sem `-d`, no Cube e no RP5: 50 sobre 55 recusado (`INSTALL_FAILED_VERSION_DOWNGRADE`, no RP5), 10000 sobre 55 instalado, 0 arquivos de saves e ajustes mudados. Workflow não rodado: só roda com push da tag.
- **O que pode quebrar:** nightly não instala por cima de release sem desinstalar. Voltar a uma release antiga exige desinstalar.
- **Como testar de novo:** instalar sem `-d` por cima da versão publicada e conferir os saves.

### 1.0.0 (este commit): ferramentas de laboratório e logscan
- **Onde:** `port/tools/logscan.py` (crash sem repetição por aparelho); `port/tools/lab/ppmcmp.py` (novo)
- **O que e por quê:** o `logscan` contava o mesmo crash duas vezes (linha do log e linha do relatório do app). `ppmcmp.py` compara as pastas de quadros `BT3_SHOT` pixel a pixel.
- **Medido/provado:** rodado nos 208 logs dos jogadores.
- **O que pode quebrar:** contagens dos relatórios antigos.
- **Como testar de novo:** `python3 port/tools/lab/ppmcmp.py <pasta a> <pasta b>`: 0 pixels entre duas rodadas da mesma gravação.

### 1.0.0 (este commit): chave de teste `BT3_TEST_CPUS`, ferramentas no repositório
- **Onde:** `android/app/src/main/cpp/loader.c`; `port/bt4/host/bt4_main.cpp`; `port/tools/lab/`; `port/tools/change_map.py`; `docs/changes/regression-tests.md`
- **O que e por quê:** o RP5 não tem root: para simular um aparelho com menos núcleos grandes, `BT3_TEST_CPUS=0-5` em `bt3_env.txt` prende as threads novas. Não limita frequência nem GPU. `change_map.py` gera `by-file.md`. `rec4.py` converte gravação de 2 para 4 portas: desde o Tag Team, `src/sys/pad.c` lê 4.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** só vale com a chave; sem ela, nenhum efeito.
- **Como testar de novo:** `BT3_TEST_CPUS=0-5`: o log diz "only cpus 0-5: done".

### 1.0.0 (este commit): `build.py` do BT2 também gera o `libbt2online.so`
- **Onde:** `bt2/android/build.py`, `main`
- **O que e por quê:** sem argumentos o script não rodava o passo `plugin`, então o APK levava o `libbt2online.so` antigo do `jniLibs` sem aviso: a correção do relay ficou de fora no primeiro build. O passo agora faz parte do padrão.
- **Medido/provado:** depois do `plugin`, o md5 do `.so` no APK é igual ao do `jniLibs` (`8bd82499acb2`).
- **O que pode quebrar:** o comando padrão fica alguns segundos mais longo.
- **Como testar de novo:** `python3 bt2/android/build.py`: a data do `libbt2online.so` no `jniLibs` muda.

### 1.0.0 (este commit): libbt4gsx.so sem librashader, e o build recusa dependência que o APK não tem
- **Onde:** `port/bt4/build_bt4.py`, função que compila o `libbt4gsx.so` (`-DARMSX2_DISABLE_LIBRASHADER=ON` e a checagem de `NEEDED` depois do strip)
- **O que e por quê:** o CMake do ARMSX2 liga o librashader sozinho quando o PC tem `cargo`. Com o alvo Rust `aarch64-linux-android` instalado, o `libbt4gsx.so` passou a pedir `liblibrashader_capi.so`, que não vai no APK: o `dlopen` falhava e o BT4 caía em silêncio no renderizador Dragon Rage, em todo aparelho. Agora a opção vai desligada, como na 0.9.0, e o build para com erro se a biblioteca pedir algo fora do sistema.
- **Medido/provado:** `llvm-objdump -p`: test9 com `liblibrashader_capi.so` em `NEEDED`; test10 com a mesma lista da 0.9.0 e o mesmo tamanho (34285872 bytes). Log do aparelho: `ARMSX2's renderer is not in this version (dlopen failed…)` no test9.
- **O que pode quebrar:** quem quiser cadeias de shader do RetroArch no BT4 precisa ligar a opção e pôr a biblioteca no APK; hoje a tela do jogo não oferece isso.
- **Como testar de novo:** no log do BT4 com `bt4_renderer=1`: linhas `bt4gsx:` e nenhuma `could not start`. Instalar o alvo Rust e compilar de novo não pode mudar a lista de `NEEDED`.

### 1.0.0 (este commit): changelogs da 1.0
- **Onde:** `CHANGELOG.md`, `CHANGELOG.pt.md`, `CHANGELOG.es.md`
- **O que e por quê:** conteúdo substituído pelo da 1.0, nos três idiomas, com o que muda para quem joga e o desempenho real em aparelho fraco.
- **Medido/provado:** cada frase conferida contra os testes nos aparelhos; os números vêm de `~/DragonRageRefs/1.0i/logs`, `1.0j/logs` e `0.9.0b/logs`.
- **O que pode quebrar:** nada no app.
- **Como testar de novo:** ler junto de `docs/release-1.0.md`.

### 1.0.0 (este commit): 1.0: scripts de medição final e faixas do changelog
- **Onde:** port/tools/lab/finals.sh, port/tools/lab/finals2.sh; CHANGELOG{,.pt,.es}.md
- **O que e por quê:** As rodadas finais de GL, Lite e 60 fps. O changelog passa a dizer faixas (8 a 10 no ARMSX2, 10 a 11 no Dragon Rage) porque a rodada final deu 10,1 e não o 11,2 de antes.
- **Medido/provado:** Logs em ~/DragonRageRefs/1.0n/logs. `gpu_api=1` sozinho não troca o renderizador: o launcher reescreve a partir de `dr_renderer`, por isso `finals2.sh` usa `dr_renderer=2 gpu_api=1`; as linhas `*_gl` de finals.sh rodaram em Vulkan e não valem como GL.
- **O que pode quebrar:** Só scripts e texto. Os scripts têm IPs da rede local do laboratório.
- **Como testar de novo:** `bash port/tools/lab/finals2.sh` com os dois aparelhos na mesma rede.

### 1.0.0 (este commit): 1.0: README nos três idiomas, gráficos e relatório
- **Onde:** README.md, README.pt.md, README.es.md; docs/images/1.0/; port/tools/perf_chart.py; docs/release-1.0.md; docs/changes/regression-tests.md; port/tools/lab/ab_bt4.sh, threads.sh
- **O que e por quê:** Pedido do desenvolvedor: tudo o que veio de novo, com imagens, números e comparativos. Os gráficos são SVG com tema claro e escuro, gerados por `perf_chart.py` a partir de números copiados dos logs. O perfil fraco do BT4 foi medido com as versões alternadas na mesma sessão, depois que sessões separadas sugeriram uma piora que não se confirmou.
- **Medido/provado:** Paleta conferida com o validador da skill de visualização, claro e escuro. SVGs abertos em PNG e olhados. Todo link de imagem dos três READMEs aponta para um arquivo existente. As fotos de tela são do aparelho, com APK 1.0.
- **O que pode quebrar:** Os números do README envelhecem: refaça ao medir de novo e rode `perf_chart.py`. As fotos mostram o jogo: entraram por pedido do desenvolvedor.
- **Como testar de novo:** `python3 port/tools/perf_chart.py`, abrir os READMEs no GitHub nos temas claro e escuro.

## Histórico: 0.9.0 para trás

Linhas: lado novo do diff naquele commit; o arquivo de hoje tem outra numeração. Fontes: mensagens de commit, diffs, changelogs de cada tag,
`docs/release-0.9.0.md`, `docs/lite-notes.md`. Mais novas primeiro.

### 0.9.0 2d6d0531: SDL 3.4.12 com os conjuntos de descritores mantidos
- **Onde:** `port/third_party/sdl3-patches/vulkan-keep-descriptor-sets.patch` (`VULKAN_BindGraphicsPipeline`), nascido em `dd4581d7`; `.github/workflows/toolchain.yml`, 9, 19 e 50-51.
- **O que e por quê:** modelos 3D alternam dois pipelines (A B A B, bits de blend 1, 4 e 6 e escrita de alfa 19), em cerca de 1.960 de 2.076 pares de draws, e o SDL recriava e regravava 4 conjuntos em cada troca. O patch mantém os conjuntos com o mesmo layout. Vale para todos.
- **Medido/provado:** G52 simulado, só CPU: 63,4% → 70,3%, `draws` 14,3 → 11,0 ms. Com brilho pulado, só GPU: 73,7% → 74,3%. RP5 VK 4x: 83,7% e 83,5%, `draws` 5,39 → 4,23 ms: empate. Pixels: 0 nos dois aparelhos contra o APK 900.
- **O que pode quebrar:** Vulkan em todos os aparelhos. O CI republica o asset do `toolchain-ndk` quando o `main` muda o patch; a tag precisa esperar, senão o APK sai sem o ganho. Local: `android-toolchain/sdl3/lib/libSDL3.so`, original em `libSDL3.so.stock`. **Hoje:** `d54095dd` trocou o arquivo por `vulkan-descriptor-sets.patch`, que não lê `DR_SDL_KEEP_SETS`; os documentos ainda citam a chave, e o A/B sumiu.
- **Como testar de novo:** `gh run list --workflow toolchain.yml`; `series_base.sh` e `ppmcmp.py` contra a referência. Comparar com o SDL original exige outra build.

### 0.9.0 b94a6faf: `check_symbols` para o `libbt4.so`
- **Onde:** `port/bt4/build_bt4.py` `check_symbols`, 169-185, e `build`, 192.
- **O que e por quê:** a mescla da 0.1.38 deixou o `libbt4.so` sem abrir: "dlopen failed: cannot locate symbol", visto só no aparelho. O build compara o que o `.so` pede com SDL3 e o sysroot, e para. `_Unwind_*` fica de fora, porque vem da libc do aparelho.
- **Medido/provado:** a build limpa da 0.9.0 passou; a checagem pegou de novo os símbolos da 0.1.39.
- **O que pode quebrar:** biblioteca nova carregada por `dlopen` passa sem conferência.
- **Como testar de novo:** `python3 port/bt4/build_bt4.py`.

### 0.9.0 8d9d5fb3: versionCode: CI contra local
- **Onde:** `android/app/build.gradle.kts`, 16-18 (`DR_VERSION_NAME`, `DR_VERSION_CODE`); `.github/workflows/android.yml`.
- **O que e por quê:** no CI o código é o número da execução; local é o nome (0.9.0 vira 900). Aparelhos de teste com o 900 local e a chave `dragonrage-test.jks` recusam APK do CI com código menor ou outra assinatura.
- **Medido/provado:** subida 801 → 900 e instalação por cima, saves com o mesmo md5.
- **O que pode quebrar:** teste em aparelho exige desinstalar; jogadores vêm só do CI.
- **Como testar de novo:** instalar por cima e conferir saves.

### 0.2.0 c9fee92d: só release, tags versionadas
- **Onde:** `.github/workflows/android.yml`, 10 trechos entre 3 e 145; `port/tools/android_toolchain.sh`, 21.
- **O que e por quê:** `android-vX.Y.Z` publica o APK de release; o nightly só leva release; o PC só compila em `v*` ou à mão.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** tags do PC e do Android convivem no mesmo repositório. Commits com `[skip ci]` (0.4.0 a 0.6.0) não geram nada.
- **Como testar de novo:** ler o resultado do workflow numa tag.

### 0.2.0 52232f3a: chave fixa para builds de teste
- **Onde:** `android/app/build.gradle.kts`, 26-33.
- **O que e por quê:** uma build nova instala por cima da velha e mantém os dados do jogo.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** a chave fica no repositório: nunca usar em release. Sem segredos, o CI avisa e assina com a de depuração (`android.yml`). O 0.7.3 explica o conflito de assinatura para quem tem outra build.
- **Como testar de novo:** `./gradlew assembleRelease` e instalar por cima.

### 0.2.0 dce2c134: toolchain do motor: LLVM, NDK e SDL
- **Onde:** `port/tools/android.py`, novo; `port/tools/android_toolchain.sh`, novo; `.github/workflows/android.yml`, 16-63. Antes dele: `cbf6eb8d`, `932d9b3a`, `dc3c0c05` (SDL 3.4.12), `8a22a0f0` em `.github/workflows/toolchain.yml`.
- **O que e por quê:** o motor compila para arm64 com LLVM 20 ou mais novo (`__ptr32` em AArch64), com float soft. Ligado sem interpretador e marcado como objeto compartilhado, para `android_dlopen_ext` carregar. `da357474` e `07280401` estão em [`bt3-engine.md`](bt3-engine.md).
- **Medido/provado:** motor sobe no Odin 3 (`def0472b`).
- **O que pode quebrar:** subir LLVM ou NDK: repetir os testes de `da357474`. Flag nova pode trocar a matemática do jogo.
- **Como testar de novo:** `python3 port/tools/android.py`.

### 0.1.0 750273dd: launcher e CI
- **Onde:** `android/`; `.github/workflows/android.yml`, novo; `release.yml` ignora mudanças só em `android/`; README do PC movido para `docs/port/upstream_README.md`.
- **O que e por quê:** primeiro APK, sobre a `v0.1.11` do PC.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** mudança só em `android/` não dispara o CI do PC.
- **Como testar de novo:** `cd android && ./gradlew assembleRelease`.
