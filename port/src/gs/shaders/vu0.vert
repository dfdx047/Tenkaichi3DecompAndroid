#version 450
// Vertex program 0 of the game (128 instructions; the fighters' models), as a shader.
// What the original does per vertex (read from its disassembly, docs/port/README.md):
//   position  two-bone skinning: p = mix(B * (pos - pivotB), A * (pos - pivotA), weight), weight = pos.w
//   screen    s = C * p; the GS gets s.xyz / s.w as 12.4 fixed point (x, y) and an integer depth
//   layer 0   texture coordinates from the vertex, the material colour
//   layer 1   the "toon" layer: u = 0.5 + 0.5 * (light . normal'), v = 0, with the second colour
//   reject    a triangle with a vertex outside the guard volume is dropped (the GPU clips instead)
// Here the position leaves in clip space, so the GPU interpolates with perspective and clips; x and y are mapped
// like gs.vert maps GS pixels, depth is the GS depth over its maximum.
layout(location = 0) in vec4 inPos;    // xyz, weight
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inSt;     // s, t, 1
layout(location = 0) out vec4 vColor;
layout(location = 1) out vec3 vStq;
layout(set = 1, binding = 0) uniform U {
    mat4 boneA, boneB;   // VU memory 0..3, 4..7
    vec4 pivotA, pivotB; // 8, 9
    mat4 screen;         // 14..17
    vec4 light;          // xyz: the x components of 10..12, w: of 13
    vec4 color0, color1; // 22, 23 (0..255)
    vec4 misc;           // x, y: XYOFFSET in pixels, z: depth maximum, w: layer (0 or 1; 2 = programs 2a / 2b; 3 = program 8; 4 = program 7; 5 = program 1 layer 1)
    vec4 light2;         // program 1: the y components of 10..13
} u;
void main() {
    float w = u.misc.w == 4.0 ? 1.0 : inPos.w; // layer 4 (debris, rigid): the 4th word is a flag, not a weight
    vec3 pa = (u.boneA * vec4(inPos.xyz - u.pivotA.xyz, 1.0)).xyz;
    vec3 pb = (u.boneB * vec4(inPos.xyz - u.pivotB.xyz, 1.0)).xyz;
    vec4 s = u.screen * vec4(mix(pb, pa, w), 1.0);
    // Programs 2a / 2b: the shadow camera's matrix is orthographic with a NEGATIVE constant w (measured: -862).
    // The PS2 only divides by it; a GPU would clip everything as "behind the eye". The same point with w > 0:
    if (u.misc.w == 2.0 && s.w < 0.0) s = -s;
    gl_Position = vec4((s.x - u.misc.x * s.w) / 512.0 - s.w, s.w - (s.y - u.misc.y * s.w) / 512.0, s.z / abs(u.misc.z), s.w);
    // The vertex program hands the GS a whole-number depth (it converts s.z / s.w to an integer), and the GS
    // compares whole numbers at every pixel (gs.frag). Here: that whole number plus a half, so that what is
    // interpolated between vertices never sits on the edge between two whole numbers.
    if (s.w > 0.0 && u.misc.z > 0.0) gl_Position.z = (floor(s.z / s.w) + 0.5) / u.misc.z * s.w; // (misc.z < 0: BT3_DEPTH_FRAC, depth with its fractions as before)
    if (u.misc.w == 2.0) {
        // programs 2a / 2b (flat-colour model for the shadow): their "screen" matrix is the shadow camera's, whose
        // depth is not kept inside the depth range; the PS2 just stores whatever integer comes out. Keep every
        // triangle: depth clamped into range instead of clipped.
        gl_Position.z = clamp(s.z / abs(u.misc.z), 0.0, s.w);
    }
    if (u.misc.w == 5.0) {
        // program 1, layer 1: the model's second texture looked up by the direction of the normal in a
        // camera-aligned frame (one matrix, not blended between the bones): (u, v) = 0.5 + 0.5 * (M * (n, 1)).xy
        vec4 n1 = vec4(inNormal.xyz, 1.0);
        vColor = u.color1 / 255.0;
        vStq = vec3(dot(u.light, n1) * 0.5 + 0.5, dot(u.light2, n1) * 0.5 + 0.5, 1.0);
    } else if (u.misc.w >= 3.0) {
        // programs 8 and 7: the colour of each vertex (0..255 floats in the second quadword); 7 takes the mesh's alpha
        vColor = clamp(floor(vec4(inNormal.rgb, u.misc.w == 4.0 ? u.color0.a : inNormal.a)), 0.0, 255.0) / 255.0;
        vStq = vec3(inSt.xy, 1.0);
    } else if (u.misc.w != 1.0) {
        vColor = u.color0 / 255.0;
        vStq = vec3(inSt.xy, 1.0);
    } else {
        float na = dot(u.light.xyz, mat3(u.boneA) * inNormal.xyz), nb = dot(u.light.xyz, mat3(u.boneB) * inNormal.xyz);
        vColor = u.color1 / 255.0;
        vStq = vec3((mix(nb, na, w) + u.light.w) * 0.5 + 0.5, 0.0, 1.0);
    }
}
