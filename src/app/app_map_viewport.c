#include "app/app_internal.h"

#include <math.h>
#include <string.h>

static SDL_FRect app_default_map_viewport_rect(const AppState *app) {
    SDL_FRect rect = {0.0f, APP_HEADER_HEIGHT, 0.0f, 0.0f};
    if (!app) {
        return rect;
    }
    rect.w = (float)app->width;
    rect.h = (float)app->height - APP_HEADER_HEIGHT;
    if (rect.h < 0.0f) {
        rect.h = 0.0f;
    }
    return rect;
}

SDL_FRect app_map_viewport_rect(const AppState *app) {
    if (!app) {
        SDL_FRect empty = {0};
        return empty;
    }
    if (app->ui_state_bridge.map_viewport_rect.w <= 1.0f || app->ui_state_bridge.map_viewport_rect.h <= 1.0f) {
        return app_default_map_viewport_rect(app);
    }
    return app->ui_state_bridge.map_viewport_rect;
}

bool app_map_viewport_contains_screen_point(const AppState *app, int screen_x, int screen_y) {
    SDL_FRect rect = app_map_viewport_rect(app);
    return rect.w > 0.0f &&
           rect.h > 0.0f &&
           (float)screen_x >= rect.x &&
           (float)screen_x <= rect.x + rect.w &&
           (float)screen_y >= rect.y &&
           (float)screen_y <= rect.y + rect.h;
}

bool app_map_screen_to_world(const AppState *app, float screen_x, float screen_y, float *out_world_x, float *out_world_y) {
    SDL_FRect rect = app_map_viewport_rect(app);
    if (!app || !out_world_x || !out_world_y || rect.w <= 0.0f || rect.h <= 0.0f) {
        return false;
    }
    if (screen_x < rect.x || screen_x > rect.x + rect.w || screen_y < rect.y || screen_y > rect.y + rect.h) {
        return false;
    }
    camera_screen_to_world(&app->view_state_bridge.camera,
                           screen_x - rect.x,
                           screen_y - rect.y,
                           (int)lroundf(rect.w),
                           (int)lroundf(rect.h),
                           out_world_x,
                           out_world_y);
    return true;
}

bool app_map_world_to_screen(const AppState *app, float world_x, float world_y, float *out_screen_x, float *out_screen_y) {
    SDL_FRect rect = app_map_viewport_rect(app);
    float local_x = 0.0f;
    float local_y = 0.0f;
    if (!app || !out_screen_x || !out_screen_y || rect.w <= 0.0f || rect.h <= 0.0f) {
        return false;
    }
    camera_world_to_screen(&app->view_state_bridge.camera,
                           world_x,
                           world_y,
                           (int)lroundf(rect.w),
                           (int)lroundf(rect.h),
                           &local_x,
                           &local_y);
    *out_screen_x = rect.x + local_x;
    *out_screen_y = rect.y + local_y;
    return true;
}

bool app_map_world_to_viewport_local(const AppState *app, float world_x, float world_y, float *out_local_x, float *out_local_y) {
    SDL_FRect rect = app_map_viewport_rect(app);
    if (!app || !out_local_x || !out_local_y || rect.w <= 0.0f || rect.h <= 0.0f) {
        return false;
    }
    camera_world_to_screen(&app->view_state_bridge.camera,
                           world_x,
                           world_y,
                           (int)lroundf(rect.w),
                           (int)lroundf(rect.h),
                           out_local_x,
                           out_local_y);
    return true;
}

bool app_map_viewport_activate(AppState *app) {
    SDL_FRect rect = {0};
    if (!app) {
        return false;
    }
    rect = app_map_viewport_rect(app);
    if (rect.w <= 1.0f || rect.h <= 1.0f) {
        return false;
    }
    return renderer_set_viewport(&app->renderer,
                                 (int)lroundf(rect.x),
                                 (int)lroundf(rect.y),
                                 (int)lroundf(rect.w),
                                 (int)lroundf(rect.h));
}

void app_map_viewport_deactivate(AppState *app) {
    if (!app) {
        return;
    }
    renderer_reset_viewport(&app->renderer);
}
