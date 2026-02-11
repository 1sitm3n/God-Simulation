#pragma once

#include "GL33Loader.h"
#include "layers/planetary/PlanetData.h"
#include "layers/planetary/Biome.h"
#include "core/util/Types.h"

#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace godsim {

/// Vertex: position (3f) + normal (3f) + UV (2f) = 8 floats
struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

/// High-resolution UV sphere. Biome colours and elevation applied via GPU textures.
class SphereMesh {
public:
    /// Create sphere and upload biome/elevation/normal textures to GPU.
    void create(const PlanetData& planet, int stacks = 200, int sectors = 400) {
        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        vertices.reserve((stacks + 1) * (sectors + 1));
        indices.reserve(stacks * sectors * 6);

        for (int i = 0; i <= stacks; i++) {
            float stack_angle = static_cast<float>(M_PI) / 2.0f -
                                static_cast<float>(i) * static_cast<float>(M_PI) / stacks;
            float xy = std::cos(stack_angle);
            float z  = std::sin(stack_angle);

            for (int j = 0; j <= sectors; j++) {
                float sector_angle = 2.0f * static_cast<float>(M_PI) * j / sectors;

                Vertex vert{};
                vert.px = xy * std::cos(sector_angle);
                vert.py = z;
                vert.pz = xy * std::sin(sector_angle);
                vert.nx = vert.px;
                vert.ny = vert.py;
                vert.nz = vert.pz;
                vert.u = static_cast<float>(j) / sectors;
                vert.v = static_cast<float>(i) / stacks;
                vertices.push_back(vert);
            }
        }

        for (int i = 0; i < stacks; i++) {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;
            for (int j = 0; j < sectors; j++, k1++, k2++) {
                if (i != 0) {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }
                if (i != (stacks - 1)) {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }

        index_count_ = static_cast<u32>(indices.size());

        gl::GenVertexArrays(1, &vao_);
        gl::GenBuffers(1, &vbo_);
        gl::GenBuffers(1, &ebo_);

        gl::BindVertexArray(vao_);
        gl::BindBuffer(GL_ARRAY_BUFFER, vbo_);
        gl::BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                       vertices.data(), GL_STATIC_DRAW);
        gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32),
                       indices.data(), GL_STATIC_DRAW);

        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, px));
        gl::EnableVertexAttribArray(0);
        gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
        gl::EnableVertexAttribArray(1);
        gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
        gl::EnableVertexAttribArray(2);
        gl::BindVertexArray(0);

        create_biome_texture(planet);
        create_elevation_texture(planet);
        create_normal_texture(planet);
    }

    void bind_textures() const {
        gl::ActiveTexture(GL_TEXTURE0);
        gl::BindTexture(GL_TEXTURE_2D, biome_tex_);
        gl::ActiveTexture(GL_TEXTURE0 + 1);
        gl::BindTexture(GL_TEXTURE_2D, elevation_tex_);
        gl::ActiveTexture(GL_TEXTURE0 + 2);
        gl::BindTexture(GL_TEXTURE_2D, normal_tex_);
    }

    void draw() const {
        gl::BindVertexArray(vao_);
        gl::DrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, nullptr);
        gl::BindVertexArray(0);
    }

    ~SphereMesh() {
        if (vao_) gl::DeleteVertexArrays(1, &vao_);
        if (vbo_) gl::DeleteBuffers(1, &vbo_);
        if (ebo_) gl::DeleteBuffers(1, &ebo_);
        GLuint textures[] = {biome_tex_, elevation_tex_, normal_tex_};
        if (biome_tex_) gl::DeleteTextures(3, textures);
    }

    SphereMesh() = default;
    SphereMesh(const SphereMesh&) = delete;
    SphereMesh& operator=(const SphereMesh&) = delete;

private:
    void create_biome_texture(const PlanetData& planet) {
        u32 w = planet.width, h = planet.height;
        std::vector<u8> pixels(w * h * 3);

        for (u32 y = 0; y < h; y++) {
            for (u32 x = 0; x < w; x++) {
                BiomeType biome = planet.biome_at(x, y);
                const auto& info = BIOME_INFO[static_cast<size_t>(biome)];
                size_t idx = (y * w + x) * 3;
                float elev = planet.elevation.get(x, y);
                float shade = 0.82f + 0.18f * elev;
                pixels[idx + 0] = static_cast<u8>(std::min(255.0f, info.r * shade));
                pixels[idx + 1] = static_cast<u8>(std::min(255.0f, info.g * shade));
                pixels[idx + 2] = static_cast<u8>(std::min(255.0f, info.b * shade));
            }
        }

        gl::GenTextures(1, &biome_tex_);
        gl::BindTexture(GL_TEXTURE_2D, biome_tex_);
        gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void create_elevation_texture(const PlanetData& planet) {
        u32 w = planet.width, h = planet.height;
        std::vector<u8> rgb(w * h * 3);
        for (u32 y = 0; y < h; y++) {
            for (u32 x = 0; x < w; x++) {
                u8 v = static_cast<u8>(std::clamp(planet.elevation.get(x, y), 0.0f, 1.0f) * 255.0f);
                size_t idx = (y * w + x) * 3;
                rgb[idx] = rgb[idx + 1] = rgb[idx + 2] = v;
            }
        }
        gl::GenTextures(1, &elevation_tex_);
        gl::BindTexture(GL_TEXTURE_2D, elevation_tex_);
        gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void create_normal_texture(const PlanetData& planet) {
        u32 w = planet.width, h = planet.height;
        std::vector<u8> pixels(w * h * 3);
        float strength = 4.0f;

        for (u32 y = 0; y < h; y++) {
            for (u32 x = 0; x < w; x++) {
                u32 xl = (x > 0) ? x - 1 : x;
                u32 xr = (x < w - 1) ? x + 1 : x;
                u32 yu = (y > 0) ? y - 1 : y;
                u32 yd = (y < h - 1) ? y + 1 : y;

                float dx = (planet.elevation.get(xr, y) - planet.elevation.get(xl, y)) * strength;
                float dy = (planet.elevation.get(x, yd) - planet.elevation.get(x, yu)) * strength;
                float len = std::sqrt(dx * dx + dy * dy + 1.0f);

                size_t idx = (y * w + x) * 3;
                pixels[idx + 0] = static_cast<u8>((-dx / len * 0.5f + 0.5f) * 255.0f);
                pixels[idx + 1] = static_cast<u8>((-dy / len * 0.5f + 0.5f) * 255.0f);
                pixels[idx + 2] = static_cast<u8>((1.0f / len * 0.5f + 0.5f) * 255.0f);
            }
        }

        gl::GenTextures(1, &normal_tex_);
        gl::BindTexture(GL_TEXTURE_2D, normal_tex_);
        gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLuint biome_tex_ = 0, elevation_tex_ = 0, normal_tex_ = 0;
    u32 index_count_ = 0;
};

} // namespace godsim
