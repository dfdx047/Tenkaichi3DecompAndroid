# Mapa de mudanças

Toda correção já estragou outra coisa em alguma versão. Este mapa diz, antes de mexer num arquivo, o que já mudou ali,
por quê, o que depende disso e como testar de novo.

## Regra

**Todo commit que muda comportamento atualiza o arquivo da área no mesmo commit.** Sem entrada, o commit não está
pronto. Commit só de documentação ou de ferramenta de teste não precisa de entrada.

## Como usar

1. Antes de mexer em `port/src/gs/gs_gpu.c`, procure o arquivo em [`by-file.md`](by-file.md): lá estão os commits que
   tocaram nele, com as linhas e o link para a área.
2. Leia as entradas da área e a parte "O que pode quebrar".
3. Faça a mudança, meça e prove como a entrada manda, e rode [`regression-tests.md`](regression-tests.md) antes de cada
   versão.
4. Depois de cada versão: `python3 port/tools/change_map.py` refaz o `by-file.md`.

## Áreas

| Arquivo | O que cobre |
|---|---|
| [`renderer-gs.md`](renderer-gs.md) | GS do Dragon Rage: Vulkan, GL, passes, DATE, texturas, Mali/Adreno |
| [`bt3-engine.md`](bt3-engine.md) | jogo descompilado, loader, memória |
| [`bt4.md`](bt4.md) | runner recompilado do BT4, ganchos, shim |
| [`bt4-armsx2.md`](bt4-armsx2.md) | renderizador ARMSX2 do BT4 |
| [`bt2.md`](bt2.md) | motor do BT2, ISO, aba BT2 |
| [`online.md`](online.md) | Dragon Net, Ranked, lobbies, espectadores, Tag Team Online, BT2 online |
| [`tag-team.md`](tag-team.md) | Tag Team Mod |
| [`launcher.md`](launcher.md) | import, drivers, perfis, Lite, atualização, textura |
| [`touch-input.md`](touch-input.md) | toque, controles físicos |
| [`audio.md`](audio.md) | som, músicas |
| [`build-ci.md`](build-ci.md) | toolchain, patch do SDL, CMake, assinatura, versionCode |
| [`merges-pc.md`](merges-pc.md) | cada mescla do PC e os conflitos resolvidos |

## Formato de cada entrada

```
### <versão> <commit>: <título curto>
- **Onde:** `arquivo` `função`, linhas `N-M` (intervalo do diff)
- **O que e por quê:** o que mudou, com o log ou o bug de origem
- **Medido/provado:** como
- **O que pode quebrar:** telas, modos e GPUs que dependem disso
- **Como testar de novo:** o comando, a gravação e a referência de pixels
```

Entradas novas ficam no topo da área. Numa entrada do commit em que ela nasce, o hash fica como `(este commit)`: o
`change_map.py` acha o hash pelo histórico do arquivo da área.
