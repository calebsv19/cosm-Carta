#include "app/app_pins.h"
#include "app/place_repository.h"
#include "core_io.h"
#include <json-c/json.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Resolves a personal workspace file that survives removal of any map pack.
bool map_forge_pins_workspace_path(char *out, size_t capacity) {
    const char *runtime = getenv("MAPFORGE_RUNTIME_DIR");
    int n = runtime && runtime[0] ? snprintf(out, capacity, "%s/pins/places.sqlite", runtime)
        : snprintf(out, capacity, "data/pins/private/places.sqlite");
    return n > 0 && (size_t)n < capacity;
}

// Merges legacy files by stable ID, preserving extension metadata and refusing conflicts.
static bool workspace_merge_directory(const char *directory, MapForgePinsFile *store, bool *migrated, char *error, size_t capacity) {
    DIR *dir = opendir(directory);
    if (!dir) return true;
    struct dirent *entry;
    bool ok = true;
    struct json_object *records = NULL, *sources = NULL;
    json_object_object_get_ex(store->source_json, "pins", &records);
    json_object_object_get_ex(store->source_json, "migration_sources", &sources);
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name), suffix = strlen(".pins.local.json");
        if (len <= suffix || strcmp(entry->d_name + len - suffix, ".pins.local.json") != 0) continue;
        char path[MAPFORGE_PIN_PATH_CAPACITY];
        int n = snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (n <= 0 || (size_t)n >= sizeof(path)) { ok = false; break; }
        MapForgePinsFile legacy; map_forge_pins_file_init(&legacy);
        if (!map_forge_pins_load(path, &legacy, error, capacity)) { map_forge_pins_file_free(&legacy); ok = false; break; }
        struct json_object *old_records = NULL;
        json_object_object_get_ex(legacy.source_json, "pins", &old_records);
        for (size_t i = 0u; i < legacy.pin_count; ++i) {
            MapForgePin *old = map_forge_pins_find_by_id(store, legacy.pins[i].id);
            if (old) {
                struct json_object *existing = NULL;
                for (size_t j = 0u; j < json_object_array_length(records); ++j) {
                    struct json_object *candidate = json_object_array_get_idx(records, j), *id = NULL;
                    if (json_object_object_get_ex(candidate, "id", &id) && strcmp(json_object_get_string(id), old->id) == 0) existing = candidate;
                }
                if (!existing || !json_object_equal(existing, json_object_array_get_idx(old_records, i))) {
                    if (error && capacity) snprintf(error, capacity, "Conflicting legacy pin ID; originals preserved. Resolve conflict before workspace migration.");
                    ok = false; break;
                }
            } else {
                if (!map_forge_pins_upsert(store, &legacy.pins[i], error, capacity)) { ok = false; break; }
                json_object_array_add(records, json_object_get(json_object_array_get_idx(old_records, i)));
            }
        }
        json_object_array_add(sources, json_object_get(legacy.source_json));
        *migrated = true;
        map_forge_pins_file_free(&legacy);
        if (!ok) break;
    }
    closedir(dir);
    return ok;
}

// Loads the independent store or atomically migrates legacy personal files without altering originals.
bool map_forge_pins_load_workspace(MapForgePinsFile *out, char *path, size_t path_capacity, char *error, size_t error_capacity) {
    if (!out || !path || !map_forge_pins_workspace_path(path, path_capacity)) return false;
    if (core_io_path_exists(path)) return map_forge_pins_load(path, out, error, error_capacity);
    char legacy_path[MAPFORGE_PIN_PATH_CAPACITY];
    snprintf(legacy_path, sizeof(legacy_path), "%s", path);
    char *extension = strrchr(legacy_path, '.');
    if (!extension || (size_t)(extension - legacy_path) + strlen(".local.json") >= sizeof(legacy_path)) return false;
    strcpy(extension, ".local.json");
    if (core_io_path_exists(legacy_path)) {
        MapForgePinsFile legacy; map_forge_pins_file_init(&legacy);
        bool ok = map_forge_pins_load(legacy_path, &legacy, error, error_capacity);
        legacy.source_signature = 0u;
        if (ok) ok = map_forge_pins_save(path, &legacy, error, error_capacity);
        map_forge_pins_file_free(&legacy);
        return ok && map_forge_pins_load(path, out, error, error_capacity);
    }
    MapForgePinsFile store; map_forge_pins_file_init(&store);
    store.source_json = json_object_new_object();
    json_object_object_add(store.source_json, "pins", json_object_new_array());
    json_object_object_add(store.source_json, "migration_sources", json_object_new_array());
    bool migrated = false;
    char directory[MAPFORGE_PIN_PATH_CAPACITY];
    snprintf(directory, sizeof(directory), "%s", path);
    char *slash = strrchr(directory, '/'); if (slash) *slash = 0;
    bool ok = workspace_merge_directory(directory, &store, &migrated, error, error_capacity);
    if (ok && strcmp(directory, "data/pins/private") != 0) ok = workspace_merge_directory("data/pins/private", &store, &migrated, error, error_capacity);
    (void)migrated;
    if (ok) ok = map_forge_pins_save(path, &store, error, error_capacity);
    if (ok) ok = map_forge_pins_load(path, &store, error, error_capacity);
    if (!ok) { map_forge_pins_file_free(&store); return false; }
    map_forge_pins_file_free(out); *out = store;
    return true;
}
