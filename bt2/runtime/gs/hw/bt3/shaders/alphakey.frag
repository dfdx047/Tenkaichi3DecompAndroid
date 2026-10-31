#version 450
// Native version of the game's "alpha key" pass (GfxAlphaKey_Draw, src/sys/gfxm_b.c): a model that must be seen
// through (a fighter between the camera and the action) does not draw its colours, only an id 0xF4..0xFE into
// the frame's alpha. The PS2 pass copies that byte into the depth buffer's spare byte and draws it over the
// screen through a table: eleven fixed colours with alpha 0x30 (of 0x80), everything else transparent.
// Here: the same table read from the exact copy of the alpha byte; the pipeline blends by the output alpha.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
layout(set = 2, binding = 0) uniform sampler2D aux; // .r = the GS alpha byte / 255
const vec3 kTable[11] = vec3[11]( // GfxAlphaKey_BuildClut
    vec3(0x3F, 0x3F, 0xFF), vec3(0x80, 0x00, 0xFF), vec3(0xFF, 0xFF, 0x20), vec3(0xFF, 0x00, 0x00),
    vec3(0x00, 0xFF, 0x00), vec3(0x54, 0xFD, 0xFF), vec3(0xFF, 0x00, 0xFF), vec3(0x40, 0x00, 0xFF),
    vec3(0xFF, 0xFF, 0x20), vec3(0xFF, 0xFF, 0x2A), vec3(0xFF, 0xFF, 0x34));
void main() {
    int id = int(texture(aux, vUv).r * 255.0 + 0.5);
    if (id < 0xF4 || id == 0xFF) discard;
    outColor = vec4(kTable[id - 0xF4] / 255.0, float(0x30) / 128.0);
}
