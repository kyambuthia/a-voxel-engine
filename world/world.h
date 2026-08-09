#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "world/chunk.h"
#include "world/coords.h"

namespace voxel::world {

// Sparse, unbounded-in-XZ world of fixed-size column chunks. The world is a
// value-like contract: given the same seed and the same sequence of edits,
// block lookups are deterministic. Generation, streaming, raycast edits, and
// the edit journal live in later milestones; this module is storage + lookup
// only.
class World {
public:
    explicit World(std::uint32_t seed = 0) : seed_(seed) {}

    std::uint32_t seed() const { return seed_; }
    void reset(std::uint32_t seed);  // clears storage, sets a new seed

    // Chunk management. loadChunk creates an empty chunk if absent.
    Chunk* loadChunk(ChunkCoord c);
    void unloadChunk(ChunkCoord c);
    Chunk* chunkAt(ChunkCoord c);
    const Chunk* chunkAt(ChunkCoord c) const;
    std::size_t chunkCount() const { return chunks_.size(); }

    // Loaded chunk coords sorted by (cx, cz): deterministic iteration order.
    std::vector<ChunkCoord> chunkCoords() const;

    // Block queries. Missing chunks and out-of-vertical-span positions read
    // as Air. setBlock creates the chunk if needed and returns false only when
    // p.y is outside the world's vertical span.
    BlockId blockAt(WorldPosition p) const;
    bool setBlock(WorldPosition p, BlockId b);

private:
    std::uint32_t seed_ = 0;
    std::unordered_map<std::uint64_t, Chunk> chunks_;
};

}  // namespace voxel::world
