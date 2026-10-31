# Dragon Rage 1.0.0

**Budokai Tenkaichi 2 solo con la ISO, Dragon Net Online y los lobbies del PC 0.1.39, el Tag Team Mod como opción y el escenario lejano
de BT4 corregido.** Probado en un RG Cube (Mali-G57) y un Retroid Pocket 5 (Adreno 650), con una pelea grabada y comparación de imagen
píxel a píxel contra la 0.9.0.

## Budokai Tenkaichi 2
- **Solo la ISO**: el motor viene dentro de la app. Ya no hay paquete de motor que importar; si tenías uno, se borra después de un
  aviso y el juego usa el de la app.
- **La ISO se queda donde está**: se lee en su lugar, sin copiarla. Solo los archivos pequeños quedan en la carpeta de la app.
- **Una ISO equivocada se rechaza** con un mensaje que dice qué se encontró y en qué pestaña se instala.
- **BT2 en línea**: partida entre dos aparatos, con pantalla propia en el menú del juego.
- Un error del juego ahora aparece en pantalla, no como pantalla negra. Una escala de salida por debajo del 100% llena la pantalla.

## Budokai Tenkaichi 4
- **Escenario lejano en el renderizador Dragon Rage**: las montañas ya no salen en bloques de ruido de colores. El desenfoque de
  distancia del juego ahora funciona.

## Aparatos débiles
- **BT4**: el valor por defecto ahora va por aparato. Un aparato débil o el modo Lite usa el renderizador Dragon Rage; si ARMSX2 se queda
  por debajo de 27 actualizaciones por segundo durante 5 s en una pelea, el siguiente inicio usa Dragon Rage. Tu elección en Ajustes siempre vale. En
  un RG Cube limitado a un chip como el G52 MC1, ARMSX2 dio de 8 a 10 imágenes por segundo y Dragon Rage de 10 a 11; el juego aún no llega a la velocidad
  completa en ese chip.
- **BT3**: el mismo rendimiento que la 0.9.0 en ese aparato limitado.

## Tag Team Mod
- Hasta 10 luchadores, con los modos y el Tag Team Online del mod de tehmufti y sus colaboradores, adaptados al móvil.
- **Viene desactivado**: actívalo en Ajustes. Desactivado, no aparece en las peleas normales.

## En línea
- **Dragon Net Online**: pantallas nuevas en lugar de Dragon Net Battle, con Clasificatoria, lobbies, espectadores, partida rápida por
  región y entrada por dirección (del PC 0.1.39).
- **Menos peticiones a los servidores**: nada en segundo plano, una espera creciente cuando el servidor rechaza y el mensaje "Servidor
  ocupado, inténtalo más tarde" en lugar de un fallo sin explicación.
- La música aleatoria ya no separa a los dos jugadores de una partida.

## Correcciones a partir de los registros de los jugadores
- **Menu font y rótulos**: ya no piden datos del juego que ya están en la ISO.
- **Aviso falso de paquete de texturas** con la carpeta vacía.
- **Un archivo que falla al leerse**: el juego dice el motivo, lo intenta de nuevo y la pelea continúa.
- **Disco modificado**: un tamaño negativo es un aviso claro, no "sin memoria".
- `.7z` y `.rar` se rechazan con un mensaje claro. Sin internet, buscar actualizaciones ya no aparece como fallo.

## App y controles
- **El mando navega por el launcher** de nuevo, en el RG Cube y el Retroid Pocket 5.
- **Equipo y licencias** en la pantalla Acerca de.
- La asignación del mando se vuelve a leer al abrir el juego.

## Equipo
Regulus CiberSec, z3xox (PC), Androidxxt01, jllacchua, victorgonl, tehmufti y los colaboradores del Tag Team Mod, Team BT4. Artistas:
MarshallPunk y pluantoine.

## Notas de actualización
- Se instala sobre la 0.9.0 y conserva partidas, ajustes y las ISO importadas.
- El modo en línea pide la misma versión en los dos aparatos.
