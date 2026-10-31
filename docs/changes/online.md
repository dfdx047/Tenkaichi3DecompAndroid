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
