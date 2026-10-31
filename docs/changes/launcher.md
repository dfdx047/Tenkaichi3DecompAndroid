# Launcher

Regras e formato: [README](README.md).


### 1.0.0 (este commit): controle volta a navegar no launcher
- **Onde:** `android/.../MainActivity.kt`, `dispatchKeyEvent`/`dispatchGenericMotionEvent` removidos
- **O que e por quê:** veio do BT2 (Regulus): o launcher engolia todo evento de controle, inclusive no RG Cube e no RP5, em que o direcional é o único jeito de navegar sem toque. Na 0.9.0 isso não existia, e o jogo roda em outra Activity, que recebe o controle dela.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** um botão de controle agir no launcher e no jogo ao mesmo tempo, o que não acontece porque são Activities diferentes.
- **Como testar de novo:** no launcher, navegar com direcional e confirmar com A; abrir o jogo e conferir que o controle só age no jogo.

### 1.0.0 (este commit): equipe na tela Sobre, licenças no app
- **Onde:** `ui/screens/SettingsScreen.kt` (`about_team`, botão Licenças); `port/tools/android.py` (gera `assets/licenses.txt`); `README.md`, `README.pt.md`, `README.es.md`, "Equipe e créditos"
- **O que e por quê:** os avisos de licença ficavam fora do APK (`android.py` excluía os arquivos) e o app não tinha tela para eles. sse2neon, pl_mpeg, Kanit, fontes, botões CC BY 4.0 e demais agora aparecem no app, juntados na compilação.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** nada de jogo; `licenses.txt` é gerado e fica no `.gitignore`. Se faltar um arquivo de licença, `android.py` falha na compilação.
- **Como testar de novo:** Settings > Sobre > Licenças: a lista abre e rola.

## Histórico: 0.9.0 para trás

Linhas: lado novo do diff naquele commit; o arquivo de hoje tem outra numeração. Fontes: mensagens de commit, diffs, changelogs de cada tag,
`docs/release-0.9.0.md`, `docs/device-lab-2026-10.md`. Caminhos curtos ficam em `android/app/src/main/java/com/dfdx047/dragonrage/`.
Mais novas primeiro.

### 0.9.0 2d6d0531: Modo Lite oferecido só para chip fraco
- **Onde:** `data/GpuProfiles.kt` `weakChip`, 82-90, e `profile`, 102; `ui/screens/SettingsScreen.kt`, 235-237; `ui/screens/GpuOfferDialog.kt`, 35; `2f3ff7b2`: `GpuOfferDialog.kt`, 45-46, a lista rola.
- **O que e por quê:** nível 0 da tabela (Mali-G52, G31, G51, T-série, Adreno 3xx a 512 e 600 a 612, PowerVR GE), mas não um G57 com mais de 2 núcleos nem um que não diga quantos tem. No RP5 a lista do diálogo cortava os últimos itens, inclusive a linha do Lite num chip fraco.
- **Medido/provado:** `gpu_name.txt` trocado: Cube e RP5 sem a linha; "Mali-G52 MC1" com a linha. `GpuProfiles` compilado à parte para a lista inteira. Ganho em [`renderer-gs.md`](renderer-gs.md).
- **O que pode quebrar:** quem já aceitou um tier não recebe a oferta de novo. Ligar o Lite põe brilho e reflexo em 0; desligar não os devolve.
- **Como testar de novo:** trocar `gpu_name.txt`, tirar `dr_gpu_tier` só no teste, abrir o launcher.

### 0.9.0 d6467b24: importar textura: 4 falhas independentes
- **Onde:** `data/FileUtils.kt` `unzip`, 72-118; `data/TexturePacks.kt`, 52-65 e 91-105; `ui/screens/TexturesScreen.kt`, 128-130; `port/src/gs/gs_texpack.c` `TexPack_Init`, 274-290; `android/app/build.gradle.kts`, 96; `5dab1213` (texto da ajuda).
- **O que e por quê:** "instala e nada muda": no Android o SDL resolve caminho relativo dentro da pasta interna do app, e o log dizia `texture pack: no folder textures` com o pacote no lugar. Zips do 7-Zip com Deflate64 ou BZip2 e gravados em fluxo eram recusados; agora é Apache Commons Compress. Pacote que não cabe é recusado antes de extrair. O seletor mostra qualquer arquivo, e `.rar`/`.7z` recebem a explicação.
- **Medido/provado:** 18.704 texturas: Cube 154 s, RP5 136 s. Antes: ENOSPC depois de 68 s no RP5. 11 variantes de zip passam no PC. Luta: 18.597 indexadas, cerca de 3.000 trocadas, 100% nos dois, trabalho 22,6 contra 22,0 ms no Cube e 16,3 contra 16,0 no RP5. Índice lido em 1,3 s.
- **O que pode quebrar:** pacote em RAR 5 continua precisando de conversão. Pastas alternativas com os mesmos nomes: vale o primeiro encontrado. O falso "pacote não foi usado" com pasta vazia, achado na seção 9 do relatório, foi corrigido em `22c21670` (`LastRun.kt`).
- **Como testar de novo:** pacote por `.zip` e pasta vazia, luta, sem aviso falso; `.7z` recebe a mensagem.

### 0.9.0 79ce6851: barras escondidas antes da superfície existir
- **Onde:** `engine/GameActivity.kt` `onCreate`, 79 e 90-109; `port/src/gs/gs_gpu.c` `vk_frame_end`, 2277-2282 (log).
- **O que e por quê:** a superfície era criada com a barra de navegação na tela (1920x972) e o sistema esticava para 1080: 11% mais alto, faixas pretas em 16:9. Refazer o swapchain foi tentado e revertido, porque a superfície continua dizendo 972.
- **Medido/provado:** RP5: 3 de 8 inícios em 1920x972 → 10 de 10 em 1920x1080. `dumpsys SurfaceFlinger` mostrava `tr=[1.00 ... 1.11]`. Pixels: 0 contra a final. A suspeita sobre a gravação que perdeu sincronia não se confirmou: o boot gasta os mesmos blanks.
- **O que pode quebrar:** aparelhos com recorte; gestos que devolvem as barras (`setWindowStyle(true)`).
- **Como testar de novo:** a linha `present:` do log traz a janela e o swapchain. RP5, 10 inícios.

### 0.9.0 84b31f2e: perfis medidos: G57 MC4 e Adreno 650
- **Onde:** `data/GpuProfiles.kt`, 11-13, `measured`, 52-57, e `profile`, 87-92.
- **O que e por quê:** G57 MC4 fica no tier 0, mas com 1.5x, e o tier 3 ganha movimento suave 60.
- **Medido/provado:** Cube VK 1.5x 100%, pior quadro 25 ms. Adreno 650 2x suave: 100%, 56 a 63 quadros/s, pior 30 ms; aplicado pelo diálogo e medido no RP5. A oferta do G57 no Cube não aparece medida.
- **O que pode quebrar:** **provável erro aberto:** a tabela procura `"mali-g57 mc4"`, mas o relatório da 0.9.0 (linhas 433 e 496) diz que o Cube informa só "Mali-G57". Pelo código, o aparelho em que a medida foi feita recebe 1x. Não rodei no aparelho.
- **Como testar de novo:** Cube com `dr_gpu_tier` tirado só no teste: o diálogo tem de oferecer 1.5x.

### 0.9.0 f3557974: trilho lateral rola; `dc9f17f3`: linha inteira escolhe o driver
- **Onde:** `ui/screens/DriversScreen.kt`; `MainActivity.kt`. Linhas: não registrado nesta entrada.
- **O que e por quê:** com as 9 páginas do BT3 em 1080p paisagem, "Ajustes" e "Jogo" ficavam fora da tela sem jeito de chegar. Nos drivers só o botão de rádio escolhia.
- **Medido/provado:** device lab, RP5.
- **O que pode quebrar:** nova página no trilho.
- **Como testar de novo:** RP5 paisagem, todas as páginas; tocar no nome do driver.

### 0.9.0 5707f85b: importação diz o que é o arquivo
- **Onde:** `data/GameDataInstaller.kt` `notIsoMessage`, cerca de 207-240.
- **O que e por quê:** jogadores mandavam `.7z`, `.zip`, `.rar` e `.chd`, ou uma página salva em vez do download. A mensagem agora vem dos primeiros bytes.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** nada de jogo; texto novo nas três línguas.
- **Como testar de novo:** escolher um `.zip` e um `.html` como ISO.

### 0.8.0 95ff74da: diário do app e três últimas execuções
- **Onde:** `data/ActivityLog.kt`, novo; `AppViewModel.kt` (42 trechos); `data/LogExport.kt`, 34-77; `android/app/src/main/cpp/loader.c` `redirect_output`, 97-103.
- **O que e por quê:** problemas vistos uma execução depois sumiam. `bt3_log.txt`, `.1` e `.2`; falha salva log em Downloads sozinha.
- **Medido/provado:** usado em `port/tools/logscan.py` (208 logs na 1.0.0).
- **O que pode quebrar:** tamanho do log exportado: 1 MB no device lab.
- **Como testar de novo:** forçar erro de importação; ler Downloads e o botão Enviar log.

### 0.7.3 2d8a5a72: log automático, causa real do último fechamento
- **Onde:** `ui/screens/TexFailDialog.kt`, novo; `data/LastRun.kt`, 16 linhas; `AppViewModel.kt`; `ui/screens/TexturesScreen.kt`.
- **O que e por quê:** o aviso mostrava o crash final, não a causa. Log vai para Downloads na hora.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** a ordem das checagens em `LastRun.kt`: "last" é o crash, depois das causas.
- **Como testar de novo:** `BT3_TEST_READ_FAIL` e ler o aviso seguinte.

### 0.7.0 1867006b: ISO que fecharia o jogo é recusada na importação
- **Onde:** `data/GameDataInstaller.kt` `compatible`, 113-133, e cópia curta, 171-173; `data/LastRun.kt`, novo; `ui/screens/LastRunDialog.kt`, novo.
- **O que e por quê:** celulares fechavam ao iniciar com imagem modificada cujo `DBZP.BIN` é menor que a lista de dados. Cópia curta (armazenamento cheio) também parava o jogo.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** `libbt3.so.dat` (cabeçalho `BT3D`, registros de 16 bytes) muda de formato; a 1.0.0 achou imagens com 3400 arquivos no AFS, que passam por aqui.
- **Como testar de novo:** importar ISO modificada e ISO cortada.

### 0.6.0 7b99f8ad: perfis de GPU, página de drivers e depuração
- **Onde:** `data/GpuProfiles.kt`, novo, 96 linhas; `data/DriverSources.kt`; `data/PerfReport.kt`; `ui/screens/DebugScreen.kt`, `DriversScreen.kt`, `GpuOfferDialog.kt`.
- **O que e por quê:** o app identifica a GPU, sugere ajustes para 30 fps estáveis e só aplica com o sim do jogador. Drivers Mr. Purple, StevenMXZ e K11MCH1 pelo GitHub.
- **Medido/provado:** tabela feita por famílias; só em `84b31f2e` há modelos medidos.
- **O que pode quebrar:** download depende da API do GitHub. Numa GPU que não é Adreno a página avisa que não se aplica.
- **Como testar de novo:** Depuração › oferecer de novo; driver novo no RP5.

### 0.2.0 87db39ee: drivers com adrenotools
- **Onde:** `android/app/src/main/cpp/loader.c`; `data/GpuDrivers.kt`; `ui/screens/SettingsScreen.kt`.
- **O que e por quê:** Turnip e Qualcomm novos, por `.zip`. Relatório de erro único em Downloads. Imagem não americana é recusada com o serial.
- **Medido/provado:** depois, device lab: Turnip axfl2-006 no RP5 VK 4x de 70,8 a 71,5% contra 70,8%; MaliSX2 só abre em GL no BT3, e melhora o BT4.
- **O que pode quebrar:** driver recusado pelo SDL_GPU: ver `23fa0efe`.
- **Como testar de novo:** Turnip no RP5, MaliSX2 no Cube, e voltar.

### 0.2.1 6e471376: crash ao abrir o app
- **Onde:** `AppViewModel.kt`, bloco `init`.
- **O que e por quê:** regressão da 0.2.0: `checkForUpdate` lia `_update` antes de ele existir, porque propriedades são criadas em ordem.
- **Medido/provado:** não registrado.
- **O que pode quebrar:** qualquer `init` antes do estado que ele usa em `AppViewModel`.
- **Como testar de novo:** instalação limpa e abrir.
