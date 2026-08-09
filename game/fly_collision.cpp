#include "game/fly_collision.h"

#include <algorithm>
#include <cmath>

#include "world/block.h"

namespace voxel::game {

bool flyPositionBlocked(const voxel::world::World& world,
                        const glm::vec3& eyePosition,
                        const FlyCollider& collider) {
    constexpr float kContactEpsilon = 1e-4f;
    const glm::vec3 minimum{eyePosition.x - collider.halfWidth,
                            eyePosition.y - collider.belowEye,
                            eyePosition.z - collider.halfWidth};
    const glm::vec3 maximum{eyePosition.x + collider.halfWidth,
                            eyePosition.y + collider.aboveEye,
                            eyePosition.z + collider.halfWidth};

    const int minX = static_cast<int>(std::floor(minimum.x));
    const int minY = static_cast<int>(std::floor(minimum.y));
    const int minZ = static_cast<int>(std::floor(minimum.z));
    const int maxX =
        static_cast<int>(std::floor(maximum.x - kContactEpsilon));
    const int maxY =
        static_cast<int>(std::floor(maximum.y - kContactEpsilon));
    const int maxZ =
        static_cast<int>(std::floor(maximum.z - kContactEpsilon));

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                if (collider.blockMissingChunks &&
                    world.chunkAt(voxel::world::chunkCoordOf({x, 0, z})) ==
                        nullptr) {
                    return true;
                }
                if (voxel::world::isSolid(world.blockAt({x, y, z}))) {
                    return true;
                }
            }
        }
    }
    return false;
}

float moveFlyAxis(const voxel::world::World& world, glm::vec3& eyePosition,
                  int axis, float distance, const FlyCollider& collider) {
    if (axis < 0 || axis > 2 || distance == 0.0f) {
        return 0.0f;
    }
    const float maxStep = std::max(collider.maxStep, 0.01f);
    const int steps =
        std::max(1, static_cast<int>(std::ceil(std::abs(distance) / maxStep)));
    const float step = distance / static_cast<float>(steps);
    const glm::vec3 start = eyePosition;
    for (int i = 0; i < steps; ++i) {
        glm::vec3 next = eyePosition;
        next[axis] += step;
        if (flyPositionBlocked(world, next, collider)) {
            break;
        }
        eyePosition = next;
    }
    return eyePosition[axis] - start[axis];
}

}  // namespace voxel::game
