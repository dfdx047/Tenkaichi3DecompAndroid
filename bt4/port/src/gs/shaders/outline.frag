#version 450
// Native version of the game's outline effect (ObjOutline_Draw, src/sys/gfxm_a.c).
// Every model writes an object number into the frame's alpha; the PS2 pass turns that number into a value with
// a 256-entry table, subtracts the same image shifted by one pixel down and one pixel right (so only the places
// where the number changes survive), and blends a dark rectangle (0x64 per channel) through the result.
// Here: the same table and the same two neighbours, read from the exact copy of the alpha byte, and the result
// is the amount to SUBTRACT from the picture (the pipeline's blend is destination minus source).
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
layout(set = 2, binding = 0) uniform sampler2D aux; // .r = the GS alpha byte / 255
layout(set = 3, binding = 0) uniform Params {
    vec4 step; // x: the line's width across in uv, y: one PS2 pixel down in uv, z: darkness (0x64 / 255)
} p;
float table(vec2 uv) {
    int id = int(texture(aux, uv).r * 255.0 + 0.5);
    if (id == 255) return 0.0;           // "nothing" (stage, sky): the table gives it no colour, so an object next to it has an edge
    int c = (id << 3) & 0xFF;            // ObjOutline_BuildClut
    return float(c == 0 ? 0x80 : c);
}
void main() {
    // Widescreen stretches the PS2's 512 pixels across the wider picture, so a line one PS2 pixel wide grew with the
    // aspect ratio on the left and right of every shape. Across, the width (step.x) is narrowed by the stretch and
    // measured in texels of the image: a texel at distance d from the edge darkens by clamp(width - d + 1, 0, 1),
    // which gives lines of the same thickness at any shape (a fraction of a texel as a lighter last texel).
    float tx = 1.0 / float(textureSize(aux, 0).x), width = p.step.x / tx;
    float here = table(vUv);
    // Nothing to subtract (the pipeline: picture minus this, colour only), so nothing is written: the same picture,
    // without reading and writing back every pixel of the screen. A place with no object (the table's 0: stage,
    // sky, most of the picture) is never on the larger side of an edge, so its neighbours are not even read.
    if (here == 0.0 && p.step.w == 0.0) {
        discard;
    }
    float down = table(vUv + vec2(0.0, p.step.y)), up = table(vUv - vec2(0.0, p.step.y));
    // the subtraction keeps only positive differences: the line is drawn on the side with the larger value
    float amount = here > down || here > up ? 1.0 : 0.0;
    for (int k = 1; k <= 8 && float(k) < width + 1.0; k++) {
        if (here > table(vUv + vec2(float(k) * tx, 0.0)) || here > table(vUv - vec2(float(k) * tx, 0.0))) {
            amount = max(amount, clamp(width - float(k) + 1.0, 0.0, 1.0));
            break; // (the nearest edge gives the most)
        }
    }
    if (amount == 0.0 && p.step.w == 0.0) {
        discard;
    }
    if (p.step.w != 0.0) { // debug view: darken by the object number instead (255 = "nothing" stays untouched)
        float id = texture(aux, vUv).r * 255.0;
        outColor = vec4(id > 254.5 ? vec3(0.0) : vec3(0.3 + id / 16.0, id == 0.0 ? 0.6 : 0.0, 0.0), 0.0);
        return;
    }
    outColor = vec4(vec3(p.step.z * amount), 0.0);
}
