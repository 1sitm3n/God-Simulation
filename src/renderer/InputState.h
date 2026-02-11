#pragma once

namespace godsim {

/// Input state passed to the renderer each frame.
struct InputState {
    // Mouse
    double mouse_x = 0, mouse_y = 0;
    double mouse_dx = 0, mouse_dy = 0;
    double scroll_dy = 0;
    bool left_mouse_down = false;
    bool right_mouse_down = false;
    bool middle_mouse_down = false;

    // Window
    int width = 1280, height = 720;
    bool resized = false;

    // Keys
    bool key_escape = false;
    bool key_w = false, key_a = false, key_s = false, key_d = false;
    bool key_space = false, key_shift = false;
    bool key_1 = false, key_2 = false, key_3 = false, key_4 = false, key_5 = false;
    bool key_r = false;
    bool key_g = false;
};

} // namespace godsim
