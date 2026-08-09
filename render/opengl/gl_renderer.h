#pragma once

#include <memory>

#include "render/renderer.h"

namespace voxel {

// OpenGL renderer: OpenGL 3.3 core on desktop, OpenGL ES 3.0 on Android.
// Owns the GL context, the mesh shader program, and the demo-scene geometry.
// All GL entry points are resolved at runtime via SDL_GL_GetProcAddress.
class GL_Renderer final : public Renderer {
public:
    explicit GL_Renderer(const RendererCreateInfo& info);
    ~GL_Renderer() override;

    bool initialize() override;
    void shutdown() override;

    bool beginFrame() override;
    void endFrame() override;
    void onResize(int width, int height) override;
    void onResume() override;

    void renderFrame(const RenderCamera& camera, double deltaSeconds) override;
    bool captureScreenshot(const std::string& path) override;
    void uploadChunkMesh(voxel::world::ChunkCoord coord,
                         const ChunkMeshData& mesh) override;
    void clearChunkMesh(voxel::world::ChunkCoord coord) override;

private:
    bool createResources();
    void destroyResources();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace voxel
