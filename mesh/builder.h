#pragma once

#include "mesh/chunk_mesh.h"
#include "world/chunk.h"
#include "world/coords.h"
#include "world/world.h"

namespace voxel::mesh {

// Exposed-face mesher. Every visible block face is emitted as two independent
// triangles. A face is visible against Air, or when an opaque block borders a
// non-opaque block. Missing chunks and out-of-world positions read as Air, so
// a face at an ungenerated border is emitted and later buried once the
// neighbor arrives. Neighbors are read across chunk borders via the World,
// keeping seams watertight once all neighbors are generated. Pure and
// deterministic.
class ChunkMesher {
public:
    // Builds the mesh for `chunk` (at `coord` within `world`) into `out`.
    // `out` is cleared first.
    void build(const voxel::world::Chunk& chunk, voxel::world::ChunkCoord coord,
               const voxel::world::World& world, ChunkMesh& out) const;
};

}  // namespace voxel::mesh
