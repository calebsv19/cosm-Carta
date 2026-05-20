#ifndef MAPFORGE_APP_APP_PINS_H
#define MAPFORGE_APP_APP_PINS_H

#include "app/region.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAPFORGE_PIN_ID_CAPACITY 128u
#define MAPFORGE_PIN_NAME_CAPACITY 128u
#define MAPFORGE_PIN_TEXT_CAPACITY 256u
#define MAPFORGE_PIN_PATH_CAPACITY 512u

typedef struct MapForgePin {
    char id[MAPFORGE_PIN_ID_CAPACITY];
    char name[MAPFORGE_PIN_NAME_CAPACITY];
    char type[MAPFORGE_PIN_TEXT_CAPACITY];
    char color[MAPFORGE_PIN_TEXT_CAPACITY];
    char notes[MAPFORGE_PIN_TEXT_CAPACITY];
    char created_at[64];
    char updated_at[64];
    double lat;
    double lon;
    bool private_flag;
} MapForgePin;

typedef struct MapForgePinsFile {
    uint32_t version;
    char map_region[MAPFORGE_PIN_NAME_CAPACITY];
    MapForgePin *pins;
    size_t pin_count;
    size_t pin_capacity;
} MapForgePinsFile;

void map_forge_pins_file_init(MapForgePinsFile *pins_file);
void map_forge_pins_file_free(MapForgePinsFile *pins_file);

bool map_forge_pins_load(const char *pins_path,
                         MapForgePinsFile *out_pins,
                         char *out_error,
                         size_t out_error_size);
bool map_forge_pins_save(const char *pins_path,
                         const MapForgePinsFile *pins_file,
                         char *out_error,
                         size_t out_error_size);
bool map_forge_pins_default_private_path(const RegionInfo *region,
                                         char *out_path,
                                         size_t out_path_size);

MapForgePin *map_forge_pins_find_by_id(MapForgePinsFile *pins_file, const char *pin_id);
const MapForgePin *map_forge_pins_find_by_id_const(const MapForgePinsFile *pins_file, const char *pin_id);
MapForgePin *map_forge_pins_find_by_name(MapForgePinsFile *pins_file, const char *pin_name);
const MapForgePin *map_forge_pins_find_by_name_const(const MapForgePinsFile *pins_file, const char *pin_name);

bool map_forge_pins_upsert(MapForgePinsFile *pins_file,
                           const MapForgePin *pin,
                           char *out_error,
                           size_t out_error_size);
bool map_forge_pins_remove_by_id(MapForgePinsFile *pins_file, const char *pin_id);
bool map_forge_pins_move(MapForgePinsFile *pins_file, size_t from_index, size_t to_index);

#endif
