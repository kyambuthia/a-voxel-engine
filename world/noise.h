#pragma once

#include <cstdint>

namespace voxel::world::noise {

// Deterministic noise primitives used by terrain generation (clean-room plan
// M1). All outputs are a pure function of the inputs and the seed, so the same
// seed produces identical terrain on PC and Android.
//
// The recipes mirror the reference architecture's documented approach
// (skewed-lattice 2D gradient noise, per-column integer hash, value-noise
// detail maps) but are implemented independently with our own permutation
// tables; observed constants are treated as re-tunable parameters.

// Simplex-style 2D gradient noise on a skewed lattice (F2/G2 simplex skew).
// Lattice gradients are selected from a seed-derived 256-entry permutation
// table and a 12-gradient set. The value includes the observed amplitude
// scale of 70, giving roughly [-1, 1]; callers apply frequency / octave
// stacking on top.
class GradientNoise2D {
public:
    explicit GradientNoise2D(std::uint32_t seed = 0);

    std::uint32_t seed() const { return seed_; }

    // Noise at (x, y); approximately in [-1, 1].
    float at(float x, float y) const;

private:
    std::uint8_t perm_[256];
    std::uint32_t seed_;
};

// Lattice value noise with quintic interpolation, in [0, 1]. Used for the
// 16x16 detail maps in the generation pipeline.
class ValueNoise2D {
public:
    explicit ValueNoise2D(std::uint32_t seed = 0);

    std::uint32_t seed() const { return seed_; }

    // Noise at (x, y); in [0, 1].
    float at(float x, float y) const;

private:
    std::uint8_t perm_[256];
    std::uint32_t seed_;
};

// Deterministic 32-bit hash for per-column decisions (caves, ores, material
// choices, tree placement). Re-tuned variant of the reference architecture's
// documented integer hash (linear combination + xor/rotate mixing).
std::uint32_t hash2(std::int32_t x, std::int32_t z, std::uint32_t seed = 0);

// hash2 normalized to [0, 1).
float hash01(std::int32_t x, std::int32_t z, std::uint32_t seed = 0);

// Multi-octave fractal sum (fBm) over a gradient-noise field, normalized to
// the same approximate range as a single octave.
float fbm2D(const GradientNoise2D& noise, float x, float y, int octaves = 4,
            float lacunarity = 2.0f, float gain = 0.5f);

}  // namespace voxel::world::noise
