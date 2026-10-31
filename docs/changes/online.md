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
