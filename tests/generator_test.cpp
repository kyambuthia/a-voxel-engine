// Terrain-generator contract tests: deterministic pass pipeline, surface
// topology, biome variety, and the frame-budgeted generation queue.

#include "test_harness.h"

#include "world/generator.h"

using namespace voxel::world;

TEST(terrain_deterministic_same_seed) {
    const TerrainGenerator a(2024u);
    const TerrainGenerator b(2024u);
    Chunk ca({0, 0});
    Chunk cb({0, 0});
    a.generate(ca, {0, 0});
    b.generate(cb, {0, 0});
    int diffs = 0;
    for (int i = 0; i < kBlocksPerChunk; ++i) {
        if (ca.data()[i] != cb.data()[i]) {
            ++diffs;
        }
    }
    CHECK_EQ(diffs, 0);
}

TEST(terrain_seed_changes_terrain) {
    const TerrainGenerator a(1u);
    const TerrainGenerator b(2u);
    Chunk ca({0, 0});
    Chunk cb({0, 0});
    a.generate(ca, {0, 0});
    b.generate(cb, {0, 0});
    int diffs = 0;
    for (int i = 0; i < kBlocksPerChunk; ++i) {
        if (ca.data()[i] != cb.data()[i]) {
            ++diffs;
        }
    }
    CHECK(diffs > 0);
}

TEST(terrain_surface_topology) {
    const TerrainGenerator g(2024u);
    Chunk chunk({0, 0});
    g.generate(chunk, {0, 0});
    for (int lx = 0; lx < kChunkSizeX; ++lx) {
        for (int lz = 0; lz < kChunkSizeZ; ++lz) {
            const int h = g.surfaceHeight(lx, lz);
            CHECK(h >= 1 && h <= kChunkHeight - 6);
            const BlockId top = chunk.blockAt({lx, h - 1, lz});
            CHECK(top == BlockId::Grass || top == BlockId::Sand);
            const BlockId above = chunk.blockAt({lx, h, lz});
            if (h < TerrainGenerator::kSeaLevel) {
                CHECK(above == BlockId::Water);
            } else {
                // A tree trunk/canopy may occupy the cell right above the
                // surface; otherwise it must be open air.
                CHECK(above == BlockId::Air || above == BlockId::Wood ||
                      above == BlockId::Leaves);
            }
        }
    }
}

TEST(terrain_water_fills_basins) {
    const TerrainGenerator g(2024u);
    Chunk chunk({0, 0});
    g.generate(chunk, {0, 0});
    for (int lx = 0; lx < kChunkSizeX; ++lx) {
        for (int lz = 0; lz < kChunkSizeZ; ++lz) {
            const int h = g.surfaceHeight(lx, lz);
            if (h < TerrainGenerator::kSeaLevel) {
                CHECK(chunk.blockAt({lx, h, lz}) == BlockId::Water);
                CHECK(chunk.blockAt({lx, TerrainGenerator::kSeaLevel, lz}) ==
                      BlockId::Water);
            } else {
                CHECK(chunk.blockAt({lx, TerrainGenerator::kSeaLevel, lz}) !=
                      BlockId::Water);
            }
        }
    }
}

TEST(terrain_has_biome_variety) {
    const TerrainGenerator g(2024u);
    int wood = 0, leaves = 0, ore = 0, grass = 0, sand = 0, water = 0;
    for (int cx = -4; cx <= 4; ++cx) {
        for (int cz = -4; cz <= 4; ++cz) {
            Chunk chunk({cx, cz});
            g.generate(chunk, {cx, cz});
            for (int i = 0; i < kBlocksPerChunk; ++i) {
                switch (chunk.data()[i]) {
                    case BlockId::Wood:
                        ++wood;
                        break;
                    case BlockId::Leaves:
                        ++leaves;
                        break;
                    case BlockId::CoalOre:
                    case BlockId::IronOre:
                        ++ore;
                        break;
                    case BlockId::Grass:
                        ++grass;
                        break;
                    case BlockId::Sand:
                        ++sand;
                        break;
                    case BlockId::Water:
                        ++water;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    CHECK(grass > 0);
    CHECK(sand > 0);
    CHECK(water > 0);
    CHECK(wood > 0);
    CHECK(leaves > 0);
    CHECK(ore > 0);
}

TEST(terrain_surface_height_in_range) {
    const TerrainGenerator g(77u);
    for (int x = -64; x <= 64; x += 7) {
        for (int z = -64; z <= 64; z += 7) {
            const int h = g.surfaceHeight(x, z);
            CHECK(h >= 1 && h <= kChunkHeight - 6);
        }
    }
}

TEST(world_generator_queue_and_budget) {
    World world(5u);
    WorldGenerator gen(5u, world);
    CHECK(gen.request({0, 0}));
    CHECK(gen.request({1, 0}));
    CHECK(gen.request({-1, 2}));
    CHECK_EQ(gen.pending(), 3u);
    CHECK(!gen.request({0, 0}));  // duplicate rejected

    CHECK(gen.tick(1));           // budget limits work per tick
    CHECK_EQ(gen.pending(), 2u);
    CHECK(world.chunkAt({0, 0}) != nullptr);
    CHECK(world.chunkAt({1, 0}) == nullptr);

    while (gen.tick(1)) {
    }
    CHECK_EQ(gen.pending(), 0u);
    CHECK(world.chunkAt({1, 0}) != nullptr);
    CHECK(world.chunkAt({-1, 2}) != nullptr);
    CHECK(!gen.request({0, 0}));  // already loaded

    int nonAir = 0;
    const Chunk* c = world.chunkAt({0, 0});
    for (int i = 0; i < kBlocksPerChunk; ++i) {
        if (c->data()[i] != BlockId::Air) {
            ++nonAir;
        }
    }
    CHECK(nonAir > 0);
}

TEST(world_generator_determinism) {
    World wa(9u);
    World wb(9u);
    WorldGenerator ga(9u, wa);
    WorldGenerator gb(9u, wb);
    const ChunkCoord reqs[] = {{0, 0}, {1, 1}, {-2, 3}, {4, -5}};
    for (const ChunkCoord c : reqs) {
        ga.request(c);
        gb.request(c);
    }
    while (ga.tick(1)) {
    }
    while (gb.tick(1)) {
    }
    for (const ChunkCoord c : reqs) {
        const Chunk* ca = wa.chunkAt(c);
        const Chunk* cb = wb.chunkAt(c);
        CHECK(ca != nullptr && cb != nullptr);
        if (ca != nullptr && cb != nullptr) {
            int diffs = 0;
            for (int i = 0; i < kBlocksPerChunk; ++i) {
                if (ca->data()[i] != cb->data()[i]) {
                    ++diffs;
                }
            }
            CHECK_EQ(diffs, 0);
        }
    }
}
