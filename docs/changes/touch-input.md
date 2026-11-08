# Toque e controles

Regras e formato: [README](README.md).


### 1.0.0 (este commit): BT4, o controle na tela troca sozinho entre menu e luta
- **Onde:** `port/bt4/host/bt4_main.cpp`, `Java_com_dfdx047_dragonrage_engine_TouchBridge_nativePlayerTeamSize`.
- **O que e por quê:** o BT4 respondia sempre -1 ("não sei"), que o controle trata como luta: nos menus ficava o analógico, que os menus do BT4 não leem, e era preciso tocar no ✛. Agora a resposta vem do contador de atualizações da luta (`ps2FightTicks`, o mesmo que decide os 60 fps e o renderizador): andando mais de 5 por segundo é luta (1), parado é menu (0, uma pausa também, cujo menu quer o direcional). Lido a cada 0,3 s no máximo.
- **Medido/provado:** Mi A3, BT4 com o renderizador Dragon Rage: layout de menu (direcional) na abertura, no título e nos menus; o de luta (analógico) na luta 1P x COM e na luta de demonstração do título; de volta ao de menu quando a demonstração acaba (log `touch roster: overlay poll teamSize=0/1`).
- **O que pode quebrar:** a luta de demonstração do título mostra o layout de luta (o START continua no topo). Quem prefere um layout fixo desliga o automático no editor do controle.
- **Como testar de novo:** BT4 pelo toque, do título a uma luta e de volta ao menu; o mesmo com o ARMSX2.

### 1.0.0 (este commit): BT4 e BT2, um toque rápido fica até o jogo ler
- **Onde:** `port/bt4/host/bt4_main.cpp`, `sendPad`; `bt2/runtime/host/sdl/touch_pad.h`, `TouchPad::set`/`get`.
- **O que e por quê:** o BT3 guarda um toque que começa e acaba entre duas leituras do jogo (`gs_input.c`, `sTouchTapped`). O BT2 não guardava nada: lia só o estado do momento, uma vez por campo. O BT4 guardava até a próxima imagem do fluxo do GS, mas o jogo lê o anel quando quer, e várias imagens podem chegar juntas entre duas leituras dele. Agora um botão apertado fica no controle por 2 quadros do jogo (BT4, `g_bt3FrameCount`) ou 2 leituras (BT2), mesmo se o dedo já saiu. Em velocidade cheia são 33 ms: apertar repetido continua subindo e descendo.
- **Medido/provado:** Mi A3, BT4 (luta a cerca de 12 quadros por segundo): toques curtos (`input tap`) no START, no direcional e no cross levaram do aviso inicial ao título, ao menu e a uma luta 1P x COM. Não houve uma medição de antes que mostre um toque perdido: as rotas que falharam antes e depois desta mudança tocaram durante os logos, que ignoram o START. BT2: compilado e jogado no Mi A3 (menus e uma luta 1P x COM pelo toque, combo de 3 golpes) com toques de 250 ms; o toque curto não foi medido no BT2.
- **O que pode quebrar:** dois toques do mesmo botão em menos de 2 quadros viram um só (com o jogo lento, 2 quadros podem passar de 150 ms).
- **Como testar de novo:** BT4 e BT2 num aparelho fraco, toques rápidos no START e no direcional dos menus e nos golpes; disputa de raios apertando repetido no BT4 e no BT2.

### 1.0.0 (este commit): Toque Pro, o comportamento (analógico, área de toque, deslizar, toque alterna)
- **Onde:** `android/.../touch/TouchTuning.kt` (novo: `stickBytes`, `follow`, `inStickZone`, `slack`, `buzz`); `TouchConfig.kt` (`StickMode`, `Haptic`, `TouchPreset`, `TouchProfile`, campos `stickMode`, `deadZone`, `stickRange`, `stickZone`, `snap8`, `hitArea`, `slideAll`, `toggles`, `haptic`, `leftHanded`, `profiles`; `withPreset`, `mirrored`); `TouchPadView.kt` (`down`, `move`, `stick`, `recompute`, `releaseAll`, desenho) e `ClassicPadView.kt` (`down`, `stick`, `find`, `buzz`).
- **O que e por quê:** no toque o analógico era linear e sem zona morta (o polegar parado escorregava o lutador), só nascia no canto esquerdo, a área de cada botão era fixa e só os botões de rosto aceitavam deslizar. Agora: zona morta e inclinação total ajustáveis (a rampa começa na borda da zona morta, então andar devagar continua possível); analógico Fixo, Flutuante ou Dinâmico (o centro segue o polegar quando ele passa da borda: virar para o outro lado é imediato); a zona onde ele nasce é ajustável e troca de lado no Canhoto; a área de toque de todos os botões é ajustável; deslizar vale também para ombros e Start/Select; botões escolhidos (Carregar Ki, Defesa, R1, R2, L1) podem ser "toque alterna", segurados por um toque e soltos pelo próximo, sem prender o polegar. Um dedo que toca um botão alternado ou a troca de lutador não aperta outro ao escorregar. Os dois estilos (Mobile e Clássico) usam as mesmas contas.
- **Migração:** um `touch_controls.json` antigo sem `stickMode` vira Flutuante se `sticksFloat` estava ligado, senão Dinâmico; `haptics` vira `haptic`. Os campos velhos continuam gravados.
- **Medido/provado:** Mi A3 (SD665, Android 17), APK de teste com o perfil "Online competitivo": o arquivo gravado bate com o perfil (dinâmico, zona morta 8, inclinação 60, área 130, L2 alterna); menus do BT3 só pelo toque (Start, Cross, direcional), seleção clássica, luta 1P x COM a 30 fps e 100 %; analógico nascido sob o dedo move o lutador; um toque no L2 carrega o Ki sem segurar; deslizar do soco ao raio de Ki; 0 crash.
- **O que pode quebrar:** quem tinha o analógico fixo e `sticksFloat` desligado passa a ter o Dinâmico (o padrão novo); o perfil "Clássico (0.9)" volta ao comportamento antigo. Com a área de toque grande, dois botões próximos disputam a borda: vence o mais perto do centro (como antes).
- **Como testar de novo:** cada perfil numa luta; zona morta 0 e 40; Canhoto; L2 em toque alterna (toque liga, toque desliga, sair da luta solta); deslizar entre rosto e ombros com "Arrastar entre todos" ligado e desligado; o mesmo no estilo Clássico.

### 1.0.0 (este commit): Toque Pro, a parte da interface (Ajustes, guia, imagens)
- **Onde:** `android/.../ui/screens/TouchPro.kt` (novo, `TouchProSection`), usado em `ControlsScreen.kt` e `bt2/Bt2ControlsScreen.kt` no lugar das chaves "Vibração" e "Analógico flutuante"; `ui/screens/TouchGuide.kt` (novo, o guia com 6 desenhos em Compose); textos `tp_*` e `tpg_*` nos 3 idiomas (saem `touch_haptics*` e `touch_float*`); `port/tools/touch_guide_images.py` e `docs/images/touch-pro/01..06*.png` (os mesmos 6 desenhos para o changelog e o README).
- **O que e por quê:** quem joga sem controle físico, principalmente no online, precisava ajustar o toque. A seção mostra, nesta ordem: perfis prontos (Padrão, Online competitivo, Tranquilo, Clássico 0.9) e Canhoto (espelha o layout); analógico (Fixo, Flutuante, Dinâmico, com a explicação do escolhido; zona morta, inclinação total, zona do analógico, 8 direções); botões (área de toque, arrastar entre todos, toque alterna em L2, O, R1, R2, L1); vibração (desligada, leve, forte); meus perfis (salvar com nome, carregar, apagar). "Como funciona" abre o guia. As escolhas antigas continuam: Fixo/Flutuante também escreve `sticksFloat`, e a vibração também escreve `haptics`, para um APK anterior que leia o mesmo arquivo. Os desenhos são feitos aqui (polegar, anéis, setas): nenhuma imagem do jogo.
- **Medido/provado:** Mi A3: a seção inteira aparece nos Controles do BT3, os perfis gravam o arquivo certo, o guia abre com os 6 desenhos (título corrigido para a cor do tema). Comportamento no jogo: ver a entrada acima.
- **O que pode quebrar:** o perfil pronto marcado é o que tem o mesmo ajuste; mexer num controle deixa "Ajuste seu". Carregar um perfil troca o layout inteiro (lugares, macros, visual) e mantém a lista de perfis e o modo (desligado/automático/sempre).
- **Como testar de novo:** Controles > Toque Pro: cada perfil pronto, Canhoto duas vezes (volta ao original), os 3 modos do analógico, os controles deslizantes, salvar e carregar um perfil, o guia nos temas claro e escuro e nos 3 idiomas; a mesma seção na aba do BT2.

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
