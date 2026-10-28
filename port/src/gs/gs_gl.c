/*
 * Graphics Synthesizer, OpenGL back end (GL 3.3 core: SDL3 window + a core context; on Android OpenGL ES 3.2, the
 * same code with the shaders translated to GLSL ES and the few things ES lacks left out, see GL_ES below).
 *
 * gs_draw.c records the frame (every primitive as a GsDraw, the vertices, the decoded textures) and installs
 * this file as the sGlBackend. It owns the GL objects (programs translated from the Vulkan GLSL at run time,
 * vertex buffers, textures, sampler objects, one framebuffer per GS frame-buffer address) and replays the list.
 *
 * Written from the ground up against the canonical OpenGL 3.3 core patterns (learnopengl.com):
 *   - one VAO per vertex layout, configured once with the VBO bound;
 *   - a framebuffer per target: colour (GL_RGBA8) + aux (GL_R8) + depth/stencil (GL_DEPTH32F_STENCIL8), complete, and
 *     used through GL_FRAMEBUFFER;
 *   - depth/colour state set explicitly before every draw, exactly like a pipeline object would be.
 * The GL floor is 3.3 core.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "gs_internal.h"
#include "gs_draw.h"
#include "shaders.h"
#include "ui.h"

extern int Port_AspectMilli(void);
extern int Port_PageAspectMilli(void); /* plat_stub.c: 4:3 while the menus run, if asked for */

/* Android: OpenGL ES 3.2 (every function used here is in it). What it lacks against desktop GL 3.3: depth clamping
   (GL_EXT_depth_clamp where the driver has it), S3TC (DXT) textures (a texture pack's .dds files are then unpacked to
   RGBA on the CPU, gs_draw.c), a default point size (the shaders write one). */
#ifdef __ANDROID__
#define GL_ES 1
#else
#define GL_ES 0
#endif
int gGsNoBcTextures; /* gs_draw.c: the renderer cannot take BC1-BC3 (DXT) textures: a pack's .dds are unpacked to RGBA */

/* ---- GL 3.3 core entry points (loaded through SDL_GL_GetProcAddress) ------------------------------ */
typedef unsigned int GLenum; typedef unsigned int GLuint; typedef int GLint; typedef int GLsizei;
typedef unsigned char GLboolean; typedef char GLchar; typedef float GLfloat; typedef unsigned int GLbitfield;
typedef void GLvoid; typedef ptrdiff_t GLsizeiptr; typedef ptrdiff_t GLintptr; typedef unsigned char GLubyte;

#define GL_FUNCS(X) \
    X(glGetError, GLenum, (void), ()) \
    X(glEnable, void, (GLenum), (GLenum)) \
    X(glDisable, void, (GLenum), (GLenum)) \
    X(glEnablei, void, (GLenum, GLuint), (GLenum, GLuint)) \
    X(glDisablei, void, (GLenum, GLuint), (GLenum, GLuint)) \
    X(glViewport, void, (GLint, GLint, GLsizei, GLsizei), (GLint, GLint, GLsizei, GLsizei)) \
    X(glScissor, void, (GLint, GLint, GLsizei, GLsizei), (GLint, GLint, GLsizei, GLsizei)) \
    X(glClearColor, void, (GLfloat, GLfloat, GLfloat, GLfloat), (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(glClear, void, (GLbitfield), (GLbitfield)) \
    X(glClearBufferfv, void, (GLenum, GLint, const GLfloat *), (GLenum, GLint, const GLfloat *)) \
    X(glGetString, const GLubyte *, (GLenum), (GLenum)) \
    X(glGetIntegerv, void, (GLenum, GLint *), (GLenum, GLint *)) \
    X(glFinish, void, (void), ()) \
    X(glPixelStorei, void, (GLenum, GLint), (GLenum, GLint)) \
    X(glBlendFuncSeparate, void, (GLenum, GLenum, GLenum, GLenum), (GLenum, GLenum, GLenum, GLenum)) \
    X(glBlendEquationSeparate, void, (GLenum, GLenum), (GLenum, GLenum)) \
    X(glBlendColor, void, (GLfloat, GLfloat, GLfloat, GLfloat), (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(glColorMaski, void, (GLuint, GLboolean, GLboolean, GLboolean, GLboolean), (GLuint, GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(glColorMask, void, (GLboolean, GLboolean, GLboolean, GLboolean), (GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(glDepthFunc, void, (GLenum), (GLenum)) \
    X(glStencilFunc, void, (GLenum, GLint, GLuint), (GLenum, GLint, GLuint)) \
    X(glStencilOp, void, (GLenum, GLenum, GLenum), (GLenum, GLenum, GLenum)) \
    X(glStencilMask, void, (GLuint), (GLuint)) \
    X(glClearStencil, void, (GLint), (GLint)) \
    X(glDepthMask, void, (GLboolean), (GLboolean)) \
    X(glCreateShader, GLuint, (GLenum), (GLenum)) \
    X(glShaderSource, void, (GLuint, GLsizei, const GLchar *const *, const GLint *), (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
    X(glCompileShader, void, (GLuint), (GLuint)) \
    X(glGetShaderiv, void, (GLuint, GLenum, GLint *), (GLuint, GLenum, GLint *)) \
    X(glGetShaderInfoLog, void, (GLuint, GLsizei, GLsizei *, GLchar *), (GLuint, GLsizei, GLsizei *, GLchar *)) \
    X(glDeleteShader, void, (GLuint), (GLuint)) \
    X(glCreateProgram, GLuint, (void), ()) \
    X(glAttachShader, void, (GLuint, GLuint), (GLuint, GLuint)) \
    X(glLinkProgram, void, (GLuint), (GLuint)) \
    X(glGetProgramiv, void, (GLuint, GLenum, GLint *), (GLuint, GLenum, GLint *)) \
    X(glGetProgramInfoLog, void, (GLuint, GLsizei, GLsizei *, GLchar *), (GLuint, GLsizei, GLsizei *, GLchar *)) \
    X(glUseProgram, void, (GLuint), (GLuint)) \
    X(glDeleteProgram, void, (GLuint), (GLuint)) \
    X(glGetUniformLocation, GLint, (GLuint, const GLchar *), (GLuint, const GLchar *)) \
    X(glUniform1i, void, (GLint, GLint), (GLint, GLint)) \
    X(glUniform2f, void, (GLint, GLfloat, GLfloat), (GLint, GLfloat, GLfloat)) \
    X(glGetUniformBlockIndex, GLuint, (GLuint, const GLchar *), (GLuint, const GLchar *)) \
    X(glUniformBlockBinding, void, (GLuint, GLuint, GLuint), (GLuint, GLuint, GLuint)) \
    X(glBindBufferBase, void, (GLenum, GLuint, GLuint), (GLenum, GLuint, GLuint)) \
    X(glBindBufferRange, void, (GLenum, GLuint, GLuint, GLintptr, GLsizeiptr), (GLenum, GLuint, GLuint, GLintptr, GLsizeiptr)) \
    X(glMapBufferRange, void *, (GLenum, GLintptr, GLsizeiptr, GLbitfield), (GLenum, GLintptr, GLsizeiptr, GLbitfield)) \
    X(glUnmapBuffer, GLboolean, (GLenum), (GLenum)) \
    X(glGenBuffers, void, (GLsizei, GLuint *), (GLsizei, GLuint *)) \
    X(glBindBuffer, void, (GLenum, GLuint), (GLenum, GLuint)) \
    X(glBufferData, void, (GLenum, GLsizeiptr, const void *, GLenum), (GLenum, GLsizeiptr, const void *, GLenum)) \
    X(glBufferSubData, void, (GLenum, GLintptr, GLsizeiptr, const void *), (GLenum, GLintptr, GLsizeiptr, const void *)) \
    X(glGenVertexArrays, void, (GLsizei, GLuint *), (GLsizei, GLuint *)) \
    X(glBindVertexArray, void, (GLuint), (GLuint)) \
    X(glEnableVertexAttribArray, void, (GLuint), (GLuint)) \
    X(glVertexAttribPointer, void, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *), (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
    X(glDrawArrays, void, (GLenum, GLint, GLsizei), (GLenum, GLint, GLsizei)) \
    X(glDrawElements, void, (GLenum, GLsizei, GLenum, const void *), (GLenum, GLsizei, GLenum, const void *)) \
    X(glGenSamplers, void, (GLsizei, GLuint *), (GLsizei, GLuint *)) \
    X(glBindSampler, void, (GLuint, GLuint), (GLuint, GLuint)) \
    X(glSamplerParameteri, void, (GLuint, GLenum, GLint), (GLuint, GLenum, GLint)) \
    X(glGenTextures, void, (GLsizei, GLuint *), (GLsizei, GLuint *)) \
    X(glBindTexture, void, (GLenum, GLuint), (GLenum, GLuint)) \
    X(glActiveTexture, void, (GLenum), (GLenum)) \
    X(glTexImage2D, void, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *), (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    X(glTexSubImage2D, void, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *), (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
    X(glCompressedTexImage2D, void, (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *), (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *)) \
    X(glTexParameteri, void, (GLenum, GLenum, GLint), (GLenum, GLenum, GLint)) \
    X(glDeleteTextures, void, (GLsizei, const GLuint *), (GLsizei, const GLuint *)) \
    X(glGenFramebuffers, void, (GLsizei, GLuint *), (GLsizei, GLuint *)) \
    X(glBindFramebuffer, void, (GLenum, GLuint), (GLenum, GLuint)) \
    X(glFramebufferTexture2D, void, (GLenum, GLenum, GLenum, GLuint, GLint), (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(glCheckFramebufferStatus, GLenum, (GLenum), (GLenum)) \
    X(glDeleteFramebuffers, void, (GLsizei, const GLuint *), (GLsizei, const GLuint *)) \
    X(glDrawBuffers, void, (GLsizei, const GLenum *), (GLsizei, const GLenum *)) \
    X(glReadBuffer, void, (GLenum), (GLenum)) \
    X(glBlitFramebuffer, void, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum), (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
    X(glReadPixels, void, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *), (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *))

#define GL_DECL(name, ret, args, callargs) static ret (*name) args;
GL_FUNCS(GL_DECL)
#undef GL_DECL

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_TRIANGLES 0x0004
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_RED 0x1903
#define GL_COLOR 0x1800
#define GL_DEPTH 0x1801
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_STENCIL 0x84F9
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_R8 0x8229
#define GL_DEPTH32F_STENCIL8 0x8CAD
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV 0x8DAD
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_ARRAY_BUFFER 0x8892
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_UNSIGNED_INT
#define GL_UNSIGNED_INT 0x1405
#endif
#define GL_STREAM_DRAW 0x88E0
#define GL_DYNAMIC_DRAW 0x88E8
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
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_SCISSOR_TEST 0x0C11
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_CLAMP 0x864F
#define GL_LOWER_LEFT 0x8CA1
#define GL_ZERO_TO_ONE 0x935F
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_CONSTANT_COLOR 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR 0x8002
#define GL_FUNC_ADD 0x8006
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_NEVER 0x0200
#define GL_GREATER 0x0204
#define GL_EQUAL 0x0202
#define GL_GEQUAL 0x0206
#define GL_KEEP 0x1E00
#define GL_INCR 0x1E02
#define GL_ALWAYS 0x0207
#define GL_FRAMEBUFFER 0x8D40
#define GL_BACK 0x0405
#define GL_FRONT 0x0404
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_INVALID_INDEX 0xFFFFFFFFu

static void (*glClipControl)(GLenum, GLenum); /* optional: GL 4.5 / ARB_clip_control */
static int sHasClipControl;

static int load_gl(void) {
#define LOAD(name, ret, args, callargs) \
    name = (ret (*) args)SDL_GL_GetProcAddress(#name); \
    if (!name) { fprintf(stderr, "bt3: gl: missing GL entry point %s\n", #name); return 0; }
    GL_FUNCS(LOAD)
#undef LOAD
    return 1;
}

#include "gl_translate.h"

static int sGlFailed; /* a shader did not compile or link (Android: the renderer gives up, see gl_init) */
static GLuint compile(GLenum stage, const char *src, const char *tag) {
    GLuint sh = glCreateShader(stage);
    GLint ok = 0;
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint n = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &n);
        char *log = malloc((size_t)n + 1);
        glGetShaderInfoLog(sh, n, NULL, log);
        fprintf(stderr, "bt3: gl: %s compile FAILED\n%s\n", tag, log);
        free(log);
#if GL_ES
        sGlFailed = 1; /* (Android: gl_init gives up and the Vulkan renderer is started instead) */
        glDeleteShader(sh);
        return 0;
#else
        exit(1);
#endif
    }
    return sh;
}

static GLuint make_program(const unsigned char *vsrc, const unsigned char *fsrc, const char *tag, const char *const *samplers, int ns, int nodiscard) {
    char *vt = gl_translate((const char *)vsrc, 1, GL_ES, 0, sHasClipControl), *ft = gl_translate((const char *)fsrc, 0, GL_ES, nodiscard, sHasClipControl);
    GLuint v = compile(GL_VERTEX_SHADER, vt, tag), f = compile(GL_FRAGMENT_SHADER, ft, tag), p;
    GLint ok = 0, i;
    free(vt); free(ft);
    p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint n = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &n);
        char *log = malloc((size_t)n + 1);
        glGetProgramInfoLog(p, n, NULL, log);
        fprintf(stderr, "bt3: gl: %s link FAILED\n%s\n", tag, log);
        free(log);
#if GL_ES
        sGlFailed = 1;
        return 0;
#else
        exit(1);
#endif
    }
    glDeleteShader(v);
    glDeleteShader(f);
    glUseProgram(p);
    for (i = 0; i < ns; i++) {
        glUniform1i(glGetUniformLocation(p, samplers[i]), i);
    }
    {   /* U (vertex programs' constants) -> binding 0; Params (draws' state) -> binding 1; never the same */
        GLuint idx = glGetUniformBlockIndex(p, "U");
        if (idx != GL_INVALID_INDEX) { glUniformBlockBinding(p, idx, 0); }
        idx = glGetUniformBlockIndex(p, "Params");
        if (idx != GL_INVALID_INDEX) { glUniformBlockBinding(p, idx, 1); }
    }
    return p;
}

/* The present: one textured full-screen triangle sampling the shown buffer's colour, drawn with the viewport
   set to the letterboxed rectangle. (A textured quad, as the OpenGL framebuffer guide does it, instead of a
   scaled/flipped blit.) */
static GLuint make_present_program(void) {
#if GL_ES
#define PRESENT_HEAD "#version 300 es\nprecision highp float;\n"
#else
#define PRESENT_HEAD "#version 330 core\n"
#endif
    static const char *vs = PRESENT_HEAD "out vec2 vUv;\nvoid main() {\n"
        "    vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);\n"
        "    vUv = vec2((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);\n"  /* y flipped: the scene's top row to the window's top */
        "    gl_Position = vec4(p, 0.0, 1.0);\n}\n";
    static const char *fs = PRESENT_HEAD "in vec2 vUv;\nout vec4 o;\nuniform sampler2D tex;\nuniform vec2 uvScale;\n"
        "void main() { o = texture(tex, vUv * uvScale); }\n";
    GLuint v = compile(GL_VERTEX_SHADER, vs, "present.vs"), f = compile(GL_FRAGMENT_SHADER, fs, "present.fs"), p;
    GLint ok = 0;
    p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        fprintf(stderr, "bt3: gl: present link FAILED\n");
#if GL_ES
        sGlFailed = 1;
        return 0;
#else
        exit(1);
#endif
    }
    glDeleteShader(v);
    glDeleteShader(f);
    glUseProgram(p);
    glUniform1i(glGetUniformLocation(p, "tex"), 0);
    glUniform2f(glGetUniformLocation(p, "uvScale"), 512.0f / (float)GS_W, 448.0f / (float)GS_H);
    return p;
}

/* ---- state -------------------------------------------------------------------------------------- */
static SDL_Window *sWindow;
static SDL_GLContext sCtx;
static GLuint sMainProg[9]; /* gs.vert / vu0 / vu4 / vu6 + gs.frag; 4..7: the same with gsnz.frag (key bit 23); 8: gs.vert + gswb.frag (bit 25) */
extern int Port_Setting(const char *name, int def); /* plat_settings.c */
static GLuint sMainProgFetch[8]; /* the same for the draws that test the destination alpha, reading it from the frame buffer (sHasFetch) */
static int sHasFetch;     /* EXT_shader_framebuffer_fetch: no copy of the alpha bytes for TEST.DATE (tile-based GPUs keep their pass) */
static int sWantFetch;    /* the draw being set up tests the destination alpha */
static GLuint sMainProgNd[8]; /* the same with the fragment shader without its discards (pipeline key bit 27, as the Vulkan back end) */
static GLuint sOutlineProg, sKeyProg, sDclutProg, sPresentProg;
static GLuint sVaoGs, sVaoVu, sVboGs, sVboVu, sIboVu;
static GLuint sSamplers[16];
static GLuint sWhite;
static GLuint sUboU, sUboParams;
static GLuint sTgCol[MAX_TARGETS], sTgAux[MAX_TARGETS], sTgDep[MAX_TARGETS], sTgFbo[MAX_TARGETS];
/* the same target without its depth buffer: for the runs of draws that neither test nor write depth (most of a
   fight's: the effects' work buffers and the 2D), which a tiled GPU then neither loads nor stores (see gs_gpu.c) */
static GLuint sTgFboNd[MAX_TARGETS];
static GLuint sDateCopy, sAuxCopy;
static GLuint sScratchRead, sScratchDraw;
static GLuint sFbTex[2], sFbFbo[2];
static int sHasIndexed;
static const char *sGsSamplers[2] = {"tex", "dateTex"};
static const char *sOneSampler[1] = {"aux"};
static const char *sDclutSamplers[3] = {"depthTex", "auxTex", "clutTex"};

typedef struct Pipe { uint32_t key, bkey, wmask; int ztst, zwrite, topo, vu, fx, nd, nz, wb, shadow; } Pipe;
static Pipe sPipes[1024];
static int sPipeCount;

static void target_ensure(int i);
static void target_drop(int i);
static void set_pipe(int pipe);
static void bind_target(int i, int clear, int nodepth);

/* ---- the attachments of the shared targets ------------------------------------------------------ */
static void target_ensure(int i) {
    GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glGenTextures(1, &sTgCol[i]);
    glBindTexture(GL_TEXTURE_2D, sTgCol[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GS_W * SCALE, GS_H * SCALE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenTextures(1, &sTgAux[i]);
    glBindTexture(GL_TEXTURE_2D, sTgAux[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, GS_W * SCALE, GS_H * SCALE, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glGenTextures(1, &sTgDep[i]);
    glBindTexture(GL_TEXTURE_2D, sTgDep[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, GS_W * SCALE, GS_H * SCALE, 0,
                 GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &sTgFbo[i]);
    glBindFramebuffer(GL_FRAMEBUFFER, sTgFbo[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sTgCol[i], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, sTgAux[i], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, sTgDep[i], 0);
    glDrawBuffers(2, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "bt3: gl: target %d framebuffer incomplete\n", i);
    }
    glGenFramebuffers(1, &sTgFboNd[i]);
    glBindFramebuffer(GL_FRAMEBUFFER, sTgFboNd[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sTgCol[i], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, sTgAux[i], 0);
    glDrawBuffers(2, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &sTgFboNd[i]);
        sTgFboNd[i] = 0; /* (the target is then always bound with its depth) */
    }
}

static void target_drop(int i) {
    glDeleteTextures(1, &sTgCol[i]);
    glDeleteTextures(1, &sTgAux[i]);
    glDeleteTextures(1, &sTgDep[i]);
    glDeleteFramebuffers(1, &sTgFbo[i]);
    if (sTgFboNd[i] != 0) {
        glDeleteFramebuffers(1, &sTgFboNd[i]);
    }
    sTgCol[i] = sTgAux[i] = sTgDep[i] = sTgFbo[i] = sTgFboNd[i] = 0;
}

static GsTex target_color(int i) { return (GsTex)sTgCol[i]; }
static GsTex target_aux(int i) { return (GsTex)sTgAux[i]; }
static GsTex target_depth(int i) { return (GsTex)sTgDep[i]; }

/* Makes a texture; its levels come with tex_upload. GL_TEXTURE_MAX_LEVEL bounds the chain so that a mipmapped
   replacement is complete even if the pack's chain stops short of 1x1. */
static GsTex tex_create(uint32_t w, uint32_t h, int format, int levels) {
    GLuint t;
    (void)w; (void)h; (void)format;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels > 0 ? levels - 1 : 0);
    return (GsTex)t;
}

/* Uploads a level at once (GL has no upload queue; the pixels are copied). Takes ownership of px. */
static int tex_upload(GsTex handle, int level, uint32_t w, uint32_t h, int format, const void *px, uint32_t bytes) {
    GLuint t = (GLuint)handle;
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (format == GS_TEXFMT_RGBA8) {
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    } else {
        GLenum ifmt = format == GS_TEXFMT_BC1 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT :
                      format == GS_TEXFMT_BC2 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT :
                      format == GS_TEXFMT_ASTC4 ? 0x93B0 /* GL_COMPRESSED_RGBA_ASTC_4x4_KHR */ : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        glCompressedTexImage2D(GL_TEXTURE_2D, level, ifmt, (GLsizei)w, (GLsizei)h, 0, (GLsizei)bytes, px);
    }
    free((void *)px);
    return 1;
}

static void tex_destroy(GsTex tex) { GLuint t = (GLuint)tex; glDeleteTextures(1, &t); }
static int tex_slots_left(void) { return 1 << 20; }
static GsTex white_tex(void) { return (GsTex)sWhite; }

static void copies_create(void) {
    glGenTextures(1, &sDateCopy);
    glBindTexture(GL_TEXTURE_2D, sDateCopy);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, GS_W * SCALE, GS_H * SCALE, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glGenTextures(1, &sAuxCopy);
    glBindTexture(GL_TEXTURE_2D, sAuxCopy);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, GS_W * SCALE, GS_H * SCALE, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
}

static void scale_changed(void) {
    int i;
    glFinish();
    for (i = 0; i < gsTargetCount; i++) {
        target_drop(i);
    }
    glDeleteTextures(1, &sDateCopy);
    glDeleteTextures(1, &sAuxCopy);
    sDateCopy = sAuxCopy = 0;
    copies_create();
}

/* ---- pipelines ---------------------------------------------------------------------------------- */
static void pipe_decode(Pipe *p, uint32_t key) {
    p->key = key;
    p->bkey = key & 0x3FF;
    p->ztst = (int)((key >> 10) & 3);
    p->zwrite = (int)((key >> 12) & 1);
    p->topo = (int)((key >> 13) & 3);
    p->vu = (int)((key >> 20) & 3);
    p->nz = (int)((key >> 23) & 1);
    p->wb = (int)((key >> 25) & 1);
    p->shadow = (int)((key >> 24) & 1);
    p->fx = (int)((key >> 22) & 1);
    p->wmask = (key >> 16) & 15;
    p->nd = (int)((key >> 27) & 1);
}

static int pipe_get(uint32_t key) {
    int i;
    for (i = 0; i < sPipeCount; i++) {
        if (sPipes[i].key == key) { return i; }
    }
    if (sPipeCount == 1024) { return 0; }
    pipe_decode(&sPipes[sPipeCount], key);
    return sPipeCount++;
}

static GLuint sCurProgram;
static int sCurPipe = -1;
static GLuint sCurTex0, sCurTex1, sCurTex2;
static int sCurSamp0, sCurSamp1;
static int haveScissor = 0;
static int lastScissor[4];
static float lastBlendc = -1.0f;

static void apply_blend(uint32_t bkey) {
    int k[2] = {(int)((bkey >> 1) & 7), (int)((bkey >> 4) & 7)}, C = (int)((bkey >> 7) & 3), i;
    GLenum fc = C == 0 ? GL_SRC_ALPHA : C == 1 ? GL_DST_ALPHA : GL_CONSTANT_COLOR;
    GLenum fi = C == 0 ? GL_ONE_MINUS_SRC_ALPHA : C == 1 ? GL_ONE_MINUS_DST_ALPHA : GL_ONE_MINUS_CONSTANT_COLOR;
    GLenum f[2];
    int enabled = (bkey & 1) != 0;
    if (sHasIndexed) {
        if (enabled) { glEnablei(GL_BLEND, 0); glDisablei(GL_BLEND, 1); } else { glDisablei(GL_BLEND, 0); glDisablei(GL_BLEND, 1); }
    } else if (enabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    if (!enabled) { return; }
    for (i = 0; i < 2; i++) {
        f[i] = k[i] == 0 ? GL_ZERO : k[i] == 1 || k[i] == 5 ? GL_ONE : k[i] == 4 ? fi : fc;
    }
    if (bkey & (1u << 9)) {
        f[0] = GL_ONE; /* (the shader scaled the source by its alpha: gs_draw.c, blend_key) */
    }
    glBlendFuncSeparate(f[0], f[1], GL_ONE, GL_ZERO);
    glBlendEquationSeparate(k[0] == 3 ? GL_FUNC_REVERSE_SUBTRACT : k[1] == 3 ? GL_FUNC_SUBTRACT : GL_FUNC_ADD, GL_FUNC_ADD);
}

static void apply_masks(uint32_t wmask) {
    if (sHasIndexed) {
        glColorMaski(0, (wmask & GS_CC_R) != 0, (wmask & GS_CC_G) != 0, (wmask & GS_CC_B) != 0, (wmask & GS_CC_A) != 0);
        glColorMaski(1, (wmask & GS_CC_A) != 0, GL_FALSE, GL_FALSE, GL_FALSE);
    } else {
        glColorMask((wmask & GS_CC_R) != 0, (wmask & GS_CC_G) != 0, (wmask & GS_CC_B) != 0, (wmask & GS_CC_A) != 0);
    }
}

#if GL_ES
/* The uniform blocks: a fresh slice of a large buffer per draw, written without waiting (unsynchronized) and bound
   by range. Rewriting one small buffer for every draw (2 000 a fight frame) made a Mali GPU's driver copy or wait
   on the buffer each time: 2 to 3 pictures a second in a fight with OpenGL ES on a Mali-G52, 60 in the menus.
   The buffer is given fresh storage at each frame's start (ubo_frame), so nothing the GPU still reads is
   written over. */
#define UBO_RING (4 << 20)
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0004
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020
#define GL_MAP_PERSISTENT_BIT_EXT 0x0040
#define GL_MAP_COHERENT_BIT_EXT 0x0080
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_SYNC_FLUSH_COMMANDS_BIT 0x00000001
#define GL_WAIT_FAILED 0x911D
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT 0x8A34
static GLint sUboAlign = 256;
static GLintptr sUboOff[2];

/* The same, on drivers whose unsynchronized maps are slow or broken (ARMSX2's GPU driver table: every Mali OpenGL ES
   driver, "buffer streaming / unsynchronized mapping"): one buffer per block kind, mapped ONCE for good
   (EXT_buffer_storage, persistent and coherent) and written in order like a ring. It is cut into UBO_SEGS pieces;
   a fence goes in when the writing leaves a piece, and a piece is written again only after its fence has passed, so
   the GPU never reads a byte that is being overwritten. No map, unmap or orphan per draw. Setting "gl_persist":
   0 automatic (Mali and Adreno with EXT_buffer_storage), 1 always where the extension is, 2 never; BT3_GL_PERSIST=0/1.
   Adreno: the map per draw stalled the Adreno 650's driver (a fight at 2x: about 4 s a picture; with the ring 100 %
   speed, 14 ms of work a picture). Mali-G57 MC4: the same speed either way (92.5 % and 93.8 % at 2x). */
#define UBO_SEGS 8
#define UBO_SEG (1 << 20)
typedef struct __GLsync *GLsync_;
static void (*glBufferStorageEXT_)(GLenum, GLsizeiptr, const void *, GLbitfield);
static GLsync_ (*glFenceSync_)(GLenum, GLbitfield);
static GLenum (*glClientWaitSync_)(GLsync_, GLbitfield, uint64_t);
static void (*glDeleteSync_)(GLsync_);
static struct { uint8_t *ptr; int seg; GLintptr off; GLsync_ fence[UBO_SEGS]; } sPer[2];
static int sPersist;
static unsigned sPersistWaits;

static void ubo_persist_init(GLuint bufU, GLuint bufP) {
    const char *r = (const char *)glGetString(GL_RENDERER);
    const char *e = getenv("BT3_GL_PERSIST");
    int want = Port_Setting("gl_persist", 0), tiled = r != NULL && (strstr(r, "Mali") != NULL || strstr(r, "Immortalis") != NULL ||
                                                                    strstr(r, "Adreno") != NULL);
    int on = e != NULL ? atoi(e) != 0 : want == 1 ? 1 : want == 2 ? 0 : tiled;
    GLuint bufs[2] = {bufU, bufP};
    int k;
    sPersist = 0;
    if (!on || !SDL_GL_ExtensionSupported("GL_EXT_buffer_storage")) {
        fprintf(stderr, "bt3: gl: uniform blocks: orphaned per frame%s\n", on ? " (no EXT_buffer_storage)" : "");
        return;
    }
    glBufferStorageEXT_ = (void (*)(GLenum, GLsizeiptr, const void *, GLbitfield))SDL_GL_GetProcAddress("glBufferStorageEXT");
    glFenceSync_ = (GLsync_ (*)(GLenum, GLbitfield))SDL_GL_GetProcAddress("glFenceSync");
    glClientWaitSync_ = (GLenum (*)(GLsync_, GLbitfield, uint64_t))SDL_GL_GetProcAddress("glClientWaitSync");
    glDeleteSync_ = (void (*)(GLsync_))SDL_GL_GetProcAddress("glDeleteSync");
    if (glBufferStorageEXT_ == NULL || glFenceSync_ == NULL || glClientWaitSync_ == NULL || glDeleteSync_ == NULL) {
        fprintf(stderr, "bt3: gl: uniform blocks: orphaned per frame (EXT_buffer_storage without its functions)\n");
        return;
    }
    while (glGetError() != 0) {} /* (errors left from before are not this one's) */
    for (k = 0; k < 2; k++) {
        /* (a buffer given its storage this way can never be given another: these two are made fresh for it) */
        glGenBuffers(1, &bufs[k]);
        glBindBuffer(GL_UNIFORM_BUFFER, bufs[k]);
        glBufferStorageEXT_(GL_UNIFORM_BUFFER, (GLsizeiptr)UBO_SEGS * UBO_SEG, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT);
        sPer[k].ptr = (uint8_t *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, (GLsizeiptr)UBO_SEGS * UBO_SEG,
                                                  GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT);
        sPer[k].seg = 0;
        sPer[k].off = 0;
        memset(sPer[k].fence, 0, sizeof(sPer[k].fence));
        if (sPer[k].ptr == NULL || glGetError() != 0) {
            fprintf(stderr, "bt3: gl: uniform blocks: the persistent buffer could not be made: orphaned per frame\n");
            sPer[0].ptr = sPer[1].ptr = NULL;
            while (glGetError() != 0) {}
            return;
        }
    }
    sUboU = bufs[0];
    sUboParams = bufs[1];
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, sUboU);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, sUboParams);
    sPersist = 1;
    fprintf(stderr, "bt3: gl: uniform blocks: persistent ring (EXT_buffer_storage, %d x %d KB)\n", UBO_SEGS, UBO_SEG >> 10);
}

static void ubo_orphan(GLuint buf) {
    glBindBuffer(GL_UNIFORM_BUFFER, buf);
    glBufferData(GL_UNIFORM_BUFFER, UBO_RING, NULL, GL_STREAM_DRAW);
}
static void ubo_frame(void) {
    if (sPersist) {
        return; /* (the ring's fences keep what the GPU reads; nothing to do at a frame's start) */
    }
    ubo_orphan(sUboU);
    ubo_orphan(sUboParams);
    sUboOff[0] = sUboOff[1] = 0;
}
static void ubo_put_persist(int which, GLuint buf, GLuint binding, const void *p, size_t n, size_t block) {
    GLintptr off = (sPer[which].off + sUboAlign - 1) / sUboAlign * sUboAlign;
    int s = sPer[which].seg;
    if (off + (GLintptr)block > UBO_SEG) { /* this piece is full: fence it, go on to the next once the GPU is done with it */
        sPer[which].fence[s] = glFenceSync_(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        s = (s + 1) % UBO_SEGS;
        if (sPer[which].fence[s] != NULL) {
            if (glClientWaitSync_(sPer[which].fence[s], GL_SYNC_FLUSH_COMMANDS_BIT, 2000000000ull) == GL_WAIT_FAILED) {
                fprintf(stderr, "bt3: gl: uniform ring: a fence failed\n");
            }
            glDeleteSync_(sPer[which].fence[s]);
            sPer[which].fence[s] = NULL;
            sPersistWaits++;
        }
        sPer[which].seg = s;
        off = 0;
    }
    memcpy(sPer[which].ptr + (GLintptr)s * UBO_SEG + off, p, n);
    glBindBufferRange(GL_UNIFORM_BUFFER, binding, buf, (GLintptr)s * UBO_SEG + off, (GLsizeiptr)block);
    sPer[which].off = off + (GLintptr)block;
}
static void ubo_put(int which, GLuint buf, GLuint binding, const void *p, size_t n, size_t block) {
    GLintptr off = (sUboOff[which] + sUboAlign - 1) / sUboAlign * sUboAlign;
    void *m;
    if (sPersist) {
        ubo_put_persist(which, buf, binding, p, n, block);
        return;
    }
    if (off + (GLintptr)block > UBO_RING) {
        ubo_orphan(buf);
        off = 0;
    }
    glBindBuffer(GL_UNIFORM_BUFFER, buf);
    m = glMapBufferRange(GL_UNIFORM_BUFFER, off, (GLsizeiptr)n, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    if (m != NULL) {
        memcpy(m, p, n);
        glUnmapBuffer(GL_UNIFORM_BUFFER);
    } else {
        glBufferSubData(GL_UNIFORM_BUFFER, off, (GLsizeiptr)n, p);
    }
    glBindBufferRange(GL_UNIFORM_BUFFER, binding, buf, off, (GLsizeiptr)block);
    sUboOff[which] = off + (GLintptr)block;
}
static void set_params(const void *p, size_t n) {
    ubo_put(1, sUboParams, 1, p, n, 80);
}
static void set_uniform_u(const Vu0Uniform *u) {
    ubo_put(0, sUboU, 0, u, sizeof(Vu0Uniform), sizeof(Vu0Uniform));
}
#else
static void set_params(const void *p, size_t n) {
    glBindBuffer(GL_UNIFORM_BUFFER, sUboParams);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, (GLsizeiptr)n, p);
}

static void set_uniform_u(const Vu0Uniform *u) {
    glBindBuffer(GL_UNIFORM_BUFFER, sUboU);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vu0Uniform), u);
}
#endif

static void set_pipe(int pipe) {
    Pipe *p;
    if (pipe * 2 + sWantFetch == sCurPipe) { return; }
    sCurPipe = pipe * 2 + sWantFetch;
    p = &sPipes[pipe];
    {
        int pi = p->vu + (p->nz ? 4 : 0);
        GLuint prog = p->fx ? sDclutProg : p->wb ? sMainProg[8] : sWantFetch && sMainProgFetch[pi] != 0 ? sMainProgFetch[pi] :
                      p->nd && sMainProgNd[pi] != 0 ? sMainProgNd[pi] : sMainProg[pi];
        if (sCurProgram != prog) { sCurProgram = prog; glUseProgram(prog); }
    }
    apply_blend(p->bkey);
    apply_masks(p->wmask);
    if (p->fx) {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDepthMask(GL_FALSE);
    } else {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(p->zwrite ? GL_TRUE : GL_FALSE);
        glDepthFunc(p->ztst == 0 ? GL_NEVER : p->ztst == 1 ? GL_ALWAYS : p->ztst == 2 ? GL_GEQUAL : GL_GREATER);
        if (p->shadow) {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_EQUAL, 0, 0xff);
            glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
            glStencilMask(0xff);
        } else {
            glDisable(GL_STENCIL_TEST);
        }
    }
}

static void set_native(int effect) {
    sCurPipe = -1;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_FALSE);
    if (sHasIndexed) { glEnablei(GL_BLEND, 0); glDisablei(GL_BLEND, 1); } else { glEnable(GL_BLEND); }
    if (effect == 2) {
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    } else {
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ZERO);
        glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
    }
    if (sHasIndexed) {
        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    } else {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    }
    if (sCurProgram != (effect == 2 ? sKeyProg : sOutlineProg)) {
        sCurProgram = effect == 2 ? sKeyProg : sOutlineProg;
        glUseProgram(sCurProgram);
    }
}

static void set_scissor(const int *sc) {
    if (!haveScissor || memcmp(sc, lastScissor, sizeof(lastScissor)) != 0) {
        glScissor(sc[0], sc[1], sc[2], sc[3]);
        memcpy(lastScissor, sc, sizeof(lastScissor));
        haveScissor = 1;
    }
}

static void set_textures(GLuint t0, int s0, GLuint t1, int s1, GLuint t2, int s2) {
    if (sCurTex0 != t0 || sCurSamp0 != s0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, t0);
        glBindSampler(0, sSamplers[s0]);
        sCurTex0 = t0;
        sCurSamp0 = s0;
    }
    if (sCurTex1 != t1 || sCurSamp1 != s1) {
        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, t1);
        glBindSampler(1, sSamplers[s1]);
        sCurTex1 = t1;
        sCurSamp1 = s1;
    }
    if (t2 != 0 && sCurTex2 != t2) {
        glActiveTexture(GL_TEXTURE0 + 2);
        glBindTexture(GL_TEXTURE_2D, t2);
        glBindSampler(2, sSamplers[6]);
        sCurTex2 = t2;
    }
}

static unsigned sBindN, sBlitN, sFrameN; /* the frames' target changes and copies since the last heartbeat (the log) */
static unsigned sNoDepthN; /* of those target changes, the ones bound without the depth buffer */
static int depthless = -1; /* framebuffers without depth where it is not used (the frame's replay) */

static void bind_target(int i, int clear, int nodepth) {
    GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    sBindN++;
    glBindFramebuffer(GL_FRAMEBUFFER, nodepth && !clear && sTgFboNd[i] != 0 ? sTgFboNd[i] : sTgFbo[i]);
    glDrawBuffers(2, bufs);
    glViewport(0, 0, GS_W * SCALE, GS_H * SCALE);
    if (clear) {
        /* as the Vulkan back end clears: colour and alpha 0 (alpha 1 only with the magenta aid), the alpha byte 0 */
        float col[4] = {0.0f, 0.0f, 0.0f, 0.0f}, aux[4] = {0.0f, 0.0f, 0.0f, 0.0f}, dep = 0.0f;
        if (getenv("BT3_GPU_MAGENTA") != NULL) { col[0] = 1.0f; col[2] = 1.0f; col[3] = 1.0f; }
        glDisable(GL_SCISSOR_TEST);
        /* a clear obeys the write masks of the draw before it: open them (the caller sets its pipeline again) */
        if (sHasIndexed) { glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); }
        else { glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); }
        glDepthMask(GL_TRUE);
        sCurPipe = -1;
        glClearBufferfv(GL_COLOR, 0, col);
        glClearBufferfv(GL_COLOR, 1, aux);
        glClearBufferfv(GL_DEPTH, 0, &dep);
        glEnable(GL_SCISSOR_TEST);
    }
}

void GsGl_Vsync(int on) { /* the settings window's "V-sync" */
    if (sCtx != NULL) {
        SDL_GL_SetSwapInterval(on != 0);
    }
}

/* ---- init --------------------------------------------------------------------------------------- */
/* The renderer cannot start: the context and the window go, so that the other renderer can make its own (gs_draw.c
   tries Vulkan next when the OpenGL one was only chosen in the settings). */
static void gl_give_up(void) {
    if (sCtx != NULL) {
        SDL_GL_MakeCurrent(sWindow, NULL);
        SDL_GL_DestroyContext(sCtx);
        sCtx = NULL;
    }
    if (sWindow != NULL) {
        SDL_DestroyWindow(sWindow);
        sWindow = NULL;
    }
}

static int gl_init(void) {
    static const uint32_t white = 0xFFFFFFFFu;
    int i;

#if GL_ES
    /* OpenGL ES 3.2. (On Android the context's kind is part of the window's EGL configuration: asked for first.) */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);   /* (the window's own buffer is only presented to) */
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
#endif
    sWindow = GsDraw_WindowCreate(1);
    if (sWindow == NULL) { return 0; }
    if (getenv("BT3_GL_TRACE") != NULL) { fprintf(stderr, "gl: the window is there\n"); }
#if !GL_ES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    if (getenv("BT3_GPU_DEBUG") != NULL) { SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG); }
    if (getenv("BT3_GL_TRACE") != NULL) { fprintf(stderr, "gl: window made, creating the context\n"); }
    sCtx = SDL_GL_CreateContext(sWindow);
    if (getenv("BT3_GL_TRACE") != NULL) { fprintf(stderr, "gl: context %p (%s)\n", (void *)sCtx, sCtx == NULL ? SDL_GetError() : "ok"); }
    if (sCtx == NULL || !SDL_GL_MakeCurrent(sWindow, sCtx)) {
        fprintf(stderr, "bt3: gl: no %s context: %s\n", GL_ES ? "OpenGL ES 3.2" : "GL 3.3 core", SDL_GetError());
        gl_give_up();
        return 0;
    }
#if GL_ES
    {   /* the present mode chosen for Vulkan, as near as GL has it: FIFO (VSync) waits for the screen, the others not */
        extern int Port_Setting(const char *name, int def);
        SDL_GL_SetSwapInterval(Port_Setting("present", 0) == 1 ? 1 : 0);
    }
#else
    SDL_GL_SetSwapInterval(Port_Setting("vsync", 0) != 0);
#endif
    if (getenv("BT3_GL_TRACE") != NULL) { fprintf(stderr, "gl: current, loading the functions\n"); }
    if (!load_gl()) { gl_give_up(); return 0; }
    fprintf(stderr, "bt3: GL renderer: %s (%s)\n", (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION));
    {   /* the meter's and the reports' name of the GPU (as the Vulkan back end prints it) */
        fprintf(stderr, "bt3: GPU renderer: opengl%s\n", GL_ES ? " es" : "");
    }
    sHasIndexed = glEnablei != NULL && glColorMaski != NULL;
#if GL_ES
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &sUboAlign);
    if (sUboAlign <= 0 || sUboAlign > 4096) { sUboAlign = 256; }
#endif
    /* Depth as the Vulkan back end has it: clip-space z in 0..w stored as z / w, and (SDL GPU's default, depth
       clip off) nothing cut at the near and far planes, the depth clamped to 0..1 instead. */
#if GL_ES
    /* ES: glClipControlEXT and depth clamping only where the driver has the extensions (Mali mostly has neither:
       z is moved to GL's range in the shaders, and what comes nearer than the near plane is cut, as with the Vulkan
       renderer on GPUs without depth clamping) */
    if (getenv("BT3_GL_NOCLIPCONTROL") == NULL && SDL_GL_ExtensionSupported("GL_EXT_clip_control")) {
        glClipControl = (void (*)(GLenum, GLenum))SDL_GL_GetProcAddress("glClipControlEXT");
    }
    sHasClipControl = glClipControl != NULL;
    if (sHasClipControl) { glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); }
    if (SDL_GL_ExtensionSupported("GL_EXT_depth_clamp")) { glEnable(GL_DEPTH_CLAMP); }
    gGsNoBcTextures = !SDL_GL_ExtensionSupported("GL_EXT_texture_compression_s3tc");
#if GL_ES
    {   /* ASTC LDR is part of OpenGL ES 3.2 (and KHR_texture_compression_astc_ldr before it): gs_astc.cpp's copies */
        extern int gGsAstcTextures;
        gGsAstcTextures = 1; /* (the context is OpenGL ES 3.2, which has ASTC LDR in its core) */
    }
#endif
    fprintf(stderr, "bt3: gl es: clip control %s, depth clamp %s, DXT textures %s\n", sHasClipControl ? "yes" : "no",
            SDL_GL_ExtensionSupported("GL_EXT_depth_clamp") ? "yes" : "no", gGsNoBcTextures ? "no (a pack's .dds are unpacked on the CPU)" : "yes");
#else
    if (getenv("BT3_GL_NOCLIPCONTROL") == NULL && SDL_GL_ExtensionSupported("GL_ARB_clip_control")) {
        glClipControl = (void (*)(GLenum, GLenum))SDL_GL_GetProcAddress("glClipControl");
    }
    sHasClipControl = glClipControl != NULL;
    if (sHasClipControl) { glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); }
    glEnable(GL_DEPTH_CLAMP);
#endif
    if (!sHasIndexed) { fprintf(stderr, "bt3: gl: no per-attachment blend/mask (GL 3.0 indexed): the aux byte may be wrong\n"); }

    sMainProg[0] = make_program(kGsVertGlsl, kGsFragGlsl, "gs", sGsSamplers, 2, 0);
    sMainProg[1] = make_program(kVu0VertGlsl, kGsFragGlsl, "vu0", sGsSamplers, 2, 0);
    sMainProg[2] = make_program(kVu4VertGlsl, kGsFragGlsl, "vu4", sGsSamplers, 2, 0);
    sMainProg[3] = make_program(kVu6VertGlsl, kGsFragGlsl, "vu6", sGsSamplers, 2, 0);
    sMainProg[4] = make_program(kGsVertGlsl, kGsnzFragGlsl, "gs nz", sGsSamplers, 2, 0);
    sMainProg[5] = make_program(kVu0VertGlsl, kGsnzFragGlsl, "vu0 nz", sGsSamplers, 2, 0);
    sMainProg[6] = make_program(kVu4VertGlsl, kGsnzFragGlsl, "vu4 nz", sGsSamplers, 2, 0);
    sMainProg[7] = make_program(kVu6VertGlsl, kGsnzFragGlsl, "vu6 nz", sGsSamplers, 2, 0);
    sMainProg[8] = make_program(kGsVertGlsl, kGswbFragGlsl, "gs wb", sGsSamplers, 2, 0);
    sOutlineProg = make_program(kFxVertGlsl, kOutlineFragGlsl, "outline", sOneSampler, 1, 0);
    sKeyProg = make_program(kFxVertGlsl, kAlphakeyFragGlsl, "alphakey", sOneSampler, 1, 0);
    sDclutProg = make_program(kFxVertGlsl, kDclutFragGlsl, "dclut", sDclutSamplers, 3, 0);
    /* the draws that never discard: a shader with `discard` in it keeps tile-based GPUs from dropping hidden pixels
       early (BT3_GPU_DISCARD=1: always the one with discard) */
    if (getenv("BT3_GPU_DISCARD") == NULL) {
        sMainProgNd[0] = make_program(kGsVertGlsl, kGsFragGlsl, "gs-nd", sGsSamplers, 2, 1);
        sMainProgNd[1] = make_program(kVu0VertGlsl, kGsFragGlsl, "vu0-nd", sGsSamplers, 2, 1);
        sMainProgNd[2] = make_program(kVu4VertGlsl, kGsFragGlsl, "vu4-nd", sGsSamplers, 2, 1);
        sMainProgNd[3] = make_program(kVu6VertGlsl, kGsFragGlsl, "vu6-nd", sGsSamplers, 2, 1);
        sMainProgNd[4] = make_program(kGsVertGlsl, kGsnzFragGlsl, "gs nz-nd", sGsSamplers, 2, 1);
        sMainProgNd[5] = make_program(kVu0VertGlsl, kGsnzFragGlsl, "vu0 nz-nd", sGsSamplers, 2, 1);
        sMainProgNd[6] = make_program(kVu4VertGlsl, kGsnzFragGlsl, "vu4 nz-nd", sGsSamplers, 2, 1);
        sMainProgNd[7] = make_program(kVu6VertGlsl, kGsnzFragGlsl, "vu6 nz-nd", sGsSamplers, 2, 1);
    }
#if GL_ES
    /* The destination alpha test (TEST.DATE: the HUD's bars, some of a stage's layers) read a copy of the alpha bytes,
       taken before each run of such draws: on a tile-based GPU (Mali) every copy ends the tiles' pass and starts
       another, and a fight with many of them ran at a fraction of its speed (50 passes a picture on a Mali-G52). With
       EXT_shader_framebuffer_fetch the shader reads the bytes where they are: no copy, no break.
       OFF unless asked for (setting "gl_fetch" = 1, or BT3_GL_FETCH=1; BT3_GL_NOFETCH=1 always off): on two Mali-G52
       (drivers r32p1 and r49p1, 0.7.0) it drew black blocks over the picture and then hung the GPU (no picture came
       back from the driver after 6 to 30 seconds). The copies are what 0.6.0 did. */
    if (getenv("BT3_GL_NOFETCH") == NULL &&
        (getenv("BT3_GL_FETCH") != NULL ? atoi(getenv("BT3_GL_FETCH")) != 0 : Port_Setting("gl_fetch", 0) != 0) &&
        SDL_GL_ExtensionSupported("GL_EXT_shader_framebuffer_fetch") && !sGlFailed) {
        static const unsigned char *const vs[4] = {kGsVertGlsl, kVu0VertGlsl, kVu4VertGlsl, kVu6VertGlsl};
        int k;
        sHasFetch = 1;
        for (k = 0; k < 8 && sHasFetch; k++) {
            sMainProgFetch[k] = make_program(vs[k & 3], k < 4 ? kGsFragGlsl : kGsnzFragGlsl, "fetch", sGsSamplers, 2, 2);
            sHasFetch = sMainProgFetch[k] != 0 && !sGlFailed;
        }
        if (!sHasFetch) {
            sGlFailed = 0; /* (only these could not be made: the copies are used) */
            memset(sMainProgFetch, 0, sizeof(sMainProgFetch));
        }
    }
    fprintf(stderr, "bt3: gl: destination alpha %s\n", sHasFetch ? "read from the frame buffer (framebuffer fetch)" : "from copies");
#endif
    sPresentProg = make_present_program();
    if (sGlFailed) {
        fprintf(stderr, "bt3: gl: the shaders could not be made: this renderer gives up\n");
        gl_give_up();
        return 0;
    }

    glGenBuffers(1, &sVboGs);
    glBindBuffer(GL_ARRAY_BUFFER, sVboGs);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTS * sizeof(Vtx), NULL, GL_STREAM_DRAW);
    glGenVertexArrays(1, &sVaoGs);
    glBindVertexArray(sVaoGs);
    glBindBuffer(GL_ARRAY_BUFFER, sVboGs);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), (void *)12);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void *)16);

    glGenBuffers(1, &sVboVu);
    glBindBuffer(GL_ARRAY_BUFFER, sVboVu);
    glBufferData(GL_ARRAY_BUFFER, MAX_VU_VERTS * 48, NULL, GL_STREAM_DRAW);
    glGenVertexArrays(1, &sVaoVu);
    glBindVertexArray(sVaoVu);
    glBindBuffer(GL_ARRAY_BUFFER, sVboVu);
    for (i = 0; i < 3; i++) {
        glEnableVertexAttribArray((GLuint)i);
        glVertexAttribPointer((GLuint)i, 4, GL_FLOAT, GL_FALSE, 48, (void *)(uintptr_t)(i * 16));
    }
    glGenBuffers(1, &sIboVu); /* the triangles' indices (gsVuIdx); the binding is part of the vertex array's state */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sIboVu);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_VU_IDX * sizeof(uint32_t), NULL, GL_STREAM_DRAW);
    glBindVertexArray(0);

    glGenBuffers(1, &sUboU);
    glBindBuffer(GL_UNIFORM_BUFFER, sUboU);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Vu0Uniform), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, sUboU);
    glGenBuffers(1, &sUboParams);
    glBindBuffer(GL_UNIFORM_BUFFER, sUboParams);
    glBufferData(GL_UNIFORM_BUFFER, 80, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, sUboParams);
#if GL_ES
    ubo_persist_init(sUboU, sUboParams);
    ubo_frame();
#endif

    for (i = 0; i < 16; i++) {
        int lin = (i & 1) != 0;
        glGenSamplers(1, &sSamplers[i]);
        glSamplerParameteri(sSamplers[i], GL_TEXTURE_MAG_FILTER, lin ? GL_LINEAR : GL_NEAREST);
        /* bit 3: a texture pack's replacement, whose smaller copies (mip levels) may be sampled on 3D geometry */
        glSamplerParameteri(sSamplers[i], GL_TEXTURE_MIN_FILTER, (i & 8) ? GL_LINEAR_MIPMAP_LINEAR : (lin ? GL_LINEAR : GL_NEAREST));
        glSamplerParameteri(sSamplers[i], GL_TEXTURE_WRAP_S, (i & 2) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glSamplerParameteri(sSamplers[i], GL_TEXTURE_WRAP_T, (i & 4) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    }
    glGenTextures(1, &sWhite);
    glBindTexture(GL_TEXTURE_2D, sWhite);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    copies_create();
    glGenFramebuffers(1, &sScratchRead);
    glGenFramebuffers(1, &sScratchDraw);

    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glViewport(0, 0, GS_W * SCALE, GS_H * SCALE);
    /* the overlay (F1's settings, the online window) here too: ImGui's OpenGL3 backend, with the context current */
    if (!Ui_Init(sWindow, NULL, (void *)sCtx, (const char *)glGetString(GL_RENDERER))) {
        fprintf(stderr, "bt3: gl: the settings window is not available\n");
    }
    return 1;
}

/* A strip of pre-rendered names (the added stages / songs) as a texture for the overlay (ui.cpp). */
unsigned GsGl_StripTexture(const void *rgba, int w, int h) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return (unsigned)id;
}

void GsGl_StripTextureFree(unsigned id) {
    GLuint t = (GLuint)id;
    glDeleteTextures(1, &t);
}

/* The shown buffer's picture onto the window, letterboxed (the window's own framebuffer, not yet swapped). */
static void present_picture(int best) {
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    {
        int sw, sh;
        SDL_GetWindowSize(sWindow, &sw, &sh);
        SDL_GetWindowSizeInPixels(sWindow, &sw, &sh); /* the drawable's real pixels (differs from the window on HiDPI / fullscreen) */
        glViewport(0, 0, sw, sh);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        if (best >= 0 && gsTargets[best].cleared) {
            float want = (float)Port_PageAspectMilli() / 1000.0f;
            int w = sw, h = sh, dx, dy;
            if ((float)sw > (float)sh * want) { w = (int)((float)sh * want + 0.5f); } else { h = (int)((float)sw / want + 0.5f); }
            dx = (sw - w) / 2;
            dy = (sh - h) / 2;
#if GL_ES
            if (sh > sw + sw / 5) { /* portrait: the picture at the top (GL counts from the bottom) */
                dy = sh - h - sh / 16;
            }
#endif
            if (getenv("BT3_GS_VERBOSE") != NULL) {
                int ww, wh;
                SDL_GetWindowSize(sWindow, &ww, &wh);
                fprintf(stderr, "gl: present window %dx%d pixels %dx%d aspect %d -> picture %dx%d at (%d,%d)\n", ww, wh, sw, sh, Port_AspectMilli(), w, h, dx, dy);
            }
            {   /* where the picture is in the window, for the overlay's stage and song names (ui.cpp), counted from
                   the top as the overlay does */
                extern volatile int gUiPresentX, gUiPresentY, gUiPresentW, gUiPresentH;
                gUiPresentX = dx;
                gUiPresentY = sh - dy - h;
                gUiPresentW = w;
                gUiPresentH = h;
            }
            glViewport(dx, dy, w, h);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glUseProgram(sPresentProg);
            glBindVertexArray(sVaoGs); /* core profile needs a bound VAO to draw (the present uses no attributes) */
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sTgCol[best]);
            glBindSampler(0, sSamplers[7]); /* linear, clamped */
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
}

static int sLastBest = -1; /* the target shown last (shown again while the game is paused) */
extern void GsGpu_PollEvents(void); /* gs_gpu.c */

#ifdef __ANDROID__
/* The log's heartbeat, as the Vulkan renderer's (gs_gpu.c): every 300 frames, what is drawn and how fast the game runs. */
static void gl_heartbeat(void) {
    extern unsigned gPortVBlanks;
    static uint64_t lastNs;
    static unsigned lastBlank, lastFrame;
    uint64_t now;
    extern int gPortInMenus; /* headless.c */
    extern int Port_Setting(const char *name, int def);
    static unsigned hbFrame = ~0u; /* (smooth motion shows a frame twice: one heartbeat for it) */
    if (gGsFrame % 300 != 0 || gGsFrame == hbFrame || Port_Setting("perf_log", 1) == 0) {
        return;
    }
    hbFrame = gGsFrame;
    now = SDL_GetTicksNS();
    fprintf(stderr, "bt3: alive: frame %u, game blank %u, %u draws, %u vertices, %d targets, %d textures\n", gGsFrame, gPortVBlanks,
            gsDrawCount, gsVertCount + gsVuVertCount, gsTargetCount, gsTexCount);
    if (lastNs != 0 && now > lastNs && (int)(gPortVBlanks - lastBlank) >= 0) { /* (the count starts again online) */
        double secs = (double)(now - lastNs) / 1e9, bps = (double)(gPortVBlanks - lastBlank) / secs;
        fprintf(stderr, "bt3: speed %.0f%% (%.1f game blanks/s, %.1f pictures/s over %.1f s%s), OpenGL ES; per frame %.1f target changes, "
                        "%.1f copies, %.1f without depth\n", bps * 100.0 / 60.0, bps, (double)(gGsFrame - lastFrame) / secs, secs,
                Ui_Pauses() ? ", the menu was open" : gPortInMenus ? ", menus" : ", fight", sFrameN ? (double)sBindN / sFrameN : 0.0,
                sFrameN ? (double)sBlitN / sFrameN : 0.0, sFrameN ? (double)sNoDepthN / sFrameN : 0.0);
    }
    lastNs = now;
    lastBlank = gPortVBlanks;
    lastFrame = gGsFrame;
    sBindN = sBlitN = sFrameN = sNoDepthN = 0;
}
#endif

/* While the game is paused (the settings window open, plat_stub.c): the last picture again with the window on it, and
   the window's events. 0: not this renderer, or nothing shown yet (the game goes on). */
int GsGl_PauseFrame(void) {
    if (sCtx == NULL || sLastBest < 0 || sLastBest >= gsTargetCount || sTgFbo[sLastBest] == 0) {
        return 0;
    }
    GsGpu_PollEvents();
    present_picture(sLastBest);
    Ui_DrawGL();
    SDL_GL_SwapWindow(sWindow);
    SDL_Delay(8); /* (nothing moves behind the window: about 100 pictures a second are plenty, and spare the battery) */
    return 1;
}

/* ---- the frame ---------------------------------------------------------------------------------- */
static void frame_end(void) {
    SDL_Event ev;
    int cur = -1, best = -1, i;
    uint32_t n;
    struct { int32_t mode[4]; float misc[4]; float rect[4]; float orig[4]; float extra[4]; } fu, lastFu;
    int haveFu = 0;

#ifdef __ANDROID__
    (void)ev;
    GsGpu_PollEvents(); /* gs_gpu.c: the same events as the Vulkan renderer (back key, START + SELECT, leaving) */
    gl_heartbeat();
#else
    GsGpu_TitlePoll();
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) { exit(0); }
        if (ev.type != SDL_EVENT_MOUSE_BUTTON_DOWN && GsDraw_FullscreenEvent(&ev)) continue;
        { extern void PortA11y_Event(const SDL_Event *ev); PortA11y_Event(&ev); } /* a11y.c: the repeat key and button */
        if (PortInput_Hotkey(&ev)) { Ui_Toggle(); continue; }
        if (Ui_Event(&ev)) { continue; }
        if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && GsDraw_FullscreenEvent(&ev)) continue;
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) { if (!ev.key.repeat && !Ui_QuitAsk()) { exit(0); } /* (asks first) */ }
        if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key == SDLK_F9) { /* in-between pictures on / off */
            extern int gsInterp;
            extern void Port_SettingSave(const char *name, int value);
            extern void Port_SettingsWrite(void);
            static int was = 1; /* (the same setting as F1, Video, Smooth motion: off, or back to what it was) */
            if (gsInterp > 0) { was = gsInterp; gsInterp = 0; } else { gsInterp = was; }
            Port_SettingSave("interp", gsInterp);
            Port_SettingsWrite();
            fprintf(stderr, "bt3: smooth motion %s\n", gsInterp == 0 ? "off" : gsInterp == 1 ? "60" : gsInterp == 2 ? "120" : "240");
        }
    }
#endif
    if (getenv("BT3_GS_VERBOSE") != NULL && gGsFrame % 30 == 0) {
        fprintf(stderr, "gpu: frame %u: %u draws, %u vertices, %d targets, %d textures, %d pipelines, %u primitives of PS2-only passes dropped, %u native effects\n",
                gGsFrame, gsDrawCount, gsVertCount + gsVuVertCount, gsTargetCount, gsTexCount, sPipeCount, gsSkipped, gsNative);
    }
    if (getenv("BT3_GPU_CUT") != NULL && (int)gGsFrame == atoi(getenv("BT3_GPU_CUT")) && strchr(getenv("BT3_GPU_CUT"), ':') != NULL) {
        uint32_t c = (uint32_t)atoi(strchr(getenv("BT3_GPU_CUT"), ':') + 1);
        fprintf(stderr, "cut: frame %u has %u draws, keeping %u\n", gGsFrame, gsDrawCount, c < gsDrawCount ? c : gsDrawCount);
        if (c < gsDrawCount) { gsDrawCount = c; }
    }
    if (getenv("BT3_GPU_TAIL") != NULL && (int)gGsFrame == atoi(getenv("BT3_GPU_TAIL"))) {
        uint32_t k;
        for (k = gsDrawCount > 14 ? gsDrawCount - 14 : 0; k < gsDrawCount; k++) {
            const GsDraw *d = &gsDraws[k];
            fprintf(stderr, "tail: draw %u native %d target %d vu %d first %u count %u pipeline key %08x\n",
                    k, d->native, d->target, d->vu, d->first, d->count, d->native ? 0 : sPipes[d->pipeline].key);
        }
    }

#if GL_ES
    /* the vertices into fresh storage (the last frame's may still be read by the GPU: rewriting them in place made
       the driver copy or wait), the uniform blocks likewise (ubo_frame) */
    ubo_frame();
    if (gsVertCount != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, sVboGs);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(gsVertCount * sizeof(Vtx)), gsVerts, GL_STREAM_DRAW);
    }
    if (gsVuVertCount != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, sVboVu);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(gsVuVertCount * 48), gsVuVerts, GL_STREAM_DRAW);
    }
    if (gsVuIdxCount != 0) {
        glBindVertexArray(sVaoVu);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(gsVuIdxCount * sizeof(uint32_t)), gsVuIdx, GL_STREAM_DRAW);
        glBindVertexArray(0);
    }
#else
    if (gsVertCount != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, sVboGs);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(gsVertCount * sizeof(Vtx)), gsVerts);
    }
    if (gsVuVertCount != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, sVboVu);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(gsVuVertCount * 48), gsVuVerts);
    }
    if (gsVuIdxCount != 0) {
        glBindVertexArray(sVaoVu);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (GLsizeiptr)(gsVuIdxCount * sizeof(uint32_t)), gsVuIdx);
        glBindVertexArray(0);
    }
#endif
    for (i = 0; i < 2; i++) {
        uint32_t *px, x, y, fbp = i ? 0x70 : 0;
        if (!(gGsFbUploads & (1u << i))) { continue; }
        if (sFbTex[i] == 0) {
            glGenTextures(1, &sFbTex[i]);
            glBindTexture(GL_TEXTURE_2D, sFbTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GS_W, GS_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glGenFramebuffers(1, &sFbFbo[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, sFbFbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sFbTex[i], 0);
        }
        px = malloc((size_t)GS_W * GS_H * 4);
        for (y = 0; y < GS_H; y++) {
            for (x = 0; x < GS_W; x++) {
                px[y * GS_W + x] = Gs_VramRead(fbp * 32, 8, 0, x, y) | 0xFF000000u;
            }
        }
        glBindTexture(GL_TEXTURE_2D, sFbTex[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GS_W, GS_H, GL_RGBA, GL_UNSIGNED_BYTE, px);
        free(px);
    }
    gGsFbUploads = 0;

    glBindVertexArray(sVaoGs);
    sCurPipe = -1;
    sCurProgram = 0;
    sCurTex0 = sCurTex1 = sCurTex2 = 0;
    sCurSamp0 = sCurSamp1 = -1;
    haveScissor = 0;
    lastBlendc = -1.0f;
    /* The present draws its full-screen triangle with the scissor test off (see the end of this function); turn it
       back on for the frame's own draws, or every partial scissor (the split screen's halves, the HUD, the
       fighters' shadows) is ignored and the two views overlap across the whole screen. */
    glEnable(GL_SCISSOR_TEST);
    if (depthless < 0) { /* BT3_GPU_DEPTHLESS=0/1, setting "depthless_passes" (0 automatic: on, 1 on, 2 off) */
        const char *e = getenv("BT3_GPU_DEPTHLESS");
        depthless = e != NULL ? atoi(e) != 0 : Port_Setting("depthless_passes", 0) != 2;
        fprintf(stderr, "bt3: gl: framebuffers without the depth buffer where it is not used: %s\n", depthless ? "on" : "off");
    }
    for (n = 0; n < gsDrawCount; n++) {
        const GsDraw *d = &gsDraws[n];
        int shadowStart;
        if (d->native >= 100) {
            GLenum one[1] = {GL_COLOR_ATTACHMENT0};
            if (sFbTex[d->native - 100] == 0) { continue; }
            glDisable(GL_SCISSOR_TEST); /* glBlitFramebuffer honours the scissor test: the copy must be whole */
            glBindFramebuffer(GL_READ_FRAMEBUFFER, sFbFbo[d->native - 100]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sTgFbo[d->target]);
            glDrawBuffers(1, one);
            glBlitFramebuffer(0, 0, GS_W, GS_H, 0, 0, GS_W * SCALE, GS_H * SCALE, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glEnable(GL_SCISSOR_TEST);
            haveScissor = 0;
            gsTargets[d->target].cleared = 1;
            cur = -1;
            continue;
        }
        if (d->native == 5 || d->native == 4) {
            GLuint dst = d->native == 5 ? sDateCopy : sAuxCopy;
            GLenum one[1] = {GL_COLOR_ATTACHMENT0};
            int rx = 0, ry = 0, rw = GS_W * SCALE, rh = GS_H * SCALE;
            if (!gsTargets[d->target].cleared) { continue; }
            /* the DATE copy: only what the draws that follow read (gs_draw.c); rows as in the Vulkan back end */
            if (d->native == 5 && (sHasFetch || !GsDraw_DateRegion(n, &rx, &ry, &rw, &rh))) { continue; } /* (fetch: no copy) */
            /* The copy of the frame's alpha ids (the outline and the DATE passes read it). glBlitFramebuffer is
               clipped by the scissor test, so with a partial scissor in force (the split screen's halves) only
               part of the alpha would be copied and the outline would draw a stale silhouette; turn it off for
               the blit. (Vulkan's copy pass is not affected, which is why only the GL back end showed it.) */
            glDisable(GL_SCISSOR_TEST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, sScratchRead);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sTgAux[d->target], 0);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sScratchDraw);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst, 0);
            glDrawBuffers(1, one);
            glBlitFramebuffer(rx, ry, rx + rw, ry + rh, rx, ry, rx + rw, ry + rh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            sBlitN++;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glEnable(GL_SCISSOR_TEST);
            haveScissor = 0;
            cur = -1;
            continue;
        }
        if (d->native == 3) {
            float params[4] = {(float)d->src, d->misc[0], 0.0f, 0.0f};
            if (!gsTargets[d->target].cleared) { continue; }
            if (cur != d->target) { bind_target(d->target, 0, 0); cur = d->target; }
            sWantFetch = 0;
            set_pipe(d->pipeline);
            set_textures(sTgDep[d->target], 6, sAuxCopy, 6, (GLuint)d->tex, 6);
            set_params(params, 16);
            haveFu = 0; /* the Params block now holds this pass's: the next primitive sends its own again */
            if (d->blendc != lastBlendc) { glBlendColor(d->blendc, d->blendc, d->blendc, d->blendc); lastBlendc = d->blendc; }
            set_scissor(d->scissor);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            continue;
        }
        if (d->native) {
            float params[4];
            if (!gsTargets[d->target].cleared) { continue; }
            if (cur != d->target) { bind_target(d->target, 0, 0); cur = d->target; }
            set_native(d->native);
            set_textures(sAuxCopy, 6, 0, 6, 0, 6);
            extern int Port_PageAspectMilli(void); /* plat_stub.c: the shape of the page shown now */
            params[0] = 1333.333f / (float)Port_PageAspectMilli() / (float)GS_W; /* one PS2 pixel across, as wide as at 4:3 (outline.frag) */
            params[1] = 1.0f / (float)GS_H;
            params[2] = 100.0f / 255.0f;
            params[3] = getenv("BT3_FX_DEBUG") != NULL ? 1.0f : 0.0f;
            set_params(params, 16);
            haveFu = 0;
            set_scissor(d->scissor);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            continue;
        }
        /* an ordinary primitive */
        shadowStart = sPipes[d->pipeline].shadow &&
            (n == 0 || gsDraws[n - 1].native || gsDraws[n - 1].target != d->target ||
             !sPipes[gsDraws[n - 1].pipeline].shadow);
        if (cur != d->target) {
            int nodepth = 0;
            if (depthless && gsTargets[d->target].cleared) {
                /* the draws on this target until the next copy or change of target (the table passes and native
                   effects in between stay in the same framebuffer and use no depth buffer) */
                uint32_t j;
                nodepth = 1;
                for (j = n; nodepth && j < gsDrawCount && gsDraws[j].target == d->target &&
                            !(gsDraws[j].native >= 4); j++) {
                    const Pipe *q = &sPipes[gsDraws[j].pipeline];
                    if (!gsDraws[j].native && !q->fx) {
                        nodepth = q->ztst == 1 && !q->zwrite && !q->shadow;
                    }
                }
            }
            sNoDepthN += nodepth;
            bind_target(d->target, !gsTargets[d->target].cleared, nodepth);
            gsTargets[d->target].cleared = 1;
            cur = d->target;
            haveFu = 0;
            sCurPipe = -1;
            haveScissor = 0;
        } else if (!gsTargets[d->target].cleared) {
            gsTargets[d->target].cleared = 1;
        }
        if (shadowStart) {
            /* One bit per pixel blocks every later triangle of this flat-shadow run. */
            glDisable(GL_SCISSOR_TEST);
            glStencilMask(0xff);
            glClearStencil(0);
            glClear(GL_STENCIL_BUFFER_BIT);
            glEnable(GL_SCISSOR_TEST);
            haveScissor = 0;
        }
        sWantFetch = sHasFetch && d->misc[1] != 0.0f; /* (a draw that tests the destination alpha) */
        set_pipe(d->pipeline);
        glBindVertexArray(d->vu ? sVaoVu : sVaoGs);
        if (d->vu) { set_uniform_u(&gsVuUni[d->uniform]); }
        memset(&fu, 0, sizeof(fu));
        memcpy(fu.mode, d->mode, sizeof(fu.mode));
        memcpy(fu.misc, d->misc, sizeof(fu.misc));
        memcpy(fu.rect, d->rect, sizeof(fu.rect));
        memcpy(fu.orig, d->orig, sizeof(fu.orig));
        fu.extra[0] = d->vu ? (gsVuUni[d->uniform].misc[2] > 0.0f ? gsVuUni[d->uniform].misc[2] : 0.0f) : gsZMax;
        fu.extra[1] = (sPipes[d->pipeline].key & (1u << 9)) ? 1.0f : 0.0f; /* the source scaled by its alpha */
        if (!haveFu || memcmp(&fu, &lastFu, sizeof(fu)) != 0) {
            set_params(&fu, sizeof(fu));
            memcpy(&lastFu, &fu, sizeof(fu));
            haveFu = 1;
        }
        set_textures((GLuint)d->tex, d->sampler, sDateCopy, 6, 0, 6);
        if (d->blendc != lastBlendc) { glBlendColor(d->blendc, d->blendc, d->blendc, d->blendc); lastBlendc = d->blendc; }
        set_scissor(d->scissor);
        if (d->vu) {
            glDrawElements(GL_TRIANGLES, (GLsizei)d->count, GL_UNSIGNED_INT, (const void *)(uintptr_t)(d->first * sizeof(uint32_t)));
        } else {
            glDrawArrays(sPipes[d->pipeline].topo == 0 ? GL_TRIANGLES : sPipes[d->pipeline].topo == 1 ? GL_LINES : GL_POINTS, (GLint)d->first, (GLsizei)d->count);
        }
    }

    /* the shown buffer: the one sceGsSwapDBuff set up for this frame, else the first target */
    best = gGsMainFbp >= 0 ? GsDraw_TargetGet((uint32_t)gGsMainFbp, 0) : -1;
    for (i = 0; best < 0 && i < gsTargetCount; i++) { best = i; }
    glDisable(GL_STENCIL_TEST);
    if (best >= 0 && gsTargets[best].cleared) {
        sLastBest = best; /* (shown again while the game is paused: GsGl_PauseFrame) */
    }
    present_picture(best);
    Ui_DrawGL(); /* the overlay, over the presented picture */
    SDL_GL_SwapWindow(sWindow);
    /* BT3_GL_WINSHOT=<n>: dump the window's own framebuffer every n frames (what the user sees) */
    if (getenv("BT3_GL_WINSHOT") != NULL && atoi(getenv("BT3_GL_WINSHOT")) > 0 && (int)gGsFrame % atoi(getenv("BT3_GL_WINSHOT")) == 0) {
        int sw, sh, x, y;
        uint8_t *wp;
        char nm[64];
        FILE *wf;
        SDL_GetWindowSize(sWindow, &sw, &sh);
        wp = malloc((size_t)sw * sh * 4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, wp);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        snprintf(nm, sizeof(nm), "port/build/shots/window_%05u.ppm", gGsFrame);
        wf = fopen(nm, "wb");
        if (wf != NULL) {
            fprintf(wf, "P6\n%d %d\n255\n", sw, sh);
            for (y = sh - 1; y >= 0; y--) {
                for (x = 0; x < sw; x++) { fwrite(&wp[(y * sw + x) * 4], 1, 3, wf); }
            }
            fclose(wf);
        }
        free(wp);
    }
    if (getenv("BT3_GS_VERBOSE") != NULL) {
        GLenum e = glGetError();
        if (e != GL_NO_ERROR) { fprintf(stderr, "bt3: gl: frame error 0x%x\n", e); }
    }

    /* BT3_SHOT=<n>: read the shown buffer back to port/build/shots/gpu_NNNNN.ppm (rows come back in texture
       order: the first row is the scene's top, as in the Vulkan back end's download) */
    {
        static int every = -1;
        if (every < 0) { every = getenv("BT3_SHOT") != NULL ? atoi(getenv("BT3_SHOT")) : 0; }
        if (every > 0 && gGsFrame % (unsigned)every == 0 && best >= 0 && gsTargets[best].cleared &&
            (getenv("BT3_SHOT_FROM") == NULL || (int)gGsFrame >= atoi(getenv("BT3_SHOT_FROM"))) &&
            (getenv("BT3_SHOT_TO") == NULL || (int)gGsFrame <= atoi(getenv("BT3_SHOT_TO")))) {
            GLsizei w = 512 * SCALE, h = 448 * SCALE;
            int aux = getenv("BT3_SHOT_AUX") != NULL;
            uint8_t *px = malloc((size_t)w * h * 4);
            char name[64];
            FILE *fp;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, sTgFbo[best]);
            glReadBuffer(aux ? GL_COLOR_ATTACHMENT1 : GL_COLOR_ATTACHMENT0);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, w, h, aux ? GL_RED : GL_RGBA, GL_UNSIGNED_BYTE, px);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            snprintf(name, sizeof(name), "port/build/shots/gpu_%05u.ppm", gGsFrame);
            fp = fopen(name, "wb");
            if (fp != NULL) {
                fprintf(fp, "P6\n%d %d\n255\n", w, h);
                for (i = 0; i < h; i++) {
                    uint32_t x;
                    for (x = 0; x < (uint32_t)w; x++) {
                        if (aux) {
                            uint8_t v = px[i * w + x], t[3] = {(uint8_t)(v * 16), (uint8_t)(v * 16), v};
                            fwrite(t, 1, 3, fp);
                        } else {
                            fwrite(&px[(i * w + x) * 4], 1, 3, fp);
                        }
                    }
                }
                fclose(fp);
            }
            free(px);
        }
    }

    sFrameN++;
    for (i = 0; i < gsTargetCount; i++) { gsTargets[i].draws = 0; }
    gsVertCount = 0;
    gsVuVertCount = 0;
    gsVuIdxCount = 0;
    gsVuUniCount = 0;
    gsDrawCount = 0;
    gsAnchor = 0;
    gsSkipped = 0;
    gsNative = 0;
}

/* Target i's picture, reduced to the GS's 512 x 448 (as the Vulkan back end's). */
static int gl_target_read(int i, uint8_t *rgba) {
    GLsizei w = 512 * SCALE, h = 448 * SCALE; /* (the picture's part of the target, as the screenshot's) */
    uint8_t *px;
    int x, y;

    if (sTgFbo[i] == 0 || (px = malloc((size_t)w * h * 4)) == NULL) {
        return 0;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sTgFbo[i]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    for (y = 0; y < 448; y++) { /* (rows come back in texture order: the first is the scene's top) */
        for (x = 0; x < 512; x++) {
            memcpy(rgba + (y * 512 + x) * 4, px + ((size_t)(y * SCALE + SCALE / 2) * (size_t)w + (size_t)(x * SCALE + SCALE / 2)) * 4, 4);
        }
    }
    free(px);
    return 1;
}

static void gl_target_copy(int src, int dst) {
    GLenum one[1] = {GL_COLOR_ATTACHMENT0};
    if (sTgCol[src] == 0 || sTgCol[dst] == 0) {
        return;
    }
    glDisable(GL_SCISSOR_TEST); /* (the copy must be whole; frame_end sets its own state when it runs) */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sScratchRead);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sTgCol[src], 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sScratchDraw);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sTgCol[dst], 0);
    glDrawBuffers(1, one);
    glBlitFramebuffer(0, 0, GS_W * SCALE, GS_H * SCALE, 0, 0, GS_W * SCALE, GS_H * SCALE, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

GsBackend sGlBackend = {
    "opengl",
    gl_init,
    frame_end,
    scale_changed,
    white_tex,
    tex_create,
    tex_upload,
    tex_destroy,
    tex_slots_left,
    target_ensure,
    target_drop,
    target_color,
    target_aux,
    target_depth,
    pipe_get,
    gl_target_read,
    gl_target_copy,
};
