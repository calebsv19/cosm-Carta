#include "ui/control_surface.h"
#include "app/app_ui_internal.h"
#include "app/app_persist_state.h"
#include "app/app_utility_workspace.h"
#include "app/app_place_tools.h"
#include "app/history_ui.h"
#include "kit_ui_interaction_sdl.h"
#include "ui/font.h"
#include <math.h>
#include <string.h>

// Keeps modal, authoring and staging ownership above the background controls.
static bool blocked(const AppState *app) {
    return app->region_prepare || app_place_tools_active() || history_ui_modal() ||
           app->ui_state_bridge.workspace_authoring.active || app->ui_state_bridge.route_plan_name_active;
}
// Registers only drawable controls, intersected with the current window.
static void add(KitUiSurface *s, uint32_t domain, uint64_t action, SDL_FRect r,
                KitRenderRect clip) {
    if (r.w <= 0 || r.h <= 0) return;
    (void)kit_ui_surface_register(s, (KitUiSurfaceKey){domain,action},
        (KitRenderRect){r.x,r.y,r.w,r.h}, &clip, 1, NULL);
}
void carta_ui_controls_sync(AppState *app) {
    if (!app) return;
    KitUiSurface *s = &app->ui_state_bridge.controls;
    bool unavailable = blocked(app);
    (void)kit_ui_focus_scope_sync(&app->ui_state_bridge.controls_focus,s,unavailable?2u:1u,unavailable);
    kit_ui_surface_begin(s, unavailable ? 2u : 1u);
    KitRenderRect clip = {0,0,app->width,app->height};
    if (!unavailable && clip.width > 0 && clip.height > 0) {
        for (unsigned i=0;i<4;i++) add(s,CARTA_CONTROL_WORKSPACE,i,
                                      app_utility_workspace_rect(app,i),clip);
        for(unsigned i=0;i<9;i++)add(s,CARTA_CONTROL_PLACES,i,carta_places_control_rect(app,CARTA_CONTROL_PLACES,i),clip);
        for(unsigned i=0;i<2;i++)add(s,CARTA_CONTROL_ROUTES,i,carta_places_control_rect(app,CARTA_CONTROL_ROUTES,i),clip);
        for(unsigned i=0;i<17;i++)add(s,CARTA_CONTROL_PANEL,i,carta_remaining_control_rect(app,CARTA_CONTROL_PANEL,i),clip);
        for(unsigned i=0;i<40+TILE_LAYER_COUNT;i++)add(s,CARTA_CONTROL_HEADER,i,carta_remaining_control_rect(app,CARTA_CONTROL_HEADER,i),clip);
        for(unsigned i=0;i<40+ROUTE_ALTERNATIVE_MAX;i++)add(s,CARTA_CONTROL_ROUTE_PANEL,i,carta_remaining_control_rect(app,CARTA_CONTROL_ROUTE_PANEL,i),clip);
        if (app->ui_state_bridge.utility_workspace == APP_UTILITY_HIKING_HISTORY)
            for (unsigned i=0;i<32;i++) add(s,CARTA_CONTROL_HISTORY,i,
                                           history_ui_control_rect(app,i),clip);
    }
    if(unavailable && history_ui_modal())
        for(unsigned i=0;i<3;i++)add(s,CARTA_CONTROL_HISTORY_FORM,i,history_ui_form_control_rect(app,i),clip);
    if(unavailable && app_place_tools_active())
        for(unsigned i=0;i<app_place_tools_control_count();i++)
            add(s,CARTA_CONTROL_PLACE_FORM,app_place_tools_control_key(i),app_place_tools_control_rect(i),clip);
    HistoryViewSelection bindings[3]={{0}};
    if(app->ui_state_bridge.utility_workspace==APP_UTILITY_HIKING_HISTORY)
        for(unsigned i=0;i<3 && i<app->ui_state_bridge.history_views.count;i++) {
            bindings[i].filter=app->ui_state_bridge.history_views.items[i].filter;
            SDL_strlcpy(bindings[i].collection,app->ui_state_bridge.history_views.items[i].collection,sizeof(bindings[i].collection));
        }
    bool bindings_changed=memcmp(bindings,app->ui_state_bridge.rollout.history_bindings,sizeof(bindings))!=0;
    memcpy(app->ui_state_bridge.rollout.history_bindings,bindings,sizeof(bindings));
    uint64_t route_bindings=app->route_state_bridge.route.alternatives.count;
    for(unsigned i=0;i<app->route_state_bridge.route.alternatives.count && i<ROUTE_ALTERNATIVE_MAX;i++)
        route_bindings=route_bindings*31u+1u+(unsigned)app->route_state_bridge.route.alternatives.objectives[i];
    CartaUiRollout *rollout=&app->ui_state_bridge.rollout;
    bool target_changed=strcmp(rollout->pin_target,app->ui_state_bridge.pin_editor_draft.id) ||
        rollout->store_signature!=app->pins_file.source_signature || rollout->route_bindings!=route_bindings;
    SDL_strlcpy(rollout->pin_target,app->ui_state_bridge.pin_editor_draft.id,sizeof(rollout->pin_target));
    rollout->store_signature=app->pins_file.source_signature;rollout->route_bindings=route_bindings;
    bool changed = target_changed || bindings_changed || s->count != s->next_count ||
        app->ui_state_bridge.controls_width!=app->width ||
        app->ui_state_bridge.controls_height!=app->height ||
        app->ui_state_bridge.controls_workspace != app->ui_state_bridge.utility_workspace ||
        strcmp(app->ui_state_bridge.controls_selection,app->ui_state_bridge.history_selected_id);
    for (unsigned i=0;!changed && i<s->count;i++)
        changed = memcmp(&s->controls[i],&s->next_controls[i],sizeof(s->controls[i])) != 0;
    if (changed) {
        KitUiInteractionEvent cancel = {.type=KIT_UI_INTERACTION_SYNC};
        KitUiInteractionResult result;
        (void)kit_ui_interaction_route(&s->interaction,NULL,0,&cancel,&result);
        s->activation_count=0;
    }
    if (kit_ui_surface_end(s).code != CORE_OK) kit_ui_surface_set_scope(s,0);
    kit_ui_focus_scope_restore(&app->ui_state_bridge.controls_focus,s);
    app->ui_state_bridge.controls_width=app->width;
    app->ui_state_bridge.controls_height=app->height;
    app->ui_state_bridge.controls_workspace=app->ui_state_bridge.utility_workspace;
    SDL_strlcpy(app->ui_state_bridge.controls_selection,
                app->ui_state_bridge.history_selected_id,64);
}
bool carta_ui_controls_event(AppState *app, const SDL_Event *event) {
    if (!app || !event) return false;
    carta_ui_controls_sync(app);
    KitUiSurface *s = &app->ui_state_bridge.controls;
    KitUiInteractionEvent input;
    if (!kit_ui_interaction_event_from_sdl(event,&input)) return false;
    if(input.type==KIT_UI_INTERACTION_POINTER_DOWN || input.type==KIT_UI_INTERACTION_POINTER_UP) {
        SDL_Keymod mods=SDL_GetModState();
        if(mods&KMOD_SHIFT)input.modifiers|=KIT_UI_INTERACTION_MOD_SHIFT;
        if(mods&KMOD_ALT)input.modifiers|=KIT_UI_INTERACTION_MOD_ALT;
        if(mods&KMOD_CTRL)input.modifiers|=KIT_UI_INTERACTION_MOD_CTRL;
        if(mods&KMOD_GUI)input.modifiers|=KIT_UI_INTERACTION_MOD_GUI;
    }
    if (input.type==KIT_UI_INTERACTION_CANCEL) {
        // Forget restoration without resetting the surface's outstanding release guard.
        memset(&app->ui_state_bridge.controls_focus,0,sizeof(app->ui_state_bridge.controls_focus));
        kit_ui_surface_set_scope(s,0);
        return false; /* Window/quit events still reach host lifecycle handling. */
    }
    // Existing search editing retains its keys; an armed button cannot submit it.
    if ((app->ui_state_bridge.place_search_active || app->ui_state_bridge.route_plan_name_active || app->ui_state_bridge.pin_name_edit_active || app->ingest_edit_mode || app_place_tools_text_active()) &&
        (input.type==KIT_UI_INTERACTION_KEY_DOWN || input.type==KIT_UI_INTERACTION_KEY_UP)) {
        kit_ui_surface_set_scope(s,0);
        return false;
    }
    if(history_ui_modal() && (input.type==KIT_UI_INTERACTION_KEY_DOWN || input.type==KIT_UI_INTERACTION_KEY_UP))return false;
    KitUiInteractionResult result;
    if (kit_ui_surface_route(s,&input,&result).code != CORE_OK) return false;
    if (result.activated_id) {
        KitUiSurfaceKey key;
        if (kit_ui_surface_key(s,result.activated_id,&key) &&
            kit_ui_surface_take_activation(s,result.activated_id)) {
            if (key.domain==CARTA_CONTROL_WORKSPACE && key.value<4) {
                app_utility_workspace_select(app,(uint32_t)key.value);
                if (app->lifetime.persisted_state_ready) app_save_persisted_view_state(app);
            } else if (key.domain==CARTA_CONTROL_HISTORY)
                history_ui_activate(app,(unsigned)key.value);
            else if(key.domain==CARTA_CONTROL_PLACES || key.domain==CARTA_CONTROL_ROUTES)
                carta_places_control_activate(app,key.domain,key.value);
            else if(key.domain==CARTA_CONTROL_HISTORY_FORM)history_ui_form_control_activate(app,(unsigned)key.value);
            else if(key.domain==CARTA_CONTROL_PLACE_FORM)app_place_tools_control_activate(app,key.value);
            else carta_remaining_control_activate(app,key.domain,(unsigned)key.value,input.modifiers);
            carta_ui_controls_sync(app);
        }
    }
    // Modal key-down belongs to the modal. Outstanding releases stay swallowed.
    if (blocked(app) && !s->count && input.type!=KIT_UI_INTERACTION_POINTER_UP &&
        input.type!=KIT_UI_INTERACTION_KEY_UP && input.type!=KIT_UI_INTERACTION_CANCEL)
        return false;
    return result.consumed != 0;
}
