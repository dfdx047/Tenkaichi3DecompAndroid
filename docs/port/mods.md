# Script mods (Lua)

The port runs **script mods**: small Lua 5.4 programs that read the fight, draw over the game's picture and change
how the fight camera is shown. A mod is a folder with a `mod.lua` in it; nothing is compiled and the same mod works
on Windows, Linux and macOS.

This is not the same as `gamedata/mods/`, which replaces files of the disc (textures, models, voices). The two can
be used together.

Code: `port/src/gs/mods.c` (the scripts), `port/src/mods_game.c` (what is read of the game, the camera hook),
`port/src/gs/mods_ui.cpp` (drawing, the settings tab), `port/src/gs/mods_lua.c` (Lua, `port/third_party/lua`).
Examples: `port/examples/mods/`. Tests: `port/tests/mods/test_mods.c`.


## Where they go

A folder **`mods`** next to the game (`$BT3_MODS` names another folder), one folder per mod:

```
mods/
  my_hud/
    mod.lua
    lib/helpers.lua      (optional: require("lib.helpers"))
  smooth_camera/
    mod.lua
```

Mods are loaded at start, in the order of their folder names. **F1 > Mods** lists them: switch each one on or off,
change its settings, see why one stopped, and **Reload the scripts** after adding or editing one (no restart
needed). The player's choices are kept in `bt3_mods.txt` (`$BT3_MODS_SETTINGS` names another file; an empty value:
no file). The log (stderr) has a line for each mod loaded or stopped, and whatever a script prints.


## A mod

`mod.lua` ends with `return` and a table:

```lua
return {
    name = "My mod",            -- shown in F1 > Mods (default: the folder's name)
    version = "1.0",
    author = "me",
    description = "What it does.",
    settings = { ... },          -- see Settings

    on_frame  = function(game) end,       -- once per blank (60 times a second)
    on_hud    = function(hud, game) end,  -- draw over the picture, once per blank
    on_camera = function(cam, game) end,  -- the fight camera, each time the game builds it
}
```

All three functions are optional. Code outside them runs once, when the mod is loaded. Variables kept between calls
(a `local` at the top of the file) are the mod's own memory.

A fight frame is two blanks: the game's logic runs at 30 frames a second, `on_frame` and `on_hud` at 60.


## `game`

What every function is given about the game, read at the last blank:

| field | |
| --- | --- |
| `game.frame` | blanks since start |
| `game.battle` | `true` while a fight's fighters are there |
| `game.hud_shown` | the fight is on with its gauges shown (not during cut-scenes or menus) |
| `game.online` | an online session |
| `game.split` | split-screen layout |
| `game.fighters[1]`, `[2]` | player 1's and player 2's side (only in a fight) |

Each fighter:

| field | |
| --- | --- |
| `side` | 1 or 2 |
| `hp`, `hp_max` | the active member's health: 10000 per bar |
| `ki`, `ki_max` | 20000 per bar |
| `blast`, `blast_max` | blast stocks: 100000 per stock |
| `max_power`, `powered_up` | the powered-up mode's timer (0..30000) and whether it is on |
| `combo_hits`, `combo_damage` | the combo this side is landing on the other |
| `chara`, `costume` | character id (the table of `port/src/gs/fight_panels_ui.cpp`) and costume |
| `active`, `count`, `next` | the member fighting (1 = first), members in the team, the one a switch would bring in |
| `members[i]` | each member: `present`, `chara`, `costume`, `hp`, `hp_max` |
| `pos` (`x`, `y`, `z`), `yaw` | world position (+Y is down) and facing, in radians |


## `hud`

`on_hud(hud, game)` draws with these. Coordinates are **HUD units**: the game's picture is `hud.h` (448) high and
`hud.w` wide (597 in 4:3, about 796 in 16:9); (0, 0) is its top-left corner. Colours are `0xRRGGBBAA` integers, or
`bt3.rgba(r, g, b, a)` with 0..255 values. Shapes are drawn in the order they were given, over the game's picture and
under the settings window. Only what was drawn at the last blank is shown: draw every blank.

| call | |
| --- | --- |
| `hud.fill(x, y, w, h, colour [, rounding])` | filled rectangle |
| `hud.rect(x, y, w, h, colour [, thickness])` | rectangle outline |
| `hud.line(x1, y1, x2, y2, colour [, thickness])` | |
| `hud.circle(x, y, radius, colour [, thickness])` | |
| `hud.disc(x, y, radius, colour)` | filled circle |
| `hud.text(x, y, text, colour [, size [, align]])` | size in HUD units (16); align `"left"`, `"center"`, `"right"` |

At most 4096 shapes and 32 KB of text per blank, for all mods together.


## `cam`

`on_camera(cam, game)` is given the camera the game is about to build for one side (`cam.side`, 1 or 2; both in
split screen) and may change it: change `cam.pos` / `cam.rot` in place, or return a table with `pos` and `rot`.

| field | |
| --- | --- |
| `cam.pos` | camera position (`x`, `y`, `z`) |
| `cam.rot` | Euler angles in radians: `x` pitch, `y` yaw, `z` roll |
| `cam.target` | the point the fighter camera looks at |
| `cam.cut` | `true` during the game's own camera work (technique and transformation cuts, fixed and scripted cameras). Change it then only if you mean to. |

With several camera mods, each is given the pose the one before it returned. A pose that is not a number (NaN, too
large) is not used.


## Settings

```lua
settings = {
    { id = "size", label = "Size", type = "slider", min = 0, max = 100, step = 5, default = 50, tip = "..." },
    { id = "glow", label = "Glow", type = "toggle", default = true },
    { id = "style", label = "Style", type = "choice", choices = { "Thin", "Bold", "Outline" }, default = 1 },
}
```

They appear under the mod in F1 > Mods, are saved, and are read with `bt3.setting(id)`: a slider's number, a toggle's
`true`/`false`, a choice's position (1 = the first). A slider with a whole-number `step` and `min` moves in whole
steps. Up to 24 settings, 8 choices each.


## `bt3` and the rest of Lua

| | |
| --- | --- |
| `bt3.api` | the version of this interface (1) |
| `bt3.folder` | the mod's folder name |
| `bt3.setting(id)` | a setting's value |
| `bt3.rgba(r, g, b [, a])` | a colour |
| `bt3.log(...)`, `print(...)` | a line in the log, with the mod's name |
| `require("a.b")` | runs `a/b.lua` of the mod's folder once and returns its result |

Lua's `string`, `table`, `math`, `utf8` and `coroutine` libraries are there. `io`, `os`, `package`, `debug`, `load`,
`loadfile`, `dofile` and `string.dump` are not: a mod cannot read or write files, start programs, load native code or
run compiled Lua.


## Limits and what happens when a script fails

- An error in a script stops that mod (only that one); F1 > Mods shows the message with the file and line.
- One call may run about 20 million Lua instructions (loading the script: 200 million); more stops the mod as an
  endless loop. A mod has 64 MB of Lua memory.
- Reload the scripts to start a stopped mod again.


## Mods and online play, replays, saves

A script cannot change a fight: it only reads, draws its own shapes and changes the camera pose that is **drawn**.
That pose is the view's copy, which the game builds again from the fighter camera at the start of the next frame;
the fight does not read it back (`include/battle/btl_char_cam.h`: only `BtlCharApi_GetCamPose` reads the fighter
camera's pose; movement goes by its yaw). So:

- An online match stays in step whether one player, both or neither has mods, and with different ones.
- Replays play the same. Nothing is written to the saves.
- Blanks that are run again after a rollback are not given to the scripts (no picture is made of them), and the
  camera hook is off in self-check runs (`BT3_SYNCTEST`), which compare the game's whole state.
- What a mod draws is seen only by the player who has it.

The scripts run on the game's thread, between frames: a slow script slows the game. Keep `on_frame` and `on_hud`
light (a few hundred shapes are nothing; building large tables every blank adds up).
