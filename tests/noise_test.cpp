// Noise-library contract tests: determinism, ranges, and basic coherence of
// the gradient/value noise primitives, the per-column hash, and fBm.

#include "test_harness.h"

#include "world/noise.h"

using namespace voxel::world::noise;

namespace {
constexpr float kEps = 1e-6f;

constexpr float kSamplePoints[][2] = {
    {0.5f, 0.5f},    {1.5f, 2.5f},    {-3.0f, 7.25f}, {10.0f, -10.0f},
    {-12.5f, -0.25f}, {4.75f, -3.0f}, {0.125f, 63.0f}, {-64.0f, 64.0f},
};
}  // namespace

TEST(gradient_noise_deterministic_same_seed) {
    const GradientNoise2D a(123u);
    const GradientNoise2D b(123u);
    for (const auto& p : kSamplePoints) {
        CHECK_NEAR(a.at(p[0], p[1]), b.at(p[0], p[1]), 0.0);
    }
}

TEST(gradient_noise_seed_changes_output) {
    const GradientNoise2D a(1u);
    const GradientNoise2D b(2u);
    bool anyDiffer = false;
    for (const auto& p : kSamplePoints) {
        if (a.at(p[0], p[1]) != b.at(p[0], p[1])) {
            anyDiffer = true;
            break;
        }
    }
    CHECK(anyDiffer);
}

TEST(gradient_noise_zero_at_origin) {
    const GradientNoise2D g(7u);
    CHECK_NEAR(g.at(0.0f, 0.0f), 0.0f, kEps);
}

TEST(gradient_noise_range) {
    const GradientNoise2D g(42u);
    for (float x = -8.0f; x <= 8.0f; x += 0.5f) {
        for (float y = -8.0f; y <= 8.0f; y += 0.5f) {
            const float v = g.at(x, y);
            CHECK(v >= -1.0f && v <= 1.0f);
        }
    }
}

TEST(value_noise_range_and_determinism) {
    const ValueNoise2D a(99u);
    const ValueNoise2D b(99u);
    for (float x = -4.0f; x <= 4.0f; x += 0.25f) {
        for (float y = -4.0f; y <= 4.0f; y += 0.25f) {
            const float v = a.at(x, y);
            CHECK(v >= 0.0f && v <= 1.0f);
            CHECK_NEAR(v, b.at(x, y), 0.0);
        }
    }
}

TEST(value_noise_matches_lattice_at_integers) {
    const ValueNoise2D v(5u);
    // Quintic interpolation gives zero derivative at lattice corners, so the
    // value approaches the corner lattice value continuously.
    CHECK_NEAR(v.at(3.999f, 4.0f), v.at(4.0f, 4.0f), 1e-3);
    CHECK_NEAR(v.at(3.0f, 3.999f), v.at(3.0f, 4.0f), 1e-3);
}

TEST(hash2_deterministic_and_distinct) {
    const std::uint32_t h1 = hash2(3, 5, 11u);
    CHECK_EQ(hash2(3, 5, 11u), h1);
    CHECK(hash2(4, 5, 11u) != h1);
    CHECK(hash2(3, 6, 11u) != h1);
    CHECK(hash2(3, 5, 12u) != h1);
    CHECK(hash2(-3, -5, 11u) != h1);
}

TEST(hash01_range) {
    for (int x = -16; x <= 16; ++x) {
        for (int z = -16; z <= 16; ++z) {
            const float v = hash01(x, z, 7u);
            CHECK(v >= 0.0f && v < 1.0f);
        }
    }
}

TEST(fbm_deterministic_and_range) {
    const GradientNoise2D n(2024u);
    for (const auto& p : kSamplePoints) {
        const float v = fbm2D(n, p[0], p[1], 4);
        CHECK(v >= -1.0f && v <= 1.0f);
        CHECK_NEAR(v, fbm2D(GradientNoise2D(2024u), p[0], p[1], 4), 0.0);
    }
}

TEST(fbm_single_octave_equals_gradient) {
    const GradientNoise2D n(1u);
    CHECK_NEAR(fbm2D(n, 1.25f, -2.5f, 1), n.at(1.25f, -2.5f), 1e-5);
}

// Golden regression values (seed 123). These pin the exact float results so
// any change to the algorithm, constants, or float behavior is caught here.
// If constants are deliberately re-tuned, recapture and update these.
TEST(noise_golden_values) {
    const GradientNoise2D g(123u);
    const ValueNoise2D v(123u);
    CHECK_NEAR(g.at(0.5f, 0.5f), -0.307156503f, 1e-6);
    CHECK_NEAR(g.at(1.5f, 2.5f), -0.0900539681f, 1e-6);
    CHECK_NEAR(g.at(-3.0f, 7.25f), 0.0444343425f, 1e-6);
    CHECK_NEAR(v.at(1.25f, 2.5f), 0.496292919f, 1e-6);
    CHECK_NEAR(fbm2D(g, 1.25f, -2.5f, 4), -0.243861198f, 1e-6);
    CHECK_EQ(hash2(3, 5, 11u), 62352578u);
}
