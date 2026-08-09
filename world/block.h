#pragma once

#include <cstdint>

namespace voxel::world {

// Block identifiers used across the world core. Values are stored packed in
// chunks and in edit journals, so they must be stable and serializable: never
// reorder or renumber existing entries. The set mirrors the descriptors
// observed in the reference target (stone, grass, sand, water, gravel, two
// ores, tree/trunk) plus air.
enum class BlockId : std::uint8_t {
    Air = 0,
    Stone = 1,
    Grass = 2,
    Sand = 3,
    Water = 4,
    Gravel = 5,
    CoalOre = 6,
    IronOre = 7,
    Wood = 8,     // trunk
    Leaves = 9,
    Count = 10,
};

// A block the player can stand on / collide with. Water is not solid.
inline constexpr bool isSolid(BlockId b) {
    return b != BlockId::Air && b != BlockId::Water;
}

// A block that occludes the block behind it for face culling.
inline constexpr bool isOpaque(BlockId b) {
    return b != BlockId::Air && b != BlockId::Water;
}

}  // namespace voxel::world
