# 0.1.39.5 R5 1.3

- Ranked now identifies which player confirmed a surrender in the pause menu. A verified abandonment transfers 16 points to the opponent, subject to rating limits. Inconclusive disconnects leave both ratings unchanged.

<!-- bt4-update:en
- A confirmed Ranked surrender penalizes the player who left and awards points to the opponent.
-->
<!-- bt4-update:es
- Una rendición confirmada en Clasificatorias descuenta puntos a quien abandona y se los otorga al rival.
-->
<!-- bt4-update:pt
- Uma desistência confirmada nas ranqueadas desconta pontos de quem saiu e os concede ao adversário.
-->

# 0.1.39.5 R5 1.2

- See the ten highest-ranked fighters and their scores in the Online menu. Open the Global Top directly from Ranked and keep the last standings visible while new data loads. The standings refresh every ten minutes.
- Reserve an online nickname for your account. Existing scores and match history stay with the account when its nickname changes.
- Resume waiting searches more quickly when the Online service switches routes.

<!-- bt4-update:en
- See the Top 10 in the Online menu, with standings refreshed every ten minutes.
- Reserve a unique online nickname and keep your Ranked progress.
- Resume waiting searches more quickly during Online failover.
-->
<!-- bt4-update:es
- Consulta el Top 10 en el menú Online, con una tabla que se actualiza cada diez minutos.
- Reserva un apodo único y conserva tu progreso en Clasificatorias.
- Reanuda más rápido las búsquedas cuando cambia el servidor Online.
-->
<!-- bt4-update:pt
- Consulte o Top 10 no menu Online, atualizado a cada dez minutos.
- Reserve um apelido exclusivo e mantenha seu progresso nas ranqueadas.
- Retome as buscas mais rapidamente quando o servidor Online mudar.
-->

# 0.1.39.5 R5 1.1

- Keep an opponent's disconnect separate from your own surrender in Ranked. When the server confirms an abandonment, the player who stayed gains 16 points and the player who left loses 16, within the rating limits. The result is shown for two seconds even if the game has already returned to the menu.
- Confirm the selected fighter from the roster in individual and native team battles so a stale portrait cannot replace Yamcha with another fighter.

<!-- bt4-update:en
- Fix Ranked abandonment notices and confirm the selected fighter from the roster.
-->
<!-- bt4-update:es
- Corrige los avisos de abandono en Clasificatorias y confirma al luchador desde su casilla.
-->
<!-- bt4-update:pt
- Corrige os avisos de abandono nas ranqueadas e confirma o lutador pelo elenco.
-->

# 0.1.39.5 R5 Hotfix

> **Windows antivirus review in progress.** Users reported alerts for the R5 executable and libdatachannel.dll. The vendor verdicts remain unresolved. See the [investigation](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/blob/main/docs/security/windows-antivirus-review.md) and the [SHA-256 manifests for the Hotfix](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/tree/main/checksums/v0.1.39.5-R5.1).

- Add authenticated Ranked fight liveness and a fixed 16-point transfer for forfeits, applied once after both players have started fighting. Failed room connections and character selection do not cost points; two disconnected clients do not receive a timeout win.
- Allow 30 minutes without room communication before established lobbies and tournaments time out; renew active room leases instead of imposing a total playtime limit.
- Expire Ranked result notifications after two seconds, including when switching menus.
- Keep advertised UDP mappings alive during long matchmaking waits, discover alternate public endpoints on UDP 3478, and add an independent STUN fallback to Tag Team.
- Retry unacknowledged Tag Team negotiation messages within a bounded connection window.
- Join the [official Discord community](https://discord.gg/EBhh5ef3T6) for matchmaking, news and support.

<!-- bt4-update:en
- Award the remaining player 16 points and deduct them from a Ranked opponent who leaves a started fight.
- Dismiss rating notifications after two seconds.
- Improve connection discovery and retry Tag Team negotiation messages.
-->
<!-- bt4-update:es
- Suma 16 puntos al rival que permanece y los descuenta a quien abandona un combate clasificatorio iniciado.
- Oculta los avisos de puntos después de dos segundos.
- Mejora el descubrimiento de conexiones y reintenta los mensajes de conexión de Tag Team.
-->
<!-- bt4-update:pt
- Adiciona 16 pontos ao jogador que permanece e desconta de quem abandona uma luta ranqueada iniciada.
- Oculta os avisos de pontos após dois segundos.
- Melhora a descoberta de conexões e repete as mensagens de negociação do Tag Team.
-->

# 0.1.39.5 R5

- Add separate Ranked queues for solo and native Team Battle, with independent ratings and matchmaking rules.
- Explain starting points, rating changes, weekly records and the hourly Top in a concise localized Ranked guide.
- Start new players at 1000 points, apply Elo K=32, and keep ratings between weekly seasons. Weekly records reset Sunday at 23:59 Argentina time; the public Top refreshes hourly.
- Submit Ranked results only after both participants report the same outcome and match digest. This peer-to-peer agreement is not independent anti-cheat verification.
- Keep the secondary database as the single Ranked rating authority. Ranked matchmaking reports unavailable while the primary route is active, avoiding separate rating histories during failover.
- Keep online room discovery and casual matchmaking separate from Ranked admission and result reporting.
- Ship only the six signed Windows, Linux and macOS update files; the database deployment archive is not a game installer asset.

<!-- bt4-update:en
- Add separate Ranked solo and native Team Battle queues with independent ratings.
- Keep points between weekly seasons, refresh the Top hourly, and reset weekly records Sunday at 23:59 Argentina time.
- Count a result only when both players report matching results; peer reports are not independent anti-cheat verification.
- Keep casual matchmaking separate and ship the six signed platform update files.
-->
<!-- bt4-update:es
- Añade colas clasificatorias separadas para Duelo individual y Combate por equipos nativo, con puntuaciones independientes.
- Conserva los puntos entre temporadas semanales, actualiza el Top cada hora y reinicia el registro semanal los domingos a las 23:59 de Argentina.
- Cuenta el resultado sólo cuando ambos jugadores informan el mismo resultado; los informes entre pares no son una verificación antitrampas independiente.
- Mantiene separado el emparejamiento casual e incluye los seis archivos firmados de actualización para las plataformas.
-->
<!-- bt4-update:pt
- Adiciona filas ranqueadas separadas para Solo e Batalha de Equipes nativa, com pontuações independentes.
- Mantém os pontos entre temporadas semanais, atualiza o Top a cada hora e reinicia o registro semanal aos domingos às 23h59 da Argentina.
- Só contabiliza o resultado quando ambos os jogadores enviam o mesmo resultado; os relatos entre pares não são uma verificação antitrapaça independente.
- Mantém o pareamento casual separado e inclui os seis arquivos de atualização assinados para as plataformas.
-->

# 0.1.39.5 R4

- Restore the full single-view layout after chase-camera updates when cinematic cameras are disabled; cache each visible camera update independently of rollback rendering.
- Keep uploaded effect work buffers on the GPU texture path and recognize pending framebuffer draws before submission, addressing the multicolored blocks during special attacks without removing their original textures.
- Preserve full-width effect pointers for stage streaks, including the Spirit Bomb sequence on 64-bit platforms.
- Restore extended native health bars in the Interface settings in English, Spanish and Brazilian Portuguese, retaining the selected layout at startup.
- Keep Potara names, descriptions and their frames together in widescreen menus; retain the original dialogue inset in Tournament menus.
- Apply the same smooth portrait edge fade to Tournament, Time Nest, customization and related selection screens without editing ISO artwork.
- Correct costume-number plates and arrow direction across the native selection menus.
- Increase label sizes throughout the online menus and render those menus at a bounded higher resolution when the internal setting is X1 or X2; restore the selected resolution on leaving it.
- Retain early duel endpoint and Tag Team negotiation messages until room membership arrives; retry rejected duel endpoint announcements and pass updated candidates to the native handshake.
- Report connection timeouts without claiming that an unconfigured relay was attempted. No new relay provider is required or enabled by this release.
- Add signed principal/reserve matchmaking routing with separate project identities, pinned rooms, invitation namespaces and bounded retries.
- Include an additive Supabase deployment bundle and a coordinated minimum-client gate for new matchmaking. Activation remains disabled until the public signed update packages are verified and the operator completes the database preparation.
- Extend native footer panels without repeating the corner ornaments or leaving discontinuous texture joins; match the main Online entry to the installed BT4 menu lettering and colour ramp.

<!-- bt4-update:en
- Correct full-screen chase-camera layout and repeated camera updates.
- Correct the GPU source used by uploaded special-attack effects.
- Preserve 64-bit stage-effect pointers and restore extended native HUD bars.
- Prepare coordinated online routing and required-update support.
- Correct widescreen menu text, costume selection and portrait fades; improve online menu readability.
- Retain early connection messages and retry failed endpoint announcements.
-->
<!-- bt4-update:es
- Corrige el encuadre completo de la cámara y sus actualizaciones repetidas.
- Corrige la fuente de GPU usada por efectos de ataques especiales.
- Conserva los punteros de efectos de 64 bits y restaura las barras extendidas.
- Prepara el respaldo coordinado del online y las actualizaciones requeridas.
- Corrige textos panorámicos, selección de trajes y difuminados; mejora la lectura del menú online.
- Conserva los mensajes de conexión anticipados y reintenta los anuncios de dirección fallidos.
-->
<!-- bt4-update:pt
- Corrige o enquadramento completo da câmera e suas atualizações repetidas.
- Corrige a fonte da GPU usada pelos efeitos dos ataques especiais.
- Preserva ponteiros de efeitos de 64 bits e restaura as barras estendidas.
- Prepara a reserva coordenada do online e as atualizações obrigatórias.
- Corrige textos panorâmicos, seleção de trajes e bordas suaves; melhora a leitura do menu online.
- Preserva mensagens antecipadas de conexão e repete anúncios de endereço que falharam.
-->

# 0.1.39.5 R3

- Restore Hit's original Time Skip backdrop and allow its HD replacement to load.
- Keep transient effect preparation running under the original pause flag during developer pauses, instead of redrawing stale aura/effect resources.
- Align the rim-light pass with its shading pass's exact interpolated bones, camera and light direction; preserve the rim's own colour and intensity.
- Remove cross-address HD replacement sharing and match native palette cache coordinates to the decoder.
- Replace periodic full-cache memory sweeps with tracked texture storage and a working-set budget that retains recent frame references. Keep a separate bounded CPU image cache.
- Invalidate artwork address aliases when cache entries move, and cancel software enlargement after an HD replacement arrives.
- Keep the native gauges suppressed throughout the Modern HUD exit animation, and upgrade only the exact unmodified bundled script.
- Respect the saved interpolation setting and allocate interpolation history only while enabled.
- Speed up indexed texture matching without adding another texture cache.
- Allocate water wakes for all Tag Team fighters and reject out-of-range object IDs instead of overwriting adjacent particle lists.
- Filter Tag Team RTT samples, ignore duplicate delay feedback, and reset adaptive timing between battles.
- Improve 1v1 room routing, legacy member descriptors and room admission diagnostics.
- Exchange Ginyu's body and current health with the struck opponent, retaining each player's controls and other roster members.
- Match BT4's tournament selection lock instead of disabling navigation in the former Yamcha slot.
- Preserve native ISO artwork and fonts; legacy generated-art/font settings no longer replace them, including after startup language selection.
- Offer the supplied BT4 HD v1.0 Light DDS archive (2.54 GB) as a separate optional GitHub download, verified and read directly from ZIP.
- Restore the ISO's Ragged Clothes Potara model selection, retain local Tag Team item sets, and block mechanical item bonuses on both online paths while retaining recognized visual components.
- Automatically migrate the managed v1.07 pack through Setup at an offline menu, with no ISO or language reinstallation; keep the previous pack until the replacement is verified.
- Receive synchronized development test rules for health, Ki, Blast Stock, pause and single-step. These rules apply equally to all fighters; only the separate Dev client exposes their controls.
- Use simulation family sim6 for the corrected gameplay and shared test-rule inputs. All players in a room need a compatible R3 build.

- Keep Modern Camera presentation consistent when a render frame is skipped; use stable view identities for alternating display buffers and freeze the world camera during native pause.
- Bound camera collision traversal to valid stage data and intersected zones.
- Apply one smooth fade across every piece of a character-selection artwork, preserving original ISO files and unrelated interface elements.
- Restore F1 font choices, including Original, without changing saved choices when selecting a language.
- Restore Modern Look energy lighting on DirectX 12 as well as Vulkan, sample effect colour with bounded work and avoid the lighting pass when no energy light is active.

<!-- bt4-update:en
- Restore Hit's Time Skip effect and correct Modern HUD transition overlap.
- Improve texture-cache lifetime and bounded memory use.
- Improve Tag Team timing feedback and 1v1 room admission.
-->
<!-- bt4-update:es
- Restaura el efecto de salto temporal de Hit y corrige la superposición durante la salida del HUD moderno.
- Mejora la duración de la caché de texturas y su control de memoria.
- Mejora el ajuste de tiempos de Tag Team y el acceso a salas 1 vs 1.
-->
<!-- bt4-update:pt
- Restaura o efeito de salto temporal do Hit e corrige a sobreposição na saída do HUD moderno.
- Melhora a duração do cache de texturas e o controle de memória.
- Melhora o ajuste de tempo do Tag Team e a entrada em salas 1 contra 1.
-->

# 0.1.39.5 R2

- Align health-bar alpha clearing with widescreen HUD scaling, so the remaining blue tier and the exposed green tier use the same mask on both sides.
- Reuse exact matching resident HD textures across GS writes and read every entry of native 16-color palettes when caching textures.
- Bound decoded texture memory and queued GPU frames, including hidden or minimized windows.
- Keep Lua camera smoothing relative to a moving fighter and preserve camera preferences during rollback. Apply the Tag Team camera mod once to the selected view.
- Interpolate rim lighting with the fighter model and sample colored HD ki effects for energy lighting on SDL GPU renderers.
- Track arena-boundary geometry by its own packets instead of reused texture addresses.
- Keep unrelated Tag Team effects running during another pair's cinematics and use external framing rules for clashes.
- Add an optional left-stick rotation control for beam and melee clashes under F1 > Controls. Button controls remain available.
- Show the ultimate technique and maximum-power readiness in Skill List.
- Align the native Team Battle stage reel and honor an explicitly selected installation data folder during setup validation.
- Upgrade unmodified bundled presentation scripts while preserving customized scripts and saved settings.

- Update the online simulation family to sim4 for the Tag Team effect changes. All players in a room need a compatible R2 build.

- Show localized update highlights beside Restart now throughout menus, excluding battles and character selection.

<!-- bt4-update:en
- Correct widescreen health-bar colors and HD texture stability.
- Reduce retained texture memory and queued GPU frames.
- Improve camera smoothing, lighting and Tag Team effects.
- Add stick rotation in clashes and maximum-power readiness.
- Show translated update highlights in menus before restarting.
-->
<!-- bt4-update:es
- Corrige los colores de las barras de vida en panorámica y la estabilidad de las texturas HD.
- Reduce la memoria retenida por texturas y los fotogramas pendientes de la GPU.
- Mejora el suavizado de cámara, la iluminación y los efectos de Tag Team.
- Añade giro de palanca en los choques e indicador de máximo poder.
- Muestra las mejoras de las actualizaciones en los menús antes de reiniciar.
-->
<!-- bt4-update:pt
- Corrige as cores das barras de vida em tela panorâmica e a estabilidade das texturas HD.
- Reduz a memória retida por texturas e os quadros pendentes da GPU.
- Melhora a suavização da câmera, a iluminação e os efeitos de Tag Team.
- Adiciona rotação do analógico nos confrontos e indicador de poder máximo.
- Mostra as novidades das atualizações nos menus antes de reiniciar.
-->

# 0.1.39.5 R1

- Integrate upstream 0.1.39.5 Skill List changes while retaining BT4 and Tag Team presentation mappings.
- Add F1 > Updates with manual checks, retry, download progress and a confirmed restart from the offline main menu.
- Read ADX version 3 and 4 loop points for combat music, restore the predictor at loop boundaries and correct shared music timing metadata.
- Skip Lua presentation work during rollback resimulation and discarded display frames.
- Add optional right-stick enemy targeting and a local special-move camera preference.
- Keep unrelated Tag Team fighters active during another pair's special-move preparation.
- Score both human players' correct button-clash answers independently.
- Restore scripted Parallel Quest setup and descriptions; load all BT4 roster chips in the remaining mission and ranking selectors.
- Center Modern HUD portraits with artwork-specific eye anchors and soften portrait borders.
- Fix the automatic GitHub download and installation flow for the existing DBZ BT4 HD v1.07 pack.
- Preserve installed language profiles, custom scripts, saves, setup ISO selection and signed update checks. The online simulation family advances to sim3; participants need compatible builds.

# 0.1.39.4 R1

- Integrate upstream 0.1.39.4 Lua presentation mods, animated matchmaking radar and widescreen message alignment.
- Include Modern HUD, Modern Camera, Modern Look, Skill List and two examples, disabled by default; deliver them through older BT4 updaters while preserving existing scripts.
- Adapt mod camera and fighter data to Tag Team targeting; keep free-camera priority and Hide HUD behavior.
- Add English, Spanish and Brazilian Portuguese controls for Mods and optional offline duel mechanics.
- Preserve BT4 language setup, HD textures, HUD colors, signed updates and native Windows, Linux and macOS packages.

# 0.1.39.3 R1.1

- Center installed-language choices and download optional packages directly from GitHub; setup only asks for the ISO.
- Fix Portuguese menu captions, restore high-resolution font rendering and keep localized lettering ahead of generic HD artwork.
- Recognize matching HD portraits across texture alpha modes while preserving palette and size checks.
- Align the Potara Shop scroll bar, restore the screen-position preview and correct full health-bar backing colors.
- Refresh the HUD editor preview and run Windows game and setup executables without requesting elevation.

# 0.1.39.3 R1

- Integrate upstream 0.1.39.3 widescreen menus, replay loading and screen reader support.
- Fix Portuguese selection and install its required English data automatically.
- Add Brazilian Portuguese installer text and keep the optional HD pack checkbox visible.
- Download public language and texture packs without GitHub sign-in.

# 0.1.39.2 R9

- Install Brazilian Portuguese through setup and select it from the native game language menu, using the English data profile.
- Download the optional HD texture pack (3.69 GB) with progress and archive verification.
- Isolate Portuguese text and textures from the Spanish and English installations; no command scripts or junctions are needed.
- Keep language packs and HD textures in their dedicated releases, separate from signed game updates.
- Rebuild the Windows RTC library from locked upstream sources and verify its build receipt during packaging.

# 0.1.39.2 R8 Hotfix

- Restore the One on One / native BT3 Team Battle selector in Quick Match and route both modes through the standard online lobby queues.

# 0.1.39.2 R8

- Separate Tag Team Quick Match (2 vs 2) from custom rooms, with 2 vs 2, 5 vs 5 and six-player Free for All presets.
- Restore configurable duel creation and use a four-frame rollback limit for Quick Match.
- Add an online-menu fighter count refreshed every five minutes.
- Expand native message panels without stretching their decorations; align menu groups and enlarge ultrawide labels.
- Synchronize Free for All rules, targeting and winner selection through the participant-hosted room protocol.
- Reset Tag Team input admission when a same-room quick-match roster fills, so the four-player match accepts its first synchronized frames.

# 0.1.39.2 R7 Hotfix

- Load all 250 BT4 portrait sheets in tournament, Hero and Time Nest selectors, including alternate forms.
- Restore BT4 menu music IDs, tournament fanfare and mode-specific Time Nest tracks.
- Preserve panoramic artwork proportions, align costume captions with their panels and hide fighter portraits during stage selection.
- Restore the full spectator controls: free camera, fighter tracking, fight overview, target, speed, zoom and reset.
- Restore keyboard and controller navigation with complete English, Spanish and Brazilian Portuguese labels.
- Keep camera motion outside rollback state; route camera controls before local input is recorded or transmitted.

# 0.1.39.2 R7

- Bundle nested macOS library dependencies, including Brotli, and reject packages with missing or external libraries.
- Expand menus and HUD across the full panoramic viewport, including character, costume, stage, settings and Dragon Wisdom screens.
- Align F1 help with the full-width layout and recognize ordered patch-revision updater tags.
- Align costume captions and Tag Team HUD editor widgets at ultrawide aspect ratios; hide selection portraits on stage previews.
- Expose the local free camera in every battle mode with English, Spanish and Brazilian Portuguese controls.
- Dismiss the Start pause menu immediately when toggling HUD visibility; Start opens it again.

# 0.1.39.2 R6

- Move room discovery and matchmaking to private Supabase signaling.
- Host Tag Team input coordination on the room owner through direct peer channels.
- Negotiate voice directly between participants, including manual-address sessions.
- Preserve room membership for voice across duel and tournament transitions.
- Pin Windows GitHub CLI packaging to its verified upstream archive; remove the deleted R26 bootstrap dependency.
- Retain R5 custom-content, music-tempo, rematch and renderer fixes.
