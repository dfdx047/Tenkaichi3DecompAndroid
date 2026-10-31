# BT4 (runner recompilado)

Regras e formato: [README](README.md).

### 1.0.0 (este commit): símbolos novos do `gs_input.c` no shim
- **Onde:** `port/bt4/host/bt4_shim.c`, depois de `gPortNetWaitNs`
- **O que e por quê:** a mescla do PC 0.1.39 e do Tag Team fez o `gs_input.c` pedir `Port_NetSpectating`,
  `Port_TagTestCheck`, `TagNet_Enabled`, `TagNet_Role`, `TagTeam_ViewActor` e `gNativePad`. O `check_symbols` parou o build;
  sem ele, o BT4 cairia no aparelho com "cannot locate symbol", como na mescla da 0.1.38.
- **Medido/provado:** `build_bt4.py` passa a checagem. Abertura e luta: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** BT4 nos dois renderizadores. Novo código de BT3 em `port/src/gs` pode pedir mais símbolos.
- **Como testar de novo:** `python3 port/bt4/build_bt4.py --armsx2`; `port/tools/lab/bt4fight.sh`.


## Histórico: 0.9.0 para trás

Linhas: lado novo do diff naquele commit; o arquivo de hoje tem outra numeração. Fontes: mensagens de commit, diffs, `docs/device-lab-2026-10.md`,
`docs/release-0.9.0.md`, `docs/lite-notes.md`, `port/bt4/README.md`. Mais novas primeiro. O BT4 só existe a partir da
0.9.0. O renderizador ARMSX2 tem o seu arquivo: [`bt4-armsx2.md`](bt4-armsx2.md).

### 0.9.0 2d6d0531: aviso em chip fraco; TLS agrupado fora
- **Onde:** `AppViewModel.kt`, 671-680; `data/GpuProfiles.kt` `weakChip`; `port/bt4/build_bt4.py`, 51, 77 e 90 (tira o patch); `port/bt4/patches/bt3recomp-lite.patch` só em `dd4581d7`.
- **O que e por quê:** o BT4 no G52 é limitado pela CPU. Perfil: `dispatchGuestBranch` 18%, `tlsdesc_resolver_dynamic` 13%, `getenv` 3%. O launcher avisa e deixa jogar. O agrupamento dos `thread_local` compilou, mas não foi medido, e ficou de fora.
- **Medido/provado:** G52 simulado, atualizações da luta por segundo (30 é o normal), tudo limitado: ARMSX2 8,7, Dragon Rage 10,0. Só CPU: 9,4 e 10,6. Só GPU: 20,3 e 18,4. O patch de TLS: não medido.
- **O que pode quebrar:** `weakChip` decide o aviso e o Lite. Nome de GPU sem núcleos dá outro resultado.
- **Como testar de novo:** `gpu_name.txt` com "Mali-G52 MC1": o aviso aparece. `bash port/tools/lab/series_bt4_cube.sh <versão>` roda o perfil fraco.

### 0.9.0 1bc64e27: vigia dos 60 fps
- **Onde:** `port/bt4/host/bt4_main.cpp` `fps60Watch`, 332-375, e `menuApply`, 437-438; `engine/Bt4Activity.kt` `fps60Dropped`, 74-80; `engine/GameMenu.kt` `PerformancePage`, 608-616.
- **O que e por quê:** no device lab, os 60 fps deixavam a luta mais lenta: Cube a 50%, RP5 a 65%. Com `bt4_fps60` ligado, entre 20 e 57 atualizações/s por 3 s seguidos o jogo volta a 30, salva `bt4_fps60=0` e `bt4_fps60_dropped=1`, e avisa. Só o que está fora dessa faixa (pausa, menu) não conta.
- **Medido/provado:** Cube: recuou, avisou, luta a 30 (`guest_ms` 20 a 23). RP5, 144 s com `BT4_FPS60_KEEP=1`: `-O2` 37,0 e 36,1 atualizações/s, `-O3` 36,1, ThinLTO 36,2: nenhum ganho, fica `-O2`. PGO não foi feito. Nenhum dos dois aparelhos segura 60: Cube cerca de 30, RP5 36 a 41.
- **O que pode quebrar:** um aparelho que oscile perto de 57 perde a opção. Ligar de novo limpa o aviso. O novo valor só vale na luta seguinte.
- **Como testar de novo:** `bt4_fps60=1` no Cube e `bash port/tools/lab/bt4fight.sh`: o log diz "60 fps fights: ... back to 30". `BT4_FPS60_KEEP=1` para medir.

### 0.9.0 sem commit: movimento suave no BT4 não oferecido
- **Onde:** nenhum código mudou. `port/src/gs` `GsGpu_InterpPump`; `docs/release-0.9.0.md`, seção 3, item 4.
- **O que e por quê:** o movimento suave interpola as matrizes do VU0 (`Vu0Uniform`). No BT4 o stream chega ao GS já transformado (`d->vu = 0`), então só sobraria a heurística de pares de triângulos, que deforma o cenário. O laço do BT4 também nunca chama `GsGpu_InterpPump`.
- **Medido/provado:** leitura do código; sem medida.
- **O que pode quebrar:** ligar o ajuste no BT4 sem resolver o acima.
- **Como testar de novo:** não registrado.

### 0.9.0 sem commit: ruído no cenário distante, renderizador Dragon Rage (aberto)
- **Onde:** `port/src/gs/gs_core.c` (GIF PACKED, `clut_hash`); `docs/device-lab-2026-10.md`, "Aberto"; `docs/release-0.9.0.md`, seção 4.
- **O que e por quê:** montanhas e pedras ao longe saem como blocos de ruído colorido; o ARMSX2 desenha certo. Descartado com `BT3_GS_DUMP`: cópias local→local, mipmap, VU1, region repeat, STQ em fp32. Q ≈ 0,001: corrigir o Q do ST para valer no próximo RGBA não muda nada, e por isso não entrou. Chave do cache: improvável. Suspeitos: X negativo fora de 0..4095, e o mesmo tbp 2a00 usado por várias texturas no mesmo quadro (T8 de 1024x256, Q ≈ 0,001).
- **Medido/provado:** 6 capturas de cada variante de Q no RP5, ruído igual; no BT3, 0 pixels nos 8 quadros. `2e894c0c` também não resolveu.
- **O que pode quebrar:** o contorno é o ARMSX2, o padrão. O changelog da 0.9.0 avisa o jogador.
- **Como testar de novo:** BT4, renderizador Dragon Rage, Duelo, Espectador, mapa da planície, uns 60 s com a câmera no horizonte. Próximo passo: `BT3_GS_FROM` e `BT3_GS_STOP` nesses prims e comparar a T8 com o dump do GS do ARMSX2.

### 0.9.0 d6467b24: pacotes de textura no renderizador Dragon Rage do BT4
- **Onde:** `port/bt4/host/bt4_main.cpp` `struct Gsx` e `gsxWriteIni`, 517-581, e `SDL_main`, 920-925; `port/src/gs/gs_texpack.c` `TexPack_Init`, 274-290.
- **O que e por quê:** nenhum dos dois renderizadores olhava `bt4/textures`. O host agora passa o caminho inteiro (`BT3_TEXTURES`), porque o jogo troca de pasta.
- **Medido/provado:** BT4 com o pacote do BT3 no Cube: 34 trocadas, 29,3 atualizações/s contra 29,6 sem. Não medido no RP5.
- **O que pode quebrar:** pacote feito para o BT4 troca muito mais: não medido.
- **Como testar de novo:** pacote em `bt4/textures`, Duelo: o log diz quantas foram trocadas.

### 0.9.0 6e57077d: símbolos novos no shim; costura com a escala certa
- **Onde:** `port/bt4/host/bt4_shim.c`, 29 e 110-113; `port/src/gs/gs_core.c` `seam_widen`, 626-630.
- **O que e por quê:** depois da mescla, `libbt4.so` não abria: "cannot locate symbol gPortNetWaitNs" e mais cinco. De passagem: `extern int gsScale` no `seam_widen` ligava com uma função do jogo de mesmo nome, e a costura ficava sempre em 0,5. Agora usa `gsScale4`. O relatório credita a checagem de símbolos a este commit; pelo diff ela está em `b94a6faf`, ver [`build-ci.md`](build-ci.md).
- **Medido/provado:** BT4 abre nos dois aparelhos (seção 4 do relatório). A costura entra nas diferenças de pixels do RP5 VK 2x atribuídas às mudanças intencionais; não isolada.
- **O que pode quebrar:** toda mescla do PC pede símbolos novos: aconteceu de novo na 1.0.0. `extern` sem tipo conferido liga em qualquer símbolo de mesmo nome.
- **Como testar de novo:** `python3 port/bt4/build_bt4.py --armsx2`; abrir o BT4 no aparelho.

### 0.9.0 8e838c71: START+SELECT também fecha o menu
- **Onde:** `engine/GameActivity.kt`, 51 e 402-412.
- **O que e por quê:** o BT4 abria o menu de toque, mas os mesmos botões não faziam nada com ele aberto. Agora fecham, e não chegam ao jogo.
- **Medido/provado:** Cube, 3 vezes seguidas, o jogo volta sem pausar. Luta seguiu a 29,4. No RP5 não dá para injetar (sem root).
- **O que pode quebrar:** se o START chega uns 50 ms antes do SELECT, o jogo vê o START sozinho e abre a própria pausa por baixo. Já acontecia antes; nenhum commit depois da 0.9.0 cita correção.
- **Como testar de novo:** Cube, BT4 em luta, apertar e soltar START+SELECT.

### 0.9.0 0cabeef6: imagem lida pelo descritor
- **Onde:** `port/bt4/host/bt4_main.cpp`, 34-37 e `SDL_main` 826-829; `port/bt4/patches/bt3recomp-android.patch`, 153-295 (`ps2_iop.cpp`).
- **O que e por quê:** depois de reiniciar, "the disc image cannot be read (/proc/self/fd/75)". A permissão persistida estava lá. Reabrir `/proc/self/fd/N` pelo caminho (`access()`, `std::ifstream`, `file_size`) refaz a checagem do FUSE em `/storage/emulated/0/Download/...`, e o app, sem permissão de armazenamento, é recusado. ISO por endereço `msf:` falha; `raw:` passa, e era o caso do RP5. Agora o runtime lê com `pread` no fd.
- **Medido/provado:** testado nos dois aparelhos. Abrir depois de reiniciar: OK no Cube, 29,1 atualizações/s.
- **O que pode quebrar:** qualquer leitura nova da imagem por caminho. O BT2 da 1.0.0 usa a mesma ideia.
- **Como testar de novo:** escolher a ISO pelo Downloads (MediaStore), reiniciar o aparelho, abrir.

### 0.9.0 069c31a6: quadros contados para o cache de texturas
- **Onde:** `port/bt4/host/bt4_main.cpp`, 45 e 902.
- **O que e por quê:** `gGsFrame` nunca andava no BT4, então o cache nunca descartava nada. Com 3.072 texturas, cada nova saía branca: depois de cerca de 2 min, tela branca, quadrados rosa e 6 quadros/s.
- **Medido/provado:** RP5 e Cube, renderizador Dragon Rage: 30 quadros/s por mais de 150 s no RP5.
- **O que pode quebrar:** quem ler `gGsFrame` no BT4 passa a ver um contador que anda só aqui, fora do GS.
- **Como testar de novo:** renderizador Dragon Rage, luta de mais de 3 min: sem branco.

### 0.9.0 4c8794ef: pad e ambiente do BT4
- **Onde:** `port/bt4/host/bt4_main.cpp` `sendPad`, 199, e `SDL_main`, 805-820; `port/bt4/build_bt4.py`, 83.
- **O que e por quê:** com o ARMSX2, os botões não chegavam ao jogo porque `sendPad` exigia o renderizador Dragon Rage. `bt4/bt3_env.txt` não era lido. O readback está em [`bt4-armsx2.md`](bt4-armsx2.md).
- **Medido/provado:** device lab: RP5 e Cube, luta jogada pelo roteiro de botões.
- **O que pode quebrar:** chaves de teste em `bt4/bt3_env.txt` agora valem de verdade: apagar depois de testar.
- **Como testar de novo:** `BT3_GS_VERBOSE=1` em `bt4/bt3_env.txt`; Duelo com os dois renderizadores.

### 0.9.0 9ecce978: 16:9, 60 fps e drivers no BT4
- **Onde:** `port/bt4/host/bt4_shim.c` (`screen_milli`, `Bt4_AspectMode`, `Bt4_AspectSet`); `port/bt4/host/bt4_main.cpp`; `data/Bt4Installer.kt`; `port/bt4/README.md`.
- **O que e por quê:** o mod desenha 16:9 espremido no quadro 4:3 do PS2 e o jogo mostra esticado por padrão (`bt4_aspect`: 0 16:9, 1 4:3, 2 tela cheia). `bt4_fps60` é o modo do BT3-Recomp, ao vivo pelo menu.
- **Medido/provado:** 60 fps: ver `1bc64e27`. Tamanho: RP5 ocupa a tela em 16:9 e tela cheia, 4:3 com faixas (`79ce6851`).
- **O que pode quebrar:** só a apresentação muda; a tabela `games/bt3/fps60_sites.txt` depende do ELF Beta 14 Rev2.
- **Como testar de novo:** menu do jogo, os três formatos, nos dois renderizadores.

### 0.9.0 5707f85b: o motor do BT4 entra
- **Onde:** `port/bt4/host/bt4_main.cpp`, `bt4_shim.c`, `bt3gl_android.cpp`, `android_ucontext.S`; `port/bt4/patches/bt3recomp-android.patch`; `port/bt4/build_bt4.py`. Launcher: `e3df9aa1` (`data/Bt4Installer.kt`, `data/Game.kt`, `ui/screens/WelcomeScreen.kt`).
- **O que e por quê:** o mod muda o programa do jogo, então o motor do BT3 não o roda. É o mod recompilado (BT3-Recomp com os patches do dbz-bt4-recomp), desenhado pelo GS do Dragon Rage. O ELF é conferido por SHA-256: o código é gerado só para o Beta 14 Rev2.
- **Medido/provado:** no commit, não registrado. Device lab: importar a ISO, 23 arquivos e 5,5 MB copiados, imagem lida no lugar; Dragon Rage a 30 quadros/s no RP5.
- **O que pode quebrar:** tudo em `port/src/gs` é compilado aqui também: qualquer símbolo novo pede o shim.
- **Como testar de novo:** `bash port/tools/lab/series_bt4_rp5.sh <versão>`; `series_bt4_cube.sh`.
