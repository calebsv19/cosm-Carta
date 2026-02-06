#include "core/input.h"

void input_init(InputState *input) {
    if (!input) {
        return;
    }

    input->quit = false;
    input->mouse_x = 0;
    input->mouse_y = 0;
    input->mouse_dx = 0;
    input->mouse_dy = 0;
    input->mouse_buttons = 0;
    input->mouse_wheel_y = 0;
    input->toggle_debug_pressed = false;
    input->pan_left = false;
    input->pan_right = false;
    input->pan_up = false;
    input->pan_down = false;
    input->toggle_single_line_pressed = false;
    input->toggle_region_pressed = false;
    input->toggle_profile_pressed = false;
    input->toggle_playback_pressed = false;
    input->toggle_landuse_pressed = false;
    input->toggle_building_fill_pressed = false;
    input->toggle_polygon_outline_pressed = false;
    input->playback_step_forward = false;
    input->playback_step_back = false;
    input->playback_speed_up = false;
    input->playback_speed_down = false;
    input->left_click_pressed = false;
    input->right_click_pressed = false;
    input->middle_click_pressed = false;
    input->left_click_released = false;
    input->right_click_released = false;
    input->enter_pressed = false;
    input->copy_overlay_pressed = false;
}

void input_begin_frame(InputState *input) {
    if (!input) {
        return;
    }

    input->mouse_wheel_y = 0;
    input->toggle_debug_pressed = false;
    input->mouse_dx = 0;
    input->mouse_dy = 0;
    input->toggle_single_line_pressed = false;
    input->toggle_region_pressed = false;
    input->toggle_profile_pressed = false;
    input->toggle_playback_pressed = false;
    input->toggle_landuse_pressed = false;
    input->toggle_building_fill_pressed = false;
    input->toggle_polygon_outline_pressed = false;
    input->playback_step_forward = false;
    input->playback_step_back = false;
    input->playback_speed_up = false;
    input->playback_speed_down = false;
    input->left_click_pressed = false;
    input->right_click_pressed = false;
    input->middle_click_pressed = false;
    input->left_click_released = false;
    input->right_click_released = false;
    input->enter_pressed = false;
    input->copy_overlay_pressed = false;
}

void input_handle_event(InputState *input, const SDL_Event *event) {
    if (!input || !event) {
        return;
    }

    switch (event->type) {
        case SDL_QUIT:
            input->quit = true;
            break;
        case SDL_MOUSEMOTION:
            input->mouse_x = event->motion.x;
            input->mouse_y = event->motion.y;
            input->mouse_dx += event->motion.xrel;
            input->mouse_dy += event->motion.yrel;
            input->mouse_buttons = event->motion.state;
            break;
        case SDL_MOUSEWHEEL:
            input->mouse_wheel_y += event->wheel.y;
            break;
        case SDL_KEYDOWN:
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                input->quit = true;
            } else if (event->key.keysym.sym == SDLK_F1) {
                input->toggle_debug_pressed = true;
            } else if (event->key.keysym.sym == SDLK_F2) {
                input->toggle_single_line_pressed = true;
            } else if (event->key.keysym.sym == SDLK_F3) {
                input->toggle_region_pressed = true;
            } else if (event->key.keysym.sym == SDLK_F4) {
                input->toggle_profile_pressed = true;
            } else if (event->key.keysym.sym == SDLK_F5) {
                input->toggle_landuse_pressed = true;
            } else if (event->key.keysym.sym == SDLK_F6) {
                input->toggle_building_fill_pressed = true;
            } else if (event->key.keysym.sym == SDLK_F7) {
                input->toggle_polygon_outline_pressed = true;
            } else if (event->key.keysym.sym == SDLK_SPACE) {
                input->toggle_playback_pressed = true;
            } else if (event->key.keysym.sym == SDLK_PERIOD) {
                input->playback_step_forward = true;
            } else if (event->key.keysym.sym == SDLK_COMMA) {
                input->playback_step_back = true;
            } else if (event->key.keysym.sym == SDLK_EQUALS) {
                input->playback_speed_up = true;
            } else if (event->key.keysym.sym == SDLK_MINUS) {
                input->playback_speed_down = true;
            } else if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER) {
                input->enter_pressed = true;
            } else if (event->key.keysym.sym == SDLK_c &&
                       (event->key.keysym.mod & KMOD_GUI) != 0) {
                input->copy_overlay_pressed = true;
            } else if (event->key.keysym.sym == SDLK_LSHIFT || event->key.keysym.sym == SDLK_RSHIFT) {
                input->shift_down = true;
            } else if (event->key.keysym.sym == SDLK_a || event->key.keysym.sym == SDLK_LEFT) {
                input->pan_left = true;
            } else if (event->key.keysym.sym == SDLK_d || event->key.keysym.sym == SDLK_RIGHT) {
                input->pan_right = true;
            } else if (event->key.keysym.sym == SDLK_w || event->key.keysym.sym == SDLK_UP) {
                input->pan_up = true;
            } else if (event->key.keysym.sym == SDLK_s || event->key.keysym.sym == SDLK_DOWN) {
                input->pan_down = true;
            }
            break;
        case SDL_KEYUP:
            if (event->key.keysym.sym == SDLK_LSHIFT || event->key.keysym.sym == SDLK_RSHIFT) {
                input->shift_down = false;
            } else if (event->key.keysym.sym == SDLK_a || event->key.keysym.sym == SDLK_LEFT) {
                input->pan_left = false;
            } else if (event->key.keysym.sym == SDLK_d || event->key.keysym.sym == SDLK_RIGHT) {
                input->pan_right = false;
            } else if (event->key.keysym.sym == SDLK_w || event->key.keysym.sym == SDLK_UP) {
                input->pan_up = false;
            } else if (event->key.keysym.sym == SDLK_s || event->key.keysym.sym == SDLK_DOWN) {
                input->pan_down = false;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            input->mouse_x = event->button.x;
            input->mouse_y = event->button.y;
            input->mouse_buttons = SDL_GetMouseState(NULL, NULL);
            if (event->button.button == SDL_BUTTON_LEFT) {
                input->left_click_pressed = true;
            } else if (event->button.button == SDL_BUTTON_RIGHT) {
                input->right_click_pressed = true;
            } else if (event->button.button == SDL_BUTTON_MIDDLE) {
                input->middle_click_pressed = true;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            input->mouse_x = event->button.x;
            input->mouse_y = event->button.y;
            input->mouse_buttons = SDL_GetMouseState(NULL, NULL);
            if (event->button.button == SDL_BUTTON_LEFT) {
                input->left_click_released = true;
            } else if (event->button.button == SDL_BUTTON_RIGHT) {
                input->right_click_released = true;
            }
            break;
        default:
            break;
    }
}
