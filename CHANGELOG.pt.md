# Dragon Rage 1.0.0

**O Budokai Tenkaichi 2 só com a ISO, o Dragon Net Online e os lobbies do PC 0.1.39, o Tag Team Mod como opção e o cenário
distante do BT4 corrigido.** Testado num RG Cube (Mali-G57) e num Retroid Pocket 5 (Adreno 650), com luta gravada e comparação de
imagem pixel a pixel contra a 0.9.0.

## Budokai Tenkaichi 2
- **Só a ISO**: o motor vem dentro do app. Não há mais pacote de motor para importar; se você tinha um, ele é apagado depois de um
  aviso e o jogo passa a usar o do app.
- **A ISO fica onde está**: é lida no lugar, sem cópia. Só arquivos pequenos ficam na pasta do app.
- **ISO errada é recusada** com uma mensagem que diz o que foi encontrado e em qual aba instalar.
- **BT2 online**: partida entre dois aparelhos, com tela própria no menu do jogo.
- Um erro do jogo agora aparece na tela, em vez de tela preta. A escala de saída abaixo de 100% enche a tela.

## Budokai Tenkaichi 4
- **Cenário distante no renderizador Dragon Rage**: as montanhas não saem mais em blocos de ruído colorido. O desfoque de distância
  do jogo agora funciona.

## Aparelhos fracos
- **BT4**: o padrão agora é por aparelho. Aparelho fraco ou modo Lite usa o renderizador Dragon Rage; se o ARMSX2 ficar abaixo de 27
  atualizações por segundo por 5 s numa luta, a próxima abertura usa o Dragon Rage. Sua escolha em Ajustes sempre vale. Num RG Cube limitado
  a um chip como o G52 MC1, o padrão passou de 7,8 para 11,2 imagens por segundo; o jogo ainda não chega à velocidade total nesse chip.
- **BT3**: mesmo desempenho da 0.9.0 nesse aparelho limitado.

## Tag Team Mod
- Até 10 lutadores, com os modos e o Tag Team Online do mod de tehmufti e dos contribuidores, adaptados para o celular.
- **Vem desligado**: ligue em Ajustes. Desligado, ele não aparece nas lutas normais.

## Online
- **Dragon Net Online**: telas novas no lugar do Dragon Net Battle, com Classificatória, lobbies, espectadores, partida rápida por
  região e entrada por endereço (do PC 0.1.39).
- **Menos pedidos aos servidores**: nada em segundo plano, espera crescente quando o servidor recusa, e a mensagem "Servidor ocupado,
  tente mais tarde" no lugar de uma falha sem explicação.
- A música aleatória não separa mais os dois jogadores numa partida.

## Correções a partir dos logs dos jogadores
- **Menu font e letreiros**: não pedem mais dados do jogo que já estão na ISO.
- **Aviso falso de pacote de textura** com a pasta vazia.
- **Arquivo que falha ao ser lido**: o jogo diz o motivo, tenta de novo e a luta continua.
- **Disco modificado**: tamanho negativo vira um aviso claro, não "sem memória".
- `.7z` e `.rar` são recusados com uma mensagem clara. Sem internet, verificar atualização não aparece mais como falha.

## App e controles
- **O controle navega no launcher** de novo, no RG Cube e no Retroid Pocket 5.
- **Equipe e licenças** na tela Sobre.
- O mapeamento de controle é relido ao abrir o jogo.

## Equipe
Regulus CiberSec, z3xox (PC), Androidxxt01, jllacchua, victorgonl, tehmufti e os contribuidores do Tag Team Mod, Team BT4. Artistas:
MarshallPunk e pluantoine.

## Notas de atualização
- Instala por cima da 0.9.0 e mantém saves, ajustes e as ISOs importadas.
- O online pede a mesma versão nos dois aparelhos.
