#include "ui/control_surface.h"
#include "app/app_place_tools_internal.h"
#include "app/app_utility_workspace.h"
#include "app/history_ui.h"
#include "app/place_runtime.h"
// Keeps action eligibility identical to the actual places toolbar host.
SDL_FRect carta_places_control_rect(AppState *app,uint32_t domain,uint64_t action) {
    if (!app || !app->ui_state_bridge.left_pane_open ||
        app->ui_state_bridge.left_pane_section!=APP_LEFT_PANE_SECTION_PINS ||
        app->ui_state_bridge.utility_workspace==APP_UTILITY_HIKING_HISTORY ||
        app_place_tools_active() || history_ui_modal())return (SDL_FRect){0};
    if(domain==CARTA_CONTROL_PLACES && action<9)return tools_toolbar(app,(unsigned)action);
    if(domain==CARTA_CONTROL_ROUTES && action<2)return app_places_route_button_rect(app,(uint32_t)action);
    return (SDL_FRect){0};
}
void carta_places_control_activate(AppState *app,uint32_t domain,uint64_t action) {
    if(carta_places_control_rect(app,domain,action).w<=0)return;
    if(domain==CARTA_CONTROL_PLACES)app_place_tools_activate_toolbar(app,(unsigned)action);
    if(domain==CARTA_CONTROL_ROUTES)app_places_begin_route_action(app,action==0);
}
