#include "test_harness.h"

#include <glm/glm.hpp>

#include "game/fly_collision.h"
#include "world/block.h"
#include "world/world.h"

using voxel::game::flyPositionBlocked;
using voxel::game::moveFlyAxis;
using voxel::world::BlockId;
using voxel::world::World;

TEST(fly_collision_uses_player_width) {
    World world(1);
    world.setBlock({1, 1, 0}, BlockId::Stone);
    CHECK(!flyPositionBlocked(world, {0.69f, 2.0f, 0.5f}));
    CHECK(flyPositionBlocked(world, {0.71f, 2.0f, 0.5f}));
}

TEST(fly_collision_water_is_not_solid) {
    World world(1);
    world.setBlock({0, 0, 0}, BlockId::Water);
    CHECK(!flyPositionBlocked(world, {0.5f, 1.6f, 0.5f}));
}

TEST(fly_collision_stops_before_wall_without_tunnelling) {
    World world(1);
    for (int y = 0; y < 3; ++y) {
        world.setBlock({2, y, 0}, BlockId::Stone);
    }
    glm::vec3 eye{0.5f, 1.6f, 0.5f};
    const float travelled = moveFlyAxis(world, eye, 0, 5.0f);
    CHECK(travelled > 0.0f);
    CHECK(eye.x < 1.71f);
    CHECK(!flyPositionBlocked(world, eye));
}

TEST(fly_collision_axis_move_preserves_other_components) {
    World world(1);
    world.loadChunk({0, -1});
    glm::vec3 eye{0.5f, 5.0f, -2.0f};
    moveFlyAxis(world, eye, 2, 1.0f);
    CHECK_NEAR(eye.x, 0.5f, 1e-6);
    CHECK_NEAR(eye.y, 5.0f, 1e-6);
    CHECK_NEAR(eye.z, -1.0f, 1e-6);
}

TEST(fly_collision_blocks_entry_into_unloaded_chunks) {
    World world(1);
    world.loadChunk({0, 0});
    glm::vec3 eye{15.5f, 5.0f, 0.5f};
    moveFlyAxis(world, eye, 0, 2.0f);
    CHECK(eye.x < 15.71f);
}
