# Añadir canciones a la selección de música

El port permite **añadir pistas** a la lista de música (BGM) que aparece en el menú. Una canción es un
fichero de audio (mp3, wav, ogg, flac…): el port lo sirve como el ADX que el juego reproduce y le da una
entrada propia en la lista, con su nombre.

Todo se controla con un **manifiesto** (`<data>/songs/songs.txt`) que el port lee al arrancar. No hay que
tocar código ni pasar variables de entorno.

---

## Uso rápido (script)

Desde la raíz del repositorio:

```sh
# instalar una canción (el nombre es el que sale en el menú)
port/tools/add_song.py add "/ruta/Cancion.mp3" "Nombre de la Canción"

# listar las instaladas (con su offset de lista)
port/tools/add_song.py list

# quitar una
port/tools/add_song.py remove "Nombre de la Canción"

# regenerar solo la tira de nombres (si cambias la fuente/estilo)
port/tools/add_song.py rebuild
```

`add` **convierte el audio a ADX** con `ffmpeg` (24000 Hz, estéreo — como los temas del juego), lo copia a
`gamedata/songs/`, añade la línea al manifiesto y **regenera la tira de nombres** (`names.rgba`).

| opción | efecto |
|---|---|
| `--data <dir>` | carpeta de datos (por defecto `gamedata`, o `$BT3_DATA`) |
| `--font <ttf>` | fuente para la tira de nombres (por defecto `$BT3_STAGE_FONT`, o `port/tools/compacta.ttf`, o una del sistema) |
| `--ffmpeg <ruta>` | binario de ffmpeg (por defecto `ffmpeg`) |
| `--rate <hz>` | frecuencia del ADX (por defecto 24000) |
| `--no-strip` | solo edita el manifiesto; no toca la tira |

### La fuente de los nombres

Los nombres de las canciones son **imágenes pre-renderizadas** en el juego (igual que los escenarios), así
que la tira de nombres se **genera** desde un TTF. Si no hay Pillow o no hay fuente, el manifiesto funciona
igual (las canciones salen y suenan) pero el nombre se dibuja con la fuente plana del juego.

Coloca el TTF en `port/tools/compacta.ttf` (o pásalo con `--font`). El estilo (relleno crema→naranja, borde
marrón, sombra, alineado a la izquierda) está en las constantes de `port/tools/add_song.py`.

---

## Uso manual

1. Convierte el audio a ADX: `ffmpeg -i Cancion.mp3 -ar 24000 -ac 2 -c:a adpcm_adx mi_cancion.adx`.
2. Copia el `.adx` a `gamedata/songs/`.
3. Añade una línea a `gamedata/songs/songs.txt`:
   ```
   mi_cancion.adx|Nombre En El Menú
   ```
4. Regenera la tira: `port/tools/add_song.py rebuild`.

---

## Cómo funciona por dentro

### La lista de música

El menú guarda la música como **offsets** dentro de un rango de ficheros BGM:

```
BGM id = 0x10B16 + offset      ->      pzs3us2/<64974 + offset>.bin
```

- Offsets `0x00..0x13` (20) = los temas del disco (0x10B16..0x10B29).
- `0x14..0x17` existen en la lista cruda pero el menú los quita (hueco).
- `0x18` = **Random** (marcador), `0x19` = **Locked** (marcador).
- Offsets `0x1A` en adelante = **libres** (sus ficheros, `0x10B30+`, son stubs de 30 KB).

Las canciones añadidas toman `0x1A`, `0x1B`, … El port:

1. **Aliasa** el fichero `pzs3us2/<64974 + 0x1A + i>.bin` → `songs/<fichero>` (`plat_songs.c`; lo consulta
   `plat_file.c`, tanto la carga normal como el **streaming** del ADX).
2. **Amplía la lista**: copia `bgmIds` a un buffer, inserta los offsets nuevos **antes** de Random y sube
   `bgmCount` (en `menu_c_e.c`, solo `#ifdef PORT`). Marca sus bits en `gSaveData->bgmBits` (u32 → caben
   offsets `0x1A..0x1F`).
3. **Dibuja el nombre**: la franja de música es fija; el port la dibuja como imagen (`gamedata/songs/names.rgba`)
   por encima del frame (overlay, back end **Vulkan/SDL_GPU**). Sin overlay (p. ej. **OpenGL**), el nombre se
   imprime con la **fuente del juego**.

### El nombre (overlay)

`names.rgba`: cabecera `w`, `h`, `count` (u32) y luego `count` imágenes de `w×h` en RGBA, una por canción en
el orden del manifiesto. El port las dibuja con la **posición real del clip** `mc_bgm_now` (padre + hijo) del
juego, mapeando por el letterbox.

## Problema conocido: la posición del nombre

En la selección de música el juego muestra el nombre en **dos** sitios distintos:

- sobre el **carrete** (la entrada que se está desplazando; la "selección"),
- en la **franja inferior** (el tema ya elegido; el "seleccionado").

Es el **mismo clip** (`mc_bgm_now`), movido por cada pantalla/estado, así que el port lo sigue y dibuja el
nombre donde el juego lo haría. El del **seleccionado** (franja) queda en su sitio; el de la **selección**
(sobre el carrete) puede quedar **un poco alto** respecto a la referencia del juego (el clip está animado y su
posición no es la definitiva). Queda pendiente afinarlo. La misma mecánica sirve para el **Map Select**, donde
el nombre va en la franja igual que en los escenarios.

---

## Reemplazar una canción del disco

Ya funciona por el mecanismo de mods: basta poner el ADX en `gamedata/mods/pzs3us2/<índice>.bin` (p. ej.
`64974.bin` es el tema 0). No hace falta nada del port.

## Límites y notas

- **6 canciones** como máximo añadidas (offsets `0x1A..0x1F`; `bgmBits` es un u32).
- El audio se convierte a **ADX** (ffmpeg `adpcm_adx`). El juego lo reproduce con su decodificador ADX; otros
  formatos no suenan.
- La lista de música es un **reel**: las añadidas aparecen antes de Random.
- El overlay va por **Vulkan (SDL_GPU)**. En OpenGL se usa el texto de la fuente del juego.

## Comprobación

```sh
port/tools/add_song.py list
BT3_STAGE_FONT=/ruta/a/compacta.ttf port/tools/add_song.py rebuild
BT3_64=1 port/run.sh menu     # Duel -> Character/Stage Select -> BGM Select
```

En el arranque el port registra cuántas canciones leyó:

```
bt3: songs: 2 added track(s) from gamedata/songs/songs.txt
bt3: song-name overlay: 2 names, 1024x64 each
```
