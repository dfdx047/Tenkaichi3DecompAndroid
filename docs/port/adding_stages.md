# Añadir escenarios (mapas) al port

El port puede cargar **escenarios que no están en el disco**: un mapa es un fichero `.unk` (el modelo de
escenario que sueltan los modders de BT3, el mismo que se reemplaza en PCSX2). El port le da una celda más
en la rejilla de selección, sirve su modelo (y un banco de sonido) al juego, y dibuja su nombre en el menú.

Todo se controla desde un **manifiesto** (`<data>/stages/maps.txt`) que el port lee al arrancar. No hay que
tocar código ni pasar variables de entorno.

---

## Uso rápido (script)

Desde la raíz del repositorio:

```sh
# instalar un mapa (el nombre es el que sale en el menú)
port/tools/add_stage.py add "/ruta/A Mi Mapa.unk" "A Mi Mapa"

# listar los instalados (con su id)
port/tools/add_stage.py list

# quitar uno
port/tools/add_stage.py remove "A Mi Mapa"

# regenerar solo la tira de nombres (si cambias la fuente/estilo)
port/tools/add_stage.py rebuild
```

`add` copia el `.unk` a `gamedata/stages/`, añade la línea al manifiesto y **regenera la tira de nombres**
(`names.rgba`) para que el nombre salga con el estilo del juego.

Opciones:

| opción | efecto |
|---|---|
| `--data <dir>` | carpeta de datos (por defecto `gamedata`, o `$BT3_DATA`) |
| `--font <ttf>` | fuente para la tira de nombres (por defecto `$BT3_STAGE_FONT`, o `port/tools/compacta.ttf`, o una del sistema) |
| `--no-strip` | solo edita el manifiesto; no toca la tira |

### La fuente de los nombres

El juego dibuja los nombres de sus escenarios como **imágenes pre-renderizadas** (no usa una fuente con
glifos). Para que los mapas añadidos tengan el mismo aire, la tira de nombres se **genera** desde un TTF. Si
no hay Pillow o no hay fuente, el manifiesto funciona igual (los mapas salen y se juegan) pero el nombre se
dibuja con la fuente plana del propio juego.

Coloca el TTF en `port/tools/compacta.ttf` (o pásalo con `--font`) para el aspecto de la referencia. El
estilo (relleno crema→naranja, borde marrón, sombra) está en las constantes de `port/tools/add_stage.py`.

---

## Uso manual

1. Copia el `.unk` a `gamedata/stages/`.
2. Añade una línea a `gamedata/stages/maps.txt`:
   ```
   mi_mapa.unk|Nombre En El Menú
   ```
3. Regenera la tira: `port/tools/add_stage.py rebuild`.

Cada línea **no comentada** (`#`) añade un mapa; el orden del fichero es el orden de los ids y el orden de
las imágenes de la tira.

---

## Cómo funciona por dentro

### Ids de escenario

El juego tiene 36 escenarios del disco, ids `0x00`–`0x23`. Los añadidos toman ids **`0x24`, `0x25`, …** (el
campo de desbloqueo `stageBits` es de 64 bits, así que caben hasta el `0x3F`; el port usa 26 como tope,
`0x24`–`0x3D`). Para que esos ids sean seleccionables, el port sube los centinelas de la rejilla:

```
STGGRID_ID_LOCKED 0x24 -> 0x3E      STGGRID_ID_EMPTY 0x25 -> 0x3F   (solo con PORT)
```

y la rejilla pasa a **6×N** (`stageRows = ceil(count/6)`), rellenando la última fila con celdas "empty".

### Ficheros que pide el juego

Para un escenario de id `S` el juego pide (ver `docs/systems/files_and_assets.md`):

| asset | id | de dónde sale el añadido |
|---|---|---|
| modelo (pantalla partida) | `0x171 + S` | **el `.unk` del mapa** |
| modelo (pantalla dividida) | `0x198 + S` | el modelo *single* (el port no usa nunca el split) |
| banco de sonido | `0x14E + S` | el banco del primer escenario (prestado) |
| miniatura del menú | `0x39D + S` | una miniatura existente (los ids altos chocan con la pantalla de carga) |
| nombre | atlas del movie | dibujado por el port (ver abajo) |

El port traduce esas peticiones a ficheros del disco mediante una **tabla de alias** que construye
`port/src/plat_stages.c` a partir del manifiesto; `port/src/plat_file.c` la consulta en `open_rel`.

**Los rangos de ids están empaquetados y se pisan.** Los ids de fichero son contiguos por tipo de asset y el
siguiente rango empieza justo después de los 36 del disco, así que:

* el modelo *split* de los añadidos (`0x198 + S`) cae sobre los **archivos de menú** (`baseFile = 0x1C1`) y
  las transiciones (`0x1BF`, `0x1C0`). Por eso el port **carga siempre el modelo *single*** en batalla
  (`battle_load.c`, con `#ifdef PORT`): nunca se pide el rango *split* y no hay colisión. En pantalla
  dividida se usa el modelo completo.
* la miniatura (`0x39D + S`, con `S ≥ 0x24`) cae sobre las pantallas de carga (`LOAD_FILE_FIRST = 0x3C1`).
  Por eso se reutiliza una miniatura existente (`menu_d.h`, `CHARSEL_STAGE_FILE_ID`).

### El nombre en el menú

El atlas de nombres del movie solo cubre los 36 escenarios del disco. Para los añadidos el clip se oculta y
el nombre lo dibuja el **port** (`ui.cpp`) como una **imagen** encima del frame:

* `gamedata/stages/names.rgba`: cabecera `w`, `h`, `count` (u32) y luego `count` imágenes de `w×h` en RGBA,
  una por mapa en el orden del manifiesto.
* El menú (`menu_c_e.c`) publica cada frame el rectángulo del clip en píxeles del juego (512×448) y qué
  imagen; `gs_gpu.c` publica el rectángulo de la imagen presentada (letterbox en píxeles de ventana);
  `ui.cpp` los combina y dibuja con Dear ImGui.

Si el overlay no está disponible (no hay `names.rgba`, o se usa el back end **OpenGL**), el menú cae a
imprimir el nombre con la **fuente del propio juego** (coloca el nombre con `gPortStageNames`, de
`port/src/plat_stages.c`).

### Compatibilidad con lo anterior

Siguen existiendo, para pruebas, las variables de entorno del prototipo:

* `BT3_EXTRA_STAGES="0x24,0x25"` — se usa **solo si no hay manifiesto**; añade ids sin nombres.
* `BT3_FILE_ALIAS="pzs3us1/00404.bin=/ruta/kaio.unk;…"` — alias manual por ruta relativa.
* `BT3_STAGE_REPLACE="0x1b=0x24,…"` — reemplaza el id de una celda existente por otro (sin añadir).

---

## Límites y notas

* **26 mapas** como máximo (`0x24`–`0x3D`). Más allá, `stageBits` (64 bits) y los sentinelas no dan.
* El **banco de sonido** de los añadidos es el del primer escenario (prestado). Si quieres el suyo, habría
  que darle un id libre o reemplazar un banco existente.
* El modelo se carga en el buffer del juego (`BTL_STAGE_BUF_SIZE = 0x6CB800`, ~7,1 MB). Los modelos
  originales llegan a ~7,12 MB, así que los `.unk` de los mods caben; uno **más grande** se truncaría.
* La **miniatura** (el preview pequeño de la rejilla) es una imagen del juego reutilizada: no es la del
  mapa. Generarla requeriría arte propio.
* El overlay de nombres va por **Vulkan (SDL_GPU)**. En OpenGL se usa el texto de la fuente del juego.

## Comprobación

```sh
port/tools/add_stage.py list
BT3_STAGE_FONT=/ruta/a/compacta.ttf port/tools/add_stage.py rebuild
BT3_64=1 port/run.sh menu     # entra a Duel -> Character/Stage Select
```

En el arranque el port registra cuántos mapas leyó:

```
bt3: stages: 11 map(s) from gamedata/stages/maps.txt
bt3: stage-name overlay: 11 names, 1024x128 each
```
