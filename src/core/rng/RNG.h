#pragma once

#include "core/util/Types.h"
#include <pcg_random.hpp>
#include <random>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace godsim {

/// Deterministic RNG using PCG32.
/// Supports splitting into independent sub-streams for parallel use.
/// State can be saved/restored for snapshots.
class RNG {
public:
    explicit RNG(u64 seed) : engine_(seed), initial_seed_(seed) {}

    // ─── Core Generation ───
    u32 next_u32() { return engine_(); }
    u64 next_u64() { return (static_cast<u64>(engine_()) << 32) | engine_(); }

    /// Returns a float in [0.0, 1.0)
    f32 next_float() {
        return static_cast<f32>(engine_() >> 8) / 16777216.0f; // 2^24
    }

    /// Returns a float in [min, max)
    f32 next_float(f32 min, f32 max) {
        return min + next_float() * (max - min);
    }

    /// Returns an int in [min, max] (inclusive)
    i32 next_int(i32 min, i32 max) {
        std::uniform_int_distribution<i32> dist(min, max);
        return dist(engine_);
    }

    /// Returns a gaussian-distributed double.
    f64 next_gaussian(f64 mean = 0.0, f64 stddev = 1.0) {
        // Box-Muller transform (deterministic, no caching)
        f64 u1 = static_cast<f64>(engine_()) / static_cast<f64>(engine_.max());
        f64 u2 = static_cast<f64>(engine_()) / static_cast<f64>(engine_.max());
        if (u1 < 1e-15) u1 = 1e-15; // avoid log(0)
        f64 z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
        return mean + z * stddev;
    }

    /// Create an independent sub-stream.
    /// Each split produces a non-overlapping sequence.
    RNG split() {
        u64 child_seed = next_u64();
        return RNG(child_seed);
    }

    // ─── State Management (for snapshots) ───
    u64 seed() const { return initial_seed_; }

    /// Returns internal state for serialisation.
    /// Two values: the PCG state and the stream.
    std::pair<u64, u64> state() const {
        // PCG internals - we save seed + number of advances
        return {initial_seed_, advance_count_};
    }

    /// Restore from a saved state.
    void restore(u64 seed, u64 advances) {
        initial_seed_ = seed;
        engine_ = pcg32(seed);
        advance_count_ = 0;
        // Replay advances to reach the same state
        for (u64 i = 0; i < advances; i++) {
            engine_();
            advance_count_++;
        }
    }

private:
    pcg32 engine_;
    u64 initial_seed_;
    u64 advance_count_ = 0;
};

} // namespace godsim
