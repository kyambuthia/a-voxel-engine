#include "render/opengl/gl_utils.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <vector>

namespace voxel::gl {

namespace {

const char* versionPreamble(unsigned int type) {
#if defined(__ANDROID__)
    // OpenGL ES 3.0 fragment shaders must declare a default float precision.
    if (type == GL_FRAGMENT_SHADER) {
        return "#version 300 es\nprecision highp float;\n";
    }
    return "#version 300 es\n";
#else
    (void)type;
    return "#version 330 core\n";
#endif
}

}  // namespace

#define VOXEL_GL_DEF(name, ret, params) ret(APIENTRY * name) params = nullptr;
VOXEL_GL_FUNCS(VOXEL_GL_DEF)
#undef VOXEL_GL_DEF

bool loadFunctions() {
    bool ok = true;
#define VOXEL_GL_LOAD(name, ret, params)                                 \
    do {                                                                 \
        name = reinterpret_cast<ret(APIENTRY *) params>(                  \
            SDL_GL_GetProcAddress(#name));                               \
        if (!name) {                                                     \
            std::fprintf(stderr, "[GL] missing entry point: %s\n",       \
                         #name);                                         \
            ok = false;                                                  \
        }                                                                \
    } while (0);
    VOXEL_GL_FUNCS(VOXEL_GL_LOAD)
#undef VOXEL_GL_LOAD
    return ok;
}

const char* errorString(unsigned int err) {
    switch (err) {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        default:
            return "unknown GL error";
    }
}

unsigned int compileShader(unsigned int type, const std::string& body,
                           std::string* outError) {
    const std::string source = versionPreamble(type) + body;
    const char* sourcePtr = source.c_str();

    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        if (outError) *outError = "glCreateShader returned 0";
        return 0;
    }
    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        std::string log;
        if (infoLen > 1) {
            log.resize(static_cast<std::size_t>(infoLen));
            GLsizei written = 0;
            glGetShaderInfoLog(shader, infoLen, &written, log.data());
            log.resize(static_cast<std::size_t>(written > 0 ? written : 0));
        }
        if (outError) *outError = "shader compile failed: " + log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

unsigned int linkProgram(const unsigned int* shaders, std::size_t count,
                         std::string* outError) {
    GLuint program = glCreateProgram();
    if (program == 0) {
        if (outError) *outError = "glCreateProgram returned 0";
        return 0;
    }
    for (std::size_t i = 0; i < count; ++i) {
        glAttachShader(program, shaders[i]);
    }
    glLinkProgram(program);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint infoLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
        std::string log;
        if (infoLen > 1) {
            log.resize(static_cast<std::size_t>(infoLen));
            GLsizei written = 0;
            glGetProgramInfoLog(program, infoLen, &written, log.data());
            log.resize(static_cast<std::size_t>(written > 0 ? written : 0));
        }
        if (outError) *outError = "program link failed: " + log;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

bool checkError(const char* where) {
    GLenum err = glGetError();
    if (err == GL_NO_ERROR) {
        return true;
    }
    std::fprintf(stderr, "[GL] error 0x%04x (%s) at %s\n",
                 static_cast<unsigned>(err), errorString(err),
                 where ? where : "?");
    return false;
}

}  // namespace voxel::gl
