# Ranked C/D de DBT-NX

Estado: implementacion local y pruebas; **no es un despliegue de produccion**.
Solo afecta a Ranked. No cambiar ni desplegar estas pasarelas sobre A o B.

## Archivos

Cliente: `community.h/c`, `community_pair.h`, `community_cd_http.inc`,
`community_match_http.inc`, `ranked_android.c`, `ranked.c`, `ranked_cd.inc`.
Servidor: `ranked_gateway.js`, `ranked_authority.js`, `ranked_cd_route.js`,
`ranked-cd-schema.sql`, las dos configuraciones de ejemplo y la aceptacion
opcional de DP20 en `ranked.js`. Pruebas: `check_ranked_cd.py`, fixtures de
Ranked existentes y `ranked-cd-test.mjs`.

## Contrato y alcance

```
Switch -> C o D -> autoridad E -> una sola D1
```

C: https://bt-ranked.arielnndrd.workers.dev

D: https://bt3-net-rooms.alexsanchez01es.workers.dev

E necesita una cuenta/cuota independiente de C y D para proporcionar respaldo
ante el agotamiento de una de esas cuentas. Se conserva la sesion CLI de B y
se autorizo por separado la cuenta alexsanchez01es de D. Se comprobo D y se
respaldo su Worker/configuracion y la D1 (rooms/used, ambas vacias). No se
modifico el servicio desplegado ni se crearon tablas. Sigue pendiente la
cuenta independiente de E y la colaboracion del administrador de C.

Las dos pasarelas ejecutan exactamente `ranked_gateway.js`, con distinto ID y
secreto. No guardan copias locales de las puntuaciones ni tienen binding D1.
E ejecuta los modulos actuales de resultados y `flow:2` contra una D1 unica.
Incluye tambien las salas de Ranked: asi `ranked/room` valida la misma sala
que crearon/consultaron los dos jugadores, incluso si usan accesos distintos.
No se habilitan chat, presencia, partida rapida ni TURN en C/D.

El cliente comparte la politica de A/B: tres fallos reales consecutivos o una
respuesta de cuota reconocida seleccionan la reserva durante ese dia UTC.
Los errores logicos JSON (sala inexistente, reglas, identidad, busy) no son
fallos de transporte. Los contadores/archivos de C/D son independientes.
En E la fila global y los destinos de las partidas viven en D1. Si D es usado
por un cliente que ha perdido C, E comprueba `/gateway-health` de C antes de
cambiar globalmente a D. Un fallo de red local de un jugador no demuestra
que C este caido para todos. En ese caso puede seguir sin poder conectar;
no se promete resolver cualquier bloqueo local de red.

Ambos jugadores reciben el mismo `backend` y `route_epoch` para el mismo UUID.
El NRO fija las operaciones de sala a ese destino. Cualquier informe de
resultado puede llegar por C o D a E; su identidad, UUID, ronda y digest se
mantienen y las transacciones existentes impiden aplicar puntos dos veces.
Si E falla, se conserva el informe local y no se liquida contra otra base.

La transicion C->D cancela conjuntamente las asignaciones C sin confirmacion
de ambos jugadores, marcador de inicio ni resultados. Las peleas que ya
empezaron y las conexiones confirmadas por ambos se conservan, incluso si
el informe HTTP de inicio aun se esta reintentando.
Al cambiar el dia UTC las nuevas asignaciones vuelven a C; una pelea D
asignada conserva D, tambien al crear una revancha nueva antes del cambio.
Cada revancha aceptada sigue teniendo su propio UUID; no se toca `flow:2`.

## Compatibilidad y persistencia

El NRO primero consulta capacidades sin enviar identidad. Un C antiguo sigue
funcionando en modo C unicamente; se consulta una vez por hilo y requiere
reiniciar el juego despues de migrar el servicio. No se duplica el polling.
Solo un C verificado por HTTPS, con `cd:1` y `migration_complete:1`, establece
la autoridad local. D debe confirmar esa misma autoridad y las URLs esperadas
antes de recibir tokens, claves de sala o resultados. Una autoridad distinta
se rechaza; no se borra ni se vuelve a registrar el perfil existente.

Archivos nuevos bajo `sdmc:/switch/bt3-switch-data/`:

- `ranked-authority.txt`: ID permanente de E, aprendido de C.
- `ranked-failover.txt`: dia UTC de seleccion de D; independiente de A/B.
- `ranked-route.txt`: destino C/D de la ultima asignacion; no vence a medianoche.
- `ranked-url-d.txt`: override opcional, solo `https://hostname`, sin ruta,
  credenciales, puerto, parametros o fragmento. Sin archivo se usa la constante D.

Los ficheros existentes de identidad, ACTIVE, NEXT, cancelaciones y outbox no
cambian de formato. La propuesta inicial de versionarlos se ha simplificado:
el destino ya no es una base diferente y E resuelve cada UUID en una unica
autoridad. El ID de autoridad persistente impide pasar a otra clasificacion;
la ruta de sala se conserva por separado. Esto evita reescribir o perder los
resultados antiguos durante una migracion. Es obligatorio importar **toda**
la base de C, incluidos hashes de token, partidas/rondas/flow y resultados.
Un ID de autoridad no demuestra por si solo que esa importacion este completa:
`MIGRATION_COMPLETE=1` es una declaracion que el administrador debe verificar.

No se modifican UDP, servicio 5001, simulacion, delay automatico, renderer ni
30/60 FPS. Android debe implementar las capacidades/rutas para garantizar
el mismo comportamiento C/D; el APK actual no adquiere respaldo por cambiar
solo el servidor. A/B y PC no reciben las capacidades ni credenciales de C/D.

## Recursos y migracion (administradores de C, D y E)

1. Elegir una cuenta gratuita independiente para E. No contratar Paid ni TURN.
   Crear D1 `ranked-authority` y Worker `ranked-authority`. El binding se llama
   `DB`. Usar `wrangler-ranked-authority.example.jsonc`, copiandolo a una
   configuracion privada con los IDs reales y `MIGRATION_COMPLETE=0`.
2. Generar un ID aleatorio permanente para la autoridad y dos claves aleatorias
   distintas de al menos 32 bytes. Guardar GATEWAY_C_KEY y GATEWAY_D_KEY como
   secretos de E; cada pasarela solo recibe su propia clave AUTHORITY_KEY.
   Ninguna clave administrativa/Cloudflare se incluye en el NRO ni el TXT.
3. El administrador de C respalda su Worker/configuracion y exporta su D1.
   La exportacion contiene identidades privadas: no subirla a GitLab ni al
   ZIP para jugadores. Pausar nuevas solicitudes Ranked en C para tener un
   punto de corte; esperar/reconciliar peleas y resultados pendientes, o
   conservarlos integramente para que los clientes los reintenten en E.
4. Importar la exportacion completa en E. No inicializar una clasificacion
   vacia ni volver a crear usuarios. Comprobar recuentos, rating/wins/losses,
   hashes, rondas, sesiones y muestras conocidas antes y despues. Esquema base:
   `schema.sql`, `ranked-schema.sql`, con las migraciones historicas que falten
   (no ejecutar ALTER duplicados). Luego ejecutar `ranked-cd-schema.sql` una
   vez: agrega tablas y atribuye las partidas importadas a C sin cambiar puntos.
5. Probar en un entorno de ensayo separado. La config de E admite solo 5001,
   `DP20_ENABLED=1`, sin chat ni TURN. La unica nueva regla admitida
   opcionalmente es `dp20-inf-v1` con flow:2; no se mezcla con individual/equipo.
6. Publicar las fuentes exactas, licencias y creditos antes de ofrecer el
   servicio. Los endpoints `/source` de las pasarelas apuntan a GitLab; ese
   repositorio debe contener esta revision antes del lanzamiento. No copiar
   ni distribuir ISO, ROM, datos extraidos, claves o recursos del juego.
7. Actualizar C y D con `ranked_gateway.js`, IDs C/D, misma AUTHORITY_ID/URL de
   E y sus secretos diferentes. El nombre de Worker D sigue siendo
   `bt3-net-rooms`; pasa a ser respaldo Ranked, no salas generales.
   Esto requiere la colaboracion del propietario de C. Las pasarelas solo
   responden 503 mientras E siga con MIGRATION_COMPLETE=0.
8. Una vez validada la importacion y el punto de corte, establecer
   `MIGRATION_COMPLETE=1`. Reiniciar dos NRO de prueba y verificar C, D por
   fallo simulado de C, ambos roles, salas, dos revanchas, puntos y UTC.
   No apagar C a mitad de partidas de usuarios para hacer la prueba.
9. Publicar el NRO y coordinar Android solo despues de esa validacion.
   No activar nuevas escrituras simultaneas en la antigua D1 de C: produciria
   dos fuentes divergentes. Un rollback despues de aceptar puntos nuevos
   requiere reconciliar/exportar E; no basta con restaurar la antigua D1.

No es posible completar los pasos de C con una sesion que solo tenga acceso
a la cuenta de B o D. No se modifico ningun servidor remoto en esta entrega.

## Limites y comprobaciones

La autoridad E tambien consume una peticion por operacion y tiene el limite
gratuito de su cuenta. C/D **no convierten 100000 en 200000 partidas/consultas**.
El respaldo mejora disponibilidad de acceso; E sigue siendo una dependencia.
Los limites de lectura/escritura D1, CPU e invocacion siguen aplicando.
No se implementa un contador global exacto ni cambio preventivo antes de
100000: responde a fallo real/1027. Las metricas no aseguran el corte exacto.

Fuentes verificadas el 22-10-2026:
https://developers.cloudflare.com/workers/platform/limits/
https://developers.cloudflare.com/d1/platform/limits/

Pruebas locales (sin cuentas ni trafico de jugadores):

```
node port/matchmaking/ranked-cd-test.mjs
node port/matchmaking/ranked-test.mjs
node port/matchmaking/ranked-flow-test.mjs
node port/matchmaking/ranked-rematch-test.mjs
python3 port/switch/check_ranked_cd.py
python3 port/switch/check_community_services.py
python3 port/switch/check_community_ranked.py
python3 port/switch/check_ranked_flow_client.py
```

El test del servidor ejecuta gateway, autoridad y Worker reales con SQLite
local como adaptador de D1 y verifica el presupuesto de 50 consultas de Free.
No sustituye una prueba en Cloudflare ni partidas fisicas Android/Switch.

Licencias: se conservan las licencias, atribuciones y fuentes de la base,
Switch y Android presentes en el proyecto. Los modulos nuevos indican MIT;
esto no relicencia el proyecto combinado ni otorga derechos sobre el juego.
Port no oficial; se exige una copia ORIGINAL del juego obtenida legalmente.
