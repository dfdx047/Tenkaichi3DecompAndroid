<p align="center">
  <img src="android/art/banner.png" alt="Dragon Rage" width="100%">
</p>

<p align="center">
  <b>Dragon Ball Z: Budokai Tenkaichi 3, corriendo de forma nativa en Android.</b><br>
  Un port para Android de <a href="https://github.com/z3xox/Tenkaichi3Decomp">Tenkaichi3Decomp</a>, el port nativo para PC construido sobre la decompilación de BT3.
</p>

<p align="center">
  <a href="README.md">English</a> · <a href="README.pt.md">Português</a> · <b>Español</b>
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
> **Estado (0.9.0): el juego completo corre de forma nativa en Android arm64**: menús, modo historia y peleas completas a
> los 30 fps del juego, en Snapdragon y en GPUs Mali, Xclipse y PowerVR. El launcher ahora **detecta tu GPU** y te ofrece
> ajustes pensados para mantener las peleas a 30 fps estables; ya están incluidos el juego online (PC y Android en las
> mismas salas), los packs de texturas HD, un nuevo menú dentro del juego hecho para pantallas táctiles, el nuevo control
> en pantalla **Dragón** y la **descarga de drivers** para Adreno. La 0.7.0 trae la versión 0.1.37 del port de PC (el
> online vuelve a funcionar), un **aviso del estado del online** en el launcher, la página **Dragon Net Battle** hecha
> para pantallas táctiles, packs de texturas en ASTC (experimental). La 0.9.0 trae la 0.1.38.7 del port de PC (más
> rápida a 2x o más, sobre todo en Mali), **Budokai Tenkaichi 4** dibujado por el renderizador de ARMSX2 por defecto
> y el online que espera cuando los servidores gratuitos llegan a su límite diario. En inglés, portugués y español.
> Mira la [Hoja de ruta](#hoja-de-ruta).

Este repositorio **no contiene ningún dato del juego**. Necesitas tu propia copia de la **versión de EE. UU. (SLUS-21678)**
en `.iso`. La app la lee en tu dispositivo; no se descarga ni se sube nada.

## Qué es

El port para PC no es un emulador: es el propio código del juego, decompilado y compilado para PC, con una nueva capa de
renderizado, sonido y controles por debajo. Dragon Rage lleva eso a Android en dos partes:

| Parte | Dónde | Estado |
|---|---|---|
| **App launcher** (Kotlin, Jetpack Compose, Material 3; inglés, portugués y español) | [`android/`](android) | ✅ Listo |
| **Motor del juego** (el código C del port, compilado para arm64 como `libbt3.so`) | `src/`, `include/`, `port/` | ✅ Corre en Snapdragon, Mali, Xclipse y PowerVR; ajustado por nivel de GPU |

## La app

<table>
<tr><td><b>Inicio</b></td><td>Elige tu ISO. La app comprueba que sea la versión de EE. UU. (las mismas verificaciones SHA-1 que la instalación de PC; una imagen traducida o modificada también se instala, con una advertencia) y copia los datos del juego con la estructura que espera el motor. El botón <b>Jugar</b>, un <b>acceso directo para Jugar</b> en la pantalla de inicio, <b>cómo funciona el online</b> (y qué hacer cuando no conecta) y enlaces a YouTube, Discord, Ko-fi y GitHub.</td></tr>
<tr><td><b>Texturas</b></td><td>Importa un pack de texturas en <code>.zip</code>, descarga uno desde la propia app (de una lista mantenida en <a href="android/texture-packs.json"><code>android/texture-packs.json</code></a>, o de cualquier enlace directo a un <code>.zip</code>), o copia su carpeta en <code>textures/</code>. Los packs hechos para PCSX2 (<code>.dds</code> / <code>.png</code>) funcionan tal cual: los archivos <code>.dds</code> se descomprimen para las GPUs de los teléfonos, que no pueden leer DXT.</td></tr>
<tr><td><b>Mods</b></td><td>Mods de reemplazo de archivos en <code>.zip</code> (con orden de prioridad), escenarios extra (<code>.unk</code>) y canciones extra (<code>.adx</code>, o <code>.mp3</code>: las convierte el juego), con el nombre que aparece en los menús del juego editable.</td></tr>
<tr><td><b>Controles</b></td><td>El control en pantalla: el estilo <b>Dragón</b> (nuevo por defecto: botones redondos oscuros con iconos dorados, letras con los colores de Xbox, una Esfera del Dragón de tres estrellas como stick), el estilo <b>Cristal</b> de la 0.5 o el mando <b>clásico</b> de consola; una distribución hecha para este juego que cambia <b>sola</b> entre menús y peleas (modo historia incluido), letras de PlayStation, Xbox o Nintendo, editor de distribución, macros, opacidad, tamaño, vibración, stick flotante, un solo botón en los choques.</td></tr>
<tr><td><b>Trucos</b></td><td><i>Desbloquear todo</i>: todos los personajes, escenarios, canciones y objetos, y Zeni al máximo (la función de depuración que los desarrolladores dejaron en el juego).</td></tr>
<tr><td><b>Drivers</b></td><td>Drivers Vulkan personalizados para GPUs Adreno (Snapdragon) mediante AdrenoTools: las últimas versiones de <b>Mr. Purple</b>, <b>StevenMXZ</b> y <b>K11MCH1</b> descargadas e instaladas con un toque, un <code>.zip</code> desde tus archivos, elegir, eliminar.</td></tr>
<tr><td><b>Depuración</b></td><td>Tu dispositivo y tu GPU con su <b>nivel</b>, los ajustes recomendados para ella, el <b>log de rendimiento</b> de la última sesión (velocidad en peleas y menús, cuadros por segundo; activado por defecto, sin un costo que se note) y <i>Exportar log</i> para reportar bugs.</td></tr>
<tr><td><b>Ajustes</b></td><td>Renderizador (<b>Vulkan</b>, u <b>OpenGL ES</b> para drivers en los que Vulkan falla), resolución interna de 1x a 8x en pasos finos, filtro de pantalla (bilineal, nítido, FXAA, AMD FSR 1, Snapdragon GSR), formato de imagen, <b>orientación de pantalla</b> (horizontal, vertical, automática), <b>profundidad precisa</b>, <b>saltar cuadros</b> en vez de ir lento, movimiento suave (60 / 120), modo de presentación, los efectos de PS2, volúmenes, idioma, tema, Material You.</td></tr>
<tr><td><b>En el juego</b></td><td>Un nuevo menú hecho para dedos y mandos, con el estilo del launcher y en tu idioma: Video, Rendimiento, Efectos, Audio, Interfaz (fuentes, HUD, barras laterales), Controles, <b>Online</b> (cómo funciona, qué hacer cuando falla), Trucos, <b>Salir a la app</b>. Botón Atrás, START + SELECT o el botón MENU en pantalla.</td></tr>
</table>

<p align="center"><img src="docs/images/v0.6-controls-es.png" alt="Los controles en pantalla y lo que hace cada uno" width="90%"></p>

### Tu GPU, a 30 fps

La primera vez que se abre con una GPU conocida, la app te dice cuál encontró y te ofrece los ajustes de ese nivel
(ves la lista y dices que sí o que ahora no; todo se puede seguir cambiando):

| Nivel | GPUs (ejemplos) | Ajustes ofrecidos |
|---|---|---|
| Básica | Mali-G31/G51/G52/G57, Mali-T, Adreno 3xx–5xx y 610/612, PowerVR GE/GM | 1x, movimiento suave desactivado, profundidad precisa desactivada, saltar cuadros, ambos desenfoques desactivados |
| Débil | Mali-G68/G71/G72, Adreno 613–620 y 702–712, Xclipse 530/540, PowerVR BXM | 1x, movimiento suave desactivado, profundidad precisa desactivada, saltar cuadros, desenfoque de movimiento desactivado |
| Intermedia | Mali-G76/G77/G78/G610/G615, Adreno 630–644 y serie 720, Maleoon | 1.5x, movimiento suave desactivado, profundidad precisa desactivada |
| Potente | Mali-G710/G715/G720/G725, Adreno 650–699 y serie 730, Xclipse 920 | 2x |
| Gama alta | Adreno 740 y más nuevas (8 Gen 2, 8 Gen 3, 8 Elite), Immortalis, Mali-G925, Xclipse 940+ | 2x, movimiento suave 60 |

### Instalación

Descarga `DragonRage-<version>.apk` desde la [última versión](../../releases/latest) e instálalo. Una versión nueva
se instala encima de la anterior y conserva tus datos del juego, ajustes y partidas guardadas. La app revisa GitHub
al abrirse y te ofrece las versiones nuevas con su changelog.

Para reportar un problema: *Depuración → Exportar log*, y luego envía el archivo `DragonRage-log-….txt` (en [Discord](https://discord.gg/6XXxT547Qx), por ejemplo).

Para compilarlo tú mismo:

```sh
sh port/tools/android_toolchain.sh   # LLVM 21, the NDK's sysroot, SDL3 (once)
python3 port/tools/android.py        # the engine -> android/app/src/main/jniLibs (and its fonts and pictures -> assets)
cd android && ./gradlew assembleRelease   # needs JDK 17+ and the Android SDK (compileSdk 35)
```

### Dónde van los archivos

Todo vive en `Android/data/com.dfdx047.dragonrage/files/`, con **la misma estructura que usa el port para PC junto a
su ejecutable**:

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

Claves de Android en `bt3_settings.txt`: `dr_unlock_all`, `dr_renderer` (0 automático, 1 Vulkan, 2 OpenGL ES), `dr_orient`
(0 horizontal, 1 vertical, 2 automática), `dr_lang` (el idioma del menú dentro del juego), `dr_gpu_tier` (el nivel cuyos
ajustes se ofrecieron), `perf_log`, `frag_depth`. El control en pantalla está en `touch_controls.json`. Un archivo
opcional `match_url.txt` (una línea, `https://...`) indica otro servicio de matchmaking.

## Hoja de ruta

### Listo
- [x] Launcher: instalación desde la ISO con checksums, packs de texturas, mods, escenarios y canciones extra, trucos,
      todos los ajustes del motor, inglés / portugués / español
- [x] Motor para arm64, cargado en cualquier parte de los 4 GB bajos; el juego completo a toda velocidad en Snapdragon y
      Mali recientes
- [x] Táctil: un control hecho para este juego con distribución automática de menú / pelea, editor de distribución,
      macros, tres estilos (0.3–0.6)
- [x] Mali y GPUs débiles: Vulkan sin las funciones opcionales que le faltan a Mali, shaders sin `discard`, render
      targets pequeños; renderizador OpenGL ES 3.2 con un anillo de uniform buffers (0.5–0.6)
- [x] Juego online con códigos de sala y rollback, PC y Android juntos (0.4)
- [x] Packs de texturas HD (`.dds` descomprimidos en la CPU), un límite de memoria para la caché (0.5)
- [x] **0.6.0**: lo último del port para PC (pantallas de Tenkaichi Budokai Online, fuentes de menú y de combate, paneles
      de luchadores en los laterales, profundidad precisa, sombras planas con stencil, canciones en MP3, Pelear de nuevo
      en el modo historia), un nuevo menú dentro del juego para pantallas táctiles, niveles de GPU con ajustes
      recomendados, descarga de drivers, página de Depuración, orientación, español, el control Dragón, la distribución
      automática del modo historia corregida
- [x] **0.7.0**: la 0.1.36 del port de PC (online aceptado de nuevo), aviso del estado del online en el launcher,
      página Dragon Net Battle táctil, packs de texturas guardados en ASTC, comprobación de ISOs modificadas o incompletas y un aviso que explica por qué se
      cerró el juego
- [x] **0.7.1**: framebuffer fetch desactivado de nuevo (bloques negros y GPU congelada en Mali-G52 con OpenGL ES),
      aviso al elegir OpenGL ES en una Mali
- [x] **0.7.2**: packs de texturas en ASTC desactivados por defecto (experimental): las peleas vuelven al código de
      motor de la 0.6.0
- [x] **0.7.3**: la 0.1.37 del port de PC (online aceptado de nuevo, mensajes al empezar la partida, blending de la
      Solar Flare, catálogos de texto)
- [x] **0.8.0**: **Dragon Net**: online con el port de Nintendo Switch (DBT-NX) en un servicio propio (salas, partida
      rápida, Clasificatoria con Elo, chat general, jugadores en línea), sin depender de las versiones del port de PC;
      nueva pantalla Dragon Net Battle; online contra PC como opción (`port/dragonnet`, `docs/DRAGON_NET.md`)
- [x] **0.8.1**: Dragon Net en tres servidores (uno de reserva y la Clasificatoria aparte), menos peticiones a ellos
- [x] **0.9.0**: la 0.1.38.7 del port de PC; Budokai Tenkaichi 4 con el renderizador de ARMSX2 por defecto (shaders
      compilados de antemano), peleas a 60 fps (experimental, vuelve a 30 donde el dispositivo no las aguanta); el
      online se pausa con el HTTP 429 y avisa que el servidor está ocupado; el control clásico cabe en pantallas
      cuadradas; medida en un RG Cube y un Retroid Pocket 5 (`docs/release-0.9.0.md`)

### Lo que viene
- [ ] Más GPUs medidas para los niveles (¡envía tu log!)
- [ ] Imagen: filtrado anisotrópico, SMAA, más posprocesado
- [ ] Pausar el juego mientras la app está en segundo plano

## Estructura del repositorio

| Ruta | Contenido |
|---|---|
| `android/` | **Dragon Rage**, la app de Android (proyecto Gradle) |
| `src/`, `include/` | El código del juego, sacado de la decompilación; los cambios de PC están dentro de `#ifdef PORT` |
| `port/` | El port para PC: capa de plataforma, renderizador, sonido, controles, herramientas de compilación |
| `docs/` | Notas sobre el juego y sobre el port para PC |

## Equipo y créditos

- **[Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp)**, de **z3xox** y colaboradores: el port nativo de PC
  en el que se basa esto, y la decompilación debajo de él.
- **Dragon Rage** (app Android y port): [dfdx047](https://github.com/dfdx047) ·
  [YouTube](https://www.youtube.com/@DFDX47) · [Discord](https://discord.gg/6XXxT547Qx) · [Ko-fi](https://ko-fi.com/dfdx047)
- **Regulus CiberSec**: BT2 Recomp en Android, Dragon Net Online, espectadores y lobbies.
- **Androidxxt01**, **jllacchua**, **victorgonl**: contribuciones al port de PC y a la app.
- **tehmufti y los colaboradores de Tag Team Mod**: el Tag Team Mod.
- **Team BT4**: el mod Budokai Tenkaichi 4.
- Arte: **MarshallPunk** (el locutor), **pluantoine** (los botones, CC BY 4.0).
- Drivers de GPU de [Mr. Purple](https://github.com/MrPurple666/purple-turnip), [StevenMXZ](https://github.com/StevenMXZ/Adreno-Tools-Drivers)
  y [K11MCH1](https://github.com/K11MCH1/AdrenoToolsDrivers), cargados con [libadrenotools](https://github.com/bylaws/libadrenotools) (BSD-2-Clause).
- [AMD FidelityFX Super Resolution 1](https://github.com/GPUOpen-Effects/FidelityFX-FSR) (MIT),
  [Snapdragon Game Super Resolution](https://github.com/SnapdragonStudios/snapdragon-gsr) (BSD-3-Clause), [SDL3](https://libsdl.org) (zlib), [Apache Commons Compress](https://commons.apache.org/proper/commons-compress/) (Apache-2.0),
  [Dear ImGui](https://github.com/ocornut/imgui) (MIT), las fuentes en `port/third_party/fonts` (cada una con su licencia).
- Hecho con ayuda de IA.

## Licencia

El código propio del port para PC (`port/`, excepto `port/third_party/`) está bajo la [GNU Affero General Public License,
versión 3](LICENSE) a partir del commit `06e4671` del port para PC (2026-10-14); el código anterior sigue disponible
bajo la [licencia MIT](LICENSE-MIT). Dragon Rage se construye sobre ese código, así que la app en su conjunto se
distribuye bajo la AGPL-3.0, con su código fuente completo aquí; los archivos propios de la app de Android (`android/`)
también se ofrecen bajo MIT. Las bibliotecas en `port/third_party/` y los shaders de FSR / GSR mantienen sus propias
licencias (en los archivos que los acompañan). El código del juego en `src/` e `include/` viene de la decompilación y
pertenece a los titulares de los derechos del juego.

## Aviso legal

Este proyecto no está afiliado ni respaldado por los desarrolladores, distribuidores ni titulares de los derechos del
juego. Existe con fines de preservación, estudio e interoperabilidad, y no distribuye ningún recurso del juego. Dragon
Ball y todos los nombres relacionados pertenecen a sus dueños. Necesitas tu propia copia del juego, obtenida legalmente.
