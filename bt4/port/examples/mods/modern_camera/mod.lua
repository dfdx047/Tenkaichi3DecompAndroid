-- Modern camera: the fight camera as it is shown, dressed the way a camera operator would film it.
--   - inertia: the shown pose follows the game's with a short lag;
--   - tilt: a slight roll into fast turns;
--   - reactions: a short shake and push in when this side's fighter is hit, a smaller one when it lands a hit;
--   - distance and height: the player's own framing, the camera re-aimed so the fighter stays where it was in it.
-- Technique and transformation cuts, fixed and frozen cameras are left exactly as the game has them; a jump of the
-- game's camera (a teleport, the start of a round) snaps the shown one to it.
-- (The BT4decomp port's "Camera" setting, written as a script mod: docs/port/mods.md.)

local pi = math.pi
local sin, cos, sqrt, abs = math.sin, math.cos, math.sqrt, math.abs
local function clamp(v, a, b) return v < a and a or v > b and b or v end

local function wrap(a)
    while a > pi do a = a - 2 * pi end
    while a < -pi do a = a + 2 * pi end
    return a
end

-- The view's rotation as the game builds it from the Euler angles (src/sys/vu0_b_c.c Vu0View_SetRotTrans):
-- R = Ry Rz Rx (rows); a world point p is seen at (p - pos) R; column 2 of R is where the camera looks.
local function rotation(r)
    local sx, cx, sz, cz, sy, cy = sin(r[1]), cos(r[1]), sin(r[3]), cos(r[3]), sin(r[2]), cos(r[2])
    local Rx = { { 1, 0, 0 }, { 0, cx, -sx }, { 0, sx, cx } }
    local Rz = { { cz, -sz, 0 }, { sz, cz, 0 }, { 0, 0, 1 } }
    local Ry = { { cy, 0, sy }, { 0, 1, 0 }, { -sy, 0, cy } }
    local T, R = {}, {}
    for i = 1, 3 do
        T[i] = {}
        for j = 1, 3 do
            T[i][j] = Rz[i][1] * Rx[1][j] + Rz[i][2] * Rx[2][j] + Rz[i][3] * Rx[3][j]
        end
    end
    for i = 1, 3 do
        R[i] = {}
        for j = 1, 3 do
            R[i][j] = Ry[i][1] * T[1][j] + Ry[i][2] * T[2][j] + Ry[i][3] * T[3][j]
        end
    end
    return R
end

-- where a world point appears from the camera: its direction in view space, of length 1
local function view_dir(pos, rot, p)
    local R = rotation(rot)
    local d = { p[1] - pos[1], p[2] - pos[2], p[3] - pos[3] }
    local o = {}
    for j = 1, 3 do o[j] = d[1] * R[1][j] + d[2] * R[2][j] + d[3] * R[3][j] end
    local l = sqrt(o[1] * o[1] + o[2] * o[2] + o[3] * o[3])
    if l > 0 then o[1], o[2], o[3] = o[1] / l, o[2] / l, o[3] / l end
    return o
end

-- turns the pitch so that the point p is seen at the height `want` again (a few Newton steps)
local function reaim(pos, rot, p, want)
    for _ = 1, 4 do
        local d0 = view_dir(pos, rot, p)
        local d1 = view_dir(pos, { rot[1] + 0.002, rot[2], rot[3] }, p)
        local slope = (d1[2] - d0[2]) / 0.002
        if abs(slope) < 1e-4 then return end
        rot[1] = rot[1] - (d0[2] - want) / slope
    end
end

-- a smooth wobble for the shake (not a random number)
local function wobble(t, seed)
    return sin(t * 47 + seed) * 0.6 + sin(t * 83 + seed * 2.3) * 0.4
end

local sides = { {}, {} }

local function reset(c) c.have = false end

local function hp_of(game, side)
    local f = game.fighters and game.fighters[side]
    return f and f.hp or -1
end

local function v3(t) return { t.x, t.y, t.z } end

return {
    name = "Modern camera",
    version = "1.1",
    author = "BT4decomp",
    description = "Inertia, a tilt into turns, a shake when a hit lands, and your own distance and height. "
        .. "Technique cuts and fixed cameras stay the game's.",
    settings = {
        { id = "lag", label = "Inertia", type = "slider", min = 0, max = 100, step = 1, default = 50,
          tip = "0: the game's camera exactly. 100: about a third of a second to settle." },
        { id = "tilt", label = "Tilt in turns", type = "slider", min = 0, max = 100, step = 1, default = 50 },
        { id = "react", label = "Hit reactions", type = "slider", min = 0, max = 100, step = 1, default = 60,
          tip = "The shake and push in when a hit is taken or landed." },
        { id = "distance", label = "Distance", type = "slider", min = -50, max = 50, step = 1, default = 0,
          tip = "Farther (+) or nearer (-) than the game's camera." },
        { id = "height", label = "Height", type = "slider", min = -50, max = 50, step = 1, default = 0,
          tip = "Higher (+) or lower (-) than the game's camera, the fighter kept where it was in the picture." },
        { id = "zoom", label = "Zoom", type = "slider", min = 70, max = 100, step = 1, default = 100,
          tip = "100: the game's picture. Lower: a wider picture (cam.zoom)." },
    },

    on_frame = function(game)
        if not game.battle then sides = { {}, {} } end
    end,

    on_camera = function(cam, game)
        local c = sides[cam.side]
        if cam.cut then
            reset(c)
            return
        end
        local lag, tilt, react = bt3.setting("lag"), bt3.setting("tilt"), bt3.setting("react")
        local dist_set, height_set = bt3.setting("distance"), bt3.setting("height")
        local pos, rot, target = v3(cam.pos), v3(cam.rot), v3(cam.target)
        -- Follow translation immediately. Smoothing absolute world positions made
        -- the camera trail behind a moving fighter and continually change distance.
        -- Only the offset around the moving target should have inertia.
        if c.have and c.target then
            for i = 1, 3 do
                local shift = target[i] - c.target[i]
                c.pos[i] = c.pos[i] + shift
                c.gamePos[i] = c.gamePos[i] + shift
            end
        end
        c.target = { target[1], target[2], target[3] }
        c.clock = (c.clock or 0) + 1

        -- the reactions: health fell on this side (hit), or on the other (a hit landed)
        local hp, foe = hp_of(game, cam.side), hp_of(game, 3 - cam.side)
        if c.have and hp >= 0 and (c.hp or -1) >= 0 and hp < c.hp then
            local dmg = c.hp - hp
            c.shake, c.shakeAmp = 1, math.min(1, 0.35 + dmg / 4000)
            c.push, c.pushAmp = 1, math.min(1, 0.4 + dmg / 5000)
        elseif c.have and foe >= 0 and (c.foeHp or -1) >= 0 and foe < c.foeHp then
            local dmg = c.foeHp - foe
            if (c.shake or 0) < 0.5 then
                c.shake, c.shakeAmp = 1, math.min(0.5, 0.15 + dmg / 8000)
            end
            c.push, c.pushAmp = 1, math.min(0.6, 0.25 + dmg / 8000)
        end
        c.hp, c.foeHp = hp, foe

        -- the player's framing: along the line from the target, higher or lower (+Y is down), re-aimed
        local d = { pos[1] - target[1], pos[2] - target[2], pos[3] - target[3] }
        local dist = sqrt(d[1] * d[1] + d[2] * d[2] + d[3] * d[3])
        local want = {}
        do
            local before = view_dir(pos, rot, target)
            local wrot = { rot[1], rot[2], rot[3] }
            local scale = 1 + dist_set / 100
            for i = 1, 3 do want[i] = target[i] + d[i] * scale end
            want[2] = want[2] - dist * (height_set / 100) * 0.6
            if dist_set ~= 0 or height_set ~= 0 then reaim(want, wrot, target, before[2]) end
            rot[1] = wrot[1]
        end

        -- a jump of the game's camera: the shown one goes with it
        if c.have then
            local j = { want[1] - c.gamePos[1], want[2] - c.gamePos[2], want[3] - c.gamePos[3] }
            if sqrt(j[1] * j[1] + j[2] * j[2] + j[3] * j[3]) > dist * 0.9 + 1 or abs(wrap(rot[2] - c.gameYaw)) > 0.9 then
                c.have = false
            end
        end
        do
            local yawRate = c.have and wrap(rot[2] - c.gameYaw) or 0
            c.gamePos = { want[1], want[2], want[3] }
            c.gameYaw = rot[2]
            local roll = clamp(-yawRate * 1.6, -0.07, 0.07) * (tilt / 50)
            c.roll = (c.roll or 0) + (roll - (c.roll or 0)) * 0.12
        end
        if not c.have then
            c.pos = { want[1], want[2], want[3] }
            c.rot = { rot[1], rot[2], rot[3] }
            c.have = true
        else
            -- inertia: a share of the way each frame (lag 0: none; 100: about a third of a second to settle)
            local k = 1 - lag / 100 * 0.82
            local kr = math.min(1, k * 1.25)
            for i = 1, 3 do
                c.pos[i] = c.pos[i] + (want[i] - c.pos[i]) * k
                c.rot[i] = wrap(c.rot[i] + wrap(rot[i] - c.rot[i]) * kr)
            end
        end

        -- the shown pose: the followed one, pushed in toward the target and shaken by a hit, tilted
        local push, s = 0, 0
        if (c.push or 0) > 0 then
            push = c.pushAmp * c.push * c.push * 0.08 * (react / 60)
            c.push = math.max(0, c.push - 1 / 8)
        end
        if (c.shake or 0) > 0 then
            s = c.shakeAmp * c.shake * (react / 60)
            c.shake = math.max(0, c.shake - 1 / 9)
        end
        local out, orot = {}, { c.rot[1], c.rot[2], c.rot[3] }
        for i = 1, 3 do out[i] = c.pos[i] + (target[i] - c.pos[i]) * push end
        orot[3] = wrap(orot[3] + c.roll)
        if s > 0 then -- along the camera's own right and up, a little turn too
            local t = c.clock / 30
            local a, b = wobble(t, 1) * s * dist * 0.012, wobble(t, 4) * s * dist * 0.012
            local R = rotation(orot)
            for i = 1, 3 do out[i] = out[i] + R[i][1] * a + R[i][2] * b end
            orot[3] = wrap(orot[3] + wobble(t, 7) * s * 0.012)
        end
        cam.pos.x, cam.pos.y, cam.pos.z = out[1], out[2], out[3]
        cam.rot.x, cam.rot.y, cam.rot.z = orot[1], orot[2], orot[3]
        cam.zoom = bt3.setting("zoom") / 100
    end,
}
