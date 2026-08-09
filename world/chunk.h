#pragma once

#include <array>
#include <cstdint>

#include "world/block.h"
#include "world/coords.h"

namespace voxel::world {

// Fixed-size vertical-column chunk: kChunkSizeX x kChunkHeight x kChunkSizeZ
// blocks. Default-constructed chunks are entirely Air.
class Chunk {
public:
    Chunk() = default;
    explicit Chunk(ChunkCoord coord) : coord_(coord) {}

    ChunkCoord coord() const { return coord_; }

    // In-bounds accessors. Out-of-bounds local coords are treated as Air /
    // ignored; use World-level queries for safe lookups at world positions.
    BlockId blockAt(LocalCoord c) const {
        return inChunkBounds(c) ? blocks_[blockIndex(c)] : BlockId::Air;
    }
    void setBlock(LocalCoord c, BlockId b) {
        if (!inChunkBounds(c)) {
            return;
        }
        blocks_[blockIndex(c)] = b;
        ++editVersion_;
        dirty_ = true;
    }

    const BlockId* data() const { return blocks_.data(); }
    std::size_t blockCount() const { return blocks_.size(); }

    // Monotonic edit counter (for meshing change detection) and content-dirty
    // flag (set on any edit, cleared once the chunk is meshed).
    std::uint32_t editVersion() const { return editVersion_; }
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

private:
    ChunkCoord coord_{0, 0};
    std::array<BlockId, kBlocksPerChunk> blocks_{};
    std::uint32_t editVersion_ = 0;
    bool dirty_ = false;
};

}  // namespace voxel::world
