#pragma once

#include <cstdint>

namespace voxel::world {

// Signed 32-bit coordinate component. 32 bits keeps chunk keys and math
// deterministic and compact on both desktop and Android.
using Coord = std::int32_t;

// Horizontal footprint and vertical span of a single chunk, in block units.
// A chunk is a vertical column of blocks; the XZ footprint is the standard
// 16x16 and the vertical span is a tunable constant (currently taller than the
// 8-layer columns observed in the reference target, leaving room for our own
// terrain tuning). Raise kChunkHeight to support taller worlds.
constexpr int kChunkSizeX = 16;
constexpr int kChunkSizeZ = 16;
constexpr int kChunkHeight = 32;
constexpr int kBlocksPerChunk = kChunkSizeX * kChunkHeight * kChunkSizeZ;  // 8192

// Global block position. Y is measured from the bottom of the world
// (y in [0, kChunkHeight) is the only valid vertical span).
struct WorldPosition {
    Coord x = 0;
    Coord y = 0;
    Coord z = 0;
};

// Column chunk index in the XZ plane. Each chunk spans the full world height.
struct ChunkCoord {
    Coord cx = 0;
    Coord cz = 0;
};

// Position inside a chunk: x in [0, kChunkSizeX), y in [0, kChunkHeight),
// z in [0, kChunkSizeZ).
struct LocalCoord {
    int x = 0;
    int y = 0;
    int z = 0;
};

// Floor division (rounds toward negative infinity) so chunk math is valid for
// negative world coordinates and deterministic across platforms.
inline constexpr Coord floorDiv(Coord a, Coord b) {
    const Coord q = a / b;
    const Coord r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        return q - 1;
    }
    return q;
}

// Chunk that owns the given world position.
inline constexpr ChunkCoord chunkCoordOf(WorldPosition p) {
    return ChunkCoord{floorDiv(p.x, kChunkSizeX), floorDiv(p.z, kChunkSizeZ)};
}

// World position of the lowest-x/lowest-z corner of a chunk.
inline constexpr WorldPosition chunkOrigin(ChunkCoord c) {
    return WorldPosition{c.cx * kChunkSizeX, 0, c.cz * kChunkSizeZ};
}

// Position of p inside its chunk. Precondition: p.y in [0, kChunkHeight).
inline constexpr LocalCoord localCoordOf(WorldPosition p) {
    const WorldPosition o = chunkOrigin(chunkCoordOf(p));
    return LocalCoord{static_cast<int>(p.x - o.x), static_cast<int>(p.y),
                      static_cast<int>(p.z - o.z)};
}

inline constexpr bool inWorldY(Coord y) {
    return y >= 0 && y < kChunkHeight;
}

inline constexpr bool inChunkBounds(LocalCoord c) {
    return c.x >= 0 && c.x < kChunkSizeX && c.y >= 0 && c.y < kChunkHeight &&
           c.z >= 0 && c.z < kChunkSizeZ;
}

// Flat index into the chunk block array: Y-innermost (stride 1), then X, then
// Z, so a vertical column of blocks at a fixed (x, z) is contiguous in memory.
// This matches the column-oriented terrain and per-column mesh building of the
// reference architecture.
inline constexpr int blockIndex(LocalCoord c) {
    return c.y + c.x * kChunkHeight + c.z * kChunkHeight * kChunkSizeX;
}

// Inverse of blockIndex.
inline constexpr LocalCoord localFromIndex(int i) {
    const int y = i % kChunkHeight;
    const int x = (i / kChunkHeight) % kChunkSizeX;
    const int z = i / (kChunkHeight * kChunkSizeX);
    return LocalCoord{x, y, z};
}

// Opaque unique key for a chunk coordinate (cx, cz packed into 64 bits). Used
// as the hash key for chunk storage and generation queues.
inline std::uint64_t chunkKey(ChunkCoord c) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.cx)) << 32) |
           static_cast<std::uint32_t>(c.cz);
}

}  // namespace voxel::world
