#include "world/noise.h"

#include <algorithm>
#include <cmath>

namespace voxel::world::noise {
namespace {

// Simplex skew / unskew constants: F2 = 0.5*(sqrt(3)-1), G2 = (3-sqrt(3))/6.
constexpr float kF2 = 0.3660254037844386f;
constexpr float kG2 = 0.21132486540518713f;

// The 12-gradient 2D set: (+-1,+-1), (+-1,0) x2, (0,+-1) x2.
constexpr float kGrad2[12][2] = {
    {1.0f, 1.0f},  {-1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f},
    {1.0f, 0.0f},  {-1.0f, 0.0f}, {1.0f, 0.0f},  {-1.0f, 0.0f},
    {0.0f, 1.0f},  {0.0f, -1.0f}, {0.0f, 1.0f},  {0.0f, -1.0f},
};

// Deterministic 32-bit generator (splitmix32) used only to build permutation
// tables from a seed. The tables themselves are our own, not a copy.
std::uint32_t splitmix32(std::uint32_t& state) {
    std::uint32_t z = (state += 0x9e3779b9u);
    z = (z ^ (z >> 16)) * 0x21f0aaadu;
    z = (z ^ (z >> 15)) * 0x735a2d97u;
    z ^= z >> 15;
    return z;
}

// Seed-derived 256-entry permutation table (Fisher-Yates shuffle).
void buildPermutation(std::uint8_t perm[256], std::uint32_t seed) {
    for (int i = 0; i < 256; ++i) {
        perm[i] = static_cast<std::uint8_t>(i);
    }
    std::uint32_t state = seed;
    for (int i = 255; i > 0; --i) {
        const std::uint32_t j =
            splitmix32(state) % (static_cast<std::uint32_t>(i) + 1u);
        std::swap(perm[i], perm[static_cast<int>(j)]);
    }
}

// Quintic fade curve: t^3 * (t * (t * 6 - 15) + 10).
inline float quintic(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline int floorToInt(float v) { return static_cast<int>(std::floor(v)); }

}  // namespace

GradientNoise2D::GradientNoise2D(std::uint32_t seed) : seed_(seed) {
    buildPermutation(perm_, seed_);
}

float GradientNoise2D::at(float x, float y) const {
    // Skew the input to the simplex lattice.
    const float s = (x + y) * kF2;
    const int i = floorToInt(x + s);
    const int j = floorToInt(y + s);
    const float t = static_cast<float>(i + j) * kG2;

    // Coordinates of the three simplex corners.
    const float x0 = x - static_cast<float>(i) + t;
    const float y0 = y - static_cast<float>(j) + t;
    const int i1 = (x0 > y0) ? 1 : 0;
    const int j1 = (x0 > y0) ? 0 : 1;
    const float x1 = x0 - static_cast<float>(i1) + kG2;
    const float y1 = y0 - static_cast<float>(j1) + kG2;
    const float x2 = x0 - 1.0f + 2.0f * kG2;
    const float y2 = y0 - 1.0f + 2.0f * kG2;

    // Gradient index for a skewed-lattice corner: hash through the
    // permutation table and select one of the 12 gradients.
    auto gradient = [this](unsigned gi, unsigned gj) -> unsigned {
        const unsigned a = perm_[(gi & 255u)];
        return perm_[(a + (gj & 255u)) & 255u] % 12u;
    };
    const unsigned ii = static_cast<unsigned>(i) & 255u;
    const unsigned jj = static_cast<unsigned>(j) & 255u;
    const unsigned gi0 = gradient(ii, jj);
    const unsigned gi1 = gradient(ii + static_cast<unsigned>(i1),
                                  jj + static_cast<unsigned>(j1));
    const unsigned gi2 = gradient(ii + 1u, jj + 1u);

    // Radial falloff (0.5 - d^2)^4 times the gradient dot product.
    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;
    float t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 > 0.0f) {
        t0 *= t0;
        n0 = t0 * t0 * (kGrad2[gi0][0] * x0 + kGrad2[gi0][1] * y0);
    }
    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 > 0.0f) {
        t1 *= t1;
        n1 = t1 * t1 * (kGrad2[gi1][0] * x1 + kGrad2[gi1][1] * y1);
    }
    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 > 0.0f) {
        t2 *= t2;
        n2 = t2 * t2 * (kGrad2[gi2][0] * x2 + kGrad2[gi2][1] * y2);
    }

    return 70.0f * (n0 + n1 + n2);
}

ValueNoise2D::ValueNoise2D(std::uint32_t seed) : seed_(seed) {
    buildPermutation(perm_, seed_);
}

float ValueNoise2D::at(float x, float y) const {
    const int xi = floorToInt(x);
    const int yi = floorToInt(y);
    const float fx = x - static_cast<float>(xi);
    const float fy = y - static_cast<float>(yi);
    const float u = quintic(fx);
    const float v = quintic(fy);

    auto lattice = [this](int lx, int ly) -> float {
        const unsigned a = perm_[static_cast<unsigned>(lx) & 255u];
        const unsigned b = perm_[(a + static_cast<unsigned>(ly)) & 255u];
        return static_cast<float>(b) / 255.0f;
    };

    const float a = lattice(xi, yi);
    const float b = lattice(xi + 1, yi);
    const float c = lattice(xi, yi + 1);
    const float d = lattice(xi + 1, yi + 1);
    const float top = a + (b - a) * u;
    const float bottom = c + (d - c) * u;
    return top + (bottom - top) * v;
}

std::uint32_t hash2(std::int32_t x, std::int32_t z, std::uint32_t seed) {
    std::uint32_t h =
        static_cast<std::uint32_t>(x) * 0x4c957f2du +
        static_cast<std::uint32_t>(z) * 0x5851f42du + seed;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

float hash01(std::int32_t x, std::int32_t z, std::uint32_t seed) {
    return static_cast<float>(hash2(x, z, seed)) / 4294967296.0f;
}

float fbm2D(const GradientNoise2D& noise, float x, float y, int octaves,
            float lacunarity, float gain) {
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * noise.at(x * freq, y * freq);
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

}  // namespace voxel::world::noise
