#include "mesh/builder.h"

using namespace voxel::world;

namespace voxel::mesh {
namespace {

// Simple asset-free palette. Grass gets a green top and earthy sides;
// everything else is a single flat color.
constexpr glm::vec3 kStone{0.55f, 0.55f, 0.55f};
constexpr glm::vec3 kGrassTop{0.30f, 0.60f, 0.24f};
constexpr glm::vec3 kGrassSide{0.48f, 0.38f, 0.24f};
constexpr glm::vec3 kSand{0.80f, 0.72f, 0.50f};
constexpr glm::vec3 kWater{0.22f, 0.48f, 0.88f};
constexpr glm::vec3 kGravel{0.46f, 0.44f, 0.42f};
constexpr glm::vec3 kCoalOre{0.28f, 0.28f, 0.31f};
constexpr glm::vec3 kIronOre{0.58f, 0.48f, 0.36f};
constexpr glm::vec3 kWood{0.46f, 0.32f, 0.16f};
constexpr glm::vec3 kLeaves{0.16f, 0.46f, 0.16f};

// Per-face color. Face index matches kNormal / kDir order (+Y, -Y, -X, +X,
// -Z, +Z); grass only tints its top (+Y) face.
glm::vec3 faceColor(BlockId b, int face) {
    switch (b) {
        case BlockId::Grass:
            return face == 0 ? kGrassTop : kGrassSide;
        case BlockId::Sand:
            return kSand;
        case BlockId::Water:
            return kWater;
        case BlockId::Gravel:
            return kGravel;
        case BlockId::CoalOre:
            return kCoalOre;
        case BlockId::IronOre:
            return kIronOre;
        case BlockId::Wood:
            return kWood;
        case BlockId::Leaves:
            return kLeaves;
        default:
            return kStone;
    }
}

constexpr glm::vec3 kNormal[6] = {
    {0.0f, 1.0f, 0.0f},   // +Y
    {0.0f, -1.0f, 0.0f},  // -Y
    {-1.0f, 0.0f, 0.0f},  // -X
    {1.0f, 0.0f, 0.0f},   // +X
    {0.0f, 0.0f, -1.0f},  // -Z
    {0.0f, 0.0f, 1.0f},   // +Z
};

constexpr int kDir[6][3] = {
    {0, 1, 0}, {0, -1, 0}, {-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1},
};

// Corner bitmasks per face, in the winding proven by the smoke-test cube (the
// renderer's front face is GL_CCW; these orders produce cross(P1-P0, P2-P0) =
// -faceNormal, matching that convention). Bit 0 = +x, bit 1 = +y, bit 2 = +z.
constexpr int kFaceCorners[6][4] = {
    {2, 3, 7, 6},  // +Y
    {0, 4, 5, 1},  // -Y
    {0, 2, 6, 4},  // -X
    {1, 5, 7, 3},  // +X
    {0, 1, 3, 2},  // -Z
    {4, 6, 7, 5},  // +Z
};

}  // namespace

void ChunkMesher::build(const Chunk& chunk, ChunkCoord coord, const World& world,
                        ChunkMesh& out) const {
    out.vertices.clear();
    const WorldPosition origin = chunkOrigin(coord);
    for (int lx = 0; lx < kChunkSizeX; ++lx) {
        for (int lz = 0; lz < kChunkSizeZ; ++lz) {
            for (int ly = 0; ly < kChunkHeight; ++ly) {
                const LocalCoord lc{lx, ly, lz};
                const BlockId b = chunk.blockAt(lc);
                if (b == BlockId::Air) {
                    continue;
                }
                const int wx = origin.x + lx;
                const int wz = origin.z + lz;
                for (int f = 0; f < 6; ++f) {
                    const BlockId neighbor = world.blockAt(
                        {wx + kDir[f][0], ly + kDir[f][1], wz + kDir[f][2]});
                    if (neighbor != BlockId::Air) {
                        continue;  // face buried by a neighbor
                    }
                    const glm::vec3 color = faceColor(b, f);
                    for (int c = 0; c < 4; ++c) {
                        const int mask = kFaceCorners[f][c];
                        const float x = static_cast<float>(wx + (mask & 1));
                        const float y =
                            static_cast<float>(ly + ((mask >> 1) & 1));
                        const float z =
                            static_cast<float>(wz + ((mask >> 2) & 1));
                        out.vertices.push_back(
                            Vertex{glm::vec3{x, y, z}, kNormal[f], color});
                    }
                }
            }
        }
    }
}

}  // namespace voxel::mesh
