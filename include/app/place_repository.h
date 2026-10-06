#ifndef MAPFORGE_PLACE_REPOSITORY_H
#define MAPFORGE_PLACE_REPOSITORY_H
#include "app/app_pins.h"
#include <json-c/json.h>
// Recognizes the app-owned SQLite workspace extension.
bool map_forge_places_is_database(const char *path);
// Loads a validated snapshot from the transactional personal repository.
bool map_forge_places_load(const char *path, MapForgePinsFile *out, char *error, size_t capacity);
// Replaces the document and relational indexes atomically with a stale-write guard.
bool map_forge_places_save_document(const char *path, struct json_object *root, uint64_t expected,
                                    char *error, size_t capacity);
// Commits one native pin edit/delete without changing caller state on a refused write.
bool map_forge_places_commit_pin(const char *path, MapForgePinsFile *file, const MapForgePin *pin,
                                 bool remove, char *error, size_t capacity);
// Acquires a nonblocking namespace owner lock; the caller closes the returned descriptor.
int map_forge_places_lock(const char *path);
// Executes the common UI-free command boundary against loaded personal records.
struct json_object *map_forge_places_command(const char *path, MapForgePinsFile *file,
                                             struct json_object *request);
#endif
