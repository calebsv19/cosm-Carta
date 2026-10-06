#include "ui/control_surface.h"
#include "app/app_ui_internal.h"
#include "app/app_utility_workspace.h"
#include "app/app_pin_panel_internal.h"
#include "app/app_runtime_ingest_internal.h"
#include "app/app_region_transition.h"
// Existing geometry and action bodies stay authoritative for the remaining HUD.
SDL_FRect carta_remaining_control_rect(AppState *app,uint32_t domain,unsigned action) {
    AppUiState *u=&app->ui_state_bridge;
    if(domain==CARTA_CONTROL_PREPARE)
        return app->region_prepare && action==0?app_region_transition_cancel_rect(app):(SDL_FRect){0};
    if(u->utility_workspace==APP_UTILITY_HIKING_HISTORY)return (SDL_FRect){0};
    if(domain==CARTA_CONTROL_PANEL) {
        if(!u->left_pane_open)return action==0?u->pin_pane_closed_rect:(SDL_FRect){0};
        if(action==1)return u->pin_pane_close_rect;
        if(action>=2 && action<5)return u->pin_pane_tab_rects[action-2];
        if(action>=5 && action<9 && u->left_pane_section!=APP_LEFT_PANE_SECTION_INSPECT) {
            SDL_FRect r[]={u->pin_pane_add_rect,u->pin_pane_save_rect,u->pin_pane_delete_rect,u->pin_pane_cancel_rect};
            return r[action-5];
        }
        if(action>=9 && action<17 && u->left_pane_section==APP_LEFT_PANE_SECTION_PINS && u->pin_editor_has_draft)
            return app_pin_panel_metadata_rect(app,action-9);
    }
    if(domain==CARTA_CONTROL_HEADER) {
        if(action<2){SDL_FRect r=app_header_button_rect(app);r.w*=.5f;r.x+=action*r.w;return r;}
        if(action==2)return u->header_zoom_toggle_rect;
        if(action>=10 && action<10+TILE_LAYER_COUNT)return u->header_layer_toggle_rects[action-10];
        if(action>=40 && action<40+TILE_LAYER_COUNT)return u->header_layer_label_rects[action-40];
    }
    if(domain==CARTA_CONTROL_ROUTE_PANEL &&
       (app->route_state_bridge.route.path.count>=2 || app->route_state_bridge.route.alternatives.count)) {
        if(action==0)return u->hud_route_panel_collapsed?u->hud_route_panel_handle_rect:u->hud_route_panel_collapse_rect;
        if(!u->hud_route_panel_collapsed) {
            if(action>=10 && action<10+app->route_state_bridge.route.alternatives.count && action<10+ROUTE_ALTERNATIVE_MAX) {
                SDL_FRect row=u->hud_route_panel_row_rects[action-10];
                SDL_FRect toggle=u->hud_route_panel_toggle_rects[action-10];
                if(toggle.w>0 && toggle.x>=row.x && toggle.x<row.x+row.w)row.w=toggle.x-row.x;
                return row;
            }
            if(action>=40 && action<40+app->route_state_bridge.route.alternatives.count && action<40+ROUTE_ALTERNATIVE_MAX)return u->hud_route_panel_toggle_rects[action-40];
        }
    }
    return (SDL_FRect){0};
}
void carta_remaining_control_activate(AppState *app,uint32_t domain,unsigned action,uint32_t modifiers) {
    AppUiState *u=&app->ui_state_bridge;
    if(domain==CARTA_CONTROL_PREPARE) {
        if(action==0)app_region_transition_cancel(app);
    } else if(domain==CARTA_CONTROL_PANEL) {
        if(action<2) {u->left_pane_open=action==0;if(action==1){app_pin_panel_name_edit_deactivate(app);app->ingest_edit_mode=false;carta_text_end(&u->rollout.ingest_root);}app_pin_panel_cancel_list_drag(app);app_pin_panel_layout(app);app_tile_viewport_invalidate(app);}
        else if(action<5) {
            app_pin_panel_cancel_list_drag(app);app_pin_panel_name_edit_deactivate(app);
            app->ingest_edit_mode=false;carta_text_end(&u->rollout.ingest_root);
            u->left_pane_section=(AppLeftPaneSection)(action-2);
            if(u->left_pane_section==APP_LEFT_PANE_SECTION_INGEST) {
                app->ingest_panel_open=true;u->hud_ingest_panel_collapsed=false;
                app_ingest_rescan_sources(app);app_ingest_rescan_active_regions(app);
            }
            app_pin_panel_layout(app);
        } else if(action<9)app_pin_panel_activate_action(app,action-5);
        else (void)app_pin_panel_activate_metadata(app,action-9);
    } else if(domain==CARTA_CONTROL_HEADER) {
        if(action<2) {
            RouteTravelMode mode=action==0?ROUTE_MODE_CAR:ROUTE_MODE_WALK;
            if(app->route_state_bridge.route.mode!=mode) {
                app->route_state_bridge.route.mode=mode;
                if(app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal)app_route_schedule_recompute(app,0);
            }
        } else if(action==2) {app->view_state_bridge.zoom_logic_enabled=!app->view_state_bridge.zoom_logic_enabled;app_refresh_layer_states(app);}
        else {
            unsigned kind=action>=40?action-40:action-10;
            if(kind>=TILE_LAYER_COUNT)return;
            if(action>=40 && (modifiers&(KIT_UI_INTERACTION_MOD_SHIFT|KIT_UI_INTERACTION_MOD_ALT))) {
                u->header_layer_selected_valid=true;u->header_layer_selected_kind=(TileLayerKind)kind;
                u->header_layer_panel_mode=(modifiers&KIT_UI_INTERACTION_MOD_SHIFT)?1:2;
            } else {
                app->view_state_bridge.layer_user_enabled[kind]=!app->view_state_bridge.layer_user_enabled[kind];app_refresh_layer_states(app);
                if(action>=40 && u->header_layer_selected_kind==(TileLayerKind)kind){u->header_layer_selected_valid=false;u->header_layer_panel_mode=0;}
            }
        }
    } else if(domain==CARTA_CONTROL_ROUTE_PANEL) {
        if(action==0)u->hud_route_panel_collapsed=!u->hud_route_panel_collapsed;
        else if(action>=40)app_route_service_toggle_alternative_visibility(app,action-40);
        else if(action>=10)app_route_service_select_alternative(app,action-10);
    }
    app->runtime_redraw_requested=true;
}
