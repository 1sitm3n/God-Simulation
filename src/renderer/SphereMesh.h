#pragma once

#include "GL33Loader.h"
#include "layers/planetary/PlanetData.h"
#include "layers/planetary/Biome.h"
#include "core/util/Types.h"

#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace godsim {

/// Vertex: position (3f) + normal (3f) + colour (3f) + UV (2f) = 11 floats
struct Vertex {
    float px, py, pz;  // Position
    float nx, ny, nz;  // Normal
    float r, g, b;     // Colour (from biome)
    float u, v;        // UV coordinates
};

/// Generates a UV sphere and uploads it to OpenGL.
class SphereMesh {
public:
    /// Create sphere mesh coloured by planet biome data.
    void create(const PlanetData& planet, int stacks = 128, int sectors = 256) {
        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        vertices.reserve((stacks + 1) * (sectors + 1));
        indices.reserve(stacks * sectors * 6);

        float radius = 1.0f;

        // ─── Generate Vertices ───
        for (int i = 0; i <= stacks; i++) {
            float stack_angle = static_cast<float>(M_PI) / 2.0f -
                                static_cast<float>(i) * static_cast<float>(M_PI) / stacks;
            float xy = radius * std::cos(stack_angle);
            float z  = radius * std::sin(stack_angle);

            for (int j = 0; j <= sectors; j++) {
                float sector_angle = 2.0f * static_cast<float>(M_PI) * j / sectors;

                Vertex vert{};

                // Position
                vert.px = xy * std::cos(sector_angle);
                vert.py = z;
                vert.pz = xy * std::sin(sector_angle);

                // Normal (sphere = normalised position)
                vert.nx = vert.px;
                vert.ny = vert.py;
                vert.nz = vert.pz;

                // UV
                vert.u = static_cast<float>(j) / sectors;
                vert.v = static_cast<float>(i) / stacks;

                // Sample biome colour from planet data
                if (planet.biome_map.size() > 0) {
                    // Map UV to planet grid
                    float fx = vert.u * (planet.width - 1);
                    float fy = vert.v * (planet.height - 1);

                    u32 px = std::min(static_cast<u32>(fx), planet.width - 1);
                    u32 py = std::min(static_cast<u32>(fy), planet.height - 1);

                    BiomeType biome = planet.biome_at(px, py);
                    const auto& info = BIOME_INFO[static_cast<size_t>(biome)];

                    vert.r = info.r / 255.0f;
                    vert.g = info.g / 255.0f;
                    vert.b = info.b / 255.0f;

                    // Subtle elevation shading
                    float elev = planet.elevation.get(px, py);
                    float shade = 0.85f + 0.15f * elev;
                    vert.r *= shade;
                    vert.g *= shade;
                    vert.b *= shade;
                } else {
                    vert.r = 0.3f; vert.g = 0.5f; vert.b = 0.8f; // Default blue
                }

                vertices.push_back(vert);
            }
        }

        // ─── Generate Indices ───
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

        // ─── Upload to GPU ───
        gl::GenVertexArrays(1, &vao_);
        gl::GenBuffers(1, &vbo_);
        gl::GenBuffers(1, &ebo_);

        gl::BindVertexArray(vao_);

        gl::BindBuffer(GL_ARRAY_BUFFER, vbo_);
        gl::BufferData(GL_ARRAY_BUFFER,
                       vertices.size() * sizeof(Vertex),
                       vertices.data(), GL_STATIC_DRAW);

        gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        gl::BufferData(GL_ELEMENT_ARRAY_BUFFER,
                       indices.size() * sizeof(u32),
                       indices.data(), GL_STATIC_DRAW);

        // Position: location 0
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                 (void*)offsetof(Vertex, px));
        gl::EnableVertexAttribArray(0);

        // Normal: location 1
        gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                 (void*)offsetof(Vertex, nx));
        gl::EnableVertexAttribArray(1);

        // Colour: location 2
        gl::VertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                 (void*)offsetof(Vertex, r));
        gl::EnableVertexAttribArray(2);

        // UV: location 3
        gl::VertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                 (void*)offsetof(Vertex, u));
        gl::EnableVertexAttribArray(3);

        gl::BindVertexArray(0);
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
    }

    SphereMesh() = default;
    SphereMesh(const SphereMesh&) = delete;
    SphereMesh& operator=(const SphereMesh&) = delete;

private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    u32 index_count_ = 0;
};

} // namespace godsim
