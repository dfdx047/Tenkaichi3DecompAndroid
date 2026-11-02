-- Modern look: the vivid, luminous style of later anime games (Sparking Zero, the anime of Dragon Ball Super),
-- drawn by the renderer over the game's own picture:
--   - anime shading: the fighters' two-tone shading with cool, coloured shadows and a soft short edge, instead of
--     the PS2's blurred ramp; the colours stay each fighter's, so it suits any model or costume mod;
--   - rim light: a thin edge of light along the fighters' silhouettes, stronger on the side facing the light;
--   - vivid colours: livelier colours (the dull ones more; skin and sand less), a little more contrast, cool
--     shadows, and part of the stage's own colour cast taken out so a warm stage does not go orange;
--   - ambient shadows: soft shadows in creases and where rocks and fighters meet the ground;
--   - brighter energy: ki blasts, beams and sparks stronger and more saturated;
--   - energy light: that light tinting the ground, the rocks and the fighters round it.
-- Only the picture changes: the fight, replays and online play are the same with or without it.
-- Vivid colours, ambient shadows and the energy light need the GPU renderer (F1 > Video), not OpenGL.
-- (The BT4decomp port's "Effects" settings, written as a script mod: docs/port/mods.md, bt3.look.)

local RIM_COLOURS = {
    0xFFECD6FF, -- warm white (the BT4 port's)
    0xD8ECFFFF, -- cool white
    0xFFD27AFF, -- gold
    0x8FD8FFFF, -- cyan
}

local function slider(id, label, default, tip)
    return { id = id, label = label, type = "slider", min = 0, max = 100, step = 5, default = default, tip = tip }
end

return {
    name = "Modern look",
    version = "1.0",
    author = "BT4decomp",
    description = "Anime shading, rim light, vivid colours, ambient shadows and glowing energy.",
    settings = {
        slider("shading", "Anime shading", 100, "Cool coloured shadows with a soft edge on the fighters."),
        slider("rim", "Rim light", 100, "Light along the fighters' silhouettes."),
        { id = "rim_colour", label = "Rim light colour", type = "choice",
          choices = { "Warm white", "Cool white", "Gold", "Cyan" }, default = 1 },
        slider("vivid", "Vivid colours", 100, "Livelier colours and a neutral balance (GPU renderer)."),
        slider("ambient", "Ambient shadows", 100, "Soft shadows in corners and at the foot of things (GPU renderer)."),
        slider("energy", "Brighter energy", 100, "Ki blasts, beams and sparks stronger and more saturated."),
        slider("energy_light", "Energy light", 0, "Energy lights up the ground and fighters round it (GPU renderer)."),
    },

    -- Asked every blank: switching the mod off (F1 > Mods) gives the game's own picture back at once.
    on_frame = function(game)
        bt3.look {
            shading = bt3.setting("shading"),
            rim = bt3.setting("rim"),
            rim_color = RIM_COLOURS[bt3.setting("rim_colour")] or RIM_COLOURS[1],
            vivid = bt3.setting("vivid"),
            ambient = bt3.setting("ambient"),
            energy = bt3.setting("energy"),
            energy_light = bt3.setting("energy_light"),
        }
    end,
}
