#version 450
// Vertex program 4 of the game (402 instructions; the stage and other static geometry), as a shader.
// What the original does per vertex of a strip (its main loop, instructions 46..66 of the disassembly):
//   screen = M * position, divided by w, to 12.4 fixed point; colour = the vertex colour as integers;
//   texture coordinates (s, t, 1) times 1/w.
// Three quarters of the program is a triangle clipper for strips that cross the edge of the guard volume: it
// rebuilds such triangles as fans. The GPU clips by itself, so none of that is needed here (and the interpreter's
// run of that clipper drew wrong triangles across the screen when the camera was close to a cliff).
// The uniform block is the one of vu0.vert; only `screen` and `misc` are used.
layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inColor; // 0..255 as floats
layout(location = 2) in vec4 inSt;
layout(location = 0) out vec4 vColor;
layout(location = 1) out vec3 vStq;
layout(set = 1, binding = 0) uniform U {
    mat4 boneA, boneB;
    vec4 pivotA, pivotB;
    mat4 screen;         // VU memory 0..3
    vec4 light;
    vec4 color0, color1;
    vec4 misc;           // x, y: XYOFFSET in pixels, z: depth maximum
} u;
void main() {
    vec4 s = u.screen * inPos;
    gl_Position = vec4((s.x - u.misc.x * s.w) / 512.0 - s.w, s.w - (s.y - u.misc.y * s.w) / 512.0, s.z / abs(u.misc.z), s.w);
    // The vertex program hands the GS a whole-number depth (it converts s.z / s.w to an integer), and the GS
    // compares whole numbers at every pixel (gs.frag). Here: that whole number plus a half, so that what is
    // interpolated between vertices never sits on the edge between two whole numbers.
    if (s.w > 0.0 && u.misc.z > 0.0) gl_Position.z = (floor(s.z / s.w) + 0.5) / u.misc.z * s.w; // (misc.z < 0: BT3_DEPTH_FRAC, depth with its fractions as before)
    vColor = clamp(floor(inColor), 0.0, 255.0) / 255.0;
    vStq = vec3(inSt.xy, 1.0);
}
