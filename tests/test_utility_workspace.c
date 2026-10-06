#include "app/app_utility_workspace.h"
#include "app/place_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Verifies navigation consumption, collection indexing, responsive hit areas and draft preservation.
int main(void) {
    AppState app = {0};
    app.width = 1000;
    MapForgePin places[3] = {{0}};
    places[0].collection_mask = 1;
    places[1].collection_mask = 2;
    places[2].collection_mask = 8 | 16;
    app.pins_file.pins = places;
    app.pins_file.pin_count = 3;
    strcpy(app.ui_state_bridge.pin_editor_draft.name, "Unsaved draft");
    app.ui_state_bridge.pin_editor_has_draft = true;
    app.ui_state_bridge.pin_name_cursor_index = 5;
    SDL_Event event = {0};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    SDL_FRect rect = app_utility_workspace_rect(&app, 1);
    event.button.x = (int)rect.x + 5;
    event.button.y = (int)rect.y + 5;
    assert(app_utility_workspace_handle_event(&app, &event));
    assert(app.ui_state_bridge.utility_workspace == 1);
    assert(app.ui_state_bridge.left_pane_open);
    assert(app_utility_pin_count(&app) == 1);
    assert(app_utility_pin_index(&app, 0) == 1);
    app.ui_state_bridge.service_filter = 4;
    assert(app_utility_pin_count(&app) == 0);
    app.ui_state_bridge.service_filter = 2;
    assert(app_utility_pin_count(&app) == 1);
    app_utility_workspace_select(&app, 2);
    assert(app.ui_state_bridge.service_filter == 0);
    assert(app_utility_pin_index(&app, 0) == 2);
    assert(strcmp(app.ui_state_bridge.pin_editor_draft.name, "Unsaved draft") == 0 &&
           app.ui_state_bridge.pin_name_cursor_index == 5);
    app_utility_workspace_select(&app, 3);
    assert(app.ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_INGEST);
    app_utility_workspace_select(&app, 0);
    assert(app_utility_pin_count(&app) == 3);
    strcpy(places[1].id, "service-place");
    strcpy(places[1].address, "100 Example Street");
    app.pins_file.source_json = json_tokener_parse(
        "{\"pets\":[{\"id\":\"pet1\",\"name\":\"Miso\"}],\"pet_places\":[{\"place_id\":\"service-place\","
        "\"pet_id\":\"pet1\"}],\"services\":[{\"place_id\":\"service-place\",\"kind\":\"pet_sitting\"}]}");
    strcpy(app.ui_state_bridge.place_search, "miso");
    assert(app_utility_pin_count(&app) == 1 && app_utility_pin_index(&app, 0) == 1);
    strcpy(app.ui_state_bridge.place_search, "pet_sitting");
    assert(app_utility_pin_count(&app) == 1);
    strcpy(app.ui_state_bridge.place_search, "Example Street");
    assert(app_utility_pin_count(&app) == 1);
    strcpy(app.ui_state_bridge.place_search, "absent");
    assert(app_utility_pin_count(&app) == 0);
    app.ui_state_bridge.place_search[0] = 0;
    json_object_put(app.pins_file.source_json);
    app.pins_file.source_json = NULL;
    assert(app_places_has_unsaved_draft(&app));
    app.ui_state_bridge.pin_editor_draft = places[1];
    app.ui_state_bridge.pin_editor_name_edit[0] = 0;
    assert(!app_places_has_unsaved_draft(&app));
    app.ui_state_bridge.pin_editor_draft.private_flag = !places[1].private_flag;
    assert(app_places_has_unsaved_draft(&app));
    app.ui_state_bridge.pin_editor_draft = places[1];
    strcpy(app.ui_state_bridge.pin_editor_name_edit, "unsaved name");
    assert(app_places_has_unsaved_draft(&app));
    app.ui_state_bridge.pin_editor_name_edit[0] = 0;
    assert(!app_places_has_unsaved_draft(&app));
    event.button.y = 100;
    assert(!app_utility_workspace_handle_event(&app, &event));
    app.width = 500;
    rect = app_utility_workspace_rect(&app, 3);
    assert(rect.x + rect.w <= 500);
    puts("utility workspace: navigation, filters, draft preservation and input consumption passed");
    return 0;
}
