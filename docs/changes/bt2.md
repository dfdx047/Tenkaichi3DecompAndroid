# BT2

Regras e formato: [README](README.md).


### 1.0.0 (este commit): BT2 só com a ISO: motor no APK, imagem lida no lugar
- **Onde:** `bt2/runtime/services/devices/disc_image.h` (novo), `disc_sectors.h`, `disc_metadata.h`; `bt2/runtime/game/startup_inputs.cpp`; `bt2/runtime/android/android_main.cpp`; `bt2/plugin/online/opening_music.cpp`; `data/GameDataInstaller.kt`, `installBt2`, `currentStateBt2`, `openBt2Image`; `data/GamePaths.kt`; `engine/Bt2GameActivity.kt`; `Bt2ViewModel.kt`; `ui/screens/bt2/Bt2HomeScreen.kt`; `data/Bt2Engine.kt` removido; `bt2/android/build.py` sem `package` e `key`
- **O que e por quê:** a aba pedia um pacote do motor baixado no Discord e copiava 4 GB para a pasta do app. Agora `libbt2.so` vem no APK compilado no PC, a ISO fica onde está e é lida com `pread` em `/proc/self/fd/N`, como o BT4 desde `0cabeef` (nunca por caminho: endereço `msf:` falha). `SLUS_214.41` é conferido por SHA-256 e a recusa diz o que foi achado. Quem tinha motor importado é avisado uma vez e o arquivo é apagado.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** quem tinha a cópia antiga em `data/BT2.iso` precisa escolher a ISO de novo (a cópia é apagada na importação; sem marcador, o motor ainda abre a antiga). Fork de CI sem `libbt2.so` mostra a mensagem de motor ausente.
- **Como testar de novo:** instalação limpa: importar a ISO por MediaStore e por pasta, abrir, lutar, salvar, fechar, reiniciar o aparelho e abrir. Dar uma ISO de BT3: a recusa diz o serial achado.

## Histórico: 0.9.0 para trás

Nada a registrar: `git log e1b9207 -- bt2` não traz nenhum commit; o BT2 só entra depois da 0.9.0, com `85cdb232`.
