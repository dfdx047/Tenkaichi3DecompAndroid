#version 450
// PC only: the fight picture's average colour (its smallest mipmap), blended into a 1 x 1 texture a little every
// frame (the pipeline's blend: constant 0.06 of the new, the rest of the old), so the grade's balance (grade.frag)
// follows the scene slowly: a flash or a beam filling the screen for a moment does not swing the colours.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
layout(set = 2, binding = 0) uniform sampler2D src; // the copy of the picture, with its mipmaps
layout(set = 3, binding = 0) uniform Params {
    vec4 p; // x: a mipmap level a few texels across the picture, yz: the picture's part of the buffer (uv), w: unused
} u;
void main() {
    // the picture is only the corner of its buffer (512 x 448 of 1024 x 1024, times the resolution): 4 x 4 samples
    // inside it, not the whole buffer, whose rest is black
    vec3 sum = vec3(0.0);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            sum += textureLod(src, (vec2(x, y) + 0.5) / 4.0 * u.p.yz, u.p.x).rgb;
        }
    }
    outColor = vec4(sum / 16.0, 1.0);
}
