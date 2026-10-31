# Toque e controles

Regras e formato: [README](README.md).


### 1.0.0 (este commit): mapeamento de controle relido no Android
- **Onde:** `port/src/gs/gs_input.c`, início de `poll_pads`, `SDL_AddGamepadMappingsFromFile`
- **O que e por quê:** a leitura usava caminho relativo, que no Android o SDL procura nos assets do APK, e a escrita usava o diretório do jogo. Quem fez a configuração automática perdia o mapeamento a cada abertura, e cada nova configuração só acrescentava a mesma linha.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** controle com mapeamento salvo errado passa a valer de verdade na próxima abertura.
- **Como testar de novo:** configurar um controle sem mapeamento, fechar, abrir: continua configurado.

## Histórico: 0.9.0 para trás

Linhas: lado novo do diff naquele commit; o arquivo de hoje tem outra numeração. Fontes: mensagens de commit, diffs, changelogs de cada tag,
`docs/release-0.9.0.md`, `docs/device-lab-2026-10.md`. Mais novas primeiro.

### 0.9.0 81e16c65: controle clássico em tela 1:1
- **Onde:** `touch/ClassicPadView.kt`, 142-157 e 162.
- **O que e por quê:** no Cube (720x720), L2 e L1 sobrepostos no canto, círculo cortado à direita. As pílulas de fora ficam nas bordas, e o losango e o direcional entram até caber.
- **Medido/provado:** Cube, CLÁSSICO e MOBILE dentro da tela; RP5 em 16:9 sem nada mudar.
- **O que pode quebrar:** só o padrão muda; posição salva pelo jogador continua. O device lab também anotou, no layout padrão do Cube, os botões de troca e de carga sobre o direcional e a barra do editor sobre o medidor: se isso foi resolvido não está registrado.
- **Como testar de novo:** tela quadrada, sem `touch_controls.json`, os dois estilos.

### 0.9.0 8e838c71: START+SELECT fecha o menu
- **Onde:** `engine/GameActivity.kt`, 51 e 402-412.
- **O que e por quê:** ver [`bt4.md`](bt4.md). Os botões ficam fora do jogo enquanto o menu está aberto.
- **Medido/provado:** Cube, 3 vezes. No RP5 não dá para injetar: o teclado não entra em `PortInput_ComboPoll`, e o gamepad pede root.
- **O que pode quebrar:** START uns 50 ms antes do SELECT abre a pausa por baixo.
- **Como testar de novo:** Cube, BT3 e BT4.

### 0.9.0 a38b7c1f: toque escondido com controle de analógico
- **Onde:** `engine/GameActivity.kt` `stickPadConnected` e `devices`, 254-272; 121, 173 e 177.
- **O que e por quê:** o toque aparecia na abertura mesmo com controle ligado. Agora esconde se há eixos X e Y num aparelho real, e aparece no primeiro toque ou quando o último controle sai.
- **Medido/provado:** RP5 e Cube no device lab.
- **O que pode quebrar:** aparelho sem analógico embutido; teclas que dizem ser gamepad são excluídas por `realDevice`.
- **Como testar de novo:** abrir com e sem controle; conectar e tirar com o jogo rodando.

### 0.9.0 5707f85b: menu do jogo por toque, em Compose
- **Onde:** `engine/GameMenu.kt`, novo, 814 linhas; `engine/MenuBridge.kt`; `touch/TouchSurface.kt`; `port/src/gs/ui_android.inc`; `android/app/src/main/cpp/loader.c` (`Ui_Nm*`), 739-813. `744447a6`: `GameMenu.kt`, cor do texto.
- **O que e por quê:** menu por cima do jogo, ligado ao motor por `Ui_NmGet`, `Ui_NmSet` e `Ui_NmAction`. `744447a6` corrige texto preto: cor com alfa próprio não faz parte do esquema, e a cor do texto tem de ser dita.
- **Medido/provado:** device lab: tecla voltar abre o menu e pausa, nos dois jogos; páginas abrem.
- **O que pode quebrar:** temas; ajuste novo tem de existir nos dois lados da ponte.
- **Como testar de novo:** tecla voltar, cada página, nos temas claro e escuro.

### 0.6.0 7b99f8ad: botões de luta presos no modo história
- **Onde:** `android/app/src/main/cpp/loader.c` `nativePlayerTeamSize`, 796-807; `touch/TouchPadView.kt`.
- **O que e por quê:** o jogo mantém a última luta como em andamento no mapa e nos diálogos, e o layout automático ficava em luta. O loader agora lê `gPortInMenus`. Visual Dragão padrão.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** `gPortInMenus` errado em tela nova, como Tag Team e Dragon Net.
- **Como testar de novo:** modo história: mapa, diálogo, luta, mapa.

### 0.4.0 c3b7c86c: estilo clássico, layout automático, um botão nos choques
- **Onde:** `touch/ClassicPadView.kt`, novo; `touch/TouchConfig.kt`; `port/src/gs/gs_input.c`, 6 trechos entre 72 e 502; `engine/GameActivity.kt`.
- **O que e por quê:** troca sozinha entre menu e luta; Quadrado várias vezes em vez de girar o analógico nos choques; START+SELECT abre e fecha no físico e no toque.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** a opção de choque muda a entrada: conferir no online que os dois lados recebem o mesmo.
- **Como testar de novo:** choque de raios em toque e físico; alternar os estilos.

### 0.3.0 f19cbe5c: controles por jogo
- **Onde:** `touch/TouchPadView.kt`, `TouchConfig.kt` (894 linhas somadas em 10 arquivos).
- **O que e por quê:** não registrado na mensagem, que só diz "customizable game-specific touch controls".
- **Medido/provado:** não registrado.
- **O que pode quebrar:** `touch_controls.json` de versões antigas.
- **Como testar de novo:** editor, Pronto, fechar e abrir.

### 0.2.0 68aa7428: controle na tela e editor
- **Onde:** `touch/TouchPadView.kt`; `android/app/src/main/cpp/loader.c` `Port_TouchPad`; `ui/screens/ControlsScreen.kt`. Tecla voltar do Odin em `d285f5d6`.
- **O que e por quê:** primeira versão do controle: direcional, analógicos, macros, três visuais, `touch_controls.json` dividido com o jogo. Alimenta o pad do jogador 1 pelo loader.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** esconder ao usar controle físico; o overlay do sistema no RP5 cobre o "...": device lab.
- **Como testar de novo:** editor, Resetar, macros, no Cube e no RP5.
