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
    using BlockStorage = std::array<BlockId, kBlocksPerChunk>;

    Chunk() = default;
    explicit Chunk(ChunkCoord coord) : coord_(coord) {}

    ChunkCoord coord() const { return coord_; }

    // In-bounds accessors. Out-of-bounds local coords are treated as Air /
    // ignored; use World-level queries for safe lookups at world positions.
    BlockId blockAt(LocalCoord c) const {
        return inChunkBounds(c) ? blocks_[blockIndex(c)] : BlockId::Air;
    }
    bool setBlock(LocalCoord c, BlockId b) {
        if (!inChunkBounds(c)) {
            return false;
        }
        BlockId& current = blocks_[blockIndex(c)];
        if (current == b) {
            return false;
        }
        current = b;
        ++editVersion_;
        dirty_ = true;
        return true;
    }

    // Atomically replace the complete chunk contents. Terrain generation uses
    // this path so consumers observe one coherent version change rather than
    // thousands of per-block edits. Returns false when content is unchanged.
    bool replaceBlocks(const BlockStorage& blocks) {
        if (blocks_ == blocks) {
            return false;
        }
        blocks_ = blocks;
        ++editVersion_;
        dirty_ = true;
        return true;
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
    BlockStorage blocks_{};
    std::uint32_t editVersion_ = 0;
    bool dirty_ = false;
};

}  // namespace voxel::world
