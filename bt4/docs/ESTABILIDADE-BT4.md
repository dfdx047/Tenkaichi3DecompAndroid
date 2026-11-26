# BT4 Online: estabilidade, desempenho e revanche

Branch `bt4-estabilidade` (parte da `1.0.2-Regulus` local, só neste PC). Aberto em 26/11/2026. **Missão não concluída:**
nada aqui foi testado em aparelho.

## Versão que estava no celular

O log de 26/11 11:41 tem BuildId `e478315f...`, igual ao de `DragonRage-teste-menu-tagteam-v4-21-11.apk`. Essa versão
não tem o commit `d9021b8f` (22/11), que mexe na revanche e na tempestade.

## Crashes (logs de 17/11 a 26/11)

| Onde caiu | Última vez | Causa | Estado |
|---|---|---|---|
| `EftStorm_DrawRain` (mapas 72 e 79, atrás da seleção) | 26/11 00:11 | ponteiro `gEftStorm` nulo; **a origem do nulo não foi provada** | proteção em `d9021b8f` (esconde, não explica) |
| `BtlCharApi_GetRadius` (mapa 34, atrás da seleção) | 21/11 00:07 | tabela de objetos com os da luta anterior, já liberados | corrigido em `fce59cdf`; não reapareceu nos logs |
| `EftBubble_UpdateAmbient` | 17/11 20:23 | cenário da seleção sem lutadores | proteção em `504e5872` |
| `libc.so`, endereço 0 (4 vezes) | 18/11 05:54 | **desconhecida** (os logs não trazem a pilha) | aberto |
| `Bpe_Decode` (2 vezes) | 17/11 20:15 | **desconhecida** | aberto |

Achado ao lado do primeiro: `EftStorm_Term` liberava o bloco sem zerar o ponteiro, e `EftStorm_Init` liberava de novo.
Corrigido em `63be2f2a` (`src/battle/eft_g.c`). É o único efeito de cenário (`eft*.c`, `stg*.c`) com esse padrão. Não há
prova de que causava algum dos crashes acima.

## Revanche

1. **Caixa fecha com "conexão perdida" logo depois de abrir** (10 de 10 na última execução do log de 26/11): conta de tempo
   sem sinal em `rematch_box` (`port/src/gs/net.c`). Corrigido em `d9021b8f`.
2. **Só "voltar ao menu" já na luta 1** (mesmas 10): a caixa recebe do serviço `rematches_left = 0`. O serviço conta a
   partida e 3 revanches por dia contra o mesmo rival (`RANKED_PER_PAIR_DAY`, `bt4online-worker/profile.js`). É regra, não
   falha de mapa. **Decisão pendente:** permitir revanche que não pontua.
3. A revanche aceita (`BattleResult_Set(8, 0x8000)`) **nunca foi testada** com dois aparelhos.

Nos logs, nada liga a falha da revanche ao mapa.

## Desempenho, antes (APK v4, Redmi 2312DRA50G)

Média das linhas `speed ... fight` do log de 26/11. O mapa é o último que o cenário da seleção informou antes da linha, e
"fight" pode incluir trechos de apresentação: valores aproximados. 100% = velocidade normal do jogo.

| Mapa | Segundos medidos | Velocidade média | Pior | Imagens/s | Desenho, ms |
|---|---|---|---|---|---|
| 3 | 48 | 101% | 94% | 39 | 5,2 |
| 5 | 125 | 87% | 83% | 52 | 5,4 |
| 7 | 112 | 95% | 57% | 35 | 5,3 |
| 8 | 480 | 91% | 79% | 46 | 5,7 |
| 23 | 218 | 101% | 77% | 40 | 5,7 |
| 58 | 141 | 89% | 63% | 32 | 6,9 |
| 61 | 83 | 80% | 74% | 46 | 5,4 |
| 70 | 109 | 97% | 79% | 40 | 5,8 |

Gargalo (CPU, GPU ou rede) **ainda não separado**. Nenhuma otimização feita.

## Matriz por mapa

Todos os mapas: **Não testado** depois das correções. Com falha reproduzida em log antes delas: 72 e 79 (crash na seleção),
34 (crash antigo, já corrigido no código).

Para cada mapa, com dois aparelhos: seleção (cenário ao fundo), carregamento, luta, FPS, fim, revanche (3 seguidas), voltar
ao menu e nova partida, nos papéis de anfitrião e de convidado.

| Mapa | Seleção | Luta | FPS | Revanche | Revanches seguidas | Menu e nova partida | Online (2) | Classe |
|---|---|---|---|---|---|---|---|---|
| (preencher) | | | | | | | | Não testado |

## Próximos passos

1. Instalar o APK de teste nos **dois** aparelhos e exportar o log dos dois.
2. Varrer os mapas na seleção, começando por 72, 79 e 34.
3. Revanche com contas que ainda tenham revanches no dia.
4. Com os logs novos (`net: link, last 5 s`), separar rede de processamento.
