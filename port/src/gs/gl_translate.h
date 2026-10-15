/* Shader translation for the OpenGL back end (gs_gl.c): Vulkan GLSL 450 -> GL 3.3 core, or GLSL ES 3.20 (Android).
   A header of its own so that port/tools can run the same translation outside the game and check the result. */
#ifndef GL_TRANSLATE_H
#define GL_TRANSLATE_H
#include <stdlib.h>
#include <string.h>

/* The rules port/tools/glprobe.c proved on hardware: #version, strip layout(set/binding) on uniforms and
   samplers, gl_VertexIndex -> gl_VertexID, and layout(location) kept only where GL 3.3 core allows it
   (vertex inputs and fragment outputs).
   es: GLSL ES 3.20 instead (OpenGL ES 3.2, Android): default precisions, std140 uniform blocks (the C side writes
   them with that layout), gl_PointSize written (ES has no default point size).
   nodiscard: GS_NO_DISCARD defined (gs.frag without its discards, as the Vulkan back end's second shader).
   clip_control: the context has glClipControl(GL_ZERO_TO_ONE), so z need not be moved to OpenGL's range. */
static char *gl_translate(const char *src, int is_vertex, int es, int nodiscard, int clip_control) {
    size_t cap = strlen(src) + 512, len = 0;
    char *out = (char *)malloc(cap);
#define PUSHN(s, n_) do { size_t _n = (n_); \
    while (len + _n + 1 > cap) cap *= 2; out = (char *)realloc(out, cap); memcpy(out + len, (s), _n); len += _n; } while (0)
#define PUSH(str) PUSHN((str), strlen(str))
    const char *p = src;
    while (*p) {
        if (!strncmp(p, "#version 450", 12)) {
            PUSH(es ? "#version 320 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;" : "#version 330 core");
            if (nodiscard) { PUSH("\n#define GS_NO_DISCARD 1"); }
            p += 12;
            continue;
        }
        if (!strncmp(p, "gl_VertexIndex", 14)) { PUSH("gl_VertexID"); p += 14; continue; }
        if (!strncmp(p, "layout(depth_less)", 18)) { /* (GL 4.2's; without it gl_FragDepth is simply written) */
            while (*p && *p != '\n') { p++; }
            continue;
        }
        if (!strncmp(p, "layout(set", 10)) {
            const char *e = strchr(p, ')');
            if (e) {
                p = e + 1;
                if (*p == ' ') p++;
                if (es && !strncmp(p, "uniform ", 8) && strncmp(p, "uniform sampler", 15) != 0) { PUSH("layout(std140) "); }
                continue;
            }
        }
        if (!strncmp(p, "layout(location", 15)) {
            const char *e = strchr(p, ')');
            if (e) {
                const char *q = e + 1;
                while (*q == ' ' || *q == '\t') q++;
                int in_out = !strncmp(q, "in ", 3) || !strncmp(q, "out ", 4);
                int bad = in_out && (is_vertex ? !strncmp(q, "out ", 4) : !strncmp(q, "in ", 3));
                if (bad) { p = q; continue; }
                PUSHN(p, (size_t)(e + 1 - p));
                p = e + 1;
                continue;
            }
        }
        out[len++] = *p++;
    }
    out[len] = 0;
    /* SDL GPU (Vulkan) maps clip-space +Y to the TOP of the render target; OpenGL maps it to the BOTTOM, so
       the same shader would store the picture in the other half of the target (black window). Flipping
       gl_Position.y makes OpenGL store the picture exactly where the Vulkan back end does, so the present,
       the screenshots, gl_FragCoord and every texture taken from a target all agree. */
    if (is_vertex) {
        char *m = strstr(out, "void main");
        if (m != NULL) {
            size_t rest = strlen(m + 5) + 1;
            memmove(m + 8, m + 5, rest);
            memcpy(m + 5, "gs_", 3);
            len += 3;
        }
        /* Depth: the shaders put z in Vulkan's clip range (0..w, stored as z / w). OpenGL's is -w..w, stored as
           0.5 + z / 2w, unless glClipControl(GL_ZERO_TO_ONE) is there (GL 4.5 / ARB_clip_control); without it z
           is moved to OpenGL's range here so that the stored depth is again z / w (the fog pass reads it). */
        PUSH("\nvoid main() { gs_main(); gl_Position.y = -gl_Position.y; ");
        if (!clip_control) { PUSH("gl_Position.z = 2.0 * gl_Position.z - gl_Position.w; "); }
        if (es) { PUSH("gl_PointSize = 1.0; "); }
        PUSH("}\n");
        out[len] = 0;
    }
#undef PUSH
#undef PUSHN
    return out;
}
#endif
