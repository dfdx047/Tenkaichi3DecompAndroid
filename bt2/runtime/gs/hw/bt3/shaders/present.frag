#version 450
// The game's picture onto the screen, with the filter the user chose (gs_gpu.c, show_target):
//   0 bilinear, 1 sharp bilinear (whole source pixels, only their edges blended), 2 FXAA (edges smoothed at the
//   game's own resolution, then scaled bilinear).
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
layout(set = 2, binding = 0) uniform sampler2D src; // filtered, clamped
layout(set = 3, binding = 0) uniform Params {
    vec4 tex;    // the source texture: 1 / width, 1 / height, width, height (texels)
    vec4 region; // xy: uv of the shown picture's bottom-right corner; zw: output pixels per source texel
    ivec4 mode;  // x: the filter
} p;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

// FXAA in its compact form: the edge's direction from the corner neighbours' luma, a blend of samples along it,
// and the wider blend refused where it would bring in a colour from beyond the local range.
vec3 fxaa(vec2 uv) {
    vec2 r = p.tex.xy;
    vec3 cM = texture(src, uv).rgb;
    float lM = luma(cM);
    float lNW = luma(texture(src, uv + vec2(-1.0, -1.0) * r).rgb);
    float lNE = luma(texture(src, uv + vec2(1.0, -1.0) * r).rgb);
    float lSW = luma(texture(src, uv + vec2(-1.0, 1.0) * r).rgb);
    float lSE = luma(texture(src, uv + vec2(1.0, 1.0) * r).rgb);
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    if (lMax - lMin < max(0.0312, lMax * 0.125)) {
        return cM; // no edge worth smoothing
    }
    vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), (lNW + lSW) - (lNE + lSE));
    float reduce = max((lNW + lNE + lSW + lSE) * 0.25 * (1.0 / 8.0), 1.0 / 128.0);
    float rcpMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
    dir = clamp(dir * rcpMin, vec2(-8.0), vec2(8.0)) * r;
    vec3 a = 0.5 * (texture(src, uv + dir * (1.0 / 3.0 - 0.5)).rgb + texture(src, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 b = a * 0.5 + 0.25 * (texture(src, uv - dir * 0.5).rgb + texture(src, uv + dir * 0.5).rgb);
    float lB = luma(b);
    return (lB < lMin || lB > lMax) ? a : b;
}

void main() {
    vec2 uv = vUv * p.region.xy;
    vec3 c;
    if (p.mode.x == 1) {
        // sharp bilinear: inside a source texel its colour, across the last output pixel at its edge a blend
        vec2 t = uv * p.tex.zw, base = floor(t), d = t - base - 0.5;
        vec2 scale = max(p.region.zw, vec2(1.0)), inner = 0.5 - 0.5 / scale;
        vec2 f = (d - clamp(d, -inner, inner)) * scale + 0.5;
        c = texture(src, (base + f) * p.tex.xy).rgb;
    } else if (p.mode.x == 2) {
        c = fxaa(uv);
    } else {
        c = texture(src, uv).rgb;
    }
    outColor = vec4(c, 1.0);
}
