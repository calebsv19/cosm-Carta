#include "app/app_utility_workspace.h"
#include "app/app_ui_internal.h"
#include "ui/font.h"
#include "ui/control_surface.h"
#include <stdio.h>

// Lays out equal navigation cells that also fit narrow windows.
SDL_FRect app_utility_workspace_rect(const AppState *app, uint32_t index) {
    float width = ((float)app->width - 16.0f) / 4.0f;
    if (width > 150.0f) width = 150.0f;
    if (width < 1.0f) width = 1.0f;
    return (SDL_FRect){8.0f + index * width, 38.0f, width - 4.0f, 26.0f};
}

void app_utility_workspace_select(AppState *app, uint32_t workspace) {
    if (!app || workspace > APP_UTILITY_DATA) return;
    app->ui_state_bridge.utility_workspace = workspace;
    app->ui_state_bridge.service_filter = 0;
    app->ui_state_bridge.pin_pane_row_base = 0;
    app->ui_state_bridge.pin_list_drag_active = false;
    app->ui_state_bridge.pin_list_drag_armed = false;
    app->ui_state_bridge.left_pane_open = workspace != APP_UTILITY_MAP;
    app->ui_state_bridge.left_pane_section = workspace == APP_UTILITY_DATA ? APP_LEFT_PANE_SECTION_INGEST : APP_LEFT_PANE_SECTION_PINS;
    /* Draft, edit cursor, selected pin and route endpoints remain intact. */
}

bool app_utility_workspace_handle_event(AppState *app, const SDL_Event *event) {
    return carta_ui_controls_event(app,event);
}
