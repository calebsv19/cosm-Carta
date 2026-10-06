#ifndef MAPFORGE_APP_PLACE_TOOLS_INTERNAL_H
#define MAPFORGE_APP_PLACE_TOOLS_INTERNAL_H
#include "app/app_map_viewport_internal.h"
#include "app/app_place_tools.h"
#include "app/app_ui_internal.h"
#include "app/app_utility_workspace.h"
#include <json-c/json.h>
// Holds one bounded native dialog; canonical data remains owned by the repository.
typedef struct PlaceTools {
    unsigned kind;
    int scroll, field, cursor, route_page, selected, link_index;
    bool archived, linking;
    char status[512], edit[1024], source[MAPFORGE_PIN_PATH_CAPACITY], store_token[32];
    struct json_object *draft, *routes, *restore;
    SDL_FRect fields[68];
    unsigned field_count;
    CartaTextInput text;
} PlaceTools;
// Single native-dialog state shared only by its input and rendering adapters.
extern PlaceTools tools;
// Supported service vocabulary used by native editing and presentation.
extern const char *const service_keys[];
extern const char *const service_labels[];
// Shares current dialog geometry or typed snapshot inspection between input and draw.
const char *tools_text(struct json_object *v, const char *key);
// Shares current dialog geometry or typed snapshot inspection between input and draw.
int64_t tools_int(struct json_object *v, const char *key);
// Shares current dialog geometry or typed snapshot inspection between input and draw.
SDL_FRect tools_rect(const AppState *app);
// Shares current dialog geometry or typed snapshot inspection between input and draw.
SDL_FRect tools_toolbar(const AppState *app, unsigned index);
// Shares current dialog geometry or typed snapshot inspection between input and draw.
struct json_object *tools_route_row(int ordinal);
#endif
