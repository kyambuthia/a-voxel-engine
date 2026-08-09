// Free-fly camera math: direction vectors and view matrix.

#include "test_harness.h"

#include <glm/glm.hpp>

#include "game/fly_camera.h"

using voxel::game::FlyCamera;

TEST(fly_camera_forward_at_yaw_zero) {
    FlyCamera cam;
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;
    const glm::vec3 f = cam.forward();
    CHECK_NEAR(f.x, 0.0f, 1e-6);
    CHECK_NEAR(f.y, 0.0f, 1e-6);
    CHECK_NEAR(f.z, 1.0f, 1e-6);
}

TEST(fly_camera_forward_yaw_quarter) {
    FlyCamera cam;
    cam.yaw = 3.14159265f / 2.0f;
    cam.pitch = 0.0f;
    const glm::vec3 f = cam.forward();
    CHECK_NEAR(f.x, 1.0f, 1e-6);
    CHECK_NEAR(f.y, 0.0f, 1e-6);
    CHECK_NEAR(f.z, 0.0f, 1e-6);
}

TEST(fly_camera_pitch_up) {
    FlyCamera cam;
    cam.yaw = 0.0f;
    cam.pitch = 0.5f;
    const glm::vec3 f = cam.forward();
    CHECK(f.y > 0.0f);  // positive pitch looks up
    CHECK_NEAR(glm::length(f), 1.0f, 1e-6);
}

TEST(fly_camera_directions_orthonormal) {
    FlyCamera cam;
    cam.yaw = 0.7f;
    cam.pitch = 0.3f;
    const glm::vec3 f = cam.forward();
    const glm::vec3 h = cam.horizontalForward();
    const glm::vec3 r = cam.right();
    CHECK_NEAR(glm::length(f), 1.0f, 1e-6);
    CHECK_NEAR(glm::length(h), 1.0f, 1e-6);
    CHECK_NEAR(glm::length(r), 1.0f, 1e-6);
    CHECK_NEAR(glm::dot(f, r), 0.0f, 1e-6);
    CHECK_NEAR(h.y, 0.0f, 1e-6);
    CHECK(glm::dot(h, f) > 0.0f);  // horizontal movement is along forward
}

TEST(fly_camera_view_looks_along_forward) {
    FlyCamera cam;
    cam.position = glm::vec3(0.0f, 0.0f, 0.0f);
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;
    // A point in front of the camera maps to negative view-space depth.
    const glm::vec4 front = cam.view() * glm::vec4(0.0f, 0.0f, 5.0f, 1.0f);
    const glm::vec4 behind = cam.view() * glm::vec4(0.0f, 0.0f, -5.0f, 1.0f);
    CHECK(front.z < 0.0f);
    CHECK(behind.z > 0.0f);
}
