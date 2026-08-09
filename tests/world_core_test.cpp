// World-core contract tests: coordinate math, chunk storage, and deterministic
// block lookup. These mirror the clean-room plan's "First implementation task"
// and run on the desktop host (CTest).

#include "test_harness.h"

#include "world/block.h"
#include "world/coords.h"
#include "world/world.h"

using namespace voxel::world;

TEST(floor_div_positive) {
    CHECK_EQ(floorDiv(0, 16), 0);
    CHECK_EQ(floorDiv(15, 16), 0);
    CHECK_EQ(floorDiv(16, 16), 1);
    CHECK_EQ(floorDiv(17, 16), 1);
    CHECK_EQ(floorDiv(31, 16), 1);
}

TEST(floor_div_negative) {
    CHECK_EQ(floorDiv(-1, 16), -1);
    CHECK_EQ(floorDiv(-15, 16), -1);
    CHECK_EQ(floorDiv(-16, 16), -1);
    CHECK_EQ(floorDiv(-17, 16), -2);
    CHECK_EQ(floorDiv(-32, 16), -2);
    CHECK_EQ(floorDiv(-33, 16), -3);
}

TEST(chunk_coord_conversion) {
    CHECK_EQ(chunkCoordOf({0, 0, 0}).cx, 0);
    CHECK_EQ(chunkCoordOf({0, 0, 0}).cz, 0);
    CHECK_EQ(chunkCoordOf({17, 3, -1}).cx, 1);
    CHECK_EQ(chunkCoordOf({17, 3, -1}).cz, -1);
    CHECK_EQ(chunkCoordOf({-1, 5, -17}).cx, -1);
    CHECK_EQ(chunkCoordOf({-1, 5, -17}).cz, -2);
    CHECK_EQ(chunkCoordOf({-16, 0, -16}).cx, -1);
    CHECK_EQ(chunkCoordOf({-16, 0, -16}).cz, -1);
}

TEST(local_coord_and_index_roundtrip) {
    const WorldPosition p{-1, 5, -17};
    const LocalCoord l = localCoordOf(p);
    CHECK_EQ(l.x, 15);
    CHECK_EQ(l.y, 5);
    CHECK_EQ(l.z, 15);

    const WorldPosition o = chunkOrigin(chunkCoordOf(p));
    CHECK_EQ(o.x, -16);
    CHECK_EQ(o.z, -32);

    CHECK(blockIndex(l) >= 0 && blockIndex(l) < kBlocksPerChunk);
    const LocalCoord back = localFromIndex(blockIndex(l));
    CHECK_EQ(back.x, l.x);
    CHECK_EQ(back.y, l.y);
    CHECK_EQ(back.z, l.z);
}

TEST(block_index_is_column_contiguous) {
    // A vertical column (fixed x, z) maps to kChunkHeight consecutive slots.
    const LocalCoord base{3, 0, 7};
    const int first = blockIndex(base);
    const int last = blockIndex(LocalCoord{3, kChunkHeight - 1, 7});
    CHECK_EQ(last - first + 1, kChunkHeight);
}

TEST(new_chunk_is_all_air) {
    World w(42);
    Chunk* c = w.loadChunk({0, 0});
    CHECK(c != nullptr);
    if (c == nullptr) {
        return;
    }
    int nonAir = 0;
    for (int i = 0; i < kBlocksPerChunk; ++i) {
        if (c->data()[i] != BlockId::Air) {
            ++nonAir;
        }
    }
    CHECK_EQ(nonAir, 0);
}

TEST(set_and_get_roundtrip) {
    World w(7);
    CHECK(w.setBlock({1, 2, 3}, BlockId::Grass));
    CHECK(w.blockAt({1, 2, 3}) == BlockId::Grass);
    CHECK(w.blockAt({0, 2, 3}) == BlockId::Air);
    w.setBlock({1, 2, 3}, BlockId::Air);
    CHECK(w.blockAt({1, 2, 3}) == BlockId::Air);
}

TEST(negative_coordinates_roundtrip) {
    World w(7);
    const WorldPosition p{-5, 3, -9};
    CHECK(w.setBlock(p, BlockId::Stone));
    CHECK(w.blockAt(p) == BlockId::Stone);
    CHECK(w.blockAt({-4, 3, -9}) == BlockId::Air);
    CHECK(w.blockAt({-5, 3, -10}) == BlockId::Air);
}

TEST(unloaded_chunk_reads_air) {
    World w(1);
    CHECK(w.blockAt({100, 0, 100}) == BlockId::Air);
    CHECK(w.chunkAt({6, 6}) == nullptr);
}

TEST(setblock_creates_chunk) {
    World w(1);
    CHECK(w.setBlock({16, 0, 16}, BlockId::Sand));
    CHECK_EQ(w.chunkCount(), 1u);
    CHECK(w.chunkAt({1, 1}) != nullptr);
    CHECK(w.chunkAt({0, 0}) == nullptr);
}

TEST(vertical_bounds) {
    World w(1);
    CHECK(!w.setBlock({0, kChunkHeight, 0}, BlockId::Stone));  // just above
    CHECK(!w.setBlock({0, -1, 0}, BlockId::Stone));
    CHECK(w.setBlock({0, kChunkHeight - 1, 0}, BlockId::Stone));
    CHECK(w.blockAt({0, kChunkHeight, 0}) == BlockId::Air);
    CHECK(w.blockAt({0, -1, 0}) == BlockId::Air);
    CHECK_EQ(w.chunkCount(), 1u);
}

TEST(chunk_dirty_and_version) {
    Chunk c({2, 3});
    CHECK(!c.dirty());
    CHECK_EQ(c.editVersion(), 0u);
    c.setBlock({0, 0, 0}, BlockId::Gravel);
    CHECK(c.dirty());
    CHECK_EQ(c.editVersion(), 1u);
    c.clearDirty();
    CHECK(!c.dirty());
    CHECK_EQ(c.editVersion(), 1u);
    c.setBlock({0, 0, 0}, BlockId::Stone);
    CHECK_EQ(c.editVersion(), 2u);

    c.clearDirty();
    CHECK(!c.setBlock({0, 0, 0}, BlockId::Stone));
    CHECK(!c.dirty());
    CHECK_EQ(c.editVersion(), 2u);
}

TEST(chunk_bulk_replace_is_one_coherent_change) {
    Chunk c({2, 3});
    Chunk::BlockStorage blocks{};

    CHECK(!c.replaceBlocks(blocks));
    CHECK_EQ(c.editVersion(), 0u);
    CHECK(!c.dirty());

    blocks[blockIndex({3, 4, 5})] = BlockId::CoalOre;
    blocks[blockIndex({4, 4, 5})] = BlockId::IronOre;
    CHECK(c.replaceBlocks(blocks));
    CHECK_EQ(c.editVersion(), 1u);
    CHECK(c.dirty());
    CHECK(c.blockAt({3, 4, 5}) == BlockId::CoalOre);
    CHECK(c.blockAt({4, 4, 5}) == BlockId::IronOre);

    c.clearDirty();
    CHECK(!c.replaceBlocks(blocks));
    CHECK_EQ(c.editVersion(), 1u);
    CHECK(!c.dirty());
}

TEST(chunk_bounds_accessors) {
    Chunk c({0, 0});
    c.setBlock({15, kChunkHeight - 1, 15}, BlockId::Water);
    CHECK(c.blockAt({15, kChunkHeight - 1, 15}) == BlockId::Water);
    // Out-of-bounds local coords are ignored on write / read as Air.
    c.setBlock({16, 0, 0}, BlockId::Stone);
    CHECK(c.blockAt({16, 0, 0}) == BlockId::Air);
    CHECK_EQ(c.editVersion(), 1u);
}

TEST(determinism_same_seed_and_edits) {
    World a(99);
    World b(99);
    const WorldPosition pts[] = {{-5, 2, -5}, {0, 0, 0}, {1234, 3, -567},
                                 {-100, 1, 77}};
    const BlockId blocks[] = {BlockId::Grass, BlockId::Stone, BlockId::Wood,
                              BlockId::Water};
    for (int i = 0; i < 4; ++i) {
        a.setBlock(pts[i], blocks[i]);
        b.setBlock(pts[i], blocks[i]);
    }
    for (int i = 0; i < 4; ++i) {
        CHECK(a.blockAt(pts[i]) == b.blockAt(pts[i]));
    }
    CHECK_EQ(a.chunkCount(), b.chunkCount());
    CHECK_EQ(a.seed(), b.seed());
}

TEST(unload_and_reset) {
    World w(5);
    w.setBlock({0, 0, 0}, BlockId::Stone);
    CHECK_EQ(w.chunkCount(), 1u);
    w.unloadChunk({0, 0});
    CHECK_EQ(w.chunkCount(), 0u);
    CHECK(w.blockAt({0, 0, 0}) == BlockId::Air);

    w.setBlock({0, 0, 0}, BlockId::Stone);
    w.reset(6);
    CHECK_EQ(w.chunkCount(), 0u);
    CHECK_EQ(w.seed(), 6u);
    CHECK(w.blockAt({0, 0, 0}) == BlockId::Air);
}

TEST(chunk_coords_sorted) {
    World w(1);
    w.setBlock({16, 0, 0}, BlockId::Stone);    // chunk (1, 0)
    w.setBlock({0, 0, 16}, BlockId::Stone);    // chunk (0, 1)
    w.setBlock({0, 0, 0}, BlockId::Stone);     // chunk (0, 0)
    w.setBlock({-16, 0, 0}, BlockId::Stone);   // chunk (-1, 0)
    const std::vector<ChunkCoord> coords = w.chunkCoords();
    CHECK_EQ(coords.size(), 4u);
    CHECK_EQ(coords[0].cx, -1);
    CHECK_EQ(coords[0].cz, 0);
    CHECK_EQ(coords[1].cx, 0);
    CHECK_EQ(coords[1].cz, 0);
    CHECK_EQ(coords[2].cx, 0);
    CHECK_EQ(coords[2].cz, 1);
    CHECK_EQ(coords[3].cx, 1);
    CHECK_EQ(coords[3].cz, 0);
}

TEST(block_flags) {
    CHECK(isSolid(BlockId::Stone));
    CHECK(!isSolid(BlockId::Air));
    CHECK(!isSolid(BlockId::Water));
    CHECK(!isOpaque(BlockId::Air));
    CHECK(!isOpaque(BlockId::Water));
    CHECK(isOpaque(BlockId::Grass));
    CHECK(isOpaque(BlockId::Leaves));
}
