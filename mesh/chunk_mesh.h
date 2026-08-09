#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace voxel::mesh {

// One meshed vertex: world position, face normal, per-face color (RGB, 0..1).
// This is the renderer-agnostic vertex format; the GL backend expands it to
// its interleaved buffer (alpha added) at upload time.
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

// Output of the exposed-face mesher for one chunk. Vertices are packed as two
// independent, outward-facing CCW triangles (6 vertices) per visible block
// face, ready for a GL_TRIANGLES draw call.
struct ChunkMesh {
    std::vector<Vertex> vertices;

    bool empty() const { return vertices.empty(); }
    std::uint32_t quadCount() const {
        return static_cast<std::uint32_t>(vertices.size() / 6);
    }
    std::uint32_t triangleCount() const {
        return static_cast<std::uint32_t>(vertices.size() / 3);
    }
};

}  // namespace voxel::mesh
