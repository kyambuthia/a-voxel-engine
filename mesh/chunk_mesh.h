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

// Output of the exposed-face mesher for one chunk. Vertices are packed 4 per
// visible face (one quad per face, mirroring the reference architecture's
// quad strategy); the renderer triangulates each quad into two triangles.
struct ChunkMesh {
    std::vector<Vertex> vertices;

    bool empty() const { return vertices.empty(); }
    std::uint32_t quadCount() const {
        return static_cast<std::uint32_t>(vertices.size() / 4);
    }
};

}  // namespace voxel::mesh
