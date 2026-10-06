#ifndef MAPFORGE_PLACE_ROUTES_H
#define MAPFORGE_PLACE_ROUTES_H
#include "app/place_repository.h"
// Lists, previews, saves or reopens route intent referencing stable place identities.
struct json_object *map_forge_place_route_command(const char *path, MapForgePinsFile *file,
                                                  struct json_object *request);
#endif
