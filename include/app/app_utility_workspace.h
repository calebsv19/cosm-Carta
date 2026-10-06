#ifndef MAPFORGE_APP_UTILITY_WORKSPACE_H
#define MAPFORGE_APP_UTILITY_WORKSPACE_H
#include "app/app_internal.h"
#include <json-c/json.h>
#include <ctype.h>
#include <string.h>

// Enumerates prepared map, service-place, journal-place and data workspaces.
typedef enum AppUtilityWorkspace {
    APP_UTILITY_MAP = 0,
    APP_UTILITY_DOG_WALKING = 1,
    APP_UTILITY_HIKING_HISTORY = 2,
    APP_UTILITY_DATA = 3
} AppUtilityWorkspace;

// Returns the responsive navigation hit/draw rectangle.
SDL_FRect app_utility_workspace_rect(const AppState *app, uint32_t index);

// Selects a prepared workspace while preserving the pending pin draft.
void app_utility_workspace_select(AppState *app, uint32_t workspace);
// Consumes top-level navigation clicks before map input intake.
bool app_utility_workspace_handle_event(AppState *app, const SDL_Event *event);
// Draws persistent navigation and the active collection view.
void app_utility_workspace_draw(AppState *app);

// Resolves the active collection filter without modifying the record store.
static inline uint32_t app_utility_collection_filter(const AppState *app) {
    if (app->ui_state_bridge.utility_workspace == 1u)
        return app->ui_state_bridge.service_filter ? app->ui_state_bridge.service_filter : 6u;
    if (app->ui_state_bridge.utility_workspace == 2u)
        return 24u;
    return 0u;
}
// Performs bounded local text matching for the service workspace.
static inline bool app_utility_text_matches(const char *text, const char *query) {
    if (!query[0])
        return true;
    size_t n = strlen(query);
    for (const char *p = text; *p; ++p) {
        size_t i = 0;
        while (i < n && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)query[i]))
            ++i;
        if (i == n)
            return true;
    }
    return false;
}
// Tests collection membership and pet/address/service search without mutating records.
static inline bool app_utility_pin_visible(const AppState *app, const MapForgePin *pin) {
    uint32_t filter = app_utility_collection_filter(app);
    if (filter && !((pin->collection_mask ? pin->collection_mask : 1u) & filter))
        return false;
    const char *query = app->ui_state_bridge.place_search;
    if (app_utility_text_matches(pin->name, query) || app_utility_text_matches(pin->address, query))
        return true;
    struct json_object *services = NULL, *links = NULL, *pets = NULL;
    if (!app->pins_file.source_json)
        return false;
    struct json_object *records = NULL;
    json_object_object_get_ex(app->pins_file.source_json, "pins", &records);
    for (size_t i = 0; records && i < json_object_array_length(records); ++i) {
        struct json_object *record = json_object_array_get_idx(records, i), *id = NULL, *evidence = NULL;
        if (!json_object_object_get_ex(record, "id", &id) || strcmp(json_object_get_string(id), pin->id) ||
            !json_object_object_get_ex(record, "location_evidence", &evidence)) continue;
        const char *keys[] = {"input_address", "matched_address"};
        for (unsigned j = 0; j < 2; ++j) {
            struct json_object *value = NULL;
            if (json_object_object_get_ex(evidence, keys[j], &value) && json_object_is_type(value, json_type_string) &&
                app_utility_text_matches(json_object_get_string(value), query)) return true;
        }
    }
    json_object_object_get_ex(app->pins_file.source_json, "services", &services);
    json_object_object_get_ex(app->pins_file.source_json, "pet_places", &links);
    json_object_object_get_ex(app->pins_file.source_json, "pets", &pets);
    for (size_t i = 0; services && i < json_object_array_length(services); ++i) {
        struct json_object *v = json_object_array_get_idx(services, i), *id = NULL, *kind = NULL;
        if (json_object_object_get_ex(v, "place_id", &id) && json_object_object_get_ex(v, "kind", &kind) &&
            !strcmp(json_object_get_string(id), pin->id) &&
            app_utility_text_matches(json_object_get_string(kind), query))
            return true;
    }
    for (size_t i = 0; links && i < json_object_array_length(links); ++i) {
        struct json_object *v = json_object_array_get_idx(links, i), *id = NULL, *pet = NULL;
        if (!json_object_object_get_ex(v, "place_id", &id) || strcmp(json_object_get_string(id), pin->id) ||
            !json_object_object_get_ex(v, "pet_id", &pet))
            continue;
        for (size_t j = 0; pets && j < json_object_array_length(pets); ++j) {
            struct json_object *p = json_object_array_get_idx(pets, j), *pid = NULL, *name = NULL;
            if (json_object_object_get_ex(p, "id", &pid) && json_object_object_get_ex(p, "name", &name) &&
                !strcmp(json_object_get_string(pid), json_object_get_string(pet)) &&
                app_utility_text_matches(json_object_get_string(name), query))
                return true;
        }
    }
    return false;
}
// Maps a visible list ordinal to its stable backing-store index.
static inline int app_utility_pin_index(const AppState *app, int ordinal) {
    if (app_utility_collection_filter(app) == 0u && !app->ui_state_bridge.place_search[0])
        return ordinal >= 0 && (size_t)ordinal < app->pins_file.pin_count ? ordinal : -1;
    for (size_t i = 0u; i < app->pins_file.pin_count; ++i) {
        if (app_utility_pin_visible(app, &app->pins_file.pins[i]) && ordinal-- == 0)
            return (int)i;
    }
    return -1;
}
// Counts visible places without changing their saved order.
static inline int app_utility_pin_count(const AppState *app) {
    if (app_utility_collection_filter(app) == 0u && !app->ui_state_bridge.place_search[0])
        return (int)app->pins_file.pin_count;
    int count = 0;
    for (size_t i = 0u; i < app->pins_file.pin_count; ++i)
        if (app_utility_pin_visible(app, &app->pins_file.pins[i]))
            ++count;
    return count;
}
#endif
