# Online

Regras e formato: [README](README.md).

### 1.0.0 (este commit): `Tb_Vanilla` sem o desvio morto da mescla
- **Onde:** `src/menu/port_tbo.c`, `Tb_Vanilla`, linhas 628-634
- **O que e por quê:** a mescla automática juntou o `state < 0` do Regulus (deixar entrar quando o fingerprint não foi
  calculado) com o `state != 1` da 0.1.39, que já retorna antes. O bloco nunca rodava. Fica o da 0.1.39:
  `Port_DataFingerprintNeed` calcula o fingerprint no Android e o jogador espera por ele.
- **Medido/provado:** leitura; a partida rápida é testada na bateria.
- **O que pode quebrar:** Partida Rápida e Ranked no Android, se o fingerprint não bater com o do disco: a tela
  "Sem Partida Rápida" apareceria para todos.
- **Como testar de novo:** Dragon Net Online › Partida Rápida com a ISO original: a busca tem de começar.


### 1.0.0 (este commit): correções pequenas de rede da revisão
- **Onde:** `data/OnlineStatus.kt` (`NET_VERSION`); `port/src/gs/net_match.c` (`"http":429`, `json_watchers`, `relay` com partida rodando); `net_lobby.c` (`MATCH_CONNECT_NS` e estouro de `goNs`, `BT3_LOBBY`); `net_relay.c` (`BT3_RELAY_NOCHAN`)
- **O que e por quê:** banner online consultava a versão 5 com o jogo em 6; "429" era procurado na resposta inteira, e um endereço `:54290` recusava um join válido (cerca de 0,5% em simulação da revisão); `addr` comparado com lixo de pilha subia `watchGen` sem mudança; o host que mantém a sala para espectadores podia pedir login TURN e alocar relay no socket de partida em andamento; lobby exigia 40 s contra os 60 s de `meet` para máquina lenta, e com `goNs` no futuro a mensagem repetia a cada 20 ms; `getenv` em todo blank e em todo pacote relayado.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** join por relay no host que conecta depois de 4 s; banner online; lobby que demora.
- **Como testar de novo:** Dragon Net e lobby entre os dois aparelhos com `BT3_RELAY_LOG=1`: sem linha "relay: asked for" depois da partida começar.

### 1.0.0 (este commit): limites de pedidos: Tag Team e BT2, segundo plano, TLS do relay
- **Onde:** `port/src/gs/tag_link.c` (laço de `poll`, `pollEvery`); `port/src/gs/net_match.c`, `gPortInBackground`, `http_post_to`; `port/src/gs/gs_gpu.c`; `data/Bt2Online.kt`; `engine/Bt2OnlineOverlay.kt`; `engine/Bt2GameActivity.kt`; `engine/RelaySocket.kt`, `connect`
- **O que e por quê:** Tag Team e BT2 pediam uma vez por segundo por sala aberta (60 pedidos/min em Worker grátis), sem recuar em 429 ou erro 1027, e continuavam com o app em segundo plano. Agora: 2 s por sala, recuo de 30 s dobrando até 10 min com a mensagem "Servidor ocupado", e nenhum pedido em segundo plano. O BT2 destruía a sala em `onPause`: um código passado pelo chat perdia a sala; agora a sala fica e só encerra em `onDestroy`. O socket do relay validava o certificado mas não o nome do servidor e não tinha tempo limite, e a chave da sala vai no endereço: agora confere o nome e tem tempo limite.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** quem entra numa sala demora até 2 s a mais para ser visto; sala de BT2 em segundo plano continua contando os 10 minutos do serviço.
- **Como testar de novo:** `BT3_NET_TEST_429=1` mostra a pausa no Dragon Net; sala de Tag aberta, app em segundo plano: o log do Worker não recebe `poll`.

### 1.0.0 (este commit): nome de usuário do relay do BT2 com limite
- **Onde:** `bt2/plugin/online/relay.cpp`, `Relay::begin`
- **O que e por quê:** o nome entra em mensagens de tamanho fixo (768 bytes) sem limite, e só o realm e o nonce eram limitados em `takeLogin`. Um nome acima de cerca de 316 bytes estouraria o buffer. Com 128 ou mais, o relay falha e o jogo segue sem ele.
- **Medido/provado:** revisão de código; compilado em `libbt2online.so`. Sem teste com servidor.
- **O que pode quebrar:** nomes vêm do token do servidor e são curtos. Só um nome anormal passa a falhar.
- **Como testar de novo:** BT2 online com relay, nome normal: entra como antes.

### 1.0.0 (este commit): BT2 online nos três idiomas
- **Onde:** `android/app/src/main/java/com/dfdx047/dragonrage/data/Bt2Online.kt`, `tr` e `explain`; `engine/Bt2OnlineOverlay.kt`, todos os textos da tela
- **O que e por quê:** as telas da sala, da busca e os erros do BT2 online só existiam em português. Agora seguem o idioma do aparelho: inglês, português ou espanhol, com inglês para os demais.
- **Medido/provado:** compila; `grep` sem texto fixo restante na tela. Não vi as telas em aparelho nos três idiomas.
- **O que pode quebrar:** texto longo pode não caber no painel, que ocupa 46% da largura.
- **Como testar de novo:** trocar o idioma do aparelho e abrir Online Mode no BT2: nome, criar sala, entrar com código errado.

### 1.0.0 (este commit): BT2 online: a partida abre e chega sozinha à seleção de personagens
- **Onde:** `bt2/plugin/online/fight_session.cpp`, `FightSession::padField` (fase Booting); `fight_session.h`, `Env::bootPress`; `bt2/plugin/online/online_plugin.cpp`, `startFightProcess`, `fightFrame`, `framePad`, `walk_press`, `fight_booted`; `bt2/plugin/online/session.h`, `fieldsDiffer`
- **O que e por quê:** a partida roda em motor novo, que anda sozinho do início até a seleção. Nos aparelhos isso nunca passava do título: a tela ficava preta para sempre, com o "Press START" por baixo, porque os toques escritos na memória do controle não chegam a esses menus. Agora o início e a caminhada apertam pelo próprio controle (`Env::bootPress`). O primeiro X no Dueling espera 120 quadros, porque o menu não aceita toque enquanto entra na tela. O log novo diz cada troca de modo. `fieldsDiffer` não acusa mais diferença quando o outro lado só foi conhecido pelos pacotes de lockstep (dizia "1909 and 0: DIFFERENT").
- **Medido/provado:** RG Cube e Retroid Pocket 5, sala por código pelo serviço real: modos 1, 4, 30 e 31 no log, "2023 and 2023: the same", imagem na seleção de personagens igual nos dois. Lutas de 11100 e 5700 quadros com toques nos dois lados: 0 "two games differ", mesmo quadro nos dois, 100%. Cube derrubado: RP5 diz "O oponente saiu ou foi desconectado" e reinicia o motor normal. `session_tests`: 2144 checagens, 0 falhas.
- **O que pode quebrar:** o tempo da caminhada é contado em quadros: uma versão do jogo com menus de outro tamanho não chegaria (só um SHA-256 entra). Se o jogo parar no Dueling, o jogador ainda aperta X, porque a linha já está de pé.
- **Como testar de novo:** duas instalações na mesma versão: Online Mode, Criar Sala, Entrar em Sala. Log: `online: the game's mode is 31` e `both are at the character select`. Lutar e conferir `grep -c "two games differ"` em `bt2/user/last-run.log`. `port/tools/lab/bt2room.sh` leva os dois até a sala.

## Histórico: 0.9.0 para trás

Linhas: lado novo do diff naquele commit; o arquivo de hoje tem outra numeração. Fontes: mensagens de commit, diffs, changelogs de cada tag,
`docs/release-0.9.0.md`, `docs/device-lab-2026-10.md` e `docs/DRAGON_NET.md`. Mais novas primeiro.

### 0.9.0 sem commit: abertos do lado do servidor e do aparelho
- **Onde:** `port/dragonnet/presence.js`, `port/dragonnet/ranked.js`; `docs/release-0.9.0.md`, seção 5.
- **O que e por quê:** servidor A: `POST /v1/presence` responde 404 `{"error":"no room"}`, e no B responde 200: falta publicar a rota no Worker do A, e o "Online" da página aparece indisponível. Servidor C: 401 `{"error":"identity"}` no perfil e 503 `{"error":"ranked unavailable"}` na tabela, então a busca ranqueada não pôde ser medida até o fim. Cube: `restricted_networking_mode=1` deixa o app em `REJECT_ALL`; é configuração do aparelho.
- **Medido/provado:** respostas lidas nos testes de 24 e 25/10. Se o servidor já mudou: não registrado, e não dá para saber pelo código.
- **O que pode quebrar:** testar o online no Cube sem desligar a rede restrita dá falso negativo.
- **Como testar de novo:** `curl` nas rotas; `adb shell settings put global restricted_networking_mode 0` no Cube, só com autorização do dono do aparelho.

### 0.9.0 89a39bb8: aviso de limite na Classificatória; teste 429 que liga no Android
- **Onde:** `port/src/gs/net_dragon.c` `worker`, 585-586; `port/src/gs/net_match.c` `limit_test`, 553.
- **O que e por quê:** uma falha de perfil por limite apagava o "Servidor ocupado". `BT3_NET_TEST_429` lia o ambiente pelo cache do PC, que guarda a primeira resposta, e não ligava no Android; agora usa `SDL_getenv`.
- **Medido/provado:** RP5: "hello", "presence" e "ranked/profile" pararam por 30 s, A passou para B, aviso na tela.
- **O que pode quebrar:** chave em `bt3_env.txt` esquecida faz todo pedido virar 1027.
- **Como testar de novo:** `BT3_NET_TEST_429=1` em `bt3_env.txt`.

### 0.9.0 0c7ed754: freio de HTTP 429 por serviço
- **Onde:** `port/src/gs/net_match.c` `limit_of`, `limit_before`, `limit_after`, 487-587, `http_post_to`, 593-603, `Match_PostDragon`, 1306-1312; `port/src/gs/net_dragon.c` (12 trechos); `data/OnlineStatus.kt`; `port/src/gs/ui_dragonnet.inc`.
- **O que e por quê:** device lab: `mitigo-rooms` respondia 429, erro 1027 do plano grátis. Num 429 o serviço não recebe nada por 30 s, dobrando até 10 min; resposta boa zera. Limite diário no A vai para o B na hora, em vez de depois de três falhas. Busca a cada 4 s, 8 s e 15 s; fila só com a tela aberta.
- **Medido/provado:** contador do log, RP5. Página parada: 1 presença ao abrir e nada em 3,5 min. Classificatória: 3 pedidos em 176 s. Resultado esperando rival: de até 30 por minuto (pelo código da 0.8.1) para 5 s dobrando até 1 min.
- **O que pode quebrar:** procurar "429" na resposta inteira recusava join válido: corrigido na 1.0.0. Só há 4 entradas em `sLimits`: um quinto serviço substitui outro.
- **Como testar de novo:** `BT3_NET_TEST_429=1`; ler no log "net: N requests to the online services in S s".

### 0.9.0 5707f85b: registro do Dragon Net espera entre tentativas
- **Onde:** `port/src/gs/net_dragon.c` `identity`, cerca de 278-305.
- **O que e por quê:** serviço sem pedidos do dia respondia 429, e o worker pedia de novo a cada 70 ms enquanto a janela atualizava. Agora espera 15 s, dobrando até 5 min.
- **Medido/provado:** não registrado no commit.
- **O que pode quebrar:** primeiro registro demora mais depois de uma falha.
- **Como testar de novo:** apagar `dragonnet-device.txt`, `BT3_NET_TEST_429=1`, abrir a Classificatória e contar os pedidos.

### 0.8.1 0b9e1c2a: servidores A, B e C; chat desligado; menos pedidos
- **Onde:** `port/src/gs/net_match.c` `DrNet_Url`, 112-169, e 29 trechos; `port/src/gs/net_dragon.c`; `port/src/gs/net_dragon.h` `DRNET_CHAT`, 22-27; `port/src/gs/ui_dragonnet.inc`, 374. Continuam em `c55cabac` e `d0b4bb8c`. Mesclado em `04321a8a` (PR 3). `3950958c` volta o `android.yml`.
- **O que e por quê:** o servidor grátis acabava com os pedidos do dia. A cai para B depois de 3 falhas, até 00:00 UTC. C é só a Classificatória. Presença a cada 5 min (antes 90 s), perfil a cada minuto (antes 15 s), busca ranqueada a cada 4 s (antes 2).
- **Medido/provado:** não registrado; o device lab achou o 429 logo depois.
- **O que pode quebrar:** dois jogadores precisam estar no mesmo servidor: sala não encontrada se um caiu para o B. Arquivos `dragonnet_url*.txt` trocam o servidor.
- **Como testar de novo:** `dragonnet_url.txt` apontando para um endereço morto: tem de passar para o B em 3 falhas. Lobby entre os aparelhos.

### 0.8.0 f11e5f18: Dragon Net
- **Onde:** `port/src/gs/net_dragon.c`, novo; `port/src/gs/ui_dragonnet.inc`, novo; `port/src/gs/net_match.c`, 12 trechos entre 41 e 1090; `port/src/gs/net.c`, 1076-1080 e 1642-1677; `src/battle/battle_load.c` `BattleResult_Finish`, 1098-1103; `port/dragonnet/`; `docs/DRAGON_NET.md`. Acertos em `95ff74da` e `176d77e8`.
- **O que e por quê:** cada versão nova do PC parava o online do Android (aconteceu na 0.7.0 e na 0.7.3). Serviço próprio com o Switch (DBT-NX), versão 5001, Classificatória Elo, chat, presença. O modo PC continua como opção.
- **Medido/provado:** testes em `port/dragonnet/*.mjs`, resultado não registrado.
- **O que pode quebrar:** Switch e Android precisam da mesma versão (`docs/DRAGON_NET.md`); pontos só mudam quando os dois resultados batem; sair da luta conta derrota.
- **Como testar de novo:** Dragon Net entre os aparelhos, 10 minutos sem dessincronia; `node port/dragonnet/test.mjs`.

### 0.7.3 2d8a5a72: 0.1.37 e escolha do servidor de partidas
- **Onde:** `port/src/port_release.h`; `port/src/gs/net.c`; `ui/screens/SettingsScreen.kt`; `data/OnlineStatus.kt`.
- **O que e por quê:** o PC lançou a 0.1.37 e o serviço passou a exigi-la. Oficial ou Comunidade em Ajustes.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** `NET_VERSION` no launcher e no motor têm de andar juntos: errado na 1.0.0.
- **Como testar de novo:** banner do launcher; partida no modo PC.

### 0.7.0 1867006b: 0.1.36 e aviso da situação do online
- **Onde:** `data/OnlineStatus.kt`, novo, 70 linhas; `port/src/gs/ui_android.inc`; `port/src/port_release.h`.
- **O que e por quê:** a 0.6.0 era 0.1.35 e o serviço passou a recusar: "Update available, you have 0.1.35; 0.1.36 or newer is needed". O launcher agora consulta o serviço a cada abertura e diz o estado. Página Dragon Net Battle para toque.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** cada abertura é um pedido a mais no servidor grátis.
- **Como testar de novo:** abrir o launcher com e sem internet e ler o banner.

### 0.5.0 bbd4c2b5: logs do online e `match_url.txt`
- **Onde:** `port/src/gs/net_match.c`, 6 trechos entre 49 e 548.
- **O que e por quê:** relatos sem dados do primeiro dia. Erros, respostas do pareamento e estado do relay vão para o log. `match_url.txt` aponta para outro serviço sem nova build.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** arquivo esquecido na pasta muda de servidor sem aviso.
- **Como testar de novo:** partida online e leitura do log.

### 0.4.0 928b2413: salvar e restaurar estado em arm64
- **Onde:** `port/src/gs/state.c`, `port_jmp_save`, `port_jmp_jump`, 191-245; `state_save`, 315-319; `regions_init`, 50-88.
- **O que e por quê:** o PC usava `getcontext`, que o Android não tem. Salva x19..x30, sp e d8..d15 em assembly; x18 é da plataforma e fica. `regions_init` soma `BT3_ENGINE_BIAS` e só lista o scratchpad se ele foi mapeado.
- **Medido/provado:** changelog da 0.4.0: primeiros testes Android com Android; contra PC não verificado na época. Números: não registrado.
- **O que pode quebrar:** qualquer variável nova fora das regiões salvas dessincroniza no rollback. O compilador pode usar registrador novo se a ABI mudar.
- **Como testar de novo:** Dragon Net entre os aparelhos, 10 minutos sem dessincronia; `BT3_FORCE_MOVE=1` num deles.
