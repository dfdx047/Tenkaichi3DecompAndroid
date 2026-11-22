# Roteiro dos testes físicos (níveis 3 e 4): PENDENTES, nada abaixo foi executado

Preparação: o mesmo APK nos dois aparelhos (o da branch `auditoria-selecao-online`); em cada um, ligar o diagnóstico: `adb shell setprop` não vale, use a variável pelo jogo
(`BT3_SELECT_DIAG=1`) ou a configuração `select_diag=1` em `bt3_settings.txt`. Guardar o log de cada um (`adb logcat -d | grep -E "bt3: (select-diag|net)"`). Para encerrar a partida se divergir: `net_desync_end=1`.
A tela se escolhe por `select_classic` (0 nova, 1 clássica) em `bt3_settings.txt`.

## Nível 3: dois aparelhos
| # | Jogador A | Jogador B | Observar |
|---|---|---|---|
| 1 | Nova | Nova | |
| 2 | Clássica | Clássica | |
| 3 | Nova | Clássica | caso que desincronizava |
| 4 | Clássica | Nova | |
| 5 | Alternância | Alternância | 3 partidas seguidas trocando `select_classic` entre elas (reiniciar o jogo após a troca) |

Em cada cenário, marcar passou/falhou: entrar na sala; seleção completa (lutador, costume, forma, mapa, música); início da luta; os dois `canon=` do log IGUAIS e os `vblank=` de
`leave` e de `battle introduction begins` parecidos nas duas máquinas; sincronização dos quadros (`bt3: net: the two games differ` NÃO deve aparecer); transformações e fusões; troca de personagens (equipes); habilidades; encerramento
(vitória, revanche); ausência de desconexão. Repetir com personagem repetido (espelho), com mapa aleatório e com música aleatória, em 1x1 e em equipes.
Critério: nenhuma linha `differ`, nenhum `desync`, `canon` igual, nenhuma desconexão atribuível à escolha da tela.

## Nível 4: Android x PC (só quando houver crossplay)
Android nova x PC clássico, e Android clássica x PC clássico, os mesmos passos. No PC rodar com `BT3_SELECT_DIAG=1` e comparar `canon` e a ordem (`flow … stageFirst=0` nos dois).
Confirmar também que o PC não precisa de nenhuma alteração nem sabe da tela nova.
