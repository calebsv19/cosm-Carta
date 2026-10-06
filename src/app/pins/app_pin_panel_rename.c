#include "app/app_internal.h"
#include "app/app_pin_panel_internal.h"
#include "app/app_ui_internal.h"

#include "core/time.h"
#include "ui/font.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

static bool app_pin_name_point_in_rect(int x, int y, const SDL_FRect *rect) {
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f) {
        return false;
    }
    return (float)x >= rect->x &&
           (float)x <= rect->x + rect->w &&
           (float)y >= rect->y &&
           (float)y <= rect->y + rect->h;
}

bool app_pin_panel_name_edit_active(const AppState *app) {
    return app && app->ui_state_bridge.pin_name_edit_active;
}

void app_pin_panel_name_edit_deactivate(AppState *app) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_name_edit_active = false;
    carta_text_end(&app->ui_state_bridge.rollout.pin_name);
}

void app_pin_panel_name_edit_sync_cursor_to_end(AppState *app) {
    size_t len = 0u;
    if (!app) {
        return;
    }
    len = strnlen(app->ui_state_bridge.pin_editor_name_edit,
                  sizeof(app->ui_state_bridge.pin_editor_name_edit));
    app->ui_state_bridge.pin_name_cursor_index = (int)len;
    carta_text_bind(&app->ui_state_bridge.rollout.pin_name,app->ui_state_bridge.pin_editor_name_edit,sizeof(app->ui_state_bridge.pin_editor_name_edit),true);
}

static void app_pin_panel_name_edit_activate(AppState *app, int screen_x) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_name_edit_active = true;
    CartaTextInput *input=&app->ui_state_bridge.rollout.pin_name;
    carta_text_bind(input,app->ui_state_bridge.pin_editor_name_edit,sizeof(app->ui_state_bridge.pin_editor_name_edit),false);
    carta_text_click(input,screen_x,app->ui_state_bridge.pin_pane_name_rect.y+10,(SDL_GetModState()&KMOD_SHIFT)!=0);
    app->ui_state_bridge.pin_name_cursor_index=(int)input->edit.cursor;
}

void app_pin_panel_draw_name_field(AppState *app,
                                   const MapForgeThemePalette *palette,
                                   SDL_Color text,
                                   SDL_Color muted) {
    if(!app || !palette)return;
    SDL_FRect box=app->ui_state_bridge.pin_pane_name_rect;
    ui_draw_text(&app->renderer,(int)box.x+6,(int)box.y+5,"Name:",muted,.9f);
    float prefix=ui_measure_text_width("Name: ",.9f)+6;
    box.x+=prefix;box.w-=prefix;
    if(app_pin_panel_name_edit_active(app)) {
        carta_text_bind(&app->ui_state_bridge.rollout.pin_name,app->ui_state_bridge.pin_editor_name_edit,sizeof(app->ui_state_bridge.pin_editor_name_edit),false);
        carta_text_draw(app,&app->ui_state_bridge.rollout.pin_name,box,.9f);
    } else {
        ui_draw_text_clipped(&app->renderer,(int)box.x+6,(int)box.y+5,
            app->ui_state_bridge.pin_editor_name_edit[0]?app->ui_state_bridge.pin_editor_name_edit:"<new pin>",text,.9f,(int)box.w-12);
    }
}

bool app_pin_panel_handle_name_click(AppState *app, int x, int y) {
    double now = 0.0;
    bool in_rect = false;
    if (!app || !app->ui_state_bridge.pin_editor_has_draft) {
        return false;
    }
    in_rect = app_pin_name_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_name_rect);
    if (!in_rect) {
        app_pin_panel_name_edit_deactivate(app);
        return false;
    }
    now = time_now_seconds();
    if (app->ui_state_bridge.pin_name_edit_active ||
        (now - app->ui_state_bridge.pin_name_last_click_time_sec) <= 0.35) {
        app_pin_panel_name_edit_activate(app, x);
        app_pin_editor_set_status(app, "Rename active. Use left/right to move the caret, Enter to save.");
    } else {
        app_pin_editor_set_status(app, "Double-click the name field to rename.");
    }
    app->ui_state_bridge.pin_name_last_click_time_sec = now;
    return true;
}

bool app_pin_panel_handle_name_runtime_inputs(AppState *app) {
    if (!app || !app->ui_state_bridge.pin_editor_has_draft || !app_pin_panel_name_edit_active(app)) {
        return false;
    }
    // Text events are handled individually before aggregation; no duplicate byte edits.
    return app->ui_state_bridge.input.enter_pressed;
}
