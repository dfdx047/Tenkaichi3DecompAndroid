#version 450
// Vertex program 6 of the game (451 instructions; the ground shadow of a fighter), as a shader. Listing and notes:
// decomp src/vu1/prog6.vsm, docs/systems/vu1/prog6.md. It draws the stage triangles under a fighter, textured
// with the silhouette that program 2b drew into the shadow page. Per vertex the original does:
//   screen = M * (x, y, z, 1), divided by w, to 12.4 fixed point (the batch's w holds a "no draw" flag instead)
//   c = SHADOWCAM * (x, y, z, 1);  s, t = (c.xy * scale + 1) / 2;  the GS gets (s, t, 1) / w
//   colour = the vertex colour as integers
// Most of the program is the same triangle clipper as program 4's; the GPU clips by itself. The "no draw" flags
// are handled where the strip is turned into triangles (GsGpu_DrawVu6).
// The uniform block is the one of vu0.vert, with other contents in three places.
layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inColor; // 0..255 as floats
layout(location = 2) in vec4 inUnused;
layout(location = 0) out vec4 vColor;
layout(location = 1) out vec3 vStq;
layout(set = 1, binding = 0) uniform U {
    mat4 shadowCam, boneB; // VU memory 0x0C..0x0F
    vec4 scale, texScale;  // x: VU memory 0x10; xy: size of the texture over the size of the buffer it lives in
    mat4 screen;           // VU memory 0..3
    vec4 light;
    vec4 color0, color1;
    vec4 misc;             // x, y: XYOFFSET in pixels, z: depth maximum
    vec4 light2;
    vec4 target;           // x: GS pixels per texel of the target (gs_draw.c target_lowres; 0 = 1)
} u;
void main() {
    vec4 p = vec4(inPos.xyz, 1.0);
    vec4 s = u.screen * p;
    vec2 st = ((u.shadowCam * p).xy * u.scale.x + 1.0) * 0.5;
    float tx = u.target.x > 0.0 ? 512.0 * u.target.x : 512.0; // (a work buffer at the PS2's resolution: its corner)
    gl_Position = vec4((s.x - u.misc.x * s.w) / tx - s.w, s.w - (s.y - u.misc.y * s.w) / tx, s.z / abs(u.misc.z), s.w);
    // The vertex program hands the GS a whole-number depth (it converts s.z / s.w to an integer), and the GS
    // compares whole numbers at every pixel (gs.frag). Here: that whole number plus a half, so that what is
    // interpolated between vertices never sits on the edge between two whole numbers.
    if (s.w > 0.0 && u.misc.z > 0.0) gl_Position.z = (floor(s.z / s.w) + 0.5) / u.misc.z * s.w; // (misc.z < 0: BT3_DEPTH_FRAC, depth with its fractions as before)
    vColor = clamp(floor(inColor), 0.0, 255.0) / 255.0;
    vStq = vec3(st * u.texScale.xy, 1.0);
}
