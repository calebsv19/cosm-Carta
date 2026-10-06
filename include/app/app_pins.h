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

// Stores a stable personal place and membership in independent collections.
typedef struct MapForgePin {
    char id[MAPFORGE_PIN_ID_CAPACITY];
    char name[MAPFORGE_PIN_NAME_CAPACITY];
    char type[MAPFORGE_PIN_TEXT_CAPACITY];
    char color[MAPFORGE_PIN_TEXT_CAPACITY];
    char notes[MAPFORGE_PIN_TEXT_CAPACITY];
    char created_at[64];
    char updated_at[64];
    uint32_t collection_mask;
    double lat;
    double lon;
    bool private_flag;
    char address[MAPFORGE_PIN_TEXT_CAPACITY];
    char location_source[MAPFORGE_PIN_TEXT_CAPACITY];
    char location_quality[64];
} MapForgePin;

// Names stable collection memberships independently of replaceable region packs.
typedef enum MapForgeCollection {
    MAPFORGE_COLLECTION_GENERAL = 1u,
    MAPFORGE_COLLECTION_DOG_WALKING = 2u,
    MAPFORGE_COLLECTION_PET_SITTING = 4u,
    MAPFORGE_COLLECTION_HIKING = 8u,
    MAPFORGE_COLLECTION_VISITED = 16u
} MapForgeCollection;

struct json_object;
// Owns personal records, preserved extension metadata and recovery state.
typedef struct MapForgePinsFile {
    uint32_t version;
    char map_region[MAPFORGE_PIN_NAME_CAPACITY];
    MapForgePin *pins;
    size_t pin_count;
    size_t pin_capacity;
    struct json_object *source_json;
    uint64_t source_signature;
    bool recovered_from_backup;
    bool write_blocked;
} MapForgePinsFile;

// Decodes a complete pin document without changing the caller on failure.
bool map_forge_pins_decode_json(const char *text, size_t length, MapForgePinsFile *out, char *error, size_t capacity);
// Resolves the independent personal-record store path.
bool map_forge_pins_workspace_path(char *out, size_t capacity);
// Loads or non-destructively migrates legacy personal records to the workspace store.
bool map_forge_pins_load_workspace(MapForgePinsFile *out, char *path, size_t path_capacity, char *error, size_t error_capacity);

// Returns the stable ID of a built-in collection.
const char *map_forge_collection_id(uint32_t collection);
// Returns the display name of a built-in collection.
const char *map_forge_collection_label(uint32_t collection);
// Returns true when a place belongs to a collection (zero selects all).
bool map_forge_pin_in_collection(const MapForgePin *pin, uint32_t collection);

void map_forge_pins_file_init(MapForgePinsFile *pins_file);
void map_forge_pins_file_free(MapForgePinsFile *pins_file);

bool map_forge_pins_load(const char *pins_path,
                         MapForgePinsFile *out_pins,
                         char *out_error,
                         size_t out_error_size);
bool map_forge_pins_save(const char *pins_path,
                         MapForgePinsFile *pins_file,
                         char *out_error,
                         size_t out_error_size);
bool map_forge_pins_default_private_path(const RegionInfo *region,
                                         char *out_path,
                                         size_t out_path_size);
bool map_forge_pins_legacy_private_path(const RegionInfo *region,
                                        char *out_path,
                                        size_t out_path_size);
bool map_forge_pins_load_preferred_region_file(const RegionInfo *region,
                                               MapForgePinsFile *out_pins,
                                               char *out_resolved_path,
                                               size_t out_resolved_path_size,
                                               bool *out_loaded_from_file,
                                               bool *out_loaded_from_legacy,
                                               char *out_error,
                                               size_t out_error_size);

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
