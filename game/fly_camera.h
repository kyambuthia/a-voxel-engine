#pragma once

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace voxel::game {

// Free-fly camera: a position plus yaw (around +Y) and pitch (around the
// horizontal right axis, positive looks up). This type only derives look
// directions and the view matrix; movement (with optional voxel collision) is
// applied by the controller. Pure math, no engine dependencies.
struct FlyCamera {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;    // radians, around +Y
    float pitch = 0.0f;  // radians, positive looks up

    // Direction the camera looks: (cosP*sinY, sinP, cosP*cosY).
    glm::vec3 forward() const {
        const float cp = std::cos(pitch);
        return glm::vec3(cp * std::sin(yaw), std::sin(pitch),
                         cp * std::cos(yaw));
    }

    // forward projected onto the XZ plane, normalized (movement direction).
    glm::vec3 horizontalForward() const {
        const float cp = std::cos(pitch);
        const glm::vec3 f(cp * std::sin(yaw), 0.0f, cp * std::cos(yaw));
        return glm::normalize(f);
    }

    // Screen-right vector on the XZ plane.
    glm::vec3 right() const {
        return glm::vec3(std::cos(yaw), 0.0f, -std::sin(yaw));
    }

    // View matrix from the camera position along forward().
    glm::mat4 view() const {
        return glm::lookAt(position, position + forward(),
                           glm::vec3(0.0f, 1.0f, 0.0f));
    }
};

}  // namespace voxel::game
