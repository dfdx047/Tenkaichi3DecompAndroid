# Tag Team Mod

Regras e formato: [README](README.md).

### 1.0.0 (este commit): `L` do Tag Team Online sai do `.data`
- **Onde:** `port/src/gs/tag_link.c`, `struct ... L` e `link_sock_init`, linhas 98-100
- **O que e por quê:** o `L = {-1}` só dava -1 ao `sock`, mas o inicializador punha os 16,9 MB dos buffers no arquivo.
  A primeira tentativa (96895d21, construtor comum) **não adiantou**: o LLVM roda o construtor em tempo de compilação e
  grava o resultado no `L`. A escrita no construtor agora é `volatile`.
- **Medido/provado:** `llvm-size`: `.data` do `gs_tag_link.o` 16.916.336 → 8 bytes; `libbt3.so` 34,7 → 17,8 MB. O `.bss`
  total é 99 MB (0.9.0: 39,8 MB); páginas de BSS só contam quando tocadas.
- **O que pode quebrar:** salas do Tag Team Online (`TagLink_Stop` e o `start` testam `L.sock >= 0`). Se o construtor
  não rodasse, `sock` seria 0 e o `Stop` fecharia o descritor 0.
- **Como testar de novo:** abrir e fechar uma sala do Tag Team Online; `llvm-size port/build/android/obj/gs_tag_link.o`.


### 1.0.0 (este commit): `CLASH_QTE` não vale em partida do jogo normal
- **Onde:** `src/battle/btl_char_flag_clash.c`, `clash_prompts`
- **O que e por quê:** a opção é local e consome `BtlChar_Rand` no primeiro clash: em Dragon Net/Ranked com um jogador só com ela ligada, os dois lados divergiam. Agora ela só vale fora de `Port_NetSession`.
- **Medido/provado:** lido no código (revisão do branch); compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** prompts de clash no Tag Team Online, se ele passasse a ser `Port_NetSession` um dia (hoje não é).
- **Como testar de novo:** ligar SELECT > Mod Settings > Clash Button Prompts num aparelho, Dragon Net 1x1 com outro, forçar clash: sem dessincronia.
