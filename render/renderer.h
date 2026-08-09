#pragma once

#include <memory>

#include <glm/glm.hpp>

namespace voxel {

// Which graphics API backend the engine should use. OpenGL is the current
// portable backend (OpenGL 3.3 core on desktop, OpenGL ES 3.0 on Android).
// A Vulkan backend can be added behind the same interface later.
enum class RendererBackend {
    OpenGL,
};

struct RendererCreateInfo {
    void* window = nullptr;  // opaque platform window handle (SDL_Window*)
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool debug = false;
};

// Per-frame camera data handed to the renderer. Math stays GLM so the
// interface is free of any SDL / GL / Vulkan types.
struct RenderCamera {
    glm::mat4 viewProj = glm::mat4(1.0f);
    glm::vec3 eye = glm::vec3(0.0f);
};

// Portable renderer contract. The implementation owns the GPU context,
// shaders, buffers, and draw submission for the current scene.
class Renderer {
public:
    virtual ~Renderer() = default;

    // Create the GPU context and upload all resources. Returns false on failure.
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    // Called once per frame. beginFrame clears the target; endFrame presents it.
    virtual bool beginFrame() = 0;
    virtual void endFrame() = 0;

    // Framebuffer (pixel) size changed. Must be safe to call at any time.
    virtual void onResize(int width, int height) = 0;

    // Platform lifecycle hooks (Android surface loss/recreation, desktop
    // minimize). onResume must rebuild any GPU resources that were lost.
    virtual void onPause() {}
    virtual void onResume() {}

    // Draw the scene. deltaSeconds drives any animation.
    virtual void renderFrame(const RenderCamera& camera, double deltaSeconds) = 0;
};

// Factory. Returns nullptr (with a logged error) if the backend cannot be
// created on this platform.
std::unique_ptr<Renderer> createRenderer(RendererBackend backend,
                                         const RendererCreateInfo& info);

}  // namespace voxel
