#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "GL33Loader.h"
#include "core/util/Log.h"

#include "InputState.h"

#include <string>
#include <functional>
#include <stdexcept>

namespace godsim {

class Window {
public:
    Window(int width = 1280, int height = 720, const std::string& title = "God Simulation") {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialise GLFW");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4); // MSAA
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!window_) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1); // VSync

        // Load OpenGL
        if (!gl::load()) {
            glfwDestroyWindow(window_);
            glfwTerminate();
            throw std::runtime_error("Failed to load OpenGL functions");
        }

        LOG_INFO("OpenGL {} on {}",
                 reinterpret_cast<const char*>(gl::GetString(GL_VERSION)),
                 reinterpret_cast<const char*>(gl::GetString(GL_RENDERER)));

        // Set up callbacks
        glfwSetWindowUserPointer(window_, this);

        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* w, int width, int height) {
            auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
            self->input_.width = width;
            self->input_.height = height;
            self->input_.resized = true;
        });

        glfwSetScrollCallback(window_, [](GLFWwindow* w, double, double yoffset) {
            auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
            self->input_.scroll_dy += yoffset;
        });

        // Store initial size
        glfwGetFramebufferSize(window_, &input_.width, &input_.height);
    }

    ~Window() {
        if (window_) glfwDestroyWindow(window_);
        glfwTerminate();
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool should_close() const { return glfwWindowShouldClose(window_); }

    /// Poll input and return current state.
    const InputState& poll() {
        glfwPollEvents();

        // Mouse position and delta
        double mx, my;
        glfwGetCursorPos(window_, &mx, &my);
        input_.mouse_dx = mx - input_.mouse_x;
        input_.mouse_dy = my - input_.mouse_y;
        input_.mouse_x = mx;
        input_.mouse_y = my;

        // Mouse buttons
        input_.left_mouse_down   = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT)   == GLFW_PRESS;
        input_.right_mouse_down  = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT)  == GLFW_PRESS;
        input_.middle_mouse_down = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

        // Keys
        input_.key_escape = glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        input_.key_w      = glfwGetKey(window_, GLFW_KEY_W)      == GLFW_PRESS;
        input_.key_a      = glfwGetKey(window_, GLFW_KEY_A)      == GLFW_PRESS;
        input_.key_s      = glfwGetKey(window_, GLFW_KEY_S)      == GLFW_PRESS;
        input_.key_d      = glfwGetKey(window_, GLFW_KEY_D)      == GLFW_PRESS;
        input_.key_space  = glfwGetKey(window_, GLFW_KEY_SPACE)  == GLFW_PRESS;
        input_.key_shift  = glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        input_.key_1      = glfwGetKey(window_, GLFW_KEY_1)      == GLFW_PRESS;
        input_.key_2      = glfwGetKey(window_, GLFW_KEY_2)      == GLFW_PRESS;
        input_.key_3      = glfwGetKey(window_, GLFW_KEY_3)      == GLFW_PRESS;
        input_.key_4      = glfwGetKey(window_, GLFW_KEY_4)      == GLFW_PRESS;
        input_.key_5      = glfwGetKey(window_, GLFW_KEY_5)      == GLFW_PRESS;
        input_.key_r      = glfwGetKey(window_, GLFW_KEY_R)      == GLFW_PRESS;
        input_.key_g      = glfwGetKey(window_, GLFW_KEY_G)      == GLFW_PRESS;

        return input_;
    }

    void swap_buffers() {
        glfwSwapBuffers(window_);

        // Reset per-frame accumulators
        input_.scroll_dy = 0;
        input_.resized = false;
    }

    GLFWwindow* handle() { return window_; }

private:
    GLFWwindow* window_ = nullptr;
    InputState input_;
};

} // namespace godsim
