-- Modern HUD: drawn over the picture in place of the game's gauges, faces, timer and combo counter.
-- Each side has, from its corner toward the middle of the screen:
--   - the fighter: its character-select portrait, cut softly round the head and shoulders, facing the middle;
--   - one pip per reserve health bar, each in its bar's colour;
--   - the health bar along a gentle curve, thinning toward its end like a brush stroke: the bar being spent over the
--     next bar's colour, the damage just taken in red, running down after a moment;
--   - one ki cell per bar, the ki just spent in red the same way; then the max power gauge: blue spreading over the full cells while it charges, and
--     the powered-up mode's time running down in them;
--   - the blast stock count, in a blade that lights up as the next stock fills;
--   - under them, a chip per status the fighter has raised or lowered;
--   - a plate with the combo it is landing: the hit count, the damage counting up, and the event messages.
-- The timer is drawn too. Announcements, technique names and the rest stay the game's.
-- (The BT4decomp port's "HUD Style: Modern", written as a script mod: docs/port/mods.md.)

local sin, cos, sqrt, floor, ceil, min, max, pi = math.sin, math.cos, math.sqrt, math.floor, math.ceil, math.min, math.max, math.pi
local function clamp01(v) return v < 0 and 0 or v > 1 and 1 or v end

-- ------------------------------------------------------------------------------------------------- the layout
-- In units of the picture's height / 940 (the mock-up the layout was agreed on); x from the side's own edge of the
-- picture toward the middle, y down.
local kBarX0, kBarX1 = 206, 606
local kPipX, kPipStep, kPipW = 214, 22, 17
local kKiX, kKiStep, kKiW = 196, 70, 67
local kFaceX, kFaceY, kArtW = 100, 92, 236

-- each health bar's colour by how many are left counting the one being spent (1 = the last): dark, light
local kBar = {
    { { 232, 52, 40 }, { 255, 128, 92 } },  -- 1 red
    { { 245, 132, 20 }, { 255, 196, 90 } }, -- 2 orange
    { { 236, 202, 30 }, { 255, 240, 130 } }, -- 3 yellow
    { { 40, 200, 70 }, { 170, 255, 130 } }, -- 4 green
    { { 30, 196, 180 }, { 150, 255, 236 } }, -- 5 teal
    { { 40, 140, 245 }, { 150, 210, 255 } }, -- 6 blue
    { { 150, 90, 245 }, { 214, 180, 255 } }, -- 7 violet
}
local OUTLINE = { 0, 0, 0, 0.9 }    -- every part's outline
local LOST = { 228, 28, 28, 0.95 }  -- health and ki just lost, before it runs down
local kMp = { { 130, 210, 255 }, { 40, 110, 255 } } -- the max power gauge: blue, told apart from the ki at a glance
local kKi = { [false] = { { 255, 222, 84 }, { 240, 168, 10 } }, [true] = { { 255, 170, 60 }, { 255, 96, 30 } } }
local kStatColour = { { 232, 70, 52 }, { 70, 200, 255 }, { 120, 150, 255 }, { 255, 210, 60 } }
local kStatName = { "attack", "speed", "defence", "ki" }
local kStatShort = { "ATK", "SPD", "DEF", "KI" }
local kMsgLabel = { [0] = "combo-counter/0/0", "combo-max-power/0/0", "combo-lock-on/0/0", "combo-first-attack/0/0", "combo-boost/0/0" }
local kMsgEnglish = { [0] = "Counter!", "MAX POWER!", "Lock on", "First Attack!", "BOOST!" }
local kMsgColour = { [0] = { 255, 120, 30 }, { 40, 160, 255 }, { 255, 170, 30 }, { 235, 70, 40 }, { 255, 140, 20 } }

-- the stroke every row follows: a gentle curve falling toward the middle of the screen
local function curve(x) return 20 + 0.000105 * (x - 150) * (x - 150) end

-- ------------------------------------------------------------------------------------------------- colours
local alphaAll = 1 -- the whole HUD's (fading in and out), times the setting

local function rgba(r, g, b, a)
    local A = floor(255 * a * alphaAll + 0.5)
    A = A < 0 and 0 or A > 255 and 255 or A
    return (floor(r) << 24) | (floor(g) << 16) | (floor(b) << 8) | A
end
local function col(c, k, a)
    return rgba(min(255, c[1] * k), min(255, c[2] * k), min(255, c[3] * k), a)
end
local function mix(a, b, t)
    local r = 0
    for s = 0, 24, 8 do
        local x, y = (a >> s) & 255, (b >> s) & 255
        r = r | ((floor(x + (y - x) * t + 0.5) & 255) << s)
    end
    return r
end

-- ------------------------------------------------------------------------------------------------- a side's view
-- X / Y: units -> HUD coordinates, mirrored on the right side
local U, W = 1, 640
local function view(right, dx, dy)
    dx, dy = dx or 0, dy or 0
    return {
        right = right,
        X = right and function(x) return W - x * U + dx end or function(x) return x * U + dx end,
        Y = function(y) return y * U + dy end,
    }
end

-- A strip along the curve from x0 to x1: its top `off` under the curve, th0 thick at x0 and th1 at x1, the top edge
-- running cutL / cutR further than the bottom at the ends (their slant). Colours: c0 -> c1 across (along x from gx0
-- to gx1) when `across`, else top c0, bottom c1. Returns the top and bottom points (for an outline).
local function strip(hud, v, x0, x1, off, th0, th1, cutL, cutR, c0, c1, across, gx0, gx1)
    local n = ceil((x1 - x0) / 10)
    n = n < 1 and 1 or n > 95 and 95 or n
    local top, bot, ct, cb = {}, {}, {}, {}
    for i = 0, n do
        local t = i / n
        local xb = x0 + (x1 - x0) * t
        local xt = x0 + cutL + (x1 + cutR - x0 - cutL) * t
        local th = th0 + (th1 - th0) * t
        top[#top + 1] = { v.X(xt), v.Y(curve(xt) + off) }
        bot[#bot + 1] = { v.X(xb), v.Y(curve(xb) + off + th) }
        if across then
            local g = gx1 > gx0 and clamp01((xb - gx0) / (gx1 - gx0)) or 0
            ct[#ct + 1] = mix(c0, c1, g)
            cb[#cb + 1] = ct[#ct]
        else
            ct[#ct + 1], cb[#cb + 1] = c0, c1
        end
    end
    hud.band(top, bot, ct, cb)
    return top, bot
end

local function outline(hud, top, bot, c, th)
    local pts = {}
    for i = 1, #top do pts[#pts + 1] = top[i] end
    for i = #bot, 1, -1 do pts[#pts + 1] = bot[i] end
    hud.path(pts, c, th, true)
end

-- a four-cornered plate (clockwise from top left) with a colour at each corner
local function plate(hud, a, b, c, d, ca, cb, cc, cd)
    hud.band({ a, b }, { d, c }, { ca, cb }, { cd, cc })
end

-- text leaning forward, outlined, its fill shaded top to bottom; x is where it starts on the side (it runs toward the
-- middle of the screen: right-aligned on the right side)
local function text_fx(hud, v, t, x, baseY, size, top, bot, line, lineK, follow)
    hud.text(follow and x * U or v.X(x), v.Y(baseY), t, top, size, v.right and "right" or "left", {
        bottom = bot, outline = line, outline_size = size * (lineK or 0.08), lean = 0.16, baseline = true,
        after = follow and not v.right, before = follow and v.right,
    })
end

local function number(hud, v, t, x, y, size)
    hud.text(v.X(x), v.Y(y), t, rgba(255, 255, 255, 1), size, "center", {
        bottom = rgba(169, 226, 255, 1), outline = rgba(4, 10, 28, 0.95), outline_size = size * 0.06, lean = 0.16, baseline = true,
    })
end

-- ------------------------------------------------------------------------------------------------- animation
local anim = { {}, {} }
local enterAt, leaveAt, wasOn, last = 0, -10, false, nil
local lastTime

local function blade(hud, v, t, colours)
    local cx, cy, r, a0, a1 = 60, 236, 64, -pi / 2 - 0.25, 0.15
    local n = 36
    local top, bot = {}, {}
    for i = 0, n do
        local k = i / n * t
        local a = a0 + (a1 - a0) * k
        local w = 1.2 + 8 * sin(pi * min(1, k * 1.1))
        top[#top + 1] = { v.X(cx + cos(a) * r), v.Y(cy + sin(a) * r) }
        bot[#bot + 1] = { v.X(cx + cos(a) * (r - w)), v.Y(cy + sin(a) * (r - w)) }
    end
    local cs = {}
    for i = 0, n do cs[#cs + 1] = colours(i / n * t) end
    hud.band(top, bot, cs, cs)
    return top, bot
end

local function draw_stats(hud, v, f, an, dt)
    an.stat = an.stat or { 0, 0, 0, 0 }
    local n = 0
    for k = 1, 4 do
        local st = f.status and f.status[kStatName[k]] or 0
        local on = st ~= 0
        an.stat[k] = an.stat[k] + ((on and 1 or 0) - an.stat[k]) * clamp01(dt * 10)
        if an.stat[k] >= 0.02 then
            an.up = an.up or {}
            if on then an.up[k] = st > 0 end -- (while it fades out: the way it was)
            local up = an.up[k]
            local a = an.stat[k]
            local x, sz = 204 + n * 40, 30
            local y = curve(x + 15) + 64
            n = n + 1
            local keep = alphaAll
            alphaAll = alphaAll * a
            local c = kStatColour[k]
            local p0, p1, p2, p3 = { v.X(x + 6), v.Y(y) }, { v.X(x + sz + 6), v.Y(y) }, { v.X(x + sz), v.Y(y + sz) }, { v.X(x), v.Y(y + sz) }
            local back = rgba(6, 14, 34, 0.8)
            plate(hud, p0, p1, p2, p3, back, back, back, back)
            hud.path({ p0, p1, p2, p3 }, col(c, 1, up and 0.9 or 0.45), 1.4 * U, true)
            local ts = 20 * U -- (the game's icons are its own pictures: a short word stands in for them)
            hud.text(v.X(x + 3 + sz / 2), v.Y(y + sz / 2) - ts * 0.5, kStatShort[k],
                     up and rgba(255, 255, 255, 1) or rgba(150, 150, 160, 0.9), ts, "center",
                     { outline = rgba(4, 10, 28, 0.9), outline_size = 1.2 * U, fit = (sz - 6) * U })
            -- the arrow at the chip's corner: up green, down red
            local ax, ay, ar = v.X(x + sz + 2), v.Y(y + (up and 5 or sz - 5)), 6 * U
            local t0 = up and { ax, ay - ar } or { ax, ay + ar }
            local t1 = up and { ax + ar, ay + ar * 0.7 } or { ax + ar, ay - ar * 0.7 }
            local t2 = up and { ax - ar, ay + ar * 0.7 } or { ax - ar, ay - ar * 0.7 }
            hud.poly({ t0, t1, t2 }, up and rgba(92, 255, 122, 1) or rgba(255, 75, 58, 1))
            hud.path({ t0, t1, t2 }, rgba(4, 10, 28, 0.9), 1.5 * U, true)
            alphaAll = keep
        end
    end
end

local function shown_damage(an, now)
    local t = an.dmgFor and an.dmgFor > 0 and clamp01((now - an.dmgAt) / an.dmgFor) or 1
    return (an.dmgFrom or 0) + ((an.dmgTo or 0) - (an.dmgFrom or 0)) * t
end

local function draw_combo(hud, v, f, an, now)
    if f.hit_seq ~= an.hitSeq then
        an.hitSeq = f.hit_seq
        if f.hud_hits >= 2 then
            if not an.comboOn then an.comboAt = now end
            an.comboOn, an.hits, an.popAt = true, f.hud_hits, now
        elseif an.comboOn then
            an.comboOn, an.comboOutAt = false, now
        end
    end
    if f.hud_damage >= 0 and f.hud_damage ~= an.dmgTo then
        local shown = shown_damage(an, now)
        an.dmgFrom = (an.comboOn and f.hud_damage > (an.dmgTo or 0)) and shown or 0 -- (as the game: a new combo counts from 0)
        an.dmgTo, an.dmgAt = f.hud_damage, now
        local diff = an.dmgTo - an.dmgFrom
        an.dmgFor = diff > 6000 and min(1.5, diff / 9500) or 0.6
    end
    if f.message_seq ~= an.msgSeq then
        if an.msgSeq ~= nil then
            an.msg = (f.message >= 0 and f.message < 5) and f.message or 0
            an.msgAt = now
        end
        an.msgSeq = f.message_seq
    end
    local inK = an.comboOn and clamp01((now - (an.comboAt or now)) / 0.15) or 1 - clamp01((now - (an.comboOutAt or -10)) / 0.25)
    local msgT = now - (an.msgAt or -10)
    local msgOn = an.msgAt ~= nil and msgT < 1.4
    if inK <= 0 and not msgOn then return end
    local x0, y0 = 214, 188
    local keep = alphaAll
    if inK > 0 then
        alphaAll = keep * inK
        local slide = (1 - inK) * -40 -- (slides in from its edge, and out back to it)
        local d0, d1 = rgba(5, 12, 32, 0.7), rgba(5, 12, 32, 0)
        plate(hud, { v.X(x0 + 14 + slide), v.Y(y0) }, { v.X(x0 + 236 + slide), v.Y(y0) }, { v.X(x0 + 222 + slide), v.Y(y0 + 86) },
              { v.X(x0 + slide), v.Y(y0 + 86) }, d0, d1, d1, d0)
        local cy = rgba(150, 235, 255, 0.95)
        plate(hud, { v.X(x0 + 14 + slide), v.Y(y0) }, { v.X(x0 + 20 + slide), v.Y(y0) }, { v.X(x0 + 6 + slide), v.Y(y0 + 86) },
              { v.X(x0 + slide), v.Y(y0 + 86) }, cy, cy, cy, cy)
        local pop = 1 + 0.22 * (1 - clamp01((now - (an.popAt or 0)) / 0.18))
        text_fx(hud, v, tostring(min(999, an.hits or 0)), x0 + 26 + slide, y0 + 54, 62 * U * pop, rgba(255, 246, 200, 1),
                rgba(255, 178, 30, 1), rgba(40, 14, 4, 0.95), 0.07)
        local word = (bt3.label("combo-hits/0/0") or "HITS"):upper()
        text_fx(hud, v, word, 8, y0 + 52, 22 * U, rgba(255, 255, 255, 1), rgba(255, 210, 122, 1), rgba(40, 14, 4, 0.95), nil, true)
        if (an.dmgTo or 0) > 0 then
            local dmg = min(99999, floor(shown_damage(an, now) + 0.5))
            local loc = bt3.locale()
            local dot = loc == "es" or loc == "pt"
            local t = dmg >= 1000 and string.format("%d%s%03d", dmg // 1000, dot and "." or ",", dmg % 1000) or tostring(dmg)
            text_fx(hud, v, t, x0 + 30 + slide, y0 + 80, 24 * U, rgba(255, 255, 255, 1), rgba(169, 226, 255, 1), rgba(4, 10, 28, 0.95))
            text_fx(hud, v, dot and "DAÑO" or "DAMAGE", 8, y0 + 79, 15 * U, rgba(207, 232, 255, 1), rgba(143, 184, 232, 1),
                    rgba(4, 10, 28, 0.95), nil, true)
        end
    end
    if msgOn then -- the event banner: in 0.1 s, held 1.2 s, out 0.1 s (as the game's)
        local a = msgT < 0.1 and msgT / 0.1 or msgT > 1.3 and (1.4 - msgT) / 0.1 or 1
        alphaAll = keep * clamp01(a)
        local slide = (1 - clamp01(msgT / 0.1)) * -30
        local by, bw = y0 + 98, 250
        local c = kMsgColour[an.msg or 0]
        plate(hud, { v.X(x0 + 10 + slide), v.Y(by) }, { v.X(x0 + bw + slide), v.Y(by) }, { v.X(x0 + bw - 10 + slide), v.Y(by + 34) },
              { v.X(x0 + slide), v.Y(by + 34) }, col(c, 1, 0.95), col(c, 1, 0), col(c, 1, 0), col(c, 1, 0.95))
        local msg = bt3.label(kMsgLabel[an.msg or 0]) or kMsgEnglish[an.msg or 0]
        text_fx(hud, v, msg, x0 + 18 + slide, by + 26, 25 * U, rgba(255, 255, 255, 1), rgba(255, 241, 200, 1), rgba(30, 8, 4, 0.9))
    end
    alphaAll = keep
end

local function draw_timer(hud, timer, now)
    if not timer then return end
    local cx, w, h, sl, top = W / 2, 132 * U, 62 * U, 14 * U, 6 * U
    local a, b, c, d = { cx - w / 2, top }, { cx + w / 2, top }, { cx + w / 2 - sl, top + h }, { cx - w / 2 + sl, top + h }
    local function dn(p) return { p[1], p[2] + 4 * U } end
    local sh = rgba(0, 0, 0, 0.35)
    plate(hud, dn(a), dn(b), dn(c), dn(d), sh, sh, sh, sh)
    plate(hud, a, b, c, d, rgba(30, 60, 118, 0.95), rgba(30, 60, 118, 0.95), rgba(7, 16, 38, 0.95), rgba(7, 16, 38, 0.95))
    hud.path({ a, b, c, d }, rgba(140, 190, 255, 0.7), 1.4 * U, true)
    hud.line(cx - w / 2 + sl + 6 * U, top + h + 5 * U, cx + w / 2 - sl - 6 * U, top + h + 5 * U, rgba(150, 235, 255, 0.9), 2.5 * U)
    if timer.clock and timer.value < 0 then -- no limit: a lemniscate, leaning as the numbers do
        local pts, r = {}, 34 * U
        for i = 0, 63 do
            local t = 2 * pi * i / 64
            local dd = 1 + sin(t) * sin(t)
            local x, y = r * cos(t) / dd, r * sin(t) * cos(t) / dd * 1.25
            pts[#pts + 1] = { cx + x - 0.16 * y, top + h * 0.5 + y }
        end
        hud.path(pts, rgba(4, 10, 28, 0.95), 12 * U, true)
        hud.path(pts, rgba(225, 245, 255, 1), 5 * U, true)
        return
    end
    local t = timer.value > 999 and 999 or timer.value < 0 and 0 or timer.value
    local low = timer.clock and t < 10
    local pulse = low and 0.75 + 0.25 * sin(now * 9) or 1
    hud.text(cx, 56 * U, tostring(t), low and rgba(255, 210, 200, pulse) or rgba(255, 255, 255, 1), 52 * U, "center", {
        bottom = low and rgba(255, 90, 60, pulse) or rgba(169, 226, 255, 1), outline = rgba(4, 10, 28, 0.95),
        outline_size = 52 * U * 0.08, lean = 0.16, baseline = true,
    })
end

local function draw_side(hud, side, right, f, an, now)
    local v = view(right)
    local hpMax = (f.hp_max or 0) > 0 and f.hp_max or 10000
    local hp = min(max(f.hp or 0, 0), hpMax)
    local bars = min(max((hpMax + 9999) // 10000, 1), 7)
    local left = hp > 0 and min((hp + 9999) // 10000, 7) or 0
    local dt = an.frameAt and (now - an.frameAt) or 0
    an.frameAt = now
    local key = (f.active << 16) | ((f.chara or 0) + 1)
    if key ~= an.key then
        an.key, an.trail, an.lastHp, an.ki, an.blast = key, hp, hp, f.ki, f.blast
        an.kiTrail, an.lastKi = f.ki, f.ki
        an.mp = (f.max_power or 0) / 30000
    end
    if hp < an.lastHp then an.hitAt = now end -- (each hit of a combo keeps the white standing while it goes on)
    an.lastHp = hp
    if hp < an.trail - 0.5 then
        if now - (an.hitAt or 0) > 0.45 then -- then it runs down to the health
            an.trail = max(an.trail - max(2500, (an.trail - hp) * 3) * dt, hp)
        end
    else
        an.trail = hp
    end
    an.ki = an.ki + (f.ki - an.ki) * clamp01(dt * 14)
    -- ki spent: as health, the part just lost stands in red, then runs down to the ki. It stands as long as ki keeps
    -- going: a charged attack spends it in steps, and a shorter hold ran it down between them, during the charge
    if f.ki < an.lastKi then an.kiAt = now end
    an.lastKi = f.ki
    if f.ki < an.kiTrail - 0.5 then
        if now - (an.kiAt or 0) > 0.9 then
            an.kiTrail = max(an.kiTrail - max(15000, (an.kiTrail - f.ki) * 3) * dt, f.ki)
        end
    else
        an.kiTrail = f.ki
    end
    an.blast = an.blast + (f.blast - an.blast) * clamp01(dt * 10)
    an.mp = an.mp + (clamp01((f.max_power or 0) / 30000) - an.mp) * clamp01(dt * 14)

    -- the corner's shade, so that the parts read over a bright sky
    hud.glow(v.X(80), v.Y(40), 620 * U, rgba(5, 12, 32, 0.55), rgba(5, 12, 32, 0.22))

    -- the fighter (unless the player turned the portraits off)
    if bt3.setting("portraits") then
        local wx0, wx1 = v.X(0), v.X(230)
        hud.portrait(side, v.X(kFaceX), v.Y(kFaceY), kArtW * U, 0, {
            face = true, flip = right, tint = rgba(255, 255, 255, 1),
            window = { x0 = min(wx0, wx1), y0 = v.Y(0), x1 = max(wx0, wx1), y1 = v.Y(210) },
            fade = { x = v.X(kFaceX + 8), y = v.Y(kFaceY - 10), r0 = 52 * U, r1 = 116 * U },
        })
    end

    -- blast stock: the number under the fighter, the blade round it lit as the next stock fills
    local stock = an.blast / 100000
    local count, most = floor(stock + 0.001), (f.blast_max or 0) // 100000
    local part = (count >= most and most > 0) and 1 or stock - count
    local top, bot = blade(hud, v, 1, function() return rgba(6, 16, 40, 0.85) end)
    outline(hud, top, bot, rgba(OUTLINE[1], OUTLINE[2], OUTLINE[3], OUTLINE[4]), 1 * U)
    if part > 0.01 then
        blade(hud, v, part, function(t) return mix(rgba(70, 170, 255, 1), rgba(215, 250, 255, 1), t) end)
    end
    number(hud, v, tostring(min(max(count, 0), 99)), 58, 258, 66 * U)

    -- health shakes for a moment after a hit
    local shake = (now - (an.hitAt or -10)) < 0.25 and (1 - (now - an.hitAt) / 0.25) * 3 or 0
    local w = v
    if shake > 0 then
        w = view(right, cos(now * 70) * shake * U * 0.4 * (right and -1 or 1), sin(now * 90) * shake * U * 0.6)
    end

    -- the reserve bars: one pip per bar after the one being spent, each in its bar's colour
    for i = 0, bars - 2 do
        local x, n = kPipX + i * kPipStep, i + 1
        if n < left then
            local t, b = strip(hud, w, x, x + kPipW, 4, 8, 8, 4, 4, col(kBar[n][2], 1, 1), col(kBar[n][1], 1, 1))
            outline(hud, t, b, rgba(OUTLINE[1], OUTLINE[2], OUTLINE[3], OUTLINE[4]), 1 * U)
        else
            local t, b = strip(hud, w, x, x + kPipW, 4, 8, 8, 4, 4, rgba(8, 18, 40, 0.85), rgba(8, 18, 40, 0.85))
            outline(hud, t, b, rgba(OUTLINE[1], OUTLINE[2], OUTLINE[3], OUTLINE[4]), 1 * U)
        end
    end

    -- the health bar: the bar being spent over the next one's colour, the damage just taken in red
    local x0, x1 = kBarX0, kBarX1
    local len = x1 - x0
    local function thick(x) return 16 - 6 * (x - x0) / len end
    local t, b = strip(hud, w, x0 - 3, x1 + 3, 15, 22, 16, 10, 14, rgba(4, 10, 26, 0.9), rgba(4, 10, 26, 0.9))
    outline(hud, t, b, rgba(OUTLINE[1], OUTLINE[2], OUTLINE[3], OUTLINE[4]), 1.2 * U)
    local barPart = left > 0 and clamp01((hp - (left - 1) * 10000) / 10000) or 0
    if left > 1 then
        strip(hud, w, x0, x1, 18, thick(x0), thick(x1), 8, 12, col(kBar[left - 1][1], 0.72, 1), col(kBar[left - 1][2], 0.8, 1), true, x0, x1)
    end
    local trailPart = left > 0 and clamp01((an.trail - (left - 1) * 10000) / 10000) or 0
    if trailPart > barPart + 0.002 then
        local xe = x0 + len * trailPart
        strip(hud, w, x0, xe, 18, thick(x0), thick(xe), 8, 8, rgba(LOST[1], LOST[2], LOST[3], LOST[4]),
              rgba(LOST[1], LOST[2], LOST[3], LOST[4]))
    end
    if barPart > 0.002 and left > 0 then
        local xe = x0 + len * barPart
        strip(hud, w, x0, xe, 18, thick(x0), thick(xe), 8, 8, col(kBar[left][1], 1, 1), col(kBar[left][2], 1, 1), true, x0, x1)
        -- the shine along its top: its ends on the bar's own slanted ends (the bar's top runs 8 further than its
        -- bottom over its thickness, so a band s deep runs 8 * s / thickness further)
        local s0, s1 = 5, 4
        local c0, c1 = 8 * s0 / thick(x0), 8 * s1 / thick(xe)
        strip(hud, w, x0 + 8 - c0, xe + 8 - c1, 18, s0, s1, c0, c1, rgba(255, 255, 255, 0.35), rgba(255, 255, 255, 0.35))
    end

    -- ki: one cell per bar, close together, along the same stroke. Then the max power gauge (max_power, 0..30000):
    -- charging on with full ki fills it, shown as blue spreading over the full cells; when it is full the
    -- powered-up mode begins and the same gauge is its time, running down in blue (the game's own HUD shows it in
    -- the ki bars too, 6000 a bar).
    local kiBars = (f.ki_max or 0) > 0 and (f.ki_max + 19999) // 20000 or 5
    kiBars = min(max(kiBars, 1), 8)
    local ki, mpCells = an.ki / 20000, an.mp * kiBars
    local mp = f.powered_up and true or false
    local pulse = mp and 0.85 + 0.15 * sin(now * 8) or 1
    local charge = (not mp and an.mp > 0.004) and 0.8 + 0.2 * sin(now * 14) or 1 -- (a quicker flicker while it fills)
    local m0, m1 = kMp[1], kMp[2]
    local k0, k1 = kKi[false][1], kKi[false][2]
    if mp then k0, k1 = m0, m1 end
    for i = 0, kiBars - 1 do
        local x = kKiX + i * kKiStep
        local fill = mp and clamp01(mpCells - i) or clamp01(ki - i)
        local g = mp and 0 or clamp01(mpCells - i) -- (the max power filling, over the ki)
        local on = fill >= 0.999
        local tt, bb = strip(hud, w, x, x + kKiW, 44, 12, 12, 6, 6, rgba(6, 14, 34, 0.85), rgba(6, 14, 34, 0.85))
        outline(hud, tt, bb, rgba(OUTLINE[1], OUTLINE[2], OUTLINE[3], OUTLINE[4]), ((on or g > 0) and 2 or 1.2) * U)
        local lost = mp and 0 or clamp01(an.kiTrail / 20000 - i) -- (the ki just spent, under what is left)
        if lost > fill + 0.01 then
            strip(hud, w, x + 2.5, x + 2.5 + (kKiW - 5) * lost, 46.5, 7, 7, 5, 5, rgba(LOST[1], LOST[2], LOST[3], LOST[4]),
                  rgba(LOST[1], LOST[2], LOST[3], LOST[4]))
        end
        if fill > 0.01 then
            strip(hud, w, x + 2.5, x + 2.5 + (kKiW - 5) * fill, 46.5, 7, 7, 5, 5, col(k0, pulse, 1), col(k1, pulse, 1))
        end
        if g > 0.01 then
            strip(hud, w, x + 2.5, x + 2.5 + (kKiW - 5) * g, 46.5, 7, 7, 5, 5, col(m0, charge, 1), col(m1, charge, 1))
        end
    end

    draw_stats(hud, v, f, an, dt)
    draw_combo(hud, v, f, an, now)
end

return {
    name = "Modern HUD",
    version = "1.0",
    author = "BT4decomp",
    description = "The fight's gauges, faces, timer and combo counter drawn anew at the screen's resolution: "
        .. "portraits, curved health bars, ki cells, blast blades, status chips and a combo plate.",
    settings = {
        { id = "opacity", label = "Opacity", type = "slider", min = 30, max = 100, step = 5, default = 100 },
        { id = "timer", label = "Draw the timer", type = "toggle", default = true,
          tip = "Off: the game's own timer stays." },
        { id = "combo", label = "Draw the combo counter", type = "toggle", default = true,
          tip = "Off: the game's own counter and messages stay." },
        { id = "portraits", label = "Show portraits", type = "toggle", default = true,
          tip = "The fighters' portraits in the top corners." },
    },

    on_hud = function(hud, game)
        local now = game.time
        local on = game.battle and game.hud_shown
        if on and not wasOn then
            enterAt = now
            anim = { {}, {} }
        end
        if not on and wasOn then leaveAt = now end
        wasOn = on
        local fade
        if on then
            last = game
            fade = clamp01((now - enterAt) / 0.25)
        elseif now - leaveAt < 0.2 and last then
            game = last -- (fading out with the last fight read)
            fade = 1 - (now - leaveAt) / 0.2
        else
            return
        end
        alphaAll = fade * bt3.setting("opacity") / 100
        U, W = hud.h / 940, hud.w
        local drawTimer, drawCombo = bt3.setting("timer"), bt3.setting("combo")
        if on then
            hud.hide_game("gauges")
            if drawTimer then hud.hide_game("timer") end
            if drawCombo then hud.hide_game("combo") end
        end
        local swap = game.swapped and 1 or 0
        for place = 0, 1 do
            local side = (place ~ swap) + 1
            local f = game.fighters[side]
            if f then
                if not drawCombo then
                    f = setmetatable({ hud_hits = 0, hud_damage = -1, hit_seq = anim[side].hitSeq, message_seq = anim[side].msgSeq }, { __index = f })
                end
                draw_side(hud, side, place == 1, f, anim[side], now)
            end
        end
        if drawTimer then draw_timer(hud, game.timer, now) end
        lastTime = now
    end,
}
