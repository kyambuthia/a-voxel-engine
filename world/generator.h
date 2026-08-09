#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
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
    float moisture01(int x, int z) const;
    void carveCaves(Chunk& out, ChunkCoord c) const;
    void placeOres(Chunk& out, ChunkCoord c) const;
    void placeTrees(Chunk& out, ChunkCoord c) const;

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
    WorldGenerator(std::uint32_t seed, World& world);

    std::uint32_t seed() const { return gen_.seed(); }

    // Queue a chunk for generation. Returns false if it is already loaded or
    // already queued.
    bool request(ChunkCoord c);

    std::size_t pending() const { return queue_.size(); }

    // Generates up to `budget` queued chunks into the world, appending each
    // newly generated coordinate to `generated` (when non-null) so the caller
    // can mesh/upload them. Returns true if work remains queued.
    bool tick(std::size_t budget = 1, std::vector<ChunkCoord>* generated = nullptr);

    // Height of the top solid surface at world (x, z); used to place the
    // camera and (later) the player. See TerrainGenerator::surfaceHeight.
    int surfaceHeight(int x, int z) const { return gen_.surfaceHeight(x, z); }

private:
    TerrainGenerator gen_;
    World& world_;
    std::deque<ChunkCoord> queue_;
    std::unordered_set<std::uint64_t> queued_;
};

}  // namespace voxel::world
