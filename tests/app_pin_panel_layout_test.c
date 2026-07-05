#include "app/app_internal.h"
#include "app/app_pin_panel_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_open_pin_pane_stacks_editor_above_list(void) {
    AppState app;
    memset(&app, 0, sizeof(app));

    app.width = 1280;
    app.height = 720;
    app.ui_state_bridge.left_pane_open = true;
    app.ui_state_bridge.left_pane_section = APP_LEFT_PANE_SECTION_PINS;
    app.ui_state_bridge.pin_editor_has_draft = true;
    app.pins_file.pin_count = 4u;

    app_pin_panel_layout(&app);

    assert(app.ui_state_bridge.left_pane_rect.w > 0.0f);
    assert(app.ui_state_bridge.pin_pane_status_rect.h > 0.0f);
    assert(app.ui_state_bridge.pin_pane_hint_rect.h > 0.0f);
    assert(app.ui_state_bridge.pin_pane_list_rect.h > 0.0f);
    assert(app.ui_state_bridge.pin_pane_status_rect.y >=
           app.ui_state_bridge.pin_pane_hint_rect.y + app.ui_state_bridge.pin_pane_hint_rect.h);
    assert(app.ui_state_bridge.pin_pane_list_rect.y >=
           app.ui_state_bridge.pin_pane_status_rect.y + app.ui_state_bridge.pin_pane_status_rect.h);
}

static void test_ingest_pane_has_controls_above_list(void) {
    AppState app;
    memset(&app, 0, sizeof(app));

    app.width = 1280;
    app.height = 720;
    app.ui_state_bridge.left_pane_open = true;
    app.ui_state_bridge.left_pane_section = APP_LEFT_PANE_SECTION_INGEST;
    app.ingest_osm_count = 3;

    app_pin_panel_layout(&app);

    assert(app.ui_state_bridge.left_pane_rect.w > 0.0f);
    assert(app.ui_state_bridge.pin_pane_add_rect.h > 0.0f);
    assert(app.ui_state_bridge.pin_pane_save_rect.h > 0.0f);
    assert(app.ui_state_bridge.pin_pane_delete_rect.h > 0.0f);
    assert(app.ui_state_bridge.pin_pane_cancel_rect.h > 0.0f);
    assert(app.ui_state_bridge.pin_pane_list_rect.y >=
           app.ui_state_bridge.pin_pane_add_rect.y + app.ui_state_bridge.pin_pane_add_rect.h);
    assert(app.ui_state_bridge.pin_pane_row_count == 3);
}

int main(void) {
    test_open_pin_pane_stacks_editor_above_list();
    test_ingest_pane_has_controls_above_list();
    printf("app_pin_panel_layout_test: success\n");
    return 0;
}
