#pragma once

#include "core/util/Types.h"
#include <cmath>
#include <array>
#include <algorithm>
#include <numeric>

namespace godsim {

/// Perlin noise generator with octave (fBm) support.
/// Deterministic — same seed produces same noise field.
class PerlinNoise {
public:
    explicit PerlinNoise(u64 seed = 0) {
        // Fill permutation table
        for (int i = 0; i < 256; i++) perm_[i] = i;

        // Fisher-Yates shuffle with simple LCG
        u64 state = seed;
        for (int i = 255; i > 0; i--) {
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            int j = static_cast<int>((state >> 33) % (i + 1));
            std::swap(perm_[i], perm_[j]);
        }

        // Duplicate for wrap-around
        for (int i = 0; i < 256; i++) perm_[256 + i] = perm_[i];
    }

    /// Single octave Perlin noise at (x, y). Returns [-1, 1].
    f64 noise(f64 x, f64 y) const {
        // Grid cell coordinates
        int xi = static_cast<int>(std::floor(x)) & 255;
        int yi = static_cast<int>(std::floor(y)) & 255;

        // Fractional position within cell
        f64 xf = x - std::floor(x);
        f64 yf = y - std::floor(y);

        // Fade curves
        f64 u = fade(xf);
        f64 v = fade(yf);

        // Hash corners
        int aa = perm_[perm_[xi] + yi];
        int ab = perm_[perm_[xi] + yi + 1];
        int ba = perm_[perm_[xi + 1] + yi];
        int bb = perm_[perm_[xi + 1] + yi + 1];

        // Gradient dot products and interpolation
        f64 x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1, yf), u);
        f64 x2 = lerp(grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1), u);

        return lerp(x1, x2, v);
    }

    /// Fractal Brownian Motion — layered octaves of noise.
    /// Returns approximately [-1, 1] (can slightly exceed).
    ///   octaves:     number of noise layers (4-8 typical)
    ///   frequency:   base sampling frequency (lower = larger features)
    ///   persistence: amplitude decay per octave (0.5 typical)
    ///   lacunarity:  frequency increase per octave (2.0 typical)
    f64 fbm(f64 x, f64 y, int octaves = 6, f64 frequency = 1.0,
            f64 persistence = 0.5, f64 lacunarity = 2.0) const {
        f64 total = 0.0;
        f64 amplitude = 1.0;
        f64 max_amplitude = 0.0;
        f64 freq = frequency;

        for (int i = 0; i < octaves; i++) {
            total += noise(x * freq, y * freq) * amplitude;
            max_amplitude += amplitude;
            amplitude *= persistence;
            freq *= lacunarity;
        }

        return total / max_amplitude; // Normalise to [-1, 1]
    }

    /// Ridged noise — creates mountain ridges and sharp features.
    f64 ridged(f64 x, f64 y, int octaves = 6, f64 frequency = 1.0,
               f64 persistence = 0.5, f64 lacunarity = 2.0) const {
        f64 total = 0.0;
        f64 amplitude = 1.0;
        f64 max_amplitude = 0.0;
        f64 freq = frequency;

        for (int i = 0; i < octaves; i++) {
            f64 n = 1.0 - std::abs(noise(x * freq, y * freq));
            n = n * n; // Sharpen ridges
            total += n * amplitude;
            max_amplitude += amplitude;
            amplitude *= persistence;
            freq *= lacunarity;
        }

        return total / max_amplitude;
    }

private:
    static f64 fade(f64 t) {
        return t * t * t * (t * (t * 6 - 15) + 10); // 6t^5 - 15t^4 + 10t^3
    }

    static f64 lerp(f64 a, f64 b, f64 t) {
        return a + t * (b - a);
    }

    static f64 grad(int hash, f64 x, f64 y) {
        switch (hash & 3) {
            case 0: return  x + y;
            case 1: return -x + y;
            case 2: return  x - y;
            case 3: return -x - y;
            default: return 0;
        }
    }

    std::array<int, 512> perm_;
};

} // namespace godsim
