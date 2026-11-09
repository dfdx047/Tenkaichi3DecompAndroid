#version 450
// GS primitive, vertex stage: frame-buffer pixels to clip space. x, y arrive in GS pixels (0..1024 covers the
// whole render target), z already scaled to 0..1.
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inStq;
layout(location = 0) out vec4 vColor;
layout(location = 1) out vec3 vStq;
void main() {
    gl_Position = vec4(inPos.x / 512.0 - 1.0, 1.0 - inPos.y / 512.0, inPos.z, 1.0); // SDL GPU: +Y is up in clip space
    vColor = inColor;
    vStq = inStq;
}
