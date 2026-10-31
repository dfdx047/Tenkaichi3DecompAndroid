# Tag Team Mod

Regras e formato: [README](README.md).

### 1.0.0 (este commit): `L` do Tag Team Online sai do `.data`
- **Onde:** `port/src/gs/tag_link.c`, `struct ... L`, linhas 98-99
- **O que e por quê:** o `L = {-1}` só dava -1 ao `sock`, mas o inicializador punha os 16,9 MB dos buffers no arquivo:
  `libbt3.so` foi de 15,5 para 34,7 MB, e o APK inteiro cresceu. O `sock` agora é posto em -1 por um construtor.
- **Medido/provado:** `llvm-size`: `.data` do `gs_tag_link.o` de 16.916.336 para bytes do restante; ver `build-ci.md`.
- **O que pode quebrar:** salas do Tag Team Online (`TagLink_Start`, `TagLink_Stop` testam `L.sock`).
- **Como testar de novo:** abrir e fechar uma sala do Tag Team Online; `llvm-size port/build/android/obj/gs_tag_link.o`.

