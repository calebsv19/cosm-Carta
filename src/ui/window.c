#include "ui/rollout.h"
#include "app/app_internal.h"
#include "app/app_pin_panel_internal.h"
#include "app/history_ui.h"
#include <string.h>
// Native transitions cancel transient ownership without altering accepted drafts.
static void cancel(AppState *app) {
    AppUiState *ui=&app->ui_state_bridge;
    history_ui_cancel_transient();
    kit_ui_surface_set_scope(&ui->controls,0);
    memset(&ui->controls_focus,0,sizeof(ui->controls_focus));
    kit_pane_splitter_interaction_end_drag(&ui->rollout.splitter);
    (void)kit_pane_pointer_route(&ui->rollout.pointer,NULL,0,0,0,0,1);
    ui->rollout.splitter.hover_active=0;
    app_pin_panel_cancel_list_drag(app);
    ui->header_layer_opacity_dragging=false;ui->header_layer_fade_drag_target=0;
    app->route_state_bridge.dragging_start=false;app->route_state_bridge.dragging_goal=false;
    ui->input.mouse_buttons=0;ui->input.mouse_dx=ui->input.mouse_dy=0;
    ui->input.pan_left=ui->input.pan_right=ui->input.pan_up=ui->input.pan_down=false;
    if(ui->native_text_owner)kit_ui_text_cancel_composition(&ui->native_text_owner->edit);
}
bool carta_window_refresh(AppState *app) {
    if(!app || !app->window)return true;
    CartaUiRollout *r=&app->ui_state_bridge.rollout;uint32_t changes=0;
    if(!r->window_initialized) {
        SDL_SetWindowMinimumSize(app->window,320,240);
        kit_ui_window_probe_init_sdl(&r->probe,"carta");r->window_initialized=true;
    }
    if(kit_ui_window_refresh_sdl(&r->window,app->window,&changes).code!=CORE_OK)return false;
    if(changes) {
        cancel(app);
        if(r->window.presentable && (changes&(KIT_UI_WINDOW_GEOMETRY|KIT_UI_WINDOW_MODE|KIT_UI_WINDOW_DISPLAY))) {
            (void)app_runtime_apply_window_size(app,r->window.logical_width,r->window.logical_height);
        }
        app->runtime_redraw_requested=true;
    }
    return r->window.presentable!=0;
}
bool carta_window_event(AppState *app,const SDL_Event *event) {
    if(!app || !event)return false;
    if(kit_ui_window_event_invalidates_sdl(event))cancel(app);
    if(event->type==SDL_WINDOWEVENT)(void)carta_window_refresh(app);
    if(app->ui_state_bridge.rollout.window_initialized && !app->ui_state_bridge.rollout.window.presentable &&
       (event->type==SDL_MOUSEMOTION || event->type==SDL_MOUSEBUTTONDOWN || event->type==SDL_MOUSEBUTTONUP ||
        event->type==SDL_MOUSEWHEEL || event->type==SDL_KEYDOWN || event->type==SDL_KEYUP ||
        event->type==SDL_TEXTINPUT || event->type==SDL_TEXTEDITING))return true;
    int mode=kit_ui_window_fullscreen_key_sdl(app->window,event);
    if(mode) {cancel(app);app->runtime_redraw_requested=true;(void)carta_window_refresh(app);return true;}
    return false;
}
