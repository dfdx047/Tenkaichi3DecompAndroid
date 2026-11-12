-- Skill list: while a player holds the charge button, their skills and specials slide in at the bottom corner on
-- their side (Sparking Zero style): the buttons that use them (all with charge held), skills' cost in blast stocks
-- (a number), specials' in ki bars (segments). Moves that cannot be paid for now are greyed.
-- Only a player's own list: not the computer's, and online only this copy's player (game.me), whose picture this is.
-- HUD units: the picture is hud.h (448) high and hud.w wide.

local PLATE, PLATE_EDGE = 0x16202CE6, 0x2C3A4ACC
local TEXT, TEXT_OFF, SPECIAL = 0xFFFFFFFF, 0x8C939BFF, 0xE9C46AFF
local KI_ON, KI_OFF = 0xF2C21BFF, 0x3B3112CC
local ICON, ICON_RING, ICON_MARK = 0xD8DDE3FF, 0x5B6570FF, 0x3A434DFF

local slide = { 0, 0 } -- per side: 0 hidden .. 1 shown
local hud              -- the drawing calls, given to on_hud

-- a slanted plate (its top shifted outwards by lean), mirrored with X
local function plate(X, x, y, w, h, lean, colour)
    hud.poly({ X(x + lean), y, X(x + w + lean), y, X(x + w), y + h, X(x), y + h }, colour)
end

-- a button icon: a grey disc with a circle or a triangle, or the left stick with an arrow
local function icon(cx, cy, r, what, dir)
    hud.disc(cx, cy, r, ICON)
    hud.circle(cx, cy, r, ICON_RING, 1)
    if what == "circle" then
        hud.circle(cx, cy, r * 0.45, ICON_MARK, 1.6)
    elseif what == "triangle" then
        local a = r * 0.5
        hud.path({ cx, cy - a, cx - a, cy + a * 0.75, cx + a, cy + a * 0.75 }, ICON_MARK, 1.6, true)
    else
        hud.text(cx, cy - r * 0.55, "L", ICON_MARK, r * 1.1, "center")
        local ay = dir == "up" and cy - r * 1.1 or cy + r * 1.1
        local tip = dir == "up" and ay - r * 0.35 or ay + r * 0.35
        hud.poly({ cx - r * 0.4, ay, cx + r * 0.4, ay, cx, tip }, ICON_MARK)
    end
end

local function list(f, left, t, S)
    local W = hud.w
    local function X(x) return left and x or W - x end
    local rows = {}
    for _, m in ipairs(f.moves or {}) do
        if m.name ~= "" then rows[#rows + 1] = m end
    end
    local step = 31 * S
    local y0 = hud.h - 16 - #rows * step            -- the last row ends 16 above the bottom
    local off = (1 - t) * -210 * S                  -- slides in from the screen's edge
    for row, m in ipairs(rows) do
        local y = y0 + (row - 1) * step
        local ultimate = m.kind == "ultimate"
        local special = m.kind == "special" or ultimate
        local x = 16 + 12 * S + 24 * S + off + (special and 10 * S or 0)
        local cost = special and m.cost / 20000 or m.cost / 100000
        plate(X, x - 2 * S, y - 2 * S, 196 * S, 25 * S, 11 * S, PLATE_EDGE)
        plate(X, x, y, 192 * S, 21 * S, 10 * S, PLATE)
        hud.text(X(x + 14 * S), y + 3 * S, m.name, m.usable and (special and SPECIAL or TEXT) or TEXT_OFF,
                 13 * S, left and "left" or "right", { fit = 160 * S, font = "game" })
        if ultimate then
            -- The ultimate requires maximum power, rather than ordinary ki.
            local ready = f.powered_up and m.usable
            local color = ready and SPECIAL or TEXT_OFF
            hud.text(X(x + 183 * S), y - 2 * S, "S", color, 19 * S, "center")
            local locale = bt3.locale() or "en"
            local label = bt3.label("combo-max-power/0/0") or
                (locale == "pt-BR" and "PODER MÁXIMO" or locale == "es" and "MÁXIMO PODER" or "MAX POWER")
            hud.text(X(x + 14 * S), y + 23 * S, label, color,
                     9 * S, left and "left" or "right", { fit = 160 * S, font = "game" })
        elseif special then                             -- ki cost: one segment a bar
            for k = 1, math.max(1, math.floor(cost + 0.5)) do
                local sx = x + 16 * S + (k - 1) * 24 * S
                plate(X, sx, y + 23 * S, 21 * S, 4 * S, 2 * S, f.ki >= k * 20000 and KI_ON or KI_OFF)
            end
        else                                            -- blast stocks: the number at the plate's end
            hud.text(X(x + 183 * S), y - 4 * S, tostring(math.floor(cost + 0.5)), m.usable and TEXT or TEXT_OFF,
                     24 * S, "center", { outline = 0x0B1118FF, lean = 0.12 })
        end
        if bt3.setting("buttons") then                  -- the button, and the stick's direction before it
            local bx = x - 13 * S
            icon(X(bx), y + 10.5 * S, 9.5 * S, m.button)
            if m.direction ~= "" then icon(X(bx - 23 * S), y + 10.5 * S, 9.5 * S, "stick", m.direction) end
        end
    end
end

return {
    name = "Skill list",
    version = "1.0",
    description = "While charge is held: your skills and specials with their buttons and costs, at the bottom corner.",
    settings = {
        { id = "size", label = "Size", type = "slider", min = 50, max = 150, step = 5, default = 100 },
        { id = "always", label = "Always shown", type = "toggle", default = false,
          tip = "Show the list all the time, not only while charge is held." },
        { id = "buttons", label = "Show buttons", type = "toggle", default = true,
          tip = "The buttons (and stick directions) that use each move, beside it." },
    },

    on_hud = function(h, game)
        hud = h
        if not game.hud_shown or not game.fighters then return end
        local S = bt3.setting("size") / 100
        for side = 1, 2 do
            local f = game.fighters[side]
            if f and not f.cpu and (game.me == nil or game.me == side) then
                local want = (f.charge_held or bt3.setting("always")) and 1 or 0
                slide[side] = slide[side] + (want - slide[side]) * 0.35
                if slide[side] < 0.01 then slide[side] = 0 end
                if slide[side] > 0 then
                    -- the left side is player 1's unless this copy shows the sides swapped
                    list(f, (side == 1) ~= (game.swapped == true), slide[side], S)
                end
            end
        end
    end,
}
