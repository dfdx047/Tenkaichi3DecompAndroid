# Bateria de regressão

Rodar antes de cada versão, nos dois aparelhos, normais e no perfil fraco. As referências de pixels ficam fora do git, em
`~/DragonRageRefs/<versão>/logs/`: são quadros tirados do jogo e não podem entrar no repositório. As ferramentas estão em
`port/tools/lab/`; as gravações `fight.rec` e `fight4.rec` ficam em `~/DragonRageRefs/`.

## Luta gravada e pixels

`fight.rec` é a gravação da 0.9.0, com 2 portas por quadro. A partir da 1.0 `src/sys/pad.c` lê 4 portas, então use
`fight4.rec` (feita com `port/tools/lab/rec4.py`). A gravação antiga tocada na 1.0 acaba no dobro da velocidade e sai do menu errado.

```sh
cd port/tools/lab
bash series_base.sh <versão> cube      # ou rp5: rodadas Vulkan e GL, com quadros 5700 a 7200
bash series_low.sh <versão>            # Cube como G52 MC1: lowend.sh on 512, depois off
python3 ppmcmp.py ~/DragonRageRefs/0.9.0/logs/cube_shot_vk1x_a ~/DragonRageRefs/<versão>/logs/cube_shot_vk1x_a
```

Passa com **0 pixels diferentes**. Antes de comparar versões, compare duas rodadas da mesma versão: elas têm de dar 0 entre si.
A referência de cada aparelho só vale para ele e para a mesma resolução.

No perfil fraco, compare sempre as versões alternadas na mesma sessão, pelo menos duas rodadas de cada (`ab_low.sh` no BT3, `ab_bt4.sh` no BT4; `threads.sh` mostra a CPU por thread durante a luta). Uma luta do BT4 não é determinística: duas rodadas da mesma versão já deram 53 e 68 ms de guest. Sessões separadas não servem: o calor e a cena mudam o resultado.

Para medir OpenGL, use `dr_renderer=2 gpu_api=1` (`finals2.sh`): `gpu_api=1` sozinho é reescrito pelo launcher e a rodada sai em Vulkan. Confira no log `renderer chosen`. Depois de um `bench.py`, apague o `bt3_env.txt`: o `BT3_PAD_PLAY` que ele deixa aperta os menus sozinho. Depois de testar idiomas, volte para "Sistema": os scripts procuram os textos em português.

`bash compare_env.sh <versão> <aparelho> <escala> <nome> "<env>"` faz a mesma rodada com chaves de `bt3_env.txt`:
`BT3_TAG_TEAM=0` para comparar sem o Tag Team, `BT3_TEST_CPUS=0-5` para o RP5 como aparelho de 2 núcleos grandes.

## Tempo dos testes

Combinado com o desenvolvedor na 1.0: só os 11 quadros fixos (5700 a 7200, de 150 em 150) em `compare_env.sh`, sem a comparação
densa (`dense.sh` fica para caçar uma diferença). Uma resolução por aparelho: Cube 1x, RP5 2x. Repetir medições só no que mudou.
A bateria completa roda uma vez, no build final. A demo da tela de título não conta como teste de jogo.

Limite dos 11 quadros: eles caem no fim da gravação, depois da luta. Menus e seleção de personagem só aparecem na rodada densa, feita
uma vez na 1.0 e obrigatória de novo se menu, fontes ou Tag Team mudarem.

## BT2

`bash bt2run.sh <rp5|cube> <segundos> <quadros> "<script>"` usa `pad_script` em `bt2/user/android` e conta o script em campos (cerca
de 60/s): nada de esperar pela tela. `bt2/tools/gate/pad-fight.txt` é a luta completa (boot, New Game sem salvar, Duel). Passa com
100% de velocidade, 30 fps e 0 travadas no contador do `sample = 1`. Tire `pad_script` e `sample` ao terminar: enquanto existirem, o
jogo ignora os botões de verdade.

## Online

`bash online10.sh <versão> [segundos]` faz uma sessão direta RP5 (host) e Cube, com botões aleatórios (`net_rand.tab`, de
`nettable.py`). Passa sem nenhuma linha "the two games differ" nos dois logs. O script libera `restricted_networking_mode` no Cube e o
põe de volta em 1.

## BT4

`bash series_bt4_rp5.sh <versão>` e `bash series_bt4_cube.sh <versão>` rodam ARMSX2 e renderizador DR, no Cube também no perfil
fraco. Confira `GAME`, `guest_ms`, `wall_ms` e o PNG de cada rodada: um PNG sem luta invalida a rodada.

## O que testar à mão

- **Jogos e renderizadores:** BT3, BT4 (ARMSX2 e DR), BT2; Vulkan e GL; Lite ligado e desligado; resoluções; 30 e 60 fps.
- **Tag Team:** todos os modos; modos normais com 0 pixels.
- **Importar:** as três ISOs por MediaStore e por pasta, ISO errada (a recusa diz o que achou), importar de novo, reiniciar o aparelho;
  pacote de textura, pasta vazia sem aviso falso, `.7z`.
- **Controles:** toque, físico, START+SELECT, menu por toque, 1:1 no Cube.
- **Drivers:** MaliSX2 no Cube, Turnip no RP5, e voltar ao do sistema.
- **Online:** Dragon Net Online e Ranked entre os aparelhos, 10 minutos sem dessincronia; lobby com espectador; Tag Team Online;
  BT2 online; modo PC; contagem de pedidos por minuto; `BT3_NET_TEST_429=1` mostra a pausa.
- **Outros:** atalho, exportar log, temas, idiomas, atualização que só verifica.

## Falhas forçadas

- `BT3_TEST_READ_FAIL=<n>`: a n-ésima leitura não entrega nada. O log diz o motivo e a luta continua.
- `BT3_NET_TEST_429=1`: toda resposta do Dragon Net vira 1027.

## Ao terminar

`bash lowend.sh off`, configuração de `~/DragonRageRefs/state/` de volta, sem `bt3_env.txt` novo (o Cube tinha um antes,
`BT3_GS_VERBOSE=1`), sem `fight*.rec`, saves com o mesmo md5, driver do sistema.

## 1.0.0: rodada final com três aparelhos (`quick.sh`, Mi A3 no laboratório)
- **Onde:** `port/tools/lab/lab.py` e `ui.py` (`DEVS`: `cube` 192.168.1.184, `rp5` 192.168.1.89, `xiaomi` 192.168.1.136); `port/tools/lab/quick.sh`, novo; resultados em `docs/release-1.0.md`, seção 0.
- **O que e por quê:** o Mi A3 (SD665, Android 17) entra no lugar do perfil do 480 Plus. `quick.sh <aparelho> <rótulo> [bt3|classic|tag]` roda as checagens rápidas num aparelho, guarda tudo em `~/DragonRageRefs/rel10/<rótulo>/`, apaga o `bt3_env.txt` e devolve os ajustes do começo.
- **Medido/provado:** a parte `tag` (5 contra 5 COM, `BT3_DEMO`) rodou no RP5 e no Mi A3, com o Gráfico leve desligado e ligado.
- **O que pode quebrar:** as partes `bt3` e `classic` usam `fight4.rec`, que saiu de passo com o jogo (fica no título e nas demonstrações); a rodada final usou rotas por teclas (`kpad.py`) e, no Mi A3, onde as teclas não chegam ao jogo, toques. `bt4route.sh`, `bt2route.sh` e `dia_smoke.sh` continuam presos ao RP5.
- **Como testar de novo:** `bash port/tools/lab/quick.sh xiaomi teste tag`, e conferir que `bt3_settings.txt` volta igual.
