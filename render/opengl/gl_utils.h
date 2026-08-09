#pragma once

// Platform-appropriate GL headers. All entry points are resolved at runtime
// through SDL_GL_GetProcAddress (see gl_utils.cpp), so neither linking against
// the GL driver nor GL_GLEXT_PROTOTYPES is required. That keeps the exact same
// source compiling for desktop OpenGL core and Android OpenGL ES 3.
#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <cstddef>
#include <string>

// Desktop gl.h defines APIENTRY; GLES3/gl3.h defines only GL_APIENTRY (which is
// empty on the platforms we target). Normalize it so the function-pointer types
// below compile identically on both.
#ifndef APIENTRY
#ifdef GL_APIENTRY
#define APIENTRY GL_APIENTRY
#else
#define APIENTRY
#endif
#endif

namespace voxel::gl {

// ---------------------------------------------------------------------------
// Runtime-resolved GL entry points.
//
// Each entry is a function pointer with the exact GL signature. APIENTRY gives
// the correct calling convention on every platform (stdcall on Win32).
// ---------------------------------------------------------------------------
#define VOXEL_GL_FUNCS(X)                                                        \
    X(glClear, void, (GLbitfield))                                               \
    X(glClearColor, void, (GLfloat, GLfloat, GLfloat, GLfloat))                  \
    X(glEnable, void, (GLenum))                                                  \
    X(glDisable, void, (GLenum))                                                 \
    X(glViewport, void, (GLint, GLint, GLsizei, GLsizei))                        \
    X(glGetError, GLenum, ())                                                    \
    X(glGetString, const GLubyte*, (GLenum))                                     \
    X(glDepthFunc, void, (GLenum))                                               \
    X(glCullFace, void, (GLenum))                                                \
    X(glFrontFace, void, (GLenum))                                               \
    X(glGenVertexArrays, void, (GLsizei, GLuint*))                               \
    X(glBindVertexArray, void, (GLuint))                                         \
    X(glDeleteVertexArrays, void, (GLsizei, const GLuint*))                      \
    X(glGenBuffers, void, (GLsizei, GLuint*))                                    \
    X(glBindBuffer, void, (GLenum, GLuint))                                      \
    X(glBufferData, void, (GLenum, GLsizeiptr, const void*, GLenum))             \
    X(glDeleteBuffers, void, (GLsizei, const GLuint*))                           \
    X(glCreateShader, GLuint, (GLenum))                                          \
    X(glShaderSource, void, (GLuint, GLsizei, const GLchar* const*,              \
                             const GLint*))                                      \
    X(glCompileShader, void, (GLuint))                                           \
    X(glGetShaderiv, void, (GLuint, GLenum, GLint*))                             \
    X(glGetShaderInfoLog, void, (GLuint, GLsizei, GLsizei*, GLchar*))            \
    X(glDeleteShader, void, (GLuint))                                            \
    X(glCreateProgram, GLuint, ())                                               \
    X(glAttachShader, void, (GLuint, GLuint))                                    \
    X(glLinkProgram, void, (GLuint))                                             \
    X(glGetProgramiv, void, (GLuint, GLenum, GLint*))                            \
    X(glGetProgramInfoLog, void, (GLuint, GLsizei, GLsizei*, GLchar*))           \
    X(glDeleteProgram, void, (GLuint))                                           \
    X(glUseProgram, void, (GLuint))                                              \
    X(glGetUniformLocation, GLint, (GLuint, const GLchar*))                      \
    X(glUniformMatrix4fv, void, (GLint, GLsizei, GLboolean, const GLfloat*))     \
    X(glUniform3f, void, (GLint, GLfloat, GLfloat, GLfloat))                     \
    X(glVertexAttribPointer, void, (GLuint, GLint, GLenum, GLboolean, GLsizei,   \
                                    const void*))                                \
    X(glEnableVertexAttribArray, void, (GLuint))                                 \
    X(glDisableVertexAttribArray, void, (GLuint))                                \
    X(glDrawElements, void, (GLenum, GLsizei, GLenum, const void*))

#define VOXEL_GL_DECL(name, ret, params) extern ret(APIENTRY * name) params;
VOXEL_GL_FUNCS(VOXEL_GL_DECL)
#undef VOXEL_GL_DECL

// Resolve every entry point above through SDL_GL_GetProcAddress. Must be
// called after a GL context exists and is current. Returns false (and logs the
// missing names) if any required entry point is unavailable.
bool loadFunctions();

// ---------------------------------------------------------------------------
// Compile / link helpers
// ---------------------------------------------------------------------------

const char* errorString(unsigned int err);

// Compiles a single shader stage. `body` is version-neutral GLSL; the correct
// #version (and, on GLES, a default float precision) preamble is prepended for
// the active context flavor. Returns 0 on failure and fills *outError.
unsigned int compileShader(unsigned int type, const std::string& body,
                           std::string* outError);

// Links the given compiled shader objects into one program. Returns 0 on
// failure and fills *outError. The caller owns and must delete the shaders.
unsigned int linkProgram(const unsigned int* shaders, std::size_t count,
                         std::string* outError);

// Drains the error queue; logs the first error to stderr. Returns true if the
// queue was clean. Call after non-render-path setup passes.
bool checkError(const char* where);

}  // namespace voxel::gl
