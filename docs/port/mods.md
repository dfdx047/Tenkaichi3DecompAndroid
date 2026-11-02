# Script mods (Lua)

The port runs **script mods**: small Lua 5.4 programs that read the fight, draw over the game's picture, change the
look the renderer gives it and change how the fight camera is shown. A mod is a folder with a `mod.lua` in it; nothing is compiled and the same mod works
on Windows, Linux and macOS.

This is not the same as `gamedata/mods/`, which replaces files of the disc (textures, models, voices). The two can
be used together.

Code: `port/src/gs/mods.c` (the scripts), `port/src/mods_game.c` (what is read of the game, the camera hook),
`port/src/gs/mods_ui.cpp` (drawing, the settings tab), `port/src/gs/mods_lua.c` (Lua, `port/third_party/lua`).
Examples: `port/examples/mods/` (copy a folder into `mods/` to use it): two small ones, and `modern_camera`,
`modern_hud` and `modern_look`, the BT4decomp port's camera, HUD and effects written as script mods. Tests:
`port/tests/mods/test_mods.c`.


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

Mods are loaded at start, in the order of their folder names. A mod starts switched off: a folder put in `mods/` does nothing until the player switches it on. **F1 > Mods** lists them: switch each one on or off,
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
| `game.me` | online: the side this copy plays (1 or 2); not set offline |
| `game.split` | split-screen layout |
| `game.swapped` | this copy shows the HUD's sides swapped (an online match seen from player 2's side) |
| `game.time` | seconds on the computer's clock, for animations |
| `game.timer` | what the game's timer shows: `value` (seconds left, -1 no limit, or a count) and `clock` (`true`: seconds) |
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
| `status` | `attack`, `speed`, `defence`, `ki`: 1 raised, -1 lowered, 0 as it was (the game's status icons) |
| `hud_hits`, `hud_damage` | what the game's combo counter shows (below 2 / negative: nothing shown) |
| `charging`, `charge_held` | the fighter is charging ki; the charge button is held |
| `cpu` | the side is played by the computer |
| `moves[1..5]` | the fighter's moves: 1, 2 skills (Blast 1), 3, 4 specials (Blast 2), 5 the ultimate. Each: `name` (as the game shows it, `""` for none), `kind` (`"skill"`, `"special"`, `"ultimate"`), `button` (`"circle"`, `"triangle"`) and `direction` (`""`, `"up"`, `"down"`) with the charge button held, `cost` (skills: blast gauge, 100000 a stock; the others: ki, 20000 a bar), `usable` (it can be paid for now) |
| `hit_seq`, `message`, `message_seq` | a count that grows with each new hit count given; the last event message (0 Counter, 1 MAX POWER, 2 Lock on, 3 First Attack, 4 BOOST) and a count of messages |


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
| `hud.gradient(x, y, w, h, top_left, top_right, bottom_right, bottom_left)` | rectangle with a colour at each corner |
| `hud.poly(points, colour)` | filled convex polygon; points `{x1, y1, x2, y2, ...}` or `{{x, y}, ...}` |
| `hud.path(points, colour [, thickness [, closed]])` | polyline |
| `hud.band(top, bottom, colours_top [, colours_bottom])` | the strip between two lines of points (as many in each), a colour per point (one colour, or a list), edges smoothed: curved bars, brush strokes, slanted plates |
| `hud.glow(x, y, radius, middle [, half_way])` | a soft round shade |
| `hud.text(x, y, text, colour [, size [, align [, options]]])` | size in HUD units (16); align `"left"`, `"center"`, `"right"` |
| `hud.image(image, x, y, w, h [, options])` | a PNG of the mod (`bt3.image`); options `u0 v0 u1 v1` (the part, 0..1), `tint`, `flip` |
| `hud.portrait(side, x, y, w, h [, options])` | the fighter's character-select portrait (below) |
| `hud.clip(x, y, w, h)`, `hud.unclip()` | what is drawn between them is cut to the rectangle |
| `hud.hide_game("gauges", "timer", "combo")` | the game's own pieces are left out of the picture this blank, for a mod that draws its own (ask every blank) |

Text options: `outline` (a colour), `outline_size`, `shadow` (a colour), `shadow_x`, `shadow_y`, `bottom` (a colour:
the fill shaded from `colour` at the top to it), `lean` (0.16 leans the letters forward), `baseline` (y is the
baseline), `fit` (a width: the text is made smaller to fit), `font` (`"game"`, the game's lettering, or `"ui"`), and
`after` / `before`: x is then a gap after the end (or before the start) of the text drawn just before, for words laid
out after a number whose width is not known.

Portrait options: `member` (1 = the first; the one fighting by default), `band` (`"colour"`, `"grey"`, or `"white"`:
the shape in white, for glows), `u0 v0 u1 v1`, `head` (a zoom: a square about the head), `tint`, `flip`. With
`face = true` it is placed by the fighter's face: (x, y) is where the face goes and w the portrait's width (it is twice
as high); `window = {x0, y0, x1, y1}` is the part of the picture it is drawn in and `fade = {x, y, r0, r1}` fades it out
from that point between the two radii, a soft round cut.

At most 4096 shapes, 16384 points and 32 KB of text per blank, for all mods together.


## `cam`

`on_camera(cam, game)` is given the camera the game is about to build for one side (`cam.side`, 1 or 2; both in
split screen) and may change it: change `cam.pos` / `cam.rot` in place, or return a table with `pos` and `rot`.

| field | |
| --- | --- |
| `cam.pos` | camera position (`x`, `y`, `z`) |
| `cam.rot` | Euler angles in radians: `x` pitch, `y` yaw, `z` roll |
| `cam.target` | the point the fighter camera looks at |
| `cam.cut` | `true` during the game's own camera work (technique and transformation cuts, fixed and scripted cameras). Change it then only if you mean to. |
| `cam.zoom` | the picture's zoom: 1 the game's, below 1 wider (0.25..4). The stage's and the effects' culling widen with it, and the ground shadows follow it. |
| `cam.collide` | `true` (the default): the pose given is kept out of the stage, traced from what the fighter camera looks at as the game traces its own camera, stopped a little before what it hits, held so that it does not wobble. `false` leaves it where the mod puts it. |

With several camera mods, each is given the pose the one before it returned. A pose that is not a number (NaN, too
large) is not used.

`collide` only knows what the stage's collision knows. Some scenery is drawn without any (on the grassland, slopes
beside the fighting ground): a camera moved far to the side can still end up inside it. Keep sideways moves modest.

Examples: `mods/modern_camera` (its Zoom setting), `mods/skill_list` (the moves with their buttons and costs while
charge is held).


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


## The look of the picture

`bt3.look{...}` asks the renderer to draw the game's picture differently this blank. Call it every blank (from
`on_frame`), like the shapes: a mod switched off or stopped gives the game's own picture back at once. Each value is
0..100, 0 (or left out) leaving that part as the game draws it; with several mods, each is the strongest asked for.

| field | |
| --- | --- |
| `shading` | the fighters' two-tone shading redrawn: cool coloured shadows (toward blue-violet, not too dark) with a soft, short edge, the lit side a little livelier; the colours stay each fighter's |
| `rim`, `rim_color` | light along the fighters' silhouettes, stronger on the side facing the light, in `rim_color` (`0xRRGGBBAA`, alpha unused; default a warm white) |
| `vivid` | livelier colours (dull ones more, skin and sand less), a little more contrast, cool shadows; part of the stage's colour cast taken out, followed slowly so a flash does not swing it |
| `ambient` | soft shadows in creases and where things meet the ground, from the depth buffer |
| `energy` | what adds light (ki blasts, beams, sparks, flashes) drawn stronger and more saturated |
| `energy_light` | that light tinting the ground, rocks and fighters round it |

`vivid`, `ambient` and `energy_light` are one pass over the fight's picture just before its HUD, so the HUD and the
menus keep their colours. Only the GPU renderer draws it (F1 > Video, not OpenGL); the others are drawn by every
renderer. A change of `shading` redraws the fighters' shading ramps once: move it in steps, not every blank.
`BT3_AO_DEBUG=1` shows the ambient shadows alone (2 the depth, 3 the colour balance, 4 the energy light).
Example: `mods/modern_look`.


## `bt3` and the rest of Lua

| | |
| --- | --- |
| `bt3.api` | the version of this interface (1) |
| `bt3.folder` | the mod's folder name |
| `bt3.setting(id)` | a setting's value |
| `bt3.rgba(r, g, b [, a])` | a colour |
| `bt3.image(file)` | a PNG of the mod's folder for `hud.image`: its number, width and height (nil and why if it cannot be read) |
| `bt3.label(id)`, `bt3.locale()` | one of the game's labels in the menus' language (`"combo-hits/0/0"`, see `port/translations/labels`), and that language |
| `bt3.log(...)`, `print(...)` | a line in the log, with the mod's name |
| `bt3.look{...}` | how the renderer draws the picture this blank (The look of the picture) |
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

A script cannot change a fight: it only reads, draws its own shapes, changes how the picture is drawn and changes the
camera pose that is **drawn**.
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
