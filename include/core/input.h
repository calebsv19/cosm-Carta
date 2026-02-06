#ifndef MAPFORGE_CORE_INPUT_H
#define MAPFORGE_CORE_INPUT_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

// Stores per-frame input state for the app loop.
typedef struct InputState {
    bool quit;
    int mouse_x;
    int mouse_y;
    int mouse_dx;
    int mouse_dy;
    uint32_t mouse_buttons;
    int mouse_wheel_y;
    bool toggle_debug_pressed;
    bool pan_left;
    bool pan_right;
    bool pan_up;
    bool pan_down;
    bool toggle_single_line_pressed;
    bool toggle_region_pressed;
    bool toggle_profile_pressed;
    bool toggle_playback_pressed;
    bool toggle_landuse_pressed;
    bool toggle_building_fill_pressed;
    bool toggle_polygon_outline_pressed;
    bool playback_step_forward;
    bool playback_step_back;
    bool playback_speed_up;
    bool playback_speed_down;
    bool shift_down;
    bool left_click_pressed;
    bool right_click_pressed;
    bool middle_click_pressed;
    bool left_click_released;
    bool right_click_released;
    bool enter_pressed;
    bool copy_overlay_pressed;
} InputState;

// Initializes an input state container.
void input_init(InputState *input);

// Resets per-frame input deltas.
void input_begin_frame(InputState *input);

// Updates input state from a single SDL event.
void input_handle_event(InputState *input, const SDL_Event *event);

#endif
