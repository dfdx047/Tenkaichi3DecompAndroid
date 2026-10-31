<p align="center">
  <img src="android/art/banner.png" alt="Dragon Rage" width="100%">
</p>

<p align="center">
  <b>Dragon Ball Z: Budokai Tenkaichi 3, rodando nativamente no Android.</b><br>
  Um port para Android do <a href="https://github.com/z3xox/Tenkaichi3Decomp">Tenkaichi3Decomp</a>, o port nativo para PC feito sobre a decompilação do BT3.
</p>

<p align="center">
  <a href="README.md">English</a> · <b>Português</b> · <a href="README.es.md">Español</a>
</p>

<p align="center">
  <a href="../../releases"><img alt="Release" src="https://img.shields.io/github/v/release/dfdx047/Tenkaichi3DecompAndroid?label=release&color=ff8a1f"></a>
  <a href="../../actions/workflows/android.yml"><img alt="Android build" src="https://github.com/dfdx047/Tenkaichi3DecompAndroid/actions/workflows/android.yml/badge.svg"></a>
  <img alt="Platform" src="https://img.shields.io/badge/platform-Android%2010%2B%20·%20arm64-1e4fa8">
  <a href="https://discord.gg/6XXxT547Qx"><img alt="Discord" src="https://img.shields.io/badge/Discord-join-5865F2?logo=discord&logoColor=white"></a>
  <a href="https://www.youtube.com/@DFDX47"><img alt="YouTube" src="https://img.shields.io/badge/YouTube-@DFDX47-ff0000?logo=youtube&logoColor=white"></a>
  <a href="https://ko-fi.com/dfdx047"><img alt="Ko-fi" src="https://img.shields.io/badge/Ko--fi-support-29abe0?logo=kofi&logoColor=white"></a>
</p>

---

> [!IMPORTANT]
> **Status (0.9.0): o jogo inteiro roda nativamente no Android arm64**: menus, modo história e lutas completas nos
> 30 fps do jogo, em Snapdragon e em GPUs Mali, Xclipse e PowerVR. O launcher agora **detecta sua GPU** e sugere
> configurações pensadas para manter as lutas em 30 fps estáveis; já estão incluídos o modo online (PC e Android nas
> mesmas salas), pacotes de texturas HD, um novo menu dentro do jogo feito para toque, o novo controle na tela **Dragão**
> e **download de drivers** para Adreno. A 0.7.0 traz a versão 0.1.37 do port de PC (o online volta a funcionar), um
> **aviso da situação do online** no launcher, a página **Dragon Net Battle** feita para toque, pacotes de textura em
> ASTC (experimental). A 0.9.0 traz a 0.1.38.7 do port de PC (mais rápida em 2x ou mais, sobretudo em Mali), o
> **Budokai Tenkaichi 4** desenhado pelo renderizador do ARMSX2 por padrão e o online que espera quando os servidores
> gratuitos chegam ao limite do dia. Em inglês, português e espanhol. Veja o [Roadmap](#roadmap).

Este repositório **não contém nenhum dado do jogo**. Você precisa da sua própria cópia da **versão americana (SLUS-21678)**
em `.iso`. O app lê o arquivo no seu aparelho; nada é baixado nem enviado.

## O que é

O port para PC não é um emulador: é o próprio código do jogo, decompilado e compilado para PC, com uma nova camada de
renderização, som e controles por baixo. O Dragon Rage leva isso para o Android em duas partes:

| Parte | Onde | Estado |
|---|---|---|
| **App launcher** (Kotlin, Jetpack Compose, Material 3; inglês, português e espanhol) | [`android/`](android) | ✅ Pronto |
| **Motor do jogo** (o código C do port, compilado para arm64 como `libbt3.so`) | `src/`, `include/`, `port/` | ✅ Roda em Snapdragon, Mali, Xclipse e PowerVR; ajustado por nível de GPU |

## O app

<table>
<tr><td><b>Início</b></td><td>Escolha sua ISO. O app confere se é a versão americana (as mesmas verificações SHA-1 da instalação do PC; uma imagem traduzida ou modificada também instala, com um aviso) e copia os dados do jogo na estrutura que o motor espera. O botão <b>Jogar</b>, um <b>atalho de Jogar</b> para a tela inicial, <b>como funciona o online</b> (e o que fazer quando não conecta) e links para YouTube, Discord, Ko-fi e GitHub.</td></tr>
<tr><td><b>Texturas</b></td><td>Importe um pacote de texturas em <code>.zip</code>, baixe um direto pelo app (de uma lista mantida em <a href="android/texture-packs.json"><code>android/texture-packs.json</code></a>, ou de qualquer link direto para um <code>.zip</code>), ou copie a pasta dele para <code>textures/</code>. Pacotes feitos para o PCSX2 (<code>.dds</code> / <code>.png</code>) funcionam do jeito que estão: os arquivos <code>.dds</code> são descompactados para as GPUs de celular, que não leem DXT.</td></tr>
<tr><td><b>Mods</b></td><td>Mods de substituição de arquivos em <code>.zip</code> (com ordem de prioridade), estágios extras (<code>.unk</code>) e músicas extras (<code>.adx</code>, ou <code>.mp3</code>: convertidas pelo jogo), com o nome exibido nos menus do jogo editável.</td></tr>
<tr><td><b>Controles</b></td><td>O controle na tela: o visual <b>Dragão</b> (novo padrão: botões redondos escuros com ícones dourados, letras nas cores do Xbox, uma Esfera do Dragão de três estrelas no analógico), o visual <b>Vidro</b> da 0.5 ou o controle <b>clássico</b> de console; um layout feito para este jogo que alterna <b>sozinho</b> entre menus e lutas (modo história incluído), letras de PlayStation, Xbox ou Nintendo, editor de layout, macros, opacidade, tamanho, vibração, analógico flutuante, um botão só nos confrontos.</td></tr>
<tr><td><b>Cheats</b></td><td><i>Desbloquear tudo</i>: todos os personagens, estágios, músicas e itens, e Zeni no máximo (a função de debug que os desenvolvedores deixaram no jogo).</td></tr>
<tr><td><b>Drivers</b></td><td>Drivers Vulkan personalizados para GPUs Adreno (Snapdragon) via AdrenoTools: as versões mais recentes de <b>Mr. Purple</b>, <b>StevenMXZ</b> e <b>K11MCH1</b> baixadas e instaladas com um toque, um <code>.zip</code> dos seus arquivos, escolher, remover.</td></tr>
<tr><td><b>Depuração</b></td><td>Seu aparelho e sua GPU com o <b>nível</b> dela, as configurações recomendadas para ele, o <b>log de desempenho</b> da última sessão (velocidade nas lutas e nos menus, quadros por segundo; ligado por padrão, sem custo perceptível) e <i>Exportar log</i> para relatar bugs.</td></tr>
<tr><td><b>Configurações</b></td><td>Renderizador (<b>Vulkan</b>, ou <b>OpenGL ES</b> para drivers em que o Vulkan falha), resolução interna de 1x a 8x em passos finos, filtro de tela (bilinear, nítido, FXAA, AMD FSR 1, Snapdragon GSR), formato da imagem, <b>orientação da tela</b> (paisagem, retrato, automática), <b>profundidade precisa</b>, <b>pular quadros</b> em vez de ficar lento, movimento suave (60 / 120), modo de apresentação, os efeitos do PS2, volumes, idioma, tema, Material You.</td></tr>
<tr><td><b>No jogo</b></td><td>Um novo menu feito para dedos e controles, no visual do launcher e no seu idioma: Vídeo, Desempenho, Efeitos, Áudio, Interface (fontes, HUD, barras laterais), Controles, <b>Online</b> (como funciona, o que fazer quando falha), Trapaças, <b>Sair para o app</b>. Botão Voltar, START + SELECT ou o botão MENU na tela.</td></tr>
</table>

<p align="center"><img src="docs/images/v0.6-controls-pt.png" alt="Os controles na tela e o que cada um faz" width="90%"></p>

### Sua GPU, a 30 fps

Na primeira vez que abre com uma GPU conhecida, o app diz qual encontrou e sugere as configurações daquele nível
(você vê a lista e escolhe sim ou agora não; tudo continua podendo ser mudado):

| Nível | GPUs (exemplos) | Configurações sugeridas |
|---|---|---|
| Entrada | Mali-G31/G51/G52/G57, Mali-T, Adreno 3xx–5xx e 610/612, PowerVR GE/GM | 1x, movimento suave desligado, profundidade precisa desligada, pular quadros, os dois desfoques desligados |
| Fraca | Mali-G68/G71/G72, Adreno 613–620 e 702–712, Xclipse 530/540, PowerVR BXM | 1x, movimento suave desligado, profundidade precisa desligada, pular quadros, desfoque de movimento desligado |
| Intermediária | Mali-G76/G77/G78/G610/G615, Adreno 630–644 e série 720, Maleoon | 1.5x, movimento suave desligado, profundidade precisa desligada |
| Forte | Mali-G710/G715/G720/G725, Adreno 650–699 e série 730, Xclipse 920 | 2x |
| Topo de linha | Adreno 740 e mais novas (8 Gen 2, 8 Gen 3, 8 Elite), Immortalis, Mali-G925, Xclipse 940+ | 2x, movimento suave 60 |

### Instalação

Baixe o `DragonRage-<version>.apk` da [versão mais recente](../../releases/latest) e instale. Uma versão nova
instala por cima da antiga e mantém seus dados do jogo, configurações e saves. O app verifica o GitHub quando abre
e oferece as versões novas com o changelog.

Para relatar um problema: *Depuração → Exportar log*, depois envie o arquivo `DragonRage-log-….txt` (no [Discord](https://discord.gg/6XXxT547Qx), por exemplo).

Para compilar você mesmo:

```sh
sh port/tools/android_toolchain.sh   # LLVM 21, the NDK's sysroot, SDL3 (once)
python3 port/tools/android.py        # the engine -> android/app/src/main/jniLibs (and its fonts and pictures -> assets)
cd android && ./gradlew assembleRelease   # needs JDK 17+ and the Android SDK (compileSdk 35)
```

### Onde ficam os arquivos

Tudo fica em `Android/data/com.dfdx047.dragonrage/files/`, na **mesma estrutura que o port para PC usa ao lado do
executável**:

```
gamedata/            the disc's files (disc/SLUS_216.78, disc/BIN/, disc/DATA/ with the AFS archives, .installed)
gamedata/mods/       replaced files (rebuilt by the app from modlib/)
textures/            active texture packs (any depth)      textures_off/   packs switched off
stages/  songs/      extra stages and songs (maps.txt / songs.txt hold the menu names)
saves/               memory card
fonts/  tbo/ui/      the menu fonts and the online screens' pictures (copied from the app)
bt3_settings.txt     engine settings (name=number lines)
bt3_log.txt          the game's log of its last run
modlib/              the app's mod library
```

Chaves do Android em `bt3_settings.txt`: `dr_unlock_all`, `dr_renderer` (0 automático, 1 Vulkan, 2 OpenGL ES), `dr_orient`
(0 paisagem, 1 retrato, 2 automático), `dr_lang` (o idioma do menu dentro do jogo), `dr_gpu_tier` (o nível cujas
configurações foram sugeridas), `perf_log`, `frag_depth`. O controle na tela fica em `touch_controls.json`. Um arquivo
opcional `match_url.txt` (uma linha, `https://...`) aponta outro serviço de matchmaking.

## Roadmap

### Pronto
- [x] Launcher: instalação a partir da ISO com checksums, pacotes de texturas, mods, estágios e músicas extras, cheats,
      todas as configurações do motor, inglês / português / espanhol
- [x] Motor para arm64, carregado em qualquer ponto dos 4 GB baixos; o jogo inteiro em velocidade total em Snapdragon e
      Mali recentes
- [x] Toque: um controle feito para este jogo com layout automático de menu / luta, editor de layout, macros, três
      visuais (0.3–0.6)
- [x] Mali e GPUs fracas: Vulkan sem os recursos opcionais que faltam na Mali, shaders sem `discard`, render targets
      pequenos; renderizador OpenGL ES 3.2 com um anel de uniform buffers (0.5–0.6)
- [x] Modo online com códigos de sala e rollback, PC e Android juntos (0.4)
- [x] Pacotes de texturas HD (`.dds` descompactados na CPU), um limite de memória para o cache (0.5)
- [x] **0.6.0**: as novidades do port para PC (telas do Tenkaichi Budokai Online, fontes de menu e de batalha, painéis
      de lutadores nas laterais, profundidade precisa, sombras planas com stencil, músicas em MP3, Lutar de Novo no modo
      história), um novo menu dentro do jogo para toque, níveis de GPU com configurações recomendadas, download de
      drivers, página de Depuração, orientação, espanhol, o controle Dragão, o layout automático do modo história
      corrigido
- [x] **0.7.0**: a 0.1.36 do port de PC (online aceito de novo), aviso da situação do online no launcher, página
      Dragon Net Battle para toque, pacotes de textura guardados em ASTC, verificação de ISOs modificadas ou incompletas e um aviso explicando por que o
      jogo fechou
- [x] **0.7.1**: framebuffer fetch desligado de novo (blocos pretos e GPU travada em Mali-G52 com OpenGL ES), aviso
      ao escolher OpenGL ES numa Mali
- [x] **0.7.2**: pacotes de textura em ASTC desligados por padrão (experimental): as lutas voltam ao código de motor
      da 0.6.0
- [x] **0.7.3**: a 0.1.37 do port de PC (online aceito de novo, mensagens no início da partida, blending da Solar Flare,
      catálogos de texto)
- [x] **0.8.0**: **Dragon Net**: online com o port de Nintendo Switch (DBT-NX) num serviço próprio (salas, partida
      rápida, Classificatória com Elo, chat geral, jogadores online), sem depender das versões do port de PC; nova tela
      Dragon Net Battle; online contra PC mantido como opção (`port/dragonnet`, `docs/DRAGON_NET.md`)
- [x] **0.8.1**: Dragon Net em três servidores (um reserva e a Classificatória à parte), menos pedidos a eles
- [x] **0.9.0**: a 0.1.38.7 do port de PC; Budokai Tenkaichi 4 com o renderizador do ARMSX2 por padrão (shaders
      compilados de antemão), lutas a 60 fps (experimental, volta para 30 onde o aparelho não aguenta); o online pausa
      no HTTP 429 e avisa que o servidor está ocupado; o controle clássico cabe em telas quadradas; medida num RG Cube
      e num Retroid Pocket 5 (`docs/release-0.9.0.md`)

### Próximos passos
- [ ] Mais GPUs medidas para os níveis (mande seu log!)
- [ ] Imagem: filtragem anisotrópica, SMAA, mais pós-processamento
- [ ] Pausar o jogo enquanto o app estiver em segundo plano

## Estrutura do repositório

| Caminho | Conteúdo |
|---|---|
| `android/` | **Dragon Rage**, o app Android (projeto Gradle) |
| `src/`, `include/` | O código do jogo, vindo da decompilação; as mudanças do PC ficam dentro de `#ifdef PORT` |
| `port/` | O port para PC: camada de plataforma, renderizador, som, controles, ferramentas de build |
| `docs/` | Notas sobre o jogo e sobre o port para PC |

## Equipe e créditos

- **[Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp)**, de **z3xox** e colaboradores: o port nativo de PC
  em que isto se baseia, e a decompilação por baixo dele.
- **Dragon Rage** (app Android e port): [dfdx047](https://github.com/dfdx047) ·
  [YouTube](https://www.youtube.com/@DFDX47) · [Discord](https://discord.gg/6XXxT547Qx) · [Ko-fi](https://ko-fi.com/dfdx047)
- **Regulus CiberSec**: BT2 Recomp no Android, Dragon Net Online, espectadores e lobbies.
- **Androidxxt01**, **jllacchua**, **victorgonl**: contribuições ao port de PC e ao app.
- **tehmufti e os colaboradores do Tag Team Mod**: o Tag Team Mod.
- **Team BT4**: o mod Budokai Tenkaichi 4.
- Arte: **MarshallPunk** (o locutor), **pluantoine** (os botões, CC BY 4.0).
- Drivers de GPU de [Mr. Purple](https://github.com/MrPurple666/purple-turnip), [StevenMXZ](https://github.com/StevenMXZ/Adreno-Tools-Drivers)
  e [K11MCH1](https://github.com/K11MCH1/AdrenoToolsDrivers), carregados com [libadrenotools](https://github.com/bylaws/libadrenotools) (BSD-2-Clause).
- [AMD FidelityFX Super Resolution 1](https://github.com/GPUOpen-Effects/FidelityFX-FSR) (MIT),
  [Snapdragon Game Super Resolution](https://github.com/SnapdragonStudios/snapdragon-gsr) (BSD-3-Clause), [SDL3](https://libsdl.org) (zlib), [Apache Commons Compress](https://commons.apache.org/proper/commons-compress/) (Apache-2.0),
  [Dear ImGui](https://github.com/ocornut/imgui) (MIT), as fontes em `port/third_party/fonts` (cada uma com sua licença).
- Feito com a ajuda de IA.

## Licença

O código próprio do port para PC (`port/`, exceto `port/third_party/`) está sob a [GNU Affero General Public License,
versão 3](LICENSE) a partir do commit `06e4671` do port para PC (2026-10-14); o código anterior a ele continua disponível
sob a [licença MIT](LICENSE-MIT). O Dragon Rage é construído sobre esse código, então o app como um todo é distribuído
sob a AGPL-3.0, com o código-fonte completo aqui; os arquivos próprios do app Android (`android/`) também são oferecidos
sob MIT. As bibliotecas em `port/third_party/` e os shaders de FSR / GSR mantêm suas próprias licenças (nos arquivos ao
lado deles). O código do jogo em `src/` e `include/` vem da decompilação e pertence aos detentores dos direitos do jogo.

## Aviso legal

Este projeto não tem vínculo com os desenvolvedores, publicadoras ou detentores dos direitos do jogo, nem é endossado
por eles. Ele existe para preservação, estudo e interoperabilidade, e não distribui nenhum asset do jogo. Dragon Ball e
todos os nomes relacionados pertencem aos seus donos. Você precisa da sua própria cópia do jogo, obtida legalmente.
