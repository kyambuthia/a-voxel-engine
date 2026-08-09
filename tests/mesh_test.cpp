// Mesher contract tests: exposed-face culling (intra- and cross-chunk),
// water handling, winding convention, and determinism on generated terrain.

#include "test_harness.h"

#include <glm/glm.hpp>

#include "mesh/builder.h"
#include "world/generator.h"
#include "world/world.h"

using namespace voxel::world;
using namespace voxel::mesh;

namespace {

// Winding normal of a quad (cross product of its first three vertices).
glm::vec3 quadWindingNormal(const ChunkMesh& mesh, std::size_t qi) {
    const Vertex& p0 = mesh.vertices[qi * 4 + 0];
    const Vertex& p1 = mesh.vertices[qi * 4 + 1];
    const Vertex& p2 = mesh.vertices[qi * 4 + 2];
    return glm::cross(p1.position - p0.position, p2.position - p0.position);
}

void meshChunk(const World& world, ChunkCoord c, ChunkMesh& out) {
    const Chunk* chunk = world.chunkAt(c);
    CHECK(chunk != nullptr);
    if (chunk == nullptr) {
        return;
    }
    ChunkMesher mesher;
    mesher.build(*chunk, c, world, out);
}

}  // namespace

TEST(mesh_single_block_six_faces) {
    World w(1);
    w.setBlock({0, 0, 0}, BlockId::Stone);
    ChunkMesh mesh;
    meshChunk(w, {0, 0}, mesh);
    CHECK_EQ(mesh.quadCount(), 6u);
    CHECK_EQ(mesh.vertices.size(), 24u);
}

TEST(mesh_two_adjacent_blocks_share_face) {
    World w(1);
    w.setBlock({0, 0, 0}, BlockId::Stone);
    w.setBlock({1, 0, 0}, BlockId::Stone);
    ChunkMesh mesh;
    meshChunk(w, {0, 0}, mesh);
    CHECK_EQ(mesh.quadCount(), 10u);  // 6 + 6 - 2 shared faces
}

TEST(mesh_cross_chunk_face_culling) {
    World w(1);
    w.setBlock({0, 0, 0}, BlockId::Stone);    // chunk (0, 0)
    w.setBlock({-1, 0, 0}, BlockId::Stone);   // chunk (-1, 0)
    ChunkMesh mesh;
    meshChunk(w, {0, 0}, mesh);
    CHECK_EQ(mesh.quadCount(), 5u);  // -X face of (0,0,0) hidden by neighbor
}

TEST(mesh_block_fully_surrounded_is_hidden) {
    // Center block at (5, 2, 5): all six neighbors are inside chunk (0, 0)
    // and inside the world's vertical span.
    World withCenter(1);
    World withoutCenter(1);
    const int dirs[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                            {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const auto& d : dirs) {
        withCenter.setBlock({5 + d[0], 2 + d[1], 5 + d[2]}, BlockId::Stone);
        withoutCenter.setBlock({5 + d[0], 2 + d[1], 5 + d[2]}, BlockId::Stone);
    }
    withCenter.setBlock({5, 2, 5}, BlockId::Stone);
    ChunkMesh withMesh;
    ChunkMesh withoutMesh;
    meshChunk(withCenter, {0, 0}, withMesh);
    meshChunk(withoutCenter, {0, 0}, withoutMesh);
    CHECK(withMesh.quadCount() > 0u);
    // Adding the center block hides the 6 faces of the surrounding blocks that
    // face it, and the center block itself emits no faces: 6 fewer quads.
    CHECK_EQ(withMesh.quadCount() + 6u, withoutMesh.quadCount());
}

TEST(mesh_water_block_is_meshed) {
    World w(1);
    w.setBlock({0, 0, 0}, BlockId::Water);
    ChunkMesh mesh;
    meshChunk(w, {0, 0}, mesh);
    CHECK_EQ(mesh.quadCount(), 6u);  // water meshes against air neighbors
}

TEST(mesh_winding_matches_renderer_front_face) {
    World w(1);
    w.setBlock({0, 0, 0}, BlockId::Grass);
    ChunkMesh mesh;
    meshChunk(w, {0, 0}, mesh);
    CHECK_EQ(mesh.quadCount(), 6u);
    for (std::size_t q = 0; q < mesh.quadCount(); ++q) {
        const glm::vec3 winding = quadWindingNormal(mesh, q);
        const glm::vec3 faceNormal = mesh.vertices[q * 4].normal;
        CHECK(glm::length(winding) > 1e-6f);
        // The smoke-test cube renders front faces with winding normal
        // antiparallel to the face normal; the mesher must match it so
        // GL_CCW back-face culling keeps every outward face visible.
        const float dot = glm::dot(glm::normalize(winding),
                                   glm::normalize(faceNormal));
        CHECK(dot < -0.999f);
    }
}

TEST(mesh_generated_terrain_deterministic) {
    World wa(2024u);
    World wb(2024u);
    WorldGenerator ga(2024u, wa);
    WorldGenerator gb(2024u, wb);
    for (int cx = -2; cx <= 2; ++cx) {
        for (int cz = -2; cz <= 2; ++cz) {
            ga.request({cx, cz});
            gb.request({cx, cz});
        }
    }
    while (ga.tick(1)) {
    }
    while (gb.tick(1)) {
    }
    ChunkMesher mesher;
    int totalQuads = 0;
    for (int cx = -2; cx <= 2; ++cx) {
        for (int cz = -2; cz <= 2; ++cz) {
            ChunkMesh ma;
            ChunkMesh mb;
            mesher.build(*wa.chunkAt({cx, cz}), {cx, cz}, wa, ma);
            mesher.build(*wb.chunkAt({cx, cz}), {cx, cz}, wb, mb);
            CHECK_EQ(ma.vertices.size(), mb.vertices.size());
            CHECK(ma.vertices.size() % 4 == 0);
            CHECK(ma.vertices.size() > 0);
            totalQuads += static_cast<int>(ma.quadCount());
        }
    }
    CHECK(totalQuads > 0);
}
