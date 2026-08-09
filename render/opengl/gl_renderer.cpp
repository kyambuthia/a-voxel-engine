#include "render/opengl/gl_renderer.h"

#include <SDL3/SDL.h>

#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <unordered_map>
#include <vector>

#include "render/opengl/gl_shaders.h"
#include "render/opengl/gl_utils.h"

namespace voxel {
namespace {

// Interleaved GPU vertex: position + normal + RGBA color (alpha is 1; the
// mesher supplies RGB per face). Layout matches the shader's
// aPosition(0)/aNormal(1)/aColor(2) attributes.
struct GpuVertex {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b, a;
};

constexpr int kFloatsPerVertex = 10;

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
    GLint uModel = -1;
    GLint uViewProj = -1;
    GLint uLightDir = -1;

    // Per-chunk geometry keyed by the world's chunk key.
    struct GpuMesh {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLsizei vertexCount = 0;
    };
    std::unordered_map<std::uint64_t, GpuMesh> chunkMeshes;
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
    return gl::checkError("createResources");
}

void GL_Renderer::destroyResources() {
    Impl& m = *m_impl;
    for (auto& kv : m.chunkMeshes) {
        if (kv.second.vao != 0) {
            gl::glDeleteVertexArrays(1, &kv.second.vao);
        }
        if (kv.second.vbo != 0) {
            gl::glDeleteBuffers(1, &kv.second.vbo);
        }
    }
    m.chunkMeshes.clear();
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
    // On desktop this path is harmless (delete + re-upload chunk meshes).
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

void GL_Renderer::uploadChunkMesh(voxel::world::ChunkCoord coord,
                                  const ChunkMeshData& data) {
    Impl& m = *m_impl;
    if (!m.ready || data.vertexCount == 0 || data.vertices == nullptr) {
        return;
    }

    std::vector<float> buffer;
    buffer.reserve(static_cast<std::size_t>(data.vertexCount) *
                   kFloatsPerVertex);
    for (std::uint32_t i = 0; i < data.vertexCount; ++i) {
        const voxel::mesh::Vertex& v = data.vertices[i];
        buffer.insert(buffer.end(),
                      {v.position.x, v.position.y, v.position.z,
                       v.normal.x, v.normal.y, v.normal.z,
                       v.color.r, v.color.g, v.color.b, 1.0f});
    }

    const std::uint64_t key = chunkKey(coord);
    auto it = m.chunkMeshes.find(key);
    if (it != m.chunkMeshes.end()) {
        gl::glDeleteVertexArrays(1, &it->second.vao);
        gl::glDeleteBuffers(1, &it->second.vbo);
    }

    Impl::GpuMesh gpu;
    gl::glGenVertexArrays(1, &gpu.vao);
    gl::glBindVertexArray(gpu.vao);
    gl::glGenBuffers(1, &gpu.vbo);
    gl::glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    gl::glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(buffer.size() * sizeof(float)),
                     buffer.data(), GL_STATIC_DRAW);

    const GLsizei stride = kFloatsPerVertex * static_cast<GLsizei>(sizeof(float));
    gl::glEnableVertexAttribArray(0);
    gl::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(0));
    gl::glEnableVertexAttribArray(1);
    gl::glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(3 * sizeof(float)));
    gl::glEnableVertexAttribArray(2);
    gl::glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(6 * sizeof(float)));
    gl::glBindVertexArray(0);

    gpu.vertexCount = static_cast<GLsizei>(data.vertexCount);
    m.chunkMeshes[key] = gpu;
}

void GL_Renderer::clearChunkMesh(voxel::world::ChunkCoord coord) {
    Impl& m = *m_impl;
    const std::uint64_t key = chunkKey(coord);
    auto it = m.chunkMeshes.find(key);
    if (it == m.chunkMeshes.end()) {
        return;
    }
    if (it->second.vao != 0) {
        gl::glDeleteVertexArrays(1, &it->second.vao);
    }
    if (it->second.vbo != 0) {
        gl::glDeleteBuffers(1, &it->second.vbo);
    }
    m.chunkMeshes.erase(it);
}

void GL_Renderer::renderFrame(const RenderCamera& camera, double /*delta*/) {
    Impl& m = *m_impl;
    if (!m.ready || m.program == 0) {
        return;
    }

    gl::glUseProgram(m.program);
    gl::glUniformMatrix4fv(m.uViewProj, 1, GL_FALSE,
                           glm::value_ptr(camera.viewProj));
    // Chunk vertices are already in world space.
    const glm::mat4 identity(1.0f);
    gl::glUniformMatrix4fv(m.uModel, 1, GL_FALSE, glm::value_ptr(identity));

    const glm::vec3 lightDir = glm::normalize(glm::vec3(-0.6f, 0.8f, 0.35f));
    gl::glUniform3f(m.uLightDir, lightDir.x, lightDir.y, lightDir.z);

    for (const auto& kv : m.chunkMeshes) {
        const Impl::GpuMesh& gpu = kv.second;
        gl::glBindVertexArray(gpu.vao);
        gl::glDrawArrays(GL_TRIANGLES, 0, gpu.vertexCount);
    }

    gl::glBindVertexArray(0);
    gl::glUseProgram(0);
}

}  // namespace voxel
