#ifndef MAPFORGE_PLACE_MAINTENANCE_H
#define MAPFORGE_PLACE_MAINTENANCE_H
#include "app/place_repository.h"
// Creates private recovery/export snapshots or previews and commits a guarded restore.
struct json_object *map_forge_place_maintenance_command(const char *path, MapForgePinsFile *file,
                                                        struct json_object *request);
// Applies explicit pet associations and service roles inside an existing guarded place edit.
bool map_forge_place_profile_apply(MapForgePinsFile *file, MapForgePin *pin, struct json_object *profile,
                                   struct json_object *request, char *error, size_t capacity);
#endif
