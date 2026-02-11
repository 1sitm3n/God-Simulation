#pragma once

#include "Window.h"
#include "Camera.h"
#include "Shader.h"
#include "SphereMesh.h"
#include "layers/planetary/PlanetData.h"
#include "core/util/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

namespace godsim {

// ═══════════════════════════════════════════════════════════════
//  EMBEDDED SHADERS
// ═══════════════════════════════════════════════════════════════

inline const char* PLANET_VERT_SRC = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColour;
layout(location = 3) in vec2 aUV;

out vec3 vNormal;
out vec3 vColour;
out vec3 vWorldPos;
out vec2 vUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * aNormal;
    vColour = aColour;
    vUV = aUV;
    gl_Position = uProjection * uView * worldPos;
}
)glsl";

inline const char* PLANET_FRAG_SRC = R"glsl(
#version 330 core

in vec3 vNormal;
in vec3 vColour;
in vec3 vWorldPos;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform float uAmbient;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);

    // Diffuse lighting
    float diff = max(dot(N, L), 0.0);

    // Subtle specular for water regions (blue-ish colours)
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    // Only apply specular to darker (ocean) areas
    float specStrength = step(0.35, 1.0 - max(max(vColour.r, vColour.g), vColour.b)) * 0.3;

    // Atmospheric rim lighting
    float rim = 1.0 - max(dot(N, V), 0.0);
    rim = pow(rim, 3.0) * 0.4;
    vec3 rimColour = vec3(0.4, 0.6, 1.0);

    // Combine
    vec3 ambient = uAmbient * vColour;
    vec3 diffuse = diff * vColour;
    vec3 specular = spec * specStrength * vec3(1.0);
    vec3 atmosphere = rim * rimColour;

    vec3 colour = ambient + diffuse + specular + atmosphere;

    // Subtle tone mapping
    colour = colour / (colour + vec3(1.0));

    FragColor = vec4(colour, 1.0);
}
)glsl";

// ═══════════════════════════════════════════════════════════════
//  PLANET RENDERER
// ═══════════════════════════════════════════════════════════════

class PlanetRenderer {
public:
    /// Initialise the renderer with planet data.
    void init(const PlanetData& planet) {
        LOG_INFO("Initialising planet renderer...");

        window_ = std::make_unique<Window>(1280, 720, "God Simulation — " + planet.name);

        // OpenGL state
        gl::Enable(GL_DEPTH_TEST);
        gl::DepthFunc(GL_LESS);
        gl::Enable(GL_CULL_FACE);
        gl::CullFace(GL_BACK);
        gl::FrontFace(GL_CCW);
        gl::Enable(GL_MULTISAMPLE);
        gl::ClearColor(0.02f, 0.02f, 0.06f, 1.0f); // Dark space

        // Compile shaders
        shader_.compile(PLANET_VERT_SRC, PLANET_FRAG_SRC);

        // Create sphere mesh from planet data
        sphere_.create(planet);

        camera_.set_distance(3.5f);

        LOG_INFO("Planet renderer ready");
        LOG_INFO("  Controls:");
        LOG_INFO("    Left-drag  : Orbit camera");
        LOG_INFO("    Scroll     : Zoom in/out");
        LOG_INFO("    R          : Reset camera");
        LOG_INFO("    G          : Toggle wireframe");
        LOG_INFO("    ESC        : Close");
    }

    /// Run the render loop. Blocks until window is closed.
    void run() {
        bool wireframe = false;
        bool g_was_pressed = false;

        while (!window_->should_close()) {
            const auto& input = window_->poll();

            if (input.key_escape) break;

            // Toggle wireframe on G press (not hold)
            if (input.key_g && !g_was_pressed) {
                wireframe = !wireframe;
                gl::PolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
            }
            g_was_pressed = input.key_g;

            // Update camera
            if (input.resized) {
                gl::Viewport(0, 0, input.width, input.height);
            }
            camera_.set_aspect(static_cast<float>(input.width) /
                               std::max(static_cast<float>(input.height), 1.0f));
            camera_.update(input, 1.0f / 60.0f);

            // ─── Render ───
            gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader_.use();

            // Matrices
            glm::mat4 model(1.0f);
            shader_.set_mat4("uModel", glm::value_ptr(model));
            shader_.set_mat4("uView", glm::value_ptr(camera_.view()));
            shader_.set_mat4("uProjection", glm::value_ptr(camera_.projection()));

            // Lighting — sun direction (slightly off-axis for interesting shadows)
            shader_.set_vec3("uLightDir", 0.6f, 0.8f, 0.4f);
            shader_.set_vec3("uCameraPos",
                              camera_.position().x,
                              camera_.position().y,
                              camera_.position().z);
            shader_.set_float("uAmbient", 0.15f);

            sphere_.draw();

            window_->swap_buffers();

            // Slow rotation when not interacting
            if (!input.left_mouse_down) {
                // The planet slowly rotates — feels alive
                rotation_angle_ += 0.001f;
            }
        }
    }

private:
    std::unique_ptr<Window> window_;
    Camera camera_;
    ShaderProgram shader_;
    SphereMesh sphere_;
    float rotation_angle_ = 0.0f;
};

} // namespace godsim
