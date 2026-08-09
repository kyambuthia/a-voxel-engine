#include "world/world.h"

#include <algorithm>

namespace voxel::world {

void World::reset(std::uint32_t seed) {
    seed_ = seed;
    chunks_.clear();
}

Chunk* World::loadChunk(ChunkCoord c) {
    const std::uint64_t key = chunkKey(c);
    auto it = chunks_.find(key);
    if (it == chunks_.end()) {
        auto emplaced = chunks_.emplace(key, Chunk(c));
        return &emplaced.first->second;
    }
    return &it->second;
}

void World::unloadChunk(ChunkCoord c) { chunks_.erase(chunkKey(c)); }

Chunk* World::chunkAt(ChunkCoord c) {
    auto it = chunks_.find(chunkKey(c));
    return it == chunks_.end() ? nullptr : &it->second;
}

const Chunk* World::chunkAt(ChunkCoord c) const {
    auto it = chunks_.find(chunkKey(c));
    return it == chunks_.end() ? nullptr : &it->second;
}

std::vector<ChunkCoord> World::chunkCoords() const {
    std::vector<ChunkCoord> out;
    out.reserve(chunks_.size());
    for (const auto& kv : chunks_) {
        out.push_back(kv.second.coord());
    }
    std::sort(out.begin(), out.end(),
              [](ChunkCoord a, ChunkCoord b) {
                  return a.cx < b.cx || (a.cx == b.cx && a.cz < b.cz);
              });
    return out;
}

BlockId World::blockAt(WorldPosition p) const {
    if (!inWorldY(p.y)) {
        return BlockId::Air;
    }
    const Chunk* c = chunkAt(chunkCoordOf(p));
    return c ? c->blockAt(localCoordOf(p)) : BlockId::Air;
}

bool World::setBlock(WorldPosition p, BlockId b) {
    if (!inWorldY(p.y)) {
        return false;
    }
    loadChunk(chunkCoordOf(p))->setBlock(localCoordOf(p), b);
    return true;
}

}  // namespace voxel::world
