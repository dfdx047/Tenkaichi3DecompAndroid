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
    vec4 step; // xy: one PS2 pixel in uv, z: darkness (0x64 / 255)
} p;
float table(vec2 uv) {
    int id = int(texture(aux, uv).r * 255.0 + 0.5);
    if (id == 255) return 0.0;           // "nothing" (stage, sky): the table gives it no colour, so an object next to it has an edge
    int c = (id << 3) & 0xFF;            // ObjOutline_BuildClut
    return float(c == 0 ? 0x80 : c);
}
void main() {
    float here = table(vUv), right = table(vUv + vec2(p.step.x, 0.0)), down = table(vUv + vec2(0.0, p.step.y));
    float left = table(vUv - vec2(p.step.x, 0.0)), up = table(vUv - vec2(0.0, p.step.y));
    // the subtraction keeps only positive differences: the line is drawn on the side with the larger value
    bool edge = here > right || here > down || here > left || here > up;
    if (p.step.w != 0.0) { // debug view: darken by the object number instead (255 = "nothing" stays untouched)
        float id = texture(aux, vUv).r * 255.0;
        outColor = vec4(id > 254.5 ? vec3(0.0) : vec3(0.3 + id / 16.0, id == 0.0 ? 0.6 : 0.0, 0.0), 0.0);
        return;
    }
    outColor = vec4(vec3(edge ? p.step.z : 0.0), 0.0);
}
