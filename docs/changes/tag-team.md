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

### 1.0.0 (este commit): Tag Team como opção do app, e espaço para as opções
- **Onde:** `android/app/src/main/cpp/loader.c`; `engine/GameActivity.kt`, `getArguments`; `data/EngineSettings.kt`; `ui/screens/SettingsScreen.kt`; `port/src/plat_settings.c`, `MAX_SETTINGS`
- **O que e por quê:** o branch deixava `BT3_TAG_TEAM=1` fixo no loader. Agora vem do sexto argumento do app, com a chave em Settings; `BT3_TAG_TEAM=0` em `bt3_env.txt` desliga para comparar. `MAX_SETTINGS` foi de 200 para 512: as opções do mod e os botões dos controles enchiam a tabela.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** quem tinha `bt3_env.txt` antigo; BT4 e BT2 não recebem o argumento.
- **Como testar de novo:** Settings > desligar Tag Team: o menu principal volta ao da 0.9.0, sem as telas do Tag.

### 1.0.0 (este commit): carga rápida só nos modos do Tag Team
- **Onde:** `port/src/tagteam_trace.c`, `Port_FastFileQueue`; `src/sys/file.c`, `File_UpdateRequests`
- **O que e por quê:** `Port_FastFileQueue` valia em todo modo offline com o Tag ligado e usa o relógio de parede (até 2 ms por quadro): uma luta normal carregava em outro número de quadros e não repetia a gravação. Agora só vale com `TagTeam_Enabled` ou `TagTeam_Active`.
- **Medido/provado:** `BT3_TAG_LEGACY_LOADING=1` no Cube, com Tag ligado: 11 quadros da gravação com 0 pixels contra a 0.9.0 (`cube_legacy_vk1x`). Sem a chave os quadros diferiam em mais de 220 mil pixels.
- **O que pode quebrar:** luta normal volta a carregar na velocidade da 0.9.0; modo do Tag que entre sem `TagTeam_Enable`.
- **Como testar de novo:** `bash port/tools/lab/series_base.sh` com Tag ligado e `ppmcmp.py` contra a referência.

### 1.0.0 (este commit): Square no clash olha o lutador certo em partida normal
- **Onde:** `port/src/gs/gs_input.c`, `clash_square`
- **O que e por quê:** com o Tag ligado, `side` virava `TagTeam_ViewActor(socket)` mesmo em 1x1, e o convidado online (lutador 1) olhava o lutador 0.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** atalho de Square no clash nos modos do Tag.
- **Como testar de novo:** Dragon Net como convidado, forçar clash, apertar Square.

### 1.0.0 (este commit): Tag Team desligado por padrão
- **Onde:** `android/app/src/main/java/com/dfdx047/dragonrage/data/EngineSettings.kt`, `TAG_TEAM_DEFAULT`
- **O que e por quê:** decisão do desenvolvedor, e é o que a ordem manda se os modos normais não ficarem idênticos. Ligado, o Tag mostra o aviso de SELECT no menu principal, muda a seleção de personagem e liga a thread de render (`gs_core.c`, `render_thread_wanted`). O jogador liga em Configurações; vale na próxima abertura.
- **Medido/provado:** Cube, Vulkan 1x, gravação da luta, 160 quadros de 2430 a 7200 contra a 0.9.0. Tag ligado: difere de 2730 a 3120 (aviso de SELECT) e de 3600 a 5100 (seleção de personagem). Tag desligado (`BT3_TAG_TEAM=0`): 0 pixels, salvo 5700 a 5820, onde a própria 0.9.0 difere entre duas rodadas; contra a outra rodada da 0.9.0 (`cube_shot_vk1x_a`) os 11 quadros fixos dão 0 pixels. RP5, Vulkan 2x: ainda difere de 6450 a 7050, na demonstração da abertura em Namek, com Tag ligado e desligado: aberto, ver `docs/release-1.0.md`.
- **O que pode quebrar:** quem usava o Tag nas versões de teste precisa ligar a chave; o Tag Team Online também depende dela.
- **Como testar de novo:** instalação limpa: sem o aviso de SELECT no menu. Ligar a chave, reabrir: SELECT abre os modos.

### 1.0.0 (este commit): mudanças de desenho do Tag só valem em batalha do Tag
- **Onde:** `src/battle/eft_ae.c`, `EftVram_Init`, `EftVram_Reset`, `EftVram_Acquire`; `src/battle/stg_c.c`, blur; `src/sys/gfxm_b.c`, água
- **O que e por quê:** o branch trocou para todas as lutas a área de VRAM dos efeitos, o CLAMP do blur e a água. Em luta normal isso dava diferenças esporádicas de desenho. Agora olham `TagTeam_Active()`, e a luta normal desenha como na 0.9.0.
- **Medido/provado:** Cube: 0 pixels com o Tag desligado (entrada acima). No RP5 os números de 6450 a 7050 não mudaram com isto, então a diferença de lá vem de outro ponto.
- **O que pode quebrar:** efeitos e água dos modos do Tag, que mantêm o caminho do mod.
- **Como testar de novo:** batalha do Tag em cenário com água e com efeito de técnica; luta normal: comparar os 11 quadros fixos.

### 1.0.0 (este commit): luta normal volta ao clash e à música do disco
- **Onde:** `src/battle/btl_char_hit.c`, `BtlHit_CheckClash`; `src/battle/battle_load.c`, escolha de `rule->bgm` com música aleatória
- **O que e por quê:** o merge do Tag Team tirou o `return 1` da primeira detecção do clash, e a dupla (1,0) era olhada de novo em 1x1, com efeito sonoro em dobro: fora do Tag Team a função volta a sair na primeira. A música aleatória passou a sair das faixas liberadas no save e das adicionadas pelo jogador, listas diferentes em cada aparelho: online os dois podiam escolher faixas diferentes, com o mesmo `rand()` mas tamanhos de lista diferentes. Em sessão online a escolha volta a ser `rand() % 24`, como na 0.9.0.
- **Medido/provado:** 11 quadros fixos: Cube 1x, 0 pixels contra a 0.9.0. RP5 2x: 0 pixels de 5700 a 6300, e as mesmas 5 diferenças de antes de 6450 a 7050 (demo da tela de título, ver `docs/release-1.0.md`). A escolha online não tem teste próprio: a sessão direta do teste de 10 min não usa música aleatória.
- **O que pode quebrar:** em Tag Team nada muda (o guarda só vale fora dele). Offline, a lista do jogador continua valendo.
- **Como testar de novo:** luta 1x1 com clash: um efeito sonoro só. Online, música aleatória nos dois aparelhos: a mesma faixa.

### 1.0.0 (este commit): bloco repetido em `tag_net.c`
- **Onde:** `port/src/gs/tag_net.c`, laço do lobby
- **O que e por quê:** o merge deixou duas cópias do mesmo `if(host && phase==2 && config.party && config.practice && !all_ready())`. A segunda nunca fazia diferença e sai.
- **Medido/provado:** compila; a comparação é só de leitura, as duas cópias eram iguais.
- **O que pode quebrar:** nada: o mesmo teste já tinha rodado na linha de cima.
- **Como testar de novo:** lobby de treino em Tag Team Online, com host e um convidado que não confirma.

## Histórico: 0.9.0 para trás

Nada a registrar: a árvore de `e1b9207` não tem `tag_link` nem `TagNet_`, e nenhum commit até ali cita o Tag Team; ele só entra depois da 0.9.0, com `cb026e37`.
