#include "ui/rollout.h"
#include "app/app_internal.h"
#include "app/app_pin_panel_internal.h"
#include "app/app_place_tools.h"
#include "app/history_ui.h"
// The product retains draft save and ingest Apply; shared editing owns bytes.
bool carta_text_hosts_event(AppState *app,const SDL_Event *event) {
    if(!app || !event || app_place_tools_active() || history_ui_modal() ||
       app->region_prepare || app->ui_state_bridge.workspace_authoring.active)return false;
    AppUiState *ui=&app->ui_state_bridge;
    CartaTextInput *owner=NULL;
    if(ui->left_pane_open && ui->left_pane_section==APP_LEFT_PANE_SECTION_PINS &&
       ui->pin_editor_has_draft && ui->pin_name_edit_active) {
        owner=&ui->rollout.pin_name;
        carta_text_bind(owner,ui->pin_editor_name_edit,sizeof(ui->pin_editor_name_edit),false);
    } else if(ui->left_pane_open && ui->left_pane_section==APP_LEFT_PANE_SECTION_INGEST &&
              app->ingest_edit_mode) {
        owner=&ui->rollout.ingest_root;
        carta_text_bind(owner,app->input_root_edit,sizeof(app->input_root_edit),false);
    }
    if(!owner)return false;
    if(event->type==SDL_KEYDOWN)kit_ui_surface_set_scope(&ui->controls,0);
    KitUiTextEventResult result=carta_text_event(owner,event);
    if(owner==&ui->rollout.pin_name) {
        ui->pin_name_cursor_index=(int)owner->edit.cursor;
        if(result.submit)(void)app_pin_editor_save_draft(app);
        if(result.cancel)app_pin_panel_name_edit_deactivate(app);
    } else {
        if(result.submit)ui->input.enter_pressed=true;
        if(result.cancel){app->ingest_edit_mode=false;carta_text_end(owner);}
    }
    if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_TAB) {
        if(owner==&ui->rollout.pin_name)app_pin_panel_name_edit_deactivate(app);
        else {app->ingest_edit_mode=false;carta_text_end(owner);}
        return true;
    }
    return result.consumed!=0;
}
