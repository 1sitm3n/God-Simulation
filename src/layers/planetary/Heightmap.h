#pragma once

#include "core/util/Types.h"
#include "core/serialise/BinaryStream.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace godsim {

/// A 2D grid of floating-point values. Used for elevation, temperature,
/// moisture, and other spatial data across the planet surface.
class Heightmap {
public:
    Heightmap() : width_(0), height_(0) {}
    Heightmap(u32 width, u32 height, f32 fill = 0.0f)
        : width_(width), height_(height), data_(width * height, fill) {}

    // ─── Access ───
    f32 get(u32 x, u32 y) const {
        return data_[index(x, y)];
    }

    void set(u32 x, u32 y, f32 value) {
        data_[index(x, y)] = value;
    }

    f32& at(u32 x, u32 y) {
        return data_[index(x, y)];
    }

    const f32& at(u32 x, u32 y) const {
        return data_[index(x, y)];
    }

    /// Bilinear interpolation at fractional coordinates [0, width) x [0, height).
    f32 sample(f32 fx, f32 fy) const {
        fx = std::clamp(fx, 0.0f, static_cast<f32>(width_ - 1));
        fy = std::clamp(fy, 0.0f, static_cast<f32>(height_ - 1));

        u32 x0 = static_cast<u32>(fx);
        u32 y0 = static_cast<u32>(fy);
        u32 x1 = std::min(x0 + 1, width_ - 1);
        u32 y1 = std::min(y0 + 1, height_ - 1);

        f32 tx = fx - x0;
        f32 ty = fy - y0;

        f32 a = get(x0, y0) * (1 - tx) + get(x1, y0) * tx;
        f32 b = get(x0, y1) * (1 - tx) + get(x1, y1) * tx;

        return a * (1 - ty) + b * ty;
    }

    /// Sample using normalised UV coordinates [0, 1].
    f32 sample_uv(f32 u, f32 v) const {
        return sample(u * (width_ - 1), v * (height_ - 1));
    }

    // ─── Bulk Operations ───

    void fill(f32 value) {
        std::fill(data_.begin(), data_.end(), value);
    }

    /// Add another heightmap's values (must be same size).
    void add(const Heightmap& other, f32 scale = 1.0f) {
        for (size_t i = 0; i < data_.size(); i++) {
            data_[i] += other.data_[i] * scale;
        }
    }

    /// Multiply all values by a scalar.
    void multiply(f32 scale) {
        for (auto& v : data_) v *= scale;
    }

    /// Clamp all values to [lo, hi].
    void clamp(f32 lo, f32 hi) {
        for (auto& v : data_) v = std::clamp(v, lo, hi);
    }

    /// Normalise values to [0, 1].
    void normalise() {
        auto [min_it, max_it] = std::minmax_element(data_.begin(), data_.end());
        f32 lo = *min_it;
        f32 hi = *max_it;
        if (hi - lo < 1e-8f) return;
        f32 range = hi - lo;
        for (auto& v : data_) v = (v - lo) / range;
    }

    /// Apply Gaussian blur (simple box approximation).
    void blur(int radius = 1) {
        Heightmap temp(width_, height_);
        int diam = 2 * radius + 1;
        f32 inv = 1.0f / (diam * diam);

        for (u32 y = 0; y < height_; y++) {
            for (u32 x = 0; x < width_; x++) {
                f32 sum = 0;
                for (int dy = -radius; dy <= radius; dy++) {
                    for (int dx = -radius; dx <= radius; dx++) {
                        u32 sx = std::clamp(static_cast<i32>(x) + dx, 0, static_cast<i32>(width_ - 1));
                        u32 sy = std::clamp(static_cast<i32>(y) + dy, 0, static_cast<i32>(height_ - 1));
                        sum += get(sx, sy);
                    }
                }
                temp.set(x, y, sum * inv);
            }
        }
        data_ = std::move(temp.data_);
    }

    // ─── Statistics ───
    f32 min_value() const { return *std::min_element(data_.begin(), data_.end()); }
    f32 max_value() const { return *std::max_element(data_.begin(), data_.end()); }
    f32 average() const {
        f64 sum = 0;
        for (auto v : data_) sum += v;
        return static_cast<f32>(sum / data_.size());
    }

    // ─── Dimensions ───
    u32 width()  const { return width_; }
    u32 height() const { return height_; }
    size_t size() const { return data_.size(); }
    const f32* data_ptr() const { return data_.data(); }
    f32* data_ptr() { return data_.data(); }

    // ─── Serialisation ───
    void serialise(BinaryWriter& writer) const {
        writer.write_u32(width_);
        writer.write_u32(height_);
        writer.write_bytes(data_.data(), data_.size() * sizeof(f32));
    }

    void deserialise(BinaryReader& reader) {
        width_ = reader.read_u32();
        height_ = reader.read_u32();
        data_.resize(width_ * height_);
        reader.read_bytes(data_.data(), data_.size() * sizeof(f32));
    }

private:
    size_t index(u32 x, u32 y) const {
        return static_cast<size_t>(y) * width_ + x;
    }

    u32 width_;
    u32 height_;
    std::vector<f32> data_;
};

} // namespace godsim
