#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "InputState.h"
#include <cmath>
#include <algorithm>

namespace godsim {

/// Orbit camera that rotates around the planet origin.
/// Left-drag to orbit, scroll to zoom, R to reset.
class Camera {
public:
    Camera() { update_matrices(); }

    void set_aspect(float aspect) {
        aspect_ = aspect;
        update_matrices();
    }

    /// Process input for one frame.
    void update(const InputState& input, float dt) {
        // Orbit: left mouse drag (only when not doing terraforming)
        if (input.left_mouse_down && !input.right_mouse_down) {
            float sensitivity = 0.005f;
            yaw_   -= static_cast<float>(input.mouse_dx) * sensitivity;
            pitch_ += static_cast<float>(input.mouse_dy) * sensitivity;

            // Clamp pitch to avoid gimbal flip
            pitch_ = std::clamp(pitch_, -1.5f, 1.5f);
        }

        // Zoom: scroll (when not terraforming)
        if (input.scroll_dy != 0 && !input.right_mouse_down) {
            distance_ -= static_cast<float>(input.scroll_dy) * 0.3f;
            distance_ = std::clamp(distance_, min_distance_, max_distance_);
        }

        // Reset
        if (input.key_r) {
            yaw_ = 0.0f;
            pitch_ = 0.3f;
            distance_ = 3.2f;
        }

        update_matrices();
    }

    /// Cast a ray from screen coordinates into world space.
    /// Returns ray origin and direction.
    void screen_to_ray(double screen_x, double screen_y,
                       int viewport_w, int viewport_h,
                       glm::vec3& ray_origin, glm::vec3& ray_dir) const {
        // Convert screen coords to NDC [-1, 1]
        float ndc_x = (2.0f * static_cast<float>(screen_x)) / viewport_w - 1.0f;
        float ndc_y = 1.0f - (2.0f * static_cast<float>(screen_y)) / viewport_h;

        // Unproject near and far points
        glm::mat4 inv_vp = glm::inverse(proj_ * view_);
        glm::vec4 near_ndc(ndc_x, ndc_y, -1.0f, 1.0f);
        glm::vec4 far_ndc(ndc_x, ndc_y, 1.0f, 1.0f);

        glm::vec4 near_world = inv_vp * near_ndc;
        glm::vec4 far_world  = inv_vp * far_ndc;

        near_world /= near_world.w;
        far_world  /= far_world.w;

        ray_origin = glm::vec3(near_world);
        ray_dir = glm::normalize(glm::vec3(far_world) - glm::vec3(near_world));
    }

    const glm::mat4& view()       const { return view_; }
    const glm::mat4& projection() const { return proj_; }
    const glm::vec3& position()   const { return position_; }

    float distance()   const { return distance_; }
    void set_distance(float d) { distance_ = std::clamp(d, min_distance_, max_distance_); update_matrices(); }

    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }

private:
    void update_matrices() {
        // Camera position on a sphere around origin
        position_.x = distance_ * std::cos(pitch_) * std::sin(yaw_);
        position_.y = distance_ * std::sin(pitch_);
        position_.z = distance_ * std::cos(pitch_) * std::cos(yaw_);

        view_ = glm::lookAt(position_, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        proj_ = glm::perspective(glm::radians(fov_), aspect_, near_, far_);
    }

    // Orbit parameters
    float yaw_   = 0.0f;
    float pitch_ = 0.3f;
    float distance_ = 3.2f;
    float min_distance_ = 1.3f;
    float max_distance_ = 20.0f;

    // Projection
    float fov_    = 45.0f;
    float aspect_ = 16.0f / 9.0f;
    float near_   = 0.01f;
    float far_    = 100.0f;

    // Computed
    glm::mat4 view_{1.0f};
    glm::mat4 proj_{1.0f};
    glm::vec3 position_{0.0f, 0.0f, 3.2f};
};

} // namespace godsim
