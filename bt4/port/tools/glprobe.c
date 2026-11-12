/* glprobe: smoke-test the GS shaders on a real OpenGL 3.3 core context, without game data.
 *
 * It exists so the OpenGL back end (gs_gl.c) can be developed and verified against the same shaders the
 * Vulkan back end uses, before any of the game runs. What it does:
 *   - opens a hidden, surfaceless EGL context at GL 3.3 core (the back end's floor);
 *   - translates each shader from its Vulkan source (port/src/gs/shaders) to GL 3.3, exactly the way gs_gl.c
 *     will (strip set=/binding= from uniform and sampler layouts, gl_VertexIndex -> gl_VertexID, #version);
 *   - compiles and LINKS every program the back end builds, so a missing varying or a bad uniform block
 *     fails here first;
 *   - draws one frame through gs.vert+gs.frag (a lit triangle) and one through fx.vert+outline.frag (the
 *     outline over a synthetic object-number buffer), and writes both to PPM.
 *
 *   glprobe <shader-dir> <out-dir>            writes <out-dir>/gs.ppm and <out-dir>/outline.ppm
 *   glprobe <shader-dir> <out-dir> -q        only compile/link; no draw, no PPM
 *
 * Build:  gcc -O2 -o port/build/glprobe port/tools/glprobe.c -lEGL -lGL -ldl
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- EGL ---------------------------------------------------------------- */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <dlfcn.h>

/* ---- minimal GL 3.3 core loader ---------------------------------------- */
typedef unsigned int GLenum; typedef unsigned int GLuint; typedef int GLint; typedef int GLsizei;
typedef unsigned char GLboolean; typedef char GLchar; typedef float GLfloat; typedef double GLdouble;
typedef unsigned int GLbitfield; typedef void GLvoid; typedef intptr_t GLsizeiptr; typedef intptr_t GLintptr;
typedef unsigned char GLubyte;

/* X(name, ret, args, callargs) — every entry point is declared with its real signature and loaded by name. */
#define GL_FUNCS(X) \
    X(glGetError, GLenum, (void), ()) \
    X(glEnable, void, (GLenum), (GLenum)) \
    X(glDisable, void, (GLenum), (GLenum)) \
    X(glViewport, void, (GLint, GLint, GLsizei, GLsizei), (GLint, GLint, GLsizei, GLsizei)) \
    X(glClearColor, void, (GLfloat, GLfloat, GLfloat, GLfloat), (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(glClear, void, (GLbitfield), (GLbitfield)) \
    X(glGetString, const GLubyte *, (GLenum), (GLenum)) \
    X(glGetIntegerv, void, (GLenum, GLint *), (GLenum, GLint *)) \
    X(glFinish, void, (void), ()) \
    X(glPixelStorei, void, (GLenum, GLint), (GLenum, GLint)) \
    X(glBlendFunc, void, (GLenum, GLenum), (GLenum, GLenum)) \
    X(glColorMask, void, (GLboolean, GLboolean, GLboolean, GLboolean), (GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(glDepthMask, void, (GLboolean), (GLboolean)) \
    X(glCreateShader, GLuint, (GLenum), (GLenum)) \
    X(glShaderSource, void, (GLuint, GLsizei, const GLchar *const *, const GLint *), (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
    X(glCompileShader, void, (GLuint), (GLuint)) \
    X(glGetShaderiv, void, (GLuint, GLenum, GLint *), (GLuint, GLenum, GLint *)) \
    X(glGetShaderInfoLog, void, (GLuint, GLsizei, GLsizei *, GLchar *), (GLuint, GLsizei, GLsizei *, GLchar *)) \
    X(glDeleteShader, void, (GLuint), (GLuint)) \
    X(glCreateProgram, GLuint, (void), ()) \
    X(glAttachShader, void, (GLuint, GLuint), (GLuint, GLuint)) \
    X(glBindAttribLocation, void, (GLuint, GLuint, const GLchar *), (GLuint, GLuint, const GLchar *)) \
    X(glLinkProgram, void, (GLuint), (GLuint)) \
    X(glGetProgramiv, void, (GLuint, GLenum, GLint *), (GLuint, GLenum, GLint *)) \
    X(glGetProgramInfoLog, void, (GLuint, GLsizei, GLsizei *, GLchar *), (GLuint, GLsizei, GLsizei *, GLchar *)) \
    X(glUseProgram, void, (GLuint), (GLuint)) \
    X(glDeleteProgram, void, (GLuint), (GLuint)) \
    X(glGetUniformLocation, GLint, (GLuint, const GLchar *), (GLuint, const GLchar *)) \
    X(glUniform1i, void, (GLint, GLint), (GLint, GLint)) \
    X(glUniform4f, void, (GLint, GLfloat, GLfloat, GLfloat, GLfloat), (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(glUniform4fv, void, (GLint, GLsizei, const GLfloat *), (GLint, GLsizei, const GLfloat *)) \
    X(glUniformMatrix4fv, void, (GLint, GLsizei, GLboolean, const GLfloat *), (GLint, GLsizei, GLboolean, const GLfloat *)) \
    X(glGetUniformBlockIndex, GLuint, (GLuint, const GLchar *), (GLuint, const GLchar *)) \
    X(glUniformBlockBinding, void, (GLuint, GLuint, GLuint), (GLuint, GLuint, GLuint)) \
    X(glBindBufferBase, void, (GLenum, GLuint, GLuint), (GLenum, GLuint, GLuint)) \
    X(glGenBuffers, void, (GLsizei, GLuint *), (GLsizei, GLuint *)) \
    X(glBindBuffer, void, (GLenum, GLuint), (GLenum, GLuint)) \
    X(glBufferData, void, (GLenum, GLsizeiptr, const void *, GLenum), (GLenum, GLsizeiptr, const void *, GLenum)) \
    X(glGenVertexArrays, void, (GLsizei, GLuint *), (GLsizei, GLuint *)) \
    X(glBindVertexArray, void, (GLuint), (GLuint)) \
    X(glEnableVertexAttribArray, void, (GLuint), (GLuint)) \
    X(glVertexAttribPointer, void, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *), (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
    X(glDrawArrays, void, (GLenum, GLint, GLsizei), (GLenum, GLint, GLsizei)) \
    X(glGenTextures, void, (GLsizei, GLuint *), (GLsizei, GLuint *)) \
    X(glBindTexture, void, (GLenum, GLuint), (GLenum, GLuint)) \
    X(glActiveTexture, void, (GLenum), (GLenum)) \
    X(glTexImage2D, void, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *), (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    X(glTexParameteri, void, (GLenum, GLenum, GLint), (GLenum, GLenum, GLint)) \
    X(glReadPixels, void, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *), (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) \
    X(glGenFramebuffers, void, (GLsizei, GLuint *), (GLsizei, GLuint *)) \
    X(glBindFramebuffer, void, (GLenum, GLuint), (GLenum, GLuint)) \
    X(glFramebufferTexture2D, void, (GLenum, GLenum, GLenum, GLuint, GLint), (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(glFramebufferRenderbuffer, void, (GLenum, GLenum, GLenum, GLuint), (GLenum, GLenum, GLenum, GLuint)) \
    X(glCheckFramebufferStatus, GLenum, (GLenum), (GLenum)) \
    X(glGenRenderbuffers, void, (GLsizei, GLuint *), (GLsizei, GLuint *)) \
    X(glBindRenderbuffer, void, (GLenum, GLuint), (GLenum, GLuint)) \
    X(glRenderbufferStorage, void, (GLenum, GLenum, GLsizei, GLsizei), (GLenum, GLenum, GLsizei, GLsizei))

/* Declaration: one function pointer per entry point, with its real type. */
#define GL_DECL(name, ret, args, callargs) static ret (*name) args;
GL_FUNCS(GL_DECL)
#undef GL_DECL

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_TRIANGLES 0x0004
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_RGBA 0x1908
#define GL_RED 0x1903
#define GL_RGBA8 0x8058
#define GL_R8 0x8229
#define GL_TEXTURE_2D 0x0DE1
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DEPTH_TEST 0x0B71

static void *libgl;
static int load_gl(void) {
    void *egl = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    void *(*gpa)(const char *) = egl ? (void *(*)(const char *))dlsym(egl, "eglGetProcAddress") : NULL;
    libgl = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
/* name = eglGetProcAddress(#name) with a dlsym(libGL) fallback. Cast to the real function type. */
#define LOAD(name, ret, args, callargs) \
    name = gpa ? (ret (*) args)gpa(#name) : NULL; \
    if (!name && libgl) name = (ret (*) args)dlsym(libgl, #name); \
    if (!name) { fprintf(stderr, "glprobe: missing GL entry point %s\n", #name); return 0; }
    GL_FUNCS(LOAD)
#undef LOAD
    return 1;
}

/* ---- shader translation: Vulkan GLSL 450 -> GL 3.3 core ------------------ */
/* The back end's two source-level differences from the Vulkan dialect:
 *   layout(set = N, binding = M) uniform ...   ->  uniform ...      (GL sets these after link)
 *   gl_VertexIndex                             ->  gl_VertexID
 * layout(location = N) on in/out stays (core since 1.30). #version 450 becomes 330 core.
 */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    if (fread(s, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(s); return NULL; }
    s[n] = 0; fclose(f); return s;
}

/* GL 3.3 core allows layout(location=N) on vertex INPUTS and fragment OUTPUTS only. Vertex OUTPUTS and
 * fragment INPUTS need GLSL 4.20 / ARB_separate_shader_objects, so strip the qualifier there and let the
 * linker match the varyings by name — every program names them identically on both sides.
 */
static char *translate(const char *src, int is_vertex) {
    size_t cap = strlen(src) + 128, len = 0;
    char *out = malloc(cap);
#define PUSHN(s, n_) do { size_t _n = (n_); \
    while (len + _n + 1 > cap) cap *= 2; out = realloc(out, cap); memcpy(out + len, (s), _n); len += _n; } while (0)
#define PUSH(str) PUSHN((str), strlen(str))
    const char *p = src;
    while (*p) {
        if (!strncmp(p, "#version 450", 12)) { PUSH("#version 330 core"); p += 12; continue; }
        if (!strncmp(p, "gl_VertexIndex", 14)) { PUSH("gl_VertexID"); p += 14; continue; }
        /* layout(set = N, binding = M) on uniforms/samplers: GL binds these after link. Drop it. */
        if (!strncmp(p, "layout(set", 10)) {
            const char *e = strchr(p, ')');
            if (e) { p = e + 1; if (*p == ' ') p++; continue; }
        }
        /* layout(location = N): keep on vertex inputs and fragment outputs; drop on the other two. */
        if (!strncmp(p, "layout(location", 15)) {
            const char *e = strchr(p, ')');
            if (e) {
                const char *q = e + 1;
                while (*q == ' ' || *q == '\t') q++;
                int in_out = !strncmp(q, "in ", 3) || !strncmp(q, "out ", 4);
                int bad = in_out && (is_vertex ? !strncmp(q, "out ", 4) : !strncmp(q, "in ", 3));
                if (bad) { p = q; continue; }
                PUSHN(p, (size_t)(e + 1 - p));   /* keep the qualifier verbatim */
                p = e + 1;
                continue;
            }
        }
        out[len++] = *p++;
    }
    out[len] = 0;
#undef PUSH
#undef PUSHN
    return out;
}

/* ---- GL helpers --------------------------------------------------------- */
static GLuint compile(GLenum stage, const char *src, const char *tag) {
    GLuint sh = glCreateShader(stage);
    GLint ok = 0;
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint n = 0; glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &n);
        char *log = malloc((size_t)n + 1); glGetShaderInfoLog(sh, n, NULL, log);
        fprintf(stderr, "glprobe: %s compile FAILED\n%s\n", tag, log);
        fprintf(stderr, "---- translated source ----\n%s\n", src);
        exit(1);
    }
    return sh;
}

static GLuint program(const char *dir, const char *vs, const char *fs, const char **fsamplers, int ns) {
    char path[1024]; char *vsrc, *fsrc; GLuint p, v, f; GLint ok = 0;
    snprintf(path, sizeof path, "%s/%s", dir, vs);
    vsrc = read_file(path); if (!vsrc) { fprintf(stderr, "glprobe: cannot read %s\n", path); exit(1); }
    snprintf(path, sizeof path, "%s/%s", dir, fs);
    fsrc = read_file(path); if (!fsrc) { fprintf(stderr, "glprobe: cannot read %s\n", path); exit(1); }
    char *vt = translate(vsrc, 1), *ft = translate(fsrc, 0);
    free(vsrc); free(fsrc);
    v = compile(GL_VERTEX_SHADER, vt, vs); free(vt);
    f = compile(GL_FRAGMENT_SHADER, ft, fs); free(ft);
    p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint n = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &n);
        char *log = malloc((size_t)n + 1); glGetProgramInfoLog(p, n, NULL, log);
        fprintf(stderr, "glprobe: %s + %s link FAILED\n%s\n", vs, fs, log);
        exit(1);
    }
    glDeleteShader(v); glDeleteShader(f);
    /* GL 3.3 has no layout(binding=): bind samplers and uniform blocks by name after link. */
    glUseProgram(p);
    for (int i = 0; i < ns; i++)
        glUniform1i(glGetUniformLocation(p, fsamplers[i]), i);
    printf("glprobe: OK  %-12s + %-14s\n", vs, fs);
    return p;
}

/* Bind a named uniform block to a binding point, and lay a std140 buffer over it. */
static void bind_block(GLuint prog, const char *block, const void *data, size_t size, GLuint *buf) {
    GLuint idx = glGetUniformBlockIndex(prog, block);
    if (idx == 0xFFFFFFFFu) { fprintf(stderr, "glprobe: no uniform block %s\n", block); exit(1); }
    glUniformBlockBinding(prog, idx, 0);
    glGenBuffers(1, buf);
    glBindBuffer(GL_UNIFORM_BUFFER, *buf);
    glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)size, data, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, *buf);
}

static void make_tex(GLuint *tex, int w, int h, const void *px, GLenum ifmt, GLenum fmt) {
    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)ifmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void write_ppm(const char *path, const uint8_t *rgb, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "glprobe: cannot write %s\n", path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    /* GL rows are bottom-up; PPM is top-down. */
    for (int y = h - 1; y >= 0; y--) fwrite(rgb + (size_t)y * w * 3, 3, (size_t)w, f);
    fclose(f);
    printf("glprobe: wrote %s\n", path);
}

/* A drawable target: one RGBA8 colour texture + a depth renderbuffer, bound as the draw framebuffer.
 * A surfaceless context has NO default framebuffer, so every clear/draw/readback here goes through this. */
static void make_fbo(GLuint *fbo, GLuint *color, GLuint *depth, int w, int h) {
    glGenTextures(1, color);
    glBindTexture(GL_TEXTURE_2D, *color);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenRenderbuffers(1, depth);
    glBindRenderbuffer(GL_RENDERBUFFER, *depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, *depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "glprobe: framebuffer incomplete\n");
        exit(1);
    }
    glViewport(0, 0, w, h);
}

/* ---- EGL: hidden surfaceless GL 3.3 core context ------------------------ */
static EGLDisplay dpy; static EGLContext ctx;
static int make_context(void) {
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL)) {
        fprintf(stderr, "glprobe: eglInitialize failed\n"); return 0;
    }
    if (!eglBindAPI(EGL_OPENGL_API)) { fprintf(stderr, "glprobe: eglBindAPI(OPENGL) failed\n"); return 0; }
    EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    EGLConfig cfg; EGLint ncfg = 0;
    if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &ncfg) || ncfg < 1) {
        fprintf(stderr, "glprobe: eglChooseConfig failed\n"); return 0;
    }
    EGLint ctx_attr[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_NONE
    };
    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (ctx == EGL_NO_CONTEXT) { fprintf(stderr, "glprobe: eglCreateContext(3.3 core) failed\n"); return 0; }
    /* Surfaceless: draw to a framebuffer object instead of a window. */
    if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        fprintf(stderr, "glprobe: eglMakeCurrent(surfaceless) failed\n"); return 0;
    }
    if (!load_gl()) return 0;
    printf("glprobe: GL_VERSION   %s\n", (const char *)glGetString(GL_VERSION));
    printf("glprobe: GL_RENDERER  %s\n", (const char *)glGetString(GL_RENDERER));
    return 1;
}

/* ---- tests --------------------------------------------------------------- */
static const char *gs_samplers[] = { "tex", "dateTex" };
static const char *one_sampler[]  = { "aux" };
static const char *dclut_samplers[] = { "depthTex", "auxTex", "clutTex" };

/* The VU programs (vu0/vu4/vu6) share one std140 block layout, U:
 *   mat4 boneA@0, boneB@64, [pivotA@128, pivotB@144 | scale@128, texScale@144],
 *   mat4 screen@160, vec4 light@224, color0@240, color1@256, misc@272 [, light2@288].
 * With boneA=boneB=screen=identity, zero pivots and misc.z=1, every program maps its position through the
 * same identity screen matrix gs.vert uses, so the same triangle lands in the same place — which is what
 * makes the probe compare the four vertex programs against each other rather than just checking they parse.
 */
static void bind_u(GLuint prog, int vu6) {
    float u[76];                                    /* 304 bytes */
    memset(u, 0, sizeof u);
    static const float I[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    memcpy(u +  0, I, sizeof I);                    /* boneA / shadowCam */
    memcpy(u + 16, I, sizeof I);                    /* boneB */
    if (vu6) {                                      /* scale, texScale */
        u[32] = u[33] = u[34] = u[35] = 1.0f;
        u[36] = u[37] = 1.0f;
    }
    memcpy(u + 40, I, sizeof I);                    /* screen */
    u[60] = 255.0f; u[61] = 0.0f; u[62] = 0.0f; u[63] = 128.0f;   /* color0 */
    u[64] = 0.0f;   u[65] = 255.0f; u[66] = 0.0f; u[67] = 128.0f; /* color1 */
    u[68] = 0.0f; u[69] = 0.0f; u[70] = 1.0f; u[71] = 0.0f;       /* misc: XYOFFSET 0, depth max 1, layer 0 */
    GLuint b;
    bind_block(prog, "U", u, sizeof u, &b);
}

/* gs.frag Params: mode.x=0 untextured, mode.w=0 no alpha test; misc.y=0 no date test. */
static void test_gs(const char *dir, const char *out, int draw) {
    static const char *vs_list[] = { "gs.vert", "vu0.vert", "vu4.vert", "vu6.vert" };
    for (unsigned i = 0; i < sizeof vs_list / sizeof *vs_list; i++) {
        int is_vu = i > 0;
        GLuint p = program(dir, vs_list[i], "gs.frag", gs_samplers, 2);
        if (!draw) { glDeleteProgram(p); continue; }

        /* Params (std140: three vec4). */
        float params[12] = {
            0, 0, 0, 0,      /* mode: untextured, no alpha test */
            0, 0, 0, 0,      /* misc: no date test, no FBA */
            0, 0, 1, 1,      /* rect */
        };
        GLuint ubo; bind_block(p, "Params", params, sizeof params, &ubo);
        if (is_vu) bind_u(p, !strcmp(vs_list[i], "vu6.vert"));

        /* One 1x1 white texture on unit 0 (tex) and unit 1 (dateTex). */
        unsigned char white[4] = { 255, 255, 255, 255 };
        GLuint t0, t1; make_tex(&t0, 1, 1, white, GL_RGBA8, GL_RGBA); make_tex(&t1, 1, 1, white, GL_RGBA8, GL_RGBA);

        /* A triangle in GS pixel space (0..1024). All three attributes are vec4 so one buffer set serves
         * every program: gs.vert takes .xyz of loc 0 and .xyz of loc 2, the VU programs take all four.
         * loc 1 is gs.vert's inColor and vu0's inNormal; the VU shaders scale it themselves. */
        float pos[12]  = { 120,120,0.5f,1,  700,220,0.5f,1,  260,760,0.5f,1 };
        float col[12]  = { 255,0,0,128,     0,255,0,128,     0,0,255,128 };
        float st[12]   = { 0,0,1,0,         1,0,1,0,         0,1,1,0 };
        GLuint vao, vb[3];
        glGenVertexArrays(1, &vao); glBindVertexArray(vao);
        glGenBuffers(3, vb);
        glBindBuffer(GL_ARRAY_BUFFER, vb[0]); glBufferData(GL_ARRAY_BUFFER, sizeof pos, pos, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 4, GL_FLOAT, 0, 0, NULL);
        glBindBuffer(GL_ARRAY_BUFFER, vb[1]); glBufferData(GL_ARRAY_BUFFER, sizeof col, col, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, 0, 0, NULL);
        glBindBuffer(GL_ARRAY_BUFFER, vb[2]); glBufferData(GL_ARRAY_BUFFER, sizeof st, st, GL_STATIC_DRAW);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, 0, 0, NULL);

        GLuint fbo, ctex, dtex;
        make_fbo(&fbo, &ctex, &dtex, 512, 512);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(p);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, t0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, t1);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        if (out) {
            uint8_t *px = malloc(512 * 512 * 3);
            uint8_t *rgba = malloc(512 * 512 * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            /* Read RGBA, then drop alpha: the PPM shows the colour channel only. */
            glReadPixels(0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
            for (size_t k = 0; k < (size_t)512 * 512; k++) {
                px[k*3+0] = rgba[k*4+0]; px[k*3+1] = rgba[k*4+1]; px[k*3+2] = rgba[k*4+2];
            }
            char path[1024]; snprintf(path, sizeof path, "%s/%s.ppm", out, vs_list[i][0] == 'g' ? "gs" : vs_list[i]);
            write_ppm(path, px, 512, 512);
            free(px); free(rgba);
        }
        glDeleteProgram(p);
    }
}

/* fx.vert + outline.frag over a synthetic object-number buffer (the alpha byte the models write). */
static void test_outline(const char *dir, const char *out, int draw) {
    GLuint p = program(dir, "fx.vert", "outline.frag", one_sampler, 1);
    if (!draw) { glDeleteProgram(p); return; }

    /* One PS2 pixel in uv; darkness 0x64/255; w=0 (not the debug view). */
    float step[4] = { 1.0f/256.0f, 1.0f/256.0f, 0x64/255.0f, 0.0f };
    GLuint ubo; bind_block(p, "Params", step, sizeof step, &ubo);

    /* 256x256 R8: a bright object (id 3) on a "nothing" background (255), so edges appear on the border. */
    enum { N = 256 };
    uint8_t *aux = malloc(N * N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
            aux[y*N+x] = (x >= 40 && x < 200 && y >= 40 && y < 200) ? 3 : 255;
    GLuint atex; make_tex(&atex, N, N, aux, GL_R8, GL_RED);
    free(aux);

    GLuint vao; glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    GLuint fbo, ctex, dtex;
    make_fbo(&fbo, &ctex, &dtex, N, N);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(p);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, atex);
    glDrawArrays(GL_TRIANGLES, 0, 3);   /* fx.vert builds the triangle from gl_VertexID */

    if (out) {
        uint8_t *rgba = malloc(N*N*4), *px = malloc(N*N*3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, N, N, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        for (size_t i = 0; i < (size_t)N*N; i++) {
            px[i*3+0] = rgba[i*4+0]; px[i*3+1] = rgba[i*4+1]; px[i*3+2] = rgba[i*4+2];
        }
        char path[1024]; snprintf(path, sizeof path, "%s/outline.ppm", out);
        write_ppm(path, px, N, N);
        free(px); free(rgba);
    }
    glDeleteProgram(p);
}

/* alphakey.frag and dclut.frag: link them (they are fullscreen too) even without a draw target. */
static void test_link_only(const char *dir) {
    GLuint a = program(dir, "fx.vert", "alphakey.frag", one_sampler, 1); glDeleteProgram(a);
    GLuint d = program(dir, "fx.vert", "dclut.frag", dclut_samplers, 3); glDeleteProgram(d);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <shader-dir> <out-dir> [-q]\n", argv[0]); return 2; }
    const char *dir = argv[1], *out = argv[2];
    int draw = (argc < 4 || strcmp(argv[3], "-q") != 0);
    if (!make_context()) return 1;
    test_gs(dir, draw ? out : NULL, draw);
    test_outline(dir, draw ? out : NULL, draw);
    test_link_only(dir);
    glFinish();
    printf("glprobe: all shaders compiled, linked%s.\n", draw ? " and drew" : "");
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    return 0;
}
