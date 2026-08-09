#include "render/opengl/gl_renderer.h"

#include <SDL3/SDL.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <string>
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

// OpenGL's clip volume is -w..+w on all three axes. Extracting the six
// homogeneous clip planes from the combined view/projection matrix lets us
// reject whole chunk columns before issuing a draw call. Planes do not need
// normalization for this AABB test.
struct Frustum {
    std::array<glm::vec4, 6> planes;

    bool intersects(const glm::vec3& boundsMin,
                    const glm::vec3& boundsMax) const {
        for (const glm::vec4& plane : planes) {
            const glm::vec3 positive{
                plane.x >= 0.0f ? boundsMax.x : boundsMin.x,
                plane.y >= 0.0f ? boundsMax.y : boundsMin.y,
                plane.z >= 0.0f ? boundsMax.z : boundsMin.z,
            };
            if (glm::dot(glm::vec3(plane), positive) + plane.w < 0.0f) {
                return false;
            }
        }
        return true;
    }
};

Frustum extractFrustum(const glm::mat4& viewProj) {
    // GLM matrices are column-major; construct the matrix rows explicitly.
    const glm::vec4 row0{viewProj[0][0], viewProj[1][0], viewProj[2][0],
                         viewProj[3][0]};
    const glm::vec4 row1{viewProj[0][1], viewProj[1][1], viewProj[2][1],
                         viewProj[3][1]};
    const glm::vec4 row2{viewProj[0][2], viewProj[1][2], viewProj[2][2],
                         viewProj[3][2]};
    const glm::vec4 row3{viewProj[0][3], viewProj[1][3], viewProj[2][3],
                         viewProj[3][3]};

    return Frustum{{row3 + row0, row3 - row0, row3 + row1, row3 - row1,
                    row3 + row2, row3 - row2}};
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
    GLint uModel = -1;
    GLint uViewProj = -1;
    GLint uLightDir = -1;

    // Per-chunk geometry keyed by the world's chunk key.
    struct GpuMesh {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLsizei vertexCount = 0;
        glm::vec3 boundsMin{0.0f};
        glm::vec3 boundsMax{0.0f};
    };
    std::unordered_map<std::uint64_t, GpuMesh> chunkMeshes;

    // Reused between captures; resize only reallocates after a framebuffer
    // size increase instead of allocating two full-frame buffers per image.
    std::vector<std::uint8_t> screenshotPixels;
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
    if (!m.ready) {
        return;
    }
    if (data.vertexCount == 0) {
        clearChunkMesh(coord);
        return;
    }
    if (data.vertices == nullptr) {
        std::fprintf(stderr,
                     "[GL] rejected chunk mesh with a null vertex buffer\n");
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
    const float minX = static_cast<float>(coord.cx) *
                       static_cast<float>(voxel::world::kChunkSizeX);
    const float minZ = static_cast<float>(coord.cz) *
                       static_cast<float>(voxel::world::kChunkSizeZ);
    gpu.boundsMin = glm::vec3(minX, 0.0f, minZ);
    gpu.boundsMax =
        glm::vec3(minX + static_cast<float>(voxel::world::kChunkSizeX),
                  static_cast<float>(voxel::world::kChunkHeight),
                  minZ + static_cast<float>(voxel::world::kChunkSizeZ));
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

    const Frustum frustum = extractFrustum(camera.viewProj);
    for (const auto& kv : m.chunkMeshes) {
        const Impl::GpuMesh& gpu = kv.second;
        if (!frustum.intersects(gpu.boundsMin, gpu.boundsMax)) {
            continue;
        }
        gl::glBindVertexArray(gpu.vao);
        gl::glDrawArrays(GL_TRIANGLES, 0, gpu.vertexCount);
    }

    gl::glBindVertexArray(0);
    gl::glUseProgram(0);
}

bool GL_Renderer::captureScreenshot(const std::string& path) {
    Impl& m = *m_impl;
    if (!m.ready || m.width <= 0 || m.height <= 0) {
        std::fprintf(stderr, "[GL] screenshot unavailable: renderer is not ready\n");
        return false;
    }
    if (path.empty()) {
        std::fprintf(stderr, "[GL] screenshot path is empty\n");
        return false;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(m.width) * 4u;
    const std::size_t byteCount = rowBytes * static_cast<std::size_t>(m.height);
    m.screenshotPixels.resize(byteCount);

    // GL's first returned row is the bottom of the framebuffer, whereas PNG
    // conventionally stores the top row first. RGBA/UNSIGNED_BYTE is present
    // in both OpenGL 3.3 and OpenGL ES 3.0. Preserve the caller's pack state.
    GLint previousPackAlignment = 4;
    gl::glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    gl::glPixelStorei(GL_PACK_ALIGNMENT, 1);
    gl::glReadPixels(0, 0, m.width, m.height, GL_RGBA, GL_UNSIGNED_BYTE,
                     m.screenshotPixels.data());
    gl::glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    if (!gl::checkError("captureScreenshot glReadPixels")) {
        return false;
    }
    for (int y = 0; y < m.height / 2; ++y) {
        auto top = m.screenshotPixels.begin() +
                   static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) *
                                               rowBytes);
        auto bottom = m.screenshotPixels.begin() + static_cast<std::ptrdiff_t>(
            static_cast<std::size_t>(m.height - 1 - y) * rowBytes);
        std::swap_ranges(top,
                         top + static_cast<std::ptrdiff_t>(rowBytes), bottom);
    }

    // SDL_PIXELFORMAT_RGBA32 denotes RGBA byte order in memory on both
    // little- and big-endian targets, matching GL_RGBA readback.
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        m.width, m.height, SDL_PIXELFORMAT_RGBA32, m.screenshotPixels.data(),
        static_cast<int>(rowBytes));
    if (surface == nullptr) {
        std::fprintf(stderr, "[GL] screenshot surface creation failed: %s\n",
                     SDL_GetError());
        return false;
    }
    const bool saved = SDL_SavePNG(surface, path.c_str());
    SDL_DestroySurface(surface);
    if (!saved) {
        std::fprintf(stderr, "[GL] screenshot save failed for '%s': %s\n",
                     path.c_str(), SDL_GetError());
        return false;
    }
    std::printf("[GL] screenshot saved: %s (%dx%d)\n", path.c_str(), m.width,
                m.height);
    return true;
}

}  // namespace voxel
