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
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace godsim {

// ═══════════════════════════════════════════════════════════════
//  PLANET SURFACE SHADER (with cursor highlight)
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

// Cursor highlight
uniform vec3  uCursorUV;      // xy = UV of cursor on planet (-1 if not hovering), z unused
uniform float uBrushRadius;   // Brush radius in UV space
uniform int   uTerraformMode; // 0 = off, 1 = on (show brush)

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
    vec3 T = normalize(cross(vec3(0.0, 1.0, 0.0), N));
    if (length(T) < 0.01) T = normalize(cross(vec3(1.0, 0.0, 0.0), N));
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    vec3 perturbedN = normalize(TBN * tangentNormal);

    float isLand = smoothstep(uSeaLevel - 0.01, uSeaLevel + 0.02, vElevation);
    vec3 finalN = normalize(mix(N, perturbedN, isLand * 0.7));

    // ─── Base colour ───
    vec3 baseColour = texture(uBiomeTex, vUV).rgb;

    float noiseVal = noise2D(vUV * 200.0) * 0.06 - 0.03;
    baseColour += vec3(noiseVal) * isLand;

    // ─── Ocean rendering ───
    float isOcean = 1.0 - isLand;
    if (isOcean > 0.5) {
        float depth = (uSeaLevel - vElevation) / uSeaLevel;
        vec3 shallowOcean = vec3(0.12, 0.30, 0.45);
        vec3 deepOcean    = vec3(0.04, 0.10, 0.22);
        baseColour = mix(shallowOcean, deepOcean, smoothstep(0.0, 0.6, depth));

        float shimmer = noise2D(vUV * 80.0 + vec2(uTime * 0.01, uTime * 0.007)) * 0.03;
        baseColour += vec3(shimmer * 0.5, shimmer * 0.7, shimmer);
    }

    // ─── Lighting ───
    float NdotL = dot(finalN, L);
    float diffuse = max(0.0, NdotL * 0.6 + 0.4) * 0.7;
    float hardDiffuse = max(0.0, NdotL);

    vec3 H = normalize(L + V);
    float NdotH = max(dot(finalN, H), 0.0);
    float specOcean = pow(NdotH, 64.0) * 0.8 * isOcean * hardDiffuse;
    float specLand = pow(NdotH, 16.0) * 0.05 * isLand * hardDiffuse;
    float spec = specOcean + specLand;

    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 4.0);
    float oceanFresnel = fresnel * 0.25 * isOcean * hardDiffuse;

    vec3 ambientColour = vec3(0.08, 0.09, 0.14);

    float nightMask = smoothstep(0.0, -0.15, NdotL);
    vec3 nightGlow = vec3(0.01, 0.01, 0.02) * nightMask;

    vec3 colour = ambientColour * baseColour
                + diffuse * baseColour
                + spec * vec3(1.0, 0.97, 0.9)
                + oceanFresnel * vec3(0.3, 0.5, 0.7)
                + nightGlow;

    // ─── Cursor highlight ───
    if (uCursorUV.x >= 0.0) {
        vec2 delta = vUV - uCursorUV.xy;
        // Wrap around longitude
        if (delta.x > 0.5) delta.x -= 1.0;
        if (delta.x < -0.5) delta.x += 1.0;
        // Correct for latitude compression
        float lat = abs(vUV.y - 0.5) * 3.14159;
        delta.x *= max(cos(lat), 0.1);

        float dist = length(delta);

        if (uTerraformMode > 0) {
            // Terraforming brush ring
            float ring = smoothstep(uBrushRadius - 0.004, uBrushRadius, dist)
                       - smoothstep(uBrushRadius, uBrushRadius + 0.004, dist);
            colour += vec3(0.4, 0.8, 0.2) * ring * 2.0;

            // Soft fill inside brush
            float fill = 1.0 - smoothstep(0.0, uBrushRadius, dist);
            colour += vec3(0.1, 0.25, 0.05) * fill * 0.5;
        } else {
            // Simple crosshair dot
            float dot_ring = smoothstep(0.004, 0.003, dist)
                           - smoothstep(0.002, 0.001, dist);
            colour += vec3(1.0, 1.0, 0.5) * dot_ring * 1.5;
        }
    }

    // Tone mapping + gamma
    colour = colour / (colour + vec3(1.0));
    colour = pow(colour, vec3(1.0 / 2.2));

    FragColor = vec4(colour, 1.0);
}
)glsl";

// ═══════════════════════════════════════════════════════════════
//  ATMOSPHERE SHADER
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

    float rim = 1.0 - max(dot(N, V), 0.0);
    rim = pow(rim, 2.5);

    float NdotL = dot(N, L);
    float litSide = smoothstep(-0.2, 0.6, NdotL);

    vec3 atmoColour = vec3(0.25, 0.45, 0.85);

    float terminator = smoothstep(-0.1, 0.1, NdotL);
    vec3 sunsetTint = mix(vec3(0.6, 0.25, 0.1), atmoColour, terminator);

    vec3 finalColour = mix(sunsetTint, atmoColour, 0.7);
    float alpha = rim * litSide * 0.55;
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
    gl_Position = vec4(aPos.xy, 0.999, 1.0);
    vec4 worldDir = uInvViewProj * vec4(aPos.xy, 1.0, 1.0);
    vDir = worldDir.xyz / worldDir.w;
}
)glsl";

inline const char* STAR_FRAG = R"glsl(
#version 330 core

in vec3 vDir;
out vec4 FragColor;

float hash(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

void main() {
    vec3 dir = normalize(vDir);

    // Quantise direction to a grid on the sphere
    float gridSize = 200.0;
    vec3 cell = floor(dir * gridSize);
    float starVal = hash(cell);

    float brightness = 0.0;
    if (starVal > 0.985) {
        brightness = (starVal - 0.985) * 66.0;
        brightness *= brightness;
    }

    vec3 starColour = vec3(0.9, 0.92, 1.0) * brightness;

    // Faint blue nebula background
    float nebula = hash(floor(dir * 15.0)) * 0.015;
    vec3 bg = vec3(0.01, 0.01, 0.03) + vec3(0.0, 0.0, nebula);

    FragColor = vec4(bg + starColour, 1.0);
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
    mat2 rot = mat2(0.87, 0.48, -0.48, 0.87);
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

    vec2 cloudUV = vUV * vec2(6.0, 3.0) + vec2(uTime * 0.003, 0.0);
    float clouds = fbm(cloudUV);
    clouds = smoothstep(0.42, 0.68, clouds);

    float latitude = abs(vUV.y - 0.5) * 2.0;
    clouds *= smoothstep(0.95, 0.6, latitude);

    float NdotL = dot(N, L);
    float lit = smoothstep(-0.1, 0.3, NdotL);

    vec3 litCloud = vec3(0.95, 0.95, 0.97);
    vec3 darkCloud = vec3(0.15, 0.15, 0.2);
    vec3 cloudColour = mix(darkCloud, litCloud, lit);

    float terminator = 1.0 - abs(NdotL);
    terminator = pow(terminator, 4.0) * 0.3;
    cloudColour += vec3(terminator);

    float alpha = clouds * 0.6;

    FragColor = vec4(cloudColour, alpha);
}
)glsl";

// ═══════════════════════════════════════════════════════════════
//  PLANET PICKING — ray-sphere intersection
// ═══════════════════════════════════════════════════════════════

struct PickResult {
    bool  hit = false;
    float u = 0, v = 0;           // UV on sphere [0,1]
    int   grid_x = 0, grid_y = 0; // Grid cell in planet data
    glm::vec3 world_pos{0.0f};
};

/// Intersect ray with unit sphere at origin. Returns nearest hit.
inline PickResult pick_planet(const Camera& camera,
                               double screen_x, double screen_y,
                               int viewport_w, int viewport_h,
                               u32 grid_w, u32 grid_h) {
    PickResult result;

    glm::vec3 origin, dir;
    camera.screen_to_ray(screen_x, screen_y, viewport_w, viewport_h, origin, dir);

    // Ray-sphere: |origin + t*dir|^2 = 1
    float a = glm::dot(dir, dir);
    float b = 2.0f * glm::dot(origin, dir);
    float c = glm::dot(origin, origin) - 1.0f;
    float disc = b * b - 4.0f * a * c;

    if (disc < 0.0f) return result;

    float t = (-b - std::sqrt(disc)) / (2.0f * a);
    if (t < 0.0f) return result;

    result.hit = true;
    result.world_pos = origin + t * dir;

    // Convert world position to UV (matching sphere mesh generation)
    glm::vec3 p = glm::normalize(result.world_pos);

    // atan2(z, x) for longitude, asin(y) for latitude
    float lon = std::atan2(p.z, p.x);             // [-pi, pi]
    float lat = std::asin(std::clamp(p.y, -1.0f, 1.0f)); // [-pi/2, pi/2]

    // Convert to UV matching SphereMesh (u = sector/sectors, v = stack/stacks)
    result.u = lon / (2.0f * static_cast<float>(M_PI));
    if (result.u < 0.0f) result.u += 1.0f;

    result.v = 0.5f - lat / static_cast<float>(M_PI);  // 0 = north pole, 1 = south pole

    result.u = std::clamp(result.u, 0.0f, 1.0f);
    result.v = std::clamp(result.v, 0.0f, 1.0f);

    result.grid_x = static_cast<int>(result.u * (grid_w - 1));
    result.grid_y = static_cast<int>(result.v * (grid_h - 1));

    result.grid_x = std::clamp(result.grid_x, 0, static_cast<int>(grid_w - 1));
    result.grid_y = std::clamp(result.grid_y, 0, static_cast<int>(grid_h - 1));

    return result;
}

// ═══════════════════════════════════════════════════════════════
//  TERRAFORMING — modify planet heightmap with gaussian brush
// ═══════════════════════════════════════════════════════════════

/// Apply a gaussian brush to the heightmap centered on (cx, cy).
/// `strength` is positive to raise, negative to lower.
/// `radius` is in grid cells.
inline void terraform_brush(PlanetData& planet, int cx, int cy,
                             int radius, float strength) {
    int w = static_cast<int>(planet.width);
    int h = static_cast<int>(planet.height);
    float inv_r2 = 1.0f / (radius * radius);

    for (int dy = -radius; dy <= radius; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= h) continue;

        for (int dx = -radius; dx <= radius; dx++) {
            int px = cx + dx;
            // Wrap longitude
            if (px < 0) px += w;
            if (px >= w) px -= w;

            float d2 = static_cast<float>(dx * dx + dy * dy);
            float weight = std::exp(-d2 * inv_r2 * 2.0f); // Gaussian falloff

            float& elev = planet.elevation.at(static_cast<u32>(px), static_cast<u32>(py));
            elev = std::clamp(elev + strength * weight, 0.0f, 1.0f);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  PLANET RENDERER
// ═══════════════════════════════════════════════════════════════

class PlanetRenderer {
public:
    void init(PlanetData& planet) {
        LOG_INFO("Initialising planet renderer...");

        planet_ = &planet;
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
        atmo_mesh_.create(planet, 64, 128);
        cloud_mesh_.create(planet, 80, 160);

        // Fullscreen quad for stars
        create_fullscreen_quad();

        camera_.set_distance(3.2f);

        LOG_INFO("Planet renderer ready");
        LOG_INFO("  Controls:");
        LOG_INFO("    Left-drag   : Orbit camera");
        LOG_INFO("    Scroll      : Zoom in/out");
        LOG_INFO("    R           : Reset camera");
        LOG_INFO("    G           : Toggle wireframe");
        LOG_INFO("    T           : Toggle terraforming mode");
        LOG_INFO("    Right-click : Terraform (raise terrain)");
        LOG_INFO("    Shift+Right : Terraform (lower terrain)");
        LOG_INFO("    B           : Cycle brush size");
        LOG_INFO("    1-4         : Map modes (biome/elev/temp/moisture)");
        LOG_INFO("    C           : Toggle clouds");
        LOG_INFO("    P / Space   : Pause/unpause simulation");
        LOG_INFO("    +/-         : Simulation speed");
        LOG_INFO("    ESC         : Close");
    }

    void run() {
        // Key toggle tracking (for edge detection)
        bool prev_g = false, prev_t = false, prev_c = false;
        bool prev_p = false, prev_space = false;
        bool prev_1 = false, prev_2 = false, prev_3 = false, prev_4 = false;
        bool prev_b = false;
        bool prev_plus = false, prev_minus = false;

        while (!window_->should_close()) {
            const auto& input = window_->poll();
            if (input.key_escape) break;

            // ─── Toggle keys (edge-triggered) ───
            if (input.key_g && !prev_g) wireframe_ = !wireframe_;
            prev_g = input.key_g;

            if (input.key_t && !prev_t) terraform_mode_ = !terraform_mode_;
            prev_t = input.key_t;

            if (input.key_c && !prev_c) show_clouds_ = !show_clouds_;
            prev_c = input.key_c;

            if ((input.key_p && !prev_p) || (input.key_space && !prev_space))
                sim_paused_ = !sim_paused_;
            prev_p = input.key_p;
            prev_space = input.key_space;

            if (input.key_b && !prev_b) {
                brush_size_idx_ = (brush_size_idx_ + 1) % 4;
                brush_radii_ = std::array<int, 4>{4, 8, 16, 32};
            }
            prev_b = input.key_b;

            // Sim speed
            if (input.key_plus && !prev_plus) {
                sim_speed_idx_ = std::min(sim_speed_idx_ + 1, 5);
            }
            prev_plus = input.key_plus;
            if (input.key_minus && !prev_minus) {
                sim_speed_idx_ = std::max(sim_speed_idx_ - 1, 0);
            }
            prev_minus = input.key_minus;

            // Map modes
            if (input.key_1 && !prev_1) set_map_mode(MapMode::Biome);
            if (input.key_2 && !prev_2) set_map_mode(MapMode::Elevation);
            if (input.key_3 && !prev_3) set_map_mode(MapMode::Temperature);
            if (input.key_4 && !prev_4) set_map_mode(MapMode::Moisture);
            prev_1 = input.key_1; prev_2 = input.key_2;
            prev_3 = input.key_3; prev_4 = input.key_4;

            // ─── Viewport / Camera ───
            if (input.resized) {
                gl::Viewport(0, 0, input.width, input.height);
            }
            camera_.set_aspect(static_cast<float>(input.width) /
                               std::max(static_cast<float>(input.height), 1.0f));
            camera_.update(input, 1.0f / 60.0f);

            time_ += 1.0f / 60.0f;

            // ─── Planet picking ───
            pick_ = pick_planet(camera_, input.mouse_x, input.mouse_y,
                                input.width, input.height,
                                planet_->width, planet_->height);

            // ─── Terraforming ───
            if (terraform_mode_ && pick_.hit && input.right_mouse_down) {
                int radius = brush_radii_[brush_size_idx_];
                float strength = input.key_shift ? -0.008f : 0.008f;

                terraform_brush(*planet_, pick_.grid_x, pick_.grid_y, radius, strength);

                // Reclassify biomes in affected area
                int w = static_cast<int>(planet_->width);
                int h = static_cast<int>(planet_->height);
                for (int dy = -radius - 2; dy <= radius + 2; dy++) {
                    int py = pick_.grid_y + dy;
                    if (py < 0 || py >= h) continue;
                    for (int dx = -radius - 2; dx <= radius + 2; dx++) {
                        int px = pick_.grid_x + dx;
                        if (px < 0) px += w;
                        if (px >= w) px -= w;
                        u32 ux = static_cast<u32>(px);
                        u32 uy = static_cast<u32>(py);
                        planet_->biome_map[uy * planet_->width + ux] =
                            classify_biome(planet_->elevation.get(ux, uy),
                                           planet_->temperature.get(ux, uy),
                                           planet_->moisture.get(ux, uy),
                                           planet_->sea_level);
                    }
                }

                // Rebuild textures
                planet_mesh_.rebuild_textures(*planet_);

                // If in a data mode, re-apply it
                if (map_mode_ != MapMode::Biome) {
                    planet_mesh_.set_map_mode(*planet_, map_mode_);
                }

                terrain_dirty_ = true;
            }

            // ─── Scroll terraform: raise/lower with scroll while right-clicking ───
            if (terraform_mode_ && pick_.hit && input.right_mouse_down && input.scroll_dy != 0) {
                int radius = brush_radii_[brush_size_idx_];
                float strength = static_cast<float>(input.scroll_dy) * 0.02f;
                terraform_brush(*planet_, pick_.grid_x, pick_.grid_y, radius, strength);
                planet_->classify_biomes();
                planet_mesh_.rebuild_textures(*planet_);
                if (map_mode_ != MapMode::Biome) {
                    planet_mesh_.set_map_mode(*planet_, map_mode_);
                }
            }

            // ─── Update title bar HUD ───
            update_title();

            // ─── Render ───
            gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::vec3 lightDir = glm::normalize(glm::vec3(0.7f, 0.5f, 0.5f));

            // Pass 1: Stars
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

            // Pass 2: Planet surface
            if (wireframe_) gl::PolygonMode(GL_FRONT_AND_BACK, GL_LINE);
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

                // Cursor highlight uniforms
                if (pick_.hit) {
                    planet_shader_.set_vec3("uCursorUV", pick_.u, pick_.v, 0.0f);
                } else {
                    planet_shader_.set_vec3("uCursorUV", -1.0f, -1.0f, 0.0f);
                }

                float brush_uv_radius = static_cast<float>(brush_radii_[brush_size_idx_])
                                       / static_cast<float>(planet_->width);
                planet_shader_.set_float("uBrushRadius", brush_uv_radius);
                planet_shader_.set_int("uTerraformMode", terraform_mode_ ? 1 : 0);

                planet_mesh_.bind_textures();
                planet_shader_.set_int("uBiomeTex", 0);
                planet_shader_.set_int("uElevationTex", 1);
                planet_shader_.set_int("uNormalTex", 2);

                planet_mesh_.draw();
            }
            if (wireframe_) gl::PolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // Pass 3: Clouds (optional)
            if (show_clouds_) {
                gl::Enable(GL_BLEND);
                gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                gl::DepthFunc(GL_LEQUAL);
                {
                    cloud_shader_.use();

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
            }

            // Pass 4: Atmosphere
            gl::Enable(GL_BLEND);
            gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gl::DepthFunc(GL_LEQUAL);
            gl::CullFace(GL_FRONT);
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
    void set_map_mode(MapMode mode) {
        if (mode == map_mode_) return;
        map_mode_ = mode;
        planet_mesh_.set_map_mode(*planet_, mode);
        LOG_INFO("Map mode: {}", map_mode_name(mode));
    }

    void update_title() {
        static int frame_counter = 0;
        if (++frame_counter < 10) return; // Update every 10 frames
        frame_counter = 0;

        char buf[256];

        // Sim speed names
        static const char* speed_names[] = {"0.5x", "1x", "2x", "5x", "10x", "50x"};

        if (pick_.hit) {
            BiomeType biome = planet_->biome_at(
                static_cast<u32>(pick_.grid_x),
                static_cast<u32>(pick_.grid_y));
            const auto& info = BIOME_INFO[static_cast<size_t>(biome)];

            float elev = planet_->elevation.get(
                static_cast<u32>(pick_.grid_x),
                static_cast<u32>(pick_.grid_y));
            float temp = planet_->temperature.get(
                static_cast<u32>(pick_.grid_x),
                static_cast<u32>(pick_.grid_y));
            float moist = planet_->moisture.get(
                static_cast<u32>(pick_.grid_x),
                static_cast<u32>(pick_.grid_y));

            float lat_deg = (0.5f - pick_.v) * 180.0f;
            float lon_deg = (pick_.u - 0.5f) * 360.0f;

            std::snprintf(buf, sizeof(buf),
                "God Sim | %s | Elev: %.2f | %.0f°C | Moist: %.2f | "
                "Lat: %.1f° Lon: %.1f° | Mode: %s | %s%s%s",
                info.name, elev, temp, moist,
                lat_deg, lon_deg,
                map_mode_name(map_mode_),
                sim_paused_ ? "PAUSED" : speed_names[sim_speed_idx_],
                terraform_mode_ ? " | TERRAFORM" : "",
                show_clouds_ ? "" : " | Clouds OFF");
        } else {
            std::snprintf(buf, sizeof(buf),
                "God Sim | Mode: %s | %s%s%s",
                map_mode_name(map_mode_),
                sim_paused_ ? "PAUSED" : speed_names[sim_speed_idx_],
                terraform_mode_ ? " | TERRAFORM" : "",
                show_clouds_ ? "" : " | Clouds OFF");
        }

        window_->set_title(buf);
    }

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

    // ─── State ───
    PlanetData* planet_ = nullptr;
    std::unique_ptr<Window> window_;
    Camera camera_;
    ShaderProgram planet_shader_, atmo_shader_, star_shader_, cloud_shader_;
    SphereMesh planet_mesh_, atmo_mesh_, cloud_mesh_;
    GLuint quad_vao_ = 0, quad_vbo_ = 0;
    float sea_level_ = 0.4f;
    float time_ = 0.0f;

    // Picking
    PickResult pick_;

    // Terraforming
    bool terraform_mode_ = false;
    int brush_size_idx_ = 1; // Default: 8-cell radius
    std::array<int, 4> brush_radii_ = {4, 8, 16, 32};
    bool terrain_dirty_ = false;

    // Map modes
    MapMode map_mode_ = MapMode::Biome;

    // Visual toggles
    bool wireframe_ = false;
    bool show_clouds_ = true;

    // Simulation time
    bool sim_paused_ = true;
    int sim_speed_idx_ = 1; // Index into speed_names
};

} // namespace godsim
