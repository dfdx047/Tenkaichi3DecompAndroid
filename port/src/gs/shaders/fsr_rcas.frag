#version 450
// AMD FidelityFX Super Resolution 1, second pass (RCAS: the contrast-adaptive sharpening) of the upscaled picture
// onto the screen. ffx/ffx_a.h and ffx/ffx_fsr1.h are AMD's (MIT, ffx/LICENSE-FidelityFX-FSR.txt).
#extension GL_GOOGLE_include_directive : require
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
layout(set = 2, binding = 0) uniform sampler2D src; // the upscaled picture, read by whole texels
layout(set = 3, binding = 0) uniform Params {
    uvec4 con;   // FsrRcasCon: x = the sharpness
    vec4 origin; // xy: the output rectangle's top-left corner on the screen
} p;
#define A_GPU 1
#define A_GLSL 1
#include "ffx/ffx_a.h"
#define FSR_RCAS_F 1
AF4 FsrRcasLoadF(ASU2 q) { return texelFetch(src, q, 0); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}
#include "ffx/ffx_fsr1.h"
void main() {
    AF3 c;
    FsrRcasF(c.r, c.g, c.b, AU2(gl_FragCoord.xy - p.origin.xy), p.con);
    outColor = vec4(c, 1.0);
}
