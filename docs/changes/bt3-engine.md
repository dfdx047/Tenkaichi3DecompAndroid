# Motor do BT3 (jogo descompilado, loader, memória)

Regras e formato: [README](README.md).


### 1.0.0 (este commit): leitura de arquivo que falha: diz o motivo, tenta de novo, luta segue
- **Onde:** `port/src/plat_fcache.c`, `Port_FileReadAt`; `src/battle/btl_obj.c`, `BtlObj_Rebind` e `BtlRes_PortModelReady`; `src/sys/gfxm_e_b.c`, `BtlObjMdl_Init`
- **O que e por quê:** maior crash dos logs: `BtlObjMdl_BindFiles+0x30`, endereço 0x6c, 10 aparelhos e 57 eventos, sempre em `BtlLoad_StepChara > BtlObj_Rebind` no meio da luta. `meshOfs` fica em 0x6c de um `file` nulo, e `ADXF_ReadNw` preenche com zeros o que não foi lido sem dizer nada. Conferi os arquivos 1994, 2824 e 2924 no AFS de um aparelho: a tabela deles está certa, então o disco não é a causa; a releitura da 0.9.0 lê do mesmo `FILE*` e também não resolvia. A causa exata da falha de leitura não está provada, porque o log não tinha o `errno`.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`. A chave de teste `BT3_TEST_READ_FAIL=<n>` faz a n-ésima leitura não entregar nada.
- **O que pode quebrar:** `exit(2)` novo quando o modelo não existe no início da batalha; a troca de forma que falha deixa o lutador com o modelo antigo e as animações novas.
- **Como testar de novo:** jogar com `BT3_TEST_READ_FAIL=2000` em `bt3_env.txt`: o log diz `reading ...: 0 of N bytes (I/O error); after opening it again N` e a luta segue. O próximo log real de jogador diz o `errno`.

### 1.0.0 (este commit): tamanho negativo pedido pelos dados vira aviso de disco modificado
- **Onde:** `port/src/plat_mem.c`, `Port_LowAlloc`
- **O que e por quê:** "no memory below 4 GB for 18446744073709531552 bytes" (3 aparelhos, 26 linhas) é -20064 lido do disco, em imagens com 3400 arquivos no primeiro AFS (o original tem 3399). O app mostrava falta de memória; agora a frase cai no `Problem.DATA` de `LastRun.kt`.
- **Medido/provado:** linhas dos logs; compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** nada para o disco original, que nunca pede tamanho negativo.
- **Como testar de novo:** abrir o jogo com uma imagem assim e ler a mensagem do último erro.

### 1.0.0 (este commit): parada do Tag e modelo ilegível aparecem como último erro
- **Onde:** `android/.../data/LastRun.kt`, `diagnose`
- **O que e por quê:** `Native Tag Team stopped` e `cannot be read from the game data` terminam em `exit(2)` sem assinatura de crash, e o app fechava sem dizer nada.
- **Medido/provado:** leitura de `diagnose`; compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** nada: só amplia o que se reconhece.
- **Como testar de novo:** `BT3_TEST_READ_FAIL` na primeira carga de modelo: o app mostra o motivo ao voltar.

### 1.0.0 (este commit): letreiros e Menu font leem o arquivo AFS inteiro
- **Onde:** `port/src/gs/menu_labels.c`, `ml_read_one`; `port/src/plat_fcache.c`, `Port_AfsSpanShared`
- **O que e por quê:** é a linha mais comum dos logs (69 aparelhos): o Android guarda o AFS inteiro e `ml_read_one` só procurava arquivos soltos. Dizia que faltavam dados do jogo, o Menu font só oferecia "Original" e o letreiro do item de PC não vinha do disco. A consulta usa o trinco porque a lista é feita em outra thread e `Port_AfsSpan` lê a tabela na primeira vez.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`. Com a gravação a lista é feita antes de desenhar, então o teste de pixels cobre o caso.
- **O que pode quebrar:** início um pouco mais lento na primeira abertura, em thread própria; `labels.tsv` passa a existir em `gamedata/game-text`.
- **Como testar de novo:** abrir o jogo com o log ligado: sem "no list of labels", e Settings > fonte de menu oferece mais que Original.

## Histórico: 0.9.0 para trás

Linhas: lado novo do diff naquele commit; o arquivo de hoje tem outra numeração. Fontes: mensagens de commit, diffs, changelogs de cada tag e
`docs/release-0.9.0.md`. Mais novas primeiro. Os commits até `b5ba41b7` são da 0.2.0, porque a tag `android-v0.1.0` fica
em `4d0ce356`, antes de o motor compilar para arm64.

### 0.9.0 744447a6: modelo vazio cai no primeiro traje
- **Onde:** `src/battle/btl_obj.c` `BtlObj_PortModel`, depois de `BtlRes_GetWork`, e `BtlObj_RequestCharaModel`; `src/battle/battle_load.c` `BtlLoad_StepChara`, cerca de 405; `src/battle/btl_scene.c` `BtlScene_TestCharPackBit`, cerca de 245.
- **O que e por quê:** traje ou forma que o personagem não tem dá um modelo sem tabela, e a batalha caía ao ligá-lo. Usa o primeiro traje, nos dois jogos de uma partida online. Pacote ausente não cai mais no endereço 4.
- **Medido/provado:** não registrado no commit. O crash continuou: a entrada 1.0.0 desta área mediu `BtlObjMdl_BindFiles+0x30` em 10 aparelhos e 57 eventos.
- **O que pode quebrar:** troca de modelo tem de dar o mesmo resultado nos dois lados do online, ou dessincroniza; depende de `File_GetSize` igual nos dois.
- **Como testar de novo:** o log diz "model file %d of fighter %d is empty". `BT3_TEST_READ_FAIL` na 1.0.0.

### 0.9.0 5707f85b: crashes dos logs: heap sem espaço, modelo não lido, rosto ausente
- **Onde:** `src/sys/file.c` `File_PortNoRoom` e `File_LoadSyncEx`; `src/battle/btl_obj.c` `BtlRes_PortReloadModel`; `src/sys/gfxm_e_b.c` `BtlObjMdl_Init`; `src/battle/hud_b.c` `HudGauge_DrawFace`, `HudGauge_ClearFaceTexMark`; `port/src/plat_fcache.c` `slot_of`, cerca de 133.
- **O que e por quê:** logs de jogadores. Sem espaço no heap, o PS2 seguia com buffer nulo e a leitura caía no endereço 0: `BtlObjMdl_BindFiles+0x30`, endereço 0x6c, em lutas de equipe. O bloco agora vem da memória do port. Rosto sem imagem: `HudGauge_DrawFace+0xac`, endereço 0x68, com discos modificados. Arquivo ausente pedido sem parar: um log chegou a 120 MB; agora escreve as primeiras vezes e em cada potência de dois.
- **Medido/provado:** mensagens dos logs; correção sem medida. A releitura lê do mesmo `FILE*` e não resolvia a causa, segundo a entrada 1.0.0.
- **O que pode quebrar:** `Port_GameBigAlloc` em aparelho com pouca RAM; `Heap_Free` precisa reconhecer esse bloco (`Port_GameBigFree`).
- **Como testar de novo:** o log diz "game heap: no room for file"; luta de equipe com 5 lutadores no perfil fraco.

### 0.7.3 2d8a5a72: heap do jogo muda de lugar se o endereço está ocupado
- **Onde:** `port/src/plat_mem.c` `map_heap`, linhas 326-352.
- **O que e por quê:** Redmi com Snapdragon 8s Gen 3 e HyperOS 2: "cannot map the game heap at 0x003BE000", o jogo fechava na hora. O heap agora vai para o primeiro lugar livre entre 0x30000000 e 0x70000000, como no Windows. Nada no jogo depende do endereço.
- **Medido/provado:** relato do jogador; testes no aparelho citado: não registrado.
- **O que pode quebrar:** tudo que assuma `HEAP_BASE`. Usa `sHeapNext` e `sHeapEnd`.
- **Como testar de novo:** o log diz "the game heap is at 0x%08X" onde o endereço está ocupado; `BT3_FORCE_MOVE=1` testa a mudança do motor, não a do heap.

### 0.4.0 928b2413: vigia e checagem de dados desligados no Android
- **Onde:** `android/app/src/main/cpp/loader.c` `SDL_main`, cerca de 693; `port/src/plat_verify.c`, novo; `port/src/plat_crash.c`, 101-104 e 143.
- **O que e por quê:** o vigia custava uma volta por todas as threads a cada 10 s em todos os aparelhos, e agora só liga com `BT3_WATCHDOG=1`. O app já confere a imagem ao instalar, então a checagem do PC fica desligada por padrão no Android.
- **Medido/provado:** custo não registrado.
- **O que pode quebrar:** jogos sem vigia não dizem onde pararam: para investigar travamentos, ligar `BT3_WATCHDOG=1` e `BT3_PEEK`.
- **Como testar de novo:** `BT3_WATCHDOG=1` em `bt3_env.txt`.

### 0.2.0 0ab41362: motor carregável em qualquer lugar abaixo de 4 GB
- **Onde:** `port/tools/android.py` `link`, 266-340; `android/app/src/main/cpp/loader.c` `custom_driver`, 434-536, e `SDL_main`, 601-667; `port/src/plat_mem.c`, 559-584; `port/src/plat_gs.c`, 12-17; `port/tools/irfix.py`.
- **O que e por quê:** alguns aparelhos põem o heap Java sobre 0x02000000-0x22000000, onde estavam motor, pilha e registradores. O link é feito duas vezes (0x03000000 e 0x13000000), e toda palavra que difere exatamente pelo deslocamento vira uma relocação. O loader soma o desvio e roda os inicializadores. Registradores do PS2 viram arrays.
- **Medido/provado:** `libbt3.so.rel` com 4975 palavras, nenhuma ambígua (mensagem do commit). Aparelhos testados: não registrado.
- **O que pode quebrar:** `.rel` e `.dat` têm de vir da mesma compilação do `.so`. `state.c` soma `BT3_ENGINE_BIAS` nas regiões do online (`928b2413`).
- **Como testar de novo:** `BT3_FORCE_MOVE=1` em `bt3_env.txt` e uma luta.

### 0.2.0 0e56e90c: endereços reservados ao carregar `libmain.so`
- **Onde:** `android/app/src/main/cpp/loader.c`, 179-231 e 498-503; `port/src/plat_mem.c`, 184-198, 202 e 601.
- **O que e por quê:** as pilhas de GPU e áudio do SDL podiam cair sobre o heap do PS2, a pilha, a região do port, os registradores e o motor. Um construtor reserva o espaço antes de o SDL começar, e `plat_mem.c` mapeia por cima (`BT3_LOW_RESERVED`). Faixas ocupadas vão para o log.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** ordem de construtores; reserva nova no loader sem o `plat_mem.c` saber.
- **Como testar de novo:** ler no log as faixas ocupadas num aparelho novo.

### 0.2.0 d285f5d6: AFS lido pela tabela, em vez de 20 mil arquivos soltos
- **Onde:** `port/src/plat_fcache.c` `Port_AfsSpan`, 27-86, e `Port_FileReadAt`, 183-186; `port/src/plat_file.c` `Port_FilePath`, 133-177; `port/src/plat_mem.c` `Port_LoadGameData`, 400-405; `android/app/src/main/cpp/loader.c`, 321-333.
- **O que e por quê:** escrever cerca de 20 mil arquivos pequenos deixava a instalação lenta. O instalador copia os arquivos AFS inteiros, e o motor lê pela tabela quando não há arquivo solto. Imagem modificada instala com aviso (`BT3_MODDED_DISC`).
- **Medido/provado:** tempo de instalação nesse commit: não registrado. Device lab, depois: 2,8 GB em 7 s.
- **O que pode quebrar:** discos modificados de outro layout; `Port_FileReadAt` é o caminho do maior crash da 1.0.0.
- **Como testar de novo:** importar por MediaStore e abrir; o log diz "archive ...: N files".

### 0.2.0 07280401: vetores por valor no ABI do AArch64 (crash no choque de raios)
- **Onde:** `include/sys/math3d.h`, 15-23; `src/battle/eft_i.c` (13 trechos), `eft_c.c`, `eft_h.c`, `eft_j.c`, `eft_k.c`, `eft_l_d.c`, `eft_w.c`, `btl_char_fx.c`; `port/tools/android_abicheck.py`, novo.
- **O que e por quê:** protótipos locais usavam `Vec4` (alinhamento 4), e as definições, vetores de alinhamento 16. No AArch64 o alinhado começa num par par de registradores, e todo argumento seguinte era lido do registrador errado: `EftPart10_DrawQuadClipped` recebia textura nula.
- **Medido/provado:** `android_abicheck.py` não acha outra diferença entre arquivos.
- **O que pode quebrar:** novo protótipo local em código do PC: a mescla pode trazer outro. O compilador não reclama, porque os arquivos são separados.
- **Como testar de novo:** `python3 port/tools/android_abicheck.py`; choque de raios numa luta.

### 0.2.0 da357474: LLVM 21 e gravação estreita por ponteiro de 4 bytes
- **Onde:** `port/tools/irfix.py`, 43-74 e 80-83; `port/tools/android.py` `compile_ee`, 103.
- **O que e por quê:** `store i8/i16` por `addrspace(271)` virava gravação de 4 ou 8 bytes e apagava os bytes depois de cada campo estreito. Sintoma: a tela do memory card presa, porque `ColorFade_Draw` zerava o alfa. `irfix` agora troca o ponteiro dessas gravações por um comum.
- **Medido/provado:** o jogo passa do memory card e joga uma luta no Odin 3 (`957c0d2a`).
- **O que pode quebrar:** trocar a versão do LLVM: refazer o teste antes de tirar a correção.
- **Como testar de novo:** compilar com `python3 port/tools/android.py` e passar da tela do memory card na primeira abertura.

### 0.2.0 52232f3a: lista de dados deslocada
- **Onde:** `port/tools/android.py` `finish`, 306-319.
- **O que e por quê:** o nome da partição AFS saía "afs" em vez de "pzs3us1". `android.py` tomava o primeiro rótulo de cada arquivo `port/data` como início dos dados, mas há bytes antes dele, e toda região era copiada alguns bytes fora.
- **Medido/provado:** os 69 arquivos conferidos contra o mapa do lld.
- **O que pode quebrar:** qualquer arquivo novo em `port/data`.
- **Como testar de novo:** primeira abertura: o jogo acha `pzs3us1`.

### 0.2.0 69457fad: motor em 0x03000000, região do port abaixo do heap Java
- **Onde:** `port/src/plat_mem.c`, 406-414 e 535-544; `port/tools/android.py`, 13 e 59; `android/app/src/main/cpp/loader.c`, 5-6.
- **O que e por quê:** o processo de um app reserva o heap Java a partir de 0x12C00000, o que colide com o layout do PC (0x20000000 e 0x13000000). Os 300 MB baixos estão livres: motor em 0x03000000, região do port em 0x05000000..0x0F000000. O scratchpad vira opcional: "the game itself addresses nothing in it".
- **Medido/provado:** não registrado.
- **O que pode quebrar:** `gPortScratchpadMapped` nas regiões do online (`state.c`); aparelhos onde a faixa baixa está ocupada: ver `0ab41362`.
- **Como testar de novo:** abrir num aparelho novo e ler o log do loader.

### 0.2.0 dce2c134: o motor no arm64
- **Onde:** `android/app/src/main/cpp/loader.c`, novo; `port/src/plat_mem.c` (23 trechos); `port/src/headless.c`, 28; `port/src/plat_crash.c`, 128-131; `port/tools/android.py`, novo.
- **O que e por quê:** ponteiros de 4 bytes do jogo em AArch64 (LLVM 20 ou mais novo, `ptr32.py`); float do PS2 pela ABI soft (`-mabi=aapcs-soft -mgeneral-regs-only`); sem handlers de sinal no Android, porque o runtime Java os usa. Depois, `4edb0841` passa a logar o crash antes do handler do Android, e `1eb77ecc` cria o vigia.
- **Medido/provado:** motor, Vulkan e áudio sobem no Odin 3 em 1920x1080 2x (`def0472b`).
- **O que pode quebrar:** tudo; mudar flags de compilação pode trocar a matemática do jogo e dessincronizar o online.
- **Como testar de novo:** `python3 port/tools/android.py`; luta gravada e `ppmcmp.py`.
