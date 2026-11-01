-- Shoulder camera: the fight camera moved over the fighter's shoulder and widened, as later games frame it.
-- The pose is moved in the camera's own directions (side, height, back) and the picture zoomed out (cam.zoom);
-- the engine keeps the moved camera out of the stage (cam.collide, on unless a mod turns it off).
-- Technique and transformation cuts, fixed and frozen cameras stay the game's.

local sin, cos = math.sin, math.cos

-- the view's rotation as the game builds it from the Euler angles (src/sys/vu0_b_c.c Vu0View_SetRotTrans):
-- R = Ry Rz Rx (rows); column 1 of R is the camera's right, column 2 its down, column 3 where it looks
local function axes(r)
    local sx, cx, sz, cz, sy, cy = sin(r.x), cos(r.x), sin(r.z), cos(r.z), sin(r.y), cos(r.y)
    local Rx = { { 1, 0, 0 }, { 0, cx, -sx }, { 0, sx, cx } }
    local Rz = { { cz, -sz, 0 }, { sz, cz, 0 }, { 0, 0, 1 } }
    local Ry = { { cy, 0, sy }, { 0, 1, 0 }, { -sy, 0, cy } }
    local T, R = {}, {}
    for i = 1, 3 do
        T[i] = {}
        for j = 1, 3 do T[i][j] = Rz[i][1] * Rx[1][j] + Rz[i][2] * Rx[2][j] + Rz[i][3] * Rx[3][j] end
    end
    for i = 1, 3 do
        R[i] = {}
        for j = 1, 3 do R[i][j] = Ry[i][1] * T[1][j] + Ry[i][2] * T[2][j] + Ry[i][3] * T[3][j] end
    end
    return R
end

return {
    name = "Shoulder camera",
    version = "1.0",
    description = "The camera over the fighter's shoulder and a wider picture. Kept out of the stage.",
    settings = {
        { id = "side", label = "Sideways", type = "slider", min = -10, max = 10, step = 0.5, default = -6,
          tip = "Negative: to the left of the fighter." },
        { id = "height", label = "Height", type = "slider", min = -5, max = 5, step = 0.5, default = -1.5 },
        { id = "back", label = "Back", type = "slider", min = -5, max = 10, step = 0.5, default = 2 },
        { id = "zoom", label = "Zoom", type = "slider", min = 70, max = 100, step = 1, default = 88,
          tip = "100: the game's. Lower: wider." },
    },

    on_camera = function(cam, game)
        if cam.cut then return end
        local R = axes(cam.rot)
        local side, up, back = bt3.setting("side"), bt3.setting("height"), bt3.setting("back")
        local p = { cam.pos.x, cam.pos.y, cam.pos.z }
        for i = 1, 3 do
            -- right along column 1, down along column 2 (so up is minus), forward along column 3 (back is minus)
            p[i] = p[i] + side * R[i][1] - up * R[i][2] - back * R[i][3]
        end
        cam.pos.x, cam.pos.y, cam.pos.z = p[1], p[2], p[3]
        cam.zoom = bt3.setting("zoom") / 100
    end,
}
