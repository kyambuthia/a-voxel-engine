#include "render/opengl/gl_renderer.h"

#include <SDL3/SDL.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <cstdio>
#include <vector>

#include "render/opengl/gl_shaders.h"
#include "render/opengl/gl_utils.h"

namespace voxel {

namespace {

// One quad per voxel face, expanded to two triangles for GL. This mirrors the
// GX_QUADS face strategy from the engine spec: every visible face is a quad
// with a shared normal; GL core has no quads, so we triangulate at build time.
struct CubeVertex {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b, a;
};

struct CubeFace {
    glm::vec3 normal;
    glm::vec3 color;  // RGB, 0..1
};

constexpr CubeFace kCubeFaces[6] = {
    {{0.0f, 1.0f, 0.0f}, {0.36f, 0.66f, 0.28f}},  // +Y grass
    {{0.0f, -1.0f, 0.0f}, {0.45f, 0.30f, 0.18f}},  // -Y dirt
    {{-1.0f, 0.0f, 0.0f}, {0.58f, 0.58f, 0.58f}},  // -X stone
    {{1.0f, 0.0f, 0.0f}, {0.58f, 0.58f, 0.58f}},   // +X stone
    {{0.0f, 0.0f, -1.0f}, {0.58f, 0.58f, 0.58f}},  // -Z stone
    {{0.0f, 0.0f, 1.0f}, {0.58f, 0.58f, 0.58f}},   // +Z stone
};

constexpr glm::vec3 kCorners[8] = {
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f},   {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f},    {-0.5f, 0.5f, 0.5f},
};

// Face -> corner indices, CCW when viewed from the outward side.
constexpr int kFaceCorners[6][4] = {
    {3, 2, 6, 7},  // +Y
    {0, 4, 5, 1},  // -Y
    {0, 3, 7, 4},  // -X
    {1, 5, 6, 2},  // +X
    {0, 1, 2, 3},  // -Z
    {4, 7, 6, 5},  // +Z
};

void buildCube(std::vector<CubeVertex>& verts,
               std::vector<unsigned int>& indices) {
    verts.clear();
    indices.clear();
    for (int f = 0; f < 6; ++f) {
        const CubeFace& face = kCubeFaces[f];
        const int base = static_cast<int>(verts.size());
        for (int c = 0; c < 4; ++c) {
            const glm::vec3& p = kCorners[kFaceCorners[f][c]];
            verts.push_back(CubeVertex{p.x, p.y, p.z,
                                       face.normal.x, face.normal.y,
                                       face.normal.z,
                                       face.color.r, face.color.g, face.color.b,
                                       1.0f});
        }
        indices.push_back(static_cast<unsigned int>(base + 0));
        indices.push_back(static_cast<unsigned int>(base + 1));
        indices.push_back(static_cast<unsigned int>(base + 2));
        indices.push_back(static_cast<unsigned int>(base + 0));
        indices.push_back(static_cast<unsigned int>(base + 2));
        indices.push_back(static_cast<unsigned int>(base + 3));
    }
}

}  // namespace

struct GL_Renderer::Impl {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;

    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool ready = false;

    // GPU resources (rebuilt from scratch in createResources()).
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLint uModel = -1;
    GLint uViewProj = -1;
    GLint uLightDir = -1;
    std::size_t indexCount = 0;

    float spin = 0.0f;
};

GL_Renderer::GL_Renderer(const RendererCreateInfo& info)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->window = static_cast<SDL_Window*>(info.window);
    m_impl->width = info.width;
    m_impl->height = info.height;
    m_impl->vsync = info.vsync;
}

GL_Renderer::~GL_Renderer() {
    shutdown();
}

bool GL_Renderer::initialize() {
    Impl& m = *m_impl;
    if (m.window == nullptr) {
        std::fprintf(stderr, "[GL] no window handle provided\n");
        return false;
    }

    // The context-attribute hints (profile, GL/GLES version) are set before
    // SDL_CreateWindow in main.cpp; this renderer only creates the context on
    // the already-configured window.
    m.context = SDL_GL_CreateContext(m.window);
    if (m.context == nullptr) {
        std::fprintf(stderr, "[GL] SDL_GL_CreateContext failed: %s\n",
                     SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(m.window, m.context);
    SDL_GL_SetSwapInterval(m.vsync ? 1 : 0);

    if (!gl::loadFunctions()) {
        std::fprintf(stderr, "[GL] failed to resolve required entry points\n");
        shutdown();
        return false;
    }

    const GLubyte* rendererStr = gl::glGetString(GL_RENDERER);
    const GLubyte* versionStr = gl::glGetString(GL_VERSION);
    std::printf("[GL] renderer: %s\n",
                rendererStr ? reinterpret_cast<const char*>(rendererStr) : "?");
    std::printf("[GL] version:  %s\n",
                versionStr ? reinterpret_cast<const char*>(versionStr) : "?");

    if (!createResources()) {
        shutdown();
        return false;
    }

    gl::glEnable(GL_DEPTH_TEST);
    gl::glDepthFunc(GL_LEQUAL);
    gl::glEnable(GL_CULL_FACE);
    gl::glCullFace(GL_BACK);
    gl::glFrontFace(GL_CCW);

    m.ready = true;
    return true;
}

void GL_Renderer::shutdown() {
    Impl& m = *m_impl;
    if (!m.ready && m.program == 0 && m.context == nullptr) {
        return;
    }
    destroyResources();
    if (m.context != nullptr) {
        SDL_GL_DestroyContext(m.context);
        m.context = nullptr;
    }
    m.ready = false;
}

bool GL_Renderer::createResources() {
    Impl& m = *m_impl;

    std::string err;
    GLuint vs = gl::compileShader(GL_VERTEX_SHADER, gl::kMeshVertexShader, &err);
    if (vs == 0) {
        std::fprintf(stderr, "[GL] %s\n", err.c_str());
        return false;
    }
    GLuint fs =
        gl::compileShader(GL_FRAGMENT_SHADER, gl::kMeshFragmentShader, &err);
    if (fs == 0) {
        std::fprintf(stderr, "[GL] %s\n", err.c_str());
        gl::glDeleteShader(vs);
        return false;
    }
    const GLuint shaders[2] = {vs, fs};
    m.program = gl::linkProgram(shaders, 2, &err);
    gl::glDeleteShader(vs);
    gl::glDeleteShader(fs);
    if (m.program == 0) {
        std::fprintf(stderr, "[GL] %s\n", err.c_str());
        return false;
    }

    m.uModel = gl::glGetUniformLocation(m.program, "uModel");
    m.uViewProj = gl::glGetUniformLocation(m.program, "uViewProj");
    m.uLightDir = gl::glGetUniformLocation(m.program, "uLightDir");

    std::vector<CubeVertex> verts;
    std::vector<unsigned int> indices;
    buildCube(verts, indices);
    m.indexCount = indices.size();

    gl::glGenVertexArrays(1, &m.vao);
    gl::glBindVertexArray(m.vao);

    gl::glGenBuffers(1, &m.vbo);
    gl::glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    gl::glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(verts.size() * sizeof(CubeVertex)),
                     verts.data(), GL_STATIC_DRAW);

    gl::glGenBuffers(1, &m.ebo);
    gl::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    gl::glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
        indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = static_cast<GLsizei>(sizeof(CubeVertex));
    gl::glEnableVertexAttribArray(0);
    gl::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(offsetof(CubeVertex, px)));
    gl::glEnableVertexAttribArray(1);
    gl::glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(offsetof(CubeVertex, nx)));
    gl::glEnableVertexAttribArray(2);
    gl::glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(offsetof(CubeVertex, r)));

    gl::glBindVertexArray(0);
    return gl::checkError("createResources");
}

void GL_Renderer::destroyResources() {
    Impl& m = *m_impl;
    if (m.vao != 0) {
        gl::glDeleteVertexArrays(1, &m.vao);
        m.vao = 0;
    }
    if (m.vbo != 0) {
        gl::glDeleteBuffers(1, &m.vbo);
        m.vbo = 0;
    }
    if (m.ebo != 0) {
        gl::glDeleteBuffers(1, &m.ebo);
        m.ebo = 0;
    }
    if (m.program != 0) {
        gl::glDeleteProgram(m.program);
        m.program = 0;
    }
}

bool GL_Renderer::beginFrame() {
    Impl& m = *m_impl;
    if (!m.ready) {
        return false;
    }
    gl::glViewport(0, 0, m.width, m.height);
    // Pale-blue sky, matching the target's atmosphere.
    gl::glClearColor(0.62f, 0.78f, 0.88f, 1.0f);
    gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return true;
}

void GL_Renderer::endFrame() {
    Impl& m = *m_impl;
    if (!m.ready) {
        return;
    }
    SDL_GL_SwapWindow(m.window);
}

void GL_Renderer::onResize(int width, int height) {
    m_impl->width = width;
    m_impl->height = height;
    // Viewport is (re)applied every frame in beginFrame, so resizing is safe
    // even while a context is being torn down on Android.
}

void GL_Renderer::onResume() {
    Impl& m = *m_impl;
    if (m.context == nullptr) {
        return;
    }
    // Android destroys GL state across surface recreation; rebuild everything.
    // On desktop this path is harmless (delete + re-upload the tiny demo mesh).
    if (!SDL_GL_MakeCurrent(m.window, m.context)) {
        std::fprintf(stderr, "[GL] onResume: SDL_GL_MakeCurrent failed: %s\n",
                     SDL_GetError());
        return;
    }
    destroyResources();
    if (!createResources()) {
        std::fprintf(stderr, "[GL] onResume: resource recreation failed\n");
        m.ready = false;
        return;
    }
    gl::glEnable(GL_DEPTH_TEST);
    gl::glDepthFunc(GL_LEQUAL);
    gl::glEnable(GL_CULL_FACE);
    gl::glCullFace(GL_BACK);
    gl::glFrontFace(GL_CCW);
    m.ready = true;
}

void GL_Renderer::renderFrame(const RenderCamera& camera,
                              double deltaSeconds) {
    Impl& m = *m_impl;
    if (!m.ready || m.program == 0) {
        return;
    }

    m.spin += static_cast<float>(deltaSeconds * 0.6);
    glm::mat4 model =
        glm::rotate(glm::mat4(1.0f), m.spin, glm::vec3(0.0f, 1.0f, 0.0f));

    gl::glUseProgram(m.program);
    gl::glBindVertexArray(m.vao);
    gl::glUniformMatrix4fv(m.uModel, 1, GL_FALSE, glm::value_ptr(model));
    gl::glUniformMatrix4fv(m.uViewProj, 1, GL_FALSE,
                           glm::value_ptr(camera.viewProj));

    const glm::vec3 lightDir = glm::normalize(glm::vec3(-0.6f, 0.8f, 0.35f));
    gl::glUniform3f(m.uLightDir, lightDir.x, lightDir.y, lightDir.z);

    gl::glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m.indexCount),
                       GL_UNSIGNED_INT, nullptr);

    gl::glBindVertexArray(0);
    gl::glUseProgram(0);
}

}  // namespace voxel
