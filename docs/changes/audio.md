# Áudio

Regras e formato: [README](README.md).


## Histórico: 0.9.0 para trás

Linhas: lado novo do diff naquele commit; o arquivo de hoje tem outra numeração. Fontes: mensagens de commit, diffs, `docs/release-0.9.0.md`,
`port/bt4/README.md`. Área fina: as mudanças de som vieram quase todas do PC. Mais novas primeiro.

### 0.9.0 01224d08: miniaudio do PC; início do arquivo dentro do AFS
- **Onde:** `port/src/gs/snd_adx.c` `start`, cerca de 537 (conflito resolvido à mão); `port/src/gs/snd_ma.c`, novo, 1-295; `port/third_party/miniaudio/`; `port/src/plat_songs.c`.
- **O que e por quê:** `snd_ma.c` entra e `song_mp3.c` sai: mp3, wav e flac tocam direto, sem conversão para ADX. O PC volta com `fseek(fp, 0, SEEK_SET)` depois de olhar a marca 0x8000, mas no Android o arquivo pode estar dentro de um AFS (`d285f5d6`). A mescla usa `ftell` e volta para o início do arquivo, não do AFS.
- **Medido/provado:** não registrado para o som. Só que a mescla compila e a luta gravada segue a 100%.
- **O que pode quebrar:** qualquer próximo conflito em `snd_adx.c` que volte ao `SEEK_SET`: música do disco sairia errada só no Android, porque o PC usa arquivos soltos.
- **Como testar de novo:** tocar música do disco e uma `.mp3` da pasta `songs`; `BT3_SND_VERBOSE=1`.

### 0.9.0 5707f85b: som do BT4 pelo SDL3
- **Onde:** `port/bt4/host/bt3gl_android.cpp`, novo; `port/bt4/README.md`.
- **O que e por quê:** o runtime do BT3-Recomp usa raylib; a versão sem janela daqui manda o áudio pelo SDL3.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** sair e voltar do segundo plano no BT4; volume.
- **Como testar de novo:** BT4, luta, ir ao segundo plano e voltar.

### 0.2.0 d285f5d6: ADX pelo AFS; pausa de som no menu
- **Onde:** `port/src/gs/snd_adx.c` `adx_blanks`, 71, `Port_AudioPause`, 127-137, `start`, 272 e 276; `port/src/plat_stub.c` `Port_VBlank`, 144-161.
- **O que e por quê:** a instalação passou a não extrair arquivos soltos, então o ADX abre por `Port_OpenSpan`. A janela de ajustes pausa imagem e som, exceto online (`Port_NetSession`).
- **Medido/provado:** não registrado.
- **O que pode quebrar:** `fopen` direto em código novo de som, que não acha o arquivo no Android. Pausa no online dessincronizaria.
- **Como testar de novo:** abrir o menu no meio da luta: o som para e volta.
