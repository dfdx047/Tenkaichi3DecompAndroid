#version 450
// GS primitive, fragment stage: texture function (TEX0.TFX / TCC) and alpha test (TEST), as the GS does them.
// Colours are 0..1 for 0..255. Alpha is carried with 1.0 = 0x80 (the GS's "opaque").
layout(location = 0) in vec4 vColor;
layout(location = 1) in vec3 vStq;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAux; // .r: the alpha byte exactly as the GS stores it (object numbers live there)
layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 2, binding = 1) uniform sampler2D dateTex; // a copy of the target's alpha bytes, for the destination alpha test
layout(set = 3, binding = 0) uniform Params {
    ivec4 mode;  // x: textured (1 from GS memory, 2 a frame buffer, 3 a texture pack's replacement), y: TFX, z: TCC, w: alpha test (0 off, else ATST + 1)
    vec4 misc;   // x: AREF in GS units (0..255); y: destination alpha test (0 off, 1 pass where bit 7 of the
                 // stored alpha is 0, 2 where it is 1); z: 1 = FBA, the alpha written gets bit 7 set;
                 // w: for 2D sprites, output pixels per GS pixel (0 = sample per output pixel; negative: a
                 // texture pack's replacement, see below)
    vec4 rect;   // textured from a frame buffer (mode.x == 2): the uv range that may be sampled
    vec4 orig;   // a texture pack's replacement: xy = the original texture's size (2D art, misc.w < 0);
                 // z = the original's largest alpha
} p;
// 1.0 where v is not negative
vec2 step2(vec2 v) { return vec2(v.x >= 0.0 ? 1.0 : 0.0, v.y >= 0.0 ? 1.0 : 0.0); }

void main() {
    float k = 255.0 / 128.0;
#ifndef GS_NO_DISCARD
    // (built a second time with GS_NO_DISCARD for the draws that never discard: a shader with `discard` in it
    // keeps tile-based GPUs from dropping hidden pixels early, whether or not it discards)
    if (p.misc.y != 0.0) {
        // TEST.DATE: the GS looks at bit 7 of the alpha already in the frame buffer. A GPU cannot read what it is
        // drawing to, so the back end hands over a copy made just before this run of draws.
        bool set = texelFetch(dateTex, ivec2(gl_FragCoord.xy), 0).r >= 127.5 / 255.0;
        if (set != (p.misc.y > 1.5)) discard;
    }
#endif
    vec3 rgb = vColor.rgb;
    float a = vColor.a * k;
    if (p.mode.x != 0) {
        vec2 uv = vStq.xy / vStq.z;
        if (p.misc.w != 0.0) {
            // A 2D sprite: take the texture coordinate where the GS takes it, at the whole GS pixel this fragment
            // belongs to (misc.w = output pixels per GS pixel). Every output pixel of that GS pixel then shows
            // the same texel, as on the console. Sampling at the output pixels' own centres reaches a quarter or
            // three quarters of a texel further, and at a sprite's edge that is the neighbouring picture of the
            // sheet (seen as slivers of another bar's colour and as thin lines along HUD panels).
            float s = abs(p.misc.w);
            vec2 f = gl_FragCoord.xy, g = floor(f / s) * s;
            vec2 dx = dFdx(uv), dy = dFdy(uv);
            vec2 at = uv - (dx * (f.x - g.x) + dy * (f.y - g.y)); // where the GS takes it
            if (p.misc.w > 0.0) {
                uv = at;
            } else {
                // A texture pack's replacement (several texels per original texel; orig.xy = the original's size,
                // rect = this piece's rectangle of the sheet). o: this output pixel's place inside its GS pixel.
                // Where the piece is drawn one texel per GS pixel (text, HUD), the GS pixel shows the texel `at`
                // falls into, so the output pixel shows the matching part of that texel; otherwise the
                // coordinate is carried on from `at`, half a GS pixel to either side.
                vec2 o = (f - g) / s, step = vec2(dx.x, dy.y) * s * p.orig.xy; // texels per GS pixel
                vec2 snapped = (floor(at * p.orig.xy) + mix(1.0 - o, o, step2(step))) / p.orig.xy;
                vec2 carried = at + (o - 0.5) * vec2(dx.x, dy.y) * s;
                bvec2 one = lessThan(abs(abs(step) - 1.0), vec2(0.02));
                uv = vec2(one.x ? snapped.x : carried.x, one.y ? snapped.y : carried.y);
                vec2 inset = 0.5 / vec2(textureSize(tex, 0));
                uv = clamp(uv, p.rect.xy + inset, p.rect.zw - inset);
            }
        }
        if (p.mode.x == 2) uv = clamp(uv, p.rect.xy, p.rect.zw);
        vec4 t = texture(tex, uv);
        if (p.mode.x == 1 || p.mode.x == 3) t.a *= k; // a texture from GS memory keeps the GS alpha (0x80 opaque, up to 0xFF)
        // A texture pack's replacement. The game does more with a texture's alpha than blend: the alpha it writes
        // into the picture is tested by later draws, bit 7 as a mask (a texture that stays at 0x7F never sets it,
        // one at 0x80 always does). A replacement's alpha is redrawn and compressed: it strays a few steps either
        // way. orig.z = the original's largest alpha (1.0 = 0x80): the replacement never goes above that; and
        // where the original reaches 0x80, just under opaque counts as opaque.
        if (p.mode.x == 3) {
            if (p.orig.z < 1.0) t.a = min(t.a, p.orig.z);
            else if (t.a > 0.93 && t.a < 1.0) t.a = 1.0;
        }
        if (p.mode.y == 0) {
            rgb = t.rgb * vColor.rgb * k;
            if (p.mode.z != 0) a = t.a * a;
        } else if (p.mode.y == 1) {
            rgb = t.rgb;
            if (p.mode.z != 0) a = t.a;
        } else {
            rgb = t.rgb * vColor.rgb * k + vec3(vColor.a);
            if (p.mode.z != 0) a = (p.mode.y == 2) ? t.a + a : t.a;
        }
    }
#ifndef GS_NO_DISCARD
    if (p.mode.w != 0) {
        // the GS compares integers; round, or a value that is exactly AREF (tree leaves: 0x7F against 0x7F) fails by float error
        float ag = floor(a * 128.0 + 0.5);
        int f = p.mode.w - 1;
        // A texture pack's replacement (mode.x == 3): its alpha is not the exact value the game tests for. The
        // compression moves it by a few steps (0x7F becomes 0x7C..0x7E) and the filtering fades it towards the
        // edge of a cut-out shape, so a test against the exact value drops most of it (trees lost their
        // leaves). There the line is drawn at half the reference: the middle of the fade, where the original
        // shape's edge is.
        float ref = p.mode.x == 3 ? p.misc.x * 0.5 : p.misc.x, tol = p.mode.x == 3 ? max(ref, 0.5) : 0.5;
        bool pass = f == 0 ? false : f == 1 ? true : f == 2 ? ag < ref : f == 3 ? ag <= ref :
                    f == 4 ? abs(ag - p.misc.x) < tol : f == 5 ? ag >= ref : f == 6 ? ag > ref : abs(ag - p.misc.x) >= tol;
        if (!pass) discard;
    }
#endif
    outColor = vec4(clamp(rgb, 0.0, 1.0), clamp(a, 0.0, 1.0));
    float ab = clamp(a * 128.0 / 255.0, 0.0, 1.0);
    if (p.misc.z != 0.0) ab = (float(int(ab * 255.0 + 0.5) | 128)) / 255.0; // FBA
    outAux = vec4(ab, 0.0, 0.0, 1.0);
}
