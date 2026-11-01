-- An example script mod (docs/port/mods.md): a small panel under each side's gauges with the ki in bars and the
-- combo being landed, and a marker that shows which side is in its powered-up mode. Copy this folder into the
-- game's mods/ folder to try it.

local kiBar = 20000 -- the game's ki: 20000 per bar

local function panel(hud, f, x, right, alpha)
    local w, h = 150, 34
    local px = right and x - w or x
    hud.fill(px, 64, w, h, bt3.rgba(0, 0, 0, 140 * alpha), 6)
    hud.text(right and x - 8 or x + 8, 68, string.format("Ki %.1f / %d", f.ki / kiBar, f.ki_max // kiBar),
             bt3.rgba(120, 200, 255, 255 * alpha), 14, right and "right" or "left")
    if f.combo_hits > 1 then
        hud.text(right and x - 8 or x + 8, 82, f.combo_hits .. " hits  " .. f.combo_damage,
                 bt3.rgba(255, 210, 90, 255 * alpha), 14, right and "right" or "left")
    end
    if f.powered_up then
        hud.disc(right and px + 10 or px + w - 10, 74, 5, bt3.rgba(255, 80, 40, 255 * alpha))
    end
end

return {
    name = "Example: fight info",
    version = "1.0",
    author = "Tenkaichi3Decomp",
    description = "Ki in bars and the combo being landed, under each side's gauges.",
    settings = {
        { id = "opacity", label = "Opacity", type = "slider", min = 10, max = 100, step = 5, default = 80,
          tip = "How see-through the panels are." },
        { id = "combo_only", label = "Only during a combo", type = "toggle", default = false },
    },

    on_hud = function(hud, game)
        if not game.battle or not game.hud_shown then
            return
        end
        local alpha = bt3.setting("opacity") / 100
        for side = 1, 2 do
            local f = game.fighters[side]
            if f and (not bt3.setting("combo_only") or f.combo_hits > 1) then
                panel(hud, f, side == 1 and 16 or hud.w - 16, side == 2, alpha)
            end
        end
    end,
}
