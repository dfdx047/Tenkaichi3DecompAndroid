#version 450
// Native version of the game's "draw a byte of the depth page through a table" pass (GfxPost_DrawDepthClut,
// src/sys/gfxm_b.c), the building block of its depth effects: depth tint, the alpha that feeds the glare and the
// object glow. On the PS2 the top byte of each depth word is spare; the game puts one of two things there and
// then draws that byte over the screen as an 8-bit texture with a 256-colour table:
//   source 0  a fog value made from the depth (GfxDepthFog_Draw): 0 for near pixels (depth >= 0xFFA8), otherwise
//             255 - depth / 256, so it rises to 255 at the far end. (Read from the code; the exact byte the
//             console produces was not compared.)
//   source 1  a copy of the frame's alpha, where every model wrote an id (GfxPost_CopyAlphaToDepth).
// Here the byte is computed from this target's depth texture, or read from the copy of the alpha bytes; the table
// arrives as a 256 x 1 texture. Output as gs.frag: colour with alpha rescaled (1.0 = 0x80), and the exact alpha.
// The pipeline carries the game's own blend mode and write mask.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAux;
layout(set = 2, binding = 0) uniform sampler2D depthTex;
layout(set = 2, binding = 1) uniform sampler2D auxTex;
layout(set = 2, binding = 2) uniform sampler2D clutTex;
layout(set = 3, binding = 0) uniform Params {
    vec4 p; // x: source (0 fog from depth, 1 alpha copy), y: depth maximum
} u;
void main() {
    int idx;
    if (u.p.x < 0.5) {
        float z = texture(depthTex, vUv).r * u.p.y;
        idx = z >= 65448.0 ? 0 : clamp(255 - int(z / 256.0), 0, 255);
    } else {
        idx = int(texture(auxTex, vUv).r * 255.0 + 0.5);
    }
    vec4 c = texelFetch(clutTex, ivec2(idx, 0), 0);
    outColor = vec4(c.rgb, clamp(c.a * 255.0 / 128.0, 0.0, 1.0));
    outAux = vec4(c.a, 0.0, 0.0, 1.0);
}
