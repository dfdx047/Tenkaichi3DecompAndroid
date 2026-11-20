# Prévias do BT4 Online (com o Trunks)

Telas de prévia do menu Online do BT4, com a animação do Trunks (vídeo do autor) recortada.

## Pastas
- `trunks-recorte/`: 101 quadros PNG com fundo transparente (20 fps, loop de 5 s), todos na mesma caixa 436×993.
- `telas/`: as telas que vieram da sessão do PC (`01-principal.jpg`, `02-partida-rapida.jpg`, `05-entrar.jpg`,
  `10-fim-da-partida.png`, `folha-online-bt4.jpg` com as 10) e a tela do Tag Team usada como referência (`tagteam-ref.jpg`).
- `out/`: resultados (`00-principal-animada.webp`, `10-fim-da-partida-animada.webp`, `08-perfil-animado.webp`, os PNG
  parados e `fundo-limpo.png`, o fundo da tela principal sem o lugar reservado).
- `fonts/`: Kanit e Chakra Petch (licença OFL, arquivos `OFL-*.txt`).

## Scripts (rodar dentro desta pasta; precisam de numpy, pillow e opencv-python)
- `python3 recorte.py <pasta dos quadros> <pasta de saída>`: tira o fundo branco e a marca d'água dos quadros do vídeo
  (`ffmpeg -i video.mp4 -vf fps=20 quadros/f%03d.png`). Depois os quadros são cortados na caixa (30, 25, 466, 1018).
- `python3 montar_principal.py . 70 anim`: tela principal com o Trunks grande (estilo Tag Team). 70 = deslocamento para a direita.
- `python3 montar_fim.py . 700 anim`: fim da partida. 700 = posição x do Trunks.
- `python3 perfil.py . anim`: perfil (cartão, lutador mais usado, liga, últimas 10, troféus, medalhas, mais usados, top).
  Sem `anim`, sai só o PNG parado.

## No online_bt4.py (PC, resolução cheia)
Para o Trunks grande e perto, cortado nas coxas como no Tag Team:
`FIGHTER = dict(cx=1480, feet=1930, height=1890)` e `TRUNKS=trunks-recorte python3 online_bt4.py 0 1 10`.
Se ele cobrir o botão "Partida rápida", aumente o `cx`.
