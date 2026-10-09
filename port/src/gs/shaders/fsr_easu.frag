#version 450
// AMD FidelityFX Super Resolution 1, first pass (EASU: the edge-adaptive upscale) of the game's picture to the size
// it is shown at. ffx/ffx_a.h and ffx/ffx_fsr1.h are AMD's (MIT, ffx/LICENSE-FidelityFX-FSR.txt); the constants
// come from FsrEasuCon, worked out on the CPU (gs_gpu.c).
#extension GL_GOOGLE_include_directive : require
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
layout(set = 2, binding = 0) uniform sampler2D src; // filtered, clamped
layout(set = 3, binding = 0) uniform Params {
    uvec4 con0, con1, con2, con3;
    vec4 origin; // xy: the output rectangle's top-left corner in the target being drawn
} p;
#define A_GPU 1
#define A_GLSL 1
#include "ffx/ffx_a.h"
#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 c) { return textureGather(src, c, 0); }
AF4 FsrEasuGF(AF2 c) { return textureGather(src, c, 1); }
AF4 FsrEasuBF(AF2 c) { return textureGather(src, c, 2); }
#include "ffx/ffx_fsr1.h"
void main() {
    AF3 c;
    FsrEasuF(c, AU2(gl_FragCoord.xy - p.origin.xy), p.con0, p.con1, p.con2, p.con3);
    outColor = vec4(c, 1.0);
}
