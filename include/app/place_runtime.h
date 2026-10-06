#ifndef MAPFORGE_PLACE_RUNTIME_H
#define MAPFORGE_PLACE_RUNTIME_H
#include "app/app_internal.h"
#include <string.h>
// Distinguishes a clean saved selection from actual unsaved editor changes.
static inline bool app_places_has_unsaved_draft(const AppState *app) {
    if (!app->ui_state_bridge.pin_editor_has_draft)
        return false;
    if (app->ui_state_bridge.pin_editor_is_new || app->ui_state_bridge.pin_editor_waiting_for_map_click ||
        app->ui_state_bridge.pin_name_edit_active)
        return true;
    const MapForgePin *draft = &app->ui_state_bridge.pin_editor_draft, *saved = NULL;
    for (size_t i = 0; i < app->pins_file.pin_count; ++i)
        if (!strcmp(app->pins_file.pins[i].id, draft->id)) {
            saved = &app->pins_file.pins[i];
            break;
        }
    if (!saved)
        return true;
    return strcmp(app->ui_state_bridge.pin_editor_name_edit, saved->name) ||
           strcmp(draft->name, saved->name) || draft->lat != saved->lat || draft->lon != saved->lon ||
           draft->private_flag != saved->private_flag || draft->collection_mask != saved->collection_mask ||
           strcmp(draft->notes, saved->notes) || strcmp(draft->type, saved->type) ||
           strcmp(draft->color, saved->color) || strcmp(draft->address, saved->address) ||
           strcmp(draft->location_source, saved->location_source) ||
           strcmp(draft->location_quality, saved->location_quality);
}
// Claims the native personal-repository owner before loading or editing records.
bool app_places_owner_acquire(void);
// Releases ownership only after the native app has finished saving.
void app_places_owner_release(void);
// Consumes service search and saved-route keyboard actions before map input.
bool app_places_handle_event(AppState *app, const SDL_Event *event);
// Returns the visible route save/open control geometry.
SDL_FRect app_places_route_button_rect(const AppState *app, uint32_t index);
// Invalidates active routes and late worker results after a location mutation.
void app_places_invalidate_route(AppState *app);
// Formats linked pet names and service roles for one selected place.
void app_places_detail_text(const AppState *app, const char *place_id, char *text, size_t capacity);
// Processes bounded local commands on the main thread without overwriting drafts.
void app_places_poll_commands(AppState *app);
#endif
