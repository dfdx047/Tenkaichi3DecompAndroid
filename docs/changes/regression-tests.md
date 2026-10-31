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

`bash compare_env.sh <versão> <aparelho> <escala> <nome> "<env>"` faz a mesma rodada com chaves de `bt3_env.txt`:
`BT3_TAG_TEAM=0` para comparar sem o Tag Team, `BT3_TEST_CPUS=0-5` para o RP5 como aparelho de 2 núcleos grandes.

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
