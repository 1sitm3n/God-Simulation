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
    u32 next_u32() { ++advance_count_; return engine_(); }

    /// Two draws, explicitly sequenced.
    /// `(u64(engine_()) << 32) | engine_()` looks fine and isn't: C++ does not
    /// sequence the operands of `|`, so which draw lands in the high half is
    /// unspecified and two conforming compilers may disagree. Not UB, but not
    /// portable either - and these draws seed the terrain noise, so a swap
    /// gives a different world.
    u64 next_u64() {
        const u64 hi = next_u32();
        const u64 lo = next_u32();
        return (hi << 32) | lo;
    }

    /// Returns a float in [0.0, 1.0)
    f32 next_float() {
        return static_cast<f32>(next_u32() >> 8) / 16777216.0f; // 2^24
    }

    /// Returns a float in [min, max)
    f32 next_float(f32 min, f32 max) {
        return min + next_float() * (max - min);
    }

    /// Returns an int in [min, max] (inclusive).
    /// Hand-rolled rather than std::uniform_int_distribution, whose algorithm is
    /// unspecified: libstdc++ and libc++ may consume a different number of
    /// engine outputs for the same call, which desynchronises every draw after
    /// it. Rejection sampling from the top removes modulo bias and consumes a
    /// well-defined number of draws.
    i32 next_int(i32 min, i32 max) {
        if (max <= min) return min;
        const u64 range = static_cast<u64>(max) - static_cast<u64>(min) + 1ull;
        if (range >= 0x100000000ull) return min + static_cast<i32>(next_u32());
        const u32 limit = static_cast<u32>(0x100000000ull - (0x100000000ull % range));
        u32 v;
        do { v = next_u32(); } while (v >= limit);
        return min + static_cast<i32>(v % range);
    }

    /// Returns a gaussian-distributed double.
    f64 next_gaussian(f64 mean = 0.0, f64 stddev = 1.0) {
        // Box-Muller transform (deterministic, no caching)
        f64 u1 = static_cast<f64>(next_u32()) / static_cast<f64>(pcg32::max());
        f64 u2 = static_cast<f64>(next_u32()) / static_cast<f64>(pcg32::max());
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

    /// Seed plus how many draws have been taken from it. Together those pin the
    /// stream position exactly.
    ///
    /// This used to be broken in a quiet way: advance_count_ was incremented in
    /// exactly one place, inside restore()'s own replay loop, and never by any
    /// generation call. So state() always returned {seed, 0} and restore()
    /// simply rewound to the seed. Nothing called either function and no test
    /// covered them, so it stayed broken while the header claimed snapshots
    /// worked.
    std::pair<u64, u64> state() const {
        return {initial_seed_, advance_count_};
    }

    /// Restore to an exact stream position.
    /// pcg32::advance is O(log n) jump-ahead, which is one of the main reasons
    /// to pick PCG in the first place - the old replay loop was O(n) and would
    /// have taken longer than the simulation it was restoring.
    void restore(u64 seed, u64 advances) {
        initial_seed_ = seed;
        engine_ = pcg32(seed);
        engine_.advance(advances);
        advance_count_ = advances;
    }

private:
    pcg32 engine_;
    u64 initial_seed_;
    u64 advance_count_ = 0;
};

} // namespace godsim
