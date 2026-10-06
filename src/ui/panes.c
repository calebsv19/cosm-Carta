#include "ui/rollout.h"
#include "app/app_ui_internal.h"
#include "app/app_pin_panel_internal.h"
#include "app/app_persist_state.h"
#include "app/app_place_tools.h"
#include "app/history_ui.h"
#include <math.h>
#include <string.h>
static CorePaneRect bounds(const AppState *app) {return (CorePaneRect){0,APP_HEADER_HEIGHT,app->width,fmaxf(0,app->height-APP_HEADER_HEIGHT)};}
bool carta_panes_event(AppState *app,const SDL_Event *event) {
    if(!app || !event)return false;
    CartaUiRollout *r=&app->ui_state_bridge.rollout;
    bool blocked=app->region_prepare || app_place_tools_active() || history_ui_modal() ||
        app->ui_state_bridge.workspace_authoring.active || !app->ui_state_bridge.left_pane_open;
    if(blocked || event->type==SDL_QUIT || kit_ui_window_event_invalidates_sdl(event)) {
        kit_pane_splitter_interaction_end_drag(&r->splitter);r->splitter.hover_active=0;
        (void)kit_pane_pointer_route(&r->pointer,NULL,0,0,0,0,1);
        return false;
    }
    app_pin_panel_layout(app);
    if(event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT) {
        if(kit_pane_splitter_interaction_begin_drag(&r->splitter,r->nodes,3,0,bounds(app),event->button.x,event->button.y).code==CORE_OK) {
            (void)kit_pane_pointer_route(&r->pointer,NULL,0,0,0,0,1);
            app_pin_panel_cancel_list_drag(app);
            kit_ui_surface_set_scope(&app->ui_state_bridge.controls,0);
            if(app->ui_state_bridge.native_text_owner)kit_ui_text_cancel_composition(&app->ui_state_bridge.native_text_owner->edit);
            return true;
        }
    }
    if(event->type==SDL_MOUSEMOTION) {
        if(r->splitter.drag_active) {
            int changed=0;
            (void)kit_pane_splitter_interaction_update_drag(&r->splitter,r->nodes,3,event->motion.x,event->motion.y,&changed);
            if(changed) {
                r->sidebar_ratio=r->nodes[0].ratio_01;app_pin_panel_layout(app);
                app_tile_viewport_invalidate(app);app->runtime_redraw_requested=true;
            }
            return true;
        }
        (void)kit_pane_splitter_interaction_set_hover(&r->splitter,r->nodes,3,0,bounds(app),event->motion.x,event->motion.y);
    }
    if(event->type==SDL_MOUSEBUTTONUP && event->button.button==SDL_BUTTON_LEFT && r->splitter.drag_active) {
        kit_pane_splitter_interaction_end_drag(&r->splitter);
        if(app->lifetime.persisted_state_ready)app_save_persisted_view_state(app);
        return true;
    }
    if(event->type==SDL_MOUSEMOTION)
        (void)kit_pane_pointer_route(&r->pointer,&r->panes,event->motion.x,event->motion.y,0,0,0);
    if((event->type==SDL_MOUSEBUTTONDOWN || event->type==SDL_MOUSEBUTTONUP) && event->button.button==SDL_BUTTON_LEFT)
        (void)kit_pane_pointer_route(&r->pointer,&r->panes,event->button.x,event->button.y,event->type==SDL_MOUSEBUTTONDOWN,event->type==SDL_MOUSEBUTTONUP,0);
    return false;
}
void carta_panes_draw(AppState *app) {
    if(!app->ui_state_bridge.left_pane_open)return;
    const KitPaneCompositionEntry *entry=kit_pane_composition_find(&app->ui_state_bridge.rollout.panes,2);
    if(!entry)return;
    MapForgeThemePalette p=app_ui_theme_palette();
    SDL_Color c=app->ui_state_bridge.rollout.splitter.drag_active?p.overlay_accent:p.route_panel_outline;
    renderer_set_draw_color(&app->renderer,c.r,c.g,c.b,c.a);
    // Fourteen logical pixels for hit testing, one logical pixel for visible chrome.
    SDL_FRect line={entry->shell.x+entry->shell.width-1,entry->shell.y,1,entry->shell.height};
    renderer_fill_rect(&app->renderer,&line);
}
