# Matchmaking por proximidade — BT3 e BT4 (branch 1.0.2-Regulus)

Como a partida rápida e o Ranqueado escolhem o adversário, o que mudou em novembro de 2026 e o que falta.
O código das regras está em `port/netgeo/match.js`, usado pelos dois servidores:

- **Dragon Net** (`port/dragonnet/worker.js`): BT3 1v1, BT4 1v1 (versão `140001`) e Ranqueado do BT3 (`ranked.js`).
- **BT4 Online** (`bt4/port/bt4online-worker/rooms.js`): Ranqueado do BT4 (fila `D20`).

## O fluxo, passo a passo

| Passo | Antes | Agora |
|---|---|---|
| **1. Região aproximada** | A Cloudflare informa, pelo IP da requisição (`request.cf`), latitude e longitude arredondadas (~100 km) e o continente. Sem GPS; o jogo não declara onde está. | Igual, e guarda também o país. |
| **2. Procura perto** | Aceitava na hora quem estivesse a até 2.500 km. | Aceita na hora quem tiver ping **estimado** de até 47 ms (mais ou menos os mesmos 2.500 km). Entre dois próximos, prefere o do mesmo país. |
| **3. Amplia a busca** | De uma vez: aos 25 s aceitava qualquer um, o mais próximo primeiro. | Em degraus: 55 ms aos 12 s, 120 ms aos 18 s, 180 ms aos 25 s, qualquer um aos 30 s, sempre o melhor primeiro. Um jogador próximo que chega alguns segundos depois ainda é escolhido antes de um de outro continente. |
| **4. Avalia a conexão** | Depois do par formado, os dois jogos medem o ping entre si (P2P) e escolhem o atraso de entrada (`net.c`, `sSetupRtt`). O ping não volta para o servidor. | Igual. |
| **5. Estabelece a partida** | Conexão direta; se não der, relay TURN da Cloudflare (anycast: fica no ponto da Cloudflare mais perto de cada um). Quem espera numa sala é o anfitrião. | Igual. |

**Ranqueado do BT3:** antes pulava os passos 2 e 3 e pareava só pelo Elo (Brasil × Japão podia sair na hora).
Agora, dentro da janela de Elo, segue as mesmas regras da partida rápida — **depois** de rodar
`ranked-migrate-loc.sql`. Sem a migração, ou quando a Cloudflare não informa o lugar, funciona como antes.

Escolher região ou "Qualquer lugar" no jogo funciona como antes (região escolhida: só ela até 40 s; depois qualquer um).

## A estimativa de ping

`estimateMs` = 10 ms + 0,015 ms por km + 30 ms quando os dois estão em regiões diferentes (as rotas entre
continentes costumam dar volta; América do Sul → Europa muitas vezes passa pelos EUA). Saindo de São Paulo:
Rio ~16 ms, Buenos Aires ~34, Bogotá ~75, Miami ~139, Lisboa ~161, Tóquio ~318.

É uma **aproximação**: provedor, rota, Wi-Fi e congestionamento não entram, porque o ping real só é medido depois
que o par já está formado.

## Por que estes limites (simulação)

`port/netgeo/sim.mjs` roda as duas regras com jogadores chegando ao acaso (maioria brasileira, jogadores nos seis
continentes). Resultado (média de 5 rodadas de 20.000 s cada):

| Movimento | Regra | Ping médio | Partidas >150 ms | Espera média | Espera p90 |
|---|---|---|---|---|---|
| 1 jogador a cada 30 s | antes | 91 ms | 27,4% | 36,1 s | 71 s |
| | agora | 91 ms | 26,2% | 35,1 s | 68 s |
| 1 a cada 10 s | antes | 73 ms | 19,6% | 19,2 s | 30 s |
| | agora | 70 ms | 17,0% | 18,3 s | 30 s |
| 1 a cada 3 s | antes | 51 ms | 11,2% | 11,0 s | 27 s |
| | agora | 47 ms | 7,5% | 10,6 s | 26 s |
| 1 por segundo | antes | 37 ms | 5,8% | 5,8 s | 20 s |
| | agora | 33 ms | 2,9% | 5,5 s | 18 s |

Melhor ou igual em todos esses indicadores; a única exceção é o p90 de ping no pico (48 ms contra 46). Uma
primeira versão (70 ms na hora, qualquer um aos 45 s) foi descartada: piorava a espera e o pico.
O "ping" da simulação é a própria estimativa: a simulação compara as regras, não substitui o ping medido.

## Privacidade e segurança

- O lugar vem da Cloudflare, no servidor: o jogo não consegue informar outro lugar (uma VPN consegue, como em
  qualquer serviço).
- Guardado só enquanto a busca existe (sala: até 10 min): graus inteiros, região e país. O adversário vê só a região.
- Nada de GPS, endereço ou cidade.

## Para entrar no ar

1. **Dragon Net:** no console D1, rodar `port/dragonnet/ranked-migrate-loc.sql` uma vez (só se quiser a mudança do
   Ranqueado). Depois `wrangler deploy` dentro de `port/dragonnet`. **Colar o código no painel da Cloudflare não
   funciona mais**: o servidor importa `../netgeo/match.js`, e só o wrangler empacota esse arquivo.
2. **BT4:** `wrangler deploy` em `bt4/port/bt4online-worker`, como já é feito.

Não precisa de APK novo: o jogo já manda `waited` e `area`, e mostra o tempo de `widen` que o servidor responde.

## Testes

| O quê | Como rodar |
|---|---|
| Regras, seis continentes, benchmark (~0,1 ms por escolha entre 50 salas) | `node port/netgeo/test.mjs` |
| Dragon Net com as regras novas | `node port/dragonnet/test.mjs` |
| Ranqueado do BT3 com localização (servidor real em SQLite) | `node port/dragonnet/ranked-geo-test.mjs` |
| Simulação antes × agora | instruções no topo de `port/netgeo/sim.mjs` |

`ranked-flow-test.mjs` falha cerca de 1 vez em 10 por IDs sorteados — igual antes destas mudanças.

## O que falta

- **Ping real na escolha:** o jogo mandar ao servidor o ping, o jitter e a perda que mede depois de conectar
  (passo 4), para o servidor aprender o ping real entre áreas e evitar jogadores com conexão instável. Precisa de
  mudança no BT3 e no BT4 e de APK novo.
- **Tag Team do BT3:** o relay é criado no continente de quem abre a sala, não da maioria.
- **Tag Team do BT4:** o código do servidor `/v1/tag` não está no repositório (item Z1 de `online_correcoes_2026-11.md`).
