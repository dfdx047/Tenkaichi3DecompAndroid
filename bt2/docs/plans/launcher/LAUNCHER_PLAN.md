# Launcher, settings and in-game overlay

Requested by the user on 2026-10-13 as the final stage before play-testing: a launcher in the style of the PS2 system menu (the "browser"), usable with a controller, that holds the port's settings, and an in-game overlay for what should change on the fly. This replaces the earlier rule that no launcher exists until the game is playable.

## What the user asked for (2026-10-13)

**Launcher**, operated with a controller:

1. **Saves:** load a memory card file that holds a save of the game. Cards are listed from the `saves` folder next to the programs (since 2026-10-19; before, `user/memcards/`).
2. **Controls:** control configuration.
3. **Integer scaling.**
4. **HD textures:** a checkbox. The pack is local, in `work/HD_TEXTURES/`.
5. **Further useful settings.** Chosen by the user: fullscreen and window size, volumes, a frame rate display. The disc image path is added because the program needs it.

**When the launcher appears:** it has its own executable. The game's executable opens the launcher by itself when no configuration file exists (first start); otherwise it starts the game directly.

**Game folders (the user, 2026-10-19):** no file or folder browsing. Next to the programs, `data` holds the disc image and `saves` the memory cards; the game program makes them on its first start and never replaces or empties them (`runtime/host/game_folders.h`). The game program shows the launcher only while the game is not set up (no settings file, or no disc image of `data` chosen); afterwards it starts the game, and `bt2-launcher` opens the launcher any time. The launcher reads both folders again twice a second while it runs, so a disc image or a card moved in appears by itself (the only image is chosen at once, and so is the first card when none is in the slot). Card image files of other programs (`.ps2` and similar) are listed as unsupported. On the first start the cards of the older `user/memcards` are copied into `saves` (nothing moved or replaced). Older settings files still load: their dialog folders and hidden cards are ignored, and a disc given as a full path makes the launcher ask again.

**Overlay**, opened in the game with a button combination (the user proposed Start + Select and left the default open to a better suggestion; see Overlay below):

- sound settings and whatever else should change on the fly;
- quitting the game;
- fast forward, with the buttons that trigger it configurable in the overlay.

**Not wanted:** save states. They were considered and dropped: the game runs as native code, so a state would have to capture native call stacks as well as memory. Fast forward is the only such feature.

**Files:** settings and card files live in `user/` in the repository root, ignored by git.

## What exists today

| Piece | State |
| --- | --- |
| Window | `runtime/host/viewer_window.h`: a Win32 window that shows each frame, with integer or stretched scaling (`BT2_VIEWER_SCALE`). No input, no fullscreen. |
| Sound | Live output on the audio device, which holds the run at the game's speed (`--sound`). Fixed levels. |
| Controller | The native controller service answers the game from a pad script or from live input (keyboard, XInput pad, DualSense) with a fixed default mapping. |
| Memory card | The native card service reports an empty slot. No card image. |
| Settings | Environment variables read by the probe. No settings file. |
| HD textures | Nothing. The GPU renderer draws from GS memory as the game wrote it. |
| Runner | The probe (`bt2-entry-probe`), started through `run_startup_probe.py` with the disc image and probe arguments. |

## Design

### Programs

Three executables (state of 2026-10-13):

| Program | Behaviour |
| --- | --- |
| `bt2-game` | Reads `user/settings`. If the file is missing, it shows the launcher first. Otherwise it starts the game at once with the stored settings. |
| `bt2-launcher` | Always shows the launcher; "Start game" runs the game. |
| `bt2-entry-probe` (the runtime) | Runs the game. The two programs above start it as a child process with `BT2_PLAYER_SETTINGS=<settings file>`, which makes the run a player's run: window, sound, live input, GPU renderer, the settings, the in-game menu. Its messages go to `user/last-run.log`. |

`bt2-game` and `bt2-launcher` are one small source file (`runtime/game/game_main.cpp`) with the host code; they hold no game code and build in seconds. The game code stays in one executable because linking its ten thousand objects three times was not worth the same-window start the first design asked for: the launcher's window closes and the game's opens. "Quit to the launcher" in the in-game menu ends the run with a code that brings the launcher back.

Automated runs (the gate, the discovery loop, pad scripts) never set `BT2_PLAYER_SETTINGS`: they show no launcher and no in-game menu and do not read `user/`.

**Limit today:** the runtime still starts from the captured startup inputs in `work/bt2-generation`, so the programs run from a prepared repository only (they look for that folder above their own). `bt2-launcher --preview-paused` shows the in-game menu over a stand-in picture without starting the game.

### Style

The console's system menu is the model, as in the user's reference pictures (`work/launcher_ui_ref/`, local; matched more closely on 2026-10-19: no title on the first screen, the chosen entry's value on the line under it with a left/right mark for entries the arrows change, glass cubes circling the fan, the disc and card screens as the console's browser, Segoe UI, the cross and circle hints placed as on the console): the first screen is a near-black haze with a ring of glowing points turning beside the list; the screens below it have a violet haze with a slowly turning fan of translucent bars around a dark sphere, the date and time in the corners and a yellow title. The list has no bar: the chosen entry is bright cyan, the others grey, unavailable ones dark. The bottom line names the buttons with their symbols (cross, circle, arrows). The in-game menu is shown on the plain dark haze (the user asked for black or opaque grey), not over the game's picture. Everything is drawn by our own code (`runtime/host/menu_renderer.h`) with a system font. No Sony artwork, logos, sounds or fonts are used. The first screen shows the game's logo, cut out of the project's README banner (`tools/launcher/cut_logo.py` makes `assets/launcher/logo.png`; the build copies it to the `assets` folder next to the programs; without the file the screen has no logo). A first version with a highlighted bar on a blue gradient was rejected by the user as ugly.

### Drawing

The launcher and the overlay draw in software into the same kind of frame the viewer presents (`DisplayFrame`). That keeps them independent of the game's renderer and lets a test or a tool save any screen as an image.

### Structure

New code in `runtime/host/`, one purpose per file:

| Part | Responsibility |
| --- | --- |
| Host input | Keyboard and XInput state, read once per frame. Used by the menus and, through the mapping, by the game's controller service. |
| Settings | A typed settings record, loaded from and saved to `user/`. Unknown or invalid entries are refused by name, not silently dropped. |
| Menu model | Screens, entries, the highlighted entry, what a press does. No drawing and no Win32 in it, so it is unit-tested with scripted presses. Shared by the launcher and the overlay. |
| Menu renderer | Draws a menu model into a frame. |
| Launcher loop | Window, input, model, renderer; ends with "start the game" or "quit". |
| Overlay | Watches for its button combination during the game, pauses the game while open, applies changes at once. |

### Launcher screens

| Screen | Contents |
| --- | --- |
| Main | Start game, Game disc, Memory card, Controls, Display, Sound, Quit; the game's logo above them; when a disc image is chosen, a small disc blinks in the top right corner for four seconds and then goes away. |
| Memory card | The console's browser: grey, the chosen card's icon, name and save count at the top left, the title at the top right, the cards of `saves` as a row of memory card icons (the chosen one lit), card image files of other programs dimmed with a question mark, and a dashed card with a plus for a new empty card; "in use" under the card in slot 1. |
| Controls | Each PS2 button with its keyboard key and its pad button. Selecting an entry waits for the next press and assigns it. The fast forward buttons and the overlay combination. Reset to defaults. |
| Display | Integer scaling on or off (off stretches to the window). Fullscreen or windowed. Window size. Frame rate display on or off. HD textures on or off. Super-sampling. Widescreen on or off (the game's 16:9 patch, from the next start). |
| Sound | Master, music and effects volume. |
| Game disc | The same browser with the `.iso` files of `data` as discs (the chosen one turning); a dashed empty disc while there is none. |

### Overlay

Opened and closed with a configurable combination. Default: the PS button on a DualSense (the game never sees it), L3 + R3 pressed together on any gamepad, Escape on the keyboard. While it is open the game does not advance and the sound is silent.

| Entry | Effect |
| --- | --- |
| Resume | Closes the overlay. |
| Volumes | Master, music, effects; heard at once. |
| Display | Integer scaling, fullscreen, frame rate display; applied at once. |
| Fast forward | Which buttons trigger it, and whether it is held or toggled. |
| Quit game | Ends the program after a confirmation. |

Changes made in the overlay are written to the settings file, so the launcher shows them next time.

**Why not Start + Select by default.** Both mean something to the game, and the first of the two reaches the game before the combination is complete (Start pauses a fight under the overlay). The two stick buttons pressed together are rare in play, and the press that completes the combination is not passed on. Start + Select stays available as a choice in Controls.

### Fast forward

Normally the audio device holds the run at the game's speed. Fast forward lets the run go as fast as the machine allows: fields are produced without waiting for the device, the picture is shown at the display's pace, and the sound is muted while it lasts. It only helps where the run is faster than the game (menus, movies, loading); fights currently run below the game's speed.

### Volumes

The mixer already has separate sources for streams (music, movies, voices in streams) and for the effects sampler. A volume per source and a master volume scale them before the mix is clamped. Full volume is today's level, so the measured stream level and the user's confirmed mix stay the reference.

### HD textures

The overlay and the launcher hold the "HD textures" entry (Display) since 2026-10-20, with "Super-sampling" (Off, 2x, 4x, 8x, 16x) next to it; both take effect from the next start, because the renderer takes them when it starts. The pack is the first folder in `data/textures` that holds a `replacements` folder, as PCSX2's packs do (the game program makes `data/textures` on its first start). How the textures are found and drawn is the [HD textures plan](../graphics/HD_TEXTURES_PLAN.md) (pack 2, made for the retail disc). The packs are never committed; their threads are credited in the README when the feature ships.

### 60 fps battles

An optional entry under Display, off by default ([60 fps plan](../graphics/HIGH_FRAME_RATE_PLAN.md)); shown as unavailable until it works.

## Order of work

1. **Host input and controller support — done 2026-10-13:** the keyboard, the first XInput pad and a DualSense read directly over HID (USB and Bluetooth) feed the native controller service (`runtime/host/host_input.h`, `input_mapping.h`, `dualsense_report.h`). Not checked: an XInput pad and a DualSense on real hardware (the user tries them).
2. **Launcher shell — done 2026-10-13:** settings file (`settings.h`), menu model (`menu_model.h`), presses from held buttons (`menu_input.h`), renderer (`menu_renderer.h`), the loop and the shared settings screens (`menu_host.h`), the launcher (`launcher.h`), the two programs. Screens: Main, Controls, Display (integer scaling, fullscreen, window size, frame rate display), Sound, Game disc.
3. **In-game menu — done 2026-10-13** (`overlay.h`, `player_session.h`): the combination, pause, Resume, Sound, Display, Fast forward, Controls, Quit to the launcher, Quit game. Volumes are the mixer's (`AudioMixer::master_percent`, `stream_percent`, `extra_percent`). Fast forward: the pacer stops waiting and the sound is not sent to the device.
4. **Controls screen — done 2026-10-13:** each PS2 button's key and gamepad button, the fast forward key and button (held or switching), the choice between both stick buttons and Start + Select for the in-game menu, reset to defaults.
5. **Saves — next:** a memory card image behind the native card service ([memory card plan](../memory-card/MEMORY_CARD_PLAN.md)), then the Saves screen, then bringing in a save-exchange file (the user's is a `.cbs`). The launcher shows Saves as not available until then.
6. **HD textures:** the investigation above, then the feature, then the checkbox (shown as not available).

**Checked so far:** unit tests (settings read back and refused, the menu model under presses, repeats, the combination, fast forward, the mixer's volumes); the launcher's screens and the in-game menu seen drawn in a window (`work/frames/launcher/`); the game started through `bt2-game.exe` to its first dialogue. **Not checked:** the in-game menu opened inside a running game, fast forward, any gamepad, fullscreen. These are the user's to try first.

## Verification

- **Unit tests:** the menu model driven by scripted presses (navigation, wrap-around, entering and leaving screens, assigning a button, toggles, sliders); settings written and read back; invalid settings refused; the mapping from host input to the PS2 pad state; the overlay combination (what reaches the game); the mixer's volumes.
- **Images:** each launcher and overlay screen saved to `work/frames/` for the user's inspection.
- **Game side:** the controller service's replies under live input have the same form as under the pad script, which is compared with the original; a card image created by the launcher is accepted by the game (its "create a save file" path), a save written by the port is read back, and a card file from PCSX2 with a save of the game is loaded by the game.
- **The gate** is unaffected: probe runs do not read `user/` and show no launcher or overlay.
- **The user:** operates the launcher and the overlay with a controller and confirms the settings take effect.

## Open questions

- **HD textures:** whether the GPU renderer can substitute textures at all (step 6 starts with this).
- **Card file formats:** the target is the card file PCSX2 writes (to my knowledge a raw image of the card that includes each page's spare bytes; to be confirmed on a real file when the Saves step starts). Folder-based cards and other layouts are refused by name unless the user needs one.
- **Gamepads:** XInput pads and the DualSense are read. A DualShock 4 has a different report layout and is not read; it is added if the user needs it.
