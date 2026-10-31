# Build e CI

Regras e formato: [README](README.md).


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
