#include "app/app_internal.h"
#include "app/app_pin_panel_internal.h"
#include "app/app_runtime_ingest_internal.h"
// Runs accepted commands in the current section; persistence remains in product bodies.
void app_pin_panel_activate_action(AppState *app,unsigned action) {
    if(app->ui_state_bridge.left_pane_section==APP_LEFT_PANE_SECTION_INGEST) {
        if(action==0) {if(app->ingest_show_active_tab)(void)app_ingest_open_selected_active_region(app);else (void)app_ingest_open_or_import_selected_source(app);}
        if(action==1){if(app->ingest_show_active_tab)app->ingest_show_active_tab=false;else (void)app_ingest_import_selected_osm(app,true);}
        if(action==2)(void)app_ingest_pick_source_file(app);
        if(action==3){app_ingest_rescan_sources(app);app_ingest_rescan_active_regions(app);}
    } else if(app->ui_state_bridge.left_pane_section==APP_LEFT_PANE_SECTION_PINS) {
        if(action==0) {
            if(app->ui_state_bridge.pin_add_mode_active) {
                app->ui_state_bridge.pin_add_mode_active=false;app->ui_state_bridge.pin_editor_waiting_for_map_click=false;
                if(app->ui_state_bridge.pin_selected_index>=0 && app->ui_state_bridge.pin_selected_index<(int)app->pins_file.pin_count)
                    app_pin_editor_select_saved_pin(app,app->ui_state_bridge.pin_selected_index);
            } else app_pin_editor_begin_new_pin(app);
        }
        if(action==1)(void)app_pin_editor_save_draft(app);
        if(action==2)(void)app_pin_editor_delete_selected(app);
        if(action==3)app_pin_editor_cancel_draft(app);
    }
}
