#include "world/generator.h"

#include <algorithm>

namespace voxel::world {
namespace {

// Terrain tuning constants. These are our own values (observed-parameter
// style choices from the reference spec's recipe, re-tuned for our world
// height) — change them freely, the pass pipeline stays the same.
constexpr int kMaxSurfaceHeight = kChunkHeight - 6;  // headroom for trees
constexpr float kHeightFrequency = 1.0f / 24.0f;
constexpr float kMoistureFrequency = 1.0f / 40.0f;

// Hash seeds salt each feature pass so its decisions are independent of the
// others (deterministic for a fixed world seed).
constexpr std::uint32_t kCaveGateSalt = 0x517cc1b7u;
constexpr std::uint32_t kCaveDepthSalt = 0x11aaaf71u;
constexpr std::uint32_t kCaveWidthSalt = 0x31b50543u;
constexpr std::uint32_t kOreGateSalt = 0xa98b4f5du;
constexpr std::uint32_t kOreDepthSalt = 0x44bb7343u;
constexpr std::uint32_t kOreKindSalt = 0x9d6fbf21u;
constexpr std::uint32_t kTreeSalt = 0xcb9d1b91u;

}  // namespace

TerrainGenerator::TerrainGenerator(std::uint32_t seed)
    : seed_(seed),
      continent_(seed),
      moisture_(seed ^ 0x9e3779b9u),
      caveDensity_(seed ^ 0x85ebca6bu) {}

int TerrainGenerator::surfaceHeight(int x, int z) const {
    const float n = noise::fbm2D(
        continent_, static_cast<float>(x) * kHeightFrequency,
        static_cast<float>(z) * kHeightFrequency, 3, 2.0f, 0.5f);
    const float h01 = std::clamp((n + 1.0f) * 0.5f, 0.0f, 1.0f);  // [0, 1]
    return 1 + static_cast<int>(h01 * static_cast<float>(kMaxSurfaceHeight - 1));
}

float TerrainGenerator::moisture01(int x, int z) const {
    const float n = noise::fbm2D(
        moisture_, static_cast<float>(x) * kMoistureFrequency,
        static_cast<float>(z) * kMoistureFrequency, 3, 2.0f, 0.5f);
    return std::clamp((n + 1.0f) * 0.5f, 0.0f, 1.0f);  // [0, 1]
}

void TerrainGenerator::generate(Chunk& out, ChunkCoord c) const {
    const WorldPosition origin = chunkOrigin(c);
    for (int lx = 0; lx < kChunkSizeX; ++lx) {
        for (int lz = 0; lz < kChunkSizeZ; ++lz) {
            const int wx = origin.x + lx;
            const int wz = origin.z + lz;
            const int h = surfaceHeight(wx, wz);
            const bool shore = h <= kSeaLevel + 1;
            for (int y = 0; y < kChunkHeight; ++y) {
                BlockId block = BlockId::Air;
                if (y < h) {
                    if (y == h - 1) {
                        block = shore ? BlockId::Sand : BlockId::Grass;
                    } else if (shore && y >= h - 2) {
                        block = BlockId::Sand;
                    } else {
                        block = BlockId::Stone;
                    }
                } else if (y <= kSeaLevel && h < kSeaLevel) {
                    block = BlockId::Water;
                }
                out.setBlock({lx, y, lz}, block);
            }
        }
    }
    carveCaves(out, c);
    placeOres(out, c);
    placeTrees(out, c);
}

void TerrainGenerator::carveCaves(Chunk& out, ChunkCoord c) const {
    const WorldPosition origin = chunkOrigin(c);
    for (int lx = 0; lx < kChunkSizeX; ++lx) {
        for (int lz = 0; lz < kChunkSizeZ; ++lz) {
            const int wx = origin.x + lx;
            const int wz = origin.z + lz;
            // Smooth cave regions where the density field is high.
            if (caveDensity_.at(static_cast<float>(wx) * 0.125f,
                                static_cast<float>(wz) * 0.125f) < 0.55f) {
                continue;
            }
            const int h = surfaceHeight(wx, wz);
            const int bottom =
                2 + static_cast<int>(noise::hash01(wx, wz, seed_ ^ kCaveDepthSalt) * 8.0f);
            const int top =
                bottom + 1 + static_cast<int>(noise::hash01(wx, wz, seed_ ^ kCaveWidthSalt) * 3.0f);
            for (int y = bottom; y <= top && y < h - 1 && y < kChunkHeight; ++y) {
                const LocalCoord lc{lx, y, lz};
                if (out.blockAt(lc) == BlockId::Stone) {
                    out.setBlock(lc, BlockId::Air);
                }
            }
        }
    }
}

void TerrainGenerator::placeOres(Chunk& out, ChunkCoord c) const {
    const WorldPosition origin = chunkOrigin(c);
    for (int lx = 0; lx < kChunkSizeX; ++lx) {
        for (int lz = 0; lz < kChunkSizeZ; ++lz) {
            const int wx = origin.x + lx;
            const int wz = origin.z + lz;
            const int h = surfaceHeight(wx, wz);
            if (h <= kSeaLevel + 1) {
                continue;  // ore only underground on land
            }
            if (noise::hash01(wx, wz, seed_ ^ kOreGateSalt) >= 0.06f) {
                continue;
            }
            const int oy = 1 + static_cast<int>(
                noise::hash01(wx, wz, seed_ ^ kOreDepthSalt) *
                static_cast<float>(std::max(1, h - 4)));
            const bool coal = noise::hash01(wx, wz, seed_ ^ kOreKindSalt) < 0.6f;
            const BlockId ore = coal ? BlockId::CoalOre : BlockId::IronOre;
            for (int y = oy; y <= oy + 1 && y < h - 1 && y < kChunkHeight; ++y) {
                const LocalCoord lc{lx, y, lz};
                if (out.blockAt(lc) == BlockId::Stone) {
                    out.setBlock(lc, ore);
                }
            }
        }
    }
}

void TerrainGenerator::placeTrees(Chunk& out, ChunkCoord c) const {
    const WorldPosition origin = chunkOrigin(c);
    for (int lx = 0; lx < kChunkSizeX; ++lx) {
        for (int lz = 0; lz < kChunkSizeZ; ++lz) {
            const int wx = origin.x + lx;
            const int wz = origin.z + lz;
            const int h = surfaceHeight(wx, wz);
            // Trees on dry, moist land only, scattered by hash.
            if (h <= kSeaLevel + 1 || moisture01(wx, wz) < 0.35f ||
                noise::hash01(wx, wz, seed_ ^ kTreeSalt) >= 0.02f) {
                continue;
            }
            const int trunkTop = h + 3;
            const int canopyTop = trunkTop + 1;
            if (canopyTop >= kChunkHeight) {
                continue;  // near world ceiling; skip (should not happen given
                           // kMaxSurfaceHeight headroom, kept as a guard)
            }
            for (int y = h; y <= trunkTop; ++y) {
                out.setBlock({lx, y, lz}, BlockId::Wood);
            }
            // Canopy: full 3x3 ring at the top of the trunk, cross above.
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dz = -1; dz <= 1; ++dz) {
                    if (dx == 0 && dz == 0) {
                        continue;  // trunk occupies the center
                    }
                    const int nx = lx + dx;
                    const int nz = lz + dz;
                    if (nx < 0 || nx >= kChunkSizeX || nz < 0 || nz >= kChunkSizeZ) {
                        continue;  // canopy clipped at chunk border for now
                    }
                    if (out.blockAt({nx, trunkTop, nz}) == BlockId::Air) {
                        out.setBlock({nx, trunkTop, nz}, BlockId::Leaves);
                    }
                }
            }
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dz = -1; dz <= 1; ++dz) {
                    if (std::abs(dx) + std::abs(dz) > 1) {
                        continue;  // cross shape
                    }
                    const int nx = lx + dx;
                    const int nz = lz + dz;
                    if (nx < 0 || nx >= kChunkSizeX || nz < 0 || nz >= kChunkSizeZ) {
                        continue;
                    }
                    if (out.blockAt({nx, canopyTop, nz}) == BlockId::Air) {
                        out.setBlock({nx, canopyTop, nz}, BlockId::Leaves);
                    }
                }
            }
        }
    }
}

WorldGenerator::WorldGenerator(std::uint32_t seed, World& world)
    : gen_(seed), world_(world) {}

bool WorldGenerator::request(ChunkCoord c) {
    if (world_.chunkAt(c) != nullptr) {
        return false;  // already generated
    }
    const std::uint64_t key = chunkKey(c);
    if (queued_.count(key) != 0) {
        return false;  // already queued
    }
    queued_.insert(key);
    queue_.push_back(c);
    return true;
}

bool WorldGenerator::tick(std::size_t budget,
                          std::vector<ChunkCoord>* generated) {
    std::size_t done = 0;
    while (!queue_.empty() && done < budget) {
        const ChunkCoord c = queue_.front();
        queue_.pop_front();
        queued_.erase(chunkKey(c));
        Chunk* chunk = world_.loadChunk(c);
        gen_.generate(*chunk, c);
        if (generated != nullptr) {
            generated->push_back(c);
        }
        ++done;
    }
    return !queue_.empty();
}

}  // namespace voxel::world
