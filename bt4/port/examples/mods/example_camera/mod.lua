-- An example script mod (docs/port/mods.md): the fight camera follows the game's with a little lag, as a camera
-- operator would, and can be set farther or nearer. Technique and transformation cuts are left as the game has
-- them. Copy this folder into the game's mods/ folder to try it.

local shown = {}  -- per side: the pose shown last frame

local function lerp(a, b, k) return a + (b - a) * k end

-- the shortest turn from angle a to angle b
local function turn(a, b)
    local d = (b - a) % (2 * math.pi)
    if d > math.pi then d = d - 2 * math.pi end
    return d
end

return {
    name = "Example: smooth camera",
    version = "1.0",
    author = "Tenkaichi3Decomp",
    description = "The fight camera follows with a little lag, and its distance can be changed.",
    settings = {
        { id = "lag", label = "Lag", type = "slider", min = 0, max = 90, step = 5, default = 40,
          tip = "0: the game's camera exactly. More: the camera takes longer to catch up." },
        { id = "distance", label = "Distance", type = "slider", min = -40, max = 60, step = 5, default = 0,
          tip = "Percent farther (or nearer) from the point the camera looks at." },
    },

    on_camera = function(cam, game)
        local s = shown[cam.side]
        if cam.cut then
            shown[cam.side] = nil -- (after the cut, start again from the game's pose)
            return
        end
        -- the wanted pose: the game's, moved along the line from the point it looks at
        local scale = 1 + bt3.setting("distance") / 100
        local want = {
            x = cam.target.x + (cam.pos.x - cam.target.x) * scale,
            y = cam.target.y + (cam.pos.y - cam.target.y) * scale,
            z = cam.target.z + (cam.pos.z - cam.target.z) * scale,
        }
        local k = 1 - bt3.setting("lag") / 100
        -- a jump of the game's camera (a teleport, a new round): go with it
        if s and math.abs(turn(s.rot.y, cam.rot.y)) < 0.9 then
            s.pos.x, s.pos.y, s.pos.z = lerp(s.pos.x, want.x, k), lerp(s.pos.y, want.y, k), lerp(s.pos.z, want.z, k)
            s.rot.x = s.rot.x + turn(s.rot.x, cam.rot.x) * k
            s.rot.y = s.rot.y + turn(s.rot.y, cam.rot.y) * k
            s.rot.z = s.rot.z + turn(s.rot.z, cam.rot.z) * k
        else
            s = { pos = want, rot = { x = cam.rot.x, y = cam.rot.y, z = cam.rot.z } }
            shown[cam.side] = s
        end
        cam.pos.x, cam.pos.y, cam.pos.z = s.pos.x, s.pos.y, s.pos.z
        cam.rot.x, cam.rot.y, cam.rot.z = s.rot.x, s.rot.y, s.rot.z
    end,
}
