#include "render/renderer.h"

#include <cstdio>

#include "render/opengl/gl_renderer.h"

namespace voxel {

std::unique_ptr<Renderer> createRenderer(RendererBackend backend,
                                         const RendererCreateInfo& info) {
    switch (backend) {
        case RendererBackend::OpenGL:
            return std::make_unique<GL_Renderer>(info);
    }
    std::fprintf(stderr, "[renderer] unsupported backend\n");
    return nullptr;
}

}  // namespace voxel
