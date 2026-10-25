// MarniGLFuncs.h - OpenGL entry points for the Linux renderer backend.
//
// Desktop GL only exports 1.2 symbols at link time, so everything above that
// has to be resolved at runtime. This is a hand-rolled loader instead of a
// glad/GLEW dependency: the list below is exactly what MarniDX_GL uses.
//
// The list is written to the GLES-3-compatible subset on purpose (see
// docs/LINUX_PORT.md section 7): no desktop-only entry points, so the same
// backend body can be repointed at GLES for Android/Switch later.
#pragma once

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
inline bool MarniGL_LoadFunctions(void* (*)(const char*)) { return true; }
#else
#include <GL/glcorearb.h>

// X(pointer type, gl name)
#define RE1_GL_FUNCS(X)                                                        \
    X(PFNGLGETSTRINGPROC,              glGetString)                            \
    X(PFNGLGETINTEGERVPROC,            glGetIntegerv)                          \
    X(PFNGLGETERRORPROC,               glGetError)                             \
    X(PFNGLCLEARCOLORPROC,             glClearColor)                           \
    X(PFNGLCLEARPROC,                  glClear)                                \
    X(PFNGLVIEWPORTPROC,               glViewport)                             \
    X(PFNGLSCISSORPROC,                glScissor)                              \
    X(PFNGLENABLEPROC,                 glEnable)                               \
    X(PFNGLDISABLEPROC,                glDisable)                              \
    X(PFNGLDEPTHFUNCPROC,              glDepthFunc)                            \
    X(PFNGLDEPTHMASKPROC,              glDepthMask)                            \
    X(PFNGLBLENDFUNCPROC,              glBlendFunc)                            \
    X(PFNGLBLENDFUNCSEPARATEPROC,      glBlendFuncSeparate)                    \
    X(PFNGLBLENDEQUATIONPROC,          glBlendEquation)                        \
    X(PFNGLPIXELSTOREIPROC,            glPixelStorei)                          \
    X(PFNGLCREATESHADERPROC,           glCreateShader)                         \
    X(PFNGLSHADERSOURCEPROC,           glShaderSource)                         \
    X(PFNGLCOMPILESHADERPROC,          glCompileShader)                        \
    X(PFNGLGETSHADERIVPROC,            glGetShaderiv)                          \
    X(PFNGLGETSHADERINFOLOGPROC,       glGetShaderInfoLog)                     \
    X(PFNGLDELETESHADERPROC,           glDeleteShader)                         \
    X(PFNGLCREATEPROGRAMPROC,          glCreateProgram)                        \
    X(PFNGLATTACHSHADERPROC,           glAttachShader)                         \
    X(PFNGLLINKPROGRAMPROC,            glLinkProgram)                          \
    X(PFNGLGETPROGRAMIVPROC,           glGetProgramiv)                         \
    X(PFNGLGETPROGRAMINFOLOGPROC,      glGetProgramInfoLog)                    \
    X(PFNGLDELETEPROGRAMPROC,          glDeleteProgram)                        \
    X(PFNGLUSEPROGRAMPROC,             glUseProgram)                           \
    X(PFNGLGETATTRIBLOCATIONPROC,      glGetAttribLocation)                    \
    X(PFNGLGETUNIFORMLOCATIONPROC,     glGetUniformLocation)                   \
    X(PFNGLUNIFORMMATRIX4FVPROC,       glUniformMatrix4fv)                     \
    X(PFNGLUNIFORM1IPROC,              glUniform1i)                            \
    X(PFNGLGENVERTEXARRAYSPROC,        glGenVertexArrays)                      \
    X(PFNGLBINDVERTEXARRAYPROC,        glBindVertexArray)                      \
    X(PFNGLDELETEVERTEXARRAYSPROC,     glDeleteVertexArrays)                   \
    X(PFNGLGENBUFFERSPROC,             glGenBuffers)                           \
    X(PFNGLBINDBUFFERPROC,             glBindBuffer)                           \
    X(PFNGLBUFFERDATAPROC,             glBufferData)                           \
    X(PFNGLBUFFERSUBDATAPROC,          glBufferSubData)                        \
    X(PFNGLDELETEBUFFERSPROC,          glDeleteBuffers)                        \
    X(PFNGLVERTEXATTRIBPOINTERPROC,    glVertexAttribPointer)                  \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)             \
    X(PFNGLGENTEXTURESPROC,            glGenTextures)                          \
    X(PFNGLBINDTEXTUREPROC,            glBindTexture)                          \
    X(PFNGLTEXIMAGE2DPROC,             glTexImage2D)                           \
    X(PFNGLTEXSUBIMAGE2DPROC,          glTexSubImage2D)                       \
    X(PFNGLTEXPARAMETERIPROC,          glTexParameteri)                        \
    X(PFNGLDELETETEXTURESPROC,         glDeleteTextures)                       \
    X(PFNGLACTIVETEXTUREPROC,          glActiveTexture)                        \
    X(PFNGLDRAWARRAYSPROC,             glDrawArrays)                           \
    X(PFNGLREADPIXELSPROC,             glReadPixels)

#define RE1_GL_DECL(type, name) extern type name;
RE1_GL_FUNCS(RE1_GL_DECL)
#undef RE1_GL_DECL

// Resolve every entry point. `getProc` is SDL_GL_GetProcAddress. Returns true
// only if all of them resolved; missing names are logged to stderr.
bool MarniGL_LoadFunctions(void* (*getProc)(const char*));

#endif // !__EMSCRIPTEN__
