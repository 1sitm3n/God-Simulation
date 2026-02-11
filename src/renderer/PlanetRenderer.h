#pragma once

#include "Window.h"
#include "Camera.h"
#include "Shader.h"
#include "SphereMesh.h"
#include "layers/planetary/PlanetData.h"
#include "core/util/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace godsim {

// ═══════════════════════════════════════════════════════════════
//  PLANET SURFACE SHADER
// ═══════════════════════════════════════════════════════════════

inline const char* PLANET_VERT = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
out float vElevation;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform sampler2D uElevationTex;
uniform float uDisplacementScale;
uniform float uSeaLevel;

void main() {
    float elev = texture(uElevationTex, aUV).r;
    vElevation = elev;

    // Displace vertices above sea level outward
    float displacement = 0.0;
    if (elev > uSeaLevel) {
        displacement = (elev - uSeaLevel) * uDisplacementScale;
    }

    vec3 displaced = aPos * (1.0 + displacement);

    vec4 worldPos = uModel * vec4(displaced, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uProjection * uView * worldPos;
}
)glsl";

inline const char* PLANET_FRAG = R"glsl(
#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
in float vElevation;

out vec4 FragColor;

uniform sampler2D uBiomeTex;
uniform sampler2D uNormalTex;
uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform float uSeaLevel;
uniform float uTime;

// Smooth noise for subtle variation
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCameraPos - vWorldPos);

    // ─── Normal map perturbation ───
    vec3 tangentNormal = texture(uNormalTex, vUV).xyz * 2.0 - 1.0;
    // Simple tangent space approximation for sphere
    vec3 T = normalize(cross(vec3(0.0, 1.0, 0.0), N));
    if (length(T) < 0.01) T = normalize(cross(vec3(1.0, 0.0, 0.0), N));
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    vec3 perturbedN = normalize(TBN * tangentNormal);

    // Blend between geometric normal and perturbed normal
    // Stronger perturbation on land, none on ocean
    float isLand = smoothstep(uSeaLevel - 0.01, uSeaLevel + 0.02, vElevation);
    vec3 finalN = normalize(mix(N, perturbedN, isLand * 0.7));

    // ─── Get base colour from biome texture ───
    vec3 baseColour = texture(uBiomeTex, vUV).rgb;

    // Add subtle noise variation to break up flat biome areas
    float noiseVal = noise2D(vUV * 200.0) * 0.06 - 0.03;
    baseColour += vec3(noiseVal) * isLand;

    // ─── Ocean rendering ───
    float isOcean = 1.0 - isLand;
    if (isOcean > 0.5) {
        // Depth-based ocean colour
        float depth = (uSeaLevel - vElevation) / uSeaLevel;
        vec3 shallowOcean = vec3(0.12, 0.30, 0.45);
        vec3 deepOcean    = vec3(0.04, 0.10, 0.22);
        baseColour = mix(shallowOcean, deepOcean, smoothstep(0.0, 0.6, depth));

        // Subtle animated caustic-like shimmer
        float shimmer = noise2D(vUV * 80.0 + vec2(uTime * 0.01, uTime * 0.007)) * 0.03;
        baseColour += vec3(shimmer * 0.5, shimmer * 0.7, shimmer);
    }

    // ─── Lighting ───
    // Soft diffuse with wrapped lighting for softer terminator
    float NdotL = dot(finalN, L);
    float diffuse = max(0.0, NdotL * 0.6 + 0.4) * 0.7; // Wrapped
    float hardDiffuse = max(0.0, NdotL);

    // Specular — strong on ocean, subtle on land
    vec3 H = normalize(L + V);
    float NdotH = max(dot(finalN, H), 0.0);
    float specOcean = pow(NdotH, 64.0) * 0.8 * isOcean * hardDiffuse;
    float specLand = pow(NdotH, 16.0) * 0.05 * isLand * hardDiffuse;
    float spec = specOcean + specLand;

    // Fresnel for ocean reflection boost
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 4.0);
    float oceanFresnel = fresnel * 0.25 * isOcean * hardDiffuse;

    // Ambient — slightly blue-tinted from sky
    vec3 ambientColour = vec3(0.08, 0.09, 0.14);

    // Night side subtle illumination (city-light feel, very faint)
    float nightMask = smoothstep(0.0, -0.15, NdotL);
    vec3 nightGlow = vec3(0.01, 0.01, 0.02) * nightMask;

    // ─── Combine ───
    vec3 colour = ambientColour * baseColour
                + diffuse * baseColour
                + spec * vec3(1.0, 0.97, 0.9)
                + oceanFresnel * vec3(0.3, 0.5, 0.7)
                + nightGlow;

    // Subtle tone mapping
    colour = colour / (colour + vec3(1.0));

    // Slight gamma
    colour = pow(colour, vec3(1.0 / 2.2));

    FragColor = vec4(colour, 1.0);
}
)glsl";

// ═══════════════════════════════════════════════════════════════
//  ATMOSPHERE SHADER (transparent shell)
// ═══════════════════════════════════════════════════════════════

inline const char* ATMO_VERT = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

out vec3 vNormal;
out vec3 vWorldPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * aNormal;
    gl_Position = uProjection * uView * worldPos;
}
)glsl";

inline const char* ATMO_FRAG = R"glsl(
#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uCameraPos;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(uLightDir);

    // Rim intensity — atmosphere visible at grazing angles
    float rim = 1.0 - max(dot(N, V), 0.0);
    rim = pow(rim, 2.5);

    // Light-side emphasis
    float NdotL = dot(N, L);
    float litSide = smoothstep(-0.2, 0.6, NdotL);

    // Rayleigh-ish scattering colour (blue dominant)
    vec3 atmoColour = vec3(0.25, 0.45, 0.85);

    // Sunset tint at terminator
    float terminator = smoothstep(-0.1, 0.1, NdotL);
    vec3 sunsetTint = mix(vec3(0.6, 0.25, 0.1), atmoColour, terminator);

    vec3 finalColour = mix(sunsetTint, atmoColour, 0.7);
    float alpha = rim * litSide * 0.55;

    // Boost very edge for visible halo
    alpha += pow(rim, 5.0) * 0.2;

    FragColor = vec4(finalColour, alpha);
}
)glsl";

// ═══════════════════════════════════════════════════════════════
//  STAR BACKGROUND SHADER
// ═══════════════════════════════════════════════════════════════

inline const char* STAR_VERT = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;

out vec3 vDir;

uniform mat4 uInvViewProj;

void main() {
    vDir = (uInvViewProj * vec4(aPos.xy, 1.0, 1.0)).xyz;
    gl_Position = vec4(aPos.xy, 0.9999, 1.0);
}
)glsl";

inline const char* STAR_FRAG = R"glsl(
#version 330 core

in vec3 vDir;
out vec4 FragColor;

float hash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

void main() {
    vec3 dir = normalize(vDir);

    // Quantise direction to create star grid
    vec3 cell = floor(dir * 120.0);
    float star = hash31(cell);

    // Only some cells have stars, and vary brightness
    float brightness = 0.0;
    if (star > 0.985) {
        brightness = (star - 0.985) / 0.015;
        brightness = pow(brightness, 2.0) * 0.8;
        // Slight colour variation
    }

    // Very subtle blue nebula background
    float nebula = hash31(floor(dir * 12.0)) * 0.008;

    vec3 colour = vec3(brightness) + vec3(0.0, 0.0, nebula);

    FragColor = vec4(colour, 1.0);
}
)glsl";

// ═══════════════════════════════════════════════════════════════
//  CLOUD SHADER
// ═══════════════════════════════════════════════════════════════

inline const char* CLOUD_VERT = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uProjection * uView * worldPos;
}
)glsl";

inline const char* CLOUD_FRAG = R"glsl(
#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform float uTime;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x),
               mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    mat2 rot = mat2(0.87, 0.48, -0.48, 0.87); // Domain rotation
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p = rot * p * 2.0;
        a *= 0.5;
    }
    return v;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);

    // Animated cloud pattern
    vec2 cloudUV = vUV * vec2(6.0, 3.0) + vec2(uTime * 0.003, 0.0);
    float clouds = fbm(cloudUV);

    // Shape the clouds — make them patchy, not uniform fog
    clouds = smoothstep(0.42, 0.68, clouds);

    // Thin out at poles
    float latitude = abs(vUV.y - 0.5) * 2.0;
    clouds *= smoothstep(0.95, 0.6, latitude);

    // Lighting
    float NdotL = dot(N, L);
    float lit = smoothstep(-0.1, 0.3, NdotL);

    // Clouds are bright on lit side, dark on night side
    vec3 litCloud = vec3(0.95, 0.95, 0.97);
    vec3 darkCloud = vec3(0.15, 0.15, 0.2);
    vec3 cloudColour = mix(darkCloud, litCloud, lit);

    // Slight silver lining at terminator
    float terminator = 1.0 - abs(NdotL);
    terminator = pow(terminator, 4.0) * 0.3;
    cloudColour += vec3(terminator);

    float alpha = clouds * 0.6;

    FragColor = vec4(cloudColour, alpha);
}
)glsl";

// ═══════════════════════════════════════════════════════════════
//  PLANET RENDERER
// ═══════════════════════════════════════════════════════════════

class PlanetRenderer {
public:
    void init(const PlanetData& planet) {
        LOG_INFO("Initialising planet renderer...");

        window_ = std::make_unique<Window>(1280, 720, "God Simulation — " + planet.name);
        sea_level_ = planet.sea_level;

        // OpenGL state
        gl::Enable(GL_DEPTH_TEST);
        gl::DepthFunc(GL_LESS);
        gl::Enable(GL_CULL_FACE);
        gl::CullFace(GL_BACK);
        gl::FrontFace(GL_CCW);
        gl::Enable(GL_MULTISAMPLE);
        gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        // Compile all shader programs
        planet_shader_.compile(PLANET_VERT, PLANET_FRAG);
        atmo_shader_.compile(ATMO_VERT, ATMO_FRAG);
        star_shader_.compile(STAR_VERT, STAR_FRAG);
        cloud_shader_.compile(CLOUD_VERT, CLOUD_FRAG);

        // Create meshes
        planet_mesh_.create(planet, 200, 400);
        atmo_mesh_.create(planet, 64, 128);  // Lower res is fine for atmosphere
        cloud_mesh_.create(planet, 80, 160);

        // Fullscreen quad for stars
        create_fullscreen_quad();

        camera_.set_distance(3.2f);

        LOG_INFO("Planet renderer ready");
        LOG_INFO("  Controls:");
        LOG_INFO("    Left-drag  : Orbit camera");
        LOG_INFO("    Scroll     : Zoom in/out");
        LOG_INFO("    R          : Reset camera");
        LOG_INFO("    G          : Toggle wireframe");
        LOG_INFO("    ESC        : Close");
    }

    void run() {
        bool wireframe = false;
        bool g_was_pressed = false;

        while (!window_->should_close()) {
            const auto& input = window_->poll();
            if (input.key_escape) break;

            if (input.key_g && !g_was_pressed) {
                wireframe = !wireframe;
            }
            g_was_pressed = input.key_g;

            if (input.resized) {
                gl::Viewport(0, 0, input.width, input.height);
            }
            camera_.set_aspect(static_cast<float>(input.width) /
                               std::max(static_cast<float>(input.height), 1.0f));
            camera_.update(input, 1.0f / 60.0f);

            time_ += 1.0f / 60.0f;

            gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::vec3 lightDir = glm::normalize(glm::vec3(0.7f, 0.5f, 0.5f));

            // ─── Pass 1: Star background ───
            gl::Disable(GL_DEPTH_TEST);
            gl::DepthFunc(GL_ALWAYS);
            {
                star_shader_.use();
                glm::mat4 invVP = glm::inverse(camera_.projection() * camera_.view());
                star_shader_.set_mat4("uInvViewProj", glm::value_ptr(invVP));

                gl::BindVertexArray(quad_vao_);
                gl::DrawArrays(GL_TRIANGLES, 0, 6);
                gl::BindVertexArray(0);
            }
            gl::Enable(GL_DEPTH_TEST);
            gl::DepthFunc(GL_LESS);

            // ─── Pass 2: Planet surface ───
            if (wireframe) gl::PolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            {
                planet_shader_.use();

                glm::mat4 model(1.0f);
                planet_shader_.set_mat4("uModel", glm::value_ptr(model));
                planet_shader_.set_mat4("uView", glm::value_ptr(camera_.view()));
                planet_shader_.set_mat4("uProjection", glm::value_ptr(camera_.projection()));

                planet_shader_.set_vec3("uLightDir", lightDir.x, lightDir.y, lightDir.z);
                planet_shader_.set_vec3("uCameraPos",
                    camera_.position().x, camera_.position().y, camera_.position().z);
                planet_shader_.set_float("uSeaLevel", sea_level_);
                planet_shader_.set_float("uDisplacementScale", 0.035f);
                planet_shader_.set_float("uTime", time_);

                planet_mesh_.bind_textures();
                planet_shader_.set_int("uBiomeTex", 0);
                planet_shader_.set_int("uElevationTex", 1);
                planet_shader_.set_int("uNormalTex", 2);

                planet_mesh_.draw();
            }
            if (wireframe) gl::PolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // ─── Pass 3: Clouds ───
            gl::Enable(GL_BLEND);
            gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gl::DepthFunc(GL_LEQUAL);
            {
                cloud_shader_.use();

                // Slightly larger than planet
                glm::mat4 cloudModel = glm::scale(glm::mat4(1.0f), glm::vec3(1.012f));
                cloud_shader_.set_mat4("uModel", glm::value_ptr(cloudModel));
                cloud_shader_.set_mat4("uView", glm::value_ptr(camera_.view()));
                cloud_shader_.set_mat4("uProjection", glm::value_ptr(camera_.projection()));
                cloud_shader_.set_vec3("uLightDir", lightDir.x, lightDir.y, lightDir.z);
                cloud_shader_.set_vec3("uCameraPos",
                    camera_.position().x, camera_.position().y, camera_.position().z);
                cloud_shader_.set_float("uTime", time_);

                cloud_mesh_.draw();
            }

            // ─── Pass 4: Atmosphere ───
            gl::CullFace(GL_FRONT); // Render back faces of atmosphere shell
            {
                atmo_shader_.use();

                glm::mat4 atmoModel = glm::scale(glm::mat4(1.0f), glm::vec3(1.06f));
                atmo_shader_.set_mat4("uModel", glm::value_ptr(atmoModel));
                atmo_shader_.set_mat4("uView", glm::value_ptr(camera_.view()));
                atmo_shader_.set_mat4("uProjection", glm::value_ptr(camera_.projection()));
                atmo_shader_.set_vec3("uLightDir", lightDir.x, lightDir.y, lightDir.z);
                atmo_shader_.set_vec3("uCameraPos",
                    camera_.position().x, camera_.position().y, camera_.position().z);

                atmo_mesh_.draw();
            }
            gl::CullFace(GL_BACK);
            gl::Disable(GL_BLEND);
            gl::DepthFunc(GL_LESS);

            window_->swap_buffers();
        }
    }

private:
    void create_fullscreen_quad() {
        float quad[] = {
            -1.0f, -1.0f, 0.0f,
             1.0f, -1.0f, 0.0f,
             1.0f,  1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f,
             1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
        };

        gl::GenVertexArrays(1, &quad_vao_);
        gl::GenBuffers(1, &quad_vbo_);
        gl::BindVertexArray(quad_vao_);
        gl::BindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
        gl::BufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        gl::EnableVertexAttribArray(0);
        gl::BindVertexArray(0);
    }

    std::unique_ptr<Window> window_;
    Camera camera_;
    ShaderProgram planet_shader_, atmo_shader_, star_shader_, cloud_shader_;
    SphereMesh planet_mesh_, atmo_mesh_, cloud_mesh_;
    GLuint quad_vao_ = 0, quad_vbo_ = 0;
    float sea_level_ = 0.4f;
    float time_ = 0.0f;
};

} // namespace godsim
