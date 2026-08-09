#pragma once

#include <glm/glm.hpp>

#include "world/world.h"

namespace voxel::game {

// Collision volume around the fly camera's eye position. The asymmetric
// vertical extent models a standing player while retaining free vertical
// movement.
struct FlyCollider {
    float halfWidth = 0.30f;
    float belowEye = 1.60f;
    float aboveEye = 0.20f;
    float maxStep = 0.25f;
    bool blockMissingChunks = true;
};

bool flyPositionBlocked(const voxel::world::World& world,
                        const glm::vec3& eyePosition,
                        const FlyCollider& collider = {});

// Move along one axis using bounded substeps. Returns the distance actually
// travelled; callers apply X/Z/Y independently to preserve wall sliding.
float moveFlyAxis(const voxel::world::World& world, glm::vec3& eyePosition,
                  int axis, float distance,
                  const FlyCollider& collider = {});

}  // namespace voxel::game
