#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "world/chunk.h"
#include "world/coords.h"
#include "world/noise.h"
#include "world/world.h"

namespace voxel::world {

// Pure, deterministic terrain generator (clean-room plan M1). Given the world
// seed it fills a chunk column with block data through a pass pipeline in the
// reference architecture's order: height field -> voxelize -> caves -> ores ->
// trees. Stateless and re-entrant; safe to call from any thread.
class TerrainGenerator {
public:
    explicit TerrainGenerator(std::uint32_t seed);

    std::uint32_t seed() const { return seed_; }

    // Fills every block of `out` from the world seed; fully defines the chunk
    // (every cell is written, so re-generation overwrites cleanly).
    void generate(Chunk& out, ChunkCoord c) const;

    // Height of the top solid surface at world (x, z), in [1, kChunkHeight-6].
    // Basins below kSeaLevel are covered by water. Deterministic.
    int surfaceHeight(int x, int z) const;

    static constexpr int kSeaLevel = 8;

private:
    using BlockStorage = Chunk::BlockStorage;

    float moisture01(int x, int z) const;
    void carveCaves(BlockStorage& blocks, ChunkCoord c) const;
    void placeOres(BlockStorage& blocks, ChunkCoord c) const;
    void placeTrees(BlockStorage& blocks, ChunkCoord c) const;

    std::uint32_t seed_;
    noise::GradientNoise2D continent_;
    noise::GradientNoise2D moisture_;
    noise::ValueNoise2D caveDensity_;
};

// Frame-budgeted generation queue. request() adds chunk columns; tick()
// generates up to `budget` chunks per call so generation spreads across frames
// instead of stalling the game loop. Runs on the game thread, never inside the
// renderer's frame submission.
class WorldGenerator {
public:
    explicit WorldGenerator(World& world);
    WorldGenerator(std::uint32_t seed, World& world);

    std::uint32_t seed() const;

    // Queue a chunk for generation. Returns false if it is already loaded or
    // already queued.
    bool request(ChunkCoord c);

    // Reprioritize pending work around the current streaming center. Nearest
    // chunks are generated first; ties are stable by coordinate.
    void setPriorityCenter(ChunkCoord center) { priorityCenter_ = center; }

    // Cancel queued work that is no longer in the active interest set.
    bool cancel(ChunkCoord c);
    std::size_t cancelOutside(ChunkCoord center, std::uint32_t radius);

    std::size_t pending() const { return queue_.size(); }

    // Generates up to `budget` queued chunks into the world, appending each
    // newly generated coordinate to `generated` (when non-null) so the caller
    // can mesh/upload them. Returns true if work remains queued.
    bool tick(std::size_t budget = 1, std::vector<ChunkCoord>* generated = nullptr);

    // Height of the top solid surface at world (x, z); used to place the
    // camera and (later) the player. See TerrainGenerator::surfaceHeight.
    int surfaceHeight(int x, int z) const;

private:
    void validateSeed() const;

    TerrainGenerator gen_;
    World& world_;
    std::vector<ChunkCoord> queue_;
    std::unordered_set<std::uint64_t> queued_;
    ChunkCoord priorityCenter_{0, 0};
};

}  // namespace voxel::world
