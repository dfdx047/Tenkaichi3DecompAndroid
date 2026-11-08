# BT2

Regras e formato: [README](README.md).


### 1.0.0 (este commit): BT2 só com a ISO: motor no APK, imagem lida no lugar
- **Onde:** `bt2/runtime/services/devices/disc_image.h` (novo), `disc_sectors.h`, `disc_metadata.h`; `bt2/runtime/game/startup_inputs.cpp`; `bt2/runtime/android/android_main.cpp`; `bt2/plugin/online/opening_music.cpp`; `data/GameDataInstaller.kt`, `installBt2`, `currentStateBt2`, `openBt2Image`; `data/GamePaths.kt`; `engine/Bt2GameActivity.kt`; `Bt2ViewModel.kt`; `ui/screens/bt2/Bt2HomeScreen.kt`; `data/Bt2Engine.kt` removido; `bt2/android/build.py` sem `package` e `key`
- **O que e por quê:** a aba pedia um pacote do motor baixado no Discord e copiava 4 GB para a pasta do app. Agora `libbt2.so` vem no APK compilado no PC, a ISO fica onde está e é lida com `pread` em `/proc/self/fd/N`, como o BT4 desde `0cabeef` (nunca por caminho: endereço `msf:` falha). `SLUS_214.41` é conferido por SHA-256 e a recusa diz o que foi achado. Quem tinha motor importado é avisado uma vez e o arquivo é apagado.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** quem tinha a cópia antiga em `data/BT2.iso` precisa escolher a ISO de novo (a cópia é apagada na importação; sem marcador, o motor ainda abre a antiga). Fork de CI sem `libbt2.so` mostra a mensagem de motor ausente.
- **Como testar de novo:** instalação limpa: importar a ISO por MediaStore e por pasta, abrir, lutar, salvar, fechar, reiniciar o aparelho e abrir. Dar uma ISO de BT3: a recusa diz o serial achado.

### 1.0.0 (este commit): o build exige o código do kernel; erro do jogo aparece em vez de tela preta
- **Onde:** `bt2/android/CMakeLists.txt`; `bt2/runtime/android/android_main.cpp`, `BT2_VIEWER_CLOSE` e `traceThisThread`
- **O que e por quê:** sem `game/generated/bt2-kernel-runtime`, o CMake compilava calado e o jogo parava no primeiro timer com "Timer replay requires generated kernel payload integration". Agora o build falha e diz para rodar `generate_kernel.py`. O `ViewerHold` segurava a janela quando o jogo parava, e a mensagem só saía depois de fechar: qualquer erro virava tela preta para sempre. No Android a janela não é mais segurada. O rastreio de parada também segue os frames depois do vDSO.
- **Medido/provado:** Cube e RP5: antes, tela preta com a thread principal em `~ViewerHold` (`debuggerd -b` no Cube). Só com `BT2_VIEWER_CLOSE`: aviso na tela e o erro no log. Com o código do kernel: logos da Spike e da Bandai Namco, 100% de velocidade no Cube.
- **O que pode quebrar:** build de quem não gerou o kernel passa a falhar, de propósito. Sem a janela segurada, um erro fecha o jogo depois do aviso.
- **Como testar de novo:** apagar `game/generated/bt2-kernel-runtime` e rodar `bt2/android/build.py`: o build para com a mensagem. Abrir o BT2: os logos aparecem.

### 1.0.0 (este commit): escala de saída abaixo de 100% enche a tela
- **Onde:** `android/app/src/main/java/com/dfdx047/dragonrage/engine/Bt2GameActivity.kt`, `applyOutputScale`
- **O que e por quê:** o SDL adiciona a view com wrap_content, então `setFixedSize` encolhia a própria view: a imagem ficava no canto, sem ampliar. Cada `onResume` ainda calculava sobre a largura já reduzida. O Cube cai nisso na primeira abertura, porque `lowEndPreset` escolhe 75%. A view agora fica em tela cheia.
- **Medido/provado:** RP5 a 75%: buffer de 1440x810 enchendo a tela de 1920x1080; antes, no canto, com a camada em 1440x810. No Cube, a 75% com a correção (APK de teste 7): luta e menus em tela cheia.
- **O que pode quebrar:** o toque que passa pelo SDL usa o tamanho do buffer; os controles na tela são uma camada à parte.
- **Como testar de novo:** BT2 no menu, escala de saída 75 e 50: imagem cheia, e continua cheia depois de sair e voltar ao app.

### 1.0.0 (este commit): o cartão do BT2 não diz mais "Em breve"
- **Onde:** `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/bt2/Bt2HomeScreen.kt`, `Bt2OnlineCard`; `res/values*/strings_bt2.xml`, `bt2_online_*`
- **O que e por quê:** o cartão dizia "Dragon Net 2, em breve" com botão desligado, com o online já funcionando dentro do jogo. Agora diz onde ele está. O botão saiu.
- **Medido/provado:** visto no RP5 em português; título corrigido para `onSurface` (estava preto sobre fundo escuro). Inglês e espanhol: só revisão.
- **O que pode quebrar:** nada no jogo.
- **Como testar de novo:** aba BT2, Início, nos três idiomas.

## Histórico: 0.9.0 para trás

Nada a registrar: `git log e1b9207 -- bt2` não traz nenhum commit; o BT2 só entra depois da 0.9.0, com `85cdb232`.
